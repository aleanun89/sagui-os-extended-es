// SPDX-License-Identifier: GPL-2.0-or-later
/* ahci.c - driver del controlador SATA AHCI (D155).
 *
 * El AHCI es el modo "nativo" del SATA: el controlador es un dispositivo del
 * PCI con una barra de memoria (ABAR) donde están sus registros, y no se
 * habla con el disco por los puertos de E/S del ATA, sino con "FIS" (Frame
 * Information Structure):
 *
 *   - el host copia un comando ATA dentro de un FIS de registro H2D, en una
 *     tabla de comandos (CT) que va colgada de la lista de comandos (CLB);
 *   - la lista de descriptores (PRDT) de esa tabla dice dónde están los
 *     datos y cómo los copia el controlador por DMA;
 *   - el disco responde con un FIS de registro D2H, que se escribe en el
 *     área de recepción de FIS (FB) de ese puerto.
 *
 * Las tres estructuras están en memoria del kernel: el controlador las ve
 * por DMA, con direcciones físicas (el kernel ve la memoria física 1:1,
 * D008), como en el driver EHCI (H29).
 *
 * El arranque va por polling, como el driver ATA por PIO (D040): se espera a
 * que el controlador baje el bit "comando en curso" (PxCI). Las
 * interrupciones del controlador (INTx o MSI-X) no hacen falta para leer y no
 * se configuran todavía; por eso el bit de interrupciones globales (GHC.IE)
 * se deja apagado y el driver no pide ninguna IRQ.
 *
 * D207: los lectores de CD/DVD (SATAPI, firma 0xEB140101) se leen con la orden PACKET:
 * el bit ATAPI (A) de la cabecera de orden y el paquete SCSI en el ACMD de la tabla de
 * comandos (desplazamiento 0x40). La parte común está en atapi.c. Un error del lector
 * (CHECK CONDITION, lo normal tras un cambio de disco) no reinicia el enlace (COMRESET):
 * se para y se vuelve a arrancar el motor del puerto (AHCI 1.3, 6.2.2), para no perder
 * el sense que hay que pedir después.
 *
 * SEGURIDAD: como los discos ATA (D040), un disco interno SATA se registra
 * en la capa de discos sólo para lectura; escribir en él exige un
 * "mount -o rw" explícito (lo comprueba disco.c).
 */
#include "ahci.h"
#include "atapi.h"
#include "abi.h"
#include "console.h"
#include "cpu.h"
#include "disco.h"
#include "memoria.h"
#include "paginacao.h"
#include "pci.h"
#include "portas.h"
#include "texto.h"
#include "info_hardware.h"

/* Registros del HBA (capítulo 3 de la especificación AHCI). */
#define HBA_CAP   0x00   /* capacidades */
#define HBA_GHC   0x04   /* control general */
#define HBA_IS    0x08   /* estado de interrupciones */
#define HBA_PI    0x0C   /* puertos implementados */
#define HBA_VS    0x10   /* versión de la especificación */

#define GHC_HRST  (1u << 0)   /* reiniciar el controlador (se borra solo) */
#define GHC_AE    (1u << 31)  /* AHCI habilitado */

/* Registros de cada puerto (0x100 + puerto * 0x80). */
#define PORTA_BASE  0x100
#define PORTA_PASSO 0x80
#define P_CLB   0x00   /* lista de comandos */
#define P_CLBU  0x04   /* (64 bits) */
#define P_FB    0x08   /* área de recepción de FIS */
#define P_FBU   0x0C   /* (64 bits) */
#define P_IS    0x10   /* estado de interrupciones del puerto */
#define P_CMD   0x18   /* control del puerto */
#define P_TFD   0x20   /* registros de tareas: estado (BSY, DRQ, ERR) y error */
#define P_SIG   0x24   /* firma del dispositivo */
#define P_SSTS  0x28   /* estado del enlace SATA */
#define P_SCTL  0x2C   /* control del enlace SATA */
#define P_SERR  0x30   /* errores del enlace SATA (se borran escribiendo 1) */
#define P_CI    0x38   /* comandos emitidos */

#define CMD_ST      (1u << 0)   /* arrancar el motor de DMA */
#define CMD_SPIN    (1u << 1)   /* girar el disco */
#define CMD_PON     (1u << 2)   /* dar corriente */
#define CMD_FRE     (1u << 4)   /* habilitar la recepción de FIS */
#define CMD_FIS_ON  (1u << 14)  /* (solo lectura) motor de FIS en marcha */
#define CMD_LIST_ON (1u << 15)  /* (solo lectura) motor de DMA en marcha */

#define SSTS_DET 0xFu        /* detección: 1 = hay algo, 3 = enlace establecido */
#define TFD_BSY  0x80u
#define TFD_DRQ  0x08u

#define FIS_H2D 0x27   /* FIS de registro del host al disco */
#define FIS_D2H 0x34   /* FIS de registro del disco al host */

