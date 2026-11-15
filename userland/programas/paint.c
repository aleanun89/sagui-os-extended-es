// SPDX-License-Identifier: GPL-2.0-or-later
/* paint [archivo] - el programa de dibujo de SaguiOS (D056).
 *
 * Un lienzo de pintura (por defecto 640x480, blanco) con las herramientas de
 * siempre: lápiz, pincel con grosor, goma, aerógrafo, línea,
 * rectángulo y elipse (contorno o rellenos), balde de pintura, cuentagotas y
 * texto con la fuente del sistema. El clic izquierdo pinta con el color primario y
 * el derecho con el secundario (en la paleta: el izquierdo elige el primario,
 * el derecho el secundario). Shift deja la línea a 0/45/90 grados y el rectángulo
 * y la elipse con lados iguales.
 *
 * El lienzo es una imagem_t (imagem.h, D054): abrir lee BMP, PPM/PGM/PBM y PNG;
 * guardar graba BMP de 24 bits (o PPM, si el nombre termina en .ppm).
 *
 * Deshacer: antes de cada operación el lienzo entero se copia a una pila
 * (como máximo DESFAZER_MAX niveles y DESFAZER_BYTES de memoria; al pasar
 * eso, se descarta el nivel más antiguo). Copiar todo el lienzo es simple y,
 * con 640x480, son 1,2 MB por nivel: 16 niveles caben en 20 MB. La misma
 * copia sirve de "fondo" para la vista previa de las figuras: mientras se arrastra,
 * solo se restaura el rectángulo que ocupaba la figura y la figura se dibuja
 * de nuevo en la posición actual.
 *
 * El balde es el relleno por líneas (scanline) con una pila explícita,
 * sin recursión: la pila del programa tiene solo 64 KB.
 *
 * Solo enteros (la libsagui y los programas no usan float, D032): la elipse sale
 * de una raíz cuadrada entera de 64 bits. */
#include "sagui.h"
#include "gui.h"
#include "fontes.h"
#include "imagem.h"

extern const unsigned char fonte_sagui[256][16];

#define LARG_INICIO 800
#define ALT_INICIO  600
#define LARG_MIN 480
#define ALT_MIN  380
#define BARRA   34
#define LATERAL 76
#define PALETA  46
#define ESTADO  20
#define MARGEM  8
#define COR_MESA 0x80878E
#define TELA_PADRAO_L 640
#define TELA_PADRAO_A 480
#define TELA_LADO_MAX 2048
#define DESFAZER_MAX 16
#define DESFAZER_BYTES (24 * 1024 * 1024)

enum { F_LAPIS, F_PINCEL, F_BORRACHA, F_SPRAY, F_LINHA, F_RET, F_RET_CHEIO, F_ELIPSE, F_ELIPSE_CHEIA,
       F_BALDE, F_CONTA_GOTAS, F_TEXTO, QTD_FERRAMENTAS };
static const char *const nome_ferr[QTD_FERRAMENTAS] = {
    "Lápiz", "Pincel", "Goma", "Aerógrafo", "Línea", "Rectángulo", "Rectángulo relleno", "Elipse",
    "Elipse rellena", "Relleno", "Cuentagotas", "Texto" };
static const char *const id_ferr[QTD_FERRAMENTAS] = {   /* para el log */
    "lapiz", "pincel", "goma", "aerografo", "linea", "rectangulo", "rectangulo_relleno", "elipse",
    "elipse_rellena", "relleno", "cuentagotas", "texto" };
static const int grossuras[4] = { 1, 3, 5, 9 };

static const uint32_t cores[28] = {
    0x000000, 0x7F7F7F, 0x880015, 0xED1C24, 0xFF7F27, 0xFFF200, 0x22B14C, 0x00A2E8, 0x3F48CC, 0xA349A4,
    0x0B7A3E, 0x8B5A2B, 0x14337E, 0xE0B92A,
    0xFFFFFF, 0xC3C3C3, 0xB97A57, 0xFFAEC9, 0xFFC90E, 0xEFE4B0, 0xB5E61D, 0x99D9EA, 0x7092BE, 0xC8BFE7,
    0x3CDC6E, 0xD2B48C, 0x7EC8E3, 0xF5F5F0,
};

static jan_t *j;
static imagem_t tela;
static imagem_t pilha[DESFAZER_MAX];
static int niveis;
static int ferr = F_LAPIS, ferr_antes = F_LAPIS;
static int grossura = 1;            /* índice en grossuras[] */
static uint32_t primaria = 0x000000, secundaria = 0xFFFFFF;
static char caminho[200];
static bool modificado;
static char aviso[160];
static int rol_x, rol_y;            /* desplazamiento del lienzo dentro del área */
static int cursor_x = -1, cursor_y = -1;

/* trazo en curso */
static bool desenhando;
static uint32_t cor_traco;
static int ini_x, ini_y, ult_x, ult_y;
static bool shift;
static int fig_x0, fig_y0, fig_x1, fig_y1;   /* rectángulo de la vista previa anterior (lienzo) */
static bool fig_tem;

/* parte del lienzo que cambió y todavía no se copió a la ventana */
static int suj_x0, suj_y0, suj_x1, suj_y1;
static bool sujo;

static botao_t bt[] = {
    { 8, 5, 64, 24, "Nuevo", false },
    { 78, 5, 64, 24, "Abrir", false },
    { 148, 5, 72, 24, "Guardar", false },
    { 226, 5, 112, 24, "Guardar como", false },
    { 344, 5, 84, 24, "Deshacer", false },
    { 434, 5, 76, 24, "Tamaño", false },
};
#define QTD_BT (int)(sizeof bt / sizeof bt[0])

/* ------------------------------------------------------------ íconos 16x16 */

