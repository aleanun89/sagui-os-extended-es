// SPDX-License-Identifier: GPL-3.0-or-later
// PlanoMini, © 2026 Alejandro Anuncibay: prototipo original (C++/SDL) del mantenedor; versión nativa de SaguiOS.
/* pm_cmd.c - la línea de órdenes de PlanoMini (D312): cada orden es una pequeña máquina de estados que
 * pide puntos, números, objetos u opciones; la entrada llega por el teclado (coordenadas absolutas x,y,
 * relativas @dx,dy, polares @d<a, solo distancia con ORTO/POLAR) o por clics. Sin dibujo ni sistema. */
#include "pm.h"

int  pm_esp;
char pm_prompt[200];
char pm_msg[160];
bool pm_msg_err;
int  pm_accion;
char pm_accion_arg[200];
bool pm_tiene_ult;
double pm_ult_x, pm_ult_y, pm_cur_x, pm_cur_y;
bool pm_quitar;
bool pm_ref_siempre;
int  pm_hist_n;
bool pm_modificado;

enum { V_INIT, V_PT, V_NUM, V_OPC, V_ENT, V_TXT, V_SEL, V_OBJ };
typedef void (*hfn)(int ev, double x, double y, int k, const char *t);
enum { F_SEL = 1, F_INM = 2 };
typedef struct { const char *nombres; hfn fn; int flags; } cmddef_t;

/* ============================================================ registro de la línea de órdenes */

#define HMAX 200
static char hlog[HMAX][120];
const char *pm_hist_linea(int i) { return i >= 0 && i < pm_hist_n ? hlog[i] : ""; }
static void log_(const char *t)
{
    if (pm_hist_n == HMAX) { memmove(hlog[0], hlog[1], sizeof(hlog[0]) * (HMAX - 1)); pm_hist_n--; }
    copia_texto(hlog[pm_hist_n++], t, sizeof hlog[0]);
}
static void msg(const char *t) { copia_texto(pm_msg, t, sizeof pm_msg); pm_msg_err = false; log_(t); }
static void err(const char *t) { copia_texto(pm_msg, t, sizeof pm_msg); pm_msg_err = true; log_(t); }
static const char *F(double v)
{
    static char b[6][32];
    static int k;
    k = (k + 1) % 6;
    pm_fmt(v, 4, b[k], 32);
    return b[k];
}
static void msg2(const char *a, const char *b, const char *c_)
{
    char t[160];
    formatar_texto(t, sizeof t, "%s%s%s", a, b, c_);
    msg(t);
}

/* ============================================================ estado de la orden activa */

static bool act;
static const cmddef_t *cur;
static struct {
    int paso;
    double pt[8][2];
    int npt;
    double n[4];
    int i[4];
    int f[4];
    char args[160];
    pm_ent_t tmp;
    double tang;
} c;
static char opts_s[200];
static int nopc;
static bool num_ang, num_pt;          /* el paso numérico es un ángulo (grados) / acepta clics como puntos */

static double ap(void) { return 8.0 / pm_ve; }

static int opc_contar(void)
{
    int n = 0;
    if (opts_s[0]) { n = 1; for (const char *q = opts_s; *q; q++) if (*q == ';') n++; }
    return n;
}
static void pide(int esp, const char *prompt, const char *opts)
{
    pm_esp = esp;
    num_ang = false; num_pt = false;
    opts_s[0] = 0;
    if (esp == ESP_SEL) copia_texto(opts_s, "Todo,All;Ultimo,Last", sizeof opts_s);
    if (opts && opts[0]) {
        if (opts_s[0]) { int k = (int)tam_texto(opts_s); opts_s[k] = ';'; copia_texto(opts_s + k + 1, opts, sizeof opts_s - (size_t)k - 1); }
        else copia_texto(opts_s, opts, sizeof opts_s);
    }
    nopc = opc_contar();
    char lista[160];
    lista[0] = 0;
    int n = 0;
    if (opts && opts[0]) {
        n = 0;
        const char *q = opts;
        for (int o = 0; *q; o++) {
            if (n) lista[n++] = '/';
            while (*q && *q != ',' && *q != ';' && n < 150) lista[n++] = *q++;
            while (*q && *q != ';') q++;
            if (*q == ';') q++;
        }
        lista[n] = 0;
    }
    char nom[24];
    copia_texto(nom, cur ? cur->nombres : "", sizeof nom);
    for (char *q = nom; *q; q++) if (*q == ',') { *q = 0; break; }
    if (lista[0]) formatar_texto(pm_prompt, sizeof pm_prompt, "%s: %s [%s]:", nom, prompt, lista);
    else formatar_texto(pm_prompt, sizeof pm_prompt, "%s: %s:", nom, prompt);
}

static char fold(char c_)
{
    unsigned char u = (unsigned char)c_;
    if (c_ >= 'a' && c_ <= 'z') return (char)(c_ - 32);
    if (u == 0xE1 || u == 0xC1 || u == 0xE0 || u == 0xC0) return 'A';
    if (u == 0xE9 || u == 0xC9 || u == 0xE8 || u == 0xC8) return 'E';
    if (u == 0xED || u == 0xCD) return 'I';
    if (u == 0xF3 || u == 0xD3) return 'O';
    if (u == 0xFA || u == 0xDA || u == 0xFC || u == 0xDC) return 'U';
    if (u == 0xF1 || u == 0xD1) return 'N';
    return c_;
}
static bool prefijo(const char *p, const char *t)   /* p es prefijo de t (sin mayúsculas ni tildes) */
{
    if (!*p) return false;
    for (; *p; p++, t++) if (!*t || fold(*p) != fold(*t)) return false;
    return true;
}
static int op_match(const char *tk)
{
    const char *q = opts_s;
    for (int o = 0; *q; o++) {
        char es[24], en[24];
        int a = 0, b = 0;
        while (*q && *q != ',' && *q != ';' && a < 23) es[a++] = *q++;
        es[a] = 0;
        if (*q == ',') { q++; while (*q && *q != ';' && b < 23) en[b++] = *q++; }
        en[b] = 0;
        while (*q && *q != ';') q++;
        if (*q == ';') q++;
        if (prefijo(tk, es) || prefijo(tk, en)) return o;
    }
    return -1;
}

static void fin(void)
{
    if (c.tmp.v) pm_ent_liberar(&c.tmp);
    act = false; cur = NULL;
    pm_esp = ESP_NADA;
    pm_prompt[0] = 0;
    pm_sel_todo(false);
    memset(&c, 0, sizeof c);
    pm_historial_guardar();
}
static void sig_pt(double x, double y) { pm_tiene_ult = true; pm_ult_x = x; pm_ult_y = y; }

/* Una entidad nueva con las propiedades actuales. */
static int nueva(pm_ent_t *e) { int r = pm_anadir_actual(e); if (r < 0) err(pm_erro[0] ? pm_erro : "No se pudo crear"); return r; }

static void aplicar_sel(const double *m, bool copiar)
{
    int n0 = pm_n;
    for (int i = 0; i < n0; i++) {
        if (!pm_ent[i].sel) continue;
        if (copiar) {
            pm_ent_t t;
            pm_ent_clonar(&t, &pm_ent[i]);
            pm_ent_transformar(&t, m);
            pm_anadir(&t);
            pm_ent_liberar(&t);
        } else pm_ent_transformar(&pm_ent[i], m);
    }
}

/* ============================================================ órdenes de dibujo */

static void h_punto(int ev, double x, double y, int k, const char *t)
{
    (void)k; (void)t;
    if (ev == V_INIT) { pide(ESP_PUNTO, "Posición del punto", ""); return; }
    if (ev == V_PT) {
        pm_ent_t e;
        pm_ent_nueva(&e, E_PUNTO);
        e.p[0] = x; e.p[1] = y;
        nueva(&e); sig_pt(x, y);
        msg2("Punto creado en ", F(x), "");
        fin();
    } else if (ev == V_ENT) fin();
}

static void pide_linea(void)
{
    pide(ESP_PUNTO, "Punto siguiente", c.i[0] >= 2 ? "Deshacer,Undo;Cerrar,Close" : c.i[0] >= 1 ? "Deshacer,Undo" : "");
}
static void h_linea(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    if (c.paso == 0) {
        if (ev == V_INIT) { pide(ESP_PUNTO, "Primer punto", ""); return; }
        if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 1; pide_linea(); }
        else if (ev == V_ENT) fin();
        return;
    }
    if (ev == V_PT) {
        pm_ent_t e;
        pm_ent_nueva(&e, E_LINEA);
        e.p[0] = pm_ult_x; e.p[1] = pm_ult_y; e.p[2] = x; e.p[3] = y;
        if (nueva(&e) >= 0) c.i[0]++;
        sig_pt(x, y);
        pide_linea();
    } else if (ev == V_OPC) {
        if (k == 0) {      /* Deshacer: quita el último segmento */
            if (c.i[0] > 0 && pm_n > 0) { sig_pt(pm_ent[pm_n - 1].p[0], pm_ent[pm_n - 1].p[1]); pm_borrar(pm_n - 1); c.i[0]--; }
            pide_linea();
        } else {            /* Cerrar */
            pm_ent_t e;
            pm_ent_nueva(&e, E_LINEA);
            e.p[0] = pm_ult_x; e.p[1] = pm_ult_y; e.p[2] = c.pt[0][0]; e.p[3] = c.pt[0][1];
            nueva(&e);
            fin();
        }
    } else if (ev == V_ENT) fin();
}

/* Polilínea: vértices en c.tmp; c.f[0] = modo arco; c.tang = dirección de salida del último tramo. */
static void poli_tang(void)
{
    c.tang = 0;
    for (int i = 0; i + 1 < c.tmp.nv; i++) {
        double ch = gm_atan2(c.tmp.v[i + 1].y - c.tmp.v[i].y, c.tmp.v[i + 1].x - c.tmp.v[i].x);
        c.tang = c.tmp.v[i].b == 0 ? ch : ch + 2 * gm_atan(c.tmp.v[i].b);
    }
}
static void pide_poli(void)
{
    pide(ESP_PUNTO, c.f[0] ? "Punto final del arco" : "Punto siguiente",
         c.f[0] ? (c.tmp.nv >= 2 ? "Linea,Line;Cerrar,Close;Deshacer,Undo" : "Linea,Line;Deshacer,Undo") : (c.tmp.nv >= 2 ? "Arco,Arc;Cerrar,Close;Deshacer,Undo" : "Arco,Arc;Deshacer,Undo"));
}
static void poli_cerrar_tramo(double x, double y)      /* tramo desde el último vértice hasta (x, y), recto o en arco tangente */
{
    pm_vert_t *u = &c.tmp.v[c.tmp.nv - 1];
    if (c.f[0]) {
        double ch = gm_atan2(y - u->y, x - u->x), al = ch - c.tang;
        while (al > PM_PI) al -= 2 * PM_PI;
        while (al <= -PM_PI) al += 2 * PM_PI;
        u->b = gm_tan(al / 2);
        c.tang = ch + al;
    } else { u->b = 0; c.tang = gm_atan2(y - u->y, x - u->x); }
}
static void poli_terminar(bool cerrar)
{
    if (c.tmp.nv >= 2) {
        c.tmp.cerrada = cerrar ? 1 : 0;
        pm_ent_t e = c.tmp;
        c.tmp.v = NULL; c.tmp.nv = 0;
        e.tipo = E_POLI;
        nueva(&e);
    }
    fin();
}
static void h_plinea(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    if (c.paso == 0) {
        if (ev == V_INIT) { pm_ent_nueva(&c.tmp, E_POLI); pide(ESP_PUNTO, "Punto inicial", ""); return; }
        if (ev == V_PT) { pm_poli_vert(&c.tmp, x, y, 0); sig_pt(x, y); c.paso = 1; pide_poli(); }
        else if (ev == V_ENT) fin();
        return;
    }
    if (ev == V_PT) {
        poli_cerrar_tramo(x, y);
        pm_poli_vert(&c.tmp, x, y, 0);
        sig_pt(x, y);
        pide_poli();
    } else if (ev == V_OPC) {
        const char *o = opts_s;
        int idx = 0;
        char nom[24];
        int a = 0;
        for (; *o && idx < k; o++) if (*o == ';') idx++;
        while (*o && *o != ',' && a < 23) nom[a++] = *o++;
        nom[a] = 0;
        if (prefijo("Arco", nom)) { c.f[0] = 1; poli_tang(); pide_poli(); }
        else if (prefijo("Linea", nom)) { c.f[0] = 0; pide_poli(); }
        else if (prefijo("Cerrar", nom)) { poli_cerrar_tramo(c.tmp.v[0].x, c.tmp.v[0].y); poli_terminar(true); }
        else if (prefijo("Deshacer", nom)) {
            if (c.tmp.nv > 1) { c.tmp.nv--; c.tmp.v[c.tmp.nv - 1].b = 0; pm_tiene_ult = true; pm_ult_x = c.tmp.v[c.tmp.nv - 1].x; pm_ult_y = c.tmp.v[c.tmp.nv - 1].y; poli_tang(); }
            pide_poli();
        }
    } else if (ev == V_ENT) poli_terminar(false);
}

