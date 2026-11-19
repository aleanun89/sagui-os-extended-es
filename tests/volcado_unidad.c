// SPDX-License-Identifier: GPL-2.0-or-later
/* volcado_unidad.c - prueba en el PC de kernel/dispositivos/volcado_logica.h (D387, h394 unidad).
 *
 * Simula lo que hará la GPU: un búfer de atrás repartido en páginas físicas desordenadas (también
 * por encima de 4 GB, como con PAE), su tabla de la GART con las entradas de la R5xx, y un motor 2D
 * que, para cada franja, calcula la dirección de cada píxel del origen como el de verdad
 * (base + sy * paso + sx * 4), la traduce por la tabla y copia a una pantalla. El resultado tiene
 * que ser el rectángulo del búfer, píxel a píxel, sin tocar nada más. Además: codificación de las
 * entradas, la ventana de la GART en el MC (delante o detrás de la VRAM, sin pisar la AGP) y los
 * búferes que no sirven.
 * gcc -O2 -Wall -I kernel/inc -o volcado_unidad tests/volcado_unidad.c && ./volcado_unidad */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/dispositivos/volcado_logica.h"

static int falhas;
#define CHECA(c, ...) do { if (!(c)) { falhas++; printf("FALLO (%s:%d): ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t semente = 12345;
static uint32_t aleat(uint32_t n) { semente = semente * 1103515245u + 12345u; return (semente >> 8) % n; }

/* ------------------------------------------------------------ entradas */
static void entradas(void)
{
    uint32_t pte;
    CHECA(gart_r300_pte(0x12345000ull, GART_R300_LEITURA, &pte) && pte == (0x123450u | 8u), "entrada baja %x", pte);
    CHECA(gart_r300_fis(pte) == 0x12345000ull, "decodificar baja");
    CHECA(gart_r300_pte(0xF00000000ull + 0xABC000ull, GART_R300_LEITURA, &pte), "PAE 36 bits");
    CHECA(gart_r300_fis(pte) == 0xF00ABC000ull, "decodificar PAE %llx", (unsigned long long)gart_r300_fis(pte));
    CHECA((pte >> 24) == 0x0F, "bits 39:32 arriba");
    CHECA(gart_r300_pte(0xFFFFFFF000ull, 0, &pte) && gart_r300_fis(pte) == 0xFFFFFFF000ull, "40 bits justos");
    CHECA(!gart_r300_pte(0x10000000000ull, 0, &pte), "41 bits: no cabe");
    CHECA(!gart_r300_pte(0x12345678ull, 0, &pte), "no es una página");
    /* Todas las páginas de 36 bits al azar ida y vuelta. */
    for (int i = 0; i < 100000; i++) {
        uint64_t f = (((uint64_t)aleat(1u << 24) << 12) | ((uint64_t)aleat(16) << 36 >> 36 << 32)) & 0xFFFFFFFFFull & ~0xFFFull;
        CHECA(gart_r300_pte(f, GART_R300_LEITURA, &pte) && gart_r300_fis(pte) == f && (pte & 0xF) == 8, "ida y vuelta %llx", (unsigned long long)f);
    }
}

