// SPDX-License-Identifier: GPL-3.0-only
/* expansao.c - expansión de las palabras, en el orden del sh POSIX:
 *
 *   1. tilde (~ -> $HOME), parámetros ($VAR, ${...}), órdenes ($(...) y
 *      `...`) y aritmética ($((...))), de izquierda a derecha;
 *   2. división en campos por el IFS (solo de lo que vino de una expansión sin comillas);
 *   3. nombres de archivo (* ? [...]), si no hay "set -f";
 *   4. eliminación de las comillas.
 *
 * El resultado intermedio es una secuencia de caracteres, cada uno con
 * marcas: vino entre comillas (literal: no divide ni casa como glob), vino de
 * una expansión sin comillas (puede dividir), o es un marcador sin carácter
 * (corte de campo forzado del "$@", o "el campo existe" de un "" vacío).
 *
 * $( ... ) se ejecuta en el propio shell (D037): la salida va a un archivo
 * temporal en /tmp, que se lee de vuelta y se borra. Así las funciones y variables
 * siguen visibles allí dentro, sin necesidad de fork. */
#include "sagsh.h"
#include "contas.h"

#define M_ASPAS  1   /* literal */
#define M_EXP    2   /* resultado de expansión sin comillas: puede dividir */
#define M_QUEBRA 4   /* marcador: termina el campo aquí */
#define M_VAZIO  8   /* marcador: el campo existe aunque esté vacío ("") */

typedef struct {
    char    *c;
    uint8_t *m;
    int      n, cap;
} inter_t;

bool erro_expansao;

static void it_poe(inter_t *t, char c, uint8_t m)
{
    if (t->n == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 64;
        t->c = sh_realoca(t->c, (size_t)t->cap);
        t->m = sh_realoca(t->m, (size_t)t->cap);
    }
    t->c[t->n] = c;
    t->m[t->n++] = m;
}

static void it_texto(inter_t *t, const char *s, uint8_t m)
{
    for (; *s; s++) it_poe(t, *s, m);
}

static void it_liberar(inter_t *t)
{
    libera(t->c);
    libera(t->m);
    memset(t, 0, sizeof *t);
}

/* Contexto de la expansión. */
#define C_ASPAS   1   /* dentro de "..." */
#define C_HEREDOC 2   /* cuerpo de here-doc: las comillas son texto */
#define C_TIL     4   /* la tilde al comienzo vale */
#define C_PADRAO  8   /* ${x#patrón}: el texto va a ser patrón */

static bool expandir_em(const char *s, int n, int ctx, inter_t *saida);

/* ------------------------------------------------------------ parámetros */

static const char *IFS(void)
{
    const char *v = var_obter("IFS");
    return v ? v : " \t\n";
}

static char *valor_especial(const char *nome, bool *achou)
{
    char buf[24];
    *achou = true;
    if (nome[1] == 0) {
        switch (nome[0]) {
        case '?': formatar_texto(buf, sizeof buf, "%d", sh.status); return sh_dup(buf);
        case '$': formatar_texto(buf, sizeof buf, "%d", sh.pid); return sh_dup(buf);
        case '!':
            if (!sh.ultimo_fundo) return sh_dup("");
            formatar_texto(buf, sizeof buf, "%d", sh.ultimo_fundo);
            return sh_dup(buf);
        case '#': formatar_texto(buf, sizeof buf, "%d", params.n); return sh_dup(buf);
        case '0': return sh_dup(sh.nome0);
        case '-': {
            texto_t t = {0};
            if (sh.opt_e) tx_car(&t, 'e');
            if (sh.opt_f) tx_car(&t, 'f');
            if (sh.interativo) tx_car(&t, 'i');
            if (sh.opt_u) tx_car(&t, 'u');
            if (sh.opt_x) tx_car(&t, 'x');
            return tx_texto(&t);
        }
        case '@': case '*': {
            const char *ifs = IFS();
            texto_t t = {0};
            for (int i = 0; i < params.n; i++) {
                if (i && *ifs) tx_car(&t, ifs[0]);
                tx_junta(&t, params.v[i]);
            }
            return tx_texto(&t);
        }
        }
    }
    if (eh_numero(nome) && nome[0] != '-' && nome[0] != '+') {
        int k = para_int(nome);
        if (k == 0) return sh_dup(sh.nome0);
        if (k <= params.n) return sh_dup(params.v[k - 1]);
        *achou = false;
        return NULL;
    }
    const char *v = var_obter(nome);
    if (!v) { *achou = false; return NULL; }
    return sh_dup(v);
}

