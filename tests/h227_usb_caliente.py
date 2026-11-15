#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H227 - USB en caliente: reintentos, estado por puerto y esperas que duermen (D227).

Todo se enchufa con el sistema en marcha, por el monitor del QEMU (drive_add y
device_add / device_del). La imagen lleva --usb-prueba primer-intento: el primer
intento de cada conexión falla a propósito (paso 3) y el dispositivo tiene que
reconocerse al segundo, que es lo que pasaba con los pinchos lentos (o detrás de
usbipd), que antes se quedaban sin reconocer para siempre.

Criterios:
  1. ehci: un pendrive por el EHCI se reconoce al segundo intento y entra en
     «discos»; el comando usb muestra el puerto «reconocido al intento 2»; un teclado
     USB enchufado en caliente se reconoce y escribe; desenchufar y volver a enchufar
     el pendrive enseguida (en la misma vuelta de la tarea) no lo pierde ni lo duplica.
  2. xhci: lo mismo por el qemu-xhci (pendrive USB 3.0 y teclado), y un hub USB con
     un pendrive detrás, enchufados en caliente uno tras otro (el pendrive aparece
     mientras el hub se está enumerando); el de detrás del hub se desenchufa y vuelve
     enseguida; al quitar el hub se van los dos.
  3. siempre: con --usb-prueba siempre, el pendrive se queda «sin reconocer tras 4
     intentos», el comando usb lo dice, y al desenchufarlo el puerto queda libre.
  4. companero: EHCI ICH9 con compañeros UHCI: un teclado USB 1.1 (12 Mb/s) no se
     puede llevar por el EHCI: o se cede al compañero (bit PO, si el controlador
     declara compañeros) o se deja tras sus intentos; al desenchufarlo, el puerto
     vuelve al EHCI y un pendrive de alta velocidad en el mismo puerto se reconoce.
Uso: h227_usb_caliente.py [ehci] [xhci] [siempre] [companero]   (sin argumentos, todo)
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, criar_pendrive  # noqa: E402


def imagem(prueba):
    return imagem_de_teste("", ["--vesa", "--console", "--usb-prueba", prueba])


def monitor(m, comando):
    """Una orden del monitor que no puede fallar (device_add, drive_add, device_del)."""
    resp = m.monitor(comando)
    verificar("rror" not in resp, f"el QEMU acepta «{comando}» ({resp.strip()[:200]})")
    return resp


def relatorios_teclado(m):
    out = m.comando("usb", 30)
    r = re.search(r"HID: teclado USB .*?, (\d+) informes", out)
    return int(r.group(1)) if r else -1


def esperar_contagem(m, texto, n, tempo=30):
    """Espera a que «texto» salga n veces en la serial."""
    fim = time.time() + tempo
    while time.time() < fim:
        if m.serial().count(texto) >= n:
            return True
        time.sleep(0.2)
    return False


def comprobar_teclado(m, rotulo):
    antes = relatorios_teclado(m)
    out = m.comando(f"echo teclado-{rotulo}", 30)
    verificar(f"\nteclado-{rotulo}" in out, f"el teclado USB enchufado en caliente escribe ({rotulo})")
    verificar(relatorios_teclado(m) > antes + 5, f"y lo que escribe llega por sus informes HID ({rotulo})")


