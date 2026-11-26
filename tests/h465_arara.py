#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H465 - Arara, diagramas al estilo de Visio (D465, v8.5.6.1).

Criterio (QEMU, datos sintéticos; cada paso lee la serie «arara: ...», el fichero que sale por un pendrive o los píxeles):
  (1) PC: el motor sin ventana (`arara --abrir`) abre los tres ejemplos, guarda y reexporta; el .arara guardado es JSON válido
      y los PNG, SVG y PDF que salen son válidos (PNG con las figuras pintadas, SVG que parsea, PDF que valida);
  (2) en SaguiOS: la app abre con la paleta (capturas), se pone una figura con clic y otra arrastrando desde la paleta;
  (3) se conectan con el conector recto (queda pegado a los puntos de conexión de las dos figuras);
  (4) al mover una figura el conector la sigue (los extremos del conector cambian con la figura);
  (5) doble clic escribe texto dentro de la figura; deshacer y rehacer quitan y devuelven el movimiento;
  (6) se guarda con Ctrl+S (diálogo), el JSON del pendrive tiene lo hecho, y se reabre (mismas figuras y conector);
  (7) exportar a PNG, SVG y PDF desde el menú Archivo y desde la línea de órdenes; los tres ejemplos se abren en la ventana;
  (8) el menú Inicio trae «Arara» y los menús Apagar y Reiniciar siguen (no se llenó la tabla de programas).
