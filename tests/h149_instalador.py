#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H149 (D179) - el instalador: SaguiOS a un disco duro (IDE) y a un pendrive (EHCI).

Parte 1 (IDE, terminal): un disco vacío de 512 MB (hdb) y un disco con datos de juegos (hdc,
  montado en /jogos/opentyrian). "instalar hdb --copiar-jogos" crea la tabla (SaguiOS 0xDA activa desde el
  LBA 1, extendida con SAGUI-DATOS, SAGUI-JOGOS y SAGUI-APPS, las tres ext2), copia y verifica.
  El origen lleva la partición SAGUI-APPS de la imagen (D194): /apps (NetSurf...) se copia.
  Desde el anfitrión: la tabla, la cabecera con el identificador de instalación, el sistema
  copiado byte a byte, los sistemas de archivos (e2fsck, fsck.vfat) y los juegos copiados.
  Después se arranca el QEMU SOLO con ese disco: /home, /jogos y /apps montados por etiqueta,
  los juegos están, se escribe en /home, se reinicia y sigue ahí.
Parte 2 (pendrive USB): se instala en un pendrive del EHCI y se arranca desde él (modo usb).
Parte 3 (escritorio): el asistente (capturas H149-*), instalando en un disco de verdad.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_console, criar_disco, extrair_particao, e2fsck, pendrive_qemu,  # noqa: E402
                  APPS_RAIZ, SAIDA)

MB = 1024 * 1024


def sfdisk(img):
    r = subprocess.run(["sfdisk", "-d", img], capture_output=True, text=True).stdout
    t = {}
    for m in re.finditer(r"img(\d+) : start=\s*(\d+), size=\s*(\d+), type=(\w+)(, bootable)?", r):
        t[int(m.group(1))] = (int(m.group(2)), int(m.group(3)), m.group(4), bool(m.group(5)))
    return t


def vazio(nome, mb):
    img = os.path.join(SAIDA, nome + ".img")
    with open(img, "wb") as f:
        f.truncate(mb * MB)
    return img


def reiniciar_e_esperar(m, rotulo):
    m.digitar("reiniciar\n", 0.04)
    fim = time.time() + 60
    while m.proc.poll() is None and time.time() < fim:
        time.sleep(0.5)
    verificar(m.proc.poll() is not None, f"[{rotulo}] reiniciar: el QEMU sale (-no-reboot)")


