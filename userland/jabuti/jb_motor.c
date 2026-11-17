// SPDX-License-Identifier: GPL-3.0-or-later
/* jb_motor.c - el intérprete de Jabuti (D377).
 *
 * Cada guion que arranca es un «hilo verde»: un puntero al bloque actual y una pila de marcos para
 * los bloques en C. En cada cuadro (1/30 s) cada hilo avanza hasta que cede el turno: al final de una
 * vuelta de un bucle o en un bloque que espera. Así varios personajes corren a la vez sin hilos reales,
 * en un Pentium II. Todo en enteros con tres decimales (U = 1000). */
#include "jb.h"

int jb_raton_x, jb_raton_y;
bool jb_raton_pulsado;
unsigned char jb_teclas_agarradas[32];
bool jb_hay_pregunta;
char jb_pregunta[96], jb_respuesta[64];
bool jb_sucio = true;
char jb_aviso[160];
bool jb_log;
const char *jb_auto_resp;

typedef struct { blk_t *b; int32_t cnt; } marco_t;
typedef struct {
    bool vivo;
    pers_t *p;
    int gi;
    blk_t *top, *pc;
    marco_t m[MAX_PROF];
    int prof;
    int fase;
    unsigned t0, dur;
    int32_t ax, ay, bx, by;
} hilo_t;
static hilo_t hilos[MAX_HILOS];
static unsigned ahora, crono0;
static void *preg_dueno;
static unsigned rnd_estado = 2463534242u;

typedef struct { uint8_t k; int32_t n; char t[48]; } val_t;   /* k: 0 número, 1 texto, 2 verdadero/falso */

static void v_num(val_t *v, int32_t n) { v->k = 0; v->n = n; v->t[0] = 0; }
static void v_txt(val_t *v, const char *s) { v->k = 1; v->n = 0; copia_texto(v->t, s, sizeof v->t); }
static void v_bool(val_t *v, bool b) { v->k = 2; v->n = b ? U : 0; v->t[0] = 0; }
static int32_t a_num(const val_t *v)
{
    if (v->k != 1) return v->n;
    int32_t x;
    return jb_num_parse(v->t, &x) ? x : 0;
}
static void a_txt(const val_t *v, char *s, int max)
{
    if (v->k == 1) copia_texto(s, v->t, (size_t)max);
    else if (v->k == 2) copia_texto(s, v->n ? "verdadero" : "falso", (size_t)max);
    else jb_num_texto(v->n, s, max);
}
static bool a_bool(const val_t *v)
{
    if (v->k == 1) return compara_texto(v->t, "verdadero") == 0 || compara_texto(v->t, "true") == 0;
    return v->n != 0;
}
static int32_t sat(int64_t x) { return x > 2147483000LL ? 2147483000 : x < -2147483000LL ? -2147483000 : (int32_t)x; }

static unsigned azar32(void) { rnd_estado ^= rnd_estado << 13; rnd_estado ^= rnd_estado >> 17; rnd_estado ^= rnd_estado << 5; return rnd_estado; }
static uint64_t raiz64(uint64_t n)
{
    uint64_t r = 0, bit = 1ULL << 62;
    while (bit > n) bit >>= 2;
    while (bit) {
        if (n >= r + bit) { n -= r + bit; r = (r >> 1) + bit; } else r >>= 1;
        bit >>= 2;
    }
    return r;
}

static void aviso(const char *fmt, const char *a)
{
    formatar_texto(jb_aviso, sizeof jb_aviso, idioma_traducir(fmt), a);
    if (jb_log) registrar("jabuti: aviso %s\n", jb_aviso);
}

/* ---- pequeñas ayudas de personajes ---- */
void jb_posicion(const pers_t *p, int *x, int *y) { *x = p->x / U; *y = p->y / U; }
static pers_t *buscar_pers(const char *n)
{
    for (int i = 0; i < jb_p.npers; i++) if (compara_texto(jb_p.pers[i]->nombre, n) == 0) return jb_p.pers[i];
    return NULL;
}
static int dir_norm(int d)
{
    d %= 360000;
    if (d > 180000) d -= 360000;
    if (d <= -180000) d += 360000;
    return d;
}
static void poner_pos(pers_t *p, int32_t x, int32_t y)
{
    if (x > 600 * U) x = 600 * U;
    if (x < -600 * U) x = -600 * U;
    if (y > 600 * U) y = 600 * U;
    if (y < -600 * U) y = -600 * U;
    if (p->lapiz && (x != p->x || y != p->y)) jb_lapiz_linea(p->x / U, p->y / U, x / U, y / U, p->lapiz_color, p->lapiz_grosor);
    p->x = x; p->y = y;
    jb_sucio = true;
}
static void mover_pasos(pers_t *p, int32_t pasos)
{
    int64_t dx = (int64_t)pasos * jb_seno(p->dir) / 10000, dy = (int64_t)pasos * jb_coseno(p->dir) / 10000;
    poner_pos(p, sat(p->x + dx), sat(p->y + dy));
}
/* ángulo (en milésimas de grado, 0 = arriba, horario) del vector (dx, dy) */
static int angulo_de(int64_t dx, int64_t dy)
{
    if (dx == 0 && dy == 0) return 90 * U;
    int64_t ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy;
    /* a en [0, 90000] = ángulo desde «arriba» hacia el eje x, con tan(a) = ax / ay */
    int lo = 0, hi = 90000;
    while (hi - lo > 50) {
        int mid = (lo + hi) / 2;
        if (ax * jb_coseno(mid) > ay * jb_seno(mid)) lo = mid; else hi = mid;
    }
    int a = (lo + hi) / 2;
    if (dx >= 0 && dy >= 0) return a;
    if (dx >= 0 && dy < 0) return 180000 - a;
    if (dx < 0 && dy < 0) return 180000 + a;
    return 360000 - a;
}
static int tam_px(const pers_t *p) { return TAM_BASE * p->tam / 100; }   /* lado lógico del traje */

