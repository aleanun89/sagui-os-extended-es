// SPDX-License-Identifier: GPL-3.0-only
/* variaveis.c - variables, entorno, parámetros, funciones y alias.
 *
 * Las tablas son vectores simples con búsqueda lineal: un shell tiene decenas de
 * variables, no miles. Las exportadas forman el entorno que executar()
 * entrega a los programas (D037). "local" guarda el valor antiguo en una pila,
 * restaurada cuando la función vuelve (alcance dinámico, como en bash). */
#include "sagsh.h"

static var_t     *vars;
static int        nvars, cap_vars;
static funcao_t  *funcs;
static int        nfuncs, cap_funcs;
static apelido_t *apelidos;
static int        napelidos, cap_apelidos;
lista_t           params;

typedef struct local {
    char *nome, *valor;
    int   flags, nivel;
    bool  existia;
    struct local *prox;
} local_t;
static local_t *locais;

/* ------------------------------------------------------------ variables */

var_t *var_achar(const char *nome)
{
    for (int i = 0; i < nvars; i++)
        if (compara_texto(vars[i].nome, nome) == 0) return &vars[i];
    return NULL;
}

const char *var_obter(const char *nome)
{
    var_t *v = var_achar(nome);
    return v ? v->valor : NULL;
}

static var_t *var_nova(const char *nome)
{
    if (nvars == cap_vars) {
        cap_vars = cap_vars ? cap_vars * 2 : 32;
        vars = sh_realoca(vars, (size_t)cap_vars * sizeof(var_t));
    }
    var_t *v = &vars[nvars++];
    v->nome = sh_dup(nome);
    v->valor = sh_dup("");
    v->flags = 0;
    return v;
}

bool var_definir(const char *nome, const char *valor, int flags_mais)
{
    var_t *v = var_achar(nome);
    if (v && (v->flags & V_SO_LEITURA)) {
        erro("%s: variable de solo lectura", nome);
        return false;
    }
    if (!v) v = var_nova(nome);
    if (valor) {
        char *novo = sh_dup(valor);
        libera(v->valor);
        v->valor = novo;
    }
    v->flags |= flags_mais;
    return true;
}

void var_remover(const char *nome)
{
    for (int i = 0; i < nvars; i++)
        if (compara_texto(vars[i].nome, nome) == 0) {
            if (vars[i].flags & V_SO_LEITURA) { erro("%s: variable de solo lectura", nome); return; }
            libera(vars[i].nome);
            libera(vars[i].valor);
            vars[i] = vars[--nvars];
            return;
        }
}

void var_exportar(const char *nome)
{
    var_t *v = var_achar(nome);
    if (!v) v = var_nova(nome);
    v->flags |= V_EXPORTADA;
}

static int compara_var(const void *a, const void *b)
{
    return compara_texto(((const var_t *)a)->nome, ((const var_t *)b)->nome);
}

void var_listar(bool so_exportadas)
{
    var_t **ord = sh_aloca((size_t)(nvars + 1) * sizeof(var_t *));
    int n = 0;
    for (int i = 0; i < nvars; i++)
        if (!so_exportadas || (vars[i].flags & V_EXPORTADA)) ord[n++] = &vars[i];
    for (int i = 1; i < n; i++) {   /* inserción */
        var_t *x = ord[i];
        int j = i - 1;
        while (j >= 0 && compara_var(ord[j], x) > 0) { ord[j + 1] = ord[j]; j--; }
        ord[j + 1] = x;
    }
    for (int i = 0; i < n; i++) {
        texto_t t = {0};
        if (so_exportadas) tx_junta(&t, "export ");
        tx_junta(&t, ord[i]->nome);
        tx_car(&t, '=');
        aspas_simples(&t, ord[i]->valor);
        tx_car(&t, '\n');
        escrever_saida(SAIDA, t.s, t.n);
        tx_liberar(&t);
    }
    libera(ord);
}

/* El entorno para un programa: las exportadas, más los "NOMBRE=valor" de
 * "extras" (asignaciones antes del comando), que tienen prioridad. El vector vale
 * hasta la siguiente llamada. */
char **ambiente(lista_t *extras)
{
    static lista_t env;
    lista_liberar(&env);
    for (int i = 0; i < nvars; i++) {
        if (!(vars[i].flags & V_EXPORTADA)) continue;
        bool sobrescrita = false;
        size_t tn = tam_texto(vars[i].nome);
        for (int k = 0; extras && k < extras->n; k++)
            if (compara_texto_n(extras->v[k], vars[i].nome, tn) == 0 && extras->v[k][tn] == '=') sobrescrita = true;
        if (sobrescrita) continue;
        texto_t t = {0};
        tx_junta(&t, vars[i].nome);
        tx_car(&t, '=');
        tx_junta(&t, vars[i].valor);
        lista_poe(&env, tx_texto(&t));
    }
    for (int k = 0; extras && k < extras->n; k++) lista_poe(&env, sh_dup(extras->v[k]));
    if (!env.v) {   /* ninguna exportada: vector vacío */
        env.v = sh_aloca(2 * sizeof(char *));
        env.v[0] = NULL;
        env.cap = 2;
    }
    return env.v;
}

