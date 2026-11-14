// SPDX-License-Identifier: GPL-3.0-only
/* aritmetica.c - $(( ... )): enteros de 32 bits, con la precedencia de C.
 *
 * El texto ya llega con $VAR y $(...) expandidos; los nombres sueltos son
 * variables (vacía o inexistente = 0; el valor puede ser otra expresión).
 * Soporta: + - * / % ** << >> < <= > >= == != & ^ | && || ! ~ ?: ,
 * paréntesis, = += -= *= /= %= <<= >>= &= ^= |=, ++ y -- (antes y después).
 * En las ramas no elegidas de && || ?: no se asigna nada ("sem_efeito"). */
#include "sagsh.h"

typedef struct {
    const char *s;
    int  pos;
    bool erro;
    int  sem_efeito;
    int  prof;
} ar_t;

static int expr(ar_t *a);
static int atribuicao(ar_t *a);

static void falhar(ar_t *a, const char *msg)
{
    if (!a->erro) erro("%s: %s", a->s, msg);
    a->erro = true;
}

static void espacos(ar_t *a)
{
    while (a->s[a->pos] == ' ' || a->s[a->pos] == '\t' || a->s[a->pos] == '\n') a->pos++;
}

static bool ver(ar_t *a, const char *op)
{
    espacos(a);
    size_t n = tam_texto(op);
    if (compara_texto_n(a->s + a->pos, op, n) != 0) return false;
    /* no confundir "<" con "<<" o "<=", "&" con "&&", etc. */
    char d = a->s[a->pos + n];
    if (n == 1 && (op[0] == '<' || op[0] == '>') && (d == op[0] || d == '=')) return false;
    if (n == 1 && (op[0] == '&' || op[0] == '|') && (d == op[0] || d == '=')) return false;
    if (n == 1 && (op[0] == '+' || op[0] == '-') && (d == op[0] || d == '=')) return false;
    if (n == 1 && (op[0] == '*' || op[0] == '/' || op[0] == '%' || op[0] == '^' || op[0] == '!' || op[0] == '=') && d == '=')
        return false;
    if (n == 1 && op[0] == '*' && d == '*') return false;
    if (n == 2 && (compara_texto(op, "<<") == 0 || compara_texto(op, ">>") == 0) && d == '=') return false;
    if (n == 2 && compara_texto(op, "**") == 0 && d == '=') return false;
    return true;
}

static bool aceitar(ar_t *a, const char *op)
{
    if (!ver(a, op)) return false;
    a->pos += (int)tam_texto(op);
    return true;
}

static int valor_de_var(ar_t *a, const char *nome)
{
    const char *v = var_obter(nome);
    if (!v || !*v) {
        if (!v && sh.opt_u) falhar(a, "variable sin definir");
        return 0;
    }
    if (eh_numero(v)) return para_int(v);
    if (a->prof > 20) { falhar(a, "demasiada recursión"); return 0; }
    ar_t b = { v, 0, false, a->sem_efeito, a->prof + 1 };
    int r = expr(&b);
    espacos(&b);
    if (b.erro || b.s[b.pos]) { falhar(a, "expresión no válida en una variable"); return 0; }
    return r;
}

static void guardar(ar_t *a, const char *nome, int v)
{
    if (a->sem_efeito) return;
    char buf[16];
    formatar_texto(buf, sizeof buf, "%d", v);
    if (!var_definir(nome, buf, 0)) a->erro = true;
}

