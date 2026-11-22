#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H62 - MicroPython en red: socket, ssl y requests (mejora 032, D069).

El anfitrión levanta, en 127.0.0.1 (10.0.2.2 visto desde la VM), un eco TCP,
un eco UDP y un servidor HTTP/HTTPS con JSON, redirecciones, respuesta
chunked y una página lenta. La CA de prueba se genera en el momento (como en
H59). No hace falta internet.

Criterio:
  1. socket TCP cliente contra el eco del anfitrión (sendall, recv, makefile,
     with), y TCP servidor: Python escucha (bind/listen/accept, select) y el
     anfitrión se conecta por hostfwd.
  2. UDP (sendto/recvfrom) con el eco del anfitrión; recvfrom con plazo da
     socket.timeout; recv no bloqueante da EAGAIN.
  3. getaddrinfo/gethostbyname con una IP; un nombre que no existe da
     socket.gaierror; un nombre real si hay internet (si no, aviso).
  4. requests: get con params y headers, json(), post con json y con form,
     put, delete, head, redirección (302 y 303), chunked, 404 con
     raise_for_status, timeout (requests.Timeout) y un cuerpo de 200 KB
     comparado por SHA-256.
  5. HTTPS: sin la CA, requests y ssl.wrap_socket fallan con el texto de la
     libtls ("emisor desconocido"); con verify="ca.pem", con cadata o con
     cert_reqs=CERT_NONE funcionan; un nombre que no coincide falla y
     check_hostname=False lo acepta; tras "certificados añadir ca.pem"
     funciona sin más opciones.
