// SPDX-License-Identifier: GPL-2.0-or-later
/* cache_disco.c - caché de bloques con lectura anticipada y escritura diferida (D281).
 * Ver kernel/inc/cache_disco.h para el modelo. Detalles:
 *
 *  - Bloque = 4 KB = 8 sectores alineados. El último bloque de un disco que no es múltiplo de 8
 *    sectores no se cachea (pasa directo al driver).
 *  - Un solo cerrojo (trava_cache). El driver puede dormir (DMA, D281) con él tomado: nadie más
 *    toca la caché mientras tanto. Orden de cerrojos: trava_fs -> trava_cache -> cerrojo del canal.
 *  - Los pedidos grandes (>= 128 KB) no pasan por la caché: se vacían los bloques sucios que
 *    solapen, se leen o graban directos y, en la grabación, se actualizan los bloques presentes.
 *  - Lectura anticipada: si un pedido empieza donde acabó el anterior del mismo disco, en un
 *    fallo se leen además hasta 8 bloques por delante (la ventana se duplica hasta 32 = 128 KB).
 *  - Escritura diferida: el bloque queda sucio con un número de orden creciente. El vaciado
 *    graba los sucios por ese orden (juntando los contiguos en una sola orden al driver, que
 *    en ATA termina con FLUSH CACHE): un corte de luz deja un prefijo de lo escrito.
 *  - Con la caché llena se expulsa el bloque limpio menos reciente; si no hay ninguno, se vacía
 *    el disco y se expulsa el último. Un error al grabar un bloque se anota y el bloque se
 *    descarta (si no, un disco retirado repetiría el error para siempre). */
#include "cache_disco.h"
#include "abi.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "trava.h"

#define BL_SETORES    8u
#define BL_BYTES      (BL_SETORES * 512u)
#define HASH_N        2048u
#define DIRETO_SETORES 256u       /* desde 128 KB el pedido va directo */
#define RA_INICIAL    8u
#define RA_MAX        32u
#define LOTE_BLOCOS   32u         /* bloques por orden al driver al leer por delante o vaciar */
#define ENVELHECE_TIQUES (3 * TIQUES_POR_SEGUNDO)   /* un bloque sucio se graba a los 3 s */
#define MAX_SEQS      8

typedef struct bloco {
    struct bloco *hprox;          /* cadena del hash */
    struct bloco *ant, *prox;     /* LRU: lru_ini es el más reciente */
    disco_t      *d;
    uint32_t      num;            /* número de bloque (sector / 8) */
    uint8_t      *dados;
    bool          sujo;
    uint32_t      seq, t_sujo;
} bloco_t;

static trava_t trava_cache;
static bloco_t *hash[HASH_N];
static bloco_t *lru_ini, *lru_fim;
static uint32_t n_blocos, max_blocos, n_sujos, max_sujos, seq_global;
static uint8_t *lote;             /* búfer contiguo de LOTE_BLOCOS bloques (lecturas) */
static uint8_t *lote_v;           /* otro para vaciar: una lectura en curso conserva el suyo */
static bool iniciada, ligada, diferida;
static struct { disco_t *d; uint32_t prox, ra; } seqs[MAX_SEQS];
static cache_disco_est_t est;
static const char canal_tarefa = 0;

static uint32_t clave(disco_t *d, uint32_t num) { return (num * 2654435761u ^ (uint32_t)(uintptr_t)d >> 4) % HASH_N; }