/* ------------------------------------------------------------ ventana en el MC */
static void janela(void)
{
    uint32_t ini, a, b;
    /* El Amilo: VRAM en c0000000 (128 MB), AGP apagada como la deja Linux (0xFFFFFFFF). */
    CHECA(mc_rango(0xC7FFC000u, &a, &b) && a == 0xC0000000u && b == 0xC7FFFFFFu, "MC_FB_LOCATION del Amilo");
    CHECA(gart_r300_janela(a, b, 0xFFFFFFFFu, 16u << 20, &ini) && ini == 0xBF000000u, "delante de la VRAM: %x", ini);
    /* VRAM en 0: detrás. */
    CHECA(mc_rango(0x07FF0000u, &a, &b) && gart_r300_janela(a, b, 0xFFFFFFFFu, 16u << 20, &ini) && ini == 0x08000000u,
          "VRAM en 0: detrás (%x)", ini);
    /* Una AGP encendida justo delante: se va detrás. */
    CHECA(gart_r300_janela(0xC0000000u, 0xC7FFFFFFu, 0xBFFFB000u, 16u << 20, &ini) && ini == 0xC8000000u,
          "AGP delante: detrás (%x)", ini);
    /* Sin sitio en ningún lado. */
    CHECA(!gart_r300_janela(0x00000000u, 0xFFFFFFFFu, 0xFFFFFFFFu, 16u << 20, &ini), "VRAM en todo el mapa: no cabe");
    CHECA(!mc_rango(0xFFFFFFFFu, &a, &b) && !mc_rango(0x0000FFFFu, &a, &b), "MC_FB_LOCATION sin sentido");
    /* Al azar: nunca pisa la VRAM ni la AGP y queda alineada. */
    for (int i = 0; i < 20000; i++) {
        uint32_t fi = aleat(0x10000) << 16, tam = (1u + aleat(64)) << 20;
        uint64_t ff64 = (uint64_t)fi + tam - 1;
        if (ff64 > 0xFFFFFFFFull) continue;
        uint32_t ff = (uint32_t)ff64, agp = aleat(2) ? 0xFFFFFFFFu : (aleat(0x10000) | (aleat(0x10000) << 16));
        if (!gart_r300_janela(fi, ff, agp, 16u << 20, &ini)) continue;
        uint32_t ai = (agp & 0xFFFF) << 16, af = (agp & 0xFFFF0000u) | 0xFFFF;
        CHECA((ini & 0xFFFFFF) == 0, "alineada");
        CHECA(!rangos_pisan(ini, (uint64_t)ini + (16u << 20) - 1, fi, ff), "pisa la VRAM %x %x-%x", ini, fi, ff);
        CHECA(!(ai <= af && rangos_pisan(ini, (uint64_t)ini + (16u << 20) - 1, ai, af)), "pisa la AGP");
    }
}

/* ------------------------------------------------------------ búfer, tabla y motor simulado */

#define MAX_PAG 4096
static uint64_t fis_pag[MAX_PAG];     /* página física de cada página del búfer */
static uint32_t tabla[MAX_PAG];      /* la GART */
static uint8_t *ram_pag[MAX_PAG];     /* contenido de cada página física (indexado por página del búfer) */

/* Busca la página física en la RAM simulada (tabla inversa sencilla). */
static uint8_t *ram_de(uint64_t fis, int npag)
{
    for (int i = 0; i < npag; i++)
        if (fis_pag[i] == fis) return ram_pag[i];
    return NULL;
}

static uint32_t px_buf(uint32_t x, uint32_t y) { return (x * 2654435761u) ^ (y * 40503u) ^ 0x5A5A00u; }

static void motor(const volcado_fonte_t *f, uint32_t gtt_base, int npag, const volcado_faixa_t *s, uint32_t *tela, uint32_t tpitch)
{
    /* Lo que la GPU sabe: SRC_PITCH_OFFSET y las coordenadas. */
    uint32_t base = (s->pitch_offset & 0x3FFFFFu) << 10, passo = (s->pitch_offset >> 22) * 64u;
    CHECA(base == s->base && passo == f->passo, "SRC_PITCH_OFFSET bien montado");
    CHECA(s->sx >= 0 && s->sx + s->l <= 8191 && s->a <= 8191, "coordenadas del motor dentro de 13 bits");
    for (int j = 0; j < s->a; j++)
        for (int i = 0; i < s->l; i++) {
            uint64_t mc = (uint64_t)base + (uint64_t)(s->sy + j) * passo + (uint64_t)(s->sx + i) * 4u;
            uint64_t idx = (mc - gtt_base) >> 12;
            if (mc < gtt_base || idx >= (uint64_t)npag) { CHECA(0, "lectura fuera de la ventana %llx", (unsigned long long)mc); return; }
            uint8_t *p = ram_de(gart_r300_fis(tabla[idx]), npag);
            if (!p) { CHECA(0, "la entrada %u apunta a una página que no es del búfer", (unsigned)idx); return; }
            tela[(uint32_t)(s->dy + j) * tpitch + (uint32_t)(s->dx + i)] = *(uint32_t *)(p + (mc & 0xFFF));
        }
}

