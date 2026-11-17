#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H365 - VPN WireGuard (D363): criptografía, protocolo e interoperabilidad.

Criterio:
  1. (anfitrión, tests/wg_unidad.c) las primitivas de la libtls con los vectores oficiales:
     BLAKE2s (RFC 7693 apéndice B y la referencia de BLAKE2 con clave), HMAC-BLAKE2s y el KDF
     del Noise contra hashlib/hmac de Python, X25519 (RFC 7748, 5.2 y 6.1), ChaCha20-Poly1305
     (RFC 8439, 2.8.2), HChaCha20 (draft-irtf-cfrg-xchacha, 2.2.1), XChaCha20-Poly1305 contra
     HChaCha20 + ChaCha20-Poly1305 de la librería cryptography; base64 de las claves y los
     prefijos de AllowedIPs. Con y sin -fsanitize=address,undefined.
  2. (anfitrión) el motor de userland/libtls/wireguard.c contra un par de referencia en Python
     (tests/wg_referencia.py, primitivas de hashlib y cryptography): saludo en los dos papeles,
     PSK, ping en los dos sentidos, un paquete repetido se descarta, cookie bajo carga y
     renovación de claves con los tiempos acortados.
  3. (QEMU, red de usuario) SaguiOS contra el mismo par de referencia en el anfitrión, sin root:
     wg genkey | wg pubkey (la pública coincide con la de Python); wg-quick check/up con un .conf
     estándar (PSK, DNS); ping y TCP (nc, MSS 1380) por el túnel; DNS del túnel; wg show e
     ifconfig; un paquete repetido se descarta y lo dice wg show; renovación de claves (tiempos
     de prueba); roaming del extremo; SaguiOS como respondedor; basura en el puerto no rompe nada;
     wg-quick down deja la red física como estaba; cookie (el par la exige); AllowedIPs que tapan la
     red local (el socket del demonio sigue saliendo por la física) y 0.0.0.0/0 (la red local queda
     fuera del túnel y lo demás entra).
  4. (QEMU, escritorio, sesión sagui sin root) la ventana genera claves e importa un .conf (600);
     D372: sin elevar, el demonio no puede abrir la interfaz (SOCK_TUN solo root); «Conectar» abre
     el diálogo de administrador (la primera vez se elige la contraseña de root), el demonio corre
     como root, hay saludo y ping; «Desconectar» apaga sin pedir nada; el permiso no se recuerda:
     «wg-quick up» desde el Terminal vuelve a preguntar (Esc = no se enciende; con la contraseña,
     sí) y «wg-quick down» apaga como sagui.
