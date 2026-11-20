/* rtl8188eu.c - el chip del pincho Wi-Fi Realtek RTL8188EU (USB 0bda:8179), D215.
 *
 * Driver propio, escrito leyendo el urtwn/rtwn de OpenBSD (licencia ISC):
 * el orden del encendido, de la carga del firmware y del inicio de los
 * bloques MAC, BB y RF es el suyo. Los registros y las tablas de inicio del
 * RTL8188EU vienen de su r92creg.h, en kernel/terceros/bsd/rtwn/ (ver
 * docs/terceros.md). No se copió nada del rtl8xxxu de Linux (GPL).
 *
 * Cómo se habla con el chip:
 *   - registros: peticiones de control del fabricante (0x05), 1, 2 o 4 bytes;
 *   - transmisión: bulk OUT por el primer endpoint de salida, cada trama con
 *     su descriptor de 32 bytes delante (todas las colas van a ese endpoint);
 *   - recepción: bulk IN continuo (usb_rx_*), con varias tramas por
 *     transferencia, cada una con su descriptor de 24 bytes y el estado del PHY.
 * El firmware (rtl8188eufw.bin, linux-firmware) lo lee wifi.c del sistema de
 * archivos y llega aquí ya cargado en memoria.
 *
 * Todo esto lo llama solo la tarea «wifi» (wifi.c): cada acceso es una
 * transferencia USB que puede dormir. Si el pincho se retira a mitad, las
 * transferencias fallan y «presente» lo dice; nada se queda esperando. */
#include "wifi.h"
#include "console.h"
#include "memoria.h"
#include "processo.h"
#include "relogio.h"
#include "texto.h"
#include "cpu.h"

/* Lo que la cabecera de OpenBSD da por supuesto. */
#define IEEE80211_ADDR_LEN 6
#define nitems(a) ((int)(sizeof(a) / sizeof((a)[0])))
#define __packed __attribute__((packed))
#include "../terceros/bsd/rtwn/r92creg_88e.h"
#include "rtl8192cu_logica.h"   /* D409: los RTL8188CU/8192CU (la parte pura) */

/* De if_urtwn.c: páginas de las colas del 8188E. */
#define R88E_HQ_NPAGES        0
#define R88E_LQ_NPAGES        9
#define R88E_NQ_NPAGES        0
#define R88E_TXPKTBUF_COUNT   177
#define R88E_TX_PAGE_COUNT    168
#define R88E_MAX_RX_DMA_SIZE  0x2400
#define R92C_REQ_REGS         0x05

/* D409: familia C (RTL8188CU/8192CU/8188RU), de if_urtwn.c: páginas de las colas con un solo endpoint
 * de salida (ntx = 1: todo por la cola de prioridad alta). */
#define R92C_HQ_NPAGES        12
#define R92C_TXPKTBUF_COUNT   256
#define R92C_TX_PAGE_COUNT    248
#define R92C_MAX_RX_DMA_SIZE  0x2800

#define RX_TAM 16384
#define TX_MAX 2400
/* D238: colas USB. La agregación del chip junta hasta R92C_RXDMA_AGG_PG_TH
 * páginas de 128 bytes (48: 6 KB) antes de mandar, y la FIFO de recepción es
 * de R88E_MAX_RX_DMA_SIZE (9 KB): una transferencia nunca pasa de 16 KB. */
#define RX_FILA 8
#define TX_FILA 8
#define TX_BUF  4096

/* ------------------------------------------------------------ registros */

static bool vivo(r88e_t *r)
{
    return r->presente && r->d && r->d->usado && r->d->wifi;
}

/* D409: true = RTL8188CU/8192CU (todo lo que cambia respecto del 8188EU cuelga de esto). */
static bool fc(const r88e_t *r) { return r->familia == WUSB_RTL8192CU; }
static int encender_c(r88e_t *r);

static int regiao(r88e_t *r, bool ler, uint16_t end, void *buf, uint16_t n)
{
    if (!vivo(r)) return USB_ERRO;
    int e = usb_controle(r->d, ler ? 0xC0 : 0x40, R92C_REQ_REGS, end, 0, buf, n);
    if (e < 0) {
        r->erros_usb++;
        if (r->erros_usb < 8) klog("[WIFI] registro %x: erro USB %d\n", end, e);
    }
    return e;
}

