// SPDX-License-Identifier: GPL-3.0-only
/* gui_nativos.c - métodos nativos de la biblioteca gráfica de Java sagui.gui (D376): la clase
 * sagui.gui.Nativo llama aquí y esto llama a la libsagui (ventanas, dibujo, imágenes, diálogos,
 * fuentes vectoriales). Los componentes (Boton, Lista...) están escritos en Java encima de estas
 * primitivas; ver userland/java/sagui/.
 *
 * Los textos de Java llegan en UTF-8. La fuente de mapa de bits de la libsagui (g_texto) y los
 * diálogos usan Latin-1: se convierte aquí. Las fuentes vectoriales (libfontes) leen UTF-8. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <dirent.h>

#include "sagui.h"
#include "gui.h"
#include "imagem.h"
#include "fontes.h"
#include "utils/d_type.h"
#include "jvm/jvm_util.h"
#include "jvm/jvm.h"

#define MAX_JAN 8
#define MAX_IMG 64
#define MAX_FONTES 16

static jan_t *g_jan[MAX_JAN + 1];
static imagem_t *g_img[MAX_IMG + 1];
static fonte_t *g_fonte[MAX_FONTES + 1];
/* Área de recorte por ventana (x0, y0, x1, y1), medio abierta. */
static int g_clip[MAX_JAN + 1][4];

/* ---------------------------------------------------------------- ayudas */

static void cadena(Runtime *r, int i, char *dst, int max)
{
    Instance *js = localvar_getRefer(r->localvar, i);
    dst[0] = 0;
    if (!js) return;
    Utf8String *u = utf8_create();
    jstring_2_utf8(js, u, r);
    snprintf(dst, max, "%s", utf8_cstr(u));
    utf8_destroy(u);
}

/* UTF-8 -> Latin-1 en el sitio (lo que no cabe, '?'). */
static void a_latin1(char *s)
{
    unsigned char *o = (unsigned char *)s, *d = o;
    while (*o) {
        unsigned c = *o;
        if (c < 0x80) { *d++ = (unsigned char)c; o++; }
        else if ((c & 0xE0) == 0xC0 && (o[1] & 0xC0) == 0x80) {
            unsigned cp = ((c & 0x1F) << 6) | (o[1] & 0x3F);
            *d++ = cp < 256 ? (unsigned char)cp : '?';
            o += 2;
        } else {
            *d++ = '?';
            o++;
            while ((*o & 0xC0) == 0x80) o++;
        }
    }
    *d = 0;
}

/* Latin-1 -> UTF-8 (para lo que sale de los diálogos hacia Java). */
static void a_utf8(const char *s, char *dst, int max)
{
    int k = 0;
    for (; *s && k < max - 3; s++) {
        unsigned char c = (unsigned char)*s;
        if (c < 0x80) dst[k++] = (char)c;
        else { dst[k++] = (char)(0xC0 | (c >> 6)); dst[k++] = (char)(0x80 | (c & 0x3F)); }
    }
    dst[k] = 0;
}

static jan_t *ventana(Runtime *r, int i, int *id)
{
    int v = localvar_getInt(r->localvar, i);
    if (id) *id = v;
    return (v >= 1 && v <= MAX_JAN) ? g_jan[v] : NULL;
}

static int cor_(Runtime *r, int i) { return localvar_getInt(r->localvar, i) & 0xFFFFFF; }

static inline void punto(jan_t *j, const int *c, int x, int y, uint32_t cor)
{
    if (x >= c[0] && x < c[2] && y >= c[1] && y < c[3]) j->px[y * j->larg + x] = cor;
}

static void rectangulo_clip(jan_t *j, const int *c, int x, int y, int w, int h, uint32_t cor)
{
    int x1 = x + w, y1 = y + h;
    if (x < c[0]) x = c[0];
    if (y < c[1]) y = c[1];
    if (x1 > c[2]) x1 = c[2];
    if (y1 > c[3]) y1 = c[3];
    for (int yy = y; yy < y1; yy++)
        for (int xx = x; xx < x1; xx++) j->px[yy * j->larg + xx] = cor;
}

