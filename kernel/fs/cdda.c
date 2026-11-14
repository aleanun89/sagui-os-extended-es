// SPDX-License-Identifier: GPL-2.0-or-later
/* cdda.c - el sistema de archivos "cdda": un CD de audio como una carpeta de WAV (D233).
 *
 * Solo lectura y sin nada grabado en el disco: la carpeta se construye con la tabla de
 * contenido (y el CD-TEXT, si lo hay) que leyó atapi.c. Contiene:
 *   - un archivo por pista de audio, "Pista01.wav"... o "01 - Título.wav" con CD-TEXT:
 *     la cabecera RIFF/WAVE de 44 bytes (PCM de 16 bits, estéreo, 44100 Hz, con el
 *     tamaño exacto de la pista) y, detrás, los sectores de la pista tal como los da el
 *     lector con READ CD (2352 bytes por sector, sin subcanal; el CD-DA ya es little
 *     endian, como el WAV);
 *   - "Lista.m3u", con todas las pistas en orden, su duración y su título.
 * Cualquier desplazamiento vale (los reproductores saltan): el byte p de los datos es el
 * byte p % 2352 del sector lba + p / 2352. Las lecturas pasan por una caché de 13
 * sectores (30 KB, lo que cabe en una orden del AHCI): un reproductor que lee de 4 KB en
 * 4 KB manda una orden al lector cada 13 sectores (174 ms de música).
 * Un sector que no se puede leer (una raya) lo resuelve atapi.c: reintenta y, si no,
 * lo da en silencio y lo anota en el log; el archivo nunca se corta. Si se saca el
 * disco, la lectura falla (y la tarea de vigilancia desmonta). */
#include "cdda.h"
#include "abi.h"
#include "console.h"
#include "memoria.h"
#include "montagem.h"
#include "texto.h"

#define CACHE_SETORES 13u
#define NOME_LISTA "Lista.m3u"
#define MAX_M3U 16384u

typedef struct {
    disco_t  *d;
    cd_toc_t *toc;                 /* copia: el lector puede volver a leer la suya */
    char     *m3u;
    uint32_t  m3u_tam;
    uint8_t  *cache;
    uint32_t  cache_lba, cache_n;  /* sectores que hay en la caché (cache_n = 0: ninguno) */
} cdda_t;

typedef struct {
    int      indice;               /* pista, o E_LISTA */
    uint32_t tam;
} cdda_arq_t;

static bool igual(const char *a, const char *b, bool sem_caixa)
{
    for (;; a++, b++) {
        char x = *a, y = *b;
        if (sem_caixa) {
            if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
            if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        }
        if (x != y) return false;
        if (!x) return true;
    }
}

#define E_LISTA 1000   /* achar: Lista.m3u */
#define E_RAIZ  1001   /* achar: la carpeta */

/* 0..qtd-1 = pista, E_LISTA, E_RAIZ, o un ERRO_* (negativo). */
static int achar(cdda_t *v, const char *rel)
{
    while (*rel == '/') rel++;
    if (!*rel) return E_RAIZ;
    for (int pasada = 0; pasada < 2; pasada++) {
        if (igual(rel, NOME_LISTA, pasada)) return E_LISTA;
        for (int i = 0; i < v->toc->qtd; i++) {
            char nome[NOME_MAX];
            cdda_nome(v->toc, i, nome, sizeof nome);
            if (igual(rel, nome, pasada)) return i;
        }
    }
    for (const char *p = rel; *p; p++)
        if (*p == '/') return ERRO_NAO_E_PASTA;
    return ERRO_NAO_EXISTE;
}

static uint32_t tamanho_de(cdda_t *v, int i)
{
    return i == E_LISTA ? v->m3u_tam : cdda_tamanho(v->toc->pista[i].setores);
}

static int c_info(void *priv, const char *rel, info_arquivo_t *inf)
{
    cdda_t *v = priv;
    int i = achar(v, rel);
    if (i == E_RAIZ) { inf->tipo = TIPO_PASTA; inf->tamanho = 0; return 0; }
    if (i < 0) return i;
    inf->tipo = TIPO_ARQUIVO;
    inf->tamanho = tamanho_de(v, i);
    return 0;
}

static int c_listar(void *priv, const char *rel, int indice, entrada_pasta_t *e)
{
    cdda_t *v = priv;
    int i = achar(v, rel);
    if (i != E_RAIZ) return i < 0 ? i : ERRO_NAO_E_PASTA;
    if (indice < 0 || indice > v->toc->qtd) return 0;
    e->tipo = TIPO_ARQUIVO;
    if (indice == v->toc->qtd) {
        txt_copia_n(e->nome, NOME_LISTA, sizeof e->nome);
        e->tamanho = v->m3u_tam;
    } else {
        cdda_nome(v->toc, indice, e->nome, sizeof e->nome);
        e->tamanho = cdda_tamanho(v->toc->pista[indice].setores);
    }
    return 1;
}

static int c_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tamanho)
{
    cdda_t *v = priv;
    cdda_arq_t *f = arq;
    int i = achar(v, rel);
    if (i == ERRO_NAO_EXISTE && (modo & ABRIR_CRIAR)) return ERRO_SO_LEITURA;
    if (i == E_RAIZ) return ERRO_E_PASTA;
    if (i < 0) return i;
    if (modo & (ABRIR_ESCREVER | ABRIR_TRUNCAR | ABRIR_ACRESCENTAR)) return ERRO_SO_LEITURA;
    f->indice = i;
    f->tam = tamanho_de(v, i);
    *tamanho = f->tam;
    return 0;
}

