// SPDX-License-Identifier: GPL-3.0-or-later
/* llm_principal.c - el programa `llm` (D380, mejora 139): modelos de lenguaje locales.
 *
 *   llm [-m modelo.gguf] [--contexto 12k] [--hilos N] [-p "texto"] [-n tokens]
 *   llm --info [-m modelo.gguf] [--contexto 24k]
 *   llm                      chat en el terminal (/nuevo, /salir)
 *   llm --servidor           API compatible con la de OpenAI en 127.0.0.1 (llm_servidor.c)
 *   llm --anunciar           si la IA local puede funcionar, lo dice en el escritorio
 *
 * Sin -m, el primer .gguf de /apps/models (o de LLM_MODELOS). El terminal de SaguiOS es
 * Latin-1: lo que se escribe pasa a UTF-8 antes de tokenizar y lo que sale del modelo,
 * a Latin-1 (lo que no cabe, a un parecido o «?»). */
#include "llm.h"
#include "llm_programa.h"

#ifdef LLM_ANFITRIAO
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#include "sagui.h"
#include "contas.h"   /* meu_uid */
#endif

#define RAM_MINIMA (18ull * 1024 * 1024 * 1024 / 10)    /* 1,8 GB: por debajo, la IA local no se activa */
#define CTX_MIN_NORMAL (12 * 1024)    /* con SSSE3 o mejor: 12k-48k */
#define CTX_MIN_ANTIGO (4 * 1024)     /* sin SSSE3 (Core Duo, Pentium 4, Athlon 64): 4k-48k (D380) */
#define CTX_MAX (48 * 1024)
#define PASTA_MODELOS "/home/compartido/modelos"

static const char AYUDA[] =
"Uso: llm [opciones]                 modelos de lenguaje en este equipo (IA local)\n"
"  -m FICHERO.gguf      el modelo (sin -m: el activo o, si solo hay uno, ese)\n"
"  --modelos            lista los modelos de " PASTA_MODELOS " y ~/modelos\n"
"  --activar FICHERO    elige el modelo que se usa sin -m\n"
"  --contexto N         tokens de contexto, p. ej. 24k o 32768: de 12k (de serie) a 48k;\n"
"                       en CPU sin SSSE3, de 4k (de serie) a 48k (se recomienda 4k-8k).\n"
"                       Menos solo si se pide así (para pruebas; mínimo 256).\n"
"  -p TEXTO             una sola pregunta (sin -p: chat; /nuevo empieza otra, /salir sale)\n"
"  -n N                 como mucho N tokens de respuesta (512 de serie)\n"
"  --sistema TEXTO      instrucciones de sistema para el chat\n"
"  --pensar             deja que el modelo razone antes de contestar (más lento)\n"
"  --crudo              sin plantilla de chat: continúa el texto de -p tal cual\n"
"  --temp T  --top-k K  --top-p P  --repeticion R  --semilla S\n"
"                       muestreo (temperatura 0 = siempre el más probable)\n"
"  --hilos N            hilos para las multiplicaciones (de serie, uno por CPU; con la imagen SMP)\n"
"  --info               arquitectura, cuantización y memoria que necesita ese contexto\n"
"  --servidor [--puerto P]  API compatible con OpenAI en 127.0.0.1:P (8080 de serie):\n"
"                       ia, chatia y los agentes la usan con el proveedor «llama.cpp»\n"
"  --anunciar           muestra el logo «IA Powered» si la IA local puede funcionar\n"
"  --ruta c|sse2|ssse3  fuerza la ruta de cálculo (de serie, la mejor que tenga la CPU)\n"
"La IA local necesita más de 1,8 GB de memoria y que el modelo y la caché quepan en el\n"
"proceso (llm --info lo calcula). Los modelos nunca van dentro del sistema: se bajan en\n"
"Configuración > IA (o se copian) a " PASTA_MODELOS " o a ~/modelos.\n";

/* ------------------------------------------------------------ utilidades */

static bool igual(const char *a, const char *b) { return llm_strcmp(a, b) == 0; }

static long numero(const char *s, bool *ok)
{
    long v = 0; int d = 0;
    *ok = false;
    if (!s) return 0;
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s++ - '0'); d++; if (v > 100000000) return 0; }
    if (!d) return 0;
    if (*s == 'k' || *s == 'K') { if (v > (1L << 20)) return 0; v *= 1024; s++; }   /* sin desbordar el long */
    *ok = *s == 0;
    return v;
}

static float real(const char *s, bool *ok)
{
    float v = 0, f = 0.1f; bool ponto = false; int d = 0;
    *ok = false;
    if (!s) return 0;
    while (*s) {
        if (*s >= '0' && *s <= '9') { if (ponto) { v += f * (*s - '0'); f *= 0.1f; } else v = v * 10 + (*s - '0'); d++; }
        else if ((*s == '.' || *s == ',') && !ponto) ponto = true;
        else return 0;
        s++;
    }
    *ok = d > 0;
    return v;
}

void llm_mb(char *b, size_t n, unsigned long long bytes)
{
    unsigned long long mb10 = (bytes * 10 + (1 << 19)) >> 20;
    if (mb10 >= 10240) llm_formatar(b, n, "%u,%u GB", (unsigned)(mb10 / 10240), (unsigned)(mb10 % 10240 * 10 / 10240));
    else llm_formatar(b, n, "%u MB", (unsigned)((mb10 + 5) / 10));
}

/* ---- salida: UTF-8 del modelo -> Latin-1 del terminal (en el PC, tal cual) ---- */
typedef struct { char b[4]; int n; } utf8_pend_t;

static void sair_texto(llm_saida_t *s, const char *t, int n)
{
    if (s->f) { s->f(s->ctx, t, n); return; }
    llm_escrever(1, t, (size_t)n);
}

