#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H317 (D317) - el cliente ssh de SaguiOS (ssh, ssh-keygen, scp).

En el anfitrión:
  1. tests/ssh_cliente_unidad.c (también con -fsanitize=address,undefined): el
     transporte común con paquetes buenos, cortados, manipulados y al azar; KEXINIT
     y ECDH_REPLY malformados; known_hosts; claves privadas; «~.»; teclas y UTF-8;
  2. las claves privadas que escribe SaguiOS las lee el ssh-keygen de OpenSSH, y al
     revés.

En QEMU (red de usuario: el anfitrión es 10.0.2.2), contra un servidor SSH escrito
aquí con la biblioteca «cryptography» (otra implementación de curve25519-sha256,
ssh-ed25519 y chacha20-poly1305@openssh.com, con el modo estricto), que admite
contraseña y clave pública y sabe portarse mal; y contra el sshd del propio
SaguiOS por 127.0.0.1. Si el anfitrión tiene el sshd de OpenSSH (SSHD=ruta o
/usr/sbin/sshd), también contra él:
  - ssh-keygen -t ed25519 crea el par en /home/.ssh;
  - primera conexión: la huella SHA256 y la pregunta «sí/no»; known_hosts guardado;
  - contraseña sin eco (no sale en la pantalla ni en la serie);
  - sesión interactiva con terminal: pty-req con el tamaño, un «echo», Ctrl+C
    llega al otro lado, «~.» corta, Ctrl+D y «exit» terminan;
  - clave pública generada en SaguiOS; orden remota con su salida y su código;
  - modo tubería (echo ... | ssh máquina cat) y scp en los dos sentidos (binario);
  - clave del servidor cambiada: rechazo, sin tocar known_hosts; ssh-keygen -R;
  - servidor hostil (tamaño absurdo, etiqueta mala, firma mala, datos fuera de la
    ventana, KEXINIT roto): el cliente corta con un mensaje y vuelve al indicador;
  - el sshd de SaguiOS: orden, código de salida y scp por 127.0.0.1.
