#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H44 - red: drivers e1000 y RTL8139, DHCP, ping, DNS, TCP, curl, nc (D046).

Criterio:
  1. (e1000, escritorio con el Terminal) el arranque detecta la e1000 y el
     DHCP de QEMU (slirp) da 10.0.2.15; ifconfig lo muestra; ping -c 3
     10.0.2.2 responde; nslookup resuelve contra un DNS de prueba del
     anfitrión (y contra el DNS real si hay internet; si no, aviso); curl
     descarga de un servidor HTTP del anfitrión una página pequeña, un
     archivo de 200 KB (CRC-32 y tamaño iguales), una respuesta chunked y una
     redirección (-L), muestra 404 y -I, https a un puerto cerrado; wget guarda en
     archivo; nc -l en la VM recibe una conexión del anfitrión (hostfwd) y
     nc conecta a un servidor de eco; Ctrl+C corta un curl bloqueado y la
     conexión se cierra.
  2. (RTL8139, consola) detección, DHCP, ping y una descarga grande.
  3. (e1000 conectada a un inyector de tramas, IP estática con ifconfig)
     3000 tramas basura (cabeceras rotas, longitudes falsas, TCP/UDP/ICMP/
     DHCP/ARP malformados) no cuelgan el sistema: después responde a un ARP
     y a un ping válidos.
"""
import functools
import http.server
import os
import random
import re
import socket
import struct
import sys
import threading
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, FalhaTeste  # noqa: E402

random.seed(44)
GRANDE = bytes(random.getrandbits(8) for _ in range(200 * 1024))
PEQUENA = "<html><head><title>Hola</title></head><body><p>Página pequeña de prueba: ¡funciona!</p></body></html>\n"
TROZOS = [b"Primer trozo. ", b"x" * 5000, b" Fin del chunked.\n"]


def puerto_libre(tipo=socket.SOCK_STREAM):
    s = socket.socket(socket.AF_INET, tipo)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


class Pedido(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def responder(self, codigo, corpo, tipo="text/html; charset=utf-8", extra=()):
        self.send_response(codigo)
        self.send_header("Content-Type", tipo)
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        for k, v in extra:
            self.send_header(k, v)
        self.end_headers()
        if self.command != "HEAD":
            self.wfile.write(corpo)
        self.close_connection = True

    def do_HEAD(self):
        self.do_GET()

    def do_GET(self):
        if self.path == "/pequena.html":
            self.responder(200, PEQUENA.encode())
        elif self.path == "/grande.bin":
            self.responder(200, GRANDE, "application/octet-stream")
        elif self.path == "/trozos":
            self.send_response(200)
            self.send_header("Content-Type", "text/plain")
            self.send_header("Transfer-Encoding", "chunked")
            self.send_header("Connection", "close")
            self.end_headers()
            for t in TROZOS:
                self.wfile.write(b"%x\r\n" % len(t) + t + b"\r\n")
                self.wfile.flush()
                time.sleep(0.05)
            self.wfile.write(b"0\r\n\r\n")
            self.close_connection = True
        elif self.path == "/redir":
            self.responder(302, b"movido\n", extra=(("Location", "/pequena.html"),))
        elif self.path == "/lento":
            self.send_response(200)
            self.send_header("Content-Length", "100000")
            self.end_headers()
            self.wfile.write(b"empieza...")
            self.wfile.flush()
            Pedido.lento_conectado = True
            # Espera hasta que el otro lado cierre (Ctrl+C en la VM).
            self.connection.settimeout(30)
            try:
                while self.connection.recv(100):
                    pass
            except OSError:
                pass
            Pedido.lento_cerrado = True
            self.close_connection = True
        else:
            self.responder(404, b"no existe\n", "text/plain")


Pedido.lento_conectado = False
Pedido.lento_cerrado = False


def servidor_dns(porta, parar):
    """DNS de prueba: sagui.prueba -> 10.1.2.3; el resto, NXDOMAIN."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", porta))
    s.settimeout(0.5)
    while not parar.is_set():
        try:
            q, de = s.recvfrom(512)
        except socket.timeout:
            continue
        if len(q) < 12:
            continue
        i, partes = 12, []
        while i < len(q) and q[i]:
            partes.append(q[i + 1:i + 1 + q[i]].decode("latin-1"))
            i += 1 + q[i]
        pergunta = q[12:i + 5]
        nome = ".".join(partes).lower()
        if nome == "sagui.prueba":
            r = q[:2] + b"\x81\x80" + struct.pack(">HHHH", 1, 1, 0, 0) + pergunta
            r += b"\xc0\x0c" + struct.pack(">HHIH", 1, 1, 60, 4) + bytes([10, 1, 2, 3])
        else:
            r = q[:2] + b"\x81\x83" + struct.pack(">HHHH", 1, 0, 0, 0) + pergunta
        s.sendto(r, de)
    s.close()


