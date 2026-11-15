#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H114 - Cipó: recursos por vuelta, pintura en bandas y guardar imágenes/enlaces."""
import hashlib
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

PNG = None
BINARIO = b"archivo de prueba h114\x00\xff"
HTML = """<!doctype html><html><head><title>Prueba descargas</title></head><body>
<h1>Recursos separados</h1><p><img src="/foto uno.png" alt="Foto uno"></p>
<p><a href="/archivo uno.bin">Enlace para guardar</a></p>
<p><img src="/segunda.png" alt="Segunda"></p></body></html>"""


def png(l, a, color):
    filas = b"".join(b"\x00" + bytes(color) * l for _ in range(a))

    def trozo(tipo, dados):
        c = struct.pack(">I", len(dados)) + tipo + dados
        return c + struct.pack(">I", zlib.crc32(tipo + dados) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", l, a, 8, 2, 0, 0, 0)) +
            trozo(b"IDAT", zlib.compress(filas)) + trozo(b"IEND", b""))


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == "/":
            dados, tipo = HTML.encode(), "text/html; charset=utf-8"
        elif self.path in ("/foto%20uno.png", "/foto uno.png"):
            dados, tipo = png(40, 24, (30, 160, 60)), "image/png"
        elif self.path == "/segunda.png":
            dados, tipo = png(36, 20, (40, 80, 200)), "image/png"
        elif self.path == "/archivo%20uno.bin":
            dados, tipo = BINARIO, "application/octet-stream"
        else:
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(dados)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(dados)
        self.close_connection = True


def esperar(m, patron, tiempo=25, desde=0):
    limite = time.time() + tiempo
    while time.time() < limite:
        encontrados = re.findall(patron, m.serial()[desde:])
        if encontrados:
            return encontrados[-1]
        time.sleep(0.1)
    raise AssertionError(f"tiempo agotado esperando {patron!r}")


def abrir_menu(m, x, y):
    desde = len(m.serial())
    m.clique(x, y, botao=2)
    g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", desde=desde)
    return tuple(map(int, g))


def linha(menu, indice):
    return menu[0] + menu[2] // 2, menu[1] + 2 + indice * 20 + 8


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


def achar_cor(caminho, cor, tol=12):
    """Devolve (x, y) do centro da mancha da cor pedida na captura."""
    largura, altura, canais, px = png_pixels(caminho)
    alvo = []
    for y in range(altura):
        linha = px[y]
        for x in range(largura):
            i = x * canais
            if (abs(linha[i] - cor[0]) <= tol and abs(linha[i + 1] - cor[1]) <= tol
                    and abs(linha[i + 2] - cor[2]) <= tol):
                alvo.append((x, y))
    if not alvo:
        return None
    xs = [a[0] for a in alvo]
    ys = [a[1] for a in alvo]
    return (min(xs) + max(xs)) // 2, (min(ys) + max(ys)) // 2