static void h_rectang(int ev, double x, double y, int k, const char *t)
{
    (void)k; (void)t;
    if (c.paso == 0) {
        if (ev == V_INIT) { pide(ESP_PUNTO, "Primera esquina", ""); return; }
        if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 1; pide(ESP_PUNTO, "Esquina opuesta", ""); }
        else if (ev == V_ENT) fin();
        return;
    }
    if (ev == V_PT) {
        if (gm_abs(x - c.pt[0][0]) < PM_EPS || gm_abs(y - c.pt[0][1]) < PM_EPS) { err("El rectángulo no puede tener lado cero"); return; }
        pm_ent_t e;
        pm_ent_nueva(&e, E_POLI);
        pm_rect_poli(&e, c.pt[0][0], c.pt[0][1], x, y);
        nueva(&e);
        sig_pt(x, y);
        msg("Rectángulo creado");
        fin();
    } else if (ev == V_ENT) fin();
}

static int pol_n = 4;
static void poligono_crear(double cx, double cy, double R, double a0, int n)
{
    pm_ent_t e;
    pm_ent_nueva(&e, E_POLI);
    for (int i = 0; i < n; i++) pm_poli_vert(&e, cx + R * gm_cos(a0 + 2 * PM_PI * i / n), cy + R * gm_sin(a0 + 2 * PM_PI * i / n), 0);
    e.cerrada = 1;
    nueva(&e);
}
static void h_poligono(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    char b[64];
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) { formatar_texto(b, sizeof b, "Número de lados <%d>", pol_n); pide(ESP_NUM, b, ""); return; }
        if (ev == V_ENT || ev == V_NUM) {
            if (ev == V_NUM) { if (x < 3 || x > 100) { err("El número de lados debe estar entre 3 y 100"); return; } pol_n = (int)(x + 0.5); }
            c.paso = 1; pide(ESP_PUNTO, "Centro del polígono", "Arista,Edge");
        }
        return;
    case 1:
        if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 3; pide(ESP_OPC, "Inscrito en o circunscrito a la circunferencia <Inscrito>", "Inscrito,Inscribed;Circunscrito,Circumscribed"); }
        else if (ev == V_OPC) { c.paso = 10; pide(ESP_PUNTO, "Primer punto de la arista", ""); }
        else if (ev == V_ENT) fin();
        return;
    case 3:
        if (ev == V_ENT || ev == V_OPC) {
            c.f[0] = ev == V_OPC ? k : 0;
            c.paso = 4;
            pide(ESP_NUM, "Radio de la circunferencia", "");
            num_pt = true;
        }
        return;
    case 4:
        if (ev == V_PT || ev == V_NUM) {
            double r, a0 = 0;
            if (ev == V_PT) { r = pm_dist(c.pt[0][0], c.pt[0][1], x, y); a0 = pm_ang(x - c.pt[0][0], y - c.pt[0][1]); } else r = x;
            if (r < PM_EPS) { err("El radio debe ser positivo"); return; }
            if (c.f[0]) { r = r / gm_cos(PM_PI / pol_n); a0 += PM_PI / pol_n; }
            poligono_crear(c.pt[0][0], c.pt[0][1], r, a0, pol_n);
            msg("Polígono creado");
            fin();
        }
        return;
    case 10:
        if (ev == V_PT) { c.pt[1][0] = x; c.pt[1][1] = y; sig_pt(x, y); c.paso = 11; pide(ESP_PUNTO, "Segundo punto de la arista", ""); }
        return;
    case 11:
        if (ev == V_PT) {
            double l = pm_dist(c.pt[1][0], c.pt[1][1], x, y);
            if (l < PM_EPS) { err("La arista no puede ser nula"); return; }
            double R = l / (2 * gm_sin(PM_PI / pol_n)), ap_ = l / (2 * gm_tan(PM_PI / pol_n));
            double mx = (c.pt[1][0] + x) / 2, my = (c.pt[1][1] + y) / 2, nx = -(y - c.pt[1][1]) / l, ny = (x - c.pt[1][0]) / l;
            double cx = mx + nx * ap_, cy = my + ny * ap_;
            poligono_crear(cx, cy, R, pm_ang(c.pt[1][0] - cx, c.pt[1][1] - cy), pol_n);
            msg("Polígono creado");
            fin();
        }
        return;
    }
}

static bool circulo3(double ax, double ay, double bx, double by, double cx, double cy, double *ox, double *oy, double *r)
{
    double d = 2 * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
    if (gm_abs(d) < 1e-12) return false;
    double a2 = ax * ax + ay * ay, b2 = bx * bx + by * by, c2 = cx * cx + cy * cy;
    *ox = (a2 * (by - cy) + b2 * (cy - ay) + c2 * (ay - by)) / d;
    *oy = (a2 * (cx - bx) + b2 * (ax - cx) + c2 * (bx - ax)) / d;
    *r = pm_dist(*ox, *oy, ax, ay);
    return true;
}
static void circulo_crear(double cx, double cy, double r)
{
    if (r < PM_EPS) { err("El radio debe ser positivo"); return; }
    pm_ent_t e;
    pm_ent_nueva(&e, E_CIRCULO);
    e.p[0] = cx; e.p[1] = cy; e.r = r;
    nueva(&e);
    msg2("Círculo de radio ", F(r), "");
    fin();
}
static void h_circulo(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) { pide(ESP_PUNTO, "Centro del círculo", "3P,3P;2P,2P"); return; }
        if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 1; pide(ESP_NUM, "Radio", "Diametro,Diameter"); num_pt = true; }
        else if (ev == V_OPC) { c.paso = k == 0 ? 10 : 20; pide(ESP_PUNTO, "Primer punto", ""); }
        else if (ev == V_ENT) fin();
        return;
    case 1:
        if (ev == V_NUM) circulo_crear(c.pt[0][0], c.pt[0][1], x);
        else if (ev == V_PT) circulo_crear(c.pt[0][0], c.pt[0][1], pm_dist(c.pt[0][0], c.pt[0][1], x, y));
        else if (ev == V_OPC) { c.paso = 5; pide(ESP_NUM, "Diámetro", ""); num_pt = true; }
        return;
    case 5:
        if (ev == V_NUM) circulo_crear(c.pt[0][0], c.pt[0][1], x / 2);
        else if (ev == V_PT) circulo_crear(c.pt[0][0], c.pt[0][1], pm_dist(c.pt[0][0], c.pt[0][1], x, y) / 2);
        return;
    case 10: if (ev == V_PT) { c.pt[1][0] = x; c.pt[1][1] = y; c.paso = 11; pide(ESP_PUNTO, "Segundo punto", ""); } return;
    case 11: if (ev == V_PT) { c.pt[2][0] = x; c.pt[2][1] = y; c.paso = 12; pide(ESP_PUNTO, "Tercer punto", ""); } return;
    case 12:
        if (ev == V_PT) {
            double ox, oy, r;
            if (!circulo3(c.pt[1][0], c.pt[1][1], c.pt[2][0], c.pt[2][1], x, y, &ox, &oy, &r)) { err("Los tres puntos están alineados"); return; }
            sig_pt(ox, oy);
            circulo_crear(ox, oy, r);
        }
        return;
    case 20: if (ev == V_PT) { c.pt[1][0] = x; c.pt[1][1] = y; c.paso = 21; pide(ESP_PUNTO, "Segundo punto del diámetro", ""); } return;
    case 21:
        if (ev == V_PT) {
            double r = pm_dist(c.pt[1][0], c.pt[1][1], x, y) / 2;
            circulo_crear((c.pt[1][0] + x) / 2, (c.pt[1][1] + y) / 2, r);
        }
        return;
    }
}

static void h_arco(int ev, double x, double y, int k, const char *t)
{
    (void)k; (void)t;
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) { pide(ESP_PUNTO, "Punto inicial del arco", "Centro,Center"); return; }
        if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 1; pide(ESP_PUNTO, "Segundo punto del arco", ""); }
        else if (ev == V_OPC) { c.paso = 10; pide(ESP_PUNTO, "Centro del arco", ""); }
        else if (ev == V_ENT) fin();
        return;
    case 1: if (ev == V_PT) { c.pt[1][0] = x; c.pt[1][1] = y; sig_pt(x, y); c.paso = 2; pide(ESP_PUNTO, "Punto final del arco", ""); } return;
    case 2:
        if (ev == V_PT) {
            double ox, oy, r;
            if (!circulo3(c.pt[0][0], c.pt[0][1], c.pt[1][0], c.pt[1][1], x, y, &ox, &oy, &r)) { err("Los tres puntos están alineados"); return; }
            double cr = (c.pt[1][0] - c.pt[0][0]) * (y - c.pt[1][1]) - (c.pt[1][1] - c.pt[0][1]) * (x - c.pt[1][0]);
            pm_ent_t e;
            pm_ent_nueva(&e, E_ARCO);
            e.p[0] = ox; e.p[1] = oy; e.r = r;
            double a1 = pm_ang(c.pt[0][0] - ox, c.pt[0][1] - oy), a3 = pm_ang(x - ox, y - oy);
            if (cr > 0) { e.a0 = a1; e.a1 = a3; } else { e.a0 = a3; e.a1 = a1; }
            nueva(&e); sig_pt(x, y);
            msg("Arco creado");
            fin();
        }
        return;
    case 10: if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 11; pide(ESP_PUNTO, "Punto inicial del arco", ""); } return;
    case 11:
        if (ev == V_PT) {
            if (pm_dist(c.pt[0][0], c.pt[0][1], x, y) < PM_EPS) { err("El radio debe ser positivo"); return; }
            c.pt[1][0] = x; c.pt[1][1] = y; c.paso = 12; pide(ESP_PUNTO, "Punto final (define el ángulo)", "");
        }
        return;
    case 12:
        if (ev == V_PT) {
            pm_ent_t e;
            pm_ent_nueva(&e, E_ARCO);
            e.p[0] = c.pt[0][0]; e.p[1] = c.pt[0][1]; e.r = pm_dist(c.pt[0][0], c.pt[0][1], c.pt[1][0], c.pt[1][1]);
            e.a0 = pm_ang(c.pt[1][0] - c.pt[0][0], c.pt[1][1] - c.pt[0][1]); e.a1 = pm_ang(x - c.pt[0][0], y - c.pt[0][1]);
            if (pm_angnorm(e.a1 - e.a0) < 1e-9) { err("El ángulo final coincide con el inicial"); return; }
            nueva(&e); sig_pt(x, y);
            msg("Arco creado");
            fin();
        }
        return;
    }
}

static double txt_alt_def = 2.5;
static void h_texto(int ev, double x, double y, int k, const char *t)
{
    (void)k;
    char b[80];
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) { pide(ESP_PUNTO, "Punto de inserción del texto", ""); return; }
        if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 1; formatar_texto(b, sizeof b, "Altura del texto <%s>", F(txt_alt_def)); pide(ESP_NUM, b, ""); num_pt = true; }
        else if (ev == V_ENT) fin();
        return;
    case 1:
        if (ev == V_NUM || ev == V_ENT || ev == V_PT) {
            if (ev == V_NUM) { if (x <= 0) { err("La altura debe ser positiva"); return; } txt_alt_def = x; }
            if (ev == V_PT) { double h = pm_dist(c.pt[0][0], c.pt[0][1], x, y); if (h < PM_EPS) return; txt_alt_def = h; }
            c.n[0] = txt_alt_def;
            c.paso = 2;
            pide(ESP_NUM, "Ángulo de giro <0>", "");
            num_ang = true;
        }
        return;
    case 2:
        if (ev == V_NUM || ev == V_ENT) { c.n[1] = ev == V_NUM ? x : 0; c.paso = 3; pide(ESP_TEXTO, "Texto", ""); }
        return;
    case 3:
        if (ev == V_TXT && t && t[0]) {
            pm_ent_t e;
            pm_ent_nueva(&e, E_TEXTO);
            e.p[0] = c.pt[0][0]; e.p[1] = c.pt[0][1]; e.alt = c.n[0]; e.giro = c.n[1] * PM_PI / 180.0;
            copia_texto(e.txt, t, sizeof e.txt);
            nueva(&e);
            msg("Texto creado");
            fin();
        } else if (ev == V_ENT) fin();
        return;
    }
}

