// SPDX-License-Identifier: GPL-3.0-only
/* troca.c - swap comprimida en RAM (D352). Ver troca.h y docs/decisoes.md (D352).
 *
 * El almacén. Páginas físicas (de la alta o la superior si las hay; de la baja si no,
 * sin bajar de la mitad de la reserva baja del kernel) partidas en 64 bloques de 64 bytes. Cada
 * página sacada se guarda comprimida en bloques seguidos de UNA página del almacén
 * (nunca cruza: se copia con una sola ventana): una cabecera de 4 bytes (longitud y
 * suma de la página original) y los datos. Las páginas que no bajan de 3072 bytes
 * (un 25 % de ahorro) no se sacan. Cada página del almacén tiene un descriptor de 32
 * bytes en la baja (en páginas de 128 descriptores que se piden según hacen falta):
 * su página física, el mapa de bloques ocupados y el de bloques que empiezan un trozo
 * (así se sabe la longitud de un trozo sin leerlo). Las páginas con hueco están en
 * listas por número de bloques libres; un trozo nuevo va a la más llena en la que
 * quepa, y una página que se vacía vuelve al sistema.
 *
 * Sacar (troca_liberar). Un reloj de segunda oportunidad recorre las entradas de las
 * tablas de los procesos (la mano: proceso y dirección). Una entrada con A puesta se
 * queda (se le quita A); sin A, y si no es prestada (ventanas, BAR) ni la palabra de
 * un futex que alguien espera (la clave del futex es la dirección física), se saca.
 * Por lotes de hasta 16 páginas de un mismo proceso, con las interrupciones apagadas:
 *   1. cada entrada pasa a 0 con un xchg (los A/D que el hardware ponga a la vez no se
 *      pierden: con P=0 ya no pone nada), invlpg si el espacio es el de esta CPU y
 *      espaco_sincronizar(): ninguna otra CPU sigue escribiendo por su TLB;
 *   2. la página se copia a un búfer (ventana), se comprime y el trozo se escribe en el
 *      almacén; la entrada pasa a ser de swap y la página física se libera. Si no
 *      comprime, la entrada vuelve como estaba con PG_INCOMPRIMIVEL y D a 0 (no se
 *      vuelve a intentar hasta que se escriba). Las páginas a cero no ocupan nada.
 * Nadie ve la entrada a 0: el kernel tiene el cerrojo general (D165) y las interrupciones
 * apagadas, y las otras CPU que corrían el proceso esperan el cerrojo (espaco_sincronizar).
 * Si el almacén no puede crecer (tope, o sin memoria), la propia página sacada pasa a
 * ser del almacén: así sacar nunca necesita memoria.
 *
 * Traer (troca_trazer). Desde el fallo de página (del programa o del kernel tocando
 * memoria del programa en una llamada al sistema) y desde espaco_traduzir(). No duerme:
 * pide una página (que puede sacar otras: búferes distintos), descomprime, comprueba la
 * suma, mapea con A puesta y suelta el trozo. Con las interrupciones apagadas. Una
 * entrada no presente no está en ninguna TLB, así que añadirla no necesita avisar a las
 * demás CPU; si dos hilos fallan a la vez en la misma página, el segundo (con el
 * cerrojo general, después) la encuentra presente y repite la instrucción. */
#include "troca.h"
#include "compressor.h"
#include "memoria.h"
#include "memalta.h"
#include "processo.h"
#include "bootinfo.h"
#include "kconf.h"
#include "console.h"
#include "cpu.h"
#include "relogio.h"
#include "texto.h"

#define BLOCO_BYTES   64u
#define BLOCOS_PAG    64u
#define CAB_TROZO     4u
#define MAX_TROZO     3072u                    /* cabecera incluida: menos de un 25 % de ahorro no compensa */
#define LOTE          16u                      /* páginas que se sacan entre dos sincronizaciones */
#define DESC_POR_PAG  (PAGINA / sizeof(desc_t))
#define TETO_MAX      (1u << 18)               /* 1 GB de almacén como mucho */
#define DIR_MAX       (TETO_MAX / 128u)        /* páginas de descriptores */
#define BUSCA_MAX     24u                      /* páginas con hueco que se miran antes de abrir otra */
#define EXAMINAR_LOTE 2048u                    /* entradas que mira un lote como mucho (interrupciones apagadas) */
#define NENHUM        0xFFFFFFFFu

