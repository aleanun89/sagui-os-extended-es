#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H59 - HTTPS: TLS 1.2/1.3, certificados X.509 y su gestión (D060-D063).

El anfitrión genera en el momento (con la biblioteca "cryptography" de
python) una CA de prueba, una CA intermedia ECDSA P-384 y certificados de
servidor para 10.0.2.2 (la IP del anfitrión vista desde la red "user" de
QEMU), y levanta servidores HTTPS con el módulo ssl de python (OpenSSL 3).

Criterio:
  1. /testes/pruebacripto: todas las primitivas con los vectores oficiales
     (FIPS 180-4/197, SP 800-38D, RFC 4231/5869/7748/8439/6979) y el
     generador aleatorio del núcleo (SIS_ALEATORIO, /dev/urandom).
  2. curl a un servidor con la CA de prueba: "emisor desconocido"; después de
     "certificados añadir ca.pem" funciona con TLS 1.3 y TLS 1.2 (versión
     forzada en el servidor), con RSA (cadena directa) y con ECDSA (cadena
     con intermedia P-384); -v muestra versión, suite y certificado.
  3. Nombre que no coincide, certificado caducado, y -k que los acepta.
  4. wget por HTTPS de 300 KB: el SHA-256 (sha256sum) coincide; se mide el
     tiempo.
  5. "certificados probar" enseña la cadena y el resultado; "certificados
     quitar" deja de confiar.
  6. Con "-cpu max" (AES-NI y RDRAND emulados), los caminos de hardware.
  7. Cipó abre una página HTTPS (con la CA de prueba en el ramdisk) y
     enseña "Conexión no segura" con el motivo ante un certificado caducado.
  8. Con un disco persistente en /home: "certificados anadir" sobrevive a
     un reinicio.
  9. (opcional) un sitio real de internet; se omite si no hay red.