/* ---- cotas ---- */
static int cota_nueva(pm_ent_t *e) { e->alt = pm_cota_alt; return nueva(e); }
static void cota_lineal(int ev, double x, double y, int forzar)   /* forzar: -1 auto, C_LIN_H, C_LIN_V, C_ALINEADA */
{
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) { pide(ESP_PUNTO, "Origen de la primera línea de referencia", ""); return; }
        if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 1; pide(ESP_PUNTO, "Origen de la segunda línea de referencia", ""); }
        else if (ev == V_ENT) fin();
        return;
    case 1:
        if (ev == V_PT) {
            if (pm_dist(c.pt[0][0], c.pt[0][1], x, y) < PM_EPS) { err("Los dos puntos coinciden"); return; }
            c.pt[1][0] = x; c.pt[1][1] = y; sig_pt(x, y); c.paso = 2; pide(ESP_PUNTO, "Ubicación de la línea de cota", "");
        }
        return;
    case 2:
        if (ev == V_PT) {
            int sub = forzar;
            if (sub < 0) {
                double y0 = c.pt[0][1] < c.pt[1][1] ? c.pt[0][1] : c.pt[1][1], y1 = c.pt[0][1] < c.pt[1][1] ? c.pt[1][1] : c.pt[0][1];
                double x0 = c.pt[0][0] < c.pt[1][0] ? c.pt[0][0] : c.pt[1][0], x1 = c.pt[0][0] < c.pt[1][0] ? c.pt[1][0] : c.pt[0][0];
                if (y > y1 || y < y0) sub = C_LIN_H;
                else if (x > x1 || x < x0) sub = C_LIN_V;
                else sub = gm_abs(c.pt[1][0] - c.pt[0][0]) >= gm_abs(c.pt[1][1] - c.pt[0][1]) ? C_LIN_H : C_LIN_V;
            }
            pm_ent_t e;
            pm_ent_nueva(&e, E_COTA);
            e.sub = (uint8_t)sub;
            e.p[0] = c.pt[0][0]; e.p[1] = c.pt[0][1]; e.p[2] = c.pt[1][0]; e.p[3] = c.pt[1][1]; e.p[4] = x; e.p[5] = y;
            cota_nueva(&e);
            char s[80];
            pm_cota_texto(&pm_ent[pm_n - 1], s, sizeof s);
            msg2("Cota ", s, "");
            fin();
        }
        return;
    }
}
static void h_cotalin(int ev, double x, double y, int k, const char *t) { (void)k; (void)t; cota_lineal(ev, x, y, -1); }
static void h_cotahor(int ev, double x, double y, int k, const char *t) { (void)k; (void)t; cota_lineal(ev, x, y, C_LIN_H); }
static void h_cotaver(int ev, double x, double y, int k, const char *t) { (void)k; (void)t; cota_lineal(ev, x, y, C_LIN_V); }
static void h_cotaali(int ev, double x, double y, int k, const char *t) { (void)k; (void)t; cota_lineal(ev, x, y, C_ALINEADA); }

static void cota_radial(int ev, double x, double y, int k, int sub)
{
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) { pide(ESP_OBJ, "Designar un arco o un círculo", ""); return; }
        if (ev == V_OBJ) {
            if (pm_ent[k].tipo != E_CIRCULO && pm_ent[k].tipo != E_ARCO) { err("Hay que elegir un círculo o un arco"); return; }
            c.i[0] = k; c.pt[0][0] = pm_ent[k].p[0]; c.pt[0][1] = pm_ent[k].p[1]; c.n[0] = pm_ent[k].r;
            c.paso = 1; pide(ESP_PUNTO, "Ubicación de la línea de cota", "");
        } else if (ev == V_ENT) fin();
        return;
    case 1:
        if (ev == V_PT) {
            if (pm_dist(c.pt[0][0], c.pt[0][1], x, y) < PM_EPS) { err("El punto no puede ser el centro"); return; }
            pm_ent_t e;
            pm_ent_nueva(&e, E_COTA);
            e.sub = (uint8_t)sub; e.p[0] = c.pt[0][0]; e.p[1] = c.pt[0][1]; e.r = c.n[0]; e.a0 = pm_ang(x - c.pt[0][0], y - c.pt[0][1]);
            cota_nueva(&e);
            char s[80];
            pm_cota_texto(&pm_ent[pm_n - 1], s, sizeof s);
            msg2("Cota ", s, "");
            fin();
        }
        return;
    }
}
static void h_cotarad(int ev, double x, double y, int k, const char *t) { (void)t; cota_radial(ev, x, y, k, C_RADIO); }
static void h_cotadiam(int ev, double x, double y, int k, const char *t) { (void)t; cota_radial(ev, x, y, k, C_DIAM); }
static void h_cotaang(int ev, double x, double y, int k, const char *t)
{
    (void)k; (void)t;
    static const char *const pr[4] = { "Vértice del ángulo", "Primer punto del ángulo", "Segundo punto del ángulo", "Ubicación del arco de cota" };
    if (ev == V_INIT) { pide(ESP_PUNTO, pr[0], ""); return; }
    if (ev == V_ENT) { fin(); return; }
    if (ev != V_PT) return;
    c.pt[c.paso][0] = x; c.pt[c.paso][1] = y;
    if (c.paso < 3) { c.paso++; sig_pt(x, y); pide(ESP_PUNTO, pr[c.paso], ""); return; }
    pm_ent_t e;
    pm_ent_nueva(&e, E_COTA);
    e.sub = C_ANGULAR;
    for (int i = 0; i < 4; i++) { e.p[2 * i] = c.pt[i][0]; e.p[2 * i + 1] = c.pt[i][1]; }
    if (pm_dist(c.pt[0][0], c.pt[0][1], x, y) < PM_EPS) { err("El arco no puede estar en el vértice"); return; }
    cota_nueva(&e);
    char s[80];
    pm_cota_texto(&pm_ent[pm_n - 1], s, sizeof s);
    msg2("Cota ", s, "");
    fin();
}
static void h_cotaestilo(int ev, double x, double y, int k, const char *t)
{
    (void)y; (void)k; (void)t;
    char b[80];
    if (ev == V_INIT) { formatar_texto(b, sizeof b, "Altura del texto <%s>", F(pm_cota_alt)); pide(ESP_NUM, b, ""); return; }
    if (ev != V_NUM && ev != V_ENT) return;
    if (c.paso == 0) {
        if (ev == V_NUM) { if (x <= 0) { err("Debe ser positivo"); return; } pm_cota_alt = x; }
        c.paso = 1; formatar_texto(b, sizeof b, "Tamaño de las flechas <%s>", F(pm_cota_flecha)); pide(ESP_NUM, b, ""); return;
    }
    if (c.paso == 1) {
        if (ev == V_NUM) { if (x <= 0) { err("Debe ser positivo"); return; } pm_cota_flecha = x; }
        c.paso = 2; formatar_texto(b, sizeof b, "Decimales <%d>", pm_cota_dec); pide(ESP_NUM, b, ""); return;
    }
    if (ev == V_NUM) { if (x < 0 || x > 8) { err("Entre 0 y 8"); return; } pm_cota_dec = (int)(x + 0.5); }
    msg("Estilo de cotas actualizado");
    fin();
}

/* ---- sombreado ---- */
static int hat_sub = 0;
static double hat_ang = 45.0, hat_esp = 0;
static double bonito(double v)
{
    double m = 1;
    while (m * 10 <= v) m *= 10;
    while (m > v) m /= 10;
    return v >= m * 5 ? m * 5 : v >= m * 2 ? m * 2 : m;
}
static void h_sombreado(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) { pide(ESP_PUNTO, "Punto dentro de un contorno cerrado", "Solido,Solid;Lineas,Lines;Angulo,Angle;Espaciado,Spacing"); return; }
        if (ev == V_PT) {
            int i = pm_contorno_en(x, y);
            if (i < 0) { i = pm_entidad_en(x, y, ap(), -1); }
            if (i < 0) { err("No hay un contorno cerrado en ese punto"); return; }
            double esp = hat_esp > 0 ? hat_esp : bonito(12.0 / pm_ve);
            if (pm_sombra_de(i, hat_sub, hat_ang * PM_PI / 180.0, esp) < 0) err(pm_erro);
            else msg("Sombreado creado");
        } else if (ev == V_OPC) {
            if (k == 0) { hat_sub = 1; msg("Sombreado sólido"); }
            else if (k == 1) { hat_sub = 0; msg("Sombreado de líneas"); }
            else if (k == 2) { c.paso = 1; pide(ESP_NUM, "Ángulo de las líneas", ""); num_ang = true; return; }
            else { c.paso = 2; pide(ESP_NUM, "Espaciado entre líneas (0 = automático)", ""); return; }
        } else if (ev == V_ENT) { fin(); return; }
        pide(ESP_PUNTO, "Punto dentro de un contorno cerrado", "Solido,Solid;Lineas,Lines;Angulo,Angle;Espaciado,Spacing");
        return;
    case 1: if (ev == V_NUM) { hat_ang = x; c.paso = 0; pide(ESP_PUNTO, "Punto dentro de un contorno cerrado", "Solido,Solid;Lineas,Lines;Angulo,Angle;Espaciado,Spacing"); } return;
    case 2: if (ev == V_NUM) { hat_esp = x > 0 ? x : 0; c.paso = 0; pide(ESP_PUNTO, "Punto dentro de un contorno cerrado", "Solido,Solid;Lineas,Lines;Angulo,Angle;Espaciado,Spacing"); } return;
    }
    (void)y;
}

/* ============================================================ órdenes de modificación */

static void h_borrar(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) { pide(ESP_SEL, "Designar objetos", ""); return; }
    if (ev != V_SEL) return;
    int n = 0;
    for (int i = pm_n - 1; i >= 0; i--) if (pm_ent[i].sel) { pm_borrar(i); n++; }
    char b[48];
    formatar_texto(b, sizeof b, "%d objetos borrados", n);
    msg(b);
    fin();
}

