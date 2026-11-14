// SPDX-License-Identifier: GPL-2.0-or-later
/* less / more (paginador) - muestra un archivo o la entrada de pantalla en pantalla (D097).
 * Teclas: espacio o f = página siguiente, b = anterior, Intro o j = una línea,
 * k = línea anterior, g/G = principio/final, / = buscar, n = siguiente coincidencia, q = salir.
 * Si la salida no es el terminal, copia el texto sin paginar (como more). */
#include "sagui.h"

static const char *AYUDA =
    "uso: less [-N] [-n LÍNEAS] [-N] [archivo...]     (sin archivo, pagina la entrada)\n"
    "  -n L  alto de la pantalla en líneas (por defecto, la variable LINES o 24)\n"
    "Teclas: espacio/f página siguiente   b página anterior   Intro/j una línea   k línea anterior\n"
    "        g principio   G final   /texto buscar   n siguiente coincidencia   q salir\n";

static char *buf;
static char **lineas;
static int nl, alto = 24, ancho = 80;

static int leer_tecla(void)
{
    char c;
    if (ler(ENTRADA, &c, 1) <= 0) return -1;
    if (c == 27) {
        char d;
        if (ler(ENTRADA, &d, 1) <= 0) return -1;
        if (d == 27) return 'q';
        return 0x100 | (unsigned char)d;
    }
    return (unsigned char)c;
}

static void pintar(int ini, int cuantas)
{
    char s[400];
    for (int i = ini; i < ini + cuantas && i < nl; i++) {
        int n = (int)tam_texto(lineas[i]);
        if (n > ancho) n = ancho;
        memcpy(s, lineas[i], (size_t)n);
        s[n++] = '\n';
        escrever_tudo(SAIDA, s, (unsigned)n);
    }
}

static bool contiene(const char *t, const char *p)
{
    size_t lp = tam_texto(p);
    for (; *t; t++) if (compara_texto_n(t, p, lp) == 0) return true;
    return false;
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    const char *l = obter_env("LINES");
    int r, v;
    if (l && texto_para_numero(l, &v) && v > 3) alto = v;
    o.numerica = true;
    while ((r = obter_opcao(argc, argv, "n:", &o)) >= 0) {
        if (r == 'n' || r == '#') {
            if (!texto_para_numero(o.valor, &v) || v < 3) { imprimir_em(ERROS, "less: %s: alto no válido\n", o.valor); return 2; }
            alto = v;
        } else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("less", r, &o);
    }
    /* lee todo el texto (varios archivos seguidos) */
    unsigned total = 0;
    int nargs = argc - o.indice;
    char *todo = aloca(1);
    if (!todo) return 1;
    todo[0] = 0;
    for (int i = 0; i < (nargs ? nargs : 1); i++) {
        int fd = nargs ? abrir_entrada("less", argv[o.indice + i]) : ENTRADA;
        if (fd < 0) continue;
        unsigned m;
        char *t = ler_todo_fd(fd, &m);
        fechar_entrada(fd);
        if (!t) { imprimir_em(ERROS, "less: sin memoria\n"); return 1; }
        m = (unsigned)utf8_para_latin1(t, m, NULL);
        char *nb = realoca(todo, total + m + 2);
        if (!nb) { imprimir_em(ERROS, "less: sin memoria\n"); return 1; }
        todo = nb;
        memcpy(todo + total, t, m);
        total += m;
        todo[total] = 0;
        libera(t);
    }
    buf = todo;
    if (!eh_terminal(SAIDA)) { escrever_tudo(SAIDA, buf, total); return 0; }
    nl = 0;
    for (unsigned i = 0; i < total; i++) if (buf[i] == '\n') nl++;
    if (total && buf[total - 1] != '\n') nl++;
    lineas = aloca((size_t)(nl + 1) * sizeof *lineas);
    if (!lineas) return 1;
    int k = 0;
    char *ini = buf;
    for (unsigned i = 0; i <= total && k < nl; i++)
        if (buf[i] == '\n' || buf[i] == 0) { buf[i] = 0; lineas[k++] = ini; ini = buf + i + 1; }
    /* sin teclado propio (entrada era un pipe): usa la terminal de la salida de errores */
    if (tipo_descritor(ENTRADA) != TIPO_TERMINAL) duplicar(ERROS, ENTRADA);
    int pagina = alto - 1;
    if (nl <= pagina) { pintar(0, nl); return 0; }
    modo_terminal(TERM_BRUTO);
    int top = 0;
    char busca[64] = "";
    pintar(0, pagina);
    for (;;) {
        char msg[120];
        int pct = (top + pagina >= nl) ? 100 : (top + pagina) * 100 / nl;
        formatar_texto(msg, sizeof msg, "\r-- %d%% (q salir, espacio sigue) --", pct);
        escrever_texto(SAIDA, msg);
        int t = leer_tecla();
        escrever_texto(SAIDA, "\r\x1b[K");
        if (t < 0 || t == 'q' || t == 'Q') break;
        int nuevo = top;
        bool buscar_ahora = false;
        if (t == ' ' || t == 'f' || t == (0x100 | TECLA_B_PGDN)) {
            if (top + pagina >= nl) break;   /* ya se vio el final: como more, sale */
            nuevo = top + pagina;
        } else if (t == '\n' || t == 'j' || t == (0x100 | TECLA_B_BAIXO)) {
            if (top + pagina >= nl) break;
            nuevo = top + 1;
        } else if (t == 'k' || t == (0x100 | TECLA_B_CIMA)) nuevo = top > 0 ? top - 1 : 0;
        else if (t == 'b' || t == (0x100 | TECLA_B_PGUP)) nuevo = top > pagina ? top - pagina : 0;
        else if (t == 'g' || t == (0x100 | TECLA_B_HOME)) nuevo = 0;
        else if (t == 'G' || t == (0x100 | TECLA_B_END)) nuevo = nl > pagina ? nl - pagina : 0;
        else if (t == '/') {
            escrever_texto(SAIDA, "/");
            modo_terminal(TERM_LINHA);
            char b[64];
            if (ler_linha(ENTRADA, b, sizeof b) > 0) copia_texto(busca, b, sizeof busca);
            modo_terminal(TERM_BRUTO);
            buscar_ahora = true;
        } else if (t == 'n') buscar_ahora = true;
        if (buscar_ahora && busca[0]) {
            for (int i = top + 1; i < nl; i++)
                if (contiene(lineas[i], busca)) { nuevo = i; break; }
        }
        if (nuevo + pagina > nl && nl > pagina) nuevo = nl - pagina;
        if (nuevo == top && !buscar_ahora) continue;
        if (nuevo > top && nuevo - top < pagina) pintar(top + pagina, nuevo - top);   /* avance corto: solo las líneas nuevas */
        else pintar(nuevo, pagina);
        top = nuevo;
    }
    modo_terminal(TERM_LINHA);
    return 0;
}
