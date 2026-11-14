#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H249 (D251, mejora 097) - entrada inyectada para el escritorio remoto (SIS_INYECTAR).

Imagen con /testes/inyectar copiado como /bin/vncd y dos servicios que arrancan solos:
  - vncd (/bin/vncd --servicio): el único que debe recibir el permiso;
  - inyfalso (/testes/inyectar --servicio --falso): lanzado por el gestor, pero desde otra
    ruta: se le niega.
Desde el Terminal, /bin/vncd a mano: se le niega (no lo lanzó el gestor de servicios).
Con el permiso:
  - antes de pedirlo, inyectar da ERRO_ACESSO;
  - argumentos fuera de rango (tecla, modificadores, rueda, botones, operación) dan
    ERRO_INVALIDO, y una posición fuera de la pantalla se recorta;
  - un clic inyectado llega a la ventana /testes/inyectar --ventana en su sitio;
  - teclas con sus modificadores (Shift, Ctrl+C = 3, á, F5) llegan como EV_TECLA;
  - la rueda llega como EV_RODA;
  - Alt+Tab inyectado (Alt solo, Tab, soltar Alt) cambia de ventana: la tecla siguiente
    no llega a la ventana; un clic inyectado le devuelve el foco y la siguiente sí;
  - tras soltar el permiso, inyectar vuelve a dar ERRO_ACESSO;
  - si el servicio termina con Ctrl+Alt pulsados, el kernel los suelta: una tecla real
    llega después sin modificadores.
