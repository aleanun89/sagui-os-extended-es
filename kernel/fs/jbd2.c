// SPDX-License-Identifier: GPL-2.0-or-later
/* jbd2.c - reproducción y escritura del diario JBD2 (D326). Ver jbd2.h.
 *
 * Reproducción (al montar un volumen que quedó con needs_recovery), en tres pasadas como
 * describe el formato:
 *   1. buscar el final: desde s_start, transacción a transacción, mientras la secuencia sea
 *      la esperada y haya commit válido (con csum v2/v3, también su suma);
 *   2. reunir las revocaciones (bloque, transacción que lo revoca);
 *   3. escribir las copias de las transacciones confirmadas en su sitio, salvo las que una
 *      revocación de esa transacción o de una posterior anula.
 * Una copia con la suma mal no se escribe y la reproducción se aborta: el volumen queda en
 * solo lectura (mejor eso que grabar un bloque estropeado sobre los metadatos).
 *
 * Escritura: una transacción cada vez, siempre desde s_first (el ext2.c la graba en su sitio
 * nada más confirmarla, D326), así el diario nunca tiene más de una pendiente. La secuencia
 * de escrituras y barreras:
 *   descriptores + copias, barrera, commit, barrera   (jbd2_confirmar)
 *   [quien llama graba los bloques en su sitio, barrera]
 *   superbloque del diario con la secuencia siguiente, barrera   (jbd2_avancar)
 * Si se corta antes del commit, la transacción no existe; después, se reproduce entera y la
 * reproducción repetida da lo mismo. */
#include "jbd2.h"
#include "memoria.h"
#include "texto.h"

#define MAGIA          0xC03B3998u
#define T_DESCRITOR    1
#define T_COMMIT       2
#define T_SB_V1        3
#define T_SB_V2        4
#define T_REVOGACAO    5

#define COMPAT_CHECKSUM   0x1u
#define INC_REVOKE        0x1u
#define INC_64BIT         0x2u
#define INC_ASYNC         0x4u
#define INC_CSUM_V2       0x8u
#define INC_CSUM_V3       0x10u
#define INC_FAST_COMMIT   0x20u
#define INC_CONHECIDOS    (INC_REVOKE | INC_64BIT | INC_ASYNC | INC_CSUM_V2 | INC_CSUM_V3)

#define TAG_ESCAPE     1u
#define TAG_MESMO_UUID 2u
#define TAG_APAGADO    4u
#define TAG_ULTIMA     8u

#include "crc32c.h"

static inline uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static inline uint16_t be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static inline void pbe32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }
static inline void pbe16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

static bool com_soma(const jbd2_t *j) { return j->csum2 || j->csum3; }

/* ------------------------------------------------------------ acceso a los bloques del diario */

static uint32_t avancar_pos(const jbd2_t *j, uint32_t p)
{
    p++;
    if (p >= j->maxlen) p = j->primeiro + (p - j->maxlen);
    return p;
}

static int ler_j(jbd2_t *j, uint32_t lb, uint8_t *buf)
{
    if (lb >= j->maxlen) return -1;
    uint32_t f = j->mapear(j->ctx, lb);
    if (!f) return -1;
    return j->ler(j->ctx, f, buf);
}

static int escrever_j(jbd2_t *j, uint32_t lb, const uint8_t *buf)
{
    if (lb >= j->maxlen) return -1;
    uint32_t f = j->mapear(j->ctx, lb);
    if (!f) return -1;
    return j->escrever(j->ctx, f, buf);
}

/* ------------------------------------------------------------ sumas */

static uint32_t soma_sb(const jbd2_t *j)
{
    uint8_t c[1024];
    memcpy(c, j->sb, 1024);
    memset(c + 0xFC, 0, 4);
    return crc32c(~0u, c, 1024);
}

/* Suma de un bloque descriptor o de revocación: la cola (últimos 4 bytes) a cero. */
static uint32_t soma_cauda(const jbd2_t *j, uint8_t *b)
{
    uint32_t guardada = be32(b + j->bloco - 4);
    memset(b + j->bloco - 4, 0, 4);
    uint32_t s = crc32c(j->semente, b, j->bloco);
    pbe32(b + j->bloco - 4, guardada);
    return s;
}

