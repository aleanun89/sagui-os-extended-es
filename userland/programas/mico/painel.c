// SPDX-License-Identifier: GPL-2.0-or-later
/* painel.c - el panel de abajo del Mico: Terminal, Problemas y Búsqueda.
 *
 * Terminal: la misma disposición del programa Terminal (D036): un sagsh conectado a
 * una terminal virtual; las teclas van al maestro y lo que escribe el shell
 * se dibuja en una grilla con historial. El F5 (principal.c) solo escribe el
 * comando en ese shell: así el programa corre a la vista, puede leer del teclado y
 * ser interrumpido con Ctrl+C, como en cualquier terminal.
 *
 * Problemas: cada línea que sale en la terminal pasa por un reconocedor:
 *   - "archivo:línea: mensaje" o "archivo:línea:columna: mensaje" (cc,
 *     D033/D039, y chip8asm, D071);
 *   - las trazas de Python: '  File "x.py", line N' seguida, más abajo,
 *     de la línea de la excepción ("NameError: ...").
 * Un clic en la lista abre el archivo y lleva el cursor a la línea. La lista se
 * vacía en cada F5, y el fin de la ejecución se reconoce cuando el prompt
 * del shell ("sagui:...$ ") vuelve.
 *
 * Búsqueda: los resultados de "Buscar en la carpeta" (buscar.c), con la
 * misma lista y el mismo clic. */
#include "mico.h"
#include "agentes.h"

#define IA_TEXTO_MAX 8192
#define IA_PERGUNTA_MAX 128
#define IA_CONTEXTO_MAX 1800
#define IA_BOTOES_A 24

static caixa_t ia_pergunta;
static char ia_resposta[IA_TEXTO_MAX];
static int ia_tamanho;
static bool ia_ocupada, ia_parar, ia_aviso_privacidade;
static ret_t ia_campo, ia_enviar, ia_deter;
static int ia_pl0, ia_pn;   /* líneas del fragmento que se envió (propuesta, D240) */
static bool ia_ptodo;

#define T_COLS_MAX 240
#define T_LINS_MAX 64
#define T_HIST 400
#define ABAS_A 22
#define ALT_ITEM 18
#define MAX_PROB 200
#define MAX_RES 400

int painel_aba = PAINEL_TERMINAL;

static char grade[T_LINS_MAX][T_COLS_MAX];
static char hist[T_HIST][T_COLS_MAX];
static int hist_ini, hist_n, volta;
static int tcols = 80, tlins = 8, tcol, tlin, esc;
static int suj0 = T_LINS_MAX, suj1 = -1;
static int mestre = -1, shell = -1;
static bool sel_ativa, selecionando;
static int sel_al, sel_ac, sel_l, sel_c;

typedef struct { char arq[CAMINHO_MAX]; int lin, col; char texto[160]; bool erro; } item_t;
static item_t *probs, *res;
static int nprobs, nres, sel_prob = -1, sel_res = -1, topo_prob, topo_res;

/* reconocimiento de las líneas que salen */
static char linha[512];
static int nlinha;
static char pend_arq[CAMINHO_MAX];
static int pend_lin;
static int execucao;      /* 0 = nada; 1 = F5 enviado; 2 = salió el comando, esperando el prompt */
static unsigned t_execucao;

static ret_t r_term(void) { return (ret_t){ r_painel.x, r_painel.y + ABAS_A, r_painel.l, r_painel.a - ABAS_A }; }

/* ------------------------------------------------------------ grilla del terminal */

static void tsujar(int l) { if (l < suj0) suj0 = l; if (l > suj1) suj1 = l; }
static void tsujar_tudo(void) { suj0 = 0; suj1 = tlins - 1; }

static void guardar_hist(const char *l)
{
    int i = (hist_ini + hist_n) % T_HIST;
    if (hist_n < T_HIST) hist_n++; else hist_ini = (hist_ini + 1) % T_HIST;
    memcpy(hist[i], l, T_COLS_MAX);
}

static const char *linha_vis(int l)
{
    int v = hist_n - volta + l;
    return v < hist_n ? hist[(hist_ini + v) % T_HIST] : grade[v - hist_n];
}

static void rolar(void)
{
    guardar_hist(grade[0]);
    memmove(grade[0], grade[1], (size_t)(tlins - 1) * T_COLS_MAX);
    memset(grade[tlins - 1], ' ', T_COLS_MAX);
    tlin = tlins - 1;
    tsujar_tudo();
}

static void sem_selecao(void)
{
    if (!sel_ativa) return;
    sel_ativa = selecionando = false;
    tsujar_tudo();
}

static item_t *novo_item(item_t *v, int *n, int max)
{
    if (!v || *n >= max) return NULL;
    item_t *it = &v[(*n)++];
    memset(it, 0, sizeof *it);
    return it;
}

static void add_problema(const char *arq, int lin, int col, const char *msg, bool erro)
{
    item_t *it = novo_item(probs, &nprobs, MAX_PROB);
    if (!it) return;
    caminho_absoluto(arq, it->arq);
    it->lin = lin;
    it->col = col;
    copia_texto(it->texto, msg, sizeof it->texto);
    it->erro = erro;
    registrar("mico: problema %s:%d: %s", it->arq, lin, it->texto);
    sujo |= R_PAINEL;
}

static bool digitos(const char *s, int *v, const char **fim)
{
    if (*s < '0' || *s > '9') return false;
    int x = 0;
    while (*s >= '0' && *s <= '9') x = x * 10 + (*s++ - '0');
    *v = x;
    *fim = s;
    return true;
}

