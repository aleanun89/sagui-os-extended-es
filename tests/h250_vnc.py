#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H250 (D252, mejora 097) - servidor de escritorio remoto VNC: vncd.

En el anfitrión (tests/vnc_unidad.c):
  1. DES con vectores conocidos y comparado con la biblioteca «cryptography» (TripleDES con la
     clave repetida = DES) en datos al azar; la respuesta al reto del RFB;
  2. el deflate continuo, descomprimido trozo a trozo con un solo zlib.decompressobj de Python
     (el flujo conserva el diccionario entre rectángulos);
  3. las codificaciones Raw, Hextile y ZRLE, con imágenes de prueba y con 5 formatos de píxel
     (32, 16 y 8 bpp, little y big endian, colores en otros bits), decodificadas aquí en Python.

En QEMU (red de usuario con hostfwd al puerto 5900), con un cliente RFB escrito en esta prueba:
  - desactivado de serie; «vncd --clave» guarda la contraseña;
  - con la configuración por defecto, desde fuera (la conexión de hostfwd llega de 10.0.2.2) no se
    acepta ni el saludo; con escuchar=red-local, sí;
  - saludo RFB 3.8, 3.7 y 3.3; un cliente que no habla RFB se corta; solo se ofrece «VNC
    Authentication» (la opción «None» no existe sin seguridad=ninguna);
  - contraseña buena y mala; tras tres fallos la dirección queda bloqueada un rato;
    el máximo de dos clientes a la vez;
  - un FramebufferUpdate completo que coincide con la pantalla de QEMU; Raw, Hextile y ZRLE dan
    la misma imagen; SetPixelFormat de 16 y 8 bpp;
  - una actualización incremental tras abrir una ventana trae solo lo cambiado y deja la imagen
    igual que la pantalla; sin cambios, no llega nada;
  - teclado (con Mayús) y ratón de un cliente llegan al terminal y a la pantalla (SIS_INYECTAR, D251);
    ClientCutText se descarta;
  - rendimiento: cuadros por segundo y KB por cuadro con la pantalla cambiando (ZRLE);
  - la contraseña no sale en el registro; al parar el servicio, el puerto se cierra.
