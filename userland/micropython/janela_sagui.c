/* janela_sagui.c - el módulo sagui de MicroPython: ventanas, dibujo,
 * eventos, imágenes (mejora 033, D083) y sonido (sagui.Sonido, D088).
 *
 *   import sagui
 *   v = sagui.Ventana(320, 200, "Hola")
 *   v.rect(10, 10, 100, 50, 0x2A6FD8)
 *   v.texto(20, 30, "¡Hola!", 0xFFFFFF)
 *   v.actualizar()
 *   while True:
 *       e = v.evento(1000)            # espera hasta 1 s; None si no llegó nada
 *       if e and e.tipo == sagui.CERRAR: break
 *
 * La API pública está en español (como la interfaz del sistema); los nombres
 * internos siguen el portugués del resto del código. Todo lo que dibuja es C:
 * Python solo llama. Cada Ventana (y cada Imagen, que acepta los mismos
 * dibujos) es una "superficie": píxeles 0x00RRGGBB, un recorte (lo que queda
 * fuera no se toca) y el área sucia, el rectángulo que cambió desde el último
 * actualizar() y que es el único que se envía al kernel.
 *
 * Los eventos esperan con plazo en el kernel (SIS_PROXIMO_EVENTO con
 * EVENTO_PRAZO, D083): un programa parado no gasta CPU. Las imágenes
 * (BMP/PNM/PNG) vienen de la biblioteca de libsagui (D054) por ponte_sagui.c.
 *
 * Como los otros *_sagui.c, los textos de este archivo están en UTF-8 y el
 * texto dibujado se convierte a Latin-1 de la fuente del sistema. */
#include <string.h>
#include <math.h>
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objlist.h"
#include "py/objtuple.h"
#include "sagui_mp.h"
#include "ponte_sagui.h"

/* ---- libsagui (sin el sagui.h; ver sagui_mp.h) ---- */
int  criar_janela(const char *titulo, int largura, int altura, uint32_t **pixels);
int  fechar_janela(int id);
int  atualizar_janela(int id, const retangulo_t *r);
int  esperar_evento(int id, evento_t *e, int ms);
int  titulo_janela(int id, const char *titulo);
int  estilo_janela(int id, unsigned estilo, int min_l, int min_a);
int  redimensionar_janela(int id, int largura, int altura, uint32_t **pixels);
int  menu_contextual(int id, int x, int y, const char *itens, int codigo);
int  teclas_seguradas(unsigned char mapa[32]);
int  tocar(const nota_t *notas, int n, int modo);
int  audio_abrir(int taxa, int canais, int bits);
int  audio_escrever(int fd, const void *buf, unsigned n);
int  audio_escrever_sem_bloquear(int fd, const void *buf, unsigned n);
int  audio_controle(int fd, int op, int valor);
int  audio_fechar(int fd, int esperar);
int  info_sistema(int tipo, void *buf, unsigned n);
/* diálogos de archivos de la D272 (gui_arquivos.c): modales, con su propia ventana */
_Bool dialogo_abrir(const char *titulo, const char *filtros, const char *inicial, char *saida, int max);
_Bool dialogo_guardar(const char *titulo, const char *filtros, const char *inicial, char *saida, int max);
_Bool dialogo_carpeta(const char *titulo, const char *inicial, char *saida, int max);
void preencher32(uint32_t *d, uint32_t cor, size_t n);
extern const unsigned char fonte_sagui[256][16];

#define COR_FUNDO 0xF4F4F0   /* el fondo de las ventanas de SaguiOS (gui.h) */

/* Portapapeles (D083): las llamadas 94-95 de la mejora 037 (Mico). Si el kernel
 * no las tiene, el texto queda solo en este proceso. */
#define SIS_COPIAR_TEXTO_MP 94
#define SIS_COLAR_TEXTO_MP  95

/* ------------------------------------------------------------ superficies */

typedef struct {
    uint32_t *px;
    int larg, alt;
    int cx0, cy0, cx1, cy1;   /* recorte: [cx0, cx1) x [cy0, cy1) */
    int sx0, sy0, sx1, sy1;   /* área sucia (vacía si sx0 >= sx1) */
} sup_t;

typedef struct {
    mp_obj_base_t base;
    sup_t s;
    int id;               /* -1 = cerrada */
    unsigned estilo;
    int min_l, min_a;
    mp_obj_t titulo;
} janela_obj_t;

typedef struct {
    mp_obj_base_t base;
    sup_t s;
    char formato[16];
} imagem_obj_t;

static const mp_obj_type_t tipo_janela;
static const mp_obj_type_t tipo_imagem;
static const mp_obj_type_t tipo_evento;

MP_REGISTER_ROOT_POINTER(mp_obj_t sagui_janelas);   /* lista de las ventanas abiertas */

static void sup_iniciar(sup_t *s, uint32_t *px, int l, int a)
{
    s->px = px;
    s->larg = l;
    s->alt = a;
    s->cx0 = s->cy0 = 0;
    s->cx1 = l;
    s->cy1 = a;
    s->sx0 = s->sy0 = s->sx1 = s->sy1 = 0;
}

static sup_t *sup_de(mp_obj_t o)
{
    if (mp_obj_is_type(o, &tipo_janela)) {
        janela_obj_t *j = MP_OBJ_TO_PTR(o);
        if (j->id < 0) mp_raise_ValueError(MP_ERROR_TEXT("ventana cerrada"));
        return &j->s;
    }
    return &((imagem_obj_t *)MP_OBJ_TO_PTR(o))->s;
}

/* Marca como sucio el rectángulo [x0, x1) x [y0, y1), ya dentro del recorte. */
static void sujar(sup_t *s, int x0, int y0, int x1, int y1)
{
    if (x0 < s->cx0) x0 = s->cx0;
    if (y0 < s->cy0) y0 = s->cy0;
    if (x1 > s->cx1) x1 = s->cx1;
    if (y1 > s->cy1) y1 = s->cy1;
    if (x0 >= x1 || y0 >= y1) return;
    if (s->sx0 >= s->sx1) { s->sx0 = x0; s->sy0 = y0; s->sx1 = x1; s->sy1 = y1; return; }
    if (x0 < s->sx0) s->sx0 = x0;
    if (y0 < s->sy0) s->sy0 = y0;
    if (x1 > s->sx1) s->sx1 = x1;
    if (y1 > s->sy1) s->sy1 = y1;
}

static inline void ponto(sup_t *s, int x, int y, uint32_t c)
{
    if (x >= s->cx0 && x < s->cx1 && y >= s->cy0 && y < s->cy1) s->px[y * s->larg + x] = c;
}

/* Línea horizontal [x0, x1] a la altura y (sin marcar la suciedad). */
static void linha_h(sup_t *s, int x0, int x1, int y, uint32_t c)
{
    if (y < s->cy0 || y >= s->cy1) return;
    if (x0 > x1) { int t = x0; x0 = x1; x1 = t; }
    if (x0 < s->cx0) x0 = s->cx0;
    if (x1 >= s->cx1) x1 = s->cx1 - 1;
    if (x0 > x1) return;
    preencher32(s->px + y * s->larg + x0, c, (size_t)(x1 - x0 + 1));
}

static void retangulo(sup_t *s, int x, int y, int l, int a, uint32_t c)
{
    int x1 = x + l, y1 = y + a;
    if (x < s->cx0) x = s->cx0;
    if (y < s->cy0) y = s->cy0;
    if (x1 > s->cx1) x1 = s->cx1;
    if (y1 > s->cy1) y1 = s->cy1;
    if (x >= x1 || y >= y1) return;
    for (int yy = y; yy < y1; yy++) preencher32(s->px + yy * s->larg + x, c, (size_t)(x1 - x));
}

/* ------------------------------------------------------------ argumentos */

/* Color: entero 0xRRGGBB o tupla/lista (r, g, b). */
static uint32_t cor_de(mp_obj_t o)
{
    if (mp_obj_is_int(o)) return (uint32_t)mp_obj_get_int_truncated(o) & 0xFFFFFF;
    size_t n;
    mp_obj_t *v;
    mp_obj_get_array(o, &n, &v);
    if (n < 3) mp_raise_ValueError(MP_ERROR_TEXT("color: 0xRRGGBB o (r, g, b)"));
    uint32_t c = 0;
    for (int i = 0; i < 3; i++) {
        mp_int_t k = mp_obj_get_int(v[i]);
        c = c << 8 | (uint32_t)(k < 0 ? 0 : k > 255 ? 255 : k);
    }
    return c;
}

static int inteiro(mp_obj_t o)
{
    if (mp_obj_is_float(o)) return (int)floor(mp_obj_get_float(o));
    return (int)mp_obj_get_int(o);
}

/* ------------------------------------------------------------ dibujo */

/* pixel(x, y) -> color; pixel(x, y, color) */
static mp_obj_t sup_pixel(size_t n, const mp_obj_t *a)
{
    sup_t *s = sup_de(a[0]);
    int x = inteiro(a[1]), y = inteiro(a[2]);
    if (n == 3) {
        if ((unsigned)x >= (unsigned)s->larg || (unsigned)y >= (unsigned)s->alt) return mp_const_none;
        return MP_OBJ_NEW_SMALL_INT(s->px[y * s->larg + x] & 0xFFFFFF);
    }
    ponto(s, x, y, cor_de(a[3]));
    sujar(s, x, y, x + 1, y + 1);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sup_pixel_obj, 3, 4, sup_pixel);

