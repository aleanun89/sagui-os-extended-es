#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H312 - PlanoMini, mini CAD 2D (mejora 129, D312).

Parte 0, en el anfitrión: pruebas de unidad del motor (tests/planomini_unidad.c: geometría, intersecciones,
órdenes por línea de comandos, trim/extend/offset/fillet, cotas, capas, REFENT, deshacer, .pmini v1 y v2,
DXF de ida y vuelta y los tres ejemplos) y validación del DXF escrito (con ezdxf si está; si no, con un
validador propio).
Parte 1, sin ventana, en SaguiOS: el .pmini del prototipo se abre, las órdenes por línea de comandos dibujan,
recortan y empalman, se exporta DXF (validado) y PDF (validado con pdf_validar) y se reabre lo guardado.
Parte 2, con ventana: dibujar por órdenes y con el ratón (con REFENT a un punto final), recortar, empalmar,
acotar, capas, selección por ventana y captura, pinzamientos, propiedades, guardar y reabrir, exportar DXF y
PDF. Capturas en docs/capturas/H312-*.png.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, mtools, SAIDA, RAIZ  # noqa: E402
import pdf_validar  # noqa: E402

PROTO = b"PLANOMINI 1\nL 10 10 200 80 0\nC 100 60 0 0 25\nR 20 100 120 160 0\nP 150 150 0 0 0\n"
TIPOS_DXF = {"LINE", "CIRCLE", "ARC", "POLYLINE", "VERTEX", "SEQEND", "TEXT", "POINT", "SOLID", "LWPOLYLINE", "MTEXT", "DIMENSION"}


def validar_dxf(texto):
    """Validador sencillo de DXF ASCII: pares código/valor, secciones, capas, tipos de línea y colores.
    Con ezdxf (si está instalado) además pasa su auditoría. Devuelve el recuento de entidades por tipo."""
    problemas = []
    lineas = texto.replace("\r", "").split("\n")
    if lineas and lineas[-1] == "":
        lineas.pop()
    if len(lineas) % 2:
        problemas.append("número impar de líneas")
        lineas.append("")
    pares = []
    for i in range(0, len(lineas), 2):
        c = lineas[i].strip()
        if not re.fullmatch(r"-?\d+", c):
            problemas.append("código no numérico %r en la línea %d" % (c, i + 1))
            c = "0"
        pares.append((int(c), lineas[i + 1].strip()))
    if pares[-1] != (0, "EOF"):
        problemas.append("no acaba en EOF")
    abiertas = 0
    capas, ltipos, usadas, lt_usados, tipos = set(), set(), set(), set(), {}
    seccion = None
    for k, (c, v) in enumerate(pares):
        if c == 0 and v == "SECTION":
            abiertas += 1
            seccion = pares[k + 1][1]
        elif c == 0 and v == "ENDSEC":
            abiertas -= 1
            seccion = None
        elif seccion == "TABLES" and c == 0 and v in ("LAYER", "LTYPE"):
            nombre = next(vv for cc, vv in pares[k + 1:k + 8] if cc == 2)
            (capas if v == "LAYER" else ltipos).add(nombre)
        elif seccion == "ENTITIES" and c == 0:
            tipos[v] = tipos.get(v, 0) + 1
        elif seccion == "ENTITIES" and c == 8:
            usadas.add(v)
        elif seccion == "ENTITIES" and c == 6:
            lt_usados.add(v)
        elif seccion == "ENTITIES" and c == 62 and not 1 <= int(v) <= 255:
            problemas.append("color ACI fuera de rango (%s)" % v)
        if 10 <= c < 60:
            try:
                float(v)
            except ValueError:
                problemas.append("valor numérico no válido %r con el código %d" % (v, c))
    if abiertas:
        problemas.append("secciones sin equilibrar")
    if not set(tipos) <= TIPOS_DXF:
        problemas.append("entidades desconocidas %s" % sorted(set(tipos) - TIPOS_DXF))
    if capas and not usadas <= capas:
        problemas.append("capas usadas sin declarar %s" % sorted(usadas - capas))
    if ltipos and not lt_usados <= ltipos:
        problemas.append("tipos de línea usados sin declarar %s" % sorted(lt_usados - ltipos))
    verificar(not problemas, "el DXF es válido (%d pares, %d entidades, %d capas)%s" % (len(pares), sum(tipos.values()), len(capas), "" if not problemas else ": " + "; ".join(problemas[:4])))
    try:
        import ezdxf
        import io
        doc = ezdxf.read(io.StringIO(texto))
        auditor = doc.audit()
        verificar(not auditor.errors, "ezdxf: el DXF pasa la auditoría (%s)" % auditor.errors)
    except ImportError:
        pass
    return tipos


