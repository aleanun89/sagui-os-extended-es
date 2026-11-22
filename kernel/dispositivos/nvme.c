// SPDX-License-Identifier: GPL-2.0-or-later
/* nvme.c - driver del controlador NVMe (D265), escrito desde la especificación pública
 * NVM Express 1.4 / 2.0 (sin código de otros sistemas).
 *
 * Un NVMe es un dispositivo PCIe con una barra de memoria (BAR0) donde viven sus registros
 * (CAP, VS, CC, CSTS, AQA, ASQ, ACQ y los "doorbells"). Se habla con él por colas circulares
 * en memoria del kernel:
 *   - una cola de envío (SQ) de entradas de 64 bytes, que escribe el kernel;
 *   - una cola de finalización (CQ) de entradas de 16 bytes, que escribe el controlador y
 *     cuyo bit de fase (P) cambia en cada vuelta, así el kernel sabe qué entradas son nuevas;
 *   - al meter una orden se avisa al controlador escribiendo la nueva cola en el "doorbell".
 * Hay una cola Admin (Identify, crear colas) y una cola de E/S (lectura, escritura, Flush).
 *
 * Interrupciones: NINGUNA. El kernel no tiene MSI/MSI-X (solo INTx y el IOAPIC), así que
 * el driver va por sondeo con plazo, como AHCI (D155): se apagan las interrupciones del
 * controlador (INTMS y bit de desactivar INTx del PCI) y se mira la CQ hasta que aparece la
 * entrada o vence el plazo (TSC, prazo.h). Para leer y escribir un disco con una sola orden en
 * curso no hacen falta; MSI queda como mejora si algún día el rendimiento lo pide.
 *
 * Datos: un búfer de rebote de 128 KB contiguo (32 páginas). Las transferencias de más de dos
 * páginas usan una lista PRP (una página). La capa de discos solo entiende sectores de 512
 * bytes: un namespace de 4096 se presenta en unidades de 512 y las escrituras parciales leen,
 * modifican y escriben el bloque entero.
 *
 * Robustez: toda espera tiene plazo; si el controlador no responde se apaga (CC.EN = 0) y los
 * discos dejan de contestar (error) en vez de colgar el arranque o el sistema. */
#include "nvme.h"
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "disco.h"
#include "info_hardware.h"
#include "memoria.h"
#include "paginacao.h"
#include "pci.h"
#include "pci_realoc.h"
#include "prazo.h"
#include "texto.h"

/* Registros del controlador (capítulo 3 de la especificación). */
#define R_CAP    0x00   /* 64 bits */
#define R_VS     0x08
#define R_INTMS  0x0C   /* enmascarar interrupciones */
#define R_CC     0x14
#define R_CSTS   0x1C
#define R_AQA    0x24
#define R_ASQ    0x28   /* 64 bits */
#define R_ACQ    0x30   /* 64 bits */
#define R_DOORBELL 0x1000

#define CC_EN        (1u << 0)
#define CSTS_RDY     (1u << 0)
#define CSTS_CFS     (1u << 1)   /* fallo fatal del controlador */

#define PAG          4096u
#define PROF_ADMIN   16u
#define PROF_ES      64u
#define TAM_REBOTE   (32u * PAG)  /* 128 KB */
#define ESPERA_ORDEN_MS  5000u    /* una orden de E/S: 5 s como mucho */
#define ESPERA_ADMIN_MS  5000u
#define ESPERA_FLUSH_MS  15000u
#define NVME_MAX_CONTROLADORES 4
#define NVME_MAX_NAMESPACES    4

/* Operaciones Admin y de E/S. */
#define ADM_CRIAR_SQ   0x01
#define ADM_CRIAR_CQ   0x05
#define ADM_IDENTIFY   0x06
#define ES_FLUSH       0x00
#define ES_ESCREVER    0x01
#define ES_LER         0x02

typedef struct {
    volatile uint32_t *sq;     /* entradas de 64 bytes (16 palabras) */
    volatile uint32_t *cq;     /* entradas de 16 bytes (4 palabras) */
    uint32_t sq_fis, cq_fis;
    uint32_t prof;
    uint32_t cauda, cabeca;    /* cola de envío (tail) y de finalización (head) */
    uint8_t  fase;             /* fase esperada de la CQ */
    uint32_t db_sq, db_cq;     /* desplazamiento de los doorbells */
} nvme_fila_t;

