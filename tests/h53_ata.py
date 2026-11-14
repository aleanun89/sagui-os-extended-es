#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H53 (D040) - disco interno ATA (IDE): detecção, só leitura por padrão.

O QEMU liga com o disco de boot no IDE 0 e um segundo disco no IDE 1
(mestre/escravo do canal primário) com uma partição ext2 e uma FAT32.
Primeira partida: o disco é detectado ("hdb"), o de boot é reconhecido como
disco de arranque, o mount sem opções deixa tudo só para leitura e toda
escrita é recusada com mensagem em espanhol; no fim, o arquivo do disco não
mudou NEM UM BYTE (nada é gravado ao ligar, detectar, montar ou desmontar).
Segunda partida: "mount -o rw" explícito, gravação, desmontar; a VPS confere
com e2fsck -fn, debugfs e mtools.
"""
import hashlib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_disco, extrair_particao, e2fsck, debugfs, RAIZ  # noqa: E402


def sha(caminho):
    return hashlib.sha256(open(caminho, "rb").read()).hexdigest()


def main():
    hd, ini = criar_disco("h53-hd", [
        dict(tipo="ext2", mb=16, rotulo="INTERNO", arquivos={"documento.txt": "texto del disco interno\n"}),
        dict(tipo="fat32", mb=40, rotulo="WINDOWS", arquivos={"win.txt": "de la FAT32 interna\n"}),
    ])
    disco = ["-drive", f"file={hd},if=ide,index=1,format=raw"]
    antes = sha(hd)
    with Maquina("h53-ro", "ide", imagem=imagem_console(), extras=disco) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar(re.search(r"\[ATA\] 1f0/1: \"QEMU HARDDISK\"", s), "ATA: o segundo disco do canal primário responde ao IDENTIFY")
        verificar("[DISCO] hdb: ATA QEMU HARDDISK" in s and "hdb1" in s and "hdb2" in s, "registrado como hdb, com hdb1 e hdb2")
        out = m.comando("discos")
        verificar(re.search(r"hda\s+ATA QEMU HARDDISK.*disco de arranque de SaguiOS", out), "hda: o disco de boot é reconhecido como de arranque")
        verificar(re.search(r"hdb1\s+16 MB\s+ext2\s+\"INTERNO\"\s+\(no montado\)", out)
                  and re.search(r"hdb2\s+40 MB\s+FAT32\s+\"WINDOWS\"", out), "discos mostra as partições do disco interno")
        verificar("No hay ningún pendrive conectado." in out and "Discos internos" in out, "discos separa pendrives de discos internos")
        out = m.comando("mount hdb1 /interno")
        verificar("montado en /interno, solo lectura" in out, "mount sem -o: disco interno fica só para leitura")
        out = m.comando("cat /interno/documento.txt")
        verificar("texto del disco interno" in out, "leitura do ext2 do disco interno")
        out = m.comando("echo x > /interno/nuevo.txt")
        verificar("sistema de archivos de solo lectura" in out, "escrita recusada: \"sistema de archivos de solo lectura\"")
        out = m.comando("mkdir /interno/carpeta")
        verificar("solo lectura" in out, "mkdir recusado")
        out = m.comando("mount hdb2 /win")
        verificar("montado en /win, solo lectura" in out, "FAT32 interna também só para leitura")
        out = m.comando("cat /win/win.txt")
        verificar("de la FAT32 interna" in out, "leitura da FAT32 interna")
        out = m.comando("rm /win/win.txt")
        verificar("solo lectura" in out, "rm na FAT32 interna recusado")
        m.comando("umount /interno")
        m.comando("umount /win")
        m.captura("H53-ata-solo-lectura")
        verificar("gravacao recusada" not in m.serial(), "nenhuma gravação chegou nem a ser tentada no disco")
    verificar(sha(hd) == antes, "o arquivo do disco interno não mudou nem um byte (detectar, montar, ler, desmontar)")

    with Maquina("h53-rw", "ide", imagem=imagem_console(), extras=disco) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar(sha(hd) == antes, "ligar de novo não grava nada no disco interno")
        out = m.comando("mount -o rw hdb1 /interno")
        verificar("montado en /interno" in out and "solo lectura" not in out, "mount -o rw explícito: leitura e escrita")
        m.comando("echo escrito por SaguiOS > /interno/nuevo.txt")
        m.comando("mkdir /interno/carpeta")
        m.comando("cp /leia-me.txt /interno/carpeta/")
        out = m.comando("mount -t fat32 -o rw hdb2 /win")
        verificar("montado en /win" in out and "solo lectura" not in out, "FAT32 interna com -o rw")
        m.comando("echo hola windows > /win/desde-sagui.txt")
        out = m.comando("umount /interno")
        verificar("Desmontado" in out, "umount /interno")
        m.comando("umount /win")
    p1 = extrair_particao(hd, ini[0], 16)
    cod, txt = e2fsck(p1)
    if cod:
        print(txt)
    verificar(cod == 0, "e2fsck -fn no ext2 do disco interno: limpo")
    verificar(debugfs(p1, "cat /nuevo.txt") == b"escrito por SaguiOS\n", "debugfs: arquivo gravado pelo ATA")
    verificar(debugfs(p1, "cat /documento.txt") == b"texto del disco interno\n", "debugfs: o que já estava continua")
    original = open(os.path.join(RAIZ, "rootfs", "leia-me.txt"), encoding="utf-8").read()
    verificar(debugfs(p1, "cat /carpeta/leia-me.txt") == original.encode("latin-1"), "debugfs: cópia de vários setores pelo ATA")
    p2 = extrair_particao(hd, ini[1], 40)
    r = subprocess.run(["mtype", "-i", p2, "::/desde-sagui.txt"], capture_output=True, env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
    verificar(r.stdout == b"hola windows\n", "mtools: arquivo gravado na FAT32 interna")
    r = subprocess.run(["fsck.vfat", "-n", p2], capture_output=True, text=True)
    verificar(r.returncode == 0, "fsck.vfat: FAT32 interna íntegra")
    return 0


if __name__ == "__main__":
    sys.exit(main())
