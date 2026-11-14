// SPDX-License-Identifier: GPL-2.0-or-later
/* acel_nv_anfitriao.c - pruebas en el anfitrión de la lógica del acelerador 2D
 * de las GeForce 6/7 (D224): formato de las órdenes del búfer (cabeceras,
 * puntos, tamaños, salto), las palabras que atan los objetos, la tabla hash
 * y los objetos de PRAMIN, el anillo, el pitch del CRTC, dónde va el búfer y
 * los modelos. Las compila y ejecuta tests/h226_acel2d_nvidia.py con el MISMO
 * kernel/dispositivos/acel_nv_logica.h (y kernel/terceros/mit/nv/nv_2d.h) que
 * usa el kernel. Imprime "ok ..." o "FALHA ..." y, al final, "TUDO OK". */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../../kernel/dispositivos/acel_nv_logica.h"
#include "../../kernel/terceros/mit/nv/nv_2d.h"

static int falhas, oks;

static void confere(bool c, const char *o_que)
{
    if (c) { oks++; printf("ok %s\n", o_que); }
    else { falhas++; printf("FALHA %s\n", o_que); }
}

static void cabecalhos(void)
{
    confere(nv_cabecalho(BLIT_POINT_SRC, 3) == 0x000CA300u, "BLIT_POINT_SRC con 3 palabras: 0x000CA300");
    confere(nv_cabecalho(RECT_SOLID_RECTS(0), 2) == 0x0008C400u, "RECT_SOLID_RECTS(0) con 2: 0x0008C400");
    confere(nv_cabecalho(RECT_SOLID_COLOR, 1) == 0x0004C3FCu, "RECT_SOLID_COLOR con 1: 0x0004C3FC");
    confere(nv_cabecalho(SURFACE_FORMAT, 4) == 0x00100300u, "SURFACE_FORMAT con 4: 0x00100300");
    confere(nv_cabecalho(ROP_SET, 1) == 0x00042300u, "ROP_SET con 1: 0x00042300 (subcanal 1)");
    confere(nv_cabecalho(PATTERN_COLOR_0, 4) == 0x00104310u, "PATTERN_COLOR_0 con 4: 0x00104310");
    confere(RECT_SOLID_RECTS(1) == 0xC408, "RECT_SOLID_RECTS(1) = 0xC408");
    confere(nv_cabecalho_valido(BLIT_POINT_SRC, 3) && nv_cabecalho_valido(RECT_SOLID_RECTS(0), 2) &&
            nv_cabecalho_valido(SURFACE_FORMAT, 4) && nv_cabecalho_valido(LINE_FORMAT, 1),
            "las cabeceras que usa el driver son válidas");
    confere(!nv_cabecalho_valido(0xA302, 1), "método sin alinear: no");
    confere(!nv_cabecalho_valido(BLIT_SIZE, 0) && !nv_cabecalho_valido(BLIT_SIZE, 2048), "0 o 2048 palabras: no");
    confere(nv_cabecalho_valido(BLIT_SIZE, 2047), "2047 palabras: sí");
    /* El subcanal de cada método (bits 15:13). */
    confere((SURFACE_FORMAT >> 13) == 0 && (ROP_SET >> 13) == 1 && (PATTERN_FORMAT >> 13) == 2 &&
            (LINE_FORMAT >> 13) == 4 && (BLIT_POINT_SRC >> 13) == 5 && (RECT_FORMAT >> 13) == 6,
            "subcanales: SURFACE 0, ROP 1, PATTERN 2, LINE 4, BLIT 5, RECT 6");
    confere(nv_salto(0) == 0x20000000u && nv_salto(0) == NV_DMA_SALTO, "el salto al principio: 0x20000000");
    confere(nv_salto(32) == 0x20000020u, "salto al byte 32");
}

static void pontos(void)
{
    confere(nv_ponto(3, 5) == 0x00050003u, "IMAGE_BLIT: punto (3,5) = y<<16 | x");
    confere(nv_tamanho(1016, 44) == ((44u << 16) | 1016u), "IMAGE_BLIT: tamaño = alto<<16 | ancho");
    confere(nv_rect_ponto(3, 5) == 0x00030005u, "RECT: punto (3,5) = x<<16 | y (al revés que el blit)");
    confere(nv_rect_tamanho(10, 20) == 0x000A0014u, "RECT: tamaño = ancho<<16 | alto");
    confere(nv_pitch_surface(4096) == 0x10001000u, "SURFACE_PITCH: origen y destino 4096");
    confere(nv_coord_valida(0, 0, 1, 1) && nv_coord_valida(0, 831, 1024, 1), "coordenadas de la zona de prueba: sí");
    confere(!nv_coord_valida(-1, 0, 1, 1) && !nv_coord_valida(0, 0, 0, 1) && !nv_coord_valida(0x7FFF, 0, 1, 1),
            "negativas, vacías o de más de 15 bits: no");
}

