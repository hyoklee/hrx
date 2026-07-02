# h5s3 handler: `get.dap` support and performance vs dmrpp

**Date:** 2026-07-01
**Host:** `matlab` (Hyrax from `~/src/hyrax/build`, OLFS in Tomcat on :8080)
**Module:** `bes/modules/h5s3_handler`
**Test file:** `MERRA2_200.tavg1_2d_slv_Nx.19970918.nc4`
(S3: Synology C2, `https://us-003.s3.synologyc2.net`, bucket `iowarp`)

---

## 1. Problem

The h5s3 handler served metadata only. Any data request returned HTTP 500:

> Request handler for 'h5s3' does not handle the response type 'get.dap'

`H5S3RequestHandler` registered only `DMR`/`DDS`/`DAS`/`HELP`/`VERS` — never
`DAP4DATA_RESPONSE`. And the existing DMR was parsed from generated XML into
plain libdap types whose `read()` is a no-op, so no data could be serialized
even if the response had been registered.

## 2. Fix

New **`H5S3DapBuilder.{h,cc}`** (module-only; depends on libdap, so it is never
linked into the libdap-free `h5s3_dap` CLI helper) builds a DAP4 DMR of
**read-capable** variables:

- `H5S3Array` (extends `libdap::Array`) and `H5S3Scalar<T>` (template over the
  libdap atomic scalar types) carry the S3 connection (`S3Auth`), bucket, object
  key, and the dataset's absolute HDF5 path.
- Their `read()` opens the file from S3 via the ROS3 VFD and reads the dataset on
  demand — `val2buf()` for numeric, `set_value()` for strings.
- `build_dmr_object()` walks the file (groups → `D4Group`, datasets → the above).

`H5S3RequestHandler` now registers:

```cpp
add_method(DAP4DATA_RESPONSE, H5S3RequestHandler::h5s3_build_dap4data);
```

`h5s3_build_dap4data()` calls `build_dmr_object()`, then
`set_dap4_constraint()` / `set_dap4_function()` — the same shape dmrpp/hdf5 use.

**Scope (thin reader):** atomic scalars/arrays of integer, float, string.
Compound datasets appear as a `Structure` but their data is not read.

**Files:** `H5S3DapBuilder.{h,cc}` (new); `H5S3RequestHandler.{h,cc}`,
`Makefile.am`, `README.md` (modified). Commit `a8bb9c8` on branch `h5s3-getdap`.

## 3. Correctness verification (live)

| Request | Before | After |
|---|---|---|
| `.dmr.xml` | 200 | 200 |
| `.dap.nc4?dap4.ce=/lon` | 500 | **200** — 576 values, −180…+179.375° by 0.625° ✓ |
| `.dap.nc4?dap4.ce=/T2M` | 500 | **200** — 24×361×576 Float32, ~209 K ✓ |
| `.dap.nc4` (whole file) | 500 | **200** — 47 vars, values match ✓ |

All produce valid HDF5/NetCDF4 with correct data read live from S3 via ROS3.

## 4. Performance: full-file `.dap.nc4` (FONC), h5s3 vs dmrpp

Same file, same request type (`get.dap` → fileout-netcdf), end-to-end `curl`
time (FONC NetCDF-4 is **not streamable** — the server builds the whole file
before sending, so this is essentially server-side build time):

| Metric | h5s3 (ROS3, live from S3) | dmrpp (local sidecar) |
|---|---|---|
| HTTP | 200 | 200 |
| **time_total** | **3097.5 s (~51.6 min)** | **491.6 s (~8.2 min)** |
| output size | 496,947,484 B (~474 MiB) | 406,902,237 B (~388 MiB) |
| avg transfer rate | ~160 KB/s | ~808 KB/s |
| variables | 47 (24×361×576) | same shape |

**h5s3 is ~6.3× slower** than dmrpp for the full file.

Single-variable timings (h5s3, live S3):
- tiny `/lon` (576 floats, ~4.6 KB): ~7.6 s — essentially all ROS3 open overhead
  (~3–4 s per open; the DMR build opens the file, then the read re-opens it).