/* Una línea entera que salió en la terminal. */
static void analisar(char *s)
{
    if (execucao) registrar("mico: tv| %s", s);
    /* Python: '  File "x.py", line 3, in <module>' */
    const char *f = s;
    while (*f == ' ') f++;
    if (comeca_com(f, "File \"")) {
        const char *a = f + 6, *b = acha_char(a, '"');
        if (b && comeca_com(b, "\", line ")) {
            int n;
            const char *fim;
            if (digitos(b + 8, &n, &fim)) {
                int k = (int)(b - a) < CAMINHO_MAX - 1 ? (int)(b - a) : CAMINHO_MAX - 1;
                memcpy(pend_arq, a, (size_t)k);
                pend_arq[k] = 0;
                pend_lin = n;
            }
        }
        return;
    }
    if (pend_arq[0] && s[0] && s[0] != ' ' && !comeca_com(s, "Traceback")) {
        add_problema(pend_arq, pend_lin, 0, s, true);
        pend_arq[0] = 0;
        return;
    }
    /* cc / chip8asm: "archivo:línea: mensaje" (el archivo sin espacios) */
    const char *p = s;
    while (*p && *p != ':' && *p != ' ') p++;
    if (*p != ':' || p == s) return;
    int lin, col = 0;
    const char *q;
    if (!digitos(p + 1, &lin, &q) || *q != ':') return;
    if (q[1] >= '0' && q[1] <= '9') { const char *r; if (digitos(q + 1, &col, &r) && *r == ':') q = r; }
    q++;
    while (*q == ' ') q++;
    char arq[CAMINHO_MAX];
    int k = (int)(p - s) < CAMINHO_MAX - 1 ? (int)(p - s) : CAMINHO_MAX - 1;
    memcpy(arq, s, (size_t)k);
    arq[k] = 0;
    bool erro = !comeca_com(q, "aviso") && !comeca_com(q, "warning") && !comeca_com(q, "advertencia");
    add_problema(arq, lin, col, q, erro);
}

static void fim_execucao(void)
{
    execucao = 0;
    registrar("mico: F5 terminado en %u ms, %d problemas", tempo_ms() - t_execucao, nprobs);
    if (nprobs > 0) {
        painel_aba = PAINEL_PROBLEMAS;
        avisar("%d problema%s: haz clic para ir a la línea", nprobs, nprobs == 1 ? "" : "s");
        sujo |= R_PAINEL;
    }
}

static void receber(char c)
{
    sem_selecao();
    if (volta) { volta = 0; tsujar_tudo(); }
    tsujar(tlin);
    if (esc == 1) { esc = c == '[' ? 2 : 0; return; }
    if (esc == 2) {
        esc = 0;
        if (c == 'D' && tcol > 0) tcol--;
        else if (c == 'C' && tcol < tcols - 1) tcol++;
        else if (c == 'K') { memset(&grade[tlin][tcol], ' ', (size_t)(T_COLS_MAX - tcol)); nlinha = tcol < nlinha ? tcol : nlinha; }
        else if (c == 'J') { memset(grade, ' ', sizeof grade); tcol = tlin = 0; tsujar_tudo(); }
        return;
    }
    if (c == 27) { esc = 1; return; }
    if (c == '\n') {
        linha[nlinha] = 0;
        analisar(linha);
        nlinha = 0;
        if (execucao == 1) execucao = 2;
        tcol = 0; tlin++;
    }
    else if (c == '\r') { tcol = 0; nlinha = 0; }
    else if (c == '\t') tcol = (tcol + 8) & ~7;
    else if (c == '\b') { if (tcol > 0) grade[tlin][--tcol] = ' '; if (nlinha > 0) nlinha--; }
    else {
        grade[tlin][tcol++] = c;
        if (nlinha < (int)sizeof linha - 1) linha[nlinha++] = c;
    }
    if (tcol >= tcols) { tcol = 0; tlin++; }
    if (tlin >= tlins) rolar();
    tsujar(tlin);
    /* ¿volvió el prompt después del F5? */
    if (execucao == 2 && nlinha >= 8 && linha[nlinha - 1] == ' ' && linha[nlinha - 2] == '$' && compara_texto_n(linha, "sagui:", 6) == 0)
        fim_execucao();
}

/* ------------------------------------------------------------ shell */

void painel_iniciar(void)
{
    if (!probs) probs = aloca_zerado(sizeof(item_t) * MAX_PROB);
    if (!res) res = aloca_zerado(sizeof(item_t) * MAX_RES);
    memset(grade, ' ', sizeof grade);
    tcol = tlin = 0;
    int tv[2];
    if (abrir_tv(tv) < 0) { registrar("mico: sin terminal virtual"); return; }
    if (pasta[0]) mudar_pasta(pasta);
    int g0 = duplicar(ENTRADA, 13), g1 = duplicar(SAIDA, 14), g2 = duplicar(ERROS, 15);
    duplicar(tv[1], ENTRADA);
    duplicar(tv[1], SAIDA);
    duplicar(tv[1], ERROS);
    char *args[] = { "sagsh", NULL };
    /* D263: el panel de Mico es un terminal sencillo, sin colores ni títulos (ANSI): el sagsh
     * de dentro nace sin TERM aunque Mico se abriera desde el Terminal, para que no mande
     * secuencias de escape que el panel pintaría tal cual. */
    static char *envp[96];
    int ne = 0;
    for (int e = 0; environ && environ[e] && ne < 94; e++)
        if (compara_texto_n(environ[e], "TERM=", 5) != 0) envp[ne++] = environ[e];
    envp[ne] = NULL;
    shell = executar_env("/bin/sagsh", args, envp);
    duplicar(g0, ENTRADA); duplicar(g1, SAIDA); duplicar(g2, ERROS);
    fechar(g0); fechar(g1); fechar(g2);
    fechar(tv[1]);
    mestre = tv[0];
    registrar("mico: terminal (sagsh pid %d)", shell);
}

void painel_fim(void)
{
    if (shell > 0) { matar(shell); int c; esperar(shell, &c); shell = -1; }
    if (mestre >= 0) { fechar(mestre); mestre = -1; }
}