static void iniciar(void)
{
    if (iniciada) return;
    iniciada = true;
    uint8_t o = cabecalho_boot[CAB_DISCO_OPC];
    uint32_t pags = memfisica_geridas() / 16;               /* 1/16 de la RAM... */
    if (pags < 256) pags = 256;                              /* ...entre 1 MB */
    if (pags > 16384) pags = 16384;                          /* ...y 64 MB */
    max_blocos = pags;
    max_sujos = pags / 4;
    lote = kmalloc(LOTE_BLOCOS * BL_BYTES);
    lote_v = kmalloc(LOTE_BLOCOS * BL_BYTES);
    ligada = !(o & CAB_DISCO_SEM_CACHE) && lote != NULL && lote_v != NULL;
    diferida = ligada && !(o & CAB_DISCO_SEM_DIFERIDA);
    klog("[CACHE] caché de bloques %s: hasta %u bloques de 4 KB (%u KB), escritura %s\n",
         ligada ? "activa" : "desactivada (--sem-cache-disco)", max_blocos, max_blocos * 4, diferida ? "diferida" : "directa");
}

bool cache_disco_aplica(disco_t *r)
{
    iniciar();
    if (!ligada || !r || r->pai || !r->ler) return false;
    if (r->removivel || r->cd || r->imagem_cd || r->audio_cd) return false;
    return true;
}

/* ------------------------------------------------------------ estructura */

static bloco_t *achar(disco_t *d, uint32_t num)
{
    for (bloco_t *b = hash[clave(d, num)]; b; b = b->hprox)
        if (b->d == d && b->num == num) return b;
    return NULL;
}

static void lru_tirar(bloco_t *b)
{
    if (b->ant) b->ant->prox = b->prox; else lru_ini = b->prox;
    if (b->prox) b->prox->ant = b->ant; else lru_fim = b->ant;
    b->ant = b->prox = NULL;
}

static void lru_frente(bloco_t *b)
{
    b->ant = NULL;
    b->prox = lru_ini;
    if (lru_ini) lru_ini->ant = b;
    lru_ini = b;
    if (!lru_fim) lru_fim = b;
}

static void tocar(bloco_t *b)
{
    if (lru_ini == b) return;
    lru_tirar(b);
    lru_frente(b);
}

static void hash_tirar(bloco_t *b)
{
    for (bloco_t **p = &hash[clave(b->d, b->num)]; *p; p = &(*p)->hprox)
        if (*p == b) { *p = b->hprox; return; }
}

static void marcar_sujo(bloco_t *b)
{
    if (!b->sujo) { b->sujo = true; n_sujos++; b->t_sujo = relogio_tiques(); }
    b->seq = ++seq_global;
}

static void marcar_limpo(bloco_t *b)
{
    if (b->sujo) { b->sujo = false; n_sujos--; }
}

/* Quita un bloque del todo (descarte) y devuelve su memoria. */
static void apagar(bloco_t *b)
{
    marcar_limpo(b);
    hash_tirar(b);
    lru_tirar(b);
    memfisica_liberar((uint32_t)b->dados);
    kfree(b);
    n_blocos--;
}

/* ------------------------------------------------------------ vaciado */

static int driver_escrever(disco_t *r, uint32_t lba, uint32_t n, const void *buf)
{
    if (!r->escrever) return -1;
    return r->escrever(r, lba, n, buf) < 0 ? -1 : 0;
}

/* Graba los bloques sucios de "r" (NULL = todos) cuyo rango de sectores solape [lba, lba+n)
 * (n = 0: sin filtro de rango), en el orden en que se escribieron. */
