#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H231 - Terminal con pestañas, preferencias y lavado de cara (D231, v7.4.2).

Criterio:
  unidad (PC): tests/terminal_unidad.c con terminal_logica.h: secuencias de escape, SGR y colores,
      temas, terminal.conf (leer/escribir), búsqueda, enlaces y títulos;
  QEMU:
  - con una sola pestaña no hay barra y el aspecto es el de siempre (fondo 0x12161E, 94 x 28);
  - Ctrl+Mayús+T abre una pestaña (otro sagsh); lo que se escribe en cada una sale en la suya
    (se comprueba en pantalla, con $((..)) para no confundir la orden con su salida);
  - Ctrl+1, Ctrl+RePág y Ctrl+AvPág cambian de pestaña; el título es la carpeta o la orden en marcha;
  - cerrar con una orden en marcha avisa (Esc la mantiene, Intro la cierra); Ctrl+Mayús+W cierra;
  - copiar la selección (arrastrando) y pegarla; buscar en el historial (Ctrl+Mayús+F);
  - Preferencias (Ctrl+,): elegir el tema «Verde fósforo» y Aceptar guarda terminal.conf, se aplica al
    instante y un Terminal nuevo arranca ya con ese tema.
Es una prueba escrita sin ejecutar al hacerla (modo solo código): al compilar, mirar primero la unidad."""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel, ler_ppm, reconhecer_texto, RAIZ, SAIDA  # noqa: E402

BORDA, TITULO = 2, 24
DIR = os.path.join(SAIDA, "h231")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "terminal_unidad")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-DTERMINAL_ANFITRION", "-I", os.path.join(RAIZ, "userland", "programas"),
                        "-o", exe, os.path.join(RAIZ, "tests", "terminal_unidad.c")], capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print(r.stdout)
    verificar(r.returncode == 0 and "todo bien" in r.stdout,
              "escapes, SGR, temas, terminal.conf, búsqueda, enlaces y títulos")


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]), y=int(g[4]),
                cx=int(g[3]) + BORDA, cy=int(g[4]) + TITULO)


def esperar(m, padrao, tempo=15, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def texto_janela(m, j, x, y, cols, lins):
    """El texto de la grilla del Terminal, leído de la pantalla (celdas de 8x16)."""
    ppm = os.path.join(SAIDA, "h231-tela.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.2)
    larg, _, px = ler_ppm(ppm)
    x0, y0 = j["cx"] + x, j["cy"] + y
    recorte = b"".join(px[((y0 + k) * larg + x0) * 3:((y0 + k) * larg + x0 + cols * 8) * 3] for k in range(lins * 16))
    return reconhecer_texto(cols * 8, lins * 16, recorte)


def prompts(m):
    return m.serial().count("sagui:")


def nueva_pestana(m, n_esperado):
    """Ctrl+Mayús+T y espera a que el sagsh nuevo escriba su indicador."""
    marca = len(m.serial())
    antes = prompts(m)
    m.tecla("ctrl-shift-t", 0.3)
    esperar(m, rf"terminal: pestana {n_esperado} abierta", desde=marca)
    fim = time.time() + 20
    while prompts(m) <= antes:
        assert time.time() < fim, "el sagsh de la pestaña nueva no escribió su indicador"
        time.sleep(0.2)
    time.sleep(0.8)   # que la grilla se dibuje


def parte_qemu():
    with Maquina("h231", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        s = m.serial()
        t = janela(s, "Terminal")
        g = re.findall(r"terminal: tema (\w+), fuente (\S+) (\d+) px, (\d+) columnas x (\d+) lineas, historial (\d+)", s)
        verificar(g and g[-1] == ("oscuro", "interna", "16", "94", "28", "500"),
                  f"sin terminal.conf: tema oscuro, fuente interna, 94 x 28 y 500 líneas de historial ({g[-1] if g else '?'})")
        png = m.captura("H231-1-una-pestana")
        verificar(cor_do_pixel(png, t["cx"] + 2, t["cy"] + 2) == (0x12, 0x16, 0x1E),
                  "con una sola pestaña no hay barra: el fondo oscuro llega a la esquina (aspecto de siempre)")

        # ---- pestaña 1: una orden
        out = m.comando("echo marca-$((20+22))")
        verificar("marca-42" in out, "la pestaña 1 ejecuta órdenes")

        # ---- Ctrl+Mayús+T: pestaña 2
        nueva_pestana(m, 2)
        verificar(re.search(r"terminal: pestana 2 de 2 \(shell \d+\)", m.serial()), "la pestaña nueva queda activa")
        s = m.serial()
        verificar(len(re.findall(r"executar /bin/sagsh", s)) >= 2, "cada pestaña lanza su propio sagsh")
        out = m.comando("echo otra-$((50+7))")
        verificar("otra-57" in out, "la pestaña 2 ejecuta órdenes")
        time.sleep(0.5)
        png = m.captura("H231-2-dos-pestanas")
        verificar(cor_do_pixel(png, t["cx"] + 2, t["cy"] + 2) != (0x12, 0x16, 0x1E),
                  "con dos pestañas aparece la barra de pestañas (más clara que el fondo)")
        tela = texto_janela(m, t, 4, 4 + 26, 94, 24)
        verificar(any("otra-57" in l and "echo" not in l for l in tela) and not any("marca-42" in l and "echo" not in l for l in tela),
                  "en pantalla solo se ve la salida de la pestaña 2")

        # ---- cambiar de pestaña
        marca = len(m.serial())
        m.tecla("ctrl-1", 0.5)
        esperar(m, r"terminal: pestana 1 de 2", desde=marca)
        time.sleep(0.6)
        tela = texto_janela(m, t, 4, 4 + 26, 94, 24)
        verificar(any("marca-42" in l and "echo" not in l for l in tela) and not any("otra-57" in l and "echo" not in l for l in tela),
                  "Ctrl+1 muestra la pestaña 1 con su propia salida")
        marca = len(m.serial())
        m.tecla("ctrl-pgdn", 0.5)
        esperar(m, r"terminal: pestana 2 de 2", desde=marca)
        marca = len(m.serial())
        m.tecla("ctrl-pgup", 0.5)
        esperar(m, r"terminal: pestana 1 de 2", desde=marca)
        verificar(True, "Ctrl+AvPág y Ctrl+RePág cambian de pestaña")
        # clic en la pestaña 2 (la barra mide 26 px; cada pestaña, hasta 170)
        marca = len(m.serial())
        m.clique(t["cx"] + 2 + 170 + 60, t["cy"] + 12)
        esperar(m, r"terminal: pestana 2 de 2", desde=marca)
        verificar(True, "el clic en una pestaña la activa")
        m.tecla("ctrl-1", 0.5)

        # ---- títulos
        m.comando("cd /bin")
        esperar(m, r"terminal: titulo de la pestana 1: bin", tempo=10)
        verificar(True, "el título de la pestaña es la carpeta (bin)")
        m.comando("cd /")

        # ---- orden en marcha: título y aviso al cerrar
        m.tecla("ctrl-2", 0.5)
        time.sleep(0.5)
        m.digitar("contador 60\n", 0.04)
        esperar(m, r"terminal: titulo de la pestana 2: contador", tempo=10)
        verificar(True, "con una orden en marcha el título es su nombre")
        marca = len(m.serial())
        m.tecla("ctrl-shift-w", 0.5)
        esperar(m, r'criada por \d+: "Cerrar pestaña"', tempo=10, desde=marca)
        verificar(True, "cerrar con una orden en marcha pregunta")
        m.tecla("esc", 0.6)
        time.sleep(0.5)
        verificar("terminal: pestana cerrada" not in m.serial()[marca:], "Esc mantiene la pestaña")
        m.tecla("ctrl-shift-w", 0.5)
        time.sleep(0.8)
        m.tecla("ret", 1.0)
        esperar(m, r"terminal: pestana cerrada, quedan 1", desde=marca)
        verificar(True, "Intro confirma: la pestaña se cierra y queda una")
        time.sleep(0.8)
        png = m.captura("H231-3-vuelve-a-una")
        verificar(cor_do_pixel(png, t["cx"] + 2, t["cy"] + 2) == (0x12, 0x16, 0x1E), "con una pestaña vuelve a desaparecer la barra")

        # ---- copiar y pegar con el ratón
        tela = texto_janela(m, t, 4, 4, 94, 28)
        fila = [i for i, l in enumerate(tela) if "marca-42" in l and "echo" not in l]
        verificar(fila, "la salida marca-42 sigue en la pestaña 1")
        f = fila[-1]
        col = tela[f].index("marca-42")
        y = t["cy"] + 4 + f * 16 + 8
        m.arrastar(t["cx"] + 4 + col * 8 + 2, y, t["cx"] + 4 + (col + 7) * 8 + 4, y)
        marca = len(m.serial())
        m.tecla("ctrl-shift-c", 0.5)
        g = esperar(m, r"terminal: copiado (\d+) bytes", desde=marca)
        verificar(g == "8", f"Ctrl+Mayús+C copia la selección ({g} bytes, marca-42)")
        marca = len(m.serial())
        m.digitar("echo ", 0.05)
        m.tecla("ctrl-shift-v", 0.5)
        esperar(m, r"terminal: pegado 8 bytes", desde=marca)
        m.tecla("ret", 0.5)
        time.sleep(0.8)
        verificar(m.serial()[marca:].count("marca-42") >= 2, "Ctrl+Mayús+V pega el texto copiado en el shell")

        # ---- buscar en el historial
        m.comando("cat /ejemplos/ejemplo.md")
        m.comando("echo fin")
        marca = len(m.serial())
        m.tecla("ctrl-shift-f", 0.5)
        m.digitar("marca-", 0.05)
        g = esperar(m, r"terminal: busqueda 'marca-' en la linea (\d+)", desde=marca)
        verificar(True, f"Ctrl+Mayús+F busca en el historial y salta a la línea {g}")
        time.sleep(0.5)
        m.captura("H231-4-buscar")
        m.tecla("esc", 0.4)

        # ---- Preferencias
        marca = len(m.serial())
        m.tecla("ctrl-comma", 0.6)
        esperar(m, r"terminal: preferencias abiertas", desde=marca)
        time.sleep(0.8)
        p = janela(m.serial(), "Terminal - Preferencias")
        m.captura("H231-5-preferencias")
        m.clique(p["cx"] + 16 + 40, p["cy"] + 32 + 2 + 2 * 20 + 10)    # «Verde fósforo»
        time.sleep(0.5)
        m.captura("H231-6-preferencias-verde")
        marca = len(m.serial())
        m.clique(p["cx"] + 640 - 106 + 45, p["cy"] + 500 - 40 + 13)    # Aceptar
        g = esperar(m, r"terminal: preferencias guardadas en (\S+) \(tema (\w+)", desde=marca)
        verificar(g[1] == "verde", f"Aceptar guarda las preferencias en {g[0]} (tema {g[1]})")
        time.sleep(1.0)
        png = m.captura("H231-7-tema-verde")
        verificar(cor_do_pixel(png, t["cx"] + 2, t["cy"] + 2) == (0x05, 0x0D, 0x05), "el tema nuevo se aplica al instante")
        # sin partición /home en la imagen de prueba cae a /tmp: se lee donde dijo guardarlo
        out = m.comando("cat " + g[0])
        verificar("tema=verde" in out and "historial=500" in out, "terminal.conf tiene el tema y el resto de opciones")

        # ---- un Terminal nuevo arranca con el tema guardado
        marca = len(m.serial())
        m.comando("terminal &")
        g = esperar(m, r"terminal: tema (\w+), fuente", desde=marca, tempo=20)
        verificar(g == "verde", "un Terminal nuevo lee terminal.conf y arranca con el tema verde")
        time.sleep(1.0)
        t2 = janela(m.serial(), "Terminal")
        png = m.captura("H231-8-terminal-nuevo")
        verificar(cor_do_pixel(png, t2["cx"] + 2, t2["cy"] + 2) == (0x05, 0x0D, 0x05), "su fondo es el del tema verde")
    return 0


def main():
    unidad()
    return parte_qemu()


if __name__ == "__main__":
    sys.exit(main())
