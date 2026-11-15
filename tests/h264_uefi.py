#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H264 - arranque UEFI (x64 e IA32, GOP) e imagen híbrida BIOS+UEFI (D264, mejora 062).

Criterio:
  - build/saguios.img es híbrida: la tabla MBR lleva la ESP (0xEF, FAT32 SAGUI-EFI) con
    EFI/BOOT/BOOTX64.EFI, BOOTIA32.EFI y SAGUI/{CABECERA.BIN,KERNEL.BIN,RAMDISK.SGF};
    CABECERA.BIN es igual que el LBA 1 y los cargadores son PE32+ (x64) y PE32 (IA32);
  - con OVMF de 64 bits y de 32 bits (QEMU, sin CSM) arranca hasta el escritorio: el
    cargador pasa el modo GOP (1024x768), el mapa de memoria (E820 convertido, la
    memoria detectada coherente con la de la máquina), el RSDP de ACPI y el SMBIOS; el
    escritorio sale con sus colores (formato de píxel y pitch bien pasados);
  - la cabecera usada es la del disco (LBA 1), así el kernel reconoce su disco;
  - la misma imagen sigue arrancando por BIOS (SeaBIOS), con firmware=bios en el registro.
"""
import os
import re
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, cor_do_pixel, achar_ovmf, mtools, IMAGEM, SAIDA, FalhaTeste  # noqa: E402


def esp_da_imagem(img):
    mbr = open(img, "rb").read(512)
    for k in range(4):
        e = mbr[446 + 16 * k:462 + 16 * k]
        if e[4] == 0xEF:
            return k + 1, struct.unpack("<II", e[8:16])
    return None, None


def conferir_imagem():
    num, pos = esp_da_imagem(IMAGEM)
    ini, qtd = pos or (0, 0)
    verificar(num is not None, f"la tabla MBR tiene la ESP (tipo 0xEF) en la entrada {num}, LBA {ini}, {qtd // 2048} MB")
    mbr = open(IMAGEM, "rb").read(512)
    verificar(mbr[0x1BE] == 0x80 and mbr[0x1BE + 4] == 0xDA, "la partición de SaguiOS sigue la 1.ª y activa (BIOS)")
    os.makedirs(SAIDA, exist_ok=True)
    esp = os.path.join(SAIDA, "h264-esp.img")
    subprocess.run(["dd", f"if={IMAGEM}", f"of={esp}", "bs=512", f"skip={ini}", f"count={qtd}", "status=none"], check=True)
    lista = mtools(esp, "mdir", "-/", "-b", "p:/", particionado=False)[0]
    for arq in ("EFI/BOOT/BOOTX64.EFI", "EFI/BOOT/BOOTIA32.EFI", "SAGUI/CABECERA.BIN", "SAGUI/KERNEL.BIN",
                "SAGUI/RAMDISK.SGF"):
        verificar(arq.upper() in lista.upper(), f"ESP: {arq}")
    bs = open(esp, "rb").read(512)
    verificar(bs[82:90] == b"FAT32   ", "ESP formateada en FAT32")
    for nome, maq, magic in (("BOOTX64.EFI", 0x8664, 0x20B), ("BOOTIA32.EFI", 0x14C, 0x10B)):
        dest = os.path.join(SAIDA, "h264-" + nome)
        mtools(esp, "mcopy", "-o", f"p:/EFI/BOOT/{nome}", dest, particionado=False)
        pe = open(dest, "rb").read()
        o = struct.unpack_from("<I", pe, 0x3C)[0]
        verificar(pe[:2] == b"MZ" and pe[o:o + 4] == b"PE\0\0" and struct.unpack_from("<H", pe, o + 4)[0] == maq
                  and struct.unpack_from("<H", pe, o + 24)[0] == magic and struct.unpack_from("<H", pe, o + 24 + 68)[0] == 10,
                  f"{nome}: PE {'32+' if magic == 0x20B else '32'}, aplicación EFI (subsistema 10)")
    cab = os.path.join(SAIDA, "h264-cab.bin")
    mtools(esp, "mcopy", "-o", "p:/SAGUI/CABECERA.BIN", cab, particionado=False)
    with open(IMAGEM, "rb") as f:
        f.seek(512)
        lba1 = f.read(512)
    verificar(open(cab, "rb").read() == lba1, "SAGUI/CABECERA.BIN es igual que el LBA 1 de la imagen")


def arrancar(firmware, modo="ide", memoria="1G"):
    with Maquina(f"h264-{firmware}", modo, memoria=memoria, firmware=firmware) as m:
        try:
            fim = time.time() + 240
            while "area de trabalho no ar" not in m.serial():
                if "no puede arrancar por UEFI" in m.serial() or time.time() > fim:
                    raise FalhaTeste("el cargador UEFI se paró o no se llegó al escritorio")
                time.sleep(0.5)
            m.esperar_serial("[GUI] relogio:", 30)
        except FalhaTeste:
            print(m.serial()[-3000:])
            raise
        s = m.serial()
        rot = f"[UEFI {firmware} {modo}]"
        b = re.search(r"\[BOOT\] firmware=uefi-(\w+) gop=(\d+)x(\d+) pitch=(\d+) fb=(\w+) rsdp=(\w+) smbios=(\w+) modos=(\d+)", s)
        verificar(b is not None, f"{rot} el kernel recibió la boot_info del cargador UEFI")
        verificar(b.group(1) == firmware, f"{rot} firmware={b.group(1)}")
        verificar((b.group(2), b.group(3)) == ("1024", "768") and int(b.group(4)) >= 4096,
                  f"{rot} modo GOP {b.group(2)}x{b.group(3)}, pitch {b.group(4)}, framebuffer {b.group(5)}, "
                  f"{b.group(8)} modos en la lista")
        verificar(int(b.group(6), 16) != 0 and "[INFO] ACPI: RSDP em" in s,
                  f"{rot} RSDP de ACPI dado por el firmware ({b.group(6)}) y tablas leídas")
        verificar(int(b.group(7), 16) != 0 and "[INFO] SMBIOS" in s, f"{rot} SMBIOS dado por el firmware ({b.group(7)})")
        mem = re.search(r"memoria_mb=(\d+)", s)
        total = int(memoria[:-1]) * 1024
        verificar(mem and total - 64 <= int(mem.group(1)) <= total,
                  f"{rot} mapa de memoria convertido a E820: {mem.group(1) if mem else '?'} MB libres de {total}")
        verificar("Mapa de memoria (E820)" in s, f"{rot} el mapa completo está en el registro")
        verificar("la del disco (LBA 1)" in s, f"{rot} el cargador usó la cabecera del LBA 1 del disco")
        png = m.captura(f"H264-uefi-{firmware}", guardar=(modo == "ide"))
        topo, meio, base = cor_do_pixel(png, 1000, 40), cor_do_pixel(png, 1000, 384), cor_do_pixel(png, 1000, 760)
        verificar(topo[1] > topo[0] and topo[1] > topo[2] and meio[0] > 180 and meio[2] < 90 and base[2] > base[0],
                  f"{rot} escritorio con sus colores (verde {topo}, amarillo {meio}, azul {base})")
        verificar(cor_do_pixel(png, 300, 2) == (0x18, 0x20, 0x2A), f"{rot} barra superior en su sitio (pitch bien)")


def main():
    conferir_imagem()
    arranques = 0
    for fw in ("x64", "ia32"):
        if not achar_ovmf(fw):
            print(f"AVISO: sin OVMF {fw}: se salta (bash tools/entorno-local.sh)")
            continue
        arrancar(fw)
        arranques += 1
    verificar(arranques > 0, "al menos un arranque por UEFI probado")
    if achar_ovmf("x64"):
        arrancar("x64", "usb")
    with Maquina("h264-bios", "ide") as m:
        m.esperar_serial("[SAGUI] boot ok", 120)
        verificar("[BOOT] firmware=bios" in m.serial(), "[BIOS] la imagen híbrida arranca por BIOS como antes")
    return 0


if __name__ == "__main__":
    sys.exit(main())