static uint32_t soma_commit(const jbd2_t *j, uint8_t *b)
{
    uint32_t guardada = be32(b + 16);
    memset(b + 16, 0, 4);
    uint32_t s = crc32c(j->semente, b, j->bloco);
    pbe32(b + 16, guardada);
    return s;
}

static uint32_t soma_tag(const jbd2_t *j, uint32_t seq, const uint8_t *dados)
{
    uint8_t s[4];
    pbe32(s, seq);
    return crc32c(crc32c(j->semente, s, 4), dados, j->bloco);
}

/* ------------------------------------------------------------ superbloque */

static int gravar_sb(jbd2_t *j)
{
    pbe32(j->sb + 0x18, j->seq);
    pbe32(j->sb + 0x1C, j->inicio);
    if (com_soma(j)) pbe32(j->sb + 0xFC, soma_sb(j));
    /* El superbloque del diario ocupa el primer bloque: se lee entero para no pisar el resto. */
    if (ler_j(j, 0, j->buf) < 0) return -1;
    memcpy(j->buf, j->sb, 1024);
    if (escrever_j(j, 0, j->buf) < 0) return -1;
    return j->sincronizar(j->ctx) < 0 ? -1 : 0;
}

static int falha(jbd2_t *j, const char *m) { j->motivo = m; return -1; }

int jbd2_abrir(jbd2_t *j)
{
    j->motivo = NULL;
    j->escrita_ok = false;
    j->buf = kmalloc(j->bloco);
    j->buf2 = kmalloc(j->bloco);
    if (!j->buf || !j->buf2) return falha(j, "sin memoria para el diario");
    j->maxlen = 1;   /* para poder leer el bloque 0 */
    if (j->bloco < 1024 || ler_j(j, 0, j->buf) < 0) return falha(j, "no se puede leer el diario");
    memcpy(j->sb, j->buf, 1024);
    uint32_t tipo = be32(j->sb + 4);
    if (be32(j->sb) != MAGIA || (tipo != T_SB_V1 && tipo != T_SB_V2)) return falha(j, "el diario no es JBD2");
    j->v2 = tipo == T_SB_V2;
    if (be32(j->sb + 0x0C) != j->bloco) return falha(j, "el diario tiene otro tamaño de bloque");
    j->maxlen = be32(j->sb + 0x10);
    j->primeiro = be32(j->sb + 0x14);
    j->seq = be32(j->sb + 0x18);
    j->inicio = be32(j->sb + 0x1C);
    if (j->maxlen > j->blocos_inode || j->primeiro < 1 || j->primeiro + 4 > j->maxlen)
        return falha(j, "superbloque del diario no válido");
    if (j->inicio && (j->inicio < j->primeiro || j->inicio >= j->maxlen)) return falha(j, "inicio del diario no válido");
    j->compat = j->v2 ? be32(j->sb + 0x24) : 0;
    j->incompat = j->v2 ? be32(j->sb + 0x28) : 0;
    j->ro = j->v2 ? be32(j->sb + 0x2C) : 0;
    j->csum2 = (j->incompat & INC_CSUM_V2) != 0;
    j->csum3 = (j->incompat & INC_CSUM_V3) != 0;
    j->b64 = (j->incompat & INC_64BIT) != 0;
    if (j->csum2 && j->csum3) return falha(j, "diario con csum v2 y v3 a la vez");
    if (com_soma(j)) {
        if (j->sb[0x50] != 4) return falha(j, "diario con un tipo de suma desconocido");
        j->semente = crc32c(~0u, j->sb + 0x30, 16);
        if (be32(j->sb + 0xFC) != soma_sb(j)) return falha(j, "el superbloque del diario tiene la suma mal");
    }
    if (j->incompat & ~INC_CONHECIDOS) return falha(j, "diario con características desconocidas");
    if (j->compat & COMPAT_CHECKSUM) return falha(j, "diario con sumas v1 (no soportadas)");
    if (j->ro) return falha(j, "diario con características desconocidas");
    j->tam_tag = j->csum3 ? 16 : (j->b64 ? 12 : 8);
    if (j->v2 && be32(j->sb + 0x40) > 1) return falha(j, "diario compartido por varios volúmenes");
    j->escrita_ok = true;
    return 0;
}