static void linha(sup_t *s, int x0, int y0, int x1, int y1, uint32_t c, int g)
{
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = y1 > y0 ? y1 - y0 : y0 - y1, sy = y0 < y1 ? 1 : -1;
    int passos = dx > dy ? dx : dy, acum = 0, m = g / 2;
    if (g <= 1 && dy == 0) { linha_h(s, x0, x1, y0, c); return; }
    for (int k = 0; k <= passos; k++) {
        if (g <= 1) ponto(s, x0, y0, c);
        else retangulo(s, x0 - m, y0 - m, g, g, c);
        if (dx >= dy) { x0 += sx; acum += dy; if (2 * acum >= dx) { y0 += sy; acum -= dx; } }
        else          { y0 += sy; acum += dx; if (2 * acum >= dy) { x0 += sx; acum -= dy; } }
    }
}

/* linea(x0, y0, x1, y1, color, grosor=1) */
static mp_obj_t sup_linea(size_t n, const mp_obj_t *a)
{
    sup_t *s = sup_de(a[0]);
    int x0 = inteiro(a[1]), y0 = inteiro(a[2]), x1 = inteiro(a[3]), y1 = inteiro(a[4]);
    int g = n > 6 ? inteiro(a[6]) : 1;
    if (g < 1) g = 1;
    linha(s, x0, y0, x1, y1, cor_de(a[5]), g);
    int m = g / 2;
    sujar(s, (x0 < x1 ? x0 : x1) - m, (y0 < y1 ? y0 : y1) - m, (x0 > x1 ? x0 : x1) - m + g, (y0 > y1 ? y0 : y1) - m + g);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sup_linea_obj, 6, 7, sup_linea);

/* rect(x, y, ancho, alto, color, relleno=True, grosor=1) */
static mp_obj_t sup_rect(size_t n, const mp_obj_t *a, mp_map_t *kw)
{
    enum { A_RELLENO, A_GROSOR };
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_relleno, MP_ARG_BOOL, { .u_bool = true } },
        { MP_QSTR_grosor, MP_ARG_INT, { .u_int = 1 } },
    };
    if (n < 6) mp_raise_TypeError(MP_ERROR_TEXT("rect(x, y, ancho, alto, color, relleno=True, grosor=1)"));
    mp_arg_val_t v[2];
    mp_arg_parse_all(n - 6, a + 6, kw, 2, permitidos, v);
    sup_t *s = sup_de(a[0]);
    int x = inteiro(a[1]), y = inteiro(a[2]), l = inteiro(a[3]), al = inteiro(a[4]);
    uint32_t c = cor_de(a[5]);
    if (l <= 0 || al <= 0) return mp_const_none;
    if (v[A_RELLENO].u_bool) retangulo(s, x, y, l, al, c);
    else {
        int g = v[A_GROSOR].u_int < 1 ? 1 : v[A_GROSOR].u_int;
        if (2 * g >= l || 2 * g >= al) retangulo(s, x, y, l, al, c);
        else {
            retangulo(s, x, y, l, g, c);
            retangulo(s, x, y + al - g, l, g, c);
            retangulo(s, x, y + g, g, al - 2 * g, c);
            retangulo(s, x + l - g, y + g, g, al - 2 * g, c);
        }
    }
    sujar(s, x, y, x + l, y + al);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(sup_rect_obj, 6, sup_rect);

/* Media anchura de la elipse de semiejes (ra, rb) en la línea que queda a dy del
 * centro (todo en medios píxeles, para los tamaños pares e impares). */
static double meia_largura(double ra, double rb, double dy)
{
    if (rb <= 0) return -1;
    double t = 1 - (dy * dy) / (rb * rb);
    return t < 0 ? -1 : ra * sqrt(t);
}

static void elipse(sup_t *s, int x, int y, int l, int a, uint32_t c, bool cheia, int g)
{
    if (l <= 0 || a <= 0) return;
    double cx = x + l / 2.0, cy = y + a / 2.0;
    double ra = l / 2.0, rb = a / 2.0, ia = ra - g, ib = rb - g;
    for (int yy = y; yy < y + a; yy++) {
        if (yy < s->cy0 || yy >= s->cy1) continue;
        double dy = yy + 0.5 - cy;
        double w = meia_largura(ra, rb, dy);
        if (w < 0) continue;
        int xl = (int)ceil(cx - w - 0.5), xr = (int)ceil(cx + w - 0.5) - 1;
        if (xr < xl) xr = xl = (int)floor(cx);
        if (cheia) { linha_h(s, xl, xr, yy, c); continue; }
        double wi = meia_largura(ia, ib, dy);
        if (ia <= 0 || wi < 0) { linha_h(s, xl, xr, yy, c); continue; }
        int il = (int)ceil(cx - wi - 0.5), ir = (int)ceil(cx + wi - 0.5) - 1;
        if (il > xl) linha_h(s, xl, il - 1, yy, c); else ponto(s, xl, yy, c);
        if (ir < xr) linha_h(s, ir + 1, xr, yy, c); else ponto(s, xr, yy, c);
    }
}

/* elipse(x, y, ancho, alto, color, relleno=True, grosor=1): a elipse inscrita en el rectángulo */
static mp_obj_t sup_elipse(size_t n, const mp_obj_t *a, mp_map_t *kw)
{
    enum { A_RELLENO, A_GROSOR };
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_relleno, MP_ARG_BOOL, { .u_bool = true } },
        { MP_QSTR_grosor, MP_ARG_INT, { .u_int = 1 } },
    };
    if (n < 6) mp_raise_TypeError(MP_ERROR_TEXT("elipse(x, y, ancho, alto, color, relleno=True, grosor=1)"));
    mp_arg_val_t v[2];
    mp_arg_parse_all(n - 6, a + 6, kw, 2, permitidos, v);
    sup_t *s = sup_de(a[0]);
    int x = inteiro(a[1]), y = inteiro(a[2]), l = inteiro(a[3]), al = inteiro(a[4]);
    int g = v[A_GROSOR].u_int < 1 ? 1 : v[A_GROSOR].u_int;
    elipse(s, x, y, l, al, cor_de(a[5]), v[A_RELLENO].u_bool, g);
    sujar(s, x, y, x + l, y + al);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(sup_elipse_obj, 6, sup_elipse);

/* Puntos de un polígono: [x0, y0, x1, y1, ...] o [(x0, y0), (x1, y1), ...]. */
static int ler_pontos(mp_obj_t o, double **px, double **py)
{
    size_t n;
    mp_obj_t *v;
    mp_obj_get_array(o, &n, &v);
    bool pares = n > 0 && !mp_obj_is_int(v[0]) && !mp_obj_is_float(v[0]);
    int k = pares ? (int)n : (int)n / 2;
    *px = m_new(double, k + 1);
    *py = m_new(double, k + 1);
    for (int i = 0; i < k; i++) {
        if (pares) {
            size_t m;
            mp_obj_t *p;
            mp_obj_get_array(v[i], &m, &p);
            if (m < 2) mp_raise_ValueError(MP_ERROR_TEXT("punto (x, y)"));
            (*px)[i] = mp_obj_get_float(p[0]);
            (*py)[i] = mp_obj_get_float(p[1]);
        } else {
            (*px)[i] = mp_obj_get_float(v[2 * i]);
            (*py)[i] = mp_obj_get_float(v[2 * i + 1]);
        }
    }
    return k;
}

/* poligono(puntos, color, relleno=True, grosor=1): relleno par-impar
 * por líneas (el centro de cada píxel dentro o fuera). */
static mp_obj_t sup_poligono(size_t n, const mp_obj_t *a, mp_map_t *kw)
{
    enum { A_RELLENO, A_GROSOR };
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_relleno, MP_ARG_BOOL, { .u_bool = true } },
        { MP_QSTR_grosor, MP_ARG_INT, { .u_int = 1 } },
    };
    if (n < 3) mp_raise_TypeError(MP_ERROR_TEXT("poligono(puntos, color, relleno=True, grosor=1)"));
    mp_arg_val_t v[2];
    mp_arg_parse_all(n - 3, a + 3, kw, 2, permitidos, v);
    sup_t *s = sup_de(a[0]);
    uint32_t c = cor_de(a[2]);
    double *xs, *ys;
    int k = ler_pontos(a[1], &xs, &ys);
    if (k < 1) return mp_const_none;
    double xmin = xs[0], xmax = xs[0], ymin = ys[0], ymax = ys[0];
    for (int i = 1; i < k; i++) {
        if (xs[i] < xmin) xmin = xs[i];
        if (xs[i] > xmax) xmax = xs[i];
        if (ys[i] < ymin) ymin = ys[i];
        if (ys[i] > ymax) ymax = ys[i];
    }
    int g = v[A_GROSOR].u_int < 1 ? 1 : v[A_GROSOR].u_int;
    if (v[A_RELLENO].u_bool && k >= 3) {
        double *cortes = m_new(double, k);
        int y0 = (int)floor(ymin), y1 = (int)ceil(ymax);
        if (y0 < s->cy0) y0 = s->cy0;
        if (y1 > s->cy1) y1 = s->cy1;
        for (int y = y0; y < y1; y++) {
            double yc = y + 0.5;
            int nc = 0;
            for (int i = 0; i < k; i++) {
                int j = (i + 1) % k;
                double ya = ys[i], yb = ys[j];
                if ((ya <= yc && yb > yc) || (yb <= yc && ya > yc)) {
                    double x = xs[i] + (yc - ya) * (xs[j] - xs[i]) / (yb - ya);
                    int p = nc++;
                    while (p > 0 && cortes[p - 1] > x) { cortes[p] = cortes[p - 1]; p--; }   /* en orden */
                    cortes[p] = x;
                }
            }
            for (int i = 0; i + 1 < nc; i += 2) {
                int xa = (int)ceil(cortes[i] - 0.5), xb = (int)ceil(cortes[i + 1] - 0.5) - 1;
                if (xa <= xb) linha_h(s, xa, xb, y, c);
            }
        }
        m_del(double, cortes, k);
    } else {
        for (int i = 0; i < k; i++) {
            int j = (i + 1) % k;
            if (j == 0 && k == 2) break;
            linha(s, (int)floor(xs[i]), (int)floor(ys[i]), (int)floor(xs[j]), (int)floor(ys[j]), c, g);
        }
    }
    sujar(s, (int)floor(xmin) - g, (int)floor(ymin) - g, (int)ceil(xmax) + g + 1, (int)ceil(ymax) + g + 1);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(sup_poligono_obj, 3, sup_poligono);