typedef struct nvme_ctl {
    pci_disp_t pci;
    volatile uint8_t *regs;
    uint32_t stride;           /* separación entre doorbells: 4 << CAP.DSTRD */
    uint32_t espera_rdy_ms;    /* CAP.TO */
    nvme_fila_t admin, es;
    uint16_t cid;
    uint8_t *rebote;           /* 128 KB para los datos */
    uint8_t *lista;            /* página de lista PRP */
    uint32_t max_bytes;        /* tamaño máximo por orden (MDTS y el rebote) */
    bool     morto;            /* dejó de contestar: se apagó */
    bool     ok;
} nvme_ctl_t;

typedef struct {
    nvme_ctl_t *ctl;
    uint32_t    nsid;
    uint64_t    blocos;
    uint32_t    tam_bloco;     /* 512 o 4096 */
} nvme_disco_t;

static nvme_ctl_t ctls[NVME_MAX_CONTROLADORES];
static int qtd_ctls, qtd_controladores, qtd_sem_suporte, qtd_discos;

int nvme_controladores(void) { return qtd_controladores; }
int nvme_sem_suporte(void) { return qtd_sem_suporte; }

/* ------------------------------------------------------------- registros */

static inline uint32_t rd32(nvme_ctl_t *c, uint32_t off) { return *(volatile uint32_t *)(c->regs + off); }
static inline void wr32(nvme_ctl_t *c, uint32_t off, uint32_t v) { *(volatile uint32_t *)(c->regs + off) = v; }
static inline void wr64(nvme_ctl_t *c, uint32_t off, uint32_t baixo) { wr32(c, off, baixo); wr32(c, off + 4, 0); }
static inline void barreira(void) { __asm__ volatile("" ::: "memory"); }

/* Espera a que CSTS.RDY valga "quer" (o que haya un fallo fatal); false si vence el plazo. */
static bool esperar_rdy(nvme_ctl_t *c, uint32_t quer)
{
    uint32_t ms = c->espera_rdy_ms;
    if (ms < 1000) ms = 1000;
    if (ms > 20000) ms = 20000;
    prazo_t pz = prazo_ms(ms);
    for (;;) {
        uint32_t s = rd32(c, R_CSTS);
        if ((s & CSTS_RDY) == quer) return true;
        if (s == 0xFFFFFFFFu) return false;   /* el dispositivo ya no está en el bus */
        if (prazo_vencido(&pz)) return false;
    }
}

static void apagar(nvme_ctl_t *c)
{
    c->morto = true;
    wr32(c, R_CC, 0);
}

/* D450: aviso de apagado (CC.SHN = 01, «apagado normal») y espera a CSTS.SHST = 10 («hecho»),
 * como mucho 5 s por controlador (el shutdown_timeout de Linux). Sin él, el disco apunta un
 * «apagado inseguro» y en el próximo arranque revisa sus tablas (más lento). Va después de grabar
 * y de vaciar la caché (montagem_desmontar_tudo y el FLUSH): aquí ya no se escribe nada. */
#define CC_SHN_NORMAL   (1u << 14)
#define CC_SHN_MASCARA  (3u << 14)
#define CSTS_SHST_MASC  (3u << 2)
#define CSTS_SHST_FEITO (2u << 2)
#define ESPERA_SHN_MS   5000u

int nvme_desligar_todos(void)
{
    int feitos = 0;
    for (int i = 0; i < qtd_ctls; i++) {
        nvme_ctl_t *c = &ctls[i];
        if (!c->ok || c->morto || !c->regs) continue;
        wr32(c, R_INTMS, 0xFFFFFFFFu);   /* sin interrupciones */
        wr32(c, R_CC, (rd32(c, R_CC) & ~CC_SHN_MASCARA) | CC_SHN_NORMAL);
        prazo_t pz = prazo_ms(ESPERA_SHN_MS);
        uint32_t s;
        for (;;) {
            s = rd32(c, R_CSTS);
            if (s == 0xFFFFFFFFu || (s & CSTS_SHST_MASC) == CSTS_SHST_FEITO || prazo_vencido(&pz)) break;
        }
        if ((s & CSTS_SHST_MASC) == CSTS_SHST_FEITO && s != 0xFFFFFFFFu) feitos++;
        else klog("[NVME] %d: el aviso de apagado no termino en %u ms (CSTS %x): se sigue\n", i, ESPERA_SHN_MS, s);
        c->morto = true;   /* nada más por esta cola */
    }
    return feitos;
}

