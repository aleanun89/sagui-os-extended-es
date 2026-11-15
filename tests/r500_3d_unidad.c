// SPDX-License-Identifier: GPL-2.0-or-later
/* r500_3d_unidad.c - prueba de unidad en el PC de las órdenes 3D de las Radeon
 * R5xx (D287).
 *
 *   gcc -O1 -fno-builtin -I kernel/inc -I kernel/dispositivos -o r500_3d_unidad \
 *       tests/r500_3d_unidad.c kernel/gui/gpu3d_mem.c
 *
 * QEMU no emula ninguna Radeon: lo que sí se puede comprobar sin la máquina es que
 * las órdenes están bien formadas. La prueba recorre lo que construye
 * kernel/dispositivos/r500_pacotes.h como lo haría el procesador de órdenes (cada
 * orden con su longitud; al final tiene que cuadrar la cuenta) apuntando el último
 * valor de cada registro, y comprueba:
 *   - que solo se escriben registros del 3D conocidos (ni pantalla, ni relojes, ni
 *     memoria, ni el 2D);
 *   - el formato del vértice: lo que declara PROG_STREAM_CNTL mide lo mismo que
 *     gpu3d_vert_t y que VAP_VTX_SIZE, y cada entrada del programa de vértices
 *     existe;
 *   - el programa de vértices contra sus palabras escritas a mano aquí (sacadas
 *     campo a campo de la documentación de registros);
 *   - los dos programas de fragmentos, también contra sus palabras a mano, y que
 *     solo leen registros temporales que el rasterizador rellena;
 *   - el destino, el Z, la mezcla, la textura y la valla de un dibujo;
 *   - los triángulos: cabecera, cuenta y los vértices tal cual.
 *
 * Usa las cabeceras del kernel (con sus tipos): por eso no incluye las de la libc
 * y declara a mano lo poco que usa. */
#include "gpu3d.h"
#include "r500_pacotes.h"

int printf(const char *, ...);
void *memset(void *d, int c, size_t n) { unsigned char *p = d; while (n--) *p++ = (unsigned char)c; return d; }

static int falhas, feitas;
#define VER(c, ...) do { feitas++; if (!(c)) { falhas++; printf("FALLO: " __VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ el recorrido */

#define N_REG (0x5000 / 4)
typedef struct {
    uint32_t val[N_REG];           /* último valor de cada registro */
    uint8_t  vezes[N_REG];
    uint32_t pvs[64], n_pvs;       /* lo escrito en VAP_PVS_DADOS */
    uint32_t us[64], n_us;         /* lo escrito en GA_US_DADOS */
    int      n_prim, n_vert, n_ordens, desconhecidos;
    uint32_t vf_cntl;
    const uint32_t *vert;          /* los vértices de la última orden de dibujo */
} cp_t;

/* ¿Es un registro que el 3D puede escribir por la cola? */
static bool reg_permitido(uint32_t r)
{
    if (r & 3) return false;
    if (r == R5_WAIT_UNTIL || r == R5_ISYNC_CNTL || r == R5_DST_PIPE_CONFIG) return true;
    if (r >= R5_SCRATCH_REG0 && r <= R5_SCRATCH_REG0 + 20) return true;
    if (r >= 0x2080 && r < 0x2300) return true;      /* VAP */
    if (r >= 0x4000 && r < 0x4800) return true;      /* GB, GA, SU, RS, SC, TX, US */
    if (r >= 0x4BC0 && r < 0x4C00) return true;      /* FG */
    if (r >= 0x4E00 && r < 0x4F60) return true;      /* RB3D y ZB */
    return false;
}

static uint32_t R(const cp_t *c, uint32_t reg) { return c->val[reg / 4]; }
static int vezes(const cp_t *c, uint32_t reg) { return c->vezes[reg / 4]; }