bool painel_ler(void)
{
    if (mestre < 0) return false;
    bool fez = false;
    int d = disponivel(mestre);
    if (d > 0) {
        char buf[512];
        int n = ler(mestre, buf, sizeof buf);
        for (int i = 0; i < n; i++) receber(buf[i]);
        fez = n > 0;
    }
    int c;
    if (shell > 0 && recolher(&c) == shell) {   /* "exit" en el shell: abre otro */
        shell = -1;
        fechar(mestre);
        mestre = -1;
        const char *m = "\n[el shell terminó; se abre otro]\n";
        for (const char *p = m; *p; p++) receber(*p);
        painel_iniciar();
        fez = true;
    }
    return fez;
}

void painel_enviar(const char *s, int n)
{
    if (mestre < 0) painel_iniciar();
    if (mestre < 0) return;
    sem_selecao();
    if (volta) { volta = 0; tsujar_tudo(); }
    escrever_tudo(mestre, s, (unsigned)n);
}

void painel_cd(const char *p)
{
    char c[CAMINHO_MAX + 16];
    int n = formatar_texto(c, sizeof c, "\x15" "cd \"%s\"\n", p);
    painel_enviar(c, n);
    ver_painel = true;
    painel_aba = PAINEL_TERMINAL;
    foco = FOCO_TERMINAL;
    layout();
}

void execucao_comecou(void)
{
    problemas_limpar();
    pend_arq[0] = 0;
    execucao = 1;
    t_execucao = tempo_ms();
}

/* ------------------------------------------------------------ listas */

void problemas_limpar(void) { nprobs = 0; sel_prob = -1; topo_prob = 0; sujo |= R_PAINEL; }
int  problemas_qtd(void) { return nprobs; }
void busca_res_limpar(void) { nres = 0; sel_res = -1; topo_res = 0; sujo |= R_PAINEL; }
int  busca_res_qtd(void) { return nres; }

void busca_res_add(const char *arq, int lin, int col, const char *texto)
{
    item_t *it = novo_item(res, &nres, MAX_RES);
    if (!it) return;
    copia_texto(it->arq, arq, sizeof it->arq);
    it->lin = lin;
    it->col = col;
    const char *t = texto;
    while (*t == ' ' || *t == '\t') t++;
    copia_texto(it->texto, t, sizeof it->texto);
    it->erro = false;
}

/* ------------------------------------------------------------ dibujo */

void painel_tamanho(void)
{
    ret_t r = r_term();
    int nc = (r.l - 8) / LETRA_L, nl = (r.a - 4) / LETRA_A;
    if (nc > T_COLS_MAX) nc = T_COLS_MAX;
    if (nl > T_LINS_MAX) nl = T_LINS_MAX;
    if (nc < 10) nc = 10;
    if (nl < 1) nl = 1;
    if (tlin >= nl) {   /* el cursor quedaría afuera: el texto sube */
        int d = tlin - nl + 1;
        for (int k = 0; k < d; k++) guardar_hist(grade[k]);
        memmove(grade[0], grade[d], (size_t)(T_LINS_MAX - d) * T_COLS_MAX);
        memset(grade[T_LINS_MAX - d], ' ', (size_t)d * T_COLS_MAX);
        tlin -= d;
    }
    if (tcol >= nc) tcol = nc - 1;
    tcols = nc; tlins = nl;
    tsujar_tudo();
}

static bool selecionada(int l, int c)
{
    if (!sel_ativa) return false;
    int a = sel_al * T_COLS_MAX + sel_ac, b = sel_l * T_COLS_MAX + sel_c, x = l * T_COLS_MAX + c;
    if (a > b) { int t = a; a = b; b = t; }
    return x >= a && x <= b;
}

static void term_linha(int l)
{
    ret_t r = r_term();
    int y = r.y + 2 + l * LETRA_A;
    recorte = r;
    d_ret(r.x, y, r.l, LETRA_A, tema->term_fundo);
    const char *s = linha_vis(l);
    for (int c = 0; c < tcols; c++) {
        bool sl = selecionada(l, c);
        if (sl) d_ret(r.x + 4 + c * LETRA_L, y, LETRA_L, LETRA_A, 0x6D9BD8);
        d_letra(r.x + 4 + c * LETRA_L, y, (unsigned char)s[c], sl ? tema->term_fundo : tema->term_texto);
    }
    if (volta && l == 0) {
        char t[40];
        int n = formatar_texto(t, sizeof t, " Historial: -%d ", volta);
        d_ret(r.x + r.l - 8 - n * LETRA_L, y, n * LETRA_L, LETRA_A, tema->destaque);
        d_texto(r.x + r.l - 8 - n * LETRA_L, y, t, n, 0xFFFFFF);
    }
    if (!volta && l == tlin && tcol < tcols) {
        uint32_t cc = foco == FOCO_TERMINAL ? tema->term_texto : 0x5A6A60;
        if (foco == FOCO_TERMINAL) {
            d_ret(r.x + 4 + tcol * LETRA_L, y, LETRA_L, LETRA_A, cc);
            d_letra(r.x + 4 + tcol * LETRA_L, y, (unsigned char)grade[l][tcol], tema->term_fundo);
        } else d_moldura(r.x + 4 + tcol * LETRA_L, y, LETRA_L, LETRA_A, cc);
    }
}

/* Solo las líneas de la terminal que cambiaron (llamado en cada vuelta del bucle). */
void painel_terminal_mostrar(void)
{
    if (!ver_painel || painel_aba != PAINEL_TERMINAL || suj1 < 0) return;
    if (suj1 >= tlins) suj1 = tlins - 1;
    for (int l = suj0; l <= suj1; l++) term_linha(l);
    ret_t r = r_term();
    d_mostrar((ret_t){ r.x, r.y + 2 + suj0 * LETRA_A, r.l, (suj1 - suj0 + 1) * LETRA_A });
    suj0 = T_LINS_MAX; suj1 = -1;
}

