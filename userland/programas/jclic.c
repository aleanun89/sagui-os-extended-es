/* jclic - visor de actividades educativas JClic (D185).
 *
 *   jclic [proyecto.jclic.zip | proyecto.jclic]
 *   jclic --autoprueba proyecto.jclic[.zip] [--semilla N]
 *
 * Abre los proyectos del repositorio de Clic del XTEC (https://projectes.xtec.cat/clic/): un ZIP con
 * un .jclic (XML) y sus medios, o el .jclic suelto con los medios al lado. Sin argumento enseña la lista
 * de los proyectos de /jogos/jclic (los que vienen con el sistema) y de /home/jclic (los bajados con
 * "Actividades JClic (bajar)", marcados con +), como el selector del CHIP-8.
 *
 * Se juegan: pantalla de información, puzles (doble, de intercambio y de agujero), asociación simple y
 * compleja, memoria, explorar, identificar, sopa de letras y crucigrama. Las actividades de texto
 * (@text.*, que son un tercio de las de los proyectos) y las de tipo desconocido avisan y se pueden saltar.
 * Todo el formato y la jugabilidad son propios (no se ha copiado código de JClic ni de JClic.js):
 * ver docs/terceros.md.
 *
 * Teclas: N o flecha derecha = siguiente actividad, P o flecha izquierda = anterior, F5 = reiniciar,
 * F1 = ayuda, Esc = volver a la lista; en el crucigrama se escribe con el teclado.
 * --autoprueba resuelve cada actividad por las mismas funciones que usa el ratón (sin ventana) y
 * escribe el resultado por la serie.
 *
 * Solo enteros (la libsagui no usa coma flotante); la fuente es la de 8x16 ampliada a enteros. */
#include "sagui.h"
#include "gui.h"
#include "jclic_motor.h"

extern const unsigned char fonte_sagui[256][16];


/* Coloca un botón sin tocar su estado de pulsación (si se rehiciera entero, un clic que llega en dos
 * eventos —pulsar y soltar— perdería el "apretado" entre un dibujo y otro). */
static void bot_pos(botao_t *b, int x, int y, int l, int a, const char *t)
{
    b->x = x; b->y = y; b->l = l; b->a = a; b->texto = t;
}

#define MSG_A 64
#define INF_A 38
#define LARG_INICIO 820
#define ALT_INICIO  600

enum { M_LISTA, M_JOGO };

static jan_t *jan;
static uint32_t *jpx;
static int modo = M_LISTA;
static int seq_atual = -1;
static bool quer_sair;
static char aviso_lista[200];

/* ------------------------------------------------------------ dibujo básico */

static void ret(int x, int y, int w, int h, uint32_t cor)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > jw ? jw : x + w, y1 = y + h > jh ? jh : y + h;
    for (int yy = y0; yy < y1; yy++) {
        uint32_t *l = jpx + yy * jw;
        for (int xx = x0; xx < x1; xx++) l[xx] = cor;
    }
}

static void moldura(int x, int y, int w, int h, int e, uint32_t cor)
{
    ret(x, y, w, e, cor); ret(x, y + h - e, w, e, cor); ret(x, y, e, h, cor); ret(x + w - e, y, e, h, cor);
}

/* Un carácter de 8x16 ampliado s veces, recortado a (cx0,cy0)-(cx1,cy1). */
static void glifo(int x, int y, int ch, int s, uint32_t cor, int cx0, int cy0, int cx1, int cy1)
{
    const unsigned char *g = fonte_sagui[(uint8_t)ch];
    for (int lin = 0; lin < 16; lin++) {
        int yy = y + lin * s;
        if (yy + s <= cy0 || yy >= cy1) continue;
        for (int col = 0; col < 8; col++) {
            if (!(g[lin] & (0x80 >> col))) continue;
            int xx = x + col * s;
            if (xx + s <= cx0 || xx >= cx1) continue;
            int a0 = xx < cx0 ? cx0 : xx, b0 = yy < cy0 ? cy0 : yy;
            int a1 = xx + s > cx1 ? cx1 : xx + s, b1 = yy + s > cy1 ? cy1 : yy + s;
            ret(a0, b0, a1 - a0, b1 - b0, cor);
        }
    }
}

#define LIN_MAX 80
typedef struct { const char *p; int n; } trecho_t;

/* Parte el texto en líneas de como mucho maxc caracteres: por palabras, y por letras si una no cabe.
 * El texto se normaliza antes (espacios repetidos, saltos de línea de los párrafos se respetan). */
static int quebrar(const char *t, int maxc, trecho_t *lin)
{
    int nl = 0;
    if (maxc < 1) maxc = 1;
    const char *p = t;
    while (*p && nl < LIN_MAX) {
        const char *fim = acha_char(p, '\n');
        int ln = fim ? (int)(fim - p) : (int)tam_texto(p);
        const char *q = p, *e = p + ln;
        if (ln == 0) { lin[nl].p = p; lin[nl++].n = 0; }
        while (q < e && nl < LIN_MAX) {
            while (q < e && *q == ' ') q++;
            if (q >= e) break;
            int len = (int)(e - q);
            if (len <= maxc) { lin[nl].p = q; lin[nl++].n = len; break; }
            int corte = maxc;
            while (corte > 0 && q[corte] != ' ') corte--;
            if (corte == 0) corte = maxc;   /* palabra más larga que la línea */
            int fin = corte;
            while (fin > 0 && q[fin - 1] == ' ') fin--;
            lin[nl].p = q; lin[nl++].n = fin;
            q += corte;
        }
        p += ln;
        if (*p == '\n') p++;
        else break;
    }
    return nl;
}

/* Texto dentro de una caja, con el mayor tamaño entero que quepa (hasta smax). */
static void texto_caixa(int x, int y, int w, int h, const char *txt, uint32_t cor, int smax, int ah, int av)
{
    if (!txt || !*txt) return;
    int mx = w >= 12 ? 4 : 0;
    static trecho_t lin[LIN_MAX];
    int s = smax < 1 ? 1 : smax > 6 ? 6 : smax, nl = 0;
    for (; s >= 1; s--) {
        nl = quebrar(txt, (w - 2 * mx) / (8 * s), lin);
        if (nl * 16 * s <= h - 2 || s == 1) break;
    }
    int alto = nl * 16 * s;
    int y0 = av == 0 ? y + 2 : av == 2 ? y + h - alto - 2 : y + (h - alto) / 2;
    for (int i = 0; i < nl; i++) {
        int lw = lin[i].n * 8 * s;
        int x0 = ah == 0 ? x + mx : ah == 2 ? x + w - lw - mx : x + (w - lw) / 2;
        for (int k = 0; k < lin[i].n; k++) glifo(x0 + k * 8 * s, y0 + i * 16 * s, (unsigned char)lin[i].p[k], s, cor, x, y, x + w, y + h);
    }
}

