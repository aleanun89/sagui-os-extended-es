#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H127 - Cipó: abrir o link noutra aba e lembrar a posição de leitura (D152).

Critério:
  - Ctrl+clic sobre um enlace abre-o numa aba nova (a aba atual fica como
    estava e a nova fica ativa, com a página do enlace);
  - o menu do botão direito sobre um enlace oferece "Abrir en pestaña nueva"
    (e continua a oferecer "Guardar imagen" e "Guardar enlace");
  - um clic normal continua a navegar na aba atual (sem regressão);
  - a posição de leitura de cada sítio fica guardada: ao voltar ao sítio
    (navegando outra vez ou recarregando) a página aparece onde se leu;
  - a posição sobrevive a fechar e abrir o programa: fica no ficheiro
    /home/.config/cipo/posicoes.
"""
import http.server
import json
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_de_teste, extrair_particao,  # noqa: E402
                  SAIDA, DEBUGFS, particao_dados_img)

LONGA = """<!doctype html><html><head><title>Pagina comprida</title>
<style>body { font-family: sans-serif; } a { color: #174fc0; }</style></head><body>
<h1>Pagina comprida</h1>
<p><a href="/outro.html">Enlace A</a></p>
<p><a href="/terceira.html">Enlace B</a></p>
""" + "".join(f"<p>Linha numero {i} da pagina comprida, para poder ler mais abaixo.</p>\n"
               for i in range(1, 120)) + """
</body></html>"""

OUTRA = "<!doctype html><html><head><title>Outra pagina</title></head><body><h1>Outra pagina</h1></body></html>"
TERCEIRA = "<!doctype html><html><head><title>Terceira pagina</title></head><body><h1>Terceira pagina</h1></body></html>"


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == "/outro.html":
            corpo = OUTRA.encode("utf-8")
        elif self.path == "/terceira.html":
            corpo = TERCEIRA.encode("utf-8")
        else:
            corpo = LONGA.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(corpo)
        self.close_connection = True


def esperar(m, patron, tempo=25, desde=0):
    """Espera um texto no registo (a serial) e devolve a última ocorrência."""
    limite = time.time() + tempo
    while time.time() < limite:
        encontrados = re.findall(patron, m.serial()[desde:])
        if encontrados:
            return encontrados[-1]
        time.sleep(0.1)
    raise AssertionError(f"tempo esgotado esperando {patron!r}")


def esperar_pintada(m, minimo=2, tempo=30):
    """Espera a que o Cipó termine de pintar a página.

    O Cipó pinta por faixas e só escreve "pronto" (com o título) ANTES de
    pintar: se a captura se tirasse logo a seguir, apanhava a página ainda
    em branco (e os enlaces, que são azuis, não apareciam). Por isso se
    espera pelo "página pintada", que sai duas vezes (a primeira antes dos
    recursos e a segunda depois).
    """
    limite = time.time() + tempo
    while time.time() < limite:
        if m.serial().count("cipo: pagina pintada") >= minimo:
            return True
        time.sleep(0.2)
    return False


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
                atual[i] = (atual[i] + ((a + b) >> 1)) & 0xFF
            elif filtro == 4:
                p0 = a + b - c
                pa, pb, pc = abs(p0 - a), abs(p0 - b), abs(p0 - c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                atual[i] = (atual[i] + pr) & 0xFF
        linhas.append(bytes(atual))
        anterior = atual
    return largura, altura, canais, linhas


def caixas_azuis(imagem, regiao):
    """Las cajas de píxeles azules (los enlaces) de una región, una por línea."""
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
        return []
    ys = sorted(set(p[1] for p in pontos))
    bandas = []
    inicio = anterior = ys[0]
    for y in ys[1:]:
        if y - anterior > 4:      # una línea nueva de texto
            bandas.append((inicio, anterior))
            inicio = y
        anterior = y
    bandas.append((inicio, anterior))
    caixas = []
    for ya, yb in bandas:
        xs = [p[0] for p in pontos if ya <= p[1] <= yb]
        caixas.append((min(xs), ya, max(xs), yb))
    return caixas


def centro(caixa):
    return (caixa[0] + caixa[2]) // 2, (caixa[1] + caixa[3]) // 2


def abrir_menu(m, x, y):
    """Botão direito: devolve (x, y, largura, altura, n_linhas) do menu aberto."""
    desde = len(m.serial())
    m.clique(x, y, botao=2)
    g = esperar(m, r"\[GUI\] menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", desde=desde)
    return tuple(map(int, g))


def linha_do_menu(menu, indice):
    """O ponto de uma linha do menu (cada linha tem 20 px de alto)."""
    return menu[0] + menu[2] // 2, menu[1] + 2 + indice * 20 + 8


class Qmp:
    """Cliente mínimo do QMP do QEMU.

    O HMP (o monitor que usa o qemu.py) não serve para isto: "sendkey ctrl"
    larga a tecla assim que se manda a próxima tecla, e para um Ctrl+clic a
    tecla tem de estar apertada enquanto o rato clica. O QMP resolve-o com
    "input-send-event", que manda os eventos um a um (tecla apertada, botão
    do rato, botão largado, tecla largada).
    """

    def __init__(self, caminho):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(10)
        self.sock.connect(caminho)
        self.buf = b""
        self._objeto()                      # a saudação
        self.comando({"execute": "qmp_capabilities"})

    def _objeto(self):
        while b"\n" not in self.buf:
            parte = self.sock.recv(65536)
            if not parte:
                raise AssertionError("o QMP fechou a ligação")
            self.buf += parte
        linha, self.buf = self.buf.split(b"\n", 1)
        return json.loads(linha)

    def comando(self, cmd):
        self.sock.sendall((json.dumps(cmd) + "\n").encode())
        while True:
            obj = self._objeto()
            if "error" in obj:
                raise AssertionError(f"erro do QMP: {obj['error']}")
            if "return" in obj:
                return obj

    def eventos(self, lista):
        return self.comando({"execute": "input-send-event", "arguments": {"events": lista}})

    def tecla(self, down, qcode="ctrl"):
        self.eventos([{"type": "key", "data": {"down": down, "key": {"type": "qcode", "data": qcode}}}])

    def botao(self, down, nome="left"):
        self.eventos([{"type": "btn", "data": {"down": down, "button": nome}}])


def clique_com_ctrl(qmp, m, x, y):
    """Clica com o Ctrl apertado (o rato é relativo: primeiro vai ao ponto)."""
    m.mouse_para(x, y)
    time.sleep(0.3)
    qmp.tecla(True)
    time.sleep(0.3)
    qmp.botao(True)
    time.sleep(0.2)
    qmp.botao(False)
    time.sleep(0.2)
    qmp.tecla(False)
    time.sleep(0.5)


def particao_dados(img):
    """El LBA y los sectores de la partición de datos (/home): lógica desde la v7.3 (D213)."""
    return particao_dados_img(img)


def ler_ficheiro_home(img, caminho):
    """Lê um ficheiro do /home (a partição de dados) desde o PC."""
    lba, setores = particao_dados(img)
    parte = os.path.join(SAIDA, "h127-home.img")
    extrair_particao(img, lba, (setores + 2047) // 2048, parte)
    debugfs = shutil.which("debugfs") or DEBUGFS
    r = subprocess.run([debugfs, "-R", f"cat {caminho}", parte], capture_output=True, text=True)
    return r.stdout


def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    base = f"http://10.0.2.2:{porta}"
    # Imagem com partição de dados (/home), para que o ficheiro das posições
    # fique no disco e se possa ver (e reler num segundo arranque).
    imagem = imagem_de_teste(f"/bin/cipo {base}/longa.html", ["--vesa", "--dados-mb", "16"])
    qmp_path = f"/tmp/sagui-h127-{os.getpid()}.sock"   # nome próprio: não choca com outra prova
    try:
        with Maquina("h127", "ide", imagem=imagem,
                     extras=["-nic", "user,model=e1000", "-qmp", f"unix:{qmp_path},server,nowait"],
                     snapshot=False) as m:
            qmp = Qmp(qmp_path)
            m.esperar_serial('cipo: pronto "Pagina comprida"', 90)
            s = m.serial().replace("\r", "")
            janela = re.search(r'janela \d+ criada por \d+: "Cip.*?" \d+x\d+ em \((\d+),(\d+)\)', s)
            verificar(janela is not None, "a janela do Cipó tem origem conhecida")
            ox, oy = map(int, janela.groups())

            # ---- onde estão os dois enlaces (os pixels azuis do texto) ----
            esperar_pintada(m)
            captura = m.captura("H127-cipo-inicio", guardar=False)
            caixas = caixas_azuis(png_pixels(captura), (ox + 4, oy + 70, 840, 200))
            verificar(len(caixas) >= 2, f"a página mostra os dois enlaces ({len(caixas)} encontrados)")
            if len(caixas) < 2:
                return 1
            link_a = centro(caixas[0])
            link_b = centro(caixas[1])

            # ---- Ctrl+clic abre o enlace noutra aba ----
            desde = len(m.serial())
            clique_com_ctrl(qmp, m, *link_a)
            esperar(m, r"cipo: link noutra aba (\d+) \((.*?)\)", 25, desde=desde)
            s = m.serial()[desde:].replace("\r", "")
            abrir = re.findall(r"cipo: link noutra aba (\d+) \((.*?)\)", s)
            verificar(bool(abrir), "o Ctrl+clic abre o link noutra aba")
            if abrir:
                verificar(abrir[-1][1].endswith("/outro.html"),
                          f"a aba nova tem a página do link ({abrir[-1][1]})")
                verificar(abrir[-1][0] == "1", f"a aba nova é a segunda ({abrir[-1][0]})")
            esperar(m, r"cipo: pronto \"Outra pagina\"", 30, desde=desde)
            verificar(True, "a aba nova mostra a página do link")
            # a aba anterior continua com a sua página (não se descarrega outra vez)
            verificar("cipo: voltar para" not in s, "a aba anterior não se mexeu")

            # ---- Ctrl+Tab volta à aba 0 ----
            desde = len(m.serial())
            m.tecla("ctrl-tab")
            esperar(m, r"cipo: aba 0", 20, desde=desde)
            verificar(True, "volta-se à aba da página comprida")

            # ---- o menu do botão direito oferece "Abrir en pestaña nueva" ----
            desde = len(m.serial())
            menu = abrir_menu(m, *link_b)
            verificar(menu[4] >= 2, f"o menu do link tem mais de uma linha ({menu[4]})")
            linhas = esperar(m, r"\[GUI\] menu linhas: (.*)", 10, desde=desde)
            print("    linhas do menu:", linhas)
            verificar("Abrir en pestaña nueva" in linhas,
                      f"o menu oferece abrir noutra aba ({linhas})")
            verificar("Guardar enlace" in linhas,
                      f"o menu continua a oferecer guardar o enlace ({linhas})")

            # ---- escolher "Abrir en pestaña nueva" abre a aba ----
            desde = len(m.serial())
            m.clique(*linha_do_menu(menu, menu[4] - 1))
            esperar(m, r"cipo: link noutra aba (\d+) \((.*?)\)", 25, desde=desde)
            s = m.serial()[desde:].replace("\r", "")
            abrir = re.findall(r"cipo: link noutra aba (\d+) \((.*?)\)", s)
            verificar(bool(abrir), "a escolha do menu abre o link noutra aba")
            if abrir:
                verificar(abrir[-1][1].endswith("/terceira.html"),
                          f"a aba nova tem a página do link B ({abrir[-1][1]})")
            esperar(m, r"cipo: pronto \"Terceira pagina\"", 30, desde=desde)
            verificar(True, "a aba nova do menu mostra a página do link")

            # ---- volta à aba 0 e um clique normal navega na aba atual ----
            desde = len(m.serial())
            m.tecla("ctrl-tab")
            esperar(m, r"cipo: aba 0", 20, desde=desde)
            desde = len(m.serial())
            m.clique(*link_a)
            esperar(m, r"cipo: abrindo (\S+)", 30, desde=desde)
            s = m.serial()[desde:].replace("\r", "")
            verificar("outro.html" in s, "o clique normal abre a página do enlace")
            verificar("link noutra aba" not in s,
                      "o clique normal não abre nenhuma aba nova")
            verificar("nova aba" not in s, "o clique normal não abre nenhuma aba nova (2)")

            # ---- volta atrás para a página comprida ----
            desde = len(m.serial())
            m.tecla("alt-left")
            esperar(m, r"cipo: voltar para (\S+)", 30, desde=desde)
            verificar(True, "volta-se atrás para a página comprida")

            # ---- ler até ao fim, sair e voltar: a posição lembra-se ----
            # (sem clicar: a janela do Cipó já tem o foco das teclas de antes)
            desde = len(m.serial())
            m.tecla("end")
            esperar(m, r"cipo: rolagem para (\d+) px", 15, desde=desde)
            s = m.serial()[desde:].replace("\r", "")
            rolagens = re.findall(r"cipo: rolagem para (\d+) px", s)
            verificar(bool(rolagens) and int(rolagens[-1]) > 0,
                      f"a página comprida desliza até ao fim ({rolagens[-1] if rolagens else '?'} px)")
            desde = len(m.serial())
            m.tecla("ctrl-l")
            time.sleep(0.4)
            m.digitar(f"{base}/outro.html")
            m.tecla("ret")
            esperar(m, r"cipo: posicao guardada \((\d+) px\) em (\S+)", 30, desde=desde)
            s = m.serial()[desde:].replace("\r", "")
            guardadas = re.findall(r"cipo: posicao guardada \((\d+) px\) em (\S+)", s)
            verificar(bool(guardadas), "a posição de leitura fica guardada ao sair do sítio")
            if guardadas:
                verificar(int(guardadas[-1][0]) > 0,
                          f"a posição guardada é a que se leu ({guardadas[-1][0]} px)")
                verificar(guardadas[-1][1].endswith("/longa.html"),
                          f"a posição é da página comprida ({guardadas[-1][1]})")

            # ---- voltar ao sítio: a posição repõe-se ----
            desde = len(m.serial())
            m.tecla("ctrl-l")
            time.sleep(0.4)
            m.digitar(f"{base}/longa.html")
            m.tecla("ret")
            esperar(m, r"cipo: posicao reposta \((\d+) px\) em (\S+)", 30, desde=desde)
            s = m.serial()[desde:].replace("\r", "")
            repostas = re.findall(r"cipo: posicao reposta \((\d+) px\) em (\S+)", s)
            verificar(bool(repostas), "ao voltar ao sítio a posição de leitura repõe-se")
            if repostas and guardadas:
                verificar(int(repostas[-1][0]) > 0,
                          f"a página volta onde se leu ({repostas[-1][0]} px)")
                verificar(int(repostas[-1][0]) == int(guardadas[-1][0]),
                          f"é a mesma posição que se guardou ({repostas[-1][0]} px)")
            esperar_pintada(m)
            m.captura("H127-cipo-posicao", guardar=False)

        # ---- a posição ficou no ficheiro (e sobrevive ao arranque) ----
        conteudo = ler_ficheiro_home(imagem, "/.config/cipo/posicoes")
        print("    ficheiro das posições:", conteudo.replace("\n", " | "))
        verificar("longa.html" in conteudo, "o ficheiro das posições tem o sítio")
        linhas = [l for l in conteudo.splitlines() if "longa.html" in l]
        if linhas:
            pos = linhas[-1].split("\t")[-1]
            verificar(pos.isdigit() and int(pos) > 0,
                      f"o ficheiro guarda a posição de leitura ({pos} px)")

        # ---- segundo arranque: a posição está lá ----
        with Maquina("h127b", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"],
                     snapshot=False) as m:
            m.esperar_serial("cipo: pronto", 90)
            s = m.serial().replace("\r", "")
            repostas = re.findall(r"cipo: posicao reposta \((\d+) px\) em (\S+)", s)
            verificar(bool(repostas), "no arranque seguinte a posição repõe-se logo")
            if repostas:
                verificar(int(repostas[-1][0]) > 0,
                          f"a página aparece onde se leu ({repostas[-1][0]} px)")
                verificar(repostas[-1][1].endswith("/longa.html"),
                          f"é a página comprida ({repostas[-1][1]})")
            esperar_pintada(m)
            m.captura("H127-cipo-arranque", guardar=False)
    finally:
        servidor.shutdown()
        servidor.server_close()
        if os.path.exists(qmp_path):
            os.remove(qmp_path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
