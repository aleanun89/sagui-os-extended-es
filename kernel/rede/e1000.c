// SPDX-License-Identifier: GPL-3.0-only
/* e1000.c - driver da Intel PRO/1000 (82540EM e parentes). Ver D046.
 *
 * Base: o driver do PR #3 do repositório original (M4R4G0N), com
 * interrupções, mais modelos e estatísticas.
 *
 * É a placa que o QEMU emula por padrão e a do VirtualBox ("Intel PRO/1000
 * MT Desktop", 82540EM; "T Server", 82543GC; "MT Server", 82545EM). A placa
 * conversa com a memória por dois anéis de descritores de 16 bytes:
 *   - recepção: cada descritor aponta para um buffer de 2 KB; a placa grava o
 *     quadro, liga o bit DD ("feito") e avança RDH. O driver consome, desliga
 *     o DD e devolve o descritor escrevendo o índice em RDT;
 *   - envio: o driver preenche o descritor seguinte com o endereço e o
 *     tamanho do quadro e avança TDT; a placa manda e liga o DD.
 * Interrupções: a placa avisa quando chegou quadro (RXT0), a fila encheu
 * (RXO, RXDMT0) ou o enlace mudou (LSC). O manipulador só lê o ICR (que
 * limpa a causa e solta a linha); quem consome o anel é a pilha (rede.c).
 * Se a IRQ não funcionar, a tarefa da rede consulta o anel a cada tique.
 * Registradores e formatos: manual do desenvolvedor dos controladores
 * Intel 8254x. O kernel enxerga a memória física 1:1 (D008), então o
 * endereço dos buffers é o próprio ponteiro. */
#include "rede.h"
#include "console.h"
#include "memoria.h"
#include "paginacao.h"
#include "portas.h"
#include "texto.h"

#define CTRL    0x0000
#define STATUS  0x0008
#define EERD    0x0014
#define ICR     0x00C0
#define IMS     0x00D0
#define ICS     0x00C8   /* D211: "Interrupt Cause Set": provoca la causa escrita */
#define IMC     0x00D8
#define RCTL    0x0100
#define TCTL    0x0400
#define TIPG    0x0410
#define RDBAL   0x2800
#define RDBAH   0x2804
#define RDLEN   0x2808
#define RDH     0x2810
#define RDT     0x2818
#define RDTR    0x2820
#define TDBAL   0x3800
#define TDBAH   0x3804
#define TDLEN   0x3808
#define TDH     0x3810
#define TDT     0x3818
#define MTA     0x5200
#define RAL0    0x5400
#define RAH0    0x5404

#define CTRL_ASDE  (1u << 5)
#define CTRL_SLU   (1u << 6)
#define CTRL_RST   (1u << 26)
#define RCTL_EN    (1u << 1)
#define RCTL_BAM   (1u << 15)    /* aceita broadcast (ARP, DHCP) */
#define RCTL_SECRC (1u << 26)    /* a placa tira o CRC */
#define TCTL_EN    (1u << 1)
#define TCTL_PSP   (1u << 3)     /* completa quadros curtos até 60 bytes */
#define CMD_EOP    0x01
#define CMD_IFCS   0x02
#define CMD_RS     0x08
#define DD         0x01
#define EOP        0x02
#define INT_LSC    (1u << 2)
#define INT_RXDMT0 (1u << 4)
#define INT_RXO    (1u << 6)
#define INT_RXT0   (1u << 7)

#define N_RX   64                 /* RDLEN precisa ser múltiplo de 128 bytes */
#define N_TX   32
#define BUF    2048

static volatile uint8_t *regs;
static uint8_t *rx, *tx;          /* anéis de descritores */
static uint8_t *rx_buf, *tx_buf;
static uint32_t rx_prox, tx_prox;

static inline uint32_t le(uint32_t r) { return *(volatile uint32_t *)(regs + r); }
static inline void escreve(uint32_t r, uint32_t v) { *(volatile uint32_t *)(regs + r) = v; }

