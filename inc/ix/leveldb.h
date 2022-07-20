#pragma once

#include <stdint.h>
#include <ix/log.h>
#include <leveldb/c.h>


#define KEYSIZE 1024
#define VALSIZE 1024

leveldb_t * db;
leveldb_options_t *options;
leveldb_readoptions_t *roptions;
leveldb_writeoptions_t *woptions;
size_t read_len;

typedef char db_key     [1024];
typedef char db_value   [1024];

// Type of Level-db operations
typedef enum
{
    PUT,
    GET,
    DELETE,
    ITERATOR,
    CUSTOM,         // This type added for tests, see below.
    
} REQ_TYPE;

typedef struct db_req
{
    REQ_TYPE    type;
    void *      params;

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


static void init_db()
{
    log_info("Generating leveldb options \n");
    roptions = leveldb_readoptions_create();
    woptions = leveldb_writeoptions_create();
}

inline struct db_req * gen_db_pkg(REQ_TYPE req, void *parameters)
{
    return (struct db_req *)(req, parameters);
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