/* Pone un valor en el resultado: entre comillas queda literal; sin comillas puede
 * dividir y casar como glob. */
static void por_valor(inter_t *s, const char *v, int ctx)
{
    it_texto(s, v, (ctx & (C_ASPAS | C_HEREDOC)) ? M_ASPAS : M_EXP);
}

/* $@ y $* sin ${}: cada parámetro es un campo. */
static void por_parametros(inter_t *s, bool arroba, int ctx, bool *teve_arroba)
{
    bool aspas = (ctx & C_ASPAS) != 0;
    if (aspas && !arroba) {   /* "$*": un solo campo, separado por el 1º carácter del IFS */
        bool achou;
        char *v = valor_especial("*", &achou);
        it_texto(s, v, M_ASPAS);
        if (!*v) it_poe(s, 0, M_VAZIO);
        libera(v);
        return;
    }
    if (teve_arroba) *teve_arroba = true;
    for (int i = 0; i < params.n; i++) {
        if (i) it_poe(s, 0, M_QUEBRA);
        it_texto(s, params.v[i], aspas ? M_ASPAS : M_EXP);
        if (aspas && !params.v[i][0]) it_poe(s, 0, M_VAZIO);
    }
}

/* Texto (sin marcas) de una expansión interna, para ${x:-palabra} etc. */
static char *achatar(inter_t *t)
{
    texto_t r = {0};
    for (int i = 0; i < t->n; i++)
        if (!(t->m[i] & (M_QUEBRA | M_VAZIO))) tx_car(&r, t->c[i]);
        else if (t->m[i] & M_QUEBRA) tx_car(&r, ' ');
    return tx_texto(&r);
}

/* Patrón de un inter_t: lo que vino entre comillas se escapa con \. */
static char *padrao_de(inter_t *t)
{
    texto_t r = {0};
    for (int i = 0; i < t->n; i++) {
        if (t->m[i] & (M_QUEBRA | M_VAZIO)) continue;
        char c = t->c[i];
        if ((t->m[i] & M_ASPAS) && (c == '*' || c == '?' || c == '[' || c == ']' || c == '\\')) tx_car(&r, '\\');
        tx_car(&r, c);
    }
    return tx_texto(&r);
}

/* Quita el menor/mayor prefijo o sufijo que casa con el patrón. */
static char *remover_casando(const char *v, const char *pad, bool sufixo, bool maior)
{
    int n = (int)tam_texto(v);
    char *tmp = sh_aloca((size_t)n + 1);
    char *r = NULL;
    if (!sufixo) {
        for (int k = maior ? n : 0; maior ? k >= 0 : k <= n; k += maior ? -1 : 1) {
            memcpy(tmp, v, (size_t)k);
            tmp[k] = 0;
            if (casar(pad, tmp)) { r = sh_dup(v + k); break; }
        }
    } else {
        for (int k = maior ? 0 : n; maior ? k <= n : k >= 0; k += maior ? 1 : -1) {
            if (casar(pad, v + k)) { r = sh_dup_n(v, k); break; }
        }
    }
    libera(tmp);
    return r ? r : sh_dup(v);
}

/* ${x/patrón/cambio} y ${x//patrón/cambio} (bash). */
static char *trocar(const char *v, const char *pad, const char *troca, bool todas)
{
    texto_t r = {0};
    int n = (int)tam_texto(v);
    char *tmp = sh_aloca((size_t)n + 1);
    int i = 0;
    bool feito = false;
    while (i <= n) {
        int casou = -1;
        if (!(feito && !todas)) {
            for (int k = n; k > i; k--) {   /* el mayor trozo a partir de i */
                memcpy(tmp, v + i, (size_t)(k - i));
                tmp[k - i] = 0;
                if (casar(pad, tmp)) { casou = k; break; }
            }
        }
        if (casou > 0) {
            tx_junta(&r, troca);
            i = casou;
            feito = true;
            continue;
        }
        if (i < n) tx_car(&r, v[i]);
        i++;
    }
    libera(tmp);
    return tx_texto(&r);
}

/* ${ ... }: s apunta al primer carácter después de "{", n es el tamaño hasta
 * la "}" (exclusive). */
