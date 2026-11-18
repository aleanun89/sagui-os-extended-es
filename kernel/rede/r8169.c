/* r8169.c - driver EXPERIMENTAL da família Realtek RTL8101E/8102E/8105E
 * ("RTL810x", PCI 10ec:8136) e RTL8168/8169 (10ec:8168, 8169). Ver D046.
 *
 * Provavelmente é a Ethernet do Lenovo G470, mas o QEMU não emula nenhuma
 * placa desta família: este código foi escrito pela documentação pública
 * (folhas de dados da Realtek, wiki do OSDev, driver r8169 do Linux) e
 * no começo nunca tinha rodado numa placa de verdade. Por isso o boot só
 * DETECTA a placa; o driver só é ligado a pedido, com "ifconfig eth0
 * experimental". Testes reais (D091): funciona no HP dv7 (2009); na RTL8111H
 * de uma ASUS B450 (2022) enviava mas não recebia, e ganhou o inicio
 * "moderno" (versões 40..48, sequências do Linux em kernel/terceros) e um
 * diagnóstico no "ip" (revisão do chip, motivos dos descartes, contadores).
 *
 * Funcionamento (o mesmo desenho do e1000): dois anéis de descritores de 16
 * bytes (opts1 com OWN/EOR/FS/LS e o tamanho, opts2, endereço de 64 bits),
 * alinhados em 256 bytes; o envio é avisado pelo TxPoll (NPQ). O que falta
 * comparado ao driver do Linux: as dezenas de ajustes do PHY e de
 * "erratas" de cada revisão do chip (rtl_hw_start_8105e_1 etc.). Se a placa
 * não receber nada, é por aí que se deve começar. */
/* D259: sin las tablas y secuencias de Linux (GPL-2.0-only) cuando se compila sin GPL o lo
 * publicable (-DSAGUI_PUBLICABLE), o si la carpeta no existe (arbol publico): solo el inicio
 * clasico propio. */
#if defined(SAGUI_SEM_GPL) || defined(SAGUI_PUBLICABLE) || !__has_include("../terceros/linux/r8169/r8169_inicio.h")
#define SAGUI_R8169_CLASICO
#endif
#include "rede.h"
#include "console.h"
#include "memoria.h"
#include "paginacao.h"
#include "portas.h"
#include "texto.h"
#ifdef SAGUI_R8169_CLASICO
#include "r8169_sem_gpl.h"   /* compilación sin GPL (D109): solo el inicio clásico, con definiciones propias */
#else
#include "r8169_logica.h"
#endif
#include "r8169_ajustes.h"

#ifdef SAGUI_R8169_CLASICO
#define R8169_LE_MISC 0
#define PERFIL_NOME "clásico"
#define PERFIL_NOME_LOG "legado"
#else
#define PERFIL_NOME_LOG (perfil == PERFIL_MODERNO ? "moderno" : perfil == PERFIL_8168E ? "8168e" : "legado")
#define PERFIL_NOME (perfil == PERFIL_MODERNO ? "moderno (8168g/h)" : perfil == PERFIL_8168E ? "8168e (Linux)" : "clásico")
#define R8169_LE_MISC (versao_mac >= 34 ? le32(R8169_MISC) : 0)
#endif

#define IDR0     0x00
#define MAR0     0x08
#define TXDESC   0x20
#define CFG0     0x51   /* Config0..Config5: 0x51..0x56 */
#define CMD      0x37
#define TXPOLL   0x38
#define IMR      0x3C
#define ISR      0x3E
#define TXCFG    0x40
#define RXCFG    0x44
#define CFG9346  0x50
#define PHYSTAT  0x6C
#define RXMAX    0xDA
#define CPCMD    0xE0
#define RXDESC   0xE4
#define MAXTX    0xEC

#define OWN (1u << 31)
#define EOR (1u << 30)
#define FS  (1u << 29)
#define LS  (1u << 28)

#define N_RX 32
#define N_TX 16
#define BUF  2048

static uint16_t io;
static volatile uint32_t *rxd, *txd;   /* 4 palavras por descritor */
static uint8_t *rx_buf, *tx_buf;
static uint32_t rx_prox, tx_prox;
static pci_disp_t pci_placa;           /* para el "Link Control" PCIe y el diagnóstico */
static volatile uint8_t *contadores_chip;   /* 64 bytes alinhados em 64, para o DumpTallyCounter */
/* D124: direcciones físicas (las que ve la tarjeta). Los punteros de arriba
 * son los de la CPU: iguales a las físicas en el inicio clásico (mapa 1:1) o
 * un alias sin caché en los demás (ver dma_para_cpu). */
static uint32_t fis_desc, fis_rx, fis_tx;
#define FIS_RXD  (fis_desc)
#define FIS_TXD  (fis_desc + 2048)
#define FIS_CONT (fis_desc + 3072)
static bool sem_cache;
/* D124: distancia entre búferes de recepción (2 KB en el clásico, 16 KB en
 * los demás para poder probar los tamaños de Linux) y los ajustes en uso
 * (RxConfig, RxMaxSize, tamaño en el descriptor), cambiables en caliente. */
