#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Hace la tabla de docs/cipo-compatibilidad.md (D271) con dos resultados de
tests/h271_cipo_css.py: el de antes (tests/web/resultado-antes.txt, del motor
de la v8.0b8) y el de ahora (build/.../h271-resultado.txt).

    python3 tests/web/inventario.py antes.txt ahora.txt > tabla.md
"""
import sys

TEMAS = {
    "01": "Variables CSS y calc()", "02": "Selectores: atributos y combinadores", "03": "Pseudoclases",
    "04": "Pseudoclases de formulario y pseudoelementos", "05": "@media, @supports, @import, @layer", "06": "Unidades",
    "07": "Flexbox", "08": "Flexbox (2)", "09": "Grid", "10": "Posición y modelo de caja", "11": "Caja, visibilidad y transformaciones",
    "12": "Colores, bordes y sombras", "13": "Fondos con imagen y object-fit", "14": "Texto", "15": "Elementos HTML5",
    "16": "Medios y otros elementos", "17": "Formularios", "18": "Formularios (2)", "19": "JavaScript: DOM", "20": "JavaScript: API de la ventana",
}


def leer(camino):
    r = {}
    with open(camino, encoding="utf-8") as f:
        for linea in f:
            p = linea.rstrip("\n").split("\t")
            if len(p) == 3:
                r[p[0]] = (p[1] == "ok", p[2])
    return r


def main():
    antes, ahora = leer(sys.argv[1]), leer(sys.argv[2])
    pags = sorted({k.split(":")[0] for k in ahora})
    print("| Página | Tema | Antes | Después | Filas |")
    print("| --- | --- | --- | --- | --- |")
    ta = td = tn = 0
    for pag in pags:
        ks = [k for k in ahora if k.split(":")[0] == pag]
        a = sum(1 for k in ks if antes.get(k, (False,))[0])
        d = sum(1 for k in ks if ahora[k][0])
        ta, td, tn = ta + a, td + d, tn + len(ks)
        print("| `%s.html` | %s | %d | %d | %d |" % (pag, TEMAS.get(pag[:2], ""), a, d, len(ks)))
    print("| | **Total** | **%d** | **%d** | **%d** |" % (ta, td, tn))
    print()
    for pag in pags:
        print("### %s — %s" % (pag, TEMAS.get(pag[:2], "")))
        print()
        print("| Fila | Función | Antes | Después |")
        print("| --- | --- | --- | --- |")
        ks = sorted((k for k in ahora if k.split(":")[0] == pag), key=lambda k: int(k.split(":")[1]))
        for k in ks:
            a = "sí" if antes.get(k, (False,))[0] else "no"
            d = "sí" if ahora[k][0] else "no"
            if a == "no" and d == "sí":
                d = "**sí**"
            rot = ahora[k][1].replace("|", "\\|").replace("<", "&lt;").replace(">", "&gt;")
            print("| %s | %s | %s | %s |" % (k.split(":")[1], rot, a, d))
        print()


if __name__ == "__main__":
    main()