typedef struct {
    uint32_t pfn;         /* página física del almacén; 0 = descriptor libre */
    uint32_t mapa[2];     /* bloque ocupado */
    uint32_t inicio[2];   /* bloque que empieza un trozo */
    uint32_t prox, ant;   /* lista de su cubo (índices; NENHUM = fin) */
    uint8_t  livres;      /* bloques libres */
    uint8_t  cubo;        /* cubo en el que está (0xFF = ninguno: llena) */
    uint8_t  alta;        /* la página es de la alta o la superior */
    uint8_t  reservado;
} desc_t;                 /* 32 bytes: 128 por página */

static desc_t  *diretorio[DIR_MAX];
static uint32_t n_desc;                        /* índices usados alguna vez */
static uint32_t livre_desc = NENHUM;           /* descriptores libres (por "prox") */
static uint32_t cubos[BLOCOS_PAG];             /* cabeza de cada cubo (1..63 libres) */
static uint32_t teto;                          /* páginas de almacén como mucho (0 = sin swap) */
static uint32_t reserva_baixa;                 /* páginas bajas que el almacén no toca */
static uint32_t umbral, alvo;                  /* la tarea: libres de los programas */
static troca_est_t est;

/* Búferes en la baja: los de sacar y los de traer son distintos, porque traer pide
 * una página y eso puede sacar otras. */
static uint8_t buf_pag[PAGINA] __attribute__((aligned(16)));
static uint8_t buf_comp[MAX_TROZO + 64] __attribute__((aligned(16)));
static uint8_t buf_trozo[MAX_TROZO + 64] __attribute__((aligned(16)));
static uint8_t buf_volta[PAGINA] __attribute__((aligned(16)));

/* La mano del reloj. */
static int      mao_proc = 1;
static uint32_t mao_virt = USUARIO_INICIO;

bool troca_ativa(void) { return teto != 0; }

/* ------------------------------------------------------------ bits de 64 */

static inline bool bit(const uint32_t *m, uint32_t i) { return (m[i >> 5] >> (i & 31)) & 1u; }
static inline void bit_por(uint32_t *m, uint32_t i)   { m[i >> 5] |= 1u << (i & 31); }
static inline void bit_tirar(uint32_t *m, uint32_t i) { m[i >> 5] &= ~(1u << (i & 31)); }

/* Primer hueco de k bloques seguidos, o -1. */
static int achar_corrida(const uint32_t *mapa, uint32_t k)
{
    uint32_t corrida = 0;
    for (uint32_t i = 0; i < BLOCOS_PAG; i++) {
        if (bit(mapa, i)) corrida = 0;
        else if (++corrida == k) return (int)(i + 1 - k);
    }
    return -1;
}

/* ------------------------------------------------------------ descriptores */

static inline desc_t *desc(uint32_t i) { return &diretorio[i / DESC_POR_PAG][i % DESC_POR_PAG]; }

static uint32_t desc_novo(void)
{
    if (livre_desc != NENHUM) {
        uint32_t i = livre_desc;
        livre_desc = desc(i)->prox;
        return i;
    }
    if (n_desc >= teto || n_desc >= (1u << (32 - TROCA_DESL_IDX))) return NENHUM;
    uint32_t d = n_desc / DESC_POR_PAG;
    if (d >= DIR_MAX) return NENHUM;
    if (!diretorio[d]) {
        uint32_t p = memfisica_alocar();   /* baja: el kernel la ve por su dirección */
        if (!p) return NENHUM;
        memset((void *)p, 0, PAGINA);
        diretorio[d] = (desc_t *)p;
    }
    return n_desc++;
}

static void cubo_tirar(uint32_t i)
{
    desc_t *d = desc(i);
    if (d->cubo == 0xFF) return;
    if (d->ant != NENHUM) desc(d->ant)->prox = d->prox;
    else cubos[d->cubo] = d->prox;
    if (d->prox != NENHUM) desc(d->prox)->ant = d->ant;
    d->cubo = 0xFF;
}

