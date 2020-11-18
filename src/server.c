#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <event.h>
#include <event2/bufferevent.h>

#include "option.h"

struct server_t
{
    struct event_base* base;
};

static struct server_t server;

static void socket_read_cb(struct bufferevent* bev, void* arg)
{
    char msg[4096];

    size_t len = bufferevent_read(bev, msg, sizeof(msg));

    msg[len] = '\0';
    printf("recv the client msg: %s", msg);

    char reply_msg[4096] = "I have recvieced the msg: ";

    strcat(reply_msg + strlen(reply_msg), msg);
    bufferevent_write(bev, reply_msg, strlen(reply_msg));
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