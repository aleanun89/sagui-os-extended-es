#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H135 - o JavaScript da página tem um DOM de verdade (D160).

Critério:
  - a página é uma árvore de elementos: o script encontra um elemento pelo
    id (getElementById), pela classe e pela etiqueta (querySelector,
    querySelectorAll, getElementsByTagName) e lê o que tem (tagName, id,
    className, textContent, getAttribute);
  - o script põe um tratador de clique (element.onclick e
    element.addEventListener("click", ...));
  - ao pulsar com o rato num elemento, o tratador corre (e o evento tem o
    type e o target);
  - um preventDefault() cancela a navegação do enlace.
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

PAGINA = """<!doctype html><html><head><title>Pagina com DOM</title>
<style>a { color: #174fc0; }</style></head>
<body>
<h1 id="titulo">Titulo</h1>
<p class="destaque" id="intro">Texto de introducao</p>
<p><a id="menu" href="/destino.html">Menu</a></p>
<div id="caixa" style="background:#ff0000">Caixa</div>
<script>
  var t = document.getElementById('titulo');
  console.log('dom id ' + t.id);
  console.log('dom tag ' + t.tagName);
  console.log('dom texto ' + t.textContent);
  var i = document.querySelector('#intro');
  console.log('dom classe ' + i.className);
  var d = document.querySelector('.destaque');
  console.log('dom porclasse ' + d.id);
  var as = document.getElementsByTagName('a');
  console.log('dom links ' + as.length);
  console.log('dom href ' + as[0].getAttribute('href'));
  console.log('dom pai ' + document.body.tagName);
  var m = document.getElementById('menu');
  m.onclick = function(ev) { console.log('dom clic menu ' + ev.type); ev.preventDefault(); };
  var c = document.getElementById('caixa');
  c.addEventListener('click', function(ev) { console.log('dom clic caixa ' + ev.target.id); });
  console.log('dom pronto');
</script>
</body></html>"""

DESTINO = ("<!doctype html><html><head><title>Destino</title></head>"
           "<body><h1>Pagina de destino</h1></body></html>")


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path.startswith("/destino"):
            corpo = DESTINO.encode("utf-8")
        else:
            corpo = PAGINA.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(corpo)
        self.close_connection = True


def esperar_pintada(m, minimo=2, tempo=40):
    """Espera a que o Cipó termine de pintar a página."""
    limite = time.time() + tempo
    while time.time() < limite:
        if m.serial().count("cipo: pagina pintada") >= minimo:
            return True
        time.sleep(0.2)
    return False


def png_pixels(caminho):
    """Decodifica os filtros PNG necessários para contar os píxeles."""
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


def caixa_de(linhas, canais, teste, regiao):
    """A caixa (x1, y1, x2, y2) dos píxeles que passam no teste, dentro da
    região dada (x, y, largura, altura)."""
    x0, y0, larg, alt = regiao
    xs, ys = [], []
    for y in range(max(0, y0), min(len(linhas), y0 + alt)):
        linha = linhas[y]
        for x in range(max(0, x0), min(len(linha) // canais, x0 + larg)):
            i = x * canais
            if teste(linha[i], linha[i + 1], linha[i + 2]):
                xs.append(x)
                ys.append(y)
    if not xs:
        return None
    return (min(xs), min(ys), max(xs), max(ys))


def azul(r, g, b):
    return b > r + 50 and b > g + 20 and b > 50


def vermelho(r, g, b):
    return r > 150 and g < 90 and b < 90


def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    imagem = imagem_de_teste(f"/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    try:
        with Maquina("h135", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial('cipo: pronto "Pagina com DOM"', 90)
            s = m.serial().replace("\r", "")

            # ---------------------------------------- o script le o DOM
            esperado = [
                "dom id titulo",
                "dom tag H1",
                "dom texto Titulo",
                "dom classe destaque",
                "dom porclasse intro",
                "dom links 1",
                "dom href /destino.html",
                "dom pai BODY",
                "dom pronto",
            ]
            for texto in esperado:
                verificar(f"cipo: js: {texto}" in s,
                          f"o script leu «{texto}» do DOM")
            verificar("qjs: erro" not in s, "o script do DOM não deu erro")
            verificar("cipo: js: 1 script(s) correram" in s,
                      "o <script> da página correu")

            # ---------------------------------------- onde está a página
            janela = re.search(r'janela \d+ criada por \d+: "Cip.*?" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
            verificar(janela is not None, "a janela do Cipó tem origem conhecida")
            jlarg, jalt, ox, oy = map(int, janela.groups())
            esperar_pintada(m)
            captura = m.captura("H135-qjs-dom", guardar=False)
            largura, altura, canais, linhas = png_pixels(captura)
            # a área da página dentro da janela (a barra de abas fica acima)
            pagina = (ox + 4, oy + 68, jlarg - 24, 320)

            # ---------------------------------------- o clique na caixa (div)
            caixa = caixa_de(linhas, canais, vermelho, pagina)
            verificar(caixa is not None, "a caixa vermelha da página aparece")
            if caixa is None:
                return 1
            x = caixa[0] + 30
            y = (caixa[1] + caixa[3]) // 2
            desde = len(m.serial())
            m.clique(x, y)
            time.sleep(1.5)
            s2 = m.serial()[desde:].replace("\r", "")
            print("    no clique na caixa:", s2.strip()[:300])
            verificar("cipo: js: dom clic caixa caixa" in s2,
                      "o tratador de clique da caixa correu")
            verificar("cipo: abrindo" not in s2,
                      "a caixa não navegou (não tem endereço)")

            # ---------------------------------------- o clique no enlace
            link = caixa_de(linhas, canais, azul, pagina)
            verificar(link is not None, "o enlace da página aparece")
            if link is None:
                return 1
            x = (link[0] + link[2]) // 2
            y = (link[1] + link[3]) // 2
            desde = len(m.serial())
            m.clique(x, y)
            time.sleep(1.5)
            s3 = m.serial()[desde:].replace("\r", "")
            print("    no clique no enlace:", s3.strip()[:200])
            verificar("cipo: js: dom clic menu click" in s3,
                      "o tratador de clique do enlace correu")
            verificar("cipo: js: el clic lo cancelo el script" in s3,
                      "o preventDefault do script cancelou o clique")
            verificar("cipo: abrindo" not in s3,
                      "o enlace não navegou (o preventDefault segurou)")
            verificar('cipo: pronto "Destino"' not in s3,
                      "a página não mudou de sítio")

            # ---------------------------------------- o navegador continua vivo
            s = m.serial().replace("\r", "")
            verificar("Fallo de página" not in s, "o navegador não se caiu")
            verificar("qjs: erro" not in s, "nenhum erro de JavaScript")
    finally:
        servidor.shutdown()
        servidor.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
