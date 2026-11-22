// SPDX-License-Identifier: GPL-2.0-or-later
/* g3d_apres_unidad.c - prueba en el PC de las piezas de «presentar sin volver a la RAM» (D432):
 *   - las bandas negras y los vértices de la copia (kernel/gui/gpu3d_mem.c);
 *   - la pantalla como destino de color en R5xx (r500_pacotes.h), sin Z, con la fuente como textura.
 * Las R6xx/R7xx y Evergreen, en tests/g3d_apres_r600_unidad.c.
 *
 *   gcc -O1 -fno-builtin -Wall -Ikernel/inc -Ikernel/dispositivos tests/g3d_apres_unidad.c kernel/gui/gpu3d_mem.c */
#include "gpu3d.h"
#include "r500_pacotes.h"

/* Con las cabeceras del kernel (sus tipos): sin las de la libc; lo poco que se usa, a mano. */
int printf(const char *, ...);
void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *o, size_t n) { unsigned char *p = d; const unsigned char *q = o; while (n--) *p++ = *q++; return d; }

static int n_ok, falhas;
#define VERIFICA(c, ...) do { if (c) n_ok++; else { falhas++; printf("FALLO: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static float f_de(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static int perto(float a, float b) { float d = a - b; return d < 0.002f && d > -0.002f; }

/* ------------------------------------------------------------ bandas y vértices */

static void bandas(void)
{
    int r[4][4];
    g3d_apres_t p = { -1, 0, 0, 320, 240, 0, 0, 1024, 768, 0 };
    VERIFICA(g3d_apres_bandas(&p, 1024, 768, r) == 0, "destino = pantalla: sin bandas");
    /* 4:3 en 1280x800: franjas a los lados. */
    p = (g3d_apres_t){ -1, 0, 0, 320, 240, 106, 0, 1067, 800, 0 };
    int n = g3d_apres_bandas(&p, 1280, 800, r);
    VERIFICA(n == 2, "pilares: 2 bandas (%d)", n);
    VERIFICA(r[0][0] == 0 && r[0][1] == 0 && r[0][2] == 106 && r[0][3] == 800, "pilar izquierdo");
    VERIFICA(r[1][0] == 1173 && r[1][1] == 0 && r[1][2] == 107 && r[1][3] == 800, "pilar derecho");
    /* Un destino en medio: 4 bandas que con él cubren la pantalla sin pisarse. */
    p = (g3d_apres_t){ -1, 0, 0, 64, 64, 100, 50, 600, 400, 0 };
    n = g3d_apres_bandas(&p, 1024, 768, r);
    VERIFICA(n == 4, "destino en medio: 4 bandas (%d)", n);
    static unsigned char tela[768][1024];
    memset(tela, 0, sizeof tela);
    for (int i = 0; i < n; i++)
        for (int y = r[i][1]; y < r[i][1] + r[i][3]; y++)
            for (int x = r[i][0]; x < r[i][0] + r[i][2]; x++) tela[y][x]++;
    for (int y = p.dy; y < p.dy + p.da; y++) for (int x = p.dx; x < p.dx + p.dl; x++) tela[y][x]++;
    int mal = 0;
    for (int y = 0; y < 768; y++) for (int x = 0; x < 1024; x++) if (tela[y][x] != 1) mal++;
    VERIFICA(mal == 0, "bandas + destino cubren la pantalla una sola vez (%d píxeles mal)", mal);
}

static void vertices(void)
{
    gpu3d_vert_t q[6];
    /* El rectángulo (16, 8) 320x200 de una fuente de 640x480 a (106, 0) 1067x800. */
    g3d_apres_t p = { 3, 16, 8, 320, 200, 106, 0, 1067, 800, 0 };
    g3d_apres_quad(&p, 640, 480, q);
    float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f, u0 = 9, u1 = -9, v0 = 9, v1 = -9;
    for (int i = 0; i < 6; i++) {
        float x = f_de(q[i].x), y = f_de(q[i].y), u = f_de(q[i].u), v = f_de(q[i].v);
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
        if (u < u0) u0 = u;
        if (u > u1) u1 = u;
        if (v < v0) v0 = v;
        if (v > v1) v1 = v;
        /* cada vértice: la esquina del destino con la esquina de la fuente que le toca */
        VERIFICA(perto(u, x == 106.0f ? 16.0f / 640 : 336.0f / 640) && perto(v, y == 0.0f ? 8.0f / 480 : 208.0f / 480),
                 "vértice %d: (%g, %g) lleva (%g, %g)", i, x, y, u, v);
        VERIFICA(q[i].cor == 0xFFFFFFFFu && q[i].espec == 0 && f_de(q[i].rhw) == 1.0f && f_de(q[i].z) == 0.5f, "vértice %d: blanco, sin especular, rhw 1", i);
    }
    VERIFICA(x0 == 106.0f && x1 == 1173.0f && y0 == 0.0f && y1 == 800.0f, "el rectángulo de destino (%g-%g, %g-%g)", x0, x1, y0, y1);
    VERIFICA(perto(u0, 16.0f / 640) && perto(u1, 336.0f / 640) && perto(v0, 8.0f / 480) && perto(v1, 208.0f / 480), "las coordenadas de la fuente");
    /* Dos triángulos que no degeneran y cubren el rectángulo (área con signo, mismo sentido). */
    float a1 = (f_de(q[1].x) - f_de(q[0].x)) * (f_de(q[2].y) - f_de(q[0].y)) - (f_de(q[2].x) - f_de(q[0].x)) * (f_de(q[1].y) - f_de(q[0].y));
    float a2 = (f_de(q[4].x) - f_de(q[3].x)) * (f_de(q[5].y) - f_de(q[3].y)) - (f_de(q[5].x) - f_de(q[3].x)) * (f_de(q[4].y) - f_de(q[3].y));
    VERIFICA(a1 * a2 > 0, "los dos triángulos van en el mismo sentido");
    VERIFICA((a1 > 0 ? a1 + a2 : -(a1 + a2)) == 2.0f * 1067.0f * 800.0f, "los dos triángulos suman el rectángulo (%g)", a1 + a2);
    /* Toda la fuente: de 0 a 1 exactos. */
    p = (g3d_apres_t){ -1, 0, 0, 800, 600, 0, 0, 1600, 900, 0 };
    g3d_apres_quad(&p, 800, 600, q);
    for (int i = 0; i < 6; i++) VERIFICA((q[i].u == 0 || q[i].u == 0x3F800000u) && (q[i].v == 0 || q[i].v == 0x3F800000u), "fuente entera: 0 o 1 exactos");
    g3d_quad_cor(10, 20, 30, 40, 0xFF000000u, q);
    VERIFICA(f_de(q[0].x) == 10.0f && f_de(q[0].y) == 20.0f && f_de(q[2].x) == 40.0f && f_de(q[2].y) == 60.0f && q[0].cor == 0xFF000000u, "rectángulo de color");
}

