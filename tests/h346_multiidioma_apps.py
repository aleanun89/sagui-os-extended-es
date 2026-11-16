#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H346 (D344, mejora 118, fase 2) - multiidioma: las aplicaciones gráficas principales con catálogos.

Parte 1 (anfitrión): los .po de las aplicaciones convertidas (Calculadora, Pluma con su cinta, Paint, Editor,
Visor, Música, Grabadora, Cámara, Recortes, Buscaminas, Tareas, Sobre el sistema y el Reproductor) están
completos en pt_BR y en, con los %d/%s intactos; el extractor saca cada línea de los menús (MENU_) y las
cabeceras de un programa (pluma_cinta.h) van a su catálogo.

Parte 2 (QEMU): con el idioma en inglés y en portugués de Brasil se abren varias aplicaciones convertidas y
sus títulos salen traducidos; con el pseudo-idioma «xx» salen entre corchetes y no queda nada marcado sin
traducir (ni en los menús de Pluma, que se traducen línea a línea al abrir la ventana).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, RAIZ, SAIDA  # noqa: E402
import h83_configuracion as h83  # noqa: E402
import h315_idiomas as h315  # noqa: E402

sys.path.insert(0, os.path.join(RAIZ, "tools"))
import extraer_textos as X  # noqa: E402

CATALOGOS = ["calculadora", "pluma", "paint", "editor", "visor", "musica", "grabadora", "camara", "recortes",
             "buscaminas", "tareas", "sobre-o-sistema", "reproductor"]

# programa -> (título en español, en inglés, en portugués de Brasil); solo títulos ASCII (el registro serie)
APPS = [
    ("calculadora", "Calculadora", "Calculator", "Calculadora"),
    ("paint", "Paint - sin nombre", "Paint - untitled", "Paint - sem nome"),
    ("editor", "Editor - sin nombre", "Editor - untitled", "Editor - sem nome"),
    ("sobre-o-sistema", "Acerca de SaguiOS-Extended", "About SaguiOS-Extended", "Sobre o SaguiOS-Extended"),
    ("tareas", "Administrador de tareas", "Task Manager", "Gerenciador de tarefas"),
    ("pluma", "Documento1 - Pluma", "Document1 - Pluma", "Documento1 - Pluma"),
    ("visor", "Visor de imágenes", "Image viewer", "Visualizador de imagens"),
    ("grabadora", "Grabadora", "Recorder", "Gravador"),
]


def parte1():
    cats = X.extraer_todo()
    for cat in CATALOGOS:
        verificar(cat in cats and cats[cat], f"el catálogo {cat} tiene cadenas marcadas ({len(cats.get(cat, {}))})")
        for idi in ("pt_BR", "en"):
            po = X.po_leer(os.path.join(X.DIR_IDIOMAS, idi, cat + ".po"))
            falta = [c for c in cats[cat] if not po.get(c, ("", False))[0] or po[c][1]]
            verificar(not falta, f"{idi}/{cat}.po completo ({falta[:3]})")
            mal = []
            for c in cats[cat]:
                if c in po and po[c][0]:
                    f1 = sorted(re.findall(r"%[-0-9.]*[a-zA-Z%]", c))
                    f2 = sorted(re.findall(r"%[-0-9.]*[a-zA-Z%]", po[c][0]))
                    if f1 != f2:
                        mal.append(c)
            verificar(not mal, f"{idi}/{cat}.po conserva los %d/%s ({mal[:2]})")
    # el extractor: cada línea de un menú es una cadena, sin el atajo ni los separadores
    verificar("Guardar como..." in cats["pluma"] and "Nuevo" in cats["pluma"], "MENU_: las líneas del menú de Pluma están en el catálogo")
    verificar(not any("\t" in c or c == "-" for c in cats["pluma"]), "MENU_: sin atajos ni separadores en las cadenas")
    verificar("Portapapeles" in cats["pluma"], "pluma_cinta.h va al catálogo de pluma")
    verificar(X.catalogo_de("userland/programas/pluma_cinta.h") == "pluma", "catalogo_de(pluma_cinta.h) = pluma")
    verificar(X.catalogo_de("userland/programas/arquivos.c") == "arquivos", "catalogo_de(arquivos.c) sigue igual")
    # traducciones elegidas a mano
    pt = X.po_leer(os.path.join(X.DIR_IDIOMAS, "pt_BR", "tareas.po"))
    en = X.po_leer(os.path.join(X.DIR_IDIOMAS, "en", "tareas.po"))
    verificar(pt["Administrador de tareas"][0] == "Gerenciador de tarefas", "pt_BR: Administrador de tareas")
    verificar(en["Administrador de tareas"][0] == "Task Manager", "en: Administrador de tareas")


def abrir_apps(m, idi, etiqueta, col):
    for prog, *tit in APPS:
        t = tit[col]
        k = h315.marca(m)
        m.comando(f"{prog} &")
        j = h315.esperar_ventana(m, t, k, 40)
        m.captura(f"H346-{etiqueta}-{prog}")
        verificar(j["l"] > 100, f"{etiqueta}: {prog} abre «{t}»")
        h315.foco_terminal(m)


def parte2():
    os.makedirs(SAIDA, exist_ok=True)
    img = h83.imagem_persistente("h346-persistente", True)
    with Maquina("h346", "ide", imagem=img, snapshot=False, som="ac97") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        verificar("area de trabalho" in m.serial(), "escritorio gráfico")
        for col, (idi, etq) in enumerate([("es", "es"), ("en", "en"), ("pt_BR", "pt")]):
            m.comando(f"configuracion idioma {idi}")
            time.sleep(1.2)
            if idi != "es":
                abrir_apps(m, idi, etq, col)
        faltan = h315.sin_traducir(m.serial())
        verificar(not faltan, f"en en y pt_BR no queda nada sin traducir en las apps convertidas ({faltan[:5]})")
        # pseudo-idioma: todo traducido, también los menús de Pluma
        m.comando("configuracion idioma xx")
        time.sleep(1.2)
        for prog, es, *_ in APPS:
            k = h315.marca(m)
            m.comando(f"{prog} &")
            h315.esperar_ventana(m, X.pseudo(es), k, 40)
            time.sleep(0.5)
            h315.foco_terminal(m)
        faltan = h315.sin_traducir(m.serial())
        verificar(not faltan, f"con «xx» no queda ninguna cadena marcada sin traducir ({faltan[:6]})")
        m.comando("configuracion idioma es")


if __name__ == "__main__":
    parte1()
    parte2()
    print("H346 OK")
