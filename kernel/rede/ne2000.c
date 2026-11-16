// SPDX-License-Identifier: GPL-2.0-or-later
/* ne2000.c - driver de las tarjetas NE2000 (D329): el DP8390 con 16 KB de RAM propia, en PCI
 * (Realtek RTL8029 y clones, 10ec:8029) y en ISA (puertos 0x300/0x280/0x320/0x340). Código propio,
 * escrito a partir de la hoja de datos del DP8390 (National Semiconductor), sin copiar a Linux.
 *
 * El chip no hace DMA hacia la memoria del PC. Todo pasa por su RAM:
 *   - registros en cuatro páginas (bits PS0/PS1 del CR en el offset 0); la página 0 es la de
 *     trabajo, la 1 tiene la MAC y el puntero de escritura de la recepción (CURR);
 *   - la RAM va de la página 0x40 a la 0x7F (páginas de 256 bytes): una caja de envío al
 *     principio (6 páginas, un quadro entero) y el resto es el anillo de recepción
 *     (PSTART..PSTOP). La tarjeta escribe cada quadro con una cabecera de 4 bytes (estado,
 *     página del siguiente, tamaño con CRC); BNRY es hasta dónde ha leído el driver;
 *   - el DMA remoto (RSAR/RBCR + el puerto de datos, offset 0x10) copia entre esa RAM y el PC,
 *     por palabras de 16 bits. No da la vuelta al llegar al final del anillo: lo hace el driver;
 *   - la MAC está en un PROM de 32 bytes al principio de la RAM, con cada byte duplicado
 *     (y 0x57 0x57 al final en las de 16 bits).
 * ISA: el sondeo es prudente (solo lee hasta que algo responde como un 8390 tras el reinicio),
 * no toca rangos de E/S de tarjetas PCI y está APAGADO por defecto (en un PC real esos puertos pueden ser de otras tarjetas: MPU-401 en 0x330, SCSI...); se activa con mkimagem.py --ne2k-isa. La IRQ
 * viene de la configuración (--ne2k-irq) o, si no, se averigua pidiendo una interrupción de DMA
 * remoto y mirando qué línea se enciende en el IRR del 8259 (como la Sound Blaster 16). */
#include "rede.h"
#include "bootinfo.h"
#include "console.h"
#include "cpu.h"
#include "isa.h"
#include "memoria.h"
#include "pci.h"
#include "portas.h"
#include "texto.h"

/* CR (offset 0, todas las páginas) */
#define CR       0x00
#define CR_STP   0x01
#define CR_STA   0x02
#define CR_TXP   0x04
#define CR_RD_LER   0x08
#define CR_RD_ESC   0x10
#define CR_RD_ABORTAR 0x20
#define CR_PAG1  0x40
/* página 0, escritura */
#define PSTART   0x01
#define PSTOP    0x02
#define BNRY     0x03
#define TPSR     0x04
#define TBCR0    0x05
#define TBCR1    0x06
#define ISR      0x07
#define RSAR0    0x08
#define RSAR1    0x09
#define RBCR0    0x0A
#define RBCR1    0x0B
#define RCR      0x0C
#define TCR      0x0D
#define DCR      0x0E
#define IMR      0x0F
/* página 1 */
#define PAR0     0x01
#define CURR     0x07
#define MAR0     0x08
/* puertos fuera de los registros */
#define PORTA_DADOS 0x10
#define PORTA_RESET 0x1F
#define FAIXA       0x20   /* tamaño del rango de E/S */

#define ISR_PRX 0x01
#define ISR_PTX 0x02
#define ISR_RXE 0x04
#define ISR_TXE 0x08
#define ISR_OVW 0x10
#define ISR_RDC 0x40
#define ISR_RST 0x80

#define PAG_TX     0x40
#define PAG_RX0    0x46
#define PAG_FIM    0x80

static uint16_t io;
static bool via_pci;
static bool rdc_pedido;     /* provocar_irq() dejó un RDC pendiente a propósito */

static uint8_t le8(uint16_t r)  { return porta_le8((uint16_t)(io + r)); }
static void escreve8(uint16_t r, uint8_t v) { porta_escreve8((uint16_t)(io + r), v); }

/* Pausa de unos microsegundos por cada vuelta (una escritura al puerto 0x80). */
static void espera(uint32_t voltas) { for (uint32_t i = 0; i < voltas; i++) porta_pausa(); }

