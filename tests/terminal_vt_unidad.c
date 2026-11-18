// SPDX-License-Identifier: GPL-3.0-only
/* terminal_vt_unidad.c - prueba de unidad en el PC del emulador del Terminal (D320, h320).
 *
 *   gcc -O2 -Wall -Wextra -DTERMINAL_ANFITRION -I userland/programas -o terminal_vt_unidad tests/terminal_vt_unidad.c
 *
 *   ./terminal_vt_unidad              expectativas escritas a mano: sale con 0 si pasan todas
 *   ./terminal_vt_unidad --volcar C L [--alt]
 *                                     lee de la entrada estándar el flujo de bytes de una aplicación y escribe
 *                                     la rejilla (una línea por fila, sin espacios finales; las cajas DEC como
 *                                     + - |) y «cursor fila columna» (base 1). tests/h320_terminal_vt.py lo
 *                                     compara con tmux como emulador de referencia. */
#include <stdio.h>
#include <stdlib.h>
#include "terminal_vt.h"

static int fallos, total;
#define COMPROBAR(c, ...) do { total++; if (!(c)) { fallos++; printf("FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static char resp[256];
static int nhist, nsuja;
static void cb_resp(void *u, const char *b, int n) { (void)u; if (strlen(resp) + (size_t)n < sizeof resp) { size_t k = strlen(resp); memcpy(resp + k, b, (size_t)n); resp[k + (size_t)n] = 0; } }
static void cb_hist(void *u, int l) { (void)u; (void)l; nhist++; }
static void cb_suja(void *u, int a, int b) { (void)u; (void)a; (void)b; nsuja++; }

static void nuevo(vt_t *v, int c, int l)
{
    memset(v, 0, sizeof *v);
    v->g = malloc((size_t)VT_STRIDE * TL_LINS_MAX);
    v->a = calloc(1, (size_t)VT_STRIDE * TL_LINS_MAX * 2);
    memset(v->g, ' ', (size_t)VT_STRIDE * TL_LINS_MAX);
    v->cols = c; v->lins = l;
    v->resp = cb_resp; v->hist = cb_hist; v->suja = cb_suja;
    vt_iniciar(v);
    resp[0] = 0; nhist = 0;
}

static void tx(vt_t *v, const char *s) { for (; *s; s++) vt_byte(v, (unsigned char)*s); }

static char linea[VT_STRIDE + 1];
static const char *fila(vt_t *v, int l)   /* la fila l (base 0) sin espacios finales */
{
    int n = v->cols;
    for (int c = 0; c < n; c++) linea[c] = (v->a[l * VT_STRIDE + c] & TL_AT_DEC) ? (strchr("jklmntuvw", v->g[l * VT_STRIDE + c]) ? '+' : v->g[l * VT_STRIDE + c] == 'q' ? '-' : v->g[l * VT_STRIDE + c] == 'x' ? '|' : '#') : v->g[l * VT_STRIDE + c];
    while (n > 0 && linea[n - 1] == ' ') n--;
    linea[n] = 0;
    return linea;
}
static int ig(vt_t *v, int l, const char *esp) { return !strcmp(fila(v, l), esp); }

static void p_cursor(void)
{
    vt_t v; nuevo(&v, 20, 6);
    tx(&v, "\033[3;5Hx\033[2Aa\033[3Bb\033[2Dc\033[Cd");
    COMPROBAR(ig(&v, 2, "    x") && v.lin == 3, "CUP/CUU/CUD");
    nuevo(&v, 20, 6);
    tx(&v, "\033[2;3H\0337\033[5;9H\0338!");
    COMPROBAR(v.lin == 1 && v.col == 3 && ig(&v, 1, "  !"), "DECSC/DECRC: %d,%d", v.lin, v.col);
    tx(&v, "\033[4;4H\033[s\033[1;1H\033[u?");
    COMPROBAR(v.lin == 3 && v.col == 4, "CSI s / CSI u");
    nuevo(&v, 20, 6);
    tx(&v, "\033[3;3H\033[2E\033[2Fz\033[8Gy\033[4d");
    COMPROBAR(v.lin == 3, "CNL/CPL/VPA: %d", v.lin);
    nuevo(&v, 20, 6);
    tx(&v, "ab\033[20Gc\033[1Gd");
    COMPROBAR(ig(&v, 0, "db                 c"), "CHA: '%s'", fila(&v, 0));
}