static void desenhar_lista(item_t *v, int n, int sel, int topo, const char *vazio)
{
    ret_t r = r_term();
    recorte = r;
    d_ret(r.x, r.y, r.l, r.a, tema->painel);
    if (!n) { d_texto_ate(r.x + 12, r.y + 8, vazio, r.l - 20, tema->margem_texto); return; }
    int vis = r.a / ALT_ITEM;
    for (int k = 0; k <= vis && topo + k < n; k++) {
        item_t *it = &v[topo + k];
        int y = r.y + 2 + k * ALT_ITEM, x = r.x + 8;
        if (topo + k == sel) d_ret(r.x, y, r.l, ALT_ITEM, tema->lateral_sel);
        d_ret(x, y + 5, 8, 8, it->erro ? tema->erro : (v == probs ? tema->aviso : tema->destaque));
        x += 14;
        const char *nome = acha_ultimo_char(it->arq, '/');
        nome = nome ? nome + 1 : it->arq;
        char pos[CAMINHO_MAX + 16];
        int pn = formatar_texto(pos, sizeof pos, "%s:%d", nome, it->lin);
        d_texto(x, y + 1, pos, pn, tema->destaque);
        x += (pn + 2) * LETRA_L;
        d_texto_ate(x, y + 1, it->texto, r.x + r.l - x - 8, tema->painel_texto);
    }
}

/* D295: la pestaña «Agente» aloja el panel de agentes de la libreria ia (el modo agente del asistente: lee y edita
 * el proyecto con las herramientas y el permiso de la persona en cada cambio). */
static ag_panel_t *ag_p;

static ag_panel_t *agente_panel(void)
{
    ret_t r = r_term();
    if (!ag_p) {
        ag_p = ag_panel_criar(j, r.x + 4, r.y, r.l - 8, r.a, NULL);
        if (ag_p) {
            ia_config_t c;
            ia_config_carregar(&c);
            ia_config_escolher(&c, IA_USO_MICO);
            ag_panel_config(ag_p, &c);
            ag_panel_escolher(ag_p, "Programador");
        }
    } else ag_panel_mudar_tamanho(ag_p, r.x + 4, r.y, r.l - 8, r.a);
    return ag_p;
}

static void rotulo_aba(int i, char *b)
{
    if (i == PAINEL_TERMINAL) copia_texto(b, "Terminal", 40);
    else if (i == PAINEL_PROBLEMAS) formatar_texto(b, 40, "Problemas (%d)", nprobs);
    else if (i == PAINEL_BUSCA) formatar_texto(b, 40, "Búsqueda (%d)", nres);
    else if (i == PAINEL_AGENTE) copia_texto(b, "Agente", 40);
    else copia_texto(b, "Asistente", 40);
}

static bool url_externa(const char *url)
{
    const char *p = comeca_com(url, "https://") ? url + 8 : comeca_com(url, "http://") ? url + 7 : url;
    if (compara_texto_n(p, "localhost", 9) == 0 || compara_texto_n(p, "127.", 4) == 0 ||
        compara_texto_n(p, "10.", 3) == 0 || compara_texto_n(p, "192.168.", 8) == 0 ||
        compara_texto_n(p, "10.0.2.2", 8) == 0 ||
        compara_texto_n(p, "172.16.", 7) == 0 || compara_texto_n(p, "172.17.", 7) == 0 ||
        compara_texto_n(p, "172.18.", 7) == 0 || compara_texto_n(p, "172.19.", 7) == 0 ||
        compara_texto_n(p, "172.20.", 7) == 0 || compara_texto_n(p, "172.21.", 7) == 0 ||
        compara_texto_n(p, "172.22.", 7) == 0 || compara_texto_n(p, "172.23.", 7) == 0 ||
        compara_texto_n(p, "172.24.", 7) == 0 || compara_texto_n(p, "172.25.", 7) == 0 ||
        compara_texto_n(p, "172.26.", 7) == 0 || compara_texto_n(p, "172.27.", 7) == 0 ||
        compara_texto_n(p, "172.28.", 7) == 0 || compara_texto_n(p, "172.29.", 7) == 0 ||
        compara_texto_n(p, "172.30.", 7) == 0 || compara_texto_n(p, "172.31.", 7) == 0 ||
        comeca_com(p, "[::1]") || comeca_com(p, "192.0.2.") || comeca_com(p, "host.docker.internal")) return false;
    return true;
}

