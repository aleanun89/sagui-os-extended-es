// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_gguf.c - lector de ficheros GGUF (versiones 2 y 3), D380.
 *
 * Formato (todo en little-endian):
 *   "GGUF", versión u32, n_tensores u64, n_kv u64,
 *   n_kv × (clave: texto, tipo u32, valor),
 *   n_tensores × (nombre: texto, n_dims u32, ne[n_dims] u64, tipo u32, desplazamiento u64),
 *   relleno hasta general.alignment (32 si no se dice), y los datos de los tensores.
 * Un texto es u64 de longitud y los bytes (sin cero final). Un vector es tipo u32, n u64
 * y los elementos.
 *
 * La cabecera se lee entera a memoria (en el LFM3 1.7B son 8 MB, casi todo el
 * vocabulario y las fusiones del tokenizador) y los textos se quedan dentro de ella:
 * no se copian. Los pesos se cargan después, con gguf_carregar, en un mapa de páginas. */
#include "llm.h"

#define ERRO(...) do { llm_formatar(g->erro, sizeof g->erro, __VA_ARGS__); return -1; } while (0)
#define CAB_MAX (64u << 20)   /* tope de la cabecera (metadatos y vocabulario) */

typedef struct { const uint8_t *p, *fim; bool ok; } leitor_t;

static uint64_t le_u(leitor_t *r, int n)
{
    if (!r->ok || r->fim - r->p < n) { r->ok = false; return 0; }
    uint64_t v = 0;
    for (int i = 0; i < n; i++) v |= (uint64_t)r->p[i] << (8 * i);
    r->p += n;
    return v;
}

static llm_texto_t le_texto(leitor_t *r)
{
    llm_texto_t t = { 0, 0 };
    uint64_t n = le_u(r, 8);
    if (!r->ok || (uint64_t)(r->fim - r->p) < n) { r->ok = false; return t; }
    t.s = (const char *)r->p; t.n = (uint32_t)n;
    r->p += n;
    return t;
}

static int tam_tipo(uint32_t tipo)
{
    switch (tipo) {
    case GGUF_UINT8: case GGUF_INT8: case GGUF_BOOL: return 1;
    case GGUF_UINT16: case GGUF_INT16: return 2;
    case GGUF_UINT32: case GGUF_INT32: case GGUF_FLOAT32: return 4;
    case GGUF_UINT64: case GGUF_INT64: case GGUF_FLOAT64: return 8;
    default: return 0;
    }
}

static void pula_valor(leitor_t *r, uint32_t tipo)
{
    if (tipo == GGUF_STRING) { le_texto(r); return; }
    int t = tam_tipo(tipo);
    if (!t) { r->ok = false; return; }
    if (r->fim - r->p < t) r->ok = false; else r->p += t;
}

/* Tamaño de bloque y bytes por bloque de cada tipo de tensor. */
static bool bloco_tipo(uint32_t tipo, int *elems, int *bytes)
{
    switch (tipo) {
    case T_F32:  *elems = 1;  *bytes = 4; return true;
    case T_F16:  *elems = 1;  *bytes = 2; return true;
    case T_Q8_0: *elems = 32; *bytes = 34; return true;
    case T_Q4_0: *elems = 32; *bytes = 18; return true;
    case T_Q4_K: *elems = 256; *bytes = 144; return true;
    case T_Q6_K: *elems = 256; *bytes = 210; return true;
    case T_Q8_K: *elems = 256; *bytes = 292; return true;
    default: return false;
    }
}

size_t gguf_tamanho_linha(uint32_t tipo, uint64_t n)
{
    int e, b;
    if (!bloco_tipo(tipo, &e, &b)) return 0;
    return (size_t)(n / (uint64_t)e) * (size_t)b;
}

const char *gguf_nome_tipo(uint32_t tipo)
{
    switch (tipo) {
    case T_F32: return "F32"; case T_F16: return "F16"; case T_Q8_0: return "Q8_0";
    case T_Q4_0: return "Q4_0"; case T_Q4_K: return "Q4_K"; case T_Q6_K: return "Q6_K"; case T_Q8_K: return "Q8_K";
    default: return "?";
    }
}

