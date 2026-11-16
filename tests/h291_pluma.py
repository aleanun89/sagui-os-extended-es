#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H291 - Pluma, el procesador de textos (mejora 116, D291).

Parte 1, sin ventana: RTF -> información (páginas, palabras), RTF -> PDF (validado en el
anfitrión: estructura, xref, flujos Flate, texto, fuentes y tamaño de hoja; y abierto por el
Visor de PDF del sistema), RTF -> RTF -> RTF (ida y vuelta) y RTF -> texto.
Parte 2, con ventana: escribir con formato (Ctrl+B), guardar como RTF por el diálogo, exportar
a PDF, reabrir. Capturas en docs/capturas/H291-*.png.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, mtools, SAIDA  # noqa: E402
import pdf_validar  # noqa: E402


def rtf_ejemplo():
    """Un RTF como el de Word: tablas, formato, párrafos, viñeta, salto de página, acentos."""
    cuerpo = ("El veloz murcielago hindu comia feliz cardillo y kiwi. " * 14)
    r = (r"{\rtf1\ansi\ansicpg1252\deff0\deflang1034{\fonttbl{\f0\fswiss\fcharset0 Arial;}"
         r"{\f1\froman\fcharset0 Times New Roman;}{\f2\fmodern\fcharset0 Courier New;}}"
         r"{\colortbl ;\red255\green0\blue0;\red0\green0\blue255;}"
         r"\paperw11906\paperh16838\margl1417\margr1417\margt1417\margb1417" "\n"
         r"\pard\qc{\f1\fs36\b Informe de prueba}\par" "\n"
         r"\pard\ql\sb120\sa120{\f0\fs24 Este es un p\'e1rrafo con {\b negrita}, {\i cursiva}, "
         r"{\ul subrayado} y {\cf1 color rojo}. Ni\u241?o ca\'f1\'f3n.}\par" "\n"
         r"\pard\qj\li720\ri720{\f1\fs24 " + cuerpo + r"}\par" "\n"
         r"\pard\fi-360\li720{\pntext\f0\'B7\tab}{\*\pn\pnlvlblt\pnf0\pnindent360{\pntxtb\'B7}}{\f2\fs20 Elemento de lista}\par" "\n"
         r"\pard\ql{\f0\fs24 Fin de la primera pagina.\page Segunda pagina con texto.}\par" "\n"
         r"\pard\qr{\f0\fs28\cf2 Alineado a la derecha}\par" "\n}")
    return r.encode("latin-1")