"""
import datetime
import hashlib
import http.server
import ipaddress
import json
import os
import random
import shutil
import socket
import ssl
import subprocess
import sys
import threading
import time
import urllib.parse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import opcoes_apps, Maquina, verificar, RAIZ, SAIDA  # noqa: E402

from cryptography import x509  # noqa: E402
from cryptography.hazmat.primitives import hashes, serialization  # noqa: E402
from cryptography.hazmat.primitives.asymmetric import rsa  # noqa: E402
from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID  # noqa: E402

PASTA = os.path.join(SAIDA, "h62-pki")
random.seed(62)
GRANDE = bytes(random.getrandbits(8) for _ in range(200 * 1024))
PORTA_VM = 8080


def puerto_libre(tipo=socket.SOCK_STREAM):
    s = socket.socket(socket.AF_INET, tipo)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


# ------------------------------------------------------------ certificados

def nome(cn):
    return x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, cn), x509.NameAttribute(NameOID.ORGANIZATION_NAME, "SaguiOS pruebas")])


def gerar_pki():
    shutil.rmtree(PASTA, ignore_errors=True)
    os.makedirs(PASTA)
    agora = datetime.datetime.now(datetime.timezone.utc)
    ca_k = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    ca = (x509.CertificateBuilder().subject_name(nome("CA de prueba H62")).issuer_name(nome("CA de prueba H62"))
          .public_key(ca_k.public_key()).serial_number(x509.random_serial_number())
          .not_valid_before(agora - datetime.timedelta(days=1)).not_valid_after(agora + datetime.timedelta(days=30))
          .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
          .add_extension(x509.KeyUsage(digital_signature=False, content_commitment=False, key_encipherment=False,
                                       data_encipherment=False, key_agreement=False, key_cert_sign=True, crl_sign=True,
                                       encipher_only=False, decipher_only=False), critical=True)
          .sign(ca_k, hashes.SHA256()))
    k = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    c = (x509.CertificateBuilder().subject_name(nome("servidor H62")).issuer_name(ca.subject)
         .public_key(k.public_key()).serial_number(x509.random_serial_number())
         .not_valid_before(agora - datetime.timedelta(days=1)).not_valid_after(agora + datetime.timedelta(days=30))
         .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
         .add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
         .add_extension(x509.SubjectAlternativeName([x509.DNSName("sagui.prueba"),
                                                     x509.IPAddress(ipaddress.ip_address("10.0.2.2"))]), critical=False)
         .sign(ca_k, hashes.SHA256()))
    with open(os.path.join(PASTA, "ca.pem"), "wb") as f:
        f.write(ca.public_bytes(serialization.Encoding.PEM))
    with open(os.path.join(PASTA, "servidor.pem"), "wb") as f:
        f.write(c.public_bytes(serialization.Encoding.PEM))
        f.write(k.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))


# ------------------------------------------------------------ servidores do anfitrião

def eco_tcp(porta):
    srv = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    srv.bind(("127.0.0.1", porta))
    srv.listen(5)

    def atender(c):
        with c:
            while True:
                d = c.recv(4096)
                if not d:
                    break
                c.sendall(d)

    def laco():
        while True:
            c, _ = srv.accept()
            threading.Thread(target=atender, args=(c,), daemon=True).start()
    threading.Thread(target=laco, daemon=True).start()


def eco_udp(porta):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", porta))

    def laco():
        while True:
            d, de = s.recvfrom(2048)
            s.sendto(b"eco:" + d, de)
    threading.Thread(target=laco, daemon=True).start()


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def responder(self, codigo, corpo, tipo="application/json; charset=utf-8", extra=()):
        if isinstance(corpo, (dict, list)):
            corpo = json.dumps(corpo, ensure_ascii=False).encode()
        self.send_response(codigo)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        for k, v in extra:
            self.send_header(k, v)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(corpo)
        self.close_connection = True

    def corpo(self):
        n = int(self.headers.get("Content-Length") or 0)
        return self.rfile.read(n) if n else b""

    def eco(self):
        u = urllib.parse.urlsplit(self.path)
        corpo = self.corpo()
        d = {"metodo": self.command, "caminho": u.path, "args": dict(urllib.parse.parse_qsl(u.query)),
             "tipo": self.headers.get("Content-Type", ""), "agente": self.headers.get("User-Agent", ""),
             "x-sagui": self.headers.get("X-Sagui", ""), "corpo": corpo.decode("utf-8", "replace")}
        if d["tipo"].startswith("application/json"):
            d["json"] = json.loads(corpo)
        if d["tipo"].startswith("application/x-www-form-urlencoded"):
            d["form"] = dict(urllib.parse.parse_qsl(corpo.decode()))
        self.responder(200, d)

    def do_GET(self):
        u = urllib.parse.urlsplit(self.path)
        if u.path == "/json":
            self.responder(200, {"nombre": "sagui", "lista": [1, 2, 3], "acento": "ñandú", "pi": 3.25})
        elif u.path == "/eco":
            self.eco()
        elif u.path == "/redir":
            self.responder(302, b"", "text/plain", [("Location", "/json")])
        elif u.path == "/chunked":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain; charset=utf-8")
            self.send_header("Transfer-Encoding", "chunked")
            self.send_header("Connection", "close")
            self.end_headers()
            for parte in ("primera parte, ", "segunda parte, ", "fin: ñ"):
                b = parte.encode()
                self.wfile.write(b"%x\r\n%s\r\n" % (len(b), b))
            self.wfile.write(b"0\r\n\r\n")
            self.close_connection = True
        elif u.path == "/lento":
            time.sleep(6)
            self.responder(200, b"tarde", "text/plain")
        elif u.path == "/grande":
            self.responder(200, GRANDE, "application/octet-stream")
        elif u.path == "/ca.pem":
            self.responder(200, open(os.path.join(PASTA, "ca.pem"), "rb").read(), "text/plain")
        else:
            self.responder(404, {"error": "no existe"})

    do_HEAD = do_GET

    def do_POST(self):
        if self.path == "/redir303":
            self.corpo()
            self.responder(303, b"", "text/plain", [("Location", "/eco?de=303")])
        else:
            self.eco()

    do_PUT = do_POST
    do_DELETE = do_POST


def servidor_http(cert=None):
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    srv.daemon_threads = True
    if cert:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(cert)
        srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv.server_address[1]


# ------------------------------------------------------------ scripts da VM

def scripts(p):
    """Os programas que rodam dentro do SaguiOS (vão para /t no ramdisk)."""
    eco, udp, http_, https = p["eco"], p["udp"], p["http"], p["https"]
    return {
        "tcp_cliente.py": f"""
