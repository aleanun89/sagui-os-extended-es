// SPDX-License-Identifier: GPL-2.0-or-later
/* grep (procurar) - busca líneas que coinciden con un patrón (D097).
 * Patrón: expresión regular básica (como grep) o extendida con -E; -F = texto literal. */
#include "sagui.h"

#define MAX_PAT 8
#define LINEA_MAX 4096

static const char *AYUDA =
    "uso: grep [opciones] PATRÓN [archivo...]     grep [opciones] -e PATRÓN... [archivo...]\n"
    "  -i  ignora mayúsculas     -v  líneas que NO coinciden    -n  número de línea\n"
    "  -c  solo cuenta           -l  solo nombres de archivos   -L  archivos sin coincidencias\n"
    "  -r  recursivo (sin archivo busca en .)   -E  expresión extendida (| + ? ( ))   -F  texto literal\n"
    "  -w  palabra completa      -x  línea completa             -o  solo la parte que coincide\n"
    "  -H / -h  con / sin nombre de archivo   -q  sin salida, solo el código   -s  sin errores\n"
    "  -m N  para tras N líneas  -A N / -B N / -C N  líneas de contexto después / antes / ambas\n"
    "Expresiones: . * + ? | ( ) [abc] [^a-z] [[:digit:]] ^ $ \\w \\s \\d \\b (en básica, \\+ \\? \\| \\( \\)).\n"
    "Código de salida: 0 si hubo coincidencias, 1 si no, 2 si hubo errores.\n";

static bool opc_i, opc_v, opc_n, opc_c, opc_l, opc_L, opc_r, opc_E, opc_F, opc_w, opc_x, opc_o, opc_q, opc_s;
static int con_nombre = -1, maximo = -1, ctx_antes, ctx_despues;
static char *pats[MAX_PAT];
static int npats;
static er_t er[MAX_PAT];
static bool hubo_error, hubo_match;

static int mayus(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }

/* Busca el literal p en t a partir de desde. */
static bool buscar_literal(const char *p, const char *t, int n, int desde, int *ini, int *fim)
{
    int lp = (int)tam_texto(p);
    for (int s = desde; s + lp <= n; s++) {
        int k = 0;
        while (k < lp && (opc_i ? mayus((unsigned char)t[s + k]) == mayus((unsigned char)p[k]) : t[s + k] == p[k])) k++;
        if (k == lp) { *ini = s; *fim = s + lp; return true; }
    }
    return false;
}

static bool letra_pal(int c) { return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0xC0; }

/* Primera coincidencia a partir de desde con cualquiera de los patrones (la más a la izquierda). */
static bool coincide(const char *t, int n, int desde, int *ini, int *fim)
{
    for (int pos = desde; pos <= n; pos++) {
        int mejor_i = -1, mejor_f = -1;
        for (int k = 0; k < npats; k++) {
            int i, f;
            bool ok = opc_F ? buscar_literal(pats[k], t, n, pos, &i, &f) : er_buscar(&er[k], t, n, pos, &i, &f);
            if (!ok) continue;
            if (opc_F && (opc_x || opc_w)) {
                /* restricciones sobre el literal */
                bool bien = true;
                if (opc_x) bien = i == 0 && f == n;
                else bien = (i == 0 || !letra_pal((unsigned char)t[i - 1])) && (f == n || !letra_pal((unsigned char)t[f]));
                if (!bien) continue;
            }
            if (mejor_i < 0 || i < mejor_i) { mejor_i = i; mejor_f = f; }
        }
        if (mejor_i >= 0) { *ini = mejor_i; *fim = mejor_f; return true; }
        break;
    }
    return false;
}

static bool linea_coincide(const char *t, int n)
{
    int i, f;
    return coincide(t, n, 0, &i, &f);
}

/* Líneas de contexto: anillo con las últimas ctx_antes líneas. */
#define CTX_MAX 8
static char anillo[CTX_MAX][512];
static int anillo_n[CTX_MAX], anillo_num[CTX_MAX];

static void escribir_linea(const char *nome, int num, char sep, const char *t, int n)
{
    static char s[LINEA_MAX + 400];
    int k = 0;
    if (con_nombre > 0) k += formatar_texto(s + k, sizeof s - (size_t)k, "%s%c", nome, sep);
    if (opc_n) k += formatar_texto(s + k, sizeof s - (size_t)k, "%d%c", num, sep);
    if (k + n + 2 > (int)sizeof s) n = (int)sizeof s - k - 2;
    memcpy(s + k, t, (size_t)n);
    saida_linha(s, k + n);
}

