// SPDX-License-Identifier: GPL-2.0-or-later
/* r500_v3_unidad.c - prueba de unidad en el PC de gpu3d v3 en las Radeon R5xx (D457): el programa de
 * vértices generado por r5_vs_gerar (kernel/dispositivos/r500_pacotes.h) y las órdenes del dibujo desde búferes.
 *
 *   gcc -O1 -fno-builtin -Wall -Wno-builtin-declaration-mismatch -I kernel/inc -I kernel/dispositivos \
 *       -o r500_v3_unidad tests/r500_v3_unidad.c kernel/gui/gpu3d_mem.c -lm
 *
 * QEMU no emula una Radeon. Lo que se comprueba sin la tarjeta:
 *   1. el programa hace la cuenta de ES 1.1: un simulador pequeño del procesador de vértices (PVS: DOT, MUL, ADD,
 *      MAD, MAX, MIN y RSQ, con sus selecciones de componentes, ceros, unos y negaciones, tal como las describe
 *      r300_reg.h de Mesa) lo ejecuta con vértices leídos como dice PROG_STREAM_CNTL, y las salidas (posición de
 *      recorte, color, especular con la niebla en las dos formas, coordenadas con y sin matriz) se comparan con
 *      la cuenta en coma flotante;
 *   2. las órdenes: la etapa de vértices solo escribe registros del 3D (0x1D98-0x1DAC y 0x2000-0x4FFC, y la espera WAIT_UNTIL), cuadra
 *      con las cuentas (instrucciones, constantes), 3D_LOAD_VBPNTR lleva sus arrays con el tamaño y el paso en
 *      palabras y 3D_DRAW_VBUF_2 el tipo y el número de vértices;
 *   3. la etapa de v1/v2 se puede volver a poner (r5_estado_v3_fim deja la tubería sin recorte ni vista).
 * Lo que NO puede comprobar: que la X1400 entienda esas palabras como este simulador. Eso lo dice la autoprueba
 * v3 de la capa en la máquina (docs/gpu3d.md).
 *
 * Usa las cabeceras del kernel: no incluye las de la libc y declara a mano lo que usa. */
#include "gpu3d.h"
#include "r500_pacotes.h"

int printf(const char *, ...);
float sqrtf(float), fabsf(float);
void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *o, size_t n) { unsigned char *p = d; const unsigned char *q = o; while (n--) *p++ = *q++; return d; }

static int falhas, feitas;
#define VER(c, ...) do { feitas++; if (!(c)) { falhas++; if (falhas < 25) { printf("FALLO: " __VA_ARGS__); printf("\n"); } } } while (0)

static uint32_t bits(float f) { uint32_t u; memcpy(&u, &f, 4); return u; }
static float de_bits(uint32_t u) { float f; memcpy(&f, &u, 4); return f; }
static uint32_t semente = 99;
static float azar(float a, float b) { semente = semente * 1664525u + 1013904223u; return a + (b - a) * (float)((semente >> 8) & 0xFFFF) / 65535.0f; }

/* ------------------------------------------------------------ el simulador del PVS */

typedef struct { float v[4]; } v4;
static v4 entradas[8], temps[8], saidas[8];
static const r5_vs_t *prog;

static v4 origem(uint32_t o)
{
    uint32_t tipo = o & 3u, n = (o >> 5) & 0xFFu;
    v4 r, b;
    if (tipo == R5_PVS_O_ENTRADA) b = entradas[n & 7];
    else if (tipo == R5_PVS_O_CONST) { for (int k = 0; k < 4; k++) b.v[k] = de_bits(prog->c[n < R5_VS_CONST ? n : 0][k]); VER(n < prog->nc, "constante %u sin subir (hay %u)", n, prog->nc); }
    else b = temps[n & 7];
    for (int k = 0; k < 4; k++) {
        uint32_t s = (o >> (13 + 3 * k)) & 7u;
        r.v[k] = s < 4 ? b.v[s] : s == 4 ? 0.0f : 1.0f;
        if (o & (1u << (25 + k))) r.v[k] = -r.v[k];
    }
    return r;
}

