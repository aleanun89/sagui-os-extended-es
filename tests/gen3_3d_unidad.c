// SPDX-License-Identifier: GPL-2.0-or-later
/* gen3_3d_unidad.c - prueba de unidad en el PC de las órdenes 3D de Intel Gen3,
 * de la memoria de GPU y de la coma flotante con enteros de la capa 3D (D285).
 *
 *   gcc -O1 -fno-builtin -I kernel/inc -I kernel/dispositivos -o gen3_3d_unidad \
 *       tests/gen3_3d_unidad.c kernel/gui/gpu3d_mem.c
 *
 * QEMU no emula el 3D de Intel: lo que sí se puede comprobar sin la máquina es
 * que las órdenes están bien formadas. La prueba recorre lo que construye
 * kernel/dispositivos/gen3_pacotes.h como lo haría la GPU (cada orden con su
 * longitud; al final tiene que cuadrar la cuenta), compara los dos programas
 * de píxeles con sus palabras escritas a mano aquí, comprueba que el formato
 * del vértice que declara el estado (S2, S4) mide lo mismo que gpu3d_vert_t,
 * y que el programa solo usa registros declarados.
 *
 * Usa las cabeceras del kernel (con sus tipos): por eso no incluye las de la
 * libc y declara a mano lo poco que usa. */
#include "gpu3d.h"
#include "gen3_pacotes.h"

int printf(const char *, ...);
void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }

static int falhas, feitas;
#define VER(c, ...) do { feitas++; if (!(c)) { falhas++; printf("FALLO: " __VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ coma flotante */

static void prova_float(void)
{
    union { float f; uint32_t u; } k;
    for (int v = -40000; v <= 40000; v += 7) {
        k.f = (float)v / 16.0f;
        VER(g3d_float_de_fixo(v, 4) == k.u, "float_de_fixo(%d, 4) = %x, se esperaba %x", v, g3d_float_de_fixo(v, 4), k.u);
        VER(g3d_fixo_de_float(k.u, 4) == v, "fixo_de_float(%d/16) = %d", v, g3d_fixo_de_float(k.u, 4));
    }
    k.f = 1.0f;     VER(g3d_float_de_fixo(65536, 16) == k.u, "1.0");
    k.f = 0.5f;     VER(g3d_float_de_fixo(32768, 16) == k.u && g3d_fixo_de_float(k.u, 16) == 32768, "0.5");
    k.f = 0.75f;    VER(g3d_fixo_de_float(k.u, 24) == 0xC00000, "0.75 con 24 bits");
    k.f = 1.0e30f;  VER(g3d_fixo_de_float(k.u, 4) == 0x7FFFFFFF, "satura por arriba");
    k.f = -1.0e30f; VER(g3d_fixo_de_float(k.u, 4) == -0x7FFFFFFF, "satura por abajo");
    k.f = 1.0e-30f; VER(g3d_fixo_de_float(k.u, 16) == 0, "lo muy pequeno es 0");
    k.u = 0x7FC00000u; VER(g3d_fixo_de_float(k.u, 4) == 0x7FFFFFFF, "NaN da el tope (la capa lo rechaza antes)");
    VER(g3d_float_de_fixo(0, 8) == 0, "cero");
}

/* ------------------------------------------------------------ memoria de GPU */

static uint32_t semente = 99;
static uint32_t aleat(uint32_t n) { semente = semente * 1103515245u + 12345u; return (semente >> 8) % n; }