static void ligacoes_e_pramin(void)
{
    static const uint8_t classe[8] = { 0x62, 0x43, 0x44, 0x19, 0x5C, 0x9F, 0x4A, 0x77 };
    bool ok_lig = true, ok_hash = true, ok_obj = true;
    int n = (int)(sizeof nv40_pramin / sizeof nv40_pramin[0]);
    for (uint32_t s = 0; s < 8; s++) {
        if (nv_dma_ligacoes[2 * s] != nv_cabecalho(s << 13, 1) || nv_dma_ligacoes[2 * s + 1] != 0x80000010u + s) ok_lig = false;
        uint32_t h = nv_ramht_hash(0x80000010u + s, 9, 0);   /* tabla de 4 KB: 512 entradas, 9 bits */
        /* La entrada: identificador y contexto (instancia >> 4, motor 1 = PGRAPH). */
        uint32_t ih = 0, ctx = 0;
        bool achou_h = false, achou_c = false;
        for (int i = 0; i < n; i++) {
            if (nv40_pramin[i].indice == h / 4) { ih = nv40_pramin[i].valor; achou_h = true; }
            if (nv40_pramin[i].indice == h / 4 + 1) { ctx = nv40_pramin[i].valor; achou_c = true; }
        }
        if (!achou_h || !achou_c || ih != 0x80000010u + s || (ctx >> 20) != 1) { ok_hash = false; continue; }
        /* El objeto: en PRAMIN 0x710000 = instancia 0x10000. */
        uint32_t inst = (ctx & 0xFFFFu) << 4, idx = (inst - 0x10000u) / 4;
        bool achou_o = false;
        for (int i = 0; i < n; i++)
            if (nv40_pramin[i].indice == idx && (nv40_pramin[i].valor & 0xFF) == classe[s]) achou_o = true;
        if (!achou_o) ok_obj = false;
    }
    confere(ok_lig, "las 16 palabras atan 0x80000010..17 a los subcanales 0..7");
    confere(ok_hash, "cada identificador está en su entrada de la tabla hash (8 bytes por entrada, sin choques)");
    confere(ok_obj, "cada entrada apunta a su objeto: SURFACES_2D, ROP, PATTERN, CLIP, LINE, IMAGE_BLIT, RECT, SCALED");
    /* El objeto DMA del búfer: 0x084C, 32 KB; el de la VRAM en 0x0800 (0x12000 >> 4 = 0x1200). */
    bool pb = false, vram = false;
    for (int i = 0; i < n; i++) {
        if (nv40_pramin[i].indice == 0x084C && nv40_pramin[i].valor == 0x3002) pb = true;
        if (nv40_pramin[i].indice == 0x0800 && nv40_pramin[i].valor == 0x3000) vram = true;
    }
    confere(pb && (0x12130u >> 4) == 0x1213, "el objeto DMA del búfer en 0x12130 (PFIFO 0x322C = 0x1213)");
    confere(vram && (0x12000u >> 4) == 0x1200, "el objeto DMA de la VRAM en 0x12000 (contextos 0x1200)");
    bool fora = true;
    for (int i = 0; i < n; i++)
        if (nv40_pramin[i].indice == NV40_PRAMIN_DMA_VRAM_LIMITE || nv40_pramin[i].indice == NV40_PRAMIN_PB_ENDERECO ||
            nv40_pramin[i].indice * 4u + 4 > 0x3000) fora = false;
    confere(fora, "la tabla no trae los dos valores de la tarjeta y cabe en la ventana de 12 KB");
    bool sem_ramfc = true;
    for (int i = 0; i < n; i++)
        if (nv40_pramin[i].indice >= 0x400 && nv40_pramin[i].indice < 0x500) sem_ramfc = false;
    confere(sem_ramfc, "los objetos no pisan RAMFC/RAMRO (0x11000-0x113FF)");
}

/* El anillo como lo usa el driver: órdenes de 4 y 5 palabras; al final, salto. */
static void anel(void)
{
    confere(nv_anel_cabe(NV_ANEL - 6, 4) && !nv_anel_cabe(NV_ANEL - 5, 4), "cabe si queda sitio para el salto");
    static uint32_t buf[NV_ANEL];
    uint32_t atual = NV_ANEL_SKIPS + 16 + 16, saltos = 0, max = 0, ultimo = 0;
    bool ok = true;
    for (int k = 0; k < 20000; k++) {
        uint32_t n = (k & 1) ? 4 : 5;
        if (!nv_anel_cabe(atual, n)) {
            if (atual >= NV_ANEL) { ok = false; break; }
            buf[atual] = nv_salto(0);
            ultimo = atual;
            atual = NV_ANEL_SKIPS;
            saltos++;
        }
        for (uint32_t i = 0; i < n; i++) {
            if (atual >= NV_ANEL - 1) ok = false;
            buf[atual++] = 0;
        }
        if (atual > max) max = atual;
    }
    confere(ok && max <= NV_ANEL - 1 && saltos > 5 && buf[ultimo] == 0x20000000u && ultimo >= NV_ANEL - 7, "20000 órdenes: nunca se pasa del búfer y salta al principio");
    confere(NV_ANEL * 4 == 32 * 1024 && NV_ANEL - 1 == NV_DMA_MAX && NV_ANEL_SKIPS == SKIPS,
            "32 KB, como dmaMax y SKIPS del original");
}