def unidad():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "planomini_unidad")
    subprocess.run(["gcc", "-O1", "-Wall", "-Wextra", "-DSAGUI_HOST", "-I", os.path.join(RAIZ, "userland/inc"),
                    "-idirafter", os.path.join(RAIZ, "kernel/inc"), "-I", os.path.join(RAIZ, "userland/gebramini"),
                    "-I", os.path.join(RAIZ, "userland/planomini"), "-finput-charset=UTF-8", "-fexec-charset=ISO-8859-1",
                    "-o", exe, os.path.join(RAIZ, "tests/planomini_unidad.c")] +
                   [os.path.join(RAIZ, "userland/planomini", f) for f in sorted(os.listdir(os.path.join(RAIZ, "userland/planomini"))) if f.endswith(".c")] +
                   ["-lm"], check=True)
    r = subprocess.run([exe, os.path.join(RAIZ, "rootfs/ejemplos/planomini")], capture_output=True)
    salida = r.stdout.decode("latin-1")
    verificar(r.returncode == 0 and "todo bien" in salida, "unidad del motor en el PC: " + repr(salida.strip().splitlines()[-2:]))
    n = re.search(r"(\d+) comprobaciones, (\d+) fallos", salida)
    verificar(n and int(n.group(1)) > 400, "la unidad hace más de 400 comprobaciones (%s)" % (n.group(1) if n else "?"))
    # el DXF de ejemplo y los .pmini se ven bien desde el PC también
    with open(os.path.join(RAIZ, "rootfs/ejemplos/planomini/plano.dxf"), "r") as f:
        validar_dxf(f.read())


def ents(serial):
    """Última descripción de cada entidad: {indice: (tipo, resto)} a partir de «planomini: ent N TIPO ...»."""
    d = {}
    for i, tipo, resto in re.findall(r"(?:^|\n|planomini: )ent (\d+) (\w+) ([^\n]*)", serial):
        d[int(i)] = (tipo, resto)
    return d


def pt(resto):
    if "p=" not in resto:
        return [1e9] * 4
    return [float(v) for v in re.findall(r"-?\d+(?:\.\d+)?", resto.split("p=", 1)[1])][:4]


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


def lanzar(m, comando):
    pos = len(m.serial())
    m.digitar(comando, 0.04)
    m.tecla("ret", 0.04)
    return pos


