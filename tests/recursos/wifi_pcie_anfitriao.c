// SPDX-License-Identifier: GPL-2.0-or-later
/* wifi_pcie_anfitriao.c - pruebas en el anfitrión de kernel/rede/wifi_pcie_logica.h (D410): catálogo por
 * identificador PCI, SREV de las Atheros, ROM «4K» del AR9285 y la del AR9287/AR5416 construidas a mano,
 * canales y palabra del sintetizador de 2,4 GHz. Compila el MISMO archivo que el kernel. Imprime «ok ...»,
 * «FALHA ...», líneas «VAL ...» que vuelve a calcular tests/h410_wifi_pcie.py y, al final, «TUDO OK». */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../kernel/rede/wifi_pcie_logica.h"

static int falhas, oks;
static void confere(bool c, const char *o_que)
{
    if (c) { oks++; printf("ok %s\n", o_que); }
    else { falhas++; printf("FALHA %s\n", o_que); }
}

/* Construye una imagen de ROM de «tam» bytes con cabecera válida y la suma de control cuadrada. */
static void rom_montar(uint16_t *w, int tam, uint8_t opcap, uint8_t rx, uint8_t tx, uint16_t versao, const uint8_t mac[6],
                       uint16_t rfsilent, uint16_t devcap)
{
    uint8_t *b = (uint8_t *)w;
    memset(w, 0, (size_t)tam);
    b[0] = (uint8_t)(tam & 0xff); b[1] = (uint8_t)(tam >> 8);
    b[4] = (uint8_t)(versao & 0xff); b[5] = (uint8_t)(versao >> 8);
    b[6] = opcap;
    memcpy(b + 10, mac, 6);
    b[16] = rx; b[17] = tx;
    b[18] = (uint8_t)(rfsilent & 0xff); b[19] = (uint8_t)(rfsilent >> 8);
    b[22] = (uint8_t)(devcap & 0xff); b[23] = (uint8_t)(devcap >> 8);
    b[30] = 0;
    int nw = tam / 2;
    uint16_t s = 0;
    for (int i = 0; i < nw; i++) if (i != 1) s ^= w[i];
    w[1] = (uint16_t)(s ^ 0xffff);   /* la palabra de suma deja el total en 0xFFFF */
}

