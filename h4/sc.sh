# !/bin/bash

f="$1"
# f="3B42.20180802.03.7.HDF"

# FONC with a Side Car

# Generate MVS (Missing ValueS) file.
/scr/hyoklee/src/hyrax4/bes/modules/dmrpp_module/check_dmrpp $f.dmrpp $f.mvs

# The above step is already performed and stored in build/share/hyrax/data/hdf4/high|old/
# They are already in sc/ directory.

# Generate bescmd file for Side Car file using template.
python3 /scr/hyoklee/src/hyrax4/bes/modules/dmrpp_module/gen_miss_vars_bescmd.py -f $f -m $f.mvs

# Copy .bescmd file.
cp "$f"_missing.bescmd /scr/hyoklee/src/hyrax4/bes/modules/fileout_netcdf/tests/bescmd/

# Generate Side Car file using FONC. This is same as HDF5.
cd /scr/hyoklee/src/hyrax4/bes/modules/fileout_netcdf
# Adjust new to high or old if necessary
ln -s /scr/hyoklee/src/hyrax4/build/share/hyrax/data/hdf4/new/$f /scr/hyoklee/src/hyrax4/bes/modules/fileout_netcdf/data/$f
besstandalone -c tests/bes.nc4.grp.conf -i tests/bescmd/"$f"_missing.bescmd > /scr/hyoklee/data/$f.m.nc.h5

# Generate DMRPP file for Side Car file.
cd /scr/hyoklee/data/
./ln.sh $f.m.nc.h5
./dmr.sh $f.m.nc.h5
./dmrpp.sh $f.m.nc.h5
cp $f.m.nc.h5.dmrpp /scr/hyoklee/data4/sc/

#  The first is the dmrpp file that contains the missing variable value information. 
#  The second is the original dmrpp file. 
#  The third one is the href to the missing variables HDF5 file. 
#  The fourth one is the text file that includes the missing variable information. 
# ./merge_dmrpp sinusoid_ll_var.nc.dmrpp sinusoid.hdf.dmrpp data/sinusoid_ll_var.nc sinusoid_test.hdf.mvs
cd /scr/hyoklee/data4/sc/
cp $f.dmrpp $f.dmrpp.orig
/scr/hyoklee/src/hyrax4/bes/modules/dmrpp_module/merge_dmrpp $f.m.nc.h5.dmrpp $f.dmrpp data/$f.m.nc.h5 $f.mvs
mv $f.dmrpp input/$f.m.dmrpp
mv $f.dmrpp.orig $f.dmrpp

# Generate bescmd
perl generate_bescmd.pl

# Adjust OPeNDAP URL.
perl -p -i -e "s/OPeNDAP_DMRpp_DATA_ACCESS_URL/data\/$f/g" /scr/hyoklee/data4/sc/input/$f.m.dmrpp

# Generate final FONC using the merged file.
cd /scr/hyoklee/src/hyrax4/bes/modules/fileout_netcdf
cp /scr/hyoklee/data4/sc/input/$f.m.dmrpp /scr/hyoklee/src/hyrax4/bes/modules/fileout_netcdf/data/
cp /scr/hyoklee/data4/sc/$f.m.dmrpp.bescmd /scr/hyoklee/src/hyrax4/bes/modules/fileout_netcdf/tests/bescmd/
ln -s /scr/hyoklee/data/$f.m.nc.h5  /scr/hyoklee/src/hyrax4/bes/modules/fileout_netcdf/data/$f.m.nc.h5
besstandalone -c tests/bes.nc4.grp.conf -i tests/bescmd/$f.m.dmrpp.bescmd > /scr/hyoklee/data4/sc/$f.m.dmrpp.nc
ncdump -h /scr/hyoklee/data4/sc/$f.m.dmrpp.nc > /scr/hyoklee/data4/sc/$f.m.dmrpp.nc.cdl