static const char *const ic[QTD_FERRAMENTAS][16] = {
    { "................", "............PP..", "...........PYYP.", "..........PYYYP.", ".........PYYYP..",
      "........PYYYP...", ".......PYYYP....", "......PYYYP.....", ".....PYYYP......", "....PYYYP.......",
      "...PWWWP........", "...PWWP.........", "..PKKP..........", "..PKP...........", "..PP............", "................" },
    { "................", ".............MM.", "............MMM.", "...........MMM..", "..........MMM...",
      ".........MMM....", "........SSS.....", ".......SSSS.....", "......SSSS......", ".....KKKS.......",
      "....KKKKK.......", "....KKKK........", "...KKKK.........", "..KKK...........", ".KK.............", "................" },
    { "................", "................", ".......PPPPPPP..", "......PRRRRRRP..", ".....PRRRRRRPP..",
      "....PRRRRRRPWP..", "...PRRRRRRPWWP..", "..PPPPPPPPWWP...", "..PWWWWWWPWP....", "..PWWWWWWPP.....",
      "..PPPPPPPP......", "................", "................", "................", "................", "................" },
    { "................", "..K..K..........", "....K..K........", ".K..K.K.........", "...K.K..........",
      ".K.K.K.PP.......", "...K..PSSP......", ".....PSSSSP.....", "......PSSSSP....", ".......PSSSSP...",
      "........PSSSSP..", ".........PSSSSP.", "..........PSSP..", "...........PP...", "................", "................" },
    { "................", "..............K.", ".............K..", "............K...", "...........K....",
      "..........K.....", ".........K......", "........K.......", ".......K........", "......K.........",
      ".....K..........", "....K...........", "...K............", "..K.............", ".K..............", "................" },
    { "................", "................", "................", ".KKKKKKKKKKKKKK.", ".K............K.",
      ".K............K.", ".K............K.", ".K............K.", ".K............K.", ".K............K.",
      ".K............K.", ".KKKKKKKKKKKKKK.", "................", "................", "................", "................" },
    { "................", "................", "................", ".KKKKKKKKKKKKKK.", ".KAAAAAAAAAAAAK.",
      ".KAAAAAAAAAAAAK.", ".KAAAAAAAAAAAAK.", ".KAAAAAAAAAAAAK.", ".KAAAAAAAAAAAAK.", ".KAAAAAAAAAAAAK.",
      ".KAAAAAAAAAAAAK.", ".KKKKKKKKKKKKKK.", "................", "................", "................", "................" },
    { "................", "................", ".....KKKKKK.....", "...KK......KK...", "..K..........K..",
      ".K............K.", ".K............K.", ".K............K.", ".K............K.", "..K..........K..",
      "...KK......KK...", ".....KKKKKK.....", "................", "................", "................", "................" },
    { "................", "................", ".....KKKKKK.....", "...KKAAAAAAKK...", "..KAAAAAAAAAAK..",
      ".KAAAAAAAAAAAAK.", ".KAAAAAAAAAAAAK.", ".KAAAAAAAAAAAAK.", ".KAAAAAAAAAAAAK.", "..KAAAAAAAAAAK..",
      "...KKAAAAAAKK...", ".....KKKKKK.....", "................", "................", "................", "................" },
    { "................", "......K.........", ".....KWK........", "....KWWWK.......", "...KWWWWWK......",
      "..KWWWWWWWK.....", ".KWWWWWWWWWK....", "..KWWWWWWWKAA...", "...KWWWWWKAAAA..", "....KWWWK.AAAA..",
      ".....KWK..AAAA..", "......K....AA...", "................", "................", "................", "................" },
    { "................", "...........KK...", "..........KKKK..", "...........KKK..", "..........KWK...",
      ".........KWK....", "........KWK.....", ".......KWK......", "......KWK.......", ".....KWK........",
      "....KAK.........", "...KAAK.........", "...KAK..........", "....K...........", "................", "................" },
    { "................", "................", ".KKKKKKKKKKKK...", ".KKKKKKKKKKKK...", ".KK....KK....K..",
      ".......KK.......", ".......KK.......", ".......KK.......", ".......KK.......", ".......KK.......",
      ".......KK.......", ".......KK.......", "......KKKK......", "................", "................", "................" },
};

static uint32_t paleta_icone(char c)
{
    switch (c) {
    case 'P': return 0x202428;
    case 'K': return 0x1E2328;
    case 'Y': return 0xF2C230;
    case 'W': return 0xF0E0C0;
    case 'M': return 0x8B5A2B;
    case 'S': return 0xC0C6CC;
    case 'R': return 0xE07090;
    case 'A': return 0x2A6FD8;
    default:  return 0xFF000000;
    }
}

/* ------------------------------------------------------------ geometría */

static int area_y1(void) { return j->alt - ESTADO - PALETA; }
static int area_l(void) { return j->larg - LATERAL; }
static int area_a(void) { int a = area_y1() - BARRA; return a > 1 ? a : 1; }
static int tela_x0(void) { return LATERAL + MARGEM - rol_x; }   /* dónde queda el píxel (0,0) del lienzo en la ventana */
static int tela_y0(void) { return BARRA + MARGEM - rol_y; }

static void limitar_rolagem(void)
{
    int max_x = tela.larg + 2 * MARGEM - area_l(), max_y = tela.alt + 2 * MARGEM - area_a();
    if (rol_x > max_x) rol_x = max_x;
    if (rol_y > max_y) rol_y = max_y;
    if (rol_x < 0) rol_x = 0;
    if (rol_y < 0) rol_y = 0;
}

static bool dentro(int x, int y, int rx, int ry, int rl, int ra) { return x >= rx && y >= ry && x < rx + rl && y < ry + ra; }

static void botao_ferr(int i, int *x, int *y) { *x = 6 + (i % 2) * 34; *y = BARRA + 6 + (i / 2) * 34; }
static int grossura_y(int i) { return BARRA + 6 + 6 * 34 + 22 + i * 20; }

/* ------------------------------------------------------------ dibujo en la ventana */

static void sujar(int x0, int y0, int x1, int y1)   /* inclusivo, en coordenadas del lienzo */
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (!sujo) { suj_x0 = x0; suj_y0 = y0; suj_x1 = x1; suj_y1 = y1; sujo = true; return; }
    if (x0 < suj_x0) suj_x0 = x0;
    if (y0 < suj_y0) suj_y0 = y0;
    if (x1 > suj_x1) suj_x1 = x1;
    if (y1 > suj_y1) suj_y1 = y1;
}

/* Copia el rectángulo (x0..x1, y0..y1) del lienzo a la ventana y lo muestra. */
static void mostrar_tela(int x0, int y0, int x1, int y1)
{
    int tx = tela_x0(), ty = tela_y0();
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= tela.larg) x1 = tela.larg - 1;
    if (y1 >= tela.alt) y1 = tela.alt - 1;
    /* recorte por el área visible */
    if (x0 < LATERAL - tx) x0 = LATERAL - tx;
    if (y0 < BARRA - ty) y0 = BARRA - ty;
    if (x1 > j->larg - 1 - tx) x1 = j->larg - 1 - tx;
    if (y1 > area_y1() - 1 - ty) y1 = area_y1() - 1 - ty;
    if (x0 > x1 || y0 > y1) return;
    for (int y = y0; y <= y1; y++)
        memcpy(j->px + (ty + y) * j->larg + tx + x0, tela.px + y * tela.larg + x0, (size_t)(x1 - x0 + 1) * 4);
    jan_mostrar_area(j, tx + x0, ty + y0, x1 - x0 + 1, y1 - y0 + 1);
}

