#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H311 - Macaco: señalar celdas al escribir fórmulas, controlador de relleno y gráficos incrustados
(mejora 117, D311).

Criterio:
  - «=» + clic en B1 + «+» + clic en B2 da `=B1+B2` (con los marcos de color en la captura);
    arrastrar da un rango (`=SUMA(B1:B3)`); las flechas señalan (Mayús amplía); F4 cicla $;
    un clic en otra pestaña inserta `Hoja2!A1`;
  - controlador de relleno: arrastrar `=A1*$D$4` de C1 a C5 da `=A5*$D$4`; la serie 1, 2 sigue 3, 4, 5;
    el doble clic rellena hasta donde llegan los datos vecinos; Ctrl+D; copiar y pegar ajusta las
    referencias relativas; un relleno se deshace en un solo paso;
  - gráficos incrustados (parte 2, ver más abajo en este fichero).
Capturas H311-*.png.
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


def asa(a, l, c):
    """El controlador de relleno de la celda (l, c): su esquina inferior derecha (un píxel dentro)."""
    return (a["x"] + GRID_X + (c + 1) * COL_W - 1, a["y"] + GRID_Y + (l + 1) * ROW_H - 1)


def escrever(m, texto):
    m.digitar(texto, 0.05)
    m.tecla("ret", 0.25)


def ir_a(m, a, l, c):
    m.clique(*celula(a, l, c))


def pestana_y(a):
    return a["y"] + a["a"] - 44 + 10


