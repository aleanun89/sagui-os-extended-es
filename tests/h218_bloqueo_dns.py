#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H218 (D218) - bloqueo de publicidad y rastreo por DNS, al estilo de Pi-hole.

1. En el PC (tests/recursos/bloqueo_anfitriao.c, con los sanitizadores): el
   analizador de listas en formato hosts (100 000 dominios en los cuatro
   formatos, con comentarios, CRLF y líneas basura), la tabla (comparada byte a
   byte con la que calcula Python), la búsqueda con subdominios, permitidos y
   bloqueados más concretos (contra un modelo en Python), la validación de
   tablas rotas y el rendimiento (memoria y nanosegundos por consulta).
2. En QEMU, con /home persistente y la red de usuario (un DNS falso y un
   servidor HTTP en el anfitrión):
   - desactivado por defecto: todo se resuelve;
   - «bloqueo activar» con la lista de ejemplo: un dominio de la lista y sus
     subdominios dan 0.0.0.0 (también por el DNS del DHCP, sin red), otro
     normal resuelve; permitir, bloquear y quitar; últimos; infopc;
   - «bloqueo actualizar» con listas del anfitrión: una de 100 000 dominios
     (tiempo de compilación y memoria del kernel) y una página HTML que se
     rechaza sin perder la anterior;
   - el servidor dnsbloqueo en el puerto UDP 53, preguntado desde el PC por
     hostfwd: bloqueado (A y AAAA), reenviado al DNS de verdad y basura;
   - tras reiniciar sigue activado con la misma tabla; desactivar resuelve.