static void h_mover(int ev, double x, double y, int k, const char *t)
{
    (void)k; (void)t;
    if (ev == V_INIT) { pide(ESP_SEL, "Designar objetos", ""); return; }
    switch (c.paso) {
    case 0:
        if (ev == V_SEL) { c.paso = 1; pide(ESP_PUNTO, "Punto base o desplazamiento", ""); }
        return;
    case 1:
        if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 2; pide(ESP_PUNTO, "Segundo punto del desplazamiento (Intro = el primero es el desplazamiento)", ""); }
        return;
    case 2:
        if (ev == V_PT || ev == V_ENT) {
            double dx = ev == V_PT ? x - c.pt[0][0] : c.pt[0][0], dy = ev == V_PT ? y - c.pt[0][1] : c.pt[0][1], m[6];
            pm_mat_traslacion(m, dx, dy);
            aplicar_sel(m, false);
            msg2("Desplazamiento ", F(dx), "");
            fin();
        }
        return;
    }
}
static void h_copiar(int ev, double x, double y, int k, const char *t)
{
    (void)k; (void)t;
    if (ev == V_INIT) { pide(ESP_SEL, "Designar objetos", ""); return; }
    switch (c.paso) {
    case 0:
        if (ev == V_SEL) { c.paso = 1; pide(ESP_PUNTO, "Punto base o desplazamiento", ""); }
        return;
    case 1:
        if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 2; pide(ESP_PUNTO, "Segundo punto (Intro para terminar)", ""); }
        return;
    case 2:
        if (ev == V_PT) {
            double m[6];
            pm_mat_traslacion(m, x - c.pt[0][0], y - c.pt[0][1]);
            aplicar_sel(m, true);
            c.i[0]++;
        } else if (ev == V_ENT) { char b[48]; formatar_texto(b, sizeof b, "%d copias", c.i[0]); msg(b); fin(); }
        return;
    }
}
static void h_girar(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    if (ev == V_INIT) { pide(ESP_SEL, "Designar objetos", ""); return; }
    switch (c.paso) {
    case 0: if (ev == V_SEL) { c.paso = 1; pide(ESP_PUNTO, "Punto base", ""); } return;
    case 1: if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 2; pide(ESP_NUM, "Ángulo de rotación", "Copia,Copy"); num_ang = true; } return;
    case 2:
        if (ev == V_OPC) { c.f[0] = 1; pide(ESP_NUM, "Ángulo de rotación (se crea una copia)", "Copia,Copy"); num_ang = true; }
        else if (ev == V_NUM) {
            double m[6];
            pm_mat_giro(m, c.pt[0][0], c.pt[0][1], x * PM_PI / 180.0);
            aplicar_sel(m, c.f[0] != 0);
            msg2("Girado ", F(x), " grados");
            fin();
        }
        (void)k;
        return;
    }
}
static void h_escala(int ev, double x, double y, int k, const char *t)
{
    (void)k; (void)t;
    if (ev == V_INIT) { pide(ESP_SEL, "Designar objetos", ""); return; }
    switch (c.paso) {
    case 0: if (ev == V_SEL) { c.paso = 1; pide(ESP_PUNTO, "Punto base", ""); } return;
    case 1: if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 2; pide(ESP_NUM, "Factor de escala", ""); } return;
    case 2:
        if (ev == V_NUM) {
            if (x <= PM_EPS) { err("El factor debe ser positivo"); return; }
            double m[6];
            pm_mat_escala(m, c.pt[0][0], c.pt[0][1], x);
            aplicar_sel(m, false);
            msg2("Escala ", F(x), "");
            fin();
        }
        return;
    }
    (void)y;
}
static void h_simetria(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    if (ev == V_INIT) { pide(ESP_SEL, "Designar objetos", ""); return; }
    switch (c.paso) {
    case 0: if (ev == V_SEL) { c.paso = 1; pide(ESP_PUNTO, "Primer punto del eje de simetría", ""); } return;
    case 1: if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 2; pide(ESP_PUNTO, "Segundo punto del eje de simetría", ""); } return;
    case 2:
        if (ev == V_PT) {
            if (pm_dist(c.pt[0][0], c.pt[0][1], x, y) < PM_EPS) { err("Los puntos del eje coinciden"); return; }
            c.pt[1][0] = x; c.pt[1][1] = y; c.paso = 3;
            pide(ESP_OPC, "¿Borrar los objetos de origen? <No>", "Si,Yes;No,No");
        }
        return;
    case 3:
        if (ev == V_OPC || ev == V_ENT) {
            double m[6];
            pm_mat_simetria(m, c.pt[0][0], c.pt[0][1], c.pt[1][0], c.pt[1][1]);
            bool borrar = ev == V_OPC && k == 0;
            aplicar_sel(m, !borrar);
            msg("Simetría aplicada");
            fin();
        }
        return;
    }
    (void)x;
}

static double off_d = 1.0;
static void h_equidist(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    char b[80];
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) { formatar_texto(b, sizeof b, "Distancia de equidistancia <%s>", F(off_d)); pide(ESP_NUM, b, ""); return; }
        if (ev == V_NUM) { if (x <= PM_EPS) { err("La distancia debe ser positiva"); return; } off_d = x; }
        if (ev == V_NUM || ev == V_ENT) { c.paso = 1; pm_sel_todo(false); pide(ESP_OBJ, "Designar objeto a desfasar", ""); }
        return;
    case 1:
        if (ev == V_OBJ) { c.i[0] = k; c.paso = 2; pm_ent[k].sel = 1; pide(ESP_PUNTO, "Punto del lado de la equidistancia", ""); }
        else if (ev == V_ENT) fin();
        return;
    case 2:
        if (ev == V_PT) {
            int r = pm_equidistancia(c.i[0], off_d, x, y);
            if (r < 0) err(pm_erro); else msg("Equidistancia creada");
            pm_sel_todo(false);
            c.paso = 1; pide(ESP_OBJ, "Designar objeto a desfasar", "");
        } else if (ev == V_ENT) fin();
        return;
    }
}

static int es_borde(int idx) { return idx >= 0; }
static void h_recorta_o_alarga(int ev, double x, double y, int k, int recorta)
{
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) { pide(ESP_SEL, "Designar bordes (Intro = todos los objetos)", ""); return; }
        if (ev == V_SEL) {
            c.f[0] = pm_sel_contar() > 0 ? 1 : 0;
            c.paso = 1;
            pide(ESP_OBJ, recorta ? "Designar objeto a recortar" : "Designar objeto a alargar", "Deshacer,Undo");
        }
        return;
    case 1:
        if (ev == V_OBJ) {
            if (!es_borde(k)) return;
            int r = recorta ? pm_recortar(k, x, y, NULL, c.f[0] ? -2 : -1) : pm_extender(k, x, y, NULL, c.f[0] ? -2 : -1);
            if (r < 0) err(pm_erro); else msg(recorta ? "Recortado" : "Alargado");
            pm_historial_guardar();
        } else if (ev == V_OPC) { if (pm_deshacer()) msg("Deshecho"); }
        else if (ev == V_ENT) fin();
        return;
    }
}
static void h_recorta(int ev, double x, double y, int k, const char *t) { (void)t; h_recorta_o_alarga(ev, x, y, k, 1); }
static void h_alarga(int ev, double x, double y, int k, const char *t) { (void)t; h_recorta_o_alarga(ev, x, y, k, 0); }

static double fil_r = 0, cha_d1 = 1, cha_d2 = 1;
static void h_empalme(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    char b[96];
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) {
            formatar_texto(b, sizeof b, "Primer objeto (radio actual %s)", F(fil_r));
            pide(ESP_OBJ, b, "Radio,Radius;Polilinea,Polyline");
            return;
        }
        if (ev == V_OPC) {
            if (k == 0) { c.paso = 5; formatar_texto(b, sizeof b, "Radio de empalme <%s>", F(fil_r)); pide(ESP_NUM, b, ""); }
            else { c.paso = 6; pide(ESP_OBJ, "Designar polilínea", ""); }
        } else if (ev == V_OBJ) { c.i[0] = k; c.pt[0][0] = x; c.pt[0][1] = y; c.paso = 1; pide(ESP_OBJ, "Segundo objeto", ""); }
        else if (ev == V_ENT) fin();
        return;
    case 1:
        if (ev == V_OBJ) {
            if (pm_empalme(c.i[0], c.pt[0][0], c.pt[0][1], k, x, y, fil_r) < 0) err(pm_erro);
            else msg("Empalme creado");
            fin();
        } else if (ev == V_ENT) fin();
        return;
    case 5:
        if (ev == V_NUM) { if (x < 0) { err("El radio no puede ser negativo"); return; } fil_r = x; c.paso = 0; formatar_texto(b, sizeof b, "Primer objeto (radio actual %s)", F(fil_r)); pide(ESP_OBJ, b, "Radio,Radius;Polilinea,Polyline"); }
        else if (ev == V_ENT) { c.paso = 0; formatar_texto(b, sizeof b, "Primer objeto (radio actual %s)", F(fil_r)); pide(ESP_OBJ, b, "Radio,Radius;Polilinea,Polyline"); }
        return;
    case 6:
        if (ev == V_OBJ) {
            int r = pm_empalme_poli(k, fil_r);
            if (r < 0) err(pm_erro); else { char s[64]; formatar_texto(s, sizeof s, "%d esquinas empalmadas", r); msg(s); }
            fin();
        } else if (ev == V_ENT) fin();
        return;
    }
}
static void h_chaflan(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    char b[96];
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) { formatar_texto(b, sizeof b, "Primera línea (distancias %s, %s)", F(cha_d1), F(cha_d2)); pide(ESP_OBJ, b, "Distancia,Distance"); return; }
        if (ev == V_OPC) { c.paso = 5; formatar_texto(b, sizeof b, "Primera distancia <%s>", F(cha_d1)); pide(ESP_NUM, b, ""); }
        else if (ev == V_OBJ) { c.i[0] = k; c.pt[0][0] = x; c.pt[0][1] = y; c.paso = 1; pide(ESP_OBJ, "Segunda línea", ""); }
        else if (ev == V_ENT) fin();
        return;
    case 1:
        if (ev == V_OBJ) {
            if (pm_chaflan(c.i[0], c.pt[0][0], c.pt[0][1], k, x, y, cha_d1, cha_d2) < 0) err(pm_erro); else msg("Chaflán creado");
            fin();
        } else if (ev == V_ENT) fin();
        return;
    case 5:
        if (ev == V_NUM || ev == V_ENT) {
            if (ev == V_NUM) { if (x < 0) { err("No puede ser negativa"); return; } cha_d1 = x; }
            c.paso = 6; formatar_texto(b, sizeof b, "Segunda distancia <%s>", F(cha_d2)); pide(ESP_NUM, b, "");
        }
        return;
    case 6:
        if (ev == V_NUM || ev == V_ENT) {
            if (ev == V_NUM) { if (x < 0) { err("No puede ser negativa"); return; } cha_d2 = x; }
            c.paso = 0; formatar_texto(b, sizeof b, "Primera línea (distancias %s, %s)", F(cha_d1), F(cha_d2)); pide(ESP_OBJ, b, "Distancia,Distance");
        }
        return;
    }
    (void)y;
}

static void h_explota(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) { pide(ESP_SEL, "Designar objetos", ""); return; }
    if (ev != V_SEL) return;
    int n = 0, fallos = 0;
    for (int i = pm_n - 1; i >= 0; i--) if (pm_ent[i].sel) { if (pm_explotar(i) >= 0) n++; else fallos++; }
    char b[80];
    formatar_texto(b, sizeof b, "%d objetos explotados%s", n, fallos ? " (algunos no se pueden explotar)" : "");
    msg(b);
    fin();
}

static void h_matriz(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    if (ev == V_INIT) { pide(ESP_SEL, "Designar objetos", ""); return; }
    switch (c.paso) {
    case 0: if (ev == V_SEL) { c.paso = 1; pide(ESP_OPC, "Tipo de matriz <Rectangular>", "Rectangular,Rectangular;Polar,Polar"); } return;
    case 1:
        if (ev == V_ENT || ev == V_OPC) {
            if (ev == V_OPC && k == 1) { c.paso = 20; pide(ESP_PUNTO, "Centro de la matriz polar", ""); }
            else { c.paso = 10; pide(ESP_NUM, "Número de filas", ""); }
        }
        return;
    case 10: if (ev == V_NUM && x >= 1 && x <= 200) { c.i[0] = (int)(x + 0.5); c.paso = 11; pide(ESP_NUM, "Número de columnas", ""); } return;
    case 11: if (ev == V_NUM && x >= 1 && x <= 200) { c.i[1] = (int)(x + 0.5); c.paso = 12; pide(ESP_NUM, "Distancia entre filas", ""); } return;
    case 12: if (ev == V_NUM) { c.n[0] = x; c.paso = 13; pide(ESP_NUM, "Distancia entre columnas", ""); } return;
    case 13:
        if (ev == V_NUM) {
            for (int r = 0; r < c.i[0]; r++)
                for (int q = 0; q < c.i[1]; q++) {
                    if (!r && !q) continue;
                    double m[6];
                    pm_mat_traslacion(m, q * x, r * c.n[0]);
                    aplicar_sel(m, true);
                }
            char b[64];
            formatar_texto(b, sizeof b, "Matriz de %d x %d", c.i[0], c.i[1]);
            msg(b);
            fin();
        }
        return;
    case 20: if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 21; pide(ESP_NUM, "Número de elementos (con el original)", ""); } return;
    case 21: if (ev == V_NUM && x >= 2 && x <= 500) { c.i[0] = (int)(x + 0.5); c.paso = 22; pide(ESP_NUM, "Ángulo a rellenar <360>", ""); num_ang = true; } return;
    case 22:
        if (ev == V_NUM || ev == V_ENT) {
            double total = ev == V_NUM ? x : 360.0;
            double paso = gm_abs(total - 360.0) < 1e-9 ? total / c.i[0] : total / (c.i[0] - 1);
            for (int q = 1; q < c.i[0]; q++) {
                double m[6];
                pm_mat_giro(m, c.pt[0][0], c.pt[0][1], q * paso * PM_PI / 180.0);
                aplicar_sel(m, true);
            }
            msg("Matriz polar creada");
            fin();
        }
        return;
    }
}

