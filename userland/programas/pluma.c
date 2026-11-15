// SPDX-License-Identifier: GPL-2.0-or-later
/* pluma [archivo] - el procesador de textos de SaguiOS (mejora 116, D291), con el aspecto
 * de un Word 97 simplificado: barra de menús, barras de herramientas Estándar y Formato con
 * iconos, regla con márgenes y sangrías, vista de página sobre fondo gris y barra de estado.
 * Formatos: RTF (leer y escribir, para intercambiar con Word y LibreOffice), texto plano y
 * exportación a PDF (sustituye a la impresión). Sin macros.
 *
 * Modos sin ventana, para las pruebas:
 *   pluma --convertir entrada salida [--carta]   .rtf/.txt -> .rtf/.txt/.pdf
 *   pluma --info archivo                         páginas, palabras y párrafos
 */
#include "sagui.h"
#include "gui.h"
#include "rtf.h"

#define BARRA_Y     20
#define FERR_A      28
#define REGUA_A     20
#define ESTADO_A    22

enum {
    A_NOVO = 1, A_ABRIR, A_GUARDAR, A_GUARDAR_COMO, A_PDF, A_PAPEL_A4, A_PAPEL_CARTA, A_SAIR,
    A_DESFAZER, A_REFAZER, A_CORTAR, A_COPIAR, A_COLAR, A_TUDO, A_BUSCAR, A_SUBSTITUIR,
    A_Z50, A_Z75, A_Z100, A_Z150, A_Z200, A_REGUA,
    A_DATA, A_PAGINA,
    A_FONTE, A_PARAGRAFO, A_VINETAS, A_NEGRITA, A_CURSIVA, A_SUBLINHADO, A_ESQ, A_CENTRO, A_DIR, A_JUST,
    A_SANG_MAIS, A_SANG_MENOS,
    A_CONTAR, A_ACERCA,
};

static jan_t *j;
static rte_t *doc;
static char caminho[200];
static bool regua_visivel = true;

static barra_menu_t menu;
static barra_ferr_t ferr1, ferr2;
static barra_item_t it1[16], it2[16];
static combo_t c_fonte, c_tam, c_zoom;
static regua_t regua;
static barra_estado_t estado;
static char ult_titulo[128];

static const char *const nomes_fonte[3] = { "Arial", "Times New Roman", "Courier New" };
static const char *const tams_txt[] = { "8", "9", "10", "11", "12", "14", "16", "18", "20", "22", "24", "26", "28", "36", "48", "72" };
static const char *const zooms_txt[] = { "50%", "75%", "100%", "150%", "200%" };

/* ------------------------------------------------------------ iconos 16x16 propios */

#define R(x_, y_, w_, h_, c_) g_retangulo(j, x + (x_), y + (y_), (w_), (h_), (c_))
#define COR(a, c) ((a) ? (c) : 0x808080)

static void folha(jan_t *jj, int x, int y, bool a, int dx, int dy, uint32_t fundo)
{
    jan_t *j = jj;
    R(dx, dy, 9, 11, COR(a, W98_NEGRO));
    R(dx + 1, dy + 1, 7, 9, COR(a, fundo));
    R(dx + 6, dy, 3, 3, COR(a, W98_FACE));
    R(dx + 6, dy + 2, 3, 1, COR(a, W98_NEGRO));
    R(dx + 6, dy, 1, 3, COR(a, W98_NEGRO));
}

