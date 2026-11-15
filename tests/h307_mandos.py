#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H307 (D307) - mandos y joysticks: USB HID, Xbox 360 con cable y puerto de juegos.

  unidad  en el PC (tests/mandos_unidad.c con kernel/lib/hid_informe.c y portajogos_conv.c):
          el analizador de descriptores HID con descriptores reales (Logitech Dual Action / F310
          en modo D, joystick genérico DragonRise, uno con Report ID y Push/Pop, un ratón y
          descriptores rotos), el informe del Xbox 360 y las cuentas del puerto de juegos;
  os      en SaguiOS con la imagen --mandos-prueba (el QEMU no emula ningún joystick USB): los dos
          mandos de prueba en «mandos --lista», informes inyectados por el mismo camino que los USB
          leídos con «mandos --estado», el puerto de juegos sin joystick (el QEMU no tiene 0x201)
          sin colgarse, la calibración guardada en sagui.conf y aplicada al arrancar, y las SDL;
  persiste  la calibración del puerto de juegos se guarda en /home/.config/sagui.conf y
          «configuracion --aplicar» la pone al arrancar otra vez;
  gui     Configuración > Mandos: la lista, la prueba en vivo (ejes y botones dibujados con el
          estado inyectado) y las capturas H307-*;
  sdl2    SDL2-GNUBoy (SDL_GameController): abre el mando de prueba al recibir
          SDL_CONTROLLERDEVICEADDED y el botón B del mapeo Logitech (botón 3) llega como la A de
          la Game Boy: la ROM de prueba invierte la paleta mientras está pulsado;
  dosbox  DOSBox (SDL 1.2, joysticktype=auto): JOY.COM lee el joystick por la BIOS (INT 15h AH=84h)
          y ve la posición y el botón del mando de prueba.

Uso: h307_mandos.py [unidad] [os] [persiste] [gui] [sdl2] [dosbox]
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, RAIZ, SAIDA, APPS_RAIZ  # noqa: E402

