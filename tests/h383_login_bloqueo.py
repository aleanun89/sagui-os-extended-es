#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H383 (D371, fase 6) - inicio de sesión en la consola, inicio automático y bloqueo de pantalla.

A. Consola con SAGUI-CONF: con el inicio automático (lo de siempre) entra el shell; root lo apaga
   («login --inicio-automatico no») y, al reiniciar, la consola pide la cuenta («login»): sagui (sin contraseña)
   entra con su carpeta; al salir vuelve a pedirla; una contraseña mala no entra; root entra con la suya en /root.
B. Escritorio: Windows+L bloquea la pantalla; Alt+Tab, Ctrl+Esc, Ctrl+Mayús+Esc y Alt+F4 no la saltan y lo
   tecleado no llega al Terminal; con la contraseña mala sigue bloqueada, con la buena se vuelve (los programas
   siguen abiertos)."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

PW = "raiz2468"
CL = "sagui135"


def esperar(m, patron, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-2500:])


def orden(m, texto, tempo=90):
    orden.n += 1
    marca = f"FIN-{orden.n:03d}"
    antes = len(m.serial())
    m.digitar(f"{texto}; echo FIN-'{orden.n:03d}'", 0.03)
    m.tecla("ret")
    g = esperar(m, rf"\n{marca}\b", tempo, antes)
    return m.serial()[antes:antes + g.end()].replace("\r", "")


orden.n = 0


def interativo(m, texto, pasos, tempo=90):
    desde = len(m.serial())
    m.digitar(texto, 0.04)
    m.tecla("ret")
    for pregunta, resposta in pasos:
        esperar(m, re.escape(pregunta), tempo, desde)
        desde = len(m.serial())
        m.digitar(resposta, 0.04)
        m.tecla("ret")
    time.sleep(3)
    return m.serial()[desde:]


def reiniciar(m):
    m.digitar("reiniciar", 0.04)
    m.tecla("ret")
    fim = time.time() + 90
    while m.proc.poll() is None and time.time() < fim:
        time.sleep(0.5)
    verificar(m.proc.poll() is not None, "reinicia")


def parte_a():
    img = imagem_de_teste("", ["--console", "--sesion-sagui", "--home-mb", "32", "--home-fs", "ext2", "--pedir-root"])
    with Maquina("h383a", "ide", imagem=img, snapshot=False) as m:
        esperar(m, r"supervisor en marcha", 150)
        esperar(m, r"sagui:\S*\$ ", 60)
        verificar("/bin/login" not in m.serial(), "con el inicio automático, el shell directo (lo de siempre)")
        time.sleep(2)
        out = interativo(m, "admin id", [("Contraseña nueva:", PW), ("Repítela:", PW)])
        verificar("uid=0(root)" in out, "root con contraseña")
        out = orden(m, "login --inicio-automatico no")
        verificar("administrador" in out, "sin ser root no se cambia")
        out = orden(m, f"echo {PW} | admin login --inicio-automatico no; cat /etc/sistema.conf")
        verificar(re.search(r"^inicio_automatico=no$", out, re.M), "inicio_automatico=no en /etc/sistema.conf")
        reiniciar(m)
    with Maquina("h383b", "ide", imagem=img, snapshot=False) as m:
        esperar(m, r"executar /bin/login", 150)
        esperar(m, r"Cuenta: ", 60)
        time.sleep(1)
        pos = len(m.serial())
        m.digitar("sagui", 0.05)
        m.tecla("ret")
        esperar(m, r"login: entra sagui \(uid 1000\)", 30, pos)
        esperar(m, r"sagui:\S*\$ ", 30, pos)
        out = orden(m, "id; pwd")
        verificar("uid=1000(sagui)" in out, "sagui entra en la consola")
        pos = len(m.serial())
        m.digitar("exit", 0.05)
        m.tecla("ret")
        esperar(m, r"Cuenta: ", 30, pos)
        verificar(True, "al salir, vuelve a pedir la cuenta")
        pos = len(m.serial())
        m.digitar("root", 0.05)
        m.tecla("ret")
        esperar(m, r"Contraseña: ", 30, pos)
        m.digitar("mala999", 0.05)
        m.tecla("ret")
        esperar(m, r"Cuenta o contraseña incorrectas", 30, pos)
        esperar(m, r"Cuenta: ", 30, pos + 10)
        pos = len(m.serial())
        m.digitar("root", 0.05)
        m.tecla("ret")
        esperar(m, r"Contraseña: ", 30, pos)
        m.digitar(PW, 0.05)
        m.tecla("ret")
        esperar(m, r"login: entra root \(uid 0\)", 30, pos)
        time.sleep(2)
        out = orden(m, "id; pwd")
        verificar("uid=0(root)" in out and re.search(r"^/root$", out, re.M), f"root entra en /root ({out[-300:]!r})")
        verificar(PW not in m.serial() and "mala999" not in m.serial(), "ninguna contraseña en la serie")


def parte_b():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--home-mb", "32", "--home-fs", "ext2"])
    with Maquina("h383c", "ide", imagem=img) as m:
        esperar(m, r"\[SAGUI\] terminal: \d+", 150)
        esperar(m, r"supervisor en marcha", 60)
        time.sleep(4)
        out = orden(m, f"printf '{CL}\\n{CL}\\n' > /tmp/p; passwd < /tmp/p; rm /tmp/p")
        verificar("contraseña de sagui cambiada" in out, f"sagui elige su contraseña ({out[-300:]!r})")
        pos = len(m.serial())
        m.tecla("meta_l-l")
        esperar(m, r"\[SESION\] pantalla bloqueada \(sagui\)", 30, pos)
        time.sleep(1)
        m.captura("H383-bloqueada")
        # D371 (revisión): los atajos no lo saltan y lo que se teclea no llega al Terminal de detrás.
        for t in ("alt-tab", "ctrl-esc", "ctrl-shift-esc", "alt-f4"):
            m.tecla(t)
            time.sleep(0.3)
        m.digitar("echo ESCAPOU", 0.05)
        m.tecla("ret")
        esperar(m, r"inicio de sesion \(pedido \d+\) de uid 1000: NEGADO", 30, pos)
        time.sleep(3)
        verificar("ESCAPOU" not in m.serial()[pos:] and "/bin/tareas" not in m.serial()[pos:],
                  "bloqueada: ni los atajos ni lo tecleado llegan a la sesión")
        pos = len(m.serial())
        m.digitar("mala999", 0.05)
        m.tecla("ret")
        esperar(m, r"inicio de sesion \(pedido \d+\) de uid 1000: NEGADO", 30, pos)
        verificar("pantalla desbloqueada" not in m.serial()[pos:], "con la mala sigue bloqueada")
        time.sleep(3)
        m.digitar(CL, 0.05)
        m.tecla("ret")
        esperar(m, r"\[SESION\] pantalla desbloqueada", 30, pos)
        out = orden(m, "id")
        verificar("uid=1000(sagui)" in out, "el Terminal sigue abierto tras desbloquear")


def main():
    parte_a()
    parte_b()
    print("H383 OK")


if __name__ == "__main__":
    main()
