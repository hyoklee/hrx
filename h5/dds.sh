#!/bin/bash
# Generate DDS from DMR. Run this after dmrpp.sh.
filename="$1"
if ! [ -f /scr/hyoklee/data/$filename.dmrpp ]; then
  echo "DMR++ file does not exist."
  exit
fi
cp /scr/hyoklee/data/$filename.dmrpp /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/data/dmrpp
DIR="/scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/tests/input"
if [ ! -d "$DIR" ]; then
  mkdir "$DIR"
fi

cp /scr/hyoklee/data/$filename /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/tests/input
cd /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/tests
perl /scr/hyoklee/data/generate_dds_bescmd.pl
cd /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module
export LD_LIBRARY_PATH=/scr/hyoklee/src/hyrax/build/lib/:$LD_LIBRARY_PATH
besstandalone -c tests/bes.conf -i tests/$filename.dds.bescmd > /scr/hyoklee/data/$filename.dds
DIR="/scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/tests/input"
if [ ! -d "$DIR" ]; then
  mkdir "$DIR"
fi
rm /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/tests/input/$filename
