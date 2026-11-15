// SPDX-License-Identifier: GPL-2.0-or-later
/* visorpdf [arquivo] - o Visor de PDF do SaguiOS (melhora 059, D138; interfaz de
 * escritorio completa en la mejora 105, D274).
 *
 * Abre un PDF y muestra sus páginas en una ventana con desplazamiento continuo.
 * El motor (userland/pdf/) dibuja cada página en un búfer RGBA; el programa la
 * mezcla sobre blanco, la gira si hace falta y la guarda en una caché pequeña,
 * de modo que desplazarse y volver no repinta.
 *
 * Interfaz:
 *   - barra de herramientas con iconos: abrir, exportar la página a imagen,
 *     anterior/siguiente, «página N de M» (clic = ir a...), zoom -/+, ajustar al
 *     ancho / a la página, porcentaje (clic = escribirlo), girar, buscar, panel
 *     lateral y presentación;
 *   - panel lateral plegable con las miniaturas (se dibujan en segundo plano, una
 *     por vuelta del bucle de eventos, y solo las que se ven) y el índice del PDF;
 *   - búsqueda con Ctrl+F: resalta todas las coincidencias y salta entre ellas
 *     (sin distinguir mayúsculas ni acentos); el escaneo va por páginas en los
 *     ratos libres, sin bloquear la ventana;
 *   - se recuerda la página y el zoom de cada documento (/home/.config/visorpdf.conf);
 *   - contraseña de los PDF cifrados con un diálogo propio (el texto va con asteriscos).
 *
 * Teclas:
 *   <- ->        página anterior y siguiente         Arriba Abajo   desplazar
 *   RePág AvPág  desplazar una pantalla               Espacio        AvPág
 *   Inicio Fin   primera y última página
 *   + -          zoom (con Ctrl y la rueda también)   0  ajustar a la página
 *   w            ajustar al ancho                     1  tamaño real (100%)
 *   r            girar 90 grados                      F4 panel lateral
 *   F5           presentación (pantalla completa)     F11 pantalla completa
 *   Ctrl+O abrir   Ctrl+F buscar   Ctrl+G ir a página   Ctrl+S exportar página
 *   F3 / Mayús+F3 coincidencia siguiente / anterior   Esc  cierra la búsqueda
 *
 * Modos para las pruebas (sin ventana):
 *   visorpdf -t arquivo              abre el PDF y dice cuántas páginas tiene,
 *                                     el título y el tamaño
 *   visorpdf --ppm arquivo saida.ppm [página [zoom [palavra-passe]]]
 *                                     dibuja la página y graba un PPM (P6)
 *   visorpdf --indice arquivo [palavra-passe]
 *                                     lista el índice (nivel, página, título)
 *   visorpdf --buscar arquivo texto [palavra-passe]
 *                                     cuenta las coincidencias por página
 *
 * El arrastrar y soltar un PDF sobre la ventana no existe todavía en SaguiOS (el
 * gestor de ventanas no envía ningún evento de archivo soltado): de momento se
 * abre desde Archivos con «Abrir con», con Ctrl+O o con el argumento. */
#include "sagui.h"
#include "gui.h"
#include "icones.h"
#include "pdf.h"

#define LARG_INICIO 900
#define ALT_INICIO  700
#define LARG_MIN 600
#define ALT_MIN  380
#define BARRA      34
#define BUSCA_H    30
#define RODAPE     24
#define PAINEL_L   180
#define ABA_H      22
#define SB_L       12          /* ancho de la barra de desplazamiento */
#define MARGEM     10
#define GAP        10
#define ZOOM_MIN   10
#define ZOOM_MAX   400
#define NCACHE     24
#define CACHE_MAX  (64u * 1024 * 1024)
#define RENDER_MAX (32u * 1024 * 1024)
#define MAX_MINIATURAS 60
#define MAX_ACERTOS 4000
#define MAX_RECTS   8000
#define PASSO_ROLAR 48

#define TECLA_F3 0x10C
#define TECLA_F4 0x10D
#define TECLA_F5 0x10E

/* ------------------------------------------------------------ estado */

static jan_t *j;
static pdf_t *doc;
static char caminho[256];
static int  pagina = 0;
static int  n_paginas = 0;
static int  zoom = 100;          /* el zoom efectivo, en % */
static int  modo_aj = 0;         /* 0 = el número, 1 = ajustar al ancho, 2 = a la página */
static int  rot = 0;             /* giro del usuario: 0, 90, 180, 270 */
static int  desl_x, desl_y;      /* desplazamiento en píxeles del documento */
static bool tela_cheia;
static bool apres;               /* presentación */
static bool painel_ver = true;
static int  painel_aba;          /* 0 = miniaturas, 1 = índice */
static char aviso[160];
static bool sujo;                /* hay que repintar */
static unsigned quadro;          /* contador de repintados (para la caché) */

/* Tamaño natural (en puntos, con el /Rotate propio de la página) y su lugar en
 * el documento, para el zoom y giro actuales. */
static int *pg_pl, *pg_pa;       /* tamaño natural */
static int *lw, *lh, *ytop;      /* tamaño en pantalla (ya girado) y posición vertical */
static int doc_l, doc_a;         /* tamaño total del documento en pantalla */

/* La caché de páginas dibujadas (0x00RRGGBB, ya sobre blanco y giradas). */
typedef struct { int pag, z, r, l, a; uint32_t *px; unsigned uso; } cache_t;
static cache_t cache[NCACHE];
static size_t cache_bytes;

/* Miniaturas. */
typedef struct { uint32_t *px; int l, a; unsigned uso; } mini_t;
static mini_t *minis;
static int n_minis;
static int painel_desl;          /* desplazamiento del panel (px) */

/* El índice (marcadores). */
static pdf_marca_t *marcas;
static int n_marcas;

/* Búsqueda. */
typedef struct { int pag, x, y, l, a, m; } rect_t;       /* en décimas de punto, sin girar */
static bool busca_ver;
static caixa_t busca_caixa;
static char busca_txt[128];
static int  busca_q[128], busca_nq;
static int  busca_prox;          /* próxima página que escanear (>= n_paginas = terminó) */
static int  busca_origem;        /* página desde la que se pidió */
static bool busca_pend;          /* saltar a la primera coincidencia en cuanto salga */
static int  n_acertos, acerto_atual;
static int  acerto_pag[MAX_ACERTOS];
static rect_t *rects;
static int  n_rects;

/* Para arrastrar con el ratón. */
static int arr_modo, arr_x, arr_y, arr_dx, arr_dy;
static bool mini_feito;      /* no queda ninguna miniatura visible por dibujar */
static bool dica_ativa;      /* el aviso del pie es la ayuda de un botón */

/* Recordar página y zoom. */
static bool conf_sujo;
static unsigned conf_t;

/* Botones de la barra de herramientas. */
enum { B_ABRIR, B_EXPORTAR, B_ANT, B_PAGINA, B_PROX, B_ZMENOS, B_ZOOM, B_ZMAIS, B_AJ_LARG, B_AJ_PAG,
       B_GIRAR, B_BUSCAR, B_PAINEL, B_APRES, B_QTD };
typedef struct { int x, l; } tb_t;
static tb_t tb[B_QTD];
/* Los iconos de la biblioteca (NULL = se dibuja a mano). */
static const char *const ICONE_BT[B_QTD] = {
    "abrir", "imagem", "voltar", NULL, "avancar", NULL, NULL, NULL, NULL, NULL,
    "sincronizar", "procurar", "lista", "janela" };
static const char *const DICA_BT[B_QTD] = {
    "Abrir (Ctrl+O)", "Exportar la página a imagen (Ctrl+S)", "Página anterior", "Ir a la página (Ctrl+G)",
    "Página siguiente", "Reducir", "Escribir el zoom", "Ampliar", "Ajustar al ancho (w)",
    "Ajustar a la página (0)", "Girar (r)", "Buscar (Ctrl+F)", "Panel lateral (F4)", "Presentación (F5)" };

/* ------------------------------------------------------------ geometría */

static int barra_h(void) { return apres ? 0 : BARRA; }
static int busca_h(void) { return (!apres && busca_ver) ? BUSCA_H : 0; }
static int rodape_h(void) { return apres ? 0 : RODAPE; }
static int painel_larg(void) { return (!apres && painel_ver) ? PAINEL_L : 0; }
static int sb_larg(void) { return apres ? 0 : SB_L; }

/* El pedazo de la ventana donde se ve el documento. */
static void area_pagina(int *x, int *y, int *l, int *a)
{
    *x = painel_larg();
    *y = barra_h() + busca_h();
    *l = j->larg - *x - sb_larg();
    *a = j->alt - *y - rodape_h();
    if (*l < 1) *l = 1;
    if (*a < 1) *a = 1;
}

static bool girado(void) { return rot == 90 || rot == 270; }

/* El tamaño natural de la página p en píxeles con el zoom actual. */
static void dim_nat(int p, int z, int *nw, int *nh)
{
    int w = pg_pl[p] * z / 100, h = pg_pa[p] * z / 100;
    if (w < 1) w = 1;
    if (h < 1) h = 1;
    *nw = w;
    *nh = h;
}

/* Recalcula dónde cae cada página con el zoom y el giro actuales. */
static void recalcular_layout(void)
{
    if (!doc) return;
    int x0, y0, vl, va;
    area_pagina(&x0, &y0, &vl, &va);
    int gap = apres ? va : GAP;
    int y = apres ? 0 : MARGEM;
    int maxl = 0;
    for (int p = 0; p < n_paginas; p++) {
        int nw, nh;
        dim_nat(p, zoom, &nw, &nh);
        lw[p] = girado() ? nh : nw;
        lh[p] = girado() ? nw : nh;
        ytop[p] = y;
        y += lh[p] + gap;
        if (lw[p] > maxl) maxl = lw[p];
    }
    doc_l = maxl + (apres ? 0 : 2 * MARGEM);
    doc_a = y - gap + (apres ? 0 : MARGEM);
    int mx = doc_a - va, my = doc_l - vl;
    if (desl_y > mx) desl_y = mx;
    if (desl_y < 0) desl_y = 0;
    if (desl_x > my) desl_x = my;
    if (desl_x < 0) desl_x = 0;
}