static void p_borrado(void)
{
    vt_t v; nuevo(&v, 10, 4);
    tx(&v, "aaaaaaaaaa\r\nbbbbbbbbbb\r\ncccccccccc\r\ndddddddddd\033[2;4H\033[J");
    COMPROBAR(ig(&v, 0, "aaaaaaaaaa") && ig(&v, 1, "bbb") && ig(&v, 2, "") && ig(&v, 3, ""), "ED 0");
    tx(&v, "\033[1;1Haaaaaaaaaa\r\nbbbbbbbbbb\033[2;4H\033[1J");
    COMPROBAR(ig(&v, 0, "") && ig(&v, 1, "    bbbbbb"), "ED 1: '%s'", fila(&v, 1));
    tx(&v, "\033[2;5H\033[K");
    COMPROBAR(ig(&v, 1, ""), "EL 0");
    tx(&v, "\033[1;1H0123456789\033[1;3H\033[1K");
    COMPROBAR(ig(&v, 0, "   3456789"), "EL 1: '%s'", fila(&v, 0));
    tx(&v, "\033[2K\033[1;3H\033[2J");
    COMPROBAR(ig(&v, 0, "") && v.col == 2, "EL 2 y ED 2 (no mueve el cursor)");
    tx(&v, "\033[1;1Habcdefghij\033[1;3H\033[3X");
    COMPROBAR(ig(&v, 0, "ab   fghij"), "ECH: '%s'", fila(&v, 0));
    tx(&v, "\033[1;1H\033[2@");
    COMPROBAR(ig(&v, 0, "  ab   fgh"), "ICH: '%s'", fila(&v, 0));
    tx(&v, "\033[3P");
    COMPROBAR(ig(&v, 0, "b   fgh"), "DCH: '%s'", fila(&v, 0));
    nuevo(&v, 10, 4);
    tx(&v, "a\033[3b");
    COMPROBAR(ig(&v, 0, "aaaa"), "REP: '%s'", fila(&v, 0));
}

static void p_region(void)
{
    vt_t v; nuevo(&v, 10, 6);
    tx(&v, "\033[1;1H1\r\n2\r\n3\r\n4\r\n5\r\n6");
    tx(&v, "\033[2;5r");   /* región 2..5; el cursor va al origen */
    COMPROBAR(v.lin == 0 && v.col == 0 && v.top == 1 && v.bot == 4, "DECSTBM");
    tx(&v, "\033[5;1H\n");   /* LF en el fondo de la región: sube 2..5 */
    COMPROBAR(ig(&v, 0, "1") && ig(&v, 1, "3") && ig(&v, 3, "5") && ig(&v, 4, "") && ig(&v, 5, "6"), "LF en región: %s|%s|%s|%s|%s", fila(&v, 0), fila(&v, 1), fila(&v, 2), fila(&v, 3), fila(&v, 5));
    COMPROBAR(nhist == 0, "una región que no empieza en la línea 1 no va al historial");
    tx(&v, "\033[2;1H\033M");   /* RI en el tope de la región: baja */
    COMPROBAR(ig(&v, 1, "") && ig(&v, 2, "3") && ig(&v, 4, "5"), "RI baja la región");
    tx(&v, "\033[3;1H\033[L");   /* IL */
    COMPROBAR(ig(&v, 2, "") && ig(&v, 3, "3") && ig(&v, 4, "4") && ig(&v, 5, "6"), "IL: %s|%s|%s", fila(&v, 2), fila(&v, 3), fila(&v, 4));
    tx(&v, "\033[M");   /* DL */
    COMPROBAR(ig(&v, 2, "3") && ig(&v, 3, "4") && ig(&v, 4, ""), "DL");
    tx(&v, "\033[S");   /* SU */
    COMPROBAR(ig(&v, 1, "3") && ig(&v, 2, "4"), "SU: %s|%s", fila(&v, 1), fila(&v, 2));
    tx(&v, "\033[T");   /* SD */
    COMPROBAR(ig(&v, 1, "") && ig(&v, 2, "3"), "SD");
    tx(&v, "\033[r\033[6;1H\n");
    COMPROBAR(nhist == 1, "sin región, el LF final va al historial (%d)", nhist);
    nuevo(&v, 10, 6);
    tx(&v, "\033[3;5r\033[?6h\033[1;1Hx");
    COMPROBAR(v.lin == 2 && v.col == 1 && ig(&v, 2, "x"), "DECOM");
    tx(&v, "\033[?6l\033[1;1Hy");
    COMPROBAR(ig(&v, 0, "y"), "DECOM fuera");
}

static void p_wrap(void)
{
    vt_t v; nuevo(&v, 5, 3);
    tx(&v, "abcde");
    COMPROBAR(v.lin == 0 && v.col == 4 && v.pend, "salto pendiente al escribir en la última columna");
    tx(&v, "f");
    COMPROBAR(v.lin == 1 && v.col == 1 && ig(&v, 1, "f"), "...y se hace con la letra siguiente");
    tx(&v, "\033[?7l\033[1;5Hxyz");
    COMPROBAR(ig(&v, 0, "abcdz") && v.lin == 0, "DECAWM apagado: '%s'", fila(&v, 0));
    nuevo(&v, 5, 3);
    tx(&v, "\033[3;5Hx");
    COMPROBAR(v.lin == 2 && nhist == 0, "escribir en la última celda no hace scroll");
}

