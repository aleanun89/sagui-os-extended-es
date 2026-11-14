// SPDX-License-Identifier: GPL-2.0-or-later
/* gnuboy_unidad.c - GNUBoy (el emulador del upstream, sin tocar) corriendo en el PC contra la
 * ROM de prueba de tools/gnuboy_rom_teste.py (D235). Sin SDL ni ventana: este fichero pone el
 * vídeo (el mismo formato de píxeles que sys/sdl2/sdl-video.c), el sonido (recoge lo que el
 * APU mezcla) y las teclas (por guion), y comprueba lo que la ROM promete:
 *
 *   - los cuatro cuadrantes con los cuatro tonos de la paleta verde de GNUBoy;
 *   - A invierte la paleta mientras se mantiene; Derecha y B desplazan la imagen;
 *   - suena un tono de 1000 Hz, y Select lo silencia;
 *   - Start escribe «SAGUI» en la RAM de la batería, el emulador la guarda en el .sav al salir
 *     (atexit) y una segunda ejecución la encuentra (el cuadrante de abajo a la derecha, oscuro).
 *
 * Uso:  gnuboy_unidad ROM.gb [segunda]
 *   sin «segunda»: el guion completo (escribe ROM.sav en el directorio actual);
 *   con «segunda»: arranca de nuevo con el .sav y mira el cuadrante.
 * Compilación: la hace tests/h235_gnuboy.py (gcc con las fuentes del upstream).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "defs.h"
#include "fb.h"
#include "input.h"
#include "rc.h"
#include "pcm.h"
#include "sys.h"
#include "rckeys.h"
#include "emu.h"
#include "exports.h"
#include "loader.h"
#include "io.h"

struct fb fb;
struct pcm pcm;

/* Las variables de configuración de cada parte (las que declaran sys/sdl2/*.c): ninguna. */
rcvar_t vid_exports[] = { RCV_END };
rcvar_t pcm_exports[] = { RCV_END };
rcvar_t joy_exports[] = { RCV_END };

static byte pix[160 * 144 * 4];
static int cuadro;              /* cuadros dibujados */
static int segunda;
static int fallos;

/* Lo que se recoge del sonido: la amplitud pico a pico y los cruces por cero del canal izquierdo,
 * cuadro a cuadro. */
#define MAX_CUADROS 400
static int pico[MAX_CUADROS], cruces[MAX_CUADROS], muestras[MAX_CUADROS];

static unsigned pixel(int x, int y)
{
    return ((unsigned *)pix)[y * 160 + x] & 0xFFFFFF;
}

/* Los cuatro tonos de la paleta (src/lcd.c, GB_SDL2GNUBOY_PALETTE = 0x0fbc9b, 0x0fac8b, 0x306230,
 * 0x0f380f, escritos como azul-verde-rojo y recortados a 5 bits por canal), tal como salen en el
 * píxel de 32 bits 0x00RRGGBB con el formato de sys/sdl2/sdl-video.c: el verde amarillento de la
 * Game Boy original. */
#define TONO0 0x9cbd08
#define TONO1 0x8cad08
#define TONO2 0x316331
#define TONO3 0x083908

static void espero(const char *que, unsigned obtenido, unsigned esperado)
{
    int bien = obtenido == esperado;
    printf("  %s %s: %06x%s\n", bien ? "ok:" : "FALLO:", que, obtenido, bien ? "" : " (esperado distinto)");
    if (!bien) {
        printf("       esperado %06x\n", esperado);
        fallos++;
    }
}

static void verifico(const char *que, int condicion)
{
    printf("  %s %s\n", condicion ? "ok:" : "FALLO:", que);
    if (!condicion) fallos++;
}

/* ---- vídeo ---- */
void vid_init(void)
{
    fb.w = 160; fb.h = 144; fb.pelsize = 4; fb.pitch = 160 * 4; fb.indexed = 0; fb.ptr = pix;
    fb.cc[0].r = fb.cc[1].r = fb.cc[2].r = fb.cc[3].r = 0;
    fb.cc[0].l = 16; fb.cc[1].l = 8; fb.cc[2].l = 0; fb.cc[3].l = 0;
    fb.enabled = 1; fb.dirty = 0;
}
void vid_preinit(void) {}
void vid_close(void) {}
void vid_setpal(int i, int r, int g, int b) {}
void vid_settitle(char *t) {}
void vid_begin(void) { fb.ptr = pix; }
void vid_fullscreen_toggle(void) {}
void vid_screenshot(void) {}