static void ia_desenhar_resposta(void)
{
    ret_t r = r_term();
    recorte = r;
    d_ret(r.x, r.y, r.l, r.a, tema->painel);
    if (prop_ativa()) {   /* la vista de diferencias sustituye al texto (D240) */
        prop_desenhar(r);
        int y = r.y + r.a - 58;
        ia_campo = (ret_t){ r.x + 8, y, r.l - 16, 26 };
        if (ia_campo.l > 250) ia_campo.l -= 176;
        ia_pergunta.x = ia_campo.x; ia_pergunta.y = y; ia_pergunta.l = ia_campo.l;
        ia_pergunta.foco = foco == FOCO_ASISTENTE;
        caixa_desenhar(j, &ia_pergunta);
        ia_enviar = (ret_t){ r.x + r.l - 164, y, 76, 26 };
        ia_deter = (ret_t){ r.x + r.l - 80, y, 72, 26 };
        d_ret(ia_enviar.x, ia_enviar.y, ia_enviar.l, ia_enviar.a, tema->destaque);
        d_texto(ia_enviar.x + 12, ia_enviar.y + 5, "Enviar", 6, 0xFFFFFF);
        d_ret(ia_deter.x, ia_deter.y, ia_deter.l, ia_deter.a, ia_ocupada ? tema->erro : tema->barra_borda);
        d_texto(ia_deter.x + 5, ia_deter.y + 5, "Detener", 7, 0xFFFFFF);
        d_mostrar(r);
        return;
    }
    int max_linhas = (r.a - IA_BOTOES_A - 42) / LETRA_A;
    if (max_linhas < 1) max_linhas = 1;
    int total = 1;
    for (int i = 0; i < ia_tamanho; i++) if (ia_resposta[i] == '\n') total++;
    int p = 0, linha = 0, saltar = total > max_linhas ? total - max_linhas : 0;
    int colunas = (r.l - 24) / LETRA_L;
    if (colunas < 1) colunas = 1;
    while (p < ia_tamanho && linha < total) {
        int ini = p;
        while (p < ia_tamanho && ia_resposta[p] != '\n' && p - ini < colunas) p++;
        if (linha >= saltar) d_texto(r.x + 12, r.y + 8 + (linha - saltar) * LETRA_A, ia_resposta + ini, p - ini, tema->painel_texto);
        if (p < ia_tamanho && ia_resposta[p] == '\n') p++;
        linha++;
    }
    if (!ia_tamanho) d_texto(r.x + 12, r.y + 8, "Escribe una pregunta o elige una acción.", 40, tema->margem_texto);
    int y = r.y + r.a - 58;
    ia_campo = (ret_t){ r.x + 8, y, r.l - 16, 26 };
    if (ia_campo.l > 250) ia_campo.l -= 176;
    ia_pergunta.x = ia_campo.x; ia_pergunta.y = y; ia_pergunta.l = ia_campo.l;
    ia_pergunta.foco = foco == FOCO_ASISTENTE;
    caixa_desenhar(j, &ia_pergunta);
    ia_enviar = (ret_t){ r.x + r.l - 164, y, 76, 26 };
    ia_deter = (ret_t){ r.x + r.l - 80, y, 72, 26 };
    d_ret(ia_enviar.x, ia_enviar.y, ia_enviar.l, ia_enviar.a, tema->destaque);
    d_texto(ia_enviar.x + 12, ia_enviar.y + 5, "Enviar", 6, 0xFFFFFF);
    d_ret(ia_deter.x, ia_deter.y, ia_deter.l, ia_deter.a, ia_ocupada ? tema->erro : tema->barra_borda);
    d_texto(ia_deter.x + 5, ia_deter.y + 5, "Detener", 7, 0xFFFFFF);
    d_texto(r.x + 12, r.y + r.a - 26, "Explicar", 8, tema->destaque);
    d_texto(r.x + 104, r.y + r.a - 26, "Corregir Problemas", 18, tema->destaque);
    d_texto(r.x + 292, r.y + r.a - 26, "Generar función", 15, tema->destaque);
    d_mostrar(r);
}

static void ia_anexar(const char *s, int n)
{
    if (n <= 0 || ia_tamanho >= (int)sizeof ia_resposta - 1) return;
    if (n > (int)sizeof ia_resposta - 1 - ia_tamanho) n = (int)sizeof ia_resposta - 1 - ia_tamanho;
    memcpy(ia_resposta + ia_tamanho, s, (size_t)n);
    ia_tamanho += n;
    ia_resposta[ia_tamanho] = 0;
    ia_desenhar_resposta();
}

static void ia_ao_texto(const char *s, int n, void *ctx)
{
    (void)ctx;
    ia_anexar(s, n);
}

static int ia_deve_parar(void *ctx)
{
    (void)ctx;
    evento_t e;
    while (proximo_evento(j->id, &e, false) == 1) {
        if (e.tipo == EV_TECLA && e.tecla == TECLA_ESC) ia_parar = true;
        else if (e.tipo == EV_CLIQUE && dentro(e.x, e.y, ia_deter)) ia_parar = true;
        else if (e.tipo == EV_FECHAR) ia_parar = true;
    }
    return ia_parar ? 1 : 0;
}