static void executar(void)
{
    memset(temps, 0, sizeof temps); memset(saidas, 0, sizeof saidas);
    for (int i = 0; i < prog->n; i++) {
        const uint32_t *p = &prog->p[i * 4];
        uint32_t op = p[0] & 0x7Fu, tipo = (p[0] >> 8) & 0xFu, n = (p[0] >> 13) & 0x7Fu, mascara = (p[0] >> 20) & 0xFu;
        v4 a = origem(p[1]), b = origem(p[2]), c = origem(p[3]), r;
        /* D386: los orígenes que no se usan, el mismo registro que el primero */
        VER((p[2] & 0x1FFFu) == (p[1] & 0x1FFFu) || ((p[2] >> 13) & 0xFFFu) != 0x924u, "instruccion %d: el cero del origen 1 no es del mismo registro", i);
        VER((p[3] & 0x1FFFu) == (p[1] & 0x1FFFu) || ((p[3] >> 13) & 0xFFFu) != 0x924u, "instruccion %d: el cero del origen 2 no es del mismo registro", i);
        /* el motor no lee dos entradas distintas ni dos constantes distintas en una instrucción (Mesa, t_src_conflict) */
        for (int x = 1; x <= 3; x++) for (int y = x + 1; y <= 3; y++)
            VER((p[x] & 3u) != (p[y] & 3u) || (p[x] & 3u) == R5_PVS_O_TEMP || ((p[x] >> 5) & 0xFFu) == ((p[y] >> 5) & 0xFFu),
                "instruccion %d: los origenes %d y %d leen dos %s distintas", i, x - 1, y - 1, (p[x] & 3u) == R5_PVS_O_CONST ? "constantes" : "entradas");
        float d = a.v[0] * b.v[0] + a.v[1] * b.v[1] + a.v[2] * b.v[2] + a.v[3] * b.v[3];
        for (int k = 0; k < 4; k++) {
            switch (op) {
            case R5_PVS_DOT: r.v[k] = d; break;
            case R5_PVS_MUL: r.v[k] = a.v[k] * b.v[k]; break;
            case R5_PVS_ADD: r.v[k] = a.v[k] + b.v[k]; break;
            case R5_PVS_MAD: r.v[k] = a.v[k] * b.v[k] + c.v[k]; break;
            case R5_PVS_MAX: r.v[k] = a.v[k] > b.v[k] ? a.v[k] : b.v[k]; break;
            case R5_PVS_MIN: r.v[k] = a.v[k] < b.v[k] ? a.v[k] : b.v[k]; break;
            case R5_PVS_RSQ: r.v[k] = a.v[3] != 0.0f ? 1.0f / sqrtf(fabsf(a.v[3])) : 3.4e38f; break;   /* el escalar, de la w */
            default: r.v[k] = 0; VER(0, "instruccion %d: operacion %x desconocida", i, op);
            }
        }
        if (op == R5_PVS_MAD) {
            /* la MAD normal no admite tres temporales distintos (r3xx_vertprog.c de Mesa) */
            int tt = ((p[1] & 3u) == 0) + ((p[2] & 3u) == 0) + ((p[3] & 3u) == 0);
            VER(tt < 3, "instruccion %d: MAD con tres temporales", i);
        }
        v4 *dst = tipo == R5_PVS_D_SAIDA ? &saidas[n & 7] : &temps[n & 7];
        VER(tipo == R5_PVS_D_SAIDA || tipo == R5_PVS_D_TEMP, "instruccion %d: destino de tipo %u", i, tipo);
        VER(mascara, "instruccion %d: no escribe nada", i);
        for (int k = 0; k < 4; k++) if (mascara & (1u << k)) dst->v[k] = r.v[k];
    }
}

/* Lee el vértice como dice PROG_STREAM_CNTL: el array k va a la entrada k con su tipo y su selección. */
static void ler_entradas(const gpu3d_desenho3_t *d, const uint8_t *mem, uint32_t i)
{
    memset(entradas, 0, sizeof entradas);
    for (uint32_t k = 0; k < prog->arrays; k++) {
        const gpu3d_atrib_t *a = prog->atr[k] == 0 ? &d->pos : prog->atr[k] == 1 ? &d->cor_a : &d->tc[(prog->atr[k] - 2u) & 3u];
        const uint8_t *p = mem + a->desloc + i * a->passo;
        uint32_t tipo = prog->psc[k] & 0xFu, dest = (prog->psc[k] >> 8) & 0x1Fu;
        float c[4] = { 0, 0, 0, 0 };
        if (tipo == R5_PSC_BYTE) { VER(prog->psc[k] & R5_PSC_NORMALIZAR, "array %u: bytes sin normalizar", k); for (int q = 0; q < 4; q++) c[q] = (float)p[q] / 255.0f; }
        else { for (uint32_t q = 0; q <= tipo && q < 4; q++) { uint32_t w; memcpy(&w, p + q * 4, 4); c[q] = de_bits(w); } }
        VER(dest == k, "array %u: va a la entrada %u", k, dest);
        VER(((prog->psc[k] & R5_PSC_ULTIMO) != 0) == (k == (uint32_t)prog->arrays - 1), "array %u: marca de ultimo", k);
        VER((prog->ext[k] >> 12) == 0xFu, "array %u: no escribe las cuatro componentes", k);
        for (int q = 0; q < 4; q++) { uint32_t s = (prog->ext[k] >> (3 * q)) & 7u; entradas[dest & 7].v[q] = s < 4 ? c[s] : s == 4 ? 0.0f : 1.0f; }
    }
}