/* Un trozo (sx,sy,sw,sh) de una imagen estirado a (dx,dy,dw,dh) y recortado a la caja (cx0..cx1). */
static void blit(const imagem_t *im, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                 int cx0, int cy0, int cx1, int cy1)
{
    if (dw <= 0 || dh <= 0 || sw <= 0 || sh <= 0) return;
    int x0 = dx > cx0 ? dx : cx0, y0 = dy > cy0 ? dy : cy0;
    int x1 = dx + dw < cx1 ? dx + dw : cx1, y1 = dy + dh < cy1 ? dy + dh : cy1;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > jw) x1 = jw;
    if (y1 > jh) y1 = jh;
    int px = (int)(((long)sw << 16) / dw), py = (int)(((long)sh << 16) / dh);
    bool suave = px > 98304 || py > 98304;   /* más de 1,5 píxeles de origen por píxel: promedia 2x2 */
    for (int y = y0; y < y1; y++) {
        for (int x = x0; x < x1; x++) {
            int u = (sx << 16) + (x - dx) * px + px / 2, v = (sy << 16) + (y - dy) * py + py / 2;
            uint32_t c;
            if (!suave) {
                int iu = u >> 16, iv = v >> 16;
                if (iu >= im->larg) iu = im->larg - 1;
                if (iv >= im->alt) iv = im->alt - 1;
                c = im->px[iv * im->larg + iu];
            } else {
                unsigned r = 0, g = 0, b = 0;
                for (int k = 0; k < 4; k++) {
                    int iu = (u + ((k & 1) ? px / 4 : -px / 4)) >> 16, iv = (v + ((k & 2) ? py / 4 : -py / 4)) >> 16;
                    if (iu < 0) iu = 0;
                    if (iv < 0) iv = 0;
                    if (iu >= im->larg) iu = im->larg - 1;
                    if (iv >= im->alt) iv = im->alt - 1;
                    uint32_t q = im->px[iv * im->larg + iu];
                    r += (q >> 16) & 255; g += (q >> 8) & 255; b += q & 255;
                }
                c = ((r / 4) << 16) | ((g / 4) << 8) | (b / 4);
            }
            jpx[y * jw + x] = c;
        }
    }
}

/* ------------------------------------------------------------ una caja del tablero */

static int escala_painel(const pn_t *p)
{
    return p->cw > 0 && p->col > 0 ? (int)((long)p->w * 1024 / ((long)p->col * p->cw)) : 1024;
}

static int fonte_para(const estilo_t *e, int esc1024)
{
    int tam = e->tam ? e->tam : 20;
    int px = (int)((long)tam * esc1024 / 1024);
    int s = (px + 7) / 16;
    return s < 1 ? 1 : s;
}

static void pintar_conteudo(const cx_t *b, const cel_t *c, int x, int y, int w, int h, int esc)
{
    if (b->g->imagem && !c->img) {
        imagem_t *im = img_obter(b->g->imagem);
        if (im) {
            int col = b->g->col, lin = b->g->lin, i = b->cel;
            int sx = (i % col) * im->larg / col, sx2 = (i % col + 1) * im->larg / col;
            int sy = (i / col) * im->alt / lin, sy2 = (i / col + 1) * im->alt / lin;
            blit(im, sx, sy, sx2 - sx, sy2 - sy, x, y, w, h, x, y, x + w, y + h);
        } else {
            ret(x, y, w, h, 0xCCCCCC);
            g_texto(jan, x + 4, y + 4, "imagen?", 0x884444);
        }
    }
    if (c->img) {
        imagem_t *im = img_obter(c->img);
        if (im) {
            int iw = w - 4, ih = h - 4;
            if (iw > 0 && ih > 0) {
                int dw = iw, dh = (int)((long)im->alt * iw / im->larg);
                if (dh > ih) { dh = ih; dw = (int)((long)im->larg * ih / im->alt); }
                if (dw < 1) dw = 1;
                if (dh < 1) dh = 1;
                blit(im, 0, 0, im->larg, im->alt, x + (w - dw) / 2, y + (h - dh) / 2, dw, dh, x, y, x + w, y + h);
            }
        } else {
            g_texto(jan, x + 4, y + h / 2 - 8, "(sin imagen)", 0x884444);
        }
    }
    if (c->txt && c->txt[0]) texto_caixa(x, y, w, h, c->txt, c->est.fg, fonte_para(&c->est, esc), c->ah, c->av);
}

static void pintar_caixa(const pn_t *p, int idx, int pi)
{
    const cx_t *b = &p->c[idx];
    int x = b->x, y = b->y, w = b->w, h = b->h;
    if (w < 2 || h < 2) return;
    int esc = escala_painel(p);
    bool sel = sel_p == pi && sel_i == idx;
    static const cel_t vazia = { "", NULL, NULL, NULL, -1, -1, { 0, 0xFFFFFF, 0, false, 0, false }, 1, 1 };
    const cel_t *c = b->cel >= 0 && b->g ? &b->g->c[b->cel] : &vazia;
    uint32_t fundo = c->est.bg;
    if (b->est == E_VAZIO) {
        uint32_t ina = c->est.tem_inativo ? c->est.inativo : at.cor_jan;
        ret(x, y, w, h, ina);
        moldura(x, y, w, h, 1, 0xA0A0A0);
        return;
    }
    if (at.tipo == T_MEMORIA && b->est == E_NORMAL) {   /* carta boca abajo */
        ret(x, y, w, h, 0x3050A0);
        moldura(x + 2, y + 2, w - 4, h - 4, 2, 0x90A8E0);
        int s = h / 40 + 1;
        if (s > 6) s = 6;
        glifo(x + (w - 8 * s) / 2, y + (h - 16 * s) / 2, '?', s, 0xE0E8FF, x, y, x + w, y + h);
        return;
    }
    ret(x, y, w, h, fundo);
    if (b->cel >= 0) pintar_conteudo(b, c, x, y, w, h, esc);
    if (b->g && b->g->borda) moldura(x, y, w, h, 1, 0x404040);
    if (b->est == E_ACERTO && at.tipo != T_DOBLE && at.tipo != T_MEMORIA) moldura(x + 1, y + 1, w - 2, h - 2, 3, 0x20A040);
    if (b->est == E_ACERTO && at.tipo == T_MEMORIA) moldura(x + 1, y + 1, w - 2, h - 2, 2, 0x20A040);
    if (sel) moldura(x + 1, y + 1, w - 2, h - 2, 3, 0xFF8000);
    if (err_p == pi && err_i == idx) moldura(x + 1, y + 1, w - 2, h - 2, 3, 0xE02020);
    if (at.tipo == T_EXPLORA && ((pi == 0 && exp_a == idx) || (pi == 1 && exp_b == idx))) moldura(x + 1, y + 1, w - 2, h - 2, 4, 0x2060FF);
}