/* El zoom que corresponde al modo de ajuste, para la página p. */
static void zoom_de_ajuste(void)
{
    if (!doc || modo_aj == 0) return;
    int x0, y0, vl, va;
    area_pagina(&x0, &y0, &vl, &va);
    int pw = girado() ? pg_pa[pagina] : pg_pl[pagina];
    int ph = girado() ? pg_pl[pagina] : pg_pa[pagina];
    int m = apres ? 8 : MARGEM;
    if (pw <= 0 || ph <= 0) return;
    int z = (vl - 2 * m) * 100 / pw;
    if (modo_aj == 2) {
        int z2 = (va - 2 * m) * 100 / ph;
        if (z2 < z) z = z2;
    }
    if (z < ZOOM_MIN) z = ZOOM_MIN;
    if (z > ZOOM_MAX) z = ZOOM_MAX;
    zoom = z;
}

/* La página que ocupa la parte de arriba de la ventana. */
static int pagina_en(int y_doc)
{
    int p = 0;
    for (int i = 0; i < n_paginas; i++)
        if (ytop[i] <= y_doc) p = i;
    return p;
}

static void actualizar_pagina_atual(void)
{
    int x0, y0, vl, va;
    area_pagina(&x0, &y0, &vl, &va);
    pagina = pagina_en(desl_y + va / 3);
}

/* ------------------------------------------------------------ la caché de páginas */

static void cache_liberar(cache_t *c)
{
    if (c->px) libera(c->px);
    if (c->px) cache_bytes -= (size_t)c->l * (size_t)c->a * 4;
    memset(c, 0, sizeof *c);
}

static void cache_limpar(void)
{
    for (int i = 0; i < NCACHE; i++) cache_liberar(&cache[i]);
}

/* La página p dibujada (con el zoom y el giro actuales), o NULL. */
static uint32_t *obter_render(int p)
{
    for (int i = 0; i < NCACHE; i++)
        if (cache[i].px && cache[i].pag == p && cache[i].z == zoom && cache[i].r == rot) {
            cache[i].uso = quadro;
            return cache[i].px;
        }
    int nw, nh;
    dim_nat(p, zoom, &nw, &nh);
    size_t bytes = (size_t)nw * (size_t)nh * 4;
    if (bytes > RENDER_MAX) {
        formatar_texto(aviso, sizeof aviso, "Página demasiado grande para este zoom");
        return NULL;
    }
    /* Sitio: se echa a la menos usada hasta que quepa. */
    for (;;) {
        int libre = -1, viejo = -1;
        for (int i = 0; i < NCACHE; i++) {
            if (!cache[i].px) { if (libre < 0) libre = i; continue; }
            if (viejo < 0 || cache[i].uso < cache[viejo].uso) viejo = i;
        }
        if (libre >= 0 && cache_bytes + bytes <= CACHE_MAX) break;
        if (viejo < 0) break;
        cache_liberar(&cache[viejo]);
    }
    int idx = -1;
    for (int i = 0; i < NCACHE; i++) if (!cache[i].px) { idx = i; break; }
    if (idx < 0) return NULL;
    uint8_t *tmp = aloca(bytes);
    uint32_t *fin = aloca(bytes);
    if (!tmp || !fin) {
        if (tmp) libera(tmp);
        if (fin) libera(fin);
        formatar_texto(aviso, sizeof aviso, "Sin memoria para dibujar la página");
        return NULL;
    }
    memset(tmp, 0, bytes);
    int r = pdf_render(doc, p, nw, nh, tmp);
    if (r != 0) formatar_texto(aviso, sizeof aviso, "Error al dibujar: %s", pdf_erro_texto(r));
    /* Mezcla sobre blanco y gira. */
    for (int y = 0; y < nh; y++) {
        const uint8_t *s = tmp + (size_t)y * nw * 4;
        for (int x = 0; x < nw; x++, s += 4) {
            uint32_t a = s[3];
            uint32_t rr = (s[0] * a + 255 * (255 - a)) / 255;
            uint32_t gg = (s[1] * a + 255 * (255 - a)) / 255;
            uint32_t bb = (s[2] * a + 255 * (255 - a)) / 255;
            uint32_t v = (rr << 16) | (gg << 8) | bb;
            size_t d;
            if (rot == 0) d = (size_t)y * nw + x;
            else if (rot == 90) d = (size_t)x * nh + (nh - 1 - y);
            else if (rot == 180) d = (size_t)(nh - 1 - y) * nw + (nw - 1 - x);
            else d = (size_t)(nw - 1 - x) * nh + y;
            fin[d] = v;
        }
    }
    libera(tmp);
    cache[idx].pag = p; cache[idx].z = zoom; cache[idx].r = rot;
    cache[idx].l = girado() ? nh : nw;
    cache[idx].a = girado() ? nw : nh;
    cache[idx].px = fin;
    cache[idx].uso = quadro;
    cache_bytes += bytes;
    return fin;
}

/* ------------------------------------------------------------ texto y búsqueda */

/* Pasa un carácter a minúscula sin acento (Latin-1) para comparar. */
static int dobrar(int c)
{
    static const char tabla[] = "aaaaaaaceeeeiiiidnooooooouuuuyts";
    if (c >= 'A' && c <= 'Z') return c + 32;
    if (c >= 0xC0 && c <= 0xDF) return tabla[c - 0xC0];
    if (c >= 0xE0 && c <= 0xFE) return tabla[c - 0xE0];
    if (c == 0xFF) return 'y';
    if (c == 0xFB01) return 'f';
    return c;
}

static void busca_limpar(void)
{
    n_acertos = 0;
    acerto_atual = -1;
    n_rects = 0;
    busca_prox = n_paginas;
    busca_pend = false;
}

/* Busca q en la página p y apunta las coincidencias y sus rectángulos. */
static int buscar_em_pagina(int p, const int *q, int nq)
{
    pdf_car_t *cars = NULL;
    int n = 0;
    if (pdf_texto_pagina(doc, p, &cars, &n) != 0 || !cars || n <= 0) {
        if (cars) libera(cars);
        return 0;
    }
    int achou = 0;
    int *fold = aloca((size_t)n * sizeof(int));
    if (!fold) { libera(cars); return 0; }
    for (int i = 0; i < n; i++) fold[i] = dobrar(cars[i].uni);
    for (int i = 0; i + nq <= n; i++) {
        int k = 0;
        while (k < nq && fold[i + k] == q[k]) k++;
        if (k < nq) continue;
        if (n_acertos >= MAX_ACERTOS) break;
        int m = n_acertos++;
        acerto_pag[m] = p;
        achou++;
        /* Un rectángulo por línea de la coincidencia. */
        int ini = -1;
        rect_t cur;
        for (int c = i; c < i + nq; c++) {
            if (cars[c].l <= 0) continue;
            if (ini >= 0 && (cars[c].y - cur.y > cur.a / 2 || cur.y - cars[c].y > cur.a / 2)) {
                if (n_rects < MAX_RECTS && rects) rects[n_rects++] = cur;
                ini = -1;
            }
            if (ini < 0) {
                cur.pag = p; cur.x = cars[c].x; cur.y = cars[c].y; cur.l = cars[c].l; cur.a = cars[c].a; cur.m = m;
                ini = c;
            } else {
                int fx = cars[c].x + cars[c].l;
                if (cars[c].x < cur.x) { cur.l += cur.x - cars[c].x; cur.x = cars[c].x; }
                if (fx > cur.x + cur.l) cur.l = fx - cur.x;
                if (cars[c].a > cur.a) cur.a = cars[c].a;
            }
        }
        if (ini >= 0 && n_rects < MAX_RECTS && rects) rects[n_rects++] = cur;
        i += nq - 1;
    }
    libera(fold);
    libera(cars);
    return achou;
}

/* Convierte el texto del cuadro a la consulta plegada. */
static void busca_preparar(void)
{
    busca_nq = 0;
    for (int i = 0; busca_txt[i] && busca_nq < 127; i++) busca_q[busca_nq++] = dobrar((unsigned char)busca_txt[i]);
}

static void busca_iniciar(void)
{
    copia_texto(busca_txt, busca_caixa.texto, sizeof busca_txt);
    busca_preparar();
    busca_limpar();
    if (busca_nq == 0 || !doc) return;
    busca_prox = 0;
    busca_origem = pagina;
    busca_pend = true;
}

/* Centra el documento en la coincidencia m. */
static void ir_acerto(int m)
{
    if (m < 0 || m >= n_acertos) return;
    acerto_atual = m;
    int p = acerto_pag[m];
    for (int i = 0; i < n_rects; i++) {
        if (rects[i].m != m) continue;
        int x0, y0, vl, va;
        area_pagina(&x0, &y0, &vl, &va);
        int nw, nh;
        dim_nat(p, zoom, &nw, &nh);
        int ux = rects[i].x * zoom / 1000, uy = rects[i].y * zoom / 1000;
        int ul = rects[i].l * zoom / 1000, ua = rects[i].a * zoom / 1000;
        int rx, ry;
        /* El punto de arriba a la izquierda del rectángulo, girado. */
        if (rot == 0) { rx = ux; ry = uy; }
        else if (rot == 90) { rx = nh - uy - ua; ry = ux; }
        else if (rot == 180) { rx = nw - ux - ul; ry = nh - uy - ua; }
        else { rx = uy; ry = nw - ux - ul; }
        desl_y = ytop[p] + ry - va / 3;
        int px0 = (doc_l <= vl) ? 0 : (doc_l - lw[p]) / 2;
        if (doc_l > vl) desl_x = px0 + rx - vl / 2;
        break;
    }
    recalcular_layout();   /* recorta el desplazamiento */
    actualizar_pagina_atual();
    sujo = true;
}