/* ---------------------------------------------------------------- ventanas */

/* static int abrir(String titulo, int w, int h) -> id (>0) o 0 */
static s32 n_abrir(Runtime *r, JClass *c)
{
    char t[256];
    cadena(r, 0, t, sizeof t);
    a_latin1(t);
    int w = localvar_getInt(r->localvar, 1), h = localvar_getInt(r->localvar, 2), id = 0;
    for (int i = 1; i <= MAX_JAN; i++)
        if (!g_jan[i]) { id = i; break; }
    if (id) {
        jan_t *j = jan_abrir(t, w, h);
        if (!j) id = 0;
        else {
            g_jan[id] = j;
            g_clip[id][0] = 0; g_clip[id][1] = 0; g_clip[id][2] = j->larg; g_clip[id][3] = j->alt;
            g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
        }
    }
    push_int(r->stack, id);
    return 0;
}

static s32 n_cerrar(Runtime *r, JClass *c)
{
    int id;
    jan_t *j = ventana(r, 0, &id);
    if (j) { jan_fechar(j); g_jan[id] = NULL; }
    return 0;
}

static s32 n_mostrar(Runtime *r, JClass *c)
{
    jan_t *j = ventana(r, 0, NULL);
    if (j) jan_mostrar(j);
    return 0;
}

static s32 n_titulo(Runtime *r, JClass *c)
{
    jan_t *j = ventana(r, 0, NULL);
    char t[256];
    cadena(r, 1, t, sizeof t);
    a_latin1(t);
    if (j) titulo_janela(j->id, t);
    return 0;
}

static s32 n_ancho(Runtime *r, JClass *c) { jan_t *j = ventana(r, 0, NULL); push_int(r->stack, j ? j->larg : 0); return 0; }
static s32 n_alto(Runtime *r, JClass *c) { jan_t *j = ventana(r, 0, NULL); push_int(r->stack, j ? j->alt : 0); return 0; }

/* static void recorte(int v, int x, int y, int w, int h): w <= 0 quita el recorte */
static s32 n_recorte(Runtime *r, JClass *c)
{
    int id;
    jan_t *j = ventana(r, 0, &id);
    if (!j) return 0;
    int x = localvar_getInt(r->localvar, 1), y = localvar_getInt(r->localvar, 2);
    int w = localvar_getInt(r->localvar, 3), h = localvar_getInt(r->localvar, 4);
    int *cl = g_clip[id];
    if (w <= 0) { cl[0] = 0; cl[1] = 0; cl[2] = j->larg; cl[3] = j->alt; return 0; }
    cl[0] = x < 0 ? 0 : x; cl[1] = y < 0 ? 0 : y;
    cl[2] = x + w > j->larg ? j->larg : x + w; cl[3] = y + h > j->alt ? j->alt : y + h;
    return 0;
}

/* ---------------------------------------------------------------- dibujo */

/* static void rect(int v, int x, int y, int w, int h, int color) */
static s32 n_rect(Runtime *r, JClass *c)
{
    int id;
    jan_t *j = ventana(r, 0, &id);
    if (j) rectangulo_clip(j, g_clip[id], localvar_getInt(r->localvar, 1), localvar_getInt(r->localvar, 2),
                           localvar_getInt(r->localvar, 3), localvar_getInt(r->localvar, 4), cor_(r, 5));
    return 0;
}

