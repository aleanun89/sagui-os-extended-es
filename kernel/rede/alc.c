/* alc.c - driver EXPERIMENTAL de las Ethernet Atheros AR813x/AR815x
 * (familia L1C/L2C, el «atl1c» de Linux), D200.
 *
 * El objetivo es la AR8152 v1.1 (PCI 1969:2060, Fast Ethernet) del Toshiba
 * Satellite L635; con el mismo código van la AR8152 v2.0 (2062), la AR8151
 * (1073, 1083) y la AR8131/AR8132 (1063, 1062). El QEMU no emula ninguna,
 * así que, como el r8169, el arranque solo DETECTA la tarjeta y el driver se
 * enciende a mano con «ifconfig eth0 experimental».
 *
 * Escrito para el SaguiOS a partir del driver alc(4) de FreeBSD (Pyun
 * YongHyeon, licencia BSD de 2 cláusulas): de allí vienen los registros, el
 * formato de los descriptores, el orden de la inicialización y los valores
 * «mágicos» del fabricante para el PHY (kernel/terceros/bsd/alc/alc_regs.h).
 * La lógica pura está en alc_logica.h, probada en el anfitrión (h165).
 *
 * Cómo funciona (el mismo dibujo que el e1000):
 *   - envío: anillo de TPD de 16 bytes (longitud, indicadores con EOP,
 *     dirección de 64 bits); el driver avanza el productor en
 *     MBOX_TD_PROD_IDX y la tarjeta publica su consumidor en MBOX_TD_CONS_IDX;
 *   - recepción: dos anillos. En el de RFD (8 bytes, solo la dirección) el
 *     driver deja búferes de 1536 bytes y avanza MBOX_RD0_PROD_IDX; la
 *     tarjeta escribe cada trama recibida en un RRD (16 bytes) del anillo de
 *     retorno, con el bit VALID, el índice del búfer y la longitud con CRC;
 *   - el PHY (Atheros F1/F2) se habla por MDIO (registro MDIO del MAC). La
 *     velocidad y el dúplex se leen cada segundo en el registro de estado
 *     propio del PHY y, al cambiar, se reprograma el MAC (alc_miibus_statchg);
 *   - interrupciones: INTR_STATUS se borra escribiendo los bits; sin IRQ
 *     usable, la tarea de la red consulta el anillo a cada tic.
 *
 * Cuidado con el hardware real: ASPM L0s/L1 apagados (en el MAC y en el
 * «Link Control» de PCIe), DMA solo con direcciones físicas por debajo de
 * 4 GB (el SaguiOS no da otras, D008), maestro de bus encendido, sin
 * «relaxed ordering» ni «no snoop», esperas medidas con el TSC (no por
 * vueltas) y todo lo útil para diagnosticar en el klog ([ALC]) y en «ip». */
#include "rede.h"
#include "console.h"
#include "info_hardware.h"
#include "memoria.h"
#include "paginacao.h"
#include "portas.h"
#include "relogio.h"
#include "texto.h"
#include "alc_logica.h"

#define N_TX   64
#define N_RX   64                 /* el anillo de retorno (RRD) tiene los mismos */
#define PASO   2048               /* distancia entre búferes (se usan 1536) */
#define REGS_BYTES 0x2000         /* hasta 0x1FFF: todos los registros que se usan */
#define MAX_REINICIOS 5
#define LOG_MAX 8                 /* errores de recepción que van al klog */

/* Página de descriptores: TPD | RFD | RRD | CMB y SMB (no se usan, pero alc(4) les da dirección). */
#define DESP_TPD 0
#define DESP_RFD (DESP_TPD + N_TX * 16)
#define DESP_RRD (DESP_RFD + N_RX * 8)
#define DESP_CMB (DESP_RRD + N_RX * 16)
#define DESP_SMB (DESP_CMB + 64)

static volatile uint8_t *regs;
static uint32_t bar_fis;
static pci_disp_t pci_alc;
static const alc_modelo_t *mod;
static uint8_t revisao;
static bool ligada;

static uint32_t fis_desc, fis_rx, fis_tx;
static volatile uint32_t *tpd, *rfd, *rrd;
static uint8_t *rx_buf, *tx_buf;
static uint32_t tx_prod, tx_cons, rx_prod, rr_cons;

static alc_enlace_t enlace;
static uint32_t proximo_sondeo, ligada_em;
static bool avisou_sem_enlace;
static uint32_t pendente;         /* causas de error de DMA vistas en la IRQ: reiniciar */
static uint32_t reinicios;

/* PCIe: lo que dejó la BIOS y lo que se programa. */
static uint8_t cap_pcie, cap_pm;
static uint16_t lnkctl_bios, devctl_bios;
static uint32_t rd_rafaga, wr_rafaga;
static bool rcb128;
static uint16_t phy_id1, phy_id2;
static const char *origem_mac = "-";
static uint8_t mac_atual[6];
static uint8_t mac_bios[6];     /* la de los PAR antes de tocar nada (ifconfig borra la placa_t) */

static struct {
    uint32_t rx_bien, tx_bien, tx_cheia;
    uint32_t crc, alineacion, truncada, corta, fragmentada, tamano, fifo_llena, fuera_de_orden;
    uint32_t irq_rx, irq_tx, irq_desborde, irq_sin_rfd, irq_tx_underrun, irq_dma;
    uint32_t mdio_falhas, registrados;
    uint32_t mib_rx, mib_rx_crc, mib_rx_corta, mib_rx_fifo, mib_rx_rrs, mib_tx, mib_tx_underrun;
} diag;

static inline uint32_t le(uint32_t r) { return *(volatile uint32_t *)(regs + r); }
static inline void escreve(uint32_t r, uint32_t v) { *(volatile uint32_t *)(regs + r) = v; }
static inline uint16_t le16(uint32_t r) { return *(volatile uint16_t *)(regs + r); }
static inline void escreve16(uint32_t r, uint16_t v) { *(volatile uint16_t *)(regs + r) = v; }

/* ------------------------------------------------------------ tiempo */

static inline uint64_t tsc(void)
{
    uint32_t lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return (uint64_t)hi << 32 | lo;
}

/* Espera de verdad: con el TSC medido en el arranque; sin él, el puerto
 * 0x80 (~1 us por acceso en un PC). */
static uint32_t tsc_mhz(void) { return (hw_tsc_khz() + 999) / 1000; }   /* redondeado hacia arriba: espera de más */

static void esperar_us(uint32_t us)
{
    uint32_t mhz = tsc_mhz();
    if (!mhz) { while (us--) porta_pausa(); return; }
    uint64_t fim = tsc() + (uint64_t)mhz * us;
    while (tsc() < fim) __asm__ volatile("pause");
}

/* Espera a que (registro & mascara) == valor, como mucho «us» microsegundos
 * de reloj. Devuelve si lo consiguió; en *ultimo, lo último leído. */
