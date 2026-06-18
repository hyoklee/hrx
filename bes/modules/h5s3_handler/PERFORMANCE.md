# Performance: `h5s3_handler` (ROS3) vs `dmrpp_module`

Comparison of two ways to serve metadata (the DAP **DMR**) for HDF5 files that
live in S3, measured against a local **localstack** S3 endpoint
(`http://localhost:4566`, bucket `h5s3`).

- **`h5s3_handler`** — opens the HDF5 file *live* from S3 through the HDF5 ROS3
  virtual file driver on every request and walks its structure. No
  preprocessing, no sidecar.
- **`dmrpp_module`** — a one-time step (`get_dmrpp_h5`) reads the file and writes
  a `.dmrpp` XML sidecar (DMR + chunk byte-offset maps). Afterwards the DMR is
  served from the **local** sidecar with no S3 access for metadata; data values
  are fetched with byte-range GETs.

## Environment

| | |
|---|---|
| Host | Linux 6.17, 4 cores |
| S3 | localstack (docker), path-style, endpoint `http://localhost:4566` |
| HDF5 | built from `~/src/hdf5` with ROS3 VFD, installed at `~/hdf5-ros3` |
| dmrpp tooling | `get_dmrpp_h5` / `build_dmrpp` from `~/src/hyrax/build/bin` (v3.21.1) |
| Sample files | 12 small HDF5 files (1 KB – 17 KB) in bucket `h5s3` |
| Method | wall-clock of the CLI, N iterations, min/avg/max ms |

Reproduce with `tests/benchmark.sh`.

## Results

### 1. Metadata (DMR) — recurring per-request cost

`h5s3_handler` reading live from S3 via ROS3 (10 iterations/file):

| file | bytes | min / avg / max (ms) |
|---|---:|---|
| comp_complex_scalar.h5 | 1039 | 89 / 90 / 96 |
| SDS_fle_shuf_2def.h5 | 4152 | 88 / 90 / 95 |
| cf_2dll_same_dimsize.h5 | 11056 | 89 / 90 / 94 |
| FakeDim_remove.h5 | 16936 | 88 / 90 / 92 |

The DMR latency is **~90 ms regardless of file size** — it is dominated by S3
round-trips (open/HEAD + reads of the superblock and metadata), not by payload.

`dmrpp_module` serving the DMR from a prebuilt local sidecar: a local XML parse
of a ~4 KB file, measured at **~2–4 ms** (no S3 access).

### 2. One-time preprocessing cost

| approach | one-time cost | notes |
|---|---:|---|
| `h5s3_handler` | **0** | no sidecar; index.parquet is per-bucket, not per-file (below) |
| `dmrpp_module` (`get_dmrpp_h5`) | **~10.25 s / file** | python + `besstandalone` + full CF HDF5 handler + `build_dmrpp`; produces a ~4.4 KB sidecar |

The ~10 s is dominated by tool/process startup (cold `besstandalone` + Python),
not intrinsic computation; a server-resident build would be lower. It is
reported here as the cost of the actual user-facing tool.

### 3. `h5s3_handler` bucket/index overhead (per bucket, amortized over all files)

| operation | time (ms) | tool |
|---|---:|---|
| list bucket (`ListObjectsV2`) | ~102 | `h5s3_list` (AWS C++ SDK) |
| write `index.parquet` (12 rows) | ~25 | `h5s3_index` (Parquet C++) |
| read `index.parquet` (browse) | ~28 | `h5s3_index` (Parquet C++) |

(The index write/read are dominated by process + Arrow library startup for this
tiny row count.) The index is built once per bucket and only when absent.

### 4. Dataset reading — actual data values

Reading the **full data of every dataset** in a file from S3 (10 iterations).

- `h5s3_handler`: `h5s3_dap data <file>` opens the file via ROS3 (which fetches
  the superblock and metadata to locate the datasets) and `H5Dread`s every
  dataset — i.e. a complete per-request data read, no precomputed state.
- `dmrpp_module`: issues `libcurl` byte-range GETs for exactly the chunks listed
  in the local `.dmrpp` sidecar (offsets/sizes already known, **no** S3 metadata
  access), reusing one HTTP connection. Same data volume as ROS3.

| file | data bytes | chunks | h5s3 data via ROS3 (min/avg/max ms) | dmrpp chunk GETs (min/avg/max ms) |
|---|---:|---:|---|---|
| cf_1dll_same_dimsize.h5 | 168 | 4 | 88 / 88 / 90 | 14 / 15 / 17 |
| cf_2dll_same_dimsize.h5 | 2768 | 6 | 90 / 91 / 94 | 16 / 19 / 22 |