static void h_igualaprop(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)t;
    switch (c.paso) {
    case 0:
        if (ev == V_INIT) { pide(ESP_OBJ, "Designar el objeto de origen", ""); return; }
        if (ev == V_OBJ) { c.i[0] = k; c.paso = 1; pide(ESP_OBJ, "Designar objetos de destino (Intro termina)", ""); }
        else if (ev == V_ENT) fin();
        return;
    case 1:
        if (ev == V_OBJ) {
            if (k != c.i[0]) { pm_ent[k].capa = pm_ent[c.i[0]].capa; pm_ent[k].color = pm_ent[c.i[0]].color; pm_ent[k].ltipo = pm_ent[c.i[0]].ltipo; c.i[1]++; }
        } else if (ev == V_ENT) { char b[48]; formatar_texto(b, sizeof b, "%d objetos igualados", c.i[1]); msg(b); fin(); }
        return;
    }
}

/* ============================================================ vistas, ayudas, consulta */

static void h_zoom(int ev, double x, double y, int k, const char *t)
{
    (void)t;
    const char *op = "Extension,Extents;Todo,All;Ventana,Window;Previo,Previous";
    if (ev == V_INIT) { pide(ESP_NUM, "Factor, o esquina de ventana", op); num_pt = true; return; }
    if (c.paso == 0) {
        if (ev == V_NUM) { pm_vista_guardar(); pm_zoom_factor(x, pm_vw / 2.0, pm_vh / 2.0); msg("Zoom"); fin(); }
        else if (ev == V_OPC) {
            if (k == 0 || k == 1) { pm_zoom_ext(); msg("Zoom extensión"); fin(); }
            else if (k == 2) { c.paso = 1; pide(ESP_PUNTO, "Primera esquina de la ventana", ""); }
            else { pm_zoom_prev(); msg("Zoom previo"); fin(); }
        } else if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 2; pide(ESP_PUNTO, "Esquina opuesta", ""); }
        else if (ev == V_ENT) fin();
        return;
    }
    if (c.paso == 1 && ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 2; pide(ESP_PUNTO, "Esquina opuesta", ""); return; }
    if (c.paso == 2 && ev == V_PT) {
        if (pm_dist(c.pt[0][0], c.pt[0][1], x, y) < PM_EPS) { err("Ventana vacía"); return; }
        pm_zoom_vent(c.pt[0][0], c.pt[0][1], x, y);
        msg("Zoom ventana");
        fin();
    }
    (void)y;
}
static void h_pan(int ev, double x, double y, int k, const char *t)
{
    (void)k; (void)t;
    if (ev == V_INIT) { pide(ESP_PUNTO, "Punto base del desplazamiento", ""); return; }
    if (c.paso == 0) { if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 1; pide(ESP_PUNTO, "Segundo punto", ""); } else if (ev == V_ENT) fin(); return; }
    if (ev == V_PT) { pm_vcx -= x - c.pt[0][0]; pm_vcy -= y - c.pt[0][1]; msg("Vista desplazada"); fin(); }
}
static void h_dist(int ev, double x, double y, int k, const char *t)
{
    (void)k; (void)t;
    if (ev == V_INIT) { pide(ESP_PUNTO, "Primer punto", ""); return; }
    if (c.paso == 0) { if (ev == V_PT) { c.pt[0][0] = x; c.pt[0][1] = y; sig_pt(x, y); c.paso = 1; pide(ESP_PUNTO, "Segundo punto", ""); } else if (ev == V_ENT) fin(); return; }
    if (ev == V_PT) {
        double dx = x - c.pt[0][0], dy = y - c.pt[0][1];
        char b[160], d[32], a[32], ex[32], ey[32];
        pm_fmt(gm_hypot(dx, dy), 4, d, sizeof d); pm_fmt(pm_ang(dx, dy) * 180 / PM_PI, 2, a, sizeof a); pm_fmt(dx, 4, ex, sizeof ex); pm_fmt(dy, 4, ey, sizeof ey);
        formatar_texto(b, sizeof b, "Distancia = %s, ángulo = %s, delta X = %s, delta Y = %s", d, a, ex, ey);
        msg(b);
        sig_pt(x, y);
        fin();
    }
}

static const char *const NOMBRE_TIPO[] = { "", "PUNTO", "LINEA", "CIRCULO", "ARCO", "POLILINEA", "TEXTO", "COTA", "SOMBREADO" };
static void h_lista(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) { pide(ESP_SEL, "Designar objetos", ""); return; }
    if (ev != V_SEL) return;
    for (int i = 0; i < pm_n; i++) {
        const pm_ent_t *e = &pm_ent[i];
        if (!e->sel) continue;
        char b[160], f[5][32];
        pm_fmt(e->p[0], 4, f[0], 32); pm_fmt(e->p[1], 4, f[1], 32); pm_fmt(e->p[2], 4, f[2], 32); pm_fmt(e->p[3], 4, f[3], 32); pm_fmt(e->r, 4, f[4], 32);
        if (e->tipo == E_LINEA) formatar_texto(b, sizeof b, "#%d LINEA capa %s: (%s,%s)-(%s,%s)", i, pm_capa[e->capa].nome, f[0], f[1], f[2], f[3]);
        else if (e->tipo == E_CIRCULO || e->tipo == E_ARCO) formatar_texto(b, sizeof b, "#%d %s capa %s: centro (%s,%s) radio %s", i, NOMBRE_TIPO[e->tipo], pm_capa[e->capa].nome, f[0], f[1], f[4]);
        else formatar_texto(b, sizeof b, "#%d %s capa %s", i, NOMBRE_TIPO[e->tipo], pm_capa[e->capa].nome);
        log_(b);
    }
    fin();
}

/* ---- órdenes inmediatas (con argumentos en la misma línea) ---- */
static const char *sin_espacios(const char *s) { while (*s == ' ') s++; return s; }
static int tok(const char **s, char *d, int max)
{
    *s = sin_espacios(*s);
    int n = 0;
    while (**s && **s != ' ' && n < max - 1) d[n++] = *(*s)++;
    d[n] = 0;
    return n;
}
static void alterna(int *v, const char *nombre, const char *arg)
{
    char a[16];
    const char *p = arg;
    tok(&p, a, sizeof a);
    if (a[0] && (prefijo(a, "ON") || prefijo(a, "SI") || prefijo(a, "ACTIVAR") || prefijo(a, "YES"))) *v = 1;
    else if (a[0] && (prefijo(a, "OFF") || prefijo(a, "NO") || prefijo(a, "DESACTIVAR"))) *v = 0;
    else *v = !*v;
    char b[64];
    formatar_texto(b, sizeof b, "%s %s", nombre, *v ? "activado" : "desactivado");
    msg(b);
}
static void h_rejilla(int ev, double x, double y, int k, const char *t) { (void)x; (void)y; (void)k; (void)t; if (ev == V_INIT) alterna(&pm_rejilla, "Rejilla", c.args); fin(); }
static void h_orto(int ev, double x, double y, int k, const char *t) { (void)x; (void)y; (void)k; (void)t; if (ev == V_INIT) { alterna(&pm_orto, "Orto", c.args); if (pm_orto) pm_polar = 0; } fin(); }
static void h_forzc(int ev, double x, double y, int k, const char *t) { (void)x; (void)y; (void)k; (void)t; if (ev == V_INIT) alterna(&pm_forzc, "Forzcursor", c.args); fin(); }
static void h_polar(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) {
        const char *p = c.args;
        char a[16];
        if (tok(&p, a, sizeof a) && ((a[0] >= '0' && a[0] <= '9'))) { double v = pm_atof(a, NULL); if (v > 0 && v <= 180) pm_polar_ang = v; pm_polar = 1; msg("Rastreo polar activado"); }
        else alterna(&pm_polar, "Rastreo polar", c.args);
        if (pm_polar) pm_orto = 0;
    }
    fin();
}
static const struct { const char *n; int m; } REFNOM[] = {
    { "FIN", REF_FIN }, { "END", REF_FIN }, { "MEDIO", REF_MEDIO }, { "MID", REF_MEDIO }, { "CENTRO", REF_CENTRO }, { "CEN", REF_CENTRO },
    { "INTERSECCION", REF_INTER }, { "INT", REF_INTER }, { "PERPENDICULAR", REF_PERP }, { "PER", REF_PERP }, { "TANGENTE", REF_TANG }, { "TAN", REF_TANG },
    { "CUADRANTE", REF_CUAD }, { "QUA", REF_CUAD }, { "CERCANO", REF_CERCA }, { "NEA", REF_CERCA },
};
static void h_refent(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) {
        const char *p = c.args;
        char a[24];
        int m = 0;
        bool alguno = false;
        while (tok(&p, a, sizeof a)) {
            alguno = true;
            if (prefijo(a, "NINGUNO") || prefijo(a, "NONE")) { m = 0; continue; }
            if (prefijo(a, "TODO") || prefijo(a, "ALL")) { m = 255; continue; }
            for (unsigned q = 0; q < sizeof REFNOM / sizeof REFNOM[0]; q++) if (prefijo(a, REFNOM[q].n)) { m |= REFNOM[q].m; break; }
        }
        if (alguno && m) { pm_refmask = m; pm_refent = 1; msg("Referencia a objetos activada"); }
        else if (alguno && !m && (prefijo(c.args, "OFF") || prefijo(c.args, "NO") || prefijo(c.args, "NINGUNO"))) { pm_refent = 0; msg("Referencia a objetos desactivada"); }
        else alterna(&pm_refent, "Referencia a objetos", c.args);
    }
    fin();
}
static void h_deshacer(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) {
        int n = c.args[0] ? (int)pm_atof(c.args, NULL) : 1;
        if (n < 1) n = 1;
        int hechos = 0;
        for (int i = 0; i < n; i++) if (pm_deshacer()) hechos++;
        if (hechos) msg("Deshacer"); else err("No hay nada que deshacer");
    }
    act = false; cur = NULL; pm_esp = ESP_NADA; pm_prompt[0] = 0;
}
static void h_rehacer(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) { if (pm_rehacer()) msg("Rehacer"); else err("No hay nada que rehacer"); }
    act = false; cur = NULL; pm_esp = ESP_NADA; pm_prompt[0] = 0;
}
static void accion(int a, const char *arg) { pm_accion = a; copia_texto(pm_accion_arg, arg, sizeof pm_accion_arg); }
static void h_regen(int ev, double x, double y, int k, const char *t) { (void)x; (void)y; (void)k; (void)t; if (ev == V_INIT) { accion(PMA_REDIBUJAR, ""); msg("Regenerado"); } fin(); }
static void h_ayuda(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) {
        accion(PMA_AYUDA, "");
        log_("Dibujo: LINEA(L) PLINEA(PL) RECTANG(REC) POLIGONO(POL) CIRCULO(C) ARCO(A) PUNTO(PO) TEXTO(T) SOMBREADO(H)");
        log_("Cotas: COTALIN COTAHOR COTAVER COTAALI COTARAD COTADIAM COTAANG COTAESTILO");
        log_("Modificar: MOVER(M) COPIAR(CO) GIRAR(RO) ESCALA(SC) SIMETRIA(MI) EQUIDIST(O) RECORTA(TR) ALARGA(EX) EMPALME(F)");
        log_("  CHAFLAN(CHA) BORRAR(E) EXPLOTA(X) MATRIZ(AR) IGUALAPROP(MA)");
        log_("Vista y ayudas: ZOOM(Z) PAN REGEN REJILLA(F7) ORTO(F8) FORZC(F9) POLAR(F10) REFENT(F3) CAPA DESHACER(U) REHACER");
        log_("Archivo: GUARDAR ABRIR NUEVO EXPORTAR(pdf|png|dxf archivo) IMPORTAR(dxf) SALIR. Inglés: LINE CIRCLE MOVE COPY TRIM...");
        log_("Coordenadas: x,y  @dx,dy  @d<angulo  (solo distancia con ORTO o POLAR). Intro repite la orden; Esc cancela.");
    }
    fin();
}
static void h_archivo(int ev, int a)
{
    if (ev == V_INIT) {
        const char *p = sin_espacios(c.args);
        accion(a, p);
        if (a == PMA_ABRIR || a == PMA_GUARDAR) log_(a == PMA_ABRIR ? "Abrir" : "Guardar");
    }
    act = false; cur = NULL; pm_esp = ESP_NADA; pm_prompt[0] = 0;
}
static void h_abrir(int ev, double x, double y, int k, const char *t) { (void)x; (void)y; (void)k; (void)t; h_archivo(ev, PMA_ABRIR); }
static void h_guardar(int ev, double x, double y, int k, const char *t) { (void)x; (void)y; (void)k; (void)t; h_archivo(ev, PMA_GUARDAR); }
static void h_nuevo(int ev, double x, double y, int k, const char *t) { (void)x; (void)y; (void)k; (void)t; h_archivo(ev, PMA_NUEVO); }
static void h_salir(int ev, double x, double y, int k, const char *t) { (void)x; (void)y; (void)k; (void)t; h_archivo(ev, PMA_SALIR); }
static void h_props(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) { pide(ESP_SEL, "Designar objetos", ""); return; }
    if (ev == V_SEL) { if (pm_sel_contar()) accion(PMA_PROPS, ""); act = false; pm_esp = ESP_NADA; pm_prompt[0] = 0; cur = NULL; }
}
static void h_exportar(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) {
        const char *p = c.args;
        char tipo[12];
        tok(&p, tipo, sizeof tipo);
        p = sin_espacios(p);
        if (prefijo(tipo, "PDF")) accion(PMA_PDF, p);
        else if (prefijo(tipo, "PNG")) accion(PMA_PNG, p);
        else if (prefijo(tipo, "DXF")) accion(PMA_DXF, p);
        else err("EXPORTAR pdf|png|dxf [archivo]");
    }
    act = false; cur = NULL; pm_esp = ESP_NADA; pm_prompt[0] = 0;
}
static void h_importar(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) {
        const char *p = c.args;
        char tipo[12];
        tok(&p, tipo, sizeof tipo);
        p = sin_espacios(p);
        if (prefijo(tipo, "DXF")) accion(PMA_IMPDXF, p); else if (tipo[0]) accion(PMA_ABRIR, c.args); else err("IMPORTAR dxf [archivo]");
    }
    act = false; cur = NULL; pm_esp = ESP_NADA; pm_prompt[0] = 0;
}

