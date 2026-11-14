// SPDX-License-Identifier: GPL-2.0-or-later
/* terminal_unidad.c - prueba de unidad en el PC de la lógica del Terminal (D231, h231).
 *
 *   gcc -O2 -Wall -Wextra -DTERMINAL_ANFITRION -I userland/programas -o terminal_unidad tests/terminal_unidad.c
 *
 * Comprueba: el analizador de secuencias de escape, SGR y los colores de una celda, los temas y la
 * paleta ANSI, terminal.conf (leer, escribir y volver a leer), la búsqueda, los enlaces y el título de la
 * pestaña. Escribe "todo bien" y sale con 0 si pasa todo. */
#include <stdio.h>
#include <stdlib.h>
#include "terminal_logica.h"

static int fallos, total;
#define COMPROBAR(c, ...) do { total++; if (!(c)) { fallos++; printf("FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* Pasa un texto por el analizador; devuelve cuántos caracteres normales salen y deja el último CSI. */
static int alimentar(tl_esc_t *e, const char *s, char *normales, tl_esc_t *ultimo, int *csis)
{
    int n = 0;
    *csis = 0;
    for (; *s; s++) {
        int r = tl_esc_alimentar(e, (unsigned char)*s);
        if (r == TL_ESC_NAO) normales[n++] = *s;
        else if (r == TL_ESC_CSI) { *ultimo = *e; (*csis)++; }
    }
    normales[n] = 0;
    return n;
}

static void prueba_escapes(void)
{
    tl_esc_t e = {0}, u = {0};
    char t[64];
    int csis;
    alimentar(&e, "ab\x1b[Dcd", t, &u, &csis);
    COMPROBAR(!strcmp(t, "abcd") && csis == 1 && u.final == 'D' && tl_par(&u, 0, 1) == 1, "ESC[D: '%s' csis=%d", t, csis);
    alimentar(&e, "\x1b[31;1mrojo\x1b[0m", t, &u, &csis);
    COMPROBAR(!strcmp(t, "rojo") && csis == 2 && u.final == 'm', "SGR: '%s' %d", t, csis);
    alimentar(&e, "\x1b[12;40Hx", t, &u, &csis);
    COMPROBAR(u.final == 'H' && tl_par(&u, 0, 1) == 12 && tl_par(&u, 1, 1) == 40 && !strcmp(t, "x"), "ESC[12;40H");
    alimentar(&e, "\x1b[;5H", t, &u, &csis);
    COMPROBAR(tl_par(&u, 0, 1) == 1 && tl_par(&u, 1, 1) == 5, "parametro vacio vale el defecto");
    alimentar(&e, "\x1b[?25l", t, &u, &csis);
    COMPROBAR(u.privado && u.final == 'l' && tl_par(&u, 0, 0) == 25, "ESC[?25l");
    alimentar(&e, "\x1b[J", t, &u, &csis);
    COMPROBAR(u.final == 'J' && u.n == 0, "ESC[J sin parametros");
    alimentar(&e, "a\x1b]0;titulo\ab\x1b]2;otro\x1b\\c", t, &u, &csis);
    COMPROBAR(!strcmp(t, "abc") && csis == 0, "OSC descartado: '%s'", t);
    alimentar(&e, "\x1bRz", t, &u, &csis);
    COMPROBAR(!strcmp(t, "z"), "ESC + letra se descarta (como siempre): '%s'", t);
    alimentar(&e, "\x1b[1;2;3;4;5;6;7;8;9;10;11;12;13mq", t, &u, &csis);
    COMPROBAR(!strcmp(t, "q") && csis == 1, "demasiados parametros no desbordan");
}

