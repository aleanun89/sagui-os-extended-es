#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""paquete_instalar.py - instala un «paquete SaguiOS» (D375) en una carpeta que luego se vuelca en /apps.

Un paquete es un tar con sagui-paquete.json (manifiesto), LICENSE y una carpeta apps/ cuyo contenido va
tal cual a /apps (bin/<ejecutable>, <app>/ con los datos). Lo usan las aplicaciones con licencia
incompatible con la GPL v3, que viven en repositorios aparte (OpenTTD, y a futuro NetSurf, GNUBoy...).
No se meten nunca en las imágenes públicas: se instalan en una imagen privada de prueba o, desde una
carpeta de /apps ya montada, a mano.

  tools/paquete_instalar.py PAQUETE.tar DESTINO          extrae apps/ en DESTINO (que es la raíz de /apps)
  tools/paquete_instalar.py --info PAQUETE.tar           muestra el manifiesto resumido
  tools/paquete_instalar.py --input-share PAQUETE.tar    lo mismo con DESTINO = input_share/apps (el make lo
                                                         vuelca sobre /apps; input_share está fuera de git)

Comprueba que el manifiesto es de formato 1, que no hay rutas fuera de apps/ ni «..», y la suma SHA-256
y el tamaño de cada fichero del manifiesto antes de extraer nada.
"""
import hashlib
import json
import os
import sys
import tarfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def leer(paquete):
    t = tarfile.open(paquete)
    man = json.loads(t.extractfile("sagui-paquete.json").read().decode("utf-8"))
    if man.get("formato") != 1:
        sys.exit("paquete_instalar: formato de paquete no soportado: %r" % man.get("formato"))
    return t, man


def comprobar(t, man):
    for f in man["ficheros"]:
        r = f["ruta"]
        if r.startswith("/") or ".." in r.split("/"):
            sys.exit("paquete_instalar: ruta peligrosa en el manifiesto: " + r)
        d = t.extractfile(r).read()
        if len(d) != f["bytes"] or hashlib.sha256(d).hexdigest() != f["sha256"]:
            sys.exit("paquete_instalar: la suma de %s no coincide con el manifiesto" % r)
    miembros = {m.name for m in t.getmembers() if m.isfile()}
    extra = miembros - {f["ruta"] for f in man["ficheros"]} - {"sagui-paquete.json"}
    if extra:
        sys.exit("paquete_instalar: ficheros que el manifiesto no lista: " + ", ".join(sorted(extra)))


def instalar(paquete, destino):
    t, man = leer(paquete)
    comprobar(t, man)
    n = 0
    for f in man["ficheros"]:
        r = f["ruta"]
        if not r.startswith("apps/"):
            continue
        d = os.path.join(destino, r[len("apps/"):])
        os.makedirs(os.path.dirname(d), exist_ok=True)
        with open(d, "wb") as g:
            g.write(t.extractfile(r).read())
        os.chmod(d, 0o755 if r.startswith("apps/bin/") else 0o644)
        n += 1
    print("paquete %s %s (%s): %d ficheros en %s" % (man["nombre"], man["version"], man["licencia"], n, destino))
    return man


def main():
    a = sys.argv[1:]
    if len(a) == 2 and a[0] == "--info":
        _, man = leer(a[1])
        print(json.dumps({k: v for k, v in man.items() if k != "ficheros"}, indent=1, ensure_ascii=False))
        print("ficheros:", len(man["ficheros"]))
    elif len(a) == 2 and a[0] == "--input-share":
        instalar(a[1], os.path.join(RAIZ, "input_share", "apps"))
    elif len(a) == 2 and not a[0].startswith("-"):
        instalar(a[0], a[1])
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    main()