static void cubo_por(uint32_t i)
{
    desc_t *d = desc(i);
    if (d->livres == 0 || d->livres >= BLOCOS_PAG) return;
    d->cubo = d->livres;
    d->ant = NENHUM;
    d->prox = cubos[d->livres];
    if (d->prox != NENHUM) desc(d->prox)->ant = i;
    cubos[d->livres] = i;
}

/* Una página nueva para el almacén: alta o superior; si no, baja por encima de la
 * reserva; si no, la que se está sacando (vitima, si la hay). */
static uint32_t pagina_nova(fis_t vitima, bool *usou_vitima)
{
    *usou_vitima = false;
    if (est.armazem_paginas >= teto) return NENHUM;
    uint32_t i = desc_novo();
    if (i == NENHUM) return NENHUM;
    fis_t f = memfisica_alocar_alta(0);
    if (!f && memfisica_livres() > reserva_baixa / 2) f = memfisica_alocar();
    if (!f && vitima) { f = vitima; *usou_vitima = true; }
    if (!f) {
        desc(i)->pfn = 0;
        desc(i)->prox = livre_desc;
        livre_desc = i;
        return NENHUM;
    }
    desc_t *d = desc(i);
    memset(d, 0, sizeof *d);
    d->pfn = (uint32_t)(f / PAGINA);
    d->livres = BLOCOS_PAG;
    d->cubo = 0xFF;
    d->alta = f >= MEMFIS_LIMITE;
    est.armazem_paginas++;
    if (d->alta) est.armazem_altas++;
    return i;
}

static void pagina_soltar(uint32_t i)
{
    desc_t *d = desc(i);
    cubo_tirar(i);
    memfisica_liberar((fis_t)d->pfn * PAGINA);
    est.armazem_paginas--;
    if (d->alta) est.armazem_altas--;
    d->pfn = 0;
    d->prox = livre_desc;
    livre_desc = i;
}

/* Sitio para un trozo de k bloques: devuelve el índice y la posición. */
static bool trozo_alocar(uint32_t k, fis_t vitima, uint32_t *idx, uint32_t *pos, bool *usou_vitima)
{
    *usou_vitima = false;
    uint32_t mirados = 0;
    for (uint32_t c = k; c < BLOCOS_PAG && mirados < BUSCA_MAX; c++) {
        for (uint32_t i = cubos[c]; i != NENHUM && mirados < BUSCA_MAX; i = desc(i)->prox, mirados++) {
            int p = achar_corrida(desc(i)->mapa, k);
            if (p >= 0) { *idx = i; *pos = (uint32_t)p; return true; }
        }
    }
    uint32_t i = pagina_nova(vitima, usou_vitima);
    if (i == NENHUM) return false;
    *idx = i;
    *pos = 0;
    return true;
}

static void trozo_ocupar(uint32_t i, uint32_t pos, uint32_t k)
{
    desc_t *d = desc(i);
    cubo_tirar(i);
    for (uint32_t b = pos; b < pos + k; b++) bit_por(d->mapa, b);
    bit_por(d->inicio, pos);
    d->livres = (uint8_t)(d->livres - k);
    cubo_por(i);
    est.blocos += k;
}

/* Bloques del trozo que empieza en "pos": hasta el siguiente inicio o bloque libre. */
static uint32_t trozo_blocos(const desc_t *d, uint32_t pos)
{
    uint32_t n = 1;
    while (pos + n < BLOCOS_PAG && bit(d->mapa, pos + n) && !bit(d->inicio, pos + n)) n++;
    return n;
}

static inline uint32_t entrada_idx(pte_t v) { return (uint32_t)v >> TROCA_DESL_IDX; }
static inline uint32_t entrada_pos(pte_t v) { return ((uint32_t)v >> TROCA_DESL_POS) & (BLOCOS_PAG - 1); }

static bool entrada_valida(pte_t v)
{
    if (!troca_e_entrada(v)) return false;
    if (v & TROCA_ZERO) return true;
    uint32_t i = entrada_idx(v), pos = entrada_pos(v);
    if (i >= n_desc) return false;
    desc_t *d = desc(i);
    return d->pfn && bit(d->inicio, pos) && bit(d->mapa, pos);
}

