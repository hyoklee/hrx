#!/bin/bash
filename="$1"
./ln.sh $filename
./dmr.sh $filename
./dmrpp.sh $filename
./dds.sh $filename
