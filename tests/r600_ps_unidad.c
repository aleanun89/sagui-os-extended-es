// SPDX-License-Identifier: GPL-2.0-or-later
/* r600_ps_unidad.c - prueba de unidad en el PC del sombreador de píxeles que genera el driver de las R6xx/R7xx
 * para gpu3d v2 (D426: r63d_ps2_gerar de kernel/dispositivos/r600_3d_pacotes.c).
 *
 *   gcc -O2 -Wall -Wextra -DR600_ANFITRION -Ikernel/inc -o r600_ps_unidad tests/r600_ps_unidad.c \
 *       kernel/dispositivos/r600_3d_pacotes.c
 *
 * QEMU no emula las R7xx. Esto ejecuta el programa generado con un intérprete propio del ISA de la R600/R700
 * (instrucciones de flujo, cláusulas de textura y de ALU, grupos de hasta cuatro ranuras con sus literales,
 * ADD/MUL/MOV/DOT4 y MULADD(_M2/_M4), negar, omod, saturar, máscara de escritura) para miles de estados al
 * azar y compara la salida con la cuenta de referencia (tests/g3d_combinar_ref.h), para las dos codificaciones
 * (R6xx y R7xx). Comprueba también las reglas de los puertos de lectura de cada grupo (un registro por canal y
 * ciclo; 4 literales), que no se lea un registro sin valor, que cada ranura escriba su propio canal, el
 * tamaño de las cláusulas y que las lecturas de textura estén alineadas a 16 bytes. */
#include <stdio.h>
#include <string.h>
#include "r600_3d.h"
#include "g3d_combinar_ref.h"

static int falhas, feitas;
#define VER(c, ...) do { feitas++; if (!(c)) { falhas++; if (falhas < 30) { printf("FALLO: " __VA_ARGS__); printf("\n"); } } } while (0)

static float R[128][4];
static unsigned def[128];
static int indef;

static float fbits(uint32_t b) { float f; memcpy(&f, &b, 4); return f; }
static float sat(float v) { return v < 0 ? 0 : v > 1 ? 1 : v; }

static float src(uint32_t sel, uint32_t chan, uint32_t neg, const uint32_t *lit, int *maxlit)
{
    float v;
    if (sel < 128) { if (!(def[sel] & (1u << chan))) indef++; v = R[sel][chan]; }
    else if (sel == 248) v = 0; else if (sel == 249) v = 1; else if (sel == 252) v = 0.5f;
    else if (sel == 253) { v = fbits(lit[chan]); if ((int)chan + 1 > *maxlit) *maxlit = (int)chan + 1; }
    else { VER(0, "origen %u desconocido", sel); v = 0; }
    return neg ? -v : v;
}

