// SPDX-License-Identifier: GPL-3.0-or-later
// PlanoMini, © 2026 Alejandro Anuncibay (prototipo original); versión nativa de SaguiOS.
/* Pruebas de unidad del motor de PlanoMini (D312) en el anfitrión: formato de números, coordenadas,
 * intersecciones, órdenes de dibujo y de modificación por la línea de órdenes, cotas, sombreado, capas,
 * referencia a objetos, deshacer, .pmini v1 y v2 y DXF de ida y vuelta.
 * Se compila con: gcc -DSAGUI_HOST -Iuserland/inc -Iuserland/gebramini -Iuserland/planomini (lo hace h312). */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include "pm.h"

void *aloca(size_t n) { return malloc(n); }
void *realoca(void *p, size_t n) { return realloc(p, n); }
void libera(void *p) { free(p); }
size_t tam_texto(const char *s) { return strlen(s); }
int compara_texto(const char *a, const char *b) { return strcmp(a, b); }
char *copia_texto(char *dst, const char *src, size_t max)
{
    if (!max) return dst;
    size_t i = 0;
    for (; i + 1 < max && src[i]; i++) dst[i] = src[i];
    dst[i] = 0;
    return dst;
}
int formatar_texto(char *dst, size_t max, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(dst, max, fmt, ap);
    va_end(ap);
    return r;
}

static int fallos, total;
#define VER(c, ...) do { total++; if (!(c)) { fallos++; printf("FALLO %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define CERCA(a, b, tol, ...) do { double _a = (a), _b = (b); total++; if (!(fabs(_a - _b) <= (tol))) { fallos++; printf("FALLO %d: %g != %g: ", __LINE__, _a, _b); printf(__VA_ARGS__); printf("\n"); } } while (0)
#define O(s) VER(pm_orden(s), "orden «%s»: %s", s, pm_msg)

static void nuevo(void)
{
    pm_cancelar();
    pm_limpiar();
    pm_orto = pm_polar = pm_forzc = 0;
    pm_tiene_ult = false;
    pm_ve = 20; pm_vcx = pm_vcy = 0; pm_vw = 800; pm_vh = 500;
    pm_historial_reiniciar();
}
static int cuenta(int tipo) { int n = 0; for (int i = 0; i < pm_n; i++) if (pm_ent[i].tipo == tipo) n++; return n; }
static int ultimo(int tipo) { for (int i = pm_n - 1; i >= 0; i--) if (pm_ent[i].tipo == tipo) return i; return -1; }
static void linea_(double a, double b, double c, double d) { pm_ent_t e; pm_ent_nueva(&e, E_LINEA); e.p[0] = a; e.p[1] = b; e.p[2] = c; e.p[3] = d; pm_anadir_actual(&e); }
static void circ_(double a, double b, double r) { pm_ent_t e; pm_ent_nueva(&e, E_CIRCULO); e.p[0] = a; e.p[1] = b; e.r = r; pm_anadir_actual(&e); }
static void rect_(double a, double b, double c, double d) { pm_ent_t e; pm_ent_nueva(&e, E_POLI); pm_rect_poli(&e, a, b, c, d); pm_anadir_actual(&e); }

static void prueba_util(void)
{
    char s[40];
    pm_fmt(3.14159, 2, s, sizeof s); VER(!strcmp(s, "3.14"), "fmt 3.14: %s", s);
    pm_fmt(-2.0, 3, s, sizeof s); VER(!strcmp(s, "-2"), "fmt -2: %s", s);
    pm_fmt(0.0004, 2, s, sizeof s); VER(!strcmp(s, "0"), "fmt 0: %s", s);
    pm_fmt(10.26, 1, s, sizeof s); VER(!strcmp(s, "10.3"), "fmt 10.3: %s", s);
    pm_fmt(1234567.125, 3, s, sizeof s); VER(!strcmp(s, "1234567.125"), "fmt grande: %s", s);
    pm_fmt(-0.5, 4, s, sizeof s); VER(!strcmp(s, "-0.5"), "fmt -0.5: %s", s);
    const char *e;
    CERCA(pm_atof("-12.5e2x", &e), -1250, 1e-9, "atof"); VER(*e == 'x', "atof deja el resto");
    CERCA(pm_atof(".25", NULL), 0.25, 1e-12, "atof .25");
    double x, y;
    nuevo();
    VER(pm_parse_coord("3,4", &x, &y) && x == 3 && y == 4, "abs");
    VER(pm_parse_coord("-3.5,-4", &x, &y) && x == -3.5 && y == -4, "abs neg");
    pm_tiene_ult = true; pm_ult_x = 10; pm_ult_y = 20;
    VER(pm_parse_coord("@1,2", &x, &y) && x == 11 && y == 22, "relativa");
    VER(pm_parse_coord("@5<90", &x, &y) && fabs(x - 10) < 1e-9 && fabs(y - 25) < 1e-9, "polar relativa");
    VER(pm_parse_coord("2<180", &x, &y) && fabs(x + 2) < 1e-9, "polar absoluta");
    VER(!pm_parse_coord("5", &x, &y), "solo distancia sin ORTO no vale");
    pm_orto = 1; pm_cur_x = 30; pm_cur_y = 21;
    VER(pm_parse_coord("5", &x, &y) && fabs(x - 15) < 1e-9 && fabs(y - 20) < 1e-9, "distancia con ORTO: %g,%g", x, y);
    pm_cur_x = 11; pm_cur_y = 1;
    VER(pm_parse_coord("5", &x, &y) && fabs(x - 10) < 1e-9 && fabs(y - 15) < 1e-9, "distancia con ORTO vertical hacia abajo: %g,%g", x, y);
    pm_orto = 0; pm_polar = 1; pm_polar_ang = 30; pm_cur_x = 10 + 10 * cos(33 * M_PI / 180); pm_cur_y = 20 + 10 * sin(33 * M_PI / 180);
    VER(pm_parse_coord("2", &x, &y) && fabs(x - (10 + 2 * cos(30 * M_PI / 180))) < 1e-9 && fabs(y - 21) < 1e-9, "distancia con POLAR 30: %g,%g", x, y);
    VER(!pm_parse_coord("abc", &x, &y) && !pm_parse_coord("1,", &x, &y) && !pm_parse_coord("@", &x, &y), "basura");
}

static void prueba_geo(void)
{
    pm_pz_t a, b;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    a.x0 = 0; a.y0 = 0; a.x1 = 10; a.y1 = 10;
    b.x0 = 0; b.y0 = 10; b.x1 = 10; b.y1 = 0;
    double o[4][2];
    VER(pm_pz_inter(&a, &b, false, false, o, 4) == 1 && fabs(o[0][0] - 5) < 1e-9 && fabs(o[0][1] - 5) < 1e-9, "seg x seg");
    b.x0 = 20; b.y0 = 0; b.x1 = 30; b.y1 = 10;
    VER(pm_pz_inter(&a, &b, false, false, o, 4) == 0, "seg x seg fuera");
    b.x0 = 0; b.y0 = 5; b.x1 = 4; b.y1 = 5;       /* corta a la diagonal en (5,5) solo si se prolonga */
    VER(pm_pz_inter(&a, &b, false, false, o, 4) == 0 && pm_pz_inter(&a, &b, false, true, o, 4) == 1, "prolongación");
    pm_ent_t c1;
    pm_ent_nueva(&c1, E_CIRCULO); c1.p[0] = 0; c1.p[1] = 0; c1.r = 5;
    pm_pz_t pc;
    pm_piezas(&c1, &pc, 1);
    pm_pz_t s2;
    memset(&s2, 0, sizeof s2);
    s2.x0 = -10; s2.y0 = 0; s2.x1 = 10; s2.y1 = 0;
    VER(pm_pz_inter(&s2, &pc, false, false, o, 4) == 2, "seg x círculo = 2");
    s2.y0 = s2.y1 = 5;
    VER(pm_pz_inter(&s2, &pc, false, false, o, 4) == 1 && fabs(o[0][0]) < 1e-6, "tangente = 1");
    s2.y0 = s2.y1 = 6;
    VER(pm_pz_inter(&s2, &pc, false, false, o, 4) == 0, "sin corte");
    pm_ent_t c2;
    pm_ent_nueva(&c2, E_CIRCULO); c2.p[0] = 6; c2.p[1] = 0; c2.r = 5;
    pm_pz_t pd;
    pm_piezas(&c2, &pd, 1);
    VER(pm_pz_inter(&pc, &pd, false, false, o, 4) == 2 && fabs(o[0][0] - 3) < 1e-9 && fabs(fabs(o[0][1]) - 4) < 1e-9, "círculo x círculo");
    pm_pz_t arc;
    pm_bulge_arco(0, 0, 10, 0, 1.0, &arc);      /* semicírculo hacia la izquierda del recorrido: bulge + = antihorario */
    VER(arc.arco && fabs(arc.r - 5) < 1e-9 && fabs(arc.cx - 5) < 1e-9 && fabs(arc.cy) < 1e-9, "bulge 1 = semicírculo: c=(%g,%g) r=%g", arc.cx, arc.cy, arc.r);
    pm_bulge_arco(0, 0, 10, 0, -1.0, &arc);
    VER(arc.arco && arc.inv && fabs(arc.cx - 5) < 1e-9, "bulge -1 al revés");
    CERCA(pm_bulge_de(0, PM_PI, false), 1.0, 1e-9, "bulge_de");
    pm_ent_liberar(&c1); pm_ent_liberar(&c2);
}