/* Siguiente carácter de un texto UTF-8 como código Latin-1 ('?' si no cabe). */
static unsigned prox_letra(const unsigned char **p, const unsigned char *fim)
{
    unsigned c = *(*p)++;
    if (c < 0x80) return c;
    int extra = (c & 0xE0) == 0xC0 ? 1 : (c & 0xF0) == 0xE0 ? 2 : (c & 0xF8) == 0xF0 ? 3 : 0;
    unsigned cp = c & (0x3F >> extra);
    for (int i = 0; i < extra && *p < fim; i++) cp = cp << 6 | (*(*p)++ & 0x3F);
    return cp <= 0xFF ? cp : '?';
}

static void letra(sup_t *s, int x, int y, unsigned ch, uint32_t c)
{
    const unsigned char *g = fonte_sagui[ch & 0xFF];
    int l0 = s->cy0 - y, l1 = s->cy1 - y;
    if (l0 < 0) l0 = 0;
    if (l1 > 16) l1 = 16;
    int c0 = s->cx0 - x, c1 = s->cx1 - x;
    if (c0 < 0) c0 = 0;
    if (c1 > 8) c1 = 8;
    for (int lin = l0; lin < l1; lin++) {
        unsigned bits = g[lin];
        if (!bits) continue;
        uint32_t *d = s->px + (y + lin) * s->larg + x;
        for (int col = c0; col < c1; col++)
            if (bits & (0x80 >> col)) d[col] = c;
    }
}

/* texto(x, y, s, color, fondo=None) -> ancho en pixels. Fuente 8x16 del
 * sistema; el '\n' empieza otra línea. */
static mp_obj_t sup_texto(size_t n, const mp_obj_t *a)
{
    sup_t *s = sup_de(a[0]);
    int x = inteiro(a[1]), y = inteiro(a[2]);
    size_t tam;
    const unsigned char *p = (const unsigned char *)mp_obj_str_get_data(a[3], &tam), *fim = p + tam;
    uint32_t c = cor_de(a[4]);
    bool fundo = n > 5 && a[5] != mp_const_none;
    uint32_t cf = fundo ? cor_de(a[5]) : 0;
    int xx = x, yy = y, maior = 0;
    while (p < fim) {
        unsigned ch = prox_letra(&p, fim);
        if (ch == '\n') { yy += 16; xx = x; continue; }
        if (fundo) retangulo(s, xx, yy, 8, 16, cf);
        if (xx < s->cx1 && xx + 8 > s->cx0 && yy < s->cy1 && yy + 16 > s->cy0) letra(s, xx, yy, ch, c);
        xx += 8;
        if (xx - x > maior) maior = xx - x;
    }
    sujar(s, x, y, x + maior, yy + 16);
    return MP_OBJ_NEW_SMALL_INT(maior);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sup_texto_obj, 5, 6, sup_texto);

/* imagen(x, y, img, ancho=None, alto=None, suave=False, transparente=None) */
static mp_obj_t sup_imagen(size_t n, const mp_obj_t *a, mp_map_t *kw)
{
    enum { A_ANCHO, A_ALTO, A_SUAVE, A_TRANSP };
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_ancho, MP_ARG_OBJ, { .u_rom_obj = MP_ROM_NONE } },
        { MP_QSTR_alto, MP_ARG_OBJ, { .u_rom_obj = MP_ROM_NONE } },
        { MP_QSTR_suave, MP_ARG_BOOL, { .u_bool = false } },
        { MP_QSTR_transparente, MP_ARG_OBJ, { .u_rom_obj = MP_ROM_NONE } },
    };
    if (n < 4) mp_raise_TypeError(MP_ERROR_TEXT("imagen(x, y, img, ancho=None, alto=None, suave=False, transparente=None)"));
    mp_arg_val_t v[4];
    mp_arg_parse_all(n - 4, a + 4, kw, 4, permitidos, v);
    sup_t *s = sup_de(a[0]);
    if (!mp_obj_is_type(a[3], &tipo_imagem)) mp_raise_TypeError(MP_ERROR_TEXT("se esperaba una sagui.Imagen"));
    sup_t *im = &((imagem_obj_t *)MP_OBJ_TO_PTR(a[3]))->s;
    int x = inteiro(a[1]), y = inteiro(a[2]);
    int l = v[A_ANCHO].u_obj == mp_const_none ? im->larg : inteiro(v[A_ANCHO].u_obj);
    int al = v[A_ALTO].u_obj == mp_const_none ? im->alt : inteiro(v[A_ALTO].u_obj);
    bool transp = v[A_TRANSP].u_obj != mp_const_none;
    uint32_t ct = transp ? cor_de(v[A_TRANSP].u_obj) : 0;
    if (l <= 0 || al <= 0 || im->larg <= 0) return mp_const_none;
    if (v[A_SUAVE].u_bool && !transp && (l != im->larg || al != im->alt)) {
        ponte_imagem_desenhar(s->px, s->larg, s->cx0, s->cy0, s->cx1 - s->cx0, s->cy1 - s->cy0,
                              im->larg, im->alt, im->px, x, y, l, al, 1);
    } else {
        int x0 = x < s->cx0 ? s->cx0 : x, x1 = x + l > s->cx1 ? s->cx1 : x + l;
        int y0 = y < s->cy0 ? s->cy0 : y, y1 = y + al > s->cy1 ? s->cy1 : y + al;
        bool igual = l == im->larg && al == im->alt;
        for (int yy = y0; yy < y1; yy++) {
            int sy = igual ? yy - y : (yy - y) * im->alt / al;
            const uint32_t *o = im->px + sy * im->larg;
            uint32_t *d = s->px + yy * s->larg;
            if (igual && !transp) { memcpy(d + x0, o + (x0 - x), (size_t)(x1 - x0) * 4); continue; }
            for (int xx = x0; xx < x1; xx++) {
                uint32_t c = o[igual ? xx - x : (xx - x) * im->larg / l] & 0xFFFFFF;
                if (!transp || c != ct) d[xx] = c;
            }
        }
    }
    sujar(s, x, y, x + l, y + al);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(sup_imagen_obj, 4, sup_imagen);

/* copiar_area(x, y, ancho, alto, dx, dy): copia un trozo de la propia
 * superficie a (dx, dy) (puede solaparse: desplazar el contenido). */
static mp_obj_t sup_copiar_area(size_t n, const mp_obj_t *a)
{
    (void)n;
    sup_t *s = sup_de(a[0]);
    int x = inteiro(a[1]), y = inteiro(a[2]), l = inteiro(a[3]), al = inteiro(a[4]);
    int dx = inteiro(a[5]), dy = inteiro(a[6]);
    /* recorta el origen en la superficie y el destino en el recorte */
    if (x < 0) { l += x; dx -= x; x = 0; }
    if (y < 0) { al += y; dy -= y; y = 0; }
    if (x + l > s->larg) l = s->larg - x;
    if (y + al > s->alt) al = s->alt - y;
    if (dx < s->cx0) { int k = s->cx0 - dx; l -= k; x += k; dx = s->cx0; }
    if (dy < s->cy0) { int k = s->cy0 - dy; al -= k; y += k; dy = s->cy0; }
    if (dx + l > s->cx1) l = s->cx1 - dx;
    if (dy + al > s->cy1) al = s->cy1 - dy;
    if (l <= 0 || al <= 0) return mp_const_none;
    if (dy <= y) for (int k = 0; k < al; k++) memmove(s->px + (dy + k) * s->larg + dx, s->px + (y + k) * s->larg + x, (size_t)l * 4);
    else for (int k = al - 1; k >= 0; k--) memmove(s->px + (dy + k) * s->larg + dx, s->px + (y + k) * s->larg + x, (size_t)l * 4);
    sujar(s, dx, dy, dx + l, dy + al);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sup_copiar_area_obj, 7, 7, sup_copiar_area);

/* limpiar(color=fondo del sistema): pinta todo lo que está en el recorte. */
static mp_obj_t sup_limpiar(size_t n, const mp_obj_t *a)
{
    sup_t *s = sup_de(a[0]);
    uint32_t c = n > 1 ? cor_de(a[1]) : COR_FUNDO;
    retangulo(s, s->cx0, s->cy0, s->cx1 - s->cx0, s->cy1 - s->cy0, c);
    sujar(s, s->cx0, s->cy0, s->cx1, s->cy1);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sup_limpiar_obj, 1, 2, sup_limpiar);

/* recorte(x, y, ancho, alto) limita el dibujo; recorte() vuelve a la
 * superficie entera. Devuelve el recorte anterior como (x, y, ancho, alto). */