#define CMD_IDENTIFY  0xEC
#define CMD_PACKET     0xA0   /* D207: orden ATAPI */
#define CMD_ID_PACKET  0xA1   /* D207: IDENTIFY PACKET DEVICE */
#define ESPERA_ATAPI_MS 15000u   /* D207: una lectura del CD con el disco parado tarda en girar */
#define CMD_READ_EXT   0x25
#define CMD_WRITE_EXT  0x35
#define CMD_FLUSH_EXT  0xEA   /* saca de la caché del disco lo escrito (D179) */
#define CMD_DSM        0x06   /* DATA SET MANAGEMENT; con features = 1, TRIM (D179) */

#define TAM_SETOR            512u
#define SETORES_POR_COMANDO 64u                        /* 32 KB por comando */
#define TAM_BUFFER           (SETORES_POR_COMANDO * TAM_SETOR)
#define ESPERA_COMANDO_MS   5000u     /* un comando: 5 s como mucho */
#define ESPERA_ENLACE_MS    1000u     /* el enlace SATA se establece en menos de 1 s */
#define ESPERA_DISCO_MS     10000u    /* un disco de platos tarda unos segundos en girar */
#define MAX_PRDT            16       /* descriptores por comando */

#define AHCI_MAX_CONTROLADORES 2
#define AHCI_MAX_PORTAS        32

typedef struct {
    uint32_t clb, fb;         /* direcciones físicas */
    uint8_t *clb_v, *fb_v;    /* sus punteros (la memoria física es 1:1) */
    uint8_t *ct;              /* tabla de comandos (FIS + PRDT) */
    bool ocupada;
    bool disco;
    uint8_t erro;             /* D207: registro de error del último comando fallido */
} ahci_porta_t;

typedef struct ahci_ctl {
    pci_disp_t pci;
    volatile uint32_t *regs;  /* la barra ABAR mapeada */
    uint32_t abbar;
    int portas;
    ahci_porta_t porta[AHCI_MAX_PORTAS];
    uint8_t *buffer;          /* datos del DMA */
    bool ok;
} ahci_ctl_t;

typedef struct {
    ahci_ctl_t *ctl;
    int         porta;
    uint32_t    setores;
    bool        lba48;
    char        modelo[48];
    bool        trim;          /* IDENTIFY palabra 169, bit 0 (D179) */
} ahci_disco_t;

static ahci_ctl_t ctls[AHCI_MAX_CONTROLADORES];
static int qtd_ctls;
static int qtd_controladores;   /* vistos en el PCI */
static int qtd_sem_suporte;      /* vistos pero no utilizables */
static int qtd_discos;           /* discos SATA registrados */

int ahci_controladores(void) { return qtd_controladores; }
int ahci_sem_suporte(void) { return qtd_sem_suporte; }

/* ------------------------------------------------------------- registros */

static uint32_t reg(ahci_ctl_t *c, int porta, uint32_t desloc)
{
    return c->regs[(PORTA_BASE + (uint32_t)porta * PORTA_PASSO + desloc) / 4];
}

static void reg_escreve(ahci_ctl_t *c, int porta, uint32_t desloc, uint32_t v)
{
    c->regs[(PORTA_BASE + (uint32_t)porta * PORTA_PASSO + desloc) / 4] = v;
}

/* Tiempo real para las esperas (D199). Las interrupciones están apagadas
 * mientras se buscan los discos, así que el reloj no avanza: se usa el TSC,
 * medido antes al arrancar. Sin TSC, el puerto 0x80 (~1 us por escritura en
 * un PC real). Contar vueltas no vale: en el QEMU una vuelta no dura nada y
 * en un PC real la espera se vuelve de decenas de segundos. */
static inline uint64_t ahci_tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

typedef struct { uint64_t fim; uint32_t voltas; } prazo_t;

static prazo_t prazo_ms(uint32_t ms)
{
    prazo_t p = { 0, 0 };
    uint32_t khz = hw_tsc_khz();
    if (khz) p.fim = ahci_tsc() + (uint64_t)khz * ms;
    else p.voltas = ms * 1000u;
    return p;
}

/* true cuando el plazo ha vencido; si no, hace una pausa corta. */
static bool prazo_vencido(prazo_t *p)
{
    porta_pausa();
    if (p->fim) return ahci_tsc() >= p->fim;
    if (!p->voltas) return true;
    p->voltas--;
    return false;
}

/* El FIS de registro del disco al host (tipo 0x34). Se busca en las ocho
 * posiciones del área de recepción: el ICH9 y el QEMU lo ponen en la misma
 * (0x40), pero el bus lo puede dejar en otra. */
static uint8_t *procurar_fis_d2h(ahci_ctl_t *c, int porta)
{
    uint8_t *fb = c->porta[porta].fb_v;
    for (int i = 0; i < 8; i++)
        if (fb[i * 32] == FIS_D2H) return fb + i * 32;
    return NULL;
}

/* ------------------------------------------------------------- comandos */

/* Un comando ATA, empaquetado como lo entiende el FIS de registro H2D. */
typedef struct {
    uint8_t  comando;
    uint8_t  features;
    uint16_t features_alto;
    uint64_t lba;         /* LBA de 48 bits */
    uint16_t contagem;    /* sectores */
    bool     escrever;
    const uint8_t *cdb;   /* D207: paquete ATAPI (12 bytes) o NULL */
} ata_comando_t;