/* Espera a que el DMA remoto termine (RDC) y lo limpia. */
static void rdc_esperar(void)
{
    for (uint32_t i = 0; i < 20000 && !(le8(ISR) & ISR_RDC); i++) porta_pausa();
    escreve8(ISR, ISR_RDC);
}

/* DMA remoto: de la RAM de la tarjeta (dirección "ender") al PC. n bytes. */
static void dma_ler(uint16_t ender, void *destino, uint32_t n)
{
    uint8_t *d = (uint8_t *)destino;
    uint32_t pal = (n + 1) / 2;
    escreve8(ISR, ISR_RDC);
    escreve8(RBCR0, (uint8_t)(pal * 2));
    escreve8(RBCR1, (uint8_t)((pal * 2) >> 8));
    escreve8(RSAR0, (uint8_t)ender);
    escreve8(RSAR1, (uint8_t)(ender >> 8));
    escreve8(CR, CR_STA | CR_RD_LER);
    for (uint32_t i = 0; i < pal; i++) {
        uint16_t w = porta_le16((uint16_t)(io + PORTA_DADOS));
        d[2 * i] = (uint8_t)w;
        if (2 * i + 1 < n) d[2 * i + 1] = (uint8_t)(w >> 8);
    }
    rdc_esperar();
}

/* DMA remoto: del PC a la RAM de la tarjeta. */
static void dma_escrever(uint16_t ender, const void *origem, uint32_t n)
{
    const uint8_t *s = (const uint8_t *)origem;
    uint32_t pal = (n + 1) / 2;
    escreve8(ISR, ISR_RDC);
    escreve8(RBCR0, (uint8_t)(pal * 2));
    escreve8(RBCR1, (uint8_t)((pal * 2) >> 8));
    escreve8(RSAR0, (uint8_t)ender);
    escreve8(RSAR1, (uint8_t)(ender >> 8));
    escreve8(CR, CR_STA | CR_RD_ESC);
    for (uint32_t i = 0; i < pal; i++) {
        uint16_t w = s[2 * i];
        if (2 * i + 1 < n) w |= (uint16_t)(s[2 * i + 1] << 8);
        porta_escreve16((uint16_t)(io + PORTA_DADOS), w);
    }
    rdc_esperar();
}

/* Reinicio por el puerto 0x1F y espera a que el ISR marque RST. false = no responde como un 8390. */
static bool reiniciar_chip(void)
{
    uint8_t v = porta_le8((uint16_t)(io + PORTA_RESET));
    porta_escreve8((uint16_t)(io + PORTA_RESET), v);
    for (uint32_t i = 0; i < 20000; i++) {
        if (le8(ISR) & ISR_RST) { escreve8(ISR, 0xFF); return true; }
        porta_pausa();
    }
    return false;
}

/* Configura el chip (modo palabra, anillo, MAC, recepción de unicast propio y difusión). */
static void configurar(const uint8_t *mac, uint8_t imr)
{
    escreve8(CR, CR_STP | CR_RD_ABORTAR);
    escreve8(DCR, 0x49);               /* palabras de 16 bits, FIFO de 8 bytes, sin loopback */
    escreve8(RBCR0, 0); escreve8(RBCR1, 0);
    escreve8(RCR, 0x20);               /* monitor: no guarda nada todavía */
    escreve8(TCR, 0x02);               /* loopback interno mientras se configura */
    escreve8(TPSR, PAG_TX);
    escreve8(PSTART, PAG_RX0);
    escreve8(PSTOP, PAG_FIM);
    escreve8(BNRY, PAG_RX0);
    escreve8(ISR, 0xFF);
    escreve8(IMR, 0);
    escreve8(CR, CR_STP | CR_RD_ABORTAR | CR_PAG1);
    for (int i = 0; i < 6; i++) escreve8((uint16_t)(PAR0 + i), mac[i]);
    for (int i = 0; i < 8; i++) escreve8((uint16_t)(MAR0 + i), 0xFF);   /* multidifusión: todo (como el RTL8139) */
    escreve8(CURR, PAG_RX0 + 1);
    escreve8(CR, CR_STA | CR_RD_ABORTAR);   /* página 0, en marcha */
    escreve8(TCR, 0x00);
    escreve8(RCR, 0x0C);               /* AB (difusión) + AM (multidifusión) */
    escreve8(ISR, 0xFF);
    escreve8(IMR, imr);
}

