// SPDX-License-Identifier: GPL-2.0-or-later
/* ata.c - discos internos ATA (IDE) por PIO, sin DMA ni interrupciones (D040).
 *
 * Canales: los dos clásicos (0x1F0/0x3F6 y 0x170/0x376) y los que un
 * controlador IDE en el PCI (clase 01h, subclase 01h) informe en modo
 * nativo (BARs). Los controladores SATA en modo AHCI puro (subclase 06h) los
 * lleva otro driver, ahci.c (D155), que los registra en la misma capa de
 * discos.
 *
 * Cada disco responde al IDENTIFY; direccionamiento LBA28 y, para sectores más allá
 * de 2^28, LBA48. D330: los discos que no anuncian LBA (palabra 49, bit 9; los de menos de
 * 1024 cilindros de la época, y los que emula 86Box) se direccionan por CHS con su
 * geometría por defecto (palabras 1, 3 y 6), que se fija antes con INITIALIZE DEVICE
 * PARAMETERS para que el disco y el driver cuenten igual. Toda espera tiene límite (un canal sin disco devuelve 0xFF o
 * 0x00 y nunca bloquea el arranque). Interrupciones del disco desactivadas (nIEN): el
 * driver solo consulta el registro de estado.
 *
 * D207: los lectores de CD/DVD (ATAPI) del mismo bus se leen con la orden PACKET por
 * PIO (pata_pacote); la parte común (sense, capacidad, la capa de discos) está en
 * atapi.c. Sus esperas son por tiempo (prazo.h), no por vueltas.
 *
 * D281: DMA por Bus Master IDE (PIIX/ICH y compatibles: BAR4, tabla PRD de una entrada y un búfer
 * de 64 KB contiguo) con interrupción. Con el planificador en marcha, el proceso que lee o graba
 * duerme hasta la IRQ (o, sin IRQ útil, cede el turno) y el resto del sistema sigue corriendo;
 * antes de eso, o dentro de una IRQ, se espera sondeando. Si el DMA falla, el canal vuelve a PIO
 * para siempre. Cada canal tiene su cerrojo (trava_t): dos procesos no mezclan órdenes.
 *
 * SEGURIDAD: este driver nunca graba por sí solo. Quien graba es un sistema de
 * archivos montado para escritura, y en un disco interno eso solo ocurre con
 * "mount -o rw" explícito (montagem.c). disco_escrever también comprueba. */
#include "ata.h"
#include "ahci.h"
#include "nvme.h"
#include "atapi.h"
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "disco.h"
#include "memoria.h"
#include "pci.h"
#include "portas.h"
#include "prazo.h"
#include "relogio.h"
#include "texto.h"
#include "trava.h"
#include "processo.h"
#include "bootinfo.h"

#define REG_DADOS     0
#define REG_ERRO      1
#define REG_QTD       2
#define REG_LBA0      3
#define REG_LBA1      4
#define REG_LBA2      5
#define REG_DISP      6
#define REG_CMD       7   /* escritura: comando; lectura: estado */
#define ST_ERR  0x01
#define ST_DRQ  0x08
#define ST_DF   0x20
#define ST_RDY  0x40
#define ST_BSY  0x80
#define CMD_LER        0x20
#define CMD_LER_EXT    0x24
#define CMD_GRAVAR     0x30
#define CMD_GRAVAR_EXT 0x34
#define CMD_FLUSH      0xE7
#define CMD_FLUSH_EXT  0xEA
#define CMD_IDENTIFY   0xEC
#define CMD_PACKET     0xA0   /* D207: orden ATAPI (paquete SCSI de 12 bytes) */
#define CMD_ID_PACKET  0xA1   /* D207: IDENTIFY PACKET DEVICE */
#define CMD_RESET_DISP 0x08   /* D207: DEVICE RESET (solo ATAPI) */
#define ESPERA_MAX     4000000u   /* lecturas del estado (algunos segundos en el peor caso) */

/* D281: Bus Master IDE (registros relativos a la base del canal). */
#define BM_CMD       0
#define BM_ST        2
#define BM_PRD       4
#define BM_CMD_INICIA 0x01
#define BM_CMD_LER    0x08   /* sentido: del disco a la memoria */
#define BM_ST_ATIVO   0x01
#define BM_ST_ERRO    0x02
#define BM_ST_IRQ     0x04
#define CMD_LER_DMA        0xC8
#define CMD_LER_DMA_EXT    0x25
#define CMD_GRAVAR_DMA     0xCA
#define CMD_GRAVAR_DMA_EXT 0x35
#define CMD_SET_FEATURES   0xEF
#define CMD_INIC_PARAMS    0x91   /* D330: INITIALIZE DEVICE PARAMETERS (geometría CHS actual) */
#define DMA_SETORES  128u    /* 64 KB por orden: una sola entrada de la tabla PRD */

typedef struct {
    uint16_t base, ctrl;
    bool     usado;
    uint16_t bm;                /* base del Bus Master (0 = el canal no tiene) */
    uint8_t  irq;               /* línea PCI/ISA del canal (0 = ninguna) */
    bool     dma_ok;            /* búfer y PRD listos */
    bool     irq_usavel;        /* la IRQ del canal llega (se pone a false si no responde) */
    uint8_t  irq_falhas;        /* esperas que terminaron por el reloj estando el DMA ya acabado */
    volatile bool irq_chegou;
    uint8_t  bm_st, ata_st;     /* lo que vio el manipulador de la IRQ */
    uint8_t *buf;               /* 64 KB contiguos y alineados a 64 KB */
    uint32_t *prd;
    trava_t  trava;
} canal_t;

typedef struct {
    canal_t *canal;
    uint8_t  unidade;          /* 0 = maestro, 1 = esclavo */
    bool     dma;              /* D281: el disco tiene un modo DMA activo */
    bool     lba48;
    bool     chs;              /* D330: sin LBA; se direcciona por cilindro/cabeza/sector */
    uint8_t  cabecas, setpista; /* geometría CHS fijada con INITIALIZE DEVICE PARAMETERS */
    bool     flush_anunciado;  /* D330: IDENTIFY palabra 83, bit 12 (FLUSH CACHE, ATA-6 en adelante) */
    bool     sem_flush;        /* D330: el disco abortó el FLUSH CACHE sin anunciarlo: ya no se manda */
    uint32_t setores;
    disco_t *disco;
} ata_disco_t;