static uint32_t paso = BUF;
static r8169_ajustes_t aj;
static placa_t *placa_ligada;

/* D091: a revisão do chip, pelos bits XID do TxConfig (tabela do Linux). */
static uint32_t xid;
static const char *nome_chip = "desconocido";
static char descricao[40];

/* D091: diagnóstico. Por que cada quadro recebido foi jogado fora aqui no
 * driver (antes de chegar à pilha) e quantas vezes cada causa de
 * interrupção apareceu. Mostrado pelo "ip" e no log ([R8169]). */
static struct {
    uint32_t erro_res, erro_crc, erro_runt, erro_rwt;   /* RxRES e o motivo */
    uint32_t fragmentado;      /* sem FS e LS juntos: quadro maior que o buffer */
    uint32_t curto, longo;     /* tamanho fora de 14..ETH_MAX (já sem o CRC) */
    uint32_t com_vlan;         /* opts2 com RxVlanTag (a placa tirou a etiqueta) */
    uint32_t voltas;           /* vezes que a leitura deu a volta no anel */
    uint32_t sem_descritor, fifo_cheia, erro_sistema, enlace, irq_erro_rx;
    uint32_t irq_rx_ok, irq_tx_ok;   /* D124: la tarjeta avisa de tramas recibidas/enviadas */
    uint32_t sem_escrever;     /* D124: descriptor devuelto con la marca intacta al principio del búfer */
    uint32_t registrados;      /* quantos descartes já foram para o log */
} diag;
#define DIAG_LOG_MAX 12

/* D124: las primeras tramas recibidas, tal como las dejó la tarjeta: índice
 * del descriptor, dirección física del búfer, el descriptor entero y los
 * primeros 64 bytes. Antes de dar cada búfer a la tarjeta se marca su
 * principio con MARCA_RX: si una trama sale con la marca, la tarjeta
 * devolvió el descriptor sin escribir ahí los datos (los escribió en otro
 * sitio, o la CPU no los ve). */
#define N_AMOSTRAS 4
#define AMOSTRA_BYTES 256   /* se muestran 256 de la primera y 64 de las demás */
#define MARCA_RX 0xA5
static struct {
    uint32_t indice, fisico, opts1, opts2, end_baixo, end_alto;
    uint8_t dados[AMOSTRA_BYTES];
} amostras[N_AMOSTRAS];
static uint32_t n_amostras;

static uint8_t le8(uint16_t r)  { return porta_le8((uint16_t)(io + r)); }
static uint16_t le16(uint16_t r) { return porta_le16((uint16_t)(io + r)); }
static void escreve8(uint16_t r, uint8_t v)   { porta_escreve8((uint16_t)(io + r), v); }
static void escreve16(uint16_t r, uint16_t v) { porta_escreve16((uint16_t)(io + r), v); }
static void escreve32(uint16_t r, uint32_t v) { porta_escreve32((uint16_t)(io + r), v); }
static uint32_t le32(uint16_t r) { return porta_le32((uint16_t)(io + r)); }

/* D091: as sequências do Linux para as revisões novas (kernel/terceros),
 * com os acessos aos registradores daqui. */
static int versao_mac;                 /* RTL_GIGA_MAC_VER_xx; 0 = desconhecida */
#define rtl_versao versao_mac
#define rtl_r8   le8
#define rtl_r16  le16
#define rtl_r32  le32
#define rtl_w8   escreve8
#define rtl_w16  escreve16
#define rtl_w32  escreve32
/* Desplazamiento de la capacidad PCI Express en el espacio de configuración
 * (0 = no la tiene). */
static uint8_t cap_pcie(const pci_disp_t *d)
{
    if (!(pci_le32(d, 0x04) & (1u << 20))) return 0;   /* sem lista de capacidades */
    uint8_t c = (uint8_t)(pci_le32(d, 0x34) & 0xFC);
    for (int voltas = 0; c && voltas < 48; voltas++) {
        uint32_t cab = pci_le32(d, c);
        if ((cab & 0xFF) == 0x10) return c;
        c = (uint8_t)((cab >> 8) & 0xFC);
    }
    return 0;
}

/* Apaga bits del "Link Control" (cap + 0x10, 16 bits bajos). Los 16 altos
 * son el "Link Status": se escriben a 0 para no borrar sus bits RW1C.
 * Devuelve los bits que estaban encendidos. */
static uint16_t lnkctl_apagar(const pci_disp_t *d, uint16_t bits)
{
    uint8_t c = cap_pcie(d);
    if (!c) return 0;
    uint16_t lc = (uint16_t)pci_le32(d, (uint8_t)(c + 0x10));
    if (lc & bits) pci_escreve32(d, (uint8_t)(c + 0x10), (uint32_t)(lc & ~bits));
    return lc & bits;
}

/* Lo mismo para el "Device Control" (cap + 8; los 16 altos son el "Device
 * Status", con bits RW1C). */
static uint16_t devctl_apagar(const pci_disp_t *d, uint16_t bits)
{
    uint8_t c = cap_pcie(d);
    if (!c) return 0;
    uint16_t dc = (uint16_t)pci_le32(d, (uint8_t)(c + 8));
    if (dc & bits) pci_escreve32(d, (uint8_t)(c + 8), (uint32_t)(dc & ~bits));
    return dc & bits;
}
static uint16_t devctl_bios;   /* como lo dejó la BIOS (diagnóstico) */

