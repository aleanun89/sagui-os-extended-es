// SPDX-License-Identifier: GPL-2.0-or-later
/* ar9485.c - Wi-Fi integrada Atheros AR9485 (PCIe 168c:0032, 802.11b/g/n 1x1, 2,4 GHz), D280.
 *
 * Driver propio, escrito leyendo el athn(4) de OpenBSD (ISC): athn.c, ar9003.c y
 * ar9380.c (el orden del encendido, del reinicio, de la carga de las tablas, de la
 * lectura de la ROM y de los descriptores). Los registros y las tablas de inicio
 * del AR9485 vienen de sus ar9003reg.h, athnreg.h y ar9380reg.h, en
 * kernel/terceros/bsd/athn/ (ver docs/terceros.md). No se usó nada del ath9k de Linux.
 *
 * Qué hace, de arriba abajo:
 *   - PCI: ventana de registros (BAR0, 128 KB), bus maestro, INTx desactivado (no se
 *     usan interrupciones: la tarea «wifi» sondea los anillos), ASPM fuera;
 *   - encendido: reinicio del RTC, despertar, SerDes, lectura de la EEPROM/OTP (bloques
 *     con suma de control sobre una plantilla; la MAC y toda la calibración salen de ahí);
 *   - cada canal es un reinicio completo (athn_switch_chan hace lo mismo): tablas de
 *     inicio y de ganancias, sintetizador, potencia por tasa, calibración del AGC y del
 *     ruido de fondo;
 *   - anillos DMA: recepción (colas LP y HP, un descriptor de estado de 48 bytes delante
 *     de cada trama), transmisión (descriptores de 128 bytes con el anillo de estados);
 *   - tramas: la 802.11 la arma wifi.c; aquí solo se le pone el descriptor, la serie de
 *     tasas (con reintentos y bajadas por hardware) y se confirma el envío;
 *   - cifrado: por defecto el CCMP lo hace wifi.c por software (athn desactiva el de
 *     transmisión); con ar9485_cifra_hw(true) se usa la caché de claves del chip.
 *
 * No hace (aún): 5 GHz (el AR9485 es solo de 2,4), 802.11n (se asocia como b/g), ahorro
 * de energía, ANI, PA predistortion, calibración IQ de transmisión, coexistencia con el
 * Bluetooth del combo. La calibración del ruido y la IQ de recepción sí van. */
#include "ar9485.h"
#include "console.h"
#include "info_hardware.h"
#include "memoria.h"
#include "paginacao.h"
#include "pci.h"
#include "portas.h"
#include "relogio.h"
#include "texto.h"
#include "wifi_n.h"
#include "ar9485_logica.h"

/* Lo que el código de OpenBSD da por supuesto. */
#define __packed __attribute__((packed))
#define nitems(a) (sizeof(a) / sizeof((a)[0]))
#define AR_READ(sc, reg)       ar_le(reg)
#define AR_WRITE(sc, reg, val) ar_esc((reg), (val))
#define AR_WRITE_BARRIER(sc)   __asm__ volatile("" ::: "memory")
#include "../terceros/bsd/athn/athnreg.h"
#include "../terceros/bsd/athn/ar9003reg.h"
#include "../terceros/bsd/athn/ar9485_datos.h"


/* De athnvar.h: índices de potencia por tasa y tamaño de la caché de claves. */
#define ATHN_POWER_OFDM6	0
#define ATHN_POWER_OFDM9	1
#define ATHN_POWER_OFDM12	2
#define ATHN_POWER_OFDM18	3
#define ATHN_POWER_OFDM24	4
#define ATHN_POWER_OFDM36	5
#define ATHN_POWER_OFDM48	6
#define ATHN_POWER_OFDM54	7
#define ATHN_POWER_CCK1_LP	8
#define ATHN_POWER_CCK2_LP	9
#define ATHN_POWER_CCK2_SP	10
#define ATHN_POWER_CCK55_LP	11
#define ATHN_POWER_CCK55_SP	12
#define ATHN_POWER_CCK11_LP	13
#define ATHN_POWER_CCK11_SP	14
#define ATHN_POWER_HT20(mcs)	(16 + (mcs))
#define ATHN_POWER_HT40(mcs)	(40 + (mcs))
#define ATHN_POWER_COUNT	68

/* Las macros AR_SREV_* de OpenBSD miran «sc->mac_ver»: aquí hay un solo chip. */
static struct { uint32_t mac_ver, mac_rev; } ar_sc_ = { AR_SREV_VERSION_9485, 0 };
#define sc (&ar_sc_)

#define A 0   /* el «sc» de AR_READ/AR_WRITE: no se usa, solo hay una tarjeta */
#define DELAY(us) ar_esperar_us(us)

#define REGS_BYTES   0x20000
#define N_TX         16                 /* descriptores y búferes de transmisión */
#define N_RX_LP      64
#define N_RX_HP      8
#define BUF_RX       3872               /* ATHN_RXBUFSZ: el descriptor de estado va dentro */
#define BUF_TX       4096
#define TXS_N        AR9003_NTXSTATUS
#define QID_BE       0                  /* ATHN_QID_AC_BE: todo va por la cola de «mejor esfuerzo» */
#define QID_N        8
#define PCI_AR9485   0x0032
#define CAL_IQ       1

_Static_assert(sizeof(struct ar9380_eeprom) == sizeof(ar9380_def_rom), "la plantilla de ROM no mide lo que la estructura");
_Static_assert(sizeof(struct ar_tx_desc) == 128, "descriptor de transmisión de 128 bytes");
_Static_assert(sizeof(struct ar_rx_status) == 48, "descriptor de estado de recepción de 48 bytes");

static ar9485_t inf;

/* ------------------------------------------------------------ estado */

static pci_disp_t pdev;
static bool       achada;
static volatile uint8_t *regs;
static uint32_t   bar_fis;

static bool     eep_otp;
static int      eep_size, eep_base;
static uint8_t  eep_img[sizeof(struct ar9380_eeprom)] __attribute__((aligned(4)));
static uint8_t  txchainmask, rxchainmask;
static int      ntx, nrx;
static bool     rfsilent;
static uint8_t  rfsilent_pin;
static bool     rfsilent_invertido;
static uint16_t cca_min, cca_max;

static uint8_t  canal_atual;
static uint16_t freq_atual;
static bool     modo_busca;
static bool     slot_curto;
static bool     preambulo_curto;
static bool     cifra_hw;
static bool     claves_hw;       /* hay claves en la caché: el chip descifra */

/* DMA */
static uint32_t txs_fis;                 /* anillo de estados de transmisión */
static struct ar_tx_status *txs;
static uint32_t txs_cur;
static uint16_t tx_seq;                  /* número de secuencia 802.11 de lo que sale (la numeración del chip va apagada) */
static uint32_t txd_fis;                 /* descriptores de transmisión */
static struct ar_tx_desc *txd;
static uint32_t txb_fis[N_TX];           /* búferes de las tramas */
static bool     txb_livre[N_TX];
static uint8_t  txfila[N_TX];            /* orden de salida de los búferes en el hardware */
static int      tx_ini, tx_qtd;
static uint32_t rxb_fis[N_RX_LP + N_RX_HP];
static int      rx_ini[2];
static const int rx_cont[2] = { N_RX_LP, N_RX_HP };
static bool     dma_pronto;

/* control de tasa y calibración */
static uint32_t cal_nf_em, cal_iq_em;
static bool     nf_pendente;
static int      nf_hist_n, nf_hist_cur;
static int16_t  nf_hist[5][2];           /* [muestra][0 = canal, 1 = extensión] (una cadena) */
static int16_t  nf_priv, nf_ext_priv;
static bool     iq_ativo;
static uint32_t periodico_em;

/* ------------------------------------------------------------ acceso */

static inline uint32_t ar_le(uint32_t r) { return *(volatile uint32_t *)(regs + r); }
static inline void ar_esc(uint32_t r, uint32_t v) { *(volatile uint32_t *)(regs + r) = v; }

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (uint64_t)hi << 32 | lo;
}

static void ar_esperar_us(uint32_t us)
{
    uint32_t mhz = (hw_tsc_khz() + 999) / 1000;
    if (!mhz) { while (us--) porta_pausa(); return; }
    uint64_t fim = tsc() + (uint64_t)mhz * us;
    while (tsc() < fim) __asm__ volatile("pause");
}

static uint32_t agora(void) { return relogio_tiques(); }

/* ------------------------------------------------------------ PCI */

static uint8_t cap_procurar(const pci_disp_t *d, uint8_t id)
{
    if (!(pci_le32(d, 0x04) & (1u << 20))) return 0;
    uint8_t c = (uint8_t)(pci_le32(d, 0x34) & 0xFC);
    for (int i = 0; c && i < 48; i++) {
        uint32_t cab = pci_le32(d, c);
        if ((cab & 0xFF) == id) return c;
        c = (uint8_t)((cab >> 8) & 0xFC);
    }
    return 0;
}

bool ar9485_procurar(void)
{
    if (achada) return true;
    pci_disp_t v[64];
    int n = pci_listar(v, 64);
    for (int i = 0; i < n; i++)
        if (v[i].fabricante == 0x168c && v[i].modelo == PCI_AR9485) {
            pdev = v[i];
            achada = true;
            inf.presente = true;
            klog("[AR9485] tarjeta Atheros AR9485 (168c:%04x) en %02x:%02x.%x\n", pdev.modelo, pdev.barramento,
                 pdev.dispositivo, pdev.funcao);
            return true;
        }
    return false;
}

static int pci_ligar(const char **motivo)
{
    uint8_t pm = cap_procurar(&pdev, 0x01);
    if (pm) {
        uint32_t x = pci_le32(&pdev, (uint8_t)(pm + 4));
        if (x & 3) {
            pci_escreve32(&pdev, (uint8_t)(pm + 4), x & ~0x8003u);   /* a D0, sin tocar el PME_Status */
            ar_esperar_us(20000);
        }
    }
    /* «PCI retry timeout» (0x41): fuera, para que los reintentos no molesten a los estados C3. */
    uint32_t r40 = pci_le32(&pdev, 0x40);
    if (r40 & 0xff00) pci_escreve32(&pdev, 0x40, r40 & ~0xff00u);
    /* Línea de caché y temporizador de latencia, como el if_athn_pci.c de OpenBSD. */
    uint32_t bhlc = pci_le32(&pdev, 0x0C);
    if (((bhlc & 0xff)) == 0) bhlc |= 8;
    bhlc = (bhlc & ~0xff00u) | (168u << 8);
    pci_escreve32(&pdev, 0x0C, bhlc);
    /* Memoria y bus maestro; INTx desactivado (bit 10): no hay manejador. */
    uint32_t cmd = pci_le32(&pdev, 0x04) & 0xFFFF;
    pci_escreve32(&pdev, 0x04, cmd | 0x6 | (1u << 10));
    /* Sin ASPM (L0s y L1): el chip se queda sin respuesta si la tarjeta se duerme. */
    uint8_t ex = cap_procurar(&pdev, 0x10);
    if (ex) {
        uint32_t l = pci_le32(&pdev, (uint8_t)(ex + 0x10));
        pci_escreve32(&pdev, (uint8_t)(ex + 0x10), l & 0x0000FFFCu);
    }
    uint32_t bar = pci_le32(&pdev, 0x10);
    if (bar & 1) { *motivo = "la BAR0 no es de memoria"; return -1; }
    if ((bar & 6) == 4 && pci_le32(&pdev, 0x14) != 0) { *motivo = "la BAR0 está por encima de 4 GB"; return -1; }
    bar_fis = bar & ~0xFu;
    if (!bar_fis) { *motivo = "la BIOS no asignó la BAR0"; return -1; }
    if (!regs) regs = (volatile uint8_t *)janela_mapear_tipo(bar_fis, REGS_BYTES, PG_PCD | PG_PWT);
    if (!regs) { *motivo = "no se pudo mapear la ventana de registros"; return -1; }
    return 0;
}

/* ------------------------------------------------------------ energía y reinicio (athn.c) */

static int reset_power_on_ar(void);