def parte1(m, sacar):
    out = m.comando("planomini --abrir /usb/proto.pmini --listar", 60)
    verificar("4 entidades" in out and "error" not in out, "el .pmini v1 del prototipo se abre: %s" % out.strip().splitlines()[-1:])
    e = ents(out)
    verificar(e[0][0] == "LINEA" and pt(e[0][1]) == [10, 10, 200, 80], "v1: la línea (%s)" % (e[0],))
    verificar(e[1][0] == "CIRCULO" and "c=(100,60) r=25" in e[1][1], "v1: el círculo")
    verificar(e[2][0] == "POLILINEA" and "nv=4 cerrada=1" in e[2][1], "v1: el rectángulo es una polilínea cerrada")
    verificar(e[3][0] == "PUNTO", "v1: el punto")
    for nombre, minimo in (("habitacion", 35), ("pieza", 16)):
        out = m.comando("planomini --abrir /ejemplos/planomini/%s.pmini" % nombre, 60)
        mm = re.search(r"(\d+) entidades, (\d+) capas", out)
        verificar(mm is not None and int(mm.group(1)) >= minimo, "el ejemplo %s se abre (%s entidades)" % (nombre, mm.group(1) if mm else "?"))
    out = m.comando("planomini --abrir /ejemplos/planomini/plano.dxf", 60)
    mm = re.search(r"(\d+) entidades, (\d+) capas", out)
    verificar(mm is not None and int(mm.group(1)) >= 10 and int(mm.group(2)) == 4, "el DXF de ejemplo se abre: %s" % str(mm.groups() if mm else out[-200:]))

    # dibujar, recortar, empalmar, acotar y capas por la línea de comandos
    out = m.comando("planomini --orden 'CAPA NUEVA Muros' 'CAPA COLOR Muros 1' 'L 0,0' '100,0' '@0,60' '<Intro>' 'L 40,-20 40,20' '<Intro>' 'REC 10,10 40,30' 'C 70,40 15' "
                    "'A 0,0 20,20 40,0' 'COTAHOR 0,0 100,0 50,-20' 'T 5,70 4 0 Texto de prueba' --guardar /usb/a.pmini --dxf /usb/a.dxf --pdf /usb/a.pdf --escala 1", 90)
    verificar("error" not in out.replace("[error]", "") or "[error]" not in out, "las órdenes se aceptan: %s" % [l for l in out.splitlines() if "[error]" in l])
    e = ents(out)
    verificar(e[0][0] == "LINEA" and pt(e[0][1]) == [0, 0, 100, 0] and "capa=Muros" in e[0][1] and "col=ff0000" in e[0][1], "la línea va a la capa Muros con su color: %s" % (e[0],))
    verificar(e[1][0] == "LINEA" and pt(e[1][1]) == [100, 0, 100, 60], "coordenadas relativas: %s" % (e[1],))
    verificar(any(v[0] == "CIRCULO" and "r=15" in v[1] for v in e.values()), "círculo")
    verificar(any(v[0] == "ARCO" and "r=20" in v[1] for v in e.values()), "arco por tres puntos (radio 20)")
    verificar(any(v[0] == "COTA" and 'txt="100"' in v[1] for v in e.values()), "cota horizontal de 100")
    verificar(any(v[0] == "TEXTO" and "Texto de prueba" in v[1] for v in e.values()), "texto")
    out = m.comando("planomini --orden 'L 0,0 100,0' '<Intro>' 'L 40,-20 40,20' '<Intro>' 'L 70,-20 70,20' '<Intro>' 'TR' '<Intro>' '55,0' '<Intro>'", 60)
    e = ents(out)
    horiz = sorted(pt(v[1]) for v in e.values() if v[0] == "LINEA" and abs(pt(v[1])[1]) < 1e-6 and abs(pt(v[1])[3]) < 1e-6)
    verificar(horiz == [[0, 0, 40, 0], [70, 0, 100, 0]], "recortar: el tramo entre dos líneas desaparece (%s)" % (horiz,))
    out = m.comando("planomini --orden 'L 0,0 100,0' '<Intro>' 'L 0,0 0,100' '<Intro>' 'F' 'R' '10' '50,0' '0,50'", 60)
    e = ents(out)
    arcos = [v for v in e.values() if v[0] == "ARCO"]
    verificar(len(arcos) == 1 and "c=(10,10) r=10" in arcos[0][1], "empalme de radio 10 entre dos líneas: %s" % (arcos,))
    lin = sorted(sorted([(l[0], l[1]), (l[2], l[3])]) for l in (pt(v[1]) for v in e.values() if v[0] == "LINEA"))
    verificar([(10, 0), (100, 0)] in lin and [(0, 10), (0, 100)] in lin, "las líneas se acortan hasta los puntos de tangencia (10,0) y (0,10): %s" % (lin,))
    out = m.comando("planomini --orden 'REC 0,0 100,60' 'O 10' '50,0' '50,30' '<Intro>' 'COTARAD' '50,0' '60,60'", 60)
    e = ents(out)
    verificar(sum(1 for v in e.values() if v[0] == "POLILINEA") == 2, "equidistancia de un rectángulo: la polilínea nueva (%s)" % [v[0] for v in e.values()])
    verificar("[error]" in out, "una cota de radio sobre una polilínea se rechaza con un error")

    # lo guardado se reabre igual
    out = m.comando("planomini --abrir /usb/a.pmini --listar", 60)
    e2 = ents(out)
    verificar(len(e2) >= 8 and e2[0][0] == "LINEA" and "capa=Muros" in e2[0][1], "lo guardado se reabre: %d entidades" % len(e2))
    out = m.comando("planomini --abrir /usb/a.dxf --listar", 60)
    verificar("planomini: error" not in out, "el DXF escrito se vuelve a leer: %s" % out.strip().splitlines()[-1:])
    e3 = ents(out)
    verificar(any(v[0] == "LINEA" and "capa=Muros" in v[1] and "col=ff0000" in v[1] for v in e3.values()), "las capas y su color vuelven del DXF")

    # el DXF y el PDF exportados son válidos
    dxf = sacar("a.dxf").decode("latin-1")
    tipos = validar_dxf(dxf)
    verificar(tipos.get("LINE", 0) >= 3 and tipos.get("CIRCLE", 0) == 1 and tipos.get("ARC", 0) == 1 and tipos.get("POLYLINE", 0) == 1 and tipos.get("TEXT", 0) >= 2, "el DXF tiene las entidades esperadas: %s" % tipos)
    verificar("AC1009" in dxf and "Muros" in dxf, "DXF R12 con la capa Muros")
    pdf = pdf_validar.analizar(sacar("a.pdf"))
    verificar(pdf["paginas"] == 1 and abs(pdf["mediabox"][0] - 841.9) < 0.2 and abs(pdf["mediabox"][1] - 595.3) < 0.2, "PDF A4 apaisado %s" % (pdf["mediabox"],))
    t = " ".join(pdf_validar.texto_de(pdf["contenidos"][0]))
    verificar("Escala 1:1" in t and "100" in t and "Texto de prueba" in t, "el PDF lleva cajetín, escala, cota y texto: %r" % t[:120])
    ext = pdf_validar.validar_externo(os.path.join(SAIDA, "h312-a.pdf"))
    if ext:
        for k, (rc, txt) in ext.items():
            verificar(rc == 0, "%s acepta el PDF" % k)
    out = m.comando("visorpdf -t /usb/a.pdf", 60)
    verificar("paginas: 1" in out, "el Visor de PDF abre el PDF de PlanoMini: %s" % out.strip().splitlines()[:2])
    out = m.comando("planomini --abrir /ejemplos/planomini/habitacion.pmini --pdf /usb/hab.pdf --escala 50 --png /usb/hab.png", 120)
    pdf = pdf_validar.analizar(sacar("hab.pdf"))
    t = " ".join(pdf_validar.texto_de(pdf["contenidos"][0]))
    verificar("Escala 1:50" in t and "4000" in t and "SALON" in t, "habitación a 1:50 con sus cotas: %r" % t[:100])
    png = sacar("hab.png")
    verificar(png[:8] == b"\x89PNG\r\n\x1a\n" and len(png) > 3000, "PNG exportado (%d bytes)" % len(png))
    out = m.comando("planomini --abrir /ejemplos/planomini/pieza.pmini --pdf /usb/pieza.pdf --papel A3", 120)
    pdf = pdf_validar.analizar(sacar("pieza.pdf"))
    verificar(abs(pdf["mediabox"][0] - 1190.6) < 0.5 and abs(pdf["mediabox"][1] - 841.9) < 0.5, "pieza en A3 apaisado %s" % (pdf["mediabox"],))