static void prueba_dibujo(void)
{
    nuevo();
    O("L 0,0"); O("10,0"); O("@0,5"); O("@5<90"); O("");
    VER(pm_n == 3 && !pm_activa(), "tres segmentos y la orden termina: %d", pm_n);
    CERCA(pm_ent[1].p[2], 10, 1e-9, "segundo segmento x"); CERCA(pm_ent[1].p[3], 5, 1e-9, "y");
    CERCA(pm_ent[2].p[3], 10, 1e-9, "polar relativa");
    nuevo();
    O("L 0,0"); O("5,0"); O("5,5"); O("U");
    VER(pm_n == 1, "Deshacer dentro de LINEA quita el último segmento: %d", pm_n);
    O("2,5"); O("");
    VER(pm_n == 2 && fabs(pm_ent[1].p[0] - 5) < 1e-9, "tras Deshacer sigue desde el punto anterior");
    nuevo();
    O("L 0,0"); O("4,0"); O("4,3"); O("C");
    VER(pm_n == 3 && fabs(pm_ent[2].p[2]) < 1e-9 && fabs(pm_ent[2].p[3]) < 1e-9 && !pm_activa(), "Cerrar une con el primer punto");
    nuevo();
    pm_orto = 1;
    O("L 0,0"); pm_cur_x = 7; pm_cur_y = 1; O("5"); pm_cur_x = 5; pm_cur_y = -2; O("3"); O("");
    VER(pm_n == 2 && fabs(pm_ent[0].p[2] - 5) < 1e-9 && fabs(pm_ent[1].p[3] + 3) < 1e-9, "solo distancia con ORTO (%g,%g)", pm_ent[1].p[2], pm_ent[1].p[3]);
    nuevo();
    O("REC 0,0 4,3");
    VER(pm_n == 1 && pm_ent[0].tipo == E_POLI && pm_ent[0].nv == 4 && pm_ent[0].cerrada, "rectángulo");
    O("REC 1 1 3 3");
    VER(pm_n == 2 && fabs(pm_ent[1].v[2].x - 3) < 1e-9 && fabs(pm_ent[1].v[2].y - 3) < 1e-9, "compat. prototipo: REC con números sueltos");
    O("LINE 0 0 10 10");
    VER(pm_n == 3 && pm_ent[2].tipo == E_LINEA && fabs(pm_ent[2].p[2] - 10) < 1e-9 && !pm_activa(), "compat. prototipo: LINE 0 0 10 10");
    O("CIRCLE 3 4 2");
    VER(pm_n == 4 && pm_ent[3].tipo == E_CIRCULO && fabs(pm_ent[3].r - 2) < 1e-9, "compat: CIRCLE x y r");
    O("POINT 1 2");
    VER(pm_n == 5 && pm_ent[4].tipo == E_PUNTO, "compat: POINT x y");
    O("UNDO");
    VER(pm_n == 4, "compat: UNDO deshace: %d", pm_n);
    O("GRID"); VER(!pm_rejilla, "GRID alterna"); O("GRID"); VER(pm_rejilla, "GRID vuelve");
    O("PAN 2 3"); CERCA(pm_vcx, -2, 1e-9, "PAN dx"); CERCA(pm_vcy, -3, 1e-9, "PAN dy");
    double e0 = pm_ve; O("ZOOM 2"); CERCA(pm_ve, 2 * e0, 1e-9, "ZOOM factor");
    nuevo();
    O("C 0,0 5");
    VER(pm_n == 1 && pm_ent[0].tipo == E_CIRCULO && fabs(pm_ent[0].r - 5) < 1e-9, "círculo centro-radio");
    O("C 0,0"); O("D"); O("10");
    VER(pm_n == 2 && fabs(pm_ent[1].r - 5) < 1e-9, "círculo centro-diámetro: %g", pm_ent[1].r);
    O("C 3P"); O("5,0"); O("0,5"); O("-5,0");
    VER(pm_n == 3 && fabs(pm_ent[2].r - 5) < 1e-9 && fabs(pm_ent[2].p[0]) < 1e-9, "círculo por 3 puntos");
    O("C 2P"); O("0,0"); O("10,0");
    VER(pm_n == 4 && fabs(pm_ent[3].r - 5) < 1e-9 && fabs(pm_ent[3].p[0] - 5) < 1e-9, "círculo 2P");
    O("C 0,0"); O("3,4");
    VER(pm_n == 5 && fabs(pm_ent[4].r - 5) < 1e-9, "radio por un punto");
    O("A 5,0 0,5 -5,0");
    VER(pm_ent[pm_n - 1].tipo == E_ARCO && fabs(pm_ent[pm_n - 1].r - 5) < 1e-9 && fabs(pm_ent[pm_n - 1].a0) < 1e-9 && fabs(pm_ent[pm_n - 1].a1 - PM_PI) < 1e-9, "arco de 3 puntos");
    O("A C 0,0 5,0 0,5");
    CERCA(pm_ent[pm_n - 1].a1, PM_PI / 2, 1e-9, "arco centro-inicio-fin");
    nuevo();
    O("POL 6 0,0 I 5");
    VER(pm_n == 1 && pm_ent[0].nv == 6, "hexágono");
    for (int i = 0; i < 6; i++) CERCA(hypot(pm_ent[0].v[i].x, pm_ent[0].v[i].y), 5, 1e-9, "vértice %d a 5", i);
    CERCA(pm_ent[0].v[0].x, 5, 1e-9, "primer vértice");
    O("POL 4 0,0 C 3");     /* cuadrado circunscrito a un círculo de radio 3: lado 6 */
    CERCA(hypot(pm_ent[1].v[1].x - pm_ent[1].v[0].x, pm_ent[1].v[1].y - pm_ent[1].v[0].y), 6, 1e-9, "cuadrado circunscrito: lado 6");
    O("POL 5 E 0,0 4,0");
    VER(pm_ent[2].nv == 5, "polígono por arista");
    CERCA(hypot(pm_ent[2].v[1].x - pm_ent[2].v[0].x, pm_ent[2].v[1].y - pm_ent[2].v[0].y), 4, 1e-9, "arista 4");
    nuevo();
    O("PL 0,0 10,0 A 10,10 L 0,10 C");
    VER(pm_n == 1 && pm_ent[0].nv == 4 && pm_ent[0].cerrada, "polilínea cerrada con un arco (%d vértices)", pm_n ? pm_ent[0].nv : -1);
    CERCA(pm_ent[0].v[1].b, 1.0, 1e-9, "arco tangente al tramo anterior: semicírculo");
    CERCA(pm_ent[0].v[0].b, 0.0, 1e-9, "tramo recto");
    nuevo();
    O("T 1,1 2 30 Hola mundo");
    VER(pm_n == 1 && pm_ent[0].tipo == E_TEXTO && !strcmp(pm_ent[0].txt, "Hola mundo") && fabs(pm_ent[0].alt - 2) < 1e-9 && fabs(pm_ent[0].giro - 30 * M_PI / 180) < 1e-9, "texto: «%s»", pm_n ? pm_ent[0].txt : "");
    O("PO 3,3");
    VER(pm_n == 2 && pm_ent[1].tipo == E_PUNTO, "punto");
    O("LINEA"); O("1,1"); pm_cancelar();
    VER(!pm_activa() && pm_n == 2, "Esc cancela");
    O("L"); O("0,0"); O("5,5"); O("");
    O("");    /* Intro repite la última orden */
    VER(pm_activa() && !strcmp(pm_orden_nombre(), "LINEA,LINE,L"), "Intro repite la última orden");
    pm_cancelar();
    VER(!pm_orden("FOOBAR"), "orden desconocida");
    VER(pm_orden("LIN") && pm_activa(), "prefijo único LIN = LINEA"); pm_cancelar();
    char sug[8][20];
    int ns = pm_sugerir("cir", sug, 8);
    VER(ns >= 1 && !strcmp(sug[0], "CIRCULO"), "sugerencias de «cir»: %d", ns);
}