/* ------------------------------------------------------------- colas */

/* Mete una orden (16 palabras) en la cola y espera su finalización. Devuelve el estado
 * (0 = correcto, >0 = código de estado de la especificación) o -1 si vence el plazo. */
static int ejecutar(nvme_ctl_t *c, nvme_fila_t *f, const uint32_t *orden, uint32_t espera_ms, uint32_t *res)
{
    if (c->morto) return -1;
    uint16_t cid = ++c->cid;
    volatile uint32_t *e = f->sq + f->cauda * 16;
    for (int i = 0; i < 16; i++) e[i] = orden[i];
    e[0] = (orden[0] & 0xFFFFu) | ((uint32_t)cid << 16);
    f->cauda = (f->cauda + 1) % f->prof;
    barreira();
    wr32(c, f->db_sq, f->cauda);

    prazo_t pz = prazo_ms(espera_ms);
    for (;;) {
        volatile uint32_t *q = f->cq + f->cabeca * 4;
        uint32_t d3 = q[3];
        if (((d3 >> 16) & 1u) == f->fase) {
            if (res) *res = q[0];
            f->cabeca++;
            if (f->cabeca == f->prof) { f->cabeca = 0; f->fase ^= 1; }
            barreira();
            wr32(c, f->db_cq, f->cabeca);
            if ((d3 & 0xFFFFu) != cid) {
                klog("[NVME] respuesta con identificador %u, se esperaba %u\n", d3 & 0xFFFFu, cid);
                apagar(c);
                return -1;
            }
            return (int)((d3 >> 17) & 0x7FFFu);
        }
        if (prazo_vencido(&pz) || (rd32(c, R_CSTS) & CSTS_CFS)) {
            klog("[NVME] orden %x sin respuesta (CSTS %x): controlador apagado\n", orden[0] & 0xFF, rd32(c, R_CSTS));
            apagar(c);
            return -1;
        }
    }
}

static int admin(nvme_ctl_t *c, uint32_t *o, uint32_t *res)
{
    return ejecutar(c, &c->admin, o, ESPERA_ADMIN_MS, res);
}

static void fila_preparar(nvme_ctl_t *c, nvme_fila_t *f, uint32_t qid, uint32_t prof)
{
    f->prof = prof;
    f->cauda = f->cabeca = 0;
    f->fase = 1;
    f->db_sq = R_DOORBELL + (2 * qid) * c->stride;
    f->db_cq = R_DOORBELL + (2 * qid + 1) * c->stride;
}

/* Reserva una página, a cero. */
static uint8_t *pagina_zero(uint32_t paginas)
{
    uint8_t *p = (uint8_t *)memfisica_alocar_seguidas(paginas);
    if (p) memset(p, 0, paginas * PAG);
    return p;
}

/* Identify: cns 1 = controlador, 0 = namespace (nsid). Los 4 KB quedan en el rebote. */
static bool identificar(nvme_ctl_t *c, uint32_t cns, uint32_t nsid)
{
    /* Palabras de la orden: 0 = código, 1 = NSID, 4-5 = MPTR, 6-7 = PRP1, 8-9 = PRP2, 10-15 = CDW10-15. */
    uint32_t o[16] = { ADM_IDENTIFY, nsid, 0, 0, 0, 0, (uint32_t)c->rebote, 0, 0, 0, cns, 0, 0, 0, 0, 0 };
    memset(c->rebote, 0, PAG);
    return admin(c, o, NULL) == 0;
}

/* ------------------------------------------------------------- E/S */

/* Una orden de lectura/escritura de "nblk" bloques del namespace desde "lba", con los datos
 * en el rebote. Devuelve 0 o -1. */