/* Monta el FIS de registro H2D en la tabla de comandos. */
static void montar_fis(uint8_t *ct, const ata_comando_t *cmd)
{
    volatile uint8_t *f = ct;
    for (int i = 0; i < 64; i++) f[i] = 0;
    f[0] = FIS_H2D;              /* tipo de FIS */
    f[1] = 0x80;                 /* bit C: es un comando */
    f[2] = cmd->comando;
    f[3] = cmd->features;
    f[4] = (uint8_t)(cmd->lba);
    f[5] = (uint8_t)(cmd->lba >> 8);
    f[6] = (uint8_t)(cmd->lba >> 16);
    f[7] = cmd->cdb ? 0 : 0x40;  /* registro de dispositivo: LBA de 48 bits (ATAPI: 0) */
    f[8] = (uint8_t)(cmd->lba >> 24);
    f[9] = (uint8_t)(cmd->lba >> 32);
    f[10] = (uint8_t)(cmd->lba >> 40);
    f[11] = (uint8_t)(cmd->features_alto);
    f[12] = (uint8_t)(cmd->contagem);
    f[13] = (uint8_t)(cmd->contagem >> 8);
    f[14] = 0;                   /* ICC */
    f[15] = 0x80;                /* control: bit C = "esto es un comando" */
}

/* Monta la lista de_descriptores de datos (PRDT): una entrada por cada
 * trozo de datos, con su dirección física y su tamaño. */
static uint32_t montar_prdt(const uint8_t *ct, const uint8_t *buffer, uint32_t bytes, bool escrever)
{
    volatile uint32_t *prdt = (volatile uint32_t *)(ct + 0x80);
    uint32_t n = 0;
    uint32_t feito = 0;
    uint32_t base = (uint32_t)buffer;
    while (feito < bytes && n < MAX_PRDT) {
        uint32_t tam = bytes - feito;
        if (tam > 0x400000u) tam = 0x400000u;
        prdt[n * 4 + 0] = base + feito;        /* dirección física */
        prdt[n * 4 + 1] = 0;                   /* (64 bits: parte alta) */
        prdt[n * 4 + 2] = 0;                   /* reservado */
        /* bit 0: interrupción al terminar; bits 2:1: 01 = el disco manda;
         * bit 31: el tamaño vale; bits 22:0: tamaño en bytes (menos uno) */
        prdt[n * 4 + 3] = 1u | (escrever ? 0u : (1u << 1)) | (1u << 31) | (tam - 1);
        feito += tam;
        n++;
    }
    return n;
}

static void recuperar_porta(ahci_ctl_t *c, int p);
static void recuperar_leve(ahci_ctl_t *c, int p);

/* Error de un comando: el de un lector ATAPI (D207) solo para y arranca el motor. */
static void recuperar_erro(ahci_ctl_t *c, int porta, const ata_comando_t *cmd, uint8_t erro)
{
    c->porta[porta].erro = erro;
    if (cmd->cdb) recuperar_leve(c, porta);
    else recuperar_porta(c, porta);
}

/* Bytes que el controlador dice haber transferido (PRDBC de la cabecera); si no lo
 * actualiza (0), los pedidos. Solo importa para el ATAPI: los discos piden lo justo. */
static int transferidos(ahci_ctl_t *c, int porta, const ata_comando_t *cmd, uint32_t bytes)
{
    if (!cmd->cdb) return 0;
    uint32_t n = ((volatile uint32_t *)c->porta[porta].clb_v)[1];
    if (!n || n > bytes) n = bytes;
    return (int)n;
}

/* Manda un comando al disco y espera a que termine. Devuelve los bytes transferidos
 * (>= 0), -1 si falla o -2 si el dispositivo dijo ERR (el error en c->porta[].erro). */