import socket
s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.settimeout(10)
s.connect(("10.0.2.2", {eco}))
s.sendall("hola eco ñ\\n".encode())
print("eco1:", s.recv(100).decode())
f = s.makefile("rb")
s.sendall(b"linea 1\\nlinea 2\\n")
print("eco2:", f.readline(), f.readline())
print("peer:", s.getpeername()[1] == {eco})
s.close()
with socket.socket() as s2:
    s2.connect(socket.getaddrinfo("10.0.2.2", {eco})[0][-1])
    s2.send(b"x" * 3000)
    total = 0
    while total < 3000:
        total += len(s2.recv(4096))
    print("eco3:", total)
try:
    socket.socket().connect(("10.0.2.2", 1))
except OSError as e:
    print("recusada:", e.errno)
""",
        "tcp_servidor.py": f"""
import socket, select
srv = socket.socket()
srv.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
srv.bind(("", {PORTA_VM}))
srv.listen(1)
print("select vacio:", select.select([srv], [], [], 0.1)[0] == [])
print("escuchando", {PORTA_VM})
r, _, _ = select.select([srv], [], [], 30)
print("select listo:", r == [srv])
c, ender = srv.accept()
print("conexion de", ender[0])
linha = c.makefile("rb").readline()
c.sendall(linha.upper())
c.close()
srv.close()
print("servidor fin:", linha)
""",
        "udp.py": f"""
import socket, errno
u = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
u.settimeout(5)
u.sendto(b"ping udp", ("10.0.2.2", {udp}))
d, de = u.recvfrom(100)
print("udp:", d, de[1] == {udp})
u.settimeout(0.5)
try:
    u.recvfrom(100)
except socket.timeout as e:
    print("udp timeout:", isinstance(e, OSError), e.errno == errno.ETIMEDOUT)
u.setblocking(False)
try:
    u.recv(10)
except OSError as e:
    print("udp eagain:", e.errno == errno.EAGAIN)
u.close()
""",
        "dns.py": """
import socket
print("gai:", socket.getaddrinfo("10.0.2.2", 80)[0][-1])
print("byname:", socket.gethostbyname("10.0.2.2"))
try:
    socket.getaddrinfo("no-existe.invalid", 80)
except socket.gaierror as e:
    print("gaierror:", isinstance(e, OSError))
try:
    print("real:", socket.gethostbyname("example.com"))
except OSError as e:
    print("real: sin dns", e)
""",
        "http.py": f"""
import requests, hashlib, json
B = "http://10.0.2.2:{http_}"
r = requests.get(B + "/json", timeout=10)
print("get:", r.status_code, r.ok, r.headers["content-type"], r.json()["acento"], r.json()["lista"])
r = requests.get(B + "/eco", params={{"q": "sagui ñ", "n": 3}}, headers={{"X-Sagui": "tito"}})
d = r.json()
print("params:", d["args"]["q"], d["args"]["n"], d["x-sagui"], d["agente"].startswith("python-requests"))
r = requests.post(B + "/eco", json={{"a": [1, 2], "b": "ñ"}})
print("post json:", r.json()["metodo"], r.json()["json"]["a"], r.json()["json"]["b"], r.json()["tipo"])
r = requests.post(B + "/eco", data={{"campo": "valor 1", "otro": "ñ"}})
print("post form:", r.json()["form"]["campo"], r.json()["form"]["otro"])
print("put:", requests.put(B + "/eco", data="texto").json()["metodo"])
print("delete:", requests.delete(B + "/eco").json()["metodo"])
r = requests.head(B + "/json")
print("head:", r.status_code, r.content, int(r.headers["Content-Length"]) > 10)
r = requests.get(B + "/redir")
print("redir:", r.status_code, r.url.endswith("/json"), len(r.history), r.history[0].status_code)
r = requests.post(B + "/redir303", json={{"x": 1}})
print("redir303:", r.json()["metodo"], r.json()["args"])
r = requests.get(B + "/chunked")
print("chunked:", r.text)
r = requests.get(B + "/nada")
print("404:", r.status_code, r.ok)
try:
    r.raise_for_status()
except requests.HTTPError as e:
    print("raise:", "404" in str(e))
try:
    requests.get(B + "/lento", timeout=1.5)
except requests.Timeout as e:
    print("timeout:", type(e).__name__)