void jb_pers_caja(const pers_t *p, int *x0, int *y0, int *x1, int *y1)
{
    int lado = tam_px(p);
    int cx = p->x / U, cy = p->y / U;
    *x0 = cx + (p->sx0 - 16) * lado / 32; *x1 = cx + (p->sx1 + 1 - 16) * lado / 32;
    *y1 = cy + (16 - p->sy0) * lado / 32; *y0 = cy + (16 - p->sy1 - 1) * lado / 32;
}
bool jb_tocando_borde(const pers_t *p)
{
    int x0, y0, x1, y1;
    jb_pers_caja(p, &x0, &y0, &x1, &y1);
    return x0 <= -ESC_L / 2 || x1 >= ESC_L / 2 || y0 <= -ESC_A / 2 || y1 >= ESC_A / 2;
}
static void rebotar(pers_t *p)
{
    int x0, y0, x1, y1;
    jb_pers_caja(p, &x0, &y0, &x1, &y1);
    int hx = 0, hy = 0;
    if (x0 < -ESC_L / 2) { p->x += (-ESC_L / 2 - x0) * U; hx = 1; }
    else if (x1 > ESC_L / 2) { p->x -= (x1 - ESC_L / 2) * U; hx = 1; }
    if (y0 < -ESC_A / 2) { p->y += (-ESC_A / 2 - y0) * U; hy = 1; }
    else if (y1 > ESC_A / 2) { p->y -= (y1 - ESC_A / 2) * U; hy = 1; }
    if (hx) p->dir = dir_norm(-p->dir);
    if (hy) p->dir = dir_norm(180000 - p->dir);
    if (hx || hy) jb_sucio = true;
}

/* ---- sonido ---- */
static const short nota_base[12] = { 262, 277, 294, 311, 330, 349, 370, 392, 415, 440, 466, 494 };
static int freq_nota(int n)
{
    if (n < 0) n = 0;
    if (n > 127) n = 127;
    int f = nota_base[n % 12], o = n / 12 - 5;
    if (o > 0) f <<= o; else if (o < 0) f >>= -o;
    return f;
}
static void sonido_nombre(const char *n)
{
    nota_t v[6]; int k = 0;
    if (compara_texto(n, "salto") == 0) { v[k++] = (nota_t){ 330, 60 }; v[k++] = (nota_t){ 440, 60 }; v[k++] = (nota_t){ 660, 90 }; }
    else if (compara_texto(n, "moneda") == 0) { v[k++] = (nota_t){ 988, 70 }; v[k++] = (nota_t){ 1319, 220 }; }
    else if (compara_texto(n, "bocina") == 0) { v[k++] = (nota_t){ 196, 300 }; v[k++] = (nota_t){ 0, 30 }; v[k++] = (nota_t){ 196, 300 }; }
    else if (compara_texto(n, "aplauso") == 0) { for (int i = 0; i < 6; i++) v[k++] = (nota_t){ (unsigned short)(120 + 40 * (i & 1)), 25 }; }
    else v[k++] = (nota_t){ 880, 120 };
    efeito_tocar(v, k);
}