/* D263: el texto de la OSC (título con «[root]») y los colores de 256 / RGB llevados a los 16 ANSI. */
static void prueba_osc_y_256(void)
{
    tl_esc_t e = {0};
    int oscs = 0;
    char ultimo[TL_OSC_MAX] = "";
    const char *sec = "x\x1b]0;[root] root@saguios:/\ay\x1b]2;normal\x1b\\";
    for (const char *p = sec; *p; p++) {
        int r = tl_esc_alimentar(&e, (unsigned char)*p);
        if (r == TL_ESC_OSC) { oscs++; strcpy(ultimo, e.osc); }
    }
    COMPROBAR(oscs == 2 && !strcmp(ultimo, "2;normal"), "OSC: %d '%s'", oscs, ultimo);
    COMPROBAR(tl_comeca("0;[root] root@saguios:/" + 2, "[root]"), "titulo de root");
    COMPROBAR(tl_256_a_16(1) == 1 && tl_256_a_16(9) == 9 && tl_256_a_16(15) == 15, "256: los 16 primeros");
    COMPROBAR(tl_256_a_16(196) == 9, "256: 196 (rojo puro) = rojo brillante, da %d", tl_256_a_16(196));
    COMPROBAR(tl_256_a_16(46) == 10, "256: 46 (verde puro) = verde brillante, da %d", tl_256_a_16(46));
    COMPROBAR(tl_256_a_16(21) == 12, "256: 21 (azul puro) = azul brillante, da %d", tl_256_a_16(21));
    COMPROBAR(tl_256_a_16(16) == 0 && tl_256_a_16(231) == 15, "256: negro y blanco del cubo");
    COMPROBAR(tl_256_a_16(232) == 0 && tl_256_a_16(255) == 15, "256: la escala de grises");
    tl_esc_t u = {0};
    char t[16];
    int csis;
    alimentar(&e, "\x1b[38;5;196;48;5;21mz", t, &u, &csis);
    uint16_t at = tl_sgr_todo(0, &u);
    COMPROBAR(TL_AT_FG(at) == 10 && TL_AT_BG(at) == 13, "38;5;196;48;5;21: fg=%u bg=%u", TL_AT_FG(at), TL_AT_BG(at));
}

static void prueba_sgr(void)
{
    tl_conf_t c;
    tl_conf_padrao(&c);
    tl_esc_t e = {0}, u = {0};
    char t[16];
    int csis;
    uint16_t at = 0;
    alimentar(&e, "\x1b[31;42m", t, &u, &csis);
    at = tl_sgr_todo(at, &u);
    COMPROBAR(TL_AT_FG(at) == 2 && TL_AT_BG(at) == 3, "31;42 -> fg 2 bg 3 (%u %u)", TL_AT_FG(at), TL_AT_BG(at));
    uint32_t fg, bg;
    tl_cores_da_celula(&c, at, &fg, &bg);
    COMPROBAR(fg == c.ansi[1] && bg == c.ansi[2], "colores ANSI de la celda");
    at = tl_sgr(at, 1);
    tl_cores_da_celula(&c, at, &fg, &bg);
    COMPROBAR(fg == c.ansi[9], "negrita + rojo = rojo brillante");
    at = tl_sgr(at, 7);
    tl_cores_da_celula(&c, at, &fg, &bg);
    COMPROBAR(fg == c.ansi[2] && bg == c.ansi[9], "inverso intercambia");
    at = tl_sgr(tl_sgr(at, 27), 22);
    at = tl_sgr(at, 39); at = tl_sgr(at, 49);
    COMPROBAR(at == 0, "39/49/22/27 vuelven al defecto (%u)", at);
    at = tl_sgr(0, 95);
    COMPROBAR(TL_AT_FG(at) == 14, "95 = magenta brillante (14)");
    at = tl_sgr(0, 104);
    COMPROBAR(TL_AT_BG(at) == 13, "104 = fondo azul brillante");
    tl_cores_da_celula(&c, 0, &fg, &bg);
    COMPROBAR(fg == c.letra && bg == c.fundo, "atributo 0 = colores de la tema");
    alimentar(&e, "\x1b[38;5;200;1mx", t, &u, &csis);
    at = tl_sgr_todo(0, &u);
    COMPROBAR(at == (TL_AT_NEGRITA | 14), "38;5;200 = magenta brillante (D263) y 1 sigue valiendo: %x", at);
    alimentar(&e, "\x1b[m", t, &u, &csis);
    COMPROBAR(tl_sgr_todo(0x123, &u) == 0, "ESC[m restablece");
}

