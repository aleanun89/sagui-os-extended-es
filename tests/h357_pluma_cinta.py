#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H357 - Pluma con la cinta y el panel del Asistente IA: nada se tapa (D355).

Pluma anota en la serie (con --cinta-log) lo que ocupa cada zona tras cada layout:
  pluma: layout base=B doc=x,y,l,a panel=x,y,l,a dibujo=x,y,l,a estado=Y ventana=LxA
Criterio, en ventana normal, maximizada, restaurada, con la cinta plegada y en 800x600:
  - el panel empieza justo donde acaba la cinta (y >= base) y acaba donde empieza la barra Dibujo;
  - el documento y el panel no se solapan, ni con la barra Dibujo ni con el estado;
  - la barra Dibujo no invade el panel (ancho = el del documento);
  - Macaco con la cinta y su panel: arranca y muestra la cinta (la h347 cubre el resto).
Capturas en docs/capturas/H357-*.png (revisadas a ojo).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402
from h337_cinta import esperar, janela, foco_terminal  # noqa: E402

PAT = re.compile(r"pluma: layout base=(\d+) doc=(\d+),(\d+),(\d+),(\d+) panel=(\d+),(\d+),(\d+),(\d+) "
                 r"dibujo=(\d+),(\d+),(\d+),(\d+) estado=(\d+) ventana=(\d+)x(\d+)")


def ultimo(m, desde):
    g = PAT.findall(m.serial()[desde:])
    verificar(bool(g), "Pluma anotó su layout")
    v = [int(x) for x in g[-1]]
    return dict(base=v[0], doc=v[1:5], panel=v[5:9], dib=v[9:13], estado=v[13], ven=(v[14], v[15]))


def solapan(a, b):
    return a[0] < b[0] + b[2] and b[0] < a[0] + a[2] and a[1] < b[1] + b[3] and b[1] < a[1] + a[3]


def comprobar(nombre, d, con_panel=True):
    doc, pan, dib = d["doc"], d["panel"], d["dib"]
    verificar(doc[1] >= d["base"], f"{nombre}: el documento empieza bajo la cinta ({doc[1]} >= {d['base']})")
    verificar(doc[1] + doc[3] <= d["estado"] - dib[3], f"{nombre}: el documento acaba sobre Dibujo/estado")
    if con_panel:
        verificar(pan[1] >= d["base"], f"{nombre}: el panel empieza bajo la cinta ({pan[1]} >= {d['base']})")
        verificar(pan[3] > 100 and pan[2] > 100, f"{nombre}: el panel tiene tamaño útil {pan[2]}x{pan[3]}")
        verificar(pan[0] + pan[2] <= d["ven"][0], f"{nombre}: el panel cabe en la ventana")
        verificar(pan[1] + pan[3] <= d["estado"] - dib[3], f"{nombre}: el panel acaba sobre Dibujo")
        verificar(not solapan(doc, pan), f"{nombre}: documento y panel no se solapan")
        verificar(doc[2] >= 150, f"{nombre}: al documento le quedan {doc[2]} px de ancho")
        verificar(not solapan(dib, pan), f"{nombre}: la barra Dibujo no pisa el panel")


PATM = re.compile(r"macaco: layout base=(\d+) hoja=(\d+),(\d+),(\d+),(\d+) formula=(\d+),(\d+),(\d+),(\d+) "
                  r"pest=(\d+),(\d+),(\d+),(\d+) panel=(\d+),(\d+),(\d+),(\d+) estado=(\d+) ventana=(\d+)x(\d+)")


def ultimo_macaco(m, desde):
    g = PATM.findall(m.serial()[desde:])
    verificar(bool(g), "Macaco anotó su layout")
    v = [int(x) for x in g[-1]]
    return dict(base=v[0], hoja=v[1:5], form=v[5:9], pest=v[9:13], panel=v[13:17], estado=v[17], ven=(v[18], v[19]))


