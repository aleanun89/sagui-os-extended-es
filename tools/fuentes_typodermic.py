#!/usr/bin/env python3
"""fuentes_typodermic.py - extrae las fuentes de Typodermic (CC0) para SaguiOS (D196).

Typodermic (Raymond Larabie) publica 731 fuentes OpenType (contornos CFF) en dominio
público (CC0). El zip se busca en NEW_FONTS/ y, si falta, se baja de la web de
Typodermic; en los dos casos se comprueba el SHA-256.

  tools/fuentes_typodermic.py seleccion DESTINO     la selección que va en la imagen
                                                    (/apps/fuentes) + LICENCIA-Typodermic.txt
  tools/fuentes_typodermic.py completa  DESTINO     las 731 fuentes en una carpeta
  tools/fuentes_typodermic.py completa  FICHERO.tar.gz   (o un .tar.gz listo para copiar a
                                                    /apps/fuentes/ de un disco instalado)
  tools/fuentes_typodermic.py listar                nombres de la selección

Opciones: --zip RUTA (otro zip), --sin-red (no intentar bajarlo: sin zip, sale con 3).
Sin el zip y sin red el Makefile sigue adelante sin las fuentes (código de salida 3).
"""
import argparse
import hashlib
import io
import os
import re
import sys
import tarfile
import urllib.request
import zipfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ZIP_NOMBRE = "typodermic-public-domain-2024-12.zip"
ZIP_SHA256 = "834518218043751906b9aba9d5fc30b952c8a3eaa18dca2c8fa07f7fafdf3b34"
ZIP_URL = "https://typodermicfonts.com/wp-content/uploads/2024/12/" + ZIP_NOMBRE
LICENCIA = "LICENCIA-Typodermic.txt"

# La selección (28): texto legible, retro/píxel, manuscritas y de títulos.
SELECCION = [
    # texto: serif, sans, monoespaciadas
    "Kingsbridge Rg", "Kingsbridge Rg It", "Mesmerize Rg", "Mesmerize Rg It", "Unispace Rg",
    "NK57 Monospace No Rg", "Goodfish Rg", "Gargle Rg", "Berylium Rg", "Tork Rg", "Vahika",
    "Echelon Rg", "Engebrechtre Rg", "Credit Valley", "Eden Mills Rg", "Effloresce",
    # retro / píxel
    "Minisystem", "1980 Portable", "256 Bytes", "6809 Chargen", "Color Basic", "Borg 9",
    # manuscritas y de rotulación
    "Hello Larry", "Sloe Gin Rickey",
    # títulos y display
    "Neuropol", "Xirod", "Zorque", "Typodermic",
]


def limpiar(nombre):
    """Nombre de archivo ASCII sin espacios: «Kingsbridge Rg» -> «Kingsbridge-Rg.otf»."""
    n = re.sub(r"[^A-Za-z0-9]+", "-", nombre).strip("-")
    return n + ".otf"


def sha256(ruta):
    h = hashlib.sha256()
    with open(ruta, "rb") as f:
        for b in iter(lambda: f.read(1 << 20), b""):
            h.update(b)
    return h.hexdigest()


