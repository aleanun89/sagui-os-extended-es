/* zlib.h - gzopen y compañía (lectura) (D184).
 *
 * Parte de la libc común (userland/libc, D210; nació con NetSurf, D184)
 * sobre la libsagui; las funciones están en los .c de userland/libc/. */
#ifndef NS_ZLIB_H
#define NS_ZLIB_H

#include <stddef.h>
/* Solo la parte de gzip que usa NetSurf para leer los ficheros de mensajes
 * y de recursos: gzopen/gzgets/gzread/gzclose. Lee ficheros sin comprimir
 * tal cual y descomprime los gzip con la libsagui (compressao.h, D103). */
typedef struct ns_gz *gzFile;
typedef unsigned char Bytef;
typedef unsigned int  uInt;
typedef unsigned long uLong;
typedef void *voidpf;
/* inflate() de una sola vez: la primera llamada descomprime toda la entrada
 * (que tiene que estar entera en next_in, como hace NetSurf con los
 * mensajes incluidos) con la libsagui y las siguientes la van entregando. */
typedef struct z_stream_s {
    const Bytef *next_in;
    uInt  avail_in;
    uLong total_in;
    Bytef *next_out;
    uInt  avail_out;
    uLong total_out;
    const char *msg;
    void *state;
    voidpf (*zalloc)(voidpf, uInt, uInt);
    void   (*zfree)(voidpf, voidpf);
    voidpf opaque;
} z_stream;
typedef z_stream *z_streamp;
#define Z_NULL 0
#define Z_NO_FLUSH 0
#define Z_FINISH 4
#define MAX_WBITS 15
#define Z_STREAM_ERROR (-2)
#define Z_BUF_ERROR (-5)
int inflateInit2(z_streamp s, int bits);
int inflateInit(z_streamp s);
int inflate(z_streamp s, int flush);
int inflateEnd(z_streamp s);
#define Z_OK 0
#define Z_STREAM_END 1
#define Z_ERRNO (-1)
#define Z_DATA_ERROR (-3)
#define Z_MEM_ERROR (-4)
gzFile gzopen(const char *caminho, const char *modo);
char  *gzgets(gzFile f, char *buf, int n);
int    gzread(gzFile f, void *buf, unsigned n);
int    gzeof(gzFile f);
int    gzclose(gzFile f);
int    gzputs(gzFile f, const char *s);
int    gzprintf(gzFile f, const char *fmt, ...);
const char *gzerror(gzFile f, int *erro);
#endif
