# SPDX-License-Identifier: GPL-2.0-or-later
# ventas.py - el módulo «tablas» (DataFrame ligero) con un CSV de ejemplo (D479).
# Ejecuta con: python /ejemplos/datos/ventas.py
from tablas import leer_csv

v = leer_csv("/ejemplos/datos/ventas.csv")
print(v.cabeza())
print("filas y columnas:", v.forma, " faltan:", v.contar_nulos())

v = v.rellenar({"unidades": "media", "precio": 2.5})
v["total"] = lambda f: f["unidades"] * f["precio"]

print("\nTotal por región:")
print(v.groupby("region").agg({"total": "suma", "unidades": "media"}).ordenar("total", descendente=True))

print("\nVentas por región y mes:")
print(v.pivot("region", "mes", "total", agg="suma"))

print("\nCon la categoría del producto:")
p = leer_csv("/ejemplos/datos/productos.csv")
print(v.merge(p, en="producto").groupby("categoria").agg({"total": "suma"}))

v.a_csv("/tmp/ventas_con_total.csv")
v.a_sqlite("/tmp/ventas.db", "ventas")
v.a_json("/tmp/ventas.json")
print("\nGuardado en /tmp: ventas_con_total.csv, ventas.db (SQLite) y ventas.json.")
print("Para Excel o Macaco: v.a_xlsx('/tmp/ventas.xlsx') o v.a_mxs('/tmp/ventas.mxs').")
