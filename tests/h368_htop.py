#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H368 (D366) - htop: monitor de procesos interactivo para el terminal (clon propio).

QEMU, en el Terminal del escritorio (sesión como sagui, TERM=xterm-256color):
  - la cabecera: barra de CPU en verde, memoria y swap con barras, Tareas, hilos, tiempo encendido;
  - la lista: PID, USUARIO, PRI, NI, RES, S, CPU%, MEM%, TIEMPO+ y ORDEN, con el proceso que gira el primero;
  - la barra de teclas F1-F10 abajo;
  - ordenar (M, P, I y clic en la cabecera), árbol (F5), buscar (F3), filtrar (F4);
  - nice (F8 y F7) de un proceso de prueba y matarlo (F9, con su ventana de confirmación);
  - ratón: clic en la cabecera, en una línea y en la barra de F;
  - F1 (ayuda) y F2 (configuración);
  - al salir (F10) vuelve la pantalla de antes y queda ~/.config/htoprc;
  - sin terminal (en una tubería) avisa y sale.
Capturas en docs/capturas/H368-*.png."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA  # noqa: E402
import h263_terminal_color as h263  # noqa: E402

COLS, LINS = 94, 28
cian = lambda r, g, b: g > 120 and b > 120 and r < 110          # noqa: E731
verde = lambda r, g, b: g > r + 40 and g > b + 40               # noqa: E731


def ver(m, t):
    time.sleep(0.4)
    return h263.pantalla(m, t, False, cols=COLS, lins=LINS)


def esperar(m, t, patron, tiempo=10, cada=0.7):
    fin = time.time() + tiempo
    while True:
        tela, px, y0, x0 = ver(m, t)
        if any(re.search(patron, l) for l in tela) or time.time() > fin:
            return tela, px, y0, x0
        time.sleep(cada)


def fila_sel(px, y0, x0):
    """La fila resaltada (fondo cian a la derecha, donde no hay texto)."""
    for f in range(LINS - 1):
        if cian(*px(x0 + (COLS - 3) * 8, y0 + f * 16 + 3)):
            return f
    return -1


def tecla(m, nombre, espera=0.5):
    m.tecla(nombre)
    time.sleep(espera)


def escribir(m, texto):
    m.digitar(texto, 0.06)
    time.sleep(0.4)


def clic_celda(m, t, col, fila):
    m.clique(t["cx"] + 4 + col * 8 + 4, t["cy"] + 4 + fila * 16 + 8)
    time.sleep(0.6)


