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
 *    descarta (si no, un disco retirado repetiría el error para siempre).
 *
 * D350 (mejora 125, fase 3a): los DATOS de los bloques van a la memoria alta o superior si la hay
 * (los descriptores y el hash siguen en la baja). Nunca se usa su dirección física como puntero:
 *  - todo acceso es una copia por fisica_ler()/fisica_escrever() (ventanas de memalta.c); si el
 *    otro lado puede ser memoria de un programa (el búfer del que lee o escribe), se pasa por
 *    "ponte" (4 KB en la baja), para no tener la ventana puesta (interrupciones apagadas) si esa
 *    copia provoca un fallo de página;
 *  - el DMA nunca va a un bloque: los drivers leen en "lote" y graban desde "lote_v" (baja), como
 *    antes, y el relleno de un bloque a medio escribir también se lee en "lote". Así el IDE Bus
 *    Master (32 bits) y cualquier controlador sirven aunque el bloque esté por encima de 4 GB.
 *  - Tamaño: sin alta (o con menos de 64 MB) como siempre, 1/16 de la baja entre 1 y 64 MB. Con
 *    alta, hasta la mitad de la alta + superior, como mucho TETO_ALTA_BLOCOS (1 GB), y sin bajar
 *    nunca de "reserva" páginas altas libres para los programas (1/32 de la alta, 16 MB como
 *    mínimo); bloques bajos solo cuando la alta no da (hasta el límite de siempre).
 *  - Ceder memoria: memfisica_alocar_usuario() sin páginas llama a ceder_memoria() (memfisica_registar_
 *    reclamo), que libera bloques LIMPIOS del final del LRU solo si trava_cache está LIBRE (nadie,
 *    tampoco el propio proceso, está dentro de la caché); no espera nunca: quien asigna páginas de
 *    programa puede estar entre la comprobación y la escritura de una entrada de la tabla
 *    (espaco_alocar) o con una ventana puesta. Además la tarea "sync-disco" devuelve cada segundo
 *    lo que haga falta para que vuelva a haber "reserva" páginas altas libres. */
#include "cache_disco.h"
#include "abi.h"
#include "bootinfo.h"
#include "kconf.h"
#include "console.h"
#include "cpu.h"
#include "memalta.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "trava.h"

#define BL_SETORES    8u
#define BL_BYTES      (BL_SETORES * 512u)
#define HASH_MIN_BITS 11u         /* 2048 cadenas como mínimo */
#define TETO_ALTA_BLOCOS 262144u  /* D350: 1 GB de caché en la alta como mucho (ver D350) */
#define ALTA_MINIMA   16384u      /* D350: con menos de 64 MB de alta, la caché sigue en la baja */
#define RESERVA_MIN   4096u       /* D350: 16 MB altos libres para los programas, como mínimo */
#define CEDER_TAREFA  8192u       /* D350: bloques que la tarea devuelve como mucho por vuelta */
#define DIRETO_SETORES 256u       /* desde 128 KB el pedido va directo */
#define RA_INICIAL    8u
#define RA_MAX        32u
#define LOTE_BLOCOS   32u         /* bloques por orden al driver al leer por delante o vaciar */
#define ENVELHECE_TIQUES (3 * TIQUES_POR_SEGUNDO)   /* un bloque sucio se graba a los 3 s (kconfig vaciado_s, D353) */
#define MAX_SEQS      8

typedef struct bloco {
    struct bloco *hprox;          /* cadena del hash */
    struct bloco *ant, *prox;     /* LRU: lru_ini es el más reciente (libre: prox = siguiente libre) */
    disco_t      *d;
    uint32_t      num;            /* número de bloque (sector / 8) */
    uint32_t      pag;            /* D350: página física de los datos (dirección >> 12), baja o alta */
    uint32_t      seq, t_sujo;
    bool          sujo;
    bool          alto;           /* D350: los datos están en la alta/superior (solo por ventanas) */
} bloco_t;

static trava_t trava_cache;
static bloco_t **hash;            /* D350: del tamaño de la caché (potencia de 2) */
static uint32_t hash_bits;
static bloco_t *descr, *descr_livres;   /* D350: los descriptores, todos de una vez en la baja */
static bloco_t *lru_ini, *lru_fim;
static uint32_t n_blocos, max_blocos, n_sujos, max_sujos, seq_global;
static uint32_t n_altos, n_baixos, max_baixos, reserva;   /* D350 */
static uint32_t envelhece_tiques = ENVELHECE_TIQUES;   /* D353 */
static bool com_alta;             /* D350: los datos van a la alta */
static uint8_t ponte[BL_BYTES];   /* D350: puente entre una ventana y memoria de un programa */
static volatile uint32_t pedido_ceder;   /* D350: bloques pedidos con la caché ocupada (ver soltar()) */
static uint8_t *lote;             /* búfer contiguo de LOTE_BLOCOS bloques (lecturas) */
static uint8_t *lote_v;           /* otro para vaciar: una lectura en curso conserva el suyo */
static bool iniciada, ligada, diferida;
static struct { disco_t *d; uint32_t prox, ra; } seqs[MAX_SEQS];
static cache_disco_est_t est;
static const char canal_tarefa = 0;