/* Un paso de la búsqueda en los ratos libres. true = hizo algo. */
static bool busca_passo(void)
{
    if (busca_prox >= n_paginas || busca_nq == 0) return false;
    unsigned t0 = tempo_ms();
    do {
        int p = busca_prox++;
        int antes = n_acertos;
        buscar_em_pagina(p, busca_q, busca_nq);
        if (busca_pend && n_acertos > antes && p >= busca_origem) {
            busca_pend = false;
            ir_acerto(antes);
        }
    } while (busca_prox < n_paginas && tempo_ms() - t0 < 40);
    if (busca_prox >= n_paginas) {
        if (busca_pend && n_acertos > 0) { busca_pend = false; ir_acerto(0); }
        imprimir_em(SAIDA, "visorpdf: busca '%s': %d resultados\n", busca_txt, n_acertos);
    }
    sujo = true;
    return true;
}

static void acerto_mover(int d)
{
    if (n_acertos <= 0) return;
    int m = acerto_atual < 0 ? (d > 0 ? 0 : n_acertos - 1) : (acerto_atual + d + n_acertos) % n_acertos;
    ir_acerto(m);
}

/* ------------------------------------------------------------ persistencia (página y zoom) */

#define CONF_ARQ "/home/.config/visorpdf.conf"
#define CONF_MAX 40

/* Una línea: pagina<TAB>zoom<TAB>modo<TAB>giro<TAB>camino */
static bool conf_ler(const char *cam, int *pag, int *z, int *modo, int *giro)
{
    int fd = abrir(CONF_ARQ, ABRIR_LER);
    if (fd < 0) return false;
    static char buf[12288];
    int n = ler(fd, buf, sizeof buf - 1);
    fechar(fd);
    if (n <= 0) return false;
    buf[n] = 0;
    for (char *l = buf; *l;) {
        char *fim = l;
        while (*fim && *fim != '\n') fim++;
        char sv = *fim;
        *fim = 0;
        int v[4] = {0, 0, 0, 0}, k = 0;
        char *s = l;
        while (k < 4) {
            v[k++] = texto_para_int(s);
            while (*s && *s != '\t') s++;
            if (*s != '\t') break;
            s++;
        }
        if (k == 4 && compara_texto(s, cam) == 0) {
            *pag = v[0]; *z = v[1]; *modo = v[2]; *giro = v[3];
            return true;
        }
        *fim = sv;
        if (!sv) break;
        l = fim + 1;
    }
    return false;
}

static void conf_gravar(void)
{
    if (!doc || !caminho[0]) return;
    static char antiguo[12288];
    static char nuevo[12800];
    int n = 0;
    int fd = abrir(CONF_ARQ, ABRIR_LER);
    if (fd >= 0) {
        n = ler(fd, antiguo, sizeof antiguo - 1);
        fechar(fd);
        if (n < 0) n = 0;
    }
    antiguo[n] = 0;
    int w = formatar_texto(nuevo, sizeof nuevo, "%d\t%d\t%d\t%d\t%s\n", pagina, zoom, modo_aj, rot, caminho);
    int linhas = 1;
    for (char *l = antiguo; *l && linhas < CONF_MAX;) {
        char *fim = l;
        while (*fim && *fim != '\n') fim++;
        char *cam = l;
        for (int t = 0; t < 4 && *cam; t++) { while (*cam && *cam != '\t') cam++; if (*cam) cam++; }
        int len = (int)(fim - l);
        char sv = *fim;
        *fim = 0;
        bool igual = compara_texto(cam, caminho) == 0;
        *fim = sv;
        if (!igual && len > 0 && w + len + 1 < (int)sizeof nuevo) {
            memcpy(nuevo + w, l, (size_t)len);
            w += len;
            nuevo[w++] = '\n';
            linhas++;
        }
        if (!sv) break;
        l = fim + 1;
    }
    criar_pasta("/home/.config");
    fd = abrir(CONF_ARQ, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return;
    escrever_tudo(fd, nuevo, (unsigned)w);
    fechar(fd);
    conf_sujo = false;
}

static void conf_marcar(void)
{
    conf_sujo = true;
    conf_t = tempo_ms();
}

/* ------------------------------------------------------------ miniaturas e índice */

static int mini_larg(void) { return PAINEL_L - 28; }

static int mini_alt(int p)
{
    int tw = mini_larg();
    int th = pg_pl[p] > 0 ? tw * pg_pa[p] / pg_pl[p] : tw;
    if (th < 20) th = 20;
    if (th > tw * 3) th = tw * 3;
    return th;
}

static void minis_limpar(void)
{
    if (minis) {
        for (int p = 0; p < n_paginas; p++) if (minis[p].px) libera(minis[p].px);
        libera(minis);
    }
    minis = NULL;
    n_minis = 0;
}

static void marcas_limpar(void)
{
    if (marcas) libera(marcas);
    marcas = NULL;
    n_marcas = 0;
}

/* El área del panel (debajo de las pestañas). */
static void area_painel(int *y, int *a)
{
    *y = barra_h() + busca_h() + ABA_H;
    *a = j->alt - *y - rodape_h();
    if (*a < 1) *a = 1;
}

/* Dibuja una miniatura que falte de las que se ven. true = dibujó una. */
static bool miniatura_passo(void)
{
    if (!doc || apres || !painel_ver || painel_aba != 0 || !minis) return false;
    int py, pa;
    area_painel(&py, &pa);
    int y = 6;
    for (int p = 0; p < n_paginas; p++) {
        int ch = mini_alt(p) + 24;
        if (y + ch > painel_desl && y < painel_desl + pa && !minis[p].px) {
            int tw = mini_larg(), th = mini_alt(p);
            uint8_t *tmp = aloca((size_t)tw * th * 4);
            uint32_t *fin = aloca((size_t)tw * th * 4);
            if (!tmp || !fin) {
                if (tmp) libera(tmp);
                if (fin) libera(fin);
                return false;
            }
            memset(tmp, 0, (size_t)tw * th * 4);
            pdf_render(doc, p, tw, th, tmp);
            for (int i = 0; i < tw * th; i++) {
                uint32_t a = tmp[i * 4 + 3];
                uint32_t r = (tmp[i * 4] * a + 255 * (255 - a)) / 255;
                uint32_t g = (tmp[i * 4 + 1] * a + 255 * (255 - a)) / 255;
                uint32_t b = (tmp[i * 4 + 2] * a + 255 * (255 - a)) / 255;
                fin[i] = (r << 16) | (g << 8) | b;
            }
            libera(tmp);
            /* Tope de miniaturas residentes: se suelta la más lejana a la página actual. */
            if (n_minis >= MAX_MINIATURAS) {
                int lejos = -1, dmax = -1;
                for (int k = 0; k < n_paginas; k++) {
                    if (!minis[k].px) continue;
                    int d = k > pagina ? k - pagina : pagina - k;
                    if (d > dmax) { dmax = d; lejos = k; }
                }
                if (lejos >= 0) { libera(minis[lejos].px); minis[lejos].px = NULL; n_minis--; }
            }
            minis[p].px = fin;
            minis[p].l = tw;
            minis[p].a = th;
            n_minis++;
            imprimir_em(SAIDA, "visorpdf: miniatura %d\n", p + 1);
            sujo = true;
            return true;
        }
        y += ch;
    }
    mini_feito = true;
    return false;
}

/* UTF-8 a Latin-1, en el sitio (los títulos del índice). */
static void titulo_latin1(char *s)
{
    char *d = s;
    while (*s) {
        unsigned char c = (unsigned char)*s;
        if (c >= 0xC0 && c < 0xE0 && ((unsigned char)s[1] & 0xC0) == 0x80) {
            int cp = ((c & 0x1F) << 6) | ((unsigned char)s[1] & 0x3F);
            *d++ = cp < 256 ? (char)cp : '?';
            s += 2;
        } else if (c >= 0xE0) {
            *d++ = '?';
            s++;
            while (((unsigned char)*s & 0xC0) == 0x80) s++;
        } else {
            *d++ = (char)c;
            s++;
        }
    }
    *d = 0;
}

/* ------------------------------------------------------------ navegación */

static void emitir_estado(void)
{
    static int u_pag = -1, u_zoom = -1, u_rot = -1;
    if (u_pag == pagina && u_zoom == zoom && u_rot == rot) return;
    u_pag = pagina; u_zoom = zoom; u_rot = rot;
    imprimir_em(SAIDA, "visorpdf: estado pagina=%d/%d zoom=%d rot=%d\n", pagina + 1, n_paginas, zoom, rot);
}

/* Lleva la página p a la parte de arriba. */
static void ir_pagina(int p)
{
    if (!doc || n_paginas <= 0) return;
    if (p < 0) p = 0;
    if (p >= n_paginas) p = n_paginas - 1;
    pagina = p;
    if (apres && modo_aj) { zoom_de_ajuste(); recalcular_layout(); }
    desl_y = ytop[p] - (apres ? 0 : MARGEM);
    recalcular_layout();
    pagina = p;
    conf_marcar();
    sujo = true;
}

/* Cambia el zoom sin perder el sitio que se estaba mirando. */
static void poner_zoom(int z, int modo)
{
    if (!doc) return;
    if (z < ZOOM_MIN) z = ZOOM_MIN;
    if (z > ZOOM_MAX) z = ZOOM_MAX;
    int x0, y0, vl, va;
    area_pagina(&x0, &y0, &vl, &va);
    int pg = pagina;
    long cy = (long)desl_y + va / 2 - ytop[pg];
    long cx = (long)desl_x + vl / 2;
    int antes = zoom;
    modo_aj = modo;
    zoom = z;
    if (modo) zoom_de_ajuste();
    recalcular_layout();
    if (antes > 0) {
        desl_y = (int)(cy * zoom / antes) + ytop[pg] - va / 2;
        desl_x = (int)(cx * zoom / antes) - vl / 2;
    }
    recalcular_layout();
    pagina = pg;
    conf_marcar();
    sujo = true;
}

static const int PASSOS_ZOOM[] = { 10, 25, 50, 75, 100, 125, 150, 200, 300, 400 };

static void zoom_passo(int d)
{
    int n = (int)(sizeof PASSOS_ZOOM / sizeof PASSOS_ZOOM[0]);
    int z = zoom;
    if (d > 0) {
        for (int i = 0; i < n; i++) if (PASSOS_ZOOM[i] > zoom) { z = PASSOS_ZOOM[i]; break; }
        if (z == zoom) z = ZOOM_MAX;
    } else {
        for (int i = n - 1; i >= 0; i--) if (PASSOS_ZOOM[i] < zoom) { z = PASSOS_ZOOM[i]; break; }
        if (z == zoom) z = ZOOM_MIN;
    }
    poner_zoom(z, 0);
}

static void girar(void)
{
    if (!doc) return;
    int pg = pagina;
    rot = (rot + 90) % 360;
    if (modo_aj) zoom_de_ajuste();
    recalcular_layout();
    ir_pagina(pg);
}

static void rolar(int dy)
{
    desl_y += dy;
    recalcular_layout();
    actualizar_pagina_atual();
    conf_marcar();
    sujo = true;
}

/* ------------------------------------------------------------ desenho */

static void icone_boton(int x, int y, int l, int a, const char *ic, bool ativo)
{
    g_retangulo(j, x, y, l, a, ativo ? COR_BOTAO_APERTADO : COR_BOTAO);
    g_moldura(j, x, y, l, a, 0xB4BCC4);
    if (ic) icone_desenhar(j, x + (l - 16) / 2, y + (a - 16) / 2, ic, 1);
}

/* Los iconos que no están en la biblioteca: +, -, ajustar al ancho y a la página. */
static void icone_a_mao(int id, int x, int y)
{
    uint32_t c = COR_TEXTO;
    if (id == B_ZMENOS || id == B_ZMAIS) {
        g_retangulo(j, x + 8, y + 15, 14, 2, c);
        if (id == B_ZMAIS) g_retangulo(j, x + 14, y + 9, 2, 14, c);
    } else if (id == B_AJ_LARG) {
        g_retangulo(j, x + 7, y + 8, 2, 16, c);
        g_retangulo(j, x + 21, y + 8, 2, 16, c);
        g_linha(j, x + 9, y + 16, x + 20, y + 16, c);
        g_linha(j, x + 9, y + 16, x + 12, y + 13, c);
        g_linha(j, x + 9, y + 16, x + 12, y + 19, c);
        g_linha(j, x + 20, y + 16, x + 17, y + 13, c);
        g_linha(j, x + 20, y + 16, x + 17, y + 19, c);
    } else if (id == B_AJ_PAG) {
        g_moldura(j, x + 9, y + 7, 12, 18, c);
        g_moldura(j, x + 10, y + 8, 10, 16, c);
        g_retangulo(j, x + 12, y + 11, 6, 2, c);
        g_retangulo(j, x + 12, y + 15, 6, 2, c);
    }
}

static void desenhar_barra(void)
{
    if (apres) return;
    g_retangulo(j, 0, 0, j->larg, BARRA, 0xE6E9EC);
    g_retangulo(j, 0, BARRA - 1, j->larg, 1, COR_BORDA);
    char txt[48];
    int x = 6;
    for (int i = 0; i < B_QTD; i++) {
        int l = 30;
        if (i == B_PAGINA) { formatar_texto(txt, sizeof txt, "%d / %d", pagina + 1, n_paginas); l = g_largura_texto(txt) + 16; if (l < 70) l = 70; }
        if (i == B_ZOOM) { formatar_texto(txt, sizeof txt, "%d%%", zoom); l = 52; }
        if (i == B_ANT || i == B_ZMENOS || i == B_GIRAR) x += 8;   /* separadores */
        tb[i].x = x; tb[i].l = l;
        bool ativo = (i == B_BUSCAR && busca_ver) || (i == B_PAINEL && painel_ver) ||
                     (i == B_AJ_LARG && modo_aj == 1) || (i == B_AJ_PAG && modo_aj == 2);
        if (i == B_PAGINA || i == B_ZOOM) {
            g_retangulo(j, x, 4, l, 26, COR_BRANCO);
            g_moldura(j, x, 4, l, 26, 0xB4BCC4);
            g_texto(j, x + (l - g_largura_texto(txt)) / 2, 4 + (26 - LETRA_A) / 2, txt, COR_TEXTO);
        } else {
            icone_boton(x, 4, l, 26, ICONE_BT[i], ativo);
            icone_a_mao(i, x, 2);
        }
        x += l + 3;
    }
}

static void desenhar_barra_busca(void)
{
    if (apres || !busca_ver) return;
    int y = BARRA;
    g_retangulo(j, 0, y, j->larg, BUSCA_H, 0xEEF0F2);
    g_retangulo(j, 0, y + BUSCA_H - 1, j->larg, 1, COR_BORDA);
    g_texto(j, 8, y + (BUSCA_H - LETRA_A) / 2, "Buscar:", COR_TEXTO);
    busca_caixa.x = 8 + 7 * LETRA_L + 6;
    busca_caixa.y = y + 3;
    busca_caixa.l = 230;
    caixa_desenhar(j, &busca_caixa);
    int x = busca_caixa.x + busca_caixa.l + 8;
    icone_boton(x, y + 3, 26, 24, "subir", false);
    icone_boton(x + 28, y + 3, 26, 24, "descer", false);
    char txt[64];
    if (busca_nq == 0) txt[0] = 0;
    else if (busca_prox < n_paginas)
        formatar_texto(txt, sizeof txt, "Buscando... %d%%  (%d)", busca_prox * 100 / (n_paginas ? n_paginas : 1), n_acertos);
    else if (n_acertos == 0) formatar_texto(txt, sizeof txt, "Sin resultados");
    else formatar_texto(txt, sizeof txt, "%d de %d", acerto_atual < 0 ? 0 : acerto_atual + 1, n_acertos);
    g_texto(j, x + 62, y + (BUSCA_H - LETRA_A) / 2, txt, COR_TEXTO_FRACO);
    icone_boton(j->larg - 32, y + 3, 26, 24, "fechar", false);
}

static void desenhar_rodape(void)
{
    if (apres) return;
    int y = j->alt - RODAPE;
    g_retangulo(j, 0, y, j->larg, RODAPE, 0xE6E9EC);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);
    if (!doc) return;
    char dir[64];
    formatar_texto(dir, sizeof dir, "%d x %d pt   %d%%", pg_pl[pagina], pg_pa[pagina], zoom);
    int ty = y + (RODAPE - LETRA_A) / 2;
    g_texto(j, j->larg - 12 - g_largura_texto(dir), ty, dir, COR_TEXTO_FRACO);
    int cols = (j->larg - 12 - g_largura_texto(dir) - 20) / LETRA_L;
    g_texto_n(j, 10, ty, aviso[0] ? aviso : caminho, cols, aviso[0] ? COR_DESTAQUE : COR_TEXTO_FRACO);
}