def main():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--dados-mb", "16"])
    with Maquina("h368", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(2)
        t = h263.janela(m.serial(), "Terminal")

        out = m.comando("echo [$(htop | cat)]")
        verificar("necesita un terminal" in out, "htop sin terminal (tubería) avisa y sale")
        out = m.comando("htop --help")
        verificar("htop [-d DECIMAS]" in out and "F10" in out, "htop --help")

        m.comando("/testes/girar &")
        m.esperar_serial("girar: comecando", 20)
        pid_g = int(re.findall(r"girar: comecando \(pid (\d+)\)", m.serial())[-1])

        m.comando("/testes/hilosdormir &")
        m.esperar_serial("hilosdormir: comecando", 20)
        pid_h = int(re.findall(r"hilosdormir: comecando \(pid (\d+)", m.serial())[-1])

        m.digitar("htop", 0.04)
        m.tecla("ret")
        tela, px, y0, x0 = esperar(m, t, r"girar")
        texto = "\n".join(tela)
        # ---- cabecera
        verificar(re.search(r"^\s*0\[\|+.*\d+\.\d%\]", tela[0]), f"barra de CPU 0: {tela[0]!r}")
        verificar(hay_verde(px, y0, x0), "la barra de CPU es verde")
        verificar(re.search(r"Mem\[.*\d[KMG]/\d+(\.\d+)?[MG]\]", texto), "barra de memoria con usado/total")
        verificar(re.search(r"Swp\[", texto), "barra de swap")
        verificar(re.search(r"Tareas: \d+, \d+ hilos; \d+ en ejecución", texto), "Tareas, hilos y en ejecución")
        verificar(re.search(r"Tiempo encendido: \d+:\d\d:\d\d", texto), "tiempo encendido")
        # ---- lista
        cab = [i for i, l in enumerate(tela) if "PID" in l and "ORDEN" in l]
        verificar(cab, "fila de títulos")
        c = cab[0]
        for col in ("PID", "USUARIO", "PRI", "NI", "RES", "S", "CPU%", "MEM%", "TIEMPO+", "ORDEN"):
            verificar(col in tela[c], f"columna {col}")
        verificar("CPU%v" in tela[c], f"ordenado por CPU, de mayor a menor: {tela[c]!r}")
        verificar(re.search(rf"\b{pid_g}\s+sagui\s+20\s+0\s+\S+\s+R\s+\d+\.\d", tela[c + 1]) and "girar" in tela[c + 1],
                  f"el proceso que gira es el primero: {tela[c + 1]!r}")
        verificar(re.search(r"F1Ayuda\s*F2Config\s*F3Buscar\s*F4Filtro\s*F5Árbol\s*F6Orden\s*F7Nice -\s*F8Nice \+\s*F9Matar\s*F10Salir", tela[LINS - 1]),
                  f"barra de teclas: {tela[LINS - 1]!r}")
        verificar(fila_sel(px, y0, x0) == c + 1, "la primera línea está seleccionada")
        m.captura("H368-1-lista")

        # ---- ordenar
        m.tecla("shift-m")
        time.sleep(0.6)
        tela, px, y0, x0 = ver(m, t)
        verificar("MEM%v" in tela[c], f"M ordena por memoria: {tela[c]!r}")
        verificar("terminal" in tela[c + 1], f"el que más memoria usa es el Terminal: {tela[c + 1]!r}")
        m.tecla("shift-i")
        time.sleep(0.6)
        tela, px, y0, x0 = ver(m, t)
        verificar("MEM%^" in tela[c], f"I invierte el orden: {tela[c]!r}")
        m.tecla("shift-i")
        m.tecla("shift-p")
        time.sleep(0.6)
        tela, px, y0, x0 = ver(m, t)
        verificar("CPU%v" in tela[c] and "girar" in tela[c + 1], "P vuelve a ordenar por CPU")
        # clic en la cabecera: PID
        clic_celda(m, t, 2, c)
        tela, px, y0, x0 = ver(m, t)
        verificar("PID^" in tela[c], f"clic en el título PID ordena por PID: {tela[c]!r}")
        pids = [int(re.match(r"\s*(\d+)", l).group(1)) for l in tela[c + 1:c + 6] if re.match(r"\s*\d+", l)]
        verificar(pids == sorted(pids) and len(pids) >= 4, f"PID ascendente {pids}")
        # clic en una línea: selecciona
        clic_celda(m, t, 30, c + 3)
        tela, px, y0, x0 = ver(m, t)
        verificar(fila_sel(px, y0, x0) == c + 3, "clic en una línea la selecciona")
        m.tecla("shift-p")
        time.sleep(0.5)

        # ---- árbol
        tecla(m, "f5", 0.8)
        tela, px, y0, x0 = ver(m, t)
        verificar(any(re.search(r"[|`]- \w", l) for l in tela), "F5: vista de árbol con ramas")
        m.captura("H368-2-arbol")
        tecla(m, "f5", 0.8)
        tela, px, y0, x0 = ver(m, t)
        verificar(not any(re.search(r"[|`]- \w", l) for l in tela), "F5 otra vez: vuelve la lista plana")

        # ---- buscar
        tecla(m, "f3", 0.4)
        escribir(m, "sagsh")
        tela, px, y0, x0 = ver(m, t)
        verificar("Buscar: sagsh" in tela[LINS - 1], f"el aviso de búsqueda: {tela[LINS - 1]!r}")
        verificar("sagsh" in tela[fila_sel(px, y0, x0)], "al teclear se va seleccionando la coincidencia")
        tecla(m, "ret", 0.6)
        tela, px, y0, x0 = ver(m, t)
        verificar("sagsh" in tela[fila_sel(px, y0, x0)], "Intro la deja seleccionada")
        verificar("Buscar:" not in tela[LINS - 1] and "F10Salir" in tela[LINS - 1], "vuelve la barra de teclas")

        # ---- filtrar
        tecla(m, "f4", 0.4)
        escribir(m, "girar")
        tecla(m, "ret", 0.8)
        tela, px, y0, x0 = ver(m, t)
        cuerpo = "\n".join(tela[c + 1:LINS - 1])
        verificar("girar" in cuerpo and "sagsh" not in cuerpo and "cdrom" not in cuerpo, f"F4: solo queda «girar»:\n{cuerpo.strip()}")
        m.captura("H368-3-filtro")

        # ---- nice
        verificar(re.search(r"\b20\s+0\b", tela[c + 1]), f"nice 0 al principio: {tela[c + 1]!r}")
        tecla(m, "f8", 0.8)
        tela, px, y0, x0 = ver(m, t)
        verificar(re.search(r"\b21\s+1\b", tela[c + 1]), f"F8 sube el nice a 1 (PRI 21): {tela[c + 1]!r}")
        tecla(m, "f8", 0.8)
        tela, px, y0, x0 = ver(m, t)
        verificar(re.search(r"\b22\s+2\b", tela[c + 1]), f"F8 otra vez: nice 2: {tela[c + 1]!r}")
        tecla(m, "f7", 0.8)
        tela, px, y0, x0 = ver(m, t)
        bajo = re.search(r"\b21\s+1\b", tela[c + 1]) or "Nice:" in tela[LINS - 1]
        verificar(bajo, f"F7 baja el nice (o dice por qué no: solo root): {tela[c + 1]!r} / {tela[LINS - 1]!r}")
        m.captura("H368-4-nice")

        # ---- matar
        tecla(m, "f9", 0.8)
        tela, px, y0, x0 = ver(m, t)
        verificar(any("Matar" in l and str(pid_g) in l for l in tela[:-1]) and any("Cancelar" in l for l in tela),
                  "F9 abre la ventana con la opción de matar")
        m.captura("H368-5-matar")
        tecla(m, "ret", 0.5)
        tela, px, y0, x0 = esperar(m, t, r"Terminados: 1", 5)
        verificar(re.search(r"Terminados: 1", tela[LINS - 1]), f"aviso de proceso terminado: {tela[LINS - 1]!r}")
        tela, px, y0, x0 = ver(m, t)
        cuerpo = "\n".join(tela[c + 1:LINS - 1])
        # el sagsh recoge al hijo muerto en el prompt siguiente: hasta entonces queda de zombi (Z)
        fil = [l for l in cuerpo.split("\n") if "girar" in l]
        verificar(not fil or re.search(r"\sZ\s+0\.0", fil[0]), f"girar ya no corre (desaparece o queda zombi): {fil}")
        # Esc quita el filtro
        tecla(m, "esc", 0.8)
        tela, px, y0, x0 = ver(m, t)
        verificar("sagsh" in "\n".join(tela), "Esc quita el filtro: vuelven los demás procesos")

        # ---- hilos (H): un proceso con dos hilos
        tecla(m, "shift-h", 0.8)
        tela, px, y0, x0 = esperar(m, t, r"hilosdormir", 6)
        cuerpo = [l for l in tela if "hilosdormir" in l]
        verificar(len(cuerpo) >= 3, f"H muestra el proceso y sus dos hilos: {cuerpo}")
        verificar(re.search(rf"\b{pid_h}\b", cuerpo[0]), "el primero es el proceso")
        tecla(m, "shift-h", 0.8)
        tela, px, y0, x0 = ver(m, t)
        verificar(len([l for l in tela if "hilosdormir" in l]) == 1, "H otra vez: solo el proceso")

        # ---- ratón en la barra de teclas: F1 y F2
        clic_celda(m, t, 2, LINS - 1)
        tela, px, y0, x0 = ver(m, t)
        verificar(any("Ayuda de htop" in l for l in tela), "clic en F1: ayuda")
        m.captura("H368-6-ayuda")
        tecla(m, "esc", 0.8)
        tecla(m, "f2", 0.8)
        tela, px, y0, x0 = ver(m, t)
        verificar(any("Configuración" in l for l in tela) and any("Intervalo:" in l for l in tela), "F2: configuración")
        m.captura("H368-7-config")
        tecla(m, "esc", 0.8)
        tela, px, y0, x0 = ver(m, t)
        verificar(not any("Intervalo:" in l and "[" in l for l in tela[2:]), "Esc cierra la configuración")

        # ---- salir
        tecla(m, "f10", 1.5)
        tela, px, y0, x0 = ver(m, t)
        verificar(any(re.search(r"sagui@saguios:/\$", l) for l in tela) and not any("Tareas:" in l for l in tela),
                  "F10 sale y vuelve la pantalla del shell")
        out = m.comando("cat /home/.config/htoprc")
        verificar("orden=" in out and "intervalo=15" in out, f"se guardó ~/.config/htoprc: {out.strip()[-120:]}")
        out = m.comando("ps")
        verificar("girar" not in out and " htop" not in out, "girar y htop ya no están en ps")
    print("h368: todo bien")


def hay_verde(px, y0, x0):
    for x in range(x0 + 30, x0 + 200):
        for y in range(y0 + 2, y0 + 14):
            if verde(*px(x, y)):
                return True
    return False


if __name__ == "__main__":
    main()