static void prova_arena(void)
{
    static g3d_arena_t a;
    static g3d_buffer_t b[24];
    static uint8_t cpu[16];
    const uint64_t base = 0x00345000u, tam = 3u * 1024 * 1024;
    g3d_arena_iniciar(&a, base, cpu, tam);
    VER(g3d_arena_livre(&a) == tam, "zona nueva: todo libre");
    memset(b, 0, sizeof b);
    for (int volta = 0; volta < 4000; volta++) {
        int i = (int)aleat(24);
        if (b[i].usado) g3d_buffer_soltar(&a, &b[i]);
        else {
            uint32_t alin = 1u << (6 + aleat(7));               /* 64 bytes a 4 KB */
            uint64_t bytes = 1 + aleat(400000);
            if (g3d_buffer_criar(&a, &b[i], G3D_BUF_TEXTURA, bytes, alin)) {
                VER(!(b[i].gpu & (alin - 1)), "direccion alineada a %u", alin);
                VER(b[i].bytes >= bytes && b[i].gpu >= base && b[i].gpu + b[i].bytes <= base + tam, "bloque dentro de la zona");
                VER(b[i].cpu == cpu + (uint32_t)(b[i].gpu - base), "ventana de la CPU con el mismo desplazamiento");
            }
        }
        /* Ningún búfer pisa a otro, y la cuenta de lo libre cuadra. */
        uint64_t usado = 0;
        for (int x = 0; x < 24; x++) {
            if (!b[x].usado) continue;
            usado += b[x].bytes;
            for (int y = x + 1; y < 24; y++)
                if (b[y].usado) VER(b[x].gpu + b[x].bytes <= b[y].gpu || b[y].gpu + b[y].bytes <= b[x].gpu, "dos buferes se pisan");
        }
        VER(g3d_arena_livre(&a) == tam - usado, "libre %u, usado %u", (unsigned)g3d_arena_livre(&a), (unsigned)usado);
        uint64_t off = 0;
        for (int x = 0; x < a.n; x++) { VER(a.b[x].off == off, "bloques seguidos"); off += a.b[x].bytes; }
        VER(off == tam, "los bloques cubren la zona");
    }
    for (int i = 0; i < 24; i++) g3d_buffer_soltar(&a, &b[i]);
    VER(a.n == 1 && g3d_arena_livre(&a) == tam, "al soltarlo todo queda un solo bloque libre");
    g3d_buffer_t g;
    VER(!g3d_buffer_criar(&a, &g, G3D_BUF_ALVO, tam + 1, 4096), "lo que no cabe se rechaza");
    VER(g3d_buffer_criar(&a, &g, G3D_BUF_ALVO, tam - 4096, 4096), "casi toda la zona cabe");
}

/* ------------------------------------------------------------ órdenes */

typedef struct {
    int n_ordens, n_prim, n_vert_pal, n_prog, n_buf, n_mapas, n_amostr, n_imediato, n_ret, n_vars, desconhecidas;
    uint32_t s2, s4, s5, s6, prog_ini, prog_pal, buf[2][2], vars, ret_max, ms[3], ss[3];
} leitura_t;