void troca_soltar(pte_t v)
{
    if (!troca_e_entrada(v)) return;
    bool irq = int_salva_desliga();
    est.paginas--;
    if (v & TROCA_ZERO) {
        est.zeros--;
    } else if (entrada_valida(v)) {
        uint32_t i = entrada_idx(v), pos = entrada_pos(v);
        desc_t *d = desc(i);
        uint32_t k = trozo_blocos(d, pos);
        cubo_tirar(i);
        for (uint32_t b = pos; b < pos + k; b++) bit_tirar(d->mapa, b);
        bit_tirar(d->inicio, pos);
        d->livres = (uint8_t)(d->livres + k);
        est.blocos -= k;
        if (d->livres >= BLOCOS_PAG) pagina_soltar(i);
        else cubo_por(i);
    } else {
        klog("[SWAP] AVISO: entrada de swap invalida al soltarla: %08x\n", (uint32_t)v);
    }
    int_restaura(irq);
}

/* ------------------------------------------------------------ sacar */

static bool pagina_zero(const uint8_t *p)
{
    const uint32_t *w = (const uint32_t *)p;
    for (uint32_t i = 0; i < PAGINA / 4; i++)
        if (w[i]) return false;
    return true;
}

/* ¿Es la palabra de un futex que alguien espera? La clave es la dirección física:
 * si la página cambiara de sitio, quien la despierte no la encontraría. */
static bool e_futex(fis_t fis)
{
    for (int i = 1; i < MAX_PROCESSOS; i++) {
        processo_t *q = processo_indice(i);
        if (q->estado != P_LIVRE && q->futex && (q->futex & ~(uint64_t)(PAGINA - 1)) == fis) return true;
    }
    return false;
}

static bool elegivel(const processo_t *p)
{
    return p && p->pid != 0 && p->usuario && !p->lider && p->espaco && p->espaco != espaco_kernel() &&
           !p->espaco_pendente && (p->estado == P_PRONTO || p->estado == P_EXECUTANDO || p->estado == P_BLOQUEADO);
}

/* Saca una página ya quitada de la tabla ("velho" era su entrada). Devuelve las
 * páginas liberadas (0 o 1); *parar si el almacén no puede crecer. */
static uint32_t sacar(void *pt, pte_t velho, bool *parar)
{
    fis_t fis = pte_endereco(velho);
    uint32_t f = pte_flags(velho);
    uint32_t perm = TROCA_MARCA | ((f & PG_ESCRITA) ? TROCA_ESCRITA : 0) | ((f & PG_NX) ? TROCA_NX : 0);
    fisica_ler(fis, buf_pag, PAGINA);
    if (pagina_zero(buf_pag)) {
        pte_gravar(pt, perm | TROCA_ZERO);
        memfisica_liberar(fis);
        est.paginas++;
        est.zeros++;
        est.saidas++;
        return 1;
    }
    uint32_t n = comp_pagina(buf_pag, buf_comp + CAB_TROZO, MAX_TROZO - CAB_TROZO);
    if (!n) {
        /* No compensa: vuelve como estaba, marcada, con A y D a cero (las TLB ya se
         * vaciaron: el hardware volverá a poner D al primer escribir). */
        pte_gravar(pt, (velho | PG_INCOMPRIMIVEL) & ~(pte_t)(PG_SUJA | PG_ACESSADA));
        est.rejeitadas++;
        return 0;
    }
    uint32_t total = n + CAB_TROZO, k = (total + BLOCO_BYTES - 1) / BLOCO_BYTES;
    uint16_t soma = comp_soma(buf_pag);
    buf_comp[0] = (uint8_t)n;
    buf_comp[1] = (uint8_t)(n >> 8);
    buf_comp[2] = (uint8_t)soma;
    buf_comp[3] = (uint8_t)(soma >> 8);
    uint32_t idx, pos;
    bool usou_vitima;
    if (!trozo_alocar(k, fis, &idx, &pos, &usou_vitima)) {
        pte_gravar(pt, velho);   /* el almacén no crece: se queda */
        *parar = true;
        return 0;
    }
    trozo_ocupar(idx, pos, k);
    fisica_escrever((fis_t)desc(idx)->pfn * PAGINA + pos * BLOCO_BYTES, buf_comp, total);
    pte_gravar(pt, perm | (pos << TROCA_DESL_POS) | (idx << TROCA_DESL_IDX));
    est.paginas++;
    est.saidas++;
    if (usou_vitima) return 0;   /* la página pasó al almacén: no se libera nada */
    memfisica_liberar(fis);
    return 1;
}