static bool executar(const r63d_ps2_t *ps, bool r7, const ref_entrada_t *in, const int coord[4], float out[4])
{
    memset(def, 0, sizeof def);
    indef = 0;
    for (int c = 0; c < 4; c++) { R[0][c] = in->prim.c[c]; R[1][c] = in->espec.c[c]; }
    def[0] = def[1] = 0xF;
    for (int i = 0; i < 4; i++) { def[2 + i] = 0xF; for (int c = 0; c < 4; c++) R[2 + i][c] = 0.1f * (float)(i + 1); }
    const uint32_t *p = ps->p;
    for (int cfi = 0; cfi < 32; cfi++) {
        uint32_t w0 = p[2 * cfi], w1 = p[2 * cfi + 1];
        uint32_t inst = (w1 >> 23) & 0x7F;
        if (((w1 >> 26) & 0xF) >= 8) {
            VER(((w1 >> 26) & 0xF) == 8, "CF_ALU sencilla");
            /* CF_ALU: dirección (palabras de 64 bits) y cuántas */
            uint32_t addr = w0 & 0x3FFFFF, n = ((w1 >> 18) & 0x7F) + 1;
            VER(n <= 128, "clausula de ALU de %u ranuras", n);
            uint32_t i = 2 * addr, fim = 2 * (addr + n);
            while (i < fim) {
                uint32_t gi[5][2];
                int ng = 0;
                for (;;) {
                    VER(ng < 4, "grupo de mas de cuatro ranuras");
                    if (ng >= 4) return false;
                    gi[ng][0] = p[i]; gi[ng][1] = p[i + 1]; ng++; i += 2;
                    if (p[i - 2] & (1u << 31)) break;
                }
                const uint32_t *lit = p + i;
                int maxlit = 0;
                /* Puertos de lectura (VEC_012) */
                int hw[3][4];
                for (int a = 0; a < 3; a++) for (int b = 0; b < 4; b++) hw[a][b] = -1;
                float res[4][4];
                int dchan[4], dgpr[4], escreve[4], clamp[4], is_dot = 0;
                float dot = 0;
                unsigned canais = 0;
                for (int s = 0; s < ng; s++) {
                    uint32_t a = gi[s][0], b = gi[s][1];
                    uint32_t s0 = a & 0x1FF, c0 = (a >> 10) & 3, n0 = (a >> 12) & 1, s1 = (a >> 13) & 0x1FF, c1 = (a >> 23) & 3, n1 = (a >> 25) & 1;
                    bool op3 = ((b >> 15) & 7) != 0;
                    dgpr[s] = (int)((b >> 21) & 0x7F); dchan[s] = (int)((b >> 29) & 3); clamp[s] = (int)(b >> 31);
                    VER(!(canais & (1u << dchan[s])), "dos ranuras escriben el mismo canal");
                    canais |= 1u << dchan[s];
                    uint32_t sel[3] = { s0, s1, 0 }, ch[3] = { c0, c1, 0 };
                    int nsrc;
                    float v[3];
                    float r;
                    if (op3) {
                        uint32_t s2 = b & 0x1FF, c2 = (b >> 10) & 3, n2 = (b >> 12) & 1, op = (b >> 13) & 0x1F;
                        sel[2] = s2; ch[2] = c2; nsrc = 3;
                        v[0] = src(s0, c0, n0, lit, &maxlit); v[1] = src(s1, c1, n1, lit, &maxlit); v[2] = src(s2, c2, n2, lit, &maxlit);
                        VER(op >= 0x10 && op <= 0x12, "OP3 MULADD (%x)", op);
                        r = v[0] * v[1] + v[2];
                        if (op == 0x11) r *= 2; else if (op == 0x12) r *= 4;
                        escreve[s] = 1;
                    } else {
                        uint32_t op = r7 ? (b >> 7) & 0x7FF : (b >> 8) & 0x3FF, om = r7 ? (b >> 5) & 3 : (b >> 6) & 3;
                        escreve[s] = (int)((b >> 4) & 1);
                        nsrc = op == 0x19 ? 1 : 2;
                        v[0] = src(s0, c0, n0, lit, &maxlit);
                        v[1] = nsrc > 1 ? src(s1, c1, n1, lit, &maxlit) : 0;
                        if (op == 0x00) r = v[0] + v[1];
                        else if (op == 0x01) r = v[0] * v[1];
                        else if (op == 0x19) r = v[0];
                        else if (op == 0x50) { dot += v[0] * v[1]; is_dot++; r = 0; }
                        else { VER(0, "operacion %x desconocida", op); r = 0; }
                        if (op != 0x50) r = om == 1 ? r * 2 : om == 2 ? r * 4 : om == 3 ? r / 2 : r;
                        else res[s][1] = (float)om;
                    }
                    for (int k = 0; k < nsrc; k++) {
                        if (sel[k] >= 128) continue;
                        if (k == 1 && sel[1] == sel[0] && ch[1] == ch[0]) continue;
                        int *h = &hw[k][ch[k]];
                        VER(*h == -1 || *h == (int)sel[k], "puertos de lectura: dos registros en el canal %u del ciclo %d", ch[k], k);
                        *h = (int)sel[k];
                    }
                    res[s][0] = r;
                }
                if (is_dot) {
                    VER(is_dot == 4 && ng == 4, "DOT4 en las cuatro ranuras");
                    float om = res[0][1];
                    float d = om == 1 ? dot * 2 : om == 2 ? dot * 4 : dot;
                    for (int s = 0; s < ng; s++) res[s][0] = d;
                }
                VER(maxlit <= 4, "como mucho 4 literales");
                i += (uint32_t)((maxlit + 1) & ~1);
                for (int s = 0; s < ng; s++) {
                    if (!escreve[s]) continue;
                    float r = clamp[s] ? sat(res[s][0]) : res[s][0];
                    R[dgpr[s]][dchan[s]] = r;
                    def[dgpr[s]] |= 1u << dchan[s];
                }
            }
            VER(i == fim, "la clausula de ALU termina donde dice su cuenta");
            continue;
        }
        if (inst == 0x01) {   /* CF_TEX */
            uint32_t addr = w0, n = (((w1 >> 10) & 7) | (((w1 >> 19) & 1) << 3)) + 1;
            VER((addr & 1) == 0, "las lecturas de textura van alineadas a 16 bytes");
            for (uint32_t t = 0; t < n; t++) {
                const uint32_t *x = p + 2 * addr + 4 * t;
                uint32_t res = (x[0] >> 8) & 0xFF, o = (x[0] >> 16) & 0x7F, d = x[1] & 0x7F, smp = (x[2] >> 15) & 0x1F;
                VER((x[0] & 0x1F) == 0x10, "SAMPLE");
                VER(res == smp && res < 4, "recurso y muestreador de la misma unidad");
                VER(res < 4 && coord[res] >= 0 && (int)o == 2 + coord[res], "la unidad %u lee su juego de coordenadas (R%u)", res, o);
                for (int c = 0; c < 4; c++) R[d][c] = in->tex[res & 3].c[c];
                def[d] = 0xF;
            }
            continue;
        }
        if (inst == 0x28 || inst == 0x27) {   /* exportación */
            uint32_t gpr = (w0 >> 15) & 0x7F, tipo = (w0 >> 13) & 3;
            VER(tipo == 0, "exporta un píxel");
            VER(inst == 0x28 && ((w1 >> 21) & 1), "la exportación es la última");
            VER(def[gpr] == 0xF, "la salida tiene valor");
            for (int c = 0; c < 4; c++) out[c] = R[gpr][c];
            return true;
        }
        VER(0, "instruccion de flujo %u desconocida", inst);
        return false;
    }
    return false;
}

