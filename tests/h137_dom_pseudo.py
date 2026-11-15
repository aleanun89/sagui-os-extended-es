#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H137 - o DOM nao se parte quando a pagina tem ::before e ::after (D161).

Contexto:
  - o motor cria nos gerados (::before, ::after e os blocos anonimos) que
    nao sao elementos do DOM; esses nos nao podem repetir o identificador de
    outro no, senao o ponte do QuickJS monta uma arvore com um ciclo e o
    navegador morre com um fallo de pagina (D161);
  - o conteudo de um ::before nao aparece no DOM, como num navegador a serio.

Criterio:
  - a pagina com pseudo-elementos carrega e fica pintada;
  - o script ve a arvore dos elementos sem PseudoElementos a mais;
  - o navegador continua vivo (sem fallo de pagina).
"""
import http.server
import os
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

PAGINA = """<!doctype html><html><head><title>Pagina com pseudo</title>
<style>
p::before { content: "pre "; color: #a00000; }
p::after { content: " pos"; color: #0000a0; }
div span { font-weight: bold; }
</style></head>
<body>
<h1 id="titulo">Titulo</h1>
<p id="p1">Texto</p>
<p id="p2">Outro texto</p>
<div id="caixa">texto <span>forte</span> mais texto</div>
<script>
  var ps = document.getElementsByTagName('p');
  console.log('pseudo ps ' + ps.length);
  console.log('pseudo texto ' + ps[0].textContent);
  console.log('pseudo pai ' + document.getElementById('p1').parentElement.tagName);
  console.log('pseudo caixa ' + document.getElementById('caixa').textContent);
  console.log('pseudo pronto');
</script>
</body></html>"""


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        corpo = PAGINA.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
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
        with Maquina("h137", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial('cipo: pronto "Pagina com pseudo"', 90)
            s = m.serial().replace("\r", "")

            # ---------------------------------------- o script le a arvore
            esperado = [
                "pseudo ps 2",
                "pseudo texto Texto",
                "pseudo pai BODY",
                "pseudo pronto",
            ]
            for texto in esperado:
                verificar(f"cipo: js: {texto}" in s,
                          f"o script leu «{texto}»")
            # o conteudo do ::before nao entra no DOM (como num navegador)
            verificar("cipo: js: pseudo texto pre " not in s,
                      "o ::before nao entrou no DOM")
            verificar("qjs: erro" not in s, "o script nao deu erro")

            # ---------------------------------------- a pagina ficou pintada
            limite = time.time() + 40
            while time.time() < limite:
                if m.serial().count("cipo: pagina pintada") >= 1:
                    break
                time.sleep(0.2)
            s = m.serial().replace("\r", "")
            verificar("cipo: pagina pintada" in s, "a pagina ficou pintada")

            # ---------------------------------------- o navegador continua vivo
            verificar("Fallo de página" not in s, "o navegador nao se caiu")
            verificar("finalizado" not in s, "nenhum processo terminou com erro")
    finally:
        servidor.shutdown()
        servidor.server_close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
