// SPDX-License-Identifier: GPL-2.0-or-later
/* find (achar) - busca archivos y carpetas por nombre, tipo, tamaño... (D097).
 * find [ruta...] [condiciones] [acción]   (sin ruta, busca en .) */
#include "sagui.h"

static const char *AYUDA =
    "uso: find [ruta...] [condiciones] [acción]\n"
    "Condiciones (se combinan con Y):\n"
    "  -name PATRÓN   el nombre casa con el patrón (* ? [abc]); -iname ignora mayúsculas\n"
    "  -path PATRÓN   la ruta completa casa con el patrón\n"
    "  -type f|d      solo archivos (f) o carpetas (d)\n"
    "  -size [+-]N[ckMG]  tamaño: +N mayor, -N menor (c bytes, k KB, M MB; sin letra = bloques de 512)\n"
    "  -empty         archivos de 0 bytes o carpetas vacías\n"
    "  -maxdepth N / -mindepth N   profundidad máxima / mínima (0 = la propia ruta)\n"
    "  ! o -not       niega la condición siguiente\n"
    "Acciones: -print (por defecto), -print0, -delete, -exec ORDEN {} ; (ejecuta ORDEN por cada resultado)\n";

#define MAX_COND 16
enum { C_NAME, C_INAME, C_PATH, C_TYPE, C_SIZE, C_EMPTY };
typedef struct { int tipo; bool negar; const char *texto; int signo; unsigned valor; } cond_t;
static cond_t conds[MAX_COND];
static int nconds, maxprof = 1000, minprof;
static bool hacer_delete, imprimir0, hay_accion;
static char **exec_args;
static int exec_n;
static int erros;

static int minus(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : (c >= 0xC0 && c <= 0xDE && c != 0xD7) ? c + 32 : c; }

static bool glob_i(const char *p, const char *t)
{
    char a[256], b[256];
    size_t i = 0;
    for (; p[i] && i < sizeof a - 1; i++) a[i] = (char)minus((unsigned char)p[i]);
    a[i] = 0;
    for (i = 0; t[i] && i < sizeof b - 1; i++) b[i] = (char)minus((unsigned char)t[i]);
    b[i] = 0;
    return casar_glob(a, b);
}

static bool cumple(const char *ruta, const char *nome, unsigned tipo, unsigned tam)
{
    for (int i = 0; i < nconds; i++) {
        const cond_t *c = &conds[i];
        bool ok = false;
        switch (c->tipo) {
        case C_NAME: ok = casar_glob(c->texto, nome); break;
        case C_INAME: ok = glob_i(c->texto, nome); break;
        case C_PATH: ok = casar_glob(c->texto, ruta); break;
        case C_TYPE: ok = (c->texto[0] == 'd') == (tipo == TIPO_PASTA); break;
        case C_SIZE: ok = c->signo > 0 ? tam > c->valor : c->signo < 0 ? tam < c->valor : tam == c->valor; break;
        case C_EMPTY:
            if (tipo == TIPO_PASTA) {
                entrada_pasta_t e;
                ok = listar(ruta, 0, &e) == 0;
            } else ok = tam == 0;
            break;
        }
        if (ok == c->negar) return false;
    }
    return true;
}

static void accion(const char *ruta, unsigned tipo)
{
    if (exec_n) {
        char *args[40];
        int k = 0;
        for (int i = 0; i < exec_n && k < 38; i++) args[k++] = compara_texto(exec_args[i], "{}") == 0 ? (char *)ruta : exec_args[i];
        args[k] = NULL;
        char prog[160];
        if (acha_char(args[0], '/')) copia_texto(prog, args[0], sizeof prog);
        else formatar_texto(prog, sizeof prog, "/bin/%s", args[0]);
        int pid = executar(prog, args), c = 1;
        if (pid <= 0) { imprimir_em(ERROS, "find: -exec %s: %s\n", args[0], nome_erro(pid)); erros++; return; }
        esperar(pid, &c);
        if (c != 0) erros++;
        return;
    }
    if (hacer_delete) {
        int r = tipo == TIPO_PASTA ? remover_pasta(ruta) : apagar(ruta);
        if (r < 0) { imprimir_em(ERROS, "find: %s: %s\n", ruta, nome_erro(r)); erros++; }
        return;
    }
    if (imprimir0) { escrever_tudo(SAIDA, ruta, (unsigned)tam_texto(ruta)); escrever_tudo(SAIDA, "", 1); }
    else if (saida_linha(ruta, -1) < 0) sair(erros ? 1 : 0);
}

static void recorrer(const char *ruta, const char *nome, unsigned tipo, unsigned tam, int prof)
{
    bool pasta = tipo == TIPO_PASTA;
    /* -delete borra el contenido antes que la carpeta */
    if (pasta && prof < maxprof && prof < 40) {
        entrada_pasta_t *v;
        int n = listar_tudo(ruta, &v);
        if (n < 0) { imprimir_em(ERROS, "find: %s: %s\n", ruta, nome_erro(n)); erros++; }
        else {
            if (!hacer_delete && prof >= minprof && cumple(ruta, nome, tipo, tam)) accion(ruta, tipo);
            for (int i = 0; i < n; i++) {
                char h[256];
                caminho_juntar(h, sizeof h, ruta, v[i].nome);
                recorrer(h, v[i].nome, v[i].tipo, v[i].tamanho, prof + 1);
            }
            libera(v);
            if (hacer_delete && prof >= minprof && cumple(ruta, nome, tipo, tam)) accion(ruta, tipo);
            return;
        }
    }
    if (prof >= minprof && prof <= maxprof && cumple(ruta, nome, tipo, tam)) accion(ruta, tipo);
}