static void caso(uint32_t larg, uint32_t alt, uint32_t desl, bool pae, int rects, int max_filas)
{
    uint32_t passo = larg * 4;
    volcado_fonte_t f = { 0xBF000000u, desl, passo, larg, alt };
    const char *porque = "";
    if (!volcado_fonte_ok(&f, &porque)) { CHECA(0, "%ux%u: %s", larg, alt, porque); return; }
    int npag = (int)volcado_paginas(desl, passo, alt);
    CHECA(npag <= MAX_PAG, "cabe en la prueba");
    /* Páginas físicas al azar y distintas (por encima de 4 GB con PAE). */
    for (int i = 0; i < npag; i++) {
        uint64_t p;
        do {
            p = ((uint64_t)aleat(0x7FFFF) + 1) << 12;
            if (pae && aleat(2)) p |= (uint64_t)(1 + aleat(15)) << 32;
            for (int k = 0; k < i; k++) if (fis_pag[k] == p) { p = 0; break; }
        } while (!p);
        fis_pag[i] = p;
        ram_pag[i] = calloc(1, 4096);
        CHECA(gart_r300_pte(p, GART_R300_LEITURA, &tabla[i]), "entrada");
    }
    /* El contenido del búfer, escrito «por la CPU» en su dirección virtual (página + desplazamiento). */
    for (uint32_t y = 0; y < alt; y++)
        for (uint32_t x = 0; x < larg; x++) {
            uint64_t v = (uint64_t)desl + (uint64_t)y * passo + x * 4u;
            *(uint32_t *)(ram_pag[v >> 12] + (v & 0xFFF)) = px_buf(x, y);
        }
    uint32_t *tela = malloc((size_t)larg * alt * 4), *ref = malloc((size_t)larg * alt * 4);
    for (uint32_t i = 0; i < larg * alt; i++) tela[i] = ref[i] = 0xDEAD0000u + i;
    static volcado_faixa_t fx[256];
    for (int r = 0; r < rects; r++) {
        int l, a, x, y;
        if (r == 0) { x = 0; y = 0; l = (int)larg; a = (int)alt; }
        else if (r == 1) { x = (int)larg - 1; y = (int)alt - 1; l = 1; a = 1; }
        else { l = 1 + (int)aleat(larg); a = 1 + (int)aleat(alt); x = (int)aleat(larg - (uint32_t)l + 1); y = (int)aleat(alt - (uint32_t)a + 1); }
        int n = volcado_trocear(&f, x, y, l, a, max_filas, fx, 256);
        CHECA(n == (a + max_filas - 1) / max_filas, "número de franjas %d", n);
        int filas = 0;
        for (int k = 0; k < n; k++) {
            CHECA((fx[k].base & 0x3FF) == 0, "base alineada a 1 KB");
            CHECA(fx[k].dx == x && fx[k].dy == y + filas && fx[k].l == l, "franjas seguidas");
            filas += fx[k].a;
            motor(&f, f.gtt_base, npag, &fx[k], tela, larg);
        }
        CHECA(filas == a, "las franjas cubren el rectángulo");
        for (int j = 0; j < a; j++)
            for (int i = 0; i < l; i++) ref[(uint32_t)(y + j) * larg + (uint32_t)(x + i)] = px_buf((uint32_t)(x + i), (uint32_t)(y + j));
        if (memcmp(tela, ref, (size_t)larg * alt * 4)) {
            uint32_t k = 0;
            while (tela[k] == ref[k]) k++;
            CHECA(0, "%ux%u desl %u, rect %d (%d,%d %dx%d): píxel %u,%u distinto (%x frente a %x)", larg, alt, desl, r, x, y, l, a,
                  k % larg, k / larg, tela[k], ref[k]);
            break;
        }
    }
    free(tela); free(ref);
    for (int i = 0; i < npag; i++) free(ram_pag[i]);
}