/* ------------------------------------------------------------ sopa de letras y crucigrama */

static void mini_num(int x, int y, int n, uint32_t cor)
{
    static const uint8_t d[10][5] = {
        { 7, 5, 5, 5, 7 }, { 2, 6, 2, 2, 7 }, { 7, 1, 7, 4, 7 }, { 7, 1, 7, 1, 7 }, { 5, 5, 7, 1, 1 },
        { 7, 4, 7, 1, 7 }, { 7, 4, 7, 5, 7 }, { 7, 1, 1, 1, 1 }, { 7, 5, 7, 5, 7 }, { 7, 5, 7, 1, 7 },
    };
    char s[8];
    int ln = formatar_texto(s, sizeof s, "%d", n);
    for (int k = 0; k < ln; k++)
        for (int r = 0; r < 5; r++)
            for (int c = 0; c < 3; c++)
                if (d[s[k] - '0'][r] & (4 >> c)) ret(x + k * 4 + c, y + r, 1, 1, cor);
}

static void grade_geometria(int larg_max)
{
    int cw = larg_max / (at.tg_col > 0 ? at.tg_col : 1), ch = (area_h - 16) / (at.tg_lin > 0 ? at.tg_lin : 1);
    int c = cw < ch ? cw : ch;
    if (c > 48) c = 48;
    if (c < 10) c = 10;
    sp_cw = sp_ch = c;
    sp_ox = area_x + 8;
    sp_oy = area_y + 8 + (area_h - 16 - c * at.tg_lin) / 2;
    if (sp_oy < area_y + 8) sp_oy = area_y + 8;
}

static void pintar_sopa(void)
{
    grade_geometria(area_w * 6 / 10);
    ret(sp_ox - 4, sp_oy - 4, sp_cw * at.tg_col + 8, sp_ch * at.tg_lin + 8, at.tg_bg);
    int s = sp_cw / 16;
    if (s < 1) s = 1;
    int dx0 = -1, dy0 = 0, n = 0;
    if (sp_x0 >= 0) sp_linea(sp_x0, sp_y0, sp_x1, sp_y1, &dx0, &dy0, &n);
    for (int y = 0; y < at.tg_lin; y++)
        for (int x = 0; x < at.tg_col; x++) {
            int cx = sp_ox + x * sp_cw, cy = sp_oy + y * sp_ch;
            uint32_t fundo = at.tg_bg;
            bool na_sel = false;
            if (sp_x0 >= 0 && n > 0 && dx0 > -2) {
                int ex = x - sp_x0, ey = y - sp_y0;
                for (int k = 0; k < n; k++) if (ex == dx0 * k && ey == dy0 * k) na_sel = true;
            }
            if (sp_est[y][x]) fundo = 0x90E090;
            if (na_sel) fundo = 0x80B0FF;
            if (fundo != at.tg_bg) ret(cx + 1, cy + 1, sp_cw - 2, sp_ch - 2, fundo);
            moldura(cx, cy, sp_cw, sp_ch, 1, 0xA0A090);
            glifo(cx + (sp_cw - 8 * s) / 2, cy + (sp_ch - 16 * s) / 2, (unsigned char)sp_letras[y][x], s, 0x202020, cx, cy, cx + sp_cw, cy + sp_ch);
        }
    int lx = sp_ox + sp_cw * at.tg_col + 20, ly = area_y + 12;
    g_texto(jan, lx, ly, "Palabras:", 0x202020);
    ly += 24;
    for (int i = 0; i < at.n_pal; i++) {
        uint32_t cor = sp_ok[i] ? 0x108030 : 0x202020;
        g_texto(jan, lx, ly, at.pal[i], cor);
        if (sp_ok[i]) ret(lx, ly + 8, (int)tam_texto(at.pal[i]) * 8, 2, 0x108030);
        ly += 20;
        if (ly > area_y + area_h - 20) { lx += 150; ly = area_y + 36; }
    }
}

/* La pista del crucigrama de la palabra que empieza en (x,y) en sentido dir. */
static const char *cz_pista(int x, int y, int dir)
{
    grupo_t *g = &at.g[dir ? G_BAJ : G_ACR];
    if (!g->tem) return NULL;
    int ord = 0;
    if (!dir) { for (int k = 0; k < x; k++) if (cz_letra(k, y) && !cz_letra(k - 1, y) && cz_letra(k + 1, y)) ord++; }
    else { for (int k = 0; k < y; k++) if (cz_letra(x, k) && !cz_letra(x, k - 1) && cz_letra(x, k + 1)) ord++; }
    int linha = dir ? x : y;
    for (int i = 0; i < g->n; i++)
        if (g->c[i].id == linha && g->c[i].item == ord && g->c[i].txt && g->c[i].txt[0]) return g->c[i].txt;
    return NULL;
}

