#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""gerar_rt2860reg.py - copia sys/dev/ic/rt2860reg.h de OpenBSD (ISC, Damien
Bergamini) a kernel/terceros/bsd/run/rt2860reg.h para el driver de los
Ralink RT2870/RT3070/RT5370 (D409), sin la tabla rt2860_rates (usa tipos de
net80211) ni la cabecera del tramo PCI de descriptores que no se usa:

    tools/gerar_rt2860reg.py RUTA/rt2860reg.h"""
import os
import sys

if len(sys.argv) < 2:
    print(__doc__)
    sys.exit(0)
src = open(sys.argv[1], encoding="latin-1").read()
i = src.index("static const struct rt2860_rate {")
j = src.index("};", i) + 2
src = src[:i] + "/* (la tabla rt2860_rates se quito: usa tipos de net80211; ver kernel/rede/rt2870_logica.h) */" + src[j:]
cab = """/* SPDX-License-Identifier: ISC
 *
 * kernel/terceros/bsd/run/rt2860reg.h - copia de OpenBSD sys/dev/ic/rt2860reg.h
 * (rev. 1.35, Damien Bergamini, ISC; el aviso original sigue debajo): registros,
 * descriptores TXD/TXWI/RXWI y tablas de inicio (MAC, BBP, RF) de los Ralink
 * RT2860/RT2870/RT3070/RT5370 (D409). Solo se quito la tabla de tasas, que usa
 * tipos de net80211. Lo usa el driver propio kernel/rede/rt2870.c. Ver
 * docs/terceros.md. Lo genera tools/gerar_rt2860reg.py. */
#ifndef SAGUI_TERCEROS_RT2860REG_H
#define SAGUI_TERCEROS_RT2860REG_H

"""
out = cab + src + "\n#endif\n"
open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "kernel", "terceros", "bsd", "run",
                  "rt2860reg.h"), "w", encoding="latin-1").write(out)
