// SPDX-License-Identifier: GPL-2.0-or-later
/* sed - editor de flujo, lo básico (D232): s///, d, p, q, =, a, i, c con direcciones.
 *   sed 's/viejo/nuevo/g' archivo     sed -n '2,4p'     sed -i '/x/d' archivo     ... | sed -e 's/a/b/' -e '$d' */
#include "sed_logica.h"

static const char *AYUDA =
    "uso: sed [-nEr] [-i[SUFIJO]] [-s] (-e GUION | -f ARCHIVO | GUION) [archivo...]\n"
    "  -n  no escribe cada línea sola (solo lo que pida la orden p)   -e  añade un guion (se puede repetir)\n"
    "  -f  lee el guion de un archivo        -E, -r  expresiones extendidas (| + ? ( ))\n"
    "  -i  edita los archivos en su sitio (con SUFIJO guarda antes una copia: -i.bak)\n"
    "Direcciones: N  $  /regex/  N,M  N,/regex/  /regex/,/regex/  y ! para negar (2!d).\n"
    "Órdenes: s/viejo/nuevo/[g][i][p][N]  d  p  q  =  a TEXTO  i TEXTO  c TEXTO  (separadas por ; o salto de línea)\n"
    "En el reemplazo: & es lo encontrado, \\n salto de línea, \\t tabulador. No hay referencias \\1.\n";

typedef struct { char buf[2048]; int n, fd; bool roto; } salida_t;

static int sal_vaciar(salida_t *o)
{
    if (o->n && escrever_tudo(o->fd, o->buf, (unsigned)o->n) < 0) o->roto = true;
    o->n = 0;
    return o->roto ? -1 : 0;
}

static int sal_escribir(void *ctx, const char *s, int n)
{
    salida_t *o = ctx;
    if (o->roto) return -1;
    if (n > (int)sizeof o->buf) { if (sal_vaciar(o) < 0) return -1; if (escrever_tudo(o->fd, s, (unsigned)n) < 0) { o->roto = true; return -1; } return 0; }
    if (o->n + n > (int)sizeof o->buf && sal_vaciar(o) < 0) return -1;
    memcpy(o->buf + o->n, s, (size_t)n);
    o->n += n;
    return 0;
}

typedef struct {
    char **archivos;
    int n, i;
    leitor_linhas_t l;
    bool abierto;
    bool error;
} entrada_t;

static int ent_leer(void *ctx, char *buf, int max)
{
    entrada_t *e = ctx;
    for (;;) {
        if (!e->abierto) {
            if (e->i >= e->n) return -1;
            const char *nom = e->archivos[e->i++];
            int fd = compara_texto(nom, "-") == 0 ? ENTRADA : abrir(nom, ABRIR_LER);
            if (fd < 0) { imprimir_em(ERROS, "sed: no se puede leer %s: %s\n", nom, nome_erro(fd)); e->error = true; continue; }
            leitor_iniciar(&e->l, fd, false);
            e->abierto = true;
        }
        int r = leitor_linha(&e->l, buf, max, NULL);
        if (r >= 0) return r;
        if (e->l.fd != ENTRADA) fechar(e->l.fd);
        e->abierto = false;
    }
}

int principal(int argc, char **argv)
{
    static sed_t s;
    char err[160], suf[32] = "";
    bool silencio = false, ext = false, local = false, tiene_guion = false;
    char *guiones[32];
    int ng = 0;
    /* -i admite sufijo pegado (-i.bak): se separa antes de analizar las opciones */
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "--") == 0) break;
        if (compara_texto_n(argv[i], "-i", 2) == 0 && argv[i][2]) { copia_texto(suf, argv[i] + 2, sizeof suf); argv[i] = "-i"; }
        else if (compara_texto_n(argv[i], "--in-place", 10) == 0) {
            if (argv[i][10] == '=') copia_texto(suf, argv[i] + 11, sizeof suf);
            argv[i] = "-i";
        }
    }
    opcoes_t o = { 0 };
    static const opcao_longa_t largas[] = { { "quiet", 'n' }, { "silent", 'n' }, { "expression", 'e' }, { "file", 'f' },
                                            { "regexp-extended", 'E' }, { "separate", 's' }, { NULL, 0 } };
    o.longas = largas;
    int r;
    while ((r = obter_opcao(argc, argv, "nEre:f:is", &o)) >= 0) {
        switch (r) {
        case 'n': silencio = true; break;
        case 'E': case 'r': ext = true; break;
        case 'i': local = true; break;
        case 's': break;
        case 'e': if (ng < 32) guiones[ng++] = (char *)o.valor; tiene_guion = true; break;
        case 'f': {
            int fd = abrir(o.valor, ABRIR_LER);
            if (fd < 0) { imprimir_em(ERROS, "sed: no se puede leer %s: %s\n", o.valor, nome_erro(fd)); return 2; }
            unsigned tam;
            char *t = ler_todo_fd(fd, &tam);
            fechar(fd);
            if (!t) { imprimir_em(ERROS, "sed: sin memoria\n"); return 2; }
            if (ng < 32) guiones[ng++] = t;
            tiene_guion = true;
            break;
        }
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("sed", r, &o);
        }
    }
    int i = o.indice;
    if (!tiene_guion) {
        if (i >= argc) { imprimir_em(ERROS, "uso: sed [opciones] GUION [archivo...]  (sed --help)\n"); return 2; }
        guiones[ng++] = argv[i++];
    }
    sed_iniciar(&s, ext, silencio);
    for (int k = 0; k < ng; k++)
        if (sed_compilar(&s, guiones[k], err, sizeof err) < 0) { imprimir_em(ERROS, "sed: -e guion %d: %s\n", k + 1, err); return 1; }
    static char *solo_entrada[] = { "-" };
    char **arch = i < argc ? &argv[i] : solo_entrada;
    int na = i < argc ? argc - i : 1;
    int codigo = 0;
    salida_t sal = { .fd = SAIDA };
    if (!local) {
        entrada_t e = { .archivos = arch, .n = na };
        sed_executar(&s, ent_leer, &e, sal_escribir, &sal);
        sal_vaciar(&sal);
        return e.error ? 2 : 0;
    }
    if (arch == solo_entrada) { imprimir_em(ERROS, "sed: -i necesita archivos\n"); return 2; }
    for (int k = 0; k < na; k++) {
        char tmp[200];
        formatar_texto(tmp, sizeof tmp, "%s.sed-tmp", arch[k]);
        int fd = abrir(tmp, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fd < 0) { imprimir_em(ERROS, "sed: no se puede crear %s: %s\n", tmp, nome_erro(fd)); codigo = 4; continue; }
        entrada_t e = { .archivos = &arch[k], .n = 1 };
        salida_t so = { .fd = fd };
        sed_executar(&s, ent_leer, &e, sal_escribir, &so);
        sal_vaciar(&so);
        fechar(fd);
        if (e.error) { apagar(tmp); codigo = 4; continue; }
        if (suf[0]) {
            char cop[200];
            formatar_texto(cop, sizeof cop, "%s%s", arch[k], suf);
            apagar(cop);
            renomear(arch[k], cop);
        } else apagar(arch[k]);
        int rr = renomear(tmp, arch[k]);
        if (rr < 0) { imprimir_em(ERROS, "sed: no se puede reemplazar %s: %s (el resultado queda en %s)\n", arch[k], nome_erro(rr), tmp); codigo = 4; }
        sed_iniciar(&s, ext, silencio);   /* cada archivo con el estado de rangos limpio */
        for (int q = 0; q < ng; q++) sed_compilar(&s, guiones[q], err, sizeof err);
    }
    return codigo;
}
