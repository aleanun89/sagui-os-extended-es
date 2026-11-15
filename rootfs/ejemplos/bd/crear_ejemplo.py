# SPDX-License-Identifier: GPL-2.0-or-later
# crear_ejemplo.py - crea una base SQLite de ejemplo con datos (mejora 127, D304).
#
#     python /ejemplos/bd/crear_ejemplo.py [/home/ejemplo.db]
#
# Dos tablas: «contactos» (60 filas) y «productos» (con un índice). Después se abre con el
# Explorador de bases de datos (Inicio > Accesorios) o se consulta desde Python:
#     import basedatos
#     bd = basedatos.conectar("sqlite:/home/ejemplo.db")
#     print(bd.consultar("contactos", {"ciudad": "Lima"}, orden="nombre", limite=5))
import os
import sys
import basedatos

ruta = sys.argv[1] if len(sys.argv) > 1 else ("/home/ejemplo.db" if os.path.isdir("/home") else "/tmp/ejemplo.db")
try:
    os.remove(ruta)
except OSError:
    pass

bd = basedatos.conectar("sqlite:" + ruta)
bd.ejecutar("CREATE TABLE contactos (id INTEGER PRIMARY KEY, nombre TEXT NOT NULL, ciudad TEXT, edad INTEGER)")
bd.ejecutar("CREATE TABLE productos (codigo TEXT PRIMARY KEY, descripcion TEXT, precio REAL, existencias INTEGER)")
bd.ejecutar("CREATE INDEX i_productos_precio ON productos(precio)")
ciudades = ["Madrid", "Sevilla", "Lima", "Bogota"]
nombres = ["Ana", "Beto", "Carla", "Dani", "Eva", "Fran", "Gema", "Hugo", "Irene", "Jorge", "Karla", "Luis", "Marta", "Nico", "Olga"]
with bd.transaccion():
    for i in range(60):
        bd.insertar("contactos", {"nombre": "%s %d" % (nombres[i % 15], i // 15 + 1), "ciudad": ciudades[i % 4], "edad": 20 + (i * 7) % 45})
    for cod, desc, precio, n in (("CAF", "Cafe molido 250 g", 4.5, 40), ("TEA", "Te verde 100 g", 3.2, 25),
                                 ("PAN", "Pan de molde", 1.2, 60), ("LEC", "Leche entera 1 l", 0.95, 120)):
        bd.insertar("productos", {"codigo": cod, "descripcion": desc, "precio": precio, "existencias": n})
bd.cerrar()
print("creada %s: contactos (60 filas) y productos (4 filas)" % ruta)