def comprobar_macaco(nombre, d):
    pan = d["panel"]
    verificar(pan[3] > 100 and pan[2] > 100, f"{nombre}: el panel tiene tamaño útil {pan[2]}x{pan[3]}")
    verificar(pan[1] >= d["base"], f"{nombre}: el panel empieza bajo las barras ({pan[1]} >= {d['base']})")
    verificar(pan[0] + pan[2] <= d["ven"][0], f"{nombre}: el panel cabe en la ventana")
    for z in ("hoja", "form", "pest"):
        verificar(not solapan(d[z], pan), f"{nombre}: {z} {d[z]} no se solapa con el panel {pan}")
        verificar(d[z][0] + d[z][2] <= pan[0], f"{nombre}: {z} acaba a la izquierda del panel")
    verificar(d["hoja"][2] >= 150, f"{nombre}: a la hoja le quedan {d['hoja'][2]} px")


def probar(res, maximizar):
    img = imagem_de_teste("", ["--vesa", "--resolucao", res])
    L = int(res.split("x")[0])
    with Maquina("h357-" + res, "ide", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(3)
        n0 = len(m.serial())
        m.comando("pluma --cinta-log &", 20)
        esperar(m, r"pluma: pronto", 40, n0)
        time.sleep(2.5)
        w = janela(m, "Documento1 - Pluma", n0)
        k = len(m.serial())
        m.tecla("f12")
        esperar(m, r"pluma: asistente visible", 20, k)
        time.sleep(2)
        d = ultimo(m, k)
        comprobar(f"{res} con panel", d)
        m.captura(f"H357-{res}-panel")
        # la cinta plegada (doble clic en la pestaña activa) deja más sitio y el panel sube con ella
        if maximizar:
            k = len(m.serial())
            m.clique(w["xv"] + w["l"] + 2 - 35, w["yv"] + 12)
            time.sleep(2.5)
            d2 = ultimo(m, k)
            comprobar(f"{res} maximizada", d2)
            m.captura(f"H357-{res}-maximizada")
            k = len(m.serial())
            m.clique(L - 35, 36)
            time.sleep(2.5)
            comprobar(f"{res} restaurada", ultimo(m, k))
        k = len(m.serial())
        m.clique(w["x"] + 86, w["y"] + 14, duplo=True)       # pestaña Inicio
        time.sleep(1.5)
        if PAT.search(m.serial()[k:]):
            d3 = ultimo(m, k)
            comprobar(f"{res} cinta plegada", d3)
            verificar(d3["base"] < d["base"], f"{res}: plegada la cinta ocupa menos ({d3['base']} < {d['base']})")
            m.captura(f"H357-{res}-plegada")
        else:
            print("   aviso: el doble clic no plegó la cinta")
        # Macaco con el panel: con la cinta (normal y maximizada) y en modo clásico
        for modo in ("", "--clasico"):
            foco_terminal(m)
            k = len(m.serial())
            m.comando(f"macaco {modo} --cinta-log &", 20)
            esperar(m, r"macaco: pronto", 40, k)
            time.sleep(2.5)
            wm = janela(m, "Macaco - Libro1", k)
            k2 = len(m.serial())
            m.tecla("f12")
            esperar(m, r"macaco: asistente visible", 20, k2)
            time.sleep(2)
            etq = f"{res} Macaco{' ' + modo if modo else ' cinta'}"
            comprobar_macaco(etq, ultimo_macaco(m, k2))
            m.captura(f"H357-{res}-macaco{modo.replace('--', '-')}-panel")
            if maximizar:
                k2 = len(m.serial())
                m.clique(wm["xv"] + wm["l"] + 2 - 35, wm["yv"] + 12)
                time.sleep(2.5)
                comprobar_macaco(etq + " maximizada", ultimo_macaco(m, k2))
                m.captura(f"H357-{res}-macaco{modo.replace('--', '-')}-maximizada")
                m.clique(L - 35, 36)
                time.sleep(2)
            m.tecla("f12")             # cierra el panel para no estorbar a la siguiente


def main():
    probar("1024x768", True)
    probar("800x600", False)
    print("H357 OK")


if __name__ == "__main__":
    main()