int main(void)
{
    /* ---- catálogo */
    const wpci_chip_t *c = wpci_buscar(0x168c, 0x0032);
    confere(c && c->estado == WPCI_ESTADO_ACTIVO && c->fam == WPCI_FAM_ATH_9003 && c->cadeias == 1 && !c->ghz5, "AR9485 sigue ACTIVO (familia AR9003, 1 cadena, 2,4 GHz)");
    c = wpci_buscar(0x168c, 0x002b);
    confere(c && c->fam == WPCI_FAM_ATH_5008 && c->cadeias == 1 && !c->ghz5 && c->estado == WPCI_ESTADO_IDENTIFICADA, "AR9285 reconocido (5008, 1 cadena)");
    confere(wpci_buscar(0x168c, 0x002e) && wpci_buscar(0x168c, 0x002e)->cadeias == 2, "AR9287: 2 cadenas");
    confere(wpci_buscar(0x168c, 0x0030) && wpci_buscar(0x168c, 0x0030)->fam == WPCI_FAM_ATH_9003 && wpci_buscar(0x168c, 0x0030)->cadeias == 3, "AR9380: AR9003, 3 cadenas");
    confere(wpci_buscar(0x168c, 0x0034) && wpci_buscar(0x168c, 0x0036) && wpci_buscar(0x168c, 0x002a), "AR9462, AR9565 y AR9280 en el catálogo");
    c = wpci_buscar(0x14e4, 0x4727);
    confere(c && c->fam == WPCI_FAM_BRCM_SMAC && strstr(c->portatil, "L635"), "BCM4313 (L635) reconocido como brcmsmac");
    c = wpci_buscar(0x1814, 0x3090);
    confere(c && c->fam == WPCI_FAM_RALINK_PCI, "RT3090 reconocido");
    confere(!wpci_buscar(0x168c, 0x0000) && !wpci_buscar(0x8086, 0x0032) && !wpci_buscar(0x10ec, 0x8168) && !wpci_buscar(0xffff, 0xffff),
            "ids ajenos (Intel, Realtek, vacío) no se reconocen");
    int act = 0, dup = 0;
    for (int i = 0; i < WPCI_N_CATALOGO; i++) {
        act += wpci_catalogo[i].estado == WPCI_ESTADO_ACTIVO;
        for (int j = i + 1; j < WPCI_N_CATALOGO; j++)
            dup += wpci_catalogo[i].fabricante == wpci_catalogo[j].fabricante && wpci_catalogo[i].modelo == wpci_catalogo[j].modelo;
    }
    confere(act == 1 && dup == 0, "solo hay un ACTIVO (el AR9485) y no hay identificadores repetidos");
    printf("VAL catalogo %d\n", WPCI_N_CATALOGO);

    /* ---- SREV */
    uint32_t ver, rev;
    wpci_ath_srev(((uint32_t)WPCI_SREV_9285 << 12) | 0x200u | 0xffu, &ver, &rev);
    confere(ver == WPCI_SREV_9285 && rev == 2, "SREV largo del AR9285 rev 2");
    wpci_ath_srev(((uint32_t)WPCI_SREV_9485 << 12) | 0x000u | 0xffu, &ver, &rev);
    confere(ver == WPCI_SREV_9485 && rev == 0, "SREV largo del AR9485 (igual que ar9485.c)");
    wpci_ath_srev(0x000000c2u, &ver, &rev);
    confere(ver == WPCI_SREV_5416_PCIE && rev == 2, "SREV corto de la AR5416 PCIe");
    struct { uint32_t v; wpci_fam_t f; wpci_rom_t r; uint8_t ch; } tab[] = {
        { WPCI_SREV_9285, WPCI_FAM_ATH_5008, WPCI_ROM_4K, 1 }, { WPCI_SREV_9287, WPCI_FAM_ATH_5008, WPCI_ROM_9287, 2 },
        { WPCI_SREV_9280, WPCI_FAM_ATH_5008, WPCI_ROM_DEF, 2 }, { WPCI_SREV_9380, WPCI_FAM_ATH_9003, WPCI_ROM_9300, 3 },
        { WPCI_SREV_9485, WPCI_FAM_ATH_9003, WPCI_ROM_9300, 1 }, { WPCI_SREV_9462, WPCI_FAM_ATH_9003, WPCI_ROM_9300, 2 },
        { WPCI_SREV_9565, WPCI_FAM_ATH_9003, WPCI_ROM_9300, 1 },
    };
    bool todo = true;
    for (size_t i = 0; i < sizeof tab / sizeof tab[0]; i++) {
        wpci_ath_ver_t a = wpci_ath_version(tab[i].v);
        todo = todo && a.fam == tab[i].f && a.rom == tab[i].r && a.cadeias == tab[i].ch && a.soportado_pcie;
    }
    confere(todo, "SREV -> familia, ROM y cadenas de los siete chips PCIe");
    confere(wpci_ath_version(WPCI_SREV_9271).soportado_pcie == false && wpci_ath_version(0x999).fam == WPCI_FAM_DESCONOCIDA,
            "AR9271 (USB) no es PCIe y una versión rara es desconocida");

    /* ---- ROM 4K del AR9285 */
    uint16_t base, tam;
    confere(wpci_rom5008_geometria(WPCI_ROM_4K, &base, &tam) == 0 && base == 64 && tam == 376, "geometría 4K: base 64 palabras, 376 bytes");
    confere(wpci_rom5008_geometria(WPCI_ROM_DEF, &base, &tam) == 0 && base == 256 && tam == 3256, "geometría AR5416: base 256, 3256 bytes");
    confere(wpci_rom5008_geometria(WPCI_ROM_9287, &base, &tam) == 0 && base == 128 && tam == 727, "geometría AR9287: base 128, 727 bytes");
    confere(wpci_rom5008_geometria(WPCI_ROM_9300, &base, &tam) < 0, "la ROM AR9300 no es de esta familia");

    static uint16_t rom[2048];
    const uint8_t mac[6] = { 0x00, 0x22, 0x68, 0xaa, 0xbb, 0xcc };
    /* 4K: opCap = 11g, sin 11a, con HT20 (bit 0x20 apagado), 1 cadena, versión 14.5, rfSilent activo en GPIO 3 con polaridad,
     * deviceCap con 2^6 entradas de caché de claves */
    rom_montar(rom, 376, 0x02, 1, 1, (uint16_t)(0xe000 | 5), mac, (uint16_t)(0x0001 | 0x0002 | (3 << 2)), (uint16_t)(6 << 12));
    confere(wpci_rom5008_soma_ok(rom, 188, false), "ROM 4K de prueba: la suma de control cuadra");
    wpci_rom5008_t r;
    int rc = wpci_rom5008_cabecera((const uint8_t *)rom, 376, &r);
    confere(rc == 0 && r.eep_ver == 14 && r.eep_rev == 5 && r.g && !r.a && r.n_2g && memcmp(r.mac, mac, 6) == 0, "ROM 4K: versión 14.5, 11g sin 11a, 11n 2,4, MAC");
    confere(r.cadeias_tx == 1 && r.cadeias_rx == 1 && r.rfsilent_ativo && r.rfsilent_invertido && r.rfsilent_pino == 3 && r.kc_entradas == 64,
            "ROM 4K: 1T1R, interruptor de radio (GPIO 3, invertido), 64 claves");
    printf("VAL rom4k_mac %02x%02x%02x%02x%02x%02x\n", r.mac[0], r.mac[1], r.mac[2], r.mac[3], r.mac[4], r.mac[5]);
    /* con el orden de bytes cambiado (ROM de otra endianness) */
    static uint16_t trocada[2048];
    for (int i = 0; i < 188; i++) trocada[i] = (uint16_t)((rom[i] << 8) | (rom[i] >> 8));
    confere(wpci_rom5008_magia(WPCI_ROM_MAGIA) == 0 && wpci_rom5008_magia(0x5aa5) == 1 && wpci_rom5008_magia(0x1234) < 0, "la magia 0xa55a se reconoce nativa, cambiada y falsa");
    confere(wpci_rom5008_soma_ok(trocada, 188, true),
            "la suma de una ROM de otra endianness solo cuadra al intercambiar");
    rom[10] ^= 0x0100;
    confere(!wpci_rom5008_soma_ok(rom, 188, false), "un bit cambiado rompe la suma de control");
    rom[10] ^= 0x0100;
    /* versión mala */
    rom_montar(rom, 376, 0x02, 1, 1, (uint16_t)(0xd000 | 5), mac, 0, 0);
    confere(wpci_rom5008_cabecera((const uint8_t *)rom, 376, &r) == -2, "versión mayor distinta de 14: rechazada");
    rom_montar(rom, 376, 0x02, 1, 1, 0xe000, mac, 0, 0);
    confere(wpci_rom5008_cabecera((const uint8_t *)rom, 376, &r) == -2, "versión menor 0: rechazada");
    confere(wpci_rom5008_cabecera((const uint8_t *)rom, 20, &r) == -1, "imagen más corta que la cabecera: rechazada");
    /* AR9287 / AR5416: 2 y 3 cadenas, con 5 GHz */
    rom_montar(rom, 726, 0x03, 7, 7, (uint16_t)(0xe000 | 17), mac, 0, 0);
    rc = wpci_rom5008_cabecera((const uint8_t *)rom, 726, &r);
    confere(rc == 0 && r.cadeias_tx == 3 && r.cadeias_rx == 3 && r.a && r.g && r.kc_entradas == 128 && r.eep_rev == 17, "ROM con 3 cadenas, 11a y 11g, 128 claves por defecto");
    rom_montar(rom, 726, 0x03, 7, 7, (uint16_t)(0xe000 | 17), mac, 0, (uint16_t)(0xf000));
    wpci_rom5008_cabecera((const uint8_t *)rom, 726, &r);
    confere(r.kc_entradas == 128, "claves de la caché limitadas a 128");
    confere(wpci_rom5008_soma_ok(rom, 363, false), "ROM de 726 bytes: suma de control");

    /* ---- canales y sintetizador */
    confere(wpci_canal_freq(1) == 2412 && wpci_canal_freq(6) == 2437 && wpci_canal_freq(13) == 2472 && wpci_canal_freq(14) == 2484 &&
            wpci_canal_freq(0) == 0 && wpci_canal_freq(15) == 0, "canales 1 a 14");
    for (int ch = 1; ch <= 14; ch++) printf("VAL synth %d %u %08x\n", ch, wpci_canal_freq((uint8_t)ch), wpci_ar9280_synth_2g(wpci_canal_freq((uint8_t)ch)));
    confere((wpci_ar9280_synth_2g(2412) & 0x30000000u) == 0x30000000u, "el sintetizador activa BMODE y FRACMODE");
    confere((wpci_ar9280_synth_2g(2484) & 0x0fffffffu) == (uint32_t)((2484u << 16) / 15), "palabra del sintetizador del canal 14");

    printf("%d ok, %d falhas\n", oks, falhas);
    if (falhas == 0) printf("TUDO OK\n");
    return falhas != 0;
}