/* ---- teclas ---- */
static int scancode(const char *n)
{
    static const char *const letras = "qwertyuiop";   /* fila de arriba: 0x10.. */
    static const char *const fila2 = "asdfghjkl";     /* 0x1E.. */
    static const char *const fila3 = "zxcvbnm";       /* 0x2C.. */
    if (n[0] && !n[1]) {
        char c = n[0];
        for (int i = 0; letras[i]; i++) if (letras[i] == c) return 0x10 + i;
        for (int i = 0; fila2[i]; i++) if (fila2[i] == c) return 0x1E + i;
        for (int i = 0; fila3[i]; i++) if (fila3[i] == c) return 0x2C + i;
        if (c >= '1' && c <= '9') return 0x02 + (c - '1');
        if (c == '0') return 0x0B;
    }
    if (compara_texto(n, "espacio") == 0) return 0x39;
    if (compara_texto(n, "flecha arriba") == 0) return 0x80 | 0x48;
    if (compara_texto(n, "flecha abajo") == 0) return 0x80 | 0x50;
    if (compara_texto(n, "flecha izquierda") == 0) return 0x80 | 0x4B;
    if (compara_texto(n, "flecha derecha") == 0) return 0x80 | 0x4D;
    if (compara_texto(n, "Intro") == 0) return 0x1C;
    return -1;
}
static unsigned tecla_virtual_hasta[4];   /* tecla «pulsada» por una orden de prueba: hasta cuándo */
static char tecla_virtual[4][20];
static bool tecla_agarrada(const char *n)
{
    if (compara_texto(n, "cualquiera") == 0) {
        for (int i = 0; i < 32; i++) if (jb_teclas_agarradas[i]) return true;
        return false;
    }
    for (int i = 0; i < 4; i++) if (tecla_virtual_hasta[i] > ahora && compara_texto(tecla_virtual[i], n) == 0) return true;
    int sc = scancode(n);
    return sc >= 0 && SEGURADA(jb_teclas_agarradas, sc);
}

/* ============================================================ evaluación */

static void evalua(hilo_t *h, const blk_t *b, val_t *o);
static void arg(hilo_t *h, const blk_t *b, int i, val_t *o)
{
    if (b->a[i].rep) { evalua(h, b->a[i].rep, o); return; }
    int32_t x;
    if (b->a[i].t[0] && jb_num_parse(b->a[i].t, &x)) v_num(o, x); else v_txt(o, b->a[i].t);
}
static int32_t arg_num(hilo_t *h, const blk_t *b, int i) { val_t v; arg(h, b, i, &v); return a_num(&v); }
static bool arg_bool(hilo_t *h, const blk_t *b, int i) { val_t v; arg(h, b, i, &v); return a_bool(&v); }

/* comparación: números si los dos lo son, si no texto sin distinguir mayúsculas */
static int comparar(const val_t *a, const val_t *b)
{
    char x[48], y[48];
    a_txt(a, x, sizeof x); a_txt(b, y, sizeof y);
    int32_t p, q;
    if (x[0] && y[0] && jb_num_parse(x, &p) && jb_num_parse(y, &q)) return p < q ? -1 : p > q ? 1 : 0;
    for (int i = 0;; i++) {
        int c = x[i], d = y[i];
        if (c >= 'A' && c <= 'Z') c += 32;
        if (d >= 'A' && d <= 'Z') d += 32;
        if (c != d) return c < d ? -1 : 1;
        if (!c) return 0;
    }
}

static void pos_de(hilo_t *h, const char *obj, int32_t *x, int32_t *y)
{
    if (compara_texto(obj, "raton") == 0) { *x = jb_raton_x * U; *y = jb_raton_y * U; return; }
    pers_t *q = buscar_pers(obj);
    if (q) { *x = q->x; *y = q->y; return; }
    *x = h->p->x; *y = h->p->y;
}

