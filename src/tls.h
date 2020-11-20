#ifndef TLS_H
#define TLS_H

#include <openssl/ssl.h>

#define CA_CERT_FILE "CA/ca.crt"
#define SERVER_CERT_FILE "CA/server.crt"
#define SERVER_KEY_FILE "CA/server.key"

void SSL_init();

SSL_CTX *generate_SSL_CTX();

#endif