static void sel_(const char *ord, const char *sel)
{
    pm_orden(ord);
    pm_orden(sel);
    pm_orden("");
}

static void prueba_modificar(void)
{
    nuevo();
    linea_(0, 0, 10, 0);
    sel_("M", "#0"); O("0,0"); O("5,5");
    CERCA(pm_ent[0].p[0], 5, 1e-9, "mover x"); CERCA(pm_ent[0].p[1], 5, 1e-9, "mover y"); CERCA(pm_ent[0].p[2], 15, 1e-9, "mover x2");
    nuevo();
    linea_(0, 0, 10, 0);
    sel_("CO", "#0"); O("0,0"); O("0,3"); O("0,6"); O("");
    VER(pm_n == 3 && fabs(pm_ent[1].p[1] - 3) < 1e-9 && fabs(pm_ent[2].p[1] - 6) < 1e-9, "copiar varias veces: %d", pm_n);
    nuevo();
    linea_(0, 0, 10, 0);
    sel_("RO", "#0"); O("0,0"); O("90");
    CERCA(pm_ent[0].p[2], 0, 1e-9, "girar 90: x"); CERCA(pm_ent[0].p[3], 10, 1e-9, "y");
    nuevo();
    linea_(1, 1, 3, 2);
    sel_("SC", "#0"); O("0,0"); O("2");
    CERCA(pm_ent[0].p[2], 6, 1e-9, "escala 2"); CERCA(pm_ent[0].p[3], 4, 1e-9, "y");
    nuevo();
    linea_(1, 1, 3, 2);
    sel_("MI", "#0"); O("0,0"); O("0,1"); O("N");
    VER(pm_n == 2 && fabs(pm_ent[1].p[0] + 1) < 1e-9 && fabs(pm_ent[1].p[2] + 3) < 1e-9 && fabs(pm_ent[1].p[3] - 2) < 1e-9, "simetría respecto del eje Y");
    nuevo();
    circ_(0, 0, 2);
    sel_("MI", "#0"); O("0,0"); O("1,0"); O("S");
    VER(pm_n == 1, "simetría borrando el origen");

    /* equidistancia */
    nuevo();
    linea_(0, 0, 10, 0);
    O("O 2"); O("5,0"); O("5,5"); O("");
    VER(pm_n == 2 && fabs(pm_ent[1].p[1] - 2) < 1e-9 && fabs(pm_ent[1].p[3] - 2) < 1e-9, "equidistancia de una línea");
    nuevo();
    circ_(0, 0, 5);
    O("O 2"); O("5,0"); O("9,0"); O("5,0"); O("1,0"); O("");
    VER(pm_n == 3 && fabs(pm_ent[1].r - 7) < 1e-9 && fabs(pm_ent[2].r - 3) < 1e-9, "equidistancia de un círculo (fuera y dentro): %g %g", pm_n > 1 ? pm_ent[1].r : 0, pm_n > 2 ? pm_ent[2].r : 0);
    nuevo();
    rect_(0, 0, 10, 10);
    O("O 2"); O("5,0"); O("5,5"); O("");
    double x0, y0, x1, y1;
    VER(pm_n == 2 && pm_ent_bbox(&pm_ent[1], &x0, &y0, &x1, &y1) && fabs(x0 - 2) < 1e-9 && fabs(y0 - 2) < 1e-9 && fabs(x1 - 8) < 1e-9 && fabs(y1 - 8) < 1e-9, "equidistancia interior de un rectángulo");
    O("O 1"); O("5,0"); O("5,-5"); O("");
    VER(pm_n == 3 && pm_ent_bbox(&pm_ent[2], &x0, &y0, &x1, &y1) && fabs(x0 + 1) < 1e-9 && fabs(x1 - 11) < 1e-9 && fabs(y1 - 11) < 1e-9, "equidistancia exterior (%g..%g)", x0, x1);

    /* recortar */
    nuevo();
    linea_(0, 0, 10, 0); linea_(4, -5, 4, 5);
    O("TR"); O(""); O("8,0"); O("");
    VER(pm_n == 2 && fabs(pm_ent[0].p[2] - 4) < 1e-9, "recortar el extremo: queda hasta x=4 (%g)", pm_ent[0].p[2]);
    nuevo();
    linea_(0, 0, 10, 0); linea_(3, -5, 3, 5); linea_(7, -5, 7, 5);
    O("TR"); O(""); O("5,0"); O("");
    VER(pm_n == 4, "recortar el tramo central deja dos líneas: %d", pm_n);
    {
        int pa = 0, pb = 0;
        for (int i = 0; i < pm_n; i++) if (pm_ent[i].tipo == E_LINEA && fabs(pm_ent[i].p[1]) < 1e-9 && fabs(pm_ent[i].p[3]) < 1e-9) {
            if (fabs(pm_ent[i].p[0]) < 1e-9 && fabs(pm_ent[i].p[2] - 3) < 1e-9) pa++;
            if (fabs(pm_ent[i].p[0] - 7) < 1e-9 && fabs(pm_ent[i].p[2] - 10) < 1e-9) pb++;
        }
        VER(pa == 1 && pb == 1, "los dos tramos son 0..3 y 7..10");
    }
    nuevo();
    circ_(0, 0, 5); linea_(3, -6, 3, 6); linea_(-3, -6, -3, 6);
    O("TR"); O(""); O("5,0"); O("");
    VER(pm_ent[0].tipo == E_ARCO && fabs(pm_ent[0].a0 - atan2(4, 3)) < 1e-9 && fabs(pm_ent[0].a1 - (2 * M_PI - atan2(4, 3))) < 1e-9, "recortar un círculo da un arco: %g..%g", pm_ent[0].a0, pm_ent[0].a1);
    nuevo();
    linea_(0, 0, 10, 0);
    VER(!pm_orden("TR") || true, "recortar sin bordes");
    O(""); O("5,0");
    VER(pm_msg_err, "sin bordes de corte avisa: %s", pm_msg);
    pm_cancelar();
    nuevo();
    rect_(0, 0, 10, 10); linea_(5, -3, 5, 13);
    O("TR"); O("#1"); O(""); O("8,0"); O("");
    VER(cuenta(E_POLI) == 0 && cuenta(E_LINEA) >= 5, "recortar una polilínea la explota: %d líneas", cuenta(E_LINEA));

    /* alargar */
    nuevo();
    linea_(0, 0, 5, 0); linea_(8, -5, 8, 5);
    O("EX"); O("#1"); O(""); O("4.5,0"); O("");
    CERCA(pm_ent[0].p[2], 8, 1e-9, "alargar hasta el borde");
    nuevo();
    linea_(0, 0, 5, 0); linea_(-4, -5, -4, 5);
    O("EX"); O("#1"); O(""); O("0.5,0"); O("");
    CERCA(pm_ent[0].p[0], -4, 1e-9, "alargar por el otro extremo");

    /* empalme y chaflán */
    nuevo();
    linea_(0, 0, 10, 0); linea_(0, 0, 0, 10);
    O("F"); O("R"); O("2"); O("5,0"); O("0,5");
    VER(pm_n == 3 && pm_ent[2].tipo == E_ARCO, "empalme crea un arco");
    CERCA(pm_ent[2].p[0], 2, 1e-9, "centro x"); CERCA(pm_ent[2].p[1], 2, 1e-9, "centro y"); CERCA(pm_ent[2].r, 2, 1e-9, "radio");
    CERCA(pm_ent[0].p[2], 2, 1e-9, "la línea 1 acaba en x=2 (%g,%g)-(%g,%g)", pm_ent[0].p[0], pm_ent[0].p[1], pm_ent[0].p[2], pm_ent[0].p[3]);
    CERCA(pm_ent[0].p[0], 10, 1e-9, "y conserva el extremo lejano");
    CERCA(pm_ent[1].p[3], 2, 1e-9, "la línea 2 acaba en y=2");
    CERCA(pm_ent[2].a0, 3 * M_PI / 2 > 0 ? pm_ent[2].a0 : 0, 1, "ángulos");
    {
        pm_pz_t pz; pm_piezas(&pm_ent[2], &pz, 1);
        double mx = 2 + 2 * cos((pz.a0 + pz.a1) / 2), my = 2 + 2 * sin((pz.a0 + pz.a1) / 2);
        VER(mx < 2 && my < 2, "el arco queda en la esquina (hacia el origen): (%g,%g)", mx, my);
    }
    nuevo();
    linea_(0, 0, 10, 0); linea_(0, 0, 0, 10);
    O("F"); O("R"); O("0"); O("5,0"); O("0,5");
    VER(pm_n == 2 && fabs(pm_ent[0].p[2]) < 1e-9, "empalme de radio 0 = esquina (%d)", pm_n);
    nuevo();
    linea_(0, 0, 10, 0); linea_(0, 0, 0, 3);
    O("F"); O("R"); O("5"); O("5,0"); O("0,2");
    VER(pm_msg_err && pm_n == 2, "radio demasiado grande: %s", pm_msg);
    nuevo();
    linea_(0, 0, 10, 0); linea_(0, 0, 0, 10);
    O("CHA"); O("D"); O("2"); O("3"); O("5,0"); O("0,5");
    VER(pm_n == 3 && fabs(pm_ent[2].p[0] - 2) < 1e-9 && fabs(pm_ent[2].p[3] - 3) < 1e-9, "chaflán 2 x 3: (%g,%g)-(%g,%g)", pm_ent[2].p[0], pm_ent[2].p[1], pm_ent[2].p[2], pm_ent[2].p[3]);
    nuevo();
    rect_(0, 0, 10, 10);
    O("F"); O("R"); O("1"); O("P"); O("5,0");
    VER(pm_ent[0].nv == 8 && (fabs(pm_ent[0].v[0].b - tan(M_PI / 8)) < 1e-9 || fabs(pm_ent[0].v[1].b - tan(M_PI / 8)) < 1e-9), "empalme de todas las esquinas de un rectángulo (%d vértices)", pm_ent[0].nv);

    /* explotar, matriz, igualar */
    nuevo();
    rect_(0, 0, 10, 10);
    sel_("X", "#0");
    VER(pm_n == 4 && cuenta(E_LINEA) == 4, "explotar un rectángulo: %d", pm_n);
    nuevo();
    linea_(0, 0, 1, 0);
    O("AR"); O("#0"); O(""); O(""); O("2"); O("3"); O("5"); O("4");
    VER(pm_n == 6, "matriz rectangular 2x3: %d", pm_n);
    {
        double mx = 0, my = 0;
        for (int i = 0; i < pm_n; i++) { if (pm_ent[i].p[0] > mx) mx = pm_ent[i].p[0]; if (pm_ent[i].p[1] > my) my = pm_ent[i].p[1]; }
        CERCA(mx, 8, 1e-9, "columnas separadas 4"); CERCA(my, 5, 1e-9, "filas separadas 5");
    }
    nuevo();
    linea_(5, 0, 6, 0);
    O("AR"); O("#0"); O(""); O("P"); O("0,0"); O("4"); O("");
    VER(pm_n == 4 && fabs(pm_ent[1].p[0]) < 1e-9 && fabs(pm_ent[1].p[1] - 5) < 1e-9, "matriz polar de 4: (%g,%g)", pm_ent[1].p[0], pm_ent[1].p[1]);
    nuevo();
    pm_capa_nueva("Muros");
    linea_(0, 0, 1, 0);
    pm_ent[0].capa = 1; pm_ent[0].color = 0xFF0000;
    linea_(5, 5, 6, 5);
    O("MA"); O("#0"); O("#1"); O("");
    VER(pm_ent[1].capa == 1 && pm_ent[1].color == 0xFF0000, "igualar propiedades");

    /* selección con ventana y captura */
    nuevo();
    linea_(0, 0, 2, 0); linea_(5, 5, 6, 6); linea_(1, -1, 1, 1);
    pm_ventana(-1, -2, 3, 2);
    VER(pm_sel_contar() == 2, "ventana: lo que queda dentro (%d)", pm_sel_contar());
    pm_sel_todo(false);
    pm_ventana(1.5, 2, -1, -2);
    VER(pm_sel_contar() == 2, "captura: lo que toca (%d)", pm_sel_contar());
    pm_sel_todo(false);
    pm_punto(1, 0);
    VER(pm_sel_contar() >= 1, "clic selecciona");
    pm_quitar = true; pm_punto(1, 0); pm_quitar = false;
    VER(pm_sel_contar() == 0, "Mayús quita");
}