static void crtc(void)
{
    confere(nv_pitch_crtc(0x00, 0x40, 0x00) == 4096, "CR13 0, CR19 0x40: 4096 bytes (1024 a 32 bits)");
    confere(nv_pitch_crtc(0xB0, 0x40, 0x00) == 5504, "CR13 0xB0, CR19 0x40: 5504 bytes");
    confere(nv_pitch_crtc(0x40, 0x20, 0x00) == 2560, "CR13 0x40, CR19 0x20: 2560 bytes (640)");
    confere(nv_pitch_crtc(0x00, 0x00, 0x40) == 16384, "CR42 bit 6: bit 11 del offset");
    confere(nv_pitch_crtc(0x40, 0x3F, 0xBF) == 2560, "los demás bits de CR19 y CR42 no cuentan");
}

static void memoria(void)
{
    confere(nv_vram_bytes(0x04000000u) == 64u << 20, "PFB 0x10020c 0x04000000: 64 MB");
    confere(nv_vram_bytes(0x0400000Fu) == 64u << 20, "los bits bajos no cuentan");
    confere(nv_vram_bytes(0x20000000u) == 256u << 20, "512 MB: tope de 256 MB, como X.Org");
    uint32_t pb = 0;
    int r = nv_colocar_pb(0, 4096, 768, 64, 64u << 20, 64u << 20, &pb);
    confere(r == NV_PB_OK && pb == 0x340000, "1024x768: el búfer en 0x340000 (debajo de las 64 filas de la zona)");
    r = nv_colocar_pb(0, 5504, 768, 64, 128u << 20, 8u << 20, &pb);
    confere(r == NV_PB_OK && (pb & 4095) == 0 && pb >= 5504u * 832, "1366x768 con pitch 5504: alineado a 4 KB, tras la zona");
    r = nv_colocar_pb(0x100000, 4096, 768, 64, 64u << 20, 64u << 20, &pb);
    confere(r == NV_PB_OK && pb == 0x440000, "la pantalla en 1 MB: el búfer se desplaza igual");
    confere(nv_colocar_pb(0, 4096, 768, 64, 64u << 20, 3u << 20, &pb) == NV_PB_SEM_VBE, "sin memoria del VBE para él: no");
    confere(nv_colocar_pb(0, 4096, 768, 64, 4u << 20, 4u << 20, &pb) == NV_PB_RAMIN, "4 MB de VRAM: pisaría PRAMIN, no");
    confere(nv_colocar_pb(0x80, 4096, 768, 64, 64u << 20, 64u << 20, &pb) == NV_PB_ALINHAMENTO, "pantalla sin alinear a 256: no");
}

