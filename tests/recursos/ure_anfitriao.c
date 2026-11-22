/* ure_anfitriao.c - prueba de la lógica del Ethernet USB Realtek (D433) en el PC (h433).
 *
 * Un chip simulado (dos mapas de 64 KB, USB y PLA, con máscaras de bytes como el
 * RTL815x) recibe las transferencias de control de ure_logica.h. Se comprueba:
 *   - identificación del chip y tabla de VID:PID;
 *   - secuencias de cada familia (8152, 8153, 8153B, 8156, 8156B): ningún error de
 *     control, registros clave escritos con el valor de la fuente, MAC y anuncio;
 *   - arranque sin respuesta: los plazos son finitos y el paso fallido se dice;
 *   - recepción agregada (tramas, alineación, errores) y bucle de ruido al azar;
 *   - trama de envío (cabecera, relleno y límites).
 * Se compila con ASan y UBSan (h433): cualquier lectura fuera de límites para el proceso.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>
#include "../../kernel/rede/ure_logica.h"

static int fallos, comprobaciones;

#define CHECK(c, ...) do { comprobaciones++; if (!(c)) { fallos++; printf("FALLO " __VA_ARGS__); printf("\n"); } else { printf("ok "); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---------------------------------------------------------- chip simulado */

typedef struct {
    uint8_t mem[2][65536];       /* [0] = USB (MCU 0x0000), [1] = PLA (MCU 0x0100) */
    uint32_t wr[2][65536];       /* escrituras por byte (para comprobar que se hicieron) */
    uint32_t transf, espera_ms, lecturas, escrituras;
    uint32_t fallar_desde;       /* >0: a partir de esta transferencia, todas fallan */
    int ultimo_m;
} chip_t;

static int sim_ctl(void *ctx, int lectura, uint16_t valor, uint16_t indice, uint8_t *buf, uint16_t n)
{
    chip_t *c = (chip_t *)ctx;
    int m = (indice & 0x0100) ? 1 : 0;
    c->transf++;
    if (c->fallar_desde && c->transf >= c->fallar_desde) return -1;
    if ((uint32_t)valor + n > 65536u) return -1;
    if (lectura) {
        memcpy(buf, &c->mem[m][valor], n);
        c->lecturas++;
        return 0;
    }
    c->escrituras++;
    uint8_t be = (uint8_t)(indice & 0xff);
    for (uint32_t j = 0; j < n; j++) {
        if (((be >> j) & 1) == 0) continue;
        c->mem[m][valor + j] = buf[j];
        c->wr[m][valor + j]++;
    }
    /* el reinicio (CR_RST) se borra solo, como en el chip */
    if (m == 1 && URE_PLA_CR >= valor && URE_PLA_CR < valor + n) c->mem[1][URE_PLA_CR] &= (uint8_t)~URE_CR_RST;
    c->ultimo_m = m;
    return 0;
}

static void sim_espera(void *ctx, uint32_t ms) { ((chip_t *)ctx)->espera_ms += ms; }

static void sim_pon16(chip_t *c, int m, uint16_t reg, uint16_t v) { c->mem[m][reg] = (uint8_t)v; c->mem[m][reg + 1] = (uint8_t)(v >> 8); }
static uint16_t sim_lee16(chip_t *c, int m, uint16_t reg) { return (uint16_t)(c->mem[m][reg] | c->mem[m][reg + 1] << 8); }
static uint32_t sim_lee32(chip_t *c, int m, uint16_t reg) { return (uint32_t)c->mem[m][reg] | (uint32_t)c->mem[m][reg + 1] << 8 | (uint32_t)c->mem[m][reg + 2] << 16 | (uint32_t)c->mem[m][reg + 3] << 24; }

static const uint8_t MAC[6] = { 0x00, 0xE0, 0x4C, 0x12, 0x34, 0x56 };   /* MAC de prueba (no real) */