static int perto(float a, float b, float tol) { float d = a - b; return (d < 0 ? -d : d) <= tol * (1.0f + (b < 0 ? -b : b)); }

/* ------------------------------------------------------------ 1. el programa contra la cuenta */

typedef struct { float x, y, z, w; uint8_t cor[4]; float corf[4]; float tc[4]; } vert_t;

static void prova_programa(void)
{
    static vert_t vs[16];
    int casos = 0;
    for (int it = 0; it < 240; it++) {
        gpu3d_transf_t t;
        gpu3d_desenho3_t d;
        memset(&t, 0, sizeof t); memset(&d, 0, sizeof d);
        float mvp[16], mv[16], tex[2][16];
        for (int k = 0; k < 16; k++) { mvp[k] = azar(-2, 2); mv[k] = azar(-2, 2); tex[0][k] = azar(-2, 2); tex[1][k] = azar(-2, 2); t.mvp[k] = bits(mvp[k]); t.mv[k] = bits(mv[k]); t.tex[0][k] = bits(tex[0][k]); t.tex[2][k] = bits(tex[1][k]); }
        int fpos = 2 + it % 3, cor_tipo = (it / 3) % 3, neb = (it / 9) % 3, ntc = 1 + (it / 27) % 2, mat = (it / 54) % 2, tc_const = (it / 108) % 2;
        uint8_t mapa[4] = { 0, 2, 0, 0 };   /* el juego 0 es la unidad 0; el 1, la unidad 2 */
        d.pos = (gpu3d_atrib_t){ 3, (unsigned char)fpos, sizeof(vert_t), 0 };
        if (cor_tipo == 0) d.cor_a = (gpu3d_atrib_t){ 3, G3D_AT_UBYTE4N, sizeof(vert_t), 16 };
        if (cor_tipo == 1) d.cor_a = (gpu3d_atrib_t){ 3, G3D_AT_FLOAT4, sizeof(vert_t), 20 };
        t.cor = 0x80C04020u;
        if (!tc_const) d.tc[0] = (gpu3d_atrib_t){ 3, G3D_AT_FLOAT2, sizeof(vert_t), 36 };
        if (ntc > 1) d.tc[2] = (gpu3d_atrib_t){ 3, G3D_AT_FLOAT4, sizeof(vert_t), 36 };
        float tcc[4] = { 0.25f, 0.5f, 0.0f, 1.0f };
        for (int k = 0; k < 4; k++) t.tc_const[0][k] = bits(tcc[k]);
        if (mat) t.flags |= G3D3_TEX_MAT(0) | G3D3_TEX_MAT(2);
        float na = azar(1, 3), nb = azar(0.1f, 0.5f);
        if (neb) { t.flags |= G3D3_NEBLINA | (neb == 2 ? G3D3_NEB_MEZCLADA : 0); t.neb_a = bits(na); t.neb_b = bits(nb); t.neb_cor = 0x00FF8040u; }
        static r5_vs_t v;
        r5_vs_gerar(&v, &t, &d, ntc, mapa, false);
        VER(!v.erro && v.n > 0 && v.n <= R5_VS_INST && v.nc >= 4 && v.nc <= R5_VS_CONST, "caso %d: programa de %u instrucciones, %u constantes, error %u", it, v.n, v.nc, v.erro);
        VER(v.arrays == 1 + (cor_tipo < 2) + !tc_const + (ntc > 1), "caso %d: %u arrays", it, v.arrays);
        prog = &v;
        for (int i = 0; i < 16; i++) {
            vert_t *q = &vs[i];
            q->x = azar(-3, 3); q->y = azar(-3, 3); q->z = azar(-3, 3); q->w = azar(0.5f, 2);
            for (int k = 0; k < 4; k++) { q->cor[k] = (uint8_t)azar(0, 255); q->corf[k] = azar(0, 1); q->tc[k] = azar(-2, 2); }
            ler_entradas(&d, (const uint8_t *)vs, (uint32_t)i);
            executar();
            float p[4] = { q->x, q->y, fpos > 2 ? q->z : 0.0f, fpos > 3 ? q->w : 1.0f };
            for (int l = 0; l < 4; l++) {
                float e = mvp[l] * p[0] + mvp[4 + l] * p[1] + mvp[8 + l] * p[2] + mvp[12 + l] * p[3];
                VER(perto(saidas[0].v[l], e, 1e-4f), "caso %d: posicion[%d] %f y no %f", it, l, (double)saidas[0].v[l], (double)e);
            }
            float c[4], f = 1.0f;
            for (int k = 0; k < 4; k++) c[k] = cor_tipo == 0 ? (float)q->cor[k] / 255.0f : cor_tipo == 1 ? q->corf[k] : (float)((0x80C04020u >> (k == 3 ? 24 : 16 - 8 * k)) & 0xFF) / 255.0f;
            if (neb) {
                float o[3];
                for (int l = 0; l < 3; l++) o[l] = mv[l] * p[0] + mv[4 + l] * p[1] + mv[8 + l] * p[2] + mv[12 + l] * p[3];
                f = na - nb * sqrtf(o[0] * o[0] + o[1] * o[1] + o[2] * o[2]);
                f = f < 0 ? 0 : f > 1 ? 1 : f;
            }
            static const float nc[3] = { 1.0f, 128.0f / 255.0f, 64.0f / 255.0f };
            for (int k = 0; k < 4; k++) {
                float ec = neb == 2 && k < 3 ? c[k] * f : c[k];
                float ee = neb == 2 ? (k < 3 ? nc[k] * (1.0f - f) : saidas[2].v[3]) : k < 3 ? 0.0f : f;
                VER(perto(saidas[1].v[k], ec, 2e-4f), "caso %d: color[%d] %f y no %f", it, k, (double)saidas[1].v[k], (double)ec);
                VER(perto(saidas[2].v[k], ee, 2e-4f), "caso %d (niebla %d): especular[%d] %f y no %f", it, neb, k, (double)saidas[2].v[k], (double)ee);
            }
            for (int j = 0; j < ntc; j++) {
                float in[4] = { q->tc[0], q->tc[1], 0.0f, 1.0f };
                if (j == 1) { in[2] = q->tc[2]; in[3] = q->tc[3]; }
                if (j == 0 && tc_const) for (int k = 0; k < 4; k++) in[k] = tcc[k];
                for (int l = 0; l < 2; l++) {
                    float e = mat ? tex[j][l] * in[0] + tex[j][4 + l] * in[1] + tex[j][8 + l] * in[2] + tex[j][12 + l] * in[3] : in[l];
                    VER(perto(saidas[3 + j].v[l], e, 1e-4f), "caso %d: juego %d coordenada %d %f y no %f", it, j, l, (double)saidas[3 + j].v[l], (double)e);
                }
            }
        }
        casos++;
    }
    printf("  programa de vertices: %d combinaciones (formato de la posicion, color, niebla, juegos, matriz) x 16 vertices dan la cuenta de ES 1.1\n", casos);
}