static void ia_enviar_pregunta(const char *extra, int modo)
{
    if (ia_ocupada) { ia_parar = true; return; }
    ia_config_t cfg;
    ia_config_carregar(&cfg);
    /* El modelo que le toca al Mico (D145). */
    ia_config_escolher(&cfg, IA_USO_MICO);
    if (!cfg.url[0]) { avisar("Configura IA_URL o /home/.config/ia/provedor"); return; }
    if (url_externa(cfg.url) && !ia_aviso_privacidade) {
        if (!confirmar("Asistente IA", "El código y la conversación se enviarán a un proveedor externo. ¿Continuar?")) return;
        ia_aviso_privacidade = true;
    }
    char contexto[IA_CONTEXTO_MAX + 64], pedido[4096], erro[200];
    int nctx = 0, s0, s1;
    doc_t *d = DOC;
    if (modo == 2) {
        nctx += formatar_texto(contexto + nctx, sizeof contexto - (size_t)nctx, "\nErrores de Problemas:\n");
        for (int i = 0; i < nprobs && i < 8 && nctx < IA_CONTEXTO_MAX; i++) {
            item_t *it = &probs[i];
            nctx += formatar_texto(contexto + nctx, sizeof contexto - (size_t)nctx, "%s:%d: %s\n", it->arq, it->lin, it->texto);
        }
    }
    bool propor = modo == 2 || modo == 3;
    prop_descartar();
    if (propor) {
        /* El fragmento se envía con líneas enteras para poder pedir el código completo
         * y compararlo línea a línea (D240). */
        int l0, l1;
        ia_ptodo = false;
        if (modo == 3) { l0 = doc_linha_de(d, d->cursor); l1 = l0 - 1; }   /* se inserta aquí */
        else if (vista_selecao(&s0, &s1)) {
            l0 = doc_linha_de(d, s0);
            l1 = doc_linha_de(d, s1);
            if (s1 > s0 && s1 == doc_ini(d, l1)) l1--;
        } else if (d->tam <= IA_CONTEXTO_MAX - nctx) { l0 = 0; l1 = d->nli - 1; ia_ptodo = true; }
        else {
            int c = doc_linha_de(d, d->cursor);
            l0 = c; l1 = c;
            while (l0 > 0 && doc_ini(d, l1 + 1 < d->nli ? l1 + 1 : l1) - doc_ini(d, l0 - 1) < IA_CONTEXTO_MAX - nctx - 64) {
                l0--;
                if (l1 + 1 < d->nli && doc_ini(d, l1 + 2 <= d->nli ? l1 + 1 : l1) - doc_ini(d, l0) < IA_CONTEXTO_MAX - nctx - 64) l1++;
                if (c - l0 > 40) break;
            }
        }
        if (l1 >= d->nli) l1 = d->nli - 1;
        if (ia_ptodo && l1 >= 0 && doc_tam_linha(d, l1) == 0) l1--;   /* la vacía tras el último '\n' */
        ia_pl0 = l0; ia_pn = l1 - l0 + 1;
        if (ia_pn < 0) ia_pn = 0;
        nctx += formatar_texto(contexto + nctx, sizeof contexto - (size_t)nctx,
                               "\nArchivo %s (%s). ", doc_nome(d), nome_ling[d->ling]);
        if (ia_pn > 0) {
            int ini = doc_ini(d, l0), fin = l1 + 1 < d->nli ? doc_ini(d, l1 + 1) - 1 : doc_fim(d, l1);
            int tam = fin - ini;
            if (tam > IA_CONTEXTO_MAX - nctx) tam = IA_CONTEXTO_MAX - nctx;
            nctx += formatar_texto(contexto + nctx, sizeof contexto - (size_t)nctx, "Código actual:\n");
            doc_pedaco(d, ini, tam, contexto + nctx);
            nctx += tam;
        } else nctx += formatar_texto(contexto + nctx, sizeof contexto - (size_t)nctx, "El código irá antes de la línea %d.", l0 + 1);
        contexto[nctx] = 0;
    } else if (modo != 3) {
        int ini, tam;
        bool sel = vista_selecao(&s0, &s1);
        if (sel) { ini = s0; tam = s1 - s0; }
        else { int l = doc_linha_de(d, d->cursor); ini = doc_ini(d, l); tam = doc_tam_linha(d, l); }
        if (tam > IA_CONTEXTO_MAX - nctx) tam = IA_CONTEXTO_MAX - nctx;
        if (tam > 0) {
            nctx += formatar_texto(contexto + nctx, sizeof contexto - (size_t)nctx,
                                   "\nCódigo de %s (%s):\n", doc_nome(d), nome_ling[d->ling]);
            if (tam > IA_CONTEXTO_MAX - nctx) tam = IA_CONTEXTO_MAX - nctx;
            doc_pedaco(d, ini, tam, contexto + nctx);
            nctx += tam;
            contexto[nctx] = 0;
        }
    } else contexto[nctx] = 0;
    cfg.sistema[0] = 0;
    char instruccion[256];
    copia_texto(instruccion, "Responde en español. No cambies archivos ni ejecutes órdenes.", sizeof instruccion);
    if (propor) copia_texto(instruccion + tam_texto(instruccion), " Explica brevemente el cambio y da el código COMPLETO del fragmento resultante (no solo las líneas que cambian) en un único bloque ```.", sizeof instruccion - tam_texto(instruccion));
    copia_texto(cfg.sistema, instruccion, sizeof cfg.sistema);
    const char *accion = modo == 1 ? "Explica el código." : modo == 2 ? "Corrige el código para resolver estos errores." :
                         modo == 3 ? "Genera una función adecuada para este archivo." : "Responde a la consulta del usuario.";
    if (extra && *extra) formatar_texto(pedido, sizeof pedido, "%s\n%s\n%s", accion, contexto, extra);
    else formatar_texto(pedido, sizeof pedido, "%s\n%s", accion, contexto);
    if (extra) { ia_pergunta.texto[0] = 0; ia_pergunta.cursor = 0; }
    ia_tamanho = 0; ia_resposta[0] = 0; ia_parar = false; ia_ocupada = true;
    ia_desenhar_resposta();
    registrar("mico: asistente pedido (contexto %d bytes, accion %d)", nctx, modo);
    int r = ia_chat(&cfg, pedido, ia_ao_texto, NULL, NULL, NULL, ia_deve_parar, NULL, erro, sizeof erro);
    ia_ocupada = false;
    if (r < 0) {
        ia_tamanho = 0; ia_resposta[0] = 0;
        copia_texto(ia_resposta, erro, sizeof ia_resposta);
        ia_tamanho = (int)tam_texto(ia_resposta);
        ia_desenhar_resposta();
        registrar("mico: asistente error %s", erro);
        avisar("Asistente: %s", erro);
    } else if (ia_parar) {
        ia_anexar("\n[resposta interrompida]", (int)tam_texto("\n[resposta interrompida]"));
        registrar("mico: asistente detenido");
        avisar("Respuesta detenida");
    } else {
        char resumen[161];
        copia_texto(resumen, ia_resposta, sizeof resumen);
        registrar("mico: asistente respuesta %s", resumen);
        avisar("Respuesta del asistente recibida");
        if (propor && prop_criar(d, ia_pl0, ia_pn, ia_ptodo, ia_resposta, ia_tamanho)) {
            foco = FOCO_ASISTENTE;
            avisar("Propuesta de cambio: revisa las diferencias y pulsa Aplicar");
        } else if (propor) registrar("mico: propuesta sin cambios aplicables");
        ia_desenhar_resposta();
    }
}

void painel_ia_acao(int acao)
{
    ver_painel = true;
    painel_aba = PAINEL_ASSISTENTE;
    if (acao == 1) ia_enviar_pregunta(NULL, 1);
    else if (acao == 2) ia_enviar_pregunta(NULL, 2);
    else if (acao == 3) ia_enviar_pregunta(NULL, 3);
    else if (ia_pergunta.texto[0]) {
        char q[sizeof ia_pergunta.texto];
        copia_texto(q, ia_pergunta.texto, sizeof q);
        ia_enviar_pregunta(q, 0);
    }
    sujo |= R_PAINEL;
}