static int executar_espera(ahci_ctl_t *c, int porta, const ata_comando_t *cmd, uint32_t bytes, uint32_t espera)
{
    ahci_porta_t *pt = &c->porta[porta];
    volatile uint32_t *clb = (volatile uint32_t *)pt->clb_v;
    uint32_t prdtl = montar_prdt(pt->ct, c->buffer, bytes, cmd->escrever);

    montar_fis(pt->ct, cmd);

    /* Cabecera del comando (ranura 0 de la lista): longitud del FIS (5
     * palabras = 32 bytes), bit de escritura y número de descriptores; luego
     * el estado y la dirección de la tabla de comandos (64 bits). */
    clb[0] = 5u | (cmd->escrever ? (1u << 6) : 0u) | (cmd->cdb ? (1u << 5) : 0u) | (prdtl << 16);
    if (cmd->cdb) {   /* D207: el paquete ATAPI va en el ACMD de la tabla de comandos */
        volatile uint8_t *ac = pt->ct + 0x40;
        for (int i = 0; i < 16; i++) ac[i] = i < 12 ? cmd->cdb[i] : 0;
    }
    clb[1] = 0;                              /* estado del comando */
    clb[2] = (uint32_t)pt->ct;               /* dirección de la tabla de comandos */
    clb[3] = 0;                              /* (64 bits: parte alta) */

    /* El área de recepción se limpia: el FIS viejo (la firma del disco) no
     * debe confundirse con la respuesta. */
    for (int i = 0; i < 256; i++) pt->fb_v[i] = 0;

    /* La "campainha": el bit 0 de PxCI lanza el comando de la ranura 0. */
    reg_escreve(c, porta, P_CI, 1);

    prazo_t prazo = prazo_ms(espera);
    for (;;) {
        uint32_t ci = reg(c, porta, P_CI);
        uint8_t *f = procurar_fis_d2h(c, porta);
        if (f) {
            if (f[2] & 0x01) {          /* el disco dice ERR */
                if (!cmd->cdb)
                    klog("[AHCI] puerto %d: comando %x falló (estado %x, erro %x)\n",
                         porta, cmd->comando, f[2], f[3]);
                recuperar_erro(c, porta, cmd, f[3]);
                return cmd->cdb ? -2 : -1;
            }
            if (!(ci & 1)) return transferidos(c, porta, cmd, bytes);   /* terminado */
        } else if (!(ci & 1)) {
            /* Terminó pero el FIS aún no está: se espera un poco por él. */
            for (int j = 0; j < 1000; j++) {
                f = procurar_fis_d2h(c, porta);
                if (f) break;
                porta_pausa();
            }
            if (f && (f[2] & 0x01)) {
                if (!cmd->cdb)
                    klog("[AHCI] puerto %d: comando %x falló (estado %x, erro %x)\n",
                         porta, cmd->comando, f[2], f[3]);
                recuperar_erro(c, porta, cmd, f[3]);
                return cmd->cdb ? -2 : -1;
            }
            /* D207: un PACKET por PIO puede acabar sin FIS D2H: el estado está en PxTFD. */
            uint32_t tfd = reg(c, porta, P_TFD);
            if (cmd->cdb && (tfd & 0x01)) {
                recuperar_erro(c, porta, cmd, (uint8_t)(tfd >> 8));
                return -2;
            }
            return transferidos(c, porta, cmd, bytes);
        }
        /* La pausa (puerto 0x80) va dentro: el QEMU procesa el DMA. */
        if (prazo_vencido(&prazo)) break;
    }
    klog("[AHCI] puerto %d: comando %x sin respuesta (tiempo agotado; TFD %x, SERR %x, IS %x, SSTS %x)\n",
         porta, cmd->comando, reg(c, porta, P_TFD), reg(c, porta, P_SERR), reg(c, porta, P_IS), reg(c, porta, P_SSTS));
    recuperar_porta(c, porta);
    return -1;
}

static int executar(ahci_ctl_t *c, int porta, const ata_comando_t *cmd, uint32_t bytes)
{
    return executar_espera(c, porta, cmd, bytes, ESPERA_COMANDO_MS);
}

/* El FLUSH y el TRIM pueden tardar mucho más que una lectura (el anfitrión del QEMU hace
 * un fsync; un SSD de verdad borra bloques): veinte veces el plazo normal (D179). */
#define ESPERA_LARGA 60000u   /* vaciar la caché o TRIM: hasta 60 s */

/* ------------------------------------------------------------- discos */

/* Los textos del IDENTIFY vienen con los dos bytes cambiados. */
static void copiar_texto_ata(char *dest, const uint16_t *id, int ini, int palavras, int max)
{
    int k = 0;
    for (int i = 0; i < palavras && k + 2 < max; i++) {
        dest[k++] = (char)(id[ini + i] >> 8);
        dest[k++] = (char)(id[ini + i] & 0xFF);
    }
    while (k > 0 && (dest[k - 1] == ' ' || dest[k - 1] == 0)) k--;
    dest[k] = 0;
}

static int ler_setores(disco_t *k, uint32_t lba, uint32_t n, void *buf)
{
    ahci_disco_t *a = k->dono;
    ahci_ctl_t *c = a->ctl;
    uint32_t feitos = 0;
    while (feitos < n) {
        uint32_t ped = n - feitos;
        if (ped > SETORES_POR_COMANDO) ped = SETORES_POR_COMANDO;
        uint32_t bytes = ped * TAM_SETOR;
        ata_comando_t cmd = { .comando = CMD_READ_EXT, .lba = lba + feitos,
                              .contagem = (uint16_t)ped };
        if (executar(c, a->porta, &cmd, bytes) < 0) return -1;
        memcpy((uint8_t *)buf + feitos * TAM_SETOR, c->buffer, bytes);
        feitos += ped;
    }
    return 0;
}

static int escrever_setores(disco_t *k, uint32_t lba, uint32_t n, const void *buf)
{
    ahci_disco_t *a = k->dono;
    ahci_ctl_t *c = a->ctl;
    uint32_t feitos = 0;
    while (feitos < n) {
        uint32_t ped = n - feitos;
        if (ped > SETORES_POR_COMANDO) ped = SETORES_POR_COMANDO;
        uint32_t bytes = ped * TAM_SETOR;
        ata_comando_t cmd = { .comando = CMD_WRITE_EXT, .lba = lba + feitos,
                              .contagem = (uint16_t)ped, .escrever = true };
        memcpy(c->buffer, (const uint8_t *)buf + feitos * TAM_SETOR, bytes);
        if (executar(c, a->porta, &cmd, bytes) < 0) return -1;
        feitos += ped;
    }
    /* D179: como en el IDE, lo escrito sale de la caché del disco antes de volver
     * (apagar o quitar la corriente justo después no pierde nada). */
    ata_comando_t f = { .comando = CMD_FLUSH_EXT };
    return executar_espera(c, a->porta, &f, 0, ESPERA_LARGA) < 0 ? -1 : 0;
}

