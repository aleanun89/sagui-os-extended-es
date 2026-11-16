#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H334 - Macaco como una hoja de cálculo de verdad (mejora 130, D332).

Criterio:
  - hoja dispersa de 65 536 filas x 256 columnas (A..IV): escribir en IV65536, guardar en .mxs y volver a
    abrir; la memoria de una hoja casi vacía no crece con el tamaño teórico (dos páginas de celdas, unos
    kilobytes frente a 200 MB);
  - celdas combinadas (B2:D4) en el formato propio, en XLSX (mergeCells) y en ODS (number-columns-spanned /
    number-rows-spanned): se exportan, se validan en el anfitrión (ZIP y XML bien formados) y se vuelven a abrir;
  - anchos de columna, alturas y filas/columnas ocultas, y el formato de celda, ida y vuelta por XLSX y ODS;
  - ventana: la combinación se ve (captura), el cuadro de nombre va a IV65536, Ctrl+Fin y Ctrl+flechas saltan
    por la hoja, el doble clic en el borde de una cabecera autoajusta.
"""
import io
import os
import re
import sys
import time
import zipfile
import xml.dom.minidom

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, mtools, SAIDA  # noqa: E402

GRID_X, GRID_Y = 40, 118
COL_W, ROW_H = 72, 20

ENTRADA = """MXS 1
hoja "Hoja1" 1 1
larguras 72,72,72,72,72,72,120
columnaoculta F
fila 3 alt=40 oculta=0
fila 9 alt=0 oculta=1
combinar B2:D4
celda A1 "origen"
celda B2 "combinado"
celda B6 10
celda C6 32
celda E6 =SUM(B6:C6)
celda IV65536 "extremo"
formato A1 nf=0 dec=0 mo=0 al=2 est=1 fam=0 tam=0 bor=0 cor=0 rel=1ffff00
"""


def janela(serial, titulo):
    achada = re.findall(rf'criada por \d+: "{re.escape(titulo)}[^"]*" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)
    m = achada[-1]
    return dict(l=int(m[0]), a=int(m[1]), x=int(m[2]) + 2, y=int(m[3]) + 24)


def volcado(m, ruta):
    """El volcado `macaco -t` de un fichero: las líneas de la serial que escribe el programa."""
    return m.comando("macaco -t " + ruta, 90)


def comprobar_libro(out, etiqueta):
    """Lo que tiene que volver igual tras pasar por un formato."""
    verificar("celula Hoja1!IV65536 = extremo" in out, "%s: IV65536 conserva su texto" % etiqueta)
    verificar("celula Hoja1!A1 = origen" in out, "%s: A1 conserva su texto" % etiqueta)
    verificar("celula Hoja1!B2 = combinado" in out, "%s: B2 conserva su texto" % etiqueta)
    verificar("combinada Hoja1!B2:D4" in out, "%s: sigue combinada B2:D4" % etiqueta)
    verificar(re.search(r"celula Hoja1!E6 = 42 \[=SUM\(B6:C6\)\]", out) is not None, "%s: la fórmula de E6 da 42" % etiqueta)
    verificar("columna Hoja1!G ancho=120 oculta=0" in out, "%s: la columna G mide 120" % etiqueta)
    verificar("columna Hoja1!F ancho=72 oculta=1" in out, "%s: la columna F sigue oculta" % etiqueta)
    verificar("fila Hoja1!3 alto=40 oculta=0" in out, "%s: la fila 3 mide 40" % etiqueta)
    verificar("fila Hoja1!9 alto=20 oculta=1" in out, "%s: la fila 9 sigue oculta" % etiqueta)
    verificar(re.search(r"formato Hoja1!A1 .*al=2 est=1 tam=10 .*rel=1ffff00", out) is not None,
              "%s: A1 sigue centrada, en negrita y con relleno amarillo" % etiqueta)


def principal():
    pen = criar_pendrive("h334-pen", rotulo="MACACO", arquivos={"entrada.mxs": ENTRADA})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    def sacar(nome):
        local = os.path.join(SAIDA, "h334-" + nome)
        mtools(pen, "mcopy", "-o", "p:/" + nome, local)
        verificar(os.path.exists(local), "%s salió del pendrive" % nome)
        with open(local, "rb") as f:
            return f.read()

    with Maquina("h334", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "el pendrive está montado")

        # ---- hoja dispersa: IV65536 y la memoria
        out = volcado(m, "/usb/entrada.mxs")
        verificar("celula Hoja1!IV65536 = extremo" in out, "IV65536 (fila 65536, columna 256) se lee")
        mm = re.search(r"memoria Hoja1 celdas=(\d+) paginas=(\d+) bytes=(\d+) teorico=(\d+)", out)
        verificar(mm is not None, "el volcado informa de la memoria de la hoja")
        celdas, paginas, nbytes, teorico = (int(x) for x in mm.groups())
        print("   memoria: %d celdas en %d páginas = %d bytes (la hoja entera serían %d)" % (celdas, paginas, nbytes, teorico))
        verificar(celdas == 6, "la hoja tiene las 6 celdas con contenido (%d)" % celdas)
        verificar(paginas == 2, "dos páginas de celdas (esquina y extremo): %d" % paginas)
        verificar(nbytes < 64 * 1024, "la hoja casi vacía ocupa menos de 64 KB: %d" % nbytes)
        verificar(teorico >= 150 * 1024 * 1024 and nbytes * 1000 < teorico, "y no el tamaño teórico (%d)" % teorico)
        verificar("combinada Hoja1!B2:D4" in out, "la combinación B2:D4 del .mxs se lee")
        verificar("columna Hoja1!A " not in out and "columna Hoja1!B " not in out, "las columnas con el ancho por omisión no se listan")
        comprobar_libro(out, "mxs")

        # ---- guardar y reabrir (formato propio)
        out = m.comando("macaco --convertir /usb/entrada.mxs /tmp/a.mxs", 90)
        verificar("convertido /usb/entrada.mxs -> /tmp/a.mxs bien" in out, "se guarda el .mxs")
        out = m.comando("cat /tmp/a.mxs")
        verificar('celda IV65536 "extremo"' in out, "el .mxs lleva IV65536")
        verificar("combinar B2:D4" in out, "el .mxs lleva «combinar B2:D4»")
        comprobar_libro(volcado(m, "/tmp/a.mxs"), "mxs guardado")

        # ---- XLSX
        out = m.comando("macaco --convertir /tmp/a.mxs /usb/a.xlsx", 90)
        verificar("convertido /tmp/a.mxs -> /usb/a.xlsx bien" in out, "exporta a XLSX")
        out = m.comando("macaco --convertir /usb/a.xlsx /tmp/b.mxs", 90)
        verificar("convertido /usb/a.xlsx -> /tmp/b.mxs bien" in out, "reabre el XLSX")
        comprobar_libro(volcado(m, "/tmp/b.mxs"), "xlsx")
        mm = re.search(r"memoria Hoja1 celdas=(\d+) paginas=(\d+)", volcado(m, "/usb/a.xlsx"))
        verificar(mm is not None and int(mm.group(2)) == 2, "el XLSX reabierto también es disperso (2 páginas)")

        # ---- ODS
        out = m.comando("macaco --convertir /tmp/a.mxs /usb/a.ods", 90)
        verificar("convertido /tmp/a.mxs -> /usb/a.ods bien" in out, "exporta a ODS")
        out = m.comando("macaco --convertir /usb/a.ods /tmp/c.mxs", 90)
        verificar("convertido /usb/a.ods -> /tmp/c.mxs bien" in out, "reabre el ODS")
        comprobar_libro(volcado(m, "/tmp/c.mxs"), "ods")
        m.comando("sync")

        # ---- los ficheros, validados en el anfitrión
        xlsx = sacar("a.xlsx")
        z = zipfile.ZipFile(io.BytesIO(xlsx))
        verificar(z.testzip() is None, "el XLSX es un ZIP íntegro")
        nombres = z.namelist()
        for n in ("[Content_Types].xml", "_rels/.rels", "xl/workbook.xml", "xl/_rels/workbook.xml.rels", "xl/styles.xml",
                  "xl/worksheets/sheet1.xml"):
            verificar(n in nombres, "el XLSX trae %s" % n)
            xml.dom.minidom.parseString(z.read(n))
        hoja = z.read("xl/worksheets/sheet1.xml").decode("utf-8")
        verificar('<mergeCell ref="B2:D4"/>' in hoja, "XLSX: mergeCells con B2:D4")
        verificar('<mergeCells count="1">' in hoja, "XLSX: un solo rango combinado")
        verificar('r="IV65536"' in hoja, "XLSX: la celda IV65536")
        verificar("<f>SUM(B6:C6)</f>" in hoja, "XLSX: la fórmula de E6")
        verificar('hidden="1"' in hoja, "XLSX: lleva filas y columnas ocultas")
        verificar('ht="30.00" customHeight="1"' in hoja, "XLSX: la fila 3 mide 30 pt")

        ods = sacar("a.ods")
        z = zipfile.ZipFile(io.BytesIO(ods))
        verificar(z.testzip() is None, "el ODS es un ZIP íntegro")
        info = z.infolist()
        verificar(info[0].filename == "mimetype" and info[0].compress_type == 0, "ODS: «mimetype» primero y sin comprimir")
        verificar(z.read("mimetype") == b"application/vnd.oasis.opendocument.spreadsheet", "ODS: tipo MIME")
        contenido = z.read("content.xml").decode("utf-8")
        xml.dom.minidom.parseString(contenido)
        xml.dom.minidom.parseString(z.read("META-INF/manifest.xml"))
        verificar('table:number-columns-spanned="3"' in contenido and 'table:number-rows-spanned="3"' in contenido,
                  "ODS: number-columns-spanned y number-rows-spanned = 3")
        verificar("<table:covered-table-cell" in contenido, "ODS: las celdas cubiertas")
        verificar("of:=SUM([.B6:.C6])" in contenido, "ODS: la fórmula en OpenFormula")
        verificar('table:visibility="collapse"' in contenido, "ODS: lleva filas y columnas ocultas")

        # ---- la ventana: la combinación, el cuadro de nombre, Ctrl+Fin y las flechas con Ctrl
        m.digitar("macaco --clasico /usb/entrada.mxs\n", 0.05)
        m.esperar_serial("macaco: pronto", 30)
        time.sleep(1.5)
        a = janela(m.serial(), "Macaco")
        m.captura("H334-1-combinada")
        # cuadro de nombre
        m.clique(a["x"] + 40, a["y"] + 76 + 12)
        m.digitar("IV65536", 0.06)
        m.tecla("ret", 0.4)
        m.esperar_serial("macaco: ir a IV65536", 15)
        time.sleep(0.5)
        m.captura("H334-2-iv65536")
        m.tecla("ctrl-up", 0.3)
        m.esperar_serial("macaco: salto a IV1", 15)
        m.tecla("ctrl-home", 0.3)
        time.sleep(0.3)
        m.tecla("ctrl-end", 0.3)
        m.esperar_serial("macaco: salto a IV65536", 15)
        verificar(True, "Ctrl+Fin va a la última celda con datos (IV65536)")
        m.tecla("ctrl-home", 0.3)
        m.tecla("ctrl-down", 0.3)
        m.esperar_serial("macaco: salto a A65536", 15)
        m.tecla("ctrl-home", 0.3)
        time.sleep(0.4)
        # autoajuste de la columna A («origen»: 6 letras de 8 px + 8 de relleno = 57)
        m.clique(a["x"] + GRID_X + COL_W, a["y"] + GRID_Y - 9, duplo=True)
        m.esperar_serial("macaco: autoajuste de la columna 1 = 57", 15)
        time.sleep(0.4)
        m.captura("H334-3-autoajuste")
        # la fila 1 vuelve a su altura
        m.clique(a["x"] + 20, a["y"] + GRID_Y + ROW_H, duplo=True)
        m.esperar_serial("macaco: autoajuste de la fila 1 = 20", 15)

        # ---- combinar y separar con el menú Formato (los elementos se miden en H334-4)
        def ir(ref):
            m.clique(a["x"] + 40, a["y"] + 76 + 12)
            m.digitar(ref, 0.06)
            m.tecla("ret", 0.4)
            m.esperar_serial("macaco: ir a " + ref.split(":")[0], 15)

        def formato(desplazamiento):
            m.clique(a["x"] + 270, a["y"] + 10)
            time.sleep(0.5)
            m.clique(a["x"] + 300, a["y"] + desplazamiento)
            time.sleep(0.4)

        ir("B6:C6")
        formato(280)                                   # Combinar celdas
        m.esperar_serial("macaco: combinadas B6:C6 (1)", 15)
        m.captura("H334-4-combinar-menu")
        formato(258)                                   # Combinar y centrar sobre una combinación: separa
        m.esperar_serial("macaco: separadas las celdas 6,2", 15)
        ir("G1")
        formato(474)                                   # Ocultar columnas
        m.esperar_serial("macaco: ocultadas columnas 7-7", 15)
        formato(496)                                   # Mostrar columnas
        m.esperar_serial("macaco: mostradas columnas 8-8", 15)
        ir("E5")
        formato(527)                                   # Inmovilizar paneles
        m.esperar_serial("macaco: paneles inmovilizados filas=4 columnas=4", 15)
        ir("A40")
        time.sleep(0.4)
        m.captura("H334-5-paneles-inmovilizados")
        formato(527)                                   # liberar
        m.esperar_serial("macaco: paneles liberados", 15)
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
    return 0


if __name__ == "__main__":
    sys.exit(principal())
