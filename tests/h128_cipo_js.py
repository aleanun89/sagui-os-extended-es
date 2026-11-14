#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H128 - Cipó: as páginas correm JavaScript (D149).

Critério:
  - o <script> da página corre com o QuickJS (bellard/quickjs, MIT);
  - o console.log do script aparece no registo do sistema;
  - o document.title posto pelo script muda o título da página;
  - o document.body.innerHTML posto pelo script troca o corpo da página;
  - o setTimeout do script corre mais tarde (o temporizador dispara);
  - o navegador continua vivo depois disto (a página continua a pintar).
"""
import http.server
import os
import re
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

HTML = """<!doctype html><html><head><title>Pagina com script</title></head>
<body><h1>Antes do script</h1>
<script>
  console.log('ola do script');
  document.title = 'Titulo do script';
  document.body.innerHTML = '<h1>Corpo novo</h1>';
  setTimeout(function(){ console.log('temporizador disparou'); }, 50);
</script>
</body></html>"""


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        cuerpo = HTML.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(cuerpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(cuerpo)
        self.close_connection = True


def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    imagem = imagem_de_teste(f"/bin/cipo http://10.0.2.2:{porta}/", ["--vesa"])
    try:
        with Maquina("h128", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("cipo: pronto", 90)
            m.esperar_serial("temporizador disparou", 60)
            s = m.serial().replace("\r", "")

            verificar("cipo: js: 1 script(s) correram" in s,
                      "o <script> da página correu")
            verificar("cipo: js: ola do script" in s,
                      "o console.log do script aparece no registo")
            verificar("cipo: js: titulo = Titulo do script" in s,
                      "o document.title do script mudou o título")
            verificar("cipo: js: o corpo foi trocado" in s,
                      "o document.body.innerHTML do script trocou o corpo")
            verificar("cipo: js: temporizador disparou" in s,
                      "o setTimeout do script disparou")
            verificar("Fallo de página" not in s,
                      "o navegador não caiu com o script")
            m.captura("H128-cipo-js")
    finally:
        servidor.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
