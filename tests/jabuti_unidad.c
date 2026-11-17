// SPDX-License-Identifier: GPL-3.0-or-later
/* Pruebas de unidad del motor de «Jabuti» (D377) en el anfitrión: el texto del proyecto (lectura y escritura),
 * el intérprete (movimiento, giros, bucles, condiciones, variables, listas, mensajes, concurrencia, lápiz,
 * preguntas) y los siete ejemplos. Se compila con: gcc -DSAGUI_HOST -Iuserland/inc -Iuserland/jabuti (lo hace h379). */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include "jb.h"
#include "compressao.h"

/* Lo que el motor pide a la libsagui. */
const unsigned char fonte_sagui[256][16];
size_t tam_texto(const char *s) { return strlen(s); }
int compara_texto(const char *a, const char *b) { return strcmp(a, b); }
int compara_texto_n(const char *a, const char *b, size_t n) { return strncmp(a, b, n); }
char *copia_texto(char *dst, const char *src, size_t max) { if (!max) return dst; size_t i = 0; for (; i + 1 < max && src[i]; i++) dst[i] = src[i]; dst[i] = 0; return dst; }
char *junta_texto(char *dst, const char *src, size_t max) { size_t n = strlen(dst); copia_texto(dst + n, src, max - n); return dst; }
char *acha_char(const char *s, int c) { return strchr(s, c); }
char *acha_ultimo_char(const char *s, int c) { return strrchr(s, c); }
int formatar_texto(char *dst, size_t max, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsnprintf(dst, max, fmt, ap); va_end(ap); return r; }
void *aloca(size_t n) { return malloc(n); }
void *aloca_zerado(size_t n) { return calloc(1, n); }
void *realoca(void *p, size_t n) { return realloc(p, n); }
void libera(void *p) { free(p); }
const char *idioma_traducir(const char *s) { return s; }
const char *idioma_traducir_sl(const char *s) { return s; }
void registrar(const char *fmt, ...) { (void)fmt; }
int efeito_tocar(const nota_t *n, int c) { (void)n; return c; }
void efeito_parar(void) {}
int abrir(const char *c, int m) { (void)c; (void)m; return -1; }
int escrever_tudo(int fd, const void *b, unsigned n) { (void)fd; (void)b; return (int)n; }
int fechar(int fd) { (void)fd; return 0; }
char *ler_todo_fd(int fd, unsigned *n) { (void)fd; (void)n; return NULL; }
int zip_adicionar(zip_escritor_t *z, const char *n, const uint8_t *d, uint32_t t, uint32_t m, int l, bool p, uint32_t md) { (void)z; (void)n; (void)d; (void)t; (void)m; (void)l; (void)p; (void)md; return -1; }
int zip_concluir(zip_escritor_t *z) { (void)z; return -1; }
void zip_escritor_liberar(zip_escritor_t *z) { (void)z; }
int zip_abrir(zip_leitor_t *z, const uint8_t *d, int n) { (void)z; (void)d; (void)n; return -1; }
int zip_extrair(const zip_leitor_t *z, int i, uint8_t **s) { (void)z; (void)i; (void)s; return -1; }
void zip_leitor_liberar(zip_leitor_t *z) { (void)z; }

static int fallos, total;
#define VER(c, ...) do { total++; if (!(c)) { fallos++; printf("FALLO %d: ", __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static bool cargar(const char *t)
{
    bool ok = jb_leer_texto(t, (unsigned)strlen(t));
    if (!ok) printf("  (error de lectura: %s)\n", jb_p.error);
    return ok;
}
static int correr(int max, bool parar_si_acaba)
{
    jb_motor_iniciar();
    jb_motor_bandera();
    int c;
    for (c = 1; c <= max; c++) {
        jb_motor_cuadro((unsigned)c * 33);
        if (parar_si_acaba && !jb_motor_activo()) break;
    }
    return c;
}
static double dx(const pers_t *p) { return p->x / 1000.0; }
static double dy(const pers_t *p) { return p->y / 1000.0; }
static double dd(const pers_t *p) { int d = p->dir % 360000; if (d > 180000) d -= 360000; return d / 1000.0; }
static int var_n(const char *n) { int i = jb_var_buscar(n); return i < 0 ? -999999 : jb_p.vars[i].n; }
static const char *CAB = "JABUTI 1\nfondo 0\nf \"Blanco\" biblioteca blanco\n";
static char buf[8192];
static const char *prog(const char *cuerpo)
{
    snprintf(buf, sizeof buf, "%spersonaje \"P\" biblioteca sagui\n  pos 0 0 90 100 1 0 0\n  guion 10 10\n%s", CAB, cuerpo);
    return buf;
}