/* Devuelve: 1 si hubo coincidencias; -1 si hay que parar todo (pipe cerrado o -q). */
static int buscar_en(int fd, const char *nome)
{
    leitor_linhas_t l;
    static char linha[LINEA_MAX];
    leitor_iniciar(&l, fd, true);
    int n, num = 0, cuenta = 0, desp = 0, hist = 0;
    bool binario = false;
    while ((n = leitor_linha(&l, linha, sizeof linha, NULL)) >= 0) {
        num++;
        if (!binario) for (int i = 0; i < n; i++) if (linha[i] == 0) { binario = true; break; }
        bool ok = linea_coincide(linha, n) != opc_v;
        if (!ok) {
            if (!opc_c && !opc_l && !opc_L && !opc_q) {
                if (desp > 0) { desp--; escribir_linea(nome, num, '-', linha, n); }
                else if (ctx_antes) {
                    int p = num % ctx_antes;
                    int m = n < 511 ? n : 511;
                    memcpy(anillo[p], linha, (size_t)m);
                    anillo_n[p] = m;
                    anillo_num[p] = num;
                    if (hist < ctx_antes) hist++;
                }
            }
            continue;
        }
        cuenta++;
        hubo_match = true;
        if (opc_q) return -1;
        if (opc_l || opc_L) break;
        if (binario && !opc_c) { imprimir("Archivo binario %s coincide\n", nome); break; }
        if (!opc_c) {
            if ((ctx_antes || ctx_despues) && hist) {   /* contexto anterior pendiente */
                for (int k = ctx_antes; k >= 1; k--) {
                    int numk = num - k;
                    if (numk < 1) continue;
                    int p = numk % ctx_antes;
                    if (anillo_num[p] == numk) escribir_linea(nome, numk, '-', anillo[p], anillo_n[p]);
                }
                hist = 0;
            }
            if (opc_o && !opc_v) {
                int pos = 0, i, f;
                while (pos <= n && coincide(linha, n, pos, &i, &f)) {
                    if (f == i) { pos = f + 1; continue; }
                    escribir_linea(nome, num, ':', linha + i, f - i);
                    pos = f;
                }
            } else escribir_linea(nome, num, ':', linha, n);
            desp = ctx_despues;
        }
        if (maximo >= 0 && cuenta >= maximo) break;
    }
    if (opc_c) {
        char t[300];
        if (con_nombre > 0) formatar_texto(t, sizeof t, "%s:%d", nome, cuenta);
        else formatar_texto(t, sizeof t, "%d", cuenta);
        saida_linha(t, -1);
    }
    if (opc_l && cuenta) saida_linha(nome, -1);
    if (opc_L && !cuenta) saida_linha(nome, -1);
    return cuenta > 0;
}

static int buscar_ruta(const char *ruta, bool explicita, int prof)
{
    info_arquivo_t inf;
    int s = info_arquivo(ruta, &inf);
    if (s < 0) {
        if (!opc_s) imprimir_em(ERROS, "grep: %s: %s\n", ruta, nome_erro(s));
        hubo_error = true;
        return 0;
    }
    if (inf.tipo == TIPO_PASTA) {
        if (!opc_r) {
            if (!opc_s) imprimir_em(ERROS, "grep: %s: es una carpeta (usa -r)\n", ruta);
            hubo_error = true;
            return 0;
        }
        if (prof > 24) return 0;
        entrada_pasta_t *v;
        int n = listar_tudo(ruta, &v);
        if (n < 0) { hubo_error = true; return 0; }
        for (int i = 0; i < n; i++) {
            char h[256];
            if (compara_texto(ruta, ".") == 0 && !explicita) copia_texto(h, v[i].nome, sizeof h);
            else caminho_juntar(h, sizeof h, ruta, v[i].nome);
            if (buscar_ruta(h, false, prof + 1) < 0) { libera(v); return -1; }
        }
        libera(v);
        return 0;
    }
    int fd = abrir(ruta, ABRIR_LER);
    if (fd < 0) { if (!opc_s) imprimir_em(ERROS, "grep: %s: %s\n", ruta, nome_erro(fd)); hubo_error = true; return 0; }
    int r = buscar_en(fd, ruta);
    fechar(fd);
    return r < 0 ? -1 : 0;
}

