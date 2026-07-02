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