static void desenhar_abas(void)
{
    int pl = painel_larg();
    int y0 = barra_h() + busca_h();
    /* Las pestañas. */
    static const char *const abas[2] = { "Miniaturas", "Índice" };
    for (int i = 0; i < 2; i++) {
        int x = i * (pl / 2);
        int l = (i == 0) ? pl / 2 : pl - pl / 2 - 1;
        g_retangulo(j, x, y0, l, ABA_H, painel_aba == i ? 0xF4F4F0 : 0xC9CFD5);
        g_moldura(j, x, y0, l, ABA_H, 0xB4BCC4);
        g_texto(j, x + (l - g_largura_texto(abas[i])) / 2, y0 + (ABA_H - LETRA_A) / 2, abas[i], COR_TEXTO);
    }
}

/* Si la página actual cambió, desplaza el panel hasta que se vea su miniatura
 * (o su marcador del índice). */
static void painel_seguir_pagina(void)
{
    static int ult = -1;
    if (ult == pagina || !doc) return;
    ult = pagina;
    int py, pa;
    area_painel(&py, &pa);
    int ini, fim;
    if (painel_aba == 0) {
        ini = 6;
        for (int p = 0; p < pagina; p++) ini += mini_alt(p) + 24;
        fim = ini + mini_alt(pagina) + 24;
    } else {
        int atual = -1;
        for (int i = 0; i < n_marcas; i++) if (marcas[i].pagina >= 0 && marcas[i].pagina <= pagina) atual = i;
        if (atual < 0) return;
        ini = atual * 20;
        fim = ini + 20;
    }
    if (ini < painel_desl) painel_desl = ini - 6;
    else if (fim > painel_desl + pa) painel_desl = fim - pa + 6;
    if (painel_desl < 0) painel_desl = 0;
}

