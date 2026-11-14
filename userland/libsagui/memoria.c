// SPDX-License-Identifier: GPL-3.0-only
/* memoria.c - aloca/libera/realoca para los programas (D209, mejora 094 B).
 *
 * Hasta la D209 era una lista de bloques con búsqueda "el primero que quepa"
 * desde el principio (O(n) por reserva) y el área solo crecía: lo liberado no
 * volvía al sistema hasta que el programa acababa. Ahora hay dos niveles:
 *
 *   - Páginas: el área de datos se reserva por trozos (MEM_RESERVAR: solo
 *     direcciones, sin memoria física) y se reparte en tramos (vao_t) de
 *     páginas seguidas. Un tramo libre puede estar asignado (con páginas
 *     físicas, listo para usar) o devuelto (sin ellas). Al pedir un tramo
 *     devuelto se asignan sus páginas (MEM_ASIGNAR); al liberar, las páginas
 *     vuelven al kernel (MEM_DEVOLVER) salvo una caché pequeña de tramos
 *     asignados (hasta 2 MB, tramos de hasta 1 MB) que ahorra pedirlas y
 *     ponerlas a cero otra vez. Los vecinos libres del mismo estado se juntan.
 *   - Clases de tamaño: hasta LIMITE_PEQ (32 KB) el tamaño se redondea a una
 *     clase (cada una entre 1/8 y 1/4 mayor que la anterior) y cada clase
 *     corta sus objetos de tramos propios (de 1 a 16 páginas). Reservar y
 *     liberar son O(1). Un tramo de clase que queda vacío vuelve al nivel de
 *     páginas (salvo uno pequeño por clase, para no ir y venir del kernel con
 *     un solo bloque que entra y sale).
 *   - Lo de más de LIMITE_PEQ es un tramo propio, alineado a página.
 *
 * Para saber de qué tramo es un puntero hay un mapa de páginas de dos niveles
 * (como el de los directorios de páginas): raiz[dirección >> 22] apunta a una
 * hoja de 1024 entradas, una por página. Los tramos en uso tienen todas sus
 * páginas en el mapa; los libres, la primera y la última (para juntarse).
 * Las cabeceras y las hojas no van junto a los bloques: viven en trozos de
 * metadatos aparte, así que desbordar un bloque no rompe las listas.
 *
 * Lo que se queda asignado sin usar está acotado: un tramo vacío pequeño por
 * clase y como mucho LIVRE_ASSIGNADO_MAX páginas en tramos libres; el resto
 * vuelve al kernel. Los mapas (mapear_memoria) vuelven siempre al quitarlos.
 *
 * Los programas que llaman a aumentar_memoria por su cuenta (GNU Prolog con su
 * sbrk, testes/memoria) siguen funcionando: el área no tiene que ser continua.
 *
 * D167: los hilos de un proceso comparten esta área. Un mutex la protege; sin
 * hilos nunca se espera (tomar y soltar son un lock cmpxchg y un xchg). */
#include "sagui.h"

#define PAG          4096u
#define PAG_DESL     12
#define LIMITE_PEQ   (32u * 1024)
#define RESERVA_MIN  (4u * 1024 * 1024)     /* direcciones que se reservan de golpe */
#define META_TROZO   (16u * 1024)           /* metadatos: cabeceras y hojas del mapa */
#define LIVRE_ASSIGNADO_MAX 512u            /* páginas libres que se quedan asignadas (2 MB) */
#define LIVRE_ASSIGNADO_TRAMO 256u          /* un tramo libre mayor que esto (1 MB) se devuelve siempre */
#define VAZIO_CACHE_MAX 4u                  /* un tramo de clase vacío de hasta 4 páginas se queda */
#define BINS         128u                   /* listas de tramos libres por número de páginas */

enum { V_LIVRE = 0, V_PEQ = 1, V_GRANDE = 2, V_MAPA = 3 };

typedef struct objeto { struct objeto *prox; } objeto_t;

