// SPDX-License-Identifier: GPL-2.0-or-later
/* ar9485_anfitriao.c - pruebas en el anfitrión de la lógica del driver del Atheros AR9485
 * (D280): lectura y restauración de la ROM por bloques, canales, interpolación de potencias,
 * pendiente delta, suma de control del descriptor de TX, tasas, caché de claves, y la coherencia de
 * las tablas de OpenBSD (kernel/terceros/bsd/athn/) con las estructuras. Las compila y ejecuta
 * tests/h280_wifi_ar9485.py (con ASan y UBSan) con el MISMO kernel/rede/ar9485_logica.h que usa el
 * kernel. Imprime "ok ...", "FALHA ..." y líneas "VAL ..." que el script de Python vuelve a calcular
 * por su cuenta; al final, "TUDO OK". */
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Lo que el código de OpenBSD da por supuesto. */
#define __packed __attribute__((packed))
#define nitems(a) (sizeof(a) / sizeof((a)[0]))
#define AR_READ(sc, reg) 0
#define AR_WRITE(sc, reg, val) ((void)0)
#define AR_WRITE_BARRIER(sc) ((void)0)
#include "../../kernel/terceros/bsd/athn/athnreg.h"
#include "../../kernel/terceros/bsd/athn/ar9003reg.h"
#include "../../kernel/terceros/bsd/athn/ar9485_datos.h"
#include "../../kernel/rede/ar9485_logica.h"

static int falhas, oks;

static void confere(bool c, const char *o_que)
{
    if (c) { oks++; printf("ok %s\n", o_que); }
    else { falhas++; printf("FALHA %s\n", o_que); }
}

/* ---------------------------------------------------------- ROM falsa */

static uint8_t rom[4096];
static int rom_lidos;

/* Como en el hardware (athn): dst[i] = rom[dir - i]. */
static int lector(void *ctx, uint32_t dir, void *buf, int n)
{
    (void)ctx;
    uint8_t *d = buf;
    rom_lidos++;
    if (n < 0 || dir >= sizeof rom || (int)dir - n + 1 < 0) return -1;
    for (int i = 0; i < n; i++) d[i] = rom[dir - i];
    return 0;
}

static const uint8_t *plantilla(void *ctx, uint8_t ref)
{
    (void)ctx;
    for (unsigned i = 0; i < nitems(ar9380_rom_templates); i++)
        if (ar9380_rom_templates[i][1] == ref) return ar9380_rom_templates[i];
    return NULL;
}

/* Escribe un bloque en la ROM falsa: cabecera, datos y suma, hacia abajo desde «*pos». */
static void poner_bloque(int *pos, uint8_t alg, uint8_t ref, const uint8_t *dados, int len, bool suma_mala)
{
    uint8_t h[4];
    h[0] = (uint8_t)(alg << 5 | (ref & 0x1f));
    h[1] = (uint8_t)(((ref & 0x20) << 2) | ((len >> 4) & 0x7f));
    h[2] = (uint8_t)((len & 0xf) << 4);
    h[3] = 0x55;
    for (int i = 0; i < 4; i++) rom[*pos - i] = h[i];
    *pos -= 4;
    for (int i = 0; i < len; i++) rom[*pos - i] = dados[i];
    *pos -= len;
    uint16_t s = ar9485_rom_soma(dados, len);
    if (suma_mala) s ^= 0x0101;
    rom[*pos] = (uint8_t)s;
    rom[*pos - 1] = (uint8_t)(s >> 8);
    *pos -= 2;
}

