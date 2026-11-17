# SPDX-License-Identifier: GPL-3.0-only
"""Un par WireGuard de referencia en Python, para las pruebas (D363).

Escrito para tests/h365_wireguard.py a partir del «whitepaper» de WireGuard, con
primitivas que no son las de SaguiOS: hashlib.blake2s y hmac de la biblioteca
estándar, X25519 y ChaCha20-Poly1305 de la librería `cryptography`, y un
HChaCha20 aquí mismo (para XChaCha20-Poly1305 de las cookies). Así, si el
WireGuard de SaguiOS y este se entienden, los dos siguen la especificación (y
no los mismos errores).

No necesita root: es un socket UDP normal. Dentro del túnel hace de «servidor»:
contesta al ping (ICMP eco), tiene un TCP mínimo de eco en el puerto 7 (pone el
texto en mayúsculas) y guarda lo que recibe para que la prueba lo mire.
"""
import hashlib
import hmac
import os
import socket
import struct
import threading
import time

from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey, X25519PublicKey
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305

CONSTRUCTION = b"Noise_IKpsk2_25519_ChaChaPoly_BLAKE2s"
IDENTIFIER = b"WireGuard v1 zx2c4 Jason@zx2c4.com"
LABEL_MAC1 = b"mac1----"
LABEL_COOKIE = b"cookie--"


# ------------------------------------------------------------ primitivas

def HASH(*partes):
    return hashlib.blake2s(b"".join(partes)).digest()


def MAC(chave, dados):
    return hashlib.blake2s(dados, key=chave, digest_size=16).digest()


def HMAC(chave, dados):
    return hmac.new(chave, dados, hashlib.blake2s).digest()


def KDF(n, chave, entrada):
    t0 = HMAC(chave, entrada)
    saida, t = [], b""
    for i in range(1, n + 1):
        t = HMAC(t0, t + bytes([i]))
        saida.append(t)
    return saida


def DH(privada, publica):
    return X25519PrivateKey.from_private_bytes(privada).exchange(X25519PublicKey.from_public_bytes(publica))


def publica_de(privada):
    from cryptography.hazmat.primitives import serialization
    return X25519PrivateKey.from_private_bytes(privada).public_key().public_bytes(
        serialization.Encoding.Raw, serialization.PublicFormat.Raw)


def gerar_privada():
    k = bytearray(os.urandom(32))
    k[0] &= 248
    k[31] &= 127
    k[31] |= 64
    return bytes(k)


def AEAD(chave, contador, texto, aad):
    return ChaCha20Poly1305(chave).encrypt(b"\0" * 4 + struct.pack("<Q", contador), texto, aad)


def AEAD_abrir(chave, contador, cifrado, aad):
    return ChaCha20Poly1305(chave).decrypt(b"\0" * 4 + struct.pack("<Q", contador), cifrado, aad)


def _rotl(v, n):
    return ((v << n) | (v >> (32 - n))) & 0xFFFFFFFF


def hchacha20(chave, nonce16):
    x = [0x61707865, 0x3320646e, 0x79622d32, 0x6b206574] + list(struct.unpack("<8I", chave)) + \
        list(struct.unpack("<4I", nonce16))

    def qr(a, b, c, d):
        x[a] = (x[a] + x[b]) & 0xFFFFFFFF; x[d] = _rotl(x[d] ^ x[a], 16)
        x[c] = (x[c] + x[d]) & 0xFFFFFFFF; x[b] = _rotl(x[b] ^ x[c], 12)
        x[a] = (x[a] + x[b]) & 0xFFFFFFFF; x[d] = _rotl(x[d] ^ x[a], 8)
        x[c] = (x[c] + x[d]) & 0xFFFFFFFF; x[b] = _rotl(x[b] ^ x[c], 7)
    for _ in range(10):
        qr(0, 4, 8, 12); qr(1, 5, 9, 13); qr(2, 6, 10, 14); qr(3, 7, 11, 15)
        qr(0, 5, 10, 15); qr(1, 6, 11, 12); qr(2, 7, 8, 13); qr(3, 4, 9, 14)
    return struct.pack("<8I", *(x[0:4] + x[12:16]))


