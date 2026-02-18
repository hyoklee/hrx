#!/bin/bash
# Generate NC4.
filename="$1"
cd /scr/hyoklee/src/hyrax4/bes/modules/fileout_netcdf
cp /scr/hyoklee/data4/$filename.dmrpp /scr/hyoklee/src/hyrax4/bes/modules/fileout_netcdf/data/ 
besstandalone -c tests/bes.nc4.grp.conf -i tests/bescmd/$filename.dmrpp.bescmd > /scr/hyoklee/data4/$filename.dmrpp.nc
ncdump -h /scr/hyoklee/data4/$filename.dmrpp.nc > /scr/hyoklee/data4/$filename.dmrpp.nc.cdl

