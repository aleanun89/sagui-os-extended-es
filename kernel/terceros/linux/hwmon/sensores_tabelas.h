// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Tabla de desvío Tctl/Tdie de los Ryzen (SaguiOS, D092, D109, D259).
 *
 * Datos tomados como referencia de drivers/hwmon/k10temp.c de Linux (v6.6),
 * cuya cabecera original es «GPL-2.0-or-later» (compatible con la GPL v3 de
 * SaguiOS): desvío Tctl/Tdie de los Ryzen 1000/2000 y Threadripper.
 * Copyright (c) Clemens Ladisch y otros, según el archivo original.
 *
 * La tabla de TjMax de Intel que había aquí (coretemp.c, GPL-2.0-only) se
 * sustituyó en la D259 por el criterio de coretemp(4) de FreeBSD:
 * kernel/terceros/bsd/coretemp/. Con `make SEM_GPL=1` este archivo no entra
 * en la compilación (no se calcula el Tdie). Solo hay datos: la lógica del
 * monitor es propia (kernel/dispositivos/sensores.c). Ver docs/terceros.md.
 */
#ifndef TERCEIROS_LINUX_HWMON_SENSORES_TABELAS_H
#define TERCEIROS_LINUX_HWMON_SENSORES_TABELAS_H

/* Tctl - Tdie en milesimas de grado, por el nombre comercial; 0 = sin desvio. */
static inline int tabela_linux_amd_desvio(const char *marca)
{
    static const struct { const char *nome; int desvio; } tabela[] = {
        { "AMD Ryzen 5 1600X", 20000 },
        { "AMD Ryzen 7 1700X", 20000 },
        { "AMD Ryzen 7 1800X", 20000 },
        { "AMD Ryzen 7 2700X", 10000 },
        { "AMD Ryzen Threadripper 19", 27000 },
        { "AMD Ryzen Threadripper 29", 27000 },
    };
    for (unsigned i = 0; i < sizeof tabela / sizeof tabela[0]; i++) {
        for (const char *s = marca; *s; s++) {
            int k = 0;
            while (tabela[i].nome[k] && s[k] == tabela[i].nome[k]) k++;
            if (!tabela[i].nome[k]) return tabela[i].desvio;
        }
    }
    return 0;
}

#endif