r = requests.get(B + "/grande")
print("grande:", len(r.content), hashlib.sha256(r.content).hexdigest())
""",
        "https.py": f"""
import requests, ssl, socket, sys
U = "https://10.0.2.2:{https}/json"
modo = sys.argv[1]
if modo == "sin":
    try:
        requests.get(U)
    except requests.exceptions.SSLError as e:
        print("sin ca:", e)
    s = socket.socket(); s.connect(("10.0.2.2", {https}))
    try:
        ssl.wrap_socket(s, server_hostname="10.0.2.2")
    except ssl.SSLError as e:
        print("wrap sin ca:", type(e).__name__, e)
    print("verify false:", requests.get(U, verify=False).json()["nombre"])
    print("verify ca:", requests.get(U, verify="/t/ca.pem").json()["nombre"])
    s = socket.socket(); s.connect(("10.0.2.2", {https}))
    t = ssl.wrap_socket(s, server_hostname="10.0.2.2", cadata=open("/t/ca.pem").read())
    t.write(b"GET /json HTTP/1.1\\r\\nHost: x\\r\\nConnection: close\\r\\n\\r\\n")
    print("cadata:", t.version(), t.readline())
    t.close()
    s = socket.socket(); s.connect(("10.0.2.2", {https}))
    t = ssl.wrap_socket(s, cert_reqs=ssl.CERT_NONE)
    print("cert_none:", t.version())
    t.close()
    ctx = ssl.create_default_context(cafile="/t/ca.pem")
    s = socket.socket(); s.connect(("10.0.2.2", {https}))
    try:
        ctx.wrap_socket(s, server_hostname="otro.ejemplo")
    except ssl.SSLCertVerificationError as e:
        print("nombre:", e)
    ctx.check_hostname = False
    s = socket.socket(); s.connect(("10.0.2.2", {https}))
    print("check_hostname false:", ctx.wrap_socket(s, server_hostname="otro.ejemplo").version())
else:
    r = requests.get(U)
    print("con ca:", r.status_code, r.json()["acento"])