bool jbd2_vazio(const jbd2_t *j) { return j->inicio == 0; }

/* ------------------------------------------------------------ revocaciones */

#define REV_BALDES 256
typedef struct { uint32_t bloco, seq; int32_t prox; } rev_t;
typedef struct { rev_t *v; uint32_t n, cap; int32_t balde[REV_BALDES]; } revs_t;

static int rev_por(revs_t *r, uint32_t b, uint32_t seq)
{
    uint32_t h = (b * 2654435761u) >> 24;
    for (int32_t k = r->balde[h]; k >= 0; k = r->v[k].prox)
        if (r->v[k].bloco == b) { if ((int32_t)(seq - r->v[k].seq) > 0) r->v[k].seq = seq; return 0; }
    if (r->n == r->cap) {
        uint32_t cap = r->cap ? r->cap * 2 : 256;
        if (cap > 1u << 20) return -1;
        rev_t *nv = kmalloc(cap * sizeof *nv);
        if (!nv) return -1;
        if (r->v) { memcpy(nv, r->v, r->n * sizeof *nv); kfree(r->v); }
        r->v = nv;
        r->cap = cap;
    }
    r->v[r->n].bloco = b;
    r->v[r->n].seq = seq;
    r->v[r->n].prox = r->balde[h];
    r->balde[h] = (int32_t)r->n++;
    return 0;
}

/* ¿Una revocación de la transacción "seq" o posterior anula este bloque? */
static bool revogado(const revs_t *r, uint32_t b, uint32_t seq)
{
    uint32_t h = (b * 2654435761u) >> 24;
    for (int32_t k = r->balde[h]; k >= 0; k = r->v[k].prox)
        if (r->v[k].bloco == b) return (int32_t)(r->v[k].seq - seq) >= 0;
    return false;
}

/* ------------------------------------------------------------ reproducción */

enum { P_BUSCA, P_REVOGA, P_REPRODUZ };

/* Recorre las etiquetas de un descriptor: devuelve cuántas hay (o -1). Con "cada", la
 * llama con (bloque destino, flags, puntero a la etiqueta) para cada una. */
static int etiquetas(jbd2_t *j, const uint8_t *b, int (*cada)(jbd2_t *, uint32_t, uint32_t, const uint8_t *, void *), void *arg)
{
    uint32_t lim = j->bloco - (com_soma(j) ? 4 : 0);
    uint32_t off = 12;
    int n = 0;
    while (off + j->tam_tag <= lim) {
        const uint8_t *t = b + off;
        uint32_t destino = be32(t), flags, alto = 0;
        if (j->csum3) { flags = be32(t + 4); alto = be32(t + 8); }
        else { flags = be16(t + 6); if (j->b64) alto = be32(t + 8); }
        if (alto) return -1;                   /* volúmenes de más de 2^32 bloques: no */
        if (cada && cada(j, destino, flags, t, arg) < 0) return -1;
        n++;
        off += j->tam_tag;
        if (!(flags & TAG_MESMO_UUID)) off += 16;
        if (flags & TAG_ULTIMA) break;
    }
    return n;
}

typedef struct { uint32_t pos, seq; revs_t *revs; int erro; } rep_t;

static int reproduzir_tag(jbd2_t *j, uint32_t destino, uint32_t flags, const uint8_t *t, void *arg)
{
    rep_t *r = arg;
    uint32_t pos = r->pos;
    r->pos = avancar_pos(j, r->pos);
    if (revogado(r->revs, destino, r->seq)) { j->revogados++; return 0; }
    if (ler_j(j, pos, j->buf2) < 0) return -1;
    if (j->csum3 || j->csum2) {
        uint32_t s = soma_tag(j, r->seq, j->buf2);
        bool ok = j->csum3 ? be32(t + 12) == s : be16(t + 4) == (s & 0xFFFF);
        if (!ok) { j->motivo = "una copia del diario tiene la suma mal"; return -1; }
    }
    if (flags & TAG_ESCAPE) pbe32(j->buf2, MAGIA);
    if (j->escrever(j->ctx, destino, j->buf2) < 0) return -1;
    j->blocos_reproduzidos++;
    return 0;
}