/* Un chip en reposo con la versión dada: autocarga hecha, PHY en LAN_ON, OOB listo, MAC. */
static void sim_nuevo(chip_t *c, uint16_t ver)
{
    memset(c, 0, sizeof *c);
    sim_pon16(c, 1, URE_PLA_TCR1, (uint16_t)(ver & URE_VERSION_MASK));
    sim_pon16(c, 1, URE_PLA_BOOT_CTRL, URE_AUTOLOAD_DONE);
    sim_pon16(c, 1, (uint16_t)((URE_OCP_PHY_STATUS & 0x0fff) | 0xb000), URE_PHY_STAT_LAN_ON);
    c->mem[1][URE_PLA_OOB_CTRL] = URE_LINK_LIST_READY;
    sim_pon16(c, 1, 0xe000, 0x0100);
    memcpy(&c->mem[1][URE_PLA_IDR], MAC, 6);
    memcpy(&c->mem[1][URE_PLA_BACKUP], MAC, 6);
}

static ure_io_t sim_io(chip_t *c)
{
    ure_io_t io;
    memset(&io, 0, sizeof io);
    io.ctx = c;
    io.ctl = sim_ctl;
    io.espera_ms = sim_espera;
    return io;
}

/* ---------------------------------------------------------- pruebas */

static void prueba_chips(void)
{
    CHECK(ure_chip_de_versao(0x4c00) == URE_CH_8152 && ure_chip_de_versao(0x4c10) == URE_CH_8152, "versiones 4c00/4c10 = RTL8152");
    CHECK(ure_chip_de_versao(0x5c00) == URE_CH_8153 && ure_chip_de_versao(0x5c30) == URE_CH_8153, "versiones 5c00..5c30 = RTL8153");
    CHECK(ure_chip_de_versao(0x6000) == URE_CH_8153B && ure_chip_de_versao(0x6010) == URE_CH_8153B, "versiones 6000/6010 = RTL8153B");
    CHECK(ure_chip_de_versao(0x7020) == URE_CH_8156 && ure_chip_de_versao(0x7030) == URE_CH_8156, "versiones 7020/7030 = RTL8156");
    CHECK(ure_chip_de_versao(0x7410) == URE_CH_8156B, "versión 7410 = RTL8156B");
    CHECK(ure_chip_de_versao(0x1030) == URE_CH_NINGUNO && ure_chip_de_versao(0x2020) == URE_CH_NINGUNO &&
          ure_chip_de_versao(0x7420) == URE_CH_NINGUNO && ure_chip_de_versao(0x0000) == URE_CH_NINGUNO,
          "RTL8157 (1030), RTL8159 (2020), RTL8153D (7420) y 0000 sin soporte");
}

static void prueba_ids(void)
{
    CHECK(URE_N_IDS >= 50, "tabla de VID:PID: %d entradas (OpenBSD)", URE_N_IDS);
    CHECK(ure_id_conhecido(0x0BDA, 0x8152) && ure_id_conhecido(0x0BDA, 0x8153) && ure_id_conhecido(0x0BDA, 0x8156),
          "0bda:8152, 0bda:8153 y 0bda:8156 reconocidos");
    CHECK(ure_id_conhecido(0x0BDA, 0x8153) && ure_id_conhecido(0x043E, 0x9819), "un OEM de la tabla (LG 043e:9819) reconocido");
    CHECK(!ure_id_conhecido(0x0BDA, 0x8179) && !ure_id_conhecido(0x0BDA, 0x8176), "los Wi-Fi Realtek (8179, 8176) NO son Ethernet");
    CHECK(!ure_id_conhecido(0x1234, 0x5678), "un VID:PID cualquiera no se reconoce");
    int repetidos = 0;
    for (int i = 0; i < URE_N_IDS; i++)
        for (int j = i + 1; j < URE_N_IDS; j++)
            if (ure_ids[i].vid == ure_ids[j].vid && ure_ids[i].pid == ure_ids[j].pid) repetidos++;
    CHECK(repetidos == 0, "ningún VID:PID repetido en la tabla");
}