"""
import os
import random
import re
import shutil
import socket
import struct
import subprocess
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA, FalhaTeste  # noqa: E402
from rfb_cliente import (PF, NATIVO, FORMATOS, Rfb, banner, pantalla_qemu, diferentes, vnc_respuesta,  # noqa: E402,F401
                         dec_raw, dec_hextile, dec_zrle, CLAVE)

TRAB = os.path.join(SAIDA, "h250")
DADOS_MB = 16


def puerto_libre():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


# ------------------------------------------------------------ anfitrión: DES, deflate y codificaciones

def imagenes_prueba():
    """Imágenes de formas variadas: lisa, degradado, ruido, texto-like, bloques con pocos colores."""
    rnd = random.Random(250)
    out = []
    for (w, h) in ((16, 16), (37, 21), (130, 70), (200, 150)):
        out.append(("lisa", w, h, [0x336699] * (w * h)))
        out.append(("degradado", w, h, [((x * 255 // w) << 16) | ((y * 255 // h) << 8) | ((x + y) & 255) for y in range(h) for x in range(w)]))
        out.append(("ruido", w, h, [rnd.getrandbits(24) for _ in range(w * h)]))
        cols = [0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0x808080]
        img = [0xE0E0E0] * (w * h)
        for _ in range(w * h // 40):   # rectángulos de pocos colores, como una interfaz
            rw, rh, rx, ry = rnd.randrange(1, 12), rnd.randrange(1, 8), rnd.randrange(w), rnd.randrange(h)
            c = rnd.choice(cols)
            for y in range(ry, min(h, ry + rh)):
                for x in range(rx, min(w, rx + rw)):
                    img[y * w + x] = c
        out.append(("interfaz", w, h, img))
        # muchas rachas largas y 17+ colores (sin paleta, con RLE)
        img = []
        for y in range(h):
            c = rnd.getrandbits(24)
            img += [c] * w
        out.append(("franjas", w, h, img))
        pal20 = [rnd.getrandbits(24) for _ in range(20)]
        out.append(("20 colores", w, h, [rnd.choice(pal20) for _ in range(w * h)]))
        pal3 = [rnd.getrandbits(24) for _ in range(3)]
        out.append(("3 colores", w, h, [pal3[(x // 3 + y // 5) % 3] for y in range(h) for x in range(w)]))
    return out


def unidad():
    os.makedirs(TRAB, exist_ok=True)
    exe = os.path.join(TRAB, "vnc_unidad")
    fontes = [os.path.join(RAIZ, "tests", "vnc_unidad.c"), os.path.join(RAIZ, "userland", "libsagui", "des.c"),
              os.path.join(RAIZ, "userland", "libsagui", "deflado_continuo.c")]
    base = ["gcc", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "userland", "inc"),
            "-I", os.path.join(RAIZ, "kernel", "inc"), "-I", os.path.join(RAIZ, "userland", "programas")]
    r = subprocess.run(base + ["-O2", "-o", exe] + fontes, capture_output=True, text=True)
    verificar(r.returncode == 0 and "warning" not in r.stderr, "vnc_unidad.c compila sin avisos" + r.stderr[:2000])
    r = subprocess.run([exe], capture_output=True, text=True, timeout=120)
    n = re.search(r"vnc_unidad: (\d+)/(\d+) comprobaciones correctas", r.stdout)
    verificar(r.returncode == 0 and n and n.group(1) == n.group(2),
              f"DES (vectores conocidos), clave y respuesta del RFB, teclas, formatos y agrupación: {r.stdout.strip()[-300:]}")
    exe2 = exe + "-asan"
    r = subprocess.run(base + ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all", "-o", exe2] + fontes,
                       capture_output=True, text=True)
    asan = r.returncode == 0
    if not asan:
        print("  AVISO: sin los sanitizadores de gcc en este anfitrión; se omiten")
    elif subprocess.run([exe2], capture_output=True, text=True, timeout=300).returncode != 0:
        raise FalhaTeste("vnc_unidad con -fsanitize=address,undefined falla")

    def lanzar(e):
        return subprocess.Popen([e, "ordenes"], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)

    p = lanzar(exe2 if asan else exe)

    def o(linea):
        p.stdin.write(linea + "\n")
        p.stdin.flush()
        return p.stdout.readline().strip()

    # --- DES contra «cryptography» y la respuesta del RFB
    try:
        from cryptography.hazmat.primitives.ciphers import Cipher, modes
        from cryptography.hazmat.primitives.ciphers.algorithms import TripleDES
        hay_cripto = True
    except ImportError:
        hay_cripto = False
        print("  AVISO: sin «cryptography»: se omite la comparación cruzada del DES")
    if hay_cripto:
        rnd = random.Random(7)
        for i in range(40):
            k, b = bytes(rnd.getrandbits(8) for _ in range(8)), bytes(rnd.getrandbits(8) for _ in range(8))
            ref = Cipher(TripleDES(k * 3), modes.ECB()).encryptor().update(b)
            if o(f"des {k.hex()} {b.hex()}") != ref.hex():
                raise FalhaTeste(f"DES distinto de cryptography (caso {i})")
            if o(f"des {k.hex()} {ref.hex()} d") != b.hex():
                raise FalhaTeste(f"DES descifrado mal (caso {i})")
        verificar(True, "DES igual que la biblioteca cryptography (40 casos al azar, cifrar y descifrar)")
        for pw in ("a", "secreto", "12345678", "passwordXYZ"):
            reto = os.urandom(16)
            ref = vnc_respuesta(pw, reto)
            if o(f"resp {pw.encode().hex()} {reto.hex()}") != ref.hex():
                raise FalhaTeste(f"la respuesta del RFB para {pw!r} no coincide con la de la prueba")
        verificar(True, "la respuesta al reto del RFB coincide con la del cliente de la prueba (clave con los bits invertidos)")

    # --- deflate continuo contra zlib de Python
    rnd = random.Random(11)
    d = zlib.decompressobj()
    base_ = bytes(rnd.getrandbits(8) for _ in range(3000))
    tin = tout = 0
    for i in range(80):
        k = rnd.randrange(5)
        if k == 0:
            ch = bytes(rnd.randrange(4) for _ in range(rnd.randrange(1, 5000)))
        elif k == 1:
            ch = base_[rnd.randrange(500):][:rnd.randrange(1, 2500)]
        elif k == 2:
            ch = bytes(rnd.getrandbits(8) for _ in range(rnd.randrange(1, 700)))
        elif k == 3:
            ch = base_[:200] * rnd.randrange(1, 60)
        else:
            ch = b"\x00" * rnd.randrange(1, 70000)
        out = bytes.fromhex(o("z " + ch.hex()))
        if d.decompress(out) != ch:
            raise FalhaTeste(f"el deflate continuo no se descomprime igual (trozo {i}, tipo {k})")
        tin += len(ch)
        tout += len(out)
    big = base_ * 30 + bytes(rnd.getrandbits(8) for _ in range(40000))
    if d.decompress(bytes.fromhex(o("z " + big.hex()))) != big:
        raise FalhaTeste("el deflate continuo falla con un trozo mayor que la ventana")
    verificar(True, f"deflate continuo: 81 trozos en un solo flujo zlib, descomprimidos con un único decompressobj ({tin} -> {tout} bytes)")
    # la ventana se aprovecha: repetir lo ya enviado en otro trozo sale mucho más corto
    o("zreiniciar")
    dd = zlib.decompressobj()
    mat = bytes(rnd.getrandbits(8) for _ in range(2000))
    a = bytes.fromhex(o("z " + mat.hex()))
    b2 = bytes.fromhex(o("z " + mat.hex()))
    verificar(dd.decompress(a) == mat and dd.decompress(b2) == mat and len(b2) < len(a) // 5,
              f"el diccionario se mantiene entre trozos (el segundo ocupa {len(b2)} bytes y el primero {len(a)})")

    # --- codificaciones contra los decodificadores de Python
    casos = 0
    for nombre, pf in FORMATOS:
        for (nom, w, h, img) in imagenes_prueba():
            esperado = [pf.valor(c) for c in img]
            hx = "".join("%08x" % c for c in img)
            for enc, dec in (("raw", dec_raw), ("hextile", dec_hextile), ("zrle", dec_zrle)):
                r = o(f"enc {enc} {w} {h} {pf.bpp} {pf.big} {pf.max[0]} {pf.max[1]} {pf.max[2]} {pf.desp[0]} {pf.desp[1]} {pf.desp[2]} {hx}")
                datos = bytes.fromhex(r)
                try:
                    got = dec(datos, w, h, pf)
                except (AssertionError, IndexError) as e:
                    raise FalhaTeste(f"{enc} {nombre} {nom} {w}x{h}: no se decodifica ({e})")
                if got != esperado:
                    bad = sum(1 for a_, b_ in zip(got, esperado) if a_ != b_)
                    raise FalhaTeste(f"{enc} {nombre} {nom} {w}x{h}: {bad} píxeles distintos")
                casos += 1
    verificar(True, f"Raw, Hextile y ZRLE decodificados en Python igual que la imagen original ({casos} casos: 6 formatos de píxel x {len(imagenes_prueba())} imágenes)")
    # tamaños: la compresión sirve de algo en una imagen de interfaz
    nom, w, h, img = [i for i in imagenes_prueba() if i[0] == "interfaz"][-1]
    hx = "".join("%08x" % c for c in img)
    tam = {e: len(bytes.fromhex(o(f"enc {e} {w} {h} 32 0 255 255 255 16 8 0 {hx}"))) for e in ("raw", "hextile", "zrle")}
    verificar(tam["hextile"] < tam["raw"] and tam["zrle"] < tam["raw"],
              f"imagen de interfaz {w}x{h}: raw {tam['raw']}, hextile {tam['hextile']}, zrle (sin zlib) {tam['zrle']} bytes")
    p.stdin.close()
    p.wait(timeout=20)


def esperar_log(m, texto, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial()[desde:]:
            return
        time.sleep(0.2)
    raise FalhaTeste(f"no apareció en el registro: {texto!r}")


def construir_imagen():
    b = os.path.join(RAIZ, "build")
    raiz = os.path.join(TRAB, "rootfs")
    shutil.rmtree(raiz, ignore_errors=True)
    shutil.copytree(os.path.join(b, "rootfs"), raiz)
    sgfs = os.path.join(TRAB, "h250.sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), raiz, sgfs], check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(TRAB, "h250.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--dados-mb", str(DADOS_MB)],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def maquina():
    img = construir_imagen()
    porta = puerto_libre()
    extras = ["-nic", f"user,model=e1000,hostfwd=tcp:127.0.0.1:{porta}-:5900"]
    with Maquina("h250", "ide", imagem=img, snapshot=False, extras=extras) as m:
        m.esperar_serial("sagui:/$", 150)
        time.sleep(4)
        esperar_log(m, "10.0.2.15 (DHCP)", 40)
        _prueba(m, porta)


def _prueba(m, porta):
    # --- de serie: parado, sin nadie en el puerto
    out = m.comando("servicio listar")
    verificar(re.search(r"^vncd\s+parado", out, re.M), "servicio listar: vncd está parado de serie")
    verificar(banner(porta) == b"", "con el servicio parado no hay nadie en el puerto 5900")
    out = m.comando("vncd --ayuda")
    verificar("ssh -L 5900:localhost:5900" in out, "vncd --ayuda explica el túnel ssh -L")

    # --- sin contraseña, nadie entra
    out = m.comando("servicio iniciar vncd", 30)
    esperar_log(m, "vncd: escuchando en el puerto 5900", 30)
    verificar("solo desde esta máquina" in m.serial(), "por defecto escucha solo desde esta máquina")
    verificar("no hay contraseña" in m.serial(), "avisa de que no hay contraseña")
    marca = len(m.serial())
    verificar(banner(porta) == b"", "con la configuración por defecto, desde fuera (10.0.2.2) no se acepta ni el saludo")
    esperar_log(m, "rechazada: no viene de esta máquina", 10, marca)
    out = m.comando("servicio parar vncd", 30)

    # --- contraseña y red local
    out = m.comando("vncd --clave " + CLAVE)
    verificar("contraseña guardada" in out, "vncd --clave guarda la contraseña")
    m.comando("echo escuchar=red-local > /home/.config/vncd.conf")
    m.comando("echo bloqueo_fallos=3 >> /home/.config/vncd.conf")
    m.comando("echo bloqueo_segundos=10 >> /home/.config/vncd.conf")
    m.comando("echo max_clientes=2 >> /home/.config/vncd.conf")
    m.comando("echo fps_max=15 >> /home/.config/vncd.conf")
    m.comando("echo indicador=no >> /home/.config/vncd.conf")   # D273: la ventana indicadora cambiaría la pantalla entre capturas
    marca = len(m.serial())
    m.comando("servicio iniciar vncd", 30)
    esperar_log(m, "vncd: escuchando en el puerto 5900", 30, marca)
    verificar("(red local)" in m.serial()[marca:], "con escuchar=red-local acepta la red local")
    b = banner(porta)
    verificar(b == b"RFB 003.008\n", f"el servidor saluda con RFB 003.008 ({b!r})")
    time.sleep(1)

    # --- lo que no es RFB
    c = socket.create_connection(("127.0.0.1", porta), timeout=10)
    c.recv(12)
    c.sendall(b"GET / HTTP/1.0\r\n\r\n")
    c.settimeout(10)
    try:
        r = c.recv(100)
    except OSError:
        r = b""
    verificar(r == b"", f"lo que no habla RFB se corta ({r!r})")
    c.close()

    # --- tipos de seguridad, versiones y contraseña
    cl = Rfb(porta)
    verificar(cl.tipos == [2], f"solo se ofrece «VNC Authentication» (tipos {cl.tipos})")
    verificar(cl.nombre == b"SaguiOS", "ServerInit con el nombre SaguiOS")
    w, h = cl.w, cl.h
    verificar(w >= 640 and h >= 480 and cl.pf_servidor.bpp == 32, f"ServerInit: pantalla {w}x{h}, 32 bpp, formato {cl.pf_servidor.desp}")
    cl.cerrar()
    time.sleep(0.5)
    for ver in (b"003.007", b"003.003"):
        c37 = Rfb(porta, version=ver)
        verificar(c37.w == w and c37.h == h, f"RFB {ver.decode()}: autenticación y ServerInit correctos")
        c37.cerrar()
        time.sleep(0.5)

    # --- pantalla completa igual a la de QEMU, con las tres codificaciones y otros formatos
    cl = Rfb(porta)
    cl.set_encodings([16, 5, 0, -223])
    cl.pedir(False)
    n, rects = cl.leer_actualizacion()
    verificar(len(rects) >= 1 and all(r[4] == 16 for r in rects), f"con ZRLE preferida, el cliente recibe ZRLE ({len(rects)} rectángulo(s), {n} bytes)")
    qw, qh, qpx = pantalla_qemu(m, "h250-qemu")
    verificar((qw, qh) == (w, h), f"la pantalla de QEMU es {qw}x{qh}")
    dif = diferentes(cl.rgb(), qpx)
    print(f"    píxeles distintos de la pantalla de QEMU: {dif} de {w * h} (cursor y reloj)")
    verificar(dif < w * h * 0.02, "el FramebufferUpdate completo coincide con la pantalla de QEMU (menos del 2 % distinto)")
    ref_zrle = cl.rgb()
    tam_zrle = n
    resultados = {}
    for nombre, codigos in (("Hextile", [5, 0]), ("Raw", [0])):
        cl.set_encodings(codigos)
        cl.pedir(False)
        n, rects = cl.leer_actualizacion()
        resultados[nombre] = n
        d = diferentes(cl.rgb(), ref_zrle)
        verificar(d < w * h * 0.005, f"{nombre} da la misma imagen que ZRLE ({d} píxeles distintos, {n} bytes)")
    print(f"    tamaño de la pantalla entera: ZRLE {tam_zrle} B, Hextile {resultados['Hextile']} B, Raw {resultados['Raw']} B")
    verificar(tam_zrle < resultados["Hextile"] < resultados["Raw"], "ZRLE < Hextile < Raw en bytes para el escritorio")
    # otros formatos de píxel
    for nombre, pf in (("16 bpp RGB565", PF(16, 0, (31, 63, 31), (11, 5, 0))), ("8 bpp BGR233", PF(8, 0, (7, 7, 3), (0, 3, 6)))):
        cl.set_encodings([16, 5, 0])
        cl.set_pf(pf)
        cl.pedir(False)
        n, rects = cl.leer_actualizacion()
        esperado = [pf.valor(c) for c in ref_zrle]
        d = diferentes(cl.fb, esperado)
        verificar(d < w * h * 0.005, f"SetPixelFormat {nombre}: la imagen es la misma convertida ({d} píxeles distintos, {n} bytes)")
    cl.set_pf(NATIVO)
    cl.set_encodings([16, 5, 0, -223])

    # --- rendimiento: la pantalla cambia (un listado largo en el terminal)
    cl2 = Rfb(porta)
    cl2.set_encodings([16, 5, 0, -223])
    cl2.pedir(False)
    cl2.leer_actualizacion()
    m.digitar("yes SaguiOS-VNC-prueba-de-rendimiento\n", 0.03)
    time.sleep(1)
    t0 = time.time()
    nup = nbytes = 0
    while time.time() - t0 < 10:
        cl2.pedir(True)
        cl2.s.settimeout(2)
        try:
            n, rects = cl2.leer_actualizacion(2)
            nup += 1
            nbytes += n
        except (socket.timeout, TimeoutError):
            pass
    dt = time.time() - t0
    print(f"    rendimiento (ZRLE, {w}x{h}, texto que se desplaza en el terminal): {nup / dt:.1f} actualizaciones/s, {nbytes / max(nup, 1) / 1024:.1f} KB por actualización")
    verificar(nup / dt <= 16.5, f"el ritmo no pasa de fps_max=15 ({nup / dt:.1f}/s)")
    verificar(nup >= 10, f"con la pantalla cambiando llegan actualizaciones ({nup} en {dt:.0f} s)")
    m.tecla("ctrl-c", 0.3)
    cl2.cerrar()
    time.sleep(1)

    # --- entrada: teclado y ratón de verdad (el terminal aún tiene el foco)
    esperar_log(m, "permiso de inyección de teclado y ratón concedido", 5)
    ci = Rfb(porta)
    marca = len(m.serial())
    ci.tecla(0xFFE1, True)    # Mayús
    ci.tecla(0x24, True)      # $ (Mayús+4 en el teclado)
    ci.tecla(0x24, False)
    ci.tecla(0xFFE1, False)
    ci.tecla(0xFF08, True)      # Retroceso (borra el $)
    ci.tecla(0xFF08, False)
    for ks in "x=ok; echo vnc-" + "$x":
        if ks == "$":
            ci.tecla(0xFFE1, True)
            ci.tecla(ord(ks), True)
            ci.tecla(ord(ks), False)
            ci.tecla(0xFFE1, False)
        else:
            ci.tecla(ord(ks), True)
            ci.tecla(ord(ks), False)
        time.sleep(0.05)
    ci.tecla(0xFF0D, True)
    ci.tecla(0xFF0D, False)
    esperar_log(m, "vnc-ok", 20, marca)
    verificar(True, "las teclas del cliente (con Mayús, Retroceso y Enter) llegan al terminal: «x=ok; echo vnc-$x» da vnc-ok")
    marca = len(m.serial())
    ci.raton(0, 500, 400)
    time.sleep(0.3)
    ci.raton(1, 500, 400)
    time.sleep(0.3)
    ci.raton(0, 500, 400)
    esperar_log(m, "botao esquerdo apertado em (500,400)", 10, marca)
    verificar(True, "el clic del cliente llega a la pantalla en la posición pedida (500,400)")
    ci.cerrar()
    time.sleep(1)

    # --- incremental: sin cambios no llega nada; al abrir una ventana llega solo lo cambiado
    cl.pedir(False)
    cl.leer_actualizacion()
    time.sleep(1.5)
    cl.pedir(True)
    # el escritorio nunca está del todo quieto (el cursor del terminal parpadea, el reloj): lo que
    # llegue con la pantalla "quieta" tiene que ser un parche pequeño, no la pantalla entera
    cl.s.settimeout(3)
    try:
        n, rects = cl.leer_actualizacion(3)
        area = sum(r[2] * r[3] for r in rects)
        print(f"    con la pantalla quieta llegó un parche: {len(rects)} rectángulo(s), {area} píxeles, {n} bytes")
        verificar(area < w * h * 0.10 and n < 8000, "una petición incremental con la pantalla quieta solo recibe un parche pequeño (cursor, reloj)")
    except (socket.timeout, TimeoutError):
        verificar(True, "una petición incremental con la pantalla quieta no recibe nada")
    cl.s.settimeout(30)
    marca = len(m.serial())
    cl.pedir(True)
    m.comando("arquivos /tmp")
    time.sleep(2)
    n, rects = cl.leer_actualizacion(30)
    # puede llegar en varias tandas mientras la ventana se dibuja: se sigue hasta que se aquieta
    cubierto = sum(r[2] * r[3] for r in rects)
    total_n = n
    for _ in range(6):
        cl.pedir(True)
        cl.s.settimeout(3)
        try:
            n, rects = cl.leer_actualizacion(3)
            cubierto += sum(r[2] * r[3] for r in rects)
            total_n += n
        except (socket.timeout, TimeoutError):
            break
    cl.s.settimeout(30)
    print(f"    incremental tras abrir Archivos: {total_n} bytes, {cubierto} píxeles de {w * h}")
    verificar(0 < total_n < tam_zrle, f"la actualización incremental ({total_n} B) es menor que la pantalla entera ({tam_zrle} B)")
    time.sleep(1)
    cl.pedir(True)
    try:
        cl.leer_actualizacion(3)
    except (socket.timeout, TimeoutError):
        pass
    qw, qh, qpx = pantalla_qemu(m, "h250-qemu2")
    dif = diferentes(cl.rgb(), qpx)
    verificar(dif < w * h * 0.02, f"tras las actualizaciones incrementales la imagen del cliente coincide con la pantalla ({dif} píxeles distintos)")

    # --- ClientCutText grande: se descarta sin romper la conexión
    cl.cut_text(b"x" * 300000)
    cl.pedir(False)
    cl.leer_actualizacion()
    verificar(True, "un ClientCutText de 300 KB se descarta sin romper la conexión")
    cl.cerrar()
    time.sleep(1)

    # --- el máximo de clientes (2) y el bloqueo por fallos
    a, b2 = Rfb(porta), Rfb(porta)
    time.sleep(0.5)
    marca = len(m.serial())
    tercero = banner(porta)
    verificar(tercero == b"", "el tercer cliente a la vez (max_clientes=2) se rechaza")
    esperar_log(m, "demasiados clientes a la vez", 10, marca)
    a.cerrar()
    b2.cerrar()
    time.sleep(2)
    marca = len(m.serial())
    for i in range(3):
        try:
            Rfb(porta, clave="mala%d" % i)
            raise FalhaTeste("una contraseña mala no debe entrar")
        except PermissionError as e:
            if i == 0:
                verificar(b"fallida" in bytes(e.args[0]) or b"Autentic" in bytes(e.args[0]), f"contraseña mala: el servidor da el motivo ({e.args[0]!r})")
    time.sleep(1)
    verificar("contraseña incorrecta" in m.serial()[marca:], "el registro anota la contraseña incorrecta")
    verificar(banner(porta) == b"", "tras tres fallos seguidos, la dirección queda bloqueada")
    esperar_log(m, "bloqueada 10 s", 10, marca)
    time.sleep(11)
    ok = Rfb(porta)
    verificar(ok.w == w, "pasado el bloqueo, la contraseña buena entra otra vez")
    ok.cerrar()
    time.sleep(1)

    m.tecla("alt-f4", 0.5)   # cierra Archivos: el terminal recupera el foco
    time.sleep(2)
    log = m.serial()
    verificar(CLAVE not in log.replace("vncd --clave " + CLAVE, "").replace("echo", ""), "la contraseña no aparece en el registro")
    r = m.comando("cat /logs/vncd.log") + m.comando("cat /tmp/.servicios/vncd.log")
    verificar("autenticado; pantalla" in r, "el registro /logs/vncd.log anota las conexiones")

    # --- parar
    out = m.comando("servicio parar vncd", 30)
    time.sleep(2)
    verificar(banner(porta) == b"", "al parar el servicio, el puerto se cierra")
    m.captura("H250-vnc", guardar=False)


def principal():
    unidad()
    maquina()
    print("H250 correcto")


if __name__ == "__main__":
    try:
        principal()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
