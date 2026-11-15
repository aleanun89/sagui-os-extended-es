#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""
crear-disco.py - crea una imagen de disco de SaguiOS con varias particiones (D120).

Disco MBR: partición 1 = el sistema (arranque), la única primaria; las de datos que SaguiOS monta
solo van como lógicas dentro de una extendida que ocupa el resto (D213, v7.3):
  SAGUI-DATOS -> /home      SAGUI-TMP -> /tmp      SAGUI-LOGS -> /logs
(con --primarias, el formato de antes: hasta 3 primarias de datos).
D270: como saguios.img, arranca por BIOS y por UEFI (la partición EFI SAGUI-EFI, D264), salvo con
--sin-uefi o --primarias (no queda entrada libre para ella).
El resultado es un .img disperso (ocupa poco en el PC aunque mida GB) que se graba entero en un
pendrive o disco con Rufus (modo imagen DD) o con dd. Uso:
  python3 tools/crear-disco.py --home-mb 4096 --tmp-mb 512 --logs-mb 256 --fs fat32
  python3 tools/crear-disco.py --home-mb 2048 --tmp-mb 256 --logs-mb 128 --extendida --extra-mb 512
  python3 tools/crear-disco.py -i            (asistente: pregunta tamaños y sistema)
Lo normal es "make imagem-particoes HOME_MB=4096 TMP_MB=512 LOGS_MB=256 PART_FS=fat32".
Un tamaño 0 omite esa partición.
"""
import argparse
import os
import struct
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NOMBRES = {0xDA: "SaguiOS (arranque)", 0x83: "ext2", 0x0C: "FAT32", 0xEF: "EFI (FAT32, UEFI)"}


def preguntar(texto, defecto, tipo=int):
    r = input(f"{texto} [{defecto}]: ").strip()
    return tipo(r) if r else defecto


def asistente(a):
    print("Asistente de disco de SaguiOS (Intro = valor entre corchetes; 0 = sin esa partición)")
    a.home_mb = preguntar("Tamaño de /home en MB", a.home_mb)
    a.tmp_mb = preguntar("Tamaño de /tmp en MB", a.tmp_mb)
    a.logs_mb = preguntar("Tamaño de /logs en MB", a.logs_mb)
    a.fs = preguntar("Sistema de archivos (fat32 = se ve desde Windows, ext2 = Linux)", a.fs, str)
    a.salida = preguntar("Archivo de salida", a.salida, str)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--home-mb", type=int, default=2048)
    ap.add_argument("--tmp-mb", type=int, default=256)
    ap.add_argument("--logs-mb", type=int, default=128)
    ap.add_argument("--fs", default="fat32", choices=("fat32", "ext2"))
    ap.add_argument("--salida", default=os.path.join(RAIZ, "build", "saguios-particiones.img"))
    ap.add_argument("--extra-mb", type=int, action="append", default=[], help="partición extra (lógica); repetible")
    ap.add_argument("--extendida", action="store_true", help="(ya es lo de por defecto, D213; se acepta por compatibilidad)")
    ap.add_argument("--primarias", action="store_true", help="el formato de antes de la v7.3: las de datos como primarias")
    ap.add_argument("--xz", action="store_true", help="además, comprime a .img.xz")
    ap.add_argument("--sin-uefi", action="store_true", help="sin la partición EFI: solo arranque por BIOS (D270)")
    ap.add_argument("-i", "--interactivo", action="store_true")
    ap.add_argument("--build", default=os.path.join(RAIZ, "build"))
    ap.add_argument("--jogos", default=None, metavar="CARPETA",
                    help="partición de juegos SAGUI-JOGOS (D181, D228); por defecto <build>/jogos/raiz si existe")
    ap.add_argument("--sin-jogos", action="store_true", help="sin la partición de juegos")
    ap.add_argument("--jogos-mb", type=int, default=0, help="tamaño de la partición de juegos en MB (0 = el justo)")
    ap.add_argument("--apps", default=None, metavar="CARPETA",
                    help="partición de aplicaciones SAGUI-APPS (D194); por defecto <build>/apps/raiz si existe")
    ap.add_argument("--sin-apps", action="store_true", help="sin la partición de aplicaciones")
    ap.add_argument("--apps-mb", type=int, default=0, help="tamaño de la partición de aplicaciones en MB (0 = lo justo más margen)")
    ap.add_argument("mkimagem_extra", nargs="*", help="opciones extra para mkimagem.py (tras --)")
    a = ap.parse_args()
    if a.interactivo:
        asistente(a)
    b = a.build
    cmd = [sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"),
           "--mbr", f"{b}/boot/mbr.bin", "--estagio2", f"{b}/boot/estagio2.bin",
           "--kernel", f"{b}/kernel.bin", "--kernel-elf", f"{b}/kernel.elf", "--ramdisk", f"{b}/ramdisk.sgfs",
           "--saida", a.salida, "--vesa", "--part-fs", a.fs,
           "--home-mb", str(a.home_mb), "--tmp-mb", str(a.tmp_mb), "--logs-mb", str(a.logs_mb), *a.mkimagem_extra]
    for mb in a.extra_mb:
        cmd += ["--extra-mb", str(mb)]
    if a.primarias:
        cmd.append("--primarias")
    # D270: la partición EFI (arranque UEFI) si el cargador está compilado (make uefi).
    uefi = os.path.join(b, "boot", "uefi")
    if not a.sin_uefi and not a.primarias and "--uefi" not in a.mkimagem_extra:
        if os.path.isfile(os.path.join(uefi, "BOOTX64.EFI")) and os.path.isfile(os.path.join(uefi, "BOOTIA32.EFI")):
            cmd += ["--uefi", uefi]
        else:
            print("crear-disco: AVISO: sin el cargador UEFI (make uefi): la imagen solo arranca por BIOS")
    jogos = a.jogos or f"{b}/jogos/raiz"
    con_jogos = not a.sin_jogos and bool(a.jogos or os.path.isdir(jogos))
    if con_jogos:
        cmd += ["--jogos", jogos, "--jogos-mb", str(a.jogos_mb)]
    apps = a.apps or f"{b}/apps/raiz"
    con_apps = not a.sin_apps and bool(a.apps or os.path.isdir(apps))
    if con_apps:
        cmd += ["--apps", apps, "--apps-mb", str(a.apps_mb)]
    if not os.path.exists(f"{b}/kernel.bin"):
        sys.exit("crear-disco: falta compilar (ejecuta 'make' antes, con '. ~/.local/saguitools/env.sh')")
    subprocess.run(cmd, check=True)

    rotulos = [(m, r) for m, r in ((a.home_mb, ("/home", "SAGUI-DATOS")), (a.tmp_mb, ("/tmp", "SAGUI-TMP")),
                                   (a.logs_mb, ("/logs", "SAGUI-LOGS"))) if m > 0]
    rotulos = [r for _, r in rotulos] + [("-", f"SAGUI-X{k + 1}") for k in range(len(a.extra_mb))]
    if con_jogos:   # las del sistema van las últimas: juegos y aplicaciones (mkimagem.py, D181, D194)
        rotulos.append(("/jogos, lectura y escritura", "SAGUI-JOGOS"))
    if con_apps:
        rotulos.append(("/apps, aplicaciones", "SAGUI-APPS"))
    if "--sin-conf" not in a.mkimagem_extra:   # D256: la última, con la configuración del sistema
        rotulos.append(("configuración del sistema", "SAGUI-CONF"))
    usados = iter(rotulos)
    print("\nTabla de particiones de", a.salida)
    print(f"{'N':>2}  {'Tipo':<20}{'Inicio (LBA)':>13}{'Tamaño':>10}  Se monta en")

    def fila(n, tipo, ini, qtd, monta):
        print(f"{n:>2}  {NOMBRES.get(tipo, f'tipo {tipo:02X}'):<20}{ini:>13}{qtd * 512 / 1048576:>7.0f} MB  {monta}")

    with open(a.salida, "rb") as f:
        mbr = f.read(512)
        logica = 5
        for k in range(4):
            e = mbr[0x1BE + 16 * k:0x1CE + 16 * k]
            if not e[4]:
                continue
            ini, qtd = struct.unpack("<II", e[8:16])
            if e[4] in (0x05, 0x0F, 0x85):
                fila(k + 1, e[4], ini, qtd, "(contenedor de las lógicas)")
                rel = 0
                while True:
                    f.seek((ini + rel) * 512)
                    ebr = f.read(512)
                    l, nx = ebr[446:462], ebr[462:478]
                    if l[4]:
                        p, r = next(usados)
                        fila(logica, l[4], ini + rel + struct.unpack("<I", l[8:12])[0], struct.unpack("<I", l[12:16])[0],
                             f"{p}  (etiqueta {r})")
                        logica += 1
                    if nx[4] not in (0x05, 0x0F, 0x85):
                        break
                    rel = struct.unpack("<I", nx[8:12])[0]
            elif k == 0:
                fila(1, e[4], ini, qtd, "(arranque)")
            elif e[4] == 0xEF:
                fila(k + 1, e[4], ini, qtd, "(arranque UEFI: SAGUI-EFI, D264)")
            else:
                p, r = next(usados)
                fila(k + 1, e[4], ini, qtd, f"{p}  (etiqueta {r})")
    st = os.stat(a.salida)
    print(f"\nImagen: {a.salida}\n  tamaño {st.st_size / 1048576:.0f} MB, ocupa en disco {st.st_blocks * 512 / 1048576:.1f} MB (dispersa)")
    if a.xz:
        subprocess.run(["xz", "-k", "-f", "-T0", a.salida], check=True)
        print(f"  comprimida: {a.salida}.xz ({os.path.getsize(a.salida + '.xz') / 1048576:.1f} MB)")
    nombre = os.path.basename(a.salida)
    print(f"""
Cómo grabarla (se borra TODO el pendrive o disco de destino):
  Rufus: elige el dispositivo, pulsa SELECCIÓN y escoge {nombre}; al pulsar EMPEZAR
         responde "Grabar en modo Imagen DD". (Rufus también abre el .img.xz.) Más pasos en docs/imagenes.md.
  Linux: sudo dd if={a.salida} of=/dev/sdX bs=4M conv=fsync status=progress
  Con {a.fs.upper()} las particiones /home, /tmp y /logs se ven desde Windows""" if a.fs == "fat32" else f"""
Cómo grabarla (se borra TODO el pendrive o disco de destino):
  Rufus: elige el dispositivo, pulsa SELECCIÓN y escoge {nombre}; al pulsar EMPEZAR responde
         "Grabar en modo Imagen DD". Más pasos en docs/imagenes.md.
  Linux: sudo dd if={a.salida} of=/dev/sdX bs=4M conv=fsync status=progress
  Con ext2 Windows no ve las particiones de datos (solo SaguiOS y Linux las leen).""")


if __name__ == "__main__":
    main()
