// SPDX-License-Identifier: GPL-3.0-or-later
/* arara [archivo.arara] - diagramas al estilo de Visio, pero sencillos (D465, v8.5.6.1).
 *
 * Paletas de figuras (básicas, diagrama de flujo, red y organigrama) que se arrastran o se eligen y se sueltan en el
 * lienzo; conectores rectos y en ángulo recto que se pegan a los puntos de conexión y siguen a las figuras cuando se
 * mueven; selección con clic o rectángulo, mover, cambiar de tamaño con asas, texto dentro de las figuras (doble
 * clic), colores y grosor en el inspector, alinear y distribuir, orden de apilado, rejilla con ajuste, zoom,
 * deshacer y rehacer, copiar y pegar, varias páginas (pestañas) y exportar a PNG, SVG y PDF. El formato propio es
 * `.arara`, un JSON legible (arara_modelo.h).
 *
 * Reutiliza los componentes de la libsagui del editor de Capibara (D454): el lienzo con zoom y asas (edvista_t),
 * el inspector de propiedades, el selector de color con alfa, el deshacer genérico y el arrastrar y soltar.
 * Todo en enteros. Es portable (D459): el mismo código compila con la capa userland/portable/ en Linux con SDL 2.
 *
 * Modo sin ventana, para las pruebas y los guiones:
 *   arara --abrir f.arara [--png f.png] [--svg f.svg] [--pdf f.pdf] [--info] [--guardar g.arara] */
#include "sagui.h"
#include "gui.h"
#include "icones.h"
#include "imagem.h"
#include "pdf_escribir.h"
#include "arara_modelo.h"
#include "arara_dibujo.h"

#define VENT_L 960
#define VENT_A 640
#define MENU_A 20
#define TB_A   36
#define EST_A  20
#define PAL_L  168
#define INS_L  212
#define CELDA_A 44
#define CON_BASE 100000          /* los identificadores de conector que devuelve «acertar» */

/* ------------------------------------------------------------ acciones */

enum {
    A_NUEVO = 1, A_ABRIR, A_GUARDAR, A_GUARDAR_COMO, A_PNG, A_SVG, A_PDF, A_SALIR,
    A_DESHACER, A_REHACER, A_CORTAR, A_COPIAR, A_PEGAR, A_DUPLICAR, A_BORRAR, A_SELTODO,
    A_FRENTE, A_ATRAS, A_AL_IZQ, A_AL_CEN, A_AL_DER, A_AL_ARR, A_AL_MED, A_AL_ABA, A_DIS_H, A_DIS_V,
    A_H_SEL, A_H_RECTO, A_H_ANGULO, A_FLECHA_INI, A_FLECHA_FIN, A_INVERTIR,
    A_ZOOM_MAS, A_ZOOM_MENOS, A_ZOOM_100, A_AJUSTAR, A_REJILLA, A_IMAN,
    A_PAG_NUEVA, A_PAG_BORRAR, A_PAG_NOMBRE, A_ACERCA
};

static const char *const MENU_NOMBRES[7] = { N_("Archivo"), N_("Edición"), N_("Organizar"), N_("Conector"), N_("Ver"), N_("Página"), N_("Ayuda") };
static char txt_conector[200];
static const char *const MENU_TEXTOS[7] = {
    N_("Nuevo\tCtrl+N\nAbrir...\tCtrl+O\nGuardar\tCtrl+S\nGuardar como...\n-\nExportar a PNG...\nExportar a SVG...\nExportar a PDF...\n-\nSalir"),
    N_("Deshacer\tCtrl+Z\nRehacer\tCtrl+Y\n-\nCortar\tCtrl+X\nCopiar\tCtrl+C\nPegar\tCtrl+V\nDuplicar\tCtrl+D\nBorrar\tSupr\n-\nSeleccionar todo\tCtrl+A"),
    N_("Traer al frente\nEnviar atrás\n-\nAlinear a la izquierda\nAlinear al centro\nAlinear a la derecha\nAlinear arriba\nAlinear al medio\nAlinear abajo\n-\nDistribuir en horizontal\nDistribuir en vertical"),
    "",   /* se rehace con el estado de las flechas (txt_conector) */
    N_("Acercar\t+\nAlejar\t-\nZoom 100 %\nAjustar a la ventana\n-\nMostrar u ocultar la rejilla\nAjuste a la rejilla"),
    N_("Página nueva\nEliminar página\nCambiar el nombre..."),
    N_("Acerca de Arara")
};
static const int MENU_ACCIONES[7][12] = {
    { A_NUEVO, A_ABRIR, A_GUARDAR, A_GUARDAR_COMO, 0, A_PNG, A_SVG, A_PDF, 0, A_SALIR },
    { A_DESHACER, A_REHACER, 0, A_CORTAR, A_COPIAR, A_PEGAR, A_DUPLICAR, A_BORRAR, 0, A_SELTODO },
    { A_FRENTE, A_ATRAS, 0, A_AL_IZQ, A_AL_CEN, A_AL_DER, A_AL_ARR, A_AL_MED, A_AL_ABA, 0, A_DIS_H, A_DIS_V },
    { A_H_SEL, A_H_RECTO, A_H_ANGULO, 0, A_FLECHA_INI, A_FLECHA_FIN, A_INVERTIR },
    { A_ZOOM_MAS, A_ZOOM_MENOS, A_ZOOM_100, A_AJUSTAR, 0, A_REJILLA, A_IMAN },
    { A_PAG_NUEVA, A_PAG_BORRAR, A_PAG_NOMBRE },
    { A_ACERCA }
};

/* ------------------------------------------------------------ estado */

enum { H_SEL, H_RECTO, H_ANGULO };
enum { B_NUEVO, B_ABRIR, B_GUARDAR, B_DESH, B_REH, B_SEL, B_RECTO, B_ANGULO, B_FRENTE, B_ATRAS, B_REJILLA, B_IMAN, B_ZMENOS, B_ZMAS, B_N };
static const int TB_ACCION[B_N] = { A_NUEVO, A_ABRIR, A_GUARDAR, A_DESHACER, A_REHACER, A_H_SEL, A_H_RECTO, A_H_ANGULO, A_FRENTE, A_ATRAS, A_REJILLA, A_IMAN, A_ZOOM_MENOS, A_ZOOM_MAS };

static jan_t *jn;
static barra_menu_t menu;
static mbotao_t tb[B_N];
static abas_t abas;
static edvista_t vs;
static inspetor_t ins;
static ins_prop_t props[12];
static int prop_id[12];
static barra_estado_t est;
static combo_t cb_cat;
static const char *cat_lista[CAT_N];
static pilha_desfazer_t pilha;
static snap_t *ult;                    /* el estado después de la última orden registrada */
static int herr = H_SEL;
static int armada = -1;                /* figura elegida en la paleta, a la espera de un clic en el lienzo */
static int cat_sel = CAT_BASICAS;
static char ruta_arch[256];
static bool sucio, cargando_ins;
static bool nuevo_flecha_ini, nuevo_flecha_fin = true;
static char aviso[96];
static int edit_fig;                   /* identificador de la figura cuyo texto se escribe (0 = ninguna) */
static char edit_orig[TXT_FIG];
static bool pegar_hay;
static fig_t clip_fig[MAX_FIG];
static con_t clip_con[MAX_CON];
static int clip_nf, clip_nc, clip_veces;
static bool salir_pedido;

/* gesto de mover / escalar */
static bool g_activa, g_cambio;
static int g_x[MAX_FIG], g_y[MAX_FIG], g_l[MAX_FIG], g_a[MAX_FIG];

/* conector que se está trazando */
static bool cg_activo;
static int cg_de, cg_pde, cg_x0, cg_y0;       /* origen: figura (0 = suelto) y punto, o coordenadas */
static int cg_a, cg_pa, cg_x1, cg_y1;         /* destino actual */
static int hov_fig, hov_pto;                  /* punto de conexión resaltado */

static void redibujar(void);
static void log_conectores(void);
static void sync_sel(void);
static void cargar_inspetor(void);

static void dizer(const char *t) { copia_texto(aviso, t, sizeof aviso); }

/* ------------------------------------------------------------ selección */

static int n_sel_fig(void) { int n = 0; for (int i = 0; i < P()->nfig; i++) if (P()->fig[i].sel) n++; return n; }
static int n_sel_con(void) { int n = 0; for (int i = 0; i < P()->ncon; i++) if (P()->con[i].sel) n++; return n; }
static void sel_limpiar(void)
{
    for (int i = 0; i < P()->nfig; i++) P()->fig[i].sel = false;
    for (int i = 0; i < P()->ncon; i++) P()->con[i].sel = false;
}
static bool sel_bbox(int *x, int *y, int *l, int *a)
{
    bool hay = false;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    for (int i = 0; i < P()->nfig; i++) {
        const fig_t *f = &P()->fig[i];
        if (!f->sel) continue;
        if (!hay) { x0 = f->x; y0 = f->y; x1 = f->x + f->l; y1 = f->y + f->a; hay = true; continue; }
        if (f->x < x0) x0 = f->x;
        if (f->y < y0) y0 = f->y;
        if (f->x + f->l > x1) x1 = f->x + f->l;
        if (f->y + f->a > y1) y1 = f->y + f->a;
    }
    *x = x0; *y = y0; *l = x1 - x0; *a = y1 - y0;
    return hay;
}
static void sync_sel(void)
{
    int x, y, l, a;
    if (sel_bbox(&x, &y, &l, &a)) edvista_selecionar(&vs, x, y, l, a, 0);
    else edvista_selecionar(&vs, 0, 0, 0, 0, 0);
}
static void sel_cambio(void)
{
    sync_sel();
    cargar_inspetor();
    registrar("arara: seleccion %d figuras %d conectores\n", n_sel_fig(), n_sel_con());
}

/* ------------------------------------------------------------ historial */

typedef struct { snap_t *antes, *despues; } cmd_t;