static bool expandir_chaves(const char *s, int n, int ctx, inter_t *saida, bool *teve_arroba)
{
    int i = 0;
    bool tamanho = false;
    if (n > 1 && s[0] == '#') { tamanho = true; i = 1; }
    /* el nombre */
    int ini = i;
    if (i < n && (s[i] == '?' || s[i] == '$' || s[i] == '!' || s[i] == '#' || s[i] == '@' || s[i] == '*' ||
                  s[i] == '-' || (s[i] >= '0' && s[i] <= '9'))) {
        if (s[i] >= '0' && s[i] <= '9') while (i < n && s[i] >= '0' && s[i] <= '9') i++;
        else i++;
    } else {
        while (i < n && ((s[i] >= 'a' && s[i] <= 'z') || (s[i] >= 'A' && s[i] <= 'Z') || s[i] == '_' ||
                         (i > ini && s[i] >= '0' && s[i] <= '9'))) i++;
    }
    if (i == ini) { erro("${%s}: sustitución incorrecta", sh_dup_n(s, n)); erro_expansao = true; return false; }
    if (tamanho && i == 1 && n == 1) { tamanho = false; i = 1; }   /* ${#} */
    char *nome = sh_dup_n(s + ini, i - ini);
    bool achou;
    bool arroba = (compara_texto(nome, "@") == 0 || compara_texto(nome, "*") == 0);
    char *valor = valor_especial(nome, &achou);

    if (tamanho) {
        if (i != n) { erro("${%s}: sustitución incorrecta", sh_dup_n(s, n)); erro_expansao = true; libera(nome); libera(valor); return false; }
        char buf[16];
        formatar_texto(buf, sizeof buf, "%d", arroba ? params.n : (int)tam_texto(valor ? valor : ""));
        por_valor(saida, buf, ctx);
        libera(nome); libera(valor);
        return true;
    }
    if (i == n) {   /* ${nombre} */
        if (!achou && sh.opt_u && !arroba) {
            erro("%s: variable sin definir", nome);
            erro_expansao = true;
            libera(nome);
            return false;
        }
        if (arroba) por_parametros(saida, nome[0] == '@', ctx, teve_arroba);
        else por_valor(saida, valor ? valor : "", ctx);
        libera(nome); libera(valor);
        return true;
    }
    /* operador */
    bool dois_pontos = s[i] == ':';
    if (dois_pontos) i++;
    char op = i < n ? s[i] : 0;
    const char *resto = s + i + 1;
    int nresto = n - i - 1;
    bool vazio = !achou || (dois_pontos && valor && !*valor);
    bool ok = true;
    if (op == '-' || op == '=' || op == '?' || op == '+') {
        bool usar_palavra = (op == '+') ? !vazio : vazio;
        if (!usar_palavra) {
            if (op == '+') { /* nada */ }
            else if (arroba) por_parametros(saida, nome[0] == '@', ctx, teve_arroba);
            else por_valor(saida, valor ? valor : "", ctx);
        } else {
            inter_t w = {0};
            ok = expandir_em(resto, nresto, (ctx & C_HEREDOC) | C_TIL, &w);
            if (ctx & (C_ASPAS | C_HEREDOC))   /* "${x:-a b}": las comillas de afuera valen para todo */
                for (int k = 0; k < w.n; k++)
                    if (!(w.m[k] & (M_QUEBRA | M_VAZIO))) w.m[k] = M_ASPAS;
            if (ok && op == '=') {
                char *v = achatar(&w);
                if (!eh_nome(nome)) { erro("%s: no se puede asignar así", nome); erro_expansao = true; ok = false; }
                else if (!var_definir(nome, v, 0)) { erro_expansao = true; ok = false; }
                else por_valor(saida, v, ctx);
                libera(v);
            } else if (ok && op == '?') {
                char *v = achatar(&w);
                erro("%s: %s", nome, *v ? v : "parámetro vacío o sin definir");
                libera(v);
                erro_expansao = true;
                ok = false;
            } else if (ok) {
                for (int k = 0; k < w.n; k++) it_poe(saida, w.c[k], w.m[k]);
            }
            it_liberar(&w);
        }
    } else if (!dois_pontos && (op == '#' || op == '%')) {
        bool maior = nresto > 0 && resto[0] == op;
        if (maior) { resto++; nresto--; }
        inter_t w = {0};
        ok = expandir_em(resto, nresto, C_PADRAO, &w);
        if (ok) {
            char *pad = padrao_de(&w);
            char *r = remover_casando(valor ? valor : "", pad, op == '%', maior);
            por_valor(saida, r, ctx);
            libera(pad);
            libera(r);
        }
        it_liberar(&w);
    } else if (!dois_pontos && op == '/') {
        bool todas = nresto > 0 && resto[0] == '/';
        if (todas) { resto++; nresto--; }
        int barra = 0;
        while (barra < nresto && resto[barra] != '/') { if (resto[barra] == '\\') barra++; barra++; }
        if (barra > nresto) barra = nresto;
        inter_t wp = {0}, wt = {0};
        ok = expandir_em(resto, barra, C_PADRAO, &wp);
        if (ok && barra < nresto) ok = expandir_em(resto + barra + 1, nresto - barra - 1, C_ASPAS, &wt);
        if (ok) {
            char *pad = padrao_de(&wp), *tr = achatar(&wt);
            char *r = trocar(valor ? valor : "", pad, tr, todas);
            por_valor(saida, r, ctx);
            libera(pad); libera(tr); libera(r);
        }
        it_liberar(&wp);
        it_liberar(&wt);
    } else if (dois_pontos && ((op >= '0' && op <= '9') || op == ' ' || op == '-')) {
        /* ${x:inicio:tamaño} (bash) */
        texto_t ex = {0};
        tx_junta_n(&ex, s + i, n - i);
        char *dp = acha_char(ex.s, ':');
        int ini_s = 0, tam = -1;
        if (dp) *dp = 0;
        if (!avaliar_aritmetica(ex.s, &ini_s) || (dp && !avaliar_aritmetica(dp + 1, &tam))) ok = false;
        tx_liberar(&ex);
        if (ok) {
            const char *v = valor ? valor : "";
            int lv = (int)tam_texto(v);
            if (ini_s < 0) ini_s += lv;
            if (ini_s < 0) ini_s = 0;
            if (ini_s > lv) ini_s = lv;
            if (tam < 0 && dp) tam += lv - ini_s;
            if (tam < 0 || ini_s + tam > lv) tam = lv - ini_s;
            char *r = sh_dup_n(v + ini_s, tam);
            por_valor(saida, r, ctx);
            libera(r);
        } else erro_expansao = true;
    } else {
        erro("${%s}: sustitución incorrecta", sh_dup_n(s, n));
        erro_expansao = true;
        ok = false;
    }
    libera(nome);
    libera(valor);
    return ok;
}