/* ---- capas ---- */
static int ltipo_de(const char *s)
{
    for (int i = 0; i < LT_N; i++) if (prefijo(s, pm_ltipo_nombre[i])) return i;
    if (prefijo(s, "DASHED") || prefijo(s, "TRAZO")) return LT_TRAZOS;
    if (prefijo(s, "DOT") || prefijo(s, "PUNTO")) return LT_PUNTOS;
    if (prefijo(s, "DASHDOT") || prefijo(s, "TRAZOPUNTO")) return LT_TRAZOPUNTO;
    if (prefijo(s, "CONT")) return LT_CONT;
    return -1;
}
static void h_capa(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) {
        char sub[16];
        const char *p = c.args;
        if (!tok(&p, sub, sizeof sub)) {
            accion(PMA_CAPAS, "");
            char b[120];
            log_("Capas (* = actual):");
            for (int i = 0; i < pm_ncapa; i++) {
                formatar_texto(b, sizeof b, " %s%s color %x tipo %s%s%s", i == pm_capa_act ? "*" : "", pm_capa[i].nome, (unsigned)pm_capa[i].color, pm_ltipo_nombre[pm_capa[i].ltipo],
                               pm_capa[i].visible ? "" : " apagada", pm_capa[i].bloq ? " bloqueada" : "");
                log_(b);
            }
        } else {
            p = sin_espacios(p);
            char nombre[32];
            copia_texto(nombre, p, sizeof nombre);
            for (int i = (int)tam_texto(nombre) - 1; i >= 0 && nombre[i] == ' '; i--) nombre[i] = 0;
            if (prefijo(sub, "NUEVA") || prefijo(sub, "NEW")) { int i = pm_capa_nueva(nombre); if (i < 0) err("No se pudo crear la capa"); else { pm_capa_act = i; msg2("Capa ", pm_capa[i].nome, " creada y actual"); } }
            else if (prefijo(sub, "ACTUAL") || prefijo(sub, "SET")) { int i = pm_capa_buscar(nombre); if (i < 0) err("Esa capa no existe"); else if (!pm_capa[i].visible) err("La capa está apagada"); else { pm_capa_act = i; msg2("Capa actual: ", pm_capa[i].nome, ""); } }
            else if (prefijo(sub, "APAGAR") || prefijo(sub, "OFF")) { int i = pm_capa_buscar(nombre); if (i < 0) err("Esa capa no existe"); else if (i == pm_capa_act) err("No se puede apagar la capa actual"); else { pm_capa[i].visible = 0; msg("Capa apagada"); } }
            else if (prefijo(sub, "ACTIVAR") || prefijo(sub, "ON")) { int i = pm_capa_buscar(nombre); if (i < 0) err("Esa capa no existe"); else { pm_capa[i].visible = 1; msg("Capa activada"); } }
            else if (prefijo(sub, "BLOQUEAR") || prefijo(sub, "LOCK")) { int i = pm_capa_buscar(nombre); if (i < 0) err("Esa capa no existe"); else { pm_capa[i].bloq = 1; msg("Capa bloqueada"); } }
            else if (prefijo(sub, "DESBLOQUEAR") || prefijo(sub, "UNLOCK")) { int i = pm_capa_buscar(nombre); if (i < 0) err("Esa capa no existe"); else { pm_capa[i].bloq = 0; msg("Capa desbloqueada"); } }
            else if (prefijo(sub, "COLOR") || prefijo(sub, "LTIPO") || prefijo(sub, "LTYPE")) {
                char *u = nombre + tam_texto(nombre);
                while (u > nombre && u[-1] != ' ') u--;
                if (u == nombre) err("CAPA COLOR nombre número (1-9)");
                else {
                    char val[24];
                    copia_texto(val, u, sizeof val);
                    u[-1] = 0;
                    for (char *q = u - 1; q > nombre && q[-1] == ' '; q--) q[-1] = 0;
                    int i = pm_capa_buscar(nombre);
                    if (i < 0) err("Esa capa no existe");
                    else if (prefijo(sub, "COLOR")) { int a = (int)pm_atof(val, NULL); if (a < 1 || a > 255) err("Color entre 1 y 255"); else { pm_capa[i].color = pm_aci_color(a); msg("Color de la capa cambiado"); } }
                    else { int lt = ltipo_de(val); if (lt < 0) err("Tipo de línea: Continua, Trazos, Puntos, TrazoPunto"); else { pm_capa[i].ltipo = (uint8_t)lt; msg("Tipo de línea de la capa cambiado"); } }
                }
            }
            else if (prefijo(sub, "LISTA") || prefijo(sub, "LIST")) accion(PMA_CAPAS, "");
            else err("CAPA [Nueva|Actual|Apagar|Activar|Bloquear|Desbloquear|Color|Ltipo] nombre");
        }
    }
    fin();
}
static void h_color(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) {
        const char *p = sin_espacios(c.args);
        if (!*p) err("COLOR PorCapa | 1..255");
        else if (prefijo(p, "PORCAPA") || prefijo(p, "BYLAYER")) { pm_color_act = COL_CAPA; msg("Color: PorCapa"); }
        else { int a = (int)pm_atof(p, NULL); if (a < 1 || a > 255) err("Color entre 1 y 255"); else { pm_color_act = (int32_t)pm_aci_color(a); msg("Color actual cambiado"); } }
    }
    fin();
}
static void h_tlinea(int ev, double x, double y, int k, const char *t)
{
    (void)x; (void)y; (void)k; (void)t;
    if (ev == V_INIT) {
        const char *p = sin_espacios(c.args);
        if (prefijo(p, "PORCAPA") || prefijo(p, "BYLAYER")) { pm_ltipo_act = LT_CAPA; msg("Tipo de línea: PorCapa"); }
        else { int lt = ltipo_de(p); if (lt < 0) err("Tipo de línea: PorCapa, Continua, Trazos, Puntos, TrazoPunto"); else { pm_ltipo_act = lt; msg("Tipo de línea actual cambiado"); } }
    }
    fin();
}

/* ============================================================ tabla de órdenes */

#define CMD(n, f, fl) { n, f, fl }
static const cmddef_t TABLA[] = {
    CMD("LINEA,LINE,L", h_linea, 0), CMD("PLINEA,PLINE,PL,POLILINEA", h_plinea, 0), CMD("RECTANG,RECTANGLE,REC,RECTANGULO", h_rectang, 0),
    CMD("POLIGONO,POLYGON,POL", h_poligono, 0), CMD("CIRCULO,CIRCLE,C", h_circulo, 0), CMD("ARCO,ARC,A", h_arco, 0),
    CMD("PUNTO,POINT,PO", h_punto, 0), CMD("TEXTO,TEXT,T", h_texto, 0),
    CMD("COTALIN,DIMLINEAR,DLI", h_cotalin, 0), CMD("COTAHOR,DIMHORIZONTAL", h_cotahor, 0), CMD("COTAVER,DIMVERTICAL", h_cotaver, 0),
    CMD("COTAALI,DIMALIGNED,DAL", h_cotaali, 0), CMD("COTARAD,DIMRADIUS,DRA", h_cotarad, 0), CMD("COTADIAM,DIMDIAMETER,DDI", h_cotadiam, 0),
    CMD("COTAANG,DIMANGULAR,DAN", h_cotaang, 0), CMD("COTAESTILO,DIMSTYLE", h_cotaestilo, 0), CMD("SOMBREADO,HATCH,H", h_sombreado, 0),
    CMD("BORRAR,ERASE,E", h_borrar, F_SEL), CMD("MOVER,MOVE,M", h_mover, F_SEL), CMD("COPIAR,COPY,CO,CP", h_copiar, F_SEL),
    CMD("GIRAR,ROTATE,RO", h_girar, F_SEL), CMD("ESCALA,SCALE,SC", h_escala, F_SEL), CMD("SIMETRIA,MIRROR,MI", h_simetria, F_SEL),
    CMD("EQUIDIST,OFFSET,O", h_equidist, 0), CMD("RECORTA,TRIM,TR", h_recorta, F_SEL), CMD("ALARGA,EXTEND,EX", h_alarga, F_SEL),
    CMD("EMPALME,FILLET,F", h_empalme, 0), CMD("CHAFLAN,CHAMFER,CHA", h_chaflan, 0), CMD("EXPLOTA,EXPLODE,X", h_explota, F_SEL),
    CMD("MATRIZ,ARRAY,AR", h_matriz, F_SEL), CMD("IGUALAPROP,MATCHPROP,MA", h_igualaprop, 0),
    CMD("ZOOM,Z", h_zoom, 0), CMD("PAN", h_pan, 0), CMD("REGEN,REGENERAR,RE", h_regen, F_INM), CMD("DIST,DISTANCIA,DI", h_dist, 0), CMD("LISTA,LIST,LI", h_lista, F_SEL),
    CMD("REJILLA,GRID", h_rejilla, F_INM), CMD("ORTO,ORTHO", h_orto, F_INM), CMD("FORZC,SNAP,FORZCURSOR", h_forzc, F_INM),
    CMD("POLAR", h_polar, F_INM), CMD("REFENT,OSNAP", h_refent, F_INM), CMD("CAPA,LAYER,LA", h_capa, F_INM),
    CMD("COLOR,COLOUR", h_color, F_INM), CMD("TLINEA,LINETYPE,LT", h_tlinea, F_INM),
    CMD("DESHACER,UNDO,U", h_deshacer, F_INM), CMD("REHACER,REDO", h_rehacer, F_INM), CMD("AYUDA,HELP,?", h_ayuda, F_INM),
    CMD("GUARDAR,SAVE,QSAVE,Q", h_guardar, F_INM), CMD("ABRIR,OPEN", h_abrir, F_INM), CMD("NUEVO,NEW", h_nuevo, F_INM),
    CMD("EXPORTAR,EXPORT", h_exportar, F_INM), CMD("IMPORTAR,IMPORT", h_importar, F_INM), CMD("SALIR,QUIT,EXIT", h_salir, F_INM),
    CMD("PROPIEDADES,PROPERTIES,PR", h_props, F_SEL),
};
#define NCMD ((int)(sizeof TABLA / sizeof TABLA[0]))