"""
import os
import re
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA  # noqa: E402
import h228_openjazz as h228  # noqa: E402
from h60_barra_tareas import esperar  # noqa: E402

TRAB = os.path.join(SAIDA, "h249")
ERRO_INVALIDO, ERRO_ACESSO = -1, -14


def construir_imagen():
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(TRAB, "rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    prog = os.path.join(raiz, "testes", "inyectar")
    verificar(os.path.exists(prog), "/testes/inyectar está en el ramdisk")
    shutil.copy(prog, os.path.join(raiz, "bin", "vncd"))
    serv = os.path.join(raiz, "etc", "servicios")
    with open(os.path.join(serv, "vncd.conf"), "w") as f:
        f.write("# prueba h249: /testes/inyectar como /bin/vncd\nnombre=vncd\ndescripcion=prueba h249\n"
                "comando=/bin/vncd --servicio\narrancar=si\nreiniciar=no\n")
    with open(os.path.join(serv, "inyfalso.conf"), "w") as f:
        f.write("# prueba h249: el mismo programa desde otra ruta\nnombre=inyfalso\ndescripcion=prueba h249\n"
                "comando=/testes/inyectar --servicio --falso\narrancar=si\nreiniciar=no\n")
    sgfs = os.path.join(TRAB, "h249.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(TRAB, "h249.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa"],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def main():
    os.makedirs(TRAB, exist_ok=True)
    img = construir_imagen()
    with Maquina("h249", "ide", imagem=img) as m:
        g = esperar(m, r"inyectar: servicio antes=(-?\d+) estado=(-?\d+)", 180)
        verificar(int(g[0]) == ERRO_ACESSO, f"sin pedir el permiso, inyectar da ERRO_ACESSO ({g[0]})")
        verificar(int(g[1]) == 0, f"/bin/vncd lanzado por el gestor de servicios recibe el permiso ({g[1]})")
        verificar(re.search(r"\[INYECTAR\] permiso concedido a vncd", m.serial()), "el kernel lo anota")
        g = esperar(m, r"inyectar: falso antes=(-?\d+) estado=(-?\d+)", 60)
        verificar(int(g[1]) == ERRO_ACESSO, f"el mismo programa desde /testes, lanzado por el gestor: denegado ({g[1]})")
        verificar(re.search(r"permiso denegado a inyectar \(pid \d+\): no es /bin/vncd", m.serial()),
                  "el kernel anota por qué (no es /bin/vncd)")
        g = esperar(m, r"inyectar: validacion ([-\d ]+)", 30)
        vals = [int(v) for v in g.split()]
        verificar(vals == [ERRO_INVALIDO] * 7, f"argumentos fuera de rango: ERRO_INVALIDO ({vals})")
        g = esperar(m, r"\[MOUSE\] roda 1 em \((\d+),(\d+)\)", 30)
        x, y = int(g[0]), int(g[1])
        verificar(600 <= x < 30000 and 400 <= y < 30000 and (x + 1) % 8 == 0 and (y + 1) % 8 == 0,
                  f"una posición fuera de la pantalla se recorta a la esquina ({x},{y})")

        h228.terminal(m)
        marca = len(m.serial())
        m.digitar("/bin/vncd --servicio\n", 0.05)
        esperar(m, r"inyectar: servicio antes=-14 estado=-14", 30, desde=marca)
        verificar(re.search(r"permiso denegado a vncd \(pid \d+\): no lo lanzo el gestor de servicios",
                            m.serial()[marca:]), "/bin/vncd lanzado a mano desde el Terminal: denegado")

        marca = len(m.serial())
        m.digitar("/testes/inyectar --ventana\n", 0.05)
        esperar(m, r"inyectar-ventana: lista", 30, desde=marca)
        esperar(m, r"inyectar: empieza", 60, desde=marca)
        esperar(m, r"inyectar: otra vez (-?\d+)", 90, desde=marca)
        esperar(m, r"\[INYECTAR\] permiso retirado al pid \d+ \(el proceso termino\)", 30, desde=marca)
        s = m.serial()[marca:]
        vent = [v.strip() for v in re.findall(r"inyectar-ventana: (.*)", s)]
        print("    ventana:", vent)

        verificar(re.search(r"\[MOUSE\] botao esquerdo apertado em \(162,204\)", s),
                  "el clic inyectado pasa por el ratón del kernel en su sitio (162,204)")
        verificar("clique (60,80) botao 1" in vent, "la ventana recibe el clic en (60,80)")
        teclas = [t for t in vent if t.startswith("tecla")]
        esperado = ["tecla 104 mods 0", "tecla 65 mods 1", "tecla 3 mods 2", "tecla 225 mods 0", "tecla 270 mods 0"]
        verificar(teclas[:5] == esperado, f"teclas con sus modificadores: h, Shift+A, Ctrl+C=3, á, F5 ({teclas[:5]})")
        verificar("roda 2" in vent and "roda -1" in vent, "la rueda inyectada llega como EV_RODA")
        verificar(re.search(r"\[GUI\] alt\+tab: ativada janela", s), "Alt+Tab inyectado cambia de ventana")
        verificar("tecla 120 mods 0" not in vent, "tras Alt+Tab, la «x» no llega a la ventana (fue al Terminal)")
        verificar(teclas[5:6] == ["tecla 122 mods 0"], f"un clic inyectado devuelve el foco: llega la «z» ({teclas[5:]})")
        g = re.search(r"inyectar: soltado (-?\d+) despues=(-?\d+)", s)
        verificar(g and int(g.group(1)) == 0 and int(g.group(2)) == ERRO_ACESSO,
                  "tras soltar el permiso, inyectar da ERRO_ACESSO")
        g = re.search(r"inyectar: otra vez (-?\d+)", s)
        verificar(int(g.group(1)) == 0, "se puede volver a pedir")

        # Ctrl+Alt pulsados al morir: el kernel los soltó y una tecla real llega sin modificadores.
        time.sleep(1)
        marca = len(m.serial())
        m.tecla("w")
        g = esperar(m, r"inyectar-ventana: tecla (\d+) mods (\d+)", 15, desde=marca)
        verificar(g == ("119", "0"), f"el servicio murió con Ctrl+Alt: el kernel los soltó (tecla real: {g})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