static mp_obj_t sup_recorte(size_t n, const mp_obj_t *a)
{
    sup_t *s = sup_de(a[0]);
    mp_obj_t ant[4] = { MP_OBJ_NEW_SMALL_INT(s->cx0), MP_OBJ_NEW_SMALL_INT(s->cy0),
                        MP_OBJ_NEW_SMALL_INT(s->cx1 - s->cx0), MP_OBJ_NEW_SMALL_INT(s->cy1 - s->cy0) };
    if (n == 1 || (n == 2 && a[1] == mp_const_none)) {
        s->cx0 = s->cy0 = 0;
        s->cx1 = s->larg;
        s->cy1 = s->alt;
    } else {
        int x, y, l, al;
        if (n == 2) {
            size_t k;
            mp_obj_t *v;
            mp_obj_get_array_fixed_n(a[1], 4, &v);
            (void)k;
            x = inteiro(v[0]); y = inteiro(v[1]); l = inteiro(v[2]); al = inteiro(v[3]);
        } else {
            if (n != 5) mp_raise_TypeError(MP_ERROR_TEXT("recorte(x, y, ancho, alto) o recorte()"));
            x = inteiro(a[1]); y = inteiro(a[2]); l = inteiro(a[3]); al = inteiro(a[4]);
        }
        int x1 = x + l, y1 = y + al;
        s->cx0 = x < 0 ? 0 : x > s->larg ? s->larg : x;
        s->cy0 = y < 0 ? 0 : y > s->alt ? s->alt : y;
        s->cx1 = x1 < s->cx0 ? s->cx0 : x1 > s->larg ? s->larg : x1;
        s->cy1 = y1 < s->cy0 ? s->cy0 : y1 > s->alt ? s->alt : y1;
    }
    return mp_obj_new_tuple(4, ant);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(sup_recorte_obj, 1, 5, sup_recorte);

static mp_obj_t sup_tamano(mp_obj_t o)
{
    sup_t *s = sup_de(o);
    mp_obj_t t[2] = { MP_OBJ_NEW_SMALL_INT(s->larg), MP_OBJ_NEW_SMALL_INT(s->alt) };
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_1(sup_tamano_obj, sup_tamano);

/* ------------------------------------------------------------ eventos */

typedef struct {
    mp_obj_base_t base;
    evento_t e;
    int larg, alt;
    mp_obj_t janela;
} evento_obj_t;

/* Eventos guardados mientras se esperaba otra cosa (la respuesta de un
 * menú): entregados antes de los nuevos. */
#define FILA 64
static evento_t fila[FILA];
static int fila_ini, fila_fim;

static void guardar(const evento_t *e)
{
    int prox = (fila_fim + 1) % FILA;
    if (prox == fila_ini) return;   /* llena: el más nuevo se pierde */
    fila[fila_fim] = *e;
    fila_fim = prox;
}

static bool tirar_guardado(int id, evento_t *e)
{
    for (int i = fila_ini; i != fila_fim; i = (i + 1) % FILA) {
        if (id && fila[i].janela != id) continue;
        *e = fila[i];
        for (int k = i; (k + 1) % FILA != fila_fim; k = (k + 1) % FILA) fila[k] = fila[(k + 1) % FILA];
        fila_fim = (fila_fim + FILA - 1) % FILA;
        return true;
    }
    return false;
}

static mp_obj_t lista_janelas(void)
{
    if (MP_STATE_VM(sagui_janelas) == MP_OBJ_NULL) MP_STATE_VM(sagui_janelas) = mp_obj_new_list(0, NULL);
    return MP_STATE_VM(sagui_janelas);
}

static janela_obj_t *janela_do_id(int id)
{
    mp_obj_list_t *l = MP_OBJ_TO_PTR(lista_janelas());
    for (size_t i = 0; i < l->len; i++) {
        janela_obj_t *j = MP_OBJ_TO_PTR(l->items[i]);
        if (j->id == id) return j;
    }
    return NULL;
}

/* Convierte un evento del kernel; la petición de otro tamaño ya se atiende
 * aquí (los píxeles cambian de lugar y de tamaño, el recorte vuelve a todo). */
static mp_obj_t novo_evento(const evento_t *e)
{
    evento_obj_t *o = mp_obj_malloc(evento_obj_t, &tipo_evento);
    o->e = *e;
    janela_obj_t *j = janela_do_id(e->janela);
    o->janela = j ? MP_OBJ_FROM_PTR(j) : mp_const_none;
    if (j && e->tipo == EV_REDIMENSIONAR && e->x > 0 && e->y > 0) {
        uint32_t *px;
        if (redimensionar_janela(j->id, e->x, e->y, &px) == 0) sup_iniciar(&j->s, px, e->x, e->y);
    }
    if (e->tipo == EV_TELA) { o->larg = e->x; o->alt = e->y; }
    else if (j) { o->larg = j->s.larg; o->alt = j->s.alt; }
    else { o->larg = o->alt = 0; }
    return MP_OBJ_FROM_PTR(o);
}

/* Plazo en ms: None = sin plazo (-1). */
static int prazo_de(mp_obj_t o)
{
    if (o == mp_const_none) return -1;
    int ms = mp_obj_is_float(o) ? (int)mp_obj_get_float(o) : (int)mp_obj_get_int(o);
    return ms < 0 ? 0 : ms;
}

static mp_obj_t esperar_um(int id, int ms)
{
    evento_t e;
    if (tirar_guardado(id, &e)) return novo_evento(&e);
    int r = esperar_evento(id, &e, ms);
    if (r == 1) return novo_evento(&e);
    if (r < 0 && r != ERRO_INVALIDO) sagui_levantar(r, NULL);
    if (r < 0 && ms < 0) mp_raise_ValueError(MP_ERROR_TEXT("no hay ventanas abiertas"));
    return mp_const_none;
}

/* sagui.evento(timeout_ms=None): el próximo evento de cualquier ventana del
 * programa, o None si el plazo pasó. */
static mp_obj_t mod_evento(size_t n, const mp_obj_t *a)
{
    return esperar_um(0, n > 0 ? prazo_de(a[0]) : -1);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_evento_obj, 0, 1, mod_evento);

static void evento_attr(mp_obj_t self, qstr q, mp_obj_t *dest)
{
    if (dest[0] != MP_OBJ_NULL) return;   /* solo lectura */
    evento_obj_t *o = MP_OBJ_TO_PTR(self);
    const evento_t *e = &o->e;
    switch (q) {
    case MP_QSTR_tipo:     dest[0] = MP_OBJ_NEW_SMALL_INT(e->tipo); break;
    case MP_QSTR_x:        dest[0] = MP_OBJ_NEW_SMALL_INT(e->x); break;
    case MP_QSTR_y:        dest[0] = MP_OBJ_NEW_SMALL_INT(e->y); break;
    case MP_QSTR_botones:  dest[0] = MP_OBJ_NEW_SMALL_INT(e->botoes); break;
    case MP_QSTR_tecla:    dest[0] = MP_OBJ_NEW_SMALL_INT(e->tipo == EV_TECLA || e->tipo == EV_MENU ? e->tecla : 0); break;
    case MP_QSTR_cliques:  dest[0] = MP_OBJ_NEW_SMALL_INT((int)e->cliques); break;
    case MP_QSTR_mods:     dest[0] = MP_OBJ_NEW_SMALL_INT(e->modificadores); break;
    case MP_QSTR_ancho:    dest[0] = MP_OBJ_NEW_SMALL_INT(o->larg); break;
    case MP_QSTR_alto:     dest[0] = MP_OBJ_NEW_SMALL_INT(o->alt); break;
    case MP_QSTR_ventana:  dest[0] = o->janela; break;
    case MP_QSTR_caracter:
        /* el carácter de la tecla como str ('' para flechas, F1...) */
        if (e->tipo == EV_TECLA && e->tecla < 0x100) {
            vstr_t v;
            vstr_init(&v, 4);
            char c = (char)e->tecla;
            sagui_latin1_para_vstr(&v, &c, 1);
            dest[0] = mp_obj_new_str_from_vstr(&v);
        } else dest[0] = MP_OBJ_NEW_QSTR(MP_QSTR_);
        break;
    default: break;
    }
}

static void evento_imprimir(const mp_print_t *p, mp_obj_t self, mp_print_kind_t k)
{
    (void)k;
    evento_obj_t *o = MP_OBJ_TO_PTR(self);
    static const char *const nomes[] = { "?", "CLIC", "SOLTAR", "MOVER", "TECLA", "CERRAR", "?", "?", "?", "?",
                                         "REDIMENSIONAR", "RUEDA", "MENU", "?", "?", "?", "PANTALLA" };
    unsigned t = o->e.tipo;
    mp_printf(p, "<Evento %s x=%d y=%d botones=%u tecla=%u cliques=%d>", t < 17 ? nomes[t] : "?",
              o->e.x, o->e.y, o->e.botoes, o->e.tecla, (int)o->e.cliques);
}

static MP_DEFINE_CONST_OBJ_TYPE(tipo_evento, MP_QSTR_Evento, MP_TYPE_FLAG_NONE,
    attr, evento_attr, print, evento_imprimir);

/* ------------------------------------------------------------ Ventana */

static janela_obj_t *janela_aberta(mp_obj_t o)
{
    janela_obj_t *j = MP_OBJ_TO_PTR(o);
    if (j->id < 0) mp_raise_ValueError(MP_ERROR_TEXT("ventana cerrada"));
    return j;
}

/* Ventana(ancho, alto, titulo="", redimensionable=True, dialogo=False, minimo=None) */
static mp_obj_t janela_nova(const mp_obj_type_t *tipo, size_t n, size_t nkw, const mp_obj_t *args)
{
    enum { A_ANCHO, A_ALTO, A_TITULO, A_REDIM, A_DIALOGO, A_MINIMO };
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_ancho, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_alto, MP_ARG_REQUIRED | MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_titulo, MP_ARG_OBJ, { .u_rom_obj = MP_ROM_QSTR(MP_QSTR_) } },
        { MP_QSTR_redimensionable, MP_ARG_BOOL, { .u_bool = true } },
        { MP_QSTR_dialogo, MP_ARG_BOOL, { .u_bool = false } },
        { MP_QSTR_minimo, MP_ARG_OBJ, { .u_rom_obj = MP_ROM_NONE } },
    };
    mp_arg_val_t v[6];
    mp_arg_parse_all_kw_array(n, nkw, args, 6, permitidos, v);
    int l = v[A_ANCHO].u_int, a = v[A_ALTO].u_int;
    if (l < 16 || a < 16 || l > JANELA_LARG_MAX || a > JANELA_ALT_MAX)
        mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("tamaño de ventana entre 16x16 y %dx%d"),
                          JANELA_LARG_MAX, JANELA_ALT_MAX);
    char titulo[64];
    sagui_str_para_latin1(v[A_TITULO].u_obj, titulo, sizeof titulo);
    janela_obj_t *j = mp_obj_malloc(janela_obj_t, tipo);
    uint32_t *px;
    j->id = criar_janela(titulo, l, a, &px);
    if (j->id < 0) sagui_levantar(j->id, "Ventana");
    sup_iniciar(&j->s, px, l, a);
    j->titulo = v[A_TITULO].u_obj;
    j->estilo = (v[A_REDIM].u_bool ? JAN_REDIMENSIONAVEL : 0) | (v[A_DIALOGO].u_bool ? JAN_DIALOGO : 0);
    j->min_l = 64;
    j->min_a = 32;
    if (v[A_MINIMO].u_obj != mp_const_none) {
        mp_obj_t *m;
        mp_obj_get_array_fixed_n(v[A_MINIMO].u_obj, 2, &m);
        j->min_l = mp_obj_get_int(m[0]);
        j->min_a = mp_obj_get_int(m[1]);
    }
    if (j->estilo) estilo_janela(j->id, j->estilo, j->min_l, j->min_a);
    mp_obj_list_append(lista_janelas(), MP_OBJ_FROM_PTR(j));
    return MP_OBJ_FROM_PTR(j);
}