static bool texto_igual(llm_texto_t t, const char *s)
{
    size_t n = llm_strlen(s);
    return t.n == n && llm_memcmp(t.s, s, n) == 0;
}

int gguf_abrir(gguf_t *g, const char *caminho)
{
    llm_memset(g, 0, sizeof *g);
    g->fd = llm_abrir(caminho);
    if (g->fd < 0) ERRO("no se puede abrir %s", caminho);
    long long tam = llm_tamanho_fd(g->fd);
    uint8_t ini[24];
    if (tam < 24 || llm_ler_em(g->fd, 0, ini, 24)) ERRO("%s no es un fichero GGUF (demasiado corto)", caminho);
    if (ini[0] != 'G' || ini[1] != 'G' || ini[2] != 'U' || ini[3] != 'F') ERRO("%s no es un fichero GGUF", caminho);
    leitor_t r = { ini + 4, ini + 24, true };
    g->versao = (uint32_t)le_u(&r, 4);
    if (g->versao < 2 || g->versao > 3) ERRO("versión de GGUF no admitida: %u", g->versao);
    g->n_tensores = le_u(&r, 8);
    g->n_kv = le_u(&r, 8);
    /* Topes (revisión D380): un modelo de verdad tiene unos cientos de tensores y unas decenas de claves. */
    if (g->n_tensores > 16384 || g->n_kv > 4096) ERRO("cabecera GGUF no válida (%u tensores, %u claves)",
                                                       (unsigned)g->n_tensores, (unsigned)g->n_kv);

    /* La cabecera crece hasta que cabe: se empieza con 1 MB y se dobla, como mucho hasta CAB_MAX. */
    size_t n = 1u << 20;
    for (;;) {
        if ((long long)n > tam) n = (size_t)tam;
        llm_libera(g->cab);
        g->cab = llm_aloca(n);
        if (!g->cab) ERRO("no hay memoria para la cabecera (%u KB)", (unsigned)(n >> 10));
        if (llm_ler_em(g->fd, 0, g->cab, n)) ERRO("error al leer la cabecera");
        g->cab_n = n;
        llm_libera(g->kv); llm_libera(g->t);
        g->kv = llm_aloca_zero(sizeof(gguf_kv_t) * (size_t)(g->n_kv + 1));
        g->t = llm_aloca_zero(sizeof(gguf_tensor_t) * (size_t)(g->n_tensores + 1));
        if (!g->kv || !g->t) ERRO("no hay memoria para la cabecera");
        r.p = g->cab + 24; r.fim = g->cab + n; r.ok = true;
        g->alinhamento = 32;
        for (uint64_t i = 0; i < g->n_kv && r.ok; i++) {
            gguf_kv_t *kv = &g->kv[i];
            kv->nome = le_texto(&r);
            kv->tipo = (uint32_t)le_u(&r, 4);
            if (kv->tipo == GGUF_ARRAY) {
                kv->arr_tipo = (uint32_t)le_u(&r, 4);
                kv->arr_n = le_u(&r, 8);
                kv->dados = r.p;
                if (kv->arr_tipo == GGUF_ARRAY) { r.ok = false; break; }
                if (kv->arr_tipo == GGUF_STRING) {
                    for (uint64_t j = 0; j < kv->arr_n && r.ok; j++) le_texto(&r);
                } else {
                    int t = tam_tipo(kv->arr_tipo);
                    if (!t || (uint64_t)(r.fim - r.p) / (uint64_t)t < kv->arr_n) { r.ok = false; break; }
                    r.p += (size_t)kv->arr_n * (size_t)t;
                }
            } else {
                kv->dados = r.p;
                pula_valor(&r, kv->tipo);
            }
            if (r.ok && texto_igual(kv->nome, "general.alignment") && kv->tipo == GGUF_UINT32)
                g->alinhamento = kv->dados[0] | kv->dados[1] << 8 | kv->dados[2] << 16 | (uint32_t)kv->dados[3] << 24;
        }
        for (uint64_t i = 0; i < g->n_tensores && r.ok; i++) {
            gguf_tensor_t *t = &g->t[i];
            t->nome = le_texto(&r);
            t->ndim = (uint32_t)le_u(&r, 4);
            if (t->ndim < 1 || t->ndim > 4) { if (r.ok) ERRO("tensor con %u dimensiones", t->ndim); break; }
            for (uint32_t d = 0; d < 4; d++) t->ne[d] = d < t->ndim ? le_u(&r, 8) : 1;
            t->tipo = (uint32_t)le_u(&r, 4);
            t->desloc = le_u(&r, 8);
        }
        if (r.ok) break;
        if ((long long)n >= tam || n >= CAB_MAX) ERRO("cabecera GGUF cortada, no válida o de más de 64 MB");
        n *= 2;
        if (n > CAB_MAX) n = CAB_MAX;
    }
    if (!g->alinhamento || g->alinhamento > 4096 || (g->alinhamento & (g->alinhamento - 1)))
        ERRO("alineación no válida: %u", g->alinhamento);
    uint64_t pos = (uint64_t)(r.p - g->cab);
    g->dados_ini = (pos + g->alinhamento - 1) / g->alinhamento * g->alinhamento;
    if (g->dados_ini > (uint64_t)tam) ERRO("cabecera GGUF no válida (los datos empiezan fuera del fichero)");
    g->dados_n = (uint64_t)tam - g->dados_ini;

    for (uint64_t i = 0; i < g->n_tensores; i++) {
        gguf_tensor_t *t = &g->t[i];
        int nn = t->nome.n > 64 ? 64 : (int)t->nome.n;
        uint64_t elems = 1;
        for (int d = 0; d < 4; d++) {   /* cada dimensión de 1 a 2^31 y el producto sin dar la vuelta */
            if (t->ne[d] < 1 || t->ne[d] > (1ull << 31)) ERRO("tensor %.*s: dimensión no válida", nn, t->nome.s);
            if (elems > (1ull << 62) / t->ne[d]) ERRO("tensor %.*s: demasiado grande", nn, t->nome.s);
            elems *= t->ne[d];
        }
        int be, bb;
        if (!bloco_tipo(t->tipo, &be, &bb)) ERRO("tensor %.*s: tipo %u no admitido", nn, t->nome.s, t->tipo);
        if (t->ne[0] % (uint64_t)be) ERRO("tensor %.*s: filas no múltiplo del bloque", nn, t->nome.s);
        t->bytes = elems / (uint64_t)be * (uint64_t)bb;   /* elems < 2^62 y bb < 512: no da la vuelta */
        if (t->desloc % g->alinhamento || t->bytes > g->dados_n || t->desloc > g->dados_n - t->bytes)
            ERRO("tensor %.*s fuera del fichero", nn, t->nome.s);
    }
    return 0;
}

