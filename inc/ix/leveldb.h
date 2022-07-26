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


// static void process_db_pkg(db_req *db_pkg)
// {
//     char * db_err = NULL;

//     switch (db_pkg->type)
//     {
//     case (PUT):
//     {
//         leveldb_put(db, woptions, 
//             ((struct kv_parameter *)(db_pkg->params))->key, KEYSIZE, 
//             ((struct kv_parameter *)(db_pkg->params))->value, VALSIZE,
//             &db_err);
//         break;
//     }

//     case (GET):
//     {
//         char* read = leveldb_get(db, roptions, 
//             (db_key)(db_pkg->params), KEYSIZE, 
//             &read_len, &db_err);

//         break;
//     }
//     case (DELETE):
//     {
//         leveldb_delete(db, woptions, 
//             (db_key)(db_pkg->params), KEYSIZE,
//             &db_err);
        
//         break;
//     }
//     case (CUSTOM):
//     {
//         struct custom_payload * payload = (struct custom_payload*)(db_pkg->params);
        
//         printf("Starting custom command with id: %d for u=%d \n", payload->id, payload->ms);
//         int i = 0;

//         do {
//                 asm volatile ("nop");
//                 i++;
//         } while ( i / 0.233 < payload->ms);

//         printf("Work ended with id: %lu \n", payload->id);
//     }
//     default:
//         break;
//     }

// }


// Test structures

// This struct added for test purposes
