#!/bin/bash
# Generate DDS from DMR.
filename="$1"
if ! [ -f $filename.dmrpp ]; then
  echo "DMR++ file does not exist."
  exit
fi
cp $filename.dmrpp /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/data/dmrpp
cp $filename /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/tests/input
cd /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/tests
perl /scr/hyoklee/data/generate_dds_bescmd.pl
cd /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module
besstandalone -c tests/bes.conf -i tests/$filename.dds.bescmd > /scr/hyoklee/data/$filename.dds
rm /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/tests/input/$filename
