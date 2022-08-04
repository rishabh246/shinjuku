#pragma once

#include <stdint.h>
#include <ix/log.h>
#include <leveldb/c.h>
#include <time.h>
#include <assert.h>


#define KEYSIZE 32
#define VALSIZE 32

static unsigned long long *mmap_file;
static unsigned long long *randomized_keys;

typedef char db_key     [32];
typedef char db_value   [32];

// Type of Level-db operations
typedef enum
{
    DB_PUT,
    DB_GET,
    DB_DELETE,
    DB_ITERATOR,
    DB_SEEK,
    DB_CUSTOM,         // This type added for tests, see below.
} DB_REQ_TYPE;

typedef struct db_req
{
    DB_REQ_TYPE type;
    db_key      key;
    db_value    val;
    uint64_t    ts;
} db_req;

typedef struct kv_parameter
{
    db_key      key;
    db_value    value;

} kv_parameter;

typedef struct custom_payload
{
    int id;
    int ns;
    long timestamp;
} custom_payload;


static void randomized_keys_init(uint64_t num_keys)
{
    randomized_keys = (unsigned long long *)malloc(num_keys * sizeof(unsigned long long));
    if (randomized_keys == 0)
    {
        assert(0 && "malloc failed");
    }
    srand(time(NULL));
    for (int i = 0; i < num_keys; i++)
    {
        randomized_keys[i] = rand() % num_keys;
    }
}

static void check_db_sequential(leveldb_t *db, long num_keys, leveldb_readoptions_t * roptions)
{
	char * db_err = NULL;

	for (size_t i = 0; i < num_keys; i++)
	{
        char keybuf[15];
        char valbuf[15];
        int len;
        snprintf(keybuf, 15, "key%d", i);
        snprintf(valbuf, 15, "val%d", i);
        char * r = leveldb_get(db, roptions, keybuf, 15, &len, &db_err);
        // assert((strcmp(r,valbuf) == 0));
    }
}

static void prepare_complex_db(leveldb_t *db, long num_keys, leveldb_writeoptions_t *woptions)
{
	char * db_err = NULL;

	randomized_keys_init(num_keys);

	for (size_t i = 0; i < num_keys; i++)
	{
        char keybuf[20], valbuf[20];
        snprintf(keybuf, 20, "key%d", i);
        snprintf(valbuf, 20, "val%d", i);
        leveldb_put(db, woptions, keybuf, 20, valbuf, 20, &db_err);
	}

	for (int i = 0; i < num_keys; i = i + num_keys / 100)
	{
   	for (int j = 1; j < num_keys / 100; j++)
    	{
			char keybuf[20];
			snprintf(keybuf, 20, "key%d", i + j);
			printf("deleted => %s\n", keybuf);
			leveldb_delete(db,woptions, keybuf, 20, &db_err); 
		}
	}
} 