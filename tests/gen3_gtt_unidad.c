// SPDX-License-Identifier: GPL-2.0-or-later
/* gen3_gtt_unidad.c - prueba de unidad en el PC de la RAM en el GTT de Intel Gen3 (D358).
 *
 *   gcc -O1 -fno-builtin -I kernel/inc -I kernel/dispositivos -o gen3_gtt_unidad \
 *       tests/gen3_gtt_unidad.c kernel/gui/gpu3d_mem.c
 *
 * Comprueba la lógica pura de kernel/dispositivos/gen3_gtt.h (memoria robada que
 * dice GMCH_CTRL, ventana elegida, ventana libre, formato de la entrada, tamaño
 * según la memoria libre) y la copia del blitter de gen3_pacotes.h, y simula un
 * GTT como lo deja una BIOS (entradas de la memoria robada válidas y el resto en
 * una página de relleno, o a cero) para ver que la ventana nunca pisa lo usado. */
#include "gpu3d.h"
#include "gen3_pacotes.h"
#include "gen3_gtt.h"

int printf(const char *, ...);
void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }

static int falhas, feitas;
#define VER(c, ...) do { feitas++; if (!(c)) { falhas++; printf("FALLO: " __VA_ARGS__); printf("\n"); } } while (0)
#define MB (1024u * 1024u)

static void prova_robada(void)
{
    static const uint32_t esperado[8] = { 0, 1, 4, 8, 16, 32, 48, 64 };
    for (uint32_t c = 0; c < 8; c++) {
        VER(g3_gtt_robada((uint16_t)(c << 4), false) == esperado[c] * MB, "915/945: GMS %u", c);
        VER(g3_gtt_robada((uint16_t)((c << 4) | (0x0F0Fu & ~0xF0u)), true) == esperado[c] * MB, "G33: GMS %u con otros bits", c);
    }
    VER(g3_gtt_robada(0x80, false) == 0xFFFFFFFFu, "915/945: GMS 8 no existe");
    VER(g3_gtt_robada(0x80, true) == 128 * MB, "G33: 128 MB");
    VER(g3_gtt_robada(0x90, true) == 256 * MB, "G33: 256 MB");
    VER(g3_gtt_robada(0xA0, true) == 96 * MB, "G33: 96 MB");
    VER(g3_gtt_robada(0xD0, true) == 352 * MB, "G33: 352 MB");
    VER(g3_gtt_robada(0xE0, true) == 0xFFFFFFFFu && g3_gtt_robada(0xF0, true) == 0xFFFFFFFFu, "códigos desconocidos");
    VER(g3_gtt_tipo_g33(0x29C2) && g3_gtt_tipo_g33(0xA011) && !g3_gtt_tipo_g33(0x27A2) && !g3_gtt_tipo_g33(0x2582),
        "familia G33 frente a 915/945");
}

static void prova_janela(void)
{
    VER(g3_gtt_janela(8 * MB, 7 * MB + 960 * 1024, 16 * MB) == 112 * MB, "8 MB robados: ventana de 16 MB en 112 MB");
    VER(g3_gtt_janela(8 * MB, 7 * MB, 8 * MB) == 120 * MB, "ventana de 8 MB en 120 MB");
    VER(g3_gtt_janela(1 * MB, 64 * MB, 16 * MB) == 112 * MB, "el VBE anuncia más que la robada: manda lo mayor");
    VER(g3_gtt_janela(64 * MB, 8 * MB, 16 * MB) == 112 * MB, "64 MB robados");
    VER(g3_gtt_janela(128 * MB, 8 * MB, 16 * MB) == 0, "128 MB robados: no cabe por debajo de 128 MB");
    VER(g3_gtt_janela(111 * MB, 8 * MB, 16 * MB) == 112 * MB && g3_gtt_janela(112 * MB, 8 * MB, 16 * MB) == 0, "con la guarda de 1 MB cabe justo; sin ella, no");
    VER(g3_gtt_janela(111 * MB + 1, 8 * MB, 16 * MB) == 0, "lo usado se redondea al MB");
    VER(g3_gtt_janela(95 * MB, 8 * MB, 16 * MB) == 112 * MB, "95 MB + 1 de guarda + 16 = 112");
    VER(g3_gtt_janela(0xFFFFFFFFu, 8 * MB, 16 * MB) == 0, "robada desconocida");
    /* Revisión de la D358: quien llama pasa el FIN de lo usado por el VBE (fb_gfx + memoria),
     * porque el ring del 2D va al final de esa memoria y la pantalla puede no empezar en 0. */
    VER(g3_gtt_janela(8 * MB, 100 * MB + 12 * MB, 16 * MB) == 0, "pantalla en 100 MB con 12 MB de VBE: el ring acaba en 112 MB, no cabe");
    VER(g3_gtt_janela(8 * MB, 12 * MB, 16 * MB) == 112 * MB, "la misma memoria con la pantalla en 0: cabe");
    VER(g3_gtt_janela(8 * MB, 0xFFFFFFFFu, 16 * MB) == 0, "fin del VBE desbordado: no hay ventana");
    VER(g3_gtt_janela(8 * MB, 8 * MB, 0) == 0 && g3_gtt_janela(8 * MB, 8 * MB, 3 * MB / 2) == 0, "tamaños no válidos");
    for (uint32_t r = 0; r <= 200; r += 3)
        for (uint32_t b = 1; b <= 32; b *= 2) {
            uint32_t j = g3_gtt_janela(r * MB, 8 * MB, b * MB);
            if (!j) continue;
            VER(j >= r * MB + G3_GTT_GUARDA && j >= 8 * MB + G3_GTT_GUARDA && j + b * MB <= G3_GTT_LIMITE && !(j & 0xFFFFFu),
                "ventana %x de %u MB con %u MB robados", j, b, r);
        }
}