#define MAX_CANAIS 4
static canal_t canais[MAX_CANAIS];
static ata_disco_t unidades[MAX_CANAIS * 2];
static int qtd_unidades;

static void pausa_400ns(canal_t *c)
{
    for (int i = 0; i < 4; i++) porta_le8(c->ctrl);   /* estado alternativo: no toca nada */
}

/* Espera a que BSY baje; devuelve el estado o -1 si se acabó el tiempo. */
static int esperar_livre(canal_t *c)
{
    /* D245: límite por tiempo real (TSC), no por vueltas: en un PC lento o rápido dura igual. */
    prazo_t p = prazo_ms(4000);
    for (;;) {
        uint8_t s = porta_le8(c->ctrl);
        if (s == 0xFF) return -1;            /* bus flotando: no hay nada */
        if (!(s & ST_BSY)) return s;
        if (prazo_vencido(&p)) return -1;
    }
}

/* El FLUSH CACHE puede tardar mucho más que un comando normal (el estándar habla de
 * hasta 30 s; en QEMU, un fsync del anfitrión con la máquina cargada pasa de 4 s). D179:
 * el formateo y el instalador escriben mucho seguido y daban "erro de gravacao". */
static int esperar_livre_flush(canal_t *c)
{
    for (int volta = 0; volta < 10; volta++) {
        int s = esperar_livre(c);
        if (s >= 0) return s;
        if (porta_le8(c->ctrl) == 0xFF) return -1;
    }
    return -1;
}

/* Espera datos (DRQ) o error. Con margen largo (D179): entre sector y sector el disco
 * puede estar ocupado grabando el anterior, y un QEMU con el anfitrión cargado tarda. */
static int esperar_dados(canal_t *c)
{
    prazo_t p = prazo_ms(15000);   /* D245: por tiempo real, como esperar_livre */
    for (;;) {
        uint8_t s = porta_le8(c->ctrl);
        if (s == 0xFF) return -1;
        if (!(s & ST_BSY)) {
            if (s & (ST_ERR | ST_DF)) return -1;
            if (s & ST_DRQ) return s;
        }
        if (prazo_vencido(&p)) return -1;
    }
}

/* Devuelve 1 si hay un disco ATA (IDENTIFY en "id"), 2 si es un ATAPI (CD/DVD, D207;
 * "id" sin rellenar) y 0 si no hay nada utilizable. */
static int identificar(canal_t *c, uint8_t unidade, uint16_t *id)
{
    porta_escreve8(c->ctrl, 0x02);                         /* nIEN: sin interrupciones */
    porta_escreve8(c->base + REG_DISP, (uint8_t)(0xA0 | unidade << 4));
    pausa_400ns(c);
    /* ¿Hay alguien respondiendo? Los registros guardan lo que se escribe. */
    porta_escreve8(c->base + REG_QTD, 0x55);
    porta_escreve8(c->base + REG_LBA0, 0xAA);
    if (porta_le8(c->base + REG_QTD) != 0x55 || porta_le8(c->base + REG_LBA0) != 0xAA) return 0;
    porta_escreve8(c->base + REG_QTD, 0);
    porta_escreve8(c->base + REG_LBA0, 0);
    porta_escreve8(c->base + REG_LBA1, 0);
    porta_escreve8(c->base + REG_LBA2, 0);
    porta_escreve8(c->base + REG_CMD, CMD_IDENTIFY);
    pausa_400ns(c);
    uint8_t s = porta_le8(c->base + REG_CMD);
    if (s == 0 || s == 0xFF) return 0;                     /* no existe */
    if (esperar_livre(c) < 0) return 0;
    uint8_t m1 = porta_le8(c->base + REG_LBA1), m2 = porta_le8(c->base + REG_LBA2);
    if ((m1 == 0x14 && m2 == 0xEB) || (m1 == 0x69 && m2 == 0x96)) {   /* firma ATAPI (PATA o SATA) */
        klog("[ATA] %x/%d: dispositivo ATAPI (firma %x %x): CD/DVD (D207)\n", c->base, unidade, m1, m2);
        return 2;
    }
    if (m1 || m2) {                                        /* SATA de otro tipo */
        klog("[ATA] %x/%d: dispositivo de firma desconocida (%x %x) ignorado\n", c->base, unidade, m1, m2);
        return 0;
    }
    if (esperar_dados(c) < 0) return 0;
    for (int i = 0; i < 256; i++) id[i] = porta_le16(c->base + REG_DADOS);
    return 1;
}

/* ------------------------------------------------------------ ATAPI (D207) */

/* Espera a que BSY baje, como mucho "ms" milisegundos. Con i0 (las interrupciones
 * estaban encendidas al entrar) se encienden mientras se espera: un lector girando el
 * disco tarda segundos y el resto del sistema no tiene por qué pararse. El canal no
 * genera interrupciones (nIEN) y quien llama tiene el cerrojo de los discos. */
static int atapi_esperar_livre(canal_t *c, uint32_t ms, bool i0)
{
    if (i0) int_restaura(true);
    prazo_t p = prazo_ms(ms);
    int r = -1;
    for (;;) {
        uint8_t s = porta_le8(c->ctrl);
        if (s == 0xFF) break;                    /* bus flotando */
        if (!(s & ST_BSY)) { r = s; break; }
        if (prazo_vencido(&p)) break;
    }
    if (i0) int_salva_desliga();
    return r;
}

/* Tras un tiempo agotado el lector puede quedarse a mitad de la orden: DEVICE RESET. */
static void atapi_reiniciar(canal_t *c, uint8_t unidade, bool i0)
{
    porta_escreve8(c->base + REG_DISP, (uint8_t)(0xA0 | unidade << 4));
    pausa_400ns(c);
    porta_escreve8(c->base + REG_CMD, CMD_RESET_DISP);
    pausa_400ns(c);
    atapi_esperar_livre(c, 3000, i0);
}