static void guion_cuadro(void);
void vid_end(void)
{
    fb.ptr = pix;
    cuadro++;
    guion_cuadro();
}

/* ---- sonido: lo que se mezcla en cada cuadro ---- */
static byte bufsom[2048];
void pcm_init(void)
{
    pcm.hz = 44100; pcm.stereo = 1; pcm.len = sizeof bufsom; pcm.buf = bufsom; pcm.pos = 0;
    memset(bufsom, 128, sizeof bufsom);
}
void pcm_close(void) {}
void pcm_pause(void) {}
void pcm_resume(void) {}
int pcm_submit(void)
{
    if (!pcm.buf) return 0;
    if (pcm.pos < pcm.len) return 1;
    int c = cuadro < MAX_CUADROS ? cuadro : MAX_CUADROS - 1;
    int mn = 255, mx = 0;
    for (int i = 0; i < pcm.len; i += 2) {   /* el canal izquierdo, sin signo de 8 bits */
        unsigned char v = pcm.buf[i];
        if (v < mn) mn = v;
        if (v > mx) mx = v;
    }
    /* La onda cuadrada sube de 128: los cruces se cuentan por el punto medio del bloque. */
    int medio = (mn + mx) / 2, antes = pcm.buf[0] > medio;
    if (mx - mn >= 4)
        for (int i = 2; i < pcm.len; i += 2) {
            int ahora = pcm.buf[i] > medio;
            if (ahora != antes) cruces[c]++;
            antes = ahora;
        }
    muestras[c] += pcm.len / 2;
    if (mx - mn > pico[c]) pico[c] = mx - mn;
    pcm.pos = 0;
    return 1;
}

/* ---- sistema ---- */
void *sys_timer(void) { return malloc(4); }
int sys_elapsed(int *prev) { return 0; }
void sys_sleep(int us) {}
void sys_checkdir(char *path, int wr) {}
void sys_initpath(void)
{
    char *p = ".";
    if (rc_getstr("rcpath") == NULL) rc_setvar("rcpath", 1, &p);
    if (rc_getstr("savedir") == NULL) rc_setvar("savedir", 1, &p);
}
void sys_sanitize(char *s) {}
void joy_init(void) {}
void joy_close(void) {}
void ev_poll(void) {}
int confirm_exit(void) { return 0; }

/* ---- teclas por guion ---- */
static void tecla(int codigo, int apretada)
{
    event_t ev;
    ev.type = apretada ? EV_PRESS : EV_RELEASE;
    ev.code = codigo;
    ev_postevent(&ev);
}

static void guion_cuadro(void)
{
    static const struct { int cuadro, codigo, apretada; } pasos[] = {
        { 70, 'z', 1 },  { 100, 'z', 0 },          /* A: paleta invertida */
        { 130, K_RIGHT, 1 }, { 160, K_RIGHT, 0 },  /* Derecha: SCX = 8 */
        { 170, 'x', 1 }, { 200, 'x', 0 },          /* B: SCY = 8 */
        { 210, K_SHIFT, 1 }, { 260, K_SHIFT, 0 },  /* Select: silencio */
        { 270, K_ENTER, 1 }, { 290, K_ENTER, 0 },  /* Start: escribe «SAGUI» */
    };
    if (segunda) return;
    for (unsigned i = 0; i < sizeof pasos / sizeof pasos[0]; i++)
        if (pasos[i].cuadro == cuadro) tecla(pasos[i].codigo, pasos[i].apretada);
}

static int ultimo_pico(int desde, int hasta)
{
    int m = 0;
    for (int c = desde; c <= hasta; c++) if (pico[c] > m) m = pico[c];
    return m;
}