int gguf_carregar(gguf_t *g, void (*progresso)(unsigned pct))
{
    size_t n = (size_t)g->dados_n;
    if ((uint64_t)n != g->dados_n) ERRO("el modelo no cabe en la memoria del proceso");
    g->dados = llm_mapa(n);
    if (!g->dados) ERRO("no hay memoria para los pesos (%u MB)", (unsigned)(n >> 20));
    const size_t trozo = 8u << 20;
    unsigned ult = 101;
    for (size_t feito = 0; feito < n; feito += trozo) {
        size_t m = n - feito < trozo ? n - feito : trozo;
        if (llm_ler_em(g->fd, g->dados_ini + feito, g->dados + feito, m)) ERRO("error al leer los pesos");
        unsigned pct = (unsigned)((unsigned long long)(feito + m) * 100 / n);
        if (progresso && pct != ult) progresso(pct);
        ult = pct;
    }
    for (uint64_t i = 0; i < g->n_tensores; i++) g->t[i].dados = g->dados + g->t[i].desloc;
    return 0;
}

void gguf_fechar(gguf_t *g)
{
    if (g->dados) llm_mapa_libera(g->dados, (size_t)g->dados_n);
    llm_libera(g->cab); llm_libera(g->kv); llm_libera(g->t);
    if (g->fd >= 0) llm_fechar(g->fd);
    llm_memset(g, 0, sizeof *g);
    g->fd = -1;
}

