#!/bin/bash
# Generate DMRPP.
filename="$1"
# cp /scr/hyoklee/src/hyrax4/build/share/hyrax/data/hdf4/high_orig/$filename.dmrpp .
# cp /scr/hyoklee/src/hyrax4/build/share/hyrax/data/hdf4/old_orig/$filename.dmrpp .

# Use this to generate a new dmrpp.
/scr/hyoklee/src/hyrax4/bes/modules/dmrpp_module/build_dmrpp_h4/get_dmrpp_h4 -i /scr/hyoklee/src/hyrax4/build/share/hyrax/data/hdf4/new/$filename 

# Use the following if .dmrpp is already generated.
# cp /scr/hyoklee/src/hyrax4/build/share/hyrax/data/hdf4/new/$filename.dmrpp .
perl -p -i -e "s/OPeNDAP_DMRpp_DATA_ACCESS_URL/data\/$filename/g" /scr/hyoklee/data4/$filename.dmrpp