/* rtl_disable_clock_request() de Linux: CLKREQ# (bit 8 del Link Control). */
static __attribute__((unused)) void sagui_pcie_desliga_clkreq(void) { lnkctl_apagar(&pci_placa, 1u << 8); }

#ifndef SAGUI_R8169_CLASICO
static void rtl_espera_us(uint32_t us) { while (us--) porta_pausa(); }   /* ~1 us por acesso à porta 0x80 */
#include "../terceros/linux/r8169/r8169_inicio.h"
#endif

static bool enviar(const void *quadro, uint32_t n)
{
    if (n > ETH_MAX) return false;
    volatile uint32_t *d = txd + tx_prox * 4;
    if (d[0] & OWN) return false;
    uint8_t *b = tx_buf + tx_prox * BUF;
    memcpy(b, quadro, n);
    if (n < ETH_MIN) { memset(b + n, 0, ETH_MIN - n); n = ETH_MIN; }
    d[1] = 0;
    d[2] = fis_tx + tx_prox * BUF;
    d[3] = 0;
    __asm__ volatile("" ::: "memory");
    d[0] = OWN | FS | LS | (tx_prox == N_TX - 1 ? EOR : 0) | n;
    tx_prox = (tx_prox + 1) % N_TX;
    escreve8(TXPOLL, 0x40);   /* NPQ: há quadro na fila normal */
    return true;
}

/* Guarda no log os primeiros descartes, com o estado do descritor e o
 * começo do quadro (MAC destino e ethertype). */
static void diag_registrar(const char *motivo, uint32_t o, uint32_t o2, const uint8_t *b)
{
    if (diag.registrados >= DIAG_LOG_MAX) return;
    diag.registrados++;
    klog("[R8169] descarte %u (%s): opts1=%08x opts2=%08x destino %02x:%02x:%02x:%02x:%02x:%02x tipo %02x%02x\n",
         diag.registrados, motivo, o, o2, b[0], b[1], b[2], b[3], b[4], b[5], b[12], b[13]);
}

static int receber(void *destino)
{
    volatile uint32_t *d = rxd + rx_prox * 4;
    uint32_t o = d[0];
    if (o & OWN) return 0;
    __asm__ volatile("" ::: "memory");
    uint32_t o2 = d[1];
    uint8_t *b = rx_buf + rx_prox * paso;
    int n;
    const char *motivo = NULL;
    switch (r8169_classificar(o, &n)) {   /* o tamanho do descritor inclui o CRC */
    case RX_FRAGMENTADO: diag.fragmentado++; motivo = "fragmentado"; break;
    case RX_ERRO:
        diag.erro_res++;
        if (o & R8169_RxCRC) diag.erro_crc++;
        if (o & R8169_RxRUNT) diag.erro_runt++;
        if (o & R8169_RxRWT) diag.erro_rwt++;
        motivo = "error en el descriptor";
        break;
    case RX_CURTO: diag.curto++; motivo = "corto"; break;
    case RX_LONGO: diag.longo++; motivo = "largo"; break;
    }
    if (o2 & R8169_RxVlanTag) diag.com_vlan++;
    bool intacto = true;
    for (int i = 0; i < 16; i++) if (b[i] != MARCA_RX) { intacto = false; break; }
    if (intacto) diag.sem_escrever++;
    if (n_amostras < N_AMOSTRAS) {
        amostras[n_amostras].indice = rx_prox;
        amostras[n_amostras].fisico = fis_rx + rx_prox * paso;
        amostras[n_amostras].opts1 = o;
        amostras[n_amostras].opts2 = o2;
        amostras[n_amostras].end_baixo = d[2];
        amostras[n_amostras].end_alto = d[3];
        memcpy(amostras[n_amostras].dados, b, AMOSTRA_BYTES);
        n_amostras++;
    }
    if (motivo) { diag_registrar(motivo, o, o2, b); n = -1; }
    else memcpy(destino, b, (uint32_t)n);
    {   /* vuelve a marcar lo que la tarjeta dijo haber escrito (al menos 256 bytes) */
        uint32_t m = (o & 0x3FFF) + 63;
        m &= ~63u;
        if (m < AMOSTRA_BYTES) m = AMOSTRA_BYTES;
        if (m > paso) m = paso;
        memset(b, MARCA_RX, m);
    }
    d[1] = 0;
    d[2] = fis_rx + rx_prox * paso;
    d[3] = 0;
    __asm__ volatile("" ::: "memory");
    d[0] = OWN | (rx_prox == N_RX - 1 ? EOR : 0) | aj.buf;
    rx_prox = (rx_prox + 1) % N_RX;
    if (rx_prox == 0) diag.voltas++;
    return n;
}

static bool cabo(void) { return le8(PHYSTAT) & 0x02; }