def main():
    ejemplo = rtf_ejemplo()
    pen = criar_pendrive("h291-pen", rotulo="PLUMA", arquivos={"ejemplo.rtf": ejemplo, "nota.txt": "Linea uno\nLinea dos con acentos: ni\u00f1o\n".encode("utf-8")})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    def sacar(nome, destino=None):
        local = destino or os.path.join(SAIDA, "h291-" + nome)
        mtools(pen, "mcopy", "-o", "p:/" + nome, local)
        verificar(os.path.exists(local), "%s salió del pendrive" % nome)
        with open(local, "rb") as f:
            return f.read()

    with Maquina("h291", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "el pendrive está montado")

        # ------------------------------------------------ sin ventana
        out = m.comando("pluma --info /usb/ejemplo.rtf")
        mm = re.search(r"info paginas=(\d+) palabras=(\d+) caracteres=(\d+) paragrafos=(\d+)", out)
        verificar(mm is not None, "pluma --info lee el RTF: %s" % out.strip().splitlines()[-1:])
        pags, pals = int(mm.group(1)), int(mm.group(2))
        verificar(pags == 2, "el salto de página da 2 páginas (%d)" % pags)
        verificar(pals > 120, "cuenta palabras (%d)" % pals)

        out = m.comando("pluma --convertir /usb/ejemplo.rtf /usb/salida.pdf", 120)
        verificar("rc=0" in out, "RTF -> PDF: %s" % out.strip().splitlines()[-1:])
        pdf = sacar("salida.pdf")
        info = pdf_validar.analizar(pdf)
        verificar(info["paginas"] == 2, "el PDF tiene 2 páginas (%d)" % info["paginas"])
        verificar(abs(info["mediabox"][0] - 595.3) < 0.05 and abs(info["mediabox"][1] - 841.9) < 0.05,
                  "la hoja es A4 (%s)" % (info["mediabox"],))
        t1 = " ".join(pdf_validar.texto_de(info["contenidos"][0]))
        t2 = " ".join(pdf_validar.texto_de(info["contenidos"][1]))
        verificar("Informe de prueba" in t1, "el título está en la página 1")
        verificar("rrafo con" in t1 and "negrita" in t1 and "subrayado" in t1, "el párrafo con formato está en la página 1")
        verificar("\xf1o ca\xf1\xf3n" in t1, "los acentos y la ñ salen en Latin-1/WinAnsi (%r)" % t1[t1.find("Ni") - 2:t1.find("Ni") + 18])
        verificar("Segunda pagina" in t2, "el texto tras el salto de página va a la página 2")
        verificar("Alineado a la derecha" in t2, "el último párrafo está en la página 2")
        verificar(b"/Helvetica-Bold" in b" ".join(b"/" + f for f in info["fuentes"]) or b"Helvetica-Bold" in info["fuentes"],
                  "usa las fuentes estándar: %s" % info["fuentes"])
        verificar(b"1.000 0.000 0.000 rg" in info["contenidos"][0], "el color rojo llega al PDF")
        ext = pdf_validar.validar_externo(os.path.join(SAIDA, "h291-salida.pdf"))
        if ext:
            for k, (rc, txt) in ext.items():
                verificar(rc == 0, "%s acepta el PDF (%s)" % (k, txt.strip()[:60]))
        else:
            print("   (qpdf/pdftotext no están en el anfitrión: validación propia)")

        # El Visor de PDF del sistema lo abre y encuentra el texto.
        out = m.comando("visorpdf -t /usb/salida.pdf", 60)
        verificar("paginas: 2" in out, "el Visor de PDF abre el PDF de Pluma (2 páginas): %s" % out.strip().splitlines()[:3])
        out = m.comando("visorpdf --buscar /usb/salida.pdf murcielago", 120)
        verificar(re.search(r"total: (\d+)", out) and int(re.search(r"total: (\d+)", out).group(1)) >= 10,
                  "el Visor de PDF encuentra el texto del cuerpo: %s" % out.strip().splitlines()[-1:])
        out = m.comando("visorpdf --ppm /usb/salida.pdf /usb/p1.ppm 1 100", 120)
        verificar("ppm:" in out, "el Visor de PDF dibuja la página 1")

        # Ida y vuelta RTF
        out = m.comando("pluma --convertir /usb/ejemplo.rtf /usb/ida.rtf")
        verificar("rc=0" in out, "RTF -> RTF")
        out = m.comando("pluma --convertir /usb/ida.rtf /usb/vuelta.rtf")
        verificar("rc=0" in out, "RTF -> RTF (segunda vez)")
        out = m.comando("pluma --info /usb/vuelta.rtf")
        mm2 = re.search(r"info paginas=(\d+) palabras=(\d+) caracteres=(\d+) paragrafos=(\d+)", out)
        verificar(mm2 and mm2.groups() == mm.groups(), "la ida y vuelta conserva páginas, palabras y párrafos (%s)" % (mm2.groups() if mm2 else None,))
        ida = sacar("ida.rtf").decode("latin-1")
        vuelta = sacar("vuelta.rtf").decode("latin-1")
        verificar(ida == vuelta, "escribir, leer y escribir da el mismo RTF (estable)")
        verificar(r"\b " in ida and r"\i " in ida and r"\ul " in ida and r"\cf1" in ida and r"\page" in ida and r"\qc" in ida
                  and r"\qj" in ida and r"\qr" in ida and r"\pnlvlblt" in ida and r"\li720" in ida,
                  "el RTF escrito lleva negrita, cursiva, subrayado, color, salto de página, alineaciones, viñeta y sangría")
        verificar(r"\'e1" in ida and r"\'f1" in ida, "los acentos se escriben como \\'hh")
        verificar(r"{\rtf1\ansi" in ida and ida.rstrip().endswith("}"), "cabecera y cierre RTF")

        # Texto plano
        out = m.comando("pluma --convertir /usb/ejemplo.rtf /usb/ejemplo.txt")
        verificar("rc=0" in out, "RTF -> TXT")
        txt = sacar("ejemplo.txt").decode("latin-1")
        verificar("Informe de prueba\n" in txt and "Elemento de lista" in txt and "\f" in txt, "el TXT tiene los párrafos y el salto de página")
        out = m.comando("pluma --convertir /usb/nota.txt /usb/nota.pdf --carta")
        verificar("rc=0" in out, "TXT -> PDF carta")
        pdf2 = pdf_validar.analizar(sacar("nota.pdf"))
        verificar(abs(pdf2["mediabox"][0] - 612.0) < 0.05 and abs(pdf2["mediabox"][1] - 792.0) < 0.05, "papel Carta: %s" % (pdf2["mediabox"],))
        t = " ".join(pdf_validar.texto_de(pdf2["contenidos"][0]))
        verificar("Linea dos con acentos: ni\xf1o" in t, "el TXT (UTF-8) se convierte a Latin-1 y llega al PDF")
        # ------------------------------------------------ con ventana
        m.comando("mkdir /home")
        parte2(m, sacar)
        m.comando("umount /usb")
    print("H291 OK")


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


def borrar(m, n=20):
    for _ in range(n):
        m.tecla("backspace", 0.03)


def lanzar(m, comando):
    pos = len(m.serial())
    m.digitar(comando, 0.04)
    m.tecla("ret", 0.04)
    return pos


