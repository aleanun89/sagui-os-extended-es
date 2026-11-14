#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H93 (D120, mejora 068) - particiones extendidas y lógicas del MBR.

Imagen de mkimagem con --esquema mixto (el antiguo --extendida; desde la v7.3, D213, lo normal es
que todas sean lógicas, pero las imágenes de antes siguen arrancando): arranque + /home primarias y /tmp, /logs y una extra
(SAGUI-X1) como lógicas dentro de una extendida (FAT32 y ext2 mezcladas). En QEMU como pendrive:
  - "discos" las lista (usb0p5, p6, p7) y la extendida sale como «extendida (3 lógicas)»;
  - el fstab monta /tmp y /logs por etiqueta aunque sean lógicas; la tercera se monta a mano;
  - se escribe, se apaga, el anfitrión comprueba el contenido y fsck limpios.
Defensas, con copias dañadas de la imagen: cadena de EBR en bucle, enlace fuera del disco y lógica con
tamaño incoherente: el sistema arranca, avisa en el registro y sigue con lo demás (nunca se cuelga).
"""
import os
import re
import shutil
import struct
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, extrair_particao, e2fsck, debugfs, RAIZ, SAIDA  # noqa: E402
from h91_particiones import apagar, B, ENV_FAT  # noqa: E402


def tabela(img):
    """Las entradas usadas del MBR (solo las primarias y la extendida)."""
    mbr = open(img, "rb").read(512)
    return [struct.unpack("<II", mbr[446 + 16 * k + 8:446 + 16 * k + 16]) for k in range(4) if mbr[446 + 16 * k + 4]]


def criar(nome):
    img = os.path.join(SAIDA, nome + ".img")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkimagem.py"),
                    "--mbr", f"{B}/boot/mbr.bin", "--estagio2", f"{B}/boot/estagio2.bin", "--kernel", f"{B}/kernel.bin",
                    "--kernel-elf", f"{B}/kernel.elf", "--ramdisk", f"{B}/ramdisk.sgfs", "--saida", img,
                    "--vesa", "--console", "--part-fs", "fat32", "--tmp-fs", "ext2", "--home-mb", "40",
                    "--tmp-mb", "8", "--logs-mb", "34", "--extra-mb", "40", "--esquema", "mixto"],
                   check=True, stdout=subprocess.DEVNULL)
    return img


def logicas(img):
    """[(inicio, mb)] de las lógicas leyendo la cadena de EBR desde el anfitrión."""
    f = open(img, "rb")
    mbr = f.read(512)
    ext = struct.unpack("<I", mbr[446 + 32 + 8:446 + 32 + 12])[0]
    r, rel = [], 0
    while True:
        f.seek((ext + rel) * 512)
        e = f.read(512)
        ini, qtd = struct.unpack("<II", e[454:462])
        r.append((ext + rel + ini, qtd // 2048))
        if e[462 + 4] not in (5, 0xF):
            return r
        rel = struct.unpack("<I", e[462 + 8:462 + 12])[0]


def pegar(img, k, ruta, fs):
    ini, mb = logicas(img)[k]
    p = extrair_particao(img, ini, mb)
    try:
        if fs == "ext2":
            return debugfs(p, f"cat /{ruta}")
        return subprocess.run(["mtype", "-i", p, f"::/{ruta}"], capture_output=True, env=ENV_FAT).stdout
    finally:
        os.remove(p)


def fsck(img, k, fs):
    ini, mb = logicas(img)[k]
    p = extrair_particao(img, ini, mb)
    if fs == "ext2":
        rc, out = e2fsck(p)
    else:
        r = subprocess.run(["fsck.vfat", "-n", p], capture_output=True, text=True)
        rc, out = r.returncode, r.stdout + r.stderr
    os.remove(p)
    verificar(rc == 0, f"fsck limpio en la lógica {k + 5} ({fs})" + ("" if rc == 0 else "\n" + out))


def sector_ebr(img, n):
    """Sector (LBA) del EBR número n (0 = el primero)."""
    f = open(img, "rb")
    mbr = f.read(512)
    ext = struct.unpack("<I", mbr[446 + 32 + 8:446 + 32 + 12])[0]
    rel = 0
    for _ in range(n):
        f.seek((ext + rel) * 512)
        rel = struct.unpack("<I", f.read(512)[462 + 8:462 + 12])[0]
    return ext, ext + rel


def tocar(img, n, desloc, valor):
    ext, ebr = sector_ebr(img, n)
    with open(img, "r+b") as f:
        f.seek(ebr * 512 + desloc)
        f.write(struct.pack("<I", valor))


def principal():
    img = criar("h93-ext")
    t = tabela(img)
    verificar(len(t) == 3, "imagen con arranque, /home y una extendida")
    verificar(len(logicas(img)) == 3, "la extendida lleva 3 lógicas")
    with Maquina("h93a", "usb", imagem=img, snapshot=False) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        out = m.comando("discos")
        for n in ("usb0p5", "usb0p6", "usb0p7"):
            verificar(n in out, f"discos lista {n}")
        verificar("extendida (3 lógicas)" in out, "la extendida sale como «extendida (3 lógicas)»")
        verificar("SAGUI-TMP" in out and "SAGUI-LOGS" in out and "SAGUI-X1" in out, "las etiquetas de las lógicas se leen")
        for disp, ponto in (("LABEL=SAGUI-TMP", "/tmp"), ("LABEL=SAGUI-LOGS", "/logs")):
            verificar(re.search(rf"\[FSTAB\] {disp} -> {ponto} \(auto, rw\): 0", s) is not None, f"el fstab monta la lógica {disp} en {ponto}")
        out = m.comando("mount usb0p7 /extra")
        verificar("montado en /extra" in out, "una lógica se monta a mano")
        m.comando("echo tmp-logica > /tmp/h93.txt; echo logs-logica > /logs/h93.txt; echo extra-logica > /extra/h93.txt")
        out = m.comando("cat /tmp/h93.txt /logs/h93.txt /extra/h93.txt")
        verificar("tmp-logica" in out and "logs-logica" in out and "extra-logica" in out, "se escribe y se lee en las lógicas")
        out = m.comando("mount usb0p3 /e")
        verificar("montado" not in out, "la extendida en sí no es montable")
        m.comando("umount /extra")
        apagar(m)
    verificar(pegar(img, 0, "h93.txt", "ext2").strip() == b"tmp-logica", "lo escrito en /tmp (ext2, lógica) está en la imagen")
    verificar(pegar(img, 1, "h93.txt", "fat32").strip() == b"logs-logica", "lo escrito en /logs (FAT32, lógica) está en la imagen")
    verificar(pegar(img, 2, "h93.txt", "fat32").strip() == b"extra-logica", "lo escrito en la tercera lógica está en la imagen")
    log = [n for n in re.findall(rb"kernel-\d{8}-\d{4}\.log", subprocess.run(
        ["mdir", "-b", "-i", extrair_particao(img, logicas(img)[1][0], 34), "::/"], capture_output=True, env=ENV_FAT).stdout)]
    verificar(log, "hay un registro del kernel en /logs (lógica)")
    for k, fs in ((0, "ext2"), (1, "fat32"), (2, "fat32")):
        fsck(img, k, fs)
    return img


def danada(base, nombre, que, esperado, minimo_logicas):
    img = os.path.join(SAIDA, nombre + ".img")
    shutil.copyfile(base, img)
    que(img)
    with Maquina(nombre, "usb", imagem=img, snapshot=True) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        verificar(esperado in s, f"{nombre}: el registro avisa («{esperado}»)")
        out = m.comando("discos")
        verificar("usb0p5" in out, f"{nombre}: el sistema arranca y lista la primera lógica")
        verificar(("usb0p%d" % (4 + minimo_logicas)) in out, f"{nombre}: quedan {minimo_logicas} lógicas válidas")
        verificar("PANICO" not in s, f"{nombre}: sin pánico")


def main():
    base = principal()
    # bucle: el enlace del 2.º EBR apunta otra vez al 1.º
    def bucle(img):
        ext, ebr2 = sector_ebr(img, 1)
        with open(img, "r+b") as f:
            f.seek(ebr2 * 512 + 462 + 4); f.write(b"\x05")
            f.seek(ebr2 * 512 + 462 + 8); f.write(struct.pack("<I", 0))
    danada(base, "h93-bucle", bucle, "bucle en la cadena de EBR", 2)

    def fuera(img):
        ext, ebr2 = sector_ebr(img, 1)
        tocar(img, 1, 462 + 8, 0x7FFFFFF0)
    danada(base, "h93-fuera", fuera, "cadena de EBR rota", 2)

    def incoherente(img):
        tocar(img, 1, 446 + 12, 0x7FFFFFF0)   # la 2.ª lógica "mide" una barbaridad
    danada(base, "h93-tam", incoherente, "partición lógica incoherente", 2)   # la 2.ª se ignora, la 3.ª pasa a ser usb0p6
    print("H93 OK")


if __name__ == "__main__":
    main()
