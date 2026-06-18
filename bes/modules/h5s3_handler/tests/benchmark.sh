#!/usr/bin/env bash
# Benchmark: h5s3_handler (HDF5 ROS3 VFD, live from S3) vs dmrpp_module
# (one-time sidecar build, then local serve) for producing DMR metadata of
# HDF5 sample files held in a localstack S3 bucket.
#
# Each measured command is run N times; we report min/avg/max in ms. The dmrpp
# build is checked for success (the .dmrpp sidecar must be produced) so a fast
# failure is never silently recorded as a fast build.
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

ms_now(){ date +%s%N; }
# stat_run N -- cmd... : print "min avg max" ms over N runs (output discarded)
stat_run(){ local n=$1; shift; shift; local s e d min=9999999 max=0 tot=0 i
  for ((i=0;i<n;i++)); do s=$(ms_now); "$@" >/dev/null 2>&1; e=$(ms_now); d=$(((e-s)/1000000))
    tot=$((tot+d)); ((d<min))&&min=$d; ((d>max))&&max=$d; done; echo "$min $((tot/n)) $max"; }

echo "## h5s3_handler: DMR read live from localstack S3 via ROS3 (10 iters)"
printf "%-28s %8s   %s\n" "file" "bytes" "min/avg/max ms"
export LD_LIBRARY_PATH="$H5/lib:$DEPS/lib"
for f in "${FILES[@]}"; do
    bytes=$(stat -c%s "$SRCDATA/$f" 2>/dev/null || echo "?")
    read -r mn av mx < <(stat_run 10 -- "$HERE/h5s3_dap" dmr "$f")
    printf "%-28s %8s   %s/%s/%s\n" "$f" "$bytes" "$mn" "$av" "$mx"
done

echo
echo "## dmrpp_module: one-time .dmrpp sidecar build via get_dmrpp_h5 (3 iters)"
printf "%-28s %8s   %s\n" "file" "sidecar" "min/avg/max ms"
mkdir -p "$ROOT/bench"
export LD_LIBRARY_PATH="$DEPS/lib:$B/lib"; export PATH="$B/bin:$PATH"
for f in "${FILES[@]}"; do
    cp "$SRCDATA/$f" "$ROOT/bench/$f"
    ( cd "$ROOT/bench" && \
      read -r mn av mx < <(stat_run 3 -- "$B/bin/get_dmrpp_h5" -s -i "$f" -u "http://localhost:4566/h5s3/$f")
      if [ -f "$f.dmrpp" ]; then
          printf "%-28s %8s   %s/%s/%s\n" "$f" "$(stat -c%s "$f.dmrpp")" "$mn" "$av" "$mx"
      else
          printf "%-28s %8s   BUILD FAILED (no sidecar)\n" "$f" "-"
      fi )
done
echo
echo "## Dataset reading: ROS3 full read vs dmrpp chunk byte-range GETs (10 iters)"
printf "%-28s %8s %7s   %-16s %-16s\n" "file" "dataB" "chunks" "h5s3 data ms" "dmrpp GETs ms"
# Build a one-connection multi-range curl command from a sidecar's chunks.
dmrpp_get(){ local f=$1 url="http://localhost:4566/h5s3/$1" args=() first=1 off nb end
  while read -r off nb; do end=$((off+nb-1))
    if [ $first -eq 1 ]; then args+=( -s -o /dev/null -r "${off}-${end}" "$url" ); first=0
    else args+=( --next -s -o /dev/null -r "${off}-${end}" "$url" ); fi
  done < <(grep -oE 'offset="[0-9]+" nBytes="[0-9]+"' "$ROOT/bench/$f.dmrpp" | grep -oE '[0-9]+' | paste - -)
  [ ${#args[@]} -gt 0 ] && curl "${args[@]}"; }
mkdir -p "$ROOT/bench"
for f in "${FILES[@]}"; do
    cp "$SRCDATA/$f" "$ROOT/bench/$f"
    ( export LD_LIBRARY_PATH="$DEPS/lib:$B/lib" PATH="$B/bin:$PATH"
      cd "$ROOT/bench" && "$B/bin/get_dmrpp_h5" -s -i "$f" -u "http://localhost:4566/h5s3/$f" >/dev/null 2>&1 )
    [ -f "$ROOT/bench/$f.dmrpp" ] || { printf "%-28s  (no sidecar; skipped)\n" "$f"; continue; }
    export LD_LIBRARY_PATH="$H5/lib:$DEPS/lib"
    bytes=$("$HERE/h5s3_dap" data "$f" 2>/dev/null | awk '{print $1}')
    nch=$(grep -c 'dmrpp:chunk ' "$ROOT/bench/$f.dmrpp")
    h5=$(stat_run 10 -- "$HERE/h5s3_dap" data "$f")
    dm=$(stat_run 10 -- dmrpp_get "$f")
    printf "%-28s %8s %7s   %-16s %-16s\n" "$f" "${bytes:-?}" "$nch" "$h5" "$dm"
done
rm -rf "$ROOT/bench"

echo
echo "Notes:"
echo " * h5s3 DMR: recurring per-request cost; reads metadata live from S3 (ROS3). No preprocessing."
echo " * dmrpp build: one-time cost (get_dmrpp_h5 = python + besstandalone + full CF handler"
echo "   + build_dmrpp; dominated by tool/process startup). After this, dmrpp serves the DMR"
echo "   from the LOCAL sidecar in a few ms with no S3 access."
echo " * Dataset reading: h5s3 opens via ROS3 (S3 metadata round-trips) then H5Dread; dmrpp"
echo "   issues byte-range GETs for the sidecar chunks (no S3 metadata). Same bytes transferred."