static void ic_novo(jan_t *j, int x, int y, bool a) { folha(j, x, y, a, 3, 2, W98_LUZ); }
static void ic_abrir(jan_t *j, int x, int y, bool a)
{
    R(1, 4, 7, 2, COR(a, 0x808000)); R(1, 5, 13, 9, COR(a, 0xFFFF00)); R(1, 5, 13, 1, COR(a, 0x808000));
    R(1, 13, 13, 1, COR(a, 0x808000)); R(1, 5, 1, 9, COR(a, 0x808000)); R(13, 5, 1, 9, COR(a, 0x808000));
    R(4, 8, 11, 6, COR(a, 0xC0C000)); R(4, 8, 11, 1, COR(a, 0x808000)); R(14, 8, 1, 6, COR(a, 0x808000));
}
static void ic_guardar(jan_t *j, int x, int y, bool a)
{
    R(1, 1, 14, 14, COR(a, W98_NEGRO)); R(2, 2, 12, 12, COR(a, 0x0000A0));
    R(4, 2, 8, 5, COR(a, W98_LUZ)); R(5, 3, 3, 3, COR(a, W98_SOMBRA)); R(4, 9, 8, 5, COR(a, W98_FACE));
    R(6, 10, 4, 3, COR(a, W98_OSCURO));
}
static void ic_pdf(jan_t *j, int x, int y, bool a)
{
    folha(j, x, y, a, 3, 1, W98_LUZ);
    R(1, 8, 14, 7, COR(a, 0xC00000));
    R(2, 9, 1, 5, COR(a, W98_LUZ)); R(2, 9, 3, 1, COR(a, W98_LUZ)); R(4, 9, 1, 3, COR(a, W98_LUZ)); R(2, 11, 3, 1, COR(a, W98_LUZ));
    R(6, 9, 1, 5, COR(a, W98_LUZ)); R(6, 9, 3, 1, COR(a, W98_LUZ)); R(8, 9, 1, 5, COR(a, W98_LUZ)); R(6, 13, 3, 1, COR(a, W98_LUZ));
    R(10, 9, 1, 5, COR(a, W98_LUZ)); R(10, 9, 3, 1, COR(a, W98_LUZ)); R(10, 11, 3, 1, COR(a, W98_LUZ));
}
static void ic_cortar(jan_t *j, int x, int y, bool a)
{
    g_linha(j, x + 4, y + 1, x + 10, y + 10, COR(a, W98_NEGRO)); g_linha(j, x + 11, y + 1, x + 5, y + 10, COR(a, W98_NEGRO));
    R(2, 10, 4, 4, COR(a, 0xC00000)); R(3, 11, 2, 2, COR(a, W98_FACE)); R(9, 10, 4, 4, COR(a, 0xC00000)); R(10, 11, 2, 2, COR(a, W98_FACE));
}
static void ic_copiar(jan_t *j, int x, int y, bool a)
{
    folha(j, x, y, a, 1, 1, W98_LUZ); folha(j, x, y, a, 6, 4, W98_LUZ);
}
static void ic_colar(jan_t *j, int x, int y, bool a)
{
    R(1, 2, 11, 13, COR(a, 0x804000)); R(2, 3, 9, 11, COR(a, 0xC08040)); R(4, 1, 5, 3, COR(a, W98_OSCURO));
    folha(j, x, y, a, 6, 5, W98_LUZ);
}
static void ic_desfazer(jan_t *j, int x, int y, bool a)
{
    R(3, 3, 1, 1, COR(a, 0x000080)); R(2, 4, 3, 1, COR(a, 0x000080)); R(1, 5, 5, 1, COR(a, 0x000080));
    R(3, 6, 8, 2, COR(a, 0x000080)); R(10, 7, 3, 3, COR(a, 0x000080)); R(11, 10, 2, 2, COR(a, 0x000080)); R(3, 10, 8, 2, COR(a, 0x000080));
    R(4, 8, 6, 1, COR(a, 0x000080));
}
static void ic_refazer(jan_t *j, int x, int y, bool a)
{
    R(12, 3, 1, 1, COR(a, 0x000080)); R(11, 4, 3, 1, COR(a, 0x000080)); R(10, 5, 5, 1, COR(a, 0x000080));
    R(4, 6, 8, 2, COR(a, 0x000080)); R(2, 7, 3, 3, COR(a, 0x000080)); R(2, 10, 2, 2, COR(a, 0x000080)); R(4, 10, 8, 2, COR(a, 0x000080));
    R(5, 8, 6, 1, COR(a, 0x000080));
}
static void ic_buscar(jan_t *j, int x, int y, bool a)
{
    R(3, 1, 6, 1, COR(a, W98_NEGRO)); R(3, 8, 6, 1, COR(a, W98_NEGRO)); R(1, 3, 1, 4, COR(a, W98_NEGRO)); R(10, 3, 1, 4, COR(a, W98_NEGRO));
    R(2, 2, 1, 1, COR(a, W98_NEGRO)); R(9, 2, 1, 1, COR(a, W98_NEGRO)); R(2, 7, 1, 1, COR(a, W98_NEGRO)); R(9, 7, 1, 1, COR(a, W98_NEGRO));
    R(3, 2, 6, 6, COR(a, 0xC0E0FF));
    g_linha(j, x + 9, y + 8, x + 14, y + 14, COR(a, W98_OSCURO)); g_linha(j, x + 10, y + 8, x + 15, y + 14, COR(a, W98_OSCURO));
}
static void linhas_alin(jan_t *j, int x, int y, bool a, const int *xs, const int *ls)
{
    for (int i = 0; i < 5; i++) R(xs[i], 2 + i * 3, ls[i], 2, COR(a, W98_NEGRO));
}
static void ic_esq(jan_t *j, int x, int y, bool a) { static const int xs[5] = { 2, 2, 2, 2, 2 }, ls[5] = { 12, 8, 12, 8, 12 }; linhas_alin(j, x, y, a, xs, ls); }
static void ic_centro(jan_t *j, int x, int y, bool a) { static const int xs[5] = { 2, 4, 2, 4, 2 }, ls[5] = { 12, 8, 12, 8, 12 }; linhas_alin(j, x, y, a, xs, ls); }
static void ic_dir(jan_t *j, int x, int y, bool a) { static const int xs[5] = { 2, 6, 2, 6, 2 }, ls[5] = { 12, 8, 12, 8, 12 }; linhas_alin(j, x, y, a, xs, ls); }
static void ic_just(jan_t *j, int x, int y, bool a) { static const int xs[5] = { 2, 2, 2, 2, 2 }, ls[5] = { 12, 12, 12, 12, 12 }; linhas_alin(j, x, y, a, xs, ls); }
static void ic_vinetas(jan_t *j, int x, int y, bool a)
{
    for (int i = 0; i < 3; i++) { R(2, 3 + i * 4, 2, 2, COR(a, W98_NEGRO)); R(6, 3 + i * 4, 8, 2, COR(a, W98_NEGRO)); }
}
static void ic_sang_mais(jan_t *j, int x, int y, bool a)
{
    R(7, 2, 7, 2, COR(a, W98_NEGRO)); R(7, 6, 7, 2, COR(a, W98_NEGRO)); R(2, 10, 12, 2, COR(a, W98_NEGRO));
    R(2, 4, 1, 5, COR(a, 0x000080)); R(3, 5, 1, 3, COR(a, 0x000080)); R(4, 6, 1, 1, COR(a, 0x000080)); R(1, 6, 3, 1, COR(a, 0x000080));
}
static void ic_sang_menos(jan_t *j, int x, int y, bool a)
{
    R(7, 2, 7, 2, COR(a, W98_NEGRO)); R(7, 6, 7, 2, COR(a, W98_NEGRO)); R(2, 10, 12, 2, COR(a, W98_NEGRO));
    R(4, 4, 1, 5, COR(a, 0x000080)); R(3, 5, 1, 3, COR(a, 0x000080)); R(2, 6, 1, 1, COR(a, 0x000080)); R(2, 6, 3, 1, COR(a, 0x000080));
}

