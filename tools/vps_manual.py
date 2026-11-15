#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Mantiene docs/visualpythons-manual.md igual que los ejemplos de /ejemplos/vps/ (D303).

El manual lleva, entre marcas, el código y los controles de cada ejemplo:

    <!-- ejemplo:calculadora -->  ...el codigo.py del ejemplo...  <!-- /ejemplo -->
    <!-- controles:calculadora -->  ...la tabla de controles del formulario.vpf...  <!-- /controles -->

Uso:
    python3 tools/vps_manual.py              reescribe esos bloques desde rootfs/ejemplos/vps/ y
                                             genera la copia de la ayuda del sistema
                                             (rootfs/ayuda/visualpythons-manual.md, sin enlaces a capturas)
    python3 tools/vps_manual.py --comprobar  no escribe; sale con 1 si el manual no coincide con los
                                             ejemplos o si falta alguna captura enlazada
"""
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(RAIZ, "rootfs", "lib", "python"))   # al final: no tapar el tkinter del anfitrión
import vps  # noqa: E402
from vps import proyecto  # noqa: E402

MANUAL = os.path.join(RAIZ, "docs", "visualpythons-manual.md")
AYUDA = os.path.join(RAIZ, "rootfs", "ayuda", "visualpythons-manual.md")
EJEMPLOS = os.path.join(RAIZ, "rootfs", "ejemplos", "vps")


def leer_latin1(ruta):
    with open(ruta, "rb") as f:
        return f.read().decode("latin-1")


def bloque_codigo(nombre):
    return "```python\n" + leer_latin1(os.path.join(EJEMPLOS, nombre, "codigo.py")).rstrip("\n") + "\n```"


def _val(v):
    if isinstance(v, str):
        return '"%s"' % v
    if isinstance(v, int) and not isinstance(v, bool) and v > 255:
        return "0x%06X" % v
    return str(v)


def tabla_controles(nombre):
    forms = proyecto.leer_vpf(leer_latin1(os.path.join(EJEMPLOS, nombre, "formulario.vpf")))
    filas = ["| Control | Nombre | x, y, ancho, alto | Propiedades |", "| --- | --- | --- | --- |"]
    for f in forms:
        p = f["props"]
        extra = ", ".join("`%s = %s`" % (k, _val(v)) for k, v in p.items())
        filas.append("| Formulario | `%s` | %s x %s | %s |" % (f["nombre"], p.get("ancho", 320), p.get("alto", 200), extra))
        for c in f["controles"]:
            cls = vps.TIPOS[c["tipo"]]
            q = c["props"]
            tam = cls.TAM
            pos = "%s, %s, %s, %s" % (q.get("x", 0), q.get("y", 0), q.get("ancho", tam[0]), q.get("alto", tam[1]))
            if c["tipo"] in ("Temporizador", "Menu", "Reloj", "Archivo"):
                pos = "(sin ventana)"
            extra = ", ".join("`%s = %s`" % (k, _val(v)) for k, v in q.items() if k not in ("x", "y", "ancho", "alto"))
            if c["padre"]:
                extra = ("dentro de `%s`; " % c["padre"]) + extra
            filas.append("| %s | `%s` | %s | %s |" % (c["tipo"], c["nombre"], pos, extra))
    return "\n".join(filas)


def reescribir(texto):
    def uno(m):
        return "<!-- ejemplo:%s -->\n%s\n<!-- /ejemplo -->" % (m.group(1), bloque_codigo(m.group(1)))

    def dos(m):
        return "<!-- controles:%s -->\n%s\n<!-- /controles -->" % (m.group(1), tabla_controles(m.group(1)))
    texto = re.sub(r"<!-- ejemplo:([a-z0-9-]+) -->\n.*?\n<!-- /ejemplo -->", uno, texto, flags=re.S)
    texto = re.sub(r"<!-- controles:([a-z0-9-]+) -->\n.*?\n<!-- /controles -->", dos, texto, flags=re.S)
    return texto


def capturas_enlazadas(texto):
    return re.findall(r"!\[[^\]]*\]\((\.\./capturas/[^)]+|capturas/[^)]+)\)", texto)


def para_ayuda(texto):
    """La copia de la ayuda del sistema: las capturas no van en la imagen de SaguiOS."""
    texto = re.sub(r"!\[([^\]]*)\]\([^)]*\)", lambda m: "*[captura: %s]*" % m.group(1), texto)
    return texto


def main():
    comprobar = "--comprobar" in sys.argv
    with open(MANUAL, encoding="utf-8") as f:
        actual = f.read()
    nuevo = reescribir(actual)
    errores = []
    if nuevo != actual:
        errores.append("el manual no coincide con los ejemplos de rootfs/ejemplos/vps/ (ejecuta tools/vps_manual.py)")
    for ruta in capturas_enlazadas(nuevo):
        if not os.path.exists(os.path.join(RAIZ, "docs", ruta.replace("../", "", 1) if ruta.startswith("../") else ruta)):
            errores.append("falta la captura " + ruta)
    if comprobar:
        for e in errores:
            print("vps_manual: " + e)
        print("vps_manual: %s" % ("FALLO" if errores else "ok"))
        sys.exit(1 if errores else 0)
    with open(MANUAL, "w", encoding="utf-8") as f:
        f.write(nuevo)
    with open(AYUDA, "w", encoding="utf-8") as f:
        f.write(para_ayuda(nuevo))
    for e in errores:
        if e.startswith("falta"):
            print("vps_manual: " + e)
    print("vps_manual: manual y ayuda al día")


if __name__ == "__main__":
    main()
