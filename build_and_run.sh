#!/bin/sh
LOAD_LEVEL=${1:-50}

rm -rf /tmpfs/experiments/leveldb/
make clean 
make -j6 -s LOAD_LEVEL=$LOAD_LEVEL
./dp/shinjuku