/* ------------------------------------------------------------ $( ) y ` ` */

static char *substituir_comando(const char *texto)
{
    static int seq;
    char nome[64];
    formatar_texto(nome, sizeof nome, "/tmp/.sagsh-%d-%d", sh.pid, seq++);
    int fd = abrir(nome, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) {
        erro("no he podido crear %s: %s", nome, nome_erro(fd));
        return sh_dup("");
    }
    int guarda = fd_livre();
    duplicar(SAIDA, guarda);
    duplicar(fd, SAIDA);
    fechar(fd);
    executar_subshell_texto(texto);
    duplicar(guarda, SAIDA);
    fechar(guarda);
    texto_t r = {0};
    fd = abrir(nome, ABRIR_LER);
    if (fd >= 0) {
        char buf[512];
        int k;
        while ((k = ler(fd, buf, sizeof buf)) > 0) tx_junta_n(&r, buf, k);
        fechar(fd);
    }
    apagar(nome);
    tx_texto(&r);
    while (r.n > 0 && r.s[r.n - 1] == '\n') r.s[--r.n] = 0;
    /* los bytes nulos no caben en un texto C: se convierten en nada */
    int k = 0;
    for (int i = 0; i < r.n; i++) if (r.s[i]) r.s[k++] = r.s[i];
    r.s[k] = 0;
    return r.s;
}

/* `...`: dentro, \$ \` \\ se vuelven el carácter. */
static char *texto_da_crase(const char *s, int n)
{
    texto_t t = {0};
    for (int i = 0; i < n; i++) {
        if (s[i] == '\\' && i + 1 < n && (s[i + 1] == '$' || s[i + 1] == '`' || s[i + 1] == '\\')) i++;
        tx_car(&t, s[i]);
    }
    return tx_texto(&t);
}

/* ------------------------------------------------------------ el bucle principal */

/* Fin de un "${...}" que empieza en s (s[0] = '{'); devuelve el índice de la "}". */
static int fim_chaves(const char *s, int n)
{
    int i = 1;
    while (i < n) {
        char c = s[i];
        if (c == '\\') { i += 2; continue; }
        if (c == '}') return i;
        if (c == '\'' ) { i++; while (i < n && s[i] != '\'') i++; i++; continue; }
        if (c == '"') { i++; while (i < n && s[i] != '"') { if (s[i] == '\\') i++; i++; } i++; continue; }
        if (c == '$' && i + 1 < n && s[i + 1] == '{') { int f = fim_chaves(s + i + 1, n - i - 1); if (f < 0) return -1; i += f + 2; continue; }
        if (c == '$' && i + 1 < n && s[i + 1] == '(') {
            texto_t tmp = {0};
            tx_junta_n(&tmp, s + i + 2, n - i - 2);
            int f = achar_fecha_subst(tmp.s, 0);
            tx_liberar(&tmp);
            if (f < 0) return -1;
            i += f + 3;
            continue;
        }
        i++;
    }
    return -1;
}