static void pintar_cruzado(void)
{
    grade_geometria(area_w * 55 / 100);
    int s = sp_cw / 18;
    if (s < 1) s = 1;
    for (int y = 0; y < at.tg_lin; y++)
        for (int x = 0; x < at.tg_col; x++) {
            int cx = sp_ox + x * sp_cw, cy = sp_oy + y * sp_ch;
            if (!cz_letra(x, y)) { ret(cx, cy, sp_cw, sp_ch, 0x303030); continue; }
            uint32_t fundo = 0xFFFFFF;
            if (cz_fx >= 0) {   /* la palabra en curso se tiñe */
                int dx = cz_dir ? 0 : 1, dy = cz_dir ? 1 : 0;
                int a = cz_fx, b = cz_fy;
                while (cz_letra(a - dx, b - dy)) { a -= dx; b -= dy; }
                for (; cz_letra(a, b); a += dx, b += dy) if (a == x && b == y) fundo = 0xFFF0A0;
                if (x == cz_fx && y == cz_fy) fundo = 0xFFC060;
            }
            ret(cx, cy, sp_cw, sp_ch, fundo);
            moldura(cx, cy, sp_cw, sp_ch, 1, 0x606060);
            if (cz_num[y][x]) mini_num(cx + 2, cy + 2, cz_num[y][x], 0x2040C0);
            char ch = cz_user[y][x];
            if (ch != ' ') glifo(cx + (sp_cw - 8 * s) / 2, cy + (sp_ch - 16 * s) / 2 + 2, (unsigned char)ch, s, 0x102060, cx, cy, cx + sp_cw, cy + sp_ch);
        }
    /* pistas */
    int lx = sp_ox + sp_cw * at.tg_col + 16, ly = area_y + 8, lw = jw - lx - 8;
    if (lw < 80) return;
    for (int dir = 0; dir < 2; dir++) {
        g_texto(jan, lx, ly, dir ? "Verticales" : "Horizontales", 0x202020);
        ly += 20;
        for (int y = 0; y < at.tg_lin; y++)
            for (int x = 0; x < at.tg_col; x++) {
                if (!cz_num[y][x]) continue;
                bool ini = dir ? (cz_letra(x, y) && !cz_letra(x, y - 1) && cz_letra(x, y + 1))
                               : (cz_letra(x, y) && !cz_letra(x - 1, y) && cz_letra(x + 1, y));
                if (!ini) continue;
                const char *t = cz_pista(x, y, dir);
                char buf[200];
                formatar_texto(buf, sizeof buf, "%d. %s", cz_num[y][x], t ? t : "(sin pista)");
                trecho_t lin[6];
                int nl = quebrar(buf, lw / 8, lin);
                if (nl > 3) nl = 3;
                if (ly + nl * 16 > area_y + area_h - 4) continue;
                bool atual = cz_fx >= 0 && cz_dir == dir;
                if (atual) {   /* resalta la pista de la palabra con el foco */
                    int dx = dir ? 0 : 1, dy = dir ? 1 : 0, a = cz_fx, b = cz_fy;
                    while (cz_letra(a - dx, b - dy)) { a -= dx; b -= dy; }
                    atual = a == x && b == y;
                }
                if (atual) ret(lx - 2, ly - 1, lw + 2, nl * 16 + 2, 0xFFF0A0);
                for (int i = 0; i < nl; i++) {
                    char tmp[120];
                    int n = lin[i].n < 119 ? lin[i].n : 119;
                    memcpy(tmp, lin[i].p, (size_t)n);
                    tmp[n] = 0;
                    g_texto(jan, lx, ly + i * 16, tmp, 0x202020);
                }
                ly += nl * 16 + 4;
            }
        ly += 8;
    }
}

/* ------------------------------------------------------------ pantalla de la actividad */

static botao_t bt_ant, bt_sig, bt_rei, bt_aju, bt_abr;

static void layout_ventana(void)
{
    if (jan) { jw = jan->larg; jh = jan->alt; }   /* sin ventana (--autoprueba) valen los de por defecto */
    area_x = 0;
    area_y = MSG_A;
    area_w = jw;
    area_h = jh - MSG_A - INF_A;
    int y = jh - INF_A + 6;
    bot_pos(&bt_ant, 8, y, 76, 26, "< Atras");
    bot_pos(&bt_sig, 90, y, 90, 26, "Siguiente >");
    bot_pos(&bt_rei, 188, y, 84, 26, "Reiniciar");
    bot_pos(&bt_aju, 278, y, 60, 26, "Ayuda");
    bot_pos(&bt_abr, 344, y, 76, 26, "Lista");
    motor_layout();
    if (at.tipo == T_SOPA) grade_geometria(area_w * 6 / 10);
    else if (at.tipo == T_CRUZ) grade_geometria(area_w * 55 / 100);
}

static void tempo_texto(char *s, int n)
{
    uint32_t t = ((resolvida && t_fim ? t_fim : tempo_ms()) - t_ini) / 1000;
    formatar_texto(s, (size_t)n, "%02u:%02u", t / 60, t % 60);
}

static void pintar_mensagem(void)
{
    int k = (resolvida && at.tem_msg[1]) ? 1 : 0;
    ret(0, 0, jw, MSG_A, at.cor_cont);
    const cel_t *m = at.tem_msg[k] ? &at.msg[k] : NULL;
    if (m) {
        ret(6, 4, jw - 12, MSG_A - 8, m->est.bg);
        moldura(6, 4, jw - 12, MSG_A - 8, 1, 0x404040);
        const char *t = m->txt;
        if (!t || !t[0]) t = k == 1 ? "" : (at.descr && at.descr[0] ? at.descr : "");
        texto_caixa(8, 6, jw - 16, MSG_A - 12, t, m->est.fg, fonte_para(&m->est, 600), 0, 1);
    } else if (at.descr && at.descr[0]) {
        ret(6, 4, jw - 12, MSG_A - 8, 0xFFFFCC);
        texto_caixa(8, 6, jw - 16, MSG_A - 12, at.descr, 0x000000, 1, 0, 1);
    }
}

static void pintar_barra_inferior(void)
{
    ret(0, jh - INF_A, jw, INF_A, at.cor_cont);
    botao_desenhar(jan, &bt_ant);
    botao_desenhar(jan, &bt_sig);
    botao_desenhar(jan, &bt_rei);
    botao_desenhar(jan, &bt_aju);
    botao_desenhar(jan, &bt_abr);
    char s[160], t[16];
    tempo_texto(t, sizeof t);
    int n = 0;
    n += formatar_texto(s + n, sizeof s - (size_t)n, "Act. %d/%d", seq_atual + 1, n_seq);
    if (at.c_pontos) n += formatar_texto(s + n, sizeof s - (size_t)n, "   Aciertos %d", acertos);
    if (at.c_acoes) n += formatar_texto(s + n, sizeof s - (size_t)n, "   Intentos %d", tentativas);
    if (at.c_tempo) n += formatar_texto(s + n, sizeof s - (size_t)n, "   %s", t);
    int sw = g_largura_texto(s);
    int sx = jw - sw - 10;
    if (sx < 430) sx = 430;
    uint32_t fg = (at.cor_cont & 0xFF) + ((at.cor_cont >> 8) & 0xFF) + ((at.cor_cont >> 16) & 0xFF) > 380 ? 0x101010 : 0xF0F0F0;
    g_texto(jan, sx, jh - INF_A + 11, s, fg);
}