/* arranque completo (attach + init) con comprobaciones de la fuente */
static void prueba_familia(uint16_t ver, const char *nombre, int chip, bool super, bool alta)
{
    chip_t c;
    sim_nuevo(&c, ver);
    ure_io_t io = sim_io(&c);
    int r = ure_iniciar_chip(&io, chip, ver, super);
    CHECK(r == 0 && io.errores == 0, "%s (%04x): inicio del chip sin errores", nombre, ver);
    uint8_t mac[6];
    ure_ler_mac(&io, chip, ver, mac);
    CHECK(memcmp(mac, MAC, 6) == 0, "%s: la MAC se lee del registro del chip (%s)", nombre,
          (chip == URE_CH_8152 && ver == 0x4c00) ? "PLA_IDR" : "PLA_BACKUP");
    int paso = -1;
    r = ure_arrancar(&io, chip, ver, super, alta, MAC, URE_BUF_RX, &paso);
    CHECK(r == 0 && paso == URE_P_FIN && io.errores == 0, "%s: arranque de la interfaz completo (paso «%s», errores %u)", nombre, ure_nome_passo(paso), io.errores);
    CHECK(sim_lee16(&c, 1, URE_PLA_RMS) == 1522, "%s: RMS = 1522 (MTU 1500 + cabecera, CRC y VLAN)", nombre);
    CHECK(memcmp(&c.mem[1][URE_PLA_IDR], MAC, 6) == 0, "%s: la MAC queda en PLA_IDR", nombre);
    CHECK((sim_lee32(&c, 1, URE_PLA_RCR) & (URE_RCR_APM | URE_RCR_AB | URE_RCR_AM)) == (URE_RCR_APM | URE_RCR_AB | URE_RCR_AM),
          "%s: filtro con APM, AB y AM", nombre);
    CHECK(c.mem[1][URE_PLA_MAR] == 0xff && c.mem[1][URE_PLA_MAR + 7] == 0xff, "%s: multicast aceptado (hash a todo 1)", nombre);
    CHECK((sim_lee32(&c, 1, URE_PLA_CR) & (URE_CR_RE | URE_CR_TE)) == (URE_CR_RE | URE_CR_TE), "%s: transmisión y recepción activas (CR)", nombre);
    CHECK(c.espera_ms < 15000, "%s: esperas acotadas (%u ms)", nombre, c.espera_ms);
}

