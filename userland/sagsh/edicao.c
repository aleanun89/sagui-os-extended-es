// SPDX-License-Identifier: GPL-3.0-only
/* edicao.c - edición de la línea con el terminal en modo crudo: historial
 * (flechas), completar nombres con Tab, cursor en cualquier posición. La pantalla
 * entiende ESC [ D (cursor a la izquierda), ESC [ C (a la derecha) y ESC [ K (borra
 * hasta el final de la línea); ver kernel/dispositivos/terminal_grafico.c. */
#include "sagsh.h"
#include "contas.h"

#define MAX_LINHA 1024
#define MAX_HIST  200

static char *historico[MAX_HIST];
static int qtd_hist;
const char *prompt_atual = "";

static void mostrar(const char *s) { escrever_texto(SAIDA, s); }
/* D385: dónde está el cursor de la pantalla, en posiciones de la línea que se edita (0 = justo tras el prompt). Con
 * el ancho del terminal (larg_ed, de COLUMNS) y el del prompt (col_prompt), cada posición es una fila y una columna:
 * así el cursor se mueve bien también en una línea más larga que la ventana, que se parte en varias filas (con
 * ESC [ D solo, no subía de fila y el cursor de la pantalla se quedaba en la columna 0). Sin el ancho (la consola de
 * texto del kernel, que solo entiende ESC [ D C K J), como siempre: de una en una con ESC [ D / C. */
static int larg_ed, col_prompt, pc;
static void prompt(void) { mostrar(prompt_atual); pc = 0; }

/* D371: el de root, en su carpeta (/root); el de una cuenta, en la suya (que ve como /home). */
#define ARQ_HIST (meu_uid() == 0 ? "/root/.sagsh_historial" : "/home/.sagsh_historial")
static bool hist_persistente;

/* D263: historial persistente. Solo con TERM (Terminal y SSH): en la consola y en las pruebas por
 * el serie no se toca el disco. Las últimas MAX_HIST líneas del archivo. */
void historico_carregar(void)
{
    if (!sh_tem_term()) return;
    int fd = abrir(ARQ_HIST, ABRIR_LER);
    hist_persistente = true;
    if (fd < 0) return;
    unsigned tam = 0;
    char *b = ler_todo_fd(fd, &tam);
    fechar(fd);
    if (!b) return;
    for (char *p = b; *p; ) {
        char *nl = acha_char(p, '\n');
        if (nl) *nl = 0;
        if (*p && qtd_hist < MAX_HIST) historico[qtd_hist++] = sh_dup(p);
        else if (*p) { libera(historico[0]); for (int i = 1; i < MAX_HIST; i++) historico[i - 1] = historico[i]; historico[MAX_HIST - 1] = sh_dup(p); }
        if (!nl) break;
        p = nl + 1;
    }
    libera(b);
}

static void historico_guardar(const char *linha)
{
    int fd = abrir(ARQ_HIST, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR);
    if (fd < 0) { hist_persistente = false; return; }
    escrever(fd, linha, (unsigned)tam_texto(linha));
    escrever(fd, "\n", 1);
    fechar(fd);
}