/* Rectángulo recortado por el área del lienzo (la sombra no invade los paneles). */
static void retangulo_na_area(int x, int y, int l, int a, uint32_t c)
{
    int x1 = x + l, y1 = y + a;
    if (x < LATERAL) x = LATERAL;
    if (y < BARRA) y = BARRA;
    if (y1 > area_y1()) y1 = area_y1();
    if (x < x1 && y < y1) g_retangulo(j, x, y, x1 - x, y1 - y, c);
}

static void desenhar_area(void)
{
    limitar_rolagem();
    g_retangulo(j, LATERAL, BARRA, area_l(), area_a(), COR_MESA);
    int tx = tela_x0(), ty = tela_y0();
    /* sombra abajo y a la derecha */
    retangulo_na_area(tx + 3, ty + tela.alt, tela.larg, 3, 0x5A6066);
    retangulo_na_area(tx + tela.larg, ty + 3, 3, tela.alt, 0x5A6066);
    sujo = false;
    mostrar_tela(0, 0, tela.larg - 1, tela.alt - 1);
    jan_mostrar_area(j, LATERAL, BARRA, area_l(), area_a());
}

static void desenhar_barra(void)
{
    g_retangulo(j, 0, 0, j->larg, BARRA, COR_FUNDO);
    g_retangulo(j, 0, BARRA - 1, j->larg, 1, COR_BORDA);
    for (int i = 0; i < QTD_BT; i++) botao_desenhar(j, &bt[i]);
    if (j->larg > 700) g_texto(j, 522, 9, "Ctrl+Z deshace", COR_TEXTO_FRACO);
    jan_mostrar_area(j, 0, 0, j->larg, BARRA);
}

static void desenhar_lateral(void)
{
    int a = area_y1() - BARRA;
    g_retangulo(j, 0, BARRA, LATERAL, a, COR_FUNDO);
    g_retangulo(j, LATERAL - 1, BARRA, 1, a, COR_BORDA);
    for (int i = 0; i < QTD_FERRAMENTAS; i++) {
        int x, y;
        botao_ferr(i, &x, &y);
        bool sel = i == ferr;
        g_retangulo(j, x, y, 32, 32, sel ? 0xCFE0F8 : COR_BOTAO);
        g_relevo(j, x, y, 32, 32, sel);
        if (sel) g_moldura(j, x - 1, y - 1, 34, 34, COR_SELECAO);
        g_desenho(j, x + 8 + sel, y + 8 + sel, ic[i], 16, 16, paleta_icone, 1);
    }
    int yg = grossura_y(0) - 18;
    g_texto(j, 6, yg, "Grosor", COR_TEXTO);
    for (int i = 0; i < 4; i++) {
        int y = grossura_y(i);
        bool sel = i == grossura;
        g_retangulo(j, 6, y, 64, 18, sel ? 0xCFE0F8 : COR_BRANCO);
        g_moldura(j, 6, y, 64, 18, sel ? COR_SELECAO : COR_BORDA);
        int g = grossuras[i];
        g_retangulo(j, 14, y + 9 - g / 2, 48, g, COR_TEXTO);
    }
    jan_mostrar_area(j, 0, BARRA, LATERAL, a);
}

static void desenhar_paleta(void)
{
    int y = area_y1();
    g_retangulo(j, 0, y, j->larg, PALETA, COR_FUNDO);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);
    /* secundario atrás, primario adelante (como en el Paint de siempre) */
    g_retangulo(j, 22, y + 16, 24, 24, secundaria);
    g_moldura(j, 21, y + 15, 26, 26, COR_TEXTO);
    g_retangulo(j, 8, y + 5, 24, 24, primaria);
    g_moldura(j, 7, y + 4, 26, 26, COR_TEXTO);
    for (int i = 0; i < 28; i++) {
        int x = 60 + (i % 14) * 20, yy = y + 4 + (i / 14) * 20;
        g_retangulo(j, x, yy, 18, 18, cores[i]);
        g_moldura(j, x, yy, 18, 18, 0x5A6470);
    }
    if (j->larg > 360 + 140) {
        g_texto(j, 352, y + 6, "Izq.: color principal", COR_TEXTO_FRACO);
        g_texto(j, 352, y + 24, "Der.: color secundario", COR_TEXTO_FRACO);
    }
    jan_mostrar_area(j, 0, y, j->larg, PALETA);
}

static const char *nome_curto(void)
{
    if (!caminho[0]) return "sin nombre";
    const char *b = acha_ultimo_char(caminho, '/');
    return b ? b + 1 : caminho;
}

static void desenhar_estado(void)
{
    int y = j->alt - ESTADO;
    g_retangulo(j, 0, y, j->larg, ESTADO, 0xE6E8EA);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);
    char t[240], pos[24] = "";
    if (cursor_x >= 0) formatar_texto(pos, sizeof pos, "%d, %d", cursor_x, cursor_y);
    if (aviso[0]) formatar_texto(t, sizeof t, "%s", aviso);
    else formatar_texto(t, sizeof t, "%s  |  %s  |  %d x %d  |  %s%s", nome_ferr[ferr], pos[0] ? pos : "-",
                        tela.larg, tela.alt, nome_curto(), modificado ? " *" : "");
    g_texto_n(j, 8, y + 2, t, (j->larg - 16) / LETRA_L, COR_TEXTO);
    jan_mostrar_area(j, 0, y, j->larg, ESTADO);
}

static bool titulo_modificado = true;
static void atualizar_titulo(bool forcar)
{
    if (!forcar && titulo_modificado == modificado) return;
    titulo_modificado = modificado;
    char t[120];
    formatar_texto(t, sizeof t, "Paint - %s%s", nome_curto(), modificado ? " *" : "");
    titulo_janela(j->id, t);
}

static void desenhar_tudo(void)
{
    desenhar_barra();
    desenhar_lateral();
    desenhar_area();
    desenhar_paleta();
    desenhar_estado();
    atualizar_titulo(true);
}

/* ------------------------------------------------------------ primitivas en el lienzo */

