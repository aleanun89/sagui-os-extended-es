#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H248 (D250, mejora 052) - SSH de administración: el servidor sshd.

En el anfitrión:
  1. tests/ssh_unidad.c: Ed25519 con los vectores de la RFC 8032, el cifrado
     chacha20-poly1305@openssh.com y la lógica del servidor (listas de
     algoritmos, authorized_keys, red local, teclas y rutas);
  2. si está la biblioteca «cryptography» de Python, las mismas funciones
     comparadas con ella en datos al azar (firmas, claves públicas, paquetes).

En QEMU (red de usuario con hostfwd al puerto 22), con el ssh, scp, sftp y
ssh-keygen de OpenSSH del anfitrión y una clave ed25519 creada para la prueba,
metida en el /etc/ssh/authorized_keys de la imagen:
  - con el servicio desactivado (de serie) no hay nadie en el puerto;
  - servicio activar/iniciar sshd; la huella de `sshd --huella` es la que ve
    el cliente;
  - ssh ... 'echo hola; uname' devuelve la salida y el código 0; una orden que
    falla da su código; salida y errores van separados; la entrada llega;
    100 KB de ida y vuelta por `tee` salen iguales (también renovando las
    claves cada 64 KB);
  - scp en los dos sentidos y sftp (listar, mkdir, put, get, rename, rm) con
    las sumas iguales;
  - una sesión interactiva con terminal (ssh -tt);
  - una clave no autorizada y la contraseña se rechazan; una clave añadida en
    /home/.ssh/authorized_keys (subida por scp) entra;
  - un cliente que solo ofrece algoritmos no admitidos recibe un rechazo
    limpio; lo que no es SSH se corta;
  - con un cliente SSH mínimo escrito aquí (si está «cryptography»): una
    contraseña y una firma falsa se rechazan, un paquete cifrado con un bit
    cambiado, repetido o de tamaño absurdo corta la conexión, al sexto intento
    se desconecta, y renovar claves sin el modo estricto funciona;
  - una dirección que falla muchas veces seguidas queda bloqueada un tiempo;
  - todo queda en el registro (conexión, clave, orden), sin claves privadas;
  - al parar el servicio, el puerto se cierra.
