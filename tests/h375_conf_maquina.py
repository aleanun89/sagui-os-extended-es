#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H375 (D371, fase 1) - la configuración de la máquina pasa de la carpeta de sagui a /etc (SAGUI-CONF).

Consola, sesión sagui, /home en ext2, SAGUI-CONF, frecuencia simulada, sin instantánea:
1. Arranque 1: la primera vez ya se crea /etc/sistema.conf (la marca). Se elige la contraseña de root. sagui
   deja ajustes «viejos» en su carpeta (sagui.conf con acel2d, latencia y acento; /home/.frecuencia) y root
   borra /etc/sistema.conf, como en un sistema de antes.
2. Arranque 2: la migración pasa a /etc/sistema.conf solo las claves de la máquina (acel2d, latencia; no el
   acento) y /home/.frecuencia a /etc/frecuencia, sin borrar nada de /home. sagui ya no puede cambiarlos:
   ni escribir /etc/frecuencia, ni guardar la aceleración (sin escritorio no hay diálogo). sagui cambia
   su /home/.frecuencia y su sagui.conf: no vale.
3. Arranque 3: el kernel y configuracion aplican lo de /etc (ahorro, acel2d=no), no lo de /home; root
   (admin) sí cambia la aceleración y queda en /etc/sistema.conf."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

PW = "raiz2468"


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


def arrancar(m):
    esperar(m, r"supervisor en marcha", 120)
    esperar(m, r"sagui:\S*\$ ", 60)
    time.sleep(2)


def main():
    img = imagem_de_teste("", ["--console", "--sesion-sagui", "--home-mb", "32", "--home-fs", "ext2",
                               "--frecuencia-simulada"])
    # ---------------------------------------------------------------- 1
    with Maquina("h375a", "ide", imagem=img, snapshot=False) as m:
        arrancar(m)
        verificar("conf_maquina: migracion de la configuracion de la maquina a /etc: 0" in m.serial(),
                  "primer arranque con SAGUI-CONF: se crea /etc/sistema.conf")
        out = interativo(m, "admin id", [("Contraseña nueva:", PW), ("Repítela:", PW)])
        verificar("uid=0(root)" in out, "root con contraseña")
        orden(m, "mkdir -p /home/.config; printf 'acel2d=no\\nlatencia=120\\nacento=2a6fd8\\n' > /home/.config/sagui.conf; "
                 "echo politica=ahorro > /home/.frecuencia")
        orden(m, f"echo {PW} | admin rm /etc/sistema.conf /sistema/conf/etc/sistema.conf")
        out = orden(m, "ls /etc")
        verificar("sistema.conf" not in out.split("ls /etc")[-1], "como antes de esta versión: sin /etc/sistema.conf")
        reiniciar(m)
    # ---------------------------------------------------------------- 2
    with Maquina("h375b", "ide", imagem=img, snapshot=False) as m:
        arrancar(m)
        s = m.serial()
        verificar("migrado /home/.frecuencia -> /etc/frecuencia: 0" in s, "migra /home/.frecuencia")
        verificar("conf_maquina: migracion de la configuracion de la maquina a /etc: 0" in s, "migra sagui.conf")
        out = orden(m, "cat /etc/sistema.conf; cat /etc/frecuencia; cat /home/.config/sagui.conf")
        verificar(re.search(r"^acel2d=no$", out, re.M) and re.search(r"^latencia=120$", out, re.M),
                  f"/etc/sistema.conf con acel2d y latencia ({out[-500:]!r})")
        verificar(out.count("acento=2a6fd8") == 1, "el acento se queda solo en sagui.conf (es de la persona)")
        verificar(re.search(r"^politica=ahorro$", out, re.M), "/etc/frecuencia")
        out = orden(m, "echo politica=rendimiento > /etc/frecuencia; cat /etc/frecuencia")
        verificar("rendimiento" not in out.split("cat /etc/frecuencia")[-1], "sagui no escribe /etc/frecuencia")
        out = orden(m, "configuracion aceleracion 2d si")
        out = orden(m, "cat /etc/sistema.conf")
        verificar(re.search(r"^acel2d=no$", out, re.M), "sagui no cambia la aceleración de la máquina")
        orden(m, "echo politica=rendimiento > /home/.frecuencia; "
                 "printf 'acel2d=si\\nacento=2a6fd8\\n' > /home/.config/sagui.conf")
        reiniciar(m)
    # ---------------------------------------------------------------- 3
    with Maquina("h375c", "ide", imagem=img, snapshot=False) as m:
        arrancar(m)
        s = m.serial()
        verificar(re.search(r"configuracion: acel2d no -> ", s), "configuracion aplica acel2d=no de /etc")
        verificar(not re.search(r"configuracion: acel2d si -> ", s), "no el acel2d=si de la carpeta de sagui")
        verificar("migracion de la configuracion" not in s, "la migración no se repite")
        out = orden(m, "frecuencia")
        verificar(re.search(r"ahorro", out, re.I) and not re.search(r"rendimiento", out.split("frecuencia", 1)[-1], re.I),
                  f"el kernel usa la política de /etc/frecuencia ({out[-300:]!r})")
        out = orden(m, f"echo {PW} | admin configuracion aceleracion 2d auto")
        out = orden(m, "cat /etc/sistema.conf; cat /sistema/conf/etc/sistema.conf")
        verificar(out.count("acel2d=auto") >= 1 and "acel2d=no" not in out.split("cat /etc/sistema.conf")[-1],
                  f"root sí cambia la aceleración en /etc/sistema.conf ({out[-400:]!r})")
        verificar(PW not in m.serial().replace(f"echo {PW}", ""), "la contraseña no sale en la serie")
    print("H375 OK")


if __name__ == "__main__":
    main()
