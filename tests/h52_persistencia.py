#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H52 (D040) - persistência: /home na partição ext2 do disco de arranque.

A imagem é a normal com uma partição ext2 de dados (make
imagem-persistente). O /etc/fstab padrão monta "arranque" (a partição ext
do disco que tem a partição do SaguiOS) em /home, para leitura e escrita.
O teste grava em /home, reinicia (o QEMU sai com -no-reboot), liga de novo
a MESMA imagem sem snapshot e lê de novo. Nos dois modos: pendrive USB
(como no G470) e disco IDE (driver ATA). No fim, e2fsck -fn limpo.
"""
import os
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, extrair_particao, e2fsck, debugfs, particoes_img, RAIZ, SAIDA, FalhaTeste  # noqa: E402

DADOS_MB = 16


def imagem_persistente(nome):
    b = os.path.join(RAIZ, "build")
    destino = os.path.join(SAIDA, nome + ".img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(b, "boot", "mbr.bin"), "--estagio2", os.path.join(b, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(b, "kernel.bin"), "--kernel-elf", os.path.join(b, "kernel.elf"),
                    "--ramdisk", os.path.join(b, "ramdisk.sgfs"), "--saida", destino, "--vesa", "--console",
                    "--dados-mb", str(DADOS_MB)], check=True, stdout=subprocess.DEVNULL)
    return destino


def inicio_dados(img):
    dados = [p for p in particoes_img(img) if p[1] != 0xDA]
    verificar(dados and dados[0][1] == 0x83 and dados[0][0] == 5,
              "a imagem tem a partição de dados do tipo Linux (0x83), lógica na extendida (D213)")
    return dados[0][2]


def rodada(modo):
    img = imagem_persistente(f"h52-{modo}")
    ini = inicio_dados(img)
    texto = f"guardado en /home ({modo}) antes de reiniciar\n"
    with Maquina(f"h52-a", modo, imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar("montado em /home, leitura e escrita" in m.serial(), f"[{modo}] o fstab montou a partição de dados em /home (rw)")
        verificar("Pendrive conectado" not in m.serial(), f"[{modo}] o disco de arranque não gera aviso de pendrive")
        out = m.comando("cat /home/leeme.txt")
        verificar("sigue aquí después de reiniciar" in out, f"[{modo}] /home/leeme.txt vem da imagem")
        out = m.comando("mount")
        verificar("en /home  tipo ext2  (rw" in out, f"[{modo}] mount lista /home")
        m.comando(f"echo '{texto.strip()}' > /home/nota.txt")   # com aspas: "(" é sintaxe no sh (D037)
        m.comando("mkdir /home/docs")
        m.comando("cp /leia-me.txt /home/docs/")
        m.captura(f"H52-persistencia-{modo}-1")
        m.digitar("reiniciar\n", 0.04)
        fim = time.time() + 60
        while m.proc.poll() is None and time.time() < fim:
            time.sleep(0.3)
        verificar(m.proc.poll() is not None, f"[{modo}] reiniciar: a máquina reiniciou (o QEMU saiu com -no-reboot)")
        verificar("[EXT2] " in m.serial() and "desmontado (tudo gravado)" in m.serial(), f"[{modo}] antes de reiniciar, /home foi desmontado")
    with Maquina(f"h52-b", modo, imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        verificar("nao foi desmontado direito" not in m.serial(), f"[{modo}] a segunda partida encontra o ext2 limpo")
        out = m.comando("cat /home/nota.txt")
        verificar(texto.strip() in out, f"[{modo}] depois de reiniciar, /home/nota.txt continua lá")
        out = m.comando("ls /home/docs")
        verificar("leia-me.txt" in out, f"[{modo}] e a pasta /home/docs com a cópia")
        m.captura(f"H52-persistencia-{modo}-2")
        m.comando("umount /home")
    p = extrair_particao(img, ini, DADOS_MB)
    cod, txt = e2fsck(p)
    if cod:
        print(txt)
    verificar(cod == 0, f"[{modo}] e2fsck -fn na partição de dados: limpo")
    verificar(debugfs(p, "cat /nota.txt").decode("latin-1") == texto, f"[{modo}] debugfs: o conteúdo de nota.txt confere")
    original = open(os.path.join(RAIZ, "rootfs", "leia-me.txt"), encoding="utf-8").read().encode("latin-1")
    verificar(debugfs(p, "cat /docs/leia-me.txt") == original, f"[{modo}] debugfs: a cópia do leia-me.txt confere")


def main():
    modos = sys.argv[1:] or ["usb", "ide"]
    for modo in modos:
        rodada(modo)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste:
        raise