static bool esperar_reg(uint32_t r, uint32_t mascara, uint32_t valor, uint32_t us, uint32_t *ultimo)
{
    uint32_t khz = tsc_mhz();
    uint64_t fim = khz ? tsc() + (uint64_t)khz * us : 0;
    uint32_t voltas = us / 5 + 1, v;
    for (;;) {
        v = le(r);
        if ((v & mascara) == valor) break;
        if (khz ? tsc() >= fim : voltas-- == 0) { if (ultimo) *ultimo = v; return false; }
        esperar_us(5);
    }
    if (ultimo) *ultimo = v;
    return true;
}

/* ------------------------------------------------------------ PHY por MDIO */

static uint16_t mdio_le(uint32_t reg)
{
    escreve(ALC_MDIO, MDIO_OP_EXECUTE | MDIO_OP_READ | MDIO_SUP_PREAMBLE | MDIO_CLK_25_4 | MDIO_REG_ADDR(reg));
    uint32_t v;
    if (!esperar_reg(ALC_MDIO, MDIO_OP_EXECUTE | MDIO_OP_BUSY, 0, 50000, &v)) {
        if (diag.mdio_falhas++ < 4) klog("[ALC] MDIO: lectura del registro %u del PHY sin respuesta (MDIO %08x)\n", reg, v);
        return 0;   /* como alc(4): 0 en los leer-modificar-escribir */
    }
    return (uint16_t)(v & MDIO_DATA_MASK);
}

static void mdio_escreve(uint32_t reg, uint16_t valor)
{
    escreve(ALC_MDIO, MDIO_OP_EXECUTE | MDIO_OP_WRITE | ((uint32_t)valor & MDIO_DATA_MASK) |
                      MDIO_SUP_PREAMBLE | MDIO_CLK_25_4 | MDIO_REG_ADDR(reg));
    uint32_t v;
    if (!esperar_reg(ALC_MDIO, MDIO_OP_EXECUTE | MDIO_OP_BUSY, 0, 50000, &v) && diag.mdio_falhas++ < 4)
        klog("[ALC] MDIO: escritura del registro %u del PHY sin respuesta (MDIO %08x)\n", reg, v);
}

/* Registros de depuración del PHY (dirección en 0x1D, dato en 0x1E). */
static uint16_t phy_dbg_le(uint16_t r) { mdio_escreve(ALC_MII_DBG_ADDR, r); return mdio_le(ALC_MII_DBG_DATA); }
static void phy_dbg_escreve(uint16_t r, uint16_t v) { mdio_escreve(ALC_MII_DBG_ADDR, r); mdio_escreve(ALC_MII_DBG_DATA, v); }

/* alc_phy_reset_813x(): reinicio del PHY por GPHY_CFG y los valores del
 * fabricante para el DSP; sin hibernación (si no, el PHY se duerme sin cable
 * y tarda en ver el enlace). */
static void phy_reiniciar(void)
{
    uint16_t m = mod->modelo;
    escreve16(ALC_GPHY_CFG, GPHY_CFG_SEL_ANA_RESET);
    (void)le16(ALC_GPHY_CFG);
    esperar_us(10 * 1000);
    escreve16(ALC_GPHY_CFG, GPHY_CFG_EXT_RESET | GPHY_CFG_SEL_ANA_RESET);
    (void)le16(ALC_GPHY_CFG);
    esperar_us(10 * 1000);

    if (m == DEVICEID_ATHEROS_AR8152_B) phy_dbg_escreve(0x000A, phy_dbg_le(0x000A) & 0xDFFF);
    if (m == DEVICEID_ATHEROS_AR8151 || m == DEVICEID_ATHEROS_AR8151_V2 ||
        m == DEVICEID_ATHEROS_AR8152_B || m == DEVICEID_ATHEROS_AR8152_B2) {
        phy_dbg_escreve(0x003B, phy_dbg_le(0x003B) & 0xFFF7);
        esperar_us(20 * 1000);
    }
    if (m == DEVICEID_ATHEROS_AR8151) phy_dbg_escreve(0x0029, 0x929D);
    if (m == DEVICEID_ATHEROS_AR8131 || m == DEVICEID_ATHEROS_AR8132 ||
        m == DEVICEID_ATHEROS_AR8151_V2 || m == DEVICEID_ATHEROS_AR8152_B2)
        phy_dbg_escreve(0x0029, 0xB6DD);

    phy_dbg_escreve(MII_ANA_CFG18, ALC_ANA_CFG18_VALOR);
    phy_dbg_escreve(MII_ANA_CFG5, ALC_ANA_CFG5_VALOR);
    phy_dbg_escreve(MII_ANA_CFG54, ALC_ANA_CFG54_VALOR);
    phy_dbg_escreve(MII_ANA_CFG4, ALC_ANA_CFG4_VALOR);
    phy_dbg_escreve(MII_ANA_CFG0, ALC_ANA_CFG0_VALOR);
    esperar_us(1000);

    phy_dbg_escreve(0x0029, phy_dbg_le(0x0029) & ~0x8000u);   /* sin hibernación */
    phy_dbg_escreve(0x000B, phy_dbg_le(0x000B) & ~0x8000u);
}

/* atphy_reset() + atphy_setmedia(): cruce automático, sin apagado,
 * anunciar 10/100 (y 1000 en los modelos gigabit) y autonegociar. */
static void phy_autonegociar(void)
{
    bool rapida = (mod->flags & ALC_F_RAPIDA) != 0;
    phy_dbg_escreve(0x29, 0);   /* fuera del modo de bajo consumo */
    uint16_t scr = mdio_le(ATPHY_SCR);
    scr |= ATPHY_SCR_AUTO_X_MODE | ATPHY_SCR_ASSERT_CRS_ON_TX | ATPHY_SCR_POLARITY_REVERSAL;
    scr &= (uint16_t)~ATPHY_SCR_MAC_PDOWN;
    mdio_escreve(ATPHY_SCR, scr);
    mdio_escreve(ALC_MII_ANAR, alc_anar());
    /* atphy(4): en un PHY rápido solo se toca el registro 9 si es el modelo F1
     * (los AR8132 dicen ser el F1 gigabit y hay que quitarle el 1000). */
    if (!rapida || ((phy_id2 >> 4) & 0x3F) == 1) mdio_escreve(ALC_MII_GTCR, alc_gtcr(rapida));
    mdio_escreve(ALC_MII_BMCR, BMCR_RESET | BMCR_AUTOEN | BMCR_STARTNEG);
    for (int i = 0; i < 100; i++) {           /* hasta 100 ms */
        esperar_us(1000);
        if (!(mdio_le(ALC_MII_BMCR) & BMCR_RESET)) break;
    }
    enlace.enlace = false;
    enlace.velocidade = 0;
    proximo_sondeo = relogio_tiques() + TIQUES_POR_SEGUNDO / 2;
}

/* ------------------------------------------------------------ PCI y PCIe */

static uint8_t cap_procurar(const pci_disp_t *d, uint8_t id)
{
    if (!(pci_le32(d, 0x04) & (1u << 20))) return 0;   /* sin lista de capacidades */
    uint8_t c = (uint8_t)(pci_le32(d, 0x34) & 0xFC);
    for (int voltas = 0; c && voltas < 48; voltas++) {
        uint32_t cab = pci_le32(d, c);
        if ((cab & 0xFF) == id) return c;
        c = (uint8_t)((cab >> 8) & 0xFC);
    }
    return 0;
}

