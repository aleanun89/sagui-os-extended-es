#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H82 - Cipó redimensionable, navegación, búsqueda e imágenes (D110, mejora 063).

Criterio (servidor HTTP local en el anfitrión, sin Internet):
 (a) la página (texto largo, una imagen PNG y enlaces) se abre y la imagen se descarga y se muestra;
 (b) maximizar vuelve a maquetar el texto al ancho nuevo (menos líneas) y restaurar lo devuelve;
 (c) la rueda desplaza; AvPág/Inicio/Fin y arrastrar la barra de desplazamiento también;
 (d) seguir un enlace, Alt+Izquierda (atrás), Alt+Derecha (adelante), F5 (recargar), Ctrl+L (dirección);
 (e) Ctrl+F busca, resalta y F3 salta a la siguiente coincidencia;
 (f) Tab pone el foco en un enlace e Intro lo abre.
"""
import functools
import http.server
import os
import re
import struct
import sys
import threading
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA  # noqa: E402

BORDA, TITULO = 2, 24


def png(w, h):
    """PNG RGB de w x h con un degradado (generado aquí, sin material de terceros)."""
    filas = b"".join(b"\x00" + bytes(c for x in range(w) for c in (x * 255 // w, y * 255 // h, 160)) for y in range(h))

    def trozo(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
            trozo(b"IDAT", zlib.compress(filas)) + trozo(b"IEND", b""))


PARRAFO = ("El sagui salta de liana en liana y el navegador de enlace en enlace; "
           "cada palabra se acomoda al ancho de la ventana sin perder el hilo de la lectura. ")
INDEX = ("<html><head><title>Pagina larga</title></head><body><h1>Cipo en una ventana grande</h1>"
         "<p><a href=\"b.html\">pagina b</a> y <a href=\"c.html\">pagina c</a>.</p>"
         "<p><img src=\"foto.png\" alt=\"degradado\"></p>"
         + "".join(f"<p>{i}. {PARRAFO * 5}</p>" for i in range(1, 25))
         + "<p>Aqui esta el tesoro escondido.</p></body></html>")
PAGINA_B = "<html><head><title>Pagina B</title></head><body><p>Bee</p></body></html>"
PAGINA_C = "<html><head><title>Pagina C</title></head><body><p>Cee</p></body></html>"


class Tratador(http.server.SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def do_GET(self):
        self.close_connection = True
        if self.path == "/foto.png":
            d = png(400, 200)
            self.send_response(200)
            self.send_header("Content-Type", "image/png")
            self.send_header("Content-Length", str(len(d)))
            self.send_header("Connection", "close")
            self.end_headers()
            self.wfile.write(d)
            return
        super().do_GET()


def ultimo(m, padrao, desde=0):
    g = re.findall(padrao, m.serial()[desde:])
    return g[-1] if g else None


def esperar(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = ultimo(m, padrao, desde)
        if g:
            return g
        time.sleep(0.2)
    raise AssertionError(f"tempo esgotado esperando {padrao!r}")


def roda(m, entalhes):
    passo = -1 if entalhes > 0 else 1
    for _ in range(abs(entalhes)):
        m.monitor(f"mouse_move 0 0 {passo}")
        time.sleep(0.15)
    time.sleep(0.4)


def main():
    pasta = os.path.join(SAIDA, "h82-www")
    os.makedirs(pasta, exist_ok=True)
    for nome, html in (("index.html", INDEX), ("b.html", PAGINA_B), ("c.html", PAGINA_C)):
        with open(os.path.join(pasta, nome), "w", encoding="utf-8") as f:
            f.write(html)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(Tratador, directory=pasta))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    porta = srv.server_address[1]

    img = imagem_de_teste(f"/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    with Maquina("h82", "ide", imagem=img, extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial("cipo: pronto", 60)
        g = re.search(r'janela (\d+) criada por \d+: "Cip.*?" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())
        jid, jl, ja, jx, jy = (int(g.group(i)) for i in range(1, 6))
        # Com o motor novo a imagem e um recurso da pagina: o registo e o
        # dos recursos por ordem (o motor antigo registava cada imagem).
        gi = esperar(m, r'cipo: recurso en trozo nos=\d+ cmds=\d+ imgs=(\d+)', 30)
        verificar(int(gi) >= 1, "(a) a imagem PNG se descarga como recurso da pagina")
        s = m.serial()
        time.sleep(1)
        m.captura("H82-1-ventana-normal")

        # (b) maximizar: el texto se vuelve a maquetar al ancho nuevo
        bx, by = jx + jl + 2 * BORDA - BORDA - 20 * 2 + 8, jy + 12
        m.clique(bx, by)
        esperar(m, rf"janela {jid} maximizada")
        gv = esperar(m, r'cipo: ventana (\d+)x(\d+), texto de (\d+) px de ancho, (\d+) lineas')
        verificar(int(gv[0]) > jl and int(gv[2]) > 800 - 40, f"(b) maximizada: ventana {gv[0]}x{gv[1]}, texto de {gv[2]} px")
        time.sleep(1)
        m.captura("H82-2-maximizada")
        n_antes = len(re.findall(r'cipo: ventana', m.serial()))
        # restaurar con el mismo botón (la ventana maximizada cambia de sitio)
        m.clique(int(gv[0]) + 2 * BORDA - BORDA - 20 * 2 + 8, 24 + 12)   # maximizada: marco en (0, 24)
        for _ in range(40):
            if len(re.findall(r'cipo: ventana', m.serial())) > n_antes:
                break
            time.sleep(0.25)
        gr = ultimo(m, r'cipo: ventana (\d+)x(\d+), texto de (\d+) px de ancho, (\d+) lineas')
        verificar(int(gr[0]) < int(gv[0]) and int(gr[2]) < int(gv[2]), f"(b) restaurada: ventana {gr[0]}x{gr[1]}, texto de {gr[2]} px")
        verificar(int(gv[3]) < int(gr[3]), f"(b) más ancho = menos líneas (maximizada {gv[3]}, restaurada {gr[3]})")
        time.sleep(1)
        m.captura("H82-3-restaurada")

        # (c) rueda, teclas y barra
        px, py = jx + 2 + 400, jy + 24 + 200
        m.mouse_para(px, py)
        roda(m, 3)
        gl = esperar(m, r'cipo: rueda 1, linea (\d+) de')
        verificar(int(gl) == 9, "(c) la rueda baja 3 líneas por muesca")
        m.tecla("pgdn")
        time.sleep(0.5)
        m.tecla("end")
        time.sleep(0.5)
        m.captura("H82-4-final")
        m.tecla("home")
        time.sleep(0.5)
        # arrastrar el pulgar de la barra de desplazamiento (a la derecha de la ventana)
        barra_x = jx + 2 + 860 - 7
        n0 = len(m.serial())
        # D172: la página empieza en y=66 (40 de la barra de herramientas + 26 de la de pestañas, D148):
        # el pulgar, en la posición Inicio, arranca ahí; antes se agarraba en la barra de pestañas.
        m.arrastar(barra_x, jy + 24 + 66 + 10, barra_x, jy + 24 + 66 + 210)
        roda(m, 1)
        gl2 = ultimo(m, r'cipo: rueda 1, linea (\d+) de')
        verificar(int(gl2) > 20, f"(c) arrastrar la barra desplaza la página (línea {gl2} tras 1 muesca)")
        m.tecla("home")
        time.sleep(0.4)

        # (e) buscar
        m.tecla("ctrl-f")
        time.sleep(0.5)
        m.digitar("tesoro", 0.1)
        esperar(m, r'cipo: busca "tesoro": 1 de 1')
        verificar(True, "(e) Ctrl+F encuentra «tesoro» (1 coincidencia) y salta a ella")
        time.sleep(0.5)
        m.captura("H82-5-busqueda")
        m.tecla("f3")
        time.sleep(0.5)
        for _ in range(6):
            m.tecla("backspace", 0.05)
        m.digitar("sagui", 0.1)
        ord1, tot = esperar(m, r'cipo: busca "sagui": (\d+) de (\d+)')
        time.sleep(0.5)
        ord1, tot = ultimo(m, r'cipo: busca "sagui": (\d+) de (\d+)')
        m.tecla("f3")
        esperar(m, rf'cipo: busca "sagui": {int(ord1) % int(tot) + 1} de {tot}')
        verificar(True, "(e) F3 salta a la siguiente coincidencia")
        m.tecla("esc")
        time.sleep(0.4)

        # (d) navegación
        m.tecla("home")
        time.sleep(0.3)
        n0 = len(m.serial())
        # (f) Tab -> primer enlace; Intro lo abre
        m.tecla("tab")
        esperar(m, r'cipo: foco en el enlace 0', desde=n0)
        m.tecla("ret")
        esperar(m, r'cipo: pronto "Pagina B"', 20, n0)
        verificar(f"abrindo http://10.0.2.2:{porta}/b.html" in m.serial()[n0:], "(f) Tab enfoca el enlace e Intro lo abre")
        n1 = len(m.serial())
        m.tecla("alt-left")
        esperar(m, r'cipo: pronto "Pagina larga"', 20, n1)
        verificar(True, "(d) Alt+Izquierda vuelve atrás")
        n2 = len(m.serial())
        m.tecla("alt-right")
        esperar(m, r'cipo: pronto "Pagina B"', 20, n2)
        verificar(True, "(d) Alt+Derecha avanza")
        n3 = len(m.serial())
        m.tecla("f5")
        esperar(m, r'cipo: recargar', 10, n3)
        esperar(m, r'cipo: pronto "Pagina B"', 20, n3)
        verificar(True, "(d) F5 recarga")
        n4 = len(m.serial())
        m.tecla("ctrl-l")
        time.sleep(0.4)
        m.digitar(f"10.0.2.2:{porta}/c.html\n", 0.05)
        esperar(m, r'cipo: pronto "Pagina C"', 20, n4)
        verificar(True, "(d) Ctrl+L lleva a la barra de dirección (la escritura reemplaza la dirección)")
        m.tecla("alt-home")
        esperar(m, r'cipo: pronto "Inicio"', 20, n4)
        verificar(True, "(d) Alt+Inicio abre sobre:inicio")
        m.captura("H82-6-inicio")
        verificar("cipo: erro" not in m.serial(), "sin errores de carga")
    srv.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
