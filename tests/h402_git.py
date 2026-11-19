#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H402 - la orden git de SaguiOS, sobre libgit2, por HTTPS y por SSH (D400, docs/git.md).

Partes (por defecto todas; o las que se pidan: h402_git.py pc local ssh https):
  pc     en el PC: /apps/bin/git es un ELF32 estático; ayuda, docs/git.md, fila de terceros y D400;
         las fuentes de libgit2 no están en git (tools/git_fuentes.sh).
  local  en SaguiOS: repositorio local (init, config, add, commit, log, diff, branch, checkout, status).
  ssh    en SaguiOS contra un servidor SSH escrito aquí (el de h317, con «cryptography»), que ante
         git-upload-pack / git-receive-pack lanza el git del anfitrión sobre un repositorio desnudo:
         clone, commit, push (comprobado con el git del anfitrión), pull y fetch por la clave que crea
         ssh-keygen dentro de SaguiOS.
  https  en SaguiOS contra «git http-backend» del anfitrión detrás de un servidor TLS con una CA de
         prueba: sin la CA, error de certificado; con «certificados añadir», clone, push y pull;
         y por HTTP plano.
"""
import base64
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

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, RAIZ, SAIDA, BUILD, FalhaTeste  # noqa: E402

TRAB = os.path.join(SAIDA, "h402")
FUENTES = os.path.join(RAIZ, "userland", "terceros", "git", "fuentes", ".preparado")
KVM = os.environ.get("SAGUI_KVM") == "1"
LENTO = 1 if KVM else 3
ENV_GIT = dict(os.environ, GIT_CONFIG_NOSYSTEM="1", HOME=os.path.join(TRAB, "home"),
               GIT_AUTHOR_NAME="Anfitrion", GIT_AUTHOR_EMAIL="pc@h402", GIT_COMMITTER_NAME="Anfitrion",
               GIT_COMMITTER_EMAIL="pc@h402")


def hgit(*args, cwd=None, entrada=None):
    r = subprocess.run(["git", *args], cwd=cwd, env=ENV_GIT, capture_output=True, input=entrada)
    if r.returncode != 0:
        raise FalhaTeste("git del anfitrión %s: %s" % (args, r.stderr.decode(errors="replace")[:400]))
    return r.stdout.decode(errors="replace")


def esperar(m, texto, desde, tempo=60):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial()[desde:]:
            return m.serial()[desde:]
        time.sleep(0.2)
    raise FalhaTeste("no apareció %r: %r" % (texto, m.serial()[desde:][-800:]))


def esperar_prompt(m, desde, tempo=60):
    """Espera a que el programa lanzado a mano acabe y vuelva el indicador (para no desincronizar m.comando)."""
    fim = time.time() + tempo
    while time.time() < fim:
        if re.search(r"terminou com codigo \d+\)?\s*\nsagui:[^\n]*\$ ?\s*$", m.serial()[desde:].replace("\r", "")):
            return
        time.sleep(0.2)
    raise FalhaTeste("no volvió el indicador: %r" % m.serial()[desde:][-400:])


# ------------------------------------------------------------ pc

def pc():
    git = os.path.join(BUILD, "apps", "raiz", "bin", "git")
    verificar(os.path.isfile(git), "la imagen lleva /apps/bin/git")
    h = subprocess.run(["readelf", "-h", "-l", "-W", git], capture_output=True, text=True).stdout
    verificar("ELF32" in h and "Intel 80386" in h and "INTERP" not in h, "/apps/bin/git: ELF32 estático de i386")
    print("    tamaño de /apps/bin/git: %d KB" % (os.path.getsize(git) // 1024))
    for f, buscado in (("rootfs/ayuda/git.md", "libgit2"), ("docs/git.md", "libgit2"), ("docs/terceros.md", "libgit2"),
                       ("docs/decisoes.md", "## D400")):
        t = open(os.path.join(RAIZ, f), encoding="utf-8").read()
        verificar(buscado in t, "%s menciona %s" % (f, buscado))
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("GPL-2.0" in t and "excepción de enlace" in t, "docs/terceros.md: libgit2 con su licencia y la excepción de enlace")
    verificar(os.path.isfile(os.path.join(RAIZ, "tools", "git_fuentes.sh")), "tools/git_fuentes.sh")


# ------------------------------------------------------------ repositorio del anfitrión

def preparar_remoto():
    shutil.rmtree(TRAB, ignore_errors=True)
    os.makedirs(os.path.join(TRAB, "home"))
    bare = os.path.join(TRAB, "remoto.git")
    hgit("init", "--bare", "-b", "main", bare)
    hgit("config", "http.receivepack", "true", cwd=bare)
    sem = os.path.join(TRAB, "semilla")
    hgit("init", "-b", "main", sem)
    with open(os.path.join(sem, "leeme.txt"), "w") as f:
        f.write("hola desde el anfitrion\n")
    os.makedirs(os.path.join(sem, "src"))
    with open(os.path.join(sem, "src", "a.c"), "w") as f:
        f.write("int main(void) { return 0; }\n" * 200)
    hgit("add", ".", cwd=sem)
    hgit("commit", "-m", "primer commit del anfitrion", cwd=sem)
    hgit("remote", "add", "origin", bare, cwd=sem)
    hgit("push", "origin", "main", cwd=sem)
    return bare, sem


def comprobar_remoto(bare, esperado):
    log = hgit("log", "--format=%s", "main", cwd=bare)
    verificar(esperado in log, "el git del anfitrión ve «%s» en el repositorio remoto: %s" % (esperado, log.split("\n")[:4]))
    r = subprocess.run(["git", "fsck", "--strict"], cwd=bare, env=ENV_GIT, capture_output=True, text=True)
    verificar(r.returncode == 0, "git fsck del anfitrión: el repositorio remoto es íntegro (%s)" % r.stderr.strip()[:200])


# ------------------------------------------------------------ servidor SSH con git

def servidor_ssh(raiz_repos):
    import h317_ssh_cliente as h  # el servidor SSH mínimo de la prueba h317

    class ConexaoGit(h.Conexao):
        def ordem(self, cmd, entrada_inicial):
            c = cmd.decode("utf-8", "replace")
            self.log("exec " + c)
            m = re.match(r"git[- ](upload-pack|receive-pack) '?([^']+)'?$", c.strip())
            if not m:
                return super().ordem(cmd, entrada_inicial)
            ruta = m.group(2)
            if not os.path.isdir(ruta):
                self.canal_dados(("fatal: '%s' no existe\n" % ruta).encode(), extra=True)
                return self.fim(128)
            p = subprocess.Popen(["git", m.group(1), ruta], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                 stderr=subprocess.DEVNULL, env=ENV_GIT)

            def salida():
                while True:
                    d = p.stdout.read1(16384)
                    if not d:
                        break
                    self.canal_dados(d)
            t = threading.Thread(target=salida, daemon=True)
            t.start()
            try:
                while True:
                    r = self.receber()
                    if r[0] == 94:
                        l = h.Leitor(r, 1)
                        l.u32()
                        d = l.str()
                        try:
                            p.stdin.write(d)
                            p.stdin.flush()
                        except BrokenPipeError:
                            pass
                        self.enviar(bytes([93]) + h._u32(self.remoto) + h._u32(len(d)))   # ventana: se consumió
                    elif r[0] in (96, 97):
                        break
                    if p.poll() is not None and not t.is_alive():
                        break
            except Exception:
                pass
            try:
                p.stdin.close()
            except Exception:
                pass
            p.wait(timeout=60)
            t.join(timeout=30)
            self.log("git %s fin %d" % (m.group(1), p.returncode))
            return self.fim(p.returncode)

    class Srv(h.Servidor):
        def bucle(self):
            while True:
                try:
                    s, _ = self.ls.accept()
                except OSError:
                    return
                threading.Thread(target=ConexaoGit(self, s).atender, daemon=True).start()

    srv = Srv()
    srv.senha = None
    return srv


# ------------------------------------------------------------ servidor HTTP(S) con git http-backend

BACKEND = "/usr/lib/git-core/git-http-backend"


def crear_pki():
    from cryptography import x509
    from cryptography.hazmat.primitives import hashes, serialization
    from cryptography.hazmat.primitives.asymmetric import ec
    from cryptography.x509.oid import NameOID
    import datetime
    import ipaddress
    ahora = datetime.datetime.now(datetime.timezone.utc)
    ca_k = ec.generate_private_key(ec.SECP256R1())
    nombre = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "CA de prueba h402")])
    ca = (x509.CertificateBuilder().subject_name(nombre).issuer_name(nombre).public_key(ca_k.public_key())
          .serial_number(x509.random_serial_number()).not_valid_before(ahora - datetime.timedelta(days=1))
          .not_valid_after(ahora + datetime.timedelta(days=30))
          .add_extension(x509.BasicConstraints(ca=True, path_length=None), critical=True)
          .add_extension(x509.KeyUsage(True, False, False, False, False, True, True, False, False), critical=True)
          .sign(ca_k, hashes.SHA256()))
    k = ec.generate_private_key(ec.SECP256R1())
    sn = x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, "servidor h402")])
    cert = (x509.CertificateBuilder().subject_name(sn).issuer_name(nombre).public_key(k.public_key())
            .serial_number(x509.random_serial_number()).not_valid_before(ahora - datetime.timedelta(days=1))
            .not_valid_after(ahora + datetime.timedelta(days=30))
            .add_extension(x509.SubjectAlternativeName([x509.IPAddress(ipaddress.ip_address("10.0.2.2"))]), critical=False)
            .add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
            .sign(ca_k, hashes.SHA256()))
    pem = os.path.join(TRAB, "servidor.pem")
    with open(pem, "wb") as f:
        f.write(cert.public_bytes(serialization.Encoding.PEM))
        f.write(k.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.TraditionalOpenSSL,
                                serialization.NoEncryption()))
    capem = os.path.join(TRAB, "ca.pem")
    with open(capem, "wb") as f:
        f.write(ca.public_bytes(serialization.Encoding.PEM))
    return pem, capem


class TratadorGit(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    raiz = TRAB
    eventos = []

    def log_message(self, *a):
        pass

    def _cuerpo(self):
        if self.headers.get("Transfer-Encoding", "").lower() == "chunked":
            d = b""
            while True:
                n = int(self.rfile.readline().strip() or b"0", 16)
                if n == 0:
                    self.rfile.readline()
                    return d
                d += self.rfile.read(n)
                self.rfile.readline()
        n = int(self.headers.get("Content-Length", 0))
        return self.rfile.read(n) if n else b""

    def _atender(self):
        if self.path.startswith("/ca.pem"):
            d = open(os.path.join(TRAB, "ca.pem"), "rb").read()
            self.send_response(200)
            self.send_header("Content-Length", str(len(d)))
            self.end_headers()
            self.wfile.write(d)
            return
        cuerpo = self._cuerpo()
        ruta, _, consulta = self.path.partition("?")
        env = dict(ENV_GIT, GIT_PROJECT_ROOT=self.raiz, GIT_HTTP_EXPORT_ALL="1", REQUEST_METHOD=self.command,
                   PATH_INFO=ruta, QUERY_STRING=consulta, CONTENT_TYPE=self.headers.get("Content-Type", ""),
                   CONTENT_LENGTH=str(len(cuerpo)), REMOTE_ADDR="127.0.0.1", REMOTE_USER="prueba")
        if self.headers.get("Content-Encoding"):
            env["HTTP_CONTENT_ENCODING"] = self.headers["Content-Encoding"]
        if self.headers.get("Git-Protocol"):
            env["GIT_PROTOCOL"] = self.headers["Git-Protocol"]
        r = subprocess.run([BACKEND], env=env, input=cuerpo, capture_output=True)
        cab, _, resto = r.stdout.partition(b"\r\n\r\n")
        if not _:
            cab, _, resto = r.stdout.partition(b"\n\n")
        estado, tipo = 200, "application/octet-stream"
        extra = []
        for l in cab.decode("latin-1").splitlines():
            k, _, v = l.partition(":")
            if k.lower() == "status":
                estado = int(v.strip().split()[0])
            elif k.lower() == "content-type":
                tipo = v.strip()
            elif k.lower() not in ("content-length", "connection"):
                extra.append((k, v.strip()))
        TratadorGit.eventos.append("%s %s -> %d (%d bytes de entrada, %d de salida)" % (self.command, self.path, estado, len(cuerpo), len(resto)))
        self.send_response(estado)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(resto)))
        for k, v in extra:
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(resto)

    do_GET = _atender
    do_POST = _atender


def servidor_http(pem=None):
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), TratadorGit)
    srv.daemon_threads = True
    if pem:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(pem)
        ctx.set_alpn_protocols(["http/1.1"])
        srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv.server_address[1]


# ------------------------------------------------------------ SaguiOS

def arrancar(nombre):
    m = Maquina(nombre, "ide", imagem=imagem_console(apps=True), memoria="512M", extras=["-nic", "user,model=e1000"])
    return m


def esperar_red(m):
    m.esperar_serial("sagui:/$", 180)
    fim = time.time() + 60
    while time.time() < fim and "(DHCP)" not in m.serial():
        time.sleep(0.3)


def local():
    with arrancar("h402-local") as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("git --version", 30)
        verificar("libgit2 1.9" in out, "git --version: libgit2 1.9 (%s)" % out.strip()[-60:])
        out = m.comando("git status", 30)
        verificar("no es un repositorio" in out, "fuera de un repositorio: error claro")
        m.comando("mkdir -p /home/proy; cd /home/proy", 30)
        out = m.comando("git init", 60)
        verificar("Repositorio git vac" in out and "/home/proy/.git" in out, "git init crea el repositorio (rama main)")
        out = m.comando("git commit -m vacio", 60)
        verificar("falta tu identidad" in out, "commit sin identidad: pide git config")
        m.comando('git config --global user.name "Prueba Sagui"', 30)
        m.comando("git config --global user.email sagui@example.com", 30)
        out = m.comando("git config user.name; git config --global --list", 30)
        verificar("Prueba Sagui" in out and "user.email=sagui@example.com" in out, "git config global se guarda y se lee")
        m.comando("echo uno > a.txt; echo dos > b.txt; mkdir src; echo int-x > src/c.c", 30)
        out = m.comando("git status", 30)
        verificar("## main" in out or "## sin commits" in out, "status: la rama")
        verificar("?? a.txt" in out and "?? src/c.c" in out or "?? src/" in out, "status: archivos sin seguir (??)")
        m.comando("git add a.txt src", 30)
        out = m.comando("git status", 30)
        verificar("A  a.txt" in out and "A  src/c.c" in out and "?? b.txt" in out, "status tras add: A  (preparado) y ?? (sin seguir)")
        out = m.comando('git commit -m "primer commit"', 60)
        verificar("[main " in out and "primer commit" in out, "git commit: el primer commit en main (%s)" % out.strip()[-70:])
        m.comando("git add b.txt", 30)
        out = m.comando("git diff --cached", 30)
        verificar("+dos" in out and "b.txt" in out, "diff --cached muestra lo preparado")
        out = m.comando('git commit -m "segundo con b"', 60)
        verificar("segundo con b" in out, "segundo commit")
        out = m.comando("git commit -m otra-vez", 30)
        verificar("Nada que confirmar" in out, "commit sin cambios: «Nada que confirmar»")
        m.comando("echo cambiado >> a.txt", 30)
        out = m.comando("git status", 30)
        verificar(" M a.txt" in out, "status: ` M` (modificado sin preparar)")
        out = m.comando("git diff", 30)
        verificar("+cambiado" in out and "--- a/a.txt" in out and "+++ b/a.txt" in out, "git diff: el parche de a.txt")
        out = m.comando("git diff --stat", 30)
        verificar("a.txt" in out and "1 file" in out or "1 archivo" in out or "insertion" in out, "git diff --stat")
        out = m.comando("git log --oneline", 30)
        verificar(len(re.findall(r"^[0-9a-f]{8} ", out, re.M)) == 2 and "segundo con b" in out, "git log --oneline: 2 commits")
        out = m.comando("git log -n 1", 30)
        verificar("Autor: Prueba Sagui <sagui@example.com>" in out and "segundo con b" in out and "Fecha:" in out,
                  "git log -n 1: autor, fecha y mensaje")
        out = m.comando("git branch", 30)
        verificar("* main" in out, "git branch: la actual lleva *")
        m.comando("git commit -a -m cambia-a", 60)
        out = m.comando("git branch nueva; git checkout nueva", 60)
        verificar("rama 'nueva'" in out, "branch + checkout: cambia a la rama nueva")
        m.comando("echo solo-nueva > n.txt; git add n.txt; git commit -m en-nueva", 60)
        out = m.comando("ls; git log --oneline", 30)
        verificar("n.txt" in out and "en-nueva" in out, "en la rama nueva hay n.txt y su commit")
        out = m.comando("git checkout main; ls; git log --oneline", 60)
        verificar("n.txt" not in out and "en-nueva" not in out and "cambia-a" in out, "checkout main: n.txt desaparece")
        out = m.comando("git checkout -b otra; git branch", 60)
        verificar("* otra" in out and "  nueva" in out and "  main" in out, "checkout -b crea y cambia; branch lista las tres")
        m.comando("git checkout main", 30)
        out = m.comando("git branch -d otra; git branch", 30)
        verificar("borrada" in out and "otra" not in out.split("borrada")[-1], "branch -d borra la rama")
        m.comando("echo roto > a.txt", 30)
        m.comando("git checkout -- a.txt", 30)
        out = m.comando("cat a.txt", 30)
        verificar("roto" not in out and "uno" in out, "checkout -- archivo restaura el archivo")
        m.comando("git remote add origen /home/otro.git; git remote add dos https://x.y/z.git", 30)
        out = m.comando("git remote -v", 30)
        verificar("origen\t/home/otro.git (fetch)" in out.replace("\r", "") or "origen" in out and "(fetch)" in out and "https://x.y/z.git" in out,
                  "git remote add y -v")
        m.comando("git remote remove dos", 30)
        out = m.comando("git remote", 30)
        verificar("origen" in out and "dos" not in out, "git remote remove")
        # clone local (transporte de archivos): ida y vuelta entre dos repositorios de SaguiOS
        out = m.comando("cd /home; git clone /home/proy copia", 120)
        verificar("Clonando" in out and "git:" not in out, "git clone de un repositorio local (file)")
        out = m.comando("cat /home/copia/a.txt; git -C /home/copia log --oneline", 30)
        verificar("uno" in out and "cambia-a" in out, "la copia tiene los archivos y el historial")
        m.captura("H402-1-local")


def ssh():
    bare, sem = preparar_remoto()
    srv = servidor_ssh(TRAB)
    P = srv.porta
    rem = "ssh://h402@10.0.2.2:%d%s" % (P, bare)
    with arrancar("h402-ssh") as m:
        esperar_red(m)
        m.comando('git config --global user.name "Prueba Sagui"; git config --global user.email sagui@example.com', 30)
        out = m.comando("ssh-keygen -t ed25519 -N ''", 60)
        verificar("Huella" in out, "ssh-keygen crea la clave de SaguiOS")
        out = m.comando("cat /home/.ssh/id_ed25519.pub", 30)
        lp = re.search(r"ssh-ed25519 ([A-Za-z0-9+/=]{68})", out)
        verificar(lp, "clave pública de SaguiOS")
        srv.autorizada = base64.b64decode(lp.group(1))[-32:]
        # la primera conexión de ssh pregunta por la huella: se hace a mano (git no tiene terminal)
        marca = len(m.serial())
        m.digitar("ssh -p %d h402@10.0.2.2 echo hola\n" % P, 0.04)
        esperar(m, "(sí/no)", marca, 60)
        m.digitar("si\n", 0.05)
        esperar(m, "hola", marca, 60)
        esperar_prompt(m, marca)
        verificar(True, "known_hosts: el servidor de prueba queda conocido")
        # clone por SSH
        out = m.comando("cd /home; git clone %s trabajo" % rem, 300 * LENTO)
        verificar("Clonando en 'trabajo'" in out and "git:" not in out, "clone por ssh:// con el cliente ssh de SaguiOS: %s" % out.strip()[-200:])
        out = m.comando("cat /home/trabajo/leeme.txt; ls /home/trabajo/src; git -C /home/trabajo log --oneline", 60)
        verificar("hola desde el anfitrion" in out and "a.c" in out and "primer commit del anfitrion" in out,
                  "el clon tiene los archivos y el historial del anfitrión")
        out = m.comando("git -C /home/trabajo status; git -C /home/trabajo branch -a", 60)
        verificar("## main" in out and "* main" in out and "remotes/origin/main" in out, "el clon sigue a origin/main")
        # commit + push, y lo comprueba el git del anfitrión
        m.comando("cd /home/trabajo; echo desde-sagui > nuevo.txt; git add nuevo.txt; git commit -m 'commit hecho en SaguiOS'", 120)
        out = m.comando("git push", 300 * LENTO)
        verificar("Enviado main" in out and "git:" not in out, "push por ssh: %s" % out.strip()[-200:])
        comprobar_remoto(bare, "commit hecho en SaguiOS")
        verificar("desde-sagui" in hgit("show", "main:nuevo.txt", cwd=bare), "el archivo llegó al remoto con su contenido")
        # el anfitrión cambia; SaguiOS fetch + pull
        hgit("pull", "origin", "main", cwd=sem)
        with open(os.path.join(sem, "otro.txt"), "w") as f:
            f.write("del anfitrion 2\n")
        hgit("add", ".", cwd=sem)
        hgit("commit", "-m", "segundo del anfitrion", cwd=sem)
        hgit("push", "origin", "main", cwd=sem)
        out = m.comando("git fetch", 300 * LENTO)
        verificar("git:" not in out, "fetch por ssh: %s" % out.strip()[-160:])
        out = m.comando("git log --oneline origin/main -n 1", 60)
        verificar("segundo del anfitrion" in out, "fetch trajo el commit nuevo a origin/main")
        out = m.comando("git pull", 300 * LENTO)
        verificar("Avance r" in out, "pull: avance rápido (%s)" % out.strip()[-120:])
        out = m.comando("cat otro.txt; git log --oneline", 60)
        verificar("del anfitrion 2" in out and "segundo del anfitrion" in out, "pull trajo el archivo nuevo")
        out = m.comando("git pull", 300 * LENTO)
        verificar("Ya est" in out, "pull sin novedades: ya actualizado")
        # una rama nueva con -u
        m.comando("git checkout -b rama-sagui; echo r > r.txt; git add r.txt; git commit -m en-rama-sagui", 120)
        out = m.comando("git push -u origin rama-sagui", 300 * LENTO)
        verificar("Enviado rama-sagui" in out, "push -u de una rama nueva")
        verificar("en-rama-sagui" in hgit("log", "--format=%s", "rama-sagui", cwd=bare), "el anfitrión ve la rama nueva")
        # un repositorio que no existe: error limpio
        out = m.comando("git clone ssh://h402@10.0.2.2:%d/no/existe nada; echo codigo=$?" % P, 120)
        verificar("git:" in out and "codigo=0" not in out, "clone de un repositorio inexistente falla con mensaje")
        m.captura("H402-2-ssh")
        verificar("EXCECAO" not in m.serial(), "sin excepciones del kernel")


def https():
    bare, sem = preparar_remoto()
    pem, capem = crear_pki()
    p_https = servidor_http(pem)
    p_http = servidor_http(None)
    url_https = "https://10.0.2.2:%d/remoto.git" % p_https
    url_http = "http://10.0.2.2:%d/remoto.git" % p_http
    with arrancar("h402-https") as m:
        esperar_red(m)
        m.comando('git config --global user.name "Prueba Sagui"; git config --global user.email sagui@example.com', 30)
        out = m.comando("cd /home; git clone %s sin-ca; echo codigo=$?" % url_https, 120)
        verificar("codigo=0" not in out and "git:" in out and "sin-ca" not in m.comando("ls /home/sin-ca/leeme.txt", 20) .replace("sin-ca/leeme", ""),
                  "HTTPS sin la CA: no clona y da error de certificado: %s" % out.strip()[-200:])
        m.comando("curl -s -o /tmp/ca.pem http://10.0.2.2:%d/ca.pem" % p_http, 60)
        out = m.comando("certificados añadir /tmp/ca.pem", 30)
        verificar("Añadido" in out, "certificados añadir la CA de prueba")
        out = m.comando("cd /home; git clone %s web" % url_https, 300 * LENTO)
        verificar("Clonando en 'web'" in out and "git:" not in out, "clone por HTTPS con TLS de SaguiOS: %s" % out.strip()[-200:])
        out = m.comando("cat /home/web/leeme.txt; git -C /home/web log --oneline", 60)
        verificar("hola desde el anfitrion" in out and "primer commit del anfitrion" in out, "el clon por HTTPS tiene archivos e historial")
        m.comando("cd /home/web; echo por-https > h.txt; git add h.txt; git commit -m 'commit por https'", 120)
        out = m.comando("git push", 300 * LENTO)
        verificar("Enviado main" in out and "git:" not in out, "push por HTTPS: %s" % out.strip()[-200:])
        comprobar_remoto(bare, "commit por https")
        hgit("pull", "origin", "main", cwd=sem)
        with open(os.path.join(sem, "h2.txt"), "w") as f:
            f.write("novedad https\n")
        hgit("add", ".", cwd=sem)
        hgit("commit", "-m", "novedad para https", cwd=sem)
        hgit("push", "origin", "main", cwd=sem)
        out = m.comando("git pull", 300 * LENTO)
        verificar("Avance r" in out, "pull por HTTPS: avance rápido (%s)" % out.strip()[-100:])
        out = m.comando("cat h2.txt", 30)
        verificar("novedad https" in out, "pull trajo el archivo")
        out = m.comando("cd /home; git clone %s plano" % url_http, 300 * LENTO)
        verificar("git:" not in out and "Clonando" in out, "clone por HTTP plano")
        out = m.comando("cat /home/plano/h2.txt", 30)
        verificar("novedad https" in out, "el clon por HTTP tiene el último commit")
        out = m.comando("GIT_SSL_NO_VERIFY=1 git clone https://10.0.2.2:%d/remoto.git sinverif; echo codigo=$?" % p_https, 300 * LENTO)
        verificar("codigo=0" in out, "GIT_SSL_NO_VERIFY=1 clona sin verificar")
        print("    peticiones HTTP(S) vistas por el servidor:", len(TratadorGit.eventos))
        m.captura("H402-3-https")
        verificar("EXCECAO" not in m.serial(), "sin excepciones del kernel")


def main():
    if not os.path.exists(FUENTES):
        print("AVISO: faltan las fuentes de libgit2 (tools/git_fuentes.sh): h402 se salta")
        return 0
    partes = sys.argv[1:] or ["pc", "local", "ssh", "https"]
    for p in partes:
        print("== parte %s" % p)
        {"pc": pc, "local": local, "ssh": ssh, "https": https}[p]()
    print("H402 OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