/* Lee/escribe los 16 bits bajos de un registro de 32 de la capacidad PCIe.
 * Los 16 altos de DevCtl/LnkCtl son registros de estado con bits RW1C: se
 * escriben a 0 para no borrarlos. */
static uint16_t pcie_le16(uint8_t desp) { return (uint16_t)pci_le32(&pci_alc, (uint8_t)(cap_pcie + desp)); }
static void pcie_escreve16(uint8_t desp, uint16_t v) { pci_escreve32(&pci_alc, (uint8_t)(cap_pcie + desp), v); }

/* D3 -> D0 si la BIOS dejó la tarjeta dormida (al despertar puede perder los BAR). */
static int despertar(const pci_disp_t *d)
{
    if (!cap_pm) return 0;
    uint32_t pmcsr = pci_le32(d, (uint8_t)(cap_pm + 4));
    int estado = (int)(pmcsr & 3);
    if (estado == 0) return 0;
    uint32_t bar0 = pci_le32(d, 0x10), bar1 = pci_le32(d, 0x14), cmd = pci_le32(d, 0x04);
    pci_escreve32(d, (uint8_t)(cap_pm + 4), pmcsr & ~(0x8003u));   /* D0, sin tocar el PME_Status (RW1C) */
    esperar_us(10 * 1000);                                            /* D3hot -> D0: 10 ms (PCI PM 1.2) */
    if (pci_le32(d, 0x10) != bar0) { pci_escreve32(d, 0x10, bar0); pci_escreve32(d, 0x14, bar1); }
    pci_escreve32(d, 0x04, cmd & 0xFFFF);
    return estado;
}

/* alc_init_pcie() para las AR813x/AR815x, más el ASPM apagado también en el
 * «Link Control» (D200): con él encendido estas tarjetas pierden la recepción. */
static void iniciar_pcie(void)
{
    escreve(ALC_PEX_UNC_ERR_SEV, le(ALC_PEX_UNC_ERR_SEV) & ~(uint32_t)(PEX_UNC_ERR_SEV_DLP | PEX_UNC_ERR_SEV_FCP));
    escreve(ALC_LTSSM_ID_CFG, le(ALC_LTSSM_ID_CFG) & ~(uint32_t)LTSSM_ID_WRO_ENB);
    escreve(ALC_PCIE_PHYMISC, le(ALC_PCIE_PHYMISC) | PCIE_PHYMISC_FORCE_RCV_DET);
    if (alc_8152_v10(mod->modelo, revisao)) {
        uint32_t v = le(ALC_PCIE_PHYMISC2);
        v &= ~(uint32_t)(PCIE_PHYMISC2_SERDES_CDR_MASK | PCIE_PHYMISC2_SERDES_TH_MASK);
        v |= 3u << PCIE_PHYMISC2_SERDES_CDR_SHIFT | 3u << PCIE_PHYMISC2_SERDES_TH_SHIFT;
        escreve(ALC_PCIE_PHYMISC2, v);
    }
    if (!cap_pcie) return;
    uint32_t lnkcap = pci_le32(&pci_alc, (uint8_t)(cap_pcie + 0x0C));
    rcb128 = (lnkctl_bios & (1u << 3)) != 0;
    if (lnkcap & (3u << 10)) {                  /* la tarjeta sabe hacer ASPM */
        uint32_t pm = le(ALC_PM_CFG);
        escreve(ALC_PM_CFG, alc_pm_sin_aspm(pm));
        if (lnkctl_bios & 3) pcie_escreve16(0x10, (uint16_t)(lnkctl_bios & ~3u));
        klog("[ALC] ASPM: la BIOS lo dejo %s (LnkCtl %04x); apagado (PM_CFG %08x -> %08x, LnkCtl %04x)\n",
             (lnkctl_bios & 3) == 0 ? "apagado" : (lnkctl_bios & 3) == 1 ? "en L0s" : (lnkctl_bios & 3) == 2 ? "en L1" : "en L0s+L1",
             lnkctl_bios, pm, le(ALC_PM_CFG), pcie_le16(0x10));
    }
}

/* ------------------------------------------------------------ MAC */

static void parar_filas(void)
{
    uint32_t r = le(ALC_RXQ_CFG);
    if (r & RXQ_CFG_ENB) escreve(ALC_RXQ_CFG, r & ~(uint32_t)RXQ_CFG_ENB);
    r = le(ALC_TXQ_CFG);
    if (r & TXQ_CFG_ENB) escreve(ALC_TXQ_CFG, r & ~(uint32_t)TXQ_CFG_ENB);
    esperar_us(40);
    if (!esperar_reg(ALC_IDLE_STATUS, IDLE_STATUS_RXQ | IDLE_STATUS_TXQ, 0, 20000, &r))
        klog("[ALC] las colas RX/TX no se paran (IDLE_STATUS %08x)\n", r);
}

static void parar_mac(void)
{
    parar_filas();
    uint32_t r = le(ALC_MAC_CFG);
    if (r & (MAC_CFG_TX_ENB | MAC_CFG_RX_ENB)) escreve(ALC_MAC_CFG, r & ~(uint32_t)(MAC_CFG_TX_ENB | MAC_CFG_RX_ENB));
    if (!esperar_reg(ALC_IDLE_STATUS, IDLE_STATUS_RXMAC | IDLE_STATUS_TXMAC, 0, 20000, &r))
        klog("[ALC] el MAC no se para (IDLE_STATUS %08x)\n", r);
}

static void iniciar_filas(void)
{
    uint32_t r = le(ALC_RXQ_CFG);
    r &= ~(uint32_t)RXQ_CFG_ENB;
    escreve(ALC_RXQ_CFG, r | RXQ_CFG_QUEUE0_ENB);   /* un solo anillo de recepción */
    escreve(ALC_TXQ_CFG, le(ALC_TXQ_CFG) | TXQ_CFG_ENB);
}

/* alc_reset(): reinicio del MAC esperando por tiempo (no por vueltas). */
static bool reiniciar_mac(void)
{
    uint64_t t0 = tsc();
    escreve(ALC_MASTER_CFG, le(ALC_MASTER_CFG) | MASTER_OOB_DIS_OFF | MASTER_RESET);
    esperar_us(10);
    uint32_t v;
    bool ok = esperar_reg(ALC_MASTER_CFG, MASTER_RESET, 0, 50000, &v);
    if (!ok) klog("[ALC] reinicio del MAC: MASTER_CFG %08x sigue con RESET tras 50 ms\n", v);
    if (!esperar_reg(ALC_IDLE_STATUS, IDLE_STATUS_RXMAC | IDLE_STATUS_TXMAC | IDLE_STATUS_RXQ | IDLE_STATUS_TXQ, 0,
                     50000, &v)) {
        klog("[ALC] reinicio del MAC: no queda en reposo (IDLE_STATUS %08x)\n", v);
        ok = false;
    }
    if (mod->flags & ALC_F_SERDES)
        escreve(ALC_SERDES_LOCK, le(ALC_SERDES_LOCK) | SERDES_MAC_CLK_SLOWDOWN | SERDES_PHY_CLK_SLOWDOWN);
    uint32_t mhz = tsc_mhz();
    klog("[ALC] reinicio del MAC %s en %u us (MASTER_CFG %08x)\n", ok ? "hecho" : "CON FALLOS",
         mhz ? (uint32_t)(tsc() - t0) / mhz : 0, le(ALC_MASTER_CFG));
    return ok;
}

