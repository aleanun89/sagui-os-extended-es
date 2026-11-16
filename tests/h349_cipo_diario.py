#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H349 - Cipó: portada de diario deportivo (D347, as.com).

Criterio: el Cipó abre tests/paginas/diario-portada.html (sintética, imita lo que
rompía en as.com) servida por HTTP y, en la captura:
  - la cabecera tiene su fondo rojo (un ::before con background-image, imagen
    número 100+ de la página: antes el límite era de 96 imágenes distintas),
  - los iconos <use href="#x"> a un sprite de otro <svg> se pintan (blanco sobre rojo),
  - las imágenes dentro de visibility:hidden no se pintan (banderas azules),
  - los titulares se leen (píxeles oscuros de texto) y no hay cuelgue (la página termina).
"""
import http.server
import os
import re
import struct
import sys
import threading
import time
import zlib

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
sys.path.insert(0, os.path.join(AQUI, "web"))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402
import comprobar  # noqa: E402


def png(r, g, b, n=8):
    """PNG RGB liso de n x n."""
    def trozo(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    fila = b"\x00" + bytes([r, g, b]) * n
    return (b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 2, 0, 0, 0))
            + trozo(b"IDAT", zlib.compress(fila * n)) + trozo(b"IEND", b""))


FONDO, BANDERA, FOTO = png(0xE4, 0x1B, 0x1B), png(0, 0, 255), png(200, 200, 200, 2)
PAGINA = os.path.join(AQUI, "paginas", "diario-portada.html")


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        ruta = self.path.split("?")[0]
        if ruta == "/fondo.png":
            corpo, tipo = FONDO, "image/png"
        elif ruta == "/bandera.png":
            corpo, tipo = BANDERA, "image/png"
        elif ruta.startswith("/i/"):
            corpo, tipo = FOTO, "image/png"
        else:
            with open(PAGINA, "rb") as f:
                corpo, tipo = f.read(), "text/html; charset=utf-8"
        self.send_response(200)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(corpo)
        self.close_connection = True


def cerca(c, d, t=24):
    return c is not None and all(abs(c[i] - d[i]) <= t for i in range(3))


def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    base = "http://10.0.2.2:%d/" % servidor.server_address[1]
    try:
        imagem = imagem_de_teste("/apps/bin/cipo " + base, ["--vesa"])
        with Maquina("h349", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.mouse_para(1000, 740)
            fim = time.time() + 150
            while time.time() < fim and 'cipo: pronto "Diario' not in m.serial_bruto():
                time.sleep(0.3)
            verificar('cipo: pronto "Diario' in m.serial_bruto(), "el Cipó terminó de abrir la portada (sin cuelgue)")
            # espera a que lleguen las ~120 imágenes y el fondo de la cabecera
            fim = time.time() + 90
            while time.time() < fim and "recursos listos" not in m.serial_bruto():
                time.sleep(0.5)
            time.sleep(1.5)
            verificar("main-header" not in m.serial_bruto() and "recursos listos" in m.serial_bruto(),
                      "todos los recursos (más de 96 imágenes) se descargaron")
            pixel, larg, alt = comprobar.leer(m.captura("H349-diario", guardar=False))
            # contenido de la ventana: x 162..1000, y 172..680
            rojo = sum(1 for y in range(176, 222, 3) for x in range(170, 1000, 6) if cerca(pixel(x, y), (0xE4, 0x1B, 0x1B)))
            verificar(rojo > 150, "la cabecera tiene el fondo rojo del ::before (%d muestras)" % rojo)
            blanco = sum(1 for y in range(176, 222) for x in range(170, 230) if cerca(pixel(x, y), (255, 255, 255), 8))
            verificar(blanco > 80, "el icono <use> del sprite se pinta en blanco (%d píxeles)" % blanco)
            azul = sum(1 for y in range(172, 680, 2) for x in range(162, 1005, 2) if cerca(pixel(x, y), (0, 0, 255), 40))
            verificar(azul == 0, "las imágenes de visibility:hidden no se pintan (%d píxeles azules)" % azul)
            oscuro = sum(1 for y in range(235, 420) for x in range(220, 900) if sum(pixel(x, y)) < 200)
            verificar(oscuro > 300, "los titulares se leen (%d píxeles de texto)" % oscuro)
    finally:
        servidor.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
