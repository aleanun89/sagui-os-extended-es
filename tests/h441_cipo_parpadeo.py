#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H441 - Cipó sin parpadeo (D441).

Criterio, con una página grande (texto, 40 imágenes que llegan despacio):
  - durante la carga, ninguna captura muestra la página vacía o a medias una vez que
    el título ya se vio (antes, cada trozo borraba la zona y se mostraba el fondo);
  - los repintados durante la carga bajan: como mucho uno cada ~80 ms (12 por segundo),
    y menos que los trozos recibidos;
  - el desplazamiento (copiar lo pintado y pintar solo la franja nueva) deja la misma
    imagen que un repintado entero: el final de la página por Fin (repintado entero)
    es igual al final alcanzado con muchos AvPág (copias);
  - el desplazamiento no deja la página en blanco en ningún cuadro capturado.
"""
import http.server
import os
import re
import struct
import sys
import threading
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, ler_ppm, SAIDA  # noqa: E402

N_IMG = 40


def png(r, g, b, n=24):
    def trozo(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    fila = b"\x00" + bytes([r, g, b]) * n
    return (b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", n, n, 8, 2, 0, 0, 0))
            + trozo(b"IDAT", zlib.compress(fila * n)) + trozo(b"IEND", b""))


def pagina():
    partes = ["<!doctype html><html><head><title>Prueba parpadeo</title>",
              "<style>body{font-family:sans-serif;color:#102030;background:#fff}"
              "h1{color:#000;font-size:40px;background:#e0f0e0}"
              "p{margin:6px}img{width:120px;height:40px}</style></head><body>",
              "<h1>TITULO FIJO GRANDE</h1>"]
    for i in range(N_IMG):
        partes.append("<p>Parrafo %d con texto de relleno para que la pagina sea larga y se pueda desplazar. "
                      "Mas texto de relleno aqui.</p><p><img src='/i/%d.png' alt='imagen %d'></p>" % (i, i, i))
    partes.append("<p>FIN DE LA PAGINA</p></body></html>")
    return "".join(partes).encode("utf-8")


HTML = pagina()


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def do_GET(self):
        if self.path.startswith("/i/"):
            n = int(re.search(r"(\d+)", self.path).group(1))
            time.sleep(0.08)
            cuerpo, tipo = png((n * 37) & 255, (n * 91) & 255, (n * 53) & 255), "image/png"
        else:
            cuerpo, tipo = HTML, "text/html; charset=utf-8"
        self.send_response(200)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(cuerpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(cuerpo)
        self.close_connection = True


def cuadro(m, n):
    """Captura rápida (PPM en memoria) de la pantalla."""
    ppm = os.path.join(SAIDA, "h441-%d.ppm" % n)
    m.monitor("screendump " + ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.02)
    time.sleep(0.03)
    try:
        return ler_ppm(ppm)
    finally:
        try:
            os.remove(ppm)
        except OSError:
            pass


def oscuros(img, x0, y0, l, a):
    """Cuenta píxeles oscuros (texto) en una región."""
    L, A, px = img
    n = 0
    for y in range(max(0, y0), min(A, y0 + a), 2):
        fila = px[(y * L + max(0, x0)) * 3:(y * L + min(L, x0 + l)) * 3]
        for i in range(0, len(fila), 3):
            if fila[i] < 90 and fila[i + 1] < 90 and fila[i + 2] < 90:
                n += 1
    return n


def region_bytes(img, x0, y0, l, a):
    L, A, px = img
    return b"".join(px[((y * L) + x0) * 3:((y * L) + x0 + l) * 3] for y in range(y0, min(A, y0 + a)))


def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    imagem = imagem_de_teste(f"/apps/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    try:
        with Maquina("h441", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial('cipo: pronto "Prueba parpadeo"', 90)
            s = m.serial().replace("\r", "")
            ja = re.search(r'janela \d+ criada por \d+: "Cip.*?" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
            verificar(ja is not None, "la ventana de Cipó tiene origen conocido")
            jl, jaa, ox, oy = map(int, ja.groups())
            # región del título dentro de la ventana (la página empieza bajo pestañas y barra)
            vistos, titulo_visto, blancos = 0, False, 0
            t_fin = time.time() + 60
            n = 0
            while time.time() < t_fin:
                img = cuadro(m, n)
                n += 1
                oc = oscuros(img, ox + 10, oy + 70, 500, 130)
                pag = region_bytes(img, ox + 10, oy + 70, 500, 130)
                if oc > 150:
                    titulo_visto = True
                elif titulo_visto:
                    blancos += 1
                vistos += 1
                if "cipo: recursos listos" in m.serial():
                    break
            verificar(titulo_visto, "se vio el título de la página durante la carga (%d cuadros)" % vistos)
            verificar(blancos == 0, "ningún cuadro mostró la página vacía o a medias tras verse el título (%d de %d)" % (blancos, vistos))
            s = m.serial().replace("\r", "")
            est = re.search(r"cipo: pintura: (\d+) repintados, (\d+) apresentacoes, (\d+) layouts, (\d+) ms pintando, (\d+) trozos", s)
            ms = re.search(r"cipo: recursos listos en (\d+) ms", s)
            verificar(est is not None and ms is not None, "Cipó informa repintados y duración de la carga")
            rep, apr, lay, mpint, trozos = map(int, est.groups())
            dur = int(ms.group(1))
            tope = dur // 80 + 3
            print("carga: %d ms, repintados %d, presentaciones %d, layouts %d (trozos recibidos %d), %d ms pintando; tope %d"
                  % (dur, rep, apr, lay, trozos, mpint, tope))
            verificar(rep <= tope, f"repintados durante la carga acotados (<= {tope}): {rep}")
            verificar(rep < trozos, f"menos repintados ({rep}) que trozos recibidos ({trozos}; antes uno por trozo)")
            # estado final estable antes de desplazar
            m.esperar_serial("cipo: pagina pintada", 30)
            time.sleep(1.0)
            x0, y0, w, h = ox + 10, oy + 110, 600, jaa - 190
            # Fin: salto grande = repintado entero
            m.tecla("end", 0.1)
            m.esperar_serial("cipo: rolagem pintada", 20)
            time.sleep(1.0)
            fin_entero = region_bytes(cuadro(m, 900), x0, y0, w, h)
            verificar(oscuros(cuadro(m, 901), x0, y0, w, h) > 100, "el final de la página tiene texto")
            m.tecla("home", 0.1)
            time.sleep(1.0)
            ant = len(re.findall("cipo: rolagem pintada", m.serial()))
            # muchos AvPág: cada uno copia lo pintado y pinta la franja nueva
            blancos = 0
            for k in range(14):
                m.tecla("pgdn", 0.0)
                time.sleep(0.25)
                img = cuadro(m, 100 + k)
                if oscuros(img, x0, y0, w, h) < 50 and k < 10:
                    blancos += 1
            time.sleep(1.0)
            fin_copias = region_bytes(cuadro(m, 902), x0, y0, w, h)
            s = m.serial().replace("\r", "")
            tiempos = [int(t) for t in re.findall(r"cipo: rolagem pintada en (\d+) ms", s)]
            verificar(blancos == 0, "ningún cuadro del desplazamiento quedó en blanco (%d)" % blancos)
            verificar(len(tiempos) - ant >= 5, "el desplazamiento por copia ocurrió (%d)" % (len(tiempos) - ant))
            if tiempos:
                print("desplazamientos (ms por pintura): max %d, media %.1f" % (max(tiempos), sum(tiempos) / len(tiempos)))
            verificar(fin_entero == fin_copias, "el final alcanzado por copias es idéntico al repintado entero")
    finally:
        servidor.shutdown()
        servidor.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
