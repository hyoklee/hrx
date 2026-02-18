This directory is for testing HDF4 files.

The source code for testing this directory is `/scr/hyoklee/src/hyrax4`.

* input_high: link to Kent's high priority data collection directory
* input_old:  link to Kent's old data collection directory
* high_orig:  DMRPP, bescmd, FONC, and CDL files using the original get_dmrpp_h4.
* old_orig:   DMRPP, bescmd, FONC, and CDL files using the original get_dmrpp_h4.

See also `/scr/hyoklee/src/hyrax4/build/share/hyrax/data/hdf4/`
for `high/`, `old/`, and `new/` directories.
They have the outputs from hacked (-D) `get_dmrpp_h4`.

# Instruction for using Bash files and Perl script

```
# Change input location.
# ln -s /mnt/wrk/myang6/file-collections-archive/NASA-files/HDF4-handler-NASA-files-cloud input
ln -s /mnt/wrk/myang6/file-collections-archive/NASA-files/NASA-files-HDF4-handler input
# Generate bescmd files.
perl generate_bescmd.pl

# Copy bescmd files.
./cp.sh

# Update ln.sh for high
# Update dmrpp.sh for high_orig.
# Update run.sh for the input location.
```

# Sidecar

`sc.sh`: This script is for side car file generation in sc/.

Usage: `cd ./sc && ../sc.sh filename.hdf`
       
