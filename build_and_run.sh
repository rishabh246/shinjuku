#!/bin/sh

# Build leveldb
if [ ! -f ./deps/leveldb/build ]; then
    cd deps/leveldb
    mkdir -p build && cd build
    cmake -DCMAKE_BUILD_TYPE=Release .. && cmake --build .
    cd ../../../
fi

rm -rf /tmpfs/experiments/leveldb/
make clean
make -j6 -s
./dp/shinjuku