/* Una pasada; devuelve 0 o -1. En P_BUSCA deja *fim = la primera transacción que no vale. */
static int passada(jbd2_t *j, int qual, uint32_t *fim, revs_t *revs)
{
    uint32_t seq = j->seq, pos = j->inicio;
    uint8_t *b = j->buf;
    for (uint32_t guarda = 0; guarda < 4 * j->maxlen + 16; guarda++) {
        if (qual != P_BUSCA && seq == *fim) break;
        if (ler_j(j, pos, b) < 0) { j->motivo = "no se puede leer el diario"; return -1; }
        uint32_t aqui = pos;
        pos = avancar_pos(j, pos);
        if (be32(b) != MAGIA || be32(b + 8) != seq) break;
        uint32_t tipo = be32(b + 4);
        if (tipo == T_DESCRITOR) {
            if (qual == P_BUSCA && com_soma(j) && soma_cauda(j, b) != be32(b + j->bloco - 4)) break;
            if (qual == P_REPRODUZ) {
                rep_t r = { pos, seq, revs, 0 };
                /* las etiquetas se recorren sobre una copia: reproducir usa los búferes */
                uint8_t *copia = kmalloc(j->bloco);
                if (!copia) return -1;
                memcpy(copia, b, j->bloco);
                int n = etiquetas(j, copia, reproduzir_tag, &r);
                kfree(copia);
                if (n < 0) { if (!j->motivo) j->motivo = "error al reproducir el diario"; return -1; }
                pos = r.pos;
            } else {
                int n = etiquetas(j, b, NULL, NULL);
                if (n < 0) { j->motivo = "descriptor del diario no válido"; return -1; }
                for (int k = 0; k < n; k++) pos = avancar_pos(j, pos);
            }
            (void)aqui;
            continue;
        }
        if (tipo == T_COMMIT) {
            if (qual == P_BUSCA && com_soma(j)) {
                if (b[12] != 4 || be32(b + 16) != soma_commit(j, b)) break;   /* commit roto: no vale */
            }
            seq++;
            if (qual == P_BUSCA) *fim = seq;
            continue;
        }
        if (tipo == T_REVOGACAO) {
            if (qual == P_BUSCA && com_soma(j) && soma_cauda(j, b) != be32(b + j->bloco - 4)) break;
            if (qual == P_REVOGA) {
                uint32_t usado = be32(b + 12), tam = j->b64 ? 8 : 4;
                uint32_t lim = j->bloco - (com_soma(j) ? 4 : 0);
                if (usado > lim) usado = lim;
                for (uint32_t off = 16; off + tam <= usado; off += tam) {
                    if (j->b64 && be32(b + off)) continue;
                    if (rev_por(revs, be32(b + off + (j->b64 ? 4 : 0)), seq) < 0) { j->motivo = "demasiadas revocaciones"; return -1; }
                }
            }
            continue;
        }
        break;
    }
    return 0;
}

int jbd2_recuperar(jbd2_t *j)
{
    j->reproduzidas = j->blocos_reproduzidos = j->revogados = 0;
    if (!j->escrita_ok) return -1;
    if (!j->inicio) return 0;
    uint32_t fim = j->seq;
    revs_t revs;
    memset(&revs, 0, sizeof revs);
    for (int k = 0; k < REV_BALDES; k++) revs.balde[k] = -1;
    j->motivo = NULL;
    int r = passada(j, P_BUSCA, &fim, &revs);
    if (r == 0) r = passada(j, P_REVOGA, &fim, &revs);
    if (r == 0) r = passada(j, P_REPRODUZ, &fim, &revs);
    if (revs.v) kfree(revs.v);
    if (r < 0) return -1;
    j->reproduzidas = fim - j->seq;
    if (j->sincronizar(j->ctx) < 0) return falha(j, "error al grabar lo reproducido");
    /* Vacío, con una secuencia posterior a todo lo que hubo. */
    j->seq = fim + 1;
    j->inicio = 0;
    if (gravar_sb(j) < 0) return falha(j, "error al grabar el superbloque del diario");
    return 0;
}

/* ------------------------------------------------------------ escritura */

static uint32_t tags_por_descritor(const jbd2_t *j)
{
    uint32_t espaco = j->bloco - 12 - (com_soma(j) ? 4 : 0);
    return (espaco - 16) / j->tam_tag;
}