static void desenhar_painel(void)
{
    int pl = painel_larg();
    if (!pl) return;
    painel_seguir_pagina();
    int y0 = barra_h() + busca_h();
    int a = j->alt - y0 - rodape_h();
    g_retangulo(j, 0, y0, pl, a, 0xDDE1E5);
    g_retangulo(j, pl - 1, y0, 1, a, COR_BORDA);
    int py, pa;
    area_painel(&py, &pa);
    g_retangulo(j, 0, py, pl - 1, pa, 0xF4F4F0);
    if (!doc) { desenhar_abas(); return; }
    if (painel_aba == 0) {
        int tw = mini_larg();
        int y = 6;
        for (int p = 0; p < n_paginas; p++) {
            int th = mini_alt(p);
            int ch = th + 24;
            int sy = py + y - painel_desl;
            if (sy + ch > py && sy < py + pa) {
                int x = (pl - 1 - tw) / 2;
                bool sel = (p == pagina);
                g_retangulo(j, x - 2, sy - 2, tw + 4, th + 4, sel ? COR_SELECAO : 0xB4BCC4);
                g_retangulo(j, x, sy, tw, th, 0xFFFFFF);
                if (minis && minis[p].px) {
                    for (int r = 0; r < th && r < minis[p].a; r++) {
                        int yy = sy + r;
                        if (yy < py || yy >= py + pa) continue;
                        memcpy(j->px + (size_t)yy * j->larg + x, minis[p].px + (size_t)r * minis[p].l,
                               (size_t)(tw < minis[p].l ? tw : minis[p].l) * 4);
                    }
                }
                char t[16];
                formatar_texto(t, sizeof t, "%d", p + 1);
                int ty = sy + th + 4;
                if (ty >= py && ty + LETRA_A <= py + pa)
                    g_texto(j, (pl - 1 - g_largura_texto(t)) / 2, ty, t, sel ? COR_SELECAO : COR_TEXTO);
            }
            y += ch;
            if (y - painel_desl > pa + 8) break;
        }
    } else {
        if (n_marcas == 0) {
            g_texto(j, 10, py + 10, "Sin índice", COR_TEXTO_FRACO);
            g_texto(j, 10, py + 28, "en este PDF", COR_TEXTO_FRACO);
        }
        int atual = -1;
        for (int i = 0; i < n_marcas; i++) if (marcas[i].pagina >= 0 && marcas[i].pagina <= pagina) atual = i;
        for (int i = 0; i < n_marcas; i++) {
            int sy = py + i * 20 - painel_desl;
            if (sy + 20 <= py || sy >= py + pa) continue;
            if (i == atual) g_retangulo(j, 0, sy, pl - 1, 20, 0xC8D8F0);
            int x = 8 + marcas[i].nivel * 12;
            int cols = (pl - 1 - x - 4) / LETRA_L;
            if (cols > 0) g_texto_n(j, x, sy + 2, marcas[i].titulo, cols, marcas[i].pagina >= 0 ? COR_TEXTO : COR_TEXTO_FRACO);
        }
    }
    desenhar_abas();
}

/* El rectángulo r (en décimas de punto) de la página p, girado, en la ventana. */
static bool rect_en_ventana(const rect_t *r, int p, int px, int py, int *rx, int *ry, int *rl, int *ra)
{
    int nw, nh;
    dim_nat(p, zoom, &nw, &nh);
    int ux = r->x * zoom / 1000, uy = r->y * zoom / 1000;
    int ul = r->l * zoom / 1000, ua = r->a * zoom / 1000;
    if (ul < 1) ul = 1;
    if (ua < 1) ua = 1;
    int x, y, l, a;
    if (rot == 0) { x = ux; y = uy; l = ul; a = ua; }
    else if (rot == 90) { x = nh - uy - ua; y = ux; l = ua; a = ul; }
    else if (rot == 180) { x = nw - ux - ul; y = nh - uy - ua; l = ul; a = ua; }
    else { x = uy; y = nw - ux - ul; l = ua; a = ul; }
    *rx = px + x; *ry = py + y; *rl = l; *ra = a;
    return true;
}

/* Tiñe (multiplicando) un rectángulo de la ventana, recortado al área. */
static void resaltar(int x, int y, int l, int a, uint32_t cor, int cx, int cy, int cl, int ca)
{
    int x1 = x + l, y1 = y + a;
    if (x < cx) x = cx;
    if (y < cy) y = cy;
    if (x1 > cx + cl) x1 = cx + cl;
    if (y1 > cy + ca) y1 = cy + ca;
    uint32_t cr = (cor >> 16) & 255, cg = (cor >> 8) & 255, cb = cor & 255;
    for (int yy = y; yy < y1; yy++) {
        uint32_t *d = j->px + (size_t)yy * j->larg;
        for (int xx = x; xx < x1; xx++) {
            uint32_t v = d[xx];
            uint32_t r = (((v >> 16) & 255) * cr) / 255;
            uint32_t g = (((v >> 8) & 255) * cg) / 255;
            uint32_t b = ((v & 255) * cb) / 255;
            d[xx] = (r << 16) | (g << 8) | b;
        }
    }
}

static void desenhar_paginas(void)
{
    int x0, y0, vl, va;
    area_pagina(&x0, &y0, &vl, &va);
    g_retangulo(j, x0, y0, vl, va, apres ? 0x000000 : 0x6A737C);
    if (!doc) return;
    quadro++;
    for (int p = 0; p < n_paginas; p++) {
        if (ytop[p] + lh[p] <= desl_y) continue;
        int sy = y0 + ytop[p] - desl_y;
        if (sy >= y0 + va) break;
        int sx = (doc_l <= vl) ? x0 + (vl - lw[p]) / 2 : x0 + (doc_l - lw[p]) / 2 - desl_x;
        uint32_t *buf = obter_render(p);
        /* El recorte visible. */
        int cx0 = sx < x0 ? x0 - sx : 0;
        int cy0 = sy < y0 ? y0 - sy : 0;
        int cx1 = sx + lw[p] > x0 + vl ? x0 + vl - sx : lw[p];
        int cy1 = sy + lh[p] > y0 + va ? y0 + va - sy : lh[p];
        if (cx1 <= cx0 || cy1 <= cy0) continue;
        if (buf) {
            for (int y = cy0; y < cy1; y++)
                memcpy(j->px + (size_t)(sy + y) * j->larg + sx + cx0, buf + (size_t)y * lw[p] + cx0,
                       (size_t)(cx1 - cx0) * 4);
        } else {
            g_retangulo(j, sx + cx0, sy + cy0, cx1 - cx0, cy1 - cy0, 0xFFFFFF);
        }
        if (!apres) g_moldura(j, sx - 1, sy - 1, lw[p] + 2, lh[p] + 2, 0x4A525A);
        /* Las coincidencias de la búsqueda. */
        if (busca_ver)
            for (int i = 0; i < n_rects; i++) {
                if (rects[i].pag != p) continue;
                int rx, ry, rl, ra;
                rect_en_ventana(&rects[i], p, sx, sy, &rx, &ry, &rl, &ra);
                resaltar(rx, ry, rl, ra, rects[i].m == acerto_atual ? 0xFF9632 : 0xFFEB28, x0, y0, vl, va);
            }
    }
    /* La barra de desplazamiento. */
    if (!apres) {
        int sx = x0 + vl;
        g_retangulo(j, sx, y0, SB_L, va, 0xD2D7DC);
        if (doc_a > va) {
            int th = va * va / doc_a;
            if (th < 20) th = 20;
            int ty = y0 + (int)((long)desl_y * (va - th) / (doc_a - va));
            g_retangulo(j, sx + 2, ty, SB_L - 4, th, 0x8A939C);
        }
    }
}

static void desenhar_tudo(void)
{
    if (apres) {
        g_retangulo(j, 0, 0, j->larg, j->alt, 0x000000);
    } else {
        g_retangulo(j, 0, 0, j->larg, j->alt, 0x6A737C);
    }
    desenhar_paginas();
    if (!doc && !apres) {
        /* Sin documento: una invitación centrada en la zona de las páginas. */
        const char *l1 = "Ningún documento abierto";
        const char *l2 = "Ctrl+O o el botón Abrir para elegir un PDF";
        int cy = j->alt / 2 - 16;
        g_texto(j, (j->larg - (int)tam_texto(l1) * LETRA_L) / 2, cy, l1, 0xFFFFFF);
        g_texto(j, (j->larg - (int)tam_texto(l2) * LETRA_L) / 2, cy + 22, l2, 0xD8DCE0);
    }
    desenhar_painel();
    desenhar_barra();
    desenhar_barra_busca();
    desenhar_rodape();
    jan_mostrar(j);
    sujo = false;
}

/* ------------------------------------------------------------ diálogos */

/* Pide la contraseña de un PDF cifrado con un diálogo propio: el texto va con
 * asteriscos. Devuelve false si se cancela. */
static bool pedir_senha(const char *nome, bool errada, char *saida, int max)
{
    int larg = 400, alt = 170;
    jan_t *d = jan_abrir("Visor de PDF", larg, alt);
    if (!d) return false;
    jan_estilo(d, JAN_DIALOGO, 0, 0);
    caixa_t c;
    memset(&c, 0, sizeof c);
    c.x = 16; c.y = 76; c.l = larg - 32; c.foco = true; c.oculta = true;
    botao_t b_ok = { larg - 16 - 90, alt - 40, 90, 26, "OK", false };
    botao_t b_no = { larg - 16 - 190, alt - 40, 90, 26, "Cancelar", false };
    bool ok = false, fim = false;
    evento_t e;
    for (;;) {
        g_retangulo(d, 0, 0, larg, alt, COR_FUNDO);
        g_texto(d, 16, 14, "Este PDF está protegido con contraseña.", COR_TEXTO);
        g_texto_n(d, 16, 34, nome, (larg - 32) / LETRA_L, COR_TEXTO_FRACO);
        g_texto(d, 16, 56, "Contraseña:", COR_TEXTO);
        if (errada) g_texto(d, 16, 108, "Contraseña incorrecta. Prueba otra vez.", 0xC02020);
        caixa_desenhar(d, &c);
        botao_desenhar(d, &b_ok);
        botao_desenhar(d, &b_no);
        jan_mostrar(d);
        if (fim) break;
        while (esperar_evento(d->id, &e, 200) != 1) { }
        if (e.tipo == EV_FECHAR || (e.tipo == EV_TECLA && e.tecla == TECLA_ESC)) { fim = true; continue; }
        if (caixa_tratar(d, &c, &e)) { ok = true; fim = true; continue; }
        if (botao_tratar(d, &b_ok, &e)) { ok = true; fim = true; continue; }
        if (botao_tratar(d, &b_no, &e)) { fim = true; continue; }
    }
    if (ok) copia_texto(saida, c.texto, (size_t)max);
    memset(&c, 0, sizeof c);
    jan_fechar(d);
    return ok;
}

/* PUNTO ÚNICO para el diálogo de abrir archivo (mejora 103): cuando llegue el
 * selector de archivos estilo Windows 95 de la rama v8.1-dialogos-archivo, solo
 * hay que cambiar el cuerpo de esta función (devolver true y el camino en
 * 'saida', o false si se cancela). De momento se pide el camino a mano. */