static void emitir_cp(llm_saida_t *s, uint32_t c)
{
    /* Al terminal no llegan secuencias de control del modelo (ESC, C1...): solo \n y \t (revisión D380). */
    if (!s->utf8 && ((c < 0x20 && c != '\n' && c != '\t') || (c >= 0x7F && c < 0xA0))) return;
#ifdef LLM_ANFITRIAO
    char o[4]; int k;
    if (c < 0x80) { o[0] = (char)c; k = 1; }
    else if (c < 0x800) { o[0] = (char)(0xC0 | c >> 6); o[1] = (char)(0x80 | (c & 0x3F)); k = 2; }
    else if (c < 0x10000) { o[0] = (char)(0xE0 | c >> 12); o[1] = (char)(0x80 | ((c >> 6) & 0x3F)); o[2] = (char)(0x80 | (c & 0x3F)); k = 3; }
    else { o[0] = (char)(0xF0 | c >> 18); o[1] = (char)(0x80 | ((c >> 12) & 0x3F)); o[2] = (char)(0x80 | ((c >> 6) & 0x3F)); o[3] = (char)(0x80 | (c & 0x3F)); k = 4; }
    sair_texto(s, o, k);
#else
    if (s->utf8) {   /* el servidor manda UTF-8 */
        char o[4]; int k;
        if (c < 0x80) { o[0] = (char)c; k = 1; }
        else if (c < 0x800) { o[0] = (char)(0xC0 | c >> 6); o[1] = (char)(0x80 | (c & 0x3F)); k = 2; }
        else if (c < 0x10000) { o[0] = (char)(0xE0 | c >> 12); o[1] = (char)(0x80 | ((c >> 6) & 0x3F)); o[2] = (char)(0x80 | (c & 0x3F)); k = 3; }
        else { o[0] = (char)(0xF0 | c >> 18); o[1] = (char)(0x80 | ((c >> 12) & 0x3F)); o[2] = (char)(0x80 | ((c >> 6) & 0x3F)); o[3] = (char)(0x80 | (c & 0x3F)); k = 4; }
        sair_texto(s, o, k);
        return;
    }
    const char *r = NULL;
    char um;
    if (c < 0x100) { um = (char)c; sair_texto(s, &um, 1); return; }
    switch (c) {
    case 0x2018: case 0x2019: case 0x201B: r = "'"; break;
    case 0x201C: case 0x201D: case 0x201E: r = "\""; break;
    case 0x2013: case 0x2014: case 0x2212: r = "-"; break;
    case 0x2026: r = "..."; break;
    case 0x2022: case 0x2023: case 0x25CF: r = "\xB7"; break;
    case 0x20AC: r = "EUR"; break;
    case 0x2192: r = "->"; break;
    case 0x2190: r = "<-"; break;
    case 0x2264: r = "<="; break;
    case 0x2265: r = ">="; break;
    case 0x2260: r = "!="; break;
    case 0x200B: case 0xFE0F: r = ""; break;
    default: r = "?";
    }
    sair_texto(s, r, (int)llm_strlen(r));
#endif
}

void llm_saida_bytes(llm_saida_t *s, const char *b, int n)
{
    for (int i = 0; i < n; i++) {
        uint8_t c = (uint8_t)b[i];
        if (s->pend == 0) {
            if (c < 0x80) { emitir_cp(s, c); continue; }
            s->falta = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : 0;
            if (!s->falta) { emitir_cp(s, '?'); continue; }
            s->cp = c & (0x3F >> s->falta);
            s->pend = 1;
            continue;
        }
        if ((c & 0xC0) != 0x80) { s->pend = 0; emitir_cp(s, '?'); i--; continue; }
        s->cp = s->cp << 6 | (c & 0x3F);
        if (--s->falta == 0) { s->pend = 0; emitir_cp(s, s->cp); }
    }
}

/* Latin-1 del terminal -> UTF-8 (en el PC ya viene en UTF-8). -> bytes */
int llm_latin1_utf8(const char *s, char *o, int max)
{
    int n = 0;
#ifdef LLM_ANFITRIAO
    while (*s && n < max - 1) o[n++] = *s++;
#else
    for (; *s && n < max - 2; s++) {
        uint8_t c = (uint8_t)*s;
        if (c < 0x80) o[n++] = (char)c;
        else { o[n++] = (char)(0xC0 | c >> 6); o[n++] = (char)(0x80 | (c & 0x3F)); }
    }
#endif
    o[n] = 0;
    return n;
}

/* ------------------------------------------------------------ el modelo por defecto */

