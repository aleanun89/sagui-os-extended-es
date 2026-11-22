#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H437 - velocidad de la red: vaciar la tarjeta en cada sondeo y ventana TCP grande (D437).

Mide con /testes/vazao contra un servidor TCP del anfitrión (10.0.2.2 por la red de usuario
de QEMU) la descarga y la subida de 10 MB, con la e1000 y con la RTL8139, también con un
retardo simulado (filter-buffer de QEMU: suelta las tramas cada 20 ms) y por el bucle local.

Criterio:
  1. Las transferencias llegan enteras (10 MB bajados; el servidor recibe los bytes subidos).
  2. Escala de ventana (RFC 7323): el slirp de QEMU no la tiene (la conexión sigue sin escala, con
     64 KB como mucho); entre dos SaguiOS unidos por un cable de mentira con retardo, el SYN lleva
     el MSS y la escala 2, el SYN+ACK la contesta y la subida pasa del mínimo.
  3. Velocidades mínimas (con KVM y sin él), muy por debajo de lo medido para no fallar por
     carga del PC, pero por encima de lo que daba la pila antigua con retardo.
  4. El ACK retrasado: en la descarga sale como mucho un ACK por cada ~1,6 segmentos.
SAGUI_VAZAO_MEDIR=1: solo mide y escribe la tabla (sin los mínimos), para comparar ramas.
"""
import os
import re
import socket
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
SO_MEDIR = os.environ.get("SAGUI_VAZAO_MEDIR") == "1"
TAM = 10 * 1024 * 1024
BLOCO = bytes((i * 7 + 3) & 0xFF for i in range(65536))
LINHA = re.compile(r"VAZAO (\w+): (\d+) bytes en (\d+) ms, (\d+) KB/s; RX (\d+) paquetes, TX (\d+) paquetes \((\d+),(\d) por tic")


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


class Servidor:
    """«DAME\\n» -> manda TAM bytes y cierra; cualquier otra cosa -> cuenta hasta el fin y contesta."""

    def __init__(self):
        self.porta = puerto_libre()
        self.recebidos = []
        self.tempos = []   # (sentido, bytes, segundos) vistos por el anfitrión
        self.parar = threading.Event()
        self.s = socket.socket()
        self.s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.s.bind(("127.0.0.1", self.porta))
        self.s.listen(4)
        self.s.settimeout(0.5)
        threading.Thread(target=self.laco, daemon=True).start()

    def laco(self):
        while not self.parar.is_set():
            try:
                c, _ = self.s.accept()
            except socket.timeout:
                continue
            threading.Thread(target=self.atender, args=(c,), daemon=True).start()

    def atender(self, c):
        c.settimeout(60)
        try:
            ini = c.recv(5)
            if ini == b"DAME\n":
                falta = TAM
                t0 = time.time()
                while falta:
                    k = min(falta, len(BLOCO))
                    c.sendall(BLOCO[:k])
                    falta -= k
                self.tempos.append(("baixar", TAM, time.time() - t0))
            else:
                total = len(ini)
                t0 = time.time()
                while True:
                    b = c.recv(65536)
                    if not b:
                        break
                    total += len(b)
                self.recebidos.append(total)
                self.tempos.append(("subir", total, time.time() - t0))
                c.sendall(b"RECIBIDO %d\n" % total)
        except OSError:
            pass
        c.close()

    def fechar(self):
        self.parar.set()
        self.s.close()


def cpu_qemu(m):
    """Tiempo de CPU (s) del proceso QEMU, para ver cuánto gasta la VM durante la transferencia."""
    try:
        with open("/proc/%d/stat" % m.proc.pid) as h:
            c = h.read().rsplit(")", 1)[1].split()
        return (int(c[11]) + int(c[12])) / os.sysconf("SC_CLK_TCK")
    except OSError:
        return 0.0


def medir(m, cmd, tempo=240):
    srv = getattr(m, "srv", None)
    n0 = len(srv.tempos) if srv else 0
    c0, t0 = cpu_qemu(m), time.time()
    out = m.comando(cmd, tempo)
    c1, t1 = cpu_qemu(m), time.time()
    r = LINHA.search(out)
    if not r:
        print("    salida: %s" % out.strip()[-400:])
        return None, out
    modo, total, ms, kbs, rx, tx, a, b = r.groups()
    d = {"modo": modo, "total": int(total), "ms": int(ms), "kbs": int(kbs), "rx": int(rx), "tx": int(tx),
         "por_tic": float("%s.%s" % (a, b)), "cpu": 100.0 * (c1 - c0) / max(t1 - t0, 0.01), "anf": 0}
    if srv and len(srv.tempos) > n0:
        _, n, seg = srv.tempos[n0]
        d["anf"] = int(n / max(seg, 0.001) / 1024)
    return d, out


def rodada(nome, modelo, srv, retardo=False):
    extras = ["-nic", f"user,model={modelo},id=n0"]
    if retardo:
        # Suelta las tramas cada 20 ms en los dos sentidos: un retardo de 0 a 20 ms, a ráfagas.
        extras = ["-netdev", "user,id=n0", "-device", f"{modelo},netdev=n0",
                  "-object", "filter-buffer,id=f0,netdev=n0,queue=all,interval=20000"]
    res = {}
    with Maquina("h437-" + nome, "ide", imagem=imagem_console(), extras=extras) as m:
        m.srv = srv
        m.esperar_serial("sagui:/$", 180)
        fim = time.time() + 30
        while "10.0.2.15 (DHCP)" not in m.serial() and time.time() < fim:
            time.sleep(0.5)
        d, out = medir(m, f"/testes/vazao baixar 10.0.2.2 {srv.porta}")
        res["baixar"] = d
        if d:
            verificar(d["total"] == TAM, f"{nome}: descarga de 10 MB entera ({d['total']} bytes)")
        d, out = medir(m, f"/testes/vazao subir 10.0.2.2 {srv.porta} {TAM}")
        res["subir"] = d
        if d:
            verificar(f"RECIBIDO {TAM}" in out, f"{nome}: subida de 10 MB entera (el servidor la recibió)")
        if not retardo and modelo == "e1000":
            m.comando("/testes/vazao escutar 9437 &", 20)
            time.sleep(1)
            d, out = medir(m, f"/testes/vazao subir 127.0.0.1 9437 {TAM // 2}")
            res["local"] = d
            if d:
                verificar(f"RECIBIDO {TAM // 2}" in out, f"{nome}: 5 MB por el bucle local enteros")
        s = m.serial()
        for l in s.split("\n"):
            if "[REDE] TCP" in l or "[REDE] rx" in l:
                print("    %s" % l.strip()[:200])
        res["serial"] = s
        verificar("EXCECAO" not in s and "PANICO" not in s, f"{nome}: sin excepciones del kernel")
    return res


def opcoes_syn(pcap):
    """De un pcap (filter-dump de QEMU), las opciones TCP de los SYN: [(flags, {opción: valor})]."""
    import struct
    with open(pcap, "rb") as h:
        d = h.read()
    i, achados = 24, []
    while i + 16 <= len(d):
        incl = struct.unpack("<I", d[i + 8:i + 12])[0]
        q = d[i + 16:i + 16 + incl]
        i += 16 + incl
        if len(q) < 54 or q[12:14] != b"\x08\x00" or q[23] != 6:
            continue
        ihl = (q[14] & 15) * 4
        t = q[14 + ihl:]
        if not (t[13] & 0x02):
            continue
        cab = (t[12] >> 4) * 4
        op, k = {}, 20
        while k < cab:
            if t[k] == 0:
                break
            if t[k] == 1:
                k += 1
                continue
            n = t[k + 1]
            op[t[k]] = t[k + 2:k + n]
            k += n
        achados.append((t[13], op))
    return achados


def entre_sagui(srv_nome="h437-dois"):
    """Dos SaguiOS unidos por un cable de mentira (sockets UDP de QEMU) con un retardo (filter-buffer
    de 10 ms en cada sentido): aquí los dos lados hablan la escala de ventana, que el slirp no tiene."""
    pa, pb = puerto_libre(), puerto_libre()
    pcap = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "build", "testes", srv_nome + ".pcap")
    if os.path.exists(pcap):
        os.remove(pcap)
    ea = ["-netdev", f"socket,id=n0,udp=127.0.0.1:{pb},localaddr=127.0.0.1:{pa}",
          "-device", "e1000,netdev=n0,mac=52:54:00:43:70:0a",
          "-object", f"filter-dump,id=d0,netdev=n0,file={pcap}"]
    eb = ["-netdev", f"socket,id=n0,udp=127.0.0.1:{pa},localaddr=127.0.0.1:{pb}",
          "-device", "e1000,netdev=n0,mac=52:54:00:43:70:0b",
          "-object", "filter-buffer,id=f0,netdev=n0,queue=all,interval=10000"]
    res = {}
    with Maquina(srv_nome + "-a", "ide", imagem=imagem_console(), extras=ea) as a, \
            Maquina(srv_nome + "-b", "ide", imagem=imagem_console(), extras=eb) as b:
        a.esperar_serial("sagui:/$", 180)
        b.esperar_serial("sagui:/$", 180)
        a.comando("ifconfig eth0 10.9.0.1/24")
        b.comando("ifconfig eth0 10.9.0.2/24")
        a.comando("/testes/vazao escutar 9437 &", 20)
        time.sleep(1)
        d, out = medir(b, f"/testes/vazao subir 10.9.0.1 9437 {TAM}")
        res["subir"] = d
        if d:
            verificar(f"RECIBIDO {TAM}" in out, "entre dos SaguiOS: subida de 10 MB entera")
        for m in (a, b):
            for l in m.serial().split("\n"):
                if "[REDE] TCP 10.9" in l or "[REDE] rx" in l:
                    print("    %s" % l.strip()[:200])
        verificar("EXCECAO" not in a.serial() + b.serial(), "entre dos SaguiOS: sin excepciones del kernel")
    res["syn"] = opcoes_syn(pcap) if os.path.exists(pcap) else []
    return res


def main():
    srv = Servidor()
    tabla = []
    minimos = {}   # (rodada, sentido) -> KB/s mínimos
    if not SO_MEDIR:
        minimos = {("e1000", "baixar"): 2000, ("e1000", "subir"): 2000, ("rtl8139", "baixar"): 1000,
                   ("e1000-retardo", "baixar"): 600, ("e1000-retardo", "subir"): 600, ("e1000", "local"): 1000}
        if not KVM:
            minimos = {k: v // 4 for k, v in minimos.items()}
    try:
        for nome, modelo, retardo in (("e1000", "e1000", False), ("rtl8139", "rtl8139", False),
                                      ("e1000-retardo", "e1000", True)):
            res = rodada(nome, modelo, srv, retardo)
            for sentido in ("baixar", "subir", "local"):
                d = res.get(sentido)
                if sentido in res:
                    verificar(d is not None, f"{nome}: {sentido} terminó con su línea VAZAO")
                if not d:
                    continue
                tabla.append((nome, sentido, d))
                m = minimos.get((nome, sentido))
                if m:
                    verificar(d["kbs"] >= m, f"{nome}: {sentido} a {d['kbs']} KB/s (mínimo {m})")
                if sentido == "baixar" and not SO_MEDIR and not retardo:
                    # ACK retrasado: unos 7000 segmentos de datos; antes salía un ACK por segmento.
                    verificar(d["tx"] * 10 <= d["rx"] * 7, f"{nome}: ACK retrasado ({d['tx']} ACK para {d['rx']} tramas)")
            if not SO_MEDIR and nome == "e1000":
                verificar("[REDE] TCP 10.0.2.2" in res["serial"] and "sin escala de ventana" in res["serial"],
                          "e1000: con el slirp (que no la ofrece) la conexión queda sin escala de ventana")
                verificar("[REDE] TCP 127.0.0.1" in res["serial"] and "con escala de ventana (escala 2" in res["serial"],
                          "e1000: por el bucle local los dos lados negocian la escala 2")
        res = entre_sagui()
        d = res.get("subir")
        verificar(d is not None, "entre dos SaguiOS: subir terminó con su línea VAZAO")
        if d:
            tabla.append(("dos-sagui", "subir", d))
            if not SO_MEDIR:
                m = 1500 if KVM else 400
                verificar(d["kbs"] >= m, f"entre dos SaguiOS con retardo: {d['kbs']} KB/s (mínimo {m})")
        if not SO_MEDIR:
            syn = [op for f, op in res["syn"] if not (f & 0x10)]
            synack = [op for f, op in res["syn"] if f & 0x10]
            print("    SYN: %s; SYN+ACK: %s" % (syn[:1], synack[:1]))
            verificar(syn and 3 in syn[0] and syn[0][3] == b"\x02" and 2 in syn[0],
                      "el SYN lleva el MSS y la escala de ventana 2 (RFC 7323)")
            verificar(synack and 3 in synack[0], "el SYN+ACK contesta con la escala (los dos lados la tienen)")
    finally:
        srv.fechar()
    print("    %-14s %-7s %9s %9s %8s %8s %8s %8s %6s" % ("rodada", "sentido", "KB/s", "anfitrión", "ms", "RX", "TX",
                                                           "RX/tic", "CPU%"))
    for nome, sentido, d in tabla:
        print("    %-14s %-7s %9d %9s %8d %8d %8d %8.1f %6.0f" % (nome, sentido, d["kbs"], d["anf"] or "-", d["ms"],
                                                                d["rx"], d["tx"], d["por_tic"], d["cpu"]))
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception as e:  # noqa: BLE001
        print("FALHA: %s" % e)
        sys.exit(1)