static uint32_t clave(disco_t *d, uint32_t num)
{
    return ((num ^ (uint32_t)(uintptr_t)d >> 4) * 2654435761u) >> (32 - hash_bits);
}

static uint32_t ceder_memoria(uint32_t quer);

/* D350: descriptores y hash para "max_blocos"; si la baja no da, la caché se reduce a la mitad. */
static bool reservar_estructuras(void)
{
    for (; max_blocos >= 256; max_blocos /= 2) {
        hash_bits = HASH_MIN_BITS;
        while ((1u << hash_bits) < max_blocos / 2) hash_bits++;
        descr = kcalloc(max_blocos * sizeof *descr);
        hash = descr ? kcalloc((1u << hash_bits) * sizeof *hash) : NULL;
        if (descr && hash) break;
        kfree(descr);
        descr = NULL;
    }
    if (!descr) return false;
    for (uint32_t i = max_blocos; i-- > 0;) { descr[i].prox = descr_livres; descr_livres = &descr[i]; }
    return true;
}

static void iniciar(void)
{
    if (iniciada) return;
    iniciada = true;
    uint8_t o = kconf_cab(CAB_DISCO_OPC);   /* D353: con lo que guardó kconfig */
    uint32_t pags = memfisica_geridas() / 16;               /* 1/16 de la RAM baja... */
    if (pags < 256) pags = 256;                              /* ...entre 1 MB */
    if (pags > 16384) pags = 16384;                          /* ...y 64 MB */
    max_baixos = pags;
    max_blocos = pags;
    /* D350: con memoria alta, hasta la mitad de ella (tope de 1 GB) y siempre dejando "reserva". */
    uint32_t alta = memfisica_geridas_alta();
    if (alta >= ALTA_MINIMA && !(o & CAB_DISCO_CACHE_BAIXA)) {
        com_alta = true;
        max_blocos = alta / 2 > TETO_ALTA_BLOCOS ? TETO_ALTA_BLOCOS : alta / 2;
        if (max_blocos < max_baixos) max_blocos = max_baixos;
        reserva = alta / 32 > RESERVA_MIN ? alta / 32 : RESERVA_MIN;
    }
    max_sujos = pags / 4;                                    /* lo sucio, como siempre (16 MB como mucho) */
    /* D353: kconfig solo puede BAJAR los topes (cache_max_mb de 4 a 1024, sucios_max_mb de 1 a 16) y
     * cambiar a cuántos segundos se graba un bloque sucio (vaciado_s, de 1 a 30; 3 de serie). */
    uint32_t tope = kconf_valor(KC_CACHE_MAX_MB) * 256u;
    if (tope && max_blocos > tope) {
        max_blocos = tope;
        if (max_baixos > max_blocos) max_baixos = max_blocos;
        if (max_sujos > max_blocos / 4) max_sujos = max_blocos / 4;
    }
    uint32_t tope_sujos = kconf_valor(KC_SUCIOS_MAX_MB) * 256u;
    if (tope_sujos && max_sujos > tope_sujos) max_sujos = tope_sujos;
    if (kconf_valor(KC_VACIADO_S)) envelhece_tiques = kconf_valor(KC_VACIADO_S) * TIQUES_POR_SEGUNDO;
    lote = kmalloc(LOTE_BLOCOS * BL_BYTES);
    lote_v = kmalloc(LOTE_BLOCOS * BL_BYTES);
    ligada = !(o & CAB_DISCO_SEM_CACHE) && lote != NULL && lote_v != NULL && reservar_estructuras();
    diferida = ligada && !(o & CAB_DISCO_SEM_DIFERIDA);
    if (com_alta && max_baixos > max_blocos) max_baixos = max_blocos;
    klog("[CACHE] caché de bloques %s: hasta %u bloques de 4 KB (%u KB), escritura %s\n",
         ligada ? "activa" : "desactivada (--sem-cache-disco)", max_blocos, max_blocos * 4, diferida ? "diferida" : "directa");
    if (ligada && (tope || tope_sujos || envelhece_tiques != ENVELHECE_TIQUES))
        klog("[CACHE] kconfig: hasta %u MB, sucios hasta %u KB, un bloque sucio se graba a los %u s\n",
             max_blocos / 256, max_sujos * 4, envelhece_tiques / TIQUES_POR_SEGUNDO);
    if (ligada && com_alta)
        klog("[CACHE] datos en la memoria alta: hasta %u MB, %u MB altos siempre libres para los programas; "
             "descriptores %u KB en la baja\n", max_blocos / 256, reserva / 256,
             (max_blocos * (uint32_t)sizeof *descr + (1u << hash_bits) * 4u) / 1024);
    else if (ligada && alta && (o & CAB_DISCO_CACHE_BAIXA))
        klog("[CACHE] datos en la memoria baja (--cache-disco-baja)\n");
    if (ligada) memfisica_registar_reclamo(ceder_memoria);
}

