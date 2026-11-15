#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H326 (D326) - ext3/ext4: reproducción del diario JBD2 al montar.

Un pendrive con un ext3 (diario sin sumas) y un ext4 por defecto (metadata_csum, diario csum
v3), los dos con una transacción confirmada y sin aplicar en el diario (debugfs: jo/jw/jc), que
reescribe el bloque de datos de a.txt. El SaguiOS los monta pidiendo escritura: reproduce el
diario (a.txt, de 6 bytes, pasa a "DESPUE") y los deja en solo lectura (la escritura en ext3/ext4 queda para
la 8.4.4). En el anfitrión: el contenido reproducido y e2fsck -fn limpio.
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, criar_disco, extrair_particao, e2fsck, debugfs,  # noqa: E402
                  pendrive_qemu, DEBUGFS, SAIDA)


def pendente(particao):
    """Deja en el diario una transacción confirmada que cambia el bloque de a.txt."""
    b = int(debugfs(particao, "bmap /a.txt 0").decode().split()[-1])
    nuevo = os.path.join(SAIDA, "h326-nuevo.tmp")
    with open(nuevo, "wb") as f:
        f.write(b"DESPUES\n" + bytes(4088))
    cmds = os.path.join(SAIDA, "h326-cmds.tmp")
    with open(cmds, "w") as f:
        f.write(f"jo\njw -b {b} {nuevo}\njc\n")
    subprocess.run([DEBUGFS, "-w", "-f", cmds, particao], check=True, capture_output=True)


def main():
    arq = {"a.txt": "ANTES\n"}
    particoes = [dict(tipo="ext3", mb=16, rotulo="TRES", arquivos=arq, depois=pendente),
                 dict(tipo="ext4", mb=32, rotulo="CUATRO", arquivos=arq, depois=pendente)]
    img, ini = criar_disco("h326-pen", particoes)
    for k, mb in ((0, 16), (1, 32)):
        p = extrair_particao(img, ini[k], mb)
        verificar("needs_recovery" in subprocess.run(["dumpe2fs", "-h", p], capture_output=True, text=True).stdout,
                  f"partición {k + 1}: diario pendiente (needs_recovery)")
        verificar(b"ANTES" in debugfs(p, "cat /a.txt"), f"partición {k + 1}: a.txt aún dice ANTES")
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(img)
    with Maquina("h326", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        for dev, pto in (("usb0p1", "/e3"), ("usb0p2", "/e4")):
            out = m.comando(f"mount -o rw {dev} {pto}")
            verificar("solo lectura" in out, f"{dev}: montado, solo lectura")
            out = m.comando(f"cat {pto}/a.txt")
            verificar("DESPUE" in out, f"{dev}: el diario se reprodujo (a.txt = DESPUES)")
            out = m.comando(f"echo x > {pto}/b.txt")
            verificar("solo lectura" in out, f"{dev}: escribir rechazado")
            m.comando(f"umount {pto}")
        s = m.serial()
        verificar(len(re.findall(r"diario reproducido: 1 transacciones", s)) == 2, "el kernel reproduce una transacción en cada volumen")
        verificar("Kernel panic" not in s and "PANICO" not in s, "sin pánicos")
    for k, mb in ((0, 16), (1, 32)):
        p = extrair_particao(img, ini[k], mb, img + f".p{k}-depois")
        verificar(b"DESPUE" in debugfs(p, "cat /a.txt"), f"partición {k + 1}: en el disco a.txt = DESPUES")
        cod, txt = e2fsck(p)
        if cod:
            print(txt)
        verificar(cod == 0, f"partición {k + 1}: e2fsck -fn limpio")
    return 0


if __name__ == "__main__":
    sys.exit(main())