def parte_ehci():
    pen = criar_pendrive("h227-pen", rotulo="CALIENTE", arquivos={"c.txt": "caliente\n"})
    # Otra imagen igual para volver a enchufar enseguida: el QEMU aún tiene el
    # cerrojo de escritura de la primera justo después de device_del.
    pen_b = criar_pendrive("h227-pen-b", rotulo="CALIENTE", arquivos={"c.txt": "caliente\n"})
    with Maquina("h227-ehci", "ide", imagem=imagem("primer-intento"), extras=["-device", "usb-ehci,id=ehci"]) as m:
        m.esperar_serial("sagui:/$", 180)
        verificar("fallos simulados (--usb-prueba): 1" in m.serial(), "la imagen pide el fallo del primer intento")
        monitor(m, f"drive_add 0 if=none,id=q1,format=raw,file={pen}")
        monitor(m, "device_add usb-storage,bus=ehci.0,port=1,drive=q1,id=pen1,removable=on")
        m.esperar_serial("[DISCO] usb0", 30)
        m.esperar_serial("reconocido (QEMU QEMU USB HARDDRIVE)", 30)   # sale tras dar de alta el disco
        s = m.serial()
        verificar(re.search(r"\[USB\] porta 0 \(EHCI 0\): conectado, intento 1 de 4", s), "primer intento en el puerto 0")
        verificar("fallo simulado en el intento 1" in s, "el primer intento falla (simulado)")
        verificar(re.search(r"porta 0 \(EHCI 0\): intento 1 fallido \(paso 3: .*\); el siguiente en 1 s", s),
                  "se programa otro intento al cabo de 1 s")
        verificar(re.search(r"porta 0 \(EHCI 0\): conectado, intento 2 de 4", s) and
                  re.search(r"porta 0 \(EHCI 0\): reconocido \(QEMU QEMU USB HARDDRIVE\) al intento 2", s),
                  "el segundo intento lo reconoce (esquema antiguo: primero la direccion)")
        verificar('rotulo "CALIENTE"' in s, "y entra en la capa de discos con su rotulo")
        out = m.comando("discos", 60)
        verificar("CALIENTE" in out, "discos muestra el pendrive")
        out = m.comando("usb", 30)
        verificar(re.search(r"puerto 0 \(EHCI 0\): reconocido al intento 2 \(QEMU QEMU USB HARDDRIVE\)", out),
                  "el comando usb muestra el estado del puerto y los intentos")

        # Teclado USB en caliente.
        monitor(m, "device_add usb-kbd,bus=ehci.0,port=2,id=kbd1")
        m.esperar_serial("[HID] teclado USB: QEMU QEMU USB Keyboard", 30)
        m.esperar_serial("reconocido (QEMU QEMU USB Keyboard)", 30)
        verificar(re.search(r"porta 1 \(EHCI 0\): reconocido \(QEMU QEMU USB Keyboard\) al intento 2", m.serial()),
                  "el teclado se reconoce al segundo intento")
        comprobar_teclado(m, "ehci")

        # Desenchufar y volver a enchufar enseguida, en el mismo puerto.
        n_con = m.serial().count("reconocido (QEMU QEMU USB HARDDRIVE)")
        monitor(m, "device_del pen1")
        monitor(m, f"drive_add 0 if=none,id=q2,format=raw,file={pen_b}")
        monitor(m, "device_add usb-storage,bus=ehci.0,port=1,drive=q2,id=pen2,removable=on")
        m.esperar_serial("[DISCO] usb1", 30)
        s = m.serial()
        verificar("removido: QEMU QEMU USB HARDDRIVE" in s, "el que se fue se retira")
        verificar(esperar_contagem(m, "reconocido (QEMU QEMU USB HARDDRIVE)", n_con + 1),
                  "el que vuelve se reconoce otra vez")
        out = m.comando("usb", 30)
        verificar(len(re.findall(r"Dispositivo \d+: QEMU QEMU USB HARDDRIVE", out)) == 1,
                  "un solo pendrive en la lista: ni perdido ni duplicado")
        out = m.comando("discos", 60)
        verificar(len(re.findall(r"^usb\d+ .*\"CALIENTE\"", out, re.M)) == 1, "y uno solo en discos")
        m.comando("montar usb1", 60)
        verificar("caliente" in m.comando("cat /usb/c.txt", 30), "el pendrive que volvio se lee")
        m.captura("H227-1-ehci-caliente")