- one full 3D var `/T2M` (~20 MB raw): ~87 s → ~7 s opens + ~80 s reading
  ≈ ~250 KB/s effective ROS3 throughput.

### Why h5s3 is slower (two independent costs)

1. **ROS3 read throughput (~160–250 KB/s), dominant.** ROS3 issues many small
   **serial** HTTPS range reads to the remote S3. dmrpp uses parallel range GETs
   with larger reads and serves the data description from a local `.dmrpp`
   sidecar (no per-request S3 metadata reads).
2. **Per-variable re-open.** Each variable's `read()` re-opens the S3 file
   (superblock + metadata range reads). Costs ~3–4 s × 47 variables. Helpful to
   fix, but small next to (1).

### Output size difference

The h5s3 output is larger (474 vs 388 MiB) because fileout-netcdf re-compresses
the data the thin reader hands it rather than passing through the original chunk
compression that dmrpp preserves — so it deflates less effectively. **Data
values are identical.**

## 4b. Repack file test (2026-07-01)

Repeated the full-file comparison on the **repack**'d variant of the same
granule, `MERRA2_200.tavg1_2d_slv_Nx.19970918.repack.nc4` (h5repack'd; different
chunking/compression layout). Same `get.dap` → fileout-netcdf, end-to-end `curl`,
both downloads verified (47 vars, `T2M` ≈ 209.2 K).

| Metric | h5s3 (ROS3, live from S3) | dmrpp (local sidecar) |
|---|---|---|
| HTTP | 200 | 200 |
| **time_total** | **2774.3 s (~46.2 min)** | **127.2 s (~2.1 min)** |
| output size | 496,947,498 B (~474 MiB) | 406,979,844 B (~388 MiB) |
| avg transfer rate | ~179 KB/s | ~3.2 MB/s |
| variables | 47 (24×361×576) | same shape |

**h5s3 is ~21.8× slower** than dmrpp on the repack file.

Observations vs. the original (§4):
- **dmrpp got much faster** on the repack file: 127 s vs 492 s (~3.9× faster).
  The repack layout (chunking/compression) is far friendlier to dmrpp's parallel
  range GETs, so nearly all of dmrpp's original cost was layout-driven, not fixed
  overhead.
- **h5s3 stayed about the same**: 2774 s vs 3098 s (~10% faster). Its cost is
  dominated by ROS3 serial small range reads + per-variable re-opens against the
  remote S3, which the repack layout barely changes.
- Consequently the **gap widened** from ~6.3× to ~21.8×. Output sizes are
  essentially unchanged (h5s3 still ~474 MiB, dmrpp ~388 MiB) — same re-compression
  effect as §4; data values identical.

## 5. Operational note: OLFS timeout

FONC NetCDF-4 is not streamable, so the full ~51-min build must complete within
the OLFS BES request timeout. The first full attempt failed at **HTTP 400**
("bes-timeout expired before transmitting: T2M", ElapsedTime 1218.7 s) against
the default **1200 s** cap.

Fix applied on the live host: raised OLFS `timeOut` **1200 → 7200 s** in
`.../webapps/opendap/WEB-INF/conf/olfs.xml` and restarted Tomcat. (`maxResponseSize`
is `0` = unlimited; `BES.TimeOutInSeconds` in bes.conf is ignored — OLFS sends the
per-request timeout.) **Kept at 7200 s.**

## 6. Next steps to close the gap (optional)

- **Share one open HDF5 handle across all variables of a request** (open once in
  the data handler; each variable reads from the shared handle) — removes cost (2).
- **Larger / parallel ROS3 reads** (bump the ROS3 read block size; concurrency) —
  addresses cost (1), the dominant one.
- Consider **passing through** the source chunk compression to fileout to match
  dmrpp's smaller output.

Together these are what it would take for a remote-S3 h5s3 whole-file download to
approach dmrpp's throughput. `get.dap` correctness is complete today.

## 7. Performance improvement analysis (2026-07-02)

