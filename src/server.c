#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>

#include <event.h>
#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <openssl/err.h>

#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#include "option.h"
#include "http.h"
#include "dynamic_string.h"
#include "tls.h"

#define FORM_DATA "multipart/form-data"
#define BOUNDARY_STR "boundary="
#define FILE_NAME_STR "filename="
#define LEN(str) (sizeof(str) - 1)
struct server_t
{
    struct event_base *base;
};

static struct server_t server;

SSL_CTX *ctx;

static const char* root_dir;

static const struct table_entry
{
    const char *extension;
    const char *content_type;
} content_type_table[] = {
    {"txt", "text/plain"},
    {"c", "text/plain"},
    {"h", "text/plain"},
    {"html", "text/html"},
    {"htm", "text/htm"},
    {"css", "text/css"},
    {"gif", "image/gif"},
    {"jpg", "image/jpeg"},
    {"jpeg", "image/jpeg"},
    {"png", "image/png"},
    {"pdf", "application/pdf"},
    {"ps", "application/postscript"},
    {NULL, NULL},
};

static const char *guess_content_type(const char *path)
{
    const char *last_period, *extension;
    const struct table_entry *ent;
    last_period = strrchr(path, '.');
    if (!last_period || strchr(last_period, '/'))
        goto not_found; /* no exension */
    extension = last_period + 1;
    for (ent = &content_type_table[0]; ent->extension; ++ent)
    {
        if (!evutil_ascii_strcasecmp(ent->extension, extension))
            return ent->content_type;
    }

not_found:
    return "application/misc";
}

static int save_file(const char *path, str_view content)
{
    FILE *file = fopen(path, "w");
    int res = 0;
    if (file == NULL)
    {
        res = -1;
    }
    else
    {
        if (fwrite(content.str, content.length, 1, file) != content.length)
            res - 1;
        fclose(file);
    }
    return res;
}

static void process_get(const http_request *req, http_response *res)
{
    struct stat st;
    int fd = -1, is_chunked = 0;
    sds payload = sdsempty();
    sds length = sdsempty();

    sds path = http_parse_location(req->location);
    sds wholepath = sdscat(sdsnew(root_dir), path);

    if (stat(wholepath, &st) < 0)
        goto err;

    if (S_ISDIR(st.st_mode))
    {
        // if path is a directory

        DIR *d;
        struct dirent *ent;

        sds trail = sdsempty();

        if (!sdslen(wholepath) || req->location[sdslen(wholepath) - 1] != '/')
            sdscpy(trail, "/");

        if (!(d = opendir(wholepath)))
            goto err;

        payload = sdscatfmt(payload,
                            "<!DOCTYPE html>\n"
                            "<html>\n <head>\n"
                            "  <meta charset='utf-8'>\n"
                            "  <title>%s</title>\n"
                            "  <base href='%s%s'>\n"
                            " </head>\n"
                            " <body>\n"
                            "  <h1>%s</h1>\n"
                            "  <ul>\n",
                            path, /* XXX html-escape this. */
                            path, /* XXX html-escape this? */
                            trail,
                            path /* XXX html-escape this */);

        while ((ent = readdir(d)))
        {
            const char *name = ent->d_name;

            payload = sdscatfmt(payload,
                                "    <li><a href=\"%s\">%s</a>\n",
                                name, name);
        }

        payload = sdscat(payload, "</ul></body></html>\n");

        closedir(d);

        http_header_append(&(res->header), sdsnew("Content-Type"), sdsnew("text/html"));
    }
    else
    {
        // is a file
        const char *type = guess_content_type(path);

        if ((fd = open(wholepath, O_RDONLY)) < 0)
        {
            goto err;
        }

        http_header_append(&(res->header), sdsnew("Content-Type"), sdsnew(type));

#ifdef _DEBUG
        const int buf_len = 512;
#else
        const int buf_len = 4096;
#endif
        char buff[buf_len];
        int n = 0;
        while (n = read(fd, buff, buf_len))
        {
            char hex[24];
            if (n == buf_len || is_chunked)
            {
                if (!is_chunked)
                    is_chunked = 1;
                snprintf(hex, 24, "%x", n);
                payload = sdscatfmt(payload, "%s\r\n", hex);
                payload = sdscatlen(payload, buff, n);
                payload = sdscat(payload, "\r\n");
            }
            else
            {
                payload = sdscatlen(payload, buff, n);
            }
        }
    }
    if (is_chunked)
    {
        payload = sdscat(payload, "0\r\n\r\n");
#ifdef _DEBUG
        fwrite(payload, sdslen(payload), 1, stdout);
        // payload = sdsnew(
        //     "7\r\n"
        //     "Mozilla\r\n"
        //     "9\r\n"
        //     "Developer\r\n"
        //     "7\r\n"
        //     "Network\r\n"
        //     "0\r\n"
        //     "\r\n"
        // );
#endif
    }
    res->status = OK;
    goto done;

err:
    res->status = NOT_FOUND;
    payload = sdscatfmt(payload, "Document %S was not found", path);
    if (fd >= 0)
    {
        close(fd);
    }
done:
    length = sdscatfmt(length, "%u", (unsigned int)sdslen(payload));
    char date[50];
    evutil_date_rfc1123(date, sizeof(date), NULL);
    if (is_chunked)
    {
        http_header_append(&res->header, sdsnew("Transfer-Encoding"), sdsnew("chunked"));
    }
    else
    {
        http_header_append(&(res->header), sdsnew("Content-Length"), length);
    }

    http_header_append(&(res->header), sdsnew("Date"), sdsnew(date));

    res->body = sdsdup(payload);
    sdsfree(wholepath);
    sdsfree(path);
    sdsfree(payload);
}

