// SPDX-License-Identifier: GPL-2.0-or-later
/* r500_fp_unidad.c - prueba de unidad en el PC del programa de fragmentos que genera el driver de las R5xx
 * para gpu3d v2 (D426: r5_fp_gerar de kernel/dispositivos/r500_pacotes.h).
 *
 *   gcc -O1 -fno-builtin -Wall -Wno-builtin-declaration-mismatch -I kernel/inc -I kernel/dispositivos \
 *       -o r500_fp_unidad tests/r500_fp_unidad.c kernel/gui/gpu3d_mem.c
 *
 * QEMU no emula el US de las R5xx. Esto lo emula: un intérprete de las palabras del US (los campos de las
 * instrucciones de r300_reg.h de Mesa: orígenes, constantes, mezclas, «negar», MAD, DP3/DP, omod, saturar,
 * máscaras de escritura y salida) ejecuta el programa generado para miles de estados al azar y compara la
 * salida con la cuenta de referencia (tests/g3d_combinar_ref.h). Además comprueba que el programa no lee un
 * temporal que nadie escribió (ni el rasterizador ni una instrucción anterior), que cada lectura de textura
 * usa las coordenadas de su posición en el vértice, que US_PIXSIZE cubre todos los temporales y que la última
 * instrucción es la de salida. Si la interpretación de un campo fuera distinta de la del hardware, esto no lo
 * vería: eso lo dicen las pruebas v2 del propio driver en la máquina (g3d_v2_provas.c). */
#include "gpu3d.h"
#include "r500_pacotes.h"
#include "g3d_combinar_ref.h"

int printf(const char *, ...);
void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }
void *memcpy(void *d, const void *s, size_t n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; return d; }

static int falhas, feitas;
#define VER(c, ...) do { feitas++; if (!(c)) { falhas++; if (falhas < 30) { printf("FALLO: " __VA_ARGS__); printf("\n"); } } } while (0)

static float t[128][4];
static unsigned definido[128];   /* bit c: el componente c tiene valor */
static float k[256][4];
static int lidos_indefinidos;

static float le(int addr, int c)
{
    if (addr & 0x100) return k[addr & 0xFF][c];
    if (!(definido[addr & 0x7F] & (1u << c))) lidos_indefinidos++;
    return t[addr & 0x7F][c];
}

/* El valor de un argumento (A, B o C) del color: sel, 3 mezclas, negar (y abs). */
static void arg_rgb(uint32_t bits, const int orig[3], uint32_t srcp_op, float out[3])
{
    uint32_t sel = bits & 3, neg = (bits >> 11) & 1, abs_ = (bits >> 12) & 1;
    for (int c = 0; c < 3; c++) {
        uint32_t s = (bits >> (2 + 3 * c)) & 7;
        float v;
        if (s == 4) v = 0; else if (s == 5) v = 0.5f; else if (s == 6) v = 1;
        else if (s == 7) v = 0;
        else if (sel == 3) {
            float a0 = le(orig[0], (int)s), a1 = le(orig[1], (int)s);
            v = srcp_op == 0 ? 1 - 2 * a0 : srcp_op == 1 ? a1 - a0 : srcp_op == 2 ? a1 + a0 : 1 - a0;
        } else v = le(orig[sel], (int)s);
        if (abs_) v = v < 0 ? -v : v;
        if (neg) v = -v;
        out[c] = v;
    }
}

static float arg_alfa(uint32_t bits, const int orig[3], uint32_t srcp_op)
{
    uint32_t sel = bits & 3, s = (bits >> 2) & 7, neg = (bits >> 5) & 1, abs_ = (bits >> 6) & 1;
    float v;
    if (s == 4) v = 0; else if (s == 5) v = 0.5f; else if (s == 6) v = 1; else if (s == 7) v = 0;
    else if (sel == 3) {
        float a0 = le(orig[0], (int)s), a1 = le(orig[1], (int)s);
        v = srcp_op == 0 ? 1 - 2 * a0 : srcp_op == 1 ? a1 - a0 : srcp_op == 2 ? a1 + a0 : 1 - a0;
    } else v = le(orig[sel], (int)s);
    if (abs_) v = v < 0 ? -v : v;
    if (neg) v = -v;
    return v;
}