static void w1(r88e_t *r, uint16_t a, uint8_t v) { regiao(r, false, a, &v, 1); }
static void w2(r88e_t *r, uint16_t a, uint16_t v) { uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) }; regiao(r, false, a, b, 2); }
static void w4(r88e_t *r, uint16_t a, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    regiao(r, false, a, b, 4);
}
static uint8_t r1(r88e_t *r, uint16_t a) { uint8_t v = 0xFF; return regiao(r, true, a, &v, 1) == 1 ? v : 0xFF; }
static uint16_t r2(r88e_t *r, uint16_t a)
{
    uint8_t b[2] = { 0xFF, 0xFF };
    regiao(r, true, a, b, 2);
    return (uint16_t)(b[0] | b[1] << 8);
}
static uint32_t r4(r88e_t *r, uint16_t a)
{
    uint8_t b[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
    regiao(r, true, a, b, 4);
    return (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24;
}
#define bb_w w4
#define bb_r r4

/* Espera de verdad (deja la CPU): para los retrasos de milisegundos de las
 * tablas del RF. Los de microsegundos ya los cubre la propia transferencia USB. */
static void esperar_ms(uint32_t ms)
{
    uint32_t tiques = (ms * TIQUES_POR_SEGUNDO + 999) / 1000;
    if (!tiques) tiques = 1;
    static const char canal = 0;
    processo_atual()->acordar_em = relogio_tiques() + tiques;
    bool i0 = int_salva_desliga();
    bloquear(&canal);
    ceder();
    int_restaura(i0);
}

/* D409: por cadena de RF (los 8192CU tienen dos; el EU, una). */
static void rf_wc(r88e_t *r, int ch, uint16_t end, uint32_t v)
{
    uint32_t a = fc(r) ? SM(R92C_LSSI_PARAM_ADDR, end) : SM(R88E_LSSI_PARAM_ADDR, end);
    bb_w(r, R92C_LSSI_PARAM(ch), a | SM(R92C_LSSI_PARAM_DATA, v));
}

static uint32_t rf_rc(r88e_t *r, int ch, uint8_t end)
{
    uint32_t reg0 = bb_r(r, R92C_HSSI_PARAM2(0)), regc = ch ? bb_r(r, R92C_HSSI_PARAM2(ch)) : reg0;
    bb_w(r, R92C_HSSI_PARAM2(0), reg0 & ~R92C_HSSI_PARAM2_READ_EDGE);
    esperar_ms(1);
    bb_w(r, R92C_HSSI_PARAM2(ch), RW(regc, R92C_HSSI_PARAM2_READ_ADDR, end) | R92C_HSSI_PARAM2_READ_EDGE);
    esperar_ms(1);
    if (fc(r)) {
        bb_w(r, R92C_HSSI_PARAM2(0), reg0 | R92C_HSSI_PARAM2_READ_EDGE);
        esperar_ms(1);
    }
    uint32_t v;
    if (bb_r(r, R92C_HSSI_PARAM1(ch)) & R92C_HSSI_PARAM1_PI) v = bb_r(r, R92C_HSPI_READBACK(ch));
    else v = bb_r(r, R92C_LSSI_READBACK(ch));
    return MS(v, R92C_LSSI_READBACK_DATA);
}

static void rf_w(r88e_t *r, uint16_t end, uint32_t v) { rf_wc(r, 0, end, v); }
static uint32_t rf_r(r88e_t *r, uint8_t end) { return rf_rc(r, 0, end); }

/* ------------------------------------------------------------ eFuse */

static uint8_t efuse_byte(r88e_t *r, uint16_t end)
{
    uint32_t reg = r4(r, R92C_EFUSE_CTRL);
    reg = RW(reg, R92C_EFUSE_CTRL_ADDR, end);
    reg &= ~R92C_EFUSE_CTRL_VALID;
    w4(r, R92C_EFUSE_CTRL, reg);
    for (int i = 0; i < 100 && vivo(r); i++) {
        reg = r4(r, R92C_EFUSE_CTRL);
        if (reg & R92C_EFUSE_CTRL_VALID) return (uint8_t)MS(reg, R92C_EFUSE_CTRL_DATA);
    }
    return 0xFF;
}

int r88e_ler_efuse(r88e_t *r, const char **motivo)
{
    *motivo = "";
    if (fc(r) && !r->c_ligado) {   /* D409: en los CU el encendido va antes de leer la eFuse (urtwn_attach) */
        int e = encender_c(r);
        if (e < 0) { *motivo = "el chip no se encendió (secuencia del RTL8192CU)"; return -1; }
        r->c_ligado = true;
    }
    r->sys_cfg = r4(r, R92C_SYS_CFG);
    if (!vivo(r) || r->sys_cfg == 0xFFFFFFFFu) { *motivo = "el pincho no responde a los registros"; return -1; }
    if (fc(r)) {
        r92c_chip_t c = r92c_tipo_chip(r->sys_cfg, r4(r, R92C_HPON_FSM));
        if (!c.ok) { *motivo = "chip de prueba de Realtek, no soportado"; return -1; }
        r->c_c2t2r = c.c2t2r; r->c_c1t2r = c.c1t2r; r->c_umc = c.umc; r->c_acut = c.a_cut;
        r->c_ntx = c.ntx; r->c_nrx = c.nrx;
    } else {
        /* El 8188ETV comparte el ID con el EU: lo distingue el registro de versión
         * (bits 15..12, «chip version»; el ETV es la versión de prueba sin
         * corte comercial, según el fabricante). Solo se anota. */
        r->etv = (r->sys_cfg & R92C_SYS_CFG_TRP_VAUX_EN) != 0;
        w1(r, R92C_EFUSE_ACCESS, R92C_EFUSE_ACCESS_ON);
    }
    uint16_t reg = r2(r, R92C_SYS_ISO_CTRL);
    if (!(reg & R92C_SYS_ISO_CTRL_PWC_EV12V)) w2(r, R92C_SYS_ISO_CTRL, reg | R92C_SYS_ISO_CTRL_PWC_EV12V);
    reg = r2(r, R92C_SYS_FUNC_EN);
    if (!(reg & R92C_SYS_FUNC_EN_ELDR)) w2(r, R92C_SYS_FUNC_EN, reg | R92C_SYS_FUNC_EN_ELDR);
    reg = r2(r, R92C_SYS_CLKR);
    if ((reg & (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M)) != (R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M))
        w2(r, R92C_SYS_CLKR, reg | R92C_SYS_CLKR_LOADER_EN | R92C_SYS_CLKR_ANA8M);

    /* Formato de la eFuse: cabecera (1 o 2 bytes, extendida en el 88E) con el
     * desplazamiento del bloque de 8 bytes y una máscara de qué palabras faltan. */
    memset(r->rom, 0xFF, sizeof r->rom);
    uint16_t end = 0;
    while (end < (fc(r) ? 512 : 256) && vivo(r)) {
        uint8_t h = efuse_byte(r, end);
        if (h == 0xFF) break;
        end++;
        uint8_t off;
        if (!fc(r) && (h & 0x1F) == 0x0F) {   /* cabecera extendida: solo el 88E */
            uint8_t tmp = (uint8_t)((h & 0xE0) >> 5);
            h = efuse_byte(r, end);
            end++;
            if ((h & 0x0F) == 0x0F) continue;
            off = (uint8_t)(((h & 0xF0) >> 1) | tmp);
        } else off = (uint8_t)(h >> 4);
        uint8_t msk = h & 0x0F;
        for (int i = 0; i < 4; i++) {
            if (msk & (1 << i)) continue;
            uint32_t k = (uint32_t)off * 8 + (uint32_t)i * 2;
            uint8_t a = efuse_byte(r, end++), b = efuse_byte(r, end++);
            if (k + 1 < sizeof r->rom) { r->rom[k] = a; r->rom[k + 1] = b; }
        }
    }
    if (fc(r)) r->c_pa = efuse_byte(r, 0x1fa);   /* «raro, pero es lo que hace el driver del fabricante» (urtwn) */
    else w1(r, R92C_EFUSE_ACCESS, R92C_EFUSE_ACCESS_OFF);
    if (!vivo(r)) { *motivo = "el pincho se retiró al leer la eFuse"; return -1; }
    if (fc(r)) {
        r92c_efuse_t e = r92c_leer_rom(r->rom);
        if (!e.valida) { *motivo = "la eFuse no tiene una MAC válida"; return -1; }
        memcpy(r->mac, e.mac, 6);
        r->c_placa = e.placa;
        r->c_regulacao = e.regulacion;
        return 0;
    }
    const struct r88e_rom *rom = (const struct r88e_rom *)r->rom;
    memcpy(r->mac, rom->r88eu_rom.macaddr, 6);
    r->xtal = rom->xtal;
    bool vazia = true;
    for (int i = 0; i < 6; i++) if (r->mac[i] != 0xFF && r->mac[i] != 0) vazia = false;
    if (vazia || (r->mac[0] & 1)) { *motivo = "la eFuse no tiene una MAC válida"; return -1; }
    return 0;
}

/* ------------------------------------------------------------ encendido */

/* D409: encendido de los RTL8188CU/8192CU (urtwn_r92c_power_on). */
static int encender_c(r88e_t *r)
{
    int i;
    for (i = 0; i < 1000 && vivo(r); i++) if (r1(r, R92C_APS_FSMCO) & R92C_APS_FSMCO_PFM_ALDN) break;
    if (i == 1000) return -1;
    w1(r, R92C_RSV_CTRL, 0);
    w1(r, R92C_SPS0_CTRL, 0x2b);
    esperar_ms(1);
    uint8_t reg = r1(r, R92C_LDOV12D_CTRL);
    if (!(reg & R92C_LDOV12D_CTRL_LDV12_EN)) {
        w1(r, R92C_LDOV12D_CTRL, reg | R92C_LDOV12D_CTRL_LDV12_EN);
        esperar_ms(1);
        w1(r, R92C_SYS_ISO_CTRL, r1(r, R92C_SYS_ISO_CTRL) & ~R92C_SYS_ISO_CTRL_MD2PP);
    }
    w2(r, R92C_APS_FSMCO, r2(r, R92C_APS_FSMCO) | R92C_APS_FSMCO_APFM_ONMAC);
    for (i = 0; i < 1000 && vivo(r); i++) if (!(r2(r, R92C_APS_FSMCO) & R92C_APS_FSMCO_APFM_ONMAC)) break;
    if (i == 1000) return -2;
    w2(r, R92C_APS_FSMCO, R92C_APS_FSMCO_AFSM_HSUS | R92C_APS_FSMCO_PDN_EN | R92C_APS_FSMCO_PFM_ALDN);
    w2(r, R92C_SYS_ISO_CTRL, r2(r, R92C_SYS_ISO_CTRL) & ~R92C_SYS_ISO_CTRL_DIOR);
    w1(r, R92C_APSD_CTRL, r1(r, R92C_APSD_CTRL) & ~R92C_APSD_CTRL_OFF);
    for (i = 0; i < 1000 && vivo(r); i++) if (!(r1(r, R92C_APSD_CTRL) & R92C_APSD_CTRL_OFF_STATUS)) break;
    if (i == 1000) return -3;
    uint16_t cr = r2(r, R92C_CR);
    cr |= R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN | R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN | R92C_CR_PROTOCOL_EN |
          R92C_CR_SCHEDULE_EN | R92C_CR_MACTXEN | R92C_CR_MACRXEN | R92C_CR_ENSEC;
    w2(r, R92C_CR, cr);
    w1(r, 0xfe10, 0x19);
    return vivo(r) ? 0 : -4;
}

static int encender(r88e_t *r)
{
    int i;
    if (fc(r)) {
        if (r->c_ligado) return 0;
        int e = encender_c(r);
        r->c_ligado = e == 0;
        return e;
    }
    for (i = 0; i < 500 && vivo(r); i++) {
        if (r4(r, R92C_APS_FSMCO) & R92C_APS_FSMCO_SUS_HOST) break;
    }
    if (i == 500) return -1;
    w1(r, R92C_SYS_FUNC_EN, r1(r, R92C_SYS_FUNC_EN) & ~(R92C_SYS_FUNC_EN_BBRSTB | R92C_SYS_FUNC_EN_BB_GLB_RST));
    w1(r, R92C_AFE_XTAL_CTRL + 2, r1(r, R92C_AFE_XTAL_CTRL + 2) | 0x80);
    w2(r, R92C_APS_FSMCO, r2(r, R92C_APS_FSMCO) & ~R92C_APS_FSMCO_APDM_HPDN);
    w2(r, R92C_APS_FSMCO, r2(r, R92C_APS_FSMCO) & ~(R92C_APS_FSMCO_AFSM_HSUS | R92C_APS_FSMCO_AFSM_PCIE));
    w2(r, R92C_APS_FSMCO, r2(r, R92C_APS_FSMCO) | R92C_APS_FSMCO_APFM_ONMAC);
    for (i = 0; i < 500 && vivo(r); i++) {
        if (!(r2(r, R92C_APS_FSMCO) & R92C_APS_FSMCO_APFM_ONMAC)) break;
    }
    if (i == 500) return -2;
    w1(r, R92C_LPLDO_CTRL, r1(r, R92C_LPLDO_CTRL) & ~0x10);
    w2(r, R92C_CR, 0);
    uint16_t reg = r2(r, R92C_CR);
    reg |= R92C_CR_HCI_TXDMA_EN | R92C_CR_HCI_RXDMA_EN | R92C_CR_TXDMA_EN | R92C_CR_RXDMA_EN |
           R92C_CR_PROTOCOL_EN | R92C_CR_SCHEDULE_EN | R92C_CR_ENSEC | R92C_CR_CALTMR_EN;
    w2(r, R92C_CR, reg);
    return vivo(r) ? 0 : -3;
}

static int llt_escrever(r88e_t *r, uint32_t end, uint32_t dado)
{
    w4(r, R92C_LLT_INIT, SM(R92C_LLT_INIT_OP, R92C_LLT_INIT_OP_WRITE) | SM(R92C_LLT_INIT_ADDR, end) |
                         SM(R92C_LLT_INIT_DATA, dado));
    for (int i = 0; i < 20 && vivo(r); i++)
        if (MS(r4(r, R92C_LLT_INIT), R92C_LLT_INIT_OP) == R92C_LLT_INIT_OP_NO_ACTIVE) return 0;
    return -1;
}

/* Las colas de transmisión van todas al primer endpoint de salida (ntx = 1
 * en el urtwn): la de prioridad alta, con las páginas públicas. */
static int dma_iniciar(r88e_t *r)
{
    bool c = fc(r);
    int paginas = c ? R92C_TX_PAGE_COUNT : R88E_TX_PAGE_COUNT, limite = paginas + 1, i;
    int total = c ? R92C_TXPKTBUF_COUNT : R88E_TXPKTBUF_COUNT, hq = c ? R92C_HQ_NPAGES : R88E_HQ_NPAGES;
    for (i = 0; i < paginas; i++) if (llt_escrever(r, (uint32_t)i, (uint32_t)i + 1) < 0) return -1;
    if (llt_escrever(r, (uint32_t)i, 0xFF) < 0) return -1;
    for (++i; i < total - 1; i++) if (llt_escrever(r, (uint32_t)i, (uint32_t)i + 1) < 0) return -1;
    if (llt_escrever(r, (uint32_t)i, (uint32_t)paginas + 1) < 0) return -1;
    paginas -= hq;
    w1(r, R92C_RQPN_NPQ, 0);
    w4(r, R92C_RQPN, SM(R92C_RQPN_PUBQ, paginas) | SM(R92C_RQPN_HPQ, hq) | SM(R92C_RQPN_LPQ, 0) | R92C_RQPN_LD);
    w1(r, R92C_TXPKTBUF_BCNQ_BDNY, (uint8_t)limite);
    w1(r, R92C_TXPKTBUF_MGQ_BDNY, (uint8_t)limite);
    w1(r, R92C_TXPKTBUF_WMAC_LBK_BF_HD, (uint8_t)limite);
    w1(r, R92C_TRXFF_BNDY, (uint8_t)limite);
    w1(r, R92C_TDECTRL + 1, (uint8_t)limite);
    uint16_t reg = r2(r, R92C_TRXDMA_CTRL);
    reg &= ~R92C_TRXDMA_CTRL_QMAP_M;
    reg |= R92C_TRXDMA_CTRL_QMAP_HQ;
    w2(r, R92C_TRXDMA_CTRL, reg);
    w2(r, R92C_TRXFF_BNDY + 2, (c ? R92C_MAX_RX_DMA_SIZE : R88E_MAX_RX_DMA_SIZE) - 1);
    w1(r, R92C_PBP, SM(R92C_PBP_PSRX, R92C_PBP_128) | SM(R92C_PBP_PSTX, R92C_PBP_128));
    return vivo(r) ? 0 : -1;
}

/* EDCA de la 802.11g (la tabla por omisión de la norma): AIFS = AIFSN *
 * ranura + SIFS. BE, BK, VI, VO. */
static void edca_iniciar(r88e_t *r)
{
    w2(r, R92C_SPEC_SIFS, 0x100a);
    w2(r, R92C_MAC_SPEC_SIFS, 0x100a);
    w2(r, R92C_SIFS_CCK, 0x100a);
    w2(r, R92C_SIFS_OFDM, 0x100a);
    w2(r, R92C_RESP_SIFS_CCK, 0x100a);
    w2(r, R92C_RESP_SIFS_OFDM, 0x100a);
    static const struct { uint16_t reg; uint8_t aifsn, ecwmin, ecwmax; uint16_t txop; } ac[4] = {
        { R92C_EDCA_BE_PARAM, 3, 4, 10, 0 }, { R92C_EDCA_BK_PARAM, 7, 4, 10, 0 },
        { R92C_EDCA_VI_PARAM, 2, 3, 4, 94 }, { R92C_EDCA_VO_PARAM, 2, 2, 3, 47 },
    };
    for (int i = 0; i < 4; i++)
        w4(r, ac[i].reg, SM(R92C_EDCA_PARAM_TXOP, ac[i].txop) | SM(R92C_EDCA_PARAM_ECWMIN, ac[i].ecwmin) |
                         SM(R92C_EDCA_PARAM_ECWMAX, ac[i].ecwmax) | SM(R92C_EDCA_PARAM_AIFS, ac[i].aifsn * 9 + 10));
    w1(r, R92C_ACMHWCTRL, R92C_ACMHW_HWEN);
    w4(r, R92C_EDCA_RANDOM_GEN, relogio_tiques() * 2654435761u);
}

/* Agregación de recepción por DMA (la que usa el fabricante para el 88E con
 * USB 2.0, UsbRxAggMode = DMA): el chip junta tramas hasta 48 páginas de 128
 * bytes o 4 unidades de tiempo y las manda en una sola transferencia, cada una
 * alineada a 128 bytes (wn_rx_desglosar). La agregación «USB» (AGG_EN de
 * USB_SPECIAL_OPTION) queda apagada, como en el rtl8xxxu de Linux. */
static void agregacao_iniciar(r88e_t *r)
{
    uint32_t reg = r4(r, R92C_TDECTRL);
    reg = RW(reg, R92C_TDECTRL_BLK_DESC_NUM, fc(r) ? 6 : 1);
    w4(r, R92C_TDECTRL, reg);
    w1(r, R92C_TRXDMA_CTRL, r1(r, R92C_TRXDMA_CTRL) | R92C_TRXDMA_CTRL_RXDMA_AGG_EN);
    w1(r, R92C_RXDMA_AGG_PG_TH, 48);
    if (fc(r)) w1(r, R92C_USB_DMA_AGG_TO, 4);   /* urtwn: en los CU el plazo va en otro registro */
    else {
        w1(r, R92C_RXDMA_AGG_PG_TH + 1, 4);
        w1(r, R92C_USB_SPECIAL_OPTION, r1(r, R92C_USB_SPECIAL_OPTION) & ~R92C_USB_SPECIAL_OPTION_AGG_EN);
    }
    w4(r, R92C_TXDMA_OFFSET_CHK, r4(r, R92C_TXDMA_OFFSET_CHK) | R92C_TXDMA_OFFSET_CHK_DROP_DATA_EN);
}

/* ------------------------------------------------------------ firmware */

static void fw_reiniciar(r88e_t *r)
{
    if (fc(r)) {   /* urtwn: rtwn_r92c_fw_reset */
        w1(r, R92C_HMETFR + 3, 0x20);
        uint16_t reg = 0;
        int i;
        for (i = 0; i < 100 && vivo(r); i++) {
            reg = r2(r, R92C_SYS_FUNC_EN);
            if (!(reg & R92C_SYS_FUNC_EN_CPUEN)) return;
            esperar_ms(1);
        }
        w2(r, R92C_SYS_FUNC_EN, reg & ~R92C_SYS_FUNC_EN_CPUEN);
        return;
    }
    w1(r, R92C_RSV_CTRL, r1(r, R92C_RSV_CTRL) & ~R92C_RSV_CTRL_WLOCK_00);
    w2(r, R92C_RSV_CTRL, r2(r, R92C_RSV_CTRL) & ~R88E_RSV_CTRL_MCU_RST);
    w2(r, R92C_SYS_FUNC_EN, r2(r, R92C_SYS_FUNC_EN) & ~R92C_SYS_FUNC_EN_CPUEN);
    w1(r, R92C_RSV_CTRL, r1(r, R92C_RSV_CTRL) & ~R92C_RSV_CTRL_WLOCK_00);
    w2(r, R92C_RSV_CTRL, r2(r, R92C_RSV_CTRL) | R88E_RSV_CTRL_MCU_RST);
    w2(r, R92C_SYS_FUNC_EN, r2(r, R92C_SYS_FUNC_EN) | R92C_SYS_FUNC_EN_CPUEN);
}

static int fw_pagina(r88e_t *r, int pagina, const uint8_t *p, uint32_t n)
{
    uint32_t reg = r4(r, R92C_MCUFWDL);
    w4(r, R92C_MCUFWDL, RW(reg, R92C_MCUFWDL_PAGE, (uint32_t)pagina));
    uint16_t end = R92C_FW_START_ADDR;
    while (n) {
        uint32_t m = n > 196 ? 196 : n > 4 ? 4 : 1;
        if (regiao(r, false, end, (void *)p, (uint16_t)m) < 0) return -1;
        end = (uint16_t)(end + m);
        p += m;
        n -= m;
    }
    return 0;
}

static int carregar_firmware(r88e_t *r, const uint8_t *fw, uint32_t n, const char **motivo)
{
    if (n < sizeof(struct r92c_fw_hdr) + 1024) { *motivo = "firmware demasiado corto"; return -1; }
    const struct r92c_fw_hdr *h = (const struct r92c_fw_hdr *)fw;
    if (fc(r)) {
        uint32_t saltar;
        unsigned v, sv;
        const char *m = r92c_firmware_ok(fw, n, &saltar, &v, &sv);
        if (m) { *motivo = m; return -1; }
        snformatar(r->fw_versao, sizeof r->fw_versao, "%u.%u (%02u-%02u)", v, sv, h->month, h->date);
    } else {
        if ((h->signature >> 4) != 0x88e) { *motivo = "el archivo no es un firmware del RTL8188E"; return -1; }
        snformatar(r->fw_versao, sizeof r->fw_versao, "%u.%u (%02u-%02u)", h->version, h->subversion, h->month, h->date);
    }
    fw += sizeof *h;
    n -= sizeof *h;
    if (r1(r, R92C_MCUFWDL) & R92C_MCUFWDL_RAM_DL_SEL) {
        w1(r, R92C_MCUFWDL, 0);
        fw_reiniciar(r);
    }
    w1(r, R92C_MCUFWDL, r1(r, R92C_MCUFWDL) | R92C_MCUFWDL_EN);
    w4(r, R92C_MCUFWDL, r4(r, R92C_MCUFWDL) & ~R92C_MCUFWDL_ROM_DLEN);
    w1(r, R92C_MCUFWDL, r1(r, R92C_MCUFWDL) | R92C_MCUFWDL_CHKSUM_RPT);
    for (int pagina = 0; n; pagina++) {
        uint32_t m = n > R92C_FW_PAGE_SIZE ? R92C_FW_PAGE_SIZE : n;
        if (fw_pagina(r, pagina, fw, m) < 0) { *motivo = "fallo USB al cargar el firmware"; return -1; }
        fw += m;
        n -= m;
    }
    int i;
    for (i = 0; i < 1000 && vivo(r); i++) if (r4(r, R92C_MCUFWDL) & R92C_MCUFWDL_CHKSUM_RPT) break;
    if (i == 1000 || !vivo(r)) { *motivo = "el chip no confirmó la suma del firmware"; return -1; }
    w1(r, R92C_MCUFWDL, r1(r, R92C_MCUFWDL) & ~R92C_MCUFWDL_EN);
    w1(r, R92C_MCUFWDL + 1, 0);
    uint32_t reg = r4(r, R92C_MCUFWDL);
    w4(r, R92C_MCUFWDL, (reg & ~R92C_MCUFWDL_WINTINI_RDY) | R92C_MCUFWDL_RDY);
    if (fc(r)) {   /* urtwn: en los CU, la CPU se apaga y enciende de nuevo */
        uint16_t f = r2(r, R92C_SYS_FUNC_EN);
        w2(r, R92C_SYS_FUNC_EN, f & ~R92C_SYS_FUNC_EN_CPUEN);
        w2(r, R92C_SYS_FUNC_EN, f | R92C_SYS_FUNC_EN_CPUEN);
    } else fw_reiniciar(r);
    for (i = 0; i < 1000 && vivo(r); i++) if (r4(r, R92C_MCUFWDL) & R92C_MCUFWDL_WINTINI_RDY) break;
    if (i == 1000 || !vivo(r)) { *motivo = "el firmware no arrancó"; return -1; }
    return 0;
}

/* ------------------------------------------------------------ MAC, BB y RF */

static void mac_iniciar(r88e_t *r)
{
    if (fc(r)) {
        for (int i = 0; i < nitems(rtl8192cu_mac); i++) w1(r, rtl8192cu_mac[i].reg, rtl8192cu_mac[i].val);
        return;
    }
    for (int i = 0; i < nitems(rtl8188eu_mac); i++) w1(r, rtl8188eu_mac[i].reg, rtl8188eu_mac[i].val);
    w1(r, R92C_MAX_AGGR_NUM, 0x07);
}

static void bb_iniciar(r88e_t *r)
{
    w2(r, R92C_SYS_FUNC_EN, r2(r, R92C_SYS_FUNC_EN) | R92C_SYS_FUNC_EN_BBRSTB | R92C_SYS_FUNC_EN_BB_GLB_RST |
                            R92C_SYS_FUNC_EN_DIO_RF);
    if (fc(r)) w2(r, R92C_AFE_PLL_CTRL, 0xdb83);
    w1(r, R92C_RF_CTRL, R92C_RF_CTRL_EN | R92C_RF_CTRL_RSTB | R92C_RF_CTRL_SDMRSTB);
    w1(r, R92C_SYS_FUNC_EN, R92C_SYS_FUNC_EN_USBA | R92C_SYS_FUNC_EN_USBD | R92C_SYS_FUNC_EN_BB_GLB_RST |
                            R92C_SYS_FUNC_EN_BBRSTB);
    if (fc(r)) {
        w1(r, R92C_LDOHCI12_CTRL, 0x0f);
        w1(r, 0x15, 0xe9);
        w1(r, R92C_AFE_XTAL_CTRL + 1, 0x80);
    }
    const struct r92c_bb_prog *p = &rtl8188eu_bb_prog;
    if (fc(r)) p = r->c_c2t2r ? &rtl8192cu_bb_prog : r->c_placa == R92C_BOARD_TYPE_HIGHPA ? &rtl8188ru_bb_prog
                                                                                        : &rtl8188cu_bb_prog;
    for (int i = 0; i < p->count; i++) bb_w(r, p->regs[i], p->vals[i]);
    uint32_t reg;
    if (fc(r) && r->c_c1t2r) {   /* RTL8192C capado a 1T2R */
        reg = bb_r(r, R92C_FPGA0_TXINFO);       bb_w(r, R92C_FPGA0_TXINFO, (reg & ~0x00000003u) | 0x2);
        reg = bb_r(r, R92C_FPGA1_TXINFO);       bb_w(r, R92C_FPGA1_TXINFO, (reg & ~0x00300033u) | 0x00200022);
        reg = bb_r(r, R92C_CCK0_AFESETTING);    bb_w(r, R92C_CCK0_AFESETTING, (reg & ~0xff000000u) | 0x45u << 24);
        reg = bb_r(r, R92C_OFDM0_TRXPATHENA);   bb_w(r, R92C_OFDM0_TRXPATHENA, (reg & ~0x000000ffu) | 0x23);
        reg = bb_r(r, R92C_OFDM0_AGCPARAM1);    bb_w(r, R92C_OFDM0_AGCPARAM1, (reg & ~0x00000030u) | 1 << 4);
        static const uint16_t rr[] = { 0xe74, 0xe78, 0xe7c, 0xe80, 0xe88 };
        for (int i = 0; i < 5; i++) { reg = bb_r(r, rr[i]); bb_w(r, rr[i], (reg & ~0x0c000000u) | 2u << 26); }
    }
    for (int i = 0; i < p->agccount; i++) bb_w(r, R92C_OFDM0_AGCRSSITABLE, p->agcvals[i]);
    if (!fc(r)) {
        bb_w(r, R92C_OFDM0_AGCCORE1(0), 0x69553422);
        bb_w(r, R92C_OFDM0_AGCCORE1(0), 0x69553420);
        uint8_t x = r->xtal & 0x3F;
        reg = bb_r(r, R92C_AFE_XTAL_CTRL);
        bb_w(r, R92C_AFE_XTAL_CTRL, RW(reg, R92C_AFE_XTAL_CTRL_ADDR, (uint32_t)x | (uint32_t)x << 6));
    }
    r->cck_alta_potencia = (bb_r(r, R92C_HSSI_PARAM2(0)) & R92C_HSSI_PARAM2_CCK_HIPWR) != 0;
}

static void rf_iniciar(r88e_t *r)
{
    const struct r92c_rf_prog *prog = &rtl8188eu_rf_prog[0];
    int ncad = 1;
    if (fc(r)) {
        prog = r->c_c2t2r ? rtl8192cu_rf_prog : r->c_placa == R92C_BOARD_TYPE_HIGHPA ? rtl8188ru_rf_prog : rtl8188cu_rf_prog;
        ncad = r->c_nrx;
    }
    for (int ch = 0; ch < ncad; ch++) {
        const struct r92c_rf_prog *p = &prog[ch];
        int idx = ch / 2, off = (ch % 2) * 16;
        uint32_t reg = bb_r(r, R92C_FPGA0_RFIFACESW(idx));
        uint32_t tipo = (reg >> off) & 0x10;
        bb_w(r, R92C_FPGA0_RFIFACEOE(ch), bb_r(r, R92C_FPGA0_RFIFACEOE(ch)) | 0x100000);
        bb_w(r, R92C_FPGA0_RFIFACEOE(ch), bb_r(r, R92C_FPGA0_RFIFACEOE(ch)) | 0x10);
        bb_w(r, R92C_HSSI_PARAM2(ch), bb_r(r, R92C_HSSI_PARAM2(ch)) & ~R92C_HSSI_PARAM2_ADDR_LENGTH);
        bb_w(r, R92C_HSSI_PARAM2(ch), bb_r(r, R92C_HSSI_PARAM2(ch)) & ~R92C_HSSI_PARAM2_DATA_LENGTH);
        for (int j = 0; j < p->count; j++) {
            switch (p->regs[j]) {
            case 0xfe: case 0xffe: esperar_ms(50); continue;
            case 0xfd: esperar_ms(5); continue;
            case 0xfc: esperar_ms(1); continue;
            case 0xfb: case 0xfa: case 0xf9: continue;   /* microsegundos: ya pasan en la transferencia */
            }
            rf_wc(r, ch, p->regs[j], p->vals[j]);
        }
        reg = bb_r(r, R92C_FPGA0_RFIFACESW(idx));
        reg &= ~(0x10u << off) | (tipo << off);
        bb_w(r, R92C_FPGA0_RFIFACESW(idx), reg);
        uint32_t c = rf_rc(r, ch, R92C_RF_CHNLBW);
        if (ch == 0) r->rf_chnlbw = c; else r->rf_chnlbw1 = c;
    }
    if (fc(r) && r->c_umc && r->c_acut && !r->c_c2t2r) {   /* UMC de corte A, 1T1R */
        rf_wc(r, 0, R92C_RF_RX_G1, 0x30255);
        rf_wc(r, 0, R92C_RF_RX_G2, 0x50a00);
    }
}

/* Calibración LC (la del oscilador). La IQ del urtwn no se hace todavía (D215). */
static void lc_calibrar(r88e_t *r)
{
    uint8_t modo = r1(r, R92C_OFDM1_LSTF + 3);
    uint32_t ac[2] = { 0, 0 };
    int ncad = fc(r) ? r->c_nrx : 1;
    if (modo & 0x70) {
        w1(r, R92C_OFDM1_LSTF + 3, modo & ~0x70);
        for (int i = 0; i < ncad; i++) {
            ac[i] = rf_rc(r, i, R92C_RF_AC);
            rf_wc(r, i, R92C_RF_AC, RW(ac[i], R92C_RF_AC_MODE, R92C_RF_AC_MODE_STANDBY));
        }
    } else w1(r, R92C_TXPAUSE, R92C_TXPAUSE_ALL);
    rf_w(r, R92C_RF_CHNLBW, rf_r(r, R92C_RF_CHNLBW) | R92C_RF_CHNLBW_LCSTART);
    esperar_ms(10);
    if (modo & 0x70) {
        w1(r, R92C_OFDM1_LSTF + 3, modo);
        for (int i = 0; i < ncad; i++) rf_wc(r, i, R92C_RF_AC, ac[i]);
    } else w1(r, R92C_TXPAUSE, 0x00);
}

/* Potencia por tasa del canal, con los valores de la eFuse (rtwn_r88e_get_txpower). */
/* D409: potencia de las dos cadenas de los CU (rtwn_write_txpower). */
static void potencia_c(r88e_t *r, uint8_t canal)
{
    for (int ch = 0; ch < r->c_ntx; ch++) {
        uint16_t p[R92C_NPOTENCIAS];
        r92c_potencia(r->rom, r->c_c2t2r, r->c_placa, r->c_ntx, ch, canal, p);
        uint32_t reg;
        if (ch == 0) {
            reg = bb_r(r, R92C_TXAGC_A_CCK1_MCS32);
            bb_w(r, R92C_TXAGC_A_CCK1_MCS32, RW(reg, R92C_TXAGC_A_CCK1, p[RTWN_POWER_CCK1]));
            reg = bb_r(r, R92C_TXAGC_B_CCK11_A_CCK2_11);
            reg = RW(reg, R92C_TXAGC_A_CCK2, p[RTWN_POWER_CCK2]);
            reg = RW(reg, R92C_TXAGC_A_CCK55, p[RTWN_POWER_CCK55]);
            reg = RW(reg, R92C_TXAGC_A_CCK11, p[RTWN_POWER_CCK11]);
            bb_w(r, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
        } else {
            reg = bb_r(r, R92C_TXAGC_B_CCK1_55_MCS32);
            reg = RW(reg, R92C_TXAGC_B_CCK1, p[RTWN_POWER_CCK1]);
            reg = RW(reg, R92C_TXAGC_B_CCK2, p[RTWN_POWER_CCK2]);
            reg = RW(reg, R92C_TXAGC_B_CCK55, p[RTWN_POWER_CCK55]);
            bb_w(r, R92C_TXAGC_B_CCK1_55_MCS32, reg);
            reg = bb_r(r, R92C_TXAGC_B_CCK11_A_CCK2_11);
            bb_w(r, R92C_TXAGC_B_CCK11_A_CCK2_11, RW(reg, R92C_TXAGC_B_CCK11, p[RTWN_POWER_CCK11]));
        }
        bb_w(r, R92C_TXAGC_RATE18_06(ch), SM(R92C_TXAGC_RATE06, p[RTWN_POWER_OFDM6]) | SM(R92C_TXAGC_RATE09, p[RTWN_POWER_OFDM9]) |
                                          SM(R92C_TXAGC_RATE12, p[RTWN_POWER_OFDM12]) | SM(R92C_TXAGC_RATE18, p[RTWN_POWER_OFDM18]));
        bb_w(r, R92C_TXAGC_RATE54_24(ch), SM(R92C_TXAGC_RATE24, p[RTWN_POWER_OFDM24]) | SM(R92C_TXAGC_RATE36, p[RTWN_POWER_OFDM36]) |
                                          SM(R92C_TXAGC_RATE48, p[RTWN_POWER_OFDM48]) | SM(R92C_TXAGC_RATE54, p[RTWN_POWER_OFDM54]));
        bb_w(r, R92C_TXAGC_MCS03_MCS00(ch), SM(R92C_TXAGC_MCS00, p[RTWN_POWER_MCS(0)]) | SM(R92C_TXAGC_MCS01, p[RTWN_POWER_MCS(1)]) |
                                            SM(R92C_TXAGC_MCS02, p[RTWN_POWER_MCS(2)]) | SM(R92C_TXAGC_MCS03, p[RTWN_POWER_MCS(3)]));
        bb_w(r, R92C_TXAGC_MCS07_MCS04(ch), SM(R92C_TXAGC_MCS04, p[RTWN_POWER_MCS(4)]) | SM(R92C_TXAGC_MCS05, p[RTWN_POWER_MCS(5)]) |
                                            SM(R92C_TXAGC_MCS06, p[RTWN_POWER_MCS(6)]) | SM(R92C_TXAGC_MCS07, p[RTWN_POWER_MCS(7)]));
        if (r->c_ntx > 1) {
            bb_w(r, R92C_TXAGC_MCS11_MCS08(ch), SM(R92C_TXAGC_MCS08, p[RTWN_POWER_MCS(8)]) | SM(R92C_TXAGC_MCS09, p[RTWN_POWER_MCS(9)]) |
                                                SM(R92C_TXAGC_MCS10, p[RTWN_POWER_MCS(10)]) | SM(R92C_TXAGC_MCS11, p[RTWN_POWER_MCS(11)]));
            bb_w(r, R92C_TXAGC_MCS15_MCS12(ch), SM(R92C_TXAGC_MCS12, p[RTWN_POWER_MCS(12)]) | SM(R92C_TXAGC_MCS13, p[RTWN_POWER_MCS(13)]) |
                                                SM(R92C_TXAGC_MCS14, p[RTWN_POWER_MCS(14)]) | SM(R92C_TXAGC_MCS15, p[RTWN_POWER_MCS(15)]));
        }
    }
}

static void potencia(r88e_t *r, uint8_t canal)
{
    if (fc(r)) { potencia_c(r, canal); return; }
    const struct r88e_rom *rom = (const struct r88e_rom *)r->rom;
    int grupo = canal <= 2 ? 0 : canal <= 5 ? 1 : canal <= 8 ? 2 : canal <= 11 ? 3 : canal <= 13 ? 4 : 5;
    uint16_t p[RTWN_POWER_COUNT];
    memset(p, 0, sizeof p);
    uint8_t cck = rom->txpwr.cck_tx_pwr[grupo];
    for (int i = RTWN_RIDX_CCK1; i <= RTWN_RIDX_CCK11; i++) {
        p[i] = (uint16_t)(i == RTWN_RIDX_CCK2 ? cck - 9 : cck);
        if (p[i] > R92C_MAX_TX_PWR) p[i] = R92C_MAX_TX_PWR;
    }
    uint8_t ht = grupo == 5 ? rom->txpwr.ht40_tx_pwr[4] : rom->txpwr.ht40_tx_pwr[grupo];
    int8_t dif = (int8_t)RTWN_SIGN4TO8(MS(rom->txpwr.ht20_ofdm_tx_pwr_diff, R88E_ROM_TXPWR_OFDM_DIFF));
    uint8_t ofdm = (uint8_t)(ht + dif);
    for (int i = RTWN_RIDX_OFDM6; i <= RTWN_RIDX_OFDM54; i++) p[i] = ofdm > R92C_MAX_TX_PWR ? R92C_MAX_TX_PWR : ofdm;
    dif = (int8_t)RTWN_SIGN4TO8(MS(rom->txpwr.ht20_ofdm_tx_pwr_diff, R88E_ROM_TXPWR_HT20_DIFF));
    ht = (uint8_t)(ht + dif);
    for (int i = RTWN_RIDX_MCS0; i < RTWN_RIDX_MCS8; i++) p[i] = ht > R92C_MAX_TX_PWR ? R92C_MAX_TX_PWR : ht;

    uint32_t reg = bb_r(r, R92C_TXAGC_A_CCK1_MCS32);
    bb_w(r, R92C_TXAGC_A_CCK1_MCS32, RW(reg, R92C_TXAGC_A_CCK1, p[RTWN_POWER_CCK1]));
    reg = bb_r(r, R92C_TXAGC_B_CCK11_A_CCK2_11);
    reg = RW(reg, R92C_TXAGC_A_CCK2, p[RTWN_POWER_CCK2]);
    reg = RW(reg, R92C_TXAGC_A_CCK55, p[RTWN_POWER_CCK55]);
    reg = RW(reg, R92C_TXAGC_A_CCK11, p[RTWN_POWER_CCK11]);
    bb_w(r, R92C_TXAGC_B_CCK11_A_CCK2_11, reg);
    bb_w(r, R92C_TXAGC_RATE18_06(0), SM(R92C_TXAGC_RATE06, p[RTWN_POWER_OFDM6]) | SM(R92C_TXAGC_RATE09, p[RTWN_POWER_OFDM9]) |
                                     SM(R92C_TXAGC_RATE12, p[RTWN_POWER_OFDM12]) | SM(R92C_TXAGC_RATE18, p[RTWN_POWER_OFDM18]));
    bb_w(r, R92C_TXAGC_RATE54_24(0), SM(R92C_TXAGC_RATE24, p[RTWN_POWER_OFDM24]) | SM(R92C_TXAGC_RATE36, p[RTWN_POWER_OFDM36]) |
                                     SM(R92C_TXAGC_RATE48, p[RTWN_POWER_OFDM48]) | SM(R92C_TXAGC_RATE54, p[RTWN_POWER_OFDM54]));
    bb_w(r, R92C_TXAGC_MCS03_MCS00(0), SM(R92C_TXAGC_MCS00, p[RTWN_POWER_MCS(0)]) | SM(R92C_TXAGC_MCS01, p[RTWN_POWER_MCS(1)]) |
                                       SM(R92C_TXAGC_MCS02, p[RTWN_POWER_MCS(2)]) | SM(R92C_TXAGC_MCS03, p[RTWN_POWER_MCS(3)]));
    bb_w(r, R92C_TXAGC_MCS07_MCS04(0), SM(R92C_TXAGC_MCS04, p[RTWN_POWER_MCS(4)]) | SM(R92C_TXAGC_MCS05, p[RTWN_POWER_MCS(5)]) |
                                       SM(R92C_TXAGC_MCS06, p[RTWN_POWER_MCS(6)]) | SM(R92C_TXAGC_MCS07, p[RTWN_POWER_MCS(7)]));
}

void r88e_canal(r88e_t *r, uint8_t canal)
{
    if (canal < 1 || canal > 14) return;
    potencia(r, canal);
    w1(r, R92C_BWOPMODE, r1(r, R92C_BWOPMODE) | R92C_BWOPMODE_20MHZ);
    bb_w(r, R92C_FPGA0_RFMOD, bb_r(r, R92C_FPGA0_RFMOD) & ~R92C_RFMOD_40MHZ);
    bb_w(r, R92C_FPGA1_RFMOD, bb_r(r, R92C_FPGA1_RFMOD) & ~R92C_RFMOD_40MHZ);
    if (fc(r)) {
        bb_w(r, R92C_FPGA0_ANAPARAM2, bb_r(r, R92C_FPGA0_ANAPARAM2) | R92C_FPGA0_ANAPARAM2_CBW20);
        for (int ch = 0; ch < r->c_nrx; ch++)
            rf_wc(r, ch, R92C_RF_CHNLBW, ((ch ? r->rf_chnlbw1 : r->rf_chnlbw) & ~0xFFFu) | canal | R92C_RF_CHNLBW_BW20);
    } else
    rf_w(r, R92C_RF_CHNLBW, (r->rf_chnlbw & ~0xFFFu) | canal | R88E_RF_CHNLBW_BW20);
    r->canal = canal;
}

static void led(r88e_t *r, bool aceso)
{
    if (fc(r)) {
        uint8_t v = r1(r, R92C_LEDCFG0) & 0x70;
        if (!aceso) v |= R92C_LEDCFG0_DIS;
        w1(r, R92C_LEDCFG0, v);
        return;
    }
    uint8_t reg = r1(r, R92C_LEDCFG2) & 0xF0;
    w1(r, R92C_LEDCFG2, reg | R92C_LEDCFG2_EN);
    if (!aceso) {
        reg = r1(r, R92C_LEDCFG2) & 0x90;
        w1(r, R92C_LEDCFG2, reg | R92C_LEDCFG0_DIS);
        w1(r, R92C_MAC_PINMUX_CFG, r1(r, R92C_MAC_PINMUX_CFG) & 0xFE);
    }
}

void r88e_limpar_chaves(r88e_t *r)
{
    w4(r, R92C_CAMCMD, R92C_CAMCMD_POLLING | R92C_CAMCMD_CLR);
}

static void cam(r88e_t *r, uint32_t end, uint32_t v)
{
    w4(r, R92C_CAMWRITE, v);
    w4(r, R92C_CAMCMD, R92C_CAMCMD_POLLING | R92C_CAMCMD_WRITE | SM(R92C_CAMCMD_ADDR, end));
}

void r88e_chave(r88e_t *r, int e, const uint8_t k[16], const uint8_t *mac, uint8_t keyid)
{
    static const uint8_t zero[6];
    if (!mac) mac = zero;
    for (int i = 0; i < 4; i++)
        cam(r, R92C_CAM_KEY(e, i), (uint32_t)k[4 * i] | (uint32_t)k[4 * i + 1] << 8 | (uint32_t)k[4 * i + 2] << 16 |
                                   (uint32_t)k[4 * i + 3] << 24);
    cam(r, R92C_CAM_CTL1(e), (uint32_t)mac[2] | (uint32_t)mac[3] << 8 | (uint32_t)mac[4] << 16 | (uint32_t)mac[5] << 24);
    cam(r, R92C_CAM_CTL0(e), SM(R92C_CAM_ALGO, R92C_CAM_ALGO_AES) | SM(R92C_CAM_KEYID, keyid) |
                             SM(R92C_CAM_MACLO, (uint32_t)mac[0] | (uint32_t)mac[1] << 8) | R92C_CAM_VALID);
}

static void filtros_iniciar(r88e_t *r)
{
    w4(r, R92C_RCR, R92C_RCR_AAP | R92C_RCR_APM | R92C_RCR_AM | R92C_RCR_AB | R92C_RCR_APP_ICV | R92C_RCR_AMF |
                    R92C_RCR_HTC_LOC_CTRL | R92C_RCR_APP_MIC | R92C_RCR_APP_PHYSTS);
    w4(r, R92C_MAR + 0, 0xFFFFFFFFu);
    w4(r, R92C_MAR + 4, 0xFFFFFFFFu);
    w2(r, R92C_RXFLTMAP0, 0xFFFF);
    w2(r, R92C_RXFLTMAP1, 0x0000);
    w2(r, R92C_RXFLTMAP2, 0xFFFF);
}

static void tipo_rede(r88e_t *r, uint8_t t)
{
    w1(r, R92C_MSR, (uint8_t)((r1(r, R92C_MSR) & ~R92C_MSR_NETTYPE_MASK) | t));
}

/* Recepción continua: los buffers se guardan para siempre (con el xHCI la
 * lectura armada sigue en el anillo hasta que se retire el pincho). D238:
 * primero una cola de RX_FILA lecturas armadas a la vez (el chip no espera a
 * que el sistema lea para mandar la siguiente); si no se puede, una sola. */
static bool abrir_recepcao(r88e_t *r)
{
    r->rx_fila = 0;
    if (r->usar_filas) {
        if (!r->rx_bufs) {
            /* RX_FILA buffers de 16 KB alineados a 16 KB (ninguno cruza 64 KB) */
            uint32_t b = memfisica_alocar_seguidas(RX_FILA * (RX_TAM / 4096) + 3);
            if (b) r->rx_bufs = (b + 0x3FFFu) & ~0x3FFFu;
        }
        if (r->rx_bufs) {
            r->rx_h = usb_rx_abrir_fila(r->d, r->rx_bufs, RX_TAM, RX_FILA);
            if (r->rx_h >= 0) {
                r->rx_fila = RX_FILA;
                klog("[WIFI] recepción: cola de %d lecturas de %d KB en vuelo\n", RX_FILA, RX_TAM / 1024);
                return true;
            }
        }
        klog("[WIFI] recepción: sin cola; una lectura cada vez (D215)\n");
    }
    if (!r->rx_buf) {
        uint32_t b = memfisica_alocar_seguidas(8);   /* dentro, 16 KB alineados a 16 KB: no cruzan 64 KB */
        if (!b) return false;
        r->rx_buf = (b + 0x3FFFu) & ~0x3FFFu;
    }
    r->rx_h = usb_rx_abrir(r->d, r->rx_buf, RX_TAM);
    return r->rx_h >= 0;
}

/* D238: envío sin esperar a cada trama (solo xHCI): una cola de TX_FILA
 * buffers de una página. Sin ella (EHCI o si falla), usb_bulk como siempre. */
static void abrir_envio(r88e_t *r)
{
    r->tx_fila = 0;
    r->tx_erros_seguidos = 0;
    if (!r->usar_filas || r->d->tipo_ctl != USB_XHCI) return;
    if (!r->tx_bufs) r->tx_bufs = memfisica_alocar_seguidas(TX_FILA * TX_BUF / 4096);
    if (r->tx_bufs && usb_tx_abrir_fila(r->d, r->tx_bufs, TX_BUF, TX_FILA) == 0) {
        r->tx_fila = TX_FILA;
        klog("[WIFI] envío: cola de %d tramas sin esperar a cada una\n", TX_FILA);
    }
}

void r88e_rx_simples(r88e_t *r)
{
    bool rx = r->rx_fila != 0;
    r->usar_filas = false;
    if (r->d && r->tx_fila) usb_tx_fechar_fila(r->d);
    r->tx_fila = 0;
    if (!rx) return;
    if (r->d && r->rx_h >= 0) usb_rx_fechar(r->d, r->rx_h);
    r->rx_h = -1;
    r->rx_fila = 0;
    if (!vivo(r)) return;
    if (!abrir_recepcao(r)) klog("[WIFI] no se pudo volver a una lectura cada vez\n");
}

void r88e_em_voo(r88e_t *r, uint32_t *rx, uint32_t *tx)
{
    uint32_t c, p;
    *rx = *tx = 0;
    if (!r->d || !vivo(r)) return;
    if (r->rx_fila && r->rx_h >= 0) usb_fila_info(r->d, r->rx_h, true, rx, &c, &p);
    if (r->tx_fila) {
        usb_fila_info(r->d, -1, false, tx, &c, &p);
        r->tx_perdidas = p;
    }
}

void r88e_ht(r88e_t *r, bool ht, bool sgi, uint8_t mcs_ap)
{
    r->ht = ht && mcs_ap;
    r->sgi = r->ht && sgi;
    r->mcs_ap = r->ht ? mcs_ap : 0;
    r->taxa = wn_taxa_inicial(r->ht, r->mcs_ap);
    if (fc(r)) {   /* D409: sin informes de transmisión no hay AMRR: OFDM 24 Mb/s fijos, sin HT */
        r->ht = r->sgi = false;
        r->mcs_ap = 0;
        r->taxa = RTWN_RIDX_OFDM6 + 4;
    }
    r->tx_ok = r->tx_falhas = r->tx_reintentos = 0;
}

int r88e_iniciar(r88e_t *r, const uint8_t *fw, uint32_t nfw, const char **motivo)
{
    *motivo = "";
    if (encender(r) < 0) { *motivo = "el chip no se encendió (APS_FSMCO)"; return -1; }
    if (dma_iniciar(r) < 0) { *motivo = "no se pudo preparar la tabla LLT de las colas"; return -1; }
    w1(r, R92C_RX_DRVINFO_SZ, 4);
    if (fc(r)) {
        w4(r, R92C_HISR, 0xFFFFFFFFu);
        w4(r, R92C_HIMR, 0xFFFFFFFFu);
    } else {
        w4(r, R88E_HISR, 0xFFFFFFFFu);
        w4(r, R88E_HIMR, R88E_HIMR_CPWM | R88E_HIMR_CPWM2 | R88E_HIMR_TBDER | R88E_HIMR_PSTIMEOUT);
        w4(r, R88E_HIMRE, R88E_HIMRE_RXFOVW | R88E_HIMRE_TXFOVW | R88E_HIMRE_RXERR | R88E_HIMRE_TXERR);
        w1(r, R92C_USB_SPECIAL_OPTION, r1(r, R92C_USB_SPECIAL_OPTION) | R92C_USB_SPECIAL_OPTION_INT_BULK_SEL);
    }
    for (int i = 0; i < 6; i++) w1(r, (uint16_t)(R92C_MACID + i), r->mac[i]);
    tipo_rede(r, R92C_MSR_NETTYPE_NOLINK);
    filtros_iniciar(r);
    w4(r, R92C_RRSR, RW(r4(r, R92C_RRSR), R92C_RRSR_RATE_BITMAP, R92C_RRSR_RATE_CCK_ONLY_1M));
    w2(r, R92C_RL, SM(R92C_RL_SRL, 0x30) | SM(R92C_RL_LRL, 0x30));
    edca_iniciar(r);
    if (fc(r)) {   /* reintentos automáticos de tasa de los datos y de las respuestas (USB) */
        w4(r, R92C_DARFRC + 0, 0x00000000);
        w4(r, R92C_DARFRC + 4, 0x10080404);
        w4(r, R92C_RARFRC + 0, 0x04030201);
        w4(r, R92C_RARFRC + 4, 0x08070605);
    }
    w1(r, R92C_FWHW_TXQ_CTRL, r1(r, R92C_FWHW_TXQ_CTRL) | R92C_FWHW_TXQ_CTRL_AMPDU_RTY_NEW);
    w1(r, R92C_ACKTO, 0x40);
    agregacao_iniciar(r);
    w2(r, R92C_BCN_CTRL, (R92C_BCN_CTRL_DIS_TSF_UDT0 << 8) | R92C_BCN_CTRL_DIS_TSF_UDT0);
    w2(r, R92C_TBTT_PROHIBIT, 0x6404);
    w1(r, R92C_DRVERLYINT, R92C_DRVERLYINT_INIT_TIME);
    w1(r, R92C_BCNDMATIM, R92C_BCNDMATIM_INIT_TIME);
    w2(r, R92C_BCNTCFG, 0x660f);
    if (fc(r)) {
        w4(r, R92C_AGGLEN_LMT, 0x99997631);   /* MCS7~0 */
        w1(r, R92C_AGGR_BREAK_TIME, 0x16);
        w2(r, R92C_MAX_AGGR_NUM, 0x0708);
        w1(r, R92C_BCN_MAX_ERR, 0xff);
    }
    if (!vivo(r)) { *motivo = "el pincho se retiró durante el inicio"; return -1; }
    if (carregar_firmware(r, fw, nfw, motivo) < 0) return -1;
    mac_iniciar(r);
    bb_iniciar(r);
    rf_iniciar(r);
    if (!fc(r)) w2(r, R92C_CR, r2(r, R92C_CR) | R92C_CR_MACTXEN | R92C_CR_MACRXEN);
    bb_w(r, R92C_FPGA0_RFMOD, bb_r(r, R92C_FPGA0_RFMOD) | R92C_RFMOD_CCK_EN);
    bb_w(r, R92C_FPGA0_RFMOD, bb_r(r, R92C_FPGA0_RFMOD) | R92C_RFMOD_OFDM_EN);
    r88e_limpar_chaves(r);
    w2(r, R92C_SECCFG, R92C_SECCFG_TXUCKEY_DEF | R92C_SECCFG_RXUCKEY_DEF | R92C_SECCFG_TXENC_ENA |
                       R92C_SECCFG_RXENC_ENA | R92C_SECCFG_TXBCKEY_DEF | R92C_SECCFG_RXBCKEY_DEF);
    w1(r, R92C_HWSEQ_CTRL, 0xFF);
    lc_calibrar(r);
    if (fc(r)) {   /* «arreglo de la interferencia USB» y polarización de la PA (urtwn) */
        w1(r, 0xfe40, 0xe0);
        w1(r, 0xfe41, 0x8d);
        w1(r, 0xfe42, 0x80);
        for (int ch = 0; ch < r->c_nrx; ch++) {
            if (r->c_pa & (1 << ch)) continue;
            rf_wc(r, ch, R92C_RF_IPA, 0x0f406);
            rf_wc(r, ch, R92C_RF_IPA, 0x4f406);
            rf_wc(r, ch, R92C_RF_IPA, 0x8f406);
            rf_wc(r, ch, R92C_RF_IPA, 0xcf406);
        }
        if (!(r->c_pa & 0x10)) w1(r, 0x16, (uint8_t)((r1(r, 0x16) & ~0xf0) | 0x90));
    }
    w1(r, R92C_GPIO_MUXCFG, r1(r, R92C_GPIO_MUXCFG) & ~R92C_GPIO_MUXCFG_ENBT);
    if (fc(r)) w1(r, 0x15, 0xe9);
    r88e_canal(r, 1);
    if (!fc(r)) w1(r, R88E_TX_RPT_CTRL, r1(r, R88E_TX_RPT_CTRL) | R88E_TX_RPT_CTRL_EN);
    if (!vivo(r)) { *motivo = "el pincho se retiró durante el inicio"; return -1; }

    if (!abrir_recepcao(r)) { *motivo = "no se pudo armar la recepción USB"; return -1; }
    abrir_envio(r);
    r88e_ht(r, false, false, 0);
    led(r, false);
    return 0;
}

const char *r88e_nome_firmware(const r88e_t *r)
{
    if (!fc(r)) return "rtl8188eufw.bin";
    r92c_chip_t c = { true, r->c_c2t2r, r->c_c1t2r, r->c_umc, r->c_acut, r->c_ntx, r->c_nrx };
    return r92c_nome_firmware(&c);
}

void r88e_parar(r88e_t *r)
{
    if (r->d && r->rx_h >= 0) usb_rx_fechar(r->d, r->rx_h);
    r->rx_h = -1;
    if (r->d && r->tx_fila) usb_tx_fechar_fila(r->d);
    r->tx_fila = 0;
    if (vivo(r)) {
        led(r, false);
        w2(r, R92C_RXFLTMAP2, 0);
        tipo_rede(r, R92C_MSR_NETTYPE_NOLINK);
    }
}

void r88e_modo_busca(r88e_t *r, bool busca)
{
    uint32_t reg = r4(r, R92C_RCR);
    if (busca) reg &= ~(R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN);
    w4(r, R92C_RCR, reg);
    reg = bb_r(r, R92C_OFDM0_AGCCORE1(0));
    bb_w(r, R92C_OFDM0_AGCCORE1(0), RW(reg, R92C_OFDM0_AGCCORE1_GAIN, busca ? 0x20 : 0x32));
    w1(r, R92C_TXPAUSE, busca ? (uint8_t)(R92C_TXPAUSE_AC_VO | R92C_TXPAUSE_AC_VI | R92C_TXPAUSE_AC_BE | R92C_TXPAUSE_AC_BK) : 0);
}

void r88e_associado(r88e_t *r, const uint8_t b[6], uint16_t intervalo, bool slot_curto, bool preambulo_curto)
{
    tipo_rede(r, R92C_MSR_NETTYPE_INFRA);
    w4(r, R92C_BSSID + 0, (uint32_t)b[0] | (uint32_t)b[1] << 8 | (uint32_t)b[2] << 16 | (uint32_t)b[3] << 24);
    w2(r, R92C_BSSID + 4, (uint16_t)(b[4] | b[5] << 8));
    w1(r, R92C_INIRTS_RATE_SEL, 3);
    w1(r, R92C_SLOT, slot_curto ? 9 : 20);
    uint32_t reg = r4(r, R92C_WMAC_TRXPTCL_CTL);
    if (preambulo_curto) reg |= R92C_WMAC_TRXPTCL_CTL_SHORT;
    else reg &= ~R92C_WMAC_TRXPTCL_CTL_SHORT;
    w4(r, R92C_WMAC_TRXPTCL_CTL, reg);
    w2(r, R92C_RXFLTMAP2, 0xFFFF);
    w1(r, R92C_TXPAUSE, 0x00);
    w2(r, R92C_BCN_INTERVAL, intervalo ? intervalo : 100);
    w4(r, R92C_RCR, r4(r, R92C_RCR) | R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN);
    w1(r, R92C_BCN_CTRL, r1(r, R92C_BCN_CTRL) & ~R92C_BCN_CTRL_DIS_TSF_UDT0);
    /* Tasas de respuesta: 1, 2, 5,5, 11, 6, 12 y 24 Mb/s (b/g). */
    w4(r, R92C_RRSR, RW(r4(r, R92C_RRSR), R92C_RRSR_RATE_BITMAP, 0x15F));
    led(r, true);
}

void r88e_desassociado(r88e_t *r)
{
    r88e_ht(r, false, false, 0);
    led(r, false);
    tipo_rede(r, R92C_MSR_NETTYPE_NOLINK);
    w2(r, R92C_RXFLTMAP2, 0);
    w1(r, R92C_DUAL_TSF_RST, 0x03);
    w1(r, R92C_BCN_CTRL, r1(r, R92C_BCN_CTRL) | R92C_BCN_CTRL_DIS_TSF_UDT0);
    r88e_limpar_chaves(r);
    w4(r, R92C_RCR, r4(r, R92C_RCR) & ~(R92C_RCR_CBSSID_DATA | R92C_RCR_CBSSID_BCN));
}

/* ------------------------------------------------------------ transmisión */

/* D238: la trama va a la cola de envío y se vuelve sin esperar. Si la cola
 * está llena (las 8 anteriores aún no salieron), se cede la CPU un poco (el
 * controlador vacía 8 tramas en menos de 1 ms); si sigue llena 3 tics, la trama
 * se pierde (como en una tarjeta con el anillo lleno) y TCP la repetirá. Muchos
 * errores seguidos: se cierra la cola y se vuelve a usb_bulk. */
static int enviar_fila(r88e_t *r, const uint8_t *buf, uint32_t tam)
{
    uint32_t fim = relogio_tiques() + 3;
    for (;;) {
        int e = usb_tx_fila(r->d, buf, tam);
        if (e > 0) {
            r->tx_erros_seguidos = 0;
            r->tx_transf++;
            r->tx_bytes_usb += tam;
            return 0;
        }
        if (e < 0) {
            r->erros_usb++;
            if (++r->tx_erros_seguidos >= 20) {
                klog("[WIFI] la cola de envío falla: se vuelve a esperar a cada trama (D215)\n");
                usb_tx_fechar_fila(r->d);
                r->tx_fila = 0;
            }
            return e;
        }
        r->tx_cheia++;
        if ((int32_t)(relogio_tiques() - fim) >= 0 || !vivo(r)) return USB_ERRO_TEMPO;
        ceder();
    }
}

int r88e_enviar(r88e_t *r, const uint8_t *q, uint32_t n, int tipo, bool cifrar)
{
    static uint8_t buf[TX_MAX + 40];
    if (n < 24 || n > TX_MAX || !vivo(r)) return -1;
    /* Si descriptor + trama fuesen un múltiplo exacto del paquete USB, el chip
     * esperaría más: se mete un hueco de 8 bytes (PKTOFF), como el fabricante. */
    uint32_t mps = r->d->mps_saida ? r->d->mps_saida : 512;
    uint32_t desl = 32;
    bool hueco = ((32 + n) % mps) == 0;
    if (hueco) desl += 8;
    if (fc(r)) {
        r92c_montar_txdesc(buf, n, desl, tipo == R88E_TX_GESTAO, (q[4] & 1) != 0, cifrar, hueco, tipo == R88E_TX_EAPOL ? 0 : r->taxa);
        memcpy(buf + desl, q, n);
        goto enviar;
    }
    memset(buf, 0, desl);
    uint32_t w0 = SM(R92C_TXDW0_PKTLEN, n) | SM(R92C_TXDW0_OFFSET, desl) | R92C_TXDW0_OWN | R92C_TXDW0_FSG | R92C_TXDW0_LSG;
    if (q[4] & 1) w0 |= R92C_TXDW0_BMCAST;
    uint32_t w1v = 0, w2v = 0, w4v = 0, w5v = 0;
    if (cifrar) w1v |= SM(R92C_TXDW1_CIPHER, R92C_TXDW1_CIPHER_AES);
    if (hueco) w1v |= SM(R92C_TXDW1_PKTOFF, 1);
    if (tipo == R88E_TX_GESTAO || (q[4] & 1)) {
        w1v |= SM(R88E_TXDW1_MACID, 0) | SM(R92C_TXDW1_QSEL, tipo == R88E_TX_GESTAO ? R92C_TXDW1_QSEL_MGNT : R92C_TXDW1_QSEL_BE) |
               SM(R92C_TXDW1_RAID, R92C_RAID_11B);
        w4v |= R92C_TXDW4_DRVRATE;
        w5v |= SM(R92C_TXDW5_DATARATE, 0);
    } else {
        /* D238: con HT, la tabla de tasas 11gn y, en las MCS, el GI corto si el
         * punto de acceso lo recibe. Sin A-MPDU (AGGBK): no hay acuerdo de Block Ack. */
        uint8_t taxa = tipo == R88E_TX_EAPOL ? 0 : r->taxa;
        w1v |= SM(R88E_TXDW1_MACID, R92C_MACID_BSS) | SM(R92C_TXDW1_QSEL, R92C_TXDW1_QSEL_BE) |
               SM(R92C_TXDW1_RAID, r->ht ? R92C_RAID_11GN : R92C_RAID_11BG);
        w2v |= R88E_TXDW2_AGGBK | R92C_TXDW2_CCX_RPT;
        w4v |= R92C_TXDW4_DRVRATE | SM(R92C_TXDW4_RTSRATE, 8);
        w5v |= 0x0001ff00u | SM(R92C_TXDW5_DATARATE, taxa);
        if (taxa >= RTWN_RIDX_MCS0 && r->sgi) w5v |= R92C_TXDW5_SGI;
    }
    w4v |= R92C_TXDW4_HWSEQ;
    uint32_t pal[8] = { w0, w1v, w2v, (uint32_t)R92C_TXDW3_HWSEQEN << 16, w4v, w5v, 0, 0 };
    for (int i = 0; i < 8; i++) {
        buf[4 * i] = (uint8_t)pal[i]; buf[4 * i + 1] = (uint8_t)(pal[i] >> 8);
        buf[4 * i + 2] = (uint8_t)(pal[i] >> 16); buf[4 * i + 3] = (uint8_t)(pal[i] >> 24);
    }
    uint16_t soma = 0;
    for (int i = 0; i < R92C_TXDESC_SUMSIZE / 2; i++) soma ^= (uint16_t)(buf[2 * i] | buf[2 * i + 1] << 8);
    buf[2 * R92C_TXDESC_SUMOFFSET] = (uint8_t)soma;
    buf[2 * R92C_TXDESC_SUMOFFSET + 1] = (uint8_t)(soma >> 8);
    memcpy(buf + desl, q, n);
enviar:
    if (r->tx_fila) return enviar_fila(r, buf, desl + n);
    int e = usb_bulk(r->d, false, buf, desl + n);
    if (e < 0) { r->erros_usb++; r->tx_perdidas++; return e; }
    r->tx_transf++;
    r->tx_bytes_usb += desl + n;
    return 0;
}

/* ------------------------------------------------------------ recepción */

/* Una AMRR sencilla: cada 10 informes de transmisión, sube la tasa si casi
 * todo salió a la primera y la baja si hubo muchos fallos o reintentos. D238:
 * con HT sube hasta MCS 7 (wn_taxa_subir). El RTL8188E no hace el control de
 * tasa en el firmware: el del fabricante lo hace en el driver (fw_ractrl =
 * false, Hal8188ERateAdaptive) y el rtl8xxxu de Linux también, con el informe
 * de transmisión; por eso aquí no se manda la tabla de tasas por H2C. */

static void informe_tx(r88e_t *r, const uint8_t *p)
{
    const struct r88e_tx_rpt_ccx *c = (const struct r88e_tx_rpt_ccx *)p;
    r->informes++;
    if (c->rptb1 & R88E_RPTB1_PKT_OK) r->tx_ok++;
    else r->tx_falhas++;
    if (MS(c->rptb2, R88E_RPTB2_RETRY_CNT) > 0) r->tx_reintentos++;
    uint32_t total = (uint32_t)r->tx_ok + r->tx_falhas;
    if (total < 10) return;
    if (r->tx_falhas * 3 > total || r->tx_reintentos * 3 > total) r->taxa = wn_taxa_descer(r->taxa, r->ht);
    else if (r->tx_reintentos * 10 <= total) r->taxa = wn_taxa_subir(r->taxa, r->ht, r->mcs_ap);
    r->tx_ok = r->tx_falhas = r->tx_reintentos = 0;
}

static int rssi_de(r88e_t *r, int taxa, const uint8_t *phy)
{
    static const int8_t cckoff[] = { 20, 14, 10, -4, -16, -22, -38, -40 };
    const struct r88e_rx_phystat *f = (const struct r88e_rx_phystat *)phy;
    if (taxa <= 3) {
        int rpt = (f->agc_rpt >> 5) & 7, s = (f->agc_rpt & 0x1F) << 1;
        if (r->cck_alta_potencia && rpt == 2) s -= 6;
        return (f->agc_rpt & 0x1F) > 27 ? -94 : cckoff[rpt] - s;
    }
    return ((f->sq_rpt >> 1) & 0x7F) - 110;
}

/* D238: el desglose de la transferencia (varias tramas, informes de TX dentro
 * de la agregación, el relleno SHIFT) está en kernel/lib/wifi_n.c, probado en
 * el anfitrión. Los bits que usa son los de r92creg.h: */
_Static_assert(WN_DW0_PKTLEN == R92C_RXDW0_PKTLEN_M && WN_DW0_CRCERR == R92C_RXDW0_CRCERR &&
               WN_DW0_ICVERR == R92C_RXDW0_ICVERR && WN_DW0_PHYST == R92C_RXDW0_PHYST &&
               WN_DW0_SWDEC == R92C_RXDW0_DECRYPTED && WN_DW0_INFOSZ_S == R92C_RXDW0_INFOSZ_S &&
               WN_DW0_SHIFT_S == R92C_RXDW0_SHIFT_S && WN_DW2_PKTCNT_S == R92C_RXDW2_PKTCNT_S &&
               WN_DW3_RATE == R92C_RXDW3_RATE_M && WN_DW3_HT == R92C_RXDW3_HT &&
               WN_DW3_RPT_S == R88E_RXDW3_RPT_S && WN_RPT_TX1 == R88E_RXDW3_RPT_TX1 &&
               WN_TAXA_MCS0 == RTWN_RIDX_MCS0, "wifi_n.h y r92creg.h no coinciden");

typedef struct { r88e_t *r; r88e_rx_t chegou; } rx_ctx_t;

static void rx_quadro(void *ctx, const wn_rx_t *q)
{
    rx_ctx_t *x = ctx;
    int rssi = !q->phy ? -100 : fc(x->r) ? r92c_rssi(q->taxa, q->phy, x->r->cck_alta_potencia) : rssi_de(x->r, q->taxa, q->phy);
    if ((q->quadro[0] & 0x0C) == 0x08) x->r->taxa_rx = q->taxa;   /* la de los datos (las balizas van a 1 Mb/s) */
    x->chegou(q->quadro, q->n, rssi, q->decifrada);
}

static void rx_relatorio(void *ctx, const uint8_t *p, uint32_t n)
{
    if (n >= sizeof(struct r88e_tx_rpt_ccx)) informe_tx(((rx_ctx_t *)ctx)->r, p);
}

int r88e_receber(r88e_t *r, r88e_rx_t chegou)
{
    static uint8_t buf[RX_TAM];
    if (r->rx_h < 0 && r->d && r->d->tipo_ctl == USB_EHCI) return 0;
    if (!vivo(r)) return -1;
    int len = usb_rx_ler(r->d, r->rx_h, buf, sizeof buf);
    if (len <= 0) return len;
    r->rx_transf++;
    r->rx_bytes_usb += (uint32_t)len;
    rx_ctx_t x = { r, chegou };
    r->rx.sem_informes = fc(r);   /* D409: en los CU esos bits del descriptor no son un informe de TX */
    int quadros = wn_rx_desglosar(buf, (uint32_t)len, rx_quadro, rx_relatorio, &x, &r->rx);
    return quadros ? quadros : 1;
}