static void process_post(const http_request *req, http_response *res)
{
    const http_header *header = &req->header;
    const char *boundary = NULL, *body = req->body, *cur = NULL;
    sds file_name = sdsempty(), path = sdsnew(root_dir), location = NULL;
    http_header_item *item = NULL;
    int path_origin_len = 0;
    DIR *dir = NULL;

    location = http_parse_location(req->location);
    path = sdscat(path, location);
    path = sdscat(path, "/");
    dir = opendir(path);
    if (dir == NULL)
        goto end;
    printf("path: %s", path);
    path_origin_len = sdslen(path);

    item = http_header_find(header, "Content-Type");
    if (item != NULL)
    {
        int boundary_len = 0;
        const char *value = item->value;

        // check whether is multipart/form-data or not
        str_view sw = str_view_init(value, LEN(FORM_DATA));
        if (!str_view_is_same2(sw, FORM_DATA))
        {
            res->status = INTERNAL_ERROR;
            goto end;
        }
        value += LEN(FORM_DATA);
        if ((boundary = strstr(value, BOUNDARY_STR)) == NULL)
        {
            res->status = BAD_REQUEST;
            goto end;
        }
        boundary += LEN(BOUNDARY_STR);
        boundary_len = strlen(boundary);
        cur = body;
        cur = strstr(cur, boundary);
        if (cur == NULL)
            goto end;
        // pass boundary and "\r\n"
        cur += (boundary_len + 2);
        for (;;)
        {
            str_view file_content;
            cur = strstr(cur, FILE_NAME_STR);
            if (cur == NULL)
            {
                break;
            }
            cur += (LEN(FILE_NAME_STR) + 1);
            const char *tmp = cur;
            while (*cur && *cur != '"')
                ++cur;
            file_name = sdscatlen(file_name, tmp, cur - tmp);
            cur = strstr(cur, "\r\n\r\n");
            if (cur == NULL)
            {
                break;
            }
            tmp = (cur += 4);
            cur = strstr(cur, boundary);
            if (cur == NULL)
            {
                res->status = BAD_REQUEST;
                goto end;
            }
            file_content.str = tmp;
            file_content.length = cur - tmp - 2;
            path = sdscat(path, file_name);
            if (save_file(path, file_content) == -1)
            {
                res->status = INTERNAL_ERROR;
            }
            else
            {
                res->status = OK;
            }
            sdsrange(path, 0, path_origin_len - 1);
            sdsclear(file_name);
        }
    }

end:
    http_header_append(&res->header, sdsnew("Content-Length"), sdsnew("0"));
    closedir(dir);
    sdsfree(path);
    sdsfree(file_name);
    sdsfree(location);
}

static void socket_read_cb(struct bufferevent *bev, void *arg)
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
        while ((len = bufferevent_read(bev, msg, sizeof(msg))))
        {
            buf = sdscatlen(buf, msg, len);
        }
#ifdef _DEBUG
        fwrite(buf, sdslen(buf), 1, stdout);
#endif
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
        process_get(&req, &res);
        break;

    case POST:
        fwrite(msg, len, 1, stdout);
        process_post(&req, &res);
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
    // unsigned long error =  bufferevent_get_openssl_error(bev);
    // printf("lib: %s\n", ERR_lib_error_string(error));
    // printf("func: %s\n", ERR_func_error_string(error));
    // printf("reason: %s\n", ERR_reason_error_string(error));
    if (event & BEV_EVENT_EOF)
        printf("connection closed\n");
    else if (event & BEV_EVENT_ERROR)
        printf("some other error\n");
    else if (event & BEV_EVENT_READING)
        printf("BEV_EVENT_READING\n");
    else if (event & BEV_EVENT_WRITING)
        printf("BEV_EVENT_WRITING\n");
    else if (event & BEV_EVENT_TIMEOUT)
        printf("BEV_EVENT_TIMEOUT\n");
    else if (event & BEV_EVENT_CONNECTED)
        printf("BEV_EVENT_CONNECTED\n");

    //这将自动close套接字和free读写缓冲区
    // bufferevent_free(bev);
}

static void accept_cb(int fd, short events, void *arg)
{
    evutil_socket_t sockfd;

    struct sockaddr_in client;
    socklen_t len = sizeof(client);

    sockfd = accept(fd, (struct sockaddr *)&client, &len);
    evutil_make_socket_nonblocking(sockfd);

    printf("accept a client %d\n", sockfd);

    struct event_base *base = (struct event_base *)arg;

    // struct bufferevent* bev = bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);
    struct bufferevent* bev = bufferevent_openssl_socket_new(base, sockfd, SSL_new(ctx), BUFFEREVENT_SSL_ACCEPTING, BEV_OPT_CLOSE_ON_FREE);
    bufferevent_setcb(bev, socket_read_cb, NULL, event_cb, arg);

    bufferevent_enable(bev, EV_READ | EV_PERSIST);
}

void server_init(struct options *options)
{
    int errno_save;
    evutil_socket_t listener;

    SSL_init();
    if((ctx = generate_SSL_CTX()) == NULL) {
        perror("SSL new failed!");
        exit(-1);
    }

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

    if (bind(listener, (struct sockaddr *)&sin, sizeof(sin)) == -1)
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
    struct event *ev_listen = event_new(server.base, listener, EV_READ | EV_PERSIST,
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