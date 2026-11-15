#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H123 - Cipó: as imagens (JPEG, tamanho do HTML e as grandes) (D148).

Critério:
  - um JPEG aparece na página (o motor descodifica JPEG e PNG);
  - o `width`/`height` do HTML mandam no tamanho da caixa da imagem;
  - uma imagem grande (mais de 800x600) é reduzida para caber em vez
    de ser deitada fora: antes, acima de 600 KB a imagem não aparecia;
  - os recursos chegam de um em um e o aviso de carga aparece antes (a
    interface não fica pendurada).
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
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402
from jpeg_baseline import jpeg_solido  # noqa: E402

HTML = """<!doctype html><html><head><title>Imagens</title>
<style>body { font-family: sans-serif; }</style></head>
<body><h1>Imagens</h1>
<p>Um JPEG pequeno e um PNG grande.</p>
<div><img src="/pequeno.jpg" width="80" height="60" alt="JPEG pequeno"></div>
<div><img src="/grande.png" alt="PNG grande"></div>
</body></html>"""


def png_solido(larg, alt, r, g, b):
    """Um PNG RGB de uma cor só."""
    linha = bytes((r, g, b)) * larg
    bruto = b"".join(b"\0" + linha for _ in range(alt))

    def pedaco(tipo, dados):
        return (struct.pack(">I", len(dados)) + tipo + dados +
                struct.pack(">I", zlib.crc32(tipo + dados) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += pedaco(b"IHDR", struct.pack(">IIBBBBB", larg, alt, 8, 2, 0, 0, 0))
    png += pedaco(b"IDAT", zlib.compress(bruto, 6))
    png += pedaco(b"IEND", b"")
    return png


def png_paleta(larg, alt, r, g, b):
    """Um PNG de paleta com uma cor só: um byte por pixel (descomprime muito
    mais depressa que o RGB, que é o que se nota na emulação)."""
    bruto = b"".join(b"\0" + bytes(larg) for _ in range(alt))

    def pedaco(tipo, dados):
        return (struct.pack(">I", len(dados)) + tipo + dados +
                struct.pack(">I", zlib.crc32(tipo + dados) & 0xFFFFFFFF))

    png = b"\x89PNG\r\n\x1a\n"
    png += pedaco(b"IHDR", struct.pack(">IIBBBBB", larg, alt, 8, 3, 0, 0, 0))
    png += pedaco(b"PLTE", bytes((r, g, b)))
    png += pedaco(b"IDAT", zlib.compress(bruto, 6))
    png += pedaco(b"IEND", b"")
    return png


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == "/pequeno.jpg":
            corpo, tipo = jpeg_solido(32, 24, 30, 60, 200), "image/jpeg"
        elif self.path == "/grande.png":
            corpo, tipo = png_paleta(900, 600, 20, 160, 40), "image/png"
        else:
            corpo, tipo = HTML.encode("utf-8"), "text/html; charset=utf-8"
        self.send_response(200)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(corpo)
        self.close_connection = True


def png_pixels(caminho):
    """Decodifica los filtros PNG necesarios para contar píxeles de la captura."""
    with open(caminho, "rb") as f:
        bruto = f.read()
    pos = 8
    dados = bytearray()
    while pos < len(bruto):
        n = struct.unpack(">I", bruto[pos:pos + 4])[0]
        tipo = bruto[pos + 4:pos + 8]
        parte = bruto[pos + 8:pos + 8 + n]
        pos += 12 + n
        if tipo == b"IHDR":
            largura, altura, profundidade, cor, _, _, _ = struct.unpack(">IIBBBBB", parte)
        elif tipo == b"IDAT":
            dados.extend(parte)
        elif tipo == b"IEND":
            break
    canais = {2: 3, 6: 4}.get(cor)
    if profundidade != 8 or canais is None:
        raise AssertionError(f"formato PNG de captura nao suportado: cor={cor}, profundidade={profundidade}")
    linha = largura * canais
    descomprimido = zlib.decompress(dados)
    linhas = []
    anterior = bytearray(linha)
    p = 0
    for _ in range(altura):
        filtro = descomprimido[p]
        atual = bytearray(descomprimido[p + 1:p + 1 + linha])
        p += linha + 1
        for i in range(linha):
            a = atual[i - canais] if i >= canais else 0
            b = anterior[i]
            c = anterior[i + canais] if i + canais < linha else 0
            if filtro == 1:
                atual[i] = (atual[i] + a) & 0xFF
            elif filtro == 2:
                atual[i] = (atual[i] + b) & 0xFF
            elif filtro == 3:
                atual[i] = (atual[i] + (a + b) // 2) & 0xFF
            elif filtro == 4:
                p0 = a + b - c
                pa, pb, pc = abs(p0 - a), abs(p0 - b), abs(p0 - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                atual[i] = (atual[i] + pr) & 0xFF
        linhas.append(bytes(atual))
        anterior = atual
    return largura, altura, canais, linhas


def caixa_where(imagem, teste, regiao):
    """La caja (x, y, x2, y2) de los píxeles que cumplen `teste` en la región."""
    largura, altura, canais, linhas = imagem
    x, y, l, a = regiao
    pontos = []
    for yy in range(max(0, y), min(altura, y + a)):
        fila = linhas[yy]
        for xx in range(max(0, x), min(largura, x + l)):
            i = xx * canais
            if teste(fila[i], fila[i + 1], fila[i + 2]):
                pontos.append((xx, yy))
    if not pontos:
        return None
    return (min(x for x, _ in pontos), min(y for _, y in pontos),
            max(x for x, _ in pontos), max(y for _, y in pontos))


def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    imagem = imagem_de_teste(f"/apps/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    try:
        with Maquina("h123", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("cipo: a carregar recursos", 60)
            verificar(True, "o aviso de carga de recursos aparece antes (a interface não fica pendurada)")
            m.esperar_serial("cipo: recursos listos", 300)
            s = m.serial().replace("\r", "")

            # ---- a imagem grande é reduzida, não deitada fora ----
            redu = re.findall(r"cipo: imagem reduzida para (\d+)x(\d+)", s)
            verificar(bool(redu), "a imagem grande é reduzida para caber")
            if redu:
                l, a = map(int, redu[-1])
                verificar(l * a <= 800 * 600, f"a imagem reduzida cabe no limite ({l}x{a})")

            # ---- as duas imagens chegaram ao motor ----
            trechos = re.findall(r"cipo: recurso en trozo nos=(\d+) cmds=(\d+) imgs=(\d+)", s)
            verificar(bool(trechos), "o motor foi pedindo as imagens por trozos")
            if trechos:
                max_imgs = max(int(t[2]) for t in trechos)
                verificar(max_imgs >= 2, f"as duas imagens chegaram ao motor (imgs={max_imgs})")

            # ---- o JPEG aparece com o tamanho pedido pelo HTML ----
            janela = re.search(r'janela \d+ criada por \d+: "Cip.*?" \d+x\d+ em \((\d+),(\d+)\)', s)
            verificar(janela is not None, "a janela do Cipó tem origem conhecida")
            ox, oy = map(int, janela.groups())
            captura = m.captura("H123-cipo-imagens")
            imagem_png = png_pixels(captura)
            azul = caixa_where(imagem_png, lambda r, g, b: b > 100 and b > r + 40 and b > g + 20,
                               (ox, oy, 860, 600))
            verificar(azul is not None, "o JPEG azul aparece na janela")
            if azul:
                larg = azul[2] - azul[0] + 1
                alt = azul[3] - azul[1] + 1
                verificar(70 <= larg <= 100 and 50 <= alt <= 80,
                          f"o JPEG fica no tamanho pedido pelo HTML ({larg}x{alt}, pedido 80x60)")
            verde = caixa_where(imagem_png, lambda r, g, b: g > 90 and g > r + 40 and g > b + 40,
                                (ox, oy, 860, 600))
            verificar(verde is not None, "o PNG grande (reduzido) também aparece")
    finally:
        servidor.shutdown()
        servidor.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