/* TRIM (D179): DATA SET MANAGEMENT con la lista de rangos por DMA. Cada entrada
 * son 8 bytes: LBA de 48 bits y cuántos sectores (hasta 65535). Un bloque de 512
 * bytes lleva 64 entradas; se manda un bloque cada vez. */
static int descartar_setores(disco_t *k, uint32_t lba, uint32_t n)
{
    ahci_disco_t *a = k->dono;
    ahci_ctl_t *c = a->ctl;
    if (!a->trim) return -1;
    while (n) {
        memset(c->buffer, 0, TAM_SETOR);
        uint8_t *e = c->buffer;
        for (int i = 0; i < 64 && n; i++, e += 8) {
            uint32_t q = n > 0xFFFFu ? 0xFFFFu : n;
            e[0] = (uint8_t)lba; e[1] = (uint8_t)(lba >> 8); e[2] = (uint8_t)(lba >> 16); e[3] = (uint8_t)(lba >> 24);
            e[4] = e[5] = 0;
            e[6] = (uint8_t)q; e[7] = (uint8_t)(q >> 8);
            lba += q; n -= q;
        }
        ata_comando_t cmd = { .comando = CMD_DSM, .features = 1, .contagem = 1, .escrever = true };
        if (executar_espera(c, a->porta, &cmd, TAM_SETOR, ESPERA_LARGA) < 0) return -1;
    }
    return 0;
}

/* ------------------------------------------------------------- arranque */

/* Memoria de un puerto: lista de comandos, tabla de comandos (con su lista
 * de descriptores) y área de recepción de FIS. */
static bool alocar_porta(ahci_ctl_t *c, int p)
{
    ahci_porta_t *pt = &c->porta[p];
    if (pt->ocupada) return true;
    uint32_t clb = memfisica_alocar();          /* lista de comandos (1 KB) */
    uint32_t ct = memfisica_alocar();           /* tabla de comandos + PRDT */
    uint32_t fb = memfisica_alocar();           /* área de recepción (256 B) */
    if (!clb || !ct || !fb) {
        klog("[AHCI] puerto %d: falta memoria para las estructuras\n", p);
        return false;
    }
    memset((void *)clb, 0, PAGINA);
    memset((void *)ct, 0, PAGINA);
    memset((void *)fb, 0, PAGINA);
    pt->clb = clb;  pt->clb_v = (uint8_t *)clb;
    pt->ct = (uint8_t *)ct;
    pt->fb = fb;   pt->fb_v = (uint8_t *)fb;
    pt->ocupada = true;
    return true;
}

/* Prepara un puerto: apunta las estructuras y enciende el motor.
 * Devuelve 0 si hay un disco, 1 si el puerto está vacío y -1 si falla. */
