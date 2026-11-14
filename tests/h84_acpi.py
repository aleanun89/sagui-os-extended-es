#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H84 - apagado por ACPI (S5) y botón de encendido (D113, mejora 030).

Criterio:
  (a) terminal: el kernel lee la FADT y el \\_S5_ de la DSDT del QEMU (PIIX4),
      pasa al modo ACPI y atiende el botón; "acpi" lo dice; "acpi desactivar"
      y "acpi activar" cambian el estado; "apagar-pc" graba el pendrive (el
      archivo escrito justo antes está en la imagen) y QEMU termina solo;
  (b) imagen con --sem-acpi: "acpi" dice "desactivado", "acpi activar" no
      puede, el botón (system_powerdown) no hace nada y "apagar-pc" deja el
      aviso «Ya puedes apagar el ordenador» con QEMU encendido;
  (c) escritorio: system_powerdown (el botón de encendido de QEMU) muestra el
      aviso y apaga ordenadamente: QEMU termina solo;
  (d) escritorio: Inicio > "Apagar..." pregunta; Enter (Sí) apaga y QEMU termina.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, imagem_de_teste, criar_pendrive, pendrive_qemu,  # noqa: E402
                  mtools)


def esperar(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(padrao, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def terminou(m, tempo):
    """QEMU saiu sozinho (o apagado por ACPI fecha o QEMU)."""
    try:
        m.proc.wait(timeout=tempo)
        return True
    except Exception:
        return False


def parte_a():
    pen = criar_pendrive("h84-pen", mb=64, rotulo="APAGADO", arquivos={"antes.txt": "antes\n"})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h84", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar(re.search(r"\[ACPI\] \\_S5_ na DSDT: SLP_TYPa=\d SLP_TYPb=\d", s) is not None,
                  "o kernel acha o \\_S5_ na DSDT do QEMU")
        verificar("[ACPI] modo ACPI ligado" in s or "SCI_EN" not in s, "passa ao modo ACPI (SMI_CMD <- ACPI_ENABLE)")
        verificar(re.search(r"\[ACPI\] botao de energia ligado \(SCI na IRQ 9\)", s) is not None,
                  "o botão de energia é atendido pelo SCI na IRQ 9")
        out = m.comando("acpi")
        verificar(re.search(r"Apagado ACPI: disponible \(S5 = \d/\d, DSDT\)", out) is not None
                  and "Botón de encendido: atendido (SCI en la IRQ 9" in out, "acpi: disponible, S5 e botão")
        out = m.comando("acpi desactivar")
        verificar("Apagado ACPI: desactivado" in out, "acpi desactivar")
        out = m.comando("acpi activar")
        verificar("Apagado ACPI: disponible" in out, "acpi activar volta ao normal")
        out = m.comando("montar usb0")
        verificar("montado" in out, "monta o pendrive")
        m.comando("echo gravado-antes-de-apagar > /usb/apagado.txt")
        marca = len(m.serial())
        m.digitar("apagar-pc\n", 0.05)
        esperar(m, r"\[SAGUI\] apagando", 20, marca)
        esperar(m, r"\[ACPI\] S5: SLP_TYPa=\d SLP_TYPb=\d", 20, marca)
        verificar(terminou(m, 20), "apagar-pc: QEMU termina sozinho (ACPI S5)")
        verificar("nao desligou" not in m.serial()[marca:], "sem o recurso dos 3 s")
    txt, _ = mtools(pen, "mtype", "p:/apagado.txt")
    verificar("gravado-antes-de-apagar" in txt, "o arquivo escrito antes de apagar está no pendrive")


def parte_b():
    img = imagem_de_teste("", ["--vesa", "--console", "--sem-acpi"])
    with Maquina("h84-sem-acpi", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar("[ACPI] desativado pela opcao de arranque" in m.serial(), "a opção --sem-acpi chega ao kernel")
        out = m.comando("acpi")
        verificar("Apagado ACPI: desactivado" in out and "no atendido" in out, "acpi: desactivado, botão não atendido")
        out = m.comando("acpi activar")
        verificar("no hay ACPI utilizable" in out, "acpi activar não reativa o que a opção de arranque desligou")
        m.monitor("system_powerdown")
        time.sleep(2)
        verificar(m.proc.poll() is None and "botao de energia apertado" not in m.serial(),
                  "sem ACPI o botão (system_powerdown) não faz nada")
        marca = len(m.serial())
        m.digitar("apagar-pc\n", 0.05)
        esperar(m, r"\[SAGUI\] ya se puede apagar el PC", 20, marca)
        time.sleep(1)
        verificar(m.proc.poll() is None, "sem ACPI o QEMU continua ligado")
        verificar("Ya puedes apagar el ordenador" in "\n".join(m.tela_texto()), "a tela diz «Ya puedes apagar el ordenador»")
        m.captura("H84-sem-acpi")


def parte_c():
    with Maquina("h84-botao", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        marca = len(m.serial())
        m.monitor("system_powerdown")
        esperar(m, r"\[ACPI\] botao de energia apertado", 10, marca)
        esperar(m, r"notificacao: Botón de encendido: apagando", 10, marca)
        verificar(True, "system_powerdown: o SCI chega e aparece o aviso")
        try:
            m.captura("H84-botao")
        except Exception:
            pass
        verificar(terminou(m, 20), "o botão de energia apaga: QEMU termina sozinho")


def parte_d():
    with Maquina("h84-menu", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        marca = len(m.serial())
        m.clique(40, 12)
        g = esperar(m, r"menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com \d+ linhas\r?\n\[GUI\] menu linhas: ([^\r\n]*)",
                    10, marca)
        linhas = g.group(5).split("|")
        verificar("Apagar..." in linhas and "~Apagar (no disponible)" not in linhas, "o menu Inicio tem «Apagar...» ativo")
        y = int(g.group(2)) + 2
        for l in linhas:
            if l == "Apagar...":
                break
            y += 9 if l == "-" else 22
        m.clique(int(g.group(1)) + 40, y + 11)
        esperar(m, r'janela \d+ criada por \d+: "Apagar"', 10, marca)
        time.sleep(0.8)
        m.captura("H84-menu-pregunta")
        m.tecla("ret")
        esperar(m, r"\[SAGUI\] apagando", 15, marca)
        verificar(terminou(m, 20), "Inicio > Apagar... > Sí: QEMU termina sozinho")


def main():
    parte_a()
    parte_b()
    parte_c()
    parte_d()
    print("H84 OK")


if __name__ == "__main__":
    main()
