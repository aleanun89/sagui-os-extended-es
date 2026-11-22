#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H459b - Tucán, Paint, PlanoMini y GebraMini portables (D459, apartado «segunda tanda»).

Hermana de h459_apps_portables.py. Sin QEMU, en el anfitrión: compila las cuatro aplicaciones para Linux con la
SDL 2 (`userland/portable/Makefile`) a partir de las MISMAS fuentes que SaguiOS y las maneja con
SDL_VIDEODRIVER=dummy y un guion de eventos (SAGUI_ROTEIRO):

  - Tucán: crea una presentación de ejemplo (--demo), la abre, añade una diapositiva (Ctrl+M), escribe texto en el
    marcador, guarda (Ctrl+S) y comprueba con --info; exporta a PDF (validado), PPTX (validado) y PNG.
  - Paint: rectángulo rojo, elipse rellena azul y trazo de lápiz con el ratón (arrastres del guion), Ctrl+S, y los
    píxeles del BMP coinciden con los de la captura; reabre el BMP, pinta una línea verde y conserva lo anterior;
    guarda también un PPM.
  - PlanoMini: líneas, círculo, rectángulo y cota tecleados en la línea de órdenes; EXPORTAR pdf/png/dxf; Ctrl+S;
    el .pmini se reabre sin ventana y da las mismas entidades.
  - GebraMini: puntos, recta, circunferencia y función tecleados, Ctrl+S, reabrir con --abrir (raíces calculadas)
    y exportar a PNG y PDF.

Las capturas salen del búfer de píxeles (sintéticas) en SAIDA/h459/; con --docs se copian a docs/capturas/H459b-*.png.