/* IDENTIFY PACKET DEVICE: 256 palabras, como el IDENTIFY de un disco. */
static bool identificar_pacote(canal_t *c, uint8_t unidade, uint16_t *id)
{
    porta_escreve8(c->base + REG_DISP, (uint8_t)(0xA0 | unidade << 4));
    pausa_400ns(c);
    if (atapi_esperar_livre(c, 1000, false) < 0) return false;
    porta_escreve8(c->base + REG_CMD, CMD_ID_PACKET);
    pausa_400ns(c);
    int s = atapi_esperar_livre(c, 2000, false);
    if (s < 0 || (s & ST_ERR) || !(s & ST_DRQ)) {
        klog("[ATAPI] IDE %x/%d: IDENTIFY PACKET falló (estado %x, error %x)\n", c->base, unidade,
             s < 0 ? 0xFF : s, porta_le8(c->base + REG_ERRO));
        return false;
    }
    for (int i = 0; i < 256; i++) id[i] = porta_le16(c->base + REG_DADOS);
    porta_le8(c->base + REG_CMD);
    return true;
}

/* Un paquete por PIO (ATA/ATAPI-7, protocolo PACKET): la orden 0xA0 con el límite de
 * bytes por bloque de datos en LBA1/LBA2; con DRQ, los 12 bytes del paquete; después,
 * mientras el lector pida DRQ, un bloque de datos de los bytes que diga LBA1/LBA2. */
static int pata_pacote(atapi_t *a, const uint8_t *cdb, void *buf, uint32_t bytes, uint32_t espera_ms)
{
    canal_t *c = a->transporte;
    uint8_t u = (uint8_t)a->unidade;
    uint8_t *dest = buf;
    uint32_t feito = 0;
    int r = ATAPI_FALHA;
    trava_pegar(&c->trava);   /* D281: el otro disco del canal puede estar haciendo DMA */
    bool i0 = int_salva_desliga();
    porta_escreve8(c->ctrl, 0x02);                                    /* nIEN */
    porta_escreve8(c->base + REG_DISP, (uint8_t)(0xA0 | u << 4));
    pausa_400ns(c);
    int s = atapi_esperar_livre(c, 1000, i0);
    if (s < 0) goto falha;
    if (s & ST_DRQ) {   /* una orden anterior quedó a medias */
        klog("[ATAPI] %s: DRQ antes de la orden (estado %x): reinicio del lector\n", a->onde, s);
        goto falha;
    }
    porta_escreve8(c->base + REG_ERRO, 0);                            /* features: PIO, sin DMA */
    porta_escreve8(c->base + REG_QTD, 0);
    porta_escreve8(c->base + REG_LBA0, 0);
    porta_escreve8(c->base + REG_LBA1, 0x00);                         /* límite: 0xF800 bytes por DRQ */
    porta_escreve8(c->base + REG_LBA2, 0xF8);
    porta_escreve8(c->base + REG_CMD, CMD_PACKET);
    pausa_400ns(c);
    /* El lector pide el paquete con DRQ (o dice ERR si no admite la orden). */
    prazo_t p = prazo_ms(1000);
    for (;;) {
        s = porta_le8(c->ctrl);
        if (s == 0xFF) goto falha;
        if (!(s & ST_BSY)) {
            if (s & ST_ERR) { a->erro_reg = porta_le8(c->base + REG_ERRO); r = ATAPI_CHECK; goto fim; }
            if (s & ST_DRQ) break;
        }
        if (prazo_vencido(&p)) goto falha;
    }
    {
        uint16_t pal[8] = { 0 };
        memcpy(pal, cdb, 12);
        int n = a->pacote16 ? 8 : 6;
        for (int i = 0; i < n; i++) porta_escreve16(c->base + REG_DADOS, pal[i]);
    }
    for (;;) {
        pausa_400ns(c);
        s = atapi_esperar_livre(c, espera_ms, i0);
        if (s < 0) goto falha;
        if (s & ST_ERR) { a->erro_reg = porta_le8(c->base + REG_ERRO); r = ATAPI_CHECK; goto fim; }
        if (!(s & ST_DRQ)) break;                                     /* terminado */
        uint32_t cnt = porta_le8(c->base + REG_LBA1) | (uint32_t)porta_le8(c->base + REG_LBA2) << 8;
        uint8_t motivo = porta_le8(c->base + REG_QTD);                 /* bit 1: del lector al host */
        if (!cnt || !(motivo & 2)) {
            klog("[ATAPI] %s: bloque de datos raro (%u bytes, motivo %x)\n", a->onde, cnt, motivo);
            goto falha;
        }
        uint32_t palavras = (cnt + 1) / 2;
        if (!(cnt & 1) && dest && feito + cnt <= bytes) {
            uint16_t *q = (uint16_t *)(dest + feito);
            uint32_t k = palavras;
            __asm__ volatile("cld; rep insw" : "+D"(q), "+c"(k) : "d"(c->base + REG_DADOS) : "memory");
            feito += cnt;
        } else {
            for (uint32_t i = 0; i < palavras; i++) {               /* lo que sobra se descarta */
                uint16_t w = porta_le16(c->base + REG_DADOS);
                if (dest && feito < bytes) dest[feito] = (uint8_t)w;
                if (dest && feito + 1 < bytes) dest[feito + 1] = (uint8_t)(w >> 8);
                feito += 2;
            }
        }
    }
    r = (int)(feito > bytes ? bytes : feito);
    goto fim;
falha:
    klog("[ATAPI] %s: orden %x sin respuesta (estado %x, error %x): reinicio del lector\n", a->onde, cdb[0],
         porta_le8(c->ctrl), porta_le8(c->base + REG_ERRO));
    atapi_reiniciar(c, u, i0);
    r = ATAPI_FALHA;
fim:
    porta_le8(c->base + REG_CMD);                                     /* estado normal: limpia la interrupción */
    int_restaura(i0);
    trava_soltar(&c->trava);
    return r;
}

/* Prepara un comando de lectura/grabación de "n" sectores (1..256, o 65536 con LBA48).
 * "dma": las órdenes de DMA (C8/25 y CA/35) en vez de las de PIO (20/24 y 30/34). */
