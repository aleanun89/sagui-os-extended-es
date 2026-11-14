#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""servidor_falso_ia.py - um servidor falso compatível com a API da OpenAI.

Serve para as provas do cliente de IA (D134) sem Internet, sem modelo real e
sem chaves: é um HTTP pequeno que responde como um llama.cpp server, um
Ollama ou um OpenRouter:

  GET  /v1/models             -> {"object":"list","data":[{"id":"..."}]}
  POST /v1/chat/completions   -> a resposta em streaming (SSE) ou inteira,
                                  conforme o "stream" do pedido

Casos especiais que a prova usa:
  - a pergunta "LENTO" -> os pedaços chegam com 0,4 s de intervalo, para
    poder cortar a resposta a meio (botão "Detener");
  - a pergunta "CORTA" -> o servidor deixa de mandar pedaços e fecha a
    ligação a meio da resposta;
  - a chave "chave-de-prova" no cabeçalho "Authorization" -> 401 se não
    vier ou vier errada;
  - "429" ou "500" na pergunta -> o servidor devolve esse erro.

O servidor escuta em 127.0.0.1 numa porta livre (a que se pedir) e não
escribe nada no disco.
"""
import http.server
import json
import os
import subprocess
import sys
import tempfile
import threading
import time

# A chave que o servidor exige (vazia = nao exige nenhuma). E global para
# que o tratamento de cada pedido a veja.
CHAVE = ""

# Os pedidos que chegaram, um por linha: as provas veem aqui o que o
# cliente mandou (as mensagens, o modelo, o systema).
PEDIDOS = []

# Respostas a medida (D240): lista de (marca, texto). Se a ultima mensagem do
# utilizador contem a marca, o servidor responde com esse texto (em pedacos de 40
# letras) em vez da frase de exemplo. A prova das diferencas do Mico a usa para
# devolver um bloco de codigo.
RESPOSTAS_MARCA = []

# Os GET /v1/models que chegaram (as provas da pagina da IA veem aqui).
PEDIDOS_MODELOS = []

# Os pedaços da resposta de exemplo. O texto e' uma frase simples para que
# a prova compare sem depender da codificacao.
PEDACOS = [
    "Hola, ",
    "isto e' ",
    "uma resposta ",
    "de exemplo ",
    "do servidor falso.",
]


# D239: la respuesta en Markdown (la pregunta "MARKDOWN"), cortada en pedazos que
# parten las marcas (la negrita, la cerca del bloque de código) a propósito, para
# probar que el chat la formatea bien mientras llega.
CODIGO_MD = 'int main(void) {\n    printf("hola\\n");\n    return 0;\n}'
PEDACOS_MD = [
    "## Resumen\n\nEsto es **negr", "ita**, esto *cursiva* y esto `codigo",
    "` en línea.\n\n- primer elemento\n- segundo ", "elemento\n\n",
    "```c\nint main(void) {\n    printf(\"hola\\n\");\n", "    return 0;\n}\n``", "`\n",
]


class Pedido(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def responder(self, codigo, corpo, tipo="application/json"):
        dados = corpo.encode("utf-8") if isinstance(corpo, str) else corpo
        self.send_response(codigo)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(dados)))
        self.send_header("Connection", "close")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(dados)
        self.close_connection = True

    def do_GET(self):
        if self.path.startswith("/v1/models"):
            PEDIDOS_MODELOS.append(self.path)
            self.responder(200, json.dumps({
                "object": "list",
                "data": [{"id": "modelo-de-prova", "object": "model"},
                         {"id": "modelo-pequeno", "object": "model"}],
            }))
        else:
            self.responder(404, json.dumps({"error": {"message": "nao existe"}}))

    def do_POST(self):
        if not self.path.startswith("/v1/chat/completions"):
            self.responder(404, json.dumps({"error": {"message": "nao existe"}}))
            return
        n = int(self.headers.get("Content-Length") or 0)
        bruto = self.rfile.read(n).decode("utf-8", "replace")
        try:
            pedido = json.loads(bruto)
        except ValueError:
            self.responder(400, json.dumps({"error": {"message": "JSON invalido"}}))
            return
        PEDIDOS.append(pedido)
        # A chave: se o servidor foi arrancado com uma, tem de vir bem.
        esperada = CHAVE
        if esperada:
            auth = self.headers.get("Authorization") or ""
            if auth != "Bearer " + esperada:
                self.responder(401, json.dumps({"error": {"message": "chave errada"}}))
                return
        texto = ""
        for m in pedido.get("messages", []):
            if m.get("role") == "user":
                texto = m.get("content", "")
        # Casos de erro a pedido.
        if "429" in texto:
            self.responder(429, json.dumps({"error": {"message": "demasiados pedidos"}}))
            return
        if "500" in texto:
            self.responder(500, json.dumps({"error": {"message": "erro do servidor"}}))
            return
        pedacos = PEDACOS
        for marca, resposta in RESPOSTAS_MARCA:
            if marca in texto:
                pedacos = [resposta[i:i + 40] for i in range(0, len(resposta), 40)]
                break
        if "MARKDOWN" in texto:
            pedacos = PEDACOS_MD   # D239: Markdown cortado a propósito en mitad de las marcas
        if not pedido.get("stream"):
            corpo = {"id": "x", "object": "chat.completion", "model": pedido.get("model", "prova"),
                     "choices": [{"index": 0, "message": {"role": "assistant", "content": "".join(pedacos)},
                                  "finish_reason": "stop"}]}
            self.responder(200, json.dumps(corpo))
            return
        # Streaming: SSE, um "data: {...}" por pedaco, e "[DONE]" no fim.
        self.send_response(200)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "close")
        self.end_headers()
        lento = "LENTO" in texto
        for i, pedaco in enumerate(pedacos):
            if "CORTA" in texto and i == 2:
                # Fecha a ligacao a meio: o cliente tem de avisar que a
                # resposta ficou incompleta.
                try:
                    self.wfile.flush()
                    self.connection.close()
                except OSError:
                    pass
                self.close_connection = True
                return
            fragmento = {"id": "x", "object": "chat.completion.chunk",
                         "model": pedido.get("model", "prova"),
                         "choices": [{"index": 0, "delta": {"content": pedaco}, "finish_reason": None}]}
            try:
                self.wfile.write(("data: " + json.dumps(fragmento) + "\n\n").encode("utf-8"))
                self.wfile.flush()
            except OSError:
                return   # o cliente cortou a ligacao ("Detener")
            if lento:
                time.sleep(0.4)
        fim = {"id": "x", "object": "chat.completion.chunk", "model": pedido.get("model", "prova"),
               "choices": [{"index": 0, "delta": {}, "finish_reason": "stop"}]}
        try:
            self.wfile.write(("data: " + json.dumps(fim) + "\n\n").encode("utf-8"))
            self.wfile.write(b"data: [DONE]\n\n")
            self.wfile.flush()
        except OSError:
            pass
        self.close_connection = True


class servidor_falso_ia:
    """Servidor falso da API de IA: guarda o httpd."""

    def __init__(self, chave="", tls=False):
        global CHAVE
        CHAVE = chave
        self.httpd = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Pedido)
        self.httpd.daemon_threads = True
        if tls:
            # Um certificado autoassinado, so para a prova: o cliente usa o
            # "-k" (inseguro) com ele. Nao e' um certificado de verdade.
            self.httpd.socket = self._com_tls(self.httpd.socket)
        self.hilo = threading.Thread(target=self.httpd.serve_forever, daemon=True)

    @staticmethod
    def _com_tls(sock):
        import ssl
        contexto = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        contexto.load_cert_chain(certificado_prova())
        return contexto.wrap_socket(sock, server_side=True)

    @property
    def porta(self):
        return self.httpd.server_address[1]

    def arrancar(self):
        self.hilo.start()
        return self

    def parar(self):
        self.httpd.shutdown()
        self.httpd.server_close()


def certificado_prova():
    """Um certificado autoassinado para a prova do HTTPS (fica em /tmp).

    O cliente fala com ele em modo inseguro ("-k"): so se prova que o
    caminho do HTTPS funciona, nao que a cadeia de certificados valida.
    """
    caminho = os.path.join(tempfile.gettempdir(), "sagui-ia-prova.pem")
    if not os.path.exists(caminho):
        subprocess.run(["openssl", "req", "-x509", "-newkey", "rsa:2048", "-nodes",
                        "-keyout", caminho, "-out", caminho, "-days", "2",
                        "-subj", "/CN=localhost"],
                       check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return caminho


def principal():
    chave = sys.argv[1] if len(sys.argv) > 1 else ""
    s = servidor_falso_ia(chave).arrancar()
    print("servidor falso da IA em http://127.0.0.1:%d/v1" % s.porta, flush=True)
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        s.parar()


if __name__ == "__main__":
    principal()
