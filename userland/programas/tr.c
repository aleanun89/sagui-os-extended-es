// SPDX-License-Identifier: GPL-2.0-or-later
/* tr - traduce, borra o aprieta caracteres de la entrada (D232).
 *   echo hola | tr a-z A-Z      tr -d '\r' < a.txt      tr -s ' ' < a.txt      tr -cd '[:alnum:]' */
#include "tr_logica.h"

static const char *AYUDA =
    "uso: tr [-cds] CONJUNTO1 [CONJUNTO2]      (lee la entrada estándar)\n"
    "  tr a-z A-Z    cambia cada carácter del conjunto 1 por el que ocupa su sitio en el 2\n"
    "  -d  borra los caracteres del conjunto 1      -s  junta los repetidos seguidos en uno\n"
    "  -c, -C  usa el complemento del conjunto 1 (todo lo que NO está en él)\n"
    "Conjuntos: abc  a-z  \\n \\t \\\\ \\NNN (octal)  [:alpha:] [:digit:] [:alnum:] [:lower:] [:upper:] [:space:] [:blank:] [:punct:]\n"
    "Si el conjunto 2 es más corto, se repite su último carácter.\n";

int principal(int argc, char **argv)
{
    bool del = false, sq = false, comp = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "dscC", &o)) >= 0) {
        switch (r) {
        case 'd': del = true; break;
        case 's': sq = true; break;
        case 'c': case 'C': comp = true; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("tr", r, &o);
        }
    }
    int n = argc - o.indice;
    if (n < 1 || n > 2) { imprimir_em(ERROS, "tr: faltan o sobran conjuntos (tr --help)\n"); return 2; }
    static tr_t t;
    char err[96];
    if (tr_preparar(&t, argv[o.indice], n == 2 ? argv[o.indice + 1] : NULL, del, sq, comp, err, sizeof err) < 0) {
        imprimir_em(ERROS, "tr: %s\n", err);
        return 2;
    }
    static unsigned char in[4096], out[4096];
    int k;
    while ((k = ler(ENTRADA, in, sizeof in)) > 0) {
        int m = 0;
        for (int i = 0; i < k; i++) {
            int c = tr_byte(&t, in[i]);
            if (c >= 0) out[m++] = (unsigned char)c;
        }
        if (m && escrever_tudo(SAIDA, out, (unsigned)m) < 0) return 0;
    }
    return 0;
}
