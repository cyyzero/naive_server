#include "http.h"
#include "str_view.h"
#include "dynamic_string.h"

#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#include <stdlib.h>

http_header http_header_init()
{
    http_header header;
    header.length = 0;
    // default 4, capacity * 2 when realloc
    header.capacity = 4;
    header.items = malloc(sizeof(http_header_item) * header.capacity);
    return header;
}

void http_header_append(http_header *header, dynamic_string key, dynamic_string value)
{
    if (header->length == header->capacity)
    {
        header->capacity *= 2;
        header->items = realloc(header->items, header->capacity);
        if (header->items == NULL)
        {
            perror("out of memory.");
            exit(-1);
        }
    }
    header->items[header->length].key = key;
    header->items[header->length].value = value;
    ++header->length;
}

http_header_item* http_header_find(http_header* header, const char* key)
{
    for (int i = 0; i < header->length; ++i)
    {
        if (!strcmp(header->items[i].key, key))
        {
            return &header->items[i];
        }
    }
    return NULL;
}

void http_header_free(http_header* header)
{
    for (int i = 0; i < header->length; ++i)
    {
        sdsfree(header->items[i].key);
        sdsfree(header->items[i].value);
    }
    free(header->items);
}

static dynamic_string parse_chunked_body(const char* buf, size_t length)
{
    int beg, end, chunked_length;
    beg = end = 0;
    dynamic_string body = sdsempty();
    for (;end < length;)
    {
        while (end < length && isdigit(buf[end]))
        {
            ++end;
        }
        str_view sv = str_view_init(buf+beg, end - beg);
        chunked_length = str_view_atoi(sv);
        if (chunked_length == 0)
            break;
        // pass "\r\n"
        beg = end+ 2;
        body = sdscatlen(body, buf+beg, chunked_length);
        if (end == length)
            break;
        // pass chunked bytes and "\r\n"
        beg = end = (beg + chunked_length + 2);
    }
    return body;
}

void http_request_init(http_request* req)
{
    req->method = -1;
    req->location = NULL;
    req->version = NULL;
    req->header = http_header_init();
    req->body = NULL;
    req->raw_data = NULL;
}

void http_request_free(http_request* req)
{
    sdsfree(req->location);
    sdsfree(req->version);
    http_header_free(&req->header);
    sdsfree(req->body);
    sdsfree(req->raw_data);
}

void http_response_init(http_response* res)
{
    res->status = 0;
    res->header = http_header_init();
    res->body = NULL;
    res->raw_data = sdsempty();
}

void http_response_free(http_response* res)
{
    http_header_free(&res->header);
    sdsfree(res->body);
    sdsfree(res->raw_data);

}

int http_request_from_buffer(http_request* request, const char* buf, size_t length)
{
    if (length <= 4)
        return -1;
    int beg, end;

    // parse start line
    // parse method
    beg = end = 0;
    while (end < length && buf[end] != ' ')
        ++end;
    if (end == length)
    {
        fprintf(stderr, "incomplete start line");
        return -1;
    }
    str_view sv = str_view_init(buf + beg, end-beg);
    if (str_view_is_same2(sv, "GET"))
    {
        request->method = GET;
    }
    else if (str_view_is_same2(sv, "POST"))
    {
        request->method = POST;
    }
    else
    {
        fprintf(stderr, "unsupport other http method");
        return -1;
    }

    // parse location
    // pass char ' '
    beg = ++end;
    while (end < length && buf[end] != ' ')
    {
        ++end;
    }
    if (end == length)
    {
        fprintf(stderr, "incomplete start line");
        return -1;
    }
    request->location = sdsnewlen(buf+beg, end-beg);

    // parse version
    // pass char ' '
    beg = ++end;
    while (end < length && buf[end] != '\r')
    {
        ++end;
    }
    sv = str_view_init(buf+beg, end-beg);
    if (!str_view_is_same2(sv, "HTTP/1.1"))
    {
        fprintf(stderr, "only support http 1.1\n");
        return -1;
    }
    request->version = sdsnewlen(buf + beg, end-beg);

    // parse headers
    // pass char '\r\n'
    dynamic_string key, value;
    beg = (end += 2);
    if (beg >= length)
    {
        return -1;
    }
    for (;;)
    {
        if (end +1 < length && buf[end] == '\r' && buf[end+1] == '\n')
        {
            beg = (end += 2);
            break;
        }
        while (end < length && buf[end] != ':')
            ++end;
        if (end == length)
        {
            // TODO: incomplete msg, need to do something 
            return -1;
        }
        key = sdsnewlen(buf + beg, end-beg);
        // pass ": "
        beg = (end += 2);
        if (end >= length)
        {
            return -1;
        }
        while (end < length && buf[end] != '\r')
        {
            ++end;
        }
        if (end == length)
        {
            return -1;
        }
        value = sdsnewlen(buf + beg, end-beg);
        http_header_append(&request->header, key, value);
        // pass "\r\n"
        beg = (end+=2);
    }

    // GET method does not have body
    if (request->method == GET)
    {
        return 0;
    }

    int body_length = 0;
    http_header_item *item = http_header_find(&request->header, "Content-Length");
    if (item)
    {
        body_length = atoi(item->value);
        request->body = sdsnewlen(buf + beg, body_length);
    }
    else
    {
        item = http_header_find(&request->header, "Transfer-Encoding");
        if (item != NULL && !strcmp(item->value, "chunked"))
        {
            request->body = parse_chunked_body(buf + beg, length - beg);
        }
        else
        {
            body_length = length - beg;
            request->body = sdsnewlen(buf+beg, body_length);
        }
        
    }

    return 0;

}

dynamic_string http_response_to_buffer(http_response* response)
{
    response->raw_data = sdsnew("HTTP/1.1 ");
    response->raw_data = sdscatfmt(response->raw_data, "%s %s\r\n", http_status_str[response->status], http_status_desc[response->status]);

    for (int i = 0; i < response->header.length; ++i)
    {
        response->raw_data = sdscatfmt(response->raw_data, "%s: %s\r\n", response->header.items[i].key, response->header.items->value);
    }
    response->raw_data = sdscat(response->raw_data, "\r\n");
    response->raw_data = sdscatlen(response->raw_data, response->body, sdslen(response->body));
}