def XAEAD(chave, nonce24, texto, aad):
    return ChaCha20Poly1305(hchacha20(chave, nonce24[:16])).encrypt(b"\0" * 4 + nonce24[16:], texto, aad)


def XAEAD_abrir(chave, nonce24, cifrado, aad):
    return ChaCha20Poly1305(hchacha20(chave, nonce24[:16])).decrypt(b"\0" * 4 + nonce24[16:], cifrado, aad)


def tai64n():
    t = time.time()
    return struct.pack(">QI", 0x400000000000000A + int(t), int((t % 1) * 1e9))


def b64(k):
    import base64
    return base64.b64encode(k).decode()


# ------------------------------------------------------------ IPv4 mínimo

def soma(d):
    if len(d) % 2:
        d += b"\0"
    s = sum(struct.unpack("!%dH" % (len(d) // 2), d))
    while s >> 16:
        s = (s & 0xFFFF) + (s >> 16)
    return (~s) & 0xFFFF


def ip_pacote(origem, destino, proto, carga, ident=0):
    cab = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + len(carga), ident, 0x4000, 64, proto, 0,
                      socket.inet_aton(origem), socket.inet_aton(destino))
    cab = cab[:10] + struct.pack("!H", soma(cab)) + cab[12:]
    return cab + carga


def tcp_segmento(origem, destino, po, pd, seq, ack, flags, dados=b"", janela=65535, mss=None):
    opc = struct.pack("!BBH", 2, 4, mss) if mss else b""
    cab = struct.pack("!HHIIBBHHH", po, pd, seq, ack, (5 + len(opc) // 4) << 4, flags, janela, 0, 0) + opc
    pseudo = socket.inet_aton(origem) + socket.inet_aton(destino) + struct.pack("!BBH", 0, 6, len(cab) + len(dados))
    s = soma(pseudo + cab + dados)
    return cab[:16] + struct.pack("!H", s) + cab[18:] + dados


# ------------------------------------------------------------ el par

class Sessao:
    def __init__(self, envio, recepcao, local, remoto, iniciador):
        self.envio, self.recepcao = envio, recepcao
        self.local, self.remoto = local, remoto
        self.iniciador = iniciador
        self.contador = 0
        self.vistos = set()
        self.criada = time.time()


class ParReferencia:
    """Un par WireGuard que escucha en 127.0.0.1:porta (o la que se le dé)."""

    def __init__(self, privada, publica_outro, psk=None, porta=0, meu_ip="10.66.0.1", endpoint=None):
        self.privada = privada
        self.publica = publica_de(privada)
        self.outro = publica_outro
        self.psk = psk or b"\0" * 32
        self.meu_ip = meu_ip
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(("127.0.0.1", porta))
        self.sock.settimeout(0.2)
        self.porta = self.sock.getsockname()[1]
        self.endpoint = endpoint
        self.trava = threading.RLock()
        self.sessoes = {}          # índice local -> Sessao
        self.atual = None
        self.hs = None             # (índice, efímera privada, C, H) del saludo que iniciamos
        self.ultimo_ts = b""
        self.exigir_cookie = False
        self.segredo = os.urandom(32)
        self.ultimo_mac1 = None
        self.cookie = None
        # Lo que la prueba mira
        self.saudacoes_respondidas = 0
        self.saudacoes_iniciadas = 0
        self.cookies_enviadas = 0
        self.iniciacoes_sem_mac2 = 0
        self.iniciacoes_com_mac2 = 0
        self.pacotes = []          # (sesión, contador, paquete IP) recibidos
        self.keepalives = 0
        self.dados_crus = []       # los datagramas de datos tal como llegaron (para repetirlos)
        self.ecos = 0
        self.tcp = {}              # (ip, puerto) -> estado del eco TCP
        self.tcp_mss = []
        self.tcp_texto = b""
        self.descartes_repeticao = 0
        self.dns_perguntas = 0
        self.parar = threading.Event()
        self.fio = threading.Thread(target=self._laco, daemon=True)
        self.fio.start()

    def mudar_porta(self):
        """Roaming: el par pasa a otro puerto (como si cambiara de red); el otro lo sigue."""
        novo = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        novo.bind(("127.0.0.1", 0))
        novo.settimeout(0.2)
        with self.trava:
            velho, self.sock = self.sock, novo
            self.porta = novo.getsockname()[1]
        time.sleep(0.3)
        velho.close()
        return self.porta

    def fechar(self):
        self.parar.set()
        self.fio.join(2)
        self.sock.close()

    # ---- mandar
    def _mandar(self, dados, para=None):
        para = para or self.endpoint
        if para:
            self.sock.sendto(dados, para)

    def _macs(self, msg, chave_pub_destino):
        mac1 = MAC(HASH(LABEL_MAC1, chave_pub_destino), msg)
        self.ultimo_mac1 = mac1
        mac2 = MAC(self.cookie, msg + mac1) if self.cookie else b"\0" * 16
        return msg + mac1 + mac2

    def iniciar(self):
        """Manda una iniciación (este lado inicia el saludo)."""
        with self.trava:
            idx = struct.unpack("<I", os.urandom(4))[0]
            e = gerar_privada()
            epub = publica_de(e)
            C = HASH(CONSTRUCTION)
            H = HASH(HASH(C, IDENTIFIER), self.outro)
            C, = KDF(1, C, epub)
            H = HASH(H, epub)
            C, k = KDF(2, C, DH(e, self.outro))
            est = AEAD(k, 0, self.publica, H)
            H = HASH(H, est)
            C, k = KDF(2, C, DH(self.privada, self.outro))
            ts = AEAD(k, 0, tai64n(), H)
            H = HASH(H, ts)
            msg = struct.pack("<BxxxI", 1, idx) + epub + est + ts
            self.hs = (idx, e, C, H)
            self.saudacoes_iniciadas += 1
            self._mandar(self._macs(msg, self.outro))

    def enviar_ip(self, pacote, sessao=None):
        with self.trava:
            s = sessao or self.atual
            if not s:
                raise RuntimeError("sin sesión")
            n = len(pacote)
            cheio = (n + 15) // 16 * 16 if n else 0
            texto = pacote + b"\0" * (cheio - n)
            cab = struct.pack("<BxxxIQ", 4, s.remoto, s.contador)
            dado = cab + AEAD(s.envio, s.contador, texto, b"")
            s.contador += 1
            self._mandar(dado)
            return dado

    def repetir(self, dado):
        self._mandar(dado)

    def ping(self, destino, seq=1, carga=b"sagui-wg"):
        icmp = struct.pack("!BBHHH", 8, 0, 0, 0x5347, seq) + carga
        icmp = icmp[:2] + struct.pack("!H", soma(icmp)) + icmp[4:]
        return self.enviar_ip(ip_pacote(self.meu_ip, destino, 1, icmp))

    # ---- recibir
    def _laco(self):
        while not self.parar.is_set():
            sock = self.sock
            try:
                d, de = sock.recvfrom(4096)
            except socket.timeout:
                continue
            except OSError:
                if self.parar.is_set():
                    return
                continue
            try:
                with self.trava:
                    self._chegou(d, de)
            except Exception as ex:   # un paquete malo no tumba al par de referencia
                print("  [ref] error:", ex)

    def _cookie_para(self, de):
        return MAC(self.segredo, socket.inet_aton(de[0]) + struct.pack("!H", de[1]))

    def _chegou(self, d, de):
        tipo = d[0]
        if tipo == 1 and len(d) == 148:
            if MAC(HASH(LABEL_MAC1, self.publica), d[:116]) != d[116:132]:
                return
            if self.exigir_cookie:
                if MAC(self._cookie_para(de), d[:132]) != d[132:148]:
                    self.iniciacoes_sem_mac2 += 1
                    nonce = os.urandom(24)
                    enc = XAEAD(HASH(LABEL_COOKIE, self.publica), nonce, self._cookie_para(de), d[116:132])
                    self.sock.sendto(struct.pack("<BxxxI", 3, struct.unpack("<I", d[4:8])[0]) + nonce + enc, de)
                    self.cookies_enviadas += 1
                    return
                self.iniciacoes_com_mac2 += 1
            self._responder(d, de)
        elif tipo == 2 and len(d) == 92:
            self._resposta(d, de)
        elif tipo == 3 and len(d) == 64:
            if self.hs and struct.unpack("<I", d[4:8])[0] == self.hs[0]:
                self.cookie = XAEAD_abrir(HASH(LABEL_COOKIE, self.outro), d[8:32], d[32:64], self.ultimo_mac1)
        elif tipo == 4 and len(d) >= 32:
            self._dados(d, de)

    def _responder(self, d, de):
        idx_i = struct.unpack("<I", d[4:8])[0]
        epub = d[8:40]
        C = HASH(CONSTRUCTION)
        H = HASH(HASH(C, IDENTIFIER), self.publica)
        C, = KDF(1, C, epub)
        H = HASH(H, epub)
        C, k = KDF(2, C, DH(self.privada, epub))
        spub = AEAD_abrir(k, 0, d[40:88], H)
        H = HASH(H, d[40:88])
        if spub != self.outro:
            return
        C, k = KDF(2, C, DH(self.privada, spub))
        ts = AEAD_abrir(k, 0, d[88:116], H)
        H = HASH(H, d[88:116])
        if ts <= self.ultimo_ts:
            self.descartes_repeticao += 1
            return
        self.ultimo_ts = ts
        idx = struct.unpack("<I", os.urandom(4))[0]
        e = gerar_privada()
        epub_r = publica_de(e)
        C, = KDF(1, C, epub_r)
        H = HASH(H, epub_r)
        C, = KDF(1, C, DH(e, epub))
        C, = KDF(1, C, DH(e, spub))
        C, tau, k = KDF(3, C, self.psk)
        H = HASH(H, tau)
        vazio = AEAD(k, 0, b"", H)
        H = HASH(H, vazio)
        msg = struct.pack("<BxxxII", 2, idx, idx_i) + epub_r + vazio
        recv, envio = KDF(2, C, b"")
        s = Sessao(envio, recv, idx, idx_i, False)
        self.sessoes[idx] = s
        self.endpoint = de
        self.saudacoes_respondidas += 1
        self._mandar(self._macs(msg, self.outro))
        self.proxima = s

    def _resposta(self, d, de):
        if not self.hs:
            return
        idx_r, idx_i = struct.unpack("<II", d[4:12])
        idx, e, C, H = self.hs
        if idx_i != idx or MAC(HASH(LABEL_MAC1, self.publica), d[:60]) != d[60:76]:
            return
        epub = d[12:44]
        C, = KDF(1, C, epub)
        H = HASH(H, epub)
        C, = KDF(1, C, DH(e, epub))
        C, = KDF(1, C, DH(self.privada, epub))
        C, tau, k = KDF(3, C, self.psk)
        H = HASH(H, tau)
        AEAD_abrir(k, 0, d[44:60], H)
        envio, recv = KDF(2, C, b"")
        s = Sessao(envio, recv, idx, idx_r, True)
        self.sessoes[idx] = s
        self.atual = s
        self.hs = None
        self.endpoint = de
        self.enviar_ip(b"")   # keepalive: confirma la sesión

    def _dados(self, d, de):
        idx, contador = struct.unpack("<IQ", d[4:16])
        s = self.sessoes.get(idx)
        if not s:
            return
        if contador in s.vistos:
            self.descartes_repeticao += 1
            return
        texto = AEAD_abrir(s.recepcao, contador, d[16:], b"")
        s.vistos.add(contador)
        self.endpoint = de
        if not s.iniciador and self.atual is not s:
            self.atual = s    # confirmada por el primer dato
        self.dados_crus.append(d)
        if not texto:
            self.keepalives += 1
            return
        n = struct.unpack("!H", texto[2:4])[0]
        pacote = texto[:n]
        self.pacotes.append((s, contador, pacote))
        self._ip(pacote)

    # ---- la «máquina» dentro del túnel
    def _ip(self, p):
        origem, destino = socket.inet_ntoa(p[12:16]), socket.inet_ntoa(p[16:20])
        cab = (p[0] & 15) * 4
        proto, carga = p[9], p[cab:]
        if destino != self.meu_ip:
            return
        if proto == 1 and carga[0] == 8:   # eco ICMP
            r = b"\0\0\0\0" + carga[4:]
            r = r[:2] + struct.pack("!H", soma(r)) + r[4:]
            self.ecos += 1
            self.enviar_ip(ip_pacote(self.meu_ip, origem, 1, r))
        elif proto == 6:
            self._tcp(origem, carga)
        elif proto == 17 and len(carga) >= 20 and struct.unpack("!H", carga[2:4])[0] == 53:
            self._dns(origem, carga)

    def _dns(self, origem, u):
        """DNS del túnel: cualquier nombre A -> 10.66.0.99."""
        po = struct.unpack("!H", u[0:2])[0]
        q = u[8:]
        i = 12
        while i < len(q) and q[i]:
            i += 1 + q[i]
        pergunta = q[12:i + 5]
        r = q[:2] + b"\x81\x80" + struct.pack(">HHHH", 1, 1, 0, 0) + pergunta
        r += b"\xc0\x0c" + struct.pack(">HHIH", 1, 1, 60, 4) + bytes([10, 66, 0, 99])
        self.dns_perguntas += 1
        udp = struct.pack("!HHHH", 53, po, 8 + len(r), 0) + r
        self.enviar_ip(ip_pacote(self.meu_ip, origem, 17, udp))

    def _tcp(self, origem, t):
        po, pd, seq, ack, cabf, flags = struct.unpack("!HHIIBB", t[:14])
        cab = (cabf >> 4) * 4
        dados = t[cab:]
        if pd != 7:
            return
        chave = (origem, po)
        if flags & 0x02:   # SYN
            i = 20
            while i < cab:
                if t[i] == 0:
                    break
                if t[i] == 1:
                    i += 1
                    continue
                if t[i] == 2:
                    self.tcp_mss.append(struct.unpack("!H", t[i + 2:i + 4])[0])
                i += t[i + 1]
            est = {"meu": 1000, "dele": seq + 1}
            self.tcp[chave] = est
            self.enviar_ip(ip_pacote(self.meu_ip, origem, 6,
                                     tcp_segmento(self.meu_ip, origem, 7, po, est["meu"], est["dele"], 0x12, mss=1380)))
            est["meu"] += 1
            return
        est = self.tcp.get(chave)
        if not est:
            return
        if dados and seq == est["dele"]:
            est["dele"] += len(dados)
            self.tcp_texto += dados
            r = dados.upper()
            self.enviar_ip(ip_pacote(self.meu_ip, origem, 6,
                                     tcp_segmento(self.meu_ip, origem, 7, po, est["meu"], est["dele"], 0x18, r)))
            est["meu"] += len(r)
        if flags & 0x01:   # FIN
            est["dele"] += 1
            self.enviar_ip(ip_pacote(self.meu_ip, origem, 6,
                                     tcp_segmento(self.meu_ip, origem, 7, po, est["meu"], est["dele"], 0x11)))
            est["meu"] += 1