void doevents(void)
{
    event_t ev;
    while (ev_getevent(&ev)) {
        if (ev.type != EV_PRESS && ev.type != EV_RELEASE) continue;
        rc_dokey(ev.code, ev.type != EV_RELEASE);
    }
    int c = cuadro;
    /* Las fotos, en los cuadros del guion. */
    if (segunda) {
        if (c == 60) {
            printf("segunda ejecución (con el .sav):\n");
            espero("abajo a la derecha, con la marca guardada, es el tono 3", pixel(120, 108), TONO3);
            espero("arriba a la izquierda no cambia (tono 3)", pixel(40, 36), TONO3);
            exit(fallos ? 1 : 0);
        }
        return;
    }
    if (c == 60) {
        printf("cuadro 60, sin teclas:\n");
        espero("arriba a la izquierda: tono 3", pixel(40, 36), TONO3);
        espero("arriba a la derecha: tono 1", pixel(120, 36), TONO1);
        espero("abajo a la izquierda: tono 2", pixel(40, 108), TONO2);
        espero("abajo a la derecha: tono 0", pixel(120, 108), TONO0);
        espero("el borde: la última fila de la izquierda (143) es del cuadrante de abajo", pixel(40, 143), TONO2);
    }
    if (c == 90) {
        printf("cuadro 90, con A (Z) apretada:\n");
        espero("arriba a la izquierda invertida: tono 0", pixel(40, 36), TONO0);
        espero("arriba a la derecha invertida: tono 2", pixel(120, 36), TONO2);
        espero("abajo a la izquierda invertida: tono 1", pixel(40, 108), TONO1);
        espero("abajo a la derecha invertida: tono 3", pixel(120, 108), TONO3);
    }
    if (c == 120) {
        printf("cuadro 120, A suelta:\n");
        espero("la paleta vuelve (tono 3 arriba a la izquierda)", pixel(40, 36), TONO3);
    }
    if (c == 150) {
        printf("cuadro 150, con Derecha:\n");
        espero("el límite entre los cuadrantes pasa de x=80 a x=72: (76, 36) es el de la derecha (tono 1)",
               pixel(76, 36), TONO1);
        espero("(68, 36) sigue siendo el de la izquierda (tono 3)", pixel(68, 36), TONO3);
    }
    if (c == 190) {
        printf("cuadro 190, con B (X):\n");
        espero("el límite de arriba y abajo pasa de y=72 a y=64: (40, 68) es el de abajo (tono 2)",
               pixel(40, 68), TONO2);
        espero("(40, 60) sigue siendo el de arriba (tono 3)", pixel(40, 60), TONO3);
    }
    if (c == 200) {
        printf("sonido antes de Select (cuadros 20-60):\n");
        int c0 = 0, m0 = 0;
        for (int i = 20; i <= 60; i++) { c0 += cruces[i]; m0 += muestras[i]; }
        double hz = m0 ? c0 / 2.0 * 44100.0 / m0 : 0;
        printf("  cruces %d en %d muestras: %.0f Hz\n", c0, m0, hz);
        verifico("suena un tono cuadrado de unos 1000 Hz (900-1100)", hz > 900 && hz < 1100);
        verifico("con amplitud (pico a pico > 10)", ultimo_pico(20, 60) > 10);
    }
    if (c == 255) {
        printf("sonido con Select (cuadros 225-255):\n");
        verifico("Select lo silencia (pico a pico < 4)", ultimo_pico(225, 255) < 4);
    }
    if (c == 330) {
        printf("sonido después de soltar Select (cuadros 300-330):\n");
        verifico("vuelve a sonar (pico a pico > 10)", ultimo_pico(300, 330) > 10);
        printf("(el .sav lo escribe el emulador al salir)\n");
        exit(fallos ? 1 : 0);
    }
}

void die(char *fmt, ...)
{
    fprintf(stderr, "die: %s\n", fmt);
    exit(1);
}

int main(int argc, char **argv)
{
    if (argc < 2) { fprintf(stderr, "uso: gnuboy_unidad ROM.gb [segunda]\n"); return 2; }
    segunda = argc > 2;
    init_exports();
    sys_initpath();
    /* Las teclas de gnuboy.rc de principal_gb.c. */
    static const char *conf[] = {
        "bind up +up", "bind down +down", "bind left +left", "bind right +right",
        "bind z +a", "bind x +b", "bind enter +start", "bind shift +select", "bind tab +select",
        "set sound 1", "set savedir .", NULL
    };
    for (int i = 0; conf[i]; i++) rc_command((char *)conf[i]);
    if (io_setup("") == 0) {}
    vid_init();
    pcm_init();
    loader_init(argv[1]);
    emu_reset();
    emu_run();
    return 0;
}