static char *leer_fich(const char *ruta)
{
    FILE *f = fopen(ruta, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *d = malloc((size_t)n + 1);
    if (fread(d, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(d); return NULL; }
    d[n] = 0;
    fclose(f);
    return d;
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "rootfs/ejemplos/jabuti";

    /* ---- números */
    int32_t v;
    VER(jb_num_parse("12.5", &v) && v == 12500, "12.5");
    VER(jb_num_parse("-3", &v) && v == -3000, "-3");
    VER(jb_num_parse(".25", &v) && v == 250, ".25");
    VER(!jb_num_parse("hola", &v) && !jb_num_parse("1a", &v) && !jb_num_parse("", &v), "no numéricos");
    char t[32];
    jb_num_texto(12500, t, sizeof t); VER(!strcmp(t, "12.5"), "formato 12.5: %s", t);
    jb_num_texto(-250, t, sizeof t); VER(!strcmp(t, "-0.25"), "formato -0.25: %s", t);
    jb_num_texto(3000, t, sizeof t); VER(!strcmp(t, "3"), "formato 3: %s", t);
    VER(jb_raiz(144) == 12 && jb_raiz(2) == 1 && jb_raiz(0) == 0, "raíz entera");
    VER(jb_seno(90000) == 10000 && jb_seno(0) == 0 && jb_seno(-90000) == -10000 && jb_coseno(0) == 10000, "seno y coseno");
    VER(abs(jb_seno(30000) - 5000) <= 1 && abs(jb_seno(45500) - (int)(sin(45.5 * M_PI / 180) * 10000)) <= 3, "seno interpolado");

    /* ---- el caso del encargo: repetir 10, mover 10, girar 15 */
    VER(cargar(prog("    bandera\n    repetir 10\n      mover 10\n      girar-der 15\n")), "carga");
    int c = correr(200, true);
    double x = 0, y = 0, d = 90;
    for (int i = 0; i < 10; i++) { x += 10 * sin(d * M_PI / 180); y += 10 * cos(d * M_PI / 180); d += 15; }
    pers_t *p = jb_p.pers[0];
    VER(fabs(dx(p) - x) < 0.1 && fabs(dy(p) - y) < 0.1, "repetir/mover/girar: (%.3f, %.3f) esperado (%.3f, %.3f)", dx(p), dy(p), x, y);
    VER(fabs(dd(p) - (d > 180 ? d - 360 : d)) < 0.01, "dirección %.3f esperado %.3f", dd(p), d);
    VER(c <= 20, "termina pronto (%d cuadros)", c);

    /* ---- bucles, condiciones y variables */
    /* «si-no» con su «sino»: se escribe como si-no */
    VER(!cargar("JABUTI 1\nfondo 0\npersonaje \"P\" biblioteca sagui\n  guion 1 1\n    bandera\n    si (igual 1 1)\n      mover 1\n    sino\n      mover 2\n"), "«sino» sin «si-no» da error");
    VER(strstr(jb_p.error, "sino") != NULL, "mensaje de error amable: %s", jb_p.error);
    VER(cargar("JABUTI 1\nfondo 0\nvariable \"n\" 0 1\nvariable \"suma\" 0 1\npersonaje \"P\" biblioteca sagui\n  guion 1 1\n    bandera\n    repetir-hasta (mayor (var \"n\") 9)\n      cambiar-var \"n\" 1\n      si-no (igual (resto (var \"n\") 2) 0)\n        cambiar-var \"suma\" (var \"n\")\n      sino\n        cambiar-var \"suma\" -1\n"), "carga si-no");
    correr(200, true);
    /* n = 1..10: pares 2+4+6+8+10 = 30; impares: -5 */
    VER(var_n("n") == 10000 && var_n("suma") == 25000, "n=%d suma=%d (esperaba 10 y 25)", var_n("n") / 1000, var_n("suma") / 1000);

    /* ---- esperar */
    VER(cargar(prog("    bandera\n    esperar 1\n    fijar-x 50\n")), "carga esperar");
    jb_motor_iniciar(); jb_motor_bandera();
    int cuando = 0;
    for (int i = 1; i < 100 && !cuando; i++) { jb_motor_cuadro((unsigned)i * 33); if (jb_p.pers[0]->x == 50000) cuando = i; }
    VER(cuando >= 30 && cuando <= 33, "esperar 1 s ocupa unos 30 cuadros (%d)", cuando);

    /* ---- deslizar */
    VER(cargar(prog("    bandera\n    deslizar 2 100 -50\n")), "carga deslizar");
    jb_motor_iniciar(); jb_motor_bandera();
    for (int i = 1; i <= 30; i++) jb_motor_cuadro((unsigned)i * 33);
    VER(fabs(dx(jb_p.pers[0]) - 47.85) < 3, "a mitad de camino x aprox. 48: %.2f", dx(jb_p.pers[0]));
    for (int i = 31; i <= 90; i++) jb_motor_cuadro((unsigned)i * 33);
    VER(jb_p.pers[0]->x == 100000 && jb_p.pers[0]->y == -50000, "llega a (100, -50): %.3f %.3f", dx(jb_p.pers[0]), dy(jb_p.pers[0]));

    /* ---- operadores y texto */
    VER(cargar("JABUTI 1\nfondo 0\nvariable \"a\" 0 1\nvariable \"s\" \"\" 1\nvariable \"b\" 0 1\nvariable \"l\" 0 1\nvariable \"r\" 0 1\nvariable \"q\" 0 1\nvariable \"z\" 0 1\npersonaje \"P\" biblioteca sagui\n  guion 1 1\n    bandera\n"
               "    fijar-var \"a\" (suma 2 (mult 3 4))\n    fijar-var \"s\" (unir \"ab\" (letra 2 \"xyz\"))\n    fijar-var \"b\" (div 10 4)\n    fijar-var \"l\" (longitud \"hola\")\n    fijar-var \"r\" (resto -7 3)\n    fijar-var \"q\" (raiz 144)\n    fijar-var \"z\" (div 1 0)\n"), "carga operadores");
    correr(20, true);
    VER(var_n("a") == 14000, "2 + 3*4 = 14 (%d)", var_n("a"));
    VER(!strcmp(jb_p.vars[jb_var_buscar("s")].t, "aby"), "unir/letra: %s", jb_p.vars[jb_var_buscar("s")].t);
    VER(var_n("b") == 2500, "10 / 4 = 2.5 (%d)", var_n("b"));
    VER(var_n("l") == 4000, "longitud");
    VER(var_n("r") == 2000, "resto -7 mod 3 = 2 (%d)", var_n("r"));
    VER(var_n("q") == 12000, "raíz de 144 (%d)", var_n("q"));
    VER(var_n("z") == 0 && jb_aviso[0], "dividir entre cero da 0 con un aviso amable: %s", jb_aviso);

    /* ---- concurrencia: dos personajes a la vez */
    VER(cargar("JABUTI 1\nfondo 0\nvariable \"x\" 0 1\npersonaje \"A\" biblioteca sagui\n  guion 1 1\n    bandera\n    siempre\n      sumar-x 1\n"
               "personaje \"B\" biblioteca pelota\n  guion 1 1\n    bandera\n    siempre\n      sumar-y 2\n      cambiar-var \"x\" 1\n"), "carga concurrente");
    correr(30, false);
    VER(jb_p.pers[0]->x == 30000 && jb_p.pers[1]->y == 60000 && var_n("x") == 30000, "dos personajes avanzan a la vez: %.0f %.0f %d", dx(jb_p.pers[0]), dy(jb_p.pers[1]), var_n("x") / 1000);
    VER(jb_motor_activo() && jb_motor_hilos() == 2, "siguen vivos los dos hilos");
    jb_motor_parar();
    VER(!jb_motor_activo(), "parar mata los hilos");

    /* ---- mensajes */
    VER(cargar("JABUTI 1\nfondo 0\nvariable \"r\" 0 1\npersonaje \"A\" biblioteca sagui\n  guion 1 1\n    bandera\n    enviar \"mensaje2\"\n"
               "personaje \"B\" biblioteca pelota\n  guion 1 1\n    recibir \"mensaje2\"\n    cambiar-var \"r\" 5\n"), "carga mensajes");
    correr(5, false);
    VER(var_n("r") == 5000, "recibir mensaje (%d)", var_n("r"));

    /* ---- teclas y clics */
    VER(cargar(prog("    tecla \"a\"\n    fijar-x 77\n") ), "carga tecla");
    jb_motor_iniciar(); jb_motor_tecla('a'); jb_motor_cuadro(33);
    VER(jb_p.pers[0]->x == 77000, "al pulsar la tecla a");
    VER(cargar(prog("    clic\n    fijar-y 33\n")), "carga clic");
    jb_motor_iniciar(); jb_motor_clic(100, 100); jb_motor_cuadro(33);
    VER(jb_p.pers[0]->y == 0, "clic fuera del personaje no hace nada");
    jb_motor_clic(0, 0); jb_motor_cuadro(66);
    VER(jb_p.pers[0]->y == 33000, "clic sobre el personaje lo dispara");

    /* ---- tocando */
    VER(cargar("JABUTI 1\nfondo 0\nvariable \"t1\" 0 1\nvariable \"t2\" 0 1\nvariable \"t3\" 0 1\npersonaje \"A\" biblioteca sagui\n  pos 0 0 90 100 1 0 0\n  guion 1 1\n    bandera\n"
               "    si (tocando \"B\")\n      fijar-var \"t1\" 1\n    ir-xy 200 0\n    si (tocando \"B\")\n      fijar-var \"t2\" 1\n    ir-xy 238 0\n    si (tocando \"borde\")\n      fijar-var \"t3\" 1\n"
               "personaje \"B\" biblioteca pelota\n  pos 10 0 90 100 1 0 0\n"), "carga tocando");
    correr(10, true);
    VER(var_n("t1") == 1000 && var_n("t2") == 0 && var_n("t3") == 1000, "tocando personaje y borde: %d %d %d", var_n("t1") / 1000, var_n("t2") / 1000, var_n("t3") / 1000);

    /* ---- rebotar */
    VER(cargar(prog("    bandera\n    ir-xy 230 0\n    apuntar 90\n    rebotar\n")), "carga rebotar");
    correr(5, true);
    VER(fabs(dd(jb_p.pers[0]) + 90) < 0.5 && dx(jb_p.pers[0]) < 230, "rebotar en el borde derecho: dir %.1f x %.1f", dd(jb_p.pers[0]), dx(jb_p.pers[0]));

    /* ---- apuntar hacia el ratón */
    VER(cargar(prog("    bandera\n    apuntar-a \"raton\"\n")), "carga apuntar-a");
    jb_raton_x = 0; jb_raton_y = 100;
    correr(3, true);
    VER(fabs(dd(jb_p.pers[0])) < 0.5, "apuntar arriba: %.2f", dd(jb_p.pers[0]));
    jb_raton_x = -100; jb_raton_y = 0;
    correr(3, true);
    VER(fabs(dd(jb_p.pers[0]) + 90) < 0.5, "apuntar a la izquierda: %.2f", dd(jb_p.pers[0]));
    jb_raton_x = 100; jb_raton_y = -100;
    correr(3, true);
    VER(fabs(dd(jb_p.pers[0]) - 135) < 0.5, "apuntar abajo a la derecha: %.2f", dd(jb_p.pers[0]));

    /* ---- listas */
    VER(cargar("JABUTI 1\nfondo 0\nvariable \"n\" 0 1\nlista \"L\"\npersonaje \"P\" biblioteca sagui\n  guion 1 1\n    bandera\n    lista-agregar \"hola\" \"L\"\n    lista-agregar 5 \"L\"\n    fijar-var \"n\" (lista-long \"L\")\n    lista-vaciar \"L\"\n"), "carga listas");
    correr(5, true);
    VER(var_n("n") == 2000 && jb_p.listas[0].n == 0, "listas: %d", var_n("n") / 1000);

    /* ---- pregunta y respuesta */
    VER(cargar("JABUTI 1\nfondo 0\nvariable \"x\" 0 1\npersonaje \"P\" biblioteca sagui\n  guion 1 1\n    bandera\n    preguntar \"¿Cuánto?\"\n    fijar-var \"x\" (suma (respuesta) 1)\n"), "carga preguntar");
    jb_motor_iniciar(); jb_motor_bandera();
    jb_motor_cuadro(33);
    VER(jb_hay_pregunta, "hay una pregunta");
    strcpy(jb_respuesta, "41"); jb_responder();
    jb_motor_cuadro(66); jb_motor_cuadro(99);
    VER(var_n("x") == 42000, "respuesta 41 + 1 = %d", var_n("x") / 1000);

    /* ---- lápiz y colores */
    VER(cargar(prog("    bandera\n    lapiz-color \"rojo\"\n    ir-xy 0 0\n    lapiz-bajar\n    ir-xy 40 0\n    lapiz-subir\n")), "carga lápiz");
    jb_lapiz_borrar();
    correr(5, true);
    VER(jb_color_en(20, 0) == 8 && jb_color_en(20, 30) == 6, "el lápiz pintó rojo y el fondo es blanco: %d %d", jb_color_en(20, 0), jb_color_en(20, 30));
    jb_lapiz_borrar();

    /* ---- ejemplos: lectura, escritura y vuelta */
    const char *ej[] = { "01-el-sagui-que-salta", "02-atrapa-la-fruta", "03-dibujar-formas", "04-conversacion", "05-carrera-con-mensajes", "06-cuenta-atras-del-cohete", "07-musica-y-trajes" };
    for (int i = 0; i < 7; i++) {
        char ruta[300];
        snprintf(ruta, sizeof ruta, "%s/%s.jbt", dir, ej[i]);
        char *txt = leer_fich(ruta);
        VER(txt != NULL, "existe %s", ruta);
        if (!txt) continue;
        VER(jb_leer_texto(txt, (unsigned)strlen(txt)), "%s se lee: %s", ej[i], jb_p.error);
        unsigned n1; char *a = jb_escribir_texto(&n1);
        VER(jb_leer_texto(a, n1), "%s se vuelve a leer: %s", ej[i], jb_p.error);
        unsigned n2; char *b = jb_escribir_texto(&n2);
        VER(n1 == n2 && memcmp(a, b, n1) == 0, "%s: escribir, leer y escribir da lo mismo", ej[i]);
        int g = 0, bl = 0;
        for (int k = 0; k < jb_p.npers; k++) { g += jb_p.pers[k]->nguiones; for (int q = 0; q < jb_p.pers[k]->nguiones; q++) for (blk_t *s = jb_p.pers[k]->guiones[q].top; s; s = s->sig) bl++; }
        VER(g >= 1 && bl >= 3, "%s tiene guiones y bloques (%d, %d)", ej[i], g, bl);
        free(a); free(b); free(txt);
    }
    /* ejecución de los ejemplos */
    {
        char ruta[300]; char *txt;
        snprintf(ruta, sizeof ruta, "%s/03-dibujar-formas.jbt", dir); txt = leer_fich(ruta);
        jb_leer_texto(txt, (unsigned)strlen(txt)); free(txt); jb_lapiz_borrar();
        correr(400, true);
        VER(fabs(dx(jb_p.pers[0]) - 120) < 1.5 && fabs(dy(jb_p.pers[0]) - 50) < 1.5, "formas: vuelve al punto de partida (%.2f, %.2f)", dx(jb_p.pers[0]), dy(jb_p.pers[0]));
        VER(jb_color_en(-110, 50) == 8 && jb_color_en(-100, 50) == 8, "el cuadrado rojo está dibujado (%d)", jb_color_en(-110, 50));
        jb_lapiz_borrar();

        snprintf(ruta, sizeof ruta, "%s/06-cuenta-atras-del-cohete.jbt", dir); txt = leer_fich(ruta);
        jb_leer_texto(txt, (unsigned)strlen(txt)); free(txt);
        correr(400, true);
        VER(var_n("cuenta") == 0 && dy(jb_p.pers[0]) > 150 && jb_p.pers[0]->traje == 1 && jb_p.fondo == 1, "cohete: cuenta atrás (%d), y=%.0f, traje=%d, fondo=%d", var_n("cuenta") / 1000, dy(jb_p.pers[0]), jb_p.pers[0]->traje, jb_p.fondo);

        snprintf(ruta, sizeof ruta, "%s/05-carrera-con-mensajes.jbt", dir); txt = leer_fich(ruta);
        jb_leer_texto(txt, (unsigned)strlen(txt)); free(txt);
        correr(400, true);
        VER(jb_p.npers == 3 && fabs(dx(jb_p.pers[1]) - 190) < 0.5 && fabs(dx(jb_p.pers[2]) - 190) < 0.5, "carrera: llegan los dos (%.1f, %.1f)", dx(jb_p.pers[1]), dx(jb_p.pers[2]));

        snprintf(ruta, sizeof ruta, "%s/01-el-sagui-que-salta.jbt", dir); txt = leer_fich(ruta);
        jb_leer_texto(txt, (unsigned)strlen(txt)); free(txt);
        jb_motor_iniciar(); jb_motor_bandera();
        for (int i = 1; i <= 10; i++) jb_motor_cuadro((unsigned)i * 33);
        jb_motor_tecla(' ');
        int ymax = -999;
        for (int i = 11; i <= 60; i++) { jb_motor_cuadro((unsigned)i * 33); if (jb_p.pers[0]->y / 1000 > ymax) ymax = jb_p.pers[0]->y / 1000; }
        VER(ymax == -110 + 12 * 9 && jb_p.pers[0]->y == -110000, "el sagui salta %d y vuelve (%.0f)", ymax + 110, dy(jb_p.pers[0]));

        snprintf(ruta, sizeof ruta, "%s/04-conversacion.jbt", dir); txt = leer_fich(ruta);
        jb_leer_texto(txt, (unsigned)strlen(txt)); free(txt);
        jb_auto_resp = "Ana";
        correr(80, false);
        VER(strstr(jb_p.pers[0]->burbuja, "Dentro de 10") != NULL, "conversación: %s", jb_p.pers[0]->burbuja);
        jb_auto_resp = NULL;

        snprintf(ruta, sizeof ruta, "%s/02-atrapa-la-fruta.jbt", dir); txt = leer_fich(ruta);
        jb_leer_texto(txt, (unsigned)strlen(txt)); free(txt);
        correr(600, false);
        VER(var_n("puntos") >= 0 && jb_p.pers[1]->y <= 150000 && jb_p.pers[1]->y > -200000, "fruta: puntos=%d y=%.0f", var_n("puntos") / 1000, dy(jb_p.pers[1]));
        jb_motor_parar();

        snprintf(ruta, sizeof ruta, "%s/07-musica-y-trajes.jbt", dir); txt = leer_fich(ruta);
        jb_leer_texto(txt, (unsigned)strlen(txt)); free(txt);
        correr(400, true);
        VER(jb_p.pers[0]->traje == 0 && fabs(dd(jb_p.pers[0]) - 90 - 8 * 45 + 360) < 0.1 + 0, "música: 8 trajes alternados y giro total (traje %d, dir %.1f)", jb_p.pers[0]->traje, dd(jb_p.pers[0]));
    }

    /* ---- el dibujo del escenario: el personaje se ve, gira y cambia de tamaño sin cambiar de sitio */
    {
        static uint32_t vista[480 * 360];
        VER(cargar("JABUTI 1\nfondo 0\nf \"Blanco\" biblioteca blanco\npersonaje \"A\" biblioteca pelota\n  pos 0 0 90 100 1 0 0\n"), "carga dibujo");
        int cuenta[4] = { 0 }, tam[4] = { 100, 100, 50, 200 }, dir[4] = { 90, 0, 90, 90 };
        long cx[4] = { 0 }, cy[4] = { 0 };
        for (int k = 0; k < 4; k++) {
            jb_p.pers[0]->tam = tam[k]; jb_p.pers[0]->dir = dir[k] * 1000;
            jb_escena_dibujar(vista, 480, 480, 360);
            for (int y = 0; y < 360; y++) for (int x = 0; x < 480; x++) if (vista[y * 480 + x] != jb_paleta[6]) { cuenta[k]++; cx[k] += x; cy[k] += y; }
        }
        VER(cuenta[0] > 1000, "la pelota se dibuja (%d píxeles)", cuenta[0]);
        VER(abs(cuenta[1] - cuenta[0]) * 20 < cuenta[0], "girar no cambia el tamaño visible (%d y %d)", cuenta[0], cuenta[1]);
        VER(cuenta[2] * 3 < cuenta[0] && cuenta[3] > cuenta[0] * 3, "el tamaño cambia el área (%d, %d, %d)", cuenta[2], cuenta[0], cuenta[3]);
        for (int k = 0; k < 4; k++) VER(abs((int)(cx[k] / cuenta[k]) - 240) < 6 && abs((int)(cy[k] / cuenta[k]) - 180) < 6, "centrada en el escenario (%ld, %ld)", cx[k] / cuenta[k], cy[k] / cuenta[k]);
        jb_p.pers[0]->tam = 100; jb_p.pers[0]->dir = 90000;
        VER(jb_pers_en(0, 0) == 0 && jb_pers_en(200, 150) == -1, "un clic encuentra al personaje");
        /* a un tercio del tamaño de la vista (160x120) también */
        jb_escena_dibujar(vista, 160, 160, 120);
        int n3 = 0; for (int y = 0; y < 120; y++) for (int x = 0; x < 160; x++) if (vista[y * 160 + x] != jb_paleta[6]) n3++;
        VER(n3 * 7 > cuenta[0] / 9 * 7 - 200 && n3 > 100, "en la vista pequeña también se dibuja (%d)", n3);
    }

    /* ---- la cabecera de las primeras versiones de trabajo («BLOQUES 1») sigue valiendo */
    VER(jb_leer_texto("BLOQUES 1\nfondo 0\npersonaje \"A\" biblioteca sagui\n  guion 1 1\n    bandera\n", 60), "cabecera BLOQUES");

    /* ---- errores de lectura con mensaje amable */
    VER(!jb_leer_texto("hola\n", 5) && strstr(jb_p.error, "no es un proyecto"), "no es un proyecto: %s", jb_p.error);
    VER(!cargar("JABUTI 1\npersonaje \"A\" biblioteca sagui\n  guion 1 1\n    cualquiercosa 5\n") && strstr(jb_p.error, "línea 4"), "bloque desconocido: %s", jb_p.error);

    printf("%d comprobaciones, %d fallos\n", total, fallos);
    if (!fallos) printf("todo bien\n");
    return fallos ? 1 : 0;
}