static void prueba_pormenores(void)
{
    /* 8156 y 8156B: anuncio de 2500 Mb/s por 0xa5d4, buffer de recepción 16 KB */
    chip_t c;
    sim_nuevo(&c, 0x7020);
    ure_io_t io = sim_io(&c);
    int paso = -1;
    ure_iniciar_chip(&io, URE_CH_8156, 0x7020, false);
    ure_arrancar(&io, URE_CH_8156, 0x7020, false, false, MAC, URE_BUF_RX, &paso);
    uint16_t c10 = (uint16_t)(c.mem[1][(URE_OCP_10GBT_CTRL & 0x0fff) | 0xb000] | c.mem[1][((URE_OCP_10GBT_CTRL & 0x0fff) | 0xb000) + 1] << 8);
    CHECK((c10 & URE_ADV_2500TFDX) != 0, "RTL8156: anuncia 2500 Mb/s full-duplex (registro 0xa5d4)");
    /* OpenBSD ure_ifmedia_init: (rxbufsz - URE_FRAMELEN(mtu) - sizeof(struct ure_rxpkt) - URE_RX_BUF_ALIGN) / 8,
     * con la cabecera de recepción de 24 bytes (seis palabras de 32 bits) */
    CHECK(sim_lee16(&c, 0, URE_USB_RX_EARLY_SIZE) == 1853, "RTL8156: RX_EARLY_SIZE = (16384 - 1522 - 24 - 8) / 8 = 1853 (%u)",
          sim_lee16(&c, 0, URE_USB_RX_EARLY_SIZE));
    CHECK(c.wr[1][URE_PLA_RCR1] > 0 && (c.mem[1][URE_PLA_RCR1] & (URE_INNER_VLAN | URE_OUTER_VLAN)) == 0,
          "RTL8156: etiqueta VLAN por hardware apagada (PLA_RCR1, ure_rxvlan)");
    CHECK(sim_lee32(&c, 0, URE_USB_RX_BUF_TH) == 0x00600400u, "RTL8156: umbral de búfer USB 0x00600400");

    /* 8152: sin 1000 ni 2500 en el anuncio; 8153: con 1000 */
    chip_t c2;
    sim_nuevo(&c2, 0x4c00);
    ure_io_t io2 = sim_io(&c2);
    ure_iniciar_chip(&io2, URE_CH_8152, 0x4c00, false);
    ure_arrancar(&io2, URE_CH_8152, 0x4c00, false, true, MAC, URE_BUF_RX, &paso);   /* alta velocidad (USB 2.0) */
    uint16_t gtcr2 = (uint16_t)(((URE_OCP_BASE_MII + MII_100T2CR * 2) & 0x0fff) | 0xb000);
    CHECK(sim_lee16(&c2, 1, gtcr2) == 0 && c2.wr[1][gtcr2] > 0, "RTL8152: el anuncio de 1000 queda a 0 (no tiene gigabit)");
    CHECK(sim_lee32(&c2, 1, URE_PLA_RXFIFO_CTRL1) == URE_RXFIFO_THR2_HIGH, "RTL8152 a alta velocidad: umbral FIFO HIGH");
    CHECK(c2.wr[1][URE_PLA_CPCR] > 0 && (c2.mem[1][URE_PLA_CPCR] & URE_CPCR_RX_VLAN) == 0,
          "RTL8152: etiqueta VLAN por hardware apagada (PLA_CPCR, ure_rxvlan)");
    /* a velocidad completa (USB 1.1) el umbral es el FULL de OpenBSD */
    chip_t c2b;
    sim_nuevo(&c2b, 0x4c00);
    ure_io_t io2b = sim_io(&c2b);
    ure_iniciar_chip(&io2b, URE_CH_8152, 0x4c00, false);
    ure_arrancar(&io2b, URE_CH_8152, 0x4c00, false, false, MAC, URE_BUF_RX, &paso);
    CHECK(sim_lee32(&c2b, 1, URE_PLA_RXFIFO_CTRL1) == URE_RXFIFO_THR2_FULL, "RTL8152 a velocidad completa: umbral FIFO FULL");

    chip_t c3;
    sim_nuevo(&c3, 0x5c00);
    ure_io_t io3 = sim_io(&c3);
    ure_iniciar_chip(&io3, URE_CH_8153, 0x5c00, false);
    ure_arrancar(&io3, URE_CH_8153, 0x5c00, false, false, MAC, URE_BUF_RX, &paso);
    uint16_t gtcr = (uint16_t)(((URE_OCP_BASE_MII + MII_100T2CR * 2) & 0x0fff) | 0xb000);
    CHECK(sim_lee16(&c3, 1, gtcr) == (GTCR_ADV_1000TFDX | GTCR_ADV_1000THDX), "RTL8153: anuncia 1000 full y medio dúplex (%04x)",
          sim_lee16(&c3, 1, gtcr));

    /* RTL8153B (6000) y 8156B (7410): el reinicio con CR_RST corre y termina */
    chip_t c4;
    sim_nuevo(&c4, 0x7410);
    ure_io_t io4 = sim_io(&c4);
    ure_iniciar_chip(&io4, URE_CH_8156B, 0x7410, true);
    CHECK(io4.errores == 0 && (sim_lee32(&c4, 1, URE_PLA_CR) & URE_CR_RST) == 0, "RTL8156B con USB 3.0: inicio y CR_RST borrado");
    CHECK(c4.espera_ms < 15000, "RTL8156B: esperas acotadas (%u ms)", c4.espera_ms);
}