static void mac_de_par(uint8_t mac[6]) { alc_mac_de_par(le(ALC_PAR0), le(ALC_PAR1), mac); }

/* alc_get_macaddr_813x(): si hay EEPROM/OTP, que el TWSI la vuelva a cargar
 * en los PAR. Solo se usa si la BIOS no dejó una MAC válida. */
static void recarregar_mac(void)
{
    uint16_t m = mod->modelo;
    bool ar813x = m == DEVICEID_ATHEROS_AR8131 || m == DEVICEID_ATHEROS_AR8132;
    uint32_t opt = le(ALC_OPT_CFG);
    if (!(le(ALC_MASTER_CFG) & MASTER_OTP_SEL) || !(le(ALC_TWSI_DEBUG) & TWSI_DEBUG_DEV_EXIST)) {
        klog("[ALC] sin EEPROM/OTP (MASTER_CFG %08x, TWSI_DEBUG %08x)\n", le(ALC_MASTER_CFG), le(ALC_TWSI_DEBUG));
        return;
    }
    if (ar813x) {
        if (!(opt & OPT_CFG_CLK_ENB)) { escreve(ALC_OPT_CFG, opt | OPT_CFG_CLK_ENB); (void)le(ALC_OPT_CFG); esperar_us(1000); }
    } else {
        phy_dbg_escreve(0x00, phy_dbg_le(0x00) & 0xFF7F);
        phy_dbg_escreve(0x3B, phy_dbg_le(0x3B) | 0x0008);
        esperar_us(20);
    }
    escreve(ALC_LTSSM_ID_CFG, le(ALC_LTSSM_ID_CFG) & ~(uint32_t)LTSSM_ID_WRO_ENB);
    escreve(ALC_WOL_CFG, 0);
    (void)le(ALC_WOL_CFG);
    escreve(ALC_TWSI_CFG, le(ALC_TWSI_CFG) | TWSI_CFG_SW_LD_START);
    uint32_t v;
    if (!esperar_reg(ALC_TWSI_CFG, TWSI_CFG_SW_LD_START, 0, 100000, &v))
        klog("[ALC] recarga de la EEPROM sin terminar tras 100 ms (TWSI_CFG %08x)\n", v);
    if (ar813x) {
        if (!(opt & OPT_CFG_CLK_ENB)) { escreve(ALC_OPT_CFG, opt & ~(uint32_t)OPT_CFG_CLK_ENB); (void)le(ALC_OPT_CFG); esperar_us(1000); }
    } else {
        phy_dbg_escreve(0x00, phy_dbg_le(0x00) | 0x0080);
        phy_dbg_escreve(0x3B, phy_dbg_le(0x3B) & 0xFFF7);
        esperar_us(20);
    }
}

/* Lee (y así borra) los contadores del MAC, acumulando los que interesan. */
static void mib_ler(void)
{
    uint32_t v[ALC_RX_MIB_N > ALC_TX_MIB_N ? ALC_RX_MIB_N : ALC_TX_MIB_N];
    for (int i = 0; i < ALC_RX_MIB_N; i++) v[i] = le(ALC_RX_MIB_BASE + 4u * (uint32_t)i);
    diag.mib_rx += v[ALC_MIB_RX_FRAMES];
    diag.mib_rx_crc += v[ALC_MIB_RX_CRCERRS];
    diag.mib_rx_corta += v[ALC_MIB_RX_RUNTS];
    diag.mib_rx_fifo += v[ALC_MIB_RX_FIFO_OFLOWS];
    diag.mib_rx_rrs += v[ALC_MIB_RX_RRS_ERRS];
    for (int i = 0; i < ALC_TX_MIB_N; i++) v[i] = le(ALC_TX_MIB_BASE + 4u * (uint32_t)i);
    diag.mib_tx += v[ALC_MIB_TX_FRAMES];
    diag.mib_tx_underrun += v[ALC_MIB_TX_UNDERRUN];
}

/* alc_stop() + alc_init_locked() para las AR813x/AR815x: anillos y
 * registros. Las colas y el MAC se encienden al subir el enlace. */
