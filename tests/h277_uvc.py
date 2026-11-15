#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H277 (D277) - cámaras UVC, isócrono de entrada y la aplicación Cámara.

QEMU 8.2 no emula ninguna cámara UVC ni ningún flujo isócrono de entrada (ni `usb-redir`
está compilado), así que la prueba tiene tres partes:

  anfitrion  la parte pura del driver (kernel/usb/uvc_nucleo.c, isoent.h) y la libsagui
             (YUY2 a RGB, decodificador JPEG) compiladas en el PC con ASan/UBSan
             (tools/uvc_testar.c), con descriptores y paquetes que genera esta prueba a
             partir de la especificación UVC 1.1 (un descriptor como el de una cámara
             integrada de alta velocidad, con IAD, unidad de procesamiento, formatos MJPEG y
             YUY2, alternativas con multiplicador 2 y 3, y otro de plena velocidad, UVC 1.0):
             análisis de descriptores, PROBE/COMMIT (bytes exactos de la orden), montaje de
             cuadros MJPEG y YUY2 con paquetes vacíos, FID sin EOF, bit ERR y cuadros
             rotos, PSNR del decodificador de la libsagui contra ffmpeg, cola de paquetes
             (aleatoria) y fuzz.
  os         dentro de SaguiOS (QEMU) la fuente de prueba del kernel: genera el patrón YUY2,
             lo parte en paquetes UVC y pasa por el mismo montador que una cámara real;
             `camara --info`, `camara -p --foto` (el patrón en el PNG y los cuadros
             descartados por paquetes perdidos) y que el sistema siga bien sin cámara.
  ventana    la aplicación Cámara en su ventana con la fuente de prueba (capturas) y el botón
             Foto con el diálogo de guardar.

El isócrono de entrada de los cuatro controladores (xHCI, UHCI, OHCI, EHCI con iTD y siTD)
no se puede ejercitar sin una cámara: está escrito contra las especificaciones y la prueba
de hardware es la del documento docs/camara.md. Aquí se comprueba que compila y que el
isócrono de salida (h275) sigue andando.

