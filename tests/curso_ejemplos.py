#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Curso de sistemas operativos (D455, D456): los ejemplos de las lecciones, dentro de SaguiOS en QEMU.

Partes (por defecto las dos; o la que se pida: curso_ejemplos.py ejemplos | ayuda):
  ejemplos  con TCC dentro de SaguiOS (imagen de prueba con /apps):
            - /ejemplos/sdl/hilo_avisa.c: un hilo calcula y avisa al principal con una condición (500000500000);
            - /ejemplos/sdl/tono.c: la llamada de vuelta del audio corre en el hilo de audio de la SDL (otro hilo que el
              principal, D393) y la llaman unas decenas de veces en dos segundos;
            - /ejemplos/sdl/ventana_mueve.c: se abre la ventana, el rectángulo amarillo se mueve con las flechas y Escape sale
              (se mide en la pantalla la caja de los píxeles amarillos);
            - /ejemplos/asm/cpuid.c (anexo de ensamblador): compila, y con «tcc -run», y lee el fabricante de la CPU.
  ayuda     abre un capítulo del curso en la aplicación Ayuda: el visor maqueta las imágenes del capítulo (los diagramas de
            tools/diagramas_so.py) y, desplazado hasta la sección 2, se ve el diagrama en la pantalla.
Capturas sintéticas: docs/capturas/curso-ventana-mueve.png y curso-ayuda-capitulo.png.
Con KVM (SAGUI_KVM=1) el procesador es el del anfitrión: el fabricante de cpuid.c cambia y la prueba lo admite.
"""
import os
import re
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
sys.path.insert(0, os.path.join(RAIZ, "tools"))
from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ, SAIDA  # noqa: E402
from h392_tcc import esperar, orden  # noqa: E402
from diagramas_so import escribir_png  # noqa: E402

KVM = os.environ.get("SAGUI_KVM") == "1"
CAPTURAS = os.path.join(RAIZ, "docs", "capturas")
VERDE_DIAGRAMA = (0x14, 0x53, 0x2D)    # la cabecera de los diagramas del curso
AMARILLO = (255, 200, 0)               # el rectángulo de ventana_mueve.c


def leer_ppm(ruta):
    d = open(ruta, "rb").read()
    partes = d.split(None, 4)
    w, h = int(partes[1]), int(partes[2])
    return w, h, d[len(d) - w * h * 3:]


def pantalla(m, nombre):
    """Vuelca la pantalla y devuelve (ancho, alto, rgb)."""
    ppm = os.path.join(SAIDA, "curso-" + nombre + ".ppm")
    m.monitor("screendump " + ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    datos = leer_ppm(ppm)
    os.remove(ppm)
    return datos


def guardar_png(datos, nombre, recorte=None):
    w, h, rgb = datos
    x0, y0, x1, y1 = recorte or (0, 0, w, h)
    filas = b"".join(rgb[(y * w + x0) * 3:(y * w + x1) * 3] for y in range(y0, y1))
    os.makedirs(CAPTURAS, exist_ok=True)
    escribir_png(os.path.join(CAPTURAS, nombre + ".png"), x1 - x0, y1 - y0, filas)


def caja_de_color(datos, color):
    """(x0, y0, x1, y1, cuantos) de los píxeles exactamente de ese color, o None."""
    w, h, rgb = datos
    patron = bytes(color)
    xs, ys, n = [], [], 0
    for y in range(h):
        fila = rgb[y * w * 3:(y + 1) * w * 3]
        pos = fila.find(patron)
        while pos >= 0:
            if pos % 3 == 0:
                xs.append(pos // 3)
                ys.append(y)
                n += 1
            pos = fila.find(patron, pos + 1)
    if not n:
        return None
    return min(xs), min(ys), max(xs), max(ys), n


def cuantos_de_color(datos, color, tolerancia=0):
    """Cuántos píxeles son de ese color (con una tolerancia por canal: la Ayuda reduce las imágenes y las suaviza)."""
    w, h, rgb = datos
    n = 0
    for i in range(0, w * h * 3, 3):
        if (abs(rgb[i] - color[0]) <= tolerancia and abs(rgb[i + 1] - color[1]) <= tolerancia
                and abs(rgb[i + 2] - color[2]) <= tolerancia):
            n += 1
    return n


def arrancar(nombre):
    import h136_opentyrian as h136
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "tcc")):
        raise SystemExit("la imagen no lleva tcc (tools/tcc_fuentes.sh y make): no se puede probar")
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    m = Maquina(nombre, "ide", imagem=img, memoria="512M")
    m.ligar()
    esperar(m, r"sagui:/\$", 150, 0)
    time.sleep(4)
    h136.focar_terminal(m)
    return m


def parte_ejemplos():
    lento = 1 if KVM else 4
    m = arrancar("cursoej")
    try:
        orden(m, "cd /tmp", 30)
        # --- hilo_avisa.c (hilos, mutex y condición de la SDL)
        out = orden(m, "tcc /ejemplos/sdl/hilo_avisa.c -o hilo_avisa -lSDL2", 60 * lento)
        verificar("error" not in out, "tcc compila hilo_avisa.c (%r)" % out[-200:])
        out = orden(m, "./hilo_avisa", 60 * lento)
        verificar("suma de 1 a 1000000 = 500000500000" in out, "hilo_avisa: el hilo calcula y avisa al principal (%r)" % out[-200:])
        # --- tono.c (audio en hilo propio, SDL_LockAudio)
        out = orden(m, "tcc /ejemplos/sdl/tono.c -o tono -lSDL2", 60 * lento)
        verificar("error" not in out, "tcc compila tono.c (%r)" % out[-200:])
        out = orden(m, "./tono", 60 * lento)
        r = re.search(r"tono: (\d+) llamadas de vuelta; en otro hilo que el principal: (\w+)", out)
        verificar(r is not None, "tono: escribe cuántas veces le llamaron (%r)" % out[-200:])
        n = int(r.group(1))
        verificar(r.group(2) == "si", "tono: la llamada de vuelta corre en el hilo de audio, no en el principal")
        verificar(20 <= n <= 400, "tono: %d llamadas de vuelta en unos dos segundos (a 22 050 Hz con bloques de 512 muestras son ~86)" % n)
        # --- ventana_mueve.c (eventos, renderizador y teclado)
        out = orden(m, "tcc /ejemplos/sdl/ventana_mueve.c -o ventana_mueve -lSDL2", 60 * lento)
        verificar("error" not in out, "tcc compila ventana_mueve.c (%r)" % out[-200:])
        marca = len(m.serial())
        m.digitar("./ventana_mueve", 0.03)
        m.tecla("ret")
        esperar(m, r'criada por \d+: "[^"]*" 3\d\dx2\d\d', 60, marca)
        time.sleep(2)
        m.mouse_para(1012, 756)
        verificar('"Rectangulo"' in m.serial()[marca:], "ventana_mueve: la ventana se titula «Rectangulo» (sin acento: TCC escribe UTF-8)")
        p0 = pantalla(m, "mueve0")
        c0 = caja_de_color(p0, AMARILLO)
        verificar(c0 is not None and c0[4] > 800, "ventana_mueve: se ve el rectángulo amarillo (%s)" % (c0,))
        for _ in range(6):
            m.tecla("right", 0.15)
        for _ in range(3):
            m.tecla("down", 0.15)
        time.sleep(1.5)
        p1 = pantalla(m, "mueve1")
        c1 = caja_de_color(p1, AMARILLO)
        verificar(c1 is not None, "ventana_mueve: sigue el rectángulo después de las flechas")
        dx, dy = c1[0] - c0[0], c1[1] - c0[1]
        ancho0 = c0[2] - c0[0] + 1
        escala = max(1, round(ancho0 / 40))
        verificar(dx == 24 * escala and dy == 12 * escala,
                  "ventana_mueve: 6 flechas a la derecha y 3 abajo mueven el rectángulo 24 y 12 puntos (x%d): se movió %d,%d" % (escala, dx, dy))
        w, h, _ = p1
        guardar_png(p1, "curso-ventana-mueve", (max(0, c1[0] - 200), max(0, c1[1] - 120), min(w, c1[2] + 200), min(h, c1[3] + 150)))
        m.tecla("esc")
        esperar(m, r"sagui:[^\n]*\$", 30, marca + 20)
        verificar(True, "ventana_mueve: Escape cierra la ventana y vuelve el prompt")
        # --- cpuid.c del anexo de ensamblador (asm en línea con TCC)
        out = orden(m, "tcc -run /ejemplos/asm/cpuid.c", 90 * lento)
        r = re.search(r"hoja maxima: (\d+)\s+ebx=([0-9a-f]{8}) edx=([0-9a-f]{8}) ecx=([0-9a-f]{8})", out)
        verificar(r is not None, "cpuid.c con tcc -run escribe la hoja máxima y el fabricante (%r)" % out[-200:])
        hoja = int(r.group(1))
        fab = b"".join(int(g, 16).to_bytes(4, "little") for g in r.groups()[1:])
        print("    CPUID: hoja máxima %d, fabricante %r" % (hoja, fab.decode("latin-1")))
        verificar(hoja >= 1, "cpuid: la hoja máxima es al menos 1")
        verificar(fab in (b"GenuineIntel", b"AuthenticAMD") or not KVM,
                  "cpuid: el fabricante sale de EBX, EDX y ECX por ese orden (%r)" % fab)
        out = orden(m, "tcc /ejemplos/asm/cpuid.c -o cpuid && ./cpuid", 90 * lento)
        verificar("hoja maxima:" in out and "ebx=" in out, "cpuid.c compilado a un ejecutable también funciona (%r)" % out[-200:])
    finally:
        m.desligar()


def parte_ayuda():
    m = arrancar("cursoay")
    try:
        marca = len(m.serial())
        m.comando("ayuda-gui /ayuda/sistemas-operativos/06-memoria.md")
        esperar(m, r"ayuda: abierto /ayuda/sistemas-operativos/06-memoria\.md", 40, marca)
        time.sleep(2)
        s = m.serial()[marca:]
        imgs = re.findall(r"markdown: imagen /ayuda/sistemas-operativos/img/([\w-]+)\.png (\d+)x(\d+) dibujada a (\d+)x(\d+)", s)
        nombres = {i[0] for i in imgs}
        verificar({"paginacion", "mapa-memoria", "dinamico"} <= nombres,
                  "el capítulo de memoria maqueta sus tres diagramas (%s)" % sorted(nombres))
        j = re.findall(r'criada por \d+: "Ayuda" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial())[-1]
        jx, jy = int(j[2]), int(j[3])
        m.clique(jx + 600, jy + 300)
        datos = None
        for _ in range(25):                  # bajar de página en página hasta que se vea un diagrama
            m.tecla("pgdn", 0.3)
            time.sleep(0.6)
            m.mouse_para(1012, 756)
            datos = pantalla(m, "ayuda")
            if cuantos_de_color(datos, VERDE_DIAGRAMA, 10) > 400:
                break
        w, h, _ = datos
        guardar_png(datos, "curso-ayuda-capitulo", (jx, jy, min(w, jx + int(j[0]) + 6), min(h, jy + int(j[1]) + 26)))
        verde = cuantos_de_color(datos, VERDE_DIAGRAMA, 10)
        verificar(verde > 400, "con la sección 2 a la vista se dibuja la cabecera verde de un diagrama (%d píxeles)" % verde)
    finally:
        m.desligar()


if __name__ == "__main__":
    partes = sys.argv[1:] or ["ejemplos", "ayuda"]
    if "ejemplos" in partes:
        parte_ejemplos()
    if "ayuda" in partes:
        parte_ayuda()
    print("curso_ejemplos ok")