static int comando(ata_disco_t *a, uint32_t lba, uint32_t n, bool gravar, bool dma)
{
    canal_t *c = a->canal;
    if (esperar_livre(c) < 0) return -1;
    bool ext = a->lba48 && (lba + n > 0x0FFFFFFFu || n > 256);
    uint8_t op28 = dma ? (gravar ? CMD_GRAVAR_DMA : CMD_LER_DMA) : (gravar ? CMD_GRAVAR : CMD_LER);
    uint8_t op48 = dma ? (gravar ? CMD_GRAVAR_DMA_EXT : CMD_LER_DMA_EXT) : (gravar ? CMD_GRAVAR_EXT : CMD_LER_EXT);
    if (ext) {
        porta_escreve8(c->base + REG_DISP, (uint8_t)(0x40 | a->unidade << 4));
        pausa_400ns(c);
        porta_escreve8(c->base + REG_QTD, (uint8_t)(n >> 8));
        porta_escreve8(c->base + REG_LBA0, (uint8_t)(lba >> 24));
        porta_escreve8(c->base + REG_LBA1, 0);
        porta_escreve8(c->base + REG_LBA2, 0);
        porta_escreve8(c->base + REG_QTD, (uint8_t)n);
        porta_escreve8(c->base + REG_LBA0, (uint8_t)lba);
        porta_escreve8(c->base + REG_LBA1, (uint8_t)(lba >> 8));
        porta_escreve8(c->base + REG_LBA2, (uint8_t)(lba >> 16));
        porta_escreve8(c->base + REG_CMD, op48);
    } else if (a->chs) {
        /* D330: el LBA del disco se pasa a CHS con la geometría actual del disco. */
        uint32_t por_cil = (uint32_t)a->cabecas * a->setpista;
        uint32_t cil = lba / por_cil, resto = lba % por_cil;
        if (cil > 0xFFFFu) return -1;                       /* fuera del alcance del CHS */
        porta_escreve8(c->base + REG_DISP, (uint8_t)(0xA0 | a->unidade << 4 | (resto / a->setpista)));
        pausa_400ns(c);
        porta_escreve8(c->base + REG_QTD, (uint8_t)n);      /* 256 pasa a 0 */
        porta_escreve8(c->base + REG_LBA0, (uint8_t)(resto % a->setpista + 1));
        porta_escreve8(c->base + REG_LBA1, (uint8_t)cil);
        porta_escreve8(c->base + REG_LBA2, (uint8_t)(cil >> 8));
        porta_escreve8(c->base + REG_CMD, op28);
    } else {
        porta_escreve8(c->base + REG_DISP, (uint8_t)(0xE0 | a->unidade << 4 | ((lba >> 24) & 0x0F)));
        pausa_400ns(c);
        porta_escreve8(c->base + REG_QTD, (uint8_t)n);      /* 256 pasa a 0 */
        porta_escreve8(c->base + REG_LBA0, (uint8_t)lba);
        porta_escreve8(c->base + REG_LBA1, (uint8_t)(lba >> 8));
        porta_escreve8(c->base + REG_LBA2, (uint8_t)(lba >> 16));
        porta_escreve8(c->base + REG_CMD, op28);
    }
    pausa_400ns(c);
    return ext;
}

/* ------------------------------------------------------------ DMA (D281) */

static bool dma_desligado;       /* mkimagem.py --sem-dma (CAB_DISCO_OPC bit 0): todo por PIO */
static bool ata_assincrono;      /* hay planificador e IRQ: la espera puede dormir */
static uint32_t dma_ordens, dma_setores, dma_irqs, dma_sondeos, dma_falhas, dma_sono;
static uint64_t dma_tsc;         /* TSC gastado dentro de las órdenes de DMA */

/* Reserva el búfer de 64 KB (alineado a 64 KB: el límite que una entrada PRD no puede cruzar)
 * y la tabla PRD de un canal. */
static bool dma_preparar_canal(canal_t *c)
{
    if (c->dma_ok) return true;
    if (!c->bm) return false;
    uint32_t base = memfisica_alocar_seguidas(32);      /* 128 KB: dentro hay un bloque de 64 KB alineado */
    uint32_t prd = memfisica_alocar();
    if (!base || !prd) {
        if (base) memfisica_liberar_seguidas(base, 32);
        if (prd) memfisica_liberar(prd);
        return false;
    }
    uint32_t alinhado = (base + 0xFFFFu) & ~0xFFFFu;
    c->buf = (uint8_t *)alinhado;
    c->prd = (uint32_t *)prd;
    memset(c->prd, 0, PAGINA);
    c->dma_ok = true;
    return true;
}

/* Si el disco no tiene un modo DMA activo (la BIOS no lo dejó puesto), se elige uno prudente:
 * UDMA hasta el modo 2 (33 MB/s, vale con cable de 40 hilos) o, si no, MWDMA. */
static bool dma_modo_disco(canal_t *c, ata_disco_t *a, const uint16_t *id)
{
    if (!(id[49] & (1 << 8))) return false;
    if ((id[88] >> 8) || ((id[63] >> 8) & 7)) return true;      /* ya hay un modo seleccionado */
    uint8_t modo;
    if ((id[53] & 4) && (id[88] & 7)) modo = (uint8_t)(0x40 | ((id[88] & 4) ? 2 : (id[88] & 2) ? 1 : 0));
    else if (id[63] & 7) modo = (uint8_t)(0x20 | ((id[63] & 4) ? 2 : (id[63] & 2) ? 1 : 0));
    else return false;
    porta_escreve8(c->base + REG_DISP, (uint8_t)(0xA0 | a->unidade << 4));
    pausa_400ns(c);
    if (esperar_livre(c) < 0) return false;
    porta_escreve8(c->base + REG_ERRO, 0x03);               /* subcomando: fijar el modo de transferencia */
    porta_escreve8(c->base + REG_QTD, modo);
    porta_escreve8(c->base + REG_CMD, CMD_SET_FEATURES);
    pausa_400ns(c);
    int s = esperar_livre(c);
    if (s < 0 || (s & (ST_ERR | ST_DF))) return false;
    klog("[ATA] %x/%d: modo de transferencia %x fijado por SET FEATURES\n", c->base, a->unidade, modo);
    return true;
}