static bool ler_nome(ar_t *a, char *nome, int max)
{
    espacos(a);
    int i = 0;
    char c = a->s[a->pos];
    if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_')) return false;
    while (((c = a->s[a->pos]) >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || (c >= '0' && c <= '9')) {
        if (i < max - 1) nome[i++] = c;
        a->pos++;
    }
    nome[i] = 0;
    return true;
}

static int numero(ar_t *a)
{
    espacos(a);
    const char *s = a->s;
    int base = 10, v = 0;
    if (s[a->pos] == '0' && (s[a->pos + 1] == 'x' || s[a->pos + 1] == 'X')) { base = 16; a->pos += 2; }
    else if (s[a->pos] == '0' && s[a->pos + 1] >= '0' && s[a->pos + 1] <= '9') { base = 8; a->pos++; }
    bool algum = false;
    for (;;) {
        char c = s[a->pos];
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else break;
        if (d >= base) { falhar(a, "número no válido"); return 0; }
        v = (int)((unsigned)v * (unsigned)base + (unsigned)d);
        a->pos++;
        algum = true;
    }
    if (!algum && base != 8) falhar(a, "número no válido");
    return v;
}

static int unario(ar_t *a);

static int primario(ar_t *a)
{
    if (a->erro) return 0;
    espacos(a);
    char c = a->s[a->pos];
    if (c == '(') {
        a->pos++;
        int v = expr(a);
        if (!aceitar(a, ")")) falhar(a, "falta ')'");
        return v;
    }
    if (c >= '0' && c <= '9') return numero(a);
    char nome[64];
    if (ler_nome(a, nome, sizeof nome)) {
        int v = valor_de_var(a, nome);
        if (aceitar(a, "++")) { guardar(a, nome, v + 1); return v; }
        if (aceitar(a, "--")) { guardar(a, nome, v - 1); return v; }
        return v;
    }
    falhar(a, c ? "error de sintaxis en la expresión" : "falta un operando");
    return 0;
}

static int unario(ar_t *a)
{
    if (a->erro) return 0;
    espacos(a);
    if (a->s[a->pos] == '+' && a->s[a->pos + 1] == '+') {
        a->pos += 2;
        char nome[64];
        if (!ler_nome(a, nome, sizeof nome)) { falhar(a, "++ sin variable"); return 0; }
        int v = valor_de_var(a, nome) + 1;
        guardar(a, nome, v);
        return v;
    }
    if (a->s[a->pos] == '-' && a->s[a->pos + 1] == '-') {
        a->pos += 2;
        char nome[64];
        if (!ler_nome(a, nome, sizeof nome)) { falhar(a, "-- sin variable"); return 0; }
        int v = valor_de_var(a, nome) - 1;
        guardar(a, nome, v);
        return v;
    }
    if (aceitar(a, "-")) return -unario(a);
    if (aceitar(a, "+")) return unario(a);
    if (aceitar(a, "!")) return !unario(a);
    if (aceitar(a, "~")) return ~unario(a);
    return primario(a);
}

static int potencia(ar_t *a)
{
    int b = unario(a);
    if (aceitar(a, "**")) {
        int e = potencia(a);   /* asocia por la derecha */
        if (e < 0) { falhar(a, "exponente negativo"); return 0; }
        int r = 1;
        while (e-- > 0) r *= b;
        return r;
    }
    return b;
}

static int multiplicacao(ar_t *a)
{
    int v = potencia(a);
    for (;;) {
        if (aceitar(a, "*")) v *= potencia(a);
        else if (ver(a, "/") || ver(a, "%")) {
            bool div = a->s[a->pos] == '/';
            a->pos++;
            int d = potencia(a);
            if (d == 0) { if (!a->sem_efeito) falhar(a, "división por cero"); return 0; }
            if (d == -1) v = div ? -v : 0;
            else v = div ? v / d : v % d;
        } else return v;
    }
}

static int soma(ar_t *a)
{
    int v = multiplicacao(a);
    for (;;) {
        if (aceitar(a, "+")) v += multiplicacao(a);
        else if (aceitar(a, "-")) v -= multiplicacao(a);
        else return v;
    }
}

static int deslocamento(ar_t *a)
{
    int v = soma(a);
    for (;;) {
        if (aceitar(a, "<<")) v <<= (soma(a) & 31);
        else if (aceitar(a, ">>")) v >>= (soma(a) & 31);
        else return v;
    }
}

static int relacional(ar_t *a)
{
    int v = deslocamento(a);
    for (;;) {
        if (aceitar(a, "<=")) v = v <= deslocamento(a);
        else if (aceitar(a, ">=")) v = v >= deslocamento(a);
        else if (aceitar(a, "<")) v = v < deslocamento(a);
        else if (aceitar(a, ">")) v = v > deslocamento(a);
        else return v;
    }
}

static int igualdade(ar_t *a)
{
    int v = relacional(a);
    for (;;) {
        if (aceitar(a, "==")) v = v == relacional(a);
        else if (aceitar(a, "!=")) v = v != relacional(a);
        else return v;
    }
}

static int e_bit(ar_t *a)  { int v = igualdade(a); while (aceitar(a, "&")) v &= igualdade(a); return v; }
static int xou_bit(ar_t *a) { int v = e_bit(a); while (aceitar(a, "^")) v ^= e_bit(a); return v; }
static int ou_bit(ar_t *a) { int v = xou_bit(a); while (aceitar(a, "|")) v |= xou_bit(a); return v; }

static int e_logico(ar_t *a)
{
    int v = ou_bit(a);
    while (aceitar(a, "&&")) {
        if (!v) a->sem_efeito++;
        int d = ou_bit(a);
        if (!v) a->sem_efeito--;
        v = v && d;
    }
    return v;
}

static int ou_logico(ar_t *a)
{
    int v = e_logico(a);
    while (aceitar(a, "||")) {
        if (v) a->sem_efeito++;
        int d = e_logico(a);
        if (v) a->sem_efeito--;
        v = v || d;
    }
    return v;
}

static int ternario(ar_t *a)
{
    int c = ou_logico(a);
    if (!aceitar(a, "?")) return c;
    if (!c) a->sem_efeito++;
    int x = expr(a);
    if (!c) a->sem_efeito--;
    if (!aceitar(a, ":")) { falhar(a, "falta ':'"); return 0; }
    if (c) a->sem_efeito++;
    int y = ternario(a);
    if (c) a->sem_efeito--;
    return c ? x : y;
}

static int atribuicao(ar_t *a)
{
    espacos(a);
    int salvo = a->pos;
    char nome[64];
    if (ler_nome(a, nome, sizeof nome)) {
        static const char *ops[] = { "=", "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "^=", "|=", NULL };
        espacos(a);
        for (int i = 0; ops[i]; i++) {
            size_t n = tam_texto(ops[i]);
            if (compara_texto_n(a->s + a->pos, ops[i], n) != 0) continue;
            if (i == 0 && a->s[a->pos + 1] == '=') break;   /* es "==" */
            a->pos += (int)n;
            int d = atribuicao(a);
            int v = i ? valor_de_var(a, nome) : 0;
            switch (i) {
            case 0: v = d; break;
            case 1: v += d; break;
            case 2: v -= d; break;
            case 3: v *= d; break;
            case 4: case 5:
                if (d == 0) { falhar(a, "división por cero"); return 0; }
                v = i == 4 ? v / d : v % d;
                break;
            case 6: v <<= (d & 31); break;
            case 7: v >>= (d & 31); break;
            case 8: v &= d; break;
            case 9: v ^= d; break;
            case 10: v |= d; break;
            }
            guardar(a, nome, v);
            return v;
        }
    }
    a->pos = salvo;
    return ternario(a);
}

static int expr(ar_t *a)
{
    int v = atribuicao(a);
    while (!a->erro && aceitar(a, ",")) v = atribuicao(a);
    return v;
}

bool avaliar_aritmetica(const char *texto, int *valor)
{
    ar_t a = { texto, 0, false, 0, 0 };
    espacos(&a);
    if (!texto[a.pos]) { *valor = 0; return true; }
    int v = expr(&a);
    espacos(&a);
    if (!a.erro && texto[a.pos]) falhar(&a, "error de sintaxis en la expresión");
    *valor = v;
    return !a.erro;
}
