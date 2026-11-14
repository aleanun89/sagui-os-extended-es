#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H252 (mejora 097, v8.0b6) - escritorio remoto de extremo a extremo: sshd + ssh -L + vncd.

El camino real del mantenedor, con la configuración de serie del vncd (solo 127.0.0.1):
  1. imagen con una clave pública del anfitrión en authorized_keys; dentro, por las órdenes
     reales del Terminal: «vncd --clave», «servicio activar/iniciar sshd» y «... vncd»;
  2. conexión directa al 5900 (hostfwd): rechazada, ni el saludo;
  3. desde el anfitrión, «ssh -N -L puerto:localhost:5900» (OpenSSH) por el hostfwd del 22;
  4. por el túnel, el cliente RFB de rfb_cliente.py: contraseña, pantalla completa igual a la
     de QEMU, teclado y ratón reales (una orden en el Terminal, abrir y cerrar Archivos con
     Alt+F4, un clic que cambia el foco, Ctrl+C a un proceso), incremental tras abrir una ventana;
  5. al cerrar el ssh, el VNC se corta limpio y el vncd acepta otra conexión por un túnel nuevo;
  6. carga: «bench mem» y «bench gui» en el Terminal con y sin un visor VNC pidiendo cuadros;
  7. si el anfitrión tiene un visor VNC real (vncviewer...), se anota (y se usa si se puede).