/* La IRQ del canal: se anota qué vio el Bus Master y el disco (leer el estado del disco quita
 * su interrupción) y se despierta al que espera. Si está corriendo el ocioso, se cambia ya de
 * proceso: esperar al próximo tic limitaría el DMA a 100 órdenes por segundo. */
static quadro_t *ata_irq(quadro_t *q)
{
    bool acordou = false;
    for (int i = 0; i < MAX_CANAIS; i++) {
        canal_t *c = &canais[i];
        if (!c->usado || !c->dma_ok) continue;
        uint8_t st = porta_le8(c->bm + BM_ST);
        if (!(st & BM_ST_IRQ)) continue;
        porta_escreve8(c->bm + BM_ST, (uint8_t)((st & 0x60) | BM_ST_IRQ));   /* quita solo la IRQ */
        c->bm_st = st;
        c->ata_st = porta_le8(c->base + REG_CMD);
        c->irq_chegou = true;
        dma_irqs++;
        acordar(c);
        acordou = true;
    }
    if (acordou && processo_atual() && processo_atual()->pid == 0) return escalonar(q);
    return q;
}

static bool pode_dormir(void)
{
    return ata_assincrono && processo_atual() && processo_atual()->pid != 0 && !contexto_irq();
}

/* Registra las IRQ de los canales con DMA (cuando ya hay planificador): primero las líneas
 * libres con manipulador propio; si el dueño es otro, como atendedor extra. */
void ata_modo_assincrono(bool ligado)
{
    if (ligado) {
        uint16_t feitas = 0;
        for (int i = 0; i < MAX_CANAIS; i++) {
            canal_t *c = &canais[i];
            if (!c->usado || !c->dma_ok || !c->irq || c->irq >= 16 || c->irq == 2) continue;
            c->irq_usavel = true;
            if (feitas & (1u << c->irq)) continue;
            feitas |= (uint16_t)(1u << c->irq);
            if (!irq_ocupada(c->irq)) irq_registrar(c->irq, ata_irq);
            else irq_registrar_extra(c->irq, ata_irq);
            klog("[ATA] canal %x: DMA con interrupcion en la IRQ %u\n", c->base, c->irq);
        }
    }
    ata_assincrono = ligado;
}

/* Detiene el Bus Master y limpia sus bits de estado. */
static void dma_parar(canal_t *c)
{
    porta_escreve8(c->bm + BM_CMD, 0);
    uint8_t st = porta_le8(c->bm + BM_ST);
    porta_escreve8(c->bm + BM_ST, (uint8_t)((st & 0x60) | BM_ST_ERRO | BM_ST_IRQ));
}

/* Una orden de DMA de "n" (<= 128) sectores desde/hacia el búfer del canal. 0 = bien. */
static int dma_orden(ata_disco_t *a, uint32_t lba, uint32_t n, bool gravar)
{
    canal_t *c = a->canal;
    uint64_t t0 = prazo_tsc();
    bool i0 = int_salva_desliga();
    bool por_irq = c->irq_usavel && pode_dormir();
    int r = -1;
    uint32_t bytes = n * 512u;

    c->prd[0] = (uint32_t)c->buf;
    c->prd[1] = (bytes & 0xFFFFu) | 0x80000000u;             /* 64 KB se codifican como 0; fin de tabla */
    porta_escreve32(c->bm + BM_PRD, (uint32_t)c->prd);
    dma_parar(c);
    porta_escreve8(c->bm + BM_CMD, gravar ? 0 : BM_CMD_LER);   /* el sentido, sin arrancar */
    c->irq_chegou = false;
    porta_escreve8(c->ctrl, por_irq ? 0x00 : 0x02);           /* nIEN: sin IRQ salvo si se va a dormir */
    if (comando(a, lba, n, gravar, true) < 0) goto fim;
    porta_escreve8(c->bm + BM_CMD, (uint8_t)((gravar ? 0 : BM_CMD_LER) | BM_CMD_INICIA));

    prazo_t p = prazo_ms(15000);
    bool dormiu = false;
    for (;;) {
        uint8_t st, est;
        if (c->irq_chegou) { st = c->bm_st; est = c->ata_st; goto terminou; }
        st = porta_le8(c->bm + BM_ST);
        est = porta_le8(c->ctrl);
        if (st & BM_ST_ERRO) goto fim;
        if ((st & BM_ST_IRQ) || (!(st & BM_ST_ATIVO) && !(est & ST_BSY))) {
            if (por_irq && dormiu && !c->irq_chegou && ++c->irq_falhas >= 3) {
                c->irq_usavel = false;
                klog("[ATA] canal %x: la IRQ no llega; el DMA espera cediendo la CPU\n", c->base);
            }
            est = porta_le8(c->base + REG_CMD);                /* quita la IRQ del disco */
            goto terminou;
        }
        if (est == 0xFF || prazo_vencido(&p)) goto fim;
        if (por_irq) {
            if (!c->irq_chegou) {
                bloquear(c);
                processo_atual()->acordar_em = relogio_tiques() + 1;
                dma_sono++;
                ceder();
                processo_atual()->acordar_em = 0;
                dormiu = true;
            }
        } else if (pode_dormir()) {
            ceder();                                           /* sin IRQ útil: se cede el turno */
            dma_sono++;
        }
        continue;
terminou:
        if (est & (ST_ERR | ST_DF)) goto fim;
        r = 0;
        break;
    }
    if (r == 0 && !c->irq_chegou) dma_sondeos++;
fim:
    dma_parar(c);
    porta_escreve8(c->ctrl, 0x02);
    int_restaura(i0);
    dma_tsc += prazo_tsc() - t0;
    if (r == 0) { dma_ordens++; dma_setores += n; }
    return r;
}

/* Lo que va quedando de un canal con DMA que falló: se anota y el disco sigue por PIO. */
static void dma_falhou(ata_disco_t *a, uint32_t lba)
{
    canal_t *c = a->canal;
    dma_falhas++;
    klog("[ATA] %s: fallo del DMA en el sector %u (estado %x, bm %x, error %x): el disco vuelve a PIO\n",
         a->disco ? a->disco->nome : "?", lba, porta_le8(c->ctrl), porta_le8(c->bm + BM_ST), porta_le8(c->base + REG_ERRO));
    a->dma = false;
    dma_parar(c);
    /* El disco pudo quedarse a mitad de orden: se espera a que suelte BSY y se limpia. */
    esperar_livre(c);
    porta_le8(c->base + REG_CMD);
}

