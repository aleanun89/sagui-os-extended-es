#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H51 (D040, D337) - ext4 e ext3 montados só para leitura (-o ro).

Um pendrive com um ext4 do mke2fs atual (extents, 64 bits, flex_bg,
metadata_csum, inodes de 256, huge_file, dir_nlink, extra_isize), um ext3
(com diário) e uma FAT32. No ext4 há um arquivo grande fragmentado de
propósito (árvore de extents com nível de índice) e uma pasta com muitas
entradas. O SaguiOS lê tudo, copia o arquivo fragmentado para a FAT32, e
qualquer tentativa de escrita é recusada. (Desde la 8.4.4 SaguiOS escribe en ext3/ext4,
D337: aquí se montan con "-o ro" y la escritura se prueba en h326.) A VPS confere que as partições ext3/ext4 não mudaram nem um byte e
que o e2fsck continua limpo.
"""
import hashlib
import os
import random
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, criar_disco, extrair_particao, e2fsck, debugfs,  # noqa: E402
                  pendrive_qemu, DEBUGFS, SAIDA)

random.seed(41)
GRANDE = bytes(random.getrandbits(8) for _ in range(1200000))
MUITOS = 400


def fragmentar(particao):
    """Apaga um arquivo sim, outro não, e grava o grande nos buracos."""
    origem = os.path.join(SAIDA, "h51-grande.tmp")
    with open(origem, "wb") as f:
        f.write(GRANDE)
    cmds = os.path.join(SAIDA, "h51-cmds.tmp")
    with open(cmds, "w") as f:
        for i in range(0, 200, 2):
            f.write(f"rm /enchimento/p{i}\n")
        f.write(f"write {origem} grande.bin\n")
    subprocess.run([DEBUGFS, "-w", "-f", cmds, particao], check=True, capture_output=True)


def sha(caminho):
    return hashlib.sha256(open(caminho, "rb").read()).hexdigest()


def main():
    arquivos4 = {"texto.txt": "ext4 leído por SaguiOS: canción, ñandú.\n", "sub/dentro/hondo.txt": "bien hondo\n"}
    for i in range(200):
        arquivos4[f"enchimento/p{i}"] = bytes([i]) * 8192
    for i in range(MUITOS):
        arquivos4[f"lista/e{i:03d}"] = f"{i}\n"
    particoes = [
        dict(tipo="ext4", mb=24, rotulo="CUATRO", arquivos=arquivos4, depois=fragmentar),
        dict(tipo="ext3", mb=8, rotulo="TRES", arquivos={"ext3.txt": "con diario\n"}),
        dict(tipo="fat32", mb=40, rotulo="SALIDA"),
    ]
    img, ini = criar_disco("h51-pen", particoes)
    p4 = extrair_particao(img, ini[0], 24, img + ".p4-antes")
    p3 = extrair_particao(img, ini[1], 8, img + ".p3-antes")
    extents = debugfs(p4, "ex /grande.bin").decode()
    verificar(re.search(r"^\s*0/\s*1\s", extents, re.M) and extents.count("\n") > 20,
              f"o arquivo grande ficou fragmentado: árvore de extents com índice ({extents.count(chr(10)) - 2} extents)")
    antes4, antes3 = sha(p4), sha(p3)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(img)
    with Maquina("h51", "ide", imagem=imagem_console(), extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("discos")
        verificar(re.search(r"usb0p1\s+24 MB\s+ext4\s+\"CUATRO\"", out) and re.search(r"usb0p2\s+8 MB\s+ext3\s+\"TRES\"", out),
                  "discos reconhece ext4 e ext3")
        out = m.comando("mount -o ro usb0p1 /e4")
        verificar("montado en /e4, solo lectura" in out, "mount -o ro de ext4: só para leitura")
        out = m.comando("cat /e4/texto.txt")
        verificar("ext4 leído por SaguiOS: canción, ñandú." in out, "cat de arquivo no ext4 (extents)")
        out = m.comando("cat /e4/sub/dentro/hondo.txt")
        verificar("bien hondo" in out, "subpastas no ext4")
        out = m.comando("ls /e4/lista | conta")
        verificar(f"{MUITOS} líneas" in out, f"pasta com {MUITOS} entradas (com índice htree, lida como lista)")
        out = m.comando("wc /e4/grande.bin")
        verificar(f"{len(GRANDE)} letras" in out, "arquivo fragmentado lido inteiro")
        out = m.comando("mount usb0p3 /fat")
        m.comando("cp /e4/grande.bin /fat/grande.bin", tempo=400)
        out = m.comando("echo x > /e4/nuevo.txt")
        verificar("solo lectura" in out, "criar arquivo no ext4: recusado (sistema de archivos de solo lectura)")
        out = m.comando("mkdir /e4/carpeta")
        verificar("solo lectura" in out, "mkdir no ext4: recusado")
        out = m.comando("rm /e4/texto.txt")
        verificar("solo lectura" in out, "rm no ext4: recusado")
        out = m.comando("mv /e4/texto.txt /e4/otro.txt")
        verificar("solo lectura" in out, "mv no ext4: recusado")
        out = m.comando("echo x >> /e4/texto.txt")
        verificar("solo lectura" in out, "acrescentar no ext4: recusado")
        out = m.comando("mount -o ro usb0p2 /e3")
        verificar("montado en /e3, solo lectura" in out, "mount -o ro de ext3 com diário: só leitura")
        out = m.comando("cat /e3/ext3.txt")
        verificar("con diario" in out, "cat no ext3")
        m.captura("H51-ext4")
        for p in ("/e4", "/e3", "/fat"):
            m.comando(f"umount {p}")
    depois4 = extrair_particao(img, ini[0], 24, img + ".p4-depois")
    depois3 = extrair_particao(img, ini[1], 8, img + ".p3-depois")
    verificar(sha(depois4) == antes4, "a partição ext4 não mudou nem um byte")
    verificar(sha(depois3) == antes3, "a partição ext3 não mudou nem um byte")
    for p, nome in ((depois4, "ext4"), (depois3, "ext3")):
        cod, txt = e2fsck(p)
        if cod:
            print(txt)
        verificar(cod == 0, f"e2fsck -fn no {nome}: limpo")
    fat = extrair_particao(img, ini[2], 40)
    r = subprocess.run(["mtype", "-i", fat, "::/grande.bin"], capture_output=True, env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
    verificar(r.stdout == GRANDE, "o arquivo fragmentado copiado para a FAT32 é idêntico ao original")
    return 0


if __name__ == "__main__":
    sys.exit(main())