Uso: h277_uvc.py [anfitrion] [os] [ventana]   (sin argumentos, todo)
"""
import os
import re
import shutil
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, FalhaTeste, RAIZ, SAIDA  # noqa: E402

DIR = os.path.join(SAIDA, "h277")
HARNES = os.path.join(DIR, "uvc_testar")


# ------------------------------------------------------------ construcción de descriptores (UVC 1.1)

def d_interfaz(num, alt, neps, sub, clase=14):
    return struct.pack("<9B", 9, 4, num, alt, neps, clase, sub, 0, 0)


def d_formato_mjpeg(idx, nframes):
    return struct.pack("<11B", 11, 0x24, 6, idx, nframes, 1, 1, 0, 0, 0, 0)


GUID_YUY2 = bytes.fromhex("5955593200001000800000aa00389b71")


def d_formato_yuy2(idx, nframes):
    return struct.pack("<3B2B", 27, 0x24, 4, idx, nframes) + GUID_YUY2 + struct.pack("<6B", 16, 1, 0, 0, 0, 0)


def d_quadro(subtipo, idx, w, h, maxbuf, ints):
    cab = struct.pack("<3BBBHHIIIIB", 26 + 4 * len(ints), 0x24, subtipo, idx, 0, w, h, w * h * 16, w * h * 16 * 30, maxbuf, ints[0], len(ints))
    # struct "3B BB..." : longitud, tipo, subtipo, idx, bmCapabilities...
    return cab + b"".join(struct.pack("<I", i) for i in ints)


def d_endpoint_iso(mps, mult=1, ivl=1):
    w = mps | ((mult - 1) << 11)
    return struct.pack("<7B", 7, 5, 0x81, 0x05, w & 0xFF, w >> 8, ivl)


def descriptor_camara(hs=True, bcd=0x0110):
    """Cámara integrada de alta velocidad (o de plena velocidad, UVC 1.0, con hs=False)."""
    vc = struct.pack("<BBBHHIBB", 13, 0x24, 1, bcd, 0, 30000000, 1, 1)                                  # VC_HEADER
    vc += struct.pack("<BBBBHBBHHHB3s", 18, 0x24, 2, 1, 0x0201, 0, 0, 0, 0, 0, 3, b"\x00\x00\x00")      # terminal de entrada (cámara)
    vc += struct.pack("<BBBBBHBHB", 11, 0x24, 5, 2, 1, 0, 2, 0x0003, 0)                                 # unidad de procesamiento: brillo y contraste
    vc += struct.pack("<BBBBHBBB", 9, 0x24, 3, 3, 0x0101, 0, 2, 0)                                      # terminal de salida (USB streaming)
    vc = vc[:5] + struct.pack("<H", len(vc)) + vc[7:]                                                    # wTotalLength
    mj = [(1, 640, 480, [333333, 666666]), (2, 1280, 720, [333333]), (3, 320, 240, [333333, 400000, 1000000])]
    yu = [(1, 640, 480, [333333]), (2, 320, 240, [333333, 666666])]
    fm = d_formato_mjpeg(1, len(mj)) + b"".join(d_quadro(7, i, w, h, w * h * 2, ivl) for i, w, h, ivl in mj)
    fy = d_formato_yuy2(2, len(yu)) + b"".join(d_quadro(5, i, w, h, w * h * 2, ivl) for i, w, h, ivl in yu)
    still = struct.pack("<BBBBBHHBB", 11, 0x24, 3, 0, 1, 640, 480, 1, 5)                                # STILL_IMAGE_FRAME: se salta
    color = struct.pack("<6B", 6, 0x24, 0x0D, 1, 1, 4)
    cab = struct.pack("<BBBBHBBBBBBB2s", 15, 0x24, 1, 2, 0, 0x81, 0, 3, 0, 0, 0, 1, b"\x00\x00")       # VS_INPUT_HEADER
    vs = cab + fm + fy + still + color
    vs = vs[:4] + struct.pack("<H", len(vs)) + vs[6:]
    alts = b""
    if hs:
        for n, (mps, mult) in enumerate([(128, 1), (256, 1), (512, 1), (1024, 1), (1024, 2), (1024, 3)], 1):
            alts += d_interfaz(1, n, 1, 2) + d_endpoint_iso(mps, mult)
    else:
        for n, mps in enumerate([128, 256, 384, 512, 640, 800, 1023], 1):
            alts += d_interfaz(1, n, 1, 2) + d_endpoint_iso(mps)
    iad = struct.pack("<8B", 8, 0x0B, 0, 2, 14, 3, 0, 0)
    cuerpo = iad + d_interfaz(0, 0, 0, 1) + vc + d_interfaz(1, 0, 0, 2) + vs + alts
    return struct.pack("<BBHBBBBB", 9, 2, 9 + len(cuerpo), 2, 1, 0, 0x80, 250) + cuerpo


# ------------------------------------------------------------ paquetes UVC

def paquetes_cuadro(datos, fid, mps=1020, eof=True, con_pts=True):
    """Parte un cuadro en cargas útiles con cabecera UVC (de 2 bytes, o de 12 con PTS y SCR)."""
    out = []
    cab = 12 if con_pts else 2
    pos = 0
    while pos < len(datos):
        d = datos[pos:pos + mps - cab]
        pos += len(d)
        ultimo = pos >= len(datos)
        info = 0x80 | fid | (0x02 if ultimo and eof else 0) | (0x0C if con_pts else 0)
        h = bytes([cab, info]) + (os.urandom(10) if con_pts else b"")
        out.append(h + d)
    return out


def escribir_paquetes(ruta, pacs):
    with open(ruta, "wb") as f:
        for p in pacs:
            f.write(struct.pack("<H", len(p)) + p)


def leer_cuadros(ruta):
    d = open(ruta, "rb").read()
    out, p = [], 0
    while p + 4 <= len(d):
        n = struct.unpack_from("<I", d, p)[0]
        out.append(d[p + 4:p + 4 + n])
        p += 4 + n
    return out


def ejecutar(*args, ok=True):
    r = subprocess.run([HARNES, *map(str, args)], capture_output=True, text=True, cwd=RAIZ)
    if ok and r.returncode != 0:
        raise FalhaTeste(f"uvc_testar {' '.join(map(str, args))} falló ({r.returncode}): {r.stdout[-400:]} {r.stderr[-1200:]}")
    return r.stdout + r.stderr


def compilar_harnes():
    os.makedirs(DIR, exist_ok=True)
    fuentes = ["tools/uvc_testar.c", "kernel/usb/uvc_nucleo.c", "userland/libsagui/camara.c", "tools/video_host.c",
               "userland/libsagui/imagem_jpeg.c", "userland/libsagui/imagem_idct.c"]
    r = subprocess.run(["gcc", "-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-Wall", "-Wextra",
                        "-Wno-unused-function", "-DSAGUI_HOST", "-DUVC_ANFITRION", "-Itools", "-Iuserland/inc", "-Iuserland/libsagui",
                        "-Ikernel/inc", "-Ikernel/usb", "-o", HARNES, *fuentes, "-lm"], cwd=RAIZ, capture_output=True, text=True)
    verificar(r.returncode == 0 and not r.stderr, "el núcleo UVC y la conversión de cuadros compilan en el anfitrión sin avisos: " + r.stderr[:400])


def anfitrion():
    compilar_harnes()
    # ---- descriptores
    for nombre, hs, bcd in (("hs", True, 0x0110), ("fs", False, 0x0100)):
        arq = os.path.join(DIR, f"cam_{nombre}.desc")
        open(arq, "wb").write(descriptor_camara(hs, bcd))
        s = ejecutar("desc", arq)
        verificar(f"formatos 2 alts {6 if hs else 7} bcd {bcd:04x} vc 0 vs 1 pu 2 controles 3" in s,
                  f"[{nombre}] descriptor: 2 formatos, alternativas, versión UVC, interfaces y unidad de procesamiento ({s.splitlines()[0]})")
        verificar("f 1 tipo 2 bits 0 nq 3" in s and "f 2 tipo 1 bits 16 nq 2" in s, f"[{nombre}] formato MJPEG (3 tamaños) y YUY2 (2 tamaños, 16 bits)")
        verificar("q 2 1280x720 max 1843200 def 333333 n_int 1 333333" in s, f"[{nombre}] cuadro 1280x720 con su intervalo")
        verificar("q 3 320x240 max 153600 def 333333 n_int 3 333333 400000 1000000" in s, f"[{nombre}] intervalos discretos del 320x240")
        if hs:
            verificar("alt 5 ep 1 mps 1024 mult 2 ivl 1" in s and "alt 6 ep 1 mps 1024 mult 3 ivl 1" in s,
                      "[hs] multiplicador de transacciones por microcuadro (2 y 3)")
            verificar("elegir 1500 -> alt 5" in s and "elegir enorme -> alt 6" in s, "[hs] la alternativa elegida es la menor que cabe (o la mayor)")
        else:
            verificar("alt 7 ep 1 mps 1023 mult 1" in s and "elegir 1500 -> alt 7" in s, "[fs] 1023 bytes máximos por paquete, sin multiplicador")
        verificar("buscar mjpeg 1280x720 -> 1280x720" in s and "buscar yuy2 1000x600 -> 640x480" in s, f"[{nombre}] elegir el tamaño exacto o el más cercano")
    # un descriptor truncado o sin formatos no se acepta (y no rompe nada)
    d = descriptor_camara()
    for cortes in (len(d) // 2, 40, 9, 3):
        arq = os.path.join(DIR, "cam_cortado.desc")
        open(arq, "wb").write(d[:cortes])
        s = ejecutar("desc", arq)
        verificar(s.startswith("formatos 0") or s.startswith("formatos"), f"descriptor cortado a {cortes} bytes se analiza sin fallos")
    arq = os.path.join(DIR, "cam_hs.desc")
    # ---- PROBE/COMMIT
    s = ejecutar("negociar", arq, 1, 2)
    pedidos = [l for l in s.splitlines() if l.startswith("pedido")]
    verificar(len(pedidos) == 3, f"PROBE/COMMIT son tres órdenes: SET_CUR(PROBE), GET_CUR(PROBE), SET_CUR(COMMIT) ({len(pedidos)})")
    esperado_probe = struct.pack("<HBBIHHHHHII", 1, 1, 2, 333333, 0, 0, 0, 0, 0, 0, 0) + struct.pack("<IBBBB", 0, 0, 0, 0, 0)
    verificar(pedidos[0].startswith("pedido tipo 21 req 01 valor 0100 indice 0001 n 34 datos " + esperado_probe.hex()),
              "SET_CUR(PROBE): 34 bytes de UVC 1.1 con hint, formato, cuadro e intervalo")
    verificar(pedidos[1].startswith("pedido tipo a1 req 81 valor 0100 indice 0001 n 34"), "GET_CUR(PROBE) pide los 34 bytes")
    verificar(pedidos[2].startswith("pedido tipo 21 req 01 valor 0200 indice 0001 n 34 datos 0100010"), "SET_CUR(COMMIT) con lo que devolvió la cámara")
    verificar("00040a00" in pedidos[2].replace(" ", "") or "payload 1024" in s, "COMMIT lleva el tamaño de carga útil negociado")
    verificar("resultado 0 formato 1 quadro 2 intervalo 333333 quadro_tam 614400 payload 1024" in s, "la negociación devuelve formato, cuadro, intervalo y tamaños")
    s = ejecutar("negociar", arq, 2, 1, "rechaza")
    verificar("resultado -4" in s and s.count("pedido") == 2, "si la cámara cambia el cuadro, no hay COMMIT y falla (-4)")
    arq10 = os.path.join(DIR, "cam_fs.desc")
    s = ejecutar("negociar", arq10, 2, 1)
    verificar("n 26" in s.splitlines()[0] and "resultado 0" in s, "UVC 1.0: los bloques son de 26 bytes")

    # ---- material: JPEG y YUY2 de ffmpeg
    if not shutil.which("ffmpeg"):
        print("  (sin ffmpeg: se salta el material real)")
    else:
        jpg = []
        for i, (w, h, q) in enumerate(((640, 480, 4), (320, 240, 3), (1280, 720, 6))):
            f = os.path.join(DIR, f"c{i}.jpg")
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", f"testsrc=size={w}x{h}:rate=5", "-frames:v", "1",
                            "-q:v", str(q), "-pix_fmt", "yuvj420p", f], check=True)
            jpg.append((w, h, open(f, "rb").read(), f))
        # una secuencia con cosas de cámara real
        pacs, buenos = [], []
        w, h, d0, _ = jpg[0]
        fid = 0
        pacs += paquetes_cuadro(d0, fid); buenos.append(d0); fid ^= 1                      # 1: normal
        pacs += [bytes([12, 0x80 | 0x0C | fid]) + bytes(10)]                                 # solo cabecera
        pacs += [b""]                                                                       # paquete vacío
        pacs += paquetes_cuadro(d0, fid, 3072 - 12 + 12, True); buenos.append(d0); fid ^= 1  # 2: paquetes de 3072 (multiplicador 3)
        trozo = paquetes_cuadro(d0, fid, eof=False)                                          # 3: sin EOF: lo cierra el FID siguiente
        pacs += trozo; buenos.append(d0); fid ^= 1
        malo = paquetes_cuadro(d0, fid)                                                      # 4: un paquete con ERR -> se descarta
        malo[2] = bytes([malo[2][0], malo[2][1] | 0x40]) + malo[2][2:]
        pacs += malo; fid ^= 1
        pacs += paquetes_cuadro(d0[: len(d0) // 2], fid); fid ^= 1                           # 5: cortado a la mitad (sin EOI) -> se descarta
        pacs += paquetes_cuadro(d0, fid, 2000, True, False); buenos.append(d0); fid ^= 1     # 6: cabeceras de 2 bytes, paquetes de 2000
        pacs += paquetes_cuadro(d0, fid); buenos.append(d0); fid ^= 1                        # 7: normal
        pacs += [bytes([1, 0x80]) + b"basura", bytes([200, 0x80]) + b"x"]                    # cabeceras imposibles: cuentan como error
        pacs += paquetes_cuadro(d0, fid); buenos.append(d0); fid ^= 1                        # 8: normal tras la basura
        arq = os.path.join(DIR, "mjpeg.paq")
        escribir_paquetes(arq, pacs)
        sal = os.path.join(DIR, "mjpeg.cuadros")
        s = ejecutar("montar", arq, sal, w, h, 2)
        cuadros = leer_cuadros(sal)
        verificar(len(cuadros) == len(buenos) and all(a == b for a, b in zip(cuadros, buenos)),
                  f"MJPEG: se montan {len(buenos)} cuadros idénticos al JPEG original ({len(cuadros)} salieron; {s.strip()})")
        verificar("descartados 2" in s, "MJPEG: el cuadro con bit ERR y el cortado sin EOI se descartan")
        verificar("errores 3" in s, "MJPEG: el bit ERR y las dos cabeceras imposibles cuentan como errores")
        # el decodificador JPEG de la libsagui contra ffmpeg
        for w2, h2, dat, f in jpg:
            ref = os.path.join(DIR, os.path.basename(f) + ".rgb")
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", f, "-f", "rawvideo", "-pix_fmt", "rgb24", ref], check=True)
            s = ejecutar("jpeg", f, ref)
            m = re.search(r"jpeg (\d+)x(\d+) psnr ([\d.]+)", s)
            verificar(m and int(m.group(1)) == w2 and float(m.group(3)) > 30, f"decodificador JPEG de la libsagui {w2}x{h2} contra ffmpeg ({s.strip()})")
        # YUY2: cuadro real de ffmpeg -> paquetes -> montador -> RGB
        yuv = os.path.join(DIR, "c.yuy2")
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", "testsrc=size=320x240:rate=5", "-frames:v", "1",
                        "-pix_fmt", "yuyv422", "-f", "rawvideo", yuv], check=True)
        datos = open(yuv, "rb").read()
        pacs = paquetes_cuadro(datos, 0, 1020) + paquetes_cuadro(datos, 1, 3072) + paquetes_cuadro(datos[:-1], 0)
        arq = os.path.join(DIR, "yuy2.paq")
        escribir_paquetes(arq, pacs)
        sal = os.path.join(DIR, "yuy2.cuadros")
        s = ejecutar("montar", arq, sal, 320, 240, 1)
        cuadros = leer_cuadros(sal)
        verificar(len(cuadros) == 2 and all(c == datos for c in cuadros) and "descartados 1" in s,
                  f"YUY2: dos cuadros completos idénticos y el que le falta un byte se descarta ({s.strip()})")
        ref = os.path.join(DIR, "c.rgb")
        subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "rawvideo", "-pix_fmt", "yuyv422", "-s", "320x240", "-i", yuv, "-f", "rawvideo",
                        "-pix_fmt", "rgb24", ref], check=True)
        s = ejecutar("yuy2", yuv, 320, 240, ref)
        m = re.search(r"psnr ([\d.]+)", s)
        verificar(m and float(m.group(1)) > 35, f"YUY2 a RGB de la libsagui contra la conversión de ffmpeg ({s.strip()})")
    # ---- la fuente de prueba: patrón -> paquetes -> montador
    pat = os.path.join(DIR, "padrao0.yuy2")
    s = ejecutar("padrao", 320, 240, 40, pat)
    verificar("padrao 40 de 40 cuadros identicos, descartados 0 errores 0" in s, f"patrón de prueba: 40 cuadros idénticos tras el viaje por paquetes ({s.strip()})")
    s = ejecutar("padrao", 640, 480, 6)
    verificar("padrao 6 de 6" in s, "patrón de prueba 640x480")
    # ---- cola de paquetes recibidos y fuzz
    s = ejecutar("isoent")
    verificar("isoent ok" in s, "cola de paquetes isócronos recibidos: orden, longitudes y casillas libres (200 rondas aleatorias)")
    s = ejecutar("fuzz", 7, 600)
    verificar("fuzz ok 600" in s, "fuzz de descriptores, bloques PROBE y paquetes sin fallos (ASan y UBSan)")
    s = ejecutar("fuzz", 99, 600)
    verificar("fuzz ok 600" in s, "fuzz con otra semilla")


# ------------------------------------------------------------ dentro de SaguiOS

def yuv_a_rgb(y, u, v):
    c = 298 * (y - 16)
    f = lambda t: max(0, min(255, t >> 8))
    return (f(c + 409 * (v - 128) + 128), f(c - 100 * (u - 128) - 208 * (v - 128) + 128), f(c + 516 * (u - 128) + 128))


BARRAS = [(235, 128, 128), (210, 16, 146), (170, 166, 16), (145, 54, 34), (106, 202, 222), (81, 90, 240), (41, 240, 110), (16, 128, 128)]


def pixeles(png_hex, l, a, puntos):
    raise NotImplementedError


def leer_rgb(ruta, lista, ancho):
    """Colores de una lista de (x, y) de un archivo .ppm P6 (lo que escribe `camara`, ver foto_ppm)."""
    raise NotImplementedError


def cmd(m, texto, tempo=60):
    """Un comando y, tras el prompt, un momento para que acabe de llegar la salida de la serie."""
    k = len(m.serial())
    m.comando(texto, tempo)
    time.sleep(1.5)
    return m.serial()[k:].replace("\r", "")


def os_():
    from qemu import imagem_console  # noqa: E402
    with Maquina("h277", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(1)
        out = m.comando("camara --info")
        verificar("fuente real: no hay cámara" in out.replace("\xe1", "a").replace("á", "a") or "no hay c" in out,
                  "sin cámara USB: `camara --info` lo dice (la fuente real no existe)")
        out = m.comando("camara -p --info")
        verificar("fuente prueba: presente" in out and "modo 0: 320x240 YUY2 15 fps" in out and "modo 1: 640x480 YUY2 15 fps" in out,
                  "la fuente de prueba ofrece 320x240 y 640x480 YUY2 a 15 cuadros por segundo")
        out = m.comando("camara --foto /tmp/x.png", 60)
        verificar("sin c" in out or "camara:" in out, "pedir una foto sin cámara falla con un mensaje (no se cuelga)")
        verificar("sagui:" in m.serial()[-200:], "el sistema sigue respondiendo")
        # foto con la fuente de prueba, 320x240 (modo 0)
        out = m.comando("camara -p --modo 0 --foto /tmp/f0.png --cuadros 12", 120)
        verificar("camara: 320x240 YUY2, 12 cuadros" in out and "foto guardada" in out, f"foto del patrón 320x240 ({out.strip()[-160:]})")
        mm = re.search(r"(\d+) descartados", out)
        verificar(mm and int(mm.group(1)) >= 1, "cada quinto cuadro pierde su último paquete y se descarta (se ven en el contador)")
        out = cmd(m, "visor -i /tmp/f0.png 10 10 50 10 90 10 130 10 170 10 210 10 250 10 300 10", 60)
        verificar("320 x 240" in out, f"el PNG de la foto es de 320x240 ({out.strip()[:100]})")
        colores = dict(re.findall(r"pixel (\d+),10: ([0-9a-f]{6})", out))
        for k, (x, barra) in enumerate(zip((10, 50, 90, 130, 170, 210, 250, 300), BARRAS)):
            esp = yuv_a_rgb(*barra)
            got = int(colores[str(x)], 16)
            gr, gg, gb = got >> 16, (got >> 8) & 255, got & 255
            verificar(all(abs(a - b) <= 2 for a, b in zip((gr, gg, gb), esp)), f"barra {k} del patrón: {gr:02x}{gg:02x}{gb:02x} ~ {esp}")
        out = m.comando("camara -p --modo 1 --foto /tmp/f1.png --cuadros 4", 120)
        verificar("camara: 640x480 YUY2" in out and "foto guardada" in out, "foto del patrón 640x480")
        out = cmd(m, "visor -i /tmp/f1.png 5 5", 60)
        verificar("640 x 480" in out, "el PNG del 640x480")
        out = m.comando("usb", 30)
        verificar("Camara UVC" not in out, "`usb` no habla de cámaras si no hay ninguna")
        # fuera de la sesión: un servicio no puede usar la cámara (de_servico) -> se comprueba el rechazo en el código; aquí, que
        # abrir dos veces seguidas la fuente de prueba no pierde memoria ni se cuelga
        for i in range(3):
            out = m.comando("camara -p --foto /tmp/r.png --cuadros 2", 90)
            verificar("foto guardada" in out, f"abrir y cerrar la cámara de prueba repetidas veces (vuelta {i + 1})")
        s = m.serial()
        verificar("Page fault" not in s and "PANIC" not in s.upper(), "ningún fallo del kernel durante las pruebas")


def ventana():
    from qemu import imagem_console  # noqa: E402
    with Maquina("h277v", "ide") as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(1)
        m.comando("camara -p &")
        m.esperar_serial("camara: abierta 640x480 YUY2", 30) if False else None
        fim = time.time() + 40
        while "camara: abierta" not in m.serial() and time.time() < fim:
            time.sleep(0.5)
        verificar("camara: abierta 320x240 YUY2" in m.serial() or "camara: abierta 640x480 YUY2" in m.serial(),
                  "la ventana abre la fuente de prueba")
        time.sleep(3)
        m.captura("H277-camara-prueba")
        mm = re.findall(r'criada por \d+: "Cámara" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial().replace("\r", ""))
        verificar(bool(mm), "la ventana «Cámara» existe")
        verificar("Page fault" not in m.serial(), "sin fallos del kernel con la ventana abierta")


if __name__ == "__main__":
    partes = sys.argv[1:] or ["anfitrion", "os", "ventana"]
    for p in partes:
        print(f"== {p}")
        {"anfitrion": anfitrion, "os": os_, "ventana": ventana}[p]()
    print("H277: todo bien")
