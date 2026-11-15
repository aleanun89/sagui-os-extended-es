#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H274 - la interfaz completa del visor de PDF (v8.1, mejora 105, D274).

Comprueba, con un PDF de seis páginas escrito a mano (tests/pdf_exemplos.py,
con índice anidado y destinos de los tres tipos), con uno cifrado y con PDF
reales (D205):

  - modos sin ventana: `visorpdf --indice` (marcadores y a qué página van) y
    `visorpdf --buscar` (coincidencias por página, sin distinguir mayúsculas);
  - en la ventana: la barra de herramientas y el panel lateral, navegación
    (teclas, Ctrl+G, clic en el botón siguiente), zoom, ajuste al ancho y a la
    página, giro, miniaturas en segundo plano, índice (clic en un marcador),
    búsqueda Ctrl+F con resaltado (se miran los píxeles amarillos/naranjas),
    presentación (F5), exportar la página a una imagen, diálogo de contraseña
    (mala y buena) y recordar página y zoom por documento.

Las comprobaciones salen de las líneas que el visor escribe en la serie
(`visorpdf: estado pagina=N/M zoom=Z rot=R`, `busca '...': N resultados`,
`miniatura N`, `indice N`...) y de las capturas H274-*.png.
"""
import os
import re
import struct
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, criar_pendrive, pendrive_qemu,  # noqa: E402
                  SAIDA)
import pdf_exemplos  # noqa: E402
import pdf_reales_comum as comum  # noqa: E402


def esperar_desde(m, texto, desde, tempo=60):
    """Espera a que 'texto' salga en la serie después de la posición 'desde'."""
    fim = time.time() + tempo
    while time.time() < fim:
        s = m.serial()[desde:]
        if texto in s:
            return s
        time.sleep(0.2)
    raise AssertionError("tiempo agotado esperando %r" % texto)


def estado(m, desde):
    """El último 'estado' de la serie después de 'desde': (pagina, total, zoom, rot)."""
    todos = re.findall(r"visorpdf: estado pagina=(\d+)/(\d+) zoom=(\d+) rot=(\d+)", m.serial()[desde:])
    return tuple(int(x) for x in todos[-1]) if todos else None


def pixeles(png):
    """La captura PNG (RGB o RGBA de 8 bits) como (ancho, alto, bytes RGB)."""
    with open(png, "rb") as f:
        dados = f.read()
    pos, idat, larg, alt, tipo = 8, b"", 0, 0, 2
    while pos < len(dados):
        n = struct.unpack(">I", dados[pos:pos + 4])[0]
        nome = dados[pos + 4:pos + 8]
        corpo = dados[pos + 8:pos + 8 + n]
        if nome == b"IHDR":
            larg, alt = struct.unpack(">II", corpo[:8])
            tipo = corpo[9]
        elif nome == b"IDAT":
            idat += corpo
        pos += 12 + n
    bpp = 4 if tipo == 6 else 3
    cru = zlib.decompress(idat)
    stride = larg * bpp
    saida = bytearray()
    ant = bytearray(stride)
    for y in range(alt):
        filtro = cru[y * (stride + 1)]
        lin = bytearray(cru[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = lin[i - bpp] if i >= bpp else 0
            b = ant[i]
            c = ant[i - bpp] if i >= bpp else 0
            if filtro == 1:
                lin[i] = (lin[i] + a) & 255
            elif filtro == 2:
                lin[i] = (lin[i] + b) & 255
            elif filtro == 3:
                lin[i] = (lin[i] + (a + b) // 2) & 255
            elif filtro == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                lin[i] = (lin[i] + pr) & 255
        ant = lin
        if bpp == 4:
            for x in range(larg):
                saida += lin[x * 4:x * 4 + 3]
        else:
            saida += lin
    return larg, alt, bytes(saida)


def contar(img, x0, y0, x1, y1, prueba):
    larg, alt, px = img
    n = 0
    for y in range(max(0, y0), min(alt, y1)):
        for x in range(max(0, x0), min(larg, x1)):
            i = (y * larg + x) * 3
            if prueba(px[i], px[i + 1], px[i + 2]):
                n += 1
    return n


def origen_ventana(m, desde=0):
    """El origen del contenido de la ventana principal del visor (900x700)."""
    for g in re.finditer(r'criada por \d+: "Visor de PDF" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial()[desde:]):
        if int(g.group(1)) >= 600:
            return int(g.group(3)) + 2, int(g.group(4)) + 24
    raise AssertionError("no se encontró la ventana del visor")


def limpiar(m, n):
    """Borra el texto de la caja de un diálogo (el cursor queda al final)."""
    for _ in range(n):
        m.tecla("backspace", 0.03)


def cerrar(m):
    pos = len(m.serial())
    m.monitor("sendkey alt-f4")
    esperar_desde(m, "visorpdf: fechado", pos, 30)
    m.esperar_serial("sagui:/$", 30)
    time.sleep(0.5)


def lanzar(m, comando):
    pos = len(m.serial())
    m.digitar(comando, 0.04)
    m.tecla("ret", 0.04)
    return pos


def main():
    destino = os.path.join(SAIDA, "h274-pdf")
    os.makedirs(destino, exist_ok=True)
    arquivos = {
        "indice.pdf": pdf_exemplos.com_indice(),
        "cifrado.pdf": pdf_exemplos.cifrado("sagui"),
    }
    for nome in ("enumitem.pdf", "rfc9116.pdf"):
        with open(os.path.join(comum.RECURSOS, nome), "rb") as f:
            arquivos[nome] = f.read()
    pen = criar_pendrive("h274-pen", rotulo="PDFGUI", arquivos=arquivos)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    with Maquina("h274", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "el pendrive con los PDF está montado")
        m.comando("mkdir /home")   # D278: el diálogo de guardar no crea carpetas; la exportación de antes sí

        # ---------------------------------------------------- modos sin ventana
        out = m.comando("visorpdf --indice /usb/indice.pdf")
        verificar("indice: 4" in out, "el índice tiene 4 marcadores")
        verificar("marca 1 nivel 0 pagina 1: Capitulo 1" in out, "Capitulo 1 va a la página 1 (destino directo)")
        verificar("marca 2 nivel 1 pagina 2: Seccion 1.1" in out, "Seccion 1.1 es hija y va a la página 2")
        verificar("marca 3 nivel 0 pagina 3: Capitulo 2" in out, "Capitulo 2 va a la página 3 (destino con nombre)")
        verificar("marca 4 nivel 0 pagina 4: Cap" in out, "Capítulo 3 va a la página 4 (acción GoTo)")

        out = m.comando("visorpdf --buscar /usb/indice.pdf zorro")
        verificar("pagina 3: 2" in out and "pagina 5: 1" in out and "total: 3" in out,
                  "«zorro»: 2 en la página 3 y 1 en la 5")
        out = m.comando("visorpdf --buscar /usb/indice.pdf ATENCION")
        verificar("pagina 4: 1" in out, "la búsqueda no distingue mayúsculas")
        out = m.comando("visorpdf --buscar /usb/indice.pdf 'capitulo uno'")
        verificar("pagina 1: 1" in out, "una frase con espacios se encuentra")
        out = m.comando("visorpdf --buscar /usb/indice.pdf inexistente")
        verificar("total: 0" in out, "sin coincidencias: total 0")

        for nome in ("enumitem.pdf", "rfc9116.pdf"):
            out = m.comando("visorpdf --indice /usb/%s" % nome, 120)
            verificar("indice:" in out, "%s: el índice se lee sin fallar" % nome)
            print("   ", nome, re.search(r"indice: \d+", out).group(0))
        out = m.comando("visorpdf --buscar /usb/enumitem.pdf enumitem", 180)
        total = int(re.search(r"total: (\d+)", out).group(1))
        verificar(total > 20, "«enumitem» sale muchas veces en su manual (%d)" % total)
        out = m.comando("visorpdf --buscar /usb/rfc9116.pdf security", 180)
        total = int(re.search(r"total: (\d+)", out).group(1))
        verificar(total > 5, "«security» sale en el RFC 9116 (%d)" % total)

        # Sin fichero (desde el menú o el escritorio): se abre vacío y no termina con error.
        pos = lanzar(m, "visorpdf")
        esperar_desde(m, "visorpdf: sin documento", pos, 30)
        time.sleep(1.0)
        m.captura("H274-0-sin-documento")
        verificar(True, "visorpdf sin argumento abre la ventana vacía")
        cerrar(m)

        # ---------------------------------------------------- la ventana
        pos = lanzar(m, "visorpdf /usb/indice.pdf")
        esperar_desde(m, "visorpdf: pronto (6 páginas)", pos, 60)
        verificar("visorpdf: indice 4" in m.serial()[pos:], "el visor carga el índice (4 marcadores)")
        time.sleep(2)
        wx, wy = origen_ventana(m, pos)
        e = estado(m, pos)
        verificar(e and e[0] == 1 and e[1] == 6, "arranca en la página 1 de 6 (%s)" % (e,))
        zoom0 = e[2]
        cap = m.captura("H274-1-inicio")
        img = pixeles(cap)
        # La barra de herramientas (gris claro) y el panel con miniaturas.
        verificar(contar(img, wx + 10, wy, wx + 500, wy + 3, lambda r, g, b: (r, g, b) == (0xE6, 0xE9, 0xEC)) > 1000,
                  "la barra de herramientas está dibujada")
        verificar(contar(img, wx + 20, wy + 80, wx + 160, wy + 300, lambda r, g, b: r > 245 and g > 245 and b > 245) > 500,
                  "el panel muestra el cuadro blanco de las miniaturas")
        miniaturas = set(re.findall(r"visorpdf: miniatura (\d+)", m.serial()[pos:]))
        verificar(len(miniaturas) >= 3, "las miniaturas se dibujan en segundo plano (%s)" % sorted(miniaturas))
        # El papel de la página en el área de lectura.
        verificar(contar(img, wx + 300, wy + 60, wx + 700, wy + 600, lambda r, g, b: r > 250 and g > 250 and b > 250) > 20000,
                  "la página se ve en el área de lectura")
        # El azul del rectángulo de la página 1.
        verificar(contar(img, wx + 180, wy + 60, wx + 880, wy + 640, lambda r, g, b: b > 170 and r < 90 and 80 < g < 150) > 300,
                  "el rectángulo azul de la página 1 se ve")

        # Navegación por teclado.
        p0 = len(m.serial())
        m.tecla("right", 0.5)
        time.sleep(1.0)
        e = estado(m, p0)
        verificar(e and e[0] == 2, "flecha derecha: página 2 (%s)" % (e,))
        m.tecla("end", 0.5)
        time.sleep(1.0)
        e = estado(m, p0)
        verificar(e and e[0] == 6, "Fin: la última página (%s)" % (e,))
        m.tecla("home", 0.5)
        time.sleep(1.0)
        e = estado(m, p0)
        verificar(e and e[0] == 1, "Inicio: la primera página (%s)" % (e,))

        # Botón «siguiente» de la barra (clic).
        p0 = len(m.serial())
        m.clique(wx + 201, wy + 17)
        time.sleep(1.0)
        e = estado(m, p0)
        verificar(e and e[0] == 2, "clic en «siguiente»: página 2 (%s)" % (e,))

        # Ctrl+G
        p0 = len(m.serial())
        m.tecla("ctrl-g", 1.0)
        time.sleep(1.0)
        limpiar(m, 4)
        m.digitar("4", 0.1)
        m.tecla("ret", 0.5)
        time.sleep(1.5)
        e = estado(m, p0)
        verificar(e and e[0] == 4, "Ctrl+G, 4: página 4 (%s)" % (e,))

        # Zoom y ajustes.
        p0 = len(m.serial())
        m.tecla("1", 0.5)
        time.sleep(1.0)
        e = estado(m, p0)
        verificar(e and e[2] == 100, "tecla 1: zoom 100 (%s)" % (e,))
        m.digitar("+", 0.5)   # +
        time.sleep(1.0)
        e = estado(m, p0)
        verificar(e and e[2] == 125, "tecla +: zoom 125 (%s)" % (e,))
        m.digitar("-", 0.5)
        m.digitar("-", 0.5)
        time.sleep(1.0)
        e = estado(m, p0)
        verificar(e and e[2] == 75, "dos veces -: zoom 75 (%s)" % (e,))
        m.tecla("0", 0.5)
        time.sleep(1.0)
        e = estado(m, p0)
        zoom_pag = e[2]
        m.tecla("w", 0.5)
        time.sleep(1.0)
        e = estado(m, p0)
        verificar(e and e[2] > zoom_pag, "ajustar al ancho amplía más que ajustar a la página (%d > %d)" % (e[2], zoom_pag))
        m.captura("H274-2-ajuste-ancho")

        # Giro.
        m.tecla("r", 0.5)
        time.sleep(1.5)
        e = estado(m, p0)
        verificar(e and e[3] == 90, "r: giro de 90 grados (%s)" % (e,))
        m.captura("H274-3-girado")
        m.tecla("r", 0.4)
        m.tecla("r", 0.4)
        m.tecla("r", 0.4)
        time.sleep(1.0)
        e = estado(m, p0)
        verificar(e and e[3] == 0, "cuatro giros vuelven a 0 (%s)" % (e,))

        # Índice: pestaña «Índice» y clic en «Capitulo 2» (tercera fila).
        m.clique(wx + 135, wy + 45)
        time.sleep(1.0)
        m.captura("H274-4-indice")
        p0 = len(m.serial())
        m.clique(wx + 60, wy + 34 + 22 + 2 * 20 + 10)
        time.sleep(1.5)
        e = estado(m, p0)
        verificar(e and e[0] == 3, "clic en «Capitulo 2» del índice: página 3 (%s)" % (e,))
        m.clique(wx + 60, wy + 34 + 22 + 3 * 20 + 10)
        time.sleep(1.5)
        e = estado(m, p0)
        verificar(e and e[0] == 4, "clic en «Capítulo 3» del índice: página 4 (%s)" % (e,))
        m.clique(wx + 45, wy + 45)   # de vuelta a las miniaturas
        time.sleep(1.0)

        # Búsqueda: Ctrl+F, «zorro».
        p0 = len(m.serial())
        m.tecla("ctrl-f", 0.8)
        time.sleep(0.8)
        m.digitar("zorro", 0.15)
        esperar_desde(m, "busca 'zorro': 3 resultados", p0, 60)
        time.sleep(1.5)
        e = estado(m, p0)
        verificar(e and e[0] == 5, "la búsqueda salta a la primera coincidencia desde la página 4: la 5 (%s)" % (e,))
        cap = m.captura("H274-5-busqueda")
        img = pixeles(cap)
        # Resaltado: pixeles naranja (coincidencia actual) y/o amarillo.
        verificar(contar(img, wx + 180, wy + 64, wx + 880, wy + 640,
                         lambda r, g, b: r > 240 and 130 < g < 170 and b < 80) > 30,
                  "la coincidencia actual se resalta en naranja")
        p1 = len(m.serial())
        m.tecla("ret", 0.5)     # siguiente: vuelve a la 3 (da la vuelta)
        time.sleep(1.5)
        e = estado(m, p1)
        verificar(e and e[0] == 3, "Intro: siguiente coincidencia (da la vuelta a la página 3) (%s)" % (e,))
        cap = m.captura("H274-6-busqueda-pag3")
        img = pixeles(cap)
        verificar(contar(img, wx + 180, wy + 64, wx + 880, wy + 640,
                         lambda r, g, b: r > 240 and g > 220 and b < 80) > 30,
                  "las demás coincidencias se resaltan en amarillo")
        m.tecla("esc", 0.8)
        time.sleep(1.0)
        # Con la búsqueda cerrada ya no hay resaltado.
        cap = m.captura("H274-7-busqueda-cerrada", guardar=False)
        img = pixeles(cap)
        verificar(contar(img, wx + 180, wy + 64, wx + 880, wy + 640,
                         lambda r, g, b: r > 240 and g > 220 and b < 80) < 30,
                  "al cerrar la búsqueda se quita el resaltado")

        # Presentación (F5): pantalla completa sin barras y con fondo negro.
        p0 = len(m.serial())
        m.tecla("f5", 1.0)
        esperar_desde(m, "visorpdf: presentacion", p0, 20)
        time.sleep(2.5)
        cap = m.captura("H274-8-presentacion")
        img = pixeles(cap)
        verificar(contar(img, 0, 0, 40, 40, lambda r, g, b: r < 8 and g < 8 and b < 8) > 1200,
                  "la presentación tiene fondo negro y ninguna barra arriba")
        m.tecla("esc", 1.0)
        time.sleep(2.0)
        cap = m.captura("H274-9-tras-presentacion", guardar=False)
        img = pixeles(cap)
        verificar(contar(img, wx + 10, wy, wx + 500, wy + 3, lambda r, g, b: (r, g, b) == (0xE6, 0xE9, 0xEC)) > 1000,
                  "al salir de la presentación vuelve la barra de herramientas")

        # Exportar la página a una imagen (Ctrl+S).
        p0 = len(m.serial())
        m.tecla("ctrl-s", 1.0)
        time.sleep(1.5)
        limpiar(m, 40)
        m.digitar("/home/pag.ppm", 0.1)
        m.tecla("ret", 0.5)
        time.sleep(2.0)
        esperar_desde(m, "visorpdf: exportada /home/pag.ppm", p0, 30)
        time.sleep(0.5)

        # Recordar página y zoom: página 5, zoom 150, cerrar y reabrir.
        m.tecla("1", 0.4)
        m.digitar("+", 0.4)
        m.digitar("+", 0.4)
        m.digitar("+", 0.4)
        p0 = len(m.serial())
        m.tecla("ctrl-g", 1.0)
        time.sleep(1.0)
        limpiar(m, 4)
        m.digitar("5", 0.1)
        m.tecla("ret", 0.5)
        time.sleep(1.5)
        e = estado(m, p0)
        verificar(e and e[0] == 5, "otra vez a la página 5 (%s)" % (e,))
        zoom_ult = e[2]
        time.sleep(2.0)
        cerrar(m)
        out = m.comando("ls /home")
        verificar("pag.ppm" in out, "la página exportada está en /home")
        out = m.comando("cat /home/.config/visorpdf.conf")
        verificar("/usb/indice.pdf" in out, "visorpdf.conf guarda el documento")
        pos = lanzar(m, "visorpdf /usb/indice.pdf")
        s = esperar_desde(m, "visorpdf: pronto", pos, 60)
        verificar("recordado pagina=5 zoom=%d" % zoom_ult in s,
                  "al reabrir, vuelve a la página 5 con el zoom que tenía (%d)" % zoom_ult)
        time.sleep(1.5)
        cerrar(m)

        # ---------------------------------------------------- contraseña
        pos = lanzar(m, "visorpdf /usb/cifrado.pdf")
        esperar_desde(m, "visorpdf: pide contraseña", pos, 60)
        time.sleep(1.5)
        m.digitar("mala", 0.15)
        m.captura("H274-10-contrasena")
        m.tecla("ret", 0.5)
        esperar_desde(m, "visorpdf: contraseña incorrecta", pos, 30)
        time.sleep(1.5)
        m.captura("H274-11-contrasena-mala")
        m.digitar("sagui", 0.15)
        m.tecla("ret", 0.5)
        esperar_desde(m, "visorpdf: pronto (1 páginas)", pos, 60)
        time.sleep(1.5)
        m.captura("H274-12-cifrado-abierto")
        cerrar(m)

        # ---------------------------------------------------- un PDF real, con índice y miniaturas
        pos = lanzar(m, "visorpdf /usb/enumitem.pdf")
        esperar_desde(m, "visorpdf: pronto (24 páginas)", pos, 120)
        time.sleep(4)
        miniaturas = set(re.findall(r"visorpdf: miniatura (\d+)", m.serial()[pos:]))
        verificar(len(miniaturas) >= 2, "el PDF real también saca miniaturas (%d)" % len(miniaturas))
        p0 = len(m.serial())
        m.tecla("ctrl-f", 0.8)
        time.sleep(0.8)
        m.digitar("enumitem", 0.15)
        esperar_desde(m, "busca 'enumitem'", p0, 240)
        m.captura("H274-13-real-busqueda")
        cerrar(m)

    print("H274: todo bien")


if __name__ == "__main__":
    try:
        main()
    except Exception as exc:  # noqa: BLE001
        print("H274 FALLÓ:", exc)
        sys.exit(1)
