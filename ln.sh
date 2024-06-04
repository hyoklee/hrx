#!/bin/bash
filename="$1"
ncdump -h $filename > $filename.cdl
cd /scr/hyoklee/src/hyrax/bes/modules/hdf5_handler/data
ln -s /scr/hyoklee/data/${filename}
cd /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/data
ln -s /scr/hyoklee/data/${filename}
cd /scr/hyoklee/src/hyrax/bes/modules/fileout_netcdf/data
ln -s /scr/hyoklee/data/${filename}




