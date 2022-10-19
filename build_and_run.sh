#!/bin/sh

rm -rf /tmpfs/experiments/leveldb/
make clean
make -j6 -s
./dp/shinjuku