/* ------------------------------------------------------------ estado del documento */

static const char *nome_curto(void)
{
    if (!caminho[0]) return "Documento1";
    const char *b = acha_ultimo_char(caminho, '/');
    return b ? b + 1 : caminho;
}

static void titulo(void)
{
    char t[160];
    formatar_texto(t, sizeof t, "%s - Pluma%s", nome_curto(), rte_modificado(doc) ? " *" : "");
    if (compara_texto(t, ult_titulo) == 0) return;
    copia_texto(ult_titulo, t, sizeof ult_titulo);
    titulo_janela(j->id, t);
    registrar("pluma: titulo %s\n", t);
}

static void estado_atualizar(void)
{
    char t[64];
    int lin, col;
    rte_linha_coluna(doc, &lin, &col);
    formatar_texto(t, sizeof t, "Pág. %d/%d", rte_pagina_atual(doc), rte_paginas(doc));
    barra_estado_texto(&estado, 0, t);
    formatar_texto(t, sizeof t, "Lín %d", lin);
    barra_estado_texto(&estado, 1, t);
    formatar_texto(t, sizeof t, "Col %d", col);
    barra_estado_texto(&estado, 2, t);
    int pw, ph;
    rte_papel_obter(doc, &pw, &ph, NULL, NULL, NULL, NULL);
    barra_estado_texto(&estado, 3, pw == 12240 ? "Carta" : pw == 11906 ? "A4" : "Personal.");
    barra_estado_texto(&estado, 4, rte_modificado(doc) ? "Modificado" : "");
    barra_estado_desenhar(j, &estado);
}

static void regua_sincronizar(void)
{
    int pw, me, md;
    rte_papel_obter(doc, &pw, NULL, &me, &md, NULL, NULL);
    rte_par_t p;
    rte_par_atual(doc, &p);
    regua.pagina = pw; regua.m_esq = me; regua.m_dir = md;
    regua.sang_esq = p.esq; regua.sang_prim = p.prim; regua.sang_dir = p.dir;
    regua.zoom = rte_zoom_obter(doc);
    regua.ox = rte_pagina_x(doc);
    if (regua_visivel) regua_desenhar(j, &regua);
}

/* Refleja el formato bajo el cursor en las barras. */
static void ui_atualizar(void)
{
    rte_fmt_t f;
    rte_fmt_atual(doc, &f);
    rte_par_t p;
    rte_par_atual(doc, &p);
    barra_ferr_marcar(&ferr2, A_NEGRITA, (f.est & RTE_NEGRITA) != 0);
    barra_ferr_marcar(&ferr2, A_CURSIVA, (f.est & RTE_CURSIVA) != 0);
    barra_ferr_marcar(&ferr2, A_SUBLINHADO, (f.est & RTE_SUBLINHADO) != 0);
    barra_ferr_marcar(&ferr2, A_ESQ, p.alin == RTE_ESQ);
    barra_ferr_marcar(&ferr2, A_CENTRO, p.alin == RTE_CENTRO);
    barra_ferr_marcar(&ferr2, A_DIR, p.alin == RTE_DIR);
    barra_ferr_marcar(&ferr2, A_JUST, p.alin == RTE_JUST);
    barra_ferr_marcar(&ferr2, A_VINETAS, p.marcador != 0);
    barra_ferr_desenhar(j, &ferr2);
    if (!c_fonte.foco) combo_definir(&c_fonte, nomes_fonte[f.fam < 3 ? f.fam : 0]);
    combo_desenhar(j, &c_fonte);
    if (!c_tam.foco) { char t[8]; formatar_texto(t, sizeof t, "%d", f.tam); combo_definir(&c_tam, t); }
    combo_desenhar(j, &c_tam);
    char z[12];
    formatar_texto(z, sizeof z, "%d%%", rte_zoom_obter(doc));
    combo_definir(&c_zoom, z);
    combo_desenhar(j, &c_zoom);
    regua_sincronizar();
    estado_atualizar();
    titulo();
}