"""
import base64
import functools
import hashlib
import hmac
import http.server
import os
import random
import re
import socket
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, FalhaTeste, RAIZ, SAIDA  # noqa: E402
import wg_referencia as R  # noqa: E402

TRAB = os.path.join(SAIDA, "h365")
FONTES = ["tests/wg_unidad.c", "userland/libtls/wireguard.c", "userland/libtls/blake2s.c", "userland/libtls/chacha.c",
          "userland/libtls/x25519.c", "userland/libtls/hash.c", "kernel/lib/sha256.c"]


def compilar(nome, extra):
    exe = os.path.join(TRAB, nome)
    cmd = ["gcc", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "userland", "inc"),
           "-I", os.path.join(RAIZ, "kernel", "inc")] + extra + ["-o", exe] + [os.path.join(RAIZ, f) for f in FONTES]
    r = subprocess.run(cmd, capture_output=True, text=True)
    return exe, r


def rodar_vetores(exe, linhas):
    r = subprocess.run([exe, "vectores"], input="\n".join(linhas) + "\n", capture_output=True, text=True, timeout=120)
    if r.returncode != 0:
        raise FalhaTeste(f"wg_unidad vectores falló: {r.stderr[-500:]}")
    return r.stdout.split("\n")


H = bytes.hex


def vetores(exe):
    random.seed(365)
    casos, esperado, nomes = [], [], []

    def caso(linha, saida, nome):
        casos.append(linha)
        esperado.append(saida)
        nomes.append(nome)

    # BLAKE2s: RFC 7693 apéndice B y la KAT con clave de la referencia de BLAKE2.
    caso("blake2s 32 - 616263", "508c5e8c327c14e2e1a72ba34eeb452f37458b209ed63a294d999b4c86675982", "BLAKE2s-256(\"abc\") (RFC 7693)")
    k32 = bytes(range(32))
    caso(f"blake2s 32 {H(k32)} -", "48a8997da407876b3d79c0d92325ad3b89cbb754d86ab71aee047ad345fd2c49", "BLAKE2s con clave, mensaje vacío (KAT)")
    caso(f"blake2s 32 {H(k32)} 00", "40d15fee7c328830166ac3f918650f807e7e01e177258cdc0a39b11f598066f1", "BLAKE2s con clave, 1 byte (KAT)")
    for n in (0, 1, 55, 63, 64, 65, 127, 128, 129, 200, 1000):
        m = bytes(random.getrandbits(8) for _ in range(n))
        for tam in (32, 16):
            chave = bytes(random.getrandbits(8) for _ in range(random.choice((0, 16, 32))))
            caso(f"blake2s {tam} {H(chave) if chave else '-'} {H(m) if m else '-'}",
                 hashlib.blake2s(m, key=chave, digest_size=tam).hexdigest(), f"BLAKE2s-{tam * 8} {n} bytes vs hashlib")
        caso(f"blake2s_trozos {H(k32)} {H(m) if m else '-'}", hashlib.blake2s(m, key=k32).hexdigest(), f"BLAKE2s por trozos, {n} bytes")
    for n in (0, 32, 100):
        chave = bytes(random.getrandbits(8) for _ in range(32))
        m = bytes(random.getrandbits(8) for _ in range(n))
        caso(f"hmac {H(chave)} {H(m) if m else '-'}", hmac.new(chave, m, hashlib.blake2s).hexdigest(), f"HMAC-BLAKE2s {n} bytes")
        for q in (1, 2, 3):
            caso(f"kdf {q} {H(chave)} {H(m) if m else '-'}", b"".join(R.KDF(q, chave, m)).hex(), f"KDF{q} del Noise {n} bytes")
    # X25519, RFC 7748 5.2 y 6.1 (Alice y Bob).
    caso("x25519 a546e36bf0527c9d3b16154b82465edd62144c0ac1fc5a18506a2244ba449ac4 "
         "e6db6867583030db3594c1a424b15f7c726624ec26b3353b10a903a6d0ab1c4c",
         "c3da55379de9c6908e94ea4df28d084f32eccf03491c71f754b4075577a28552", "X25519 RFC 7748 5.2 (1)")
    caso("x25519 4b66e9d4d1b4673c5ad22691957d6af5c11b6421e0ea01d42ca4169e7918ba0d "
         "e5210f12786811d3f4b7959d0538ae2c31dbe7106fc03c3efc4cd549c715a493",
         "95cbde9476e8907d7aade45cb4b873f88b595a68799fa152e6f8f7647aac7957", "X25519 RFC 7748 5.2 (2)")
    alice = "77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a"
    caso(f"pub {alice}", "8520f0098930a754748b7ddcb43ef75a0dbf3a0d26381af4eba4a98eaa9b4e6a", "X25519 RFC 7748 6.1: pública de Alice")
    caso(f"x25519 {alice} de9edb7d7b7dc1b4d35b61c2ece435373f8343c85b78674dadfc7e146f882b4f",
         "4a5d9d5ba4ce2de1728e3bf480350f25e07e21c947d19e3376f09b3c1e161742", "X25519 RFC 7748 6.1: secreto compartido")
    for _ in range(4):
        a, b = R.gerar_privada(), R.gerar_privada()
        caso(f"x25519 {H(a)} {H(R.publica_de(b))}", R.DH(a, R.publica_de(b)).hex(), "X25519 vs cryptography")
    # ChaCha20-Poly1305 (RFC 8439, 2.8.2)
    texto = (b"Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, "
             b"sunscreen would be it.")
    caso("chapoly 808182838485868788898a8b8c8d8e8f909192939495969798999a9b9c9d9e9f 070000004041424344454647 "
         f"50515253c0c1c2c3c4c5c6c7 {H(texto)}",
         "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d63dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67e"
         "cd3b3692ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc3ff4def08e4b7a9de576d26586cec64b6116"
         "1ae10b594f09e26a7e902ecbd0600691", "ChaCha20-Poly1305 RFC 8439 2.8.2")
    from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
    for n in (0, 1, 16, 17, 300):
        k = os.urandom(32)
        nn = os.urandom(12)
        aad = os.urandom(random.choice((0, 13, 32)))
        m = os.urandom(n)
        caso(f"chapoly {H(k)} {H(nn)} {H(aad) if aad else '-'} {H(m) if m else '-'}",
             ChaCha20Poly1305(k).encrypt(nn, m, aad).hex(), f"ChaCha20-Poly1305 {n} bytes vs cryptography")
    # HChaCha20 (draft-irtf-cfrg-xchacha-03, 2.2.1) y XChaCha20-Poly1305
    caso(f"hchacha {H(k32)} 000000090000004a0000000031415927",
         "82413b4227b27bfed30e42508a877d73a0f9e4d58a74a853c12ec41326d3ecdc", "HChaCha20 (draft xchacha 2.2.1)")
    for n in (0, 16, 100):
        k = os.urandom(32)
        nn = os.urandom(24)
        aad = os.urandom(16)
        m = os.urandom(n)
        caso(f"xchapoly {H(k)} {H(nn)} {H(aad)} {H(m) if m else '-'}", R.XAEAD(k, nn, m, aad).hex(),
             f"XChaCha20-Poly1305 {n} bytes vs HChaCha20 + cryptography")
    # Claves en base64 y prefijos
    for _ in range(3):
        k = os.urandom(32)
        caso(f"b64 {H(k)}", base64.b64encode(k).decode() + " ok", "base64 de una clave (ida y vuelta)")
    caso("b64ler " + base64.b64encode(os.urandom(31)).decode(), "invalida", "base64 de 31 bytes: no es una clave")
    caso("b64ler AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAB=", "invalida", "base64 con bits sobrantes: no es una clave")
    caso("prefixo 10.66.0.7/24", "0a420000/24", "AllowedIPs 10.66.0.7/24 -> 10.66.0.0/24")
    caso("prefixo 0.0.0.0/0", "00000000/0", "AllowedIPs 0.0.0.0/0")
    caso("prefixo 192.168.1.1", "c0a80101/32", "AllowedIPs sin prefijo = /32")
    caso("prefixo 300.1.1.1/8", "invalido", "AllowedIPs con un octeto de más de 255")
    caso("prefixo 10.0.0.0/33", "invalido", "AllowedIPs /33")
    saidas = rodar_vetores(exe, casos)
    erros = [l for l in saidas if l.startswith("ERRO")]
    verificar(not erros, f"descifrado de vuelta y rechazo de lo adulterado: {erros[:3]}")
    saidas = [l for l in saidas if l and not l.startswith("ERRO")]
    falhas = [f"{nomes[i]}: {saidas[i] if i < len(saidas) else '(nada)'} != {esperado[i]}"
              for i in range(len(casos)) if i >= len(saidas) or saidas[i] != esperado[i]]
    verificar(not falhas, f"{len(casos)} vectores de BLAKE2s, HMAC, KDF, X25519, ChaCha20-Poly1305, HChaCha20, XChaCha20, "
                          f"base64 y prefijos" + (f"; fallan: {falhas[:4]}" if falhas else ""))


class ParC:
    """El motor de wireguard.c (wg_unidad par) como proceso del anfitrión."""

    def __init__(self, exe, priv, pub_ref, porta_ref, psk=None, corto=False):
        import queue
        self.q = queue.Queue()
        self.p = subprocess.Popen([exe, "par", R.b64(priv), R.b64(pub_ref), str(porta_ref), R.b64(psk) if psk else "-",
                                   "corto" if corto else "normal"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        threading.Thread(target=lambda: [self.q.put(l.strip()) for l in self.p.stdout], daemon=True).start()
        self.linhas = []
        self.esperar("PORTA")

    def ordem(self, t):
        self.p.stdin.write(t + "\n")
        self.p.stdin.flush()

    def esperar(self, pref, t=10):
        import queue
        fim = time.time() + t
        while time.time() < fim:
            try:
                l = self.q.get(timeout=0.2)
            except queue.Empty:
                continue
            self.linhas.append(l)
            if l.startswith(pref):
                return l
        return None

    def fechar(self):
        try:
            self.ordem("salir")
            self.p.wait(3)
        except Exception:
            self.p.kill()


def interop_anfitriao(exe):
    for psk, cookie, corto in ((None, False, False), (os.urandom(32), True, False), (None, False, True)):
        rotulo = ("con PSK y cookie" if cookie else "con tiempos cortos" if corto else "sin PSK")
        priv_c, priv_r = R.gerar_privada(), R.gerar_privada()
        ref = R.ParReferencia(priv_r, R.publica_de(priv_c), psk=psk)
        ref.exigir_cookie = cookie
        c = ParC(exe, priv_c, R.publica_de(priv_r), ref.porta, psk, corto)
        try:
            c.ordem("ping 1")
            verificar(c.esperar("ECO 1", 15), f"anfitrión {rotulo}: el motor C inicia el saludo y el ping vuelve por el túnel")
            if cookie:
                verificar(ref.cookies_enviadas >= 1 and ref.iniciacoes_com_mac2 >= 1,
                          "anfitrión: con la cookie del par, la iniciación siguiente lleva un mac2 que vale")
            d = ref.ping("10.66.0.2", seq=7)
            verificar(c.esperar("PING 7"), f"anfitrión {rotulo}: el par de referencia hace ping al motor C")
            time.sleep(0.3)
            n = len(ref.pacotes)
            ref.repetir(d)
            time.sleep(0.5)
            c.ordem("estado")
            e = c.esperar("ESTADO")
            verificar(e and "repeticion=1" in e and len(ref.pacotes) == n, f"anfitrión {rotulo}: el paquete repetido se descarta ({e})")
            ref.iniciar()
            time.sleep(1)
            ref.ping("10.66.0.2", seq=8)
            verificar(c.esperar("PING 8") and ref.atual and ref.atual.iniciador,
                      f"anfitrión {rotulo}: el motor C como respondedor (el par de referencia inicia)")
            if corto:
                c.ordem("saludar")   # el motor C vuelve a ser el iniciador: es el que renueva por tiempo
                time.sleep(1)
                antes = ref.saudacoes_respondidas
                for i in range(3, 10):
                    c.ordem(f"ping {i}")
                    verificar(c.esperar(f"ECO {i}", 10), f"anfitrión: ping {i} con renovación de claves")
                    time.sleep(1)
                verificar(ref.saudacoes_respondidas > antes, "anfitrión: el motor C renueva las claves (REKEY_AFTER_TIME acortado)")
        finally:
            c.fechar()
            ref.fechar()


# ------------------------------------------------------------ QEMU

class Http(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass


def conf_texto(priv_s, pub_r, porta, psk=None, ips="10.66.0.0/24", dns="10.66.0.1", extra=""):
    t = f"# prueba h365\n[Interface]\nPrivateKey = {R.b64(priv_s)}\nAddress = 10.66.0.2/24, fd00::2/64\n"
    if dns:
        t += f"DNS = {dns}, vpn.prueba\n"
    t += f"{extra}\n[Peer]\nPublicKey = {R.b64(pub_r)}\n"
    if psk:
        t += f"PresharedKey = {R.b64(psk)}\n"
    t += f"Endpoint = 10.0.2.2:{porta}\nAllowedIPs = {ips}\n"
    return t


def descartes(out):
    m = re.search(r"descartados: (\d+) repetidos, (\d+) con MAC", out)
    return (int(m.group(1)), int(m.group(2))) if m else (-1, -1)


def qemu():
    priv_s, priv_r, psk = R.gerar_privada(), R.gerar_privada(), os.urandom(32)
    pub_s, pub_r = R.publica_de(priv_s), R.publica_de(priv_r)
    ref = R.ParReferencia(priv_r, pub_s, psk=psk)
    www = os.path.join(TRAB, "www")
    os.makedirs(www, exist_ok=True)
    with open(os.path.join(www, "prueba.conf"), "w") as f:
        f.write(conf_texto(priv_s, pub_r, ref.porta, psk))
    with open(os.path.join(www, "mala.conf"), "w") as f:
        f.write("[Interface]\nPrivateKey = esto-no-es-una-clave\nAddress = 10.66.0.2/24\n")
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(Http, directory=www))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    hp = srv.server_address[1]
    try:
        with Maquina("h365-wireguard", "ide", imagem=imagem_console(), extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)
            time.sleep(3)
            m.comando("echo listo")

            # Claves en SaguiOS
            m.comando("wg genkey > /tmp/k.txt")
            out = m.comando("cat /tmp/k.txt")
            priv_g = re.search(r"([A-Za-z0-9+/]{43}=)", out)
            verificar(priv_g, "wg genkey escribe una clave en base64")
            out = m.comando("wg pubkey < /tmp/k.txt")
            pub_g = R.b64(R.publica_de(base64.b64decode(priv_g.group(1))))
            verificar(pub_g in out, "wg pubkey da la misma pública que X25519 de cryptography")
            out = m.comando("wg genkey | wg pubkey")
            verificar(re.search(r"\n[A-Za-z0-9+/]{43}=\n", out), "wg genkey | wg pubkey")

            for n in ("prueba", "mala"):
                m.comando(f"curl -o /tmp/{n}.conf http://10.0.2.2:{hp}/{n}.conf", tempo=30)
            out = m.comando("wg-quick check /tmp/mala.conf")
            verificar("PrivateKey no es una clave válida" in out, "wg-quick check rechaza una PrivateKey mala")
            out = m.comando("wg-quick check /tmp/prueba.conf")
            verificar("es correcto" in out and "IPv6 no está soportado" in out and "otros usuarios pueden leer" in out,
                      "wg-quick check: correcto, avisa de IPv6 (se ignora) y del archivo legible por otros")
            m.comando("chmod 600 /tmp/prueba.conf")
            out = m.comando("wg-quick up /tmp/prueba.conf --tiempos-prueba", tempo=30)
            verificar("prueba encendida" in out and "otros usuarios pueden leer" not in out, "wg-quick up (archivo 600: sin aviso)")
            out = m.comando("wg-quick up /tmp/prueba.conf", tempo=30)
            verificar("ya está encendida" in out, "un segundo wg-quick up: ya hay un túnel")

            out = m.comando("ping -c 3 10.66.0.1", tempo=40)
            verificar("3 paquetes enviados, 3 recibidos" in out, "ping -c 3 por el túnel (el saludo lo empieza el primer paquete)")
            verificar(ref.ecos >= 3 and ref.saudacoes_respondidas >= 1, f"el par de referencia contestó {ref.ecos} ecos")
            out = m.comando("wg show")
            verificar(R.b64(pub_s) in out and R.b64(pub_r) in out and "clave precompartida: (oculta)" in out and
                      re.search(r"último saludo: hace \d+", out) and "recibidos" in out and "10.66.0.0/24" in out,
                      "wg show: claves públicas, PSK oculta, último saludo, bytes e IP permitidas")
            verificar(R.b64(priv_s) not in m.serial(), "la clave privada no sale nunca por pantalla ni por el registro")
            out = m.comando("ifconfig")
            verificar("prueba: Túnel WireGuard" in out and "IPv4 10.66.0.2/24" in out and "DNS 10.66.0.1" in out,
                      "ifconfig enseña la interfaz del túnel")

            out = m.comando("echo hola eco | nc 10.66.0.1 7", tempo=40)
            verificar("HOLA ECO" in out, "TCP por el túnel (nc al eco del par de referencia)")
            verificar(ref.tcp_mss and ref.tcp_mss[-1] == 1380, f"el SYN por el túnel anuncia MSS 1380 (MTU 1420): {ref.tcp_mss}")
            out = m.comando("nslookup cualquiera.vpn", tempo=30)
            verificar("10.66.0.99" in out and "10.66.0.1" in out and ref.dns_perguntas >= 1, "el DNS del túnel (DNS = 10.66.0.1)")

            # Un paquete repetido se descarta
            n0 = len(ref.pacotes)
            d = ref.ping("10.66.0.2", seq=40)
            time.sleep(1.5)
            verificar(len(ref.pacotes) == n0 + 1, "SaguiOS contesta al ping del par de referencia por el túnel")
            ref.repetir(d)
            time.sleep(1.5)
            verificar(len(ref.pacotes) == n0 + 1, "el mismo paquete repetido no tiene respuesta")
            out = m.comando("wg show")
            verificar(descartes(out)[0] >= 1, f"wg show cuenta el repetido: {descartes(out)}")

            # Renovación de claves (tiempos de prueba: REKEY_AFTER_TIME 4 s)
            antes = ref.saudacoes_respondidas
            out = m.comando("ping -c 8 10.66.0.1", tempo=60)
            verificar("8 paquetes enviados, 8 recibidos" in out and ref.saudacoes_respondidas > antes,
                      f"renovación de claves sin perder pings ({antes} -> {ref.saudacoes_respondidas} saludos)")

            # Roaming del extremo: el par cambia de puerto
            nova = ref.mudar_porta()
            n0 = len(ref.pacotes)
            ref.ping("10.66.0.2", seq=41)
            time.sleep(1.5)
            out = m.comando("wg show")
            verificar(len(ref.pacotes) > n0 and f"10.0.2.2:{nova}" in out, f"roaming: SaguiOS sigue al par en el puerto {nova}")

            # SaguiOS como respondedor
            ref.iniciar()
            time.sleep(1.5)
            n0 = len(ref.pacotes)
            ref.ping("10.66.0.2", seq=42)
            time.sleep(1.5)
            verificar(ref.atual and ref.atual.iniciador and len(ref.pacotes) > n0, "SaguiOS como respondedor del saludo")

            # Basura en el puerto
            ep = ref.endpoint
            for i in range(300):
                tipo = random.choice((1, 2, 3, 4, 9))
                tam = random.choice((148, 92, 64, 32, 33, 1, 4, 200, 1500))
                lixo = bytes([tipo, 0, 0, 0]) + os.urandom(max(0, tam - 4))
                ref.sock.sendto(lixo[:tam], ep)
            time.sleep(1)
            out = m.comando("ping -c 2 10.66.0.1", tempo=30)
            verificar("2 recibidos" in out, "300 datagramas basura al puerto no rompen el túnel")
            out = m.comando("wg show")
            verificar(descartes(out)[1] >= 1, f"wg show cuenta los que no pasan el MAC: {descartes(out)}")

            out = m.comando("wg-quick down prueba", tempo=30)
            verificar("túnel apagado" in out, "wg-quick down")
            out = m.comando("wg show")
            verificar("No hay ningún túnel" in out, "wg show sin túnel")
            out = m.comando("ping -c 1 10.66.0.1", tempo=30)
            verificar("0 recibidos" in out, "sin túnel, 10.66.0.1 ya no responde")
            out = m.comando("ping -c 1 10.0.2.2", tempo=30)
            verificar("1 recibidos" in out, "la red física sigue como estaba")
            out = m.comando("nslookup cualquiera.vpn", tempo=30)
            verificar("10.66.0.99" not in out, "sin túnel, el DNS vuelve a ser el de la red física")

            # Las otras dos configuraciones, con el puerto del par después del roaming.
            with open(os.path.join(www, "tapa.conf"), "w") as f:
                f.write(conf_texto(priv_s, pub_r, ref.porta, psk, ips="10.66.0.0/24, 10.0.2.0/24, ::/0", dns=None))
            with open(os.path.join(www, "todo.conf"), "w") as f:
                f.write(conf_texto(priv_s, pub_r, ref.porta, psk, ips="0.0.0.0/0", dns=None, extra="MTU = 1400"))
            for n in ("tapa", "todo"):
                m.comando(f"curl -o /tmp/{n}.conf http://10.0.2.2:{hp}/{n}.conf", tempo=30)
            # Cookie: el par de referencia está «bajo carga»
            ref.exigir_cookie = True
            m.comando("chmod 600 /tmp/tapa.conf")
            out = m.comando("wg-quick up /tmp/tapa.conf", tempo=30)
            verificar("tapa encendida" in out, "wg-quick up con AllowedIPs que tapan la red local")
            # La cookie no se reenvía en el acto: la iniciación con mac2 sale a los REKEY_TIMEOUT (5 s).
            out = m.comando("ping -c 10 10.66.0.1", tempo=60)
            rec = re.search(r"(\d+) recibidos", out)
            verificar(rec and int(rec.group(1)) >= 3 and ref.cookies_enviadas >= 1 and ref.iniciacoes_com_mac2 >= 1,
                      "cookie: la primera iniciación recibe la cookie y la siguiente pasa con mac2")
            out = m.comando("wg show")
            verificar("cookies recibidas 1" in out or "cookies recibidas 2" in out, "wg show cuenta la cookie recibida")
            # 10.0.2.0/24 en AllowedIPs: el ping a 10.0.2.2 entra en el túnel, el demonio sigue por la física
            n0 = len(ref.pacotes)
            m.comando("ping -c 1 10.0.2.2", tempo=30)
            time.sleep(0.5)
            dst = [socket.inet_ntoa(p[16:20]) for _, _, p in ref.pacotes[n0:]]
            verificar("10.0.2.2" in dst, f"AllowedIPs 10.0.2.0/24 gana a la red local: el ping va por el túnel ({dst})")
            out = m.comando("ping -c 2 10.66.0.1", tempo=40)
            verificar("2 recibidos" in out, "el extremo (10.0.2.2) sigue alcanzándose: el socket del demonio sale por la física")
            ref.exigir_cookie = False
            m.comando("wg-quick down", tempo=30)

            # 0.0.0.0/0: todo por el túnel menos la red local
            m.comando("chmod 600 /tmp/todo.conf")
            out = m.comando("wg-quick up /tmp/todo.conf", tempo=30)
            verificar("todo encendida" in out and "MTU 1400" in out, "wg-quick up con AllowedIPs = 0.0.0.0/0 y MTU 1400")
            out = m.comando("ping -c 1 10.0.2.2", tempo=30)
            verificar("1 recibidos" in out, "con 0.0.0.0/0 la red local (10.0.2.0/24) sigue fuera del túnel")
            n0 = len(ref.pacotes)
            m.comando("ping -c 1 1.2.3.4", tempo=30)
            time.sleep(0.5)
            dst = [socket.inet_ntoa(p[16:20]) for _, _, p in ref.pacotes[n0:]]
            verificar("1.2.3.4" in dst, f"con 0.0.0.0/0 lo de fuera va por el túnel ({dst})")
            m.comando("echo x | nc 10.66.0.1 7", tempo=40)
            verificar(ref.tcp_mss[-1] == 1360, f"MSS con MTU 1400: {ref.tcp_mss[-1]}")
            m.comando("wg-quick down", tempo=30)
            s = m.serial()
            verificar("[TUNEL] todo cerrada" in s, "el kernel cierra la interfaz al apagar")
            verificar("Oops" not in s and "PANICO" not in s.upper() and "excecao" not in s.lower(), "sin fallos del kernel")
    finally:
        srv.shutdown()
        ref.fechar()


# ------------------------------------------------------------ escritorio (sesión sagui, sin root)

BORDA, TITULO = 2, 24
CLAVE = "clave1234"   # la contraseña de root que se elige en el primer diálogo (D372)


def esperar(m, patron, tempo=40, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.search(patron, m.serial()[desde:])
        if g:
            return g
        time.sleep(0.25)
    raise FalhaTeste(f"tiempo agotado esperando /{patron}/\n" + m.serial()[-2000:])


def ventana(m, titulo, desde=0):
    g = re.findall(r'janela (\d+) criada por \d+: "%s" (\d+)x(\d+) em \((\d+),(\d+)\)' % re.escape(titulo), m.serial()[desde:])
    verificar(g, f"ventana «{titulo}» creada")
    g = g[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]) + BORDA, y=int(g[4]) + TITULO)


def foco(m, id_):
    """Da el foco a una ventana por su botón de la barra de tareas (como h356)."""
    linha = re.findall(r"\[GUI\] barra \d+:[^\r\n]*", m.serial())[-1]
    x, l, e = re.findall(rf" {id_}@(\d+)\+(\d+)([fm]?)", linha)[0]
    if e != "f":
        m.clique(int(x) + int(l) // 2, 12)
        time.sleep(0.8)


def escritorio():
    """La ventana de la VPN y wg-quick como el usuario de la sesión local (sagui, sin root)."""
    from qemu import imagem_de_teste
    priv_r = R.gerar_privada()
    pub_r = R.publica_de(priv_r)
    ref = R.ParReferencia(priv_r, b"\0" * 32)
    www = os.path.join(TRAB, "www2")
    os.makedirs(www, exist_ok=True)
    with open(os.path.join(www, "casa.conf"), "w") as f:   # sin PrivateKey: la pone la ventana
        f.write(f"[Interface]\nAddress = 10.66.0.2/24\n\n[Peer]\nPublicKey = {R.b64(pub_r)}\n"
                f"Endpoint = 10.0.2.2:{ref.porta}\nAllowedIPs = 10.66.0.0/24\nPersistentKeepalive = 5\n")
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(Http, directory=www))
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    try:
        img = imagem_de_teste("", ["--vesa", "--sesion-sagui", "--dados-mb", "16"])
        with Maquina("h365-escritorio", "ide", imagem=img, extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)
            esperar(m, r"supervisor en marcha", 60)
            time.sleep(3)
            verificar("[PERMISOS] sesion como sagui" in m.serial(), "escritorio: la sesión es sagui (sin root)")
            term = int(re.findall(r'janela (\d+) criada por \d+: "Terminal"', m.serial())[-1])

            def cmd(orden, tempo=30):
                foco(m, term)
                return m.comando(orden, tempo)
            m.comando(f"curl -o /home/casa.conf http://10.0.2.2:{srv.server_address[1]}/casa.conf", tempo=30)
            n = len(m.serial())
            m.digitar("wg --ventana &", 0.04)
            m.tecla("ret")
            esperar(m, r"wg: ventana, 0 configuraciones", 30, n)
            v = ventana(m, "VPN (WireGuard)", n)
            time.sleep(1.5)
            m.captura("H365-1-ventana-vacia")
            m.clique(v["x"] + 87, v["y"] + v["a"] - 150 + 82 + 13)   # Generar claves
            esperar(m, r"wg: claves generadas", 15, n)
            time.sleep(1)
            m.captura("H365-2-claves-generadas")
            out = cmd("wg pubkey < /home/.config/wireguard/.clave")
            pub = re.search(r"\n([A-Za-z0-9+/]{43}=)\n", out)
            verificar(pub, "la ventana guarda la clave generada en /home/.config/wireguard/.clave")
            ref.outro = base64.b64decode(pub.group(1))
            out = cmd("ls -la /home/.config/wireguard")
            verificar(re.search(r"rw-------[^\n]*\.clave", out), f"la clave queda con permisos 600 ({out[-300:]!r})")
            foco(m, term)
            n = len(m.serial())
            m.digitar("wg --ventana --importar /home/casa.conf &", 0.04)
            m.tecla("ret")
            esperar(m, r"wg: importada casa\b", 30, n)
            v = ventana(m, "VPN (WireGuard)", n)
            out = cmd("ls -la /home/.config/wireguard")
            verificar(re.search(r"rw-------[^\n]*casa\.conf", out), "Importar copia el .conf con permisos 600")
            out = cmd("wg-quick check casa")
            verificar("es correcto" in out, "el .conf importado lleva la PrivateKey generada y es correcto")
            # D372: sin elevar, ningún programa de la sesión abre la interfaz del túnel.
            out = cmd("wg --demonio casa < /home/.config/wireguard/casa.conf")
            verificar("solo para el administrador" in out and "wg show" not in out,
                      f"como sagui sin elevar, abrir SOCK_TUN falla ({out[-200:]!r})")
            out = cmd("wg show")
            verificar("No hay ningún túnel" in out, "y no queda ningún túnel")
            foco(m, v["id"])
            n = len(m.serial())
            m.clique(v["x"] + 67, v["y"] + v["a"] - 150 + 13)   # Conectar (la seleccionada: casa)
            # El diálogo de administrador (D269); root aún no tiene contraseña: se elige dos veces.
            esperar(m, r"dialogo de permisos: wg \(pid \d+\), primera contraseña", 30, n)
            time.sleep(1)
            m.captura("H365-5-pide-permiso")
            m.digitar(CLAVE, 0.05)
            m.tecla("tab")
            m.digitar(CLAVE, 0.05)
            m.tecla("ret")
            esperar(m, r"wg: conectar -> 0", 40, n)
            verificar(re.search(r"ELEVACION concedida: wg \(pid \d+\) pasa de uid 1000", m.serial()[n:]),
                      "Conectar pide permiso con el diálogo de administrador y se concede")
            # El demonio es root: la sesión (sagui) no lo puede matar (SIS_MATAR solo a los suyos).
            out = cmd("wg show")
            pid = re.search(r"demonio: proceso (\d+)", out)
            verificar(pid, "wg show da el proceso del demonio")
            n2 = len(m.serial())
            out = cmd(f"kill {pid.group(1)}")
            verificar(f"kill: {pid.group(1)}:" in out or re.search(r"denegado a kill \(pid \d+, uid 1000\)", m.serial()[n2:]),
                      f"el demonio corre como root: sagui no lo puede matar ({out[-200:]!r})")
            out = cmd("wg show")
            verificar("demonio: proceso " + pid.group(1) in out, "y el túnel sigue encendido")
            fim = time.time() + 20
            while time.time() < fim and not ref.saudacoes_respondidas:
                time.sleep(0.5)
            verificar(ref.saudacoes_respondidas >= 1, "la sesión local enciende la VPN desde la ventana y hay saludo (keepalive persistente)")
            time.sleep(3)
            m.captura("H365-3-vpn-encendida")
            out = cmd("ping -c 2 10.66.0.1", tempo=30)
            verificar("2 recibidos" in out, "ping por la VPN encendida desde la ventana (como sagui)")
            esperar(m, r"\[GUI\] bandeja [^\n]* vpn@\d+", 20, n)
            verificar(True, "el candado de la VPN aparece en la bandeja")
            foco(m, v["id"])
            n = len(m.serial())
            m.clique(v["x"] + 183, v["y"] + v["a"] - 150 + 13)   # Desconectar
            esperar(m, r"wg: desconectar -> 0", 30, n)
            out = cmd("wg show")
            verificar("No hay ningún túnel" in out, "Desconectar apaga el túnel")
            verificar("dialogo de permisos" not in m.serial()[n:], "Desconectar no pide permiso (la sesión local apaga)")
            m.captura("H365-4-vpn-apagada")
            # El permiso no se recuerda (PED_SIN_RECORDAR): wg-quick up vuelve a preguntar. Esc = No.
            foco(m, term)
            n = len(m.serial())
            m.digitar("wg-quick up casa", 0.04)
            m.tecla("ret")
            esperar(m, r"dialogo de permisos: wg \(pid \d+\)", 30, n)
            verificar("ELEVACION recordada" not in m.serial(), "encender la VPN no usa la autorización recordada")
            time.sleep(1)
            m.tecla("esc")
            esperar(m, r"no se encendió: hace falta permiso de administrador", 30, n)
            out = cmd("wg show")
            verificar("No hay ningún túnel" in out, "con «No» en el diálogo la VPN no se enciende")
            # Otra vez, con la contraseña: se enciende desde el Terminal.
            foco(m, term)
            n = len(m.serial())
            m.digitar("wg-quick up casa", 0.04)
            m.tecla("ret")
            esperar(m, r"dialogo de permisos: wg \(pid \d+\)", 30, n)
            time.sleep(1)
            m.digitar(CLAVE, 0.05)
            m.tecla("ret")
            esperar(m, r"casa encendida", 40, n)
            out = cmd("ping -c 2 10.66.0.1", tempo=30)
            verificar("2 recibidos" in out, "wg-quick up desde el Terminal con el permiso: ping por la VPN")
            out = cmd("id")
            verificar("uid=1000" in out, "el Terminal sigue siendo sagui (solo el demonio es root)")
            out = cmd("wg-quick down")
            verificar("túnel apagado" in out, "wg-quick down como sagui (sin elevar)")
    finally:
        srv.shutdown()
        ref.fechar()


def fontes():
    """Lo que no se ve en QEMU: los permisos y la ventana."""
    with open(os.path.join(RAIZ, "kernel", "nucleo", "permisos.c"), encoding="utf-8") as f:
        p = f.read()
    verificar("return a != SOCK_TUN;" in p and "a == REDE_TUNEL_DESLIGAR && !eu->de_servico" in p,
              "D372: encender la VPN (SOCK_TUN) solo root (ni la sesión local, ni servicios, ni SSH sin root); "
              "apagar, también la sesión local")
    with open(os.path.join(RAIZ, "kernel", "rede", "socket.c"), encoding="utf-8") as f:
        s = f.read()
    verificar(s.count("if (processo_recursos()->uid != 0) return ERRO_ACESSO;") >= 2,
              "D372: OPC_FISICA y OPC_TUN_CONFIG (rutas, DNS) solo root")
    with open(os.path.join(RAIZ, "kernel", "nucleo", "supervisor.c"), encoding="utf-8") as f:
        sv = f.read()
    verificar("!sem_lembrar && lembrada(eu)" in sv and "p->graf && !p->sem_lembrar" in sv,
              "D372: PED_SIN_RECORDAR ni usa ni guarda la autorización recordada")
    with open(os.path.join(RAIZ, "userland", "programas", "wg.c"), encoding="utf-8") as f:
        wgc = f.read()
    dem = wgc[wgc.index("static int demonio("):wgc.index("/* ------------------------------------------------------------ wg show")]
    verificar("conf_ler_fd(ENTRADA" in dem and "abrir(" not in dem and "conf_ler(" not in dem,
              "D372: el demonio (root) lee el .conf por la entrada y no abre ninguna ruta")
    verificar("elevar_grafico_ex(motivo, PED_SIN_RECORDAR)" in wgc, "D372: wg-quick y la ventana piden permiso sin recordarlo")
    with open(os.path.join(RAIZ, "kernel", "rede", "rede.c"), encoding="utf-8") as f:
        r = f.read()
    # Revisión de seguridad: el demonio puede ser cualquier programa de la sesión local, así que
    # el kernel filtra lo que escribe (origen por las rutas del túnel, destino la dirección del
    # túnel, nada de DHCP) y el cable no llega a la dirección del túnel.
    verificar("destino != tunel_ip() || !pelo_tunel(origem)" in r and "be16(c + 2) == PORTA_DHCP_CLIENTE" in r
              and "de == DE_CABO && tunel_ativo() && destino == tunel_ip()" in r,
              "el kernel filtra lo que entra por el túnel (origen en sus rutas, sin DHCP) y por el cable hacia el túnel")
    with open(os.path.join(RAIZ, "userland", "programas", "wg_ventana.h"), encoding="utf-8") as f:
        v = f.read()
    verificar("mascara_criacao(077)" in v, "la ventana crea las claves y los .conf ya con 0600 (sin carrera)")
    with open(os.path.join(RAIZ, "userland", "libtls", "wireguard.c"), encoding="utf-8") as f:
        w = f.read()
    verificar("memcmp(" not in w.replace("memcmp(ts, p->ultimo_tai64n, 12)", ""),
              "wireguard.c: las comparaciones de claves y MAC son de tiempo constante (cripto_igual)")


def main():
    os.makedirs(TRAB, exist_ok=True)
    exe, r = compilar("wg_unidad", ["-O2"])
    verificar(r.returncode == 0 and "warning" not in r.stderr, "wg_unidad.c y la libtls compilan sin avisos en el anfitrión" + r.stderr[:1500])
    vetores(exe)
    exe2, r = compilar("wg_unidad-asan", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"])
    if r.returncode != 0:
        print("  AVISO: sin los sanitizadores de gcc en este anfitrión; se omiten")
    else:
        vetores(exe2)
        interop_anfitriao(exe2)
    interop_anfitriao(exe)
    fontes()
    if os.environ.get("H365_SOLO_ESCRITORIO"):
        escritorio()
        return
    if os.environ.get("H365_SIN_QEMU"):
        print("  (H365_SIN_QEMU: se omite la parte de QEMU)")
        return
    qemu()
    escritorio()


if __name__ == "__main__":
    try:
        main()
        print("h365: PASSOU")
    except FalhaTeste as e:
        print(f"h365: FALHOU: {e}")
        sys.exit(1)