static void prova_entrada(void)
{
    VER(g3_gtt_entrada(0x12345000u) == 0x12345001u, "entrada por debajo de 4 GB");
    VER(g3_gtt_entrada(0x12345FFFu) == 0x12345001u, "solo cuenta la página");
    VER(g3_gtt_entrada(0x323456000ull) == 0x23456031u, "bits 35:32 en 7:4");
    VER(g3_gtt_entrada(0) == 1, "la página 0 también es válida");
    VER(g3_gtt_tamanho(16384) == 16 * MB && g3_gtt_tamanho(100000) == 16 * MB, "16 MB con 64 MB libres");
    VER(g3_gtt_tamanho(8192) == 8 * MB && g3_gtt_tamanho(16383) == 8 * MB, "8 MB con 32 MB libres");
    VER(g3_gtt_tamanho(8191) == 0 && g3_gtt_tamanho(0) == 0, "con menos, nada");
}

/* Un GTT de 128 MB como lo deja la BIOS: la robada (y lo que anuncie el VBE) mapeada
 * a páginas seguidas; el resto a una página de relleno (o a cero). */
static uint32_t gtt[32768];

static void bios(uint32_t robada, uint32_t vbe, uint32_t relleno)
{
    uint32_t usado = (robada > vbe ? robada : vbe) / 4096;
    for (uint32_t i = 0; i < 32768; i++) gtt[i] = i < usado ? g3_gtt_entrada(0x7F800000ull + i * 4096u) : relleno;
}

static void prova_gtt_simulado(void)
{
    static const uint32_t rellenos[2] = { 0x7FFFF001u, 0 };
    for (int k = 0; k < 2; k++) {
        bios(8 * MB, 7 * MB + 960 * 1024, rellenos[k]);
        uint32_t j = g3_gtt_janela(8 * MB, 7 * MB + 960 * 1024, 16 * MB);
        uint32_t n = 16 * MB / 4096;
        VER(j && g3_gtt_livre(gtt + j / 4096, n) == n, "relleno %x: la ventana está libre", rellenos[k]);
        for (uint32_t i = 0; i < n; i++) gtt[j / 4096 + i] = g3_gtt_entrada(0x00400000u + i * 4096u);
        int pisadas = 0;
        for (uint32_t i = 0; i < 8 * MB / 4096; i++) if (gtt[i] != g3_gtt_entrada(0x7F800000ull + i * 4096u)) pisadas++;
        VER(pisadas == 0, "relleno %x: la robada sigue igual tras escribir la ventana", rellenos[k]);
        VER(g3_gtt_livre(gtt + j / 4096, n) == 1, "escrita, la ventana ya no está libre");
    }
    /* Una BIOS que mapeó más memoria (DVMT) dentro de la ventana: se ve que está en uso. */
    bios(8 * MB, 7 * MB, 0x7FFFF001u);
    for (uint32_t i = 0; i < 4; i++) gtt[(116 * MB) / 4096 + i] = g3_gtt_entrada(0x20000000u + i * 4096u);
    uint32_t j = g3_gtt_janela(8 * MB, 7 * MB, 16 * MB);
    uint32_t r = g3_gtt_livre(gtt + j / 4096, 16 * MB / 4096);
    VER(r == (116 * MB - j) / 4096, "entradas usadas dentro de la ventana: se ve la primera (%u)", r);
}

static void prova_copia(void)
{
    uint32_t b[16];
    g3d_fila_t f;
    g3d_fila_iniciar(&f, b, 16);
    g3_blt_copiar(&f, 0x00123000u, 64, 0x07000000u, 128, 16, 4);
    VER(f.n == 8 && (b[0] & 0xFF) + 2 == 8, "copia del blitter: 8 palabras");
    VER((b[0] >> 29) == 2 && ((b[0] >> 22) & 0x7F) == 0x53 && (b[0] & (3u << 20)) == (3u << 20), "XY_SRC_COPY_BLT de 32 bits");
    VER(b[1] == ((3u << 24) | (0xCCu << 16) | 64), "BR13: 32 bpp, SRCCOPY, paso del destino");
    VER(b[2] == 0 && b[3] == ((4u << 16) | 16) && b[4] == 0x00123000u, "rectángulo y destino");
    VER(b[5] == 0 && b[6] == 128 && b[7] == 0x07000000u, "origen: posición, paso y dirección");
    VER(G3_MI_FLUSH == 0x02000000u && (G3_MI_FLUSH | G3_MI_FLUSH_LEITURA) == 0x02000001u, "MI_FLUSH con invalidación de lectura");
}

int main(void)
{
    prova_robada();
    prova_janela();
    prova_entrada();
    prova_gtt_simulado();
    prova_copia();
    printf("gen3_gtt_unidad: %d comprobaciones, %d fallos: %s\n", feitas, falhas, falhas ? "MAL" : "BIEN");
    return falhas ? 1 : 0;
}