int principal(int argc, char **argv)
{
    int rutas_n = 0;
    char *rutas[16];
    int i = 1;
    while (i < argc && argv[i][0] != '-' && compara_texto(argv[i], "!") != 0 && rutas_n < 16) rutas[rutas_n++] = argv[i++];
    bool negar = false;
    for (; i < argc; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--help") == 0 || compara_texto(a, "--ayuda") == 0 || compara_texto(a, "-h") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
        if (compara_texto(a, "!") == 0 || compara_texto(a, "-not") == 0) { negar = !negar; continue; }
        if (compara_texto(a, "-print") == 0) { hay_accion = true; continue; }
        if (compara_texto(a, "-print0") == 0) { imprimir0 = true; continue; }
        if (compara_texto(a, "-delete") == 0) { hacer_delete = true; continue; }
        if (compara_texto(a, "-empty") == 0) {
            if (nconds < MAX_COND) { conds[nconds].tipo = C_EMPTY; conds[nconds].negar = negar; nconds++; }
            negar = false;
            continue;
        }
        if (compara_texto(a, "-exec") == 0) {
            exec_args = &argv[i + 1];
            int k = i + 1;
            while (k < argc && compara_texto(argv[k], ";") != 0) k++;
            if (k >= argc) { imprimir_em(ERROS, "find: -exec: falta el «;» final\n"); return 2; }
            exec_n = k - i - 1;
            if (exec_n == 0) { imprimir_em(ERROS, "find: -exec: falta la orden\n"); return 2; }
            i = k;
            continue;
        }
        bool con_valor = !compara_texto(a, "-name") || !compara_texto(a, "-iname") || !compara_texto(a, "-path") ||
                         !compara_texto(a, "-type") || !compara_texto(a, "-size") || !compara_texto(a, "-maxdepth") ||
                         !compara_texto(a, "-mindepth");
        if (!con_valor) { imprimir_em(ERROS, "find: condición desconocida: %s (prueba find --help)\n", a); return 2; }
        if (i + 1 >= argc) { imprimir_em(ERROS, "find: falta el valor de %s\n", a); return 2; }
        const char *v = argv[++i];
        if (!compara_texto(a, "-maxdepth") || !compara_texto(a, "-mindepth")) {
            int n;
            if (!texto_para_numero(v, &n)) { imprimir_em(ERROS, "find: %s: número no válido\n", v); return 2; }
            if (a[2] == 'a') maxprof = n; else minprof = n;
            continue;
        }
        if (nconds >= MAX_COND) { imprimir_em(ERROS, "find: demasiadas condiciones\n"); return 2; }
        cond_t *c = &conds[nconds++];
        c->negar = negar;
        negar = false;
        c->texto = v;
        if (!compara_texto(a, "-name")) c->tipo = C_NAME;
        else if (!compara_texto(a, "-iname")) c->tipo = C_INAME;
        else if (!compara_texto(a, "-path")) c->tipo = C_PATH;
        else if (!compara_texto(a, "-type")) {
            c->tipo = C_TYPE;
            if (v[0] != 'f' && v[0] != 'd') { imprimir_em(ERROS, "find: -type: usa f (archivo) o d (carpeta)\n"); return 2; }
        } else {
            c->tipo = C_SIZE;
            const char *s = v;
            c->signo = *s == '+' ? 1 : *s == '-' ? -1 : 0;
            if (c->signo) s++;
            int n = 0;
            if (!texto_para_numero("0", &n)) return 2;
            const char *d = s;
            int val = 0;
            while (*d >= '0' && *d <= '9') val = val * 10 + (*d++ - '0');
            unsigned mult = 512;
            if (*d == 'c') mult = 1; else if (*d == 'k') mult = 1024; else if (*d == 'M') mult = 1048576; else if (*d == 'G') mult = 1073741824u;
            else if (*d && *d != 'b') { imprimir_em(ERROS, "find: -size %s no válido\n", v); return 2; }
            if (d == s) { imprimir_em(ERROS, "find: -size %s no válido\n", v); return 2; }
            c->valor = (unsigned)val * mult;
            if (c->signo == 0 && mult != 1) c->signo = 0;   /* igual exacto: aproximación en bloques */
        }
    }
    (void)hay_accion;
    if (rutas_n == 0) rutas[rutas_n++] = ".";
    for (int k = 0; k < rutas_n; k++) {
        info_arquivo_t inf;
        int s = info_arquivo(rutas[k], &inf);
        if (s < 0) { imprimir_em(ERROS, "find: %s: %s\n", rutas[k], nome_erro(s)); erros++; continue; }
        char ruta[256];
        copia_texto(ruta, rutas[k], sizeof ruta);
        size_t n = tam_texto(ruta);
        while (n > 1 && ruta[n - 1] == '/') ruta[--n] = 0;
        recorrer(ruta, nome_base(ruta), inf.tipo, inf.tamanho, 0);
    }
    return erros ? 1 : 0;
}