static void pintar_final(void)
{
    if (!mostrar_final) return;
    int w = jw - 120 > 560 ? 560 : jw - 120, h = 150;
    int x = (jw - w) / 2, y = area_y + (area_h - h) / 2;
    const cel_t *m = at.tem_msg[1] ? &at.msg[1] : NULL;
    uint32_t bg = m ? m->est.bg : 0xFFFFCC, fg = m ? m->est.fg : 0x006020;
    ret(x + 5, y + 5, w, h, 0x303030);
    ret(x, y, w, h, bg);
    moldura(x, y, w, h, 3, fg);
    int tx = x + 10, tw = w - 20;
    if (m && m->img) {
        imagem_t *im = img_obter(m->img);
        if (im) {
            int ih = h - 20, iw = (int)((long)im->larg * ih / im->alt);
            if (iw > w / 3) { iw = w / 3; ih = (int)((long)im->alt * iw / im->larg); }
            blit(im, 0, 0, im->larg, im->alt, x + 10, y + (h - ih) / 2, iw, ih, x, y, x + w, y + h);
            tx += iw + 10;
            tw -= iw + 10;
        }
    }
    const char *t = m && m->txt && m->txt[0] ? m->txt : "Muy bien!";
    texto_caixa(tx, y + 6, tw, h - 30, t, fg, m ? fonte_para(&m->est, 700) : 3, 1, 1);
    g_texto(jan, x + w - 150, y + h - 20, "(clic para seguir)", fg);
}

static void pintar_nao_suportada(void)
{
    char s[300];
    ret(area_x, area_y, area_w, area_h, at.cor_jan);
    formatar_texto(s, sizeof s, "Esta actividad es de un tipo que todavia no se puede jugar aqui:\n\n%s (%s)\n\nPulsa \"Siguiente\" para saltarla.",
                   nome_do_tipo(at.tipo), at.classe_curta);
    texto_caixa(area_x + 30, area_y + 30, area_w - 60, area_h - 60, s, 0x602020, 2, 1, 1);
}

/* Punto de enganche del sonido de los medios del proyecto (PLAY_AUDIO: WAV, MP3, OGG, MIDI).
 * TODO al fusionar con la síntesis MIDI de la libsagui (rama v7.1-midi, D190): extraer el medio con
 * midia_ler() a un fichero de /tmp y pasárselo al reproductor de audio común (audio_arquivo_abrir y
 * audio_abrir, como musica.c), también para los .mid. Mientras no esté, se salta en silencio y sin
 * error: los efectos de acierto/error/fin son notas propias (efeito_tocar). */
static void tocar_midia(const char *nome)
{
    (void)nome;
}

static void pintar_arrasto(void);
static int arr_p = -1, arr_i = -1, arr_x, arr_y;
static bool arrastando;

static void pintar_jogo(void)
{
    ret(0, 0, jw, jh, at.cor_cont);
    pintar_mensagem();
    ret(area_x, area_y, area_w, area_h, at.cor_jan);
    if (at.tipo == T_SOPA) pintar_sopa();
    else if (at.tipo == T_CRUZ) pintar_cruzado();
    else if (at.tipo == T_TEXTO || at.tipo == T_OUTRO || at.tipo == T_NADA) pintar_nao_suportada();
    else {
        for (int k = 0; k < 2; k++) {
            if (!pn[k].tem) continue;
            for (int i = 0; i < pn[k].n; i++) {
                if (arrastando && arr_p == k && arr_i == i) continue;   /* la pieza que se arrastra se dibuja aparte */
                pintar_caixa(&pn[k], i, k);
            }
        }
        pintar_arrasto();
    }
    pintar_barra_inferior();
    pintar_final();
    jan_mostrar(jan);
}

static void pintar_arrasto(void)
{
    if (!arrastando || arr_p < 0) return;
    pn_t *p = &pn[arr_p];
    cx_t b = p->c[arr_i];
    pn_t tmp = *p;
    b.x = arr_x - b.w / 2;
    b.y = arr_y - b.h / 2;
    tmp.c = &b;
    int sp_ = sel_p;
    sel_p = -1;
    pintar_caixa(&tmp, 0, arr_p);
    moldura(b.x, b.y, b.w, b.h, 2, 0xFF8000);
    sel_p = sp_;
}

/* ------------------------------------------------------------ la lista de proyectos */

#define LISTA_MAX 160
static char itens_nome[LISTA_MAX][64];
static char itens_rota[LISTA_MAX][160];
static int n_itens;
static lista_t lista;
static botao_t bt_abrir, bt_baixar, bt_sair;

static bool eh_projeto(const char *n) { return termina_em(n, ".jclic") || termina_em(n, ".jclic.zip"); }

static void lista_carregar(void)
{
    static const char *const pastas[3] = { "/jogos/jclic", "/home/jclic", "/tmp/jclic" };
    n_itens = 0;
    for (int p = 0; p < 3; p++) {
        entrada_pasta_t *v = NULL;
        int n = listar_tudo(pastas[p], &v);
        for (int i = 0; i < n && n_itens < LISTA_MAX; i++) {
            if (v[i].tipo == TIPO_PASTA || !eh_projeto(v[i].nome)) continue;
            caminho_juntar(itens_rota[n_itens], sizeof itens_rota[0], pastas[p], v[i].nome);
            char nome[64];
            copia_texto(nome, v[i].nome, sizeof nome - 4);
            if (p) {   /* los bajados llevan su .txt con el titulo en la primera linea */
                char t[200];
                copia_texto(t, itens_rota[n_itens], sizeof t);
                int ln = (int)tam_texto(t);
                if (ln > 10 && termina_em(t, ".jclic.zip")) {
                    copia_texto(t + ln - 10, ".txt", 8);
                    int tn = 0;
                    uint8_t *d = carregar_arquivo(t, &tn);
                    if (d) {
                        int k = 0;
                        while (k < tn && k < 58 && d[k] != '\n' && d[k] != '\r') k++;
                        if (k > 0) { memcpy(nome, d, (size_t)k); nome[k] = 0; }
                        libera(d);
                    }
                }
            }
            formatar_texto(itens_nome[n_itens], sizeof itens_nome[0], "%s%s", p ? "+ " : "  ", nome);
            n_itens++;
        }
        if (n > 0) libera(v);
    }
    lista.qtd = n_itens;
    if (lista.sel >= n_itens) lista.sel = n_itens - 1;
    if (lista.sel < 0) lista.sel = 0;
}