static void evalua(hilo_t *h, const blk_t *b, val_t *o)
{
    pers_t *p = h->p;
    char t1[48], t2[48];
    val_t a, c;
    switch (b->op) {
    case O_SUMA:  v_num(o, sat((int64_t)arg_num(h, b, 0) + arg_num(h, b, 1))); break;
    case O_RESTA: v_num(o, sat((int64_t)arg_num(h, b, 0) - arg_num(h, b, 1))); break;
    case O_MULT:  v_num(o, sat((int64_t)arg_num(h, b, 0) * arg_num(h, b, 1) / U)); break;
    case O_DIV: {
        int32_t d = arg_num(h, b, 1);
        if (d == 0) { aviso("No se puede dividir entre cero: el resultado es 0", ""); v_num(o, 0); }
        else v_num(o, sat((int64_t)arg_num(h, b, 0) * U / d));
        break;
    }
    case O_AZAR: {
        val_t x, y; arg(h, b, 0, &x); arg(h, b, 1, &y);
        int32_t lo = a_num(&x), hi = a_num(&y);
        if (lo > hi) { int32_t t = lo; lo = hi; hi = t; }
        if (lo % U == 0 && hi % U == 0) v_num(o, lo + (int32_t)(azar32() % (unsigned)((hi - lo) / U + 1)) * U);
        else v_num(o, lo + (int32_t)(azar32() % (unsigned)(hi - lo + 1)));
        break;
    }
    case O_MAYOR: arg(h, b, 0, &a); arg(h, b, 1, &c); v_bool(o, comparar(&a, &c) > 0); break;
    case O_MENOR: arg(h, b, 0, &a); arg(h, b, 1, &c); v_bool(o, comparar(&a, &c) < 0); break;
    case O_IGUAL: arg(h, b, 0, &a); arg(h, b, 1, &c); v_bool(o, comparar(&a, &c) == 0); break;
    case O_Y: v_bool(o, arg_bool(h, b, 0) && arg_bool(h, b, 1)); break;
    case O_O: v_bool(o, arg_bool(h, b, 0) || arg_bool(h, b, 1)); break;
    case O_NO: v_bool(o, !arg_bool(h, b, 0)); break;
    case O_UNIR: arg(h, b, 0, &a); arg(h, b, 1, &c); a_txt(&a, t1, sizeof t1); a_txt(&c, t2, sizeof t2); junta_texto(t1, t2, sizeof t1); v_txt(o, t1); break;
    case O_LETRA: {
        int32_t n = arg_num(h, b, 0) / U;
        arg(h, b, 1, &c); a_txt(&c, t2, sizeof t2);
        if (n >= 1 && n <= (int)tam_texto(t2)) { t1[0] = t2[n - 1]; t1[1] = 0; } else t1[0] = 0;
        v_txt(o, t1);
        break;
    }
    case O_LONGITUD: arg(h, b, 0, &a); a_txt(&a, t1, sizeof t1); v_num(o, (int32_t)tam_texto(t1) * U); break;
    case O_RESTO: {
        int32_t x = arg_num(h, b, 0), y = arg_num(h, b, 1);
        if (y == 0) { v_num(o, 0); break; }
        int32_t r = x % y;
        if (r != 0 && ((r < 0) != (y < 0))) r += y;
        v_num(o, r);
        break;
    }
    case O_REDONDEAR: { int32_t x = arg_num(h, b, 0); v_num(o, x >= 0 ? (x + U / 2) / U * U : -((-x + U / 2) / U * U)); break; }
    case O_ABS: { int32_t x = arg_num(h, b, 0); v_num(o, x < 0 ? -x : x); break; }
    case O_RAIZ: {
        int32_t x = arg_num(h, b, 0);
        if (x < 0) { aviso("No existe la raíz de un número negativo: el resultado es 0", ""); v_num(o, 0); }
        else v_num(o, (int32_t)raiz64((uint64_t)x * U));
        break;
    }
    case O_POS_X: v_num(o, p->x); break;
    case O_POS_Y: v_num(o, p->y); break;
    case O_DIRECCION: v_num(o, dir_norm(p->dir)); break;
    case O_N_TRAJE: v_num(o, (p->traje + 1) * U); break;
    case O_TAMANO: v_num(o, p->tam * U); break;
    case O_TOCANDO: {
        const char *m = b->a[0].t;
        if (compara_texto(m, "borde") == 0) v_bool(o, jb_tocando_borde(p));
        else if (compara_texto(m, "raton") == 0) {
            int x0, y0, x1, y1;
            jb_pers_caja(p, &x0, &y0, &x1, &y1);
            v_bool(o, jb_raton_x >= x0 && jb_raton_x <= x1 && jb_raton_y >= y0 && jb_raton_y <= y1);
        } else { pers_t *q = buscar_pers(m); v_bool(o, q && q != p && q->visible && p->visible && jb_pers_toca_pers(p, q)); }
        break;
    }
    case O_TOCANDO_COLOR: { int ci = jb_color_por_nombre(b->a[0].t); v_bool(o, ci >= 0 && p->visible && jb_pers_toca_color(p, ci)); break; }
    case O_TECLA_PULSADA: v_bool(o, tecla_agarrada(b->a[0].t)); break;
    case O_RATON_PULSADO: v_bool(o, jb_raton_pulsado); break;
    case O_RATON_X: v_num(o, jb_raton_x * U); break;
    case O_RATON_Y: v_num(o, jb_raton_y * U); break;
    case O_RESPUESTA: v_txt(o, jb_respuesta); { int32_t x; if (jb_respuesta[0] && jb_num_parse(jb_respuesta, &x)) v_num(o, x); } break;
    case O_CRONO: v_num(o, (int32_t)(ahora - crono0)); break;
    case O_DISTANCIA: {
        int32_t x, y; pos_de(h, b->a[0].t, &x, &y);
        int64_t dx = (int64_t)(x - p->x), dy = (int64_t)(y - p->y);
        v_num(o, (int32_t)raiz64((uint64_t)(dx * dx + dy * dy)));
        break;
    }
    case O_VAR: {
        int i = jb_var_buscar(b->a[0].t);
        if (i < 0) { aviso("La variable «%s» no existe todavía", b->a[0].t); v_num(o, 0); }
        else if (jb_p.vars[i].es_t) v_txt(o, jb_p.vars[i].t); else v_num(o, jb_p.vars[i].n);
        break;
    }
    case O_LISTA_ELEM: {
        int i = jb_lista_buscar(b->a[1].t);
        int32_t n = arg_num(h, b, 0) / U;
        if (i < 0) { aviso("La lista «%s» no existe todavía", b->a[1].t); v_txt(o, ""); }
        else if (n >= 1 && n <= jb_p.listas[i].n) { int32_t x; if (jb_num_parse(jb_p.listas[i].v[n - 1], &x)) v_num(o, x); else v_txt(o, jb_p.listas[i].v[n - 1]); }
        else v_txt(o, "");
        break;
    }
    case O_LISTA_LONG: { int i = jb_lista_buscar(b->a[0].t); v_num(o, i < 0 ? 0 : jb_p.listas[i].n * U); break; }
    default: v_num(o, 0); break;
    }
}

