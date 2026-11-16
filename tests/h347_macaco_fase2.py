#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H347 - Macaco, fase 2 (mejora 130, D345): barras de desplazamiento, buscar y reemplazar, ajustar texto, brocha,
autofiltro, formato condicional, comentarios y validación de datos.

Criterio:
  - formato propio: cada función se guarda y se vuelve a leer (volcado `macaco -t`), y pasa por XLSX y ODS sin romper
    nada (el ajuste de texto sobrevive a XLSX como wrapText; lo demás se pierde en ese formato pero no lo rompe);
  - ventana (modo clásico, que fija las posiciones): las barras de desplazamiento (flechas, clic en el cauce y arrastre
    de la guía), el comentario se ve al pasar el ratón, la lista de validación y su rechazo, buscar y reemplazar, la brocha
    (Ctrl+Mayús+C / V), el autofiltro (Ctrl+Mayús+L) y el color del formato condicional en pantalla;
  - ventana con la cinta: los botones nuevos están en Inicio, Insertar y Datos y sus órdenes llegan (ajustar texto,
    formato condicional, validación, autofiltro).
"""
import io
import os
import re
import sys
import time
import zipfile
import xml.dom.minidom

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, mtools, imagem_de_teste, cor_do_pixel, SAIDA  # noqa: E402
from h337_cinta import bloque, clic_ctrl, clic_tab, esperar, esperar_orden  # noqa: E402

GRID_X, GRID_Y = 40, 118
COL_W, ROW_H = 72, 20
RODAPE = 44
SB = 16

ENTRADA = """MXS 1
hoja "Hoja1" 8 4
larguras 72,72,72,72
fila 3 alt=0 oculta=2
fila 4 alt=0 oculta=2
fila 5 alt=0 oculta=2
celda A1 "Nombre"
celda B1 "Nota"
celda A2 "Ana"
celda B2 8
celda A3 "Luis"
celda B3 3
celda A4 "Eva"
celda B4 5
celda A5 "texto largo partido"
celda C2 "si"
celda D2 500
formato A5 nf=0 dec=0 mo=0 al=0 est=0 fam=0 tam=0 bor=0 cor=0 rel=0 aj=1
comentario B2 "revisar la nota"
validacion C2:C5 tipo=1 min=0 max=0 lista="si;no;quizas"
validacion D2:D5 tipo=2 min=10000 max=100000 lista=""
condicional B2:B5 op=1 v1=700 v2=0 fondo=1ffc0c0 cor=1800000 txt=""
autofiltro A1:B5
filtro A "Ana"
"""

# Para la ventana: sin filtro (lo activa la prueba), con las reglas de formato condicional y de validación ya puestas.
UI = """MXS 1
hoja "Hoja1" 14 4
larguras 72,72,72,72
fila 7 alt=52 oculta=0
celda A1 "Nombre"
celda B1 "Nota"
celda A2 "Ana"
celda B2 8
celda A3 "Luis"
celda B3 3
celda A4 "Eva"
celda B4 5
celda A5 "Pepe"
celda B5 9
celda A7 "texto largo partido"
celda C2 "si"
formato A7 nf=0 dec=0 mo=0 al=0 est=0 fam=0 tam=0 bor=0 cor=0 rel=0 aj=1
validacion C2:C5 tipo=1 min=0 max=0 lista="si;no;quizas"
validacion D2:D5 tipo=2 min=10000 max=100000 lista=""
condicional B2:B5 op=1 v1=700 v2=0 fondo=1ffc0c0 cor=1800000 txt=""
"""

# Para la cinta: sin reglas, para crearlas con los botones.
UI2 = """MXS 1
hoja "Hoja1" 14 4
larguras 72,72,72,72
celda A1 "Nombre"
celda B1 "Nota"
celda A2 "Ana"
celda B2 8
celda A3 "Luis"
celda B3 3
celda A4 "Eva"
celda B4 5
celda A5 "Pepe"
celda B5 9
celda A7 "texto largo partido"
"""


def janela(serial, titulo):
    achada = re.findall(rf'criada por \d+: "{re.escape(titulo)}[^"]*" (\d+)x(\d+) em \((-?\d+),(-?\d+)\)', serial)
    m = achada[-1]
    return dict(l=int(m[0]), a=int(m[1]), x=int(m[2]) + 2, y=int(m[3]) + 24)


def volcado(m, ruta):
    return m.comando("macaco -t " + ruta, 90)


def serial_desde(m, n):
    return m.serial()[n:]


def esperar_log(m, patron, tempo=15, desde=0):
    return esperar(m, patron, tempo, desde)


def comprobar_f2(out, etiqueta, hidden_ok=(2,)):
    verificar("comentario Hoja1!B2 = revisar la nota" in out, "%s: comentario de B2" % etiqueta)
    verificar("validacion Hoja1!C2:C5 tipo=1 min=0 max=0 lista=[si;no;quizas]" in out, "%s: validación de lista C2:C5" % etiqueta)
    verificar("validacion Hoja1!D2:D5 tipo=2 min=10000 max=100000 lista=[]" in out, "%s: validación numérica D2:D5" % etiqueta)
    verificar(re.search(r"condicional Hoja1!B2:B5 >7 fondo=1ffc0c0 cor=1800000", out) is not None, "%s: formato condicional >7" % etiqueta)
    verificar("autofiltro Hoja1!A1:B5" in out, "%s: autofiltro en A1:B5" % etiqueta)
    verificar("filtro Hoja1!A = Ana" in out, "%s: filtro de la columna A = Ana" % etiqueta)
    for fila in (3, 4, 5):
        verificar(re.search(r"fila Hoja1!%d alto=20 oculta=2" % fila, out) is not None, "%s: la fila %d sigue oculta por el filtro" % (etiqueta, fila))
    verificar(re.search(r"formato Hoja1!A5 .* aj=1 ", out) is not None, "%s: A5 conserva el ajuste de texto" % etiqueta)


def fase_consola(m):
    out = volcado(m, "/usb/entrada.mxs")
    comprobar_f2(out, "lectura")
    out = m.comando("macaco --convertir /usb/entrada.mxs /tmp/a.mxs", 90)
    verificar("convertido /usb/entrada.mxs -> /tmp/a.mxs bien" in out, "se guarda el .mxs con la fase 2")
    out = re.sub(r"\[SAGUI\][^\n]*\n", "", m.comando("cat /tmp/a.mxs"))   # el aviso del kernel se cuela entre las líneas
    for linea in ('comentario B2 "revisar la nota"', 'validacion C2:C5 tipo=1 min=0 max=0 lista="si;no;quizas"',
                  'condicional B2:B5 op=1 v1=700 v2=0 fondo=1ffc0c0 cor=1800000 txt=""', "autofiltro A1:B5",
                  'filtro A "Ana"', "fila 3 alt=0 oculta=2", " aj=1"):
        verificar(linea in out, "el .mxs guardado lleva «%s»" % linea.strip())
    comprobar_f2(volcado(m, "/tmp/a.mxs"), "mxs guardado")

    # XLSX y ODS: no se rompe nada, el ajuste de texto sobrevive a XLSX
    out = m.comando("macaco --convertir /tmp/a.mxs /usb/a.xlsx", 90)
    verificar("convertido /tmp/a.mxs -> /usb/a.xlsx bien" in out, "exporta a XLSX con comentarios, filtro y reglas")
    out = m.comando("macaco --convertir /usb/a.xlsx /tmp/b.mxs", 90)
    verificar("convertido /usb/a.xlsx -> /tmp/b.mxs bien" in out, "reabre el XLSX")
    b = volcado(m, "/tmp/b.mxs")
    verificar("celula Hoja1!A2 = Ana" in b and "celula Hoja1!B2 = 8" in b, "XLSX: los datos vuelven")
    verificar(re.search(r"formato Hoja1!A5 .* aj=1 ", b) is not None, "XLSX: el ajuste de texto vuelve (wrapText)")
    verificar(re.search(r"fila Hoja1!3 alto=20 oculta=[12]", b) is not None, "XLSX: las filas filtradas siguen ocultas")
    out = m.comando("macaco --convertir /tmp/a.mxs /usb/a.ods", 90)
    verificar("convertido /tmp/a.mxs -> /usb/a.ods bien" in out, "exporta a ODS")
    out = m.comando("macaco --convertir /usb/a.ods /tmp/c.mxs", 90)
    verificar("convertido /usb/a.ods -> /tmp/c.mxs bien" in out, "reabre el ODS")
    c = volcado(m, "/tmp/c.mxs")
    verificar("celula Hoja1!A2 = Ana" in c and "celula Hoja1!B2 = 8" in c, "ODS: los datos vuelven")
    m.comando("sync")


def validar_xlsx(sacar):
    z = zipfile.ZipFile(io.BytesIO(sacar("a.xlsx")))
    verificar(z.testzip() is None, "el XLSX es un ZIP íntegro")
    for n in ("xl/workbook.xml", "xl/styles.xml", "xl/worksheets/sheet1.xml"):
        xml.dom.minidom.parseString(z.read(n))
    verificar(b'wrapText="1"' in z.read("xl/styles.xml"), "XLSX: wrapText en los estilos")
    z = zipfile.ZipFile(io.BytesIO(sacar("a.ods")))
    verificar(z.testzip() is None, "el ODS es un ZIP íntegro")
    xml.dom.minidom.parseString(z.read("content.xml"))


def fase_ventana_clasica(m):
    n0 = len(m.serial())
    m.digitar("macaco --clasico /usb/ui.mxs\n", 0.05)
    esperar_log(m, r"macaco: pronto", 40, n0)
    time.sleep(1.5)
    a = janela(m.serial()[n0:], "Macaco")
    ga = a["a"] - GRID_Y - RODAPE - SB                      # alto de la cuadrícula
    vx = a["x"] + a["l"] - SB // 2                          # centro de la barra vertical
    vy_abajo = a["y"] + GRID_Y + ga - SB // 2               # flecha de abajo
    hy = a["y"] + GRID_Y + ga + SB // 2                     # centro de la barra horizontal
    hx_der = a["x"] + a["l"] - SB - SB // 2                 # flecha de la derecha
    hx_izq = a["x"] + GRID_X + SB // 2                      # flecha de la izquierda

    def ir(ref):
        m.clique(a["x"] + 40, a["y"] + 76 + 12)
        m.digitar(ref, 0.06)
        m.tecla("ret", 0.4)
        esperar_log(m, r"macaco: ir a " + ref.split(":")[0], 15, len(m.serial()) - 400)

    def ultimo(patron, desde):
        r = re.findall(patron, m.serial()[desde:])
        return r[-1] if r else None

    # ---- el formato condicional se ve: B2 (8) y B5 (9) en rosa, B3 (3) en blanco
    time.sleep(0.5)
    png = m.captura("H347-1-barras")
    rosa = cor_do_pixel(png, a["x"] + GRID_X + COL_W + 4, a["y"] + GRID_Y + ROW_H + 2)
    blanco = cor_do_pixel(png, a["x"] + GRID_X + COL_W + 4, a["y"] + GRID_Y + 2 * ROW_H + 2)
    verificar(abs(rosa[0] - 255) < 8 and abs(rosa[1] - 192) < 10 and abs(rosa[2] - 192) < 10, "formato condicional: B2 (8) en rosa %s" % (rosa,))
    verificar(blanco == (255, 255, 255), "formato condicional: B3 (3) sin color %s" % (blanco,))

    # ---- barras de desplazamiento
    k = len(m.serial())
    m.clique(vx, vy_abajo)
    m.clique(vx, vy_abajo)
    esperar_log(m, r"barra vertical: primera fila 3", 10, k)
    verificar(True, "la flecha de abajo de la barra vertical baja una fila cada vez (fila 3)")
    k = len(m.serial())
    m.clique(vx, vy_abajo - 12)                              # el cauce bajo la guía: una página
    esperar_log(m, r"barra vertical: primera fila \d+", 10, k)
    f = int(ultimo(r"barra vertical: primera fila (\d+)", k))
    verificar(f > 10, "un clic en el cauce salta una página (fila %d)" % f)
    m.tecla("ctrl-home", 0.4)
    time.sleep(0.5)
    k = len(m.serial())
    m.arrastar(vx, a["y"] + GRID_Y + SB + 10, vx, a["y"] + GRID_Y + SB + 90)
    esperar_log(m, r"barra vertical: primera fila \d+", 10, k)
    f = int(ultimo(r"barra vertical: primera fila (\d+)", k))
    verificar(f >= 5, "arrastrar la guía mueve la hoja (fila %d)" % f)
    m.captura("H347-1b-barra-arrastrada")
    m.tecla("ctrl-home", 0.4)
    k = len(m.serial())
    m.clique(hx_der, hy)
    esperar_log(m, r"barra horizontal: primera columna B", 10, k)
    verificar(True, "la flecha derecha de la barra horizontal avanza una columna (B)")
    k = len(m.serial())
    m.clique(hx_izq, hy)
    esperar_log(m, r"barra horizontal: primera columna A", 10, k)
    verificar(True, "la flecha izquierda vuelve a la columna A")

    # ---- comentario: se crea con Mayús+F2 y se ve al pasar el ratón
    ir("B2")
    k = len(m.serial())
    m.tecla("shift-f2", 0.8)
    time.sleep(0.8)
    m.digitar("revisar", 0.06)
    m.tecla("ret", 0.6)
    esperar_log(m, r"macaco: comentario B2 = revisar", 10, k)
    verificar(True, "Mayús+F2 crea el comentario de B2")
    k = len(m.serial())
    m.mouse_para(a["x"] + GRID_X + COL_W + 30, a["y"] + GRID_Y + ROW_H + 10)
    esperar_log(m, r"macaco: comentario visible en B2: revisar", 10, k)
    time.sleep(0.5)
    png = m.captura("H347-2-comentario")
    amarillo = cor_do_pixel(png, a["x"] + GRID_X + COL_W + 30 + 20, a["y"] + GRID_Y + ROW_H + 10 + 20)
    verificar(abs(amarillo[0] - 255) < 4 and abs(amarillo[1] - 255) < 4 and abs(amarillo[2] - 225) < 8, "el comentario se dibuja en un recuadro amarillo %s" % (amarillo,))
    m.mouse_para(a["x"] + 400, a["y"] + 300)

    # ---- validación: lista desplegable, rechazo y rango numérico
    ir("C2")
    time.sleep(0.4)
    m.captura("H347-3-validacion")
    k = len(m.serial())
    m.clique(a["x"] + GRID_X + 2 * COL_W + COL_W - 9, a["y"] + GRID_Y + ROW_H + 10)
    time.sleep(0.8)
    for _ in range(2):
        m.tecla("down", 0.2)
    m.tecla("ret", 0.6)
    esperar_log(m, r"macaco: lista de validación: C2 = no", 10, k)
    verificar(True, "la flecha de la celda abre la lista y elige «no»")
    ir("C3")
    k = len(m.serial())
    m.digitar("zzz", 0.06)
    m.tecla("ret", 0.5)
    esperar_log(m, r"macaco: validación rechazó C3: El valor no está en la lista", 10, k)
    time.sleep(0.8)
    m.tecla("ret", 0.6)
    verificar(True, "un valor fuera de la lista se rechaza")
    ir("D2")
    k = len(m.serial())
    m.digitar("50", 0.06)
    m.tecla("ret", 0.5)
    esperar_log(m, r"macaco: validación rechazó D2: El número debe estar entre 100 y 1000", 10, k)
    time.sleep(0.8)
    m.tecla("ret", 0.6)
    m.tecla("esc", 0.4)
    verificar(True, "un número fuera del rango 100..1000 se rechaza")
    ir("D2")
    k = len(m.serial())
    m.digitar("150", 0.06)
    m.tecla("ret", 0.5)
    esperar_log(m, r"macaco: celula D2 = 150", 10, k)
    verificar(True, "un número dentro del rango se acepta")

    # ---- buscar y reemplazar
    ir("A1")
    k = len(m.serial())
    m.tecla("ctrl-f", 0.9)
    time.sleep(0.6)
    m.digitar("Pepe", 0.06)
    m.tecla("ret", 0.6)
    esperar_log(m, r"macaco: encontrado .Pepe. en A5", 10, k)
    verificar(True, "Ctrl+F busca «Pepe» y va a A5")
    k = len(m.serial())
    m.tecla("ctrl-h", 0.9)
    time.sleep(0.6)
    m.tecla("tab", 0.2)
    m.digitar("Pablo", 0.06)
    m.tecla("ret", 0.6)
    esperar_log(m, r"macaco: reemplazado en A5", 10, k)
    verificar(True, "Ctrl+H reemplaza «Pepe» por «Pablo» en A5")

    # ---- brocha: negrita en A1, copiar el formato y pegarlo en B1
    ir("A1")
    m.tecla("ctrl-b", 0.4)
    k = len(m.serial())
    m.tecla("ctrl-shift-c", 0.6)
    esperar_log(m, r"macaco: brocha: formato copiado de A1", 10, k)
    ir("B1")
    k = len(m.serial())
    m.tecla("ctrl-shift-v", 0.6)
    esperar_log(m, r"macaco: brocha: formato aplicado a B1", 10, k)
    verificar(True, "la brocha copia el formato de A1 y lo pega en B1")

    # ---- autofiltro
    ir("A1")
    k = len(m.serial())
    m.tecla("ctrl-shift-l", 0.8)
    esperar_log(m, r"macaco: autofiltro activado en A1:D1", 10, k)
    time.sleep(0.4)
    m.captura("H347-4-autofiltro")
    k = len(m.serial())
    m.clique(a["x"] + GRID_X + COL_W - 9, a["y"] + GRID_Y + 10)
    time.sleep(0.8)
    for _ in range(3):                                      # (Todos), Ana, Eva
        m.tecla("down", 0.2)
    m.tecla("ret", 0.6)
    esperar_log(m, r"macaco: filtro columna A = Eva: 3 filas ocultas", 10, k)
    time.sleep(0.4)
    m.captura("H347-5-filtrado")
    verificar(True, "la flecha del encabezado filtra por «Eva» (3 filas ocultas)")

    # ---- se guarda todo y la ventana se cierra
    k = len(m.serial())
    m.tecla("ctrl-s", 0.8)
    esperar_log(m, r"macaco: guardado /usb/ui.mxs", 15, k)
    m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
    time.sleep(1.5)
    m.captura("H347-9-cierre", guardar=False)
    m.clique(560, 300)                                    # foco en la terminal
    time.sleep(1.0)
    out = volcado(m, "/usb/ui.mxs")
    verificar("comentario Hoja1!B2 = revisar" in out, "tras guardar: el comentario nuevo")
    verificar("celula Hoja1!A5 = Pablo" in out and "celula Hoja1!C2 = no" in out and "celula Hoja1!D2 = 150" in out, "tras guardar: los cambios de buscar/validar")
    verificar("autofiltro Hoja1!A1:D5" in out and "filtro Hoja1!A = Eva" in out, "tras guardar: el autofiltro con su filtro")
    verificar(re.search(r"fila Hoja1!2 alto=20 oculta=2", out) is not None, "tras guardar: las filas filtradas")
    verificar(re.search(r"formato Hoja1!B1 .* est=1 ", out) is not None, "tras guardar: B1 en negrita por la brocha")
    verificar(re.search(r"condicional Hoja1!B2:B5 >7 ", out) is not None and "validacion Hoja1!C2:C5" in out, "tras guardar: las reglas")
    verificar(re.search(r"fila Hoja1!7 alto=52 oculta=0", out) is not None and re.search(r"formato Hoja1!A7 .* aj=1 ", out) is not None,
              "tras guardar: la fila 7 con el texto ajustado")


def fase_cinta(m):
    n0 = len(m.serial())
    m.comando("macaco --cinta-log /usb/ui2.mxs &", 20)
    esperar(m, r"macaco: pronto", 40, n0)
    time.sleep(2.5)
    g = re.findall(r'criada por \d+: "Macaco - Libro1" (\d+)x(\d+) em \((-?\d+),(-?\d+)\)', m.serial()[n0:])
    verificar(bool(g), "ventana de Macaco con la cinta")
    l, an, x, y = (int(v) for v in g[-1])
    w = dict(l=l, a=an, x=x + 2, y=y + 24)
    b = bloque(m, n0)
    for nombre in ("Ajustar texto", "Copiar formato", "Buscar"):
        verificar(nombre in b["ctrl"], "Inicio: el botón «%s» está en la cinta" % nombre)
    m.captura("H347-6-cinta-inicio")

    def ir(ref):
        m.tecla("ctrl-g", 0.8)
        time.sleep(0.5)
        m.digitar(ref, 0.06)
        m.tecla("ret", 0.6)
        time.sleep(0.4)

    # ---- Ajustar texto (Inicio)
    ir("A7")
    k = len(m.serial())
    clic_ctrl(m, w, b, "Ajustar texto")
    esperar_orden(m, "Ajustar texto", k)
    esperar(m, r"macaco: ajustar texto activado", 8, k)
    verificar(True, "el botón «Ajustar texto» da su orden")
    # ---- pestaña Datos
    clic_tab(m, w, b, "Datos")
    time.sleep(0.8)
    b2 = bloque(m, n0)
    verificar(b2["pestana"] == "Datos", "pestaña Datos")
    for nombre in ("Autofiltro", "Quitar filtros", "Validación de datos", "Formato condicional", "Borrar reglas"):
        verificar(nombre in b2["ctrl"], "Datos: el botón «%s» está en la cinta" % nombre)
    m.captura("H347-7-cinta-datos")
    # formato condicional sobre B2:B5
    ir("B2:B5")
    k = len(m.serial())
    clic_ctrl(m, w, b2, "Formato condicional", dy=20)
    esperar_orden(m, "Formato condicional", k)
    time.sleep(1.2)
    m.tecla("backspace", 0.15)
    m.tecla("backspace", 0.15)
    m.digitar(">7", 0.08)
    m.tecla("ret", 0.8)
    esperar(m, r"macaco: formato condicional en B2:B5: >7 fondo=1ffc0c0 cor=1800000", 10, k)
    verificar(True, "el botón «Formato condicional» crea la regla >7")
    # validación en C2:C5
    ir("C2:C5")
    k = len(m.serial())
    clic_ctrl(m, w, b2, "Validación de datos", dy=20)
    esperar_orden(m, "Validación de datos", k)
    time.sleep(1.2)
    m.digitar("x;y;z", 0.08)
    m.tecla("ret", 0.8)
    esperar(m, r"macaco: validación en C2:C5: x;y;z", 10, k)
    verificar(True, "el botón «Validación de datos» crea la lista x;y;z")
    # autofiltro
    ir("A1")
    k = len(m.serial())
    clic_ctrl(m, w, b2, "Autofiltro", dy=20)
    esperar(m, r"macaco: autofiltro activado en A1:B1", 8, k)
    verificar(True, "el botón «Autofiltro» activa los filtros")
    time.sleep(0.5)
    m.captura("H347-8-cinta-filtro")
    k = len(m.serial())
    clic_ctrl(m, w, b2, "Quitar filtros")
    esperar(m, r"macaco: filtros quitados", 8, k)
    verificar(True, "el botón «Quitar filtros» responde")
    k = len(m.serial())
    clic_ctrl(m, w, b2, "Autofiltro", dy=20)
    esperar(m, r"macaco: autofiltro quitado", 8, k)
    verificar(True, "el botón «Autofiltro» lo quita")
    # pestaña Insertar: comentario
    clic_tab(m, w, b, "Insertar")
    time.sleep(0.8)
    b3 = bloque(m, n0)
    verificar("Comentario" in b3["ctrl"] and "Eliminar comentario" in b3["ctrl"], "Insertar: los botones de comentario")
    ir("B3")
    k = len(m.serial())
    clic_ctrl(m, w, b3, "Comentario", dy=20)
    esperar_orden(m, "Comentario", k)
    time.sleep(1.2)
    m.digitar("desde la cinta", 0.06)
    m.tecla("ret", 0.8)
    esperar(m, r"macaco: comentario B3 = desde la cinta", 10, k)
    verificar(True, "el botón «Comentario» crea el comentario")
    k = len(m.serial())
    m.tecla("ctrl-s", 0.8)
    esperar(m, r"macaco: guardado /usb/ui2.mxs", 15, k)
    m.clique(w["x"] + w["l"] - 12, w["y"] - 12)
    time.sleep(1.5)
    m.clique(560, 300)
    time.sleep(1.0)
    out = volcado(m, "/usb/ui2.mxs")
    verificar(re.search(r"fila Hoja1!7 alto=52 oculta=0", out) is not None and re.search(r"formato Hoja1!A7 .* aj=1 ", out) is not None,
              "tras guardar: el ajuste de texto de A7 sube la fila a 52 px")
    verificar("comentario Hoja1!B3 = desde la cinta" in out, "tras guardar: el comentario de la cinta")
    verificar(re.search(r"condicional Hoja1!B2:B5 >7 ", out) is not None and "validacion Hoja1!C2:C5 tipo=1" in out and "lista=[x;y;z]" in out,
              "tras guardar: las reglas creadas con la cinta")


def principal():
    pen = criar_pendrive("h347-pen", rotulo="MACACO", arquivos={"entrada.mxs": ENTRADA, "ui.mxs": UI, "ui2.mxs": UI2})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    def sacar(nome):
        local = os.path.join(SAIDA, "h347-" + nome)
        mtools(pen, "mcopy", "-o", "p:/" + nome, local)
        with open(local, "rb") as f:
            return f.read()

    img = imagem_de_teste("", ["--vesa", "--resolucao", "1024x768"])
    with Maquina("h347", "ide", imagem=img, extras=extras, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(3)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "el pendrive está montado")
        if "--solo-ventana" not in sys.argv:
            fase_consola(m)
            validar_xlsx(sacar)
        fase_ventana_clasica(m)
        fase_cinta(m)
    print("H347 OK")
    return 0


if __name__ == "__main__":
    sys.exit(principal())
