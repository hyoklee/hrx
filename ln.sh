#!/bin/bash
filename="$1"
scp giraffe:/usr/share/hyrax/data/NASAFILES/hdf5/$filename /scr/hyoklee/data/
export PATH=/scr/hyoklee/src/hyrax/build/deps/bin:$PATH
ncdump -h /scr/hyoklee/data/$filename > /scr/hyoklee/data/$filename.cdl
cd /scr/hyoklee/src/hyrax/bes/modules/hdf5_handler/data
ln -s /scr/hyoklee/data/${filename}
cd /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/data
ln -s /scr/hyoklee/data/${filename}
cd /scr/hyoklee/src/hyrax/bes/modules/fileout_netcdf/data
ln -s /scr/hyoklee/data/${filename}




