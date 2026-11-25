#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Mantiene rootfs/ayuda/visualpythons.md igual que docs/visualpythons.md (la referencia corta de VisualPythonS).

La copia de la ayuda del sistema es la misma referencia, salvo lo que solo tiene sentido en el repositorio
(ADAPTACIONES: texto que se quita al generar la copia de /ayuda). No se edita la copia a mano: se cambia
docs/visualpythons.md y se vuelve a generar.

Uso:
    python3 tools/vps_referencia.py              escribe rootfs/ayuda/visualpythons.md
    python3 tools/vps_referencia.py --comprobar  no escribe; sale con 1 si la copia no coincide con la referencia
                                                 (o si una adaptación ya no encuentra su texto)

No usa tkinter ni el módulo sagui: solo la biblioteca estándar.
"""
import os
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORIGEN = os.path.join(RAIZ, "docs", "visualpythons.md")
DESTINO = os.path.join(RAIZ, "rootfs", "ayuda", "visualpythons.md")

# (texto del repositorio, texto de la ayuda del sistema). Cada texto debe aparecer exactamente una vez.
ADAPTACIONES = [
    # Las pruebas son ficheros del repositorio: no van a la imagen.
    (", pruebas `tests/h297_visualpython.py`, `tests/h299_vps_avanzado.py` y\n  `tests/h303_vps_manual.py`", ""),
]
# En la copia de /ayuda no puede quedar ninguna ruta del repositorio de pruebas o herramientas.
PROHIBIDOS_EN_AYUDA = ("tests/", "tools/")


def leer(ruta):
    with open(ruta, encoding="utf-8", newline="") as f:
        return f.read()


def para_ayuda(texto):
    for antes, despues in ADAPTACIONES:
        if texto.count(antes) != 1:
            print("vps_referencia: la adaptación no aparece exactamente una vez: %r" % antes[:60])
            sys.exit(1)
        texto = texto.replace(antes, despues)
    for prohibido in PROHIBIDOS_EN_AYUDA:
        if prohibido in texto:
            print("vps_referencia: queda una ruta del repositorio (%s) en la copia de la ayuda; añade una adaptación" % prohibido)
            sys.exit(1)
    return texto


def main():
    nuevo = para_ayuda(leer(ORIGEN))
    if "--comprobar" in sys.argv:
        actual = leer(DESTINO) if os.path.exists(DESTINO) else None
        if actual != nuevo:
            print("vps_referencia: rootfs/ayuda/visualpythons.md no coincide con docs/visualpythons.md (ejecuta tools/vps_referencia.py)")
            print("vps_referencia: FALLO")
            sys.exit(1)
        print("vps_referencia: ok")
        sys.exit(0)
    with open(DESTINO, "w", encoding="utf-8", newline="") as f:
        f.write(nuevo)
    print("vps_referencia: referencia de la ayuda al día")


if __name__ == "__main__":
    main()