static inline void ponto(int x, int y, uint32_t c)
{
    if ((unsigned)x < (unsigned)tela.larg && (unsigned)y < (unsigned)tela.alt) tela.px[y * tela.larg + x] = c;
}

static void linha_h(int x0, int x1, int y, uint32_t c)
{
    if ((unsigned)y >= (unsigned)tela.alt) return;
    if (x0 < 0) x0 = 0;
    if (x1 >= tela.larg) x1 = tela.larg - 1;
    if (x0 > x1) return;
    preencher32(tela.px + y * tela.larg + x0, c, (size_t)(x1 - x0 + 1));
}

static void retangulo_cheio(int x0, int y0, int x1, int y1, uint32_t c)
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    for (int y = y0; y <= y1; y++) linha_h(x0, x1, y, c);
}

/* Un disco de diámetro g centrado en (x, y) (g = 1: un píxel). */
static void disco(int x, int y, int g, uint32_t c)
{
    if (g <= 1) { ponto(x, y, c); return; }
    int r = g / 2;
    for (int dy = -r; dy <= r; dy++) {
        int w = 0;
        while ((w + 1) * (w + 1) + dy * dy <= r * r + r) w++;
        linha_h(x - w, x + w, y + dy, c);
    }
}

/* Bresenham, estampando un disco (g > 1) o un cuadrado (quadrado = true) en cada punto. */
static void linha_grossa(int x0, int y0, int x1, int y1, int g, bool quadrado, uint32_t c)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1, sy = y0 < y1 ? 1 : -1;
    int erro = dx - dy;
    for (;;) {
        if (quadrado) retangulo_cheio(x0 - g / 2, y0 - g / 2, x0 - g / 2 + g - 1, y0 - g / 2 + g - 1, c);
        else disco(x0, y0, g, c);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * erro;
        if (e2 > -dy) { erro -= dy; x0 += sx; }
        if (e2 < dx) { erro += dx; y0 += sy; }
    }
}

static void retangulo(int x0, int y0, int x1, int y1, int g, bool cheio, uint32_t c)
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    if (cheio || x1 - x0 + 1 <= 2 * g || y1 - y0 + 1 <= 2 * g) { retangulo_cheio(x0, y0, x1, y1, c); return; }
    retangulo_cheio(x0, y0, x1, y0 + g - 1, c);
    retangulo_cheio(x0, y1 - g + 1, x1, y1, c);
    retangulo_cheio(x0, y0 + g, x0 + g - 1, y1 - g, c);
    retangulo_cheio(x1 - g + 1, y0 + g, x1, y1 - g, c);
}

static uint32_t raiz64(uint64_t v)
{
    uint64_t r = 0, bit = 1ull << 62;
    while (bit > v) bit >>= 2;
    while (bit) {
        if (v >= r + bit) { v -= r + bit; r = (r >> 1) + bit; }
        else r >>= 1;
        bit >>= 2;
    }
    return (uint32_t)r;
}

static int piso2(int v) { return v >= 0 ? v / 2 : -((-v + 1) / 2); }   /* floor(v / 2) */
static int teto2(int v) { return -piso2(-v); }

/* Media anchura (en medios píxeles) de una elipse de diámetros d_l x d_a en la
 * línea dy2 (también en medios píxeles, desde el centro). */
static int meia_largura(int d_l, int d_a, int dy2)
{
    uint64_t v = (uint64_t)((uint32_t)d_l * (uint32_t)d_l) * (uint32_t)(d_a * d_a - dy2 * dy2);
    return (int)(raiz64(v) / (uint32_t)d_a);
}

/* Elipse inscrita en el rectángulo (x0,y0)-(x1,y1), línea a línea. Todo en
 * medios píxeles, para que los centros de los píxeles caigan en enteros aunque el
 * diámetro sea par. El contorno de grosor g es la elipse menos la elipse
 * achicada g en cada lado. */
static void elipse(int x0, int y0, int x1, int y1, int g, bool cheia, uint32_t c)
{
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { int t = y0; y0 = y1; y1 = t; }
    int dl = x1 - x0 + 1, da = y1 - y0 + 1, cx2 = x0 + x1, cy2 = y0 + y1;
    int il = dl - 2 * g, ia = da - 2 * g;
    if (il <= 0 || ia <= 0) cheia = true;
    for (int y = y0; y <= y1; y++) {
        if ((unsigned)y >= (unsigned)tela.alt) continue;
        int dy2 = 2 * y - cy2;
        int h = meia_largura(dl, da, dy2);
        int xl = teto2(cx2 - h), xr = piso2(cx2 + h);
        if (cheia || dy2 <= -ia || dy2 >= ia) { linha_h(xl, xr, y, c); continue; }
        int hi = meia_largura(il, ia, dy2);
        int il_x = teto2(cx2 - hi), ir_x = piso2(cx2 + hi);
        linha_h(xl, il_x - 1 >= xl ? il_x - 1 : xl, y, c);
        linha_h(ir_x + 1 <= xr ? ir_x + 1 : xr, xr, y, c);
    }
}

static uint32_t semente = 12345;
static int aleatorio(int n) { semente = semente * 1103515245u + 12345u; return (int)((semente >> 16) % (unsigned)n); }

static void spray(int x, int y, int g, uint32_t c)
{
    int r = 4 + g * 3;
    for (int k = 0; k < 12 + g * 4; k++) {
        int dx = aleatorio(2 * r + 1) - r, dy = aleatorio(2 * r + 1) - r;
        if (dx * dx + dy * dy <= r * r) ponto(x + dx, y + dy, c);
    }
    sujar(x - r, y - r, x + r, y + r);
}

