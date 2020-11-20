#ifndef HASH_TABLE_H
#include "uhash.h"
#include "sds.h"

typedef struct {
    int key;
    sds buffer;
    int status;
    int times;
    UT_hash_handle hh;
} connection_info;

enum connection_status
{
    CONN_START,
    CONN_SENDING,
    CONN_END
};

#define MAX_TIMES 5
#define TIME_OUT  1
#define KEEP_ALIVE_PARAMS "timeout=10, max=5"

extern connection_info* connections;

connection_info* connection_add(int fd);
connection_info* connection_find(int fd);
void connection_remove(int fd);
void connection_mask_end(int fd);
void connection_update(int fd, int* is_start, int* is_close);

#endif // HASH_TABLE_H
