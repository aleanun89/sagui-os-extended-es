#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H279 (D279, mejora 059, v8.2) - Bluetooth con un controlador USB estándar (CSR8510).

QEMU 8.2 no emula Bluetooth (ya no existe usb-bt-dongle), así que la prueba tiene tres partes:

  unidad   en el PC (tests/bt_unidad.c con tests/bt_sim.c): la MISMA pila de kernel/bluetooth/ contra un
           controlador HCI simulado y tres periféricos (teclado, ratón y combo) que hablan HCI, L2CAP,
           SDP (con respuesta fragmentada) y HIDP de verdad. Con ASan y UBSan: inicio del controlador
           (y sus fallos), búsqueda (resultados RSSI, clásico y extendido, nombres), emparejamiento
           Secure Simple Pairing (Just Works, comparación numérica aceptada y rechazada, passkey, PIN
           antiguo), claves de enlace guardadas y recargadas, reconexión (iniciada por nosotros y por el
           teclado), clave vieja, SDP con error o mudo, canal HID rechazado, cortes, créditos ACL (un solo
           búfer), fragmentación con búferes de 27 bytes, apagar/encender, olvidar, desenchufar el cable
           virtual, conexión de un desconocido, informes de teclado y ratón, y 260.000 entradas aleatorias;
  consola  en QEMU, con un xHCI y un teclado USB (dispositivos que NO son Bluetooth): el driver nuevo no
           rompe la enumeración ni el HID, y la orden `bluetooth` sin pincho informa bien (estado, lista,
           buscar, emparejar, encender);
  ventana  en QEMU, escritorio: la aplicación «Bluetooth» abre su ventana sin pincho y sigue viva.

Lo que no se puede probar sin pincho (el transporte USB real, ACL por bulk, teclado Bluetooth de verdad)
está en docs/bluetooth.md como guion de prueba con el CSR8510.
Uso: h279_bluetooth.py [unidad] [consola] [ventana]   (sin argumentos, todo)"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, imagem_de_teste, RAIZ, SAIDA  # noqa: E402

DIR = os.path.join(SAIDA, "h279")


def compilar(nome, banderas):
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, nome)
    fuentes = [os.path.join(RAIZ, "tests", "bt_unidad.c"), os.path.join(RAIZ, "tests", "bt_sim.c")]
    fuentes += sorted(os.path.join(RAIZ, "kernel", "bluetooth", f) for f in os.listdir(os.path.join(RAIZ, "kernel", "bluetooth"))
                      if f.endswith(".c"))
    r = subprocess.run(["gcc", "-Wall", "-Wextra", "-DBT_ANFITRION", "-I", os.path.join(RAIZ, "kernel", "bluetooth"), *banderas,
                        "-o", exe, *fuentes], capture_output=True, text=True)
    verificar(r.returncode == 0 and not r.stderr.strip(), "la pila compila sin avisos\n" + r.stderr)
    return exe


def unidad():
    for nome, banderas in (("asan", ["-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined"]),
                           ("o2", ["-O2"])):
        exe = compilar("bt_unidad_" + nome, banderas)
        r = subprocess.run([exe], capture_output=True, text=True, timeout=300)
        print(r.stdout[-1500:], r.stderr[-1500:])
        verificar(r.returncode == 0 and "todo bien" in r.stdout and not r.stderr.strip(),
                  f"pruebas de unidad de la pila Bluetooth ({nome})")
        m = re.search(r"(\d+) comprobaciones, 0 fallos", r.stdout)
        verificar(m and int(m.group(1)) > 140, "y son las comprobaciones esperadas")


def consola():
    extras = ["-device", "qemu-xhci,id=xhci", "-device", "usb-kbd,bus=xhci.0"]
    with Maquina("h279-consola", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 240)
        s = m.serial()
        verificar("[HID] teclado USB" in s, "el teclado USB sigue funcionando con el driver nuevo")
        verificar("[BT]" not in s, "sin pincho no hay trazas de Bluetooth")
        out = m.comando("bluetooth", 30)
        verificar("Bluetooth: sin controlador" in out, "bluetooth sin pincho: " + out[-200:])
        out = m.comando("bluetooth lista", 30)
        verificar("ningún dispositivo" in out.replace("\xc3\xba", "\xfa") or "ning" in out and "dispositivo" in out,
                  "bluetooth lista: " + out[-200:])
        out = m.comando("bluetooth buscar", 30)
        verificar("no hay ning" in out and "controlador Bluetooth" in out, "bluetooth buscar sin pincho: " + out[-200:])
        out = m.comando("bluetooth emparejar 0", 30)
        verificar("no conozco" in out, "emparejar un dispositivo inexistente: " + out[-200:])
        out = m.comando("bluetooth encender", 30)
        verificar("no hay ning" in out, "encender sin pincho: " + out[-200:])
        out = m.comando("bluetooth confirmar si", 30)
        verificar("no hay ning" in out, "confirmar sin pincho: " + out[-200:])
        out = m.comando("bluetooth --ayuda", 30)
        verificar("uso: bluetooth" in out, "ayuda: " + out[-200:])
        out = m.comando("usb", 30)
        verificar("Bluetooth" not in out and "QEMU USB Keyboard" in out, "el comando usb no cambia sin pincho")
        out = m.comando("echo sigue-vivo", 30)
        verificar("\nsigue-vivo" in out, "el sistema sigue respondiendo")
        verificar("[BT]" not in m.serial(), "y nada de Bluetooth en el registro")


def ventana():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--dados-mb", "16"])
    with Maquina("h279-ventana", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        antes = len(m.serial())
        m.comando("bluetooth --ventana &", 20)
        fim = time.time() + 30
        while time.time() < fim and not re.search(r'criada por \d+: "Bluetooth"', m.serial()[antes:]):
            time.sleep(0.3)
        verificar(re.search(r'criada por \d+: "Bluetooth" (\d+)x(\d+)', m.serial()[antes:]), "la ventana «Bluetooth» se abre sin pincho")
        time.sleep(1.5)
        m.captura("H279-ventana")
        time.sleep(2)
        verificar("(bluetooth) terminou" not in m.serial()[antes:], "y la aplicación sigue viva (no terminó ni falló)")
        verificar("[BT]" not in m.serial(), "sin trazas de Bluetooth")


if __name__ == "__main__":
    partes = sys.argv[1:] or ["unidad", "consola", "ventana"]
    for p in partes:
        print(f"== {p}")
        {"unidad": unidad, "consola": consola, "ventana": ventana}[p]()
    print("H279 correcta")