typedef struct vao {
    uint32_t pag;            /* primera página (dirección >> 12) */
    uint32_t n;              /* páginas */
    uint8_t  estado;         /* V_* */
    uint8_t  assignado;      /* 1 = sus páginas tienen memoria física (los en uso, siempre) */
    uint16_t classe;         /* V_PEQ: la clase */
    uint32_t usados;         /* V_PEQ: objetos en uso */
    objeto_t *livres;        /* V_PEQ: objetos liberados */
    uint8_t  *corte, *fim;   /* V_PEQ: lo que aún no se ha cortado nunca */
    struct vao *prox, *ant;  /* lista de su clase (con sitio) o de libres */
} vao_t;

/* Clases: múltiplos de 8, cada una entre 1/8 y 1/4 mayor que la anterior. */
static const uint16_t classes[] = {
    8, 16, 24, 32, 48, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512,
    640, 768, 896, 1024, 1280, 1536, 1792, 2048, 2560, 3072, 3584, 4096, 5120, 6144,
    7168, 8192, 10240, 12288, 14336, 16384, 20480, 24576, 28672, 32768u
};
#define N_CLASSES (sizeof classes / sizeof classes[0])

static vao_t   *com_sitio[N_CLASSES];   /* tramos de cada clase con objetos libres */
static uint8_t  classe_de_8[LIMITE_PEQ / 8 + 1];   /* (n + 7) / 8 -> clase */
static uint16_t paginas_classe[N_CLASSES];
static vao_t   *livres[BINS + 1];        /* devueltos: [k] = de k páginas; [BINS] = más */
static vao_t   *cache;                   /* libres asignados (pocos: LIVRE_ASSIGNADO_MAX páginas) */
static vao_t  **raiz[1024];              /* mapa de páginas: dirección >> 22 -> hoja */
static vao_t   *cabecas_livres;          /* cabeceras para reutilizar */
static uint8_t *meta, *meta_fim;
static uint32_t paginas_assignadas;      /* páginas con memoria física que tiene el reservador */
static uint32_t livres_assignadas;       /* de ellas, en tramos libres */
static bool     iniciado;

static mutex_t trava = MUTEX_INICIAL;

/* Bytes con memoria física que tiene el reservador (para medir fugas, D118). */
unsigned memoria_total(void) { return paginas_assignadas * PAG; }

static bool erro(int r) { return r < 0 && r > -100; }   /* las direcciones altas también son < 0 */

/* ---- metadatos ---- */

static void *meta_cortar(uint32_t n)
{
    n = (n + 7) & ~7u;
    if (!meta || (uint32_t)(meta_fim - meta) < n) {
        uint32_t t = n > META_TROZO ? (n + PAG - 1) & ~(PAG - 1) : META_TROZO;
        int r = memoria_paginas(MEM_RESERVAR, 0, t);
        if (erro(r)) return NULL;
        if (memoria_paginas(MEM_ASIGNAR, (void *)r, t) < 0) return NULL;   /* la reserva queda sin usar */
        paginas_assignadas += t / PAG;
        meta = (uint8_t *)r;
        meta_fim = meta + t;
    }
    void *p = meta;
    meta += n;
    return p;
}

static vao_t *cabeca_nova(void)
{
    vao_t *v = cabecas_livres;
    if (v) cabecas_livres = v->prox;
    else v = meta_cortar(sizeof(vao_t));
    if (v) memset(v, 0, sizeof *v);
    return v;
}

static void cabeca_soltar(vao_t *v) { v->prox = cabecas_livres; cabecas_livres = v; }

/* ---- mapa de páginas ---- */

static vao_t *mapa_ler(uint32_t pag)
{
    vao_t **h = raiz[(pag >> 10) & 1023];
    return h ? h[pag & 1023] : NULL;
}

static bool mapa_por(uint32_t pag, vao_t *v)
{
    vao_t **h = raiz[(pag >> 10) & 1023];
    if (!h) {
        h = meta_cortar(1024 * sizeof(vao_t *));   /* los metadatos vienen a cero del kernel */
        if (!h) return false;
        raiz[(pag >> 10) & 1023] = h;
    }
    h[pag & 1023] = v;
    return true;
}