/* ============================================================ hilos */

static void lanzar_hilo(pers_t *p, int gi)
{
    hilo_t *libre = NULL;
    for (int i = 0; i < MAX_HILOS; i++) {
        if (hilos[i].vivo && hilos[i].p == p && hilos[i].gi == gi) { libre = &hilos[i]; break; }
        if (!hilos[i].vivo && !libre) libre = &hilos[i];
    }
    if (!libre) return;
    memset(libre, 0, sizeof *libre);
    libre->vivo = true; libre->p = p; libre->gi = gi;
    libre->top = p->guiones[gi].top;
    libre->pc = libre->top ? libre->top->sig : NULL;
}

static void disparar(int op, const char *clave, pers_t *solo)
{
    for (int i = 0; i < jb_p.npers; i++) {
        pers_t *p = jb_p.pers[i];
        if (solo && solo != p) continue;
        for (int g = 0; g < p->nguiones; g++) {
            blk_t *t = p->guiones[g].top;
            if (!t || t->op != op) continue;
            if (clave && compara_texto(t->a[0].t, clave) != 0 && compara_texto(t->a[0].t, "cualquiera") != 0) continue;
            lanzar_hilo(p, g);
        }
    }
}

static void burbuja(pers_t *p, const char *t, bool pensar)
{
    copia_texto(p->burbuja, t, sizeof p->burbuja);
    p->burb_pensar = pensar;
    jb_sucio = true;
}

static int buscar_traje(pers_t *p, const char *n)
{
    for (int i = 0; i < p->ntrajes; i++) if (compara_texto(p->trajes[i]->nombre, n) == 0) return i;
    int32_t x;
    if (jb_num_parse(n, &x) && x / U >= 1 && x / U <= p->ntrajes) return x / U - 1;
    return -1;
}

