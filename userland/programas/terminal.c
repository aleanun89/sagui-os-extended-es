// SPDX-License-Identifier: GPL-3.0-only
/* terminal - el Terminal en ventana (H24), con pestañas y preferencias (D231).
 *
 * Abre una ventana y ejecuta el sagsh dentro, conectado por una terminal
 * virtual (D036):
 *   teclas de la ventana  -> maestro -> esclavo = entrada 0 del sagsh
 *   salidas 1 y 2 del sagsh = esclavo -> maestro -> dibujado en la ventana
 * La terminal virtual hace lo que la terminal del kernel hace: en modo línea,
 * eco, Backspace, Enter, Ctrl+D y Ctrl+C (que termina el programa en
 * primer plano); en modo crudo, las teclas pasan directo (caracteres
 * Latin-1; flechas y afines como ESC + letra), y el sagsh tiene historial, Tab y
 * edición. En la salida, el Terminal entiende las secuencias de la terminal del
 * kernel (ESC [ D / C / K / J) y algunas más (D231): cursor (A B C D G H f d), borrar (J K),
 * colores y negrita (ESC [ n m, los 16 colores ANSI de la tema), guardar y restaurar
 * el cursor (s u) y ocultarlo (ESC [ ? 25 l / h). Las secuencias OSC se descartan.
 * D320: el emulador está en terminal_vt.h (un subconjunto de xterm/VT220 para vim, less, htop, nano y mc por
 * ssh: regiones de desplazamiento, IL/DL, pantalla alternativa, 256 colores aproximados, cajas DEC, DSR/DA); este
 * archivo le pone la rejilla, el historial y el dibujo, y manda las teclas como xterm cuando ssh lo pide.
 *
 * Todo lo que escribe el shell también va a la salida del propio Terminal
 * (que, con el escritorio en marcha, es la serial): es lo que leen los tests.
 *
 * La ventana es redimensionable (D044): la grilla tiene espacio para la pantalla
 * entera y solo aparecen las primeras "columnas x líneas". Al cambiar de tamaño, el
 * texto queda donde estaba; si las líneas ya no entran, suben (como desplazarse).
 *
 * Historial (D048): cada línea que sale por encima de la grilla va a un anillo
 * (500 líneas por defecto, configurable; solo se guarda lo que ocupa cada línea). La rueda del
 * mouse vuelve en el historial (RODA_LINHAS líneas por muesca) y un aviso en la
 * esquina muestra cuántas líneas arriba se está; cualquier tecla o cualquier salida del
 * shell vuelve hacia abajo.
 *
 * Botón derecho (D051): menú contextual con Limpiar pantalla (manda Ctrl+L al shell, que
 * limpia y redibuja la línea que se está escribiendo), Copiar, Pegar, Seleccionar todo, Buscar,
 * Nueva pestaña, Nueva ventana, Preferencias y Cerrar pestaña.
 *
 * Copiar y pegar (D074): arrastrar con el botón izquierdo selecciona un trozo
 * corrido de las líneas que están a la vista (de una letra a otra, como en un
 * editor, no un rectángulo); doble clic selecciona una palabra. La selección desaparece cuando llega
 * salida nueva, cuando se desplaza el historial o con cualquier tecla. Copiar sin selección
 * copia la línea del cursor. Pegar manda el texto a la terminal virtual como si
 * fuera escrito. Ctrl+C sigue siendo la interrupción del shell: los atajos
 * son Ctrl+Mayús+C y Ctrl+Mayús+V.
 *
 * Pestañas (D231): cada pestaña es una sesión con su propio sagsh, su propia terminal virtual,
 * su grilla y su historial. Hasta MAX_SESSOES (8: cada una cuesta un proceso y la tabla del kernel
 * admite 64). La barra de pestañas solo aparece con dos o más (o siempre, si se prefiere), de modo
 * que con una sola el aspecto y las coordenadas son los de siempre. Ctrl+Mayús+T o «+»: nueva;
 * Ctrl+Mayús+W o «x»: cerrar (avisa si hay una orden en marcha); Ctrl+RePág / Ctrl+AvPág,
 * Ctrl+Tab, Ctrl+1..9 y el clic: cambiar. El título es la carpeta del indicador del shell o,
 * si hay una orden en marcha, su nombre. Al cerrar la última, se cierra la ventana.
 *
 * Preferencias (Ctrl+, o el menú): tema de colores y los 16 ANSI, fuente (la interna 8x16 o una
 * vectorial monoespaciada de /apps/fuentes con su tamaño), tamaño inicial, historial, cursor,
 * campana. Se guardan en /home/.config/terminal.conf (texto «clave=valor», terminal_logica.h).
 * Otros atajos: Ctrl+Mayús+F busca en el historial, Ctrl+más / menos / 0 cambian la letra
 * (con una fuente vectorial), Ctrl+clic abre un enlace http(s) en el Cipó. */
#include "sagui.h"
#include "gui.h"
#include "fontes.h"
#include "terminal_logica.h"
#include "terminal_vt.h"

#define COLS_MAX TL_COLS_MAX
#define LINS_MAX TL_LINS_MAX
#define MARGEM 4
#define BARRA_A 26
#define MAX_SESSOES 8
#define COR_BUSCA 0xE5C07B
#define COR_RAIZ  0xD0342C   /* franja y pestaña de un shell de root (D263) */

typedef struct {
    int       shell, mestre;
    vt_t      vt;                /* el emulador (D320): grilla, atributos, cursor, modos y secuencias */
    unsigned char **hist;        /* anillo de líneas: [n lo, n hi, letras, atributos de 2 bytes] */
    int       hmax, hist_ini, hist_n;
    int       volta;             /* líneas arriba del fin (0 = viendo la grilla) */
    bool      sel_ativa, selecionando;   /* selección (D074), en líneas y columnas de la ventana */
    int       sel_al, sel_ac, sel_l, sel_c;
    char      titulo[28], dir[28];
    bool      em_marcha;
    char      cmd[28];
    int       se, sn;            /* filtro de la copia a la serial (D263): estado y secuencia pendiente */
    char      sp[24];
    bool      raiz;              /* el shell avisó (OSC 0 «[root] ...») de que es root (D263) */
} sessao_t;

#define GR(s, l, c) ((s)->vt.g[(l) * COLS_MAX + (c)])
#define AT(s, l, c) ((s)->vt.a[(l) * COLS_MAX + (c)])

static jan_t *j;
static tl_conf_t conf;
static sessao_t *sess[MAX_SESSOES];
static int nsess, atual;
static int COLS = 94, LINS = 28;               /* lo que entra en la ventana ahora */
static int suj_l0 = LINS_MAX, suj_l1 = -1;     /* líneas a redibujar */
static bool barra_suja;

/* La fuente: la interna 8x16 o una vectorial monoespaciada (la celda es la de la 'M'). */
static fonte_t *fnt;
static char fonte_rota[96];
static int fonte_px = 16;
static int celL = LETRA_L, celA = LETRA_A, y0 = MARGEM;

static unsigned t_cursor, t_titulos, t_campana;
static bool cursor_aceso = true, inverso;
static int menu_tab = -1;

/* Búsqueda (Ctrl+Mayús+F) */
static bool busca_ativa, busca_ok;
static char busca[48];
static int busca_idx, busca_col;

/* ------------------------------------------------------------ configuración */

static void conf_ler(void)
{
    static char t[3072];
    tl_conf_padrao(&conf);
    static const char *const arq[2] = { "/home/.config/terminal.conf", "/tmp/terminal.conf" };
    for (int a = 0; a < 2; a++) {
        int fd = abrir(arq[a], ABRIR_LER);
        if (fd < 0) continue;
        int n = ler(fd, t, sizeof t - 1);
        fechar(fd);
        if (n <= 0) continue;
        t[n] = 0;
        tl_conf_ler(&conf, t);
        break;
    }
}

