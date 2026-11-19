// SPDX-License-Identifier: GPL-2.0-or-later
/* dmesg - el registro del kernel (D232): lo que escribió al arrancar y después. */
#include "sagui.h"

static const char *AYUDA =
    "uso: dmesg [-n LÍNEAS] [BÚSQUEDA]\n"
    "  Muestra el final del registro del kernel (hasta 32 KB). Con -n N, solo las últimas N líneas;\n"
    "  con BÚSQUEDA, las líneas que contienen ese texto (sin distinguir mayúsculas).\n";

static bool contiene(const char *t, int n, const char *b)
{
    int m = (int)tam_texto(b);
    for (int i = 0; i + m <= n; i++) {
        int k = 0;
        while (k < m && ((t[i + k] | 32) == (b[k] | 32))) k++;
        if (k == m) return true;
    }
    return false;
}

int principal(int argc, char **argv)
{
    int ultimas = 0;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "n:", &o)) >= 0) {
        if (r == 'n') { if (!texto_para_numero(o.valor, &ultimas) || ultimas < 1) { imprimir_em(ERROS, "dmesg: -n: número no válido\n"); return 2; } }
        else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("dmesg", r, &o);
    }
    const char *busca = o.indice < argc ? argv[o.indice] : NULL;
    static char log[131072 + 8];   /* el anillo del kernel (128 KB desde D387) */
    int n = info_sistema(INFO_LOG, log, 131072);
    if (n < 0) { imprimir_em(ERROS, "dmesg: no se puede leer el registro: %s\n", nome_erro(n)); return 1; }
    log[n] = 0;
    /* líneas: [ini, fin) */
    static int ini[2000], fin[2000];
    int nl = 0, p = 0;
    while (p < n) {
        int q = p;
        while (q < n && log[q] != '\n') q++;
        if (!busca || contiene(log + p, q - p, busca)) {
            if (nl == 2000) { for (int i = 1; i < 2000; i++) { ini[i - 1] = ini[i]; fin[i - 1] = fin[i]; } nl--; }
            ini[nl] = p; fin[nl] = q; nl++;
        }
        p = q + 1;
    }
    int desde = ultimas && nl > ultimas ? nl - ultimas : 0;
    for (int i = desde; i < nl; i++) if (saida_linha(log + ini[i], fin[i] - ini[i]) < 0) return 0;
    return nl ? 0 : (busca ? 1 : 0);
}