/* Balde: rellena la región del mismo color que el de (x, y). Devuelve cuántos píxeles pintó. */
static int preencher(int x, int y, uint32_t c)
{
    if ((unsigned)x >= (unsigned)tela.larg || (unsigned)y >= (unsigned)tela.alt) return 0;
    uint32_t alvo = tela.px[y * tela.larg + x];
    if (alvo == c) return 0;
    int cap = 1024, n = 0, pintados = 0;
    int *p = aloca((size_t)cap * 2 * sizeof(int));
    if (!p) return 0;
    p[n * 2] = x; p[n * 2 + 1] = y; n++;
    int mx0 = x, my0 = y, mx1 = x, my1 = y;
    while (n > 0) {
        n--;
        int sx = p[n * 2], sy = p[n * 2 + 1];
        uint32_t *lin = tela.px + sy * tela.larg;
        if (lin[sx] != alvo) continue;
        int xl = sx, xr = sx;
        while (xl > 0 && lin[xl - 1] == alvo) xl--;
        while (xr < tela.larg - 1 && lin[xr + 1] == alvo) xr++;
        preencher32(lin + xl, c, (size_t)(xr - xl + 1));
        pintados += xr - xl + 1;
        if (xl < mx0) mx0 = xl;
        if (xr > mx1) mx1 = xr;
        if (sy < my0) my0 = sy;
        if (sy > my1) my1 = sy;
        /* en las líneas de arriba y de abajo, una semilla por tramo del color meta */
        for (int ny = sy - 1; ny <= sy + 1; ny += 2) {
            if ((unsigned)ny >= (unsigned)tela.alt) continue;
            uint32_t *l2 = tela.px + ny * tela.larg;
            for (int xx = xl; xx <= xr; xx++) {
                if (l2[xx] != alvo || (xx > xl && l2[xx - 1] == alvo)) continue;
                if (n == cap) {
                    int *q = realoca(p, (size_t)cap * 4 * sizeof(int));
                    if (!q) { libera(p); sujar(mx0, my0, mx1, my1); return pintados; }
                    p = q;
                    cap *= 2;
                }
                p[n * 2] = xx; p[n * 2 + 1] = ny; n++;
            }
        }
    }
    libera(p);
    sujar(mx0, my0, mx1, my1);
    return pintados;
}

/* La fuente de documentos elegida en Configuración (D196): se abre la primera vez que se
 * escribe un texto; sin ninguna, el texto sale con la fuente de mapa de bits del sistema. */
static fonte_t *fuente_texto;
static bool fuente_texto_probada;

/* Texto con la fuente del sistema, ampliada por el grosor (1x a 4x); con una fuente
 * vectorial elegida, de 14*esc+4 píxeles (18 a 60) y con antialias. */
static void texto(int x, int y, const char *t, int esc, uint32_t c)
{
    if (!fuente_texto_probada) { fuente_texto_probada = true; fuente_texto = fuentes_abrir_defecto(FUENTE_DOCUMENTOS); }
    if (fuente_texto) {
        int px = 14 * esc + 4;
        int w = fonte_texto_buf(fuente_texto, tela.px, tela.larg, tela.alt, x, y, t, px, c);
        sujar(x, y, x + w, y + fonte_altura_normal(fuente_texto, px));
        return;
    }
    int x_ini = x;
    for (; *t; t++, x += 8 * esc) {
        const unsigned char *gl = fonte_sagui[(uint8_t)*t];
        for (int lin = 0; lin < 16; lin++)
            for (int col = 0; col < 8; col++)
                if (gl[lin] & (0x80 >> col))
                    retangulo_cheio(x + col * esc, y + lin * esc, x + col * esc + esc - 1, y + lin * esc + esc - 1, c);
    }
    sujar(x_ini, y, x - 1, y + 16 * esc - 1);
}
/* ------------------------------------------------------------ deshacer */

static size_t bytes_imagem(const imagem_t *i) { return (size_t)i->larg * (size_t)i->alt * 4; }

static void descartar_mais_antigo(void)
{
    imagem_liberar(&pilha[0]);
    for (int i = 1; i < niveis; i++) pilha[i - 1] = pilha[i];
    niveis--;
}

/* Guarda el lienzo actual antes de una operación. */
static void guardar_desfazer(void)
{
    size_t por_nivel = bytes_imagem(&tela), total = 0;
    for (int i = 0; i < niveis; i++) total += bytes_imagem(&pilha[i]);
    while (niveis > 0 && (niveis >= DESFAZER_MAX || total + por_nivel > DESFAZER_BYTES)) {
        total -= bytes_imagem(&pilha[0]);
        descartar_mais_antigo();
    }
    imagem_t c;
    while (!imagem_copiar(&c, &tela)) {   /* sin memoria: abre espacio descartando niveles */
        if (niveis == 0) return;
        descartar_mais_antigo();
    }
    pilha[niveis++] = c;
}

static void esquecer_desfazer(void)
{
    while (niveis > 0) imagem_liberar(&pilha[--niveis]);
}

static void desfazer(void)
{
    if (desenhando) return;
    if (niveis == 0) { copia_texto(aviso, "Nada que deshacer", sizeof aviso); desenhar_estado(); return; }
    bool mudou_tamanho = pilha[niveis - 1].larg != tela.larg || pilha[niveis - 1].alt != tela.alt;
    imagem_liberar(&tela);
    tela = pilha[--niveis];
    modificado = true;
    registrar("paint: deshacer (quedan %d niveles)\n", niveis);
    formatar_texto(aviso, sizeof aviso, "Deshecho (quedan %d niveles)", niveis);
    if (mudou_tamanho) desenhar_area();
    else mostrar_tela(0, 0, tela.larg - 1, tela.alt - 1);
    desenhar_estado();
}

/* Durante la vista previa de una figura: vuelve a poner lo que estaba debajo. */
static void restaurar(int x0, int y0, int x1, int y1)
{
    if (niveis == 0) return;
    const imagem_t *f = &pilha[niveis - 1];
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= tela.larg) x1 = tela.larg - 1;
    if (y1 >= tela.alt) y1 = tela.alt - 1;
    for (int y = y0; y <= y1; y++)
        memcpy(tela.px + y * tela.larg + x0, f->px + y * f->larg + x0, (size_t)(x1 - x0 + 1) * 4);
}

/* ------------------------------------------------------------ archivos */

static bool termina_com(const char *s, const char *fim)
{
    int n = (int)tam_texto(s), m = (int)tam_texto(fim);
    if (n < m) return false;
    for (int i = 0; i < m; i++) {
        int a = (uint8_t)s[n - m + i], b = (uint8_t)fim[i];
        if (a >= 'A' && a <= 'Z') a += 32;
        if (a != b) return false;
    }
    return true;
}

static bool salvar_como(void);

static bool salvar(void)
{
    if (!caminho[0]) return salvar_como();
    bool ppm = termina_com(caminho, ".ppm");
    int r = ppm ? imagem_gravar_ppm(caminho, &tela) : imagem_gravar_bmp(caminho, &tela);
    if (r < 0) {
        char m[260];
        formatar_texto(m, sizeof m, "No se ha podido guardar\n%s\n(%s)", caminho, imagem_erro(r));
        mensagem("Paint", m);
        return false;
    }
    modificado = false;
    formatar_texto(aviso, sizeof aviso, "Guardado: %s", caminho);
    registrar("paint: guardado %s %dx%d %s\n", caminho, tela.larg, tela.alt, ppm ? "PPM" : "BMP 24 bits");
    atualizar_titulo(false);
    desenhar_estado();
    return true;
}

