// SPDX-License-Identifier: GPL-3.0-only
/* rm (apagar) - borra archivos y, con -r, carpetas (D097).
 * En el pendrive (/usb) pregunta antes de borrar (PRD 3.2) salvo con -f. */
#include "sagui.h"

static bool recursivo, forzar, interactivo, verboso, carpetas_vacias;
static int erros;

static const char *AYUDA =
    "uso: rm [-r] [-f] [-i] [-v] [-d] archivo...\n"
    "  -r, -R  borra carpetas y todo lo que contienen\n"
    "  -f      no se queja si no existe y no pregunta nada\n"
    "  -i      pregunta antes de cada borrado      -v  explica lo que borra\n"
    "  -d      borra también carpetas vacías (como rmdir)\n"
    "En /usb pregunta siempre antes de borrar (salvo con -f).\n";

static bool preguntar(const char *texto, const char *c)
{
    char r[8];
    imprimir("%s %s%s? (s/n) ", texto, c, comeca_com(texto, "¿Borrar") ? " del pendrive" : "");
    return ler_linha(ENTRADA, r, sizeof r) >= 0 && (r[0] == 's' || r[0] == 'S' || r[0] == 'y' || r[0] == 'Y');
}

static bool en_usb(const char *c)
{
    char d[160];
    if (comeca_com(c, "/usb/") || compara_texto(c, "/usb") == 0) return true;
    if (c[0] == '/') return false;
    pasta_atual(d, sizeof d);
    return compara_texto(d, "/usb") == 0 || comeca_com(d, "/usb/");
}

static int borrar(const char *c, int prof, bool confirmado)
{
    info_arquivo_t inf;
    int s = info_arquivo(c, &inf);
    if (s < 0) {
        if (!forzar) { imprimir_em(ERROS, "rm: %s: %s\n", c, nome_erro(s)); return 1; }
        return 0;
    }
    if (inf.tipo == TIPO_PASTA) {
        if (!recursivo && !carpetas_vacias) { imprimir_em(ERROS, "rm: %s: es una carpeta (usa -r)\n", c); return 1; }
        int st = 0;
        if (recursivo) {
            if (prof > 24) { imprimir_em(ERROS, "rm: %s: carpetas demasiado anidadas\n", c); return 1; }
            entrada_pasta_t *v;
            int n = listar_tudo(c, &v);
            if (n < 0) { imprimir_em(ERROS, "rm: %s: %s\n", c, nome_erro(n)); return 1; }
            for (int i = 0; i < n; i++) {
                char h[256];
                caminho_juntar(h, sizeof h, c, v[i].nome);
                st |= borrar(h, prof + 1, true);
            }
            libera(v);
            if (st) return 1;
        }
        if (interactivo && !forzar && !preguntar("rm: ¿borrar la carpeta", c)) return 0;
        int r = remover_pasta(c);
        if (r < 0) { imprimir_em(ERROS, "rm: %s: %s\n", c, nome_erro(r)); return 1; }
        if (verboso) imprimir("carpeta '%s' borrada\n", c);
        return 0;
    }
    if (interactivo && !forzar && !preguntar("rm: ¿borrar", c)) return 0;
    if (!confirmado && !forzar && !interactivo && en_usb(c) && !preguntar("¿Borrar", c)) {
        imprimir("No se ha borrado.\n");
        return 0;
    }
    int r = apagar(c);
    if (r < 0) {
        if (!forzar) imprimir_em(ERROS, "rm: %s: %s\n", c, nome_erro(r));
        return forzar ? 0 : 1;
    }
    if (verboso) imprimir("'%s' borrado\n", c);
    return 0;
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "rRfivd", &o)) >= 0) {
        switch (r) {
        case 'r': case 'R': recursivo = true; break;
        case 'f': forzar = true; break;
        case 'i': interactivo = true; break;
        case 'v': verboso = true; break;
        case 'd': carpetas_vacias = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("rm", r, &o);
        }
    }
    if (o.indice >= argc) {
        if (forzar) return 0;
        imprimir_em(ERROS, "rm: falta un archivo (uso: rm [-rfiv] archivo...; rm --help)\n");
        return 2;
    }
    for (int i = o.indice; i < argc; i++) {
        const char *c = argv[i];
        if (compara_texto(c, "/") == 0) { imprimir_em(ERROS, "rm: no se borra la raíz /\n"); erros++; continue; }
        const char *b = nome_base(c);
        if (compara_texto(b, ".") == 0 || compara_texto(b, "..") == 0) {
            imprimir_em(ERROS, "rm: no se pueden borrar '.' ni '..'\n");
            erros++;
            continue;
        }
        /* con -r en el pendrive se pregunta una sola vez por la carpeta */
        bool confirmado = false;
        if (recursivo && !forzar && !interactivo && en_usb(c)) {
            info_arquivo_t inf;
            if (info_arquivo(c, &inf) == 0 && inf.tipo == TIPO_PASTA) {
                if (!preguntar("¿Borrar la carpeta", c)) { imprimir("No se ha borrado.\n"); continue; }
                confirmado = true;
            }
        }
        erros += borrar(c, 0, confirmado);
    }
    return erros ? 1 : 0;
}