/* Recorre las órdenes como la GPU: devuelve false si alguna no encaja en la cuenta. */
static bool percorrer(const uint32_t *p, uint32_t n, leitura_t *r)
{
    memset(r, 0, sizeof *r);
    uint32_t i = 0;
    while (i < n) {
        uint32_t w = p[i], len = 1;
        if (w == G3_MI_NOOP) { i++; continue; }
        if ((w >> 29) != 3) { r->desconhecidas++; return false; }
        uint32_t op = (w >> 24) & 0x1F;
        if (op == 0x1F) {
            len = (w & 0x3FFFF) + 2;
            r->n_prim++; r->n_vert_pal += (int)len - 1;
            if (((w >> 18) & 0x1F) != 0 || (w & (1u << 23))) return false;      /* lista de triángulos, vértices dentro */
        } else if (op == 0x1D) {
            uint32_t sub = (w >> 16) & 0xFF;
            /* En el estado inmediato la longitud son los 4 bits bajos (los demás dicen qué S se cargan). */
            len = (sub == 0x04 ? (w & 0xF) : (w & 0xFFFF)) + 2;
            if (i + len > n) return false;
            if (sub == 0x04) {
                r->n_imediato++;
                uint32_t k = i + 1;
                for (int s = 0; s < 8; s++)
                    if (w & G3_CARGA_S(s)) {
                        if (s == 2) r->s2 = p[k];
                        if (s == 4) r->s4 = p[k];
                        if (s == 5) r->s5 = p[k];
                        if (s == 6) r->s6 = p[k];
                        k++;
                    }
                if (k != i + len) return false;       /* tantas palabras como bits de carga */
            } else if (sub == 0x05) { r->n_prog++; r->prog_ini = i + 1; r->prog_pal = len - 1; }
            else if (sub == 0x8E) { if (len != 3 || r->n_buf >= 2) return false; r->buf[r->n_buf][0] = p[i + 1]; r->buf[r->n_buf][1] = p[i + 2]; r->n_buf++; }
            else if (sub == 0x85) { if (len != 2) return false; r->n_vars++; r->vars = p[i + 1]; }
            else if (sub == 0x80) { if (len != 5) return false; r->n_ret++; r->ret_max = p[i + 3]; }
            else if (sub == 0x00) { if (len != 5 || p[i + 1] != 1) return false; r->n_mapas++; r->ms[0] = p[i + 2]; r->ms[1] = p[i + 3]; r->ms[2] = p[i + 4]; }
            else if (sub == 0x01) { if (len != 5 || p[i + 1] != 1) return false; r->n_amostr++; r->ss[0] = p[i + 2]; r->ss[1] = p[i + 3]; r->ss[2] = p[i + 4]; }
            else if (sub == 0x81) { if (len != 3) return false; }
            else if (sub == 0x07 || sub == 0x98 || sub == 0x99 || sub == 0x9A) { if (len != 2) return false; }
            else { r->desconhecidas++; return false; }
        } else if (op == 0x06 || op == 0x07 || op == 0x08 || op == 0x0B || op == 0x0D || op == 0x16 || op == 0x1C) {
            len = 1;
        } else { r->desconhecidas++; return false; }
        if (i + len > n) return false;
        r->n_ordens++;
        i += len;
    }
    return i == n;
}

/* El programa: declaraciones primero; cada registro T o S que se usa está declarado;
 * la última instrucción escribe el color de salida saturado. */
static bool programa_ok(const uint32_t *p, uint32_t pal, int com_textura)
{
    if (pal % 3 || pal < 6) return false;
    uint32_t decl_t = 0, decl_s = 0, r_escritos = 0;
    bool instrucoes = false, saiu = false;
    for (uint32_t i = 0; i < pal; i += 3) {
        uint32_t w0 = p[i], w1 = p[i + 1], w2 = p[i + 2], op = w0 >> 24;
        if (op == 0x19) {
            if (instrucoes || w1 || w2) return false;
            uint32_t tipo = (w0 >> 19) & 7, nr = (w0 >> 14) & 0xF;
            if (tipo == G3_REG_T) { if (!(w0 & G3_CANAIS_TODOS)) return false; decl_t |= 1u << nr; }
            else if (tipo == G3_REG_S) decl_s |= 1u << nr;
            else return false;
            continue;
        }
        instrucoes = true;
        if (saiu) return false;     /* nada después de escribir el color */
        uint32_t dt = (w0 >> 19) & 7, dn = (w0 >> 14) & 0xF;
        if (op == 0x15) {
            uint32_t ct = (w1 >> 24) & 7, cn = (w1 >> 17) & 0xF, am = w0 & 0xF;
            if (w2 || !(decl_s & (1u << am))) return false;
            if (ct != G3_REG_T || !(decl_t & (1u << cn))) return false;
            if (dt != G3_REG_R) return false;
            r_escritos |= 1u << dn;
        } else if (op >= 0x01 && op <= 0x04) {
            int fontes = op == 0x02 ? 1 : op == 0x04 ? 3 : 2;
            uint32_t t[3] = { (w0 >> 7) & 7, (w1 >> 13) & 7, (w2 >> 21) & 7 };
            uint32_t nr[3] = { (w0 >> 2) & 0xF, (w1 >> 8) & 0xF, (w2 >> 16) & 0xF };
            for (int k = 0; k < fontes; k++) {
                if (t[k] == G3_REG_T && !(decl_t & (1u << nr[k]))) return false;
                if (t[k] == G3_REG_R && !(r_escritos & (1u << nr[k]))) return false;
                if (t[k] != G3_REG_T && t[k] != G3_REG_R) return false;
            }
            if (!(w0 & G3_CANAIS_TODOS)) return false;
            if (dt == G3_REG_OC) { if (!(w0 & G3_SATURA) || dn) return false; saiu = true; }
            else if (dt == G3_REG_R) r_escritos |= 1u << dn;
            else return false;
        } else return false;
    }
    if (!saiu) return false;
    if (com_textura != (decl_s != 0)) return false;
    return (decl_t & (1u << G3_T_DIFUSO)) && (decl_t & (1u << G3_T_ESPEC));
}