def fase_formulas(m, a):
    # ---- datos: A1:A5 = 1..5, B1:B3 = 10, 20, 30, D4 = 3
    ir_a(m, a, 0, 0)
    for v in "12345":
        escrever(m, v)
    ir_a(m, a, 0, 1)
    for v in ("10", "20", "30"):
        escrever(m, v)
    ir_a(m, a, 3, 3)
    escrever(m, "3")

    # ---- E1: «=» + clic en B1 + «+» + clic en B2
    marca = len(m.serial())
    ir_a(m, a, 0, 4)
    m.digitar("=", 0.05)
    m.clique(*celula(a, 0, 1))
    m.digitar("+", 0.05)
    m.clique(*celula(a, 1, 1))
    time.sleep(0.4)
    m.captura("H311-1-senalar-colores")
    m.tecla("ret", 0.4)
    m.esperar_serial("celula E1 = 30 (=B1+B2)", 15)
    verificar("macaco: señalada =B1" in m.serial()[marca:], "el clic en B1 inserta B1 tras «=»")

    # ---- E2: =SUMA( + arrastrar B1:B3 + )
    ir_a(m, a, 1, 4)
    m.digitar("=SUMA(", 0.05)
    m.mouse_para(*celula(a, 0, 1))
    m.monitor("mouse_button 1")
    time.sleep(0.2)
    m.mouse_mover(*celula(a, 2, 1))
    time.sleep(0.4)
    m.captura("H311-2-senalar-rango")
    m.monitor("mouse_button 0")
    time.sleep(0.4)
    m.digitar(")", 0.05)
    m.tecla("ret", 0.4)
    m.esperar_serial("celula E2 = 60 (=SUMA(B1:B3))", 15)

    # ---- F2: =SUMA( + flechas: 4 a la izquierda (B2) y Mayús+arriba (B1:B2)
    ir_a(m, a, 1, 5)
    m.digitar("=SUMA(", 0.05)
    for _ in range(4):
        m.tecla("left", 0.15)
    m.tecla("shift-up", 0.2)
    m.digitar(")", 0.05)
    m.tecla("ret", 0.4)
    m.esperar_serial("celula F2 = 30 (=SUMA(B1:B2))", 15)

    # ---- G1: clic en B1 y F4 (B1 -> $B$1 -> B$1)
    ir_a(m, a, 0, 6)
    m.digitar("=", 0.05)
    m.clique(*celula(a, 0, 1))
    m.tecla("f4", 0.2)
    m.tecla("f4", 0.2)
    m.tecla("ret", 0.4)
    m.esperar_serial("celula G1 = 10 (=B$1)", 15)

    # ---- hoja 2 y señalar en otra pestaña
    x_mas = 6 + len("Hoja1") * 8 + 22 + 14
    m.clique(a["x"] + x_mas, pestana_y(a))
    m.esperar_serial("macaco: nueva hoja Hoja2", 15)
    ir_a(m, a, 0, 0)
    escrever(m, "99")
    m.clique(a["x"] + 6 + 30, pestana_y(a))                         # de vuelta a Hoja1
    time.sleep(0.4)
    ir_a(m, a, 1, 6)                                                # G2
    m.digitar("=", 0.05)
    m.clique(a["x"] + 6 + 62 + 30, pestana_y(a))                    # la pestaña Hoja2
    time.sleep(0.4)
    m.clique(*celula(a, 0, 0))
    time.sleep(0.3)
    m.captura("H311-3-senalar-otra-hoja")
    m.tecla("ret", 0.4)
    m.esperar_serial("celula G2 = 99 (=Hoja2!A1)", 15)

    # ---- relleno: C1 = =A1*$D$4, arrastrar el controlador hasta C5; se deshace en un paso y se repite
    ir_a(m, a, 0, 2)
    escrever(m, "=A1*$D$4")
    ir_a(m, a, 0, 2)
    time.sleep(0.3)
    x0, y0 = asa(a, 0, 2)
    m.mouse_para(x0 - 1, y0 - 1)
    m.monitor("mouse_button 1")
    time.sleep(0.2)
    m.mouse_mover(*celula(a, 4, 2))
    time.sleep(0.4)
    m.captura("H311-4-relleno-arrastrando")
    m.monitor("mouse_button 0")
    time.sleep(0.5)
    m.esperar_serial("macaco: relleno dir=0 cuantas=4", 15)
    m.captura("H311-5-relleno-hecho")
    m.tecla("ctrl-z", 0.5)                                          # un solo paso deshace todo el relleno
    time.sleep(0.3)
    m.captura("H311-5b-relleno-deshecho")
    ir_a(m, a, 0, 2)
    x0, y0 = asa(a, 0, 2)
    m.mouse_para(x0 - 1, y0 - 1)
    m.monitor("mouse_button 1")
    time.sleep(0.2)
    m.mouse_mover(*celula(a, 4, 2))
    m.monitor("mouse_button 0")
    time.sleep(0.5)

    # ---- serie 1, 2 -> 3, 4, 5 en H1:H5
    ir_a(m, a, 0, 7)
    escrever(m, "1")
    escrever(m, "2")
    m.arrastar(*celula(a, 0, 7), *celula(a, 1, 7))
    time.sleep(0.3)
    x0, y0 = asa(a, 1, 7)
    m.arrastar(x0 - 1, y0 - 1, *celula(a, 4, 7))
    m.captura("H311-6-serie")

    # ---- doble clic en el controlador: I1 = =H1*2 hasta donde llega H
    ir_a(m, a, 0, 8)
    escrever(m, "=H1*2")
    ir_a(m, a, 0, 8)
    time.sleep(0.3)
    x0, y0 = asa(a, 0, 8)
    m.clique(x0 - 1, y0 - 1, duplo=True)
    m.esperar_serial("macaco: relleno dir=0 cuantas=4", 15)

    # ---- Ctrl+D: J1 = =A1+100, seleccionar J1:J3 y Ctrl+D
    ir_a(m, a, 0, 9)
    escrever(m, "=A1+100")
    m.arrastar(*celula(a, 0, 9), *celula(a, 2, 9))
    m.tecla("ctrl-d", 0.5)
    m.esperar_serial("macaco: rellenar hacia abajo (Ctrl+D)", 15)

    # ---- copiar y pegar ajusta lo relativo: C1 -> C7
    ir_a(m, a, 0, 2)
    m.tecla("ctrl-c", 0.3)
    ir_a(m, a, 6, 2)
    m.tecla("ctrl-v", 0.5)

    # ---- un relleno deshecho en un paso: K1 = =A1, arrastrar a K3 y Ctrl+Z (no debe quedar K2 ni K3)
    ir_a(m, a, 0, 10)
    escrever(m, "=A1")
    ir_a(m, a, 0, 10)
    x0, y0 = asa(a, 0, 10)
    m.arrastar(x0 - 1, y0 - 1, *celula(a, 2, 10))
    m.tecla("ctrl-z", 0.5)
    time.sleep(0.3)
    ir_a(m, a, 0, 2)
    m.captura("H311-7-todo")


X_GRAFICO = 4 + 4 * 23 + 8 + 3 * 23 + 8 + 23 + 8 + 3 * 23 + 8 + 11    # el botón de gráficos de la barra Estándar


