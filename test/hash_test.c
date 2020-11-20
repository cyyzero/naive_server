#include "../src/hash_table.h"
#include <stdio.h>
int main()
{
    request_buffer * buffer = NULL;
    request_buffer *item1 = malloc(sizeof(request_buffer));
    request_buffer *item2 = malloc(sizeof(request_buffer));
    item1->key.ip = 1;
    item1->key.port = 1;
    item1->buffer = sdsnew("hello world");
    HASH_TABLE_ADD(buffer, item1);

    request_buffer_key key = {1, 1};

    HASH_TABLE_FIND(buffer, &key, item2);

    puts(item2->buffer);
    printf("%ld\n", sdslen(item2->buffer));

    HASH_TABLE_FREE(buffer);
}