/* Palabras por vértice que declara el estado (como la comprobación de Mesa). */
static int palavras_vertice(uint32_t s2, uint32_t s4)
{
    int n;
    switch (s4 & (7u << 6)) {
    case 1u << 6: n = 3; break;
    case 2u << 6: n = 4; break;
    case 3u << 6: n = 2; break;
    case 4u << 6: n = 3; break;
    default: return -1;
    }
    if (s4 & (1u << 11)) n++;
    if (s4 & (1u << 10)) n++;
    if (s4 & (1u << 9)) n++;
    if (s4 & (1u << 12)) n++;
    if (s4 & (1u << 2)) n++;
    for (int u = 0; u < 8; u++, s2 >>= 4) {
        switch (s2 & 0xF) {
        case 0: n += 2; break;
        case 1: n += 3; break;
        case 2: n += 4; break;
        case 3: n += 1; break;
        case 4: n += 1; break;
        case 5: n += 2; break;
        case 0xF: break;
        default: return -1;
        }
    }
    return n;
}

static void prova_programas(void)
{
    static const uint32_t cor[] = {
        0x7D050008u,
        0x190A3C00u, 0, 0,                      /* dcl difuso */
        0x190A7C00u, 0, 0,                      /* dcl especular */
        0x01603CA0u, 0x01232901u, 0x24000000u,  /* add_sat oC, difuso, especular.xyz0 */
    };
    static const uint32_t tex[] = {
        0x7D050011u,
        0x190A3C00u, 0, 0,
        0x190A7C00u, 0, 0,
        0x19083C00u, 0, 0,                      /* dcl coordenadas 0 */
        0x19180000u, 0, 0,                      /* dcl amostrador 0 (2D) */
        0x15000000u, 0x01000000u, 0,            /* texld r0, s0, t0 */
        0x04603C00u, 0x01232801u, 0x23290124u,  /* mad_sat oC, r0, difuso, especular.xyz0 */
    };
    uint32_t b[64];
    g3d_fila_t f;
    g3d_fila_iniciar(&f, b, 64);
    uint32_t n = g3_programa(&f, G3_PROG_COR);
    VER(n == sizeof cor / 4 && !f.transbordou, "programa de color: %u palabras", n);
    for (uint32_t i = 0; i < n && i < sizeof cor / 4; i++) VER(b[i] == cor[i], "programa de color, palabra %u: %08x, a mano %08x", i, b[i], cor[i]);
    VER(programa_ok(b + 1, n - 1, 0), "programa de color: bien formado");
    g3d_fila_iniciar(&f, b, 64);
    n = g3_programa(&f, G3_PROG_TEXTURA);
    VER(n == sizeof tex / 4 && !f.transbordou, "programa con textura: %u palabras", n);
    for (uint32_t i = 0; i < n && i < sizeof tex / 4; i++) VER(b[i] == tex[i], "programa con textura, palabra %u: %08x, a mano %08x", i, b[i], tex[i]);
    VER(programa_ok(b + 1, n - 1, 1), "programa con textura: bien formado");
    /* El comprobador ve los errores: un registro sin declarar y una salida sin saturar. */
    uint32_t mal[32];
    for (uint32_t i = 0; i < n; i++) mal[i] = b[i];
    mal[7] = 0x19083C00u | (3u << 14);   /* declara las coordenadas 3 en vez de las 0 */
    VER(!programa_ok(mal + 1, n - 1, 1), "se nota un registro sin declarar");
    for (uint32_t i = 0; i < n; i++) mal[i] = b[i];
    mal[16] &= ~G3_SATURA;
    VER(!programa_ok(mal + 1, n - 1, 1), "se nota una salida sin saturar");
}