/* El chip no responde: el paso fallido se dice y el arranque no se cuelga. */
static void prueba_sin_respuesta(void)
{
    chip_t c;
    sim_nuevo(&c, 0x5c00);
    c.mem[1][URE_PLA_BOOT_CTRL] = 0;            /* la autocarga nunca termina */
    c.mem[1][URE_PLA_BOOT_CTRL + 1] = 0;
    ure_io_t io = sim_io(&c);
    int r = ure_iniciar_chip(&io, URE_CH_8153, 0x5c00, false);
    CHECK(r == URE_P_AUTOCARGA, "RTL8153 sin autocarga: el paso es «%s»", ure_nome_passo(r));
    CHECK(c.espera_ms <= URE_VUELTAS_AUTOCARGA * 20 + 100, "RTL8153 sin autocarga: espera acotada (%u ms, tope 10000 como OpenBSD)", c.espera_ms);

    chip_t c2;
    sim_nuevo(&c2, 0x7020);
    c2.mem[1][URE_PLA_BOOT_CTRL] = 0;
    ure_io_t io2 = sim_io(&c2);
    r = ure_iniciar_chip(&io2, URE_CH_8156, 0x7020, false);
    CHECK(r == URE_P_AUTOCARGA && c2.espera_ms <= URE_VUELTAS_AUTOCARGA * 20 + 100, "RTL8156 sin autocarga: paso «%s», espera %u ms",
          ure_nome_passo(r), c2.espera_ms);

    /* el adaptador se desenchufa a mitad: todas las transferencias fallan, no hay bucle */
    chip_t c3;
    sim_nuevo(&c3, 0x7410);
    c3.fallar_desde = 5;
    ure_io_t io3 = sim_io(&c3);
    int paso = -1;
    int r3 = ure_iniciar_chip(&io3, URE_CH_8156B, 0x7410, false);
    int r4 = ure_arrancar(&io3, URE_CH_8156B, 0x7410, false, false, MAC, URE_BUF_RX, &paso);
    CHECK(io3.errores > 0 && (r3 != 0 || r4 != 0) && paso != URE_P_FIN, "RTL8156B desenchufado a mitad: error contado (%u), paso «%s»",
          io3.errores, ure_nome_passo(paso));
    CHECK(c3.espera_ms < 1000, "RTL8156B desenchufado: las esperas se cortan al fallar el USB (%u ms)", c3.espera_ms);

    /* desenchufado justo antes de la autocarga: no se esperan los 10 s enteros */
    chip_t c5;
    sim_nuevo(&c5, 0x5c30);
    c5.mem[1][URE_PLA_BOOT_CTRL] = 0;
    c5.mem[1][URE_PLA_BOOT_CTRL + 1] = 0;
    c5.fallar_desde = 3;
    ure_io_t io5 = sim_io(&c5);
    r = ure_iniciar_chip(&io5, URE_CH_8153, 0x5c30, false);
    CHECK(r == URE_P_AUTOCARGA && c5.espera_ms <= 40, "RTL8153 desenchufado en la autocarga: corta enseguida (%u ms)", c5.espera_ms);

    chip_t c4;
    sim_nuevo(&c4, 0x4c10);
    c4.fallar_desde = 1;
    ure_io_t io4 = sim_io(&c4);
    int r5 = ure_iniciar_chip(&io4, URE_CH_8152, 0x4c10, false);
    printf("ok RTL8152 sin ninguna transferencia: vuelve sin colgarse (paso %d, %u errores)\n", r5, io4.errores);
    comprobaciones++;
    CHECK(io4.errores > 0, "RTL8152 sin ninguna transferencia: los errores se cuentan");
}

/* ---------------------------------------------------------- recepción */

