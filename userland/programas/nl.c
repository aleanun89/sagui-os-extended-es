// SPDX-License-Identifier: GPL-2.0-or-later
/* nl - numera las líneas (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: nl [-b a|t|n] [-n ln|rn|rz] [-w ANCHO] [-s SEPARADOR] [-v INICIO] [-i PASO] [archivo...]\n"
    "  -b a  numera todas las líneas   -b t  solo las no vacías (por defecto)   -b n  ninguna\n"
    "  -n rn  número a la derecha (por defecto)   -n ln  a la izquierda   -n rz  a la derecha con ceros\n"
    "  -w N  ancho del número (6)   -s S  texto entre el número y la línea (tabulador)\n"
    "  -v N  primer número (1)      -i N  incremento (1)\n";

int principal(int argc, char **argv)
{
    char modo = 't';
    const char *form = "rn", *sep = "\t";
    int ancho = 6, ini = 1, paso = 1;
    opcoes_t o = { 0 };
    int r, codigo = 0;
    while ((r = obter_opcao(argc, argv, "b:n:w:s:v:i:", &o)) >= 0) {
        switch (r) {
        case 'b': modo = o.valor[0]; if (modo != 'a' && modo != 't' && modo != 'n') { imprimir_em(ERROS, "nl: -b: use a, t o n\n"); return 2; } break;
        case 'n': form = o.valor; if (compara_texto(form, "ln") && compara_texto(form, "rn") && compara_texto(form, "rz")) { imprimir_em(ERROS, "nl: -n: use ln, rn o rz\n"); return 2; } break;
        case 'w': if (!texto_para_numero(o.valor, &ancho) || ancho < 1 || ancho > 30) { imprimir_em(ERROS, "nl: -w: de 1 a 30\n"); return 2; } break;
        case 's': sep = o.valor; break;
        case 'v': ini = texto_para_int(o.valor); break;
        case 'i': if (!texto_para_numero(o.valor, &paso)) { imprimir_em(ERROS, "nl: -i: número no válido\n"); return 2; } break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("nl", r, &o);
        }
    }
    static char linea[8192], sal[8300];
    int num = ini;
    for (int i = o.indice; i < argc || i == o.indice; i++) {
        int fd = abrir_entrada("nl", i < argc ? argv[i] : "-");
        if (fd < 0) { codigo = 1; if (i >= argc) break; continue; }
        leitor_linhas_t l;
        leitor_iniciar(&l, fd, false);
        int n;
        while ((n = leitor_linha(&l, linea, sizeof linea, NULL)) >= 0) {
            int k;
            if (modo == 'a' || (modo == 't' && n > 0)) {
                if (form[0] == 'l') k = formatar_texto(sal, sizeof sal, "%-*d%s%s", ancho, num, sep, linea);
                else if (form[1] == 'z') k = formatar_texto(sal, sizeof sal, "%0*d%s%s", ancho, num, sep, linea);
                else k = formatar_texto(sal, sizeof sal, "%*d%s%s", ancho, num, sep, linea);
                num += paso;
            } else k = formatar_texto(sal, sizeof sal, "%*s%s", ancho + (int)tam_texto(sep), "", linea);
            if (saida_linha(sal, k) < 0) { fechar_entrada(fd); return 0; }
        }
        fechar_entrada(fd);
        if (i >= argc) break;
    }
    return codigo;
}
