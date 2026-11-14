#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H151 - una imagen de 38 MB arranca de verdad (D181).

La meta de la imagen pasa a 38 MB. Hoy el ramdisk ocupa unos 12 MB, pero si la
base crece, el cargador (boot/estagio2.asm) tiene que poder cargar un ramdisk
que llene toda la meta. Esta prueba lo comprueba con una variante de prueba:
el contenido normal del ramdisk (build/rootfs) más un fichero de datos
aleatorios (/relleno/aleatorio.bin) hasta casi 38 MB de bytes útiles.

Criterio:
  - la imagen arranca hasta el shell por IDE, por USB (EHCI con la emulación de
    disco de la BIOS) y por CD (El Torito con emulación de disco duro, D077);
  - el fichero grande llega entero: su SHA-256 dentro del SaguiOS es el mismo
    que en el anfitrión (si el cargador perdiera o repitiera un bloque, la suma
    del cargador o la del SGFS lo pararían antes, pero esto lo comprueba de
    punta a punta);
  - con 32 MB de RAM el ramdisk no cabe: la etapa 2 lo dice en pantalla
    ("AVISO: el ramdisk no cabe en la RAM que da la BIOS") en vez de solo
    "datos leidos corruptos";
  - con la RAM mínima arranca: 48 MB en consola (RAM_MINIMA) y 56 MB con el
    escritorio (RAM_MINIMA_ESCRITORIO; con 48 MB el escritorio no llega a abrir
    el Terminal).