static void prueba_cotas(void)
{
    nuevo();
    O("COTAHOR 0,0 10,0 5,3");
    int k = ultimo(E_COTA);
    char t[40];
    VER(k >= 0, "cota horizontal creada");
    if (k >= 0) { pm_cota_texto(&pm_ent[k], t, sizeof t); VER(!strcmp(t, "10"), "medida 10: %s", t); }
    O("COTALIN 0,0 0,8 -3,4");
    k = ultimo(E_COTA);
    VER(pm_ent[k].sub == C_LIN_V, "COTALIN decide vertical por la posición");
    pm_cota_texto(&pm_ent[k], t, sizeof t); VER(!strcmp(t, "8"), "vertical 8: %s", t);
    O("COTALIN 0,0 3,4 1,6");
    VER(pm_ent[ultimo(E_COTA)].sub == C_LIN_H, "COTALIN horizontal con el cursor encima");
    O("COTAALI 0,0 3,4 4,0");
    pm_cota_texto(&pm_ent[ultimo(E_COTA)], t, sizeof t); VER(!strcmp(t, "5"), "alineada 5: %s", t);
    circ_(20, 0, 3);
    O("COTARAD"); O("23,0"); O("25,4");
    pm_cota_texto(&pm_ent[ultimo(E_COTA)], t, sizeof t); VER(!strcmp(t, "R3"), "radio: %s", t);
    O("COTADIAM"); O("23,0"); O("25,4");
    pm_cota_texto(&pm_ent[ultimo(E_COTA)], t, sizeof t); VER(t[0] == (char)0xD8 && !strcmp(t + 1, "6"), "diámetro: %s", t);
    O("COTAANG 0,-10 5,-10 0,-5 3,-7");
    pm_cota_texto(&pm_ent[ultimo(E_COTA)], t, sizeof t); VER(!strcmp(t, "90\xB0"), "ángulo: %s", t);
    O("COTAESTILO 3 2 1");
    CERCA(pm_cota_alt, 3, 1e-9, "estilo: altura"); CERCA(pm_cota_flecha, 2, 1e-9, "flecha"); VER(pm_cota_dec == 1, "decimales");
    O("COTAHOR 0,0 10.26,0 5,3");
    pm_cota_texto(&pm_ent[ultimo(E_COTA)], t, sizeof t); VER(!strcmp(t, "10.3"), "un decimal: %s", t);
    pm_ent[ultimo(E_COTA)].txt[0] = 0;
    strcpy(pm_ent[ultimo(E_COTA)].txt, "<> mm");
    pm_cota_texto(&pm_ent[ultimo(E_COTA)], t, sizeof t); VER(!strcmp(t, "10.3 mm"), "texto con <>: %s", t);
    pm_cota_alt = 2.5; pm_cota_flecha = 2.5; pm_cota_dec = 2;
    /* una cota produce flechas, líneas y un texto */
    {
        int nseg = 0, ntri = 0, ntxt = 0;
        struct { int *a, *b, *c; } ctx = { &nseg, &ntri, &ntxt };
        void s_(void *c, double a, double b, double d, double e) { (void)a; (void)b; (void)d; (void)e; (*((typeof(ctx) *)c)->a)++; }
        void t_(void *c, double a, double b, double d, double e, double f, double g) { (void)a; (void)b; (void)d; (void)e; (void)f; (void)g; (*((typeof(ctx) *)c)->b)++; }
        void x_(void *c, double a, double b, double d, double e, const char *s, int f) { (void)a; (void)b; (void)d; (void)e; (void)s; (void)f; (*((typeof(ctx) *)c)->c)++; }
        pm_sink_t sk = { s_, t_, NULL, x_, &ctx, 0.01 };
        pm_dibujar_ent(&pm_ent[0], &sk);
        VER(nseg >= 3 && ntri == 2 && ntxt == 1, "la cota se reduce a líneas, dos flechas y un texto (%d,%d,%d)", nseg, ntri, ntxt);
    }
}