/* avanza un hilo hasta que ceda; false si terminó */
static void paso_hilo(hilo_t *h)
{
    pers_t *p = h->p;
    for (int presupuesto = 4000; presupuesto > 0; presupuesto--) {
        blk_t *b = h->pc;
        if (!b) {
            if (h->prof == 0) { h->vivo = false; return; }
            marco_t *f = &h->m[h->prof - 1];
            blk_t *o = f->b;
            if (o->op == O_REPETIR) {
                if (--f->cnt > 0) h->pc = o->c1; else { h->prof--; h->pc = o->sig; }
                return;          /* cada vuelta cede el turno, también la última */
            }
            if (o->op == O_SIEMPRE) { h->pc = o->c1; return; }
            if (o->op == O_REPETIR_HASTA) { h->prof--; h->pc = o; return; }
            h->prof--; h->pc = o->sig;
            continue;
        }
        val_t v, w;
        switch (b->op) {
        case O_MOVER: mover_pasos(p, arg_num(h, b, 0)); break;
        case O_GIRAR_D: p->dir = dir_norm(p->dir + arg_num(h, b, 0)); jb_sucio = true; break;
        case O_GIRAR_I: p->dir = dir_norm(p->dir - arg_num(h, b, 0)); jb_sucio = true; break;
        case O_IR_XY: { int32_t x = arg_num(h, b, 0), y = arg_num(h, b, 1); poner_pos(p, x, y); break; }
        case O_IR_A: {
            int32_t x, y;
            if (compara_texto(b->a[0].t, "azar") == 0) { x = (int32_t)(azar32() % ESC_L) * U - ESC_L / 2 * U; y = (int32_t)(azar32() % ESC_A) * U - ESC_A / 2 * U; }
            else pos_de(h, b->a[0].t, &x, &y);
            poner_pos(p, x, y);
            break;
        }
        case O_DESLIZAR:
            if (h->fase == 0) {
                h->t0 = ahora; h->dur = (unsigned)(arg_num(h, b, 0) < 0 ? 0 : arg_num(h, b, 0));
                h->ax = p->x; h->ay = p->y; h->bx = arg_num(h, b, 1); h->by = arg_num(h, b, 2);
                h->fase = 1;
            }
            if (ahora - h->t0 >= h->dur) { poner_pos(p, h->bx, h->by); h->fase = 0; break; }
            poner_pos(p, h->ax + (int32_t)((int64_t)(h->bx - h->ax) * (int)(ahora - h->t0) / (int)h->dur),
                         h->ay + (int32_t)((int64_t)(h->by - h->ay) * (int)(ahora - h->t0) / (int)h->dur));
            return;
        case O_APUNTAR: p->dir = dir_norm(arg_num(h, b, 0)); jb_sucio = true; break;
        case O_APUNTAR_A: {
            int32_t x, y; pos_de(h, b->a[0].t, &x, &y);
            p->dir = dir_norm(angulo_de((int64_t)(x - p->x), (int64_t)(y - p->y)));
            jb_sucio = true;
            break;
        }
        case O_SUMAR_X: poner_pos(p, sat((int64_t)p->x + arg_num(h, b, 0)), p->y); break;
        case O_FIJAR_X: poner_pos(p, arg_num(h, b, 0), p->y); break;
        case O_SUMAR_Y: poner_pos(p, p->x, sat((int64_t)p->y + arg_num(h, b, 0))); break;
        case O_FIJAR_Y: poner_pos(p, p->x, arg_num(h, b, 0)); break;
        case O_REBOTAR: rebotar(p); break;
        case O_GIRO: p->giro = compara_texto(b->a[0].t, "izquierda-derecha") == 0 ? 1 : compara_texto(b->a[0].t, "ninguno") == 0 ? 2 : 0; jb_sucio = true; break;
        case O_DECIR_T: case O_PENSAR_T:
            if (h->fase == 0) {
                arg(h, b, 0, &v); char t[96]; a_txt(&v, t, sizeof t);
                burbuja(p, t, b->op == O_PENSAR_T);
                h->t0 = ahora; { int32_t d = arg_num(h, b, 1); h->dur = (unsigned)(d < 0 ? 0 : d); } h->fase = 1;
            }
            if (ahora - h->t0 >= h->dur) { p->burbuja[0] = 0; jb_sucio = true; h->fase = 0; break; }
            return;
        case O_DECIR: case O_PENSAR: { arg(h, b, 0, &v); char t[96]; a_txt(&v, t, sizeof t); burbuja(p, t, b->op == O_PENSAR); break; }
        case O_TRAJE: { int i = buscar_traje(p, b->a[0].t); if (i >= 0) { p->traje = i; jb_pers_recalcular(p); jb_sucio = true; } else aviso("No encuentro el traje «%s»", b->a[0].t); break; }
        case O_SIG_TRAJE: if (p->ntrajes) { p->traje = (p->traje + 1) % p->ntrajes; jb_pers_recalcular(p); jb_sucio = true; } break;
        case O_FONDO: {
            int ix = -1;
            for (int i = 0; i < jb_p.nfondos; i++) if (compara_texto(jb_p.fondos[i]->nombre, b->a[0].t) == 0) ix = i;
            if (ix < 0) { int32_t x; if (jb_num_parse(b->a[0].t, &x) && x / U >= 1 && x / U <= jb_p.nfondos) ix = x / U - 1; }
            if (ix >= 0) { jb_p.fondo = ix; jb_sucio = true; } else aviso("No encuentro el fondo «%s»", b->a[0].t);
            break;
        }
        case O_SUMAR_TAM: { int32_t t = p->tam + arg_num(h, b, 0) / U; p->tam = t < 5 ? 5 : t > 500 ? 500 : t; jb_sucio = true; break; }
        case O_FIJAR_TAM: { int32_t t = arg_num(h, b, 0) / U; p->tam = t < 5 ? 5 : t > 500 ? 500 : t; jb_sucio = true; break; }
        case O_MOSTRAR: p->visible = true; jb_sucio = true; break;
        case O_OCULTAR: p->visible = false; jb_sucio = true; break;
        case O_NOTA:
            if (h->fase == 0) {
                int32_t n = arg_num(h, b, 0) / U, pul = arg_num(h, b, 1);
                int ms = (int)((int64_t)(pul < 0 ? 0 : pul) * 1000 / U);
                if (ms > 4000) ms = 4000;
                nota_t nt = { (unsigned short)freq_nota(n), (unsigned short)ms };
                if (ms >= 10) efeito_tocar(&nt, 1);
                h->t0 = ahora; h->dur = (unsigned)ms; h->fase = 1;
            }
            if (ahora - h->t0 >= h->dur) { h->fase = 0; break; }
            return;
        case O_SONIDO: sonido_nombre(b->a[0].t); break;
        case O_CALLAR: efeito_parar(); break;
        case O_LAPIZ_BAJAR: p->lapiz = true; jb_lapiz_punto(p->x / U, p->y / U, p->lapiz_color, p->lapiz_grosor); jb_sucio = true; break;
        case O_LAPIZ_SUBIR: p->lapiz = false; break;
        case O_LAPIZ_BORRAR: jb_lapiz_borrar(); jb_sucio = true; break;
        case O_LAPIZ_COLOR: { int ci = jb_color_por_nombre(b->a[0].t); if (ci >= 0) p->lapiz_color = ci; break; }
        case O_LAPIZ_GROSOR: { int32_t g = arg_num(h, b, 0) / U; p->lapiz_grosor = g < 1 ? 1 : g > 30 ? 30 : g; break; }
        case O_LAPIZ_SELLAR: jb_lapiz_punto(p->x / U, p->y / U, p->lapiz_color, tam_px(p) / 3 + 1); jb_sucio = true; break;
        case O_LAPIZ_SIG: p->lapiz_color = p->lapiz_color % 31 + 1; break;
        case O_ESPERAR:
            if (h->fase == 0) { h->t0 = ahora; int32_t d = arg_num(h, b, 0); h->dur = (unsigned)(d < 0 ? 0 : (int64_t)d); h->fase = 1; }
            if (ahora - h->t0 >= h->dur) { h->fase = 0; break; }
            return;
        case O_REPETIR: {
            int32_t n = arg_num(h, b, 0) / U;
            if (n <= 0) break;
            if (h->prof >= MAX_PROF) { aviso("Demasiados bloques metidos unos dentro de otros", ""); h->vivo = false; return; }
            h->m[h->prof].b = b; h->m[h->prof].cnt = n; h->prof++;
            h->pc = b->c1;
            continue;
        }
        case O_SIEMPRE:
            if (h->prof >= MAX_PROF) { h->vivo = false; return; }
            h->m[h->prof].b = b; h->m[h->prof].cnt = 0; h->prof++;
            h->pc = b->c1;
            if (!h->pc) return;
            continue;
        case O_SI: case O_SI_NO:
            if (arg_bool(h, b, 0)) {
                if (!b->c1) break;
                if (h->prof >= MAX_PROF) { h->vivo = false; return; }
                h->m[h->prof].b = b; h->prof++; h->pc = b->c1;
                continue;
            } else if (b->op == O_SI_NO && b->c2) {
                if (h->prof >= MAX_PROF) { h->vivo = false; return; }
                h->m[h->prof].b = b; h->prof++; h->pc = b->c2;
                continue;
            }
            break;
        case O_ESPERAR_HASTA: if (!arg_bool(h, b, 0)) return; break;
        case O_REPETIR_HASTA:
            if (arg_bool(h, b, 0)) break;
            if (h->prof >= MAX_PROF) { h->vivo = false; return; }
            h->m[h->prof].b = b; h->prof++; h->pc = b->c1;
            if (!h->pc) { h->prof--; h->pc = b; return; }
            continue;
        case O_DETENER:
            if (compara_texto(b->a[0].t, "todo") == 0) { jb_motor_parar(); return; }
            h->vivo = false; return;
        case O_ENVIAR: disparar(O_RECIBIR, b->a[0].t, NULL); break;
        case O_PREGUNTAR:
            if (h->fase == 0) {
                if (jb_hay_pregunta) return;
                arg(h, b, 0, &v); a_txt(&v, jb_pregunta, sizeof jb_pregunta);
                jb_hay_pregunta = true; jb_respuesta[0] = 0; preg_dueno = h; h->fase = 1; jb_sucio = true;
                return;
            }
            if (jb_hay_pregunta && preg_dueno == h) return;
            h->fase = 0;
            break;
        case O_CRONO_REINICIAR: crono0 = ahora; break;
        case O_FIJAR_VAR: {
            int i = jb_var_buscar(b->a[0].t);
            if (i < 0) { aviso("La variable «%s» no existe todavía", b->a[0].t); break; }
            arg(h, b, 1, &w);
            if (w.k == 1) { copia_texto(jb_p.vars[i].t, w.t, sizeof jb_p.vars[i].t); jb_p.vars[i].es_t = true; }
            else { jb_p.vars[i].n = w.n; jb_p.vars[i].es_t = false; }
            jb_sucio = true;
            break;
        }
        case O_CAMBIAR_VAR: {
            int i = jb_var_buscar(b->a[0].t);
            if (i < 0) { aviso("La variable «%s» no existe todavía", b->a[0].t); break; }
            int32_t base = 0;
            if (jb_p.vars[i].es_t) { int32_t x; if (jb_num_parse(jb_p.vars[i].t, &x)) base = x; } else base = jb_p.vars[i].n;
            jb_p.vars[i].n = sat((int64_t)base + arg_num(h, b, 1)); jb_p.vars[i].es_t = false;
            jb_sucio = true;
            break;
        }
        case O_MOSTRAR_VAR: case O_OCULTAR_VAR: { int i = jb_var_buscar(b->a[0].t); if (i >= 0) { jb_p.vars[i].mostrar = b->op == O_MOSTRAR_VAR; jb_sucio = true; } break; }
        case O_LISTA_AGREGAR: {
            int i = jb_lista_buscar(b->a[1].t);
            if (i < 0) { aviso("La lista «%s» no existe todavía", b->a[1].t); break; }
            arg(h, b, 0, &v);
            if (jb_p.listas[i].n < MAX_ELEM) { char t[48]; a_txt(&v, t, sizeof t); copia_texto(jb_p.listas[i].v[jb_p.listas[i].n++], t, 24); }
            break;
        }
        case O_LISTA_VACIAR: { int i = jb_lista_buscar(b->a[0].t); if (i >= 0) jb_p.listas[i].n = 0; break; }
        default: break;   /* sombreros: no hacen nada */
        }
        h->pc = b->sig;
    }
}

