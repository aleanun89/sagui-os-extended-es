#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H445 - varias interfaces de red a la vez, con rutas por métrica (D445).

QEMU con dos tarjetas en dos redes de usuario distintas: una e1000 (eth0, 10.0.2.0/24) y una
RTL8139 (eth1, 10.0.3.0/24), con un volcado pcap de cada cable (filter-dump) para saber por cuál
salió cada paquete.

Criterio:
  1. fuentes: la pila tiene un estado por interfaz (DHCP, ARP), la ruta por defecto por métrica
     y la ABI no cambia de tamaño;
  2. arrancan las dos tarjetas (eth0 e1000, eth1 rtl8139) y cada una toma su dirección por DHCP
     (10.0.2.15 y 10.0.3.15); ifconfig enseña las dos y la ruta por defecto (eth0, cable con
     métrica 100 e índice menor);
  3. ping a la puerta de enlace de cada red responde y cada eco sale por su cable;
  4. un destino fuera de las dos redes sale por la ruta por defecto (eth0);
  5. «ifconfig eth1 metric 50»: la ruta por defecto pasa a eth1 (y el DNS del sistema, al de esa red);
     al volver la métrica a 200, vuelve a eth0;
  6. caída del enlace de eth0 (set_link off en QEMU): la ruta por defecto pasa a eth1 sola; al volver
     el enlace, vuelve a eth0;
  7. «ifconfig eth0 down»: lo mismo; «up» la devuelve;
  8. una descarga HTTP por cada red (curl a 10.0.2.2 y a 10.0.3.2) llega entera.
