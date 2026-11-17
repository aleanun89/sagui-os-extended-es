#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H370 - Administrador de tareas modernizado (D368).

QEMU con 512 MB y un programa que obliga a usar la swap (/testes/troca llenar):
  - Procesos: la búsqueda al escribir filtra la tabla, el botón derecho abre el menú de la fila y el de la
    cabecera (columnas y árbol), doble clic en una fila lleva a Detalles;
  - Detalles: el proceso elegido con su planificación y la lista de hilos (INFO_HILOS);
  - Rendimiento: barra lateral con CPU, Memoria, Swap, Disco, Red, GPU y Sensores; cada recurso se elige con un
    clic y muestra su panel; la swap enseña uso real (> 0) mientras el programa la llena, y la caché de
    disco (INFO_DISCO_CACHE, nueva) cuenta accesos;
  - el administrador apenas consume (no se repinta la tabla si nada cambia: se mide la CPU del propio tareas).
Capturas en docs/capturas/H370-*.png."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste  # noqa: E402

TAB_Y = 20                    # centro vertical de las pestañas
ABA_X = {"procesos": 60, "rendimiento": 170, "ventanas": 278, "red": 360, "servicios": 440, "detalles": 530}
LINHA0_Y = 65 + 9             # centro de la primera fila de la tabla
SB_X, SB_Y0, SB_A = 12 + 80, 42, 52     # barra lateral: centro x, y del primer elemento, alto de cada uno
RECURSOS = ["cpu", "memoria", "swap", "disco", "red", "gpu", "sensores"]


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24, fx=int(g[2]), fy=int(g[3]))


def contar(m, padrao):
    return len(re.findall(padrao, m.serial()))


def esperar_novo(m, padrao, n_antes, tempo=20):
    fim = time.time() + tempo
    while time.time() < fim:
        achados = re.findall(padrao, m.serial())
        if len(achados) > n_antes:
            return achados[-1]
        time.sleep(0.2)
    raise FalhaTeste(f"tiempo agotado esperando {padrao!r}")