/* Recorre las órdenes como el CP. acumula: no borra lo apuntado antes. false: algo no encaja. */
static bool percorrer(const uint32_t *p, uint32_t n, cp_t *c, bool acumula)
{
    if (!acumula) memset(c, 0, sizeof *c);
    uint32_t i = 0;
    while (i < n) {
        uint32_t w = p[i], tipo = w >> 30;
        c->n_ordens++;
        if (tipo == 2) { if (w != R5_PACOTE2) return false; i++; continue; }
        if (tipo == 0) {
            uint32_t reg = (w & 0x1FFF) << 2, cnt = ((w >> 16) & 0x3FFF) + 1;
            bool um_so = (w & 0x8000) != 0;
            if (i + 1 + cnt > n) return false;
            for (uint32_t k = 0; k < cnt; k++) {
                uint32_t r = um_so ? reg : reg + k * 4, v = p[i + 1 + k];
                if (!reg_permitido(r)) { c->desconhecidos++; printf("registro no permitido %x\n", r); return false; }
                c->val[r / 4] = v;
                if (c->vezes[r / 4] < 255) c->vezes[r / 4]++;
                if (r == R5_VAP_PVS_INDICE) c->n_pvs = v * 4;   /* índice en instrucciones de 4 palabras */
                if (r == R5_VAP_PVS_DADOS) { if (c->n_pvs >= 64) return false; c->pvs[c->n_pvs++] = v; }
                if (r == R5_GA_US_INDICE) c->n_us = v * 6;
                if (r == R5_GA_US_DADOS) { if (c->n_us >= 64) return false; c->us[c->n_us++] = v; }
            }
            i += 1 + cnt;
            continue;
        }
        if (tipo == 3) {
            uint32_t cnt = ((w >> 16) & 0x3FFF) + 1, op = (w >> 8) & 0xFF;
            if (i + 1 + cnt > n) return false;
            if (op != 0x35) { c->desconhecidos++; return false; }      /* solo 3D_DRAW_IMMD_2 */
            uint32_t vf = p[i + 1], nv = vf >> 16;
            if ((vf & 0xF) != 4 || ((vf >> 4) & 3) != 3) return false;  /* lista de triángulos, vértices en la orden */
            if (cnt - 1 != nv * R(c, R5_VAP_VTX_SIZE)) { printf("orden de dibujo: %u palabras para %u vertices\n", cnt - 1, nv); return false; }
            if (nv % 3 || !nv) return false;
            c->n_prim++; c->n_vert += (int)nv; c->vf_cntl = vf; c->vert = &p[i + 2];
            i += 1 + cnt;
            continue;
        }
        return false;   /* tipo 1: no se usa */
    }
    return i == n;
}

/* ------------------------------------------------------------ el estado fijo */

/* Palabras de un elemento del flujo de vértices (campo de 16 bits de PROG_STREAM_CNTL). */
static int psc_palavras(uint32_t e)
{
    switch (e & 0xF) {
    case 0: return 1;   /* FLOAT_1 */
    case 1: return 2;
    case 2: return 3;
    case 3: return 4;
    case 4: case 5: return 1;   /* 4 bytes; D3DCOLOR */
    }
    return -1;
}