static void p_alt(void)
{
    vt_t v; nuevo(&v, 10, 4);
    tx(&v, "\033[1;1Hnormal\033[3;3Hcur\033[?1049h");
    COMPROBAR(v.alt && ig(&v, 0, "") , "pantalla alternativa limpia");
    tx(&v, "alterna\033[2;2H*");
    tx(&v, "\033[?1049l");
    COMPROBAR(!v.alt && ig(&v, 0, "normal") && v.lin == 2 && v.col == 5, "vuelve la pantalla y el cursor: %s %d,%d", fila(&v, 0), v.lin, v.col);
    tx(&v, "\033[?1049h");
    COMPROBAR(ig(&v, 1, ""), "la alternativa se limpia al entrar");
    tx(&v, "\033[?1049l\033[?47h");
    COMPROBAR(v.alt, "?47");
    tx(&v, "\033[?47l\033[?1047h\033[?1047l");
    COMPROBAR(!v.alt && ig(&v, 0, "normal"), "?1047");
    tx(&v, "\033[2;2H\033[?1048h\033[4;4H\033[?1048l");
    COMPROBAR(v.lin == 1 && v.col == 1, "?1048");
}

static void p_modos(void)
{
    vt_t v; nuevo(&v, 10, 4);
    tx(&v, "\033[?25l"); COMPROBAR(v.cur_oculto, "?25l");
    tx(&v, "\033[?25h"); COMPROBAR(!v.cur_oculto, "?25h");
    tx(&v, "\033[?1h"); COMPROBAR(v.app_cur, "DECCKM");
    tx(&v, "\033[?1l"); COMPROBAR(!v.app_cur, "DECCKM off");
    tx(&v, "\033="); COMPROBAR(v.app_kp, "DECKPAM");
    tx(&v, "\033>"); COMPROBAR(!v.app_kp, "DECKPNM");
    tx(&v, "\033[?2004h"); COMPROBAR(v.colar_cor, "?2004");
    tx(&v, "\033[?1000h\033[?1006h\033[?1000l\033[?1006l\033[?12l\033[?1004h"); COMPROBAR(ig(&v, 0, ""), "ratón y demás se aceptan sin escribir nada");
    tx(&v, "\033[4h""ab\033[1;1Hxy\033[4l");
    COMPROBAR(ig(&v, 0, "xyab"), "IRM: '%s'", fila(&v, 0));
    COMPROBAR(v.lnm && !v.xterm, "por defecto LF = CR+LF");
    tx(&v, "\033[?7700h\033[2J\033[1;1Ha\nb");
    COMPROBAR(v.xterm && !v.lnm && ig(&v, 1, " b"), "modo xterm: LF no vuelve a la columna 0 (%s)", fila(&v, 1));
    tx(&v, "\b\bz");
    COMPROBAR(ig(&v, 1, "zb"), "modo xterm: BS solo mueve: '%s'", fila(&v, 1));
    tx(&v, "\033[?7700l");
    COMPROBAR(!v.xterm && v.lnm, "modo xterm apagado");
    tx(&v, "\033]0;titulo\007\033P1$r0m\033\\\033_algo\033\\x");
    COMPROBAR(strchr(fila(&v, 1), 'x') != NULL, "OSC, DCS y APC se descartan");
}

static void p_sgr(void)
{
    vt_t v; nuevo(&v, 20, 3);
    tx(&v, "\033[1;4;31;42mA\033[22;24;39;49mB\033[7mC\033[27;2;8mD\033[0mE");
    uint16_t a = v.a[0], b = v.a[1], c = v.a[2], d = v.a[3], e = v.a[4];
    COMPROBAR((a & TL_AT_NEGRITA) && (a & TL_AT_SUBRAYA) && TL_AT_FG(a) == 2 && TL_AT_BG(a) == 3, "negrita, subrayado, rojo sobre verde");
    COMPROBAR(b == 0, "anulaciones: %04X", b);
    COMPROBAR(c == TL_AT_INVERSO, "inverso");
    COMPROBAR((d & TL_AT_TENUE) && (d & TL_AT_OCULTO), "tenue y oculto");
    COMPROBAR(e == 0, "SGR 0");
    nuevo(&v, 20, 3);
    tx(&v, "\033[38;5;196mA\033[48;5;21mB\033[38;2;0;255;0mC\033[38:5:244mD\033[91;103mE\033[5;25mF");
    COMPROBAR(TL_AT_FG(v.a[0]) == 10, "256: rojo 196 -> ANSI rojo vivo (%u)", TL_AT_FG(v.a[0]));
    COMPROBAR(TL_AT_BG(v.a[1]) == 13, "256: azul 21 -> azul vivo (%u)", TL_AT_BG(v.a[1]));
    COMPROBAR(TL_AT_FG(v.a[2]) == 11, "truecolor verde -> verde vivo (%u)", TL_AT_FG(v.a[2]));
    COMPROBAR(TL_AT_FG(v.a[3]) != 0, "38:5:n con dos puntos");
    COMPROBAR(TL_AT_FG(v.a[4]) == 10 && TL_AT_BG(v.a[4]) == 12, "colores vivos 90 y 100");
    COMPROBAR(v.a[5] == v.a[4], "parpadeo ignorado");
    /* bce: borrar usa el fondo actual */
    nuevo(&v, 10, 2);
    tx(&v, "\033[44m\033[K\033[2J");
    COMPROBAR(TL_AT_BG(v.a[0]) == 5 && TL_AT_BG(v.a[VT_STRIDE]) == 5, "bce");
}