static void prova_estado(void)
{
    static uint32_t b[20000];
    static gpu3d_vert_t v[600];
    for (unsigned i = 0; i < sizeof v / 4; i++) ((uint32_t *)v)[i] = 0xC0000000u | i;   /* parecen órdenes: no deben contarse */
    const g3_alvo_t alvo = { 0x00500000u, 2560, 0x00700000u, 1280, 640, 480, 16 };
    const g3_tex_t tex = { 0x00900000u, 256, 64, 32 };
    VER(sizeof(gpu3d_vert_t) == 32, "gpu3d_vert_t mide 8 palabras");
    for (int caso = 0; caso < 64; caso++) {
        gpu3d_estado_t e;
        e.flags = (uint32_t)(caso | ((caso & 3) << 5));
        e.func_z = (uint8_t)(caso % 3); e.mez_orig = (uint8_t)(caso & 3); e.mez_dest = (uint8_t)((caso >> 2) & 3); e.textura = 0;
        bool ct = (e.flags & G3D_TEXTURA) != 0;
        g3d_fila_t f;
        g3d_fila_iniciar(&f, b, 20000);
        g3_estado_fixo(&f);
        g3_estado_alvo(&f, &alvo);
        g3_estado_desenho(&f, &e, ct ? &tex : NULL);
        uint32_t pal_estado = f.n;
        VER(pal_estado <= G3_PAL_ESTADO, "el estado ocupa %u palabras (tope %d)", pal_estado, G3_PAL_ESTADO);
        int nv = 3 * (1 + caso * 3);
        g3_triangulos(&f, v, nv);
        VER(!f.transbordou, "cabe en la cola");
        leitura_t r;
        VER(percorrer(b, f.n, &r), "caso %d: las ordenes encajan una tras otra", caso);
        VER(r.n_prim == 1 && r.n_vert_pal == nv * 8, "caso %d: una primitiva con %d vertices de 8 palabras", caso, nv);
        VER(r.n_prog == 1 && programa_ok(b + r.prog_ini, r.prog_pal, ct), "caso %d: programa %s", caso, ct ? "con textura" : "de color");
        VER(palavras_vertice(r.s2, r.s4) == 8, "caso %d: el vertice declarado (S2 %x, S4 %x) mide %d palabras", caso, r.s2, r.s4, palavras_vertice(r.s2, r.s4));
        VER(((r.s4 >> 13) & 3) == 1, "sin descarte de caras en la GPU (lo hace SaguiGL)");
        VER(r.n_imediato == 1 && r.n_buf == 2 && r.n_vars == 1 && r.n_ret == 1, "caso %d: estado completo", caso);
        VER(r.buf[0][0] == ((3u << 24) | 2560) && r.buf[0][1] == 0x00500000u, "destino de color: paso y direccion");
        VER(r.buf[1][0] == ((7u << 24) | 1280) && r.buf[1][1] == 0x00700000u, "Z: paso y direccion");
        VER((r.vars & (0xFu << 8)) == (3u << 8) && (r.vars & (3u << 2)) == 0, "ARGB8888 y Z de 16 bits");
        VER(r.ret_max == ((479u << 16) | 639u), "rectangulo de dibujo: todo el destino");
        VER(r.s5 == 0, "S5 a cero");
        VER((r.s6 & (1u << 2)) != 0, "se escribe el color");
        bool zt = (e.flags & G3D_Z_TESTE) != 0, ze = zt && (e.flags & G3D_Z_ESCRITA), mz = (e.flags & G3D_MEZCLA) != 0;
        VER(((r.s6 >> 19) & 1) == zt && ((r.s6 >> 3) & 1) == ze && ((r.s6 >> 15) & 1) == mz, "caso %d: Z y mezcla en S6 (%x)", caso, r.s6);
        if (zt) VER(((r.s6 >> 16) & 7) == (e.func_z == 0 ? 2u : e.func_z == 1 ? 4u : 0u), "funcion del Z");
        static const uint32_t fat[4] = { 1, 2, 5, 6 };
        if (mz) VER(((r.s6 >> 8) & 0xF) == fat[e.mez_orig] && ((r.s6 >> 4) & 0xF) == fat[e.mez_dest], "factores de la mezcla");
        else VER(((r.s6 >> 8) & 0xF) == 2 && ((r.s6 >> 4) & 0xF) == 1, "sin mezcla: uno y cero");
        VER(r.n_mapas == (ct ? 1 : 0) && r.n_amostr == (ct ? 1 : 0), "caso %d: mapa y amostrador solo con textura", caso);
        if (ct) {
            VER(r.ms[0] == 0x00900000u && r.ms[1] == ((31u << 21) | (63u << 10) | (3u << 7)) && r.ms[2] == (63u << 21), "mapa: 64x32, ARGB, paso 256");
            uint32_t mx = e.flags & G3D_TEX_REPETE_S ? 0u : 2u, my = e.flags & G3D_TEX_REPETE_T ? 0u : 2u, fl = e.flags & G3D_TEX_LINEAR ? 1u : 0u;
            VER(r.ss[0] == ((fl << 17) | (fl << 14)) && r.ss[1] == ((mx << 12) | (my << 9) | (1u << 5)) && r.ss[2] == 0, "amostrador (%x %x)", r.ss[0], r.ss[1]);
        }
    }
    /* Z de 24 bits. */
    g3_alvo_t a24 = alvo;
    a24.zbits = 24;
    g3d_fila_t f;
    g3d_fila_iniciar(&f, b, 64);
    g3_estado_alvo(&f, &a24);
    leitura_t r;
    VER(percorrer(b, f.n, &r) && (r.vars & (3u << 2)) == (2u << 2), "Z de 24 bits en el destino");
    /* Una cola pequeña se desborda sin escribir fuera. */
    uint32_t pequena[8 + 1];
    pequena[8] = 0xDEADBEEFu;
    g3d_fila_iniciar(&f, pequena, 8);
    g3_estado_fixo(&f);
    VER(f.transbordou && f.n == 8 && pequena[8] == 0xDEADBEEFu, "la cola no escribe mas alla de su tamano");
    /* El relleno del blitter. */
    g3d_fila_iniciar(&f, b, 16);
    g3_blt_preencher(&f, 0x00500000u, 2560, 640, 480, 32, 0x00112233u);
    VER(f.n == 6 && (b[0] >> 29) == 2 && (b[0] & 0xFF) + 2 == 6 && ((b[0] >> 22) & 0x7F) == 0x50 && (b[0] & (3u << 20)), "relleno de 32 bits: 6 palabras");
    VER(b[1] == ((3u << 24) | (0xF0u << 16) | 2560) && b[2] == 0 && b[3] == ((480u << 16) | 640u) && b[4] == 0x00500000u && b[5] == 0x00112233u, "relleno de 32 bits: campos");
    g3d_fila_iniciar(&f, b, 16);
    g3_blt_preencher(&f, 0x00700000u, 1280, 640, 480, 16, 0xFFFFu);
    VER(f.n == 6 && !(b[0] & (3u << 20)) && b[1] == ((1u << 24) | (0xF0u << 16) | 1280), "relleno de 16 bits (Z)");
}

int main(void)
{
    prova_float();
    prova_arena();
    prova_programas();
    prova_estado();
    printf("gen3_3d_unidad: %d comprobaciones, %d fallos: %s\n", feitas, falhas, falhas ? "MAL" : "BIEN");
    return falhas ? 1 : 0;
}