static float omod(uint32_t m, float v) { return m == 1 ? v * 2 : m == 2 ? v * 4 : m == 3 ? v * 8 : m == 4 ? v / 2 : m == 5 ? v / 4 : m == 6 ? v / 8 : v; }
static float sat(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

/* Ejecuta el programa; tex[u] es lo que devuelve la unidad u. coord_esperada[u]: el temporal de coordenadas que
 * debe leer. Devuelve false si no llega a una salida. */
static bool executar(const r5_fp_t *fp, const ref_entrada_t *in, const int coord_esperada[4], float saida[4])
{
    memset(definido, 0, sizeof definido);
    lidos_indefinidos = 0;
    for (int c = 0; c < 4; c++) { t[R5V_T_COR][c] = in->prim.c[c]; t[R5V_T_ESPEC][c] = in->espec.c[c]; }
    definido[R5V_T_COR] = definido[R5V_T_ESPEC] = 0xF;
    for (int i = 0; i < fp->ntc; i++) { definido[R5V_T_COORD(i)] = 0xF; for (int c = 0; c < 4; c++) t[R5V_T_COORD(i)][c] = 0.25f * (float)(i + 1); }
    for (int u = 0; u < 4; u++) for (int c = 0; c < 4; c++) k[u][c] = in->cons[u].c[c];
    for (int c = 0; c < 4; c++) k[4][c] = in->neblina.c[c];
    for (int i = 0; i < fp->n; i++) {
        const uint32_t *w = fp->p + i * 6;
        uint32_t tipo = w[0] & 3;
        if (tipo == 3) {
            uint32_t u = (w[1] >> 16) & 0xF, o = w[2] & 0x7F, d = (w[2] >> 16) & 0x7F;
            VER(((w[1] >> 22) & 7) == 1, "la lectura de textura es LD");
            VER(u < 4 && coord_esperada[u] >= 0 && (int)o == coord_esperada[u], "la unidad %u lee sus coordenadas (t%u, esperado t%d)", u, o,
                u < 4 ? coord_esperada[u] : -1);
            VER(definido[o] == 0xF, "las coordenadas t%u tienen valor", o);
            for (int c = 0; c < 4; c++) t[d][c] = in->tex[u & 3].c[c];
            definido[d] = 0xF;
            continue;
        }
        VER(tipo == 0 || tipo == 1, "instruccion %d: tipo ALU o salida (%u)", i, tipo);
        int oc[3], oa[3];
        for (int j = 0; j < 3; j++) { oc[j] = (int)((w[1] >> (10 * j)) & 0x1FF); oa[j] = (int)((w[2] >> (10 * j)) & 0x1FF); }
        uint32_t srcp_c = w[1] >> 30, srcp_a = w[2] >> 30;
        float A[3], B[3], C[3];
        uint32_t op_c = w[5] & 0xF, op_a = w[4] & 0xF;
        bool usa_c = ((w[0] >> 11) & 7) || ((w[0] >> 15) & 7), usa_a = ((w[0] >> 14) & 1) || ((w[0] >> 18) & 1);
        float res[4] = { 0, 0, 0, 0 };
        int defs_antes = lidos_indefinidos;
        if (usa_c) {
            arg_rgb(w[3] & 0x1FFF, oc, srcp_c, A);
            arg_rgb((w[3] >> 13) & 0x1FFF, oc, srcp_c, B);
            arg_rgb((w[5] >> 12) & 0x1FFF, oc, srcp_c, C);
            if (op_c == 1) { float d = A[0] * B[0] + A[1] * B[1] + A[2] * B[2]; res[0] = res[1] = res[2] = d; }
            else { VER(op_c == 0, "operacion de color MAD o DP3 (%u)", op_c); for (int c = 0; c < 3; c++) res[c] = A[c] * B[c] + C[c]; }
            for (int c = 0; c < 3; c++) { res[c] = omod((w[3] >> 26) & 7, res[c]); if (w[0] & (1u << 19)) res[c] = sat(res[c]); }
        }
        if (usa_a) {
            if (op_a == 1) {
                VER(op_c == 1, "el DP del alfa va con un DP3 en el color");
                float d = A[0] * B[0] + A[1] * B[1] + A[2] * B[2];
                res[3] = d + arg_alfa((w[4] >> 12) & 0x7F, oa, srcp_a) * arg_alfa((w[4] >> 19) & 0x7F, oa, srcp_a);
            } else {
                VER(op_a == 0, "operacion de alfa MAD o DP (%u)", op_a);
                res[3] = arg_alfa((w[4] >> 12) & 0x7F, oa, srcp_a) * arg_alfa((w[4] >> 19) & 0x7F, oa, srcp_a) + arg_alfa((w[5] >> 25) & 0x7F, oa, srcp_a);
            }
            res[3] = omod((w[4] >> 26) & 7, res[3]);
            if (w[0] & (1u << 20)) res[3] = sat(res[3]);
        }
        VER(lidos_indefinidos == defs_antes, "instruccion %d: lee un temporal sin valor", i);
        if (tipo == 1) {
            VER(i == fp->n - 1 && (w[0] & R5_FP_ULTIMA), "la salida es la ultima instruccion");
            VER(((w[0] >> 15) & 7) == 7 && ((w[0] >> 18) & 1), "la salida escribe r, g, b y a");
            for (int c = 0; c < 4; c++) saida[c] = res[c];
            return true;
        }
        uint32_t dc = (w[5] >> 4) & 0x7F, da = (w[4] >> 4) & 0x7F;
        for (int c = 0; c < 3; c++) if (w[0] & (1u << (11 + c))) { t[dc][c] = res[c]; definido[dc] |= 1u << c; }
        if (w[0] & (1u << 14)) { t[da][3] = res[3]; definido[da] |= 8; }
        VER((int)dc <= fp->max_temp || !((w[0] >> 11) & 7), "US_PIXSIZE cubre t%u", dc);
        VER((int)da <= fp->max_temp || !((w[0] >> 14) & 1), "US_PIXSIZE cubre t%u", da);
    }
    return false;
}

static float absf(float v) { return v < 0 ? -v : v; }

static void caso(const gpu3d_estado2_t *e, const char *nome)
{
    static r5_fp_t fp;
    r5_fp_gerar(&fp, e);
    VER(!fp.erro, "%s: el programa cabe (%d instrucciones)", nome, fp.n);
    if (fp.erro) return;
    VER(fp.n <= R5V_MAX_INST && fp.max_temp <= (int)R5V_T_MAX, "%s: tamaño", nome);
    /* Posición de cada unidad activa en el vértice y sus coordenadas. */
    int coord[4] = { -1, -1, -1, -1 }, pos = 0;
    for (int u = 0; u < e->unidades; u++) if (e->u[u].ativa) { VER(fp.mapa[pos] == e->u[u].coord, "%s: mapa", nome); coord[u] = (int)R5V_T_COORD(pos); pos++; }
    VER(fp.ntc == (pos ? pos : 1), "%s: juegos de coordenadas (%d)", nome, fp.ntc);
    /* D426 (Amilo): los semáforos de textura como Mesa (radeon_pair_schedule.c): solo la última lectura del bloque
     * lleva SEM_WAIT y SEM_ACQUIRE; con los dos en cada lectura la X1400 se colgaba con dos o más texturas. */
    int ult_tex = -1;
    for (int k = 0; k < fp.n; k++) if ((fp.p[k * R5_FP_PAL_INST] & 3u) == R5_FP_TIPO_TEX) ult_tex = k;
    for (int k = 0; k <= ult_tex; k++) {
        bool ultima = k == ult_tex;
        uint32_t w0 = fp.p[k * R5_FP_PAL_INST], w1 = fp.p[k * R5_FP_PAL_INST + 1];
        VER((w0 & 3u) == R5_FP_TIPO_TEX, "%s: las lecturas van juntas al principio", nome);
        VER(!!(w0 & R5_FP_ESPERA_TEX) == ultima && !!(w1 & R5_FP_TEX_PEDIR) == ultima,
            "%s: lectura %d de %d: SEM_WAIT %d SEM_ACQUIRE %d (solo en la ultima)", nome, k, ult_tex + 1,
            !!(w0 & R5_FP_ESPERA_TEX), !!(w1 & R5_FP_TEX_PEDIR));
    }
    for (int rep = 0; rep < 8; rep++) {
        ref_entrada_t in;
        in.prim = ref_cor(); in.espec = ref_cor(); in.neblina = ref_cor();
        for (int u = 0; u < 4; u++) { in.tex[u] = ref_cor(); in.cons[u] = ref_cor(); }
        ref_v4 r = ref_combinar(e, &in);
        float s[4];
        bool ok = executar(&fp, &in, coord, s);
        VER(ok, "%s: llega a la salida", nome);
        if (!ok) return;
        bool igual = true;
        for (int c = 0; c < 4; c++) if (absf(s[c] - r.c[c]) > 1e-4f) igual = false;
        VER(igual, "%s: salida (%.4f %.4f %.4f %.4f), referencia (%.4f %.4f %.4f %.4f)", nome, (double)s[0], (double)s[1], (double)s[2],
            (double)s[3], (double)r.c[0], (double)r.c[1], (double)r.c[2], (double)r.c[3]);
    }
}

int main(void)
{
    gpu3d_estado2_t e;
    /* Sin unidades: color + especular. */
    memset(&e, 0, sizeof e); e.versao = 2;
    caso(&e, "sin unidades");
    e.flags = G3D2_NEBLINA;
    caso(&e, "sin unidades, con niebla");
    /* Cada función con cada escala, una unidad. */
    for (int f = 0; f < 8; f++)
        for (int sc = 1; sc <= 4; sc *= 2) {
            memset(&e, 0, sizeof e); e.versao = 2; e.unidades = 1;
            gpu3d_unidade_t *x = &e.u[0];
            x->ativa = 1; x->rgb_func = (unsigned char)f; x->alfa_func = (unsigned char)(f % 6); x->rgb_escala = x->alfa_escala = (unsigned char)sc;
            x->rgb_fonte[0] = G3D_FT_TEXTURA; x->rgb_fonte[1] = G3D_FT_PRIMARIA; x->rgb_fonte[2] = G3D_FT_CONSTANTE;
            x->alfa_fonte[0] = G3D_FT_TEXTURA; x->alfa_fonte[1] = G3D_FT_PRIMARIA; x->alfa_fonte[2] = G3D_FT_CONSTANTE;
            x->rgb_opd[2] = G3D_OPD_ALFA; x->alfa_opd[0] = x->alfa_opd[1] = x->alfa_opd[2] = G3D_OPD_ALFA;
            caso(&e, "una unidad");
        }
    /* Al azar: todas las combinaciones de orígenes, operandos, funciones y unidades apagadas. */
    for (int i = 0; i < 3000; i++) { ref_estado_azar(&e); caso(&e, "al azar"); }
    /* La clave de la caché cambia con la combinación y no con lo que no la toca. */
    ref_estado_azar(&e);
    gpu3d_estado2_t e2 = e;
    e2.func_z = 3; e2.flags ^= G3D2_ESTENCIL; e2.u[0].constante ^= 0x123;
    VER(r5_fp_chave(&e) == r5_fp_chave(&e2), "la clave no depende de lo que no cambia el programa");
    e2.u[0].ativa = 1; e2.u[0].rgb_func = (unsigned char)((e.u[0].rgb_func + 1) % 8); e2.u[0].ativa = e.u[0].ativa ? 1 : 1;
    if (e.u[0].ativa) VER(r5_fp_chave(&e) != r5_fp_chave(&e2), "la clave cambia con la función");
    printf("r500_fp_unidad: %d comprobaciones, %d fallos\n", feitas, falhas);
    return falhas ? 1 : 0;
}
