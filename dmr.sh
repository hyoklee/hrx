#!/bin/bash
# Generate DMR.
filename="$1"
cd /scr/hyoklee/src/hyrax/bes/modules/hdf5_handler/bes-testsuite/h5.nasa.default.dap4
cp /scr/hyoklee/src/hyrax/bes/modules/hdf5_handler/data/$filename input/
perl /scr/hyoklee/data/generate_bescmd.pl
cd /scr/hyoklee/src/hyrax/bes/modules/hdf5_handler/
besstandalone -c bes-testsuite/bes.default.conf -i bes-testsuite/h5.nasa.default.dap4/$filename.dmr.bescmd > /scr/hyoklee/data/$filename.dmr
cp bes-testsuite/h5.nasa.default.dap4/$filename.dmrpp.bescmd  /scr/hyoklee/src/hyrax/bes/modules/fileout_netcdf/tests/bescmd/
rm /scr/hyoklee/src/hyrax/bes/modules/hdf5_handler/bes-testsuite/h5.nasa.default.dap4/input/$filename