/* ============================================================ interfaz */

void jb_motor_iniciar(void)
{
    memset(hilos, 0, sizeof hilos);
    jb_hay_pregunta = false; jb_respuesta[0] = 0;
    jb_aviso[0] = 0;
    jb_sucio = true;
    for (int i = 0; i < jb_p.npers; i++) jb_pers_recalcular(jb_p.pers[i]);
}

void jb_motor_parar(void)
{
    for (int i = 0; i < MAX_HILOS; i++) hilos[i].vivo = false;
    for (int i = 0; i < jb_p.npers; i++) { jb_p.pers[i]->burbuja[0] = 0; jb_p.pers[i]->lapiz = false; }
    jb_hay_pregunta = false;
    efeito_parar();
    jb_sucio = true;
}

void jb_motor_bandera(void)
{
    jb_motor_parar();
    jb_aviso[0] = 0;
    crono0 = ahora;
    disparar(O_BANDERA, NULL, NULL);
}

void jb_motor_tecla(int tecla)
{
    char n[20];
    switch (tecla) {
    case ' ': copia_texto(n, "espacio", sizeof n); break;
    case TECLA_CIMA: copia_texto(n, "flecha arriba", sizeof n); break;
    case TECLA_BAIXO: copia_texto(n, "flecha abajo", sizeof n); break;
    case TECLA_ESQUERDA: copia_texto(n, "flecha izquierda", sizeof n); break;
    case TECLA_DIREITA: copia_texto(n, "flecha derecha", sizeof n); break;
    case '\n': case '\r': copia_texto(n, "Intro", sizeof n); break;
    default:
        if (tecla >= 'A' && tecla <= 'Z') tecla += 32;
        if ((tecla >= 'a' && tecla <= 'z') || (tecla >= '0' && tecla <= '9')) { n[0] = (char)tecla; n[1] = 0; }
        else return;
    }
    disparar(O_TECLA, n, NULL);
}