static float absf(float v) { return v < 0 ? -v : v; }

static void caso(const gpu3d_estado2_t *e, const char *nome, bool r7)
{
    static r63d_ps2_t ps;
    r63d_ps2_gerar(&ps, e, r7);
    VER(!ps.erro, "%s: el programa cabe", nome);
    if (ps.erro) return;
    VER(ps.n <= R63D_PS2_MAX, "%s: tamaño", nome);
    int coord[4] = { -1, -1, -1, -1 };
    for (int u = 0; u < e->unidades; u++) if (e->u[u].ativa) coord[u] = e->u[u].coord;
    for (int rep = 0; rep < 6; rep++) {
        ref_entrada_t in;
        in.prim = ref_cor(); in.espec = ref_cor(); in.neblina = ref_cor(); in.neblina.c[3] = 1;
        for (int u = 0; u < 4; u++) { in.tex[u] = ref_cor(); in.cons[u] = ref_cor(); }
        /* Las constantes van al programa como literales: el estado lleva sus bytes. */
        gpu3d_estado2_t ee = *e;
        for (int u = 0; u < 4; u++) {
            uint32_t b[4];
            for (int c = 0; c < 4; c++) { b[c] = (uint32_t)(in.cons[u].c[c] * 255.0f + 0.5f); in.cons[u].c[c] = (float)b[c] / 255.0f; }
            ee.u[u].constante = (b[3] << 24) | (b[0] << 16) | (b[1] << 8) | b[2];
        }
        uint32_t nb[3];
        for (int c = 0; c < 3; c++) { nb[c] = (uint32_t)(in.neblina.c[c] * 255.0f + 0.5f); in.neblina.c[c] = (float)nb[c] / 255.0f; }
        ee.cor_neblina = (nb[0] << 16) | (nb[1] << 8) | nb[2];
        r63d_ps2_gerar(&ps, &ee, r7);
        VER(!ps.erro, "%s: el programa cabe con estas constantes", nome);
        if (ps.erro) return;
        ref_v4 r = ref_combinar(&ee, &in);
        float s[4];
        bool ok = executar(&ps, r7, &in, coord, s);
        VER(ok, "%s: llega a la exportacion", nome);
        if (!ok) return;
        VER(indef == 0, "%s: lee registros sin valor", nome);
        bool igual = true;
        for (int c = 0; c < 4; c++) if (absf(s[c] - r.c[c]) > 2e-4f) igual = false;
        VER(igual, "%s (%s): salida (%.4f %.4f %.4f %.4f), referencia (%.4f %.4f %.4f %.4f)", nome, r7 ? "R7xx" : "R6xx", (double)s[0],
            (double)s[1], (double)s[2], (double)s[3], (double)r.c[0], (double)r.c[1], (double)r.c[2], (double)r.c[3]);
    }
}

int main(void)
{
    gpu3d_estado2_t e;
    for (int r7 = 0; r7 < 2; r7++) {
        memset(&e, 0, sizeof e); e.versao = 2;
        caso(&e, "sin unidades", r7);
        e.flags = G3D2_NEBLINA;
        caso(&e, "sin unidades, con niebla", r7);
        for (int f = 0; f < 8; f++)
            for (int sc = 1; sc <= 4; sc *= 2) {
                memset(&e, 0, sizeof e); e.versao = 2; e.unidades = 1;
                gpu3d_unidade_t *x = &e.u[0];
                x->ativa = 1; x->rgb_func = (unsigned char)f; x->alfa_func = (unsigned char)(f % 6);
                x->rgb_escala = x->alfa_escala = (unsigned char)sc;
                x->rgb_fonte[0] = G3D_FT_TEXTURA; x->rgb_fonte[1] = G3D_FT_PRIMARIA; x->rgb_fonte[2] = G3D_FT_CONSTANTE;
                x->alfa_fonte[0] = G3D_FT_TEXTURA; x->alfa_fonte[1] = G3D_FT_PRIMARIA; x->alfa_fonte[2] = G3D_FT_CONSTANTE;
                x->rgb_opd[2] = G3D_OPD_ALFA; x->alfa_opd[0] = x->alfa_opd[1] = x->alfa_opd[2] = G3D_OPD_ALFA;
                caso(&e, "una unidad", r7);
            }
        for (int i = 0; i < 2000; i++) { ref_estado_azar(&e); caso(&e, "al azar", r7); }
    }
    /* El sombreador de vértices v2: cabe y sus lecturas están alineadas. */
    uint32_t vs[R63D_VS2_MAX];
    int n = r63d_vs2(vs);
    VER(n <= R63D_VS2_MAX && n % 4 == 0, "sombreador de vertices v2: %d palabras", n);
    VER(vs[0] == 8 && (vs[0] & 1) == 0, "las lecturas de vertices en la palabra de 64 bits 8 (alineada)");
    printf("r600_ps_unidad: %d comprobaciones, %d fallos\n", feitas, falhas);
    return falhas ? 1 : 0;
}