def parte_xhci():
    pen = criar_pendrive("h227-xpen", rotulo="XCALIENTE", arquivos={"x.txt": "xhci caliente\n"})
    pen_hub = criar_pendrive("h227-hpen", rotulo="DETRASHUB", arquivos={"h.txt": "detras del hub\n"})
    pen_hub_b = criar_pendrive("h227-hpen-b", rotulo="DETRASHUB", arquivos={"h.txt": "detras del hub\n"})
    with Maquina("h227-xhci", "ide", imagem=imagem("primer-intento"), extras=["-device", "qemu-xhci,id=xhci"]) as m:
        m.esperar_serial("sagui:/$", 180)
        monitor(m, f"drive_add 0 if=none,id=x1,format=raw,file={pen}")
        monitor(m, "device_add usb-storage,bus=xhci.0,port=1,drive=x1,id=xpen,removable=on")
        m.esperar_serial("[DISCO] usb0", 30)
        m.esperar_serial("reconocido (QEMU QEMU USB HARDDRIVE)", 30)   # sale tras dar de alta el disco
        s = m.serial()
        verificar(re.search(r"porta \d+ \(xHCI 0\): reconocido \(QEMU QEMU USB HARDDRIVE\) al intento 2", s),
                  "xHCI: el pendrive se reconoce al segundo intento (la ranura del primero se libera)")
        verificar("[XHCI] el controlador se ha parado" not in s, "el controlador sigue en marcha")

        monitor(m, "device_add usb-kbd,bus=xhci.0,port=2,id=xkbd")
        m.esperar_serial("[HID] teclado USB: QEMU QEMU USB Keyboard", 30)
        m.esperar_serial("reconocido (QEMU QEMU USB Keyboard)", 30)
        comprobar_teclado(m, "xhci")

        # Hub y, enseguida, un pendrive detrás (llega mientras el hub se enumera).
        monitor(m, "device_add usb-hub,bus=xhci.0,port=4,id=hub1")
        monitor(m, f"drive_add 0 if=none,id=h1,format=raw,file={pen_hub}")
        monitor(m, "device_add usb-storage,bus=xhci.0,port=4.1,drive=h1,id=hpen,removable=on")
        m.esperar_serial("[DISCO] usb1", 40)
        esperar_contagem(m, "reconocido (QEMU QEMU USB HARDDRIVE)", 2, 30)
        s = m.serial()
        verificar(re.search(r"reconocido \(QEMU QEMU USB Hub\) al intento 2", s), "el hub se reconoce al segundo intento")
        verificar(re.search(r"\[USB\] hub \d+ com 8 portas", s), "y se configura")
        verificar(re.search(r"porta 1 \(hub \d+\): reconocido \(QEMU QEMU USB HARDDRIVE\) al intento 2", s),
                  "el pendrive de detras del hub tambien, con su propio estado de puerto")
        out = m.comando("usb", 30)
        verificar(re.search(r"puerto 1 \(hub \d+\): reconocido al intento 2", out), "usb muestra el puerto del hub")

        # Fuera y dentro enseguida, detrás del hub.
        monitor(m, "device_del hpen")
        monitor(m, f"drive_add 0 if=none,id=h2,format=raw,file={pen_hub_b}")
        monitor(m, "device_add usb-storage,bus=xhci.0,port=4.1,drive=h2,id=hpen2,removable=on")
        m.esperar_serial("[DISCO] usb2", 40)
        out = m.comando("discos", 60)
        verificar(len(re.findall(r"^usb\d+ .*\"DETRASHUB\"", out, re.M)) == 1, "detras del hub: vuelve una vez, sin duplicarse")

        # Quitar el hub se lleva lo de detrás.
        n_rem = m.serial().count("[USB] removido")
        monitor(m, "device_del hub1")
        verificar(esperar_contagem(m, "[USB] removido", n_rem + 2), "al quitar el hub se retiran el hub y su pendrive")
        out = m.comando("discos", 60)
        verificar("DETRASHUB" not in out and "XCALIENTE" in out, "queda solo el pendrive del puerto 1")
        verificar("[XHCI] el controlador se ha parado" not in m.serial(), "el controlador no se ha parado nunca")
        m.captura("H227-2-xhci-hub-caliente")