static void prueba_temas(void)
{
    tl_conf_t c;
    tl_conf_padrao(&c);
    COMPROBAR(c.fundo == 0x12161E && c.letra == 0xD8DDE2 && c.cols == 94 && c.lins == 28 && c.hist == 500,
              "el tema por defecto es el de siempre (D231)");
    for (int i = 0; i < TL_TEMAS; i++) {
        tl_conf_tema(&c, i);
        COMPROBAR(c.tema == i && c.fundo == tl_temas[i].fundo, "tema %d", i);
        int distintos = 0;
        for (int k = 1; k < 16; k++) if (c.ansi[k] != c.ansi[0]) distintos++;
        COMPROBAR(distintos >= 12, "tema %s: paleta con colores distintos (%d)", tl_temas[i].id, distintos);
        /* la letra se distingue del fondo */
        int d = (int)tl_luma(c.fundo) - (int)tl_luma(c.letra);
        COMPROBAR(d > 80 || d < -80, "tema %s: contraste entre fondo y letra (%d)", tl_temas[i].id, d);
    }
    tl_conf_tema(&c, 2);   /* verde: todo en tonos de verde */
    int ok = 1;
    for (int k = 0; k < 16; k++) { uint32_t v = c.ansi[k]; if (((v >> 16) & 255) > ((v >> 8) & 255)) ok = 0; }
    COMPROBAR(ok, "verde fosforo: la paleta ANSI es de una sola tinta");
}

static void prueba_conf(void)
{
    tl_conf_t c, d;
    tl_conf_padrao(&c);
    tl_conf_ler(&c, "# nada\n");
    COMPROBAR(c.tema == 0 && c.hist == 500, "archivo sin claves: el defecto");
    tl_conf_ler(&c, "tema=ambar\nfuente=/apps/fuentes/x.otf\ntamano=18\ncolumnas=100\nfilas=30\nhistorial=2000\n"
                    "cursor=barra\nparpadeo=si\ncampana=sonora\nbarra=siempre\n");
    COMPROBAR(c.tema == 3 && c.fundo == 0x120A00 && !strcmp(c.fonte, "/apps/fuentes/x.otf") && c.fpx == 18 &&
              c.cols == 100 && c.lins == 30 && c.hist == 2000 && c.cursor == TL_CUR_BARRA && c.parpadeo == 1 &&
              c.campana == TL_CAMP_SOM && c.barra_siempre == 1, "lectura de todas las claves");
    char b[1024];
    int n = tl_conf_escrever(&c, b, sizeof b);
    COMPROBAR(n > 50 && !strstr(b, "ansi0="), "con un tema fijo no se escriben los colores");
    tl_conf_padrao(&d);
    tl_conf_ler(&d, b);
    COMPROBAR(d.tema == 3 && d.fpx == 18 && d.hist == 2000 && d.cursor == TL_CUR_BARRA && d.barra_siempre == 1 &&
              !strcmp(d.fonte, c.fonte), "escribir y leer da lo mismo");
    /* personalizado */
    tl_conf_tema(&c, 0);
    c.tema = TL_PERSONALIZADO; c.fundo = 0x102030; c.ansi[5] = 0xABCDEF; c.selecao = 0x445566;
    n = tl_conf_escrever(&c, b, sizeof b);
    COMPROBAR(strstr(b, "tema=personalizado") && strstr(b, "ansi5=ABCDEF") && strstr(b, "fondo=102030"), "personalizado escribe colores");
    tl_conf_padrao(&d);
    tl_conf_ler(&d, b);
    COMPROBAR(d.tema == TL_PERSONALIZADO && d.fundo == 0x102030 && d.ansi[5] == 0xABCDEF && d.selecao == 0x445566 &&
              d.ansi[1] == c.ansi[1], "personalizado: se recuperan los colores");
    /* un tema fijo ignora colores sueltos */
    tl_conf_padrao(&d);
    tl_conf_ler(&d, "tema=claro\nfondo=000000\n");
    COMPROBAR(d.fundo == 0xF8F8F4, "tema fijo ignora fondo=");
    /* limites */
    tl_conf_ler(&d, "columnas=5\nfilas=999\nhistorial=999999\ntamano=2\n");
    COMPROBAR(d.cols == TL_COLS_MIN && d.lins == TL_LINS_MAX && d.hist == TL_HIST_MAX && d.fpx == TL_PX_MIN, "limites");
    tl_conf_ler(&d, "columnas=abc\ncursor=raro\ncampana=\n");
    COMPROBAR(d.cols == TL_COLS_MIN && d.cursor == TL_CUR_BLOCO, "valores raros no rompen");
    char pequeno[20];
    n = tl_conf_escrever(&c, pequeno, sizeof pequeno);
    COMPROBAR(n < (int)sizeof pequeno, "escribir en un buffer pequeno no desborda");
}

