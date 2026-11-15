// SPDX-License-Identifier: GPL-3.0-or-later
// PlanoMini, © 2026 Alejandro Anuncibay: prototipo original (C++/SDL) del mantenedor; versión nativa de SaguiOS.
/* pm_mod.c - selección, pinzamientos, propiedades, referencia a objetos y órdenes de modificación
 * (recortar, alargar, equidistancia, empalme, chaflán, explotar, sombreado) de PlanoMini (D312). */
#include "pm.h"

int pm_explo_n;
static int fallo(const char *t) { copia_texto(pm_erro, t, sizeof pm_erro); return -1; }

/* ============================================================ selección */

int pm_sel_contar(void) { int n = 0; for (int i = 0; i < pm_n; i++) if (pm_ent[i].sel) n++; return n; }
void pm_sel_todo(bool v) { for (int i = 0; i < pm_n; i++) pm_ent[i].sel = (v && pm_ent_editable(&pm_ent[i])) ? 1 : 0; }

int pm_entidad_en(double x, double y, double ap, int saltar)
{
    int mejor = -1;
    double dm = ap;
    for (int i = 0; i < pm_n; i++) {
        if (i == saltar || !pm_ent_editable(&pm_ent[i])) continue;
        double d = pm_dist_ent(&pm_ent[i], x, y);
        if (d <= dm) { dm = d; mejor = i; }
    }
    return mejor;
}

int pm_sel_ventana(double x0, double y0, double x1, double y1, bool cruce, bool quitar)
{
    if (x0 > x1) { double t = x0; x0 = x1; x1 = t; }
    if (y0 > y1) { double t = y0; y0 = y1; y1 = t; }
    int n = 0;
    for (int i = 0; i < pm_n; i++) {
        if (!pm_ent_editable(&pm_ent[i])) continue;
        if (pm_ent_en_ventana(&pm_ent[i], x0, y0, x1, y1, cruce)) { pm_ent[i].sel = quitar ? 0 : 1; n++; }
    }
    return n;
}

/* ============================================================ pinzamientos */

int pm_grips(const pm_ent_t *e, double (*o)[2], int max)
{
    int n = 0;
#define G(a, b) do { if (n < max) { o[n][0] = (a); o[n][1] = (b); n++; } } while (0)
    switch (e->tipo) {
    case E_PUNTO: G(e->p[0], e->p[1]); break;
    case E_LINEA: G(e->p[0], e->p[1]); G(e->p[2], e->p[3]); G((e->p[0] + e->p[2]) / 2, (e->p[1] + e->p[3]) / 2); break;
    case E_CIRCULO:
        G(e->p[0], e->p[1]); G(e->p[0] + e->r, e->p[1]); G(e->p[0], e->p[1] + e->r); G(e->p[0] - e->r, e->p[1]); G(e->p[0], e->p[1] - e->r);
        break;
    case E_ARCO: {
        double sw = pm_angnorm(e->a1 - e->a0);
        if (sw < 1e-12) sw = 2 * PM_PI;
        G(e->p[0] + e->r * gm_cos(e->a0), e->p[1] + e->r * gm_sin(e->a0));
        G(e->p[0] + e->r * gm_cos(e->a0 + sw), e->p[1] + e->r * gm_sin(e->a0 + sw));
        G(e->p[0] + e->r * gm_cos(e->a0 + sw / 2), e->p[1] + e->r * gm_sin(e->a0 + sw / 2));
        G(e->p[0], e->p[1]);
        break;
    }
    case E_POLI: for (int i = 0; i < e->nv; i++) G(e->v[i].x, e->v[i].y); break;
    case E_TEXTO: G(e->p[0], e->p[1]); break;
    case E_COTA:
        if (e->sub == C_RADIO || e->sub == C_DIAM) G(e->p[0] + e->r * gm_cos(e->a0), e->p[1] + e->r * gm_sin(e->a0));
        else if (e->sub == C_ANGULAR) { G(e->p[6], e->p[7]); G(e->p[2], e->p[3]); G(e->p[4], e->p[5]); }
        else { G(e->p[0], e->p[1]); G(e->p[2], e->p[3]); G(e->p[4], e->p[5]); }
        break;
    }
#undef G
    return n;
}

void pm_grip_mover(int idx, int g, double x, double y)
{
    if (idx < 0 || idx >= pm_n) return;
    pm_ent_t *e = &pm_ent[idx];
    double gp[PM_VERT_MAX + 8][2], m[6];
    int n = pm_grips(e, gp, PM_VERT_MAX + 8);
    if (g < 0 || g >= n) return;
    switch (e->tipo) {
    case E_PUNTO: case E_TEXTO: e->p[0] = x; e->p[1] = y; break;
    case E_LINEA:
        if (g == 0) { e->p[0] = x; e->p[1] = y; }
        else if (g == 1) { e->p[2] = x; e->p[3] = y; }
        else { pm_mat_traslacion(m, x - gp[2][0], y - gp[2][1]); pm_ent_transformar(e, m); }
        break;
    case E_CIRCULO:
        if (g == 0) { pm_mat_traslacion(m, x - e->p[0], y - e->p[1]); pm_ent_transformar(e, m); }
        else { double r = gm_hypot(x - e->p[0], y - e->p[1]); if (r > PM_EPS) e->r = r; }
        break;
    case E_ARCO:
        if (g == 0) e->a0 = pm_ang(x - e->p[0], y - e->p[1]);
        else if (g == 1) e->a1 = pm_ang(x - e->p[0], y - e->p[1]);
        else if (g == 2) { double r = gm_hypot(x - e->p[0], y - e->p[1]); if (r > PM_EPS) e->r = r; }
        else { pm_mat_traslacion(m, x - e->p[0], y - e->p[1]); pm_ent_transformar(e, m); }
        break;
    case E_POLI: e->v[g].x = x; e->v[g].y = y; break;
    case E_COTA:
        if (e->sub == C_RADIO || e->sub == C_DIAM) e->a0 = pm_ang(x - e->p[0], y - e->p[1]);
        else if (e->sub == C_ANGULAR) { int k = g == 0 ? 6 : g == 1 ? 2 : 4; e->p[k] = x; e->p[k + 1] = y; }
        else { e->p[2 * g] = x; e->p[2 * g + 1] = y; }
        break;
    }
}