int pm_ordenes_lista(int i, const char **nombre, const char **alias)
{
    if (i < 0 || i >= NCMD) return 0;
    *nombre = TABLA[i].nombres;
    *alias = TABLA[i].nombres;
    return 1;
}

static bool nombre_igual(const char *lista, const char *tk)       /* tk coincide exactamente con algún nombre de la lista */
{
    const char *q = lista;
    while (*q) {
        const char *a = tk, *b = q;
        while (*a && *b && *b != ',' && fold(*a) == fold(*b)) { a++; b++; }
        if (!*a && (!*b || *b == ',')) return true;
        while (*q && *q != ',') q++;
        if (*q == ',') q++;
    }
    return false;
}
static const cmddef_t *buscar_orden(const char *tk)
{
    if (!tk[0]) return NULL;
    for (int i = 0; i < NCMD; i++) if (nombre_igual(TABLA[i].nombres, tk)) return &TABLA[i];
    if (tam_texto(tk) >= 3) {          /* prefijo único del nombre principal */
        const cmddef_t *u = NULL;
        int n = 0;
        for (int i = 0; i < NCMD; i++) {
            char nm[24];
            int a = 0;
            for (const char *q = TABLA[i].nombres; *q && *q != ',' && a < 23; q++) nm[a++] = *q;
            nm[a] = 0;
            if (a > 2 && prefijo(tk, nm)) { u = &TABLA[i]; n++; }
        }
        if (n == 1) return u;
    }
    return NULL;
}

int pm_sugerir(const char *pref, char (*sal)[20], int max)
{
    int n = 0;
    if (!pref[0]) return 0;
    for (int i = 0; i < NCMD && n < max; i++) {
        const char *q = TABLA[i].nombres;
        while (*q && n < max) {
            char nm[20];
            int a = 0;
            while (*q && *q != ',' && a < 19) nm[a++] = *q++;
            nm[a] = 0;
            if (a >= 2 && prefijo(pref, nm) && (int)tam_texto(pref) <= a) { copia_texto(sal[n++], nm, 20); }
            if (*q == ',') q++;
        }
    }
    return n;
}

bool pm_activa(void) { return act; }
const char *pm_orden_nombre(void) { return cur ? cur->nombres : ""; }
static const cmddef_t *ultima;
static char pend_args[160];

static void evento(int ev, double x, double y, int k, const char *t)
{
    if (!act || !cur) return;
    cur->fn(ev, x, y, k, t);
}

void pm_iniciar_orden(const char *nombre)
{
    const cmddef_t *d = buscar_orden(nombre);
    if (!d) { char b[80]; formatar_texto(b, sizeof b, "Orden desconocida: %s (AYUDA para ver la lista)", nombre); err(b); return; }
    if (act) { if (c.tmp.v) pm_ent_liberar(&c.tmp); pm_sel_todo(false); }
    memset(&c, 0, sizeof c);
    copia_texto(c.args, pend_args, sizeof c.args);
    pend_args[0] = 0;
    act = true; cur = d;
    pm_msg_err = false;
    if (!(d->flags & F_INM)) ultima = d;
    {
        char b[64];
        char nom[24];
        copia_texto(nom, d->nombres, sizeof nom);
        for (char *q = nom; *q; q++) if (*q == ',') { *q = 0; break; }
        formatar_texto(b, sizeof b, "Orden: %s", nom);
        log_(b);
    }
    bool presel = pm_sel_contar() > 0;
    d->fn(V_INIT, 0, 0, 0, NULL);
    if ((d->flags & F_SEL) && presel && act && cur == d && pm_esp == ESP_SEL) {   /* objetos ya designados: se usan sin preguntar */
        char b[48];
        formatar_texto(b, sizeof b, "%d objetos designados", pm_sel_contar());
        log_(b);
        d->fn(V_SEL, 0, 0, 0, NULL);
    }
}

/* ============================================================ entrada de puntos y texto */

static void dir_restringida(double *ux, double *uy)
{
    double dx = pm_cur_x - pm_ult_x, dy = pm_cur_y - pm_ult_y;
    if (pm_orto) {
        if (gm_abs(dx) >= gm_abs(dy)) { *ux = dx >= 0 ? 1 : -1; *uy = 0; } else { *ux = 0; *uy = dy >= 0 ? 1 : -1; }
        return;
    }
    double l = gm_hypot(dx, dy);
    if (l < 1e-12) { *ux = 1; *uy = 0; return; }
    double inc = pm_polar_ang * PM_PI / 180.0, a = gm_atan2(dy, dx);
    a = gm_round(a / inc) * inc;
    *ux = gm_cos(a); *uy = gm_sin(a);
    if (gm_abs(*ux) < 1e-12) *ux = 0;
    if (gm_abs(*uy) < 1e-12) *uy = 0;
}

bool pm_parse_coord(const char *s, double *x, double *y)
{
    bool rel = false;
    if (*s == '@') { rel = true; s++; }
    const char *e;
    double a = pm_atof(s, &e);
    if (e == s) return false;
    double bx = rel && pm_tiene_ult ? pm_ult_x : 0, by = rel && pm_tiene_ult ? pm_ult_y : 0;
    if (*e == ',') {
        const char *s2 = e + 1;
        double b = pm_atof(s2, &e);
        if (e == s2 || *e) return false;
        *x = (rel ? bx : 0) + a; *y = (rel ? by : 0) + b;
        return true;
    }
    if (*e == '<') {
        const char *s2 = e + 1;
        double ang = pm_atof(s2, &e);
        if (e == s2 || *e) return false;
        ang *= PM_PI / 180.0;
        *x = bx + a * gm_cos(ang); *y = by + a * gm_sin(ang);
        return true;
    }
    if (*e == 0 && (pm_orto || pm_polar) && pm_tiene_ult) {
        double ux, uy;
        dir_restringida(&ux, &uy);
        *x = pm_ult_x + ux * a; *y = pm_ult_y + uy * a;
        return true;
    }
    return false;
}

static void sel_punto(double x, double y)
{
    int i = pm_entidad_en(x, y, ap(), -1);
    if (i < 0) { err("Ningún objeto en ese punto"); return; }
    pm_ent[i].sel = pm_quitar ? 0 : 1;
    char b[48];
    formatar_texto(b, sizeof b, "%d objetos designados", pm_sel_contar());
    msg(b);
}

/* Punto procedente del teclado o de un clic (ya refinado). */
static void punto_interno(double x, double y)
{
    if (!act) { sel_punto(x, y); return; }
    switch (pm_esp) {
    case ESP_PUNTO: evento(V_PT, x, y, 0, NULL); break;
    case ESP_NUM:
        if (num_pt) { evento(V_PT, x, y, 0, NULL); break; }
        if (num_ang) {
            double bx = pm_tiene_ult ? pm_ult_x : 0, by = pm_tiene_ult ? pm_ult_y : 0;
            evento(V_NUM, pm_ang(x - bx, y - by) * 180.0 / PM_PI, 0, 0, NULL);
        } else {
            double bx = pm_tiene_ult ? pm_ult_x : 0, by = pm_tiene_ult ? pm_ult_y : 0;
            evento(V_NUM, pm_dist(bx, by, x, y), 0, 0, NULL);
        }
        break;
    case ESP_SEL: sel_punto(x, y); break;
    case ESP_OBJ: {
        int i = pm_entidad_en(x, y, ap(), -1);
        if (i < 0) { err("Ningún objeto en ese punto"); return; }
        evento(V_OBJ, x, y, i, NULL);
        break;
    }
    default: err("Ahora no se espera un punto"); break;
    }
}

void pm_punto(double x, double y)
{
    char b[120], f1[32], f2[32];
    if (act && pm_prompt[0]) { pm_fmt(x, 3, f1, sizeof f1); pm_fmt(y, 3, f2, sizeof f2); formatar_texto(b, sizeof b, "%s %s,%s", pm_prompt, f1, f2); log_(b); }
    punto_interno(x, y);
}

void pm_ventana(double x0, double y0, double x1, double y1)
{
    if (act && pm_esp != ESP_SEL) return;
    int n = pm_sel_ventana(x0, y0, x1, y1, x1 < x0, pm_quitar);
    char b[64];
    formatar_texto(b, sizeof b, "%d objetos designados (%s)", pm_sel_contar(), x1 < x0 ? "captura" : "ventana");
    (void)n;
    msg(b);
}

void pm_enter(void)
{
    if (act) {
        if (pm_prompt[0]) { char b[220]; formatar_texto(b, sizeof b, "%s <Intro>", pm_prompt); log_(b); }
        if (pm_esp == ESP_SEL) evento(V_SEL, 0, 0, 0, NULL); else evento(V_ENT, 0, 0, 0, NULL);
    } else if (ultima) {
        char nom[24];
        copia_texto(nom, ultima->nombres, sizeof nom);
        for (char *q = nom; *q; q++) if (*q == ',') { *q = 0; break; }
        pm_iniciar_orden(nom);
    }
}

void pm_cancelar(void)
{
    if (act) { err("*Cancelar*"); fin(); }
    else pm_sel_todo(false);
}

static bool es_alfa(const char *tk)
{
    char c_ = tk[0];
    unsigned char u = (unsigned char)c_;
    if (c_ >= '0' && c_ <= '9') return (tk[1] == 'P' || tk[1] == 'p') && !tk[2];      /* 3P, 2P */
    return (c_ >= 'a' && c_ <= 'z') || (c_ >= 'A' && c_ <= 'Z') || u >= 0xC0 || c_ == '?';
}

static bool alimentar(const char *tk)
{
    char b[260];
    if (act && pm_prompt[0] && pm_esp != ESP_TEXTO) { formatar_texto(b, sizeof b, "%s %s", pm_prompt, tk); log_(b); }
    else if (act && pm_esp == ESP_TEXTO) { formatar_texto(b, sizeof b, "%s %s", pm_prompt, tk); log_(b); }
    int esp = pm_esp;
    if (esp == ESP_TEXTO) { evento(V_TXT, 0, 0, 0, tk); return true; }
    if (tk[0] == '#' && (esp == ESP_SEL || esp == ESP_OBJ)) {
        int i = (int)pm_atof(tk + 1, NULL);
        if (i < 0 || i >= pm_n) { err("No existe ese objeto"); return false; }
        if (esp == ESP_SEL) { pm_ent[i].sel = pm_quitar ? 0 : 1; return true; }
        evento(V_OBJ, pm_ent[i].p[0], pm_ent[i].p[1], i, NULL);
        return true;
    }
    if (es_alfa(tk)) {
        int o = op_match(tk);
        if (o >= 0) {
            if (esp == ESP_SEL && o < 2) {      /* opciones propias de la selección */
                if (o == 0) { for (int i = 0; i < pm_n; i++) if (pm_ent_editable(&pm_ent[i])) pm_ent[i].sel = 1; }
                else { for (int i = pm_n - 1; i >= 0; i--) if (pm_ent_editable(&pm_ent[i])) { pm_ent[i].sel = 1; break; } }
                formatar_texto(b, sizeof b, "%d objetos designados", pm_sel_contar());
                msg(b);
                return true;
            }
            evento(V_OPC, 0, 0, esp == ESP_SEL ? o - 2 : o, NULL);
            return true;
        }
        if (buscar_orden(tk)) { pm_iniciar_orden(tk); return true; }       /* una orden nueva interrumpe la actual */
        err("Opción no válida");
        return false;
    }
    if (esp == ESP_OPC) { err("Elige una de las opciones"); return false; }
    if (esp == ESP_NUM && !num_pt) {
        const char *e;
        double v = pm_atof(tk, &e);
        if (e != tk && !*e) { evento(V_NUM, v, 0, 0, NULL); return true; }
    }
    double x, y;
    if (pm_parse_coord(tk, &x, &y)) { punto_interno(x, y); return true; }
    if (esp == ESP_NUM) {
        const char *e;
        double v = pm_atof(tk, &e);
        if (e != tk && !*e) { evento(V_NUM, v, 0, 0, NULL); return true; }
    }
    err("Valor no válido");
    return false;
}

/* Prototipo: «LINE 0 0 10 10», «CIRCLE 3 4 2», «RECTANGLE 0 0 4 3», «POINT 1 2» (números sueltos) y «PAN dx dy», «ZOOM f». */
static bool solo_numeros(const char *s, int *cuantos)
{
    int n = 0;
    while (*s) {
        s = sin_espacios(s);
        if (!*s) break;
        const char *e;
        pm_atof(s, &e);
        if (e == s || (*e && *e != ' ')) return false;
        n++;
        s = e;
    }
    *cuantos = n;
    return n > 0;
}