/* Fin de "$((...))": devuelve el índice del primer ")" de los "))", o -1. */
static int fim_aritmetica(const char *s, int n)
{
    int nivel = 0;
    for (int i = 0; i < n; i++) {
        if (s[i] == '(') nivel++;
        else if (s[i] == ')') {
            if (nivel == 0) return (i + 1 < n && s[i + 1] == ')') ? i : -1;
            nivel--;
        }
    }
    return -1;
}

static bool expandir_em(const char *s, int n, int ctx, inter_t *saida)
{
    bool aspas = false;      /* dentro de "..." en esta palabra */
    int  ini_aspas = 0;      /* saida->n cuando se abrieron las comillas */
    bool arroba = false;     /* hubo "$@" dentro de estas comillas */
    for (int i = 0; i < n; i++) {
        char c = s[i];
        int dq = (aspas || (ctx & C_ASPAS)) ? C_ASPAS : 0;
        int ctx_aqui = (ctx & ~C_ASPAS) | dq;
        bool literal_aspas = dq || (ctx & C_HEREDOC);

        if (c == '~' && i == 0 && (ctx & C_TIL) && !literal_aspas) {
            int j = 1;
            while (j < n && s[j] != '/') j++;
            if (j == 1) {
                const char *h = var_obter("HOME");
                it_texto(saida, h ? h : "/", M_ASPAS);
                i = j - 1;
                continue;
            }
            if (j == 2 && s[1] == '+') {
                const char *h = var_obter("PWD");
                it_texto(saida, h ? h : "", M_ASPAS);
                i = j - 1;
                continue;
            }
            it_poe(saida, c, 0);
            continue;
        }
        if (c == '\'' && !literal_aspas) {
            int j = i + 1;
            while (j < n && s[j] != '\'') j++;
            for (int k = i + 1; k < j; k++) it_poe(saida, s[k], M_ASPAS);
            if (j == i + 1) it_poe(saida, 0, M_VAZIO);
            i = j;
            continue;
        }
        if (c == '"' && !(ctx & C_HEREDOC) && !(ctx & C_ASPAS)) {
            if (!aspas) { aspas = true; ini_aspas = saida->n; arroba = false; }
            else {
                aspas = false;
                if (saida->n == ini_aspas && !arroba) it_poe(saida, 0, M_VAZIO);
            }
            continue;
        }
        if (c == '\\') {
            if (i + 1 >= n) { it_poe(saida, c, M_ASPAS); continue; }
            char d = s[i + 1];
            if (!literal_aspas) { it_poe(saida, d, M_ASPAS); i++; continue; }
            if (d == '$' || d == '`' || d == '\\' || (d == '"' && !(ctx & C_HEREDOC) && dq)) {
                it_poe(saida, d, M_ASPAS);
                i++;
                continue;
            }
            if (d == '\n') { i++; continue; }
            it_poe(saida, c, M_ASPAS);
            continue;
        }
        if (c == '`') {
            int j = i + 1;
            while (j < n && s[j] != '`') { if (s[j] == '\\') j++; j++; }
            if (j > n) j = n;
            char *cmd = texto_da_crase(s + i + 1, j - i - 1);
            char *r = substituir_comando(cmd);
            por_valor(saida, r, ctx_aqui);
            libera(cmd);
            libera(r);
            i = j;
            continue;
        }
        if (c == '$' && i + 1 < n) {
            char d = s[i + 1];
            if (d == '(' && i + 2 < n && s[i + 2] == '(') {
                int f = fim_aritmetica(s + i + 3, n - i - 3);
                if (f >= 0) {
                    inter_t w = {0};
                    if (!expandir_em(s + i + 3, f, C_ASPAS, &w)) { it_liberar(&w); return false; }
                    char *ex = achatar(&w);
                    it_liberar(&w);
                    int v;
                    bool ok = avaliar_aritmetica(ex, &v);
                    libera(ex);
                    if (!ok) { erro_expansao = true; return false; }
                    char buf[16];
                    formatar_texto(buf, sizeof buf, "%d", v);
                    por_valor(saida, buf, ctx_aqui);
                    i += 3 + f + 1;
                    continue;
                }
            }
            if (d == '(') {
                char *resto = sh_dup_n(s + i + 2, n - i - 2);
                int f = achar_fecha_subst(resto, 0);
                if (f < 0) {
                    libera(resto);
                    erro("$(: falta ')'");
                    erro_expansao = true;
                    return false;
                }
                resto[f] = 0;
                char *r = substituir_comando(resto);
                por_valor(saida, r, ctx_aqui);
                libera(resto);
                libera(r);
                i += 2 + f;
                continue;
            }
            if (d == '{') {
                int f = fim_chaves(s + i + 1, n - i - 1);
                if (f < 0) { erro("${: falta '}'"); erro_expansao = true; return false; }
                if (!expandir_chaves(s + i + 2, f - 1, ctx_aqui, saida, &arroba)) return false;
                i += 1 + f;
                continue;
            }
            if (d == '@' || d == '*') {
                por_parametros(saida, d == '@', ctx_aqui, &arroba);
                i++;
                continue;
            }
            if (d == '?' || d == '$' || d == '!' || d == '#' || d == '-' || (d >= '0' && d <= '9')) {
                char nome[2] = { d, 0 };
                bool achou;
                char *v = valor_especial(nome, &achou);
                if (!achou && sh.opt_u) { erro("$%s: variable sin definir", nome); erro_expansao = true; return false; }
                por_valor(saida, v ? v : "", ctx_aqui);
                libera(v);
                i++;
                continue;
            }
            if ((d >= 'a' && d <= 'z') || (d >= 'A' && d <= 'Z') || d == '_') {
                int j = i + 1;
                while (j < n && ((s[j] >= 'a' && s[j] <= 'z') || (s[j] >= 'A' && s[j] <= 'Z') || s[j] == '_' ||
                                 (s[j] >= '0' && s[j] <= '9'))) j++;
                char *nome = sh_dup_n(s + i + 1, j - i - 1);
                const char *v = var_obter(nome);
                if (!v && sh.opt_u) { erro("%s: variable sin definir", nome); libera(nome); erro_expansao = true; return false; }
                por_valor(saida, v ? v : "", ctx_aqui);
                libera(nome);
                i = j - 1;
                continue;
            }
        }
        it_poe(saida, c, literal_aspas ? M_ASPAS : 0);
    }
    return true;
}