/* ============================================================ propiedades editables */

static void et(char (*q)[20], int i, const char *t) { copia_texto(q[i], t, 20); }
int pm_props_campos(const pm_ent_t *e, char (*q)[20], double *v)
{
    const double R2D = 180.0 / PM_PI;
    switch (e->tipo) {
    case E_PUNTO: et(q, 0, "X"); et(q, 1, "Y"); v[0] = e->p[0]; v[1] = e->p[1]; return 2;
    case E_LINEA:
        et(q, 0, "X inicial"); et(q, 1, "Y inicial"); et(q, 2, "X final"); et(q, 3, "Y final"); et(q, 4, "Longitud"); et(q, 5, "Ángulo");
        v[0] = e->p[0]; v[1] = e->p[1]; v[2] = e->p[2]; v[3] = e->p[3];
        v[4] = gm_hypot(e->p[2] - e->p[0], e->p[3] - e->p[1]); v[5] = pm_ang(e->p[2] - e->p[0], e->p[3] - e->p[1]) * R2D;
        return 6;
    case E_CIRCULO: et(q, 0, "Centro X"); et(q, 1, "Centro Y"); et(q, 2, "Radio"); v[0] = e->p[0]; v[1] = e->p[1]; v[2] = e->r; return 3;
    case E_ARCO:
        et(q, 0, "Centro X"); et(q, 1, "Centro Y"); et(q, 2, "Radio"); et(q, 3, "Ángulo inicial"); et(q, 4, "Ángulo final");
        v[0] = e->p[0]; v[1] = e->p[1]; v[2] = e->r; v[3] = pm_angnorm(e->a0) * R2D; v[4] = pm_angnorm(e->a1) * R2D;
        return 5;
    case E_POLI: et(q, 0, "Cerrada (0/1)"); v[0] = e->cerrada; return 1;
    case E_TEXTO: et(q, 0, "X"); et(q, 1, "Y"); et(q, 2, "Altura"); et(q, 3, "Giro"); v[0] = e->p[0]; v[1] = e->p[1]; v[2] = e->alt; v[3] = e->giro * R2D; return 4;
    case E_COTA: et(q, 0, "Altura del texto"); v[0] = e->alt; return 1;
    case E_SOMBRA: et(q, 0, "Ángulo"); et(q, 1, "Espaciado"); v[0] = e->a0 * R2D; v[1] = e->r; return 2;
    }
    return 0;
}

void pm_props_aplicar(pm_ent_t *e, const double *v)
{
    const double D2R = PM_PI / 180.0;
    switch (e->tipo) {
    case E_PUNTO: e->p[0] = v[0]; e->p[1] = v[1]; break;
    case E_LINEA: {
        double l0 = gm_hypot(e->p[2] - e->p[0], e->p[3] - e->p[1]), a0 = pm_ang(e->p[2] - e->p[0], e->p[3] - e->p[1]) * 180.0 / PM_PI;
        bool cambio_pos = v[0] != e->p[0] || v[1] != e->p[1] || v[2] != e->p[2] || v[3] != e->p[3];
        e->p[0] = v[0]; e->p[1] = v[1];
        if (cambio_pos) { e->p[2] = v[2]; e->p[3] = v[3]; }
        else if (gm_abs(v[4] - l0) > 1e-9 || gm_abs(v[5] - a0) > 1e-9) {     /* longitud o ángulo: el extremo final se recoloca */
            double a = v[5] * D2R;
            e->p[2] = e->p[0] + v[4] * gm_cos(a); e->p[3] = e->p[1] + v[4] * gm_sin(a);
        }
        break;
    }
    case E_CIRCULO: e->p[0] = v[0]; e->p[1] = v[1]; if (v[2] > PM_EPS) e->r = v[2]; break;
    case E_ARCO: e->p[0] = v[0]; e->p[1] = v[1]; if (v[2] > PM_EPS) e->r = v[2]; e->a0 = pm_angnorm(v[3] * D2R); e->a1 = pm_angnorm(v[4] * D2R); break;
    case E_POLI: e->cerrada = v[0] != 0; break;
    case E_TEXTO: e->p[0] = v[0]; e->p[1] = v[1]; if (v[2] > PM_EPS) e->alt = v[2]; e->giro = v[3] * D2R; break;
    case E_COTA: if (v[0] > PM_EPS) e->alt = v[0]; break;
    case E_SOMBRA: e->a0 = v[0] * D2R; if (v[1] > PM_EPS) e->r = v[1]; break;
    }
}

/* ============================================================ referencia a objetos */

typedef struct { pm_pz_t p; int ent; } pzc_t;