static bool salvar_como(void)
{
    char c[200];
    copia_texto(c, caminho[0] ? caminho : "/tmp/dibujo.bmp", sizeof c);
    if (!dialogo_guardar("Guardar como", "Mapa de bits (*.bmp)|*.bmp|PPM (*.ppm)|*.ppm|Todos los archivos (*.*)|*", c, c, sizeof c) || !c[0]) return false;
    copia_texto(caminho, c, sizeof caminho);
    atualizar_titulo(true);
    return salvar();
}

static bool pode_descartar(void)
{
    if (!modificado) return true;
    if (confirmar("Paint", "El dibujo tiene cambios sin guardar.\n¿Guardar antes?")) return salvar();
    return true;
}

static void nova_tela(int l, int a)
{
    imagem_t n;
    if (!imagem_criar(&n, l, a, 0xFFFFFF)) { mensagem("Paint", "No hay memoria para un lienzo de ese tamaño."); return; }
    imagem_liberar(&tela);
    tela = n;
    esquecer_desfazer();
    rol_x = rol_y = 0;
    registrar("paint: lienzo nuevo %dx%d\n", l, a);
}

static bool abrir_arquivo(const char *c)
{
    imagem_t n;
    int r = imagem_ler(c, &n);
    if (r == 0 && (n.larg > TELA_LADO_MAX || n.alt > TELA_LADO_MAX)) { imagem_liberar(&n); r = IMG_ERRO_GRANDE; }
    if (r < 0) {
        char m[260];
        formatar_texto(m, sizeof m, "No se ha podido abrir\n%s\n(%s)", c, imagem_erro(r));
        mensagem("Paint", m);
        return false;
    }
    imagem_liberar(&tela);
    tela = n;
    esquecer_desfazer();
    rol_x = rol_y = 0;
    copia_texto(caminho, c, sizeof caminho);
    /* Solo se graban BMP y PPM: un PNG abierto se guarda como .bmp al lado. */
    if (!termina_com(caminho, ".bmp") && !termina_com(caminho, ".ppm")) {
        char *p = acha_ultimo_char(caminho, '.');
        if (p && !acha_char(p, '/')) *p = 0;
        junta_texto(caminho, ".bmp", sizeof caminho);
    }
    modificado = false;
    aviso[0] = 0;
    registrar("paint: abierto %s %s %dx%d\n", c, tela.formato, tela.larg, tela.alt);
    return true;
}

/* "640x480" o "640 480" -> tamaño */
static bool ler_tamanho(const char *s, int *l, int *a)
{
    *l = texto_para_int(s);
    while (*s >= '0' && *s <= '9') s++;
    while (*s == ' ' || *s == 'x' || *s == 'X' || *s == '*') s++;
    *a = texto_para_int(s);
    return *l >= 1 && *a >= 1 && *l <= TELA_LADO_MAX && *a <= TELA_LADO_MAX;
}

static void novo(void)
{
    if (!pode_descartar()) return;
    char t[32];
    formatar_texto(t, sizeof t, "%dx%d", tela.larg, tela.alt);
    if (!pedir_texto("Nuevo dibujo", "Tamaño del lienzo (ancho x alto):", t, sizeof t)) return;
    int l, a;
    if (!ler_tamanho(t, &l, &a)) { mensagem("Paint", "Tamaño no válido (de 1x1 a 2048x2048)."); return; }
    nova_tela(l, a);
    caminho[0] = 0;
    modificado = false;
    aviso[0] = 0;
    desenhar_tudo();
}

/* Ctrl+V (D125): pega la imagen del portapapeles (Recortes) en la esquina de arriba a la
 * izquierda; si no cabe, el lienzo crece (hasta TELA_LADO_MAX). */
static void colar_imagem_pintura(void)
{
    int d = colar_imagem(NULL, 0);
    int l = d & 0xFFFF, a = (d >> 16) & 0xFFFF;
    if (d <= 0 || l == 0 || a == 0) { copia_texto(aviso, "No hay ninguna imagen copiada.", sizeof aviso); return; }
    uint32_t *px = aloca((size_t)l * (size_t)a * 4);
    if (!px || colar_imagem(px, (unsigned)(l * a)) <= 0) { libera(px); copia_texto(aviso, "No se pudo pegar la imagen.", sizeof aviso); return; }
    int nl = l > tela.larg ? l : tela.larg, na = a > tela.alt ? a : tela.alt;
    if (nl > TELA_LADO_MAX || na > TELA_LADO_MAX) { libera(px); copia_texto(aviso, "La imagen es demasiado grande.", sizeof aviso); return; }
    guardar_desfazer();
    if (nl != tela.larg || na != tela.alt) {
        imagem_t n;
        if (!imagem_criar(&n, nl, na, 0xFFFFFF)) { libera(px); copia_texto(aviso, "No hay memoria.", sizeof aviso); return; }
        for (int y = 0; y < tela.alt; y++) memcpy(n.px + y * nl, tela.px + y * tela.larg, (size_t)tela.larg * 4);
        imagem_liberar(&tela);
        tela = n;
    }
    for (int y = 0; y < a; y++) memcpy(tela.px + y * tela.larg, px + y * l, (size_t)l * 4);
    libera(px);
    modificado = true;
    registrar("paint: pegada imagen %dx%d (lienzo %dx%d)\n", l, a, tela.larg, tela.alt);
    desenhar_area();
    desenhar_estado();
}

/* Cambia el tamaño del lienzo sin escalar: corta o agrega blanco. */
static void mudar_tamanho(void)
{
    char t[32];
    formatar_texto(t, sizeof t, "%dx%d", tela.larg, tela.alt);
    if (!pedir_texto("Tamaño del lienzo", "Nuevo tamaño (ancho x alto):", t, sizeof t)) return;
    int l, a;
    if (!ler_tamanho(t, &l, &a)) { mensagem("Paint", "Tamaño no válido (de 1x1 a 2048x2048)."); return; }
    imagem_t n;
    if (!imagem_criar(&n, l, a, 0xFFFFFF)) { mensagem("Paint", "No hay memoria para un lienzo de ese tamaño."); return; }
    guardar_desfazer();
    int cl = l < tela.larg ? l : tela.larg, ca = a < tela.alt ? a : tela.alt;
    for (int y = 0; y < ca; y++) memcpy(n.px + y * l, tela.px + y * tela.larg, (size_t)cl * 4);
    imagem_liberar(&tela);
    tela = n;
    modificado = true;
    registrar("paint: lienzo cambiado a %dx%d\n", l, a);
    desenhar_area();
    desenhar_estado();
}