static inline bool usa_dma(ata_disco_t *a)
{
    return a->dma && a->canal->dma_ok && !dma_desligado;
}

/* D245: entre sector y sector se dejan pasar las interrupciones pendientes (tic, sonido,
 * compositor) en lugar de mantenerlas apagadas durante todo el bloque. Quien llama tiene el
 * cerrojo del canal y este está en nIEN, así que nadie más toca el controlador.
 * "ceder" es el estado de IF al entrar; ata_cedidas cuenta las cesiones (para las pruebas). */
static uint32_t ata_cedidas;
uint32_t ata_cesiones(void) { return ata_cedidas; }
static inline void ata_ceder(bool ceder)
{
    if (!ceder) return;
    ata_cedidas++;
    __asm__ volatile("sti; nop; nop; cli" ::: "memory");   /* el nop tras sti deja entrar la interrupción */
}

/* Estadísticas del DMA para el log y la prueba. */
void ata_estatisticas(uint32_t *ordens, uint32_t *setores, uint32_t *irqs, uint32_t *falhas, uint32_t *tsc_ms)
{
    uint32_t khz = hw_tsc_khz();
    if (ordens) *ordens = dma_ordens;
    if (setores) *setores = dma_setores;
    if (irqs) *irqs = dma_irqs;
    if (falhas) *falhas = dma_falhas;
    if (tsc_ms) *tsc_ms = khz ? (uint32_t)(dma_tsc / khz) : 0;
}

/* Lectura por PIO de "n" sectores (el camino de siempre, D245). */
static int ler_pio(disco_t *k, ata_disco_t *a, uint32_t lba, uint32_t n, uint16_t *p)
{
    bool i0 = int_salva_desliga();
    int r = 0;
    uint32_t n0 = n, ces0 = ata_cedidas, tic0 = relogio_tiques();
    while (n && r == 0) {
        uint32_t parte = n > 256 ? 256 : n;
        if (comando(a, lba, parte, false, false) < 0) { r = -1; break; }
        for (uint32_t s = 0; s < parte; s++) {
            if (esperar_dados(a->canal) < 0) { r = -1; break; }
            uint32_t cnt = 256;
            __asm__ volatile("cld; rep insw" : "+D"(p), "+c"(cnt) : "d"(a->canal->base + REG_DADOS) : "memory");
            ata_ceder(i0);
        }
        lba += parte; n -= parte;
    }
    int_restaura(i0);
    /* D245: para la prueba (y para diagnosticar un PC real): cada 8192 sectores leídos se anota
     * cuántos tics (acumulados) pasaron dentro de las lecturas y cuántas veces se dejaron pasar las
     * interrupciones (primeras 8 anotaciones). */
    static uint32_t acum_set, acum_tic, acum_ces;
    static int avisos;
    if (r == 0) {
        acum_set += n0; acum_tic += relogio_tiques() - tic0; acum_ces += ata_cedidas - ces0;
        if (acum_set >= 8192 && avisos < 8) {
            avisos++;
            klog("[ATA] %s: lectura de %u sectores, %u tics, %u cesiones de CPU\n", k->nome, acum_set, acum_tic, acum_ces);
            acum_set = 0;   /* tics y cesiones siguen sumando */
        }
    }
    if (r < 0) klog("[ATA] %s: erro de leitura no setor %u (estado %x, erro %x)\n", k->nome, lba,
                    porta_le8(a->canal->ctrl), porta_le8(a->canal->base + REG_ERRO));
    return r;
}

/* Grabación por PIO; "ext" dice si alguna orden fue LBA48 (para elegir el FLUSH). */
static int gravar_pio(disco_t *k, ata_disco_t *a, uint32_t lba, uint32_t n, const uint16_t *p, bool *ext)
{
    bool i0 = int_salva_desliga();
    int r = 0;
    while (n && r == 0) {
        uint32_t parte = n > 256 ? 256 : n;
        int e = comando(a, lba, parte, true, false);
        if (e < 0) { r = -1; break; }
        *ext |= e;
        for (uint32_t s = 0; s < parte; s++) {
            if (esperar_dados(a->canal) < 0) { r = -1; break; }
            uint32_t cnt = 256;
            __asm__ volatile("cld; rep outsw" : "+S"(p), "+c"(cnt) : "d"(a->canal->base + REG_DADOS) : "memory");
            ata_ceder(i0);
        }
        if (r == 0 && esperar_livre_flush(a->canal) < 0) r = -1;
        lba += parte; n -= parte;
    }
    int_restaura(i0);
    if (r < 0) klog("[ATA] %s: erro de gravacao no setor %u (estado %x, erro %x)\n", k->nome, lba,
                    porta_le8(a->canal->ctrl), porta_le8(a->canal->base + REG_ERRO));
    return r;
}

static int ler(disco_t *k, uint32_t lba, uint32_t n, void *buf)
{
    ata_disco_t *a = k->dono;
    canal_t *c = a->canal;
    uint8_t *p = buf;
    int r = 0;
    trava_pegar(&c->trava);
    while (n && r == 0) {
        uint32_t parte = n;
        if (usa_dma(a)) {
            if (parte > DMA_SETORES) parte = DMA_SETORES;
            if (dma_orden(a, lba, parte, false) == 0) {
                memcpy(p, c->buf, parte * 512u);
                p += parte * 512u; lba += parte; n -= parte;
                continue;
            }
            dma_falhou(a, lba);
        }
        parte = n > 256 ? 256 : n;
        r = ler_pio(k, a, lba, parte, (uint16_t *)p);
        p += parte * 512u; lba += parte; n -= parte;
    }
    trava_soltar(&c->trava);
    return r;
}