/* Compone el patrón con -x o -w para el motor de expresiones. */
static int compilar(int k, const char *p)
{
    static char buf[300];
    const char *a = "", *b = "";
    if (opc_x) { a = opc_E ? "^(" : "^\\("; b = opc_E ? ")$" : "\\)$"; }
    else if (opc_w) { a = opc_E ? "\\b(" : "\\b\\("; b = opc_E ? ")\\b" : "\\)\\b"; }
    formatar_texto(buf, sizeof buf, "%s%s%s", a, p, b);
    if (er_compilar(&er[k], buf, opc_E, opc_i) < 0) {
        imprimir_em(ERROS, "grep: patrón no válido «%s»: %s\n", p, er[k].erro);
        return -1;
    }
    return 0;
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    static const opcao_longa_t largas[] = {
        { "ignore-case", 'i' }, { "invert-match", 'v' }, { "line-number", 'n' }, { "count", 'c' },
        { "files-with-matches", 'l' }, { "recursive", 'r' }, { "extended-regexp", 'E' },
        { "fixed-strings", 'F' }, { "word-regexp", 'w' }, { "line-regexp", 'x' }, { "only-matching", 'o' },
        { "with-filename", 'H' }, { "no-filename", 'h' }, { "quiet", 'q' }, { "max-count", 'm' }, { NULL, 0 } };
    o.longas = largas;
    int r, v;
    while ((r = obter_opcao(argc, argv, "ivnclLrREFwxoHhqsm:e:A:B:C:", &o)) >= 0) {
        switch (r) {
        case 'i': opc_i = true; break;
        case 'v': opc_v = true; break;
        case 'n': opc_n = true; break;
        case 'c': opc_c = true; break;
        case 'l': opc_l = true; break;
        case 'L': opc_L = true; break;
        case 'r': case 'R': opc_r = true; break;
        case 'E': opc_E = true; break;
        case 'F': opc_F = true; break;
        case 'w': opc_w = true; break;
        case 'x': opc_x = true; break;
        case 'o': opc_o = true; break;
        case 'H': con_nombre = 1; break;
        case 'h': con_nombre = 0; break;
        case 'q': opc_q = true; break;
        case 's': opc_s = true; break;
        case 'm': if (!texto_para_numero(o.valor, &v)) { imprimir_em(ERROS, "grep: -m: número no válido\n"); return 2; } maximo = v; break;
        case 'A': case 'B': case 'C':
            if (!texto_para_numero(o.valor, &v)) { imprimir_em(ERROS, "grep: -%c: número no válido\n", r); return 2; }
            if (v > CTX_MAX - 1) v = CTX_MAX - 1;
            if (r != 'B') ctx_despues = v;
            if (r != 'A') ctx_antes = v;
            break;
        case 'e':
            if (npats < MAX_PAT) pats[npats++] = (char *)o.valor;
            break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("grep", r, &o);
        }
    }
    int i0 = o.indice;
    if (npats == 0) {
        if (i0 >= argc) { imprimir_em(ERROS, "grep: falta el patrón (uso: grep [opciones] PATRÓN [archivo...]; grep --help)\n"); return 2; }
        pats[npats++] = argv[i0++];
    }
    /* un patrón con saltos de línea son varios patrones */
    for (int k = 0; k < npats && npats < MAX_PAT; k++) {
        char *nl = acha_char(pats[k], '\n');
        if (nl) { *nl = 0; pats[npats++] = nl + 1; }
    }
    if (!opc_F) for (int k = 0; k < npats; k++) if (compilar(k, pats[k]) < 0) return 2;
    int nfiles = argc - i0;
    if (con_nombre < 0) con_nombre = (nfiles > 1 || opc_r) ? 1 : 0;
    if (nfiles == 0 && !opc_r) {
        int rr = buscar_en(ENTRADA, "(entrada estándar)");
        (void)rr;
    } else if (nfiles == 0) {
        buscar_ruta(".", false, 0);
    } else {
        for (int i = i0; i < argc; i++) {
            if (compara_texto(argv[i], "-") == 0) { if (buscar_en(ENTRADA, "(entrada estándar)") < 0) break; continue; }
            if (buscar_ruta(argv[i], true, 0) < 0) break;
        }
    }
    if (hubo_error && !(opc_q && hubo_match)) return 2;
    return hubo_match ? 0 : 1;
}
