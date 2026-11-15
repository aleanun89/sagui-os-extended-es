#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H267 - USB 1.1: controladores UHCI y OHCI propios (D267).

Criterios, en QEMU:
  1. uhci: un piix3-usb-uhci con un hub USB 1.1 (teclado y ratón detrás), un pendrive
     directo y otro enchufado en caliente detrás del hub. El teclado escribe en el
     terminal, el ratón mueve el cursor y hace clic, el pendrive se lee y se escribe
     (SHA-256 igual en el PC), conexión y retirada en caliente.
  2. ohci: lo mismo con un pci-ohci (teclado y ratón directos a los puertos, el
     pendrive detrás de un hub).
  3. companero: EHCI ICH9 con tres UHCI compañeros: un teclado de plena velocidad, un
     hub USB 1.1 con un pendrive y un pendrive de alta velocidad. El EHCI cede al
     compañero lo que es USB 1.x (bit PORT_OWNER), el UHCI lo enumera y los drivers de
     clase funcionan igual; el de alta velocidad sigue por el EHCI.
Uso: h267_usb11.py [uhci] [ohci] [companero]   (sin argumentos, todo)
"""
import hashlib
import os
import random
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_pendrive, mtools, SAIDA  # noqa: E402
from h214_xhci import sha_no_sagui, relatorios_teclado, cliques, clicar  # noqa: E402


def probar_teclado_y_raton(m, ctl):
    """Teclado y ratón USB sobre el controlador ctl (sin PS/2: solo ellos pueden ser)."""
    s = m.serial()
    verificar(re.search(rf"\[HID\] teclado USB: QEMU QEMU USB Keyboard .*por {ctl}", s),
              f"el teclado USB se reconoce por el {ctl}")
    verificar(re.search(rf"\[HID\] raton USB: QEMU QEMU USB Mouse .*por {ctl}", s),
              f"el ratón USB se reconoce por el {ctl}")
    antes = relatorios_teclado(m)
    out = m.comando("echo teclado-usb11 ñandú", 30)
    verificar("teclado-usb11 ñandú" in out, "el teclado de 12 Mb/s escribe en el terminal: " + out[-100:])
    verificar(relatorios_teclado(m) > antes + 10, "las teclas llegaron por el endpoint de interrupción (informes HID)")
    clicar(m)
    m.monitor("mouse_move 60 40")
    time.sleep(0.5)
    clicar(m)
    c = cliques(m.serial())
    verificar(len(c) >= 2, f"el ratón USB hace clic ({c})")
    verificar(c[-1][0] - c[-2][0] == 60 and c[-1][1] - c[-2][1] == 40,
              f"el ratón USB mueve el cursor (+60, +40): {c[-2]} -> {c[-1]}")


def probar_pendrive(m, ctl, grande, sha, disco="usb0"):
    m.comando(f"montar {disco}", 60)
    out = m.comando("cat /usb/ola.txt", 30)
    verificar("ola do usb11" in out, f"lectura de un fichero del pendrive por el {ctl}")
    verificar(sha_no_sagui(m, "/usb/grande.bin") == sha, "SHA-256 del fichero de 1 MB igual al del PC")
    m.comando("cp /usb/grande.bin /usb/copia.bin", 300)
    verificar(sha_no_sagui(m, "/usb/copia.bin") == sha, "la copia escrita en el pendrive se lee igual")
    out = m.comando(f"desmontar {disco}", 120)
    verificar("Ya puedes retirar el pendrive." in out, "desmontar graba todo")


def copia_en_el_pc(pen, sha, nombre):
    destino = os.path.join(SAIDA, f"{nombre}-copia.bin")
    if os.path.exists(destino):
        os.remove(destino)
    mtools(pen, "mcopy", "-o", "p:/copia.bin", destino)
    with open(destino, "rb") as f:
        verificar(hashlib.sha256(f.read()).hexdigest() == sha, "en el PC, la copia escrita por el USB 1.1 es idéntica")


def en_caliente(m, ctl, bus_puerto, pen3):
    m.monitor(f"drive_add 0 if=none,id=p3,format=raw,file={pen3}")
    m.monitor(f"device_add usb-storage,bus={bus_puerto[0]},port={bus_puerto[1]},drive=p3,id=dev-p3,removable=on")
    m.esperar_serial("[DISCO] usb1", 60)
    verificar(True, f"pendrive conectado en caliente por el {ctl}: usb1")
    m.monitor("device_del dev-p3")
    m.esperar_serial("removido: QEMU QEMU USB HARDDRIVE", 30)
    verificar(True, "pendrive retirado en caliente")


def parte_generica(ctl, nombre, dispositivo, extras_usb, puerto_caliente, patron_ctl):
    grande = random.Random(267).randbytes(1024 * 1024 + 333)
    sha = hashlib.sha256(grande).hexdigest()
    pen = criar_pendrive(f"h267-{nombre}", rotulo="USB11", arquivos={"ola.txt": "ola do usb11\n", "grande.bin": grande})
    pen3 = criar_pendrive(f"h267-{nombre}-3", rotulo="QUENTE", arquivos={"q.txt": "quente\n"})
    extras = ["-machine", "q35,i8042=off", "-device", dispositivo,
              "-drive", f"if=none,id=pen,format=raw,file={pen}"] + extras_usb
    with Maquina(f"h267-{nombre}", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 240)
        s = m.serial()
        verificar(re.search(patron_ctl, s), f"el {ctl} arranca y dice sus puertos")
        verificar(re.search(rf"\[USB\] conectado: QEMU QEMU USB HARDDRIVE .*{ctl}, 12 Mb/s", s),
                  f"el pendrive entra por el {ctl} a 12 Mb/s")
        verificar(re.search(rf"\[USB\] hub \d+ com \d+ portas \(12 Mb/s", s), "el hub USB 1.1 se configura")
        verificar("[DISCO] usb0" in s, "el pendrive entra en la capa de discos")
        probar_teclado_y_raton(m, ctl)
        out = m.comando("usb", 30)
        verificar(re.search(rf"Controladores USB 1\.1: {'1 UHCI, 0 OHCI' if ctl == 'UHCI' else '0 UHCI, 1 OHCI'}", out),
                  f"el comando usb lista el {ctl}")
        verificar(re.search(rf"QEMU USB HARDDRIVE .*12 Mb/s \(USB 1\.1\) por {ctl}", out), "y el pendrive a 12 Mb/s")
        verificar(re.search(rf"HID: teclado USB .* por {ctl}", out), "y el teclado")
        probar_pendrive(m, ctl, grande, sha)
        en_caliente(m, ctl, puerto_caliente, pen3)
        out = m.comando("echo sigue-vivo", 30)
        verificar("\nsigue-vivo" in out, "el sistema sigue respondiendo")
        m.captura(f"H267-{nombre}")
    copia_en_el_pc(pen, sha, f"h267-{nombre}")


def parte_uhci():
    # piix3-usb-uhci: 2 puertos. Hub en el 1 (teclado 1.1, ratón 1.2; el hueco 1.3 es para el caliente), pendrive en el 2.
    parte_generica(
        "UHCI", "uhci", "piix3-usb-uhci,id=uhci",
        ["-device", "usb-hub,bus=uhci.0,port=1",
         "-device", "usb-kbd,bus=uhci.0,port=1.1", "-device", "usb-mouse,bus=uhci.0,port=1.2",
         "-device", "usb-storage,bus=uhci.0,port=2,drive=pen,removable=on"],
        ("uhci.0", "1.3"), r"\[UHCI\] controlador .*8086:7020.* 2 puertos")


def parte_ohci():
    # pci-ohci: 3 puertos. Teclado y ratón directos (1 y 2) y un hub en el 3 con el pendrive (3.1) y el hueco 3.2.
    parte_generica(
        "OHCI", "ohci", "pci-ohci,id=ohci",
        ["-device", "usb-kbd,bus=ohci.0,port=1", "-device", "usb-mouse,bus=ohci.0,port=2",
         "-device", "usb-hub,bus=ohci.0,port=3",
         "-device", "usb-storage,bus=ohci.0,port=3.1,drive=pen,removable=on"],
        ("ohci.0", "3.2"), r"\[OHCI\] controlador .*106b:003f.* 3 puertos")


def parte_companero():
    grande = random.Random(2670).randbytes(256 * 1024 + 5)
    sha = hashlib.sha256(grande).hexdigest()
    pen11 = criar_pendrive("h267-comp11", rotulo="DETRASHUB", arquivos={"ola.txt": "ola do usb11\n", "grande.bin": grande})
    pen20 = criar_pendrive("h267-comp20", rotulo="ALTAVEL", arquivos={"alta.txt": "alta velocidade pelo ehci\n"})
    extras = ["-device", "ich9-usb-ehci1,id=ehci,addr=1d.7,multifunction=on",
              "-device", "ich9-usb-uhci1,masterbus=ehci.0,firstport=0,addr=1d.0,multifunction=on",
              "-device", "ich9-usb-uhci2,masterbus=ehci.0,firstport=2,addr=1d.1",
              "-device", "ich9-usb-uhci3,masterbus=ehci.0,firstport=4,addr=1d.2",
              "-device", "usb-kbd,bus=ehci.0,port=1,usb_version=1,id=kbd11",
              "-device", "usb-hub,bus=ehci.0,port=3",
              "-drive", f"if=none,id=p11,format=raw,file={pen11}",
              "-device", "usb-storage,bus=ehci.0,port=3.1,drive=p11,removable=on",
              "-drive", f"if=none,id=p20,format=raw,file={pen20}",
              "-device", "usb-storage,bus=ehci.0,port=4,drive=p20,removable=on"]
    with Maquina("h267-comp", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 240)
        s = m.serial()
        verificar(re.search(r"\[EHCI\] controlador .*8086:293a", s), "el EHCI ICH9 arranca")
        verificar(len(re.findall(r"\[UHCI\] controlador .*8086:2934|\[UHCI\] controlador .*8086:2935|"
                                 r"\[UHCI\] controlador .*8086:2936", s)) == 3, "y sus tres UHCI compañeros")
        verificar(re.search(r"porta 0 \(EHCI 0\): USB 1\.x, cedido al controlador companero", s),
                  "el teclado de plena velocidad se cede al compañero (PORT_OWNER)")
        verificar(re.search(r"porta 2 \(EHCI 0\): USB 1\.x, cedido al controlador companero", s),
                  "el hub USB 1.1 también")
        verificar(re.search(r"\[USB\] conectado: QEMU QEMU USB Keyboard .*UHCI, 12 Mb/s", s),
                  "el UHCI compañero enumera el teclado")
        verificar(re.search(r"\[USB\] conectado: QEMU QEMU USB HARDDRIVE .*hub \d+ porta 1, UHCI, 12 Mb/s", s),
                  "y el pendrive detrás del hub USB 1.1")
        verificar(re.search(r"\[USB\] conectado: QEMU QEMU USB HARDDRIVE .*raiz 0 porta 3, EHCI, 480 Mb/s", s),
                  "mientras el de alta velocidad sigue por el EHCI")
        verificar(re.search(r"\[HID\] teclado USB: QEMU QEMU USB Keyboard .*por UHCI", s), "el driver HID funciona sobre el UHCI")
        antes = relatorios_teclado(m)
        out = m.comando("echo companero-uhci", 30)
        verificar("\ncompanero-uhci" in out, "el teclado cedido al UHCI escribe")
        verificar(relatorios_teclado(m) > antes + 8, "por su endpoint de interrupción")
        out = m.comando("usb", 30)
        verificar(re.search(r"puerto 0 \(EHCI 0\): USB 1\.x, cedido al controlador compañero", out), "usb dice quién tiene el puerto")
        verificar("Controladores USB 1.1: 3 UHCI, 0 OHCI" in out, "y lista los tres UHCI")
        # Cuál es cada disco se ve en `discos` (por la etiqueta del volumen).
        out = m.comando("discos", 60)
        d11 = re.search(r"^(usb\d+) .*\"DETRASHUB\"", out, re.M)
        d20 = re.search(r"^(usb\d+) .*\"ALTAVEL\"", out, re.M)
        verificar(d11 and d20, "los dos pendrives están en la capa de discos: " + out[-300:])
        m.comando(f"montar {d11.group(1)}", 60)
        verificar("ola do usb11" in m.comando("cat /usb/ola.txt", 30), "el pendrive detrás del hub USB 1.1 se lee")
        verificar(sha_no_sagui(m, "/usb/grande.bin") == sha, "el fichero de 256 KB leído por el UHCI compañero coincide (SHA-256)")
        m.comando(f"desmontar {d11.group(1)}", 120)
        m.comando(f"montar {d20.group(1)}", 60)
        verificar("alta velocidade pelo ehci" in m.comando("cat /usb/alta.txt", 30), "y el de alta velocidad se lee por el EHCI")
        m.comando(f"desmontar {d20.group(1)}", 120)
        # Retirada en caliente del teclado cedido: el puerto vuelve al EHCI.
        m.monitor("device_del kbd11")
        m.esperar_serial("[HID] teclado USB desconectado", 30)
        m.esperar_serial("el USB 1.x se ha retirado, el puerto vuelve al EHCI", 30)
        verificar(True, "al desenchufar el teclado cedido, el UHCI lo retira y el puerto vuelve al EHCI")
        out = m.comando("echo vivo-tras-retirar", 30)
        verificar("\nvivo-tras-retirar" in out, "el sistema sigue respondiendo")
        # Y un teclado nuevo de plena velocidad en el mismo puerto vuelve a pasar por el compañero.
        n = m.serial().count("cedido al controlador companero")
        n_hid = m.serial().count("[HID] teclado USB: QEMU QEMU USB Keyboard")
        m.monitor("device_add usb-kbd,bus=ehci.0,port=1,usb_version=1,id=kbd11b")
        fim = time.time() + 60
        while time.time() < fim and m.serial().count("cedido al controlador companero") <= n:
            time.sleep(0.5)
        fim = time.time() + 60
        while time.time() < fim and m.serial().count("[HID] teclado USB: QEMU QEMU USB Keyboard") <= n_hid:
            time.sleep(0.5)
        verificar(m.serial().count("[HID] teclado USB: QEMU QEMU USB Keyboard") > n_hid,
                  "un teclado enchufado en caliente se cede y se reconoce por el UHCI")
        m.captura("H267-companero")


def main():
    partes = {"uhci": parte_uhci, "ohci": parte_ohci, "companero": parte_companero}
    for nome in (sys.argv[1:] or list(partes)):
        partes[nome]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
