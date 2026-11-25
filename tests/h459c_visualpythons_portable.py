#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H459c - VisualPythonS y el MicroPython de SaguiOS en Linux (D459, apartado «VisualPythonS»).

Sin QEMU, en el anfitrión. Compila el MicroPython de SaguiOS (los MISMOS fuentes que la imagen: `userland/micropython/`
más la capa `userland/portable/` sobre la SDL 2) y comprueba, con SDL_VIDEODRIVER=dummy y un guion de eventos
(SAGUI_ROTEIRO):

  1. el intérprete: salida UTF-8, math, json, hilos (`_thread` y `threading` sobre pthreads), sqlite3, hashlib,
     red con sockets reales (un servidor HTTP local) y TLS (un servidor HTTPS local con un certificado propio:
     sin verificar, con la CA propia y rechazo sin ella); `os.system` y el sonido dan un error limpio;
  2. las autopruebas de VisualPythonS que ya pasan en SaguiOS (autoprueba.py, autoprueba_disenador.py,
     autoprueba_toast.py y autoprueba_grafico.py);
  3. el diseñador: se pone un botón y una etiqueta, doble clic en el botón, se escribe el manejador, F5 ejecuta el
     formulario y el clic en el botón corre el manejador (la etiqueta cambia en la captura y el programa escribe
     `vps-click`);
  4. los proyectos de /ejemplos/vps (26) arrancan, se dibujan y se cierran.

Con `--repo` genera además el repositorio `visualpythons` con tools/portable_repos.py, lo clona limpio, compila con
`env -u CPATH make` y repite todo con ese binario; comprueba la higiene del repositorio (un commit, autor público, sin
binarios, sin rutas /home/ reales, sin correos ni claves, licencias).

Las capturas salen del búfer de píxeles (sintéticas), en SAIDA/h459c/; con --docs se copia la del diseñador a
docs/capturas/H459c-visualpythons.png.