static void layout(void)
{
    int l = j->larg, a = j->alt;
    barra_menu_iniciar(&menu, 0, 0, l);
    barra_ferr_iniciar(&ferr1, 0, BARRA_Y, l, it1, 14);
    barra_ferr_iniciar(&ferr2, 0, BARRA_Y + FERR_A, l, it2, 15);
    combo_iniciar(&c_fonte, barra_ferr_x(&ferr2, 0), BARRA_Y + FERR_A + 3, 164, nomes_fonte, 3, false, 7101);
    combo_iniciar(&c_tam, barra_ferr_x(&ferr2, 1), BARRA_Y + FERR_A + 3, 52, tams_txt, 16, true, 7102);
    combo_iniciar(&c_zoom, barra_ferr_x(&ferr1, 13), BARRA_Y + 3, 70, zooms_txt, 5, false, 7103);
    int y = BARRA_Y + 2 * FERR_A;
    regua_iniciar(&regua, 0, y, l);
    if (regua_visivel) y += REGUA_A;
    rte_mover(doc, 0, y, l, a - y - ESTADO_A);
    static const int larg[5] = { 100, 70, 70, 70, 100 };
    barra_estado_iniciar(&estado, 0, a - ESTADO_A, l, 5, larg);
}

static void itens_iniciar(void)
{
    static const barra_item_t u1[14] = {
        { A_NOVO, ic_novo, NULL, false, false, false, false, 0 },
        { A_ABRIR, ic_abrir, NULL, false, false, false, false, 0 },
        { A_GUARDAR, ic_guardar, NULL, false, false, false, false, 0 },
        { A_PDF, ic_pdf, NULL, false, false, false, false, 0 },
        { 0, NULL, NULL, false, false, false, false, 0 },
        { A_CORTAR, ic_cortar, NULL, false, false, false, false, 0 },
        { A_COPIAR, ic_copiar, NULL, false, false, false, false, 0 },
        { A_COLAR, ic_colar, NULL, false, false, false, false, 0 },
        { 0, NULL, NULL, false, false, false, false, 0 },
        { A_DESFAZER, ic_desfazer, NULL, false, false, false, false, 0 },
        { A_REFAZER, ic_refazer, NULL, false, false, false, false, 0 },
        { A_BUSCAR, ic_buscar, NULL, false, false, false, false, 0 },
        { 0, NULL, NULL, false, false, false, false, 0 },
        { -1, NULL, NULL, false, false, false, false, 74 },
    };
    static const barra_item_t u2[15] = {
        { -1, NULL, NULL, false, false, false, false, 170 },
        { -1, NULL, NULL, false, false, false, false, 58 },
        { 0, NULL, NULL, false, false, false, false, 0 },
        { A_NEGRITA, NULL, "N", true, true, false, false, 0 },
        { A_CURSIVA, NULL, "K", false, true, false, false, 0 },
        { A_SUBLINHADO, NULL, "S", false, true, false, false, 0 },
        { 0, NULL, NULL, false, false, false, false, 0 },
        { A_ESQ, ic_esq, NULL, false, true, false, false, 0 },
        { A_CENTRO, ic_centro, NULL, false, true, false, false, 0 },
        { A_DIR, ic_dir, NULL, false, true, false, false, 0 },
        { A_JUST, ic_just, NULL, false, true, false, false, 0 },
        { 0, NULL, NULL, false, false, false, false, 0 },
        { A_VINETAS, ic_vinetas, NULL, false, true, false, false, 0 },
        { A_SANG_MENOS, ic_sang_menos, NULL, false, false, false, false, 0 },
        { A_SANG_MAIS, ic_sang_mais, NULL, false, false, false, false, 0 },
    };
    memcpy(it1, u1, sizeof u1);
    memcpy(it2, u2, sizeof u2);
}

/* ------------------------------------------------------------ menús */

static const char *const menu_nomes[7] = { "Archivo", "Edición", "Ver", "Insertar", "Formato", "Herramientas", "Ayuda" };
static const char *const menu_textos[7] = {
    "Nuevo\tCtrl+N\nAbrir...\tCtrl+O\n-\nGuardar\tCtrl+S\nGuardar como...\n-\nExportar a PDF...\tCtrl+P\n-\nPapel A4\nPapel Carta\n-\nSalir",
    "Deshacer\tCtrl+Z\nRehacer\tCtrl+Y\n-\nCortar\tCtrl+X\nCopiar\tCtrl+C\nPegar\tCtrl+V\n-\nSeleccionar todo\tCtrl+A\n-\nBuscar...\tCtrl+F\nReemplazar...\tCtrl+H",
    "Zoom 50%\nZoom 75%\nZoom 100%\nZoom 150%\nZoom 200%\n-\nMostrar u ocultar la regla",
    "Fecha\nSalto de página\tCtrl+Intro",
    "Fuente...\nPárrafo...\nViñetas\n-\nNegrita\tCtrl+B\nCursiva\tCtrl+I\nSubrayado\tCtrl+U\n-\nAlinear a la izquierda\nCentrar\nAlinear a la derecha\nJustificar\n-\nAumentar sangría\nDisminuir sangría",
    "Contar palabras...",
    "Acerca de Pluma",
};
static const int menu_acoes[7][16] = {
    { A_NOVO, A_ABRIR, 0, A_GUARDAR, A_GUARDAR_COMO, 0, A_PDF, 0, A_PAPEL_A4, A_PAPEL_CARTA, 0, A_SAIR },
    { A_DESFAZER, A_REFAZER, 0, A_CORTAR, A_COPIAR, A_COLAR, 0, A_TUDO, 0, A_BUSCAR, A_SUBSTITUIR },
    { A_Z50, A_Z75, A_Z100, A_Z150, A_Z200, 0, A_REGUA },
    { A_DATA, A_PAGINA },
    { A_FONTE, A_PARAGRAFO, A_VINETAS, 0, A_NEGRITA, A_CURSIVA, A_SUBLINHADO, 0, A_ESQ, A_CENTRO, A_DIR, A_JUST, 0, A_SANG_MAIS, A_SANG_MENOS },
    { A_CONTAR },
    { A_ACERCA },
};