/* ------------------------------------------------------------ R5xx: la pantalla como destino */

static uint32_t r5_valor(const g3d_fila_t *f, uint32_t reg, int *vezes)
{
    uint32_t v = 0xDEADBEEFu;
    int n = 0;
    for (uint32_t i = 0; i + 1 < f->n; i += 2)
        if (f->p[i] == R5_PACOTE0(reg, 0)) { v = f->p[i + 1]; n++; }
    if (vezes) *vezes = n;
    return v;
}

static void r500(void)
{
    uint32_t b[256];
    g3d_fila_t f;
    g3d_fila_iniciar(&f, b, 256);
    /* El Amilo: pantalla de 1280x800 en MC c0000000; el Z del programa donde estaba. */
    r5_alvo_t tela = { 0xC0000000u, 0xC0A00000u, 1280, 1280, 800, 24 };
    r5_vaciar(&f);
    uint32_t n_vaciar = f.n;
    r5_estado_alvo(&f, &tela);
    gpu3d_estado_t e = { G3D_TEXTURA, G3D_Z_SEMPRE, G3D_F_UM, G3D_F_ZERO, 0 };
    r5_tex_t t = { 0xC0600000u, 640, 640, 480, 0 };
    r5_estado_desenho(&f, &e, &t);
    VERIFICA(!f.transbordou, "la cola de la presentación cabe");
    /* D426: el vaciado de las cachés va delante de la espera al 3D limpio. */
    int i_dc = -1, i_zc = -1, i_wait = -1;
    for (uint32_t i = 0; i + 1 < n_vaciar; i += 2) {
        if (b[i] == R5_PACOTE0(R5_RB3D_DSTCACHE, 0)) i_dc = (int)i;
        if (b[i] == R5_PACOTE0(R5_ZB_ZCACHE, 0)) i_zc = (int)i;
        if (b[i] == R5_PACOTE0(R5_WAIT_UNTIL, 0)) i_wait = (int)i;
    }
    VERIFICA(i_dc >= 0 && i_zc >= 0 && i_wait > i_dc && i_wait > i_zc, "R5xx: vaciar las cachés antes de esperar (D426): %d %d %d", i_dc, i_zc, i_wait);
    VERIFICA(r5_valor(&f, R5_RB3D_COLOROFFSET0, NULL) == 0xC0000000u, "R5xx: el color va a la pantalla");
    VERIFICA(r5_valor(&f, R5_RB3D_COLORPITCH0, NULL) == (1280u | R5_COR_ARGB8888), "R5xx: paso de la pantalla, ARGB8888");
    VERIFICA(r5_valor(&f, R5_SC_SCISSOR1, NULL) == r5_tijera1(1280, 800), "R5xx: tijera a toda la pantalla");
    VERIFICA(r5_valor(&f, R5_ZB_CNTL, NULL) == 0, "R5xx: sin Z (ni prueba ni escritura)");
    VERIFICA(r5_valor(&f, R5_RB3D_CBLEND, NULL) == 0 && r5_valor(&f, R5_RB3D_MASCARA, NULL) == 0xFu, "R5xx: sin mezcla, los cuatro canales");
    VERIFICA(r5_valor(&f, R5_TX_OFFSET_0, NULL) == 0xC0600000u && r5_valor(&f, R5_TX_FORMAT2_0, NULL) == 639u, "R5xx: la fuente como textura, con su paso");
    VERIFICA(r5_valor(&f, R5_TX_ENABLE, NULL) == 1, "R5xx: una textura");
}

int main(void)
{
    bandas();
    vertices();
    r500();
    printf("g3d_apres_unidad: %d comprobaciones, %d fallos\n", n_ok + falhas, falhas);
    return falhas ? 1 : 0;
}
