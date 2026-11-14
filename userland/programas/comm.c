// SPDX-License-Identifier: GPL-2.0-or-later
/* comm - compara dos archivos ordenados línea a línea (D232). */
#include "sagui.h"

static const char *AYUDA =
    "uso: comm [-123] archivo1 archivo2\n"
    "  Los dos archivos deben estar ordenados (sort). Escribe tres columnas: líneas solo del primero,\n"
    "  solo del segundo y comunes. -1, -2, -3 suprimen la columna correspondiente (comm -12 = solo las comunes).\n";

int principal(int argc, char **argv)
{
    bool c1 = true, c2 = true, c3 = true;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "123", &o)) >= 0) {
        switch (r) {
        case '1': c1 = false; break;
        case '2': c2 = false; break;
        case '3': c3 = false; break;
        case OPC_AJUDA: escrever_texto(SAIDA, AYUDA); return 0;
        default: return opcao_invalida("comm", r, &o);
        }
    }
    if (argc - o.indice != 2) { imprimir_em(ERROS, "uso: comm [-123] archivo1 archivo2  (comm --help)\n"); return 2; }
    int f1 = abrir_entrada("comm", argv[o.indice]);
    if (f1 < 0) return 1;
    int f2 = abrir_entrada("comm", argv[o.indice + 1]);
    if (f2 < 0) return 1;
    static leitor_linhas_t l1, l2;
    leitor_iniciar(&l1, f1, false);
    leitor_iniciar(&l2, f2, false);
    static char a[2048], b[2048], t[4200];
    int na = leitor_linha(&l1, a, sizeof a, NULL), nb = leitor_linha(&l2, b, sizeof b, NULL);
    const char *p2 = c1 ? "\t" : "";
    char pref3[3];
    pref3[0] = 0;
    if (c1) copia_texto(pref3, "\t", sizeof pref3);
    if (c2) junta_texto(pref3, "\t", sizeof pref3);
    while (na >= 0 || nb >= 0) {
        int c = na < 0 ? 1 : nb < 0 ? -1 : compara_texto(a, b);
        if (c < 0) {
            if (c1 && saida_linha(a, na) < 0) return 0;
            na = leitor_linha(&l1, a, sizeof a, NULL);
        } else if (c > 0) {
            if (c2) { formatar_texto(t, sizeof t, "%s%s", p2, b); if (saida_linha(t, (int)tam_texto(t)) < 0) return 0; }
            nb = leitor_linha(&l2, b, sizeof b, NULL);
        } else {
            if (c3) { formatar_texto(t, sizeof t, "%s%s", pref3, a); if (saida_linha(t, (int)tam_texto(t)) < 0) return 0; }
            na = leitor_linha(&l1, a, sizeof a, NULL);
            nb = leitor_linha(&l2, b, sizeof b, NULL);
        }
    }
    fechar_entrada(f1);
    fechar_entrada(f2);
    return 0;
}
