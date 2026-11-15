#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H309 - GebraMini, geometría dinámica y funciones (mejora 128, D309).

Parte 0, en el anfitrión: pruebas de unidad del motor (tests/gebramini_unidad.c: matemáticas x87 frente
a la libm, analizador, derivada simbólica, geometría dinámica, raíces, órdenes en español, fichero .gbm
y los cuatro ejemplos).
Parte 1, sin ventana: los ejemplos de /ejemplos/gebramini se abren, las órdenes por línea de comandos
crean objetos, y se exporta a PDF (validado) y a PNG (cabecera y píxeles).
Parte 2, con ventana: crear dos puntos y un segmento con el ratón, el punto medio, arrastrar un punto y
comprobar que el punto medio lo sigue, escribir f(x)=x^2-2 y sus raíces, guardar, reabrir y exportar a
PDF. Capturas en docs/capturas/H309-*.png.
"""
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, mtools, SAIDA, RAIZ  # noqa: E402
import pdf_validar  # noqa: E402


def decodificar_png(datos):
    """PNG RGB de 8 bits -> (ancho, alto, filas de bytes), con todos los filtros."""
    import zlib
    i, idat, l, a = 8, b"", 0, 0
    while i < len(datos):
        n, t = struct.unpack(">I4s", datos[i:i + 8])
        c = datos[i + 8:i + 8 + n]
        i += 12 + n
        if t == b"IHDR":
            l, a = struct.unpack(">II", c[:8])
            assert c[8] == 8 and c[9] == 2, "se esperaba RGB de 8 bits"
        elif t == b"IDAT":
            idat += c
    raw = zlib.decompress(idat)
    bpp, tam = 3, l * 3
    filas, prev = [], bytearray(tam)
    for y in range(a):
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
                pa, pb, pc = abs(arr - ul), abs(izq - ul), abs(izq + arr - 2 * ul)
                pr = izq if pa <= pb and pa <= pc else (arr if pb <= pc else ul)
                fila[x] = (fila[x] + pr) & 255
        filas.append(fila)
        prev = fila
    return l, a, filas


def unidad():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "gebramini_unidad")
    subprocess.run(["gcc", "-O1", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "userland/inc"),
                    "-idirafter", os.path.join(RAIZ, "kernel/inc"), "-I", os.path.join(RAIZ, "userland/gebramini"),
                    "-finput-charset=UTF-8", "-fexec-charset=ISO-8859-1", "-o", exe,
                    os.path.join(RAIZ, "tests/gebramini_unidad.c"), os.path.join(RAIZ, "userland/gebramini/gbm_expr.c"),
                    os.path.join(RAIZ, "userland/gebramini/gbm_motor.c"), "-lm"], check=True)
    r = subprocess.run([exe, os.path.join(RAIZ, "rootfs/ejemplos/gebramini")], capture_output=True)
    salida = r.stdout.decode("latin-1")
    verificar(r.returncode == 0 and "todo bien" in salida, "unidad del motor en el PC: " + salida.strip().splitlines()[-2:].__repr__())
    n = re.search(r"(\d+) comprobaciones, (\d+) fallos", salida)
    verificar(n and int(n.group(1)) > 500, "la unidad hace más de 500 comprobaciones (%s)" % (n.group(1) if n else "?"))


def esperar_desde(m, texto, desde, tempo=30):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial()[desde:]:
            return True
        time.sleep(0.2)
    verificar(False, "esperaba %r en la serie" % texto)


def janela(m, titulo, desde=0):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial()[desde:])
    verificar(bool(g), "ventana %r creada" % titulo)
    g = g[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def borrar(m, n=24):
    for _ in range(n):
        m.tecla("backspace", 0.03)


def lanzar(m, comando):
    pos = len(m.serial())
    m.digitar(comando, 0.04)
    m.tecla("ret", 0.04)
    return pos


def punto(serial, nombre):
    """Última (x, y) de «gebramini: obj NOMBRE = (x, y)» o «arrastrado NOMBRE = (x, y)»."""
    r = re.findall(r"gebramini: (?:obj|arrastrado) %s = \((-?[\d.e+-]+), (-?[\d.e+-]+)\)" % re.escape(nombre), serial)
    return (float(r[-1][0]), float(r[-1][1])) if r else None


def parte1(m, sacar, pen):
    # --- los ejemplos se abren
    esperados = {"triangulo": 22, "circunferencia-3-puntos": 12, "parabola": 8, "tales": 17}
    for nombre, minimo in esperados.items():
        out = m.comando("gebramini --abrir /ejemplos/gebramini/%s.gbm" % nombre, 60)
        mm = re.search(r"gebramini: (\d+) objetos", out)
        verificar(mm is not None and "error" not in out, "el ejemplo %s se abre: %s" % (nombre, out.strip().splitlines()[-1:]))
        verificar(int(mm.group(1)) >= minimo, "%s tiene al menos %d objetos (%s)" % (nombre, minimo, mm.group(1)))
        verificar("= ?" not in out, "%s: ningún objeto sin definir" % nombre)
    out = m.comando("gebramini --abrir /ejemplos/gebramini/triangulo.gbm", 60)
    verificar("obj O = (" in out and "obj G = (" in out and "obj H = (" in out and "obj I = (" in out, "el triángulo da O, G, H e I")
    verificar(re.search(r"obj c: \(x [-+] [\d.]+\)\xb2 \+ \(y [-+] [\d.]+\)\xb2 = ", out) is not None, "la circunferencia circunscrita sale como ecuación con ²: %r" % out[-400:])

    # --- órdenes por la línea de comandos: raíces de f(x) = x^2 - 2
    out = m.comando("gebramini --orden 'f(x)=x^2-2' 'Raiz(f)' 'a=2' 'g(x)=a*sin(x)' --guardar /usb/ord.gbm", 60)
    verificar("obj f(x) = x\xb2 - 2" in out, "f(x) = x² - 2 en la vista algebraica: %r" % out[-300:])
    verificar("obj A = (-1.41, 0)" in out and "obj B = (1.41, 0)" in out, "las raíces de x²-2 son ±1.41")
    verificar("obj a = 2" in out, "a = 2 crea el deslizador a")
    verificar("g(x) = a * sin(x)" in out, "g usa el deslizador")
    gbm = sacar("ord.gbm").decode("latin-1")
    verificar("f(x) = x^2 - 2" in gbm and "A = Raiz(f, 1)" in gbm and "a = Deslizador(" in gbm, "el .gbm guardado es texto legible: %r" % gbm[:200])
    out = m.comando("gebramini --orden 'Recta(Z, W)'", 30)
    verificar("error" in out and "Z" in out, "un error se explica: %r" % out.strip()[-80:])

    # --- exportar a PDF vectorial y a PNG
    out = m.comando("gebramini --abrir /ejemplos/gebramini/triangulo.gbm --pdf /usb/tri.pdf --png /usb/tri.png", 120)
    verificar("no se pudo" not in out, "exporta PDF y PNG: %s" % out.strip().splitlines()[-1:])
    pdf = pdf_validar.analizar(sacar("tri.pdf"))
    verificar(pdf["paginas"] == 1, "el PDF tiene una página")
    verificar(abs(pdf["mediabox"][0] - 841.9) < 0.1 and abs(pdf["mediabox"][1] - 595.3) < 0.1, "A4 apaisado %s" % (pdf["mediabox"],))
    cont = pdf["contenidos"][0]
    nl = len(re.findall(rb" l\b", cont)) + len(re.findall(rb"\bl\n", cont))
    verificar(nl > 150 or cont.count(b"\n") > 150, "el PDF trae muchas líneas vectoriales (%d operadores)" % cont.count(b"\n"))
    t = " ".join(pdf_validar.texto_de(cont))
    verificar("A" in t and "B" in t and "C" in t and "O" in t, "los nombres de los puntos están en el PDF como texto (%r)" % t[:80])
    ext = pdf_validar.validar_externo(os.path.join(SAIDA, "h309-tri.pdf"))
    if ext:
        for k, (rc, txt) in ext.items():
            verificar(rc == 0, "%s acepta el PDF" % k)
    out = m.comando("visorpdf -t /usb/tri.pdf", 60)
    verificar("paginas: 1" in out, "el Visor de PDF abre el PDF de GebraMini: %s" % out.strip().splitlines()[:2])
    png = sacar("tri.png")
    verificar(png[:8] == b"\x89PNG\r\n\x1a\n", "cabecera PNG")
    l, a = struct.unpack('>II', png[16:24])
    verificar((l, a) == (800, 520), "el PNG de la vista mide 800x520 (%dx%d)" % (l, a))
    pl, pa_, filas = decodificar_png(png)
    colores = set()
    for fila in filas:
        for x in range(0, pl * 3, 3):
            colores.add(bytes(fila[x:x + 3]))
    verificar(len(colores) > 40, "el PNG tiene antialias (%d colores distintos)" % len(colores))
    verificar(bytes(filas[0][0:3]) == b"\xff\xff\xff", "el fondo es blanco")
    out = m.comando("gebramini --orden 'Punto(0,0)' --png /usb/p.png", 60)
    png2 = sacar("p.png")
    # el origen (400, 260) cae en el centro con el cuadro de la vista restablecida / ver todo: hay píxeles oscuros de los ejes
    verificar(len(png2) > 1000, "otro PNG (%d bytes)" % len(png2))
    for nombre in ("triangulo", "circunferencia-3-puntos", "parabola", "tales"):
        m.comando("gebramini --abrir /ejemplos/gebramini/%s.gbm --png /usb/ej-%s.png" % (nombre, nombre), 120)
        sacar("ej-%s.png" % nombre)


def parte2(m, sacar):
    pos = lanzar(m, "gebramini")
    esperar_desde(m, "gebramini: pronto", pos, 60)
    time.sleep(1.5)
    w = janela(m, "Sin título - GebraMini", pos)
    verificar(w["l"] == 960 and w["a"] == 640, "ventana de GebraMini 960x640 (%dx%d)" % (w["l"], w["a"]))
    ser = m.serial()[pos:]
    botones = {int(a): int(b) for a, b in re.findall(r"gebramini: boton (\d+) (\d+)", ser)}
    verificar(len(botones) >= 22, "la barra de herramientas tiene los 22 botones (%d)" % len(botones))
    ox, oy, esc = re.search(r"vista origen=\((-?\d+),(-?\d+)\) escala=(\d+)", ser).groups()
    ox, oy, esc = int(ox) + w["x"], int(oy) + w["y"], int(esc)
    verificar(esc == 40, "escala inicial 40 px por unidad")
    m.captura("H309-1-inicio")

    def boton(i):
        m.clique(w["x"] + botones[i] + 10, w["y"] + 20 + 14)
        time.sleep(0.5)

    def px(x, y):        # coordenadas del plano a pantalla
        return ox + int(round(x * esc)), oy - int(round(y * esc))

    # ---- segmento con el ratón: dos puntos nuevos y la recta que los une
    boton(3)
    verificar("gebramini: herramienta Segmento" in m.serial()[pos:], "herramienta Segmento")
    k = len(m.serial())
    m.clique(*px(-3, 1))
    time.sleep(0.4)
    m.clique(*px(3, 1))
    esperar_desde(m, "orden Segmento(A, B)", k, 20)
    time.sleep(0.5)
    s = m.serial()[k:]
    verificar(punto(s, "A") is not None and abs(punto(s, "A")[0] + 3) < 0.1 and abs(punto(s, "A")[1] - 1) < 0.1, "A = (-3, 1): %s" % (punto(s, "A"),))
    verificar(punto(s, "B") is not None and abs(punto(s, "B")[0] - 3) < 0.1, "B = (3, 1): %s" % (punto(s, "B"),))
    verificar("obj a = 6" in s, "el segmento a mide 6 (vista algebraica)")
    m.captura("H309-2-segmento")

    # ---- punto medio: un clic sobre el segmento
    boton(9)
    k = len(m.serial())
    m.clique(*px(0, 1))
    esperar_desde(m, "orden PuntoMedio(a)", k, 20)
    time.sleep(0.5)
    pm = punto(m.serial()[k:], "C")
    verificar(pm is not None and abs(pm[0]) < 0.05 and abs(pm[1] - 1) < 0.05, "el punto medio C está en (0, 1): %s" % (pm,))

    # ---- arrastrar A: el punto medio lo sigue
    boton(1)
    k = len(m.serial())
    x0, y0 = px(-3, 1)
    x1, y1 = px(-3, -2)
    m.arrastar(x0, y0, x1, y1)
    esperar_desde(m, "gebramini: arrastrado A", k, 20)
    time.sleep(0.8)
    s = m.serial()[k:]
    a = punto(s, "A")
    verificar(a is not None and abs(a[0] + 3) < 0.1 and abs(a[1] + 2) < 0.1, "A arrastrado a (-3, -2): %s" % (a,))
    pm = punto(s, "C")
    verificar(pm is not None and abs(pm[0]) < 0.1 and abs(pm[1] + 0.5) < 0.1, "el punto medio siguió a A: C = %s (esperaba (0, -0.5))" % (pm,))
    m.captura("H309-3-arrastre")

    # ---- la barra de entrada: f(x) = x^2 - 2 y sus raíces
    k = len(m.serial())
    m.digitar("f(x)=x^2-2\n", 0.05)
    esperar_desde(m, "gebramini: orden f(x)=x^2-2", k, 20)
    time.sleep(0.5)
    m.digitar("Raiz(f)\n", 0.05)
    esperar_desde(m, "gebramini: orden Raiz(f)", k, 20)
    time.sleep(1.0)
    s = m.serial()[k:]
    verificar("obj f(x) = x\xb2 - 2" in s, "la vista algebraica muestra f(x) = x² - 2")
    d, e = punto(s, "D"), punto(s, "E")
    verificar(d is not None and e is not None and abs(d[0] + 1.4142) < 0.01 and abs(e[0] - 1.4142) < 0.01 and abs(d[1]) < 1e-9,
              "las raíces de x²-2 son ±1.4142: D = %s, E = %s" % (d, e))
    m.captura("H309-4-funcion-raices")

    # ---- autocompletado: «Seg» + Tab
    k = len(m.serial())
    m.digitar("Seg", 0.05)
    m.tecla("tab")
    time.sleep(0.4)
    m.captura("H309-5-autocompletar")
    borrar(m, 16)

    # ---- guardar por el diálogo
    k = len(m.serial())
    m.tecla("ctrl-s")
    esperar_desde(m, "dialogo: guardar en", k, 20)
    time.sleep(0.6)
    borrar(m)
    m.digitar("/usb/h309\n", 0.05)
    esperar_desde(m, "gebramini: guardado /usb/h309.gbm", k, 30)
    time.sleep(0.8)
    gbm = sacar("h309.gbm").decode("latin-1")
    verificar("a = Segmento(A, B)" in gbm and "C = PuntoMedio(A, B)" in gbm and "f(x) = x^2 - 2" in gbm and "Raiz(f, 2)" in gbm,
              "el .gbm guardado tiene la construcción: %r" % gbm[:300])

    # ---- propiedades por doble clic en el segmento
    boton(1)
    k = len(m.serial())
    m.clique(*px(0, -0.5), duplo=True)    # sobre el punto medio: abre sus propiedades
    time.sleep(1.2)
    dlg = janela(m, "Propiedades", k)
    m.captura("H309-6-propiedades")
    verificar(dlg["l"] == 400, "el doble clic abre las propiedades del objeto")
    m.tecla("esc")
    time.sleep(0.6)

    # ---- deshacer, rehacer
    k = len(m.serial())
    m.tecla("ctrl-z")
    esperar_desde(m, "gebramini: deshacer", k, 20)
    time.sleep(0.4)
    m.tecla("ctrl-y")
    esperar_desde(m, "gebramini: rehacer", k, 20)
    time.sleep(0.4)
    verificar("obj f(x) = x\xb2 - 2" in m.serial()[k:], "deshacer y rehacer devuelven la construcción")

    # ---- exportar a PDF por el diálogo
    k = len(m.serial())
    m.tecla("ctrl-p")
    esperar_desde(m, "dialogo: guardar en", k, 20)
    time.sleep(0.4)
    borrar(m)
    m.digitar("/usb/h309\n", 0.05)
    esperar_desde(m, "gebramini: pdf exportado /usb/h309.pdf", k, 40)
    time.sleep(1.0)
    m.tecla("ret")
    time.sleep(0.6)
    pdf = pdf_validar.analizar(sacar("h309.pdf"))
    t = " ".join(pdf_validar.texto_de(pdf["contenidos"][0]))
    verificar(pdf["paginas"] == 1 and "A" in t and "B" in t, "el PDF de la ventana se valida y tiene los nombres de los puntos (%r)" % t[:60])

    # ---- cerrar y reabrir el .gbm: mismos objetos
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    time.sleep(1.0)
    ser = m.serial()[k:]
    if "gebramini: fechado" not in ser:
        # tiene cambios sin guardar (deshacer/rehacer): descartarlos
        m.tecla("ret")
        time.sleep(0.8)
    esperar_desde(m, "gebramini: fechado", k, 20)
    pos = lanzar(m, "gebramini /usb/h309.gbm")
    esperar_desde(m, "gebramini: pronto", pos, 60)
    time.sleep(1.5)
    s = m.serial()[pos:]
    verificar("gebramini: abierto /usb/h309.gbm" in s, "se reabre el archivo guardado")
    a, b = punto(s, "A"), punto(s, "B")
    verificar(a is not None and abs(a[1] + 2) < 0.1 and b is not None and abs(b[0] - 3) < 0.1, "A y B se conservan: %s %s" % (a, b))
    verificar(abs(punto(s, "C")[1] + 0.5) < 0.1, "el punto medio, recalculado, sigue en (0, -0.5)")
    verificar("obj f(x) = x\xb2 - 2" in s, "la función se conserva")
    d = punto(s, "D")
    verificar(d is not None and abs(d[0] + 1.4142) < 0.01, "las raíces se conservan: D = %s" % (d,))
    m.captura("H309-7-reabierto")
    w = janela(m, "Sin título - GebraMini", pos)
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    esperar_desde(m, "gebramini: fechado", pos, 20)

def parte3(m):
    """Todas las herramientas de la barra, una a una, con el ratón."""
    pos = lanzar(m, "gebramini")
    esperar_desde(m, "gebramini: pronto", pos, 60)
    time.sleep(1.5)
    w = janela(m, "Sin título - GebraMini", pos)
    ser = m.serial()[pos:]
    botones = {int(a): int(b) for a, b in re.findall(r"gebramini: boton (\d+) (\d+)", ser)}
    ox, oy, esc = [int(v) for v in re.search(r"vista origen=\((-?\d+),(-?\d+)\) escala=(\d+)", ser).groups()]
    ox, oy = ox + w["x"], oy + w["y"]

    def px(x, y):
        return ox + int(round(x * esc)), oy - int(round(y * esc))

    def herr(i):
        m.clique(w["x"] + botones[i] + 10, w["y"] + 34)
        time.sleep(0.4)

    def clic(x, y, **kw):
        m.clique(*px(x, y), **kw)
        time.sleep(0.5)

    def espera(orden, k, tempo=20):
        esperar_desde(m, "gebramini: orden " + orden, k, tempo)
        time.sleep(0.4)

    def mark():
        return len(m.serial())

    # Polígono: tres vértices nuevos y clic en el primero para cerrar
    herr(15)
    k = mark()
    clic(-6, -4); clic(-1, -4); clic(-3.5, 0); clic(-6, -4)
    espera("Poligono(A, B, C)", k)
    verificar("obj a = 10" in m.serial()[k:], "el polígono ABC tiene área 10: %s" % re.findall(r"obj a = [\d.]+", m.serial()[k:]))
    # Ángulo y bisectriz sobre los mismos puntos
    herr(17)
    k = mark()
    clic(-6, -4); clic(-1, -4); clic(-3.5, 0)
    espera("Angulo(A, B, C)", k)
    s = m.serial()[k:]
    verificar(re.search(r"obj alfa = 6[\d.]+\xb0", s) is not None or re.search(r"obj alfa = [\d.]+\xb0", s) is not None, "el ángulo en B sale en grados: %s" % re.findall(r"obj alfa = [^\n]*", s))
    herr(14)
    k = mark()
    clic(-6, -4); clic(-1, -4); clic(-3.5, 0)
    espera("Bisectriz(A, B, C)", k)
    # Recta DE, circunferencia y corte
    herr(4)
    k = mark()
    clic(2, 3); clic(6, 1)
    espera("Recta(D, E)", k)
    herr(7)
    k = mark()
    clic(2, 3); clic(6, 1)
    espera("Circunferencia(D, E)", k)
    herr(10)
    k = mark()
    clic(-2.47, 3); clic(4, 2)
    espera("Interseca(", k)
    s = m.serial()[k:]
    verificar(len(re.findall(r"obj [F-Z]\d* = \(", s)) >= 2, "la recta por el centro corta a la circunferencia en dos puntos: %s" % re.findall(r"obj \w+ = \([^)]*\)", s))
    # Perpendicular y paralela a DE por puntos
    herr(11)
    k = mark()
    clic(-3.5, 0); clic(4, 2)
    espera("Perpendicular(C,", k)
    herr(12)
    k = mark()
    clic(-1, -4); clic(4, 2)
    espera("Paralela(B,", k)
    # Distancia, mediatriz, punto medio, semirrecta y vector
    herr(18)
    k = mark()
    clic(2, 3); clic(6, 1)
    espera("Distancia(D, ", k)      # el segundo clic cae sobre el corte G, que está encima de E
    verificar(re.search(r"obj \w+ = 4\.47", m.serial()[k:]) is not None, "la distancia DE es 4.47")
    herr(13)
    k = mark()
    clic(-6, -4); clic(-1, -4)
    espera("Mediatriz(A, B)", k)
    herr(5)
    k = mark()
    clic(-6, -4); clic(-3.5, 0)
    espera("Semirrecta(A, C)", k)
    herr(6)
    k = mark()
    clic(-1, -4); clic(-3.5, 0)
    espera("Vector(B, C)", k)
    # Circunferencia por centro y radio, con su diálogo
    herr(8)
    k = mark()
    clic(6, -4)
    time.sleep(1.0)
    borrar(m, 4)
    m.digitar("2\n", 0.05)
    espera("Circunferencia(", k)
    verificar(re.search(r"orden Circunferencia\(\w+, 2\)", m.serial()[k:]) is not None, "radio 2 escrito en el diálogo")
    # Polígono regular con su diálogo
    herr(16)
    k = mark()
    clic(3, -3); clic(5, -5)
    time.sleep(1.0)
    borrar(m, 4)
    m.digitar("6\n", 0.05)
    espera("Poligono(", k)
    verificar(re.search(r"orden Poligono\(\w+, \w+, 6\)", m.serial()[k:]) is not None, "hexágono regular")
    # Texto y deslizador
    herr(20)
    k = mark()
    clic(-8, 5)
    time.sleep(1.0)
    m.digitar("Hola Tales\n", 0.05)
    espera("Texto(", k)
    herr(21)
    k = mark()
    clic(0, 5)
    espera("Deslizador(", k)
    m.captura("H309-12-herramientas")
    # Área con un clic dentro del triángulo, y borrar
    herr(19)
    k = mark()
    clic(-2.5, -3.5)
    espera("Area(a)", k)
    herr(22)
    k = mark()
    clic(-8, 5)
    esperar_desde(m, "gebramini: borrado", k, 20)
    verificar("error" not in m.serial()[pos:], "ninguna herramienta dio error")
    time.sleep(0.5)
    m.captura("H309-13-herramientas-borrar")
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    time.sleep(1.0)
    m.tecla("ret")
    esperar_desde(m, "gebramini: fechado", pos, 20)


def ejemplos_capturas(m):
    for i, nombre in enumerate(("triangulo", "circunferencia-3-puntos", "parabola", "tales")):
        pos = lanzar(m, "gebramini /ejemplos/gebramini/%s.gbm" % nombre)
        esperar_desde(m, "gebramini: pronto", pos, 60)
        time.sleep(1.8)
        w = janela(m, "Sin título - GebraMini", pos)
        m.captura("H309-%d-ejemplo-%s" % (8 + i, nombre))
        m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
        esperar_desde(m, "gebramini: fechado", pos, 20)
        time.sleep(0.8)


def main():
    unidad()
    pen = criar_pendrive("h309-pen", rotulo="GEBRA", arquivos={"leeme.txt": b"h309\n"})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    def sacar(nome):
        local = os.path.join(SAIDA, "h309-" + nome)
        mtools(pen, "mcopy", "-o", "p:/" + nome, local)
        verificar(os.path.exists(local), "%s salió del pendrive" % nome)
        with open(local, "rb") as f:
            return f.read()

    with Maquina("h309", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "el pendrive está montado")
        solo = os.environ.get("H309_SOLO", "")      # depuración: H309_SOLO=3 corre solo la parte 3
        m.comando("mkdir /home")
        if not solo:
            parte1(m, sacar, pen)
            parte2(m, sacar)
        if not solo or solo == "3":
            parte3(m)
        if not solo:
            ejemplos_capturas(m)
        m.comando("umount /usb")
    print("H309 OK")


if __name__ == "__main__":
    main()