bool pm_ref_buscar(double x, double y, double ap, int mask, bool tiene_ult, double ux, double uy, pm_ref_t *r)
{
    /* Tres niveles, como en los CAD: 1) punto final, medio, cuadrante e intersección, por la distancia del cursor al punto;
     * 2) centro, perpendicular y tangente, que saltan al pasar por encima del objeto (la distancia es al objeto);
     * 3) punto cercano, si no hay otra cosa. */
    static pzc_t pc[160];
    int np = 0;
    double m1 = ap + 1e-12, x1 = 0, y1 = 0, m2 = ap + 1e-12, x2 = 0, y2 = 0, m3 = ap + 1e-12, x3 = 0, y3 = 0;
    int t1 = 0, t2 = 0;
    bool hay3 = false;
#define C1(tt, ax, ay) do { double d_ = gm_hypot((ax) - x, (ay) - y); if (d_ < m1) { m1 = d_; x1 = (ax); y1 = (ay); t1 = (tt); } } while (0)
#define C2(tt, ax, ay, dd) do { if ((dd) < m2) { m2 = (dd); x2 = (ax); y2 = (ay); t2 = (tt); } } while (0)
    for (int i = 0; i < pm_n && np < 150; i++) {
        const pm_ent_t *e = &pm_ent[i];
        if (!pm_ent_visible(e)) continue;
        double b0, b1, b2, b3;
        if (!pm_ent_bbox(e, &b0, &b1, &b2, &b3) || x < b0 - ap || x > b2 + ap || y < b1 - ap || y > b3 + ap) continue;
        if ((mask & REF_FIN) && (e->tipo == E_PUNTO || e->tipo == E_TEXTO)) C1(REF_FIN, e->p[0], e->p[1]);
        pm_pz_t pz[PM_VERT_MAX + 2];
        int n = pm_piezas(e, pz, PM_VERT_MAX + 2);
        for (int k = 0; k < n; k++) {
            const pm_pz_t *p = &pz[k];
            double qx, qy, d = pm_pz_dist(p, x, y, &qx, &qy);
            if (d > ap + 1e-9) continue;        /* solo las piezas bajo el cursor */
            if (np < 150) { pc[np].p = *p; pc[np].ent = i; np++; }
            bool completo = p->arco && p->a1 - p->a0 > 2 * PM_PI - 1e-9;
            if ((mask & REF_FIN) && !completo) { C1(REF_FIN, p->x0, p->y0); C1(REF_FIN, p->x1, p->y1); }
            if (mask & REF_MEDIO) {
                if (!p->arco) C1(REF_MEDIO, (p->x0 + p->x1) / 2, (p->y0 + p->y1) / 2);
                else if (!completo) { double a = (p->a0 + p->a1) / 2; C1(REF_MEDIO, p->cx + p->r * gm_cos(a), p->cy + p->r * gm_sin(a)); }
            }
            if (p->arco && (mask & REF_CUAD)) {
                for (int q = 0; q < 4; q++) {
                    double a = q * PM_PI / 2;
                    if (pm_ang_en(a, p->a0, p->a1)) C1(REF_CUAD, p->cx + p->r * gm_cos(a), p->cy + p->r * gm_sin(a));
                }
            }
            if (p->arco && (mask & REF_CENTRO)) C2(REF_CENTRO, p->cx, p->cy, d);
            if (tiene_ult && (mask & REF_PERP)) {
                if (!p->arco) {
                    double dx = p->x1 - p->x0, dy = p->y1 - p->y0, l2 = dx * dx + dy * dy;
                    if (l2 > PM_EPS) {
                        double t = ((ux - p->x0) * dx + (uy - p->y0) * dy) / l2;
                        if (t >= -1e-9 && t <= 1 + 1e-9) C2(REF_PERP, p->x0 + t * dx, p->y0 + t * dy, d);
                    }
                } else {
                    double a = pm_ang(ux - p->cx, uy - p->cy);
                    if (pm_ang_en(a, p->a0, p->a1)) C2(REF_PERP, p->cx + p->r * gm_cos(a), p->cy + p->r * gm_sin(a), d);
                }
            }
            if (tiene_ult && p->arco && (mask & REF_TANG)) {
                double dd = gm_hypot(ux - p->cx, uy - p->cy);
                if (dd > p->r + 1e-9) {
                    double phi = pm_ang(ux - p->cx, uy - p->cy), del = gm_acos(p->r / dd);
                    for (int s = -1; s <= 1; s += 2) {
                        double a = phi + s * del;
                        if (pm_ang_en(pm_angnorm(a), p->a0, p->a1)) {
                            double tx = p->cx + p->r * gm_cos(a), ty = p->cy + p->r * gm_sin(a);
                            C2(REF_TANG, tx, ty, gm_hypot(tx - x, ty - y) < d + 1e9 ? gm_hypot(tx - x, ty - y) : d);
                        }
                    }
                }
            }
            if ((mask & REF_CERCA) && d < m3) { m3 = d; x3 = qx; y3 = qy; hay3 = true; }
        }
    }
    if (mask & REF_INTER) {
        for (int a = 0; a < np; a++)
            for (int b = a + 1; b < np; b++) {
                if (pc[a].ent == pc[b].ent) continue;
                double o[2][2];
                int k = pm_pz_inter(&pc[a].p, &pc[b].p, false, false, o, 2);
                for (int q = 0; q < k; q++) C1(REF_INTER, o[q][0], o[q][1]);
            }
    }
#undef C1
#undef C2
    if (t1) { r->tipo = t1; r->x = x1; r->y = y1; return true; }
    if (t2) { r->tipo = t2; r->x = x2; r->y = y2; return true; }
    if (hay3) { r->tipo = REF_CERCA; r->x = x3; r->y = y3; return true; }
    return false;
}

/* ============================================================ utilidades de modificación */

static int agregar_como(const pm_ent_t *src, pm_ent_t *e)
{
    e->capa = src->capa; e->color = src->color; e->ltipo = src->ltipo;
    int r = pm_anadir(e);
    pm_ent_liberar(e);
    return r;
}

static int bordes_piezas(int idx, const int *bordes, int nb, pm_pz_t *out, int max)
{
    int n = 0;
    if (nb < 0) {
        for (int i = 0; i < pm_n; i++) {
            if (i == idx || !pm_ent_visible(&pm_ent[i]) || (nb == -2 && !pm_ent[i].sel)) continue;
            n += pm_piezas(&pm_ent[i], out + n, max - n < PM_VERT_MAX + 2 ? max - n : PM_VERT_MAX + 2);
            if (n >= max - PM_VERT_MAX - 2) break;
        }
    } else {
        for (int k = 0; k < nb; k++) {
            int i = bordes[k];
            if (i == idx || i < 0 || i >= pm_n) continue;
            n += pm_piezas(&pm_ent[i], out + n, max - n < PM_VERT_MAX + 2 ? max - n : PM_VERT_MAX + 2);
            if (n >= max - PM_VERT_MAX - 2) break;
        }
    }
    return n;
}