Uso: h459b_apps_portables.py [--docs]
"""
import os
import shutil
import struct
import subprocess
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import h459_apps_portables as h  # noqa: E402
from h459_apps_portables import B, DIR, TRAB, DATOS, correr, png_valido, leer  # noqa: E402
from qemu import verificar, RAIZ, CAPTURAS  # noqa: E402
import pdf_validar  # noqa: E402
import tucan_pptx_validar  # noqa: E402

APPS = ("tucan", "paint", "planomini", "gebramini")
ENV = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", SAGUI_DATOS=DATOS, LANG="es_ES.UTF-8")


def ejecutar(app, args, timeout=60):
    env = dict(ENV, HOME=os.path.join(TRAB, "casa"))
    r = subprocess.run([os.path.join(B, app)] + args, capture_output=True, env=env, timeout=timeout, cwd=TRAB)
    return r.returncode, (r.stdout + r.stderr).decode("latin-1")


def decodificar_png(ruta):
    """PNG RGB de 8 bits -> (ancho, alto, filas de bytes), con todos los filtros."""
    datos = open(ruta, "rb").read()
    i, idat, ancho, alto = 8, b"", 0, 0
    while i < len(datos):
        n, t = struct.unpack(">I4s", datos[i:i + 8])
        c = datos[i + 8:i + 8 + n]
        i += 12 + n
        if t == b"IHDR":
            ancho, alto = struct.unpack(">II", c[:8])
            verificar(c[8] == 8 and c[9] in (2, 6), "PNG RGB/RGBA de 8 bits")
            bpp = 3 if c[9] == 2 else 4
        elif t == b"IDAT":
            idat += c
    raw = zlib.decompress(idat)
    tam = ancho * bpp
    filas, prev = [], bytearray(tam)
    for y in range(alto):
        f = raw[y * (tam + 1)]
        fila = bytearray(raw[y * (tam + 1) + 1:(y + 1) * (tam + 1)])
        for x in range(tam):
            izq = fila[x - bpp] if x >= bpp else 0
            arr = prev[x]
            ul = prev[x - bpp] if x >= bpp else 0
            if f == 1:
                fila[x] = (fila[x] + izq) & 255
            elif f == 2:
                fila[x] = (fila[x] + arr) & 255
            elif f == 3:
                fila[x] = (fila[x] + (izq + arr) // 2) & 255
            elif f == 4:
                p = izq + arr - ul
                pa, pb, pc = abs(p - izq), abs(p - arr), abs(p - ul)
                fila[x] = (fila[x] + (izq if pa <= pb and pa <= pc else arr if pb <= pc else ul)) & 255
        filas.append(bytes(fila))
        prev = fila
    return ancho, alto, filas, bpp


def pixel_png(png, x, y):
    _, _, filas, bpp = png
    return tuple(filas[y][x * bpp:x * bpp + 3])


def leer_bmp(ruta):
    """BMP de 24 bits -> (ancho, alto, función pixel(x, y) -> (r, g, b))."""
    d = open(ruta, "rb").read()
    verificar(d[:2] == b"BM", "cabecera BMP")
    off = struct.unpack("<I", d[10:14])[0]
    w, hh, _, bits = struct.unpack("<iiHH", d[18:30])
    verificar(bits == 24, "BMP de 24 bits")
    fila = (w * 3 + 3) & ~3
    alto = abs(hh)

    def pix(x, y):
        yy = alto - 1 - y if hh > 0 else y
        o = off + yy * fila + x * 3
        return (d[o + 2], d[o + 1], d[o])
    return w, alto, pix


def prueba_tucan():
    demo = os.path.join(TRAB, "demo.tucan")
    rc, out = ejecutar("tucan", ["--demo", demo])
    verificar(rc == 0 and os.path.exists(demo), "tucan --demo crea una presentación: " + out)
    rc, out = ejecutar("tucan", ["--info", demo])
    verificar("5 diapositivas" in out, "la presentación de ejemplo tiene 5 diapositivas: " + out[:200])
    rc, _ = correr("tucan", [demo], [
        "esperar 300", "ctrl m", "esperar 100", "clique 567 322", "texto Hola desde Linux ñ", "esperar 100",
        "captura " + os.path.join(DIR, "H459b-tucan.png"), "ctrl s", "esperar 300", "sair"])
    verificar(rc == 0, "Tucán termina bien (rc=%d)" % rc)
    rc, out = ejecutar("tucan", ["--info", demo])
    verificar("6 diapositivas" in out, "Ctrl+M añadió una diapositiva y Ctrl+S la guardó: " + out[:120])
    verificar("Hola desde Linux \xf1" in out, "el texto tecleado (con ñ en Latin-1) está en el archivo guardado")
    png_valido(os.path.join(DIR, "H459b-tucan.png"), 980)
    # exportaciones
    pdf = os.path.join(TRAB, "tucan.pdf")
    rc, out = ejecutar("tucan", ["--convertir", demo, pdf])
    verificar(rc == 0 and open(pdf, "rb").read(5) == b"%PDF-", "Tucán exporta a PDF: " + out)
    info = pdf_validar.analizar(open(pdf, "rb").read())
    verificar(info["paginas"] == 6, "pdf_validar acepta el PDF de Tucán (%s páginas)" % info["paginas"])
    pptx = os.path.join(TRAB, "tucan.pptx")
    rc, out = ejecutar("tucan", ["--convertir", demo, pptx])
    resumen = tucan_pptx_validar.validar_pptx(open(pptx, "rb").read())
    verificar(rc == 0 and resumen, "Tucán exporta a PPTX y el paquete es válido")
    png = os.path.join(TRAB, "tucan-diap.png")
    rc, out = ejecutar("tucan", ["--convertir", demo, png, "--diap", "2", "--ancho", "640"])
    verificar(rc == 0, "Tucán exporta una diapositiva a PNG: " + out)
    png_valido(png, 640)
    # idas y vueltas: .pptx -> .tucan y la información coincide
    de_pptx = os.path.join(TRAB, "vuelta.tucan")
    rc, out = ejecutar("tucan", ["--convertir", pptx, de_pptx])
    rc, out = ejecutar("tucan", ["--info", de_pptx])
    verificar("6 diapositivas" in out and "Hola desde Linux" in out, "PPTX -> formato propio conserva diapositivas y texto")


def prueba_paint():
    bmp = os.path.join(TRAB, "dibujo.bmp")
    cap = os.path.join(DIR, "H459b-paint.png")
    # paleta: color k en x=68+20k, y=547; herramientas: rectángulo (56,124), elipse rellena (22,191), lápiz (22,56)
    rc, _ = correr("paint", [bmp], [
        "esperar 300",
        "clique 128 547", "clique 56 124", "arrastar 100 60 300 200",
        "clique 228 547", "clique 22 191", "arrastar 350 100 450 200",
        "clique 22 56", "arrastar 120 300 400 330",
        "ctrl s", "esperar 200", "captura " + cap, "sair"])
    verificar(rc == 0, "Paint termina bien (rc=%d)" % rc)
    w, a, pix = leer_bmp(bmp)
    verificar((w, a) == (640, 480), "el BMP es de 640x480")
    rojo, azul = (0xED, 0x1C, 0x24), (0x3F, 0x48, 0xCC)
    # el lienzo está en (84,42) de la ventana: rectángulo de (16,18) a (216,158); elipse de (266,58) a (366,158)
    borde = [pix(116, y) for y in range(16, 21)]
    verificar(rojo in borde, "el borde superior del rectángulo es rojo: %s" % borde)
    verificar(pix(116, 88) == (255, 255, 255), "el interior del rectángulo (sin relleno) sigue blanco")
    verificar(pix(316, 108) == azul, "el centro de la elipse rellena es azul: %s" % (pix(316, 108),))
    verificar(pix(5, 5) == (255, 255, 255), "el fondo del lienzo es blanco")
    verificar(any(pix(x, y) == azul for x in range(36, 316) for y in range(255, 292)), "hay trazo de lápiz azul")
    # comparar con la captura de la ventana (lo que se ve es lo que se guarda)
    png = decodificar_png(cap)
    malos = [(x, y) for x in range(0, 640, 7) for y in range(0, 480, 5) if pixel_png(png, 84 + x, 42 + y) != pix(x, y)]
    verificar(not malos, "los píxeles de la captura coinciden con los del BMP (%d diferencias)" % len(malos))
    # reabrir, pintar una línea verde y conservar lo anterior
    rc, _ = correr("paint", [bmp], [
        "esperar 300", "clique 188 547", "clique 22 124", "arrastar 120 400 500 420", "ctrl s", "esperar 200", "sair"])
    verificar(rc == 0, "Paint reabre el BMP y termina (rc=%d)" % rc)
    w, a, pix2 = leer_bmp(bmp)
    verde = (0x22, 0xB1, 0x4C)
    verificar(pix2(316, 108) == azul and rojo in [pix2(116, y) for y in range(16, 21)], "lo dibujado antes se conserva tras reabrir")
    verificar(any(pix2(x, y) == verde for x in range(36, 416) for y in range(350, 372)), "la línea verde nueva está en el BMP")
    # PPM
    ppm = os.path.join(TRAB, "dibujo.ppm")
    rc, _ = correr("paint", [ppm], ["esperar 300", "clique 128 547", "clique 56 124", "arrastar 100 60 200 120", "ctrl s", "esperar 200", "sair"])
    d = open(ppm, "rb").read()
    verificar(rc == 0 and d[:2] == b"P6" and b"640 480" in d[:20], "Paint guarda PPM (P6, 640x480)")


def prueba_planomini():
    cap = os.path.join(DIR, "H459b-planomini.png")
    pm = os.path.join(TRAB, "plano.pmini")
    rc, out = ejecutar("planomini", ["--orden", "L 0,0 100,0", "<Intro>", "--guardar", pm])
    verificar(rc == 0 and os.path.exists(pm), "planomini --orden crea un dibujo base: " + out)
    exp = {e: os.path.join(TRAB, "plano." + e) for e in ("pdf", "png", "dxf")}
    rc, _ = correr("planomini", [pm], [
        "esperar 300",
        "texto L 40,-20 40,20\\n\\nC 70,40 15\\nREC 10,10 40,30\\nCOTALIN 0,0 100,0 50,-20\\n",
        "texto EXPORTAR pdf " + exp["pdf"] + "\\nEXPORTAR png " + exp["png"] + "\\nEXPORTAR dxf " + exp["dxf"] + "\\n",
        "esperar 300", "captura " + cap, "ctrl s", "esperar 300", "sair"])
    verificar(rc == 0, "PlanoMini termina bien (rc=%d)" % rc)
    m = leer(pm)
    for lin in ("L 0 -1 255 0 0 100 0", "L 0 -1 255 40 -20 40 20", "C 0 -1 255 70 40 15"):
        verificar(lin in m, "el .pmini guardado contiene «%s»" % lin)
    verificar("\nD " in m and "\nW " in m, "el .pmini lleva la cota y el rectángulo")
    verificar(open(exp["pdf"], "rb").read(5) == b"%PDF-", "EXPORTAR pdf escribe un PDF")
    info = pdf_validar.analizar(open(exp["pdf"], "rb").read())
    verificar(info["paginas"] >= 1, "pdf_validar acepta el PDF de PlanoMini")
    png_valido(exp["png"])
    verificar(open(exp["dxf"], "rb").read().count(b"LINE") >= 2 and b"CIRCLE" in open(exp["dxf"], "rb").read(), "el DXF lleva líneas y círculo")
    png_valido(cap, 960)
    # reabrir sin ventana
    rc, out = ejecutar("planomini", ["--abrir", pm, "--listar"])
    verificar(rc == 0 and "CIRCULO" in out and out.count("LINEA") >= 2, "el .pmini guardado se reabre con sus entidades: " + out[-200:])
    # DXF -> .pmini
    rc, out = ejecutar("planomini", ["--orden", "IMPORTAR dxf " + exp["dxf"], "--listar"])
    verificar("CIRCULO" in out, "se importa el DXF exportado: " + out[-200:])


def prueba_gebramini():
    cap = os.path.join(DIR, "H459b-gebramini.png")
    gbm = os.path.join(TRAB, "construccion.gbm")
    rc, out = ejecutar("gebramini", ["--orden", "Punto(0,0)", "--guardar", gbm])
    verificar(rc == 0 and os.path.exists(gbm), "gebramini --orden crea una construcción base: " + out)
    rc, _ = correr("gebramini", [gbm], [
        "esperar 300", "texto A=(1,2)\\nB=(4,6)\\nr=Recta(A,B)\\nc=Circunferencia(A,3)\\nf(x)=x^2-2\\n", "esperar 200",
        "captura " + cap, "ctrl s", "esperar 300", "sair"])
    verificar(rc == 0, "GebraMini termina bien (rc=%d)" % rc)
    g = leer(gbm)
    for lin in ("A = (1, 2)", "B = (4, 6)", "r = Recta(A, B)", "c = Circunferencia(A, 3)", "f(x) = x^2 - 2"):
        verificar(lin in g, "el .gbm guardado contiene «%s»" % lin)
    png_valido(cap, 960)
    rc, out = ejecutar("gebramini", ["--abrir", gbm, "--orden", "Raiz(f)"])
    verificar("(-1.41, 0)" in out and "(1.41, 0)" in out and "7 objetos" in out, "reabierto, las raíces de f valen ±1,41: " + out[-200:])
    pdf, png = os.path.join(TRAB, "construccion.pdf"), os.path.join(TRAB, "construccion.png")
    rc, out = ejecutar("gebramini", ["--abrir", gbm, "--pdf", pdf, "--png", png])
    verificar(rc == 0 and open(pdf, "rb").read(5) == b"%PDF-", "GebraMini exporta a PDF: " + out)
    pdf_validar.analizar(open(pdf, "rb").read())
    png_valido(png)
    # los ejemplos de la carpeta de datos (/ejemplos/gebramini) se abren por su ruta
    rc, out = ejecutar("gebramini", ["--abrir", "/ejemplos/gebramini/parabola.gbm"])
    verificar(rc == 0 and "R1 = (-1, 0)" in out, "la ruta /ejemplos/gebramini/ apunta a la carpeta de datos: " + out[-150:])


def main():
    shutil.rmtree(TRAB, ignore_errors=True)
    os.makedirs(os.path.join(TRAB, "casa"), exist_ok=True)
    os.makedirs(DIR, exist_ok=True)
    shutil.rmtree(DATOS, ignore_errors=True)
    os.makedirs(DATOS)
    os.symlink(os.path.join(RAIZ, "rootfs", "fontes"), os.path.join(DATOS, "fontes"))
    os.symlink(os.path.join(RAIZ, "rootfs", "ejemplos"), os.path.join(DATOS, "ejemplos"))
    verificar(shutil.which("sdl2-config") is not None, "SDL 2 de desarrollo instalada (sdl2-config)")
    h.compilar()
    for a in APPS:
        verificar(os.path.exists(os.path.join(B, a)), "binario " + a)
    prueba_tucan()
    prueba_paint()
    prueba_planomini()
    prueba_gebramini()
    if "--docs" in sys.argv:
        for n in ("tucan", "paint", "planomini", "gebramini"):
            shutil.copy(os.path.join(DIR, "H459b-%s.png" % n), os.path.join(CAPTURAS, "H459b-%s.png" % n))
    print("H459b OK")


if __name__ == "__main__":
    main()