uint32_t jbd2_capacidade(const jbd2_t *j)
{
    uint32_t livre = j->maxlen - j->primeiro, por = tags_por_descritor(j);
    if (livre < 3) return 0;
    /* n copias + ceil(n / por) descriptores + 1 commit <= livre */
    uint32_t n = (livre - 1) * por / (por + 1);
    while (n && n + (n + por - 1) / por + 1 > livre) n--;
    return n;
}

int jbd2_comecar(jbd2_t *j)
{
    if (!j->escrita_ok) return -1;
    j->inicio = j->primeiro;
    return gravar_sb(j);
}

int jbd2_avancar(jbd2_t *j)
{
    j->seq++;
    return gravar_sb(j);
}

int jbd2_fechar(jbd2_t *j)
{
    if (!j->escrita_ok) return 0;
    j->inicio = 0;
    return gravar_sb(j);
}

static void cabecalho(uint8_t *b, uint32_t tipo, uint32_t seq)
{
    pbe32(b, MAGIA);
    pbe32(b + 4, tipo);
    pbe32(b + 8, seq);
}

int jbd2_confirmar(jbd2_t *j, uint32_t n, const uint32_t *fis, uint8_t *const *dados)
{
    if (!j->escrita_ok || j->inicio != j->primeiro) return -1;
    if (!n) return 0;
    if (n > jbd2_capacidade(j)) { j->motivo = "transacción mayor que el diario"; return -1; }
    uint32_t por = tags_por_descritor(j), pos = j->primeiro, k = 0;
    uint8_t *desc = j->buf, *copia = j->buf2;
    while (k < n) {
        uint32_t pos_desc = pos;
        pos = avancar_pos(j, pos);
        memset(desc, 0, j->bloco);
        cabecalho(desc, T_DESCRITOR, j->seq);
        uint32_t off = 12;
        for (uint32_t t = 0; t < por && k < n; t++, k++) {
            memcpy(copia, dados[k], j->bloco);
            uint32_t flags = 0;
            if (be32(copia) == MAGIA) { memset(copia, 0, 4); flags |= TAG_ESCAPE; }
            if (t > 0) flags |= TAG_MESMO_UUID;
            if (t + 1 == por || k + 1 == n) flags |= TAG_ULTIMA;
            uint8_t *tg = desc + off;
            uint32_t s = com_soma(j) ? soma_tag(j, j->seq, copia) : 0;
            pbe32(tg, fis[k]);
            if (j->csum3) { pbe32(tg + 4, flags); pbe32(tg + 8, 0); pbe32(tg + 12, s); }
            else { pbe16(tg + 4, j->csum2 ? (s & 0xFFFF) : 0); pbe16(tg + 6, flags); if (j->b64) pbe32(tg + 8, 0); }
            off += j->tam_tag;
            if (!(flags & TAG_MESMO_UUID)) { memcpy(desc + off, j->sb + 0x30, 16); off += 16; }
            if (escrever_j(j, pos, copia) < 0) return falha(j, "error al escribir en el diario");
            pos = avancar_pos(j, pos);
        }
        if (com_soma(j)) pbe32(desc + j->bloco - 4, soma_cauda(j, desc));
        if (escrever_j(j, pos_desc, desc) < 0) return falha(j, "error al escribir en el diario");
    }
    if (j->sincronizar(j->ctx) < 0) return falha(j, "error al escribir en el diario");
    memset(desc, 0, j->bloco);
    cabecalho(desc, T_COMMIT, j->seq);
    if (com_soma(j)) { desc[12] = 4; desc[13] = 4; }
    pbe32(desc + 0x30, 0);
    pbe32(desc + 0x34, j->hora);
    if (com_soma(j)) pbe32(desc + 16, soma_commit(j, desc));
    if (escrever_j(j, pos, desc) < 0) return falha(j, "error al escribir en el diario");
    if (j->sincronizar(j->ctx) < 0) return falha(j, "error al escribir en el diario");
    return 0;
}

void jbd2_liberar(jbd2_t *j)
{
    if (j->buf) kfree(j->buf);
    if (j->buf2) kfree(j->buf2);
    j->buf = j->buf2 = NULL;
}
