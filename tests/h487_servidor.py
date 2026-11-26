#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Servidor de pruebas de la red del Cipó (D487, h487): cookies, redirecciones, POST, gzip, caché
con 304, CORS y un WebSocket de eco. Dos instancias (dos puertos) dan dos orígenes distintos."""
import base64
import gzip
import hashlib
import http.server
import json
import struct
import threading
import urllib.parse
import zlib

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


def brotli_sin_comprimir(datos: bytes) -> bytes:
    """Un flujo Brotli válido con un meta-bloque sin comprimir (suficiente para probar el decodificador)."""
    assert 0 < len(datos) <= 65536
    bits = []

    def poner(valor, n):
        for i in range(n):
            bits.append((valor >> i) & 1)

    poner(0, 1)                 # WBITS = 16
    poner(0, 1)                 # ISLAST = 0
    poner(0, 2)                 # MNIBBLES = 4
    poner(len(datos) - 1, 16)   # MLEN - 1
    poner(1, 1)                 # ISUNCOMPRESSED
    while len(bits) % 8:
        bits.append(0)
    cab = bytearray()
    for i in range(0, len(bits), 8):
        cab.append(sum(b << k for k, b in enumerate(bits[i:i + 8])))
    return bytes(cab) + datos + bytes([0x03])   # y un meta-bloque final vacío (ISLAST=1, ISLASTEMPTY=1)


class Estado:
    def __init__(self):
        self.lock = threading.Lock()
        self.pedidos = []           # (metodo, ruta, cabeceras dict en minúsculas, cuerpo)
        self.contadores = {}
        self.otro_puerto = 0

    def anotar(self, metodo, ruta, cab, cuerpo):
        with self.lock:
            self.pedidos.append((metodo, ruta, cab, cuerpo))

    def contar(self, clave):
        with self.lock:
            self.contadores[clave] = self.contadores.get(clave, 0) + 1
            return self.contadores[clave]


def crear(estado: Estado, puerto_otro=lambda: 0):
    class Tratador(http.server.BaseHTTPRequestHandler):
        protocol_version = "HTTP/1.1"

        def log_message(self, *a):
            pass

        # ---- utilidades ----
        def leer_cuerpo(self):
            n = int(self.headers.get("Content-Length") or 0)
            return self.rfile.read(n) if n else b""

        def responder(self, codigo, cuerpo=b"", tipo="text/html; charset=utf-8", extra=(), cors=True):
            if isinstance(cuerpo, str):
                cuerpo = cuerpo.encode("utf-8")
            self.send_response(codigo)
            self.send_header("Content-Type", tipo)
            self.send_header("Content-Length", str(len(cuerpo)))
            self.send_header("Connection", "close")
            for k, v in extra:
                self.send_header(k, v)
            self.end_headers()
            if self.command != "HEAD":
                self.wfile.write(cuerpo)
            self.close_connection = True

        def cab(self):
            return {k.lower(): v for k, v in self.headers.items()}

        def procesar(self):
            cuerpo = self.leer_cuerpo()
            url = urllib.parse.urlsplit(self.path)
            ruta, q = url.path, urllib.parse.parse_qs(url.query)
            cab = self.cab()
            estado.anotar(self.command, self.path, cab, cuerpo)
            m = self.command
            if "upgrade" in cab.get("connection", "").lower() and ruta == "/ws":
                return self.websocket(cab)
            if ruta.startswith("/p/"):                       # las páginas de prueba
                return self.responder(200, PAGINAS[ruta[3:]].replace("__OTRO__", str(puerto_otro())))
            if ruta == "/set":
                extra = [("Set-Cookie", c) for c in (
                    "a=1; Path=/", "b=2; Path=/sub", "h=secreto; Path=/; HttpOnly", "c=3; Path=/", "c=3; Path=/; Max-Age=0",
                    "d=4; Path=/; Domain=otro.com", "e=5; Path=/; Secure", "f=6; Path=/; SameSite=Strict",
                    "g=7; Path=/; Max-Age=3600", "caduca=1; Path=/; Expires=Thu, 01 Jan 1970 00:00:01 GMT",
                    "ses=1; Path=/",
                    "l=lax; Path=/; SameSite=Lax",
                    "__Host-x=1; Path=/", "dom=1; Path=/; Domain=10.0.2.2")]
                return self.responder(200, "ok", "text/plain", extra)
            if ruta in ("/eco", "/sub/eco"):
                datos = {"metodo": m, "ruta": self.path, "cab": cab, "cuerpo": cuerpo.decode("latin-1")}
                extra = [("Access-Control-Allow-Origin", cab.get("origin", "*")), ("Access-Control-Allow-Credentials", "true"),
                         ("X-Visible", "si"), ("X-Oculta", "no"), ("Access-Control-Expose-Headers", "X-Visible")]
                return self.responder(200, json.dumps(datos), "application/json", extra)
            if ruta.startswith("/redir/"):
                codigo = int(ruta.split("/")[2])
                return self.responder(codigo, "", "text/plain", [("Location", q.get("to", ["/eco"])[0])])
            if ruta == "/gz":
                return self.responder(200, gzip.compress(b"hola gzip " * 50), "text/plain", [("Content-Encoding", "gzip")])
            if ruta == "/deflate":
                return self.responder(200, zlib.compress(b"hola deflate " * 50), "text/plain", [("Content-Encoding", "deflate")])
            if ruta == "/deflate-crudo":
                c = zlib.compressobj(9, zlib.DEFLATED, -15)
                return self.responder(200, c.compress(b"hola crudo " * 50) + c.flush(), "text/plain", [("Content-Encoding", "deflate")])
            if ruta == "/br":
                return self.responder(200, brotli_sin_comprimir(b"hola brotli " * 20), "text/plain", [("Content-Encoding", "br")])
            if ruta == "/cache":                             # ETag y 304
                n = estado.contar("cache-" + ("304" if cab.get("if-none-match") == '"v1"' else "200"))
                if cab.get("if-none-match") == '"v1"':
                    return self.responder(304, "", "text/plain", [("ETag", '"v1"'), ("Cache-Control", "max-age=0")])
                return self.responder(200, "contenido-cache", "text/plain", [("ETag", '"v1"'), ("Cache-Control", "max-age=0")])
            if ruta == "/fresco":                            # fresca 60 s: la segunda vez no llega
                estado.contar("fresco")
                return self.responder(200, "contenido-fresco", "text/plain", [("Cache-Control", "max-age=60")])
            if ruta == "/lm":                                # Last-Modified / If-Modified-Since
                if cab.get("if-modified-since") == "Wed, 21 Oct 2015 07:28:00 GMT":
                    estado.contar("lm-304")
                    return self.responder(304, "", "text/plain", [("Last-Modified", "Wed, 21 Oct 2015 07:28:00 GMT"), ("Cache-Control", "no-cache")])
                estado.contar("lm-200")
                return self.responder(200, "contenido-lm", "text/plain", [("Last-Modified", "Wed, 21 Oct 2015 07:28:00 GMT"), ("Cache-Control", "no-cache")])
            if ruta == "/sincache":
                estado.contar("sincache")
                return self.responder(200, "x", "text/plain", [("Cache-Control", "no-store"), ("ETag", '"zz"')])
            if ruta == "/stats":
                with estado.lock:
                    return self.responder(200, json.dumps(estado.contadores), "application/json", [("Access-Control-Allow-Origin", "*")])
            if ruta == "/cors-ok":
                return self.responder(200, "cors-ok", "text/plain", [("Access-Control-Allow-Origin", cab.get("origin", "*")), ("Access-Control-Allow-Credentials", "true")])
            if ruta == "/cors-no":
                return self.responder(200, "cors-no", "text/plain")
            if ruta == "/cors-pre":
                if m == "OPTIONS":
                    return self.responder(204, "", "text/plain", [("Access-Control-Allow-Origin", cab.get("origin", "*")), ("Access-Control-Allow-Methods", "GET, PUT, POST"),
                                                                    ("Access-Control-Allow-Headers", cab.get("access-control-request-headers", ""))])
                return self.responder(200, json.dumps({"metodo": m, "x": cab.get("x-prueba", "")}), "application/json", [("Access-Control-Allow-Origin", cab.get("origin", "*"))])
            if ruta == "/cors-pre-no":
                if m == "OPTIONS":
                    return self.responder(204, "", "text/plain")
                return self.responder(200, "no deberia llegar", "text/plain", [("Access-Control-Allow-Origin", "*")])
            if ruta == "/form":                              # formulario del navegador (HTML)
                partes = cuerpo.decode("latin-1")
                return self.responder(200, "<title>POST OK</title><h1>Recibido</h1><pre>%s|%s|%s</pre>" % (
                    m, cab.get("content-type", ""), partes.replace("<", "&lt;")))
            if ruta == "/login":                             # POST -> 303 con cookie -> /eco
                return self.responder(303, "", "text/plain", [("Set-Cookie", "sesion=abc123; Path=/"), ("Location", "/eco")])
            if ruta == "/larga":
                return self.responder(200, "x" * 200000, "text/plain")
            if ruta == "/vacio":
                return self.responder(204, "", "text/plain")
            return self.responder(404, "no existe", "text/plain")

        do_GET = do_POST = do_PUT = do_DELETE = do_PATCH = do_HEAD = do_OPTIONS = procesar

        # ---- WebSocket de eco ----
        def websocket(self, cab):
            clave = cab.get("sec-websocket-key", "")
            acc = base64.b64encode(hashlib.sha1((clave + GUID).encode()).digest()).decode()
            self.wfile.write(("HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n"
                              "Sec-WebSocket-Accept: %s\r\n\r\n" % acc).encode())
            self.wfile.flush()
            estado.contar("ws")
            self.trama(1, ("hola desde el servidor; cookie=" + cab.get("cookie", "")).encode())
            while True:
                t = self.leer_trama()
                if t is None:
                    break
                op, datos = t
                if op == 8:
                    self.trama(8, datos[:2])
                    break
                if op == 9:
                    self.trama(10, datos)
                elif op == 1:
                    self.trama(1, b"eco:" + datos)
                elif op == 2:
                    self.trama(2, datos[::-1])
            self.close_connection = True

        def trama(self, op, datos):
            cab = bytes([0x80 | op])
            n = len(datos)
            cab += bytes([n]) if n < 126 else b"\x7e" + struct.pack(">H", n) if n < 65536 else b"\x7f" + struct.pack(">Q", n)
            self.wfile.write(cab + datos)
            self.wfile.flush()

        def leer_n(self, n):
            b = b""
            while len(b) < n:
                x = self.rfile.read(n - len(b))
                if not x:
                    return None
                b += x
            return b

        def leer_trama(self):
            try:
                h = self.leer_n(2)
                if h is None:
                    return None
                op, n = h[0] & 15, h[1] & 0x7F
                if n == 126:
                    n = struct.unpack(">H", self.leer_n(2))[0]
                elif n == 127:
                    n = struct.unpack(">Q", self.leer_n(8))[0]
                mk = self.leer_n(4) if h[1] & 0x80 else None
                d = self.leer_n(n) if n else b""
                if mk:
                    d = bytes(c ^ mk[i % 4] for i, c in enumerate(d))
                return op, d
            except OSError:
                return None

    return Tratador


# Páginas de prueba (las sirve /p/<nombre>); __OTRO__ = el puerto del segundo servidor
PAGINAS = {}