static int es_orden(nvme_disco_t *n, uint32_t op, uint64_t lba, uint32_t nblk)
{
    nvme_ctl_t *c = n->ctl;
    uint32_t bytes = nblk * n->tam_bloco;
    uint32_t paginas = (bytes + PAG - 1) / PAG;
    uint32_t prp2 = 0;
    if (paginas == 2) prp2 = (uint32_t)c->rebote + PAG;
    else if (paginas > 2) {
        volatile uint64_t *l = (volatile uint64_t *)c->lista;
        for (uint32_t i = 1; i < paginas; i++) l[i - 1] = (uint64_t)((uint32_t)c->rebote + i * PAG);
        prp2 = (uint32_t)c->lista;
    }
    uint32_t o[16] = { op, n->nsid, 0, 0, 0, 0, (uint32_t)c->rebote, 0,
                       prp2, 0, (uint32_t)lba, (uint32_t)(lba >> 32), nblk - 1, 0, 0, 0 };
    int st = ejecutar(c, &c->es, o, ESPERA_ORDEN_MS, NULL);
    if (st != 0) {
        if (st > 0) klog("[NVME] nsid %u: orden %x en el bloque %u falló, estado %x\n", n->nsid, op, (uint32_t)lba, st);
        return -1;
    }
    return 0;
}

/* La capa de discos trabaja en sectores de 512: se traducen a bloques del namespace. */
static int transferir(disco_t *k, uint32_t lba, uint32_t n, uint8_t *buf, bool escrever)
{
    nvme_disco_t *d = k->dono;
    nvme_ctl_t *c = d->ctl;
    uint32_t por = d->tam_bloco / 512;
    while (n) {
        uint32_t desloc = (lba % por) * 512;
        uint64_t b0 = lba / por;
        uint32_t max_blocos = c->max_bytes / d->tam_bloco;
        uint32_t nblk = (desloc + n * 512 + d->tam_bloco - 1) / d->tam_bloco;
        if (nblk > max_blocos) nblk = max_blocos;
        uint32_t util = nblk * d->tam_bloco - desloc;
        if (util > n * 512) util = n * 512;
        if (!escrever) {
            if (es_orden(d, ES_LER, b0, nblk) < 0) return -1;
            memcpy(buf, c->rebote + desloc, util);
        } else {
            /* Escritura parcial de un bloque grande: leer, cambiar la parte y escribir. */
            if (desloc || util % d->tam_bloco) {
                if (es_orden(d, ES_LER, b0, nblk) < 0) return -1;
            }
            memcpy(c->rebote + desloc, buf, util);
            if (es_orden(d, ES_ESCREVER, b0, nblk) < 0) return -1;
        }
        buf += util;
        lba += util / 512;
        n -= util / 512;
    }
    return 0;
}

static int ler_setores(disco_t *k, uint32_t lba, uint32_t n, void *buf)
{
    return transferir(k, lba, n, buf, false);
}

static int escrever_setores(disco_t *k, uint32_t lba, uint32_t n, const void *buf)
{
    if (transferir(k, lba, n, (uint8_t *)buf, true) < 0) return -1;
    /* Como en AHCI (D179): lo escrito sale de la caché del controlador antes de volver. */
    nvme_disco_t *d = k->dono;
    uint32_t o[16] = { ES_FLUSH, d->nsid };
    return ejecutar(d->ctl, &d->ctl->es, o, ESPERA_FLUSH_MS, NULL) == 0 ? 0 : -1;
}

/* ------------------------------------------------------------- arranque */

static void texto_nvme(char *dest, const uint8_t *s, int n, int max)
{
    int k = 0;
    for (int i = 0; i < n && k + 1 < max; i++) dest[k++] = (s[i] >= 32 && s[i] < 127) ? (char)s[i] : ' ';
    while (k > 0 && (dest[k - 1] == ' ' || dest[k - 1] == 0)) k--;
    dest[k] = 0;
}