/* Un lote: las candidatas de "p" desde la mano. Con las interrupciones apagadas. */
static uint32_t lote(processo_t *p, uint32_t *examinadas, bool *parar)
{
    espaco_t e = p->espaco;
    void *pts[LOTE];
    uint32_t virts[LOTE], n = 0;
    uint32_t bloco = paginacao_bloco(), limite = *examinadas + EXAMINAR_LOTE;
    while (n < LOTE && mao_virt >= USUARIO_INICIO && mao_virt < USUARIO_FIM && *examinadas < limite) {
        void *pt = espaco_pte(e, mao_virt);
        (*examinadas)++;
        if (!pt) {   /* sin tabla: salta al bloque siguiente */
            mao_virt = (mao_virt & ~(bloco - 1)) + bloco;
            if (!mao_virt) break;
            continue;
        }
        pte_t v = pte_ler(pt);
        if ((v & PG_PRESENTE) && (v & PG_USUARIO) && !(v & PG_EMPRESTADA)) {
            if (v & PG_ACESSADA) pte_mudar(pt, 0, PG_ACESSADA);    /* segunda oportunidad */
            else if ((v & PG_INCOMPRIMIVEL) && !(v & PG_SUJA)) { } /* no ha cambiado */
            else if (!e_futex(pte_endereco(v))) { pts[n] = pt; virts[n] = mao_virt; n++; }
        }
        mao_virt += PAGINA;
        if (!mao_virt) break;
    }
    if (!n) return 0;
    pte_t velhos[LOTE];
    bool atual = espaco_atual() == e;
    for (uint32_t i = 0; i < n; i++) {
        velhos[i] = pte_esvaziar(pts[i]);
        if (atual) pagina_invalidar(virts[i]);
    }
    espaco_sincronizar(e);   /* ninguna otra CPU sigue usando las entradas viejas */
    uint32_t liberadas = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (!(velhos[i] & PG_PRESENTE)) { pte_gravar(pts[i], velhos[i]); continue; }   /* no debería */
        if (*parar) { pte_gravar(pts[i], velhos[i]); continue; }
        liberadas += sacar(pts[i], velhos[i], parar);
    }
    return liberadas;
}

uint32_t troca_liberar(uint32_t quer)
{
    if (!teto || !quer) return 0;
    uint32_t liberadas = 0, examinadas = 0, voltas = 0;
    uint32_t orcamento = quer * 64u + 8192u;
    bool parar = false;
    /* Dos vueltas a la tabla de procesos como mucho: la primera puede solo quitar A. */
    while (liberadas < quer && examinadas < orcamento && voltas <= 2u * MAX_PROCESSOS && !parar) {
        bool irq = int_salva_desliga();
        processo_t *p = processo_indice(mao_proc);
        if (elegivel(p)) liberadas += lote(p, &examinadas, &parar);
        else mao_virt = 0;
        if (mao_virt < USUARIO_INICIO || mao_virt >= USUARIO_FIM) {
            mao_proc = mao_proc + 1 < MAX_PROCESSOS ? mao_proc + 1 : 1;
            mao_virt = USUARIO_INICIO;
            voltas++;
        }
        int_restaura(irq);
    }
    return liberadas;
}

/* Saca a la swap las páginas presentes de [ini, fim) de "e", sin mirar A ni la mano del
 * reloj (MEM_SACAR, como el MADV_PAGEOUT de Linux, y la autoprueba). Por lotes, como el
 * reloj. Devuelve cuántas de ellas están en la swap al terminar. */
