// SPDX-License-Identifier: GPL-3.0-only
/* edicao.c - edición de la línea con el terminal en modo crudo: historial
 * (flechas), completar nombres con Tab, cursor en cualquier posición. La pantalla
 * entiende ESC [ D (cursor a la izquierda), ESC [ C (a la derecha) y ESC [ K (borra
 * hasta el final de la línea); ver kernel/dispositivos/terminal_grafico.c. */
#include "sagsh.h"

#define MAX_LINHA 1024
#define MAX_HIST  200

static char *historico[MAX_HIST];
static int qtd_hist;
const char *prompt_atual = "";

static void mostrar(const char *s) { escrever_texto(SAIDA, s); }
static void prompt(void) { mostrar(prompt_atual); }

void historico_por(const char *linha)
{
    if (!*linha) return;
    if (qtd_hist && compara_texto(historico[qtd_hist - 1], linha) == 0) return;
    if (qtd_hist == MAX_HIST) {
        libera(historico[0]);
        for (int i = 1; i < MAX_HIST; i++) historico[i - 1] = historico[i];
        qtd_hist--;
    }
    historico[qtd_hist++] = sh_dup(linha);
}

/* Borra todo el historial (history -c). */
void historico_limpar(void)
{
    for (int i = 0; i < qtd_hist; i++) libera(historico[i]);
    qtd_hist = 0;
}

/* Lista el historial; con ultimos > 0, solo las últimas N líneas. */
void historico_listar(int ultimos)
{
    for (int i = (ultimos > 0 && ultimos < qtd_hist) ? qtd_hist - ultimos : 0; i < qtd_hist; i++) {
        texto_t t = {0};
        tx_formatar(&t, "%5d  ", i + 1);
        tx_junta(&t, historico[i]);
        tx_car(&t, '\n');
        escrever_saida(SAIDA, t.s, t.n);
        tx_liberar(&t);
    }
}

/* Lee una tecla del terminal en modo crudo: carácter, o 0x100 + letra para
 * las teclas especiales (abi.h). */
static int ler_tecla(void)
{
    uint8_t c;
    if (ler(ENTRADA, &c, 1) <= 0) return -1;
    if (c != 27) return c;
    uint8_t d;
    if (ler(ENTRADA, &d, 1) <= 0) return -1;
    return 0x100 | d;
}

static void cursor_esquerda(int n) { while (n-- > 0) mostrar("\x1b[D"); }
static void cursor_direita(int n)  { while (n-- > 0) mostrar("\x1b[C"); }

/* Reescribe desde la posición "de" hasta el final, borra el resto de la línea antigua
 * y devuelve el cursor a "cur". */
static void reescrever_de(const char *linha, int n, int de, int cur)
{
    escrever(SAIDA, linha + de, (unsigned)(n - de));
    mostrar("\x1b[K");
    cursor_esquerda(n - cur);
}

static void trocar_linha(char *linha, int *n, int *cur, const char *nova)
{
    cursor_esquerda(*cur);
    copia_texto(linha, nova, MAX_LINHA);
    *n = *cur = (int)tam_texto(linha);
    reescrever_de(linha, *n, 0, *cur);
}

static bool separador(char c)
{
    return c == ' ' || c == '|' || c == '>' || c == '<' || c == ';' || c == '&' || c == '(' || c == '`' || c == '=';
}

static void candidato(lista_t *l, const char *nome)
{
    for (int i = 0; i < l->n; i++) if (compara_texto(l->v[i], nome) == 0) return;
    lista_poe(l, sh_dup(nome));
}

/* Junta los candidatos para completar la palabra antes del cursor. */
static void candidatos(const char *linha, int n, lista_t *lista, int *ini_palavra)
{
    int ini = n;
    while (ini > 0 && !separador(linha[ini - 1])) ini--;
    *ini_palavra = ini;
    char *palavra = sh_dup_n(linha + ini, n - ini);
    int k = ini;   /* ¿es la primera palabra del comando? */
    while (k > 0 && linha[k - 1] == ' ') k--;
    bool primeira = k == 0 || acha_char("|;&(`", linha[k - 1]);
    if (primeira && !acha_char(palavra, '/')) {
        for (int i = 0; i < qtd_comandos; i++) {
            if (comeca_com(comandos[i].nome, palavra)) candidato(lista, comandos[i].nome);
            if (comandos[i].apelido && comeca_com(comandos[i].apelido, palavra)) candidato(lista, comandos[i].apelido);
        }
        for (int i = 0; nome_interno(i); i++)
            if (*palavra && nome_interno(i)[0] != '.' && comeca_com(nome_interno(i), palavra)) candidato(lista, nome_interno(i));
        libera(palavra);
        return;
    }
    if (palavra[0] == '$') { libera(palavra); return; }
    /* Nombre de archivo: separa la carpeta del comienzo del nombre. */
    char *barra = acha_ultimo_char(palavra, '/');
    const char *prefixo = palavra;
    char *pasta;
    if (barra) {
        int tp = (int)(barra - palavra);
        pasta = tp == 0 ? sh_dup("/") : sh_dup_n(palavra, tp);
        prefixo = barra + 1;
    } else {
        pasta = sh_dup(".");
    }
    if (pasta[0] == '~') {
        const char *h = var_obter("HOME");
        texto_t t = {0};
        tx_junta(&t, h ? h : "/");
        tx_junta(&t, pasta + 1);
        libera(pasta);
        pasta = tx_texto(&t);
    }
    entrada_pasta_t e;
    for (int i = 0; listar(pasta, i, &e) == 1 && lista->n < 200; i++)
        if (comeca_com(e.nome, prefixo) && (e.nome[0] != '.' || prefixo[0] == '.')) {
            texto_t t = {0};
            tx_junta(&t, e.nome);
            if (e.tipo == TIPO_PASTA) tx_car(&t, '/');
            lista_poe(lista, tx_texto(&t));
        }
    *ini_palavra = ini + (int)(prefixo - palavra);
    libera(pasta);
    libera(palavra);
}