static int athn_reset(int cold)
{
    int ntries;
    AR_WRITE(A, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN | AR_RTC_FORCE_WAKE_ON_INT);
    if (AR_READ(A, AR_INTR_SYNC_CAUSE) & (AR_INTR_SYNC_LOCAL_TIMEOUT | AR_INTR_SYNC_RADM_CPL_TIMEOUT)) {
        AR_WRITE(A, AR_INTR_SYNC_ENABLE, 0);
        AR_WRITE(A, AR_RC, AR_RC_HOSTIF);
    }
    AR_WRITE(A, AR_RTC_RC, AR_RTC_RC_MAC_WARM | (cold ? AR_RTC_RC_MAC_COLD : 0));
    AR_WRITE_BARRIER(A);
    DELAY(50);
    AR_WRITE(A, AR_RTC_RC, 0);
    for (ntries = 0; ntries < 1000; ntries++) {
        if (!(AR_READ(A, AR_RTC_RC) & (AR_RTC_RC_MAC_WARM | AR_RTC_RC_MAC_COLD))) break;
        DELAY(10);
    }
    if (ntries == 1000) return -1;
    AR_WRITE(A, AR_RC, 0);
    AR_WRITE_BARRIER(A);
    return 0;
}

static int reset_power_on_ar(void)
{
    int ntries;
    AR_WRITE(A, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN | AR_RTC_FORCE_WAKE_ON_INT);
    AR_WRITE(A, AR_RTC_RESET, 0);
    AR_WRITE_BARRIER(A);
    DELAY(2);
    AR_WRITE(A, AR_RTC_RESET, 1);
    for (ntries = 0; ntries < 1000; ntries++) {
        if ((AR_READ(A, AR_RTC_STATUS) & AR_RTC_STATUS_M) == AR_RTC_STATUS_ON) break;
        DELAY(10);
    }
    if (ntries == 1000) return -1;
    return athn_reset(0);
}

static int set_power_awake(void)
{
    int ntries;
    if ((AR_READ(A, AR_RTC_STATUS) & AR_RTC_STATUS_M) == AR_RTC_STATUS_SHUTDOWN)
        if (reset_power_on_ar() != 0) return -1;
    AR_SETBITS(A, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN);
    AR_WRITE_BARRIER(A);
    DELAY(50);
    for (ntries = 0; ntries < 4000; ntries++) {
        if ((AR_READ(A, AR_RTC_STATUS) & AR_RTC_STATUS_M) == AR_RTC_STATUS_ON) break;
        DELAY(50);
        AR_SETBITS(A, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN);
    }
    if (ntries == 4000) return -1;
    AR_CLRBITS(A, AR_STA_ID1, AR_STA_ID1_PWR_SAV);
    AR_WRITE_BARRIER(A);
    return 0;
}

static void set_power_sleep(void)
{
    AR_SETBITS(A, AR_STA_ID1, AR_STA_ID1_PWR_SAV);
    AR_CLRBITS(A, AR_RTC_FORCE_WAKE, AR_RTC_FORCE_WAKE_EN);
    AR_CLRBITS(A, AR_RTC_RESET, AR_RTC_RESET_EN);
    AR_WRITE_BARRIER(A);
}

static void init_pll(void)
{
    AR_WRITE(A, AR_RTC_PLL_CONTROL2, 0x886666);   /* AR9485 */
    uint32_t pll = SM(AR_RTC_9160_PLL_REFDIV, 0x5) | SM(AR_RTC_9160_PLL_DIV, 0x2c);
    AR_WRITE(A, AR_RTC_PLL_CONTROL, pll);
    AR_WRITE_BARRIER(A);
    DELAY(100);
    AR_WRITE(A, AR_RTC_SLEEP_CLK, AR_RTC_FORCE_DERIVED_CLK);
    AR_WRITE_BARRIER(A);
}

static void config_pcie(void)
{
    /* SerDes: sin PLL en L0s ni reloj del receptor en L1. */
    for (int i = 0; i < ar9485_1_1_serdes.nvals; i++) AR_WRITE(A, ar9485_1_1_serdes.regs[i], ar9485_1_1_serdes.vals[i]);
    AR_WRITE_BARRIER(A);
    DELAY(1000);
    AR_SETBITS(A, AR_PCIE_PM_CTRL, AR_PCIE_PM_CTRL_ENA);
    AR_WRITE(A, AR_WA, 0);   /* como athn: sc->workaround no se asigna para la familia AR9003 */
    AR_WRITE_BARRIER(A);
}

/* ------------------------------------------------------------ ROM (ar9003.c) */

/* Lectura de 16 bits de la EEPROM: el registro de datos trae los dos bytes. */
static int eep_word(uint32_t addr, uint16_t *val)
{
    (void)AR_READ(A, AR_EEPROM_OFFSET(addr));
    for (int n = 0; n < 1000; n++) {
        uint32_t reg = AR_READ(A, AR_EEPROM_STATUS_DATA);
        if (!(reg & (AR_EEPROM_STATUS_DATA_BUSY | AR_EEPROM_STATUS_DATA_PROT_ACCESS))) {
            *val = (uint16_t)MS(reg, AR_EEPROM_STATUS_DATA_VAL);
            return 0;
        }
        DELAY(10);
    }
    *val = 0xffff;
    return -1;
}

/* ar9003_read_eep_data: lee «len» bytes desde «addr», que baja de dos en dos bytes. */
static int eep_dados(uint32_t addr, void *buf, int len)
{
    uint8_t *dst = buf;
    uint16_t val;
    if (len > 0 && (addr & 1)) {
        addr >>= 1;
        if (eep_word(addr, &val) != 0) return -1;
        *dst++ = val & 0xff;
        addr--;
        len--;
    } else addr >>= 1;
    for (; len >= 2; addr--, len -= 2) {
        if (eep_word(addr, &val) != 0) return -1;
        *dst++ = (uint8_t)(val >> 8);
        *dst++ = val & 0xff;
    }
    if (len > 0) {
        if (eep_word(addr, &val) != 0) return -1;
        *dst++ = (uint8_t)(val >> 8);
    }
    return 0;
}

static int otp_word(uint32_t addr, uint32_t *val)
{
    (void)AR_READ(A, AR_OTP_BASE(addr));
    for (int n = 0; n < 1000; n++) {
        uint32_t reg = AR_READ(A, AR_OTP_STATUS);
        if (MS(reg, AR_OTP_STATUS_TYPE) == AR_OTP_STATUS_VALID) {
            *val = AR_READ(A, AR_OTP_READ_DATA);
            return 0;
        }
        DELAY(10);
    }
    return -1;
}

static int otp_dados(uint32_t addr, void *buf, int len)
{
    uint8_t *dst = buf;
    uint32_t val;
    for (; len > 0; addr--, len--) {
        if (otp_word(addr >> 2, &val) != 0) return -1;
        *dst++ = (val >> ((addr & 3) * 8)) & 0xff;
    }
    return 0;
}

static int rom_dados(uint32_t addr, void *buf, int len) { return eep_otp ? otp_dados(addr, buf, len) : eep_dados(addr, buf, len); }

static int lector_rom(void *ctx, uint32_t dir, void *buf, int n) { (void)ctx; return rom_dados(dir, buf, n); }

static const uint8_t *plantilla_rom(void *ctx, uint8_t ref)
{
    (void)ctx;
    for (unsigned i = 0; i < nitems(ar9380_rom_templates); i++)
        if (ar9380_rom_templates[i][1] == ref) return ar9380_rom_templates[i];
    return (const uint8_t *)0;
}

static int rom_procurar(void)
{
    uint32_t hdr;
    for (int k = 0; k < 4; k++) {
        eep_otp = k >= 2;
        eep_size = k == 0 ? 4096 : k == 1 ? 512 : k == 2 ? 1024 : 512;
        eep_base = eep_size - 1;
        if (rom_dados((uint32_t)eep_base, &hdr, sizeof hdr) == 0 && hdr != 0 && hdr != 0xffffffff) return 0;
    }
    return -1;
}

static int rom_ler(void)
{
    static uint8_t tmp[2048];
    memset(eep_img, 0, sizeof eep_img);
    int n = ar9485_rom_montar(lector_rom, plantilla_rom, (void *)0, (uint32_t)eep_base, eep_img, (int)sizeof eep_img, tmp);
    return n > 0 ? 0 : -1;
}

/* ------------------------------------------------------------ GPIO del interruptor de radio */

static int gpio_ler(int pin) { return ((AR_READ(A, AR_GPIO_IN) & AR9300_GPIO_IN_VAL) & (1u << pin)) != 0; }

static void gpio_entrada(int pin)
{
    uint32_t reg = AR_READ(A, AR_GPIO_OE_OUT);
    reg &= ~(AR_GPIO_OE_OUT_DRV_M << (pin * 2));
    reg |= AR_GPIO_OE_OUT_DRV_NO << (pin * 2);
    AR_WRITE(A, AR_GPIO_OE_OUT, reg);
}

static void rfsilent_init(void)
{
    AR_SETBITS(A, AR_GPIO_INPUT_EN_VAL, AR_GPIO_INPUT_EN_VAL_RFSILENT_BB);
    uint32_t reg = AR_READ(A, AR_GPIO_INPUT_MUX2);
    reg = RW(reg, AR_GPIO_INPUT_MUX2_RFSILENT, 0);
    AR_WRITE(A, AR_GPIO_INPUT_MUX2, reg);
    gpio_entrada(rfsilent_pin);
    AR_SETBITS(A, AR_PHY_TEST, AR_PHY_TEST_RFSILENT_BB);
    if (!rfsilent_invertido) AR_SETBITS(A, AR_GPIO_INTR_POL, AR_GPIO_INTR_POL_PIN(rfsilent_pin));
    AR_WRITE_BARRIER(A);
}

/* ------------------------------------------------------------ PHY (ar9003.c y ar9380.c) */

static const struct ar9380_eeprom *eep(void) { return (const struct ar9380_eeprom *)eep_img; }

static void set_rf_mode(void) { AR_WRITE(A, AR_PHY_MODE, AR_PHY_MODE_DYNAMIC); AR_WRITE_BARRIER(A); }

static uint32_t synth_delay(void)
{
    uint32_t d = MS(AR_READ(A, AR_PHY_RX_DELAY), AR_PHY_RX_DELAY_DELAY);
    return d / 10;   /* en pasos de 100 ns (modo g) */
}

static int rf_bus_request(void)
{
    AR_WRITE(A, AR_PHY_RFBUS_REQ, AR_PHY_RFBUS_REQ_EN);
    for (int n = 0; n < 10000; n++) {
        if (AR_READ(A, AR_PHY_RFBUS_GRANT) & AR_PHY_RFBUS_GRANT_EN) return 0;
        DELAY(10);
    }
    return -1;
}

static void rf_bus_release(void)
{
    DELAY(AR_BASE_PHY_ACTIVE_DELAY + synth_delay());
    AR_WRITE(A, AR_PHY_RFBUS_REQ, 0);
    AR_WRITE_BARRIER(A);
}

static void set_phy(void)
{
    uint32_t phy = AR_READ(A, AR_PHY_GEN_CTRL);
    phy |= AR_PHY_GC_HT_EN | AR_PHY_GC_SHORT_GI_40 | AR_PHY_GC_SINGLE_HT_LTF1 | AR_PHY_GC_WALSH;
    phy &= ~AR_PHY_GC_GF_DETECT_EN;
    AR_WRITE(A, AR_PHY_GEN_CTRL, phy);
    AR_WRITE(A, AR_2040_MODE, 0);
    AR_WRITE(A, AR_GTXTO, SM(AR_GTXTO_TIMEOUT_LIMIT, 25));
    AR_WRITE(A, AR_CST, SM(AR_CST_TIMEOUT_LIMIT, 15));
    AR_WRITE_BARRIER(A);
}

static void set_delta_slope(void)
{
    uint32_t coeff = (100u << 24) / freq_atual, exp, man;
    ar9485_delta_slope(coeff, &exp, &man);
    uint32_t reg = AR_READ(A, AR_PHY_TIMING3);
    reg = RW(reg, AR_PHY_TIMING3_DSC_EXP, exp);
    reg = RW(reg, AR_PHY_TIMING3_DSC_MAN, man);
    AR_WRITE(A, AR_PHY_TIMING3, reg);
    coeff = (9 * coeff) / 10;
    ar9485_delta_slope(coeff, &exp, &man);
    reg = AR_READ(A, AR_PHY_SGI_DELTA);
    reg = RW(reg, AR_PHY_SGI_DSC_EXP, exp);
    reg = RW(reg, AR_PHY_SGI_DSC_MAN, man);
    AR_WRITE(A, AR_PHY_SGI_DELTA, reg);
    AR_WRITE_BARRIER(A);
}

