#include "hash_table.h"
#include <stdio.h>

connection_info* connections = NULL;

connection_info* connection_add(int fd)
{
    connection_info* conn = malloc(sizeof(connection_info));
    conn->key = fd;
    conn->buffer = NULL;
    conn->status = CONN_START;
    conn->times = 0;
    HASH_ADD_INT(connections, key, conn);
    return conn;
}

connection_info* connection_find(int fd)
{
    connection_info* conn;
    HASH_FIND_INT(connections, &fd, conn);
    return conn;
}

void connection_remove(int fd)
{
    connection_info* conn = connection_find(fd);
    if (conn)
    {
        HASH_DEL(connections, conn);
        free(conn);
    }
}

void connection_mask_end(int fd)
{
    connection_info* conn = connection_find(fd);
    if (conn)
    {
        conn->status = CONN_END;
    }
}

void connection_update(int fd, int* is_start, int* is_close)
{
    *is_start = *is_close = 0;
    connection_info* conn = connection_find(fd);
    if (!conn)
    {
        conn = connection_add(fd);
    }
#ifdef _DEBUG
    printf("conn: %d %d\n", conn->status, conn->times);
#endif
    switch (conn->status)
    {
    case CONN_START:
        *is_start = 1;
        ++conn->times;
        conn->status = CONN_SENDING;
        break;
    case CONN_SENDING:
        if (++conn->times >= MAX_TIMES)
        {
            conn->status = CONN_END;
            *is_close = 1;
        }
        break;
    case CONN_END:
        fprintf(stderr, "connection status error");
        break;
    default:
        conn->status = CONN_END;
        break;
    }
}
