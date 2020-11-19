#include "../src/http.h"
#include "../src/option.h"
#include <stdio.h>

void str_view_show(str_view sv)
{
    for (int i = 0; i < sv.length; ++i)
        putchar(sv.str[i]);
}

void test_http_request()
{
    const char str[] = "POST / HTTP/1.1\r\nHost: www.sina.com.cn\r\nConnection: close\r\nTransfer-Encoding: chunked\r\n\r\n"
        "5\r\nhello\r\n5\r\nworld\r\n0\r\n\r\n";
    http_request req;
    http_request_init(&req);
    int res = http_request_from_buffer(&req, str, sizeof(str) - 1);

    printf("%s %s %s\n", http_method_str[req.method], req.location, req.version);
    printf("headerlen: %d\n", req.header.length);
    for (int i = 0; i < req.header.length; ++i)
    {
        http_header_item* item = req.header.items + i;
        
        printf("%s: %s\n", item->key, item->value);
    }
    puts(req.body);
    http_request_free(&req);
}

void test_http_response()
{
    http_response res;
    http_response_init(&res);

    res.status = OK;
    http_header_append(&res.header, sdsnew("Connection"), sdsnew("Close"));
    http_header_append(&res.header, sdsnew("aka "), sdsnew("777"));
    res.body = sdsnew("<html>hello world</html>");

    http_response_to_buffer(&res);

    puts(res.raw_data);

    http_response_free(&res);
}

int main(int argc, char*argv[])
{
    // test_http_request();
    // puts("\n\n\n");
    // test_http_response();
    struct options options;
    options = parse_options(argc, argv);
    
    server_init(&options);

    server_run();
    server_destroy();
    // test_http_request();
    // puts("\n\n\n");
    // test_http_response();
    // test_http_path();
    // sds str=sdsnew("test");
    // printf("%d\n", strcmp(str, "test"));
}
