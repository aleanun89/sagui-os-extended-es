/* grafico_sagui.c - el módulo sagui_grafico de MicroPython: el componente de gráficos de la libsagui
 * (userland/libsagui/gui_grafico.c, D469) visto desde Python.
 *
 *   import sagui, sagui_grafico
 *   g = sagui_grafico.Grafico()
 *   g.tipo("barras"); g.titulo("Ventas"); g.categorias(["Ene", "Feb", "Mar"])
 *   g.agregar_serie("2026", [3, 5, 2])
 *   img = sagui.Imagen.nueva(400, 240, 0xFFFFFF)
 *   g.avanzar(sagui.ticks_ms())          # la animación va por tiempo: se llama en cada tic
 *   g.dibujar(img, 400, 240)             # el gráfico se pinta en los píxeles de la Imagen
 *   ventana.imagen(0, 0, img)            # y la imagen se enseña donde se quiera (tkinter, VisualPythonS)
 *
 * El gráfico dibuja en un búfer de píxeles; MicroPython solo lo pasa de un lado a otro. El ratón entra por
 * mover/salir/pulsar con coordenadas relativas al gráfico. Los textos son Latin-1 en C y str en Python.
 * La cabecera grafico.h solo usa tipos básicos de C (no puede incluirse sagui.h junto con los de MicroPython,
 * ver sagui_mp.h). */
#include <string.h>
#include "py/runtime.h"
#include "py/objstr.h"
#include "py/objtuple.h"
#include "sagui_mp.h"
#include "../inc/grafico.h"

typedef struct {
    mp_obj_base_t base;
    grafico_t *g;
} graf_obj_t;

static const mp_obj_type_t tipo_grafico;

static graf_obj_t *graf(mp_obj_t o)
{
    graf_obj_t *s = MP_OBJ_TO_PTR(o);
    if (!s->g) mp_raise_ValueError(MP_ERROR_TEXT("gráfico cerrado"));
    return s;
}

static void texto_de(mp_obj_t o, char *buf, size_t max)
{
    if (o == mp_const_none) { buf[0] = 0; return; }
    sagui_str_para_latin1(o, buf, max);
}

/* Lista, tupla, array o rango de números -> double[]; devuelve cuántos (hasta max). */
static int numeros_de(mp_obj_t o, double *v, int max)
{
    int n = 0;
    mp_obj_t it = mp_getiter(o, NULL), x;
    while ((x = mp_iternext(it)) != MP_OBJ_STOP_ITERATION) {
        if (n < max) v[n] = (double)mp_obj_get_float(x);
        n++;
    }
    return n < max ? n : max;
}

/* ------------------------------------------------------------ nombres de tipos y opciones */

static const char *const nombres_tipo[GR_NTIPOS] = {
    "lineas", "area", "barras", "barras_h", "tarta", "anillo", "dispersion", "histograma", "velas"
};
static const char *const nombres_ley[] = { "ninguna", "abajo", "arriba", "derecha", "izquierda" };
static const char *const nombres_ease[] = { "lineal", "salida", "entrada", "ambas" };
static const struct { const char *n; int id; } opciones[] = {
    { "leyenda", GR_O_LEYENDA }, { "rejilla", GR_O_REJILLA }, { "animado", GR_O_ANIMADO }, { "apilado", GR_O_APILADO },
    { "suave", GR_O_SUAVE }, { "puntos", GR_O_PUNTOS }, { "duracion", GR_O_DURACION }, { "valores", GR_O_VALORES },
    { "hueco", GR_O_HUECO }, { "bins", GR_O_BINS }, { "ventana", GR_O_VENTANA }, { "cero", GR_O_CERO },
    { "ease", GR_O_EASE }, { "fondo", GR_O_FONDO },
};

static int indice_de_nombre(mp_obj_t o, const char *const *tabla, int n, const char *que)
{
    if (mp_obj_is_int(o)) return mp_obj_get_int(o);
    char b[32];
    sagui_str_para_latin1(o, b, sizeof b);
    for (int i = 0; i < n; i++) if (!strcmp(b, tabla[i])) return i;
    mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("%s desconocido"), que);
}