#define FILTROS_ABRIR "Documentos (*.rtf;*.txt)|*.rtf;*.txt|Todos los archivos (*.*)|*"
#define FILTROS_GUARDAR "Documento RTF (*.rtf)|*.rtf|Texto (*.txt)|*.txt"
#define FILTROS_PDF "Documento PDF (*.pdf)|*.pdf"

/* ------------------------------------------------------------ archivos */

static bool termina_en(const char *t, const char *suf)
{
    int a = (int)tam_texto(t), b = (int)tam_texto(suf);
    if (a < b) return false;
    for (int i = 0; i < b; i++) {
        char c = t[a - b + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != suf[i]) return false;
    }
    return true;
}

static char *ler_arquivo(const char *c, int *n)
{
    int fd = abrir(c, ABRIR_LER);
    if (fd < 0) return NULL;
    int cap = 65536, tam = 0, r;
    char *b = aloca((size_t)cap);
    while (b && (r = ler(fd, b + tam, (unsigned)(cap - tam - 1))) > 0) {
        tam += r;
        if (tam >= cap - 1) { char *nb = realoca(b, (size_t)cap * 2); if (!nb) break; b = nb; cap *= 2; }
    }
    fechar(fd);
    if (b) b[tam] = 0;
    *n = tam;
    return b;
}

static bool escrever_arquivo(const char *c, const void *d, int n)
{
    int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    int r = escrever_tudo(fd, d, (unsigned)n);
    fechar(fd);
    return r == n;
}

/* Carga de un texto plano: UTF-8 -> Latin-1, sin '\r'. */
static void carregar_texto(rte_t *r, char *b, int n)
{
    n = (int)utf8_para_latin1(b, (size_t)n, NULL);
    int k = 0;
    for (int i = 0; i < n; i++) if (b[i] != '\r') b[k++] = b[i];
    rte_limpar(r);
    rte_fmt_t f = { RTE_SERIF, 0, 12, 0 };
    rte_doc_anexar(r, b, k, &f);
    rte_doc_fim(r);
}

static int abrir_documento(rte_t *r, const char *c)
{
    int n;
    char *b = ler_arquivo(c, &n);
    if (!b) return -1;
    if (n >= 5 && memcmp(b, "{\\rtf", 5) == 0) {
        if (rtf_ler(r, b, n) != 0) { libera(b); return -1; }
    } else carregar_texto(r, b, n);
    libera(b);
    return 0;
}

static int gravar_documento(rte_t *r, const char *c)
{
    if (termina_en(c, ".txt")) {
        int n = rte_tamanho(r);
        char *t = aloca((size_t)n + 1);
        if (!t) return -1;
        memcpy(t, rte_texto(r), (size_t)n);
        bool ok = escrever_arquivo(c, t, n);
        libera(t);
        return ok ? 0 : -1;
    }
    char *d; int n;
    if (rtf_escrever(r, &d, &n) != 0) return -1;
    bool ok = escrever_arquivo(c, d, n);
    libera(d);
    return ok ? 0 : -1;
}

static int exportar_pdf(rte_t *r, const char *c, const char *tit)
{
    uint8_t *d; int n;
    if (rte_exportar_pdf(r, tit, &d, &n) != 0) return -1;
    bool ok = escrever_arquivo(c, d, n);
    libera(d);
    if (ok) registrar("pluma: pdf exportado %s (%d paginas, %d bytes)\n", c, rte_paginas(r), n);
    return ok ? 0 : -1;
}

/* ------------------------------------------------------------ acciones */

static void tudo_redesenhar(void)
{
    g_retangulo(j, 0, 0, j->larg, j->alt, W98_FACE);
    barra_menu_desenhar(j, &menu);
    barra_ferr_desenhar(j, &ferr1);
    combo_desenhar(j, &c_zoom);
    ui_atualizar();
    rte_desenhar(j, doc);
    jan_mostrar(j);
}

static bool guardar_como(void)
{
    char c[200];
    copia_texto(c, caminho[0] ? caminho : "/home/documento", sizeof c);
    if (!dialogo_guardar("Guardar como", FILTROS_GUARDAR, c, c, sizeof c) || !c[0]) return false;
    if (gravar_documento(doc, c) != 0) { mensagem("Pluma", "No se ha podido guardar el documento."); return false; }
    copia_texto(caminho, c, sizeof caminho);
    rte_marcar_limpo(doc);
    registrar("pluma: guardado %s\n", caminho);
    return true;
}

static bool guardar(void)
{
    if (!caminho[0]) return guardar_como();
    if (gravar_documento(doc, caminho) != 0) { mensagem("Pluma", "No se ha podido guardar el documento."); return false; }
    rte_marcar_limpo(doc);
    registrar("pluma: guardado %s\n", caminho);
    return true;
}

static bool pode_descartar(void)
{
    if (!rte_modificado(doc)) return true;
    return confirmar("Pluma", "El documento tiene cambios sin guardar.\n¿Guardarlos antes de continuar?") ? guardar() : true;
}