static bool escolher_arquivo(char *saida, int max)
{
    copia_texto(saida, caminho, (size_t)max);
    /* D272: el diálogo común de abrir (estilo Windows 95), con filtro de PDF. */
    return dialogo_abrir("Abrir PDF", "Documentos PDF (*.pdf)|*.pdf|Todos los archivos (*.*)|*", saida, saida, max);
}

/* ------------------------------------------------------------ el PDF */

static void liberar_documento(void)
{
    cache_limpar();
    minis_limpar();
    marcas_limpar();
    if (rects) libera(rects);
    rects = NULL;
    if (pg_pl) libera(pg_pl);
    if (pg_pa) libera(pg_pa);
    if (lw) libera(lw);
    if (lh) libera(lh);
    if (ytop) libera(ytop);
    pg_pl = pg_pa = lw = lh = ytop = NULL;
    if (doc) pdf_fechar(doc);
    doc = NULL;
    n_paginas = 0;
}

/* Abre un PDF (o lo vuelve a abrir). Pide la contraseña si hace falta. */
static bool abrir_arquivo(const char *caminho_pdf)
{
    int erro = 0;
    pdf_t *d = pdf_abrir(caminho_pdf, &erro);
    if ((!d && erro == PDF_ERRO_CIFRADO) || (d && pdf_necesita_contrasena(d))) {
        if (d) { pdf_fechar(d); d = NULL; }
        bool errada = false;
        for (;;) {
            char senha[64];
            imprimir_em(SAIDA, "visorpdf: pide contraseña\n");
            if (!pedir_senha(caminho_pdf, errada, senha, sizeof senha)) {
                memset(senha, 0, sizeof senha);
                return false;
            }
            d = pdf_abrir_com_contrasena(caminho_pdf, senha, &erro);
            memset(senha, 0, sizeof senha);
            if (d && !pdf_necesita_contrasena(d)) break;
            if (d) { pdf_fechar(d); d = NULL; }
            if (erro != PDF_ERRO_CIFRADO && erro != 0) break;
            imprimir_em(SAIDA, "visorpdf: contraseña incorrecta\n");
            errada = true;
        }
    }
    if (!d) {
        mensagem("Visor de PDF", pdf_erro_texto(erro));
        return false;
    }
    int np = pdf_paginas(d);
    if (np <= 0) {
        pdf_fechar(d);
        mensagem("Visor de PDF", "El PDF no tiene páginas");
        return false;
    }
    if (doc) conf_gravar();
    liberar_documento();
    doc = d;
    copia_texto(caminho, caminho_pdf, sizeof caminho);
    n_paginas = np;
    pg_pl = aloca_zerado((size_t)np * sizeof(int));
    pg_pa = aloca_zerado((size_t)np * sizeof(int));
    lw = aloca_zerado((size_t)np * sizeof(int));
    lh = aloca_zerado((size_t)np * sizeof(int));
    ytop = aloca_zerado((size_t)np * sizeof(int));
    minis = aloca_zerado((size_t)np * sizeof(mini_t));
    rects = aloca((size_t)MAX_RECTS * sizeof(rect_t));
    if (!pg_pl || !pg_pa || !lw || !lh || !ytop || !minis || !rects) {
        liberar_documento();
        mensagem("Visor de PDF", "Sin memoria para el documento");
        return false;
    }
    for (int p = 0; p < np; p++) {
        int w = pdf_pagina_largura(doc, p), h = pdf_pagina_altura(doc, p);
        pg_pl[p] = w > 0 ? w : 595;
        pg_pa[p] = h > 0 ? h : 842;
    }
    n_minis = 0;
    aviso[0] = 0;
    desl_x = desl_y = 0;
    painel_desl = 0;
    busca_limpar();
    busca_txt[0] = 0;
    busca_nq = 0;
    n_marcas = pdf_indice(doc, &marcas);
    for (int i = 0; i < n_marcas; i++) titulo_latin1(marcas[i].titulo);
    /* Lo recordado de este documento, o el ajuste a la página. */
    int rp = 0, rz = 0, rm = 0, rr = 0;
    pagina = 0;
    if (conf_ler(caminho_pdf, &rp, &rz, &rm, &rr)) {
        rot = (rr == 90 || rr == 180 || rr == 270) ? rr : 0;
        modo_aj = (rm >= 0 && rm <= 2) ? rm : 0;
        zoom = (rz >= ZOOM_MIN && rz <= ZOOM_MAX) ? rz : 100;
        if (rp < 0 || rp >= np) rp = 0;
        pagina = rp;
        zoom_de_ajuste();
        recalcular_layout();
        ir_pagina(rp);
        imprimir_em(SAIDA, "visorpdf: recordado pagina=%d zoom=%d\n", rp + 1, zoom);
    } else {
        rot = 0;
        modo_aj = 1;
        zoom_de_ajuste();
        recalcular_layout();
        ir_pagina(0);
    }
    conf_sujo = false;
    desenhar_tudo();
    /* Las señales de la serie: así las pruebas saben que ya está listo. */
    imprimir_em(SAIDA, "visorpdf: pronto (%d páginas)\n", n_paginas);
    imprimir_em(SAIDA, "visorpdf: indice %d\n", n_marcas);
    emitir_estado();
    return true;
}

/* Exporta la página actual (con el zoom y el giro que se ven) a una imagen. */
static void exportar_pagina(void)
{
    if (!doc) return;
    char base[96];
    const char *b = caminho;
    for (const char *s = caminho; *s; s++) if (*s == '/') b = s + 1;
    int k = 0;
    while (b[k] && b[k] != '.' && k < 60) { base[k] = b[k]; k++; }
    base[k] = 0;
    char dest[200];
    formatar_texto(dest, sizeof dest, "/home/%s-p%d.png", base, pagina + 1);
    if (!dialogo_guardar("Exportar página a imagen", "PNG (*.png)|*.png|BMP (*.bmp)|*.bmp|PPM (*.ppm)|*.ppm", dest, dest, sizeof dest) || !dest[0]) return;
    uint32_t *buf = obter_render(pagina);
    if (!buf) { mensagem("Visor de PDF", "No se pudo dibujar la página"); return; }
    imagem_t img;
    memset(&img, 0, sizeof img);
    if (!imagem_criar(&img, lw[pagina], lh[pagina], 0xFFFFFF)) { mensagem("Visor de PDF", "Sin memoria"); return; }
    memcpy(img.px, buf, (size_t)lw[pagina] * lh[pagina] * 4);
    int n = (int)tam_texto(dest);
    int r = 0;
    for (int intento = 0; intento < 2; intento++) {
        if (n > 4 && compara_texto(dest + n - 4, ".bmp") == 0) r = imagem_gravar_bmp(dest, &img);
        else if (n > 4 && compara_texto(dest + n - 4, ".ppm") == 0) r = imagem_gravar_ppm(dest, &img);
        else r = imagem_gravar_png(dest, &img);
        if (r != ERRO_NAO_EXISTE) break;
        /* La carpeta de destino no existe (p. ej. /home sin crear): se crea y se repite. */
        char pai[200];
        copia_texto(pai, dest, sizeof pai);
        char *barra = acha_ultimo_char(pai, '/');
        if (!barra || barra == pai) break;
        *barra = 0;
        criar_pasta(pai);
    }
    imagem_liberar(&img);
    if (r != 0) {
        mensagem("Visor de PDF", imagem_erro(r));
        return;
    }
    formatar_texto(aviso, sizeof aviso, "Página %d exportada a %s", pagina + 1, dest);
    imprimir_em(SAIDA, "visorpdf: exportada %s\n", dest);
    sujo = true;
}

static void pedir_pagina(void)
{
    char t[24];
    formatar_texto(t, sizeof t, "%d", pagina + 1);
    char q[48];
    formatar_texto(q, sizeof q, "Ir a la página (1-%d):", n_paginas);
    if (pedir_texto("Visor de PDF", q, t, sizeof t)) ir_pagina(texto_para_int(t) - 1);
}

static void pedir_zoom(void)
{
    char t[16];
    formatar_texto(t, sizeof t, "%d", zoom);
    if (pedir_texto("Visor de PDF", "Zoom en % (10 a 400):", t, sizeof t)) {
        int z = texto_para_int(t);
        if (z > 0) poner_zoom(z, 0);
    }
}

static void abrir_outro(void)
{
    char outro[256];
    if (escolher_arquivo(outro, sizeof outro)) abrir_arquivo(outro);
    sujo = true;
}

static void alternar_busca(bool ver)
{
    busca_ver = ver;
    busca_caixa.foco = ver;
    if (ver && busca_nq > 0 && n_acertos == 0 && busca_prox >= n_paginas && busca_txt[0]) busca_iniciar();
    recalcular_layout();
    if (modo_aj) { zoom_de_ajuste(); recalcular_layout(); }
    sujo = true;
}

static void alternar_painel(void)
{
    painel_ver = !painel_ver;
    if (modo_aj) zoom_de_ajuste();
    recalcular_layout();
    sujo = true;
}

static void alternar_apresentacao(void)
{
    static bool ant_painel, ant_busca;
    static int ant_zoom, ant_modo;
    if (!apres) {
        ant_painel = painel_ver; ant_busca = busca_ver; ant_zoom = zoom; ant_modo = modo_aj;
        apres = true;
        busca_ver = false;
        modo_aj = 2;
        jan_tela_cheia(j, true);
        imprimir_em(SAIDA, "visorpdf: presentacion\n");
    } else {
        apres = false;
        painel_ver = ant_painel; busca_ver = ant_busca; zoom = ant_zoom; modo_aj = ant_modo;
        jan_tela_cheia(j, false);
        recalcular_layout();
        ir_pagina(pagina);
    }
    zoom_de_ajuste();
    recalcular_layout();
    ir_pagina(pagina);
}

/* ------------------------------------------------------------ eventos */