static int escrever(disco_t *k, uint32_t lba, uint32_t n, const void *buf)
{
    ata_disco_t *a = k->dono;
    canal_t *c = a->canal;
    const uint8_t *p = buf;
    int r = 0;
    bool ext = a->lba48 && (lba + n > 0x0FFFFFFFu);
    trava_pegar(&c->trava);
    while (n && r == 0) {
        uint32_t parte = n;
        if (usa_dma(a)) {
            if (parte > DMA_SETORES) parte = DMA_SETORES;
            memcpy(c->buf, p, parte * 512u);
            if (dma_orden(a, lba, parte, true) == 0) {
                p += parte * 512u; lba += parte; n -= parte;
                continue;
            }
            dma_falhou(a, lba);
        }
        parte = n > 256 ? 256 : n;
        r = gravar_pio(k, a, lba, parte, (const uint16_t *)p, &ext);
        p += parte * 512u; lba += parte; n -= parte;
    }
    if (r == 0 && !a->sem_flush) {   /* saca de la caché del disco: apagar justo después no pierde nada */
        bool i0 = int_salva_desliga();
        porta_escreve8(c->ctrl, 0x02);
        porta_escreve8(c->base + REG_DISP, (uint8_t)(0xA0 | a->unidade << 4));
        pausa_400ns(c);
        porta_escreve8(c->base + REG_CMD, ext ? CMD_FLUSH_EXT : CMD_FLUSH);
        pausa_400ns(c);
        int s = esperar_livre_flush(c);
        /* D330: los discos de antes del ATA-4 (y el IDE de 86Box) no conocen el FLUSH CACHE y
         * lo abortan (ABRT). Si el disco no lo anuncia, eso no es un fallo de la grabación (que
         * ya terminó bien): se anota y no se vuelve a mandar. Si lo anuncia, sigue siendo error. */
        if (s >= 0 && (s & ST_ERR) && !(s & ST_DF) && !a->flush_anunciado &&
            (porta_le8(c->base + REG_ERRO) & 0x04)) {
            a->sem_flush = true;
            klog("[ATA] %s: el disco no admite FLUSH CACHE (no lo anuncia y lo aborta): se graba sin vaciar su cache (D330)\n", k->nome);
        } else if (s < 0 || (s & ST_ERR)) r = -1;
        int_restaura(i0);
    }
    trava_soltar(&c->trava);
    if (r < 0) klog("[ATA] %s: erro de gravacao (estado %x, erro %x)\n", k->nome, porta_le8(c->ctrl), porta_le8(c->base + REG_ERRO));
    return r;
}

static canal_t *acrescentar_canal(uint16_t base, uint16_t ctrl)
{
    for (int i = 0; i < MAX_CANAIS; i++)
        if (canais[i].usado && canais[i].base == base) return &canais[i];
    for (int i = 0; i < MAX_CANAIS; i++)
        if (!canais[i].usado) { canais[i] = (canal_t){ .base = base, .ctrl = ctrl, .usado = true }; return &canais[i]; }
    return NULL;
}

/* D330: un disco sin LBA. Toma la geometría por defecto del IDENTIFY (palabras 1, 3 y 6),
 * comprueba que es posible y la fija como la actual con INITIALIZE DEVICE PARAMETERS: la BIOS
 * pudo dejar otra (traducción) y, sin LBA, el disco no dice cuál (palabras 54-56 no válidas).
 * Devuelve los sectores del disco, o 0 si no se puede usar. Con las interrupciones apagadas. */
static uint32_t preparar_chs(canal_t *c, ata_disco_t *a, const uint16_t *id)
{
    uint32_t cil = id[1], cab = id[3], spt = id[6];
    if (!cil || !cab || cab > 16 || !spt || spt > 63) {
        klog("[ATA] %x/%d: sin LBA y con geometria imposible (%u/%u/%u), ignorado\n", c->base, a->unidade, cil, cab, spt);
        return 0;
    }
    porta_escreve8(c->base + REG_DISP, (uint8_t)(0xA0 | a->unidade << 4 | (cab - 1)));
    pausa_400ns(c);
    if (esperar_livre(c) < 0) return 0;
    porta_escreve8(c->base + REG_QTD, (uint8_t)spt);
    porta_escreve8(c->base + REG_CMD, CMD_INIC_PARAMS);
    pausa_400ns(c);
    int s = esperar_livre(c);
    if (s < 0 || (s & (ST_ERR | ST_DF))) {
        klog("[ATA] %x/%d: sin LBA y no acepta la geometria %u/%u/%u (estado %x), ignorado\n",
             c->base, a->unidade, cil, cab, spt, s < 0 ? 0xFF : s);
        return 0;
    }
    a->chs = true;
    a->cabecas = (uint8_t)cab;
    a->setpista = (uint8_t)spt;
    return cil * cab * spt;
}

static void copiar_texto_ata(char *dest, const uint16_t *id, int ini, int palavras, int max)
{
    int k = 0;
    for (int i = 0; i < palavras && k + 2 < max; i++) {   /* cada palabra trae dos caracteres intercambiados */
        dest[k++] = (char)(id[ini + i] >> 8);
        dest[k++] = (char)(id[ini + i] & 0xFF);
    }
    while (k > 0 && (dest[k - 1] == ' ' || dest[k - 1] == 0)) k--;
    dest[k] = 0;
}