void historico_por(const char *linha)
{
    if (!*linha) return;
    if (qtd_hist && compara_texto(historico[qtd_hist - 1], linha) == 0) return;
    if (hist_persistente) historico_guardar(linha);
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
    if (hist_persistente) { int fd = abrir(ARQ_HIST, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR); if (fd >= 0) fechar(fd); }
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

/* D385: bytes leídos de más mientras se esperaba la respuesta del terminal a ESC [ 18 t (lo que se
 * escribió antes de que llegara): se entregan antes que el resto de la entrada. */
static uint8_t pend[64];
static int npend, ipend;

static void por_pendente(const uint8_t *b, int n)
{
    if (ipend == npend) ipend = npend = 0;
    for (int i = 0; i < n && npend < (int)sizeof pend; i++) pend[npend++] = b[i];
}

static int ler_byte(uint8_t *c)
{
    if (ipend < npend) { *c = pend[ipend++]; return 1; }
    return ler(ENTRADA, c, 1);
}

/* D385: el tamaño del terminal en COLUMNS y LINES (como bash con checkwinsize), para que ls, ayuda y
 * los programas maqueten al ancho de la ventana. No hay llamada al sistema para eso: se pregunta con
 * ESC [ 18 t y el terminal contesta ESC [ 8 ; filas ; columnas t (el Terminal de SaguiOS, D317, y los
 * xterm). Se espera poco (300 ms; 500 mientras no ha contestado nunca) y, tras tres veces sin respuesta, no se
 * vuelve a preguntar (un cliente SSH que no lo entiende). Una respuesta que llegue tarde la recoge ler_tecla.
 * Se hace en modo crudo, para que la respuesta no se vea como eco. */
static int estado_tam;   /* 0 sin respuesta todavía, 1 contesta, -1 no contesta (se dejó de preguntar) */
static int falhas_tam;

/* «8;filas;columnas» (respuesta a ESC [ 18 t) o «48;filas;columnas;...» (aviso del modo 2048): COLUMNS y LINES. */
static void tamanho_de_csi(const char *p)
{
    int v[3] = { 0, 0, 0 }, k = 0;
    for (; *p && k < 3; p++) {
        if (*p >= '0' && *p <= '9') v[k] = v[k] * 10 + (*p - '0');
        else if (*p == ';') k++;
        else return;
    }
    if ((v[0] != 8 && v[0] != 48) || v[1] <= 0 || v[2] <= 0) return;
    char b[16];
    formatar_texto(b, sizeof b, "%d", v[2]);
    var_definir("COLUMNS", b, V_EXPORTADA);
    formatar_texto(b, sizeof b, "%d", v[1]);
    var_definir("LINES", b, V_EXPORTADA);
}

static void consultar_tamanho(void)
{
    if (estado_tam < 0 || !sh_tem_term() || !eh_terminal(SAIDA) || !eh_terminal(ENTRADA)) return;
    escrever_texto(SAIDA, "\x1b[18t");
    unsigned t0 = tempo_ms(), lim = estado_tam > 0 ? 300 : 500;
    uint8_t r[24];
    int n = 0;
    while (tempo_ms() - t0 < lim) {
        if (disponivel(ENTRADA) <= 0) { dormir(5); continue; }
        uint8_t c;
        if (ler(ENTRADA, &c, 1) <= 0) break;
        if (n == 0 && c != 27) { por_pendente(&c, 1); continue; }
        r[n++] = c;
        bool ok = (n == 1) || (n == 2 && c == '[') || (n == 3 && c == '8') || (n == 4 && c == ';') ||
                  (n > 4 && n < (int)sizeof r && ((c >= '0' && c <= '9') || c == ';' || c == 't'));
        if (!ok) { por_pendente(r, n); n = 0; continue; }
        if (c != 't') continue;
        r[n - 1] = 0;
        tamanho_de_csi((const char *)r + 2);
        estado_tam = 1;
        return;
    }
    if (n > 0) por_pendente(r, n);
    if (estado_tam == 0 && ++falhas_tam >= 3) estado_tam = -1;
}

/* D385: mientras se edita la línea, el Terminal avisa si cambia el tamaño (modo 2048): así COLUMNS vale ya para
 * la orden que se está escribiendo. Fuera del prompt se apaga, para no mandarle el aviso a otro programa. */
static void aviso_tamanho(bool on)
{
    if (!sh_tem_term() || !eh_terminal(SAIDA)) return;   /* un terminal que no lo conozca ignora el modo */
    escrever_texto(SAIDA, on ? "\x1b[?2048h" : "\x1b[?2048l");
}

/* Lee una tecla del terminal en modo crudo: carácter, o 0x100 + letra para
 * las teclas especiales (abi.h). D385: también entiende las secuencias CSI de un xterm (por SSH: flechas
 * ESC [ A, Supr ESC [ 3 ~...) y se come el aviso de tamaño del Terminal (modo 2048, CSI 48 ; ... t), que
 * pone COLUMNS y LINES; el Terminal de SaguiOS en su modo de siempre manda ESC + letra, que no choca. */
static int ler_tecla(void)
{
    for (;;) {
        uint8_t c;
        if (ler_byte(&c) <= 0) return -1;
        if (c != 27) return c;
        uint8_t d;
        if (ler_byte(&d) <= 0) return -1;
        if (d != '[' && d != 'O') return 0x100 | d;
        char p[24];
        int n = 0;
        uint8_t f;
        for (;;) {
            if (ler_byte(&f) <= 0) return -1;
            if (f >= 0x40 && f <= 0x7E) break;
            if (n < (int)sizeof p - 1) p[n++] = (char)f;
        }
        p[n] = 0;
        if (d == '[' && f == 't') { tamanho_de_csi(p); if (estado_tam == 0) estado_tam = 1; continue; }
        if (f == 'A' || f == 'B' || f == 'C' || f == 'D' || f == 'H' || f == 'F') return 0x100 | f;
        if (f == '~') {
            int k = 0;
            for (int i = 0; p[i] >= '0' && p[i] <= '9'; i++) k = k * 10 + (p[i] - '0');
            if (k == 3) return 0x100 | TECLA_B_DELETE;
            if (k == 5) return 0x100 | TECLA_B_PGUP;
            if (k == 6) return 0x100 | TECLA_B_PGDN;
            if (k == 1 || k == 7) return 0x100 | TECLA_B_HOME;
            if (k == 4 || k == 8) return 0x100 | TECLA_B_END;
        }
        /* otra secuencia (F1..F12, pegado...): se ignora */
    }
}

/* Columnas que ocupa el prompt en su última línea: sin las secuencias de escape (colores, título) ni \001/\002. */
static int largura_visivel(const char *s)
{
    int w = 0;
    const unsigned char *p = (const unsigned char *)s;
    while (*p) {
        unsigned char c = *p++;
        if (c == '\n' || c == '\r') { w = 0; continue; }
        if (c == 27) {
            if (*p == '[') { p++; while (*p && !(*p >= 0x40 && *p <= 0x7E)) p++; if (*p) p++; }
            else if (*p == ']') { while (*p && *p != 7 && !(*p == 27 && p[1] == '\\')) p++; if (*p == 27) p++; if (*p) p++; }
            else if (*p) p++;
            continue;
        }
        if (c == '\b') { if (w) w--; continue; }
        if (c == '\t') { w = (w + 8) & ~7; continue; }
        if (c < 0x20 || c == 0x7F) continue;
        w++;
    }
    return w;
}

/* Lleva el cursor de la pantalla de la posición pc a la b. */
static void ir_para(int b)
{
    if (b == pc) return;
    if (!larg_ed) {
        while (pc > b) { mostrar("\x1b[D"); pc--; }
        while (pc < b) { mostrar("\x1b[C"); pc++; }
        return;
    }
    int ra = (col_prompt + pc) / larg_ed, rb = (col_prompt + b) / larg_ed, cb = (col_prompt + b) % larg_ed;
    char t[32];
    if (rb < ra) { formatar_texto(t, sizeof t, "\x1b[%dA", ra - rb); mostrar(t); }
    else if (rb > ra) { formatar_texto(t, sizeof t, "\x1b[%dB", rb - ra); mostrar(t); }
    formatar_texto(t, sizeof t, "\x1b[%dG", cb + 1);
    mostrar(t);
    pc = b;
}

static void cursor_esquerda(int n) { ir_para(pc - n); }
static void cursor_direita(int n)  { ir_para(pc + n); }

/* Escribe linha[de..ate) con el cursor en "de". Si acaba justo en la última columna, el terminal deja el salto de
 * línea pendiente (el cursor sigue en esa columna): un espacio y CR lo bajan al principio de la fila siguiente, que es
 * donde lo cuenta ir_para (y lo que se escriba o borre después cae en su sitio). */
static void escrever_linha(const char *linha, int de, int ate)
{
    if (ate > de) escrever(SAIDA, linha + de, (unsigned)(ate - de));
    pc = ate;
    if (larg_ed && ate > de && (col_prompt + ate) % larg_ed == 0) mostrar(" \r");
}

/* Borra lo que quede de la línea antigua: con varias filas, hasta el final de la pantalla. */
static void limpar_resto(void) { mostrar(larg_ed ? "\x1b[J" : "\x1b[K"); }

/* Reescribe desde la posición "de" (donde está el cursor) hasta el final, borra el resto de la línea antigua
 * y devuelve el cursor a "cur". */
static void reescrever_de(const char *linha, int n, int de, int cur)
{
    escrever_linha(linha, de, n);
    limpar_resto();
    ir_para(cur);
}

static void trocar_linha(char *linha, int *n, int *cur, const char *nova)
{
    ir_para(0);
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
            escrever_linha(linha, *n - 1, *n);
        }
        if (lista.n == 1 && lista.v[0][comum - 1] != '/' && *n < MAX_LINHA - 2) {
            linha[(*n)++] = ' ';
            escrever_linha(linha, *n - 1, *n);
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
        escrever_linha(linha, 0, *n);
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
    modo_terminal(TERM_BRUTO);
    consultar_tamanho();
    aviso_tamanho(true);
    larg_ed = 0;
    if (estado_tam > 0 && sh_tem_term() && eh_terminal(SAIDA)) {   /* D385: el ancho, solo si el terminal lo dijo */
        const char *c = var_obter("COLUMNS");
        int w = c ? para_int(c) : 0;
        if (w >= 10 && w <= 1000) larg_ed = w;
    }
    col_prompt = larg_ed ? largura_visivel(prompt_atual) % larg_ed : 0;
    prompt();
    int n = 0, cur = 0, pos_hist = qtd_hist;
    linha[0] = 0;
    for (;;) {
        int t = ler_tecla();
        if (t < 0) { aviso_tamanho(false); modo_terminal(TERM_LINHA); return -1; }
        if (t == '\n' || t == '\r') break;
        if (t == '\b' || t == 0x7F) {                    /* borra antes del cursor */
            if (cur > 0) {
                bool no_fim = cur == n;
                memmove(linha + cur - 1, linha + cur, (size_t)(n - cur + 1));
                n--; cur--;
                if (no_fim && !larg_ed) { mostrar("\b"); pc--; continue; }   /* caso común: solo borra el último */
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
            aviso_tamanho(false);
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
            escrever_linha(linha, 0, n);
        } else if (t == 11) {                             /* Ctrl+K: borra hasta el final */
            if (cur < n) { linha[cur] = 0; n = cur; limpar_resto(); }
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
            mostrar("\x1b[H\x1b[2J");   /* D320: ED 2 ya no lleva el cursor al origen; la consola del kernel ignora «H» */
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
            if (cur == n - 1) escrever_linha(linha, cur, n);   /* caso común: escribiendo al final */
            else reescrever_de(linha, n, cur, cur + 1);
            cur++;
        }
    }
    ir_para(n);   /* D385: la salida empieza debajo de la última fila de la orden, no en medio */
    mostrar("\n");
    aviso_tamanho(false);
    modo_terminal(TERM_LINHA);
    linha[n] = 0;
    historico_por(linha);
    *saida = sh_dup(linha);
    return n;
}
