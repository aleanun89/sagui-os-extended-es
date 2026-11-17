#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H364 (D362) - DEBUG-APP: depuración remota con un código de un solo uso.

En el anfitrión:
  1. tests/debug_app_unidad.c (también con ASan/UBSan): el código (ida y vuelta,
     letras confusas, cada letra cambiada la detecta la suma, otra versión), la
     semilla y las direcciones permitidas (subred, redes, públicas, errores);
  2. lo mismo comparado con tools/depurar-remoto.py en casos al azar (codificar en
     C y decodificar en Python y al revés, semilla, direcciones), su Ed25519 contra
     la biblioteca «cryptography» (si está) y su clave privada leída por ssh-keygen.

En QEMU (red de usuario con hostfwd al 2222 de SaguiOS; el anfitrión hace de PC
del mantenedor con su OpenSSH, por tools/depurar-remoto.py --host/--puerto, sin
suponer ninguna IP de la red local):
  - sin ser root (su sagui -c ...) no se abre y no hay nadie en el puerto;
  - una lista con una red pública sin «publicas» se rechaza;
  - debug-app --texto: el código, la dirección y la huella (la de sshd --huella);
    «info» del código da 10.0.2.15:2222; dmesg anota la apertura;
  - un código con otra huella (suma bien hecha): el cliente se niega a conectar;
  - conectar, ejecutar (salida y código), subir y bajar con sumas iguales, recoger
    (dmesg, infopc...) a la carpeta pedida;
  - mientras dura, otra conexión con la misma clave se rechaza (una a la vez);
  - al cerrar la conexión, el código ya no vale: la sesión se acaba sola;
  - tres claves malas anulan el código (y después tampoco entra la buena);
  - con --permitir de otra red, el anfitrión (10.0.2.2) no entra;
  - Intro corta una sesión con alguien dentro (la conexión cae) y caduca sola;
  - el registro /home/.config/debug-app/debug-app.log tiene las órdenes; sin el
    secreto; sshd normal sigue sin arrancar.
