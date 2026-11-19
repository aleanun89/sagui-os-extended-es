// SPDX-License-Identifier: GPL-2.0-or-later
/* atom_sintetica.h - una BIOS ATOM de juguete, construida a mano, para probar sin tarjeta (D399).
 *
 * La usan la prueba del PC (tests/gpu_energia_unidad.c) y el driver simulado del kernel
 * (--gpu-energia-simulada, h401): el intérprete ATOM de verdad (kernel/terceros/mit/atom/atom.c)
 * corre estas tablas contra registros de mentira. NO es la BIOS de ningún fabricante: todo lo que hay
 * aquí está escrito para estas pruebas (las firmas y desplazamientos son los del formato ATOM).
 *
 * Qué lleva:
 *   - firmas (55 AA, « 761295520», «ATOM»), PCIR 1002:7145 y el nombre «SAGUI-SINTETICA»;
 *   - FirmwareInfo 1.4: 445 / 250 MHz por defecto, referencia 27 MHz, arranque a 1200 mV;
 *   - PowerPlayInfo 2 con tres estados: 0 «sin pantalla» 100/100 MHz, 1 batería 200/200 MHz con la
 *     tensión baja por GPIO (pin 5: registro 0x7C0, bit 3, activo alto) y 2 por defecto 445/250 MHz;
 *   - GPIO_Pin_LUT con ese pin y VRAM_UsageByFirmware (4 KB);
 *   - órdenes: GetEngineClock/GetMemoryClock (leen los PLL de mentira 0x30/0x31), SetEngineClock y
 *     SetMemoryClock (los escriben, la primera por una llamada a ComputeMemoryEnginePLL, que copia el
 *     parámetro desplazado a WS y de WS al PLL 0x32, y escribe además el registro 0x0400), con un
 *     DELAY de 10 us.
 * Variantes, solo por debajo del reloj por defecto (para que la vuelta a la BIOS salga bien; COMPARE y
 * JUMP_AboveOrEqual): SINT_FALLO (SetEngineClock escribe 20 MHz de más: la comprobación tiene que verlo) y
 * SINT_CUELGUE (SetEngineClock salta a sí mismo para siempre: el vigilante del intérprete lo corta). */
#ifndef SAGUI_ATOM_SINTETICA_H
#define SAGUI_ATOM_SINTETICA_H
#ifdef SAGUI_KERNEL
#include "tipos.h"
#else
#include <stdint.h>
#endif

#define SINT_TAM       0x2000
#define SINT_NORMAL    0
#define SINT_FALLO     1
#define SINT_CUELGUE   2
#define SINT_PLL_SCLK  0x30      /* PLL de mentira: reloj del motor en 10 kHz */
#define SINT_PLL_MCLK  0x31
#define SINT_PLL_COPIA 0x32      /* lo que dejó ComputeMemoryEnginePLL */
#define SINT_REG_ECO   0x0400    /* MMIO (bytes) que escribe SetEngineClock */
#define SINT_GPIO_REG  0x07C0
#define SINT_GPIO_BIT  3

static inline void sint_p8(uint8_t *b, uint32_t o, uint32_t v) { b[o] = (uint8_t)v; }
static inline void sint_p16(uint8_t *b, uint32_t o, uint32_t v) { b[o] = (uint8_t)v; b[o + 1] = (uint8_t)(v >> 8); }
static inline void sint_p32(uint8_t *b, uint32_t o, uint32_t v) { sint_p16(b, o, v & 0xFFFF); sint_p16(b, o + 2, v >> 16); }

/* Una tabla de órdenes en "o" con su código; devuelve el siguiente sitio libre (alineado a 16). */
static inline uint32_t sint_cmd(uint8_t *b, uint32_t o, const uint8_t *cod, uint32_t n, uint32_t ws, uint32_t ps)
{
    sint_p16(b, o, 6 + n);
    sint_p8(b, o + 2, 1); sint_p8(b, o + 3, 1);
    sint_p8(b, o + 4, ws); sint_p8(b, o + 5, ps);
    for (uint32_t i = 0; i < n; i++) b[o + 6 + i] = cod[i];
    return (o + 6 + n + 15) & ~15u;
}