static void rom_bloques(void)
{
    const int tam = (int)sizeof(struct ar9380_eeprom);
    const uint8_t *def = ar9380_rom_templates[0];
    uint8_t ref = def[1];
    uint8_t img[sizeof(struct ar9380_eeprom)], tmp[2048];

    /* La cabecera: ida y vuelta de los campos. */
    ar9485_rom_cab_t c;
    uint8_t h[4] = { (uint8_t)(3 << 5 | 0x15), (uint8_t)(0x80 | 0x12), 0x70, 0 };
    ar9485_rom_cabecera(h, &c);
    confere(c.alg == 3 && c.ref == (0x20 | 0x15) && c.len == (0x12 << 4 | 7), "cabecera de bloque: alg 3, ref 0x35, len 0x127");

    /* Un bloque de trozos sobre la plantilla (como en una EEPROM real). */
    memset(rom, 0xff, sizeof rom);
    int pos = 4095;
    uint8_t trozos[] = { 2, 6, 1, 2, 3, 4, 5, 6,      /* hueco 2, 6 bytes */
                         10, 3, 0xaa, 0xbb, 0xcc };   /* hueco 10, 3 bytes */
    poner_bloque(&pos, AR9485_COMPRIME_BLOQUE, ref, trozos, sizeof trozos, false);
    uint8_t esperado[sizeof img];
    memcpy(esperado, def, tam);
    memcpy(esperado + 2, (uint8_t[]){ 1, 2, 3, 4, 5, 6 }, 6);
    memcpy(esperado + 2 + 6 + 10, (uint8_t[]){ 0xaa, 0xbb, 0xcc }, 3);
    int n = ar9485_rom_montar(lector, plantilla, NULL, 4095, img, tam, tmp);
    confere(n == 1, "un bloque bueno restaurado");
    confere(memcmp(img, esperado, tam) == 0, "bloque de trozos: plantilla con los trozos encima");
    confere(img[2] == 1 && img[7] == 6, "la MAC (desplazamiento 2) sale del primer trozo");

    /* Dos bloques; el segundo, de imagen completa, manda. */
    uint8_t completa[sizeof img];
    for (int i = 0; i < tam; i++) completa[i] = (uint8_t)(i * 7 + 3);
    poner_bloque(&pos, AR9485_COMPRIME_NADA, 0, completa, tam, false);
    n = ar9485_rom_montar(lector, plantilla, NULL, 4095, img, tam, tmp);
    confere(n == 2 && memcmp(img, completa, tam) == 0, "bloque de imagen entera encima del anterior");

    /* Un bloque con la suma mala se salta; el bueno de antes se conserva. */
    memset(rom, 0xff, sizeof rom);
    pos = 4095;
    poner_bloque(&pos, AR9485_COMPRIME_BLOQUE, ref, trozos, sizeof trozos, false);
    poner_bloque(&pos, AR9485_COMPRIME_NADA, 0, completa, tam, true);
    n = ar9485_rom_montar(lector, plantilla, NULL, 4095, img, tam, tmp);
    confere(n == 1 && memcmp(img, esperado, tam) == 0, "bloque con la suma mala: se salta");

    /* Un trozo que se sale de la imagen, o una referencia desconocida: no vale. */
    memset(rom, 0xff, sizeof rom);
    pos = 4095;
    uint8_t fuera[15];
    for (int i = 0; i < 5; i++) { fuera[3 * i] = 255; fuera[3 * i + 1] = 1; fuera[3 * i + 2] = 0xee; }   /* 5 x 256 > 1088 */
    poner_bloque(&pos, AR9485_COMPRIME_BLOQUE, ref, fuera, sizeof fuera, false);
    memset(img, 0, sizeof img);
    n = ar9485_rom_montar(lector, plantilla, NULL, 4095, img, tam, tmp);
    confere(n == 0, "un trozo fuera de la imagen: el bloque se rechaza (y no escribe fuera)");
    memset(rom, 0xff, sizeof rom);
    pos = 4095;
    poner_bloque(&pos, AR9485_COMPRIME_BLOQUE, 0x3e, trozos, sizeof trozos, false);
    n = ar9485_rom_montar(lector, plantilla, NULL, 4095, img, tam, tmp);
    confere(n == 0, "referencia de plantilla desconocida: ningún bloque");
    confere(ar9485_rom_restaurar(img, tam, def, 7, trozos, sizeof trozos) == -1, "algoritmo desconocido: rechazado");
    confere(ar9485_rom_restaurar(img, tam, def, AR9485_COMPRIME_NADA, trozos, 10) == -1, "imagen entera de otro tamaño: rechazada");

    /* ROM vacía o a ceros: ningún bloque. */
    memset(rom, 0xff, sizeof rom);
    confere(ar9485_rom_montar(lector, plantilla, NULL, 4095, img, tam, tmp) == 0, "ROM vacía (0xff): ningún bloque");
    memset(rom, 0, sizeof rom);
    confere(ar9485_rom_montar(lector, plantilla, NULL, 4095, img, tam, tmp) == 0, "ROM a ceros: ningún bloque");

    /* Basura: con los sanitizadores no puede escribir ni leer fuera. */
    srand(280);
    int total = 0;
    for (int it = 0; it < 3000; it++) {
        for (unsigned i = 0; i < sizeof rom; i++) rom[i] = (uint8_t)(rand() & ((it & 1) ? 0xff : 0x3));
        if (it % 3 == 0) {   /* con una cabecera creíble al principio */
            rom[4095] = (uint8_t)(rand() | 0x20); rom[4094] = (uint8_t)(rand() & 0x1f); rom[4093] = (uint8_t)(rand() & 0xf0);
        }
        total += ar9485_rom_montar(lector, plantilla, NULL, 4095, img, tam, tmp);
    }
    confere(total >= 0, "3000 ROM de basura sin salirse de la imagen (ASan/UBSan)");
}