static int vaciar(disco_t *r, uint32_t lba, uint32_t n)
{
    if (!n_sujos) return 0;
    bloco_t **v = kmalloc((size_t)n_sujos * sizeof *v);
    if (!v) return -1;
    uint32_t q = 0;
    for (bloco_t *b = lru_ini; b && q < n_sujos; b = b->prox) {
        if (!b->sujo || (r && b->d != r)) continue;
        if (n && (b->num * BL_SETORES >= lba + n || (b->num + 1) * BL_SETORES <= lba)) continue;
        v[q++] = b;
    }
    for (uint32_t a = 1; a < q; a++) {              /* inserción por número de orden */
        bloco_t *x = v[a];
        uint32_t k = a;
        while (k > 0 && v[k - 1]->seq > x->seq) { v[k] = v[k - 1]; k--; }
        v[k] = x;
    }
    int res = 0;
    for (uint32_t a = 0; a < q; ) {
        uint32_t c = 1;
        while (a + c < q && c < LOTE_BLOCOS && v[a + c]->d == v[a]->d && v[a + c]->num == v[a]->num + c) c++;
        for (uint32_t j = 0; j < c; j++) memcpy(lote_v + j * BL_BYTES, v[a + j]->dados, BL_BYTES);
        int w = driver_escrever(v[a]->d, v[a]->num * BL_SETORES, c * BL_SETORES, lote_v);
        if (w < 0) {
            est.erros++;
            klog("[CACHE] %s: error al grabar %u bloques desde el sector %u; se descartan\n", v[a]->d->nome, c,
                 v[a]->num * BL_SETORES);
            res = -1;
        }
        for (uint32_t j = 0; j < c; j++) marcar_limpo(v[a + j]);   /* también con error (ver arriba) */
        est.gravados += c;
        a += c;
    }
    kfree(v);
    est.vaciados++;
    return res;
}

int cache_disco_sincronizar(disco_t *r)
{
    iniciar();
    if (!ligada) return 0;
    trava_pegar(&trava_cache);
    int x = vaciar(r, 0, 0);
    trava_soltar(&trava_cache);
    return x < 0 ? ERRO_ACESSO : 0;
}

void cache_disco_descartar(disco_t *r)
{
    if (!iniciada || !ligada) return;
    trava_pegar(&trava_cache);
    for (bloco_t *b = lru_ini, *p; b; b = p) {
        p = b->prox;
        if (b->d == r) apagar(b);
    }
    for (int i = 0; i < MAX_SEQS; i++) if (seqs[i].d == r) seqs[i].d = NULL;
    trava_soltar(&trava_cache);
}

/* ------------------------------------------------------------ asignación */

/* Un bloque nuevo (sin datos válidos), ya en el hash y en la cabeza del LRU. NULL si no hay memoria. */
static bloco_t *novo_bloco(disco_t *d, uint32_t num)
{
    bloco_t *b = NULL;
    if (n_blocos < max_blocos && memfisica_livres() > 2048) {
        b = kcalloc(sizeof *b);
        uint32_t pag = b ? memfisica_alocar() : 0;
        if (b && !pag) { kfree(b); b = NULL; }
        else if (b) { b->dados = (uint8_t *)pag; n_blocos++; }
    }
    if (!b) {                                       /* expulsar: el limpio menos reciente */
        bloco_t *v = lru_fim;
        for (int i = 0; v && v->sujo && i < 64; i++) v = v->ant;
        if (!v || v->sujo) {
            if (!lru_fim) return NULL;
            vaciar(lru_fim->d, 0, 0);
            v = lru_fim;
            if (v->sujo) return NULL;
        }
        hash_tirar(v);
        lru_tirar(v);
        b = v;
        b->sujo = false;
    }
    b->d = d;
    b->num = num;
    b->hprox = hash[clave(d, num)];
    hash[clave(d, num)] = b;
    lru_frente(b);
    return b;
}

/* ------------------------------------------------------------ lectura */

/* Lectura anticipada: ¿el pedido empieza donde acabó el anterior? Devuelve los bloques extra. */
static uint32_t anticipo(disco_t *r, uint32_t lba, uint32_t n)
{
    int k = -1, livre = -1;
    for (int i = 0; i < MAX_SEQS; i++) {
        if (seqs[i].d == r) { k = i; break; }
        if (!seqs[i].d && livre < 0) livre = i;
    }
    if (k < 0) {
        k = livre >= 0 ? livre : 0;
        seqs[k].d = r; seqs[k].prox = lba + n; seqs[k].ra = 0;
        return 0;
    }
    if (seqs[k].prox == lba) seqs[k].ra = seqs[k].ra ? (seqs[k].ra * 2 > RA_MAX ? RA_MAX : seqs[k].ra * 2) : RA_INICIAL;
    else seqs[k].ra = 0;
    seqs[k].prox = lba + n;
    return seqs[k].ra;
}