Y en el anfitrión, la compilación portable (userland/portable, SDL 2 con SDL_VIDEODRIVER=dummy) abre, edita con un guion de ratón,
guarda y exporta con el mismo código (sin cambiar la aplicación).
"""
import json
import os
import re
import shutil
import struct
import subprocess
import sys
import time
import xml.etree.ElementTree as ET
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, mtools, SAIDA, RAIZ, CAPTURAS, ler_ppm  # noqa: E402
import pdf_validar  # noqa: E402

TITULO = "Arara"
EJEMPLOS = os.path.join(RAIZ, "rootfs/ejemplos/arara")


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def esperar(m, patron, tempo=15, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(patron, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    try:
        m.captura("fallo-h465", guardar=False)
    except Exception:
        pass
    ult = "\n".join(l for l in m.serial()[desde:].splitlines() if "arara" in l)[-1500:]
    raise AssertionError(f"tiempo agotado esperando {patron!r}; serie:\n{ult}")


def decodificar_png(datos):
    """PNG RGB de 8 bits -> (ancho, alto, filas) con los filtros Sub y los demás."""
    assert datos[:8] == b"\x89PNG\r\n\x1a\n", "firma PNG"
    i, idat, ancho, alto = 8, b"", 0, 0
    while i < len(datos):
        n, t = struct.unpack(">I4s", datos[i:i + 8])
        c = datos[i + 8:i + 8 + n]
        i += 12 + n
        if t == b"IHDR":
            ancho, alto = struct.unpack(">II", c[:8])
            assert c[8] == 8 and c[9] == 2, "PNG RGB de 8 bits"
        elif t == b"IDAT":
            idat += c
    raw = zlib.decompress(idat)
    tam = ancho * 3
    filas, prev = [], bytearray(tam)
    for y in range(alto):
        f = raw[y * (tam + 1)]
        fila = bytearray(raw[y * (tam + 1) + 1:(y + 1) * (tam + 1)])
        for x in range(tam):
            izq = fila[x - 3] if x >= 3 else 0
            arr = prev[x]
            ul = prev[x - 3] if x >= 3 else 0
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
    return ancho, alto, filas


def contar_colores(png):
    """{(r, g, b): píxeles} de un PNG decodificado."""
    cuenta = {}
    for f in png[2]:
        for px in zip(f[0::3], f[1::3], f[2::3]):
            cuenta[px] = cuenta.get(px, 0) + 1
    return cuenta


def validar_salidas(png, svg, pdf, paginas, con_texto=None):
    """Valida los tres ficheros exportados de un diagrama."""
    p = decodificar_png(png)
    verificar(p[0] > 100 and p[1] > 100, f"PNG: tamaño razonable ({p[0]}x{p[1]})")
    cuenta = contar_colores(p)
    rellenos = [c for c, n in cuenta.items() if n >= 150 and c not in ((255, 255, 255), (0, 0, 0)) and max(c) - min(c) < 120 or (n >= 150 and c not in ((255, 255, 255), (0, 0, 0)))]
    verificar(len(rellenos) >= 3, f"PNG: las figuras están pintadas con sus rellenos ({len(rellenos)} colores)")
    verificar(cuenta.get((0x2F, 0x4F, 0x6F), 0) > 20, "PNG: los conectores están pintados con su color")
    raiz = ET.fromstring(svg)
    ns = "{http://www.w3.org/2000/svg}"
    n_pol = len(raiz.findall(f".//{ns}polygon")) + len(raiz.findall(f".//{ns}ellipse"))
    textos = ["".join(t.itertext()) for t in raiz.findall(f".//{ns}text")]
    verificar(raiz.tag == ns + "svg" and n_pol >= 2, f"SVG: parsea y trae figuras ({n_pol})")
    verificar(len(raiz.findall(f".//{ns}polyline")) >= 1, "SVG: trae los conectores (polyline)")
    if con_texto:
        verificar(con_texto in textos, f"SVG: el texto «{con_texto}» de la figura está ({textos})")
    info = pdf_validar.analizar(pdf)
    verificar(info["paginas"] == paginas, f"PDF: pdf_validar lo acepta con {paginas} página(s) ({info['paginas']})")
    return p, textos


# ---------------------------------------------------------------- parte 1: el anfitrión (compilación portable)

def parte_portable():
    """Compila la app con la capa portable (SDL 2) y la prueba sin QEMU."""
    b = os.path.join(RAIZ, "build", "portable")
    r = subprocess.run(["make", "-C", os.path.join(RAIZ, "userland/portable"), "-j2", "APPS=arara"], capture_output=True)
    verificar(r.returncode == 0 and os.path.exists(os.path.join(b, "arara")), "la compilación portable de Arara funciona: " + r.stderr.decode()[-300:])
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", LANG="es_ES.UTF-8")
    trab = os.path.join(SAIDA, "h465-portable")
    shutil.rmtree(trab, ignore_errors=True)
    os.makedirs(trab)
    for nom, pags, figs, cons in (("flujo", 2, 11, 7), ("red", 1, 7, 6), ("organigrama", 1, 6, 5)):
        base = os.path.join(trab, nom)
        r = subprocess.run([os.path.join(b, "arara"), "--abrir", os.path.join(EJEMPLOS, nom + ".arara"), "--png", base + ".png", "--svg", base + ".svg",
                            "--pdf", base + ".pdf", "--guardar", base + ".rt.arara"], capture_output=True, env=env, cwd=trab, timeout=60)
        out = (r.stdout + r.stderr).decode("latin-1")
        verificar(r.returncode == 0 and f"{pags} paginas, {figs} figuras, {cons} conectores" in out, f"portable: {nom}.arara se abre ({out.strip()})")
        validar_salidas(open(base + ".png", "rb").read(), open(base + ".svg", "rb").read(), open(base + ".pdf", "rb").read(), pags)
        a, c = json.load(open(os.path.join(EJEMPLOS, nom + ".arara"), encoding="utf-8")), json.load(open(base + ".rt.arara", encoding="utf-8"))
        verificar(a == c, f"portable: {nom}.arara guardado es el mismo JSON que el original (ida y vuelta)")
    # ejemplo guardado con el formato propio y reabierto: sin ventana
    r = subprocess.run([os.path.join(b, "arara"), "--abrir", os.path.join(trab, "red.rt.arara"), "--info"], capture_output=True, env=env, timeout=30)
    verificar(b"1 paginas, 7 figuras, 6 conectores" in r.stdout, "portable: lo guardado se reabre con lo mismo")
    # un fichero que no es de Arara no rompe nada
    malo = os.path.join(trab, "malo.arara")
    open(malo, "w").write('{"arara": 1, "paginas": [ {')
    r = subprocess.run([os.path.join(b, "arara"), "--abrir", malo], capture_output=True, env=env, timeout=30)
    verificar(r.returncode != 0 and b"no se pudo abrir" in r.stdout + r.stderr, "portable: un JSON roto se rechaza sin caerse")
    # con ventana (SDL dummy), guion de ratón: dos figuras desde la paleta y guardar con Ctrl+S
    destino = os.path.join(trab, "gui.arara")
    cap = os.path.join(trab, "arara-gui.png")
    guion = os.path.join(trab, "guion.txt")
    open(guion, "w").write("\n".join(["esperar 300", "clique 84 112", "clique 300 200", "clique 84 200", "clique 600 300",
                                       "captura " + cap, "ctrl s", "esperar 300", "sair"]) + "\n")
    r = subprocess.run([os.path.join(b, "arara"), destino], capture_output=True, env=dict(env, SAGUI_ROTEIRO=guion, HOME=trab), cwd=trab, timeout=60)
    verificar(r.returncode == 0, f"portable: con ventana, Arara termina bien (rc={r.returncode})")
    verificar(os.path.exists(cap) and open(cap, "rb").read(8) == b"\x89PNG\r\n\x1a\n", "portable: la captura de la ventana existe")
    r = subprocess.run([os.path.join(b, "arara"), "--abrir", destino, "--info"], capture_output=True, env=env, timeout=30)
    verificar(b"1 paginas, 2 figuras, 0 conectores" in r.stdout, "portable: el guion de ratón puso dos figuras desde la paleta y las guardó: " + r.stdout.decode()[-80:])
    # el repositorio portable que genera tools/portable_repos.py (en una carpeta de prueba; no se sube nada) compila solo
    repos = os.path.join(SAIDA, "h465-repos")
    shutil.rmtree(repos, ignore_errors=True)
    r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools/portable_repos.py"), repos, "arara"], capture_output=True)
    verificar(r.returncode == 0 and os.path.isdir(os.path.join(repos, "arara", "datos/ejemplos/arara")), "portable_repos.py genera el repositorio de Arara: " + r.stderr.decode()[-200:])
    r = subprocess.run(["make", "-C", os.path.join(repos, "arara"), "-j4"], capture_output=True)
    verificar(r.returncode == 0 and os.path.exists(os.path.join(repos, "arara", "build/portable/arara")), "el repositorio de Arara compila solo: " + r.stderr.decode()[-200:])
    r = subprocess.run([os.path.join(repos, "arara", "build/portable/arara"), "--abrir", os.path.join(repos, "arara", "datos/ejemplos/arara/red.arara"), "--info"],
                       capture_output=True, env=env, timeout=30)
    verificar(b"1 paginas, 7 figuras, 6 conectores" in r.stdout, "el binario del repositorio abre su ejemplo")
    return cap


# ---------------------------------------------------------------- parte 2: SaguiOS (QEMU)

def principal():
    if not os.environ.get("H465_SIN_PORTABLE"):
        parte_portable()
    pen = criar_pendrive("h465-pen", rotulo="ARARA")
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    def sacar(nome):
        local = os.path.join(SAIDA, "h465-" + nome)
        if os.path.exists(local):
            os.remove(local)
        mtools(pen, "mcopy", "-o", "p:/" + nome, local)
        verificar(os.path.exists(local), f"{nome} salió del pendrive")
        with open(local, "rb") as f:
            return f.read()

    with Maquina("h465", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "el pendrive está montado")

        # ---------------------------------------------------------------- PC dentro de SaguiOS: motor sin ventana
        out = m.comando("arara --abrir /ejemplos/arara/flujo.arara --png /usb/f.png --svg /usb/f.svg --pdf /usb/f.pdf --guardar /usb/f.arara", 90)
        verificar("arara: 2 paginas, 11 figuras, 7 conectores" in out, "SaguiOS: abre el ejemplo del diagrama de flujo sin ventana")
        png, svg, pdf = sacar("f.png"), sacar("f.svg"), sacar("f.pdf")
        validar_salidas(png, svg, pdf, 2, "Pedidos")
        verificar(json.loads(sacar("f.arara").decode("utf-8"))["paginas"][0]["figuras"][0]["texto"] == "Inicio", "SaguiOS: el .arara guardado es JSON y trae el texto")
        out = m.comando("arara --abrir /ejemplos/arara/red.arara --png /usb/r.png", 60)
        verificar("arara: 1 paginas, 7 figuras, 6 conectores" in out, "SaguiOS: abre el ejemplo de red sin ventana")

        # ---------------------------------------------------------------- la ventana
        m.comando("arara &")
        m.esperar_serial("arara: pronto", 30)
        time.sleep(1.0)
        w = janela(m.serial(), TITULO)
        S = m.serial()

        def pos(nombre):
            return [tuple(int(v) for v in g) for g in re.findall(rf"arara: pos {nombre} (-?\d+) (-?\d+) (-?\d+) (-?\d+)", S)][-1]

        def celda(k):
            g = [tuple(int(v) for v in x) for x in re.findall(r"arara: pos celda (\d+) (-?\d+) (-?\d+) (-?\d+) (-?\d+)", S) if int(x[0]) == k][-1]
            return g[1] + g[3] // 2, g[2] + g[4] // 2

        def boton(i):
            g = [tuple(int(v) for v in x) for x in re.findall(r"arara: pos boton (\d+) (-?\d+) (-?\d+) (-?\d+) (-?\d+)", S) if int(x[0]) == i][-1]
            return g[1] + g[3] // 2, g[2] + g[4] // 2

        zoom, ox, oy = [int(v) for v in re.findall(r"arara: vista zoom (\d+) origen (-?\d+) (-?\d+)", S)[-1]]
        lx, ly, ll, la = pos("lienzo")
        verificar(lx > 100 and ly > 50 and ll > 400 and la > 300, f"la ventana tiene paleta, lienzo e inspector ({lx},{ly},{ll},{la})")

        def mundo(wx, wy):
            return w["x"] + ox + wx * zoom // 1000, w["y"] + oy + wy * zoom // 1000

        def clique(x, y, **k):
            m.clique(x, y, **k)

        m.captura("H465-1-paleta")

        # (2) un clic en la paleta y otro en el lienzo; después arrastrar otra figura desde la paleta
        marca = len(m.serial())
        clique(w["x"] + celda(0)[0], w["y"] + celda(0)[1])
        esperar(m, r"arara: paleta rect", 8, marca)
        clique(*mundo(112, 112))
        g = esperar(m, r"arara: figura 1 rect en (-?\d+) (-?\d+) (\d+) (\d+)", 8, marca)
        x1, y1, l1, a1 = [int(v) for v in g]
        verificar((x1, y1, l1, a1) == (64, 80, 112, 64), f"clic en la paleta y clic en el lienzo ponen un rectángulo alineado a la rejilla ({g})")
        marca = len(m.serial())
        m.arrastar(w["x"] + celda(2)[0], w["y"] + celda(2)[1], *mundo(400, 112))
        g = esperar(m, r"arara: soltar figura elipse en (-?\d+) (-?\d+)", 8, marca)
        g2 = esperar(m, r"arara: figura 2 elipse en (-?\d+) (-?\d+) (\d+) (\d+)", 8, marca)
        x2, y2, l2, a2 = [int(v) for v in g2]
        verificar(abs(x2 + l2 // 2 - 400) <= 8 and abs(y2 + a2 // 2 - 112) <= 8, f"arrastrar una elipse desde la paleta la suelta en el lienzo ({g2})")
        m.captura("H465-2-figuras")

        # (3) conector recto: de la derecha del rectángulo a la izquierda de la elipse
        S = m.serial()
        clique(w["x"] + boton(6)[0], w["y"] + boton(6)[1])           # herramienta «Recto»
        marca = len(m.serial())
        m.arrastar(*mundo(x1 + l1 - 4, y1 + a1 // 2), *mundo(x2 + 4, y2 + a2 // 2))
        g = esperar(m, r"arara: conector nuevo (\d+) de (\d+)/(\d+) a (\d+)/(\d+)", 8, marca)
        verificar(g == ("3", "1", "1", "2", "3"), f"el conector queda pegado al punto derecho del rectángulo y al izquierdo de la elipse ({g})")
        c = esperar(m, r"arara: conector 3 de 1/1 a 2/3 desde (-?\d+) (-?\d+) hasta (-?\d+) (-?\d+)", 8, marca)
        c0 = [int(v) for v in c]
        verificar(c0 == [x1 + l1, y1 + a1 // 2, x2, y2 + a2 // 2], f"los extremos del conector son los puntos de conexión ({c0})")
        m.captura("H465-3-conector")

        # (4) volver al puntero y mover el rectángulo: el conector lo sigue
        clique(w["x"] + boton(5)[0], w["y"] + boton(5)[1])           # «Puntero»
        marca = len(m.serial())
        m.arrastar(*mundo(x1 + 20, y1 + 20), *mundo(x1 + 20, y1 + 20 + 96))
        c = esperar(m, r"arara: conector 3 de 1/1 a 2/3 desde (-?\d+) (-?\d+) hasta (-?\d+) (-?\d+)", 10, marca)
        c1 = [int(v) for v in c]
        verificar(c1[1] == c0[1] + 96 and c1[0] == c0[0] and c1[2:] == c0[2:], f"al mover el rectángulo 96 hacia abajo el conector la sigue ({c0} -> {c1})")
        esperar(m, r"arara: orden Mover", 8, marca)
        m.captura("H465-4-conector-sigue")

        # (5) texto con doble clic; deshacer y rehacer
        marca = len(m.serial())
        m.clique(*mundo(x1 + l1 // 2, y1 + 96 + a1 // 2), duplo=True)
        esperar(m, r"arara: editar texto figura 1", 8, marca)
        m.digitar("Hola", 0.06)
        m.tecla("esc", 0.4)
        esperar(m, r'arara: texto figura 1 "Hola"', 8, marca)
        m.captura("H465-5-texto")
        marca = len(m.serial())
        m.tecla("ctrl-z", 0.6)
        esperar(m, r"arara: deshacer", 8, marca)
        m.tecla("ctrl-z", 0.6)
        c = esperar(m, r"arara: conector 3 de 1/1 a 2/3 desde (-?\d+) (-?\d+) hasta", 8, marca)
        verificar(int(c[1]) == c0[1], f"dos veces Ctrl+Z quitan el texto y el movimiento: el conector vuelve a su sitio ({c})")
        marca = len(m.serial())
        m.tecla("ctrl-y", 0.6)
        m.tecla("ctrl-y", 0.6)
        c = esperar(m, r"arara: conector 3 de 1/1 a 2/3 desde (-?\d+) (-?\d+) hasta", 8, marca)
        verificar(int(c[1]) == c0[1] + 96, "Ctrl+Y rehace el movimiento")

        # selector de color con alfa: figura elegida y el inspector
        clique(*mundo(x1 + 20, y1 + 96 + 20))
        esperar(m, r"arara: seleccion 1 figuras 0 conectores", 8, marca)
        m.captura("H465-6-inspector")

        # (6) guardar con Ctrl+S (diálogo) en el pendrive y reabrir
        marca = len(m.serial())
        m.tecla("ctrl-s", 0.6)
        m.esperar_serial('"Guardar como"', 15)
        time.sleep(0.5)
        for _ in range(30):
            m.tecla("backspace", 0.03)
        m.digitar("/usb/prueba.arara", 0.05)
        m.tecla("ret", 0.6)
        esperar(m, r"arara: guardado /usb/prueba.arara \d+ bytes 1 paginas 2 figuras 1 conectores", 15, marca)
        doc = json.loads(sacar("prueba.arara").decode("utf-8"))
        figs, cons = doc["paginas"][0]["figuras"], doc["paginas"][0]["conectores"]
        verificar([f["tipo"] for f in figs] == ["rect", "elipse"] and figs[0]["texto"] == "Hola", f"el .arara guardado trae las dos figuras y el texto ({[f['tipo'] for f in figs]})")
        verificar(figs[0]["y"] == y1 + 96 and cons[0]["de"] == 1 and cons[0]["a"] == 2 and cons[0]["pto_de"] == 1 and cons[0]["pto_a"] == 3,
                  "el JSON guarda la posición movida y el conector con sus puntos de conexión")

        # (7) exportar desde el menú Archivo
        S = m.serial()
        menus = [tuple(int(v) for v in x) for x in re.findall(r"arara: pos menu (\d+) (-?\d+) (-?\d+)", S)]
        mx = menus[0][1] + 8
        for nombre, linea in (("png", 0), ("svg", 1), ("pdf", 2)):
            clique(w["x"] + mx, w["y"] + 10)                         # menú «Archivo»
            time.sleep(0.6)
            if nombre == "png":
                m.captura("H465-7-menu-archivo")
            clique(w["x"] + mx + 20, w["y"] + 131 + linea * 22)   # su línea: cuatro de 22 px y un separador de 9 por encima de «PNG»
            m.esperar_serial('"Exportar"', 15)
            time.sleep(0.5)
            for _ in range(40):
                m.tecla("backspace", 0.03)
            m.digitar(f"/usb/x.{nombre}", 0.05)
            m.tecla("ret", 0.6)
            esperar(m, rf"arara: {nombre} /usb/x.{nombre} ", 20, marca)
        png, svg, pdf = sacar("x.png"), sacar("x.svg"), sacar("x.pdf")
        p, textos = validar_salidas(png, svg, pdf, 1, "Hola")

        # cerrar la ventana (la X de la moldura: no hay cambios sin guardar) y reabrir lo guardado
        def cerrar(w):
            k = len(m.serial())
            m.clique(w["x"] + w["l"] - 12, w["y"] - 12)
            esperar(m, r"arara: fechado", 10, k)
            time.sleep(0.8)

        marca = len(m.serial())
        cerrar(w)
        m.comando("arara /usb/prueba.arara &")
        esperar(m, r"arara: abierto /usb/prueba.arara 1 paginas 2 figuras 1 conectores", 20, marca)
        time.sleep(0.8)
        w = janela(m.serial(), TITULO)
        m.captura("H465-8-reabierto")
        cerrar(w)
        # los ejemplos en la ventana
        for nom, patron in (("flujo", "2 paginas 11 figuras 7 conectores"), ("red", "1 paginas 7 figuras 6 conectores"), ("organigrama", "1 paginas 6 figuras 5 conectores")):
            marca = len(m.serial())
            m.comando(f"arara /ejemplos/arara/{nom}.arara &")
            esperar(m, rf"arara: abierto /ejemplos/arara/{nom}.arara {patron}", 20, marca)
            time.sleep(1.0)
            w = janela(m.serial(), TITULO)
            m.captura(f"H465-9-{nom}")
            cerrar(w)

        # (8) el menú Inicio: «Arara (diagramas)» en Accesorios y siguen «Apagar...» y «Reiniciar...»
        import h109_menu_inicio as h109
        marca = len(m.serial())
        m.clique(40, 12)
        esperar(m, r"\[GUI\] menu Inicio aberto \(mouse(, estilo \d+)?\)", 10, marca)
        px0, py0, pl, pa = h109.painel_do_log(m, marca)
        linhas = h109.linhas_do_log(m, marca)
        verificar("Apagar..." in linhas and "Reiniciar..." in linhas, f"el menú Inicio sigue llevando «Apagar...» y «Reiniciar...» ({len(linhas)} líneas)")
        ys = h109.y_das_linhas(py0, linhas)
        marca2 = len(m.serial())
        m.clique(px0 + 34 + 30, ys[linhas.index("Accesorios")] + h109.LINHA_A // 2)
        h109.painel_do_log(m, marca2, sub=True)
        sub = h109.linhas_do_log(m, marca2)
        verificar("Arara (diagramas)" in sub, f"Inicio > Accesorios trae «Arara (diagramas)» ({sub})")
        time.sleep(0.4)
        m.captura("H465-10-inicio")
        m.tecla("esc", 0.3)
    print("H465 OK")


if __name__ == "__main__":
    principal()