/* ------------------------------------------------------------ local */

void var_local(const char *nome)
{
    for (local_t *l = locais; l && l->nivel == sh.funcoes; l = l->prox)
        if (compara_texto(l->nome, nome) == 0) return;   /* ya es local en este nivel */
    local_t *l = sh_aloca(sizeof *l);
    var_t *v = var_achar(nome);
    l->nome = sh_dup(nome);
    l->existia = v != NULL;
    l->valor = v ? sh_dup(v->valor) : NULL;
    l->flags = v ? v->flags : 0;
    l->nivel = sh.funcoes;
    l->prox = locais;
    locais = l;
}

static void restaurar_local(local_t *l)
{
    if (l->existia) {
        var_t *v = var_achar(l->nome);
        if (!v) v = var_nova(l->nome);
        libera(v->valor);
        v->valor = l->valor;
        v->flags = l->flags;
        l->valor = NULL;
    } else {
        var_t *v = var_achar(l->nome);
        if (v) v->flags &= ~V_SO_LEITURA;
        var_remover(l->nome);
    }
    libera(l->nome);
    libera(l->valor);
    libera(l);
}

void locais_entrar(void) { }

void locais_sair(void)
{
    while (locais && locais->nivel >= sh.funcoes) {
        local_t *l = locais;
        locais = l->prox;
        restaurar_local(l);
    }
}

/* ------------------------------------------------------------ funciones */

funcao_t *funcao_achar(const char *nome)
{
    for (int i = 0; i < nfuncs; i++)
        if (compara_texto(funcs[i].nome, nome) == 0) return &funcs[i];
    return NULL;
}

void funcao_definir(const char *nome, no_t *corpo)
{
    funcao_t *f = funcao_achar(nome);
    if (!f) {
        if (nfuncs == cap_funcs) {
            cap_funcs = cap_funcs ? cap_funcs * 2 : 16;
            funcs = sh_realoca(funcs, (size_t)cap_funcs * sizeof(funcao_t));
        }
        f = &funcs[nfuncs++];
        f->nome = sh_dup(nome);
    }
    f->corpo = corpo;   /* la arena del árbol quedó fija: el árbol vive mientras el shell */
}

void funcao_remover(const char *nome)
{
    for (int i = 0; i < nfuncs; i++)
        if (compara_texto(funcs[i].nome, nome) == 0) {
            libera(funcs[i].nome);
            funcs[i] = funcs[--nfuncs];
            return;
        }
}

/* ------------------------------------------------------------ alias */

apelido_t *apelido_achar(const char *nome)
{
    for (int i = 0; i < napelidos; i++)
        if (compara_texto(apelidos[i].nome, nome) == 0) return &apelidos[i];
    return NULL;
}

void apelido_definir(const char *nome, const char *valor)
{
    apelido_t *a = apelido_achar(nome);
    if (!a) {
        if (napelidos == cap_apelidos) {
            cap_apelidos = cap_apelidos ? cap_apelidos * 2 : 16;
            apelidos = sh_realoca(apelidos, (size_t)cap_apelidos * sizeof(apelido_t));
        }
        a = &apelidos[napelidos++];
        a->nome = sh_dup(nome);
        a->valor = NULL;
    }
    libera(a->valor);
    a->valor = sh_dup(valor);
}

bool apelido_remover(const char *nome)
{
    for (int i = 0; i < napelidos; i++)
        if (compara_texto(apelidos[i].nome, nome) == 0) {
            libera(apelidos[i].nome);
            libera(apelidos[i].valor);
            apelidos[i] = apelidos[--napelidos];
            return true;
        }
    return false;
}

void apelidos_limpar(void)
{
    while (napelidos) apelido_remover(apelidos[0].nome);
}

void apelidos_listar(void)
{
    for (int i = 0; i < napelidos; i++) {
        texto_t t = {0};
        tx_formatar(&t, "alias %s=", apelidos[i].nome);
        aspas_simples(&t, apelidos[i].valor);
        tx_car(&t, '\n');
        escrever_saida(SAIDA, t.s, t.n);
        tx_liberar(&t);
    }
}

/* ------------------------------------------------------------ parámetros */

void params_definir(char **v, int n)
{
    lista_liberar(&params);
    for (int i = 0; i < n; i++) lista_poe(&params, sh_dup(v[i]));
}

void params_trocar(lista_t *novos, lista_t *antigos)
{
    *antigos = params;
    params = *novos;
}

/* ------------------------------------------------------------ estado entero */

struct estado_salvo {
    var_t     *vars;      int nvars;
    funcao_t  *funcs;     int nfuncs;
    apelido_t *apelidos;  int napelidos;
    lista_t    params;
    local_t   *locais;
    bool       e, u, x, f;
    char       pasta[256];
};

