#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H02 - estágio 1 (MBR).

Critério: no QEMU (IDE e USB) aparece `SaguiOS: cargando...`.
Também confere a estrutura do setor: 512 bytes, 0x55AA, partição ativa e
área de BPB sem código. Para isolar o MBR, monta uma imagem de teste em que
o estágio 2 só para a máquina (assim nada apaga a mensagem da tela).
"""
import os
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, particoes_img, RAIZ, SAIDA  # noqa: E402

B = os.path.join(RAIZ, "build")


def main():
    mbr = open(os.path.join(B, "saguios.img"), "rb").read(512)
    verificar(len(mbr) == 512 and mbr[510:512] == b"\x55\xAA", "assinatura 0x55AA no fim do setor 0")
    verificar(mbr[0] == 0xEB and mbr[2] == 0x90, "salto curto + NOP no início (formato com BPB)")
    verificar(mbr[3:0x5A] == bytes(0x57), "área do BPB falso (0x03-0x59) reservada e zerada")
    part = mbr[0x1BE:0x1CE]
    status, tipo = part[0], part[4]
    inicio, qtd = struct.unpack("<II", part[8:16])
    verificar(status == 0x80, "partição 1 marcada como ativa (0x80)")
    verificar(tipo != 0 and inicio == 1 and qtd > 0, f"partição válida: tipo 0x{tipo:02X}, LBA {inicio}, {qtd} setores")
    # D181/D194: la imagen con GPL lleva la partición de juegos y la de aplicaciones (ext2,
    # 0x83); la sin GPL, ninguna (o solo la de aplicaciones). D213 (v7.3): van como lógicas
    # dentro de una extendida (entrada 2) que empieza tras la de SaguiOS, alineada en 1 MB; la
    # de SaguiOS es la única primaria. El resto de la tabla, vacío.
    n = 0
    e = mbr[0x1CE:0x1DE]
    if e != bytes(16):
        i2, q2 = struct.unpack("<II", e[8:16])
        verificar(e[0] == 0 and e[4] == 0x0F and i2 % 2048 == 0 and i2 >= inicio + qtd and q2 > 0,
                  f"entrada 2: la extendida (0x0F, LBA {i2}, {q2} sectores, no activa, D213)")
        logicas = [p for p in particoes_img(os.path.join(B, "saguios.img")) if p[0] >= 5]
        fim = i2
        for num, tipo, ini, cnt in logicas:
            verificar(tipo == 0x83 and ini % 2048 == 0 and ini >= fim + 2048 and ini + cnt <= i2 + q2,
                      f"lógica {num}: partición del sistema (ext2, LBA {ini}, {cnt} sectores, D181/D194/D213)")
            fim = ini + cnt
        verificar(logicas and fim == i2 + q2, "la extendida acaba con la última lógica")
        n = 1
    verificar(mbr[0x1BE + 16 * (n + 1):0x1FE] == bytes(16 * (3 - n)), "demais entradas da tabela vazias")

    os.makedirs(SAIDA, exist_ok=True)
    stub = os.path.join(SAIDA, "estagio2_parado.bin")
    subprocess.run(["nasm", "-f", "bin", "-o", stub, os.path.join(RAIZ, "tests", "recursos", "estagio2_parado.asm")], check=True)
    img = os.path.join(SAIDA, "h02.img")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"), "--mbr", os.path.join(B, "boot", "mbr.bin"),
                    "--estagio2", stub, "--kernel", os.path.join(B, "kernel.bin"), "--kernel-elf", os.path.join(B, "kernel.elf"),
                    "--ramdisk", os.path.join(B, "ramdisk.sgfs"), "--saida", img], check=True, stdout=subprocess.DEVNULL)
    # mkimagem grava tamanho.txt ao lado da imagem; o da imagem real não é tocado.
    for modo in ("ide", "usb"):
        with Maquina("h02", modo, imagem=img) as m:
            m.esperar_tela("SaguiOS: cargando...", 60)
            verificar(True, f"[{modo}] mensagem 'SaguiOS: cargando...' na tela")
            m.captura(f"H02-mbr-{modo}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