/* ------------------------------------------------------------ objeto */

static mp_obj_t graf_nuevo(const mp_obj_type_t *tipo, size_t n, size_t nkw, const mp_obj_t *args)
{
    (void)n; (void)nkw; (void)args;
    graf_obj_t *s = mp_obj_malloc_with_finaliser(graf_obj_t, tipo);
    s->g = grafico_nuevo();
    if (!s->g) mp_raise_msg(&mp_type_MemoryError, MP_ERROR_TEXT("sin memoria para el gráfico"));
    return MP_OBJ_FROM_PTR(s);
}

static mp_obj_t graf_cerrar(mp_obj_t self)
{
    graf_obj_t *s = MP_OBJ_TO_PTR(self);
    if (s->g) { grafico_liberar(s->g); s->g = NULL; }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(graf_cerrar_obj, graf_cerrar);

static mp_obj_t graf_tipo(size_t n, const mp_obj_t *a)
{
    graf_obj_t *s = graf(a[0]);
    if (n > 1) { grafico_tipo(s->g, indice_de_nombre(a[1], nombres_tipo, GR_NTIPOS, "tipo")); return mp_const_none; }
    return mp_obj_new_str(nombres_tipo[grafico_tipo_actual(s->g)], strlen(nombres_tipo[grafico_tipo_actual(s->g)]));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(graf_tipo_obj, 1, 2, graf_tipo);

static mp_obj_t graf_titulo(mp_obj_t self, mp_obj_t t)
{
    char b[96];
    texto_de(t, b, sizeof b);
    grafico_titulo(graf(self)->g, b);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(graf_titulo_obj, graf_titulo);

static mp_obj_t graf_ejes(mp_obj_t self, mp_obj_t x, mp_obj_t y)
{
    char bx[48], by[48];
    texto_de(x, bx, sizeof bx);
    texto_de(y, by, sizeof by);
    grafico_etiquetas_ejes(graf(self)->g, bx, by);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(graf_ejes_obj, graf_ejes);

static mp_obj_t graf_formato(mp_obj_t self, mp_obj_t f)
{
    char b[48];
    texto_de(f, b, sizeof b);
    grafico_formato(graf(self)->g, b);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(graf_formato_obj, graf_formato);

static mp_obj_t graf_separadores(mp_obj_t self, mp_obj_t d, mp_obj_t m)
{
    char bd[4], bm[4];
    texto_de(d, bd, sizeof bd);
    texto_de(m, bm, sizeof bm);
    grafico_separadores(graf(self)->g, bd[0] ? bd[0] : ',', bm[0] ? bm[0] : '.');
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(graf_separadores_obj, graf_separadores);

/* rango(min, max): None en cualquiera = automático */
static mp_obj_t graf_rango(mp_obj_t self, mp_obj_t mi, mp_obj_t ma)
{
    graf_obj_t *s = graf(self);
    if (mi == mp_const_none || ma == mp_const_none) grafico_rango(s->g, 1, 0, 0);
    else grafico_rango(s->g, 0, (double)mp_obj_get_float(mi), (double)mp_obj_get_float(ma));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(graf_rango_obj, graf_rango);

static mp_obj_t graf_opcion(size_t n, const mp_obj_t *a)
{
    graf_obj_t *s = graf(a[0]);
    char b[24];
    sagui_str_para_latin1(a[1], b, sizeof b);
    int id = -1;
    for (unsigned i = 0; i < sizeof opciones / sizeof opciones[0]; i++) if (!strcmp(b, opciones[i].n)) id = opciones[i].id;
    if (id < 0) mp_raise_msg_varg(&mp_type_ValueError, MP_ERROR_TEXT("opción desconocida: %s"), b);
    if (n < 3) {
        double v = grafico_opcion_leer(s->g, id);
        if (id == GR_O_LEYENDA) return mp_obj_new_str(nombres_ley[(int)v], strlen(nombres_ley[(int)v]));
        if (id == GR_O_EASE) return mp_obj_new_str(nombres_ease[(int)v], strlen(nombres_ease[(int)v]));
        return mp_obj_new_int((mp_int_t)v);
    }
    double v;
    if (id == GR_O_LEYENDA && mp_obj_is_exact_type(a[2], &mp_type_bool)) v = mp_obj_is_true(a[2]) ? GR_LEY_ABAJO : GR_LEY_NINGUNA;
    else if (id == GR_O_LEYENDA && !mp_obj_is_int(a[2])) v = indice_de_nombre(a[2], nombres_ley, 5, "posición de la leyenda");
    else if (id == GR_O_EASE && !mp_obj_is_int(a[2])) v = indice_de_nombre(a[2], nombres_ease, 4, "curva");
    else v = (double)mp_obj_get_float(a[2]);
    grafico_opcion(s->g, id, v);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(graf_opcion_obj, 2, 3, graf_opcion);

static mp_obj_t graf_categorias(mp_obj_t self, mp_obj_t lista)
{
    graf_obj_t *s = graf(self);
    grafico_categorias_limpiar(s->g);
    if (lista == mp_const_none) return mp_const_none;
    mp_obj_t it = mp_getiter(lista, NULL), x;
    while ((x = mp_iternext(it)) != MP_OBJ_STOP_ITERATION) {
        char b[48];
        if (mp_obj_is_str(x)) sagui_str_para_latin1(x, b, sizeof b);
        else {
            mp_obj_t t = mp_obj_str_make_new(&mp_type_str, 1, 0, &x);
            sagui_str_para_latin1(t, b, sizeof b);
        }
        if (grafico_categoria_agregar(s->g, b) < 0) break;
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(graf_categorias_obj, graf_categorias);

/* agregar_serie(nombre, datos=None, color=0, x=None, tam=None) -> índice */
static mp_obj_t graf_agregar_serie(size_t n, const mp_obj_t *args, mp_map_t *kw)
{
    enum { A_NOMBRE, A_DATOS, A_COLOR, A_X, A_TAM };
    static const mp_arg_t permitidos[] = {
        { MP_QSTR_nombre, MP_ARG_OBJ, { .u_rom_obj = MP_ROM_QSTR(MP_QSTR_) } },
        { MP_QSTR_datos, MP_ARG_OBJ, { .u_rom_obj = MP_ROM_NONE } },
        { MP_QSTR_color, MP_ARG_INT, { .u_int = 0 } },
        { MP_QSTR_x, MP_ARG_OBJ, { .u_rom_obj = MP_ROM_NONE } },
        { MP_QSTR_tam, MP_ARG_OBJ, { .u_rom_obj = MP_ROM_NONE } },
    };
    mp_arg_val_t v[5];
    mp_arg_parse_all(n - 1, args + 1, kw, 5, permitidos, v);
    graf_obj_t *s = graf(args[0]);
    char nom[48];
    texto_de(v[A_NOMBRE].u_obj, nom, sizeof nom);
    int i = grafico_agregar_serie(s->g, nom, (unsigned)v[A_COLOR].u_int);
    if (i < 0) mp_raise_msg(&mp_type_ValueError, MP_ERROR_TEXT("demasiadas series"));
    if (v[A_DATOS].u_obj != mp_const_none) {
        double y[GR_MAX_PUNTOS], x[GR_MAX_PUNTOS], t[GR_MAX_PUNTOS];
        int m = numeros_de(v[A_DATOS].u_obj, y, GR_MAX_PUNTOS);
        const double *px = NULL, *pt = NULL;
        if (v[A_X].u_obj != mp_const_none) { numeros_de(v[A_X].u_obj, x, GR_MAX_PUNTOS); px = x; }
        if (v[A_TAM].u_obj != mp_const_none) { numeros_de(v[A_TAM].u_obj, t, GR_MAX_PUNTOS); pt = t; }
        grafico_datos_xy(s->g, i, px, y, pt, m);
    }
    return mp_obj_new_int(i);
}
static MP_DEFINE_CONST_FUN_OBJ_KW(graf_agregar_serie_obj, 2, graf_agregar_serie);

/* datos(serie, y, x=None, tam=None) */
static mp_obj_t graf_datos(size_t n, const mp_obj_t *a)
{
    graf_obj_t *s = graf(a[0]);
    double y[GR_MAX_PUNTOS], x[GR_MAX_PUNTOS], t[GR_MAX_PUNTOS];
    int m = numeros_de(a[2], y, GR_MAX_PUNTOS);
    const double *px = NULL, *pt = NULL;
    if (n > 3 && a[3] != mp_const_none) { numeros_de(a[3], x, GR_MAX_PUNTOS); px = x; }
    if (n > 4 && a[4] != mp_const_none) { numeros_de(a[4], t, GR_MAX_PUNTOS); pt = t; }
    if (grafico_datos_xy(s->g, mp_obj_get_int(a[1]), px, y, pt, m) < 0) mp_raise_msg(&mp_type_IndexError, MP_ERROR_TEXT("serie fuera de rango"));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(graf_datos_obj, 3, 5, graf_datos);

/* agregar_punto(serie, y, x=None, tam=0) */
static mp_obj_t graf_agregar_punto(size_t n, const mp_obj_t *a)
{
    graf_obj_t *s = graf(a[0]);
    int k = mp_obj_get_int(a[1]);
    double y = (double)mp_obj_get_float(a[2]);
    int r;
    if (n > 3 && a[3] != mp_const_none) r = grafico_agregar_punto_xy(s->g, k, (double)mp_obj_get_float(a[3]), y, n > 4 ? (double)mp_obj_get_float(a[4]) : 0);
    else r = grafico_agregar_punto(s->g, k, y);
    if (r < 0) mp_raise_msg(&mp_type_IndexError, MP_ERROR_TEXT("serie fuera de rango"));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(graf_agregar_punto_obj, 3, 5, graf_agregar_punto);

static mp_obj_t graf_limpiar(mp_obj_t self) { grafico_limpiar(graf(self)->g); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_1(graf_limpiar_obj, graf_limpiar);

static mp_obj_t graf_quitar_series(mp_obj_t self) { grafico_quitar_series(graf(self)->g); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_1(graf_quitar_series_obj, graf_quitar_series);

static mp_obj_t graf_color(mp_obj_t self, mp_obj_t i, mp_obj_t c)
{
    grafico_serie_color(graf(self)->g, mp_obj_get_int(i), (unsigned)mp_obj_get_int(c));
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(graf_color_obj, graf_color);

static mp_obj_t graf_nombre(mp_obj_t self, mp_obj_t i, mp_obj_t t)
{
    char b[48];
    texto_de(t, b, sizeof b);
    grafico_serie_nombre(graf(self)->g, mp_obj_get_int(i), b);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_3(graf_nombre_obj, graf_nombre);

static mp_obj_t graf_ocultar(size_t n, const mp_obj_t *a)
{
    graf_obj_t *s = graf(a[0]);
    grafico_ocultar(s->g, mp_obj_get_int(a[1]), n > 2 ? mp_obj_is_true(a[2]) : 1);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(graf_ocultar_obj, 2, 3, graf_ocultar);

static mp_obj_t graf_oculto(mp_obj_t self, mp_obj_t i) { return mp_obj_new_bool(grafico_oculto(graf(self)->g, mp_obj_get_int(i))); }
static MP_DEFINE_CONST_FUN_OBJ_2(graf_oculto_obj, graf_oculto);

static mp_obj_t graf_n_series(mp_obj_t self) { return mp_obj_new_int(grafico_n_series(graf(self)->g)); }
static MP_DEFINE_CONST_FUN_OBJ_1(graf_n_series_obj, graf_n_series);

static mp_obj_t graf_n_puntos(mp_obj_t self, mp_obj_t i) { return mp_obj_new_int(grafico_n_puntos(graf(self)->g, mp_obj_get_int(i))); }
static MP_DEFINE_CONST_FUN_OBJ_2(graf_n_puntos_obj, graf_n_puntos);

static mp_obj_t graf_valor(mp_obj_t self, mp_obj_t i, mp_obj_t j)
{
    return mp_obj_new_float(grafico_valor(graf(self)->g, mp_obj_get_int(i), mp_obj_get_int(j)));
}
static MP_DEFINE_CONST_FUN_OBJ_3(graf_valor_obj, graf_valor);

static mp_obj_t graf_avanzar(mp_obj_t self, mp_obj_t ms)
{
    return mp_obj_new_bool(grafico_avanzar(graf(self)->g, (unsigned)mp_obj_get_int_truncated(ms)));
}
static MP_DEFINE_CONST_FUN_OBJ_2(graf_avanzar_obj, graf_avanzar);

static mp_obj_t graf_sucio(mp_obj_t self) { return mp_obj_new_bool(grafico_sucio(graf(self)->g)); }
static MP_DEFINE_CONST_FUN_OBJ_1(graf_sucio_obj, graf_sucio);

static mp_obj_t graf_reiniciar(mp_obj_t self) { grafico_reiniciar(graf(self)->g); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_1(graf_reiniciar_obj, graf_reiniciar);

/* dibujar(imagen, ancho, alto): pinta en los píxeles de una sagui.Imagen (o cualquier búfer 'I' de ancho*alto) */
static mp_obj_t graf_dibujar(size_t n, const mp_obj_t *a)
{
    (void)n;
    graf_obj_t *s = graf(a[0]);
    int l = mp_obj_get_int(a[2]), h = mp_obj_get_int(a[3]);
    mp_buffer_info_t b;
    mp_get_buffer_raise(a[1], &b, MP_BUFFER_RW);
    if (l < 8 || h < 8 || (size_t)l * (size_t)h * 4 > b.len) mp_raise_ValueError(MP_ERROR_TEXT("tamaño del gráfico"));
    grafico_dibujar(s->g, (unsigned *)b.buf, l, h);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(graf_dibujar_obj, 4, 4, graf_dibujar);

static mp_obj_t graf_mover(mp_obj_t self, mp_obj_t x, mp_obj_t y)
{
    return mp_obj_new_bool(grafico_mover(graf(self)->g, mp_obj_get_int(x), mp_obj_get_int(y)));
}
static MP_DEFINE_CONST_FUN_OBJ_3(graf_mover_obj, graf_mover);

static mp_obj_t graf_salir(mp_obj_t self) { grafico_salir(graf(self)->g); return mp_const_none; }
static MP_DEFINE_CONST_FUN_OBJ_1(graf_salir_obj, graf_salir);

/* pulsar(x, y) -> None o (serie, indice, valor, leyenda) */
static mp_obj_t graf_pulsar(mp_obj_t self, mp_obj_t x, mp_obj_t y)
{
    grafico_ev_t ev;
    int r = grafico_pulsar(graf(self)->g, mp_obj_get_int(x), mp_obj_get_int(y), &ev);
    if (!r) return mp_const_none;
    mp_obj_t t[4] = { mp_obj_new_int(ev.serie), mp_obj_new_int(ev.indice), mp_obj_new_float(ev.valor), mp_obj_new_bool(ev.leyenda) };
    return mp_obj_new_tuple(4, t);
}
static MP_DEFINE_CONST_FUN_OBJ_3(graf_pulsar_obj, graf_pulsar);

static mp_obj_t graf_resaltado(mp_obj_t self)
{
    int s, i;
    if (!grafico_resaltado(graf(self)->g, &s, &i)) return mp_const_none;
    mp_obj_t t[2] = { mp_obj_new_int(s), mp_obj_new_int(i) };
    return mp_obj_new_tuple(2, t);
}
static MP_DEFINE_CONST_FUN_OBJ_1(graf_resaltado_obj, graf_resaltado);

static mp_obj_t graf_tooltip(mp_obj_t self)
{
    const char *t = grafico_tooltip(graf(self)->g);
    vstr_t v;
    vstr_init(&v, 48);
    sagui_latin1_para_vstr(&v, t, strlen(t));
    return mp_obj_new_str_from_vstr(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_1(graf_tooltip_obj, graf_tooltip);

/* exportar(ruta, ancho=640, alto=400): .png o .bmp */
static mp_obj_t graf_exportar(size_t n, const mp_obj_t *a)
{
    graf_obj_t *s = graf(a[0]);
    char ruta[256];
    sagui_str_para_latin1(a[1], ruta, sizeof ruta);
    int r = grafico_exportar(s->g, ruta, n > 2 ? mp_obj_get_int(a[2]) : 640, n > 3 ? mp_obj_get_int(a[3]) : 400);
    if (r < 0) mp_raise_msg_varg(&mp_type_OSError, MP_ERROR_TEXT("no se pudo exportar el gráfico (%d)"), r);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(graf_exportar_obj, 2, 4, graf_exportar);

static const mp_rom_map_elem_t graf_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&graf_cerrar_obj) },
    { MP_ROM_QSTR(MP_QSTR_cerrar), MP_ROM_PTR(&graf_cerrar_obj) },
    { MP_ROM_QSTR(MP_QSTR_tipo), MP_ROM_PTR(&graf_tipo_obj) },
    { MP_ROM_QSTR(MP_QSTR_titulo), MP_ROM_PTR(&graf_titulo_obj) },
    { MP_ROM_QSTR(MP_QSTR_ejes), MP_ROM_PTR(&graf_ejes_obj) },
    { MP_ROM_QSTR(MP_QSTR_formato), MP_ROM_PTR(&graf_formato_obj) },
    { MP_ROM_QSTR(MP_QSTR_separadores), MP_ROM_PTR(&graf_separadores_obj) },
    { MP_ROM_QSTR(MP_QSTR_rango), MP_ROM_PTR(&graf_rango_obj) },
    { MP_ROM_QSTR(MP_QSTR_opcion), MP_ROM_PTR(&graf_opcion_obj) },
    { MP_ROM_QSTR(MP_QSTR_categorias), MP_ROM_PTR(&graf_categorias_obj) },
    { MP_ROM_QSTR(MP_QSTR_agregar_serie), MP_ROM_PTR(&graf_agregar_serie_obj) },
    { MP_ROM_QSTR(MP_QSTR_datos), MP_ROM_PTR(&graf_datos_obj) },
    { MP_ROM_QSTR(MP_QSTR_agregar_punto), MP_ROM_PTR(&graf_agregar_punto_obj) },
    { MP_ROM_QSTR(MP_QSTR_limpiar), MP_ROM_PTR(&graf_limpiar_obj) },
    { MP_ROM_QSTR(MP_QSTR_quitar_series), MP_ROM_PTR(&graf_quitar_series_obj) },
    { MP_ROM_QSTR(MP_QSTR_color), MP_ROM_PTR(&graf_color_obj) },
    { MP_ROM_QSTR(MP_QSTR_nombre), MP_ROM_PTR(&graf_nombre_obj) },
    { MP_ROM_QSTR(MP_QSTR_ocultar), MP_ROM_PTR(&graf_ocultar_obj) },
    { MP_ROM_QSTR(MP_QSTR_oculto), MP_ROM_PTR(&graf_oculto_obj) },
    { MP_ROM_QSTR(MP_QSTR_n_series), MP_ROM_PTR(&graf_n_series_obj) },
    { MP_ROM_QSTR(MP_QSTR_n_puntos), MP_ROM_PTR(&graf_n_puntos_obj) },
    { MP_ROM_QSTR(MP_QSTR_valor), MP_ROM_PTR(&graf_valor_obj) },
    { MP_ROM_QSTR(MP_QSTR_avanzar), MP_ROM_PTR(&graf_avanzar_obj) },
    { MP_ROM_QSTR(MP_QSTR_sucio), MP_ROM_PTR(&graf_sucio_obj) },
    { MP_ROM_QSTR(MP_QSTR_reiniciar), MP_ROM_PTR(&graf_reiniciar_obj) },
    { MP_ROM_QSTR(MP_QSTR_dibujar), MP_ROM_PTR(&graf_dibujar_obj) },
    { MP_ROM_QSTR(MP_QSTR_mover), MP_ROM_PTR(&graf_mover_obj) },
    { MP_ROM_QSTR(MP_QSTR_salir), MP_ROM_PTR(&graf_salir_obj) },
    { MP_ROM_QSTR(MP_QSTR_pulsar), MP_ROM_PTR(&graf_pulsar_obj) },
    { MP_ROM_QSTR(MP_QSTR_resaltado), MP_ROM_PTR(&graf_resaltado_obj) },
    { MP_ROM_QSTR(MP_QSTR_tooltip), MP_ROM_PTR(&graf_tooltip_obj) },
    { MP_ROM_QSTR(MP_QSTR_exportar), MP_ROM_PTR(&graf_exportar_obj) },
};
static MP_DEFINE_CONST_DICT(graf_metodos, graf_metodos_tab);

static MP_DEFINE_CONST_OBJ_TYPE(tipo_grafico, MP_QSTR_Grafico, MP_TYPE_FLAG_NONE,
    make_new, graf_nuevo, locals_dict, &graf_metodos);

/* ------------------------------------------------------------ módulo */

/* escala(minimo, maximo, marcas=6, cero=True) -> (min, max, paso, n): las marcas 1-2-5 que usa el gráfico */
static mp_obj_t mod_escala(size_t n, const mp_obj_t *a)
{
    gr_escala_t e;
    gr_escala((double)mp_obj_get_float(a[0]), (double)mp_obj_get_float(a[1]), n > 2 ? mp_obj_get_int(a[2]) : 6, n > 3 ? mp_obj_is_true(a[3]) : 1, &e);
    mp_obj_t t[4] = { mp_obj_new_float(e.min), mp_obj_new_float(e.max), mp_obj_new_float(e.paso), mp_obj_new_int(e.n) };
    return mp_obj_new_tuple(4, t);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_escala_obj, 2, 4, mod_escala);

/* formatear(valor, formato="auto") -> str: el mismo formato de números que los ejes y el tooltip */
static mp_obj_t mod_formatear(size_t n, const mp_obj_t *a)
{
    gr_fmt_t f;
    char spec[48], b[64];
    if (n > 1) sagui_str_para_latin1(a[1], spec, sizeof spec); else strcpy(spec, "auto");
    gr_fmt_parse(spec, &f);
    int k = gr_fmt_num(&f, (double)mp_obj_get_float(a[0]), b, sizeof b);
    vstr_t v;
    vstr_init(&v, 32);
    sagui_latin1_para_vstr(&v, b, (size_t)k);
    return mp_obj_new_str_from_vstr(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_formatear_obj, 1, 2, mod_formatear);

/* color_serie(i, oscuro=False) -> color de la paleta */
static mp_obj_t mod_color_serie(size_t n, const mp_obj_t *a)
{
    return mp_obj_new_int(gr_color_serie(mp_obj_get_int(a[0]), n > 1 ? mp_obj_is_true(a[1]) : 0));
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_color_serie_obj, 1, 2, mod_color_serie);

static const mp_rom_map_elem_t grafico_globais_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_sagui_grafico) },
    { MP_ROM_QSTR(MP_QSTR_Grafico), MP_ROM_PTR(&tipo_grafico) },
    { MP_ROM_QSTR(MP_QSTR_escala), MP_ROM_PTR(&mod_escala_obj) },
    { MP_ROM_QSTR(MP_QSTR_formatear), MP_ROM_PTR(&mod_formatear_obj) },
    { MP_ROM_QSTR(MP_QSTR_color_serie), MP_ROM_PTR(&mod_color_serie_obj) },
};
static MP_DEFINE_CONST_DICT(grafico_globais, grafico_globais_tab);

const mp_obj_module_t modulo_sagui_grafico = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&grafico_globais,
};
MP_REGISTER_MODULE(MP_QSTR_sagui_grafico, modulo_sagui_grafico);