static void pintar_lista(void)
{
    jw = jan->larg;
    jh = jan->alt;
    ret(0, 0, jw, jh, COR_FUNDO);
    g_texto(jan, 16, 12, "Actividades JClic", COR_TEXTO);
    g_texto(jan, 16, 32, "Los proyectos de /jogos/jclic vienen con el sistema; los marcados con + se bajaron (/home/jclic o /tmp/jclic).", COR_TEXTO_FRACO);
    lista.x = 16; lista.y = 60; lista.l = jw - 32; lista.a = jh - 60 - 80;
    lista.itens = itens_nome;
    lista_desenhar(jan, &lista);
    if (n_itens == 0) {
        g_texto(jan, 32, 80, "No hay proyectos todavia. Pulsa \"Bajar de Internet\" para elegir del repositorio del XTEC,", COR_TEXTO);
        g_texto(jan, 32, 100, "o abre un .jclic.zip desde el gestor de Archivos.", COR_TEXTO);
    }
    if (aviso_lista[0]) g_texto(jan, 16, jh - 72, aviso_lista, 0xB02020);
    bot_pos(&bt_abrir, 16, jh - 42, 110, 28, "Abrir");
    bot_pos(&bt_baixar, 134, jh - 42, 180, 28, "Bajar de Internet...");
    bot_pos(&bt_sair, jw - 106, jh - 42, 90, 28, "Salir");
    botao_desenhar(jan, &bt_abrir);
    botao_desenhar(jan, &bt_baixar);
    botao_desenhar(jan, &bt_sair);
    jan_mostrar(jan);
}

/* ------------------------------------------------------------ navegación */

static void registrar_estado(const char *por)
{
    registrar("jclic: estado %s act=%d tipo=%d aciertos=%d intentos=%d alvo=%d resuelta=%d sel=%d/%d", por, seq_atual, at.tipo,
              acertos, tentativas, alvo, resolvida ? 1 : 0, sel_p, sel_i);
    for (int k = 0; k < 2; k++) {
        if (!pn[k].tem) continue;
        for (int i = 0; i < pn[k].n && i < 90; i++) {
            cx_t *b = &pn[k].c[i];
            registrar("jclic: caja %d %d x=%d y=%d w=%d h=%d orig=%d est=%d", k, i, b->x, b->y, b->w, b->h, b->orig, b->est);
        }
    }
    if (at.tipo == T_SOPA || at.tipo == T_CRUZ)
        registrar("jclic: grade ox=%d oy=%d cw=%d ch=%d lin=%d col=%d", sp_ox, sp_oy, sp_cw, sp_ch, at.tg_lin, at.tg_col);
}

static void ir_para(int idx)
{
    if (idx < 0 || idx >= n_seq) return;
    seq_atual = idx;
    arrastando = false;
    if (!ativ_ler(seq[idx].ativ, &at)) {
        memset(&at, 0, sizeof at);
        at.cor_cont = at.cor_jan = 0xFFFFFF;
        at.tipo = T_OUTRO;
        copia_texto(at.classe_curta, "actividad no encontrada", sizeof at.classe_curta);
    }
    motor_iniciar();
    layout_ventana();
    registrar("jclic: actividad %d/%d \"%s\" %s (%s)", idx + 1, n_seq, at.nome ? at.nome : "", at.classe_curta, nome_do_tipo(at.tipo));
    registrar_estado("inicio");
    if (jan) pintar_jogo();
}

static int achar_seq(const char *nome)
{
    for (int i = 0; i < n_seq; i++) if (!compara_texto(seq[i].nome, nome) || (seq[i].id[0] && !compara_texto(seq[i].id, nome))) return i;
    for (int i = 0; i < n_seq; i++) if (!compara_texto(seq[i].ativ, nome)) return i;
    return -1;
}

static void adiante(void)
{
    if (seq_atual < 0) return;
    item_t *it = &seq[seq_atual];
    int d = seq_atual + 1;
    if (it->acao_f == 1 && it->salto_f) { int k = achar_seq(it->salto_f); if (k >= 0) d = k; }
    else if (it->acao_f == 2) return;
    if (d < n_seq) ir_para(d);
}

static void atras(void)
{
    if (seq_atual < 0) return;
    item_t *it = &seq[seq_atual];
    int d = seq_atual - 1;
    if (it->acao_a == 1 && it->salto_a) { int k = achar_seq(it->salto_a); if (k >= 0) d = k; }
    else if (it->acao_a == 2) return;
    if (d >= 0) ir_para(d);
}

static bool abrir_projeto(const char *caminho)
{
    int r = projeto_abrir(caminho);
    if (r < 0) {
        formatar_texto(aviso_lista, sizeof aviso_lista, "No se pudo abrir %s: %s", base_nome(caminho),
                       r <= -100 && r > -110 ? comp_nome_erro(r) : nome_erro(r));
        registrar("jclic: %s", aviso_lista);
        projeto_fechar();
        return false;
    }
    aviso_lista[0] = 0;
    registrar("jclic: proyecto \"%s\" (%s): %d actividades en la secuencia", proj_titulo, base_nome(caminho), n_seq);
    if (jan) {
        char t[200], ti[128];
        copia_texto(ti, proj_titulo, sizeof ti);
        formatar_texto(t, sizeof t, "JClic - %s", ti);
        titulo_janela(jan->id, t);
    }
    modo = M_JOGO;
    ir_para(0);
    return true;
}

static void voltar_lista(void)
{
    projeto_fechar();
    cache_limpar();
    modo = M_LISTA;
    seq_atual = -1;
    arrastando = false;
    titulo_janela(jan->id, "Actividades JClic");
    lista_carregar();
    pintar_lista();
}

static void ajuda(void)
{
    char t[900];
    formatar_texto(t, sizeof t, "%s\n\n%s\n\nClic en una pieza y en su destino, o arrastra. N = siguiente, P = anterior, F5 = reiniciar.",
                   at.nome ? at.nome : "", at.descr && at.descr[0] ? at.descr : "(esta actividad no tiene descripcion)");
    t[880] = 0;
    mensagem("Ayuda", t);
}

/* ------------------------------------------------------------ ratón y teclado en el juego */

static void sopa_celda(int x, int y, int *cx, int *cy)
{
    *cx = (x - sp_ox) / sp_cw;
    *cy = (y - sp_oy) / sp_ch;
    if (x < sp_ox) *cx = -1;
    if (y < sp_oy) *cy = -1;
}

