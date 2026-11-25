// SPDX-License-Identifier: GPL-3.0-only
/* cp (copiar) - copia archivos y carpetas (D097).
 * cp [-rinvfpa] origen destino   |   cp [-rinvfpa] origen... carpeta */
#include "sagui.h"

static bool recursivo, interactivo, no_pisar, verboso, forzar, conservar;
static int erros;

static const char *AYUDA =
    "uso: cp [-r] [-i] [-n] [-v] [-f] origen destino\n"
    "     cp [opciones] origen... carpeta\n"
    "  -r, -R  copia carpetas enteras      -i  pregunta antes de sobrescribir\n"
    "  -n      no sobrescribe lo que existe  -v  explica lo que va copiando\n"
    "  -f      sobrescribe sin preguntar (anula -i)\n"
    "  -p      conserva la fecha de modificación y los permisos   -a  como -r -p\n"
    "Si el destino es una carpeta, copia dentro de ella con el mismo nombre.\n";

static bool es_pasta(const char *c, unsigned *tam)
{
    info_arquivo_t inf;
    if (info_arquivo(c, &inf) < 0) return false;
    if (tam) *tam = inf.tamanho;
    return inf.tipo == TIPO_PASTA;
}

static bool existe(const char *c) { info_arquivo_t inf; return info_arquivo(c, &inf) == 0; }

static bool confirmar(const char *c)
{
    char r[8];
    imprimir("cp: ¿sobrescribir %s? (s/n) ", c);
    return ler_linha(ENTRADA, r, sizeof r) >= 0 && (r[0] == 's' || r[0] == 'S' || r[0] == 'y' || r[0] == 'Y');
}

/* -p (D341): la fecha de modificación (y la de acceso, si la hay) y los permisos del origen. En un
 * sistema sin fechas fijables (ISO 9660) se sigue en silencio. */
static int conservar_datos(const char *org, const char *dst)
{
    info_permisos_t p;
    if (permisos_arquivo(org, &p) == 0 && p.com_donos) mudar_permisos(dst, p.modo);
    datas_arquivo_t d;
    if (datas_arquivo(org, &d) < 0 || !d.modificado) return 0;
    int r = fixar_datas(dst, d.modificado, d.acessado ? d.acessado : PARQ_DATA_MANTER);
    if (r < 0 && r != ERRO_NAO_SUPORTADO) {
        imprimir_em(ERROS, "cp: %s: no se pudo conservar la fecha: %s\n", dst, nome_erro(r));
        return 1;
    }
    return 0;
}

static int copiar_arquivo(const char *org, const char *dst)
{
    if (existe(dst)) {
        if (no_pisar) return 0;
        if (interactivo && !forzar && !confirmar(dst)) return 0;
    }
    int o = abrir(org, ABRIR_LER);
    if (o < 0) { imprimir_em(ERROS, "cp: %s: %s\n", org, nome_erro(o)); return 1; }
    int d = abrir(dst, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (d < 0) { imprimir_em(ERROS, "cp: %s: %s\n", dst, nome_erro(d)); fechar(o); return 1; }
    /* 1 MB por petición: cada lectura o escritura en un pendrive USB cuesta tres
     * transferencias además de los datos, y con 1 KB la copia iba a 50 KB/s (D095); la
     * escritura en un pendrive va directa, así que el tamaño de la petición es el de las
     * órdenes (hasta el trozo del pendrive, 240 KB o 1 MB, D468; antes 128 KB).
     * Estático: la pila del programa solo tiene 64 KB. */
    static char buf[1024 * 1024];
    int n, st = 0;
    while ((n = ler(o, buf, sizeof buf)) > 0) {
        int w = escrever_tudo(d, buf, (unsigned)n);
        if (w < 0) { imprimir_em(ERROS, "cp: %s: %s\n", dst, nome_erro(w)); st = 1; break; }
    }
    if (n < 0) { imprimir_em(ERROS, "cp: %s: %s\n", org, nome_erro(n)); st = 1; }
    fechar(o);
    fechar(d);
    if (conservar && !st) st = conservar_datos(org, dst);
    if (verboso && !st) imprimir("'%s' -> '%s'\n", org, dst);
    return st;
}

static int copiar_arvore(const char *org, const char *dst, int prof)
{
    if (prof > 24) { imprimir_em(ERROS, "cp: %s: carpetas demasiado anidadas\n", org); return 1; }
    if (!es_pasta(org, NULL)) return copiar_arquivo(org, dst);
    if (!recursivo) { imprimir_em(ERROS, "cp: %s: es una carpeta (usa -r para copiarla)\n", org); return 1; }
    if (!es_pasta(dst, NULL)) {
        int r = criar_pasta(dst);
        if (r < 0) { imprimir_em(ERROS, "cp: %s: %s\n", dst, nome_erro(r)); return 1; }
        if (verboso) imprimir("'%s' -> '%s'\n", org, dst);
    }
    entrada_pasta_t *v;
    int n = listar_tudo(org, &v), st = 0;
    if (n < 0) { imprimir_em(ERROS, "cp: %s: %s\n", org, nome_erro(n)); return 1; }
    for (int i = 0; i < n; i++) {
        char a[256], b[256];
        caminho_juntar(a, sizeof a, org, v[i].nome);
        caminho_juntar(b, sizeof b, dst, v[i].nome);
        /* no se copia la carpeta de destino dentro de sí misma */
        if (compara_texto(a, dst) == 0) continue;
        st |= copiar_arvore(a, b, prof + 1);
    }
    libera(v);
    if (conservar) st |= conservar_datos(org, dst);   /* después del contenido, que la cambia */
    return st;
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "rRinvfpa", &o)) >= 0) {
        switch (r) {
        case 'r': case 'R': recursivo = true; break;
        case 'p': conservar = true; break;
        case 'a': recursivo = conservar = true; break;

        case 'i': interactivo = true; break;
        case 'n': no_pisar = true; break;
        case 'v': verboso = true; break;
        case 'f': forzar = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("cp", r, &o);
        }
    }
    int nargs = argc - o.indice;
    if (nargs < 2) {
        imprimir_em(ERROS, "cp: faltan argumentos (uso: cp [-rinvf] origen destino; cp --help)\n");
        return 2;
    }
    const char *destino = argv[argc - 1];
    bool destino_pasta = es_pasta(destino, NULL);
    if (nargs > 2 && !destino_pasta) {
        imprimir_em(ERROS, "cp: %s: no es una carpeta\n", destino);
        return 1;
    }
    for (int i = o.indice; i < argc - 1; i++) {
        char alvo[256];
        if (destino_pasta) caminho_juntar(alvo, sizeof alvo, destino, nome_base(argv[i]));
        else copia_texto(alvo, destino, sizeof alvo);
        if (compara_texto(argv[i], alvo) == 0) {
            imprimir_em(ERROS, "cp: %s y %s son el mismo archivo\n", argv[i], alvo);
            erros++;
            continue;
        }
        /* copiar una carpeta dentro de sí misma */
        size_t n = tam_texto(argv[i]);
        if (recursivo && es_pasta(argv[i], NULL) && compara_texto_n(alvo, argv[i], n) == 0 && alvo[n] == '/') {
            imprimir_em(ERROS, "cp: no se puede copiar %s dentro de sí misma (%s)\n", argv[i], alvo);
            erros++;
            continue;
        }
        if (!existe(argv[i])) { imprimir_em(ERROS, "cp: %s: no existe\n", argv[i]); erros++; continue; }
        erros += copiar_arvore(argv[i], alvo, 0);
    }
    return erros ? 1 : 0;
}