def servidor_eco(porta, parar):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", porta))
    s.listen(4)
    s.settimeout(0.5)
    while not parar.is_set():
        try:
            c, _ = s.accept()
        except socket.timeout:
            continue
        c.settimeout(10)
        dados = b""
        try:
            while True:
                b = c.recv(4096)
                if not b:
                    break
                dados += b
                c.sendall(b.upper())
        except OSError:
            pass
        c.close()
    s.close()


def boot_e1000(porta_http, porta_dns, porta_eco):
    porta_fwd = puerto_libre()
    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{porta_fwd}-:8080"]
    with Maquina("h44-e1000", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[E1000] 00:03.0 (8086:100e)" in s, "detecta la Intel e1000 (8086:100E) en el PCI")
        verificar("[ ok ] Red: e1000 52:54:00:12:34:56, 10.0.2.15 (DHCP)" in s,
                  "línea de arranque: e1000, MAC y 10.0.2.15 por DHCP")

        out = m.comando("ifconfig")
        verificar("IPv4 10.0.2.15/24" in out and "puerta de enlace 10.0.2.2" in out and "DNS 10.0.2.3" in out,
                  "ifconfig: IP, máscara, puerta de enlace y DNS del DHCP")
        verificar("MAC 52:54:00:12:34:56" in out and "enlace: conectado" in out, "ifconfig: MAC y enlace")

        out = m.comando("ping -c 3 10.0.2.2", tempo=30)
        verificar("3 paquetes enviados, 3 recibidos, 0% perdidos" in out, "ping -c 3 10.0.2.2: 3 respuestas")
        verificar(re.search(r"icmp_seq=1 ttl=\d+ tiempo=\d+\.\d{3} ms", out), "ping muestra ttl y tiempo")

        out = m.comando(f"nslookup sagui.prueba 10.0.2.2:{porta_dns}", tempo=30)
        verificar("Dirección: 10.1.2.3" in out, "nslookup contra el DNS de prueba del anfitrión")
        out = m.comando(f"nslookup no.existe.prueba 10.0.2.2:{porta_dns}", tempo=30)
        verificar("el nombre no existe" in out, "nslookup: nombre inexistente (NXDOMAIN)")
        out = m.comando("nslookup example.com", tempo=30)
        if "Dirección:" in out:
            verificar(True, "nslookup example.com por el DNS de slirp (hay internet)")
        else:
            print("  AVISO: sin DNS real en este entorno (nslookup example.com no resolvió); se omite")

        base = f"http://10.0.2.2:{porta_http}"
        out = m.comando(f"curl {base}/pequena.html", tempo=30)
        verificar("Página pequeña de prueba: ¡funciona!" in out, "curl: página pequeña (UTF-8 convertida para la pantalla)")

        out = m.comando(f"curl -v -o /grande.bin {base}/grande.bin", tempo=90)
        crc = "%08x" % zlib.crc32(GRANDE)
        verificar(f"* {len(GRANDE)} bytes en" in out and f"CRC32 {crc}" in out,
                  f"curl -o: 200 KB descargados con el CRC-32 correcto ({crc})")
        verificar("> GET /grande.bin HTTP/1.1" in out and "< HTTP/1.1 200 OK" in out, "curl -v muestra pedido y respuesta")
        out = m.comando("wc /grande.bin")
        verificar(f"{len(GRANDE)} letras" in out, "wc lee el archivo guardado con el tamaño correcto")

        out = m.comando(f"curl {base}/trozos", tempo=30)
        verificar("Primer trozo. " + "x" * 30 in out and "x Fin del chunked." in out, "curl: respuesta chunked")
        m.comando(f"curl -o /trozos.txt {base}/trozos", tempo=30)
        out = m.comando("wc /trozos.txt")
        verificar(f"{sum(len(t) for t in TROZOS)} letras" in out, "chunked guardado con el tamaño exacto")

        out = m.comando(f"curl -L {base}/redir", tempo=30)
        verificar("redirigido (302)" in out and "¡funciona!" in out, "curl -L sigue la redirección 302")
        out = m.comando(f"curl {base}/redir", tempo=30)
        verificar("movido" in out and "¡funciona!" not in out, "sin -L no sigue la redirección")
        out = m.comando(f"curl {base}/no-hay", tempo=30)
        verificar("el servidor respondió 404" in out, "curl informa del 404")
        out = m.comando(f"curl -I {base}/pequena.html", tempo=30)
        verificar("HTTP/1.1 200 OK" in out and "Content-Length:" in out and "<html>" not in out, "curl -I solo cabecera")
        out = m.comando("curl https://10.0.2.2:1/", tempo=30)
        verificar("conexión rechazada" in out, "curl https a un puerto cerrado: error claro (HTTPS en h59)")
        out = m.comando(f"wget {base}/pequena.html", tempo=30)
        verificar("guardados en pequena.html" in out, "wget guarda en archivo")
        out = m.comando("cat pequena.html")
        verificar("¡funciona!" in out, "el archivo de wget tiene la página")

        # nc -l en la VM, conexión desde el anfitrión por hostfwd.
        n = m.serial().count("sagui:")
        m.digitar("echo respuesta del sagui | nc -l 8080\n", 0.04)
        m.esperar_serial("esperando conexión en el puerto 8080", 20)
        time.sleep(0.5)
        c = socket.create_connection(("127.0.0.1", porta_fwd), timeout=20)
        c.sendall("hola desde el anfitrión\n".encode())
        c.shutdown(socket.SHUT_WR)
        recibido = b""
        while True:
            b = c.recv(4096)
            if not b:
                break
            recibido += b
        c.close()
        fim = time.time() + 20
        while m.serial().count("sagui:") <= n and time.time() < fim:
            time.sleep(0.2)
        s = m.serial()
        verificar(b"respuesta del sagui" in recibido, "servidor TCP en la VM (nc -l): el anfitrión recibe la respuesta")
        verificar("nc: conexión de 10.0.2.2:" in s and "hola desde el anfitrión" in s, "nc -l muestra lo que llegó")

        out = m.comando(f"echo hola eco | nc 10.0.2.2 {porta_eco}", tempo=30)
        verificar("HOLA ECO" in out, "nc cliente: servidor de eco del anfitrión")
        out = m.comando("nc 10.0.2.2 1", tempo=30)
        verificar("conexión rechazada" in out, "nc a un puerto cerrado: conexión rechazada")

        # Ctrl+C con una conexión bloqueada.
        m.digitar(f"curl {base}/lento\n", 0.04)
        fim = time.time() + 20
        while not Pedido.lento_conectado and time.time() < fim:
            time.sleep(0.2)
        verificar(Pedido.lento_conectado, "curl quedó esperando un servidor lento")
        time.sleep(1)
        m.tecla("ctrl-c")
        m.esperar_serial("(curl) terminou com codigo 130", 15)
        fim = time.time() + 15
        while not Pedido.lento_cerrado and time.time() < fim:
            time.sleep(0.2)
        verificar(Pedido.lento_cerrado, "Ctrl+C termina curl y la conexión se cierra (el servidor lo nota)")
        out = m.comando("ifconfig -a")
        verificar("ESTABLECIDA" not in out, "ifconfig -a: no quedan conexiones establecidas")
        verificar("10.0.2.2" in out.split("Caché ARP:")[1], "ifconfig -a muestra la caché ARP")
        m.captura("H44-red")


def boot_rtl8139(porta_http):
    with Maquina("h44-rtl8139", "ide", imagem=imagem_console(), extras=["-nic", "user,model=rtl8139"]) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[RTL8139] 00:03.0" in s, "detecta la Realtek RTL8139")
        verificar("[ ok ] Red: rtl8139 52:54:00:12:34:56, 10.0.2.15 (DHCP)" in s, "RTL8139: 10.0.2.15 por DHCP")
        tela = "\n".join(m.tela_texto())
        verificar("[ ok ] Red: rtl8139" in tela, "la línea de la red cabe en la pantalla de arranque")
        out = m.comando("ping -c 3 10.0.2.2", tempo=30)
        verificar("3 recibidos" in out, "RTL8139: ping")
        out = m.comando(f"curl -v -o /g.bin http://10.0.2.2:{porta_http}/grande.bin", tempo=90)
        verificar("CRC32 %08x" % zlib.crc32(GRANDE) in out, "RTL8139: descarga de 200 KB íntegra")


def tramas_basura(mac_vm):
    """Tramas Ethernet malformadas, hacia la VM o de difusión."""
    mac_vm = bytes(mac_vm)
    meu = bytes([0x52, 0x54, 0x00, 0xAA, 0xBB, 0xCC])
    ip_vm = bytes([10, 0, 2, 15])
    ip_eu = bytes([10, 0, 2, 99])

    def soma(b):
        if len(b) % 2:
            b += b"\0"
        s = sum(struct.unpack("!%dH" % (len(b) // 2), b))
        while s >> 16:
            s = (s & 0xFFFF) + (s >> 16)
        return (~s) & 0xFFFF

    def ip(proto, carga, total=None, ihl=5, frag=0, csum=True):
        t = total if total is not None else 20 + len(carga)
        h = struct.pack("!BBHHHBBH4s4s", 0x40 | ihl, 0, t & 0xFFFF, random.getrandbits(16), frag, 64, proto, 0, ip_eu, ip_vm)
        if csum:
            h = h[:10] + struct.pack("!H", soma(h)) + h[12:]
        return h + b"\0" * max(0, ihl * 4 - 20) + carga

    for i in range(3000):
        destino = mac_vm if i % 3 else b"\xff" * 6
        tipo = random.choice([0x0800] * 6 + [0x0806, 0x86DD, random.getrandbits(16)])
        r = random.random()
        rnd = lambda n: bytes(random.getrandbits(8) for _ in range(n))  # noqa: E731
        if tipo == 0x0806:
            corpo = rnd(random.choice([0, 5, 27, 28, 40]))
        elif tipo != 0x0800:
            corpo = rnd(random.randint(0, 100))
        elif r < 0.15:
            corpo = rnd(random.randint(0, 60))                                      # lixo puro
        elif r < 0.3:
            corpo = ip(random.choice([1, 6, 17]), rnd(random.randint(0, 80)), total=random.randint(0, 3000))
        elif r < 0.4:
            corpo = ip(random.choice([1, 6, 17]), rnd(40), ihl=random.randint(0, 15))
        elif r < 0.5:
            corpo = ip(17, rnd(40), frag=random.getrandbits(16))
        elif r < 0.65:   # TCP com deslocamento e flags aleatórios
            t = struct.pack("!HHIIBBHHH", random.getrandbits(16), random.choice([8080, 80, random.getrandbits(16)]),
                            random.getrandbits(32), random.getrandbits(32), random.getrandbits(8), random.getrandbits(8),
                            random.getrandbits(16), 0, 0) + rnd(random.randint(0, 60))
            corpo = ip(6, t)
        elif r < 0.8:    # UDP com comprimento falso (inclusive para o DHCP e o DNS)
            u = struct.pack("!HHHH", random.choice([67, 53, random.getrandbits(16)]),
                            random.choice([68, 50001, random.getrandbits(16)]), random.randint(0, 2000), 0)
            corpo = ip(17, u + rnd(random.randint(0, 300)))
        elif r < 0.9:    # ICMP com tamanho e tipo aleatórios
            c = bytes([random.choice([0, 8, 3, 11, random.getrandbits(8)]), 0, 0, 0]) + rnd(random.randint(0, 200))
            c = c[:2] + struct.pack("!H", soma(c)) + c[4:]
            corpo = ip(1, c)
        else:            # resposta DHCP com opções quebradas
            b = bytearray(rnd(240 + random.randint(0, 60)))
            b[0] = 2
            b[236:240] = b"\x63\x82\x53\x63"
            b[28:34] = mac_vm
            u = struct.pack("!HHHH", 67, 68, 8 + len(b), 0)
            corpo = ip(17, u + bytes(b))
        yield destino + meu + struct.pack("!H", tipo) + corpo


def boot_basura():
    p_vm, p_eu = puerto_libre(socket.SOCK_DGRAM), puerto_libre(socket.SOCK_DGRAM)
    # La e1000 de la VM va conectada a un socket UDP por el que este test
    # inyecta tramas Ethernet crudas (y recibe lo que la VM manda). No se
    # junta con el slirp en un "hub": el propio slirp de QEMU 8.2 se cae
    # (SIGSEGV) con parte de esta basura.
    extras = ["-netdev", f"socket,id=s0,udp=127.0.0.1:{p_eu},localaddr=127.0.0.1:{p_vm}", "-device", "e1000,netdev=s0"]
    mac_vm = bytes([0x52, 0x54, 0x00, 0x12, 0x34, 0x56])
    meu = bytes([0x52, 0x54, 0x00, 0xAA, 0xBB, 0xCC])
    with Maquina("h44-basura", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar("pidiendo IP por DHCP" in m.serial(), "sin servidor DHCP el arranque no se bloquea (sigue pidiendo)")
        out = m.comando("ifconfig eth0 10.0.2.15/24 gw 10.0.2.2")
        verificar("IPv4 10.0.2.15/24" in out and "(estática)" in out, "ifconfig eth0 10.0.2.15/24 gw 10.0.2.2: dirección estática")
        inj = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        inj.bind(("127.0.0.1", p_eu))
        n = 0
        for q in tramas_basura(mac_vm):
            inj.sendto(q, ("127.0.0.1", p_vm))
            n += 1
            if n % 100 == 0:
                time.sleep(0.02)
        time.sleep(1)
        # Después de la basura, un ARP y un ping válidos tienen que tener respuesta.
        inj.settimeout(0.2)
        try:
            while True:
                inj.recv(2048)   # descarta lo que la VM haya mandado hasta ahora
        except socket.timeout:
            pass
        arp = mac_vm + meu + b"\x08\x06" + struct.pack("!HHBBH6s4s6s4s", 1, 0x0800, 6, 4, 1, meu, bytes([10, 0, 2, 99]),
                                                          b"\0" * 6, bytes([10, 0, 2, 15]))
        inj.sendto(arp, ("127.0.0.1", p_vm))
        eco = b"\x08\x00\x00\x00\x12\x34\x00\x01" + b"sagui-vivo" * 4
        s = 0
        for i in range(0, len(eco), 2):
            s += (eco[i] << 8) + (eco[i + 1] if i + 1 < len(eco) else 0)
        while s >> 16:
            s = (s & 0xFFFF) + (s >> 16)
        eco = eco[:2] + struct.pack("!H", ~s & 0xFFFF) + eco[4:]
        h = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + len(eco), 1, 0, 64, 1, 0, bytes([10, 0, 2, 99]), bytes([10, 0, 2, 15]))
        s = sum(struct.unpack("!10H", h))
        while s >> 16:
            s = (s & 0xFFFF) + (s >> 16)
        h = h[:10] + struct.pack("!H", ~s & 0xFFFF) + h[12:]
        inj.sendto(mac_vm + meu + b"\x08\x00" + h + eco, ("127.0.0.1", p_vm))
        resposta = False
        fim = time.time() + 5
        while time.time() < fim and not resposta:
            try:
                q = inj.recv(2048)
            except socket.timeout:
                continue
            resposta = q[12:14] == b"\x08\x00" and q[23] == 1 and q[34] == 0 and b"sagui-vivo" in q
        inj.close()
        verificar(resposta, f"después de {n} tramas basura el kernel responde a un ping válido")
        out = m.comando("ifconfig")
        rx = re.search(r"RX: (\d+) paquetes", out)
        verificar(rx and int(rx.group(1)) >= n, f"ifconfig cuenta las tramas recibidas ({rx.group(1) if rx else '?'})")
        out = m.comando("echo sigo vivo")
        verificar("sigo vivo" in out and "[SAGUI] EXCECAO" not in m.serial(), "el sistema sigue vivo, sin excepciones")


def main():
    parar = threading.Event()
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Pedido)
    srv.daemon_threads = True
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    porta_http = srv.server_address[1]
    porta_dns = puerto_libre(socket.SOCK_DGRAM)
    threading.Thread(target=servidor_dns, args=(porta_dns, parar), daemon=True).start()
    porta_eco = puerto_libre()
    threading.Thread(target=servidor_eco, args=(porta_eco, parar), daemon=True).start()
    try:
        partes = sys.argv[1:] or ["e1000", "rtl8139", "basura"]   # para depurar: h44_red.py basura
        if "e1000" in partes:
            boot_e1000(porta_http, porta_dns, porta_eco)
        if "rtl8139" in partes:
            boot_rtl8139(porta_http)
        if "basura" in partes:
            boot_basura()
    finally:
        parar.set()
        srv.shutdown()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
