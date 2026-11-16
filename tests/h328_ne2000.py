#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H328 (D329) - driver NE2000 (DP8390): PCI (RTL8029, 10ec:8029) e ISA (0x300, IRQ configurable).

QEMU emula las dos (-device ne2k_pci / -device ne2k_isa). En cada modelo: el kernel detecta la
tarjeta, DHCP da 10.0.2.15, ping al anfitrión y descarga HTTP íntegra de 200 KB (CRC32) desde un
servidor del anfitrión. Casos ISA:
  - (con --ne2k-isa) IRQ autodetectada (IRR del 8259, sin configuración), con la tarjeta en 0x300 IRQ 9;
  - IRQ forzada por la cabecera (--ne2k-irq 10) y puerto 0x280 (sondeo de las cuatro bases);
  - sondeo apagado (por defecto, sin --ne2k-isa): no hay tarjeta, sin tocar puertos;
  - sin tarjeta NE2000: el sondeo ISA no encuentra nada y el sistema arranca igual.

Uso: h328_ne2000.py [pci] [pciioapic] [isa] [isa280] [sinisa] [ninguna]
"""
import http.server
import os
import random
import sys
import threading
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, FalhaTeste  # noqa: E402

random.seed(328)
GRANDE = bytes(random.getrandbits(8) for _ in range(200 * 1024))


class Pedido(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def do_GET(self):
        self.send_response(200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Length", str(len(GRANDE)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(GRANDE)
        self.close_connection = True


def imagem(extras=()):
    return imagem_de_teste("", ["--vesa", "--console", *extras])


def red_basica(m, linea_nic, porta_http, mensaje_driver):
    m.esperar_serial("sagui:/$", 120)
    s = m.serial()
    verificar(mensaje_driver in s, f"el kernel detecta la tarjeta: {mensaje_driver}")
    verificar("[ ok ] Red: ne2000 52:54:00:12:34:56, 10.0.2.15 (DHCP)" in s, "DHCP: 10.0.2.15 con la línea de arranque")
    verificar("[REDE] placa ne2000" in s, "la pila enciende la placa ne2000")
    out = m.comando("ifconfig")
    verificar("driver ne2000" in out and "IPv4 10.0.2.15/24" in out, "ifconfig: driver ne2000 y 10.0.2.15/24")
    out = m.comando("ping -c 3 10.0.2.2", tempo=30)
    verificar("3 paquetes enviados, 3 recibidos, 0% perdidos" in out, "ping -c 3 10.0.2.2")
    out = m.comando(f"curl -v -o /g.bin http://10.0.2.2:{porta_http}/grande.bin", tempo=120)
    verificar("CRC32 %08x" % zlib.crc32(GRANDE) in out, "descarga HTTP de 200 KB íntegra (CRC32)")
    verificar("[SAGUI] EXCECAO" not in m.serial(), "sin excepciones del kernel")
    return s


def parte_pci(porta_http):
    extras = ["-netdev", "user,id=n0", "-device", "ne2k_pci,netdev=n0"]
    with Maquina("h328-pci", "ide", imagem=imagem(), extras=extras) as m:
        s = red_basica(m, extras, porta_http, "[NE2000] 00:03.0 (10ec:8029)")
        verificar("[REDE] placa ne2000 (Realtek RTL8029 (NE2000 PCI)), com interrupcao" in s, "PCI: con interrupción")
        out = m.comando("lspci")
        verificar("10ec:8029" in out or "10EC:8029" in out.upper(), "lspci muestra la RTL8029")


def parte_pci_ioapic(porta_http):
    """Con el IOAPIC el kernel autoprueba la IRQ de la tarjeta (D211) con provocar_irq()."""
    extras = ["-netdev", "user,id=n0", "-device", "ne2k_pci,netdev=n0"]
    with Maquina("h328-pci-ioapic", "ide", imagem=imagem(["--ioapic"]), extras=extras) as m:
        red_basica(m, extras, porta_http, "[NE2000] 00:03.0 (10ec:8029)")
        s = m.serial()
        verificar("autoprueba (arranque): ne2000 00:03.0" in s and "IRQ 11: llega" in s, "IOAPIC: la autoprueba de la IRQ de la ne2000 llega")


def parte_isa(porta_http):
    extras = ["-netdev", "user,id=n0", "-device", "ne2k_isa,netdev=n0,iobase=0x300,irq=9"]
    with Maquina("h328-isa", "ide", imagem=imagem(["--ne2k-isa"]), extras=extras) as m:
        s = red_basica(m, extras, porta_http, "[NE2000] ISA portas 300, irq 9, MAC 52:54:00:12:34:56")
        verificar("[REDE] placa ne2000 (NE2000 compatible (ISA)), com interrupcao" in s, "ISA: IRQ 9 autodetectada y en uso")


def parte_isa280(porta_http):
    extras = ["-netdev", "user,id=n0", "-device", "ne2k_isa,netdev=n0,iobase=0x280,irq=10"]
    with Maquina("h328-isa280", "ide", imagem=imagem(["--ne2k-isa", "--ne2k-irq", "10"]), extras=extras) as m:
        s = red_basica(m, extras, porta_http, "[NE2000] ISA portas 280, irq 10, MAC 52:54:00:12:34:56")
        verificar("com interrupcao" in s, "ISA 0x280: IRQ 10 por configuración")


def parte_sinisa():
    extras = ["-netdev", "user,id=n0", "-device", "ne2k_isa,netdev=n0,iobase=0x300,irq=9"]
    with Maquina("h328-sinisa", "ide", imagem=imagem(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("sondeo ISA desactivado" in s, "por defecto el kernel no sondea la ISA")
        verificar("[NE2000] ISA portas" not in s and "[ ok ] Red:" not in s, "sin sondeo no hay tarjeta de red")


def parte_ninguna():
    with Maquina("h328-ninguna", "ide", imagem=imagem(["--ne2k-isa"]), extras=["-nic", "none"]) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar("[NE2000] ISA portas" not in s and "[ ok ] Red:" not in s, "sin tarjeta NE2000: ni detecta ni inventa nada")
        verificar("[SAGUI] EXCECAO" not in s, "el sondeo de puertos vacíos no provoca excepciones")


def main():
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Pedido)
    srv.daemon_threads = True
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    porta = srv.server_address[1]
    partes = sys.argv[1:] or ["pci", "pciioapic", "isa", "isa280", "sinisa", "ninguna"]
    try:
        if "pci" in partes:
            parte_pci(porta)
        if "pciioapic" in partes:
            parte_pci_ioapic(porta)
        if "isa" in partes:
            parte_isa(porta)
        if "isa280" in partes:
            parte_isa280(porta)
        if "sinisa" in partes:
            parte_sinisa()
        if "ninguna" in partes:
            parte_ninguna()
    finally:
        srv.shutdown()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALLÓ:", e)
        sys.exit(1)