uint32_t troca_sacar_faixa(espaco_t e, uint32_t ini, uint32_t fim)
{
    if (!teto) return 0;
    ini &= ~(PAGINA - 1);
    if (ini < USUARIO_INICIO) ini = USUARIO_INICIO;
    if (fim > USUARIO_FIM || fim < ini) fim = USUARIO_FIM;
    bool parar = false;
    uint32_t v = ini;
    while (v < fim && !parar) {
        bool irq = int_salva_desliga();
        void *pts[LOTE];
        uint32_t virts[LOTE], n = 0;
        for (uint32_t vistas = 0; v < fim && n < LOTE && vistas < EXAMINAR_LOTE; v += PAGINA, vistas++) {
            void *pt = espaco_pte(e, v);
            pte_t x = pt ? pte_ler(pt) : 0;
            if ((x & PG_PRESENTE) && (x & PG_USUARIO) && !(x & PG_EMPRESTADA) && !e_futex(pte_endereco(x))) {
                pts[n] = pt;
                virts[n++] = v;
            }
        }
        pte_t velhos[LOTE];
        for (uint32_t i = 0; i < n; i++) {
            velhos[i] = pte_esvaziar(pts[i]);
            if (espaco_atual() == e) pagina_invalidar(virts[i]);
        }
        if (n) espaco_sincronizar(e);
        for (uint32_t i = 0; i < n; i++) {
            if (parar || !(velhos[i] & PG_PRESENTE)) pte_gravar(pts[i], velhos[i]);
            else sacar(pts[i], velhos[i], &parar);
        }
        int_restaura(irq);
    }
    uint32_t fora = 0;
    for (v = ini; v < fim; v += PAGINA) {
        void *pt = espaco_pte(e, v);
        if (pt && troca_e_entrada(pte_ler(pt))) fora++;
    }
    return fora;
}

/* ------------------------------------------------------------ traer */

/* Descomprime el trozo de "v" en buf_volta. */
static bool ler_trozo(pte_t v)
{
    if (v & TROCA_ZERO) {
        memset(buf_volta, 0, PAGINA);
        return true;
    }
    if (!entrada_valida(v)) return false;
    uint32_t i = entrada_idx(v), pos = entrada_pos(v);
    desc_t *d = desc(i);
    uint32_t k = trozo_blocos(d, pos);
    fisica_ler((fis_t)d->pfn * PAGINA + pos * BLOCO_BYTES, buf_trozo, k * BLOCO_BYTES);
    uint32_t n = (uint32_t)buf_trozo[0] | (uint32_t)buf_trozo[1] << 8;
    uint16_t soma = (uint16_t)(buf_trozo[2] | buf_trozo[3] << 8);
    if (n + CAB_TROZO > k * BLOCO_BYTES || n + CAB_TROZO <= (k - 1) * BLOCO_BYTES) return false;
    if (!descomp_pagina(buf_trozo + CAB_TROZO, n, buf_volta)) return false;
    return comp_soma(buf_volta) == soma;
}

int troca_trazer(espaco_t e, uint32_t virt, bool do_kernel)
{
    bool irq = int_salva_desliga();
    int r;
    void *pt = espaco_pte(e, virt);
    pte_t v = pt ? pte_ler(pt) : 0;
    if (!troca_e_entrada(v)) {
        r = (v & PG_PRESENTE) ? TROCA_PRESENTE : TROCA_NAO;
        goto fim;
    }
    /* La página nueva puede sacar otras (troca_liberar usa otros búferes). */
    fis_t nova = memfisica_alocar_usuario();
    if (!nova && do_kernel) nova = memfisica_alocar();   /* el kernel no puede fallar aquí: la baja */
    if (!nova) {
        est.sem_memoria++;
        r = TROCA_SEM_MEMORIA;
        goto fim;
    }
    v = pte_ler(pt);
    if (!troca_e_entrada(v)) {   /* no debería: nadie más corre en el kernel */
        memfisica_liberar(nova);
        r = (v & PG_PRESENTE) ? TROCA_PRESENTE : TROCA_NAO;
        goto fim;
    }
    if (!ler_trozo(v)) {
        memfisica_liberar(nova);
        est.corrompidas++;
        klog("[SWAP] ERRO: trozo corrupto en %08x (entrada %08x)\n", virt, (uint32_t)v);
        r = TROCA_CORROMPIDA;
        goto fim;
    }
    if (v & TROCA_ZERO) fisica_zerar(nova);
    else fisica_escrever(nova, buf_volta, PAGINA);
    troca_soltar(v);
    pte_gravar(pt, pte_valor(nova, PG_PRESENTE | PG_USUARIO | PG_ACESSADA |
                                   ((v & TROCA_ESCRITA) ? PG_ESCRITA : 0) | ((v & TROCA_NX) ? PG_NX : 0)));
    if (espaco_atual() == e) pagina_invalidar(virt);
    est.entradas++;
    if (do_kernel) est.do_kernel++;
    r = TROCA_TRAZIDA;
fim:
    int_restaura(irq);
    return r;
}