"""
import os
import re
import shutil
import socket
import statistics
import subprocess
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, FalhaTeste  # noqa: E402
from rfb_cliente import Rfb, banner, pantalla_qemu, diferentes, CLAVE  # noqa: E402
import h248_ssh as base  # noqa: E402
from h251_ssh_tunel import Tunel  # noqa: E402

TRAB = os.path.join(SAIDA, "h252")
DADOS_MB = 16
VISORES = ("vncviewer", "xtightvncviewer", "gvncviewer", "vncdo", "xtigervncviewer", "remmina")
CON_MAYUS = set('$_:<>|&*()"!?+~^%#@{}')


def construir_imagen(pub):
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(TRAB, "rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    with open(os.path.join(raiz, "etc", "ssh", "authorized_keys"), "w") as f:
        f.write("# clave de la prueba h252\n" + open(pub).read())
    sgfs = os.path.join(TRAB, "h252.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(TRAB, "h252.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--dados-mb", str(DADOS_MB)],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def escribir(c, texto, pausa=0.05):
    """Teclea «texto» por el cliente RFB (Mayús donde hace falta, como un teclado de verdad)."""
    for ch in texto:
        sh = ch.isupper() or ch in CON_MAYUS
        if sh:
            c.tecla(0xFFE1, True)
        c.tecla(ord(ch), True)
        c.tecla(ord(ch), False)
        if sh:
            c.tecla(0xFFE1, False)
        time.sleep(pausa)


def enter(c):
    c.tecla(0xFF0D, True)
    c.tecla(0xFF0D, False)


def ventana(m, nombre):
    """(ancho, alto, x, y) de la última ventana con ese título que anotó el kernel."""
    r = re.findall(r'janela \d+ criada por \d+: "%s" (\d+)x(\d+) em \((\d+),(\d+)\)' % nombre, m.serial())
    return tuple(map(int, r[-1])) if r else None


def ventana_cerrada(m, nombre):
    """True si el kernel anotó un Alt+F4 sobre una ventana con ese título."""
    return re.search(r"\[GUI\] Alt\+F4: fechar janela \d+ \(%s\)" % nombre, m.serial()) is not None


class Visor(threading.Thread):
    """Un visor VNC que pide actualizaciones incrementales sin parar (la carga de un cliente conectado)."""

    def __init__(self, porta):
        super().__init__(daemon=True)
        self.c = Rfb(porta)
        self.c.set_encodings([16, 5, 0, -223])
        self.c.pedir(False)
        self.parar = False
        self.actualizaciones = 0
        self.bytes = 0
        self.error = None

    def run(self):
        try:
            self.c.leer_actualizacion(30)
            while not self.parar:
                self.c.pedir(True)
                try:
                    n, _ = self.c.leer_actualizacion(2)
                except (socket.timeout, TimeoutError):
                    continue
                self.actualizaciones += 1
                self.bytes += n
        except Exception as e:  # noqa: BLE001
            if not self.parar:
                self.error = repr(e)

    def fin(self):
        self.parar = True
        self.join(10)
        self.c.cerrar()


def medir_bench(m):
    """(MB/s de memcpy de 1 MB, cuadros/s de redibujar+mostrar, cuadros/s de solo mostrar) del Terminal."""
    out = m.comando("bench mem", 400)
    mem = re.search(r"^memcpy auto\s+(?:\d+\s+){5}(\d+)", out, re.M)
    out = m.comando("bench gui", 180)
    gui = re.search(r"redibujar y mostrar (\d+)\.(\d+) cuadros/s, solo mostrar (\d+)\.(\d+)", out)
    if not mem or not gui:
        raise FalhaTeste(f"no se entiende la salida de bench: {out[-400:]!r}")
    return int(mem.group(1)), float(f"{gui.group(1)}.{gui.group(2)}"), float(f"{gui.group(3)}.{gui.group(4)}")


def maquina():
    base.TRAB = TRAB
    chave = base.gerar_chave("clave_h252")
    img = construir_imagen(chave + ".pub")
    p_ssh, p_vnc = base.puerto_libre(), base.puerto_libre()
    cl = base.Cliente(p_ssh, chave)
    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{p_ssh}-:22,hostfwd=tcp:127.0.0.1:{p_vnc}-:5900"]
    tuneis = []
    visores = []
    try:
        with Maquina("h252", "ide", imagem=img, snapshot=False, extras=extras) as m:
            m.esperar_serial("sagui:/$", 150)
            time.sleep(4)
            base.esperar_log(m, "10.0.2.15 (DHCP)", 40)
            _prueba(m, cl, p_ssh, p_vnc, tuneis, visores)
    finally:
        for v in visores:
            v.fin()
        for t in tuneis:
            t.fechar()


def _prueba(m, cl, p_ssh, p_vnc, tuneis, visores):
    # --- 1. la receta de SaguiOS, con las órdenes reales
    out = m.comando("vncd --clave " + CLAVE)
    verificar("contraseña guardada" in out, "vncd --clave: contraseña guardada")
    for sv in ("sshd", "vncd"):
        out = m.comando(f"servicio activar {sv}", 30)
        verificar("error" not in out.lower() and "no existe" not in out.lower(), f"servicio activar {sv}: {out.strip()[-80:]!r}")
    marca = len(m.serial())
    m.comando("servicio iniciar sshd", 30)
    base.esperar_log(m, "sshd: escuchando en el puerto 22", 40, marca)
    m.comando("servicio iniciar vncd", 30)
    base.esperar_log(m, "vncd: escuchando en el puerto 5900", 30, marca)
    verificar("solo desde esta máquina" in m.serial()[marca:], "vncd de serie: solo desde esta máquina")
    out = m.comando("servicio listar")
    verificar(re.search(r"^vncd\s+en marcha", out, re.M) and re.search(r"^sshd\s+en marcha", out, re.M)
              or (re.search(r"^vncd\s+(?!parado)", out, re.M) and re.search(r"^sshd\s+(?!parado)", out, re.M)),
              "servicio listar: sshd y vncd en marcha")
    time.sleep(1)

    # --- 2. sin túnel, por el hostfwd: rechazado
    marca = len(m.serial())
    verificar(banner(p_vnc) == b"", "conexión directa al 5900 (hostfwd, llega de 10.0.2.2): ni el saludo")
    base.esperar_log(m, "rechazada: no viene de esta máquina", 10, marca)

    # --- 3. el túnel ssh -N -L del anfitrión
    marca = len(m.serial())
    t = Tunel(cl, ["localhost:5900"])
    tuneis.append(t)
    p_tun = t.portas[0]
    base.esperar_log(m, "túnel a 127.0.0.1:5900 abierto", 20, marca)   # la sonda de esperar_puerto
    time.sleep(2)
    verificar(t.p.poll() is None, f"ssh -N -L {p_tun}:localhost:5900 conectado con la clave del anfitrión")

    # --- 4. el RFB por el túnel
    cl1 = Rfb(p_tun)
    w, h = cl1.w, cl1.h
    verificar(cl1.nombre == b"SaguiOS", f"RFB por el túnel: contraseña buena, ServerInit {w}x{h}")
    cl1.set_encodings([16, 5, 0, -223])
    cl1.pedir(False)
    n, rects = cl1.leer_actualizacion()
    qw, qh, qpx = pantalla_qemu(m, "h252-qemu")
    dif = diferentes(cl1.rgb(), qpx)
    print(f"    píxeles distintos de la pantalla de QEMU: {dif} de {w * h} (cursor y reloj); {n} bytes")
    verificar((qw, qh) == (w, h) and dif < w * h * 0.02, "pantalla completa por el túnel igual a la de QEMU (menos del 2 % distinto)")
    tam_zrle = n

    # teclado: una orden en el Terminal
    esperar_ok = base.esperar_log
    marca = len(m.serial())
    escribir(cl1, "x=ok; echo tunel-$x")
    enter(cl1)
    esperar_ok(m, "tunel-ok", 20, marca)
    verificar(True, "teclado por el túnel: «x=ok; echo tunel-$x» da tunel-ok en el Terminal")

    # Ctrl+C a un proceso del Terminal
    marca = len(m.serial())
    escribir(cl1, "sleep 300")
    enter(cl1)
    time.sleep(2)
    cl1.tecla(0xFFE3, True)     # Control
    cl1.tecla(ord("c"), True)
    cl1.tecla(ord("c"), False)
    cl1.tecla(0xFFE3, False)
    time.sleep(1)
    marca = len(m.serial())
    escribir(cl1, "x=ok; echo ctrlc-$x")
    enter(cl1)
    esperar_ok(m, "ctrlc-ok", 20, marca)
    verificar(True, "Ctrl+C por el túnel corta «sleep 300»: el Terminal vuelve al prompt y ejecuta la orden siguiente")
    ps = m.comando("ps")
    verificar("sleep" not in ps, "y el proceso sleep ya no existe (ps)")

    # abrir Archivos con el teclado, incremental, comparar
    cl1.pedir(False)
    cl1.leer_actualizacion()
    time.sleep(1)
    cl1.pedir(True)
    try:
        cl1.leer_actualizacion(3)
    except (socket.timeout, TimeoutError):
        pass
    marca = len(m.serial())
    escribir(cl1, "arquivos /tmp")
    enter(cl1)
    time.sleep(3)
    total_n = 0
    cubierto = 0
    for _ in range(8):
        cl1.pedir(True)
        try:
            n, rects = cl1.leer_actualizacion(3)
            total_n += n
            cubierto += sum(r[2] * r[3] for r in rects)
        except (socket.timeout, TimeoutError):
            break
    print(f"    incremental tras abrir Archivos por el teclado remoto: {total_n} bytes, {cubierto} píxeles de {w * h}")
    verificar(0 < total_n < tam_zrle, f"la actualización incremental ({total_n} B) es menor que la pantalla entera ({tam_zrle} B)")
    time.sleep(1)
    cl1.pedir(True)
    try:
        cl1.leer_actualizacion(3)
    except (socket.timeout, TimeoutError):
        pass
    qw, qh, qpx = pantalla_qemu(m, "h252-qemu2")
    dif = diferentes(cl1.rgb(), qpx)
    verificar(dif < w * h * 0.02, f"tras lo incremental, la imagen del cliente coincide con QEMU ({dif} píxeles distintos)")

    # un clic que cambia el foco: Archivos tiene el foco; se teclea; se hace clic en el Terminal; se teclea otra vez
    ar = ventana(m, "Archivos")
    te = ventana(m, "Terminal")
    verificar(ar and te, f"el kernel anota las ventanas Terminal {te} y Archivos {ar}")
    marca = len(m.serial())
    escribir(cl1, "x=ok; echo sinfoco-$x")
    enter(cl1)
    time.sleep(2)
    verificar("sinfoco-ok" not in m.serial()[marca:], "con Archivos en primer plano, lo tecleado no llega al Terminal")
    # un punto del Terminal que Archivos no tape
    tw, th, tx, ty = te
    aw, ah, ax, ay = ar
    punto = None
    for dx, dy in ((10, th // 2), (tw // 2, 10), (tw - 3, th // 2), (tw // 2, th - 3), (10, th - 10)):
        px, py = tx + dx, ty + dy
        if not (ax <= px < ax + aw and ay <= py < ay + ah):
            punto = (px, py)
            break
    verificar(punto is not None, f"hay un punto del Terminal sin tapar para el clic {punto}")
    marca = len(m.serial())
    cl1.raton(0, *punto)
    time.sleep(0.4)
    cl1.raton(1, *punto)
    time.sleep(0.3)
    cl1.raton(0, *punto)
    esperar_ok(m, f"botao esquerdo apertado em ({punto[0]},{punto[1]})", 10, marca)
    time.sleep(1)
    marca = len(m.serial())
    escribir(cl1, "x=ok; echo confoco-$x")
    enter(cl1)
    esperar_ok(m, "confoco-ok", 20, marca)
    verificar(True, f"un clic remoto en {punto} devuelve el foco al Terminal: ahora sí llega la orden")
    # cerrar Archivos con Alt+F4 remoto (queda la ventana que tiene el foco: el Terminal): se enfoca Archivos antes
    aw, ah, ax, ay = ventana(m, "Archivos")
    pa = (ax + aw // 2, ay + ah + 22)    # la franja inferior de Archivos (con marco y título) que el Terminal no tapa
    verificar(not (tx <= pa[0] < tx + tw and ty <= pa[1] < ty + th + 26), f"punto de Archivos sin tapar {pa}")
    marca = len(m.serial())
    cl1.raton(0, *pa)
    time.sleep(0.3)
    cl1.raton(1, *pa)
    time.sleep(0.3)
    cl1.raton(0, *pa)
    time.sleep(1)
    cl1.tecla(0xFFE9, True)     # Alt
    cl1.tecla(0xFFC1, True)     # F4
    cl1.tecla(0xFFC1, False)
    cl1.tecla(0xFFE9, False)
    time.sleep(2)
    m.captura("h252-vnc-tunel", guardar=False)
    print("    registro tras el clic y Alt+F4:", [l for l in m.serial()[marca:].splitlines() if "GUI" in l or "MOUSE" in l or "VNC" in l or "INYEC" in l][-12:])
    verificar(ventana_cerrada(m, "Archivos"), "Alt+F4 remoto cierra Archivos (la ventana con el foco)")
    marca = len(m.serial())
    escribir(cl1, "x=ok; echo final-$x")
    enter(cl1)
    esperar_ok(m, "final-ok", 20, marca)
    verificar(True, "y el Terminal sigue vivo y con el foco tras cerrar Archivos")

    # --- 5. cerrar el ssh: el VNC se corta limpio y el vncd sigue
    marca = len(m.serial())
    t.fechar()
    cl1.s.settimeout(30)
    try:
        # puede haber una actualización en vuelo: se lee hasta el cierre
        for _ in range(5):
            cl1.pedir(True)
            cl1.leer_actualizacion(10)
        cerrado = False
    except (EOFError, ConnectionError, OSError):
        cerrado = True
    verificar(cerrado, "al cerrar el ssh, la conexión VNC se corta (EOF o reinicio, sin colgarse)")
    cl1.cerrar()
    time.sleep(3)
    s = m.serial()[marca:]
    verificar("PANICO" not in s.upper() and "EXCEPCION" not in s.upper(), "sin pánico del kernel al cortar el túnel")
    marca = len(m.serial())
    t = Tunel(cl, ["localhost:5900"])
    tuneis.append(t)
    time.sleep(2)
    cl2 = Rfb(t.portas[0])
    cl2.set_encodings([16, 5, 0, -223])
    cl2.pedir(False)
    n, rects = cl2.leer_actualizacion()
    verificar(cl2.w == w and n > 0, "tras cortar, un túnel nuevo y un cliente nuevo entran: el vncd sigue aceptando")
    qw, qh, qpx = pantalla_qemu(m, "h252-qemu3")
    dif = diferentes(cl2.rgb(), qpx)
    verificar(dif < w * h * 0.02, f"y la pantalla coincide con la de QEMU ({dif} píxeles distintos)")
    cl2.cerrar()
    time.sleep(1)
    log = m.comando("cat /logs/vncd.log") + m.comando("cat /tmp/.servicios/vncd.log")
    verificar("autenticado; pantalla" in log, "el registro del vncd anota las conexiones")
    verificar(CLAVE not in log, "la contraseña no sale en el registro del vncd")

    # --- 6. carga del escritorio con y sin visor conectado
    m.captura("h252-antes-bench", guardar=False)
    sin, con = [], []
    visor_stats = []
    for ronda in range(2):
        sin.append(medir_bench(m))
        v = Visor(t.portas[0])
        visores.append(v)
        v.start()
        time.sleep(2)
        con.append(medir_bench(m))
        visor_stats.append((v.actualizaciones, v.bytes))
        verificar(v.error is None, f"el visor VNC siguió conectado durante bench (ronda {ronda + 1}, {v.actualizaciones} actualizaciones)")
        v.fin()
        time.sleep(1)
    print("    CARGA (por el túnel ssh, 1 visor pidiendo cuadros sin parar):")
    for nom, i, unidad in (("memcpy 1 MB", 0, "MB/s"), ("bench gui redibujar+mostrar", 1, "cuadros/s"), ("bench gui solo mostrar", 2, "cuadros/s")):
        a = statistics.mean(x[i] for x in sin)
        b = statistics.mean(x[i] for x in con)
        print(f"      {nom}: sin visor {a:.1f}, con visor {b:.1f} {unidad} ({100 * b / a:.0f} %)  [sin {[x[i] for x in sin]}, con {[x[i] for x in con]}]")
    print(f"      el visor recibió {[(a, b // 1024) for a, b in visor_stats]} (actualizaciones, KB) en cada ronda")
    a, b = statistics.mean(x[1] for x in sin), statistics.mean(x[1] for x in con)
    verificar(b > a / 2, f"con un visor conectado, bench gui no es más del doble de lento ({b:.1f} frente a {a:.1f} cuadros/s)")
    a, b = statistics.mean(x[0] for x in sin), statistics.mean(x[0] for x in con)
    verificar(b > a / 2, f"con un visor conectado, memcpy no es más del doble de lento ({b:.0f} frente a {a:.0f} MB/s)")

    # --- 7. visor real del anfitrión
    hay = [v for v in VISORES if shutil.which(v)]
    if hay:
        print(f"  AVISO: hay visores VNC en el anfitrión ({hay}); la prueba no los lanza (necesitan pantalla)")
    else:
        print(f"  NOTA: ningún visor VNC real en el anfitrión ({', '.join(VISORES)}): solo el cliente RFB de la prueba")


def main():
    os.makedirs(TRAB, exist_ok=True)
    for prog in ("ssh", "ssh-keygen"):
        if not shutil.which(prog):
            print(f"FALHOU: falta {prog} (OpenSSH) en el anfitrión")
            return 1
    maquina()
    print("H252 correcto")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
