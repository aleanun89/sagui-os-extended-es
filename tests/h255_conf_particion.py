#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H255 (D256, mejora 098) - la partición SAGUI-CONF en las imágenes y el instalador.

Parte 1 (anfitrión): mkimagem.py crea SAGUI-CONF (ext2, 16 MB, tipo 0x83, lógica de la extendida,
  la última, con un LEEME) en la imagen normal, la multinúcleo, la uACPI, las de /home persistente
  (ext2 y FAT32), la de particiones y la sin GPL; no la crea con --sin-conf ni en la imagen de CD.
  Si existen, se comprueban también las imágenes de build/ (saguios.img, .vdi).
Parte 2 (QEMU): "discos" la lista, con su nota de configuración del sistema.
Parte 3 (instalador): instalar en un disco vacío crea SAGUI-CONF; se pone un fichero en ella desde
  el anfitrión; reinstalar lo conserva; con --formatear-conf se borra.
"""
import os
import re
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, extrair_particao, e2fsck, RAIZ, BUILD, SAIDA)  # noqa: E402

MB = 1024 * 1024
DEBUGFS = "/usr/sbin/debugfs"
E2LABEL = "/usr/sbin/e2label"


def tabla(img):
    """Todas las particiones (primarias y lógicas) de una imagen: [(tipo, inicio, setores)]."""
    res = []
    with open(img, "rb") as f:
        mbr = f.read(512)
        for k in range(4):
            e = mbr[446 + 16 * k:462 + 16 * k]
            ini, qtd = struct.unpack("<II", e[8:16])
            if e[4] in (0x05, 0x0F, 0x85):
                res.append((e[4], ini, qtd))
                rel = 0
                while True:
                    f.seek((ini + rel) * 512)
                    ebr = f.read(512)
                    l, nx = ebr[446:462], ebr[462:478]
                    if l[4]:
                        res.append((l[4], ini + rel + struct.unpack("<I", l[8:12])[0], struct.unpack("<I", l[12:16])[0]))
                    if nx[4] not in (0x05, 0x0F, 0x85):
                        break
                    rel = struct.unpack("<I", nx[8:12])[0]
            elif e[4]:
                res.append((e[4], ini, qtd))
    return res


def achar_conf(img):
    """(tipo, inicio, setores, ruta_extraida) de la partición ext2 SAGUI-CONF, o None."""
    for tipo, ini, qtd in tabla(img):
        if tipo in (0x05, 0x0F, 0x85):
            continue
        with open(img, "rb") as f:
            f.seek(ini * 512 + 1024)
            sb = f.read(136)
        if sb[56:58] == b"\x53\xef" and sb[120:136].split(b"\0")[0] == b"SAGUI-CONF":
            return tipo, ini, qtd
    return None


def comprobar(img, rotulo, solo_leeme=True):
    c = achar_conf(img)
    verificar(c is not None, f"[{rotulo}] hay una partición ext2 SAGUI-CONF")
    if c is None:
        return None
    tipo, ini, qtd = c
    verificar(tipo == 0x83 and qtd == 16 * 2048, f"[{rotulo}] SAGUI-CONF: tipo 0x83 y 16 MB (tipo {tipo:02X}, {qtd // 2048} MB)")
    ultima = max((p for p in tabla(img) if p[0] not in (0x05, 0x0F, 0x85)), key=lambda p: p[1])
    verificar(ultima[1] == ini, f"[{rotulo}] SAGUI-CONF es la última partición")
    p = extrair_particao(img, ini, 16, os.path.join(SAIDA, "h255-conf.part"))
    rc, o = e2fsck(p)
    verificar(rc == 0, f"[{rotulo}] e2fsck da por buena SAGUI-CONF ({o.strip()[-100:]})")
    leeme = subprocess.run([DEBUGFS, "-R", "cat /LEEME", p], capture_output=True).stdout
    verificar(b"SAGUI-CONF" in leeme and b"configuraci\xc3\xb3n" in leeme, f"[{rotulo}] LEEME con el texto de la partición (UTF-8, igual en imagen e instalador)")
    ls = subprocess.run([DEBUGFS, "-R", "ls -p /", p], capture_output=True, text=True).stdout
    nombres = {l.split("/")[5] for l in ls.splitlines() if l.count("/") >= 5} - {".", "..", "lost+found"}
    if solo_leeme:
        # D318: el instalador deja ademas etc/ (sombra, hostname, zona, servicios) en SAGUI-CONF
        verificar(nombres in ({"LEEME"}, {"LEEME", "etc"}), f"[{rotulo}] solo contiene LEEME (y etc/ del instalador, D318) ({sorted(nombres)})")
    return c


def mkimagem(nombre, extras):
    destino = os.path.join(SAIDA, f"h255-{nombre}.img")
    os.makedirs(SAIDA, exist_ok=True)
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(BUILD, "boot", "mbr.bin"), "--estagio2", os.path.join(BUILD, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(BUILD, "kernel.bin"), "--kernel-elf", os.path.join(BUILD, "kernel.elf"),
                    "--ramdisk", os.path.join(BUILD, "ramdisk.sgfs"), "--saida", destino, "--vesa", *extras],
                   check=True, stdout=subprocess.DEVNULL)
    return destino


def parte1():
    apps = os.path.join(BUILD, "apps", "raiz")
    apps_opc = ["--apps", apps] if os.path.isdir(apps) else []
    variantes = {
        "normal": apps_opc,
        "smp": ["--smp"] + apps_opc,
        "uacpi": ["--uacpi"] + apps_opc,
        "persistente-ext2": ["--home-mb", "64", "--part-fs", "ext2"] + apps_opc,
        "persistente-fat32": ["--home-mb", "64", "--part-fs", "fat32"] + apps_opc,
        "particiones": ["--home-mb", "64", "--tmp-mb", "16", "--logs-mb", "16", "--part-fs", "ext2"] + apps_opc,
    }
    for nombre, extras in variantes.items():
        img = mkimagem(nombre, extras)
        comprobar(img, nombre)
        os.remove(img)
    img = mkimagem("sin-conf", ["--sin-conf"] + apps_opc)
    verificar(achar_conf(img) is None, "[--sin-conf] no hay SAGUI-CONF")
    os.remove(img)
    cd = os.path.join(SAIDA, "h255-cd.img")
    subprocess.run(["python3", os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", os.path.join(BUILD, "boot", "mbr.bin"), "--estagio2", os.path.join(BUILD, "boot", "estagio2.bin"),
                    "--kernel", os.path.join(BUILD, "kernel.bin"), "--kernel-elf", os.path.join(BUILD, "kernel.elf"),
                    "--ramdisk", os.path.join(BUILD, "ramdisk.sgfs"), "--saida", cd, "--vesa", "--cd"],
                   check=True, stdout=subprocess.DEVNULL)
    verificar(achar_conf(cd) is None, "[cd] la imagen de la ISO no lleva SAGUI-CONF")
    os.remove(cd)
    # Las imágenes ya hechas por make y por gerar_versao.sh, si están.
    for arq in ("saguios.img", "saguios-smp.img", "saguios-uacpi.img"):
        ruta = os.path.join(BUILD, arq)
        if os.path.exists(ruta):
            comprobar(ruta, "build/" + arq)
    vdi = os.path.join(BUILD, "saguios.vdi")
    if os.path.exists(vdi):
        raw = os.path.join(SAIDA, "h255-vdi.raw")
        subprocess.run(["qemu-img", "convert", "-f", "vdi", "-O", "raw", vdi, raw], check=True)
        comprobar(raw, "build/saguios.vdi")
        os.remove(raw)


def poner_en_conf(img, nombre, contenido):
    c = achar_conf(img)
    p = extrair_particao(img, c[1], 16, os.path.join(SAIDA, "h255-conf-w.part"))
    tmp = os.path.join(SAIDA, "h255-contenido.bin")
    with open(tmp, "wb") as f:
        f.write(contenido)
    subprocess.run([DEBUGFS, "-w", "-R", f"write {tmp} {nombre}", p], check=True, capture_output=True)
    subprocess.run(["dd", f"of={img}", f"if={p}", "bs=512", f"seek={c[1]}", "conv=notrunc", "status=none"], check=True)


def leer_de_conf(img, nombre):
    c = achar_conf(img)
    p = extrair_particao(img, c[1], 16, os.path.join(SAIDA, "h255-conf-r.part"))
    return subprocess.run([DEBUGFS, "-R", f"cat {nombre}", p], capture_output=True).stdout


def parte2():
    img = imagem_console()
    with Maquina("h255-discos", "ide", imagem=img) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("discos")
        verificar("SAGUI-CONF" in out, "en QEMU, discos lista SAGUI-CONF")
        verificar("configuración del sistema" in out.replace("\xf3", "ó") or "configuraci" in out,
                  "discos la marca como configuración del sistema")


def parte3():
    dest = os.path.join(SAIDA, "h255-dest.img")
    with open(dest, "wb") as f:
        f.truncate(512 * MB)
    origem = imagem_console()
    extras = ["-drive", f"file={dest},if=ide,index=1,format=raw"]
    secreto = os.urandom(3000)
    with Maquina("h255-inst1", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("instalar hdb -s", 1200)
        verificar("SaguiOS instalado en hdb y verificado" in out, f"instalar hdb termina ({out.strip()[-200:]})")
        verificar("SAGUI-CONF" in out, "el resumen del instalador menciona SAGUI-CONF")
    comprobar_instalado(dest, "instalado")
    poner_en_conf(dest, "clave-prueba", secreto)
    verificar(leer_de_conf(dest, "/clave-prueba") == secreto, "el fichero de prueba está en SAGUI-CONF")
    with Maquina("h255-inst2", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("instalar hdb -s", 1200)
        verificar("SaguiOS instalado en hdb y verificado" in out, "la reinstalación termina")
        verificar("Se conserva el contenido de SAGUI-CONF" in out, "avisa de que conserva SAGUI-CONF")
        verificar("contenido anterior conservado" in out, "dice que lo ha conservado")
    comprobar_instalado(dest, "reinstalado", False)
    verificar(leer_de_conf(dest, "/clave-prueba") == secreto, "la reinstalación conserva el fichero de SAGUI-CONF byte a byte")
    with Maquina("h255-inst3", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("instalar hdb --formatear-conf -s", 1200)
        verificar("SaguiOS instalado en hdb y verificado" in out, "la instalación con --formatear-conf termina")
        verificar("se BORRA también SAGUI-CONF" in out, "avisa de que borra SAGUI-CONF")
    comprobar_instalado(dest, "formateado")
    verificar(b"clave-prueba" not in subprocess.run([DEBUGFS, "-R", "ls /", extrair_particao(dest, achar_conf(dest)[1], 16,
              os.path.join(SAIDA, "h255-conf-r.part"))], capture_output=True).stdout,
              "--formatear-conf borra el contenido de SAGUI-CONF")


def comprobar_instalado(dest, rotulo, solo_leeme=True):
    c = comprobar(dest, rotulo, solo_leeme)
    if c:
        t = tabla(dest)
        with open(dest, "rb") as f:
            f.seek(512)
            cab = f.read(512)
        sis = [p for p in t if p[0] == 0x83][1:]   # todas menos /home
        verificar(int.from_bytes(cab[136:140], "little") == sum(p[2] for p in sis),
                  f"[{rotulo}] CAB_SIS_SETORES incluye SAGUI-CONF")


def main():
    partes = sys.argv[1:] or ["1", "2", "3"]   # para repetir solo una
    for n, f in (("1", parte1), ("2", parte2), ("3", parte3)):
        if n in partes:
            f()
    return 0


if __name__ == "__main__":
    sys.exit(main())
