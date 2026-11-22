#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H283 (D283) - pendientes del NVMe: BAR por encima de 4 GB, bloques de 4096 con particiones e instalador.

Criterio:
  - con OVMF (sin apagar su ventana de 64 bits) la BAR0 de 64 bits del NVMe de QEMU queda por encima de 4 GB;
    el kernel la recoloca por debajo («[PCI] NVMe: BAR0 recolocada»), el controlador se enciende y el NVMe
    funciona (se monta una partición FAT32 y se lee un fichero);
  - un NVMe de bloques de 4096, vacío: «particiones tabla-nueva ... mbr», crear primarias, una extendida y
    una lógica; el MBR del anfitrión cuenta en bloques de 4096 (inicio 256 = 1 MB); tras reiniciar se ven;
  - lo mismo con GPT: cabecera en el bloque 1 (byte 4096), entradas en el bloque 2, copia de respaldo en el
    último bloque, CRC correctos y las entradas en bloques de 4096;
  - instalar en un NVMe de 4096 desde el arranque UEFI (GPT automática) y arrancar de él por UEFI;
  - instalar con --mbr en un NVMe de 4096 se rechaza con un mensaje claro.
"""
import os
import re
import struct
import subprocess
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, imagem_console, criar_disco, achar_ovmf, BUILD, SAIDA  # noqa: E402
from h266_gpt_esp import disco_vazio  # noqa: E402
from h270_uefi_gpt import esperar_prompt, esperar_escritorio, UEFI_DIR, conferir_ramdisk  # noqa: E402

BLOCO = 4096


def nvme_4k(img, snapshot=False, boot=False):
    snap = "on" if snapshot else "off"
    return ["-drive", f"file={img},if=none,id=nv0,format=raw,snapshot={snap}",
            "-device", "nvme,id=nvme0,serial=h283" + (",bootindex=0" if boot else ""),
            "-device", f"nvme-ns,drive=nv0,bus=nvme0,nsid=1,logical_block_size={BLOCO},physical_block_size={BLOCO}"]


def leer_bloque(img, n, cuantos=1):
    with open(img, "rb") as f:
        f.seek(n * BLOCO)
        return f.read(cuantos * BLOCO)


def gpt_4k(img):
    """Lee una GPT en bloques de 4096 comprobando cabecera principal, respaldo y CRC; devuelve las entradas usadas
    como (tipo_hex, inicio_bloque, fin_bloque, nombre)."""
    total = os.path.getsize(img) // BLOCO
    h = leer_bloque(img, 1)[:92]
    verificar(h[:8] == b"EFI PART", "GPT 4K: cabecera en el bloque 1 (byte 4096)")
    c = bytearray(h)
    crc = struct.unpack_from("<I", c, 16)[0]
    struct.pack_into("<I", c, 16, 0)
    verificar(zlib.crc32(bytes(c)) == crc, "GPT 4K: CRC de la cabecera principal")
    mi, alt, prim, ult = struct.unpack_from('<QQQQ', h, 24)
    ent_lba, n, tam, crc_ent = struct.unpack_from('<QIII', h, 72)
    verificar(mi == 1 and alt == total - 1, f"GPT 4K: MyLBA=1 y AlternateLBA=último bloque ({mi}, {alt})")
    verificar(ent_lba == 2, "GPT 4K: entradas en el bloque 2")
    ent = leer_bloque(img, ent_lba, (n * tam + BLOCO - 1) // BLOCO)[:n * tam]
    verificar(zlib.crc32(ent) == crc_ent, "GPT 4K: CRC de las entradas")
    r = leer_bloque(img, total - 1)[:92]
    verificar(r[:8] == b"EFI PART", "GPT 4K: cabecera de respaldo en el último bloque")
    c = bytearray(r)
    crc = struct.unpack_from("<I", c, 16)[0]
    struct.pack_into("<I", c, 16, 0)
    verificar(zlib.crc32(bytes(c)) == crc, "GPT 4K: CRC del respaldo")
    ent_r = struct.unpack_from("<Q", r, 72)[0]
    verificar(leer_bloque(img, ent_r, 4)[:n * tam] == ent, "GPT 4K: entradas del respaldo iguales a las principales")
    verificar(prim == 6 and ult == total - 6, f"GPT 4K: zona utilizable en bloques de 4096 ({prim}..{ult} de {total})")
    out = []
    for i in range(n):
        e = ent[i * tam:(i + 1) * tam]
        if not any(e[:16]):
            continue
        a, b = struct.unpack_from("<QQ", e, 32)
        nombre = e[56:128].decode("utf-16le").split("\0")[0]
        out.append((e[:16].hex(), a, b, nombre))
    return out


def mbr_4k(img):
    mbr = leer_bloque(img, 0)
    verificar(mbr[510:512] == b"\x55\xaa", "MBR 4K: firma 55AA")
    res = []
    for p in range(4):
        e = mbr[446 + 16 * p:462 + 16 * p]
        if e[4]:
            res.append((e[4], struct.unpack_from("<I", e, 8)[0], struct.unpack_from("<I", e, 12)[0]))
    return res


def tabla_mbr_4k():
    img = disco_vazio("h283-mbr4k", 200)
    with Maquina("h283-mbr", "ide", imagem=imagem_console(), extras=nvme_4k(img)) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("particiones tabla-nueva nvme0 mbr -s")
        print(out)
        out = m.comando("particiones crear nvme0 40 fat32 --etiqueta A4K -s", 300)
        verificar("creada" in out.lower() or "nvme0p1" in out, f"crear la primera partición en el NVMe 4K ({out.strip()[-200:]})")
        out = m.comando("particiones crear nvme0 100 extendida -s")
        out = m.comando("particiones crear nvme0 30 ext2 --logica --etiqueta L4K -s", 300)
        verificar("nvme0p5" in out or "creada" in out.lower(), f"crear una lógica en el NVMe 4K ({out.strip()[-200:]})")
        out = m.comando("discos")
        verificar(re.search(r"nvme0p1\s+40 MB\s+FAT32\s+\"A4K\"", out), f"discos: nvme0p1 de 40 MB ({out.strip()[-300:]})")
        verificar(re.search(r"nvme0p5\s+30 MB\s+ext2\s+\"L4K\"", out), "discos: la lógica nvme0p5 de 30 MB")
    tabla = mbr_4k(img)
    verificar(tabla[0] == (0x0C, 256, 40 * 256) or tabla[0][1:] == (256, 40 * 256), f"el MBR del anfitrión cuenta en bloques de 4096 ({tabla})")
    verificar(any(t[0] in (0x05, 0x0F) for t in tabla), "hay una extendida en el MBR")
    with Maquina("h283-mbr-r", "ide", imagem=imagem_console(), extras=nvme_4k(img, snapshot=True)) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("discos")
        verificar(re.search(r"nvme0p1\s+40 MB\s+FAT32", out) and re.search(r"nvme0p5\s+30 MB\s+ext2", out),
                  "tras reiniciar el kernel lee la tabla MBR en bloques de 4096")
        m.comando("mount -o rw nvme0p1 /n1")
        m.comando("echo hola4k > /n1/a.txt")
        verificar("hola4k" in m.comando("cat /n1/a.txt"), "escribir y leer en la partición creada")


def tabla_gpt_4k():
    img = disco_vazio("h283-gpt4k", 200)
    with Maquina("h283-gpt", "ide", imagem=imagem_console(), extras=nvme_4k(img)) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("particiones tabla-nueva nvme0 gpt -s")
        print(out)
        out = m.comando("particiones crear nvme0 40 fat32 --etiqueta G4K -s", 300)
        out = m.comando("particiones crear nvme0 30 ext2 --etiqueta GE4K -s", 300)
        out = m.comando("discos")
        verificar(re.search(r"nvme0p1\s+40 MB\s+FAT32\s+\"G4K\"", out) and re.search(r"nvme0p2\s+30 MB\s+ext2", out),
                  f"discos: las dos particiones GPT ({out.strip()[-300:]})")
    ents = gpt_4k(img)
    verificar(len(ents) == 2 and ents[0][1] == 256 and ents[0][2] == 256 + 40 * 256 - 1,
              f"las entradas GPT están en bloques de 4096 ({ents})")
    mbr = leer_bloque(img, 0)
    verificar(mbr[450] == 0xEE and struct.unpack_from("<I", mbr, 454)[0] == 1 and
              struct.unpack_from("<I", mbr, 458)[0] == os.path.getsize(img) // BLOCO - 1, "MBR protector en bloques de 4096")
    with Maquina("h283-gpt-r", "ide", imagem=imagem_console(), extras=nvme_4k(img, snapshot=True)) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("discos")
        verificar(re.search(r"nvme0p1\s+40 MB\s+FAT32", out) and re.search(r"nvme0p2\s+30 MB\s+ext2", out), "tras reiniciar: la GPT de 4096 se lee")


def uefi_bar_alta(origem):
    hd, _ = criar_disco("h283-nvme512", [dict(tipo="fat32", mb=24, rotulo="NVFAT", arquivos={"hola.txt": "hola desde el NVMe sobre OVMF\n"})])
    extras = ["-drive", f"if=none,id=nv0,format=raw,file={hd},snapshot=on", "-device", "nvme,id=nvme0,serial=h283bar",
              "-device", "nvme-ns,drive=nv0,bus=nvme0,nsid=1"]
    with Maquina("h283-bar", "ide", imagem=origem, extras=extras, firmware="x64") as m:
        esperar_prompt(m, "[BAR alta]")
        s = m.serial()
        verificar(re.search(r"\[PCI\] NVMe: BAR0 recolocada de [0-9a-f]+:[0-9a-f]+ \(alto:bajo\) a [0-9a-f]+", s),
                  "la BAR0 por encima de 4 GB se recoloca por debajo")
        verificar("por encima de 4 GB" not in s.replace("BAR0 recolocada", ""), "sin aviso de BAR inalcanzable")
        verificar(re.search(r"\[NVME\] namespace 1: ", s) is not None, "el NVMe se enciende tras recolocar la BAR")
        m.comando("mount nvme0p1 /nvf")
        verificar("hola desde el NVMe sobre OVMF" in m.comando("cat /nvf/hola.txt"), "se lee un fichero del NVMe con la BAR recolocada")


def instalar_4k(origem):
    nv = disco_vazio("h283-nvme-inst4k", 800)
    with Maquina("h283-inst", "ide", imagem=origem, extras=nvme_4k(nv), firmware="x64") as m:
        esperar_prompt(m, "[inst 4K]")
        out = m.comando("instalar nvme0 --mbr -s", 300)
        verificar("bloques de 4096" in out and "solo se instala con GPT" in out, f"instalar --mbr en 4K se rechaza ({out.strip()[-200:]})")
        out = m.comando("instalar nvme0 -s", 1800)
        verificar("SaguiOS instalado en nvme0 y verificado" in out and "Tabla de particiones: GPT" in out,
                  f"instalar en un NVMe de 4096 por UEFI: GPT automática ({out.strip()[-400:]})")
    ents = gpt_4k(nv)
    verificar(len(ents) >= 3, f"la GPT 4K instalada tiene ESP, SaguiOS y SAGUI-CONF ({len(ents)} entradas)")
    # quitar la opción de consola de la cabecera de la partición SaguiOS para llegar al escritorio
    sagui = [e for e in ents if e[0] == "554741534f49534f80015341" + "47554953"]
    verificar(len(sagui) == 1, "hay una partición de tipo SaguiOS")
    bloq = sagui[0][1]
    with open(nv, "r+b") as f:
        f.seek(bloq * BLOCO)
        cab = bytearray(f.read(512))
        op = struct.unpack_from("<H", cab, 6)[0]
        struct.pack_into("<H", cab, 6, op & ~2)
        f.seek(bloq * BLOCO)
        f.write(cab)
    vazio = disco_vazio("h283-vazio", 2)
    with Maquina("h283-boot", "ide", imagem=vazio, extras=nvme_4k(nv, boot=True), firmware="x64") as m:
        esperar_escritorio(m, "[boot 4K]")
        s = m.serial()
        verificar("es el disco de arranque (GPT: cabecera en nvme0p" in s, "arranca del NVMe de 4096 por UEFI y el kernel lo reconoce")
        verificar("[CONF] SAGUI-CONF en" in s or "[CONF] /root en la raiz del disco" in s,
                  "SAGUI-CONF del NVMe de 4096 montada (D379: o fundida en la raíz instalada)")


def main():
    if not achar_ovmf("x64"):
        print("AVISO: sin OVMF x64: se salta H283")
        return 0
    conferir_ramdisk()
    tabla_mbr_4k()
    tabla_gpt_4k()
    origem = imagem_de_teste("", ["--vesa", "--console", "--uefi", UEFI_DIR])
    uefi_bar_alta(origem)
    instalar_4k(origem)
    return 0


if __name__ == "__main__":
    sys.exit(main())
