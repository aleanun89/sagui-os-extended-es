#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H131 (D155) - discos SATA en modo AHCI y esqueleto del USB 3.0 (xHCI).

Parte 1: el QEMU lleva el disco de arranque en el IDE y un segundo disco SATA
en un controlador AHCI (ich9-ahci) con dos particiones (un ext2 y un FAT32).
Lo que se comprueba:
  - el controlador AHCI se encuentra en el PCI y se enciende;
  - el disco responde al IDENTIFY y se registra en la capa de discos ("hdb");
  - sus particiones se ven (hdb1 y hdb2) y el comando "discos" las lista;
  - se monta una partición y se lee un fichero: la lectura va de verdad por el
    driver AHCI (READ DMA EXT), no es una casualidad del arranque;
  - el disco no se modifica ni un byte (está en modo "snapshot").

Parte 2: con un controlador xHCI (el de QEMU, 1b36:0001) el kernel tiene que
arrancar igual, decir por el log qué ha encontrado y qué le falta, y no
colgarse.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_disco  # noqa: E402


def main():
    hd, ini = criar_disco("h131-sata", [
        dict(tipo="ext2", mb=16, rotulo="SATAEXT", arquivos={"documento.txt": "texto del disco SATA\n"}),
        dict(tipo="fat32", mb=32, rotulo="SATAFAT", arquivos={"win.txt": "de la FAT32 SATA\n"}),
    ])
    antes = os.path.getsize(hd)
    extras = ["-device", "ich9-ahci,id=ahci",
              "-drive", f"if=none,id=sata0,format=raw,file={hd},snapshot=on",
              "-device", "ide-hd,drive=sata0,bus=ahci.0"]
    with Maquina("h131-sata", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar("[AHCI] controlador" in s, "el controlador AHCI se ha encontrado en el PCI")
        verificar(re.search(r"\[AHCI\] puerto 0: \"QEMU HARDDISK\", \d+ setores", s),
                  "el disco SATA ha respondido al IDENTIFY")
        verificar("[DISCO] hdb: SATA QEMU HARDDISK" in s, "el disco SATA se ha registrado en la capa de discos")
        verificar("hdb1" in s and "hdb2" in s, "sus dos particiones se han visto")
        out = m.comando("discos")
        verificar(re.search(r"hdb1\s+16 MB\s+ext2\s+\"SATAEXT\"", out),
                  "el comando discos muestra la partición ext2 del disco SATA")
        verificar(re.search(r"hdb2\s+32 MB\s+FAT32\s+\"SATAFAT\"", out),
                  "el comando discos muestra la partición FAT32 del disco SATA")
        out = m.comando("mount hdb1 /sata")
        verificar("montado en /sata, solo lectura" in out,
                  "la partición SATA se monta (sólo lectura, como los discos internos)")
        out = m.comando("cat /sata/documento.txt")
        verificar("texto del disco SATA" in out, "lectura de verdad a través del AHCI")
        out = m.comando("mount hdb2 /win")
        verificar("montado en /win, solo lectura" in out, "la partición FAT32 SATA también se monta")
        out = m.comando("cat /win/win.txt")
        verificar("de la FAT32 SATA" in out, "lectura de la FAT32 SATA")
        m.comando("umount /sata")
        m.comando("umount /win")
        verificar("gravacao recusada" not in m.serial(), "no se ha escrito nada en el disco SATA")
    verificar(os.path.getsize(hd) == antes, "el disco SATA no ha cambiado de tamaño")

    # ---- parte 2: el xHCI (USB 3.0), que era un esqueleto (D155) y ya enumera (D214, h214) ----
    extras = ["-device", "qemu-xhci,id=xhci"]
    with Maquina("h131-xhci", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar("[XHCI] controlador" in s and "en marcha" in s, "el controlador xHCI se ha encontrado en el PCI y arranca")
        verificar("[ ok ]" in s or "Discos internos ATA" in s, "el kernel sigue vivo después del xHCI")
    return 0


if __name__ == "__main__":
    sys.exit(main())