/* ------------------------------------------------------------ datos (D350) */

static inline uint64_t fis_de(const bloco_t *b) { return (uint64_t)b->pag << 12; }

/* Copia del bloque a "dest". "seguro": dest es memoria del kernel que no puede fallar (lote,
 * lote_v); si no, puede ser de un programa y, con el bloque en la alta, se pasa por "ponte". */
static void dados_ler(const bloco_t *b, uint32_t off, void *dest, uint32_t n, bool seguro)
{
    if (b->alto && !seguro) {
        fisica_ler(fis_de(b) + off, ponte, n);
        memcpy(dest, ponte, n);
    } else fisica_ler(fis_de(b) + off, dest, n);
}

static void dados_escrever(const bloco_t *b, uint32_t off, const void *orig, uint32_t n, bool seguro)
{
    if (b->alto && !seguro) {
        memcpy(ponte, orig, n);
        fisica_escrever(fis_de(b) + off, ponte, n);
    } else fisica_escrever(fis_de(b) + off, orig, n);
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
    memfisica_liberar(fis_de(b));
    if (b->alto) n_altos--; else n_baixos--;
    b->d = NULL;
    b->prox = descr_livres;
    descr_livres = b;
    n_blocos--;
}

/* D350: devuelve hasta "quer" bloques limpios, del menos reciente al más. Con trava_cache.
 * "so_altos": solo los que están en la alta (la tarea, que repone la reserva de la alta: soltar
 * bloques bajos no la repone y vaciaría la caché cada segundo sin provecho). */
static uint32_t liberar_limpos(uint32_t quer, bool so_altos)
{
    uint32_t n = 0;
    for (bloco_t *b = lru_fim, *a; b && n < quer; b = a) {
        a = b->ant;
        if (b->sujo || (so_altos && !b->alto)) continue;
        apagar(b);
        n++;
    }
    est.cedidos += n;
    return n;
}

/* D350: el reclamo de memfisica_alocar_usuario(). Solo si nadie está dentro de la caché (ni
 * siquiera este proceso: podría ser un fallo de página a mitad de una copia); nunca espera. */
static uint32_t ceder_memoria(uint32_t quer)
{
    if (!ligada || !n_blocos || !processo_atual()) return 0;
    bool i = int_salva_desliga();
    if (trava_cache.profundidade != 0) {
        /* Ocupada: lo libera quien la tiene al terminar su operación (soltar()). Ahora la página
         * sale de la baja, pero las siguientes vuelven a encontrar alta. */
        if (pedido_ceder < CEDER_TAREFA) pedido_ceder += quer;
        est.reclamos_ocupada++;
        int_restaura(i);
        return 0;
    }
    trava_pegar(&trava_cache);              /* libre: no duerme */
    int_restaura(i);
    uint32_t n = liberar_limpos(quer, false);
    if (n) est.reclamos++;
    trava_soltar(&trava_cache);
    return n;
}

/* D350: suelta trava_cache atendiendo antes lo que se pidió mientras estaba ocupada. En un punto
 * en que la caché está entera (al final de cada operación) y por su dueño: sin carreras. Solo si
 * el cerrojo sigue siendo nuestro (si nos lo hubieran robado, otro podría estar a medias). */
static void soltar(void)
{
    processo_t *eu = processo_atual();
    if (pedido_ceder && trava_cache.profundidade == 1 && eu && trava_cache.dono == eu &&
        trava_cache.pid_dono == eu->pid) {
        bool i = int_salva_desliga();
        uint32_t q = pedido_ceder;
        pedido_ceder = 0;
        int_restaura(i);
        if (liberar_limpos(q, false)) est.reclamos++;
    }
    trava_soltar(&trava_cache);
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
        for (uint32_t j = 0; j < c; j++) dados_ler(v[a + j], 0, lote_v + j * BL_BYTES, BL_BYTES, true);
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
    soltar();
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
    soltar();
}

/* ------------------------------------------------------------ asignación */

/* D350: una página para un bloque nuevo: de la alta si la caché va allí (sin bajar de "reserva"),
 * si no de la baja como siempre (con un margen de 8 MB para el kernel). 0 si no hay. */
