#include "../src/hash_table.h"
#include <stdio.h>
#include <assert.h>
int main()
{
    int fd = 1;
    connection_add(1);
    connection_info* conn = connection_find(1);
    printf("%d\n", conn->status);
    conn->status = CONN_END;
    conn = NULL;
    conn = connection_find(1);
    printf("%d\n", conn->status);
    connection_remove(1);
    conn = connection_find(1);
    assert(conn == NULL);
}