static void fontes_que_no_sirven(void)
{
    const char *p;
    volcado_fonte_t f = { 0xBF000000u, 12, 1366 * 4, 1366, 768 };
    CHECA(!volcado_fonte_ok(&f, &p), "1366 de ancho: el paso no es múltiplo de 64");
    f = (volcado_fonte_t){ 0xBF000000u, 14, 4096, 1024, 768 };
    CHECA(!volcado_fonte_ok(&f, &p), "desplazamiento que no es de un píxel");
    f = (volcado_fonte_t){ 0xBF000800u, 12, 4096, 1024, 768 };
    CHECA(!volcado_fonte_ok(&f, &p), "ventana sin alinear");
    f = (volcado_fonte_t){ 0xBF000000u, 12, 256 * 64, 4096, 768 };
    CHECA(!volcado_fonte_ok(&f, &p), "paso de más de 255 * 64");
    f = (volcado_fonte_t){ 0xBF000000u, 12, 4096, 1024, 768 };
    volcado_faixa_t fx[4];
    CHECA(volcado_trocear(&f, 0, 0, 1024, 768, 128, fx, 4) == -1, "no caben las franjas: -1");
    CHECA(volcado_trocear(&f, 1000, 0, 25, 1, 128, fx, 4) == -1, "fuera del búfer: -1");
    CHECA(volcado_trocear(&f, 0, 0, 0, 5, 128, fx, 4) == 0, "vacío: 0");
    CHECA(volcado_paginas(12, 4096, 768) == 769, "páginas de 1024x768 con cabecera: 769");
    CHECA(volcado_paginas(0, 4096, 768) == 768, "sin cabecera: 768");
}


/* ------------------------------------------------------------ D391: la vuelta (VRAM -> RAM)
 *
 * Un destino repartido en páginas físicas desordenadas (también por encima de 4 GB) con entradas de
 * escritura, rodeado de páginas solo de lectura (las de relleno y las del búfer de atrás). El motor
 * simulado escribe cada fila de cada franja como el de verdad (base + fila * paso + dx * 4), la
 * traduce por la tabla y solo escribe si la entrada lo permite (si no, cuenta una escritura
 * perdida, que es un fallo). Tiene que quedar el origen enmascarado (sin el alfa) en el rectángulo y
 * nada más tocado: ni el resto de las filas del destino ni las páginas de alrededor. */
static uint32_t px_vram(uint32_t x, uint32_t y) { return 0xA5000000u ^ (x * 2654435761u) ^ (y * 40503u) ^ 0x00A5C3u; }

