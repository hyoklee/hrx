#!/bin/bash
filename="$1"
cp input/${filename} /scr/hyoklee/src/hyrax4/build/share/hyrax/data/hdf4/new/
cd /scr/hyoklee/src/hyrax4/bes/modules/fileout_netcdf/data
# ln -s /scr/hyoklee/src/hyrax4/build/share/hyrax/data/hdf4/high/${filename}
# ln -s /scr/hyoklee/src/hyrax4/build/share/hyrax/data/hdf4/old/${filename}
ln -s /scr/hyoklee/src/hyrax4/build/share/hyrax/data/hdf4/new/${filename}