void painel_ia_deter(void) { if (ia_ocupada) ia_parar = true; }
bool painel_ia_em_curso(void) { return ia_ocupada; }

bool painel_ia_tecla(unsigned t, unsigned mods)
{
    if (painel_aba == PAINEL_AGENTE && foco == FOCO_ASISTENTE) {
        if (t == TECLA_ESC) { foco = FOCO_EDITOR; sujo |= R_PAINEL; return true; }
        ag_panel_t *p = agente_panel();
        evento_t e = { .tipo = EV_TECLA, .tecla = t, .modificadores = mods };
        if (p) ag_panel_tratar(p, &e);
        return true;
    }
    if (painel_aba != PAINEL_ASSISTENTE || foco != FOCO_ASISTENTE) return false;
    if (t == TECLA_ESC) { if (ia_ocupada) painel_ia_deter(); else foco = FOCO_EDITOR; return true; }
    if (ia_ocupada) return true;
    if (prop_tecla(t)) return true;
    evento_t e = { .tipo = EV_TECLA, .tecla = t, .modificadores = mods };
    if (caixa_tratar(j, &ia_pergunta, &e)) { painel_ia_acao(0); return true; }
    sujo |= R_PAINEL;
    return true;
}

void painel_desenhar(void)
{
    if (!ver_painel) return;
    recorte = r_painel;
    d_ret(r_painel.x, r_painel.y, r_painel.l, ABAS_A, tema->barra);
    d_ret(r_painel.x, r_painel.y, r_painel.l, 1, tema->barra_borda);
    char t[5][40];
    for (int i = 0; i < 5; i++) rotulo_aba(i, t[i]);
    int x = r_painel.x + 8;
    for (int i = 0; i < 5; i++) {
        int w = (int)tam_texto(t[i]) * LETRA_L + 16;
        bool at = painel_aba == i;
        if (at) { d_ret(x, r_painel.y + 1, w, ABAS_A - 1, tema->painel); d_ret(x, r_painel.y + ABAS_A - 3, w, 3, tema->aba_marca); }
        uint32_t cor = i == 1 && nprobs ? tema->erro : (at ? tema->barra_texto : tema->margem_texto);
        d_texto(x + 8, r_painel.y + 3, t[i], (int)tam_texto(t[i]), cor);
        x += w + 4;
    }
    /* "x" para ocultar el panel */
    d_texto(r_painel.x + r_painel.l - 20, r_painel.y + 3, "x", 1, tema->margem_texto);
    if (painel_aba == PAINEL_TERMINAL) {
        ret_t r = r_term();
        recorte = r;
        d_ret(r.x, r.y, r.l, r.a, tema->term_fundo);
        for (int l = 0; l < tlins; l++) term_linha(l);
        suj0 = T_LINS_MAX; suj1 = -1;
    } else if (painel_aba == PAINEL_PROBLEMAS) desenhar_lista(probs, nprobs, sel_prob, topo_prob, "Sin problemas. F5 compila y ejecuta el archivo actual.");
    else if (painel_aba == PAINEL_BUSCA) desenhar_lista(res, nres, sel_res, topo_res, "Sin resultados. Ctrl+Mayús+F busca en todos los archivos de la carpeta.");
    else if (painel_aba == PAINEL_AGENTE) { ag_panel_t *p = agente_panel(); if (p) ag_panel_desenhar(p); }
    else ia_desenhar_resposta();
    d_mostrar(r_painel);
}

/* ------------------------------------------------------------ eventos */

static void celula(int x, int y, int *l, int *c)
{
    ret_t r = r_term();
    *l = (y - r.y - 2) / LETRA_A; *c = (x - r.x - 4) / LETRA_L;
    if (*l < 0) *l = 0;
    if (*l >= tlins) *l = tlins - 1;
    if (*c < 0) *c = 0;
    if (*c >= tcols) *c = tcols - 1;
}

static int aba_em(int x)
{
    int cx = r_painel.x + 8;
    char b[40];
    for (int i = 0; i < 5; i++) {
        rotulo_aba(i, b);
        int w = (int)tam_texto(b) * LETRA_L + 16;
        if (x >= cx && x < cx + w) return i;
        cx += w + 4;
    }
    return -1;
}

void painel_clique(int x, int y, int botoes, int cliques)
{
    (void)cliques;
    if (y < r_painel.y + ABAS_A) {
        if (x >= r_painel.x + r_painel.l - 28) { ver_painel = false; if (foco == FOCO_TERMINAL) foco = FOCO_EDITOR; layout(); return; }
        int a = aba_em(x);
        if (a >= 0) { painel_aba = a; if (a == PAINEL_TERMINAL) foco = FOCO_TERMINAL; else if (a == PAINEL_ASSISTENTE || a == PAINEL_AGENTE) foco = FOCO_ASISTENTE; sujo |= R_PAINEL; }
        return;
    }
    if (painel_aba == PAINEL_AGENTE) {
        ag_panel_t *p = agente_panel();
        foco = FOCO_ASISTENTE;
        evento_t e = { .tipo = EV_CLIQUE, .x = x, .y = y, .botoes = (unsigned)botoes };
        if (p) ag_panel_tratar(p, &e);
        sujo |= R_PAINEL;
        return;
    }
    if (painel_aba == PAINEL_TERMINAL) {
        foco = FOCO_TERMINAL;
        sujo |= R_EDITOR | R_PAINEL;
        vista_sujar_tudo();
        if (botoes & 2) {
            menu_contextual(j->id, x, y, "Copiar\tCtrl+Mayús+C\nPegar\tCtrl+Mayús+V\n-\nLimpiar\nInterrumpir\tCtrl+C", 4);
            return;
        }
        sem_selecao();
        celula(x, y, &sel_al, &sel_ac);
        sel_l = sel_al; sel_c = sel_ac;
        selecionando = true;
        return;
    }
    if (painel_aba == PAINEL_ASSISTENTE) {
        foco = FOCO_ASISTENTE;
        evento_t e = { .tipo = EV_CLIQUE, .x = x, .y = y };
        if (dentro(x, y, ia_campo)) { caixa_tratar(j, &ia_pergunta, &e); sujo |= R_PAINEL; return; }
        if (dentro(x, y, ia_enviar)) { painel_ia_acao(0); return; }
        if (dentro(x, y, ia_deter)) { painel_ia_deter(); return; }
        if (!ia_ocupada && prop_clique(x, y)) return;
        if (prop_ativa()) return;
        if (y >= r_painel.y + r_painel.a - 30) {
            if (x < r_painel.x + 96) painel_ia_acao(1);
            else if (x < r_painel.x + 280) painel_ia_acao(2);
            else if (x < r_painel.x + 440) painel_ia_acao(3);
        }
        return;
    }
    ret_t r = r_term();
    bool p = painel_aba == PAINEL_PROBLEMAS;
    int i = (p ? topo_prob : topo_res) + (y - r.y - 2) / ALT_ITEM;
    if (i < 0 || i >= (p ? nprobs : nres)) return;
    item_t *it = p ? &probs[i] : &res[i];
    if (p) sel_prob = i; else sel_res = i;
    sujo |= R_PAINEL;
    registrar("mico: salto a %s:%d", it->arq, it->lin);
    ir_para(it->arq, it->lin, it->col);
}