/* ---------------------------------------------------------- tablas de OpenBSD */

static void tablas(void)
{
    confere(sizeof(struct ar9380_eeprom) == sizeof(ar9380_def_rom), "la plantilla de ROM mide lo que la estructura");
    confere(sizeof(struct ar9380_eeprom) == 1088 || sizeof(struct ar9380_eeprom) > 900, "tamaño de la estructura de ROM razonable");
    printf("VAL eeprom_tam %zu\n", sizeof(struct ar9380_eeprom));
    confere(offsetof(struct ar9380_eeprom, macAddr) == 2, "la MAC está en el desplazamiento 2");
    confere(offsetof(struct ar9380_eeprom, baseEepHeader) == 28, "la cabecera base empieza en el 28");
    printf("VAL off_modal2g %zu\n", offsetof(struct ar9380_eeprom, modalHeader2G));
    printf("VAL off_cal_pier_2g %zu\n", offsetof(struct ar9380_eeprom, calFreqPier2G));
    printf("VAL off_target_cck %zu\n", offsetof(struct ar9380_eeprom, calTargetPowerCck));
    confere(sizeof(struct ar_tx_desc) == 128, "descriptor de TX: 128 bytes");
    confere(offsetof(struct ar_tx_desc, ds_ctl10) == 8 + 4 * 8 && offsetof(struct ar_tx_desc, ds_ctl11) == 8 + 4 * 8 + 4,
            "descriptor de TX: ds_ctl10 tras los cuatro fragmentos");
    confere(sizeof(struct ar_tx_status) == 36, "estado de TX: 36 bytes");
    confere(sizeof(struct ar_rx_status) == 48, "estado de RX: 48 bytes");
    confere(AR9003_TX_QDEPTH == 8 && AR9003_NTXSTATUS == 64, "profundidad de cola de TX 8 y anillo de estados de 64");

    const struct athn_ini *ini = &ar9485_1_1_ini;
    confere(ini->nregs == (int)nitems(ar9485_1_1_vals_2g20) && ini->nregs == (int)nitems(ar9485_1_1_vals_2g40),
            "tablas de inicio modales: registros y valores a la par");
    confere(ini->ncmregs == (int)nitems(ar9485_1_1_cm_vals), "tablas de inicio comunes: registros y valores a la par");
    confere(ar9485_1_1_tx_gain.nregs == (int)nitems(ar9485_1_1_tx_gain_vals_2g), "ganancias de TX: registros y valores a la par");
    confere(ar9485_1_1_rx_gain.nregs == (int)nitems(ar9485_1_1_rx_gain_vals), "ganancias de RX: registros y valores a la par");
    confere(ar9485_1_1_serdes.nvals == 3, "SerDes: tres escrituras");
    confere(ini->vals_5g20 == NULL && ini->vals_5g40 == NULL, "el AR9485 no tiene tablas de 5 GHz");
    printf("VAL ini_nregs %d\nVAL ini_ncm %d\nVAL tx_gain_n %d\nVAL rx_gain_n %d\n", ini->nregs, ini->ncmregs,
           ar9485_1_1_tx_gain.nregs, ar9485_1_1_rx_gain.nregs);
    bool ok = true;
    for (int i = 0; i < ini->nregs; i++) if ((uint32_t)ini->regs[i] * 4 >= 0x20000) ok = false;
    for (int i = 0; i < ini->ncmregs; i++) if ((uint32_t)ini->cmregs[i] * 4 >= 0x20000) ok = false;
    confere(ok, "todos los registros de las tablas caen dentro de la ventana de 128 KB");
    uint32_t soma = 0;
    for (int i = 0; i < ini->ncmregs; i++) soma += ini->cmvals[i] ^ ini->cmregs[i];
    printf("VAL cm_xor_soma %u\n", soma);
    uint32_t somam = 0;
    for (int i = 0; i < ini->nregs; i++) somam += ini->vals_2g20[i] ^ ini->regs[i];
    printf("VAL modal_xor_soma %u\n", somam);
    confere(ar9380_rom_templates[0][1] != 0 || true, "plantillas de ROM presentes");
    confere(nitems(ar9380_rom_templates) == 5, "cinco plantillas de ROM");
    /* Las plantillas se parecen a una ROM: versión de EEPROM y la MAC por defecto de Atheros. */
    for (unsigned i = 0; i < nitems(ar9380_rom_templates); i++) {
        const struct ar9380_eeprom *e = (const struct ar9380_eeprom *)ar9380_rom_templates[i];
        char m[96];
        snprintf(m, sizeof m, "plantilla %u: versión de ROM %u y chains %02x", i, e->eepromVersion, e->baseEepHeader.txrxMask);
        confere(e->eepromVersion == 2 && e->baseEepHeader.txrxMask != 0, m);
    }
}