static void clique_jogo(int x, int y, int botoes)
{
    (void)botoes;
    if (mostrar_final) { mostrar_final = false; return; }
    if (y < area_y || y >= area_y + area_h) return;
    if (at.tipo == T_SOPA) {
        int cx, cy;
        sopa_celda(x, y, &cx, &cy);
        if (cx >= 0 && cy >= 0 && cx < at.tg_col && cy < at.tg_lin && !resolvida) { sp_x0 = sp_x1 = cx; sp_y0 = sp_y1 = cy; arrastando = true; }
        return;
    }
    if (at.tipo == T_CRUZ) {
        int cx, cy;
        sopa_celda(x, y, &cx, &cy);
        if (cx >= 0 && cy >= 0) cz_foco(cx, cy);
        return;
    }
    for (int k = 1; k >= 0; k--) {
        int i = zona_em(k, x, y);
        if (i >= 0) {
            arr_p = k; arr_i = i; arr_x = x; arr_y = y;
            arrastando = false;
            return;
        }
    }
    arr_p = arr_i = -1;
}

static void soltou_jogo(int x, int y)
{
    if (at.tipo == T_SOPA) {
        if (sp_x0 >= 0) {
            int cx, cy;
            sopa_celda(x, y, &cx, &cy);
            if (cx >= 0 && cy >= 0 && cx < at.tg_col && cy < at.tg_lin) { sp_x1 = cx; sp_y1 = cy; }
            sp_soltar(sp_x0, sp_y0, sp_x1, sp_y1);
        }
        sp_x0 = -1;
        arrastando = false;
        return;
    }
    if (arr_p < 0) { arrastando = false; return; }
    int p0 = arr_p, i0 = arr_i;
    arr_p = arr_i = -1;
    arrastando = false;
    for (int k = 1; k >= 0; k--) {
        int i = zona_em(k, x, y);
        if (i < 0) continue;
        if (k == p0 && i == i0) clique(p0, i0);
        else gesto(p0, i0, k, i);
        return;
    }
}

static void mover_jogo(int x, int y)
{
    if (at.tipo == T_SOPA && sp_x0 >= 0) {
        int cx, cy;
        sopa_celda(x, y, &cx, &cy);
        if (cx >= 0 && cy >= 0 && cx < at.tg_col && cy < at.tg_lin) { sp_x1 = cx; sp_y1 = cy; }
        return;
    }
    if (arr_p >= 0) {
        int dx = x - arr_x, dy = y - arr_y;
        if (arrastando || dx * dx + dy * dy > 36) {
            if (!arrastando && (at.tipo == T_ASOC_S || at.tipo == T_ASOC_C || at.tipo == T_MEMORIA || at.tipo == T_IDENT ||
                                at.tipo == T_EXPLORA || at.tipo == T_INFO)) return;   /* solo los puzles se arrastran con pieza que sigue al ratón */
            arrastando = true;
            arr_x = x;
            arr_y = y;
        }
    }
}

/* ------------------------------------------------------------ autoprueba */

static int autoprueba(const char *arq)
{
    int r = projeto_abrir(arq);
    if (r < 0) {
        registrar("jclic: autoprueba FALLO no se abre %s (%d)", arq, r);
        return 1;
    }
    registrar("jclic: autoprueba proyecto \"%s\" %d actividades", proj_titulo, n_seq);
    int jugadas = 0, resueltas = 0, saltadas = 0, fallos = 0;
    uint32_t soportados = 0;
    for (int i = 0; i < n_seq; i++) {
        if (!ativ_ler(seq[i].ativ, &at)) { registrar("jclic: autoprueba %d no existe la actividad \"%s\"", i, seq[i].ativ); fallos++; continue; }
        seq_atual = i;
        if (at.tipo == T_TEXTO || at.tipo == T_OUTRO) {
            registrar("jclic: autoprueba %d \"%s\" %s: no soportada (se salta)", i + 1, at.nome, at.classe_curta);
            saltadas++;
            continue;
        }
        motor_iniciar();
        layout_ventana();
        /* un gesto equivocado cuenta como intento y no como acierto */
        int t0 = tentativas, a0 = acertos;
        bool prob_err = false;
        if (at.tipo == T_DOBLE && pn[0].n >= 2) {
            for (int k = 0; k < pn[0].n; k++) if (pn[0].c[k].orig != 0) { gesto(0, k, 1, 0); prob_err = true; break; }
        } else if (at.tipo == T_ASOC_S && pn[0].n >= 2 && pn[1].n >= 2) {
            for (int k = 0; k < pn[1].n; k++) if (pn[1].c[k].orig != pn[0].c[0].orig) { gesto(0, 0, 1, k); prob_err = true; break; }
        }
        bool err_bem = !prob_err || (tentativas == t0 + 1 && acertos == a0);
        if (!err_bem) { registrar("jclic: autoprueba FALLO el gesto equivocado cuenta mal (t %d->%d a %d->%d)", t0, tentativas, a0, acertos); fallos++; }
        auto_resolver();
        bool se_resolve = tipo_se_resolve(at.tipo);
        bool ok = !se_resolve || resolvida;
        registrar("jclic: autoprueba %d \"%s\" %s tipo=%d celdas=%d aciertos=%d intentos=%d %s", i + 1, at.nome, at.classe_curta, at.tipo,
                  pn[0].tem ? pn[0].n : at.tg_lin * at.tg_col, acertos, tentativas, ok ? "ok" : "FALLO no se resolvio");
        jugadas++;
        if (se_resolve && resolvida) resueltas++;
        if (!ok) fallos++;
        soportados |= 1u << at.tipo;
    }
    registrar("jclic: autoprueba resumen jugadas=%d resueltas=%d saltadas=%d fallos=%d tipos=0x%x", jugadas, resueltas, saltadas, fallos, soportados);
    registrar("jclic: autoprueba %s", fallos == 0 ? "ok" : "FALLO");
    projeto_fechar();
    cache_limpar();
    return fallos ? 1 : 0;
}

/* ------------------------------------------------------------ principal */

static void bajar_de_internet(void)
{
    char *av[] = { "jclicbajar", NULL };
    int pid = executar("/apps/bin/jclicbajar", av);
    if (pid < 0) formatar_texto(aviso_lista, sizeof aviso_lista, "No se pudo abrir el descargador (/apps/bin/jclicbajar): %s", nome_erro(pid));
}