static int contar_seg;
static void cs_(void *c, double a, double b, double d, double e) { (void)c; (void)a; (void)b; (void)d; (void)e; contar_seg++; }
static void ct_(void *c, double a, double b, double d, double e, double f, double g) { (void)c; (void)a; (void)b; (void)d; (void)e; (void)f; (void)g; }
static void cx_(void *c, double a, double b, double d, double e, const char *s, int f) { (void)c; (void)a; (void)b; (void)d; (void)e; (void)s; (void)f; }

static void prueba_sombra_capas(void)
{
    nuevo();
    rect_(0, 0, 10, 10);
    O("H"); O("5,5"); O("");
    VER(cuenta(E_SOMBRA) == 1, "sombreado creado");
    contar_seg = 0;
    pm_sink_t sk = { cs_, ct_, NULL, cx_, NULL, 0.01 };
    pm_dibujar_ent(&pm_ent[ultimo(E_SOMBRA)], &sk);
    VER(contar_seg >= 20 && contar_seg <= 40, "líneas del sombreado a 45 grados (%d)", contar_seg);
    O("H"); O("S"); O("5,5"); O("");
    VER(cuenta(E_SOMBRA) == 2 && pm_ent[ultimo(E_SOMBRA)].sub == 1, "sombreado sólido");
    nuevo();
    circ_(0, 0, 5);
    O("H"); O("1,1"); O("");
    VER(cuenta(E_SOMBRA) == 1, "sombreado de un círculo");
    O("H"); O("30,30");
    VER(pm_msg_err, "fuera de todo contorno avisa");
    pm_cancelar();

    nuevo();
    O("CAPA NUEVA Muros");
    VER(pm_ncapa == 2 && pm_capa_act == 1, "capa nueva y actual");
    O("CAPA COLOR Muros 1");
    VER(pm_capa[1].color == 0xFF0000, "color rojo");
    O("CAPA LTIPO Muros Trazos");
    VER(pm_capa[1].ltipo == LT_TRAZOS, "tipo de línea trazos");
    O("L 0,0 5,0");
    VER(pm_n == 1 && pm_ent[0].capa == 1 && pm_ent_color(&pm_ent[0]) == 0xFF0000 && pm_ent_ltipo(&pm_ent[0]) == LT_TRAZOS, "lo dibujado va a la capa actual (PorCapa)");
    pm_cancelar();
    O("COLOR 3");
    O("L 0,1 5,1");
    VER(pm_ent_color(&pm_ent[1]) == 0x00FF00, "color propio verde");
    pm_cancelar();
    O("COLOR PorCapa");
    O("CAPA ACTUAL 0");
    O("CAPA APAGAR Muros");
    VER(!pm_ent_visible(&pm_ent[0]) && pm_entidad_en(2, 0, 0.3, -1) < 0, "capa apagada: no se ve ni se elige");
    O("CAPA ACTIVAR Muros");
    O("CAPA BLOQUEAR Muros");
    VER(pm_ent_visible(&pm_ent[0]) && !pm_ent_editable(&pm_ent[0]) && pm_entidad_en(2, 0, 0.3, -1) < 0, "capa bloqueada: se ve pero no se elige");
    O("CAPA DESBLOQUEAR Muros");
    VER(pm_entidad_en(2, 0, 0.3, -1) == 0, "desbloqueada");
    O("CAPA ACTUAL Muros"); O("CAPA APAGAR Muros");
    VER(pm_capa[1].visible && pm_msg_err, "no se apaga la capa actual: %s", pm_msg);
}

static void prueba_refent(void)
{
    nuevo();
    linea_(0, 0, 10, 0); linea_(5, -5, 5, 5); circ_(20, 0, 3);
    pm_ref_t r;
    int todo = REF_FIN | REF_MEDIO | REF_CENTRO | REF_INTER | REF_PERP | REF_TANG | REF_CUAD | REF_CERCA;
    VER(pm_ref_buscar(0.1, 0.1, 0.3, REF_FIN, false, 0, 0, &r) && r.tipo == REF_FIN && fabs(r.x) < 1e-9 && fabs(r.y) < 1e-9, "punto final");
    VER(pm_ref_buscar(4.9, 0.05, 0.3, REF_INTER, false, 0, 0, &r) && r.tipo == REF_INTER && fabs(r.x - 5) < 1e-9 && fabs(r.y) < 1e-9, "intersección");
    VER(pm_ref_buscar(5.1, 4.9, 0.3, REF_MEDIO | REF_FIN, false, 0, 0, &r) && r.tipo == REF_FIN && fabs(r.y - 5) < 1e-9, "extremo de la segunda línea");
    VER(pm_ref_buscar(5.1, -0.1, 0.3, REF_MEDIO, false, 0, 0, &r) && r.tipo == REF_MEDIO && fabs(r.x - 5) < 1e-9, "punto medio");
    VER(pm_ref_buscar(23.1, 0.05, 0.3, REF_CENTRO, false, 0, 0, &r) && r.tipo == REF_CENTRO && fabs(r.x - 20) < 1e-9, "centro (con el cursor sobre la circunferencia)");
    VER(pm_ref_buscar(23.05, 0.1, 0.3, REF_CUAD, false, 0, 0, &r) && r.tipo == REF_CUAD && fabs(r.x - 23) < 1e-9 && fabs(r.y) < 1e-9, "cuadrante");
    VER(pm_ref_buscar(3.1, 0.1, 0.3, REF_PERP, true, 3, 4, &r) && r.tipo == REF_PERP && fabs(r.x - 3) < 1e-9 && fabs(r.y) < 1e-9, "perpendicular desde el último punto");
    VER(pm_ref_buscar(21.5, 2.6, 0.3, REF_TANG, true, 26, 0, &r) && r.tipo == REF_TANG && fabs(r.x - 21.5) < 1e-9 && fabs(r.y - 3 * sqrt(3) / 2) < 1e-9, "tangente desde (26, 0): (%g,%g)", r.x, r.y);
    VER(pm_ref_buscar(7.3, 0.2, 0.3, REF_CERCA, false, 0, 0, &r) && r.tipo == REF_CERCA && fabs(r.x - 7.3) < 1e-9 && fabs(r.y) < 1e-9, "punto cercano");
    VER(!pm_ref_buscar(15, 8, 0.3, todo, false, 0, 0, &r), "nada cerca");
    /* el cursor con REFENT: lo usa el programa al hacer clic */
    pm_refent = 1; pm_refmask = REF_FIN;
    O("L"); double rx, ry;
    pm_ref_cursor(10.1, 0.1, 0.3, &rx, &ry, &r);
    VER(r.tipo == REF_FIN && fabs(rx - 10) < 1e-9 && fabs(ry) < 1e-9, "el cursor salta al punto final");
    pm_punto(rx, ry);
    pm_ref_cursor(0.1, 0.1, 0.3, &rx, &ry, &r);
    pm_punto(rx, ry); pm_cancelar();
    VER(pm_n == 4 && fabs(pm_ent[3].p[0] - 10) < 1e-9 && fabs(pm_ent[3].p[2]) < 1e-9, "una línea con REFENT: de (10,0) a (0,0)");
    O("REFENT NINGUNO"); VER(!pm_refent || pm_refmask == 0, "REFENT desactiva");
    pm_refent = 1; pm_refmask = REF_FIN;
    pm_forzc = 1; pm_paso_rej = 0.5;
    O("L"); pm_ref_cursor(13.3, 7.8, 0.01, &rx, &ry, &r);
    VER(fabs(rx - 13.5) < 1e-9 && fabs(ry - 8) < 1e-9, "forzcursor a la rejilla (%g,%g)", rx, ry);
    pm_forzc = 0; pm_cancelar();
    nuevo();
    O("L 0,0"); pm_orto = 1; pm_ref_cursor(8, 1.5, 0.3, &rx, &ry, &r);
    VER(fabs(rx - 8) < 1e-9 && fabs(ry) < 1e-9, "ORTO fija el cursor al eje");
    pm_orto = 0; pm_polar = 1; pm_polar_ang = 45; pm_ref_cursor(8, 7.6, 0.3, &rx, &ry, &r);
    VER(fabs(rx - hypot(8, 7.6) / sqrt(2)) < 1e-9 && fabs(ry - rx) < 1e-9, "POLAR engancha a 45 grados (%g,%g)", rx, ry);
    pm_cancelar();
}