void painel_mover(int x, int y)
{
    if (!selecionando) return;
    int l, c;
    celula(x, y, &l, &c);
    if (l != sel_l || c != sel_c || !sel_ativa) { sel_l = l; sel_c = c; sel_ativa = true; tsujar_tudo(); }
}

void painel_soltar(void) { selecionando = false; }

void painel_rodar(int n)
{
    if (painel_aba == PAINEL_TERMINAL) {
        int antes = volta;
        volta -= n * RODA_LINHAS;
        if (volta > hist_n) volta = hist_n;
        if (volta < 0) volta = 0;
        if (volta != antes) { sem_selecao(); tsujar_tudo(); }
        return;
    }
    if (painel_aba == PAINEL_ASSISTENTE) { if (prop_ativa()) prop_rodar(n); return; }
    ret_t r = r_term();
    int vis = r.a / ALT_ITEM, *topo = painel_aba == PAINEL_PROBLEMAS ? &topo_prob : &topo_res;
    int total = painel_aba == PAINEL_PROBLEMAS ? nprobs : nres;
    *topo += n * RODA_LINHAS;
    if (*topo > total - vis) *topo = total - vis;
    if (*topo < 0) *topo = 0;
    sujo |= R_PAINEL;
}

void painel_copiar(void)
{
    static char t[T_LINS_MAX * (T_COLS_MAX + 1)];
    int n = 0, l0, c0, l1, c1;
    if (sel_ativa) {
        int a = sel_al * T_COLS_MAX + sel_ac, b = sel_l * T_COLS_MAX + sel_c;
        if (a > b) { int x = a; a = b; b = x; }
        l0 = a / T_COLS_MAX; c0 = a % T_COLS_MAX; l1 = b / T_COLS_MAX; c1 = b % T_COLS_MAX;
    } else { l0 = l1 = volta ? tlins - 1 : tlin; c0 = 0; c1 = tcols - 1; }
    for (int l = l0; l <= l1 && l < tlins; l++) {
        const char *s = linha_vis(l);
        int de = l == l0 ? c0 : 0, ate = l == l1 ? c1 : tcols - 1, fim = ate;
        while (fim >= de && s[fim] == ' ') fim--;
        for (int c = de; c <= fim; c++) t[n++] = s[c];
        if (l < l1) t[n++] = '\n';
    }
    copiar_texto(t, (unsigned)n);
    registrar("mico: terminal copiado %d bytes", n);
    avisar("Copiado del terminal (%d caracteres)", n);
}

void painel_colar(void)
{
    int n = colar_texto(NULL, 0);
    if (n <= 0) return;
    char *b = aloca((size_t)n + 1);
    if (!b) return;
    n = colar_texto(b, (unsigned)n + 1);
    int k = 0;
    for (int i = 0; i < n; i++) if (b[i] != '\r') b[k++] = b[i];
    painel_enviar(b, k);
    libera(b);
    registrar("mico: terminal pegado %d bytes", k);
}

void painel_menu(int idx)
{
    if (idx == 0) painel_copiar();
    else if (idx == 1) painel_colar();
    else if (idx == 3) painel_enviar("\x0c", 1);   /* Ctrl+L: el sagsh limpia la pantalla */
    else if (idx == 4) painel_enviar("\x03", 1);
}

void painel_tecla(unsigned t, unsigned mods)
{
    if (t == 3 && (mods & 3) == 3) { painel_copiar(); return; }
    if (t == 22 && (mods & 3) == 3) { painel_colar(); return; }
    char b[2];
    int n = 1;
    if (t < 0x100) b[0] = (char)t;
    else {
        b[0] = 27;
        switch (t) {
        case TECLA_CIMA: b[1] = TECLA_B_CIMA; break;
        case TECLA_BAIXO: b[1] = TECLA_B_BAIXO; break;
        case TECLA_ESQUERDA: b[1] = TECLA_B_ESQUERDA; break;
        case TECLA_DIREITA: b[1] = TECLA_B_DIREITA; break;
        case TECLA_HOME: b[1] = TECLA_B_HOME; break;
        case TECLA_END: b[1] = TECLA_B_END; break;
        case TECLA_DELETE: b[1] = TECLA_B_DELETE; break;
        case TECLA_PGUP: b[1] = TECLA_B_PGUP; break;
        case TECLA_PGDN: b[1] = TECLA_B_PGDN; break;
        case TECLA_ESC: b[1] = 27; break;
        default: return;
        }
        n = 2;
    }
    painel_enviar(b, n);
}