/* El anillo se desbordó: parar, vaciarlo y reanudar (receta de la hoja de datos). */
static void recuperar_desborde(void)
{
    escreve8(CR, CR_STP | CR_RD_ABORTAR);
    espera(2000);                      /* >1,6 ms */
    escreve8(RBCR0, 0); escreve8(RBCR1, 0);
    escreve8(TCR, 0x02);
    escreve8(CR, CR_STA | CR_RD_ABORTAR);
    escreve8(BNRY, PAG_RX0);
    escreve8(CR, CR_STA | CR_RD_ABORTAR | CR_PAG1);
    escreve8(CURR, PAG_RX0 + 1);
    escreve8(CR, CR_STA | CR_RD_ABORTAR);
    escreve8(ISR, ISR_OVW);
    escreve8(TCR, 0x00);
}

static uint8_t imr_ativa;

static bool enviar(const void *quadro, uint32_t n)
{
    if (n > ETH_MAX) return false;
    bool i0 = int_salva_desliga();
    for (uint32_t i = 0; i < 20000 && (le8(CR) & CR_TXP); i++) porta_pausa();   /* envío anterior en curso */
    if (le8(CR) & CR_TXP) { int_restaura(i0); return false; }
    uint8_t quadro_min[ETH_MIN];
    if (n < ETH_MIN) {
        memset(quadro_min, 0, sizeof quadro_min);
        memcpy(quadro_min, quadro, n);
        dma_escrever((uint16_t)(PAG_TX << 8), quadro_min, ETH_MIN);
        n = ETH_MIN;
    } else {
        dma_escrever((uint16_t)(PAG_TX << 8), quadro, n);
    }
    escreve8(TPSR, PAG_TX);
    escreve8(TBCR0, (uint8_t)n);
    escreve8(TBCR1, (uint8_t)(n >> 8));
    escreve8(CR, CR_STA | CR_RD_ABORTAR | CR_TXP);
    int_restaura(i0);
    return true;
}

static int receber(void *destino)
{
    bool i0 = int_salva_desliga();
    int r = 0;
    if (le8(ISR) & ISR_OVW) recuperar_desborde();
    uint8_t bnry = le8(BNRY);
    escreve8(CR, CR_STA | CR_RD_ABORTAR | CR_PAG1);
    uint8_t curr = le8(CURR);
    escreve8(CR, CR_STA | CR_RD_ABORTAR);
    uint8_t prox = (uint8_t)(bnry + 1);
    if (prox >= PAG_FIM) prox = PAG_RX0;
    if (prox != curr) {
        uint8_t cab[4];
        dma_ler((uint16_t)(prox << 8), cab, 4);
        uint8_t seg = cab[1];
        uint32_t tam = (uint32_t)(cab[2] | cab[3] << 8);   /* con el CRC */
        if (!(cab[0] & 1) || tam < 18 || tam > ETH_MAX + 4 || seg < PAG_RX0 || seg >= PAG_FIM) {
            /* Cabecera incoherente: se vacía el anillo hasta lo que la tarjeta ha escrito. */
            escreve8(BNRY, (uint8_t)(curr == PAG_RX0 ? PAG_FIM - 1 : curr - 1));
            r = -1;
        } else {
            uint32_t n = tam - 4;
            uint32_t ini = ((uint32_t)prox << 8) + 4;
            uint32_t ate_fim = ((uint32_t)PAG_FIM << 8) - ini;
            if (n <= ate_fim) {
                dma_ler((uint16_t)ini, destino, n);
            } else {   /* el quadro da la vuelta al final del anillo */
                dma_ler((uint16_t)ini, destino, ate_fim);
                dma_ler((uint16_t)(PAG_RX0 << 8), (uint8_t *)destino + ate_fim, n - ate_fim);
            }
            escreve8(BNRY, (uint8_t)(seg == PAG_RX0 ? PAG_FIM - 1 : seg - 1));
            r = (int)n;
        }
    }
    int_restaura(i0);
    return r;
}

static bool cabo(void) { return true; }   /* el DP8390 no informa del enlace */

static bool atender(void)
{
    uint8_t s = le8(ISR) & (uint8_t)~ISR_RST;
    if (!rdc_pedido) s &= (uint8_t)~ISR_RDC;   /* el RDC normal se espera por sondeo, no cuenta como IRQ */
    if (!s) return false;
    if (s & ISR_RDC) { rdc_pedido = false; escreve8(IMR, imr_ativa); }
    escreve8(ISR, s);                  /* escribir 1 limpia */
    if (s & ISR_OVW) {
        klog("[NE2000] anillo de recepcion desbordado\n");
        recuperar_desborde();
    }
    return true;
}

