/* rtl8139.c - driver da Realtek RTL8139 (QEMU: -nic user,model=rtl8139). Ver D046.
 *
 * Uma placa antiga e simples, acessada por portas de E/S:
 *   - recepção: um único buffer circular de 8 KB (+16 bytes e mais 1,5 KB de
 *     folga para o modo WRAP, em que a placa escreve o fim do quadro depois
 *     do fim do anel em vez de dar a volta). Cada quadro vem precedido de 4
 *     bytes (estado e tamanho com o CRC) e começa num endereço múltiplo de 4.
 *     O driver avisa até onde leu escrevendo CAPR (com um desconto de 16
 *     bytes, peculiaridade do chip);
 *   - envio: quatro "caixas" fixas (TSAD0-3 com o endereço, TSD0-3 com o
 *     tamanho), usadas em rodízio; o bit OWN do TSD diz que a placa já
 *     copiou o quadro.
 * Interrupções: ROK/RER/RXOVW/TOK/LinkChg; o manipulador limpa o ISR. */
#include "rede.h"
#include "console.h"
#include "memoria.h"
#include "portas.h"
#include "texto.h"

#define IDR0    0x00
#define TSD0    0x10
#define TSAD0   0x20
#define RBSTART 0x30
#define CR      0x37
#define CAPR    0x38
#define IMR     0x3C
#define ISR     0x3E
#define TCR     0x40
#define RCR     0x44
#define CONFIG1 0x52
#define MSR     0x58

#define CR_BUFE  0x01
#define CR_TE    0x04
#define CR_RE    0x08
#define CR_RST   0x10
#define TSD_OWN  (1u << 13)
#define RX_ROK   0x0001

#define RX_ANEL  8192
#define RX_TOTAL (RX_ANEL + 16 + 1536)
#define TX_BUF   1536

static uint16_t io;
static uint8_t *rx_buf, *tx_buf;
static uint32_t rx_pos;
static int tx_prox;
static bool tx_usado[4];

static uint8_t le8(uint16_t r)  { return porta_le8((uint16_t)(io + r)); }
static uint16_t le16(uint16_t r) { return porta_le16((uint16_t)(io + r)); }
static uint32_t le32(uint16_t r) { return porta_le32((uint16_t)(io + r)); }
static void escreve8(uint16_t r, uint8_t v)   { porta_escreve8((uint16_t)(io + r), v); }
static void escreve16(uint16_t r, uint16_t v) { porta_escreve16((uint16_t)(io + r), v); }
static void escreve32(uint16_t r, uint32_t v) { porta_escreve32((uint16_t)(io + r), v); }

static bool enviar(const void *quadro, uint32_t n)
{
    if (n > ETH_MAX) return false;
    uint16_t tsd = (uint16_t)(TSD0 + 4 * tx_prox);
    if (tx_usado[tx_prox] && !(le32(tsd) & TSD_OWN)) return false;   /* a placa ainda não copiou */
    uint8_t *b = tx_buf + tx_prox * TX_BUF;
    memcpy(b, quadro, n);
    if (n < ETH_MIN) { memset(b + n, 0, ETH_MIN - n); n = ETH_MIN; }   /* esta placa não completa sozinha */
    escreve32((uint16_t)(TSAD0 + 4 * tx_prox), (uint32_t)b);
    escreve32(tsd, n);    /* OWN = 0: a placa começa a copiar; limiar de envio 0 = 8 bytes */
    tx_usado[tx_prox] = true;
    tx_prox = (tx_prox + 1) & 3;
    return true;
}

static void reiniciar_recepcao(void)
{
    escreve8(CR, CR_TE);
    rx_pos = 0;
    escreve16(CAPR, (uint16_t)(0 - 16));
    escreve8(CR, CR_TE | CR_RE);
    escreve32(RCR, 0x0E | (1u << 7) | (7u << 8));   /* como em rtl8139_procurar */
}