def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    imagem = imagem_de_teste(f"/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    try:
        with Maquina("h114", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial('cipo: pronto "Prueba descargas"', 90)
            esperar(m, r"cipo: recurso en trozo", 25)
            serial = m.serial()
            trozos = re.findall(r"cipo: recurso en trozo nos=(\d+) cmds=(\d+) imgs=(\d+)", serial)
            verificar(len(trozos) >= 1, f"recursos procesados individualmente por el bucle ({len(trozos)} pasos)")
            verificar("cipo: a carregar recursos" in serial,
                      "se anuncia a carga dos recursos antes de os processar")
            # O indicador fica ligado enquanto houver recursos por tratar: o
            # aviso de carga tem de vir antes dos passos de recurso.
            aviso = serial.find("cipo: a carregar recursos")
            primeiro = serial.find("cipo: recurso en trozo")
            verificar(0 <= aviso < primeiro,
                      "o indicador de carga fica ligado durante o processamento por trozos")
            m.esperar_serial("cipo: guardado", 1) if "cipo: guardado" in serial else None

            captura = m.captura("H114-cipo-recursos")
            verificar(os.path.getsize(captura) > 1000, "captura H114 generada con página y recursos")

            # La imagen está en la primera línea de contenido de página (tras la cabecera).
            # El clic derecho en el recurso ofrece la acción para guardar.
            ox = re.search(r'janela \d+ criada por \d+: "Cip.*?" \d+x\d+ em \((\d+),(\d+)\)', serial)
            verificar(ox is not None, "origen de ventana disponible para localizar objetivos")
            caminhos = []
            if ox:
                x0, y0 = map(int, ox.groups())
                # A imagem de teste e de uma cor solida: acha-se na captura em
                # vez de adivinhar as coordenadas.
                pos_img = achar_cor(captura, (30, 160, 60))
                verificar(pos_img is not None, "a imagem de teste aparece na captura")
                menu = abrir_menu(m, pos_img[0], pos_img[1])
                verificar(menu[4] >= 1, f"el menú contextual de imagen abre ({menu[4]} opciones)")
                m.clique(*linha(menu, 0))
                m.esperar_serial("dialogo: guardar en", 25)   # mejora 103: diálogo común de guardar
                time.sleep(0.6)
                m.tecla("ret")                                # acepta la propuesta
                caminho, tam = esperar(m, r"cipo: guardado (\S+) \((\d+) bytes\)", 25)
                caminhos.append(caminho)
                verificar(caminho.endswith("foto_uno.png"),
                          f"guardar imagen escribe nombre saneado ({caminho})")
                esperado = len(png(40, 24, (30, 160, 60)))
                verificar(int(tam) == esperado,
                          f"o arquivo da imagem tem os bytes do servidor ({tam} de {esperado})")

                # O enlace esta entre a imagem verde e a azul: procura os pixels
                # azuis do texto do link na faixa entre as duas imagens.
                pos_azul = achar_cor(captura, (40, 80, 200))
                verificar(pos_azul is not None, "a segunda imagem aparece na captura")
                if pos_azul:
                    topo_link = pos_img[1] + 12
                    base_link = pos_azul[1] - 10
                    caixa = caixa_azul(png_pixels(captura), (x0 + 10, topo_link, 500, base_link - topo_link))
                    verificar(caixa is not None, "o texto do enlace tem pixels azuis")
                    if caixa:
                        x = (caixa[0] + caixa[2]) // 2
                        y = (caixa[1] + caixa[3]) // 2
                        antes = len(m.serial())
                        menu = abrir_menu(m, x, y)
                        verificar(menu[4] >= 1, f"el menú contextual del enlace abre ({menu[4]} opciones)")
                        m.clique(*linha(menu, 0))
                        esperar(m, r"dialogo: guardar en", 25, desde=antes)
                        time.sleep(0.6)
                        m.tecla("ret")
                        caminho, tam = esperar(m, r"cipo: guardado (\S+) \((\d+) bytes\)", 25, desde=antes)
                        caminhos.append(caminho)
                        verificar(caminho.endswith("archivo_uno.bin"),
                                  f"guardar enlace escribe el archivo binario ({caminho})")
                        verificar(int(tam) == len(BINARIO),
                                  f"o arquivo do enlace tem os bytes do servidor ({tam} de {len(BINARIO)})")

            # O shell nao responde em modo grafico: a prova de que o ficheiro
            # ficou no disco e o proprio registo do Cipó (caminho e contagem de
            # bytes escrita, que so e igual se a escrita foi completa).
            verificar(len(caminhos) == 2, "as duas descargas foram gravadas com o nome saneado")
            for c in caminhos:
                verificar(c.startswith("/tmp/") or c.startswith("/home/"),
                          f"a descarga foi guardada numa pasta do sistema ({c})")
    finally:
        servidor.shutdown()
        servidor.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
