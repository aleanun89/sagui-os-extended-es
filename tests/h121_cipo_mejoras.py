#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H121 - Cipó: busca sem tildes, copiar e marcadores (D148).

Critério:
  - a busca ignora os acentos: "cancion" encontra "canción";
  - Ctrl+D guarda a página como marcador e o registo diz qual é;
  - Ctrl+B abre o menu dos marcadores;
  - Ctrl+C copia: o link que está debaixo do rato, ou o texto da página;
  - nada disto parte o navegador (a página continua a pintar).
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

HTML = """<!doctype html><html><head><title>Página com tildes</title>
<style>body { font-family: sans-serif; } a { color: #174fc0; }</style></head>
<body><h1>Busca sem tildes</h1>
<p>Esta pagina fala de cancion e de cancion tambem, com tilde: canción.</p>
<p><a href="/destino.html">Enlace de prueba</a></p></body></html>"""


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


def caixa_azul(imagem, regiao):
    """La caja de píxeles azules (un enlace) dentro de una región."""
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


def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    imagem = imagem_de_teste(f"/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    try:
        with Maquina("h121", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial('cipo: pronto', 90)
            s = m.serial().replace("\r", "")
            janela = re.search(r'janela \d+ criada por \d+: "Cip.*?" \d+x\d+ em \((\d+),(\d+)\)', s)
            verificar(janela is not None, "a janela do Cipó tem origem conhecida")
            ox, oy = map(int, janela.groups())

            # ---- a busca ignora as tildes ----
            m.clique(ox + 400, oy + 60)          # clica no título (não é um link): a janela fica com o foco
            m.tecla("ctrl-f")
            time.sleep(0.5)
            m.digitar("cancion")
            m.esperar_serial('cipo: busca "cancion"', 20)
            s = m.serial().replace("\r", "")
            busca = re.findall(r'cipo: busca "cancion": (\d+) de (\d+)', s)
            verificar(bool(busca), "a busca responde no registo")
            if busca:
                verificar(int(busca[-1][1]) >= 1, f"\"cancion\" encontra a palavra com tilde ({busca[-1][1]} de {busca[-1][0]})")
            m.tecla("esc")
            time.sleep(0.3)

            # ---- Ctrl+D guarda um marcador ----
            m.tecla("ctrl-d")
            m.esperar_serial("cipo: marcador 1", 15)
            s = m.serial().replace("\r", "")
            marc = re.findall(r"cipo: marcador (\d+) (\S+) \((.*?)\)", s)
            verificar(bool(marc), "o marcador fica registado")
            if marc:
                verificar(marc[-1][1].endswith("/"), f"o marcador aponta para a página ({marc[-1][1]})")
                verificar("tildes" in marc[-1][2] or "Busca" in marc[-1][2], f"o marcador guarda o título ({marc[-1][2]})")

            # ---- Ctrl+B abre o menu dos marcadores ----
            m.tecla("ctrl-b")
            m.esperar_serial("cipo: menu de marcadores", 15)
            s = m.serial().replace("\r", "")
            mm = re.findall(r"cipo: menu de marcadores \((\d+)\)", s)
            verificar(bool(mm) and int(mm[-1]) == 1, f"o menu mostra o marcador guardado ({mm[-1] if mm else '?'})")
            m.tecla("esc")
            time.sleep(0.3)

            # ---- Ctrl+C copia o texto da página ----
            m.tecla("ctrl-c")
            m.esperar_serial("cipo: copiado", 15)
            s = m.serial().replace("\r", "")
            cop = re.findall(r"cipo: copiado (.+?) \((\d+) bytes\)", s)
            print("    linhas do registo:", cop)
            verificar(bool(cop), "o Ctrl+C copia para a área de transferência")
            if cop:
                verificar(int(cop[-1][1]) > 0, f"a cópia tem conteúdo ({cop[-1][1]} bytes)")

            # ---- Ctrl+C copia o link que está debaixo do rato ----
            captura = m.captura("H121-cipo-antes")
            caixa = caixa_azul(png_pixels(captura), (ox + 10, oy + 60, 700, 300))
            if caixa:
                m.mouse_para(caixa[0] + (caixa[2] - caixa[0]) // 2, caixa[1] + (caixa[3] - caixa[1]) // 2)
                m.tecla("ctrl-c")
                m.esperar_serial("cipo: copiado o link", 15)
                s = m.serial().replace("\r", "")
                link = re.findall(r"cipo: copiado o link (\S+) \((\d+) bytes\)", s)
                verificar(bool(link), "o Ctrl+C copia o link que está debaixo do rato")
                if link:
                    verificar(link[-1][0].endswith("/destino.html"), f"o link copiado é o certo ({link[-1][0]})")
            else:
                verificar(False, "não se viu o link azul na captura")

            # ---- a página continua a pintar ----
            s = m.serial().replace("\r", "")
            motor = re.findall(r"cipo: motor nos=(\d+) cmds=(\d+)", s)
            verificar(bool(motor) and int(motor[-1][1]) > 0, "a página continua pintada depois das teclas")
            m.captura("H121-cipo-marcadores")
    finally:
        servidor.shutdown()
        servidor.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