static void prova_inicio(int tuberias, uint32_t fpus)
{
    static uint32_t b[1024];
    static cp_t c;
    g3d_fila_t f;
    g3d_fila_iniciar(&f, b, 1024);
    r5_inicio(&f, (uint32_t)tuberias, fpus);
    VER(!f.transbordou && f.n > 150 && f.n < 400, "el estado fijo cabe (%u palabras)", f.n);
    VER(percorrer(b, f.n, &c, false), "el estado fijo se recorre entero (%d ordenes)", c.n_ordens);
    VER(c.n_prim == 0, "el estado fijo no dibuja");
    VER(R(&c, R5_ISYNC_CNTL) == 0x33, "ISYNC_CNTL: 2D y 3D se esperan y los apuntes esperan al motor");
    VER(R(&c, R5_SU_REG_DEST) == (1u << tuberias) - 1, "SU_REG_DEST con %d tuberias", tuberias);
    static const uint32_t tile[5] = { 0, 0x11, 0x17, 0x1D, 0x1F };
    VER(R(&c, R5_GB_TILE_CONFIG) == tile[tuberias], "GB_TILE_CONFIG %x con %d tuberias", R(&c, R5_GB_TILE_CONFIG), tuberias);
    VER(R(&c, R5_DST_PIPE_CONFIG) == 0x80000000u, "tuberias en automatico");
    VER(R(&c, R5_VAP_CNTL) == (5u | (5u << 4) | (fpus << 8) | (9u << 18)), "VAP_CNTL con %u FPU", fpus);
    VER(R(&c, R5_VAP_CNTL_STATUS) == 0, "programa de vertices en uso (sin PVS_BYPASS)");
    VER(R(&c, R5_VAP_VTE_CNTL) == 0x300, "VTE: x, y, z ya divididos; la cuarta coordenada es 1/w; sin escala de ventana");
    VER(R(&c, R5_VAP_CLIP_CNTL) == 0x10000, "sin recorte (SaguiGL ya recorto)");
    VER(R(&c, R5_SU_CULL_MODE) == 0, "sin descarte de caras");
    VER(R(&c, R5_GA_COLOR_CONTROL) == 0xAAAA, "Gouraud en los cuatro colores");
    /* El formato del vértice. */
    uint32_t e[4] = { R(&c, R5_VAP_PSC_0) & 0xFFFF, R(&c, R5_VAP_PSC_0) >> 16, R(&c, R5_VAP_PSC_1) & 0xFFFF, R(&c, R5_VAP_PSC_1) >> 16 };
    int pal = 0;
    for (int i = 0; i < 4; i++) {
        VER(psc_palavras(e[i]) > 0, "elemento %d del flujo con tipo conocido (%x)", i, e[i]);
        pal += psc_palavras(e[i]) + (int)((e[i] >> 4) & 0xF);
        VER(((e[i] >> 8) & 0x1F) == (uint32_t)i, "elemento %d a la entrada %d del programa", i, i);
        VER(((e[i] >> 13) & 1) == (i == 3), "solo el ultimo elemento lleva la marca de ultimo");
    }
    VER((e[0] & 0xF) == 3 && (e[1] & 0x800F) == 0x8005 && (e[2] & 0x800F) == 0x8005 && (e[3] & 0xF) == 1,
        "posicion de 4 flotantes, dos colores D3D normalizados y textura de 2 flotantes");
    VER(pal == (int)(sizeof(gpu3d_vert_t) / 4), "el flujo declara %d palabras y gpu3d_vert_t tiene %d", pal, (int)(sizeof(gpu3d_vert_t) / 4));
    VER(R(&c, R5_VAP_VTX_SIZE) == sizeof(gpu3d_vert_t) / 4 && R5_VERT_PALAVRAS == sizeof(gpu3d_vert_t) / 4, "VAP_VTX_SIZE = 8");
    VER(R(&c, R5_VAP_PSC_EXT_0) == 0xF688F688u && R(&c, R5_VAP_PSC_EXT_1) == 0xFB08F688u, "xyzw en posicion y colores; xy01 en la textura");
    VER(R(&c, R5_VAP_OUT_VTX_FMT_0) == 7 && R(&c, R5_VAP_OUT_VTX_FMT_1) == 2, "salen posicion, dos colores y una textura de 2");
    /* El programa de vértices, palabra a palabra: ADD salida[i] = entrada[i] + 0.
     *   operación: opcode 3 (ADD) | tipo de destino 2 (salida) << 8 | i << 13 | xyzw << 20
     *   origen:    tipo 1 (entrada) | i << 5 | x << 13 | y << 16 | z << 19 | w << 22  (0 = 4, 1 = 5) */
    static const uint32_t pvs[16] = {
        0x00F00203u, 0x00D10001u, 0x01248001u, 0x01248001u,
        0x00F02203u, 0x00D10021u, 0x01248001u, 0x01248001u,
        0x00F04203u, 0x00D10041u, 0x01248001u, 0x01248001u,
        0x00F06203u, 0x01610061u, 0x01248001u, 0x01248001u,
    };
    VER(c.n_pvs == 16, "programa de vertices de 4 instrucciones (%u palabras)", c.n_pvs);
    for (int i = 0; i < 16; i++) VER(c.pvs[i] == pvs[i], "PVS palabra %d: %08x, a mano %08x", i, c.pvs[i], pvs[i]);
    VER(R(&c, R5_VAP_PVS_CODE_CNTL_0) == ((3u << 10) | (3u << 20)) && R(&c, R5_VAP_PVS_CODE_CNTL_1) == 3, "el programa va de la instruccion 0 a la 3");
    /* El rasterizador: dos colores y dos coordenadas de textura a t0, t1 y t2. */
    VER(R(&c, R5_RS_COUNT) == (2u | (2u << 7) | (1u << 18)), "RS_COUNT: 2 coordenadas de textura y 2 colores");
    VER(R(&c, R5_RS_INST_COUNT) == 1, "dos instrucciones del rasterizador");
    VER(R(&c, R5_RS_IP_0) == (0u | (1u << 6) | (62u << 12) | (63u << 18)), "RS_IP_0: (s, t, 0, 1) y el color 0");
    VER(R(&c, R5_RS_IP_0 + 4) == (62u | (62u << 6) | (62u << 12) | (63u << 18) | (1u << 24)), "RS_IP_1: el color 1");
    VER(R(&c, R5_RS_INST_0) == ((1u << 4) | (2u << 5) | (1u << 16) | (0u << 18)), "RS_INST_0: textura a t2 y color a t0");
    VER(R(&c, R5_RS_INST_0 + 4) == ((1u << 12) | (1u << 16) | (1u << 18)), "RS_INST_1: especular a t1");
    VER(R(&c, R5_US_PIXSIZE) == 2, "el programa de fragmentos usa hasta t2");
    VER(R(&c, R5_US_OUT_FMT_0) == 0x1B00 && R(&c, R5_US_OUT_FMT_0 + 4) == 0x1B0F, "salida 0 en ARGB8888; las demas sin usar");
    VER(R(&c, R5_SC_CLIP_0_A) == 0 && R(&c, R5_SC_CLIP_RULE) == 0xAAAA, "recorte del rasterizador sin el desplazamiento de las R300");
    VER(R(&c, R5_TX_ENABLE) == 0 && R(&c, R5_ZB_CNTL) == 0 && R(&c, R5_RB3D_CBLEND) == 0, "texturas, Z y mezcla apagados al empezar");
    /* Acaba con el motor vaciado. */
    VER(f.n >= 6 && b[f.n - 2] == R5_PACOTE0(R5_WAIT_UNTIL, 0) && b[f.n - 1] == 0x30000, "acaba esperando al motor parado y limpio");
}

