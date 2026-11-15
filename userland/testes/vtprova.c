// SPDX-License-Identifier: GPL-3.0-only
/* vtprova.c - emite secuencias en el Terminal de ventana para probar su emulador (D320, h320).
 *
 *   vtprova regiao   región de desplazamiento (DECSTBM), IL y DL
 *   vtprova alt      pantalla alternativa (?1049) y vuelta a la de antes
 *   vtprova cores    colores 8/16/256/truecolor, atributos y cajas DEC
 *   vtprova dsr      pregunta CSI 6n, CSI 5n, CSI c y CSI > c y escribe lo que contesta el Terminal
 *   vtprova teclas   modo xterm (CSI ? 7700 h) y escribe en hexadecimal los bytes de cada tecla pulsada
 * Cada paso avisa por la serial con «vtprova: ...» (registrar). */
#include "sagui.h"

static void s(const char *t) { escrever_tudo(SAIDA, t, (unsigned)tam_texto(t)); }

static void linhas(int n)
{
    char t[40];
    for (int i = 1; i <= n; i++) {
        formatar_texto(t, sizeof t, "\033[%d;1HL%02d texto de la linea", i, i);
        s(t);
    }
}

static void regiao(void)
{
    s("\033[2J");
    linhas(28);
    s("\033[5;20r\033[20;1H\r\nX1\r\nX2\r\nX3");
    registrar("vtprova: paso 1 (tras tres LF en el fondo de la region 5..20)");
    dormir(4000);
    s("\033[6;1H\033[2L\033[10;1H\033[1M");
    registrar("vtprova: paso 2 (tras IL 2 en la fila 6 y DL 1 en la 10)");
    dormir(4000);
    s("\033[r\033[28;1H\r\n");
}

static void alt(void)
{
    s("\033[2J\033[3;1HANTES\033[?1049h\033[2J\033[1;1HALTERNA");
    registrar("vtprova: paso 1 (pantalla alternativa)");
    dormir(4000);
    s("\033[?1049l VOLVI");
    registrar("vtprova: paso 2 (de vuelta)");
    dormir(4000);
    s("\r\n");
}

static void cores(void)
{
    s("\033[2J\033[1;1H");
    s("\033[91mROJO256\033[0m \033[38;5;196mR196\033[0m \033[38;5;46mV46\033[0m \033[48;5;21m   \033[0m \033[38;2;0;255;0mTRUE\033[0m\r\n");
    s("\033[1mNEGRITA\033[0m \033[2mTENUE\033[0m \033[4mSUBRAYADO\033[0m \033[7mINVERSO\033[0m \033[8mOCULTO\033[0m|fin\r\n");
    s("\033[3;1H\033(0lqqqqk\r\nx    x\r\ntqqqqu\r\nx    x\r\nmqqqqj\033(B\r\n");
    s("\033[44m\033[K\033[0m\033[10;1Hfin");
    registrar("vtprova: colores listos");
    dormir(5000);
    s("\033[12;1H");
}

static void mostrar_resp(const char *ent, const unsigned char *b, int n)
{
    char t[160];
    int k = 0;
    for (int i = 0; i < n && k < 150; i++) {
        if (b[i] == 27) { t[k++] = '^'; t[k++] = '['; }
        else if (b[i] >= 32 && b[i] < 127) t[k++] = (char)b[i];
        else k += formatar_texto(t + k, (unsigned)(sizeof t - (unsigned)k), "<%02X>", b[i]);
    }
    t[k] = 0;
    registrar("vtprova: %s -> %s", ent, t);
}

static int esperar_resp(unsigned char *b, int max)
{
    unsigned fim = tempo_ms() + 1500;
    int n = 0;
    while ((int)(fim - tempo_ms()) > 0 && n < max) {
        int d = disponivel(ENTRADA);
        if (d > 0) {
            int r = ler(ENTRADA, b + n, (unsigned)(max - n));
            if (r > 0) n += r;
            if (n && b[n - 1] >= 0x40 && b[n - 1] != '[' && b[n - 1] != ';') break;
        } else dormir(10);
    }
    return n;
}

static void dsr(void)
{
    unsigned char b[64];
    modo_terminal(TERM_BRUTO);
    s("\033[5;9H\033[6n");
    mostrar_resp("CSI 6n en 5;9", b, esperar_resp(b, sizeof b));
    s("\033[5n");
    mostrar_resp("CSI 5n", b, esperar_resp(b, sizeof b));
    s("\033[c");
    mostrar_resp("CSI c", b, esperar_resp(b, sizeof b));
    s("\033[>c");
    mostrar_resp("CSI > c", b, esperar_resp(b, sizeof b));
    s("\033[18t");
    mostrar_resp("CSI 18 t", b, esperar_resp(b, sizeof b));
    modo_terminal(TERM_LINHA);
    registrar("vtprova: dsr listo");
}

static void teclas(void)
{
    unsigned char b[32];
    modo_terminal(TERM_BRUTO);
    s("\033[?7700h");
    registrar("vtprova: modo xterm listo");
    unsigned fim = tempo_ms() + 25000;
    int total = 0;
    while ((int)(fim - tempo_ms()) > 0 && total < 40) {
        if (disponivel(ENTRADA) > 0) {
            int r = ler(ENTRADA, b, sizeof b);
            if (r > 0) { mostrar_resp("tecla", b, r); total++; if (b[0] == 'q') break; }
        } else dormir(10);
    }
    s("\033[?7700l");
    modo_terminal(TERM_LINHA);
}

int principal(int argc, char **argv)
{
    const char *m = argc > 1 ? argv[1] : "";
    if (!compara_texto(m, "regiao")) regiao();
    else if (!compara_texto(m, "alt")) alt();
    else if (!compara_texto(m, "cores")) cores();
    else if (!compara_texto(m, "dsr")) dsr();
    else if (!compara_texto(m, "teclas")) teclas();
    else { imprimir("uso: vtprova regiao|alt|cores|dsr|teclas\n"); return 1; }
    return 0;
}
