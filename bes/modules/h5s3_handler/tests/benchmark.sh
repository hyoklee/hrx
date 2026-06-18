#!/usr/bin/env bash
# Benchmark: h5s3_handler (HDF5 ROS3 VFD, live from S3) vs dmrpp_module
# (one-time sidecar build, then local serve) for producing DMR metadata of
# HDF5 sample files held in a localstack S3 bucket.
set -u

HERE="$(cd "$(dirname "$0")/.." && pwd)"          # .../modules/h5s3_handler
B="$HOME/src/hyrax/build"                          # BES install prefix
H5="$HOME/hdf5-ros3"
DEPS="$B/deps"
ROOT="$B/share/hyrax"
SRCDATA="$HOME/src/hyrax/bes/modules/hdf5_handler/data"

export AWS_ACCESS_KEY_ID=test AWS_SECRET_ACCESS_KEY=test AWS_DEFAULT_REGION=us-east-1
export H5S3_BUCKET=h5s3 H5S3_ENDPOINT=http://localhost:4566 HDF5_ROS3_VFD_FORCE_PATH_STYLE=true

FILES=(comp_complex_scalar.h5 SDS_fle_shuf_2def.h5 cf_2dll_same_dimsize.h5 FakeDim_remove.h5)
ITERS=5

# milliseconds wall clock for a command (stderr/stdout discarded)
timed() { local s e; s=$(date +%s%N); "$@" >/dev/null 2>&1; e=$(date +%s%N); echo $(( (e - s) / 1000000 )); }

avg_ms() {  # avg_ms ITERS cmd...
    local n=$1; shift; local total=0 i ms
    for ((i=0;i<n;i++)); do ms=$(timed "$@"); total=$((total+ms)); done
    echo $(( total / n ))
}

printf "%-28s %8s %14s %16s\n" "file" "bytes" "h5s3 DMR(S3)" "dmrpp build(1x)"
printf "%-28s %8s %14s %16s\n" "----" "-----" "------------" "---------------"

mkdir -p "$ROOT/bench"
for f in "${FILES[@]}"; do
    bytes=$(stat -c%s "$SRCDATA/$f" 2>/dev/null || echo "?")

    # --- h5s3_handler: DMR read live from localstack via ROS3 ---
    export LD_LIBRARY_PATH="$H5/lib:$DEPS/lib"
    h5s3_ms=$(avg_ms "$ITERS" "$HERE/h5s3_dap" dmr "$f")

    # --- dmrpp_module: one-time sidecar build (reads file, emits DMR++) ---
    cp "$SRCDATA/$f" "$ROOT/bench/$f"
    export LD_LIBRARY_PATH="$DEPS/lib:$B/lib"; export PATH="$B/bin:$PATH"
    ( cd "$ROOT/bench" && \
      dmrpp_ms=$(avg_ms 2 "$B/bin/get_dmrpp_h5" -s -i "$f" -u "http://localhost:4566/h5s3/$f")
      printf "%-28s %8s %12s ms %13s ms\n" "$f" "$bytes" "$h5s3_ms" "$dmrpp_ms" )
done

echo
echo "Notes:"
echo " * h5s3 DMR(S3): recurring per-request cost; reads metadata live from S3 (ROS3). No preprocessing."
echo " * dmrpp build(1x): one-time cost to create the .dmrpp sidecar. After this,"
echo "   dmrpp serves the DMR from the LOCAL sidecar (sub-millisecond, no S3 access)."
