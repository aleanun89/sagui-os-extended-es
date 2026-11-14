// SPDX-License-Identifier: GPL-2.0-or-later
/* watch - repite una orden cada cierto tiempo y muestra su salida (D232). */
#include "execucao.h"

static const char *AYUDA =
    "uso: watch [-n SEGUNDOS] [-t] [-c VECES] ORDEN [argumentos...]\n"
    "  Limpia la pantalla, ejecuta la orden, espera (2 s por defecto) y vuelve a empezar. Ctrl+C sale.\n"
    "  -n S  segundos entre ejecuciones (admite decimales: 0.5)   -t  sin la cabecera\n"
    "  -c N  se detiene tras N ejecuciones (útil en scripts y pruebas)\n"
    "  Solo vale para programas (los comandos internos del shell no).\n";

int principal(int argc, char **argv)
{
    unsigned ms = 2000;
    int veces = -1;
    bool sin_cab = false;
    int i = 1;
    for (; i < argc && argv[i][0] == '-' && argv[i][1]; i++) {
        const char *a = argv[i];
        if (compara_texto(a, "--help") == 0 || compara_texto(a, "--ayuda") == 0) { escrever_texto(SAIDA, AYUDA); return 0; }
        else if (compara_texto(a, "-t") == 0) sin_cab = true;
        else if ((compara_texto(a, "-n") == 0 || compara_texto(a, "-c") == 0) && i + 1 < argc) {
            const char *v = argv[++i];
            if (a[1] == 'n') {
                unsigned e = 0, d = 0;
                while (*v >= '0' && *v <= '9') e = e * 10 + (unsigned)(*v++ - '0');
                if (*v == '.' && v[1] >= '0' && v[1] <= '9') d = (unsigned)(v[1] - '0') * 100;
                ms = e * 1000 + d;
                if (ms < 100) ms = 100;
            } else if (!texto_para_numero(v, &veces) || veces < 1) { imprimir_em(ERROS, "watch: -c: número no válido\n"); return 2; }
        } else { imprimir_em(ERROS, "watch: opción desconocida: %s (watch --help)\n", a); return 2; }
    }
    if (i >= argc) { imprimir_em(ERROS, "uso: watch [-n SEGUNDOS] ORDEN [argumentos...]  (watch --help)\n"); return 2; }
    bool term = eh_terminal(SAIDA);
    for (int k = 0; veces < 0 || k < veces; k++) {
        if (term) limpar_tela();
        if (!sin_cab) {
            fuso_data_t d;
            agora_local(&d);
            imprimir("Cada %u,%us:", ms / 1000, ms % 1000 / 100);
            for (int j = i; j < argc; j++) imprimir(" %s", argv[j]);
            imprimir("    %02u:%02u:%02u\n\n", d.hora, d.minuto, d.segundo);
        }
        int pid = lanzar_orden("watch", &argv[i]);
        if (pid < 0) return 127;
        int c;
        esperar(pid, &c);
        if (veces < 0 || k + 1 < veces) dormir(ms);
    }
    return 0;
}