/* Las hojas que hacen falta ya existen: crescer() las crea para todo el
 * tramo reservado, así que poner un tramo en el mapa nunca falla. */
static void mapa_bordes(vao_t *v)
{
    mapa_por(v->pag, v);
    mapa_por(v->pag + v->n - 1, v);
}

static void mapa_todo(vao_t *v)
{
    for (uint32_t i = 0; i < v->n; i++) mapa_por(v->pag + i, v);
}

/* ---- listas ---- */

static void lista_por(vao_t **l, vao_t *v)
{
    v->ant = NULL;
    v->prox = *l;
    if (*l) (*l)->ant = v;
    *l = v;
}

static void lista_tirar(vao_t **l, vao_t *v)
{
    if (v->ant) v->ant->prox = v->prox;
    else *l = v->prox;
    if (v->prox) v->prox->ant = v->ant;
    v->prox = v->ant = NULL;
}

static vao_t **lista_de(const vao_t *v) { return v->assignado ? &cache : &livres[v->n < BINS ? v->n : BINS]; }

static void livre_tirar(vao_t *v)
{
    lista_tirar(lista_de(v), v);
    if (v->assignado) livres_assignadas -= v->n;
}

/* Un tramo libre nuevo (el resto de otro), sin juntarlo con nadie. */
static void livre_resto(vao_t *r)
{
    r->estado = V_LIVRE;
    if (r->assignado) livres_assignadas += r->n;
    mapa_bordes(r);
    lista_por(lista_de(r), r);
}

/* ---- nivel de páginas ---- */

static void devolver(vao_t *v)
{
    if (!v->assignado) return;
    memoria_paginas(MEM_DEVOLVER, (void *)(v->pag << PAG_DESL), v->n << PAG_DESL);
    paginas_assignadas -= v->n;
    v->assignado = 0;
}

/* Pone un tramo en las listas de libres, juntándolo con los vecinos libres
 * del mismo estado (asignado con asignado, devuelto con devuelto). Un tramo
 * asignado que pasa de LIVRE_ASSIGNADO_TRAMO, o que no cabe en el límite de la
 * caché, se devuelve, y entonces se junta con los vecinos devueltos. */
static vao_t *juntar(vao_t *v)
{
    vao_t *a = mapa_ler(v->pag - 1);
    if (a && a->estado == V_LIVRE && a->pag + a->n == v->pag && a->assignado == v->assignado) {
        livre_tirar(a);
        a->n += v->n;
        cabeca_soltar(v);
        v = a;
    }
    vao_t *d = mapa_ler(v->pag + v->n);
    if (d && d->estado == V_LIVRE && d->pag == v->pag + v->n && d->assignado == v->assignado) {
        livre_tirar(d);
        v->n += d->n;
        cabeca_soltar(d);
    }
    return v;
}

static void livre_por(vao_t *v)
{
    v->estado = V_LIVRE;
    if (v->assignado && livres_assignadas + v->n > LIVRE_ASSIGNADO_MAX) devolver(v);
    v = juntar(v);
    if (v->assignado && v->n > LIVRE_ASSIGNADO_TRAMO) {
        devolver(v);
        v = juntar(v);
    }
    livre_resto(v);
}

/* Reserva direcciones nuevas para al menos n páginas. */
static bool crescer(uint32_t n)
{
    if (n > 0x7FFFFu) return false;
    uint32_t bytes = n << PAG_DESL;
    if (bytes < RESERVA_MIN) bytes = RESERVA_MIN;
    int r = memoria_paginas(MEM_RESERVAR, 0, bytes);
    if (erro(r)) {
        bytes = n << PAG_DESL;   /* queda poco sitio: solo lo justo */
        r = memoria_paginas(MEM_RESERVAR, 0, bytes);
        if (erro(r)) return false;
    }
    uint32_t pag = (uint32_t)r >> PAG_DESL, fim = pag + (bytes >> PAG_DESL);
    /* Las hojas del mapa de todo el tramo, ahora (ver mapa_bordes). Si faltan
     * metadatos, las direcciones se pierden, pero no hay nada a medias. */
    for (uint32_t p = pag; p < fim; p = (p | 1023) + 1)
        if (!mapa_por(p, NULL)) return false;
    vao_t *v = cabeca_nova();
    if (!v) return false;
    v->pag = pag;
    v->n = fim - pag;
    livre_por(v);
    return true;
}