/* Copia "n" bytes de audio de la pista desde el byte "off" de sus datos. */
static int ler_audio(cdda_t *v, const cdda_pista_t *p, uint32_t off, uint8_t *dest, uint32_t n)
{
    uint32_t feito = 0;
    while (feito < n) {
        uint32_t s = (off + feito) / CDDA_SETOR, dentro = (off + feito) % CDDA_SETOR;
        if (s >= p->setores) break;
        uint32_t lba = p->lba + s;
        if (!v->cache_n || lba < v->cache_lba || lba >= v->cache_lba + v->cache_n) {
            uint32_t q = p->setores - s;
            if (q > CACHE_SETORES) q = CACHE_SETORES;
            v->cache_n = 0;
            if (!v->d->ler_audio || v->d->ler_audio(v->d, lba, q, v->cache) < 0) break;
            v->cache_lba = lba;
            v->cache_n = q;
        }
        const uint8_t *src = v->cache + (lba - v->cache_lba) * CDDA_SETOR + dentro;
        uint32_t resto = (v->cache_lba + v->cache_n - lba) * CDDA_SETOR - dentro;
        if (resto > n - feito) resto = n - feito;
        memcpy(dest + feito, src, resto);
        feito += resto;
    }
    return feito || !n ? (int)feito : ERRO_ACESSO;
}

static int c_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{
    cdda_t *v = priv;
    cdda_arq_t *f = arq;
    if (pos >= f->tam) return 0;
    if (n > f->tam - pos) n = f->tam - pos;
    uint8_t *dest = buf;
    if (f->indice == E_LISTA) { memcpy(dest, v->m3u + pos, n); return (int)n; }
    const cdda_pista_t *p = &v->toc->pista[f->indice];
    uint32_t feito = 0;
    if (pos < CDDA_CABECALHO) {
        uint8_t c[CDDA_CABECALHO];
        cdda_cabecalho(c, p->setores);
        feito = CDDA_CABECALHO - pos;
        if (feito > n) feito = n;
        memcpy(dest, c + pos, feito);
    }
    if (feito < n) {
        int r = ler_audio(v, p, pos + feito - CDDA_CABECALHO, dest + feito, n - feito);
        if (r < 0) return feito ? (int)feito : r;
        feito += (uint32_t)r;
    }
    return (int)feito;
}

static int c_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tam)
{ (void)priv; (void)arq; (void)pos; (void)buf; (void)n; (void)tam; return ERRO_SO_LEITURA; }
static int c_truncar(void *priv, void *arq, uint32_t tam) { (void)priv; (void)arq; (void)tam; return ERRO_SO_LEITURA; }
static void c_fechar(void *priv, void *arq) { (void)priv; (void)arq; }
static int c_so_leitura(void *priv, const char *rel) { (void)priv; (void)rel; return ERRO_SO_LEITURA; }
static int c_renomear(void *priv, const char *de, const char *para) { (void)priv; (void)de; (void)para; return ERRO_SO_LEITURA; }

static void c_espaco(void *priv, uint32_t *total_kb, uint32_t *livre_kb)
{
    cdda_t *v = priv;
    uint32_t setores = 0;
    for (int i = 0; i < v->toc->qtd; i++) setores += v->toc->pista[i].setores;
    *total_kb = setores / 1024 * CDDA_SETOR + setores % 1024 * CDDA_SETOR / 1024;
    *livre_kb = 0;
}

static void liberar(cdda_t *v)
{
    if (v->toc) kfree(v->toc);
    if (v->m3u) kfree(v->m3u);
    if (v->cache) kfree(v->cache);
    kfree(v);
}

static int c_desmontar(void *priv, bool gravar)
{
    (void)gravar;
    cdda_t *v = priv;
    klog("[CDDA] %s desmontado\n", v->d->nome);
    liberar(v);
    return 0;
}

static int c_montar(disco_t *d, uint32_t *opcoes, void **priv)
{
    *opcoes |= MONTAR_SO_LEITURA;
    *opcoes &= ~(uint32_t)MONTAR_LEITURA_ESCRITA;
    if (!d->toc || !d->toc->qtd || !d->ler_audio) return ERRO_FORMATO;
    cdda_t *v = kcalloc(sizeof *v);
    if (!v) return ERRO_SEM_MEMORIA;
    v->d = d;
    v->toc = kmalloc(sizeof *v->toc);
    v->m3u = kmalloc(MAX_M3U);
    v->cache = kmalloc(CACHE_SETORES * CDDA_SETOR);
    if (!v->toc || !v->m3u || !v->cache) { liberar(v); return ERRO_SEM_MEMORIA; }
    memcpy(v->toc, d->toc, sizeof *v->toc);
    v->m3u_tam = cdda_m3u(v->toc, v->m3u, MAX_M3U);
    uint32_t s = cdda_segundos(v->toc);
    klog("[CDDA] %s: %d pistas de audio, %u:%02u%s%s\n", d->nome, v->toc->qtd, s / 60, s % 60,
         v->toc->album[0] ? ", CD-TEXT: " : "", v->toc->album);
    *priv = v;
    return 0;
}

const sistema_arquivos_t fs_cdda = {
    .nome = "cdda", .tam_arq = sizeof(cdda_arq_t),
    .montar = c_montar, .desmontar = c_desmontar, .info = c_info, .datas = NULL, .listar = c_listar,
    .abrir = c_abrir, .ler = c_ler, .escrever = c_escrever, .truncar = c_truncar, .fechar = c_fechar,
    .sincronizar = NULL, .criar_pasta = c_so_leitura, .remover_pasta = c_so_leitura, .apagar = c_so_leitura,
    .renomear = c_renomear, .espaco = c_espaco,
};