/* D211: una interrupción de DMA remoto terminado a pedido (autoprueba de la IRQ). */
static bool provocar_irq(void)
{
    bool i0 = int_salva_desliga();
    rdc_pedido = true;
    escreve8(ISR, ISR_RDC);
    escreve8(IMR, (uint8_t)(imr_ativa | ISR_RDC));
    escreve8(RBCR0, 2); escreve8(RBCR1, 0);
    escreve8(RSAR0, 0); escreve8(RSAR1, 0);
    escreve8(CR, CR_STA | CR_RD_LER);
    porta_le16((uint16_t)(io + PORTA_DADOS));   /* al acabar el DMA remoto el chip sube la línea; el RDC se queda puesto */
    int_restaura(i0);
    return true;
}

/* Quita los 8 bytes del PROM duplicados: palabra i = MAC[i] (en las 16 bits). true si parece una NE2000. */
static bool ler_prom(uint8_t *mac, bool exigir_ww)
{
    uint8_t prom[32];
    escreve8(DCR, 0x49);
    escreve8(RCR, 0x20);
    escreve8(TCR, 0x02);
    dma_ler(0, prom, 32);
    uint8_t soma = 0, ff = 0xFF;
    for (int i = 0; i < 6; i++) { mac[i] = prom[2 * i]; soma |= mac[i]; ff &= mac[i]; }
    if (!soma || ff == 0xFF) return false;   /* todo ceros o todo unos: no hay PROM */
    if (exigir_ww && !(prom[28] == 0x57 && prom[29] == 0x57)) return false;
    return true;
}

/* ------------------------------------------------------------ PCI */

static const struct { uint16_t fab, mod; const char *nome; } modelos_pci[] = {
    { 0x10EC, 0x8029, "Realtek RTL8029 (NE2000 PCI)" },
    { 0x1050, 0x0940, "Winbond W89C940 (NE2000 PCI)" },
    { 0x1106, 0x0926, "VIA VT86C926 (NE2000 PCI)" },
    { 0x11F6, 0x1401, "Compex RL2000 (NE2000 PCI)" },
};

static void completar(placa_t *p, const char *descricao, int irq)
{
    p->nome = "ne2000";
    p->descricao = descricao;
    p->irq = irq;
    p->enviar = enviar;
    p->receber = receber;
    p->cabo = cabo;
    p->atender = atender;
}