static void completar(char *linha, int *n)
{
    lista_t lista = {0};
    int ini;
    linha[*n] = 0;
    candidatos(linha, *n, &lista, &ini);
    if (lista.n == 0) { lista_liberar(&lista); return; }
    int ja = *n - ini;   /* cuánto del nombre ya está escrito */
    int comum = (int)tam_texto(lista.v[0]);
    for (int i = 1; i < lista.n; i++) {
        int k = 0;
        while (k < comum && lista.v[i][k] == lista.v[0][k]) k++;
        comum = k;
    }
    if (comum > ja) {
        for (int k = ja; k < comum && *n < MAX_LINHA - 2; k++) {
            linha[(*n)++] = lista.v[0][k];
            escrever(SAIDA, &lista.v[0][k], 1);
        }
        if (lista.n == 1 && lista.v[0][comum - 1] != '/' && *n < MAX_LINHA - 2) {
            linha[(*n)++] = ' ';
            mostrar(" ");
        }
        linha[*n] = 0;
        lista_liberar(&lista);
        return;
    }
    if (lista.n > 1) {   /* varias opciones y nada que añadir: muestra las opciones */
        ordenar(lista.v, lista.n);
        mostrar("\n");
        for (int i = 0; i < lista.n; i++)
            imprimir("%-20s%s", lista.v[i], (i % 4 == 3 || i == lista.n - 1) ? "\n" : "");
        prompt();
        linha[*n] = 0;
        mostrar(linha);
    }
    lista_liberar(&lista);
}

/* Búsqueda incremental hacia atrás (Ctrl+R): cada letra afina la búsqueda, otro
 * Ctrl+R salta a una coincidencia más antigua, Intro deja la línea hallada para
 * editarla y Esc o Ctrl+C la descartan. Devuelve el texto elegido o NULL. */
static const char *buscar_historial(void)
{
    char q[64];
    int nq = 0, hallada = -1;
    q[0] = 0;
    for (;;) {
        char etiqueta[160];
        formatar_texto(etiqueta, sizeof etiqueta, "\r\x1b[K(búsqueda)`%s': %s", q, hallada >= 0 ? historico[hallada] : "");
        mostrar(etiqueta);
        int t = ler_tecla();
        if (t < 0 || t == 3 || t == (0x100 | 27) || t == 7) { hallada = -1; break; }
        if (t == '\n' || t == '\r') break;
        bool otra = false;
        if (t == 18) otra = true;                      /* Ctrl+R: la siguiente más antigua */
        else if (t == '\b' || t == 0x7F) { if (nq > 0) q[--nq] = 0; }
        else if (t >= 0x20 && t < 0x100 && nq < 62) { q[nq++] = (char)t; q[nq] = 0; }
        else continue;
        int desde = otra && hallada >= 0 ? hallada - 1 : qtd_hist - 1;
        if (!otra && hallada >= 0) desde = hallada;      /* afinar: la actual puede seguir valiendo */
        int nueva = -1;
        for (int i = desde; i >= 0 && nq > 0; i--) {
            size_t lq = (size_t)nq;
            for (const char *p = historico[i]; *p; p++)
                if (compara_texto_n(p, q, lq) == 0) { nueva = i; break; }
            if (nueva >= 0) break;
        }
        if (nueva >= 0 || nq == 0 || !otra) hallada = nueva;
    }
    mostrar("\r\x1b[K");
    return hallada >= 0 ? historico[hallada] : NULL;
}

/* Lee una línea con edición. Devuelve el tamaño, -1 al final de la entrada o -2
 * si la línea fue descartada con Ctrl+C;
 * *saida recibe una copia (reserva memoria). */
