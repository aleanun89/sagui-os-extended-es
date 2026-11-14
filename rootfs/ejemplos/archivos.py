# SPDX-License-Identifier: GPL-2.0-or-later
# archivos.py - archivos, carpetas y texto con acentos desde MicroPython.
# Ejecuta con: python /ejemplos/archivos.py
import os
import json

carpeta = "/tmp/notas"
if not os.path.isdir(carpeta):
    os.mkdir(carpeta)

# Escribir: en modo texto se guarda en Latin-1, como el Editor de SaguiOS.
ruta = os.path.join(carpeta, "compras.txt")
with open(ruta, "w") as f:
    for cosa in ("plátanos", "piñas", "café"):
        f.write("- " + cosa + "\n")

# Añadir al final
with open(ruta, "a") as f:
    print("- azúcar", file=f)

# Leer línea a línea
with open(ruta) as f:
    for n, linea in enumerate(f, 1):
        print(n, linea.rstrip())

# Datos estructurados con json
datos = {"nombre": "Tito", "frutas": ["plátano", "mango"], "edad": 3}
with open(os.path.join(carpeta, "sagui.json"), "w") as f:
    json.dump(datos, f)
with open(os.path.join(carpeta, "sagui.json")) as f:
    leido = json.load(f)
print("json:", leido["nombre"], leido["frutas"][0])

# Binario
with open(os.path.join(carpeta, "bytes.bin"), "wb") as f:
    f.write(bytes(range(16)))
print("binario:", open(os.path.join(carpeta, "bytes.bin"), "rb").read().hex())

# La carpeta
for nombre in sorted(os.listdir(carpeta)):
    st = os.stat(os.path.join(carpeta, nombre))
    print("%-12s %5d bytes" % (nombre, st.st_size))
print("Abre", ruta, "con el Editor: los acentos se ven bien.")
