#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""gerar_ids_wifi_usb.py - genera kernel/rede/wifi_usb_ids.h (D409).

Las tablas de VID:PID de los pinchos Wi-Fi USB salen de las listas de
dispositivos de OpenBSD (ISC): if_urtwn.c (Realtek RTL8188CU/RTL8192CU),
if_run.c (Ralink RT2870/RT3070/RT5370...) y if_athn_usb.c (Atheros AR9271),
resueltas con usbdevs. Solo se usa en el anfitrion, al regenerar el archivo:

    tools/gerar_ids_wifi_usb.py DIRECTORIO_CON_LAS_FUENTES_DE_OPENBSD

(el directorio lleva if_urtwn.c, if_run.c, if_athn_usb.c y usbdevs; ver
docs/wifi_usb.md). Sin argumentos no hace nada."""
import re
import sys
import os


def leer_usbdevs(ruta):
    fab, prod = {}, {}
    for l in open(ruta, encoding="latin-1"):
        m = re.match(r"vendor\s+(\w+)\s+(0x[0-9a-fA-F]+)", l)
        if m:
            fab[m.group(1)] = int(m.group(2), 16)
        m = re.match(r"product\s+(\w+)\s+(\w+)\s+(0x[0-9a-fA-F]+)", l)
        if m:
            prod[(m.group(1), m.group(2))] = int(m.group(3), 16)
    return fab, prod


def pares(texto, patron, fab, prod):
    sal = []
    for m in re.finditer(patron, texto):
        v, p = m.group(1), m.group(2)
        if v in fab and (v, p) in prod:
            sal.append((fab[v], prod[(v, p)], v + "_" + p))
    return sorted(set(sal))


def main():
    if len(sys.argv) < 2:
        print(__doc__)
        return 0
    d = sys.argv[1]
    fab, prod = leer_usbdevs(os.path.join(d, "usbdevs"))
    urt = open(os.path.join(d, "if_urtwn.c"), encoding="latin-1").read()
    run = open(os.path.join(d, "if_run.c"), encoding="latin-1").read()
    ath = open(os.path.join(d, "if_athn_usb.c"), encoding="latin-1").read()
    cu = pares(urt, r"URTWN_DEV_8192CU\((\w+),\s*(\w+)\)", fab, prod)
    rt = pares(run, r"USB_ID\((\w+),\s*(\w+)\)", fab, prod)
    # AR9271 solo (el AR7010 de los AR9280/AR9287 usa otro firmware): los que dicen AR9271 o el WNA1100
    at = [x for x in pares(ath, r"\{\{\s*USB_VENDOR_(\w+),\s*USB_PRODUCT_\w+?_(\w+)\s*\}\s*\}?", fab, prod)]
    at2 = []
    for m in re.finditer(r"\{\{\s*USB_VENDOR_(\w+),\s*USB_PRODUCT_(\w+)\s*\}(\s*,\s*ATHN_USB_FLAG_AR7010)?\s*\}", ath):
        if m.group(3):
            continue
        v, pn = m.group(1), m.group(2)[len(m.group(1)) + 1:]
        if v in fab and (v, pn) in prod:
            at2.append((fab[v], prod[(v, pn)], v + "_" + pn))
    at2 = sorted(set(at2))
    def tabla(nombre, lista):
        s = "static const struct wusb_id %s[] = {\n" % nombre
        for v, p, n in lista:
            s += "    { 0x%04X, 0x%04X },   /* %s */\n" % (v, p, n)
        return s + "};\n"
    out = """/* wifi_usb_ids.h - VID:PID de los pinchos Wi-Fi USB (D409). GENERADO por
 * tools/gerar_ids_wifi_usb.py desde las listas de dispositivos de OpenBSD
 * (if_urtwn.c, if_run.c, if_athn_usb.c y usbdevs; licencia ISC; solo son
 * identificadores numericos). No editar a mano. */
#ifndef SAGUI_WIFI_USB_IDS_H
#define SAGUI_WIFI_USB_IDS_H
struct wusb_id { unsigned short vid, pid; };

/* Realtek RTL8188CU / RTL8192CU (urtwn, familia «C»). */
""" + tabla("wusb_ids_rtl8192cu", cu) + "\n/* Ralink RT2870 / RT3070 / RT5370 y parientes (run). */\n" + tabla("wusb_ids_rt2870", rt) + \
        "\n/* Atheros AR9271 (athn, sin el AR7010 de los AR9280/AR9287). */\n" + tabla("wusb_ids_ar9271", at2) + "#endif\n"
    open(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "kernel", "rede", "wifi_usb_ids.h"), "w").write(out)
    print(len(cu), len(rt), len(at2))
    return 0


sys.exit(main())