static void p_dec(void)
{
    vt_t v; nuevo(&v, 10, 3);
    tx(&v, "\033(0lqqk\r\nx  x\r\nmqqj\033(Bq");
    COMPROBAR(ig(&v, 0, "+--+") && ig(&v, 1, "|  |") && ig(&v, 2, "+--+q"), "cajas DEC: %s|%s|%s", fila(&v, 0), fila(&v, 1), fila(&v, 2));
    nuevo(&v, 10, 3);
    tx(&v, "\033)0\016q\017q");
    COMPROBAR(ig(&v, 0, "-q"), "SO/SI con G1: '%s'", fila(&v, 0));
}

static void p_respuestas(void)
{
    vt_t v; nuevo(&v, 80, 24);
    tx(&v, "\033[5;9H\033[6n");
    COMPROBAR(!strcmp(resp, "\033[5;9R"), "DSR 6");
    resp[0] = 0; tx(&v, "\033[5n");
    COMPROBAR(!strcmp(resp, "\033[0n"), "DSR 5");
    resp[0] = 0; tx(&v, "\033[c");
    COMPROBAR(!strcmp(resp, "\033[?62;22c"), "DA1: %s", resp + 1);
    resp[0] = 0; tx(&v, "\033[>c");
    COMPROBAR(!strncmp(resp, "\033[>", 3), "DA2");
    resp[0] = 0; tx(&v, "\033[18t");
    COMPROBAR(!strcmp(resp, "\033[8;24;80t"), "18t");
    /* D385: modo 2048 (aviso de tamaño): al activarse manda el tamaño; RIS lo apaga */
    resp[0] = 0; tx(&v, "\033[?2048h");
    COMPROBAR(!strcmp(resp, "\033[48;24;80;0;0t") && v.aviso_tam, "2048: aviso al activarse: %s", resp + 1);
    resp[0] = 0; tx(&v, "\033[?2048l");
    COMPROBAR(!resp[0] && !v.aviso_tam, "2048: apagado, sin aviso");
    resp[0] = 0; tx(&v, "\033[2;5r\033[?6h\033[3;3H\033[6n");
    COMPROBAR(!strcmp(resp, "\033[3;3R"), "DSR con modo origen: %s", resp + 1);
}

static void p_redim(void)
{
    vt_t v; nuevo(&v, 10, 6);
    tx(&v, "\033[1;1Ha\r\nb\r\nc\r\nd\r\ne\033[6;1Hf");
    vt_redimensionar(&v, 10, 4);
    COMPROBAR(nhist == 2 && ig(&v, 0, "c") && ig(&v, 3, "f") && v.lin == 3, "redimensionar: %d %s", nhist, fila(&v, 0));
}

int main(int argc, char **argv)
{
    if (argc >= 4 && !strcmp(argv[1], "--volcar")) {
        vt_t v;
        nuevo(&v, atoi(argv[2]), atoi(argv[3]));
        v.lnm = false; v.xterm = true;
        int c;
        while ((c = getchar()) != EOF) vt_byte(&v, (unsigned char)c);
        if (argc > 4 && v.alt) { /* nada: se vuelca la pantalla que esté a la vista */ }
        for (int l = 0; l < v.lins; l++) puts(fila(&v, l));
        printf("cursor %d %d\n", v.lin + 1, v.col + 1);
        return 0;
    }
    p_cursor(); p_borrado(); p_region(); p_wrap(); p_alt(); p_modos(); p_sgr(); p_dec(); p_respuestas(); p_redim();
    if (fallos) { printf("%d de %d comprobaciones fallan\n", fallos, total); return 1; }
    printf("todo bien (%d comprobaciones)\n", total);
    return 0;
}
