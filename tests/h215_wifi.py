#!/usr/bin/env python3
"""H215 (D215) - Wi-Fi con el pincho Realtek RTL8188EU (0bda:8179).

  1. anfitrion: kernel/lib/wpa.c (compilado con -DSAGUI_HOST y sanitizadores)
     contra los vectores de las normas y contra implementaciones independientes
     en Python:
       * SHA-1 (FIPS 180-4), HMAC-SHA1 (RFC 2202), PBKDF2/PMK (IEEE 802.11i,
         anexo H: «password»/«IEEE» y «ThisIsAPassword»/«ThisIsASSID»),
         PRF-512 (802.11i, anexo H), AES-128 (FIPS 197, C.1) y la envoltura de
         claves (RFC 3394, 4.1);
       * el analizador de balizas: SSID, canal, RSN con PSK/CCMP, WPA1, WEP,
         abierta, RSN cortado y elementos que se salen del final;
       * el saludo de cuatro pasos contra un punto de acceso simulado aquí
         (hashlib/hmac y, para envolver la GTK, el módulo cryptography si está):
         mensajes 2 y 4 con MIC correcto, la PTK y la GTK que salen, el
         mensaje 3 con MIC falso, repetido o con otro RSN (rechazados) y la
         renovación de la GTK;
  2. fuentes: los enganches (usb.c, rede.c, el firmware en el ramdisk, SIS_WIFI,
     terceros.md, SEM_FIRMWARE);
  3. QEMU sin pincho: arranca igual, «wifi» dice que no hay adaptador e
     «ifconfig wlan0» que no hay wlan0;
  4. QEMU con el pincho real (pasado por usbipd; si no hay 0bda:8179 en
     /sys/bus/usb/devices se SALTA con un aviso): firmware cargado, MAC de la
     eFuse, «wifi buscar» con al menos una red, el estado del enlace e
     «ifconfig wlan0». Los SSID y las MAC ajenas no se imprimen (se ofuscan).
     No se conecta a ninguna red.

  5. `make SEM_FIRMWARE=1` (en su carpeta): sin /lib/firmware y, con el pincho,
     el driver lo dice («esta compilación no lleva firmware») y `wifi` da la MAC.

  6. el pincho por el xHCI (qemu-xhci), también con el pincho real.

  D238 (rendimiento): en «anfitrion», kernel/lib/wifi_n.c con los sanitizadores
  (tests/recursos/wifi_n_anfitriao.c: el desglose de la recepción agregada, los
  elementos HT y WMM, la respuesta a un ADDBA y la escalera de tasas); en
  «fuentes», las colas USB, la HT y los contadores; con el pincho real, la cola
  de recepción armada y los contadores de «wifi». QEMU no emula el pincho: sin
  él, el rendimiento (una descarga grande antes y después) lo mide el
  mantenedor con su router (docs/decisoes.md, D238).

Uso: h215_wifi.py [anfitrion] [fuentes] [sin] [pincho] [pincho-xhci] [sem-firmware]
"""
import glob
import hashlib
import hmac
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import RAIZ, SAIDA, Maquina, FalhaTeste, imagem_console, verificar, criar_pendrive, pendrive_qemu  # noqa: E402

# ------------------------------------------------------------ anfitrión


class Wpa:
    """El envoltorio tests/recursos/wpa_anfitriao.c, de orden en orden."""

    def __init__(self):
        os.makedirs(SAIDA, exist_ok=True)
        exe = os.path.join(SAIDA, "wpa_anfitriao")
        subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                        "-fno-sanitize-recover=all", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "kernel", "inc"),
                        "-o", exe, os.path.join(RAIZ, "tests", "recursos", "wpa_anfitriao.c")] +
                       [os.path.join(RAIZ, "kernel", "lib", f + ".c") for f in ("wpa", "wpa_cifra", "p256", "sae", "sha256")],
                       check=True)
        self.p = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    def ordem(self, linha, nlinhas=1):
        self.p.stdin.write(linha + "\n")
        self.p.stdin.flush()
        r = {}
        for _ in range(nlinhas):
            l = self.p.stdout.readline().strip()
            k, _, v = l.partition(" ")
            r[k] = v
        return r

    def fechar(self):
        self.p.stdin.close()
        verificar(self.p.wait() == 0, "el envoltorio terminó sin errores de los sanitizadores")


def prf(k, a, b, n):
    out = b""
    i = 0
    while len(out) < n:
        out += hmac.new(k, a.encode() + b"\0" + b + bytes([i]), hashlib.sha1).digest()
        i += 1
    return out[:n]


def eapol(info, replay, nonce=bytes(32), dados=b"", rsc=bytes(8), kck=None, versao=2, mic=None):
    corpo = bytes([2]) + info.to_bytes(2, "big") + (16).to_bytes(2, "big") + replay + nonce + bytes(16) + rsc + \
        bytes(8) + bytes(16) + len(dados).to_bytes(2, "big") + dados
    q = bytes([versao, 3]) + len(corpo).to_bytes(2, "big") + corpo
    if kck is not None:
        m = mic if mic is not None else hmac.new(kck, q, hashlib.sha1).digest()[:16]
        q = q[:81] + m + q[97:]
    return q


def mic_ok(kck, q):
    z = q[:81] + bytes(16) + q[97:]
    return hmac.new(kck, z, hashlib.sha1).digest()[:16] == q[81:97]


def envolver(kek, dados):
    try:
        from cryptography.hazmat.primitives.keywrap import aes_key_wrap
    except ImportError:
        return None
    return aes_key_wrap(kek, dados)