def parte_siempre():
    pen = criar_pendrive("h227-nunca", rotulo="NUNCA")
    with Maquina("h227-siempre", "ide", imagem=imagem("siempre"), extras=["-device", "usb-ehci,id=ehci"]) as m:
        m.esperar_serial("sagui:/$", 180)
        monitor(m, f"drive_add 0 if=none,id=n1,format=raw,file={pen}")
        monitor(m, "device_add usb-storage,bus=ehci.0,port=1,drive=n1,id=npen,removable=on")
        m.esperar_serial("sin reconocer tras 4 intentos", 40)
        s = m.serial()
        verificar(re.search(r"porta 0 \(EHCI 0\): sin reconocer tras 4 intentos \(paso 3: .*\); se vuelve a probar cada 10 s",
                            s), "tras 4 intentos rapidos se pasa a uno cada 10 s")
        for k in (2, 3, 4):
            verificar(f"conectado, intento {k} de 4" in s, f"el intento {k} se hizo")
        verificar("[DISCO] usb" not in s, "nada entra en la capa de discos")
        out = m.comando("usb", 30)
        verificar(re.search(r"puerto 0 \(EHCI 0\): sin reconocer, intento \d+ de \d+ fallido \(paso 3", out),
                  "el comando usb dice el paso y los intentos")
        out = m.comando("echo vivo", 30)
        verificar("\nvivo" in out, "el sistema sigue respondiendo mientras tanto")
        monitor(m, "device_del npen")
        m.esperar_serial("desconectado sin haberse reconocido", 15)
        out = m.comando("usb", 30)
        verificar("puerto 0 (EHCI 0)" not in out, "al desenchufarlo, el puerto queda libre")


def parte_companero():
    pen = criar_pendrive("h227-comp", rotulo="TRASUSB1", arquivos={"t.txt": "tras el usb 1\n"})
    extras = ["-device", "ich9-usb-ehci1,id=ehci,addr=1d.7,multifunction=on",
              "-device", "ich9-usb-uhci1,masterbus=ehci.0,firstport=0,addr=1d.0,multifunction=on",
              "-device", "ich9-usb-uhci2,masterbus=ehci.0,firstport=2,addr=1d.1",
              "-device", "ich9-usb-uhci3,masterbus=ehci.0,firstport=4,addr=1d.2"]
    with Maquina("h227-comp", "ide", imagem=imagem("primer-intento"), extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        monitor(m, "device_add usb-kbd,bus=ehci.0,port=1,usb_version=1,id=kbd11")
        fim = time.time() + 40
        while time.time() < fim:
            s = m.serial()
            if "cedido al controlador companero" in s or "sin reconocer tras 4 intentos" in s:
                break
            time.sleep(0.3)
        s = m.serial()
        cedido = "porta 0 (EHCI 0): USB 1.x, cedido al controlador companero" in s
        verificar(cedido or "porta 0 (EHCI 0): sin reconocer tras 4 intentos (paso 9" in s,
                  "el USB 1.1 se cede al compañero o se deja tras sus intentos (paso 9), sin colgar nada")
        verificar(not re.search(r"conectado: QEMU QEMU USB Keyboard .*EHCI", s), "el EHCI no intenta llevar un USB 1.1")
        # Con el driver UHCI (D267) el compañero lo enumera y el teclado funciona; mientras
        # esté enchufado el QEMU le manda a él las teclas de sendkey: «responde» se mira después.
        monitor(m, "device_del kbd11")
        m.esperar_serial("el USB 1.x se ha retirado, el puerto vuelve al EHCI" if cedido
                         else "desconectado sin haberse reconocido", 15)
        verificar(True, "al desenchufarlo, el puerto vuelve al EHCI")
        out = m.comando("echo vivo", 30)
        verificar("\nvivo" in out, "el sistema sigue respondiendo")
        monitor(m, f"drive_add 0 if=none,id=c1,format=raw,file={pen}")
        monitor(m, "device_add usb-storage,bus=ehci.0,port=1,drive=c1,id=cpen,removable=on")
        m.esperar_serial("[DISCO] usb0", 30)
        m.esperar_serial("reconocido (QEMU QEMU USB HARDDRIVE)", 30)   # sale tras dar de alta el disco
        verificar(re.search(r"porta 0 \(EHCI 0\): reconocido \(QEMU QEMU USB HARDDRIVE\)", m.serial()),
                  "un pendrive de alta velocidad en el mismo puerto se reconoce por el EHCI")


def main():
    partes = {"ehci": parte_ehci, "xhci": parte_xhci, "siempre": parte_siempre, "companero": parte_companero}
    pedidas = sys.argv[1:] or list(partes)
    for p in pedidas:
        partes[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