/* ============================================================ explotar */

typedef struct { const pm_ent_t *src; } exp_t;
static void x_seg(void *c, double x0, double y0, double x1, double y1)
{
    pm_ent_t e;
    pm_ent_nueva(&e, E_LINEA);
    e.p[0] = x0; e.p[1] = y0; e.p[2] = x1; e.p[3] = y1;
    if (gm_hypot(x1 - x0, y1 - y0) < 1e-9) return;
    agregar_como(((exp_t *)c)->src, &e);
}
static void x_tri(void *c, double a, double b, double d, double f, double g, double h)
{
    pm_ent_t e;
    pm_ent_nueva(&e, E_SOMBRA);
    pm_poli_vert(&e, a, b, 0); pm_poli_vert(&e, d, f, 0); pm_poli_vert(&e, g, h, 0);
    e.sub = 1; e.cerrada = 1;
    agregar_como(((exp_t *)c)->src, &e);
}
static void x_txt(void *c, double x, double y, double alt, double giro, const char *t, int cen)
{
    pm_ent_t e;
    pm_ent_nueva(&e, E_TEXTO);
    double w = pm_txt_ancho(alt, t);
    e.p[0] = cen ? x - w / 2 * gm_cos(giro) : x; e.p[1] = cen ? y - w / 2 * gm_sin(giro) : y;
    e.alt = alt; e.giro = giro;
    copia_texto(e.txt, t, sizeof e.txt);
    agregar_como(((exp_t *)c)->src, &e);
}

int pm_explotar(int idx)
{
    if (idx < 0 || idx >= pm_n) return fallo("Entidad inexistente");
    pm_ent_t src;
    pm_ent_clonar(&src, &pm_ent[idx]);
    int antes = pm_n;
    int rc = 0;
    if (src.tipo == E_POLI) {
        pm_pz_t pz[PM_VERT_MAX + 2];
        int n = pm_piezas(&src, pz, PM_VERT_MAX + 2);
        for (int i = 0; i < n; i++) {
            pm_ent_t e;
            if (!pz[i].arco) { pm_ent_nueva(&e, E_LINEA); e.p[0] = pz[i].x0; e.p[1] = pz[i].y0; e.p[2] = pz[i].x1; e.p[3] = pz[i].y1; }
            else { pm_ent_nueva(&e, E_ARCO); e.p[0] = pz[i].cx; e.p[1] = pz[i].cy; e.r = pz[i].r; e.a0 = pz[i].a0; e.a1 = pm_angnorm(pz[i].a1); }
            agregar_como(&src, &e);
        }
    } else if (src.tipo == E_COTA || src.tipo == E_SOMBRA) {
        exp_t x = { &src };
        pm_sink_t s = { x_seg, x_tri, NULL, x_txt, &x, 0.01 };
        if (src.tipo == E_SOMBRA && src.sub == 1) x_tri(&x, src.v[0].x, src.v[0].y, src.nv > 1 ? src.v[1].x : 0, src.nv > 1 ? src.v[1].y : 0, src.nv > 2 ? src.v[2].x : 0, src.nv > 2 ? src.v[2].y : 0);
        else pm_dibujar_ent(&src, &s);
    } else rc = fallo("Esta entidad no se puede explotar");
    pm_ent_liberar(&src);
    if (rc) return rc;
    int nuevos = pm_n - antes;
    pm_borrar(idx);
    pm_explo_n = nuevos;
    return antes - 1;           /* primer índice de las partes (la original ya no está) */
}

/* ============================================================ recortar y alargar */

typedef struct { double u; } par_t;

static int ordenar_par(double *u, int n) { for (int a = 1; a < n; a++) { double v = u[a]; int b = a - 1; while (b >= 0 && u[b] > v) { u[b + 1] = u[b]; b--; } u[b + 1] = v; } return n; }

