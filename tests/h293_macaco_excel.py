#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H293 - Macaco con el estilo de Excel 97 (mejora 117, D293).

Criterio:
  - interfaz: menús, barras Estándar y de Formato, barra de fórmulas con cuadro de nombre,
    cabeceras biseladas, pestañas de hojas y barra de estado con SUMA de la selección
    (capturas H293-*.png);
  - formato de celdas: moneda y negrita por la barra, diálogo Formato de celdas (Alineación,
    Fuente, Bordes, Tramas), con el resultado visible en el volcado `macaco -t`;
  - teclado: F2, Mayús+flechas, F4 (referencias absolutas), Ctrl+B;
  - guardar y reabrir el .mxs con los formatos; otro programa viejo ignoraría las líneas nuevas;
  - CSV de salida y de entrada, y PDF exportado por el diálogo y por --convertir, validado
    en el anfitrión (estructura, texto, A4).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, mtools, SAIDA  # noqa: E402
import pdf_validar  # noqa: E402

GRID_X, GRID_Y = 40, 118
COL_W, ROW_H = 72, 20


def janela(serial, titulo):
    achada = re.findall(rf'criada por \d+: "{re.escape(titulo)}[^"]*" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)
    m = achada[-1]
    return dict(l=int(m[0]), a=int(m[1]), x=int(m[2]) + 2, y=int(m[3]) + 24)


def celula(a, l, c):
    return (a["x"] + GRID_X + c * COL_W + COL_W // 2, a["y"] + GRID_Y + l * ROW_H + ROW_H // 2)


def escrever(m, texto):
    m.digitar(texto, 0.05)
    m.tecla("ret", 0.2)


MENUS = ["Archivo", "Edición", "Ver", "Insertar", "Formato", "Herramientas", "Datos", "Ayuda"]


def principal():
    pen = criar_pendrive("h293-pen", rotulo="MACACO", arquivos={"nota.txt": b"hola\n"})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    def sacar(nome):
        local = os.path.join(SAIDA, "h293-" + nome)
        mtools(pen, "mcopy", "-o", "p:/" + nome, local)
        verificar(os.path.exists(local), "%s salió del pendrive" % nome)
        with open(local, "rb") as f:
            return f.read()

    with Maquina("h293", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "el pendrive está montado")

        m.digitar("macaco --clasico\n", 0.05)
        m.esperar_serial("macaco: pronto", 30)
        time.sleep(1.5)
        a = janela(m.serial(), "Macaco")
        verificar(a["l"] >= 800, "la ventana tiene su tamaño (%d)" % a["l"])
        m.captura("H293-1-vacia")

        # ---- datos
        escrever(m, "Norte")
        escrever(m, "Sur")
        escrever(m, "Este")
        for _ in range(3):
            m.tecla("up", 0.1)
        m.tecla("tab", 0.1)
        escrever(m, "1234.5")
        escrever(m, "17")
        escrever(m, "25")
        time.sleep(0.3)

        # ---- cuadro de nombre: ir a B1:B3 y ver la SUMA en la barra de estado
        m.clique(a["x"] + 40, a["y"] + 76 + 12)               # el cuadro de nombre
        m.digitar("B1:B3", 0.06)
        m.tecla("ret", 0.4)
        time.sleep(0.4)
        m.captura("H293-2-seleccion-suma")

        # ---- formato: moneda por la barra de Formato (el botón $ va tras 3 de N-K-S,
        # separador, 4 de alineación... se busca por la posición calculada)
        # Formato: [combo 146][combo 58][sep 8][N][K][S][sep][izq][cen][der][sep][$]
        x_dolar = 4 + 146 + 58 + 8 + 3 * 23 + 8 + 3 * 23 + 8 + 11
        marca = len(m.serial())
        m.clique(a["x"] + x_dolar, a["y"] + 48 + 3 + 11)
        time.sleep(0.4)
        # negrita con Ctrl+B
        m.tecla("ctrl-b", 0.4)
        m.captura("H293-3-moneda-negrita")

        # ---- diálogo Formato de celdas (Ctrl+1) sobre A1: alineación centrada y relleno amarillo
        m.clique(*celula(a, 0, 0))
        marca = len(m.serial())
        m.tecla("ctrl-1", 0.6)
        m.esperar_serial('"Formato de celdas"', 15)
        time.sleep(0.6)
        d = janela(m.serial(), "Formato de celdas")
        m.captura("H293-4-celdas-numero")
        tab = lambda x0, w: (d["x"] + x0 + w // 2, d["y"] + 8 + 11)   # noqa: E731
        m.clique(*tab(78, 108))                                        # Alineación
        time.sleep(0.4)
        m.tecla("down", 0.15)
        m.tecla("down", 0.15)                                          # General -> Centro
        time.sleep(0.3)
        m.captura("H293-5-celdas-alineacion")
        m.clique(*tab(186, 68))                                        # Fuente
        time.sleep(0.5)
        m.captura("H293-6-celdas-fuente")
        m.clique(*tab(254, 68))                                        # Bordes
        time.sleep(0.5)
        m.captura("H293-7-celdas-bordes")
        m.clique(*tab(322, 68))                                        # Tramas
        time.sleep(0.4)
        m.tecla("home", 0.2)                                           # primer color: amarillo
        time.sleep(0.4)
        m.captura("H293-8-celdas-tramas")
        m.tecla("ret", 0.8)
        m.esperar_serial("macaco: formato de celdas aplicado", 15)
        verificar("macaco: formato de celdas aplicado" in m.serial()[marca:], "el diálogo aplica el formato")

        # ---- teclado: F2, F4 y Mayús+flechas
        m.clique(*celula(a, 0, 2))                                     # C1
        m.tecla("f2", 0.3)
        m.digitar("=B1", 0.05)
        m.tecla("f4", 0.3)                                             # B1 -> $B$1
        m.tecla("ret", 0.3)
        m.esperar_serial("celula C1 = 1234.5 (=$B$1)", 10)
        m.clique(*celula(a, 0, 0))
        m.tecla("shift-down", 0.15)
        m.tecla("shift-down", 0.15)
        m.tecla("shift-right", 0.15)
        time.sleep(0.3)
        m.captura("H293-9-mayus-flechas")

        # ---- menú Formato de la barra: lo abrimos solo para la captura y lo cerramos
        marca = len(m.serial())
        x = 2 + sum(len(n) * 8 + 14 for n in MENUS[:4]) + 30
        m.clique(a["x"] + x, a["y"] + 10)
        time.sleep(0.8)
        m.captura("H293-10-menu-formato")
        m.tecla("esc", 0.4)

        # ---- pestañas: segunda hoja y su menú
        x = 6 + len("Hoja1") * 8 + 22 + 14
        m.clique(a["x"] + x, a["y"] + a["a"] - 44 + 10)
        m.esperar_serial("macaco: nueva hoja Hoja2", 15)
        time.sleep(0.4)
        m.captura("H293-11-hojas")
        m.clique(a["x"] + 6 + 30, a["y"] + a["a"] - 44 + 10)           # de vuelta a Hoja1
        time.sleep(0.4)

        # ---- guardar
        marca = len(m.serial())
        m.tecla("ctrl-s", 0.5)
        m.esperar_serial('"Guardar como"', 15)
        time.sleep(0.5)
        for _ in range(30):
            m.tecla("backspace", 0.03)
        m.digitar("/tmp/livro.mxs", 0.05)
        m.tecla("ret", 0.5)
        m.esperar_serial("macaco: guardado /tmp/livro.mxs", 20)

        # ---- PDF por el diálogo (Ctrl+P) a /usb
        m.tecla("ctrl-p", 0.6)
        m.esperar_serial('"Exportar a PDF"', 15)
        time.sleep(0.5)
        for _ in range(40):
            m.tecla("backspace", 0.03)
        m.digitar("/usb/gui.pdf", 0.05)
        m.tecla("ret", 0.6)
        m.esperar_serial("macaco: pdf exportado /usb/gui.pdf", 20)
        time.sleep(0.5)
        m.captura("H293-12-pdf-exportado")

        # ---- cerrar (con cambios guardados no pregunta)
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("macaco: cerrado", 20)
        time.sleep(0.8)

        # ---- el fichero y su recarga: formatos
        out = m.comando("cat /tmp/livro.mxs")
        verificar(re.search(r"^formato A1 .*al=2 .*rel=1ffff00", out, re.M) is not None, "A1: centrada y con relleno amarillo en el .mxs")
        verificar(re.search(r"^formato B1 nf=2 dec=2 .*est=1", out, re.M) is not None, "B1: moneda con 2 decimales y negrita en el .mxs")
        verificar("celda C1 =$B$1" in out, "la referencia absoluta de F4 se guarda")
        out = m.comando("macaco -t /tmp/livro.mxs")
        verificar("celula Hoja1!C1 = 1234.5" in out, "al recargar, =$B$1 da 1234.5")
        verificar(re.search(r"formato Hoja1!B1 nf=2 dec=2 .*vis=\[\$1,234\.50\]", out) is not None, "B1 se ve como $1,234.50")
        verificar(re.search(r"formato Hoja1!B2 nf=2 .*vis=\[\$17\.00\]", out) is not None, "B2 se ve como $17.00")
        verificar(re.search(r"formato Hoja1!A1 .*al=2", out) is not None, "A1 sigue centrada tras recargar")

        # ---- CSV: salida y entrada
        out = m.comando("macaco --convertir /tmp/livro.mxs /tmp/libro.csv")
        verificar("convertido /tmp/livro.mxs -> /tmp/libro.csv bien" in out, "exporta a CSV")
        out = m.comando("cat /tmp/libro.csv")
        verificar("Norte,1234.5" in out and "Sur,17" in out and "Este,25" in out, "el CSV lleva los valores sin formato")
        m.comando("macaco --convertir /tmp/libro.csv /tmp/de_csv.mxs")
        out = m.comando("macaco -t /tmp/de_csv.mxs")
        verificar("celula Hoja1!A1 = Norte" in out and "celula Hoja1!B1 = 1234.5" in out, "el CSV se importa de vuelta")

        # ---- PDF por --convertir (con formatos) y validación en el anfitrión
        out = m.comando("macaco --convertir /tmp/livro.mxs /usb/salida.pdf", 120)
        verificar("convertido /tmp/livro.mxs -> /usb/salida.pdf bien" in out, "exporta a PDF sin ventana")
        m.comando("sync")
        for nome in ("gui.pdf", "salida.pdf"):
            pdf = sacar(nome)
            info = pdf_validar.analizar(pdf)
            verificar(info["paginas"] >= 1, "%s: %d página(s)" % (nome, info["paginas"]))
            verificar(abs(info["mediabox"][0] - 595.3) < 1 and abs(info["mediabox"][1] - 841.9) < 1, "%s: papel A4 %s" % (nome, info["mediabox"]))
            texto = " ".join(pdf_validar.texto_de(info["contenidos"][0]))
            verificar("Norte" in texto and "Este" in texto, "%s: lleva el texto de las celdas" % nome)
            verificar("$1,234.50" in texto, "%s: lleva el número con formato de moneda" % nome)
            verificar(b"Helvetica" in b" ".join(info["fuentes"]), "%s: usa Helvetica" % nome)
            ext = pdf_validar.validar_externo(os.path.join(SAIDA, "h293-" + nome))
            if ext is None:
                print("   (qpdf/pdftotext no están en el anfitrión: validación propia)")
            elif "qpdf" in ext:
                verificar(ext["qpdf"][0] in (0, 3), "%s: qpdf --check: %s" % (nome, ext["qpdf"][1].strip().splitlines()[-1:]))
    return 0


if __name__ == "__main__":
    sys.exit(principal())
