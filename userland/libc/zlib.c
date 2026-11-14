/* zlib.c - la parte de zlib que usa NetSurf (D184), sobre la compresión de
 * la libsagui (compressao.h, D103: inflar y gzip_descomprimir).
 *
 * Carencia: el SaguiOS no tiene zlib con flujos; la libsagui descomprime de
 * una vez, todo en memoria. NetSurf lee con gz* los ficheros de mensajes
 * (comprimidos o no) y con inflate() los mensajes incluidos en el binario,
 * que le llegan enteros: con eso basta descomprimir todo de una vez y
 * entregarlo a trozos.
 */
#include "sagui.h"
#include "compressao.h"

#include "errno.h"
#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "zlib.h"

struct ns_gz {
    uint8_t *dados;      /* el fichero descomprimido (malloc) */
    int tam, pos;
};

/* Descomprime gzip (o zlib) a un búfer de malloc; NULL si no es válido. */
static uint8_t *descomprimir(const uint8_t *ent, int n, int *tam)
{
    if (eh_gzip(ent, n)) {
        uint8_t *s;
        int t;
        if (gzip_descomprimir(ent, n, &s, &t, NULL, 0) < 0) return NULL;
        uint8_t *c = malloc((size_t)t + 1);   /* lo de la libsagui se libera con libera() */
        if (c) memcpy(c, s, (size_t)t);
        libera(s);
        *tam = t;
        return c;
    }
    /* zlib: 2 bytes de cabecera, DEFLATE crudo y 4 de Adler-32. Se prueba
     * con búferes cada vez más grandes. */
    if (n < 6 || (ent[0] & 0x0F) != 8 || ((ent[0] << 8) | ent[1]) % 31) return NULL;
    for (int max = n * 4 + 1024; max <= (int)COMP_LIMITE_MAX; max *= 2) {
        uint8_t *s = malloc((size_t)max);
        if (!s) return NULL;
        int r = inflar(ent + 2, n - 2, s, max);
        if (r >= 0) { *tam = r; return s; }
        free(s);
        if (r != COMP_ERRO_CHEIO) return NULL;
    }
    return NULL;
}

gzFile gzopen(const char *caminho, const char *modo)
{
    if (!modo || modo[0] != 'r') { errno = ENOSYS; return NULL; }   /* solo lectura */
    FILE *f = fopen(caminho, "rb");
    if (!f) return NULL;
    size_t cap = 16384, n = 0;
    uint8_t *b = malloc(cap);
    size_t r;
    while (b && (r = fread(b + n, 1, cap - n, f)) > 0) {
        n += r;
        if (n == cap) {
            uint8_t *nb = realloc(b, cap * 2);
            if (!nb) { free(b); b = NULL; break; }
            b = nb;
            cap *= 2;
        }
    }
    fclose(f);
    if (!b) { errno = ENOMEM; return NULL; }
    gzFile g = malloc(sizeof *g);
    if (!g) { free(b); errno = ENOMEM; return NULL; }
    g->pos = 0;
    int t;
    uint8_t *d = eh_gzip(b, (int)n) ? descomprimir(b, (int)n, &t) : NULL;
    if (d) { free(b); g->dados = d; g->tam = t; }
    else { g->dados = b; g->tam = (int)n; }   /* sin comprimir: tal cual */
    return g;
}

int gzread(gzFile g, void *buf, unsigned n)
{
    if (!g) return -1;
    int d = g->tam - g->pos;
    if ((int)n < d) d = (int)n;
    memcpy(buf, g->dados + g->pos, (size_t)d);
    g->pos += d;
    return d;
}

char *gzgets(gzFile g, char *buf, int n)
{
    if (!g || n <= 0 || g->pos >= g->tam) return NULL;
    int i = 0;
    while (i < n - 1 && g->pos < g->tam) {
        char c = (char)g->dados[g->pos++];
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = 0;
    return buf;
}

int gzeof(gzFile g) { return !g || g->pos >= g->tam; }

int gzclose(gzFile g)
{
    if (!g) return Z_STREAM_ERROR;
    free(g->dados);
    free(g);
    return Z_OK;
}

int gzputs(gzFile g, const char *s) { (void)g; (void)s; return -1; }
int gzprintf(gzFile g, const char *fmt, ...) { (void)g; (void)fmt; return -1; }
const char *gzerror(gzFile g, int *erro) { (void)g; if (erro) *erro = Z_ERRNO; return "zlib del SaguiOS: solo lectura"; }

/* ---- inflate() de una vez ---- */
typedef struct {
    uint8_t *dados;
    int tam, pos;
    bool feito;
} estado_t;

int inflateInit2(z_streamp s, int bits)
{
    (void)bits;
    estado_t *e = malloc(sizeof *e);
    if (!e) return Z_MEM_ERROR;
    memset(e, 0, sizeof *e);
    s->state = e;
    s->total_in = s->total_out = 0;
    s->msg = NULL;
    return Z_OK;
}

int inflateInit(z_streamp s) { return inflateInit2(s, MAX_WBITS); }

int inflate(z_streamp s, int flush)
{
    (void)flush;
    estado_t *e = s->state;
    if (!e) return Z_STREAM_ERROR;
    if (!e->feito) {
        e->dados = descomprimir(s->next_in, (int)s->avail_in, &e->tam);
        if (!e->dados) { s->msg = "datos comprimidos no válidos"; return Z_DATA_ERROR; }
        s->total_in += s->avail_in;
        s->next_in += s->avail_in;
        s->avail_in = 0;
        e->feito = true;
    }
    int d = e->tam - e->pos;
    if ((int)s->avail_out < d) d = (int)s->avail_out;
    memcpy(s->next_out, e->dados + e->pos, (size_t)d);
    e->pos += d;
    s->next_out += d;
    s->avail_out -= (uInt)d;
    s->total_out += (uLong)d;
    if (e->pos >= e->tam) return Z_STREAM_END;
    return d ? Z_OK : Z_BUF_ERROR;
}

int inflateEnd(z_streamp s)
{
    estado_t *e = s->state;
    if (e) { free(e->dados); free(e); }
    s->state = NULL;
    return Z_OK;
}