/* Busca un tramo libre de al menos n páginas: primero en la caché de los
 * asignados (no hay que pedir páginas ni ponerlas a cero), el más pequeño que
 * baste; si no, entre los devueltos, el de tamaño exacto o el más pequeño. */
static vao_t *procurar(uint32_t n)
{
    vao_t *melhor = NULL;
    for (vao_t *v = cache; v; v = v->prox)
        if (v->n >= n && (!melhor || v->n < melhor->n)) melhor = v;
    if (melhor) return melhor;
    for (uint32_t k = n; k < BINS; k++)
        if (livres[k]) return livres[k];
    for (vao_t *v = livres[BINS]; v; v = v->prox)
        if (v->n >= n && (!melhor || v->n < melhor->n)) melhor = v;
    return melhor;
}

/* Separa las primeras n páginas de un tramo libre (ya fuera de las listas);
 * el resto sigue libre, en el mismo estado. */
static bool separar(vao_t *v, uint32_t n)
{
    if (v->n == n) return true;
    vao_t *r = cabeca_nova();
    if (!r) return false;
    r->pag = v->pag + n;
    r->n = v->n - n;
    r->assignado = v->assignado;
    v->n = n;
    livre_resto(r);
    return true;
}

/* Tramo de n páginas con memoria física, en uso. *zerado = sus páginas acaban
 * de llegar del kernel (vienen a cero). */
static vao_t *paginas_pedir(uint32_t n, uint8_t estado, bool *zerado)
{
    vao_t *v = procurar(n);
    if (!v) {
        if (!crescer(n)) return NULL;
        v = procurar(n);
        if (!v) return NULL;
    }
    livre_tirar(v);
    if (!separar(v, n)) { livre_por(v); return NULL; }
    bool novo = !v->assignado;
    if (novo) {
        if (memoria_paginas(MEM_ASIGNAR, (void *)(v->pag << PAG_DESL), n << PAG_DESL) < 0) {
            /* Sin memoria: lo que llegara a asignarse se devuelve. */
            memoria_paginas(MEM_DEVOLVER, (void *)(v->pag << PAG_DESL), n << PAG_DESL);
            livre_por(v);
            return NULL;
        }
        paginas_assignadas += n;
        v->assignado = 1;
    }
    if (zerado) *zerado = novo;
    v->estado = estado;
    mapa_todo(v);
    return v;
}

static void paginas_soltar(vao_t *v)
{
    /* Un mapa vuelve al kernel enseguida (es lo que se espera de munmap); lo
     * demás se queda asignado si cabe en la caché (livre_por decide). */
    if (v->estado == V_MAPA) devolver(v);
    livre_por(v);
}

/* ---- clases ---- */

static void iniciar(void)
{
    uint32_t c = 0;
    for (uint32_t i = 0; i <= LIMITE_PEQ / 8; i++) {
        while (classes[c] < i * 8) c++;
        classe_de_8[i] = (uint8_t)c;
    }
    /* Tramos de unos 8 objetos, entre 1 y 16 páginas. */
    for (uint32_t k = 0; k < N_CLASSES; k++) {
        uint32_t alvo = 8u * classes[k];
        if (alvo > 64u * 1024) alvo = 64u * 1024;
        if (alvo < PAG) alvo = PAG;
        paginas_classe[k] = (uint16_t)((alvo + PAG - 1) / PAG);
    }
    iniciado = true;
}

