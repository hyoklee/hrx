# HDF5 ROS3 VFD: Threading & Optimization Analysis

**Scope.** Does the HDF5 Read-Only S3 (ROS3) virtual file driver use threading?
Is concurrency purely the AWS Common Runtime (CRT), or can the HDF5 library be
tweaked to go faster? Findings are grounded in the ROS3 source in
`~/src/hdf5/src/H5FDros3.c` and `H5FDros3_s3comms.c` (HDF5 2.2.0 dev, the
AWS-CRT/`aws-c-s3`-based ROS3 used by `h5s3_handler`).

This is the implementation analysis behind the performance gap measured against
`dmrpp_module` (see `bes/modules/h5s3_handler/PERFORMANCE.md`, "Real cloud
endpoint" section, and `tests/synology_performance.png`).

---

## TL;DR

- The AWS CRT under ROS3 **is** multi-threaded, but it only parallelizes
  **within a single request** (multipart split of one large GET). HDF5 drives
  the VFD with **scalar, blocking, one-range-at-a-time** reads, and the ROS3 VFD
  does **not** implement `read_vector`/`read_selection`.
- Result: chunked/compressed datasets (many small chunks) and metadata (many
  small reads) are issued **serially and are RTT-bound**; the CRT's worker
  threads sit idle. This is exactly why `dmrpp_module` — which fetches chunks in
  parallel itself — was faster over the WAN.
- HDF5 **can** be optimized. Highest leverage: **implement `read_vector` in the
  ROS3 VFD** so HDF5 hands it many ranges at once and it can issue them
  concurrently via the CRT. Secondary: tune the `aws-c-s3` client config and use
  HDF5 caching/page-buffering knobs.

---

## 1. What threading exists today

| Aspect | Finding | Source |
|---|---|---|
| Event-loop group | `loop_count = 0` → CRT default ≈ one event loop per core (+ host resolver) | `H5FDros3_s3comms.c` (event_loop_group_opts, ~ln 278) |
| Per-read request | Each read = one `aws_s3_client_make_meta_request(GET_OBJECT)` | `H5FDros3_s3comms.c` `H5FD__s3comms_s3r_read` (~ln 994) |
| Intra-request parallelism | `aws-c-s3` auto-splits a large GET into parts fetched concurrently across event-loop threads | CRT default behavior |

So worker threads exist, and **a single sufficiently large contiguous read is
already parallelized** by the CRT. Concurrency today is **entirely the CRT's**,
and only *intra-request*.

## 2. Why the real-cloud workload didn't benefit

Two structural limits at the HDF5 ↔ VFD boundary:

1. **The VFD implements only scalar `read`.** In the `H5FD_class_t` for ros3,
   `read_vector` and `read_selection` are `NULL`
   (`H5FDros3.c`, ~ln 212–214 / struct ~ln 182). HDF5 therefore cannot hand the
   VFD a batch of byte ranges; it falls back to calling scalar `read` **once per
   chunk**.
2. **Each scalar read blocks.** `s3r_read` issues one meta-request and then
   `aws_condition_variable_wait_pred`s until it completes
   (`H5FDros3_s3comms.c`, ~ln 1004) before HDF5 requests the next chunk.

For the measured cases:
- `/CLDPRS` = **384 × ~22 KB** chunks — each far below the CRT part-size
  threshold, so no intra-request split, and they run **one at a time**.
- **DMR metadata** = many small b-tree / object-header reads — likewise serial,
  RTT-bound.

The CRT's threads are mostly idle; latency is dominated by per-request round
trips. `dmrpp_module` wins because it parallelizes chunk GETs itself and serves
metadata from a local `.dmrpp` sidecar (no S3 round trips for metadata).

### Measured impact (Synology C2, WAN)

| | h5s3_handler (ROS3) | dmrpp_module |
|---|---|---|
| DMR, MERRA2_200 (407 MB, 50 vars) | ~5.1 s (live) | ~8 ms (local sidecar) |
| Data read `/CLDPRS` (8.56 MB, 384 chunks) | 18–26 s (sequential) | 15.5 s @16 threads / 22 s @8 (parallel) |

## 3. Optimization levers (ranked)

### 3.1 Implement `read_vector` (and/or `read_selection`) in the ROS3 VFD — biggest win
HDF5's I/O path can gather many ranges and call `read_vector` once. A ROS3
`read_vector` could launch **N meta-requests concurrently** (or use the CRT's
batching) and wait for all, instead of serializing. This is the in-VFD analog of
dmrpp's parallel fetch and would turn 384 sequential GETs into a bounded-
concurrency fan-out.
- **Caveat:** for **filtered (compressed)** chunks (e.g. MERRA-2 deflate),
  HDF5's filtered-chunk read path historically locks/reads chunks individually,
  so a `read_vector` win is **not automatic** there — it mainly helps contiguous
  and *unfiltered*-chunked data plus the metadata path. Fully parallelizing
  filtered chunks needs work above the VFD (a prefetch/chunk cache) — essentially
  what dmrpp does.

### 3.2 Tune the `aws_s3_client_config`
Currently only `client_bootstrap`, `region`, and `signing_config` are set
(`H5FDros3_s3comms.c`, ~ln 737–739). Left at CRT defaults:
- `part_size` — threshold/size for splitting a GET into parts.
- `throughput_target_gbps` — drives how aggressively the CRT parallelizes.
- `max_active_connections_override` — connection pool ceiling.
Setting these raises intra-request parallelism for **large** reads (won't help
sub-part-size chunks).

### 3.3 Event-loop count
`loop_count = 0` (auto). A knob, rarely the bottleneck.

### 3.4 HDF5-side knobs (no VFD code change)
- `H5Pset_chunk_cache` / `H5Pset_cache` — larger raw-chunk cache avoids
  refetching chunks under overlapping access.
- `H5Pset_page_buffer_size` + paged file-space strategy — coalesces metadata
  into page-sized reads, shrinking the DMR round-trip count. **Requires the file
  to have been written with paged allocation**, so it helps new files, not
  arbitrary existing ones.

## 4. Bottom line

Today the VFD relies entirely on the CRT and only for *intra-request*
parallelism, so chunked/compressed and metadata access is serialized and
RTT-bound over the WAN. The library is not "maxed out" — the highest-leverage
change is to **implement `read_vector` in the ROS3 VFD** for concurrent
multi-range fetches, complemented by tuning the `aws-c-s3` client and (for new
files) page buffering. Those changes would close most of the measured gap to
`dmrpp_module`, with the caveat that the compressed-chunk path needs additional
prefetch logic beyond `read_vector` alone.

---

*Generated from source inspection of `~/src/hdf5` (HDF5 2.2.0 dev, ROS3 VFD with
`aws-c-s3` backend). Line numbers are approximate and may drift with revisions —
verify against the current source.*
