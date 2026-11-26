#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H485 - unidades de red WebDAV en /red/<nombre> (D485, docs/decisoes.md).

Partes (por defecto todas; o las que se pidan: h485_webdav.py pc saguios gui):
  pc       el servidor WebDAV de la prueba contra sí mismo (PROPFIND, GET con Range, PUT, MKCOL, MOVE, DELETE, Basic);
           D485 en decisoes.md, la ayuda y la fila del ROADMAP.
  saguios  consola con sesión root, /home en ext2 y una cuenta «ana». Un servidor WebDAV en el anfitrión (de esta
           prueba, en Python) por la red de usuario de QEMU (10.0.2.2), con usuario y contraseña:
           - sagui agrega la unidad (la contraseña por la entrada) y el fichero del llavero (0600) no la lleva en claro;
           - monta /red/nube: listar, leer, escribir (shell, cp y Python), carpetas, renombrar, borrar, carpeta no
             vacía, nombre con acento, fichero grande leído por Range;
           - un programa de la unidad NO se ejecuta (lo dice el kernel), aunque copiado fuera sí;
           - ana no ve la unidad (ni en /red ni en «dav estado») ni puede entrar en /red/nube;
           - se corta el servidor: «dav estado» dice desconectada y un «ls» falla al momento, sin colgarse;
             vuelve el servidor: conectada otra vez;
           - una segunda unidad por HTTPS con un certificado de prueba (su autoridad con --cacert) y, sin ella, no monta;
           - el llavero con contraseña propia: cerrado no da la contraseña y se abre con ella;
           - desmontar: /red/nube desaparece.
  gui      escritorio: una unidad montada desde el Terminal; Configuración > Unidades de red (lista con su estado,
           formulario y llavero) y Archivos dentro de /red/nube con la sección RED de la barra lateral.
