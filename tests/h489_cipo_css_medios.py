#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H489 - Cipó: zoom, overflow con barras, tablas, CSS pendiente y <video>/<audio> (D489).

Páginas propias en tests/web489/ (servidas por HTTP desde el anfitrión). Todas llevan una marca
magenta de 4x4 en la esquina de la página (como las de tests/web/) para saber dónde está en la captura.

  - zoom: Ctrl++, Ctrl+- y Ctrl+0 (y Ctrl+rueda) maquetan otra vez: una caja de 100x20 px pasa a 200x40
    al 200 %, el texto se reparte en más líneas (la altura de la página sube más que el doble) y no se
    desborda; el zoom es del sitio (otra página del mismo servidor lo conserva) y se guarda en disco.
"""
import http.server
import math
import os
import re
import shutil
import struct
import subprocess
import sys
import threading
import time

AQUI = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, AQUI)
sys.path.insert(0, os.path.join(AQUI, "web"))
from qemu import Maquina, verificar, imagem_de_teste, RAIZ  # noqa: E402
import comprobar  # noqa: E402
import h70_sonido as h70  # noqa: E402

WEB = os.path.join(AQUI, "web489")
TIPOS = {".html": "text/html; charset=utf-8", ".css": "text/css", ".png": "image/png", ".js": "text/javascript",
         ".mpg": "video/mpeg", ".mp3": "audio/mpeg", ".wav": "audio/wav", ".mp4": "video/mp4"}
EXTRA = {}      # ruta -> bytes: lo que genera la prueba (vídeo, audio...)


class Tratador(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def do_GET(self):
        ruta = self.path.split("?")[0].lstrip("/")
        corpo, estado = b"no hay", 404
        if ruta in EXTRA:
            corpo, estado = EXTRA[ruta], 200
        else:
            camino = os.path.normpath(os.path.join(WEB, ruta))
            if camino.startswith(WEB + os.sep) and os.path.isfile(camino):
                with open(camino, "rb") as f:
                    corpo, estado = f.read(), 200
        self.send_response(estado)
        self.send_header("Content-Type", TIPOS.get(os.path.splitext(ruta)[1], "application/octet-stream"))
        self.send_header("Content-Length", str(len(corpo)))
        self.send_header("Connection", "close")
        self.end_headers()
        try:
            self.wfile.write(corpo)
        except (BrokenPipeError, ConnectionResetError):
            pass
        self.close_connection = True


class Navegador:
    """Lleva el Cipó de la máquina: abre páginas, espera a que estén listas, captura y lee píxeles."""

    def __init__(self, m, base):
        self.m, self.base = m, base
        self.marca2 = 0            # página con una segunda marca de 16 px de alto a esa y (para saber el desplazamiento de la página)
        self.primera = True        # la primera página es la que lleva la línea de órdenes de la imagen

    def abrir(self, nombre, titulo):
        desde = 0 if self.primera else len(self.m.serial_bruto())
        if not self.primera:
            self.m.tecla("ctrl-l", 0.3)
            self.m.digitar(self.base + nombre, 0.05)
            self.m.tecla("ret", 0.2)
        self.primera = False
        fim = time.time() + 120
        marca = 'cipo: pronto "%s' % titulo
        while time.time() < fim:
            if self.m.serial_bruto().find(marca, desde) >= 0:
                break
            time.sleep(0.2)
        else:
            raise AssertionError("el Cipó no terminó de abrir %r" % titulo)
        time.sleep(1.0)

    def ver(self, nombre, guardar=False):
        """Captura y devuelve (pixel, ox, oy)."""
        c = self.m.captura(nombre, guardar=guardar)
        pixel, larg, alt = comprobar.leer(c)
        o = comprobar.origen(pixel, larg, alt)
        if o is not None and self.marca2:
            # la página puede estar desplazada: la marca de arriba mide 8 px de alto, la de y=marca2 mide 16
            h = 0
            while pixel(o[0] + 1, o[1] + h) == (255, 0, 255):
                h += 1
            if h >= 12:
                o = (o[0], o[1] - self.marca2)
        verificar(o is not None, "%s: la página se ve (marca de origen)" % nombre)
        return pixel, o[0], o[1], larg, alt

    def log_desde(self, desde):
        return self.m.serial_bruto()[desde:]


def es(c, d, tol=24):
    return c is not None and all(abs(c[i] - d[i]) <= tol for i in range(3))


VERDE, AZUL, ROJO, BLANCO = (0, 128, 0), (0, 0, 255), (255, 0, 0), (255, 255, 255)


def primer_y(pixel, x, y0, y1, color):
    for y in range(y0, y1):
        if es(pixel(x, y), color):
            return y
    return None


def caja(pixel, ox, oy, color, x0, y0, x1, y1):
    """Rectángulo mínimo de `color` dentro de la ventana (x0..x1, y0..y1) de la página; None si no hay."""
    xs, ys = [], []
    for y in range(y0, y1):
        for x in range(x0, x1):
            if es(pixel(ox + x, oy + y), color):
                xs.append(x)
                ys.append(y)
    return (min(xs), min(ys), max(xs) + 1, max(ys) + 1) if xs else None


def prueba_zoom(nav):
    m = nav.m
    nav.abrir("z1.html", "Z1")
    p, ox, oy, larg, alt = nav.ver("h489-zoom100")
    a = caja(p, ox, oy, VERDE, 0, 0, 300, 40)
    verificar(a == (10, 10, 110, 30), "zoom 100 %%: la caja de 100x20 mide %s" % (a,))
    y100 = primer_y(p, ox + 15, oy + 50, alt - 2, AZUL)
    verificar(y100 is not None, "zoom 100 %%: la marca azul tras el texto está en y=%s" % y100)
    h100 = y100 - oy - 50

    # Ctrl++ : 100 -> 110 -> 125 -> 150 -> 175 -> 200
    d = len(m.serial_bruto())
    for _ in range(5):
        m.tecla("ctrl-bracket_right", 0.6)
    time.sleep(1.5)
    lg = nav.log_desde(d)
    verificar("cipo: zoom 200%" in lg, "Ctrl++ cinco veces llega al 200 %% (log: %s)" % re.findall(r"cipo: zoom \d+%", lg))
    p, ox, oy, larg, alt = nav.ver("h489-zoom200", guardar=True)
    a = caja(p, ox, oy, VERDE, 0, 0, 400, 80)
    verificar(a == (20, 20, 220, 60), "zoom 200 %%: la caja pasa a 200x40 en (20,20): %s" % (a,))
    y200 = primer_y(p, ox + 30, oy + 100, alt - 2, AZUL)
    verificar(y200 is not None, "zoom 200 %%: la marca azul sigue estando (y=%s)" % y200)
    h200 = y200 - oy - 100
    verificar(h200 > h100 * 2.6, "el texto se reparte en más líneas (altura %d -> %d, más del doble: se maqueta de nuevo)" % (h100, h200))
    # ningún texto pasa del borde derecho de la página (la ventana mide larg - 14 de rolagem...)
    m200 = max((x for x in range(ox, larg - 20) for y in (oy + 110, oy + 140) if es(p(x, y), (0, 0, 0), 60)), default=0)
    verificar(m200 and m200 < larg - 14 - 10, "al 200 %% el texto cabe en la ventana (borde derecho del texto en x=%d de %d)" % (m200, larg))

    # el zoom es del sitio: otra página del mismo servidor lo conserva
    nav.abrir("z2.html", "Z2")
    p, ox, oy, larg, alt = nav.ver("h489-zoom-sitio")
    a = caja(p, ox, oy, VERDE, 0, 0, 400, 80)
    verificar(a == (20, 20, 220, 60), "otra página del mismo sitio conserva el 200 %%: %s" % (a,))

    # Ctrl+- baja un paso y Ctrl+0 vuelve al 100 %
    d = len(m.serial_bruto())
    m.tecla("ctrl-slash", 0.8)
    time.sleep(1.0)
    verificar("cipo: zoom 175%" in nav.log_desde(d), "Ctrl+- baja del 200 al 175 %")
    m.tecla("ctrl-0", 0.8)
    time.sleep(1.0)
    p, ox, oy, larg, alt = nav.ver("h489-zoom-0")
    a = caja(p, ox, oy, VERDE, 0, 0, 400, 80)
    verificar(a == (10, 10, 110, 30), "Ctrl+0 vuelve a 100x20 en (10,10): %s" % (a,))
    # y alejar: 50 % -> 50x10
    for _ in range(4):        # 100 -> 90 -> 80 -> 75 -> 67
        m.tecla("ctrl-slash", 0.5)
    m.tecla("ctrl-slash", 0.5)   # 50
    time.sleep(1.0)
    p, ox, oy, larg, alt = nav.ver("h489-zoom50")
    a = caja(p, ox, oy, VERDE, 0, 0, 200, 40)
    verificar(a == (5, 5, 55, 15), "zoom 50 %%: la caja pasa a 50x10 en (5,5): %s" % (a,))
    m.tecla("ctrl-0", 0.8)
    time.sleep(0.8)
    # lo dejó guardado en disco
    return h100, h200

BANDAS = [(255, 0, 0), (0, 170, 0), (0, 0, 255), (255, 255, 0), (0, 255, 255), (255, 128, 0), (128, 0, 128), (128, 128, 128)]


def indice_banda(c):
    for i, b in enumerate(BANDAS):
        if es(c, b, 30):
            return i
    return None


def desplazamiento(pixel, x, y0, y1):
    """Cuánto está desplazado un bloque de bandas de 50 px: banda de arriba y primera frontera."""
    i0 = indice_banda(pixel(x, y0 + 1))
    if i0 is None:
        return None
    for y in range(y0 + 1, y1):
        if indice_banda(pixel(x, y)) != i0:
            return i0 * 50 + 50 - (y - y0)
    return i0 * 50


def arrastrar(m, x0, y0, x1, y1):
    """Como m.arrastar pero con pausas largas: el Cipó repinta el bloque en cada movimiento."""
    m.mouse_para(x0, y0)
    m.monitor("mouse_button 1")
    time.sleep(0.4)
    pasos = 6
    for i in range(1, pasos + 1):
        m.mouse_mover(x0 + (x1 - x0) * i // pasos, y0 + (y1 - y0) * i // pasos)
        time.sleep(0.4)
    m.monitor("mouse_button 0")
    time.sleep(0.6)


def prueba_overflow(nav):
    m = nav.m
    nav.abrir("o1.html", "O1")
    nav.marca2 = 300
    p, ox, oy, larg, alt = nav.ver("h489-overflow-0", guardar=True)
    PISTA, PULGAR = (241, 241, 241), (168, 168, 168)
    # A: overflow:auto con contenido más alto: barra vertical (carril + pulgar), nada de barra horizontal
    verificar(es(p(ox + 205, oy + 105), PISTA, 8), "overflow:auto con desbordamiento: el carril de la barra vertical se ve")
    verificar(es(p(ox + 205, oy + 20), PULGAR, 8), "overflow:auto: el pulgar de la barra está arriba")
    verificar(es(p(ox + 100, oy + 20), BANDAS[0], 20) and es(p(ox + 100, oy + 70), BANDAS[1], 20), "el bloque A empieza por la banda roja y sigue con la verde")
    verificar(not es(p(ox + 100, oy + 112), BANDAS[2], 20), "el contenido se recorta en el borde de abajo del bloque A")
    # B: overflow:scroll sin desbordamiento: las dos barras se ven igual
    verificar(es(p(ox + 445, oy + 60), PULGAR, 8) and es(p(ox + 300, oy + 105), PULGAR, 8) and es(p(ox + 444, oy + 40), PULGAR, 8) and es(p(ox + 447, oy + 104), PISTA, 40), "overflow:scroll sin desbordamiento: las dos barras se ven")
    # C: overflow:hidden: sin barra
    verificar(indice_banda(p(ox + 490 + 194, oy + 40)) is not None, "overflow:hidden no pinta barra (hasta el borde derecho hay contenido)")
    # D: overflow-x:auto: barra horizontal abajo, la vertical no
    verificar(es(p(ox + 730 + 100, oy + 105), PISTA, 8) or es(p(ox + 730 + 195, oy + 105), PISTA, 8), "overflow-x:auto: barra horizontal abajo")

    # rueda sobre A: se desplaza el bloque, no la página
    sx, sy = ox + 60, oy + 60       # coordenadas de pantalla (la captura es 1:1 con la pantalla)
    m.mouse_para(sx, sy)
    d = len(m.serial_bruto())
    m.monitor("mouse_move 0 0 1")
    time.sleep(0.8)
    lg = nav.log_desde(d)
    p, ox2, oy2, _, _ = nav.ver("h489-overflow-rueda1")
    off1 = desplazamiento(p, ox2 + 100, oy2 + 10, oy2 + 110)
    sentido = 1
    if off1 in (None, 0):          # el signo de dz depende del QEMU: probamos al revés
        m.monitor("mouse_move 0 0 -2")
        time.sleep(0.8)
        p, ox2, oy2, _, _ = nav.ver("h489-overflow-rueda1b")
        off1 = desplazamiento(p, ox2 + 100, oy2 + 10, oy2 + 110)
        sentido = -1
    verificar(off1 is not None and off1 > 0 and oy2 == oy, "la rueda desplaza el contenido del bloque A en %s px y la página se queda quieta" % off1)
    verificar("sobre un bloque con overflow" in nav.log_desde(d), "el Cipó lo anota en el log")
    # varias vueltas más: llega al final del contenido (400 - 100 = 300 px)
    d = len(m.serial_bruto())
    for _ in range(10):         # hasta que una vuelta la reciba la página (el bloque ya está en el final)
        m.monitor("mouse_move 0 0 %d" % sentido)
        time.sleep(0.5)
        if re.search(r"cipo: rueda -?\d+, linea", nav.log_desde(d)):
            break
    verificar(re.search(r"cipo: rueda -?\d+, linea", nav.log_desde(d)) is not None, "al llegar al final del bloque, la rueda pasa a la página")
    time.sleep(0.5)
    p, oxp, oyp, _, _ = nav.ver("h489-overflow-pagina")
    verificar(oyp < oy, "la página subió un poco por la vuelta sobrante (el origen pasó de y=%d a y=%d)" % (oy, oyp))
    m.mouse_para(sx + 500, sy + 200)       # fuera de los bloques: la rueda mueve la página
    m.monitor("mouse_move 0 0 %d" % (-sentido))
    time.sleep(0.8)
    p, ox2, oy2, _, _ = nav.ver("h489-overflow-fin")
    verificar(oy2 == oy, "la rueda fuera de los bloques devuelve la página a su sitio")
    off2 = desplazamiento(p, ox2 + 100, oy2 + 10, oy2 + 110)
    verificar(off2 is not None and 280 <= off2 <= 300, "con más vueltas llega al final del contenido (desplazado %s de 300 px)" % off2)
    # el pulgar baja con el contenido
    verificar(es(p(ox2 + 205, oy2 + 105), PULGAR, 8), "el pulgar de la barra bajó al final")

    # arrastrar el pulgar de A hasta el principio
    p, ox, oy, _, _ = nav.ver("h489-overflow-antes-arrastre")
    ty = next((y for y in range(oy + 10, oy + 110) if es(p(ox + 201, y), PULGAR, 8)), None)
    verificar(ty is not None, "el pulgar está a la vista (y=%s)" % ty)
    arrastrar(m, ox + 205, ty + 8, ox + 205, oy + 12)
    time.sleep(0.6)
    p, ox, oy, _, _ = nav.ver("h489-overflow-arrastre", guardar=True)
    off3 = desplazamiento(p, ox + 100, oy + 10, oy + 110)
    verificar(off3 is not None and off3 <= 12, "arrastrar el pulgar arriba devuelve el bloque al principio (desplazado %s px)" % off3)
    # y abajo: agarra el pulgar y lo lleva al fondo
    ty = next((y for y in range(oy + 10, oy + 110) if es(p(ox + 201, y), PULGAR, 8)), None)
    arrastrar(m, ox + 205, ty + 8, ox + 205, oy + 140)
    time.sleep(0.6)
    p, ox, oy, _, _ = nav.ver("h489-overflow-arrastre2")
    off4 = desplazamiento(p, ox + 100, oy + 10, oy + 110)
    verificar(off4 is not None and off4 >= 280, "arrastrar el pulgar al fondo lleva el bloque al final (desplazado %s px)" % off4)

def prueba_tablas(nav):
    nav.abrir("t1.html", "T1")
    p, ox, oy, larg, alt = nav.ver("h489-tablas", guardar=True)
    ROJO_, VERDE_, AZUL_, AMAR, CIAN, NARA, MORA = (255, 0, 0), (0, 128, 0), (0, 0, 255), (255, 255, 0), (0, 255, 255), (255, 128, 0), (128, 0, 128)

    def celda(nombre, color, esperado, ventana):
        x0, y0, x1, y1 = ventana
        c = caja(p, ox, oy, color, x0, y0, x1, y1)
        ok = c is not None and all(abs(c[i] - esperado[i]) <= 1 for i in range(4))
        verificar(ok, "%s: caja %s (esperada %s)" % (nombre, c, esperado))

    # 1: la celda de colspan=2 pide 100 y las dos columnas de 40 se reparten el resto por igual (50 + 50)
    celda("colspan A (2 columnas)", ROJO_, (10, 10, 110, 30), (0, 0, 200, 40))
    celda("colspan B", AZUL_, (110, 10, 140, 30), (0, 0, 200, 40))
    celda("colspan C (ensanchada a 50)", VERDE_, (10, 30, 60, 50), (0, 0, 200, 60))
    celda("colspan D (ensanchada a 50)", AMAR, (60, 30, 110, 50), (0, 0, 200, 60))
    celda("colspan E", CIAN, (110, 30, 140, 50), (0, 0, 200, 60))
    # 2: rowspan
    celda("rowspan A (50 de alto)", ROJO_, (10, 100, 50, 150), (0, 90, 200, 170))
    celda("rowspan B (fila 1)", AZUL_, (50, 100, 90, 120), (0, 90, 200, 170))
    celda("rowspan C (fila 2, cogió el sobrante)", VERDE_, (50, 120, 90, 150), (0, 90, 200, 170))
    # 3: table-layout: fixed, 300 de ancho: 100 + 50 + el resto; el contenido ancho de la segunda fila no cambia nada
    celda("fixed 1 (100)", ROJO_, (10, 200, 110, 220), (0, 190, 600, 250))
    celda("fixed 2 (50)", AZUL_, (110, 200, 160, 220), (0, 190, 600, 250))
    celda("fixed 3 (150)", VERDE_, (160, 200, 310, 220), (0, 190, 600, 250))
    celda("fixed fila 2, columna 1 (con contenido de 400)", AMAR, (10, 220, 110, 240), (0, 190, 600, 250))
    celda("fixed fila 2, columna 3", NARA, (160, 220, 310, 240), (0, 190, 600, 250))
    # 4: porcentajes (400 de ancho: 25 % = 100, 50 % = 200, el resto 100)
    celda("porcentaje 25 %", ROJO_, (10, 300, 110, 320), (0, 290, 600, 340))
    celda("porcentaje auto", AZUL_, (110, 300, 210, 320), (0, 290, 600, 340))
    celda("porcentaje 50 %", VERDE_, (210, 300, 410, 320), (0, 290, 600, 340))
    # 5: ancho explícito de 120 + colspan + rowspan
    celda("td width=120", ROJO_, (10, 380, 130, 400), (0, 370, 600, 450))
    celda("colspan 2 de 60 (columnas 20+20 ensanchadas a 30+30)", AZUL_, (130, 380, 190, 400), (0, 370, 600, 450))
    celda("rowspan debajo de la celda de 120", VERDE_, (10, 400, 130, 440), (0, 370, 600, 450))
    celda("celda 20 que reparte", AMAR, (130, 400, 160, 420), (0, 370, 600, 450))
    celda("celda 20 contigua", CIAN, (160, 400, 190, 420), (0, 370, 600, 450))
    celda("colspan 2 de 30 bajo rowspan", NARA, (130, 420, 190, 440), (0, 370, 600, 450))

def prueba_css(nav):
    nav.abrir("c1.html", "C1")
    p, ox, oy, larg, alt = nav.ver("h489-css", guardar=True)
    ROJO_, BLANCO_ = (255, 0, 0), (255, 255, 255)
    px = lambda x, y: p(ox + x, oy + y)       # noqa: E731
    # bordes dashed / dotted / double (antes salían sólidos)
    verificar(es(px(15, 11), ROJO_) and es(px(28, 11), BLANCO_) and es(px(40, 11), ROJO_), "border dashed de 4 px: raya, hueco, raya (%s %s %s)" % (px(15, 11), px(28, 11), px(40, 11)))
    verificar(es(px(11, 61), ROJO_) and es(px(13, 61), BLANCO_) and es(px(15, 61), ROJO_), "border dotted de 2 px: punto, hueco, punto (%s %s %s)" % (px(11, 61), px(13, 61), px(15, 61)))
    verificar(es(px(50, 111), ROJO_) and es(px(50, 113), BLANCO_) and es(px(50, 115), ROJO_), "border double de 6 px: dos rayas con un hueco en medio (%s %s %s)" % (px(50, 111), px(50, 113), px(50, 115)))
    # anidamiento
    c = caja(p, ox, oy, ROJO_, 0, 150, 200, 240)
    verificar(c == (10, 160, 110, 180), "anidamiento: .cont .a (rojo) en su sitio: %s" % (c,))
    c = caja(p, ox, oy, (0, 128, 0), 0, 150, 200, 240)
    verificar(c == (10, 180, 110, 200), "anidamiento con &: .cont.on .b2 (verde): %s" % (c,))
    c = caja(p, ox, oy, (255, 255, 0), 0, 150, 200, 240)
    verificar(c == (10, 200, 110, 220), "@media anidado dentro de la regla: .c (amarillo): %s" % (c,))
    verificar(es(px(200, 210), (204, 204, 204)) and es(px(200, 160), (204, 204, 204)), "la regla del padre sigue valiendo (fondo gris)")
    # colores nuevos
    def color(x):
        return px(x + 30, 250)
    verificar(es(color(10), (128, 0, 128), 12), "color-mix(in srgb, #f00 50%%, #00f 50%%) = morado %s" % (color(10),))
    verificar(es(color(80), (255, 0, 0), 30), "oklch(0.628 0.2577 29.23) = rojo sRGB %s" % (color(80),))
    verificar(es(color(150), (0, 0, 255), 30), "oklch(0.452 0.313 264.05) = azul sRGB %s" % (color(150),))
    verificar(es(color(220), (0, 128, 0), 8), "light-dark(#080, #f00) escoge el claro %s" % (color(220),))
    verificar(es(color(290), (255, 0, 0), 30), "oklab(0.628 0.2249 0.1258) = rojo sRGB %s" % (color(290),))
    verificar(es(color(360), (191, 191, 64), 12), "color-mix(in srgb, #00f 25%%, #ff0) = %s" % (color(360),))

    # position: sticky de verdad: al bajar la página, la franja se queda arriba
    nav.abrir("s1.html", "S1")
    m = nav.m
    p, ox, oy, larg, alt = nav.ver("h489-sticky-0")
    verificar(es(p(ox + 50, oy + 15), (0, 128, 0)), "sticky sin desplazar: la franja verde está en su sitio (y=10)")
    m.mouse_para(ox + 600, oy + 300)
    for _ in range(8):
        m.monitor("mouse_move 0 0 -1")
        time.sleep(0.3)
    time.sleep(0.8)
    c = m.captura("h489-sticky-1", guardar=True)
    p, larg, alt = comprobar.leer(c)
    ARRIBA = 168      # la primera fila de la página en la pantalla
    verificar(es(p(ox + 50, ARRIBA + 5), (0, 128, 0)) and es(p(ox + 50, ARRIBA + 19), (0, 128, 0)), "tras bajar la página la franja sticky sigue pegada arriba")
    verificar(es(p(ox + 50, ARRIBA + 30), (221, 221, 221), 12), "y debajo de ella sigue el fondo del contenedor")

def generar_medios():
    """Un vídeo de 8 s (rojo y luego azul, con tono) con ffmpeg y un WAV de 3 s a 440 Hz (lo hace la prueba)."""
    dur = 3
    taxa = 22050
    datos = b"".join(struct.pack("<h", int(12000 * math.sin(2 * math.pi * 440 * i / taxa))) for i in range(taxa * dur))
    EXTRA["a.wav"] = (b"RIFF" + struct.pack("<I", 36 + len(datos)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, taxa, taxa * 2, 2, 16)
                      + b"data" + struct.pack("<I", len(datos)) + datos)
    if not shutil.which("ffmpeg"):
        return False
    ruta = os.path.join(os.environ.get("SAGUI_SAIDA") or os.path.join(RAIZ, "build", "testes"), "h489-v.mp4")
    tom = ["-f", "lavfi", "-i", "sine=frequency=440:sample_rate=44100"]
    r = subprocess.run(["ffmpeg", "-v", "error", "-y", "-f", "lavfi", "-i", "color=c=red:size=320x180:rate=15:d=4",
                        "-f", "lavfi", "-i", "color=c=blue:size=320x180:rate=15:d=4", *tom, "-t", "8",
                        "-filter_complex", "[0:v][1:v]concat=n=2:v=1:a=0[v]", "-map", "[v]", "-map", "2:a",
                        "-c:v", "libx264", "-profile:v", "main", "-pix_fmt", "yuv420p", "-g", "15", "-c:a", "aac", "-b:a", "96k", "-ac", "2",
                        "-movflags", "+faststart", ruta], capture_output=True)
    if r.returncode:
        print("H489: ffmpeg falló: " + r.stderr.decode(errors="replace")[:200])
        return False
    with open(ruta, "rb") as f:
        EXTRA["v.mp4"] = f.read()
    return True


AZULC = (26, 115, 232)       # el relleno del progreso


def fila_progreso(p, x0, x1, y):
    """(ancho del relleno azul, extremos de la barra) en una fila de la barra de controles."""
    azul = [x for x in range(x0, x1) if es(p(x, y), AZULC, 14)]
    return len(azul)


def prueba_video(nav):
    m = nav.m
    nav.abrir("m1.html", "M1")
    fim = time.time() + 60
    while time.time() < fim and "cipo: video H.264" not in m.serial_bruto():
        time.sleep(0.3)
    verificar("cipo: video H.264" in m.serial_bruto(), "el Cipó abre el MP4 del <video controls>")
    time.sleep(1.0)
    p, ox, oy, larg, alt = nav.ver("h489-video-controles", guardar=True)
    BY = oy + 10 + 180 - 28          # la barra de controles: abajo de la caja del vídeo
    fondo = p(ox + 11, BY + 2)
    verificar(es(fondo, (32, 36, 40), 6), "la barra de controles del <video> está abajo, sobre el cuadro (%s)" % (fondo,))
    verificar(es(p(ox + 10 + 10, BY + 14), (255, 255, 255)) and es(p(ox + 10 + 18, BY + 14), (255, 255, 255)), "mientras suena se ve el icono de pausa")
    w1 = fila_progreso(p, ox + 10, ox + 330, BY + 14)
    time.sleep(2.0)
    p, ox, oy, larg, alt = nav.ver("h489-video-controles2")
    w2 = fila_progreso(p, ox + 10, ox + 330, BY + 14)
    verificar(w2 > w1, "la barra de progreso avanza con la reproducción (%d -> %d px)" % (w1, w2))
    verificar(es(p(ox + 10 + 160, oy + 10 + 60), (255, 0, 0), 40) or es(p(ox + 10 + 160, oy + 10 + 60), (0, 0, 255), 40), "el cuadro del vídeo se ve encima de la caja")
    # extremos de la barra de progreso: el relleno azul y el carril gris
    xs = [x for x in range(ox + 10, ox + 330) if es(p(x, BY + 14), AZULC, 14) or es(p(x, BY + 14), (89, 98, 107), 10)]
    x0 = x1 = min(xs)
    while any(es(p(x1 + 1 + k, BY + 14), AZULC, 14) or es(p(x1 + 1 + k, BY + 14), (89, 98, 107), 10) for k in range(0, 9)):
        x1 += 1       # la primera tira continua: la barra de progreso (después viene la del volumen)
        if x1 > ox + 330:
            break
    # un clic al 75 %: salta al segundo 6 (ya azul)
    d = len(m.serial_bruto())
    m.clique(x0 + (x1 - x0) * 3 // 4, BY + 14)
    time.sleep(1.5)
    salto = re.findall(r"salto a (\d+) ms", nav.log_desde(d))
    verificar(salto and 5000 <= int(salto[-1]) <= 7000, "un clic al 75 %% de la barra de progreso salta al segundo 6 de 8 (%s ms)" % salto)
    p, ox, oy, larg, alt = nav.ver("h489-video-salto")
    verificar(es(p(ox + 10 + 160, oy + 10 + 60), (0, 0, 255), 40), "tras saltar al 75 %% el vídeo está en la parte azul (%s)" % (p(ox + 10 + 160, oy + 10 + 60),))
    # pausa con el botón y vuelta a tocar
    d = len(m.serial_bruto())
    m.clique(ox + 10 + 14, BY + 14)
    time.sleep(0.8)
    verificar("cipo: medio en pausa" in nav.log_desde(d), "el botón de la barra pone el vídeo en pausa")
    m.mouse_para(ox + 600, oy + 400)       # el puntero fuera del icono
    time.sleep(0.5)
    p, ox, oy, larg, alt = nav.ver("h489-video-pausa", guardar=True)
    verificar(es(p(ox + 10 + 14, BY + 14), (255, 255, 255)) and not es(p(ox + 10 + 19, BY + 14), (255, 255, 255)), "en pausa el icono es un triángulo de reproducir")
    m.clique(ox + 10 + 14, BY + 14)
    time.sleep(0.8)
    verificar("cipo: medio reanudado" in nav.log_desde(d), "otro clic en el botón lo reanuda")
    # volumen al 75 %
    d = len(m.serial_bruto())
    vx1 = ox + 10 + 320 - 8
    m.clique(vx1 - 48 + 36, BY + 14)
    time.sleep(0.8)
    vol = re.findall(r"cipo: medio: volumen (\d+)", nav.log_desde(d))
    verificar(vol and 65 <= int(vol[-1]) <= 85, "un clic en el control de volumen lo pone al 75 %% (%s)" % vol)


def prueba_audio(nav):
    m = nav.m
    nav.abrir("m2.html", "M2")
    fim = time.time() + 60
    while time.time() < fim and "cipo: audio " not in m.serial_bruto()[m.serial_bruto().rfind("M2"):]:
        time.sleep(0.3)
    verificar("cipo: audio WAV" in m.serial_bruto() or "cipo: audio" in m.serial_bruto(), "el Cipó abre el archivo del <audio controls>")
    time.sleep(0.8)
    p, ox, oy, larg, alt = nav.ver("h489-audio-controles", guardar=True)
    BY = oy + 10 + (54 - 28) // 2
    verificar(es(p(ox + 11, BY + 2), (241, 243, 244), 6), "el <audio> pinta su barra de controles (%s)" % (p(ox + 11, BY + 2),))
    verificar(es(p(ox + 10 + 14, BY + 14), (32, 36, 40)) and not es(p(ox + 10 + 19, BY + 14), (32, 36, 40), 10), "sin autoplay se ve el triángulo de reproducir")
    d = len(m.serial_bruto())
    m.clique(ox + 10 + 14, BY + 14)
    time.sleep(1.5)
    verificar("cipo: medio reanudado" in nav.log_desde(d), "el botón de reproducir arranca el audio")
    m.mouse_para(ox + 600, oy + 400)
    time.sleep(0.5)
    p, ox, oy, larg, alt = nav.ver("h489-audio-tocando", guardar=True)
    verificar(es(p(ox + 10 + 10, BY + 14), (32, 36, 40)) and es(p(ox + 10 + 18, BY + 14), (32, 36, 40)), "mientras suena el icono es el de pausa")
    azul = [x for x in range(ox + 10, ox + 310) if es(p(x, BY + 14), AZULC, 14)]
    verificar(len(azul) > 3, "la barra de progreso del audio avanza (%d px de relleno)" % len(azul))
    fim = time.time() + 15
    while time.time() < fim and "cipo: audio terminado" not in nav.log_desde(d):
        time.sleep(0.4)
    verificar("cipo: audio terminado" in nav.log_desde(d), "el audio de 3 s llega al final y se para")
    time.sleep(0.5)


def comprobar_sonido(m):
    esq, dir_ = h70.ler_gravacao(m.wav)
    tr = [h70.Trecho(esq, dir_, a, b) for a, b in h70.trechos(esq, dir_)]
    verificar(len(tr) >= 1, "la tarjeta de sonido recibió audio del Cipó (%d trechos)" % len(tr))
    mejor = max(tr, key=lambda t: t.rms_esq)
    verificar(h70.perto(mejor.f_esq, 440, 25) or h70.perto(mejor.f_dir, 440, 25),
              "suena el tono de 440 Hz (izq %.0f Hz, der %.0f Hz)" % (mejor.f_esq, mejor.f_dir))


def main():
    servidor = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Tratador)
    threading.Thread(target=servidor.serve_forever, daemon=True).start()
    base = "http://10.0.2.2:%d/" % servidor.server_address[1]
    pruebas = sys.argv[1:]
    con_video = generar_medios() if (not pruebas or "video" in pruebas or "audio" in pruebas) else False
    try:
        inicial = {"zoom": "z1.html", "overflow": "o1.html", "tablas": "t1.html", "css": "c1.html", "video": "m1.html", "audio": "m2.html"}
        imagem = imagem_de_teste("/apps/bin/cipo %s%s" % (base, inicial[pruebas[0]] if pruebas else "z1.html"), ["--vesa"])
        with Maquina("h489", "ide", imagem=imagem, extras=["-nic", "user,model=e1000"], som="hda") as m:
            nav = Navegador(m, base)
            m.mouse_para(1000, 740)
            if not pruebas or "zoom" in pruebas:
                prueba_zoom(nav)
            if not pruebas or "overflow" in pruebas:
                prueba_overflow(nav)
            if not pruebas or "tablas" in pruebas:
                prueba_tablas(nav)
            if not pruebas or "css" in pruebas:
                prueba_css(nav)
            if not pruebas or "video" in pruebas:
                if con_video:
                    prueba_video(nav)
                else:
                    print("H489: sin ffmpeg: se salta el <video>")
            if not pruebas or "audio" in pruebas:
                prueba_audio(nav)
        if not pruebas or "audio" in pruebas:
            comprobar_sonido(m)
    finally:
        servidor.shutdown()
    return 0


if __name__ == "__main__":
    sys.exit(main())
