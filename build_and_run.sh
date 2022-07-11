#!/bin/sh

# Build leveldb
if [ ! -f deps/leveldb/build ]; then
    cd deps/leveldb
    mkdir -p build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
    cd ../../../
fi

make clean
make -s
LD_PRELOAD=./deps/opnew/dest/libnew.so ./dp/shinjuku