static bool atender(void)
{
    uint16_t s = le16(ISR);
    if (!s || s == 0xFFFF) return false;
    escreve16(ISR, s);
    if (s & R8169_RxOverflow) diag.sem_descritor++;
    if (s & R8169_RxFIFOOver) diag.fifo_cheia++;
    if (s & R8169_SYSErr) diag.erro_sistema++;
    if (s & R8169_LinkChg) diag.enlace++;
    if (s & R8169_RxErr) diag.irq_erro_rx++;
    if (s & 0x0001) diag.irq_rx_ok++;   /* RxOK */
    if (s & 0x0004) diag.irq_tx_ok++;   /* TxOK */
    return true;
}


/* D091: como cada revisão é ligada. LEGADO é a sequência original (a que
 * funciona no HP dv7, revisão ainda desconhecida): fica para tudo o que não
 * foi revisado. MODERNO segue o rtl_hw_initialize/rtl_hw_start do Linux para
 * as RTL8168g/8111g, 8168gu, 8411b, 8168h/8111h e 8107e (versões 40..48),
 * que saem do BIOS em modo "fora de banda" e com a recepção bloqueada
 * (RXDV_GATED_EN). As 8168ep/8117 (51..53) precisam também parar o
 * firmware de gerência (DASH): ainda ficam no legado. */
static int perfil;
/* Como o BIOS deixou a placa (perfil moderno), para o diagnóstico. */
#ifndef SAGUI_R8169_CLASICO
static bool achou_oob, achou_rx_bloqueada, aspm_desligado;
#endif

/* Versões já vistas funcionando em hardware real: essas não são mais
 * experimentais e o boot liga o driver sozinho. Ver D091 (vazia até sabermos
 * o XID do HP dv7). */
/* Probadas en hardware real: 25/26 = RTL8168d/8111d (HP dv7, DHCP y ping, 2026-11-07/08);
 * 34 = RTL8168evl/8111evl (Toshiba Satellite L850, perfil 8168e con RxConfig 0x870E,
 * DHCP y ping, 2026-11-08, D124); 4 = RTL8169sb/8110sb (10ec:8169 rev 10, inicio clásico:
 * DHCP, SSH de DEBUG-APP, ping sin pérdidas y 0 errores; confirmado por el mantenedor, 2026-11-18). */
static const int versoes_confirmadas[] = { 4, 25, 26, 34 };

static bool versao_confirmada(int v)
{
    for (uint32_t i = 0; i < sizeof versoes_confirmadas / sizeof versoes_confirmadas[0]; i++)
        if (v && versoes_confirmadas[i] == v) return true;
    return false;
}

#ifndef SAGUI_R8169_CLASICO
/* Pede à placa os contadores internos (DumpTallyCounter): mostram se o chip
 * recebe quadros que não chegam ao anel (perdidos) e quantos são unicast,
 * difusão e multicast. false = a placa não terminou a tempo. */
static bool ler_contadores_chip(void)
{
    if (!contadores_chip) return false;
    memset((void *)contadores_chip, 0, 64);
    escreve32(R8169_CounterAddrHigh, 0);
    escreve32(R8169_CounterAddrLow, FIS_CONT);
    escreve32(R8169_CounterAddrLow, FIS_CONT | R8169_CounterDump);
    for (int i = 0; i < 20000; i++) {
        if (!(le32(R8169_CounterAddrLow) & R8169_CounterDump)) return true;
        porta_pausa();
    }
    return false;
}

static uint32_t cont32(int desl) { return *(volatile uint32_t *)(contadores_chip + desl); }
static uint32_t cont16(int desl) { return *(volatile uint16_t *)(contadores_chip + desl); }
#endif