/* una orden de prueba: mantiene «pulsada» una tecla durante ms milisegundos */
void jb_tecla_virtual(const char *nombre, unsigned ms)
{
    for (int i = 0; i < 4; i++) if (tecla_virtual_hasta[i] <= ahora) { copia_texto(tecla_virtual[i], nombre, sizeof tecla_virtual[i]); tecla_virtual_hasta[i] = ahora + ms; return; }
}

void jb_motor_clic(int x, int y)
{
    int i = jb_pers_en(x, y);
    if (i >= 0) disparar(O_CLIC, NULL, jb_p.pers[i]);
}

void jb_responder(void)
{
    jb_hay_pregunta = false;
    jb_sucio = true;
}

void jb_motor_ejecutar(pers_t *p, blk_t *desde)
{
    hilo_t *libre = NULL;
    for (int i = 0; i < MAX_HILOS; i++) if (!hilos[i].vivo) { libre = &hilos[i]; break; }
    if (!libre || !desde) return;
    memset(libre, 0, sizeof *libre);
    libre->vivo = true; libre->p = p; libre->gi = 1000;
    libre->top = desde;
    libre->pc = jb_ops[desde->op].tipo == K_HAT ? desde->sig : desde;
}

bool jb_motor_activo(void) { for (int i = 0; i < MAX_HILOS; i++) if (hilos[i].vivo) return true; return false; }
int jb_motor_hilos(void) { int n = 0; for (int i = 0; i < MAX_HILOS; i++) if (hilos[i].vivo) n++; return n; }

void jb_motor_cuadro(unsigned t)
{
    ahora = t;
    static int espera_resp;
    if (jb_hay_pregunta && jb_auto_resp) {
        if (++espera_resp > 3) { copia_texto(jb_respuesta, jb_auto_resp, sizeof jb_respuesta); jb_responder(); espera_resp = 0; }
    } else espera_resp = 0;
    for (int i = 0; i < MAX_HILOS; i++) {
        if (!hilos[i].vivo) continue;
        paso_hilo(&hilos[i]);
    }
    /* los bocadillos con tiempo ya los gestionan sus hilos */
}