static bool programar(void)
{
    escreve(ALC_INTR_MASK, 0);
    escreve(ALC_INTR_STATUS, 0xFFFFFFFFu);
    escreve(ALC_DMA_CFG, (le(ALC_DMA_CFG) & ~(uint32_t)(DMA_CFG_CMB_ENB | DMA_CFG_SMB_ENB)) | DMA_CFG_SMB_DIS);
    esperar_us(1000);
    parar_mac();
    escreve(ALC_INTR_STATUS, 0xFFFFFFFFu);
    bool ok = reiniciar_mac();

    /* Anillos: todos los RFD con su búfer; el productor apunta al último. */
    memset((void *)fis_desc, 0, PAGINA);
    for (uint32_t i = 0; i < N_RX; i++) { rfd[2 * i] = fis_rx + i * PASO; rfd[2 * i + 1] = 0; }
    rx_prod = N_RX - 1;
    rr_cons = tx_prod = tx_cons = 0;
    pendente = 0;
    __asm__ volatile("" ::: "memory");
    escreve(ALC_MBOX_RD0_PROD_IDX, rx_prod);

    escreve(ALC_CLK_GATING_CFG, 0);
    uint32_t par0, par1;
    alc_mac_para_par(mac_atual, &par0, &par1);
    escreve(ALC_PAR0, par0);
    escreve(ALC_PAR1, par1);
    (void)le(ALC_WOL_CFG);
    escreve(ALC_WOL_CFG, 0);                  /* sin Wake-on-LAN: estorba a la recepción */

    escreve(ALC_TX_BASE_ADDR_HI, 0);          /* todo por debajo de 4 GB */
    escreve(ALC_TDL_HEAD_ADDR_LO, fis_desc + DESP_TPD);
    escreve(ALC_TDH_HEAD_ADDR_LO, 0);         /* la cola de prioridad alta no se usa */
    escreve(ALC_TD_RING_CNT, N_TX & TD_RING_CNT_MASK);
    escreve(ALC_RX_BASE_ADDR_HI, 0);
    escreve(ALC_RD0_HEAD_ADDR_LO, fis_desc + DESP_RFD);
    escreve(ALC_RD1_HEAD_ADDR_LO, 0);
    escreve(ALC_RD2_HEAD_ADDR_LO, 0);
    escreve(ALC_RD3_HEAD_ADDR_LO, 0);
    escreve(ALC_RD_RING_CNT, N_RX & RD_RING_CNT_MASK);
    escreve(ALC_RX_BUF_SIZE, RX_BUF_SIZE_MAX);
    escreve(ALC_RRD0_HEAD_ADDR_LO, fis_desc + DESP_RRD);
    escreve(ALC_RRD1_HEAD_ADDR_LO, 0);
    escreve(ALC_RRD2_HEAD_ADDR_LO, 0);
    escreve(ALC_RRD3_HEAD_ADDR_LO, 0);
    escreve(ALC_RRD_RING_CNT, N_RX & RRD_RING_CNT_MASK);
    escreve(ALC_CMB_BASE_ADDR_LO, fis_desc + DESP_CMB);
    escreve(ALC_SMB_BASE_ADDR_HI, 0);
    escreve(ALC_SMB_BASE_ADDR_LO, fis_desc + DESP_SMB);

    if (mod->modelo == DEVICEID_ATHEROS_AR8152_B) {
        /* Reparto de la SRAM de la AR8152 v1.x (valores del fabricante). */
        escreve(ALC_SRAM_RX_FIFO_LEN, 0x000002A0);
        escreve(ALC_SRAM_TX_FIFO_LEN, 0x00000100);
        escreve(ALC_SRAM_RX_FIFO_ADDR, 0x029F0000);
        escreve(ALC_SRAM_RD0_ADDR, 0x02BF02A0);
        escreve(ALC_SRAM_TX_FIFO_ADDR, 0x03BF02C0);
        escreve(ALC_SRAM_TD_ADDR, 0x03DF03C0);
        escreve(ALC_TXF_WATER_MARK, 0);
        escreve(ALC_RD_DMA_CFG, 0);
    }
    escreve(ALC_DMA_BLOCK, DMA_BLOCK_LOAD);   /* que la tarjeta cargue las direcciones */

    /* Moderación: 100 us para la recepción, 1 ms para el envío (los de alc(4)). */
    escreve(ALC_IM_TIMER, (uint32_t)ALC_USECS(ALC_IM_RX_TIMER_DEFAULT) << IM_TIMER_RX_SHIFT |
                          (uint32_t)ALC_USECS(ALC_IM_TX_TIMER_DEFAULT) << IM_TIMER_TX_SHIFT);
    uint32_t r = le(ALC_MASTER_CFG);
    r &= ~(uint32_t)(MASTER_IM_RX_TIMER_ENB | MASTER_IM_TX_TIMER_ENB);
    escreve(ALC_MASTER_CFG, r | MASTER_SA_TIMER_ENB | MASTER_IM_RX_TIMER_ENB | MASTER_IM_TX_TIMER_ENB);
    escreve(ALC_INTR_RETRIG_TIMER, 0);
    escreve(ALC_CMB_TX_TIMER, 0);             /* sin CMB: tiene fallos en el silicio */
    escreve(ALC_SMB_STAT_TIMER, 0);
    mib_ler();
    memset(&diag.mib_rx, 0, sizeof diag - __builtin_offsetof(__typeof__(diag), mib_rx));

    escreve(ALC_FRAME_SIZE, mod->max_trama);
    escreve(ALC_HDS_CFG, 0);
    escreve(ALC_IPG_IFG_CFG, ((IPG_IFG_IPGT_DEFAULT << IPG_IFG_IPGT_SHIFT) & IPG_IFG_IPGT_MASK) |
                             ((IPG_IFG_MIFG_DEFAULT << IPG_IFG_MIFG_SHIFT) & IPG_IFG_MIFG_MASK) |
                             ((IPG_IFG_IPG1_DEFAULT << IPG_IFG_IPG1_SHIFT) & IPG_IFG_IPG1_MASK) |
                             ((IPG_IFG_IPG2_DEFAULT << IPG_IFG_IPG2_SHIFT) & IPG_IFG_IPG2_MASK));
    escreve(ALC_HDPX_CFG, ((HDPX_CFG_LCOL_DEFAULT << HDPX_CFG_LCOL_SHIFT) & HDPX_CFG_LCOL_MASK) |
                          ((HDPX_CFG_RETRY_DEFAULT << HDPX_CFG_RETRY_SHIFT) & HDPX_CFG_RETRY_MASK) |
                          HDPX_CFG_EXC_DEF_EN |
                          ((HDPX_CFG_ABEBT_DEFAULT << HDPX_CFG_ABEBT_SHIFT) & HDPX_CFG_ABEBT_MASK) |
                          ((HDPX_CFG_JAMIPG_DEFAULT << HDPX_CFG_JAMIPG_SHIFT) & HDPX_CFG_JAMIPG_MASK));
    escreve(ALC_TSO_OFFLOAD_THRESH, ((uint32_t)mod->max_trama >> TSO_OFFLOAD_THRESH_UNIT_SHIFT) & TSO_OFFLOAD_THRESH_MASK);
    escreve(ALC_TXQ_CFG, alc_txq_cfg(mod->modelo, rd_rafaga));
    escreve(ALC_RX_RD_FREE_THRESH,
            ((RX_RD_FREE_THRESH_HI_DEFAULT << RX_RD_FREE_THRESH_HI_SHIFT) & RX_RD_FREE_THRESH_HI_MASK) |
            ((RX_RD_FREE_THRESH_LO_DEFAULT << RX_RD_FREE_THRESH_LO_SHIFT) & RX_RD_FREE_THRESH_LO_MASK));
    if (mod->modelo == DEVICEID_ATHEROS_AR8131 || mod->modelo == DEVICEID_ATHEROS_AR8132) {
        uint32_t f = le(ALC_SRAM_RX_FIFO_LEN), alto = f * 8 / 10, baixo = f * 3 / 10;   /* XON 80 %, XOFF 30 % */
        escreve(ALC_RX_FIFO_PAUSE_THRESH, ((baixo << RX_FIFO_PAUSE_THRESH_LO_SHIFT) & RX_FIFO_PAUSE_THRESH_LO_MASK) |
                                          ((alto << RX_FIFO_PAUSE_THRESH_HI_SHIFT) & RX_FIFO_PAUSE_THRESH_HI_MASK));
    }
    escreve(ALC_RSS_IDT_TABLE0, 0);
    escreve(ALC_RSS_CPU, 0);
    escreve(ALC_RXQ_CFG, alc_rxq_cfg(mod->modelo, mod->flags));
    escreve(ALC_DMA_CFG, alc_dma_cfg(rcb128, rd_rafaga, wr_rafaga));
    escreve(ALC_MAC_CFG, alc_mac_cfg_inicial(mod->flags));
    escreve(ALC_MAR0, 0);                     /* sin multicast: la pila no lo usa */
    escreve(ALC_MAR1, 0);

    escreve(ALC_INTR_MASK, ALC_INTRS);
    escreve(ALC_INTR_STATUS, 0xFFFFFFFFu);
    escreve(ALC_INTR_STATUS, 0);              /* borradas y encendidas */
    return ok;
}

/* ------------------------------------------------------------ enlace */

/* alc_miibus_statchg(): cuando cambia el enlace, el MAC se para y se vuelve
 * a programar con la velocidad y el dúplex resueltos. */
