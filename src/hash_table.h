#ifndef HASH_TABLE_H
#include "uhash.h"
#include "sds.h"
typedef struct {
    int ip;
    int port;
} request_buffer_key;

typedef struct {
    request_buffer_key key;
    sds buffer;
    UT_hash_handle hh;
} request_buffer;

#define HASH_TABLE_ADD(container, item) HASH_ADD(hh, container, key, sizeof(request_buffer_key), item)
#define HASH_TABLE_FIND(container, target_key_ptr, item) HASH_FIND(hh, container, (target_key_ptr), sizeof(request_buffer_key), item)

#define HASH_TABLE_FREE(container) \
do { \
    request_buffer* p, *tmp; \
    HASH_ITER(hh, container, p, tmp) { \
        HASH_DEL(container, p); \
        free(p); \
    } \
} while (0)

#endif // HASH_TABLE_H