const gguf_kv_t *gguf_kv(const gguf_t *g, const char *nome)
{
    for (uint64_t i = 0; i < g->n_kv; i++)
        if (texto_igual(g->kv[i].nome, nome)) return &g->kv[i];
    return NULL;
}

static uint64_t le_num(const uint8_t *p, uint32_t tipo, bool *ok)
{
    leitor_t r = { p, p + 8, true };
    *ok = true;
    switch (tipo) {
    case GGUF_UINT8: case GGUF_BOOL: return le_u(&r, 1);
    case GGUF_INT8: return (uint64_t)(int64_t)(int8_t)le_u(&r, 1);
    case GGUF_UINT16: return le_u(&r, 2);
    case GGUF_INT16: return (uint64_t)(int64_t)(int16_t)le_u(&r, 2);
    case GGUF_UINT32: return le_u(&r, 4);
    case GGUF_INT32: return (uint64_t)(int64_t)(int32_t)le_u(&r, 4);
    case GGUF_UINT64: case GGUF_INT64: return le_u(&r, 8);
    default: *ok = false; return 0;
    }
}

bool gguf_u32(const gguf_t *g, const char *nome, uint32_t *v)
{
    const gguf_kv_t *kv = gguf_kv(g, nome);
    bool ok;
    if (!kv || kv->tipo == GGUF_ARRAY) return false;
    uint64_t x = le_num(kv->dados, kv->tipo, &ok);
    if (ok) *v = (uint32_t)x;
    return ok;
}

bool gguf_f32(const gguf_t *g, const char *nome, float *v)
{
    const gguf_kv_t *kv = gguf_kv(g, nome);
    if (!kv) return false;
    if (kv->tipo == GGUF_FLOAT32) { llm_memcpy(v, kv->dados, 4); return true; }
    if (kv->tipo == GGUF_FLOAT64) { double d; llm_memcpy(&d, kv->dados, 8); *v = (float)d; return true; }
    uint32_t u;
    if (gguf_u32(g, nome, &u)) { *v = (float)u; return true; }
    return false;
}

bool gguf_texto(const gguf_t *g, const char *nome, llm_texto_t *v)
{
    const gguf_kv_t *kv = gguf_kv(g, nome);
    if (!kv || kv->tipo != GGUF_STRING) return false;
    leitor_t r = { kv->dados, g->cab + g->cab_n, true };
    *v = le_texto(&r);
    return r.ok;
}

bool gguf_arr_u32(const gguf_t *g, const char *nome, uint64_t i, uint32_t *v)
{
    const gguf_kv_t *kv = gguf_kv(g, nome);
    if (!kv || kv->tipo != GGUF_ARRAY || i >= kv->arr_n) return false;
    int t = tam_tipo(kv->arr_tipo);
    bool ok;
    if (!t || kv->arr_tipo == GGUF_FLOAT32 || kv->arr_tipo == GGUF_FLOAT64) return false;
    uint64_t x = le_num(kv->dados + i * (uint64_t)t, kv->arr_tipo, &ok);
    if (ok) *v = (uint32_t)x;
    return ok;
}

/* El siguiente texto de un vector, sin pasar de «fim» (el final real de la cabecera). NULL si no cabe. */
const uint8_t *gguf_arr_texto(const uint8_t *p, const uint8_t *fim, llm_texto_t *t)
{
    leitor_t r = { p, fim, true };
    *t = le_texto(&r);
    return r.ok ? r.p : NULL;
}

gguf_tensor_t *gguf_tensor(const gguf_t *g, const char *nome)
{
    for (uint64_t i = 0; i < g->n_tensores; i++)
        if (texto_igual(g->t[i].nome, nome)) return &g->t[i];
    return NULL;
}