static uint8_t *cuadro(uint8_t *buf, uint32_t *pos, uint32_t pl, uint8_t relleno)
{
    /* struct ure_rxpkt de OpenBSD (if_urereg.h): pktlen, vlan, csum y tres reservados = 24 bytes, con la
     * longitud (CRC incluido) en los 15 bits bajos; los otros campos llevan basura a propósito (bits de
     * suma de control, VLAN), que no debe acabar en la trama. Después el cuerpo; el siguiente, alineado a 8. */
    buf[*pos] = (uint8_t)pl; buf[*pos + 1] = (uint8_t)(pl >> 8); buf[*pos + 2] = 0x08; buf[*pos + 3] = 0;
    memset(buf + *pos + 4, 0xA5, 20);
    for (uint32_t i = 0; i < pl; i++) buf[*pos + 24 + i] = (uint8_t)(relleno + i);
    *pos += 24 + ure_alinea(pl, 8);
    return buf;
}

static void prueba_rx(void)
{
    /* dos tramas: de 60 (+4 CRC) y de 100 (+4 CRC) */
    uint32_t n = 0;
    uint8_t *b = malloc(512);
    uint8_t *dst = malloc(1600);
    memset(b, 0, 512);
    cuadro(b, &n, 64, 0x10);       /* trama de 60 bytes sin CRC */
    cuadro(b, &n, 104, 0x40);      /* trama de 100 bytes sin CRC */
    ure_rx_t r = { b, n, 0 };
    int f1 = ure_rx_siguiente(&r, dst, 1514);
    CHECK(f1 == 60 && dst[0] == 0x10 && dst[59] == (uint8_t)(0x10 + 59), "rx: primera trama de 60 bytes, contenido correcto (%d)", f1);
    int f2 = ure_rx_siguiente(&r, dst, 1514);
    CHECK(f2 == 100 && dst[0] == 0x40 && dst[99] == (uint8_t)(0x40 + 99), "rx: segunda trama de 100 bytes tras la alineación (%d)", f2);
    CHECK(ure_rx_siguiente(&r, dst, 1514) == 0, "rx: sin más tramas al acabar la transferencia");
    free(b);
    CHECK(URE_RX_CABECERA == 24 && URE_RX_CABECERA == 6 * 4, "rx: la cabecera de recepción es struct ure_rxpkt (24 bytes)");

    /* agregación llena como la de 16 KB: tramas de 1514 + CRC seguidas, la última sin relleno final */
    uint8_t *big = calloc(URE_BUF_RX, 1);
    uint32_t nb = 0;
    int puestas = 0;
    while (nb + 24 + 1518 <= URE_BUF_RX) { cuadro(big, &nb, 1518, (uint8_t)puestas); puestas++; }
    ure_rx_t rb2 = { big, nb, 0 };
    int leidas = 0, malas = 0;
    for (;;) {
        int f = ure_rx_siguiente(&rb2, dst, 1514);
        if (f == 0) break;
        if (f != 1514 || dst[0] != (uint8_t)leidas || dst[1513] != (uint8_t)(leidas + 1513)) malas++;
        leidas++;
    }
    CHECK(leidas == puestas && malas == 0, "rx: transferencia de %u bytes con %d tramas de 1514: todas enteras (%d leídas, %d malas)",
          nb, puestas, leidas, malas);
    free(big);

    /* errores: cabecera incompleta, longitud que se sale, trama pequeña, trama grande */
    uint8_t c5[5] = { 1, 2, 3, 4, 5 };
    ure_rx_t r5 = { c5, 5, 0 };
    CHECK(ure_rx_siguiente(&r5, dst, 1514) == URE_RX_E_CORTA, "rx: cabecera incompleta -> E_CORTA");
    uint8_t c12[24] = { 0 };
    c12[0] = 200;                  /* 200 bytes que no hay */
    ure_rx_t r12 = { c12, 24, 0 };
    CHECK(ure_rx_siguiente(&r12, dst, 1514) == URE_RX_E_LARGA, "rx: longitud mayor que lo que queda -> E_LARGA");
    uint8_t c40[40] = { 0 };
    c40[0] = 10;                   /* 10 bytes: menor que cabecera + CRC */
    ure_rx_t r20 = { c40, 40, 0 };
    CHECK(ure_rx_siguiente(&r20, dst, 1514) == URE_RX_E_PEQUENA, "rx: trama de 10 bytes (con CRC) -> E_PEQUENA");
    uint8_t *c2000 = calloc(2100, 1);
    c2000[0] = (uint8_t)(1600 & 0xff); c2000[1] = (uint8_t)(1600 >> 8);
    ure_rx_t rg = { c2000, 2100, 0 };
    CHECK(ure_rx_siguiente(&rg, dst, 1514) == URE_RX_E_MAYOR, "rx: trama de 1596 bytes (sin CRC 1596) -> E_MAYOR");
    free(c2000);

    /* longitud con los bits altos (0x8000) puestos: solo cuentan los 15 bajos (URE_RXPKT_LEN_MASK) */
    uint8_t cm[88] = { 0 };
    cm[0] = 0x40; cm[1] = 0x80;    /* 0x8040 & 0x7fff = 64 -> trama de 60 */
    memset(cm + 24, 0x77, 64);
    ure_rx_t rm = { cm, 88, 0 };
    CHECK(ure_rx_siguiente(&rm, dst, 1514) == 60, "rx: los bits altos de la longitud se ignoran");

    /* ruido al azar: nunca se sale del búfer y siempre termina */
    uint32_t semilla = 12345u;
    int lecturas_malas = 0, tramas = 0;
    for (int k = 0; k < 4000; k++) {
        uint32_t len = 1 + (semilla % 700);
        uint8_t *rb = malloc(len);
        for (uint32_t i = 0; i < len; i++) {
            semilla = semilla * 1103515245u + 12345u;
            rb[i] = (uint8_t)(semilla >> 16);
        }
        ure_rx_t rr = { rb, len, 0 };
        int vueltas = 0;
        for (;;) {
            int f = ure_rx_siguiente(&rr, dst, 1514);
            if (f == 0) break;
            if (f > 0) { tramas++; if (f > 1514) lecturas_malas++; }
            if (++vueltas > 2000) { lecturas_malas++; break; }
        }
        free(rb);
    }
    CHECK(lecturas_malas == 0, "rx: 4000 transferencias de ruido: terminan, sin salirse (%d tramas válidas)", tramas);
    free(dst);
}