static int receber(void *destino)
{
    if (le8(CR) & CR_BUFE) return 0;   /* anel vazio */
    uint8_t *h = rx_buf + rx_pos;
    uint16_t estado = (uint16_t)(h[0] | h[1] << 8);
    uint16_t tam = (uint16_t)(h[2] | h[3] << 8);   /* inclui os 4 bytes do CRC */
    if (!(estado & RX_ROK) || tam < 18 || tam > ETH_MAX + 4) {
        /* Cabeçalho incoerente: o manual manda reiniciar a recepção. */
        reiniciar_recepcao();
        return -1;
    }
    uint32_t n = tam - 4u;
    memcpy(destino, h + 4, n);   /* com WRAP o quadro é contíguo mesmo passando do fim do anel */
    rx_pos = (rx_pos + tam + 4 + 3) & ~3u;
    rx_pos %= RX_ANEL;
    escreve16(CAPR, (uint16_t)(rx_pos - 16));
    return (int)n;
}

static bool cabo(void) { return !(le8(MSR) & 0x04); }   /* LINKB: 0 = enlace bom */

static bool atender(void)
{
    uint16_t s = le16(ISR);
    if (!s || s == 0xFFFF) return false;
    escreve16(ISR, s);   /* escrever 1 limpa */
    if (s & 0x0010) {    /* RXOVW: anel estourou */
        klog("[RTL8139] anel de recepcao cheio\n");
    }
    return true;
}

bool rtl8139_procurar(placa_t *p)
{
    pci_disp_t achados[4];
    int n = pci_procurar(0x02, 0x00, 0x00, achados, 4);
    pci_disp_t *d = NULL;
    for (int i = 0; i < n && !d; i++)
        if (achados[i].fabricante == 0x10EC && achados[i].modelo == 0x8139) d = &achados[i];
    if (!d) return false;
    uint32_t bar = pci_le32(d, 0x10);
    if (!(bar & 1)) return false;   /* BAR0 é a faixa de portas */
    io = (uint16_t)(bar & ~3u);
    pci_escreve32(d, 0x04, (pci_le32(d, 0x04) | 0x5) & ~(1u << 10));   /* portas + mestre do barramento */

    uint32_t brx = memfisica_alocar_seguidas((RX_TOTAL + PAGINA - 1) / PAGINA);
    uint32_t btx = memfisica_alocar_seguidas((4 * TX_BUF + PAGINA - 1) / PAGINA);
    if (!brx || !btx) { klog("[RTL8139] sem memoria\n"); return false; }
    rx_buf = (uint8_t *)brx;
    tx_buf = (uint8_t *)btx;
    memset(rx_buf, 0, RX_TOTAL);

    escreve8(CONFIG1, 0);   /* acorda a placa */
    escreve8(CR, CR_RST);
    for (int i = 0; i < 100000 && (le8(CR) & CR_RST); i++) porta_pausa();
    for (int i = 0; i < 6; i++) p->mac[i] = le8((uint16_t)(IDR0 + i));

    escreve32(RBSTART, (uint32_t)rx_buf);
    escreve16(IMR, 0x0001 | 0x0002 | 0x0004 | 0x0008 | 0x0010 | 0x0020 | 0x0040);   /* ROK RER TOK TER RXOVW LinkChg FIFOOVW */
    escreve8(CR, CR_TE | CR_RE);
    /* RCR: AB (broadcast) | AM (multicast) | APM (o nosso MAC), WRAP; sem
     * AAP (todos os quadros). Limiar e DMA máximos padrão. */
    escreve32(RCR, 0x0E | (1u << 7) | (7u << 8));
    escreve32(TCR, 0x03000600);   /* IFG padrão, DMA máximo 1024 */
    escreve16(CAPR, (uint16_t)(0 - 16));
    rx_pos = 0;
    tx_prox = 0;
    for (int i = 0; i < 4; i++) tx_usado[i] = false;
    escreve16(ISR, 0xFFFF);

    p->nome = "rtl8139";
    p->descricao = "Realtek RTL8139";
    p->pci = *d;
    p->irq = (int)(pci_le32(d, 0x3C) & 0xFF);
    p->enviar = enviar;
    p->receber = receber;
    p->cabo = cabo;
    p->atender = atender;
    klog("[RTL8139] %02x:%02x.%x portas %x, irq %d, MAC %02x:%02x:%02x:%02x:%02x:%02x\n",
         d->barramento, d->dispositivo, d->funcao, io, p->irq,
         p->mac[0], p->mac[1], p->mac[2], p->mac[3], p->mac[4], p->mac[5]);
    return true;
}
