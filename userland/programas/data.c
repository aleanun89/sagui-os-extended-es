// SPDX-License-Identifier: GPL-3.0-only
/* data (date) - fecha y hora (D080).
 *
 * El kernel guarda UTC; la hora local sale de la zona del proceso: la variable TZ
 * si existe ("TZ=America/Sao_Paulo date"), si no la del sistema (zona).
 *
 *   data            sábado, 07/11/2026 17:20:05 CET
 *   data -u         la misma hora en UTC
 *   data +FORMATO   con %Y %m %d %H %M %S %Z %z %a %A %b %B %j %s %F %T...
 *   data -d @N      el instante N (segundos desde 1970) en vez de ahora */
#include "sagui.h"

int principal(int argc, char **argv)
{
    bool utc = false;
    const char *formato = "%A, %d/%m/%Y %H:%M:%S %Z";
    uint32_t t = hora_utc(NULL);
    for (int i = 1; i < argc; i++) {
        if (compara_texto(argv[i], "-u") == 0 || compara_texto(argv[i], "--utc") == 0) utc = true;
        else if (argv[i][0] == '+') formato = argv[i] + 1;
        else if (compara_texto(argv[i], "-d") == 0 && i + 1 < argc && argv[i + 1][0] == '@') {
            t = (uint32_t)texto_para_int(argv[++i] + 1);
        } else {
            imprimir_em(ERROS, "uso: date [-u] [-d @segundos] [+formato]\n");
            return 2;
        }
    }
    fuso_data_t d;
    if (utc) {
        fuso_decompor(t, &d);
        copia_texto(d.abrev, "UTC", sizeof d.abrev);
    } else hora_local(t, &d);
    char s[256];
    formatar_data(s, sizeof s, formato, &d);
    imprimir("%s\n", s);
    return 0;
}
