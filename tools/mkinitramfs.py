#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
mkinitramfs.py - el initramfs de SaguiOS (D379, prototipo de la mejora 035 F2).

Uso: mkinitramfs.py <rootfs> <saida.sgfs> [--raiz LABEL=SAGUI-RAIZ] [--modo rw|ro] [--espera 10]

Hace un SGFS pequeño (el mismo formato que el ramdisk, tools/mksgfs.py) con lo justo
para arrancar y para el modo rescate: el shell y unas utilidades de /bin del <rootfs>
(build/rootfs), las carpetas vacías que el kernel espera y /etc/raiz.conf, que dice al
kernel qué partición montar en "/" (kernel/fs/raiz.c). Todo lo demás se lee de esa
partición (mkimagem.py --raiz). Si la raíz no se monta, el sistema sigue con esto.
"""
import argparse
import os
import shutil
import subprocess
import sys
import tempfile

# Lo mínimo para arreglar el sistema a mano: shell, listar y copiar, montar y mirar
# discos, el registro del kernel y la memoria. Siempre programas estáticos (D378: el
# initramfs no puede depender de /lib).
PROGRAMAS = ["sagsh", "sh", "ls", "cat", "cp", "mv", "rm", "mkdir", "echo", "mount", "umount",
             "montar", "desmontar", "discos", "dmesg", "mem", "ps", "sync"]
PASTAS = ["bin", "etc", "home", "tmp", "dev", "sistema"]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("rootfs")
    ap.add_argument("saida")
    ap.add_argument("--raiz", default="LABEL=SAGUI-RAIZ")
    ap.add_argument("--modo", default="rw", choices=("rw", "ro", "live", "toram"),
                    help="rw: instalado; live: raíz del medio de solo lectura con capa escribible (D379 L1); toram (T1)")
    ap.add_argument("--capa", default="auto",
                    help="live: auto (SAGUI-PERSIST si la imagen la lleva; si no, la RAM), ram, o el volumen de la capa persistente")
    ap.add_argument("--espera", type=int, default=10)
    a = ap.parse_args()
    if not os.path.isdir(os.path.join(a.rootfs, "bin")):
        sys.exit(f"mkinitramfs: {a.rootfs} no parece un rootfs (sin bin/)")
    with tempfile.TemporaryDirectory() as tmp:
        for p in PASTAS:
            os.makedirs(os.path.join(tmp, p), exist_ok=True)
        faltan = []
        for n in PROGRAMAS:
            src = os.path.join(a.rootfs, "bin", n)
            if os.path.isfile(src):
                shutil.copy2(src, os.path.join(tmp, "bin", n))
            else:
                faltan.append(n)
        if not os.path.isfile(os.path.join(tmp, "bin", "sagsh")):
            sys.exit("mkinitramfs: falta /bin/sagsh: sin shell no hay modo rescate")
        with open(os.path.join(tmp, "etc", "raiz.conf"), "w", encoding="utf-8") as f:
            f.write("# /etc/raiz.conf del initramfs (D379): la raíz del sistema\n"
                    f"raiz={a.raiz}\nmodo={a.modo}\nespera={a.espera}\ncapa={a.capa}\n")
        with open(os.path.join(tmp, "etc", "rescate.txt"), "w", encoding="utf-8") as f:
            f.write("Sistema mínimo en RAM (initramfs, D379). Si ves esto, la raíz del disco no se ha\n"
                    "montado: mira «dmesg» (líneas [RAIZ]) y «discos»; se puede montar a mano con «mount».\n"
                    "\n"
                    "- Si «discos» no muestra el disco o pendrive del que arrancaste, este PC no deja a\n"
                    "  SaguiOS verlo después de arrancar: usa la imagen saguios-completo.img, que carga todo\n"
                    "  el sistema en la RAM y no lo necesita.\n"
                    "- Con la tecla M pulsada al arrancar se elige el modo: 1 live desde el medio, 2 todo a\n"
                    "  la RAM, 3 instalado, 4 rescate. «kconfig raiz_modo=live|toram» lo deja guardado.\n"
                    "- Ctrl al arrancar: modo rescate con la raíz montada (sin lo guardado con kconfig).\n")
        aqui = os.path.dirname(os.path.abspath(__file__))
        subprocess.run([sys.executable, os.path.join(aqui, "mksgfs.py"), tmp, a.saida], check=True,
                       stdout=subprocess.DEVNULL)
    if faltan:
        print(f"mkinitramfs: no están en {a.rootfs}/bin (se siguen sin ellos): {' '.join(faltan)}")
    print(f"mkinitramfs: {a.saida}: {os.path.getsize(a.saida)} bytes, raiz={a.raiz} ({a.modo})")


if __name__ == "__main__":
    main()
