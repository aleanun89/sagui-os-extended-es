#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H409 (D409) - más pinchos Wi-Fi USB: Realtek RTL8188CU/8192CU y Ralink
RT2870/RT3070/RT5370 (experimentales, sin probar en hardware), y el AR9271
reconocido sin driver.

  1. anfitrion: kernel/rede/wifi_usb_logica.h, rtl8192cu_logica.h y
     rt2870_logica.h (+ la bandera sem_informes de kernel/lib/wifi_n.c) con ASan y
     UBSan (tests/recursos/wifi_usb_anfitriao.c): tablas de VID:PID, tipo de
     chip, firmware reales, ROM, potencia, descriptores de transmisión,
     transferencias de recepción armadas a mano y basura al azar. Además, en
     Python (independiente): las tablas generadas contra las listas de
     dispositivos de OpenBSD guardadas en el generador, la suma de control del
     descriptor y el formato del TXWI;
  2. fuentes: enganches (usb.c, wifi.c, Makefile, ramdisk), firmware con SHA-256 y
     licencia al lado, terceros.md, aviso «experimental» y documentación;
  3. QEMU sin pincho: arranca igual, «wifi» dice que no hay adaptador, y
     «wifi compatibles VID:PID» reconoce los VID:PID nuevos por el sistema ya
     compilado (QEMU no sabe falsear un VID:PID de USB). El RTL8188EU real, si
     está enchufado, lo prueba tests/h215_wifi.py (parte «pincho»): con «h215» se
     repite aquí su parte sin pincho para comprobar que todo sigue igual.

Uso: h409_wifi_usb.py [anfitrion] [fuentes] [sin] [h215]
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import RAIZ, SAIDA, Maquina, FalhaTeste, imagem_console, verificar  # noqa: E402


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8", errors="replace") as f:
        return f.read()


def parte_anfitriao():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "wifi_usb_anfitriao")
    subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "kernel", "inc"),
                    "-o", exe, os.path.join(RAIZ, "tests", "recursos", "wifi_usb_anfitriao.c"),
                    os.path.join(RAIZ, "kernel", "lib", "wifi_n.c")], check=True)
    fw = lambda r: os.path.join(RAIZ, "firmware", r)
    r = subprocess.run([exe, fw("realtek/rtl8192cufw_A.bin"), fw("realtek/rtl8192cufw_TMSC.bin"), fw("ralink/rt2870.bin")],
                       capture_output=True, text=True)
    for l in r.stdout.splitlines():
        if l.startswith("FALLO"):
            print("  " + l)
    verificar(r.returncode == 0 and "wifi_usb ok" in r.stdout, "lógica de los pinchos (C, ASan/UBSan): " + (r.stdout + r.stderr)[-300:])
    n = sum(1 for l in r.stdout.splitlines() if l.startswith("ok "))
    print("  %d comprobaciones en C" % n)

    # Python independiente: la tabla del 8188EU sigue siendo la de la D215 y el generador y las tablas cuadran
    ids = ler("kernel/rede/wifi_usb_ids.h")
    def tabla(nombre):
        m = re.search(r"%s\[\] = \{(.*?)\};" % nombre, ids, re.S)
        return set((int(a, 16), int(b, 16)) for a, b in re.findall(r"\{ 0x([0-9A-F]{4}), 0x([0-9A-F]{4}) \}", m.group(1)))
    cu, rt, at = tabla("wusb_ids_rtl8192cu"), tabla("wusb_ids_rt2870"), tabla("wusb_ids_ar9271")
    verificar((0x0BDA, 0x8176) in cu and (0x0BDA, 0x8178) in cu and (0x0BDA, 0x8179) not in cu, "tabla CU: 8176 y 8178 sí; 8179 (EU) no")
    verificar((0x148F, 0x2870) in rt and (0x148F, 0x3070) in rt and (0x148F, 0x3072) in rt and (0x148F, 0x5370) in rt,
              "tabla RT: 2870, 3070, 3072 y 5370")
    verificar((0x0CF3, 0x9271) in at, "tabla AR9271: 0cf3:9271")
    verificar(not (cu & rt) and not (cu & at) and not (rt & at), "las tres tablas no se pisan")
    verificar(len(cu) >= 60 and len(rt) >= 200 and len(at) >= 10, "tamaños de las tablas: %d / %d / %d" % (len(cu), len(rt), len(at)))


