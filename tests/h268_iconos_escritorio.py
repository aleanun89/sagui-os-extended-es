#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H268 (D268, mejora 101) - ordenar los iconos del escritorio.

Criterio:
  - arrastrar un icono a una casilla libre: se coloca en la rejilla (registro [ICONOS] y captura);
  - soltarlo encima de otro: se intercambian;
  - submenu "Ordenar iconos" del menú del escritorio: por nombre y por tipo;
  - selección múltiple por rectángulo y mover varios a la vez (el Ctrl+clic no se puede probar con
    el monitor de QEMU, que no mantiene teclas pulsadas);
  - teclado: flechas mueven el foco e Intro abre el programa;
  - las casillas se guardan en /home/.config/escritorio.conf y sobreviven a un reinicio;
  - al cambiar de resolución ningún icono queda fuera de la pantalla.
"""
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel, SAIDA, BUILD  # noqa: E402

DIREITO = 2
ICONE_X, ICONE_Y0, PASSO_X, PASSO_Y = 20, 40, 104, 84
LINHA_A, SEP_A = 22, 9   # kernel/gui/menu.c
DADOS_MB = 32
NOMES = ["acerca", "archivos", "banana", "calculadora", "chip8", "configuracion", "discos", "editor",
         "paint", "sagui3d", "tareas", "terminal", "visor"]   # por nombre, sin el pendrive
TIPO = {"archivos": 0, "discos": 0, "editor": 1, "paint": 1, "visor": 1}   # el resto, 2 (programas)


def imagem_persistente(nome):
    """Imagen normal con la partición de datos (/home) que se conserva entre arranques."""
    destino = os.path.join(SAIDA, nome + ".img")
    os.makedirs(SAIDA, exist_ok=True)
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(BUILD, "boot", "mbr.bin"), "--estagio2", os.path.join(BUILD, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(BUILD, "kernel.bin"), "--kernel-elf", os.path.join(BUILD, "kernel.elf"),
                    "--ramdisk", os.path.join(BUILD, "ramdisk.sgfs"), "--saida", destino, "--vesa",
                    "--dados-mb", str(DADOS_MB)], check=True, stdout=subprocess.DEVNULL)
    return destino


def esperar(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def casillas(m, desde=0):
    """Las casillas de la última línea «[ICONOS] casillas...» del registro (desde un offset)."""
    ls = re.findall(r"\[ICONOS\] casillas[^:\n]*:([^\n]*)", m.serial()[desde:])
    if not ls:
        return None
    return {k: (int(c), int(f)) for k, c, f in re.findall(r"(\w+)=(\d+),(\d+)", ls[-1])}


def esperar_casillas(m, desde, tempo=15):
    fim = time.time() + tempo
    while time.time() < fim:
        c = casillas(m, desde)
        if c:
            return c
        time.sleep(0.2)
    raise AssertionError("no hay línea [ICONOS] casillas")


def centro(c, f):
    return ICONE_X + c * PASSO_X + 48, ICONE_Y0 + f * PASSO_Y + 30


def sem_solapes(cs):
    return len(set(cs.values())) == len(cs)


def abrir_menu(m, x, y):
    marca = len(m.serial())
    m.clique(x, y, botao=DIREITO)
    g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", desde=marca)
    return tuple(int(v) for v in g)


def linha(menu, i, seps=0):
    mx, my, ml = menu[0], menu[1], menu[2]
    return mx + ml // 2, my + 2 + (i - seps) * LINHA_A + seps * SEP_A + LINHA_A // 2


def clique_no_submenu(m, x, y):
    """Clic sin pasar por la esquina (mouse_para): el submenú se cerraría al salir de su línea."""
    m.mouse_mover(x, m._my)   # primero en horizontal, por la línea del submenú padre
    m.mouse_mover(x, y)
    m.monitor("mouse_button 1")
    time.sleep(0.05)
    m.monitor("mouse_button 0")
    time.sleep(0.5)


def ordenar_por_menu(m, texto, x_fundo, y_fundo):
    """Menú del escritorio > Ordenar iconos > texto (clic en las líneas del submenú)."""
    menu = abrir_menu(m, x_fundo, y_fundo)
    ls = re.findall(r"\[GUI\] menu linhas: ([^\n]*)", m.serial())[-1].split("|")
    i_ord = [k for k, l in enumerate(ls) if "Ordenar iconos" in l][0]
    seps = ls[:i_ord].count("-")
    marca = len(m.serial())
    m.clique(*linha(menu, i_ord, seps))
    g = esperar(m, r"\[GUI\] submenu \d+ \(Ordenar iconos\) aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", desde=marca)
    sub = tuple(int(v) for v in g)
    sl = re.findall(r"\[GUI\] menu linhas: ([^\n]*)", m.serial())[-1].split("|")
    i_it = [k for k, l in enumerate(sl) if texto in l][0]
    marca = len(m.serial())
    clique_no_submenu(m, *linha(sub, i_it))
    esperar(m, r"\[ICONOS\] ordenar:", desde=marca)
    return marca


def main():
    img = imagem_persistente("h268")
    # ---------------------------------------------------------------- sesión 1
    with Maquina("h268-a", "ide", imagem=img, snapshot=False) as m:
        esperar_boot = m.esperar_serial("[ICONOS] casillas al arrancar", 120)
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        c0 = casillas(m)
        verificar(c0 and len(c0) == 13 and c0["archivos"] == (0, 0) and c0["terminal"] == (0, 1) and c0["tareas"] == (1, 0),
                  f"sin archivo de configuración, las casillas son las de siempre {c0}")
        mt = re.findall(r'"Terminal" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())[-1]
        tx, ty, tl, ta = int(mt[2]), int(mt[3]), int(mt[0]) + 4, int(mt[1]) + 28
        # una zona vacía del escritorio, fuera de la ventana del Terminal
        vazio = (1000, 700) if not (tx <= 1000 < tx + tl and ty <= 700 < ty + ta) else (1000, 60)

        # ---- arrastrar a una casilla libre (la de abajo de la columna 0)
        marca = len(m.serial())
        m.arrastar(*centro(0, 0), *centro(0, 7))
        c = esperar_casillas(m, marca)
        verificar(c["archivos"] == (0, 7), f"arrastrar Archivos a la casilla libre (0,7): {c['archivos']}")
        verificar(sem_solapes(c), "ningún icono se solapa")
        verificar(all(c[k] == c0[k] for k in c0 if k != "archivos"), "los demás iconos no se mueven")
        png = m.captura("H268-1-arrastrado")
        x, y = centro(0, 7)
        verificar(cor_do_pixel(png, 47, 65 + 7 * PASSO_Y) == (255, 213, 46), "la carpeta se ve en su casilla nueva")
        verificar(cor_do_pixel(png, 47, 65) != (255, 213, 46), "y ya no está en la antigua")

        # ---- soltarlo encima de otro: se intercambian
        marca = len(m.serial())
        m.arrastar(*centro(0, 1), *centro(0, 2))   # Terminal sobre Editor
        c = esperar_casillas(m, marca)
        verificar(c["terminal"] == (0, 2) and c["editor"] == (0, 1), f"Terminal sobre Editor: se intercambian ({c['terminal']}, {c['editor']})")
        verificar(sem_solapes(c), "sin solapes tras el intercambio")

        # ---- ordenar por nombre
        marca = ordenar_por_menu(m, "Por nombre", *vazio)
        c = esperar_casillas(m, marca)
        orden = sorted(c, key=lambda k: (c[k][0], c[k][1]))
        verificar(orden == NOMES, f"ordenar por nombre: {orden}")
        verificar(sem_solapes(c) and c["acerca"] == (0, 0), "el primero por nombre va a la casilla (0,0)")
        time.sleep(0.5)
        m.captura("H268-2-por-nombre")
        # ---- por tipo
        marca = ordenar_por_menu(m, "Por tipo", *vazio)
        c = esperar_casillas(m, marca)
        orden = sorted(c, key=lambda k: (c[k][0], c[k][1]))
        esperado = sorted(NOMES, key=lambda k: (TIPO.get(k, 2), k))
        verificar(orden == esperado, f"ordenar por tipo: {orden}")
        # ---- por fecha (en el ramdisk todas iguales: desempata el nombre) y alinear
        marca = ordenar_por_menu(m, "Por fecha", *vazio)
        c = esperar_casillas(m, marca)
        verificar(sem_solapes(c) and len(c) == 13, "ordenar por fecha deja las casillas válidas")
        marca = ordenar_por_menu(m, "Alinear", *vazio)
        c = esperar_casillas(m, marca)
        verificar(sem_solapes(c), "alinear a la rejilla sin solapes")

        # ---- selección múltiple: rectángulo y mover el grupo
        marca = ordenar_por_menu(m, "Por nombre", *vazio)
        antes = esperar_casillas(m, marca)
        marca = len(m.serial())
        # rectángulo que cubre las casillas (0,0) y (0,1) (acerca y archivos)
        m.mouse_para(10, 36)
        m.monitor("mouse_button 1")
        time.sleep(0.2)
        m.mouse_mover(110, 170)
        m.monitor("mouse_button 0")
        time.sleep(0.5)
        esperar(m, r"seleccion por rectangulo: 2 iconos", desde=marca)
        verificar(True, "el rectángulo de selección elige 2 iconos")
        png = m.captura("H268-3-seleccion")
        verificar(cor_do_pixel(png, 28, 50) != cor_do_pixel(png, 28, 700), "el icono seleccionado se ve realzado")
        marca = len(m.serial())
        m.arrastar(*centro(0, 1), *centro(2, 5))   # el grupo, 2 columnas a la derecha y 4 filas abajo
        c = esperar_casillas(m, marca)
        verificar(c["acerca"] == (2, 4) and c["archivos"] == (2, 5), f"el grupo se mueve junto: {c['acerca']}, {c['archivos']}")
        verificar(sem_solapes(c), "sin solapes tras mover el grupo")
        # ---- teclado: flechas e Intro
        m.clique(*centro(0, 2))           # un icono (el tercero por nombre, Banana)
        marca = len(m.serial())
        m.tecla("down")
        esperar(m, r"\[ICONOS\] foco: calculadora", desde=marca)
        verificar(True, "flecha abajo mueve el foco al icono de abajo (Calculadora)")
        marca = len(m.serial())
        m.tecla("ret")
        esperar(m, r"\[GUI\] icone Calculadora -> /bin/calculadora", desde=marca)
        verificar(True, "Intro abre el programa del icono con foco")
        time.sleep(1)

        # ---- estado final de la sesión: casillas guardadas
        marca = len(m.serial())
        m.arrastar(*centro(0, 3), *centro(3, 6))
        c_final = esperar_casillas(m, marca)
        esperar(m, r"\[ICONOS\] guardado en /home/\.config/escritorio\.conf \(\d+ bytes\): 0", desde=marca)
        verificar(True, "las casillas se guardan en /home/.config/escritorio.conf (escritura atómica)")
        verificar(sem_solapes(c_final), "casillas finales válidas")
        m.tecla("alt-f4")   # cierra la Calculadora, que tiene el foco
        time.sleep(1)
        m.clique(tx + 60, ty + 12)   # barra de título del Terminal: le devuelve el foco
        m.digitar("reiniciar\n", 0.04)
        fim = time.time() + 60
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.3)
        verificar(m.proc.poll() is not None, "reiniciar: la máquina se reinició")

    # ---------------------------------------------------------------- sesión 2: persistencia y resolución
    with Maquina("h268-b", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("[ICONOS] casillas al arrancar", 120)
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        verificar("[ICONOS] cargado /home/.config/escritorio.conf: 13 casillas" in m.serial(), "tras reiniciar se carga el archivo")
        c = casillas(m)
        verificar(c == c_final, f"las casillas se conservan tras reiniciar {c}")
        m.captura("H268-4-persistente")
        # ---- cambio de resolución desde el menú: Resolución > 800x600
        mt = re.findall(r'"Terminal" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())[-1]
        tx, ty, tl, ta = int(mt[2]), int(mt[3]), int(mt[0]) + 4, int(mt[1]) + 28
        vazio = (1000, 700) if not (tx <= 1000 < tx + tl and ty <= 700 < ty + ta) else (1000, 60)
        menu = abrir_menu(m, *vazio)
        ls = re.findall(r"\[GUI\] menu linhas: ([^\n]*)", m.serial())[-1].split("|")
        i_res = [k for k, l in enumerate(ls) if "Resoluci" in l][0]
        marca = len(m.serial())
        m.clique(*linha(menu, i_res, ls[:i_res].count("-")))
        g = esperar(m, r"\[GUI\] submenu \d+ \(Resoluci.n[^)]*\) aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", desde=marca)
        sub = tuple(int(v) for v in g)
        sl = re.findall(r"\[GUI\] menu linhas: ([^\n]*)", m.serial())[-1].split("|")
        alvo = [k for k, l in enumerate(sl) if l.lstrip("*~") == "800x600"]
        verificar(alvo, f"el submenú ofrece 800x600 ({sl})")
        marca = len(m.serial())
        clique_no_submenu(m, *linha(sub, alvo[0]))
        esperar(m, r"\[ICONOS\] casillas tras cambiar de resolucion:", desde=marca, tempo=30)
        c800 = casillas(m, marca)
        filas = (600 - 40) // PASSO_Y
        cols = (800 - ICONE_X) // PASSO_X
        verificar(all(cc < cols and ff < filas for cc, ff in c800.values()), f"a 800x600 ningún icono queda fuera ({c800})")
        verificar(sem_solapes(c800) and len(c800) == 13, "y no se solapan")
        # el orden relativo (por columnas) se conserva
        o1 = sorted(c_final, key=lambda k: (c_final[k][0], c_final[k][1]))
        o2 = sorted(c800, key=lambda k: (c800[k][0], c800[k][1]))
        verificar(o1 == o2, f"el orden relativo se conserva al cambiar de resolución ({o2})")
        time.sleep(1)
        m.captura("H268-5-800x600")
    return 0


if __name__ == "__main__":
    sys.exit(main())
