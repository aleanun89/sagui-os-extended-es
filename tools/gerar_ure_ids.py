#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Genera kernel/rede/ure_ids.h (D433): la lista de VID:PID de los adaptadores
Ethernet USB Realtek RTL8152/8153/8156 (y los OEM de la tabla de OpenBSD).

Uso: gerar_ure_ids.py <if_ure.c> <usbdevs.h> [salida]

Las fuentes son de OpenBSD (sys/dev/usb/if_ure.c, ISC/BSD de dos clausulas, y
sys/dev/usb/usbdevs.h): solo se toman los identificadores numericos de cada
nombre (USB_VENDOR_*, USB_PRODUCT_*); no se copia texto de ellas. Si un nombre
de la tabla no aparece en usbdevs.h, el generador se para."""
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        return 2
    ure_c = open(sys.argv[1], encoding="utf-8", errors="replace").read()
    devs = open(sys.argv[2], encoding="utf-8", errors="replace").read()
    salida = sys.argv[3] if len(sys.argv) > 3 else os.path.join(RAIZ, "kernel", "rede", "ure_ids.h")

    m = re.search(r"const struct usb_devno ure_devs\[\] = \{(.*?)\n\};", ure_c, re.S)
    if not m:
        print("no encuentro la tabla ure_devs[] en", sys.argv[1])
        return 1
    nombres = re.findall(r"\{\s*(USB_VENDOR_\w+)\s*,\s*(USB_PRODUCT_\w+)\s*\}", m.group(1))
    if not nombres:
        print("tabla vacia")
        return 1

    def valor(nombre):
        r = re.search(r"^#define\s+%s\s+(0x[0-9A-Fa-f]+)" % re.escape(nombre), devs, re.M)
        if not r:
            raise SystemExit("ERROR: %s no esta en usbdevs.h" % nombre)
        return int(r.group(1), 16)

    pares = []
    for v, p in nombres:
        pares.append((valor(v), valor(p), p))
    pares.sort()
    if len(set((a, b) for a, b, _ in pares)) != len(pares):
        raise SystemExit("ERROR: VID:PID repetidos en la tabla")

    lineas = [
        "/* kernel/rede/ure_ids.h - VID:PID de los Ethernet USB Realtek RTL8152/8153/8156 (D433).",
        " * GENERADO por tools/gerar_ure_ids.py desde OpenBSD sys/dev/usb/if_ure.c (ure_devs[])",
        " * y usbdevs.h: no editar a mano. Solo identificadores numericos. */",
        "#ifndef SAGUI_URE_IDS_H",
        "#define SAGUI_URE_IDS_H",
        "",
        "struct ure_id { uint16_t vid, pid; };",
        "",
        "static const struct ure_id ure_ids[] = {",
    ]
    for v, p, nombre in pares:
        lineas.append("    { 0x%04X, 0x%04X },   /* %s */" % (v, p, nombre[len("USB_PRODUCT_"):]))
    lineas += [
        "};",
        "#define URE_N_IDS ((int)(sizeof ure_ids / sizeof ure_ids[0]))",
        "",
        "#endif",
        "",
    ]
    with open(salida, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lineas))
    print("escrito %s: %d VID:PID" % (salida, len(pares)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