/* static void linea(int v, int x0, int y0, int x1, int y1, int color) (Bresenham con recorte por punto) */
static s32 n_linea(Runtime *r, JClass *c)
{
    int id;
    jan_t *j = ventana(r, 0, &id);
    if (!j) return 0;
    int x0 = localvar_getInt(r->localvar, 1), y0 = localvar_getInt(r->localvar, 2);
    int x1 = localvar_getInt(r->localvar, 3), y1 = localvar_getInt(r->localvar, 4);
    uint32_t cor = (uint32_t)cor_(r, 5);
    int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
    int dy = -(y1 > y0 ? y1 - y0 : y0 - y1), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    for (int n = 0; n < 20000; n++) {
        punto(j, g_clip[id], x0, y0, cor);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
    return 0;
}

/* static void elipse(int v, int x, int y, int w, int h, int color, boolean relleno) */
static s32 n_elipse(Runtime *r, JClass *c)
{
    int id;
    jan_t *j = ventana(r, 0, &id);
    if (!j) return 0;
    int x = localvar_getInt(r->localvar, 1), y = localvar_getInt(r->localvar, 2);
    int w = localvar_getInt(r->localvar, 3), h = localvar_getInt(r->localvar, 4);
    uint32_t cor = (uint32_t)cor_(r, 5);
    int relleno = localvar_getInt(r->localvar, 6);
    if (w <= 0 || h <= 0) return 0;
    /* por cada fila, el semiancho de la elipse (entero, con raíz por aproximación) */
    long a = w, b = h;
    for (int yy = 0; yy < h; yy++) {
        long dy2 = 2 * yy + 1 - b;                       /* -b..b */
        long num = a * a * (b * b - dy2 * dy2);          /* (a^2 b^2 - a^2 dy2^2) */
        if (num < 0) continue;
        long s = 0, lo = 0, hi = a;                      /* s = floor(sqrt(num)/b) con sqrt entera */
        while (lo < hi) { long m = (lo + hi + 1) / 2; if (m * m * b * b <= num) lo = m; else hi = m - 1; }
        s = lo;                                          /* semiancho * 2 */
        int xa = x + (int)((a - s) / 2), xb = x + (int)((a + s) / 2);
        if (relleno) rectangulo_clip(j, g_clip[id], xa, y + yy, xb - xa, 1, cor);
        else {
            punto(j, g_clip[id], xa, y + yy, cor);
            punto(j, g_clip[id], xb - 1, y + yy, cor);
            if (yy == 0 || yy == h - 1) rectangulo_clip(j, g_clip[id], xa, y + yy, xb - xa, 1, cor);
        }
    }
    return 0;
}

/* static void texto(int v, int x, int y, String t, int color, int fuente, int px) */
static s32 n_texto(Runtime *r, JClass *c)
{
    jan_t *j = ventana(r, 0, NULL);
    char t[1024];
    cadena(r, 3, t, sizeof t);
    int x = localvar_getInt(r->localvar, 1), y = localvar_getInt(r->localvar, 2);
    uint32_t cor = (uint32_t)cor_(r, 4);
    int f = localvar_getInt(r->localvar, 5), px = localvar_getInt(r->localvar, 6);
    if (!j) return 0;
    if (f >= 1 && f <= MAX_FONTES && g_fonte[f]) {
        fonte_texto(g_fonte[f], j, x, y, t, px, cor);
    } else {
        a_latin1(t);
        g_texto(j, x, y, t, cor);
    }
    return 0;
}

/* static int anchoTexto(String t, int fuente, int px) */
static s32 n_ancho_texto(Runtime *r, JClass *c)
{
    char t[1024];
    cadena(r, 0, t, sizeof t);
    int f = localvar_getInt(r->localvar, 1), px = localvar_getInt(r->localvar, 2);
    int w;
    if (f >= 1 && f <= MAX_FONTES && g_fonte[f]) w = fonte_medir(g_fonte[f], t, px);
    else { a_latin1(t); w = g_largura_texto(t); }
    push_int(r->stack, w);
    return 0;
}

/* ---------------------------------------------------------------- imágenes (PNG, JPEG, BMP, GIF) */

/* static int cargarImagen(String ruta) -> handle (>0) o el error (<0) */
static s32 n_cargar_imagen(Runtime *r, JClass *c)
{
    char ruta[512];
    cadena(r, 0, ruta, sizeof ruta);
    int h = 0;
    for (int i = 1; i <= MAX_IMG; i++)
        if (!g_img[i]) { h = i; break; }
    int res = -1;
    if (h) {
        imagem_t *im = calloc(1, sizeof *im);
        res = imagem_ler(ruta, im);
        if (res == 0) { g_img[h] = im; res = h; }
        else free(im);
    }
    push_int(r->stack, res);
    return 0;
}

static imagem_t *imagen(Runtime *r, int i)
{
    int h = localvar_getInt(r->localvar, i);
    return (h >= 1 && h <= MAX_IMG) ? g_img[h] : NULL;
}
static s32 n_imagen_ancho(Runtime *r, JClass *c) { imagem_t *m = imagen(r, 0); push_int(r->stack, m ? m->larg : 0); return 0; }
static s32 n_imagen_alto(Runtime *r, JClass *c) { imagem_t *m = imagen(r, 0); push_int(r->stack, m ? m->alt : 0); return 0; }
static s32 n_liberar_imagen(Runtime *r, JClass *c)
{
    imagem_t *m = imagen(r, 0);
    if (m) { imagem_liberar(m); free(m); g_img[localvar_getInt(r->localvar, 0)] = NULL; }
    return 0;
}

/* static void dibujarImagen(int v, int h, int x, int y, int w, int alto): escalada al vuelo (vecino más próximo) */
static s32 n_dibujar_imagen(Runtime *r, JClass *c)
{
    int id;
    jan_t *j = ventana(r, 0, &id);
    imagem_t *m = imagen(r, 1);
    if (!j || !m) return 0;
    int x = localvar_getInt(r->localvar, 2), y = localvar_getInt(r->localvar, 3);
    int w = localvar_getInt(r->localvar, 4), h = localvar_getInt(r->localvar, 5);
    if (w <= 0) w = m->larg;
    if (h <= 0) h = m->alt;
    const int *cl = g_clip[id];
    for (int yy = 0; yy < h; yy++) {
        int py = y + yy;
        if (py < cl[1] || py >= cl[3]) continue;
        const uint32_t *fila = m->px + (long)(yy * m->alt / h) * m->larg;
        for (int xx = 0; xx < w; xx++) {
            int pxx = x + xx;
            if (pxx >= cl[0] && pxx < cl[2]) j->px[py * j->larg + pxx] = fila[xx * m->larg / w] & 0xFFFFFF;
        }
    }
    return 0;
}

/* ---------------------------------------------------------------- fuentes del sistema */

/* static int abrirFuente(String ruta) -> handle (>0) o 0 */
static s32 n_abrir_fuente(Runtime *r, JClass *c)
{
    char ruta[512];
    cadena(r, 0, ruta, sizeof ruta);
    int h = 0;
    for (int i = 1; i <= MAX_FONTES; i++)
        if (!g_fonte[i]) { h = i; break; }
    if (h) {
        g_fonte[h] = fonte_abrir_archivo(ruta);
        if (!g_fonte[h]) h = 0;
    }
    push_int(r->stack, h);
    return 0;
}

static s32 n_cerrar_fuente(Runtime *r, JClass *c)
{
    int h = localvar_getInt(r->localvar, 0);
    if (h >= 1 && h <= MAX_FONTES && g_fonte[h]) { fonte_fechar(g_fonte[h]); g_fonte[h] = NULL; }
    return 0;
}

/* static String listarFuentes(String carpeta): los ficheros .ttf/.otf/.woff separados por '\n' */
static s32 n_listar_fuentes(Runtime *r, JClass *c)
{
    char dir[256], res[4096];
    cadena(r, 0, dir, sizeof dir);
    res[0] = 0;
    DIR *d = opendir(dir);
    if (d) {
        struct dirent *e;
        while ((e = readdir(d)) != NULL) {
            size_t n = strlen(e->d_name);
            if (n > 4 && (!strcmp(e->d_name + n - 4, ".ttf") || !strcmp(e->d_name + n - 4, ".otf"))) {
                if (strlen(res) + n + 2 < sizeof res) { strcat(res, e->d_name); strcat(res, "\n"); }
            }
        }
        closedir(d);
    }
    push_ref(r->stack, jstring_create_cstr(res, r));
    return 0;
}

/* ---------------------------------------------------------------- eventos y portapapeles */

/* static int evento(int v, int ms, int[] salida): 1 si llegó (salida = tipo, x, y, botones, clics, tecla, modificadores) */
static s32 n_evento(Runtime *r, JClass *c)
{
    jan_t *j = ventana(r, 0, NULL);
    int ms = localvar_getInt(r->localvar, 1);
    Instance *arr = localvar_getRefer(r->localvar, 2);
    evento_t e;
    int got = 0;
    if (j) {
        jthread_block_enter(r);
        got = esperar_evento(j->id, &e, ms);
        jthread_block_exit(r);
    }
    if (got > 0 && arr) {
        s32 *o = (s32 *)jarray_body(arr);
        if (e.tipo == EV_REDIMENSIONAR) jan_redimensionar(j, &e);
        o[0] = (s32)e.tipo; o[1] = e.x; o[2] = e.y; o[3] = (s32)e.botoes;
        o[4] = (s32)e.cliques; o[5] = (s32)e.tecla; o[6] = (s32)e.modificadores;
    }
    push_int(r->stack, got > 0 ? 1 : 0);
    return 0;
}

/* ---------------------------------------------------------------- diálogos y portapapeles */

static void devolver(Runtime *r, const char *latin1)
{
    char u[1100];
    a_utf8(latin1, u, sizeof u);
    push_ref(r->stack, jstring_create_cstr(u, r));
}

static s32 n_mensaje(Runtime *r, JClass *c)
{
    char t[256], m[1024];
    cadena(r, 0, t, sizeof t); cadena(r, 1, m, sizeof m);
    a_latin1(t); a_latin1(m);
    jthread_block_enter(r);
    mensagem(t, m);
    jthread_block_exit(r);
    return 0;
}

static s32 n_confirmar(Runtime *r, JClass *c)
{
    char t[256], m[1024];
    cadena(r, 0, t, sizeof t); cadena(r, 1, m, sizeof m);
    a_latin1(t); a_latin1(m);
    jthread_block_enter(r);
    int ok = confirmar(t, m);
    jthread_block_exit(r);
    push_int(r->stack, ok ? 1 : 0);
    return 0;
}

/* static String pedirTexto(String titulo, String pregunta, String inicial): null si se cancela */
static s32 n_pedir_texto(Runtime *r, JClass *c)
{
    char t[256], m[1024], resp[512];
    cadena(r, 0, t, sizeof t); cadena(r, 1, m, sizeof m); cadena(r, 2, resp, sizeof resp);
    a_latin1(t); a_latin1(m); a_latin1(resp);
    jthread_block_enter(r);
    int ok = pedir_texto(t, m, resp, sizeof resp);
    jthread_block_exit(r);
    if (ok) devolver(r, resp); else push_ref(r->stack, NULL);
    return 0;
}

/* static String dialogoFichero(int guardar, String titulo, String filtros, String inicial): null si se cancela */
static s32 n_dialogo_fichero(Runtime *r, JClass *c)
{
    char t[256], f[256], ini[512], sal[512];
    int guardar = localvar_getInt(r->localvar, 0);
    cadena(r, 1, t, sizeof t); cadena(r, 2, f, sizeof f); cadena(r, 3, ini, sizeof ini);
    a_latin1(t); a_latin1(f); a_latin1(ini);
    sal[0] = 0;
    jthread_block_enter(r);
    int ok = guardar ? dialogo_guardar(t, f[0] ? f : NULL, ini, sal, sizeof sal)
                     : dialogo_abrir(t, f[0] ? f : NULL, ini, sal, sizeof sal);
    jthread_block_exit(r);
    if (ok) devolver(r, sal); else push_ref(r->stack, NULL);
    return 0;
}

static s32 n_copiar(Runtime *r, JClass *c)
{
    char t[2048];
    cadena(r, 0, t, sizeof t);
    a_latin1(t);
    copiar_texto(t, (unsigned)strlen(t));
    return 0;
}

static s32 n_pegar(Runtime *r, JClass *c)
{
    char t[2048];
    t[0] = 0;
    colar_texto(t, sizeof t);
    devolver(r, t);
    return 0;
}

static s32 n_milisegundos(Runtime *r, JClass *c)
{
    push_int(r->stack, (s32)(currentTimeMillis() & 0x7FFFFFFF));
    return 0;
}

static java_native_method TABLA_GUI[] = {
    {"sagui/gui/Nativo", "abrir", "(Ljava/lang/String;II)I", n_abrir},
    {"sagui/gui/Nativo", "cerrar", "(I)V", n_cerrar},
    {"sagui/gui/Nativo", "mostrar", "(I)V", n_mostrar},
    {"sagui/gui/Nativo", "titulo", "(ILjava/lang/String;)V", n_titulo},
    {"sagui/gui/Nativo", "ancho", "(I)I", n_ancho},
    {"sagui/gui/Nativo", "alto", "(I)I", n_alto},
    {"sagui/gui/Nativo", "recorte", "(IIIII)V", n_recorte},
    {"sagui/gui/Nativo", "rect", "(IIIIII)V", n_rect},
    {"sagui/gui/Nativo", "linea", "(IIIIII)V", n_linea},
    {"sagui/gui/Nativo", "elipse", "(IIIIIIZ)V", n_elipse},
    {"sagui/gui/Nativo", "texto", "(IIILjava/lang/String;III)V", n_texto},
    {"sagui/gui/Nativo", "anchoTexto", "(Ljava/lang/String;II)I", n_ancho_texto},
    {"sagui/gui/Nativo", "cargarImagen", "(Ljava/lang/String;)I", n_cargar_imagen},
    {"sagui/gui/Nativo", "imagenAncho", "(I)I", n_imagen_ancho},
    {"sagui/gui/Nativo", "imagenAlto", "(I)I", n_imagen_alto},
    {"sagui/gui/Nativo", "liberarImagen", "(I)V", n_liberar_imagen},
    {"sagui/gui/Nativo", "dibujarImagen", "(IIIIII)V", n_dibujar_imagen},
    {"sagui/gui/Nativo", "abrirFuente", "(Ljava/lang/String;)I", n_abrir_fuente},
    {"sagui/gui/Nativo", "cerrarFuente", "(I)V", n_cerrar_fuente},
    {"sagui/gui/Nativo", "listarFuentes", "(Ljava/lang/String;)Ljava/lang/String;", n_listar_fuentes},
    {"sagui/gui/Nativo", "evento", "(II[I)I", n_evento},
    {"sagui/gui/Nativo", "mensaje", "(Ljava/lang/String;Ljava/lang/String;)V", n_mensaje},
    {"sagui/gui/Nativo", "confirmar", "(Ljava/lang/String;Ljava/lang/String;)Z", n_confirmar},
    {"sagui/gui/Nativo", "pedirTexto", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", n_pedir_texto},
    {"sagui/gui/Nativo", "dialogoFichero", "(ZLjava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", n_dialogo_fichero},
    {"sagui/gui/Nativo", "copiar", "(Ljava/lang/String;)V", n_copiar},
    {"sagui/gui/Nativo", "pegar", "()Ljava/lang/String;", n_pegar},
    {"sagui/gui/Nativo", "milisegundos", "()I", n_milisegundos},
};

/* java_main.c lo llama después de jvm_init. */
void sagui_gui_registrar(MiniJVM *jvm)
{
    native_reg_lib(jvm, &TABLA_GUI[0], sizeof(TABLA_GUI) / sizeof(java_native_method));
}