Uso: h459c_visualpythons_portable.py [--repo] [--docs]
"""
import http.server
import os
import re
import shutil
import ssl
import subprocess
import sys
import tempfile
import threading

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import verificar, RAIZ, SAIDA, CAPTURAS  # noqa: E402

DIR = os.path.join(SAIDA, "h459c")
TRAB = os.path.join(DIR, "trabajo")
B = os.path.join(DIR, "build")


def compilar():
    jobs = os.environ.get("SAGUI_JOBS", "4")
    env = dict(os.environ)
    env.pop("CPATH", None)   # D191
    r = subprocess.run(["make", "-f", os.path.join(RAIZ, "userland", "portable", "Makefile"), "-j" + jobs, "B=" + B, "python"],
                       cwd=RAIZ, capture_output=True, text=True, env=env)
    verificar(r.returncode == 0, "el MicroPython de SaguiOS compila para Linux con SDL 2: " + r.stderr[-800:])
    verificar(os.path.exists(os.path.join(B, "python")), "binario python")
    return os.path.join(B, "python")


class Entorno:
    """El binario, la carpeta de datos (None: la que encuentra solo, junto al ejecutable) y una carpeta de trabajo."""

    def __init__(self, py, datos):
        self.py, self.datos = py, datos
        shutil.rmtree(TRAB, ignore_errors=True)
        os.makedirs(os.path.join(TRAB, "casa"))

    def env(self, guion=None):
        e = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", HOME=os.path.join(TRAB, "casa"), LANG="es_ES.UTF-8")
        e.pop("SAGUI_ROTEIRO", None)
        if self.datos:
            e["SAGUI_DATOS"] = self.datos
        else:
            e.pop("SAGUI_DATOS", None)
        if guion:
            g = os.path.join(TRAB, "guion.txt")
            with open(g, "w", encoding="utf-8") as f:
                f.write("\n".join(guion) + "\n")
            e["SAGUI_ROTEIRO"] = g
        return e

    def correr(self, args, guion=None, timeout=60):
        r = subprocess.run([self.py] + args, capture_output=True, env=self.env(guion), timeout=timeout, cwd=TRAB)
        return r.returncode, r.stdout.decode("utf-8", "replace") + r.stderr.decode("utf-8", "replace")


def png_ok(ruta):
    d = open(ruta, "rb").read()
    verificar(d[:8] == b"\x89PNG\r\n\x1a\n" and len(set(d[100:4000])) > 4, "captura PNG con contenido " + os.path.basename(ruta))


# ------------------------------------------------------------------------------------------------ 1. el intérprete

def servidores(ruta_www):
    """Un servidor HTTP y otro HTTPS (certificado propio, si hay openssl) en el anfitrión, en puertos libres."""
    class H(http.server.SimpleHTTPRequestHandler):
        def __init__(self, *a, **k):
            super().__init__(*a, directory=ruta_www, **k)

        def log_message(self, *a):
            pass
    http_s = http.server.HTTPServer(("127.0.0.1", 0), H)
    threading.Thread(target=http_s.serve_forever, daemon=True).start()
    https_s, cert = None, None
    if shutil.which("openssl"):
        cert = os.path.join(TRAB, "ca.pem")
        clave = os.path.join(TRAB, "clave.pem")
        r = subprocess.run(["openssl", "req", "-x509", "-newkey", "ec", "-pkeyopt", "ec_paramgen_curve:prime256v1", "-nodes",
                            "-keyout", clave, "-out", cert, "-days", "2", "-subj", "/CN=localhost",
                            "-addext", "subjectAltName=IP:127.0.0.1,DNS:localhost"], capture_output=True)
        if r.returncode == 0:
            https_s = http.server.HTTPServer(("127.0.0.1", 0), H)
            c = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
            c.load_cert_chain(cert, clave)
            https_s.socket = c.wrap_socket(https_s.socket, server_side=True)
            threading.Thread(target=https_s.serve_forever, daemon=True).start()
    return http_s, https_s, cert


def prueba_interprete(e):
    www = os.path.join(TRAB, "www")
    os.makedirs(www)
    open(os.path.join(www, "hola.txt"), "w").write("hola desde el servidor\n")
    rc, s = e.correr(["-c", "import sys, math, json; print(sys.platform, math.sqrt(2), json.dumps({'a': [1, 2]})); print('ñandú €')"])
    verificar(rc == 0 and "saguios" in s and "1.414213562373095" in s and '{"a": [1, 2]}' in s, "intérprete: sys, math y json: " + s)
    verificar("ñandú €" in s, "la salida es UTF-8 en el terminal del PC: %r" % s)
    p = subprocess.run([e.py, "-c", "print(input('x> ') + 'ñ')"], input="hola ú\n".encode(), capture_output=True, env=e.env(), cwd=TRAB)
    verificar(p.returncode == 0 and p.stdout.decode().strip() == "x> hola úñ", "la entrada estándar también es UTF-8: %r" % p.stdout)

    rc, s = e.correr(["-c", """
import threading, time, _thread, gc
res = []
def w(n):
    for i in range(5):
        time.sleep(0.005)
        res.append(str(n) * 50)
ts = [threading.Thread(target=w, args=(i,)) for i in range(4)]
[t.start() for t in ts]
basura = [[str(i)] * 20 for i in range(30000)]
gc.collect()
[t.join() for t in ts]
print('hilos', len(res), len(basura))
"""])
    verificar(rc == 0 and "hilos 20 30000" in s, "hilos (threading sobre pthreads) con recolección de basura a la vez: " + s)

    rc, s = e.correr(["-c", """
import sqlite3, hashlib
c = sqlite3.connect('%s')
c.execute('create table t(a, b)'); c.execute("insert into t values(1, 'x'), (2, 'y')"); c.commit()
print('sqlite', c.execute('select count(*), sum(a) from t').fetchall()); c.close()
print(hashlib.sha256(b'abc').hexdigest()[:16], hashlib.sha1(b'abc').hexdigest()[:8], hashlib.sha512(b'abc').hexdigest()[:8])
""" % os.path.join(TRAB, "prueba.db")])
    verificar(rc == 0 and "sqlite [(2, 3)]" in s and "ba7816bf8f01cfea a9993e36 ddaf35a1" in s, "sqlite3 y hashlib: " + s)

    http_s, https_s, cert = servidores(www)
    try:
        rc, s = e.correr(["-c", "import requests; r = requests.get('http://127.0.0.1:%d/hola.txt'); print('http', r.status_code, r.text.strip())"
                          % http_s.server_address[1]])
        verificar(rc == 0 and "http 200 hola desde el servidor" in s, "red: requests sobre los sockets del PC: " + s)
        if https_s:
            rc, s = e.correr(["-c", """