int pm_recortar(int idx, double px, double py, const int *bordes, int nb)
{
    if (idx < 0 || idx >= pm_n) return fallo("Entidad inexistente");
    pm_ent_t *e = &pm_ent[idx];
    if (!pm_ent_editable(e)) return fallo("La capa está bloqueada");
    if (e->tipo == E_POLI) {
        int primero = pm_explotar(idx);
        if (primero < 0) return -1;
        int mejor = -1;
        double dm = 1e300;
        for (int i = primero; i < pm_n; i++) { double d = pm_dist_ent(&pm_ent[i], px, py); if (d < dm) { dm = d; mejor = i; } }
        if (mejor < 0) return fallo("No hay nada que recortar");
        return pm_recortar(mejor, px, py, NULL, -1);
    }
    if (e->tipo != E_LINEA && e->tipo != E_ARCO && e->tipo != E_CIRCULO) return fallo("Solo se recortan líneas, arcos, círculos y polilíneas");
    static pm_pz_t ed[600];
    int ne = bordes_piezas(idx, bordes, nb, ed, 600);
    pm_pz_t t;
    pm_piezas(e, &t, 1);
    double u[200];
    int nu = 0;
    for (int i = 0; i < ne && nu < 190; i++) {
        double o[2][2];
        int k = pm_pz_inter(&t, &ed[i], false, false, o, 2);
        for (int q = 0; q < k; q++) {
            double v;
            if (!t.arco) v = pm_pz_param(&t, o[q][0], o[q][1]);
            else { v = pm_angnorm(pm_pz_param(&t, o[q][0], o[q][1]) - t.a0); if (v > 2 * PM_PI - 1e-9) v = 0; }
            u[nu++] = v;
        }
    }
    ordenar_par(u, nu);
    if (nu == 0) return fallo("No hay bordes de corte que crucen el objeto");
    double L = t.arco ? t.a1 - t.a0 : 1.0, tp;
    {
        double qx, qy;
        pm_pz_dist(&t, px, py, &qx, &qy);
        if (!t.arco) { tp = pm_pz_param(&t, qx, qy); tp = tp < 0 ? 0 : tp > 1 ? 1 : tp; }
        else { tp = pm_angnorm(pm_pz_param(&t, qx, qy) - t.a0); if (tp > L) tp = L; }
    }
    double eps = t.arco ? 1e-9 : 1e-9, lo = -1e300, hi = 1e300;
    for (int i = 0; i < nu; i++) { if (u[i] < tp - eps && u[i] > lo) lo = u[i]; if (u[i] > tp + eps && u[i] < hi) hi = u[i]; }
    if (e->tipo == E_CIRCULO) {
        /* hacen falta dos cortes distintos: se quita el arco que contiene el punto */
        int distintos = 0;
        for (int i = 0; i < nu; i++) if (i == 0 || u[i] - u[i - 1] > 1e-9) distintos++;
        if (distintos < 2) return fallo("Un círculo necesita dos cortes para recortarse");
        double l2 = lo > -1e299 ? lo : u[nu - 1] - 2 * PM_PI, h2 = hi < 1e299 ? hi : u[0] + 2 * PM_PI;
        e->tipo = E_ARCO;
        e->a0 = pm_angnorm(h2); e->a1 = pm_angnorm(l2 + 2 * PM_PI);
        return 0;
    }
    bool hay_lo = lo > -1e299 && lo > eps * 10, hay_hi = hi < 1e299 && hi < L - eps * 10;
    bool lo_ex = lo > -1e299, hi_ex = hi < 1e299;
    if (!lo_ex && !hi_ex) return fallo("No hay bordes de corte a los lados");
    /* tramos que quedan: [0, lo] y [hi, L] */
    pm_ent_t copia;
    pm_ent_clonar(&copia, e);
    bool queda_a = lo_ex && hay_lo, queda_b = hi_ex && hay_hi;
    if (!queda_a && !queda_b) { pm_borrar(idx); return 0; }
    if (e->tipo == E_LINEA) {
        double x0 = e->p[0], y0 = e->p[1], dx = e->p[2] - x0, dy = e->p[3] - y0;
        if (queda_a && queda_b) {
            e->p[2] = x0 + dx * lo; e->p[3] = y0 + dy * lo;
            copia.p[0] = x0 + dx * hi; copia.p[1] = y0 + dy * hi;
            pm_anadir(&copia);
        } else if (queda_a) { e->p[2] = x0 + dx * lo; e->p[3] = y0 + dy * lo; }
        else { e->p[0] = x0 + dx * hi; e->p[1] = y0 + dy * hi; }
    } else {
        double a0 = t.a0;
        if (queda_a && queda_b) {
            e->a0 = pm_angnorm(a0); e->a1 = pm_angnorm(a0 + lo);
            copia.a0 = pm_angnorm(a0 + hi); copia.a1 = pm_angnorm(a0 + L);
            pm_anadir(&copia);
        } else if (queda_a) { e->a0 = pm_angnorm(a0); e->a1 = pm_angnorm(a0 + lo); }
        else { e->a0 = pm_angnorm(a0 + hi); e->a1 = pm_angnorm(a0 + L); }
    }
    pm_ent_liberar(&copia);
    return 0;
}

int pm_extender(int idx, double px, double py, const int *bordes, int nb)
{
    if (idx < 0 || idx >= pm_n) return fallo("Entidad inexistente");
    pm_ent_t *e = &pm_ent[idx];
    if (!pm_ent_editable(e)) return fallo("La capa está bloqueada");
    if (e->tipo != E_LINEA && e->tipo != E_ARCO) return fallo("Solo se alargan líneas y arcos");
    static pm_pz_t ed[600];
    int ne = bordes_piezas(idx, bordes, nb, ed, 600);
    pm_pz_t t;
    pm_piezas(e, &t, 1);
    if (!t.arco) {
        double x0 = e->p[0], y0 = e->p[1], dx = e->p[2] - x0, dy = e->p[3] - y0;
        double tp = pm_pz_param(&t, px, py);
        bool fin = tp > 0.5;
        double mejor = fin ? 1e300 : -1e300;
        for (int i = 0; i < ne; i++) {
            double o[2][2];
            int k = pm_pz_inter(&t, &ed[i], true, false, o, 2);
            for (int q = 0; q < k; q++) {
                double v = pm_pz_param(&t, o[q][0], o[q][1]);
                if (fin && v > 1 + 1e-9 && v < mejor) mejor = v;
                if (!fin && v < -1e-9 && v > mejor) mejor = v;
            }
        }
        if (mejor > 1e299 || mejor < -1e299) return fallo("No hay borde al que alargar");
        if (fin) { e->p[2] = x0 + dx * mejor; e->p[3] = y0 + dy * mejor; } else { e->p[0] = x0 + dx * mejor; e->p[1] = y0 + dy * mejor; }
        return 0;
    }
    double sw = t.a1 - t.a0;
    double d0 = gm_hypot(px - t.x0, py - t.y0), d1 = gm_hypot(px - t.x1, py - t.y1);
    bool fin = d1 < d0;
    double mejor = 1e300;
    for (int i = 0; i < ne; i++) {
        double o[2][2];
        int k = pm_pz_inter(&t, &ed[i], true, false, o, 2);
        for (int q = 0; q < k; q++) {
            double a = pm_ang(o[q][0] - t.cx, o[q][1] - t.cy);
            double dl = fin ? pm_angnorm(a - t.a1) : pm_angnorm(t.a0 - a);
            if (dl > 1e-9 && dl < mejor) mejor = dl;
        }
    }
    if (mejor > 1e299 || sw + mejor > 2 * PM_PI - 1e-9) return fallo("No hay borde al que alargar");
    if (fin) e->a1 = pm_angnorm(t.a1 + mejor); else e->a0 = pm_angnorm(t.a0 - mejor);
    return 0;
}

