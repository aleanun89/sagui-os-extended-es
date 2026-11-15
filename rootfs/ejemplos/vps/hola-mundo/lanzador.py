# SPDX-License-Identifier: GPL-2.0-or-later
# Lanzador de VisualPythonS (D297): python lanzador.py abre esta aplicación sin el diseñador.
import sys
from vps import proyecto

carpeta = sys.argv[0].rpartition("/")[0] or "."
proyecto.lanzar(carpeta)