int ata_iniciar(void)
{
    /* Controladores en el PCI: IDE (nativo o compatible). Los AHCI (SATA)
     * los lleva el driver de ahci.c, que se llama al final. */
    pci_disp_t achados[4];
    int n = pci_procurar_classe(0x01, 0x01, achados, 4);
    for (int i = 0; i < n; i++) {
        uint8_t pi = achados[i].interface;
        klog("[ATA] controlador IDE %x:%x no PCI %u:%u.%u, interface %x\n", achados[i].fabricante, achados[i].modelo,
             achados[i].barramento, achados[i].dispositivo, achados[i].funcao, pi);
        /* D281: el Bus Master (BAR4, 8 puertos por canal) y la IRQ de cada canal. */
        uint32_t b4 = pci_le32(&achados[i], 0x20);
        uint16_t bm = (b4 & 1) ? (uint16_t)(b4 & ~3u) : 0;
        uint8_t linha = (uint8_t)(pci_le32(&achados[i], 0x3C) & 0xFF);
        if (bm && !dma_desligado)   /* activar el Bus Master en el PCI (sin tocar los bits de estado) */
            pci_escreve32(&achados[i], 0x04, (pci_le32(&achados[i], 0x04) & 0xFFFFu) | 0x5);
        for (int ch = 0; ch < 2; ch++) {
            bool nativo = pi & (1 << (ch * 2));
            canal_t *cn = NULL;
            if (nativo) {
                uint32_t b0 = pci_le32(&achados[i], (uint8_t)(0x10 + ch * 8)), b1 = pci_le32(&achados[i], (uint8_t)(0x14 + ch * 8));
                if ((b0 & 1) && (b1 & 1)) cn = acrescentar_canal((uint16_t)(b0 & ~3u), (uint16_t)((b1 & ~3u) + 2));
            } else {
                cn = acrescentar_canal(ch ? 0x170 : 0x1F0, ch ? 0x376 : 0x3F6);
            }
            if (cn && bm && !cn->bm) {
                cn->bm = (uint16_t)(bm + ch * 8);
                cn->irq = nativo ? linha : (uint8_t)(14 + ch);
                if (cn->irq == 0xFF) cn->irq = 0;
            }
        }
    }
    /* Los canales clásicos existen incluso sin PCI (o con el controlador oculto). */
    acrescentar_canal(0x1F0, 0x3F6);
    acrescentar_canal(0x170, 0x376);

    dma_desligado = (cabecalho_boot[CAB_DISCO_OPC] & CAB_DISCO_SEM_DMA) != 0;
    if (dma_desligado) klog("[ATA] DMA desactivado (--sem-dma): todo por PIO\n");
    uint16_t *id = kmalloc(512);
    if (!id) return 0;
    /* D207: los lectores ATAPI se registran al final (su preparación puede tardar). */
    struct { canal_t *c; uint8_t u; uint16_t *id; } cds[MAX_CANAIS * 2];
    int qtd_cds = 0;
    bool i0 = int_salva_desliga();
    for (int i = 0; i < MAX_CANAIS; i++) {
        canal_t *c = &canais[i];
        if (!c->usado || porta_le8(c->base + REG_CMD) == 0xFF) continue;   /* canal vacío */
        for (uint8_t u = 0; u < 2; u++) {
            int t = identificar(c, u, id);
            if (t == 2) {
                uint16_t *x = kmalloc(512);
                if (x && identificar_pacote(c, u, x)) { cds[qtd_cds].c = c; cds[qtd_cds].u = u; cds[qtd_cds].id = x; qtd_cds++; }
                else if (x) kfree(x);
                continue;
            }
            if (!t) continue;
            ata_disco_t *a = &unidades[qtd_unidades];
            a->canal = c;
            a->unidade = u;
            a->chs = false;
            a->sem_flush = false;
            a->flush_anunciado = (id[83] & 0xC000) == 0x4000 && (id[83] & (1 << 12));
            a->lba48 = (id[49] & (1 << 9)) && (id[83] & (1 << 10)) != 0;
            uint32_t s28 = (uint32_t)id[60] | (uint32_t)id[61] << 16;
            uint32_t s48 = (uint32_t)id[100] | (uint32_t)id[101] << 16;
            bool grande = id[102] || id[103];
            a->setores = a->lba48 ? (grande ? 0xFFFFFFFFu : s48) : s28;
            if (!a->setores) a->setores = s28;
            if (!(id[49] & (1 << 9))) a->setores = preparar_chs(c, a, id);   /* D330 */
            if (!a->setores) { klog("[ATA] %x/%d: sem LBA, ignorado\n", c->base, u); continue; }
            char modelo[48];
            copiar_texto_ata(modelo, id, 27, 20, sizeof modelo);
            /* D179: el TRIM (DATA SET MANAGEMENT) solo existe por DMA y este driver es PIO:
             * se anota que el disco lo admite, pero no se manda. */
            bool trim = (id[169] & 1) != 0;
            if (a->chs)
                klog("[ATA] %x/%d: \"%s\", %u setores, CHS %u/%u/%u (sin LBA, D330)\n", c->base, u, modelo, a->setores,
                     id[1], a->cabecas, a->setpista);
            else
                klog("[ATA] %x/%d: \"%s\", %u setores, LBA%s%s\n", c->base, u, modelo, a->setores, a->lba48 ? "48" : "28",
                     grande ? " (limitado a 2 TB)" : "");
            if (!dma_desligado && c->bm && dma_preparar_canal(c)) {
                a->dma = dma_modo_disco(c, a, id);
                klog("[ATA] %x/%d: %s (Bus Master %x, IRQ %u)\n", c->base, u, a->dma ? "DMA" : "solo PIO", c->bm, c->irq);
            }
            qtd_unidades++;
            int_restaura(i0);
            a->disco = disco_registrar("hd", "ATA", modelo, a->setores, 512, a, false, ler, escrever);
            if (a->disco && trim) a->disco->trim = TRIM_SEM_DRIVER;
            i0 = int_salva_desliga();
        }
    }
    int_restaura(i0);
    kfree(id);
    for (int i = 0; i < qtd_cds; i++) {
        atapi_t m;
        memset(&m, 0, sizeof m);
        m.transporte = cds[i].c;
        m.unidade = cds[i].u;
        m.pacote = pata_pacote;
        m.pacote16 = (cds[i].id[0] & 3) == 1;
        snformatar(m.onde, sizeof m.onde, "IDE %x/%d", cds[i].c->base, cds[i].u);
        atapi_registrar(&m, cds[i].id);
        kfree(cds[i].id);
    }
    /* D155: los discos SATA (controladores AHCI) entran en la misma capa de
     * discos, con el mismo prefijo ("hda", "hdb"...). */
    int sata = ahci_iniciar();
    /* D265: los discos NVMe (PCIe), también por la capa de discos ("nvme0"...). */
    int nvme = nvme_iniciar();
    return qtd_unidades + sata + nvme;
}

/* Cuántos controladores AHCI se han visto en el PCI. */
int ata_ahci_quantidade(void) { return ahci_controladores(); }

/* Cuántos controladores AHCI siguen sin soporte: los que el driver no ha
 * podido encender (sin barra de memoria, etc.). Con el driver en marcha es 0. */
int ata_ahci_sem_suporte(void) { return ahci_sem_suporte(); }