static void *peq_aloca(uint32_t n, bool *zerado)
{
    uint32_t c = classe_de_8[(n + 7) / 8];
    vao_t *v = com_sitio[c];
    bool novo = false;
    if (!v) {
        v = paginas_pedir(paginas_classe[c], V_PEQ, &novo);
        if (!v) return NULL;
        v->classe = (uint16_t)c;
        v->usados = 0;
        v->livres = NULL;
        v->corte = (uint8_t *)(v->pag << PAG_DESL);
        v->fim = v->corte + (v->n << PAG_DESL) / classes[c] * classes[c];
        lista_por(&com_sitio[c], v);
    }
    void *p;
    if (v->livres) {
        p = v->livres;
        v->livres = v->livres->prox;
        novo = false;
    } else {
        p = v->corte;
        v->corte += classes[c];
    }
    if (zerado) *zerado = novo;
    v->usados++;
    if (!v->livres && v->corte >= v->fim) lista_tirar(&com_sitio[c], v);   /* lleno */
    return p;
}

static void peq_libera(vao_t *v, void *p)
{
    uint32_t c = v->classe;
    bool estava_cheio = !v->livres && v->corte >= v->fim;
    objeto_t *o = p;
    o->prox = v->livres;
    v->livres = o;
    v->usados--;
    if (estava_cheio) lista_por(&com_sitio[c], v);
    if (v->usados == 0) {
        /* Vacío: vuelve al nivel de páginas, salvo si es el único con sitio de
         * su clase y es pequeño (el próximo bloque de la clase lo usaría). */
        if (com_sitio[c] == v && !v->prox && v->n <= VAZIO_CACHE_MAX) return;
        lista_tirar(&com_sitio[c], v);
        paginas_soltar(v);
    }
}

/* El tramo de un puntero que salió de aquí, o NULL. */
static vao_t *vao_de(const void *p)
{
    uint32_t e = (uint32_t)p, pag = e >> PAG_DESL;
    vao_t *v = mapa_ler(pag);
    if (!v || v->estado == V_LIVRE || pag < v->pag || pag >= v->pag + v->n) return NULL;
    if (v->estado == V_PEQ) {
        uint32_t desl = e - (v->pag << PAG_DESL);
        if (desl % classes[v->classe] || (const uint8_t *)p >= v->corte) return NULL;
    } else if (e != v->pag << PAG_DESL) return NULL;
    return v;
}

/* ---- la interfaz ---- */

static void *aloca_sem_trava(size_t n, bool *zerado)
{
    if (!iniciado) iniciar();
    if (n == 0) n = 1;
    if (n <= LIMITE_PEQ) return peq_aloca((uint32_t)n, zerado);
    if (n > 0x7FFFF000u) return NULL;
    vao_t *v = paginas_pedir((uint32_t)((n + PAG - 1) >> PAG_DESL), V_GRANDE, zerado);
    return v ? (void *)(v->pag << PAG_DESL) : NULL;
}

static void libera_sem_trava(void *p)
{
    vao_t *v = vao_de(p);
    if (!v || v->estado == V_MAPA) return;   /* no salió de aloca: se ignora */
    if (v->estado == V_PEQ) peq_libera(v, p);
    else paginas_soltar(v);
}

void *aloca(size_t n)
{
    mutex_tomar(&trava);
    void *p = aloca_sem_trava(n, NULL);
    mutex_largar(&trava);
    return p;
}

void *aloca_zerado(size_t n)
{
    bool zerado = false;
    mutex_tomar(&trava);
    void *p = aloca_sem_trava(n, &zerado);
    mutex_largar(&trava);
    if (p && !zerado) memset(p, 0, n);
    return p;
}

void libera(void *p)
{
    if (!p) return;
    mutex_tomar(&trava);
    libera_sem_trava(p);
    mutex_largar(&trava);
}

/* Un bloque grande crece en su sitio si el tramo siguiente está libre y basta. */
static bool grande_crescer(vao_t *v, uint32_t n)
{
    vao_t *d = mapa_ler(v->pag + v->n);
    uint32_t falta = n - v->n;
    if (!d || d->estado != V_LIVRE || d->pag != v->pag + v->n || d->n < falta) return false;
    livre_tirar(d);
    if (!separar(d, falta)) { livre_por(d); return false; }
    if (!d->assignado) {
        if (memoria_paginas(MEM_ASIGNAR, (void *)(d->pag << PAG_DESL), falta << PAG_DESL) < 0) {
            memoria_paginas(MEM_DEVOLVER, (void *)(d->pag << PAG_DESL), falta << PAG_DESL);
            livre_por(d);
            return false;
        }
        paginas_assignadas += falta;
    }
    cabeca_soltar(d);
    v->n = n;
    mapa_todo(v);
    return true;
}