static void tras_restaurar(void)
{
    edit_fig = 0; cg_activo = false;
    sync_sel();
    cargar_inspetor();
    for (int i = 0; i < npag; i++) copia_texto(abas.titulos[i], pag[i].nombre, sizeof abas.titulos[i]);
    abas.n = npag; abas.sel = cur;
    vs.zoom = P()->vzoom; vs.ox = P()->vox; vs.oy = P()->voy;
    sucio = true;
    if (P()->nfig + P()->ncon <= 24) log_conectores();
}
static void cmd_aplicar(void *d, bool refazer)
{
    cmd_t *c = d;
    snap_t *s = refazer ? c->despues : c->antes;
    snap_aplicar(s);
    snap_soltar(ult);
    ult = snap_ref(s);
    tras_restaurar();
}
static void cmd_liberar(void *d)
{
    cmd_t *c = d;
    snap_soltar(c->antes); snap_soltar(c->despues);
    libera(c);
}
static void historial_reiniciar(void)
{
    ed_desfazer_limpar(&pilha);
    snap_soltar(ult);
    ult = snap_tomar();
}
/* Registra lo que acaba de pasar como una orden que se puede deshacer. */
static void commit(const char *nombre)
{
    snap_t *n = snap_tomar();
    cmd_t *c = aloca(sizeof *c);
    if (!n || !c) { snap_soltar(n); libera(c); dizer(_("Sin memoria para deshacer")); return; }
    c->antes = ult; c->despues = n;
    ult = snap_ref(n);
    if (!ed_desfazer_registrar(&pilha, nombre, cmd_aplicar, cmd_liberar, c)) cmd_liberar(c);
    sucio = true;
    registrar("arara: orden %s (%d figuras %d conectores)\n", nombre, P()->nfig, P()->ncon);
    if (P()->nfig + P()->ncon <= 24) log_conectores();
}

/* ------------------------------------------------------------ acciones sobre el modelo */

static int cerca_de_pto(const fig_t *f, int wx, int wy, int *pt)    /* punto de conexión más cercano de f */
{
    int mejor = 0x7FFFFFFF;
    for (int k = 0; k < 4; k++) {
        int x, y;
        fig_punto(f, k, &x, &y, NULL);
        int d = (x - wx) * (x - wx) + (y - wy) * (y - wy);
        if (d < mejor) { mejor = d; *pt = k; }
    }
    return mejor;
}

/* Figura bajo el punto (la de más arriba) con un poco de margen para los puntos de conexión. */
static int fig_en(int wx, int wy, int margen)
{
    pag_t *p = P();
    for (int i = p->nfig - 1; i >= 0; i--) {
        const fig_t *f = &p->fig[i];
        if (wx >= f->x - margen && wy >= f->y - margen && wx <= f->x + f->l + margen && wy <= f->y + f->a + margen) return i;
    }
    return -1;
}

static void borrar_sel(void)
{
    pag_t *p = P();
    if (!n_sel_fig() && !n_sel_con()) return;
    for (int i = 0; i < p->ncon; i++) {          /* los conectores de figuras borradas quedan sueltos donde estaban */
        con_t *c = &p->con[i];
        int fa = c->de ? fig_idx(p, c->de) : -1, fb = c->a ? fig_idx(p, c->a) : -1;
        bool ba = fa >= 0 && p->fig[fa].sel, bb = fb >= 0 && p->fig[fb].sel;
        if (!ba && !bb) continue;
        int x0, y0, d0, x1, y1, d1;
        con_extremos(p, c, &x0, &y0, &d0, &x1, &y1, &d1);
        if (ba) { c->de = 0; c->x0 = x0; c->y0 = y0; }
        if (bb) { c->a = 0; c->x1 = x1; c->y1 = y1; }
    }
    int m = 0;
    for (int i = 0; i < p->nfig; i++) if (!p->fig[i].sel) p->fig[m++] = p->fig[i];
    p->nfig = m;
    m = 0;
    for (int i = 0; i < p->ncon; i++) if (!p->con[i].sel) p->con[m++] = p->con[i];
    p->ncon = m;
    sel_cambio();
    commit(_("Borrar"));
}

static void copiar_sel(void)
{
    pag_t *p = P();
    clip_nf = clip_nc = 0;
    for (int i = 0; i < p->nfig; i++) if (p->fig[i].sel) clip_fig[clip_nf++] = p->fig[i];
    if (!clip_nf && !n_sel_con()) return;
    for (int i = 0; i < p->ncon; i++) {
        const con_t *c = &p->con[i];
        bool a = !c->de, b = !c->a;
        for (int k = 0; k < clip_nf; k++) { if (clip_fig[k].id == c->de) a = true; if (clip_fig[k].id == c->a) b = true; }
        bool ambos_figuras = c->de && c->a;
        if (c->sel ? (a && b) : (ambos_figuras && a && b)) clip_con[clip_nc++] = *c;
    }
    pegar_hay = clip_nf || clip_nc;
    clip_veces = 0;
    registrar("arara: copiado %d figuras %d conectores\n", clip_nf, clip_nc);
}

static void pegar_clip(void)
{
    pag_t *p = P();
    if (!pegar_hay) return;
    if (p->nfig + clip_nf > MAX_FIG || p->ncon + clip_nc > MAX_CON) { dizer(_("La página está llena")); return; }
    clip_veces++;
    int off = 16 * clip_veces;
    int viejo[MAX_FIG], nuevo[MAX_FIG];
    sel_limpiar();
    for (int i = 0; i < clip_nf; i++) {
        fig_t *f = &p->fig[p->nfig++];
        *f = clip_fig[i];
        viejo[i] = f->id; f->id = p->sig_id++; nuevo[i] = f->id;
        f->x = mundo(f->x + off); f->y = mundo(f->y + off);
        f->sel = true;
    }
    for (int i = 0; i < clip_nc; i++) {
        con_t *c = &p->con[p->ncon++];
        *c = clip_con[i];
        c->id = p->sig_id++;
        for (int k = 0; k < clip_nf; k++) { if (c->de == viejo[k]) c->de = nuevo[k]; if (c->a == viejo[k]) c->a = nuevo[k]; }
        if (c->de && fig_idx(p, c->de) < 0) c->de = 0;
        if (c->a && fig_idx(p, c->a) < 0) c->a = 0;
        if (!c->de) { c->x0 = mundo(c->x0 + off); c->y0 = mundo(c->y0 + off); }
        if (!c->a) { c->x1 = mundo(c->x1 + off); c->y1 = mundo(c->y1 + off); }
        c->sel = true;
    }
    sel_cambio();
    commit(_("Pegar"));
}

static void alinear(int modo)
{
    pag_t *p = P();
    int x, y, l, a;
    if (n_sel_fig() < 2 || !sel_bbox(&x, &y, &l, &a)) { dizer(_("Elige al menos dos figuras")); return; }
    for (int i = 0; i < p->nfig; i++) {
        fig_t *f = &p->fig[i];
        if (!f->sel) continue;
        switch (modo) {
        case A_AL_IZQ: f->x = x; break;
        case A_AL_CEN: f->x = x + (l - f->l) / 2; break;
        case A_AL_DER: f->x = x + l - f->l; break;
        case A_AL_ARR: f->y = y; break;
        case A_AL_MED: f->y = y + (a - f->a) / 2; break;
        default: f->y = y + a - f->a; break;
        }
    }
    sync_sel();
    cargar_inspetor();
    commit(_("Alinear"));
}

static void distribuir(bool horiz)
{
    pag_t *p = P();
    int idx[MAX_FIG], n = 0;
    for (int i = 0; i < p->nfig; i++) if (p->fig[i].sel) idx[n++] = i;
    if (n < 3) { dizer(_("Elige al menos tres figuras")); return; }
    for (int a = 1; a < n; a++) {                      /* ordena por posición */
        int v = idx[a], b = a - 1;
        while (b >= 0 && (horiz ? p->fig[idx[b]].x > p->fig[v].x : p->fig[idx[b]].y > p->fig[v].y)) { idx[b + 1] = idx[b]; b--; }
        idx[b + 1] = v;
    }
    fig_t *p0 = &p->fig[idx[0]], *pn = &p->fig[idx[n - 1]];
    int ini = horiz ? p0->x : p0->y, fin = horiz ? pn->x + pn->l : pn->y + pn->a, suma = 0;
    for (int k = 0; k < n; k++) suma += horiz ? p->fig[idx[k]].l : p->fig[idx[k]].a;
    int hueco = (fin - ini - suma) / (n - 1), pos = ini;
    for (int k = 0; k < n; k++) {
        fig_t *f = &p->fig[idx[k]];
        if (horiz) { f->x = pos; pos += f->l + hueco; } else { f->y = pos; pos += f->a + hueco; }
    }
    sync_sel();
    cargar_inspetor();
    commit(_("Distribuir"));
}

static void orden_z(bool frente)
{
    pag_t *p = P();
    if (!n_sel_fig()) return;
    fig_t *tmp = aloca(sizeof(fig_t) * (size_t)p->nfig);
    if (!tmp) return;
    int m = 0;
    if (!frente) for (int i = 0; i < p->nfig; i++) if (p->fig[i].sel) tmp[m++] = p->fig[i];
    for (int i = 0; i < p->nfig; i++) if (!p->fig[i].sel) tmp[m++] = p->fig[i];
    if (frente) for (int i = 0; i < p->nfig; i++) if (p->fig[i].sel) tmp[m++] = p->fig[i];
    memcpy(p->fig, tmp, sizeof(fig_t) * (size_t)p->nfig);
    libera(tmp);
    commit(frente ? _("Traer al frente") : _("Enviar atrás"));
}

static void nueva_figura(int tipo, int wx, int wy)
{
    pag_t *p = P();
    int x = edvista_iman(&vs, wx - TIPOS[tipo].l / 2), y = edvista_iman(&vs, wy - TIPOS[tipo].a / 2);
    int i = fig_nueva(p, tipo, x, y);
    if (i < 0) { dizer(_("La página está llena")); return; }
    sel_limpiar();
    p->fig[i].sel = true;
    sel_cambio();
    commit(_("Insertar figura"));
    registrar("arara: figura %d %s en %d %d %d %d\n", p->fig[i].id, TIPOS[tipo].clave, p->fig[i].x, p->fig[i].y, p->fig[i].l, p->fig[i].a);
}

/* ------------------------------------------------------------ vista: zoom */