def main():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h370", "ide", imagem=img, memoria="512M") as m:
        m.esperar_serial("sagui:/$", 240)
        time.sleep(1)
        m.comando("/testes/girar &")
        m.esperar_serial("girar: comecando", 20)
        pid_g = int(re.findall(r"girar: comecando \(pid (\d+)\)", m.serial())[-1])
        # Un programa que llena 700 MB en una máquina de 512: la swap comprimida tiene que trabajar.
        m.comando("/testes/troca llenar 700 12 &")
        time.sleep(3)
        m.comando("tareas")
        m.esperar_serial("tareas: pronto", 30)
        pid_t = int(re.findall(r"tareas: pronto \(pid (\d+)\)", m.serial())[-1])
        j = janela(m.serial(), "Administrador de tareas")
        ox, oy = j["x"], j["y"]
        time.sleep(2)

        # --- Procesos: búsqueda, menús, doble clic
        m.clique(ox + 300, oy + 470 - 34)   # un sitio inofensivo para dar el foco
        n = contar(m, r"tareas: buscar")
        m.digitar("gir")
        b = esperar_novo(m, r'tareas: buscar "(.*)"', n + 2)
        verificar(b == "gir", f"escribir busca: {b!r}")
        time.sleep(1.5)
        lista = re.findall(r"tareas: lista \d+ procesos: ([^\r\n]*)", m.serial())[-1]
        verificar("girar" in lista, "la lista de procesos incluye el girar")
        m.captura("H370-procesos-busqueda")
        # Una fila: el girar es la única que casa con la búsqueda.
        n = contar(m, r"tareas: seleccionado pid")
        m.clique(ox + 125, oy + LINHA0_Y)
        s = esperar_novo(m, r"tareas: seleccionado pid (\d+)", n)
        verificar(int(s) == pid_g, f"con la búsqueda, la primera fila es el girar (pid {pid_g})")
        n = contar(m, r"tareas: menu proceso")
        m.clique(ox + 125, oy + LINHA0_Y, botao=2)
        esperar_novo(m, r"tareas: menu proceso pid (\d+)", n)
        time.sleep(0.6)
        m.captura("H370-procesos-menu")
        m.tecla("esc")
        time.sleep(0.5)
        m.tecla("esc")   # Esc sin menú: borra la búsqueda
        time.sleep(0.8)
        n = contar(m, r"tareas: menu columnas")
        m.clique(ox + 60, oy + 42 + 10, botao=2)
        esperar_novo(m, r"tareas: menu columnas", n)
        time.sleep(0.6)
        m.captura("H370-procesos-columnas")
        m.tecla("esc")
        time.sleep(0.5)
        # Doble clic en la fila: va a Detalles con ese proceso.
        n = contar(m, r"tareas: detalles pid")
        m.clique(ox + 125, oy + LINHA0_Y + 18 * 3, duplo=True)
        m.esperar_serial("tareas: pestana detalles", 10)
        d = esperar_novo(m, r"tareas: detalles pid (-?\d+) hilos (-?\d+) extra (\d+) nice (-?\d+)", n)
        verificar(int(d[0]) > 0 and int(d[3]) > -99, f"Detalles: proceso {d[0]}, {d[1]} hilos, nice {d[3]}")
        time.sleep(1.2)
        m.captura("H370-detalles")

        # --- Rendimiento
        m.clique(ox + ABA_X["rendimiento"], oy + TAB_Y)
        m.esperar_serial("tareas: pestana rendimiento", 10)
        for r, nombre in enumerate(RECURSOS):
            if nombre != "cpu":   # CPU es el recurso por defecto
                n = contar(m, rf"tareas: recurso {nombre}")
                m.clique(ox + SB_X, oy + SB_Y0 + r * SB_A + 22)
                esperar_novo(m, rf"tareas: recurso {nombre}", n, 10)
            time.sleep(1.5)
            if nombre == "swap":
                # La swap trabaja: el almacén tiene datos mientras el programa llena la memoria.
                fin = time.time() + 240
                usada = 0
                while time.time() < fin:
                    g = re.findall(r"tareas: swap tope (\d+) KB almacen (\d+) KB datos (\d+) KB comprimida (\d+) KB "
                                   r"salidas (\d+) entradas (\d+) sale/s (\d+) entra/s (\d+)", m.serial())
                    if g and int(g[-1][1]) > 0:
                        usada = int(g[-1][1])
                        break
                    time.sleep(2)
                g = [int(x) for x in g[-1]] if g else [0] * 8
                verificar(usada > 0 and g[0] > 0 and g[2] >= g[3] > 0,
                          f"Swap: tope {g[0]} KB, almacén {g[1]} KB, {g[2]} KB sin comprimir, {g[3]} KB comprimidos")
                verificar(g[4] > 0, f"Swap: {g[4]} páginas sacadas desde el arranque")
                time.sleep(1.5)
            if nombre == "disco":
                g = re.findall(r"tareas: disco ativa (\d+) bloques (\d+)/(\d+) sucios (\d+) aciertos (\d+) fallos (\d+) "
                               r"grabados (\d+) lee (\d+) KB/s escribe (\d+) KB/s", m.serial())[-1]
                verificar(int(g[0]) == 1 and int(g[2]) > 0 and int(g[4]) + int(g[5]) > 0,
                          f"Disco: caché activa (tope {g[2]} bloques, ahora {g[1]}) y {int(g[4]) + int(g[5])} lecturas")
            if nombre == "gpu":
                verificar(re.search(r"tareas: gpu 2d \S+ \d+ ops/s 3d \S+ \d+ cuadros/s bateria -?\d+%", m.serial()) is not None,
                          "GPU: el log trae el estado 2D y 3D")
            m.captura(f"H370-rendimiento-{nombre}")
        r = re.findall(r"tareas: rendimiento cpu (\d+)% mem (\d+)/(\d+) KB", m.serial())[-1]
        verificar(0 < int(r[1]) <= int(r[2]), f"Rendimiento: memoria {r[1]}/{r[2]} KB")

        # --- Consumo del propio administrador: con la tabla quieta casi no gasta CPU.
        m.clique(ox + ABA_X["procesos"], oy + TAB_Y)
        m.esperar_serial("tareas: pestana procesos", 10)
        m.clique(ox + 300, oy + 470 - 34)
        m.tecla("esc")
        time.sleep(2)
        out = m.comando("tareas -t")
        mt = re.search(r"\s*%d\s+\d+\s+\S+\s+(\d+),(\d) " % pid_t, out)
        if mt:
            verificar(int(mt.group(1)) <= 10, f"el administrador gasta poca CPU ({mt.group(1)},{mt.group(2)} %)")
        # Cerrar con la X.
        m.clique(j["fx"] + j["l"] + 4 - 2 - 20 + 8, j["fy"] + 12)
        m.esperar_serial(f"processo {pid_t} (tareas) terminou", 10)
    return 0


if __name__ == "__main__":
    sys.exit(main())
