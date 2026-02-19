#!/bin/bash
# Generate DMR. Run ln.sh first.
filename="$1"
cd /scr/hyoklee/src/hyrax/bes/modules/hdf5_handler/bes-testsuite/h5.nasa.default.dap4

# Define the directory name
DIR="input/"

# Check if the directory does not exist
if [ ! -d "$DIR" ]; then
  # Create the directory
  mkdir "$DIR"
  echo "Directory '$DIR' created."
else
  # Optional: Let the user know it's already there
  echo "Directory '$DIR' already exists."
fi

cp /scr/hyoklee/src/hyrax/bes/modules/hdf5_handler/data/$filename input/
perl /scr/hyoklee/data/generate_bescmd.pl
cd /scr/hyoklee/src/hyrax/bes/modules/hdf5_handler/
export PATH=/scr/hyoklee/src/hyrax/build/bin/:$PATH
export LD_LIBRARY_PATH=/scr/hyoklee/src/hyrax/build/lib/:$LD_LIBRARY_PATH
besstandalone -c bes-testsuite/bes.default.conf -i bes-testsuite/h5.nasa.default.dap4/$filename.dmr.bescmd > /scr/hyoklee/data/$filename.dmr
cp bes-testsuite/h5.nasa.default.dap4/$filename.dmrpp.bescmd  /scr/hyoklee/src/hyrax/bes/modules/fileout_netcdf/tests/bescmd/
rm /scr/hyoklee/src/hyrax/bes/modules/hdf5_handler/bes-testsuite/h5.nasa.default.dap4/input/$filename