/* ------------------------------------------------------------ división y glob */

typedef struct {
    inter_t atual;
    bool    aberto;   /* el campo empezó (tiene contenido o un "" vacío) */
} campo_t;

static void fechar_campo(campo_t *c, lista_t *saida, bool glob)
{
    if (!c->aberto) return;
    bool tem_glob_ativo = false;
    for (int i = 0; i < c->atual.n; i++)
        if (!(c->atual.m[i] & (M_ASPAS | M_QUEBRA | M_VAZIO)) &&
            (c->atual.c[i] == '*' || c->atual.c[i] == '?' || c->atual.c[i] == '['))
            tem_glob_ativo = true;
    if (glob && tem_glob_ativo && !sh.opt_f) {
        char *pad = padrao_de(&c->atual);
        lista_t achados = {0};
        glob_caminhos(pad, &achados);
        libera(pad);
        if (achados.n) {
            for (int i = 0; i < achados.n; i++) lista_poe(saida, achados.v[i]);
            libera(achados.v);
            c->atual.n = 0;
            c->aberto = false;
            return;
        }
    }
    texto_t t = {0};
    for (int i = 0; i < c->atual.n; i++)
        if (!(c->atual.m[i] & (M_QUEBRA | M_VAZIO))) tx_car(&t, c->atual.c[i]);
    lista_poe(saida, tx_texto(&t));
    c->atual.n = 0;
    c->aberto = false;
}

static void dividir(inter_t *t, lista_t *saida, bool glob)
{
    const char *ifs = IFS();
    campo_t c;
    memset(&c, 0, sizeof c);
    bool depois_de_branco = false;   /* acaba de cerrar un campo en un blanco del IFS */
    for (int i = 0; i < t->n; i++) {
        uint8_t m = t->m[i];
        char ch = t->c[i];
        if (m & M_QUEBRA) { fechar_campo(&c, saida, glob); depois_de_branco = false; continue; }
        if (m & M_VAZIO) { c.aberto = true; it_poe(&c.atual, 0, M_VAZIO); continue; }
        if ((m & M_EXP) && *ifs && acha_char(ifs, (unsigned char)ch)) {
            bool branco = ch == ' ' || ch == '\t' || ch == '\n';
            if (branco) {
                if (c.aberto) { fechar_campo(&c, saida, glob); depois_de_branco = true; }
            } else {
                if (!c.aberto && !depois_de_branco) c.aberto = true;   /* ",a": campo vacío antes */
                if (c.aberto) fechar_campo(&c, saida, glob);
                depois_de_branco = false;
            }
            continue;
        }
        c.aberto = true;
        depois_de_branco = false;
        it_poe(&c.atual, ch, m);
    }
    fechar_campo(&c, saida, glob);
    it_liberar(&c.atual);
}

