// SPDX-License-Identifier: GPL-3.0-only
/* executar.c - ejecuta el árbol (D037).
 *
 * Sin fork (D010), cada cosa se ejecuta donde corresponde:
 *   - programa: executar() con los descriptores 0/1/2 ya arreglados (el hijo
 *     hereda solo esos tres); el shell guarda los suyos en descriptores altos y
 *     los restaura después;
 *   - interno, función, if/for/while/case, { }: en el propio shell;
 *   - ( ... ) y $( ... ): en el propio shell, con el estado (variables,
 *     funciones, carpeta, opciones) guardado antes y restaurado después;
 *   - pipe: las etapas que son programas se vuelven procesos; de las etapas internas,
 *     una se ejecuta en el propio shell (la última, si es interna: "echo x | read v"
 *     funciona como en zsh) y las otras se vuelven un /bin/sagsh hijo que recibe
 *     el estado en un preludio (serializar_estado);
 *   - "&" de algo interno: también un sagsh hijo.
 *
 * El control de flujo (break, continue, return, exit, Ctrl+C) es una marca en
 * sh.fluxo que cada bucle y lista verifica, en vez de longjmp.
 *
 * Trabajos: en el modo interactivo cada trabajo obtiene un grupo de procesos
 * (todas las etapas del pipe) y el grupo pasa al primer plano del terminal
 * mientras el shell espera: el Ctrl+C termina el grupo entero (código 130). */
#include "sagsh.h"

shell_t sh;

#define MAX_FUNCOES 1000
#define MAX_TRAB    32
#define MAX_PIDS    16
#define LIMITE_PILHA (44 * 1024)

static int executar_simples(no_t *n);

/* ------------------------------------------------------------ descriptores */

int fd_livre(void)
{
    for (int fd = 10; fd < 32; fd++)
        if (tipo_descritor(fd) < 0) return fd;
    return -1;
}

/* Descriptores guardados por un grupo de redirecciones. El vector solo se
 * reserva cuando hay redirección: la estructura queda pequeña en la pila
 * (la pila del programa tiene 64 KB y la ejecución es recursiva). */
typedef struct { int fd, guarda; } salvo_t;
typedef struct { salvo_t *v; int n; } salvos_t;

static bool salvar_fd(salvos_t *s, int fd)
{
    for (int i = 0; i < s->n; i++)
        if (s->v[i].fd == fd) return true;
    if (s->n == 24) { erro("demasiadas redirecciones"); return false; }
    if (!s->v) s->v = sh_aloca(24 * sizeof(salvo_t));
    int g = -1;
    if (tipo_descritor(fd) >= 0) {
        g = fd_livre();
        if (g < 0) { erro("demasiados descriptores abiertos"); return false; }
        duplicar(fd, g);
    }
    s->v[s->n].fd = fd;
    s->v[s->n++].guarda = g;
    return true;
}

static void desfazer(salvos_t *s)
{
    for (int i = s->n - 1; i >= 0; i--) {
        if (s->v[i].guarda >= 0) {
            duplicar(s->v[i].guarda, s->v[i].fd);
            fechar(s->v[i].guarda);
        } else {
            fechar(s->v[i].fd);
        }
    }
    s->n = 0;
    libera(s->v);
    s->v = NULL;
}

/* exec >archivo: queda vigente; solo cierra las copias guardadas. */
static void esquecer(salvos_t *s)
{
    for (int i = 0; i < s->n; i++)
        if (s->v[i].guarda >= 0) fechar(s->v[i].guarda);
    s->n = 0;
    libera(s->v);
    s->v = NULL;
}

/* Pone un texto como entrada: pipe (si cabe en el buffer de 512 bytes) o
 * archivo temporal ya borrado (el archivo desaparece al cerrarse). */
