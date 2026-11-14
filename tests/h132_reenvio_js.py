#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H132 - o Cipó segue o reenvio de uma página (D156).

Critério:
  - uma página que reenvia com JavaScript (como a do DuckDuckGo, que faz
    window.parent.location.replace("...")) leva o navegador à página de
    destino, e não fica em branco;
  - uma página que reenvia com <meta http-equiv="refresh"> leva o
    navegador à página de destino;
  - o endereço pedido pelo script é resolvido contra a página actual
    (um endereço relativo vira absoluto);
  - o navegador continua vivo depois do reenvio.
"""
import http.server
import os
import re
import sys
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

# A página de reenvio do DuckDuckGo: um script que troca a página da janela.
REENVIO = ("<html><head><meta name='referrer' content='origin'></head><body>"
           "<script language='JavaScript'>"
           "window.parent.location.replace('/destino.html');"
           "</script>"
           "<noscript><META http-equiv='refresh' content=\"0;URL=/destino.html\"></noscript>"
           "</body></html>")

# A mesma coisa, mas só com a etiqueta meta (sem JavaScript).
REENVIO_META = ("<html><head><title>Reenvio</title>"
                "<meta http-equiv='refresh' content='0;url=/destino.html'>"
                "</head><body>Reenvio imediato</body></html>")

DESTINO = ("<!doctype html><html><head><title>Destino</title></head>"
           "<body><h1>Pagina de destino</h1><p>Chegou aqui</p></body></html>")


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *args):
        pass

    def do_GET(self):
        if self.path.startswith("/destino"):
            corpo = DESTINO.encode("utf-8")
        elif self.path.startswith("/meta"):
            corpo = REENVIO_META.encode("utf-8")
        else:
            corpo = REENVIO.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(corpo)
        self.close_connection = True


def rodar(caminho, nome, extras):
    """Arranca o Cipó numa página e devolve o que saiu no registo."""
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    porta = servidor.server_address[1]
    imagem = imagem_de_teste(f"/bin/cipo http://10.0.2.2:{porta}{caminho}", ["--vesa"])
    try:
        with Maquina(nome, "ide", imagem=imagem, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("cipo: pronto \"Destino\"", 90)
            s = m.serial().replace("\r", "")
            m.captura(nome + "-destino", guardar=False)
            return s
    finally:
        servidor.shutdown()


def main():
    # ------------------------------------------------ o reenvio por JavaScript
    s = rodar("/redir", "h132-js", [])
    print("   pedido:", re.findall(r"cipo: pedido para ir a (\S+)", s))
    print("   navegou:", re.findall(r"cipo: navega para (\S+)", s))
    verificar("cipo: pedido para ir a http://10.0.2.2:" in s,
              "o script pediu a página de destino")
    verificar(re.search(r"cipo: pedido para ir a http://10\.0\.2\.2:\d+/destino\.html", s)
              is not None, "o endereço relativo do script ficou absoluto")
    verificar("cipo: navega para" in s, "o navegador navegou para o destino")
    verificar('cipo: pronto "Destino"' in s, "a página de destino foi desenhada")
    verificar("Fallo de página" not in s, "o navegador não caiu com o reenvio")
    verificar("qjs: erro" not in s, "o script do reenvio não deu erro")

    # ------------------------------------------------ o reenvio por meta
    s = rodar("/meta", "h132-meta", [])
    print("   meta:", re.findall(r"cipo: meta refresh para (\S+)", s))
    verificar("cipo: meta refresh para" in s, "o meta refresh foi visto")
    verificar(re.search(r"cipo: pedido para ir a http://10\.0\.2\.2:\d+/destino\.html", s)
              is not None, "o meta refresh aponta para o destino")
    verificar("cipo: navega para" in s, "o navegador navegou para o destino (meta)")
    verificar('cipo: pronto "Destino"' in s, "a página de destino foi desenhada (meta)")
    verificar("Fallo de página" not in s, "o navegador não caiu com o meta refresh")
    return 0


if __name__ == "__main__":
    sys.exit(main())