Capturas: docs/capturas/H485-*.png (datos sintéticos de la prueba).
"""
import base64
import email.utils
import http.server
import os
import re
import shutil
import socket
import ssl
import subprocess
import sys
import threading
import time
import urllib.parse
from xml.sax.saxutils import escape

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, imagem_de_teste, verificar, SAIDA  # noqa: E402

TRAB = os.path.join(SAIDA, "h485")
USUARIO, SENHA = "ana-dav", "Secreto-Dav-4851"


# ------------------------------------------------------------------ el servidor WebDAV de la prueba

class Dav(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    raiz = None
    pedidos = []

    def log_message(self, *a):
        pass

    def _auth(self):
        if self.path.startswith("/publico/"):
            return True
        a = self.headers.get("Authorization", "")
        ok = a == "Basic " + base64.b64encode(f"{USUARIO}:{SENHA}".encode()).decode()
        if not ok:
            self._resp(401, b"", {"WWW-Authenticate": 'Basic realm="h485"'})
        return ok

    def _resp(self, cod, corpo=b"", cab=None):
        self.send_response(cod)
        for k, v in (cab or {}).items():
            self.send_header(k, v)
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(corpo)
        self.close_connection = True

    def _local(self, path=None):
        p = urllib.parse.urlparse(path or self.path).path
        if p.startswith("/publico/"):
            return os.path.join(self.raiz + "-publico", p[len("/publico/"):])
        if not p.startswith("/dav"):
            return None
        rel = urllib.parse.unquote(p[4:]).strip("/")
        if ".." in rel.split("/"):
            return None
        return os.path.join(self.raiz, rel) if rel else self.raiz

    def _corpo(self):
        n = int(self.headers.get("Content-Length", "0") or 0)
        return self.rfile.read(n) if n else b""

    def do_OPTIONS(self):
        self._resp(200, b"", {"DAV": "1", "Allow": "OPTIONS, GET, HEAD, PUT, DELETE, PROPFIND, MKCOL, MOVE"})

    def _prop(self, href, f):
        st = os.stat(f)
        tipo = "<D:resourcetype><D:collection/></D:resourcetype>" if os.path.isdir(f) else "<D:resourcetype/>"
        tam = "" if os.path.isdir(f) else f"<D:getcontentlength>{st.st_size}</D:getcontentlength>"
        data = email.utils.formatdate(st.st_mtime, usegmt=True)
        return (f"<D:response><D:href>{escape(href)}</D:href><D:propstat><D:prop>{tipo}{tam}"
                f"<D:getlastmodified>{data}</D:getlastmodified></D:prop><D:status>HTTP/1.1 200 OK</D:status>"
                f"</D:propstat></D:response>")

    def do_PROPFIND(self):
        Dav.pedidos.append(("PROPFIND", self.path))
        self._corpo()
        if not self._auth():
            return
        f = self._local()
        if not f or not os.path.exists(f):
            return self._resp(404)
        href = urllib.parse.urlparse(self.path).path
        partes = [self._prop(href, f)]
        if os.path.isdir(f) and self.headers.get("Depth", "1") != "0":
            base = href if href.endswith("/") else href + "/"
            for n in sorted(os.listdir(f)):
                q = urllib.parse.quote(n) + ("/" if os.path.isdir(os.path.join(f, n)) else "")
                partes.append(self._prop(base + q, os.path.join(f, n)))
        xml = ('<?xml version="1.0" encoding="utf-8"?>\n<D:multistatus xmlns:D="DAV:">' + "".join(partes) +
               "</D:multistatus>").encode()
        self._resp(207, xml, {"Content-Type": 'application/xml; charset="utf-8"'})

    def do_GET(self):
        Dav.pedidos.append(("GET", self.path, self.headers.get("Range", "")))
        if not self._auth():
            return
        f = self._local()
        if not f or not os.path.isfile(f):
            return self._resp(404)
        d = open(f, "rb").read()
        r = re.match(r"bytes=(\d+)-(\d*)$", self.headers.get("Range", ""))
        if r:
            a = int(r.group(1))
            b = int(r.group(2)) if r.group(2) else len(d) - 1
            b = min(b, len(d) - 1)
            return self._resp(206, d[a:b + 1], {"Content-Range": f"bytes {a}-{b}/{len(d)}"})
        self._resp(200, d)

    do_HEAD = do_GET

    def do_PUT(self):
        Dav.pedidos.append(("PUT", self.path))
        d = self._corpo()
        if not self._auth():
            return
        f = self._local()
        if not f or not os.path.isdir(os.path.dirname(f)):
            return self._resp(409)
        existia = os.path.exists(f)
        open(f, "wb").write(d)
        self._resp(204 if existia else 201)

    def do_DELETE(self):
        Dav.pedidos.append(("DELETE", self.path))
        if not self._auth():
            return
        f = self._local()
        if not f or not os.path.exists(f) or f == self.raiz:
            return self._resp(404)
        if os.path.isdir(f):
            shutil.rmtree(f)
        else:
            os.remove(f)
        self._resp(204)

    def do_MKCOL(self):
        Dav.pedidos.append(("MKCOL", self.path))
        self._corpo()
        if not self._auth():
            return
        f = self._local()
        if not f:
            return self._resp(403)
        if os.path.exists(f):
            return self._resp(405)
        if not os.path.isdir(os.path.dirname(f.rstrip("/"))):
            return self._resp(409)
        os.mkdir(f)
        self._resp(201)

    def do_MOVE(self):
        Dav.pedidos.append(("MOVE", self.path, self.headers.get("Destination", "")))
        if not self._auth():
            return
        f, d = self._local(), self._local(self.headers.get("Destination", ""))
        if not f or not d or not os.path.exists(f):
            return self._resp(404)
        if not os.path.isdir(os.path.dirname(d.rstrip("/"))):
            return self._resp(409)
        existia = os.path.exists(d)
        if existia and self.headers.get("Overwrite", "T") == "F":
            return self._resp(412)
        if existia:
            shutil.rmtree(d) if os.path.isdir(d) else os.remove(d)
        os.rename(f, d)
        self._resp(204 if existia else 201)


class Servidor:
    def __init__(self, raiz, porta, contexto=None):
        self.raiz, self.porta, self.contexto = raiz, porta, contexto
        self.srv = None

    def ligar(self):
        h = type("DavH", (Dav,), {"raiz": self.raiz})
        http.server.ThreadingHTTPServer.allow_reuse_address = True
        self.srv = http.server.ThreadingHTTPServer(("0.0.0.0", self.porta), h)
        if self.contexto:
            self.srv.socket = self.contexto.wrap_socket(self.srv.socket, server_side=True)
        threading.Thread(target=self.srv.serve_forever, daemon=True).start()

    def desligar(self):
        if self.srv:
            self.srv.shutdown()
            self.srv.server_close()
            self.srv = None


def puerto_libre():
    s = socket.socket()
    s.bind(("0.0.0.0", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def preparar_raiz(raiz):
    if os.path.exists(raiz):
        shutil.rmtree(raiz)
    os.makedirs(os.path.join(raiz, "carpeta"))
    os.makedirs(raiz + "-publico", exist_ok=True)
    open(os.path.join(raiz, "hola.txt"), "w").write("Hola desde el servidor WebDAV\n")
    open(os.path.join(raiz, "carpeta", "dentro.txt"), "w").write("dentro de la carpeta\n")
    open(os.path.join(raiz, "ñandú.txt"), "w").write("con acento\n")
    grande = bytes((i * 7 + i // 251) & 0xFF for i in range(9 * 1024 * 1024 + 123))
    open(os.path.join(raiz, "grande.bin"), "wb").write(grande)
    prog = os.path.join(RAIZ, "build", "userland", "bin", "eco")
    if not os.path.exists(prog):
        prog = os.path.join(RAIZ, "build", "userland", "bin", "echo")
    shutil.copy(prog, os.path.join(raiz, "programa"))
    return grande


def certificados(d):
    """Una autoridad de prueba y un certificado para 10.0.2.2 (la IP del anfitrión vista desde QEMU)."""
    os.makedirs(d, exist_ok=True)
    ca_k, ca_c = os.path.join(d, "ca.key"), os.path.join(d, "ca.pem")
    k, csr, c, ext = (os.path.join(d, x) for x in ("srv.key", "srv.csr", "srv.pem", "ext.cnf"))
    run = lambda *a: subprocess.run(a, check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    run("openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", ca_k)
    run("openssl", "req", "-x509", "-new", "-key", ca_k, "-sha256", "-days", "30", "-subj", "/CN=Autoridad H485", "-out", ca_c)
    run("openssl", "ecparam", "-name", "prime256v1", "-genkey", "-noout", "-out", k)
    run("openssl", "req", "-new", "-key", k, "-subj", "/CN=10.0.2.2", "-out", csr)
    open(ext, "w").write("subjectAltName=IP:10.0.2.2\nbasicConstraints=CA:FALSE\nkeyUsage=digitalSignature\n"
                         "extendedKeyUsage=serverAuth\n")
    run("openssl", "x509", "-req", "-in", csr, "-CA", ca_c, "-CAkey", ca_k, "-CAcreateserial", "-days", "30", "-sha256",
        "-extfile", ext, "-out", c)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(c, k)
    return ctx, ca_c


# ------------------------------------------------------------------ pc

def pc():
    import urllib.request
    raiz = os.path.join(TRAB, "pc")
    preparar_raiz(raiz)
    porta = puerto_libre()
    s = Servidor(raiz, porta)
    s.ligar()
    try:
        auth = {"Authorization": "Basic " + base64.b64encode(f"{USUARIO}:{SENHA}".encode()).decode()}

        def pedir(metodo, ruta, corpo=None, cab=None):
            r = urllib.request.Request(f"http://127.0.0.1:{porta}{ruta}", data=corpo, method=metodo, headers={**auth, **(cab or {})})
            try:
                with urllib.request.urlopen(r, timeout=10) as x:
                    return x.status, x.read()
            except urllib.error.HTTPError as e:
                return e.code, e.read()
        st, xml = pedir("PROPFIND", "/dav/", b"", {"Depth": "1"})
        verificar(st == 207 and b"hola.txt" in xml and b"<D:collection/>" in xml, "PROPFIND Depth 1")
        st, d = pedir("GET", "/dav/grande.bin", cab={"Range": "bytes=10-19"})
        verificar(st == 206 and len(d) == 10, "GET con Range")
        verificar(pedir("PUT", "/dav/nuevo.txt", b"abc")[0] == 201, "PUT")
        verificar(pedir("MKCOL", "/dav/d1/")[0] == 201 and pedir("MKCOL", "/dav/d1/")[0] == 405, "MKCOL")
        verificar(pedir("MOVE", "/dav/nuevo.txt", cab={"Destination": f"http://127.0.0.1:{porta}/dav/d1/n.txt"})[0] == 201, "MOVE")
        verificar(pedir("DELETE", "/dav/d1/")[0] == 204, "DELETE")
        sem = urllib.request.Request(f"http://127.0.0.1:{porta}/dav/", method="PROPFIND")
        try:
            urllib.request.urlopen(sem, timeout=10)
            verificar(False, "sin contraseña: 401")
        except urllib.error.HTTPError as e:
            verificar(e.code == 401, "sin contraseña: 401")
    finally:
        s.desligar()
    d = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D485." in d, "D485 está en docs/decisoes.md")
    verificar(os.path.isfile(os.path.join(RAIZ, "rootfs", "ayuda", "unidades-red.md")), "rootfs/ayuda/unidades-red.md")
    r = open(os.path.join(RAIZ, "docs", "ROADMAP.md"), encoding="utf-8").read()
    verificar("v8.5.6.5" in r and "D485" in r, "la fila de la v8.5.6.5 en el ROADMAP")


# ------------------------------------------------------------------ SaguiOS

def esperar(m, patron, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise AssertionError(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-2500:])


def orden(m, texto, tempo=90):
    orden.n += 1
    marca = f"FIN-{orden.n:03d}"
    antes = len(m.serial())
    m.digitar(f"{texto}; echo FIN-'{orden.n:03d}'", 0.03)
    m.tecla("ret")
    g = esperar(m, rf"\n{marca}\b", tempo, antes)
    return m.serial()[antes:antes + g.end()].replace("\r", "")


orden.n = 0


def fuga(m):
    """¿Sale la contraseña en alguna línea de la serie que no sea la orden tecleada (con «echo»)?"""
    return [l for l in m.serial().split("\n") if SENHA in l and "echo " not in l]


def saguios():
    raiz = os.path.join(TRAB, "servidor")
    grande = preparar_raiz(raiz)
    ctx, ca = certificados(os.path.join(TRAB, "cert"))
    shutil.copy(ca, os.path.join(raiz + "-publico", "ca.pem"))
    p_http, p_https = puerto_libre(), puerto_libre()
    srv = Servidor(raiz, p_http)
    srv_tls = Servidor(raiz, p_https, ctx)
    srv.ligar()
    srv_tls.ligar()
    img = imagem_de_teste("", ["--console", "--sesion-root", "--home-mb", "32", "--home-fs", "ext2"])
    try:
        with Maquina("h485", "ide", imagem=img, memoria="512M", extras=["-nic", "user,model=e1000"]) as m:
            esperar(m, r"supervisor en marcha", 150)
            esperar(m, r"(root|sagui):\S*\$ ", 60)
            verificar("uid=0" in orden(m, "id"), "la sesión de la prueba es root")
            orden(m, "echo ana:1001:1001:/home/ana:/bin/sagsh:Ana >> /etc/usuarios; mkdir /home/ana; "
                     "chown ana:ana /home/ana; chmod 700 /home/ana")
            # la red (DHCP de QEMU)
            for _ in range(30):
                if "10.0.2." in orden(m, "ifconfig"):
                    break
                time.sleep(1)
            sg = lambda t, tempo=90: orden(m, f"su sagui -c '{t}'", tempo)
            out = sg("dav lista")
            verificar("No tienes unidades" in out, f"sin unidades al principio ({out[-200:]!r})")
            out = sg(f"echo {SENHA} | dav agregar nube http://10.0.2.2:{p_http}/dav/ {USUARIO}")
            verificar("Unidad nube guardada" in out, f"dav agregar ({out[-300:]!r})")
            verificar("quien copie el fichero lo abre" in out, "aviso: llavero sin contraseña")
            out = orden(m, "ls -l --color=never /home/.config/llavero /home/.config/unidades-red; cat /home/.config/llavero")
            verificar(re.search(r"^-rw------- +sagui .*llavero", out, re.M), f"llavero 0600 de sagui ({out[-500:]!r})")
            verificar(re.search(r"^-rw------- +sagui .*unidades-red", out, re.M), "unidades-red 0600")
            verificar("SAGUI-LLAVERO 1 vacia" in out and "datos " in out, "cabecera del llavero")
            verificar(SENHA not in out and SENHA.encode().hex() not in out, "la contraseña NO está en claro en el llavero")
            out = orden(m, "cat /home/.config/unidades-red")
            verificar(SENHA not in out and "nube\t" in out.replace("    ", "\t") or "nube" in out, "unidades-red sin la contraseña")
            verificar(not fuga(m), "la contraseña no sale por la serie (registro) salvo en lo tecleado")

            out = sg("dav montar nube", 60)
            verificar("/red/nube montada: conectada" in out, f"dav montar ({out[-300:]!r})")
            out = sg("ls /red; ls /red/nube; cat /red/nube/hola.txt; cat /red/nube/carpeta/dentro.txt")
            verificar(re.search(r"\bnube\b", out) and "hola.txt" in out and "carpeta" in out and "programa" in out,
                      f"listar /red y /red/nube ({out[-400:]!r})")
            verificar("Hola desde el servidor WebDAV" in out and "dentro de la carpeta" in out, "leer ficheros")
            out = sg("echo escrito desde SaguiOS > /red/nube/nuevo.txt; cat /red/nube/nuevo.txt")
            verificar("escrito desde SaguiOS" in out, "escribir y volver a leer")
            verificar(open(os.path.join(raiz, "nuevo.txt")).read() == "escrito desde SaguiOS\n", "el fichero llegó al servidor")
            sg("cp /red/nube/hola.txt /tmp/copia.txt; cp /tmp/copia.txt /red/nube/carpeta/copia.txt")
            verificar(open(os.path.join(raiz, "carpeta", "copia.txt")).read() == "Hola desde el servidor WebDAV\n", "cp de ida y vuelta")
            out = sg("python -c \"f=open(\\\"/red/nube/py.txt\\\",\\\"w\\\"); f.write(\\\"desde python\\\"); f.close(); "
                     "print(open(\\\"/red/nube/py.txt\\\").read())\"")
            verificar(os.path.exists(os.path.join(raiz, "py.txt")) and open(os.path.join(raiz, "py.txt")).read() == "desde python",
                      f"una aplicación (Python) guarda y abre ({out[-300:]!r})")
            sg("mkdir /red/nube/dir1; mv /red/nube/nuevo.txt /red/nube/dir1/movido.txt; rm /red/nube/hola.txt")
            verificar(os.path.isfile(os.path.join(raiz, "dir1", "movido.txt")) and not os.path.exists(os.path.join(raiz, "hola.txt")),
                      "mkdir, mv y rm en el servidor")
            out = sg("rmdir /red/nube/dir1")
            verificar(os.path.isdir(os.path.join(raiz, "dir1")), f"rmdir de una carpeta no vacía no borra nada ({out[-200:]!r})")
            sg("rm /red/nube/dir1/movido.txt; rmdir /red/nube/dir1")
            verificar(not os.path.exists(os.path.join(raiz, "dir1")), "rmdir de una carpeta vacía")
            out = sg("cat /red/nube/ñandú.txt")
            verificar("con acento" in out, f"nombre con acento ({out[-200:]!r})")
            # fichero grande por Range: cmp de un trozo con lo que hay en el servidor
            Dav.pedidos.clear()
            out = sg("sha256sum /red/nube/grande.bin", 240)
            import hashlib
            verificar(hashlib.sha256(grande).hexdigest() in out, f"fichero de 9 MB leído entero ({out[-200:]!r})")
            verificar(any(p[0] == "GET" and p[2].startswith("bytes=") for p in Dav.pedidos if len(p) > 2), "leído por Range")
            # sin ejecución
            antes = len(m.serial())
            out = sg("/red/nube/programa hola; ls -l --color=never /red/nube/programa")
            nuevo = m.serial()[antes:]
            verificar("no se ejecuta (montaje sin ejecucion)" in nuevo or "denegado a sagsh" in nuevo, f"el kernel no ejecuta de /red ({out[-300:]!r})")
            verificar("programa hola" not in out.split("\n", 2)[-1].replace("programa hola;", ""), "el programa no escribió nada")
            antes = len(m.serial())
            orden(m, "/red/nube/programa como-root")   # root se salta los permisos, pero no el montaje sin ejecución
            verificar("no se ejecuta (montaje sin ejecucion)" in m.serial()[antes:], "ni root ejecuta desde /red (carregador.c)")
            verificar(re.search(r"^-rw------- .*programa", out, re.M), f"los ficheros de /red, sin la «x» ({out[-300:]!r})")
            out = sg("cp /red/nube/programa /tmp/prog; chmod 755 /tmp/prog; /tmp/prog copiado-fuera")
            verificar("copiado-fuera" in out, "copiado fuera de la unidad sí se ejecuta")
            # otra cuenta
            out = orden(m, "su ana -c 'ls /red; ls /red/nube; dav estado'")
            verificar("nube" not in out.split("su ana -c")[-1].replace("/red/nube", ""), f"ana no ve la unidad ({out[-300:]!r})")
            verificar("No hay unidades de red montadas" in out, "«dav estado» de ana: ninguna")
            out = orden(m, "ls /red; dav estado")
            verificar("nube" in out, "root sí la ve")
            # se corta el servidor
            srv.desligar()
            time.sleep(1)
            t0 = time.time()
            out = sg("ls /red/nube; cat /red/nube/carpeta/dentro.txt; dav estado", 60)
            dt = time.time() - t0
            verificar(dt < 40, f"sin servidor, las órdenes vuelven enseguida ({dt:.1f} s)")
            out = sg("sleep 1; dav estado; ls /red/nube", 60)
            verificar("desconectada" in out, f"estado: desconectada ({out[-300:]!r})")
            srv.ligar()
            ok = False
            for _ in range(12):
                time.sleep(3)
                out = sg("dav estado")
                if re.search(r"/red/nube +conectada", out):
                    ok = True
                    break
            verificar(ok, f"vuelve el servidor: conectada ({out[-200:]!r})")
            out = sg("cat /red/nube/carpeta/dentro.txt")
            verificar("dentro de la carpeta" in out, "y se lee otra vez")
            # HTTPS
            sg(f"wget -q -O /home/ca.pem http://10.0.2.2:{p_http}/publico/ca.pem")
            out = sg(f"echo {SENHA} | dav agregar sinca https://10.0.2.2:{p_https}/dav/ {USUARIO}; dav montar sinca", 90)
            verificar("certificado" in out and "montada" not in out, f"HTTPS sin la autoridad: no monta ({out[-300:]!r})")
            out = sg(f"echo {SENHA} | dav agregar segura https://10.0.2.2:{p_https}/dav/ {USUARIO} --cacert /home/ca.pem; "
                     "dav montar segura; cat /red/segura/carpeta/dentro.txt", 120)
            verificar("/red/segura montada: conectada" in out and "dentro de la carpeta" in out, f"HTTPS con --cacert ({out[-300:]!r})")
            # llavero con contraseña propia
            out = sg("printf \"propia-1234\\npropia-1234\\n\" | dav llavero contrasena; dav llavero estado")
            verificar("contraseña propia" in out, f"llavero con contraseña propia ({out[-300:]!r})")
            out = orden(m, "head -n 1 /home/.config/llavero")
            verificar("SAGUI-LLAVERO 1 propia" in out, "cabecera: propia")
            out = sg("dav llavero cerrar; dav llavero estado; dav probar nube")
            verificar("Llavero cerrado" in out and "llavero está cerrado" in out, f"cerrado no da la contraseña ({out[-300:]!r})")
            out = sg("echo propia-1234 | dav llavero abrir; dav probar nube")
            verificar("Llavero abierto" in out and "conexión correcta" in out, f"abierto con la propia ({out[-300:]!r})")
            # desmontar
            out = sg("dav desmontar nube; dav desmontar segura; ls /red; dav estado")
            verificar("No hay unidades de red montadas" in out, f"desmontadas ({out[-300:]!r})")
            verificar(not fuga(m), "la contraseña no salió nunca por la serie (salvo en lo tecleado)")
            m.captura("H485-1-consola")
    finally:
        srv.desligar()
        srv_tls.desligar()


def gui():
    import h136_opentyrian as h136
    raiz = os.path.join(TRAB, "servidor-gui")
    preparar_raiz(raiz)
    p_http = puerto_libre()
    srv = Servidor(raiz, p_http)
    srv.ligar()
    img = imagem_de_teste("", ["--vesa"])
    try:
        with Maquina("h485g", "ide", imagem=img, memoria="512M", extras=["-nic", "user,model=e1000"]) as m:
            esperar(m, r"sagui:/\$", 200)
            time.sleep(4)
            h136.focar_terminal(m)

            def gorden(t, tempo=90):
                marca = len(m.serial())
                m.digitar(t, 0.03)
                m.tecla("ret")
                esperar(m, r"sagui:[^\n]*\$", tempo, marca + len(t))
                time.sleep(0.8)
                return m.serial()[marca:].replace("\r", "")
            for _ in range(30):
                if "10.0.2." in gorden("ifconfig"):
                    break
                time.sleep(1)
            out = gorden(f"echo {SENHA} | dav agregar nube http://10.0.2.2:{p_http}/dav/ {USUARIO} --auto; dav montar nube", 90)
            verificar("/red/nube montada: conectada" in out, f"montada desde el Terminal ({out[-300:]!r})")
            esperar(m, r"\[GUI\] bandeja [^\n]*dav", 30)   # el icono de la unidad en la bandeja
            antes = len(m.serial())
            gorden("configuracion --ventana unidades-red &")
            esperar(m, r"configuracion: unidades de red 1, llavero 0", 60, antes)
            time.sleep(3)
            m.captura("H485-2-configuracion")
            m.tecla("alt-f4")
            time.sleep(1)
            h136.focar_terminal(m)
            antes = len(m.serial())
            gorden("arquivos /red/nube &")
            esperar(m, r"arquivos: lateral RED y=\d+", 60, antes)
            time.sleep(3)
            m.captura("H485-3-archivos")
    finally:
        srv.desligar()


def main():
    partes = sys.argv[1:] or ["pc", "saguios", "gui"]
    os.makedirs(TRAB, exist_ok=True)
    if "pc" in partes:
        pc()
    if "saguios" in partes:
        saguios()
    if "gui" in partes:
        gui()
    print("H485 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