static bool acabar_em(const char *s, const char *fim)
{
    size_t a = llm_strlen(s), b = llm_strlen(fim);
    if (a < b) return false;
    for (size_t i = 0; i < b; i++) {
        char x = s[a - b + i], y = fim[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (x != y) return false;
    }
    return true;
}

/* Los modelos nunca van dentro del sistema (D380): se buscan en /home/compartido/modelos (el
 * descargador de Configuración guarda ahí) y en ~/modelos; LLM_MODELOS añade otra carpeta. El
 * modelo activo se apunta en ~/.config/llm/modelo (llm --activar). */
static const char *casa(void)
{
#ifdef LLM_ANFITRIAO
    const char *h = getenv("HOME");
#else
    const char *h = obter_env("HOME");
#endif
    return h && *h ? h : "/home";
}

static int pastas_modelos(char p[3][200])
{
    int n = 0;
#ifdef LLM_ANFITRIAO
    const char *e = getenv("LLM_MODELOS");
#else
    const char *e = obter_env("LLM_MODELOS");
#endif
    if (e && *e) llm_formatar(p[n++], 200, "%s", e);
    llm_formatar(p[n++], 200, "%s", PASTA_MODELOS);
    llm_formatar(p[n], 200, "%s/modelos", casa());
    if (llm_strcmp(p[n], PASTA_MODELOS)) n++;
    return n;
}

static bool eh_modelo(const char *nome) { return acabar_em(nome, ".gguf") && llm_strncmp(nome, "mmproj", 6); }

/* Llama a f con cada modelo encontrado. -> cuántos */
static int percorrer_modelos(void (*f)(void *ctx, const char *caminho, unsigned long long tam), void *ctx)
{
    char p[3][200], c[460];
    int np = pastas_modelos(p), n = 0;
    for (int k = 0; k < np; k++) {
#ifdef LLM_ANFITRIAO
        DIR *d = opendir(p[k]);
        if (!d) continue;
        struct dirent *de;
        while ((de = readdir(d))) {
            if (!eh_modelo(de->d_name)) continue;
            llm_formatar(c, sizeof c, "%s/%s", p[k], de->d_name);
            int fd = llm_abrir(c);
            unsigned long long t = fd >= 0 ? (unsigned long long)llm_tamanho_fd(fd) : 0;
            if (fd >= 0) llm_fechar(fd);
            if (f) f(ctx, c, t);
            n++;
        }
        closedir(d);
#else
        entrada_pasta_t ent;
        for (int i = 0; listar(p[k], i, &ent) > 0; i++) {
            if (!eh_modelo(ent.nome)) continue;
            llm_formatar(c, sizeof c, "%s/%s", p[k], ent.nome);
            if (f) f(ctx, c, ent.tamanho);
            n++;
        }
#endif
    }
    return n;
}

static void caminho_ativo(char *c, size_t max) { llm_formatar(c, max, "%s/.config/llm/modelo", casa()); }

/* El modelo activo apuntado, si aún existe. */
static bool ler_ativo(char *dst, size_t max)
{
    char c[256];
    caminho_ativo(c, sizeof c);
    int fd = llm_abrir(c);
    if (fd < 0) return false;
    long long t = llm_tamanho_fd(fd);
    bool ok = t > 0 && (size_t)t < max && !llm_ler_em(fd, 0, dst, (size_t)t);
    llm_fechar(fd);
    if (!ok) return false;
    dst[t] = 0;
    for (long long i = 0; i < t; i++) if (dst[i] == '\n' || dst[i] == '\r') { dst[i] = 0; break; }
    int f = llm_abrir(dst);
    if (f < 0) return false;
    llm_fechar(f);
    return true;
}

/* Para escribir nombres de fichero en el terminal: sin caracteres de control. */
static void limpar(char *d, size_t max, const char *s)
{
    size_t k = 0;
    for (; *s && k + 1 < max; s++) {
        unsigned char c = (unsigned char)*s;
        d[k++] = (c < 0x20 || (c >= 0x7F && c < 0xA0)) ? '?' : (char)c;
    }
    d[k] = 0;
}

/* ¿Es de esta cuenta o de root? /home/compartido es común: un modelo de otra cuenta no se elige solo
 * (podría haberlo puesto ahí para que otro lo cargue). Con -m o --activar, el usuario decide. */
static bool dono_ok(const char *c)
{
#ifdef LLM_ANFITRIAO
    struct stat st;
    return stat(c, &st) == 0 && (st.st_uid == getuid() || st.st_uid == 0);
#else
    info_permisos_t p;
    if (permisos_arquivo(c, &p) != 0) return false;
    if (!p.com_donos) return true;   /* FAT y demás: el dueño es el del montaje */
    return p.uid == meu_uid() || p.uid == 0;
#endif
}

typedef struct { char *dst; size_t max; int n; } primeiro_t;
static void guardar_primeiro(void *ctx, const char *c, unsigned long long t)
{
    primeiro_t *p = ctx;
    (void)t;
    if (!dono_ok(c)) {
        char l[300];
        limpar(l, sizeof l, c);
        llm_eprintf("llm: se ignora %s: es de otra cuenta (úselo con -m si se fía de él)\n", l);
        return;
    }
    if (!p->n++) llm_formatar(p->dst, p->max, "%s", c);
}

/* Sin -m: el activo o, si solo hay uno (de esta cuenta o de root), ese. -> 1 ok, 0 ninguno, -1 varios sin activo */
int llm_modelo_padrao(char *dst, size_t max)
{
    if (ler_ativo(dst, max)) return 1;
    dst[0] = 0;
    primeiro_t p = { dst, max, 0 };
    percorrer_modelos(guardar_primeiro, &p);
    return p.n == 1 ? 1 : p.n == 0 ? 0 : -1;
}

static void listar_um(void *ctx, const char *c, unsigned long long t)
{
    const char *ativo = ctx;
    char b[32], l[460];
    llm_mb(b, sizeof b, t);
    limpar(l, sizeof l, c);
    llm_printf("%c %-60s %s\n", llm_strcmp(c, ativo) ? ' ' : '*', l, b);
}

static int modo_modelos(void)
{
    char ativo[256] = "", p[3][200];
    if (!ler_ativo(ativo, sizeof ativo)) ativo[0] = 0;
    int np = pastas_modelos(p);
    llm_printf("Modelos (en");
    for (int k = 0; k < np; k++) llm_printf(" %s", p[k]);
    llm_printf("; * = el activo):\n");
    if (!percorrer_modelos(listar_um, ativo)) llm_printf("  ninguno. Se bajan en Configuración > IA o se copian a %s.\n", PASTA_MODELOS);
    return 0;
}

static int modo_ativar(const char *modelo)
{
    int fd = llm_abrir(modelo);
    if (fd < 0) { llm_eprintf("llm: no existe %s\n", modelo); return 1; }
    llm_fechar(fd);
    char c[256], d[256];
    llm_formatar(d, sizeof d, "%s/.config", casa());
#ifdef LLM_ANFITRIAO
    mkdir(d, 0755);
    llm_formatar(d, sizeof d, "%s/.config/llm", casa());
    mkdir(d, 0755);
    caminho_ativo(c, sizeof c);
    FILE *f = fopen(c, "w");
    if (!f) { llm_eprintf("llm: no se puede escribir %s\n", c); return 1; }
    fprintf(f, "%s\n", modelo);
    fclose(f);
#else
    criar_pasta(d);
    llm_formatar(d, sizeof d, "%s/.config/llm", casa());
    criar_pasta(d);
    caminho_ativo(c, sizeof c);
    int w = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (w < 0) { llm_eprintf("llm: no se puede escribir %s\n", c); return 1; }
    escrever_texto(w, modelo);
    escrever_texto(w, "\n");
    fechar(w);
#endif
    llm_printf("Modelo activo: %s\n", modelo);
    return 0;
}

/* ------------------------------------------------------------ comprobaciones */

/* La regla de la memoria (D380): más de 1,8 GB de RAM en el equipo. */
bool llm_ram_suficiente(char *msg, size_t max)
{
#ifdef LLM_ANFITRIAO
    (void)msg; (void)max;
    return true;
#else
    unsigned long long ram = llm_ram_fisica();
    if (ram > RAM_MINIMA) return true;
    char b[32];
    llm_mb(b, sizeof b, ram);
    llm_formatar(msg, max, "La IA local necesita más de 1,8 GB de memoria; este equipo tiene %s.", b);
    return false;
#endif
}

/* ¿Cabe el modelo con ese contexto? Rellena las cuentas para --info. */
bool llm_memoria_cabe(const llm_modelo_t *m, int ctx, llm_contas_t *c, char *msg, size_t max)
{
    c->pesos = llm_memoria_pesos(m);
    c->cache = llm_memoria_cache(m, ctx);
    c->trabalho = llm_memoria_trabalho(m, ctx);
    c->tokens = 24ull << 20;   /* vocabulario y tablas de fusiones (el LFM3: unos 20 MB) */
    c->total = c->pesos + c->cache + c->trabalho + c->tokens;
    c->espaco = llm_espaco_processo();
    c->livre = llm_memoria_livre();
    char t[32], e[32];
    llm_mb(t, sizeof t, c->total);
    if (c->total > c->espaco) {
        llm_mb(e, sizeof e, c->espaco);
        llm_formatar(msg, max, "El modelo con %d tokens de contexto necesita %s y el proceso solo puede tener %s. "
                     "Pruebe con menos contexto o con un modelo más pequeño.", ctx, t, e);
        return false;
    }
    if (c->livre && c->total > c->livre) {
        llm_mb(e, sizeof e, c->livre);
        llm_formatar(msg, max, "El modelo con %d tokens de contexto necesita %s de memoria y solo quedan %s libres. "
                     "Cierre programas o pruebe con menos contexto.", ctx, t, e);
        return false;
    }
    return true;
}

/* ------------------------------------------------------------ carga */

static void progresso(unsigned pct)
{
    static unsigned ult = 1000;
    if (pct / 10 == ult / 10 && pct != 100) return;
    ult = pct;
    llm_eprintf("\rCargando el modelo: %u%%", pct);
    if (pct == 100) llm_eprintf("\n");
}

int llm_preparar(llm_sessao_t *s, const char *caminho, int ctx, bool silencio, char *erro, size_t max)
{
    if (llm_modelo_abrir(&s->m, caminho, erro, max)) { llm_modelo_fechar(&s->m); return -1; }
    if (ctx > s->m.ctx_treino) ctx = s->m.ctx_treino;
    llm_contas_t c;
    if (!llm_memoria_cabe(&s->m, ctx, &c, erro, max)) { llm_modelo_fechar(&s->m); return -1; }
    if (llm_tokens_criar(&s->t, &s->m.g, erro, max)) { llm_modelo_fechar(&s->m); return -1; }
    /* El embedding (y la salida) no puede tener más filas que el vocabulario: un token sin texto
     * leería fuera de las tablas del tokenizador (revisión D380). */
    if (s->m.n_vocab < 1 || s->m.n_vocab > s->t.n) {
        llm_formatar(erro, max, "el modelo tiene %d filas de embedding y el vocabulario %d tokens", s->m.n_vocab, s->t.n);
        llm_tokens_libera(&s->t); llm_modelo_fechar(&s->m);
        return -1;
    }
    unsigned t0 = llm_ms();
    if (llm_modelo_carregar(&s->m, silencio ? NULL : progresso)) {
        llm_formatar(erro, max, "%s", s->m.g.erro);
        llm_tokens_libera(&s->t); llm_modelo_fechar(&s->m);
        return -1;
    }
    s->ms_carga = llm_ms() - t0;
    if (llm_estado_criar(&s->e, &s->m, ctx)) {
        llm_formatar(erro, max, "no hay memoria para la caché de %d tokens", ctx);
        llm_tokens_libera(&s->t); llm_modelo_fechar(&s->m);
        return -1;
    }
    llm_amostra_iniciar(&s->a, llm_aleatorio());
    float f; uint32_t u;
    if (gguf_f32(&s->m.g, "general.sampling.temp", &f)) s->a.temperatura = f;
    if (gguf_u32(&s->m.g, "general.sampling.top_k", &u)) s->a.top_k = (int)u;
    if (gguf_f32(&s->m.g, "general.sampling.top_p", &f)) s->a.top_p = f;
    if (gguf_f32(&s->m.g, "general.sampling.penalty_repeat", &f)) s->a.penalizacao = f;
    s->hist_n = 0;
    s->hist_max = ctx;
    s->hist = llm_aloca((size_t)ctx * sizeof(int));
    if (!s->hist) { llm_formatar(erro, max, "no hay memoria"); llm_fechar_sessao(s); return -1; }
    return 0;
}

void llm_fechar_sessao(llm_sessao_t *s)
{
    llm_estado_libera(&s->e);
    llm_tokens_libera(&s->t);
    llm_modelo_fechar(&s->m);
    llm_libera(s->hist);
    s->hist = NULL;
}

/* Pasa tokens por la red y los apunta en el historial. 0 = ok; -1 = contexto lleno. */
int llm_alimentar(llm_sessao_t *s, const int *toks, int n)
{
    if (s->e.kv.pos + n > s->e.kv.ctx) return -1;
    unsigned t0 = llm_ms();
    if (llm_avaliar(&s->e, toks, n, NULL)) return -1;
    s->ms_leitura += llm_ms() - t0;
    s->n_leitura += n;
    for (int i = 0; i < n && s->hist_n < s->hist_max; i++) s->hist[s->hist_n++] = toks[i];
    return 0;
}

/* Tokeniza texto UTF-8 (con especiales si se pide) y lo pasa por la red. */
int llm_alimentar_texto(llm_sessao_t *s, const char *utf8, bool especiais)
{
    size_t n = llm_strlen(utf8);
    int max = (int)n + 8;
    int *t = llm_aloca((size_t)max * sizeof(int));
    if (!t) return -2;
    int k = llm_tokenizar(&s->t, utf8, n, especiais, t, max);
    int r = llm_alimentar(s, t, k);
    llm_libera(t);
    return r;
}

/* Genera hasta n tokens, sacando el texto por «saida». Para en <|im_end|>, en el fin de
 * texto o si se llena el contexto. -> tokens generados */
int llm_gerar(llm_sessao_t *s, int n, llm_saida_t *saida, bool *cheio)
{
    char b[256];
    int g = 0;
    bool pensando = false;
    *cheio = false;
    for (; g < n; g++) {
        int id = llm_amostrar(&s->a, s->e.logits, s->m.n_vocab, s->hist, s->hist_n);
        if (id == s->t.eos || id == s->t.im_end || (s->t.tipo[id] == 3 && id != s->t.think)) {
            s->ultimo_fim = id;
            break;
        }
        if (id == s->t.think) pensando = true;
        if (saida && saida->pensamento_oculto && pensando) { if (id == s->t.think_fim) pensando = false; }
        else if (saida && !(saida->ocultar_think && (id == s->t.think || id == s->t.think_fim))) {
            int k = llm_token_bytes(&s->t, id, b, sizeof b);
            llm_saida_bytes(saida, b, k);
        }
        if (id == s->t.think_fim) pensando = false;
        unsigned t0 = llm_ms();
        if (s->e.kv.pos + 1 > s->e.kv.ctx) { *cheio = true; g++; break; }
        if (llm_avaliar(&s->e, &id, 1, NULL)) { *cheio = true; g++; break; }
        s->ms_geracao += llm_ms() - t0;
        s->n_geracao++;
        if (s->hist_n < s->hist_max) s->hist[s->hist_n++] = id;
        s->ultimo_fim = -1;
    }
    return g;
}

static void estatisticas(const llm_sessao_t *s)
{
    unsigned tl = s->ms_leitura ? (unsigned)((unsigned long long)s->n_leitura * 10000 / s->ms_leitura) : 0;
    unsigned tg = s->ms_geracao ? (unsigned)((unsigned long long)s->n_geracao * 10000 / s->ms_geracao) : 0;
    llm_eprintf("[llm] lectura: %d tokens en %u ms (%u,%u tok/s); generación: %d tokens en %u ms (%u,%u tok/s); "
                "contexto %d/%d; ruta %s\n",
                s->n_leitura, s->ms_leitura, tl / 10, tl % 10, s->n_geracao, s->ms_geracao, tg / 10, tg % 10,
                s->e.kv.pos, s->e.kv.ctx, llm_quant_rota());
}

/* ------------------------------------------------------------ plantilla de chat (LFM3) */

/* El principio de una conversación: <|startoftext|> y el sistema, si hay. */
static void inicio_chat(llm_sessao_t *s, const char *sistema, char *buf, int max)
{
    int n = llm_formatar(buf, (size_t)max, "<|startoftext|>");
    if (sistema && *sistema) {
        char u[4096];
        llm_latin1_utf8(sistema, u, sizeof u);
        llm_formatar(buf + n, (size_t)(max - n), "<|im_start|>system\n%s<|im_end|>\n", u);
    }
    (void)s;
}

int llm_turno_usuario(llm_sessao_t *s, const char *texto_utf8, bool pensar)
{
    int r = llm_alimentar_texto(s, "<|im_start|>user\n", true);
    if (!r) r = llm_alimentar_texto(s, texto_utf8, false);
    if (!r) r = llm_alimentar_texto(s, pensar ? "<|im_end|>\n<|im_start|>assistant\n<think>"
                                              : "<|im_end|>\n<|im_start|>assistant\n<think></think>", true);
    return r;
}

int llm_fim_turno(llm_sessao_t *s)
{
    return llm_alimentar_texto(s, "<|im_end|>\n", true);
}

/* ------------------------------------------------------------ modos */

/* ¿La CPU tiene SSSE3? LLM_SIN_SSSE3=1 hace como si no (para las pruebas). */
static bool tem_ssse3(void)
{
#ifdef LLM_ANFITRIAO
    const char *e = getenv("LLM_SIN_SSSE3");
#else
    const char *e = obter_env("LLM_SIN_SSSE3");
#endif
    if (e && *e && *e != '0') return false;
    return llm_cpu_ssse3();
}

static int ctx_minimo(void) { return tem_ssse3() ? CTX_MIN_NORMAL : CTX_MIN_ANTIGO; }

static int modo_info(const char *caminho, int ctx)
{
    llm_modelo_t m;
    char erro[256];
    if (llm_modelo_abrir(&m, caminho, erro, sizeof erro)) { llm_eprintf("llm: %s\n", erro); llm_modelo_fechar(&m); return 1; }
    if (ctx > m.ctx_treino) ctx = m.ctx_treino;
    int cont[16] = { 0 };
    unsigned long long bytes[16] = { 0 };
    for (uint64_t i = 0; i < m.g.n_tensores; i++) {
        uint32_t t = m.g.t[i].tipo;
        if (t < 16) { cont[t]++; bytes[t] += m.g.t[i].bytes; }
    }
    char b1[32], b2[32], b3[32], b4[32];
    llm_printf("Modelo:         %s\n", m.nome[0] ? m.nome : caminho);
    llm_printf("Fichero:        %s (GGUF v%u, %u tensores)\n", caminho, m.g.versao, (unsigned)m.g.n_tensores);
    llm_printf("Arquitectura:   %s, %d bloques (%d de convolución corta, %d de atención: %d completas y %d con ventana de %d)\n",
               m.arquitetura, m.n_camadas, m.n_conv, m.n_atencao, m.n_completas, m.n_janelas, m.n_janela);
    llm_printf("Dimensiones:    embedding %d, FFN %d, %d cabezas de %d, vocabulario %d\n",
               m.n_embd, m.n_ff, m.n_cab, m.dim_cab, m.n_vocab);
    llm_printf("Contexto:       %d tokens (entrenado hasta %d; este equipo: de %dk a 48k)\n", ctx, m.ctx_treino,
               ctx_minimo() / 1024);
    if (!tem_ssse3())
        llm_printf("Procesador:     no tiene SSSE3: se recomienda un contexto de 4k-8k y modelos Q4_0\n");
    llm_printf("Cuantización:  ");
    for (int t = 0; t < 16; t++) if (cont[t]) { llm_mb(b1, sizeof b1, bytes[t]); llm_printf(" %s x%d (%s)", gguf_nome_tipo((uint32_t)t), cont[t], b1); }
    llm_printf("\n");
    llm_contas_t c;
    char msg[256];
    bool cabe = llm_memoria_cabe(&m, ctx, &c, msg, sizeof msg);
    llm_mb(b1, sizeof b1, c.pesos); llm_mb(b2, sizeof b2, c.cache); llm_mb(b3, sizeof b3, c.trabalho + c.tokens);
    llm_mb(b4, sizeof b4, c.total);
    llm_printf("Memoria:        pesos %s + caché K/V Q8_0 %s + trabajo %s = %s\n", b1, b2, b3, b4);
    for (int k = 12; k <= 48; k += 12) {
        int cx = k * 1024 > m.ctx_treino ? m.ctx_treino : k * 1024;
        llm_mb(b1, sizeof b1, llm_memoria_cache(&m, cx));
        llm_mb(b2, sizeof b2, c.pesos + llm_memoria_cache(&m, cx) + llm_memoria_trabalho(&m, cx) + c.tokens);
        llm_printf("                con %2dk: caché %s, total %s\n", k, b1, b2);
    }
    llm_mb(b1, sizeof b1, c.espaco); llm_mb(b2, sizeof b2, c.livre);
    llm_printf("Este equipo:    el proceso puede tener %s; memoria libre %s", b1, b2);
    unsigned long long ram = llm_ram_fisica();
    if (ram) { llm_mb(b3, sizeof b3, ram); llm_printf("; RAM %s", b3); }
    llm_printf("\n");
    /* Estimación (D380, perfil de equipos antiguos): ~0,3 s de productos Q4_K con la ruta elegida;
     * cada token pasa por todos los pesos, así que tok/s ~ elementos por segundo / parámetros. */
    {
        unsigned long long params = 0;
        for (uint64_t i = 0; i < m.g.n_tensores; i++)
            if (m.g.t[i].ne[1] > 1) params += m.g.t[i].ne[0] * m.g.t[i].ne[1];
        static bloco_q4_K w[8 * 16];
        static bloco_q8_K x[8];
        float xf[8 * QK_K];
        for (int i = 0; i < 8 * QK_K; i++) xf[i] = (float)((i * 37) % 17) - 8.0f;
        llm_quant_q8_K(xf, x, 8 * QK_K);
        for (unsigned i = 0; i < sizeof w; i++) ((uint8_t *)w)[i] = (uint8_t)(i * 151 + 7);
        unsigned t0 = llm_ms(), feitos = 0;
        volatile float acum = 0;
        while (llm_ms() - t0 < 300) {
            for (int r = 0; r < 16; r++) acum += llm_dot_q4_K(w + r * 8, x, 8 * QK_K);
            feitos += 16;
        }
        unsigned ms = llm_ms() - t0;
        unsigned long long el_s = ms ? (unsigned long long)feitos * 8 * QK_K * 1000 / ms : 0;
        unsigned long long tok10 = params ? el_s * 10 / params : 0;   /* décimas de tok/s, un hilo */
        llm_printf("Estimación:     unos %u,%u tok/s con un hilo (%d CPU)", (unsigned)(tok10 / 10), (unsigned)(tok10 % 10), llm_cpus());
        if (tok10) {
            unsigned long long seg = (unsigned long long)ctx * 10 / tok10;
            llm_printf("; leer %d tokens: unos %u min", ctx, (unsigned)((seg + 59) / 60));
            if (seg > 300) llm_printf(" (lento: mejor menos contexto%s)", tem_ssse3() ? "" : " o un modelo Q4_0");
        }
        llm_printf("\n");
    }
    char rm[160];
    if (!llm_ram_suficiente(rm, sizeof rm)) llm_printf("Estado:         %s\n", rm);
    else if (!cabe) llm_printf("Estado:         %s\n", msg);
    else llm_printf("Estado:         cabe; ruta de cálculo %s\n", llm_quant_rota());
    llm_modelo_fechar(&m);
    return 0;
}

/* Lista de ids "1,2,3" */
static int ler_ids(const char *s, int *v, int max)
{
    int n = 0;
    while (*s && n < max) {
        int x = 0, d = 0;
        while (*s >= '0' && *s <= '9') { x = x * 10 + (*s++ - '0'); d++; }
        if (d) v[n++] = x;
        if (*s) s++;
    }
    return n;
}

/* Comparación con llama.cpp: evalúa los tokens dados, genera n con el más probable y
 * vuelca los logits de cada posición (float32) a un fichero. Solo en el PC. */
static int modo_tokens(llm_sessao_t *s, const char *ids, int n, const char *volcado)
{
    int toks[4096];
    int nt = ler_ids(ids, toks, 4096);
    if (nt <= 0) { llm_eprintf("llm: --tokens vacío\n"); return 1; }
    int total = nt + n;
    float *lg = llm_aloca((size_t)total * (size_t)s->m.n_vocab * 4);
    if (!lg) { llm_eprintf("llm: no hay memoria para los logits\n"); return 1; }
    unsigned t0 = llm_ms();
    if (llm_avaliar(&s->e, toks, nt, lg)) { llm_eprintf("llm: contexto lleno\n"); return 1; }
    s->ms_leitura = llm_ms() - t0; s->n_leitura = nt;
    llm_printf("GERADOS:");
    int pos = nt - 1;
    for (int i = 0; i < n; i++) {
        int id = llm_argmax(lg + (size_t)pos * s->m.n_vocab, s->m.n_vocab);
        llm_printf(" %d", id);
        if (i + 1 == n) break;
        t0 = llm_ms();
        if (llm_avaliar(&s->e, &id, 1, lg + (size_t)(pos + 1) * s->m.n_vocab)) break;
        s->ms_geracao += llm_ms() - t0; s->n_geracao++;
        pos++;
    }
    llm_printf("\n");
#ifdef LLM_ANFITRIAO
    if (volcado) {
        FILE *f = fopen(volcado, "wb");
        if (f) { fwrite(lg, 4, (size_t)(pos + 1) * s->m.n_vocab, f); fclose(f); }
    }
#else
    (void)volcado;
    /* En SaguiOS, una suma de los logits del último para comprobar sin volcar */
    double sum = 0;
    for (int i = 0; i < s->m.n_vocab; i++) sum += lg[(size_t)pos * s->m.n_vocab + i];
    long long sm = (long long)(sum * 1000);
    llm_printf("SUMA_LOGITS: %d\n", (int)sm);
#endif
    estatisticas(s);
    llm_libera(lg);
    return 0;
}

static int modo_tokenizar(llm_sessao_t *s, const char *texto, bool especiais)
{
    char u[8192];
    llm_latin1_utf8(texto, u, sizeof u);
    int t[4096];
    int n = llm_tokenizar(&s->t, u, llm_strlen(u), especiais, t, 4096);
    llm_printf("TOKENS:");
    for (int i = 0; i < n; i++) llm_printf(" %d", t[i]);
    llm_printf("\n");
    return 0;
}

static int ler_linha_entrada(char *b, int max)
{
#ifdef LLM_ANFITRIAO
    if (!fgets(b, max, stdin)) return -1;
    int n = (int)strlen(b);
    while (n && (b[n - 1] == '\n' || b[n - 1] == '\r')) b[--n] = 0;
    return n;
#else
    return ler_linha(ENTRADA, b, (unsigned)max);
#endif
}

static int modo_chat(llm_sessao_t *s, const char *sistema, bool pensar, int n_max)
{
    char ini[4200];
    inicio_chat(s, sistema, ini, sizeof ini);
    if (llm_alimentar_texto(s, ini, true)) { llm_eprintf("llm: contexto lleno\n"); return 1; }
    int base = s->e.kv.pos;
    llm_printf("Chat con %s (contexto %d tokens). /nuevo empieza otra conversación; /salir sale.\n",
               s->m.nome[0] ? s->m.nome : "el modelo", s->e.kv.ctx);
    char linha[4096], u[8192];
    for (;;) {
        llm_printf("\n> ");
        int n = ler_linha_entrada(linha, sizeof linha);
        if (n < 0 || igual(linha, "/salir") || igual(linha, "/sair")) break;
        if (!n) continue;
        if (igual(linha, "/nuevo")) {
            llm_estado_reiniciar(&s->e);
            s->hist_n = 0;
            llm_alimentar_texto(s, ini, true);
            base = s->e.kv.pos;
            llm_printf("(conversación nueva)\n");
            continue;
        }
        if (igual(linha, "/estado")) { estatisticas(s); continue; }
        llm_latin1_utf8(linha, u, sizeof u);
        for (int intento = 0; intento < 2; intento++) {
            int pos_antes = s->e.kv.pos;
            if (!llm_turno_usuario(s, u, pensar)) break;
            /* contexto lleno: se empieza de nuevo con solo esta pregunta */
            (void)pos_antes;
            llm_printf("(el contexto se ha llenado: se olvida la conversación anterior)\n");
            llm_estado_reiniciar(&s->e);
            s->hist_n = 0;
            llm_alimentar_texto(s, ini, true);
            base = s->e.kv.pos;
        }
        llm_saida_t sd = { 0 };
        sd.ocultar_think = true;
        bool cheio;
        llm_gerar(s, n_max, &sd, &cheio);
        llm_printf("\n");
        if (cheio) llm_printf("(contexto lleno: /nuevo para seguir)\n");
        else llm_fim_turno(s);
    }
    (void)base;
    return 0;
}

#ifndef LLM_ANFITRIAO
#define main principal   /* la entrada de los programas de SaguiOS (inicio.asm) */
#endif
int main(int argc, char **argv)
{
    const char *modelo = NULL, *prompt = NULL, *sistema = NULL, *tokens = NULL, *volcado = NULL, *tokenizar = NULL;
    int ctx = CTX_MIN_NORMAL, hilos = 0, n_max = 512, porta = 8080;
    const char *rota = NULL;
    bool ctx_explicito = false, info = false, pensar = false, crudo = false, servidor = false, anunciar = false;
    bool mostrar_ids = false, listar_modelos = false;
    const char *ativar = NULL;
    bool temp_d = false, topk_d = false, topp_d = false, rep_d = false, sem_d = false, especiais = false;
    float temp = 0, topp = 0, rep = 0;
    int topk = 0;
    uint32_t semente = 0;
    bool ok;
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i], *v = i + 1 < argc ? argv[i + 1] : NULL;
#define VALOR() do { if (!v) { llm_eprintf("llm: falta el valor de %s\n", a); return 2; } i++; } while (0)
        if (igual(a, "-h") || igual(a, "--ayuda") || igual(a, "--help")) { llm_printf("%s", AYUDA); return 0; }
        else if (igual(a, "-m") || igual(a, "--modelo")) { VALOR(); modelo = v; }
        else if (igual(a, "-p")) { VALOR(); prompt = v; }
        else if (igual(a, "--sistema")) { VALOR(); sistema = v; }
        else if (igual(a, "-n")) { VALOR(); n_max = (int)numero(v, &ok); if (!ok || n_max < 1) { llm_eprintf("llm: -n no válido\n"); return 2; } }
        else if (igual(a, "--contexto") || igual(a, "-c")) {
            VALOR(); ctx = (int)numero(v, &ok); ctx_explicito = true;
            if (!ok) { llm_eprintf("llm: contexto no válido: %s (p. ej. 12k, 24k, 48k)\n", v); return 2; }
        }
        else if (igual(a, "--hilos")) { VALOR(); hilos = (int)numero(v, &ok); if (!ok || hilos < 1) { llm_eprintf("llm: --hilos no válido\n"); return 2; } }
        else if (igual(a, "--temp")) { VALOR(); temp = real(v, &ok); temp_d = true; if (!ok) { llm_eprintf("llm: --temp no válido\n"); return 2; } }
        else if (igual(a, "--top-k")) { VALOR(); topk = (int)numero(v, &ok); topk_d = true; if (!ok) { llm_eprintf("llm: --top-k no válido\n"); return 2; } }
        else if (igual(a, "--top-p")) { VALOR(); topp = real(v, &ok); topp_d = true; if (!ok) { llm_eprintf("llm: --top-p no válido\n"); return 2; } }
        else if (igual(a, "--repeticion")) { VALOR(); rep = real(v, &ok); rep_d = true; if (!ok) { llm_eprintf("llm: --repeticion no válido\n"); return 2; } }
        else if (igual(a, "--semilla")) { VALOR(); semente = (uint32_t)numero(v, &ok); sem_d = true; if (!ok) { llm_eprintf("llm: --semilla no válida\n"); return 2; } }
        else if (igual(a, "--info")) info = true;
        else if (igual(a, "--pensar")) pensar = true;
        else if (igual(a, "--crudo")) crudo = true;
        else if (igual(a, "--servidor")) servidor = true;
        else if (igual(a, "--puerto")) { VALOR(); porta = (int)numero(v, &ok); if (!ok || porta < 1 || porta > 65535) { llm_eprintf("llm: --puerto no válido\n"); return 2; } }
        else if (igual(a, "--anunciar")) anunciar = true;
        else if (igual(a, "--modelos")) listar_modelos = true;
        else if (igual(a, "--activar")) { VALOR(); ativar = v; }
        else if (igual(a, "--ruta")) {
            VALOR(); rota = v;
            if (!igual(v, "c") && !igual(v, "sse2") && !igual(v, "ssse3")) { llm_eprintf("llm: --ruta c, sse2 o ssse3\n"); return 2; }
        }
        /* para las pruebas y la comparación con llama.cpp */
        else if (igual(a, "--tokens")) { VALOR(); tokens = v; }
        else if (igual(a, "--volcar-logits")) { VALOR(); volcado = v; }
        else if (igual(a, "--tokenizar")) { VALOR(); tokenizar = v; }
        else if (igual(a, "--especiales")) especiais = true;
        else if (igual(a, "--mostrar-ids")) mostrar_ids = true;
        else { llm_eprintf("llm: opción desconocida: %s (llm --ayuda)\n", a); return 2; }
#undef VALOR
    }
    if (ctx > CTX_MAX) { llm_eprintf("llm: el contexto va como mucho hasta 48k tokens (pedido: %d)\n", ctx); return 2; }
    if (ctx < 256) { llm_eprintf("llm: contexto demasiado pequeño (mínimo 256 tokens)\n"); return 2; }
    if (!ctx_explicito) ctx = ctx_minimo();
    else if (ctx < ctx_minimo())
        llm_eprintf("llm: aviso: %d tokens es menos que el mínimo de este equipo (%dk); vale para pruebas\n", ctx,
                    ctx_minimo() / 1024);
    if (!rota && !tem_ssse3() && llm_cpu_sse2()) rota = "sse2";
    if (!llm_quant_escolher(rota)) { llm_eprintf("llm: esta CPU no tiene %s\n", rota); return 2; }

    char erro[320], caminho[256];
    if (anunciar) return llm_anunciar(modelo);
    if (listar_modelos) return modo_modelos();
    if (ativar) return modo_ativar(ativar);
    if (!modelo) {
        int r = llm_modelo_padrao(caminho, sizeof caminho);
        if (r == 0) {
            llm_eprintf("llm: no hay ningún modelo en %s ni en ~/modelos (use -m FICHERO.gguf; ver /ayuda/llm.md)\n", PASTA_MODELOS);
            return 1;
        }
        if (r < 0) {
            llm_eprintf("llm: hay varios modelos: elija uno con -m o con llm --activar (llm --modelos los lista)\n");
            return 1;
        }
        modelo = caminho;
    }
    if (info) return modo_info(modelo, ctx);
    if (!llm_ram_suficiente(erro, sizeof erro)) { llm_eprintf("llm: %s\n", erro); return 1; }

    llm_sessao_t s;
    llm_memset(&s, 0, sizeof s);
    if (llm_preparar(&s, modelo, ctx, tokens != NULL || tokenizar != NULL, erro, sizeof erro)) {
        llm_eprintf("llm: %s\n", erro);
        return 1;
    }
    if (!hilos) hilos = llm_cpus() > 8 ? 8 : llm_cpus();   /* de serie, un hilo por CPU (D380) */
    if (hilos > 1) hilos = llm_hilos_iniciar(hilos);
    if (temp_d) s.a.temperatura = temp;
    if (topk_d) s.a.top_k = topk;
    if (topp_d) s.a.top_p = topp;
    if (rep_d) s.a.penalizacao = rep;
    if (sem_d) llm_amostra_iniciar_semente(&s.a, semente);
    int r = 0;
    if (tokenizar) r = modo_tokenizar(&s, tokenizar, especiais);
    else if (tokens) r = modo_tokens(&s, tokens, n_max, volcado);
    else if (servidor) r = llm_servidor(&s, porta, sistema);
    else if (prompt) {
        char u[8192];
        llm_latin1_utf8(prompt, u, sizeof u);
        int e;
        if (crudo) e = llm_alimentar_texto(&s, u, false);
        else {
            char ini[4200];
            inicio_chat(&s, sistema, ini, sizeof ini);
            e = llm_alimentar_texto(&s, ini, true);
            if (!e) e = llm_turno_usuario(&s, u, pensar);
        }
        if (e) { llm_eprintf("llm: el texto no cabe en el contexto (%d tokens)\n", s.e.kv.ctx); r = 1; }
        else {
            llm_saida_t sd = { 0 };
            sd.ocultar_think = true;
            bool cheio;
            int h0 = s.hist_n;
            llm_gerar(&s, n_max, &sd, &cheio);
            llm_printf("\n");
            if (mostrar_ids) {   /* para las pruebas: los tokens generados */
                llm_printf("GERADOS:");
                for (int i = h0; i < s.hist_n; i++) llm_printf(" %d", s.hist[i]);
                llm_printf("\n");
            }
            estatisticas(&s);
        }
    } else r = modo_chat(&s, sistema, pensar, n_max);
    if (hilos > 1) llm_hilos_parar();
    llm_fechar_sessao(&s);
    return r;
}