static int preparar_porta(ahci_ctl_t *c, int p)
{
    ahci_porta_t *pt = &c->porta[p];
    if (!alocar_porta(c, p)) return -1;

    /* Primero se para el puerto. Si la BIOS lo dejó en marcha (típico), sus
     * estructuras están en memoria de la BIOS, que ya no vale: se apagan los
     * dos motores (ST y luego FRE) y se espera a que el controlador lo
     * confirme (CR y FR a 0), como pide la especificación (AHCI 1.3, 10.3.2).
     * Se conservan los bits de giro y corriente. */
    uint32_t cmd = reg(c, p, P_CMD);
    reg_escreve(c, p, P_CMD, cmd & ~CMD_ST);
    prazo_t pz = prazo_ms(500);
    while ((reg(c, p, P_CMD) & CMD_LIST_ON) && !prazo_vencido(&pz)) ;
    cmd = reg(c, p, P_CMD);
    reg_escreve(c, p, P_CMD, cmd & ~CMD_FRE);
    pz = prazo_ms(500);
    while ((reg(c, p, P_CMD) & CMD_FIS_ON) && !prazo_vencido(&pz)) ;
    if (reg(c, p, P_CMD) & (CMD_LIST_ON | CMD_FIS_ON))
        klog("[AHCI] puerto %d: la BIOS lo dejó en marcha y no se para (CMD %x)\n", p, reg(c, p, P_CMD));

    reg_escreve(c, p, P_CLB, pt->clb);
    reg_escreve(c, p, P_CLBU, 0);
    reg_escreve(c, p, P_FB, pt->fb);
    reg_escreve(c, p, P_FBU, 0);
    for (int i = 0; i < 256; i++) pt->fb_v[i] = 0;

    /* Recepción de FIS: hasta que el bit "FIS on" no esté, el controlador no
     * escribe las respuestas del disco. */
    reg_escreve(c, p, P_CMD, CMD_FRE | CMD_PON | CMD_SPIN);
    pz = prazo_ms(500);
    while (!(reg(c, p, P_CMD) & CMD_FIS_ON) && !prazo_vencido(&pz)) ;
    if (!(reg(c, p, P_CMD) & CMD_FIS_ON)) {
        klog("[AHCI] puerto %d: el motor de FIS no arrancó\n", p);
        return -1;
    }

    /* Enlace: DET = 3 (dispositivo y comunicación). DET = 1 solo dice que
     * hay algo enchufado; con eso el disco aún no ha mandado su firma. Tras
     * el reinicio del controlador (y con giro escalonado, CAP.SSS, que el
     * ICH7 tiene) el enlace tarda unos milisegundos en subir. */
    pz = prazo_ms(ESPERA_ENLACE_MS);
    while ((reg(c, p, P_SSTS) & SSTS_DET) != 3 && !prazo_vencido(&pz)) ;
    uint32_t det = reg(c, p, P_SSTS) & SSTS_DET;
    if (det == 0) return 1;   /* puerto vacío: no es un error */
    if (det != 3) {
        /* Algo hay, pero el enlace no sube: un COMRESET suele bastar. */
        klog("[AHCI] puerto %d: enlace sin establecer (SSTS %x), COMRESET\n", p, reg(c, p, P_SSTS));
        reg_escreve(c, p, P_SCTL, (reg(c, p, P_SCTL) & ~0xFu) | 1u);
        pz = prazo_ms(2); while (!prazo_vencido(&pz)) ;
        reg_escreve(c, p, P_SCTL, reg(c, p, P_SCTL) & ~0xFu);
        pz = prazo_ms(ESPERA_ENLACE_MS);
        while ((reg(c, p, P_SSTS) & SSTS_DET) != 3 && !prazo_vencido(&pz)) ;
        if ((reg(c, p, P_SSTS) & SSTS_DET) != 3) {
            klog("[AHCI] puerto %d: el enlace no sube (SSTS %x)\n", p, reg(c, p, P_SSTS));
            return 1;
        }
    }

    /* Errores del enlace: al subir queda PxSERR.DIAG.X (y otros) a 1, y
     * mientras no se borren el controlador no actualiza PxTFD con el FIS
     * del disco (AHCI 1.3, 10.1.2). El QEMU no lo exige; el ICH7 sí. */
    reg_escreve(c, p, P_SERR, 0xFFFFFFFFu);
    reg_escreve(c, p, P_IS, 0xFFFFFFFFu);

    /* El disco manda su firma con BSY a 0 cuando ha terminado de girar: hasta
     * entonces el controlador no emite ningún comando (se quedaría en PxCI
     * para siempre). Un disco de platos tarda unos segundos. */
    pz = prazo_ms(ESPERA_DISCO_MS);
    while ((reg(c, p, P_TFD) & (TFD_BSY | TFD_DRQ)) && !prazo_vencido(&pz)) ;
    if (reg(c, p, P_TFD) & (TFD_BSY | TFD_DRQ)) {
        klog("[AHCI] puerto %d: el disco sigue ocupado (TFD %x, SERR %x)\n", p,
             reg(c, p, P_TFD), reg(c, p, P_SERR));
        return -1;
    }
    reg_escreve(c, p, P_SERR, 0xFFFFFFFFu);

    /* Motor de DMA: la lista de comandos ya está en su sitio. */
    reg_escreve(c, p, P_CMD, CMD_FRE | CMD_PON | CMD_SPIN | CMD_ST);
    pz = prazo_ms(500);
    while (!(reg(c, p, P_CMD) & CMD_LIST_ON) && !prazo_vencido(&pz)) ;
    if (!(reg(c, p, P_CMD) & CMD_LIST_ON)) {
        klog("[AHCI] puerto %d: el motor de DMA no arrancó\n", p);
        return -1;
    }
    return 0;
}

/* Reinicia el puerto (COMRESET) y lo vuelve a preparar: se usa cuando un
 * comando falla, porque el bit de "comando en curso" se queda levantado. */
static void recuperar_porta(ahci_ctl_t *c, int p)
{
    /* Se para el motor de DMA (si no, el COMRESET no se puede hacer). */
    reg_escreve(c, p, P_CMD, reg(c, p, P_CMD) & ~CMD_ST);
    prazo_t pz = prazo_ms(500);
    while ((reg(c, p, P_CMD) & CMD_LIST_ON) && !prazo_vencido(&pz)) ;
    reg_escreve(c, p, P_SCTL, (reg(c, p, P_SCTL) & ~0xFu) | 1u);
    pz = prazo_ms(2); while (!prazo_vencido(&pz)) ;
    reg_escreve(c, p, P_SCTL, reg(c, p, P_SCTL) & ~0xFu);
    preparar_porta(c, p);
}

/* D207: tras un error (PxIS.TFES) el controlador deja de procesar la lista: se para el
 * motor (ST), se borran los errores y se vuelve a arrancar, sin COMRESET (AHCI 1.3,
 * 6.2.2). Si el dispositivo sigue ocupado, el reinicio completo. */