static void exportar_pdf_dialogo(void)
{
    char c[200];
    if (caminho[0]) {
        copia_texto(c, caminho, sizeof c);
        char *p = acha_ultimo_char(c, '.');
        char *b = acha_ultimo_char(c, '/');
        if (p && (!b || p > b)) *p = 0;
        copia_texto(c + tam_texto(c), ".pdf", sizeof c - tam_texto(c));
    } else copia_texto(c, "/home/documento.pdf", sizeof c);
    if (!dialogo_guardar("Exportar a PDF", FILTROS_PDF, c, c, sizeof c) || !c[0]) return;
    if (exportar_pdf(doc, c, nome_curto()) != 0) mensagem("Pluma", "No se ha podido exportar el PDF.");
    else {
        char m[260];
        formatar_texto(m, sizeof m, "PDF creado: %s\n%d página(s).", c, rte_paginas(doc));
        mensagem("Pluma", m);
    }
}

static void buscar_dialogo(bool subst)
{
    static char b[64], p[64];
    static bool maius;
    for (;;) {
        rte_desenhar(j, doc);
        int r = dialogo_buscar(b, sizeof b, p, sizeof p, &maius, subst);
        if (r == DLG_BUSCAR_NADA) break;
        if (r == DLG_BUSCAR_SEGUINTE) {
            if (!rte_buscar(doc, b, maius, true)) { mensagem("Pluma", "No se encuentra el texto buscado."); break; }
            registrar("pluma: buscar '%s' encontrado\n", b);
        } else if (r == DLG_BUSCAR_SUBSTITUIR) {
            int a, c2;
            if (!rte_selecao(doc, &a, &c2)) rte_buscar(doc, b, maius, true);
            if (rte_substituir(doc, p)) { registrar("pluma: reemplazado '%s' por '%s'\n", b, p); rte_buscar(doc, b, maius, true); }
        } else if (r == DLG_BUSCAR_TUDO) {
            int n = rte_substituir_tudo(doc, b, p, maius);
            registrar("pluma: reemplazados %d\n", n);
            char m[80];
            formatar_texto(m, sizeof m, "Se han hecho %d reemplazos.", n);
            mensagem("Pluma", m);
            break;
        }
    }
    rte_foco(doc, true);
}

static void alinhar(int al)
{
    rte_par_t p = { (uint8_t)al, 0, 0, 0, 0, 100, 0, 0 };
    rte_par_aplicar(doc, RTE_P_ALIN, &p);
}

static void sangria(int delta)
{
    rte_par_t p;
    rte_par_atual(doc, &p);
    int e = p.esq + delta;
    if (e < 0) e = 0;
    p.esq = (int16_t)e;
    rte_par_aplicar(doc, RTE_P_SANG, &p);
}

static void alternar(unsigned bit)
{
    rte_fmt_t f;
    rte_fmt_atual(doc, &f);
    rte_estilo(doc, bit, !(f.est & bit));
}

static void zoom(int z) { rte_zoom(doc, z); }

/* Zoom inicial: la hoja entera de ancho, en múltiplos de 5 %, sin pasar de 100 %. */
static void zoom_ajustar(void)
{
    int pw;
    rte_papel_obter(doc, &pw, NULL, NULL, NULL, NULL, NULL);
    int z = (j->larg - 16 - 24) * 1500 / pw;
    z = z / 5 * 5;
    zoom(z > 100 ? 100 : z < 50 ? 50 : z);
}

static void data_inserir(void)
{
    fuso_data_t d;
    agora_local(&d);
    char t[32];
    int n = formatar_data(t, sizeof t, "%d/%m/%Y", &d);
    if (n > 0) rte_inserir(doc, t, (int)tam_texto(t));
}

static bool sair_pedido;

