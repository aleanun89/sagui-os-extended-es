# SPDX-License-Identifier: GPL-2.0-or-later
# grafico.py - un gráfico de la tabla en una ventana (D479). Se cierra con el botón de cerrar.
# Ejecuta con: python /ejemplos/datos/grafico.py
from tablas import leer_csv

v = leer_csv("/ejemplos/datos/ventas.csv").rellenar({"unidades": 0})
por_region = v.groupby("region").agg({"unidades": "suma"})
por_region.grafico("barras", x="region", y="unidades", titulo="Unidades por región", ventana=True)