"""
import hashlib
import os
import re
import shutil
import socket
import struct
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, FalhaTeste  # noqa: E402

TRAB = os.path.join(SAIDA, "h317")
SENHA = "segredo317"


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


# ------------------------------------------------------------ anfitrión

def unidad():
    os.makedirs(TRAB, exist_ok=True)
    exe = os.path.join(TRAB, "ssh_cliente_unidad")
    fontes = [os.path.join(RAIZ, "tests", "ssh_cliente_unidad.c"), os.path.join(RAIZ, "kernel", "lib", "sha256.c")]
    fontes += [os.path.join(RAIZ, "userland", "libtls", f + ".c") for f in ("ed25519", "hash", "chacha", "sshcifra", "x25519")]
    base = ["gcc", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "userland", "inc"),
            "-I", os.path.join(RAIZ, "kernel", "inc"), "-I", os.path.join(RAIZ, "userland", "programas")]
    r = subprocess.run(base + ["-O2", "-o", exe] + fontes, capture_output=True, text=True)
    verificar(r.returncode == 0 and "warning" not in r.stderr, "ssh_cliente_unidad.c compila sin avisos" + r.stderr[:2000])
    r = subprocess.run([exe], capture_output=True, text=True, timeout=600)
    n = re.search(r"ssh_cliente_unidad: (\d+)/(\d+) comprobaciones correctas", r.stdout)
    verificar(r.returncode == 0 and n and n.group(1) == n.group(2),
              f"transporte, mensajes malformados, known_hosts, claves, escape y terminal: {r.stdout.strip()[-400:]}")
    exe2 = exe + "-asan"
    r = subprocess.run(base + ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-o", exe2] + fontes,
                       capture_output=True, text=True)
    if r.returncode == 0:
        r = subprocess.run([exe2], capture_output=True, text=True, timeout=900)
        verificar(r.returncode == 0, "lo mismo con -fsanitize=address,undefined (paquetes y mensajes al azar)" + r.stderr[-1500:])
    else:
        print("  AVISO: sin los sanitizadores de gcc en este anfitrión; se omite")
    # el formato de las claves, contra el ssh-keygen de OpenSSH
    if shutil.which("ssh-keygen"):
        k1 = os.path.join(TRAB, "clave_saguios")
        with open(k1, "wb") as f:
            f.write(subprocess.run([exe, "escribir", "37" * 32, "prueba@h317"], capture_output=True, check=True).stdout)
        os.chmod(k1, 0o600)
        r = subprocess.run(["ssh-keygen", "-y", "-f", k1], capture_output=True, text=True)
        from_ref = None
        try:
            from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
            from cryptography.hazmat.primitives import serialization as ser
            import base64
            pub = Ed25519PrivateKey.from_private_bytes(bytes.fromhex("37" * 32)).public_key().public_bytes(
                ser.Encoding.Raw, ser.PublicFormat.Raw)
            from_ref = "ssh-ed25519 " + base64.b64encode(_str(b"ssh-ed25519") + _str(pub)).decode()
        except ImportError:
            pass
        verificar(r.returncode == 0 and r.stdout.startswith("ssh-ed25519 ") and (from_ref is None or r.stdout.startswith(from_ref)),
                  f"ssh-keygen de OpenSSH lee la clave privada escrita por SaguiOS: {r.stdout.strip()[:80]} {r.stderr[:200]}")
        k2 = os.path.join(TRAB, "clave_openssh")
        for f in (k2, k2 + ".pub"):
            if os.path.exists(f):
                os.remove(f)
        subprocess.run(["ssh-keygen", "-q", "-t", "ed25519", "-N", "", "-C", "pc@anfitrion", "-f", k2], check=True)
        r = subprocess.run([exe, "leer", k2], capture_output=True, text=True)
        import base64
        pub_pc = base64.b64decode(open(k2 + ".pub").read().split()[1])[-32:]
        verificar(r.returncode == 0 and r.stdout.split()[0] == pub_pc.hex() and "pc@anfitrion" in r.stdout,
                  "SaguiOS lee una clave privada creada por el ssh-keygen de OpenSSH")


# ------------------------------------------------------------ un servidor SSH mínimo (anfitrión)

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

    def u8(self):
        v = self.d[self.pos]
        self.pos += 1
        return v

    def u32(self):
        v = struct.unpack(">I", self.d[self.pos:self.pos + 4])[0]
        self.pos += 4
        return v

    def str(self):
        n = self.u32()
        v = self.d[self.pos:self.pos + n]
        self.pos += n
        return v


class Conexao:
    """Una conexión del lado del servidor: SSH-2 con curve25519-sha256 (modo estricto),
    ssh-ed25519 y chacha20-poly1305@openssh.com."""

    CIFRA = b"chacha20-poly1305@openssh.com"

    def __init__(self, srv, s):
        self.srv, self.s = srv, s
        self.s.settimeout(60)
        self.buf = b""
        self.seq_tx = self.seq_rx = 0
        self.ktx = self.krx = None
        self.vs = b"SSH-2.0-h317_servidor"

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

    def pacote(self, conteudo, tam_falso=None, estragar=False):
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
        pkt = bytearray(el + ec + self._etiqueta(self.ktx, self.seq_tx, el + ec))
        if estragar:
            pkt[10] ^= 0x01
        return bytes(pkt)

    def enviar(self, conteudo=None, cru=None):
        d = cru if cru is not None else self.pacote(conteudo)
        self.seq_tx = (self.seq_tx + 1) & 0xFFFFFFFF
        self.s.sendall(d)

    def receber(self):
        if not self.krx:
            corpo = self._ler(struct.unpack(">I", self._ler(4))[0])
        else:
            el = self._ler(4)
            tam = struct.unpack(">I", self._cc(self.krx[32:], 0, self.seq_rx, el))[0]
            if tam > 40000:
                raise FalhaTeste("el cliente mandó un tamaño absurdo")
            ec = self._ler(tam)
            if self._ler(16) != self._etiqueta(self.krx, self.seq_rx, el + ec):
                raise FalhaTeste("la etiqueta de un paquete del cliente no coincide")
            corpo = self._cc(self.krx[:32], 1, self.seq_rx, ec)
        self.seq_rx = (self.seq_rx + 1) & 0xFFFFFFFF
        return corpo[1:len(corpo) - corpo[0]]

    def log(self, t):
        self.srv.eventos.append(t)

    # -------------------------------------------------- intercambio de claves
    def kex(self):
        from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey, X25519PublicKey
        from cryptography.hazmat.primitives import serialization as ser
        modo = self.srv.modo
        self.s.sendall(self.vs + b"\r\n")
        while b"\n" not in self.buf:
            x = self.s.recv(4096)
            if not x:
                raise EOFError
            self.buf += x
        vc, self.buf = self.buf.split(b"\n", 1)
        vc = vc.rstrip(b"\r")
        self.log("version " + vc.decode("latin-1"))
        kexalgs = b"curve25519-sha256,kex-strict-s-v00@openssh.com"
        if modo == "kexinit_roto":
            is_ = bytes([20]) + os.urandom(16) + _u32(0x7FFFFFF0) + b"curve"
        else:
            is_ = bytes([20]) + os.urandom(16) + _str(kexalgs) + _str(b"ssh-ed25519") + _str(self.CIFRA) * 2 + \
                _str(b"hmac-sha2-256") * 2 + _str(b"none") * 2 + _str(b"") * 2 + b"\0" + _u32(0)
        self.enviar(is_)
        ic = self.receber()
        if ic[0] != 20:
            raise FalhaTeste("el primer paquete del cliente no es KEXINIT")
        l = Leitor(ic, 17)
        listas = [l.str() for _ in range(10)]
        self.log("kexinit " + listas[0].decode())
        estrito = b"kex-strict-c-v00@openssh.com" in listas[0].split(b",")
        r = self.receber()
        if r[0] != 30:
            raise FalhaTeste(f"se esperaba ECDH_INIT y llegó {r[0]}")
        qc = Leitor(r, 1).str()
        priv = X25519PrivateKey.generate()
        qs = priv.public_key().public_bytes(ser.Encoding.Raw, ser.PublicFormat.Raw)
        k = priv.exchange(X25519PublicKey.from_public_bytes(qc))
        hk = self.srv.chave
        pub = hk.public_key().public_bytes(ser.Encoding.Raw, ser.PublicFormat.Raw)
        ks = _str(b"ssh-ed25519") + _str(pub)
        h = hashlib.sha256(_str(vc) + _str(self.vs) + _str(ic) + _str(is_) + _str(ks) + _str(qc) + _str(qs) + _mpint(k)).digest()
        firma = hk.sign(h if modo != "firma_mala" else h[::-1])
        self.enviar(bytes([31]) + _str(ks) + _str(qs) + _str(_str(b"ssh-ed25519") + _str(firma)))
        self.enviar(bytes([21]))
        sid = h

        def derivar(letra):
            k1 = hashlib.sha256(_mpint(k) + h + letra + sid).digest()
            return k1 + hashlib.sha256(_mpint(k) + h + k1).digest()
        self.ktx = derivar(b"D")
        if estrito:
            self.seq_tx = 0
        r = self.receber()
        if r[0] != 21:
            raise FalhaTeste(f"se esperaba NEWKEYS y llegó {r[0]}")
        self.krx = derivar(b"C")
        if estrito:
            self.seq_rx = 0
        self.sid = sid
        self.log("kex ok" + (" estricto" if estrito else ""))

    # -------------------------------------------------- autenticación
    def autenticar(self):
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PublicKey
        r = self.receber()
        if r[0] != 5:
            raise FalhaTeste("se esperaba SERVICE_REQUEST")
        modo = self.srv.modo
        if modo == "tamano":
            self.enviar(cru=self.pacote(bytes([6]) + _str(b"ssh-userauth"), tam_falso=0x00FFFFF0))
            self.log("hostil: tamaño")
            return False
        if modo == "mac":
            self.enviar(cru=self.pacote(bytes([6]) + _str(b"ssh-userauth"), estragar=True))
            self.log("hostil: etiqueta")
            return False
        self.enviar(bytes([6]) + _str(b"ssh-userauth"))
        metodos = []
        if self.srv.autorizada:
            metodos.append(b"publickey")
        if self.srv.senha:
            metodos.append(b"password")
        for _ in range(12):
            r = self.receber()
            l = Leitor(r, 1)
            usuario, servico, metodo = l.str(), l.str(), l.str()
            self.usuario = usuario
            if metodo == b"publickey" and self.srv.autorizada:
                com_firma = l.u8()
                alg, blob = l.str(), l.str()
                lb = Leitor(blob)
                if com_firma and alg == b"ssh-ed25519" and lb.str() == b"ssh-ed25519":
                    pub = lb.str()
                    firma_blob = l.str()
                    lf = Leitor(firma_blob)
                    lf.str()
                    firma = lf.str()
                    dados = _str(self.sid) + r[:l.pos - len(firma_blob) - 4]
                    if pub == self.srv.autorizada:
                        try:
                            Ed25519PublicKey.from_public_bytes(pub).verify(firma, dados)
                            self.log(f"publickey ok {usuario.decode()}")
                            self.enviar(bytes([52]))
                            return True
                        except Exception:
                            self.log("publickey firma mala")
                    else:
                        self.log("publickey no autorizada")
            elif metodo == b"password" and self.srv.senha:
                l.u8()
                s = l.str()
                if s == self.srv.senha.encode():
                    self.log(f"password ok {usuario.decode()}")
                    self.enviar(bytes([52]))
                    return True
                self.log("password mala")
            elif metodo == b"none":
                self.log("none")
            self.enviar(bytes([51]) + _str(b",".join(metodos)) + b"\0")
        return False

    # -------------------------------------------------- la sesión
    def canal_dados(self, d, extra=False):
        for i in range(0, len(d), 16384):
            p = d[i:i + 16384]
            if extra:
                self.enviar(bytes([95]) + _u32(self.remoto) + _u32(1) + _str(p))
            else:
                self.enviar(bytes([94]) + _u32(self.remoto) + _str(p))

    def fim(self, codigo):
        self.enviar(bytes([98]) + _u32(self.remoto) + _str(b"exit-status") + b"\0" + _u32(codigo))
        self.enviar(bytes([96]) + _u32(self.remoto))
        self.enviar(bytes([97]) + _u32(self.remoto))
        self.log(f"fin {codigo}")

    def ordem(self, cmd, entrada_inicial):
        """Unas pocas órdenes de mentira."""
        c = cmd.decode("utf-8", "replace")
        self.log("exec " + c)
        if c.startswith("echo "):
            self.canal_dados(c[5:].encode("utf-8") + b"\n")
            return self.fim(0)
        m = re.match(r"codigo (\d+)$", c)
        if m:
            self.canal_dados(b"saliendo\n")
            self.canal_dados(b"aviso por stderr\n", extra=True)
            return self.fim(int(m.group(1)))
        m = re.match(r"cat > '([^']*)'$", c)
        if c == "cat" or m:
            dados = self.ler_entrada(entrada_inicial)
            if m:
                self.srv.arquivos[m.group(1)] = dados
                self.log(f"guardado {m.group(1)} {len(dados)}")
            else:
                self.canal_dados(dados)
            return self.fim(0)
        m = re.match(r"cat '([^']*)'$", c)
        if m:
            d = self.srv.arquivos.get(m.group(1))
            if d is None:
                self.canal_dados(b"cat: no existe\n", extra=True)
                return self.fim(1)
            self.canal_dados(d)
            return self.fim(0)
        if self.srv.modo == "ventana":
            self.enviar(bytes([94]) + _u32(self.remoto) + _str(b"x" * 33000))
            self.log("hostil: ventana")
            return
        self.canal_dados(b"orden desconocida\n", extra=True)
        self.fim(127)

    def ler_entrada(self, inicial):
        d = inicial
        while True:
            r = self.receber()
            if r[0] == 94:
                l = Leitor(r, 1)
                l.u32()
                d += l.str()
            elif r[0] == 96:
                return d
            elif r[0] == 97:
                return d

    def shell(self):
        """Un shell de mentira con eco, como el de un terminal: procesa líneas."""
        self.canal_dados(b"bienvenido a h317\r\nh317$ ")
        linha = b""
        while True:
            r = self.receber()
            l = Leitor(r, 1)
            if r[0] == 98:
                l.u32()
                nome = l.str()
                l.u8()
                if nome == b"window-change":
                    c, f = l.u32(), l.u32()
                    self.log(f"window-change {c}x{f}")
                continue
            if r[0] in (96, 97):
                self.log("shell: eof")
                return self.fim(0)
            if r[0] != 94:
                continue
            l.u32()
            for b in l.str():
                if b == 3:
                    self.log("recibido ^C")
                    linha = b""
                    self.canal_dados(b"^C\r\nh317$ ")
                elif b == 4 and not linha:
                    self.log("recibido ^D")
                    self.canal_dados(b"\r\n")
                    return self.fim(0)
                elif b == 0x7F:
                    if linha:
                        linha = linha[:-1]
                        self.canal_dados(b"\b \b")
                elif b == 13:
                    self.canal_dados(b"\r\n")
                    t = linha.decode("utf-8", "replace")
                    self.log("linea " + t)
                    linha = b""
                    if t == "exit":
                        return self.fim(0)
                    if t.startswith("echo "):
                        self.canal_dados(t[5:].upper().encode() + b"\r\n")
                    self.canal_dados(b"h317$ ")
                elif b == 27 or 32 <= b < 127 or b >= 0x80:
                    linha += bytes([b])
                    self.canal_dados(bytes([b]))
                else:
                    self.log(f"control {b}")

    def sessao(self):
        while True:
            r = self.receber()
            if r[0] == 90:
                l = Leitor(r, 1)
                l.str()
                self.remoto = l.u32()
                self.enviar(bytes([91]) + _u32(self.remoto) + _u32(5) + _u32(1 << 20) + _u32(32768))
                continue
            if r[0] == 98:
                l = Leitor(r, 1)
                l.u32()
                nome = l.str()
                resp = l.u8()
                if nome == b"pty-req":
                    term, c, f = l.str(), l.u32(), l.u32()
                    self.log(f"pty-req {term.decode()} {c}x{f}")
                    if resp:
                        self.enviar(bytes([99]) + _u32(self.remoto))
                    continue
                if nome == b"exec":
                    cmd = l.str()
                    if resp:
                        self.enviar(bytes([99]) + _u32(self.remoto))
                    self.ordem(cmd, b"")
                    return
                if nome == b"shell":
                    if resp:
                        self.enviar(bytes([99]) + _u32(self.remoto))
                    self.shell()
                    return
                if resp:
                    self.enviar(bytes([100]) + _u32(self.remoto))
                continue
            if r[0] == 1:
                return

    def atender(self):
        try:
            self.kex()
            if self.autenticar():
                self.sessao()
                # esperar el cierre del cliente
                try:
                    while True:
                        self.receber()
                except Exception:
                    pass
        except (EOFError, OSError, struct.error, IndexError):
            pass
        except FalhaTeste as e:
            self.log("error: " + str(e))
        except Exception as e:  # noqa: BLE001
            self.log(f"excepción: {type(e).__name__}: {e}")
        finally:
            self.s.close()


class Servidor:
    def __init__(self):
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        self.chave = Ed25519PrivateKey.generate()
        self.autorizada = None
        self.senha = SENHA
        self.modo = None
        self.eventos = []
        self.arquivos = {}
        self.ls = socket.socket()
        self.ls.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.ls.bind(("127.0.0.1", 0))
        self.porta = self.ls.getsockname()[1]
        self.ls.listen(4)
        threading.Thread(target=self.bucle, daemon=True).start()

    def bucle(self):
        while True:
            try:
                s, _ = self.ls.accept()
            except OSError:
                return
            threading.Thread(target=Conexao(self, s).atender, daemon=True).start()

    def huella(self):
        from cryptography.hazmat.primitives import serialization as ser
        import base64
        pub = self.chave.public_key().public_bytes(ser.Encoding.Raw, ser.PublicFormat.Raw)
        blob = _str(b"ssh-ed25519") + _str(pub)
        return "SHA256:" + base64.b64encode(hashlib.sha256(blob).digest()).decode().rstrip("=")

    def esperar(self, texto, tempo=30):
        fim = time.time() + tempo
        while time.time() < fim:
            if any(texto in e for e in self.eventos):
                return True
            time.sleep(0.1)
        raise FalhaTeste(f"el servidor no vio «{texto}»: {self.eventos[-8:]}")


# ------------------------------------------------------------ QEMU

def construir_imagen():
    b = os.path.join(RAIZ, "build")
    img = os.path.join(TRAB, "h317.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", img, "--vesa", "--console", "--dados-mb", "16"],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def esperar_serial(m, texto, desde, tempo=60):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial()[desde:]:
            return m.serial()[desde:]
        time.sleep(0.2)
    raise FalhaTeste(f"no apareció {texto!r} en la serie: {m.serial()[desde:][-600:]!r}")


def esperar_indicador(m, desde, tempo=60):
    """Espera a que vuelva el indicador del shell después de "desde"."""
    fim = time.time() + tempo
    while time.time() < fim:
        if re.search(r"sagui:[^\n]*\$ ?$", m.serial()[desde:].rstrip("\n")):
            return m.serial()[desde:]
        if "sagui:" in m.serial()[desde:]:
            return m.serial()[desde:]
        time.sleep(0.2)
    raise FalhaTeste(f"no volvió el indicador: {m.serial()[desde:][-600:]!r}")


def comando_ate(m, texto, tempo=120):
    """Como m.comando, pero espera a una marca propia: el registro del sshd
    («usuario sagui: autenticado») también contiene «sagui:»."""
    marca = len(m.serial())
    m.digitar(texto + "; echo fin-de-$((300+17))\n", 0.04)
    s = esperar_serial(m, "\nfin-de-317", marca, tempo)
    return s.replace("\r", "")


def maquina():
    srv = Servidor()
    P = srv.porta
    img = construir_imagen()
    with Maquina("h317", "ide", imagem=img, snapshot=True, extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial("sagui:/$", 120)
        fim = time.time() + 40
        while time.time() < fim and "(DHCP)" not in m.serial():
            time.sleep(0.3)

        # --- ssh-keygen
        out = m.comando("ssh-keygen -t ed25519 -N ''", 60)
        h = re.search(r"Huella: (SHA256:[A-Za-z0-9+/]{43})", out)
        verificar(h and "/home/.ssh/id_ed25519.pub" in out, "ssh-keygen -t ed25519 crea el par en /home/.ssh")
        out = m.comando("cat /home/.ssh/id_ed25519.pub", 30)
        lp = re.search(r"ssh-ed25519 ([A-Za-z0-9+/=]{68}) (\S+)", out)
        verificar(lp, "la clave pública en el formato de OpenSSH")
        import base64
        pub = base64.b64decode(lp.group(1))[-32:]
        blob = _str(b"ssh-ed25519") + _str(pub)
        verificar("SHA256:" + base64.b64encode(hashlib.sha256(blob).digest()).decode().rstrip("=") == h.group(1),
                  "la huella que enseña ssh-keygen es la de la clave pública")
        out = m.comando("ssh-keygen -t ed25519 -N ''", 30)
        verificar("ya existe" in out, "ssh-keygen no sobrescribe una clave que ya existe")
        out = m.comando("ssh-keygen -l", 30)
        verificar(h.group(1) in out, "ssh-keygen -l da la misma huella")
        verificar("PRIVATE KEY" not in m.serial(), "la clave privada no sale por la pantalla ni por la serie")

        # --- primera conexión: huella, «sí», contraseña sin eco, sesión interactiva
        srv.autorizada = None
        srv.senha = SENHA
        marca = len(m.serial())
        m.digitar(f"ssh -p {P} h317@10.0.2.2\n", 0.04)
        s = esperar_serial(m, "(sí/no)", marca, 60)
        verificar(srv.huella() in s and "[10.0.2.2]:%d" % P in s,
                  f"primera conexión: enseña la huella SHA256 del servidor ({srv.huella()}) y pregunta")
        m.digitar("si\n", 0.05)
        esperar_serial(m, "Contrase", marca, 30)
        m.digitar(SENHA + "\n", 0.05)
        esperar_serial(m, "h317$ ", marca, 40)
        verificar(SENHA not in m.serial(), "la contraseña no sale por la pantalla (sin eco) ni por la serie")
        srv.esperar("password ok h317")
        verificar(any(e.startswith("kex ok estricto") for e in srv.eventos), "intercambio de claves con el modo estricto")
        pty = [e for e in srv.eventos if e.startswith("pty-req ")]
        verificar(pty and re.match(r"pty-req \S+ \d+x\d+", pty[-1]), f"pty-req con el TERM y el tamaño: {pty[-1:]}")
        marca2 = len(m.serial())
        m.digitar("echo interactivo\n", 0.05)
        esperar_serial(m, "INTERACTIVO", marca2, 30)
        verificar(True, "sesión interactiva: el eco y la respuesta del otro lado llegan a la pantalla")
        m.digitar("exit\n", 0.05)
        esperar_indicador(m, marca2, 30)
        srv.esperar("fin 0")
        out = m.comando("cat /home/.ssh/known_hosts", 30)
        verificar(f"[10.0.2.2]:{P} ssh-ed25519 " in out, "la máquina queda en /home/.ssh/known_hosts")

        # --- clave pública (la de SaguiOS) y orden remota con su código
        srv.autorizada = pub
        srv.senha = None
        out = m.comando(f"ssh -p {P} h317@10.0.2.2 echo hola-h317; echo codigo=$?", 60)
        verificar("hola-h317" in out and "codigo=0" in out and "(sí/no)" not in out,
                  "orden remota con la clave pública creada en SaguiOS: salida y código 0, sin volver a preguntar")
        srv.esperar("publickey ok h317")
        out = m.comando(f"ssh -p {P} h317@10.0.2.2 codigo 7; echo codigo=$?", 60)
        verificar("saliendo" in out and "aviso por stderr" in out and "codigo=7" in out,
                  "el código de salida de la orden remota (7) y su salida de errores")
        out = m.comando(f"ssh -p {P} h317@10.0.2.2 codigo 7 > /home/salida.txt; echo codigo=$?; cat /home/salida.txt", 60)
        verificar("codigo=7" in out and "saliendo" in out, "la salida redirigida a un archivo")

        # --- modo tubería
        out = m.comando(f"echo dato-por-tuberia | ssh -p {P} h317@10.0.2.2 cat", 60)
        verificar("dato-por-tuberia" in out, "modo tubería: echo ... | ssh máquina cat")

        # --- scp
        srv.arquivos["/remoto/bin.dat"] = bytes(range(256)) * 300 + b"fin"
        out = m.comando(f"scp -P {P} h317@10.0.2.2:/remoto/bin.dat /home/bin.dat; sha256sum /home/bin.dat", 120)
        verificar(hashlib.sha256(srv.arquivos["/remoto/bin.dat"]).hexdigest() in out,
                  "scp: traer un archivo binario de 76 KB, igual byte a byte")
        out = m.comando(f"scp -P {P} /home/bin.dat h317@10.0.2.2:/subido/", 120)
        srv.esperar("guardado /subido/bin.dat")
        verificar(srv.arquivos.get("/subido/bin.dat") == srv.arquivos["/remoto/bin.dat"], "scp: llevarlo de vuelta, igual")
        out = m.comando(f"scp -P {P} h317@10.0.2.2:/no/existe /home/nada.txt; echo codigo=$?; ls /home", 60)
        verificar("codigo=1" in out and "nada.txt" not in out.split("codigo=")[1], "scp de algo que no existe: error y sin archivo a medias")

        # --- Ctrl+C, ~. y Ctrl+D en una sesión
        marca = len(m.serial())
        m.digitar(f"ssh -p {P} h317@10.0.2.2\n", 0.04)
        esperar_serial(m, "h317$ ", marca, 40)
        m.digitar("abc", 0.05)
        m.tecla("ctrl-c", 0.2)
        srv.esperar("recibido ^C")
        verificar(True, "Ctrl+C llega al otro lado (no termina el cliente)")
        m.digitar("~.", 0.1)
        esperar_indicador(m, marca, 30)
        verificar("cerrada" in m.serial()[marca:], "«~.» al principio de una línea corta la conexión")
        marca = len(m.serial())
        m.digitar(f"ssh -p {P} h317@10.0.2.2\n", 0.04)
        esperar_serial(m, "h317$ ", marca, 40)
        m.tecla("ctrl-d", 0.2)
        srv.esperar("recibido ^D")
        esperar_indicador(m, marca, 30)
        verificar(True, "Ctrl+D llega al otro lado y la sesión termina")

        # --- servidor hostil
        for modo, texto in (("tamano", "tama"), ("mac", "etiqueta"), ("firma_mala", "firma"),
                            ("ventana", "ventana"), ("kexinit_roto", "KEXINIT")):
            srv.modo = modo
            t0 = time.time()
            out = m.comando(f"ssh -p {P} h317@10.0.2.2 hola; echo codigo=$?", 90)
            verificar("codigo=255" in out and texto in out and time.time() - t0 < 60,
                      f"servidor hostil ({modo}): el cliente corta con un mensaje y no se cuelga")
        srv.modo = None

        # --- la clave del servidor cambia: rechazo
        from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey
        antes = m.comando("cat /home/.ssh/known_hosts", 30)
        srv.chave = Ed25519PrivateKey.generate()
        out = m.comando(f"ssh -p {P} h317@10.0.2.2 echo no-debe-salir; echo codigo=$?", 60)
        verificar("HA CAMBIADO" in out and srv.huella() in out and "codigo=255" in out and out.count("no-debe-salir") == 1,
                  "clave del servidor cambiada: aviso claro con la huella nueva y la conexión se rechaza")
        verificar("(sí/no)" not in out, "sin opción de aceptarla")
        despues = m.comando("cat /home/.ssh/known_hosts", 30)
        verificar(antes.count("ssh-ed25519") == despues.count("ssh-ed25519"), "known_hosts no cambia")
        out = m.comando(f"ssh -o StrictHostKeyChecking=no -p {P} h317@10.0.2.2 echo x; echo codigo=$?", 30)
        verificar("no se admite" in out and "codigo=255" in out, "StrictHostKeyChecking=no no existe")
        out = m.comando(f"ssh-keygen -R [10.0.2.2]:{P}", 30)
        verificar("quitada" in out, "ssh-keygen -R quita la línea (a mano, a propósito)")
        out = m.comando(f"ssh -o StrictHostKeyChecking=accept-new -p {P} h317@10.0.2.2 echo otra-vez; echo codigo=$?", 60)
        verificar("otra-vez" in out and "codigo=0" in out, "después de quitarla, la clave nueva se acepta")

        # --- el sshd de SaguiOS por 127.0.0.1
        m.comando("cp /home/.ssh/id_ed25519.pub /home/.ssh/authorized_keys", 30)
        m.comando("servicio activar sshd", 30)
        m.comando("servicio iniciar sshd", 30)
        fim = time.time() + 40
        while time.time() < fim and "escuchando en el puerto 22" not in m.serial():
            time.sleep(0.3)
        out = comando_ate(m, "ssh -o StrictHostKeyChecking=accept-new sagui@127.0.0.1 'echo desde-sshd; exit 3'; echo codigo=$?", 90)
        verificar("\ndesde-sshd\n" in out and "\ncodigo=3\n" in out, "contra el sshd de SaguiOS: orden y su código de salida (3)")
        out = comando_ate(m, "echo por-sshd > /home/l.txt; scp /home/l.txt sagui@127.0.0.1:/tmp/r.txt; "
                             "scp sagui@127.0.0.1:/tmp/r.txt /home/v.txt; cat /home/v.txt", 120)
        verificar("\npor-sshd\n" in out and "/tmp/r.txt -> /home/v.txt" in out, "scp de ida y vuelta con el sshd de SaguiOS")
        # sesión interactiva con el sagsh del sshd de SaguiOS (terminal remoto con su conversión)
        marca = len(m.serial())
        m.digitar("ssh sagui@127.0.0.1\n", 0.04)
        esperar_serial(m, "shell con terminal", marca, 60)
        time.sleep(1.5)
        m.digitar("echo remoto-$((40+2))\n", 0.05)
        esperar_serial(m, "\nremoto-42", marca, 30)
        m.digitar("exit\n", 0.05)
        esperar_serial(m, "(ssh) terminou com codigo 0", marca, 30)
        verificar(True, "sesión interactiva con el sagsh del sshd de SaguiOS (echo y exit)")
        verificar(SENHA not in m.serial() and "PRIVATE KEY" not in m.serial(), "ni contraseñas ni claves en la serie")
        m.captura("H317-ssh-cliente", guardar=False)
    return srv


def maquina_terminal(srv):
    """En el escritorio, desde el Terminal en ventana: el tamaño del terminal remoto
    sale de la respuesta del Terminal a ESC [ 18 t (no es 80x24) y esa respuesta no
    llega al shell."""
    P = srv.porta
    srv.modo = None
    srv.senha = SENHA
    srv.autorizada = None
    with Maquina("h317t", "ide", snapshot=True, extras=["-nic", "user,model=e1000"]) as m:
        m.esperar_serial("sagui:/$", 180)
        fim = time.time() + 40
        while time.time() < fim and "(DHCP)" not in m.serial():
            time.sleep(0.3)
        n0 = len(srv.eventos)
        marca = len(m.serial())
        m.digitar(f"ssh -o StrictHostKeyChecking=accept-new -p {P} h317@10.0.2.2\n", 0.04)
        esperar_serial(m, "Contrase", marca, 60)
        m.digitar(SENHA + "\n", 0.05)
        esperar_serial(m, "h317$ ", marca, 40)
        pty = [e for e in srv.eventos[n0:] if e.startswith("pty-req ")]
        t = re.match(r"pty-req (\S+) (\d+)x(\d+)", pty[-1]) if pty else None
        verificar(t and t.group(1) == "xterm-256color" and (t.group(2), t.group(3)) != ("80", "24"),
                  f"desde el Terminal: TERM y tamaño de la ventana en el pty-req ({pty[-1:]})")
        m.digitar("echo ventana\n", 0.05)
        esperar_serial(m, "VENTANA", marca, 30)
        m.captura("H317-ssh-terminal", guardar=False)
        m.digitar("exit\n", 0.05)
        esperar_serial(m, "sagui:", len(m.serial()) - 10, 30)
        time.sleep(1.5)
        out = m.comando("echo limpio", 30)
        verificar("8;" not in out.split("echo limpio")[0] and "\nlimpio" in out,
                  "la respuesta del Terminal al tamaño no llega al shell local")


def main():
    try:
        import cryptography  # noqa: F401
        tem_crypto = True
    except ImportError:
        tem_crypto = False
    unidad()
    if not tem_crypto:
        print("  AVISO: sin la biblioteca cryptography de Python no se puede montar el servidor de prueba")
        return
    srv = maquina()
    maquina_terminal(srv or Servidor())


if __name__ == "__main__":
    try:
        main()
        print("H317 OK")
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