DIR = os.path.join(SAIDA, "h307")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "mandos_unidad")
    r = subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-fsanitize=address,undefined", "-DSAGUI_HOST",
                        "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe,
                        os.path.join(RAIZ, "tests", "mandos_unidad.c"),
                        os.path.join(RAIZ, "kernel", "lib", "hid_informe.c"),
                        os.path.join(RAIZ, "kernel", "lib", "portajogos_conv.c")],
                       capture_output=True, text=True)
    verificar(r.returncode == 0 and "warning" not in r.stderr, "la prueba de unidad compila sin avisos\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print(r.stdout + r.stderr)
    verificar(r.returncode == 0 and "todo bien" in r.stdout,
              "analizador HID (Dual Action, DragonRise, Report ID, ratón, rotos), Xbox 360 y puerto de juegos")


def imagem():
    return imagem_de_teste("", ["--vesa", "--console", "--mandos-prueba"])


def provar_os():
    with Maquina("h307", "ide", imagem=imagem()) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[MANDOS] puerto de juegos: no hay joystick" in s, "sin puerto de juegos (QEMU): detectado como ausente")
        verificar("[MANDOS] mando 1: Mando de prueba (Dual Action) (prueba HID): 4 ejes, 12 botones, 1 hats" in s,
                  "el Dual Action de prueba pasa por el analizador: 4 ejes, 12 botones, 1 hat")
        verificar("Mando de prueba (Xbox 360) (prueba Xbox): 6 ejes, 11 botones, 1 hats" in s, "el Xbox 360 de prueba")
        out = m.comando("mandos --lista")
        verificar("0: Mando de prueba (Dual Action)" in out and "1: Mando de prueba (Xbox 360)" in out,
                  "mandos --lista enseña los dos")
        verificar("046d:c216; mapeo Logitech" in out and "045e:028e; mapeo Xbox 360" in out, "VID:PID y mapeo")
        out = m.comando("mandos --estado 0")
        verificar("mando 0: ejes 0 0 0 0; botones ninguno; hat 0 centro" in out,
                  "estado inicial del Dual Action: en reposo")
        # Dual Action: stick izquierdo a la izquierda y abajo, derecho centrado, hat suelto, botones 2 y 10
        verificar("rechazado" not in m.comando("mandos --inyectar 0 " + bytes([0, 0xFF, 0x80, 0x80, 0x28, 0x20, 0, 0]).hex()), "informe inyectado")
        out = m.comando("mandos --estado 0")
        verificar("ejes -32768 32767 128 128; botones 2 10; hat 0 centro" in out,
                  "Dual Action: X, Y, Z, Rz, botones 2 y 10 y el hat suelto")
        # Xbox 360: A + Start + cruceta arriba-izquierda, LT a fondo, LX a la derecha, LY arriba
        xbox = bytes([0x00, 0x14, 0x15, 0x10, 0xFF, 0x00, 0xFF, 0x7F, 0xFF, 0x7F, 0, 0, 0x00, 0x80]) + bytes(6)
        m.comando("mandos --inyectar 1 " + xbox.hex())
        out = m.comando("mandos --estado 1")
        verificar("ejes 32767 -32767 32767 0 32767 -32768; botones 1 8; hat 0 arriba-izq" in out,
                  "Xbox 360: ejes (Y invertida), gatillos, A, Start y la cruceta")
        out = m.comando("mandos --inyectar 1 010302")
        verificar("rechazado" in out, "Xbox 360: el mensaje de las luces no es un informe")
        out = m.comando("mandos --inyectar 7 00")
        verificar("rechazado" in out, "un mando que no existe")
        out = m.comando("mandos --detectar")
        verificar("No hay joystick en el puerto de juegos." in out, "--detectar no se cuelga sin puerto")
        out = m.comando("mandos --calibracion 100,600,1200/100,600,1200/0,0,0/0,0,0")
        verificar("a mano" in out and "X1: 100 / 600 / 1200" in out, "calibración a mano")
        verificar("[MANDOS] puerto de juegos: calibracion puesta" in m.serial(), "el kernel la recibe")
        out = m.comando("mandos --calibracion 1,2")
        verificar("mal escrita" in out, "calibración mal escrita rechazada")
        out = m.comando("mandos --calibracion auto")
        verificar("Calibración automática." in out, "vuelve a la automática")
        out = m.comando("mandos --probar 0 1")
        verificar("mando 0: ejes -32768 32767 128 128" in out, "--probar enseña el estado")


def da(x=0x80, y=0x80, z=0x80, rz=0x80, hat=8, botoes=0):
    """Un informe del Dual Action de prueba (botones: bit 0 = botón 1)."""
    return bytes([x, y, z, rz, (hat & 0x0F) | ((botoes & 0x0F) << 4), (botoes >> 4) & 0xFF, 0, 0]).hex()


def persiste():
    import h83_configuracion as h83
    img = h83.imagem_persistente("h307-persistente", False)
    cal = "200,6000,12000/250,6100,12100/0,0,0/0,0,0"
    with Maquina("h307p", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("mandos --calibracion " + cal)
        verificar("Guardado en /home/.config/sagui.conf." in out, "la calibración se guarda en sagui.conf")
        out = m.comando("cat /home/.config/sagui.conf")
        verificar("mando_puerto=" + cal in out, "sagui.conf tiene mando_puerto=")
        h83.reiniciar(m)
    with Maquina("h307p2", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("configuracion: mando_puerto " + cal + " -> 0" in s, "el arranque aplica la calibración guardada")
        verificar("[MANDOS] puerto de juegos: calibracion puesta (X 200/6000/12000, Y 250/6100/12100)" in s,
                  "el kernel la recibe al arrancar")
        out = m.comando("mandos --calibracion")
        verificar("(a mano)" in out and "Y1: 250 / 6100 / 12100" in out, "mandos --calibracion la enseña")
        out = m.comando("mandos --calibracion auto")
        out = m.comando("cat /home/.config/sagui.conf")
        verificar("mando_puerto=" not in out, "«auto» la quita de sagui.conf")


def imagem_grafica(extras=()):
    return imagem_de_teste("", ["--vesa", "--mandos-prueba", *extras])


def gui():
    import h83_configuracion as h83
    import h228_openjazz as h228
    with Maquina("h307g", "ide", imagem=imagem_grafica()) as m:
        h228.terminal(m)
        # Estado de partida: stick izquierdo a la izquierda y abajo, botones 2 y 10, cruceta arriba;
        # el Xbox 360 con A, LB y el stick derecho arriba.
        m.digitar("mandos --inyectar 0 " + da(x=0, y=0xFF, hat=0, botoes=(1 << 1) | (1 << 9)) + "\n", 0.04)
        xbox = bytes([0x00, 0x14, 0x00, 0x11, 0x00, 0xFF, 0, 0, 0, 0, 0, 0, 0xFF, 0x7F]) + bytes(6)
        m.digitar("mandos --inyectar 1 " + xbox.hex() + "\n", 0.04)
        m.digitar("configuracion &\n", 0.04)
        fim = time.time() + 30
        while time.time() < fim and '"Configuración"' not in m.serial():
            time.sleep(0.3)
        time.sleep(1.5)
        j = h83.janela(m.serial(), "Configuración")
        verificar(j["l"] == 840, f"la ventana de Configuración es de 840 px ({j['l']})")
        ox, oy = j["x"] + 2, j["y"] + 24
        pos = h83.pos_paginas()
        verificar(len(pos) == 11, "once pestañas, la última «Mandos»")
        m.clique(ox + pos[10][0] + pos[10][1] // 2, oy + 24)
        fim = time.time() + 15
        while time.time() < fim and "configuracion: pagina Mandos" not in m.serial():
            time.sleep(0.3)
        verificar("configuracion: pagina Mandos" in m.serial(), "la pestaña Mandos se abre")
        time.sleep(1.5)
        m.captura("H307-1-mandos-dual-action")
        w, h, rgb = h228.pantalla(m, "h307-gui")

        def cor(x, y):
            i = ((oy + y) * w + ox + x) * 3
            return tuple(rgb[i:i + 3])

        def perto(c, e, tol=24):
            return all(abs(a - b) <= tol for a, b in zip(c, (e >> 16 & 255, e >> 8 & 255, e & 255)))
        Y_PAG, X0 = 50, 380
        yb = Y_PAG + 172 + 18
        def botao(k):   # una esquina de la casilla del botón k (desde 0), lejos del número
            return cor(X0 + (k % 16) * 27 + 3, yb + (k // 16) * 26 + 3)
        verificar(perto(botao(1), 0x2A6FD8), f"el botón 2 se ve pulsado ({botao(1)})")
        verificar(perto(botao(9), 0x2A6FD8), "el botón 10 se ve pulsado")
        verificar(perto(botao(0), 0xF2F2F2), f"el botón 1 no ({botao(0)})")
        # El punto del stick izquierdo: X -32768 e Y 32767 = abajo a la izquierda del primer cuadro.
        verificar(perto(cor(X0 + 6, Y_PAG + 48 + 48 + 41), 0xC0392B), f"el punto del stick, abajo a la izquierda ({cor(X0 + 6, Y_PAG + 137)})")
        verificar(perto(cor(X0 + 48, Y_PAG + 48 + 48), 0xD0D0D0) or perto(cor(X0 + 48, Y_PAG + 48 + 48), 0xFFFFFF),
                  "el centro del cuadro está libre")
        # El Xbox 360: segundo de la lista.
        m.clique(ox + 100, oy + Y_PAG + 30 + 20 + 10)
        time.sleep(1.5)
        m.captura("H307-2-mandos-xbox360")
        w, h, rgb = h228.pantalla(m, "h307-gui2")
        verificar(perto(botao(0), 0x2A6FD8) and perto(botao(4), 0x2A6FD8),
                  "Xbox 360: A (botón 1) y LB (botón 5) pulsados")
        verificar(perto(cor(X0 + 108 + 48, Y_PAG + 48 + 48 - 41), 0xC0392B),
                  f"Xbox 360: el stick derecho arriba (Y invertida) ({cor(X0 + 156, Y_PAG + 55)})")
        # Buscar joystick: el QEMU no tiene puerto de juegos.
        m.clique(ox + 24 + 80, oy + Y_PAG + 208 + 14)
        fim = time.time() + 15
        while time.time() < fim and "configuracion: mandos detectar -> 0" not in m.serial():
            time.sleep(0.3)
        verificar("configuracion: mandos detectar -> 0" in m.serial(), "«Buscar joystick» sin puerto de juegos no se cuelga")
        time.sleep(0.5)
        m.captura("H307-3-buscar-joystick")


def sdl2():
    import h228_openjazz as h228
    import h235_gnuboy as h235
    if not os.path.isfile(h235.GNUBOY):
        raise AssertionError("falta %s (make: GNUBOY=1, sin SEM_GPL)" % h235.GNUBOY)
    rom = os.path.join(SAIDA, "saguitest.gb")
    h235.generar_rom(rom)
    img = imagem_grafica(["--jogos", h235.jogos_con({"saguitest.gb": rom}), "--apps", APPS_RAIZ])
    with Maquina("h307s", "ide", imagem=img) as m:
        h228.terminal(m)
        marca = len(m.serial())
        # El botón 3 del Dual Action es la B del mapeo Logitech; GNUBoy pone la B del mando
        # (K_JOY1) en la A de la Game Boy, que en la ROM de prueba invierte la paleta.
        m.digitar("gnuboy /jogos/gb/saguitest.gb & sleep 20; mandos --inyectar 0 %s; sleep 12; mandos --inyectar 0 %s\n"
                  % (da(botoes=1 << 2), da()), 0.03)
        h235.esperar(m, r"\[GNUBOY\] primer cuadro dibujado", 90, desde=marca)
        h235.esperar(m, r"mando abierto como SDL_GameController: Mando de prueba \(Dual Action\) \(mapeo Logitech\)", 30, desde=marca)
        verificar(True, "GNUBoy abre el Dual Action de prueba como SDL_GameController (SDL_CONTROLLERDEVICEADDED)")
        time.sleep(2)
        normal = [h235.TONO3, h235.TONO1, h235.TONO2, h235.TONO0]
        h235.comprobar_cuadrantes(m, "h307-gb-normal", normal, "la ROM pinta los cuatro tonos")
        h235.esperar(m, r"mando: boton b pulsado", 60, desde=marca)
        inv = [h235.TONO0, h235.TONO2, h235.TONO1, h235.TONO3]
        fim = time.time() + 8
        ok = False
        while time.time() < fim and not ok:
            cs, _, _ = h235.cuadrantes(m, "h307-gb-inv")
            ok = all(h235.parecido(c, e) for c, e in zip(cs, inv))
        m.captura("H307-4-gnuboy-mando")
        verificar(ok, "el botón del mando llega a GNUBoy: la paleta se invierte (%s)" % " ".join(h235.hex3(c) for c in cs))
        h235.esperar(m, r"mando: boton b suelto", 30, desde=marca)
        time.sleep(2)
        h235.comprobar_cuadrantes(m, "h307-gb-volta", normal, "al soltarlo la paleta vuelve")


def dosbox():
    import shutil
    import dosbox_com
    import h228_openjazz as h228
    import h294_dosbox as h294
    if not os.path.isfile(h294.DOSBOX):
        raise AssertionError("falta %s (make: hace falta el compilador de C++)" % h294.DOSBOX)
    d = os.path.join(SAIDA, "h307-jogos")
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(os.path.join(d, "dos"))
    with open(os.path.join(d, "dos", "JOY.COM"), "wb") as f:
        f.write(dosbox_com.generar_joystick())
    img = imagem_grafica(["--jogos", d, "--apps", APPS_RAIZ])
    with Maquina("h307d", "ide", imagem=img) as m:
        h228.terminal(m)
        # Stick a la izquierda (0) y abajo (255) y el botón 1 pulsado.
        m.digitar("mandos --inyectar 0 " + da(x=0, y=0xFF, botoes=1) + "\n", 0.04)
        time.sleep(1)
        desde = len(m.serial())
        m.digitar("dosbox /jogos/dos/JOY.COM\n", 0.04)
        h294.esperar(m, r"\[DOSBOX\] programa JOY.COM en /jogos/dos", 60, desde=desde)
        h294.esperar(m, r"\[DOSBOX\] joystick abierto: Mando de prueba \(Dual Action\)", 60, desde=desde)
        h294.esperar(m, r"Using joystick Mando de prueba \(Dual Action\) with 4 axes, 12 buttons and 1 hat", 60, desde=desde)
        verificar(True, "DOSBox abre el mando por la SDL 1.2 (joysticktype=auto)")
        h294.esperar(m, r"\[DOSBOX\] fin \(0\)", 90, desde=desde)
        h294.sin_fallos(m, desde, "JOY.COM")
        h228.terminal(m)
        desde = len(m.serial())
        m.digitar("cat /jogos/dos/JOY.TXT\n", 0.04)
        time.sleep(2.5)
        r = re.findall(r"B=([0-9A-F]{2}) X=([0-9A-F]{2}) Y=([0-9A-F]{2}) C=([0-9A-F]{2})", m.serial()[desde:])
        verificar(bool(r), "JOY.COM escribe JOY.TXT")
        b, x, y, c = (int(v, 16) for v in r[-1])
        print("    JOY.TXT: B=%02X X=%02X Y=%02X C=%02X" % (b, x, y, c))
        verificar(c == 0, "la BIOS de DOSBox dice que hay joystick")
        verificar(b & 0x10 == 0 and b & 0x20, f"el botón 1 pulsado y el 2 no (puerto 0x201 = {b:02X})")
        verificar(x <= 8 and y >= 0xF6, f"el joystick A a la izquierda y abajo (X={x}, Y={y})")


def main():
    partes = sys.argv[1:] or ["unidad", "os", "persiste", "gui", "sdl2", "dosbox"]
    if "unidad" in partes:
        unidad()
    if "os" in partes:
        provar_os()
    if "persiste" in partes:
        persiste()
    if "gui" in partes:
        gui()
    if "sdl2" in partes:
        sdl2()
    if "dosbox" in partes:
        dosbox()
    print("H307 OK")


if __name__ == "__main__":
    main()