Además mide el tiempo desde que arranca el QEMU hasta "[SAGUI] boot ok" con la
imagen normal y con la de 38 MB, en cada modo (informativo, no es criterio).
"""
import hashlib
import os
import random
import shutil
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, BUILD, SAIDA, RAIZ  # noqa: E402

META = 38 * 1024 * 1024
MARGEM = 256 * 1024          # por debajo de la meta, para el redondeo de sectores y entradas
RAM_MINIMA = "48M"           # D181: con 38 MB de ramdisk, la RAM que hace falta para llegar al shell
RAM_MINIMA_ESCRITORIO = "56M"   # ...y para el escritorio con el Terminal abierto


def construir():
    """Ramdisk y las imágenes de prueba (IDE/USB y CD). Devuelve (img, iso, sha256)."""
    rootfs = os.path.join(BUILD, "rootfs")
    if not os.path.isdir(os.path.join(rootfs, "bin")):
        raise AssertionError("falta build/rootfs (haz make antes)")
    os.makedirs(SAIDA, exist_ok=True)
    pasta = os.path.join(SAIDA, "h151-rootfs")
    if os.path.isdir(pasta):
        shutil.rmtree(pasta)
    shutil.copytree(rootfs, pasta)
    # Lo que no es ramdisk en la imagen: MBR, cabecera, etapa 2 y kernel.
    fixo = 512 * 2 + os.path.getsize(os.path.join(BUILD, "boot", "estagio2.bin")) + \
        os.path.getsize(os.path.join(BUILD, "kernel.bin")) + 2 * 512
    rd_normal = os.path.getsize(os.path.join(BUILD, "ramdisk.sgfs"))
    relleno = META - MARGEM - fixo - rd_normal - 2 * 72
    os.makedirs(os.path.join(pasta, "relleno"))
    grande = os.path.join(pasta, "relleno", "aleatorio.bin")
    gerador = random.Random(181)
    h = hashlib.sha256()
    with open(grande, "wb") as f:
        restam = relleno
        while restam > 0:
            n = min(restam, 1 << 20)
            bloco = gerador.getrandbits(8 * n).to_bytes(n, "little")
            h.update(bloco)
            f.write(bloco)
            restam -= n
    rd = os.path.join(SAIDA, "h151-ramdisk.sgfs")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mksgfs.py"), pasta, rd],
                   check=True, stdout=subprocess.DEVNULL)
    shutil.rmtree(pasta)
    comum = ["--mbr", os.path.join(BUILD, "boot", "mbr.bin"), "--estagio2", os.path.join(BUILD, "boot", "estagio2.bin"),
             "--kernel", os.path.join(BUILD, "kernel.bin"), "--kernel-elf", os.path.join(BUILD, "kernel.elf"),
             "--ramdisk", rd, "--vesa", "--console"]
    img = os.path.join(SAIDA, "h151-38mb.img")
    saida = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"), *comum, "--saida", img],
                           check=True, capture_output=True, text=True).stdout
    print("   ", saida.strip().splitlines()[-1])
    util = int(open(os.path.join(SAIDA, "tamanho-base.txt")).read())
    verificar(META - 2 * MARGEM < util <= META, f"la imagen de prueba tiene {util} bytes útiles (meta {META})")
    cab = open(img, "rb").read(1024)[512:]
    r_end, = struct.unpack_from("<I", cab, 44)
    r_bytes, = struct.unpack_from("<I", cab, 40)
    print(f"    ramdisk de {r_bytes} bytes en 0x{r_end:X}: acaba en 0x{r_end + r_bytes:X} "
          f"({(r_end + r_bytes + (1 << 20) - 1) >> 20} MB de RAM)")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"), *[o for o in comum if o != "--console"],
                    "--saida", os.path.join(SAIDA, "h151-38mb-esc.img")], check=True, stdout=subprocess.DEVNULL)
    img_cd = os.path.join(SAIDA, "h151-38mb-cd.img")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"), *comum, "--saida", img_cd, "--cd"],
                   check=True, stdout=subprocess.DEVNULL)
    iso = os.path.join(SAIDA, "h151-38mb.iso")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkiso.py"), "--imagem", img_cd, "--saida", iso],
                   check=True, stdout=subprocess.DEVNULL)
    os.remove(img_cd)
    return img, iso, h.hexdigest()


def arrancar(nome, modo, imagem, memoria="512M"):
    """Arranca y devuelve (máquina abierta, segundos hasta "[SAGUI] boot ok")."""
    m = Maquina(nome, modo, imagem=imagem, memoria=memoria).ligar()
    t0 = time.time()
    m.esperar_serial("[SAGUI] boot ok", 300)
    return m, time.time() - t0


def main():
    img, iso, sha = construir()
    normal = imagem_de_teste("", ["--vesa", "--console"])
    normal_cd = None
    tempos = {}
    try:
        for modo in ("ide", "usb", "cd"):
            if modo == "cd":
                c = os.path.join(SAIDA, "h151-normal-cd.img")
                subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"),
                                "--mbr", os.path.join(BUILD, "boot", "mbr.bin"),
                                "--estagio2", os.path.join(BUILD, "boot", "estagio2.bin"),
                                "--kernel", os.path.join(BUILD, "kernel.bin"),
                                "--kernel-elf", os.path.join(BUILD, "kernel.elf"),
                                "--ramdisk", os.path.join(BUILD, "ramdisk.sgfs"),
                                "--vesa", "--console", "--cd", "--saida", c], check=True, stdout=subprocess.DEVNULL)
                normal_cd = os.path.join(SAIDA, "h151-normal.iso")
                subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkiso.py"), "--imagem", c,
                                "--saida", normal_cd], check=True, stdout=subprocess.DEVNULL)
                os.remove(c)
            # La normal primero, para comparar el tiempo de arranque.
            m, t = arrancar("h151n", modo, normal_cd if modo == "cd" else normal)
            m.desligar()
            tempos[(modo, "normal")] = t
            m, t = arrancar("h151", modo, iso if modo == "cd" else img)
            tempos[(modo, "38mb")] = t
            try:
                m.esperar_serial("sagui:/$", 120)
                verificar(True, f"[{modo}] la imagen de 38 MB arranca hasta el shell ({t:.1f} s hasta boot ok)")
                out = m.comando("sha256sum /relleno/aleatorio.bin", 600)
                verificar(sha in out, f"[{modo}] el fichero de relleno llega entero (SHA-256 igual al del anfitrión)")
            finally:
                m.desligar()

        # RAM mínima con 38 MB de ramdisk.
        m, t = arrancar("h151-min", "ide", img, memoria=RAM_MINIMA)
        try:
            m.esperar_serial("sagui:/$", 120)
            out = m.comando("mem", 30)
            libre = [l for l in out.splitlines() if "Libre" in l]
            verificar(True, f"[{RAM_MINIMA}] con la RAM mínima llega al shell ({libre[0].strip() if libre else '?'})")
        finally:
            m.desligar()

        esc = os.path.join(SAIDA, "h151-38mb-esc.img")
        m, t = arrancar("h151-esc", "ide", esc, memoria=RAM_MINIMA_ESCRITORIO)
        try:
            m.esperar_serial("sagui:/$", 180)
            verificar("\"Terminal\"" in m.serial(),
                      f"[{RAM_MINIMA_ESCRITORIO}, escritorio] con la RAM mínima el escritorio abre el Terminal")
        finally:
            m.desligar()

        # Con 32 MB no cabe: aviso claro de la etapa 2.
        with Maquina("h151-32m", "ide", imagem=img, memoria="32M") as m:
            m.esperar_tela("AVISO: el ramdisk no cabe", 120)
            linhas = [l for l in m.tela_texto() if "hacen falta" in l or "AVISO" in l or "ERROR" in l]
            print("    pantalla:", " / ".join(l.strip() for l in linhas))
            verificar(True, "[32M] la etapa 2 avisa de que el ramdisk no cabe en la RAM")
            verificar(any("ERROR" in l for l in m.tela_texto()) or "[SAGUI] boot ok" not in m.serial(),
                      "[32M] y no arranca con un ramdisk incompleto")
    finally:
        for f in (img, iso, normal_cd, os.path.join(SAIDA, "h151-ramdisk.sgfs"), os.path.join(SAIDA, "h151-38mb-esc.img")):
            if f and os.path.exists(f):
                os.remove(f)
        print("    tiempos hasta boot ok (s):")
        for (modo, cual), t in sorted(tempos.items()):
            print(f"      {modo:4s} {cual:6s} {t:6.1f}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