def parte2(m, sacar):
    pos = lanzar(m, "planomini")
    esperar_desde(m, "planomini: pronto", pos, 60)
    time.sleep(1.5)
    w = janela(m, "Sin título - PlanoMini", pos)
    ser = m.serial()[pos:]
    botones = {int(a): (int(b), int(c)) for a, b, c in re.findall(r"planomini: boton (\d+) (\d+) (\d)", ser)}
    verificar(len(botones) >= 40, "las barras tienen sus botones (%d)" % len(botones))
    vx, vy, vw, vh = [int(v) for v in re.search(r"vista area=(\d+),(\d+),(\d+),(\d+)", ser).groups()]
    cx, cy, e100 = [int(v) for v in re.search(r"vista cx=(-?\d+) cy=(-?\d+) e100=(\d+)", ser).groups()]
    cx, cy, esc = cx / 100.0, cy / 100.0, e100 / 100.0

    def px(x, y):
        return w["x"] + int(round(vx + vw / 2 + (x - cx) * esc)), w["y"] + int(round(vy + vh / 2 - (y - cy) * esc))

    def boton(i):
        x, fila = botones[i]
        m.clique(w["x"] + x + 10, w["y"] + 20 + (fila - 1) * 28 + 14)
        time.sleep(0.5)

    def mark():
        return len(m.serial())

    def escribir(t, espera=0.05):
        m.digitar(t + "\n", espera)
        time.sleep(0.5)

    m.captura("H312-1-inicio")

    # ---- dibujar por órdenes en la línea de comandos
    k = mark()
    escribir("L"); escribir("20,20"); escribir("120,20"); escribir("@0,60"); escribir("")
    esperar_desde(m, "planomini: n=2", k, 20)
    time.sleep(0.5)
    e = ents(m.serial()[k:])
    verificar(e[0][0] == "LINEA" and pt(e[0][1]) == [20, 20, 120, 20] and pt(e[1][1]) == [120, 20, 120, 80], "dos segmentos por coordenadas absolutas y relativas: %s %s" % (e[0], e[1]))
    k = mark()
    escribir("C"); escribir("60,60"); escribir("15")
    esperar_desde(m, "planomini: n=3", k, 20)
    time.sleep(0.4)
    verificar(any(v[0] == "CIRCULO" and "c=(60,60) r=15" in v[1] for v in ents(m.serial()[k:]).values()), "círculo centro-radio por órdenes")

    # ---- dibujar con el ratón y REFENT a un punto final: la línea empieza donde acaba la primera, aunque se pinche a 3 px
    boton(100)
    k = mark()
    m.clique(*[a + b for a, b in zip(px(120, 80), (3, -2))])      # cerca del extremo (120,80)
    m.clique(*px(60, 120))
    esperar_desde(m, "planomini: n=4", k, 20)
    time.sleep(0.4)
    e = ents(m.serial()[k:])
    l = [v for v in e.values() if v[0] == "LINEA"][-1]
    p = pt(l[1])
    verificar(abs(p[0] - 120) < 1e-6 and abs(p[1] - 80) < 1e-6, "REFENT: el clic cercano salta exactamente al punto final (120,80): %s" % (p,))
    verificar(abs(p[2] - 60) < 1.2 and abs(p[3] - 120) < 1.2, "el segundo punto cae donde se hizo clic (%s)" % (p,))
    m.digitar("\n", 0.05)         # Intro termina la orden
    time.sleep(0.4)
    # captura del marcador: LINEA activa y el ratón sobre un extremo
    boton(100)
    m.clique(*px(20, 20))
    m.mouse_para(*[a + b for a, b in zip(px(120, 20), (3, 3))])
    time.sleep(0.8)
    m.captura("H312-2-refent-punto-final")
    m.tecla("esc")
    time.sleep(0.5)

    # ---- recortar: una línea que cruza y se corta el tramo derecho
    k = mark()
    escribir("L"); escribir("80,0"); escribir("80,100"); escribir("")
    esperar_desde(m, "planomini: n=5", k, 20)
    time.sleep(0.4)
    boton(121)                                    # RECORTA
    k = mark()
    m.tecla("ret"); time.sleep(0.4)               # bordes: todos
    m.clique(*px(100, 20))                        # el tramo derecho de la primera línea
    esperar_desde(m, "planomini: ent ", k, 20)
    time.sleep(0.6)
    e = ents(m.serial()[k:])
    h = [pt(v[1]) for v in e.values() if v[0] == "LINEA" and abs(pt(v[1])[1] - 20) < 1e-6 and abs(pt(v[1])[3] - 20) < 1e-6]
    verificar(h and abs(h[0][0] - 20) < 1e-6 and abs(h[0][2] - 80) < 1e-6, "recortar con el ratón: la línea horizontal acaba en x=80 (%s)" % (h,))
    m.tecla("ret"); time.sleep(0.4)
    m.captura("H312-3-recortar")

    # ---- empalme con el ratón: dos líneas nuevas que se cortan en (200,20), radio 6
    k = mark()
    escribir("L"); escribir("200,20"); escribir("280,20"); escribir("")
    escribir("L"); escribir("200,20"); escribir("200,100"); escribir("")
    time.sleep(0.6)
    k = mark()
    escribir("F"); escribir("R"); escribir("6")
    m.clique(*px(240, 20)); time.sleep(0.5)
    m.clique(*px(200, 60)); time.sleep(0.9)
    s = m.serial()[k:]
    arcos = [v for v in ents(s).values() if v[0] == "ARCO"]
    verificar(arcos and "c=(206,26) r=6" in arcos[-1][1], "empalme de radio 6 con el ratón: arco con centro (206,26): %s" % (arcos[-1:],))
    m.captura("H312-3b-empalme")
    m.tecla("esc"); time.sleep(0.4)

    # ---- cota
    k = mark()
    escribir("COTAHOR"); escribir("20,20"); escribir("120,20"); escribir("70,0")
    esperar_desde(m, "COTA", k, 20)
    time.sleep(0.5)
    verificar(any(v[0] == "COTA" and 'txt="100"' in v[1] for v in ents(m.serial()[k:]).values()), "cota horizontal de 100")
    m.captura("H312-4-cota")

    # ---- capas
    k = mark()
    escribir("CAPA NUEVA Muros"); escribir("CAPA COLOR Muros 1"); escribir("CAPA LTIPO Muros Trazos")
    escribir("L"); escribir("0,100"); escribir("40,100"); escribir("")
    time.sleep(0.8)
    e = ents(m.serial()[k:])
    ult = e[max(e)]
    verificar("capa=Muros" in ult[1] and "col=ff0000" in ult[1] and "lt=1" in ult[1], "lo dibujado va a la capa actual con su color y tipo de línea: %s" % (ult,))
    k = mark()
    boton(23)
    time.sleep(1.5)
    dl = janela(m, "Administrador de capas", k)
    m.captura("H312-5-capas")
    m.tecla("esc")
    time.sleep(0.8)

    # ---- selección por ventana (izquierda a derecha) y por captura (derecha a izquierda)
    k = mark()
    m.arrastar(*px(5, 10), *px(130, 90))
    time.sleep(0.8)
    s = m.serial()[k:]
    mm = re.search(r"planomini: ventana normal \((\d+) seleccionados\)", s)
    verificar(mm is not None and int(mm.group(1)) >= 3, "ventana: solo lo que queda dentro (%s)" % (mm.group(1) if mm else s[-200:]))
    m.captura("H312-6-seleccion-ventana")
    m.tecla("esc"); time.sleep(0.4)
    k = mark()
    m.arrastar(*px(130, 90), *px(60, 10))
    time.sleep(0.8)
    s = m.serial()[k:]
    mm2 = re.search(r"planomini: ventana captura \((\d+) seleccionados\)", s)
    verificar(mm2 is not None and int(mm2.group(1)) >= 3, "captura: lo que toca (%s)" % (mm2.group(1) if mm2 else s[-200:]))
    m.tecla("esc"); time.sleep(0.4)

    # ---- pinzamiento: elegir la línea (20,20)-(80,20) y arrastrar su extremo a (60,30)
    k = mark()
    m.clique(*px(50, 20))
    esperar_desde(m, "planomini: sel", k, 20)
    time.sleep(0.5)
    m.captura("H312-7-pinzamientos")
    k = mark()
    m.arrastar(*px(80, 20), *px(66, 36))
    esperar_desde(m, "planomini: grip ent", k, 20)
    time.sleep(0.6)
    e = ents(m.serial()[k:])
    mov = [pt(v[1]) for v in e.values() if v[0] == "LINEA" and abs(pt(v[1])[0] - 20) < 1e-6 and abs(pt(v[1])[1] - 20) < 1e-6]
    verificar(mov and abs(mov[0][2] - 66) < 1.5 and abs(mov[0][3] - 36) < 1.5, "el pinzamiento mueve el extremo hasta ~(66,36): %s" % (mov,))
    m.tecla("esc"); time.sleep(0.4)

    # ---- propiedades con doble clic
    k = mark()
    m.clique(*px(60 + 10.607, 60 + 10.607), duplo=True)    # sobre el círculo
    time.sleep(1.6)
    dl = janela(m, "Propiedades", k)
    verificar(dl["l"] == 480, "el doble clic abre las propiedades del objeto")
    m.captura("H312-8-propiedades")
    m.tecla("esc"); time.sleep(0.8)

    # ---- deshacer y rehacer
    k = mark()
    m.tecla("ctrl-z")
    esperar_desde(m, "planomini: deshacer", k, 20)
    m.tecla("ctrl-y")
    esperar_desde(m, "planomini: rehacer", k, 20)
    time.sleep(0.4)

    # ---- guardar por el diálogo, exportar DXF y PDF por la línea de órdenes
    k = mark()
    m.tecla("ctrl-s")
    esperar_desde(m, "dialogo: guardar en", k, 20)
    time.sleep(0.6)
    for _ in range(30):
        m.tecla("backspace", 0.03)
    m.digitar("/usb/h312\n", 0.05)
    esperar_desde(m, "planomini: guardado /usb/h312.pmini", k, 30)
    time.sleep(0.6)
    k = mark()
    escribir("EXPORTAR dxf /usb/h312.dxf")
    esperar_desde(m, "planomini: dxf exportado /usb/h312.dxf", k, 30)
    k = mark()
    escribir("EXPORTAR pdf /usb/h312.pdf 1:2 Plano de prueba")
    esperar_desde(m, "planomini: pdf exportado /usb/h312.pdf", k, 60)
    time.sleep(0.6)
    sacar("h312.pmini")
    validar_dxf(sacar("h312.dxf").decode("latin-1"))
    pdf = pdf_validar.analizar(sacar("h312.pdf"))
    t = " ".join(pdf_validar.texto_de(pdf["contenidos"][0]))
    verificar(pdf["paginas"] == 1 and "Escala 1:2" in t and "Plano de prueba" in t, "el PDF de la ventana se valida (%r)" % t[:80])
    # cerrar
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    time.sleep(1.0)
    ser = m.serial()[k:]
    if "planomini: fechado" not in ser:
        m.tecla("ret"); time.sleep(0.8)
    esperar_desde(m, "planomini: fechado", k, 20)

    # ---- reabrir lo guardado y los ejemplos con la ventana
    pos = lanzar(m, "planomini /usb/h312.pmini")
    esperar_desde(m, "planomini: pronto", pos, 60)
    time.sleep(1.5)
    s = m.serial()[pos:]
    verificar("planomini: abierto /usb/h312.pmini" in s, "se reabre el archivo guardado")
    e = ents(s)
    verificar(any(v[0] == "CIRCULO" and "c=(60,60) r=15" in v[1] for v in e.values()) and any(v[0] == "COTA" for v in e.values()) and any("capa=Muros" in v[1] for v in e.values()), "se conservan círculo, cota y capas (%d entidades)" % len(e))
    m.captura("H312-9-reabierto")
    w = janela(m, "Sin título - PlanoMini", pos)
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    esperar_desde(m, "planomini: fechado", pos, 20)

    for nombre, cap in (("habitacion.pmini", "H312-10-ejemplo-habitacion"), ("pieza.pmini", "H312-11-ejemplo-pieza"), ("plano.dxf", "H312-12-ejemplo-dxf")):
        pos = lanzar(m, "planomini /ejemplos/planomini/" + nombre)
        esperar_desde(m, "planomini: pronto", pos, 60)
        time.sleep(1.8)
        m.captura(cap)
        w = janela(m, "Sin título - PlanoMini", pos)
        m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
        esperar_desde(m, "planomini: fechado", pos, 20)

    # ---- el .pmini del prototipo con la ventana
    pos = lanzar(m, "planomini /usb/proto.pmini")
    esperar_desde(m, "planomini: pronto", pos, 60)
    time.sleep(1.5)
    s = m.serial()[pos:]
    verificar("planomini: abierto /usb/proto.pmini (4 entidades" in s, "el .pmini del prototipo se abre en la ventana")
    m.captura("H312-13-prototipo")
    w = janela(m, "Sin título - PlanoMini", pos)
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    esperar_desde(m, "planomini: fechado", pos, 20)


def main():
    unidad()
    pen = criar_pendrive("h312-pen", rotulo="PLANOMINI", arquivos={"proto.pmini": PROTO})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    def sacar(nome):
        local = os.path.join(SAIDA, "h312-" + nome)
        mtools(pen, "mcopy", "-o", "p:/" + nome, local)
        verificar(os.path.exists(local), "%s salió del pendrive" % nome)
        with open(local, "rb") as f:
            return f.read()

    with Maquina("h312", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "el pendrive está montado")
        solo = os.environ.get("H312_SOLO", "")
        m.comando("mkdir /home")
        if not solo or solo == "1":
            parte1(m, sacar)
        if not solo or solo == "2":
            parte2(m, sacar)
        m.comando("umount /usb")
    print("H312 OK")


if __name__ == "__main__":
    main()