static bool enviar(const void *quadro, uint32_t n)
{
    if (n > ETH_MAX) return false;
    volatile uint8_t *d = tx + tx_prox * 16;
    if (!(d[12] & DD)) return false;   /* a placa ainda não mandou este: fila cheia */
    memcpy(tx_buf + tx_prox * BUF, quadro, n);
    *(volatile uint32_t *)(d + 0) = (uint32_t)(tx_buf + tx_prox * BUF);
    *(volatile uint32_t *)(d + 4) = 0;
    *(volatile uint16_t *)(d + 8) = (uint16_t)n;
    d[10] = 0;
    d[11] = CMD_EOP | CMD_IFCS | CMD_RS;
    d[12] = 0;
    tx_prox = (tx_prox + 1) % N_TX;
    __asm__ volatile("" ::: "memory");   /* descritor completo antes de avisar a placa */
    escreve(TDT, tx_prox);
    return true;
}

static int receber(void *destino)
{
    volatile uint8_t *d = rx + rx_prox * 16;
    uint8_t st = d[12];
    if (!(st & DD)) return 0;
    __asm__ volatile("" ::: "memory");
    int n = *(volatile uint16_t *)(d + 8);
    /* Quadro com erro ou maior que um buffer (não deveria vir: LPE desligado). */
    if (d[13] || !(st & EOP) || n > ETH_MAX || n < 14) n = -1;
    else memcpy(destino, rx_buf + rx_prox * BUF, (uint32_t)n);
    d[12] = 0;
    escreve(RDT, rx_prox);   /* devolve o descritor à placa */
    rx_prox = (rx_prox + 1) % N_RX;
    return n;
}

static bool cabo(void) { return le(STATUS) & 2; }

/* D211: la autoprueba de la IRQ. Escribir una causa en el ICS levanta la
 * interrupción como si hubiera pasado (aquí "cambió el enlace", que el
 * driver solo lee y limpia). */
static bool provocar_irq(void)
{
    escreve(ICS, INT_LSC);
    return true;
}

static bool atender(void)
{
    return le(ICR) != 0;   /* ler o ICR limpa as causas e solta a linha */
}

static uint16_t eeprom(uint8_t endereco)
{
    escreve(EERD, ((uint32_t)endereco << 8) | 1);
    for (int i = 0; i < 100000; i++) {
        uint32_t v = le(EERD);
        if (v & (1u << 4)) return (uint16_t)(v >> 16);
    }
    return 0;
}

/* Modelos com o mesmo conjunto de registradores do 82540EM. O 82574L
 * (e1000e) e as placas PCIe mais novas são diferentes. */
static const struct { uint16_t modelo; const char *nome; } modelos[] = {
    { 0x100E, "Intel 82540EM (PRO/1000 MT)" }, { 0x100F, "Intel 82545EM (PRO/1000 MT)" },
    { 0x1004, "Intel 82543GC (PRO/1000 T)" },  { 0x1015, "Intel 82540EM (LOM)" },
    { 0x1016, "Intel 82540EP" },               { 0x1017, "Intel 82540EP" },
    { 0x101E, "Intel 82540EP (portátil)" },    { 0x1019, "Intel 82547EI" },
    { 0x1026, "Intel 82545GM" },               { 0x1076, "Intel 82541GI" },
    { 0x107C, "Intel 82541PI" },
};