/* Texto para o "ip" (várias linhas, separadas por \n). */
static void diagnostico(char *s, uint32_t max)
{
    uint32_t k = 0;
#define POE(...) do { if (k < max) { int _r = snformatar(s + k, max - k, __VA_ARGS__); k += _r > 0 ? (uint32_t)_r : 0; } } while (0)
    int da_placa = 0;
    for (int i = 0; i < N_RX; i++) if (rxd[i * 4] & OWN) da_placa++;
    POE("Chip: %s (XID %03x, versión MAC %d), inicio %s%s\n", nome_chip, xid, versao_mac,
        PERFIL_NOME,
        versao_confirmada(versao_mac) ? ", probado" : "");
    POE("Registros: CMD %02x RXCFG %08x CPCMD %04x PHY %02x IMR %04x ISR %04x MISC %08x\n", le8(CMD), le32(RXCFG),
        le16(CPCMD), le8(PHYSTAT), le16(IMR), le16(ISR), R8169_LE_MISC);
#ifndef SAGUI_R8169_CLASICO
    if (perfil == PERFIL_MODERNO)
        POE("MCU %02x (fuera de banda: %s). La BIOS la dejó: fuera de banda %s, recepción bloqueada %s, ASPM %s\n",
            le8(MCU), (le8(MCU) & NOW_IS_OOB) ? "SÍ" : "no", achou_oob ? "sí" : "no", achou_rx_bloqueada ? "sí" : "no",
            aspm_desligado ? "activo (apagado ahora)" : "apagado");
    if (perfil == PERFIL_8168E)
        POE("MCU %02x, MISC %08x, ASPM en la BIOS: %s\n", le8(MCU), le32(MISC),
            aspm_desligado ? "activo (apagado ahora)" : "apagado");
#endif
    POE("Anillo RX: siguiente %u, %d/%d de la tarjeta, vueltas %u, opts1 actual %08x\n", rx_prox, da_placa, N_RX,
        diag.voltas, rxd[rx_prox * 4]);
    POE("Descartes en el driver: error descriptor %u (CRC %u, corto %u, largo %u), fragmentado %u, "
        "tamaño corto %u, tamaño largo %u, con VLAN %u\n", diag.erro_res, diag.erro_crc, diag.erro_runt, diag.erro_rwt,
        diag.fragmentado, diag.curto, diag.longo, diag.com_vlan);
    POE("Interrupciones: RX bien %u, TX bien %u, sin descriptores %u, FIFO llena %u, error de sistema %u, "
        "cambio de enlace %u, error RX %u\n", diag.irq_rx_ok, diag.irq_tx_ok,
        diag.sem_descritor, diag.fifo_cheia, diag.erro_sistema, diag.enlace, diag.irq_erro_rx);
    /* D124: cómo está programada la tarjeta, para comparar con lo que espera cada versión. */
    POE("Config: RMS %u, CPCMD %04x, RxDesc %08x:%08x (anillo %08x), TxDesc %08x:%08x (anillo %08x)\n",
        le16(RXMAX), le16(CPCMD), le32(RXDESC + 4), le32(RXDESC), FIS_RXD, le32(TXDESC + 4), le32(TXDESC),
        FIS_TXD);
    POE("Memoria de DMA: %s\n", sem_cache ? "alias sin caché (PCD|PWT)" : "con caché (mapa 1:1)");
    POE("Config: MaxTx %02x, TXCFG %08x, Config0-5 %02x %02x %02x %02x %02x %02x, descriptor 16 B, búfer %u B\n",
        le8(MAXTX), le32(TXCFG), le8(CFG0), le8(CFG0 + 1), le8(CFG0 + 2), le8(CFG0 + 3), le8(CFG0 + 4), le8(CFG0 + 5),
        aj.buf);
    uint8_t c = cap_pcie(&pci_placa);
    if (c) {
        uint16_t dc = (uint16_t)pci_le32(&pci_placa, (uint8_t)(c + 8));
        uint16_t lc = (uint16_t)pci_le32(&pci_placa, (uint8_t)(c + 0x10));
        POE("PCIe: DevCtl %04x (carga máx %u B, lectura máx %u B, orden relajado %s, sin snoop %s), "
            "LnkCtl %04x (ASPM %u, CLKREQ %s), comando PCI %04x, DevCtl de la BIOS %04x\n", dc, 128u << ((dc >> 5) & 7),
            128u << ((dc >> 12) & 7), (dc & (1u << 4)) ? "sí" : "no", (dc & (1u << 11)) ? "sí" : "no", lc, lc & 3,
            (lc & (1u << 8)) ? "sí" : "no", (uint16_t)pci_le32(&pci_placa, 0x04), devctl_bios);
    }
    POE("Tramas sin datos (con la marca a5 intacta): %u. Ajustes: RxConfig %08x, RMS %u, búfer %u (paso %u)\n",
        diag.sem_escrever, aj.rxcfg, aj.rms, aj.buf, paso);
    /* D124: datos que no salieron por su descriptor: lo leído se vuelve a
     * marcar, así que todo lo que no sea a5 en los búferes está "huérfano". */
    POE("Búferes con datos sin leer (índice:primer byte-último):");
    int huerfanos = 0;
    for (int i = 0; i < N_RX; i++) {
        const volatile uint32_t *w = (const volatile uint32_t *)(rx_buf + i * paso);
        int pri = -1, ult = -1;
        for (uint32_t k = 0; k < paso / 4; k++)
            if (w[k] != 0xA5A5A5A5u) { if (pri < 0) pri = (int)k * 4; ult = (int)k * 4 + 3; }
        if (pri >= 0) { POE(" %d:%d-%d", i, pri, ult); huerfanos++; }
    }
    POE(huerfanos ? "\n" : " ninguno\n");
    for (uint32_t i = 0; i < n_amostras; i++) {
        POE("Trama %u: descriptor %u, búfer %08x, opts1 %08x opts2 %08x dir %08x:%08x\n", i + 1, amostras[i].indice,
            amostras[i].fisico, amostras[i].opts1, amostras[i].opts2, amostras[i].end_alto, amostras[i].end_baixo);
        for (int l = 0; l < (i == 0 ? AMOSTRA_BYTES : 64); l += 32) {
            POE("  %02x:", l);
            for (int j = 0; j < 32; j++) POE(j % 4 ? "%02x" : " %02x", amostras[i].dados[l + j]);
            POE("\n");
        }
    }
#ifndef SAGUI_R8169_CLASICO
    if (ler_contadores_chip())
        POE("Contadores del chip: RX bien %u (unicast %u, difusión %u, multicast %u), errores %u, perdidos %u\n",
            cont32(R8169_TALLY_RX_PACKETS), cont32(R8169_TALLY_RX_UNICAST), cont32(R8169_TALLY_RX_BROADCAST),
            cont32(R8169_TALLY_RX_MULTICAST), cont32(R8169_TALLY_RX_ERRORS), cont16(R8169_TALLY_RX_MISSED));
    else
        POE("Contadores del chip: no responde\n");
#else
    POE("Contadores del chip: no disponible (compilación sin GPL)\n");
#endif
#undef POE
}

