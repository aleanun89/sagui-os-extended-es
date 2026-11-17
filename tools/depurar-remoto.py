#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""depurar-remoto.py - el otro lado de DEBUG-APP (D362): entra por SSH en un
SaguiOS que ha abierto una sesión de depuración y le pasa pruebas.

  depurar-remoto.py [OPCIONES] CÓDIGO ORDEN [ARGUMENTOS]

CÓDIGO es el que enseña DEBUG-APP (40 letras en grupos, con o sin guiones).
Con «-» se lee de la entrada estándar (en un terminal se pide sin eco) y con
«env» de la variable DEBUG_APP_CODIGO: así no queda en el historial del shell
ni en la línea de órdenes que ve «ps». Después de la primera orden el código
ya está gastado (la conexión maestra sigue abierta), pero la primera vez,
mejor «-» o «env».

Órdenes:
  info                     lo que dice el código (dirección, puerto, huella), sin el secreto
  conectar                 abre la conexión (lo hacen solas las demás órdenes)
  ejecutar "ORDEN"         ejecuta ORDEN en SaguiOS (como root); sale con su código
  shell                    un sagsh interactivo
  recoger                  dmesg, infopc -t, acel2d/gpu3d probar --detalle, uname, lspci...
                           a RESULTADOS/<máquina>-<fecha>/
  subir FICHERO [DESTINO]  copia un fichero a SaguiOS (de serie a /tmp/)
  bajar RUTA [LOCAL]       trae un fichero de SaguiOS
  cerrar                   corta la conexión (la sesión de DEBUG-APP termina con ella)

Opciones:
  --host H         conectar a H en vez de la dirección del código (NAT, reenvío de puertos)
  --puerto N       idem con el puerto
  --resultados D   carpeta de «recoger» (de serie, pruebas-remotas/ en la raíz del repositorio)
  --maquina NOMBRE nombre de la carpeta de «recoger» (de serie, el de SaguiOS y su dirección)