static int set_synth(void)
{
    uint32_t chansel = (((uint32_t)freq_atual << 16) - 215) / 15;   /* AR9485, 2,4 GHz */
    AR_WRITE(A, AR_PHY_SYNTH_CONTROL, AR9380_BMODE);
    AR_SETBITS(A, AR_PHY_65NM_CH0_SYNTH4, AR_PHY_SYNTH4_LONG_SHIFT_SELECT);
    AR_WRITE_BARRIER(A);
    uint32_t phy = (chansel << 2) | AR9380_FRACMODE;
    AR_WRITE(A, AR_PHY_65NM_CH0_SYNTH7, phy);
    AR_WRITE_BARRIER(A);
    AR_WRITE(A, AR_PHY_65NM_CH0_SYNTH7, phy | AR9380_LOAD_SYNTH);
    AR_WRITE_BARRIER(A);
    return 0;
}

static void init_chains(void)
{
    if (rxchainmask == 0x5 || txchainmask == 0x5) AR_SETBITS(A, AR_PHY_ANALOG_SWAP, AR_PHY_SWAP_ALT_CHAIN);
    AR_WRITE(A, AR_PHY_RX_CHAINMASK, rxchainmask);
    AR_WRITE(A, AR_PHY_CAL_CHAINMASK, rxchainmask);
    AR_WRITE(A, AR_SELFGEN_MASK, txchainmask);
    AR_WRITE_BARRIER(A);
}

static void write_txpower(int16_t power[ATHN_POWER_COUNT])
{
    AR_WRITE(A, AR_PHY_TX_FORCED_GAIN, 0);
    AR_WRITE(A, AR_PHY_PWRTX_RATE1, (power[ATHN_POWER_OFDM18] & 0x3f) << 24 | (power[ATHN_POWER_OFDM12] & 0x3f) << 16 |
                                    (power[ATHN_POWER_OFDM9] & 0x3f) << 8 | (power[ATHN_POWER_OFDM6] & 0x3f));
    AR_WRITE(A, AR_PHY_PWRTX_RATE2, (power[ATHN_POWER_OFDM54] & 0x3f) << 24 | (power[ATHN_POWER_OFDM48] & 0x3f) << 16 |
                                    (power[ATHN_POWER_OFDM36] & 0x3f) << 8 | (power[ATHN_POWER_OFDM24] & 0x3f));
    AR_WRITE(A, AR_PHY_PWRTX_RATE3, (power[ATHN_POWER_CCK2_SP] & 0x3f) << 24 | (power[ATHN_POWER_CCK2_LP] & 0x3f) << 16 |
                                    (power[ATHN_POWER_CCK1_LP] & 0x3f));
    AR_WRITE(A, AR_PHY_PWRTX_RATE4, (power[ATHN_POWER_CCK11_SP] & 0x3f) << 24 | (power[ATHN_POWER_CCK11_LP] & 0x3f) << 16 |
                                    (power[ATHN_POWER_CCK55_SP] & 0x3f) << 8 | (power[ATHN_POWER_CCK55_LP] & 0x3f));
    AR_WRITE(A, AR_PHY_PWRTX_RATE5, (power[ATHN_POWER_HT20(5)] & 0x3f) << 24 | (power[ATHN_POWER_HT20(4)] & 0x3f) << 16 |
                                    (power[ATHN_POWER_HT20(1)] & 0x3f) << 8 | (power[ATHN_POWER_HT20(0)] & 0x3f));
    AR_WRITE(A, AR_PHY_PWRTX_RATE6, (power[ATHN_POWER_HT20(13)] & 0x3f) << 24 | (power[ATHN_POWER_HT20(12)] & 0x3f) << 16 |
                                    (power[ATHN_POWER_HT20(7)] & 0x3f) << 8 | (power[ATHN_POWER_HT20(6)] & 0x3f));
    AR_WRITE(A, AR_PHY_PWRTX_RATE7, (power[ATHN_POWER_HT40(5)] & 0x3f) << 24 | (power[ATHN_POWER_HT40(4)] & 0x3f) << 16 |
                                    (power[ATHN_POWER_HT40(1)] & 0x3f) << 8 | (power[ATHN_POWER_HT40(0)] & 0x3f));
    AR_WRITE(A, AR_PHY_PWRTX_RATE8, (power[ATHN_POWER_HT40(13)] & 0x3f) << 24 | (power[ATHN_POWER_HT40(12)] & 0x3f) << 16 |
                                    (power[ATHN_POWER_HT40(7)] & 0x3f) << 8 | (power[ATHN_POWER_HT40(6)] & 0x3f));
    AR_WRITE(A, AR_PHY_PWRTX_RATE10, (power[ATHN_POWER_HT20(21)] & 0x3f) << 24 | (power[ATHN_POWER_HT20(20)] & 0x3f) << 16 |
                                     (power[ATHN_POWER_HT20(15)] & 0x3f) << 8 | (power[ATHN_POWER_HT20(14)] & 0x3f));
    AR_WRITE(A, AR_PHY_PWRTX_RATE11, (power[ATHN_POWER_HT40(23)] & 0x3f) << 24 | (power[ATHN_POWER_HT40(22)] & 0x3f) << 16 |
                                     (power[ATHN_POWER_HT20(23)] & 0x3f) << 8 | (power[ATHN_POWER_HT20(22)] & 0x3f));
    AR_WRITE(A, AR_PHY_PWRTX_RATE12, (power[ATHN_POWER_HT40(21)] & 0x3f) << 24 | (power[ATHN_POWER_HT40(20)] & 0x3f) << 16 |
                                     (power[ATHN_POWER_HT40(15)] & 0x3f) << 8 | (power[ATHN_POWER_HT40(14)] & 0x3f));
    AR_WRITE_BARRIER(A);
}

/* ar9380_set_correction: la compensación de potencia en lazo abierto y de temperatura. */
static void set_correction(void)
{
    const struct ar9380_eeprom *e = eep();
    const struct ar9380_modal_eep_header *modal = &e->modalHeader2G;
    uint8_t fbin = ar9485_chan2fbin(freq_atual);
    int lo, hi, temp0 = 0;
    ar9485_pier_ival(fbin, e->calFreqPier2G, AR9380_NUM_2G_CAL_PIERS, &lo, &hi);
    for (int i = 0; i < AR9380_MAX_CHAINS; i++) {
        const struct ar9380_cal_data_per_freq_op_loop *pd = e->calPierData2G[i];
        int corr = ar9485_interpolar(fbin, e->calFreqPier2G[lo], pd[lo].refPower, e->calFreqPier2G[hi], pd[hi].refPower);
        int temp = ar9485_interpolar(fbin, e->calFreqPier2G[lo], pd[lo].tempMeas, e->calFreqPier2G[hi], pd[hi].tempMeas);
        if (i == 0) temp0 = temp;
        uint32_t reg = AR_READ(A, AR_PHY_TPC_11_B(i));
        reg = RW(reg, AR_PHY_TPC_11_OLPC_GAIN_DELTA, corr);
        AR_WRITE(A, AR_PHY_TPC_11_B(i), reg);
        reg = AR_READ(A, AR_PHY_TPC_6_B(i));
        reg = RW(reg, AR_PHY_TPC_6_ERROR_EST_MODE, 3);
        AR_WRITE(A, AR_PHY_TPC_6_B(i), reg);
    }
    uint32_t reg = AR_READ(A, AR_PHY_TPC_19);
    reg = RW(reg, AR_PHY_TPC_19_ALPHA_THERM, modal->tempSlope);
    AR_WRITE(A, AR_PHY_TPC_19, reg);
    reg = AR_READ(A, AR_PHY_TPC_18);
    reg = RW(reg, AR_PHY_TPC_18_THERM_CAL, temp0);
    AR_WRITE(A, AR_PHY_TPC_18, reg);
    AR_WRITE_BARRIER(A);
}

static void set_txpower(void)
{
    const struct ar9380_eeprom *e = eep();
    uint8_t fbin = ar9485_chan2fbin(freq_atual);
    uint8_t tcck[4], tofdm[4], tht20[14];
    int16_t power[ATHN_POWER_COUNT];
    ar9485_tpow(fbin, e->calTargetFbinCck, AR9380_NUM_2G_CCK_TARGET_POWERS, e->calTargetPowerCck[0].tPow2x, 4, 4, tcck);
    ar9485_tpow(fbin, e->calTargetFbin2G, AR9380_NUM_2G_20_TARGET_POWERS, e->calTargetPower2G[0].tPow2x, 4, 4, tofdm);
    ar9485_tpow(fbin, e->calTargetFbin2GHT20, AR9380_NUM_2G_20_TARGET_POWERS, e->calTargetPower2GHT20[0].tPow2x, 14, 14,
                tht20);
    memset(power, 0, sizeof power);
    power[ATHN_POWER_OFDM6] = power[ATHN_POWER_OFDM9] = power[ATHN_POWER_OFDM12] = power[ATHN_POWER_OFDM18] =
        power[ATHN_POWER_OFDM24] = tofdm[0];
    power[ATHN_POWER_OFDM36] = tofdm[1];
    power[ATHN_POWER_OFDM48] = tofdm[2];
    power[ATHN_POWER_OFDM54] = tofdm[3];
    power[ATHN_POWER_CCK1_LP] = power[ATHN_POWER_CCK2_LP] = power[ATHN_POWER_CCK2_SP] = power[ATHN_POWER_CCK55_LP] = tcck[0];
    power[ATHN_POWER_CCK55_SP] = tcck[1];
    power[ATHN_POWER_CCK11_LP] = tcck[2];
    power[ATHN_POWER_CCK11_SP] = tcck[3];
    power[ATHN_POWER_HT20(0)] = tht20[0];
    power[ATHN_POWER_HT20(1)] = tht20[1];
    power[ATHN_POWER_HT20(4)] = tht20[2];
    power[ATHN_POWER_HT20(5)] = tht20[3];
    power[ATHN_POWER_HT20(6)] = tht20[4];
    power[ATHN_POWER_HT20(7)] = tht20[5];
    power[ATHN_POWER_HT20(12)] = tht20[6];
    power[ATHN_POWER_HT20(13)] = tht20[7];
    power[ATHN_POWER_HT20(14)] = tht20[8];
    power[ATHN_POWER_HT20(15)] = tht20[9];
    power[ATHN_POWER_HT20(20)] = tht20[10];
    power[ATHN_POWER_HT20(21)] = tht20[11];
    power[ATHN_POWER_HT20(22)] = tht20[12];
    power[ATHN_POWER_HT20(23)] = tht20[13];
    write_txpower(power);
    set_correction();
}

/* ar9380_spur_mitigate_cck: la mitigación de espurias de CCK (se apaga fuera de las cuatro frecuencias). */
static void spur_cck(void)
{
    static const int16_t freqs[] = { 2420, 2440, 2464, 2480 };
    int spur = 0, i;
    for (i = 0; i < 4; i++) {
        spur = freqs[i] - (int)freq_atual;
        if ((spur < 0 ? -spur : spur) < 10) break;
    }
    uint32_t reg;
    if (i == 4) {
        reg = AR_READ(A, AR_PHY_AGC_CONTROL);
        reg = RW(reg, AR_PHY_AGC_CONTROL_YCOK_MAX, 0x5);
        AR_WRITE(A, AR_PHY_AGC_CONTROL, reg);
        reg = AR_READ(A, AR_PHY_CCK_SPUR_MIT);
        reg = RW(reg, AR_PHY_CCK_SPUR_MIT_CCK_SPUR_FREQ, 0);
        reg &= ~AR_PHY_CCK_SPUR_MIT_USE_CCK_SPUR_MIT;
        AR_WRITE(A, AR_PHY_CCK_SPUR_MIT, reg);
        AR_WRITE_BARRIER(A);
        return;
    }
    int freq = (spur * 524288) / 11;
    reg = AR_READ(A, AR_PHY_AGC_CONTROL);
    reg = RW(reg, AR_PHY_AGC_CONTROL_YCOK_MAX, 0x7);
    AR_WRITE(A, AR_PHY_AGC_CONTROL, reg);
    reg = AR_READ(A, AR_PHY_CCK_SPUR_MIT);
    reg = RW(reg, AR_PHY_CCK_SPUR_MIT_CCK_SPUR_FREQ, freq);
    reg = RW(reg, AR_PHY_CCK_SPUR_MIT_SPUR_RSSI_THR, 0x7f);
    reg = RW(reg, AR_PHY_CCK_SPUR_MIT_SPUR_FILTER_TYPE, 0x2);
    reg |= AR_PHY_CCK_SPUR_MIT_USE_CCK_SPUR_MIT;
    AR_WRITE(A, AR_PHY_CCK_SPUR_MIT, reg);
    AR_WRITE_BARRIER(A);
}

