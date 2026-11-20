#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""gerar_r92creg_92c.py - extrae de OpenBSD sys/dev/ic/r92creg.h las tablas y
estructuras de los Realtek RTL8188CU/RTL8192CU (D409) a
kernel/terceros/bsd/rtwn/r92creg_92c.h. Solo en el anfitrion, al regenerar:

    tools/gerar_r92creg_92c.py RUTA/r92creg.h

Sin cambios en los valores (ISC, Damien Bergamini); se quitan los otros chips."""
import re
import sys
import os

if len(sys.argv) < 2:
    print(__doc__)
    sys.exit(0)
src = open(sys.argv[1], encoding="latin-1").read()


def bloque(inicio_re, fin="};"):
    m = re.search(inicio_re, src)
    if not m:
        sys.exit("no encontrado: " + inicio_re)
    i = src.index(fin, m.start()) + len(fin)
    # retroceder al comienzo de la línea
    a = src.rfind("\n", 0, m.start()) + 1
    return src[a:i]


def struct_packed(nombre):
    m = re.search(r"struct %s \{" % nombre, src)
    i = src.index("} __packed", m.start())
    j = src.index("\n", i) + 1
    return src[m.start():j]


partes = []
partes.append("/* RTL8192CU ROM image (eFuse). */\n" + struct_packed("r92c_rom"))
partes.append("/* Rx PHY descriptor. */\n" + struct_packed("r92c_rx_phystat"))
partes.append("/* Rx PHY CCK descriptor. */\n" + struct_packed("r92c_rx_cck"))
# (rtl8192cu_mac ya esta en r92creg_88e.h, junto a la del 8188EU)
for n in ["rtl8192ce_bb_regs", "rtl8192ce_agc_vals", "rtl8188ce_agc_vals", "rtl8192cu_bb_vals", "rtl8188cu_bb_vals",
          "rtl8188ru_bb_regs", "rtl8188ru_bb_vals", "rtl8188ru_agc_vals"]:
    partes.append(bloque(r"static const uint(16|32)_t %s\[\] = \{" % n))
partes.append("""static const struct r92c_bb_prog rtl8192cu_bb_prog = {
	nitems(rtl8192ce_bb_regs), rtl8192ce_bb_regs, rtl8192cu_bb_vals,
	nitems(rtl8192ce_agc_vals), rtl8192ce_agc_vals
};
static const struct r92c_bb_prog rtl8188cu_bb_prog = {
	nitems(rtl8192ce_bb_regs), rtl8192ce_bb_regs, rtl8188cu_bb_vals,
	nitems(rtl8188ce_agc_vals), rtl8188ce_agc_vals
};
static const struct r92c_bb_prog rtl8188ru_bb_prog = {
	nitems(rtl8188ru_bb_regs), rtl8188ru_bb_regs, rtl8188ru_bb_vals,
	nitems(rtl8188ru_agc_vals), rtl8188ru_agc_vals
};""")
for n in ["rtl8192ce_rf1_regs", "rtl8192ce_rf1_vals", "rtl8192ce_rf2_regs", "rtl8192ce_rf2_vals", "rtl8188cu_rf_vals",
          "rtl8188ru_rf_vals"]:
    partes.append(bloque(r"static const uint(16|32)_t %s\[\] = \{" % n))
partes.append("""static const struct r92c_rf_prog rtl8192cu_rf_prog[] = {
	{ nitems(rtl8192ce_rf1_regs), rtl8192ce_rf1_regs, rtl8192ce_rf1_vals },
	{ nitems(rtl8192ce_rf2_regs), rtl8192ce_rf2_regs, rtl8192ce_rf2_vals }
};
static const struct r92c_rf_prog rtl8188cu_rf_prog[] = {
	{ nitems(rtl8192ce_rf1_regs), rtl8192ce_rf1_regs, rtl8188cu_rf_vals }
};
static const struct r92c_rf_prog rtl8188ru_rf_prog[] = {
	{ nitems(rtl8192ce_rf1_regs), rtl8192ce_rf1_regs, rtl8188ru_rf_vals }
};""")
partes.append("struct r92c_txpwr {\n\tuint8_t\tpwr[3][28];\n};")
partes.append(bloque(r"static const struct r92c_txpwr rtl8192cu_txagc\[\] = \{"))
partes.append(bloque(r"static const struct r92c_txpwr rtl8188ru_txagc\[\] = \{"))

cab = """/*	$OpenBSD: r92creg.h,v 1.32 2026/11/12 00:46:16 kevlo Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2015 Stefan Sperling <stsp@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* SPDX-License-Identifier: ISC
 *
 * kernel/terceros/bsd/rtwn/r92creg_92c.h - extracto de OpenBSD
 * sys/dev/ic/r92creg.h (rev. 1.32): imagen de la eFuse, estado del PHY, y las
 * tablas de inicio (MAC, BB, AGC, RF y potencia) de los Realtek
 * RTL8188CU/RTL8192CU/RTL8188RU (D409). Solo definiciones y datos, sin cambios
 * en los valores; se quitaron los otros chips y las tarjetas minicard (PCIe).
 * Se incluye despues de r92creg_88e.h (que trae los registros y r92c_bb_prog /
 * r92c_rf_prog). Lo genera tools/gerar_r92creg_92c.py. Ver docs/terceros.md. */
#ifndef SAGUI_TERCEROS_R92CREG_92C_H
#define SAGUI_TERCEROS_R92CREG_92C_H

"""
out = cab + "\n\n".join(partes) + "\n\n#endif\n"
open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "kernel", "terceros", "bsd", "rtwn",
                  "r92creg_92c.h"), "w", encoding="latin-1").write(out)
print("ok", len(out))