static uint64_t pagina_nova(bool *alto)
{
    if (com_alta) {
        uint64_t f = memfisica_alocar_alta(reserva);
        if (f) { *alto = true; return f; }
    }
    if (n_baixos < max_baixos && memfisica_livres() > 2048) {
        *alto = false;
        return memfisica_alocar();
    }
    return 0;
}

/* Un bloque nuevo (sin datos válidos), ya en el hash y en la cabeza del LRU. NULL si no hay memoria. */
static bloco_t *novo_bloco(disco_t *d, uint32_t num)
{
    bloco_t *b = NULL;
    if (n_blocos < max_blocos && descr_livres) {
        bool alto = false;
        uint64_t f = pagina_nova(&alto);
        if (f) {
            b = descr_livres;
            descr_livres = b->prox;
            b->prox = b->ant = b->hprox = NULL;
            b->pag = (uint32_t)(f >> 12);
            b->alto = alto;
            b->sujo = false;
            if (alto) n_altos++; else n_baixos++;
            n_blocos++;
        }
    }
    if (!b) {                                       /* expulsar: el limpio menos reciente */
        bloco_t *v = lru_fim;
        for (int i = 0; v && v->sujo && i < 64; i++) v = v->ant;
        if (!v || v->sujo) {
            if (!lru_fim) return NULL;
            vaciar(lru_fim->d, 0, 0);
            v = lru_fim;
            if (!v || v->sujo) return NULL;
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
            dados_ler(k, (ini - s0) * 512u, dest + (ini - lba) * 512u, (fim - ini) * 512u, false);
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
            dados_escrever(nb, 0, lote + j * BL_BYTES, BL_BYTES, true);
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
    soltar();
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
                dados_escrever(k, (ini - s0) * 512u, orig + (ini - lba) * 512u, (fim - ini) * 512u, false);
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
            /* D350: el relleno de un bloque a medio escribir se lee ANTES de crear el bloque (el DMA,
             * a la baja: "lote"). Si se creara antes, el bloque estaría en el hash con datos ajenos
             * mientras el driver duerme y, si matan al proceso ahí (el cerrojo se roba al muerto),
             * quedaría así: lecturas con basura y, al grabarlo más tarde, basura en el disco.
             * novo_bloco() puede vaciar, pero lo hace desde "lote_v": "lote" no se toca. */
            if (!inteiro && r->ler(r, s0, BL_SETORES, lote) < 0) { res = ERRO_ACESSO; break; }
            k = novo_bloco(r, b);
            if (k && !inteiro) dados_escrever(k, 0, lote, BL_BYTES, true);
            if (!k) {                                       /* sin memoria: directo */
                res = driver_escrever(r, ini, fim - ini, orig + (ini - lba) * 512u) < 0 ? ERRO_ACESSO : 0;
                continue;
            }
        } else tocar(k);
        dados_escrever(k, (ini - s0) * 512u, orig + (ini - lba) * 512u, (fim - ini) * 512u, false);
        marcar_sujo(k);
    }
    if (res == 0 && n_sujos > max_sujos) vaciar(r, 0, 0);
fim:
    soltar();
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
        if (!n_sujos && !pedido_ceder && !(com_alta && n_altos && memfisica_livres_alta() < reserva)) continue;
        trava_pegar(&trava_cache);
        uint32_t agora = relogio_tiques();
        bool velho = false;
        for (bloco_t *b = lru_ini; b; b = b->prox)
            if (b->sujo && (int32_t)(agora - b->t_sujo) >= (int32_t)envelhece_tiques) { velho = true; break; }
        if (velho || n_sujos > max_sujos / 2) vaciar(NULL, 0, 0);
        /* D350: si los programas comieron la reserva de la alta, se les devuelve. */
        if (com_alta && n_altos) {
            uint32_t livres = memfisica_livres_alta();
            if (livres < reserva) {
                uint32_t q = reserva - livres;
                liberar_limpos(q > CEDER_TAREFA ? CEDER_TAREFA : q, true);
            }
        }
        soltar();
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
    e->altos = n_altos; e->reserva = reserva;
}

void cache_disco_registar(void)
{
    cache_disco_est_t e;
    cache_disco_estatisticas(&e);
    klog("[CACHE] bloques %u/%u, sucios %u, aciertos %u, fallos %u, anticipados %u, grabados %u, vaciados %u, directos %u, errores %u, "
         "altos %u, cedidos %u, reclamos %u (ocupada %u)\n",
         e.blocos, e.max_blocos, e.sujos, e.acertos, e.falhas, e.anticipados, e.gravados, e.vaciados, e.diretos, e.erros,
         e.altos, e.cedidos, e.reclamos, e.reclamos_ocupada);
}