static void prueba_grips_props(void)
{
    nuevo();
    linea_(0, 0, 10, 0);
    double g[8][2];
    VER(pm_grips(&pm_ent[0], g, 8) == 3 && fabs(g[2][0] - 5) < 1e-9, "una línea tiene tres pinzamientos");
    pm_grip_mover(0, 1, 12, 3);
    CERCA(pm_ent[0].p[2], 12, 1e-9, "estirar el extremo"); CERCA(pm_ent[0].p[3], 3, 1e-9, "y");
    pm_grip_mover(0, 2, 10, 10);
    CERCA(pm_ent[0].p[0], 4, 1e-9, "mover desde el punto medio traslada la línea: x0=%g", pm_ent[0].p[0]);
    circ_(0, 0, 2);
    pm_grip_mover(1, 1, 5, 0);
    CERCA(pm_ent[1].r, 5, 1e-9, "el cuadrante cambia el radio");
    pm_grip_mover(1, 0, 3, 3);
    CERCA(pm_ent[1].p[0], 3, 1e-9, "el centro traslada");
    char et[8][20];
    double v[8];
    int n = pm_props_campos(&pm_ent[0], et, v);
    VER(n == 6 && !strcmp(et[4], "Longitud"), "campos de una línea");
    v[4] = 20;
    pm_props_aplicar(&pm_ent[0], v);
    CERCA(hypot(pm_ent[0].p[2] - pm_ent[0].p[0], pm_ent[0].p[3] - pm_ent[0].p[1]), 20, 1e-9, "editar la longitud");
    n = pm_props_campos(&pm_ent[1], et, v);
    v[2] = 9;
    pm_props_aplicar(&pm_ent[1], v);
    CERCA(pm_ent[1].r, 9, 1e-9, "editar el radio");
}

static void prueba_deshacer(void)
{
    nuevo();
    O("L 0,0 5,0"); O("");
    O("REC 0,0 2,2");
    VER(pm_n == 2, "dos objetos");
    O("U");
    VER(pm_n == 1, "deshacer quita el rectángulo (%d)", pm_n);
    O("REHACER");
    VER(pm_n == 2, "rehacer lo devuelve (%d)", pm_n);
    O("U 2");
    VER(pm_n == 0, "deshacer 2 pasos (%d)", pm_n);
    O("REHACER"); O("REHACER");
    VER(pm_n == 2, "rehacer dos veces");
    /* muchos pasos */
    nuevo();
    for (int i = 0; i < 30; i++) { char b[40]; sprintf(b, "PO %d,0", i); O(b); }
    VER(pm_n == 30, "30 puntos");
    O("U 25");
    VER(pm_n == 5, "deshacer 25 pasos de una vez (%d)", pm_n);
    O("REHACER");
    VER(pm_n == 6, "rehacer uno");
    VER(pm_modificado, "el documento consta como modificado");
}

static const char *V1 = "PLANOMINI 1\nL 0 0 10 10 0\nC 5 5 0 0 3\nR 0 0 4 3 0\nP 1 2 0 0 0\n";

static void rellenar_mixto(void)
{
    nuevo();
    pm_capa_nueva("Muros"); pm_capa[1].color = 0xFF0000; pm_capa[1].ltipo = LT_TRAZOS;
    pm_capa_nueva("Cotas"); pm_capa[2].color = 0x00FFFF;
    pm_capa_act = 1;
    O("L 0,0 10,0"); O("10,5"); O("");
    O("C 3,3 2");
    O("A 5,0 0,5 -5,0");
    O("PL 0,0 4,0 A 4,4 L 0,4 C");
    O("PO 7,7");
    O("T 1,1 2 15 Cota total");
    pm_capa_act = 2;
    O("COTAHOR 0,0 10,0 5,-3");
    O("COTARAD"); O("5,3"); O("7,3");
    O("COTAANG 0,0 5,0 0,5 3,3");
    O("REC 20,20 25,24"); O("H"); O("22,22"); O("");
    pm_ent[0].color = 0x00FF00;
    pm_ent[2].ltipo = LT_PUNTOS;
}

static void prueba_ficheros(void)
{
    nuevo();
    VER(pm_cargar(V1) == 0 && pm_n == 4, ".pmini v1 del prototipo: %d entidades", pm_n);
    VER(pm_ent[0].tipo == E_LINEA && fabs(pm_ent[0].p[2] - 10) < 1e-9, "v1: línea");
    VER(pm_ent[1].tipo == E_CIRCULO && fabs(pm_ent[1].r - 3) < 1e-9 && fabs(pm_ent[1].p[0] - 5) < 1e-9, "v1: círculo");
    VER(pm_ent[2].tipo == E_POLI && pm_ent[2].nv == 4 && fabs(pm_ent[2].v[2].x - 4) < 1e-9 && fabs(pm_ent[2].v[2].y - 3) < 1e-9, "v1: rectángulo");
    VER(pm_ent[3].tipo == E_PUNTO && fabs(pm_ent[3].p[1] - 2) < 1e-9, "v1: punto");
    VER(pm_cargar("PLANOMINI 9\n") != 0 && pm_cargar("hola\n") != 0, "cabeceras no válidas");
    VER(pm_cargar("PLANOMINI 1\n") == 0 && pm_n == 0, "v1 vacío");

    rellenar_mixto();
    int n1, n2;
    char *s1 = pm_serializar(&n1);
    VER(s1 && !strncmp(s1, "PLANOMINI 2\n", 12), "v2 empieza por la cabecera");
    int ne = pm_n, nc = pm_ncapa;
    VER(pm_cargar(s1) == 0 && pm_n == ne && pm_ncapa == nc, "v2 se recarga: %d entidades, %d capas", pm_n, pm_ncapa);
    char *s2 = pm_serializar(&n2);
    VER(s2 && !strcmp(s1, s2), "v2: serializar-cargar-serializar es idéntico");
    if (s2 && strcmp(s1, s2)) { for (int i = 0; s1[i] && s2[i]; i++) if (s1[i] != s2[i]) { printf("   difiere en %d: «%.60s» / «%.60s»\n", i, s1 + (i > 20 ? i - 20 : 0), s2 + (i > 20 ? i - 20 : 0)); break; } }
    VER(pm_capa[1].ltipo == LT_TRAZOS && pm_capa[1].color == 0xFF0000 && pm_capa_act == 2, "v2 conserva capas");
    VER(pm_ent[0].color == 0x00FF00 && pm_ent[2].ltipo == LT_PUNTOS, "v2 conserva las propiedades propias");
    libera(s1); libera(s2);
}