Cómo funciona: el secreto del código es la semilla de una clave ed25519 de un
solo uso (SHA-256("SaguiOS DEBUG-APP v1" | secreto)); SaguiOS solo acepta esa
clave y solo una vez. Antes de entregar nada se lee la clave del servidor con
ssh-keyscan y se calcula su huella atada al secreto (SHA-256 del rótulo, el
secreto y el SHA-256 de la clave): si no es la del código, no se conecta. Esa
clave queda fijada en un known_hosts propio y la conexión de verdad usa
StrictHostKeyChecking=yes: otra clave en la segunda conexión no entra. La primera conexión queda abierta como maestra de OpenSSH
(ControlMaster) y las órdenes siguientes (ssh, scp, sftp) van por ella sin
volver a autenticarse. La clave privada solo existe en disco (0600, en una
carpeta 0700 bajo $XDG_RUNTIME_DIR o /tmp) mientras se abre la conexión. Nada
del código ni del secreto se escribe en el repositorio ni en los registros.
Sirve en cualquier Linux (y en WSL, con NAT o en espejo) con OpenSSH 8 o más:
no toca ~/.ssh ni la configuración del usuario (ssh -F /dev/null).
"""
import base64
import hashlib
import hmac
import os
import re
import shutil
import stat
import struct
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# ------------------------------------------------------------ el código (como debug_app_logica.h)

ALFABETO = "0123456789ABCDEFGHJKMNPQRSTVWXYZ"
VERSAO = 2   # v2: huella con llave (ver huella_codigo)
SEGREDO = 8
HUELLA = 8
CARGA = 1 + 4 + 2 + SEGREDO + HUELLA
BYTES = CARGA + 2
LETRAS = 40
ROTULO = b"SaguiOS DEBUG-APP v1"            # la semilla (no cambió en v2)
ROTULO_HUELLA = b"SaguiOS DEBUG-APP huella v2"


class CodigoMal(Exception):
    pass


# Marcas del formato de clave de OpenSSH (no son una clave: la clave de un solo uso se genera al vuelo).
PEM_INICIO = "-----BEGIN OPENSSH PRIVATE KEY-----"
PEM_FIN = "-----END OPENSSH PRIVATE KEY-----"

def _valor(c):
    c = c.upper()
    if c in "IL":
        return 1
    if c == "O":
        return 0
    i = ALFABETO.find(c)
    if i < 0:
        raise CodigoMal(f"«{c}» no es una letra del código")
    return i


def codificar(ip, porta, segredo, huella):
    carga = bytes([VERSAO]) + bytes(int(x) for x in ip.split(".")) + struct.pack(">H", porta) + segredo + huella
    b = carga + hashlib.sha256(carga).digest()[:2]
    n = int.from_bytes(b, "big")
    letras = "".join(ALFABETO[(n >> (5 * (LETRAS - 1 - i))) & 31] for i in range(LETRAS))
    return "-".join(letras[i:i + 5] for i in range(0, LETRAS, 5))


def decodificar(texto):
    letras = [c for c in texto if c not in "- \t\r\n"]
    if len(letras) != LETRAS:
        raise CodigoMal(f"el código tiene {len(letras)} letras y deben ser {LETRAS}")
    n = 0
    for c in letras:
        n = n << 5 | _valor(c)
    b = n.to_bytes(BYTES, "big")
    if hashlib.sha256(b[:CARGA]).digest()[:2] != b[CARGA:]:
        raise CodigoMal("la suma de control no cuadra: alguna letra está mal copiada")
    if b[0] != VERSAO:
        raise CodigoMal(f"código de otra versión de DEBUG-APP ({b[0]})")
    ip = ".".join(str(x) for x in b[1:5])
    porta = struct.unpack(">H", b[5:7])[0]
    return ip, porta, b[7:7 + SEGREDO], b[7 + SEGREDO:CARGA]


def semente(segredo):
    return hashlib.sha256(ROTULO + segredo).digest()


def huella_codigo(segredo, blob):
    """La huella del código (como dapp_huella): atada al secreto, para que quien
    quiera suplantar al servidor no sepa qué valor buscar."""
    return hashlib.sha256(ROTULO_HUELLA + segredo + hashlib.sha256(blob).digest()).digest()[:HUELLA]

# ------------------------------------------------------------ Ed25519 (RFC 8032), solo la clave pública


_P = 2 ** 255 - 19
_D = -121665 * pow(121666, _P - 2, _P) % _P
_I = pow(2, (_P - 1) // 4, _P)


def _xrecuperar(y):
    xx = (y * y - 1) * pow(_D * y * y + 1, _P - 2, _P)
    x = pow(xx, (_P + 3) // 8, _P)
    if (x * x - xx) % _P:
        x = x * _I % _P
    return _P - x if x & 1 else x


_BY = 4 * pow(5, _P - 2, _P) % _P
_B = (_xrecuperar(_BY), _BY, 1, _xrecuperar(_BY) * _BY % _P)


def _somar(a, b):
    x1, y1, z1, t1 = a
    x2, y2, z2, t2 = b
    A = (y1 - x1) * (y2 - x2) % _P
    B = (y1 + x1) * (y2 + x2) % _P
    C = 2 * t1 * t2 * _D % _P
    D = 2 * z1 * z2 % _P
    E, F, G, H = B - A, D - C, D + C, B + A
    return (E * F % _P, G * H % _P, F * G % _P, E * H % _P)


def _multiplicar(p, k):
    q = (0, 1, 1, 0)
    while k:
        if k & 1:
            q = _somar(q, p)
        p = _somar(p, p)
        k >>= 1
    return q


def ed25519_publica(sem):
    h = hashlib.sha512(sem).digest()
    a = int.from_bytes(h[:32], "little")
    a &= (1 << 254) - 8
    a |= 1 << 254
    x, y, z, _ = _multiplicar(_B, a)
    zi = pow(z, _P - 2, _P)
    x, y = x * zi % _P, y * zi % _P
    return (y | (x & 1) << 255).to_bytes(32, "little")


def _s(b):
    return struct.pack(">I", len(b)) + b


def blob_publica(pub):
    return _s(b"ssh-ed25519") + _s(pub)


def chave_openssh(sem, pub):
    """La clave privada en el formato de OpenSSH (sin cifrar)."""
    chk = os.urandom(4)
    priv = chk + chk + _s(b"ssh-ed25519") + _s(pub) + _s(sem + pub) + _s(b"debug-app")
    i = 1
    while len(priv) % 8:
        priv += bytes([i])
        i += 1
    dados = b"openssh-key-v1\0" + _s(b"none") + _s(b"none") + _s(b"") + struct.pack(">I", 1) + \
        _s(blob_publica(pub)) + _s(priv)
    b64 = base64.b64encode(dados).decode()
    return PEM_INICIO + "\n" + "\n".join(b64[i:i + 70] for i in range(0, len(b64), 70)) + "\n" + PEM_FIN + "\n"


def huella_texto(blob):
    return "SHA256:" + base64.b64encode(hashlib.sha256(blob).digest()).decode().rstrip("=")

# ------------------------------------------------------------ la conexión


def falhar(texto, codigo=1):
    print(f"depurar-remoto: {texto}", file=sys.stderr)
    sys.exit(codigo)


def ler_chave_servidor(host, porta, intentos=3):
    """El blob de la clave ed25519 del servidor (ssh-keyscan), o (None, errores)."""
    r = None
    for intento in range(intentos):   # una conexión anterior puede estar terminando (una a la vez)
        r = subprocess.run(["ssh-keyscan", "-T", "10", "-p", str(porta), "-t", "ed25519", host],
                           capture_output=True, text=True, stdin=subprocess.DEVNULL)
        for linha in r.stdout.splitlines():
            partes = linha.split()
            if len(partes) >= 3 and partes[1] == "ssh-ed25519":
                return base64.b64decode(partes[2]), ""
        time.sleep(1.5)
    return None, (r.stderr.strip()[-200:] if r else "")


def pasta_privada(caminho):
    """Crea (o acepta) una carpeta nuestra, 0700 y que no sea un enlace: en /tmp
    otro usuario podría haberla preparado antes para leer la clave."""
    try:
        os.mkdir(caminho, 0o700)
    except FileExistsError:
        pass
    st = os.lstat(caminho)
    if not stat.S_ISDIR(st.st_mode) or st.st_uid != os.getuid():
        falhar(f"{caminho} no es una carpeta de este usuario: no se usa (bórrala si es tuya)")
    if st.st_mode & 0o077:
        os.chmod(caminho, 0o700)


class Sessao:
    def __init__(self, codigo, host=None, porta=None):
        self.ip, self.porta_codigo, self.segredo, self.huella = decodificar(codigo)
        self.host = host or self.ip
        self.porta = int(porta or self.porta_codigo)
        # La carpeta privada de esta sesión: su nombre no dice nada del secreto.
        # El socket de control de OpenSSH (más un sufijo temporal de 17 letras) tiene que caber en
        # los 108 bytes de una dirección Unix: si no cabe, a /tmp. Si fallara después de entrar,
        # el código se habría gastado sin servir de nada.
        base = os.environ.get("XDG_RUNTIME_DIR")
        if not base or not os.path.isdir(base) or not os.access(base, os.W_OK) or len(base) > 60:
            base = "/tmp"
        raiz = os.path.join(base, f"depurar-remoto-{os.getuid()}")
        pasta_privada(raiz)
        ident = hashlib.sha256(f"{self.host}:{self.porta}:{self.huella.hex()}".encode()).hexdigest()[:12]
        self.dir = os.path.join(raiz, ident)
        pasta_privada(self.dir)
        self.alias = "debug-app-" + ident
        self.known = os.path.join(self.dir, "known_hosts")
        self.ctl = os.path.join(self.dir, "c")

    def opcoes(self):
        return ["-F", "/dev/null", "-p", str(self.porta),
                "-o", "UserKnownHostsFile=" + self.known, "-o", "GlobalKnownHostsFile=/dev/null",
                "-o", "StrictHostKeyChecking=yes", "-o", "HostKeyAlias=" + self.alias,
                "-o", "HostKeyAlgorithms=ssh-ed25519", "-o", "IdentitiesOnly=yes", "-o", "IdentityAgent=none",
                "-o", "PasswordAuthentication=no", "-o", "KbdInteractiveAuthentication=no",
                "-o", "BatchMode=yes", "-o", "ConnectTimeout=15", "-o", "ServerAliveInterval=30",
                "-o", "LogLevel=ERROR", "-o", "ControlPath=" + self.ctl]

    def destino(self):
        return "root@" + self.host

    def viva(self):
        r = subprocess.run(["ssh"] + self.opcoes() + ["-O", "check", self.destino()],
                           capture_output=True, stdin=subprocess.DEVNULL)
        return r.returncode == 0

    def _comprobar_huella(self):
        """Lee la clave del servidor, comprueba que es la del código y la fija en
        nuestro known_hosts: la conexión de verdad (StrictHostKeyChecking=yes)
        solo acepta esa clave entera, sin hueco entre las dos conexiones."""
        blob, err = ler_chave_servidor(self.host, self.porta)
        if blob is None:
            falhar(f"no contesta un servidor SSH en {self.host}:{self.porta} "
                   f"(¿sesión cortada o caducada, otra red, Wi-Fi con aislamiento de clientes?) {err}")
        if not hmac.compare_digest(huella_codigo(self.segredo, blob), self.huella):
            falhar(f"la clave del servidor en {self.host}:{self.porta} ({huella_texto(blob)}) NO es la del código: "
                   "no se conecta (¿otro equipo en esa dirección, alguien en medio, o una letra del secreto mal copiada?)")
        with open(self.known, "w") as f:
            f.write(f"{self.alias} ssh-ed25519 {base64.b64encode(blob).decode()}\n")
        return huella_texto(blob)

    def conectar(self, silencio=False):
        if os.path.exists(self.ctl) and self.viva():
            return
        if os.path.exists(self.ctl):
            os.remove(self.ctl)
        h = self._comprobar_huella()
        sem = semente(self.segredo)
        arq = os.path.join(self.dir, "k")
        try:
            if os.path.lexists(arq):
                os.remove(arq)   # de una ejecución que se cortó a medias
            fd = os.open(arq, os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW, 0o600)
            with os.fdopen(fd, "w") as f:
                f.write(chave_openssh(sem, ed25519_publica(sem)))
            erro, erros = "", []
            for intento in range(4):
                # -f: queda en segundo plano como maestra; las órdenes siguientes van por ella
                # los errores a un fichero y no a una tubería: con -f, el ssh que queda en
                # segundo plano la mantendría abierta y run() no volvería nunca
                arq_err = os.path.join(self.dir, "err")
                with open(arq_err, "w") as fe:
                    r = subprocess.run(["ssh"] + self.opcoes() + ["-i", arq, "-o", "ControlMaster=yes",
                                       "-o", "ControlPersist=yes", "-f", "-N", self.destino()],
                                       stdout=subprocess.DEVNULL, stderr=fe, stdin=subprocess.DEVNULL, timeout=60)
                with open(arq_err) as fe:
                    erro = fe.read().strip()
                os.remove(arq_err)
                erros.append(f"[{intento + 1}] {erro or r.returncode}")
                if r.returncode == 0 and self.viva():
                    break
                if "Permission denied" in erro:
                    break
                time.sleep(1.5)   # el hueco puede seguir ocupado un momento por ssh-keyscan
            else:
                falhar("no se pudo conectar: " + " ".join(erros)[-600:])
            if "Permission denied" in erro:
                falhar("SaguiOS no acepta la clave: el código ya se usó, se anuló o es de otra sesión. "
                       "Genera otro en DEBUG-APP.")
        finally:
            if os.path.exists(arq):
                os.remove(arq)   # ya no hace falta: el resto va por la conexión maestra
        if not silencio:
            print(f"depurar-remoto: conectado a {self.host}:{self.porta} (huella {h})", file=sys.stderr)

    def ssh(self, orden, tty=False, **kw):
        cmd = ["ssh"] + self.opcoes() + ["-o", "ControlMaster=no"] + (["-tt"] if tty else []) + [self.destino()]
        if orden is not None:
            cmd.append(orden)
        return subprocess.run(cmd, **kw)

    def scp(self, origem, destino):
        cmd = ["scp", "-q"] + [o if o != "-p" else "-P" for o in self.opcoes()] + ["-o", "ControlMaster=no", "--", origem, destino]
        return subprocess.run(cmd, stdin=subprocess.DEVNULL)

    def cerrar(self):
        if os.path.exists(self.ctl):
            subprocess.run(["ssh"] + self.opcoes() + ["-O", "exit", self.destino()], capture_output=True,
                           stdin=subprocess.DEVNULL)
        shutil.rmtree(self.dir, ignore_errors=True)

# ------------------------------------------------------------ recoger


RECOGER = [
    ("version.txt", "uname -a"),
    ("dmesg.txt", "dmesg"),
    ("infopc.txt", "infopc -t"),
    ("acel2d.txt", "acel2d probar --detalle"),
    ("gpu3d.txt", "gpu3d probar --detalle"),
    ("lspci.txt", "lspci"),
    ("debug-app.log", "cat /home/.config/debug-app/debug-app.log"),
]


def recoger(s, pasta_base, maquina):
    if not maquina:
        r = s.ssh("hostname", capture_output=True, text=True, stdin=subprocess.DEVNULL, timeout=60)
        maquina = (r.stdout.strip() or "saguios") + "-" + s.ip
    maquina = re.sub(r"[^A-Za-z0-9_.-]+", "_", maquina)
    pasta = os.path.join(pasta_base, f"{maquina}-{time.strftime('%Y%m%d-%H%M%S')}")
    os.makedirs(pasta, exist_ok=True)
    resumen = []
    for arq, orden in RECOGER:
        try:
            r = s.ssh(orden, capture_output=True, stdin=subprocess.DEVNULL, timeout=300)
            datos, cod = r.stdout + (b"\n--- errores ---\n" + r.stderr if r.stderr else b""), r.returncode
        except subprocess.TimeoutExpired:
            datos, cod = b"(no termino en 300 s)\n", "tiempo"
        with open(os.path.join(pasta, arq), "wb") as f:
            f.write(datos)
        resumen.append(f"{arq}: «{orden}» -> {cod}")
        print(f"  {arq}: {cod}", file=sys.stderr)
    with open(os.path.join(pasta, "LEEME.txt"), "w") as f:
        f.write(f"Recogido con tools/depurar-remoto.py de {s.host}:{s.porta} el {time.strftime('%Y-%m-%d %H:%M:%S')}\n")
        f.write("\n".join(resumen) + "\n")
    print(pasta)
    return 0

# ------------------------------------------------------------ principal


def main(argv):
    host = porta = maquina = None
    resultados = os.path.join(RAIZ, "pruebas-remotas")
    args = []
    i = 0
    while i < len(argv):
        a = argv[i]
        if a in ("--host", "--puerto", "--resultados", "--maquina") and i + 1 < len(argv):
            v = argv[i + 1]
            if a == "--host":
                host = v
            elif a == "--puerto":
                porta = int(v)
            elif a == "--resultados":
                resultados = v
            else:
                maquina = v
            i += 2
            continue
        if a in ("-h", "--help", "--ayuda"):
            print(__doc__)
            return 0
        args.append(a)
        i += 1
    if len(args) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    codigo, orden, resto = args[0], args[1], args[2:]
    if codigo == "-":
        if sys.stdin.isatty():
            import getpass
            codigo = getpass.getpass("Código de DEBUG-APP: ")
        else:
            codigo = sys.stdin.readline()
    elif codigo == "env":
        codigo = os.environ.get("DEBUG_APP_CODIGO", "")
    try:
        s = Sessao(codigo, host, porta)
    except CodigoMal as e:
        falhar(f"código no válido: {e}", 2)
    if orden == "info":
        print(f"dirección {s.ip}, puerto {s.porta_codigo}, huella (principio) {s.huella.hex()}")
        if host or porta:
            print(f"se conectará a {s.host}:{s.porta}")
        return 0
    if orden == "cerrar":
        s.cerrar()
        return 0
    s.conectar(silencio=orden != "conectar")
    if orden == "conectar":
        return 0
    if orden == "ejecutar" and resto:
        return s.ssh(" ".join(resto)).returncode
    if orden == "shell":
        return s.ssh(None, tty=True).returncode
    if orden == "recoger":
        return recoger(s, resultados, maquina)
    if orden == "subir" and resto:
        dest = resto[1] if len(resto) > 1 else "/tmp/"
        return s.scp(resto[0], f"{s.destino()}:{dest}").returncode
    if orden == "bajar" and resto:
        local = resto[1] if len(resto) > 1 else "."
        return s.scp(f"{s.destino()}:{resto[0]}", local).returncode
    falhar(f"orden desconocida o sin argumentos: {orden}", 2)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