static void conf_gravar(void)
{
    static char b[3072];
    int n = tl_conf_escrever(&conf, b, sizeof b);
    criar_pasta("/home/.config");
    const char *onde = "/home/.config/terminal.conf";
    int fd = abrir(onde, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { onde = "/tmp/terminal.conf"; fd = abrir(onde, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR); }
    if (fd < 0) { registrar("terminal: no he podido guardar las preferencias"); return; }
    escrever_tudo(fd, b, (unsigned)n);
    fechar(fd);
    registrar("terminal: preferencias guardadas en %s (tema %s, historial %d)", onde,
              conf.tema == TL_PERSONALIZADO ? "personalizado" : tl_temas[conf.tema].id, conf.hist);
}

/* ------------------------------------------------------------ fuente */

/* Abre la fuente de fonte_rota con fonte_px y calcula la celda; si no sirve, vuelve a la interna. */
static void fonte_aplicar(void)
{
    if (fnt) { fonte_fechar(fnt); fnt = NULL; }
    celL = LETRA_L; celA = LETRA_A;
    if (!fonte_rota[0]) return;
    if (fonte_px < TL_PX_MIN) fonte_px = TL_PX_MIN;
    if (fonte_px > TL_PX_MAX) fonte_px = TL_PX_MAX;
    fonte_t *f = fonte_abrir_archivo(fonte_rota);
    if (!f) { registrar("terminal: no he podido abrir la fuente %s (uso la interna)", fonte_rota); fonte_rota[0] = 0; return; }
    int l = (fonte_avanco(f, 'M', fonte_px) + 32) >> 6, a = fonte_altura_normal(f, fonte_px);
    if (l < 5 || a < 9) { fonte_fechar(f); fonte_rota[0] = 0; return; }
    fnt = f; celL = l; celA = a;
}

/* ¿El nombre de familia parece de una fuente monoespaciada? */
static bool nome_mono(const char *f)
{
    static const char *const k[] = { "mono", "unispace", "courier", "consol", "typewriter", "fixed" };
    for (unsigned i = 0; i < sizeof k / sizeof *k; i++)
        if (tl_achar(f, (int)tl_len(f), k[i], (int)tl_len(k[i]), 0) >= 0) return true;
    return false;
}

/* Primera fuente monoespaciada del catálogo (para ampliar la letra sin haber elegido ninguna). */
static bool primeira_mono(char *ruta, int max)
{
    static fuente_info_t cat[FUENTES_MAX];
    int n = fuentes_catalogo(cat, FUENTES_MAX);
    for (int i = 0; i < n; i++)
        if (nome_mono(cat[i].familia)) { tl_copiar(ruta, (unsigned)max, cat[i].ruta); return true; }
    return false;
}

/* ------------------------------------------------------------ geometría */

static sessao_t *sa;
/* D263: con un shell de root en la pestaña activa la barra se ve siempre (con la franja ADMINISTRADOR). */
static bool barra_visivel(void) { return nsess > 1 || conf.barra_siempre || (sa && sa->raiz); }

static void calcular_y0(void) { y0 = MARGEM + (barra_visivel() ? BARRA_A : 0); }

static void sujar(int l) { if (l < suj_l0) suj_l0 = l; if (l > suj_l1) suj_l1 = l; }
static void sujar_tudo(void) { for (int l = 0; l < LINS; l++) sujar(l); }

/* ------------------------------------------------------------ historial y sesión */

static void guardar_historico(sessao_t *s, int l)
{
    if (s->hmax <= 0) return;
    const char *ch = &GR(s, l, 0);
    const uint16_t *at = &AT(s, l, 0);
    int n = COLS_MAX;
    while (n > 0 && ch[n - 1] == ' ' && at[n - 1] == 0) n--;
    int i = (s->hist_ini + s->hist_n) % s->hmax;
    if (s->hist_n < s->hmax) s->hist_n++;
    else { libera(s->hist[i]); s->hist[i] = NULL; s->hist_ini = (s->hist_ini + 1) % s->hmax; }
    unsigned char *p = aloca((size_t)(2 + 3 * n));
    if (!p) { s->hist[i] = NULL; return; }
    p[0] = (unsigned char)(n & 255); p[1] = (unsigned char)(n >> 8);
    memcpy(p + 2, ch, (size_t)n);
    for (int k = 0; k < n; k++) { p[2 + n + 2 * k] = (unsigned char)(at[k] & 255); p[3 + n + 2 * k] = (unsigned char)(at[k] >> 8); }
    s->hist[i] = p;
}

static char vb_ch[COLS_MAX + 1];
static uint16_t vb_at[COLS_MAX];

/* Línea l de la ventana: con volta > 0, las primeras vienen del historial. */
static void linha_vis(sessao_t *s, int l, const char **ch, const uint16_t **at)
{
    int v = s->hist_n - s->volta + l;
    if (v >= s->hist_n) { *ch = &GR(s, v - s->hist_n, 0); *at = &AT(s, v - s->hist_n, 0); return; }
    unsigned char *p = s->hist[(s->hist_ini + v) % s->hmax];
    int n = p ? p[0] | p[1] << 8 : 0;
    memset(vb_ch, ' ', sizeof vb_ch);
    memset(vb_at, 0, sizeof vb_at);
    if (p) {
        memcpy(vb_ch, p + 2, (size_t)n);
        for (int k = 0; k < n; k++) vb_at[k] = (uint16_t)(p[2 + n + 2 * k] | p[3 + n + 2 * k] << 8);
    }
    *ch = vb_ch; *at = vb_at;
}

/* Línea del «documento» (historial + grilla) para buscar: sin relleno. */
static void linha_doc(sessao_t *s, int idx, const char **ch, int *n)
{
    if (idx >= s->hist_n) { *ch = &GR(s, idx - s->hist_n, 0); *n = COLS; return; }
    unsigned char *p = s->hist[(s->hist_ini + idx) % s->hmax];
    *n = p ? p[0] | p[1] << 8 : 0;
    *ch = p ? (const char *)p + 2 : "";
}

static void sessao_limpar(sessao_t *s)
{
    memset(s->vt.g, ' ', (size_t)COLS_MAX * LINS_MAX);
    memset(s->vt.a, 0, (size_t)COLS_MAX * LINS_MAX * 2);
    s->vt.col = s->vt.lin = 0;
    s->vt.pend = false;
    if (s == sa) sujar_tudo();
}

static void sem_selecao(sessao_t *s)
{
    if (!s->sel_ativa) { s->selecionando = false; return; }
    s->sel_ativa = s->selecionando = false;
    if (s != sa) return;
    int l0 = s->sel_al < s->sel_l ? s->sel_al : s->sel_l, l1 = s->sel_al < s->sel_l ? s->sel_l : s->sel_al;
    for (int l = l0; l <= l1 && l < LINS; l++) sujar(l);
}

static void voltar_para_baixo(sessao_t *s)
{
    if (!s->volta) return;
    s->volta = 0;
    if (s == sa) sujar_tudo();
}

static bool selecionada(sessao_t *s, int l, int c)
{
    if (!s->sel_ativa) return false;
    int a = s->sel_al * COLS_MAX + s->sel_ac, b = s->sel_l * COLS_MAX + s->sel_c, x = l * COLS_MAX + c;
    if (a > b) { int t = a; a = b; b = t; }
    return x >= a && x <= b;
}

static void rodar(int entalhes)
{
    int antes = sa->volta;
    sa->volta -= entalhes * RODA_LINHAS;
    if (sa->volta > sa->hist_n) sa->volta = sa->hist_n;
    if (sa->volta < 0) sa->volta = 0;
    if (sa->volta == antes) return;
    sem_selecao(sa);
    sujar_tudo();
    registrar("terminal: historial %d lineas arriba (de %d)", sa->volta, sa->hist_n);
}

/* ------------------------------------------------------------ dibujo */

static void cel_texto(int x, int y, const char *t, int n, uint32_t cor)
{
    if (!fnt) { g_texto_n(j, x, y, t, n, cor); return; }
    for (int i = 0; i < n; i++)
        if (t[i] != ' ') fonte_texto_buf_n(fnt, j->px, j->larg, j->alt, x + i * celL, y, &t[i], 1, fonte_px, cor);
}

/* Colores de la celda (l, c) de la ventana. */
static void cores(int l, int c, uint16_t at, uint32_t *fg, uint32_t *bg)
{
    tl_cores_da_celula(&conf, at, fg, bg);
    if (busca_ok && sa->hist_n - sa->volta + l == busca_idx && c >= busca_col && c < busca_col + (int)tl_len(busca)) {
        *fg = 0x1E2328; *bg = COR_BUSCA;
    } else if (selecionada(sa, l, c)) { *fg = conf.fundo; *bg = conf.selecao; }
    else if (!sa->volta && l == sa->vt.lin && c == sa->vt.col && conf.cursor == TL_CUR_BLOCO && !sa->vt.cur_oculto &&
             (!conf.parpadeo || cursor_aceso)) { *fg = conf.fundo; *bg = conf.cursor_cor; }
    if (inverso) { uint32_t t = *fg; *fg = *bg; *bg = t; }
}

/* D320: el dibujo de línea del juego ESC ( 0. Las cajas se trazan con rectángulos (así se ven en cualquier
 * fuente, también en la interna 8x16); lo demás se aproxima con una letra Latin-1. Devuelve la letra a
 * escribir (' ' si lo traza dibujar_dec) y en *brazos los lados de la línea: 1 izq, 2 der, 4 arriba, 8 abajo,
 * 16 relleno completo, 32 trama. */
static char dec_letra(char c, int *brazos)
{
    *brazos = 0;
    switch (c) {
    case 'j': *brazos = 1 | 4; return ' ';
    case 'k': *brazos = 1 | 8; return ' ';
    case 'l': *brazos = 2 | 8; return ' ';
    case 'm': *brazos = 2 | 4; return ' ';
    case 'n': *brazos = 15; return ' ';
    case 'q': *brazos = 3; return ' ';
    case 't': *brazos = 2 | 4 | 8; return ' ';
    case 'u': *brazos = 1 | 4 | 8; return ' ';
    case 'v': *brazos = 3 | 4; return ' ';
    case 'w': *brazos = 3 | 8; return ' ';
    case 'x': *brazos = 12; return ' ';
    case '0': *brazos = 16; return ' ';
    case 'a': *brazos = 32; return ' ';
    case '`': return '*';
    case 'f': return (char)0xB0;
    case 'g': return (char)0xB1;
    case '~': return (char)0xB7;
    case '}': return (char)0xA3;
    case '{': return 'p';
    case '|': return '!';
    case 'y': return '<';
    case 'z': return '>';
    case 'o': case 'p': case 'r': case 's': return '-';
    default: return ' ';
    }
}

static void dibujar_dec(int x, int y, int brazos, uint32_t fg, uint32_t bg)
{
    int cx = celL / 2, cy = celA / 2;
    if (brazos & 16) { g_retangulo(j, x, y, celL, celA, fg); return; }
    if (brazos & 32) { g_retangulo(j, x, y, celL, celA, tl_misturar(bg, fg, 40)); return; }
    if (brazos & 1) g_retangulo(j, x, y + cy, cx + 1, 1, fg);
    if (brazos & 2) g_retangulo(j, x + cx, y + cy, celL - cx, 1, fg);
    if (brazos & 4) g_retangulo(j, x + cx, y, 1, cy + 1, fg);
    if (brazos & 8) g_retangulo(j, x + cx, y + cy, 1, celA - cy, fg);
}

static void desenhar_linha(int l)
{
    const char *ch;
    const uint16_t *at;
    linha_vis(sa, l, &ch, &at);
    const char *orig = ch;
    char lc[COLS_MAX];   /* las letras, con las del dibujo DEC ya traducidas */
    bool especial = false;
    for (int c = 0; c < COLS; c++) {
        lc[c] = ch[c];
        if (at[c] & (TL_AT_DEC | TL_AT_SUBRAYA)) especial = true;
        if (at[c] & TL_AT_DEC) { int b; lc[c] = dec_letra(ch[c], &b); }
    }
    ch = lc;
    int y = y0 + l * celA;
    uint32_t fundo = inverso ? conf.letra : conf.fundo;
    g_retangulo(j, 0, y, j->larg, celA, fundo);
    for (int c = 0; c < COLS;) {
        uint32_t fg, bg, fg2, bg2;
        cores(l, c, at[c], &fg, &bg);
        int e = c + 1;
        while (e < COLS) {
            cores(l, e, at[e], &fg2, &bg2);
            if (fg2 != fg || bg2 != bg) break;
            e++;
        }
        if (bg != fundo) g_retangulo(j, MARGEM + c * celL, y, (e - c) * celL, celA, bg);
        cel_texto(MARGEM + c * celL, y, ch + c, e - c, fg);
        c = e;
    }
    if (especial) {
        for (int c = 0; c < COLS; c++) {
            if (!(at[c] & (TL_AT_DEC | TL_AT_SUBRAYA))) continue;
            uint32_t fg, bg;
            cores(l, c, at[c], &fg, &bg);
            if (at[c] & TL_AT_DEC) { int b; dec_letra(orig[c], &b); if (b) dibujar_dec(MARGEM + c * celL, y, b, fg, bg); }
            if (at[c] & TL_AT_SUBRAYA) g_retangulo(j, MARGEM + c * celL, y + celA - 1, celL, 1, fg);
        }
    }
    if (!sa->volta && l == sa->vt.lin && sa->vt.col < COLS && !sa->vt.cur_oculto && (!conf.parpadeo || cursor_aceso)) {
        if (conf.cursor == TL_CUR_SUBLINHADO) g_retangulo(j, MARGEM + sa->vt.col * celL, y + celA - 2, celL, 2, conf.cursor_cor);
        else if (conf.cursor == TL_CUR_BARRA) g_retangulo(j, MARGEM + sa->vt.col * celL, y, 2, celA, conf.cursor_cor);
    }
    if (sa->volta && l == 0) {   /* aviso en la esquina: cuánto se volvió */
        char t[40];
        formatar_texto(t, sizeof t, " Historial: -%d ", sa->volta);
        int w = g_largura_texto(t), x = j->larg - MARGEM - w;
        g_retangulo(j, x, y, w, LETRA_A, COR_DESTAQUE);
        g_texto(j, x, y, t, COR_BRANCO);
    }
}

/* Barra de pestañas: posición y ancho de la pestaña i. */
static void geom_tab(int i, int *x, int *w)
{
    int tw = (j->larg - 4 - 30) / nsess;
    if (tw > 170) tw = 170;
    if (tw < 36) tw = 36;
    *x = 2 + i * tw;
    *w = tw - 2;
}

static void desenhar_barra(void)
{
    barra_suja = false;
    if (!barra_visivel()) return;
    uint32_t fundo_b = tl_misturar(conf.fundo, 0x808080, 22), letra_f = tl_misturar(conf.letra, conf.fundo, 45);
    bool raiz_a = sa && sa->raiz;
    if (raiz_a) fundo_b = tl_misturar(fundo_b, COR_RAIZ, 45);   /* barra teñida de rojo mientras el shell es root */
    g_retangulo(j, 0, 0, j->larg, BARRA_A, fundo_b);
    for (int i = 0; i < nsess; i++) {
        int x, w;
        geom_tab(i, &x, &w);
        bool ativa = i == atual;
        g_retangulo(j, x, ativa ? 3 : 5, w, BARRA_A - (ativa ? 3 : 5), ativa ? conf.fundo : tl_misturar(fundo_b, conf.fundo, 30));
        if (ativa) g_retangulo(j, x, 3, w, 2, sess[i]->raiz ? COR_RAIZ : COR_DESTAQUE);
        bool com_x = w >= 70 || ativa;
        int cab = (w - 12 - (com_x ? 18 : 0)) / LETRA_L;
        char tr[40];
        const char *t = sess[i]->titulo;
        if (sess[i]->raiz) { formatar_texto(tr, sizeof tr, "[root] %s", sess[i]->titulo); t = tr; }
        int n = (int)tl_len(t);
        if (cab < 1) cab = 1;
        if (n > cab) {   /* se corta con una marca de continuación */
            char c[40];
            int k = cab > 36 ? 36 : cab;
            memcpy(c, t, (size_t)k);
            if (k > 1) c[k - 1] = '~';
            g_texto_n(j, x + 8, 6, c, k, ativa ? conf.letra : letra_f);
        } else g_texto_n(j, x + 8, 6, t, n, ativa ? conf.letra : letra_f);
        if (com_x) {
            int cx = x + w - 14, cy = 9;
            g_linha(j, cx, cy, cx + 7, cy + 7, ativa ? conf.letra : letra_f);
            g_linha(j, cx + 7, cy, cx, cy + 7, ativa ? conf.letra : letra_f);
        }
    }
    int px, pw;
    geom_tab(nsess - 1, &px, &pw);
    px += pw + 6;
    if (px + 22 > j->larg) px = j->larg - 24;
    g_retangulo(j, px, 6, 20, 16, tl_misturar(fundo_b, conf.fundo, 30));
    g_linha(j, px + 5, 14, px + 14, 14, conf.letra);
    g_linha(j, px + 9, 10, px + 9, 18, conf.letra);
    if (raiz_a) {   /* franja roja con ADMINISTRADOR, a la derecha */
        int lw = 13 * LETRA_L + 16, lx = j->larg - lw - 4;
        if (lx > px + 26) {
            g_retangulo(j, lx, 3, lw, BARRA_A - 6, COR_RAIZ);
            g_texto(j, lx + 8, 6, "ADMINISTRADOR", 0xFFFFFF);
        }
        g_retangulo(j, 0, BARRA_A - 2, j->larg, 2, COR_RAIZ);
    }
    jan_mostrar_area(j, 0, 0, j->larg, BARRA_A);
}

/* Qué hay bajo (x, y): pestaña i (0..n-1), cerrar i (100 + i), nueva (200) o nada (-1). */
static int barra_ponto(int x, int y)
{
    if (!barra_visivel() || y >= BARRA_A) return -1;
    for (int i = 0; i < nsess; i++) {
        int tx, tw;
        geom_tab(i, &tx, &tw);
        if (x < tx || x >= tx + tw) continue;
        bool com_x = tw >= 70 || i == atual;
        if (com_x && x >= tx + tw - 20) return 100 + i;
        return i;
    }
    int px, pw;
    geom_tab(nsess - 1, &px, &pw);
    px += pw + 6;
    if (px + 22 > j->larg) px = j->larg - 24;
    if (x >= px && x < px + 22) return 200;
    return -1;
}

static void desenhar_busca(void)
{
    int a = LETRA_A + 10, y = j->alt - a;
    uint32_t f = tl_misturar(conf.fundo, 0x808080, 25);
    g_retangulo(j, 0, y, j->larg, a, f);
    g_retangulo(j, 0, y, j->larg, 1, COR_DESTAQUE);
    char t[100];
    formatar_texto(t, sizeof t, "Buscar: %s_", busca);
    g_texto(j, MARGEM + 4, y + 5, t, conf.letra);
    const char *dica = !busca[0] ? "Intro: anterior  Mayús+Intro: siguiente  Esc: cerrar"
                       : busca_ok ? "" : "no encontrado";
    int w = g_largura_texto(dica);
    if (w) g_texto(j, j->larg - w - MARGEM - 4, y + 5, dica, busca_ok || !busca[0] ? tl_misturar(conf.letra, conf.fundo, 50) : 0xE06C6C);
    jan_mostrar_area(j, 0, y, j->larg, a);
}

static void mostrar(void)
{
    if (suj_l1 >= 0) {
        for (int l = suj_l0; l <= suj_l1; l++) desenhar_linha(l);
        jan_mostrar_area(j, 0, y0 + suj_l0 * celA, j->larg, (suj_l1 - suj_l0 + 1) * celA);
        suj_l0 = LINS_MAX; suj_l1 = -1;
        if (busca_ativa) desenhar_busca();
    }
    if (barra_suja) desenhar_barra();
}

/* ------------------------------------------------------------ salida del shell */

static void campana(void)
{
    if (conf.campana == TL_CAMP_SOM) bip(880, 70);
    else if (conf.campana == TL_CAMP_VISUAL) { inverso = true; t_campana = tempo_ms() + 110; sujar_tudo(); }
}

static int limitar(int v, int a, int b) { return v < a ? a : v > b ? b : v; }

static void tamanho_minimo(void);
static void novo_tamanho(bool avisar);

/* Tras cambiar si la barra se ve (root en la sesión activa, otra pestaña): recalcula la geometría. */
static void reavaliar_barra(bool tinha_barra)
{
    barra_suja = true;
    if (tinha_barra == barra_visivel()) return;
    calcular_y0();
    tamanho_minimo();
    novo_tamanho(false);
}

/* OSC 0 / 2 (título): «[root] ...» marca la sesión como de administrador (D263). Lo manda sagsh en
 * cada prompt, así que al salir de «su» la marca se quita sola. Sirve igual por SSH. */
static void osc(sessao_t *s)
{
    const char *t = s->vt.esc.osc;
    if (!((t[0] == '0' || t[0] == '2') && t[1] == ';')) return;
    bool raiz = tl_comeca(t + 2, "[root]");
    if (raiz == s->raiz) return;
    bool tinha = barra_visivel();
    s->raiz = raiz;
    registrar("terminal: pestana con shell %s", raiz ? "de root (ADMINISTRADOR)" : "normal");
    reavaliar_barra(tinha);
}

/* La copia a la serial (lo que leen las pruebas y SSH de depuración) no lleva colores ni títulos
 * (D263): se quitan ESC [ ... m y las OSC; el resto de secuencias (cursor, borrado) pasa como siempre. */
static void copiar_serie(sessao_t *s, const char *b, int n)
{
    char o[512 + 32];
    int m = 0;
    for (int k = 0; k < n; k++) {
        unsigned char c = (unsigned char)b[k];
        if (s->se == 0) {
            if (c == 27) { s->se = 1; s->sn = 0; s->sp[s->sn++] = (char)c; }
            else o[m++] = (char)c;
        } else if (s->se == 1) {   /* tras ESC */
            if (c == '[') { s->se = 2; s->sp[s->sn++] = (char)c; }
            else if (c == ']') s->se = 3;
            else { s->se = 0; memcpy(o + m, s->sp, (size_t)s->sn); m += s->sn; o[m++] = (char)c; }
        } else if (s->se == 2) {   /* CSI: parámetros hasta el final */
            if (c >= 0x40 && c <= 0x7E) {
                if (c != 'm') { memcpy(o + m, s->sp, (size_t)s->sn); m += s->sn; o[m++] = (char)c; }
                s->se = 0;
            } else if (s->sn < (int)sizeof s->sp - 1) s->sp[s->sn++] = (char)c;
            else { s->se = 0; memcpy(o + m, s->sp, (size_t)s->sn); m += s->sn; o[m++] = (char)c; }
        } else {                   /* OSC: se descarta hasta BEL, o ESC \ */
            if (c == 7) s->se = 0;
            else if (c == 27) s->se = 4;
        }
        if (s->se == 4 && c == '\\') s->se = 0;
        else if (s->se == 4 && c != 27) s->se = 3;
    }
    if (m > 0) escrever(SAIDA, o, (unsigned)m);
}

/* Callbacks del emulador (terminal_vt.h): u es la sesión. */
static void cb_hist(void *u, int l) { guardar_historico(u, l); }
static void cb_suja(void *u, int l0, int l1) { if (u == sa) for (int l = l0; l <= l1 && l < LINS; l++) sujar(l); }
static void cb_resp(void *u, const char *b, int n) { sessao_t *s = u; if (s->mestre >= 0) escrever_tudo(s->mestre, b, (unsigned)n); }
static void cb_sino(void *u) { (void)u; campana(); }
static void cb_osc(void *u) { osc(u); }
static void cb_limpar_hist(void *u)
{
    sessao_t *s = u;
    for (int i = 0; i < s->hist_n; i++) { int k = (s->hist_ini + i) % s->hmax; if (s->hist[k]) { libera(s->hist[k]); s->hist[k] = NULL; } }
    s->hist_n = 0; s->hist_ini = 0; s->volta = 0;
    if (s == sa) sujar_tudo();
}

static void receber(sessao_t *s, unsigned char c)
{
    if (s == sa) {
        sem_selecao(s);
        if (!busca_ativa) voltar_para_baixo(s);
        else busca_ok = false;
    } else s->sel_ativa = s->selecionando = false;
    vt_byte(&s->vt, c);
}

/* ------------------------------------------------------------ tamaño de la ventana */

/* La ventana (o la celda, o la barra) cambió: recalcula columnas y líneas de todas las sesiones. */
static void novo_tamanho(bool avisar)
{
    calcular_y0();
    int nc = (j->larg - 2 * MARGEM) / celL, nl = (j->alt - y0 - MARGEM) / celA;
    nc = limitar(nc, 1, COLS_MAX);
    nl = limitar(nl, 1, LINS_MAX);
    for (int i = 0; i < nsess; i++) {
        sessao_t *s = sess[i];
        vt_redimensionar(&s->vt, nc, nl);
        s->sel_ativa = s->selecionando = false;
        if (s->volta > s->hist_n) s->volta = s->hist_n;
    }
    COLS = nc; LINS = nl;
    g_retangulo(j, 0, 0, j->larg, j->alt, conf.fundo);
    sujar_tudo();
    mostrar();
    barra_suja = true;
    desenhar_barra();
    if (busca_ativa) desenhar_busca();
    jan_mostrar(j);
    if (avisar) registrar("terminal: janela %dx%d, %d colunas x %d linhas", j->larg, j->alt, COLS, LINS);
}

static void tamanho_minimo(void)
{
    jan_estilo(j, JAN_REDIMENSIONAVEL, 30 * celL + 2 * MARGEM, 6 * celA + 2 * MARGEM + (barra_visivel() ? BARRA_A : 0));
}

/* ------------------------------------------------------------ sesiones (pestañas) */

static sessao_t *sessao_nova(void)
{
    if (nsess >= MAX_SESSOES) return NULL;
    sessao_t *s = aloca_zerado(sizeof *s);
    if (!s) return NULL;
    s->vt.g = aloca((size_t)COLS_MAX * LINS_MAX);
    s->vt.a = aloca_zerado((size_t)COLS_MAX * LINS_MAX * 2);
    s->vt.cols = COLS; s->vt.lins = LINS;
    s->vt.u = s; s->vt.hist = cb_hist; s->vt.suja = cb_suja; s->vt.resp = cb_resp;
    s->vt.sino = cb_sino; s->vt.osc = cb_osc; s->vt.limpar_hist = cb_limpar_hist;
    vt_iniciar(&s->vt);
    s->hmax = conf.hist;
    if (s->hmax > 0) s->hist = aloca_zerado((size_t)s->hmax * sizeof *s->hist);
    if (!s->vt.g || !s->vt.a || (s->hmax > 0 && !s->hist)) goto falta;
    memset(s->vt.g, ' ', (size_t)COLS_MAX * LINS_MAX);
    s->shell = -1;
    int tv[2];
    if (abrir_tv(tv) < 0) goto falta;
    /* El shell nace con el esclavo como 0, 1 y 2. */
    int g0 = duplicar(ENTRADA, 13), g1 = duplicar(SAIDA, 14), g2 = duplicar(ERROS, 15);
    duplicar(tv[1], ENTRADA);
    duplicar(tv[1], SAIDA);
    duplicar(tv[1], ERROS);
    char *args[] = { "sagsh", NULL };
    /* D263: el shell sabe que habla con un terminal con colores y títulos (ANSI SGR + OSC). */
    static char *envp[96];
    int ne = 0;
    for (int e = 0; environ && environ[e] && ne < 90; e++)
        if (compara_texto_n(environ[e], "TERM=", 5) != 0) envp[ne++] = environ[e];
    envp[ne++] = "TERM=xterm-256color";   /* D320: 256 colores (aproximados a la paleta de 16), bce, regiones, pantalla alternativa */
    envp[ne] = NULL;
    int shell = executar_env("/bin/sagsh", args, envp);
    duplicar(g0, ENTRADA); duplicar(g1, SAIDA); duplicar(g2, ERROS);
    fechar(g0); fechar(g1); fechar(g2);
    fechar(tv[1]);
    if (shell < 0) {
        fechar(tv[0]);
        imprimir("terminal: no he podido abrir sagsh (%s)\n", nome_erro(shell));
        goto falta;
    }
    s->shell = shell;
    s->mestre = tv[0];
    tl_copiar(s->titulo, sizeof s->titulo, "sagsh");
    sess[nsess++] = s;
    return s;
falta:
    if (s->hist) libera(s->hist);
    libera(s->vt.g); libera(s->vt.a); libera(s);
    return NULL;
}

static void sessao_liberar(sessao_t *s)
{
    for (int i = 0; i < s->hmax && s->hist; i++) if (s->hist[i]) libera(s->hist[i]);
    if (s->hist) libera(s->hist);
    libera(s->vt.g); libera(s->vt.a);
    if (s->vt.g2) libera(s->vt.g2);
    if (s->vt.a2) libera(s->vt.a2);
    fechar(s->mestre);
    libera(s);
}

static void trocar(int i)
{
    if (i < 0 || i >= nsess) return;
    if (i == atual && sa) return;
    if (sa) { sem_selecao(sa); }
    bool tinha_b = barra_visivel();
    atual = i; sa = sess[i];
    if (tinha_b != barra_visivel()) { calcular_y0(); tamanho_minimo(); novo_tamanho(false); }
    busca_ativa = busca_ok = false;
    sujar_tudo();
    barra_suja = true;
    registrar("terminal: pestana %d de %d (shell %d)", atual + 1, nsess, sa->shell);
}

static void sessao_tirar(int i)
{
    sessao_t *s = sess[i];
    for (int k = i; k < nsess - 1; k++) sess[k] = sess[k + 1];
    nsess--;
    sess[nsess] = NULL;
    sessao_liberar(s);
    if (nsess == 0) { sa = NULL; return; }
    if (atual > i || atual >= nsess) atual--;
    if (atual < 0) atual = 0;
    sa = sess[atual];
    busca_ativa = busca_ok = false;
    calcular_y0();
    tamanho_minimo();
    novo_tamanho(false);
    registrar("terminal: pestana cerrada, quedan %d", nsess);
}

/* Qué orden hay en marcha en cada sesión (hijos del shell sin ventana) y el título de cada pestaña. */
static void atualizar_titulos(void)
{
    static info_processo_t v[64];
    int n = info_sistema(INFO_PROCESSOS, v, 64);
    if (n < 0) n = 0;
    for (int i = 0; i < nsess; i++) {
        sessao_t *s = sess[i];
        char antes[28], novo[28];
        tl_copiar(antes, sizeof antes, s->titulo);
        bool marcha = false;
        for (int k = 0; k < n; k++)
            if (v[k].pai == s->shell && v[k].estado != 4 && v[k].janelas == 0) {
                marcha = true;
                tl_copiar(s->cmd, sizeof s->cmd, v[k].nome);
            }
        s->em_marcha = marcha;
        if (marcha) tl_copiar(novo, sizeof novo, s->cmd);
        else {
            const char *ch;
            int nn;
            linha_doc(s, s->hist_n + s->vt.lin, &ch, &nn);
            char d[28];
            if (tl_titulo_do_prompt(ch, nn, d, sizeof d)) tl_copiar(s->dir, sizeof s->dir, d);
            tl_copiar(novo, sizeof novo, s->dir[0] ? s->dir : "sagsh");
        }
        if (compara_texto(antes, novo)) {
            tl_copiar(s->titulo, sizeof s->titulo, novo);
            barra_suja = true;
            registrar("terminal: titulo de la pestana %d: %s", i + 1, novo);
        }
    }
}

static void nova_pestana(void)
{
    if (nsess >= MAX_SESSOES) {
        registrar("terminal: limite de %d pestanas", MAX_SESSOES);
        return;
    }
    bool tinha_barra = barra_visivel();
    sessao_t *s = sessao_nova();
    if (!s) return;
    if (!sa) sa = s;
    if (tinha_barra != barra_visivel()) { calcular_y0(); tamanho_minimo(); novo_tamanho(false); }
    registrar("terminal: pestana %d abierta (shell %d)", nsess, s->shell);
    trocar(nsess - 1);
    atualizar_titulos();
    barra_suja = true;
}

/* Cierra la pestaña i; con "perguntar", avisa si hay una orden en marcha. */
static void fechar_pestana(int i, bool perguntar)
{
    if (i < 0 || i >= nsess) return;
    if (perguntar) {
        atualizar_titulos();
        if (sess[i]->em_marcha) {
            char t[120];
            formatar_texto(t, sizeof t, "Hay una orden en marcha (%s).\n¿Cerrar la pestaña y terminarla?", sess[i]->cmd);
            if (!confirmar("Cerrar pestaña", t)) return;
        }
    }
    int pid = sess[i]->shell, c;
    if (pid > 0) { matar(pid); esperar(pid, &c); }
    sess[i]->shell = -1;
    sessao_tirar(i);
}

/* Una sesión cuyo shell terminó solo (sair, Ctrl+D). */
static void shell_terminou(int i)
{
    int pid = sess[i]->shell, c;
    sess[i]->shell = -1;
    if (pid > 0) esperar(pid, &c);
    sessao_tirar(i);
}

/* ------------------------------------------------------------ copiar, pegar, buscar */

/* Copia la selección (o, sin ella, la línea del cursor), sin los espacios del fin
 * de cada línea. */
static void copiar(void)
{
    static char t[LINS_MAX * (COLS_MAX + 1)];
    int n = 0, l0, c0, l1, c1;
    if (sa->sel_ativa) {
        int a = sa->sel_al * COLS_MAX + sa->sel_ac, b = sa->sel_l * COLS_MAX + sa->sel_c;
        if (a > b) { int x = a; a = b; b = x; }
        l0 = a / COLS_MAX; c0 = a % COLS_MAX; l1 = b / COLS_MAX; c1 = b % COLS_MAX;
    } else { l0 = l1 = sa->volta ? LINS - 1 : sa->vt.lin; c0 = 0; c1 = COLS - 1; }
    for (int l = l0; l <= l1 && l < LINS; l++) {
        const char *s;
        const uint16_t *at;
        linha_vis(sa, l, &s, &at);
        int de = l == l0 ? c0 : 0, ate = l == l1 ? c1 : COLS - 1;
        if (ate >= COLS) ate = COLS - 1;
        int fim = ate;
        while (fim >= de && s[fim] == ' ') fim--;
        for (int c = de; c <= fim; c++) t[n++] = s[c];
        if (l < l1) t[n++] = '\n';
    }
    copiar_texto(t, (unsigned)n);
    registrar("terminal: copiado %d bytes%s", n, sa->sel_ativa ? "" : " (linea actual)");
}

/* Pegar: el texto va al shell como si fuera escrito. */
static void colar(void)
{
    int n = colar_texto(NULL, 0);
    if (n <= 0) return;
    char *b = aloca((size_t)n + 1);
    if (!b) return;
    n = colar_texto(b, (unsigned)n + 1);
    int k = 0;
    for (int i = 0; i < n; i++) if (b[i] != '\r') b[k++] = b[i];
    sem_selecao(sa);
    voltar_para_baixo(sa);
    if (sa->vt.colar_cor) escrever_tudo(sa->mestre, "\033[200~", 6);   /* D320: pegado entre corchetes (?2004) */
    escrever_tudo(sa->mestre, b, (unsigned)k);
    if (sa->vt.colar_cor) escrever_tudo(sa->mestre, "\033[201~", 6);
    libera(b);
    registrar("terminal: pegado %d bytes", k);
}

static void selecionar_tudo(void)
{
    sem_selecao(sa);
    sa->sel_al = 0; sa->sel_ac = 0; sa->sel_l = LINS - 1; sa->sel_c = COLS - 1;
    sa->sel_ativa = true;
    sujar_tudo();
    registrar("terminal: seleccionado todo");
}

/* Línea y columna de la grilla debajo del mouse. */
static void celula(int x, int y, int *l, int *c)
{
    *l = (y - y0) / celA; *c = (x - MARGEM) / celL;
    if (y < y0) *l = 0;
    if (x < MARGEM) *c = 0;
    *l = limitar(*l, 0, LINS - 1);
    *c = limitar(*c, 0, COLS - 1);
}

static bool letra_palavra(char c) { return c != ' ' && c != '\t' && c != 0; }

static void selecionar_palavra(int l, int c)
{
    const char *s;
    const uint16_t *at;
    linha_vis(sa, l, &s, &at);
    if (!letra_palavra(s[c])) return;
    int a = c, b = c;
    while (a > 0 && letra_palavra(s[a - 1])) a--;
    while (b < COLS - 1 && letra_palavra(s[b + 1])) b++;
    sem_selecao(sa);
    sa->sel_al = l; sa->sel_ac = a; sa->sel_l = l; sa->sel_c = b;
    sa->sel_ativa = true;
    sujar(l);
}

/* Ctrl+clic: abre un enlace http(s) en el Cipó. */
static bool abrir_enlace(int l, int c)
{
    const char *s;
    const uint16_t *at;
    char url[200];
    linha_vis(sa, l, &s, &at);
    if (!tl_url_en(s, COLS, c, url, sizeof url)) return false;
    char *a[] = { "cipo", url, NULL };
    int r = executar("/apps/bin/cipo", a);   /* D324: el Cipó vive en /apps */
    registrar("terminal: enlace %s (cipo %d)", url, r);
    return true;
}

/* --- búsqueda --- */

static void ir_para(int idx)
{
    if (idx >= sa->hist_n) sa->volta = 0;
    else {
        int v0 = limitar(idx - LINS / 2, 0, sa->hist_n);
        sa->volta = sa->hist_n - v0;
    }
    sem_selecao(sa);
    sujar_tudo();
}

/* Busca la siguiente aparición hacia atrás (dir < 0) o adelante; "mismo" = vale la del sitio actual. */
static bool buscar(int dir, bool mismo)
{
    int ql = (int)tl_len(busca), total = sa->hist_n + LINS;
    if (ql == 0) { busca_ok = false; return false; }
    int idx = busca_ok ? busca_idx : (dir < 0 ? total - 1 : 0);
    for (int k = 0; k <= total; k++) {
        const char *ch;
        int n, col;
        linha_doc(sa, idx, &ch, &n);
        if (k == 0 && busca_ok) {
            if (mismo) col = tl_achar(ch, n, busca, ql, busca_col);
            else col = dir < 0 ? tl_achar_atras(ch, n, busca, ql, busca_col) : tl_achar(ch, n, busca, ql, busca_col + 1);
        } else col = dir < 0 ? tl_achar_atras(ch, n, busca, ql, n + 1) : tl_achar(ch, n, busca, ql, 0);
        if (col >= 0) {
            busca_ok = true; busca_idx = idx; busca_col = col;
            ir_para(idx);
            registrar("terminal: busqueda '%s' en la linea %d columna %d", busca, idx, col);
            return true;
        }
        idx = (idx + (dir < 0 ? -1 : 1) + total) % total;
    }
    busca_ok = false;
    sujar_tudo();
    return false;
}

static void fechar_busca(void)
{
    busca_ativa = busca_ok = false;
    sujar_tudo();
}

static void abrir_busca(void)
{
    if (busca_ativa) { fechar_busca(); return; }
    busca_ativa = true; busca_ok = false;
    sujar_tudo();
    desenhar_busca();
}

static void busca_tecla(const evento_t *e)
{
    unsigned t = e->tecla;
    int n = (int)tl_len(busca);
    if (t == TECLA_ESC || (t == 6 && (e->modificadores & 3) == 3)) { fechar_busca(); return; }
    if (t == '\n') { buscar((e->modificadores & 1) ? 1 : -1, false); desenhar_busca(); return; }
    if (t == '\b') { if (n) busca[n - 1] = 0; busca_ok = false; buscar(-1, true); desenhar_busca(); return; }
    if (t >= 0x20 && t < 0x100 && n < (int)sizeof busca - 1) {
        busca[n] = (char)t; busca[n + 1] = 0;
        if (!buscar(-1, true)) busca_ok = false;
        desenhar_busca();
    }
}

/* ------------------------------------------------------------ letra */

/* Ctrl+más / menos / 0: con la fuente interna, pasa a la primera vectorial monoespaciada. */
static void zoom(int delta)
{
    if (delta == 0) {
        tl_copiar(fonte_rota, sizeof fonte_rota, conf.fonte);
        fonte_px = conf.fpx;
    } else if (!fonte_rota[0]) {
        if (!primeira_mono(fonte_rota, sizeof fonte_rota)) { registrar("terminal: no hay fuentes vectoriales monoespaciadas"); return; }
        fonte_px = delta > 0 ? 20 : 14;
    } else {
        int p = fonte_px + delta;
        if (p < TL_PX_MIN || p > TL_PX_MAX) return;
        fonte_px = p;
    }
    fonte_aplicar();
    tamanho_minimo();
    novo_tamanho(true);
    registrar("terminal: letra %s %d px, celda %dx%d", fonte_rota[0] ? fonte_rota : "interna", fonte_px, celL, celA);
}

/* ------------------------------------------------------------ teclas hacia el shell */

/* D320: la tecla como la manda un xterm (solo cuando el cliente ssh lo pidió con CSI ? 7700 h).
 * Devuelve los bytes, o 0 si la tecla no tiene secuencia. */
static int tecla_xterm(const vt_t *v, unsigned t, unsigned m, char *b)
{
    int mod = 1 + ((m & 1) ? 1 : 0) + ((m & 4) ? 2 : 0) + ((m & 2) ? 4 : 0);   /* Mayús 1, Alt 2, Ctrl 4 */
    char fin = 0;
    int num = 0;
    switch (t) {
    case TECLA_CIMA: fin = 'A'; break;
    case TECLA_BAIXO: fin = 'B'; break;
    case TECLA_DIREITA: fin = 'C'; break;
    case TECLA_ESQUERDA: fin = 'D'; break;
    case TECLA_HOME: fin = 'H'; break;
    case TECLA_END: fin = 'F'; break;
    case TECLA_INSERT: num = 2; break;
    case TECLA_DELETE: num = 3; break;
    case TECLA_PGUP: num = 5; break;
    case TECLA_PGDN: num = 6; break;
    case TECLA_ESC: b[0] = 27; return 1;
    default:
        if (t >= 0x10A && t <= 0x115) {   /* F1..F12 */
            static const char fn[12] = { 'P', 'Q', 'R', 'S', 15, 17, 18, 19, 20, 21, 23, 24 };
            int f = (int)(t - 0x10A);
            if (f < 4) fin = fn[f]; else num = fn[f];
            if (f < 4 && mod == 1) { b[0] = 27; b[1] = 'O'; b[2] = fin; return 3; }
            break;
        }
        return 0;
    }
    int n = 0;
    b[n++] = 27;
    if (fin) {
        if (mod == 1) { b[n++] = v->app_cur && (fin <= 'D' || fin == 'H' || fin == 'F') ? 'O' : '['; b[n++] = fin; return n; }
        b[n++] = '['; b[n++] = '1'; b[n++] = ';'; b[n++] = (char)('0' + mod); b[n++] = fin; return n;
    }
    b[n++] = '[';
    if (num >= 10) { b[n++] = (char)('0' + num / 10); }
    b[n++] = (char)('0' + num % 10);
    if (mod != 1) { b[n++] = ';'; b[n++] = (char)('0' + mod); }
    b[n++] = '~';
    return n;
}

/* Tecla de la ventana -> bytes para el shell, en el formato del modo crudo. */
static void enviar_tecla(unsigned t, unsigned m)
{
    char b[16];
    int n = 1;
    sem_selecao(sa);
    voltar_para_baixo(sa);
    if (sa->vt.xterm) {
        n = tecla_xterm(&sa->vt, t, m, b);
        if (!n && t < 0x100) {
            int k = 0;
            if (m & 4) b[k++] = 27;   /* Alt+tecla = ESC + tecla */
            b[k++] = (char)t;
            n = k;
        }
        if (n) escrever_tudo(sa->mestre, b, (unsigned)n);
        return;
    }
    if (t < 0x100) b[0] = (char)t;
    else {
        b[0] = 27;
        switch (t) {
        case TECLA_CIMA: b[1] = TECLA_B_CIMA; break;
        case TECLA_BAIXO: b[1] = TECLA_B_BAIXO; break;
        case TECLA_ESQUERDA: b[1] = TECLA_B_ESQUERDA; break;
        case TECLA_DIREITA: b[1] = TECLA_B_DIREITA; break;
        case TECLA_HOME: b[1] = TECLA_B_HOME; break;
        case TECLA_END: b[1] = TECLA_B_END; break;
        case TECLA_DELETE: b[1] = TECLA_B_DELETE; break;
        case TECLA_PGUP: b[1] = TECLA_B_PGUP; break;
        case TECLA_PGDN: b[1] = TECLA_B_PGDN; break;
        case TECLA_ESC: b[1] = 27; break;
        default: return;
        }
        n = 2;
    }
    escrever_tudo(sa->mestre, b, (unsigned)n);
}

/* ------------------------------------------------------------ preferencias */

#define PW 640
#define PH 500
#define PV_X 196
#define PV_Y 32
#define PV_W 428
#define PV_H 96

typedef struct {
    jan_t     *jp;
    tl_conf_t  ed;
    char       itens_tema[TL_TEMAS + 1][64];
    lista_t    l_tema, l_fonte;
    char     (*itens_fonte)[64];
    char     (*rutas)[88];
    int        nf;
    bool       todas;
    casilla_t  c_todas, c_parp, c_barra;
    caixa_t    c_px, c_cols, c_lins, c_hist, c_hex;
    botao_t    b_cur, b_camp, b_ok, b_apl, b_can;
    int        item;           /* color elegido: 0..15 ANSI, 16 fondo, 17 letra, 18 cursor, 19 selección */
    fonte_t   *pf;             /* la fuente de la vista previa */
} prefs_t;

static prefs_t *pf;

static const char *const nomes_item[20] = {
    "negro", "rojo", "verde", "amarillo", "azul", "magenta", "cian", "blanco",
    "negro vivo", "rojo vivo", "verde vivo", "amarillo vivo", "azul vivo", "magenta vivo", "cian vivo", "blanco vivo",
    "fondo", "letra", "cursor", "selección",
};

static uint32_t *cor_do_item(tl_conf_t *c, int it)
{
    if (it < 16) return &c->ansi[it];
    return it == 16 ? &c->fundo : it == 17 ? &c->letra : it == 18 ? &c->cursor_cor : &c->selecao;
}

/* Posición de la muestra p (0..19) en la rejilla de 2 filas x 10: ANSI 0-7 + fondo + letra, y ANSI 8-15 + cursor + selección. */
static int item_da_posicao(int p) { return p < 8 ? p : p < 10 ? 16 + (p - 8) : p < 18 ? 8 + (p - 10) : 18 + (p - 18); }

#define SW_X PV_X
#define SW_Y 138
#define SW_L 42
#define SW_A 20

static void prefs_texto_prev(int x, int y, const char *t, uint32_t cor)
{
    int n = (int)tl_len(t);
    if (!pf->pf) { g_texto_n(pf->jp, x, y, t, n, cor); return; }
    int cl = (fonte_avanco(pf->pf, 'M', pf->ed.fpx) + 32) >> 6;
    for (int i = 0; i < n; i++)
        if (t[i] != ' ') fonte_texto_buf_n(pf->pf, pf->jp->px, pf->jp->larg, pf->jp->alt, x + i * cl, y, &t[i], 1, pf->ed.fpx, cor);
}

static void prefs_desenhar(void)
{
    prefs_t *p = pf;
    jan_t *w = p->jp;
    tl_conf_t *c = &p->ed;
    g_retangulo(w, 0, 0, w->larg, w->alt, COR_FUNDO);
    rotulo(w, 16, 10, "Tema", COR_TEXTO);
    lista_desenhar(w, &p->l_tema);
    /* vista previa */
    g_retangulo(w, PV_X, PV_Y, PV_W, PV_H, c->fundo);
    g_moldura(w, PV_X - 1, PV_Y - 1, PV_W + 2, PV_H + 2, COR_BORDA);
    int lh = p->pf ? fonte_altura_normal(p->pf, c->fpx) : LETRA_A, cl = p->pf ? (fonte_avanco(p->pf, 'M', c->fpx) + 32) >> 6 : LETRA_L;
    int x = PV_X + 8, y = PV_Y + 6;
    prefs_texto_prev(x, y, "sagui:/home$ ls", c->letra);
    y += lh;
    static const char *const pal[5] = { "Documentos", " script.sh", " imagen.bmp", " aviso", " error" };
    static const int pi[5] = { 4, 2, 5, 3, 1 };
    int xx = x;
    for (int i = 0; i < 5; i++) { prefs_texto_prev(xx, y, pal[i], c->ansi[pi[i]]); xx += (int)tl_len(pal[i]) * cl; }
    y += lh;
    g_retangulo(w, x, y, 18 * cl, lh, c->selecao);
    prefs_texto_prev(x, y, "texto seleccionado", c->fundo);
    y += lh;
    prefs_texto_prev(x, y, "sagui:/home$ ", c->letra);
    int cx = x + 13 * cl;
    if (c->cursor == TL_CUR_BLOCO) g_retangulo(w, cx, y, cl, lh, c->cursor_cor);
    else if (c->cursor == TL_CUR_SUBLINHADO) g_retangulo(w, cx, y + lh - 2, cl, 2, c->cursor_cor);
    else g_retangulo(w, cx, y, 2, lh, c->cursor_cor);
    /* muestras de color */
    for (int q = 0; q < 20; q++) {
        int it = item_da_posicao(q), sx = SW_X + (q % 10) * SW_L, sy = SW_Y + (q / 10) * (SW_A + 4);
        g_retangulo(w, sx, sy, SW_L - 3, SW_A, *cor_do_item(c, it));
        g_moldura(w, sx, sy, SW_L - 3, SW_A, it == p->item ? COR_DESTAQUE : COR_BORDA);
        if (it == p->item) g_moldura(w, sx + 1, sy + 1, SW_L - 5, SW_A - 2, COR_DESTAQUE);
    }
    char t[80];
    formatar_texto(t, sizeof t, "Color elegido: %s  (RRGGBB)", nomes_item[p->item]);
    rotulo(w, PV_X, 190, t, COR_TEXTO);
    rotulo(w, 16, 190, "Fuente", COR_TEXTO);
    lista_desenhar(w, &p->l_fonte);
    casilla_desenhar(w, &p->c_todas);
    rotulo(w, 36, 318, "Todas las fuentes", COR_TEXTO);
    rotulo(w, 16, 346, "Tamaño (px)", COR_TEXTO);
    caixa_desenhar(w, &p->c_px);
    rotulo(w, PV_X, 258, "Columnas", COR_TEXTO);
    rotulo(w, PV_X + 150, 258, "Filas", COR_TEXTO);
    rotulo(w, PV_X, 292, "Historial (líneas)", COR_TEXTO);
    caixa_desenhar(w, &p->c_hex);
    caixa_desenhar(w, &p->c_cols);
    caixa_desenhar(w, &p->c_lins);
    caixa_desenhar(w, &p->c_hist);
    botao_desenhar(w, &p->b_cur);
    botao_desenhar(w, &p->b_camp);
    casilla_desenhar(w, &p->c_parp);
    rotulo(w, PV_X + 20, 364, "Cursor parpadeante", COR_TEXTO);
    casilla_desenhar(w, &p->c_barra);
    rotulo(w, PV_X + 20, 388, "Barra de pestañas siempre visible", COR_TEXTO);
    rotulo(w, PV_X, 424, "Tamaño, historial y barra valen para ventanas y pestañas nuevas.", COR_TEXTO_FRACO);
    botao_desenhar(w, &p->b_ok);
    botao_desenhar(w, &p->b_apl);
    botao_desenhar(w, &p->b_can);
    jan_mostrar(w);
}

static void prefs_botoes_texto(void)
{
    static char tc[40], tp[40];
    formatar_texto(tc, sizeof tc, "Cursor: %s", pf->ed.cursor == TL_CUR_BLOCO ? "bloque" : pf->ed.cursor == TL_CUR_SUBLINHADO ? "subrayado" : "barra");
    formatar_texto(tp, sizeof tp, "Campana: %s", pf->ed.campana == TL_CAMP_NADA ? "ninguna" : pf->ed.campana == TL_CAMP_VISUAL ? "visual" : "sonora");
    pf->b_cur.texto = tc;
    pf->b_camp.texto = tp;
}

static void prefs_hex_da_cor(void)
{
    char h[8];
    formatar_texto(h, sizeof h, "%06X", (unsigned)*cor_do_item(&pf->ed, pf->item));
    caixa_definir(&pf->c_hex, h);
}

/* Reconstruye la lista de fuentes (las monoespaciadas, o todas) y deja elegida la actual. */
static void prefs_lista_fontes(void)
{
    static fuente_info_t cat[FUENTES_MAX];
    int n = fuentes_catalogo(cat, FUENTES_MAX), k = 0, sel = 0;
    if (!pf->itens_fonte) { pf->itens_fonte = aloca_zerado(201 * 64); pf->rutas = aloca_zerado(201 * 88); }
    if (!pf->itens_fonte || !pf->rutas) { pf->l_fonte.qtd = 0; return; }
    tl_copiar(pf->itens_fonte[0], 64, "Interna 8x16 (mapa de bits)");
    pf->rutas[0][0] = 0;
    k = 1;
    for (int i = 0; i < n && k < 201; i++) {
        if (!pf->todas && !nome_mono(cat[i].familia)) continue;
        formatar_texto(pf->itens_fonte[k], 64, "%s %s", cat[i].familia, cat[i].estilo);
        tl_copiar(pf->rutas[k], 88, cat[i].ruta);
        if (!compara_texto(cat[i].ruta, pf->ed.fonte)) sel = k;
        k++;
    }
    pf->nf = k;
    pf->l_fonte.itens = pf->itens_fonte;
    pf->l_fonte.qtd = k;
    pf->l_fonte.sel = sel;
    pf->l_fonte.topo = sel > 3 ? sel - 3 : 0;
}

static void prefs_abrir_fonte_prev(void)
{
    if (pf->pf) { fonte_fechar(pf->pf); pf->pf = NULL; }
    if (pf->ed.fonte[0]) pf->pf = fonte_abrir_archivo(pf->ed.fonte);
}

static void prefs_ler_campos(void)
{
    pf->ed.fpx = tl_num(pf->c_px.texto, TL_PX_MIN, TL_PX_MAX, pf->ed.fpx);
    pf->ed.cols = tl_num(pf->c_cols.texto, TL_COLS_MIN, TL_COLS_MAX, pf->ed.cols);
    pf->ed.lins = tl_num(pf->c_lins.texto, TL_LINS_MIN, TL_LINS_MAX, pf->ed.lins);
    pf->ed.hist = tl_num(pf->c_hist.texto, 0, TL_HIST_MAX, pf->ed.hist);
    pf->ed.parpadeo = pf->c_parp.marcada;
    pf->ed.barra_siempre = pf->c_barra.marcada;
    uint32_t cor;   /* un color escrito y no confirmado con Intro también cuenta */
    if (tl_cor_de_texto(pf->c_hex.texto, &cor) && cor != *cor_do_item(&pf->ed, pf->item)) {
        *cor_do_item(&pf->ed, pf->item) = cor;
        pf->ed.tema = TL_PERSONALIZADO;
        pf->l_tema.sel = TL_TEMAS;
    }
}

static void prefs_fechar(void)
{
    if (!pf) return;
    if (pf->pf) fonte_fechar(pf->pf);
    if (pf->itens_fonte) libera(pf->itens_fonte);
    if (pf->rutas) libera(pf->rutas);
    jan_fechar(pf->jp);
    libera(pf);
    pf = NULL;
}

/* Aplica lo elegido: guarda terminal.conf y lo pone en esta ventana. */
static void prefs_aplicar(void)
{
    prefs_ler_campos();
    conf = pf->ed;
    conf_gravar();
    tl_copiar(fonte_rota, sizeof fonte_rota, conf.fonte);
    fonte_px = conf.fpx;
    fonte_aplicar();
    calcular_y0();
    tamanho_minimo();
    novo_tamanho(true);
    barra_suja = true;
    cursor_aceso = true;
}

static void prefs_abrir(void)
{
    if (pf) return;
    pf = aloca_zerado(sizeof *pf);
    if (!pf) return;
    pf->jp = jan_abrir("Terminal - Preferencias", PW, PH);
    if (!pf->jp) { libera(pf); pf = NULL; return; }
    jan_estilo(pf->jp, JAN_DIALOGO, 0, 0);
    pf->ed = conf;
    for (int i = 0; i < TL_TEMAS; i++) tl_copiar(pf->itens_tema[i], 64, tl_temas[i].nome);
    tl_copiar(pf->itens_tema[TL_TEMAS], 64, "Personalizado");
    pf->l_tema = (lista_t){ .x = 16, .y = 32, .l = 160, .a = (TL_TEMAS + 1) * LISTA_ALTURA_ITEM + 4, .itens = pf->itens_tema,
                            .qtd = TL_TEMAS + 1, .sel = conf.tema };
    pf->l_fonte = (lista_t){ .x = 16, .y = 208, .l = 160, .a = 5 * LISTA_ALTURA_ITEM + 4 };
    prefs_lista_fontes();
    pf->c_todas = (casilla_t){ 16, 318, false };
    pf->c_px = (caixa_t){ .x = 110, .y = 342, .l = 66 };
    char b[16];
    formatar_texto(b, sizeof b, "%d", conf.fpx); caixa_definir(&pf->c_px, b);
    pf->c_cols = (caixa_t){ .x = PV_X + 76, .y = 254, .l = 60 };
    formatar_texto(b, sizeof b, "%d", conf.cols); caixa_definir(&pf->c_cols, b);
    pf->c_lins = (caixa_t){ .x = PV_X + 190, .y = 254, .l = 60 };
    formatar_texto(b, sizeof b, "%d", conf.lins); caixa_definir(&pf->c_lins, b);
    pf->c_hist = (caixa_t){ .x = PV_X + 140, .y = 288, .l = 80 };
    formatar_texto(b, sizeof b, "%d", conf.hist); caixa_definir(&pf->c_hist, b);
    pf->c_hex = (caixa_t){ .x = PV_X, .y = 210, .l = 100 };
    pf->item = 17;
    prefs_hex_da_cor();
    pf->c_parp = (casilla_t){ PV_X, 364, conf.parpadeo != 0 };
    pf->c_barra = (casilla_t){ PV_X, 388, conf.barra_siempre != 0 };
    pf->b_cur = (botao_t){ PV_X, 322, 190, 26, "", false };
    pf->b_camp = (botao_t){ PV_X + 206, 322, 200, 26, "", false };
    pf->b_ok = (botao_t){ PW - 106, PH - 40, 90, 26, "Aceptar", false };
    pf->b_apl = (botao_t){ PW - 206, PH - 40, 90, 26, "Aplicar", false };
    pf->b_can = (botao_t){ PW - 306, PH - 40, 90, 26, "Cancelar", false };
    prefs_botoes_texto();
    prefs_abrir_fonte_prev();
    prefs_desenhar();
    registrar("terminal: preferencias abiertas");
}

/* Un evento de la ventana de preferencias. */
static void prefs_evento(const evento_t *e)
{
    prefs_t *p = pf;
    bool redibujar = false;
    if (e->tipo == EV_FECHAR || (e->tipo == EV_TECLA && e->tecla == TECLA_ESC)) { prefs_fechar(); return; }
    int r = lista_tratar(p->jp, &p->l_tema, e);
    if (r != LISTA_NADA) {
        int i = p->l_tema.sel;
        if (i < TL_TEMAS) tl_conf_tema(&p->ed, i); else p->ed.tema = TL_PERSONALIZADO;
        prefs_hex_da_cor();
        redibujar = true;
    }
    r = lista_tratar(p->jp, &p->l_fonte, e);
    if (r != LISTA_NADA) {
        tl_copiar(p->ed.fonte, sizeof p->ed.fonte, p->rutas[p->l_fonte.sel]);
        prefs_ler_campos();
        prefs_abrir_fonte_prev();
        redibujar = true;
    }
    if (casilla_tratar(&p->c_todas, e)) { p->todas = p->c_todas.marcada; prefs_lista_fontes(); redibujar = true; }
    if (casilla_tratar(&p->c_parp, e) || casilla_tratar(&p->c_barra, e)) { prefs_ler_campos(); redibujar = true; }
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {   /* muestras de color */
        for (int q = 0; q < 20; q++) {
            int sx = SW_X + (q % 10) * SW_L, sy = SW_Y + (q / 10) * (SW_A + 4);
            if (e->x >= sx && e->x < sx + SW_L - 3 && e->y >= sy && e->y < sy + SW_A) {
                p->item = item_da_posicao(q);
                prefs_hex_da_cor();
                redibujar = true;
            }
        }
    }
    caixa_t *cs[5] = { &p->c_px, &p->c_cols, &p->c_lins, &p->c_hist, &p->c_hex };
    for (int i = 0; i < 5; i++) {
        if (e->tipo == EV_CLIQUE && cs[i] != &p->c_hex) { /* el foco lo decide cada caja */ }
        if (caixa_tratar(p->jp, cs[i], e) && cs[i] == &p->c_hex) {
            uint32_t cor;
            if (tl_cor_de_texto(p->c_hex.texto, &cor)) {
                *cor_do_item(&p->ed, p->item) = cor;
                p->ed.tema = TL_PERSONALIZADO;
                p->l_tema.sel = TL_TEMAS;
                redibujar = true;
            }
        }
    }
    if (botao_tratar(p->jp, &p->b_cur, e)) { p->ed.cursor = (p->ed.cursor + 1) % TL_CUR_QTD; prefs_botoes_texto(); redibujar = true; }
    if (botao_tratar(p->jp, &p->b_camp, e)) {
        p->ed.campana = (p->ed.campana + 1) % TL_CAMP_QTD;
        if (p->ed.campana == TL_CAMP_SOM) bip(880, 70);
        prefs_botoes_texto();
        redibujar = true;
    }
    if (botao_tratar(p->jp, &p->b_apl, e)) { prefs_aplicar(); redibujar = true; }
    if (botao_tratar(p->jp, &p->b_ok, e)) { prefs_aplicar(); prefs_fechar(); return; }
    if (botao_tratar(p->jp, &p->b_can, e)) { prefs_fechar(); return; }
    if (redibujar) prefs_desenhar();
}

/* ------------------------------------------------------------ teclas y ratón de la ventana */

/* Atajos del Terminal. true = tratada (no va al shell). */
static bool atalho(const evento_t *e)
{
    unsigned t = e->tecla, m = e->modificadores;
    if (busca_ativa) { busca_tecla(e); return true; }
    if ((m & 3) == 3) {   /* Ctrl+Mayús+... */
        if (t == 20) { nova_pestana(); return true; }
        if (t == 23) { fechar_pestana(atual, true); return true; }
        if (t == 6) { abrir_busca(); return true; }
        if (t == 3) { copiar(); return true; }
        if (t == 22) { colar(); return true; }
    }
    if ((m & 2) && !(m & 4)) {   /* Ctrl solo */
        if (t == TECLA_PGUP) { trocar((atual + nsess - 1) % nsess); return true; }
        if (t == TECLA_PGDN) { trocar((atual + 1) % nsess); return true; }
        if (t == 9) { trocar((m & 1) ? (atual + nsess - 1) % nsess : (atual + 1) % nsess); return true; }
        if (t >= '1' && t <= '9') { trocar(t == '9' && nsess < 9 ? nsess - 1 : (int)(t - '1')); return true; }
        if (t == ',') { prefs_abrir(); return true; }
        if (t == '+' || t == '=' ) { zoom(2); return true; }
        if (t == '-' || t == '_') { zoom(-2); return true; }
        if (t == '0') { zoom(0); return true; }
    }
    return false;
}

#define MENU_PRINCIPAL "Limpiar pantalla\tCtrl+L\n" "Copiar\tCtrl+Mayús+C\n" "Pegar\tCtrl+Mayús+V\n" "Seleccionar todo\n" \
                       "Buscar...\tCtrl+Mayús+F\n-\n" "Nueva pestaña\tCtrl+Mayús+T\n" "Nueva ventana\n-\n" \
                       "Preferencias...\tCtrl+,\n-\n" "Cerrar pestaña\tCtrl+Mayús+W"

static void ratom(const evento_t *e)
{
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
        int b = barra_ponto(e->x, e->y);
        if (b >= 0) {
            if (b == 200) nova_pestana();
            else if (b >= 100) fechar_pestana(b - 100, true);
            else trocar(b);
            return;
        }
        if (e->y < y0 - MARGEM + 1 && barra_visivel()) return;
        int l, c;
        celula(e->x, e->y, &l, &c);
        if ((e->modificadores & 2) && abrir_enlace(l, c)) return;
        sem_selecao(sa);
        if (e->cliques >= 2) { selecionar_palavra(l, c); return; }
        sa->sel_al = l; sa->sel_ac = c; sa->sel_l = l; sa->sel_c = c;
        sa->selecionando = true;
        return;
    }
    if (e->tipo == EV_CLIQUE && (e->botoes & 4)) {   /* botón del medio: pestaña -> cerrar, texto -> pegar */
        int b = barra_ponto(e->x, e->y);
        if (b >= 0 && b < 100) fechar_pestana(b, true);
        else if (b < 0) colar();
        return;
    }
    if (e->tipo == EV_MOVER && sa->selecionando && (e->botoes & 1)) {
        int l, c;
        celula(e->x, e->y, &l, &c);
        if (l != sa->sel_l || c != sa->sel_c || !sa->sel_ativa) {
            int l0 = sa->sel_l, l1 = l;
            if (l0 > l1) { int x = l0; l0 = l1; l1 = x; }
            if (sa->sel_al < l0) l0 = sa->sel_al;
            if (sa->sel_al > l1) l1 = sa->sel_al;
            sa->sel_l = l; sa->sel_c = c;
            sa->sel_ativa = true;
            for (int k = l0; k <= l1; k++) sujar(k);
        }
        return;
    }
    if (e->tipo == EV_SOLTOU && sa->selecionando) { sa->selecionando = false; return; }
    if (e->tipo == EV_CLIQUE && (e->botoes & 2)) {
        int b = barra_ponto(e->x, e->y);
        if (b >= 0 && b < 100) { menu_tab = b; menu_contextual(j->id, e->x, e->y, "Nueva pestaña\nCerrar esta pestaña", 1); }
        else if (b < 0) menu_contextual(j->id, e->x, e->y, MENU_PRINCIPAL, 0);
    }
}