/* ---------------------------------------------------------- envío */

static void prueba_tx(void)
{
    uint8_t q[1514], out[1600];
    for (int i = 0; i < 1514; i++) q[i] = (uint8_t)i;
    uint32_t t = ure_tx_montar(out, sizeof out, q, 20);
    CHECK(t == 8 + 60, "tx: trama de 20 bytes se rellena a 60 (total %u)", t);
    uint32_t pl = (uint32_t)out[0] | (uint32_t)out[1] << 8 | (uint32_t)out[2] << 16 | (uint32_t)out[3] << 24;
    CHECK(pl == (60u | URE_TXPKT_TX_FS | URE_TXPKT_TX_LS), "tx: cabecera con FS y LS (%08x)", pl);
    CHECK(out[4] == 0 && out[5] == 0 && out[6] == 0 && out[7] == 0, "tx: vlan = 0");
    CHECK(out[8] == 0 && out[8 + 19] == 19 && out[8 + 20] == 0 && out[8 + 59] == 0, "tx: cuerpo copiado y relleno a cero");
    t = ure_tx_montar(out, sizeof out, q, 1514);
    CHECK(t == 8 + 1514, "tx: trama máxima de 1514 (total %u)", t);
    CHECK(ure_tx_montar(out, sizeof out, q, 1515) == 0, "tx: 1515 bytes -> rechazada");
    CHECK(ure_tx_montar(out, sizeof out, q, 0) == 0, "tx: trama vacía -> rechazada");
    CHECK(ure_tx_montar(out, 60, q, 100) == 0, "tx: búfer pequeño -> rechazada");

    /* total múltiplo del paquete USB: la trama crece un byte (sin paquete de longitud cero) */
    uint8_t o2[1601];
    t = ure_tx_montar(o2, 1600, q, 504);
    uint32_t t2 = ure_tx_evitar_mps(o2, sizeof o2, t, 512);
    uint32_t pl2 = (uint32_t)o2[0] | (uint32_t)o2[1] << 8 | (uint32_t)o2[2] << 16 | (uint32_t)o2[3] << 24;
    CHECK(t == 512 && t2 == 513 && (pl2 & URE_TXPKT_LEN_MASK) == 505 && (pl2 & URE_TXPKT_TX_FS) && o2[512] == 0,
          "tx: 8 + 504 = 512 bytes -> 513, longitud 505 con FS/LS (%u, %08x)", t2, pl2);
    t = ure_tx_montar(o2, 1600, q, 1016);
    CHECK(ure_tx_evitar_mps(o2, sizeof o2, t, 1024) == 1025 && ure_tx_evitar_mps(o2, sizeof o2, 1025, 1024) == 1025,
          "tx: 8 + 1016 con paquetes de 1024 (USB 3) -> 1025");
    t = ure_tx_montar(o2, 1600, q, 1000);
    CHECK(ure_tx_evitar_mps(o2, sizeof o2, t, 512) == t, "tx: un total que no es múltiplo no cambia");
    t = ure_tx_montar(o2, 1600, q, 120);
    CHECK(ure_tx_evitar_mps(o2, sizeof o2, t, 64) == 129, "tx: a velocidad completa (64 bytes) también: 128 -> 129");
}