def comprobar_disco(img, origem, jogo=None, rotulo=""):
    t = sfdisk(img)
    print("tabla:", t)
    verificar(t.get(1) and t[1][0] == 1 and t[1][2] == "da" and t[1][3], f"[{rotulo}] partición 1: SaguiOS (0xDA), activa, desde el LBA 1")
    verificar(t.get(2) and t[2][2] == "f", f"[{rotulo}] partición 2: extendida")
    verificar(t.get(5) and t[5][2] == "83" and t.get(6) and t[6][2] == "83" and t.get(7) and t[7][2] == "83"
              and t.get(8) and t[8][2] == "83" and t[8][1] == 16 * 2048,
              f"[{rotulo}] lógicas: /home, juegos, /apps y SAGUI-CONF de 16 MB (83, ext2; como en la imagen, D194, D256)")
    with open(img, "rb") as f:
        dst = f.read(2 * 512)
        cab = dst[512:1024]
        verificar(cab[:4] == b"SGHD" and cab[144:148] == b"SGIN", f"[{rotulo}] cabecera de arranque con el identificador de instalación (144)")
        verificar(int.from_bytes(cab[136:140], "little") == t[6][1] + t[7][1] + t[8][1],
                  f"[{rotulo}] CAB_SIS_SETORES (136) = las particiones de juegos, de apps y de configuración (D181, D194, D256)")
        e2, total = int.from_bytes(cab[8:12], "little"), int.from_bytes(cab[48:52], "little")
        f.seek(e2 * 512)
        copiado = f.read((total - e2) * 512)
    with open(origem, "rb") as g:
        orig = g.read(total * 512)
    verificar(copiado == orig[e2 * 512:total * 512], f"[{rotulo}] etapa 2, kernel y ramdisk iguales byte a byte al origen")
    verificar(dst[:440] == orig[:440] and dst[510:512] == b"\x55\xAA", f"[{rotulo}] el código del MBR es el de SaguiOS")
    verificar(cab[:136] == orig[512:648], f"[{rotulo}] la cabecera es la del origen (salvo el tamaño y el identificador)")
    home = extrair_particao(img, t[5][0], t[5][1] // 2048)
    rc, o = e2fsck(home)
    verificar(rc == 0, f"[{rotulo}] e2fsck da por bueno el /home ({o.strip()[-120:]})")
    jg = extrair_particao(img, t[6][0], t[6][1] // 2048)
    rc, o = e2fsck(jg)
    verificar(rc == 0 and "SAGUI-JOGOS" in subprocess.run(["/usr/sbin/e2label", jg], capture_output=True, text=True).stdout,
              f"[{rotulo}] la partición de juegos es un ext2 SAGUI-JOGOS sano, como en la imagen (D181)")
    if jogo:
        r = subprocess.run(["/usr/sbin/debugfs", "-R", "cat /opentyrian/tyrian1.lvl", jg], capture_output=True).stdout
        verificar(r == jogo, f"[{rotulo}] los datos de juegos están en opentyrian/ de la partición, byte a byte (D228)")
    r = subprocess.run(["/usr/sbin/debugfs", "-R", "ls /chip8", jg], capture_output=True, text=True).stdout
    verificar(".ch8" in r, f"[{rotulo}] los juegos de la base (chip8) van también a la partición: el montaje en /jogos los tapa (D228)")
    ap = extrair_particao(img, t[7][0], t[7][1] // 2048)
    rc, o = e2fsck(ap)
    verificar(rc == 0 and "SAGUI-APPS" in subprocess.run(["/usr/sbin/e2label", ap], capture_output=True, text=True).stdout,
              f"[{rotulo}] la partición /apps es un ext2 SAGUI-APPS sano, como en la imagen (D194)")
    cf = extrair_particao(img, t[8][0], t[8][1] // 2048)
    rc, o = e2fsck(cf)
    verificar(rc == 0 and "SAGUI-CONF" in subprocess.run(["/usr/sbin/e2label", cf], capture_output=True, text=True).stdout,
              f"[{rotulo}] SAGUI-CONF es un ext2 sano con su etiqueta (D256)")
    verificar(b"SAGUI-CONF" in subprocess.run(["/usr/sbin/debugfs", "-R", "cat /LEEME", cf], capture_output=True).stdout,
              f"[{rotulo}] SAGUI-CONF lleva su LEEME")
    ns = os.path.join(APPS_RAIZ, "bin", "netsurf")
    if os.path.exists(ns):
        r = subprocess.run(["/usr/sbin/debugfs", "-R", "cat /bin/netsurf", ap], capture_output=True).stdout
        verificar(r == open(ns, "rb").read(), f"[{rotulo}] /apps/bin/netsurf copiado byte a byte al disco instalado")
    return t


def arrancar_instalado(img, modo, rotulo, com_jogos):
    with Maquina(f"h149-{rotulo}-a", modo, imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 180)
        s = m.serial()
        verificar("es el disco de arranque (cabecera igual" in s, f"[{rotulo}] el disco instalado se reconoce como el de arranque")
        out = m.comando("mount")
        verificar(re.search(r"\s/home\s", out) and re.search(r"\s/jogos\s", out) and re.search(r"\s/apps\s", out),
                  f"[{rotulo}] /home, /jogos y /apps montados (por etiqueta)")
        verificar(re.search(r"/jogos\s+tipo ext2\s+\(rw", out), f"[{rotulo}] los juegos, ext2 de lectura y escritura como en la imagen (D228)")
        verificar(re.search(r"/apps\s+tipo ext2\s+\(rw", out), f"[{rotulo}] /apps, ext2 de lectura y escritura (D194)")
        out = m.comando("ls /jogos /jogos/opentyrian /apps")
        verificar("chip8" in out, f"[{rotulo}] /jogos/chip8 está (copiado a la partición, D228)")
        if com_jogos:
            verificar("tyrian1.lvl" in out, f"[{rotulo}] los datos de juegos están en /jogos/opentyrian")
        out = m.comando("which sqlite3")
        verificar("/apps/bin/sqlite3" in out, f"[{rotulo}] las apps de /apps están en el disco instalado (which sqlite3)")
        m.comando(f"echo persistente {rotulo} > /home/h149.txt")
        reiniciar_e_esperar(m, rotulo)
    with Maquina(f"h149-{rotulo}-b", modo, imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("cat /home/h149.txt")
        verificar(f"persistente {rotulo}" in out, f"[{rotulo}] /home persiste entre dos arranques")
        verificar(re.search(r"\s/jogos\s", m.comando("mount")), f"[{rotulo}] los juegos siguen montados en el segundo arranque")


def parte1():
    dest = vazio("h149-ide-dest", 512)
    jogo = os.urandom(300000)
    jg, _ = criar_disco("h149-jogos", [dict(tipo="fat32", mb=40, rotulo="JUEGOS", arquivos={"tyrian1.lvl": jogo})])
    origem = imagem_console(apps=True)   # D194: con /apps, que el instalador copia
    extras = ["-drive", f"file={dest},if=ide,index=1,format=raw", "-drive", f"file={jg},if=ide,index=2,format=raw,snapshot=on"]
    with Maquina("h149-ide", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("instalar")
        verificar(re.search(r"hda .*disco de arranque", out) and re.search(r"hdb .*se puede instalar", out),
                  "instalar lista los discos: el de arranque no vale, hdb sí")
        out = m.comando("instalar hda -s")
        verificar("es el disco de arranque" in out, "instalar en el disco de arranque se niega")
        m.comando("mount hdc1 /jogos/opentyrian")   # como la partición de la imagen (D181)
        out = m.comando("instalar hdb --copiar-jogos -s", 1200)
        verificar("SaguiOS instalado en hdb y verificado" in out, f"instalar hdb termina y verifica ({out.strip()[-300:]})")
        verificar(re.search(r"Datos de juegos copiados: (\d+) archivos", out) and int(re.search(r"Datos de juegos copiados: (\d+) archivos", out).group(1)) >= 2,
                  "se copian los datos de /jogos (los del disco en /jogos/opentyrian y los juegos de la base, D228)")
        verificar(re.search(r"Aplicaciones de /apps copiadas: [1-9]\d* archivos", out), "se copian las aplicaciones de /apps (D194)")
        out = m.comando("particiones hdb")
        print(out)
        m.captura("H149-1-terminal")
    comprobar_disco(dest, origem, jogo, "ide")
    arrancar_instalado(dest, "ide", "ide", True)


def parte2():
    dest = vazio("h149-usb-dest", 400)
    origem = imagem_console(apps=True)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(dest)
    with Maquina("h149-usbinst", "ide", imagem=origem, extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        out = m.comando("instalar usb0 -s", 1200)
        verificar("SaguiOS instalado en usb0 y verificado" in out, f"instalar en el pendrive termina ({out.strip()[-300:]})")
    comprobar_disco(dest, origem, None, "usb")
    arrancar_instalado(dest, "usb", "usb", False)


def parte3():
    # D318: el asistente nuevo (ocho pasos); se maneja con las zonas que escribe en el registro
    # (la clase de h318). Camino corto: disco hdb, usar todo el disco y lo que propone.
    from h318_instalador import Asistente
    dest = vazio("h149-gui-dest", 300)
    extras = ["-drive", f"file={dest},if=ide,index=1,format=raw"]
    with Maquina("h149-gui", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 180)
        time.sleep(2)
        a = Asistente(m)
        m.digitar("instalar --ventana\n", 0.04)
        a.esperar_pagina(0)
        a.origen()
        a.pulsar("siguiente")
        a.esperar_pagina(1)
        z = a.zonas()
        verificar("disco1" in z and "disco0" not in z, "el asistente no deja elegir el disco de arranque y sí hdb")
        a.pulsar("disco1")
        m.captura("H149-2-elegir-disco")
        a.pulsar("siguiente")
        a.esperar_pagina(2)
        a.pulsar("tipo-entero")
        a.escribir("confirmar", "hdb")
        a.pulsar("siguiente")
        a.esperar_pagina(3)
        verificar(re.search(r"instalar: plan ok", m.serial()[m.serial().rfind("instalar: pagina 3"):]), "el plan se calcula y se muestra")
        m.captura("H149-3-plan")
        a.pulsar("siguiente")
        a.esperar_pagina(4)
        a.escribir("clave-root", "clave149")   # D371 (P1 b): la de root es obligatoria
        a.escribir("clave-root2", "clave149")
        for pagina in (5, 6):
            a.pulsar("siguiente")
            a.esperar_pagina(pagina)
        a.pulsar("siguiente")                  # «Instalar»
        time.sleep(1.5)
        m.captura("H149-4-confirmar")
        m.tecla("ret")
        m.esperar_serial("instalar: fase 3", 300)
        time.sleep(0.5)
        m.captura("H149-5-progreso")
        m.esperar_serial("instalar: resultado", 900)
        time.sleep(1.5)
        verificar("instalar: resultado 0" in m.serial(), "el asistente termina la instalación sin errores")
        m.captura("H149-6-terminado")
    t = sfdisk(dest)
    verificar(t.get(1) and t[1][2] == "da" and t.get(5), "el disco instalado desde el asistente tiene su tabla")


def main():
    parte1()
    parte2()
    parte3()
    return 0


if __name__ == "__main__":
    sys.exit(main())
