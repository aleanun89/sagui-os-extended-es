#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H214 - USB 3.0 (xHCI) de verdad, con teclado y ratón USB (D214).

Criterios, en QEMU:
  1. pc + qemu-xhci: un pendrive USB 3.0 (SuperSpeed) se monta, se lee (SHA-256 igual
     al del PC) y se escribe una copia que el PC lee después igual; un hub USB con otro
     pendrive detrás (12 Mb/s); el teclado USB escribe en el terminal (distribución ES,
     teclas muertas, AltGr); conexión y retirada en caliente; `usb`, `discos -t` e
     `infopc -t` muestran el xHCI y sus dispositivos.
  2. q35 sin controlador PS/2 (i8042=off): el teclado USB basta para usar el terminal y
     el ratón USB mueve el cursor y hace clic.
  3. nec-usb-xhci (otro xHCI del QEMU): el pendrive se lee.
  4. EHCI con teclado y ratón USB (lista periódica, D214): el teclado escribe y el
     ratón hace clic.
  5. --smp --ioapic (q35, dos CPU): la autoprueba de la IRQ (D211) tiene una sonda del
     xHCI (orden No-Op) y la IRQ llega; con la entrada ISA rota (--irq-prueba
     isa-errada, lo del Amilo) la línea pasa al 8259 y el USB sigue funcionando.