static void sondar_enlace(void)
{
    uint16_t bmsr = mdio_le(ALC_MII_BMSR);
    bmsr = mdio_le(ALC_MII_BMSR);             /* el bit de enlace es «latched»: la segunda lectura vale */
    uint16_t ssr = mdio_le(ATPHY_SSR);
    uint16_t anar = mdio_le(ALC_MII_ANAR), anlpar = mdio_le(ALC_MII_ANLPAR);
    alc_enlace_t e = alc_phy_resolver(bmsr, ssr, anar, anlpar, (mod->flags & ALC_F_RAPIDA) != 0);
    mib_ler();
    if (e.enlace == enlace.enlace && e.velocidade == enlace.velocidade && e.completo == enlace.completo) {
        if (!e.enlace && !avisou_sem_enlace && relogio_tiques() - ligada_em > 10 * TIQUES_POR_SEGUNDO) {
            avisou_sem_enlace = true;
            klog("[ALC] sin enlace tras 10 s: BMSR %04x SSR %04x ANAR %04x ANLPAR %04x BMCR %04x\n", bmsr, ssr, anar,
                 anlpar, mdio_le(ALC_MII_BMCR));
        }
        return;
    }
    parar_mac();
    enlace = e;
    if (e.enlace) {
        iniciar_filas();
        uint32_t r = alc_mac_cfg_enlace(le(ALC_MAC_CFG), mod->flags, e);
        escreve(ALC_MAC_CFG, r | MAC_CFG_TX_ENB | MAC_CFG_RX_ENB);
        /* alc_aspm_813x() con L0s/L1 apagados. */
        escreve(ALC_PM_CFG, alc_pm_enlace(le(ALC_PM_CFG), mod->modelo, mod->flags, e.velocidade));
        if ((mod->flags & ALC_F_APS) && cap_pcie) {
            uint16_t lc = pcie_le16(0x10) & (uint16_t)~3u;   /* el ASPM, siempre apagado */
            lc &= (uint16_t)~(1u << 7);                       /* sin «extended sync»... */
            if (alc_8152_v10(mod->modelo, revisao)) lc |= 1u << 7;   /* ...salvo en la AR8152 v1.0 */
            pcie_escreve16(0x10, lc);
        }
        klog("[ALC] enlace: %u Mb/s %s (BMSR %04x SSR %04x ANLPAR %04x), MAC_CFG %08x PM_CFG %08x\n", e.velocidade,
             e.completo ? "full-duplex" : "half-duplex", bmsr, ssr, anlpar, le(ALC_MAC_CFG), le(ALC_PM_CFG));
    } else {
        klog("[ALC] enlace perdido (BMSR %04x SSR %04x)\n", bmsr, ssr);
    }
    avisou_sem_enlace = false;
}

/* ------------------------------------------------------------ envío y recepción */

static bool enviar(const void *quadro, uint32_t n)
{
    if (!ligada || !enlace.enlace || n > ETH_MAX) return false;
    uint32_t c = (le(ALC_MBOX_TD_CONS_IDX) & MBOX_TD_CONS_LO_IDX_MASK) >> MBOX_TD_CONS_LO_IDX_SHIFT;
    if (c < N_TX) tx_cons = c;
    if (alc_tx_ocupados(tx_prod, tx_cons, N_TX) >= N_TX - 3) { diag.tx_cheia++; return false; }
    memcpy(tx_buf + tx_prod * PASO, quadro, n);
    alc_tpd_montar(tpd + tx_prod * 4, fis_tx + tx_prod * PASO, n);
    tx_prod = alc_avanzar(tx_prod, 1, N_TX);
    __asm__ volatile("" ::: "memory");       /* datos y descriptor antes de avisar (x86 no reordena escrituras) */
    escreve(ALC_MBOX_TD_PROD_IDX, (tx_prod << MBOX_TD_PROD_LO_IDX_SHIFT) & MBOX_TD_PROD_LO_IDX_MASK);
    diag.tx_bien++;
    return true;
}

static void reiniciar_tudo(void);

static int receber(void *destino)
{
    if (!ligada) return 0;
    if (pendente) { reiniciar_tudo(); return 0; }
    if ((int32_t)(relogio_tiques() - proximo_sondeo) >= 0) {
        proximo_sondeo = relogio_tiques() + TIQUES_POR_SEGUNDO;
        sondar_enlace();
    }
    volatile uint32_t *d = rrd + rr_cons * 4;
    uint32_t status = d[3];
    if (!(status & RRD_VALID)) return 0;
    __asm__ volatile("" ::: "memory");
    uint32_t rdinfo = d[0];
    alc_rrd_info_t info = alc_rrd_decodificar(rdinfo, status, ETH_MAX);
    if (info.estado == ALC_RRD_SIN_BUFERES) {
        klog("[ALC] RRD %u sin buferes (rdinfo %08x status %08x): reiniciando\n", rr_cons, rdinfo, status);
        pendente |= 1;
        return -1;
    }
    uint32_t esperado = alc_avanzar(rx_prod, 1, N_RX);
    if (info.indice != esperado) diag.fuera_de_orden++;
    if (status & RRD_FIFO_FULL) diag.fifo_llena++;
    int r = -1;
    switch (info.estado) {
    case ALC_RRD_BIEN:
        if (info.indice < N_RX) {
            memcpy(destino, rx_buf + info.indice * PASO, info.n);
            r = (int)info.n;
            diag.rx_bien++;
        } else {
            diag.fuera_de_orden++;
        }
        break;
    case ALC_RRD_ERROR_CRC:        diag.crc++; break;
    case ALC_RRD_ERROR_ALINEACION: diag.alineacion++; break;
    case ALC_RRD_ERROR_TRUNCADA:   diag.truncada++; break;
    case ALC_RRD_ERROR_CORTA:      diag.corta++; break;
    case ALC_RRD_FRAGMENTADA:      diag.fragmentada++; break;
    default:                       diag.tamano++; break;
    }
    if (r < 0 && diag.registrados < LOG_MAX) {
        diag.registrados++;
        klog("[ALC] trama descartada (%u): RRD %u rdinfo %08x status %08x, esperado el bufer %u\n", (uint32_t)info.estado,
             rr_cons, rdinfo, status, esperado);
    }
    d[3] = 0;
    rr_cons = alc_avanzar(rr_cons, 1, N_RX);
    rx_prod = alc_avanzar(rx_prod, info.nbuf, N_RX);   /* los búferes vuelven a la tarjeta */
    __asm__ volatile("" ::: "memory");
    escreve(ALC_MBOX_RD0_PROD_IDX, rx_prod);
    return r;
}

static bool cabo(void) { return ligada && enlace.enlace; }

static bool atender(void)
{
    if (!ligada) return false;
    uint32_t s = le(ALC_INTR_STATUS);
    if (s == 0xFFFFFFFFu || !(s & ALC_INTRS)) return false;
    escreve(ALC_INTR_STATUS, s & ~(uint32_t)INTR_DIS_INT);   /* borra las causas; bit 31 a 0 = sigue encendida */
    if (s & INTR_RX_PKT) diag.irq_rx++;
    if (s & INTR_TX_PKT) diag.irq_tx++;
    if (s & INTR_RX_FIFO_OFLOW) diag.irq_desborde++;
    if (s & INTR_RD_UNDERRUN) diag.irq_sin_rfd++;
    if (s & INTR_TX_FIFO_UNDERRUN) diag.irq_tx_underrun++;
    if (s & (INTR_DMA_RD_TO_RST | INTR_DMA_WR_TO_RST | INTR_TXQ_TO_RST)) {
        diag.irq_dma++;
        pendente |= s & (INTR_DMA_RD_TO_RST | INTR_DMA_WR_TO_RST | INTR_TXQ_TO_RST);
    }
    return true;
}

