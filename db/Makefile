CC = g++
FLAGS = -fno-rtti -I ../deps/leveldb/include -O2 -std=c++17 -g -lpthread -lrt -ldl -lsnappy -lgflags -lz -lbz2 -llz4 -lnuma -ltbb -faligned-new

all: clean build

build:
	$(CC) database.cc -o db ../deps/leveldb/build/libleveldb.a $(FLAGS) 

clean:
	rm -f db
	rm -rf /tmp/leveldb_microbench/