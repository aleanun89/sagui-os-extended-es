#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H47 - Cipó, el navegador (D047).

Criterio: Cipó abre una página HTML servida por el anfitrión (que, desde
dentro de QEMU, está en 10.0.2.2), muestra el título, sigue un enlace
relativo con un clic, vuelve con el botón Atrás, y abre una página enviada
en trozos (chunked) por HTTP/1.1.

Basado en el test del navegador del PR #3 del repositorio original (M4R4G0N).
"""
import functools
import http.server
import os
import re
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA  # noqa: E402
import h114_cipo_descargas as h114  # noqa: E402  (decodificador de PNG das capturas)

PAGINA_A = """<!DOCTYPE html><html><head><meta charset="utf-8"><title>Página A</title>
<style>body { color: red }</style><script>alert("no aparece")</script></head>
<body><h1>¡Hola, sagui!</h1><p><a href="carpeta/../b.html">segunda página</a> y más texto &amp; acción&eacute;.</p>
<ul><li>uno</li><li>dos</li></ul></body></html>"""
PAGINA_B = "<html><head><title>Página B</title></head><body><p>Llegó &#233; aquí.</p></body></html>"
PAGINA_C = "<html><head><title>Página en trozos</title></head><body><p>" + "palabra " * 300 + "fin.</p></body></html>"


class Tratador(http.server.SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def do_GET(self):
        if self.path == "/trozos.html":
            dados = PAGINA_C.encode()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Transfer-Encoding", "chunked")
            self.send_header("Connection", "close")
            self.end_headers()
            for i in range(0, len(dados), 700):
                t = dados[i:i + 700]
                self.wfile.write(b"%x\r\n" % len(t) + t + b"\r\n")
            self.wfile.write(b"0\r\n\r\n")
            self.close_connection = True
            return
        self.close_connection = True
        super().do_GET()


def main():
    pasta = os.path.join(SAIDA, "h45-www")
    os.makedirs(pasta, exist_ok=True)
    for nome, html in (("index.html", PAGINA_A), ("b.html", PAGINA_B)):
        with open(os.path.join(pasta, nome), "w", encoding="utf-8") as f:
            f.write(html)
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(Tratador, directory=pasta))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    porta = srv.server_address[1]

    img = imagem_de_teste(f"/apps/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    with Maquina("h47", "ide", imagem=img, extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial("cipo: pronto", 60)
        s = m.serial()
        jan = re.search(r'janela (\d+) criada por \d+: "Cip.*?" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
        jx, jy = int(jan.group(4)), int(jan.group(5))
        m.esperar_serial('pronto "P', 30)
        s = m.serial()
        verificar(re.search(r'cipo: pronto "P.gina A" \(\d+ linhas, 1 links\)', s), "la página A se abre, con el título y un enlace")
        captura = m.captura("H47-cipo-pagina")
        # El enlace se busca por su color (es el unico texto azul de la pagina):
        # con el motor nuevo la maqueta es distinta y las coordenadas fijas ya
        # no valen.
        imagem_png = h114.png_pixels(captura)
        caixa = h114.caixa_azul(imagem_png, (jx + 4, jy + 24 + 40 + 4, 780, 260))
        verificar(caixa is not None, "el enlace se ve en azul en la pagina")
        m.clique((caixa[0] + caixa[2]) // 2, (caixa[1] + caixa[3]) // 2)
        m.esperar_serial("gina B", 20)
        verificar("abrindo http://10.0.2.2:%d/b.html" % porta in m.serial(), "el clic sigue el enlace relativo (carpeta/../b.html -> /b.html)")
        m.clique(jx + 2 + 18, jy + 24 + 45)   # Atrás (D373: la barra de herramientas va bajo las pestañas)
        time.sleep(2)
        verificar(len(re.findall(r'cipo: pronto "P.gina A"', m.serial())) == 2, "Atrás vuelve a abrir la página A")
        # Dirección nueva escrita en la barra: una página chunked.
        m.clique(jx + 2 + 400, jy + 24 + 45)   # la dirección
        for _ in range(60):
            m.tecla("backspace", 0.02)
        m.digitar(f"10.0.2.2:{porta}/trozos.html\n", 0.04)
        m.esperar_serial('gina en trozos" (', 30)
        verificar(True, "abre una página HTTP/1.1 enviada en trozos (chunked)")
        verificar("cipo: erro" not in m.serial(), "sin errores de carga")
    srv.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
