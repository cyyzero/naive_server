#ifndef SERVER_H
#define SERVER_H

struct options;
extern void server_init(struct options* options);
extern void server_run();
extern void server_destroy();

#endif // SERVER_H