static void tratar_lista(const evento_t *ev)
{
    int r = lista_tratar(jan, &lista, ev);
    if (r == LISTA_ATIVOU && lista.sel >= 0 && lista.sel < n_itens) { abrir_projeto(itens_rota[lista.sel]); if (modo == M_LISTA) pintar_lista(); return; }
    if (botao_tratar(jan, &bt_abrir, ev) && lista.sel >= 0 && lista.sel < n_itens) { abrir_projeto(itens_rota[lista.sel]); if (modo == M_LISTA) pintar_lista(); return; }
    if (botao_tratar(jan, &bt_baixar, ev)) { bajar_de_internet(); pintar_lista(); return; }
    if (botao_tratar(jan, &bt_sair, ev)) { quer_sair = true; return; }
    if (ev->tipo == EV_TECLA && ev->tecla == TECLA_F11) return;
    if (ev->tipo == EV_TECLA && ev->tecla == TECLA_ESC) { quer_sair = true; return; }
    if (ev->tipo == EV_CLIQUE || ev->tipo == EV_TECLA || ev->tipo == EV_RODA || ev->tipo == EV_SOLTOU) pintar_lista();
}

static void tratar_jogo(const evento_t *ev)
{
    bool redesenhar = false;
    if (botao_tratar(jan, &bt_sig, ev)) { adiante(); return; }
    if (botao_tratar(jan, &bt_ant, ev)) { atras(); return; }
    if (botao_tratar(jan, &bt_rei, ev)) { ir_para(seq_atual); return; }
    if (botao_tratar(jan, &bt_aju, ev)) { ajuda(); redesenhar = true; }
    if (botao_tratar(jan, &bt_abr, ev)) { voltar_lista(); return; }
    switch (ev->tipo) {
    case EV_CLIQUE: if (ev->botoes & 1) { clique_jogo(ev->x, ev->y, (int)ev->botoes); redesenhar = true; } break;
    case EV_MOVER: if (ev->botoes & 1) { mover_jogo(ev->x, ev->y); redesenhar = arrastando || sp_x0 >= 0; } break;
    case EV_SOLTOU: soltou_jogo(ev->x, ev->y); redesenhar = true; registrar_estado("soltar"); break;
    case EV_TECLA: {
        unsigned t = ev->tecla;
        if (t == TECLA_ESC) { voltar_lista(); return; }
        if (t == 0x10A) { ajuda(); pintar_jogo(); return; }   /* F1 */
        if (at.tipo == T_CRUZ && ((t >= 32 && t < 256) || t == 8)) { cz_escrever((int)t); redesenhar = true; registrar_estado("tecla"); break; }
        if (at.tipo == T_CRUZ && (t == TECLA_DIREITA || t == TECLA_ESQUERDA || t == TECLA_CIMA || t == TECLA_BAIXO) && cz_fx >= 0) {
            int dx = t == TECLA_DIREITA ? 1 : t == TECLA_ESQUERDA ? -1 : 0, dy = t == TECLA_BAIXO ? 1 : t == TECLA_CIMA ? -1 : 0;
            if (cz_letra(cz_fx + dx, cz_fy + dy)) { cz_fx += dx; cz_fy += dy; cz_dir = dx ? 0 : 1; }
            redesenhar = true;
            break;
        }
        if (t == 'n' || t == 'N' || t == TECLA_DIREITA || t == TECLA_PGDN) { adiante(); return; }
        if (t == 'p' || t == 'P' || t == TECLA_ESQUERDA || t == TECLA_PGUP) { atras(); return; }
        if (t == 0x10E /* F5 */ || t == 'r' || t == 'R') { ir_para(seq_atual); return; }
        break;
    }
    default: break;
    }
    if (som_pedido) { tocar_midia(som_pedido); som_pedido = NULL; }
    if (salto_pedido) {
        int k = achar_seq(salto_pedido);
        salto_pedido = NULL;
        if (k >= 0) { ir_para(k); return; }
    }
    if (redesenhar) pintar_jogo();
}

int principal(int argc, char **argv)
{
    const char *arq = NULL;
    bool autop = false;
    uint32_t sem = 0;
    for (int k = 1; k < argc; k++) {
        if (!compara_texto(argv[k], "--autoprueba")) autop = true;
        else if (!compara_texto(argv[k], "--semilla") && k + 1 < argc) sem = (uint32_t)texto_para_int(argv[++k]);
        else if (argv[k][0] == '-') { imprimir("uso: jclic [proyecto.jclic.zip | proyecto.jclic] | --autoprueba proyecto [--semilla N]\n"); return 2; }
        else arq = argv[k];
    }
    if (sem == 0) { uint32_t b = 0; bytes_aleatorios(&b, sizeof b); sem = (b ^ tempo_ms()) | 1; }
    rng_estado = sem;
    if (autop) {
        if (!arq) { imprimir("jclic: falta el proyecto\n"); return 2; }
        return autoprueba(arq);
    }
    jan = jan_abrir("Actividades JClic", LARG_INICIO, ALT_INICIO);
    if (!jan) { registrar("jclic: no se ha abierto la ventana"); return 1; }
    jan_estilo(jan, JAN_REDIMENSIONAVEL, 640, 480);
    jpx = jan->px;
    jw = jan->larg;
    jh = jan->alt;
    memset(&lista, 0, sizeof lista);
    lista_carregar();
    if (arq) {
        if (!abrir_projeto(arq)) { modo = M_LISTA; pintar_lista(); }
    } else pintar_lista();
    registrar("jclic: pronto");
    evento_t ev;
    int seg_visto = -1;
    while (!quer_sair) {
        int r = esperar_evento(jan->id, &ev, modo == M_JOGO ? 200 : -1);
        if (r == 1) {
            do {
                if (ev.tipo == EV_FECHAR) { quer_sair = true; break; }
                if (ev.tipo == EV_REDIMENSIONAR) {
                    if (jan_redimensionar(jan, &ev)) {
                        jpx = jan->px;
                        jw = jan->larg;
                        jh = jan->alt;
                        if (modo == M_JOGO) { layout_ventana(); pintar_jogo(); } else pintar_lista();
                    }
                    continue;
                }
                if (modo == M_LISTA) tratar_lista(&ev); else tratar_jogo(&ev);
            } while (!quer_sair && proximo_evento(jan->id, &ev, false) == 1);
            if (jpx != jan->px) jpx = jan->px;
        } else if (modo == M_JOGO) {
            bool mudou = motor_tempo();
            int seg = (int)((tempo_ms() - t_ini) / 1000);
            if (!resolvida && seg != seg_visto) { seg_visto = seg; mudou = true; }
            if (mudou) pintar_jogo();
        }
    }
    registrar("jclic: cerrado");
    projeto_fechar();
    cache_limpar();
    jan_fechar(jan);
    return 0;
}