static void abrir_pedindo(void)
{
    if (!pode_descartar()) return;
    char c[200];
    copia_texto(c, caminho[0] ? caminho : "/imagenes/", sizeof c);
    if (dialogo_abrir("Abrir", "Imágenes|*.bmp;*.ppm;*.pgm;*.png|Todos los archivos (*.*)|*", c, c, sizeof c) && c[0] && abrir_arquivo(c))
        desenhar_tudo();
}

/* ------------------------------------------------------------ herramientas */

static void escolher_ferramenta(int f)
{
    if (f == ferr) return;
    if (f == F_CONTA_GOTAS) ferr_antes = ferr;
    ferr = f;
    registrar("paint: herramienta %s\n", id_ferr[ferr]);
    desenhar_lateral();
    desenhar_estado();
}

/* Shift: línea a 0/45/90 grados; rectángulo y elipse con lados iguales. */
static void restringir(int *x, int *y)
{
    if (!shift) return;
    int dx = *x - ini_x, dy = *y - ini_y;
    int ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    if (ferr == F_LINHA) {
        if (ax > 2 * ay) { *y = ini_y; return; }
        if (ay > 2 * ax) { *x = ini_x; return; }
    }
    int m = ax > ay ? ax : ay;
    *x = ini_x + (dx < 0 ? -m : m);
    *y = ini_y + (dy < 0 ? -m : m);
}

static bool eh_figura(int f) { return f >= F_LINHA && f <= F_ELIPSE_CHEIA; }

/* Dibuja la figura de (ini_x, ini_y) a (x, y) encima del fondo guardado. */
static void figura(int x, int y)
{
    restringir(&x, &y);
    int g = grossuras[grossura];
    if (fig_tem) restaurar(fig_x0, fig_y0, fig_x1, fig_y1);
    int m = g / 2 + 1;
    int x0 = (ini_x < x ? ini_x : x) - m, x1 = (ini_x > x ? ini_x : x) + m;
    int y0 = (ini_y < y ? ini_y : y) - m, y1 = (ini_y > y ? ini_y : y) + m;
    switch (ferr) {
    case F_LINHA:        linha_grossa(ini_x, ini_y, x, y, g, false, cor_traco); break;
    case F_RET:          retangulo(ini_x, ini_y, x, y, g, false, cor_traco); break;
    case F_RET_CHEIO:    retangulo(ini_x, ini_y, x, y, g, true, cor_traco); break;
    case F_ELIPSE:       elipse(ini_x, ini_y, x, y, g, false, cor_traco); break;
    case F_ELIPSE_CHEIA: elipse(ini_x, ini_y, x, y, g, true, cor_traco); break;
    }
    if (fig_tem) sujar(fig_x0, fig_y0, fig_x1, fig_y1);
    sujar(x0, y0, x1, y1);
    fig_x0 = x0; fig_y0 = y0; fig_x1 = x1; fig_y1 = y1;
    fig_tem = true;
    ult_x = x; ult_y = y;
}

static void traco_ate(int x, int y)
{
    int g = grossuras[grossura];
    switch (ferr) {
    case F_LAPIS:
        linha_grossa(ult_x, ult_y, x, y, 1, false, cor_traco);
        sujar(ult_x, ult_y, x, y);
        break;
    case F_PINCEL:
        linha_grossa(ult_x, ult_y, x, y, g, false, cor_traco);
        sujar(ult_x - g, ult_y - g, x + g, y + g);   /* las dos esquinas opuestas: sujar() ordena */
        sujar(x - g, y - g, ult_x + g, ult_y + g);
        break;
    case F_BORRACHA: {
        int q = g * 2 + 4;   /* la goma es un cuadrado más grande que el pincel */
        linha_grossa(ult_x, ult_y, x, y, q, true, cor_traco);
        sujar(ult_x - q, ult_y - q, x + q, y + q);
        sujar(x - q, y - q, ult_x + q, ult_y + q);
        break;
    }
    case F_SPRAY:
        spray(x, y, g, cor_traco);
        break;
    default:
        if (eh_figura(ferr)) { figura(x, y); return; }
    }
    ult_x = x; ult_y = y;
}

static void comecar(int x, int y, int botoes, unsigned mods)
{
    bool dir = !(botoes & 1);
    uint32_t c = dir ? secundaria : primaria;
    shift = mods & 1;
    aviso[0] = 0;
    if (ferr == F_CONTA_GOTAS) {
        if ((unsigned)x >= (unsigned)tela.larg || (unsigned)y >= (unsigned)tela.alt) return;
        uint32_t v = tela.px[y * tela.larg + x];
        if (dir) secundaria = v; else primaria = v;
        registrar("paint: cuentagotas %06x en (%d,%d) -> color %s\n", v, x, y, dir ? "secundario" : "principal");
        desenhar_paleta();
        escolher_ferramenta(ferr_antes);
        return;
    }
    if ((unsigned)x >= (unsigned)tela.larg || (unsigned)y >= (unsigned)tela.alt) {
        if (!eh_figura(ferr)) return;   /* las figuras pueden empezar afuera y entrar en el lienzo */
    }
    if (ferr == F_BALDE) {
        guardar_desfazer();
        int n = preencher(x, y, c);
        if (n == 0) { if (niveis) imagem_liberar(&pilha[--niveis]); return; }
        modificado = true;
        registrar("paint: relleno en (%d,%d) con %06x: %d pixeles\n", x, y, c, n);
        return;
    }
    if (ferr == F_TEXTO) {
        char t[100] = "";
        if (!pedir_texto("Texto", "Texto a escribir en el dibujo:", t, sizeof t) || !t[0]) return;
        guardar_desfazer();
        texto(x, y, t, grossura + 1, c);
        modificado = true;
        registrar("paint: texto \"%s\" en (%d,%d) %s\n", t, x, y, fuente_texto ? "con fuente vectorial" : "con la fuente del sistema");
        return;
    }
    guardar_desfazer();
    desenhando = true;
    cor_traco = ferr == F_BORRACHA ? (dir ? primaria : secundaria) : c;
    ini_x = ult_x = x;
    ini_y = ult_y = y;
    fig_tem = false;
    if (eh_figura(ferr)) figura(x, y);
    else traco_ate(x, y);
}

