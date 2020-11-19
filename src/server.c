#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <event.h>
#include <event2/bufferevent.h>

#include "option.h"
#include "http.h"
#include "dynamic_string.h"
struct server_t
{
    struct event_base* base;
};

static struct server_t server;

static const char* root_dir;

static void process_post_file(const http_request* req, http_response* res)
{
    sds body = req->body;
}

static void process_get(const http_request* req, http_response* res)
{
    
}

static void process_post(const http_request* req, http_response* res)
{
    http_header* header = &req->header;
    http_header_item* item = NULL;
    

}

static void socket_read_cb(struct bufferevent* bev, void* arg)
{
    char msg[4096];
    sds buf = NULL;
    http_request req;
    http_response res;

    http_request_init(&req);
    http_response_init(&res);

    size_t len = bufferevent_read(bev, msg, sizeof(msg));
    int ret = 0;
    if (len == sizeof(msg))
    {
        buf = sdsnewlen(msg, len);
        do {
            len = bufferevent_read(bev, msg, sizeof(msg));
            buf = sdscatlen(buf, msg, len);
        } while (len < sizeof(msg));
        ret = http_request_from_buffer(&req, buf, sdslen(buf));
    }
    else
    {
        ret = http_request_from_buffer(&req, msg, len);
    }
    
    if (ret == -1)
    {
        res.status = INTERNAL_ERROR;
        goto end;
    }

    switch (req.method)
    {
    case GET:
        process_get(&res, &req);
        break;

    case POST:
        process_post(&res, &req);
        break;

    default:
        res.status = INTERNAL_ERROR;
        break;
    }

end:
    http_response_to_buffer(&res);
    bufferevent_write(bev, res.raw_data, sdslen(res.raw_data));

    http_request_free(&req);
    http_response_free(&res);
}

static void event_cb(struct bufferevent *bev, short event, void *arg)
{

    if (event & BEV_EVENT_EOF)
        printf("connection closed\n");
    else if (event & BEV_EVENT_ERROR)
        printf("some other error\n");

    //这将自动close套接字和free读写缓冲区
    bufferevent_free(bev);
}

static void accept_cb(int fd, short events, void* arg)
{
    evutil_socket_t sockfd;

    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    sockfd = accept(fd, (struct sockaddr*)&client, &len);
    evutil_make_socket_nonblocking(sockfd);

    printf("accept a client %d\n", sockfd);

    struct event_base* base = (struct event_base*)arg;

    struct bufferevent* bev = bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, socket_read_cb, NULL, event_cb, arg);

    bufferevent_enable(bev, EV_READ | EV_PERSIST);
}


void server_init(struct options* options)
{
    int errno_save;
    evutil_socket_t listener;

    root_dir = options->docroot;
    listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1)
    {
        perror("socket create failed.");
        exit(-1);
    }

    evutil_make_listen_socket_reuseable(listener);

    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = 0;
    sin.sin_port = htons(options->port);

    if (bind(listener, (struct sockaddr*)&sin, sizeof(sin)) == -1)
    {
        perror("bind failed.");
        exit(-1);
    }

    if (listen(listener, 10) == -1)
    {
        perror("listen failed");
        exit(-1);
    }

    evutil_make_socket_nonblocking(listener);

    server.base = event_base_new();

    //添加监听客户端请求连接事件
    struct event* ev_listen = event_new(server.base, listener, EV_READ | EV_PERSIST,
                                        accept_cb, server.base);
    if (ev_listen == NULL)
    {
        perror("event_new failed.");
        exit(-1);
    }
    event_add(ev_listen, NULL);
}

void server_run()
{
    event_base_dispatch(server.base);
}

void server_destroy()
{
    event_base_free(server.base);
}