/* Error de DMA o anillo roto: como alc(4), todo de nuevo (con un límite). */
static void reiniciar_tudo(void)
{
    uint32_t p = pendente;
    klog("[ALC] %s%s%s%s: IDLE_STATUS %08x DMA_CFG %08x MASTER_CFG %08x; reinicio %u de %u\n",
         p & INTR_DMA_RD_TO_RST ? "error de lectura DMA " : "", p & INTR_DMA_WR_TO_RST ? "error de escritura DMA " : "",
         p & INTR_TXQ_TO_RST ? "cola TX reiniciada " : "", p == 1 ? "anillo de recepcion roto" : "",
         le(ALC_IDLE_STATUS), le(ALC_DMA_CFG), le(ALC_MASTER_CFG), reinicios + 1, MAX_REINICIOS);
    if (++reinicios > MAX_REINICIOS) {
        klog("[ALC] demasiados reinicios: driver apagado\n");
        escreve(ALC_INTR_MASK, 0);
        parar_mac();
        ligada = false;
        return;
    }
    programar();
    phy_autonegociar();
}

/* ------------------------------------------------------------ diagnóstico ("ip") */

static void diagnostico(char *s, uint32_t max)
{
    uint32_t k = 0;
#define POE(...) do { if (k < max) { int _r = snformatar(s + k, max - k, __VA_ARGS__); k += _r > 0 ? (uint32_t)_r : 0; } } while (0)
    if (!ligada) { POE("Atheros alc: apagado (demasiados errores; ver klog [ALC])\n"); return; }
    mib_ler();
    POE("Chip: %s, PCI rev %02x, chip rev %02x, PHY %04x:%04x, MAC de %s\n", mod->nome, revisao,
        (le(ALC_MASTER_CFG) >> MASTER_CHIP_REV_SHIFT) & 0xFF, phy_id1, phy_id2, origem_mac);
    POE("Enlace: %s", enlace.enlace ? "" : "no");
    if (enlace.enlace) POE("%u Mb/s %s", enlace.velocidade, enlace.completo ? "full-duplex" : "half-duplex");
    POE("; BMSR %04x SSR %04x ANLPAR %04x\n", mdio_le(ALC_MII_BMSR), mdio_le(ATPHY_SSR), mdio_le(ALC_MII_ANLPAR));
    POE("MASTER %08x MAC_CFG %08x IDLE %08x INTR %08x/%08x PM %08x DMA %08x TXQ %08x RXQ %08x\n",
        le(ALC_MASTER_CFG), le(ALC_MAC_CFG), le(ALC_IDLE_STATUS), le(ALC_INTR_STATUS), le(ALC_INTR_MASK),
        le(ALC_PM_CFG), le(ALC_DMA_CFG), le(ALC_TXQ_CFG), le(ALC_RXQ_CFG));
    POE("PCIe: LnkCtl BIOS %04x ahora %04x, DevCtl BIOS %04x ahora %04x, ráfagas %u/%u bytes, RCB %u\n", lnkctl_bios,
        cap_pcie ? pcie_le16(0x10) : 0, devctl_bios, cap_pcie ? pcie_le16(0x08) : 0, alc_rafaga_bytes(rd_rafaga),
        alc_rafaga_bytes(wr_rafaga), rcb128 ? 128 : 64);
    POE("Anillos: TX prod %u cons %u (tarjeta %08x), RX prod %u, RRD %u (tarjeta %08x)\n", tx_prod, tx_cons,
        le(ALC_MBOX_TD_CONS_IDX), rx_prod, rr_cons, le(ALC_MBOX_RD01_CONS_IDX));
    POE("Driver: RX %u, TX %u (llena %u); descartes CRC %u, alineación %u, truncada %u, corta %u, fragmentada %u, "
        "tamaño %u, FIFO llena %u, fuera de orden %u\n", diag.rx_bien, diag.tx_bien, diag.tx_cheia, diag.crc,
        diag.alineacion, diag.truncada, diag.corta, diag.fragmentada, diag.tamano, diag.fifo_llena, diag.fuera_de_orden);
    POE("IRQ: RX %u, TX %u, desborde %u, sin RFD %u, TX underrun %u, DMA %u; reinicios %u; MDIO sin respuesta %u\n",
        diag.irq_rx, diag.irq_tx, diag.irq_desborde, diag.irq_sin_rfd, diag.irq_tx_underrun, diag.irq_dma, reinicios,
        diag.mdio_falhas);
    POE("Contadores del MAC: RX %u (CRC %u, cortas %u, FIFO %u, RRS %u), TX %u (underrun %u)\n", diag.mib_rx,
        diag.mib_rx_crc, diag.mib_rx_corta, diag.mib_rx_fifo, diag.mib_rx_rrs, diag.mib_tx, diag.mib_tx_underrun);
#undef POE
}

/* ------------------------------------------------------------ detección y arranque */