/* ar9380_spur_mitigate_ofdm, canal de 20 MHz. */
static void spur_ofdm(void)
{
    const uint8_t *sp = eep()->modalHeader2G.spurChans;
    uint32_t reg;
    if (sp[0] == 0) return;
    AR_CLRBITS(A, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_FILTER);
    reg = AR_READ(A, AR_PHY_TIMING11);
    reg = RW(reg, AR_PHY_TIMING11_SPUR_FREQ_SD, 0);
    reg = RW(reg, AR_PHY_TIMING11_SPUR_DELTA_PHASE, 0);
    reg &= ~AR_PHY_TIMING11_USE_SPUR_FILTER_IN_AGC;
    reg &= ~AR_PHY_TIMING11_USE_SPUR_FILTER_IN_SELFCOR;
    AR_WRITE(A, AR_PHY_TIMING11, reg);
    AR_CLRBITS(A, AR_PHY_SFCORR_EXT, AR_PHY_SFCORR_EXT_SPUR_SUBCHANNEL_SD);
    AR_CLRBITS(A, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_RSSI);
    reg = AR_READ(A, AR_PHY_SPUR_REG);
    reg = RW(reg, AR_PHY_SPUR_REG_MASK_RATE_CNTL, 0);
    reg &= ~AR_PHY_SPUR_REG_EN_VIT_SPUR_RSSI;
    reg &= ~AR_PHY_SPUR_REG_ENABLE_NF_RSSI_SPUR_MIT;
    reg &= ~AR_PHY_SPUR_REG_ENABLE_MASK_PPM;
    AR_WRITE(A, AR_PHY_SPUR_REG, reg);
    AR_WRITE_BARRIER(A);
    int i, spur = 0;
    for (i = 0; i < AR9380_EEPROM_MODAL_SPURS; i++) {
        spur = sp[i];
        if (spur == 0) return;
        spur = 2300 + spur - (int)freq_atual;
        if ((spur < 0 ? -spur : spur) < 10) break;
    }
    if (i == AR9380_EEPROM_MODAL_SPURS) return;
    int spur_delta_phase = (spur * 262144) / 5;
    int spur_freq_sd = (spur * 512) / 11;
    AR_SETBITS(A, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_FILTER);
    reg = AR_READ(A, AR_PHY_TIMING11);
    reg = RW(reg, AR_PHY_TIMING11_SPUR_FREQ_SD, spur_freq_sd);
    reg = RW(reg, AR_PHY_TIMING11_SPUR_DELTA_PHASE, spur_delta_phase);
    reg |= AR_PHY_TIMING11_USE_SPUR_FILTER_IN_AGC;
    reg |= AR_PHY_TIMING11_USE_SPUR_FILTER_IN_SELFCOR;
    AR_WRITE(A, AR_PHY_TIMING11, reg);
    AR_CLRBITS(A, AR_PHY_SFCORR_EXT, AR_PHY_SFCORR_EXT_SPUR_SUBCHANNEL_SD);
    AR_SETBITS(A, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_SPUR_RSSI);
    reg = AR_READ(A, AR_PHY_SPUR_REG);
    reg = RW(reg, AR_PHY_SPUR_REG_MASK_RATE_CNTL, 0xff);
    reg = RW(reg, AR_PHY_SPUR_REG_SPUR_RSSI_THRESH, 34);
    reg |= AR_PHY_SPUR_REG_EN_VIT_SPUR_RSSI;
    if (AR_READ(A, AR_PHY_MODE) & AR_PHY_MODE_DYNAMIC) reg |= AR_PHY_SPUR_REG_ENABLE_NF_RSSI_SPUR_MIT;
    reg |= AR_PHY_SPUR_REG_ENABLE_MASK_PPM;
    AR_WRITE(A, AR_PHY_SPUR_REG, reg);
    int idx = (spur * 16) / 5;
    if (idx < 0) idx--;
    AR_SETBITS(A, AR_PHY_TIMING4, AR_PHY_TIMING4_ENABLE_PILOT_MASK | AR_PHY_TIMING4_ENABLE_CHAN_MASK);
    reg = AR_READ(A, AR_PHY_PILOT_SPUR_MASK);
    reg = RW(reg, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_IDX_A, idx);
    reg = RW(reg, AR_PHY_PILOT_SPUR_MASK_CF_PILOT_MASK_A, 0x0c);
    AR_WRITE(A, AR_PHY_PILOT_SPUR_MASK, reg);
    reg = AR_READ(A, AR_PHY_SPUR_MASK_A);
    reg = RW(reg, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_IDX_A, idx);
    reg = RW(reg, AR_PHY_SPUR_MASK_A_CF_PUNC_MASK_A, 0xa0);
    AR_WRITE(A, AR_PHY_SPUR_MASK_A, reg);
    reg = AR_READ(A, AR_PHY_CHAN_SPUR_MASK);
    reg = RW(reg, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_IDX_A, idx);
    reg = RW(reg, AR_PHY_CHAN_SPUR_MASK_CF_CHAN_MASK_A, 0x0c);
    AR_WRITE(A, AR_PHY_CHAN_SPUR_MASK, reg);
    AR_WRITE_BARRIER(A);
}

static int pmu_escrever(uint32_t addr, uint32_t val)
{
    AR_WRITE(A, addr, val);
    for (int n = 0; n < 100; n++) {
        if (AR_READ(A, addr) == val) return 0;
        AR_WRITE(A, addr, val);
        AR_WRITE_BARRIER(A);
        DELAY(10);
    }
    return -1;
}

static void init_swreg(void)
{
    const struct ar9380_eeprom *e = eep();
    pmu_escrever(AR_PHY_PMU2, AR_READ(A, AR_PHY_PMU2) & ~AR_PHY_PMU2_PGM);
    if (e->baseEepHeader.featureEnable & AR_EEP_INTERNAL_REGULATOR) {
        pmu_escrever(AR_PHY_PMU1, 0x131dc17a);
        uint32_t reg = AR_READ(A, AR_PHY_PMU2);
        reg = (reg & ~0xffc00000u) | 0x10000000u;
        pmu_escrever(AR_PHY_PMU2, reg);
    } else pmu_escrever(AR_PHY_PMU1, AR_READ(A, AR_PHY_PMU1) | AR_PHY_PMU1_PWD);
    pmu_escrever(AR_PHY_PMU2, AR_READ(A, AR_PHY_PMU2) | AR_PHY_PMU2_PGM);
}

/* ar9380_init_from_rom (AR9485, 2,4 GHz): lo que la calibración de fábrica dice del PHY. */
static void init_from_rom(void)
{
    const struct ar9380_eeprom *e = eep();
    const struct ar9380_modal_eep_header *modal = &e->modalHeader2G;
    uint32_t reg;
    reg = AR_READ(A, AR9485_PHY_65NM_CH0_TOP2);
    reg = RW(reg, AR9485_PHY_65NM_CH0_TOP2_XPABIASLVL, modal->xpaBiasLvl);
    AR_WRITE(A, AR9485_PHY_65NM_CH0_TOP2, reg);
    reg = AR_READ(A, AR_PHY_SWITCH_COM);
    reg = RW(reg, AR_SWITCH_TABLE_COM_ALL, modal->antCtrlCommon);
    AR_WRITE(A, AR_PHY_SWITCH_COM, reg);
    reg = AR_READ(A, AR_PHY_SWITCH_COM_2);
    reg = RW(reg, AR_SWITCH_TABLE_COM_2_ALL, modal->antCtrlCommon2);
    AR_WRITE(A, AR_PHY_SWITCH_COM_2, reg);
    reg = AR_READ(A, AR_PHY_SWITCH_CHAIN(0));
    reg = RW(reg, AR_SWITCH_TABLE_ALL, modal->antCtrlChain[0]);
    AR_WRITE(A, AR_PHY_SWITCH_CHAIN(0), reg);
    uint8_t div = e->base_ext1.ant_div_control;
    reg = AR_READ(A, AR_PHY_MC_GAIN_CTRL);
    reg = RW(reg, AR_PHY_MC_GAIN_CTRL_ANT_DIV_CTRL_ALL, MS(div, AR_EEP_ANT_DIV_CTRL_ALL));
    if (div & AR_EEP_ANT_DIV_CTRL_ANT_DIV) reg |= AR_PHY_MC_GAIN_CTRL_ENABLE_ANT_DIV;
    else reg &= ~AR_PHY_MC_GAIN_CTRL_ENABLE_ANT_DIV;
    AR_WRITE(A, AR_PHY_MC_GAIN_CTRL, reg);
    reg = AR_READ(A, AR_PHY_CCK_DETECT);
    if (div & AR_EEP_ANT_DIV_CTRL_FAST_DIV) reg |= AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV;
    else reg &= ~AR_PHY_CCK_DETECT_BB_ENABLE_ANT_FAST_DIV;
    AR_WRITE(A, AR_PHY_CCK_DETECT, reg);
    if (e->baseEepHeader.miscConfiguration & AR_EEP_DRIVE_STRENGTH) {
        reg = AR_READ(A, AR_PHY_65NM_CH0_BIAS1);
        reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_0, 5); reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_1, 5);
        reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_2, 5); reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_3, 5);
        reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_4, 5); reg = RW(reg, AR_PHY_65NM_CH0_BIAS1_5, 5);
        AR_WRITE(A, AR_PHY_65NM_CH0_BIAS1, reg);
        reg = AR_READ(A, AR_PHY_65NM_CH0_BIAS2);
        reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_0, 5); reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_1, 5);
        reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_2, 5); reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_3, 5);
        reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_4, 5); reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_5, 5);
        reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_6, 5); reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_7, 5);
        reg = RW(reg, AR_PHY_65NM_CH0_BIAS2_8, 5);
        AR_WRITE(A, AR_PHY_65NM_CH0_BIAS2, reg);
        reg = AR_READ(A, AR_PHY_65NM_CH0_BIAS4);
        reg = RW(reg, AR_PHY_65NM_CH0_BIAS4_0, 5); reg = RW(reg, AR_PHY_65NM_CH0_BIAS4_1, 5);
        reg = RW(reg, AR_PHY_65NM_CH0_BIAS4_2, 5);
        AR_WRITE(A, AR_PHY_65NM_CH0_BIAS4, reg);
    }
    reg = AR_READ(A, AR_PHY_EXT_ATTEN_CTL(0));
    reg = RW(reg, AR_PHY_EXT_ATTEN_CTL_XATTEN1_DB, modal->xatten1DB[0]);
    reg = RW(reg, AR_PHY_EXT_ATTEN_CTL_XATTEN1_MARGIN, modal->xatten1Margin[0]);
    AR_WRITE(A, AR_PHY_EXT_ATTEN_CTL(0), reg);
    init_swreg();
    if (e->baseEepHeader.featureEnable & AR_EEP_TUNING_CAPS) {
        reg = AR_READ(A, AR9485_PHY_CH0_XTAL);
        reg = RW(reg, AR9485_PHY_CH0_XTAL_CAPINDAC, e->baseEepHeader.params_for_tuning_caps[0]);
        reg = RW(reg, AR9485_PHY_CH0_XTAL_CAPOUTDAC, e->baseEepHeader.params_for_tuning_caps[0]);
        AR_WRITE(A, AR9485_PHY_CH0_XTAL, reg);
    }
    AR_WRITE_BARRIER(A);
}

#define X(x) ((uint32_t)(x) << 2)

static void hw_init_tablas(void)
{
    const struct athn_ini *ini = &ar9485_1_1_ini;
    for (int i = 0; i < ini->ncmregs; i++) {
        AR_WRITE(A, X(ini->cmregs[i]), ini->cmvals[i]);
        if (AR_IS_ANALOG_REG(X(ini->cmregs[i]))) DELAY(100);
        if ((i & 0x1f) == 0) DELAY(1);
    }
    for (int i = 0; i < ini->nregs; i++) {
        AR_WRITE(A, X(ini->regs[i]), ini->vals_2g20[i]);
        if (AR_IS_ANALOG_REG(X(ini->regs[i]))) DELAY(100);
        if ((i & 0x1f) == 0) DELAY(1);
    }
    const struct athn_gain *rg = &ar9485_1_1_rx_gain, *tg = &ar9485_1_1_tx_gain;
    for (int i = 0; i < rg->nregs; i++) AR_WRITE(A, X(rg->regs[i]), rg->vals_2g[i]);
    for (int i = 0; i < tg->nregs; i++) AR_WRITE(A, X(tg->regs[i]), tg->vals_2g[i]);
    AR_WRITE_BARRIER(A);
    AR_SETBITS(A, AR_DIAG_SW, AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT);
    uint32_t reg = AR_READ(A, AR_PCU_MISC_MODE2);
    reg &= ~AR_PCU_MISC_MODE2_ADHOC_MCAST_KEYID_ENABLE;
    reg |= AR_PCU_MISC_MODE2_AGG_WEP_ENABLE_FIX;
    reg |= AR_PCU_MISC_MODE2_ENABLE_AGGWEP;
    AR_WRITE(A, AR_PCU_MISC_MODE2, reg);
    AR_WRITE_BARRIER(A);
    set_phy();
    init_chains();
    set_txpower();
}
#undef X