bool ne2000_procurar(placa_t *p)
{
    pci_disp_t achados[8];
    int n = pci_procurar(0x02, 0x00, 0x00, achados, 8);
    for (int i = 0; i < n; i++) {
        pci_disp_t *d = &achados[i];
        const char *nome = NULL;
        for (unsigned k = 0; k < sizeof modelos_pci / sizeof modelos_pci[0]; k++)
            if (d->fabricante == modelos_pci[k].fab && d->modelo == modelos_pci[k].mod) nome = modelos_pci[k].nome;
        if (!nome) continue;
        uint32_t bar = pci_le32(d, 0x10);
        if (!(bar & 1)) continue;   /* BAR0 es la franja de puertos */
        io = (uint16_t)(bar & ~3u);
        pci_escreve32(d, 0x04, (pci_le32(d, 0x04) | 0x1) & ~(1u << 10));   /* puertos de E/S */
        via_pci = true;
        if (!reiniciar_chip()) { klog("[NE2000] %02x:%02x.%x no responde al reinicio\n", d->barramento, d->dispositivo, d->funcao); continue; }
        escreve8(CR, CR_STP | CR_RD_ABORTAR);
        uint8_t mac[6];
        if (!ler_prom(mac, false)) { klog("[NE2000] PROM sin MAC valida\n"); continue; }
        memcpy(p->mac, mac, 6);
        int irq = (int)(pci_le32(d, 0x3C) & 0xFF);
        imr_ativa = ISR_PRX | ISR_PTX | ISR_RXE | ISR_TXE | ISR_OVW;
        configurar(mac, imr_ativa);
        completar(p, nome, irq);
        p->pci = *d;
        p->provocar_irq = provocar_irq;
        klog("[NE2000] %02x:%02x.%x (%04x:%04x) portas %x, irq %d, MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
             d->barramento, d->dispositivo, d->funcao, d->fabricante, d->modelo, io, irq,
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ ISA */

/* Una franja de E/S de [base, base+FAIXA) usada por una tarjeta PCI: no se toca. */
static bool faixa_pci_ocupada(uint16_t base)
{
    pci_disp_t v[32];
    int n = pci_listar(v, 32);
    for (int i = 0; i < n; i++) {
        for (uint8_t r = 0x10; r <= 0x24; r += 4) {
            uint32_t bar = pci_le32(&v[i], r);
            if (!(bar & 1)) continue;
            uint32_t ini = bar & ~3u;
            if (ini && ini < (uint32_t)base + FAIXA && ini + 0x08 > base) return true;   /* tamaño desconocido: al menos 8 */
        }
    }
    return false;
}

/* Averigua la IRQ de una tarjeta ISA: pide un DMA remoto terminado con la IMR abierta y mira el IRR. */
static int irq_provar(void)
{
    const uint16_t candidatas = (1u << 3) | (1u << 4) | (1u << 5) | (1u << 9) | (1u << 10) | (1u << 11) | (1u << 12) | (1u << 15);
    uint8_t lixo[1] = { 0 };
    bool i0 = int_salva_desliga();
    uint16_t antes = pic_pedidos();
    /* Un DMA remoto de una palabra, sin limpiar el RDC todavía: la línea tiene que seguir alta
     * mientras se mira el IRR (algunos 8259, como el de 86Box, olvidan la petición al bajar la línea). */
    escreve8(ISR, ISR_RDC);
    escreve8(IMR, ISR_RDC);
    escreve8(RBCR0, 2); escreve8(RBCR1, 0);
    escreve8(RSAR0, 0); escreve8(RSAR1, 0);
    escreve8(CR, CR_STA | CR_RD_LER);
    lixo[0] = (uint8_t)porta_le16((uint16_t)(io + PORTA_DADOS));
    (void)lixo[0];
    uint16_t novos = 0;
    for (uint32_t i = 0; i < 2000 && !novos; i++) {
        novos = (uint16_t)(pic_pedidos() & ~antes & candidatas);
        if (!novos) porta_pausa();
    }
    escreve8(IMR, 0);
    escreve8(ISR, 0xFF);
    int_restaura(i0);
    if (!novos || (novos & (novos - 1))) return -1;   /* ninguna, o varias a la vez: no se fía */
    return __builtin_ctz(novos);
}

bool ne2000_isa_procurar(placa_t *p)
{
    uint8_t opc = cabecalho_boot[CAB_NE2K_OPC];
    if (!(opc & 1)) { klog("[NE2000] sondeo ISA desactivado (opcion de arranque --ne2k-isa para activarlo)\n"); return false; }
    static const uint16_t bases[] = { 0x300, 0x280, 0x320, 0x340 };
    for (unsigned i = 0; i < sizeof bases / sizeof bases[0]; i++) {
        io = bases[i];
        via_pci = false;
        if (faixa_pci_ocupada(io)) continue;
        if (le8(CR) == 0xFF) continue;          /* bus flotante: nadie responde, no se escribe nada */
        if (!reiniciar_chip()) continue;        /* tras el reinicio, el ISR de un 8390 tiene RST */
        escreve8(CR, CR_STP | CR_RD_ABORTAR);
        if (le8(CR) != (CR_STP | CR_RD_ABORTAR)) continue;
        uint8_t mac[6];
        if (!ler_prom(mac, true)) continue;
        memcpy(p->mac, mac, 6);
        int irq = cabecalho_boot[CAB_NE2K_IRQ];
        if (irq < 3 || irq > 15 || irq == 2) irq = irq_provar();
        imr_ativa = irq > 0 ? (uint8_t)(ISR_PRX | ISR_PTX | ISR_RXE | ISR_TXE | ISR_OVW) : 0;
        configurar(mac, imr_ativa);
        completar(p, "NE2000 compatible (ISA)", irq);
        char det[40];
        snformatar(det, sizeof det, "DP8390, MAC %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        isa_registrar("NE2000 (red ISA)", io, irq > 0 ? (uint8_t)irq : 0xFF, 0xFF, 0xFF, det);
        klog("[NE2000] ISA portas %x, irq %d%s, MAC %02x:%02x:%02x:%02x:%02x:%02x\n", io, irq,
             irq > 0 ? "" : " (sin IRQ: sondagem)", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return true;
    }
    return false;
}
