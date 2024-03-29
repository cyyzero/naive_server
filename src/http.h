#ifndef HTTP_H
#define HTTP_H

#include <stdint.h>
#include "dynamic_string.h"
#include "str_view.h"

enum http_method
{
    GET,
    POST
};

static const char*http_method_str[] =
{
    "GET",
    "POST",
};

enum http_status
{
    OK,
    BAD_REQUEST,
    NOT_FOUND,
    INTERNAL_ERROR,
    VERSION_NOT_SUPPORT,
};

static int http_status_num[] = {
    200,
    400,
    404,
    500,
    505,
};

static const char* http_status_str[] = {
    "200",
    "400",
    "404",
    "500",
    "505",
};

static const char* http_status_desc[] = {
    "OK",
    "Bad Request",
    "Not Found",
    "Internal Server Error",
    "HTTP Version Not Supported",
};

typedef struct
{
    dynamic_string key;
    dynamic_string value;
} http_header_item;

typedef struct
{
    http_header_item * items;
    int length;
    int capacity;
} http_header;

http_header http_header_init();
void http_header_append(http_header* header, dynamic_string key, dynamic_string value);
http_header_item* http_header_find(const http_header* header, const char* key);
void http_header_free(http_header* header);

typedef struct
{
    int method;
    sds location;
    sds version;
    // simple dynamic array instead of complex data struct such as hash table or BST
    http_header header;
    dynamic_string body;

    // message from client
    dynamic_string raw_data;
} http_request;

typedef struct
{
    int status;
    http_header header;
    dynamic_string body;

    // message to client
    dynamic_string raw_data;
} http_response;


void http_request_init(http_request* req);
void http_request_free(http_request* request);

void http_response_init(http_response* response);
void http_response_free(http_response* response);

// parse message from client
int http_request_from_buffer(http_request* request, const char* buf, size_t length);

// 
dynamic_string http_response_to_buffer(http_response* response);
void http_response_add_header(http_response* response, dynamic_string key, dynamic_string value);


// process uri
dynamic_string http_parse_location(dynamic_string loation);

// http connection state
enum connection_state {
    CONN_DISCONNECTED,
    CONN_CONNECTING,
    CONN_IDLE,
    CONN_READING,
    CONN_WRITING
};

// state in exchange msg
enum transfer_state
{
    TRANS_START,
    TRANS_SENDING,
    TRANS_END,
};

#define MAX_TIMES 5
#define TIME_OUT  10
#define KEEP_ALIVE_PARAMS "timeout=10, max=5"

// http connection
typedef struct {
    enum connection_state state;
    enum transfer_state trans_state;
    int times;
    struct bufferevent *bufev;

    http_request *request;

    void (*cb)(struct http_connection *, void *);
	void *cb_arg;

	void (*closecb)(struct http_connection *, void *);
	void *closecb_arg;

    struct event_base *base;
} http_connection;

http_connection *http_connection_new(struct event_base *base, struct bufferevent *env);

void get_requests_cb(struct bufferevent *bufev, void *arg);

void http_connection_set_request_cb(http_connection *conn, void (*cb)(http_connection *conn, void *), void *arg);

void http_connection_free(http_connection *conn);
void connection_trans_state_update(http_connection* conn, int *is_start, int* is_end);


// check whether Connection: close in header
int is_connection_close(const http_request* req);
#endif // HTTP_H
