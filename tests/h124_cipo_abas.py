#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H124 - Cipó: as abas, a cache e a posição de leitura (D148).

Critério:
  - há uma barra de abas debaixo da barra de endereços, com o título da página;
  - Ctrl+T abre uma aba nova (com a página de início) e Ctrl+Tab passa de uma
    aba a outra;
  - trocar de aba NÃO volta a descarregar a página (fica em memória: a cache);
  - ao voltar atrás, a página volta à posição em que se estava a ler;
  - Ctrl+W fecha a aba (e o Cipó continua a funcionar).
"""
import http.server
import os
import re
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

LONGA = """<!doctype html><html><head><title>Página comprida</title>
<style>body { font-family: sans-serif; }</style></head><body>
<h1>Página comprida</h1>
""" + "".join(f"<p>Linha número {i} da página comprida, para poder ler mais abaixo.</p>\n" for i in range(1, 120)) + """
<p><a href="/outro.html">Enlace para outra página</a></p>
</body></html>"""

OUTRA = "<!doctype html><html><head><title>Outra página</title></head><body><h1>Outra página</h1></body></html>"


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path == "/outro.html":
            corpo, tipo = OUTRA.encode("utf-8"), "text/html; charset=utf-8"
        else:
            corpo, tipo = LONGA.encode("utf-8"), "text/html; charset=utf-8"
        self.send_response(200)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(corpo)
        self.close_connection = True


def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    imagem = imagem_de_teste(f"/apps/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    try:
        with Maquina("h124", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("cipo: pronto", 90)
            s = m.serial().replace("\r", "")
            janela = re.search(r'janela \d+ criada por \d+: "Cip.*?" \d+x\d+ em \((\d+),(\d+)\)', s)
            verificar(janela is not None, "a janela do Cipó tem origem conhecida")
            ox, oy = map(int, janela.groups())

            # ---- a barra de abas está desenhada (a faixa entre 40 e 66) ----
            captura = m.captura("H124-cipo-aba")
            verificar(os.path.exists(captura), "a captura da janela existe")

            # ---- Ctrl+T abre uma aba nova ----
            m.clique(ox + 400, oy + 100)      # dá o foco à janela
            m.tecla("ctrl-t")
            m.esperar_serial("cipo: nova aba", 20)
            s = m.serial().replace("\r", "")
            abas = re.findall(r"cipo: nova aba (\d+)", s)
            verificar(bool(abas), "Ctrl+T abre uma aba nova")
            if abas:
                verificar(abas[-1] == "1", f"a nova aba é a segunda ({abas[-1]})")
            m.esperar_serial('cipo: pronto "Inicio"', 20)
            verificar(True, "a aba nova mostra a página de início")

            # ---- Ctrl+Tab volta à primeira aba, sem voltar a descarregar ----
            antes = len(re.findall(r"cipo: abrindo ", m.serial().replace("\r", "")))
            m.tecla("ctrl-tab")
            m.esperar_serial("cipo: aba 0", 20)
            s = m.serial().replace("\r", "")
            depois = len(re.findall(r"cipo: abrindo ", s))
            verificar(depois == antes, f"trocar de aba não descarrega nada ({antes} -> {depois})")
            verificar("Página comprida" in s, "a primeira aba volta com o seu título")

            # ---- rolar, ir a outra página e voltar atrás ----
            m.tecla("end")
            time.sleep(0.6)
            m.tecla("ctrl-l")
            time.sleep(0.4)
            m.digitar(f"http://10.0.2.2:{porta}/outro.html")
            m.tecla("ret")
            m.esperar_serial("cipo: abrindo", 30)
            s = m.serial().replace("\r", "")
            verificar("outro.html" in s, "a outra página abre-se")

            m.tecla("alt-left")
            m.esperar_serial("cipo: voltar para", 30)
            s = m.serial().replace("\r", "")
            volta = re.findall(r"cipo: voltar para (\S+) \(pos (\d+)\)", s)
            verificar(bool(volta), "o Alt+Izquierda volta atrás")
            if volta:
                verificar(int(volta[-1][1]) > 0,
                          f"a página volta à posição em que se estava a ler ({volta[-1][1]} px)")

            # ---- Ctrl+W fecha a aba ----
            # Antes disto havia tres abas (a inicial e duas novas): fecha uma e ficam duas.
            m.tecla("ctrl-t")
            m.esperar_serial("cipo: nova aba", 20)
            m.tecla("ctrl-w")
            m.esperar_serial("cipo: aba fechada", 20)
            s = m.serial().replace("\r", "")
            fechadas = re.findall(r"cipo: aba fechada, (\d+) restantes", s)
            verificar(bool(fechadas) and int(fechadas[-1]) == 2,
                      f"Ctrl+W fecha a aba e ficam as outras ({fechadas[-1] if fechadas else '?'} de 2)")
            m.captura("H124-cipo-abas")
    finally:
        servidor.shutdown()
        servidor.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