static void acao(int a)
{
    switch (a) {
    case A_NOVO:
        if (!pode_descartar()) return;
        rte_limpar(doc); caminho[0] = 0;
        registrar("pluma: novo\n");
        break;
    case A_ABRIR: {
        if (!pode_descartar()) return;
        char c[200];
        copia_texto(c, caminho[0] ? caminho : "/home", sizeof c);
        if (!dialogo_abrir("Abrir", FILTROS_ABRIR, c, c, sizeof c) || !c[0]) break;
        if (abrir_documento(doc, c) != 0) { mensagem("Pluma", "No se ha podido abrir el archivo."); break; }
        copia_texto(caminho, c, sizeof caminho);
        registrar("pluma: aberto %s (%d letras, %d paginas)\n", caminho, rte_tamanho(doc), rte_paginas(doc));
        break;
    }
    case A_GUARDAR: guardar(); break;
    case A_GUARDAR_COMO: guardar_como(); break;
    case A_PDF: exportar_pdf_dialogo(); break;
    case A_PAPEL_A4: case A_PAPEL_CARTA: {
        int me, md, ms, mi;
        rte_papel_obter(doc, NULL, NULL, &me, &md, &ms, &mi);
        if (a == A_PAPEL_A4) rte_papel(doc, 11906, 16838, me, md, ms, mi);
        else rte_papel(doc, 12240, 15840, me, md, ms, mi);
        break;
    }
    case A_SAIR: sair_pedido = true; break;
    case A_DESFAZER: rte_desfazer(doc); break;
    case A_REFAZER: rte_refazer(doc); break;
    case A_CORTAR: rte_cortar(j, doc); break;
    case A_COPIAR: rte_copiar(doc); break;
    case A_COLAR: rte_colar(j, doc); break;
    case A_TUDO: rte_selecionar_tudo(doc); break;
    case A_BUSCAR: buscar_dialogo(false); break;
    case A_SUBSTITUIR: buscar_dialogo(true); break;
    case A_Z50: zoom(50); break;
    case A_Z75: zoom(75); break;
    case A_Z100: zoom(100); break;
    case A_Z150: zoom(150); break;
    case A_Z200: zoom(200); break;
    case A_REGUA: regua_visivel = !regua_visivel; layout(); break;
    case A_DATA: data_inserir(); break;
    case A_PAGINA: rte_salto_pagina(doc); break;
    case A_FONTE: {
        rte_fmt_t f;
        rte_fmt_atual(doc, &f);
        if (!dialogo_fonte(&f)) break;
        rte_fmt_aplicar(doc, RTE_C_FAM | RTE_C_TAM | RTE_C_COR, &f);
        rte_estilo(doc, RTE_NEGRITA | RTE_CURSIVA | RTE_SUBLINHADO | RTE_RISCADO, false);
        if (f.est) rte_estilo(doc, f.est, true);
        break;
    }
    case A_PARAGRAFO: {
        rte_par_t p;
        rte_par_atual(doc, &p);
        if (!dialogo_paragrafo(&p)) break;
        rte_par_aplicar(doc, RTE_P_ALIN | RTE_P_SANG | RTE_P_INTER | RTE_P_ESPACO | RTE_P_MARC, &p);
        break;
    }
    case A_VINETAS: {
        rte_par_t p;
        rte_par_atual(doc, &p);
        p.marcador = !p.marcador;
        rte_par_aplicar(doc, RTE_P_MARC, &p);
        break;
    }
    case A_NEGRITA: alternar(RTE_NEGRITA); break;
    case A_CURSIVA: alternar(RTE_CURSIVA); break;
    case A_SUBLINHADO: alternar(RTE_SUBLINHADO); break;
    case A_ESQ: alinhar(RTE_ESQ); break;
    case A_CENTRO: alinhar(RTE_CENTRO); break;
    case A_DIR: alinhar(RTE_DIR); break;
    case A_JUST: alinhar(RTE_JUST); break;
    case A_SANG_MAIS: sangria(720); break;
    case A_SANG_MENOS: sangria(-720); break;
    case A_CONTAR: {
        int w, c, p;
        rte_contar(doc, &w, &c, &p);
        char m[200];
        formatar_texto(m, sizeof m, "Páginas: %d\nPalabras: %d\nCaracteres (sin espacios): %d\nPárrafos: %d", rte_paginas(doc), w, c, p);
        mensagem("Contar palabras", m);
        registrar("pluma: contar palabras=%d caracteres=%d paragrafos=%d paginas=%d\n", w, c, p, rte_paginas(doc));
        break;
    }
    case A_ACERCA:
        mensagem("Acerca de Pluma", "Pluma - procesador de textos de SaguiOS\nLeer y escribir RTF y texto,\nexportar a PDF. Sin macros.");
        break;
    }
    layout();
    tudo_redesenhar();
}

static void tecla_atajo(unsigned t, bool *usada)
{
    *usada = true;
    switch (t) {
    case 14: acao(A_NOVO); break;
    case 15: acao(A_ABRIR); break;
    case 19: acao(A_GUARDAR); break;
    case 16: acao(A_PDF); break;
    case 6: acao(A_BUSCAR); break;
    case 8: acao(A_SUBSTITUIR); break;
    default: *usada = false;
    }
}

/* ------------------------------------------------------------ modos sin ventana */

static int modo_converter(const char *ent, const char *sai, bool carta)
{
    rte_t *r = rte_criar(0, 0, 800, 600);
    if (!r) return 1;
    if (carta) rte_papel(r, 12240, 15840, 1417, 1417, 1417, 1417);
    if (abrir_documento(r, ent) != 0) { imprimir("pluma: no se puede abrir %s\n", ent); return 1; }
    int rc;
    if (termina_en(sai, ".pdf")) rc = exportar_pdf(r, sai, "Documento");
    else rc = gravar_documento(r, sai);
    int w, c, p;
    rte_contar(r, &w, &c, &p);
    imprimir("pluma: convertido %s -> %s (%d paginas, %d palabras, rc=%d)\n", ent, sai, rte_paginas(r), w, rc);
    rte_liberar(r);
    return rc ? 1 : 0;
}

static int modo_info(const char *ent)
{
    rte_t *r = rte_criar(0, 0, 800, 600);
    if (!r || abrir_documento(r, ent) != 0) { imprimir("pluma: no se puede abrir %s\n", ent); return 1; }
    int w, c, p;
    rte_contar(r, &w, &c, &p);
    imprimir("pluma: info paginas=%d palabras=%d caracteres=%d paragrafos=%d\n", rte_paginas(r), w, c, p);
    rte_liberar(r);
    return 0;
}

/* ------------------------------------------------------------ programa */