int cache_disco_ler(disco_t *r, uint32_t lba, uint32_t n, void *buf)
{
    uint8_t *dest = buf;
    int res = 0;
    if (!n) return 0;
    trava_pegar(&trava_cache);
    if (n >= DIRETO_SETORES) {
        est.diretos++;
        vaciar(r, lba, n);
        res = r->ler(r, lba, n, buf) < 0 ? ERRO_ACESSO : 0;
        goto fim;
    }
    uint32_t ra = anticipo(r, lba, n);
    uint32_t completos = r->setores / BL_SETORES;           /* bloques enteros del disco */
    uint32_t b0 = lba / BL_SETORES, b1 = (lba + n - 1) / BL_SETORES;
    for (uint32_t b = b0; b <= b1 && res == 0; ) {
        uint32_t s0 = b * BL_SETORES;                       /* sectores del bloque que piden: [ini, fim) */
        uint32_t ini = lba > s0 ? lba : s0;
        uint32_t fim = lba + n < s0 + BL_SETORES ? lba + n : s0 + BL_SETORES;
        if (b >= completos) {                               /* el bloque final incompleto no se cachea */
            res = r->ler(r, ini, fim - ini, dest + (ini - lba) * 512u) < 0 ? ERRO_ACESSO : 0;
            b++;
            continue;
        }
        bloco_t *k = achar(r, b);
        if (k) {
            est.acertos++;
            tocar(k);
            memcpy(dest + (ini - lba) * 512u, k->dados + (ini - s0) * 512u, (fim - ini) * 512u);
            b++;
            continue;
        }
        /* Fallo: se leen juntos los bloques que faltan del pedido y, si es secuencial, de por delante. */
        uint32_t c = 1;
        while (b + c <= b1 && b + c < completos && c < LOTE_BLOCOS && !achar(r, b + c)) c++;
        uint32_t extra = (b + c > b1) ? ra : 0;
        while (extra && b + c < completos && c < LOTE_BLOCOS && !achar(r, b + c)) { c++; extra--; est.anticipados++; }
        est.falhas++;
        if (r->ler(r, b * BL_SETORES, c * BL_SETORES, lote) < 0) { res = ERRO_ACESSO; break; }
        for (uint32_t j = 0; j < c; j++) {
            bloco_t *nb = novo_bloco(r, b + j);
            if (!nb) continue;                              /* sin memoria: el bloque no se cachea */
            memcpy(nb->dados, lote + j * BL_BYTES, BL_BYTES);
        }
        /* Se sirve el pedido desde el lote (sin volver a buscar: el bloque pudo no entrar). */
        for (uint32_t j = 0; j < c && b + j <= b1; j++) {
            uint32_t t0 = (b + j) * BL_SETORES;
            uint32_t i2 = lba > t0 ? lba : t0;
            uint32_t f2 = lba + n < t0 + BL_SETORES ? lba + n : t0 + BL_SETORES;
            memcpy(dest + (i2 - lba) * 512u, lote + j * BL_BYTES + (i2 - t0) * 512u, (f2 - i2) * 512u);
        }
        b += (c > b1 - b + 1) ? (b1 - b + 1) : c;
    }
fim:
    trava_soltar(&trava_cache);
    return res;
}

/* ------------------------------------------------------------ escritura */