static void clic_barra(int x)
{
    for (int i = 0; i < B_QTD; i++) {
        if (x < tb[i].x || x >= tb[i].x + tb[i].l) continue;
        aviso[0] = 0;
        switch (i) {
        case B_ABRIR: abrir_outro(); break;
        case B_EXPORTAR: exportar_pagina(); break;
        case B_ANT: ir_pagina(pagina - 1); break;
        case B_PROX: ir_pagina(pagina + 1); break;
        case B_PAGINA: pedir_pagina(); break;
        case B_ZMENOS: zoom_passo(-1); break;
        case B_ZMAIS: zoom_passo(1); break;
        case B_ZOOM: pedir_zoom(); break;
        case B_AJ_LARG: poner_zoom(zoom, 1); break;
        case B_AJ_PAG: poner_zoom(zoom, 2); break;
        case B_GIRAR: girar(); break;
        case B_BUSCAR: alternar_busca(!busca_ver); break;
        case B_PAINEL: alternar_painel(); break;
        case B_APRES: alternar_apresentacao(); break;
        }
        sujo = true;
        return;
    }
}

static void clic_painel(int x, int y)
{
    (void)x;
    int y0 = barra_h() + busca_h();
    if (y < y0 + ABA_H) {
        painel_aba = x < painel_larg() / 2 ? 0 : 1;
        painel_desl = 0;
        sujo = true;
        return;
    }
    int py, pa;
    area_painel(&py, &pa);
    if (painel_aba == 0) {
        int yy = 6;
        for (int p = 0; p < n_paginas; p++) {
            int ch = mini_alt(p) + 24;
            int sy = py + yy - painel_desl;
            if (y >= sy - 2 && y < sy + ch - 2) { ir_pagina(p); return; }
            yy += ch;
        }
    } else {
        int i = (y - py + painel_desl) / 20;
        if (i >= 0 && i < n_marcas && marcas[i].pagina >= 0) ir_pagina(marcas[i].pagina);
    }
}

static void painel_rolar(int dy)
{
    int py, pa;
    area_painel(&py, &pa);
    int total;
    if (painel_aba == 0) {
        total = 6;
        for (int p = 0; p < n_paginas; p++) total += mini_alt(p) + 24;
    } else {
        total = n_marcas * 20;
    }
    painel_desl += dy;
    if (painel_desl > total - pa) painel_desl = total - pa;
    if (painel_desl < 0) painel_desl = 0;
    sujo = true;
}

static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_REDIMENSIONAR) {
        bool cheia = e->tecla == JAN_ESTADO_TELA_CHEIA;
        if (jan_redimensionar(j, e)) {
            tela_cheia = cheia;
            if (apres && !cheia) {   /* se salió de la pantalla completa: fin de la presentación */
                apres = false;
                painel_ver = true;
                modo_aj = 2;
            }
            if (modo_aj) zoom_de_ajuste();
            recalcular_layout();
            if (apres) ir_pagina(pagina);
            sujo = true;
        }
        return true;
    }
    int x0, y0, vl, va;
    area_pagina(&x0, &y0, &vl, &va);

    if (e->tipo == EV_TECLA) {
        unsigned t = e->tecla;
        aviso[0] = 0;
        bool shift = (e->modificadores & 1) != 0;
        /* La caja de búsqueda tiene el foco. */
        if (busca_ver && busca_caixa.foco && t != TECLA_ESC && t != TECLA_F3 && t != TECLA_F5 && t != TECLA_F4 &&
            t != 6 && t != 15 && t != 7 && t != 19 && t < 0x100) {
            bool enter = caixa_tratar(j, &busca_caixa, e);
            if (compara_texto(busca_caixa.texto, busca_txt) != 0) busca_iniciar();
            else if (enter) acerto_mover(shift ? -1 : 1);
            sujo = true;
            return true;
        }
        switch (t) {
        case TECLA_ESC:
            if (busca_ver) alternar_busca(false);
            else if (apres) alternar_apresentacao();
            else if (tela_cheia) jan_tela_cheia(j, false);
            return true;
        case TECLA_ESQUERDA: ir_pagina(pagina - 1); return true;
        case TECLA_DIREITA: ir_pagina(pagina + 1); return true;
        case TECLA_CIMA: rolar(-PASSO_ROLAR); return true;
        case TECLA_BAIXO: rolar(PASSO_ROLAR); return true;
        case TECLA_PGUP: rolar(-(va * 9 / 10)); return true;
        case TECLA_PGDN: case ' ': rolar(va * 9 / 10); return true;
        case '\b': rolar(-(va * 9 / 10)); return true;
        case TECLA_HOME: ir_pagina(0); return true;
        case TECLA_END: ir_pagina(n_paginas - 1); return true;
        case '+': case '=': zoom_passo(1); return true;
        case '-': zoom_passo(-1); return true;
        case '0': poner_zoom(zoom, 2); return true;
        case 'w': case 'W': poner_zoom(zoom, 1); return true;
        case '1': poner_zoom(100, 0); return true;
        case 'r': case 'R': girar(); return true;
        case TECLA_F4: alternar_painel(); return true;
        case TECLA_F5: alternar_apresentacao(); return true;
        case TECLA_F11: jan_tela_cheia(j, !tela_cheia); return true;
        case TECLA_F3:
            if (!busca_ver) alternar_busca(true);
            acerto_mover(shift ? -1 : 1);
            return true;
        case 15: abrir_outro(); return true;                 /* Ctrl+O */
        case 6: alternar_busca(true); busca_caixa.foco = true; return true;   /* Ctrl+F */
        case 7: pedir_pagina(); return true;                 /* Ctrl+G */
        case 19: exportar_pagina(); return true;             /* Ctrl+S */
        default: return true;
        }
    }
    if (e->tipo == EV_RODA) {
        int n = (int)e->cliques;
        if (n == 0) n = 1;
        if (painel_larg() && e->x < painel_larg()) { painel_rolar(n * 40); return true; }
        if (e->modificadores & 2) { zoom_passo(n > 0 ? -1 : 1); return true; }
        rolar(n * PASSO_ROLAR);
        return true;
    }
    if (e->tipo == EV_SOLTOU) { arr_modo = 0; return true; }
    if (e->tipo == EV_MOVER) {
        if (!apres && arr_modo == 0) {
            const char *dica = NULL;
            if (e->y < BARRA)
                for (int i = 0; i < B_QTD; i++)
                    if (e->x >= tb[i].x && e->x < tb[i].x + tb[i].l) dica = DICA_BT[i];
            if (dica) { copia_texto(aviso, dica, sizeof aviso); dica_ativa = true; sujo = true; }
            else if (dica_ativa) { aviso[0] = 0; dica_ativa = false; sujo = true; }
        }
        if (arr_modo == 1 && (e->botoes & 1)) {
            desl_x = arr_dx - (e->x - arr_x);
            desl_y = arr_dy - (e->y - arr_y);
            recalcular_layout();
            actualizar_pagina_atual();
            sujo = true;
        } else if (arr_modo == 2 && (e->botoes & 1) && doc_a > va) {
            desl_y = (int)((long)(e->y - y0) * doc_a / va) - va / 2;
            recalcular_layout();
            actualizar_pagina_atual();
            sujo = true;
        }
        return true;
    }
    if (e->tipo == EV_CLIQUE) {
        arr_modo = 0;
        aviso[0] = 0;
        if (apres) { ir_pagina(pagina + (e->botoes & 2 ? -1 : 1)); return true; }
        if (e->y < BARRA) { clic_barra(e->x); return true; }
        if (busca_ver && e->y < BARRA + BUSCA_H) {
            int bx = busca_caixa.x + busca_caixa.l + 8;
            if (e->x >= bx && e->x < bx + 26) acerto_mover(-1);
            else if (e->x >= bx + 28 && e->x < bx + 54) acerto_mover(1);
            else if (e->x >= j->larg - 32) alternar_busca(false);
            else { busca_caixa.foco = true; caixa_tratar(j, &busca_caixa, e); sujo = true; }
            return true;
        }
        if (painel_larg() && e->x < painel_larg()) { clic_painel(e->x, e->y); return true; }
        if (e->y >= y0 && e->y < y0 + va) {
            if (e->x >= x0 + vl) {          /* la barra de desplazamiento */
                arr_modo = 2;
                if (doc_a > va) {
                    desl_y = (int)((long)(e->y - y0) * doc_a / va) - va / 2;
                    recalcular_layout();
                    actualizar_pagina_atual();
                    sujo = true;
                }
            } else if (e->x >= x0) {
                arr_modo = 1;
                arr_x = e->x; arr_y = e->y; arr_dx = desl_x; arr_dy = desl_y;
                if (busca_ver) busca_caixa.foco = false;
            }
        }
        return true;
    }
    return true;
}

/* ------------------------------------------------------------ modos de prova */

/* -t: abre o PDF e diz o que tem. */
static int modo_testar(const char *caminho_pdf, const char *contrasena)
{
    int erro = 0;
    pdf_t *d = contrasena ? pdf_abrir_com_contrasena(caminho_pdf, contrasena, &erro)
                           : pdf_abrir(caminho_pdf, &erro);
    if (!d) {
        imprimir_em(ERROS, "visorpdf: %s: %s\n", caminho_pdf, pdf_erro_texto(erro));
        return 1;
    }
    if (pdf_necesita_contrasena(d)) {
        imprimir_em(ERROS, "visorpdf: %s: %s\n", caminho_pdf, pdf_erro_texto(PDF_ERRO_CIFRADO));
        pdf_fechar(d);
        return 1;
    }
    int n = pdf_paginas(d);
    imprimir_em(SAIDA, "paginas: %d\n", n);
    const char *t = pdf_titulo(d);
    if (t && t[0]) imprimir_em(SAIDA, "titulo: %s\n", t);
    const char *a = pdf_autor(d);
    if (a && a[0]) imprimir_em(SAIDA, "autor: %s\n", a);
    for (int i = 0; i < n; i++)
        imprimir_em(SAIDA, "pagina %d: %d x %d\n", i + 1, pdf_pagina_largura(d, i), pdf_pagina_altura(d, i));

    unsigned t0 = tempo_ms();
    int larg = pdf_pagina_largura(d, 0) * 100 / 100;
    int alt = pdf_pagina_altura(d, 0);
    if (larg > 0 && alt > 0) {
        uint8_t *buf = aloca((size_t)larg * alt * 4);
        if (buf) {
            memset(buf, 0, (size_t)larg * alt * 4);
            int r = pdf_render(d, 0, larg, alt, buf);
            unsigned t1 = tempo_ms();
            /* Um checksum simples para comparar com o render de referência. */
            unsigned soma = 0;
            for (size_t k = 0; k < (size_t)larg * alt * 4; k += 997) soma = soma * 31 + buf[k];
            imprimir_em(SAIDA, "render: %d ms, soma %u\n", t1 - t0, soma);
            libera(buf);
            if (r != 0) {
                imprimir_em(ERROS, "visorpdf: erro ao desenhar: %s\n", pdf_erro_texto(r));
                pdf_fechar(d);
                return 1;
            }
        }
    }
    pdf_fechar(d);
    return 0;
}

