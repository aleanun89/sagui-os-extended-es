// SPDX-License-Identifier: GPL-2.0-or-later
/* stat (estado) - datos de un archivo o carpeta (D097). */
#include "sagui.h"

static const char *AYUDA =
    "uso: stat [-c FORMATO] archivo...\n"
    "Muestra nombre, tipo, tamaño y las fechas que guarda el sistema de archivos (hora local).\n"
    "  -c  solo lo pedido: %n nombre, %s tamaño, %Y modificación, %X acceso (segundos UTC desde 1970)\n";

/* D341: stat -c "%Y %n", como el de GNU (lo justo para scripts y pruebas). */
static void com_formato(const char *f, const char *nome, const info_arquivo_t *inf, const datas_arquivo_t *d)
{
    for (; *f; f++) {
        if (*f != '%' || !f[1]) { imprimir("%c", *f); continue; }
        switch (*++f) {
        case 'n': imprimir("%s", nome); break;
        case 's': imprimir("%u", inf->tamanho); break;
        case 'Y': imprimir("%u", d->modificado); break;
        case 'X': imprimir("%u", d->acessado); break;
        case '%': imprimir("%%"); break;
        default: imprimir("%%%c", *f);
        }
    }
    imprimir("\n");
}

static void fecha(const char *rotulo, uint32_t utc)
{
    if (!utc) { imprimir("%s sin dato\n", rotulo); return; }
    fuso_data_t l;
    char t[48];
    hora_local(utc, &l);
    formatar_data(t, sizeof t, "%Y-%m-%d %H:%M:%S %Z", &l);
    imprimir("%s %s\n", rotulo, t);
}

int principal(int argc, char **argv)
{
    opcoes_t o = { 0 };
    int r, erros = 0;
    const char *formato = NULL;
    while ((r = obter_opcao(argc, argv, "c:", &o)) >= 0) {
        if (r == 'c') { formato = o.valor; continue; }
        if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        return opcao_invalida("stat", r, &o);
    }
    if (o.indice >= argc) { imprimir_em(ERROS, "stat: falta el archivo (stat --help)\n"); return 2; }
    for (int i = o.indice; i < argc; i++) {
        info_arquivo_t inf;
        int s = info_arquivo(argv[i], &inf);
        if (s < 0) { imprimir_em(ERROS, "stat: %s: %s\n", argv[i], nome_erro(s)); erros++; continue; }
        if (formato) {
            datas_arquivo_t d = { 0, 0, 0 };
            datas_arquivo(argv[i], &d);
            com_formato(formato, argv[i], &inf, &d);
            continue;
        }

        const char *tipo = inf.tipo == TIPO_PASTA ? "carpeta" : inf.tipo == TIPO_PROGRAMA ? "programa (ejecutable)" :
                           inf.tipo == TIPO_ARQUIVO ? "archivo" : "especial";
        imprimir("  Archivo: %s\n  Tamaño:  %u bytes\n  Tipo:    %s\n", argv[i], inf.tamanho, tipo);
        datas_arquivo_t d;
        if (datas_arquivo(argv[i], &d) == 0) {
            fecha("  Modificado:", d.modificado);
            fecha("  Creado:    ", d.criado);
            fecha("  Acceso:    ", d.acessado);
        }
    }
    return erros ? 1 : 0;
}
