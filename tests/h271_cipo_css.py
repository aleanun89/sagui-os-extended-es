#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H271 - Cipó: compatibilidad con CSS y HTML5 (D271).

Criterio: el Cipó abre, una tras otra, las páginas de tests/web/ (una fila
por función de CSS, de HTML5 o del DOM; ver tests/web/generar.py) servidas
por HTTP, y en la captura de cada una las casillas tienen que estar verdes:
la función que prueba cada fila es justo la que quita el rojo. Las sondas de
cada casilla van en el propio HTML (`data-p`) y las mira
tests/web/comprobar.py.

Lo que el motor todavía no hace está en tests/web/pendientes.txt (una fila
por línea): esas filas pueden fallar; las demás no. Si una pendiente ya
funciona, la prueba lo dice para que se quite de la lista.

El resultado fila a fila queda en build/.../h271-resultado.txt (con él se
hace la tabla de docs/cipo-compatibilidad.md).
"""
import http.server
import os
import re
import sys
import threading
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
sys.path.insert(0, os.path.join(AQUI, "web"))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA, RAIZ  # noqa: E402
import comprobar  # noqa: E402

WEB = os.path.join(AQUI, "web")
PAGINAS = sorted(f[:-5] for f in os.listdir(WEB) if f.endswith(".html"))
GUARDAR = {"01", "03", "07", "09", "12", "15", "17", "19"}   # las capturas que van a docs/capturas/
TIPOS = {".html": "text/html; charset=utf-8", ".css": "text/css", ".png": "image/png", ".txt": "text/plain",
         ".json": "application/json", ".ttf": "font/ttf"}


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        ruta = self.path.split("?")[0].lstrip("/")
        if re.fullmatch(r"\d+", ruta):          # /3 = la tercera página (dirección corta de teclear)
            n = int(ruta)
            ruta = PAGINAS[n - 1] + ".html" if 1 <= n <= len(PAGINAS) else "no-hay"
        base = os.path.join(RAIZ, "rootfs") if ruta.startswith("fontes/") else WEB
        camino = os.path.normpath(os.path.join(base, ruta))
        corpo, estado = b"no hay", 404
        if camino.startswith(base + os.sep) and os.path.isfile(camino):
            with open(camino, "rb") as f:
                corpo, estado = f.read(), 200
        self.send_response(estado)
        self.send_header("Content-Type", TIPOS.get(os.path.splitext(camino)[1], "application/octet-stream"))
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(corpo)
        self.close_connection = True


def pendientes():
    """{"NN-nombre": {fila, ...}} de tests/web/pendientes.txt."""
    p = {}
    camino = os.path.join(WEB, "pendientes.txt")
    if os.path.exists(camino):
        with open(camino, encoding="utf-8") as f:
            for linea in f:
                m = re.match(r"\s*([\w-]+):(\d+)", linea)
                if m:
                    p.setdefault(m.group(1), set()).add(int(m.group(2)))
    return p


def esperar_pagina(m, titulo, desde, tempo=120):
    """Espera a que el Cipó diga que la página está lista (después de `desde` bytes de serie)."""
    fim = time.time() + tempo
    marca = 'cipo: pronto "%s' % titulo
    while time.time() < fim:
        s = m.serial_bruto()
        i = s.find(marca, desde)
        if i >= 0:
            return i
        time.sleep(0.2)
    raise AssertionError("el Cipó no terminó de abrir %r" % titulo)


def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    base = "http://10.0.2.2:%d/" % porta
    pend = pendientes()
    resultado = []      # (página, fila, rótulo, bien)
    try:
        imagem = imagem_de_teste("/bin/cipo %s1" % base, ["--vesa"])
        with Maquina("h271", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            desde = 0
            for n, pag in enumerate(PAGINAS, 1):
                with open(os.path.join(WEB, pag + ".html"), encoding="utf-8") as f:
                    html = f.read()
                titulo = re.search(r"<title>(.*?)</title>", html).group(1)
                if n > 1:
                    desde = len(m.serial_bruto())
                    m.tecla("ctrl-l", 0.3)
                    m.digitar("%s%d" % (base, n), 0.05)
                    m.tecla("ret", 0.2)
                if n == 1:
                    m.mouse_para(1000, 740)       # el puntero, fuera de la página
                esperar_pagina(m, titulo[:3], desde)
                time.sleep(2.5 if "js" in pag else 1.0)     # los temporizadores y el fetch de la página
                captura = m.captura("H271-" + pag, guardar=pag[:2] in GUARDAR)
                pixel, larg, alt = comprobar.leer(captura)
                o = comprobar.origen(pixel, larg, alt)
                if o is None:
                    print("   %s: la página no se ve (sin marca de origen)" % pag)
                    filas = [(r, False) for r, _ in comprobar.filas_de(html)]
                else:
                    filas = comprobar.comprobar(html, pixel, o[0], o[1])
                for i, (rotulo, bien) in enumerate(filas, 1):
                    resultado.append((pag, i, rotulo, bien))
                print("   %s: %d de %d" % (pag, sum(1 for _, b in filas if b), len(filas)))
    finally:
        servidor.shutdown()

    os.makedirs(SAIDA, exist_ok=True)
    with open(os.path.join(SAIDA, "h271-resultado.txt"), "w", encoding="utf-8") as f:
        for pag, i, rotulo, bien in resultado:
            f.write("%s:%d\t%s\t%s\n" % (pag, i, "ok" if bien else "FALLA", rotulo))

    verificar(len(resultado) > 250, "se comprobaron las %d filas de las %d páginas" % (len(resultado), len(PAGINAS)))
    for pag in PAGINAS:
        filas = [r for r in resultado if r[0] == pag]
        malas = [r for r in filas if not r[3] and r[1] not in pend.get(pag, ())]
        for _, i, rotulo, _ in malas:
            print("   FALLA %s:%d %s" % (pag, i, rotulo))
        for _, i, rotulo, bien in filas:
            if bien and i in pend.get(pag, ()):
                print("   nota: %s:%d (%s) ya funciona; quitarla de pendientes.txt" % (pag, i, rotulo))
            elif not bien and i in pend.get(pag, ()):
                print("   pendiente %s:%d %s" % (pag, i, rotulo))
        bien = sum(1 for r in filas if r[3])
        verificar(not malas, "%s: %d de %d filas en verde (pendientes: %d)" % (pag, bien, len(filas), len(filas) - bien))
    total = sum(1 for r in resultado if r[3])
    print("   total: %d de %d filas" % (total, len(resultado)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