static bool registrar_namespace(nvme_ctl_t *c, uint32_t nsid, const char *modelo)
{
    if (!identificar(c, 0, nsid)) {
        klog("[NVME] namespace %u: Identify falló\n", nsid);
        return false;
    }
    const uint8_t *ns = c->rebote;
    uint64_t nsze = (uint64_t)((const uint32_t *)ns)[0] | ((uint64_t)((const uint32_t *)ns)[1] << 32);
    if (!nsze) return false;                       /* inactivo */
    uint8_t flbas = ns[26] & 0xF;                  /* formato en uso (0..15; los 16.. no se usan aquí) */
    uint32_t lbaf = *(const uint32_t *)(ns + 128 + 4 * flbas);
    uint32_t ms = lbaf & 0xFFFFu, lbads = (lbaf >> 16) & 0xFFu;
    uint32_t tam = lbads < 31 ? 1u << lbads : 0;
    if (tam != 512 && tam != 4096) {
        klog("[NVME] namespace %u: bloque de %u bytes no soportado\n", nsid, tam);
        return false;
    }
    if (ms && (ns[26] & 0x10)) {
        klog("[NVME] namespace %u: metadatos intercalados no soportados\n", nsid);
        return false;
    }
    nvme_disco_t *d = kmalloc(sizeof *d);
    if (!d) return false;
    d->ctl = c; d->nsid = nsid; d->blocos = nsze; d->tam_bloco = tam;
    uint64_t setores = nsze * (tam / 512);
    if (setores > 0xFFFFFFFFull) setores = 0xFFFFFFFFull;    /* la capa de discos usa 32 bits (2 TB) */
    klog("[NVME] namespace %u: \"%s\", %u bloques de %u bytes\n", nsid, modelo, (uint32_t)nsze, tam);
    disco_t *k = disco_registrar_nvme(modelo, (uint32_t)setores, (uint16_t)tam, d, ler_setores, escrever_setores);
    if (!k) { kfree(d); return false; }
    qtd_discos++;
    return true;
}