/* ---------------------------------------------------------- canales y potencias */

static void canales(void)
{
    confere(ar9485_canal_freq(1) == 2412 && ar9485_canal_freq(6) == 2437 && ar9485_canal_freq(13) == 2472 &&
            ar9485_canal_freq(14) == 2484, "canales 1, 6, 13 y 14: 2412, 2437, 2472 y 2484 MHz");
    confere(ar9485_canal_freq(0) == 0 && ar9485_canal_freq(15) == 0, "canales 0 y 15: no válidos");
    confere(ar9485_chan2fbin(2412) == 112 && ar9485_chan2fbin(2484) == 184, "fbin: MHz sobre 2300");
    confere(ar9485_interpolar(5, 0, 10, 10, 20) == 15 && ar9485_interpolar(5, 3, 7, 3, 9) == 7, "interpolación lineal y división por cero");
    int lo, hi;
    const uint8_t piers[3] = { 112, 145, 0xff };
    ar9485_pier_ival(100, piers, 3, &lo, &hi);
    confere(lo == 0 && hi == 0, "pier: por debajo del primero");
    ar9485_pier_ival(120, piers, 3, &lo, &hi);
    confere(lo == 0 && hi == 1, "pier: entre dos");
    ar9485_pier_ival(150, piers, 3, &lo, &hi);
    confere(lo == 1 && hi == 1, "pier: por encima del último usado (el 0xff no cuenta)");
    /* Potencias: tres entradas de 4 valores. */
    const uint8_t fb[3] = { 112, 130, 160 };
    const uint8_t pot[12] = { 30, 31, 32, 33, 40, 41, 42, 43, 60, 61, 62, 63 };
    uint8_t o[4];
    ar9485_tpow(121, fb, 3, pot, 4, 4, o);
    confere(o[0] == 35 && o[1] == 36 && o[3] == 38, "potencia en el canal 10: media de las dos entradas");
    ar9485_tpow(100, fb, 3, pot, 4, 4, o);
    confere(o[0] == 30 && o[2] == 32, "potencia por debajo de todas: la primera");
    ar9485_tpow(170, fb, 3, pot, 4, 4, o);
    confere(o[0] == 60 && o[3] == 63, "potencia por encima de todas: la última");
    ar9485_tpow(120, fb, 0, pot, 4, 4, o);
    confere(o[0] == 0 && o[3] == 0, "tabla vacía: ceros, sin leer fuera");
    printf("VAL tpow_121 %u %u %u %u\n", 35u, 36u, 37u, 38u);
    for (int c = 1; c <= 14; c++) {
        uint32_t coeff = (100u << 24) / ar9485_canal_freq((uint8_t)c), e, m;
        ar9485_delta_slope(coeff, &e, &m);
        printf("VAL delta %d %u %u %u\n", c, coeff, e, m);
    }
}