bool e1000_procurar(placa_t *p)
{
    pci_disp_t achados[4];
    int n = pci_procurar(0x02, 0x00, 0x00, achados, 4);   /* rede, Ethernet */
    pci_disp_t *d = NULL;
    const char *desc = NULL;
    for (int i = 0; i < n && !d; i++)
        for (uint32_t k = 0; k < sizeof modelos / sizeof modelos[0]; k++)
            if (achados[i].fabricante == 0x8086 && achados[i].modelo == modelos[k].modelo) {
                d = &achados[i];
                desc = modelos[k].nome;
                break;
            }
    if (!d) return false;

    uint32_t bar = pci_le32(d, 0x10);
    if (bar & 1) return false;                              /* precisa ser memória, não porta */
    /* memória + mestre do barramento (DMA); bit 10 desligado = interrupções permitidas */
    pci_escreve32(d, 0x04, (pci_le32(d, 0x04) | 0x6) & ~(1u << 10));
    regs = (volatile uint8_t *)janela_mapear(bar & ~0xFu, 128 * 1024);
    uint32_t desc_fis = memfisica_alocar();
    uint32_t brx = memfisica_alocar_seguidas(N_RX * BUF / PAGINA);
    uint32_t btx = memfisica_alocar_seguidas(N_TX * BUF / PAGINA);
    if (!regs || !desc_fis || !brx || !btx) { klog("[E1000] sem memoria\n"); return false; }
    rx = (uint8_t *)desc_fis;
    tx = rx + N_RX * 16;                                     /* 1 KB + 512 bytes: cabem numa página */
    rx_buf = (uint8_t *)brx;
    tx_buf = (uint8_t *)btx;
    memset(rx, 0, PAGINA);

    escreve(IMC, 0xFFFFFFFFu);
    escreve(CTRL, le(CTRL) | CTRL_RST);
    for (int i = 0; i < 100000 && (le(CTRL) & CTRL_RST); i++) porta_pausa();
    escreve(IMC, 0xFFFFFFFFu);                               /* o reset religa o que estava desligado */
    (void)le(ICR);
    /* Liga o link e desliga os resets do PHY e o "sinal invertido". */
    escreve(CTRL, (le(CTRL) | CTRL_SLU | CTRL_ASDE) & ~((1u << 3) | (1u << 7) | (1u << 31)));

    /* O MAC vem da EEPROM; a placa costuma copiá-lo sozinha para RAL/RAH. */
    uint32_t ral = le(RAL0), rah = le(RAH0);
    if (!(rah & (1u << 31))) {
        uint16_t w0 = eeprom(0), w1 = eeprom(1), w2 = eeprom(2);
        ral = w0 | (uint32_t)w1 << 16;
        rah = w2 | (1u << 31);
        escreve(RAL0, ral);
        escreve(RAH0, rah);
    }
    for (int i = 0; i < 4; i++) p->mac[i] = (uint8_t)(ral >> (8 * i));
    p->mac[4] = (uint8_t)rah;
    p->mac[5] = (uint8_t)(rah >> 8);
    for (int i = 0; i < 128; i++) escreve(MTA + 4 * i, 0);

    for (int i = 0; i < N_RX; i++) *(uint32_t *)(rx + i * 16) = (uint32_t)(rx_buf + i * BUF);
    escreve(RDBAL, (uint32_t)rx);
    escreve(RDBAH, 0);
    escreve(RDLEN, N_RX * 16);
    escreve(RDH, 0);
    escreve(RDT, N_RX - 1);
    escreve(RDTR, 0);                                        /* interrupção sem atraso */
    escreve(RCTL, RCTL_EN | RCTL_BAM | RCTL_SECRC);          /* buffers de 2048 bytes (BSIZE = 0) */

    for (int i = 0; i < N_TX; i++) tx[i * 16 + 12] = DD;     /* todos livres */
    escreve(TDBAL, (uint32_t)tx);
    escreve(TDBAH, 0);
    escreve(TDLEN, N_TX * 16);
    escreve(TDH, 0);
    escreve(TDT, 0);
    escreve(TCTL, TCTL_EN | TCTL_PSP | (0x0Fu << 4) | (0x40u << 12));   /* repetições e distância de colisão */
    escreve(TIPG, 0x0060200A);                               /* intervalo entre quadros recomendado (cobre) */
    rx_prox = tx_prox = 0;

    escreve(IMS, INT_RXT0 | INT_RXO | INT_RXDMT0 | INT_LSC);
    (void)le(ICR);

    p->nome = "e1000";
    p->descricao = desc;
    p->pci = *d;
    p->irq = (int)(pci_le32(d, 0x3C) & 0xFF);
    p->enviar = enviar;
    p->receber = receber;
    p->cabo = cabo;
    p->atender = atender;
    p->provocar_irq = provocar_irq;
    klog("[E1000] %02x:%02x.%x (%04x:%04x) em %x, irq %d, MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
         d->barramento, d->dispositivo, d->funcao, d->fabricante, d->modelo, bar & ~0xFu, p->irq,
         p->mac[0], p->mac[1], p->mac[2], p->mac[3], p->mac[4], p->mac[5]);
    return true;
}