def region_hash(nome, x, y, w, h):
    """Los colores de una rejilla de puntos de un trozo de la captura (para ver si un gráfico cambió)."""
    from qemu import cor_do_pixel
    png = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "capturas", nome + ".png")
    return tuple(cor_do_pixel(png, x + 10 + i * (w - 20) // 5, y + 30 + j * (h - 60) // 3) for i in range(6) for j in range(4))


def asistente(m):
    m.esperar_serial('"Asistente para gráficos"', 20)
    time.sleep(0.6)
    return janela(m.serial(), "Asistente para gráficos")


def boton_asistente(d, nombre):
    x = {"atras": 278 + 42, "sig": 366 + 48, "fin": 470 + 40, "can": 556 + 38}[nombre]
    return d["x"] + x, d["y"] + 368 + 13


def log_grafico(m, marca, patron):
    r = re.findall(r"macaco: gráfico " + patron, m.serial()[marca:])
    return r[-1] if r else None


def fase_graficos(m, a):
    # ---- datos: A1:C5 (títulos y 4 filas) y, más abajo, A11:B14 (para la tarta)
    for fila, vals in enumerate((("Mes", "Ventas", "Gastos"), ("Ene", "10", "4"), ("Feb", "20", "8"),
                                 ("Mar", "30", "6"), ("Abr", "15", "12"))):
        ir_a(m, a, fila, 0)
        for v in vals:
            m.digitar(v, 0.05)
            m.tecla("tab", 0.15)
    for fila, vals in enumerate((("Parte", "Peso"), ("Uno", "5"), ("Dos", "3"), ("Tres", "2")), start=10):
        ir_a(m, a, fila, 0)
        for v in vals:
            m.digitar(v, 0.05)
            m.tecla("tab", 0.15)

    # ---- gráfico de columnas con A1:C5 por la barra Estándar
    marca = len(m.serial())
    m.arrastar(*celula(a, 0, 0), *celula(a, 4, 2))
    m.clique(a["x"] + X_GRAFICO, a["y"] + 20 + 3 + 11)
    d = asistente(m)
    m.captura("H311-8-asistente-paso1")
    m.clique(*boton_asistente(d, "sig"))
    time.sleep(0.5)
    m.captura("H311-9-asistente-paso2")
    m.clique(*boton_asistente(d, "sig"))
    time.sleep(0.5)
    m.digitar("Ventas y gastos", 0.05)
    time.sleep(0.4)
    m.captura("H311-10-asistente-paso3")
    m.clique(*boton_asistente(d, "fin"))
    m.esperar_serial("macaco: gráfico creado tipo=0 rango=A1:C5", 20)
    time.sleep(0.6)
    gx, gy = a["x"] + GRID_X + 236, a["y"] + GRID_Y          # esquina del gráfico (x=236, y=0 en la hoja)
    m.captura("H311-11-grafico-columnas")
    antes = region_hash("H311-11-grafico-columnas", gx, gy, 360, 240)

    # ---- cambiar un dato: el gráfico cambia solo
    ir_a(m, a, 1, 1)
    escrever(m, "80")
    time.sleep(0.5)
    m.captura("H311-12-grafico-datos-cambiados")
    despues = region_hash("H311-12-grafico-datos-cambiados", gx, gy, 360, 240)
    verificar(antes != despues, "el gráfico se redibuja solo al cambiar B2 (captura antes y después distintas)")

    # ---- mover: arrastrar por el cuerpo (+40, +0) -> x=276
    marca = len(m.serial())
    m.arrastar(gx + 180, gy + 120, gx + 220, gy + 120)
    r = log_grafico(m, marca, r"movido tipo=0 rango=A1:C5 en (\d+),(\d+) (\d+)x(\d+)")
    verificar(r is not None, "el gráfico se mueve arrastrándolo")

    mm = re.findall(r"macaco: gráfico movido tipo=0 rango=A1:C5 en (\d+),(\d+) (\d+)x(\d+)", m.serial()[marca:])[-1]
    nx, ny, nw, nh = [int(v) for v in mm]
    verificar(abs(nx - 276) <= 8 and ny <= 8 and nw == 360 and nh == 240, "movido a (%d,%d) %dx%d" % (nx, ny, nw, nh))
    gx += nx - 236
    ny0 = ny

    # ---- redimensionar: el asa de abajo (centro) -40 px de alto
    marca = len(m.serial())
    m.arrastar(gx + nw // 2, gy + nh, gx + nw // 2, gy + nh - 40)
    mm = re.findall(r"macaco: gráfico movido tipo=0 rango=A1:C5 en (\d+),(\d+) (\d+)x(\d+)", m.serial()[marca:])
    verificar(mm, "el asa cambia el tamaño")
    rx, ry, rw, rh = [int(v) for v in mm[-1]]
    verificar(abs(rh - 200) <= 8 and rw == 360, "redimensionado a %dx%d" % (rw, rh))
    m.captura("H311-13-grafico-movido-redimensionado")

    # ---- copiar como imagen (Ctrl+C con el gráfico seleccionado)
    marca = len(m.serial())
    m.tecla("ctrl-c", 0.6)
    m.esperar_serial("macaco: gráfico copiado como imagen", 15)
    verificar("copiado como imagen 360x%d (bien)" % rh in m.serial()[marca:], "el gráfico se copia como imagen")

    # ---- la tarta con A11:B14
    marca = len(m.serial())
    m.arrastar(*celula(a, 10, 0), *celula(a, 13, 1))
    m.clique(a["x"] + X_GRAFICO, a["y"] + 20 + 3 + 11)
    d = asistente(m)
    m.clique(d["x"] + 100, d["y"] + 64 + 4 * 20 + 10)       # «Circular (tarta)»
    time.sleep(0.4)
    m.captura("H311-14-asistente-circular")
    m.clique(*boton_asistente(d, "fin"))
    m.esperar_serial("macaco: gráfico creado tipo=4 rango=A11:B14", 20)
    time.sleep(0.6)
    # el asa del lado derecho (centro) -40: 360 -> 320 de ancho
    px, py = a["x"] + GRID_X + 164, a["y"] + GRID_Y + 10 * ROW_H
    m.arrastar(px + 360, py + 120, px + 320, py + 120)
    time.sleep(0.5)
    m.captura("H311-15-grafico-tarta")

    # ---- doble clic en el gráfico de columnas: vuelve al asistente (lo dejo como estaba con Cancelar)
    m.clique(gx + 180, gy + 100, duplo=True)
    d = asistente(m)
    m.captura("H311-16-doble-clic-asistente")
    m.clique(*boton_asistente(d, "can"))
    time.sleep(0.6)


def principal_graficos():
    pen = criar_pendrive("h311-pen", rotulo="MACACO", arquivos={"nota.txt": b"hola\n"})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    def sacar(nome):
        local = os.path.join(SAIDA, "h311-" + nome)
        mtools(pen, "mcopy", "-o", "p:/" + nome, local)
        with open(local, "rb") as f:
            return f.read()

    with Maquina("h311g", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "el pendrive está montado")
        m.comando("echo 1 > /etc/gui-revisar.modo")             # D300: el detector de problemas de la GUI, activo
        m.digitar("macaco --clasico\n", 0.05)
        m.esperar_serial("macaco: pronto", 30)
        time.sleep(1.5)
        a = janela(m.serial(), "Macaco")
        fase_graficos(m, a)

        # ---- guardar y exportar a PDF
        m.tecla("ctrl-s", 0.5)
        m.esperar_serial('"Guardar como"', 15)
        time.sleep(0.5)
        for _ in range(30):
            m.tecla("backspace", 0.03)
        m.digitar("/tmp/h311g.mxs", 0.05)
        m.tecla("ret", 0.5)
        m.esperar_serial("macaco: guardado /tmp/h311g.mxs", 20)
        m.tecla("ctrl-p", 0.6)
        m.esperar_serial('"Exportar a PDF"', 15)
        time.sleep(0.5)
        for _ in range(40):
            m.tecla("backspace", 0.03)
        m.digitar("/usb/graf.pdf", 0.05)
        m.tecla("ret", 0.6)
        m.esperar_serial("macaco: pdf exportado /usb/graf.pdf", 30)
        time.sleep(0.5)
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("macaco: cerrado", 20)
        time.sleep(0.8)

        # ---- el fichero guarda los gráficos y se reabre igual
        out = m.comando("grep -c ^grafico /tmp/h311g.mxs")
        verificar(re.search(r"^2\s*$", out, re.M) is not None, "el .mxs lleva dos líneas «grafico»")
        out = m.comando("macaco -t /tmp/h311g.mxs")
        verificar(re.search(r"grafico Hoja1 tipo=0 rango=0:0,0:4,2 x=27\d y=\d w=360 h=(19\d|20\d) .*ts=1 tc=1 ley=1 tit=\[Ventas y gastos\]", out) is not None,
                  "al recargar, el gráfico de columnas vuelve con su sitio, tamaño, rango y título")
        verificar(re.search(r"grafico Hoja1 tipo=4 rango=0:10,0:13,1 ", out) is not None, "al recargar, la tarta vuelve")
        verificar("celula Hoja1!B2 = 80" in out, "los datos siguen")

        # ---- reabrir con la GUI para ver los gráficos (captura)
        m.digitar("macaco --clasico /tmp/h311g.mxs\n", 0.05)
        m.esperar_serial("macaco: pronto", 30)
        time.sleep(1.5)
        m.captura("H311-17-reabierto")
        a = janela(m.serial(), "Macaco")
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("macaco: cerrado", 20)
        time.sleep(0.5)

        # ---- el PDF lleva los dos gráficos como imágenes
        m.comando("sync")
        pdf = sacar("graf.pdf")
        info = pdf_validar.analizar(pdf)
        verificar(info["paginas"] >= 3, "el PDF tiene la hoja y una página por gráfico (%d páginas)" % info["paginas"])
        verificar(len(info["imagenes"]) == 2, "el PDF lleva 2 imágenes de gráfico (%d)" % len(info["imagenes"]))
        verificar(all(i["ancho"] in (320, 360) for i in info["imagenes"]), "las imágenes miden 320 o 360 px de ancho")
        ext = pdf_validar.validar_externo(os.path.join(SAIDA, "h311-graf.pdf"))
        if ext and "qpdf" in ext:
            verificar(ext["qpdf"][0] in (0, 3), "qpdf --check: %s" % ext["qpdf"][1].strip().splitlines()[-1:])

        # ---- D300: el detector de problemas de la GUI no ve solapes en Macaco ni en el asistente
        gr = [l for l in m.serial().splitlines() if "[GUI-REVISAR]" in l and "detector activo" not in l and "cobertura" not in l]
        macaco_av = [l for l in gr if "macaco" in l.lower() or "Asistente para gráficos" in l]
        print("   avisos GUI-REVISAR de Macaco: %d" % len(macaco_av))
        for l in macaco_av[:12]:
            print("     " + l[:160])
        verificar(not macaco_av, "el detector de la GUI (D300) no avisa de nada en Macaco ni en el asistente de gráficos")
    return 0


def principal():
    with Maquina("h311", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.digitar("macaco --clasico\n", 0.05)
        m.esperar_serial("macaco: pronto", 30)
        time.sleep(1.5)
        a = janela(m.serial(), "Macaco")
        fase_formulas(m, a)

        # ---- guardar y cerrar; las comprobaciones se hacen sobre el fichero
        m.tecla("ctrl-s", 0.5)
        m.esperar_serial('"Guardar como"', 15)
        time.sleep(0.5)
        for _ in range(30):
            m.tecla("backspace", 0.03)
        m.digitar("/tmp/h311.mxs", 0.05)
        m.tecla("ret", 0.5)
        m.esperar_serial("macaco: guardado /tmp/h311.mxs", 20)
        m.clique(a["x"] + a["l"] - 12, a["y"] - 12)
        m.esperar_serial("macaco: cerrado", 20)
        time.sleep(0.8)
        out = m.comando("macaco -t /tmp/h311.mxs")
        for ref, val, f in (("E1", "30", "=B1+B2"), ("E2", "60", "=SUMA(B1:B3)"), ("F2", "30", "=SUMA(B1:B2)"),
                            ("G1", "10", "=B$1"), ("G2", "99", "=Hoja2!A1"),
                            ("C1", "3", "=A1*$D$4"), ("C2", "6", "=A2*$D$4"), ("C5", "15", "=A5*$D$4"),
                            ("H3", "3", None), ("H5", "5", None),
                            ("I1", "2", "=H1*2"), ("I5", "10", "=H5*2"),
                            ("J1", "101", "=A1+100"), ("J3", "103", "=A3+100"),
                            ("C7", "0", "=A7*$D$4")):
            if f:
                verificar(("celula Hoja1!%s = %s [%s]" % (ref, val, f)) in out, "%s = %s (%s)" % (ref, val, f))
            else:
                verificar(("celula Hoja1!%s = %s\n" % (ref, val)) in out + "\n", "%s = %s (serie)" % (ref, val))
        verificar("celula Hoja1!K1 =" in out, "K1 sigue ahí")
        verificar("celula Hoja1!K2 =" not in out and "celula Hoja1!K3 =" not in out,
                  "un relleno se deshace en un solo paso (no quedan K2 ni K3)")
    return 0


if __name__ == "__main__":
    sys.exit(principal() or principal_graficos())