/* actualizar(todo=False): envía al kernel solo el área sucia (o todo). */
static mp_obj_t janela_actualizar(size_t n, const mp_obj_t *a)
{
    janela_obj_t *j = janela_aberta(a[0]);
    sup_t *s = &j->s;
    if (n > 1 && mp_obj_is_true(a[1])) atualizar_janela(j->id, NULL);
    else if (s->sx0 < s->sx1) {
        retangulo_t r = { s->sx0, s->sy0, s->sx1 - s->sx0, s->sy1 - s->sy0 };
        atualizar_janela(j->id, &r);
    }
    s->sx0 = s->sy0 = s->sx1 = s->sy1 = 0;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(janela_actualizar_obj, 1, 2, janela_actualizar);

/* titulo() -> str; titulo(s) */
static mp_obj_t janela_titulo(size_t n, const mp_obj_t *a)
{
    janela_obj_t *j = janela_aberta(a[0]);
    if (n == 1) return j->titulo;
    char t[64];
    sagui_str_para_latin1(a[1], t, sizeof t);
    titulo_janela(j->id, t);
    j->titulo = a[1];
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(janela_titulo_obj, 1, 2, janela_titulo);

static mp_obj_t janela_cerrar(mp_obj_t o)
{
    janela_obj_t *j = MP_OBJ_TO_PTR(o);
    if (j->id < 0) return mp_const_none;
    fechar_janela(j->id);
    /* los eventos guardados de la ventana se pierden */
    evento_t e;
    while (tirar_guardado(j->id, &e)) ;
    j->id = -1;
    j->s.px = NULL;
    j->s.larg = j->s.alt = 0;
    mp_obj_t lista = lista_janelas();
    mp_obj_list_remove(lista, o);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(janela_cerrar_obj, janela_cerrar);

static mp_obj_t janela_abierta(mp_obj_t o)
{
    return mp_obj_new_bool(((janela_obj_t *)MP_OBJ_TO_PTR(o))->id >= 0);
}
static MP_DEFINE_CONST_FUN_OBJ_1(janela_abierta_obj, janela_abierta);

/* evento(timeout_ms=None): el próximo evento de esta ventana. */
static mp_obj_t janela_evento(size_t n, const mp_obj_t *a)
{
    janela_obj_t *j = janela_aberta(a[0]);
    return esperar_um(j->id, n > 1 ? prazo_de(a[1]) : -1);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(janela_evento_obj, 1, 2, janela_evento);

/* Ítems del menú: str (con la marcación del kernel: "-", "~Texto", "*Texto",
 * "Texto\tAtalho") o (texto, [subítems]) para un submenú (D064). */
static void montar_menu(vstr_t *v, mp_obj_t itens, int *linhas)
{
    size_t n;
    mp_obj_t *it;
    mp_obj_get_array(itens, &n, &it);
    for (size_t i = 0; i < n; i++) {
        mp_obj_t texto = it[i], sub = MP_OBJ_NULL;
        if (!mp_obj_is_str(it[i])) {
            mp_obj_t *par;
            mp_obj_get_array_fixed_n(it[i], 2, &par);
            texto = par[0];
            sub = par[1];
        }
        char t[MENU_MAX_TEXTO + 8];
        size_t k = 0;
        if (sub != MP_OBJ_NULL) t[k++] = '>';
        k += sagui_str_para_latin1(texto, t + k, sizeof t - k - 1);
        for (size_t c = 0; c < k; c++) if (t[c] == '\n') t[c] = ' ';
        vstr_add_strn(v, t, k);
        vstr_add_byte(v, '\n');
        (*linhas)++;
        if (sub != MP_OBJ_NULL) {
            montar_menu(v, sub, linhas);
            vstr_add_str(v, "<\n");
            (*linhas)++;
        }
    }
}

/* menu_contextual(x, y, items) -> índice de la línea elegida (contando todas,
 * como el kernel: separadores, ">" y "<" inclusive) o None. Espera la
 * respuesta; los otros eventos que lleguen en ese intervalo quedan
 * guardados para el próximo evento(). */
static mp_obj_t janela_menu(size_t n, const mp_obj_t *a)
{
    (void)n;
    janela_obj_t *j = janela_aberta(a[0]);
    static int codigo = 0x3300;
    vstr_t v;
    vstr_init(&v, 128);
    int linhas = 0;
    montar_menu(&v, a[3], &linhas);
    if (linhas > MENU_MAX_ITENS) mp_raise_ValueError(MP_ERROR_TEXT("demasiadas líneas en el menú"));
    if (v.len) v.len--;   /* sin el último '\n' */
    int cod = ++codigo;
    int r = menu_contextual(j->id, inteiro(a[1]), inteiro(a[2]), vstr_null_terminated_str(&v), cod);
    vstr_clear(&v);
    if (r < 0) return mp_const_none;   /* ventana sin foco, por ejemplo */
    for (;;) {
        evento_t e;
        int k = esperar_evento(j->id, &e, -1);
        if (k != 1) return mp_const_none;
        if (e.tipo == EV_MENU && (int)e.tecla == cod) return e.x < 0 ? mp_const_none : MP_OBJ_NEW_SMALL_INT(e.x);
        guardar(&e);
    }
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(janela_menu_obj, 4, 4, janela_menu);

/* teclas() -> bytes(32) con las teclas pulsadas (códigos de barrido,
 * CV_* en abi.h), o None si la ventana no está en foco. Para juegos. */
static mp_obj_t janela_teclas(mp_obj_t o)
{
    janela_aberta(o);
    unsigned char mapa[32];
    if (teclas_seguradas(mapa) != 1) return mp_const_none;
    return mp_obj_new_bytes(mapa, 32);
}
static MP_DEFINE_CONST_FUN_OBJ_1(janela_teclas_obj, janela_teclas);

/* pantalla_completa(sí/no) */
static mp_obj_t janela_tela_cheia(mp_obj_t o, mp_obj_t ligar)
{
    janela_obj_t *j = janela_aberta(o);
    unsigned e = j->estilo | (mp_obj_is_true(ligar) ? JAN_TELA_CHEIA | JAN_TELA_CHEIA_OK : 0);
    return mp_obj_new_bool(estilo_janela(j->id, e, j->min_l, j->min_a) == 0);
}
static MP_DEFINE_CONST_FUN_OBJ_2(janela_tela_cheia_obj, janela_tela_cheia);

/* estilo(redimensionable=None, minimo=None): cambia después de creada. */
static mp_obj_t janela_estilo(size_t n, const mp_obj_t *a, mp_map_t *kw)
{
    enum { A_REDIM, A_MINIMO };
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_redimensionable, MP_ARG_OBJ, { .u_rom_obj = MP_ROM_NONE } },
        { MP_QSTR_minimo, MP_ARG_OBJ, { .u_rom_obj = MP_ROM_NONE } },
    };
    mp_arg_val_t v[2];
    mp_arg_parse_all(n - 1, a + 1, kw, 2, permitidos, v);
    janela_obj_t *j = janela_aberta(a[0]);
    if (v[A_REDIM].u_obj != mp_const_none) {
        if (mp_obj_is_true(v[A_REDIM].u_obj)) j->estilo |= JAN_REDIMENSIONAVEL;
        else j->estilo &= ~JAN_REDIMENSIONAVEL;
    }
    if (v[A_MINIMO].u_obj != mp_const_none) {
        mp_obj_t *m;
        mp_obj_get_array_fixed_n(v[A_MINIMO].u_obj, 2, &m);
        j->min_l = mp_obj_get_int(m[0]);
        j->min_a = mp_obj_get_int(m[1]);
    }
    estilo_janela(j->id, j->estilo, j->min_l, j->min_a);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_KW(janela_estilo_obj, 1, janela_estilo);

/* redimensionar(ancho, alto): pide otro tamaño al kernel (el propio
 * programa, no el usuario). */
static mp_obj_t janela_redimensionar(mp_obj_t o, mp_obj_t ol, mp_obj_t oa)
{
    janela_obj_t *j = janela_aberta(o);
    int l = mp_obj_get_int(ol), a = mp_obj_get_int(oa);
    if (l < 16 || a < 16 || l > JANELA_LARG_MAX || a > JANELA_ALT_MAX) mp_raise_ValueError(MP_ERROR_TEXT("tamaño no válido"));
    uint32_t *px;
    int r = redimensionar_janela(j->id, l, a, &px);
    if (r < 0) sagui_levantar(r, NULL);
    sup_iniciar(&j->s, px, l, a);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(janela_redimensionar_obj, janela_redimensionar);

static void janela_attr(mp_obj_t self, qstr q, mp_obj_t *dest)
{
    janela_obj_t *j = MP_OBJ_TO_PTR(self);
    if (dest[0] == MP_OBJ_NULL) {
        if (q == MP_QSTR_id) { dest[0] = MP_OBJ_NEW_SMALL_INT(j->id); return; }
        if (q == MP_QSTR_ancho) { dest[0] = MP_OBJ_NEW_SMALL_INT(j->s.larg); return; }
        if (q == MP_QSTR_alto) { dest[0] = MP_OBJ_NEW_SMALL_INT(j->s.alt); return; }
    }
    dest[1] = MP_OBJ_SENTINEL;   /* el resto: los métodos */
}

static void janela_imprimir(const mp_print_t *p, mp_obj_t self, mp_print_kind_t k)
{
    (void)k;
    janela_obj_t *j = MP_OBJ_TO_PTR(self);
    mp_printf(p, "<Ventana %d %dx%d%s>", j->id, j->s.larg, j->s.alt, j->id < 0 ? " cerrada" : "");
}

static const mp_rom_map_elem_t janela_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR_pixel), MP_ROM_PTR(&sup_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_linea), MP_ROM_PTR(&sup_linea_obj) },
    { MP_ROM_QSTR(MP_QSTR_rect), MP_ROM_PTR(&sup_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_elipse), MP_ROM_PTR(&sup_elipse_obj) },
    { MP_ROM_QSTR(MP_QSTR_poligono), MP_ROM_PTR(&sup_poligono_obj) },
    { MP_ROM_QSTR(MP_QSTR_texto), MP_ROM_PTR(&sup_texto_obj) },
    { MP_ROM_QSTR(MP_QSTR_imagen), MP_ROM_PTR(&sup_imagen_obj) },
    { MP_ROM_QSTR(MP_QSTR_copiar_area), MP_ROM_PTR(&sup_copiar_area_obj) },
    { MP_ROM_QSTR(MP_QSTR_limpiar), MP_ROM_PTR(&sup_limpiar_obj) },
    { MP_ROM_QSTR(MP_QSTR_recorte), MP_ROM_PTR(&sup_recorte_obj) },
    { MP_ROM_QSTR(MP_QSTR_tamano), MP_ROM_PTR(&sup_tamano_obj) },
    { MP_ROM_QSTR(MP_QSTR_actualizar), MP_ROM_PTR(&janela_actualizar_obj) },
    { MP_ROM_QSTR(MP_QSTR_titulo), MP_ROM_PTR(&janela_titulo_obj) },
    { MP_ROM_QSTR(MP_QSTR_cerrar), MP_ROM_PTR(&janela_cerrar_obj) },
    { MP_ROM_QSTR(MP_QSTR_abierta), MP_ROM_PTR(&janela_abierta_obj) },
    { MP_ROM_QSTR(MP_QSTR_evento), MP_ROM_PTR(&janela_evento_obj) },
    { MP_ROM_QSTR(MP_QSTR_menu_contextual), MP_ROM_PTR(&janela_menu_obj) },
    { MP_ROM_QSTR(MP_QSTR_teclas), MP_ROM_PTR(&janela_teclas_obj) },
    { MP_ROM_QSTR(MP_QSTR_pantalla_completa), MP_ROM_PTR(&janela_tela_cheia_obj) },
    { MP_ROM_QSTR(MP_QSTR_estilo), MP_ROM_PTR(&janela_estilo_obj) },
    { MP_ROM_QSTR(MP_QSTR_redimensionar), MP_ROM_PTR(&janela_redimensionar_obj) },
};
static MP_DEFINE_CONST_DICT(janela_metodos, janela_metodos_tab);

static MP_DEFINE_CONST_OBJ_TYPE(tipo_janela, MP_QSTR_Ventana, MP_TYPE_FLAG_NONE,
    make_new, janela_nova, print, janela_imprimir, attr, janela_attr, locals_dict, &janela_metodos);

/* ------------------------------------------------------------ Imagen */

static imagem_obj_t *criar_imagem(int l, int a)
{
    if (l <= 0 || a <= 0 || l > 8192 || a > 8192 || (int64_t)l * a > 16 * 1024 * 1024)
        mp_raise_ValueError(MP_ERROR_TEXT("tamaño de imagen no válido"));
    imagem_obj_t *im = mp_obj_malloc(imagem_obj_t, &tipo_imagem);
    uint32_t *px = m_new(uint32_t, (size_t)l * a);
    sup_iniciar(&im->s, px, l, a);
    im->formato[0] = 0;
    return im;
}

/* Imagen.nueva(ancho, alto, color=0xFFFFFF) */
static mp_obj_t imagem_nova(size_t n, const mp_obj_t *a)
{
    imagem_obj_t *im = criar_imagem(inteiro(a[0]), inteiro(a[1]));
    uint32_t c = n > 2 ? cor_de(a[2]) : 0xFFFFFF;
    preencher32(im->s.px, c, (size_t)im->s.larg * im->s.alt);
    return MP_OBJ_FROM_PTR(im);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(imagem_nova_fun, 2, 3, imagem_nova);
static MP_DEFINE_CONST_STATICMETHOD_OBJ(imagem_nova_obj, MP_ROM_PTR(&imagem_nova_fun));

static NORETURN void erro_imagem(int r, mp_obj_t caminho)
{
    if (r > -200) sagui_levantar_obj(r, caminho);   /* error de archivo del sistema */
    mp_obj_t msg = sagui_str_de_latin1(ponte_imagem_erro(r));
    mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("%s: %s"), mp_obj_str_get_str(msg), mp_obj_str_get_str(caminho));
}

/* Imagen.cargar(ruta): BMP, PPM/PGM/PBM o PNG */
static mp_obj_t imagem_carregar(mp_obj_t caminho)
{
    char c[256];
    sagui_str_para_latin1(caminho, c, sizeof c);
    int l, a;
    unsigned *px;
    char formato[16];
    int r = ponte_imagem_ler(c, &l, &a, &px, formato, sizeof formato);
    if (r != 0) erro_imagem(r, caminho);
    imagem_obj_t *im;
    nlr_buf_t nlr;
    if (nlr_push(&nlr) == 0) {   /* sin memoria en el heap: libera los píxeles de libsagui antes */
        im = criar_imagem(l, a);
        nlr_pop();
    } else {
        libera(px);
        nlr_jump(nlr.ret_val);
    }
    memcpy(im->s.px, px, (size_t)l * a * 4);
    libera(px);
    memcpy(im->formato, formato, sizeof formato);
    return MP_OBJ_FROM_PTR(im);
}
static MP_DEFINE_CONST_FUN_OBJ_1(imagem_carregar_fun, imagem_carregar);
static MP_DEFINE_CONST_STATICMETHOD_OBJ(imagem_carregar_obj, MP_ROM_PTR(&imagem_carregar_fun));

/* guardar(ruta): BMP de 24 bits (PPM si la ruta acaba en .ppm) */
static mp_obj_t imagem_guardar(mp_obj_t o, mp_obj_t caminho)
{
    imagem_obj_t *im = MP_OBJ_TO_PTR(o);
    char c[256];
    size_t n = sagui_str_para_latin1(caminho, c, sizeof c);
    bool ppm = n > 4 && (!strcmp(c + n - 4, ".ppm") || !strcmp(c + n - 4, ".PPM"));
    int r = ponte_imagem_gravar(c, im->s.larg, im->s.alt, im->s.px, ppm);
    if (r != 0) erro_imagem(r, caminho);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(imagem_guardar_obj, imagem_guardar);

/* escalar(ancho, alto, suave=False) -> Imagen nueva */
static mp_obj_t imagem_escalar(size_t n, const mp_obj_t *a)
{
    imagem_obj_t *im = MP_OBJ_TO_PTR(a[0]);
    int l = inteiro(a[1]), al = inteiro(a[2]);
    imagem_obj_t *nova = criar_imagem(l, al);
    ponte_imagem_desenhar(nova->s.px, l, 0, 0, l, al, im->s.larg, im->s.alt, im->s.px, 0, 0, l, al,
                          n > 3 && mp_obj_is_true(a[3]));
    memcpy(nova->formato, im->formato, sizeof nova->formato);
    return MP_OBJ_FROM_PTR(nova);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(imagem_escalar_obj, 3, 4, imagem_escalar);

/* recortar(x, y, ancho, alto) -> Imagen nueva con ese trozo */
static mp_obj_t imagem_recortar(size_t n, const mp_obj_t *a)
{
    (void)n;
    imagem_obj_t *im = MP_OBJ_TO_PTR(a[0]);
    int x = inteiro(a[1]), y = inteiro(a[2]), l = inteiro(a[3]), al = inteiro(a[4]);
    if (x < 0 || y < 0 || x + l > im->s.larg || y + al > im->s.alt) mp_raise_ValueError(MP_ERROR_TEXT("fuera de la imagen"));
    imagem_obj_t *nova = criar_imagem(l, al);
    for (int k = 0; k < al; k++) memcpy(nova->s.px + k * l, im->s.px + (y + k) * im->s.larg + x, (size_t)l * 4);
    return MP_OBJ_FROM_PTR(nova);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(imagem_recortar_obj, 5, 5, imagem_recortar);

static void imagem_attr(mp_obj_t self, qstr q, mp_obj_t *dest)
{
    imagem_obj_t *im = MP_OBJ_TO_PTR(self);
    if (dest[0] == MP_OBJ_NULL) {
        if (q == MP_QSTR_ancho) { dest[0] = MP_OBJ_NEW_SMALL_INT(im->s.larg); return; }
        if (q == MP_QSTR_alto) { dest[0] = MP_OBJ_NEW_SMALL_INT(im->s.alt); return; }
        if (q == MP_QSTR_formato) { dest[0] = mp_obj_new_str(im->formato, strlen(im->formato)); return; }
    }
    dest[1] = MP_OBJ_SENTINEL;
}

/* memoryview(img): los píxeles como enteros de 32 bits 0x00RRGGBB, para
 * montar un cuadro entero en Python y dibujar con ventana.imagen(). */
static mp_int_t imagem_buffer(mp_obj_t o, mp_buffer_info_t *b, mp_uint_t flags)
{
    (void)flags;
    imagem_obj_t *im = MP_OBJ_TO_PTR(o);
    b->buf = im->s.px;
    b->len = (size_t)im->s.larg * im->s.alt * 4;
    b->typecode = 'I';
    return 0;
}

static void imagem_imprimir(const mp_print_t *p, mp_obj_t self, mp_print_kind_t k)
{
    (void)k;
    imagem_obj_t *im = MP_OBJ_TO_PTR(self);
    mp_printf(p, "<Imagen %dx%d%s%s>", im->s.larg, im->s.alt, im->formato[0] ? " " : "", im->formato);
}

static const mp_rom_map_elem_t imagem_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR_cargar), MP_ROM_PTR(&imagem_carregar_obj) },
    { MP_ROM_QSTR(MP_QSTR_nueva), MP_ROM_PTR(&imagem_nova_obj) },
    { MP_ROM_QSTR(MP_QSTR_guardar), MP_ROM_PTR(&imagem_guardar_obj) },
    { MP_ROM_QSTR(MP_QSTR_escalar), MP_ROM_PTR(&imagem_escalar_obj) },
    { MP_ROM_QSTR(MP_QSTR_recortar), MP_ROM_PTR(&imagem_recortar_obj) },
    { MP_ROM_QSTR(MP_QSTR_pixel), MP_ROM_PTR(&sup_pixel_obj) },
    { MP_ROM_QSTR(MP_QSTR_linea), MP_ROM_PTR(&sup_linea_obj) },
    { MP_ROM_QSTR(MP_QSTR_rect), MP_ROM_PTR(&sup_rect_obj) },
    { MP_ROM_QSTR(MP_QSTR_elipse), MP_ROM_PTR(&sup_elipse_obj) },
    { MP_ROM_QSTR(MP_QSTR_poligono), MP_ROM_PTR(&sup_poligono_obj) },
    { MP_ROM_QSTR(MP_QSTR_texto), MP_ROM_PTR(&sup_texto_obj) },
    { MP_ROM_QSTR(MP_QSTR_imagen), MP_ROM_PTR(&sup_imagen_obj) },
    { MP_ROM_QSTR(MP_QSTR_copiar_area), MP_ROM_PTR(&sup_copiar_area_obj) },
    { MP_ROM_QSTR(MP_QSTR_limpiar), MP_ROM_PTR(&sup_limpiar_obj) },
    { MP_ROM_QSTR(MP_QSTR_recorte), MP_ROM_PTR(&sup_recorte_obj) },
    { MP_ROM_QSTR(MP_QSTR_tamano), MP_ROM_PTR(&sup_tamano_obj) },
};
static MP_DEFINE_CONST_DICT(imagem_metodos, imagem_metodos_tab);

static MP_DEFINE_CONST_OBJ_TYPE(tipo_imagem, MP_QSTR_Imagen, MP_TYPE_FLAG_NONE,
    print, imagem_imprimir, attr, imagem_attr, buffer, imagem_buffer, locals_dict, &imagem_metodos);

/* ------------------------------------------------------------ funciones del módulo */

static mp_obj_t mod_ticks_ms(void) { return mp_obj_new_int_from_uint(tempo_ms()); }
static MP_DEFINE_CONST_FUN_OBJ_0(mod_ticks_ms_obj, mod_ticks_ms);

static mp_obj_t mod_dormir(mp_obj_t ms)
{
    int k = prazo_de(ms);
    if (k > 0) SEM_GIL(dormir((unsigned)k));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mod_dormir_obj, mod_dormir);

/* pantalla() -> (ancho, alto) de la pantalla */
static mp_obj_t mod_tela(void)
{
    info_tela_t t;
    memset(&t, 0, sizeof t);
    info_sistema(INFO_TELA, &t, sizeof t);
    mp_obj_t v[2] = { MP_OBJ_NEW_SMALL_INT(t.largura), MP_OBJ_NEW_SMALL_INT(t.altura) };
    return mp_obj_new_tuple(2, v);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mod_tela_obj, mod_tela);

/* bip(frecuencia, ms): vuelve enseguida; el kernel suena en segundo plano. */
static mp_obj_t mod_bip(mp_obj_t f, mp_obj_t ms)
{
    nota_t n = { (uint16_t)mp_obj_get_int(f), (uint16_t)mp_obj_get_int(ms) };
    tocar(&n, 1, BIP_SUBSTITUIR);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(mod_bip_obj, mod_bip);

/* ------------------------------------------------------------ Sonido (D088-D090)
 *
 *   s = sagui.Sonido(22050, canales=1, bits=16)   # OSError si no hay tarjeta
 *   s.escribir(datos)          # bytes/bytearray/array: PCM; espera si el flujo está lleno
 *   s.escribir(datos, False)   # sin esperar: -> bytes aceptados
 *   s.volumen(50); s.pendiente() -> ms; s.cerrar(True) espera a que acabe
 *   sagui.volumen() / sagui.volumen(80): el volumen general
 * Cada Sonido es un flujo del mezclador del kernel; varios suenan a la vez. */

typedef struct {
    mp_obj_base_t base;
    int fd;
} som_obj_t;

static som_obj_t *som_aberto(mp_obj_t o)
{
    som_obj_t *s = MP_OBJ_TO_PTR(o);
    if (s->fd < 0) mp_raise_ValueError(MP_ERROR_TEXT("sonido cerrado"));
    return s;
}

static mp_obj_t som_novo(const mp_obj_type_t *tipo, size_t n, size_t nkw, const mp_obj_t *args)
{
    enum { A_TAXA, A_CANAIS, A_BITS };
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_frecuencia, MP_ARG_INT, { .u_int = 22050 } },
        { MP_QSTR_canales, MP_ARG_INT, { .u_int = 1 } },
        { MP_QSTR_bits, MP_ARG_INT, { .u_int = 16 } },
    };
    mp_arg_val_t v[3];
    mp_arg_parse_all_kw_array(n, nkw, args, 3, permitidos, v);
    int fd = audio_abrir(v[A_TAXA].u_int, v[A_CANAIS].u_int, v[A_BITS].u_int);
    if (fd == ERRO_NAO_EXISTE) mp_raise_msg(&mp_type_OSError, MP_ERROR_TEXT("no hay tarjeta de sonido"));
    if (fd < 0) mp_raise_ValueError(MP_ERROR_TEXT("formato de sonido no válido"));
    som_obj_t *s = mp_obj_malloc_with_finaliser(som_obj_t, tipo);
    s->fd = fd;
    return MP_OBJ_FROM_PTR(s);
}

static mp_obj_t som_escrever(size_t n, const mp_obj_t *a)
{
    som_obj_t *s = som_aberto(a[0]);
    mp_buffer_info_t b;
    mp_get_buffer_raise(a[1], &b, MP_BUFFER_READ);
    bool esperar = n < 3 || mp_obj_is_true(a[2]);
    int r = esperar ? audio_escrever(s->fd, b.buf, (unsigned)b.len)
                    : audio_escrever_sem_bloquear(s->fd, b.buf, (unsigned)b.len);
    return MP_OBJ_NEW_SMALL_INT(r < 0 ? 0 : r);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(som_escrever_obj, 2, 3, som_escrever);

static mp_obj_t som_volume(size_t n, const mp_obj_t *a)
{
    som_obj_t *s = som_aberto(a[0]);
    return MP_OBJ_NEW_SMALL_INT(audio_controle(s->fd, AUDIO_VOLUME, n > 1 ? mp_obj_get_int(a[1]) : -1));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(som_volume_obj, 1, 2, som_volume);

static mp_obj_t som_pendente(mp_obj_t o)
{
    return MP_OBJ_NEW_SMALL_INT(audio_controle(som_aberto(o)->fd, AUDIO_PENDENTE, 0));
}
static MP_DEFINE_CONST_FUN_OBJ_1(som_pendente_obj, som_pendente);

/* cerrar(esperar=False); también es el __del__ y el __exit__ (args extra ignorados). */
static mp_obj_t som_fechar(size_t n, const mp_obj_t *a)
{
    som_obj_t *s = MP_OBJ_TO_PTR(a[0]);
    if (s->fd >= 0) audio_fechar(s->fd, n == 2 && mp_obj_is_true(a[1]));
    s->fd = -1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(som_fechar_obj, 1, 4, som_fechar);

static const mp_rom_map_elem_t som_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR_escribir), MP_ROM_PTR(&som_escrever_obj) },
    { MP_ROM_QSTR(MP_QSTR_volumen), MP_ROM_PTR(&som_volume_obj) },
    { MP_ROM_QSTR(MP_QSTR_pendiente), MP_ROM_PTR(&som_pendente_obj) },
    { MP_ROM_QSTR(MP_QSTR_cerrar), MP_ROM_PTR(&som_fechar_obj) },
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&som_fechar_obj) },
    { MP_ROM_QSTR(MP_QSTR___enter__), MP_ROM_PTR(&mp_identity_obj) },
    { MP_ROM_QSTR(MP_QSTR___exit__), MP_ROM_PTR(&som_fechar_obj) },
};
static MP_DEFINE_CONST_DICT(som_metodos, som_metodos_tab);