Grounded in the code on host `matlab` (the running server loads **HDF5 2.2.0**,
`build/deps/lib/libhdf5.so.1000.0.0`). Full read path traced:
`H5Dread` → ROS3 VFD `H5FD__ros3_read` → `H5FD__s3comms_s3r_read` (AWS C CRT S3).

**Why it's slow:** cost is dominated by many small, strictly-serial HTTPS range
GETs to remote S3, multiplied by per-variable file re-opens. Neither HDF5 cache
helps MERRA2:
- The ROS3 **64 MB page buffer** (`H5FDros3.c:55`) activates only for files
  created with `H5F_FSPACE_STRATEGY_PAGE`; MERRA2/netCDF-4 are not paged → inactive.
- The **16 MB start-of-file cache** (`H5FDros3.c:38`, served at `:1352`) covers
  only the first 16 MB; past it, one GET per read (`:1362`), no read-ahead.
- ROS3 registers **NULL** for `read_vector`/`read_selection`
  (`H5FDros3.c:212–215`), so chunked reads fall back to one-chunk-at-a-time; each
  `s3r_read` blocks on a condition variable until its single GET completes
  (`H5FDros3_s3comms.c:1004`) → **zero read concurrency** for chunked data. This
  is why the repack sped up dmrpp (parallel) but not h5s3 (serial).

### h5s3 handler improvements
- **A1 — Share one open file handle per request** (biggest, low effort). Each
  `H5S3Array/H5S3Scalar::read()` calls `reader.open()`→`H5Fopen`
  (`H5S3DapBuilder.cc:99,166,188`); per variable that spins up a **new
  `aws_s3_client`** + TLS (`H5FDros3_s3comms.c:741`), **re-reads the 16 MB start
  cache** (`H5FDros3.c:1038`), and re-reads metadata. For 47 vars ≈ **~730 MB of
  redundant reads** + 46 client setups. Open once, reuse.
- **A2 — Honor the DAP constraint** (big for subsets). `read_into()` uses
  `H5S_ALL` (`H5S3DapBuilder.cc:128`) even for `dap4.ce` subsets → downloads the
  whole variable, libdap discards the rest. Use `H5Sselect_hyperslab`.
- **A3 — Drop the temp-buffer + `val2buf` double copy** (minor;
  `H5S3DapBuilder.cc:120–132`).
- **A4 — Larger chunk cache (rdcc)** on the fapl (`H5S3Reader.cc:105`; situational).

### HDF5 ROS3 VFD improvements
- **B1 — Implement `read_vector`** (highest-value, in HDF5). Fan the chunk range
  GETs out concurrently through the AWS CRT client instead of serial blocking
  reads. `H5FDros3.c:212–215` are NULL today.
- **B2 — Reuse the S3 client across opens** (`H5FDros3_s3comms.c:741`; the
  event-loop group is already global at `:229`).
- **B3 — Set `client_config` throughput/part-size** (`:737`; defaults today).
- **B4 — General VFD block cache / read-ahead** for non-paged files.

### Data-side lever (no code)
- **C — Repack granules with paged aggregation** (`h5repack -S PAGE -G <size>`) so
  the 64 MB ROS3 page buffer actually engages.

## 8. Optimization experiments (results)

Benchmark: full-file `.dap.nc4` (`get.dap`→FONC) of the **original** granule
`MERRA2_200.tavg1_2d_slv_Nx.19970918.nc4`, end-to-end `curl` time, unless noted.
Baselines from §4: **h5s3 3097.5 s**, dmrpp 491.6 s.

| Step | Change | time_total | vs baseline | notes |
|---|---|---|---|---|
| baseline | as-shipped get.dap | 3097.5 s | — | 47 vars, ~474 MiB |
| A1 | shared open handle | 2638.4 s | −14.8 % (1.17×) | 47 vars ✓; `/lon` 7.6→3.5 s |
| B1 | + ROS3 `read_vector` | 2666.9 s | −13.9 % vs base; ≈A1 | data identical ✓; no gain over A1 |
| A2 | + constraint pushdown | 3.76 s | ~15× vs 57.6 s | subset `T2M[0][100:199][100:199]`; **also fixes wrong-data bug** |
| B3 | + CRT tuning (part/conn) | 2589.5 s | −16.4 % vs base; ≈A1 | full file; data ✓; ~2 % over A1 |