bool expandir_palavras(char **cruas, int n, lista_t *saida)
{
    erro_expansao = false;
    for (int i = 0; i < n; i++) {
        inter_t t = {0};
        if (!expandir_em(cruas[i], (int)tam_texto(cruas[i]), C_TIL, &t) || erro_expansao) {
            it_liberar(&t);
            return false;
        }
        dividir(&t, saida, true);
        it_liberar(&t);
    }
    return true;
}

static char *expandir_simples(const char *crua, int ctx)
{
    erro_expansao = false;
    inter_t t = {0};
    bool ok = expandir_em(crua, (int)tam_texto(crua), ctx, &t);
    char *r = ok ? achatar(&t) : NULL;
    it_liberar(&t);
    if (!ok) erro_expansao = true;
    return r ? r : sh_dup("");
}

char *expandir_texto(const char *crua, bool til) { return expandir_simples(crua, til ? C_TIL : 0); }
char *expandir_heredoc(const char *corpo) { return expandir_simples(corpo, C_HEREDOC); }

char *expandir_padrao(const char *crua)
{
    erro_expansao = false;
    inter_t t = {0};
    bool ok = expandir_em(crua, (int)tam_texto(crua), C_TIL, &t);
    char *r = ok ? padrao_de(&t) : sh_dup("");
    it_liberar(&t);
    if (!ok) erro_expansao = true;
    return r;
}

char *tirar_aspas(const char *crua)
{
    texto_t t = {0};
    char aspa = 0;
    for (const char *c = crua; *c; c++) {
        if (aspa == '\'') { if (*c == '\'') aspa = 0; else tx_car(&t, *c); continue; }
        if (*c == '\'' && !aspa) { aspa = '\''; continue; }
        if (*c == '"') { aspa = aspa ? 0 : '"'; continue; }
        if (*c == '\\' && c[1]) { c++; }
        tx_car(&t, *c);
    }
    return tx_texto(&t);
}

/* PS1/PS2: primero los escapes de bash (\w \W \u \h \$ \n \\), después
 * $VAR y $(...) como dentro de comillas. */
char *expandir_prompt(const char *ps)
{
    texto_t t = {0};
    for (const char *c = ps; *c; c++) {
        if (*c != '\\' || !c[1]) { tx_car(&t, *c); continue; }
        c++;
        char pasta[256];
        switch (*c) {
        case 'w': case 'W':
            if (pasta_atual(pasta, sizeof pasta) < 0) copia_texto(pasta, "?", sizeof pasta);
            if (*c == 'W' && compara_texto(pasta, "/") != 0) {
                char *b = acha_ultimo_char(pasta, '/');
                tx_junta(&t, b ? b + 1 : pasta);
            } else {
                tx_junta(&t, pasta);
            }
            break;
        case 'u': { char n[32]; tx_junta(&t, conta_nome(meu_uid(), n, sizeof n)); break; }   /* D255 */
        case 'h': case 'H': tx_junta(&t, "saguios"); break;
        case 's': tx_junta(&t, "sagsh"); break;
        case '$': tx_car(&t, '$'); break;
        case 'n': tx_car(&t, '\n'); break;
        case '\\': tx_car(&t, '\\'); break;
        case '[': case ']': break;
        default: tx_car(&t, '\\'); tx_car(&t, *c); break;
        }
    }
    tx_texto(&t);
    if (!acha_char(t.s, '$') && !acha_char(t.s, '`')) return t.s;
    int st = sh.status;
    char *r = expandir_simples(t.s, C_ASPAS);
    sh.status = st;
    tx_liberar(&t);
    return r;
}

/* ------------------------------------------------------------ casar y glob */

bool tem_glob(const char *s)
{
    for (; *s; s++) {
        if (*s == '\\' && s[1]) { s++; continue; }
        if (*s == '*' || *s == '?' || *s == '[') return true;
    }
    return false;
}

/* [abc], [a-z], [!abc] o [^abc]. Devuelve el tamaño de la clase (0 = no es
 * una clase: el "[" es literal) y si casó. */