"""
import datetime
import functools
import hashlib
import http.server
import ipaddress
import os
import random
import re
import shutil
import socket
import ssl
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, RAIZ, SAIDA, opcoes_apps  # noqa: E402

from cryptography import x509  # noqa: E402
from cryptography.hazmat.primitives import hashes, serialization  # noqa: E402
from cryptography.hazmat.primitives.asymmetric import ec, rsa  # noqa: E402
from cryptography.x509.oid import ExtendedKeyUsageOID, NameOID  # noqa: E402

PASTA = os.path.join(SAIDA, "h59-pki")
WWW = os.path.join(SAIDA, "h59-www")
HOLA = "¡Hola desde HTTPS! Página cifrada de prueba.\n"
random.seed(59)
GRANDE = bytes(random.getrandbits(8) for _ in range(300 * 1024))


# ------------------------------------------------------------ certificados

def nome(cn):
    return x509.Name([x509.NameAttribute(NameOID.COMMON_NAME, cn), x509.NameAttribute(NameOID.ORGANIZATION_NAME, "SaguiOS pruebas")])


def gravar(caminho, cert=None, chave=None, extra=()):
    with open(caminho, "wb") as f:
        if cert is not None:
            f.write(cert.public_bytes(serialization.Encoding.PEM))
        for c in extra:
            f.write(c.public_bytes(serialization.Encoding.PEM))
        if chave is not None:
            f.write(chave.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8,
                                        serialization.NoEncryption()))


def emitir(sujeito, chave_pub, emissor, chave_emissor, ca=False, caminho=None, nomes=(), ips=(),
           inicio=None, fim=None, hash_alg=None):
    agora = datetime.datetime.now(datetime.timezone.utc)
    b = (x509.CertificateBuilder().subject_name(nome(sujeito)).issuer_name(emissor).public_key(chave_pub)
         .serial_number(x509.random_serial_number())
         .not_valid_before(inicio or agora - datetime.timedelta(days=1))
         .not_valid_after(fim or agora + datetime.timedelta(days=30)))
    if ca:
        b = b.add_extension(x509.BasicConstraints(ca=True, path_length=caminho), critical=True)
        b = b.add_extension(x509.KeyUsage(digital_signature=False, content_commitment=False, key_encipherment=False,
                                          data_encipherment=False, key_agreement=False, key_cert_sign=True,
                                          crl_sign=True, encipher_only=False, decipher_only=False), critical=True)
    else:
        b = b.add_extension(x509.BasicConstraints(ca=False, path_length=None), critical=True)
        b = b.add_extension(x509.KeyUsage(digital_signature=True, content_commitment=False, key_encipherment=True,
                                          data_encipherment=False, key_agreement=False, key_cert_sign=False,
                                          crl_sign=False, encipher_only=False, decipher_only=False), critical=True)
        b = b.add_extension(x509.ExtendedKeyUsage([ExtendedKeyUsageOID.SERVER_AUTH]), critical=False)
        san = [x509.DNSName(n) for n in nomes] + [x509.IPAddress(ipaddress.ip_address(i)) for i in ips]
        b = b.add_extension(x509.SubjectAlternativeName(san), critical=False)
    return b.sign(chave_emissor, hash_alg or hashes.SHA256())


def gerar_pki():
    if os.path.exists(PASTA):
        shutil.rmtree(PASTA)
    os.makedirs(PASTA)
    ca_k = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    ca = emitir("CA de prueba SaguiOS", ca_k.public_key(), nome("CA de prueba SaguiOS"), ca_k, ca=True)
    gravar(os.path.join(PASTA, "ca.pem"), ca)
    # RSA directo de la CA
    k = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    c = emitir("servidor RSA", k.public_key(), ca.subject, ca_k, nomes=["sagui.prueba"], ips=["10.0.2.2"])
    gravar(os.path.join(PASTA, "rsa.pem"), c, k)
    # ECDSA: CA -> intermedia P-384 (firmada con RSA) -> servidor P-256 (firmado con ECDSA SHA-384)
    ki = ec.generate_private_key(ec.SECP384R1())
    inter = emitir("Intermedia ECDSA P-384", ki.public_key(), ca.subject, ca_k, ca=True, caminho=0)
    ke = ec.generate_private_key(ec.SECP256R1())
    ce = emitir("servidor ECDSA", ke.public_key(), inter.subject, ki, ips=["10.0.2.2"], hash_alg=hashes.SHA384())
    gravar(os.path.join(PASTA, "ecdsa.pem"), ce, ke, extra=[inter])
    # nombre que no coincide
    k2 = rsa.generate_private_key(public_exponent=65537, key_size=2048)
    c2 = emitir("otro servidor", k2.public_key(), ca.subject, ca_k, nomes=["otro.ejemplo"], ips=["192.0.2.7"])
    gravar(os.path.join(PASTA, "nombre.pem"), c2, k2)
    # caducado hace un día
    agora = datetime.datetime.now(datetime.timezone.utc)
    c3 = emitir("servidor caducado", k2.public_key(), ca.subject, ca_k, ips=["10.0.2.2"],
                inicio=agora - datetime.timedelta(days=60), fim=agora - datetime.timedelta(days=1))
    gravar(os.path.join(PASTA, "caducado.pem"), c3, k2)
    return ca


# ------------------------------------------------------------ servidores

class Tratador(http.server.SimpleHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def end_headers(self):
        self.send_header("Connection", "close")
        super().end_headers()

    def do_GET(self):
        self.close_connection = True
        if self.path == "/redir":
            self.send_response(302)
            self.send_header("Location", "/hola.txt")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        super().do_GET()


def servidor(certificado=None, versao=None):
    """Servidor HTTP (sin certificado) o HTTPS; versao = "1.2", "1.3" o None (las dos)."""
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), functools.partial(Tratador, directory=WWW))
    srv.daemon_threads = True
    if certificado:
        ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
        ctx.load_cert_chain(os.path.join(PASTA, certificado))
        ctx.set_alpn_protocols(["http/1.1"])
        if versao == "1.2":
            ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_2
        elif versao == "1.3":
            ctx.minimum_version = ctx.maximum_version = ssl.TLSVersion.TLSv1_3
        srv.socket = ctx.wrap_socket(srv.socket, server_side=True)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv.server_address[1]


def ms_tls(out):
    m = re.search(r"Conexión TLS: (TLSv1\.[23]), (\S+), (\S+) \((\d+) ms\)", out)
    return m


# ------------------------------------------------------------ partes

def parte_principal(p):
    base = "https://10.0.2.2"
    # D081: H59_RTC_LOCAL=Europe/Madrid liga com o RTC em hora local (como um
    # PC com Windows) e configura a zona antes; o h67 roda assim.
    local = os.environ.get("H59_RTC_LOCAL")
    extras = []
    if local:
        import zoneinfo
        extras = ["-rtc", "base=" + datetime.datetime.now(zoneinfo.ZoneInfo(local)).strftime("%Y-%m-%dT%H:%M:%S")]
    with Maquina("h59", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        if local:
            m.comando(f"zona {local}")
            out = m.comando("zona --rtc local")
            verificar("guarda la hora LOCAL" in out, f"RTC em hora local, zona {local}")
        out = m.comando("/testes/pruebacripto -r", tempo=300)
        r = re.search(r"pruebacripto: (\d+)/(\d+) vectores correctos", out)
        verificar(r and r.group(1) == r.group(2), f"pruebacripto: {r.group(0) if r else 'sin resultado'}")
        for linha in out.splitlines():
            if linha.startswith(("AES-128", "X25519", "aleatorio")):
                print("   ", linha)

        out = m.comando("data")
        verificar(str(datetime.datetime.now(datetime.timezone.utc).year) in out, "el reloj del sistema tiene la fecha correcta (data)")

        out = m.comando(f"curl {base}:{p['13rsa']}/hola.txt", tempo=60)
        verificar("emisor desconocido" in out and "-k" in out and "Hola desde HTTPS" not in out,
                  "sin la CA de prueba: \"emisor desconocido\" y la pista de -k")
        verificar("terminou com codigo 60" in out, "curl devuelve 60 (certificado no verificado)")

        m.comando(f"curl -s -o /tmp/ca.pem http://10.0.2.2:{p['http']}/ca.pem", tempo=30)
        out = m.comando("certificados añadir /tmp/ca.pem")
        verificar("Añadido \"CA de prueba SaguiOS\"" in out and "/etc/ssl/certs/" in out, "certificados añadir guarda la CA de prueba")
        verificar("no hay un disco persistente" in out, "sin disco persistente: avisa de que solo dura hasta apagar")
        out = m.comando("certificados añadir /tmp/ca.pem")
        verificar("ya está en el almacén" in out, "añadir dos veces: ya está")
        out = m.comando("certificados")
        n_ca = re.search(r"^\s*(\d+)\s+CA de prueba SaguiOS", out, re.M)
        verificar(n_ca and "ISRG Root X1" in out and "SHA-256" in out and "caduca" in out,
                  "certificados lista: raíces de fábrica y la CA de prueba, con caducidad y huella")
        n_ca = n_ca.group(1)

        tempos = {}
        for chave, versao, texto in (("13rsa", "TLSv1.3", "TLS 1.3 + RSA"), ("12rsa", "TLSv1.2", "TLS 1.2 + RSA"),
                                     ("13ec", "TLSv1.3", "TLS 1.3 + ECDSA (intermedia P-384)"),
                                     ("12ec", "TLSv1.2", "TLS 1.2 + ECDSA (intermedia P-384)")):
            ini = time.time()
            out = m.comando(f"curl -v {base}:{p[chave]}/hola.txt", tempo=120)
            seg = time.time() - ini
            t = ms_tls(out)
            verificar("Hola desde HTTPS" in out and t and t.group(1) == versao and "certificado válido (raíz: CA de prueba SaguiOS)" in out,
                      f"curl -v {texto}: {t.group(1) + ', ' + t.group(2) if t else '?'}")
            tempos[texto] = (int(t.group(4)) if t else -1, seg, t.group(2) if t else "?")
        verificar("sujeto:  CN=servidor RSA" in m.serial() and "nombres: sagui.prueba, 10.0.2.2" in m.serial(),
                  "curl -v muestra el sujeto y los nombres del certificado")

        out = m.comando(f"curl -L {base}:{p['13rsa']}/redir", tempo=60)
        verificar("redirigido (302)" in out and "Hola desde HTTPS" in out, "curl -L sigue una redirección dentro de HTTPS")

        out = m.comando(f"curl {base}:{p['nombre']}/hola.txt", tempo=60)
        verificar("nombre no coincide" in out and "Hola desde HTTPS" not in out, "certificado para otro nombre: \"nombre no coincide\"")
        out = m.comando(f"curl {base}:{p['caducado']}/hola.txt", tempo=60)
        verificar("certificado caducado" in out and "Hola desde HTTPS" not in out, "certificado vencido: \"certificado caducado\"")
        out = m.comando(f"curl -k -v {base}:{p['caducado']}/hola.txt", tempo=60)
        verificar("Hola desde HTTPS" in out and "aceptado por -k" in out, "curl -k acepta el certificado caducado y lo dice con -v")
        out = m.comando(f"curl --inseguro {base}:{p['nombre']}/hola.txt", tempo=60)
        verificar("Hola desde HTTPS" in out, "curl --inseguro acepta el nombre que no coincide")
        out = m.comando(f"curl --cacert /tmp/ca.pem {base}:{p['13ec']}/hola.txt", tempo=60)
        verificar("Hola desde HTTPS" in out, "curl --cacert con la CA de prueba")

        ini = time.time()
        out = m.comando(f"wget {base}:{p['13rsa']}/grande.bin", tempo=300)
        seg_wget = time.time() - ini
        verificar(f"{len(GRANDE)} bytes guardados en grande.bin" in out and "100%" in out,
                  f"wget por HTTPS: 300 KB guardados con barra de progreso ({seg_wget:.1f} s)")
        out = m.comando("sha256sum grande.bin", tempo=60)
        verificar(hashlib.sha256(GRANDE).hexdigest() in out, "sha256sum del archivo descargado = el del anfitrión")
        out = m.comando(f"wget -O /tmp/h.txt {base}:{p['12ec']}/hola.txt", tempo=60)
        verificar("guardados en /tmp/h.txt" in out, "wget -O por TLS 1.2 + ECDSA")
        out = m.comando(f"wget {base}:{p['13rsa']}/no-existe", tempo=60)
        verificar("404" in out, "wget: un 404 se informa")

        out = m.comando(f"certificados probar 10.0.2.2:{p['13ec']}", tempo=60)
        verificar("Certificado 1:" in out and "Intermedia ECDSA P-384" in out and "Resultado: certificado válido" in out,
                  "certificados probar: cadena (servidor + intermedia) y resultado")
        out = m.comando(f"certificados probar 10.0.2.2:{p['caducado']}", tempo=60)
        verificar("Resultado: NO VÁLIDO: certificado caducado" in out, "certificados probar: explica el fallo")
        out = m.comando(f"certificados ver {n_ca}")
        verificar("CA:      sí" in out and "RSA de 2048 bits" in out, "certificados ver N: detalles de la CA")
        out = m.comando("certificados ver /tmp/ca.pem")
        verificar("CA de prueba SaguiOS" in out, "certificados ver ARCHIVO")

        out = m.comando(f"certificados quitar {n_ca}")
        verificar("Quitado \"CA de prueba SaguiOS\"" in out, "certificados quitar N")
        out = m.comando(f"curl {base}:{p['13rsa']}/hola.txt", tempo=60)
        verificar("emisor desconocido" in out, "después de quitarla, la CA ya no es de confianza")
        out = m.comando("echo cipo listo")   # el sistema sigue vivo
        verificar("[SAGUI] EXCECAO" not in m.serial(), "sin excepciones del núcleo ni de los programas")

        # Internet real (opcional)
        out = m.comando("nslookup www.google.com", tempo=30)
        if "Dirección:" in out:
            out = m.comando("curl -I https://www.google.com/", tempo=120)
            if "HTTP/1.1" in out:
                verificar(True, "curl -I https://www.google.com/ (internet real, raíces de fábrica)")
            else:
                print("  AVISO: hay DNS pero no se pudo abrir https://www.google.com/ (¿cortafuegos?); se omite")
        else:
            print("  AVISO: sin internet en este entorno; se omite la prueba con un sitio real")
        m.captura("H59-tls")
    print("  Tiempos (reloj de SaguiOS para el handshake; segundos reales del comando entero en QEMU):")
    for k, (ms, seg, suite) in tempos.items():
        print(f"    {k:40s} handshake {ms:4d} ms   comando {seg:5.1f} s   {suite}")
    print(f"    wget 300 KB por HTTPS: {seg_wget:.1f} s")


def imagem_persistente():
    b = os.path.join(RAIZ, "build")
    destino = os.path.join(SAIDA, "h59-persistente.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", destino, "--vesa", "--console",
                    "--dados-mb", "16"], check=True, stdout=subprocess.DEVNULL)
    return destino


def parte_persistente(p):
    img = imagem_persistente()
    with Maquina("h59-pa", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        m.comando(f"curl -s -o /tmp/ca.pem http://10.0.2.2:{p['http']}/ca.pem", tempo=30)
        out = m.comando("certificados anadir /tmp/ca.pem")
        verificar("/home/.certificados/" in out and "se guardan en /home/.certificados" in out,
                  "con /home persistente, \"certificados anadir\" guarda en /home/.certificados")
        out = m.comando("certificados")
        n = re.search(r"^\s*(\d+)\s+ISRG Root X2", out, re.M)
        out = m.comando(f"certificados quitar {n.group(1)}") if n else ""
        verificar("Quitado \"ISRG Root X2\"" in out, "quitar una raíz de fábrica con disco persistente")
        m.digitar("reiniciar\n", 0.04)
        fim = time.time() + 60
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.3)
    with Maquina("h59-pb", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("certificados")
        verificar("CA de prueba SaguiOS" in out, "después de reiniciar, la CA añadida sigue en el almacén")
        verificar("ISRG Root X2" not in out and "ISRG Root X1" in out, "y la raíz de fábrica quitada sigue fuera")
        out = m.comando(f"curl https://10.0.2.2:{p['13rsa']}/hola.txt", tempo=60)
        verificar("Hola desde HTTPS" in out, "curl confía en la CA persistente tras el reinicio")


def parte_cpu_max(p):
    """Con "-cpu max" el QEMU emula AES-NI y RDRAND/RDSEED: se comprueban los
    otros caminos (aesni.c y las instrucciones del generador del núcleo)."""
    with Maquina("h59-max", "ide", imagem=imagem_console(), extras=["-cpu", "max"]) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar("aleatorio: rdrand=1" in m.serial(), "-cpu max: el núcleo detecta RDRAND y lo usa")
        out = m.comando("/testes/pruebacripto -r", tempo=300)
        r = re.search(r"pruebacripto: (\d+)/(\d+) vectores correctos", out)
        verificar(r and r.group(1) == r.group(2) and "instrucciones AES-NI" in out,
                  f"-cpu max: {r.group(0) if r else 'sin resultado'}, con AES-NI y también la versión en C")
        for linha in out.splitlines():
            if linha.startswith(("AES-128", "aleatorio")):
                print("   ", linha)
        out = m.comando(f"wget -k -O /tmp/g.bin https://10.0.2.2:{p['13rsa']}/grande.bin", tempo=120)
        verificar(f"{len(GRANDE)} bytes guardados" in out, "-cpu max: wget por HTTPS con AES-GCM en hardware")
        out = m.comando("sha256sum /tmp/g.bin", tempo=60)
        verificar(hashlib.sha256(GRANDE).hexdigest() in out, "-cpu max: SHA-256 del archivo correcto")


def parte_cipo(p):
    """Cipó por HTTPS: un ramdisk con la CA de prueba en /etc/ssl/certs abre una
    página cifrada; después, una dirección con certificado caducado da la página
    de error con el motivo."""
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(SAIDA, "h59-rootfs")
    if os.path.exists(raiz):
        shutil.rmtree(raiz)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    shutil.copy(os.path.join(PASTA, "ca.pem"), os.path.join(raiz, "etc", "ssl", "certs", "prueba.pem"))
    sgfs = os.path.join(SAIDA, "h59.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, "h59-cipo.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa",
                    "--teste", f"/apps/bin/cipo https://10.0.2.2:{p['13ec']}/segura.html",
                    *opcoes_apps()], check=True, stdout=subprocess.DEVNULL)
    with Maquina("h59-cipo", "ide", imagem=img) as m:
        m.esperar_serial("cipo: pronto", 90)
        s = m.serial()
        verificar('cipo: pronto "Página segura"' in s and "cipo: erro" not in s,
                  "Cipó abre una página HTTPS (TLS 1.3, cadena ECDSA con intermedia)")
        jan = re.search(r'janela (\d+) criada por \d+: "Cip.*?" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
        jx, jy = int(jan.group(4)), int(jan.group(5))
        m.captura("H59-cipo-https")
        m.clique(jx + 2 + 400, jy + 24 + 45)   # barra de dirección (D373: la barra va bajo las pestañas)
        for _ in range(60):
            m.tecla("backspace", 0.02)
        m.digitar(f"https://10.0.2.2:{p['caducado']}/hola.txt\n", 0.04)
        m.esperar_serial("cipo: erro", 60)
        verificar("cipo: erro: Conexión no segura 10.0.2.2: certificado caducado" in m.serial(),
                  "Cipó: certificado caducado = página de error \"Conexión no segura\" con el motivo")


def main():
    gerar_pki()
    os.makedirs(WWW, exist_ok=True)
    with open(os.path.join(WWW, "hola.txt"), "w", encoding="utf-8") as f:
        f.write(HOLA)
    with open(os.path.join(WWW, "segura.html"), "w", encoding="utf-8") as f:
        f.write("<html><head><title>Página segura</title></head><body><h1>Cifrado</h1><p>Servida por TLS.</p></body></html>")
    with open(os.path.join(WWW, "grande.bin"), "wb") as f:
        f.write(GRANDE)
    shutil.copy(os.path.join(PASTA, "ca.pem"), os.path.join(WWW, "ca.pem"))
    p = {
        "http": servidor(),
        "13rsa": servidor("rsa.pem", "1.3"),
        "12rsa": servidor("rsa.pem", "1.2"),
        "13ec": servidor("ecdsa.pem", "1.3"),
        "12ec": servidor("ecdsa.pem", "1.2"),
        "nombre": servidor("nombre.pem"),
        "caducado": servidor("caducado.pem"),
    }
    partes = sys.argv[1:] or ["principal", "cpu_max", "cipo", "persistente"]
    if "principal" in partes:
        parte_principal(p)
    if "cpu_max" in partes:
        parte_cpu_max(p)
    if "cipo" in partes:
        parte_cipo(p)
    if "persistente" in partes:
        parte_persistente(p)
    return 0


if __name__ == "__main__":
    sys.exit(main())