/* ------------------------------------------------------------ ruido de fondo y calibración */

static int16_t sext9(uint32_t v) { return (int16_t)(((v) ^ 0x100) - 0x100); }

static void nf_ler(int16_t *nf, int16_t *nfe)
{
    uint32_t reg = AR_READ(A, AR_PHY_CCA(0));
    *nf = sext9(MS(reg, AR_PHY_MINCCA_PWR));
    reg = AR_READ(A, AR_PHY_EXT_CCA(0));
    *nfe = sext9(MS(reg, AR_PHY_EXT_MINCCA_PWR));
}

static void nf_escrever(int16_t nf, int16_t nfe)
{
    uint32_t reg = AR_READ(A, AR_PHY_CCA(0));
    reg = RW(reg, AR_PHY_MAXCCA_PWR, nf);
    AR_WRITE(A, AR_PHY_CCA(0), reg);
    reg = AR_READ(A, AR_PHY_EXT_CCA(0));
    reg = RW(reg, AR_PHY_EXT_MAXCCA_PWR, nfe);
    AR_WRITE(A, AR_PHY_EXT_CCA(0), reg);
    AR_WRITE_BARRIER(A);
}

static int nf_obter(void)
{
    if (AR_READ(A, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF) return 0;   /* la calibración no ha terminado */
    int16_t nf, nfe;
    nf_ler(&nf, &nfe);
    nf_hist[nf_hist_cur][0] = nf;
    nf_hist[nf_hist_cur][1] = nfe;
    if (++nf_hist_cur >= 5) nf_hist_cur = 0;
    return 1;
}

static int16_t nf_capar(int nf)
{
    if (nf == 0 || nf == -1) return AR_DEFAULT_NOISE_FLOOR;
    if (nf < cca_min) return cca_min;
    if (nf > cca_max) return cca_max;
    return (int16_t)nf;
}

static int nf_mediana(const int *v, int n)
{
    if (n <= 1) return v[0];
    int s[5];
    for (int i = 0; i < n; i++) s[i] = v[i];
    for (int i = 0; i < n; i++)
        for (int j = 1; j < n - i; j++)
            if (s[j] > s[j - 1]) { int t = s[j]; s[j] = s[j - 1]; s[j - 1] = t; }
    return s[n / 2];
}

static void nf_filtrar(void)
{
    int v[5], ve[5], cur = nf_hist_cur > 0 ? nf_hist_cur - 1 : 4;
    for (int n = 0; n < nf_hist_n; n++) {
        v[n] = nf_hist[cur][0];
        ve[n] = nf_hist[cur][1];
        if (++cur >= 5) cur = 0;
    }
    nf_priv = nf_capar(nf_mediana(v, nf_hist_n));
    nf_ext_priv = nf_capar(nf_mediana(ve, nf_hist_n));
}

static void nf_aplicar(void)
{
    uint32_t agc = AR_READ(A, AR_PHY_AGC_CONTROL) &
                   (AR_PHY_AGC_CONTROL_NF | AR_PHY_AGC_CONTROL_ENABLE_NF | AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
    if (agc & AR_PHY_AGC_CONTROL_NF) { AR_CLRBITS(A, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF); AR_WRITE_BARRIER(A); }
    nf_escrever((int16_t)(nf_priv * 2), (int16_t)(nf_ext_priv * 2));
    AR_CLRBITS(A, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
    AR_CLRBITS(A, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
    AR_SETBITS(A, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
    int n;
    for (n = 0; n < 1000; n++) {
        if (!(AR_READ(A, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_NF)) break;
        DELAY(10);
    }
    nf_escrever(-50 * 2, -50 * 2);
    if (agc & AR_PHY_AGC_CONTROL_NF) { AR_SETBITS(A, AR_PHY_AGC_CONTROL, agc); AR_WRITE_BARRIER(A); }
}

static void nf_comecar(bool zerar)
{
    if (zerar) nf_hist_n = 0;
    nf_pendente = true;
    cal_nf_em = agora();
    AR_SETBITS(A, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_ENABLE_NF);
    AR_SETBITS(A, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NO_UPDATE_NF);
    AR_SETBITS(A, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);
    AR_WRITE_BARRIER(A);
}

/* ar9003_init_calib sin la IQ de transmisión: calibra el AGC y deja el ruido en marcha. */
static int init_calib(void)
{
    init_chains();
    AR_WRITE(A, AR_PHY_ACTIVE, AR_PHY_ACTIVE_DIS);
    AR_WRITE_BARRIER(A);
    DELAY(5);
    AR_WRITE(A, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);
    AR_SETBITS(A, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_CAL);
    int n;
    for (n = 0; n < 10000; n++) {
        if (!(AR_READ(A, AR_PHY_AGC_CONTROL) & AR_PHY_AGC_CONTROL_CAL)) break;
        DELAY(10);
    }
    if (n == 10000) return -1;
    AR_SETBITS(A, AR_PHY_AGC_CONTROL, AR_PHY_AGC_CONTROL_NF);   /* ruido inicial */
    AR_WRITE_BARRIER(A);
    return 0;
}

static void iq_comecar(void)
{
    uint32_t reg = AR_READ(A, AR_PHY_TIMING4);
    reg = RW(reg, AR_PHY_TIMING4_IQCAL_LOG_COUNT_MAX, 10);
    AR_WRITE(A, AR_PHY_TIMING4, reg);
    AR_WRITE(A, AR_PHY_CALMODE, AR_PHY_CALMODE_IQ);
    AR_SETBITS(A, AR_PHY_TIMING4, AR_PHY_TIMING4_DO_CAL);
    AR_WRITE_BARRIER(A);
    iq_ativo = true;
    cal_iq_em = agora();
}

static void iq_terminar(void)
{
    uint32_t pi = AR_READ(A, AR_PHY_IQ_ADC_MEAS_0_B(0));
    uint32_t pq = AR_READ(A, AR_PHY_IQ_ADC_MEAS_1_B(0));
    int32_t corr = (int32_t)AR_READ(A, AR_PHY_IQ_ADC_MEAS_2_B(0));
    iq_ativo = false;
    if (pq == 0) return;
    bool neg = corr < 0;
    if (neg) corr = -corr;
    uint32_t icd = (pi / 2 + pq / 2) / 256, qcd = pq / 64;
    if (icd == 0 || qcd == 0) return;
    int32_t ic = (int32_t)((uint32_t)corr / icd), qc = (int32_t)(pi / qcd) - 64;
    if (ic > 63) ic = 63; else if (ic < -63) ic = -63;
    if (!neg) ic = -ic;
    if (qc > 63) qc = 63; else if (qc < -63) qc = -63;
    uint32_t reg = AR_READ(A, AR_PHY_RX_IQCAL_CORR_B(0));
    reg = RW(reg, AR_PHY_RX_IQCAL_CORR_IQCORR_Q_I_COFF, ic);
    reg = RW(reg, AR_PHY_RX_IQCAL_CORR_IQCORR_Q_Q_COFF, qc);
    AR_WRITE(A, AR_PHY_RX_IQCAL_CORR_B(0), reg);
    AR_SETBITS(A, AR_PHY_RX_IQCAL_CORR_B(0), AR_PHY_RX_IQCAL_CORR_IQCORR_ENABLE);
    AR_WRITE_BARRIER(A);
}

/* ------------------------------------------------------------ colas y DMA */

static void tx_reiniciar_anillo(void)
{
    txs_cur = 0;
    memset(txs, 0, TXS_N * sizeof(struct ar_tx_status));
    AR_WRITE(A, AR_Q_STATUS_RING_START, txs_fis);
    AR_WRITE(A, AR_Q_STATUS_RING_END, txs_fis + TXS_N * sizeof(struct ar_tx_status));
    AR_WRITE_BARRIER(A);
    tx_ini = tx_qtd = 0;
    for (int i = 0; i < N_TX; i++) txb_livre[i] = true;
}

static int dma_alocar(void)
{
    if (dma_pronto) return 0;
    txs_fis = memfisica_alocar();
    txd_fis = memfisica_alocar();
    if (!txs_fis || !txd_fis) return -1;
    memset((void *)txs_fis, 0, 4096);
    memset((void *)txd_fis, 0, 4096);
    txs = (struct ar_tx_status *)txs_fis;
    txd = (struct ar_tx_desc *)txd_fis;
    for (int i = 0; i < N_TX; i++) {
        txb_fis[i] = memfisica_alocar();
        if (!txb_fis[i]) return -1;
    }
    for (int i = 0; i < N_RX_LP + N_RX_HP; i++) {
        rxb_fis[i] = memfisica_alocar();
        if (!rxb_fis[i]) return -1;
        memset((void *)rxb_fis[i], 0, sizeof(struct ar_rx_status));
    }
    dma_pronto = true;
    return 0;
}

static uint32_t rx_buf(int q, int i) { return rxb_fis[(q ? N_RX_LP : 0) + i]; }

static void rx_postar(int q, int i)
{
    uint32_t a = rx_buf(q, i);
    memset((void *)a, 0, sizeof(struct ar_rx_status));
    AR_WRITE(A, q ? AR_HP_RXDP : AR_LP_RXDP, a);
    AR_WRITE_BARRIER(A);
}

static void rx_habilitar(void)
{
    uint32_t reg = AR_READ(A, AR_RXBP_THRESH);
    reg = RW(reg, AR_RXBP_THRESH_HP, 1);
    reg = RW(reg, AR_RXBP_THRESH_LP, 1);
    AR_WRITE(A, AR_RXBP_THRESH, reg);
    AR_WRITE(A, AR_DATABUF_SIZE, BUF_RX - sizeof(struct ar_rx_status));
    for (int q = 0; q < 2; q++) {
        rx_ini[q] = 0;
        for (int i = 0; i < rx_cont[q]; i++) rx_postar(q, i);
    }
    AR_WRITE(A, AR_CR, 0);
    AR_WRITE_BARRIER(A);
}

static void set_rxfilter(uint32_t rfilt)
{
    AR_WRITE(A, AR_RX_FILTER, rfilt);
    AR_WRITE(A, AR_PHY_ERR, 0);
    AR_CLRBITS(A, AR_RXCFG, AR_RXCFG_ZLFDMA);
    AR_WRITE_BARRIER(A);
}

/* Unidifusión (las respuestas de sondeo y todo lo nuestro), difusión, multidifusión y las balizas de todos
 * (la búsqueda las necesita); no hace falta más en el modo de búsqueda. */
static uint32_t filtro_atual(void)
{
    uint32_t f = AR_RX_FILTER_UCAST | AR_RX_FILTER_BCAST | AR_RX_FILTER_MCAST | AR_RX_FILTER_COMPR_BAR | AR_RX_FILTER_BEACON;
    return f;
}

static void set_opmode(void)
{
    uint32_t reg = AR_READ(A, AR_STA_ID1);
    reg &= ~(AR_STA_ID1_ADHOC | AR_STA_ID1_STA_AP);
    reg |= AR_STA_ID1_KSRCH_MODE;
    AR_WRITE(A, AR_STA_ID1, reg);
    AR_WRITE_BARRIER(A);
}

static void rx_comecar(void)
{
    rx_habilitar();
    set_rxfilter(filtro_atual());
    AR_WRITE(A, AR_BSSMSKL, 0xffffffff);
    AR_WRITE(A, AR_BSSMSKU, 0xffff);
    set_opmode();
    AR_WRITE(A, AR_MCAST_FIL0, 0xffffffff);
    AR_WRITE(A, AR_MCAST_FIL1, 0xffffffff);
    AR_WRITE(A, AR_FILT_OFDM, 0);
    AR_WRITE(A, AR_FILT_CCK, 0);
    AR_WRITE(A, AR_MIBC, 0);
    AR_WRITE(A, AR_PHY_ERR_MASK_1, AR_PHY_ERR_OFDM_TIMING);
    AR_WRITE(A, AR_PHY_ERR_MASK_2, AR_PHY_ERR_CCK_TIMING);
    AR_WRITE(A, AR_PHY_ERR_1, 0);
    AR_WRITE(A, AR_PHY_ERR_2, 0);
    /* Cifrado por hardware solo si se pidió; si no, wifi.c lo hace por software. */
    if (cifra_hw && claves_hw) AR_CLRBITS(A, AR_DIAG_SW, AR_DIAG_ENCRYPT_DIS | AR_DIAG_DECRYPT_DIS);
    else AR_SETBITS(A, AR_DIAG_SW, AR_DIAG_ENCRYPT_DIS | AR_DIAG_DECRYPT_DIS);
    AR_CLRBITS(A, AR_DIAG_SW, AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT);
    AR_WRITE_BARRIER(A);
}

static int tx_pendente(int qid)
{
    return MS(AR_READ(A, AR_QSTS(qid)), AR_Q_STS_PEND_FR_CNT) != 0 || (AR_READ(A, AR_Q_TXE) & (1u << qid)) != 0;
}

static void tx_parar(void)
{
    for (int qid = 0; qid < QID_N; qid++) {
        AR_WRITE(A, AR_Q_TXD, 1u << qid);
        for (int n = 0; n < 40; n++) {
            if (!tx_pendente(qid)) break;
            DELAY(100);
        }
        AR_WRITE(A, AR_Q_TXD, 0);
    }
    AR_WRITE_BARRIER(A);
}

static int rx_parar(void)
{
    AR_WRITE(A, AR_CR, AR_CR_RXD);
    for (int n = 0; n < 100; n++) {
        if (!(AR_READ(A, AR_CR) & AR_CR_RXE)) return 0;
        DELAY(100);
    }
    return -1;
}

/* ------------------------------------------------------------ reinicio completo (athn_hw_reset) */

static void clock_timers(void)
{
    int clk = AR_CLOCK_RATE_2GHZ_OFDM;
    int sifs = 10;
    int slot = slot_curto ? 9 : 20;
    AR_WRITE(A, AR_D_GBL_IFS_SIFS, (uint32_t)((sifs - 2) * clk));
    AR_WRITE(A, AR_D_GBL_IFS_SLOT, (uint32_t)(slot * clk));
    int ackto = sifs + slot;
    ackto += 64 - sifs - slot;             /* arreglo de timeouts tempranos en 2,4 GHz */
    int ctsto = ackto + 48 - sifs - slot;
    uint32_t reg = AR_READ(A, AR_TIME_OUT);
    reg = RW(reg, AR_TIME_OUT_ACK, ackto * clk);
    reg = RW(reg, AR_TIME_OUT_CTS, ctsto * clk);
    AR_WRITE(A, AR_TIME_OUT, reg);
    reg = AR_READ(A, AR_USEC);
    reg = RW(reg, AR_USEC_USEC, clk - 1);
    AR_WRITE(A, AR_USEC, reg);
    AR_WRITE_BARRIER(A);
}

static void init_tx_queues(void)
{
    for (int qid = 0; qid < QID_N; qid++) {
        AR_WRITE(A, AR_DRETRY_LIMIT(qid), SM(AR_D_RETRY_LIMIT_STA_SH, 32) | SM(AR_D_RETRY_LIMIT_STA_LG, 32) |
                                          SM(AR_D_RETRY_LIMIT_FR_SH, 10));
        AR_WRITE(A, AR_QMISC(qid), AR_Q_MISC_DCU_EARLY_TERM_REQ);
        AR_WRITE(A, AR_DMISC(qid), SM(AR_D_MISC_BKOFF_THRESH, 2) | AR_D_MISC_CW_BKOFF_EN | AR_D_MISC_FRAG_WAIT_EN);
    }
    AR_SETBITS(A, AR_QMISC(7), AR_Q_MISC_FSP_DBA_GATED | AR_Q_MISC_BEACON_USE | AR_Q_MISC_CBR_INCR_DIS1);
    AR_SETBITS(A, AR_DMISC(7), SM(AR_D_MISC_ARB_LOCKOUT_CNTRL, AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL) |
                               AR_D_MISC_BEACON_USE | AR_D_MISC_POST_FR_BKOFF_DIS);
    AR_WRITE(A, AR_DLCL_IFS(7), SM(AR_D_LCL_IFS_CWMIN, 0) | SM(AR_D_LCL_IFS_CWMAX, 0) | SM(AR_D_LCL_IFS_AIFS, 1));
    AR_SETBITS(A, AR_QMISC(6), AR_Q_MISC_FSP_DBA_GATED | AR_Q_MISC_CBR_INCR_DIS1 | AR_Q_MISC_CBR_INCR_DIS0);
    AR_SETBITS(A, AR_DMISC(6), SM(AR_D_MISC_ARB_LOCKOUT_CNTRL, AR_D_MISC_ARB_LOCKOUT_CNTRL_GLOBAL));
    AR_SETBITS(A, AR_QMISC(1), AR_Q_MISC_CBR_INCR_DIS1);
    AR_SETBITS(A, AR_DMISC(5), AR_D_MISC_POST_FR_BKOFF_DIS);
    AR_WRITE(A, AR_Q_DESC_CRCCHK, AR_Q_DESC_CRCCHK_EN);
    AR_WRITE(A, AR_IMR_S0, 0x00ff0000);
    AR_WRITE(A, AR_IMR_S1, 0x00df0000);
    AR_WRITE_BARRIER(A);
}

static void init_qos(void)
{
    AR_WRITE(A, AR_MIC_QOS_CONTROL, 0x100aa);
    AR_WRITE(A, AR_MIC_QOS_SELECT, 0x3210);
    AR_WRITE(A, AR_QOS_NO_ACK, SM(AR_QOS_NO_ACK_TWO_BIT, 2) | SM(AR_QOS_NO_ACK_BIT_OFF, 5) | SM(AR_QOS_NO_ACK_BYTE_OFF, 0));
    AR_WRITE(A, AR_TXOP_X, AR_TXOP_X_VAL);
    AR_WRITE(A, AR_TXOP_0_3, 0xffffffff);
    AR_WRITE(A, AR_TXOP_4_7, 0xffffffff);
    AR_WRITE(A, AR_TXOP_8_11, 0xffffffff);
    AR_WRITE(A, AR_TXOP_12_15, 0xffffffff);
    AR_WRITE_BARRIER(A);
}

static void init_dma_regs(void)
{
    uint32_t reg = AR_READ(A, AR_TXCFG);
    reg = RW(reg, AR_TXCFG_DMASZ, AR_DMASZ_128B);
    AR_WRITE(A, AR_TXCFG, reg);
    reg = AR_READ(A, AR_RXCFG);
    reg = RW(reg, AR_RXCFG_DMASZ, AR_DMASZ_128B);
    AR_WRITE(A, AR_RXCFG, reg);
    AR_WRITE(A, AR_RXFIFO_CFG, 512);
    AR_WRITE(A, AR_PCU_TXBUF_CTRL, AR_PCU_TXBUF_CTRL_USABLE_SIZE);
    AR_WRITE_BARRIER(A);
    tx_reiniciar_anillo();
}

static int hw_reset(void)
{
    uint32_t def_ant, sta_id1, cfg_led, reg;
    if (set_power_awake() != 0) return -1;
    if ((def_ant = AR_READ(A, AR_DEF_ANTENNA)) == 0) def_ant = 1;
    sta_id1 = AR_READ(A, AR_STA_ID1) & AR_STA_ID1_BASE_RATE_11B;
    cfg_led = AR_READ(A, AR_CFG_LED) & (AR_CFG_LED_ASSOC_CTL_M | AR_CFG_LED_MODE_SEL_M | AR_CFG_LED_BLINK_THRESH_SEL_M |
                                        AR_CFG_LED_BLINK_SLOW);
    AR_WRITE(A, AR_PHY_ACTIVE, AR_PHY_ACTIVE_DIS);   /* PHY inactivo */
    AR_WRITE_BARRIER(A);
    if (athn_reset(0) != 0) return -2;
    if (set_power_awake() != 0) return -1;
    init_pll();
    set_rf_mode();
    if (rfsilent) {
        reg = (uint32_t)gpio_ler(rfsilent_pin);
        if (rfsilent_invertido) reg = !reg;
        if (!reg) return -3;               /* el interruptor de radio está apagado */
    }
    AR_SETBITS(A, AR_GPIO_INPUT_EN_VAL, AR_GPIO_JTAG_DISABLE);
    hw_init_tablas();
    reg = AR_READ(A, AR_AES_MUTE_MASK1);
    reg = RW(reg, AR_AES_MUTE_MASK1_FC0_MGMT, 0xff);
    reg = RW(reg, AR_AES_MUTE_MASK1_FC1_MGMT, ~(0x08u | 0x10u | 0x20u));   /* reintento, ahorro de energía, más datos */
    AR_WRITE(A, AR_AES_MUTE_MASK1, reg);
    set_delta_slope();
    spur_cck();
    spur_ofdm();
    init_from_rom();
    AR_WRITE(A, AR_STA_ID0, (uint32_t)inf.mac[0] | (uint32_t)inf.mac[1] << 8 | (uint32_t)inf.mac[2] << 16 |
                            (uint32_t)inf.mac[3] << 24);
    AR_WRITE(A, AR_STA_ID1, ((uint32_t)inf.mac[4] | (uint32_t)inf.mac[5] << 8) | sta_id1 | AR_STA_ID1_RTS_USE_DEF |
                            AR_STA_ID1_CRPT_MIC_ENABLE);
    set_opmode();
    AR_WRITE(A, AR_BSSMSKL, 0xffffffff);
    AR_WRITE(A, AR_BSSMSKU, 0xffff);
    AR_WRITE(A, AR_DEF_ANTENNA, def_ant);
    AR_WRITE(A, AR_BSS_ID0, 0);
    AR_WRITE(A, AR_BSS_ID1, 0);
    AR_WRITE(A, AR_ISR, 0xffffffff);
    AR_WRITE(A, AR_RSSI_THR, SM(AR_RSSI_THR_BM_THR, 7));
    if (set_synth() != 0) return -4;
    for (int i = 0; i < AR_NUM_DCU; i++) AR_WRITE(A, AR_DQCUMASK(i), 1u << i);
    init_tx_queues();
    /* Sin interrupciones: la máscara se programa como en athn pero IER (el interruptor general) queda a 0. */
    AR_WRITE(A, AR_IMR, AR_IMR_TXDESC | AR_IMR_TXEOL | AR_IMR_RXERR | AR_IMR_RXEOL | AR_IMR_RXORN | AR_IMR_RXMINTR |
                        AR_IMR_RXINTM | AR_IMR_GENTMR | AR_IMR_BCNMISC | AR_IMR_HP_RXOK);
    AR_SETBITS(A, AR_IMR_S2, AR_IMR_S2_GTT);
    AR_WRITE(A, AR_INTR_SYNC_CAUSE, 0xffffffff);
    AR_WRITE(A, AR_INTR_SYNC_ENABLE, 0);
    AR_WRITE(A, AR_INTR_SYNC_MASK, 0);
    AR_WRITE(A, AR_INTR_PRIO_ASYNC_ENABLE, 0);
    AR_WRITE(A, AR_INTR_PRIO_ASYNC_MASK, 0);
    AR_WRITE(A, AR_INTR_PRIO_SYNC_ENABLE, 0);
    AR_WRITE(A, AR_INTR_PRIO_SYNC_MASK, 0);
    AR_WRITE(A, AR_IER, 0);
    AR_WRITE(A, AR_INTR_ASYNC_ENABLE, 0);
    init_qos();
    AR_SETBITS(A, AR_PCU_MISC, AR_PCU_MIC_NEW_LOC_ENA);
    clock_timers();
    AR_SETBITS(A, AR_STA_ID1, AR_STA_ID1_PRESERVE_SEQNUM);   /* sin numeración del chip: la pone ar9485_enviar */
    init_dma_regs();
    AR_WRITE(A, AR_OBS, 8);
    AR_WRITE(A, AR_RIMT, SM(AR_RIMT_FIRST, 2000) | SM(AR_RIMT_LAST, 500));
    AR_WRITE(A, AR_TIMT, SM(AR_TIMT_FIRST, 2000) | SM(AR_TIMT_LAST, 500));
    AR_WRITE(A, AR_MIRT, SM(AR_MIRT_RATE_THRES, 2000));
    /* Activa el PHY. */
    AR_WRITE(A, AR_PHY_ACTIVE, AR_PHY_ACTIVE_EN);
    AR_WRITE_BARRIER(A);
    DELAY(AR_BASE_PHY_ACTIVE_DELAY + synth_delay());
    if (init_calib() != 0) return -5;
    AR_WRITE(A, AR_CFG_LED, cfg_led | AR_CFG_SCLK_32KHZ);
    AR_WRITE_BARRIER(A);
    return 0;
}

static void chaves_reiniciar_tudo(void);

/* Cambia de canal con un reinicio completo (como athn_switch_chan). */
static int canal_aplicar(uint8_t canal)
{
    uint16_t f = ar9485_canal_freq(canal);
    if (!f) return -1;
    AR_WRITE(A, AR_IER, 0);
    tx_parar();
    AR_SETBITS(A, AR_DIAG_SW, AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT);
    AR_WRITE(A, AR_MIBC, AR_MIBC_FMC);
    AR_WRITE(A, AR_MIBC, AR_MIBC_CMC);
    AR_WRITE(A, AR_FILT_OFDM, 0);
    AR_WRITE(A, AR_FILT_CCK, 0);
    set_rxfilter(0);
    (void)rx_parar();
    canal_atual = canal;
    freq_atual = f;
    int e = hw_reset();
    if (e != 0) { klog("[AR9485] reinicio del canal %u falló (%d)\n", canal, e); return e; }
    rx_comecar();
    inf.canal = canal;
    nf_comecar(true);
    return 0;
}

/* ------------------------------------------------------------ caché de claves */

static void chave_zerar(int entrada)
{
    AR_WRITE(A, AR_KEYTABLE_KEY0(entrada), 0);
    AR_WRITE(A, AR_KEYTABLE_KEY1(entrada), 0);
    AR_WRITE(A, AR_KEYTABLE_KEY2(entrada), 0);
    AR_WRITE(A, AR_KEYTABLE_KEY3(entrada), 0);
    AR_WRITE(A, AR_KEYTABLE_KEY4(entrada), 0);
    AR_WRITE(A, AR_KEYTABLE_TYPE(entrada), AR_KEYTABLE_TYPE_CLR);
    AR_WRITE(A, AR_KEYTABLE_MAC0(entrada), 0);
    AR_WRITE(A, AR_KEYTABLE_MAC1(entrada), 0);
    AR_WRITE_BARRIER(A);
}

static void chaves_reiniciar_tudo(void)
{
    for (int i = 0; i < AR_KEYTABLE_SIZE; i++) chave_zerar(i);
    claves_hw = false;
}

void ar9485_limpar_chaves(void)
{
    if (!inf.pronto) return;
    chaves_reiniciar_tudo();
    AR_SETBITS(A, AR_DIAG_SW, AR_DIAG_ENCRYPT_DIS | AR_DIAG_DECRYPT_DIS);
}

void ar9485_chave(int entrada, const uint8_t k[16], const uint8_t *mac)
{
    if (!inf.pronto || !cifra_hw || entrada < 0 || entrada >= AR_KEYTABLE_SIZE) return;
#define L4(p) ((uint32_t)(p)[0] | (uint32_t)(p)[1] << 8 | (uint32_t)(p)[2] << 16 | (uint32_t)(p)[3] << 24)
#define L2(p) ((uint32_t)(p)[0] | (uint32_t)(p)[1] << 8)
    /* Las escrituras van por pares: las de los registros 0x0 y 0x8 van a un registro temporal. */
    AR_WRITE(A, AR_KEYTABLE_KEY0(entrada), L4(&k[0]));
    AR_WRITE(A, AR_KEYTABLE_KEY1(entrada), L2(&k[4]));
    AR_WRITE(A, AR_KEYTABLE_KEY2(entrada), L4(&k[6]));
    AR_WRITE(A, AR_KEYTABLE_KEY3(entrada), L2(&k[10]));
    AR_WRITE(A, AR_KEYTABLE_KEY4(entrada), L4(&k[12]));
    AR_WRITE(A, AR_KEYTABLE_TYPE(entrada), AR_KEYTABLE_TYPE_CCM);
    uint32_t lo = 0, hi = 0, unicast = AR_KEYTABLE_VALID;
    if (mac) ar9485_chave_mac(mac, &lo, &hi);
    else unicast = 0;
    AR_WRITE(A, AR_KEYTABLE_MAC0(entrada), lo);
    AR_WRITE(A, AR_KEYTABLE_MAC1(entrada), hi | unicast);
    AR_WRITE_BARRIER(A);
    AR_CLRBITS(A, AR_DIAG_SW, AR_DIAG_ENCRYPT_DIS | AR_DIAG_DECRYPT_DIS);
    claves_hw = true;
#undef L4
#undef L2
}

void ar9485_cifra_hw(bool sim) { cifra_hw = sim; inf.cifra_hw = sim; }

/* ------------------------------------------------------------ transmisión */

static void tx_confirmar(void)
{
    for (int k = 0; k < TXS_N; k++) {
        struct ar_tx_status *ds = &txs[txs_cur];
        if (!(ds->ds_status8 & AR_TXS8_DONE)) return;
        txs_cur = (txs_cur + 1) % TXS_N;
        if ((ds->ds_info & (AR_TXI_DESC_ID_M | AR_TXI_DESC_TX)) != (SM(AR_TXI_DESC_ID, AR_VENDOR_ATHEROS) | AR_TXI_DESC_TX)) {
            memset(ds, 0, sizeof *ds);
            continue;
        }
        if (tx_qtd > 0) {
            int b = txfila[tx_ini];
            tx_ini = (tx_ini + 1) % N_TX;
            tx_qtd--;
            txb_livre[b] = true;
        }
        uint32_t st3 = ds->ds_status3;
        uint8_t fail = (uint8_t)(MS(st3, AR_TXS3_DATA_FAIL_CNT) + MS(ds->ds_status8, AR_TXS8_FINAL_IDX) * 2);
        if (st3 & AR_TXS3_EXCESSIVE_RETRIES) inf.tx_falhas++;
        else if (fail) inf.tx_reintentos++;
        else inf.tx_ok++;
        if (st3 & AR_TXS3_UNDERRUN) {
            inf.tx_underrun++;
            uint32_t r = AR_READ(A, AR_TXCFG);
            uint32_t ft = MS(r, AR_TXCFG_FTRIG);
            if (ft < 0x1f) AR_WRITE(A, AR_TXCFG, RW(r, AR_TXCFG_FTRIG, ft + 1));   /* más disparo (el AR9485 llega a 2 KB) */
        }
        memset(ds, 0, sizeof *ds);
    }
}

/* Control de tasa sencillo (la AMRR del RTL8188EU, la misma escalera): se mira cada ~64 envíos. */
static void taxa_ajustar(void)
{
    uint32_t total = (uint32_t)inf.tx_ok + inf.tx_reintentos + inf.tx_falhas;
    if (total < 32) return;
    if (inf.tx_falhas * 3 > total || inf.tx_reintentos * 3 > total) inf.taxa = wn_taxa_descer(inf.taxa, false);
    else if (inf.tx_reintentos * 10 <= total) inf.taxa = wn_taxa_subir(inf.taxa, false, 0);
    inf.tx_ok = inf.tx_reintentos = inf.tx_falhas = 0;
}

int ar9485_enviar(const uint8_t *q, uint32_t n, int tipo, bool cifrar)
{
    if (!inf.pronto) return -1;
    tx_confirmar();
    if (n < 24 || n > BUF_TX - 16) return -1;
    if (tx_qtd >= AR9003_TX_QDEPTH || tx_qtd >= N_TX) { inf.tx_cheia++; return -2; }
    int b = -1;
    for (int i = 0; i < N_TX; i++)
        if (txb_livre[i]) { b = i; break; }
    if (b < 0) { inf.tx_cheia++; return -2; }
    taxa_ajustar();
    uint8_t type = AR_FRAME_TYPE_NORMAL;
    if ((q[0] & 0xFC) == 0x50) type = AR_FRAME_TYPE_PROBE_RESP;
    bool mcast = (q[4] & 1) != 0;
    bool dados = (q[0] & 0x0C) == 0x08;
    bool hw_cifra = cifrar && cifra_hw && claves_hw;
    uint32_t totlen = n + 4 + (hw_cifra ? 8 : 0);   /* FCS (y el MIC del CCMP, que añade el chip) */
    memcpy((void *)txb_fis[b], q, n);
    tx_seq = (uint16_t)((tx_seq + 1) & 0x0fff);
    ar9485_seq_poner((uint8_t *)txb_fis[b], tx_seq);
    struct ar_tx_desc *ds = &txd[b];
    memset(ds, 0, sizeof *ds);
    ds->ds_info = SM(AR_TXI_DESC_ID, AR_VENDOR_ATHEROS) | SM(AR_TXI_DESC_NDWORDS, 23) | SM(AR_TXI_QCU_NUM, QID_BE) |
                  AR_TXI_DESC_TX | AR_TXI_CTRL_STAT;
    ds->ds_ctl11 = AR_TXC11_CLR_DEST_MASK | SM(AR_TXC11_XMIT_POWER, AR_MAX_RATE_POWER);
    ds->ds_ctl12 = SM(AR_TXC12_FRAME_TYPE, type);
    if (mcast) ds->ds_ctl12 |= AR_TXC12_NO_ACK;
    uint8_t encr = AR_ENCR_TYPE_CLEAR;
    if (hw_cifra) {
        encr = AR_ENCR_TYPE_AES;
        ds->ds_ctl12 |= SM(AR_TXC12_DEST_IDX, 4);   /* entrada 4: la clave de par */
        ds->ds_ctl11 |= AR_TXC11_DEST_IDX_VALID;
    }
    ds->ds_ctl17 = SM(AR_TXC17_ENCR_TYPE, encr);
    /* Serie de tasas: la actual y tres bajadas; gestión y EAPOL, siempre CCK 1 Mb/s. */
    uint8_t ridx[4];
    if (!dados || mcast || tipo != AR9485_TX_DADOS) ridx[0] = ridx[1] = ridx[2] = ridx[3] = 0;
    else {
        ridx[0] = inf.taxa;
        ridx[1] = wn_taxa_descer(ridx[0], false);
        ridx[2] = wn_taxa_descer(ridx[1], false);
        ridx[3] = 0;
    }
    uint8_t hw[4];
    uint16_t dur[4] = { 0, 0, 0, 0 };
    for (int i = 0; i < 4; i++) {
        hw[i] = ar9485_taxa_hw(ridx[i]);
        if (!ar9485_taxa_ofdm(ridx[i]) && ridx[i] != 0 && preambulo_curto) hw[i] |= 0x04;
        if (!(ds->ds_ctl12 & AR_TXC12_NO_ACK)) dur[i] = (uint16_t)ar9485_txtime(14, ar9485_taxa_resposta(ridx[i]), preambulo_curto);
    }
    ds->ds_ctl13 = SM(AR_TXC13_XMIT_DATA_TRIES0, 2) | SM(AR_TXC13_XMIT_DATA_TRIES1, 2) | SM(AR_TXC13_XMIT_DATA_TRIES2, 2) |
                   SM(AR_TXC13_XMIT_DATA_TRIES3, 4) | AR_TXC13_DUR_UPDATE_ENA;
    ds->ds_ctl14 = SM(AR_TXC14_XMIT_RATE0, hw[0]) | SM(AR_TXC14_XMIT_RATE1, hw[1]) | SM(AR_TXC14_XMIT_RATE2, hw[2]) |
                   SM(AR_TXC14_XMIT_RATE3, hw[3]);
    ds->ds_ctl15 = SM(AR_TXC15_PACKET_DUR0, dur[0]) | SM(AR_TXC15_PACKET_DUR1, dur[1]);
    ds->ds_ctl16 = SM(AR_TXC16_PACKET_DUR2, dur[2]) | SM(AR_TXC16_PACKET_DUR3, dur[3]);
    ds->ds_ctl18 = SM(AR_TXC18_CHAIN_SEL0, txchainmask) | SM(AR_TXC18_CHAIN_SEL1, txchainmask) |
                   SM(AR_TXC18_CHAIN_SEL2, txchainmask) | SM(AR_TXC18_CHAIN_SEL3, txchainmask);
    ds->ds_ctl11 |= SM(AR_TXC11_FRAME_LEN, totlen);
    ds->ds_ctl19 = AR_TXC19_NOT_SOUNDING;
    ds->ds_segs[0].ds_data = txb_fis[b];
    ds->ds_segs[0].ds_ctl = SM(AR_TXC_BUF_LEN, n);
    uint32_t segs[8];
    for (int i = 0; i < 4; i++) { segs[2 * i] = ds->ds_segs[i].ds_data; segs[2 * i + 1] = ds->ds_segs[i].ds_ctl; }
    ds->ds_ctl10 = SM(AR_TXC10_PTR_CHK_SUM, ar9485_tx_soma(ds->ds_info, ds->ds_link, segs));
    txfila[(tx_ini + tx_qtd) % N_TX] = (uint8_t)b;
    txb_livre[b] = false;
    tx_qtd++;
    __asm__ volatile("" ::: "memory");
    AR_WRITE(A, AR_QTXDP(QID_BE), txd_fis + (uint32_t)b * (uint32_t)sizeof(struct ar_tx_desc));
    AR_WRITE_BARRIER(A);
    inf.tx_quadros++;
    return 0;
}

/* ------------------------------------------------------------ recepción */

static int rssi_dbm(uint32_t rssi)
{
    int v = -95 + (int)rssi;   /* el RSSI del chip son dB sobre el ruido de fondo (-95 dBm) */
    return v > -20 ? -20 : v < -100 ? -100 : v;
}

static int rx_um(int q, ar9485_rx_t chegou)
{
    int i = rx_ini[q];
    struct ar_rx_status *ds = (struct ar_rx_status *)rx_buf(q, i);
    if (!(ds->ds_status11 & AR_RXS11_DONE)) return 0;
    int entregue = 0;
    if ((ds->ds_info & (AR_RXI_DESC_ID_M | AR_RXI_DESC_TX | AR_RXI_CTRL_STAT)) != SM(AR_RXI_DESC_ID, AR_VENDOR_ATHEROS)) goto saltar;
    if (!(ds->ds_status11 & AR_RXS11_FRAME_OK)) {
        if (ds->ds_status11 & AR_RXS11_CRC_ERR) inf.rx_crc++;
        else if (ds->ds_status11 & AR_RXS11_PHY_ERR) inf.rx_phy++;
        inf.rx_erros++;
        goto saltar;
    }
    uint32_t len = MS(ds->ds_status2, AR_RXS2_DATA_LEN);
    if (len < 10 + 4 || len > BUF_RX - sizeof *ds) { inf.rx_erros++; goto saltar; }
    {
        uint8_t *q80 = (uint8_t *)&ds[1];
        /* El chip rellena con 2 bytes tras la cabecera cuando ésta no es múltiplo de 4. */
        if ((q80[0] & 0x0C) != 0x04) {
            uint32_t hl = 24;
            if ((q80[1] & 3) == 3) hl += 6;
            if (q80[0] & 0x80) hl += 2;
            if ((q80[0] & 0x80) && (q80[1] & 0x80)) hl += 4;
            if ((hl & 3) && len > hl + 2) {
                memmove(q80 + 2, q80, hl);
                q80 += 2;
                len -= 2;
            }
        }
        len -= 4;   /* FCS */
        uint8_t taxa_hw = (uint8_t)MS(ds->ds_status1, AR_RXS1_RATE);
        (void)taxa_hw;
        int rssi = rssi_dbm(MS(ds->ds_status5, AR_RXS5_RSSI_COMBINED));
        inf.rx_quadros++;
        chegou(q80, len, rssi, cifra_hw && claves_hw);
        entregue = 1;
    }
saltar:
    rx_postar(q, i);
    rx_ini[q] = (i + 1) % rx_cont[q];
    AR_WRITE(A, AR_CR, 0);
    AR_WRITE_BARRIER(A);
    return entregue ? 1 : -1;
}

int ar9485_receber(ar9485_rx_t chegou)
{
    if (!inf.pronto) return 0;
    tx_confirmar();
    int n = 0;
    for (int q = 0; q < 2; q++)
        for (int k = 0; k < rx_cont[q]; k++) {
            int r = rx_um(q, chegou);
            if (r == 0) break;
            if (r > 0) n++;
        }
    return n;
}

/* ------------------------------------------------------------ ordenes de wifi.c */

void ar9485_canal(uint8_t canal)
{
    if (!inf.pronto) return;
    if (canal == canal_atual) return;   /* el mismo canal: no hace falta un reinicio entero */
    if (canal_aplicar(canal) != 0) inf.pronto = false;
}

void ar9485_modo_busca(bool busca)
{
    if (busca == modo_busca) return;
    modo_busca = busca;
    if (inf.pronto) set_rxfilter(filtro_atual());
}

void ar9485_associado(const uint8_t b[6], uint16_t aid, bool curto, bool pre_curto)
{
    if (!inf.pronto) return;
    AR_WRITE(A, AR_BSS_ID0, (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
    AR_WRITE(A, AR_BSS_ID1, ((uint32_t)b[4] | (uint32_t)b[5] << 8) | SM(AR_BSS_ID1_AID, aid & 0x3fff));
    slot_curto = curto;
    preambulo_curto = pre_curto;
    clock_timers();
    inf.taxa = wn_taxa_inicial(false, 0);
    inf.tx_ok = inf.tx_reintentos = inf.tx_falhas = 0;
    AR_WRITE_BARRIER(A);
}

void ar9485_desassociado(void)
{
    if (!inf.pronto) return;
    AR_WRITE(A, AR_BSS_ID0, 0);
    AR_WRITE(A, AR_BSS_ID1, 0);
    ar9485_limpar_chaves();
    slot_curto = preambulo_curto = false;
    clock_timers();
    AR_WRITE_BARRIER(A);
}

void ar9485_periodico(void)
{
    if (!inf.pronto || !((int32_t)(agora() - periodico_em) >= 0)) return;
    periodico_em = agora() + TIQUES_POR_SEGUNDO / 2;
    tx_confirmar();
    if (nf_pendente && nf_obter()) {
        if (nf_hist_n < 5) nf_hist_n++;
        nf_filtrar();
        nf_aplicar();
        nf_pendente = false;
    }
    if ((int32_t)(agora() - (cal_nf_em + 240 * TIQUES_POR_SEGUNDO)) >= 0) nf_comecar(false);
    if (iq_ativo) {
        if (!(AR_READ(A, AR_PHY_TIMING4) & AR_PHY_TIMING4_DO_CAL)) iq_terminar();
    } else if ((int32_t)(agora() - (cal_iq_em + 30 * TIQUES_POR_SEGUNDO)) >= 0) iq_comecar();
}

void ar9485_diagnostico(void)
{
    if (!inf.pronto) return;
    klog("[AR9485] SREV %08x, canal %u, RX_FILTER %08x, DIAG_SW %08x, IMR %08x, RTC %x; rx %u (err %u: crc %u phy %u), "
         "tx %u (llena %u, perdidas %u, underrun %u), tasa %u\n", AR_READ(A, AR_SREV), canal_atual, AR_READ(A, AR_RX_FILTER),
         AR_READ(A, AR_DIAG_SW), AR_READ(A, AR_IMR), AR_READ(A, AR_RTC_STATUS) & AR_RTC_STATUS_M, inf.rx_quadros,
         inf.rx_erros, inf.rx_crc, inf.rx_phy, inf.tx_quadros, inf.tx_cheia, inf.tx_perdidas, inf.tx_underrun, inf.taxa);
}

const ar9485_t *ar9485_info(void) { return &inf; }

/* ------------------------------------------------------------ encendido */

int ar9485_iniciar(const char **motivo)
{
    const char *m_pci = "";
    inf.pronto = false;
    if (!achada && !ar9485_procurar()) { *motivo = "no hay tarjeta AR9485"; return -1; }
    if (pci_ligar(&m_pci) != 0) { *motivo = m_pci; return -1; }
    uint32_t reg = AR_READ(A, AR_SREV);
    if (reg == 0xffffffffu) { *motivo = "el chip no contesta (lecturas 0xFFFFFFFF)"; return -1; }
    if (MS(reg, AR_SREV_ID) == 0xff) {
        ar_sc_.mac_ver = MS(reg, AR_SREV_VERSION2);
        ar_sc_.mac_rev = MS(reg, AR_SREV_REVISION2);
    } else {
        ar_sc_.mac_ver = MS(reg, AR_SREV_VERSION);
        ar_sc_.mac_rev = MS(reg, AR_SREV_REVISION);
    }
    inf.mac_ver = ar_sc_.mac_ver;
    inf.mac_rev = ar_sc_.mac_rev;
    klog("[AR9485] SREV %08x: versión %x revisión %u\n", reg, ar_sc_.mac_ver, ar_sc_.mac_rev);
    if (ar_sc_.mac_ver != AR_SREV_VERSION_9485) { *motivo = "el chip no es un AR9485 (versión de MAC distinta)"; return -1; }
    if (reset_power_on_ar() != 0) { *motivo = "no se pudo reiniciar el chip (el RTC no despierta)"; return -1; }
    if (set_power_awake() != 0) { *motivo = "no se pudo despertar el chip"; return -1; }
    config_pcie();
    if (rom_procurar() != 0) { *motivo = "no se encuentra la EEPROM ni la OTP"; return -1; }
    klog("[AR9485] %s de %d bytes\n", eep_otp ? "OTP" : "EEPROM", eep_size);
    if (rom_ler() != 0) { *motivo = "no se pudo leer la ROM (ningún bloque con suma buena)"; return -1; }
    const struct ar9380_eeprom *e = eep();
    memcpy(inf.mac, e->macAddr, 6);
    if (!ar9485_mac_valida(inf.mac)) klog("[AR9485] la MAC de la ROM no es válida (%02x:%02x:%02x:%02x:%02x:%02x)\n",
                                          inf.mac[0], inf.mac[1], inf.mac[2], inf.mac[3], inf.mac[4], inf.mac[5]);
    txchainmask = (uint8_t)MS(e->baseEepHeader.txrxMask, AR_EEP_TX_MASK);
    rxchainmask = (uint8_t)MS(e->baseEepHeader.txrxMask, AR_EEP_RX_MASK);
    if (!txchainmask) txchainmask = 1;
    if (!rxchainmask) rxchainmask = 1;
    ntx = (txchainmask & 1) + ((txchainmask >> 1) & 1) + ((txchainmask >> 2) & 1);
    nrx = (rxchainmask & 1) + ((rxchainmask >> 1) & 1) + ((rxchainmask >> 2) & 1);
    inf.txcadeias = (uint8_t)ntx;
    inf.rxcadeias = (uint8_t)nrx;
    rfsilent = (e->baseEepHeader.rfSilent & AR_EEP_RFSILENT_ENABLED) != 0;
    rfsilent_pin = (uint8_t)MS(e->baseEepHeader.rfSilent, AR_EEP_RFSILENT_GPIO_SEL);
    rfsilent_invertido = (e->baseEepHeader.rfSilent & AR_EEP_RFSILENT_POLARITY) != 0;
    cca_min = AR9380_PHY_CCA_MIN_GOOD_VAL_2GHZ;
    cca_max = AR9380_PHY_CCA_MAX_GOOD_VAL_2GHZ;
    klog("[AR9485] ROM v%u, plantilla %u, MAC %02x:%02x:%02x:%02x:%02x:%02x, %uT%uR (máscaras %x/%x), 11g %s, 11n %s%s\n",
         e->eepromVersion, e->templateVersion, inf.mac[0], inf.mac[1], inf.mac[2], inf.mac[3], inf.mac[4], inf.mac[5], ntx, nrx,
         txchainmask, rxchainmask, (e->baseEepHeader.opFlags & AR_OPFLAGS_11G) ? "sí" : "no",
         (e->baseEepHeader.opFlags & AR_OPFLAGS_11N) ? "sí" : "no", rfsilent ? ", interruptor de radio" : "");
    if (dma_alocar() != 0) { *motivo = "sin memoria para los anillos DMA"; return -1; }
    if (rfsilent) rfsilent_init();
    chaves_reiniciar_tudo();
    inf.taxa = wn_taxa_inicial(false, 0);
    periodico_em = agora();
    cal_iq_em = agora();
    int r = canal_aplicar(1);
    if (r == -3) { *motivo = "el interruptor de radio está apagado (Fn + tecla de Wi-Fi)"; return -1; }
    if (r != 0) { *motivo = "el reinicio del chip falló (ver el registro)"; return -1; }
    AR_SETBITS(A, AR_DIAG_SW, AR_DIAG_ENCRYPT_DIS | AR_DIAG_DECRYPT_DIS);
    inf.pronto = true;
    klog("[AR9485] chip listo, canal %u (%u MHz)\n", canal_atual, freq_atual);
    ar9485_diagnostico();
    return 0;
}

void ar9485_parar(void)
{
    if (!inf.pronto) return;
    inf.pronto = false;
    AR_WRITE(A, AR_IER, 0);
    tx_parar();
    AR_SETBITS(A, AR_DIAG_SW, AR_DIAG_RX_DIS | AR_DIAG_RX_ABORT);
    (void)rx_parar();
    set_power_sleep();
}

void ar9485_taxa_acertou(uint8_t taxa) { inf.taxa = taxa; }