/* ============================================================ equidistancia */

static void pz_ini(const pm_pz_t *p, double *x, double *y) { if (p->inv) { *x = p->x1; *y = p->y1; } else { *x = p->x0; *y = p->y0; } }
static void pz_fin(const pm_pz_t *p, double *x, double *y) { if (p->inv) { *x = p->x0; *y = p->y0; } else { *x = p->x1; *y = p->y1; } }

/* La pieza desplazada "off" a la izquierda del recorrido (negativo = derecha). */
static bool pz_desplaza(const pm_pz_t *p, double off, pm_pz_t *o)
{
    *o = *p;
    if (!p->arco) {
        double dx = p->x1 - p->x0, dy = p->y1 - p->y0, l = gm_hypot(dx, dy);
        if (l < PM_EPS) return false;
        double nx = -dy / l * off, ny = dx / l * off;
        o->x0 += nx; o->y0 += ny; o->x1 += nx; o->y1 += ny;
        return true;
    }
    double r = p->inv ? p->r + off : p->r - off;
    if (r < 1e-9) return false;
    o->r = r;
    o->x0 = p->cx + r * gm_cos(p->a0); o->y0 = p->cy + r * gm_sin(p->a0);
    o->x1 = p->cx + r * gm_cos(p->a1); o->y1 = p->cy + r * gm_sin(p->a1);
    return true;
}

int pm_equidistancia(int idx, double d, double sx, double sy)
{
    if (idx < 0 || idx >= pm_n) return fallo("Entidad inexistente");
    if (d <= PM_EPS) return fallo("La distancia debe ser positiva");
    const pm_ent_t *e = &pm_ent[idx];
    pm_ent_t n;
    pm_ent_clonar(&n, e);
    switch (e->tipo) {
    case E_LINEA: {
        double dx = e->p[2] - e->p[0], dy = e->p[3] - e->p[1], l = gm_hypot(dx, dy);
        if (l < PM_EPS) { pm_ent_liberar(&n); return fallo("Línea de longitud cero"); }
        double nx = -dy / l, ny = dx / l, side = (sx - e->p[0]) * nx + (sy - e->p[1]) * ny, s = side >= 0 ? 1 : -1;
        n.p[0] += s * d * nx; n.p[1] += s * d * ny; n.p[2] += s * d * nx; n.p[3] += s * d * ny;
        break;
    }
    case E_CIRCULO: case E_ARCO: {
        bool fuera = gm_hypot(sx - e->p[0], sy - e->p[1]) > e->r;
        double r = fuera ? e->r + d : e->r - d;
        if (r < 1e-9) { pm_ent_liberar(&n); return fallo("El radio resultante no es positivo"); }
        n.r = r;
        break;
    }
    case E_POLI: {
        pm_pz_t pz[PM_VERT_MAX + 2], op[PM_VERT_MAX + 2];
        int np = pm_piezas(e, pz, PM_VERT_MAX + 2);
        if (np < 1) { pm_ent_liberar(&n); return fallo("Polilínea vacía"); }
        int k = 0;
        double dm = 1e300;
        for (int i = 0; i < np; i++) { double q = pm_pz_dist(&pz[i], sx, sy, NULL, NULL); if (q < dm) { dm = q; k = i; } }
        bool izq;
        if (!pz[k].arco) izq = (pz[k].x1 - pz[k].x0) * (sy - pz[k].y0) - (pz[k].y1 - pz[k].y0) * (sx - pz[k].x0) > 0;
        else { bool dentro = gm_hypot(sx - pz[k].cx, sy - pz[k].cy) < pz[k].r; izq = dentro != (pz[k].inv != 0); }
        double off = izq ? d : -d;
        for (int i = 0; i < np; i++) if (!pz_desplaza(&pz[i], off, &op[i])) { pm_ent_liberar(&n); return fallo("No se puede desplazar esa distancia"); }
        int nv = e->nv;
        double vx[PM_VERT_MAX + 2], vy[PM_VERT_MAX + 2];
        for (int i = 0; i < nv; i++) {
            double ox, oy;
            if (!e->cerrada && i == 0) { pz_ini(&op[0], &vx[0], &vy[0]); continue; }
            if (!e->cerrada && i == nv - 1) { pz_fin(&op[np - 1], &vx[i], &vy[i]); continue; }
            int a = (i - 1 + np) % np, b = i % np;
            double o[2][2];
            int c = pm_pz_inter(&op[a], &op[b], true, true, o, 2);
            pz_fin(&op[a], &ox, &oy);
            if (c > 0) {
                double mj = 1e300;
                for (int q = 0; q < c; q++) { double dd = gm_hypot(o[q][0] - e->v[i].x, o[q][1] - e->v[i].y); if (dd < mj) { mj = dd; ox = o[q][0]; oy = o[q][1]; } }
            }
            vx[i] = ox; vy[i] = oy;
        }
        for (int i = 0; i < nv; i++) {
            n.v[i].x = vx[i]; n.v[i].y = vy[i];
            n.v[i].b = 0;
            if (i < np && op[i].arco) {
                double as = pm_ang(vx[i] - op[i].cx, vy[i] - op[i].cy), ae = pm_ang(vx[(i + 1) % nv] - op[i].cx, vy[(i + 1) % nv] - op[i].cy);
                n.v[i].b = pm_bulge_de(as, ae, op[i].inv != 0);
            }
        }
        break;
    }
    default: pm_ent_liberar(&n); return fallo("Esta entidad no admite equidistancia");
    }
    n.sel = 0;
    int r = pm_anadir(&n);
    pm_ent_liberar(&n);
    return r;
}