/* ------------------------------------------------------------ 2 y 3. las órdenes */

static void prova_ordens(void)
{
    static uint32_t pal[2048];
    gpu3d_transf_t t;
    gpu3d_desenho3_t d;
    memset(&t, 0, sizeof t); memset(&d, 0, sizeof d);
    for (int k = 0; k < 16; k++) t.mvp[k] = t.mv[k] = bits(k % 5 == 0 ? 1.0f : 0.0f);
    t.flags = G3D3_NEBLINA | G3D3_NEB_MEZCLADA; t.neb_a = bits(1.5f); t.neb_b = bits(0.1f);
    t.vp_escala[0] = bits(320); t.vp_desloc[0] = bits(320); t.vp_escala[1] = bits(-240); t.vp_desloc[1] = bits(240);
    t.vp_escala[2] = bits(0.5f); t.vp_desloc[2] = bits(0.5f);
    t.descarte = G3D_DESC_POS;
    d.pos = (gpu3d_atrib_t){ 0, G3D_AT_FLOAT3, 24, 0 };
    d.tc[0] = (gpu3d_atrib_t){ 0, G3D_AT_FLOAT2, 24, 12 };
    d.cor_a = (gpu3d_atrib_t){ 0, G3D_AT_UBYTE4N, 24, 20 };
    d.modo = G3D_PRIM_TRIANGULOS; d.n = 3000; d.indices = GPU3D_SEM_INDICES;
    static const uint8_t mapa[4] = { 0, 0, 0, 0 };
    static r5_vs_t v, v2;
    r5_vs_gerar(&v, &t, &d, 1, mapa, false);
    r5_vs_gerar(&v2, &t, &d, 1, mapa, false);
    int igual = 1;
    for (uint32_t i = 0; i < sizeof v; i++) if (((const uint8_t *)&v)[i] != ((const uint8_t *)&v2)[i]) igual = 0;
    VER(igual, "dos veces lo mismo no da la misma etapa (el driver la mandaria en cada dibujo)");
    r5_vs_gerar(&v2, &t, &d, 1, mapa, true);
    /* el convenio medido en la X1400 (revisión de D457): área positiva con la y hacia abajo = «atrás» */
    VER(v.descarte == R5_CULL_ATRAS && v2.descarte == R5_CULL_FRENTE, "descarte %u y, invertido, %u", v.descarte, v2.descarte);
    VER(v.arrays == 3 && v.atr[0] == 0 && v.atr[1] == 1 && v.atr[2] == 2, "arrays: posicion, color y la coordenada");
    g3d_fila_t f;
    g3d_fila_iniciar(&f, pal, 2048);
    r5_estado_vertices3(&f, &v);
    VER(!f.transbordou && f.n % 2 == 0, "la etapa de vertices cabe (%u palabras)", f.n);
    uint32_t n_pvs = 0, vte = 0, clip = 0xFFFFFFFFu, cull = 0xFFFFFFFFu, cod0 = 0, konst = 0, vista = 0, indice = 0xFFFFFFFFu, n_codigo = 0, n_const = 0;
    for (uint32_t i = 0; i + 1 < f.n; i += 2) {
        VER((pal[i] >> 16) == 0, "palabra %u: no es un registro suelto (%x)", i, pal[i]);
        uint32_t reg = (pal[i] & 0xFFFFu) << 2, val = pal[i + 1];
        VER((reg >= 0x1D98u && reg <= 0x1DACu) || (reg >= 0x2000u && reg <= 0x4FFCu) || reg == R5_WAIT_UNTIL, "registro %x fuera del 3D", reg);
        if (reg == R5_VAP_PVS_INDICE) indice = val;
        if (reg == R5_VAP_PVS_DADOS) { n_pvs++; if (indice == 0) n_codigo++; else if (indice == R5_PVS_CONST_INDICE) n_const++; }
        if (reg == R5_VAP_VTE_CNTL) vte = val;
        if (reg == R5_VAP_CLIP_CNTL) clip = val;
        if (reg == R5_SU_CULL_MODE) cull = val;
        if (reg == R5_VAP_PVS_CODE_CNTL_0) cod0 = val;
        if (reg == R5_VAP_PVS_CONST_CNTL) konst = val;
        if (reg >= R5_VAP_VPORT_XSCALE && reg <= R5_VAP_VPORT_XSCALE + 20) vista++;
    }
    VER(n_codigo == (uint32_t)v.n * 4 && n_const == (uint32_t)v.nc * 4, "codigo %u palabras (%u instrucciones), constantes %u (%u)", n_codigo, v.n, n_const, v.nc);
    VER(vte == (R5_VTE_VISTA | R5_VTE_W0) && clip == R5_CLIP_ABANICO && cull == R5_CULL_ATRAS && vista == 6, "vista, recorte y descarte: VTE %x CLIP %x CULL %x", vte, clip, cull);
    VER(cod0 == ((((uint32_t)v.n - 1) << 10) | (((uint32_t)v.n - 1) << 20)) && konst == (((uint32_t)v.nc - 1) << 16), "PVS_CODE_CNTL_0 %x, CONST_CNTL %x", cod0, konst);
    VER(pal[0] == R5_PACOTE0(R5_RB3D_DSTCACHE, 0), "la etapa empieza vaciando las caches y esperando (D451)");
    /* el dibujo */
    g3d_fila_iniciar(&f, pal, 2048);
    uint32_t gpu[R5_VS_ARRAYS] = { 0xC1000000u, 0xC1000014u, 0xC100000Cu, 0, 0, 0 };
    r5_desenho3(&f, &v, &d, gpu);
    VER(f.n == 13 && pal[0] == R5_PACOTE0(R5_VAP_VF_MAX_VTX_INDX, 0) && pal[1] == 2999 && pal[2] == R5_PACOTE0(R5_VAP_VF_MAX_VTX_INDX + 4, 0) && pal[3] == 0,
        "indices mayor y menor delante del dibujo: %u palabras, %x %x %x %x", f.n, pal[0], pal[1], pal[2], pal[3]);
    const uint32_t *q = pal + 4;
    VER(q[0] == (R5_P3_LOAD_VBPNTR | (5u << 16)) && q[1] == (3u | R5_VBPNTR_PREBUSCA), "LOAD_VBPNTR: cabecera %x, arrays %x", q[0], q[1]);
    VER(q[2] == (3u | (6u << 8) | (1u << 16) | (6u << 24)) && q[3] == 0xC1000000u && q[4] == 0xC1000014u, "posicion (3 palabras, paso 6) y color (1, 6): %x %x %x", q[2], q[3], q[4]);
    VER(q[5] == (2u | (6u << 8)) && q[6] == 0xC100000Cu, "coordenada (2 palabras, paso 6): %x %x", q[5], q[6]);
    VER(q[7] == R5_P3_DRAW_VBUF_2 && q[8] == (R5_PRIM_TRIANGULOS | R5_PRIM_LISTA | (3000u << 16)), "DRAW_VBUF_2 de 3000 vertices: %x %x", q[7], q[8]);
    g3d_fila_iniciar(&f, pal, 2048);
    d.modo = G3D_PRIM_TIRA; d.n = 65535;
    r5_desenho3(&f, &v, &d, gpu);
    VER(pal[f.n - 2] == R5_P3_DRAW_VBUF_2 && pal[f.n - 1] == (R5_PRIM_TIRA | R5_PRIM_LISTA | (65535u << 16)), "DRAW_VBUF_2 de una tira de 65535: %x %x", pal[f.n - 2], pal[f.n - 1]);
    /* la vuelta a v1/v2 */
    g3d_fila_iniciar(&f, pal, 2048);
    r5_estado_v3_fim(&f);
    vte = clip = cull = 0xFFFFFFFFu;
    for (uint32_t i = 0; i + 1 < f.n; i += 2) {
        uint32_t reg = (pal[i] & 0xFFFFu) << 2;
        if (reg == R5_VAP_VTE_CNTL) vte = pal[i + 1];
        if (reg == R5_VAP_CLIP_CNTL) clip = pal[i + 1];
        if (reg == R5_SU_CULL_MODE) cull = pal[i + 1];
    }
    VER(vte == (R5_VTE_XY_DIVIDIDO | R5_VTE_Z_DIVIDIDO) && clip == R5_CLIP_DESLIGADO && cull == 0, "la vuelta a v1/v2: VTE %x CLIP %x CULL %x", vte, clip, cull);
    /* el montón de los búferes */
    static g3d_vbmonte_t m;
    g3d_vbmonte_iniciar(&m, 1u << 20);
    int n = 0;
    while (n < GPU3D_VBUFS && g3d_vbmonte_criar(&m, n, 3000 + (uint32_t)n * 7)) n++;
    VER(n > 200, "el monton de 1 MB admite %d buferes de ~4 KB", n);
    for (int i = 0; i < n; i++) for (int j = i + 1; j < n; j++)
        if (m.off[i] < m.off[j] + m.tam[j] && m.off[j] < m.off[i] + m.tam[i]) { VER(0, "los buferes %d y %d se pisan", i, j); i = j = n; }
    for (int i = 0; i < n; i += 2) g3d_vbmonte_soltar(&m, i);
    VER(g3d_vbmonte_criar(&m, 0, 2000) && !g3d_vbmonte_criar(&m, 0, 100) && !g3d_vbmonte_criar(&m, 2, 2u << 20), "huecos reutilizados; un hueco ocupado o demasiado grande, no");
    for (int i = 0; i < GPU3D_VBUFS; i++) g3d_vbmonte_soltar(&m, i);
    VER(m.usado == 0 && m.n == 0 && g3d_vbmonte_criar(&m, 5, 1u << 20), "vacio, cabe uno del tamaño del monton");
    printf("  ordenes: etapa de vertices solo con registros del 3D, LOAD_VBPNTR y DRAW_VBUF_2 bien formados, vuelta a v1/v2 y monton de buferes\n");
}

int main(void)
{
    prova_programa();
    prova_ordens();
    printf("%d comprobaciones, %d fallos\n", feitas, falhas);
    if (!falhas) printf("todo bien\n");
    return falhas ? 1 : 0;
}