"""
import http.server
import os
import re
import socket
import struct
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, RAIZ, SAIDA  # noqa: E402

CORPO = (b"multi-nic " * 2000)[:16384]


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


class Pedido(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(CORPO)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(CORPO)
        self.close_connection = True


def ecos(pcap):
    """Destinos de los ICMP echo request (tipo 8) que salieron por ese cable."""
    if not os.path.exists(pcap):
        return []
    with open(pcap, "rb") as h:
        d = h.read()
    out, i = [], 24
    while i + 16 <= len(d):
        incl = struct.unpack("<I", d[i + 8:i + 12])[0]
        q = d[i + 16:i + 16 + incl]
        i += 16 + incl
        if len(q) < 14 + 20 + 8 or q[12:14] != b"\x08\x00":
            continue
        h_ip = q[14:]
        cab = (h_ip[0] & 15) * 4
        if h_ip[9] == 1 and len(h_ip) > cab and h_ip[cab] == 8:
            out.append(".".join(str(b) for b in h_ip[16:20]))
    return out


def parte_fuentes():
    r = ler("kernel/rede/rede.c")
    verificar("static iface_t ifs[REDE_IFS];" in r and "static iface_t *rota(" in r and "static iface_t *if_padrao(" in r,
              "rede.c: un estado por interfaz y la elección de la ruta")
    verificar("uint32_t quando; iface_t *f; } arp[ARP_MAX];" in r and "static void arp_esquecer(iface_t *f)" in r,
              "rede.c: las entradas ARP son de una interfaz")
    verificar("static void dhcp_tique(iface_t *f)" in r and "static void dhcp_chegou(iface_t *f," in r,
              "rede.c: un DHCP por interfaz")
    verificar("METRICA_CABO 100" in r and "METRICA_WIFI 600" in r, "métricas por defecto: cable 100, Wi-Fi 600")
    abi = ler("kernel/inc/abi.h")
    verificar("char           diagnostico[3064];" in abi and "unsigned int   metrica;" in abi,
              "info_rede_t: los campos nuevos salen del final de diagnostico (mismo tamaño)")
    verificar("#define REDE_METRICA      11" in abi, "abi.h: REDE_METRICA")
    t = ler("kernel/rede/tcp.c")
    verificar("ip_local" in t and "ip_enviar_de(origem" in t, "tcp.c: la conexión guarda su dirección local")
    # sin bloqueos ni esperas en la pila: nada de ceder la CPU fuera de la tarea (cabecera de wifi.c)
    trozo = r[r.index("static iface_t *rota("):r.index("uint32_t rede_origem_para(")]
    verificar("ceder" not in trozo and "bloquear" not in trozo, "la elección de la ruta no cede la CPU")


def esperar(m, patron, tempo=60, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        s = m.serial()[desde:].replace("\r", "")
        if re.search(patron, s):
            return s
        time.sleep(0.5)
    verificar(False, f"esperaba {patron!r} en el registro")


def parte_qemu():
    porta = puerto_libre()
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", porta), Pedido)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    os.makedirs(SAIDA, exist_ok=True)
    p0 = os.path.join(SAIDA, "h445-eth0.pcap")
    p1 = os.path.join(SAIDA, "h445-eth1.pcap")
    for p in (p0, p1):
        if os.path.exists(p):
            os.remove(p)
    extras = ["-netdev", "user,id=n0,net=10.0.2.0/24", "-device", "e1000,netdev=n0,mac=52:54:00:12:34:56",
              "-object", f"filter-dump,id=d0,netdev=n0,file={p0}",
              "-netdev", "user,id=n1,net=10.0.3.0/24", "-device", "rtl8139,netdev=n1,mac=52:54:00:12:34:57",
              "-object", f"filter-dump,id=d1,netdev=n1,file={p1}"]
    try:
        with Maquina("h445-multi", "ide", imagem=imagem_console(), extras=extras) as m:
            m.esperar_serial("sagui:/$", 180)
            s = esperar(m, r"\[REDE\] eth1: DHCP: ip 10\.0\.3\.15", 60)
            verificar("[REDE] placa e1000" in s and re.search(r"\[REDE\] placa rtl8139 \([^)]*\), [^\n]*, eth1", s),
                      "arrancan las dos tarjetas: e1000 como eth0 y rtl8139 como eth1")
            verificar("[REDE] eth0: DHCP: ip 10.0.2.15" in s, "eth0 toma 10.0.2.15 por DHCP")
            verificar("[REDE] eth1: DHCP: ip 10.0.3.15" in s, "eth1 toma 10.0.3.15 por DHCP")

            out = m.comando("ifconfig", tempo=30)
            verificar("eth0: " in out and "eth1: " in out, "ifconfig enseña eth0 y eth1")
            verificar("IPv4 10.0.2.15/24" in out and "IPv4 10.0.3.15/24" in out, "ifconfig: las dos direcciones")
            verificar("Ruta por defecto: eth0 por 10.0.2.2 (métrica 100)" in out, "ruta por defecto: eth0 (cable, índice menor)")
            out = m.comando("ifconfig route", tempo=30)
            verificar(re.search(r"10\.0\.2\.0/24 +- +eth0", out) and re.search(r"10\.0\.3\.0/24 +- +eth1", out),
                      "ifconfig route: la red local de cada interfaz")
            verificar(re.search(r"por defecto +10\.0\.2\.2 +eth0 +100 +<- en uso", out), "ifconfig route: la por defecto, eth0")
            verificar("DNS del sistema: 10.0.2.3 (eth0)" in out, "el DNS del sistema es el de la interfaz por defecto")

            out = m.comando("ping -c 2 10.0.2.2", tempo=30)
            verificar("2 recibidos" in out, "ping a la puerta de eth0")
            out = m.comando("ping -c 2 10.0.3.2", tempo=30)
            verificar("2 recibidos" in out, "ping a la puerta de eth1")
            time.sleep(1)
            e0, e1 = ecos(p0), ecos(p1)
            verificar("10.0.2.2" in e0 and "10.0.2.2" not in e1, f"el eco a 10.0.2.2 sale por eth0 ({e0} / {e1})")
            verificar("10.0.3.2" in e1 and "10.0.3.2" not in e0, f"el eco a 10.0.3.2 sale por eth1 ({e0} / {e1})")

            def por_onde(dest):
                m.comando(f"ping -c 1 -W 500 {dest}", tempo=30)
                time.sleep(1)
                a, b = dest in ecos(p0), dest in ecos(p1)
                return "eth0" if a and not b else "eth1" if b and not a else f"?({a},{b})"

            verificar(por_onde("10.0.50.1") == "eth0", "fuera de las dos redes: por la ruta por defecto (eth0)")

            marca = len(m.serial())
            out = m.comando("ifconfig eth1 metric 50", tempo=30)
            verificar(re.search(r"por defecto +10\.0\.3\.2 +eth1 +50 +<- en uso", out), "métrica 50: la ruta por defecto es eth1")
            esperar(m, r"\[REDE\] ruta por defecto: eth1", 10, marca)
            out = m.comando("ifconfig route", tempo=30)
            verificar("DNS del sistema: 10.0.3.3 (eth1)" in out, "con eth1 por defecto, el DNS del sistema es el de eth1")
            verificar(por_onde("10.0.50.2") == "eth1", "métrica 50: fuera de las redes sale por eth1")
            m.comando("ifconfig eth1 metric 200", tempo=30)
            verificar(por_onde("10.0.50.3") == "eth0", "métrica 200: vuelve a eth0")

            marca = len(m.serial())
            m.monitor("set_link n0 off")
            esperar(m, r"\[REDE\] eth0: enlace caído", 10, marca)
            esperar(m, r"\[REDE\] ruta por defecto: eth1", 10, marca)
            verificar(por_onde("10.0.50.4") == "eth1", "eth0 sin enlace: la ruta por defecto pasa a eth1")
            out = m.comando("ping -c 1 10.0.3.2", tempo=30)
            verificar("1 recibidos" in out, "con eth0 caída, eth1 sigue respondiendo")
            marca = len(m.serial())
            m.monitor("set_link n0 on")
            esperar(m, r"\[REDE\] ruta por defecto: eth0", 10, marca)
            verificar(por_onde("10.0.50.5") == "eth0", "vuelve el enlace de eth0: la ruta por defecto vuelve")

            marca = len(m.serial())
            m.comando("ifconfig eth0 down", tempo=30)
            esperar(m, r"\[REDE\] ruta por defecto: eth1", 10, marca)
            verificar(por_onde("10.0.50.6") == "eth1", "eth0 desactivada: la ruta por defecto pasa a eth1")
            m.comando("ifconfig eth0 up", tempo=30)
            verificar(por_onde("10.0.50.7") == "eth0", "eth0 activada: vuelve")

            for red in ("10.0.2.2", "10.0.3.2"):
                m.comando(f"curl -o /d.bin http://{red}:{porta}/x", tempo=60)
                out = m.comando("wc /d.bin", tempo=30)
                verificar(f"{len(CORPO)} letras" in out, f"descarga HTTP por {red} entera ({len(CORPO)} bytes)")
                m.comando("rm /d.bin", tempo=30)
            verificar("[SAGUI] EXCECAO" not in m.serial(), "sin excepciones")
    finally:
        srv.shutdown()


def main():
    partes = sys.argv[1:] or ["fuentes", "qemu"]
    if "fuentes" in partes:
        parte_fuentes()
    if "qemu" in partes:
        parte_qemu()
    print("H445 OK")


if __name__ == "__main__":
    main()