/* ============================================================ empalme y chaflán */

typedef struct { double px, py, u1x, u1y, u2x, u2y; int e1, e2; double far1x, far1y, far2x, far2y; } esq_t;

static int esquina(int i1, double p1x, double p1y, int i2, double p2x, double p2y, esq_t *q)
{
    if (i1 < 0 || i2 < 0 || i1 >= pm_n || i2 >= pm_n) return fallo("Entidad inexistente");
    if (i1 == i2) return fallo("Hay que elegir dos líneas distintas");
    pm_ent_t *a = &pm_ent[i1], *b = &pm_ent[i2];
    if (a->tipo != E_LINEA || b->tipo != E_LINEA) return fallo("Solo se empalma entre dos líneas");
    if (!pm_ent_editable(a) || !pm_ent_editable(b)) return fallo("La capa está bloqueada");
    pm_pz_t pa, pb;
    pm_piezas(a, &pa, 1); pm_piezas(b, &pb, 1);
    double o[1][2];
    if (pm_pz_inter(&pa, &pb, true, true, o, 1) < 1) return fallo("Las líneas son paralelas");
    q->px = o[0][0]; q->py = o[0][1]; q->e1 = i1; q->e2 = i2;
    for (int k = 0; k < 2; k++) {
        const pm_ent_t *e = k ? b : a;
        double qx = k ? p2x : p1x, qy = k ? p2y : p1y;
        double dx = e->p[2] - e->p[0], dy = e->p[3] - e->p[1], l = gm_hypot(dx, dy);
        dx /= l; dy /= l;
        double s = ((qx - q->px) * dx + (qy - q->py) * dy) >= 0 ? 1 : -1;
        double ux = dx * s, uy = dy * s;
        double ta = (e->p[0] - q->px) * ux + (e->p[1] - q->py) * uy, tb = (e->p[2] - q->px) * ux + (e->p[3] - q->py) * uy;
        double fx = ta > tb ? e->p[0] : e->p[2], fy = ta > tb ? e->p[1] : e->p[3];
        if (k) { q->u2x = ux; q->u2y = uy; q->far2x = fx; q->far2y = fy; } else { q->u1x = ux; q->u1y = uy; q->far1x = fx; q->far1y = fy; }
    }
    return 0;
}

static void fijar_extremo(pm_ent_t *e, double fx, double fy, double tx, double ty)
{
    e->p[0] = fx; e->p[1] = fy; e->p[2] = tx; e->p[3] = ty;
}

int pm_empalme(int i1, double p1x, double p1y, int i2, double p2x, double p2y, double r)
{
    esq_t q;
    if (esquina(i1, p1x, p1y, i2, p2x, p2y, &q)) return -1;
    double c = q.u1x * q.u2x + q.u1y * q.u2y, th = gm_acos(c < -1 ? -1 : c > 1 ? 1 : c);
    if (th < 1e-6 || th > PM_PI - 1e-6) return fallo("Las líneas son paralelas");
    double tl = r <= 0 ? 0 : r / gm_tan(th / 2);
    double l1 = (q.far1x - q.px) * q.u1x + (q.far1y - q.py) * q.u1y, l2 = (q.far2x - q.px) * q.u2x + (q.far2y - q.py) * q.u2y;
    if (tl > l1 - 1e-9 || tl > l2 - 1e-9) return fallo("El radio es demasiado grande para esas líneas");
    double t1x = q.px + q.u1x * tl, t1y = q.py + q.u1y * tl, t2x = q.px + q.u2x * tl, t2y = q.py + q.u2y * tl;
    pm_ent_t *a = &pm_ent[q.e1], *b = &pm_ent[q.e2];
    pm_ent_t arco;
    bool con_arco = r > 0;
    if (con_arco) {
        double bx = q.u1x + q.u2x, by = q.u1y + q.u2y, bl = gm_hypot(bx, by);
        double dc = r / gm_sin(th / 2), cx = q.px + bx / bl * dc, cy = q.py + by / bl * dc;
        pm_ent_nueva(&arco, E_ARCO);
        double cr = (t1x - cx) * (t2y - cy) - (t1y - cy) * (t2x - cx);
        double a1 = pm_ang(t1x - cx, t1y - cy), a2 = pm_ang(t2x - cx, t2y - cy);
        arco.p[0] = cx; arco.p[1] = cy; arco.r = r;
        if (cr > 0) { arco.a0 = a1; arco.a1 = a2; } else { arco.a0 = a2; arco.a1 = a1; }
        arco.capa = a->capa; arco.color = a->color; arco.ltipo = a->ltipo;
    }
    fijar_extremo(a, q.far1x, q.far1y, t1x, t1y);
    fijar_extremo(b, q.far2x, q.far2y, t2x, t2y);
    if (con_arco) pm_anadir(&arco);
    return 0;
}

int pm_chaflan(int i1, double p1x, double p1y, int i2, double p2x, double p2y, double d1, double d2)
{
    esq_t q;
    if (esquina(i1, p1x, p1y, i2, p2x, p2y, &q)) return -1;
    double l1 = (q.far1x - q.px) * q.u1x + (q.far1y - q.py) * q.u1y, l2 = (q.far2x - q.px) * q.u2x + (q.far2y - q.py) * q.u2y;
    if (d1 > l1 - 1e-9 || d2 > l2 - 1e-9) return fallo("Las distancias son demasiado grandes para esas líneas");
    double t1x = q.px + q.u1x * d1, t1y = q.py + q.u1y * d1, t2x = q.px + q.u2x * d2, t2y = q.py + q.u2y * d2;
    pm_ent_t *a = &pm_ent[q.e1], *b = &pm_ent[q.e2];
    pm_ent_t l;
    pm_ent_nueva(&l, E_LINEA);
    l.p[0] = t1x; l.p[1] = t1y; l.p[2] = t2x; l.p[3] = t2y;
    l.capa = a->capa; l.color = a->color; l.ltipo = a->ltipo;
    fijar_extremo(a, q.far1x, q.far1y, t1x, t1y);
    fijar_extremo(b, q.far2x, q.far2y, t2x, t2y);
    if (d1 > 0 || d2 > 0) pm_anadir(&l);
    return 0;
}

