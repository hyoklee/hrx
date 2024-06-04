#!/bin/bash
# Generate DMRPP.
filename="$1"
cd /scr/hyoklee/src/hyrax/bes/modules/dmrpp_module/data
./get_dmrpp -A -b. -o /scr/hyoklee/data/$filename.dmrpp -X $filename
perl -p -i -e "s/OPeNDAP_DMRpp_DATA_ACCESS_URL/data\/$filename/g" /scr/hyoklee/data/$filename.dmrpp