/* ------------------------------------------------------------ arranque y tarea */

static uint32_t liberar_para_memfisica(uint32_t paginas) { return troca_liberar(paginas); }

void troca_iniciar(void)
{
    uint8_t opc = kconf_cab(CAB_MEM_OPC);   /* D353: con lo que guardó kconfig */
    uint32_t baixa = memfisica_geridas(), alta = memfisica_geridas_alta();
    uint32_t total = baixa + alta;
    reserva_baixa = baixa / 32;
    if (reserva_baixa < 4096) reserva_baixa = 4096;          /* 16 MB */
    if (reserva_baixa > 16384) reserva_baixa = 16384;        /* 64 MB */
    if (opc & CAB_MEM_SEM_TROCA) {
        klog("[SWAP] swap comprimida desactivada (%s)\n",
             (cabecalho_boot[CAB_MEM_OPC] & CAB_MEM_SEM_TROCA) ? "--sin-swap" : "kconfig swap=no");
        return;
    }
    if (total < 8192 || baixa < 2 * reserva_baixa) {   /* menos de 32 MB: no merece la pena */
        klog("[SWAP] swap comprimida desactivada: poca memoria (%u MB)\n", total / 256);
        return;
    }
    uint16_t mb = (uint16_t)(kconf_cab(CAB_TROCA_MB) | kconf_cab(CAB_TROCA_MB + 1) << 8);   /* D353: o kconfig swap_mb */
    /* Por defecto: con alta, la mitad de la RAM; sin alta (solo la baja, la del kernel),
     * una cuarta parte. Es un tope, no una reserva: el almacén solo crece al sacar. */
    teto = mb ? (uint32_t)mb * 256u : (alta ? total / 2 : total / 4);
    if (teto > TETO_MAX) teto = TETO_MAX;
    if (teto < 1024) teto = 1024;
    umbral = total / 128;
    if (umbral < 1024) umbral = 1024;      /* 4 MB */
    if (umbral > 8192) umbral = 8192;      /* 32 MB */
    alvo = 2 * umbral;
    for (uint32_t c = 0; c < BLOCOS_PAG; c++) cubos[c] = NENHUM;
    memfisica_registar_troca(liberar_para_memfisica, reserva_baixa);
    est.teto_paginas = teto;
    klog("[SWAP] swap comprimida: tope %u MB (%s), reserva baja del kernel %u MB, umbral %u MB\n",
         teto / 256, mb ? (mb != (cabecalho_boot[CAB_TROCA_MB] | cabecalho_boot[CAB_TROCA_MB + 1] << 8) ? "kconfig swap_mb" : "--swap-mb") : alta ? "la mitad de la RAM" : "sin alta: un cuarto de la RAM",
         reserva_baixa / 256, umbral / 256);
}

static const char canal_tarefa = 0;

/* Cada 100 ms: si los programas tienen menos de "umbral" páginas libres, primero la
 * caché de disco devuelve bloques limpios y después se saca a la swap hasta "alvo". */
static void tarefa(void *arg)
{
    (void)arg;
    for (;;) {
        int_desliga();
        processo_atual()->acordar_em = relogio_tiques() + TIQUES_POR_SEGUNDO / 10;
        bloquear(&canal_tarefa);
        ceder();
        int_liga();
        uint32_t livres = memfisica_livres_programas();
        if (livres >= umbral) continue;
        uint32_t falta = alvo - livres;
        if (falta > 4096) falta = 4096;   /* 16 MB por vuelta */
        uint32_t c = memfisica_reclamar(falta);
        falta = c >= falta ? 0 : falta - c;
        while (falta) {
            uint32_t q = falta > 64 ? 64 : falta;
            uint32_t r = troca_liberar(q);
            if (!r) break;
            falta = r >= falta ? 0 : falta - r;
        }
    }
}

void troca_tarefa_iniciar(void)
{
    if (teto) tarefa_kernel("swap", tarefa, NULL);
}

void troca_estatisticas(troca_est_t *e)
{
    bool irq = int_salva_desliga();
    *e = est;
    e->bytes = est.blocos * BLOCO_BYTES;
    int_restaura(irq);
}