Uso: h214_xhci.py [pc] [q35] [nec] [ehci] [ioapic]   (sin argumentos, todo)
"""
import hashlib
import os
import random
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, imagem_de_teste, criar_pendrive, mtools, SAIDA  # noqa: E402


def sha_no_sagui(m, caminho):
    out = m.comando(f"sha256sum {caminho}", 180)
    r = re.search(r"([0-9a-f]{64})\s+" + re.escape(caminho), out)
    return r.group(1) if r else None


def relatorios_teclado(m):
    out = m.comando("usb", 30)
    r = re.search(r"HID: teclado USB .*?, (\d+) informes", out)
    return int(r.group(1)) if r else -1


def cliques(serial):
    return [(int(a), int(b)) for a, b in re.findall(r"\[MOUSE\] botao esquerdo apertado em \((\d+),(\d+)\)", serial)]


def clicar(m):
    m.monitor("mouse_button 1")
    time.sleep(0.3)
    m.monitor("mouse_button 0")
    time.sleep(0.5)


def parte_pc():
    grande = random.Random(214).randbytes(1024 * 1024 + 777)
    sha = hashlib.sha256(grande).hexdigest()
    pen = criar_pendrive("h214-pen", rotulo="XHCIPEN", arquivos={"ola.txt": "ola do xhci\n", "grande.bin": grande})
    pen2 = criar_pendrive("h214-pen2", rotulo="HUBPEN", arquivos={"hub.txt": "atras do hub\n"})
    pen3 = criar_pendrive("h214-pen3", rotulo="QUENTE", arquivos={"q.txt": "quente\n"})
    extras = ["-device", "qemu-xhci,id=xhci",
              "-drive", f"if=none,id=pen,format=raw,file={pen}",
              "-device", "usb-storage,bus=xhci.0,port=1,drive=pen,removable=on",
              "-device", "usb-kbd,bus=xhci.0,port=2",
              "-device", "usb-hub,bus=xhci.0,port=4",
              "-drive", f"if=none,id=pen2,format=raw,file={pen2}",
              "-device", "usb-storage,bus=xhci.0,port=4.1,drive=pen2,removable=on"]
    with Maquina("h214-pc", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar(re.search(r"\[XHCI\] controlador .*1b36:000d.*8 puertos \(4 USB 2\.0, 4 USB 3\.x\)", s),
                  "el xHCI arranca y dice sus puertos USB 2.0 y USB 3.x")
        verificar(re.search(r"\[USB\] conectado: QEMU QEMU USB HARDDRIVE .*xHCI, 5 Gb/s", s),
                  "el pendrive entra por un puerto USB 3.0 a 5 Gb/s")
        verificar(re.search(r"\[USB\] hub \d+ com 8 portas", s), "el hub USB se configura por el xHCI")
        verificar(re.search(r"\[USB\] conectado: QEMU QEMU USB HARDDRIVE .*hub \d+ porta 1, xHCI, 12 Mb/s", s),
                  "el pendrive detrás del hub se enumera (12 Mb/s, por la ruta del hub)")
        verificar("[HID] teclado USB: QEMU QEMU USB Keyboard" in s, "el teclado USB se reconoce (protocolo boot)")
        verificar("[DISCO] usb0" in s and "[DISCO] usb1" in s, "los dos pendrives entran en la capa de discos")

        antes = relatorios_teclado(m)
        out = m.comando("echo ñandú pingüino @#[]", 30)
        verificar("ñandú pingüino @#[]" in out, "el teclado USB escribe con acentos, ñ, diéresis y AltGr: " + out[-120:])
        verificar(relatorios_teclado(m) > antes + 10, "las teclas llegaron por el teclado USB (informes HID)")

        out = m.comando("montar usb0", 60)
        out = m.comando("cat /usb/ola.txt", 30)
        verificar("ola do xhci" in out, "lectura de un fichero del pendrive por el xHCI")
        verificar(sha_no_sagui(m, "/usb/grande.bin") == sha, "SHA-256 del fichero de 1 MB igual al del PC")
        m.comando("cp /usb/grande.bin /usb/copia.bin", 180)
        verificar(sha_no_sagui(m, "/usb/copia.bin") == sha, "la copia escrita en el pendrive se lee igual")
        out = m.comando("discos -t", 60)
        verificar("pendrive USB 3.0 (xHCI)" in out, "discos dice que el pendrive es USB 3.0 por el xHCI")
        verificar("pendrive USB (xHCI)" in out, "y que el del hub va por el xHCI")
        out = m.comando("usb", 30)
        verificar("Controladores USB 3.0 (xHCI): 1" in out and "5 Gb/s (USB 3.0) por xHCI" in out,
                  "el comando usb lista el xHCI y la velocidad de cada dispositivo")
        verificar(re.search(r"QEMU USB Hub .*clase hub, 12 Mb/s", out), "y el hub")
        out = m.comando("infopc -t sistema", 120)
        verificar("Controladores USB 3.0 (xHCI): 1" in out and "HID: teclado USB" in out,
                  "infopc muestra el xHCI, sus dispositivos y el teclado USB")
        out = m.comando("infopc -t pci", 120)
        verificar(re.search(r"1b36:000d\s+USB 3\.0 \(xHCI\)", out), "infopc -t pci muestra el xHCI")

        # En caliente: un tercer pendrive entra y sale.
        m.monitor(f"drive_add 0 if=none,id=p3,format=raw,file={pen3}")
        m.monitor("device_add usb-storage,bus=xhci.0,port=3,drive=p3,id=dev-p3,removable=on")
        m.esperar_serial("[DISCO] usb2", 30)
        verificar(True, "pendrive conectado en caliente: usb2")
        m.monitor("device_del dev-p3")
        m.esperar_serial("removido: QEMU QEMU USB HARDDRIVE", 30)
        verificar(True, "pendrive retirado en caliente")

        out = m.comando("desmontar usb0", 120)
        verificar("Ya puedes retirar el pendrive." in out, "desmontar graba todo")
        verificar("[XHCI] el controlador se ha parado" not in m.serial(), "el controlador no se ha parado nunca")
        m.captura("H214-xhci-usb")
    destino = os.path.join(SAIDA, "h214-copia.bin")
    if os.path.exists(destino):
        os.remove(destino)
    mtools(pen, "mcopy", "-o", "p:/copia.bin", destino)
    with open(destino, "rb") as f:
        verificar(hashlib.sha256(f.read()).hexdigest() == sha, "en el PC, la copia escrita por el xHCI es idéntica")


def parte_q35_sem_ps2():
    extras = ["-machine", "q35,i8042=off", "-device", "qemu-xhci,id=xhci",
              "-device", "usb-kbd,bus=xhci.0", "-device", "usb-mouse,bus=xhci.0"]
    with Maquina("h214-q35", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 240)
        s = m.serial()
        verificar("mouse PS/2 nao respondeu" in s, "q35 sin i8042: no hay ratón PS/2")
        verificar("[HID] raton USB" in s or "[HID] teclado USB" in s, "teclado y ratón USB reconocidos")
        out = m.comando("echo solo-teclado-usb", 30)
        verificar("\nsolo-teclado-usb" in out, "sin PS/2, el teclado USB basta para usar el terminal")
        clicar(m)
        m.monitor("mouse_move 60 40")
        time.sleep(0.5)
        clicar(m)
        c = cliques(m.serial())
        verificar(len(c) >= 2, f"el ratón USB hace clic ({c})")
        verificar(c[-1][0] - c[-2][0] == 60 and c[-1][1] - c[-2][1] == 40,
                  f"el ratón USB mueve el cursor (+60, +40): {c[-2]} -> {c[-1]}")


def parte_nec():
    pen = criar_pendrive("h214-nec", rotulo="NECPEN", arquivos={"nec.txt": "pelo nec-usb-xhci\n"})
    extras = ["-device", "nec-usb-xhci,id=xhci", "-drive", f"if=none,id=pen,format=raw,file={pen}",
              "-device", "usb-storage,bus=xhci.0,drive=pen,removable=on"]
    with Maquina("h214-nec", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        verificar(re.search(r"\[XHCI\] controlador .*1033:0194", m.serial()), "el nec-usb-xhci arranca")
        m.comando("montar usb0", 60)
        out = m.comando("cat /usb/nec.txt", 30)
        verificar("pelo nec-usb-xhci" in out, "y su pendrive se lee")


def parte_ehci_hid():
    extras = ["-device", "usb-ehci,id=ehci", "-device", "usb-kbd,bus=ehci.0", "-device", "usb-mouse,bus=ehci.0"]
    with Maquina("h214-ehci", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar("[HID] teclado USB: QEMU QEMU USB Keyboard" in s and "por EHCI" in s,
                  "teclado USB por el EHCI")
        antes = relatorios_teclado(m)
        out = m.comando("echo teclado-ehci", 30)
        verificar("\nteclado-ehci" in out, "el teclado USB del EHCI escribe")
        verificar(relatorios_teclado(m) > antes + 5, "por la lista periódica del EHCI (informes HID)")
        clicar(m)
        verificar("HID: ratón USB" in m.comando("usb", 30), "el ratón USB del EHCI está en el informe")


def parte_ioapic():
    for prueba in ("", "isa-errada"):
        opcoes = ["--vesa", "--console", "--smp", "--ioapic"] + (["--irq-prueba", prueba] if prueba else [])
        pen = criar_pendrive("h214-io", rotulo="IOAPIC", arquivos={"io.txt": "irq do xhci\n"})
        extras = ["-machine", "q35", "-device", "qemu-xhci,id=xhci", "-device", "usb-kbd,bus=xhci.0",
                  "-drive", f"if=none,id=pen,format=raw,file={pen}", "-device", "usb-storage,bus=xhci.0,drive=pen"]
        with Maquina("h214-ioapic" + ("-" + prueba if prueba else ""), "ide", imagem=imagem_de_teste("", opcoes),
                     extras=extras, nucleos=2) as m:
            m.esperar_serial("sagui:/$", 300)
            s = m.serial()
            if prueba:
                verificar(re.search(r"autoprueba \(arranque\): xHCI 00:\d\d\.\d GSI \d+ no llega -> PIC", s),
                          "entrada ISA rota: la sonda del xHCI no llega y la línea pasa al 8259")
            else:
                verificar(re.search(r"autoprueba \(arranque\): xHCI 00:\d\d\.\d GSI \d+ -> IRQ \d+: llega", s),
                          "la sonda del xHCI (orden No-Op) recibe su interrupción por el IOAPIC")
            m.comando("montar usb0", 60)
            out = m.comando("cat /usb/io.txt", 30)
            verificar("irq do xhci" in out, "el pendrive se lee" + (" con la línea en el 8259" if prueba else ""))
            out = m.comando("usb", 30)
            verificar("durmiendo hasta la IRQ" in out, "las transferencias duermen hasta la IRQ del xHCI")
            r = re.search(r"IRQ (\d+), (\d+) interrupciones", out)
            verificar(r and int(r.group(2)) > 0, f"y las interrupciones llegan ({r.group(0) if r else '?'})")


def main():
    partes = {"pc": parte_pc, "q35": parte_q35_sem_ps2, "nec": parte_nec, "ehci": parte_ehci_hid, "ioapic": parte_ioapic}
    for nome in (sys.argv[1:] or list(partes)):
        partes[nome]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