Both transfer the **identical** number of data bytes (verified: the sidecar
chunk sizes sum to exactly what ROS3 reads). For these small files the data
payload is negligible, so the times are almost entirely *access pattern*:

- **dmrpp ≈ chunks × ~3 ms** — one small range GET per chunk, offsets taken from
  the local sidecar. It never touches S3 for metadata.
- **ROS3 ≈ ~88 ms** — dominated by the file **open** (HEAD + superblock +
  metadata round-trips to discover the datasets); the actual `H5Dread` adds
  little on top of the DMR cost. ROS3 pays the metadata-discovery cost on *every*
  data request.

Net: **dmrpp reads data ~5× faster here**, and the entire gap is the S3 metadata
round-trips that dmrpp precomputed once into its sidecar. dmrpp's data cost
scales with the **chunk count** (more chunks = more range GETs); ROS3's is
governed by the file's **metadata complexity** at open time.

## Interpretation

The two designs trade preprocessing against per-request latency:

```
total time for N metadata (DMR) requests on one file
  h5s3_handler : 90 * N                     (ms)
  dmrpp_module : 10250 + ~4 * N             (ms)   [one-time build + local serves]

crossover:  90 N = 10250 + 4 N  ->  N ≈ 119 requests
```

- **`h5s3_handler` wins** for one-off or rarely-accessed files, ad-hoc browsing,
  and rapidly-changing buckets: it needs **zero preprocessing** and no sidecar
  maintenance — point it at a bucket and serve. The cost is ~90 ms of S3
  round-trips on every metadata request.
- **`dmrpp_module` wins** for frequently served, stable files: after a one-time
  (~10 s here) build, DMR responses are local and a few ms, independent of S3
  latency. The break-even is on the order of ~100 metadata requests per file.
- For **data** reads (section 4) both stream the same bytes from S3, but
  `dmrpp_module` is ~5× faster on these files because it already knows the chunk
  offsets (local sidecar) and skips the S3 metadata-discovery that ROS3 repeats
  on every request. This reinforces the same trade-off: dmrpp's one-time build
  pays off for repeated metadata *and* data access; h5s3/ROS3 avoids the build
  but re-pays the open/metadata cost each time.

## Real cloud endpoint (Synology C2, large files)

The localstack numbers above are a low-latency floor. This section repeats the
comparison against a **real** S3-compatible endpoint over the WAN:

| | |
|---|---|
| Endpoint | `https://us-003.s3.synologyc2.net` (OpenStack Swift + S3 layer, behind nginx) |
| Bucket | `iowarp`, path-style, **SigV4** auth (region `us-east-1`) |
| Measured raw GET throughput | ~8.8–10.6 MB/s (407 MB object in ~38 s) |
| Files | nava.h5 (83 KB), MERRA2_200…nc4 (**407 MB**, 50 vars, 18,074 chunks), MERRA2_400…nc4 (**4.16 GB**, 27 vars) |

Anonymous access is refused (HTTP 412); credentials are required even to list.

### Prerequisite finding: the ROS3 HDF5 must include the data filters

The MERRA-2 files are deflate-compressed. A minimal ROS3 HDF5 build reads the
metadata fine but **fails every data read** with *"required filter (name
unavailable) is not registered."* HDF5 must be built with zlib — and the CMake
option was renamed to **`-DHDF5_ENABLE_ZLIB_SUPPORT=ON`** (the older
`HDF5_ENABLE_Z_LIB_SUPPORT` is silently ignored). Confirm with
`H5_HAVE_FILTER_DEFLATE 1` in `H5pubconf.h` / `DEFLATE` in `libhdf5.settings`.
dmrpp is unaffected here because its data path is raw byte-range GETs (the
filters are applied by the serving handler, not the fetcher).

### Metadata (DMR) over the WAN — recurring per request

| file | size | vars | h5s3 DMR via ROS3 (min/avg/max ms) | dmrpp DMR from local sidecar |
|---|---:|---:|---|---|
| nava.h5 | 83 KB | 3 | 612 / 631 / 652 | ~8 ms |
| MERRA2_200…nc4 | 407 MB | 50 | 3681 / 5084 / 6035 | ~8 ms |
| MERRA2_400…nc4 | 4.16 GB | 27 | 2685 / 3048 / 3244 | ~8 ms |