static void recuperar_leve(ahci_ctl_t *c, int p)
{
    reg_escreve(c, p, P_CMD, reg(c, p, P_CMD) & ~CMD_ST);
    prazo_t pz = prazo_ms(500);
    while ((reg(c, p, P_CMD) & CMD_LIST_ON) && !prazo_vencido(&pz)) ;
    reg_escreve(c, p, P_SERR, 0xFFFFFFFFu);
    reg_escreve(c, p, P_IS, 0xFFFFFFFFu);
    if (reg(c, p, P_TFD) & (TFD_BSY | TFD_DRQ)) { recuperar_porta(c, p); return; }
    reg_escreve(c, p, P_CMD, reg(c, p, P_CMD) | CMD_ST);
    pz = prazo_ms(500);
    while (!(reg(c, p, P_CMD) & CMD_LIST_ON) && !prazo_vencido(&pz)) ;
}

/* D207: un paquete ATAPI. Con DMA si el lector lo admite (features bit 0); si no, por
 * PIO con el límite de bytes por bloque en LBA mid/high. Los datos pasan por el búfer
 * de DMA del controlador (32 KB). */
static int sata_pacote(atapi_t *a, const uint8_t *cdb, void *buf, uint32_t bytes, uint32_t espera_ms)
{
    ahci_ctl_t *c = a->transporte;
    int p = a->unidade;
    if (bytes > TAM_BUFFER) return ATAPI_FALHA;
    ata_comando_t cmd = { .comando = CMD_PACKET, .features = a->dma ? 1 : 0, .lba = (uint64_t)0xF800u << 8, .cdb = cdb };
    int r = executar_espera(c, p, &cmd, bytes, espera_ms);
    if (r == -2) { a->erro_reg = c->porta[p].erro; return ATAPI_CHECK; }
    if (r < 0) {
        klog("[ATAPI] %s: orden %x sin respuesta (TFD %x)\n", a->onde, cdb[0], reg(c, p, P_TFD));
        return ATAPI_FALHA;
    }
    if (buf && r) memcpy(buf, c->buffer, (uint32_t)r);
    return r;
}

/* D207: un lector SATAPI: IDENTIFY PACKET DEVICE y a la capa común. */
static bool registrar_cd(ahci_ctl_t *c, int p)
{
    uint16_t *id = kmalloc(512);
    if (!id) return false;
    ata_comando_t cmd = { .comando = CMD_ID_PACKET, .lba = 0, .contagem = 1 };
    if (executar(c, p, &cmd, 512) < 0) {
        klog("[ATAPI] AHCI puerto %d: IDENTIFY PACKET falló\n", p);
        kfree(id);
        return false;
    }
    memcpy(id, c->buffer, 512);
    atapi_t m;
    memset(&m, 0, sizeof m);
    m.transporte = c;
    m.unidade = p;
    m.pacote = sata_pacote;
    m.pacote16 = (id[0] & 3) == 1;
    m.dma = (id[49] & (1 << 8)) != 0;
    snformatar(m.onde, sizeof m.onde, "AHCI %x puerto %d", c->pci.modelo, p);
    atapi_t *a = atapi_registrar(&m, id);
    kfree(id);
    return a != NULL;
}

/* Lee el IDENTIFY del disco y lo registra en la capa de discos. */
static bool registrar_disco(ahci_ctl_t *c, int p)
{
    uint16_t *id = kmalloc(512);
    if (!id) return false;
    ata_comando_t cmd = { .comando = CMD_IDENTIFY, .lba = 0, .contagem = 1 };
    if (executar(c, p, &cmd, 512) < 0) {
        klog("[AHCI] puerto %d: el disco no respondió al IDENTIFY\n", p);
        kfree(id);
        return false;
    }
    /* Los datos vienen por DMA a nuestro buffer: se copian al IDENTIFY. */
    memcpy(id, c->buffer, 512);

    uint32_t s28 = (uint32_t)id[60] | (uint32_t)id[61] << 16;
    uint64_t s48 = (uint64_t)id[100] | ((uint64_t)id[101] << 16) |
                   ((uint64_t)id[102] << 32) | ((uint64_t)id[103] << 48);
    bool lba48 = (id[83] & (1 << 10)) != 0;
    uint32_t setores = lba48 ? (uint32_t)s48 : s28;
    if (!setores) setores = s28;
    if (!setores || !(id[49] & (1 << 9))) {
        klog("[AHCI] puerto %d: disco sin LBA, ignorado\n", p);
        kfree(id);
        return false;
    }
    char modelo[48];
    copiar_texto_ata(modelo, id, 27, 20, sizeof modelo);
    bool trim = (id[169] & 1) != 0;
    kfree(id);

    ahci_disco_t *a = kmalloc(sizeof *a);
    if (!a) return false;
    memset(a, 0, sizeof *a);
    a->ctl = c;
    a->porta = p;
    a->setores = setores;
    a->lba48 = lba48;
    a->trim = trim;
    klog("[AHCI] puerto %d: \"%s\", %u setores, LBA%s%s\n", p, modelo, setores, lba48 ? "48" : "28",
         trim ? ", TRIM" : "");
    disco_t *d = disco_registrar("hd", "SATA", modelo, setores, TAM_SETOR, a, false, ler_setores, escrever_setores);
    if (!d) {
        klog("[AHCI] puerto %d: no se ha podido registrar el disco\n", p);
        kfree(a);
        return false;
    }
    if (trim) { d->descartar = descartar_setores; d->trim = TRIM_SIM; }
    qtd_discos++;
    return true;
}