def parte_anfitriao():
    w = Wpa()
    # Vectores de las normas
    verificar(w.ordem("sha1 616263")["sha1"] == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1('abc') (FIPS 180-4)")
    verificar(w.ordem("sha1 -")["sha1"] == "da39a3ee5e6b4b0d3255bfef95601890afd80709", "SHA-1 vacío")
    verificar(w.ordem("hmac " + "0b" * 20 + " 4869205468657265")["hmac"] == "b617318655057264e28bc0b6fb378c8ef146be00",
              "HMAC-SHA1, RFC 2202 caso 1")
    verificar(w.ordem("pmk password IEEE")["pmk"] == "f42c6fc52df0ebef9ebb4b90b38a5f902e83fe1b135a70e23aed762e9710a12e",
              "PMK «password»/«IEEE» (802.11i, anexo H)")
    verificar(w.ordem("pmk ThisIsAPassword ThisIsASSID")["pmk"] ==
              "0dc0d6eb90555ed6419756b9a15ec3e3209b63df707dd508d14581f8982721af", "PMK «ThisIsAPassword»/«ThisIsASSID»")
    verificar(w.ordem("pmk corta SSID")["pmk"] == "recusada", "contraseña de menos de 8 caracteres: rechazada")
    largo = "a" * 63
    esperado = hashlib.pbkdf2_hmac("sha1", largo.encode(), b"Z" * 32, 4096, 32).hex()
    verificar(w.ordem(f"pmk {largo} {'Z' * 32}")["pmk"] == esperado, "PMK con 63 caracteres y SSID de 32 (contra hashlib)")
    verificar(w.ordem("prf " + "0b" * 20 + " prefix 4869205468657265 64")["prf"] ==
              "bcd4c650b30b9684951829e0d75f9d54b862175ed9f00606e17d8da35402ffee"
              "75df78c3d31e0f889f012120c0862beb67753e7439ae242edb8373698356cf5a", "PRF-512 (802.11i, anexo H)")
    k, b = os.urandom(32), os.urandom(76)
    verificar(w.ordem(f"prf {k.hex()} Pairwise_key_expansion {b.hex()} 48")["prf"] ==
              prf(k, "Pairwise key expansion", b, 48).hex(), "PRF-384 contra Python")
    r = w.ordem("aes 000102030405060708090a0b0c0d0e0f 00112233445566778899aabbccddeeff", 2)
    verificar(r["aes"] == "69c4e0d86a7b0430d8cdb78070b4c55a" and r["inv"] == "00112233445566778899aabbccddeeff",
              "AES-128 (FIPS 197, C.1) y su inversa")
    verificar(w.ordem("envolver 000102030405060708090A0B0C0D0E0F 00112233445566778899AABBCCDDEEFF")["envolto"] ==
              "1fa68b0a8112b447aef34bd8fb5a7b829d3e862371d2cfe5", "envoltura AES (RFC 3394, 4.1)")
    verificar(w.ordem("desenvolver 000102030405060708090A0B0C0D0E0F 1FA68B0A8112B447AEF34BD8FB5A7B829D3E862371D2CFE5")
              ["claro"] == "00112233445566778899aabbccddeeff", "desenvoltura AES (RFC 3394)")
    verificar(w.ordem("desenvolver 000102030405060708090A0B0C0D0E0F 1FA68B0A8112B447AEF34BD8FB5A7B829D3E862371D2CFE6")
              ["claro"] == "falhou", "desenvoltura con la comprobación rota: rechazada")

    # Balizas
    rsn = bytes([48, 20, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 2, 0, 0])
    ies = bytes([0, 6]) + b"MiRed1" + bytes([1, 4, 0x82, 0x84, 0x8B, 0x96, 3, 1, 6]) + rsn + bytes([45, 2, 0, 0])
    o = w.ordem("ies 1 " + ies.hex())["ies"]
    verificar("ssid=MiRed1 canal=6 seg=3 grupo=4 par=16 psk=1 ht=1 ntasas=4 nrsn=22" in o, f"baliza WPA2-PSK/CCMP: {o}")
    mixto = bytes(rsn[:7]) + bytes([2]) + bytes(rsn[8:9]) + bytes([0]) + bytes(rsn[10:])
    o = w.ordem("ies 1 " + (bytes([0, 2]) + b"MX" + mixto).hex())["ies"]
    verificar("seg=3 grupo=2 par=16" in o, f"grupo TKIP (modo mixto): se ve el grupo 2: {o}")
    ent = bytes([48, 20]) + rsn[2:16] + bytes([1, 0, 0, 0x0F, 0xAC, 1, 0, 0])
    verificar("seg=4" in w.ordem("ies 1 " + ent.hex())["ies"], "RSN con 802.1X (Enterprise): no admitida")
    verificar("seg=2" in w.ordem("ies 1 " + bytes([221, 6, 0, 0x50, 0xF2, 1, 1, 0]).hex())["ies"], "solo WPA1")
    verificar("seg=1" in w.ordem("ies 1 " + bytes([0, 1, 65]).hex())["ies"], "bit de privacidad sin RSN: WEP")
    verificar("seg=0" in w.ordem("ies 0 " + bytes([0, 1, 65]).hex())["ies"], "abierta")
    verificar("ssid= canal=0 seg=0" in w.ordem("ies 0 " + bytes([0, 40]).hex() + "41" * 5)["ies"],
              "elemento que se sale del final: ignorado")
    verificar("seg=4" in w.ordem("ies 1 " + bytes([48, 2, 1, 0]).hex())["ies"],
              "RSN cortado tras la versión: sin AKM vale 802.1X (no admitida)")

    # Saludo de cuatro pasos contra un punto de acceso simulado
    pmk = hashlib.pbkdf2_hmac("sha1", b"clave de prueba", b"RedPrueba", 4096, 32)
    aa, spa = bytes.fromhex("020000aabbcc"), bytes.fromhex("e0e1a9000001")
    anonce, snonce = os.urandom(32), os.urandom(32)
    ap_rsn = rsn
    r = w.ordem(f"sup {pmk.hex()} {aa.hex()} {spa.hex()} {ap_rsn.hex()} {snonce.hex()}")
    nosso_ie = bytes.fromhex(r["ie"])
    verificar(nosso_ie == rsn, "nuestro RSN: CCMP/CCMP/PSK, versión 1")
    replay = (1).to_bytes(8, "big")
    m1 = eapol(0x008A, replay, anonce)
    r = w.ordem("eapol " + m1.hex(), 5)
    verificar(r["codigo"] == "1", "mensaje 1: el suplicante responde")
    m2 = bytes.fromhex(r["resp"])
    b = min(aa, spa) + max(aa, spa) + min(anonce, snonce) + max(anonce, snonce)
    ptk = prf(pmk, "Pairwise key expansion", b, 48)
    kck, kek, tk = ptk[:16], ptk[16:32], ptk[32:48]
    verificar(r["ptk"] == ptk.hex(), "PTK igual a la del punto de acceso")
    verificar(mic_ok(kck, m2), "mensaje 2: MIC correcto")
    verificar(m2[17:49] == snonce and m2[9:17] == replay, "mensaje 2: SNonce y contador")
    verificar(int.from_bytes(m2[5:7], "big") == 0x010A and m2[99:] == rsn, "mensaje 2: key info 0x010A y el RSN")
    gtk = os.urandom(16)
    kde = bytes([0xDD, 22, 0x00, 0x0F, 0xAC, 1, 1, 0]) + gtk
    claro = ap_rsn + kde
    claro += bytes([0xDD]) + bytes((8 - (len(claro) + 1) % 8) % 8) if len(claro) % 8 else b""
    dados = envolver(kek, claro)
    if dados is None:
        print("  AVISO: sin el módulo cryptography no se prueba el mensaje 3 (envolver la GTK)")
    else:
        replay3 = (2).to_bytes(8, "big")
        rsc = bytes([5, 0, 0, 0, 0, 0, 0, 0])
        m3 = eapol(0x13CA, replay3, anonce, dados, rsc, kck=kck)
        falso = eapol(0x13CA, replay3, anonce, dados, rsc, kck=kck, mic=bytes(16))
        verificar(w.ordem("eapol " + falso.hex(), 5)["codigo"] == "-1", "mensaje 3 con MIC falso: rechazado")
        outro_rsn = envolver(kek, rsn[:-2] + bytes([0x0C, 0]) + kde + bytes([0xDD, 0]))
        m3b = eapol(0x13CA, replay3, anonce, outro_rsn, rsc, kck=kck)
        verificar(w.ordem("eapol " + m3b.hex(), 5)["codigo"] == "-2", "mensaje 3 con otro RSN (degradación): rechazado")
        r = w.ordem("eapol " + m3.hex(), 5)
        verificar(r["codigo"] == "2", "mensaje 3: claves listas")
        m4 = bytes.fromhex(r["resp"])
        verificar(mic_ok(kck, m4) and m4[9:17] == replay3 and int.from_bytes(m4[5:7], "big") == 0x030A,
                  "mensaje 4: MIC, contador y key info 0x030A")
        verificar(r["gtk"] == gtk.hex() and r["gtkid"] == "1", "GTK desenvuelta e id 1")
        verificar(w.ordem("eapol " + m3.hex(), 5)["codigo"] == "-2", "mensaje 3 repetido: rechazado")
        gtk2 = os.urandom(16)
        kde2 = bytes([0xDD, 22, 0x00, 0x0F, 0xAC, 1, 2, 0]) + gtk2 + bytes([0xDD, 0, 0, 0, 0, 0, 0, 0])
        g1 = eapol(0x1382, (3).to_bytes(8, "big"), bytes(32), envolver(kek, kde2), kck=kck)
        r = w.ordem("eapol " + g1.hex(), 5)
        verificar(r["codigo"] == "3" and r["gtk"] == gtk2.hex() and r["gtkid"] == "2", "renovación de la GTK (grupo, mensaje 1)")
        g2 = bytes.fromhex(r["resp"])
        verificar(mic_ok(kck, g2) and int.from_bytes(g2[5:7], "big") == 0x0302, "grupo, mensaje 2: MIC y key info 0x0302")
    verificar(w.ordem("eapol 0203", 5)["codigo"] == "0", "trama corta: ignorada")
    parte_anfitriao_d220(w)
    w.fechar()
    parte_anfitriao_d238()


def parte_anfitriao_d238():
    """D238: kernel/lib/wifi_n.c en el anfitrión, con ASan y UBSan."""
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "wifi_n_anfitriao")
    subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "kernel", "inc"),
                    "-o", exe, os.path.join(RAIZ, "tests", "recursos", "wifi_n_anfitriao.c"),
                    os.path.join(RAIZ, "kernel", "lib", "wifi_n.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    for l in r.stdout.splitlines():
        if l.startswith("FALLO"):
            print("  " + l)
    verificar(r.returncode == 0 and "wifi_n ok" in r.stdout, "wifi_n.c (D238): " + (r.stdout + r.stderr)[-400:])


# ------------------------------------------------------------ anfitrión, D220 (v7.3.2)
#
# Implementaciones de referencia en Python (independientes de kernel/lib) para
# comprobar lo que no tiene vector oficial a mano y para hacer de punto de
# acceso: RC4, Michael, la mezcla de TKIP, P-256 con enteros y el SAE.

M32 = 0xFFFFFFFF


def rc4(k, d, pular=0):
    s = list(range(256))
    j = 0
    for i in range(256):
        j = (j + s[i] + k[i % len(k)]) & 255
        s[i], s[j] = s[j], s[i]
    i = j = 0
    out = bytearray()
    for n in range(pular + len(d)):
        i = (i + 1) & 255
        j = (j + s[i]) & 255
        s[i], s[j] = s[j], s[i]
        if n >= pular:
            out.append(d[n - pular] ^ s[(s[i] + s[j]) & 255])
    return bytes(out)


def _rotl(x, n):
    return ((x << n) | (x >> (32 - n))) & M32


def michael_py(k, m):
    l, r = int.from_bytes(k[:4], "little"), int.from_bytes(k[4:8], "little")
    m = m + b"\x5a" + bytes(4 + (-(len(m) + 5)) % 4)
    for i in range(0, len(m), 4):
        l ^= int.from_bytes(m[i:i + 4], "little")
        r ^= _rotl(l, 17); l = (l + r) & M32
        r ^= ((l & 0xff00ff00) >> 8) | ((l & 0x00ff00ff) << 8); l = (l + r) & M32
        r ^= _rotl(l, 3); l = (l + r) & M32
        r ^= _rotl(l, 30); l = (l + r) & M32
    return l.to_bytes(4, "little") + r.to_bytes(4, "little")


def _aes_sbox():
    s = [0] * 256
    for x in range(256):
        inv = 0
        if x:
            for y in range(1, 256):   # inverso en GF(2^8) por fuerza bruta
                a, b, p = x, y, 0
                while b:
                    if b & 1:
                        p ^= a
                    a = ((a << 1) ^ (0x1B if a & 0x80 else 0)) & 255
                    b >>= 1
                if p == 1:
                    inv = y
                    break
        v = inv
        for k in range(1, 5):
            v ^= ((inv << k) | (inv >> (8 - k))) & 255
        s[x] = v ^ 0x63
    return s


AES_S = _aes_sbox()


def _tkip_s(v):
    def t(x):
        a = AES_S[x]
        a2 = ((a << 1) ^ (0x1B if a & 0x80 else 0)) & 255
        return (a2 << 8) | (a2 ^ a)
    hi = t(v >> 8)
    return t(v & 255) ^ (((hi & 255) << 8) | (hi >> 8))


def tkip_mix_py(tk, ta, iv32, iv16):
    mk = lambda hi, lo: (hi << 8) | lo
    p = [iv32 & 0xFFFF, iv32 >> 16, mk(ta[1], ta[0]), mk(ta[3], ta[2]), mk(ta[5], ta[4])]
    for i in range(8):
        j = 2 * (i & 1)
        p[0] = (p[0] + _tkip_s(p[4] ^ mk(tk[1 + j], tk[0 + j]))) & 0xFFFF
        p[1] = (p[1] + _tkip_s(p[0] ^ mk(tk[5 + j], tk[4 + j]))) & 0xFFFF
        p[2] = (p[2] + _tkip_s(p[1] ^ mk(tk[9 + j], tk[8 + j]))) & 0xFFFF
        p[3] = (p[3] + _tkip_s(p[2] ^ mk(tk[13 + j], tk[12 + j]))) & 0xFFFF
        p[4] = (p[4] + _tkip_s(p[3] ^ mk(tk[1 + j], tk[0 + j])) + i) & 0xFFFF
    ttak = list(p)
    q = p + [(p[4] + iv16) & 0xFFFF]
    r1 = lambda v: ((v >> 1) | (v << 15)) & 0xFFFF
    for i in range(6):
        q[i] = (q[i] + _tkip_s(q[(i + 5) % 6] ^ mk(tk[2 * i + 1], tk[2 * i]))) & 0xFFFF
    q[0] = (q[0] + r1(q[5] ^ mk(tk[13], tk[12]))) & 0xFFFF
    q[1] = (q[1] + r1(q[0] ^ mk(tk[15], tk[14]))) & 0xFFFF
    for i in range(2, 6):
        q[i] = (q[i] + r1(q[i - 1])) & 0xFFFF
    k = bytes([iv16 >> 8, ((iv16 >> 8) | 0x20) & 0x7F, iv16 & 255, ((q[5] ^ mk(tk[1], tk[0])) >> 1) & 255])
    for v in q:
        k += bytes([v & 255, v >> 8])
    return ttak, k


def tkip_cifrar_py(q, cab, tk, mic_k, tsc, mic_ruim=False):
    """Una trama de datos TKIP hecha en Python (como la mandaría el punto de acceso)."""
    import zlib
    da, sa = (q[4:10], q[16:22]) if (q[1] & 3) == 2 else (q[16:22], q[10:16])
    dados = q[cab:]
    mic = michael_py(mic_k, da + sa + bytes(4) + dados)
    if mic_ruim:
        mic = bytes([mic[0] ^ 1]) + mic[1:]
    claro = dados + mic
    claro += zlib.crc32(claro).to_bytes(4, "little")
    _, k = tkip_mix_py(tk, q[10:16], tsc >> 16, tsc & 0xFFFF)
    iv = bytes([(tsc >> 8) & 255, (((tsc >> 8) & 255) | 0x20) & 0x7F, tsc & 255, 0x20]) + \
        (tsc >> 16).to_bytes(4, "little")
    return bytes([q[0], q[1] | 0x40]) + q[2:cab] + iv + rc4(k, claro)


# P-256 con enteros (FIPS 186-4, D.1.2.3)
EC_P = 0xffffffff00000001000000000000000000000000ffffffffffffffffffffffff
EC_N = 0xffffffff00000000ffffffffffffffffbce6faada7179e84f3b9cac2fc632551
EC_B = 0x5ac635d8aa3a93e7b3ebbd55769886bc651d06b0cc53b0f63bce3c3e27d2604b
EC_G = (0x6b17d1f2e12c4247f8bce6e563a440f277037d812deb33a0f4a13945d898c296,
        0x4fe342e2fe1a7f9b8ee7eb4a7c0f9e162bce33576b315ececbb6406837bf51f5)


def ec_add(a, b):
    if a is None:
        return b
    if b is None:
        return a
    if a[0] == b[0] and (a[1] + b[1]) % EC_P == 0:
        return None
    if a == b:
        l = (3 * a[0] * a[0] - 3) * pow(2 * a[1], -1, EC_P) % EC_P
    else:
        l = (b[1] - a[1]) * pow(b[0] - a[0], -1, EC_P) % EC_P
    x = (l * l - a[0] - b[0]) % EC_P
    return x, (l * (a[0] - x) - a[1]) % EC_P


def ec_mul(k, a):
    r = None
    while k:
        if k & 1:
            r = ec_add(r, a)
        a = ec_add(a, a)
        k >>= 1
    return r


def ec_bytes(a):
    return a[0].to_bytes(32, "big") + a[1].to_bytes(32, "big")


def ec_ponto(b):
    return int.from_bytes(b[:32], "big"), int.from_bytes(b[32:64], "big")


def kdf256(k, rotulo, ctx, n):
    out, i = b"", 1
    while len(out) < n:
        out += hmac.new(k, i.to_bytes(2, "little") + rotulo.encode() + ctx + (8 * n).to_bytes(2, "little"),
                        hashlib.sha256).digest()
        i += 1
    return out[:n]


def hkdf_ext(sal, ikm):
    return hmac.new(sal or bytes(32), ikm, hashlib.sha256).digest()


def hkdf_exp(prk, info, n):
    out, t, i = b"", b"", 1
    while len(out) < n:
        t = hmac.new(prk, t + info + bytes([i]), hashlib.sha256).digest()
        out += t
        i += 1
    return out[:n]


def sae_hp_py(senha, a, b):
    macs = max(a, b) + min(a, b)
    achado = None
    for c in range(1, 41):
        semente = hmac.new(macs, senha + bytes([c]), hashlib.sha256).digest()
        x = int.from_bytes(kdf256(semente, "SAE Hunting and Pecking", EC_P.to_bytes(32, "big"), 32), "big")
        if x >= EC_P or achado:
            continue
        v = (x ** 3 - 3 * x + EC_B) % EC_P
        if pow(v, (EC_P - 1) // 2, EC_P) == 1:
            y = pow(v, (EC_P + 1) // 4, EC_P)
            if (y & 1) != (semente[-1] & 1):
                y = EC_P - y
            achado = (x, y)
    return achado


def sswu_py(u):
    z, a = EC_P - 10, EC_P - 3
    t = (z * z * pow(u, 4, EC_P) + z * u * u) % EC_P
    t = pow(t, EC_P - 2, EC_P)
    x1 = EC_B * pow(z * a, -1, EC_P) % EC_P if t == 0 else (-EC_B * pow(a, -1, EC_P)) * (1 + t) % EC_P
    g = lambda x: (x ** 3 + a * x + EC_B) % EC_P
    x2 = z * u * u * x1 % EC_P
    x, v = (x1, g(x1)) if pow(g(x1), (EC_P - 1) // 2, EC_P) in (0, 1) else (x2, g(x2))
    y = pow(v, (EC_P + 1) // 4, EC_P)
    if (y & 1) != (u & 1):
        y = EC_P - y
    return x, y


def sae_pt_py(ssid, senha, ident=b""):
    s = hkdf_ext(ssid, senha + ident)
    u1 = int.from_bytes(hkdf_exp(s, b"SAE Hash to Element u1 P1", 48), "big") % EC_P
    u2 = int.from_bytes(hkdf_exp(s, b"SAE Hash to Element u2 P2", 48), "big") % EC_P
    return ec_add(sswu_py(u1), sswu_py(u2))


def sae_pwe_pt_py(pt, a, b):
    val = int.from_bytes(hkdf_ext(bytes(32), max(a, b) + min(a, b)), "big") % (EC_N - 1) + 1
    return ec_mul(val, pt)


class ApSae:
    """El lado del punto de acceso de un SAE (12.4.5), en Python."""

    def __init__(self, pwe, rand=None, mask=None):
        self.pwe = pwe
        self.rand = rand or int.from_bytes(os.urandom(32), "big") % (EC_N - 2) + 2
        self.mask = mask or int.from_bytes(os.urandom(32), "big") % (EC_N - 2) + 2
        self.escalar = (self.rand + self.mask) % EC_N
        m = ec_mul(self.mask, pwe)
        self.elemento = (m[0], (EC_P - m[1]) % EC_P)

    def commit(self):
        return self.escalar.to_bytes(32, "big") + ec_bytes(self.elemento)

    def processar(self, escalar, elemento):
        self.par = (escalar, elemento)
        k = ec_mul(self.rand, ec_add(ec_mul(escalar, self.pwe), elemento))
        semente = hkdf_ext(bytes(32), k[0].to_bytes(32, "big"))
        ctx = ((self.escalar + escalar) % EC_N).to_bytes(32, "big")
        kp = kdf256(semente, "SAE KCK and PMK", ctx, 64)
        self.kck, self.pmk, self.pmkid = kp[:32], kp[32:], ctx[:16]

    def confirm(self, sc, de_mim=True):
        e1, p1, e2, p2 = self.escalar, self.elemento, self.par[0], self.par[1]
        if not de_mim:
            e1, p1, e2, p2 = e2, p2, e1, p1
        return hmac.new(self.kck, sc.to_bytes(2, "little") + e1.to_bytes(32, "big") + ec_bytes(p1) +
                        e2.to_bytes(32, "big") + ec_bytes(p2), hashlib.sha256).digest()


def cmac(k, m):
    try:
        from cryptography.hazmat.primitives import cmac as _cmac
        from cryptography.hazmat.primitives.ciphers import algorithms
    except ImportError:
        return None
    c = _cmac.CMAC(algorithms.AES(k))
    c.update(m)
    return c.finalize()


def eapol_x(info, replay, nonce=bytes(32), dados=b"", rsc=bytes(8), kck=None, tipo=2, iv=bytes(16), tam=16,
            mic_ruim=False):
    """EAPOL-Key de cualquier versión: el MIC según los bits 0-2 de «info»."""
    corpo = bytes([tipo]) + info.to_bytes(2, "big") + tam.to_bytes(2, "big") + replay + nonce + iv + rsc + \
        bytes(8) + bytes(16) + len(dados).to_bytes(2, "big") + dados
    q = bytes([2, 3]) + len(corpo).to_bytes(2, "big") + corpo
    if kck is not None:
        m = mic_eapol(info & 7, kck, q)
        if mic_ruim:
            m = bytes(16)
        q = q[:81] + m + q[97:]
    return q


def mic_eapol(versao, kck, q):
    z = q[:81] + bytes(16) + q[97:]
    if versao == 1:
        return hmac.new(kck, z, hashlib.md5).digest()
    if versao == 2:
        return hmac.new(kck, z, hashlib.sha1).digest()[:16]
    return cmac(kck, z)


def mic_ok_x(versao, kck, q):
    return mic_eapol(versao, kck, q) == q[81:97]


def ptk_x(pmk, aa, spa, an, sn, n, sha256=False):
    b = min(aa, spa) + max(aa, spa) + min(an, sn) + max(an, sn)
    if sha256:
        return kdf256(pmk, "Pairwise key expansion", b, n)
    return prf(pmk, "Pairwise key expansion", b, n)


def kde_pad(claro):
    """El relleno de los datos de la clave antes de envolverlos (12.7.2)."""
    if len(claro) % 8 or len(claro) < 16:
        claro += bytes([0xDD]) + bytes((8 - (len(claro) + 1) % 8) % 8)
    return claro


def parte_anfitriao_d220(w):
    # --- vectores de las normas
    for msg, h in ((b"", "d41d8cd98f00b204e9800998ecf8427e"), (b"a", "0cc175b9c0f1b6a831c399e269772661"),
                   (b"abc", "900150983cd24fb0d6963f7d28e17f72"), (b"message digest", "f96b697d7cb7938d525a2f31aaf161d0"),
                   (b"abcdefghijklmnopqrstuvwxyz", "c3fcd3d76192e4007dfb496cca67e13b"),
                   (b"12345678901234567890123456789012345678901234567890123456789012345678901234567890",
                    "57edf4a22be3c955ac49da2e2107b67a")):
        verificar(w.ordem("md5 " + (msg.hex() or "-"))["md5"] == h, f"MD5 de {msg[:20]!r} (RFC 1321)")
    x = os.urandom(200)
    verificar(w.ordem("md5 " + x.hex())["md5"] == hashlib.md5(x).hexdigest(), "MD5 de 200 bytes contra hashlib")
    verificar(w.ordem("hmacmd5 " + "0b" * 16 + " " + b"Hi There".hex())["hmacmd5"] == "9294727a3638bb1c13f48ef8158bfc9d",
              "HMAC-MD5, RFC 2202 caso 1")
    verificar(w.ordem("hmacmd5 " + b"Jefe".hex() + " " + b"what do ya want for nothing?".hex())["hmacmd5"] ==
              "750c783e6ab0b503eaa86e310a5db738", "HMAC-MD5, RFC 2202 caso 2")
    verificar(w.ordem("hmacmd5 " + "aa" * 80 + " " + b"Test Using Larger Than Block-Size Key - Hash Key First".hex())
              ["hmacmd5"] == "6b1ab7fe4bd7bf8f0b62e6ce61b9d0cd", "HMAC-MD5, RFC 2202 caso 6 (clave larga)")
    r = w.ordem("rc4 0102030405 0 32")["rc4"]
    verificar(r == "b2396305f03dc027ccc3524a0a1118a86982944f18fc82d589c403a47a0d0919", "RC4 40 bits, desplazamiento 0 (RFC 6229)")
    r = w.ordem("rc4 0102030405060708090a0b0c0d0e0f10 0 32")["rc4"]
    verificar(r == "9ac7cc9a609d1ef7b2932899cde41b975248c4959014126a6e8a84f11d1a9e1c", "RC4 128 bits, desplazamiento 0 (RFC 6229)")
    k = os.urandom(13)
    verificar(w.ordem(f"rc4 {k.hex()} 1536 32")["rc4"] == rc4(k, bytes(32), 1536).hex(), "RC4 desplazado 1536 contra Python")
    verificar(w.ordem("crc32 " + b"123456789".hex())["crc32"] == "cbf43926", "CRC-32 de «123456789»")
    for clave, msg, mic in (("0000000000000000", b"", "82925c1ca1d130b8"), ("82925c1ca1d130b8", b"M", "434721ca40639b3f"),
                            ("434721ca40639b3f", b"Mi", "e8f9becae97e5d29"), ("e8f9becae97e5d29", b"Mic", "90038fc6cf13c1db"),
                            ("90038fc6cf13c1db", b"Mich", "d55e100510128986"),
                            ("d55e100510128986", b"Michael", "0a942b124ecaa546")):
        verificar(w.ordem(f"michael {clave} {msg.hex() or '-'}")["michael"] == mic, f"Michael «{msg.decode()}» (802.11, anexo M)")
    k, m = os.urandom(8), os.urandom(77)
    verificar(w.ordem(f"michael {k.hex()} {m.hex()}")["michael"] == michael_py(k, m).hex(), "Michael de 77 bytes contra Python")
    # Mezcla de clave de TKIP: los vectores del anexo M de la 802.11 (TTAK y la clave RC4)
    for tk, ta, iv32, iv16, ttak, rk in (
            ("000102030405060708090a0b0c0d0e0f", "102233445566", "00000000", "0000", "3dd2016e76f48697b2e8",
             "00200033ea8d2f60ca6d1374234a660b"),
            ("000102030405060708090a0b0c0d0e0f", "102233445566", "00000000", "0001", "3dd2016e76f48697b2e8",
             "00200190ffdc314389a9d9d074fd20aa"),
            ("63893b250840b8ae0bd0fa7e61d2783e", "64f2eaeddc25", "20dcfd43", "ffff", "7c6749d79724b5e9b4f1",
             "ff7fff93810fc6e58f5dd326251544ce"),
            ("63893b250840b8ae0bd0fa7e61d2783e", "64f2eaeddc25", "20dcfd44", "0000", "5a5d73a8a8592ec1dc8b",
             "002000498ca471fcfbfaa16e3610f005"),
            ("983a16ef4facb351aa9ecc271d7309e2", "509c4b1727d9", "f0a410fc", "058c", "f2dfebb188d35923a07c",
             "05258cf4d85152f4d9af1a64f1d07021"),
            ("983a16ef4facb351aa9ecc271d7309e2", "509c4b1727d9", "f0a410fc", "058d", "f2dfebb188d35923a07c",
             "05258d09f81543b76a596fc2c6738b30")):
        r = w.ordem(f"tkipmix {tk} {ta} {iv32} {iv16}", 2)
        verificar(r["ttak"] == ttak and r["rc4k"] == rk, f"mezcla TKIP, TSC {iv32}{iv16} (802.11, anexo M)")
    tk, ta = os.urandom(16), os.urandom(6)
    iv32, iv16 = int.from_bytes(os.urandom(4), "big"), int.from_bytes(os.urandom(2), "big")
    tt, rk = tkip_mix_py(tk, ta, iv32, iv16)
    r = w.ordem(f"tkipmix {tk.hex()} {ta.hex()} {iv32:08x} {iv16:04x}", 2)
    verificar(r["ttak"] == "".join(f"{v:04x}" for v in tt) and r["rc4k"] == rk.hex(), "mezcla TKIP aleatoria contra Python")
    k = "2b7e151628aed2a6abf7158809cf4f3c"
    m = ("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
         "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710")
    for n, t in ((0, "bb1d6929e95937287fa37d129b756746"), (16, "070a16b46b4d4144f79bdd9dd04a287c"),
                 (40, "dfa66747de9ae63030ca32611497c827"), (64, "51f0bebf7e3b9d92fc49741779363cfe")):
        verificar(w.ordem(f"cmac {k} {m[:2 * n] or '-'}")["cmac"] == t, f"AES-CMAC de {n} bytes (RFC 4493)")
    verificar(w.ordem("hmac256 " + "0b" * 20 + " " + b"Hi There".hex())["hmac256"] ==
              "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7", "HMAC-SHA256, RFC 4231 caso 1")
    r = w.ordem("hkdf 000102030405060708090a0b0c " + "0b" * 22 + " f0f1f2f3f4f5f6f7f8f9 42", 2)
    verificar(r["prk"] == "077709362c2e32df0ddc3f0dc47bba6390b6c73bb50f9c3122ec844ad7c2b3e5" and
              r["okm"] == "3cb25f25faacd57a90434f64d0362f2a2d2d0a90cf1a5a4c5db02d56ecc4c5bf34007208d5b887185865",
              "HKDF-SHA256, RFC 5869 caso 1")
    k, c = os.urandom(32), os.urandom(76)
    verificar(w.ordem(f"kdf256 {k.hex()} Pairwise_key_expansion {c.hex()} 48")["kdf256"] ==
              kdf256(k, "Pairwise key expansion", c, 48).hex(), "KDF-SHA256 de la 802.11 (384 bits) contra Python")

    # --- P-256 (FIPS 186-4; la clave de RFC 6979, A.2.5)
    r = w.ordem("p256mul " + "00" * 31 + "02 -")["ponto"]
    verificar(r == "7cf27b188d034f7e8a52380304b51ac3c08969e277f21b35a60b48fc47669978"
                   "07775510db8ed040293d9ac69f7430dbba7dade63ce982299e04b79d227873d1", "P-256: 2G")
    r = w.ordem("p256mul c9afa9d845ba75166b5c215767b1d6934e50c3db36e89b127b8a622b120f6721 -")["ponto"]
    verificar(r == "60fed4ba255a9d31c961eb74c6356d68c049b8923b61fa6ce669622e60f29fb6"
                   "7903fe1008b8bc99a41ae9e95628bc64f2f1b20c2d7e9f5177a3c294d4462299", "P-256: la clave pública de RFC 6979 (A.2.5)")
    verificar(w.ordem("p256mul " + f"{EC_N - 1:064x} -")["ponto"] == ec_bytes((EC_G[0], EC_P - EC_G[1])).hex(), "P-256: (n-1)G = -G")
    verificar(w.ordem("p256mul " + f"{EC_N:064x} -")["ponto"] == "infinito", "P-256: nG = infinito")
    for _ in range(3):
        k1, k2 = int.from_bytes(os.urandom(32), "big") % EC_N, int.from_bytes(os.urandom(32), "big") % EC_N
        a = ec_mul(k1, EC_G)
        r = w.ordem(f"p256mul {k2:064x} {ec_bytes(a).hex()}")["ponto"]
        verificar(r == ec_bytes(ec_mul(k2, a)).hex(), "P-256: k·P de un punto cualquiera contra Python")
        b = ec_mul(k2, EC_G)
        verificar(w.ordem(f"p256add {ec_bytes(a).hex()} {ec_bytes(b).hex()}")["ponto"] == ec_bytes(ec_add(a, b)).hex(),
                  "P-256: P + Q contra Python")
    verificar(w.ordem(f"p256add {ec_bytes(a).hex()} {ec_bytes(a).hex()}")["ponto"] == ec_bytes(ec_add(a, a)).hex(), "P-256: P + P")
    verificar(w.ordem(f"p256add {ec_bytes(a).hex()} {ec_bytes((a[0], EC_P - a[1])).hex()}")["ponto"] == "infinito",
              "P-256: P + (-P) = infinito")
    verificar(w.ordem(f"p256ok {ec_bytes((a[0], a[1] ^ 1)).hex()}")["valido"] == "0", "P-256: punto fuera de la curva rechazado")
    try:
        from cryptography.hazmat.primitives.asymmetric import ec as _ec
        d = int.from_bytes(os.urandom(32), "big") % EC_N
        pub = _ec.derive_private_key(d, _ec.SECP256R1()).public_key().public_numbers()
        verificar(w.ordem(f"p256mul {d:064x} -")["ponto"] == f"{pub.x:064x}{pub.y:064x}", "P-256: d·G contra el módulo cryptography")
    except ImportError:
        print("  AVISO: sin el módulo cryptography no se compara P-256 con él")

    # --- SAE: IEEE 802.11-2020, anexo J.10 (los mismos vectores que hostapd, common_module_tests.c)
    a1, a2 = "4d3f2fffe387", "a5d8aa958e3c"
    pwe = w.ordem(f"saehp mekmitasdigoat {a1} {a2}")["pwe"]
    verificar(pwe == ec_bytes(sae_hp_py(b"mekmitasdigoat", bytes.fromhex(a1), bytes.fromhex(a2))).hex(),
              "SAE: PWE por cazar y picotear igual que en Python")
    r = w.ordem(f"saecommit {pwe} 992465fd3daa3c60aa6565b7f62a2a7f2e12dd12f198faf4fbed89d7ff1ace94 "
                "9507a90f777a044d6a0830b91ea3d5dd70bece44e1acffb86983b5e1bf9fb322")["commit"]
    verificar(r == "2e2c0f0db52440ad146d967114ce005ce1eab0aa2c2e5c2871b774f6c2575c65"
                   "d5ad9e00829707aa36ba8b859738fc961d08243505f47c035376d7ac4bc8d7b9"
                   "5083bf43827d0fc31ed778dd3671fd21a46d1091d64b6f9a1e1272621325dbe1", "SAE: el Commit del anexo J.10 (escalar y elemento)")
    r = w.ordem("saepeer 591b96f3397fb945100848e7b550543b6720d88337ee93fc49fd6df7e08b5223 "
                "e71b9bb048d3873f20556953a96c91536fd8ee6ca9b4a68a148b056a909be03e"
                "83ae208f60f8ef5537858074db06687032399862999b511e0a1552a5fea317c2", 4)
    verificar(r["codigo"] == "0", "SAE: el Commit del otro lado (anexo J.10) se acepta")
    verificar(r["kck"] == "1e733f6d9bd53256287304338831b09a39406d121017073a5c30db36f36cb81a", "SAE: KCK del anexo J.10")
    verificar(r["pmk"] == "4e4dfab1a2dd8ac1a91790f953faaa452ae5c6873ab75b63605ba663f8a7fe59", "SAE: PMK del anexo J.10")
    verificar(r["pmkid"] == "8747a600eea3f9f22475df58ca1e5498", "SAE: PMKID del anexo J.10")
    pt = w.ordem("saept byteme mekmitasdigoat psk4internet")["pt"]
    verificar(pt == ec_bytes(sae_pt_py(b"byteme", b"mekmitasdigoat", b"psk4internet")).hex(), "SAE H2E: PT igual que en Python")
    r = w.ordem(f"saepwept {pt} 00095b66ec1e 000b6bd90246")["pwe"]
    verificar(r == "c93049b9e64000f848201649e999f2b5c22dea69b5632c9df4d633b8aa1f6c1e"
                   "73634e94b53d82e7383a8d258199d9dc1a5ee8269d060382ccbf33e614ff59a0", "SAE H2E: PWE del anexo J.10")
    # Con datos aleatorios, contra Python
    senha, ssid = os.urandom(10).hex().encode(), b"RedWPA3"
    ma, mb = os.urandom(6), os.urandom(6)
    verificar(w.ordem(f"saehp {senha.decode()} {ma.hex()} {mb.hex()}")["pwe"] == ec_bytes(sae_hp_py(senha, ma, mb)).hex(),
              "SAE: PWE aleatorio contra Python (y no depende del orden de las MAC)")
    verificar(w.ordem(f"saehp {senha.decode()} {mb.hex()} {ma.hex()}")["pwe"] == ec_bytes(sae_hp_py(senha, ma, mb)).hex(),
              "SAE: PWE con las MAC al revés")
    pt = w.ordem(f"saept {ssid.decode()} {senha.decode()} -")["pt"]
    verificar(pt == ec_bytes(sae_pt_py(ssid, senha)).hex(), "SAE H2E: PT sin identificador contra Python")
    pwe = w.ordem(f"saepwept {pt} {ma.hex()} {mb.hex()}")["pwe"]
    verificar(pwe == ec_bytes(sae_pwe_pt_py(sae_pt_py(ssid, senha), ma, mb)).hex(), "SAE H2E: PWE contra Python")
    # Intercambio completo contra un punto de acceso en Python
    ap = ApSae(ec_ponto(bytes.fromhex(pwe)))
    rand, mask = (int.from_bytes(os.urandom(32), "big") % (EC_N - 2) + 2 for _ in range(2))
    nosso = bytes.fromhex(w.ordem(f"saecommit {pwe} {rand:064x} {mask:064x}")["commit"])
    r = w.ordem(f"saepeer {ap.commit()[:32].hex()} {ap.commit()[32:].hex()}", 4)
    ap.processar(int.from_bytes(nosso[:32], "big"), ec_ponto(nosso[32:]))
    verificar(r["codigo"] == "0" and r["kck"] == ap.kck.hex() and r["pmk"] == ap.pmk.hex() and r["pmkid"] == ap.pmkid.hex(),
              "SAE: KCK, PMK y PMKID iguales a los del punto de acceso")
    verificar(w.ordem("saeconf 1")["confirm"] == ap.confirm(1, de_mim=False).hex(), "SAE: nuestro Confirm es el que espera el AP")
    verificar(w.ordem(f"saever 1 {ap.confirm(1).hex()}")["verifica"] == "1", "SAE: el Confirm del AP se verifica")
    verificar(w.ordem(f"saever 2 {ap.confirm(1).hex()}")["verifica"] == "0", "SAE: Confirm con otro contador: rechazado")
    verificar(w.ordem(f"saepeer {nosso[:32].hex()} {nosso[32:].hex()}", 4)["codigo"] == "-3", "SAE: nuestro propio Commit reflejado: rechazado")
    verificar(w.ordem(f"saepeer {'00' * 31}01 {ap.commit()[32:].hex()}", 4)["codigo"] == "-1", "SAE: escalar 1: rechazado")
    verificar(w.ordem(f"saepeer {EC_N:064x} {ap.commit()[32:].hex()}", 4)["codigo"] == "-1", "SAE: escalar n: rechazado")
    malo = bytearray(ap.commit()[32:]); malo[63] ^= 1
    verificar(w.ordem(f"saepeer {ap.commit()[:32].hex()} {bytes(malo).hex()}", 4)["codigo"] == "-2", "SAE: elemento fuera de la curva: rechazado")
    # Contraseña equivocada: el Confirm no cuadra
    otro = ec_bytes(sae_hp_py(b"otra clave", ma, mb)).hex()
    ap2 = ApSae(ec_ponto(bytes.fromhex(otro)))
    nosso2 = bytes.fromhex(w.ordem(f"saecommit {ec_bytes(sae_hp_py(senha, ma, mb)).hex()} {rand:064x} {mask:064x}")["commit"])
    w.ordem(f"saepeer {ap2.commit()[:32].hex()} {ap2.commit()[32:].hex()}", 4)
    ap2.processar(int.from_bytes(nosso2[:32], "big"), ec_ponto(nosso2[32:]))
    verificar(w.ordem(f"saever 1 {ap2.confirm(1).hex()}")["verifica"] == "0", "SAE: con otra contraseña el Confirm no cuadra")

    # --- tramas: TKIP, WEP, CCMP por software y BIP
    tk, mtx, mrx = os.urandom(16), os.urandom(8), os.urandom(8)
    cab = bytes([0x08, 0x02, 0, 0]) + bytes.fromhex("e0e1a9000001") + bytes.fromhex("020000aabbcc") + \
        bytes.fromhex("020000ddeeff") + bytes(2)                       # del AP hacia nosotros (From DS)
    dados = bytes([0xAA, 0xAA, 3, 0, 0, 0, 8, 0]) + os.urandom(60)
    q = tkip_cifrar_py(cab + dados, 24, tk, mrx, 0x123456789A)
    r = w.ordem(f"tkipdec {tk.hex()} {mrx.hex()} 24 {q.hex()}", 2)
    verificar(r["codigo"] == "0" and r["claro"] == dados.hex(), "TKIP: trama del AP (Python) descifrada, MIC e ICV bien")
    r = w.ordem(f"tkipdec {tk.hex()} {mrx.hex()} 24 {tkip_cifrar_py(cab + dados, 24, tk, mrx, 7, mic_ruim=True).hex()}", 2)
    verificar(r["codigo"] == "-2", "TKIP: Michael falso con el ICV bien: fallo de MIC (contramedidas)")
    r = w.ordem(f"tkipdec {os.urandom(16).hex()} {mrx.hex()} 24 {q.hex()}", 2)
    verificar(r["codigo"] == "-1", "TKIP: otra clave: falla el ICV (se descarta sin contramedidas)")
    cab_tx = bytes([0x08, 0x01, 0, 0]) + bytes.fromhex("020000aabbcc") + bytes.fromhex("e0e1a9000001") + \
        bytes.fromhex("020000ddeeff") + bytes(2)                       # nuestro, hacia el AP (To DS)
    q = bytes.fromhex(w.ordem(f"tkipcif {tk.hex()} {mtx.hex()} 0000000000ab 24 {(cab_tx + dados).hex()}")["quadro"])
    verificar(q == tkip_cifrar_py(cab_tx + dados, 24, tk, mtx, 0xab) and q[1] & 0x40, "TKIP: nuestra trama igual que en Python")
    qos = bytes([0x88, 0x02, 0, 0]) + cab[4:24] + bytes([5, 0])
    q = tkip_cifrar_py(qos + dados, 26, tk, mrx, 99)
    verificar(w.ordem(f"tkipdec {tk.hex()} {mrx.hex()} 26 {q.hex()}", 2)["codigo"] == "-2",
              "TKIP QoS: Michael con prioridad 0 en Python y TID 5 en la trama: no cuadra")
    # WEP: 40 y 104 bits contra Python (RC4 con IV || clave e ICV CRC-32)
    import zlib
    for kw in (b"12345", b"0123456789abc"):
        q = bytes.fromhex(w.ordem(f"wepcif {kw.hex()} 0a0b0c 1 24 {(cab_tx + dados).hex()}")["quadro"])
        iv = bytes([0x0c, 0x0b, 0x0a])
        claro = dados + zlib.crc32(dados).to_bytes(4, "little")
        verificar(q[24:28] == iv + bytes([0x40]) and q[28:] == rc4(iv + kw, claro), f"WEP {len(kw) * 8 + 24} bits igual que en Python")
        r = w.ordem(f"wepdec {kw.hex()} 24 {q.hex()}", 2)
        verificar(r["codigo"] == "0" and r["claro"] == dados.hex(), f"WEP {len(kw) * 8 + 24} bits descifrado")
        verificar(w.ordem(f"wepdec {b'54321'.hex()} 24 {q.hex()}", 2)["codigo"] == "-1", "WEP con otra clave: ICV malo")
    # CCMP: el vector del anexo M de la 802.11 (M.6.4)
    r = w.ordem("ccmpcif c97c1f67ce371185514a8a19f2bdd52f b5039776e70c 0 24 0848c32c0fd2e128a57c5030f1844408abaea5b8fcba8033"
                "f8ba1a55d02f85ae967bb62fb6cda8eb7e78a050")["quadro"]
    verificar(r == "0848c32c0fd2e128a57c5030f1844408abaea5b8fcba80330ce70020769703b5"
                   "f3d0a2fe9a3dbf2342a643e43246e80c3c04d0197845ce0b16f97623", "CCMP: el vector del anexo M (cabecera, datos y MIC)")
    r = w.ordem("ccmpdec c97c1f67ce371185514a8a19f2bdd52f 24 " + r, 3)
    verificar(r["codigo"] == "0" and r["pn"] == "b5039776e70c" and r["claro"] == "f8ba1a55d02f85ae967bb62fb6cda8eb7e78a050",
              "CCMP: descifrado del vector del anexo M y su PN")
    # Una trama de gestión protegida (802.11w): el nonce lleva el bit de gestión
    deauth = bytes([0xC0, 0x00, 0, 0]) + bytes.fromhex("e0e1a9000001") + bytes.fromhex("020000aabbcc") * 2 + bytes([0x30, 0]) + bytes([7, 0])
    r = w.ordem(f"ccmpcif {tk.hex()} 000000000005 0 24 {deauth.hex()}")["quadro"]
    try:
        from cryptography.hazmat.primitives.ciphers.aead import AESCCM
        aad = bytes([0xC0, 0x40]) + deauth[4:22] + bytes(2)
        nonce = bytes([0x10]) + deauth[10:16] + (5).to_bytes(6, "big")
        verificar(bytes.fromhex(r)[32:] == AESCCM(tk, tag_length=8).encrypt(nonce, bytes([7, 0]), aad),
                  "CCMP de gestión (802.11w) igual que AES-CCM del módulo cryptography")
    except ImportError:
        print("  AVISO: sin el módulo cryptography no se compara el CCMP de gestión")
    verificar(w.ordem(f"ccmpdec {tk.hex()} 24 {r}", 3)["claro"] == "0700", "CCMP de gestión: ida y vuelta")
    malo = bytearray.fromhex(r); malo[-1] ^= 1
    verificar(w.ordem(f"ccmpdec {tk.hex()} 24 {bytes(malo).hex()}", 3)["codigo"] == "-1", "CCMP: MIC tocado: rechazado")
    # BIP-CMAC-128 de una desautenticación de difusión
    igtk = os.urandom(16)
    bdeauth = bytes([0xC0, 0x00, 0, 0]) + b"\xff" * 6 + bytes.fromhex("020000aabbcc") * 2 + bytes([0x40, 0]) + bytes([3, 0])
    q = bytes.fromhex(w.ordem(f"bipass {igtk.hex()} 4 00000000002a {bdeauth.hex()}")["quadro"])
    mmie = bytes([76, 16, 4, 0]) + (42).to_bytes(6, "little")
    m = cmac(igtk, bytes([0xC0, 0x00]) + bdeauth[4:22] + bdeauth[24:] + mmie + bytes(8))
    if m is not None:
        verificar(q == bdeauth + mmie + m[:8], "BIP: el MMIE con el MIC de AES-CMAC (contra cryptography)")
    verificar(w.ordem(f"bipver {igtk.hex()} 4 {q.hex()}")["bip"] == "1 00000000002a", "BIP: verificado, con su IPN")
    verificar(w.ordem(f"bipver {igtk.hex()} 5 {q.hex()}")["bip"].startswith("0"), "BIP: otro keyid: rechazado")
    reint = bytearray(q); reint[1] |= 0x08
    verificar(w.ordem(f"bipver {igtk.hex()} 4 {bytes(reint).hex()}")["bip"].startswith("1"), "BIP: el bit de reintento no entra en la AAD")
    reint[25] ^= 1
    verificar(w.ordem(f"bipver {igtk.hex()} 4 {bytes(reint).hex()}")["bip"].startswith("0"), "BIP: motivo cambiado: rechazado")

    # --- balizas nuevas y lo que se elige
    def rsn_ie(grupo, pares, akms, cap=0, pmkid=None, gestao=None):
        c = bytes([1, 0, 0, 0x0F, 0xAC, grupo]) + len(pares).to_bytes(2, "little") + \
            b"".join(bytes([0, 0x0F, 0xAC, p]) for p in pares) + len(akms).to_bytes(2, "little") + \
            b"".join(bytes([0, 0x0F, 0xAC, a]) for a in akms) + cap.to_bytes(2, "little")
        if gestao is not None:
            c += bytes(2) + bytes([0, 0x0F, 0xAC, gestao])
        return bytes([48, len(c)]) + c

    def wpa_ie(grupo, pares):
        c = bytes([0, 0x50, 0xF2, 1, 1, 0, 0, 0x50, 0xF2, grupo]) + len(pares).to_bytes(2, "little") + \
            b"".join(bytes([0, 0x50, 0xF2, p]) for p in pares) + bytes([1, 0, 0, 0x50, 0xF2, 2])
        return bytes([221, len(c)]) + c

    ssid = bytes([0, 4]) + b"Rede"
    r = w.ordem("ies2 1 " + (ssid + wpa_ie(2, [2])).hex(), 4)
    verificar("seg=2" in r["ies2"] and "wpa1=1 wgrupo=2 wpares=4 wpsk=1 nwpa1=24" in r["ies2"], f"WPA1 con TKIP y PSK: {r['ies2']}")
    verificar(r["escolha1"] == "wpa1=1 akm=2 par=2 grupo=2 pmf=0" and r["escolha0"] == "nada", "WPA1: se elige WPA con TKIP (y nada sin PMK)")
    r = w.ordem("ies2 1 " + (ssid + wpa_ie(2, [2, 4]) + rsn_ie(2, [4, 2], [2])).hex(), 4)
    verificar("seg=7" in r["ies2"] and r["escolha1"] == "wpa1=0 akm=2 par=4 grupo=2 pmf=0",
              f"WPA/WPA2 mixto: se elige el RSN con par CCMP y grupo TKIP: {r}")
    sae_rsn = rsn_ie(4, [4], [8], cap=0xC0)
    r = w.ordem("ies2 1 " + (ssid + sae_rsn + bytes([244, 1, 0x20])).hex(), 4)
    verificar("seg=5 akms=4 mfpc=1 mfpr=1 gestao=6 h2e=1 soh2e=0 nrsnxe=3" in r["ies2"], f"WPA3 con H2E: {r['ies2']}")
    verificar(r["escolha0"] == "wpa1=0 akm=8 par=4 grupo=4 pmf=2" and r["escolha1"] == "nada", "WPA3: SAE con PMF obligatoria")
    r = w.ordem("ies2 1 " + (ssid + rsn_ie(4, [4], [2, 8], cap=0x80) + bytes([1, 3, 0x82, 0x84, 0x80 | 123])).hex(), 4)
    verificar("seg=6 akms=5 mfpc=1 mfpr=0" in r["ies2"] and "soh2e=1" in r["ies2"], f"WPA2/WPA3 de transición: {r['ies2']}")
    verificar(r["escolha2"] == "wpa1=0 akm=8 par=4 grupo=4 pmf=2" and r["escolha1"] == "wpa1=0 akm=2 par=4 grupo=4 pmf=0",
              "transición: SAE si hay con qué, si no WPA2-PSK sin PMF")
    r = w.ordem("ies2 1 " + (ssid + rsn_ie(4, [4], [6], cap=0xC0)).hex(), 4)
    verificar(r["escolha1"] == "wpa1=0 akm=6 par=4 grupo=4 pmf=2", "WPA2 con PMF obligatoria: PSK-SHA256")
    r = w.ordem("ies2 1 " + (ssid + rsn_ie(4, [4], [8], cap=0xC0, gestao=12)).hex(), 4)
    verificar(r["escolha0"] == "nada", "WPA3 con BIP-GMAC-256 (no admitido): nada")

    # --- saludos contra un punto de acceso simulado
    if envolver(bytes(16), bytes(16)) is None or cmac(bytes(16), b"") is None:
        print("  AVISO: sin el módulo cryptography no se prueban los saludos de la D220")
        return
    aa, spa = bytes.fromhex("020000aabbcc"), bytes.fromhex("e0e1a9000001")
    pmk = hashlib.pbkdf2_hmac("sha1", b"clave de prueba", b"RedVieja", 4096, 32)

    # 1) WPA1 con TKIP: descriptor 254, versión 1 (HMAC-MD5; la GTK con RC4)
    ie_ap = wpa_ie(2, [2])
    anonce, snonce = os.urandom(32), os.urandom(32)
    r = w.ordem(f"supx 1,2,2,2,0,0 {pmk.hex()} {aa.hex()} {spa.hex()} {ie_ap.hex()} - - {snonce.hex()}", 2)
    verificar(r["ie"] == ie_ap.hex() and r["versao"] == "1", "WPA1: nuestro elemento WPA y versión 1 del descriptor")
    m1 = eapol_x(0x0089, (1).to_bytes(8, "big"), anonce, tipo=254, tam=32)
    r = w.ordem("eapol " + m1.hex(), 5)
    ptk = ptk_x(pmk, aa, spa, anonce, snonce, 64)
    kck, kek = ptk[:16], ptk[16:32]
    m2 = bytes.fromhex(r["resp"])
    verificar(r["codigo"] == "1" and m2[4] == 254 and int.from_bytes(m2[5:7], "big") == 0x0109 and mic_ok_x(1, kck, m2),
              "WPA1 mensaje 2: descriptor 254, key info 0x0109 y MIC HMAC-MD5")
    verificar(m2[99:] == ie_ap and m2[7:9] == (32).to_bytes(2, "big"), "WPA1 mensaje 2: el elemento WPA y la longitud de la clave")
    verificar(bytes.fromhex(w.ordem("chaves", 7)["ptk"]) == ptk, "WPA1: la PTK de TKIP (64 bytes)")
    m3 = eapol_x(0x01C9, (2).to_bytes(8, "big"), anonce, ie_ap, kck=kck, tipo=254, tam=32)
    r = w.ordem("eapol " + m3.hex(), 5)
    m4 = bytes.fromhex(r["resp"])
    verificar(r["codigo"] == "2" and int.from_bytes(m4[5:7], "big") == 0x0109 and mic_ok_x(1, kck, m4),
              "WPA1 mensaje 3 (WPA en claro) aceptado; mensaje 4 con key info 0x0109")
    m3x = eapol_x(0x01C9, (3).to_bytes(8, "big"), anonce, wpa_ie(2, [4]), kck=kck, tipo=254, tam=32)
    verificar(w.ordem("eapol " + m3x.hex(), 5)["codigo"] == "-2", "WPA1 mensaje 3 con otro elemento WPA: rechazado")
    gtk = os.urandom(32)
    iv = os.urandom(16)
    g1 = eapol_x(0x03A1, (4).to_bytes(8, "big"), bytes(32), rc4(iv + kek, gtk, 256), kck=kck, tipo=254, iv=iv, tam=32)
    r = w.ordem("eapol " + g1.hex(), 5)
    g2 = bytes.fromhex(r["resp"])
    verificar(r["codigo"] == "3" and r["gtk"] == gtk.hex() and r["gtkid"] == "2", "WPA1 grupo: la GTK de TKIP (RC4) con id 2")
    verificar(mic_ok_x(1, kck, g2) and int.from_bytes(g2[5:7], "big") == 0x0321, "WPA1 grupo mensaje 2: key info 0x0321 (con el índice)")
    f = bytes.fromhex(w.ordem("falhamic 1")["resp"])
    verificar(mic_ok_x(1, kck, f) and int.from_bytes(f[5:7], "big") == 0x0F09 and f[9:17] == (5).to_bytes(8, "big"),
              "fallo de Michael: EAPOL-Key con error y petición, contador nuevo y MIC")

    # 2) RSN con TKIP de par: versión 1, datos del mensaje 3 cifrados con RC4
    ie_ap = rsn_ie(2, [2], [2])
    anonce, snonce = os.urandom(32), os.urandom(32)
    w.ordem(f"supx 0,2,2,2,0,0 {pmk.hex()} {aa.hex()} {spa.hex()} {ie_ap.hex()} - - {snonce.hex()}", 2)
    r = w.ordem("eapol " + eapol_x(0x0089, (1).to_bytes(8, "big"), anonce, tam=32).hex(), 5)
    ptk = ptk_x(pmk, aa, spa, anonce, snonce, 64)
    kck, kek = ptk[:16], ptk[16:32]
    verificar(mic_ok_x(1, kck, bytes.fromhex(r["resp"])), "WPA2-TKIP mensaje 2: MIC HMAC-MD5")
    gtk = os.urandom(32)
    iv = os.urandom(16)
    claro = ie_ap + bytes([0xDD, 38, 0, 0x0F, 0xAC, 1, 1, 0]) + gtk
    m3 = eapol_x(0x13C9, (2).to_bytes(8, "big"), anonce, rc4(iv + kek, claro, 256), kck=kck, iv=iv, tam=32)
    r = w.ordem("eapol " + m3.hex(), 5)
    verificar(r["codigo"] == "2" and r["gtk"] == gtk.hex() and mic_ok_x(1, kck, bytes.fromhex(r["resp"])),
              "WPA2-TKIP mensaje 3: KDE descifradas con RC4, GTK de 32 bytes")

    # 3) WPA/WPA2 mixto: par CCMP (versión 2) y grupo TKIP
    ie_ap = rsn_ie(2, [4, 2], [2])
    anonce, snonce = os.urandom(32), os.urandom(32)
    w.ordem(f"supx 0,2,4,2,0,0 {pmk.hex()} {aa.hex()} {spa.hex()} {ie_ap.hex()} - - {snonce.hex()}", 2)
    r = w.ordem("eapol " + eapol_x(0x008A, (1).to_bytes(8, "big"), anonce).hex(), 5)
    ptk = ptk_x(pmk, aa, spa, anonce, snonce, 48)
    kck, kek = ptk[:16], ptk[16:32]
    m2 = bytes.fromhex(r["resp"])
    verificar(mic_ok_x(2, kck, m2) and m2[99:] == bytes([48, 20, 1, 0, 0, 0x0F, 0xAC, 2, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 2, 0, 0]),
              "mixto: mensaje 2 con nuestro RSN (grupo TKIP, par CCMP)")
    gtk = os.urandom(32)
    m3 = eapol_x(0x13CA, (2).to_bytes(8, "big"), anonce, envolver(kek, kde_pad(ie_ap + bytes([0xDD, 38, 0, 0x0F, 0xAC, 1, 1, 0]) + gtk)), kck=kck)
    r = w.ordem("eapol " + m3.hex(), 5)
    verificar(r["codigo"] == "2" and r["gtk"] == gtk.hex() and r["ptk"] == ptk.hex(), "mixto: PTK de CCMP y GTK de TKIP (32 bytes)")

    # 4) WPA3-SAE: AKM 8, descriptor 0 (AES-CMAC, KDF-SHA256), RSNXE, PMKID e IGTK
    pmk3 = os.urandom(32)
    pmkid = os.urandom(16)
    ie_ap = rsn_ie(4, [4], [8], cap=0xC0)
    rsnxe = bytes([244, 1, 0x20])
    anonce, snonce = os.urandom(32), os.urandom(32)
    r = w.ordem(f"supx 0,8,4,4,2,1 {pmk3.hex()} {aa.hex()} {spa.hex()} {ie_ap.hex()} {rsnxe.hex()} {pmkid.hex()} {snonce.hex()}", 2)
    verificar(r["ie"] == (bytes([48, 20, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 4, 1, 0, 0, 0x0F, 0xAC, 8, 0xC0, 0]) + rsnxe).hex()
              and r["versao"] == "0", "SAE: nuestro RSN (AKM 8, MFPC y MFPR) con el RSNXE y descriptor 0")
    kde_pmkid = bytes([0xDD, 20, 0, 0x0F, 0xAC, 4]) + pmkid
    r = w.ordem("eapol " + eapol_x(0x0088, (1).to_bytes(8, "big"), anonce, kde_pmkid, tam=16).hex(), 5)
    ptk = kdf256(pmk3, "Pairwise key expansion", min(aa, spa) + max(aa, spa) + min(anonce, snonce) + max(anonce, snonce), 48)
    kck, kek = ptk[:16], ptk[16:32]
    m2 = bytes.fromhex(r["resp"])
    verificar(r["codigo"] == "1" and r["ptk"] == ptk.hex() and mic_ok_x(0, kck, m2) and int.from_bytes(m2[5:7], "big") == 0x0108,
              "SAE mensaje 2: PTK con KDF-SHA256, MIC AES-CMAC, key info 0x0108")
    verificar(m2[99:].endswith(rsnxe), "SAE mensaje 2: lleva el RSNXE")
    verificar(w.ordem("chaves", 7)["pmkid"] == "1 1", "SAE: el PMKID del mensaje 1 es el del SAE")
    gtk, igtk = os.urandom(16), os.urandom(16)
    kdes = ie_ap + rsnxe + bytes([0xDD, 22, 0, 0x0F, 0xAC, 1, 1, 0]) + gtk + \
        bytes([0xDD, 28, 0, 0x0F, 0xAC, 9, 4, 0]) + (9).to_bytes(6, "little") + igtk
    m3 = eapol_x(0x13C8, (2).to_bytes(8, "big"), anonce, envolver(kek, kde_pad(kdes)), kck=kck)
    m3_sem_rsnxe = eapol_x(0x13C8, (2).to_bytes(8, "big"), anonce,
                           envolver(kek, kde_pad(ie_ap + bytes([0xDD, 22, 0, 0x0F, 0xAC, 1, 1, 0]) + gtk)), kck=kck)
    verificar(w.ordem("eapol " + m3_sem_rsnxe.hex(), 5)["codigo"] == "-2", "SAE mensaje 3 sin el RSNXE de la baliza (degradación del H2E): rechazado")
    r = w.ordem("eapol " + m3.hex(), 5)
    m4 = bytes.fromhex(r["resp"])
    verificar(r["codigo"] == "2" and r["gtk"] == gtk.hex() and mic_ok_x(0, kck, m4) and int.from_bytes(m4[5:7], "big") == 0x0308,
              "SAE mensaje 3: GTK y mensaje 4 con AES-CMAC")
    c = w.ordem("chaves", 7)
    verificar(c["igtk"] == igtk.hex() and c["igtkid"] == "4" and c["ipn"] == (9).to_bytes(6, "little").hex(),
              "SAE: IGTK (802.11w) con su id y su IPN")
    igtk2 = os.urandom(16)
    g1 = eapol_x(0x1380, (3).to_bytes(8, "big"), bytes(32), envolver(kek, kde_pad(
        bytes([0xDD, 22, 0, 0x0F, 0xAC, 1, 2, 0]) + gtk + bytes([0xDD, 28, 0, 0x0F, 0xAC, 9, 5, 0]) + bytes(6) + igtk2)), kck=kck)
    r = w.ordem("eapol " + g1.hex(), 5)
    verificar(r["codigo"] == "3" and w.ordem("chaves", 7)["igtk"] == igtk2.hex(), "SAE: renovación de la GTK y de la IGTK")
    m3f = eapol_x(0x13C8, (9).to_bytes(8, "big"), anonce, envolver(kek, kde_pad(kdes)), kck=kck, mic_ruim=True)
    verificar(w.ordem("eapol " + m3f.hex(), 5)["codigo"] == "-1", "SAE mensaje 3 con MIC falso: rechazado")

    # 5) PSK-SHA256 (AKM 6, descriptor 3)
    ie_ap = rsn_ie(4, [4], [6], cap=0xC0)
    anonce, snonce = os.urandom(32), os.urandom(32)
    r = w.ordem(f"supx 0,6,4,4,2,0 {pmk.hex()} {aa.hex()} {spa.hex()} {ie_ap.hex()} - - {snonce.hex()}", 2)
    verificar(r["versao"] == "3", "PSK-SHA256: descriptor 3")
    r = w.ordem("eapol " + eapol_x(0x008B, (1).to_bytes(8, "big"), anonce).hex(), 5)
    ptk = kdf256(pmk, "Pairwise key expansion", min(aa, spa) + max(aa, spa) + min(anonce, snonce) + max(anonce, snonce), 48)
    verificar(r["ptk"] == ptk.hex() and mic_ok_x(3, ptk[:16], bytes.fromhex(r["resp"])), "PSK-SHA256: PTK y MIC AES-CMAC")
    verificar(w.ordem("eapol " + eapol_x(0x008A, (1).to_bytes(8, "big"), anonce).hex(), 5)["codigo"] == "0",
              "PSK-SHA256: un mensaje 1 de versión 2 se ignora")


# ------------------------------------------------------------ fuentes


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def parte_fuentes():
    usb = ler("kernel/usb/usb.c")
    verificar("wifi_usb_reconhecer(d)" in usb and "wifi_usb_retirado(d)" in usb, "usb.c pasa el pincho a wifi.c")
    wifi = ler("kernel/rede/wifi.c")
    verificar("0x1111" in wifi and "0x1B" in wifi, "cambio de modo del CD virtual 1111:1111 (START STOP UNIT)")
    verificar("SAGUI_SEM_FIRMWARE" in wifi, "con SEM_FIRMWARE=1 el driver dice que falta el firmware")
    mk = ler("Makefile")
    verificar("firmware/realtek/rtl8188eufw.bin" in mk and "FIRMWARE_RAMDISK" in mk, "el firmware entra en el ramdisk")
    # D220: WEP, TKIP y WPA3 en la pila y en el programa
    verificar("tkip_decifrar(" in wifi and "wep_decifrar(" in wifi and "auth_sae()" in wifi and "auth_compartida()" in wifi,
              "wifi.c descifra TKIP y WEP y hace SAE y la clave compartida")
    verificar("bip_verificar(" in wifi and "iniciar_sa_query()" in wifi and "michael_falhou(" in wifi,
              "wifi.c: BIP, consulta SA (802.11w) y contramedidas de TKIP")
    verificar("$(B)/userland/sae.o" in mk and "$(B)/userland/p256.o" in mk, "el programa wifi lleva el SAE y P-256")
    abi = ler("kernel/inc/abi.h")
    verificar("WIFI_WPA2_WPA3" in abi and "pwe_wifi_t" in abi, "abi.h: seguridad exacta y el PT/PWE de SAE en el pedido")
    prog = ler("userland/programas/wifi.c")
    verificar("WEP, que no protege nada" in prog and "sae_pwe_hp(" in prog and "sae_pt(" in prog,
              "programa wifi: aviso de WEP y el PT/PWE de SAE calculados en el programa")
    verificar("SIS_WIFI" in ler("kernel/inc/abi.h") and "sis_wifi" in ler("kernel/nucleo/sistema.c"), "llamada SIS_WIFI")
    # D238: rendimiento
    rtl = ler("kernel/rede/rtl8188eu.c")
    verificar("usb_rx_abrir_fila(" in rtl and "usb_tx_abrir_fila(" in rtl and "r88e_rx_simples(" in rtl,
              "rtl8188eu.c: colas USB de recepción y envío, con vuelta a una lectura cada vez")
    verificar("wn_rx_desglosar(" in rtl and "R92C_RAID_11GN" in rtl and "R92C_TXDW5_SGI" in rtl,
              "rtl8188eu.c: desglose de wifi_n.c y tasas HT en el descriptor")
    verificar("wn_ie_ht_cap(" in wifi and "wn_ie_wmm(" in wifi and "recusar_addba(" in wifi and "amsdu_chegou(" in wifi,
              "wifi.c: HT y WMM en la asociación, ADDBA rechazado y A-MSDU")
    verificar("ht_vetar(" in wifi and "wifi-desempenho.conf" in wifi and "rede_acordar()" in wifi,
              "wifi.c: vuelta a b/g, ajustes y la pila despertada")
    verificar("xhci_fila_abrir" in ler("kernel/usb/xhci.c") and "ehci_rx_abrir_fila" in ler("kernel/usb/ehci.c"),
              "xHCI y EHCI: colas de transferencias en vuelo")
    verificar("tx_cola_llena" in abi and "taxa_dec" in abi and "rx_en_vuelo" in abi, "abi.h: contadores de rendimiento")
    verificar("lecturas (cola de" in prog and "MCS %u" in prog, "programa wifi: contadores y MCS")
    ter = ler("docs/terceros.md")
    verificar("kernel/terceros/bsd/rtwn/" in ter and "firmware/realtek/rtl8188eufw.bin" in ter, "docs/terceros.md registra rtwn y el firmware")
    cab = ler("kernel/terceros/bsd/rtwn/r92creg_88e.h")
    verificar("SPDX-License-Identifier: ISC" in cab and "Damien Bergamini" in cab, "la cabecera de OpenBSD con su licencia ISC")
    r = subprocess.run([os.path.join(RAIZ, "tools", "firmware_rtl8188eu.sh"), "--comprobar"], capture_output=True, text=True)
    verificar(r.returncode == 0, "SHA-256 del firmware y de su licencia")
    rf = os.path.join(RAIZ, "build", "rootfs", "lib", "firmware", "rtl8188eufw.bin")
    if os.path.exists(rf):
        verificar(open(rf, "rb").read() == open(os.path.join(RAIZ, "firmware", "realtek", "rtl8188eufw.bin"), "rb").read(),
                  "el ramdisk lleva /lib/firmware/rtl8188eufw.bin")


# ------------------------------------------------------------ QEMU


def ofuscar(txt):
    """Quita SSID y MAC de la salida de «wifi buscar» (no son nuestros)."""
    s = []
    for l in txt.splitlines():
        m = re.match(r"^(.{32}) +(\d+) +(-?\d+) dBm", l)
        if m and not l.startswith("SSID"):
            l = "*" * 8 + l[32:]
        s.append(re.sub(r"([0-9a-f]{2}:){5}[0-9a-f]{2}", "xx:xx:xx:xx:xx:xx", l))
    return "\n".join(s)


def parte_sin(imagem):
    with Maquina("h215-sin", "ide", imagem=imagem) as m:
        m.esperar_serial("sagui:/$", 150)
        out = m.comando("wifi")
        verificar("no hay ningún adaptador" in out, "sin pincho: wifi dice que no hay adaptador")
        out = m.comando("ifconfig wlan0")
        verificar("no hay wlan0" in out, "sin pincho: ifconfig wlan0")
        out = m.comando("ifconfig")
        verificar("wlan0" not in out and "10.0.2.15" in out, "sin pincho: ifconfig sigue con la eth0")
        verificar("[WIFI]" not in m.serial(), "sin pincho: ningún mensaje [WIFI]")


def pincho_presente():
    for d in glob.glob("/sys/bus/usb/devices/*"):
        try:
            v = open(os.path.join(d, "idVendor")).read().strip()
            p = open(os.path.join(d, "idProduct")).read().strip()
        except OSError:
            continue
        if (v, p) in (("0bda", "8179"), ("1111", "1111")):
            return v + ":" + p
    return None


def parte_pincho(imagem):
    ident = pincho_presente()
    if not ident:
        print("  AVISO: no hay ningún pincho RTL8188EU (0bda:8179) en /sys/bus/usb/devices: se salta la parte con el "
              "pincho real (docs/usb-real.md: usbipd attach --wsl)")
        return
    if int(subprocess.run(["pgrep", "-c", "qemu-system"], capture_output=True, text=True).stdout or 0) > 0:
        print("  AVISO: ya hay otro QEMU: puede tener el pincho (solo uno a la vez)")
    # Con un pendrive en el mismo EHCI: la recepción continua del Wi-Fi (una QH
    # fija en el anillo asíncrono) no puede estorbar a sus transferencias.
    dados = os.urandom(3 * 1024 * 1024)
    pen = criar_pendrive("h215-pen", mb=64, rotulo="WIFIPEN", arquivos={"d3.bin": dados})
    extras = ["-device", "usb-ehci,id=ehci",
              "-device", "usb-host,bus=ehci.0,vendorid=0x0bda,productid=0x8179",
              "-device", "usb-host,bus=ehci.0,vendorid=0x1111,productid=0x1111"] + pendrive_qemu(pen, "p1")
    with Maquina("h215-pincho", "ide", imagem=imagem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 150)
        fim = time.time() + 90
        while time.time() < fim:
            s = m.serial()
            if "[WIFI] firmware" in s or re.search(r"\[WIFI\] (falta|el |la |no |sin |fallo)", s):
                break
            time.sleep(1)
        s = m.serial()
        verificar("[USB] conectado: Realtek" in s or "1111:1111" in s, f"el pincho ({ident}) aparece en el USB")
        mm = re.search(r"\[WIFI\] Realtek RTL8188(EUS|ETV) \(versión [0-9a-f]+\), MAC ([0-9a-f:]{17}) \(eFuse\)", s)
        verificar(mm is not None, "MAC leída de la eFuse")
        mac = bytes.fromhex(mm.group(2).replace(":", ""))
        verificar(mac != bytes(6) and mac != b"\xff" * 6 and not (mac[0] & 1), "MAC de la eFuse válida (unicast)")
        verificar(re.search(r"\[WIFI\] firmware \d+\.\d+ .* cargado; chip listo", s) is not None, "firmware cargado y chip listo")
        out = m.comando("wifi", 60)
        verificar("Estado:    listo, sin conectar" in out, "wifi: listo, sin conectar")
        out = m.comando("wifi buscar", 120)
        print(ofuscar(out))
        n = re.search(r"(\d+) redes", out)
        verificar(n is not None and int(n.group(1)) >= 1, "wifi buscar: al menos una red")
        verificar(re.search(r"^\S.{31} +\d+ +-\d+ dBm", out, re.M) is not None, "cada red con canal y señal en dBm")
        out = m.comando("wifi", 60)
        # D238: la cola de recepción armada (o la vuelta avisada) y los contadores
        s = m.serial()
        verificar("[WIFI] recepción: cola de 8 lecturas" in s or "[WIFI] recepción: sin cola" in s,
                  "D238: la recepción dice si va con cola")
        verificar(re.search(r"^USB: +\d+ lecturas \(cola de \d+, \d+ en vuelo", out, re.M) is not None,
                  "D238: wifi muestra las lecturas USB y las que hay en vuelo")
        verificar("la cola de recepción no trajo nada" not in s, "D238: la cola de recepción trae las balizas")
        rs = re.search(r"(\d+) respuestas a nuestros sondeos", out)
        print(f"  (informativo) respuestas de sondeo dirigidas a nosotros: {rs.group(1) if rs else '?'}"
              " (si es > 0, la transmisión funciona)")
        out = m.comando("ifconfig wlan0", 60)
        verificar("wlan0: Realtek RTL8188EU" in out and "sin conectar" in out, "ifconfig wlan0: la interfaz, sin conectar")
        out = m.comando("ifconfig", 60)
        verificar("eth0:" in out and "wlan0:" in out, "ifconfig: eth0 activa y wlan0 en la lista")
        out = m.comando("infopc -t", 120)
        verificar("Wi-Fi:" in out and "driver rtl8188eu" in out, "infopc: la línea del Wi-Fi")
        m.comando("montar usb0", 60)
        out = m.comando("sha256sum /usb/d3.bin", 300)
        verificar(hashlib.sha256(dados).hexdigest() in out, "pendrive en el mismo EHCI que el Wi-Fi: 3 MB leídos íntegros")
        out = m.comando("wifi buscar", 120)
        verificar(re.search(r"(\d+) redes", out) is not None, "el Wi-Fi sigue buscando después del pendrive")
        verificar("[WIFI] registro" not in m.serial(), "sin errores USB en los registros del chip")


def parte_sem_firmware():
    """make SEM_FIRMWARE=1: sin /lib/firmware; con el pincho, el driver dice que falta."""
    b = os.path.join(SAIDA, "h215", "build-sem-firmware")
    os.makedirs(os.path.dirname(b), exist_ok=True)
    r = subprocess.run(["make", "-j2", "SEM_FIRMWARE=1", "B=" + b, b + "/saguios.img"], cwd=RAIZ,
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "make SEM_FIRMWARE=1 compila " + r.stderr[-300:])
    verificar(not os.path.exists(os.path.join(b, "rootfs", "lib", "firmware")), "SEM_FIRMWARE=1: el ramdisk no lleva /lib/firmware")
    img = os.path.join(SAIDA, "h215", "sem-firmware-console.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", img, "--vesa", "--console"],
                   check=True, stdout=subprocess.DEVNULL)
    ident = pincho_presente()
    extras = []
    if ident:
        extras = ["-device", "usb-ehci,id=ehci",
                  "-device", "usb-host,bus=ehci.0,vendorid=0x0bda,productid=0x8179",
                  "-device", "usb-host,bus=ehci.0,vendorid=0x1111,productid=0x1111"]
    with Maquina("h215-sem-firmware", "ide", imagem=img, extras=extras) as m:
        m.esperar_serial("sagui:/$", 150)
        if not ident:
            print("  AVISO: sin el pincho, SEM_FIRMWARE=1 solo se comprueba arrancando")
            verificar("no hay ningún adaptador" in m.comando("wifi"), "SEM_FIRMWARE=1 sin pincho: arranca y wifi responde")
            return
        fim = time.time() + 60
        while time.time() < fim and "no lleva firmware" not in m.serial():
            time.sleep(1)
        verificar("esta compilación no lleva firmware (SEM_FIRMWARE=1)" in m.serial(), "SEM_FIRMWARE=1: el driver dice que falta el firmware")
        out = m.comando("wifi")
        verificar("Estado:    sin firmware" in out and "MAC:" in out, "wifi: sin firmware, pero con la MAC de la eFuse")


def parte_pincho_xhci(imagem):
    """El pincho por el xHCI (qemu-xhci): la recepción continua es la lectura armada del xHCI."""
    if not pincho_presente():
        print("  AVISO: sin el pincho, se salta la parte del xHCI")
        return
    extras = ["-device", "qemu-xhci,id=xhci",
              "-device", "usb-host,bus=xhci.0,vendorid=0x0bda,productid=0x8179",
              "-device", "usb-host,bus=xhci.0,vendorid=0x1111,productid=0x1111"]
    with Maquina("h215-pincho-xhci", "ide", imagem=imagem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 150)
        fim = time.time() + 90
        while time.time() < fim and "[WIFI] firmware" not in m.serial():
            time.sleep(1)
        s = m.serial()
        verificar("(0bda:8179)" in s and "xHCI, 480 Mb/s" in s, "xHCI: el pincho conectado a alta velocidad")
        verificar(re.search(r"\[WIFI\] firmware \d+\.\d+ .* cargado; chip listo", s) is not None, "xHCI: firmware cargado")
        out = m.comando("wifi buscar", 120)
        n = re.search(r"(\d+) redes", out)
        verificar(n is not None and int(n.group(1)) >= 1, "xHCI: wifi buscar con al menos una red")


def main():
    partes = sys.argv[1:] or ["anfitrion", "fuentes", "sin", "pincho", "pincho-xhci", "sem-firmware"]
    if "anfitrion" in partes:
        parte_anfitriao()
    if "fuentes" in partes:
        parte_fuentes()
    imagem = imagem_console() if ("sin" in partes or "pincho" in partes) else None
    if "sin" in partes:
        parte_sin(imagem)
    if "pincho" in partes:
        parte_pincho(imagem)
    if "pincho-xhci" in partes:
        parte_pincho_xhci(imagem if imagem else imagem_console())
    if "sem-firmware" in partes:
        parte_sem_firmware()
    print("H215 ok")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
