#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Genera rootfs/ejemplos/vps/<ejemplo>/ (formulario.vpf, codigo.py y lanzador.py) a partir de las
plantillas de VisualPythonS (rootfs/lib/python/vps/plantillas.py, mejora 123, D297).

Se ejecuta con el Python del anfitrión (hace falta su tkinter solo para importar el módulo vps);
el resultado se versiona, en Latin-1 como los archivos del SaguiOS. Uso: python3 tools/vps_ejemplos.py"""
import os
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(RAIZ, "rootfs", "lib", "python"))   # al final: no tapar el tkinter del anfitrión
import vps  # noqa: E402
from vps import plantillas, proyecto  # noqa: E402

DESTINO = os.path.join(RAIZ, "rootfs", "ejemplos", "vps")
LANZADOR = '''# Lanzador de VisualPythonS (D297): python lanzador.py abre esta aplicación sin el diseñador.
import sys
from vps import proyecto

carpeta = sys.argv[0].rpartition("/")[0] or "."
proyecto.lanzar(carpeta)
'''

NOMBRES = {"Hola mundo": "hola-mundo", "Calculadora sencilla": "calculadora", "Lista de tareas": "lista-de-tareas",
           "Cronómetro": "cronometro", "Visor de imágenes": "visor-de-imagenes",
           "Formularios modales": "formularios-modales", "Validar una entrada": "validar-entrada",
           "Abrir y guardar": "abrir-guardar", "Pestañas": "pestanas", "Tabla": "tabla", "Numérico": "numerico",
           "Lienzo": "lienzo", "Enlace": "enlace", "Barra de estado": "barra-estado",
           "Selector de color": "selector-color", "Reloj": "reloj", "Archivo": "archivo"}


def main():
    for titulo, fabrica in plantillas.PLANTILLAS:
        if titulo not in NOMBRES:
            continue
        forms, codigo = fabrica()
        d = os.path.join(DESTINO, NOMBRES[titulo])
        os.makedirs(d, exist_ok=True)
        with open(os.path.join(d, "formulario.vpf"), "w", encoding="latin-1") as f:
            f.write(proyecto.escribir_vpf(forms))
        # Sin línea SPDX a propósito: es código para el usuario y las pruebas (h299, h303) y el manual
        # lo leen por números de línea (tools/spdx.py y auditar_licencias.py los excluyen).
        with open(os.path.join(d, "codigo.py"), "w", encoding="latin-1") as f:
            f.write(codigo)
        with open(os.path.join(d, "lanzador.py"), "w", encoding="latin-1") as f:
            f.write(LANZADOR)
        print("ejemplo:", d)


if __name__ == "__main__":
    main()
