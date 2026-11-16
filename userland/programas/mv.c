// SPDX-License-Identifier: GPL-3.0-only
/* mv (mover) - mueve o renombra archivos y carpetas (D097).
 * mv [-ivnf] origen destino   |   mv [-ivnf] origen... carpeta */
#include "sagui.h"

static bool interactivo, verboso, no_pisar, forzar;

static const char *AYUDA =
    "uso: mv [-i] [-n] [-v] [-f] origen destino\n"
    "     mv [opciones] origen... carpeta\n"
    "  -i  pregunta antes de sobrescribir   -n  no sobrescribe\n"
    "  -v  explica lo que mueve             -f  sobrescribe sin preguntar\n";

static bool existe(const char *c, bool *pasta)
{
    info_arquivo_t inf;
    if (info_arquivo(c, &inf) < 0) return false;
    if (pasta) *pasta = inf.tipo == TIPO_PASTA;
    return true;
}

static int correr(const char *prog, char *const args[])
{
    int pid = executar(prog, args), c = 1;
    if (pid > 0) esperar(pid, &c);
    return c;
}

static int mover_um(const char *org, const char *dst)
{
    bool pasta_org;
    if (!existe(org, &pasta_org)) { imprimir_em(ERROS, "mv: %s: no existe\n", org); return 1; }
    bool dst_pasta;
    if (existe(dst, &dst_pasta)) {
        if (no_pisar) return 0;
        if (interactivo && !forzar) {
            char r[8];
            imprimir("mv: ¿sobrescribir %s? (s/n) ", dst);
            if (ler_linha(ENTRADA, r, sizeof r) < 0 || (r[0] != 's' && r[0] != 'S' && r[0] != 'y' && r[0] != 'Y')) return 0;
        }
        if (dst_pasta) { imprimir_em(ERROS, "mv: %s: ya existe una carpeta con ese nombre\n", dst); return 1; }
        if (!pasta_org) apagar(dst);
    }
    int r = renomear(org, dst);
    if (r == ERRO_ACESSO) {
        /* Entre la RAM y el pendrive el kernel no renombra: copia (con fechas y permisos, D341) y borra. */
        char *cp[] = { "cp", "-rp", (char *)org, (char *)dst, NULL };

        char *rm[] = { "rm", "-rf", (char *)org, NULL };
        r = ERRO_ACESSO;
        if (correr("/bin/cp", cp) == 0 && correr("/bin/rm", rm) == 0) r = 0;
    }
    if (r < 0) { imprimir_em(ERROS, "mv: %s: %s\n", org, nome_erro(r)); return 1; }
    if (verboso) imprimir("'%s' -> '%s'\n", org, dst);
    return 0;
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "ivnf", &o)) >= 0) {
        switch (r) {
        case 'i': interactivo = true; break;
        case 'v': verboso = true; break;
        case 'n': no_pisar = true; break;
        case 'f': forzar = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("mv", r, &o);
        }
    }
    int nargs = argc - o.indice;
    if (nargs < 2) { imprimir_em(ERROS, "mv: faltan argumentos (uso: mv [-ivnf] origen destino; mv --help)\n"); return 2; }
    const char *destino = argv[argc - 1];
    bool dst_pasta = false;
    existe(destino, &dst_pasta);
    if (nargs > 2 && !dst_pasta) { imprimir_em(ERROS, "mv: %s: no es una carpeta\n", destino); return 1; }
    int erros = 0;
    for (int i = o.indice; i < argc - 1; i++) {
        char alvo[256];
        if (dst_pasta) caminho_juntar(alvo, sizeof alvo, destino, nome_base(argv[i]));
        else copia_texto(alvo, destino, sizeof alvo);
        if (compara_texto(argv[i], alvo) == 0) { imprimir_em(ERROS, "mv: %s y %s son el mismo archivo\n", argv[i], alvo); erros++; continue; }
        erros += mover_um(argv[i], alvo);
    }
    return erros ? 1 : 0;
}