/* Construye la imagen en "b" (SINT_TAM bytes). Devuelve el tamaño. */
static inline uint32_t atom_sintetica(uint8_t *b, int variante)
{
    for (uint32_t i = 0; i < SINT_TAM; i++) b[i] = 0;
    sint_p16(b, 0, 0xAA55);
    sint_p8(b, 2, SINT_TAM / 512);
    static const char ati[] = " 761295520";
    for (uint32_t i = 0; i < sizeof ati - 1; i++) b[0x30 + i] = (uint8_t)ati[i];
    /* PCIR */
    sint_p16(b, 0x18, 0x60);
    b[0x60] = 'P'; b[0x61] = 'C'; b[0x62] = 'I'; b[0x63] = 'R';
    sint_p16(b, 0x64, 0x1002); sint_p16(b, 0x66, 0x7145);
    /* cabecera ATOM en 0x80 */
    sint_p16(b, 0x48, 0x80);
    sint_p16(b, 0x80, 0x24); sint_p8(b, 0x82, 1); sint_p8(b, 0x83, 1);
    b[0x84] = 'A'; b[0x85] = 'T'; b[0x86] = 'O'; b[0x87] = 'M';
    sint_p16(b, 0x80 + 0x10, 0xE0);          /* mensaje del arranque */
    sint_p16(b, 0x80 + 0x1E, 0x100);         /* tabla maestra de órdenes */
    sint_p16(b, 0x80 + 0x20, 0x200);         /* tabla maestra de datos */
    static const char nome[] = "\r\nSAGUI-SINTETICA";
    for (uint32_t i = 0; i < sizeof nome - 1; i++) b[0xE0 + i] = (uint8_t)nome[i];
    sint_p16(b, 0x100, 4 + 2 * 81); sint_p8(b, 0x102, 1); sint_p8(b, 0x103, 1);
    sint_p16(b, 0x200, 4 + 2 * 34); sint_p8(b, 0x202, 1); sint_p8(b, 0x203, 1);

    /* FirmwareInfo 1.4 en 0x300 (0x58 bytes) */
    uint32_t fw = 0x300;
    sint_p16(b, 0x200 + 4 + 2 * 4, fw);
    sint_p16(b, fw, 0x58); sint_p8(b, fw + 2, 1); sint_p8(b, fw + 3, 4);
    sint_p32(b, fw + 0x08, 44500); sint_p32(b, fw + 0x0C, 25000);
    sint_p32(b, fw + 0x24, 50000); sint_p32(b, fw + 0x28, 30000);
    sint_p8(b, fw + 0x2C, 105);
    sint_p16(b, fw + 0x2E, 1200);
    sint_p16(b, fw + 0x52, 2700);

    /* VRAM_UsageByFirmware en 0x380: 4 KB */
    sint_p16(b, 0x200 + 4 + 2 * 11, 0x380);
    sint_p16(b, 0x380, 12); sint_p8(b, 0x382, 1); sint_p8(b, 0x383, 1);
    sint_p32(b, 0x384, 0x07FFF000); sint_p16(b, 0x388, 4);

    /* PowerPlayInfo 2 en 0x400: 3 entradas de 21 bytes */
    uint32_t pp = 0x400;
    sint_p16(b, 0x200 + 4 + 2 * 15, pp);
    sint_p16(b, pp, 10 + 3 * 21); sint_p8(b, pp + 2, 2); sint_p8(b, pp + 3, 1);
    sint_p8(b, pp + 8, 21); sint_p8(b, pp + 9, 3);
    struct { uint32_t misc, sclk, mclk; uint8_t vid; } st[3] = {
        { 0x00040000u /* ahorro */, 10000, 10000, 0 },
        { 0x00001000u | 0x4u | 0x8u /* batería, tensión por GPIO, activo alto */, 20000, 20000, 5 },
        { 0x00008000u | 0x08000000u /* por defecto, 3D */, 44500, 25000, 0 },
    };
    for (uint32_t i = 0; i < 3; i++) {
        uint32_t p = pp + 10 + i * 21;
        sint_p32(b, p, st[i].misc);
        sint_p32(b, p + 8, st[i].sclk);
        sint_p32(b, p + 12, st[i].mclk);
        sint_p8(b, p + 16, st[i].vid);
        sint_p8(b, p + 20, 16);
    }
    /* GPIO_Pin_LUT en 0x480: dos pines (el 5 es el de la tensión) */
    sint_p16(b, 0x200 + 4 + 2 * 12, 0x480);
    sint_p16(b, 0x480, 4 + 2 * 4); sint_p8(b, 0x482, 1); sint_p8(b, 0x483, 1);
    sint_p16(b, 0x484, 0x1F4); sint_p8(b, 0x486, 1); sint_p8(b, 0x487, 9);
    sint_p16(b, 0x488, SINT_GPIO_REG / 4); sint_p8(b, 0x48A, SINT_GPIO_BIT); sint_p8(b, 0x48B, 5);

    /* Órdenes desde 0x600. Operandos: [orden][atributo][destino][origen]; atributo = tipo del origen
     * (bits 2:0) | alineación (5:3) | alineación del destino (7:6); dword = 0. */
    uint32_t o = 0x600;
    /* GetEngineClock: MOVE PS[0] <- PLL[0x30]; EOT */
    static const uint8_t get_s[] = { 2, 0x06, 0, SINT_PLL_SCLK, 91 };
    static const uint8_t get_m[] = { 2, 0x06, 0, SINT_PLL_MCLK, 91 };
    sint_p16(b, 0x100 + 4 + 2 * 48, o); o = sint_cmd(b, o, get_s, sizeof get_s, 0, 0);
    sint_p16(b, 0x100 + 4 + 2 * 47, o); o = sint_cmd(b, o, get_m, sizeof get_m, 0, 0);
    /* ComputeMemoryEnginePLL (60), llamada con PS desplazado 4 palabras: MOVE WS[0] <- PS[0]; MOVE PLL[0x32] <- WS[0] */
    static const uint8_t comp[] = { 3, 0x01, 0, 0, 5, 0x02, SINT_PLL_COPIA, 0, 91 };
    sint_p16(b, 0x100 + 4 + 2 * 60, o); o = sint_cmd(b, o, comp, sizeof comp, 4, 0);
    /* SetEngineClock: MOVE PS[4] <- PS[0]; CALL_TABLE 60; MOVE PLL[0x30] <- PS[0]; MOVE REG[0x100] <- PS[0];
     * DELAY_MicroSec 10; (fallo: ADD PLL[0x30] <- IMM 2000); EOT. La tabla dice PS = 16 bytes (4 palabras):
     * la llamada ve PS[4] como su PS[0]. */
    uint8_t set_s[40];
    uint32_t n = 0;
    set_s[n++] = 2; set_s[n++] = 0x01; set_s[n++] = 4; set_s[n++] = 0;          /* MOVE PS[4] <- PS[0] */
    set_s[n++] = 82; set_s[n++] = 60;                                          /* CALL_TABLE 60 */
    set_s[n++] = 5; set_s[n++] = 0x01; set_s[n++] = SINT_PLL_SCLK; set_s[n++] = 0;   /* MOVE PLL[0x30] <- PS[0] */
    set_s[n++] = 1; set_s[n++] = 0x01; set_s[n++] = (SINT_REG_ECO / 4) & 0xFF; set_s[n++] = (SINT_REG_ECO / 4) >> 8; set_s[n++] = 0;
    set_s[n++] = 81; set_s[n++] = 10;                                          /* DELAY_MicroSec 10 */
    if (variante != SINT_NORMAL) {
        /* Solo por debajo del reloj por defecto (la vuelta a la BIOS tiene que salir bien):
         * COMPARE PS[0], IMM 44500; JUMP_AboveOrEqual fin; y luego el fallo o el cuelgue. */
        set_s[n++] = 61; set_s[n++] = 0x05; set_s[n++] = 0;
        set_s[n++] = 44500 & 0xFF; set_s[n++] = 44500 >> 8; set_s[n++] = 0; set_s[n++] = 0;
        uint32_t salto = n;
        set_s[n++] = 72; set_s[n++] = 0; set_s[n++] = 0;                       /* destino, abajo */
        if (variante == SINT_FALLO) {                                          /* ADD PLL[0x30] <- IMM 2000 */
            set_s[n++] = 47; set_s[n++] = 0x05; set_s[n++] = SINT_PLL_SCLK;
            set_s[n++] = 2000 & 0xFF; set_s[n++] = 2000 >> 8; set_s[n++] = 0; set_s[n++] = 0;
        } else {                                                               /* JUMP a sí mismo */
            uint32_t aqui = 6 + n;
            set_s[n++] = 67; set_s[n++] = (uint8_t)aqui; set_s[n++] = (uint8_t)(aqui >> 8);
        }
        set_s[salto + 1] = (uint8_t)(6 + n); set_s[salto + 2] = (uint8_t)((6 + n) >> 8);
    }
    set_s[n++] = 91;
    sint_p16(b, 0x100 + 4 + 2 * 10, o); o = sint_cmd(b, o, set_s, n, 0, 16);
    /* SetMemoryClock: MOVE PLL[0x31] <- PS[0]; EOT */
    static const uint8_t set_m[] = { 5, 0x01, SINT_PLL_MCLK, 0, 91 };
    sint_p16(b, 0x100 + 4 + 2 * 11, o); o = sint_cmd(b, o, set_m, sizeof set_m, 0, 0);
    (void)o;
    return SINT_TAM;
}

#endif