/* ------------------------------------------------------------ programas de fragmentos */

static void prova_fp(void)
{
    /* A mano, campo a campo (US de R500, 6 palabras por instrucción):
     *  salida:  tipo 1 | espera 1<<2 | última 1<<8 | máscara de salida 0xF<<15 | saturar 3<<19
     *  orígenes (rgb y alfa): a | b<<10 | c<<20
     *  rgb:   A = origen 0 .rgb (0<<2|1<<5|2<<8) | B = origen 1 (1<<13) .rgb (0<<15|1<<18|2<<21) o 1 (6<<15|6<<18|6<<21)
     *  alfa:  MAD | A = origen 0 .a (3<<14) | B = origen 1 (1<<19) .a (3<<21) o 1 (6<<21)
     *  C:     MAD | origen 2 (2<<12) .rgb (0<<14|1<<17|2<<20) | alfa C = 0 (4<<27) */
    static const uint32_t cor[6] = { 0x001F8105u, 0x00100000u, 0x00100000u, 0x00DB2220u, 0x00C8C000u, 0x20222000u };
    static const uint32_t tex[12] = {
        /* textura: tipo 3 | espera | escribir rgba 0xF<<11 | saturar; unidad 0 | leer 1<<22 | pedir 1<<25 | solo cubiertos 1<<26;
         * origen t2 (s = r, t = g: 1<<10) | destino t2 << 16 (rgba: 1<<26|2<<28|3<<30); derivadas de t2 */
        0x00187807u, 0x06400000u, 0xE4020402u, 0x00020002u, 0, 0,
        0x001F8105u, 0x00100800u, 0x00100800u, 0x00442220u, 0x0068C000u, 0x20222000u,
    };
    uint32_t p[12];
    VER(r5_programa_fp(p, false) == 1, "sin textura: una instruccion");
    for (int i = 0; i < 6; i++) VER(p[i] == cor[i], "programa de color, palabra %d: %08x, a mano %08x", i, p[i], cor[i]);
    VER(r5_programa_fp(p, true) == 2, "con textura: dos instrucciones");
    for (int i = 0; i < 12; i++) VER(p[i] == tex[i], "programa de textura, palabra %d: %08x, a mano %08x", i, p[i], tex[i]);
    /* Solo lee temporales que el rasterizador rellena (t0, t1, t2) y la última lleva la marca. */
    for (int k = 0; k < 2; k++) {
        int ni = r5_programa_fp(p, k);
        for (int i = 0; i < ni; i++) {
            const uint32_t *q = p + i * 6;
            bool ultima = (q[0] >> 8) & 1;
            VER(ultima == (i == ni - 1), "solo la ultima instruccion lleva la marca");
            if ((q[0] & 3) == 3) VER((q[2] & 0x7F) <= 2 && ((q[2] >> 16) & 0x7F) <= 2, "la textura lee y escribe t0..t2");
            else VER((q[1] & 0x3FF) <= 2 && ((q[1] >> 10) & 0x3FF) <= 2 && ((q[1] >> 20) & 0x3FF) <= 2 && q[1] == q[2], "la suma lee t0..t2");
        }
    }
}

/* ------------------------------------------------------------ un dibujo */

