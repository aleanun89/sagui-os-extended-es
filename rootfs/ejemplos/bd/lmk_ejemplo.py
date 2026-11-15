# SPDX-License-Identifier: GPL-2.0-or-later
# lmk_ejemplo.py - inserta documentos de ejemplo en lite-monkey (mejora 127, D304).
#
#     servicio iniciar litemonkey            # (o: litemonkey &)
#     python /ejemplos/bd/lmk_ejemplo.py
#
# Después, en el Explorador: Archivo > Conectar a lite-monkey (127.0.0.1:7979, base «tienda»).
import basedatos

bd = basedatos.conectar("litemonkey://127.0.0.1:7979/tienda")
if bd.contar("productos") == 0:
    for p in ({"nombre": "cafe", "precio": 4.5, "tags": ["bebida", "caliente"]},
              {"nombre": "te", "precio": 3.2, "tags": ["bebida"]},
              {"nombre": "pan", "precio": 1.2}):
        bd.insertar("productos", p)
print("tienda.productos:", bd.contar("productos"), "documentos")
print("campos:", [(c["nombre"], c["tipo"]) for c in bd.esquema("productos")])
bd.cerrar()
