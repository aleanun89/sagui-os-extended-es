#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H113 - Cipó renderiza una página HTML con el motor web real."""
import functools
import http.server
import os
import re
import struct
import sys
import threading
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

HTML = """<!doctype html><html><head><title>Prueba motor Cipó</title>
<style>body { font-family: sans-serif; color: #202830; } h1 { color: #176b37; }
a { color: #174fc0; text-decoration: underline; }</style></head>
<body><h1>Motor web real</h1><p>Texto maquetado por el motor CSS.</p>
<p><a href=\"/destino.html\">Enlace visible de prueba</a></p></body></html>"""


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        cuerpo = HTML.encode("utf-8") if self.path == "/" else b"<title>destino</title>Destino"
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(cuerpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(cuerpo)
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
            c = anterior[i - canais] if i >= canais else 0
            if filtro == 1:
                atual[i] = (atual[i] + a) & 255
            elif filtro == 2:
                atual[i] = (atual[i] + b) & 255
            elif filtro == 3:
                atual[i] = (atual[i] + ((a + b) >> 1)) & 255
            elif filtro == 4:
                q = a + b - c
                da, db, dc = abs(q - a), abs(q - b), abs(q - c)
                pr = a if da <= db and da <= dc else b if db <= dc else c
                atual[i] = (atual[i] + pr) & 255
            elif filtro != 0:
                raise AssertionError(f"filtro PNG desconhecido: {filtro}")
        linhas.append(bytes(atual))
        anterior = atual
    return largura, altura, canais, linhas


def caixa_azul(imagem, regiao):
    largura, altura, canais, linhas = imagem
    x, y, l, a = regiao
    pontos = []
    for yy in range(max(0, y), min(altura, y + a)):
        fila = linhas[yy]
        for xx in range(max(0, x), min(largura, x + l)):
            i = xx * canais
            r, g, b = fila[i:i + 3]
            if b > r + 50 and b > g + 20 and b > 50:
                pontos.append((xx, yy))
    if not pontos:
        return None
    return (min(x for x, _ in pontos), min(y for _, y in pontos),
            max(x for x, _ in pontos), max(y for _, y in pontos))


def pixels_pintados(imagem, regiao):
    largura, altura, canais, linhas = imagem
    x, y, l, a = regiao
    x1, y1 = min(largura, x + l), min(altura, y + a)
    conta = 0
    for yy in range(max(0, y), y1):
        fila = linhas[yy]
        for xx in range(max(0, x), x1):
            i = xx * canais
            if fila[i:i + 3] != bytes((255, 255, 255)):
                conta += 1
    return conta


def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    imagem = imagem_de_teste(f"/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    try:
        with Maquina("h113", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial('cipo: pronto "Prueba motor Cipó"', 90)
            # a página pinta-se a faixas: só se captura quando acabou
            m.esperar_serial("cipo: pagina pintada", 60)
            s = m.serial().replace("\r", "")
            motor = re.findall(r"cipo: motor nos=(\d+) cmds=(\d+) imgs=(\d+) altura=(\d+) links=(\d+)", s)
            verificar(bool(motor), "el Cipó informa las estadísticas del motor real")
            nos, cmds, _, altura, enlaces = map(int, motor[-1])
            verificar(nos > 0 and cmds > 0, f"el motor crea árbol y comandos ({nos} nodos, {cmds} comandos)")
            verificar(altura > 0, f"la página tiene altura de layout ({altura} px)")
            verificar(enlaces >= 1, f"el layout reconoce el enlace ({enlaces})")
            janela = re.search(r'janela \d+ criada por \d+: "Cip.*?" \d+x\d+ em \((\d+),(\d+)\)', s)
            verificar(janela is not None, "a janela do Cipó tem origen conocida")
            origem_x, origem_y = map(int, janela.groups())
            captura = m.captura("H113-cipo-motor-real")
            imagem_png = png_pixels(captura)
            largura_png, altura_png = imagem_png[:2]
            verificar(largura_png >= 860 and altura_png >= 600,
                      f"captura da janela completa ({largura_png}x{altura_png})")
            texto = pixels_pintados(imagem_png, (20, 64, 820, 100))
            link = pixels_pintados(imagem_png, (20, 150, 820, 40))
            verificar(texto > 100, f"el motor pintó texto visible ({texto} píxeles no blancos)")
            verificar(link > 10, f"el texto y subrayado del enlace son visibles ({link} píxeles no blancos)")
            caja = caixa_azul(imagem_png, (origem_x + 10, origem_y + 60, 500, 220))
            verificar(caja is not None, "el enlace tiene píxeles azules visibles")
            if caja:
                x = (caja[0] + caja[2]) // 2
                y = (caja[1] + caja[3]) // 2
                m.clique(x, y)
                m.esperar_serial("cipo: abrindo http://10.0.2.2:%d/destino.html" % porta, 20)
                verificar(True, "motor_alvo sigue el enlace visible al destino")
    finally:
        servidor.shutdown()
        servidor.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