/* Enciende un controlador: capacidades, reinicio y puertos. */
static bool iniciar_controlador(ahci_ctl_t *c)
{
    /* La barra del ABAR es la quinta, de 64 bits (registros 0x24 y 0x28). */
    uint32_t b5 = pci_le32(&c->pci, 0x24);
    if (b5 & 1) {
        klog("[AHCI] %x:%x: la barra ABAR no es de memoria\n", c->pci.fabricante, c->pci.modelo);
        return false;
    }
    uint64_t abbar64 = b5 & ~0xFu;
    if ((b5 & 3) == 2) abbar64 |= (uint64_t)pci_le32(&c->pci, 0x28) << 32;
    if (!abbar64) {
        klog("[AHCI] %x:%x: sin barra ABAR\n", c->pci.fabricante, c->pci.modelo);
        return false;
    }
    if (abbar64 > 0xFFFFFFFFull) {
        klog("[AHCI] %x:%x: la barra ABAR está por encima de 4 GB\n", c->pci.fabricante, c->pci.modelo);
        return false;
    }
    uint32_t abbar = (uint32_t)abbar64;
    /* Comando PCI: espacio de memoria y maestro del bus (para el DMA). */
    pci_escreve32(&c->pci, 0x04, pci_le32(&c->pci, 0x04) | 0x6);
    uint32_t v = janela_mapear(abbar & ~0xFFFu, 0x1000);
    if (!v) {
        klog("[AHCI] %x:%x: no se ha podido mapear la barra ABAR\n", c->pci.fabricante, c->pci.modelo);
        return false;
    }
    c->regs = (volatile uint32_t *)(v + (abbar & 0xFFF));
    c->abbar = abbar;

    uint32_t cap = c->regs[HBA_CAP / 4];
    uint32_t pi = c->regs[HBA_PI / 4];
    c->portas = (int)(cap & 0xF) + 1;
    if (c->portas > AHCI_MAX_PORTAS) c->portas = AHCI_MAX_PORTAS;

    /* Reinicio del controlador y habilitación (GHC.AE). Las interrupciones
     * globales se dejan apagadas: el driver va por polling. */
    c->regs[HBA_GHC / 4] = GHC_AE;
    c->regs[HBA_GHC / 4] = GHC_HRST | GHC_AE;
    prazo_t pz = prazo_ms(1000);
    while ((c->regs[HBA_GHC / 4] & GHC_HRST) && !prazo_vencido(&pz)) ;
    c->regs[HBA_GHC / 4] = GHC_AE;
    /* En los Intel (ICH) la BIOS escribe CAP y PI una sola vez y el reinicio
     * los pierde: se devuelven, como hace Linux (D199). */
    c->regs[HBA_CAP / 4] = cap;
    c->regs[HBA_PI / 4] = pi;

    /* Buffer de datos: 32 KB de memoria física contigua para el DMA. */
    c->buffer = (uint8_t *)memfisica_alocar_seguidas(TAM_BUFFER / PAGINA);
    if (!c->buffer) {
        klog("[AHCI] %x:%x: sin memoria para el buffer de datos\n", c->pci.fabricante, c->pci.modelo);
        return false;
    }
    memset(c->buffer, 0, TAM_BUFFER);

    klog("[AHCI] controlador %x:%x no PCI %u:%u.%u, %d puerto(s), %u ranuras de comando, ABAR %x\n",
         c->pci.fabricante, c->pci.modelo, c->pci.barramento, c->pci.dispositivo, c->pci.funcao,
         c->portas, (cap >> 8) & 0x1F, abbar);

    c->ok = true;
    for (int p = 0; p < c->portas; p++) {
        if (!(pi & (1u << p))) continue;      /* puerto no implementado */
        int r = preparar_porta(c, p);
        if (r < 0) {
            klog("[AHCI] puerto %d: no se ha podido preparar\n", p);
            c->ok = false;
            continue;
        }
        if (r > 0) {
            klog("[AHCI] puerto %d: vacío\n", p);
            continue;
        }
        uint32_t sig = reg(c, p, P_SIG);
        if (sig == 0xEB140101u) {   /* D207: lector de CD/DVD (SATAPI) */
            klog("[AHCI] puerto %d: lector de CD/DVD (SATAPI)\n", p);
            if (registrar_cd(c, p)) c->porta[p].disco = true;
            continue;
        }
        if (registrar_disco(c, p)) c->porta[p].disco = true;
        else c->ok = false;
    }
    return c->ok;
}

int ahci_iniciar(void)
{
    pci_disp_t achados[AHCI_MAX_CONTROLADORES];
    int n = pci_procurar_classe(0x01, 0x06, achados, AHCI_MAX_CONTROLADORES);  /* almacenamiento, SATA */
    qtd_controladores = n;
    if (!n) return 0;
    bool i0 = int_salva_desliga();
    for (int i = 0; i < n; i++) {
        ahci_ctl_t *c = &ctls[qtd_ctls];
        memset(c, 0, sizeof *c);
        c->pci = achados[i];
        if (iniciar_controlador(c)) qtd_ctls++;
        else qtd_sem_suporte++;
    }
    int_restaura(i0);
    return qtd_discos;
}