"""
import importlib.util
import hashlib
import os
import random
import re
import shutil
import socket
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, FalhaTeste  # noqa: E402

TRAB = os.path.join(SAIDA, "h364")
HERR = os.path.join(RAIZ, "tools", "depurar-remoto.py")


def modulo():
    spec = importlib.util.spec_from_file_location("depurar_remoto", HERR)
    m = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(m)
    return m


DR = modulo()


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


# ------------------------------------------------------------ anfitrión

def unidad():
    exe = os.path.join(TRAB, "debug_app_unidad")
    fontes = [os.path.join(RAIZ, "tests", "debug_app_unidad.c"), os.path.join(RAIZ, "kernel", "lib", "sha256.c")]
    base = ["gcc", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "userland", "inc"),
            "-I", os.path.join(RAIZ, "kernel", "inc"), "-I", os.path.join(RAIZ, "userland", "programas")]
    r = subprocess.run(base + ["-O2", "-o", exe] + fontes, capture_output=True, text=True)
    verificar(r.returncode == 0 and "warning" not in r.stderr, "debug_app_unidad.c compila sin avisos" + r.stderr[:2000])
    r = subprocess.run([exe], capture_output=True, text=True, timeout=60)
    n = re.search(r"debug_app_unidad: (\d+)/(\d+) comprobaciones correctas", r.stdout)
    verificar(r.returncode == 0 and n and n.group(1) == n.group(2), f"lógica de DEBUG-APP en C: {r.stdout.strip()[-400:]}")
    r = subprocess.run(base + ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-o", exe + "-asan"] + fontes,
                       capture_output=True, text=True)
    if r.returncode == 0:
        r = subprocess.run([exe + "-asan"], capture_output=True, text=True, timeout=120)
        verificar(r.returncode == 0, "lo mismo con -fsanitize=address,undefined" + r.stderr[-1500:])
    else:
        print("  AVISO: sin los sanitizadores de gcc en este anfitrión; se omite")

    p = subprocess.Popen([exe, "ordenes"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    def o(linea):
        p.stdin.write(linea + "\n")
        p.stdin.flush()
        return p.stdout.readline().strip()

    rnd = random.Random(364)
    for i in range(200):
        ip = ".".join(str(rnd.randrange(256)) for _ in range(4))
        porta = rnd.randrange(65536)
        seg, hu = bytes(rnd.randrange(256) for _ in range(8)), bytes(rnd.randrange(256) for _ in range(8))
        c = o(f"codificar {ip} {porta} {seg.hex()} {hu.hex()}")
        if c != DR.codificar(ip, porta, seg, hu) or DR.decodificar(c) != (ip, porta, seg, hu):
            raise FalhaTeste(f"C y Python codifican distinto (caso {i}): {c}")
        if o(f"decodificar {DR.codificar(ip, porta, seg, hu)}") != f"{ip} {porta} {seg.hex()} {hu.hex()}":
            raise FalhaTeste(f"C no decodifica lo de Python (caso {i})")
        if o(f"semilla {seg.hex()}") != DR.semente(seg).hex():
            raise FalhaTeste(f"semilla distinta (caso {i})")
        blob = DR.blob_publica(bytes(rnd.randrange(256) for _ in range(32)))
        if o(f"huella {seg.hex()} {hashlib.sha256(blob).hexdigest()}") != DR.huella_codigo(seg, blob).hex():
            raise FalhaTeste(f"huella distinta (caso {i})")
        # una letra cambiada: los dos la rechazan
        k = rnd.randrange(len(c))
        if c[k] != "-":
            mala = c[:k] + ("A" if c[k] != "A" else "B") + c[k + 1:]
            try:
                DR.decodificar(mala)
                raise FalhaTeste(f"Python acepta un código con una letra cambiada ({mala})")
            except DR.CodigoMal:
                pass
            if not o(f"decodificar {mala}").startswith("error"):
                raise FalhaTeste(f"C acepta un código con una letra cambiada ({mala})")
    verificar(True, "C y tools/depurar-remoto.py codifican, decodifican y derivan la semilla y la huella igual (200 casos al azar)")
    casos = [("subred", "10.0.2.2", "10.0.2.15", "255.255.255.0", "1"),
             ("subred", "10.0.3.2", "10.0.2.15", "255.255.255.0", "0"),
             ("192.168.77.0/24", "10.0.2.2", "10.0.2.15", "255.255.255.0", "0"),
             ("8.8.8.0/24", "8.8.8.8", "10.0.2.15", "255.255.255.0", "error"),
             ("8.8.8.0/24,publicas", "8.8.8.8", "10.0.2.15", "255.255.255.0", "1")]
    for lista, cli, meu, masc, esperado in casos:
        verificar(o(f"permitido {lista} {cli} {meu} {masc}") == esperado, f"permitido {lista} {cli}: {esperado}")
    p.stdin.close()
    p.wait(timeout=10)

    try:
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        from cryptography.hazmat.primitives import serialization as ser
        for i in range(16):
            sem = os.urandom(32)
            pub = Ed25519PrivateKey.from_private_bytes(sem).public_key().public_bytes(ser.Encoding.Raw, ser.PublicFormat.Raw)
            if DR.ed25519_publica(sem) != pub:
                raise FalhaTeste("Ed25519 de depurar-remoto.py distinto de cryptography")
        verificar(True, "la clave pública Ed25519 de depurar-remoto.py es la de cryptography (16 casos)")
    except ImportError:
        print("  AVISO: sin la biblioteca cryptography de Python; se omite la comparación")
    sem = os.urandom(32)
    pub = DR.ed25519_publica(sem)
    arq = os.path.join(TRAB, "clave_prueba")
    fd = os.open(arq, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600)
    with os.fdopen(fd, "w") as f:
        f.write(DR.chave_openssh(sem, pub))
    r = subprocess.run(["ssh-keygen", "-y", "-f", arq], capture_output=True, text=True)
    import base64
    verificar(r.returncode == 0 and base64.b64encode(DR.blob_publica(pub)).decode() in r.stdout,
              "ssh-keygen lee la clave privada que escribe depurar-remoto.py")
    os.remove(arq)


# ------------------------------------------------------------ QEMU

def construir_imagen():
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(TRAB, "rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    sgfs = os.path.join(TRAB, "h364.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(TRAB, "h364.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--console", "--dados-mb", "16"],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def esperar(m, padrao, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(padrao, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.2)
    raise FalhaTeste(f"no apareció en la serie: {padrao!r}")


def banner(porta, tempo=4):
    try:
        c = socket.create_connection(("127.0.0.1", porta), timeout=tempo)
    except OSError:
        return b""
    c.settimeout(tempo)
    try:
        return c.recv(64)
    except OSError:
        return b""
    finally:
        c.close()


CARPETAS = []


class Herramienta:
    """tools/depurar-remoto.py con su propia carpeta de control (no la del usuario)."""

    def __init__(self, porta, nome):
        self.porta = porta
        # carpeta corta (el socket de control de OpenSSH no admite rutas largas) y propia de la prueba
        self.env = dict(os.environ, XDG_RUNTIME_DIR=tempfile.mkdtemp(prefix=f"h364{nome}-", dir="/tmp"))
        CARPETAS.append(self.env["XDG_RUNTIME_DIR"])

    def __call__(self, codigo, *orden, tempo=180, entrada=None):
        cmd = [sys.executable, HERR, "--host", "127.0.0.1", "--puerto", str(self.porta), "env"] + list(orden)
        env = dict(self.env, DEBUG_APP_CODIGO=codigo)
        return subprocess.run(cmd, capture_output=True, text=True, timeout=tempo, env=env, input=entrada,
                              stdin=None if entrada is not None else subprocess.DEVNULL)


def abrir(m, opciones="--segundos 900"):
    """Lanza debug-app --texto (se queda en primer plano) y devuelve el código y la huella."""
    desde = len(m.serial())
    m.digitar(f"debug-app --texto {opciones}", 0.04)
    m.tecla("ret")
    g = esperar(m, r"C.digo: ([0-9A-Z]{5}(?:-[0-9A-Z]{5}){7})", 60, desde)
    h = esperar(m, r"Huella del servidor: (SHA256:[A-Za-z0-9+/]{43})", 10, desde)
    return g.group(1), h.group(1), desde


def volver_prompt(m, desde, tempo=30):
    n = m.serial()[:desde].count("sagui:")
    fim = time.time() + tempo
    while m.serial().count("sagui:") <= n:
        if time.time() > fim:
            raise FalhaTeste("debug-app no volvió al prompt")
        time.sleep(0.2)


def maquina():
    img = construir_imagen()
    porta = puerto_libre()
    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{porta}-:2222"]
    with Maquina("h364", "ide", imagem=img, snapshot=False, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        esperar(m, r"10\.0\.2\.15 \(DHCP\)", 30)

        # --- sin ser root
        desde = len(m.serial())
        m.comando("su sagui -c 'debug-app --texto'", 60)
        # (el registro de su dice «-> sagui:», que m.comando toma por el prompt: se espera el mensaje)
        esperar(m, r"debug-app: hace falta ser administrador", 60, desde)
        esperar(m, r"processo \d+ \(su\) terminou", 30, desde)
        time.sleep(1.5)   # que llegue el prompt de después de su
        verificar(True, "sin ser root, debug-app no abre la sesión")
        verificar(banner(porta) == b"", "sin ser root no hay nadie en el puerto")
        out = m.comando("debug-app --texto --permitir 8.8.8.0/24", 60)
        verificar("no es una red privada" in out, "una red pública sin «publicas» se rechaza")
        out = m.comando("sshd --huella")
        huella_sshd = re.search(r"(SHA256:[A-Za-z0-9+/]{43}) \(ED25519\)", out).group(1)

        # --- abrir
        codigo, huella, desde = abrir(m)
        verificar(huella == huella_sshd, f"la huella que enseña DEBUG-APP es la de sshd --huella ({huella})")
        verificar(re.search(r"Direcci.n: 10\.0\.2\.15  puerto 2222", m.serial()[desde:]), "dirección y puerto en claro")
        verificar("ADMINISTRADOR (root)" in m.serial()[desde:], "avisa de que entra como root")
        verificar("debug-app: sesi" in m.serial()[desde:] and "ABIERTA" in m.serial()[desde:], "el registro del sistema anota la apertura")
        ip, pt, seg, hu = DR.decodificar(codigo)
        verificar((ip, pt) == ("10.0.2.15", 2222), f"el código lleva la dirección y el puerto ({ip}:{pt})")
        verificar(banner(porta).startswith(b"SSH-2.0-SaguiOS_sshd"), "hay un sshd en el 2222")
        h1 = Herramienta(porta, "a")
        r = h1(codigo, "info")
        verificar("10.0.2.15" in r.stdout and "2222" in r.stdout and seg.hex() not in r.stdout, "info: dirección y puerto, sin el secreto")

        # --- otra huella: no conecta
        malo = DR.codificar(ip, pt, seg, bytes(8))
        r = Herramienta(porta, "huella")(malo, "ejecutar", "echo no")
        verificar(r.returncode != 0 and "NO es la del código" in r.stderr, f"con otra huella no se conecta ({r.stderr[-200:]!r})")

        # --- conectar y trabajar
        r = h1(codigo, "ejecutar", "echo hola-debug; uname")
        verificar(r.returncode == 0 and "hola-debug" in r.stdout and "SaguiOS" in r.stdout, f"ejecutar: {r.stdout!r} {r.stderr[-300:]!r}")
        r = h1(codigo, "ejecutar", "id")
        verificar("uid=0(root)" in r.stdout, f"la sesión es root ({r.stdout.strip()!r})")
        r = h1(codigo, "ejecutar", "false")
        verificar(r.returncode == 1, f"el código de salida de la orden llega ({r.returncode})")
        local = os.path.join(TRAB, "datos.bin")
        with open(local, "wb") as f:
            f.write(os.urandom(150000))
        r = h1(codigo, "subir", local, "/tmp/datos.bin")
        verificar(r.returncode == 0, f"subir ({r.stderr[-200:]!r})")
        vuelta = os.path.join(TRAB, "vuelta.bin")
        if os.path.exists(vuelta):
            os.remove(vuelta)
        r = h1(codigo, "bajar", "/tmp/datos.bin", vuelta)
        verificar(r.returncode == 0 and open(vuelta, "rb").read() == open(local, "rb").read(), "bajar: el fichero vuelve igual")
        res = os.path.join(TRAB, "resultados")
        shutil.rmtree(res, ignore_errors=True)
        r = subprocess.run([sys.executable, HERR, "--host", "127.0.0.1", "--puerto", str(porta), "--resultados", res,
                            "--maquina", "qemu", "env", "recoger"], capture_output=True, text=True, timeout=600,
                           env=dict(h1.env, DEBUG_APP_CODIGO=codigo), stdin=subprocess.DEVNULL)
        carpeta = r.stdout.strip().splitlines()[-1] if r.stdout.strip() else ""
        verificar(r.returncode == 0 and os.path.isdir(carpeta) and os.path.basename(carpeta).startswith("qemu-"),
                  f"recoger deja una carpeta ({carpeta}) {r.stderr[-300:]!r}")
        dm = open(os.path.join(carpeta, "dmesg.txt"), "rb").read()
        verificar(len(dm) > 1000, "recoger: dmesg")
        verificar(os.path.getsize(os.path.join(carpeta, "infopc.txt")) > 100, "recoger: infopc -t")
        todo = b"".join(open(os.path.join(carpeta, f), "rb").read() for f in os.listdir(carpeta))
        verificar(seg.hex().encode() not in todo and codigo.encode() not in todo, "en lo recogido no está el secreto ni el código")
        verificar(re.search(r"Conexi.n de 10\.0\.2\.2", m.serial()[desde:]) or "CONECTADO desde 10.0.2.2" in m.serial()[desde:],
                  "debug-app dice quién se ha conectado")

        # --- una sola conexión: la misma clave desde otra carpeta (sin la maestra) no entra
        r = Herramienta(porta, "b")(codigo, "ejecutar", "echo dos")
        verificar(r.returncode != 0 and "dos" not in r.stdout, f"una segunda conexión con el mismo código se rechaza ({r.stderr[-200:]!r})")
        # --- al cerrar, el código ya no vale y la sesión acaba
        h1(codigo, "cerrar")
        g = esperar(m, r"Sesi.n cerrada: ([^\n]*)", 30, desde)
        verificar("ya no vale" in g.group(1), f"tras la conexión, la sesión se cierra sola ({g.group(1)})")
        volver_prompt(m, desde)
        verificar("CERRADA" in m.serial()[desde:], "el registro del sistema anota el cierre")
        r = Herramienta(porta, "c")(codigo, "ejecutar", "echo tres")
        verificar(r.returncode != 0, "el mismo código otra vez: no entra (no hay sesión)")
        verificar(banner(porta) == b"", "y el puerto queda cerrado")

        # --- tres claves malas anulan el código
        codigo, huella, desde = abrir(m)
        ip, pt, seg, hu = DR.decodificar(codigo)
        # la huella va atada al secreto: con otro secreto, el cliente ya no conectaría;
        # para probar claves malas en el servidor, cada código malo lleva la huella de su secreto
        blob, _ = DR.ler_chave_servidor("127.0.0.1", porta)
        verificar(blob is not None and DR.huella_codigo(seg, blob) == hu, "la huella del código es la de la clave del servidor con ese secreto")
        malo = DR.codificar(ip, pt, bytes([9] * 8), hu)
        r = Herramienta(porta, "malsecreto")(malo, "ejecutar", "echo no")
        verificar(r.returncode != 0 and "NO es la del código" in r.stderr,
                  f"otro secreto con la huella vieja: el cliente no llega a probar la clave ({r.stderr[-200:]!r})")
        for i in range(3):
            sm = bytes([i + 1] * 8)
            malo = DR.codificar(ip, pt, sm, DR.huella_codigo(sm, blob))
            r = Herramienta(porta, f"mal{i}")(malo, "ejecutar", "echo no")
            verificar(r.returncode != 0 and "no acepta la clave" in r.stderr, f"clave mala {i + 1}: rechazada ({r.stderr[-200:]!r})")
        g = esperar(m, r"Sesi.n cerrada: ([^\n]*)", 30, desde)
        verificar("anulado" in g.group(1), f"tres claves malas anulan el código ({g.group(1)})")
        volver_prompt(m, desde)
        r = Herramienta(porta, "d")(codigo, "ejecutar", "echo no")
        verificar(r.returncode != 0, "después, el código bueno tampoco entra")

        # --- otra red permitida: el anfitrión no entra
        codigo, huella, desde = abrir(m, "--segundos 900 --permitir 192.168.77.0/24")
        r = Herramienta(porta, "e")(codigo, "ejecutar", "echo no")
        verificar(r.returncode != 0, "con --permitir de otra red, 10.0.2.2 no entra")
        esperar(m, r"rechazada: 10\.0\.2\.2 direcci.n no permitida", 20, desde)
        m.tecla("ret")
        g = esperar(m, r"Sesi.n cerrada: ([^\n]*)", 30, desde)
        verificar("cortada" in g.group(1), f"Intro corta la sesión ({g.group(1)})")
        volver_prompt(m, desde)

        # --- cortar con alguien dentro
        codigo, huella, desde = abrir(m)
        h2 = Herramienta(porta, "f")
        r = h2(codigo, "ejecutar", "echo dentro")
        verificar("dentro" in r.stdout, "conectado otra vez con un código nuevo")
        m.tecla("ret")
        g = esperar(m, r"Sesi.n cerrada: ([^\n]*)", 30, desde)
        verificar("cortada" in g.group(1), f"Intro corta con alguien dentro ({g.group(1)})")
        volver_prompt(m, desde)
        time.sleep(2)
        r = h2(codigo, "ejecutar", "echo sigue")
        verificar(r.returncode != 0 and "sigue" not in r.stdout, "tras cortar, la conexión ya no sirve")
        h2(codigo, "cerrar")

        # --- caduca sola
        codigo, huella, desde = abrir(m, "--segundos 8")
        g = esperar(m, r"Sesi.n cerrada: ([^\n]*)", 40, desde)
        verificar("caduc" in g.group(1), f"la sesión caduca sola ({g.group(1)})")
        volver_prompt(m, desde)

        # --- registro
        out = m.comando("cat /home/.config/debug-app/debug-app.log", 30)
        verificar("orden: echo hola-debug; uname" in out, "el registro tiene las órdenes ejecutadas")
        verificar("ABIERTA" in out and "CERRADA" in out and "anulado" in out and "caduc" in out, "y las aperturas y cierres")
        verificar(seg.hex() not in out, "el registro no tiene el secreto")
        out = m.comando("servicio listar")
        verificar(re.search(r"^sshd\s+parado", out, re.M), "el servicio sshd normal sigue parado")


def main():
    os.makedirs(TRAB, exist_ok=True)
    unidad()
    try:
        maquina()
    finally:
        for c in CARPETAS:
            shutil.rmtree(c, ignore_errors=True)
    print("H364 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print(f"FALLO: {e}")
        sys.exit(1)