static void retorno_caso(uint32_t larg, uint32_t alt, uint32_t passo, uint32_t desl, bool alta)
{
    retorno_destino_t d = { 0xBF000000u + 64u * 4096u, desl, passo };
    const char *porque;
    CHECA(retorno_destino_ok(&d, (int)larg, (int)alt, &porque), "%ux%u paso %u desl %u: %s", larg, alt, passo, desl, porque);
    uint32_t np = (retorno_bytes(&d, (int)larg, (int)alt) + 4095u) >> 12;
    /* Tabla de toda la ventana: 64 páginas de lectura delante (relleno), las del destino con escritura y 8 detrás. */
    uint32_t total = 64 + np + 8;
    uint32_t *tabla = calloc(total, 4);
    uint8_t **ram = calloc(total, sizeof *ram);
    uint64_t *fis = calloc(total, 8);
    for (uint32_t i = 0; i < total; i++) {
        uint64_t f;
        do {
            f = ((uint64_t)aleat(1u << 20) << 12) | (alta ? (uint64_t)aleat(16) << 32 : 0);
            for (uint32_t k = 0; k < i; k++) if (fis[k] == f) { f = 0; break; }
        } while (!f);
        fis[i] = f;
        ram[i] = malloc(4096);
        for (uint32_t k = 0; k < 1024; k++) ((uint32_t *)ram[i])[k] = 0x5A000000u + i * 1024u + k;
        bool dest = i >= 64 && i < 64 + np;
        CHECA(gart_r300_pte(f, dest ? GART_R300_LEITURA | GART_R300_ESCRITA : GART_R300_LEITURA, &tabla[i]), "entrada");
    }
    uint8_t **antes = calloc(total, sizeof *antes);
    for (uint32_t i = 0; i < total; i++) { antes[i] = malloc(4096); memcpy(antes[i], ram[i], 4096); }
    uint32_t perdidas = 0, escritas = 0;
    int n = retorno_n_faixas(&d, (int)alt, 128), filas = 0;
    for (int k = 0; k < n + 1; k++) {
        volcado_faixa_t f = { 0, 0, 0, 0, 0, 0, 0, 0 };
        bool ok = retorno_faixa(&d, (int)larg, (int)alt, 128, k, &f);
        if (k == n) { CHECA(!ok, "no hay franja %d", k); break; }
        CHECA(ok, "franja %d", k);
        if (!ok) break;
        CHECA((f.base & 0x3FF) == 0 && f.dx >= 0 && f.dx <= 255 && f.sy == filas && f.l == (int)larg, "franja %d bien formada", k);
        CHECA((f.pitch_offset & 0x3FFFFF) == f.base >> 10, "DST_PITCH_OFFSET lleva la base");
        uint32_t p = (f.pitch_offset >> 22) * 64u;
        CHECA(p >= ((uint32_t)f.dx + larg) * 4u || f.a > 1, "paso de la orden suficiente");
        CHECA(f.a == 1 || p == passo, "con varias filas, el paso es el del destino");
        for (int j = 0; j < f.a; j++)
            for (uint32_t x = 0; x < larg; x++) {
                uint32_t mc = f.base + (uint32_t)j * p + ((uint32_t)f.dx + x) * 4u;
                uint32_t idx = (mc - 0xBF000000u) >> 12;
                if (idx >= total || !(tabla[idx] & GART_R300_ESCRITA)) { perdidas++; continue; }
                uint32_t q = (uint32_t)(gart_r300_fis(tabla[idx]) == fis[idx] ? idx : 0xFFFFFFFFu);
                CHECA(q != 0xFFFFFFFFu, "traducción");
                *(uint32_t *)(ram[q] + (mc & 0xFFFu)) = px_vram(x, (uint32_t)(f.sy + j)) & 0x00FFFFFFu;
                escritas++;
            }
        filas += f.a;
    }
    CHECA(filas == (int)alt && perdidas == 0 && escritas == larg * alt, "%ux%u paso %u desl %u: %d filas, %u perdidas, %u escritas",
          larg, alt, passo, desl, filas, perdidas, escritas);
    /* Lo esperado: el rectángulo con el origen sin alfa; el resto como estaba. */
    uint32_t mal = 0;
    for (uint32_t i = 0; i < total && !mal; i++)
        for (uint32_t k = 0; k < 4096; k += 4) {
            uint32_t esperado = *(uint32_t *)(antes[i] + k);
            if (i >= 64 && i < 64 + np) {
                uint64_t v = (uint64_t)(i - 64) * 4096u + k;
                if (v >= desl && (v - desl) % passo < larg * 4u && (v - desl) / passo < alt) {
                    uint32_t y = (uint32_t)((v - desl) / passo), x = (uint32_t)((v - desl) % passo) / 4u;
                    esperado = px_vram(x, y) & 0x00FFFFFFu;
                }
            }
            if (*(uint32_t *)(ram[i] + k) != esperado) {
                CHECA(0, "%ux%u paso %u desl %u: página %u +%u distinta (%x frente a %x)", larg, alt, passo, desl, i, k,
                      *(uint32_t *)(ram[i] + k), esperado);
                mal = 1;
                break;
            }
        }
    for (uint32_t i = 0; i < total; i++) { free(ram[i]); free(antes[i]); }
    free(ram); free(antes); free(fis); free(tabla);
}

