#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""sonidos_privados.py - los esquemas de sonido PRIVADOS (D421), solo para quien compila con INPUT_SHARE.

Lee input_share/sounds/ (only_private/*.wav: un jingle de arranque por versión de Windows; pixbay_free/*.mp3)
y crea en DESTINO una carpeta por esquema (esquema.conf, CREDITOS.txt y los archivos) para /apps/sonidos.
NUNCA va a git ni a las imágenes públicas: gerar_versao.sh, «make variantes» y la batería compilan con
INPUT_SHARE vacío, y tools/publicar-excluir.txt y publicar.sh rechazan estas rutas.

Uso: sonidos_privados.py INPUT_SHARE/sounds DESTINO
"""
import os
import shutil
import sys

# archivo -> (id, nombre, evento -> ese archivo)
WINDOWS = {
    "Windows_95.wav": ("windows95", "Windows 95"),
    "Windows_98.wav": ("windows98", "Windows 98"),
    "Windows_ME_2000.wav": ("windowsme", "Windows ME/2000"),
    "Windows_XP.wav": ("windowsxp", "Windows XP"),
    "Windows_Vista_7_8_10.wav": ("windows7", "Windows Vista/7/8/10"),
    "Windows_11.wav": ("windows11", "Windows 11"),
}
PIXABAY = {  # evento -> archivo
    "aviso": "alexzavesa-cartoon-2-468376.mp3",
    "error": "alexzavesa-cartoon-7-468374.mp3",
    "pregunta": "universfield-funny-rimshot-250961.mp3",
    "papelera": "universfield-funny-rimshot-250961.mp3",
    "usb_conectar": "alexzavesa-cartoon-2-468376.mp3",
    "bateria": "alexzavesa-cartoon-7-468374.mp3",
}


def escribir(d, nombre, creditos, mapa, texto_creditos):
    with open(os.path.join(d, "esquema.conf"), "w", encoding="utf-8") as f:
        f.write(f"# esquema PRIVADO (D421): nunca en git ni en imágenes públicas\nnombre={nombre}\ncreditos={creditos}\n")
        for ev, arq in mapa.items():
            f.write(f"{ev}={arq}\n")
    with open(os.path.join(d, "CREDITOS.txt"), "w", encoding="utf-8") as f:
        f.write(texto_creditos)


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 1
    orig, dest = sys.argv[1:]
    priv = os.path.join(orig, "only_private")
    n = 0
    if os.path.isdir(priv):
        for arq, (ident, nombre) in WINDOWS.items():
            p = os.path.join(priv, arq)
            if not os.path.isfile(p):
                continue
            d = os.path.join(dest, ident)
            os.makedirs(d, exist_ok=True)
            shutil.copyfile(p, os.path.join(d, "arranque.wav"))
            escribir(d, nombre, "Sonido de arranque de Microsoft: uso privado, no se distribuye",
                     {"arranque": "arranque.wav"},
                     f"Esquema «{nombre}»: el sonido de arranque de Windows, aportado por quien compila (input_share/sounds/only_private).\n"
                     "Material de Microsoft: PRIVADO, solo para esta imagen; no se publica ni se redistribuye.\n"
                     "Solo suena el evento «arranque» (no hay más archivos).\n")
            n += 1
    pix = os.path.join(orig, "pixbay_free")
    if os.path.isdir(pix) and all(os.path.isfile(os.path.join(pix, a)) for a in set(PIXABAY.values())):
        d = os.path.join(dest, "pixabay")
        os.makedirs(d, exist_ok=True)
        # Nombres cortos: el SGFS del ramdisk no admite rutas de más de 55 bytes (tools/mksgfs.py) y los de Pixabay son largos.
        cortos = {a: f"p{i}{os.path.splitext(a)[1]}" for i, a in enumerate(sorted(set(PIXABAY.values())), 1)}
        for a, c in cortos.items():
            shutil.copyfile(os.path.join(pix, a), os.path.join(d, c))
        escribir(d, "Pixabay", "Sonidos de Pixabay (alexzavesa, universfield): uso privado",
                 {ev: cortos[a] for ev, a in PIXABAY.items()},
                 "Esquema «Pixabay»: sonidos de pixabay.com bajo la Licencia de Contenido de Pixabay.\n"
                 "  alexzavesa: cartoon-2 y cartoon-7 (https://pixabay.com/users/alexzavesa/)\n"
                 "  universfield: funny-rimshot (https://pixabay.com/users/universfield/)\n"
                 "La licencia permite usarlos, pero no redistribuirlos tal cual por separado: PRIVADO, solo en esta imagen.\n")
        n += 1
    print(f"sonidos_privados: {n} esquemas en {dest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