def buscar_zip(ruta=None, red=True):
    cands = [ruta] if ruta else [os.path.join(RAIZ, "NEW_FONTS", ZIP_NOMBRE)]
    for c in cands:
        if c and os.path.exists(c):
            if sha256(c) != ZIP_SHA256:
                sys.exit("fuentes_typodermic: el SHA-256 de %s no coincide con el esperado" % c)
            return c
    if not red:
        return None
    destino = os.path.join(RAIZ, "NEW_FONTS", ZIP_NOMBRE)
    try:
        os.makedirs(os.path.dirname(destino), exist_ok=True)
        print("fuentes_typodermic: bajando %s ..." % ZIP_URL, file=sys.stderr)
        # typodermicfonts.com rechaza (406) el agente por defecto de Python/curl
        peticion = urllib.request.Request(ZIP_URL, headers={"User-Agent": "Mozilla/5.0 (X11; Linux x86_64)"})
        with urllib.request.urlopen(peticion, timeout=60) as r, open(destino + ".parte", "wb") as f:
            while True:
                bloque = r.read(1 << 20)
                if not bloque:
                    break
                f.write(bloque)
        if sha256(destino + ".parte") != ZIP_SHA256:
            os.remove(destino + ".parte")
            print("fuentes_typodermic: el SHA-256 de lo bajado no coincide", file=sys.stderr)
            return None
        os.replace(destino + ".parte", destino)
        return destino
    except Exception as e:  # sin red: se sigue sin las fuentes
        if os.path.exists(destino + ".parte"):
            os.remove(destino + ".parte")
        print("fuentes_typodermic: no se pudo bajar el zip (%s).\n"
              "  Las fuentes son opcionales: el sistema se construye igual sin ellas. Para incluirlas, baja\n"
              "  %s (CC0, de https://typodermicfonts.com/public-domain/ ;\n"
              "  SHA-256 %s) y ponlo a mano en NEW_FONTS/." % (e, ZIP_NOMBRE, ZIP_SHA256), file=sys.stderr)
        return None


def indice(z):
    """Mapa nombre sin extensión -> entrada del zip, para las .otf."""
    r = {}
    for n in z.namelist():
        if n.lower().endswith(".otf"):
            r[os.path.basename(n)[:-4]] = n
    return r


def licencia(z):
    # El License.txt viene en Windows-1252 (comillas curvas); se pasa a UTF-8 con comillas rectas
    # para que el mksgfs (UTF-8 -> Latin-1 en los .txt) no se atragante.
    t = z.read("License.txt").decode("cp1252", "replace")
    t = t.replace("\u201c", '"').replace("\u201d", '"').replace("\u2018", "'").replace("\u2019", "'")
    cab = ("Licencia de las fuentes de Typodermic (Raymond Larabie), tal cual la trae el zip original\n"
           "typodermic-public-domain-2024-12.zip (https://typodermicfonts.com). CC0 / dominio público.\n"
           "----------------------------------------------------------------------\n")
    return (cab + t).encode("utf-8")


def elegir(modo, z):
    idx = indice(z)
    if modo == "seleccion":
        faltan = [n for n in SELECCION if n not in idx]
        if faltan:
            sys.exit("fuentes_typodermic: faltan en el zip: %s" % ", ".join(faltan))
        return [(limpiar(n), idx[n]) for n in SELECCION]
    return [(limpiar(n), e) for n, e in sorted(idx.items())]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("modo", choices=["seleccion", "completa", "listar"])
    ap.add_argument("destino", nargs="?")
    ap.add_argument("--zip")
    ap.add_argument("--sin-red", action="store_true")
    a = ap.parse_args()
    if a.modo == "listar":
        for n in SELECCION:
            print(limpiar(n))
        return 0
    if not a.destino:
        ap.error("falta DESTINO")
    ruta = buscar_zip(a.zip, not a.sin_red)
    if not ruta:
        print("fuentes_typodermic: sin el zip: se sigue sin las fuentes", file=sys.stderr)
        return 3
    z = zipfile.ZipFile(ruta)
    pares = elegir(a.modo, z)
    lic = licencia(z)
    if a.destino.endswith((".tar.gz", ".tgz")):
        with tarfile.open(a.destino, "w:gz") as t:
            for nombre, ent in pares + [(LICENCIA, None)]:
                datos = lic if ent is None else z.read(ent)
                ti = tarfile.TarInfo("fuentes/" + nombre)
                ti.size = len(datos)
                ti.mtime = 0
                ti.mode = 0o644
                t.addfile(ti, io.BytesIO(datos))
    else:
        os.makedirs(a.destino, exist_ok=True)
        for nombre, ent in pares:
            with open(os.path.join(a.destino, nombre), "wb") as f:
                f.write(z.read(ent))
        with open(os.path.join(a.destino, LICENCIA), "wb") as f:
            f.write(lic)
    print("fuentes_typodermic: %d fuentes en %s" % (len(pares), a.destino))
    return 0


if __name__ == "__main__":
    sys.exit(main())