static void vista_guardar(void) { P()->vzoom = vs.zoom; P()->vox = vs.ox; P()->voy = vs.oy; }
static void zoom_centro(int z) { edvista_zoom(&vs, z, vs.x + vs.l / 2, vs.y + vs.a / 2); if (vs.zoom > 4000) edvista_zoom(&vs, 4000, vs.x + vs.l / 2, vs.y + vs.a / 2); }
static void ajustar_vista(void)
{
    int x0, y0, x1, y1;
    if (!pag_limites(P(), &x0, &y0, &x1, &y1)) { vs.zoom = 1000; vs.ox = 40; vs.oy = 40; return; }
    int w = x1 - x0 + 80, h = y1 - y0 + 80;
    int z = vs.l * 1000 / w, z2 = vs.a * 1000 / h;
    if (z2 < z) z = z2;
    z = limitar(z, EDVISTA_ZOOM_MIN, 2000);
    vs.zoom = z;
    edvista_centrar(&vs, (x0 + x1) / 2, (y0 + y1) / 2);
}

/* ------------------------------------------------------------ ficheros */

static char *leer_texto(const char *c, int *n)
{
    int fd = abrir(c, ABRIR_LER);
    if (fd < 0) return NULL;
    int cap = 65536, tam = 0, r;
    char *b = aloca((size_t)cap);
    while (b && (r = ler(fd, b + tam, (unsigned)(cap - tam - 1))) > 0) {
        tam += r;
        if (tam >= cap - 1) { char *nb = realoca(b, (size_t)cap * 2); if (!nb) { libera(b); b = NULL; break; } b = nb; cap *= 2; }
    }
    fechar(fd);
    if (b) b[tam] = 0;
    *n = tam;
    return b;
}
static bool escribir_bytes(const char *c, const void *d, int n)
{
    int fd = abrir(c, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return false;
    int r = escrever_tudo(fd, d, (unsigned)n);
    fechar(fd);
    return r == n;
}
static void titulo(void)
{
    char t[300];
    const char *n = ruta_arch[0] ? acha_ultimo_char(ruta_arch, '/') : NULL;
    formatar_texto(t, sizeof t, "%s%s - Arara", n ? n + 1 : ruta_arch[0] ? ruta_arch : _("Sin título"), sucio ? "*" : "");
    if (jn) titulo_janela(jn->id, t);
}

static bool guardar_en(const char *c)
{
    buf_t b;
    memset(&b, 0, sizeof b);
    arara_serializar(&b);
    bool ok = !b.err && escribir_bytes(c, b.p, b.n);
    int tam = b.n;
    libera(b.p);
    if (!ok) { mensagem("Arara", _("No se ha podido guardar el archivo.")); return false; }
    copia_texto(ruta_arch, c, sizeof ruta_arch);
    sucio = false;
    int nf = 0, nc = 0;
    for (int i = 0; i < npag; i++) { nf += pag[i].nfig; nc += pag[i].ncon; }
    registrar("arara: guardado %s %d bytes %d paginas %d figuras %d conectores\n", c, tam, npag, nf, nc);
    char t[160];
    formatar_texto(t, sizeof t, "%s: %s", _("Guardado"), c);
    dizer(t);
    return true;
}
static bool guardar_como(void)
{
    char c[256];
    copia_texto(c, ruta_arch[0] ? ruta_arch : "/home/diagrama.arara", sizeof c);
    if (!dialogo_guardar(_("Guardar como"), _("Diagramas de Arara (*.arara)|*.arara|Todos los archivos (*.*)|*"), c, c, sizeof c) || !c[0]) return false;
    return guardar_en(c);
}
static bool guardar(void) { return ruta_arch[0] ? guardar_en(ruta_arch) : guardar_como(); }

static bool abrir_en(const char *c)
{
    int n;
    char *t = leer_texto(c, &n);
    if (!t) return false;
    int rc = arara_cargar(t);
    libera(t);
    if (rc != 0) return false;
    copia_texto(ruta_arch, c, sizeof ruta_arch);
    sucio = false; edit_fig = 0; cg_activo = false;
    for (int i = 0; i < npag; i++) { pag[i].vzoom = 1000; pag[i].vox = 40; pag[i].voy = 40; }
    int nf = 0, nc = 0;
    for (int i = 0; i < npag; i++) { nf += pag[i].nfig; nc += pag[i].ncon; }
    registrar("arara: abierto %s %d paginas %d figuras %d conectores\n", c, npag, nf, nc);
    return true;
}

static bool pregunta_cambios(void)
{
    if (!sucio) return true;
    if (confirmar("Arara", _("El diagrama tiene cambios sin guardar.\n¿Guardar antes?"))) return guardar();
    return true;
}

static int exportar_png(const char *c, int max_px)
{
    imagem_t img;
    if (!pag_a_imagen(P(), 2, max_px, &img)) return -1;
    uint8_t *d = NULL;
    int n = 0;
    int rc = imagem_codificar_png(&img, &d, &n);
    int l = img.larg, a = img.alt;
    imagem_liberar(&img);
    if (rc < 0 || !d) return -1;
    bool ok = escribir_bytes(c, d, n);
    libera(d);
    if (ok) registrar("arara: png %s %dx%d %d bytes\n", c, l, a, n);
    return ok ? 0 : -1;
}
static int exportar_svg(const char *c)
{
    buf_t b;
    memset(&b, 0, sizeof b);
    pag_a_svg(P(), &b);
    bool ok = !b.err && escribir_bytes(c, b.p, b.n);
    int n = b.n;
    libera(b.p);
    if (ok) registrar("arara: svg %s %d bytes\n", c, n);
    return ok ? 0 : -1;
}
static int exportar_pdf(const char *c)
{
    pdfw_t *w = pdfw_criar(PDFW_A4);
    if (!w) return -1;
    pdfw_tamanho(w, 16838, 11906);                      /* A4 apaisado */
    pdfw_titulo(w, ruta_arch[0] ? ruta_arch : "Arara");
    for (int k = 0; k < npag; k++) {
        imagem_t img;
        if (!pdfw_pagina(w)) break;
        if (!pag_a_imagen(&pag[k], 2, 2 * 1024 * 1024, &img)) { pdfw_liberar(w); return -1; }
        int margen = 700, dl = 16838 - 2 * margen, da = 11906 - 2 * margen;
        int l = dl, a = img.alt * dl / img.larg;
        if (a > da) { a = da; l = img.larg * da / img.alt; }
        pdfw_imagem_rgb(w, (16838 - l) / 2, (11906 - a) / 2, l, a, img.px, img.larg, img.alt);
        imagem_liberar(&img);
    }
    uint8_t *d;
    int n;
    bool ok = pdfw_finalizar(w, &d, &n);
    int np = pdfw_paginas(w);
    pdfw_liberar(w);
    if (!ok) return -1;
    ok = escribir_bytes(c, d, n);
    libera(d);
    if (ok) registrar("arara: pdf %s %d bytes %d paginas\n", c, n, np);
    return ok ? 0 : -1;
}

static void exportar(int accion)
{
    char c[256];
    const char *ext = accion == A_PNG ? ".png" : accion == A_SVG ? ".svg" : ".pdf";
    const char *flt = accion == A_PNG ? _("Imagen PNG (*.png)|*.png") : accion == A_SVG ? _("Imagen SVG (*.svg)|*.svg") : _("Documento PDF (*.pdf)|*.pdf");
    if (ruta_arch[0]) {                                  /* propone el mismo nombre con otra extensión */
        copia_texto(c, ruta_arch, sizeof c);
        char *pto = acha_ultimo_char(c, '.');
        if (pto && !acha_char(pto, '/')) *pto = 0;
        junta_texto(c, ext, sizeof c);
    } else formatar_texto(c, sizeof c, "/home/diagrama%s", ext);
    if (!dialogo_guardar(_("Exportar"), flt, c, c, sizeof c) || !c[0]) return;
    int rc = accion == A_PNG ? exportar_png(c, EXPORT_PX_MAX) : accion == A_SVG ? exportar_svg(c) : exportar_pdf(c);
    if (rc != 0) mensagem("Arara", _("No se ha podido exportar el diagrama."));
    else { char t[160]; formatar_texto(t, sizeof t, "%s: %s", _("Exportado"), c); dizer(t); }
}

/* ------------------------------------------------------------ inspector */

enum { PR_PAG_SEC, PR_PAG_NOMBRE, PR_PAG_REJ, PR_PAG_IMAN,
       PR_FIG_SEC, PR_TEXTO, PR_X, PR_Y, PR_L, PR_A, PR_EST_SEC, PR_RELLENO, PR_LINEA, PR_GROSOR,
       PR_CON_SEC, PR_CTIPO, PR_CFINI, PR_CFFIN, PR_CCOLOR, PR_CGROSOR };
static const char *const CON_TIPOS[2] = { N_("Recto"), N_("En ángulo recto") };

static void cargar_inspetor(void)
{
    cargando_ins = true;
    int n = 0, nf = n_sel_fig(), nc = n_sel_con();
    const fig_t *f1 = NULL;
    const con_t *c1 = NULL;
    for (int i = 0; i < P()->nfig; i++) if (P()->fig[i].sel) { f1 = &P()->fig[i]; break; }
    for (int i = 0; i < P()->ncon; i++) if (P()->con[i].sel) { c1 = &P()->con[i]; break; }
    if (nf == 0 && nc == 0) {
        prop_id[n] = PR_PAG_SEC; inspetor_secao(&props[n++], N_("Página"), true);
        prop_id[n] = PR_PAG_NOMBRE; inspetor_texto(&props[n++], N_("Nombre"), P()->nombre);
        prop_id[n] = PR_PAG_REJ; inspetor_int(&props[n++], N_("Rejilla"), vs.grade, 0, 128, 4);
        prop_id[n] = PR_PAG_IMAN; inspetor_bool(&props[n++], N_("Ajuste"), vs.iman);
    } else {
        if (nf > 0) {
            prop_id[n] = PR_FIG_SEC; inspetor_secao(&props[n++], N_("Figura"), true);
            if (nf == 1) {
                prop_id[n] = PR_TEXTO; inspetor_texto(&props[n++], N_("Texto"), f1->texto);
                prop_id[n] = PR_X; inspetor_int(&props[n++], N_("Posición X"), f1->x, -MUNDO_MAX, MUNDO_MAX, 1);
                prop_id[n] = PR_Y; inspetor_int(&props[n++], N_("Posición Y"), f1->y, -MUNDO_MAX, MUNDO_MAX, 1);
                prop_id[n] = PR_L; inspetor_int(&props[n++], N_("Ancho"), f1->l, 8, 4000, 1);
                prop_id[n] = PR_A; inspetor_int(&props[n++], N_("Alto"), f1->a, 8, 4000, 1);
            }
            prop_id[n] = PR_EST_SEC; inspetor_secao(&props[n++], N_("Estilo"), true);
            prop_id[n] = PR_RELLENO; inspetor_cor(&props[n++], N_("Relleno"), f1->relleno, true);
            prop_id[n] = PR_LINEA; inspetor_cor(&props[n++], N_("Línea"), f1->linea, true);
            prop_id[n] = PR_GROSOR; inspetor_int(&props[n++], N_("Grosor"), f1->grosor, 1, 12, 1);
        } else {
            prop_id[n] = PR_CON_SEC; inspetor_secao(&props[n++], N_("Conector"), true);
            prop_id[n] = PR_CTIPO; inspetor_enum(&props[n++], N_("Tipo"), c1->tipo, CON_TIPOS, 2);
            prop_id[n] = PR_CFINI; inspetor_bool(&props[n++], N_("Flecha inicial"), c1->flecha_ini);
            prop_id[n] = PR_CFFIN; inspetor_bool(&props[n++], N_("Flecha final"), c1->flecha_fin);
            prop_id[n] = PR_CCOLOR; inspetor_cor(&props[n++], N_("Color"), c1->linea, true);
            prop_id[n] = PR_CGROSOR; inspetor_int(&props[n++], N_("Grosor"), c1->grosor, 1, 12, 1);
        }
    }
    ins.p = props; ins.n = n;
    if (ins.topo > 0) ins.topo = 0;
    cargando_ins = false;
}

static void ins_mudou(void *ctx, inspetor_t *s, int i)
{
    (void)ctx; (void)s;
    if (cargando_ins || i < 0) return;
    pag_t *p = P();
    ins_prop_t *q = &props[i];
    const char *nombre = _("Propiedad");
    switch (prop_id[i]) {
    case PR_PAG_NOMBRE: copia_texto(p->nombre, q->texto, sizeof p->nombre); copia_texto(abas.titulos[cur], p->nombre, sizeof abas.titulos[cur]); nombre = _("Cambiar el nombre de la página"); break;
    case PR_PAG_REJ: vs.grade = q->valor; nombre = _("Rejilla"); break;
    case PR_PAG_IMAN: vs.iman = q->valor != 0; return;
    case PR_CTIPO: for (int k = 0; k < p->ncon; k++) if (p->con[k].sel) p->con[k].tipo = q->valor; nombre = _("Tipo de conector"); break;
    case PR_CFINI: for (int k = 0; k < p->ncon; k++) if (p->con[k].sel) p->con[k].flecha_ini = q->valor != 0; nombre = _("Flechas"); break;
    case PR_CFFIN: for (int k = 0; k < p->ncon; k++) if (p->con[k].sel) p->con[k].flecha_fin = q->valor != 0; nombre = _("Flechas"); break;
    case PR_CCOLOR: for (int k = 0; k < p->ncon; k++) if (p->con[k].sel) p->con[k].linea = (uint32_t)q->valor; nombre = _("Color"); break;
    case PR_CGROSOR: for (int k = 0; k < p->ncon; k++) if (p->con[k].sel) p->con[k].grosor = q->valor; nombre = _("Grosor"); break;
    case PR_TEXTO: for (int k = 0; k < p->nfig; k++) if (p->fig[k].sel) copia_texto(p->fig[k].texto, q->texto, sizeof p->fig[k].texto); nombre = _("Editar texto"); break;
    case PR_X: case PR_Y: case PR_L: case PR_A:
        for (int k = 0; k < p->nfig; k++) {
            fig_t *f = &p->fig[k];
            if (!f->sel) continue;
            if (prop_id[i] == PR_X) f->x = mundo(q->valor); else if (prop_id[i] == PR_Y) f->y = mundo(q->valor);
            else if (prop_id[i] == PR_L) f->l = q->valor; else f->a = q->valor;
        }
        sync_sel();
        nombre = _("Mover");
        break;
    case PR_RELLENO: for (int k = 0; k < p->nfig; k++) if (p->fig[k].sel) p->fig[k].relleno = (uint32_t)q->valor; nombre = _("Color de relleno"); break;
    case PR_LINEA:
        for (int k = 0; k < p->nfig; k++) if (p->fig[k].sel) p->fig[k].linea = (uint32_t)q->valor;
        nombre = _("Color de línea");
        break;
    case PR_GROSOR:
        for (int k = 0; k < p->nfig; k++) if (p->fig[k].sel) p->fig[k].grosor = q->valor;
        nombre = _("Grosor");
        break;
    default: return;
    }
    registrar("arara: propiedad %d valor %d\n", prop_id[i], q->valor);
    commit(nombre);
}

/* ------------------------------------------------------------ dibujo del lienzo */

static void cb_desenhar(void *ctx, jan_t *j, edvista_t *v)
{
    (void)ctx;
    pag_t *p = P();
    int ts = (v->zoom + 500) / 1000;
    if (ts < 1) ts = 1;
    if (ts > 3) ts = 3;
    rd_t r = { j->px, j->larg, v->x, v->y, v->x + v->l, v->y + v->a, v->x + v->ox, v->y + v->oy, v->zoom, ts };
    if (r.cx1 > j->larg) r.cx1 = j->larg;
    if (r.cy1 > j->alt) r.cy1 = j->alt;
    sink_t s = rd_sink(&r);
    for (int i = 0; i < p->nfig; i++) {
        const fig_t *f = &p->fig[i];
        if (f->id == edit_fig) {
            fig_t t = *f;
            junta_texto(t.texto, "|", sizeof t.texto);
            dib_fig(&s, &t);
        } else dib_fig(&s, f);
    }
    for (int i = 0; i < p->ncon; i++) {
        const con_t *c = &p->con[i];
        if (c->sel) {                                                   /* halo del conector elegido */
            int pts[RUTA_MAX][2], xy[2 * RUTA_MAX];
            int n = con_ruta(p, c, pts);
            for (int k = 0; k < n; k++) { xy[2 * k] = pts[k][0]; xy[2 * k + 1] = pts[k][1]; }
            s.abierta(s.ctx, xy, n, 0x80000000u | (mod_tema.acento & 0xFFFFFF), c->grosor + 5);
        }
        dib_con(&s, p, c);
    }
    for (int i = 0; i < p->nfig; i++) {                                 /* contorno de las figuras elegidas */
        const fig_t *f = &p->fig[i];
        if (!f->sel) continue;
        int x0 = rd_x(&r, f->x) - 1, y0 = rd_y(&r, f->y) - 1, x1 = rd_x(&r, f->x + f->l) + 1, y1 = rd_y(&r, f->y + f->a) + 1;
        rd_linea1(&r, x0, y0, x1, y0, mod_tema.acento | 0xFF000000u); rd_linea1(&r, x1, y0, x1, y1, mod_tema.acento | 0xFF000000u);
        rd_linea1(&r, x1, y1, x0, y1, mod_tema.acento | 0xFF000000u); rd_linea1(&r, x0, y1, x0, y0, mod_tema.acento | 0xFF000000u);
    }
    if (herr != H_SEL) {                                                /* puntos de conexión visibles con la herramienta de conectores */
        for (int i = 0; i < p->nfig; i++)
            for (int k = 0; k < 4; k++) {
                int x, y;
                fig_punto(&p->fig[i], k, &x, &y, NULL);
                bool h = hov_fig == p->fig[i].id && hov_pto == k;
                rd_disco(&r, rd_x(&r, x), rd_y(&r, y), h ? 6 : 4, h ? 6 : 4, h ? 0xFFE0731A : 0xFF2F7FD0);
                rd_disco(&r, rd_x(&r, x), rd_y(&r, y), h ? 3 : 2, h ? 3 : 2, 0xFFFFFFFF);
            }
        if (cg_activo) {
            int x0 = cg_x0, y0 = cg_y0, x1 = cg_x1, y1 = cg_y1;
            if (cg_de) { int i = fig_idx(p, cg_de); if (i >= 0) fig_punto(&p->fig[i], cg_pde, &x0, &y0, NULL); }
            if (cg_a) { int i = fig_idx(p, cg_a); if (i >= 0) fig_punto(&p->fig[i], cg_pa, &x1, &y1, NULL); }
            con_t t;
            memset(&t, 0, sizeof t);
            t.tipo = herr == H_ANGULO ? C_ANGULO : C_RECTO;
            t.de = cg_de; t.pde = cg_pde; t.a = cg_a; t.pa = cg_pa;
            t.x0 = x0; t.y0 = y0; t.x1 = x1; t.y1 = y1;
            t.flecha_ini = nuevo_flecha_ini; t.flecha_fin = nuevo_flecha_fin;
            t.linea = 0xFFE0731A; t.grosor = 2;
            dib_con(&s, p, &t);
        }
    }
}

static int cb_acertar(void *ctx, int wx, int wy)
{
    (void)ctx;
    pag_t *p = P();
    int tope = 5000 / vs.zoom;
    if (tope < 3) tope = 3;
    for (int i = p->ncon - 1; i >= 0; i--) if (con_acierta(p, &p->con[i], wx, wy, tope + p->con[i].grosor / 2)) return CON_BASE + p->con[i].id;
    for (int i = p->nfig - 1; i >= 0; i--) if (fig_acierta(&p->fig[i], wx, wy)) return p->fig[i].id;
    return -1;
}

static void gesto_empezar(void)
{
    pag_t *p = P();
    g_activa = true; g_cambio = false;
    for (int i = 0; i < p->nfig; i++) { g_x[i] = p->fig[i].x; g_y[i] = p->fig[i].y; g_l[i] = p->fig[i].l; g_a[i] = p->fig[i].a; }
}

static void cb_aviso(void *ctx, int ev, int a, int b, int c, int d, bool fim)
{
    (void)ctx;
    pag_t *p = P();
    switch (ev) {
    case EDVISTA_E_CLIQUE: {
        bool mayus = (d & 1) != 0;
        if (a >= CON_BASE) {
            int i = con_idx(p, a - CON_BASE);
            if (!mayus) sel_limpiar();
            if (i >= 0) p->con[i].sel = mayus ? !p->con[i].sel : true;
        } else if (a >= 0) {
            int i = fig_idx(p, a);
            if (i >= 0) {
                if (mayus) p->fig[i].sel = !p->fig[i].sel;
                else if (!p->fig[i].sel) { sel_limpiar(); p->fig[i].sel = true; }
            }
        } else if (!mayus) sel_limpiar();
        sel_cambio();
        break;
    }
    case EDVISTA_E_RET: {
        if (!fim) break;
        for (int i = 0; i < p->nfig; i++) {
            const fig_t *f = &p->fig[i];
            if (f->x >= a && f->y >= b && f->x + f->l <= c && f->y + f->a <= d) p->fig[i].sel = true;
        }
        for (int i = 0; i < p->ncon; i++) {
            int pts[RUTA_MAX][2];
            int n = con_ruta(p, &p->con[i], pts);
            bool dentro = true;
            for (int k = 0; k < n; k++) if (pts[k][0] < a || pts[k][0] > c || pts[k][1] < b || pts[k][1] > d) dentro = false;
            if (dentro) p->con[i].sel = true;
        }
        sel_cambio();
        break;
    }
    case EDVISTA_E_MOVER: {
        if (!g_activa) gesto_empezar();
        int dx = a - vs.s0x, dy = b - vs.s0y;
        for (int i = 0; i < p->nfig; i++) if (p->fig[i].sel) { p->fig[i].x = mundo(g_x[i] + dx); p->fig[i].y = mundo(g_y[i] + dy); }
        if (dx || dy) g_cambio = true;
        if (fim) {
            g_activa = false;
            cargar_inspetor();
            if (g_cambio) { registrar("arara: mover %d %d\n", dx, dy); commit(_("Mover")); }
        }
        break;
    }
    case EDVISTA_E_ESCALAR: {
        if (!g_activa) gesto_empezar();
        int ol = vs.s0l > 0 ? vs.s0l : 1, oa = vs.s0a > 0 ? vs.s0a : 1;
        for (int i = 0; i < p->nfig; i++) {
            if (!p->fig[i].sel) continue;
            fig_t *f = &p->fig[i];
            f->x = mundo(a + (g_x[i] - vs.s0x) * c / ol);
            f->y = mundo(b + (g_y[i] - vs.s0y) * d / oa);
            f->l = limitar(g_l[i] * c / ol, 8, 4000);
            f->a = limitar(g_a[i] * d / oa, 8, 4000);
        }
        g_cambio = true;
        if (fim) { g_activa = false; cargar_inspetor(); commit(_("Cambiar tamaño")); }
        break;
    }
    case EDVISTA_E_GIRAR:
        vs.srot = 0;                           /* Arara no gira figuras */
        break;
    case EDVISTA_E_ZOOM:
        if (vs.zoom > 4000) edvista_zoom(&vs, 4000, vs.x + vs.l / 2, vs.y + vs.a / 2);
        break;
    default: break;
    }
}

/* ------------------------------------------------------------ paleta */

static void celda_rect(int k, int *x, int *y, int *l, int *a)
{
    *x = 2; *l = PAL_L - 4; *a = CELDA_A - 2;
    *y = cb_cat.y + cb_cat.a + 8 + k * CELDA_A;
}
static int tipo_de_celda(int k)       /* k-ésima figura de la categoría elegida */
{
    for (int t = 0, n = 0; t < T_N; t++) if (TIPOS[t].cat == cat_sel) { if (n == k) return t; n++; }
    return -1;
}
static int celda_en(int x, int y)
{
    for (int k = 0; k < 8; k++) {
        int cx, cy, cl, ca;
        celda_rect(k, &cx, &cy, &cl, &ca);
        if (x >= cx && x < cx + cl && y >= cy && y < cy + ca) return tipo_de_celda(k);
    }
    return -1;
}

static void dibujar_paleta(void)
{
    g_retangulo(jn, 0, MENU_A + TB_A, PAL_L, jn->alt - MENU_A - TB_A - EST_A, mod_tema.barra);
    g_retangulo(jn, PAL_L - 1, MENU_A + TB_A, 1, jn->alt - MENU_A - TB_A - EST_A, mod_tema.borde);
    combo_desenhar(jn, &cb_cat);
    for (int k = 0; k < 8; k++) {
        int t = tipo_de_celda(k);
        if (t < 0) break;
        int x, y, l, a;
        celda_rect(k, &x, &y, &l, &a);
        g_caixa_red(jn, x, y, l, a, 4, t == armada ? mod_tema.acento_suave : mod_tema.superficie, t == armada ? mod_tema.acento : mod_tema.borde);
        fig_t f;
        memset(&f, 0, sizeof f);
        f.tipo = t; f.l = TIPOS[t].l; f.a = TIPOS[t].a; f.grosor = 2;
        f.relleno = TIPOS[t].relleno; f.linea = TIPOS[t].linea;
        if (t == T_TEXTO) { f.relleno = 0xFFF2F2F2; f.linea = 0xFF999999; }
        int z = 60000 / f.l, z2 = 32000 / f.a;
        if (z2 < z) z = z2;
        int px = x + 4 + (60 - f.l * z / 1000) / 2, py = y + 3 + (34 - f.a * z / 1000) / 2;
        rd_t r = { jn->px, jn->larg, x + 3, y + 2, x + 66, y + a - 2, px, py, z, 1 };
        sink_t s = rd_sink(&r);
        dib_fig(&s, &f);
        char lin[LIN_MAX][LIN_LONG];
        int nl = texto_lineas(_(TIPOS[t].nombre), (l - 72) / LETRA_L, lin);
        if (nl > 2) nl = 2;
        for (int i = 0; i < nl; i++) g_texto(jn, x + 70, y + (a - nl * 16) / 2 + i * 16, lin[i], mod_tema.texto);
    }
    int y = cb_cat.y + cb_cat.a + 8 + 6 * CELDA_A + 4;
    char lin[LIN_MAX][LIN_LONG];
    int nl = texto_lineas(_("Arrastra una figura al lienzo o elígela y haz clic donde la quieras."), (PAL_L - 12) / LETRA_L, lin);
    for (int i = 0; i < nl && y + i * 16 < jn->alt - EST_A - 16; i++) g_texto(jn, 6, y + i * 16, lin[i], mod_tema.texto_fraco);
}

/* ------------------------------------------------------------ barras y estado */

static void herr_botones(void)
{
    tb[B_SEL].ativo = herr == H_SEL;
    tb[B_RECTO].ativo = herr == H_RECTO;
    tb[B_ANGULO].ativo = herr == H_ANGULO;
    tb[B_REJILLA].ativo = vs.grade > 0;
    tb[B_IMAN].ativo = vs.iman;
    tb[B_DESH].desabilitado = !ed_desfazer_pode(&pilha);
    tb[B_REH].desabilitado = !ed_refazer_pode(&pilha);
    tb[B_FRENTE].desabilitado = tb[B_ATRAS].desabilitado = n_sel_fig() == 0;
}

static void estado_texto(void)
{
    char t[96];
    formatar_texto(t, sizeof t, "%s %d %%", _("Zoom"), (vs.zoom + 5) / 10);
    barra_estado_texto(&est, 0, t);
    formatar_texto(t, sizeof t, "%s %d/%d", _("Página"), cur + 1, npag);
    barra_estado_texto(&est, 1, t);
    formatar_texto(t, sizeof t, "%d %s, %d %s", P()->nfig, _("figuras"), P()->ncon, _("conectores"));
    barra_estado_texto(&est, 2, t);
    barra_estado_texto(&est, 3, aviso[0] ? aviso : herr != H_SEL ? _("Arrastra de un punto de conexión a otro") : armada >= 0 ? _("Haz clic en el lienzo para colocarla") : _("Doble clic en una figura para escribir"));
}

static void layout(void)
{
    int W = jn->larg, H = jn->alt;
    barra_menu_iniciar(&menu, 0, 0, W);
    int bx = 4;
    for (int i = 0; i < B_N; i++) {
        tb[i].y = MENU_A + 4; tb[i].a = 28;
        mbotao_ajustar(&tb[i]);
        if (i == B_DESH || i == B_SEL || i == B_FRENTE || i == B_REJILLA || i == B_ZMENOS) bx += 8;
        tb[i].x = bx;
        bx += tb[i].l + 3;
    }
    int top = MENU_A + TB_A;
    abas.x = PAL_L; abas.y = top; abas.l = W - PAL_L - INS_L;
    edvista_mover(&vs, PAL_L, top + ABAS_ALTURA, W - PAL_L - INS_L, H - EST_A - top - ABAS_ALTURA);
    inspetor_iniciar(&ins, W - INS_L, top, INS_L, H - EST_A - top, props, ins.n);
    ins.mudou = ins_mudou; ins.col_nome = 108;
    combo_iniciar(&cb_cat, 4, top + 6, PAL_L - 8, cat_lista, CAT_N, false, 7461);
    cb_cat.sel = cat_sel;
    combo_definir(&cb_cat, cat_lista[cat_sel]);
    int larg[4] = { 90, 90, 190, 0 };
    barra_estado_iniciar(&est, 0, H - EST_A, W, 4, larg);
}

static void redibujar(void)
{
    herr_botones();
    estado_texto();
    titulo();
    g_retangulo(jn, 0, 0, jn->larg, jn->alt, mod_tema.barra);
    g_retangulo(jn, 0, MENU_A + TB_A - 1, jn->larg, 1, mod_tema.borde);
    barra_menu_desenhar(jn, &menu);
    for (int i = 0; i < B_N; i++) { tb[i].fundo = mod_tema.barra; mbotao_desenhar(jn, &tb[i]); }
    dibujar_paleta();
    abas_desenhar(jn, &abas);
    edvista_desenhar(jn, &vs);
    inspetor_desenhar(jn, &ins);
    barra_estado_desenhar(jn, &est);
    jan_mostrar(jn);
}
static void redibujar_lienzo(void)
{
    herr_botones();
    estado_texto();
    edvista_desenhar(jn, &vs);
    barra_estado_desenhar(jn, &est);
    jan_mostrar_area(jn, 0, jn->alt - EST_A, jn->larg, EST_A);
}

static void menu_conector_rehacer(void)
{
    formatar_texto(txt_conector, sizeof txt_conector, "%s\n%s\n%s\n-\n%s: %s\n%s: %s\n%s",
                   _("Puntero"), _("Conector recto"), _("Conector en ángulo recto"),
                   _("Flecha al inicio"), nuevo_flecha_ini ? _("sí") : _("no"), _("Flecha al final"), nuevo_flecha_fin ? _("sí") : _("no"),
                   _("Invertir los conectores elegidos"));
    menu.itens[3] = txt_conector;
}

static void log_posiciones(void)
{
    registrar("arara: pos lienzo %d %d %d %d\n", vs.x, vs.y, vs.l, vs.a);
    registrar("arara: pos inspector %d %d %d %d\n", ins.x, ins.y, ins.l, ins.a);
    registrar("arara: pos combo %d %d %d %d\n", cb_cat.x, cb_cat.y, cb_cat.l, cb_cat.a);
    for (int k = 0; k < 6; k++) { int x, y, l, a; celda_rect(k, &x, &y, &l, &a); registrar("arara: pos celda %d %d %d %d %d\n", k, x, y, l, a); }
    for (int i = 0; i < B_N; i++) registrar("arara: pos boton %d %d %d %d %d\n", i, tb[i].x, tb[i].y, tb[i].l, tb[i].a);
    for (int i = 0; i < menu.n; i++) registrar("arara: pos menu %d %d %d\n", i, menu.x_item[i], menu.l_item[i]);
    registrar("arara: pos abas %d %d %d\n", abas.x, abas.y, abas.l);
}
static void log_vista(void)
{
    registrar("arara: vista zoom %d origen %d %d\n", vs.zoom, vs.x + vs.ox, vs.y + vs.oy);
}
static void log_conectores(void)
{
    pag_t *p = P();
    for (int i = 0; i < p->ncon; i++) {
        int x0, y0, d0, x1, y1, d1;
        con_extremos(p, &p->con[i], &x0, &y0, &d0, &x1, &y1, &d1);
        registrar("arara: conector %d de %d/%d a %d/%d desde %d %d hasta %d %d\n", p->con[i].id, p->con[i].de, p->con[i].pde, p->con[i].a, p->con[i].pa, x0, y0, x1, y1);
    }
    for (int i = 0; i < p->nfig; i++) registrar("arara: fig %d %s %d %d %d %d\n", p->fig[i].id, TIPOS[p->fig[i].tipo].clave, p->fig[i].x, p->fig[i].y, p->fig[i].l, p->fig[i].a);
}

/* ------------------------------------------------------------ páginas */

static void abas_sinc(void)
{
    for (int i = 0; i < npag; i++) copia_texto(abas.titulos[i], pag[i].nombre, sizeof abas.titulos[i]);
    abas.n = npag; abas.sel = cur;
}
static void ir_pagina(int i)
{
    if (i < 0 || i >= npag) return;
    vista_guardar();
    cur = i;
    vs.zoom = P()->vzoom; vs.ox = P()->vox; vs.oy = P()->voy;
    edit_fig = 0; cg_activo = false;
    abas_sinc();
    sync_sel();
    cargar_inspetor();
}
static void pagina_nueva(void)
{
    if (npag >= MAX_PAG) { dizer(_("No caben más páginas")); return; }
    char n[32];
    formatar_texto(n, sizeof n, "%s %d", _("Página"), npag + 1);
    if (!pag_iniciar(&pag[npag], n)) return;
    npag++;
    ir_pagina(npag - 1);
    commit(_("Página nueva"));
}
static void pagina_borrar(int i)
{
    if (i < 0 || i >= npag) return;
    if ((pag[i].nfig || pag[i].ncon) && !confirmar("Arara", _("¿Eliminar la página con todo su contenido?"))) return;
    if (npag == 1) { pag[0].nfig = pag[0].ncon = 0; sel_cambio(); commit(_("Eliminar página")); return; }
    pag_liberar(&pag[i]);
    for (int k = i; k + 1 < npag; k++) pag[k] = pag[k + 1];
    memset(&pag[npag - 1], 0, sizeof pag[0]);
    npag--;
    if (cur >= npag) cur = npag - 1;
    else if (cur > i) cur--;
    vs.zoom = P()->vzoom; vs.ox = P()->vox; vs.oy = P()->voy;
    abas_sinc();
    sync_sel();
    cargar_inspetor();
    commit(_("Eliminar página"));
}
static void pagina_nombre(void)
{
    char n[32];
    copia_texto(n, P()->nombre, sizeof n);
    if (!pedir_texto(_("Cambiar el nombre"), _("Nombre de la página:"), n, sizeof n) || !n[0]) return;
    copia_texto(P()->nombre, n, sizeof P()->nombre);
    abas_sinc();
    cargar_inspetor();
    commit(_("Cambiar el nombre de la página"));
}

/* ------------------------------------------------------------ edición de texto en la figura */

static void editar_empezar(int id)
{
    int i = fig_idx(P(), id);
    if (i < 0) return;
    edit_fig = id;
    copia_texto(edit_orig, P()->fig[i].texto, sizeof edit_orig);
    registrar("arara: editar texto figura %d\n", id);
}
static void editar_fin(bool aceptar)
{
    if (!edit_fig) return;
    int i = fig_idx(P(), edit_fig);
    edit_fig = 0;
    if (i < 0) return;
    fig_t *f = &P()->fig[i];
    if (!aceptar) { copia_texto(f->texto, edit_orig, sizeof f->texto); return; }
    if (compara_texto(f->texto, edit_orig) != 0) {
        registrar("arara: texto figura %d \"%s\"\n", f->id, f->texto);
        cargar_inspetor();
        commit(_("Editar texto"));
    }
}
static void editar_tecla(const evento_t *e)
{
    int i = fig_idx(P(), edit_fig);
    if (i < 0) { edit_fig = 0; return; }
    fig_t *f = &P()->fig[i];
    int n = (int)tam_texto(f->texto);
    unsigned t = e->tecla;
    if (t == TECLA_ESC) { editar_fin(true); return; }
    if ((e->modificadores & 2) && (t == 1 || t == 26 || t == 25)) return;
    if (t == 8) { if (n > 0) f->texto[n - 1] = 0; }
    else if (t == '\n' || t == '\r') { if (n < TXT_FIG - 1) { f->texto[n] = '\n'; f->texto[n + 1] = 0; } }
    else if (t >= 32 && t < 256 && !(e->modificadores & 6)) { if (n < TXT_FIG - 1) { f->texto[n] = (char)t; f->texto[n + 1] = 0; } }
}

/* ------------------------------------------------------------ conectores */

/* Extremo bajo el ratón: un punto de conexión si hay una figura cerca (el más próximo de ella), o un punto libre. */
static void objetivo_conector(int wx, int wy, int *fid, int *pt, int *x, int *y)
{
    int margen = 14000 / vs.zoom;
    int i = fig_en(wx, wy, margen);
    if (i >= 0) {
        *fid = P()->fig[i].id;
        cerca_de_pto(&P()->fig[i], wx, wy, pt);
        fig_punto(&P()->fig[i], *pt, x, y, NULL);
        return;
    }
    *fid = 0; *pt = 0;
    *x = edvista_iman(&vs, wx); *y = edvista_iman(&vs, wy);
}

static bool tratar_conector(const evento_t *e)
{
    bool en = e->x >= vs.x && e->y >= vs.y && e->x < vs.x + vs.l && e->y < vs.y + vs.a;
    int wx, wy;
    edvista_para_mundo(&vs, e->x, e->y, &wx, &wy);
    if (e->tipo == EV_CLIQUE && (e->botoes & 1) && en) {
        int fid, pt, x, y;
        objetivo_conector(wx, wy, &fid, &pt, &x, &y);
        cg_activo = true; cg_de = fid; cg_pde = pt; cg_x0 = x; cg_y0 = y;
        cg_a = 0; cg_pa = 0; cg_x1 = x; cg_y1 = y;
        sel_limpiar();
        sel_cambio();
        redibujar_lienzo();
        return true;
    }
    if (e->tipo == EV_MOVER && (en || cg_activo)) {
        int fid, pt, x, y;
        if (!en) return cg_activo;
        objetivo_conector(wx, wy, &fid, &pt, &x, &y);
        int hf = fid, hp = pt;
        if (!fid) hf = 0;
        if (cg_activo) { cg_a = fid; cg_pa = pt; cg_x1 = x; cg_y1 = y; }
        if (hf != hov_fig || hp != hov_pto || cg_activo) { hov_fig = hf; hov_pto = hp; redibujar_lienzo(); }
        return cg_activo;
    }
    if (e->tipo == EV_SOLTOU && cg_activo) {
        cg_activo = false;
        int x0 = cg_x0, y0 = cg_y0, x1 = cg_x1, y1 = cg_y1;
        bool mismo = cg_de && cg_de == cg_a && cg_pde == cg_pa;
        int dxs = (x1 - x0) * vs.zoom / 1000, dys = (y1 - y0) * vs.zoom / 1000;
        bool corto = !cg_de && !cg_a && dxs * dxs + dys * dys < 36;
        if (!mismo && !corto) {
            pag_t *p = P();
            if (p->ncon >= MAX_CON) dizer(_("La página está llena"));
            else {
                con_t *c = &p->con[p->ncon++];
                memset(c, 0, sizeof *c);
                c->id = p->sig_id++;
                c->tipo = herr == H_ANGULO ? C_ANGULO : C_RECTO;
                c->de = cg_de; c->pde = cg_pde; c->a = cg_a; c->pa = cg_pa;
                c->x0 = x0; c->y0 = y0; c->x1 = x1; c->y1 = y1;
                c->flecha_ini = nuevo_flecha_ini; c->flecha_fin = nuevo_flecha_fin;
                c->linea = 0xFF2F4F6F; c->grosor = 2;
                sel_limpiar();
                c->sel = true;
                sel_cambio();
                commit(_("Conectar"));
                registrar("arara: conector nuevo %d de %d/%d a %d/%d\n", c->id, c->de, c->pde, c->a, c->pa);
            }
        }
        redibujar_lienzo();
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ acciones */

static void invertir_conectores(void)
{
    bool hecho = false;
    for (int i = 0; i < P()->ncon; i++) {
        con_t *c = &P()->con[i];
        if (!c->sel) continue;
        int t;
        t = c->de; c->de = c->a; c->a = t;
        t = c->pde; c->pde = c->pa; c->pa = t;
        t = c->x0; c->x0 = c->x1; c->x1 = t;
        t = c->y0; c->y0 = c->y1; c->y1 = t;
        bool b = c->flecha_ini; c->flecha_ini = c->flecha_fin; c->flecha_fin = b;
        hecho = true;
    }
    if (hecho) { cargar_inspetor(); commit(_("Invertir")); }
}

static void herramienta(int h)
{
    herr = h; armada = -1; cg_activo = false; hov_fig = 0;
    if (edit_fig) editar_fin(true);
}

static void nuevo_doc(void)
{
    if (!pregunta_cambios()) return;
    if (!doc_nuevo()) return;
    ruta_arch[0] = 0; sucio = false; edit_fig = 0;
    vs.zoom = 1000; vs.ox = 40; vs.oy = 40;
    abas_sinc(); sync_sel(); cargar_inspetor(); historial_reiniciar();
    registrar("arara: documento nuevo\n");
}

static void accion(int a)
{
    switch (a) {
    case A_NUEVO: nuevo_doc(); break;
    case A_ABRIR: {
        if (!pregunta_cambios()) break;
        char c[256];
        copia_texto(c, ruta_arch[0] ? ruta_arch : "/home/", sizeof c);
        if (dialogo_abrir(_("Abrir"), _("Diagramas de Arara (*.arara)|*.arara|Todos los archivos (*.*)|*"), c, c, sizeof c) && c[0]) {
            if (abrir_en(c)) {
                vs.zoom = 1000; vs.ox = 40; vs.oy = 40;
                abas_sinc(); sel_limpiar(); sync_sel(); cargar_inspetor(); historial_reiniciar();
                ajustar_vista(); vista_guardar();
            } else mensagem("Arara", _("No se ha podido abrir el archivo."));
        }
        break;
    }
    case A_GUARDAR: guardar(); break;
    case A_GUARDAR_COMO: guardar_como(); break;
    case A_PNG: case A_SVG: case A_PDF: exportar(a); break;
    case A_SALIR: if (pregunta_cambios()) salir_pedido = true; break;
    case A_DESHACER: if (ed_desfazer(&pilha)) registrar("arara: deshacer (%d figuras %d conectores)\n", P()->nfig, P()->ncon); break;
    case A_REHACER: if (ed_refazer(&pilha)) registrar("arara: rehacer (%d figuras %d conectores)\n", P()->nfig, P()->ncon); break;
    case A_CORTAR: copiar_sel(); borrar_sel(); break;
    case A_COPIAR: copiar_sel(); break;
    case A_PEGAR: pegar_clip(); break;
    case A_DUPLICAR: copiar_sel(); pegar_clip(); break;
    case A_BORRAR: borrar_sel(); break;
    case A_SELTODO:
        for (int i = 0; i < P()->nfig; i++) P()->fig[i].sel = true;
        for (int i = 0; i < P()->ncon; i++) P()->con[i].sel = true;
        sel_cambio();
        break;
    case A_FRENTE: orden_z(true); break;
    case A_ATRAS: orden_z(false); break;
    case A_AL_IZQ: case A_AL_CEN: case A_AL_DER: case A_AL_ARR: case A_AL_MED: case A_AL_ABA: alinear(a); break;
    case A_DIS_H: distribuir(true); break;
    case A_DIS_V: distribuir(false); break;
    case A_H_SEL: herramienta(H_SEL); break;
    case A_H_RECTO: herramienta(H_RECTO); break;
    case A_H_ANGULO: herramienta(H_ANGULO); break;
    case A_FLECHA_INI: nuevo_flecha_ini = !nuevo_flecha_ini; menu_conector_rehacer(); break;
    case A_FLECHA_FIN: nuevo_flecha_fin = !nuevo_flecha_fin; menu_conector_rehacer(); break;
    case A_INVERTIR: invertir_conectores(); break;
    case A_ZOOM_MAS: zoom_centro(vs.zoom * 5 / 4); break;
    case A_ZOOM_MENOS: zoom_centro(vs.zoom * 4 / 5); break;
    case A_ZOOM_100: zoom_centro(1000); break;
    case A_AJUSTAR: ajustar_vista(); break;
    case A_REJILLA: vs.grade = vs.grade ? 0 : 16; cargar_inspetor(); break;
    case A_IMAN: vs.iman = !vs.iman; cargar_inspetor(); break;
    case A_PAG_NUEVA: pagina_nueva(); break;
    case A_PAG_BORRAR: pagina_borrar(cur); break;
    case A_PAG_NOMBRE: pagina_nombre(); break;
    case A_ACERCA: mensagem("Arara", _("Arara: diagramas sencillos al estilo de Visio.\nFiguras, conectores, páginas y exportación a PNG, SVG y PDF.\nSoftware libre, licencia GPL v3.")); break;
    default: break;
    }
    if (a >= A_ZOOM_MAS && a <= A_AJUSTAR) { vista_guardar(); log_vista(); }
    registrar("arara: accion %d\n", a);
    sync_sel();
    redibujar();
}

static void mover_con_teclas(int dx, int dy)
{
    if (!n_sel_fig()) return;
    for (int i = 0; i < P()->nfig; i++) if (P()->fig[i].sel) { P()->fig[i].x = mundo(P()->fig[i].x + dx); P()->fig[i].y = mundo(P()->fig[i].y + dy); }
    sync_sel(); cargar_inspetor();
    commit(_("Mover"));
}

static bool tecla_global(const evento_t *e)
{
    unsigned t = e->tecla;
    bool ctrl = (e->modificadores & 2) && !(e->modificadores & 4);
    if (ctrl) {
        switch (t) {
        case 14: accion(A_NUEVO); return true;
        case 15: accion(A_ABRIR); return true;
        case 19: accion(A_GUARDAR); return true;
        case 26: accion(A_DESHACER); return true;
        case 25: accion(A_REHACER); return true;
        case 24: accion(A_CORTAR); return true;
        case 3: accion(A_COPIAR); return true;
        case 22: accion(A_PEGAR); return true;
        case 4: accion(A_DUPLICAR); return true;
        case 1: accion(A_SELTODO); return true;
        default: break;
        }
    }
    int paso = vs.iman && vs.grade ? vs.grade : 8;
    if (t == TECLA_DELETE) { accion(A_BORRAR); return true; }
    if (t == TECLA_ESC) { herramienta(H_SEL); armada = -1; sel_limpiar(); sel_cambio(); redibujar(); return true; }
    if (t == TECLA_ESQUERDA) { mover_con_teclas(-paso, 0); redibujar(); return true; }
    if (t == TECLA_DIREITA) { mover_con_teclas(paso, 0); redibujar(); return true; }
    if (t == TECLA_CIMA) { mover_con_teclas(0, -paso); redibujar(); return true; }
    if (t == TECLA_BAIXO) { mover_con_teclas(0, paso); redibujar(); return true; }
    if (t == '+' || t == '=') { accion(A_ZOOM_MAS); return true; }
    if (t == '-') { accion(A_ZOOM_MENOS); return true; }
    return false;
}

/* ------------------------------------------------------------ arrastrar desde la paleta */

static bool alvo_lienzo(void *ctx, int fase, const arrsol_item_t *it, int x, int y)
{
    (void)ctx;
    if (fase != ARRSOL_SOLTA) return true;
    int t = it->dados[0], wx, wy;
    if (t < 0 || t >= T_N) return false;
    edvista_para_mundo(&vs, x, y, &wx, &wy);
    registrar("arara: soltar figura %s en %d %d\n", TIPOS[t].clave, wx, wy);
    nueva_figura(t, wx, wy);
    armada = -1;
    return true;
}
static void registrar_alvos(void)
{
    arrsol_alvos_limpar();
    arrsol_alvo(vs.x, vs.y, vs.l, vs.a, alvo_lienzo, NULL);
}

/* ------------------------------------------------------------ modo sin ventana */

static int modo_sin_ventana(int argc, char **argv)
{
    const char *abrir_f = NULL, *png = NULL, *svg = NULL, *pdf = NULL, *gua = NULL;
    bool info = false;
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--abrir") == 0 && i + 1 < argc) abrir_f = argv[++i];
        else if (compara_texto(argv[i], "--png") == 0 && i + 1 < argc) png = argv[++i];
        else if (compara_texto(argv[i], "--svg") == 0 && i + 1 < argc) svg = argv[++i];
        else if (compara_texto(argv[i], "--pdf") == 0 && i + 1 < argc) pdf = argv[++i];
        else if (compara_texto(argv[i], "--guardar") == 0 && i + 1 < argc) gua = argv[++i];
        else if (compara_texto(argv[i], "--info") == 0) info = true;
    }
    if (!doc_nuevo()) return 1;
    int rc = 0;
    if (abrir_f && !abrir_en(abrir_f)) { escrever_texto(SAIDA, "arara: no se pudo abrir el archivo\n"); return 1; }
    if (png && exportar_png(png, EXPORT_PX_MAX) != 0) { escrever_texto(SAIDA, "arara: no se pudo exportar el PNG\n"); rc = 1; }
    if (svg && exportar_svg(svg) != 0) { escrever_texto(SAIDA, "arara: no se pudo exportar el SVG\n"); rc = 1; }
    if (pdf && exportar_pdf(pdf) != 0) { escrever_texto(SAIDA, "arara: no se pudo exportar el PDF\n"); rc = 1; }
    if (gua && !guardar_en(gua)) rc = 1;
    int nf = 0, nc = 0;
    for (int i = 0; i < npag; i++) { nf += pag[i].nfig; nc += pag[i].ncon; }
    if (info || abrir_f) {
        char s[160];
        formatar_texto(s, sizeof s, "arara: %d paginas, %d figuras, %d conectores\n", npag, nf, nc);
        escrever_texto(SAIDA, s);
    }
    dormir(300);
    return rc;
}

/* ------------------------------------------------------------ principal */

static void iniciar_botones(void)
{
    static const char *const icono[B_N] = { "documento-novo", "abrir", "guardar", "voltar", "avancar", NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
    static const char *const texto[B_N] = { NULL, NULL, NULL, NULL, NULL, N_("Puntero"), N_("Recto"), N_("Ángulo"), N_("Al frente"), N_("Atrás"), N_("Rejilla"), N_("Ajuste"), "-", "+" };
    for (int i = 0; i < B_N; i++) {
        memset(&tb[i], 0, sizeof tb[i]);
        tb[i].variante = icono[i] || i >= B_ZMENOS ? MB_ICONO : MB_PLANO;
        if (!icono[i] && i < B_ZMENOS) tb[i].variante = MB_SECUNDARIO;
        tb[i].icone = icono[i];
        tb[i].texto = texto[i];
        tb[i].fundo = mod_tema.barra;
    }
    tb[B_ZMENOS].variante = tb[B_ZMAS].variante = MB_SECUNDARIO;
}

int principal(int argc, char **argv)
{
    if (argc >= 2 && argv[1][0] == '-' && argv[1][1] == '-') return modo_sin_ventana(argc, argv);

    jn = jan_abrir(_("Arara"), VENT_L, VENT_A);
    if (!jn) return 1;
    jan_estilo(jn, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, 720, 480);
    if (!doc_nuevo()) return 1;
    ed_desfazer_iniciar(&pilha, 60);
    ult = snap_tomar();

    for (int i = 0; i < CAT_N; i++) cat_lista[i] = _(CATEGORIAS[i]);
    for (int i = 0; i < 7; i++) { menu.nomes[i] = _(MENU_NOMBRES[i]); menu.itens[i] = _(MENU_TEXTOS[i]); }
    menu.n = 7;
    menu_conector_rehacer();
    iniciar_botones();
    edvista_iniciar(&vs, 0, 0, 100, 100);
    vs.zoom = 1000; vs.ox = 40; vs.oy = 40; vs.grade = 16; vs.iman = true;
    vs.desenhar = cb_desenhar; vs.acertar = cb_acertar; vs.aviso = cb_aviso;
    memset(&abas, 0, sizeof abas);
    abas.destaque = -1; abas.premida = -1;
    abas_sinc();
    memset(&ins, 0, sizeof ins);
    ins.menu_prop = -1; ins.editando = -1; ins.arrasto = -1; ins.sel = -1;
    cargar_inspetor();
    layout();
    cargar_inspetor();

    if (argc >= 2) {
        if (abrir_en(argv[1])) { abas_sinc(); sync_sel(); historial_reiniciar(); ajustar_vista(); vista_guardar(); }
        else { copia_texto(ruta_arch, argv[1], sizeof ruta_arch); dizer(_("Archivo nuevo")); }
    }
    cargar_inspetor();
    redibujar();
    log_posiciones();
    log_vista();
    registrar("arara: pronto\n");

    evento_t e;
    while (!salir_pedido) {
        if (proximo_evento(jn->id, &e, true) != 1) continue;
        if (e.tipo == EV_FECHAR) { if (pregunta_cambios()) break; continue; }
        if (jan_redimensionar(jn, &e)) { layout(); redibujar(); log_posiciones(); continue; }
        if (e.tipo == EV_TEMA) { redibujar(); continue; }

        /* arrastrar desde la paleta */
        registrar_alvos();
        int d = arrsol_tratar(jn, &e);
        if (d != ARRSOL_NADA) {
            if (d == ARRSOL_FIM || d == ARRSOL_CANCELOU) { armada = -1; redibujar(); }
            continue;
        }

        /* menús y botones */
        if (e.tipo == EV_MENU) {
            int m = barra_menu_tratar(jn, &menu, &e);
            if (m >= 0) { int a = MENU_ACCIONES[m / 100][m % 100]; if (a) accion(a); continue; }
            int r = combo_tratar(jn, &cb_cat, &e);
            if (r == COMBO_SELECIONOU) { cat_sel = cb_cat.sel; armada = -1; redibujar(); continue; }
            r = inspetor_tratar(jn, &ins, &e);
            if (r == INSPETOR_MUDOU) { redibujar(); }
            continue;
        }
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && e.y < MENU_A) { barra_menu_tratar(jn, &menu, &e); continue; }
        bool hecho = false;
        for (int i = 0; i < B_N && !hecho; i++) {
            if (!mbotao_tratar(jn, &tb[i], &e)) continue;
            if (!tb[i].desabilitado) accion(TB_ACCION[i]);
            hecho = true;
        }
        if (hecho) continue;

        /* texto en la figura */
        if (edit_fig) {
            if (e.tipo == EV_TECLA) { editar_tecla(&e); redibujar_lienzo(); continue; }
            if (e.tipo == EV_CLIQUE) { editar_fin(true); sync_sel(); redibujar(); }
        }

        /* paleta */
        if (e.tipo == EV_CLIQUE && (e.botoes & 1) && e.x < PAL_L && e.y >= MENU_A + TB_A) {
            if (combo_tratar(jn, &cb_cat, &e) == COMBO_SELECIONOU) { cat_sel = cb_cat.sel; redibujar(); continue; }
            int t = celda_en(e.x, e.y);
            if (t >= 0) {
                armada = t; herr = H_SEL;
                registrar("arara: paleta %s\n", TIPOS[t].clave);
                arrsol_item_t it;
                memset(&it, 0, sizeof it);
                it.tipo = 1; it.dados[0] = (unsigned char)t; it.n = 1;
                copia_texto(it.rotulo, _(TIPOS[t].nombre), sizeof it.rotulo);
                arrsol_preparar(&it, e.x, e.y);
                redibujar();
            }
            continue;
        }
        if (combo_tratar(jn, &cb_cat, &e) == COMBO_SELECIONOU) { cat_sel = cb_cat.sel; redibujar(); continue; }

        /* pestañas de las páginas */
        if (e.tipo == EV_CLIQUE || e.tipo == EV_MOVER || e.tipo == EV_SOLTOU) {
            int i = -1, k = -1;
            int r = abas_tratar(jn, &abas, &e, &i, &k);
            if (r == ABAS_SELECIONOU) { ir_pagina(i); redibujar(); log_vista(); continue; }
            if (r == ABAS_NOVA) { pagina_nueva(); redibujar(); continue; }
            if (r == ABAS_FECHOU) { pagina_borrar(i); redibujar(); continue; }
            if (r == ABAS_MOVEU && i >= 0 && k >= 0 && i < npag && k < npag) {
                pag_t t = pag[i];
                if (i < k) for (int q = i; q < k; q++) pag[q] = pag[q + 1]; else for (int q = i; q > k; q--) pag[q] = pag[q - 1];
                pag[k] = t;
                cur = k; abas_sinc();
                commit(_("Mover página"));
                redibujar();
                continue;
            }
            if (r != ABAS_NADA) continue;
        }

        /* teclado */
        if (e.tipo == EV_TECLA) {
            if (ins.foco && ins.editando >= 0) { if (inspetor_tratar(jn, &ins, &e) == INSPETOR_MUDOU) redibujar(); continue; }
            if (tecla_global(&e)) continue;
            if (ins.foco) { if (inspetor_tratar(jn, &ins, &e) == INSPETOR_MUDOU) redibujar(); }
            continue;
        }

        /* inspector */
        bool en_ins = e.x >= ins.x && e.y >= ins.y && e.y < ins.y + ins.a;
        if ((e.tipo == EV_CLIQUE && en_ins) || e.tipo == EV_RODA || ins.arrasto >= 0 || (e.tipo == EV_CLIQUE && ins.foco)) {
            if (e.tipo == EV_RODA && !en_ins) goto lienzo;
            int r = inspetor_tratar(jn, &ins, &e);
            if (r != INSPETOR_NADA) redibujar();
            if (en_ins || ins.arrasto >= 0) continue;
        }
        if (e.tipo == EV_MOVER && ins.arrasto >= 0) { inspetor_tratar(jn, &ins, &e); continue; }
        if (e.tipo == EV_SOLTOU && en_ins) { if (inspetor_tratar(jn, &ins, &e) != INSPETOR_NADA) redibujar(); continue; }

    lienzo:
        {
            bool en = e.x >= vs.x && e.y >= vs.y && e.x < vs.x + vs.l && e.y < vs.y + vs.a;
            if (e.tipo == EV_CLIQUE && (e.botoes & 1) && en) ins.foco = false;
            /* figura elegida en la paleta: un clic la coloca */
            if (armada >= 0 && e.tipo == EV_CLIQUE && (e.botoes & 1) && en) {
                int wx, wy;
                edvista_para_mundo(&vs, e.x, e.y, &wx, &wy);
                nueva_figura(armada, wx, wy);
                armada = -1;
                redibujar();
                continue;
            }
            /* herramientas de conectores */
            if (herr != H_SEL && !(e.tipo == EV_CLIQUE && (e.botoes & 4)) && e.tipo != EV_RODA && (en || cg_activo)) {
                if (tratar_conector(&e)) continue;
            }
            /* doble clic: escribir en la figura */
            if (e.tipo == EV_CLIQUE && (e.botoes & 1) && e.cliques == 2 && herr == H_SEL && en) {
                int wx, wy;
                edvista_para_mundo(&vs, e.x, e.y, &wx, &wy);
                int id = cb_acertar(NULL, wx, wy);
                if (id > 0 && id < CON_BASE) {
                    sel_limpiar();
                    P()->fig[fig_idx(P(), id)].sel = true;
                    sel_cambio();
                    editar_empezar(id);
                    redibujar();
                    continue;
                }
            }
            if (e.tipo == EV_MOVER && !en && vs.modo == 0) { continue; }
            int antes_zoom = vs.zoom, ox = vs.ox, oy = vs.oy;
            if (edvista_tratar(jn, &vs, &e) == EDVISTA_MUDOU) {
                herr_botones(); estado_texto();
                barra_estado_desenhar(jn, &est);
                jan_mostrar_area(jn, 0, jn->alt - EST_A, jn->larg, EST_A);
                if (vs.zoom != antes_zoom || vs.ox != ox || vs.oy != oy) { vista_guardar(); }
                if (vs.zoom != antes_zoom) log_vista();
                if (e.tipo != EV_MOVER) { cargar_inspetor(); inspetor_desenhar(jn, &ins); jan_mostrar_area(jn, ins.x, ins.y, ins.l, ins.a); }
            }
            if (e.tipo == EV_SOLTOU) { log_conectores(); herr_botones(); redibujar(); }
        }
    }
    ed_desfazer_liberar(&pilha);
    snap_soltar(ult);
    doc_vaciar();
    jan_fechar(jn);
    registrar("arara: fechado\n");
    return 0;
}