static void prueba_busca(void)
{
    const char *l = "Hola Mundo, hola SaguiOS";
    COMPROBAR(tl_achar(l, (int)strlen(l), "hola", 4, 0) == 0, "busca sin distinguir mayusculas");
    COMPROBAR(tl_achar(l, (int)strlen(l), "hola", 4, 1) == 12, "segunda aparicion");
    COMPROBAR(tl_achar(l, (int)strlen(l), "xyz", 3, 0) == -1, "no esta");
    COMPROBAR(tl_achar(l, (int)strlen(l), "", 0, 0) == -1, "aguja vacia");
    COMPROBAR(tl_achar_atras(l, (int)strlen(l), "hola", 4, 20) == 12, "hacia atras desde 20");
    COMPROBAR(tl_achar_atras(l, (int)strlen(l), "hola", 4, 12) == 0, "hacia atras desde 12");
    COMPROBAR(tl_achar_atras(l, (int)strlen(l), "hola", 4, 0) == -1, "hacia atras desde 0");
    COMPROBAR(tl_achar("A\xC1" "B", 3, "\xE1", 1, 0) == 1, "Latin-1 con acentos");
}

static void prueba_enlaces(void)
{
    const char *l = "ver http://ejemplo.org/a?b=1, y https://x.es/p.";
    char u[100];
    COMPROBAR(tl_url_en(l, (int)strlen(l), 10, u, sizeof u) > 0 && !strcmp(u, "http://ejemplo.org/a?b=1"), "url 1: '%s'", u);
    COMPROBAR(tl_url_en(l, (int)strlen(l), 4, u, sizeof u) > 0 && !strcmp(u, "http://ejemplo.org/a?b=1"), "inicio de la url");
    COMPROBAR(tl_url_en(l, (int)strlen(l), 36, u, sizeof u) > 0 && !strcmp(u, "https://x.es/p"), "url 2 sin el punto final: '%s'", u);
    COMPROBAR(tl_url_en(l, (int)strlen(l), 1, u, sizeof u) == 0, "una palabra normal no es enlace");
    COMPROBAR(tl_url_en(l, (int)strlen(l), 3, u, sizeof u) == 0, "un espacio no es enlace");
    COMPROBAR(tl_url_en("(http://a.b/c)", 14, 5, u, sizeof u) > 0 && !strcmp(u, "http://a.b/c"), "entre parentesis: '%s'", u);
    COMPROBAR(tl_url_en("http://", 7, 2, u, sizeof u) == 0, "http:// vacio no cuenta");
    COMPROBAR(tl_url_en("ftp://a.b/c", 11, 4, u, sizeof u) == 0, "solo http y https");
}

static void prueba_titulos(void)
{
    char t[32];
    COMPROBAR(tl_titulo_do_prompt("sagui:/home/ale$ ls", 19, t, sizeof t) && !strcmp(t, "ale"), "ultimo componente: '%s'", t);
    COMPROBAR(tl_titulo_do_prompt("sagui:/$ ", 9, t, sizeof t) && !strcmp(t, "/"), "raiz: '%s'", t);
    COMPROBAR(tl_titulo_do_prompt("sagui:/bin$", 11, t, sizeof t) && !strcmp(t, "bin"), "sin nada tras el prompt");
    COMPROBAR(tl_titulo_do_prompt("sagui:/tmp/$ x", 14, t, sizeof t) && !strcmp(t, "tmp"), "barra final");
    COMPROBAR(!tl_titulo_do_prompt("hola mundo", 10, t, sizeof t), "texto normal no es un prompt");
    COMPROBAR(!tl_titulo_do_prompt("sagui:/x costo $5", 17, t, sizeof t), "espacio en la ruta: no es prompt");
    COMPROBAR(!tl_titulo_do_prompt("", 0, t, sizeof t), "linea vacia");
    COMPROBAR(tl_titulo_do_prompt("sagui:/a_muy_largo_nombre_de_carpeta$ ", 37, t, 8) && strlen(t) == 7, "se corta al maximo");
}

int main(void)
{
    prueba_escapes();
    prueba_osc_y_256();
    prueba_sgr();
    prueba_temas();
    prueba_conf();
    prueba_busca();
    prueba_enlaces();
    prueba_titulos();
    if (fallos) { printf("%d fallos de %d\n", fallos, total); return 1; }
    printf("todo bien (%d comprobaciones)\n", total);
    return 0;
}
