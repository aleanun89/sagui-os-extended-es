# SPDX-License-Identifier: GPL-3.0-only
"""Ayudas comunes de h335 (Explorador SQLite) y h336 (Explorador lite-monkey): la sonda de F12 de los exploradores
(D333), con las posiciones de nodos, botones, rejillas, cajas y líneas, y los diálogos con su propia sonda."""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import SAIDA, RAIZ, BUILD, opcoes_apps  # noqa: E402
from h304_bd_visual import janela, limpio, esperar, cerrar  # noqa: E402,F401

CONV = {"nodo": (2, 3), "boton": (2, 3), "cab": (2, 3), "celda": (3, 4), "texto": (1, 2), "arbol": (1, 2),
        "caja": (2, 3), "linea": (3, 4), "entrada": (2, 3), "casilla": (2, 3)}


def imagen_con_home(nombre):
    """La imagen normal con un disco de datos que se monta en /home (el servicio lite-monkey guarda ahí sus datos)."""
    sgfs = os.path.join(SAIDA, nombre + ".sgfs")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mksgfs.py"), os.path.join(BUILD, "rootfs"), sgfs],
                   check=True, stdout=subprocess.DEVNULL)
    img = os.path.join(SAIDA, nombre + ".img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(BUILD, "boot", "mbr.bin"), "--estagio2", os.path.join(BUILD, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(BUILD, "kernel.bin"), "--kernel-elf", os.path.join(BUILD, "kernel.elf"),
                    "--ramdisk", sgfs, "--saida", img, "--vesa", "--dados-mb", "32", *opcoes_apps()],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def sonda(m, w, tempo=20):
    """F12 en la ventana `w`: {"nodo": [...], "boton": [[texto, x, y]], "caja": {nombre: (x, y)}, "linea": {lista: [(k, x, y, texto)]},
    "rejilla": {nombre: {(i, j): texto}}, "cab": {rejilla: {columna: (x, y)}}, "entrada": [...], "casilla": [...]} con las posiciones
    ya en pantalla (la sonda las da relativas al contenido de la ventana)."""
    marca = len(m.serial())
    m.tecla("f12", 0.8)
    esperar(m, "sonda: fin", marca, tempo)
    s = limpio(m.serial()[marca:])
    s = s[s.rfind("vps-sonda: inicio"):]
    r = {"nodo": [], "boton": [], "caja": {}, "linea": {}, "rejilla": {}, "cab": {}, "celda": {}, "entrada": [], "casilla": [],
         "texto": []}
    rej = None
    # La serie parte a veces una línea de la sonda (otra salida del kernel se intercala): se unen los
    # fragmentos que no empiezan por «sonda: » a la línea de sonda anterior, hasta «sonda: fin».
    unidas = []
    for linea in s.splitlines():
        linea = linea.rstrip("\r")
        if linea.startswith("sonda: ") or linea.startswith("vps-sonda"):
            unidas.append(linea)
        elif unidas and unidas[-1].startswith("sonda: ") and not unidas[-1].startswith("sonda: fin"):
            unidas[-1] += linea
    for linea in unidas:
        if not linea.startswith("sonda: ") or linea.startswith("sonda: fin"):
            continue
        p = linea[len("sonda: "):].rstrip("\r").split("|")
        k = p[0]
        pos = CONV.get(k)
        if pos:
            p[pos[0]] = str(int(p[pos[0]]) + w["x"])
            p[pos[1]] = str(int(p[pos[1]]) + w["y"])
        if k.startswith("rejilla"):
            rej = k
            r["rejilla"][k] = {}
            r["cab"][k] = {}
            r["celda"][k] = {}
        elif k == "celda":
            r["rejilla"][rej][(int(p[1]), int(p[2]))] = p[5] if len(p) > 5 else ""
            r["celda"][rej][(int(p[1]), int(p[2]))] = (int(p[3]), int(p[4]))
        elif k == "cab":
            r["cab"][rej][p[1]] = (int(p[2]), int(p[3]))
        elif k == "caja":
            r["caja"][p[1]] = (int(p[2]), int(p[3]))
        elif k == "linea":
            r["linea"].setdefault(p[1], []).append((int(p[2]), int(p[3]), int(p[4]), p[5] if len(p) > 5 else ""))
        elif k == "nodo":
            r["nodo"].append((int(p[1]), int(p[2]), int(p[3]), p[4] if len(p) > 4 else ""))
        elif k == "boton":
            r["boton"].append((p[1], int(p[2]), int(p[3])))
        elif k in ("entrada", "casilla"):
            r[k].append((int(p[1]), int(p[2]), int(p[3]), p[4] if len(p) > 4 else ""))
        elif k == "texto":
            r["texto"].append((int(p[1]), int(p[2])))
    return r


def boton(s, texto):
    for t, x, y in s["boton"]:
        if t == texto:
            return x, y
    raise AssertionError("la sonda no tiene el botón %r: %s" % (texto, [b[0] for b in s["boton"]]))


def nodo(s, texto):
    for _k, x, y, t in s["nodo"]:
        if texto in t:
            return x, y
    raise AssertionError("la sonda no tiene el nodo %r: %s" % (texto, [n[3] for n in s["nodo"]]))


def linea(s, lista, texto):
    for _k, x, y, t in s["linea"].get(lista, []):
        if texto in t:
            return x, y
    raise AssertionError("la sonda no tiene la línea %r en %s: %s" % (texto, lista, [t for _k, _x, _y, t in s["linea"].get(lista, [])]))


def caja(s, nombre):
    if nombre not in s["caja"]:
        raise AssertionError("la sonda no tiene la caja %r: %s" % (nombre, list(s["caja"])))
    return s["caja"][nombre]


def celda_txt(s, rejilla, i, j):
    return s["rejilla"][rejilla][(i, j)]


def escribir_en(m, pos, texto, borrar=True, espera=0.06):
    """Clic en una caja de texto, selecciona todo y escribe."""
    m.clique(*pos)
    time.sleep(0.3)
    if borrar:
        m.tecla("ctrl-a", 0.1)
    m.digitar(texto, espera)


def al_terminal(m):
    """Alt+Tab: lleva el foco entre la ventana del explorador y el terminal (hay solo esas dos)."""
    m.tecla("alt-tab", 0.8)


def python_c(m, codigo, tempo=60, alt_tab=True):
    """`python -c` en el terminal (con Alt+Tab si el explorador tiene el foco); si no vuelve al prompt, imprime el final de la
    serial para depurar."""
    if alt_tab:
        al_terminal(m)
    try:
        out = m.comando("python -c \"%s\"" % codigo, tempo)
    except Exception:
        print(m.serial()[-1800:])
        m.captura("bd-fallo-python", guardar=False)
        raise
    if alt_tab:
        al_terminal(m)
    return out


def comando(m, texto, tempo=60, alt_tab=True):
    """Una orden en el terminal, con Alt+Tab de ida y vuelta."""
    if alt_tab:
        al_terminal(m)
    out = m.comando(texto, tempo)
    if alt_tab:
        al_terminal(m)
    return out