estado_salvo_t *estado_salvar(void)
{
    estado_salvo_t *e = sh_aloca(sizeof *e);
    e->vars = sh_aloca((size_t)(nvars + 1) * sizeof(var_t));
    for (int i = 0; i < nvars; i++) {
        e->vars[i].nome = sh_dup(vars[i].nome);
        e->vars[i].valor = sh_dup(vars[i].valor);
        e->vars[i].flags = vars[i].flags;
    }
    e->nvars = nvars;
    e->funcs = sh_aloca((size_t)(nfuncs + 1) * sizeof(funcao_t));
    for (int i = 0; i < nfuncs; i++) {
        e->funcs[i].nome = sh_dup(funcs[i].nome);
        e->funcs[i].corpo = funcs[i].corpo;
    }
    e->nfuncs = nfuncs;
    e->apelidos = sh_aloca((size_t)(napelidos + 1) * sizeof(apelido_t));
    for (int i = 0; i < napelidos; i++) {
        e->apelidos[i].nome = sh_dup(apelidos[i].nome);
        e->apelidos[i].valor = sh_dup(apelidos[i].valor);
    }
    e->napelidos = napelidos;
    memset(&e->params, 0, sizeof e->params);
    for (int i = 0; i < params.n; i++) lista_poe(&e->params, sh_dup(params.v[i]));
    e->locais = locais;
    e->e = sh.opt_e; e->u = sh.opt_u; e->x = sh.opt_x; e->f = sh.opt_f;
    if (pasta_atual(e->pasta, sizeof e->pasta) < 0) e->pasta[0] = 0;
    return e;
}

void estado_restaurar(estado_salvo_t *e)
{
    while (locais && locais != e->locais) {   /* "local" hechos dentro del sub-shell */
        local_t *l = locais;
        locais = l->prox;
        libera(l->nome); libera(l->valor); libera(l);
    }
    for (int i = 0; i < nvars; i++) { libera(vars[i].nome); libera(vars[i].valor); }
    libera(vars);
    vars = e->vars; nvars = cap_vars = e->nvars;
    if (!cap_vars) { libera(vars); vars = NULL; }
    for (int i = 0; i < nfuncs; i++) libera(funcs[i].nome);
    libera(funcs);
    funcs = e->funcs; nfuncs = cap_funcs = e->nfuncs;
    if (!cap_funcs) { libera(funcs); funcs = NULL; }
    for (int i = 0; i < napelidos; i++) { libera(apelidos[i].nome); libera(apelidos[i].valor); }
    libera(apelidos);
    apelidos = e->apelidos; napelidos = cap_apelidos = e->napelidos;
    if (!cap_apelidos) { libera(apelidos); apelidos = NULL; }
    lista_liberar(&params);
    params = e->params;
    sh.opt_e = e->e; sh.opt_u = e->u; sh.opt_x = e->x; sh.opt_f = e->f;
    char agora[256];
    if (e->pasta[0] && (pasta_atual(agora, sizeof agora) < 0 || compara_texto(agora, e->pasta) != 0))
        mudar_pasta(e->pasta);
    libera(e);
}

static bool var_especial_interna(const char *nome)
{
    return compara_texto(nome, "SAGSH_NIVEL") == 0;
}

/* Preludio para un sagsh hijo que ejecuta parte de este shell (etapa de pipe o
 * bloque en segundo plano): variables no exportadas (las exportadas ya van
 * en el entorno), funciones, alias, parámetros y el $$/$?/$! de este shell. */
void serializar_estado(texto_t *t)
{
    tx_formatar(t, ".sagsh_estado %d %d %d %s%s%s%s\n", sh.pid, sh.status, sh.ultimo_fundo,
                sh.opt_e ? "e" : "-", sh.opt_u ? "u" : "", sh.opt_x ? "x" : "", sh.opt_f ? "f" : "");
    for (int i = 0; i < nvars; i++) {
        if ((vars[i].flags & V_EXPORTADA) || var_especial_interna(vars[i].nome)) continue;
        tx_junta(t, vars[i].nome);
        tx_car(t, '=');
        aspas_simples(t, vars[i].valor);
        tx_car(t, '\n');
    }
    for (int i = 0; i < nfuncs; i++) {
        tx_junta(t, funcs[i].nome);
        tx_junta(t, "() ");
        desanalisar(funcs[i].corpo, t, false);
        tx_car(t, '\n');
    }
    for (int i = 0; i < napelidos; i++) {
        tx_formatar(t, "alias %s=", apelidos[i].nome);
        aspas_simples(t, apelidos[i].valor);
        tx_car(t, '\n');
    }
    tx_junta(t, "set --");
    for (int i = 0; i < params.n; i++) { tx_car(t, ' '); aspas_simples(t, params.v[i]); }
    tx_car(t, '\n');
}