(B2 "reuse S3 client across opens" was subsumed by A1: after A1 there is only one
open per request, so a global client saves almost nothing. Not separately built.)

### Root cause (why B1/B3 barely moved the full-file number)

A standalone ROS3 probe (`probe.c`, patched HDF5, `HDF5_ROS3_VFD_DEBUG=1`) reading
`/T2M` (20 MB) directly, bypassing BES/FONC:

```
RESULT dataset=/T2M npoints=4990464 bytes=19961856 read_ok=1 open_s=3.05 read_s=52.02
GET count: 397   (1 × 16 MB start-cache + 396 tiny chunk GETs, ~3 KB–19 KB each)
```

- The bottleneck is the **ROS3 read itself (52 s)**, not FONC — one variable is
  stored as ~**396 small chunks**, each fetched by its own GET, ~131 ms apart →
  **latency-bound on hundreds of serial small GETs**.
- **`read_vector` (B1) is never called for these reads.** All 397 GETs are logged
  by the *scalar* `s3r_read` path (the `read_vector` path has no such log). HDF5's
  **chunked-dataset read path fetches each chunk via a scalar block read through
  the chunk cache and does not route through `read_vector`** — even with selection
  I/O **forced on** (`H5Pset_selection_io(..., ON)`): still 397 scalar GETs, 52 s.
  So the concurrency machinery (B1) and the CRT tuning that feeds it (B3) have
  nothing to accelerate. `read_vector` only fires for contiguous-dataset reads and
  some metadata, not per-chunk raw data.

### What would actually work (updated recommendations)

1. **A1 + A2**: keep — real, safe wins (open reuse; subset pushdown + correctness).
2. **Fewer/larger chunks in the data** (biggest lever for this access pattern):
   the file has ~400 chunks/variable. Rewriting with larger chunks (or paged
   aggregation, so the 64 MB page buffer also engages) collapses hundreds of
   small GETs into a handful. `h5repack -l CHUNK=... ` / `-S PAGE`.
3. **Batch chunk reads into `read_vector` inside HDF5** — the only way to make B1
   pay off; requires changing HDF5's chunk-read path (`H5D__chunk_read` / chunk
   cache) to gather multiple chunk addresses and issue one vector read. Deep core
   change, not done here.
4. **VFD-level read-ahead cache** for chunked non-paged files: partially helps,
   but this file's chunks are strided ~1.1 MB apart (interleaved across variables),
   so naive contiguous read-ahead wastes bandwidth.

The HDF5 `read_vector`/CRT-tuning patch is correct and committed
(`hdf5-ros3-patches/ros3_readvector_crttuning.patch`); it simply isn't exercised
by chunked raw-data reads in this HDF5 version. It remains the right foundation
for improvement (2) or (3).

**Probe on the repack file confirms (2):** `/T2M` in `…repack.nc4` also needs
~398 GETs, many just **53–512 bytes** (chunk B-tree index nodes) → the existing
repack did *not* enlarge chunks, which is exactly why it never sped up h5s3
(while it did help dmrpp's parallel reader). A repack with genuinely large chunks
is required to cut the GET count.

### Final numbers (original granule, full `.dap.nc4`)

| config | time | vs baseline |
|---|---|---|
| baseline (as-shipped get.dap) | 3097.5 s | — |
| A1 (shared handle) | 2638.4 s | −14.8 % |
| A1+B1 | 2666.9 s | −13.9 % |
| A1+A2+B1+B3 (all) | 2589.5 s | −16.4 % |
| dmrpp (reference) | 491.6 s | — |

Net: the code optimizations reach ~16 % on whole-file download; closing the gap
to dmrpp needs the data-layout fix (2) and/or the deeper HDF5 chunk-vectorization
(3). A2 additionally makes **subset** requests ~15× faster and correct.