int ler_linha_editada(const char *texto_prompt, char **saida)
{
    static char linha[MAX_LINHA];
    prompt_atual = texto_prompt;
    prompt();
    int n = 0, cur = 0, pos_hist = qtd_hist;
    linha[0] = 0;
    modo_terminal(TERM_BRUTO);
    for (;;) {
        int t = ler_tecla();
        if (t < 0) { modo_terminal(TERM_LINHA); return -1; }
        if (t == '\n' || t == '\r') break;
        if (t == '\b' || t == 0x7F) {                    /* borra antes del cursor */
            if (cur > 0) {
                bool no_fim = cur == n;
                memmove(linha + cur - 1, linha + cur, (size_t)(n - cur + 1));
                n--; cur--;
                if (no_fim) { mostrar("\b"); continue; }   /* caso común: solo borra el último */
                cursor_esquerda(1);
                reescrever_de(linha, n, cur, cur);
            }
        } else if (t == (0x100 | TECLA_B_DELETE)) {      /* borra bajo el cursor */
            if (cur < n) {
                memmove(linha + cur, linha + cur + 1, (size_t)(n - cur));
                n--;
                reescrever_de(linha, n, cur, cur);
            }
        } else if (t == (0x100 | TECLA_B_ESQUERDA)) {
            if (cur > 0) { cur--; cursor_esquerda(1); }
        } else if (t == (0x100 | TECLA_B_DIREITA)) {
            if (cur < n) { cur++; cursor_direita(1); }
        } else if (t == (0x100 | TECLA_B_HOME) || t == 1) {
            cursor_esquerda(cur); cur = 0;
        } else if (t == (0x100 | TECLA_B_END) || t == 5) {
            cursor_direita(n - cur); cur = n;
        } else if (t == (0x100 | TECLA_B_REDESENHAR)) {  /* otro programa escribió en la pantalla */
            prompt();
            reescrever_de(linha, n, 0, cur);
        } else if (t == '\t') {
            cursor_direita(n - cur); cur = n;              /* completa siempre al final */
            completar(linha, &n);
            cur = n;
        } else if (t == 3) {                              /* Ctrl+C: descarta la línea */
            cursor_direita(n - cur);
            mostrar("^C\n");
            sh.status = 130;
            modo_terminal(TERM_LINHA);
            return -2;   /* quien llamó abandona también las líneas de continuación (PS2) */
        } else if (t == 21) {                             /* Ctrl+U: borra hasta el comienzo */
            if (cur > 0) {
                memmove(linha, linha + cur, (size_t)(n - cur + 1));
                n -= cur;
                cursor_esquerda(cur);
                cur = 0;
                reescrever_de(linha, n, 0, 0);
            }
        } else if (t == 18) {                             /* Ctrl+R: buscar en el historial */
            const char *h = buscar_historial();
            prompt();
            if (h) { copia_texto(linha, h, sizeof linha); n = cur = (int)tam_texto(linha); }
            else { linha[n] = 0; cur = n; }
            mostrar(linha);
        } else if (t == 11) {                             /* Ctrl+K: borra hasta el final */
            if (cur < n) { linha[cur] = 0; n = cur; mostrar("\x1b[K"); }
        } else if (t == 23) {                             /* Ctrl+W: borra la palabra anterior */
            int ini = cur;
            while (ini > 0 && linha[ini - 1] == ' ') ini--;
            while (ini > 0 && linha[ini - 1] != ' ') ini--;
            if (ini < cur) {
                memmove(linha + ini, linha + cur, (size_t)(n - cur + 1));
                n -= cur - ini;
                cursor_esquerda(cur - ini);
                cur = ini;
                reescrever_de(linha, n, cur, cur);
            }
        } else if (t == 12) {                             /* Ctrl+L: limpia la pantalla */
            mostrar("\x1b[J");
            prompt();
            reescrever_de(linha, n, 0, cur);
        } else if (t == (0x100 | TECLA_B_CIMA)) {
            if (pos_hist > 0) trocar_linha(linha, &n, &cur, historico[--pos_hist]);
        } else if (t == (0x100 | TECLA_B_BAIXO)) {
            if (pos_hist < qtd_hist - 1) trocar_linha(linha, &n, &cur, historico[++pos_hist]);
            else if (pos_hist < qtd_hist) { pos_hist = qtd_hist; trocar_linha(linha, &n, &cur, ""); }
        } else if (t >= 0x20 && t < 0x100 && t != 0x7F && n < MAX_LINHA - 1) {   /* inserta en el cursor */
            memmove(linha + cur + 1, linha + cur, (size_t)(n - cur + 1));
            linha[cur] = (char)t;
            n++;
            if (cur == n - 1) escrever(SAIDA, &linha[cur], 1);   /* caso común: escribiendo al final */
            else reescrever_de(linha, n, cur, cur + 1);
            cur++;
        }
    }
    mostrar("\n");
    modo_terminal(TERM_LINHA);
    linha[n] = 0;
    historico_por(linha);
    *saida = sh_dup(linha);
    return n;
}