static MP_DEFINE_CONST_OBJ_TYPE(tipo_som, MP_QSTR_Sonido, MP_TYPE_FLAG_NONE,
    make_new, som_novo, locals_dict, &som_metodos);

/* volumen() -> 0-100 (el general); volumen(v) lo cambia. -1 si no hay tarjeta. */
static mp_obj_t mod_volume(size_t n, const mp_obj_t *a)
{
    int r = audio_controle(-1, AUDIO_VOLUME, n ? mp_obj_get_int(a[0]) : -1);
    return MP_OBJ_NEW_SMALL_INT(r < 0 ? -1 : r);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_volume_obj, 0, 1, mod_volume);

/* portapapeles() -> str; portapapeles(texto). Por las llamadas 94-95 si el
 * kernel las tiene; si no, solo dentro de este programa. */
MP_REGISTER_ROOT_POINTER(mp_obj_t sagui_portapapeis);

static int chamar_portapapeis(unsigned n, void *buf, unsigned tam)
{
    int r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(buf), "c"(tam), "d"(0) : "memory");
    return r;
}

static mp_obj_t mod_portapapeis(size_t n, const mp_obj_t *a)
{
    if (n == 1) {
        size_t tam;
        const char *s = mp_obj_str_get_data(a[0], &tam);
        char *lat = m_new(char, tam + 1);
        size_t k = sagui_str_para_latin1(a[0], lat, tam + 1);
        (void)s;
        int r = chamar_portapapeis(SIS_COPIAR_TEXTO_MP, lat, (unsigned)k);
        m_del(char, lat, tam + 1);
        MP_STATE_VM(sagui_portapapeis) = r == ERRO_INVALIDO ? a[0] : MP_OBJ_NULL;
        return mp_const_none;
    }
    if (MP_STATE_VM(sagui_portapapeis) != MP_OBJ_NULL) return MP_STATE_VM(sagui_portapapeis);
    char buf[256];
    int r = chamar_portapapeis(SIS_COLAR_TEXTO_MP, buf, sizeof buf);
    if (r <= 0) return MP_OBJ_NEW_QSTR(MP_QSTR_);
    vstr_t v;
    vstr_init(&v, r + 8);
    if (r <= (int)sizeof buf) sagui_latin1_para_vstr(&v, buf, r);
    else {
        char *grande = m_new(char, r);
        int k = chamar_portapapeis(SIS_COLAR_TEXTO_MP, grande, r);
        sagui_latin1_para_vstr(&v, grande, k < r ? (k < 0 ? 0 : k) : r);
        m_del(char, grande, r);
    }
    return mp_obj_new_str_from_vstr(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_portapapeis_obj, 0, 1, mod_portapapeis);

/* ------------------------------------------------------------ diálogos de archivos (D272, D297) */

/* dialogo(tipo, titulo="", filtros="", inicial=""): el diálogo de archivos de estilo Windows 95 del
 * sistema. tipo: 0 = abrir, 1 = guardar, 2 = elegir carpeta (esta no usa filtros). filtros =
 * "Texto (*.txt)|*.txt|Todos|*" (vacío: todos). Devuelve la ruta elegida o None si se cancela.
 * Una sola función y no tres porque un nombre nuevo pediría regenerar los qstr (D034); vps.py
 * le pone los nombres bonitos (dialogo_abrir, dialogo_guardar). */
static mp_obj_t mod_dialogo(size_t n, const mp_obj_t *a)
{
    int tipo = mp_obj_get_int(a[0]);
    char titulo[64] = "", filtros[256] = "", inicial[256] = "", saida[256];
    if (n > 1 && a[1] != mp_const_none) sagui_str_para_latin1(a[1], titulo, sizeof titulo);
    if (n > 2 && a[2] != mp_const_none) sagui_str_para_latin1(a[2], filtros, sizeof filtros);
    if (n > 3 && a[3] != mp_const_none) sagui_str_para_latin1(a[3], inicial, sizeof inicial);
    saida[0] = 0;
    _Bool ok;
    MP_THREAD_GIL_EXIT();
    if (tipo == 0) ok = dialogo_abrir(titulo[0] ? titulo : "Abrir", filtros[0] ? filtros : NULL, inicial, saida, sizeof saida);
    else if (tipo == 1) ok = dialogo_guardar(titulo[0] ? titulo : "Guardar como", filtros[0] ? filtros : NULL, inicial, saida, sizeof saida);
    else ok = dialogo_carpeta(titulo[0] ? titulo : "Elegir carpeta", inicial, saida, sizeof saida);
    MP_THREAD_GIL_ENTER();
    if (!ok) return mp_const_none;
    vstr_t v;
    vstr_init(&v, 64);
    sagui_latin1_para_vstr(&v, saida, strlen(saida));
    return mp_obj_new_str_from_vstr(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_dialogo_obj, 1, 4, mod_dialogo);

static const mp_rom_map_elem_t sagui_globais_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_sagui) },
    { MP_ROM_QSTR(MP_QSTR_Ventana), MP_ROM_PTR(&tipo_janela) },
    { MP_ROM_QSTR(MP_QSTR_Imagen), MP_ROM_PTR(&tipo_imagem) },
    { MP_ROM_QSTR(MP_QSTR_Evento), MP_ROM_PTR(&tipo_evento) },
    { MP_ROM_QSTR(MP_QSTR_evento), MP_ROM_PTR(&mod_evento_obj) },
    { MP_ROM_QSTR(MP_QSTR_ticks_ms), MP_ROM_PTR(&mod_ticks_ms_obj) },
    { MP_ROM_QSTR(MP_QSTR_dormir), MP_ROM_PTR(&mod_dormir_obj) },
    { MP_ROM_QSTR(MP_QSTR_pantalla), MP_ROM_PTR(&mod_tela_obj) },
    { MP_ROM_QSTR(MP_QSTR_bip), MP_ROM_PTR(&mod_bip_obj) },
    { MP_ROM_QSTR(MP_QSTR_Sonido), MP_ROM_PTR(&tipo_som) },
    { MP_ROM_QSTR(MP_QSTR_volumen), MP_ROM_PTR(&mod_volume_obj) },
    { MP_ROM_QSTR(MP_QSTR_portapapeles), MP_ROM_PTR(&mod_portapapeis_obj) },
    { MP_ROM_QSTR(MP_QSTR_dialogo), MP_ROM_PTR(&mod_dialogo_obj) },
    /* tipos de evento */
    { MP_ROM_QSTR(MP_QSTR_CLIC), MP_ROM_INT(EV_CLIQUE) },
    { MP_ROM_QSTR(MP_QSTR_SOLTAR), MP_ROM_INT(EV_SOLTOU) },
    { MP_ROM_QSTR(MP_QSTR_MOVER), MP_ROM_INT(EV_MOVER) },
    { MP_ROM_QSTR(MP_QSTR_TECLA), MP_ROM_INT(EV_TECLA) },
    { MP_ROM_QSTR(MP_QSTR_CERRAR), MP_ROM_INT(EV_FECHAR) },
    { MP_ROM_QSTR(MP_QSTR_REDIMENSIONAR), MP_ROM_INT(EV_REDIMENSIONAR) },
    { MP_ROM_QSTR(MP_QSTR_RUEDA), MP_ROM_INT(EV_RODA) },
    { MP_ROM_QSTR(MP_QSTR_MENU), MP_ROM_INT(EV_MENU) },
    { MP_ROM_QSTR(MP_QSTR_PANTALLA), MP_ROM_INT(EV_TELA) },
    /* botones y modificadores */
    { MP_ROM_QSTR(MP_QSTR_BOTON_IZQUIERDO), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_BOTON_DERECHO), MP_ROM_INT(2) },
    { MP_ROM_QSTR(MP_QSTR_BOTON_CENTRAL), MP_ROM_INT(4) },
    { MP_ROM_QSTR(MP_QSTR_MAYUSCULAS), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_CONTROL), MP_ROM_INT(2) },
    /* teclas especiales (los caracteres llegan como su código Latin-1:
     * Enter = 10, Retroceso = 8, Tab = 9) */
    { MP_ROM_QSTR(MP_QSTR_TECLA_ARRIBA), MP_ROM_INT(0x100) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_ABAJO), MP_ROM_INT(0x101) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_IZQUIERDA), MP_ROM_INT(0x102) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_DERECHA), MP_ROM_INT(0x103) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_INICIO), MP_ROM_INT(0x104) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_FIN), MP_ROM_INT(0x105) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_REPAG), MP_ROM_INT(0x106) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_AVPAG), MP_ROM_INT(0x107) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_INSERTAR), MP_ROM_INT(0x108) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_SUPRIMIR), MP_ROM_INT(0x109) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F1), MP_ROM_INT(0x10A) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F2), MP_ROM_INT(0x10B) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F3), MP_ROM_INT(0x10C) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F4), MP_ROM_INT(0x10D) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F5), MP_ROM_INT(0x10E) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F6), MP_ROM_INT(0x10F) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F7), MP_ROM_INT(0x110) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F8), MP_ROM_INT(0x111) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F9), MP_ROM_INT(0x112) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F10), MP_ROM_INT(0x113) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F11), MP_ROM_INT(0x114) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_F12), MP_ROM_INT(0x115) },
    { MP_ROM_QSTR(MP_QSTR_TECLA_ESC), MP_ROM_INT(0x116) },
    { MP_ROM_QSTR(MP_QSTR_COLOR_FONDO), MP_ROM_INT(COR_FUNDO) },
};
static MP_DEFINE_CONST_DICT(sagui_globais, sagui_globais_tab);

const mp_obj_module_t modulo_sagui = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&sagui_globais,
};
MP_REGISTER_MODULE(MP_QSTR_sagui, modulo_sagui);