static int fd_com_texto(const char *s)
{
    int n = (int)tam_texto(s);
    if (n <= 500) {
        int p[2];
        if (criar_pipe(p) < 0) return ERRO_LIMITE;
        if (n) escrever_tudo(p[1], s, (unsigned)n);
        fechar(p[1]);
        return p[0];
    }
    static int seq;
    char nome[64];
    formatar_texto(nome, sizeof nome, "/tmp/.sagsh-doc-%d-%d", sh.pid, seq++);
    int fd = abrir(nome, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return fd;
    escrever_tudo(fd, s, (unsigned)n);
    fechar(fd);
    fd = abrir(nome, ABRIR_LER);
    apagar(nome);
    return fd;
}

/* /dev/null no existe en el SGFS: la entrada se vuelve un pipe vacío y la salida un
 * archivo temporal ya borrado. */
static int abrir_nulo(bool escrita)
{
    if (!escrita) return fd_com_texto("");
    static int seq;
    char nome[64];
    formatar_texto(nome, sizeof nome, "/tmp/.sagsh-nulo-%d-%d", sh.pid, seq++);
    int fd = abrir(nome, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd >= 0) apagar(nome);
    return fd;
}

static bool colocar(salvos_t *s, int novo, int fd)
{
    if (novo == fd) return true;
    duplicar(novo, fd);
    fechar(novo);
    (void)s;
    return true;
}

static bool aplicar_redirs(redir_t *r, salvos_t *s)
{
    for (; r; r = r->prox) {
        bool entrada = r->tipo == R_ENTRADA || r->tipo == R_DUP_ENTRADA || r->tipo == R_HEREDOC ||
                       r->tipo == R_HERESTRING || r->tipo == R_LER_ESCREVER;
        int fd = r->fd >= 0 ? r->fd : (entrada ? ENTRADA : SAIDA);
        if (fd >= 32) { erro("%d: descriptor erróneo", fd); return false; }
        if (r->tipo == R_HEREDOC || r->tipo == R_HERESTRING) {
            char *texto;
            if (r->tipo == R_HEREDOC) {
                texto = r->expandir ? expandir_heredoc(r->corpo ? r->corpo : "") : sh_dup(r->corpo ? r->corpo : "");
            } else {
                char *v = expandir_texto(r->alvo, true);
                texto_t t = {0};
                tx_junta(&t, v);
                tx_car(&t, '\n');
                libera(v);
                texto = tx_texto(&t);
            }
            if (erro_expansao) { libera(texto); return false; }
            if (!salvar_fd(s, fd)) { libera(texto); return false; }
            int novo = fd_com_texto(texto);
            libera(texto);
            if (novo < 0) { erro("here-doc: %s", nome_erro(novo)); return false; }
            colocar(s, novo, fd);
            continue;
        }
        char *alvo = expandir_texto(r->alvo, true);
        if (erro_expansao) { libera(alvo); return false; }
        if (r->tipo == R_DUP_ENTRADA || r->tipo == R_DUP_SAIDA) {
            if (compara_texto(alvo, "-") == 0) {
                if (!salvar_fd(s, fd)) { libera(alvo); return false; }
                fechar(fd);
                libera(alvo);
                continue;
            }
            if (!eh_numero(alvo) || alvo[0] == '-' || alvo[0] == '+') {
                if (r->tipo == R_DUP_SAIDA && r->fd < 0) {   /* >&archivo = &>archivo */
                    /* cae en el caso de archivo de abajo, como R_TUDO */
                } else {
                    erro("%s: redirección ambigua", alvo);
                    libera(alvo);
                    return false;
                }
            } else {
                int m = para_int(alvo);
                libera(alvo);
                if (m >= 32 || tipo_descritor(m) < 0) { erro("%d: descriptor erróneo", m); return false; }
                if (m == fd) continue;
                if (!salvar_fd(s, fd)) return false;
                duplicar(m, fd);
                continue;
            }
        }
        int tipo = r->tipo == R_DUP_SAIDA ? R_TUDO : r->tipo;
        int modo;
        switch (tipo) {
        case R_ENTRADA: modo = ABRIR_LER; break;
        case R_LER_ESCREVER: modo = ABRIR_LER | ABRIR_ESCREVER | ABRIR_CRIAR; break;
        case R_ACRESCENTA: case R_TUDO_ACRESCENTA: modo = ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_ACRESCENTAR; break;
        default: modo = ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR; break;
        }
        bool tudo = tipo == R_TUDO || tipo == R_TUDO_ACRESCENTA;
        if (!salvar_fd(s, fd) || (tudo && !salvar_fd(s, ERROS))) { libera(alvo); return false; }
        int novo;
        if (compara_texto(alvo, "/dev/null") == 0) novo = abrir_nulo(tipo != R_ENTRADA);
        else novo = abrir(alvo, modo);
        if (novo < 0) {
            erro("%s: %s", alvo, nome_erro(novo));
            libera(alvo);
            return false;
        }
        libera(alvo);
        if (tudo && novo != ERROS) duplicar(novo, ERROS);
        colocar(s, novo, fd);
    }
    return true;
}

/* ------------------------------------------------------------ programas */

char *achar_programa(const char *nome)
{
    if (acha_char(nome, '/')) return sh_dup(nome);
    const char *canon = nome_canonico(nome);
    const char *path = var_obter("PATH");
    if (!path) path = SAGUI_PATH_PADRAO;
    const char *p = path;
    for (;;) {
        const char *fim = p;
        while (*fim && *fim != ':') fim++;
        texto_t t = {0};
        if (fim == p) tx_car(&t, '.');
        else tx_junta_n(&t, p, (int)(fim - p));
        tx_car(&t, '/');
        tx_junta(&t, canon);
        info_arquivo_t inf;
        if (info_arquivo(tx_texto(&t), &inf) == 0 && inf.tipo != TIPO_PASTA) return t.s;
        tx_liberar(&t);
        if (!*fim) break;
        p = fim + 1;
    }
    return NULL;
}

int status_de_codigo(int codigo)
{
    if (codigo < 0) return 143;   /* terminado por el kernel o por kill */
    return codigo & 0xFF;
}

/* Un script (el kernel solo carga ELF): "#!intérprete [arg]" en la primera
 * línea; sin "#!", un archivo de texto se ejecuta con sagsh, como en sh. */
static int lancar_script(const char *caminho, char **argv, char **env)
{
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return fd;
    char linha[128];
    int n = ler(fd, linha, sizeof linha - 1);
    fechar(fd);
    if (n < 0) return n;
    linha[n] = 0;
    for (int i = 0; i < n; i++) if (linha[i] == '\n' || linha[i] == '\r') { linha[i] = 0; break; }
    for (int i = 0; i < n && linha[i]; i++)
        if ((unsigned char)linha[i] < 9) return ERRO_FORMATO;   /* binario que no es ELF */
    char interp[96] = "/bin/sagsh", *arg = NULL;
    if (linha[0] == '#' && linha[1] == '!') {
        char *p = linha + 2;
        while (*p == ' ' || *p == '\t') p++;
        char *f = p;
        while (*f && *f != ' ' && *f != '\t') f++;
        if (*f) { *f++ = 0; while (*f == ' ' || *f == '\t') f++; if (*f) arg = f; }
        copia_texto(interp, p, sizeof interp);
        const char *base = acha_ultimo_char(interp, '/') ? acha_ultimo_char(interp, '/') + 1 : interp;
        if (compara_texto(base, "sh") == 0 || compara_texto(base, "sagsh") == 0 || compara_texto(base, "bash") == 0)
            copia_texto(interp, "/bin/sagsh", sizeof interp);
        else if (compara_texto(base, "env") == 0 && arg) {   /* #!/usr/bin/env python */
            char *a = arg;
            while (*a && *a != ' ') a++;
            *a = 0;
            char *c = achar_programa(arg);
            if (!c) return ERRO_NAO_EXISTE;
            copia_texto(interp, c, sizeof interp);
            libera(c);
            arg = NULL;
        }
    }
    lista_t nv = {0};
    lista_poe(&nv, sh_dup(interp));
    if (arg) lista_poe(&nv, sh_dup(arg));
    lista_poe(&nv, sh_dup(caminho));
    for (int i = 1; argv[i]; i++) lista_poe(&nv, sh_dup(argv[i]));
    int pid = executar_env(interp, nv.v, env);
    lista_liberar(&nv);
    return pid;
}

/* Inicia un programa; devuelve el pid, o -código ($? 126/127) si falló. */
static int lancar(char **argv, lista_t *extras)
{
    char *caminho = achar_programa(argv[0]);
    if (!caminho) {
        erro("comando no encontrado: %s (escribe ajuda)", argv[0]);
        return -127;
    }
    char **env = ambiente(extras);
    int pid = executar_env(caminho, argv, env);
    if (pid == ERRO_FORMATO) pid = lancar_script(caminho, argv, env);
    if (pid < 0) {
        erro("%s: %s", argv[0], nome_erro(pid));
        libera(caminho);
        return pid == ERRO_NAO_EXISTE ? -127 : -126;
    }
    libera(caminho);
    return pid;
}

/* Un /bin/sagsh hijo que ejecuta "no" con el estado de este shell. */
static int lancar_filho(no_t *no, const char *texto_pronto)
{
    texto_t t = {0};
    serializar_estado(&t);
    if (texto_pronto) tx_junta(&t, texto_pronto);
    else desanalisar(no, &t, false);
    tx_car(&t, '\n');
    char *argv[6];
    char arq[64];
    int k = 0;
    argv[k++] = "sagsh";
    if (t.n < 12000) {
        argv[k++] = "-c";
        argv[k++] = t.s;
    } else {   /* demasiado grande para los argumentos: va por un archivo que el hijo borra */
        static int seq;
        formatar_texto(arq, sizeof arq, "/tmp/.sagsh-filho-%d-%d", sh.pid, seq++);
        int fd = abrir(arq, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fd < 0) { tx_liberar(&t); erro("%s: %s", arq, nome_erro(fd)); return -1; }
        escrever_tudo(fd, t.s, (unsigned)t.n);
        fechar(fd);
        argv[k++] = "--temporario";
        argv[k++] = arq;
    }
    argv[k++] = sh.nome0;
    argv[k] = NULL;
    int pid = executar_env("/bin/sagsh", argv, ambiente(NULL));
    tx_liberar(&t);
    if (pid < 0) erro("no he podido iniciar /bin/sagsh: %s", nome_erro(pid));
    return pid;
}

/* ------------------------------------------------------------ trabajos */

typedef struct {
    int   num;
    int   pids[MAX_PIDS];   /* negativo = ya terminó */
    int   npids, vivos;
    int   status;
    int   grupo;
    char *texto;
} trabalho_t;

static trabalho_t *trabs[MAX_TRAB];
static int ultimo_trab;   /* número del trabajo "+" */

static trabalho_t *novo_trabalho(int *pids, int n, const char *texto)
{
    int livre = -1, maior = 0;
    for (int i = 0; i < MAX_TRAB; i++) {
        if (!trabs[i]) { if (livre < 0) livre = i; }
        else if (trabs[i]->num > maior) maior = trabs[i]->num;
    }
    if (livre < 0) return NULL;
    trabalho_t *t = sh_aloca(sizeof *t);
    memset(t, 0, sizeof *t);
    t->num = maior + 1;
    for (int i = 0; i < n && i < MAX_PIDS; i++) t->pids[t->npids++] = pids[i];
    t->vivos = t->npids;
    t->grupo = pids[0];
    t->texto = sh_dup(texto);
    trabs[livre] = t;
    ultimo_trab = t->num;
    return t;
}

static void remover_trabalho(trabalho_t *t)
{
    for (int i = 0; i < MAX_TRAB; i++)
        if (trabs[i] == t) trabs[i] = NULL;
    libera(t->texto);
    libera(t);
}

static void anotar_fim(trabalho_t *t, int pid, int codigo)
{
    for (int k = 0; k < t->npids; k++)
        if (t->pids[k] == pid) {
            t->pids[k] = -pid;
            t->vivos--;
            if (k == t->npids - 1) t->status = status_de_codigo(codigo);
        }
}

static trabalho_t *trabalho_do_pid(int pid)
{
    for (int i = 0; i < MAX_TRAB; i++)
        if (trabs[i])
            for (int k = 0; k < trabs[i]->npids; k++)
                if (trabs[i]->pids[k] == pid) return trabs[i];
    return NULL;
}

static void avisar_fim(trabalho_t *t)
{
    const char *estado;
    char buf[24];
    if (t->status == 0) estado = "Hecho";
    else if (t->status == 143 || t->status == 130) estado = "Terminado";
    else { formatar_texto(buf, sizeof buf, "Salida %d", t->status); estado = buf; }
    imprimir_em(ERROS, "[%d]%c %-12s%s\n", t->num, t->num == ultimo_trab ? '+' : '-', estado, t->texto);
}

/* Recoge los hijos que terminaron. Llamar solo cuando no hay nada en
 * primer plano esperándose (si no, el esperar(pid) perdería el hijo). */
void recolher_trabalhos(bool avisar)
{
    int codigo, pid;
    while ((pid = recolher(&codigo)) > 0) {
        trabalho_t *t = trabalho_do_pid(pid);
        if (!t) continue;   /* programa gráfico abierto sin trabajo */
        anotar_fim(t, pid, codigo);
    }
    for (int i = 0; i < MAX_TRAB; i++) {
        trabalho_t *t = trabs[i];
        if (!t || t->vivos > 0) continue;
        if (avisar) avisar_fim(t);
        remover_trabalho(t);
    }
}

void listar_trabalhos(void)
{
    recolher_trabalhos(false);
    for (int i = 0; i < MAX_TRAB; i++) {
        trabalho_t *t = trabs[i];
        if (!t) continue;
        imprimir("[%d]%c %-12s%s &\n", t->num, t->num == ultimo_trab ? '+' : '-',
                 t->vivos ? "Ejecutando" : "Hecho", t->texto);
    }
}

static trabalho_t *achar_trabalho(const char *spec)
{
    if (!spec || compara_texto(spec, "%+") == 0 || compara_texto(spec, "%%") == 0 || compara_texto(spec, "%") == 0) {
        for (int i = 0; i < MAX_TRAB; i++)
            if (trabs[i] && trabs[i]->num == ultimo_trab) return trabs[i];
        trabalho_t *m = NULL;
        for (int i = 0; i < MAX_TRAB; i++)
            if (trabs[i] && (!m || trabs[i]->num > m->num)) m = trabs[i];
        return m;
    }
    if (spec[0] == '%') {
        int n = para_int(spec + 1);
        for (int i = 0; i < MAX_TRAB; i++)
            if (trabs[i] && trabs[i]->num == n) return trabs[i];
        return NULL;
    }
    return trabalho_do_pid(para_int(spec));
}

int pids_do_trabalho(const char *spec, int *pids, int max)
{
    trabalho_t *t = achar_trabalho(spec);
    if (!t) return -1;
    int n = 0;
    for (int k = 0; k < t->npids && n < max; k++)
        if (t->pids[k] > 0) pids[n++] = t->pids[k];
    return n;
}

static int esperar_trabalho(trabalho_t *t)
{
    for (int k = 0; k < t->npids; k++) {
        if (t->pids[k] <= 0) continue;
        int codigo = 0, pid = t->pids[k];
        if (esperar(pid, &codigo) == pid) anotar_fim(t, pid, codigo);
        else anotar_fim(t, pid, 127);
        if (checar_interrupcao()) return 130;
    }
    int st = t->status;
    remover_trabalho(t);
    return st;
}

int esperar_trabalhos(int argc, char **argv)
{
    if (argc < 2) {
        for (int i = 0; i < MAX_TRAB; i++)
            if (trabs[i]) {
                esperar_trabalho(trabs[i]);
                if (sh.fluxo != F_NORMAL) return 130;
            }
        return 0;
    }
    int st = 0;
    for (int i = 1; i < argc; i++) {
        trabalho_t *t = achar_trabalho(argv[i]);
        if (t) { st = esperar_trabalho(t); continue; }
        if (argv[i][0] != '%' && eh_numero(argv[i])) {   /* pid que no es trabajo (o ya recogido) */
            int codigo;
            st = esperar(para_int(argv[i]), &codigo) > 0 ? status_de_codigo(codigo) : 127;
            continue;
        }
        erro("wait: %s: no existe ese trabajo", argv[i]);
        st = 127;
    }
    return st;
}

int trazer_trabalho(int argc, char **argv)
{
    trabalho_t *t = achar_trabalho(argc > 1 ? argv[1] : NULL);
    if (!t) { erro("fg: %s: no existe ese trabajo", argc > 1 ? argv[1] : "actual"); return 1; }
    imprimir("%s\n", t->texto);
    int ant = 0;
    if (sh.interativo && sh.terminal) ant = primeiro_plano(t->grupo);
    int st = esperar_trabalho(t);
    if (sh.interativo && sh.terminal) primeiro_plano(ant);
    return st;
}

/* ------------------------------------------------------------ Ctrl+C */

bool checar_interrupcao(void)
{
    if (sh.fluxo == F_INTERROMPIDO) return true;
    if (!sh.interativo || !sh.terminal) return false;
    int k = interrupcoes();
    if (k != sh.base_ctrl_c) {
        sh.base_ctrl_c = k;
        sh.fluxo = F_INTERROMPIDO;
        sh.status = 130;
        return true;
    }
    return false;
}

/* Espera los procesos de un trabajo en primer plano. */
static int esperar_primeiro_plano(int *pids, int n, int *status_por_pid)
{
    int grupo = 0;
    for (int i = 0; i < n; i++) if (pids[i] > 0) { grupo = pids[i]; break; }
    bool plano = sh.interativo && sh.terminal && grupo > 0;
    int ant = plano ? primeiro_plano(grupo) : 0;
    for (int i = 0; i < n; i++) {
        if (pids[i] <= 0) continue;
        int codigo = 0;
        if (esperar(pids[i], &codigo) != pids[i]) codigo = 127;
        status_por_pid[i] = status_de_codigo(codigo);
    }
    if (plano) primeiro_plano(ant);
    return grupo;
}

static void agrupar(int *pids, int n)
{
    if (!sh.interativo) return;
    int lider = 0;
    for (int i = 0; i < n; i++) {
        if (pids[i] <= 0) continue;
        if (!lider) { lider = pids[i]; definir_grupo(lider, 0); }
        else definir_grupo(pids[i], lider);
    }
}

/* ------------------------------------------------------------ funciones */

static int chamar_funcao(funcao_t *f, char **argv, int argc)
{
    if (sh.funcoes >= MAX_FUNCOES) {
        erro("%s: demasiada recursión (máximo %d llamadas anidadas)", argv[0], MAX_FUNCOES);
        return 1;
    }
    lista_t novos = {0}, antigos;
    for (int i = 1; i < argc; i++) lista_poe(&novos, sh_dup(argv[i]));
    params_trocar(&novos, &antigos);
    int lacos = sh.lacos;
    sh.lacos = 0;
    sh.funcoes++;
    int st = executar_no(f->corpo);
    if (sh.fluxo == F_VOLTAR || sh.fluxo == F_PARAR || sh.fluxo == F_CONTINUAR) {
        st = sh.status;
        sh.fluxo = F_NORMAL;
    }
    locais_sair();
    sh.funcoes--;
    sh.lacos = lacos;
    lista_liberar(&params);
    params = antigos;
    sh.status = st;
    return st;
}

/* ------------------------------------------------------------ alias */

static const char *apelidos_ativos[16];
static int napelidos_ativos;

static bool palavra_literal(const char *w)
{
    for (; *w; w++)
        if (*w == '\'' || *w == '"' || *w == '\\' || *w == '$' || *w == '`') return false;
    return true;
}

static int executar_apelido(no_t *n, apelido_t *a)
{
    for (int i = 0; i < napelidos_ativos; i++)
        if (compara_texto(apelidos_ativos[i], a->nome) == 0) return -1;   /* "alias ls='ls -l'" */
    if (napelidos_ativos == 16) return -1;
    texto_t t = {0};
    for (int i = 0; i < n->natr; i++) { tx_junta(&t, n->atribs[i]); tx_car(&t, ' '); }
    tx_junta(&t, a->valor);
    for (int i = 1; i < n->npal; i++) { tx_car(&t, ' '); tx_junta(&t, n->palavras[i]); }
    if (n->redir) {
        no_t so_redir;
        memset(&so_redir, 0, sizeof so_redir);
        so_redir.tipo = N_SIMPLES;
        so_redir.redir = n->redir;
        tx_car(&t, ' ');
        desanalisar(&so_redir, &t, false);
    }
    apelidos_ativos[napelidos_ativos++] = a->nome;
    int st = executar_texto(tx_texto(&t), NULL);
    napelidos_ativos--;
    tx_liberar(&t);
    return st;
}

/* ------------------------------------------------------------ comando simple */

/* Expande las asignaciones "NOMBRE=valor" (y "NOMBRE+=valor"). */
static bool expandir_atribs(no_t *n, lista_t *atr)
{
    for (int i = 0; i < n->natr; i++) {
        const char *w = n->atribs[i];
        const char *igual = acha_char(w, '=');
        int tn = (int)(igual - w);
        bool mais = tn > 0 && w[tn - 1] == '+';
        char *nome = sh_dup_n(w, mais ? tn - 1 : tn);
        char *valor = expandir_texto(igual + 1, true);
        if (erro_expansao) { libera(nome); libera(valor); return false; }
        texto_t t = {0};
        tx_junta(&t, nome);
        tx_car(&t, '=');
        if (mais) { const char *v = var_obter(nome); if (v) tx_junta(&t, v); }
        tx_junta(&t, valor);
        lista_poe(atr, tx_texto(&t));
        libera(nome);
        libera(valor);
    }
    return true;
}

static bool definir_atrib(const char *a, int flags)
{
    const char *igual = acha_char(a, '=');
    char *nome = sh_dup_n(a, (int)(igual - a));
    bool ok = var_definir(nome, igual + 1, flags);
    libera(nome);
    return ok;
}

static void rastrear(lista_t *atr, lista_t *argv)
{
    texto_t t = {0};
    tx_car(&t, '+');
    for (int i = 0; i < atr->n; i++) { tx_car(&t, ' '); tx_junta(&t, atr->v[i]); }
    for (int i = 0; i < argv->n; i++) { tx_car(&t, ' '); tx_junta(&t, argv->v[i]); }
    tx_car(&t, '\n');
    escrever_tudo(ERROS, t.s, (unsigned)t.n);
    tx_liberar(&t);
}

typedef struct { char *nome, *valor; int flags; bool existia; } temp_t;

/* Ejecuta un comando simple ya expandido. "sozinho": fuera de pipe (un programa
 * gráfico pasa a segundo plano sin retener el shell). */
static int rodar_simples(no_t *n, lista_t *argv, lista_t *atr, bool sozinho)
{
    salvos_t s = { NULL, 0 };
    if (argv->n == 0) {
        int st = sh.status_subst >= 0 ? sh.status_subst : 0;   /* de una $( ) en las asignaciones, o 0 */
        for (int i = 0; i < atr->n; i++)
            if (!definir_atrib(atr->v[i], sh.opt_a ? V_EXPORTADA : 0)) st = 1;
        if (!aplicar_redirs(n->redir, &s)) st = 1;
        desfazer(&s);
        return st;
    }
    if (sh.opt_x) rastrear(atr, argv);
    char **v = argv->v;
    int argc = argv->n;
    bool sem_funcao = false;
    while (compara_texto(v[0], "command") == 0 && argc > 1 && v[1][0] != '-') {
        v++; argc--; sem_funcao = true;
    }
    if (compara_texto(v[0], "builtin") == 0 && argc > 1) { v++; argc--; sem_funcao = true; }

    /* exec: solo redirecciones (quedan vigentes) o cambia el shell por el programa */
    if (compara_texto(v[0], "exec") == 0) {
        if (!aplicar_redirs(n->redir, &s)) { desfazer(&s); return 1; }
        if (argc == 1) { esquecer(&s); return 0; }
        char **vv = v + 1;
        int pid = lancar(vv, atr);
        desfazer(&s);
        if (pid < 0) sair_do_shell(-pid);
        agrupar(&pid, 1);
        int st;
        esperar_primeiro_plano(&pid, 1, &st);
        sair_do_shell(st);
    }

    funcao_t *f = sem_funcao ? NULL : funcao_achar(v[0]);
    interno_f bi = f ? NULL : interno_achar(v[0]);
    if (f || bi) {
        bool especial = bi && interno_especial(v[0]);
        temp_t *temps = sh_aloca((size_t)(atr->n + 1) * sizeof(temp_t));
        int nt = 0;
        for (int i = 0; i < atr->n; i++) {
            if (!especial) {
                const char *igual = acha_char(atr->v[i], '=');
                temps[nt].nome = sh_dup_n(atr->v[i], (int)(igual - atr->v[i]));
                var_t *vv = var_achar(temps[nt].nome);
                temps[nt].existia = vv != NULL;
                temps[nt].valor = vv ? sh_dup(vv->valor) : NULL;
                temps[nt].flags = vv ? vv->flags : 0;
                nt++;
            }
            definir_atrib(atr->v[i], especial ? 0 : V_EXPORTADA);
        }
        int st;
        if (!aplicar_redirs(n->redir, &s)) st = 1;
        else st = f ? chamar_funcao(f, v, argc) : bi(argc, v);
        desfazer(&s);
        for (int i = nt - 1; i >= 0; i--) {
            var_t *vv = var_achar(temps[i].nome);
            if (temps[i].existia) {
                if (!vv) { var_definir(temps[i].nome, temps[i].valor, 0); vv = var_achar(temps[i].nome); }
                else { libera(vv->valor); vv->valor = sh_dup(temps[i].valor); }
                vv->flags = temps[i].flags;
            } else if (vv) {
                vv->flags &= ~V_SO_LEITURA;
                var_remover(temps[i].nome);
            }
            libera(temps[i].nome);
            libera(temps[i].valor);
        }
        libera(temps);
        return st;
    }

    if (!aplicar_redirs(n->redir, &s)) { desfazer(&s); return 1; }
    int pid = lancar(v, atr);
    desfazer(&s);
    if (pid < 0) return -pid;
    const comando_t *c = achar_comando(v[0]);
    /* Algunas opciones trabajan sin ventana y el shell las espera: las del midi
     * (--a-wav, --info, --medir: D190) y la ayuda o la versión de cualquiera.
     * Otras opciones largas sí abren la ventana (sagui3d --medir, h40). */
    bool opcion = false;
    if (v[1] && v[1][0] == '-' && v[1][1] == '-') {
        const char *base = v[0], *b;
        for (b = v[0]; *b; b++) if (*b == '/') base = b + 1;
        opcion = tam_texto(base) == 4 && base[0] == 'm' && base[1] == 'i' && base[2] == 'd' && base[3] == 'i';
        if (!opcion) {
            static const char *const sem_janela[] = { "--ayuda", "--help", "--version" };
            for (unsigned k = 0; k < sizeof sem_janela / sizeof sem_janela[0]; k++) {
                const char *x = sem_janela[k], *y = v[1];
                while (*x && *x == *y) { x++; y++; }
                if (!*x && !*y) opcion = true;
            }
        }
    }
    if (sozinho && c && c->grafico && !opcion) {   /* abre una ventana: no retiene el shell */
        if (sh.interativo) definir_grupo(pid, 0);
        imprimir("Abriendo %s (proceso %d)...\n", v[0], pid);
        return 0;
    }
    agrupar(&pid, 1);
    int st;
    esperar_primeiro_plano(&pid, 1, &st);
    return st;
}

static int executar_simples(no_t *n)
{
    sh.linha = n->linha;
    if (n->npal > 0 && palavra_literal(n->palavras[0])) {
        apelido_t *a = apelido_achar(n->palavras[0]);
        if (a) {
            int st = executar_apelido(n, a);
            if (st >= 0) return st;
        }
    }
    lista_t argv = {0}, atr = {0};
    sh.status_subst = -1;
    if (!expandir_palavras(n->palavras, n->npal, &argv) || !expandir_atribs(n, &atr)) {
        lista_liberar(&argv);
        lista_liberar(&atr);
        sh.status = 1;
        if (!sh.interativo) sh.fluxo = F_SAIR;
        return 1;
    }
    int st = rodar_simples(n, &argv, &atr, true);
    lista_liberar(&argv);
    lista_liberar(&atr);
    return st;
}

/* ------------------------------------------------------------ pipes */

typedef struct {
    no_t   *no;
    lista_t argv, atr;
    bool    externo, falhou, expandida;
} etapa_t;

static bool eh_externo(const char *nome)
{
    return !funcao_achar(nome) && !interno_achar(nome) && !apelido_achar(nome) &&
           compara_texto(nome, "exec") != 0 && compara_texto(nome, "command") != 0 &&
           compara_texto(nome, "builtin") != 0;
}

/* Texto de una etapa simple ya expandida, para un sagsh hijo. */
static char *texto_expandido(etapa_t *e)
{
    texto_t t = {0};
    for (int i = 0; i < e->atr.n; i++) {
        const char *igual = acha_char(e->atr.v[i], '=');
        tx_junta_n(&t, e->atr.v[i], (int)(igual - e->atr.v[i]) + 1);
        aspas_simples(&t, igual + 1);
        tx_car(&t, ' ');
    }
    for (int i = 0; i < e->argv.n; i++) {
        if (i) tx_car(&t, ' ');
        aspas_simples(&t, e->argv.v[i]);
    }
    if (e->no->redir) {
        no_t so;
        memset(&so, 0, sizeof so);
        so.tipo = N_SIMPLES;
        so.redir = e->no->redir;
        desanalisar(&so, &t, false);
    }
    return tx_texto(&t);
}

static int rodar_pipeline(no_t **etapas, int k, bool fundo, const char *texto_trab)
{
    etapa_t *et = sh_aloca((size_t)k * sizeof(etapa_t));
    memset(et, 0, (size_t)k * sizeof(etapa_t));
    for (int i = 0; i < k; i++) {
        et[i].no = etapas[i];
        if (etapas[i]->tipo != N_SIMPLES) continue;
        no_t *n = etapas[i];
        if (n->npal > 0 && palavra_literal(n->palavras[0]) && apelido_achar(n->palavras[0])) continue;
        if (!expandir_palavras(n->palavras, n->npal, &et[i].argv) || !expandir_atribs(n, &et[i].atr)) {
            et[i].falhou = true;
            continue;
        }
        et[i].expandida = true;
        et[i].externo = et[i].argv.n > 0 && eh_externo(et[i].argv.v[0]);
    }
    /* Qué etapa interna se ejecuta en el propio shell. */
    int ip = -1;
    if (!fundo) {
        if (!et[k - 1].externo && !et[k - 1].falhou) ip = k - 1;
        else for (int i = 0; i < k; i++) if (!et[i].externo && !et[i].falhou) { ip = i; break; }
    }
    int *pids = sh_aloca((size_t)k * sizeof(int));
    int *sts = sh_aloca((size_t)k * sizeof(int));
    int g0 = fd_livre();
    duplicar(ENTRADA, g0);
    int g1 = fd_livre();
    duplicar(SAIDA, g1);
    int leitura = -1, in0 = -1, in1 = -1;
    bool ok = true;
    for (int i = 0; i < k; i++) {
        pids[i] = 0;
        sts[i] = et[i].falhou ? 1 : 0;
        int p[2] = { -1, -1 };
        if (i < k - 1 && criar_pipe(p) < 0) { erro("no he podido crear el pipe"); ok = false; k = i; break; }
        if (i == 0) duplicar(g0, ENTRADA);
        else { duplicar(leitura, ENTRADA); fechar(leitura); leitura = -1; }
        if (i < k - 1) { duplicar(p[1], SAIDA); fechar(p[1]); leitura = p[0]; }
        else duplicar(g1, SAIDA);
        if (et[i].falhou) continue;
        if (i == ip) {
            in0 = fd_livre(); duplicar(ENTRADA, in0);
            in1 = fd_livre(); duplicar(SAIDA, in1);
            continue;
        }
        if (et[i].externo) {
            salvos_t s = { NULL, 0 };
            if (aplicar_redirs(et[i].no->redir, &s)) {
                int pid = lancar(et[i].argv.v, &et[i].atr);
                if (pid < 0) sts[i] = -pid;
                else pids[i] = pid;
            } else {
                sts[i] = 1;
            }
            desfazer(&s);
        } else {
            char *txt = et[i].expandida ? texto_expandido(&et[i]) : NULL;
            int pid = lancar_filho(et[i].no, txt);
            libera(txt);
            if (pid < 0) sts[i] = 126;
            else pids[i] = pid;
        }
    }
    if (leitura >= 0) fechar(leitura);
    duplicar(g0, ENTRADA);
    duplicar(g1, SAIDA);
    agrupar(pids, k);

    int st = 0;
    if (fundo) {
        int vivos[MAX_PIDS], nv = 0;
        for (int i = 0; i < k && nv < MAX_PIDS; i++) if (pids[i] > 0) vivos[nv++] = pids[i];
        if (nv) {
            sh.ultimo_fundo = vivos[nv - 1];
            const comando_t *c = (k == 1 && et[0].externo) ? achar_comando(et[0].argv.v[0]) : NULL;
            if (c && c->grafico) {
                imprimir("Abriendo %s (proceso %d)...\n", et[0].argv.v[0], vivos[0]);
            } else {
                trabalho_t *t = novo_trabalho(vivos, nv, texto_trab);
                if (t && sh.interativo) imprimir_em(ERROS, "[%d] %d\n", t->num, vivos[nv - 1]);
            }
        }
    } else {
        int grupo = 0;
        for (int i = 0; i < k; i++) if (pids[i] > 0) { grupo = pids[i]; break; }
        bool plano = sh.interativo && sh.terminal && grupo > 0;
        int ant = plano ? primeiro_plano(grupo) : 0;
        if (ip >= 0 && ok) {
            duplicar(in0, ENTRADA); fechar(in0);
            duplicar(in1, SAIDA); fechar(in1);
            sh.em_estagio++;
            if (et[ip].expandida) sts[ip] = rodar_simples(et[ip].no, &et[ip].argv, &et[ip].atr, false);
            else sts[ip] = executar_no(et[ip].no);
            sh.em_estagio--;
            if (sh.fluxo == F_PIPE || sh.fluxo == F_SAIR) { sts[ip] = sh.status; sh.fluxo = F_NORMAL; }
            duplicar(g0, ENTRADA);
            duplicar(g1, SAIDA);
        } else {
            if (in0 >= 0) fechar(in0);
            if (in1 >= 0) fechar(in1);
        }
        int *sp = sh_aloca((size_t)k * sizeof(int));
        for (int i = 0; i < k; i++) sp[i] = sts[i];
        for (int i = 0; i < k; i++) {
            if (pids[i] <= 0) continue;
            int codigo = 0;
            if (esperar(pids[i], &codigo) != pids[i]) codigo = 127;
            sp[i] = status_de_codigo(codigo);
        }
        if (plano) primeiro_plano(ant);
        st = sp[k - 1];
        libera(sp);
    }
    fechar(g0);
    fechar(g1);
    for (int i = 0; i < k; i++) { lista_liberar(&et[i].argv); lista_liberar(&et[i].atr); }
    libera(et);
    libera(pids);
    libera(sts);
    return st;
}

static int executar_pipe(no_t *n)
{
    int st;
    if (n->nfilhos == 1) st = executar_no(n->filhos[0]);
    else {
        char *txt = texto_do_no(n, true);
        st = rodar_pipeline(n->filhos, n->nfilhos, false, txt);
        libera(txt);
    }
    if (n->negar) st = !st;
    return st;
}

static int executar_fundo(no_t *n)
{
    char *txt = texto_do_no(n, true);
    if (n->tipo == N_SIMPLES || (n->tipo == N_PIPE && !n->negar)) {
        no_t *um[1] = { n };
        if (n->tipo == N_SIMPLES) rodar_pipeline(um, 1, true, txt);
        else rodar_pipeline(n->filhos, n->nfilhos, true, txt);
    } else {
        int pid = lancar_filho(n, NULL);
        if (pid > 0) {
            if (sh.interativo) definir_grupo(pid, 0);
            sh.ultimo_fundo = pid;
            trabalho_t *t = novo_trabalho(&pid, 1, txt);
            if (t && sh.interativo) imprimir_em(ERROS, "[%d] %d\n", t->num, pid);
        }
    }
    libera(txt);
    return 0;
}

/* ------------------------------------------------------------ sub-shell */

static int subshell(no_t *corpo, const char *texto)
{
    estado_salvo_t *e = estado_salvar();
    int lacos = sh.lacos;
    sh.lacos = 0;
    sh.subshell++;
    int st = corpo ? executar_no(corpo) : executar_texto(texto, NULL);
    if (sh.fluxo != F_NORMAL && sh.fluxo != F_INTERROMPIDO && sh.fluxo != F_ABORTAR) {
        st = sh.status;
        sh.fluxo = F_NORMAL;
    }
    sh.subshell--;
    sh.lacos = lacos;
    estado_restaurar(e);
    sh.status = st;
    return st;
}

int executar_subshell_texto(const char *texto)
{
    int st = subshell(NULL, texto);
    sh.status_subst = st;
    return st;
}

/* ------------------------------------------------------------ bucles */

/* Después del cuerpo de un bucle: true = salir del bucle. */
static bool fim_de_volta(void)
{
    if (sh.fluxo == F_PARAR) {
        if (--sh.nivel > 0) return true;   /* break 2: sigue saliendo */
        sh.fluxo = F_NORMAL;
        return true;
    }
    if (sh.fluxo == F_CONTINUAR) {
        if (--sh.nivel > 0) return true;   /* continue 2: el bucle de afuera continúa */
        sh.fluxo = F_NORMAL;
        return false;
    }
    if (sh.fluxo != F_NORMAL) return true;
    return checar_interrupcao();
}

static int executar_composto(no_t *n)
{
    int st = 0;
    switch (n->tipo) {
    case N_SUBSHELL:
        st = subshell(n->a, NULL);
        break;
    case N_GRUPO:
        st = executar_no(n->a);
        break;
    case N_SE: {
        sh.condicao++;
        int c = executar_no(n->a);
        sh.condicao--;
        if (sh.fluxo != F_NORMAL) return sh.status;
        if (c == 0) st = executar_no(n->b);
        else if (n->c) st = executar_no(n->c);
        else st = 0;
        break;
    }
    case N_ENQUANTO:
    case N_ATE:
        sh.lacos++;
        for (;;) {
            sh.condicao++;
            int c = executar_no(n->a);
            sh.condicao--;
            if (sh.fluxo != F_NORMAL) { if (fim_de_volta()) break; continue; }
            if ((c == 0) != (n->tipo == N_ENQUANTO)) break;
            st = executar_no(n->b);
            if (fim_de_volta()) break;
        }
        sh.lacos--;
        break;
    case N_PARA: {
        lista_t itens = {0};
        if (n->com_lista) {
            if (!expandir_palavras(n->palavras, n->npal, &itens)) { lista_liberar(&itens); return 1; }
        } else {
            for (int i = 0; i < params.n; i++) lista_poe(&itens, sh_dup(params.v[i]));
        }
        sh.lacos++;
        for (int i = 0; i < itens.n; i++) {
            if (!var_definir(n->nome, itens.v[i], 0)) { st = 1; break; }
            st = executar_no(n->b);
            if (fim_de_volta()) break;
        }
        sh.lacos--;
        lista_liberar(&itens);
        break;
    }
    case N_CASO: {
        char *w = expandir_texto(n->nome, true);
        st = 0;
        bool achou = false;
        for (item_caso_t *it = n->itens; it && !achou; it = it->prox)
            for (int i = 0; i < it->npadroes && !achou; i++) {
                char *pad = expandir_padrao(it->padroes[i]);
                if (casar(pad, w)) {
                    achou = true;
                    st = executar_no(it->corpo);
                }
                libera(pad);
            }
        libera(w);
        break;
    }
    }
    return st;
}

/* La pila del programa tiene 64 KB: recursión demasiado profunda (función que se llama
 * a sí misma sin fin, $( ) dentro de $( )...) se detiene con un error en vez de
 * tumbar el shell. */
static bool pilha_esgotada(void)
{
    char *aqui = __builtin_frame_address(0);
    int usado = (int)(sh.pilha_base - aqui);
    if (!sh.pilha_base || usado < LIMITE_PILHA) return false;
    erro("demasiada recursión: se ha agotado la pila");
    sh.status = 2;
    sh.fluxo = F_ABORTAR;
    return true;
}

int executar_no(no_t *n)
{
    if (!n) return 0;
    if (sh.fluxo != F_NORMAL) return sh.status;
    if (pilha_esgotada()) return sh.status;
    if (checar_interrupcao()) return sh.status;
    int st = 0;
    switch (n->tipo) {
    case N_SIMPLES:
        st = executar_simples(n);
        break;
    case N_PIPE:
        st = executar_pipe(n);
        break;
    case N_E:
    case N_OU:
        sh.condicao++;
        st = executar_no(n->a);
        sh.condicao--;
        if (sh.fluxo != F_NORMAL) return sh.status;
        if ((st == 0) == (n->tipo == N_E)) st = executar_no(n->b);
        break;
    case N_LISTA:
        for (int i = 0; i < n->nfilhos; i++) {
            if (n->fundo[i]) st = executar_fundo(n->filhos[i]);
            else st = executar_no(n->filhos[i]);
            sh.status = st;
            if (sh.fluxo != F_NORMAL || checar_interrupcao()) return sh.status;
        }
        break;
    case N_FUNCAO:
        funcao_definir(n->nome, n->a);
        n->arena->fixa = 1;
        st = 0;
        break;
    default: {
        salvos_t s = { NULL, 0 };
        if (!aplicar_redirs(n->redir, &s)) st = 1;
        else st = executar_composto(n);
        desfazer(&s);
        break;
    }
    }
    if (sh.fluxo == F_NORMAL || sh.fluxo == F_PIPE) sh.status = st;
    else if (sh.fluxo == F_SAIR || sh.fluxo == F_VOLTAR || sh.fluxo == F_INTERROMPIDO || sh.fluxo == F_ABORTAR) st = sh.status;
    if (sh.opt_e && st != 0 && !sh.condicao && sh.fluxo == F_NORMAL &&
        (n->tipo == N_SIMPLES || (n->tipo == N_PIPE && !n->negar) || n->tipo == N_SUBSHELL))
        sh.fluxo = F_SAIR;
    return st;
}

/* ------------------------------------------------------------ texto */

void relatar_sintaxe(resultado_t *r)
{
    sh.linha = r->linha_erro;
    if (r->perto[0]) erro("error de sintaxis cerca de '%s'", r->perto);
    else erro("error de sintaxis: el texto termina antes de tiempo (¿falta cerrar comillas, fi, done, esac o '}'?)");
}

int executar_texto(const char *texto, const char *nome)
{
    /* eval y $( ): los números de línea siguen los del script */
    int pos = 0, linha = nome ? 1 : (sh.linha > 0 ? sh.linha : 1), st = 0;
    for (;;) {
        arena_t *ar = arena_nova();
        resultado_t r = analisar(ar, texto, pos, linha, true);
        if (r.estado == A_VAZIO) { arena_liberar(ar); break; }
        if (r.estado != A_OK) {
            relatar_sintaxe(&r);
            arena_liberar(ar);
            st = 2;
            sh.status = 2;
            break;
        }
        for (int i = pos; i < r.fim; i++) if (texto[i] == '\n') linha++;
        pos = r.fim;
        st = executar_no(r.no);
        arena_liberar(ar);
        if (sh.fluxo != F_NORMAL) break;
    }
    return st;
}
