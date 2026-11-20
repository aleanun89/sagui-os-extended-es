#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H411 (D411, v8.5.5.1) - Bluetooth con controladores que necesitan firmware (EXPERIMENTAL, sin probar en hardware).

Broadcom (.hcd), Realtek (rtl_bt/*_fw.bin y *_config.bin) y Atheros (AR3011 y AR3012), y la búsqueda de baja
energía (BLE). QEMU no emula Bluetooth, así que la prueba tiene tres partes:

  unidad    en el PC (tests/bt_fw_unidad.c con tests/bt_sim.c y kernel/bluetooth/): análisis del .hcd (a mano),
            de los archivos «Realtech» REALES de linux-firmware y de otros hechos a mano (parche por versión de
            ROM, epílogo, identificador de proyecto, fragmentos de 252 bytes con el índice que da la vuelta,
            fragmento final vacío), nombres de archivo, y la MÁQUINA DE ESTADOS de la carga contra un
            controlador HCI simulado (Realtek: ROM, descarga entera comprobada byte a byte, Reset, versión
            releída; sin archivo, ROM sin parche, fragmento rechazado, controlador mudo; Broadcom: minicontrolador,
            registros, esperas de 50 y 250 ms, rechazos, .hcd corrupto; Atheros por USB; BLE: máscara de eventos,
            búsqueda activa, anuncios con nombre y HID, balizas ignoradas, conectar BLE). Con ASan y UBSan. Además
            repite la prueba de la pila con CSR (tests/bt_unidad.c): nada cambió para los controladores sin firmware;
  ficheros  los firmware de firmware/ cuadran con su SHA-256 (tools/firmware_bt.sh), cada uno con su licencia al
            lado, docs/terceros.md los registra y el Makefile los mete en el ramdisk;
  consola   en QEMU (un xHCI y un teclado USB, sin Bluetooth): el sistema arranca con los firmware en /lib/firmware
            y la orden `bluetooth` sigue informando sin controlador.

Uso: h411_bluetooth_firmware.py [unidad] [ficheros] [consola]   (sin argumentos, todo)"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, RAIZ, SAIDA  # noqa: E402

DIR = os.path.join(SAIDA, "h411")


def compilar(nome, fuente, banderas):
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, nome)
    kb = os.path.join(RAIZ, "kernel", "bluetooth")
    fuentes = [os.path.join(RAIZ, "tests", fuente), os.path.join(RAIZ, "tests", "bt_sim.c")]
    fuentes += sorted(os.path.join(kb, f) for f in os.listdir(kb) if f.endswith(".c"))
    r = subprocess.run(["gcc", "-Wall", "-Wextra", "-DBT_ANFITRION", "-I", kb, *banderas, "-o", exe, *fuentes],
                       capture_output=True, text=True)
    verificar(r.returncode == 0 and not r.stderr.strip(), "compila sin avisos\n" + r.stderr)
    return exe


def unidad():
    for nome, banderas in (("asan", ["-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]),
                           ("o2", ["-O2"])):
        exe = compilar("bt_fw_" + nome, "bt_fw_unidad.c", banderas)
        r = subprocess.run([exe, RAIZ], capture_output=True, text=True, timeout=300)
        print(r.stdout[-1500:], r.stderr[-1500:])
        verificar(r.returncode == 0 and "todo bien" in r.stdout and not r.stderr.strip(),
                  f"pruebas de unidad del firmware Bluetooth ({nome})")
        m = re.search(r"(\d+) comprobaciones, 0 fallos", r.stdout)
        verificar(m and int(m.group(1)) >= 240, "y son las comprobaciones esperadas")
    exe = compilar("bt_unidad_csr", "bt_unidad.c", ["-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"])
    r = subprocess.run([exe], capture_output=True, text=True, timeout=300)
    verificar(r.returncode == 0 and "todo bien" in r.stdout and not r.stderr.strip(),
              "la pila con el CSR sigue igual (tests/bt_unidad.c)")
    m = re.search(r"(\d+) comprobaciones, 0 fallos", r.stdout)
    verificar(m and int(m.group(1)) > 140, "y con todas sus comprobaciones")


def ficheros():
    r = subprocess.run([os.path.join(RAIZ, "tools", "firmware_bt.sh"), "--comprobar"], capture_output=True, text=True)
    verificar(r.returncode == 0, "SHA-256 de todo el firmware Bluetooth\n" + r.stdout + r.stderr)
    rt = os.path.join(RAIZ, "firmware", "realtek")
    at = os.path.join(RAIZ, "firmware", "atheros")
    verificar(os.path.exists(os.path.join(rt, "LICENCE.rtlwifi_firmware.txt")) and os.path.exists(os.path.join(at, "LICENCE.atheros_firmware")),
              "las licencias de Realtek y Atheros al lado")
    for f in ("rtl8723b_fw.bin", "rtl8723d_fw.bin", "rtl8761bu_fw.bin", "rtl8761bu_config.bin", "rtl8821a_fw.bin", "rtl8821c_fw.bin", "rtl8821c_config.bin"):
        verificar(os.path.exists(os.path.join(rt, "rtl_bt", f)), "firmware/realtek/rtl_bt/" + f)
    verificar(len(os.listdir(os.path.join(at, "ar3k"))) == 18 and os.path.exists(os.path.join(at, "ath3k-1.fw")), "firmware/atheros: ar3k y ath3k-1.fw")
    verificar(not any(n.endswith(".hcd") for _, _, fs in os.walk(os.path.join(RAIZ, "firmware")) for n in fs),
              "ningun .hcd de Broadcom en git (no son redistribuibles)")
    mk = open(os.path.join(RAIZ, "Makefile"), encoding="utf-8").read()
    verificar("FIRMWARE_BT_RTL" in mk and "lib/firmware/rtl_bt" in mk and "lib/firmware/ar3k" in mk and "ath3k-1.fw" in mk,
              "el Makefile mete el firmware en /lib/firmware (y SEM_FIRMWARE=1 lo deja fuera)")
    ter = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("rtl_bt" in ter and "ar3k" in ter and "D411" in ter and "ath3k-1.fw" in ter, "docs/terceros.md los registra")
    rf = os.path.join(RAIZ, "build", "rootfs", "lib", "firmware", "rtl_bt", "rtl8761bu_fw.bin")
    if os.path.exists(rf):
        verificar(open(rf, "rb").read() == open(os.path.join(rt, "rtl_bt", "rtl8761bu_fw.bin"), "rb").read(), "el ramdisk lleva /lib/firmware/rtl_bt/")


def consola():
    extras = ["-device", "qemu-xhci,id=xhci", "-device", "usb-kbd,bus=xhci.0"]
    with Maquina("h411-consola", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 240)
        verificar("[BT]" not in m.serial(), "sin pincho no hay trazas de Bluetooth")
        out = m.comando("ls /lib/firmware/rtl_bt", 30)
        verificar("rtl8761bu_fw.bin" in out and "rtl8723b_fw.bin" in out and "rtl8821c_config.bin" in out, "firmware Realtek en /lib/firmware/rtl_bt: " + out[-300:])
        out = m.comando("ls /lib/firmware/ar3k", 30)
        verificar("AthrBT_0x11020000.dfu" in out and "ramps_0x11020000_40.dfu" in out, "firmware Atheros en /lib/firmware/ar3k: " + out[-300:])
        out = m.comando("ls /lib/firmware", 30)
        verificar("ath3k-1.fw" in out and "LICENCE.atheros_firmware" in out and "LICENCE.rtlwifi_firmware.txt" in out, "AR3011 y licencias: " + out[-300:])
        out = m.comando("bluetooth", 30)
        verificar("Bluetooth: sin controlador" in out, "bluetooth sin pincho: " + out[-200:])
        out = m.comando("bluetooth buscar", 30)
        verificar("controlador Bluetooth" in out, "bluetooth buscar sin pincho: " + out[-200:])
        out = m.comando("echo sigue-vivo", 30)
        verificar("\nsigue-vivo" in out, "el sistema sigue respondiendo")


if __name__ == "__main__":
    partes = sys.argv[1:] or ["unidad", "ficheros", "consola"]
    for p in partes:
        print(f"== {p}")
        {"unidad": unidad, "ficheros": ficheros, "consola": consola}[p]()
    print("H411 correcta")
