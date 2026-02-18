#!/bin/bash

# Loop through all files in the current directory
for file in *
do
  # Check if the file ends with ".h5" (use [[ ]] for improved string comparison)
  if [[ $file == *.h5 ]]; then
    # Print the filename
    echo "$file"
    ./dds.sh $file
  fi
done