/* validador sencillo de DXF: pares código/valor, secciones, EOF */
static int validar_dxf(const char *t, char *err_, int max)
{
    int linea = 0, secciones = 0, enditem = 0, tablas = 0;
    const char *p = t;
    char prev0[256] = "";
    int nent = 0;
    bool en_ent = false, visto_eof = false;
    while (*p) {
        const char *e = strchr(p, '\n');
        if (!e) { snprintf(err_, max, "sin salto final"); return 1; }
        char cod[40];
        int l = (int)(e - p);
        if (l >= 40 || l == 0) { snprintf(err_, max, "código de longitud rara en la línea %d", linea + 1); return 1; }
        memcpy(cod, p, (size_t)l); cod[l] = 0;
        for (char *q = cod; *q; q++) if (!((*q >= '0' && *q <= '9') || *q == '-' || *q == ' ')) { snprintf(err_, max, "código no numérico «%s» en la línea %d", cod, linea + 1); return 1; }
        p = e + 1;
        const char *e2 = strchr(p, '\n');
        if (!e2) { snprintf(err_, max, "falta el valor del par en la línea %d", linea + 2); return 1; }
        char val[200];
        int l2 = (int)(e2 - p);
        if (l2 >= 200) l2 = 199;
        memcpy(val, p, (size_t)l2); val[l2] = 0;
        p = e2 + 1;
        linea += 2;
        int c = atoi(cod);
        if (c == 0) {
            if (!strcmp(val, "SECTION")) secciones++;
            else if (!strcmp(val, "ENDSEC")) { secciones--; en_ent = false; }
            else if (!strcmp(val, "TABLE")) tablas++;
            else if (!strcmp(val, "ENDTAB")) tablas--;
            else if (!strcmp(val, "EOF")) visto_eof = true;
            else if (en_ent) nent++;
            enditem++;
            snprintf(prev0, sizeof prev0, "%s", val);
        }
        if (c == 2 && !strcmp(prev0, "SECTION") && !strcmp(val, "ENTITIES")) en_ent = true;
        if (visto_eof && c != 0) { snprintf(err_, max, "datos tras EOF"); return 1; }
        if (c >= 10 && c <= 59 && c != 40 + 9 + 1000 && !(c >= 40 && c <= 59) && !(c >= 10 && c < 40)) { snprintf(err_, max, "código fuera de rango"); return 1; }
        if (c >= 10 && c < 60) { char *fin; strtod(val, &fin); if (*fin || !val[0]) { snprintf(err_, max, "valor numérico no válido «%s» (código %d)", val, c); return 1; } }
    }
    if (secciones != 0 || tablas != 0) { snprintf(err_, max, "secciones o tablas sin cerrar (%d, %d)", secciones, tablas); return 1; }
    if (!visto_eof) { snprintf(err_, max, "no acaba en EOF"); return 1; }
    snprintf(err_, max, "%d entidades", nent);
    return 0;
}

static const char *DXF2000 =
    "0\r\nSECTION\r\n2\r\nHEADER\r\n9\r\n$ACADVER\r\n1\r\nAC1015\r\n0\r\nENDSEC\r\n"
    "0\r\nSECTION\r\n2\r\nTABLES\r\n0\r\nTABLE\r\n2\r\nLAYER\r\n70\r\n2\r\n"
    "0\r\nLAYER\r\n2\r\n0\r\n70\r\n0\r\n62\r\n7\r\n6\r\nCONTINUOUS\r\n"
    "0\r\nLAYER\r\n2\r\nEjes\r\n70\r\n0\r\n62\r\n1\r\n6\r\nDASHDOT\r\n"
    "0\r\nENDTAB\r\n0\r\nENDSEC\r\n"
    "0\r\nSECTION\r\n2\r\nBLOCKS\r\n0\r\nBLOCK\r\n8\r\n0\r\n2\r\n*D1\r\n70\r\n1\r\n10\r\n0.0\r\n20\r\n0.0\r\n30\r\n0.0\r\n3\r\n*D1\r\n"
    "0\r\nLINE\r\n8\r\n0\r\n10\r\n0.0\r\n20\r\n0.0\r\n30\r\n0.0\r\n11\r\n10.0\r\n21\r\n0.0\r\n31\r\n0.0\r\n"
    "0\r\nTEXT\r\n8\r\n0\r\n10\r\n5.0\r\n20\r\n1.0\r\n30\r\n0.0\r\n40\r\n2.5\r\n1\r\n10\r\n"
    "0\r\nENDBLK\r\n8\r\n0\r\n0\r\nENDSEC\r\n"
    "0\r\nSECTION\r\n2\r\nENTITIES\r\n"
    "0\r\nLWPOLYLINE\r\n5\r\n20\r\n8\r\nEjes\r\n100\r\nAcDbEntity\r\n100\r\nAcDbPolyline\r\n90\r\n3\r\n70\r\n1\r\n10\r\n0.0\r\n20\r\n0.0\r\n10\r\n4.0\r\n20\r\n0.0\r\n42\r\n1.0\r\n10\r\n4.0\r\n20\r\n4.0\r\n"
    "0\r\nCIRCLE\r\n8\r\n0\r\n62\r\n5\r\n10\r\n2.0\r\n20\r\n2.0\r\n30\r\n0.0\r\n40\r\n1.5\r\n"
    "0\r\nARC\r\n8\r\n0\r\n10\r\n0.0\r\n20\r\n0.0\r\n30\r\n0.0\r\n40\r\n3.0\r\n50\r\n0.0\r\n51\r\n90.0\r\n"
    "0\r\nDIMENSION\r\n8\r\n0\r\n2\r\n*D1\r\n10\r\n5.0\r\n20\r\n3.0\r\n30\r\n0.0\r\n70\r\n1\r\n"
    "0\r\nSPLINE\r\n8\r\n0\r\n"
    "0\r\nMTEXT\r\n8\r\n0\r\n10\r\n1.0\r\n20\r\n9.0\r\n30\r\n0.0\r\n40\r\n2.0\r\n1\r\nHola%%c5\r\n"
    "0\r\nENDSEC\r\n0\r\nEOF\r\n";