/* Un bloque grande que encoge devuelve las páginas que le sobran. */
static void grande_encolher(vao_t *v, uint32_t n)
{
    vao_t *r = cabeca_nova();
    if (!r) return;   /* sin cabecera: se queda como está */
    r->pag = v->pag + n;
    r->n = v->n - n;
    r->assignado = 1;
    r->estado = V_GRANDE;
    v->n = n;
    mapa_bordes(v);
    paginas_soltar(r);
}

void *realoca(void *p, size_t n)
{
    if (!p) return aloca(n);
    if (n == 0) n = 1;
    mutex_tomar(&trava);
    vao_t *v = vao_de(p);
    if (!v || v->estado == V_MAPA) { mutex_largar(&trava); return NULL; }
    uint32_t atual = v->estado == V_PEQ ? classes[v->classe] : v->n << PAG_DESL;
    if (v->estado == V_GRANDE && n > LIMITE_PEQ) {
        uint32_t pags = (uint32_t)((n + PAG - 1) >> PAG_DESL);
        if (pags <= v->n) {
            /* Solo se devuelve si sobra al menos un cuarto (evita ir y venir). */
            if (v->n - pags >= v->n / 4 && pags < v->n) grande_encolher(v, pags);
            mutex_largar(&trava);
            return p;
        }
        if (grande_crescer(v, pags)) { mutex_largar(&trava); return p; }
    } else if (v->estado == V_PEQ && n <= atual && (n * 2 > atual || atual <= 16)) {
        mutex_largar(&trava);   /* cabe y no desperdicia más de la mitad */
        return p;
    }
    void *novo = aloca_sem_trava(n, NULL);
    if (novo) {
        memcpy(novo, p, n < atual ? n : atual);
        libera_sem_trava(p);
    }
    mutex_largar(&trava);
    return novo;
}

/* Bytes utilizables del bloque p (0 si no salió de aloca/realoca). La libc
 * común lo usa para malloc_usable_size y para avisar de un free() de un
 * puntero desconocido. */
size_t memoria_tamanho(const void *p)
{
    if (!p) return 0;
    mutex_tomar(&trava);
    vao_t *v = vao_de(p);
    size_t t = !v || v->estado == V_MAPA ? 0 : v->estado == V_PEQ ? classes[v->classe] : (size_t)v->n << PAG_DESL;
    mutex_largar(&trava);
    return t;
}

/* Bloque de n bytes alineado a "alin" (potencia de 2), que se libera con
 * libera(). Hasta 4 KB se elige una clase cuyo tamaño sea múltiplo de alin
 * (los tramos empiezan en página, así que todos sus objetos quedan
 * alineados); más, un tramo propio con lo que sobra delante devuelto. */
void *aloca_alinhado(size_t alin, size_t n)
{
    if (!alin || (alin & (alin - 1))) return NULL;
    if (alin <= 8) return aloca(n);
    if (n == 0) n = 1;
    void *p = NULL;
    mutex_tomar(&trava);
    if (!iniciado) iniciar();
    if (alin <= PAG && n <= LIMITE_PEQ) {
        for (uint32_t c = classe_de_8[(n + 7) / 8]; c < N_CLASSES; c++)
            if (classes[c] % alin == 0) {
                p = peq_aloca(classes[c], NULL);
                break;
            }
    }
    if (!p && n <= 0x7FFFF000u - alin) {
        uint32_t pags = (uint32_t)((n + PAG - 1) >> PAG_DESL);
        uint32_t extra = alin > PAG ? (uint32_t)(alin >> PAG_DESL) - 1 : 0;
        vao_t *v = paginas_pedir(pags + extra, V_GRANDE, NULL);
        if (v) {
            uint32_t mascara = (uint32_t)(alin >> PAG_DESL) - 1;
            uint32_t salto = alin > PAG ? ((v->pag + mascara) & ~mascara) - v->pag : 0;
            vao_t *a = salto ? cabeca_nova() : NULL;
            if (salto && !a) {
                paginas_soltar(v);
            } else {
                if (a) {   /* lo de delante vuelve a estar libre */
                    a->pag = v->pag; a->n = salto; a->assignado = 1; a->estado = V_GRANDE;
                    v->pag += salto; v->n -= salto;
                    mapa_bordes(v);
                    paginas_soltar(a);
                }
                if (v->n > pags) grande_encolher(v, pags);
                p = (void *)(v->pag << PAG_DESL);
            }
        }
    }
    mutex_largar(&trava);
    return p;
}