""",
    }


def main():
    gerar_pki()
    p = {"eco": puerto_libre(), "udp": puerto_libre(socket.SOCK_DGRAM), "fwd": puerto_libre()}
    eco_tcp(p["eco"])
    eco_udp(p["udp"])
    p["http"] = servidor_http()
    p["https"] = servidor_http(os.path.join(PASTA, "servidor.pem"))

    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(SAIDA, "h62-rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    os.makedirs(os.path.join(raiz, "t"))
    for nomearq, texto in scripts(p).items():
        with open(os.path.join(raiz, "t", nomearq), "w", encoding="utf-8") as f:
            f.write(texto.lstrip())
    shutil.copy(os.path.join(PASTA, "ca.pem"), os.path.join(raiz, "t", "ca.pem"))
    sgfs = os.path.join(SAIDA, "h62.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, "h62.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", *opcoes_apps(), "--console"], check=True, stdout=subprocess.DEVNULL)

    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{p['fwd']}-:{PORTA_VM}"]
    with Maquina("h62", "ide", imagem=img, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)

        # 1. TCP cliente e servidor
        out = m.comando("python /t/tcp_cliente.py", tempo=90)
        verificar("eco1: hola eco ñ" in out, "TCP: connect, sendall y recv contra el eco del anfitrión (con ñ)")
        verificar("eco2: b'linea 1\\n' b'linea 2\\n'" in out, "TCP: makefile('rb').readline()")
        verificar("peer: True" in out and "eco3: 3000" in out, "TCP: getpeername, getaddrinfo y with socket()")
        verificar("recusada: 111" in out, "TCP: puerto cerrado = OSError ECONNREFUSED (111)")

        resposta = {}

        def cliente():
            fim = time.time() + 60
            while f"escuchando {PORTA_VM}" not in m.serial() and time.time() < fim:
                time.sleep(0.2)
            time.sleep(0.5)
            try:
                with socket.create_connection(("127.0.0.1", p["fwd"]), timeout=20) as c:
                    c.sendall(b"hola servidor sagui\n")
                    resposta["r"] = c.recv(100)
            except OSError as e:
                resposta["r"] = repr(e).encode()
        t = threading.Thread(target=cliente)
        t.start()
        out = m.comando("python /t/tcp_servidor.py", tempo=90)
        t.join(30)
        verificar("select vacio: True" in out and "select listo: True" in out, "select.select sobre el socket que escucha")
        verificar(resposta.get("r") == b"HOLA SERVIDOR SAGUI\n" and "conexion de" in out,
                  f"TCP servidor: Python acepta la conexión del anfitrión (hostfwd) y responde ({resposta.get('r')!r})")

        # 2. UDP
        out = m.comando("python /t/udp.py", tempo=60)
        verificar("udp: b'eco:ping udp' True" in out, "UDP: sendto y recvfrom con el eco del anfitrión")
        verificar("udp timeout: True True" in out, "UDP: recvfrom con plazo = socket.timeout (OSError ETIMEDOUT)")
        verificar("udp eagain: True" in out, "socket no bloqueante sin datos = EAGAIN")

        # 3. DNS
        out = m.comando("python /t/dns.py", tempo=60)
        verificar("gai: ('10.0.2.2', 80)" in out and "byname: 10.0.2.2" in out, "getaddrinfo y gethostbyname con una IP")
        verificar("gaierror: True" in out, "un nombre que no existe = socket.gaierror")
        if "real: sin dns" in out:
            print("  AVISO: sin DNS real (sin internet); se omite example.com")
        else:
            verificar("real: " in out, "gethostbyname('example.com') por el DNS del sistema")

        # 4. requests
        out = m.comando("python /t/http.py", tempo=120)
        verificar("get: 200 True application/json; charset=utf-8 ñandú [1, 2, 3]" in out, "requests.get: status, headers y json()")
        verificar("params: sagui ñ 3 tito True" in out, "requests.get con params y headers")
        verificar("post json: POST [1, 2] ñ application/json" in out, "requests.post con json=")
        verificar("post form: valor 1 ñ" in out, "requests.post con data= (formulario)")
        verificar("put: PUT" in out and "delete: DELETE" in out, "requests.put y requests.delete")
        verificar("head: 200 b'' True" in out, "requests.head: sin cuerpo, con cabeceras")
        verificar("redir: 200 True 1 302" in out, "redirección 302 seguida (history)")
        verificar("redir303: GET {'de': '303'}" in out, "303 tras un POST: sigue con GET")
        verificar("chunked: primera parte, segunda parte, fin: ñ" in out, "respuesta chunked")
        verificar("404: 404 False" in out and "raise: True" in out, "404: ok False y raise_for_status")
        verificar("timeout: ReadTimeout" in out, "timeout= da requests.Timeout")
        verificar(f"grande: {len(GRANDE)} {hashlib.sha256(GRANDE).hexdigest()}" in out, "200 KB por HTTP: SHA-256 igual")

        # 5. HTTPS
        out = m.comando("python /t/https.py sin", tempo=180)
        verificar("sin ca: " in out and "emisor desconocido" in out, "HTTPS sin la CA: requests da SSLError con el texto de la libtls")
        verificar("wrap sin ca: SSLCertVerificationError" in out, "ssl.wrap_socket sin la CA: SSLCertVerificationError")
        verificar("verify false: sagui" in out, "requests verify=False")
        verificar("verify ca: sagui" in out, "requests verify='/t/ca.pem'")
        verificar("cadata: TLSv1.3 b'HTTP/1.1 200 OK\\r\\n'" in out, "ssl.wrap_socket(cadata=...): TLS 1.3 y readline")
        verificar("cert_none: TLSv1." in out, "ssl.wrap_socket(cert_reqs=CERT_NONE)")
        verificar("nombre: " in out and "check_hostname false: TLSv1." in out,
                  "nombre que no coincide: error; check_hostname=False lo acepta")
        m.comando("certificados añadir /t/ca.pem", tempo=30)
        out = m.comando("python /t/https.py con", tempo=120)
        verificar("con ca: 200 ñandú" in out, "tras 'certificados añadir', requests.get por HTTPS sin opciones")
        m.captura("H62-python-red", guardar=False)
    return 0


if __name__ == "__main__":
    sys.exit(main())