static void identificar(void)
{
    versao_mac = r8169_versao_do_txconfig(le32(TXCFG), &xid);
    nome_chip = r8169_nome_da_versao(versao_mac);
}

#ifndef SAGUI_R8169_CLASICO
/* Desliga o ASPM (L0s/L1) no "Link Control" da capacidade PCI Express da
 * placa: o Linux faz isso nas Realtek quando não controla o ASPM, porque com
 * ele ligado várias revisões perdem ou travam a recepção. */
static bool desligar_aspm(const pci_disp_t *d)
{
    return lnkctl_apagar(d, 3) != 0;
}
#endif

/* Puntero de la CPU para memoria de DMA: la física (mapa 1:1) o, con
 * sem_cache, un alias sin caché (PCD|PWT, tipo UC con la PAT del SaguiOS).
 * El WBINVD vacía antes lo que la caché tenía de esas páginas, y después la
 * CPU solo las toca por el alias. */
static uint8_t *dma_para_cpu(uint32_t fis, uint32_t bytes)
{
    if (!sem_cache) return (uint8_t *)fis;
    __asm__ volatile("wbinvd" ::: "memory");
    uint32_t v = janela_mapear_tipo(fis, bytes, PG_PCD | PG_PWT);
    if (!v) { sem_cache = false; return (uint8_t *)fis; }
    return (uint8_t *)v;
}

static void programar(placa_t *p);
static int32_t ajustar(const ajuste_driver_t *a);

static const struct { uint16_t modelo; const char *nome; } modelos[] = {
    { 0x8136, "Realtek RTL8101E/8102E/8105E" },
    { 0x8168, "Realtek RTL8111/8168" },
    { 0x8169, "Realtek RTL8169" },
};

bool r8169_procurar(placa_t *p, bool ativar)
{
    pci_disp_t achados[4];
    int n = pci_procurar(0x02, 0x00, 0x00, achados, 4);
    pci_disp_t *d = NULL;
    const char *desc = NULL;
    for (int i = 0; i < n && !d; i++)
        for (uint32_t k = 0; k < sizeof modelos / sizeof modelos[0]; k++)
            if (achados[i].fabricante == 0x10EC && achados[i].modelo == modelos[k].modelo) {
                d = &achados[i];
                desc = modelos[k].nome;
                break;
            }
    if (!d) return false;
    p->nome = r8169_nome_driver(d->modelo);
    p->descricao = desc;
    p->pci = *d;
    p->experimental = true;   /* até identificar a revisão, abaixo */
    p->irq = (int)(pci_le32(d, 0x3C) & 0xFF);
    uint32_t bar = pci_le32(d, 0x10);
    if (!(bar & 1)) return false;   /* esperava portas no BAR0 */
    io = (uint16_t)(bar & ~3u);
    pci_placa = *d;
    for (int i = 0; i < 6; i++) p->mac[i] = le8((uint16_t)(IDR0 + i));
    identificar();
    if (versao_mac) {
        snformatar(descricao, sizeof descricao, "Realtek %s", nome_chip);
        p->descricao = descricao;
    }
#ifdef SAGUI_R8169_CLASICO
    perfil = PERFIL_LEGADO;   /* sin la tabla de Linux no se conoce la revisión: solo el inicio clásico */
#else
    perfil = r8169_perfil_da_versao(versao_mac);
#endif
    p->experimental = !versao_confirmada(versao_mac);
    if (!ativar) return true;       /* só detectar */

    pci_escreve32(d, 0x04, (pci_le32(d, 0x04) | 0x5) & ~(1u << 10));
    paso = perfil == PERFIL_LEGADO ? BUF : 16384;
    fis_desc = memfisica_alocar();
    fis_rx = memfisica_alocar_seguidas(N_RX * paso / PAGINA);
    fis_tx = memfisica_alocar_seguidas(N_TX * BUF / PAGINA);
    if (!fis_desc || !fis_rx || !fis_tx) { klog("[RTL810X] sem memoria\n"); return false; }
    memset((void *)fis_desc, 0, PAGINA);
    memset((void *)fis_rx, MARCA_RX, N_RX * paso);   /* todo marcado: ver el diagnóstico */
    /* D124: fuera del inicio clásico (el probado en el HP dv7), la CPU usa
     * los anillos y los búferes por un alias sin caché, como el HDA
     * (audio_dma_alocar): así lo que se lee es lo que hay en la memoria
     * aunque la tarjeta escribiera sin "snoop", y la marca del diagnóstico
     * dice la verdad. */
    sem_cache = perfil != PERFIL_LEGADO;
    {
        uint8_t c = cap_pcie(d);
        devctl_bios = c ? (uint16_t)pci_le32(d, (uint8_t)(c + 8)) : 0;
    }
    /* D124: y, por lo mismo, sin "relaxed ordering" (bit 4: la escritura del
     * descriptor podría adelantarse a la de los datos) ni "no snoop" (bit 11). */
    if (sem_cache) devctl_apagar(d, (1u << 4) | (1u << 11));
    uint8_t *cpu_desc = dma_para_cpu(fis_desc, PAGINA);
    rxd = (volatile uint32_t *)cpu_desc;
    txd = (volatile uint32_t *)(cpu_desc + 2048);   /* alinhados em 256 */
    contadores_chip = (volatile uint8_t *)(cpu_desc + 3072);   /* alinhado em 64 */
    rx_buf = dma_para_cpu(fis_rx, N_RX * paso);
    memset(&diag, 0, sizeof diag);
    n_amostras = 0;
    tx_buf = dma_para_cpu(fis_tx, N_TX * BUF);

    /* D124: ajustes iniciales. Clásico: los de siempre. 8168E: el RxConfig
     * de Linux SIN RX_MULTI_EN (bit 14): con él, en la 8168evl del Toshiba
     * solo la primera trama traía datos y las demás llegaban con la marca
     * intacta. Moderno: el de Linux. */
    aj.rms = BUF;
    aj.buf = BUF;
    aj.rxcfg = 0x0000E70E;
#ifndef SAGUI_R8169_CLASICO
    if (perfil != PERFIL_LEGADO) aj.rxcfg = rtl_init_rxcfg() | R8169_ACEITAR;
    if (perfil == PERFIL_8168E) aj.rxcfg &= ~R8169_MULTI;
#endif
    placa_ligada = p;
    programar(p);
    p->enviar = enviar;
    p->receber = receber;
    p->cabo = cabo;
    p->atender = atender;
    p->diagnostico = diagnostico;
    p->ajustar = ajustar;
    return true;
}