static void menu_resposta(const evento_t *e)
{
    int k = e->x;
    if (e->tecla == 1) {
        if (k == 0) nova_pestana();
        else if (k == 1) fechar_pestana(menu_tab, true);
        return;
    }
    registrar("terminal: menu -> %d", k);
    if (k == 0) enviar_tecla(12, 0);
    else if (k == 1) copiar();
    else if (k == 2) colar();
    else if (k == 3) selecionar_tudo();
    else if (k == 4) abrir_busca();
    else if (k == 6) nova_pestana();
    else if (k == 7) { char *a[] = { "terminal", NULL }; executar("/bin/terminal", a); }
    else if (k == 9) prefs_abrir();
    else if (k == 11) fechar_pestana(atual, true);
}

/* ------------------------------------------------------------ principal */

int principal(int argc, char **argv)
{
    (void)argc; (void)argv;
    conf_ler();
    tl_copiar(fonte_rota, sizeof fonte_rota, conf.fonte);
    fonte_px = conf.fpx;
    fonte_aplicar();
    calcular_y0();
    j = jan_abrir("Terminal", conf.cols * celL + 2 * MARGEM, conf.lins * celA + 2 * MARGEM + (conf.barra_siempre ? BARRA_A : 0));
    /* Con poca RAM (32 MB) el kernel puede no tener un hueco seguido para el búfer de la ventana
     * (documentado en D313/h95): se reintenta con menos líneas en vez de salir con error. */
    while (!j && conf.lins > 10) {
        conf.lins -= 2;
        j = jan_abrir("Terminal", conf.cols * celL + 2 * MARGEM, conf.lins * celA + 2 * MARGEM + (conf.barra_siempre ? BARRA_A : 0));
    }
    if (!j) return 1;
    tamanho_minimo();
    g_retangulo(j, 0, 0, j->larg, j->alt, conf.fundo);   /* márgenes también oscuros */
    sessao_t *primeira = sessao_nova();
    if (!primeira) { jan_fechar(j); return 1; }
    sa = primeira; atual = 0;
    novo_tamanho(false);
    registrar("terminal: tema %s, fuente %s %d px, %d columnas x %d lineas, historial %d",
              conf.tema == TL_PERSONALIZADO ? "personalizado" : tl_temas[conf.tema].id,
              fonte_rota[0] ? fonte_rota : "interna", fonte_px, COLS, LINS, conf.hist);

    char buf[512];
    evento_t e;
    unsigned t_antes = tempo_ms();
    t_cursor = t_antes; t_titulos = t_antes;
    for (;;) {
        bool fez = false;
        for (int i = 0; i < nsess; i++) {
            sessao_t *s = sess[i];
            int d = disponivel(s->mestre);
            if (d == ERRO_PIPE_QUEBRADO) { shell_terminou(i); i--; fez = true; continue; }   /* el shell terminó (sair) */
            if (d > 0) {
                int n = ler(s->mestre, buf, sizeof buf);
                for (int k = 0; k < n; k++) receber(s, (unsigned char)buf[k]);
                if (n > 0) copiar_serie(s, buf, n);  /* copia a la serial, sin colores ni títulos */
                if (n > 0 && s == sa) { cursor_aceso = true; t_cursor = tempo_ms(); }
                fez = true;
            }
        }
        if (nsess == 0) break;
        while (proximo_evento(j->id, &e, false) == 1) {
            fez = true;
            if (e.tipo == EV_FECHAR) {
                if (nsess > 1) {
                    atualizar_titulos();
                    bool marcha = false;
                    for (int i = 0; i < nsess; i++) if (sess[i]->em_marcha) marcha = true;
                    if (marcha && !confirmar("Cerrar Terminal", "Hay órdenes en marcha en las pestañas.\n¿Cerrar la ventana y terminarlas?")) continue;
                }
                goto fim;
            }
            if (e.tipo == EV_TECLA) {
                cursor_aceso = true; t_cursor = tempo_ms();
                if (!atalho(&e)) enviar_tecla(e.tecla, e.modificadores);
                if (nsess == 0) break;
                continue;
            }
            if (e.tipo == EV_RODA) rodar((int)e.cliques);
            else if (e.tipo == EV_MENU) menu_resposta(&e);
            else if (jan_redimensionar(j, &e)) novo_tamanho(true);
            else ratom(&e);
            if (nsess == 0) break;
        }
        if (nsess == 0) break;
        if (pf) {
            while (pf && proximo_evento(pf->jp->id, &e, false) == 1) { fez = true; prefs_evento(&e); }
        }
        unsigned agora = tempo_ms();
        if (inverso && agora >= t_campana) { inverso = false; sujar_tudo(); }
        if (conf.parpadeo && agora - t_cursor >= 530) {
            cursor_aceso = !cursor_aceso; t_cursor = agora;
            sujar(sa->vt.lin);
        }
        if (agora - t_titulos >= 500) {
            t_titulos = agora;
            if (barra_visivel()) atualizar_titulos();
        }
        mostrar();
        /* ¿Un shell terminó? (No se puede depender solo del fin del pipe: un
         * programa gráfico abierto por el shell hereda la salida y la mantiene abierta.) */
        int c, pid;
        while ((pid = recolher(&c)) > 0) {
            for (int i = 0; i < nsess; i++)
                if (sess[i]->shell == pid) { sess[i]->shell = -1; shell_terminou(i); fez = true; break; }
        }
        if (nsess == 0) break;
        if (!fez) dormir(10);
    }
fim:
    prefs_fechar();
    while (nsess > 0) {
        int pid = sess[0]->shell, c;
        if (pid > 0) { matar(pid); esperar(pid, &c); }
        sess[0]->shell = -1;
        sessao_tirar(0);
    }
    if (fnt) fonte_fechar(fnt);
    jan_fechar(j);
    return 0;
}