static void modelos(void)
{
    const nv_modelo_t *m = nv_modelo(0x10DE, 0x0240);
    confere(m && nv_modelo_aceita(m, 0x4E) && !nv_modelo_aceita(m, 0x4C), "0240 (6150, C51): solo el chip 0x4E");
    m = nv_modelo(0x10DE, 0x03D0);
    confere(m && nv_modelo_aceita(m, 0x4C) && !nv_modelo_aceita(m, 0x4E), "03D0 (6150SE, C61): chip 0x4C");
    m = nv_modelo(0x10DE, 0x053E);
    confere(m && nv_modelo_aceita(m, 0x68) && nv_modelo_aceita(m, 0x67) && !nv_modelo_aceita(m, 0x63),
            "053E (7025, C68): 0x68 (o 0x67)");
    m = nv_modelo(0x10DE, 0x053A);
    confere(m && nv_modelo_aceita(m, 0x68), "053A (7050 PV, C68)");
    m = nv_modelo(0x10DE, 0x07E2);
    confere(m && nv_modelo_aceita(m, 0x63) && !nv_modelo_aceita(m, 0x68), "07E2 (7050, C73): chip 0x63");
    m = nv_modelo(0x10DE, 0x07E0);
    confere(m && nv_modelo_aceita(m, 0x63), "07E0 (7150, C73)");
    m = nv_modelo(0x10DE, 0x00F1);
    confere(m && nv_modelo_aceita(m, 0x43) && nv_modelo_aceita(m, 0x47) && !nv_modelo_aceita(m, 0x50) &&
            !nv_modelo_aceita(m, 0x36), "00F1 (AGP con puente): cualquier NV4x/G7x, no otra familia");
    confere(nv_modelo(0x10DE, 0x01D7) && nv_modelo(0x10DE, 0x0398) && nv_modelo(0x10DE, 0x0144),
            "las Go 7300, Go 7600 y Go 6600 están");
    confere(!nv_modelo(0x10DE, 0x0322) && !nv_modelo(0x10DE, 0x0191) && !nv_modelo(0x10DE, 0x0243) &&
            !nv_modelo(0x10DE, 0x0120) && !nv_modelo(0x10DE, 0x07E4),
            "FX 5200, 8800, y huecos de las familias (0243, 0120, 07E4): no");
    confere(!nv_modelo(0x1002, 0x0240), "otro fabricante con el mismo modelo: no");
    bool unicos = true, chips = true;
    for (int i = 0; i < NV_N_MODELOS; i++) {
        for (int j = i + 1; j < NV_N_MODELOS; j++)
            if (nv_modelos[i].modelo == nv_modelos[j].modelo) unicos = false;
        if (nv_modelos[i].chip_a && !nv_chip_suportado(nv_modelos[i].chip_a)) chips = false;
        if (nv_modelos[i].chip_b && !nv_chip_suportado(nv_modelos[i].chip_b)) chips = false;
        if (!nv_modelos[i].nome || !nv_modelos[i].nome[0]) chips = false;
    }
    confere(unicos, "ningún identificador repetido");
    confere(chips, "cada modelo espera chips de este driver y tiene nombre");
    confere(nv_chip_de_boot0(0x04E000A2u) == 0x4E && nv_chip_de_boot0(0x063000A1u) == 0x63 &&
            nv_chip_de_boot0(0x046000A1u) == 0x46, "PMC_BOOT_0: el chip en los bits 28:20");
    confere(nv_chip_de_boot0(0) == -1 && !nv_chip_suportado(nv_chip_de_boot0(0xFFFFFFFFu)),
            "PMC_BOOT_0 a cero o todo unos (la tarjeta no responde): no");
    confere(!nv_chip_suportado(0x50) && !nv_chip_suportado(0x34) && !nv_chip_suportado(0x48), "NV50, NV34 y 0x48: no");
}

static void grupos(void)
{
    confere(nv_memcfg(0x40) == NV_MEMCFG_9A4, "NV40: memoria en 0x9A4");
    confere(nv_memcfg(0x4E) == NV_MEMCFG_9F0 && nv_memcfg(0x44) == NV_MEMCFG_9F0, "C51 y NV44: 0x9F0");
    confere(nv_memcfg(0x47) == NV_MEMCFG_DF0 && nv_memcfg(0x46) == NV_MEMCFG_DF0, "G70 y G72: 0xDF0");
    confere(nv_memcfg(0x4C) == (NV_MEMCFG_9F0 | NV_MEMCFG_DF0) && nv_memcfg(0x68) == (NV_MEMCFG_9F0 | NV_MEMCFG_DF0) &&
            nv_memcfg(0x63) == (NV_MEMCFG_9F0 | NV_MEMCFG_DF0), "C61, C68 y C73: las dos (X.Org y nouveau no coinciden)");
    confere(nv_chip_pmc1700(0x4E) && nv_chip_pmc1700(0x68) && nv_chip_pmc1700(0x63) && nv_chip_pmc1700(0x46) &&
            !nv_chip_pmc1700(0x43) && !nv_chip_pmc1700(0x47), "PMC 0x1700: NV44, G72 y las integradas; no NV43 ni G70");
    confere(nv_chip_integrado(0x4C) && nv_chip_integrado(0x67) && !nv_chip_integrado(0x44), "integradas: C51/C61/C67/C68/C73");
    uint32_t b = 0;
    confere(nv_regioes_tiling(0x40, &b) == 8 && b == 0x100240, "NV40: 8 regiones de tiling en 0x100240");
    confere(nv_regioes_tiling(0x4C, &b) == 15 && b == 0x100600, "C61: 15 en 0x100600");
    confere(nv_regioes_tiling(0x4E, &b) == 12 && b == 0x100600, "C51: 12 en 0x100600");
}

int main(void)
{
    cabecalhos();
    pontos();
    ligacoes_e_pramin();
    anel();
    crtc();
    memoria();
    modelos();
    grupos();
    printf("%d comprobaciones, %d fallos\n", oks + falhas, falhas);
    if (falhas) return 1;
    printf("TUDO OK\n");
    return 0;
}
