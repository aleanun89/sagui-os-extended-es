// SPDX-License-Identifier: GPL-2.0-or-later
/* realpath - ruta absoluta y limpia de un archivo o carpeta (D232). SaguiOS no tiene enlaces simbólicos:
 * solo resuelve ".", ".." y las rutas relativas a la carpeta actual. */
#include "sagui.h"

static const char *AYUDA =
    "uso: realpath [-e] [-m] [-s] ruta...\n"
    "  Escribe la ruta absoluta sin \".\" ni \"..\" (relativa a la carpeta actual). No hay enlaces simbólicos que resolver.\n"
    "  -e  exige que exista (error si no)   -m  no exige que exista (el comportamiento por defecto)\n"
    "  -s  se acepta por compatibilidad\n";

/* normaliza dst (absoluta) quitando . y .. */
static void normalizar(const char *ruta, char *dst, size_t max)
{
    char tmp[256];
    size_t n = 0;
    tmp[0] = 0;
    const char *p = ruta;
    while (*p) {
        while (*p == '/') p++;
        const char *f = p;
        while (*f && *f != '/') f++;
        size_t l = (size_t)(f - p);
        if (l == 0) break;
        if (l == 1 && p[0] == '.') { }
        else if (l == 2 && p[0] == '.' && p[1] == '.') { while (n > 0 && tmp[n - 1] != '/') n--; if (n > 0) n--; tmp[n] = 0; }
        else if (n + l + 2 < sizeof tmp) { tmp[n++] = '/'; memcpy(tmp + n, p, l); n += l; tmp[n] = 0; }
        p = f;
    }
    copia_texto(dst, n ? tmp : "/", max);
}

int principal(int argc, char **argv)
{
    bool existe = false;
    opcoes_t o = { 0 };
    int r, codigo = 0;
    while ((r = obter_opcao(argc, argv, "ems", &o)) >= 0) {
        switch (r) {
        case 'e': existe = true; break;
        case 'm': existe = false; break;
        case 's': break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("realpath", r, &o);
        }
    }
    if (o.indice >= argc) { imprimir_em(ERROS, "uso: realpath ruta...  (realpath --help)\n"); return 2; }
    for (int i = o.indice; i < argc; i++) {
        char completa[400], sal[256];
        if (argv[i][0] == '/') copia_texto(completa, argv[i], sizeof completa);
        else {
            char cwd[200];
            if (pasta_atual(cwd, sizeof cwd) < 0) copia_texto(cwd, "/", sizeof cwd);
            formatar_texto(completa, sizeof completa, "%s/%s", cwd, argv[i]);
        }
        normalizar(completa, sal, sizeof sal);
        if (existe) {
            info_arquivo_t inf;
            int e = info_arquivo(sal, &inf);
            if (e < 0) { imprimir_em(ERROS, "realpath: %s: %s\n", argv[i], nome_erro(e)); codigo = 1; continue; }
        }
        saida_linha(sal, -1);
    }
    return codigo;
}