def parte_fuentes():
    usb = ler("kernel/usb/usb.c")
    verificar("wifi_usb_reconhecer(d)" in usb and "wifi_usb_retirado(d)" in usb, "usb.c pasa el pincho a wifi.c (sin cambios)")
    wifi = ler("kernel/rede/wifi.c")
    verificar("wusb_familia(" in wifi and "WUSB_AR9271" in wifi and "wusb_experimental(" in wifi,
              "wifi.c reconoce por VID:PID, deja el AR9271 sin driver y marca lo experimental")
    verificar("via_rt()" in wifi and "rt28_iniciar(" in wifi and "r88e_nome_firmware(" in wifi and "rt28_nome_firmware(" in wifi,
              "wifi.c despacha por familia (r88e_*, rt28_*)")
    verificar("EXPERIMENTAL" in wifi and "(experimental)" in wifi, "el registro y el nombre del adaptador dicen «experimental»")
    verificar("SAGUI_SEM_FIRMWARE" in wifi, "con SEM_FIRMWARE=1 el driver dice que falta el firmware")
    rtl = ler("kernel/rede/rtl8188eu.c")
    verificar("fc(r)" in rtl and "encender_c(" in rtl and "rtl8192cu_bb_prog" in rtl and "r92c_montar_txdesc(" in rtl,
              "rtl8188eu.c: la familia C comparte el driver del 8188EU")
    verificar(os.path.exists(os.path.join(RAIZ, "kernel/rede/rt2870.c")) and "rt_rx_desglosar(" in ler("kernel/rede/rt2870.c")
              and "rt_montar_tx(" in ler("kernel/rede/rt2870.c"), "rt2870.c usa la lógica probada en el anfitrión")
    for f in ("kernel/rede/wifi_usb_logica.h", "kernel/rede/rtl8192cu_logica.h", "kernel/rede/rt2870_logica.h",
              "kernel/rede/wifi_usb_ids.h", "tests/recursos/wifi_usb_anfitriao.c", "docs/wifi_usb.md"):
        verificar(os.path.exists(os.path.join(RAIZ, f)), "existe " + f)
    mk = ler("Makefile")
    for f in ("rtl8192cufw_A.bin", "rtl8192cufw_TMSC.bin", "rt2870.bin", "LICENCE.ralink-firmware.txt"):
        verificar(f in mk, "el ramdisk lleva " + f)
    r = subprocess.run([os.path.join(RAIZ, "tools", "firmware_wifi_usb.sh"), "--comprobar"], capture_output=True, text=True)
    verificar(r.returncode == 0, "SHA-256 de los firmware y sus licencias: " + (r.stdout + r.stderr)[-120:])
    ter = ler("docs/terceros.md")
    for k in ("r92creg_92c.h", "bsd/run/rt2860reg.h", "rtl8192cufw", "rt2870.bin", "LICENCE.ralink-firmware.txt", "D409"):
        verificar(k in ter, "docs/terceros.md registra " + k)
    for f in ("kernel/terceros/bsd/rtwn/r92creg_92c.h", "kernel/terceros/bsd/run/rt2860reg.h"):
        c = ler(f)
        verificar("SPDX-License-Identifier: ISC" in c and "Damien Bergamini" in c and "Permission to use, copy, modify" in c,
                  f + ": licencia ISC y copyright originales")
    verificar("D409" in ler("docs/decisoes.md") and "wifi_usb.md" in ler("docs/decisoes.md"), "D409 en docs/decisoes.md")
    verificar("8.5.5.1" in ler("docs/ROADMAP.md") and "D409" in ler("docs/ROADMAP.md"), "fila 8.5.5.1 con D409 en el ROADMAP")
    guia = ler("docs/wifi_usb.md")
    verificar("experimental" in guia.lower() and "Cómo probarlo" in guia and "148f:5370" in guia and "0bda:8176" in guia and "AR9271" in guia,
              "la guía de prueba para quien tenga el pincho")
    rf = os.path.join(RAIZ, "build", "rootfs", "lib", "firmware")
    if os.path.isdir(rf):
        for f, o in (("rtl8192cufw_A.bin", "realtek"), ("rtl8192cufw_TMSC.bin", "realtek"), ("rt2870.bin", "ralink")):
            verificar(os.path.exists(os.path.join(rf, f)) and
                      open(os.path.join(rf, f), "rb").read() == open(os.path.join(RAIZ, "firmware", o, f), "rb").read(),
                      "el ramdisk lleva /lib/firmware/" + f)


def parte_sin(imagem):
    with Maquina("h409-sin", "ide", imagem=imagem) as m:
        m.esperar_serial("sagui:/$", 150)
        out = m.comando("wifi")
        verificar("no hay ningún adaptador" in out, "sin pincho: wifi dice que no hay adaptador")
        out = m.comando("ifconfig wlan0")
        verificar("no hay wlan0" in out, "sin pincho: ifconfig wlan0")
        verificar("[WIFI]" not in m.serial(), "sin pincho: ningún mensaje [WIFI]")
        casos = [("0bda:8179", "RTL8188EU", "probado"), ("0bda:8176", "RTL8188CU", "experimental"),
                 ("0bda:8178", "RTL8188CU", "experimental"), ("148f:2870", "RT2870", "experimental"),
                 ("148f:3070", "RT3070", "experimental"), ("148f:3072", "RT3070", "experimental"),
                 ("148f:5370", "RT5370", "experimental"), ("0cf3:9271", "AR9271", "sin driver")]
        for idp, nombre, nota in casos:
            out = m.comando("wifi compatibles " + idp)
            verificar(nombre in out and nota in out, "wifi compatibles %s -> %s (%s)" % (idp, nombre, nota))
        out = m.comando("wifi compatibles 1234:5678")
        verificar("no es un pincho Wi-Fi USB reconocido" in out, "wifi compatibles: un VID:PID cualquiera no se reconoce")
        out = m.comando("wifi compatibles")
        verificar("experimental" in out and "probado en hardware" in out and "AR9271" in out, "wifi compatibles: la lista por familias")


def main():
    partes = sys.argv[1:] or ["anfitrion", "fuentes", "sin"]
    if "anfitrion" in partes:
        parte_anfitriao()
    if "fuentes" in partes:
        parte_fuentes()
    if "sin" in partes:
        parte_sin(imagem_console())
    if "h215" in partes:
        r = subprocess.run([sys.executable, os.path.join(RAIZ, "tests", "h215_wifi.py"), "anfitrion", "fuentes", "sin"],
                           capture_output=True, text=True)
        verificar(r.returncode == 0 and "H215 ok" in r.stdout, "h215 (8188EU, partes sin pincho) sigue igual: " + (r.stdout + r.stderr)[-300:])
    print("H409 ok")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