int pm_empalme_poli(int idx, double r)
{
    if (idx < 0 || idx >= pm_n || pm_ent[idx].tipo != E_POLI) return fallo("Hace falta una polilínea");
    pm_ent_t *e = &pm_ent[idx];
    if (r <= 0) return fallo("El radio debe ser positivo");
    int nv = e->nv;
    pm_vert_t out[PM_VERT_MAX + 2];
    int no = 0, hechos = 0;
    for (int i = 0; i < nv; i++) {
        bool interior = e->cerrada || (i > 0 && i < nv - 1);
        int ia = (i - 1 + nv) % nv, ib = (i + 1) % nv;
        bool rectos = e->v[ia].b == 0 && e->v[i].b == 0;
        if (!interior || !rectos || no + 2 >= PM_VERT_MAX) { out[no++] = e->v[i]; continue; }
        double ax = e->v[ia].x - e->v[i].x, ay = e->v[ia].y - e->v[i].y, bx = e->v[ib].x - e->v[i].x, by = e->v[ib].y - e->v[i].y;
        double la = gm_hypot(ax, ay), lb = gm_hypot(bx, by);
        if (la < PM_EPS || lb < PM_EPS) { out[no++] = e->v[i]; continue; }
        ax /= la; ay /= la; bx /= lb; by /= lb;
        double c = ax * bx + ay * by, th = gm_acos(c < -1 ? -1 : c > 1 ? 1 : c);
        if (th < 1e-6 || th > PM_PI - 1e-6) { out[no++] = e->v[i]; continue; }
        double tl = r / gm_tan(th / 2);
        if (tl > la * 0.5 + 1e-9 || tl > lb * 0.5 + 1e-9) { out[no++] = e->v[i]; continue; }
        double t1x = e->v[i].x + ax * tl, t1y = e->v[i].y + ay * tl, t2x = e->v[i].x + bx * tl, t2y = e->v[i].y + by * tl;
        double cr = ax * by - ay * bx;      /* giro del recorrido: >0 antihorario... el arco va de T1 a T2 */
        double sw = PM_PI - th, b = gm_tan(sw / 4);
        /* el recorrido llega por la línea A (hacia el vértice) y sale por la B: el arco gira como la esquina */
        out[no].x = t1x; out[no].y = t1y; out[no].b = cr < 0 ? b : -b; no++;
        out[no].x = t2x; out[no].y = t2y; out[no].b = e->v[i].b; no++;
        hechos++;
    }
    if (!hechos) return fallo("Ninguna esquina admite ese radio");
    pm_vert_t *nuevo = aloca((size_t)no * sizeof(pm_vert_t));
    if (!nuevo) return fallo("Sin memoria");
    memcpy(nuevo, out, (size_t)no * sizeof(pm_vert_t));
    libera(e->v);
    e->v = nuevo; e->nv = no;
    return hechos;
}

/* ============================================================ sombreado */

int pm_contorno_en(double x, double y)
{
    static double xs[800], ys[800];
    int mejor = -1;
    double am = 1e300;
    for (int i = 0; i < pm_n; i++) {
        const pm_ent_t *e = &pm_ent[i];
        if (!pm_ent_editable(e) || !((e->tipo == E_POLI && e->cerrada) || e->tipo == E_CIRCULO)) continue;
        int n = pm_aplanar(e, xs, ys, 800);
        if (e->tipo == E_CIRCULO) { n = 0; for (int k = 0; k < 64; k++) { xs[n] = e->p[0] + e->r * gm_cos(k * PM_PI / 32); ys[n] = e->p[1] + e->r * gm_sin(k * PM_PI / 32); n++; } }
        bool dentro = false;
        for (int a = 0, b = n - 1; a < n; b = a++)
            if (((ys[a] > y) != (ys[b] > y)) && (x < (xs[b] - xs[a]) * (y - ys[a]) / (ys[b] - ys[a]) + xs[a])) dentro = !dentro;
        if (!dentro) continue;
        double x0, y0, x1, y1;
        pm_ent_bbox(e, &x0, &y0, &x1, &y1);
        double area = (x1 - x0) * (y1 - y0);
        if (area < am) { am = area; mejor = i; }
    }
    return mejor;
}

int pm_sombra_de(int idx, int sub, double ang, double esp)
{
    if (idx < 0 || idx >= pm_n) return fallo("Entidad inexistente");
    const pm_ent_t *e = &pm_ent[idx];
    pm_ent_t s;
    pm_ent_nueva(&s, E_SOMBRA);
    if (e->tipo == E_CIRCULO) {
        pm_poli_vert(&s, e->p[0] + e->r, e->p[1], 1.0);
        pm_poli_vert(&s, e->p[0] - e->r, e->p[1], 1.0);
    } else if (e->tipo == E_POLI && e->cerrada) {
        for (int i = 0; i < e->nv; i++) pm_poli_vert(&s, e->v[i].x, e->v[i].y, e->v[i].b);
    } else { pm_ent_liberar(&s); return fallo("El contorno debe ser una polilínea cerrada o un círculo"); }
    s.cerrada = 1; s.sub = (uint8_t)sub; s.a0 = ang; s.r = esp > 0 ? esp : 1.0;
    return pm_anadir_actual(&s);
}
