#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H50 (D040) - ext2 com leitura e escrita, vários volumes montados ao mesmo tempo.

Um pendrive com três partições: ext2 de blocos de 1 KB e inodes de 128
bytes, FAT32, e ext2 de blocos de 4 KB e inodes de 256 bytes. O SaguiOS
monta as três com o mount (em pastas diferentes), lê arquivos (UTF-8,
pasta com muitas entradas, arquivo grande com indiretos duplos), grava
(criar, crescer, truncar, pastas, renomear, mover, apagar), desmonta, e a
VPS confere com e2fsck -fn (tem que sair 0), debugfs e mtools.
"""
import os
import random
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, criar_disco, extrair_particao, e2fsck, debugfs,  # noqa: E402
                  pendrive_qemu, RAIZ)

GRANDE = "".join(f"linha {i:06d} do arquivo grande no ext2\n" for i in range(24000))   # ~860 KB: indireto duplo com 1 KB
random.seed(40)
BINARIO = bytes(random.getrandbits(8) for _ in range(5 * 1024 * 1024 + 123))           # 5 MB: indireto duplo com 4 KB
MUITOS = 300


def main():
    arquivos1 = {
        "leeme.txt": "Canción del ñandú: acción, corazón.\n",
        "fotos/2026/notas de março.txt": "pasta dentro de pasta\n",
        "grande.txt": GRANDE,
        "sobrescribir.txt": "x" * 5000,
    }
    for i in range(MUITOS):
        arquivos1[f"muchos/f{i:03d}.txt"] = f"archivo {i}\n"
    particoes = [
        dict(tipo="ext2", mb=24, rotulo="EXTUNO", arquivos=arquivos1, opcoes=["-b", "1024", "-I", "128"]),
        dict(tipo="fat32", mb=40, rotulo="FATDOS", arquivos={"oi.txt": "na FAT\n"}),
        dict(tipo="ext2", mb=24, rotulo="EXTTRES", arquivos={"binario.bin": BINARIO}, opcoes=["-b", "4096", "-I", "256"]),
    ]
    img, ini = criar_disco("h50-pen", particoes)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(img)
    with Maquina("h50", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("discos")
        verificar(re.search(r"usb0\s+QEMU.*ext2\s+\"EXTUNO\"\s+\(no montado\)", out), "discos: usb0 com o primeiro volume ext2")
        verificar(re.search(r"usb0p1\s+24 MB\s+ext2\s+\"EXTUNO\"", out) and re.search(r"usb0p2\s+40 MB\s+FAT32\s+\"FATDOS\"", out)
                  and re.search(r"usb0p3\s+24 MB\s+ext2\s+\"EXTTRES\"", out), "discos: as três partições com formato e rótulo")
        out = m.comando("mount usb0p1 /datos")
        verificar("montado en /datos" in out and "solo lectura" not in out, "mount usb0p1 /datos (ext2, leitura e escrita)")
        out = m.comando("mount -t fat32 usb0p2 /mnt/fat")
        verificar("montado en /mnt/fat" in out, "mount -t fat32 usb0p2 /mnt/fat (pasta criada no caminho)")
        out = m.comando("mount -t ext2 -o rw LABEL=EXTTRES /mnt/cuatro")
        verificar("montado en /mnt/cuatro" in out, "mount por LABEL= do ext2 de blocos de 4 KB")
        out = m.comando("mount")
        verificar("usb0p1  en /datos  tipo ext2  (rw" in out and "usb0p2  en /mnt/fat  tipo fat32" in out
                  and "usb0p3  en /mnt/cuatro  tipo ext2" in out, "mount sem argumentos lista as três montagens")
        # ---- leitura
        out = m.comando("cat /datos/leeme.txt")
        verificar("Canción del ñandú: acción, corazón." in out, "cat de arquivo UTF-8 do Linux, com acentos")
        out = m.comando("cat \"/datos/fotos/2026/notas de março.txt\"")
        verificar("pasta dentro de pasta" in out, "nome com acento (UTF-8 no disco) em subpastas")
        out = m.comando("ls /datos/muchos | conta")
        verificar(f"{MUITOS} líneas" in out, f"pasta com {MUITOS} entradas listada inteira")
        out = m.comando("cat /datos/muchos/f287.txt")
        verificar("archivo 287" in out, "arquivo do fim da pasta grande")
        out = m.comando("wc /datos/grande.txt")
        verificar(f"{GRANDE.count(chr(10))} líneas" in out and f"{len(GRANDE)} letras" in out,
                  "arquivo grande (indireto duplo, blocos de 1 KB) lido inteiro")
        out = m.comando("wc /mnt/cuatro/binario.bin")
        verificar(f"{len(BINARIO)} letras" in out, "binário de 5 MB (indireto duplo, blocos de 4 KB) lido inteiro")
        out = m.comando("cat /mnt/fat/oi.txt")
        verificar("na FAT" in out, "a FAT32 montada ao mesmo tempo")
        # ---- escrita
        m.comando("echo hola ext2 > /datos/nuevo.txt")
        m.comando("echo otra linea >> /datos/nuevo.txt")
        m.comando("echo corto > /datos/sobrescribir.txt")
        m.comando("cp /datos/grande.txt /datos/copia.txt")
        m.comando("cp /mnt/cuatro/binario.bin /mnt/cuatro/copia.bin")
        m.comando("mkdir \"/datos/carpeta nueva\"")
        m.comando("echo dentro > \"/datos/carpeta nueva/dentro.txt\"")
        m.comando("mv /datos/nuevo.txt \"/datos/carpeta nueva\"")
        m.comando("mkdir /datos/vacia")
        m.comando("rmdir /datos/vacia")
        m.comando("rm /datos/leeme.txt")
        m.comando("mv /datos/fotos /datos/imagenes")
        m.comando("mkdir /datos/a")
        m.comando("mv /datos/imagenes /datos/a")
        m.comando("echo año > /datos/año.txt")
        for i in range(0, MUITOS, 3):
            if i % 30 == 0:
                m.comando(f"rm /datos/muchos/f{i:03d}.txt")
        for i in range(40):
            m.comando(f"echo nuevo {i} > /datos/muchos/n{i:02d}.txt") if i % 8 == 0 else None
        m.comando("cp /datos/copia.txt /mnt/fat/del-ext2.txt")
        out = m.comando("cat \"/datos/carpeta nueva/nuevo.txt\"")
        verificar("hola ext2" in out and "otra linea" in out, "o SaguiOS relê o que gravou no ext2")
        out = m.comando("echo x > /datos/a")
        out = m.comando("rmdir /datos/a")
        verificar("no está vacía" in out, "rmdir de pasta com coisas: recusado")
        out = m.comando("umount /datos")
        verificar("Desmontado: /datos" in out, "umount /datos")
        out = m.comando("ls /datos")
        verificar("no existe" in out, "a pasta criada pela montagem some ao desmontar")
        m.comando("umount usb0p3")
        out = m.comando("umount /mnt/fat")
        verificar("Ya puedes retirar el pendrive" in out, "último volume desmontado: pode retirar")
        m.captura("H50-ext2")
        verificar("[TESTE] falha" not in m.serial() and "PANICO" not in m.serial(), "nenhum erro no kernel")

    p1 = extrair_particao(img, ini[0], 24)
    p2 = extrair_particao(img, ini[1], 40)
    p3 = extrair_particao(img, ini[2], 24)
    for p, nome in ((p1, "ext2 1 KB"), (p3, "ext2 4 KB")):
        cod, txt = e2fsck(p)
        if cod != 0:
            print(txt)
        verificar(cod == 0, f"e2fsck -fn no {nome}: íntegro (código {cod})")
    verificar(debugfs(p1, "cat \"/carpeta nueva/nuevo.txt\"") == b"hola ext2\notra linea\n", "debugfs: arquivo criado, aumentado e movido")
    verificar(debugfs(p1, "cat \"/carpeta nueva/dentro.txt\"") == b"dentro\n", "debugfs: arquivo em pasta nova")
    verificar(debugfs(p1, "cat /sobrescribir.txt") == b"corto\n", "debugfs: arquivo truncado e regravado")
    verificar(debugfs(p1, "cat /copia.txt") == GRANDE.encode(), "debugfs: cópia de ~860 KB idêntica (indireto duplo gravado)")
    verificar(debugfs(p3, "cat /copia.bin") == BINARIO, "debugfs: cópia binária de 5 MB idêntica (blocos de 4 KB)")
    verificar(debugfs(p1, "cat \"/a/imagenes/2026/notas de março.txt\"") == b"pasta dentro de pasta\n",
              "debugfs: pasta renomeada e movida para outra pasta (com '..' certo)")
    verificar(debugfs(p1, "cat /año.txt") == "año\n".encode("latin-1"), "debugfs: nome com ñ gravado em UTF-8")
    lista = debugfs(p1, "ls -l /").decode("utf-8", "replace")
    verificar("leeme.txt" not in lista and "vacia" not in lista and "fotos" not in lista, "debugfs: apagados e renomeados sumiram")
    muitos = debugfs(p1, "ls /muchos").decode()
    verificar("f000.txt" not in muitos and "f030.txt" not in muitos and "f001.txt" in muitos and "n08.txt" in muitos,
              "debugfs: pasta grande com entradas apagadas e novas")
    r = subprocess.run(["mtype", "-i", p2, "::/del-ext2.txt"], capture_output=True, env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
    verificar(r.stdout == GRANDE.encode(), "mtools: cópia do ext2 para a FAT32 idêntica")
    r = subprocess.run(["fsck.vfat", "-n", p2], capture_output=True, text=True)
    verificar(r.returncode == 0, "fsck.vfat: FAT32 íntegra")
    return 0


if __name__ == "__main__":
    sys.exit(main())