/* --ppm: desenha a página e grava um PPM (P6) para comparar. */
static int modo_ppm(const char *caminho_pdf, const char *saida, int pag, int z,
                    const char *senha)
{
    int erro = 0;
    pdf_t *d = senha ? pdf_abrir_com_contrasena(caminho_pdf, senha, &erro)
                     : pdf_abrir(caminho_pdf, &erro);
    if (!d) {
        imprimir_em(ERROS, "visorpdf: %s: %s\n", caminho_pdf, pdf_erro_texto(erro));
        return 1;
    }
    if (pdf_necesita_contrasena(d)) {
        imprimir_em(ERROS, "visorpdf: %s está cifrado: falta a palavra-passe\n", caminho_pdf);
        pdf_fechar(d);
        return 1;
    }
    if (pag < 0 || pag >= pdf_paginas(d)) {
        imprimir_em(ERROS, "visorpdf: não há essa página\n");
        pdf_fechar(d);
        return 1;
    }
    int larg = pdf_pagina_largura(d, pag) * z / 100;
    int alt = pdf_pagina_altura(d, pag) * z / 100;
    if (larg < 1) larg = 1;
    if (alt < 1) alt = 1;
    uint8_t *buf = aloca((size_t)larg * alt * 4);
    if (!buf) {
        imprimir_em(ERROS, "visorpdf: sem memória\n");
        pdf_fechar(d);
        return 1;
    }
    memset(buf, 0, (size_t)larg * alt * 4);
    int r = pdf_render(d, pag, larg, alt, buf);
    if (r != 0) {
        imprimir_em(ERROS, "visorpdf: %s\n", pdf_erro_texto(r));
        libera(buf);
        pdf_fechar(d);
        return 1;
    }
    int fd = abrir(saida, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) {
        imprimir_em(ERROS, "visorpdf: não consegui criar %s\n", saida);
        libera(buf);
        pdf_fechar(d);
        return 1;
    }
    char cab[64];
    int n = formatar_texto(cab, sizeof cab, "P6\n%d %d\n255\n", larg, alt);
    /* escrever_tudo: a escrever pode escrever menos bytes do que se pede
     * (o ficheiro fica truncado se nao se repetir). */
    if (escrever_tudo(fd, cab, (unsigned)n) < 0) {
        imprimir_em(ERROS, "visorpdf: não consegui escrever %s\n", saida);
        fechar(fd);
        libera(buf);
        pdf_fechar(d);
        return 1;
    }
    uint8_t *linha = aloca((size_t)larg * 3);
    for (int y = 0; y < alt; y++) {
        for (int x = 0; x < larg; x++) {
            const uint8_t *p = buf + ((size_t)y * larg + x) * 4;
            uint32_t a = p[3];
            linha[x * 3 + 0] = (uint8_t)((p[0] * a + 255 * (255 - a)) / 255);
            linha[x * 3 + 1] = (uint8_t)((p[1] * a + 255 * (255 - a)) / 255);
            linha[x * 3 + 2] = (uint8_t)((p[2] * a + 255 * (255 - a)) / 255);
        }
        escrever_tudo(fd, linha, (unsigned)(larg * 3));
    }
    libera(linha);
    fechar(fd);
    libera(buf);
    pdf_fechar(d);
    imprimir_em(SAIDA, "ppm: %s (%dx%d)\n", saida, larg, alt);
    return 0;
}


/* --indice: lista el índice del PDF (nivel, página, título). */
static int modo_indice(const char *caminho_pdf, const char *senha)
{
    int erro = 0;
    pdf_t *d = senha ? pdf_abrir_com_contrasena(caminho_pdf, senha, &erro) : pdf_abrir(caminho_pdf, &erro);
    if (!d) {
        imprimir_em(ERROS, "visorpdf: %s: %s\n", caminho_pdf, pdf_erro_texto(erro));
        return 1;
    }
    pdf_marca_t *m = NULL;
    int n = pdf_indice(d, &m);
    imprimir_em(SAIDA, "indice: %d\n", n);
    for (int i = 0; i < n; i++) {
        titulo_latin1(m[i].titulo);
        imprimir_em(SAIDA, "marca %d nivel %d pagina %d: %s\n", i + 1, m[i].nivel, m[i].pagina + 1, m[i].titulo);
    }
    if (m) libera(m);
    pdf_fechar(d);
    return 0;
}

/* --buscar: cuenta las coincidencias de un texto en cada página. */
static int modo_buscar(const char *caminho_pdf, const char *texto, const char *senha)
{
    int erro = 0;
    doc = senha ? pdf_abrir_com_contrasena(caminho_pdf, senha, &erro) : pdf_abrir(caminho_pdf, &erro);
    if (!doc) {
        imprimir_em(ERROS, "visorpdf: %s: %s\n", caminho_pdf, pdf_erro_texto(erro));
        return 1;
    }
    n_paginas = pdf_paginas(doc);
    rects = aloca((size_t)MAX_RECTS * sizeof(rect_t));
    copia_texto(busca_txt, texto, sizeof busca_txt);
    busca_preparar();
    n_acertos = 0;
    n_rects = 0;
    int total = 0;
    unsigned t0 = tempo_ms();
    for (int p = 0; p < n_paginas && busca_nq > 0; p++) {
        int c = buscar_em_pagina(p, busca_q, busca_nq);
        if (c) imprimir_em(SAIDA, "pagina %d: %d\n", p + 1, c);
        total += c;
    }
    imprimir_em(SAIDA, "total: %d\ntempo: %u ms\n", total, tempo_ms() - t0);
    if (rects) libera(rects);
    pdf_fechar(doc);
    doc = NULL;
    return 0;
}

/* ------------------------------------------------------------ programa */

int principal(int argc, char **argv)
{
    int i = 1;
    if (i < argc && (compara_texto(argv[i], "-t") == 0 || compara_texto(argv[i], "--testar") == 0)) {
        if (i + 1 >= argc) { imprimir_em(ERROS, "uso: visorpdf -t arquivo [palavra-passe]\n"); return 2; }
        return modo_testar(argv[i + 1], i + 2 < argc ? argv[i + 2] : NULL);
    }
    if (i < argc && compara_texto(argv[i], "--ppm") == 0) {
        if (i + 3 >= argc) { imprimir_em(ERROS, "uso: visorpdf --ppm arquivo saida.ppm [página [zoom [palavra-passe]]]\n"); return 2; }
        int pag = i + 3 < argc ? texto_para_int(argv[i + 3]) : 1;
        int z = i + 4 < argc ? texto_para_int(argv[i + 4]) : 100;
        const char *senha = i + 5 < argc ? argv[i + 5] : NULL;
        return modo_ppm(argv[i + 1], argv[i + 2], pag - 1, z, senha);
    }
    if (i < argc && compara_texto(argv[i], "--indice") == 0) {
        if (i + 1 >= argc) { imprimir_em(ERROS, "uso: visorpdf --indice arquivo [palavra-passe]\n"); return 2; }
        return modo_indice(argv[i + 1], i + 2 < argc ? argv[i + 2] : NULL);
    }
    if (i < argc && compara_texto(argv[i], "--buscar") == 0) {
        if (i + 2 >= argc) { imprimir_em(ERROS, "uso: visorpdf --buscar arquivo texto [palavra-passe]\n"); return 2; }
        return modo_buscar(argv[i + 1], argv[i + 2], i + 3 < argc ? argv[i + 3] : NULL);
    }
    /* Sin fichero (abierto desde el menú o el escritorio), la ventana se abre vacía con una
     * invitación a abrir un PDF (Ctrl+O o el botón Abrir), en vez de terminar con un error. */
    bool sin_arquivo = i >= argc;

    j = jan_abrir("Visor de PDF", LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK | JAN_ESC_LIVRE, LARG_MIN, ALT_MIN);

    if (sin_arquivo) {
        registrar("visorpdf: sin documento\n");
        sujo = true;
        desenhar_tudo();
    } else if (!abrir_arquivo(argv[i])) {
        jan_fechar(j);
        return 1;
    }

    evento_t e;
    bool continuar = true;
    while (continuar) {
        /* Con trabajo pendiente (búsqueda, miniaturas) se espera poco; si no, 200 ms. */
        bool trabajo = (busca_prox < n_paginas && busca_nq > 0) || (painel_ver && painel_aba == 0 && !apres && !mini_feito);
        if (esperar_evento(j->id, &e, trabajo ? 10 : 200) == 1) {
            continuar = tratar(&e);
            mini_feito = false;
            if (sujo && continuar) desenhar_tudo();
            emitir_estado();
        } else {
            if (busca_passo() || miniatura_passo()) { if (sujo) desenhar_tudo(); }
            if (conf_sujo && tempo_ms() - conf_t > 800) conf_gravar();
        }
    }
    registrar("visorpdf: fechado\n");
    conf_gravar();
    liberar_documento();
    jan_fechar(j);
    return 0;
}
