#!/bin/bash

# Put outputs in $c directory.
# Loop through each file ending with .hdf in the $d directory.

#c="high"
#d=/mnt/wrk/myang6/file-collections-archive/NASA-files/HDF4-handler-NASA-files-cloud/

#c="old"
#d="/mnt/wrk/myang6/file-collections-archive/NASA-files/NASA-files-HDF4-handler/"

#c="new"
#d="/scr/hyoklee/src/hyrax4/build/share/hyrax/data/hdf4/new/"

d="input/"
perl generate_bescmd.pl
./cp.sh

for file in "$d"*.hdf; do
      # Print only the filename using basename command
      echo "$file"
      bn=$(basename $file)
      echo "$bn"
      ./ln.sh $bn
      ./dmrpp.sh $bn
      ./fonc.sh $bn
      done