/* Reset y programación completa de la tarjeta con los anillos vacíos (al
 * activar y con "ifconfig eth0 r8169 reiniciar-rx"). */
static void programar(placa_t *p)
{
    const pci_disp_t *d = &pci_placa;
    bool aspm = false, oob = false, porta_rx = false;
    uint32_t misc_antes = 0;
#ifndef SAGUI_R8169_CLASICO
    if (perfil == PERFIL_MODERNO) {
        aspm = desligar_aspm(d);
        misc_antes = le32(MISC);
        oob = le8(MCU) & NOW_IS_OOB;
        porta_rx = misc_antes & RXDV_GATED_EN;
        achou_oob = oob;
        achou_rx_bloqueada = porta_rx;
        aspm_desligado = aspm;
        rtl_hw_init_8168g();               /* sai do modo "fora de banda" */
    } else if (perfil == PERFIL_8168E) {
        aspm = desligar_aspm(d);           /* el Linux apaga al menos el L1 */
        aspm_desligado = aspm;
        sagui_pcie_desliga_clkreq();
    }
#else
    (void)aspm; (void)oob; (void)porta_rx; (void)misc_antes;
#endif
    escreve8(CMD, 0x10);   /* reset */
    for (int i = 0; i < 100000 && (le8(CMD) & 0x10); i++) porta_pausa();
#ifndef SAGUI_R8169_CLASICO
    if (perfil == PERFIL_8168E) rtl_rar_set(p->mac);   /* la 34 copia la MAC también al ERI */
    if (perfil == PERFIL_MODERNO) {
        uint8_t m[6];
        if (rtl_read_mac_address(m) && !(m[0] & 1) && (m[0] | m[1] | m[2] | m[3] | m[4] | m[5])) {
            if (memcmp(m, p->mac, 6) != 0)
                klog("[R8169] MAC do eFuse %02x:%02x:%02x:%02x:%02x:%02x diferente da do IDR: usando a do eFuse\n",
                     m[0], m[1], m[2], m[3], m[4], m[5]);
            memcpy(p->mac, m, 6);
        }
        rtl_rar_set(p->mac);
    }
#endif
    memset(rx_buf, MARCA_RX, N_RX * paso);
    for (int i = 0; i < N_TX * 4; i++) txd[i] = 0;
    for (int i = 0; i < N_RX; i++) {
        rxd[i * 4 + 1] = 0;
        rxd[i * 4 + 2] = fis_rx + i * paso;
        rxd[i * 4 + 3] = 0;
        rxd[i * 4 + 0] = OWN | (i == N_RX - 1 ? EOR : 0) | aj.buf;
    }
    rx_prox = tx_prox = 0;
    n_amostras = 0;
    diag.sem_escrever = 0;
    diag.voltas = 0;
    escreve8(CFG9346, 0xC0);              /* destrava os registradores de configuração */
#ifndef SAGUI_R8169_CLASICO
    if (perfil == PERFIL_MODERNO || perfil == PERFIL_8168E) rtl_hw_aspm_clkreq_disable();
    if (perfil == PERFIL_8168E) {
        /* D124: rtl_hw_start() de Linux para las 8168e/8168evl, en su orden:
         * CPlusCmd, rtl_hw_start_8168 (MaxTx, EPHY/ERI, sin agrupar
         * interrupciones), salida de L1, RxMaxSize, anillos (alta antes que
         * baja), jumbo apagado. Los RxConfig/TxConfig van más abajo. */
        escreve16(CPCMD, rtl_cp_cmd(le16(CPCMD)));
        rtl_hw_start_8168e();
        rtl_enable_exit_l1();
        escreve16(RXMAX, (uint16_t)aj.rms);
        escreve32(TXDESC + 4, 0);
        escreve32(TXDESC, FIS_TXD);
        escreve32(RXDESC + 4, 0);
        escreve32(RXDESC, FIS_RXD);
        rtl_jumbo_config_8168e();
        le8(CMD);                         /* rtl_pci_commit */
    } else
#endif
    escreve16(CPCMD, 0);                  /* sem RxChkSum nem RxVlan: o descritor fica no formato simples */
#ifndef SAGUI_R8169_CLASICO
    if (perfil == PERFIL_MODERNO) {
        escreve8(MAXTX, EarlySize);       /* rtl_hw_start_8168, 8168evl em diante */
        if (versao_mac >= 46) rtl_hw_start_8168h_1();
        else rtl_hw_start_8168g();
        escreve16(IntrMitigate, 0);       /* sem agrupar interrupções */
    }
#endif
    if (perfil != PERFIL_8168E) {
        escreve16(RXMAX, (uint16_t)aj.rms);
        if (perfil == PERFIL_LEGADO) escreve8(MAXTX, 0x3B);
        escreve32(TXDESC, FIS_TXD);
        escreve32(TXDESC + 4, 0);
        escreve32(RXDESC, FIS_RXD);
        escreve32(RXDESC + 4, 0);
    }
    escreve8(CMD, 0x0C);                  /* RE | TE */
#ifndef SAGUI_R8169_CLASICO
    if (perfil == PERFIL_MODERNO) {
        escreve32(TXCFG, 0x03000700 | TXCFG_AUTO_FIFO);
    } else if (perfil == PERFIL_8168E) {
        escreve32(TXCFG, rtl_tx_config());
    } else
#endif
    {
        escreve32(TXCFG, 0x03000700);     /* IFG padrão, DMA sem limite */
    }
    escreve32(RXCFG, aj.rxcfg);           /* clásico 0xE70E; 8168E sin MULTI; moderno el de Linux (D124) */
    escreve32(MAR0, 0xFFFFFFFFu);
    escreve32(MAR0 + 4, 0xFFFFFFFFu);
    escreve8(CFG9346, 0x00);
    escreve16(ISR, 0xFFFF);
    escreve16(IMR, 0x0001 | 0x0002 | 0x0004 | 0x0008 | 0x0010 | 0x0020);

    klog("[R8169] chip %s, XID %03x, versao MAC %d, perfil %s, TxConfig %08x, RxConfig %08x\n", nome_chip, xid, versao_mac,
         PERFIL_NOME_LOG, le32(TXCFG), le32(RXCFG));
#ifndef SAGUI_R8169_CLASICO
    if (perfil == PERFIL_MODERNO)
        klog("[R8169] antes: MISC %08x (recepcao %s), %s; ASPM %s; depois: MISC %08x MCU %02x\n", misc_antes,
             porta_rx ? "BLOQUEADA" : "livre", oob ? "modo fora de banda" : "sem modo fora de banda",
             aspm ? "desligado agora" : "ja estava desligado", le32(MISC), le8(MCU));
    if (perfil == PERFIL_8168E)
        klog("[R8169] 8168e: CPCMD %04x RMS %u MaxTx %02x Config1-5 %02x %02x %02x %02x %02x MISC %08x ASPM %s\n",
             le16(CPCMD), le16(RXMAX), le8(MAXTX), le8(CFG0 + 1), le8(CFG0 + 2), le8(CFG0 + 3), le8(CFG0 + 4),
             le8(CFG0 + 5), le32(MISC), aspm ? "desligado agora" : "ja estava desligado");
#else
    klog("[R8169] compilacion sin GPL: solo inicio clasico, sin tabla de revisiones\n");
#endif
    klog("[RTL810X] %02x:%02x.%x (%04x:%04x) portas %x, irq %d, MAC %02x:%02x:%02x:%02x:%02x:%02x (experimental)\n",
         d->barramento, d->dispositivo, d->funcao, d->fabricante, d->modelo, io, p->irq,
         p->mac[0], p->mac[1], p->mac[2], p->mac[3], p->mac[4], p->mac[5]);
}

/* D124: "ifconfig eth0 r8169 ...". Sin reiniciar, RxConfig y RxMaxSize se
 * escriben en el momento; el tamaño del búfer (y "reiniciar-rx") reprograma
 * la tarjeta entera con los anillos vacíos. */
static int32_t ajustar(const ajuste_driver_t *a)
{
    if (r8169_aplicar_ajuste(&aj, a, paso) != 0) return ERRO_INVALIDO;
    if (a->mascara & (AJUSTE_REINICIAR | AJUSTE_BUF)) {
        programar(placa_ligada);
        return 0;
    }
    escreve16(RXMAX, (uint16_t)aj.rms);
    escreve32(RXCFG, aj.rxcfg);
    return 0;
}