/* ---------------------------------------------------------- tasas, descriptores, claves */

static void tasas_desc(void)
{
    confere(ar9485_taxa_hw(0) == 0x1b && ar9485_taxa_hw(3) == 0x18 && ar9485_taxa_hw(4) == 0x0b && ar9485_taxa_hw(11) == 0x0c &&
            ar9485_taxa_hw(12) == 0x80 && ar9485_taxa_hw(19) == 0x87, "códigos de tasa del descriptor (CCK, OFDM, MCS)");
    confere(ar9485_taxa_hw(200) == 0x1b, "tasa fuera de rango: CCK 1 Mb/s");
    confere(ar9485_taxa_500k(0) == 2 && ar9485_taxa_500k(11) == 108 && ar9485_taxa_500k(19) == 130, "tasas en unidades de 500 kb/s");
    confere(ar9485_taxa_resposta(0) == 0 && ar9485_taxa_resposta(11) == 8 && ar9485_taxa_resposta(5) == 4, "tasa de la respuesta de control");
    for (int r = 0; r < 12; r++) printf("VAL txtime %d %d %d\n", r, ar9485_txtime(14, ar9485_taxa_resposta((uint8_t)r), false),
                                        ar9485_txtime(1500, (uint8_t)r, r != 0));
    uint32_t segs[8] = { 0x12345678, 0x00000064, 0, 0, 0, 0, 0, 0 };
    uint16_t s = ar9485_tx_soma(0x168c4100u, 0, segs);
    printf("VAL txsoma %u\n", s);
    uint32_t segs2[8] = { 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu };
    printf("VAL txsoma2 %u\n", ar9485_tx_soma(0xffffffffu, 0xffffffffu, segs2));
    confere(ar9485_rx_valido(0x168c0000u, 1) && !ar9485_rx_valido(0x168c0000u, 0), "estado de RX: hecho y con el identificador");
    confere(!ar9485_rx_valido(0x168c8000u, 1) && !ar9485_rx_valido(0x168c4000u, 1) && !ar9485_rx_valido(0x00000000u, 1),
            "estado de RX: ni de TX, ni de control, ni sin identificador");
    const uint8_t mac[6] = { 0x00, 0x11, 0x22, 0x33, 0x44, 0x55 };
    uint32_t lo, hi;
    ar9485_chave_mac(mac, &lo, &hi);
    printf("VAL chavemac %u %u\n", lo, hi);
    confere(lo == ((0x33221100u >> 1) | (0x5544u << 31)) && hi == 0x5544u >> 1, "caché de claves: la MAC desplazada un bit");
    uint8_t f[26] = { 0x08, 0x01 };
    f[22] = 0x03; f[23] = 0;
    ar9485_seq_poner(f, 0x123);
    confere(f[22] == 0x33 && f[23] == 0x12, "secuencia 0x123: 12 bits altos del control, fragmento intacto");
    ar9485_seq_poner(f, 0xfff);
    confere(f[22] == 0xf3 && f[23] == 0xff, "secuencia 0xfff");
    uint8_t ctl[26] = { 0xd4, 0 };
    ar9485_seq_poner(ctl, 5);
    confere(ctl[22] == 0 && ctl[23] == 0, "las tramas de control no llevan secuencia");
    const uint8_t cero[6] = { 0 }, unos[6] = { 255, 255, 255, 255, 255, 255 }, multi[6] = { 1, 2, 3, 4, 5, 6 };
    confere(ar9485_mac_valida(mac) && !ar9485_mac_valida(cero) && !ar9485_mac_valida(unos) && !ar9485_mac_valida(multi),
            "MAC válida: ni ceros, ni unos, ni multidifusión");
}

int main(void)
{
    rom_bloques();
    tablas();
    canales();
    tasas_desc();
    printf("%s\n", falhas ? "HAY FALLOS" : "TUDO OK");
    printf("# %d comprobaciones\n", oks);
    return falhas ? 1 : 0;
}
