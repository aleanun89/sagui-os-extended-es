#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H400 (D398) - «escaner»: el escáner de red propio de SaguiOS.

En QEMU con la red de usuario (slirp), el anfitrión es 10.0.2.2 y sus servicios en 127.0.0.1 se ven desde la VM.
Se levantan en el anfitrión un servidor que manda un rótulo «SSH-2.0-...» al conectar, un servidor HTTP con cabecera
«Server:» y un puerto cerrado, y en la VM se comprueba:
  1. escaner -p A,B,C 10.0.2.2: A y B abiertos, C no; sin -sV el servicio sale de la tabla; el recuento de cerrados.
  2. escaner -sV: el rótulo SSH y el «Server:» del HTTP.
  3. escaner -sn 10.0.2.2: el equipo está activo; -sn 10.0.2.1-3 (rango) y 10.0.2.0/30 (CIDR) expanden bien.
  4. -oN guarda el informe en un fichero; -T4 y -v funcionan; errores de uso (CIDR enorme, puerto mal escrito).
  5. Un servicio local de la propia VM (nc -l) en 127.0.0.1 sale abierto.
"""
import http.server
import os
import re
import socket
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, FalhaTeste  # noqa: E402


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


class Http(http.server.BaseHTTPRequestHandler):
    server_version = "PruebaHTTP/1.0"
    sys_version = ""

    def log_message(self, *a):
        pass

    def do_HEAD(self):
        self.send_response(200)
        self.send_header("Content-Length", "0")
        self.end_headers()

    do_GET = do_HEAD


def servidor_ssh(p, parar):
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", p))
    s.listen(16)
    s.settimeout(0.5)
    while not parar.is_set():
        try:
            c, _ = s.accept()
        except socket.timeout:
            continue
        try:
            c.sendall(b"SSH-2.0-PruebaSSH_1.0\r\n")
        except OSError:
            pass
        c.close()
    s.close()


def main():
    parar = threading.Event()
    p_ssh, p_http, p_cerr = puerto_libre(), 0, puerto_libre()
    while p_cerr == p_ssh:
        p_cerr = puerto_libre()
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Http)
    srv.daemon_threads = True
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    p_http = srv.server_address[1]
    threading.Thread(target=servidor_ssh, args=(p_ssh, parar), daemon=True).start()
    try:
        with Maquina("h400", "ide", extras=["-nic", "user,model=e1000"]) as m:
            m.esperar_serial("sagui:/$", 120)
            lista = f"{p_ssh},{p_http},{p_cerr}"

            out = m.comando(f"escaner -p {lista} 10.0.2.2", tempo=90)
            verificar("PUERTO" in out and "ESTADO" in out and "SERVICIO" in out, "cabecera de la tabla")
            verificar(re.search(rf"{p_ssh}/tcp\s+abierto", out) and re.search(rf"{p_http}/tcp\s+abierto", out),
                      "los dos puertos del anfitrión salen abiertos")
            verificar(f"{p_cerr}/tcp" not in out, "el puerto cerrado no se lista")
            verificar("1 puertos cerrados" in out, "recuento de cerrados (1)")
            verificar("Hecho: 1 direcciones (1 activas) escaneadas en" in out, "resumen con tiempo total")

            out = m.comando(f"escaner -sV -p {lista} 10.0.2.2", tempo=90)
            verificar("VERSIÓN" in out or "VERSI" in out, "columna VERSIÓN con -sV")
            verificar(re.search(rf"{p_ssh}/tcp\s+abierto\s+ssh\s+SSH-2\.0-PruebaSSH_1\.0", out), "rótulo SSH leído")
            verificar(re.search(rf"{p_http}/tcp\s+abierto\s+http\s+PruebaHTTP/1\.0", out), "Server: del HTTP leído con HEAD")

            out = m.comando("escaner -sn 10.0.2.2", tempo=60)
            verificar("Equipo 10.0.2.2 activo" in out, "-sn: el anfitrión está activo")
            out = m.comando("escaner -sn 10.0.2.1-3", tempo=120)
            verificar("Equipo 10.0.2.2 activo" in out and "3 direcciones escaneadas" in out, "-sn con rango a.b.c.1-3")
            out = m.comando("escaner -sn 10.0.2.0/30", tempo=120)
            verificar("2 direcciones escaneadas" in out, "CIDR /30: dos equipos (sin red ni difusión)")
            out = m.comando("escaner -sn 10.0.0.0/16", tempo=30)
            verificar("demasiado grande" in out, "CIDR enorme rechazado")
            out = m.comando("escaner -p 99999 10.0.2.2", tempo=30)
            verificar("lista de puertos no válida" in out, "puerto mal escrito rechazado")

            out = m.comando(f"escaner -T4 -v -p {p_ssh} -oN /tmp/inf.txt 10.0.2.2", tempo=90)
            verificar("Puerto abierto descubierto" in out, "-v muestra el hallazgo")
            out = m.comando("cat /tmp/inf.txt", tempo=30)
            verificar(f"{p_ssh}/tcp" in out and "abierto" in out and "Hecho:" in out, "-oN guarda el informe")

            m.digitar("nc -l 9000 &\n", 0.04)
            time.sleep(1.5)
            out = m.comando("escaner -p 9000,9001 127.0.0.1", tempo=60)
            verificar(re.search(r"9000/tcp\s+abierto", out) and "9001/tcp" not in out, "servicio local (nc -l) abierto, vecino cerrado")
            out = m.comando("escaner --top-ports 5 -Pn 10.0.2.2", tempo=60)
            verificar("Informe de escaneo de 10.0.2.2" in out and "Hecho:" in out, "--top-ports corre")
            verificar("[SAGUI] EXCECAO" not in m.serial(), "sin excepciones del kernel")
            m.captura("H400-escaner")
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