static void retorno_que_no_sirve(void)
{
    const char *p;
    retorno_destino_t d = { 0xBF000000u, 2, 1280 };
    CHECA(!retorno_destino_ok(&d, 320, 240, &p), "destino que no empieza en un píxel");
    d = (retorno_destino_t){ 0xBF000000u, 0, 1000 };
    CHECA(!retorno_destino_ok(&d, 320, 240, &p), "paso menor que la fila");
    d = (retorno_destino_t){ 0xBF000400u, 0, 1280 };
    CHECA(!retorno_destino_ok(&d, 320, 240, &p), "región sin alinear");
    d = (retorno_destino_t){ 0xBF000000u, 0, 3995 * 4 };
    CHECA(!retorno_destino_ok(&d, 3990, 10, &p), "fila a fila que no cabe en el motor");
    d = (retorno_destino_t){ 0xFFFF0000u, 0, 4096 };
    CHECA(!retorno_destino_ok(&d, 1024, 64, &p), "se sale de 32 bits");
    d = (retorno_destino_t){ 0xBF000000u, 0, 1280 };
    CHECA(retorno_filas(&d, 128) == 128 && retorno_n_faixas(&d, 240, 128) == 2, "paso de 1280: franjas de 128 filas");
    d.passo = 1284;
    CHECA(retorno_filas(&d, 128) == 1 && retorno_n_faixas(&d, 240, 128) == 240, "paso de 1284: fila a fila");
    CHECA(retorno_bytes(&d, 320, 2) == 1284 + 1280, "bytes tocados");
}

int main(void)
{
    entradas();
    janela();
    fontes_que_no_sirven();
    caso(1024, 768, 12, false, 40, 256);     /* el Amilo: kmalloc deja 12 bytes de cabecera */
    caso(1024, 768, 12, true, 40, 128);      /* con páginas por encima de 4 GB (PAE) */
    caso(800, 600, 16, true, 40, 64);
    caso(1280, 800, 4092, false, 30, 256);   /* el búfer empieza en el último píxel de una página */
    caso(640, 480, 0, true, 60, 7);
    caso(1680, 1050, 1020, true, 10, 512);
    caso(320, 200, 1024, false, 80, 1);
    retorno_que_no_sirve();
    retorno_caso(320, 240, 1280, 0, false);       /* la ventana del cubo de sgldemo (páginas propias) */
    retorno_caso(320, 240, 1280, 12, true);       /* un búfer de kmalloc, con PAE */
    retorno_caso(321, 200, 1284, 0, true);        /* paso que no es múltiplo de 64: fila a fila */
    retorno_caso(640, 480, 2560, 4092, false);    /* empieza en el último píxel de una página */
    retorno_caso(1024, 48, 1031 * 4, 1016, true); /* la autoprueba con el paso torcido */
    retorno_caso(1, 1, 4, 4092, false);
    retorno_caso(1280, 1024, 5120, 0, true);
    retorno_caso(3000, 3, 3000 * 4 + 4, 8, false);
    if (falhas) { printf("%d fallos\n", falhas); return 1; }
    printf("volcado: entradas de la GART (40 bits), ventana en el MC, franjas, vuelta a la RAM y motor simulado: todo bien\n");
    return 0;
}
