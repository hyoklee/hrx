#!/bin/bash
# Generate NC4.
filename="$1"
cd /scr/hyoklee/src/hyrax/bes/modules/fileout_netcdf
cp /scr/hyoklee/data/$filename.dmrpp /scr/hyoklee/src/hyrax/bes/modules/fileout_netcdf/data/ 
besstandalone -c tests/bes.nc4.grp.conf -i tests/bescmd/$filename.dmrpp.bescmd > /scr/hyoklee/data/$filename.dmrpp.nc
ncdump -h /scr/hyoklee/data/$filename.dmrpp.nc > /scr/hyoklee/data/$filename.dmrpp.nc.cdl