ROS3 DMR latency tracks **metadata complexity (variable/object count), not file
size** — the 4.16 GB file (27 vars) builds its DMR *faster* than the 407 MB file
(50 vars), because each object costs WAN round-trips. dmrpp serves the DMR from
the local sidecar in a few ms regardless. Over the WAN the metadata gap widens
to ~600× (≈5 s vs ≈8 ms) versus ~30× on localstack.

### Data read of one compressed variable (`/CLDPRS`: 8.56 MB, 384 chunks)

Same bytes transferred on every path; the variable's 384 ~22 KB chunks are
**scattered** through the file (cannot be coalesced into one GET).

| approach | time | notes |
|---|---:|---|
| h5s3 / ROS3 (sequential per-chunk) | 26.4 s cold, 18.2 s warm | one range GET per chunk; CRT keep-alive |
| dmrpp, 1 connection (sequential) | 80.6 s | naive `curl --next` per chunk — request overhead dominates |
| dmrpp, 8 parallel fetchers | 22.2 s | ≈ dmrpp default thread pool |
| dmrpp, 16 parallel fetchers | 15.5 s | parallelism beats ROS3 |
| raw single GET, 8.56 MB contiguous (hypothetical) | <1 s @ 10 MB/s | unattainable here — chunks are scattered |

dmrpp one-time build for the 407 MB file: ~40 s download + ~10 s processing
(18,074 chunks → 1.8 MB sidecar).

The data read is dominated by **per-chunk request latency** over the WAN, so the
**chunk count** is the real driver, not the byte volume. ROS3 fetches chunks
sequentially (≈18–26 s); dmrpp's win is **parallelism** (≈15 s at 16 threads)
plus serving metadata locally. A single-threaded chunk fetch is worst of all.

### Takeaways for the real cloud case

- **Metadata:** dmrpp is dramatically better over the WAN (~8 ms local vs ~5 s
  for a 50-variable file), because ROS3 re-pays the per-object round-trips every
  request. This is the strongest argument for the dmrpp sidecar.
- **Data:** comparable order of magnitude; dmrpp pulls ahead through parallel
  chunk fetching, but only after its ~50 s one-time build. ROS3 needs zero setup
  and its sequential CRT read is respectable.
- **Operational:** ROS3 must be compiled with the right filters or it can't read
  real compressed data at all; dmrpp shifts that concern to the serving handler.
- The crossover strongly favors dmrpp for any file served more than a handful of
  times; h5s3/ROS3 remains attractive for ad-hoc access to many files where
  building and maintaining sidecars for all of them is impractical.

## Caveats

- localstack is local, so S3 latency here is a floor; against real AWS S3 the
  ~90 ms ROS3 per-request cost would grow with network RTT, pushing the
  crossover **lower** (favoring dmrpp's local serve for hot files).
- `h5s3_handler` uses a deliberately thin DMR builder (atomic/array/compound +
  attributes), while `dmrpp_module` uses the full CF-aware HDF5 handler, so the
  dmrpp DMR is richer — part of why its build is heavier.
- An earlier draft of `benchmark.sh` mis-reported the dmrpp build as ~240 ms; it
  was silently timing a fast tool failure. The script now asserts the sidecar is
  actually produced. The numbers above are the corrected, verified values.
- The dmrpp data figure (section 4) measures the chunk byte-range GETs — the
  dominant, network-bound cost — with `curl`, not the dmrpp handler's in-process
  chunk assembly/decompression. For these uncompressed sample files that overhead
  is negligible; for large or compressed/filtered data, transfer volume and
  decompression would add to *both* paths (ROS3 and dmrpp), and the samples here
  (≤ ~2.7 KB of data) are too small to exercise that.
- The data comparison uses the two files whose datasets read cleanly on both
  paths; some compound/scalar samples either failed `get_dmrpp_h5` or read zero
  bytes through the thin ROS3 reader, and were excluded.
- The real-endpoint (Synology C2) numbers are single/low-iteration over a shared
  WAN link, so they carry real variance (e.g. ROS3 `/CLDPRS` measured 26.4 s cold
  vs 18.2 s warm) — read them as order-of-magnitude, not precise.
- The dmrpp data figures over the WAN use `curl` (sequential `--next` and
  `xargs -P` for parallel) as a stand-in for the dmrpp handler's internal
  transfer-thread pool; the real handler's concurrency/connection reuse will
  differ. The ROS3 figures are the actual `h5s3` reader.
- ROS3's per-chunk sequential fetch could likely be improved with HDF5 read
  coalescing / larger chunk cache or the CRT's multi-range support; these were
  not tuned here.
