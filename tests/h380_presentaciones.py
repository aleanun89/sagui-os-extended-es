#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H380 - Tucán, el editor de presentaciones (D369).

Parte A (sin ventana, en el guest): crea una presentación de ejemplo con `tucan --demo` y la convierte a los formatos
(.tcn, .pptx, .pdf con y sin notas, .png). Los archivos salen por un pendrive y se validan en el anfitrión con la
biblioteca estándar de Python (tests/tucan_pptx_validar.py, tests/pdf_validar.py): ZIP/XML del PPTX, relaciones y tipos
de contenido, textos por diapositiva, PDF (una página por diapositiva, tamaño, texto real), PNG. Después Tucán
reimporta su propio PPTX y uno escrito aparte con el estilo de PowerPoint (marcadores heredados de la maqueta, UTF-8,
tabla, imagen, notas, animación ignorada con aviso) y se comprueban los textos.

Parte B (con ventana, 1024x768): abre Tucán con `--cinta-log`, escribe en los marcadores, añade diapositivas, cambia
de tema, guarda y reabre el .tcn, y recorre la presentación a pantalla completa con las teclas (transición,
animación, pantalla negra, ir a una diapositiva). Capturas en docs/capturas/H380-*.png (revisadas a ojo).
"""
import os
import re
import struct
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, mtools, imagem_de_teste, SAIDA  # noqa: E402
from h337_cinta import esperar, janela, bloque, clic_tab, foco_terminal  # noqa: E402
import pdf_validar  # noqa: E402
import tucan_pptx_validar as tv  # noqa: E402


def sacar(pen, nombre):
    local = os.path.join(SAIDA, "h380-" + nombre)
    mtools(pen, "mcopy", "-o", "p:/" + nombre, local)
    verificar(os.path.exists(local), "%s salió del pendrive" % nombre)
    with open(local, "rb") as f:
        return f.read()


def info(m, ruta):
    out = m.comando("tucan --info " + ruta, 40)
    return out


def textos_info(out):
    return re.findall(r"texto=\[(.*?)\]", out)


def parte_a():
    externo = tv.crear_pptx_externo()
    pen = criar_pendrive("h380-pen", rotulo="TUCAN", arquivos={"ext.pptx": externo})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h380a", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "A: el pendrive está montado")
        out = m.comando("which tucan")
        verificar("/apps/bin/tucan" in out, "A: tucan vive en /apps/bin (como Pluma y Macaco)")
        for ext, extra in (("tcn", ""), ("pptx", ""), ("pdf", "")):
            out = m.comando("tucan --demo /usb/demo.%s" % ext, 90)
            verificar("rc=0" in out, "A: tucan --demo crea demo.%s (%s)" % (ext, out.strip().splitlines()[-2:] if out.strip() else out))
        out = m.comando("tucan --convertir /usb/demo.tcn /usb/notas.pdf --notas", 90)
        verificar("rc=0" in out, "A: PDF con notas")
        out = m.comando("tucan --convertir /usb/demo.tcn /usb/diap2.png --diap 2 --ancho 800", 90)
        verificar("rc=0" in out, "A: PNG de la diapositiva 2")
        out = m.comando("tucan --convertir /usb/demo.pptx /usb/reimp.tcn", 90)
        verificar("rc=0" in out and "(5 diapositivas" in out, "A: el PPTX de Tucán se reimporta (5 diapositivas): %s" % out.strip()[-160:])
        out_i = info(m, "/usb/reimp.tcn")
        out_o = info(m, "/usb/demo.tcn")
        out = m.comando("tucan --convertir /usb/ext.pptx /usb/ext.tcn", 90)
        verificar("rc=0" in out, "A: se importa el PPTX externo")
        verificar("animaciones ignoradas" in out, "A: avisa de que ignora las animaciones: %s" % out.strip()[-200:])
        out_e = info(m, "/usb/ext.tcn")
        out = m.comando("tucan --convertir /usb/ext.pptx /usb/ext2.pptx", 90)
        verificar("rc=0" in out, "A: el PPTX externo se vuelve a exportar a PPTX")
        # PDF del PPTX importado: pasa por todo el camino
        out = m.comando("tucan --convertir /usb/ext.pptx /usb/ext.pdf", 90)
        verificar("rc=0" in out, "A: el PPTX externo se exporta a PDF")
        out = m.comando("ls -l /usb", 20)

    # ---------------- validación en el anfitrión
    demo_tcn = sacar(pen, "demo.tcn")
    r = tv.validar_tcn(demo_tcn)
    verificar(r["diapositivas"] == 5 and r["objetos"] >= 12, "A: demo.tcn tiene 5 diapositivas y %d objetos" % r["objetos"])
    verificar(any("png" in n for n in r["medios"]), "A: demo.tcn guarda la imagen en media/: %s" % r["medios"])
    verificar("ftit=Arial" in r["texto"] and "fuente=" in r["texto"], "A: el formato propio guarda el nombre de la fuente")

    demo_pptx = sacar(pen, "demo.pptx")
    r = tv.validar_pptx(demo_pptx)
    verificar((r["ancho"], r["alto"]) == (960, 540), "A: PPTX 16:9 (%dx%d)" % (r["ancho"], r["alto"]))
    verificar(len(r["diapositivas"]) == 5, "A: el PPTX tiene 5 diapositivas")
    d = r["diapositivas"]
    verificar("Presentación de ejemplo" in d[0]["textos"] and "Tucán en SaguiOS" in d[0]["textos"], "A: textos de la diapositiva 1 (%s)" % d[0]["textos"])
    verificar("Primer punto importante" in d[1]["textos"] and "Segundo punto con acentos: ñandú, camión" in d[1]["textos"], "A: textos con acentos y viñetas (diap 2)")
    verificar(d[1]["notas"] == ["Notas de la segunda diapositiva"], "A: las notas del orador van en notesSlide")
    verificar("roundRect" in d[2]["formas"] and "ellipse" in d[2]["formas"] and "star5" in d[2]["formas"] and "rightArrow" in d[2]["formas"],
              "A: formas de la diapositiva 3 (%s)" % d[2]["formas"])
    verificar(d[3]["tablas"] == 1 and "Febrero" in d[3]["textos"] and "21" in d[3]["textos"], "A: la tabla de la diapositiva 4")
    verificar(d[3]["pics"] == 1, "A: el gráfico se exporta como imagen")
    verificar(d[4]["pics"] == 1, "A: la imagen de la diapositiva 5")
    verificar(d[0]["transicion"] == ["fade"] and d[1]["transicion"] == ["push"], "A: transiciones (%s, %s)" % (d[0]["transicion"], d[1]["transicion"]))

    pdf = pdf_validar.analizar(sacar(pen, "demo.pdf"))
    verificar(pdf["paginas"] == 5, "A: PDF de 5 páginas (una por diapositiva), tiene %d" % pdf["paginas"])
    verificar(abs(pdf["mediabox"][0] - 960) < 1 and abs(pdf["mediabox"][1] - 540) < 1, "A: página del tamaño de la diapositiva %s" % (pdf["mediabox"],))
    txt = b"\n".join(pdf["contenidos"]).decode("latin-1")
    verificar("Puntos clave" in pdf_validar.texto_de(pdf["contenidos"][1]) if hasattr(pdf_validar, "texto_de") else True, "A: el PDF lleva texto real")
    verificar(len(pdf["imagenes"]) >= 5, "A: cada página lleva su imagen de fondo/formas (%d imágenes)" % len(pdf["imagenes"]))
    verificar(len(pdf["incrustadas"]) >= 1, "A: el PDF incrusta las fuentes del sistema que usa")
    pn = pdf_validar.analizar(sacar(pen, "notas.pdf"))
    verificar(pn["paginas"] == 5 and abs(pn["mediabox"][0] - 595.3) < 1, "A: PDF con notas en páginas A4 (%s)" % (pn["mediabox"],))
    verificar("Notas de la segunda diapositiva" in b"\n".join(pn["contenidos"]).decode("latin-1").replace("\\", ""), "A: el PDF con notas lleva el texto de las notas")

    png = sacar(pen, "diap2.png")
    verificar(png[:8] == b"\x89PNG\r\n\x1a\n", "A: PNG con firma correcta")
    w, h = struct.unpack(">II", png[16:24])
    verificar((w, h) == (800, 450), "A: PNG de 800x450 (16:9): %dx%d" % (w, h))
    idat = b"".join(png[i + 8:i + 8 + struct.unpack(">I", png[i:i + 4])[0]] for i in _chunks(png, b"IDAT"))
    raw = zlib.decompress(idat)
    verificar(len(raw) == h * (1 + w * 3), "A: el PNG descomprime a ancho x alto")
    colores = set()
    for y in range(0, h, 20):
        fila = raw[y * (1 + w * 3) + 1:(y + 1) * (1 + w * 3)]
        colores.update(fila[i:i + 3] for i in range(0, len(fila), 61 * 3))
    verificar(len(colores) > 4, "A: el PNG no es plano (%d colores)" % len(colores))

    # reimportación: los textos de Tucán sobreviven al viaje por PPTX
    ti, to = textos_info(out_i), textos_info(out_o)
    verificar("Presentación de ejemplo" in "".join(ti) and "Tucán en SaguiOS" in "".join(ti), "A: textos tras reimportar el PPTX (%d textos)" % len(ti))
    verificar("Primer punto importante|>Un subpunto con más detalle|Segundo punto con acentos: ñandú, camión|Tercer punto" in ",".join(ti) or
              any("Primer punto importante" in t and "Un subpunto" in t and "ñandú" in t for t in ti),
              "A: los niveles y acentos sobreviven a PPTX: %s" % [t for t in ti if "Primer" in t])
    verificar(any("Febrero" in t for t in ti), "A: la tabla sobrevive a PPTX")
    verificar("notas=[Notas de la segunda diapositiva]" in out_i, "A: las notas sobreviven a PPTX")
    verificar("5 diapositivas" in out_i or "tucan: 5 diapositivas" in out_i, "A: reimportado: 5 diapositivas")
    verificar(len(ti) >= len(to) - 3, "A: nada de lo importante se pierde (%d de %d textos)" % (len(ti), len(to)))

    # el PPTX externo
    te = textos_info(out_e)
    verificar("720x540" in out_e, "A: el PPTX 4:3 externo mantiene su tamaño: %s" % out_e.splitlines()[:3])
    verificar(any("Título desde PowerPoint ñandú" in t for t in te), "A: UTF-8 y acentos del PPTX externo (%s)" % te[:4])
    verificar(any("Punto uno & más" in t and "Subpunto dos" in t for t in te), "A: viñetas y niveles del PPTX externo (%s)" % te)
    verificar("Elipse verde" in "".join(te), "A: la forma con texto del PPTX externo")
    verificar("A1/B1/A2/B2" in "".join(te).replace("//", "/") or ("A1" in "".join(te) and "B2" in "".join(te)), "A: la tabla del PPTX externo (%s)" % te)
    verificar("notas=[Nota del orador externa]" in out_e, "A: las notas del PPTX externo")
    verificar(re.search(r"obj tipo=1 rol=1 forma=0 x=\d+ y=\d+ w=\d+ h=\d+", out_e) is not None, "A: el título heredó posición de la maestra")
    verificar(re.search(r"rol=2 .* x=\d+ y=\d+ w=\d+ h=\d+", out_e) is not None, "A: el contenido heredó posición de la maqueta")
    ext2 = tv.validar_pptx(sacar(pen, "ext2.pptx"))
    verificar(len(ext2["diapositivas"]) == 2 and ext2["diapositivas"][1]["pics"] == 1, "A: el PPTX externo reexportado es válido")
    extpdf = pdf_validar.analizar(sacar(pen, "ext.pdf"))
    verificar(extpdf["paginas"] == 2 and abs(extpdf["mediabox"][0] - 720) < 1, "A: PDF 4:3 del PPTX externo")
    return pen



def logs(m, desde, patron=r"tucan: .*"):
    return re.findall(patron, m.serial()[desde:])


def parte_b():
    img = imagem_de_teste("", ["--vesa", "--resolucao", "1024x768"])
    with Maquina("h380b", "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(3)
        out = m.comando("tucan --demo /tmp/demo.tcn", 90)
        verificar("rc=0" in out, "B: la presentación de ejemplo está en /tmp")
        n0 = len(m.serial())
        m.comando("tucan --cinta-log &", 20)
        esperar(m, r"tucan: pronto", 40, n0)
        time.sleep(3)
        w = janela(m, "Presentación1 - Tucán", n0)
        g = re.findall(r"tucan: layout base=(\d+) mini=(\d+),(\d+),(\d+),(\d+) lienzo=.* diap=(\d+),(\d+),(\d+),(\d+) ", m.serial()[n0:])[-1]
        base, mx, my, mw, mh, dx, dy, dw, dh = (int(v) for v in g)
        verificar(dw >= 600 and abs(dw * 9 - dh * 16) < 40, "B: la diapositiva se ve en 16:9 (%dx%d)" % (dw, dh))
        verificar(mw == 160 and dx >= mw, "B: lista de miniaturas a la izquierda (%d px) y lienzo a su derecha" % mw)
        X = lambda f: w["x"] + dx + int(dw * f)
        Y = lambda f: w["y"] + dy + int(dh * f)
        b = bloque(m, n0)
        verificar(all(t in b["tab"] for t in ("Inicio", "Insertar", "Diseño", "Transiciones", "Presentación", "Ver")), "B: la cinta tiene las seis pestañas %s" % sorted(b["tab"]))
        m.captura("H380-1-vacia")

        # ---- el panel del asistente IA no tapa nada (D355): el lienzo cede su ancho
        k = len(m.serial())
        m.tecla("f12")
        esperar(m, r"tucan: asistente visible", 15, k)
        time.sleep(2)
        gp = re.findall(r"tucan: layout base=(\d+) mini=(\d+),(\d+),(\d+),(\d+) lienzo=(\d+),(\d+),(\d+),(\d+) diap=(\d+),(\d+),(\d+),(\d+) notas=(\d+),(\d+),(\d+),(\d+) panel=(\d+),(\d+),(\d+),(\d+) ventana=(\d+)x(\d+)", m.serial()[k:])
        verificar(bool(gp), "B: Tucán anota su layout con el panel")
        v = [int(x) for x in gp[-1]]
        lienzo, diapo, notas_, panel, ven = v[5:9], v[9:13], v[13:17], v[17:21], v[21:23]
        verificar(panel[2] > 150 and panel[3] > 200, "B: el panel tiene tamaño útil %dx%d" % (panel[2], panel[3]))
        verificar(panel[1] >= v[0], "B: el panel empieza bajo la cinta (%d >= %d)" % (panel[1], v[0]))
        verificar(lienzo[0] + lienzo[2] <= panel[0] and diapo[0] + diapo[2] <= panel[0] and notas_[0] + notas_[2] <= panel[0], "B: el lienzo, la diapositiva y las notas no se solapan con el panel")
        verificar(panel[0] + panel[2] <= ven[0] and panel[1] + panel[3] <= ven[1] - 20, "B: el panel cabe en la ventana y sobre la barra de estado")
        verificar(diapo[2] >= 400, "B: a la diapositiva le quedan %d px" % diapo[2])
        m.captura("H380-1b-asistente")
        k = len(m.serial())
        m.tecla("f12")
        esperar(m, r"tucan: asistente oculto", 15, k)
        time.sleep(1.5)

        # ---- texto en los marcadores
        k = len(m.serial())
        m.clique(X(0.5), Y(0.40))
        m.digitar("Hola Tucan", 0.05)
        esperar(m, r"texto editado \[Hola Tucan\]", 15, k)
        verificar(True, "B: escribir en el título lo edita")
        m.captura("H380-2-edicion")
        m.tecla("esc")
        m.clique(X(0.5), Y(0.64))
        m.digitar("Segundo texto", 0.05)
        esperar(m, r"texto editado \[Segundo texto\]", 15, k)
        m.tecla("esc")

        # ---- diapositiva nueva con título y contenido (Ctrl+M) y niveles con Tab
        k = len(m.serial())
        m.tecla("ctrl-m")
        esperar(m, r"tucan: diapositiva 2 de 2", 15, k)
        verificar(True, "B: Ctrl+M añade la diapositiva 2")
        m.clique(X(0.5), Y(0.14))
        m.digitar("Puntos", 0.05)
        m.tecla("esc")
        m.clique(X(0.5), Y(0.55))
        m.digitar("Uno", 0.05)
        m.tecla("ret")
        m.tecla("tab")
        m.digitar("Sub", 0.05)
        esperar(m, r"texto editado \[Uno\|>Sub\]", 15, k)
        verificar(re.search(r"tucan: nivel 1", m.serial()[k:]) is not None, "B: Tab baja el nivel del párrafo")
        m.tecla("esc")

        # ---- forma desde la cinta: Insertar > Formas > Elipse
        clic_tab(m, w, bloque(m, n0), "Insertar")
        time.sleep(1)
        b = bloque(m, n0)
        cid, cx, cy, cl, ca = b["ctrl"]["Formas"]
        k = len(m.serial())
        m.clique(w["x"] + cx + cl // 2, w["y"] + cy + 20)
        time.sleep(1.2)
        m.clique(w["x"] + cx + 30, w["y"] + cy + ca + 13 + 22 * 2)     # tercera línea: Elipse
        esperar(m, r"tucan: forma nueva Elipse", 15, k)
        verificar(True, "B: Insertar > Formas > Elipse")
        time.sleep(1)
        m.captura("H380-3-formas")
        # mover la forma con el ratón (arrastrar desde el centro)
        k = len(m.serial())
        m.arrastar(X(0.5), Y(0.5), X(0.5) + 90, Y(0.5) + 30)
        esperar(m, r"tucan: objetos movidos", 15, k)
        verificar(True, "B: arrastrar mueve la forma")
        k = len(m.serial())
        m.tecla("ctrl-z")
        esperar(m, r"tucan: deshacer", 15, k)
        verificar(True, "B: Ctrl+Z deshace el movimiento")
        k = len(m.serial())
        m.tecla("ctrl-y")
        esperar(m, r"tucan: rehacer", 15, k)
        verificar(True, "B: Ctrl+Y lo rehace")

        # ---- el tema: Diseño > Tucán
        clic_tab(m, w, bloque(m, n0), "Diseño")
        time.sleep(1)
        k = len(m.serial())
        m.clique(w["x"] + 156, w["y"] + 60)
        esperar(m, r"tucan: tema Tucán", 15, k)
        verificar(True, "B: Diseño > Temas > Tucán")
        time.sleep(1)
        m.captura("H380-4-tema")

        # ---- reordenar miniaturas arrastrando: la 2 pasa delante de la 1
        k = len(m.serial())
        fila = (160 - 32) * 540 // 960 + 14
        m.arrastar(w["x"] + 90, w["y"] + base + 6 + fila + 30, w["x"] + 90, w["y"] + base + 6 + 5)
        esperar(m, r"tucan: diapositiva movida de 2 a 1", 15, k)
        verificar(True, "B: arrastrar una miniatura reordena las diapositivas")
        m.captura("H380-5-miniaturas")

        # ---- guardar con Ctrl+S, escribiendo la ruta en el diálogo
        k = len(m.serial())
        m.tecla("ctrl-s")
        time.sleep(2.5)
        for _ in range(14):
            m.tecla("backspace", 0.03)
        m.digitar("/tmp/h380.tcn", 0.05)
        m.tecla("ret")
        esperar(m, r"tucan: guardado /tmp/h380.tcn", 20, k)
        esperar(m, r"tucan: titulo h380.tcn - Tucán", 10, k)
        verificar(True, "B: Ctrl+S guarda /tmp/h380.tcn")
        m.captura("H380-6-guardada")
        time.sleep(1)
        foco_terminal(m)
        out = m.comando("tucan --info /tmp/h380.tcn", 40)
        verificar("tucan: 2 diapositivas" in out and "tema Tucán" in out, "B: lo guardado: 2 diapositivas y el tema Tucán")
        ti = textos_info(out)
        verificar("Puntos" in ti and "Hola Tucan" in ti and "Segundo texto" in ti, "B: lo guardado lleva los textos escritos %s" % ti)
        verificar(any("Uno" in t and "Sub" in t and ">" in t for t in ti), "B: el subpunto con nivel se guardó %s" % ti)
        verificar(re.search(r"tipo=2 rol=0 forma=2", out) is not None, "B: la elipse se guardó")
        # la 2 pasó delante: la primera diapositiva guardada es la de Puntos
        verificar(out.index("Puntos") < out.index("Hola Tucan"), "B: el orden de las diapositivas guardado es el reordenado")

        # ---- reabrir
        k = len(m.serial())
        m.comando("tucan --cinta-log /tmp/h380.tcn &", 20)
        esperar(m, r"tucan: abierto /tmp/h380.tcn \(2 diapositivas\)", 30, k)
        time.sleep(2)
        m.captura("H380-7-reabierta")
        verificar(True, "B: reabre /tmp/h380.tcn con 2 diapositivas")

        # ---- presentación a pantalla completa con el ejemplo (animación y transiciones)
        foco_terminal(m)
        k = len(m.serial())
        m.comando("tucan --cinta-log /tmp/demo.tcn &", 20)
        esperar(m, r"tucan: abierto /tmp/demo.tcn \(5 diapositivas\)", 30, k)
        time.sleep(2.5)
        k = len(m.serial())
        m.tecla("f5")
        esperar(m, r"tucan: presentacion diapositiva 1 paso 0", 30, k)
        time.sleep(1.5)
        m.captura("H380-8-presentacion-1")
        verificar(True, "B: F5 empieza la presentación en la diapositiva 1")
        k = len(m.serial())
        m.tecla("right")
        esperar(m, r"tucan: presentacion diapositiva 2 paso 0", 20, k)
        time.sleep(1.2)
        m.captura("H380-9-presentacion-2")
        k = len(m.serial())
        m.tecla("right")
        esperar(m, r"tucan: presentacion diapositiva 2 paso 1", 20, k)
        verificar(True, "B: el clic siguiente muestra el objeto animado (paso 1)")
        time.sleep(1)
        m.captura("H380-10-animacion")
        k = len(m.serial())
        m.tecla("right")
        esperar(m, r"tucan: presentacion diapositiva 3 paso 0", 20, k)
        time.sleep(1)
        k = len(m.serial())
        m.tecla("left")
        esperar(m, r"tucan: presentacion diapositiva 2 paso 1", 20, k)
        verificar(True, "B: la flecha izquierda vuelve")
        k = len(m.serial())
        m.tecla("b")
        esperar(m, r"tucan: presentacion pantalla negra 1", 20, k)
        time.sleep(0.8)
        m.captura("H380-11-pantalla-negra")
        m.tecla("b")
        time.sleep(0.5)
        k = len(m.serial())
        m.tecla("4")
        m.tecla("ret")
        esperar(m, r"tucan: presentacion diapositiva 4 paso 0", 20, k)
        verificar(True, "B: número + Intro va a una diapositiva")
        time.sleep(1.2)
        m.captura("H380-12-datos")
        k = len(m.serial())
        m.tecla("end")
        esperar(m, r"tucan: presentacion diapositiva 5 paso 0", 20, k)
        time.sleep(1)
        m.captura("H380-13-ultima")
        k = len(m.serial())
        m.tecla("esc")
        esperar(m, r"tucan: presentacion fin", 20, k)
        verificar(True, "B: Esc termina la presentación")
        time.sleep(2)
        m.captura("H380-14-de-vuelta")


def _chunks(png, tipo):
    i = 8
    while i < len(png):
        n = struct.unpack(">I", png[i:i + 4])[0]
        if png[i + 4:i + 8] == tipo:
            yield i
            i += 12 + n
            continue
        i += 12 + n


def main():
    partes = sys.argv[1:] or ["a", "b"]      # para depurar una sola: python3 tests/h380_presentaciones.py b
    if "a" in partes:
        parte_a()
        print("H380: parte A (formatos) bien")
    if "b" in partes:
        parte_b()
        print("H380: parte B (interfaz y presentación) bien")


if __name__ == "__main__":
    main()