static bool ligar(placa_t *p, const pci_disp_t *d)
{
    /* 1. PCI: despierta, memoria + maestro de bus, INTx permitido. */
    int estado = despertar(d);
    uint32_t cmd = pci_le32(d, 0x04);
    pci_escreve32(d, 0x04, ((cmd | 0x6) & ~(1u << 10)) & 0xFFFF);
    if (!regs) regs = (volatile uint8_t *)janela_mapear_tipo(bar_fis, REGS_BYTES, PG_PCD | PG_PWT);
    if (!regs) { klog("[ALC] sin ventana para los registros\n"); return false; }
    uint32_t master = le(ALC_MASTER_CFG);
    if (master == 0xFFFFFFFFu) {
        klog("[ALC] la tarjeta no responde (MASTER_CFG %08x, comando PCI %04x, D%d)\n", master,
             pci_le32(d, 0x04) & 0xFFFF, estado);
        return false;
    }
    /* 2. PCIe: tamaños de ráfaga, RCB; sin «relaxed ordering» (bit 4) ni «no snoop» (bit 11). */
    if (cap_pcie) {
        devctl_bios = pcie_le16(0x08);
        lnkctl_bios = pcie_le16(0x10);
        rd_rafaga = alc_rafaga((devctl_bios >> 12) & 7);
        wr_rafaga = alc_rafaga((devctl_bios >> 5) & 7);
        if (devctl_bios & ((1u << 4) | (1u << 11))) pcie_escreve16(0x08, (uint16_t)(devctl_bios & ~((1u << 4) | (1u << 11))));
    }
    klog("[ALC] PCI: estado D%d -> D0, comando %04x -> %04x, DevCtl %04x (lectura %u, carga %u bytes), LnkCtl %04x\n",
         estado, cmd & 0xFFFF, pci_le32(d, 0x04) & 0xFFFF, devctl_bios, 128u << ((devctl_bios >> 12) & 7),
         128u << ((devctl_bios >> 5) & 7), lnkctl_bios);
    klog("[ALC] chip rev %02x (MASTER_CFG %08x), FIFO TX %u RX %u bytes, PM_CFG %08x\n",
         (master >> MASTER_CHIP_REV_SHIFT) & 0xFF, master, le(ALC_SRAM_TX_FIFO_LEN) * 8, le(ALC_SRAM_RX_FIFO_LEN) * 8,
         le(ALC_PM_CFG));

    if (!alc_mac_valida(mac_bios)) mac_de_par(mac_bios);   /* antes de cualquier reinicio */

    /* 3. PCIe del lado del MAC (ASPM apagado), PHY y MAC. */
    iniciar_pcie();
    phy_reiniciar();
    phy_id1 = mdio_le(ALC_MII_PHYID1);
    phy_id2 = mdio_le(ALC_MII_PHYID2);
    klog("[ALC] PHY id %04x:%04x (OUI %06x, modelo %u, rev %u)%s\n", phy_id1, phy_id2,
         (uint32_t)phy_id1 << 6 | phy_id2 >> 10, (phy_id2 >> 4) & 0x3F, phy_id2 & 0xF,
         (phy_id1 == 0 || phy_id1 == 0xFFFF) ? ": el PHY NO responde por MDIO" : "");
    parar_mac();
    reiniciar_mac();

    /* 4. MAC: la que dejó la BIOS; si no vale, la de después del reinicio; si
     *    tampoco, la de la EEPROM/OTP; y si nada, una local inventada. */
    if (alc_mac_valida(mac_bios)) {
        memcpy(mac_atual, mac_bios, 6);
        origem_mac = "la BIOS";
    } else {
        mac_de_par(mac_atual);
        origem_mac = "el reinicio";
        if (!alc_mac_valida(mac_atual)) {
            recarregar_mac();
            mac_de_par(mac_atual);
            origem_mac = "la EEPROM/OTP";
        }
        if (!alc_mac_valida(mac_atual)) {
            uint32_t t = (uint32_t)tsc();
            uint8_t local[6] = { 0x02, 0x53, 0x47, (uint8_t)(t >> 16), (uint8_t)(t >> 8), (uint8_t)t };
            memcpy(mac_atual, local, 6);
            origem_mac = "ninguna parte (inventada)";
        }
    }
    memcpy(p->mac, mac_atual, 6);
    klog("[ALC] MAC %02x:%02x:%02x:%02x:%02x:%02x (de %s)\n", mac_atual[0], mac_atual[1], mac_atual[2], mac_atual[3],
         mac_atual[4], mac_atual[5], origem_mac);

    /* 5. Memoria de DMA (una vez): todo debajo de MEMFIS_LIMITE, o sea de 4 GB. */
    if (!fis_desc) {
        fis_desc = memfisica_alocar();
        fis_rx = memfisica_alocar_seguidas(N_RX * PASO / PAGINA);
        fis_tx = memfisica_alocar_seguidas(N_TX * PASO / PAGINA);
        if (!fis_desc || !fis_rx || !fis_tx) { klog("[ALC] sin memoria para los anillos\n"); fis_desc = 0; return false; }
        tpd = (volatile uint32_t *)(fis_desc + DESP_TPD);
        rfd = (volatile uint32_t *)(fis_desc + DESP_RFD);
        rrd = (volatile uint32_t *)(fis_desc + DESP_RRD);
        rx_buf = (uint8_t *)fis_rx;
        tx_buf = (uint8_t *)fis_tx;
        memset(rx_buf, 0, N_RX * PASO);
    }
    bool ok = programar();
    ligada = true;
    reinicios = 0;
    ligada_em = relogio_tiques();
    phy_autonegociar();
    klog("[ALC] anillos: TPD %08x RFD %08x RRD %08x, buferes RX %08x TX %08x; MAC_CFG %08x DMA_CFG %08x TXQ %08x RXQ %08x%s\n",
         fis_desc + DESP_TPD, fis_desc + DESP_RFD, fis_desc + DESP_RRD, fis_rx, fis_tx, le(ALC_MAC_CFG), le(ALC_DMA_CFG),
         le(ALC_TXQ_CFG), le(ALC_RXQ_CFG), ok ? "" : " (el reinicio dio fallos)");
    klog("[ALC] esperando el enlace (autonegociacion)\n");

    p->enviar = enviar;
    p->receber = receber;
    p->cabo = cabo;
    p->atender = atender;
    p->diagnostico = diagnostico;
    return true;
}

bool alc_procurar(placa_t *p, bool ativar)
{
    pci_disp_t achados[4];
    int n = pci_procurar(0x02, 0x00, 0x00, achados, 4);
    pci_disp_t *d = NULL;
    for (int i = 0; i < n && !d; i++)
        if ((mod = alc_modelo(achados[i].fabricante, achados[i].modelo)) != NULL) d = &achados[i];
    if (!d) return false;
    pci_alc = *d;
    revisao = (uint8_t)pci_le32(d, 0x08);
    cap_pcie = cap_procurar(d, 0x10);
    cap_pm = cap_procurar(d, 0x01);
    p->nome = "alc";
    p->descricao = mod->nome;
    p->pci = *d;
    p->experimental = true;   /* sin probar en hardware real: solo a pedido (D200) */
    p->irq = (int)(pci_le32(d, 0x3C) & 0xFF);
    uint32_t bar = pci_le32(d, 0x10);
    if (bar & 1) { klog("[ALC] BAR0 %08x no es de memoria\n", bar); return false; }
    if ((bar & 6) == 4 && pci_le32(d, 0x14) != 0) {
        klog("[ALC] BAR0 por encima de 4 GB (%08x:%08x): no se puede usar\n", pci_le32(d, 0x14), bar);
        return false;
    }
    bar_fis = bar & ~0xFu;
    if (!bar_fis) { klog("[ALC] BAR0 sin asignar\n"); return false; }
    if (!ativar) {
        /* Solo leer la MAC que dejó la BIOS (si la tarjeta está despierta y decodifica memoria). */
        bool despierta = !cap_pm || (pci_le32(d, (uint8_t)(cap_pm + 4)) & 3) == 0;
        if (!regs && despierta && (pci_le32(d, 0x04) & 2))
            regs = (volatile uint8_t *)janela_mapear_tipo(bar_fis, REGS_BYTES, PG_PCD | PG_PWT);
        if (regs && despierta && (pci_le32(d, 0x04) & 2)) {
            mac_de_par(mac_bios);
            memcpy(p->mac, mac_bios, 6);
        }
        klog("[ALC] %02x:%02x.%x (%04x:%04x rev %02x) %s, BAR %08x, irq %d, MAC %02x:%02x:%02x:%02x:%02x:%02x "
             "(experimental, sin activar)\n", d->barramento, d->dispositivo, d->funcao, d->fabricante, d->modelo, revisao,
             mod->nome, bar_fis, p->irq, p->mac[0], p->mac[1], p->mac[2], p->mac[3], p->mac[4], p->mac[5]);
        return true;
    }
    return ligar(p, d);
}
