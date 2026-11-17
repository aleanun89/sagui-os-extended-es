/* ssl_client.h - sustituto (D376) del cliente TLS de miniJVM, que usa mbedtls: en SaguiOS no hay TLS
 * desde Java todavía; las funciones devuelven error (ver minijvm_sustitutos.c). */
#ifndef SAGUI_SSL_CLIENT_H
#define SAGUI_SSL_CLIENT_H
typedef struct _SSLC_Entry { int fd; } SSLC_Entry;
int sslc_init(SSLC_Entry *e);
int sslc_wrap(SSLC_Entry *e, int fd, const char *host);
int sslc_connect(SSLC_Entry *e, const char *host, const char *port);
int sslc_close(SSLC_Entry *e);
int sslc_read(SSLC_Entry *e, void *buf, int len);
int sslc_write(SSLC_Entry *e, const void *buf, int len);
#endif