static bool alimentar_linea(const char *p)
{
    bool ok = true;
    while (*p && act) {
        p = sin_espacios(p);
        if (!*p) break;
        if (pm_esp == ESP_TEXTO) { char t[100]; copia_texto(t, p, sizeof t); alimentar(t); break; }
        char tk[80];
        tok(&p, tk, sizeof tk);
        if (!alimentar(tk)) { ok = false; break; }
    }
    return ok;
}

bool pm_orden(const char *linea)
{
    const char *p = sin_espacios(linea);
    char b[200];
    if (!*p) { pm_enter(); return true; }
    if (act) return alimentar_linea(p);
    char nom[40];
    tok(&p, nom, sizeof nom);
    if (nom[0] == '-' && nom[1]) memmove(nom, nom + 1, tam_texto(nom));        /* -CAPA: igual que CAPA con argumentos */
    const cmddef_t *d = buscar_orden(nom);
    if (!d) { formatar_texto(b, sizeof b, "Orden desconocida: %s (AYUDA para ver la lista)", nom); err(b); return false; }
    const char *rest = sin_espacios(p);
    int nn = 0;
    if (nombre_igual("PAN", nom) && solo_numeros(rest, &nn) && nn >= 2) {
        double dx = pm_atof(rest, &p), dy = pm_atof(p, NULL);
        pm_vcx -= dx; pm_vcy -= dy;
        msg("Vista desplazada");
        return true;
    }
    if (nombre_igual("ZOOM,Z", nom) && solo_numeros(rest, &nn) && nn == 1) {
        double f = pm_atof(rest, NULL);
        pm_vista_guardar();
        pm_zoom_factor(f, pm_vw / 2.0, pm_vh / 2.0);
        msg("Zoom");
        return true;
    }
    copia_texto(pend_args, rest, sizeof pend_args);
    pm_iniciar_orden(nom);
    pend_args[0] = 0;
    if (!act || (d->flags & F_INM)) return true;
    /* Prototipo: «LINE 0 0 10 10», «CIRCLE 3 4 2», «RECTANGLE 0 0 4 3», «POINT 1 2» con los números sueltos */
    if (solo_numeros(rest, &nn) && ((nombre_igual("LINEA,LINE,L", nom) && nn == 4) || (nombre_igual("RECTANG,RECTANGLE,REC,RECTANGULO", nom) && nn == 4))) {
        double v[4];
        const char *q = rest;
        for (int i = 0; i < 4; i++) v[i] = pm_atof(q, &q);
        char t1[64], t2[64], f[4][32];
        for (int i = 0; i < 4; i++) pm_fmt(v[i], 9, f[i], 32);
        formatar_texto(t1, sizeof t1, "%s,%s", f[0], f[1]); formatar_texto(t2, sizeof t2, "%s,%s", f[2], f[3]);
        alimentar(t1); alimentar(t2);
        if (act) pm_enter();
        return true;
    }
    if (solo_numeros(rest, &nn) && ((nombre_igual("CIRCULO,CIRCLE,C", nom) && nn == 3) || (nombre_igual("PUNTO,POINT,PO", nom) && nn == 2))) {
        double v[3];
        const char *q = rest;
        for (int i = 0; i < nn; i++) v[i] = pm_atof(q, &q);
        char t1[64], f[3][32];
        for (int i = 0; i < nn; i++) pm_fmt(v[i], 9, f[i], 32);
        formatar_texto(t1, sizeof t1, "%s,%s", f[0], f[1]);
        alimentar(t1);
        if (nn == 3 && act) alimentar(f[2]);
        return true;
    }
    return alimentar_linea(rest);
}

/* ============================================================ cursor, vista previa elástica */

bool pm_base_ghost(double *bx, double *by)
{
    if (!act || !cur || !pm_tiene_ult || c.paso == 0) return false;
    if (pm_esp != ESP_PUNTO && !(pm_esp == ESP_NUM && (num_pt || num_ang || true))) return false;
    if (cur->fn == h_equidist || cur->fn == h_sombreado || cur->fn == h_cotarad || cur->fn == h_cotadiam || cur->fn == h_igualaprop || cur->fn == h_zoom) return false;
    *bx = pm_ult_x; *by = pm_ult_y;
    return true;
}

void pm_ref_cursor(double wx, double wy, double ap_, double *rx, double *ry, pm_ref_t *ref)
{
    *rx = wx; *ry = wy;
    ref->tipo = 0; ref->x = wx; ref->y = wy;
    pm_cur_x = wx; pm_cur_y = wy;
    if (!pm_ref_siempre && (!act || (pm_esp != ESP_PUNTO && pm_esp != ESP_NUM))) return;
    double ux = 0, uy = 0;
    bool base = pm_base_ghost(&ux, &uy);
    if (pm_refent && pm_refmask) {
        pm_ref_t r;
        if (pm_ref_buscar(wx, wy, ap_, pm_refmask, base, ux, uy, &r)) { *rx = r.x; *ry = r.y; *ref = r; return; }
    }
    if (base && pm_orto) {
        double dx = wx - ux, dy = wy - uy;
        if (gm_abs(dx) >= gm_abs(dy)) *ry = uy; else *rx = ux;
        return;
    }
    if (base && pm_polar) {
        double dx = wx - ux, dy = wy - uy, l = gm_hypot(dx, dy);
        if (l > 1e-12) {
            double inc = pm_polar_ang * PM_PI / 180.0, a = gm_atan2(dy, dx), q = gm_round(a / inc) * inc;
            if (gm_abs(a - q) < 4.0 * PM_PI / 180.0) { *rx = ux + l * gm_cos(q); *ry = uy + l * gm_sin(q); }
        }
        return;
    }
    if (pm_forzc && pm_paso_rej > 1e-9) { *rx = gm_round(wx / pm_paso_rej) * pm_paso_rej; *ry = gm_round(wy / pm_paso_rej) * pm_paso_rej; }
}

static int ghost_sel(const double *m, pm_ent_t *out, int max)
{
    int n = 0;
    for (int i = 0; i < pm_n && n < max; i++) {
        if (!pm_ent[i].sel) continue;
        pm_ent_clonar(&out[n], &pm_ent[i]);
        pm_ent_transformar(&out[n], m);
        n++;
    }
    return n;
}
static int ghost_poligono(pm_ent_t *e, double cx, double cy, double R, double a0, int n)
{
    pm_ent_nueva(e, E_POLI);
    for (int i = 0; i < n; i++) pm_poli_vert(e, cx + R * gm_cos(a0 + 2 * PM_PI * i / n), cy + R * gm_sin(a0 + 2 * PM_PI * i / n), 0);
    e->cerrada = 1;
    return 1;
}

int pm_fantasma(double x, double y, pm_ent_t *out, int max)
{
    if (!act || !cur || max < 1) return 0;
    hfn f = cur->fn;
    double m[6];
    if (f == h_linea && c.paso >= 1 && pm_esp == ESP_PUNTO) {
        pm_ent_nueva(&out[0], E_LINEA);
        out[0].p[0] = pm_ult_x; out[0].p[1] = pm_ult_y; out[0].p[2] = x; out[0].p[3] = y;
        return 1;
    }
    if (f == h_plinea && c.paso >= 1 && c.tmp.nv >= 1) {
        pm_ent_clonar(&out[0], &c.tmp);
        pm_vert_t *u = &out[0].v[out[0].nv - 1];
        if (c.f[0]) {
            double ch = gm_atan2(y - u->y, x - u->x), al = ch - c.tang;
            while (al > PM_PI) al -= 2 * PM_PI;
            while (al <= -PM_PI) al += 2 * PM_PI;
            u->b = gm_tan(al / 2);
        } else u->b = 0;
        pm_poli_vert(&out[0], x, y, 0);
        return 1;
    }
    if (f == h_rectang && c.paso == 1) { pm_ent_nueva(&out[0], E_POLI); pm_rect_poli(&out[0], c.pt[0][0], c.pt[0][1], x, y); return 1; }
    if (f == h_circulo && (c.paso == 1 || c.paso == 5)) {
        double r = pm_dist(c.pt[0][0], c.pt[0][1], x, y) / (c.paso == 5 ? 2 : 1);
        if (r < PM_EPS) return 0;
        pm_ent_nueva(&out[0], E_CIRCULO);
        out[0].p[0] = c.pt[0][0]; out[0].p[1] = c.pt[0][1]; out[0].r = r;
        return 1;
    }
    if (f == h_poligono && c.paso == 4) {
        double r = pm_dist(c.pt[0][0], c.pt[0][1], x, y), a0 = pm_ang(x - c.pt[0][0], y - c.pt[0][1]);
        if (r < PM_EPS) return 0;
        if (c.f[0]) { r /= gm_cos(PM_PI / pol_n); a0 += PM_PI / pol_n; }
        return ghost_poligono(&out[0], c.pt[0][0], c.pt[0][1], r, a0, pol_n);
    }
    if (f == h_arco && c.paso == 2) {
        double ox, oy, r;
        if (!circulo3(c.pt[0][0], c.pt[0][1], c.pt[1][0], c.pt[1][1], x, y, &ox, &oy, &r)) return 0;
        double cr = (c.pt[1][0] - c.pt[0][0]) * (y - c.pt[1][1]) - (c.pt[1][1] - c.pt[0][1]) * (x - c.pt[1][0]);
        pm_ent_nueva(&out[0], E_ARCO);
        out[0].p[0] = ox; out[0].p[1] = oy; out[0].r = r;
        double a1 = pm_ang(c.pt[0][0] - ox, c.pt[0][1] - oy), a3 = pm_ang(x - ox, y - oy);
        if (cr > 0) { out[0].a0 = a1; out[0].a1 = a3; } else { out[0].a0 = a3; out[0].a1 = a1; }
        return 1;
    }
    if ((f == h_mover || f == h_copiar) && c.paso == 2) { pm_mat_traslacion(m, x - c.pt[0][0], y - c.pt[0][1]); return ghost_sel(m, out, max); }
    if (f == h_girar && c.paso == 2) { pm_mat_giro(m, c.pt[0][0], c.pt[0][1], pm_ang(x - c.pt[0][0], y - c.pt[0][1])); return ghost_sel(m, out, max); }
    if (f == h_escala && c.paso == 2) { double fa = pm_dist(c.pt[0][0], c.pt[0][1], x, y); if (fa < PM_EPS) return 0; pm_mat_escala(m, c.pt[0][0], c.pt[0][1], fa); return ghost_sel(m, out, max); }
    if (f == h_simetria && c.paso == 2) { if (pm_dist(c.pt[0][0], c.pt[0][1], x, y) < PM_EPS) return 0; pm_mat_simetria(m, c.pt[0][0], c.pt[0][1], x, y); return ghost_sel(m, out, max); }
    return 0;
}

/* ============================================================ deshacer / rehacer */

#define HS_MAX 100
static char *hs[HS_MAX];
static int hs_n, hs_p = -1;

void pm_historial_guardar(void)
{
    int n;
    char *t = pm_serializar_ex(&n, false);
    if (!t) return;
    if (hs_p >= 0 && compara_texto(hs[hs_p], t) == 0) { libera(t); return; }
    for (int i = hs_p + 1; i < hs_n; i++) { libera(hs[i]); hs[i] = NULL; }
    hs_n = hs_p + 1;
    if (hs_n >= HS_MAX) { libera(hs[0]); memmove(hs, hs + 1, sizeof(char *) * (HS_MAX - 1)); hs_n--; }
    hs[hs_n++] = t;
    hs_p = hs_n - 1;
    if (hs_n > 1) pm_modificado = true;
}
void pm_historial_reiniciar(void)
{
    for (int i = 0; i < hs_n; i++) { libera(hs[i]); hs[i] = NULL; }
    hs_n = 0; hs_p = -1;
    pm_historial_guardar();
    pm_modificado = false;
}
bool pm_deshacer(void)
{
    if (hs_p <= 0) return false;
    hs_p--;
    pm_cargar_ex(hs[hs_p], false);
    pm_modificado = true;
    return true;
}
bool pm_rehacer(void)
{
    if (hs_p + 1 >= hs_n) return false;
    hs_p++;
    pm_cargar_ex(hs[hs_p], false);
    pm_modificado = true;
    return true;
}