static void prova_desenho(void)
{
    static uint32_t b[4096];
    static cp_t c;
    g3d_fila_t f;
    r5_alvo_t a = { 0xC0100000u, 0xC0200000u, 144, 128, 96, 24 };
    gpu3d_vert_t v[6];
    for (int i = 0; i < 6; i++) v[i] = (gpu3d_vert_t){ 0x41000000u + (uint32_t)i, 0x42000000u, 0x3F000000u, 0x3F800000u, 0xFF102030u + (uint32_t)i, 0x00010203u, 0x3E800000u, 0x3F400000u };
    /* El estado fijo primero (VAP_VTX_SIZE y demás), y después un dibujo sin textura con Z. */
    g3d_fila_iniciar(&f, b, 4096);
    r5_inicio(&f, 1, 2);
    uint32_t n0 = f.n;
    gpu3d_estado_t e = { G3D_Z_TESTE | G3D_Z_ESCRITA, G3D_Z_MENOR, G3D_F_UM, G3D_F_ZERO, 0 };
    r5_estado_alvo(&f, &a);
    r5_estado_desenho(&f, &e, NULL);
    VER(r5_pal_triangulos(6) == 2 + 48, "palabras de 6 vertices");
    r5_triangulos(&f, v, 6);
    r5_valla(&f, R5_SCRATCH_REG0 + 8, 0x3D500001u);
    VER(!f.transbordou, "un dibujo cabe");
    VER(percorrer(b, f.n, &c, false), "estado fijo + dibujo se recorren enteros");
    VER(f.n - n0 < 160, "un dibujo sin textura de 6 vertices son %u palabras", f.n - n0);
    VER(c.n_prim == 1 && c.n_vert == 6 && c.vf_cntl == (4u | (3u << 4) | (6u << 16)), "una orden de dibujo con 6 vertices");
    bool iguais = true;
    for (int i = 0; i < 48; i++) if (c.vert[i] != ((const uint32_t *)v)[i]) iguais = false;
    VER(iguais, "los vertices van tal cual (x, y, z, 1/w, color, especular, u, v)");
    VER(R(&c, R5_RB3D_COLOROFFSET0) == a.cor_gpu && R(&c, R5_RB3D_COLORPITCH0) == (144u | (6u << 21)), "destino ARGB8888 con su paso");
    VER(R(&c, R5_ZB_DEPTHOFFSET) == a.z_gpu && R(&c, R5_ZB_DEPTHPITCH) == 144 && R(&c, R5_ZB_FORMAT) == 2, "Z de 24 bits con su paso");
    VER(R(&c, R5_SC_SCISSOR0) == 0 && R(&c, R5_SC_SCISSOR1) == (127u | (95u << 13)), "tijera a todo el destino");
    VER(R(&c, R5_ZB_CNTL) == 6 && R(&c, R5_ZB_ZSTENCILCNTL) == 1, "Z: comparar (menor) y escribir");
    VER(R(&c, R5_RB3D_CBLEND) == 0 && R(&c, R5_RB3D_MASCARA) == 0xF, "sin mezcla, los cuatro canales");
    VER(R(&c, R5_TX_ENABLE) == 0, "sin textura");
    VER(R(&c, R5_US_CODE_ADDR) == 0 && R(&c, R5_US_CODE_RANGE) == 0 && c.n_us == 6, "programa de fragmentos de una instruccion");
    VER(R(&c, R5_SCRATCH_REG0 + 8) == 0x3D500001u && vezes(&c, R5_SCRATCH_REG0 + 8) == 1, "la valla escribe su numero en el apunte");
    VER(b[f.n - 8] == R5_PACOTE0(R5_RB3D_DSTCACHE, 0) && b[f.n - 7] == 0xA && b[f.n - 6] == R5_PACOTE0(R5_ZB_ZCACHE, 0) && b[f.n - 5] == 3 &&
        b[f.n - 4] == R5_PACOTE0(R5_WAIT_UNTIL, 0) && b[f.n - 3] == 0x30000, "antes de la valla: vaciar color y Z y esperar al motor");

    /* Con textura y mezcla; Z de 16 bits. */
    a.zbits = 16;
    r5_tex_t t = { 0xC0300000u, 80, 70, 33 };
    e = (gpu3d_estado_t){ G3D_TEXTURA | G3D_MEZCLA | G3D_TEX_LINEAR | G3D_Z_TESTE, G3D_Z_MENOR_IGUAL, G3D_F_ALFA, G3D_F_UM_MENOS_ALFA, 0 };
    g3d_fila_iniciar(&f, b + n0, 4096 - n0);
    r5_estado_alvo(&f, &a);
    r5_estado_desenho(&f, &e, &t);
    r5_triangulos(&f, v, 3);
    VER(percorrer(b, n0, &c, false) && percorrer(b + n0, f.n, &c, true), "dibujo con textura recorrido entero");
    VER(R(&c, R5_ZB_FORMAT) == 0, "Z de 16 bits");
    VER(R(&c, R5_ZB_CNTL) == 2 && R(&c, R5_ZB_ZSTENCILCNTL) == 2, "Z: comparar (menor o igual) sin escribir");
    VER(R(&c, R5_RB3D_CBLEND) == (5u | (38u << 16) | (39u << 24)), "mezcla alfa, 1 - alfa leyendo el destino");
    VER(R(&c, R5_TX_ENABLE) == 1 && R(&c, R5_TX_OFFSET_0) == t.gpu, "textura 0 en su direccion");
    VER(R(&c, R5_TX_FORMAT0_0) == (69u | (32u << 11) | 0x80000000u), "textura de 70x33 con paso propio");
    VER(R(&c, R5_TX_FORMAT2_0) == 79 && R(&c, R5_TX_FORMAT1_0) == 0xA60C, "paso de 80 texels, ARGB8888");
    VER(R(&c, R5_TX_FILTER0_0) == (2u | (2u << 3) | (2u << 9) | (2u << 11)), "sujeta al borde y bilineal");
    VER(R(&c, R5_US_CODE_ADDR) == 0x10000 && R(&c, R5_US_CODE_RANGE) == 0x10000 && c.n_us == 12, "programa de fragmentos de dos instrucciones");
    e.flags = G3D_TEXTURA | G3D_TEX_REPETE_S | G3D_TEX_REPETE_T;
    g3d_fila_iniciar(&f, b + n0, 4096 - n0);
    r5_estado_desenho(&f, &e, &t);
    VER(percorrer(b + n0, f.n, &c, true) && R(&c, R5_TX_FILTER0_0) == ((1u << 9) | (1u << 11)), "repite y vecino mas proximo");
    VER(R(&c, R5_ZB_CNTL) == 0, "sin G3D_Z_TESTE no hay Z (ni escritura)");

    /* El borrado: flags internos. */
    e = (gpu3d_estado_t){ R5_INT_SEM_COR | R5_INT_Z_FORCAR, G3D_Z_SEMPRE, G3D_F_UM, G3D_F_ZERO, 0 };
    g3d_fila_iniciar(&f, b + n0, 4096 - n0);
    r5_estado_desenho(&f, &e, NULL);
    VER(percorrer(b + n0, f.n, &c, true), "estado del borrado recorrido");
    VER(R(&c, R5_ZB_CNTL) == 6 && R(&c, R5_ZB_ZSTENCILCNTL) == 7 && R(&c, R5_RB3D_MASCARA) == 0, "borrar solo el Z: escribe siempre y no toca el color");

    /* El trozo más grande que manda el driver cabe en su cola y en una orden. */
    static uint32_t g[6144];
    static gpu3d_vert_t vg[R5_VERT_POR_PACOTE];
    memset(vg, 0, sizeof vg);
    g3d_fila_iniciar(&f, g, 6144);
    e = (gpu3d_estado_t){ G3D_TEXTURA, G3D_Z_SEMPRE, G3D_F_UM, G3D_F_ZERO, 0 };
    r5_estado_alvo(&f, &a);
    r5_estado_desenho(&f, &e, &t);
    r5_triangulos(&f, vg, R5_VERT_POR_PACOTE);
    VER(!f.transbordou && R5_VERT_POR_PACOTE % 3 == 0 && R5_VERT_POR_PACOTE * 8 <= 16380, "el dibujo mas grande (%u palabras) cabe en la cola de 6144", f.n);
    VER(percorrer(b, n0, &c, false) && percorrer(g, f.n, &c, true) && c.n_vert == R5_VERT_POR_PACOTE, "y se recorre entero");
}

int main(void)
{
    static const uint32_t fpus[4] = { 2, 5, 8, 8 };
    for (int t = 1; t <= 4; t++) prova_inicio(t, fpus[t - 1]);
    prova_fp();
    prova_desenho();
    printf("r500_3d_unidad: %d comprobaciones, %d fallos\n", feitas, falhas);
    return falhas ? 1 : 0;
}