static bool iniciar_controlador(nvme_ctl_t *c)
{
    uint32_t b0 = pci_le32(&c->pci, 0x10);
    if (b0 & 1) { klog("[NVME] %x:%x: BAR0 no es de memoria\n", c->pci.fabricante, c->pci.modelo); return false; }
    uint32_t alto = (b0 & 6) == 4 ? pci_le32(&c->pci, 0x14) : 0;
    uint32_t base = b0 & ~0xFu;
    if (alto) {   /* D283: el kernel es de 32 bits: se baja la BAR a un hueco por debajo de 4 GB */
        uint32_t nueva = pci_realoc_bar_baixo(&c->pci, 0, "NVMe");
        if (nueva) { base = nueva; alto = 0; }
    }
    if (alto || !base) { klog("[NVME] %x:%x: BAR0 inexistente o por encima de 4 GB\n", c->pci.fabricante, c->pci.modelo); return false; }
    /* Memoria y maestro del bus; sin INTx (se sondea). */
    pci_escreve32(&c->pci, 0x04, (pci_le32(&c->pci, 0x04) & 0xFFFFu) | 0x6 | (1u << 10));
    uint32_t v = janela_mapear_tipo(base, 0x4000, PG_PCD);
    if (!v) { klog("[NVME] %x:%x: no se pudo mapear BAR0\n", c->pci.fabricante, c->pci.modelo); return false; }
    c->regs = (volatile uint8_t *)v;

    uint32_t cap_l = rd32(c, R_CAP), cap_h = rd32(c, R_CAP + 4);
    uint32_t vs = rd32(c, R_VS);
    if (cap_l == 0xFFFFFFFFu && cap_h == 0xFFFFFFFFu) { klog("[NVME] el controlador no responde (CAP todo unos)\n"); return false; }
    uint32_t dstrd = cap_h & 0xF;                    /* CAP bits 35:32 */
    uint32_t mpsmin = (cap_h >> 16) & 0xF;      /* CAP.MPSMIN: 0 = páginas de 4 KB */
    c->stride = 4u << dstrd;
    c->espera_rdy_ms = ((cap_l >> 24) & 0xFF) * 500u;
    klog("[NVME] controlador %x:%x en %u:%u.%u, versión %u.%u, BAR0 %x, cola máx. %u\n", c->pci.fabricante, c->pci.modelo,
         c->pci.barramento, c->pci.dispositivo, c->pci.funcao, vs >> 16, (vs >> 8) & 0xFF, base, (cap_l & 0xFFFF) + 1);
    if (!(cap_h & (1u << 5))) { klog("[NVME] sin el conjunto de órdenes NVM\n"); return false; }
    if (mpsmin != 0) { klog("[NVME] páginas de 4 KB no soportadas\n"); return false; }
    if (R_DOORBELL + 4 * c->stride + 4 > 0x4000) { klog("[NVME] separación de doorbells excesiva\n"); return false; }
    if (PROF_ES > (cap_l & 0xFFFF) + 1) { klog("[NVME] colas demasiado pequeñas\n"); return false; }

    /* Parar el controlador (CC.EN = 0) y esperar a RDY = 0. */
    wr32(c, R_CC, 0);
    if (!esperar_rdy(c, 0)) { klog("[NVME] no se detiene (RDY sigue en 1)\n"); return false; }
    wr32(c, R_INTMS, 0xFFFFFFFFu);

    /* Memoria: colas Admin y de E/S, lista PRP y rebote. */
    uint8_t *asq = pagina_zero(1), *acq = pagina_zero(1), *isq = pagina_zero(PROF_ES * 64 / PAG);
    uint8_t *icq = pagina_zero(1);
    c->lista = pagina_zero(1);
    c->rebote = pagina_zero(TAM_REBOTE / PAG);
    if (!asq || !acq || !isq || !icq || !c->lista || !c->rebote) { klog("[NVME] sin memoria para las colas\n"); return false; }
    fila_preparar(c, &c->admin, 0, PROF_ADMIN);
    c->admin.sq = (volatile uint32_t *)asq; c->admin.cq = (volatile uint32_t *)acq;
    fila_preparar(c, &c->es, 1, PROF_ES);
    c->es.sq = (volatile uint32_t *)isq; c->es.cq = (volatile uint32_t *)icq;

    wr32(c, R_AQA, ((PROF_ADMIN - 1) << 16) | (PROF_ADMIN - 1));
    wr64(c, R_ASQ, (uint32_t)asq);
    wr64(c, R_ACQ, (uint32_t)acq);
    /* CC: IOCQES = 4 (16 bytes), IOSQES = 6 (64 bytes), MPS = 0 (4 KB), CSS = NVM, EN. */
    wr32(c, R_CC, (4u << 20) | (6u << 16) | CC_EN);
    if (!esperar_rdy(c, CSTS_RDY)) { klog("[NVME] no llega a RDY\n"); apagar(c); return false; }

    /* Identify Controller. */
    if (!identificar(c, 1, 0)) { klog("[NVME] Identify Controller falló\n"); apagar(c); return false; }
    char modelo[44];
    texto_nvme(modelo, c->rebote + 24, 40, sizeof modelo);   /* MN; el número de serie (SN) no se muestra */
    uint32_t nn = *(const uint32_t *)(c->rebote + 516);
    uint8_t mdts = c->rebote[77];
    c->max_bytes = TAM_REBOTE;
    if (mdts && mdts < 16 && (PAG << mdts) < c->max_bytes) c->max_bytes = PAG << mdts;
    if (!modelo[0]) txt_copia_n(modelo, "NVMe", sizeof modelo);

    /* Crear la cola de E/S 1: primero la CQ, luego la SQ. */
    uint32_t cq[16] = { ADM_CRIAR_CQ, 0, 0, 0, 0, 0, (uint32_t)icq, 0, 0, 0, ((PROF_ES - 1) << 16) | 1, 1u /* PC, sin interrupciones */, 0 };
    if (admin(c, cq, NULL) != 0) { klog("[NVME] no se pudo crear la CQ de E/S\n"); apagar(c); return false; }
    uint32_t sq[16] = { ADM_CRIAR_SQ, 0, 0, 0, 0, 0, (uint32_t)isq, 0, 0, 0, ((PROF_ES - 1) << 16) | 1, (1u << 16) | 1u, 0 };
    if (admin(c, sq, NULL) != 0) { klog("[NVME] no se pudo crear la SQ de E/S\n"); apagar(c); return false; }

    c->ok = true;
    if (nn > NVME_MAX_NAMESPACES) nn = NVME_MAX_NAMESPACES;
    if (!nn) nn = 1;
    int hallados = 0;
    for (uint32_t id = 1; id <= nn && !c->morto; id++)
        if (registrar_namespace(c, id, modelo)) hallados++;
    if (!hallados) klog("[NVME] el controlador no tiene namespaces utilizables\n");
    return c->ok;
}

int nvme_iniciar(void)
{
    pci_disp_t achados[NVME_MAX_CONTROLADORES];
    int n = pci_procurar(0x01, 0x08, 0x02, achados, NVME_MAX_CONTROLADORES);
    qtd_controladores = n;
    if (!n) return 0;
    bool i0 = int_salva_desliga();
    for (int i = 0; i < n; i++) {
        nvme_ctl_t *c = &ctls[qtd_ctls];
        memset(c, 0, sizeof *c);
        c->pci = achados[i];
        if (iniciar_controlador(c)) qtd_ctls++;
        else qtd_sem_suporte++;
    }
    int_restaura(i0);
    return qtd_discos;
}