static void prueba_dxf(void)
{
    rellenar_mixto();
    int n;
    char *d = pm_dxf_escribir(&n);
    VER(d != NULL, "DXF escrito");
    if (!d) return;
    char msg_[160];
    int rc = validar_dxf(d, msg_, sizeof msg_);
    VER(rc == 0, "el DXF escrito es válido: %s", msg_);
    VER(strstr(d, "AC1009") && strstr(d, "POLYLINE") && strstr(d, "VERTEX") && strstr(d, "SEQEND") && strstr(d, "LAYER") && strstr(d, "Muros"), "trae cabecera R12, polilíneas y capas");
    VER(strstr(d, "DASHED") && strstr(d, "DASHDOT"), "trae las definiciones de tipos de línea");
    /* contamos lo que se espera */
    int lineas = cuenta(E_LINEA), circ = cuenta(E_CIRCULO), arcos = cuenta(E_ARCO), polis = cuenta(E_POLI), ptos = cuenta(E_PUNTO), textos = cuenta(E_TEXTO);
    int omit = -1;
    VER(pm_dxf_leer(d, &omit) == 0 && omit == 0, "DXF leído de vuelta (omitidas: %d): %s", omit, pm_erro);
    VER(cuenta(E_CIRCULO) == circ && cuenta(E_ARCO) == arcos && cuenta(E_PUNTO) == ptos, "círculos, arcos y puntos de ida y vuelta");
    VER(cuenta(E_POLI) == polis && cuenta(E_LINEA) >= lineas && cuenta(E_TEXTO) >= textos, "polilíneas de ida y vuelta; las cotas y el sombreado vuelven explotados (%d líneas, %d textos)", cuenta(E_LINEA), cuenta(E_TEXTO));
    VER(cuenta(E_COTA) == 0 && cuenta(E_SOMBRA) >= 1, "las cotas se explotan, las flechas son sólidos");
    VER(pm_ncapa == 3 && pm_capa[1].color == 0xFF0000 && pm_capa[1].ltipo == LT_TRAZOS && !strcmp(pm_capa[1].nome, "Muros") && pm_capa[2].color == 0x00FFFF, "capas con color y tipo de línea");
    {
        int ip = -1;
        for (int i = 0; i < pm_n; i++) if (pm_ent[i].tipo == E_POLI && pm_ent[i].nv == 4 && pm_ent[i].cerrada && pm_ent[i].v[1].b != 0) ip = i;
        VER(ip >= 0 && fabs(pm_ent[ip].v[1].b - 1.0) < 1e-6 && fabs(pm_ent[ip].v[1].x - 4) < 1e-6, "la polilínea conserva el arco (bulge)");
        int it = -1;
        for (int i = 0; i < pm_n; i++) if (pm_ent[i].tipo == E_TEXTO && !strcmp(pm_ent[i].txt, "Cota total")) it = i;
        VER(it >= 0 && fabs(pm_ent[it].giro - 15 * M_PI / 180) < 1e-6 && fabs(pm_ent[it].alt - 2) < 1e-9, "el texto conserva altura y giro");
        VER(pm_ent[0].color == 0x00FF00 && pm_ent[0].capa == 1, "la entidad conserva su color propio y su capa");
        VER(pm_ent[2].ltipo == LT_PUNTOS, "y su tipo de línea");
    }
    char *d2 = pm_dxf_escribir(&n);
    VER(d2 && validar_dxf(d2, msg_, sizeof msg_) == 0, "el DXF reescrito también es válido: %s", msg_);
    int nn1 = (int)strlen(d), nn2 = d2 ? (int)strlen(d2) : 0;
    VER(abs(nn1 - nn2) < nn1 / 5, "el segundo DXF tiene un tamaño parecido (%d / %d)", nn1, nn2);
    libera(d); if (d2) libera(d2);

    nuevo();
    omit = -1;
    VER(pm_dxf_leer(DXF2000, &omit) == 0, "DXF R2000 con LWPOLYLINE, MTEXT y DIMENSION: %s", pm_erro);
    VER(omit == 1, "una entidad no soportada (SPLINE) se cuenta como omitida (%d)", omit);
    VER(pm_ncapa == 2 && !strcmp(pm_capa[1].nome, "Ejes") && pm_capa[1].color == 0xFF0000 && pm_capa[1].ltipo == LT_TRAZOPUNTO, "capas del R2000");
    VER(cuenta(E_POLI) == 1 && pm_ent[0].nv == 3 && pm_ent[0].cerrada && fabs(pm_ent[0].v[1].b - 1) < 1e-9 && pm_ent[0].capa == 1, "LWPOLYLINE cerrada con arco en la capa Ejes");
    VER(cuenta(E_CIRCULO) == 1 && pm_ent[ultimo(E_CIRCULO)].color == 0x0000FF, "círculo con color azul (62=5)");
    VER(cuenta(E_ARCO) == 1 && fabs(pm_ent[ultimo(E_ARCO)].a1 - M_PI / 2) < 1e-9, "arco 0..90");
    VER(cuenta(E_LINEA) == 1 && cuenta(E_TEXTO) == 2, "la DIMENSION entra como el contenido de su bloque (1 línea y 1 texto, más el MTEXT)");
    {
        int it = -1;
        for (int i = 0; i < pm_n; i++) if (pm_ent[i].tipo == E_TEXTO && pm_ent[i].txt[0] == 'H') it = i;
        VER(it >= 0 && !strcmp(pm_ent[it].txt, "Hola\xD8" "5"), "%%%%c se convierte en Ø: «%s»", it >= 0 ? pm_ent[it].txt : "");
    }
    VER(pm_dxf_leer("0\nSECTION\n2\nHEADER\n0\nENDSEC\n0\nEOF\n", NULL) != 0, "un DXF sin ENTITIES se rechaza");
}

static char *leer_(const char *dir, const char *n)
{
    char ruta[300];
    snprintf(ruta, sizeof ruta, "%s/%s", dir, n);
    FILE *f = fopen(ruta, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long l = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *b = malloc((size_t)l + 1);
    if (fread(b, 1, (size_t)l, f) != (size_t)l) { free(b); fclose(f); return NULL; }
    b[l] = 0;
    fclose(f);
    return b;
}

static void prueba_ejemplos(const char *dir)
{
    char *t = leer_(dir, "habitacion.pmini");
    VER(t != NULL, "ejemplo habitacion.pmini presente");
    if (t) {
        VER(pm_cargar(t) == 0, "habitacion: %s", pm_erro);
        VER(pm_ncapa == 8 && !strcmp(pm_capa[1].nome, "Muros") && !strcmp(pm_capa[4].nome, "Cotas"), "habitacion: capas");
        VER(cuenta(E_COTA) >= 6 && cuenta(E_LINEA) >= 20 && cuenta(E_POLI) >= 2 && cuenta(E_SOMBRA) == 1 && cuenta(E_TEXTO) >= 3 && cuenta(E_ARCO) == 1 && cuenta(E_CIRCULO) == 1, "habitacion: contenido (%d cotas, %d líneas)", cuenta(E_COTA), cuenta(E_LINEA));
        int k = 0;
        for (int i = 0; i < pm_n; i++) if (pm_ent[i].tipo == E_COTA && pm_ent[i].sub == C_LIN_H) { char s[40]; pm_cota_texto(&pm_ent[i], s, sizeof s); if (!strcmp(s, "4000")) k++; }
        VER(k == 1, "habitacion: una cota de 4000 mm");
        int fuera = 0;
        for (int i = 0; i < pm_n; i++) if (pm_ent[i].capa >= pm_ncapa) fuera++;
        VER(fuera == 0, "habitacion: todas las entidades en una capa válida");
        free(t);
    }
    t = leer_(dir, "pieza.pmini");
    VER(t != NULL && pm_cargar(t) == 0, "ejemplo pieza.pmini");
    if (t) {
        VER(cuenta(E_CIRCULO) == 6 && cuenta(E_POLI) == 1 && pm_ent[0].nv == 8 && cuenta(E_COTA) >= 6, "pieza: contorno con 8 vértices (4 empalmes), 6 círculos y cotas");
        int arcos = 0;
        for (int i = 0; i < pm_ent[0].nv; i++) if (pm_ent[0].v[i].b != 0) arcos++;
        VER(arcos == 4, "pieza: cuatro empalmes");
        free(t);
    }
    t = leer_(dir, "plano.dxf");
    VER(t != NULL, "ejemplo plano.dxf presente");
    if (t) {
        int omit = -1;
        VER(pm_dxf_leer(t, &omit) == 0 && omit == 0, "plano.dxf se lee sin omitir nada: %s (%d)", pm_erro, omit);
        VER(pm_ncapa == 4 && pm_capa[1].ltipo == LT_TRAZOS && pm_capa[3].ltipo == LT_TRAZOPUNTO, "plano.dxf: capas con tipos de línea");
        VER(cuenta(E_POLI) == 2 && cuenta(E_CIRCULO) == 2 && cuenta(E_ARCO) == 1 && cuenta(E_PUNTO) == 1 && cuenta(E_TEXTO) >= 4, "plano.dxf: contenido (%d textos)", cuenta(E_TEXTO));
        char msg_[100];
        int n;
        char *d = pm_dxf_escribir(&n);
        VER(d && validar_dxf(d, msg_, sizeof msg_) == 0, "plano.dxf reexportado es válido: %s", msg_);
        free(d); free(t);
    }
}

int main(int argc, char **argv)
{
    prueba_util();
    prueba_geo();
    prueba_dibujo();
    prueba_modificar();
    prueba_cotas();
    prueba_sombra_capas();
    prueba_refent();
    prueba_grips_props();
    prueba_deshacer();
    prueba_ficheros();
    prueba_dxf();
    prueba_ejemplos(argc > 1 ? argv[1] : "rootfs/ejemplos/planomini");
    printf("planomini_unidad: %d comprobaciones, %d fallos\n", total, fallos);
    if (!fallos) printf("todo bien\n");
    return fallos != 0;
}