int principal(int argc, char **argv)
{
    if (argc >= 4 && compara_texto(argv[1], "--convertir") == 0)
        return modo_converter(argv[2], argv[3], argc >= 5 && compara_texto(argv[4], "--carta") == 0);
    if (argc >= 3 && compara_texto(argv[1], "--info") == 0) return modo_info(argv[2]);

    j = jan_abrir("Documento1 - Pluma", 820, 600);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, 560, 360);
    doc = rte_criar(0, 0, 100, 100);
    if (!doc) return 1;
    itens_iniciar();
    for (int i = 0; i < 7; i++) { menu.nomes[i] = menu_nomes[i]; menu.itens[i] = menu_textos[i]; }
    menu.n = 7;
    if (argc >= 2) {
        if (abrir_documento(doc, argv[1]) == 0) {
            copia_texto(caminho, argv[1], sizeof caminho);
            registrar("pluma: aberto %s (%d letras, %d paginas)\n", caminho, rte_tamanho(doc), rte_paginas(doc));
        } else mensagem("Pluma", "No se ha podido abrir el archivo.");
    }
    zoom_ajustar();
    layout();
    tudo_redesenhar();
    registrar("pluma: pronto\n");

    evento_t e;
    bool usada;
    while (!sair_pedido) {
        if (proximo_evento(j->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) { if (pode_descartar()) break; continue; }
        if (jan_redimensionar(j, &e)) { layout(); tudo_redesenhar(); continue; }
        /* menús y listas desplegables */
        if (e.tipo == EV_MENU) {
            int m = barra_menu_tratar(j, &menu, &e);
            if (m >= 0) { int a = menu_acoes[m / 100][m % 100]; if (a) acao(a); continue; }
            int r = combo_tratar(j, &c_fonte, &e);
            if (r == COMBO_SELECIONOU) {
                rte_fmt_t f = { (uint8_t)c_fonte.sel, 0, 12, 0 };
                rte_fmt_aplicar(doc, RTE_C_FAM, &f);
                tudo_redesenhar();
                continue;
            }
            r = combo_tratar(j, &c_tam, &e);
            if (r == COMBO_SELECIONOU) {
                rte_fmt_t f = { 0, 0, (uint8_t)(tams_txt[c_tam.sel][0] ? 0 : 12), 0 };
                int v = 0;
                for (const char *s = tams_txt[c_tam.sel]; *s; s++) v = v * 10 + (*s - '0');
                f.tam = (uint8_t)v;
                rte_fmt_aplicar(doc, RTE_C_TAM, &f);
                tudo_redesenhar();
                continue;
            }
            r = combo_tratar(j, &c_zoom, &e);
            if (r == COMBO_SELECIONOU) { static const int zs[5] = { 50, 75, 100, 150, 200 }; zoom(zs[c_zoom.sel]); tudo_redesenhar(); }
            continue;
        }
        if (e.tipo == EV_CLIQUE && (e.botoes & 1)) {
            int m = barra_menu_tratar(j, &menu, &e);
            (void)m;
        }
        /* combos con foco: reciben el teclado */
        if (e.tipo == EV_TECLA && (c_tam.foco || c_fonte.foco)) {
            int r = combo_tratar(j, c_tam.foco ? &c_tam : &c_fonte, &e);
            if (r == COMBO_ENTROU) {
                int v = 0;
                for (const char *s = c_tam.texto; *s >= '0' && *s <= '9'; s++) v = v * 10 + (*s - '0');
                if (v >= 1 && v <= 127) { rte_fmt_t f = { 0, 0, (uint8_t)v, 0 }; rte_fmt_aplicar(doc, RTE_C_TAM, &f); }
                rte_foco(doc, true);
                tudo_redesenhar();
            }
            continue;
        }
        if (e.tipo == EV_CLIQUE) {
            bool antes_t = c_tam.foco;
            combo_tratar(j, &c_fonte, &e);
            combo_tratar(j, &c_tam, &e);
            combo_tratar(j, &c_zoom, &e);
            if (c_tam.foco && !antes_t) rte_foco(doc, false);
        }
        /* barras */
        int id = barra_ferr_tratar(j, &ferr1, &e);
        if (!id) id = barra_ferr_tratar(j, &ferr2, &e);
        if (id > 0) { acao(id); continue; }
        if (regua_visivel) {
            int rr = regua_tratar(j, &regua, &e);
            if (rr == REGUA_MUDOU) {
                if (regua.marcador == REGUA_M_ESQ || regua.marcador == REGUA_M_DIR) {
                    int pw, ph, me, md, ms, mi;
                    rte_papel_obter(doc, &pw, &ph, &me, &md, &ms, &mi);
                    rte_papel(doc, pw, ph, regua.m_esq, regua.m_dir, ms, mi);
                } else {
                    rte_par_t p = { 0, 0, (int16_t)regua.sang_esq, (int16_t)regua.sang_dir, (int16_t)regua.sang_prim, 100, 0, 0 };
                    rte_par_aplicar(doc, RTE_P_SANG, &p);
                }
                tudo_redesenhar();
                continue;
            }
            if (rr != REGUA_NADA) continue;
        }
        /* atajos de teclado */
        if (e.tipo == EV_TECLA && (e.modificadores & 2) && !(e.modificadores & 4)) {
            tecla_atajo(e.tecla, &usada);
            if (usada) continue;
        }
        int r = rte_tratar(j, doc, &e);
        if (r) ui_atualizar();
    }
    jan_fechar(j);
    registrar("pluma: fechado\n");
    return 0;
}