"""
import hashlib
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, FalhaTeste  # noqa: E402

DADOS_MB = 16
TRAB = os.path.join(SAIDA, "h248")


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


# ------------------------------------------------------------ anfitrión: criptografía y lógica

def unidad():
    exe = os.path.join(TRAB, "ssh_unidad")
    fontes = [os.path.join(RAIZ, "tests", "ssh_unidad.c"), os.path.join(RAIZ, "kernel", "lib", "sha256.c")]
    fontes += [os.path.join(RAIZ, "userland", "libtls", f + ".c") for f in ("ed25519", "hash", "chacha", "sshcifra", "x25519")]
    base = ["gcc", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "userland", "inc"),
            "-I", os.path.join(RAIZ, "kernel", "inc"), "-I", os.path.join(RAIZ, "userland", "programas")]
    r = subprocess.run(base + ["-O2", "-o", exe] + fontes, capture_output=True, text=True)
    verificar(r.returncode == 0 and "warning" not in r.stderr, "ssh_unidad.c compila sin avisos" + r.stderr[:2000])
    r = subprocess.run([exe], capture_output=True, text=True, timeout=300)
    n = re.search(r"ssh_unidad: (\d+)/(\d+) comprobaciones correctas", r.stdout)
    verificar(r.returncode == 0 and n and n.group(1) == n.group(2),
              f"Ed25519 (RFC 8032), chacha20-poly1305@openssh.com y lógica del servidor: {r.stdout.strip()[-300:]}")
    exe2 = exe + "-asan"
    r = subprocess.run(base + ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-o", exe2] + fontes,
                       capture_output=True, text=True)
    if r.returncode == 0:
        r = subprocess.run([exe2], capture_output=True, text=True, timeout=600)
        verificar(r.returncode == 0, "lo mismo con -fsanitize=address,undefined" + r.stderr[-1500:])
    else:
        print("  AVISO: sin los sanitizadores de gcc en este anfitrión; se omite")

    try:
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        from cryptography.hazmat.primitives import serialization as ser, poly1305
        from cryptography.hazmat.primitives.ciphers import Cipher, algorithms
    except ImportError:
        print("  AVISO: sin la biblioteca cryptography de Python; se omite la comparación cruzada")
        return
    p = subprocess.Popen([exe, "ordenes"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    def o(linea):
        p.stdin.write(linea + "\n")
        p.stdin.flush()
        return p.stdout.readline().strip()

    for i in range(24):
        sem, msg = os.urandom(32), os.urandom(i * 17)
        k = Ed25519PrivateKey.from_private_bytes(sem)
        pub = k.public_key().public_bytes(ser.Encoding.Raw, ser.PublicFormat.Raw)
        firma = k.sign(msg)
        mh = msg.hex() or "-"
        if o("publica " + sem.hex()) != pub.hex() or o(f"firmar {sem.hex()} {mh}") != firma.hex():
            raise FalhaTeste(f"Ed25519 distinto de cryptography (caso {i})")
        mala = bytearray(firma)
        mala[i % 64] ^= 1 << (i % 8)
        if o(f"verificar {pub.hex()} {firma.hex()} {mh}") != "1" or o(f"verificar {pub.hex()} {mala.hex()} {mh}") != "0":
            raise FalhaTeste(f"verificación Ed25519 mal (caso {i})")
        clave, seq, cuerpo = os.urandom(64), int.from_bytes(os.urandom(4), "big"), os.urandom(8 * (i + 1))
        nonce = seq.to_bytes(8, "big")

        def cc(kk, ctr, d):
            return Cipher(algorithms.ChaCha20(kk, ctr.to_bytes(8, "little") + nonce), None).encryptor().update(d)
        el, ec = cc(clave[32:], 0, len(cuerpo).to_bytes(4, "big")), cc(clave[:32], 1, cuerpo)
        mac = poly1305.Poly1305(cc(clave[:32], 0, bytes(32)))
        mac.update(el + ec)
        pkt = el + ec + mac.finalize()
        if o(f"selar {clave.hex()} {seq} {cuerpo.hex()}") != pkt.hex() or \
           o(f"abrir {clave.hex()} {seq} {pkt.hex()}") != f"{len(cuerpo)} {cuerpo.hex()}":
            raise FalhaTeste(f"chacha20-poly1305@openssh.com distinto de cryptography (caso {i})")
    p.stdin.close()
    p.wait(timeout=10)
    verificar(True, "Ed25519 y chacha20-poly1305@openssh.com iguales que la biblioteca cryptography (24 casos al azar)")


# ------------------------------------------------------------ imagen y claves

def gerar_chave(nome):
    arq = os.path.join(TRAB, nome)
    for f in (arq, arq + ".pub"):
        if os.path.exists(f):
            os.remove(f)
    subprocess.run(["ssh-keygen", "-q", "-t", "ed25519", "-N", "", "-C", nome, "-f", arq], check=True)
    return arq


def construir_imagen(pub):
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(TRAB, "rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    with open(os.path.join(raiz, "etc", "ssh", "authorized_keys"), "w") as f:
        f.write("# clave de la prueba h248\n" + open(pub).read())
        f.write("ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAAAgQC7 otra@pc\n")   # de un tipo no admitido: se ignora
    # el bloqueo por fallos, más corto que de serie para poder probarlo (y que no estorbe antes)
    with open(os.path.join(raiz, "etc", "ssh", "sshd.conf"), "a") as f:
        f.write("\n# prueba h248\nbloqueo_fallos=10\nbloqueo_segundos=8\n")
    sgfs = os.path.join(TRAB, "h248.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(TRAB, "h248.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--console", "--dados-mb", str(DADOS_MB)],
                   check=True, stdout=subprocess.DEVNULL)
    return img


class Cliente:
    """El OpenSSH del anfitrión contra el puerto reenviado."""

    def __init__(self, porta, chave):
        self.porta = porta
        self.chave = chave

    def opcoes(self, chave=None, extra=()):
        o = ["-F", "/dev/null"]
        for e in extra:          # delante: en ssh gana la primera vez que aparece una opción
            o += ["-o", e]
        o += ["-o", "StrictHostKeyChecking=no", "-o", "UserKnownHostsFile=/dev/null",
              "-o", "IdentitiesOnly=yes", "-o", "IdentityAgent=none", "-o", "BatchMode=yes", "-o", "LogLevel=ERROR",
              "-o", "ConnectTimeout=20", "-i", chave or self.chave]
        return o

    def ssh(self, orden, entrada=None, chave=None, extra=(), tempo=90, args=()):
        cmd = ["ssh", "-p", str(self.porta)] + self.opcoes(chave, extra) + list(args) + ["root@127.0.0.1"]
        if orden is not None:
            cmd.append(orden)
        return subprocess.run(cmd, input=entrada, capture_output=True, timeout=tempo,
                              stdin=subprocess.DEVNULL if entrada is None else None)

    def scp(self, origem, destino, tempo=180):
        cmd = ["scp", "-P", str(self.porta)] + self.opcoes() + [origem, destino]
        return subprocess.run(cmd, capture_output=True, timeout=tempo, stdin=subprocess.DEVNULL)

    def sftp(self, lote, tempo=180):
        cmd = ["sftp", "-P", str(self.porta)] + self.opcoes() + ["-b", "-", "root@127.0.0.1"]
        return subprocess.run(cmd, input=lote.encode(), capture_output=True, timeout=tempo)


def banner(porta, tempo=4):
    """Lo primero que manda quien escucha en el puerto (b'' = nadie)."""
    try:
        c = socket.create_connection(("127.0.0.1", porta), timeout=tempo)
    except OSError:
        return b""
    c.settimeout(tempo)
    try:
        return c.recv(200)
    except OSError:
        return b""
    finally:
        c.close()


def suma(dados):
    return hashlib.sha256(dados).hexdigest()


def esperar_log(m, texto, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial()[desde:]:
            return
        time.sleep(0.2)
    raise FalhaTeste(f"no apareció en el registro: {texto!r}")


# ------------------------------------------------------------ un cliente SSH mínimo, para hacer trampas

def _u32(n):
    return struct.pack(">I", n)


def _str(b):
    return _u32(len(b)) + b


def _mpint(k):
    k = k.lstrip(b"\0")
    if k and k[0] & 0x80:
        k = b"\0" + k
    return _str(k)


class Leitor:
    def __init__(self, d, pos=0):
        self.d, self.pos = d, pos

    def u32(self):
        v = struct.unpack(">I", self.d[self.pos:self.pos + 4])[0]
        self.pos += 4
        return v

    def str(self):
        n = self.u32()
        v = self.d[self.pos:self.pos + n]
        self.pos += n
        return v


class Mini:
    """Lo justo de SSH-2 (curve25519-sha256, ssh-ed25519, chacha20-poly1305@openssh.com) con la
    biblioteca cryptography, sin el modo estricto, para mandar al servidor lo que OpenSSH no manda:
    contraseñas, firmas falsas, paquetes manipulados o repetidos."""

    CIFRA = b"chacha20-poly1305@openssh.com"

    def __init__(self, porta):
        self.s = socket.create_connection(("127.0.0.1", porta), timeout=20)
        self.s.settimeout(20)
        self.vc = b"SSH-2.0-mini_h248"
        self.s.sendall(self.vc + b"\r\n")
        self.buf = b""
        while b"\n" not in self.buf:
            self.buf += self.s.recv(4096)
        self.vs, self.buf = self.buf.split(b"\n", 1)
        self.vs = self.vs.rstrip(b"\r")
        self.seq_tx = self.seq_rx = 0
        self.ktx = self.krx = self.sid = None
        self.pub_servidor = None

    def fechar(self):
        self.s.close()

    def _ler(self, n):
        while len(self.buf) < n:
            x = self.s.recv(65536)
            if not x:
                raise EOFError
            self.buf += x
        r, self.buf = self.buf[:n], self.buf[n:]
        return r

    @staticmethod
    def _cc(chave, contador, seq, dados):
        from cryptography.hazmat.primitives.ciphers import Cipher, algorithms
        return Cipher(algorithms.ChaCha20(chave, contador.to_bytes(8, "little") + seq.to_bytes(8, "big")), None) \
            .encryptor().update(dados)

    def _etiqueta(self, chave, seq, dados):
        from cryptography.hazmat.primitives import poly1305
        mac = poly1305.Poly1305(self._cc(chave[:32], 0, seq, bytes(32)))
        mac.update(dados)
        return mac.finalize()

    def pacote(self, conteudo, tam_falso=None):
        base = (1 if self.ktx else 5) + len(conteudo)
        pad = 8 - base % 8
        if pad < 4:
            pad += 8
        corpo = bytes([pad]) + conteudo + os.urandom(pad)
        tam = _u32(len(corpo) if tam_falso is None else tam_falso)
        if not self.ktx:
            return tam + corpo
        el = self._cc(self.ktx[32:], 0, self.seq_tx, tam)
        ec = self._cc(self.ktx[:32], 1, self.seq_tx, corpo)
        return el + ec + self._etiqueta(self.ktx, self.seq_tx, el + ec)

    def enviar(self, conteudo=None, cru=None):
        d = cru if cru is not None else self.pacote(conteudo)
        self.seq_tx = (self.seq_tx + 1) & 0xFFFFFFFF
        self.s.sendall(d)
        return d

    def receber(self):
        if not self.krx:
            corpo = self._ler(struct.unpack(">I", self._ler(4))[0])
        else:
            el = self._ler(4)
            tam = struct.unpack(">I", self._cc(self.krx[32:], 0, self.seq_rx, el))[0]
            ec = self._ler(tam)
            if self._ler(16) != self._etiqueta(self.krx, self.seq_rx, el + ec):
                raise FalhaTeste("la etiqueta de un paquete del servidor no coincide")
            corpo = self._cc(self.krx[:32], 1, self.seq_rx, ec)
        self.seq_rx = (self.seq_rx + 1) & 0xFFFFFFFF
        return corpo[1:len(corpo) - corpo[0]]

    def ate_fechar(self):
        """Los paquetes que lleguen hasta que el servidor cierre."""
        v = []
        try:
            while True:
                v.append(self.receber())
        except (EOFError, OSError, struct.error):
            pass
        return v

    def kex(self):
        from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey, X25519PublicKey
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
        from cryptography.hazmat.primitives import serialization as ser
        ic = bytes([20]) + os.urandom(16) + _str(b"curve25519-sha256") + _str(b"ssh-ed25519") + _str(self.CIFRA) * 2 + \
            _str(b"hmac-sha2-256") * 2 + _str(b"none") * 2 + _str(b"") * 2 + b"\0" + _u32(0)
        self.enviar(ic)
        is_ = self.receber()
        while is_[0] != 20:          # datos de canal que venían de camino (al renovar claves)
            is_ = self.receber()
        priv = X25519PrivateKey.generate()
        qc = priv.public_key().public_bytes(ser.Encoding.Raw, ser.PublicFormat.Raw)
        self.enviar(bytes([30]) + _str(qc))
        r = self.receber()
        if r[0] != 31:
            raise FalhaTeste(f"se esperaba la respuesta del intercambio y llegó el mensaje {r[0]}")
        l = Leitor(r, 1)
        ks, qs, firma = l.str(), l.str(), l.str()
        k = priv.exchange(X25519PublicKey.from_public_bytes(qs))
        h = hashlib.sha256(_str(self.vc) + _str(self.vs) + _str(ic) + _str(is_) + _str(ks) + _str(qc) + _str(qs) +
                           _mpint(k)).digest()
        lk, lf = Leitor(ks), Leitor(firma)
        if lk.str() != b"ssh-ed25519" or lf.str() != b"ssh-ed25519":
            raise FalhaTeste("la clave o la firma del servidor no son ssh-ed25519")
        self.pub_servidor = lk.str()
        Ed25519PublicKey.from_public_bytes(self.pub_servidor).verify(lf.str(), h)   # lanza si la firma no vale
        if self.sid is None:
            self.sid = h

        def derivar(letra):
            k1 = hashlib.sha256(_mpint(k) + h + letra + self.sid).digest()
            return k1 + hashlib.sha256(_mpint(k) + h + k1).digest()
        self.enviar(bytes([21]))
        if self.receber()[0] != 21:
            raise FalhaTeste("se esperaba NEWKEYS")
        self.ktx, self.krx = derivar(b"C"), derivar(b"D")

    def servico(self):
        self.enviar(bytes([5]) + _str(b"ssh-userauth"))
        return self.receber()[0] == 6

    def senha(self, texto):
        self.enviar(bytes([50]) + _str(b"root") + _str(b"ssh-connection") + _str(b"password") + b"\0" + _str(texto))
        return self.receber()

    def chave(self, arq, estragar=False):
        from cryptography.hazmat.primitives import serialization as ser
        priv = ser.load_ssh_private_key(open(arq, "rb").read(), None)
        pub = priv.public_key().public_bytes(ser.Encoding.Raw, ser.PublicFormat.Raw)
        blob = _str(b"ssh-ed25519") + _str(pub)
        pedido = bytes([50]) + _str(b"root") + _str(b"ssh-connection") + _str(b"publickey") + b"\1" + \
            _str(b"ssh-ed25519") + _str(blob)
        firma = bytearray(priv.sign(_str(self.sid) + pedido))
        if estragar:
            firma[40] ^= 0x20
        self.enviar(pedido + _str(_str(b"ssh-ed25519") + _str(bytes(firma))))
        return self.receber()

    def ordem(self, texto, canal=7):
        """Abre un canal, ejecuta la orden y devuelve (salida, código)."""
        self.enviar(bytes([90]) + _str(b"session") + _u32(canal) + _u32(1 << 20) + _u32(32768))
        r = self.receber()
        if r[0] != 91:
            raise FalhaTeste(f"el canal no se abrió (mensaje {r[0]})")
        l = Leitor(r, 1)
        l.u32()
        meu = l.u32()
        self.enviar(bytes([98]) + _u32(meu) + _str(b"exec") + b"\1" + _str(texto))
        saida, codigo = b"", None
        while True:
            r = self.receber()
            l = Leitor(r, 1)
            if r[0] == 94:
                l.u32()
                saida += l.str()
            elif r[0] == 98:
                l.u32()
                if l.str() == b"exit-status":
                    codigo = Leitor(r, len(r) - 4).u32()
            elif r[0] == 97:
                self.enviar(bytes([97]) + _u32(meu))
                return saida, codigo
            elif r[0] == 100:
                raise FalhaTeste("el servidor rechazó la orden")


def trampas(m, porta, chave):
    """Lo que un cliente honrado no hace."""
    try:
        import cryptography  # noqa: F401
    except ImportError:
        print("  AVISO: sin la biblioteca cryptography de Python; se omiten las pruebas con el cliente mínimo")
        return
    marca = len(m.serial())

    # contraseña: rechazada, y nunca al registro; firma falsa; firma buena; orden; renovar claves; repetir un paquete
    c = Mini(porta)
    c.kex()
    verificar(True, "cliente mínimo (sin modo estricto): intercambio de claves y firma del servidor verificada aparte")
    verificar(c.servico(), "servicio ssh-userauth aceptado")
    r = c.senha(b"secreto-h248-no-al-registro")
    verificar(r[0] == 51 and Leitor(r, 1).str() == b"publickey", "una contraseña se rechaza: solo queda publickey")
    esperar_log(m, "intento con contrase", 20, marca)
    r = c.chave(chave, estragar=True)
    verificar(r[0] == 51, "una firma manipulada con una clave autorizada se rechaza")
    esperar_log(m, "firma NO v", 20, marca)
    r = c.chave(chave)
    verificar(r[0] == 52, "la firma buena entra")
    saida, codigo = c.ordem(b"echo mini; exit 5")
    verificar(saida == b"mini\n" and codigo == 5, "orden por el cliente mínimo: salida y código 5")
    c.kex()
    saida, codigo = c.ordem(b"echo otra-vez")
    verificar(saida == b"otra-vez\n" and codigo == 0, "renovar claves sin el modo estricto (los números de secuencia siguen) y otra orden")
    cru = c.pacote(bytes([2]) + _str(b"relleno"))
    c.enviar(cru=cru)
    c.enviar(cru=cru)        # el mismo paquete otra vez: su número de secuencia ya no es ese
    resto = c.ate_fechar()
    c.fechar()
    verificar(not resto, "un paquete repetido (replay) corta la conexión sin respuesta")
    # con otro número de secuencia, lo primero que sale mal es el tamaño descifrado (o la etiqueta)
    esperar_log(m, "conexi", 20, marca)
    fim = time.time() + 20
    while time.time() < fim and not re.search(r"terminada: (tama.o de paquete no v|la etiqueta \(MAC\))", m.serial()[marca:]):
        time.sleep(0.2)
    verificar(re.search(r"terminada: (tama.o de paquete no v|la etiqueta \(MAC\))", m.serial()[marca:]),
              "el servidor anota por qué cortó (tamaño o etiqueta no válidos)")

    # un bit cambiado en un paquete cifrado
    marca = len(m.serial())
    c = Mini(porta)
    c.kex()
    cru = bytearray(c.pacote(bytes([5]) + _str(b"ssh-userauth")))
    cru[9] ^= 0x04
    c.enviar(cru=bytes(cru))
    resto = c.ate_fechar()
    c.fechar()
    verificar(not resto, "un bit cambiado en un paquete cifrado: conexión cortada antes de procesarlo")
    esperar_log(m, "la etiqueta (MAC) de un paquete no coincide", 20, marca)

    # tamaño absurdo en un paquete cifrado
    marca = len(m.serial())
    c = Mini(porta)
    c.kex()
    c.enviar(cru=c.pacote(bytes([2]) + bytes(40), tam_falso=0x00FFFFF0))
    resto = c.ate_fechar()
    c.fechar()
    verificar(not resto, "tamaño de paquete absurdo (16 MB): conexión cortada sin esperar esos datos")
    esperar_log(m, "de paquete no v", 20, marca)

    # demasiados intentos
    marca = len(m.serial())
    c = Mini(porta)
    c.kex()
    c.servico()
    v = []
    try:
        for i in range(10):
            v.append(c.senha(b"intento%d" % i)[0])
    except (EOFError, OSError, struct.error):
        pass
    c.fechar()
    verificar(v.count(51) == 5 and v[-1] == 1, f"al sexto intento fallido el servidor desconecta ({v})")
    esperar_log(m, "demasiados intentos", 20, marca)

    # más datos de los que caben en un paquete de canal, y un canal que no existe
    c = Mini(porta)
    c.kex()
    c.servico()
    c.chave(chave)
    c.enviar(bytes([94]) + _u32(3) + _str(b"x"))
    r = c.ate_fechar()
    c.fechar()
    verificar(r and r[-1][0] == 1, "datos para un canal que no existe: desconexión")
    log = m.serial()[marca:]
    verificar("secreto-h248" not in m.serial() and "intento3" not in log, "las contraseñas probadas no aparecen en el registro")


# ------------------------------------------------------------ QEMU

def maquina():
    chave = gerar_chave("clave_h248")
    outra = gerar_chave("clave_no_autorizada")
    segunda = gerar_chave("clave_segunda")
    img = construir_imagen(chave + ".pub")
    porta = puerto_libre()
    cl = Cliente(porta, chave)
    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{porta}-:22"]
    with Maquina("h248", "ide", imagem=img, snapshot=False, extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        esperar_log(m, "10.0.2.15 (DHCP)", 30)

        # --- desactivado de serie
        out = m.comando("servicio listar")
        verificar(re.search(r"^sshd\s+parado", out, re.M), "servicio listar: sshd está parado de serie")
        verificar("sshd: escuchando" not in m.serial(), "sshd no arranca solo")
        verificar(banner(porta) == b"", "con el servicio desactivado no hay nadie en el puerto 22")
        r = cl.ssh("echo hola", tempo=40)
        verificar(r.returncode == 255, "ssh con el servicio desactivado: no conecta")

        # --- activar e iniciar
        out = m.comando("servicio activar sshd", 30)
        out += m.comando("servicio iniciar sshd", 30)
        esperar_log(m, "sshd: escuchando en el puerto 22 (solo red local)", 40)
        verificar("1 clave(s) autorizada(s)" in m.serial(), "servicio activar + iniciar sshd: escucha, con 1 clave autorizada")
        verificar("authorized_keys no valen" in m.serial(), "avisa de la línea ssh-rsa que no vale")
        b = banner(porta)
        verificar(b.startswith(b"SSH-2.0-SaguiOS_sshd"), f"el servidor se identifica: {b[:40]!r}")

        out = m.comando("sshd --huella")
        h = re.search(r"(SHA256:[A-Za-z0-9+/]{43}) \(ED25519\)", out)
        verificar(h, "sshd --huella muestra la huella de la clave del servidor")
        verificar("archivo: /home/.ssh/ssh_host_ed25519_key" in out and "aviso" not in out,
                  "la clave del servidor está en /home/.ssh (persistente: sin aviso)")
        r = subprocess.run(["ssh-keyscan", "-T", "20", "-p", str(porta), "-t", "ed25519", "127.0.0.1"],
                           capture_output=True, text=True, timeout=60)
        conhecidos = os.path.join(TRAB, "known_hosts")
        with open(conhecidos, "w") as f:
            f.write(r.stdout)
        r2 = subprocess.run(["ssh-keygen", "-lf", conhecidos], capture_output=True, text=True)
        verificar(h.group(1) in r2.stdout, f"la huella que ve el cliente (ssh-keyscan) es la de sshd --huella ({h.group(1)})")

        # --- exec
        r = cl.ssh("echo hola; uname")
        verificar(r.returncode == 0 and r.stdout.startswith(b"hola\n") and b"Sagui" in r.stdout,
                  f"ssh 'echo hola; uname': salida y código 0 ({r.stdout!r} {r.stderr[-200:]!r})")
        # con el known_hosts de ssh-keyscan y comprobación estricta: la firma del servidor vale
        cmd = ["ssh", "-p", str(porta), "-F", "/dev/null", "-o", "StrictHostKeyChecking=yes",
               "-o", f"UserKnownHostsFile={conhecidos}", "-o", "IdentitiesOnly=yes", "-o", "IdentityAgent=none",
               "-o", "BatchMode=yes", "-i", chave, "-v", "root@127.0.0.1", "echo estricto"]
        r = subprocess.run(cmd, capture_output=True, timeout=60, stdin=subprocess.DEVNULL)
        dep = r.stderr.decode("latin-1")
        verificar(r.returncode == 0 and r.stdout == b"estricto\n", "con StrictHostKeyChecking=yes: la clave del servidor se verifica")
        verificar("kex: algorithm: curve25519-sha256" in dep and "kex: host key algorithm: ssh-ed25519" in dep and
                  "cipher: chacha20-poly1305@openssh.com" in dep,
                  "negociado: curve25519-sha256, ssh-ed25519, chacha20-poly1305@openssh.com")
        verificar("modo estricto" in m.serial(), "intercambio en modo estricto (kex-strict de OpenSSH 9.6, contra Terrapin)")
        r = cl.ssh("exit 7")
        verificar(r.returncode == 7, "una orden que falla devuelve su código (exit 7)")
        r = cl.ssh("cat /no/existe/nada")
        verificar(r.returncode not in (0, 255) and r.stdout == b"" and b"/no/existe/nada" in r.stderr,
                  f"cat de algo que no existe: código {r.returncode} y el error por stderr")
        r = cl.ssh("echo por-salida; echo por-errores >&2")
        verificar(r.stdout == b"por-salida\n" and r.stderr == b"por-errores\n", "la salida y los errores llegan separados")
        r = cl.ssh("cat > /home/entrada.txt; wc -c /home/entrada.txt", entrada=b"datos por la entrada\n")
        verificar(r.returncode == 0 and b"21" in r.stdout, "la entrada del cliente llega a la orden")
        r = cl.ssh("dmesg | head -5")
        verificar(r.returncode == 0 and len(r.stdout.splitlines()) == 5, "ssh 'dmesg | head -5' (tuberías dentro de la orden)")
        datos = os.urandom(100 * 1024)
        t0 = time.time()
        # (tee y no cat: el cat de SaguiOS convierte UTF-8 a Latin-1, D027)
        r = cl.ssh("tee /home/ida.bin", entrada=datos, tempo=240)
        verificar(r.returncode == 0 and r.stdout == datos,
                  f"100 KB binarios de ida y vuelta por `tee` salen iguales ({time.time() - t0:.1f} s)")
        r = cl.ssh("sha256sum /home/ida.bin")
        verificar(suma(datos).encode() in r.stdout, "y lo que guardó tee en el SaguiOS tiene la misma suma")
        r = cl.ssh("tee /home/ida.bin", entrada=datos, extra=["RekeyLimit=64K"], tempo=240)
        verificar(r.returncode == 0 and r.stdout == datos, "lo mismo renovando las claves cada 64 KB (re-kex)")
        verificar("claves renovadas" in m.serial(), "el servidor anotó la renovación de claves")

        # un proceso que queda en segundo plano no retiene la sesión
        t0 = time.time()
        r = cl.ssh("sleep 8 & echo ya")
        verificar(r.returncode == 0 and r.stdout == b"ya\n" and time.time() - t0 < 6,
                  f"'sleep 8 & echo ya' vuelve enseguida ({time.time() - t0:.1f} s)")
        # el cliente se va a medias: la orden y lo que lanzó se terminan
        p = subprocess.Popen(["ssh", "-p", str(porta)] + cl.opcoes() + ["root@127.0.0.1", "sleep 300 | sleep 301"],
                             stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(9)   # la orden arranca y el «sleep 8» de antes termina
        verificar(cl.ssh("ps").stdout.count(b"sleep") == 2, "una orden larga (sleep 300 | sleep 301) está en marcha")
        p.kill()
        p.wait()
        fim = time.time() + 20
        while time.time() < fim and b"sleep" in cl.ssh("ps").stdout:
            time.sleep(0.5)
        verificar(b"sleep" not in cl.ssh("ps").stdout, "al irse el cliente, la orden y los procesos que lanzó se terminan")
        # exec_utf8 (se lee en cada conexión): la salida sin terminal pasa de Latin-1 a UTF-8
        r = cl.ssh("echo año")
        verificar(r.stdout == "año\n".encode("latin-1"), "sin terminal y de serie, la salida va tal cual (Latin-1)")
        cl.ssh("mkdir /home/.ssh; echo exec_utf8=si > /home/.ssh/sshd.conf")
        r = cl.ssh("echo año; echo señal >&2")
        verificar(r.stdout == "año\n".encode("utf-8") and r.stderr == "señal\n".encode("utf-8"),
                  "con exec_utf8=si en /home/.ssh/sshd.conf, en UTF-8 (salida y errores)")
        cl.ssh("rm /home/.ssh/sshd.conf")

        # --- scp y sftp
        grande = os.urandom(700 * 1024 + 123)
        local = os.path.join(TRAB, "subir.bin")
        with open(local, "wb") as f:
            f.write(grande)
        t0 = time.time()
        r = cl.scp(local, "root@127.0.0.1:/home/subido.bin")
        verificar(r.returncode == 0, f"scp al SaguiOS de 700 KB ({time.time() - t0:.1f} s)")
        r = cl.ssh("sha256sum /home/subido.bin")
        verificar(suma(grande).encode() in r.stdout, "la suma SHA-256 en el SaguiOS es la del archivo enviado")
        vuelta = os.path.join(TRAB, "bajado.bin")
        if os.path.exists(vuelta):
            os.remove(vuelta)
        t0 = time.time()
        r = cl.scp("root@127.0.0.1:/home/subido.bin", vuelta)
        verificar(r.returncode == 0 and open(vuelta, "rb").read() == grande,
                  f"scp de vuelta al PC: el archivo es idéntico ({time.time() - t0:.1f} s)")
        # sobre un archivo que ya existe y es mayor: no quedan restos del anterior
        corto = os.path.join(TRAB, "corto.bin")
        with open(corto, "wb") as f:
            f.write(grande[:5000])
        r = cl.scp(corto, "root@127.0.0.1:/home/subido.bin")
        r2 = cl.ssh("sha256sum /home/subido.bin")
        verificar(r.returncode == 0 and suma(grande[:5000]).encode() in r2.stdout,
                  "scp de un archivo más corto sobre el anterior: queda solo el nuevo")
        peq = os.path.join(TRAB, "nota.txt")
        with open(peq, "w") as f:
            f.write("una nota\n")
        peq2 = os.path.join(TRAB, "nota-vuelta.txt")
        r = cl.sftp(f"pwd\nmkdir /home/carpeta\nput {peq} /home/carpeta/nota.txt\nls -l /home/carpeta\n"
                    f"rename /home/carpeta/nota.txt /home/carpeta/otra.txt\nget /home/carpeta/otra.txt {peq2}\n"
                    "rm /home/carpeta/otra.txt\nrmdir /home/carpeta\nls /home\n")
        s = r.stdout.decode("latin-1")
        verificar(r.returncode == 0, "sftp por lotes: pwd, mkdir, put, ls, rename, get, rm, rmdir")
        verificar("Remote working directory: /home" in s and "nota.txt" in s and open(peq2).read() == "una nota\n",
                  "sftp: carpeta de entrada /home, el listado muestra el archivo y vuelve igual")
        verificar("subido.bin" in s.split("ls /home")[-1] and "carpeta" not in s.split("ls /home")[-1],
                  "sftp: la carpeta se borró y el listado de /home tiene subido.bin")
        r = cl.sftp("get /home/no-existe.bin /dev/null\n")
        verificar(r.returncode != 0 and b"not found" in r.stderr + r.stdout or b"No such file" in r.stderr + r.stdout,
                  "sftp: pedir un archivo que no existe da error (y la sesión sigue en pie)")

        # --- terminal interactivo
        r = cl.ssh(None, entrada=b"echo PTY-$((6*7))\nexit\n", args=["-tt"], tempo=60)
        verificar(b"PTY-42" in r.stdout and b"sagui:" in r.stdout and r.returncode == 0,
                  f"sesión interactiva con terminal (ssh -tt): sagsh responde ({r.stdout[-120:]!r})")
        r = cl.ssh("echo año; exit 3", args=["-tt"], tempo=60)
        verificar("año".encode("utf-8") in r.stdout and r.returncode == 3,
                  "orden con terminal (ssh -t): acentos en UTF-8 y código de salida")

        # --- rechazos
        marca = len(m.serial())
        r = cl.ssh("echo no", chave=outra)
        verificar(r.returncode == 255 and b"Permission denied" in r.stderr and b"no" not in r.stdout,
                  "una clave no autorizada se rechaza (Permission denied)")
        esperar_log(m, "clave NO autorizada SHA256:", 20, marca)
        r = cl.ssh("echo no", extra=["PubkeyAuthentication=no", "PreferredAuthentications=password,keyboard-interactive",
                                     "BatchMode=no", "NumberOfPasswordPrompts=1"])
        verificar(r.returncode == 255 and b"Permission denied (publickey)" in r.stderr,
                  "la contraseña no se admite: el servidor solo ofrece publickey")
        for opcao, falta in (("Ciphers=aes128-ctr", "no matching cipher"),
                             ("KexAlgorithms=ecdh-sha2-nistp256", "no matching key exchange"),
                             ("HostKeyAlgorithms=rsa-sha2-256", "no matching host key")):
            r = cl.ssh("echo no", extra=[opcao, "LogLevel=INFO"])
            verificar(r.returncode == 255 and falta.encode() in r.stderr and r.stdout == b"",
                      f"cliente con solo {opcao}: rechazo limpio ({falta})")
        c = socket.create_connection(("127.0.0.1", porta), timeout=10)
        c.settimeout(10)
        c.recv(200)
        c.sendall(b"GET / HTTP/1.0\r\n\r\n")
        resto = b""
        try:
            while True:
                x = c.recv(200)
                if not x:
                    break
                resto += x
        except OSError:
            pass
        c.close()
        verificar(b"Protocol mismatch" in resto, "lo que no es SSH se corta (Protocol mismatch)")
        # un paquete en claro con un tamaño absurdo tras la identificación
        c = socket.create_connection(("127.0.0.1", porta), timeout=10)
        c.settimeout(10)
        c.recv(200)
        c.sendall(b"SSH-2.0-prueba_h248\r\n" + b"\xff\xff\xff\xf0" + b"\x00" * 64)
        try:
            while c.recv(4096):
                pass
        except OSError:
            pass
        c.close()
        esperar_log(m, "de paquete no v", 20, marca)
        r = cl.ssh("echo sigo-vivo")
        verificar(r.stdout == b"sigo-vivo\n", "tras los rechazos y la basura, el servidor sigue atendiendo")

        # --- límite de conexiones a la vez (4): la quinta se cierra sin atender
        time.sleep(1.5)   # que la sesión anterior termine de cerrarse
        abertas = []
        for _ in range(4):
            c = socket.create_connection(("127.0.0.1", porta), timeout=10)
            c.settimeout(10)
            verificar(c.recv(200).startswith(b"SSH-2.0-"), f"conexión {len(abertas) + 1} de 4 atendida")
            abertas.append(c)
        marca = len(m.serial())
        verificar(banner(porta) == b"", "la quinta conexión a la vez no se atiende")
        esperar_log(m, "demasiadas conexiones a la vez", 10, marca)
        for c in abertas:
            c.close()
        time.sleep(1)
        r = cl.ssh("echo hay-sitio")
        verificar(r.stdout == b"hay-sitio\n", "al cerrarse, vuelve a haber sitio")

        trampas(m, porta, chave)
        r = cl.ssh("echo tras-las-trampas")
        verificar(r.stdout == b"tras-las-trampas\n", "tras las trampas, el servidor sigue atendiendo")

        # --- una clave más, subida por scp a /home/.ssh/authorized_keys
        r = cl.ssh("echo no", chave=segunda)
        verificar(r.returncode == 255, "la segunda clave aún no entra")
        r = cl.scp(segunda + ".pub", "root@127.0.0.1:/home/.ssh/authorized_keys")
        verificar(r.returncode == 0, "scp de la clave pública a /home/.ssh/authorized_keys")
        r = cl.ssh("echo segunda-clave", chave=segunda)
        verificar(r.returncode == 0 and r.stdout == b"segunda-clave\n", "la clave añadida en /home/.ssh/authorized_keys entra")
        out = m.comando("sshd --claves")
        verificar("2 clave(s) autorizada(s)" in out and "clave_segunda" in out, "sshd --claves lista las dos")

        # --- el registro
        r = cl.ssh("cat /tmp/.servicios/sshd.log /logs/sshd.log 2>/dev/null")
        log = r.stdout.decode("latin-1")
        for texto in ("escuchando en el puerto 22", "10.0.2.2:", "autenticado con la clave SHA256:", "orden: echo hola; uname",
                      "clave NO autorizada", "intento con contrase", "subsistema sftp", "sftp: abrir /home/subido.bin",
                      "sin algoritmo com", "shell con terminal", "el proceso", "no es un cliente SSH-2",
                      "de paquete no v"):
            verificar(texto in log, f"el registro tiene: {texto}")
        r = cl.ssh("cat /home/.ssh/ssh_host_ed25519_key")
        semilla = r.stdout.split()[-1].decode() if r.stdout.split() else "?"
        verificar(len(semilla) == 44 and semilla not in log and semilla not in m.serial(),
                  "la clave privada del servidor no aparece en el registro ni en la serie")
        m.captura("H248-sshd")

        # --- bloqueo de una dirección que falla muchas veces seguidas (aquí 10; de serie 5)
        marca = len(m.serial())
        for _ in range(10):
            c = socket.create_connection(("127.0.0.1", porta), timeout=10)
            c.settimeout(10)
            try:
                c.recv(200)
                c.sendall(b"basura\r\n")
                while c.recv(200):
                    pass
            except OSError:
                pass
            c.close()
        esperar_log(m, "10.0.2.2 bloqueada 8 s", 20, marca)
        r = cl.ssh("echo no", tempo=40)
        verificar(r.returncode == 255 and b"no" not in r.stdout, "tras 10 conexiones fallidas seguidas, la dirección queda bloqueada")
        esperar_log(m, "rechazada: direcci", 10, marca)
        time.sleep(9)
        r = cl.ssh("echo de-vuelta")
        verificar(r.stdout == b"de-vuelta\n", "pasado el tiempo de bloqueo vuelve a entrar")

        # --- parar: el puerto se cierra
        out = m.comando("servicio parar sshd", 30)
        verificar("sshd: parado" in out, "servicio parar sshd")
        time.sleep(1)
        verificar(banner(porta) == b"", "con el servicio parado ya no hay nadie en el puerto")
        m.comando("servicio desactivar sshd", 30)
        out = m.comando("servicio listar")
        verificar(re.search(r"^sshd\s+parado", out, re.M), "servicio desactivar sshd: vuelve a estar como de serie")


def main():
    os.makedirs(TRAB, exist_ok=True)
    for prog in ("ssh", "scp", "sftp", "ssh-keygen", "ssh-keyscan"):
        if not shutil.which(prog):
            print(f"FALHOU: falta {prog} (OpenSSH) en el anfitrión")
            return 1
    partes = sys.argv[1:] or ["unidad", "qemu"]
    if "unidad" in partes:
        unidad()
    if "qemu" in partes:
        maquina()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