import requests
u = 'https://127.0.0.1:%d/hola.txt'
print('sin verificar', requests.get(u, verify=False).status_code)
print('con CA propia', requests.get(u, verify=%r).text.strip())
try:
    requests.get(u)
    print('SIN CA: no debia conectar')
except Exception as ex:
    print('sin CA rechazada', type(ex).__name__)
""" % (https_s.server_address[1], cert)])
            verificar(rc == 0 and "sin verificar 200" in s and "con CA propia hola desde el servidor" in s and "sin CA rechazada" in s,
                      "TLS (TLS 1.3 contra OpenSSL): sin verificar, con CA propia y rechazo sin ella: " + s)
        else:
            print("  (TLS omitido: no hay openssl)")
    finally:
        http_s.shutdown()
        if https_s:
            https_s.shutdown()

    rc, s = e.correr(["-c", """
import os, sagui
try:
    os.system('true')
except OSError as ex:
    print('system: error limpio', ex.errno)
try:
    sagui.Sonido(440, 100)
except OSError as ex:
    print('sonido: error limpio')
print(len(os.environ) > 3, os.listdir('/ejemplos/vps')[0])
"""])
    verificar(rc == 0 and "system: error limpio" in s and "sonido: error limpio" in s and "True abrir-guardar" in s,
              "lo que pide el kernel da un error limpio (procesos, sonido): " + s)


# ------------------------------------------------------------------------------------------------ 2. autopruebas

def prueba_autopruebas(e):
    # D466 (toast, Arbol, Calendario) y D469 (Grafico): los mismos mínimos que h466 y h469 en SaguiOS
    for nombre, minimo in (("autoprueba", 150), ("autoprueba_disenador", 100), ("autoprueba_toast", 90), ("autoprueba_grafico", 40)):
        rc, s = e.correr(["/lib/python/vps/%s.py" % nombre], timeout=120)
        fallos = re.findall(r"vps-autoprueba: FALLO (.*)", s)
        oks = s.count(": ok ")
        verificar(rc == 0 and not fallos and oks >= minimo, "%s: %d comprobaciones, fallos %s (rc=%d) %s" % (nombre, oks, fallos[:3], rc, s[-300:] if rc else ""))


# ------------------------------------------------------------------------------------------------ 3. el diseñador

def posiciones(e):
    """Lo que imprime el diseñador con --sonda: origen del formulario y posición de las herramientas."""
    rc, s = e.correr(["/lib/python/vps/disenador.py", "--sonda"], guion=["esperar 500", "sair", "esperar 400", "texto \\t\\n"], timeout=30)
    r = {}
    for l in s.splitlines():
        p = l.split("|")
        if l.startswith("vps-sonda: cliente"):
            r["cliente"] = (int(p[1]), int(p[2]))
        elif l.startswith("vps-sonda: herr"):
            r[p[1]] = (int(p[2]), int(p[3]))
    verificar("cliente" in r and "Boton" in r and "Etiqueta" in r, "el diseñador da sus posiciones (--sonda): " + s[:300])
    return r


def prueba_disenador(e, docs=False):
    pos = posiciones(e)
    cx, cy = pos["cliente"]
    capt = {k: os.path.join(DIR, "H459c-%s.png" % k) for k in ("visualpythons", "codigo", "antes", "despues")}
    guion = [
        "esperar 800",
        "clique %d %d" % pos["Boton"],
        "arrastar %d %d %d %d" % (cx + 16, cy + 16, cx + 120, cy + 48),        # un botón arrastrando
        "esperar 200",
        "clique %d %d" % pos["Etiqueta"],
        "clique %d %d" % (cx + 16, cy + 72),                                    # una etiqueta con un clic
        "esperar 300",
        "captura " + capt["visualpythons"],
        "duplo %d %d" % (cx + 16 + 52, cy + 16 + 16),                           # doble clic en el botón: su editor de código
        "esperar 800",
        "captura " + capt["codigo"],
        "tecla 0x109", "tecla 0x109", "tecla 0x109", "tecla 0x109",             # quita el «pass»
        'texto Etiqueta1.texto = "Hola ñ"\\nprint("vps-click")',
        "esperar 300",
        "tecla 0x10E",                                                          # F5: ejecutar
        "esperar 1500",
        "captura " + capt["antes"],
        "clique 68 32",                                                         # el botón del formulario en marcha
        "esperar 500",
        "captura " + capt["despues"],
        "sair", "esperar 300",                                                  # cierra el formulario
        "sair", "esperar 300",                                                  # esconde el editor
        "sair", "esperar 600",                                                  # cierra el diseñador: ¿guardar? ...
        "clique 225 80", "esperar 600",                                         # ... «No»
    ]
    rc, s = e.correr(["/lib/python/vps/disenador.py"], guion=guion, timeout=60)
    verificar(rc == 0, "el diseñador termina bien (rc=%d): %s" % (rc, s[-400:]))
    verificar("vps: ejecutando" in s and "vps-click" in s, "F5 ejecuta el formulario y el clic corre Boton1_click (vps-click): " + s[-300:])
    for c in capt.values():
        png_ok(c)
    a, d = open(capt["antes"], "rb").read(), open(capt["despues"], "rb").read()
    verificar(a != d, "la captura del formulario cambia al pulsar el botón (la etiqueta pasa a «Hola ñ»)")
    verificar(open(capt["codigo"], "rb").read() != open(capt["antes"], "rb").read(), "el editor de código es otra ventana")
    if docs:
        shutil.copy(capt["visualpythons"], os.path.join(CAPTURAS, "H459c-visualpythons.png"))


# ------------------------------------------------------------------------------------------------ 4. los ejemplos

def prueba_ejemplos(e):
    base = os.path.join(RAIZ, "rootfs", "ejemplos", "vps")
    nombres = sorted(d for d in os.listdir(base) if os.path.exists(os.path.join(base, d, "lanzador.py")))
    verificar(len(nombres) >= 20, "hay al menos 20 proyectos de ejemplo (%d)" % len(nombres))
    for n in nombres:
        cap = os.path.join(DIR, "ej-%s.png" % n)
        rc, s = e.correr(["/ejemplos/vps/%s/lanzador.py" % n], guion=["esperar 500", "captura " + cap, "sair"], timeout=30)
        verificar(rc == 0 and "Traceback" not in s, "ejemplo %s arranca y se cierra (rc=%d) %s" % (n, rc, s[-200:]))
        png_ok(cap)
    rc, s = e.correr(["/ejemplos/vps/hola.py"], guion=["esperar 400", "sair"])
    verificar(rc == 0 and "vps: cargado" in s, "/ejemplos/vps/hola.py: " + s)


# ------------------------------------------------------------------------------------------------ el repositorio

def higiene_repo(repo):
    git = lambda *a: subprocess.run(["git", "-C", repo, *a], capture_output=True, text=True, check=True).stdout
    verificar(git("rev-list", "--count", "HEAD").strip() == "1", "un único commit")
    # el correo no va en el código (publicar.sh no deja correos): el mismo nombre y un noreply de GitHub en los dos
    quem = git("log", "-1", "--format=%an <%ae>|%cn <%ce>").strip().split("|")
    verificar(len(quem) == 2 and quem[0] == quem[1] and
              re.fullmatch(r"aleanun89 <(\d+\+)?aleanun89@users\.noreply\.github\.com>", quem[0]) is not None, "autor público")
    verificar("Co-Authored-By" not in git("log", "-1", "--format=%B"), "sin firma de IA")
    ficheros = git("ls-files").splitlines()
    for f in ("LICENSE", "README.md", "AVISOS-TERCEROS.md", "Makefile", "CMakeLists.txt", ".gitignore", "docs/visualpythons.png",
              "docs/visualpythons-manual.md", "docs/capturas/VPS-manual-disenador.png", "userland/micropython/LICENCA-MICROPYTHON.txt", "userland/terceros/sqlite/LICENCA-SQLITE.txt",
              "datos/lib/python/vps/disenador.py", "datos/ejemplos/vps/calculadora/lanzador.py", "LICENSES/GPL-2.0.txt"):
        verificar(f in ficheros, "el repositorio lleva " + f)
    verificar(not any(f.startswith(("userland/micropython/libc/", "userland/micropython/libm_dbl/")) for f in ficheros),
              "sin la libc ni la libm de 32 bits de SaguiOS")
    verificar(not any("chip8roms" in f for f in ficheros), "sin chip8roms.py")
    for f in ficheros:
        d = open(os.path.join(repo, f), "rb").read()
        verificar(not d.startswith((b"\x7fELF", b"MZ", b"!<arch>")) and not f.endswith((".o", ".a", ".so", ".exe", ".img", ".iso")),
                  "sin binarios ejecutables: " + f)
        if f.endswith(".png") or f.endswith(".ttf") or f.endswith(".bmp") or f.endswith(".pgm") or f.endswith(".ppm"):
            continue
        t = d.decode("latin-1")
        verificar("/home/aanun" not in t and not re.search(r"aanun(?!89)", t.replace("aleanun89", "")), "sin rutas ni nombres personales: " + f)
        verificar(not re.search(r"[\w.+-]+@[\w-]+\.[\w.-]+", t.replace("users.noreply.github.com", "").replace("poly1305@openssh.com", "")) or f.startswith(("userland/terceros/sqlite", "userland/micropython/micropython_embed", "datos/fontes", "LICENSE", "LICENSES", "datos/etc")),
                  "sin correos: " + f + " " + str(re.findall(r"[\w.+-]+@[\w-]+\.[\w.-]+", t)[:2]))
        verificar(not re.search(r"ghp_[A-Za-z0-9]{20}|github_pat_|BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY|AKIA[0-9A-Z]{16}", t), "sin claves ni tokens: " + f)
        verificar(not re.search(r"\b(192\.168|10\.\d{1,3})\.\d{1,3}\.\d{1,3}\b", re.sub(r"10\.0\.2\.\d+", "", t)) or   # 10.0.2.x: la red por omisión de QEMU, citada en la documentación
                   f.startswith(("userland/terceros/sqlite", "userland/micropython/micropython_embed", "datos/etc")),
                  "sin IP privadas: " + f)
    verificar(not any("input_share" in f for f in ficheros), "sin datos de input_share")
    manual = open(os.path.join(repo, "docs/visualpythons-manual.md"), encoding="utf-8").read()
    for c in re.findall(r"\(capturas/([A-Za-z0-9_.-]+\.png)\)", manual):
        verificar(os.path.exists(os.path.join(repo, "docs/capturas", c)), "el manual enlaza una captura que existe: " + c)


def prueba_repo():
    destino = os.path.join(DIR, "repos")
    clon = os.path.join(DIR, "clon")
    shutil.rmtree(destino, ignore_errors=True)
    shutil.rmtree(clon, ignore_errors=True)
    os.makedirs(destino)
    r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "portable_repos.py"), destino, "visualpythons"], capture_output=True, text=True)
    verificar(r.returncode == 0, "portable_repos.py genera el repositorio: " + r.stdout + r.stderr[-400:])
    repo = os.path.join(destino, "visualpythons")
    higiene_repo(repo)
    subprocess.run(["git", "clone", "-q", repo, clon], check=True)
    env = dict(os.environ)
    env.pop("CPATH", None)
    r = subprocess.run(["make", "-j" + os.environ.get("SAGUI_JOBS", "4")], cwd=clon, capture_output=True, text=True, env=env)
    verificar(r.returncode == 0 and os.path.exists(os.path.join(clon, "build/portable/python")), "el clon limpio compila con `env -u CPATH make`: " + r.stderr[-600:])
    r = subprocess.run(["make", "probar"], cwd=clon, capture_output=True, text=True, env=env, timeout=240)
    verificar(r.returncode == 0 and "todos arrancan" in r.stdout, "make probar en el clon: " + r.stdout[-400:] + r.stderr[-300:])
    return Entorno(os.path.join(clon, "build/portable/python"), None)


def bateria(e, docs=False):
    prueba_interprete(e)
    prueba_autopruebas(e)
    prueba_disenador(e, docs)
    prueba_ejemplos(e)


def main():
    os.makedirs(DIR, exist_ok=True)
    verificar(shutil.which("sdl2-config") is not None, "SDL 2 de desarrollo instalada (sdl2-config)")
    py = compilar()
    bateria(Entorno(py, os.path.join(RAIZ, "rootfs")), "--docs" in sys.argv)
    if "--repo" in sys.argv:
        e = prueba_repo()
        bateria(e)
    print("H459c OK")


if __name__ == "__main__":
    main()