static void prueba_enlace(void)
{
    bool fdx;
    CHECK(ure_velocidade(URE_PHYSTATUS_LINK | URE_PHYSTATUS_1000MBPS | URE_PHYSTATUS_FDX, &fdx) == 1000 && fdx, "enlace 1000 Mb/s full-duplex");
    CHECK(ure_velocidade(URE_PHYSTATUS_LINK | URE_PHYSTATUS_2500MBPS | URE_PHYSTATUS_FDX, &fdx) == 2500 && fdx, "enlace 2500 Mb/s full-duplex");
    CHECK(ure_velocidade(URE_PHYSTATUS_LINK | URE_PHYSTATUS_100MBPS, &fdx) == 100 && !fdx, "enlace 100 Mb/s medio dúplex");
    CHECK(ure_velocidade(URE_PHYSTATUS_LINK | URE_PHYSTATUS_10MBPS | URE_PHYSTATUS_FDX, &fdx) == 10 && fdx, "enlace 10 Mb/s full-duplex");
    CHECK(ure_velocidade(URE_PHYSTATUS_1000MBPS | URE_PHYSTATUS_FDX, &fdx) == 0, "sin LINK no hay velocidad");
    chip_t c;
    sim_nuevo(&c, 0x7020);
    ure_io_t io = sim_io(&c);
    ure_enlace_arriba(&io);
    CHECK((c.mem[1][URE_PLA_CR] & (URE_CR_RE | URE_CR_TE)) == (URE_CR_RE | URE_CR_TE) && io.errores == 0,
          "al subir el enlace se reactivan RE y TE (ure_link_state)");
}

int main(void)
{
    prueba_chips();
    prueba_ids();
    prueba_familia(0x4c00, "RTL8152", URE_CH_8152, false, false);
    prueba_familia(0x5c00, "RTL8153", URE_CH_8153, false, true);
    prueba_familia(0x5c30, "RTL8153 (5c30)", URE_CH_8153, true, false);
    prueba_familia(0x6000, "RTL8153B", URE_CH_8153B, false, true);
    prueba_familia(0x7020, "RTL8156", URE_CH_8156, false, false);
    prueba_familia(0x7030, "RTL8156 (7030)", URE_CH_8156, true, true);
    prueba_familia(0x7410, "RTL8156B", URE_CH_8156B, true, false);
    prueba_pormenores();
    prueba_sin_respuesta();
    prueba_rx();
    prueba_tx();
    prueba_enlace();
    printf("%d comprobaciones, %d fallos\n", comprobaciones, fallos);
    if (fallos == 0) printf("ure_anfitriao ok\n");
    return fallos ? 1 : 0;
}
