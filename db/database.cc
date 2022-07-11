#include "leveldb/db.h"

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <cassert>
#include <chrono>
#include <stdlib.h>
#include <time.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#if defined(OS_WIN)
std::string kDBPath = "C:\\Windows\\TEMP\\leveldb_microbench";
#else
std::string kDBPath = "/tmp/leveldb_microbench";
#endif

unsigned long long num_keys = 1024;
std::string key_base = "key";
std::string value_base = "value";

int main(int argc, char **argv)
{
    // destroy and open DB
    leveldb::DB *db;
    leveldb::Options options;
    options.create_if_missing = true;
    leveldb::Status s = leveldb::DB::Open(options, kDBPath, &db);
    assert(s.ok());

    for(int i = 0; i < num_keys; i++) 
    {
        leveldb::Status s = db->Put(leveldb::WriteOptions(), key_base+std::to_string(i), value_base+std::to_string(i));
    }  

    std::string value;
    for (int i = 0; i < num_keys; i++)
    {
        leveldb::Status s = db->Get(leveldb::ReadOptions(), key_base + std::to_string(i), &value);
        assert(std::strcmp(value.c_str(), (value_base + std::to_string(i)).c_str() ) == 0);
    }

    std::printf("Hello from LevelDb, all operations runned correctly\n");
    // close DB
    delete db;
}