"""
import functools
import http.server
import os
import random
import socket
import struct
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, FalhaTeste  # noqa: E402

DADOS_MB = 32
N_GRANDE = 100_000
MASCARA = (1 << 64) - 1


def fnv(nome):
    h = 0xcbf29ce484222325
    for b in nome.lower().encode():
        h ^= b
        h = (h * 0x100000001b3) & MASCARA
    return h


def decidir(nome, bloq, perm, sub):
    """El modelo de bloqueo_decidir (kernel/inc/bloqueo_tabla.h)."""
    partes = nome.split(".")
    for i in range(len(partes)):
        s = ".".join(partes[i:])
        if fnv(s) in perm:
            return 2
        if (i == 0 or sub) and fnv(s) in bloq:
            return 1
    return 0


def nome_aleatorio(r):
    letras = "abcdefghijklmnopqrstuvwxyz0123456789-"
    rot = lambda: "".join(r.choice(letras[:26]) + "".join(r.choice(letras) for _ in range(r.randint(2, 12))))  # noqa: E731
    n = r.choice([2, 2, 3, 3, 4])
    return ".".join(rot() for _ in range(n - 1)) + "." + r.choice(["com", "net", "org", "io", "es", "info"])


def gerar_lista(caminho, n, semente):
    """Una lista hosts de n dominios en los cuatro formatos, con ruido. Devuelve el conjunto de dominios válidos."""
    r = random.Random(semente)
    dominios = set()
    while len(dominios) < n:
        dominios.add(nome_aleatorio(r))
    dominios = sorted(dominios)
    r.shuffle(dominios)
    linhas = ["# Lista de prueba generada (h218)", "127.0.0.1 localhost", "127.0.0.1 localhost.localdomain",
              "255.255.255.255 broadcasthost", "::1 localhost ip6-localhost ip6-loopback", "fe80::1%lo0 localhost",
              "0.0.0.0 0.0.0.0", ""]
    validos = set()
    for i, d in enumerate(dominios):
        f = i % 7
        if f == 0:
            linhas.append(f"0.0.0.0 {d}")
        elif f == 1:
            linhas.append(f"127.0.0.1 {d}\r")
        elif f == 2:
            linhas.append(d)
        elif f == 3:
            linhas.append(f"||{d}^")
        elif f == 4:
            linhas.append(f"0.0.0.0\t{d.upper()}   # comentario al final")
        elif f == 5:
            linhas.append(f"  0.0.0.0   {d}.  ")
        else:
            linhas.append(f"0.0.0.0 {d}")
        validos.add(d)
        if i % 1000 == 0:   # basura que no debe entrar
            linhas += ["esto no es una lista", "0.0.0.0 mal_formado..com", "0.0.0.0 " + "a" * 70 + ".com",
                       "<html><body>404</body></html>", "0.0.0.0", "1.2.3.4 5.6.7.8", "# 0.0.0.0 comentado.com"]
    open(caminho, "w", encoding="utf-8", newline="\n").write("\n".join(linhas) + "\n")
    return validos


def parte_anfitriao():
    print("== en el PC: analizador, tabla, búsqueda y rendimiento ==")
    os.makedirs(SAIDA, exist_ok=True)
    fonte = os.path.join(RAIZ, "tests", "recursos", "bloqueo_anfitriao.c")
    inc = ["-I", os.path.join(RAIZ, "kernel", "inc")]
    san = os.path.join(SAIDA, "bloqueo_anfitriao")
    rap = os.path.join(SAIDA, "bloqueo_anfitriao_o2")
    subprocess.run(["gcc", "-O1", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-fno-sanitize-recover=all"] + inc + ["-o", san, fonte], check=True)
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror"] + inc + ["-o", rap, fonte], check=True)
    verificar(True, "el envoltorio compila con -Wall -Wextra -Werror y los sanitizadores")

    hosts = os.path.join(SAIDA, "h218_hosts.txt")
    perm_txt = os.path.join(SAIDA, "h218_permitidos.txt")
    tabla = os.path.join(SAIDA, "h218_tabla.bin")
    validos = gerar_lista(hosts, N_GRANDE, 218)
    r = random.Random(7)
    amostra = sorted(validos)
    r.shuffle(amostra)
    permitidos = {"sub." + amostra[0], amostra[1], "livre-" + amostra[2]}
    open(perm_txt, "w").write("".join(p + "\n" for p in sorted(permitidos)))

    out = subprocess.run([san, "compilar", hosts, perm_txt, tabla], check=True, capture_output=True, text=True).stdout
    campos = dict(zip(out.split()[0::2], out.split()[1::2]))
    print("  ", out.strip())
    verificar(int(campos["bloq"]) == len(validos), f"la lista de {N_GRANDE} da {len(validos)} dominios (sin la basura ni localhost)")
    verificar(int(campos["mas"]) > 0, f"las líneas basura se cuentan como malas ({campos['mas']})")
    esperado_b = sorted({fnv(d) for d in validos})
    esperado_p = sorted({fnv(d) for d in permitidos})
    dados = open(tabla, "rb").read()
    cab = struct.pack("<8sIII12x", b"SGBLOQ01", 1, len(esperado_b), len(esperado_p))
    corpo = struct.pack(f"<{len(esperado_b)}Q", *esperado_b) + struct.pack(f"<{len(esperado_p)}Q", *esperado_p)
    verificar(dados == cab + corpo, "la tabla es byte a byte la que calcula Python (FNV-1a de 64 bits, ordenada)")
    verificar(len(dados) == 32 + 8 * (len(validos) + len(permitidos)),
              f"memoria: {len(dados)} bytes ({len(dados) / 1048576:.2f} MB) para {len(validos)} dominios")

    # consultas contra el modelo
    nomes = []
    for d in amostra[:400]:
        nomes += [d, "ads." + d, "a.b." + d, "x" + d, d.split(".", 1)[1] if d.count(".") > 1 else "otro.es"]
    nomes += ["sub." + amostra[0], "mas.sub." + amostra[0], amostra[1], "ads." + amostra[1],
              "livre-" + amostra[2], amostra[2], "ejemplo.com", "sagui.prueba", "com", "a.com"]
    for i in range(2000):
        nomes.append(nome_aleatorio(r))
    lista_nomes = os.path.join(SAIDA, "h218_nomes.txt")
    open(lista_nomes, "w").write("".join(n + "\n" for n in nomes))
    bset, pset = set(esperado_b), set(esperado_p)
    m = {}
    for sub in (1, 0):
        out = subprocess.run([san, "consultar", tabla, str(sub), lista_nomes], check=True, capture_output=True, text=True).stdout
        res = [int(l.rsplit(" ", 1)[1]) for l in out.splitlines()]
        esperado = [decidir(n, bset, pset, sub) for n in nomes]
        diferentes = [(n, a, b) for n, a, b in zip(nomes, res, esperado) if a != b]
        verificar(len(res) == len(nomes) and not diferentes,
                  f"subdominios={sub}: {len(nomes)} consultas iguales al modelo "
                  f"({sum(1 for x in res if x == 1)} bloqueadas, {sum(1 for x in res if x == 2)} permitidas)"
                  + (f"; primera distinta: {diferentes[0]}" if diferentes else ""))
        if sub:
            m = dict(zip(nomes, res))
    verificar(m["ads." + amostra[5]] == 1 and m[amostra[5]] == 1, "un subdominio de un dominio de la lista se bloquea")
    verificar(m["sub." + amostra[0]] == 2 and m["mas.sub." + amostra[0]] == 2 and m[amostra[0]] == 1,
              "un permitido gana (también para sus subdominios) sin desbloquear al padre")
    verificar(m[amostra[1]] == 2 and m["ads." + amostra[1]] == 2, "permitir un dominio de la lista lo desbloquea con sus subdominios")

    # tablas rotas: el kernel las rechaza
    def validar(b):
        p = os.path.join(SAIDA, "h218_rota.bin")
        open(p, "wb").write(b)
        return int(subprocess.run([san, "validar", p], check=True, capture_output=True, text=True).stdout)
    verificar(validar(dados) == 0, "la tabla buena se acepta")
    rotas = {
        "vacía": b"",
        "solo media cabecera": dados[:16],
        "cortada": dados[:-8],
        "con un byte de más": dados + b"\0",
        "otra magia": b"SGBLOQ02" + dados[8:],
        "otra versión": dados[:8] + struct.pack("<I", 2) + dados[12:],
        "desordenada": dados[:32] + dados[40:48] + dados[32:40] + dados[48:],
        "repetida": dados[:32] + dados[32:40] + dados[32:40] + dados[48:],
        "cuenta enorme": dados[:12] + struct.pack("<I", 0xFFFFFFF0) + dados[16:],
        "suma que desborda": dados[:12] + struct.pack("<II", 0x80000000, 0x80000000) + dados[20:],
    }
    for nome, b in rotas.items():
        verificar(validar(b) != 0, f"tabla rota ({nome}): rechazada")

    # rendimiento (sin sanitizadores)
    out = subprocess.run([rap, "compilar", hosts, perm_txt, tabla], check=True, capture_output=True, text=True).stdout
    ms = float(out.split()[-1])
    out = subprocess.run([rap, "medir", tabla, "1", lista_nomes, "200"], check=True, capture_output=True, text=True).stdout
    ns = float(out.split()[1])
    print(f"   analizar y ordenar {len(validos)} dominios: {ms:.0f} ms; consulta: {ns:.0f} ns")
    verificar(ns < 5000, f"una consulta con {len(validos)} dominios tarda {ns:.0f} ns (< 5 us)")
    return hosts


# ------------------------------------------------------------ QEMU

def puerto_libre(tipo=socket.SOCK_STREAM):
    s = socket.socket(socket.AF_INET, tipo)
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


def nome_dns(m, i):
    partes = []
    while m[i]:
        partes.append(m[i + 1:i + 1 + m[i]].decode())
        i += 1 + m[i]
    return ".".join(partes), i + 1


def pergunta_dns(nome, tipo=1, ident=0x1234):
    q = struct.pack(">HHHHHH", ident, 0x0100, 1, 0, 0, 0)
    for p in nome.split("."):
        q += bytes([len(p)]) + p.encode()
    return q + b"\0" + struct.pack(">HH", tipo, 1)


def servidor_dns(porta, parar, perguntas):
    """DNS falso: sagui.prueba -> 10.1.2.3; doubleclick.net (y subdominios) -> 10.9.9.9; resto NXDOMAIN."""
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.bind(("127.0.0.1", porta))
    s.settimeout(0.3)
    while not parar.is_set():
        try:
            m, de = s.recvfrom(2048)
        except socket.timeout:
            continue
        if len(m) < 12:
            continue
        nome, i = nome_dns(m, 12)
        perguntas.append(nome)
        tipo = struct.unpack(">H", m[i:i + 2])[0]
        q = m[12:i + 4]
        ip = {"sagui.prueba": "10.1.2.3"}.get(nome)
        if nome == "doubleclick.net" or nome.endswith(".doubleclick.net"):
            ip = "10.9.9.9"
        if ip and tipo == 1:
            r = m[:2] + struct.pack(">HHHHH", 0x8180, 1, 1, 0, 0) + q
            r += b"\xc0\x0c" + struct.pack(">HHIH", 1, 1, 60, 4) + socket.inet_aton(ip)
        else:
            r = m[:2] + struct.pack(">HHHHH", 0x8183 if not ip else 0x8180, 1, 0, 0, 0) + q
        s.sendto(r, de)
    s.close()


class Http(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        pass

    def do_GET(self):
        arquivos = self.server.arquivos
        if self.path not in arquivos:
            self.send_response(404)
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        corpo = arquivos[self.path]
        self.send_response(200)
        self.send_header("Content-Type", "text/plain")
        self.send_header("Content-Length", str(len(corpo)))
        self.end_headers()
        self.wfile.write(corpo)


def imagem_persistente(nome):
    b = os.path.join(RAIZ, "build")
    destino = os.path.join(SAIDA, nome + ".img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", destino, "--vesa", "--console",
                    "--dados-mb", str(DADOS_MB)], check=True, stdout=subprocess.DEVNULL)
    return destino


def consultar_udp(porta, nome, tipo=1, ident=0x4242, prazo=6.0):
    s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    s.settimeout(prazo)
    try:
        s.sendto(pergunta_dns(nome, tipo, ident), ("127.0.0.1", porta))
        m, _ = s.recvfrom(2048)
    except socket.timeout:
        return None
    finally:
        s.close()
    ident_r, flags, qd, an = struct.unpack(">HHHH", m[:8])
    _, i = nome_dns(m, 12)
    i += 4
    ips = []
    for _ in range(an):
        i += 2 if m[i] & 0xC0 == 0xC0 else len(nome_dns(m, i)[0]) + 2
        t, _c, _ttl, n = struct.unpack(">HHIH", m[i:i + 10])
        i += 10
        ips.append(socket.inet_ntoa(m[i:i + 4]) if t == 1 else socket.inet_ntop(socket.AF_INET6, m[i:i + 16]))
        i += n
    return {"id": ident_r, "rcode": flags & 15, "ips": ips}


def nslookup(m, nome, servidor=None):
    out = m.comando(f"nslookup {nome}" + (f" {servidor}" if servidor else ""), 40)
    for l in out.splitlines():
        if l.startswith("Dirección:"):
            return l.split()[1]
    return "error"


def parte_qemu(hosts_grande):
    print("== en QEMU ==")
    p_dns = puerto_libre(socket.SOCK_DGRAM)
    p_http = puerto_libre()
    p_udp = puerto_libre(socket.SOCK_DGRAM)
    parar = threading.Event()
    perguntas = []
    threading.Thread(target=servidor_dns, args=(p_dns, parar, perguntas), daemon=True).start()
    pequena = "# lista del anfitrión\n0.0.0.0 rastreo.prueba\n0.0.0.0 publicidad.prueba\n"
    srv = http.server.ThreadingHTTPServer(("127.0.0.1", p_http), Http)
    srv.arquivos = {"/pequena.txt": pequena.encode(), "/grande.txt": open(hosts_grande, "rb").read(),
                    "/malo.html": b"<html><body>Not found</body></html>\n" * 50}
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    dns = f"10.0.2.2:{p_dns}"
    img = imagem_persistente("h218")
    extras = ["-nic", f"user,model=e1000,hostfwd=udp:127.0.0.1:{p_udp}-:53"]
    try:
        with Maquina("h218-a", "ide", imagem=img, snapshot=False, extras=extras) as m:
            m.esperar_serial("sagui:/$", 120)
            out = m.comando("bloqueo")
            verificar("desactivado" in out, "por defecto el bloqueo está desactivado")
            verificar("[BLOQUEO]" not in m.serial(), "desactivado: el kernel no carga nada al arrancar")
            verificar(nslookup(m, "doubleclick.net", dns) == "10.9.9.9", "desactivado: doubleclick.net se resuelve")
            out = m.comando("bloqueo activar", 60)
            verificar("Bloqueo activado" in out and "ejemplo" in out, "bloqueo activar con la lista de ejemplo (sin red)")
            n = len(perguntas)
            verificar(nslookup(m, "doubleclick.net", dns) == "0.0.0.0", "activado: doubleclick.net da 0.0.0.0")
            verificar(nslookup(m, "ad.doubleclick.net", dns) == "0.0.0.0", "y su subdominio ad.doubleclick.net")
            verificar(len(perguntas) == n, "lo bloqueado no sale a la red (el DNS no recibió nada)")
            out = m.comando("nslookup stats.g.doubleclick.net", 40)
            verificar("0.0.0.0" in out and "bloqueado" in out, "por el DNS del DHCP también (y nslookup lo explica)")
            verificar(nslookup(m, "sagui.prueba", dns) == "10.1.2.3", "un nombre normal se resuelve")
            out = m.comando("bloqueo permitir ad.doubleclick.net")
            verificar("Permitido" in out, "bloqueo permitir")
            verificar(nslookup(m, "ad.doubleclick.net", dns) == "10.9.9.9", "permitido: ad.doubleclick.net se resuelve")
            verificar(nslookup(m, "x.ad.doubleclick.net", dns) == "10.9.9.9", "y sus subdominios")
            verificar(nslookup(m, "doubleclick.net", dns) == "0.0.0.0", "el padre sigue bloqueado")
            out = m.comando("bloqueo bloquear sagui.prueba")
            verificar(nslookup(m, "sagui.prueba", dns) == "0.0.0.0", "bloqueo bloquear: bloqueado a mano")
            out = m.comando("bloqueo probar www.sagui.prueba")
            verificar("bloqueado" in out, "bloqueo probar: el subdominio también")
            m.comando("bloqueo quitar sagui.prueba")
            verificar(nslookup(m, "sagui.prueba", dns) == "10.1.2.3", "bloqueo quitar: vuelve a resolver")
            out = m.comando("bloqueo ultimos")
            verificar("doubleclick.net" in out and "sagui.prueba" in out, "bloqueo ultimos: los últimos bloqueados")
            out = m.comando("bloqueo")
            verificar("activado" in out and "bloqueadas" in out, "bloqueo estado: activado, con los contadores")
            m.captura("H218-bloqueo-estado")
            out = m.comando("infopc -t sistema", 120)
            verificar("Bloqueo DNS:       activado" in out, "infopc: la línea del bloqueo")

            # listas descargadas: una buena pequeña, una HTML que se rechaza
            m.comando("bloqueo listas quitar 1")
            m.comando(f"bloqueo listas agregar http://10.0.2.2:{p_http}/pequena.txt")
            m.comando(f"bloqueo listas agregar http://10.0.2.2:{p_http}/malo.html")
            out = m.comando("bloqueo actualizar", 120)
            verificar("bien: 2 dominios" in out, "actualizar: la lista del anfitrión se descarga y se comprueba")
            verificar("HTML" in out, "actualizar: una página HTML se rechaza")
            verificar(nslookup(m, "publicidad.prueba", dns) == "0.0.0.0", "lo de la lista descargada se bloquea")
            verificar(nslookup(m, "doubleclick.net", dns) == "10.9.9.9", "y la de ejemplo ya no se usa")

            # la grande: 100 000 dominios
            m.comando("bloqueo listas quitar 2")
            m.comando("bloqueo listas quitar 1")
            m.comando(f"bloqueo listas agregar http://10.0.2.2:{p_http}/grande.txt")
            t0 = time.time()
            out = m.comando("bloqueo actualizar", 600)
            print(f"   (actualizar con {N_GRANDE} dominios: {time.time() - t0:.1f} s en el QEMU)")
            for l in out.splitlines():
                if l.startswith("Tabla:") or "bien:" in l:
                    print("  ", l.strip())
            verificar(f"bien: {N_GRANDE} dominios" in out, f"actualizar: la lista de {N_GRANDE} dominios")
            out = m.comando("bloqueo")
            print("  ", [l.strip() for l in out.splitlines() if "Tabla" in l])
            verificar(f"{N_GRANDE} dominios bloqueados" in out, "el kernel la tiene cargada")
            uno = sorted(l.split()[1] for l in open(hosts_grande)
                         if l.startswith("0.0.0.0 ") and l.split()[1][0].isalpha() and l.split()[1].count(".") > 1)[0]
            verificar(nslookup(m, uno) == "0.0.0.0" and nslookup(m, "ads." + uno) == "0.0.0.0",
                      f"un dominio de la lista grande ({uno}) y su subdominio dan 0.0.0.0")
            for l in m.serial().splitlines():
                if "[BLOQUEO] activo:" in l:
                    ultima = l.strip()
            print("   kernel:", ultima)

            # fase 2: el servidor en el puerto 53
            m.comando(f"bloqueo listas quitar 1")
            m.comando("bloqueo activar", 60)
            m.comando(f"dnsbloqueo -r {dns} &", 20)
            time.sleep(2)
            r = consultar_udp(p_udp, "doubleclick.net")
            verificar(r is not None and r["ips"] == ["0.0.0.0"] and r["id"] == 0x4242,
                      f"dnsbloqueo: doubleclick.net desde el PC da 0.0.0.0 ({r})")
            r = consultar_udp(p_udp, "ads.doubleclick.net", tipo=28)
            verificar(r is not None and r["ips"] == ["::"], f"dnsbloqueo: AAAA bloqueado da :: ({r})")
            r = consultar_udp(p_udp, "sagui.prueba", ident=0x777)
            verificar(r is not None and r["ips"] == ["10.1.2.3"] and r["id"] == 0x777,
                      f"dnsbloqueo: lo demás se reenvía al DNS de verdad ({r})")
            r = consultar_udp(p_udp, "no.existe.prueba")
            verificar(r is not None and r["rcode"] == 3, "dnsbloqueo: NXDOMAIN del DNS de verdad")
            s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
            for b in (b"", b"\0" * 5, b"\xff" * 40, pergunta_dns("x.com")[:20], os.urandom(300)):
                s.sendto(b, ("127.0.0.1", p_udp))
            s.close()
            time.sleep(1)
            r = consultar_udp(p_udp, "doubleclick.net")
            verificar(r is not None and r["ips"] == ["0.0.0.0"], "dnsbloqueo sigue respondiendo tras recibir basura")
            m.captura("H218-dnsbloqueo")

            m.digitar("reiniciar\n", 0.04)
            fim = time.time() + 60
            while m.proc.poll() is None and time.time() < fim:
                time.sleep(0.3)
            verificar(m.proc.poll() is not None, "reiniciar: la máquina se reinició")
        with Maquina("h218-b", "ide", imagem=img, snapshot=False, extras=extras) as m:
            m.esperar_serial("sagui:/$", 120)
            verificar("[BLOQUEO] activo:" in m.serial(), "tras reiniciar, el kernel carga la tabla al arrancar")
            out = m.comando("bloqueo")
            verificar("activado" in out, "tras reiniciar sigue activado (persistente en /home)")
            verificar(nslookup(m, "doubleclick.net", dns) == "0.0.0.0", "tras reiniciar doubleclick.net sigue bloqueado")
            verificar(nslookup(m, "ad.doubleclick.net", dns) == "10.9.9.9", "y el permitido se recuerda")
            out = m.comando("bloqueo desactivar")
            verificar("desactivado" in out, "bloqueo desactivar")
            verificar(nslookup(m, "doubleclick.net", dns) == "10.9.9.9", "desactivado: vuelve a resolverse todo")
            out = m.comando("infopc -t sistema", 120)
            verificar("Bloqueo DNS:       desactivado" in out, "infopc: desactivado")
    finally:
        parar.set()
        srv.shutdown()


def main():
    hosts = parte_anfitriao()
    if "--solo-pc" in sys.argv:
        return 0
    parte_qemu(hosts)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
