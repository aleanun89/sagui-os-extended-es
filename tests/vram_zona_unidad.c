// SPDX-License-Identifier: GPL-2.0-or-later
/* vram_zona_unidad.c - prueba en el PC de dónde va la zona de trabajo de la GPU en
 * la VRAM (kernel/dispositivos/vram_zona_logica.h, D386).
 *
 *   gcc -O1 -Wall -I kernel/dispositivos -o vram_zona_unidad tests/vram_zona_unidad.c
 *
 * El caso principal son los registros que dio el Amilo (Mobility X1400, RV515
 * 1002:7145, 128 MB) con SaguiOS 8.5.4.9: D1 con la pantalla del VBE en
 * c0000000 (1024x768, pitch 1024) y D2 activo en c0100000 con pitch 1280. Antes
 * la zona 3D iba a c0400000 y se rechazaba por estar «cerca» de D2. */
#include <stdio.h>
#include "vram_zona_logica.h"

static int falhas, feitas;
#define VER(c, ...) do { feitas++; if (!(c)) { falhas++; printf("FALLO: " __VA_ARGS__); printf("\n"); } } while (0)
#define MB (1u << 20)
#define ZMIN (2u * MB)
#define ZMAX (16u * MB)

int main(void)
{
    uint64_t ini;
    uint32_t tam;

    /* Tamaño de una superficie desde sus registros. */
    VER(avivo_superficie_bytes(2, 1024, 768, 0, 0, (1024u << 16) | 768) == 1024u * 4 * 768, "D1 1024x768x32");
    VER(avivo_superficie_bytes(2, 1280, 800, 0, 0, (1280u << 16) | 800) == 1280u * 4 * 800, "D2 1280x800x32");
    VER(avivo_superficie_bytes(1, 1280, 1024, 0, 0, 0) == 1280u * 2 * 1024, "16 bits, sin ventana");
    VER(avivo_superficie_bytes(2, 1024, 600, 0, 100, (1024u << 16) | 768) == 1024u * 4 * 868,
        "la ventana pasa de Y_END: manda la ventana");
    VER(avivo_superficie_bytes(2, 1280, 0, 0, 0, 0) == VRAM_MARGEM_DESCONHECIDA, "sin alto: margen de 8 MB");
    VER(avivo_superficie_bytes(2, 0, 800, 0, 0, 0) == VRAM_MARGEM_DESCONHECIDA, "sin pitch: margen de 8 MB");
    VER(avivo_superficie_bytes(2, 1280, 0x3FFF, 0, 0, 0) == VRAM_MARGEM_DESCONHECIDA, "alto absurdo: margen de 8 MB");

    /* El Amilo. Pantalla del VBE: 4096 bytes por línea, 768 + 2 * 64 líneas (con la zona del 2D). */
    uint64_t lim = 128u * MB;
    uint64_t fim_tela = 4096u * (768 + 128);
    {
        /* D2 de 1280x800: acaba en 1 MB + 4000 KB = 4,9 MB -> la zona en 5 MB. */
        vram_ocupado_t oc[] = { { 0, fim_tela }, { 0, 1024u * 4 * 768 }, { MB, MB + avivo_superficie_bytes(2, 1280, 800, 0, 0, 0) } };
        VER(vram_zona_buscar(oc, 3, fim_tela, lim, ZMIN, ZMAX, MB, &ini, &tam), "Amilo con D2 1280x800: hay zona");
        VER(ini == 5u * MB && tam == ZMAX, "Amilo con D2 1280x800: zona en %llu KB de %u KB (se esperaba 5120 KB, 16 MB)",
            (unsigned long long)(ini >> 10), tam >> 10);
        VER(vram_zona_pisa(oc, 3, ini, tam) < 0, "Amilo: la zona no pisa nada");
        /* Lo que hacía antes: c0400000 pisa D2. */
        VER(vram_zona_pisa(oc, 3, 4u * MB, ZMAX) == 2, "la zona vieja (4 MB) pisaba D2");
    }
    {
        /* D2 sin tamaño conocido: margen de 8 MB -> zona en 9 MB. */
        vram_ocupado_t oc[] = { { 0, fim_tela }, { MB, MB + VRAM_MARGEM_DESCONHECIDA } };
        VER(vram_zona_buscar(oc, 2, fim_tela, lim, ZMIN, ZMAX, MB, &ini, &tam) && ini == 9u * MB && tam == ZMAX,
            "Amilo con D2 de tamaño desconocido: zona en 9 MB");
    }
    {
        /* Cursores por hardware dentro: se saltan. */
        vram_ocupado_t oc[] = { { 0, fim_tela }, { 4u * MB, 4u * MB + 0x4000 }, { 6u * MB + 0x8000, 6u * MB + 0xC000 } };
        VER(vram_zona_buscar(oc, 3, fim_tela, lim, ZMIN, ZMAX, MB, &ini, &tam) && ini == 7u * MB && tam == ZMAX,
            "cursores en 4 MB y 6 MB: zona en 7 MB (%llu KB)", (unsigned long long)(ini >> 10));
        VER(vram_zona_pisa(oc, 3, ini, tam) < 0, "y no los pisa");
    }
    {
        /* Una superficie más arriba recorta la zona por arriba si deja al menos 2 MB. */
        vram_ocupado_t oc[] = { { 0, fim_tela }, { 8u * MB, 12u * MB } };
        VER(vram_zona_buscar(oc, 2, fim_tela, lim, ZMIN, ZMAX, MB, &ini, &tam) && ini == 4u * MB && tam == 4u * MB,
            "superficie en 8 MB: zona de 4 MB en 4 MB (%llu KB, %u KB)", (unsigned long long)(ini >> 10), tam >> 10);
        VER(vram_zona_pisa(oc, 2, ini, tam) < 0, "y no la pisa");
    }
    {
        /* Hueco de menos de 2 MB entre dos superficies: se salta al siguiente. */
        vram_ocupado_t oc[] = { { 0, fim_tela }, { 5u * MB, 6u * MB } };
        VER(vram_zona_buscar(oc, 2, fim_tela, lim, ZMIN, ZMAX, MB, &ini, &tam) && ini == 6u * MB && tam == ZMAX,
            "hueco de 1 MB: zona en 6 MB");
    }
    {
        /* Tarjeta de 16 MB llena: no hay sitio. */
        vram_ocupado_t oc[] = { { 0, fim_tela }, { 4u * MB, 15u * MB } };
        VER(!vram_zona_buscar(oc, 2, fim_tela, 16u * MB, ZMIN, ZMAX, MB, &ini, &tam), "16 MB llenos: sin zona");
        /* Con 32 MB: detrás de la superficie, hasta el final. */
        VER(vram_zona_buscar(oc, 2, fim_tela, 32u * MB, ZMIN, ZMAX, MB, &ini, &tam) && ini == 15u * MB && tam == 16u * MB,
            "32 MB: zona en 15 MB de 16 MB");
        VER(vram_zona_buscar(oc, 2, fim_tela, 20u * MB, ZMIN, ZMAX, MB, &ini, &tam) && ini == 15u * MB && tam == 5u * MB,
            "20 MB: zona en 15 MB de 5 MB");
    }
    {
        /* Regiones desordenadas y encadenadas: cada salto lleva a otra. */
        vram_ocupado_t oc[] = { { 9u * MB, 11u * MB }, { 7u * MB, 9u * MB }, { 0, fim_tela }, { 5u * MB, 7u * MB }, { 3u * MB, 5u * MB } };
        VER(vram_zona_buscar(oc, 5, fim_tela, lim, ZMIN, ZMAX, MB, &ini, &tam) && ini == 11u * MB && tam == ZMAX,
            "regiones encadenadas: zona en 11 MB (%llu KB)", (unsigned long long)(ini >> 10));
    }
    {
        /* Una sola pantalla, como antes (la X1400 sin D2): la zona justo detrás, en 4 MB. */
        vram_ocupado_t oc[] = { { 0, fim_tela } };
        VER(vram_zona_buscar(oc, 1, fim_tela, lim, ZMIN, ZMAX, MB, &ini, &tam) && ini == 4u * MB && tam == ZMAX,
            "una sola pantalla: zona en 4 MB, como antes");
        /* Región vacía o al revés: no cuenta. */
        vram_ocupado_t oc2[] = { { 0, fim_tela }, { 6u * MB, 6u * MB }, { 7u * MB, 5u * MB } };
        VER(vram_zona_buscar(oc2, 3, fim_tela, lim, ZMIN, ZMAX, MB, &ini, &tam) && ini == 4u * MB, "regiones vacías no cuentan");
    }
    {
        /* La tabla de la GART en el Amilo (dbg2, 2026-11-19: «no caben los 16 KB»): pantalla con sus dos zonas,
         * D1 3072 KB en 0, D2 3840 KB en 1 MB (CRTC apagado, imagen activa), cursores apagados (en MC 0: no se
         * apuntan), VRAM de 128 MB, zona 3D de gpu3d_r500 con sus 16 MB. La tabla, detrás de todo: en 21 MB
         * (MC c1500000). */
        vram_ocupado_t oc[5] = { { 0, fim_tela }, { 0, 3072u * 1024 }, { MB, MB + 3840u * 1024 } };
        int n = 3;
        VER(vram_zona_buscar(oc, n, fim_tela, lim, ZMIN, ZMAX, MB, &ini, &tam) && ini == 5u * MB, "GART: zona 3D del Amilo en 5 MB");
        oc[n].ini = ini; oc[n++].fim = ini + ZMAX;
        uint64_t t = 0;
        VER(vram_tabela_gart(oc, n, lim, 16384, &t) && t == 21u * MB, "GART: tabla del Amilo en 21 MB (%llu KB)",
            (unsigned long long)(t >> 10));
        VER(vram_zona_pisa(oc, n, t, 16384) < 0, "GART: la tabla no pisa nada");
        /* Sin sitio detrás: no se pone delante. */
        VER(!vram_tabela_gart(oc, n, 21u * MB + 8192, 16384, &t), "GART: sin sitio detrás de todo, no hay tabla");
        /* Las regiones vacías (cursor apagado a 0) no cuentan. */
        vram_ocupado_t oc2[] = { { 0, fim_tela }, { 0, 0 } };
        VER(vram_tabela_gart(oc2, 2, lim, 16384, &t) && t == fim_tela, "GART: una pantalla sola, tabla justo detrás");
    }
    {
        /* D387, cuarta tanda: la zona 3D de las R5xx hasta 8 MB antes del final de la VRAM. En el Amilo (128 MB):
         * de 5 a 120 MB (115 MB) y la tabla de la GART justo detrás, en 120 MB (MC c7800000), con 8 MB de cola. */
        vram_ocupado_t oc[5] = { { 0, fim_tela }, { 0, 3072u * 1024 }, { MB, MB + 3840u * 1024 } };
        int n = 3;
        uint64_t z = 0, t = 0;
        uint32_t zt = 0;
        VER(r5_zona3d_buscar(oc, n, fim_tela, 128u * MB, &z, &zt) && z == 5u * MB && zt == 115u * MB,
            "zona 3D grande del Amilo: 115 MB desde 5 MB (%llu KB, %u KB)", (unsigned long long)(z >> 10), zt >> 10);
        oc[n].ini = z; oc[n++].fim = z + zt;
        VER(vram_tabela_gart(oc, n, 128u * MB, 16384, &t) && t == 120u * MB, "GART detrás de la zona grande: 120 MB");
        VER(t + 16384 <= 128u * MB && vram_zona_pisa(oc, n, t, 16384) < 0, "la tabla cabe en la cola y no pisa nada");
        /* 16 MB de VRAM: 16 - 8 = 8 MB útiles, la pantalla llega a 4 MB: zona de 4 MB. */
        vram_ocupado_t o2[] = { { 0, fim_tela } };
        VER(r5_zona3d_buscar(o2, 1, fim_tela, 16u * MB, &z, &zt) && z == 4u * MB && zt == 4u * MB, "VRAM de 16 MB: zona de 4 MB");
        VER(!r5_zona3d_buscar(o2, 1, fim_tela, 8u * MB, &z, &zt), "VRAM de 8 MB: sin zona");
    }
    printf("vram_zona_unidad: %d comprobaciones, %d fallos\n", feitas, falhas);
    return falhas ? 1 : 0;
}