int cache_disco_escrever(disco_t *r, uint32_t lba, uint32_t n, const void *buf, bool diferir)
{
    const uint8_t *orig = buf;
    int res = 0;
    if (!n) return 0;
    trava_pegar(&trava_cache);
    uint32_t completos = r->setores / BL_SETORES;
    uint32_t b0 = lba / BL_SETORES, b1 = (lba + n - 1) / BL_SETORES;
    if (!diferir || !diferida || n >= DIRETO_SETORES) {
        /* Directo: primero lo sucio que solape (para no pisar lo nuevo con lo viejo al vaciar), luego
         * el driver y por último los bloques presentes se ponen al día. */
        est.diretos++;
        vaciar(r, lba, n);
        res = driver_escrever(r, lba, n, buf) < 0 ? ERRO_ACESSO : 0;
        if (res == 0)
            for (uint32_t b = b0; b <= b1; b++) {
                bloco_t *k = b < completos ? achar(r, b) : NULL;
                if (!k) continue;
                uint32_t s0 = b * BL_SETORES;
                uint32_t ini = lba > s0 ? lba : s0, fim = lba + n < s0 + BL_SETORES ? lba + n : s0 + BL_SETORES;
                memcpy(k->dados + (ini - s0) * 512u, orig + (ini - lba) * 512u, (fim - ini) * 512u);
            }
        goto fim;
    }
    for (uint32_t b = b0; b <= b1 && res == 0; b++) {
        uint32_t s0 = b * BL_SETORES;
        uint32_t ini = lba > s0 ? lba : s0, fim = lba + n < s0 + BL_SETORES ? lba + n : s0 + BL_SETORES;
        bool inteiro = fim - ini == BL_SETORES;
        if (b >= completos) {                               /* bloque final incompleto: directo */
            res = driver_escrever(r, ini, fim - ini, orig + (ini - lba) * 512u) < 0 ? ERRO_ACESSO : 0;
            continue;
        }
        bloco_t *k = achar(r, b);
        if (!k) {
            k = novo_bloco(r, b);
            if (k && !inteiro && r->ler(r, s0, BL_SETORES, k->dados) < 0) { apagar(k); k = NULL; res = ERRO_ACESSO; break; }
            if (!k) {                                       /* sin memoria: directo */
                res = driver_escrever(r, ini, fim - ini, orig + (ini - lba) * 512u) < 0 ? ERRO_ACESSO : 0;
                continue;
            }
        } else tocar(k);
        memcpy(k->dados + (ini - s0) * 512u, orig + (ini - lba) * 512u, (fim - ini) * 512u);
        marcar_sujo(k);
    }
    if (res == 0 && n_sujos > max_sujos) vaciar(r, 0, 0);
fim:
    trava_soltar(&trava_cache);
    return res;
}

/* ------------------------------------------------------------ vaciado periódico y cifras */

static void tarefa_vaciar(void *arg)
{
    (void)arg;
    for (;;) {
        int_desliga();
        processo_atual()->acordar_em = relogio_tiques() + TIQUES_POR_SEGUNDO;
        bloquear(&canal_tarefa);
        ceder();
        int_liga();
        if (!n_sujos) continue;
        trava_pegar(&trava_cache);
        uint32_t agora = relogio_tiques();
        bool velho = false;
        for (bloco_t *b = lru_ini; b; b = b->prox)
            if (b->sujo && (int32_t)(agora - b->t_sujo) >= (int32_t)ENVELHECE_TIQUES) { velho = true; break; }
        if (velho || n_sujos > max_sujos / 2) vaciar(NULL, 0, 0);
        trava_soltar(&trava_cache);
    }
}

void cache_disco_tarefa_iniciar(void)
{
    iniciar();
    if (ligada && diferida) tarefa_kernel("sync-disco", tarefa_vaciar, NULL);
}

void cache_disco_estatisticas(cache_disco_est_t *e)
{
    *e = est;
    e->blocos = n_blocos; e->max_blocos = max_blocos; e->sujos = n_sujos; e->max_sujos = max_sujos;
}

void cache_disco_registar(void)
{
    cache_disco_est_t e;
    cache_disco_estatisticas(&e);
    klog("[CACHE] bloques %u/%u, sucios %u, aciertos %u, fallos %u, anticipados %u, grabados %u, vaciados %u, directos %u, errores %u\n",
         e.blocos, e.max_blocos, e.sujos, e.acertos, e.falhas, e.anticipados, e.gravados, e.vaciados, e.diretos, e.erros);
}
