#include "tls.h"
#include <openssl/err.h>

static void *my_zeroing_malloc(size_t howmuch)
{
  return calloc(1, howmuch);
}

void SSL_init()
{
    CRYPTO_set_mem_functions(my_zeroing_malloc, realloc, free);
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
}

SSL_CTX *generate_SSL_CTX()
{
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if(!ctx) {
        printf("SSL_CTX_new failed!\n");
        goto err;
    }
    // 不验证对端
    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);
    // 加载CA证书
    // if(!SSL_CTX_load_verify_locations(ctx, CA_CERT_FILE, NULL)) {
    //     printf("SSL_CTX_load_verify_locations failed!\n");
    //     goto err;
    // }
    // 加载服务器证书
    if(SSL_CTX_use_certificate_file(ctx, SERVER_CERT_FILE, SSL_FILETYPE_PEM) <= 0) {
        printf("SSL_CTX_use_certificate_file failed!\n");
        goto err;
    }
    // 加载服务器私钥
    if(SSL_CTX_use_PrivateKey_file(ctx, SERVER_KEY_FILE, SSL_FILETYPE_PEM) <= 0) {
        printf("SSL_CTX_use_PrivateKey_file failed!\n");
        goto err;
    }

    if(!SSL_CTX_check_private_key(ctx)) {
        printf("SSL_CTX_check_private_key error!\n");
        goto err;
    }

    printf("SSL 准备OK\n");

    return ctx;
err:
    if(ctx)
        SSL_CTX_free(ctx);
    return NULL;
}