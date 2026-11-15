#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H265 (D265) - discos NVMe.

QEMU emula un NVMe (-device nvme + nvme-ns). Se comprueba, con un disco con una partición FAT32 y
otra ext2 hechas en el anfitrión:
  - el controlador se encuentra por el PCI (01:08:02), se enciende y registra el namespace ("nvme0");
  - `discos` e `infopc` lo muestran (modelo, capacidad, tamaño de bloque; el número de serie no sale);
  - se montan las dos particiones y se lee un fichero conocido;
  - un fichero de varios MB (listas PRP) se lee con la suma correcta;
  - se escribe (fichero pequeño y copia de varios MB), se apaga, se vuelve a arrancar con el mismo
    disco y se relee todo;
  - dos namespaces en un controlador;
  - un namespace con bloques de 4096 (tablas de particiones en bloques de 4096);
  - un disco GPT (lectura de la tabla, ver D179/D265);
  - un controlador NVMe sin disco no cuelga el arranque.
"""
import hashlib
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, criar_disco, extrair_particao, SAIDA  # noqa: E402

GRANDE = bytes((i * 7 + (i >> 8) * 13 + (i >> 16) * 31) & 0xFF for i in range(5 * 1024 * 1024 + 777))
SHA_GRANDE = hashlib.sha256(GRANDE).hexdigest()
PARTICOES = [
    dict(tipo="fat32", mb=24, rotulo="NVFAT", arquivos={"hola.txt": "hola desde el NVMe FAT\n", "grande.bin": GRANDE}),
    dict(tipo="ext2", mb=24, rotulo="NVEXT", arquivos={"documento.txt": "texto del NVMe ext2\n", "grande.bin": GRANDE}),
]


def extras_nvme(img, snapshot=True, bloco=None, serie="SGNVME265", extra_ns=None):
    """-device nvme con un namespace (o dos, con extra_ns = otra imagen)."""
    snap = "on" if snapshot else "off"
    e = ["-drive", f"if=none,id=nv0,format=raw,file={img},snapshot={snap}",
         "-device", f"nvme,id=nvme0,serial={serie}"]
    ns = f"nvme-ns,drive=nv0,bus=nvme0,nsid=1" + (f",logical_block_size={bloco},physical_block_size={bloco}" if bloco else "")
    e += ["-device", ns]
    if extra_ns:
        e += ["-drive", f"if=none,id=nv1,format=raw,file={extra_ns},snapshot=on",
              "-device", "nvme-ns,drive=nv1,bus=nvme0,nsid=2"]
    return e


def sha_guest(m, caminho):
    out = m.comando(f"sha256sum {caminho}", 600)
    r = re.search(r"([0-9a-f]{64})", out)
    return r.group(1) if r else ""


def disco_gpt(nome, base_img, ini):
    """GPT hecha con sfdisk a partir de las particiones de la imagen MBR base."""
    img = os.path.join(SAIDA, nome + ".img")
    if os.path.exists(img):
        os.remove(img)
    with open(img, "wb") as f:
        f.truncate(60 * 1024 * 1024)
    script = ("label: gpt\n"
              f"start=2048, size={24 * 2048}, type=EBD0A0A2-B9E5-4433-87C0-68B6B72699C7, name=NVFAT\n"
              f"start={2048 + 24 * 2048}, size={24 * 2048}, type=0FC63DAF-8483-4772-8E79-3D69D8477DE4, name=NVEXT\n")
    subprocess.run(["sfdisk", "-q", img], input=script, text=True, check=True)
    for i, mb in zip(ini, (24, 24)):
        parte = extrair_particao(base_img, i, mb, os.path.join(SAIDA, nome + ".p.tmp"))
        subprocess.run(["dd", f"if={parte}", f"of={img}", "bs=512", f"seek={i}", "conv=notrunc", "status=none"], check=True)
        os.remove(parte)
    return img


def tabla_4k(img):
    """Reescribe la tabla MBR de la imagen en bloques de 4096 (inicio y tamaño / 8)."""
    with open(img, "r+b") as f:
        mbr = bytearray(f.read(512))
        for p in range(4):
            o = 446 + 16 * p
            if mbr[o + 4]:
                ini = int.from_bytes(mbr[o + 8:o + 12], "little") // 8
                qtd = int.from_bytes(mbr[o + 12:o + 16], "little") // 8
                mbr[o + 8:o + 12] = ini.to_bytes(4, "little")
                mbr[o + 12:o + 16] = qtd.to_bytes(4, "little")
        f.seek(0)
        f.write(mbr)


def main():
    hd, ini = criar_disco("h265-nvme", PARTICOES)

    # ---- parte 1: lectura, discos, infopc ----
    with Maquina("h265-lee", "ide", imagem=imagem_console(), extras=extras_nvme(hd)) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar(re.search(r"\[NVME\] controlador \w+:\w+ en \d+:\d+\.\d+, versión", s), "el controlador NVMe se encuentra en el PCI y se enciende")
        verificar(re.search(r"\[NVME\] namespace 1: \".*\", \d+ bloques de 512 bytes", s), "Identify Namespace: bloques de 512")
        verificar("[DISCO] nvme0: NVMe" in s, "el namespace se registra en la capa de discos como nvme0")
        verificar("nvme0p1" in s and "nvme0p2" in s, "se ven sus dos particiones")
        verificar("SGNVME265" not in s, "el número de serie no aparece en el registro")
        out = m.comando("discos")
        verificar(re.search(r"nvme0p1\s+24 MB\s+FAT32\s+\"NVFAT\"", out), "discos: partición FAT32 del NVMe")
        verificar(re.search(r"nvme0p2\s+24 MB\s+ext2\s+\"NVEXT\"", out), "discos: partición ext2 del NVMe")
        out = m.comando("infopc -t", 120)
        verificar(re.search(r"nvme0 \[NVMe\]: NVMe .*, 50 MB, bloque 512", out), "infopc: nvme0 con modelo, capacidad y bloque")
        verificar("SGNVME265" not in out, "infopc no muestra el número de serie")
        verificar("Controlador NVMe" in out, "infopc: el controlador NVMe sale en el PCI")
        m.comando("mount nvme0p1 /nvf")
        m.comando("mount nvme0p2 /nve")
        out = m.comando("cat /nvf/hola.txt")
        verificar("hola desde el NVMe FAT" in out, "lectura de un fichero de la FAT32")
        out = m.comando("cat /nve/documento.txt")
        verificar("texto del NVMe ext2" in out, "lectura de un fichero de la ext2")
        verificar(sha_guest(m, "/nvf/grande.bin") == SHA_GRANDE, "FAT32: fichero de 5 MB leído completo (listas PRP)")
        verificar(sha_guest(m, "/nve/grande.bin") == SHA_GRANDE, "ext2: fichero de 5 MB leído completo")
        verificar("gravacao recusada" not in m.serial(), "el disco interno no se escribe sin -o rw")

    # ---- parte 2: escritura y relectura tras reiniciar ----
    antes = os.path.getsize(hd)
    with Maquina("h265-escribe", "ide", imagem=imagem_console(), extras=extras_nvme(hd, snapshot=False)) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("mount -o rw nvme0p1 /nvf")
        verificar("montado" in out, "mount -o rw de la FAT32")
        m.comando("mount -o rw nvme0p2 /nve")
        m.comando("echo escrito-por-nvme > /nvf/nuevo.txt")
        m.comando("cp /nvf/grande.bin /nvf/copia.bin", 600)
        m.comando("echo ext2-escrito > /nve/nuevo.txt")
        m.comando("cp /nve/grande.bin /nve/copia.bin", 600)
        m.comando("umount /nvf")
        m.comando("umount /nve")
    verificar(os.path.getsize(hd) == antes, "el disco no cambia de tamaño")
    with Maquina("h265-relee", "ide", imagem=imagem_console(), extras=extras_nvme(hd)) as m:
        m.esperar_serial("sagui:/$", 180)
        m.comando("mount nvme0p1 /nvf")
        m.comando("mount nvme0p2 /nve")
        verificar("escrito-por-nvme" in m.comando("cat /nvf/nuevo.txt"), "FAT32: el fichero escrito sobrevive al reinicio")
        verificar("ext2-escrito" in m.comando("cat /nve/nuevo.txt"), "ext2: el fichero escrito sobrevive al reinicio")
        verificar(sha_guest(m, "/nvf/copia.bin") == SHA_GRANDE, "FAT32: la copia de 5 MB escrita es idéntica")
        verificar(sha_guest(m, "/nve/copia.bin") == SHA_GRANDE, "ext2: la copia de 5 MB escrita es idéntica")
    # el anfitrión ve lo mismo
    p1 = extrair_particao(hd, ini[0], 24)
    r = subprocess.run(["mtype", "-i", p1, "::/nuevo.txt"], capture_output=True, text=True, env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
    verificar("escrito-por-nvme" in r.stdout, "el anfitrión lee lo que escribió SaguiOS en la FAT32 del NVMe")
    os.remove(p1)

    # ---- parte 3: dos namespaces y bloques de 4096 ----
    hd2, _ = criar_disco("h265-nvme2", [dict(tipo="ext2", mb=16, rotulo="SEGUNDO", arquivos={"dos.txt": "namespace dos\n"})])
    with Maquina("h265-ns", "ide", imagem=imagem_console(), extras=extras_nvme(hd, extra_ns=hd2)) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar("[DISCO] nvme0:" in s and "[DISCO] nvme1:" in s, "dos namespaces: nvme0 y nvme1")
        m.comando("mount nvme1p1 /nv2")
        verificar("namespace dos" in m.comando("cat /nv2/dos.txt"), "se lee el segundo namespace")

    hd4, _ = criar_disco("h265-nvme4k", PARTICOES[:1] + [dict(tipo="ext2", mb=24, rotulo="NVEXT", arquivos={"documento.txt": "texto del NVMe ext2\n"})])
    tabla_4k(hd4)
    with Maquina("h265-4k", "ide", imagem=imagem_console(), extras=extras_nvme(hd4, bloco=4096)) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar(re.search(r"\[NVME\] namespace 1: .* de 4096 bytes", s), "Identify Namespace: bloques de 4096")
        verificar("nvme0p1" in s and "nvme0p2" in s, "la tabla en bloques de 4096 se traduce a sectores de 512")
        out = m.comando("discos")
        verificar(re.search(r"nvme0p1\s+24 MB\s+FAT32", out) and re.search(r"nvme0p2\s+24 MB\s+ext2", out), "discos: particiones de 24 MB en el disco de 4096")
        out = m.comando("infopc -t", 120)
        verificar("bloque 4096" in out, "infopc: bloque 4096")
        m.comando("mount nvme0p1 /nvf")
        m.comando("mount nvme0p2 /nve")
        verificar("hola desde el NVMe FAT" in m.comando("cat /nvf/hola.txt"), "4096: lectura de la FAT32")
        verificar("texto del NVMe ext2" in m.comando("cat /nve/documento.txt"), "4096: lectura de la ext2")
        verificar(sha_guest(m, "/nvf/grande.bin") == SHA_GRANDE, "4096: fichero de 5 MB")
    antes4 = open(hd4, "rb").read()
    with Maquina("h265-4k-w", "ide", imagem=imagem_console(), extras=extras_nvme(hd4, snapshot=False, bloco=4096)) as m:
        m.esperar_serial("sagui:/$", 180)
        m.comando("mount -o rw nvme0p1 /nvf")
        m.comando("echo escrito-4k > /nvf/n4k.txt")   # escritura de un bloque suelto: lee-modifica-escribe
        m.comando("umount /nvf")
    p1 = extrair_particao(hd4, 2048, 24)
    r = subprocess.run(["mtype", "-i", p1, "::/n4k.txt"], capture_output=True, text=True, env=dict(os.environ, MTOOLS_SKIP_CHECK="1"))
    verificar("escrito-4k" in r.stdout, "4096: la escritura llega al disco")
    verificar(open(hd4, "rb").read()[:512] == antes4[:512], "4096: la tabla de particiones no se tocó")
    os.remove(p1)

    # ---- parte 4: GPT ----
    gpt = disco_gpt("h265-gpt", hd, ini)
    with Maquina("h265-gpt", "ide", imagem=imagem_console(), extras=extras_nvme(gpt)) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar("nvme0p1" in s and "nvme0p2" in s, "GPT: las dos particiones se ven")
        out = m.comando("discos")
        verificar(re.search(r"nvme0p1\s+24 MB\s+FAT32\s+\"NVFAT\"", out) and re.search(r"nvme0p2\s+24 MB\s+ext2\s+\"NVEXT\"", out),
                  "GPT: FAT32 y ext2 reconocidas")
        m.comando("mount nvme0p2 /nve")
        verificar("texto del NVMe ext2" in m.comando("cat /nve/documento.txt"), "GPT: lectura de la ext2")

    # ---- parte 5: controlador sin disco ----
    with Maquina("h265-vacio", "ide", imagem=imagem_console(), extras=["-device", "nvme,id=nvme0,serial=VACIO"]) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar("[NVME] controlador" in s, "controlador NVMe sin namespaces: se detecta")
        verificar("sagui:/$" in s, "el arranque sigue y llega al prompt")
    return 0


if __name__ == "__main__":
    sys.exit(main())