/* ---- mapas de memoria (D209) ----
 * mapear_memoria(n): páginas a cero, alineadas, en un tramo propio.
 * desmapear_memoria(p, n): devuelve [p, p+n) (páginas enteras) de un mapa; el
 * resto del mapa sigue valiendo (como munmap, también un trozo del medio). */
void *mapear_memoria(size_t n)
{
    if (n == 0 || n > 0x7FFFF000u) return NULL;
    bool zerado = false;
    mutex_tomar(&trava);
    if (!iniciado) iniciar();
    vao_t *v = paginas_pedir((uint32_t)((n + PAG - 1) >> PAG_DESL), V_MAPA, &zerado);
    void *p = v ? (void *)(v->pag << PAG_DESL) : NULL;
    mutex_largar(&trava);
    if (p && !zerado) memset(p, 0, (n + PAG - 1) & ~(size_t)(PAG - 1));
    return p;
}

int desmapear_memoria(void *p, size_t n)
{
    uint32_t ini = (uint32_t)p;
    if ((ini & (PAG - 1)) || n == 0 || n > 0x7FFFF000u) return ERRO_INVALIDO;
    uint32_t pag = ini >> PAG_DESL, pags = (uint32_t)((n + PAG - 1) >> PAG_DESL);
    mutex_tomar(&trava);
    vao_t *v = mapa_ler(pag);
    if (!v || v->estado != V_MAPA || pag < v->pag || pag + pags > v->pag + v->n) {
        mutex_largar(&trava);
        return ERRO_INVALIDO;
    }
    /* Hasta tres trozos: lo de antes y lo de después siguen siendo mapas. */
    vao_t *a = pag > v->pag ? cabeca_nova() : NULL;
    vao_t *d = pag + pags < v->pag + v->n ? cabeca_nova() : NULL;
    if ((pag > v->pag && !a) || (pag + pags < v->pag + v->n && !d)) {
        if (a) cabeca_soltar(a);
        if (d) cabeca_soltar(d);
        mutex_largar(&trava);
        return ERRO_SEM_MEMORIA;
    }
    if (a) {
        a->pag = v->pag; a->n = pag - v->pag; a->estado = V_MAPA; a->assignado = 1;
        v->pag = pag; v->n -= a->n;
        mapa_todo(a);
    }
    if (d) {
        d->pag = v->pag + pags; d->n = v->n - pags; d->estado = V_MAPA; d->assignado = 1;
        v->n = pags;
        mapa_todo(d);
    }
    paginas_soltar(v);
    mutex_largar(&trava);
    return 0;
}

/* Un fichero en memoria: se lee en un mapa nuevo (copia privada: no comparte
 * páginas con la caché del disco y lo escrito no vuelve al fichero). */
void *mapear_arquivo(int fd, unsigned desloc, size_t n)
{
    uint8_t *p = mapear_memoria(n);
    if (!p) return NULL;
    if (posicionar(fd, (int)desloc, 0) < 0) { desmapear_memoria(p, n); return NULL; }
    size_t lido = 0;
    while (lido < n) {
        int r = ler(fd, p + lido, (unsigned)(n - lido > 0x100000 ? 0x100000 : n - lido));
        if (r < 0) { desmapear_memoria(p, n); return NULL; }
        if (r == 0) break;   /* pasado el final: el resto queda a cero, como en POSIX */
        lido += (size_t)r;
    }
    return p;
}