static void terminar(void)
{
    if (!desenhando) return;
    desenhando = false;
    modificado = true;
    registrar("paint: trazo %s de (%d,%d) a (%d,%d) color %06x\n", id_ferr[ferr], ini_x, ini_y, ult_x, ult_y, cor_traco);
    desenhar_estado();
}

/* ------------------------------------------------------------ eventos */

static void rolar(int dx, int dy)
{
    int ax = rol_x, ay = rol_y;
    rol_x += dx;
    rol_y += dy;
    limitar_rolagem();
    if (rol_x != ax || rol_y != ay) desenhar_area();
}

static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) { if (desenhando) terminar(); return !pode_descartar(); }
    if (jan_redimensionar(j, e)) {
        registrar("paint: ventana %dx%d\n", j->larg, j->alt);
        desenhar_tudo();
        return true;
    }
    if (e->tipo == EV_TECLA) {
        unsigned t = e->tecla;
        aviso[0] = 0;
        if (t == 26) desfazer();                                       /* Ctrl+Z */
        else if (t == 19) salvar();                                    /* Ctrl+S */
        else if (t == 15) abrir_pedindo();                             /* Ctrl+O */
        else if (t == 14) novo();                                      /* Ctrl+N */
        else if (t == 22) colar_imagem_pintura();                      /* Ctrl+V: imagen del portapapeles (D125) */
        else if (t == TECLA_ESQUERDA) rolar(-48, 0);
        else if (t == TECLA_DIREITA) rolar(48, 0);
        else if (t == TECLA_CIMA) rolar(0, -48);
        else if (t == TECLA_BAIXO) rolar(0, 48);
        else if (t == TECLA_PGUP) rolar(0, -area_a() + 32);
        else if (t == TECLA_PGDN) rolar(0, area_a() - 32);
        return true;
    }
    if (e->tipo == EV_RODA) {
        int n = (int)e->cliques * 48;
        if (e->modificadores & 1) rolar(n, 0); else rolar(0, n);
        return true;
    }
    if (e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU)
        for (int i = 0; i < QTD_BT; i++)
            if (botao_tratar(j, &bt[i], e)) {
                aviso[0] = 0;
                if (i == 0) novo();
                else if (i == 1) abrir_pedindo();
                else if (i == 2) salvar();
                else if (i == 3) salvar_como();
                else if (i == 4) desfazer();
                else mudar_tamanho();
                atualizar_titulo(false);
                return true;
            }
    int tx = e->x - tela_x0(), ty = e->y - tela_y0();
    bool na_area = dentro(e->x, e->y, LATERAL, BARRA, area_l(), area_a());
    if (e->tipo == EV_SOLTOU) { terminar(); return true; }
    if (e->tipo == EV_MOVER) {
        if (desenhando && (e->botoes & 3)) { shift = e->modificadores & 1; traco_ate(tx, ty); }
        else if (desenhando) terminar();
        int nx = -1, ny = -1;
        if (na_area && (unsigned)tx < (unsigned)tela.larg && (unsigned)ty < (unsigned)tela.alt) { nx = tx; ny = ty; }
        if (nx != cursor_x || ny != cursor_y) { cursor_x = nx; cursor_y = ny; }
        return true;
    }
    if (e->tipo != EV_CLIQUE) return true;
    if (na_area) { comecar(tx, ty, (int)e->botoes, e->modificadores); return true; }
    /* herramientas y grosores */
    for (int i = 0; i < QTD_FERRAMENTAS; i++) {
        int x, y;
        botao_ferr(i, &x, &y);
        if (dentro(e->x, e->y, x, y, 32, 32)) { escolher_ferramenta(i); return true; }
    }
    for (int i = 0; i < 4; i++)
        if (dentro(e->x, e->y, 6, grossura_y(i), 64, 18)) {
            grossura = i;
            registrar("paint: grosor %d\n", grossuras[i]);
            desenhar_lateral();
            return true;
        }
    /* paleta */
    int py = area_y1();
    for (int i = 0; i < 28; i++) {
        int x = 60 + (i % 14) * 20, yy = py + 4 + (i / 14) * 20;
        if (!dentro(e->x, e->y, x, yy, 18, 18)) continue;
        if (e->botoes & 2) secundaria = cores[i]; else primaria = cores[i];
        registrar("paint: color %s %06x\n", (e->botoes & 2) ? "secundario" : "principal", cores[i]);
        desenhar_paleta();
        return true;
    }
    /* clic en las muestras: cambia primario y secundario */
    if (dentro(e->x, e->y, 8, py + 5, 38, 35)) {
        uint32_t t = primaria; primaria = secundaria; secundaria = t;
        desenhar_paleta();
    }
    return true;
}

int principal(int argc, char **argv)
{
    j = jan_abrir("Paint - sin nombre", LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, LARG_MIN, ALT_MIN);
    if (!imagem_criar(&tela, TELA_PADRAO_L, TELA_PADRAO_A, 0xFFFFFF)) return 1;
    if (argc > 1) {
        info_arquivo_t inf;
        if (info_arquivo(argv[1], &inf) >= 0) abrir_arquivo(argv[1]);
        else {
            copia_texto(caminho, argv[1], sizeof caminho);   /* archivo nuevo: graba ahí */
            formatar_texto(aviso, sizeof aviso, "Archivo nuevo: %s", caminho);
        }
    }
    desenhar_tudo();
    registrar("paint: pronto, lienzo %dx%d en (%d,%d)\n", tela.larg, tela.alt, tela_x0(), tela_y0());

    evento_t e;
    bool continuar = true;
    int cx = cursor_x, cy = cursor_y;
    while (continuar) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        continuar = tratar(&e);
        /* une los eventos que ya llegaron antes de mostrar */
        while (continuar && proximo_evento(j->id, &e, false) == 1) continuar = tratar(&e);
        if (!continuar) break;
        if (sujo) { sujo = false; mostrar_tela(suj_x0, suj_y0, suj_x1, suj_y1); }
        if (cx != cursor_x || cy != cursor_y) { cx = cursor_x; cy = cursor_y; desenhar_estado(); }
        atualizar_titulo(false);
    }
    registrar("paint: fechado\n");
    jan_fechar(j);
    return 0;
}