static int classe(const char *p, char c, bool *casou)
{
    int i = 1;
    bool negar = false;
    if (p[i] == '!' || p[i] == '^') { negar = true; i++; }
    bool achou = false;
    bool primeiro = true;
    while (p[i] && (p[i] != ']' || primeiro)) {
        char a = p[i];
        if (a == '\\' && p[i + 1]) a = p[++i];
        if (p[i + 1] == '-' && p[i + 2] && p[i + 2] != ']') {
            char b = p[i + 2];
            if (b == '\\' && p[i + 3]) { b = p[i + 3]; i++; }
            if ((unsigned char)c >= (unsigned char)a && (unsigned char)c <= (unsigned char)b) achou = true;
            i += 3;
        } else {
            if (c == a) achou = true;
            i++;
        }
        primeiro = false;
    }
    if (p[i] != ']') return 0;
    *casou = achou != negar;
    return i + 1;
}

bool casar(const char *p, const char *t)
{
    const char *estrela_p = NULL, *estrela_t = NULL;
    for (;;) {
        if (*p == '*') {
            while (*p == '*') p++;
            if (!*p) return true;
            estrela_p = p;
            estrela_t = t;
            continue;
        }
        if (*t) {
            bool ok = false;
            int avanco = 1;
            if (*p == '?') ok = true;
            else if (*p == '[') {
                bool casou;
                int k = classe(p, *t, &casou);
                if (k) { ok = casou; avanco = k; }
                else ok = *t == '[';
            } else if (*p == '\\' && p[1]) { ok = p[1] == *t; avanco = 2; }
            else ok = *p && *p == *t;
            if (ok) { p += avanco; t++; continue; }
        } else if (!*p) {
            return true;
        }
        if (!estrela_p || !*estrela_t) return false;
        p = estrela_p;
        t = ++estrela_t;
    }
}

static void sem_barras_invertidas(texto_t *t, const char *s, int n)
{
    for (int i = 0; i < n; i++) {
        if (s[i] == '\\' && i + 1 < n) i++;
        tx_car(t, s[i]);
    }
}

/* Expande un componente cada vez: "base" es lo que ya casó (vacío =
 * carpeta actual), "resto" el patrón que falta. */
static void glob_rec(const char *base, const char *resto, lista_t *saida, int prof)
{
    if (prof > 32) return;
    while (*resto == '/') resto++;
    const char *barra = resto;
    while (*barra && *barra != '/') { if (*barra == '\\' && barra[1]) barra++; barra++; }
    int ncomp = (int)(barra - resto);
    char *comp = sh_dup_n(resto, ncomp);
    bool ultimo = !*barra;
    if (!tem_glob(comp)) {
        texto_t c = {0};
        tx_junta(&c, base);
        if (*base && base[tam_texto(base) - 1] != '/') tx_car(&c, '/');
        sem_barras_invertidas(&c, comp, ncomp);
        tx_texto(&c);
        info_arquivo_t inf;
        if (info_arquivo(c.s, &inf) == 0) {
            if (ultimo) lista_poe(saida, sh_dup(c.s));
            else if (inf.tipo == TIPO_PASTA) {
                if (!barra[1]) { tx_car(&c, '/'); lista_poe(saida, sh_dup(c.s)); }
                else glob_rec(c.s, barra, saida, prof + 1);
            }
        }
        tx_liberar(&c);
        libera(comp);
        return;
    }
    const char *pasta = *base ? base : ".";
    lista_t nomes = {0};
    entrada_pasta_t e;
    for (int i = 0; listar(pasta, i, &e) == 1; i++) {
        if (e.nome[0] == '.' && comp[0] != '.') continue;   /* ocultos solo con "." explícito */
        if (!casar(comp, e.nome)) continue;
        if (!ultimo && e.tipo != TIPO_PASTA) continue;
        lista_poe(&nomes, sh_dup(e.nome));
    }
    ordenar(nomes.v, nomes.n);
    for (int i = 0; i < nomes.n; i++) {
        texto_t c = {0};
        tx_junta(&c, base);
        if (*base && base[tam_texto(base) - 1] != '/') tx_car(&c, '/');
        tx_junta(&c, nomes.v[i]);
        tx_texto(&c);
        if (ultimo) lista_poe(saida, c.s);
        else {
            if (!barra[1]) { tx_car(&c, '/'); lista_poe(saida, c.s); }
            else { glob_rec(c.s, barra, saida, prof + 1); tx_liberar(&c); }
        }
    }
    lista_liberar(&nomes);
    libera(comp);
}

void glob_caminhos(const char *padrao, lista_t *saida)
{
    if (padrao[0] == '/') glob_rec("/", padrao + 1, saida, 0);
    else glob_rec("", padrao, saida, 0);
}