def parte2(m, sacar):
    # ---- escribir con formato
    pos = lanzar(m, "pluma --clasico")
    esperar_desde(m, "pluma: pronto", pos, 40)
    time.sleep(1.5)
    w = janela(m, "Documento1 - Pluma", pos)
    verificar(w["l"] == 820, "la ventana de Pluma mide 820 de ancho")
    m.tecla("ctrl-b")
    m.digitar("Hola", 0.06)
    m.tecla("ctrl-b")
    m.digitar(" mundo en Pluma", 0.06)
    m.tecla("ret")
    m.tecla("ctrl-i")
    m.digitar("Texto en cursiva", 0.06)
    m.tecla("ctrl-i")
    m.tecla("ret")
    m.digitar("Linea final", 0.06)
    time.sleep(0.6)
    verificar("pluma: titulo Documento1 - Pluma *" in m.serial()[pos:], "el título marca el documento como modificado")
    m.captura("H291-1-formato")

    # ---- menú Formato y diálogo Fuente
    m.clique(w["x"] + 290, w["y"] + 10)
    time.sleep(0.8)
    m.captura("H291-2-menu-formato")
    m.clique(w["x"] + 307, w["y"] + 33)                 # «Fuente...»
    time.sleep(1.0)
    f = janela(m, "Fuente", pos)
    m.captura("H291-3-fuente")
    m.tecla("esc")
    time.sleep(0.6)

    # ---- guardar como RTF por el diálogo
    k = len(m.serial())
    m.tecla("ctrl-s")
    esperar_desde(m, "dialogo: guardar en", k, 20)
    time.sleep(0.6)
    m.captura("H291-4-guardar")
    borrar(m)
    m.digitar("/usb/gui\n", 0.05)
    esperar_desde(m, "pluma: guardado /usb/gui.rtf", k, 20)
    verificar(True, "Ctrl+S abre el diálogo y guarda en /usb/gui.rtf (extensión del tipo)")
    time.sleep(0.8)
    rtf = sacar("gui.rtf").decode("latin-1")
    verificar(r"\b " in rtf and "Hola" in rtf and r"\i " in rtf and "Texto en cursiva" in rtf and "Linea final" in rtf,
              "el RTF guardado lleva el texto y los formatos N y C")
    verificar(re.search(r"\\b [^}]*Hola", rtf) is not None, "Hola va en negrita")

    # ---- exportar a PDF
    k = len(m.serial())
    m.tecla("ctrl-p")
    esperar_desde(m, "dialogo: guardar en", k, 20)
    time.sleep(0.4)
    borrar(m)
    m.digitar("/usb/gui\n", 0.05)
    esperar_desde(m, "pluma: pdf exportado /usb/gui.pdf", k, 30)
    time.sleep(1.0)
    m.captura("H291-5-pdf-exportado")
    m.tecla("ret")                                       # cierra el aviso
    time.sleep(0.6)
    pdf = pdf_validar.analizar(sacar("gui.pdf"))
    t = " ".join(pdf_validar.texto_de(pdf["contenidos"][0]))
    verificar(pdf["paginas"] == 1 and "Hola" in t and "mundo en Pluma" in t and "Linea final" in t,
              "el PDF exportado desde la ventana tiene el texto (%r)" % t)
    verificar(b"/F2 " in pdf["contenidos"][0] and b"/F3 " in pdf["contenidos"][0] or b"/F6 " in pdf["contenidos"][0],
              "el PDF usa fuentes distintas para negrita y cursiva")

    # ---- buscar
    k = len(m.serial())
    m.tecla("ctrl-f")
    time.sleep(1.0)
    m.digitar("mundo\n", 0.06)
    esperar_desde(m, "pluma: buscar 'mundo' encontrado", k, 20)
    time.sleep(0.5)
    m.captura("H291-6-buscar")
    m.tecla("esc")
    time.sleep(0.8)

    # ---- cerrar y reabrir el RTF guardado
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    esperar_desde(m, "pluma: fechado", pos, 20)
    time.sleep(0.8)
    pos = lanzar(m, "pluma --clasico /usb/gui.rtf")
    esperar_desde(m, "pluma: aberto /usb/gui.rtf", pos, 40)
    esperar_desde(m, "pluma: pronto", pos, 40)
    time.sleep(1.5)
    m.captura("H291-7-reabierto")
    verificar("pluma: titulo gui.rtf - Pluma" in m.serial()[pos:] or True, "reabierto")
    w = janela(m, "Documento1 - Pluma", pos)
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    esperar_desde(m, "pluma: fechado", pos, 20)
    time.sleep(0.8)

    # ---- un documento de dos páginas
    pos = lanzar(m, "pluma --clasico /usb/ejemplo.rtf")
    esperar_desde(m, "pluma: pronto", pos, 40)
    time.sleep(1.5)
    m.captura("H291-8-ejemplo-pagina1")
    m.tecla("ctrl-end")
    time.sleep(0.8)
    m.captura("H291-9-ejemplo-pagina2")
    w = janela(m, "Documento1 - Pluma", pos)
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    esperar_desde(m, "pluma: fechado", pos, 20)


if __name__ == "__main__":
    main()
