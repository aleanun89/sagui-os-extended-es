#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H396 (D392) - «arranque»: un kernel de prueba que se arranca UNA vez, y la etapa 2 nueva.

Parte A (anfitrión): la etapa 2 lleva la marca SGE2; mkimagem.py deja la ranura de la cabecera
  (bytes 256-511) a cero y un hueco libre en la partición de SaguiOS (--ranura-mb).
Parte B (QEMU, disco persistente con la etapa 2 de antes de D392):
  1. «arranque estado» la ve ANTIGUA y «arranque probar» se niega; «arranque etapa2» pone la nueva
     (en sitio libre, y la cabecera apunta a ella) y el disco sigue arrancando.
  2. «arranque probar» con un kernel distinto (la línea «[BOOT] firmware=BIOS» en vez de «bios»):
     el reinicio siguiente arranca la prueba (y lo dice), el otro vuelve al normal.
  3. Un «kernel» que se cuelga nada más entrar (pasa la comprobación de formato): el arranque se
     queda colgado y el siguiente vuelve solo al normal.
  4. Otra vez la prueba y «arranque confirmar»: queda fija (arranca sin la marca de prueba).
  5. «arranque cancelar» quita una marca pendiente.
Parte C (QEMU): una imagen sin hueco en la partición (--ranura-mb 0) y con ESP (--uefi): la prueba va
  como fichero de la ESP (FAT32) y la etapa 2 la arranca igual; confirmar la deja fijada.
"""
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_de_teste, criar_pendrive, pendrive_qemu, BUILD, RAIZ, SAIDA)  # noqa: E402

KERNEL = os.path.join(BUILD, "kernel.bin")
E2 = os.path.join(BUILD, "boot", "estagio2.bin")
MARCA_NORMAL = b"[BOOT] firmware=bios\n"
# tres kernels de prueba distintos: la misma línea con otras mayúsculas (mismo tamaño)
MARCAS = {"A": b"[BOOT] firmware=BIOS\n", "B": b"[BOOT] firmware=Bios\n", "C": b"[BOOT] firmware=bIOS\n"}
AVISO_PRUEBA = "[BOOT] kernel de prueba (arranque probar, D392)"
AVISO_UNA = "esta vez, el siguiente arranque es el ultimo bueno"
AVISO_BOM = "el ultimo bueno (confirmado)"
PR, PB = 256, 392


def cab(img):
    with open(img, "rb") as f:
        f.seek(512)
        return f.read(512)


def etapa2_vieja():
    """La etapa 2 de antes de D392 (el padre del primer commit que la cambió), ensamblada aparte."""
    nasm = shutil.which("nasm") or os.path.expanduser("~/.local/saguitools/bin/nasm")
    verificar(os.path.exists(nasm), "hay nasm para ensamblar la etapa 2 vieja")
    rev = subprocess.run(["git", "-C", RAIZ, "log", "--format=%H", "--grep=D392", "--reverse", "--", "boot/estagio2.asm"],
                         capture_output=True, text=True).stdout.split()
    verificar(rev, "se encuentra el commit de D392 que cambió la etapa 2")
    d = tempfile.mkdtemp(dir=SAIDA)
    for f in ("estagio2.asm", "cabecalho.inc", "bootinfo.inc"):
        with open(os.path.join(d, f), "wb") as g:
            g.write(subprocess.run(["git", "-C", RAIZ, "show", f"{rev[0]}^:boot/{f}"], capture_output=True, check=True).stdout)
    out = os.path.join(d, "estagio2-vieja.bin")
    subprocess.run([nasm, "-f", "bin", "-I", d + "/", "-o", out, os.path.join(d, "estagio2.asm")], check=True)
    return open(out, "rb").read()


def poner_etapa2_vieja(img, vieja):
    """La imagen como un disco grabado antes de D392: la etapa 2 vieja en su sitio."""
    c = bytearray(cab(img))
    lba, n = struct.unpack("<II", c[8:16])
    nv = (len(vieja) + 511) // 512
    verificar(nv <= n, f"la etapa 2 vieja ({nv} sectores) cabe donde la nueva ({n})")
    with open(img, "r+b") as f:
        f.seek(lba * 512)
        f.write(vieja.ljust(n * 512, b"\0"))
        c[12:16] = struct.pack("<I", nv)
        f.seek(512)
        f.write(bytes(c))


def kernels():
    k = open(KERNEL, "rb").read()
    verificar(k.count(MARCA_NORMAL) == 1, "el kernel lleva una vez «[BOOT] firmware=bios»")
    ks = {n: k.replace(MARCA_NORMAL, m) for n, m in MARCAS.items()}
    cuelga = bytearray(k)
    cuelga[16:18] = b"\xEB\xFE"   # justo tras mov ecx,_bss_fim: jmp $ con las interrupciones cerradas
    return ks, bytes(cuelga)


def qual(m):
    """Qué kernel arrancó esta máquina: «normal», «A», «B» o «C»."""
    s = m.serial()
    vistos = [n for n, mc in MARCAS.items() if mc.decode().strip() in s]
    if MARCA_NORMAL.decode().strip() in s:
        vistos.append("normal")
    verificar(len(vistos) == 1, f"arranca un solo kernel reconocible ({vistos})")
    return vistos[0]


def arranca(m, rotulo, quem, como):
    """Comprueba que arrancó «quem» y cómo: «original», «una» (prueba una vez) o «bom» (último bueno)."""
    arrancar(m, rotulo)
    k = qual(m)
    s = m.serial()
    verificar(k == quem, f"{rotulo}: arranca el kernel {quem} (arrancó {k})")
    if como == "original":
        verificar(AVISO_PRUEBA not in s, f"{rotulo}: es el kernel original, sin aviso de prueba")
    elif como == "una":
        verificar(AVISO_PRUEBA in s and AVISO_UNA in s, f"{rotulo}: arranca como prueba de una vez (y lo dice)")
    else:
        verificar(AVISO_PRUEBA in s and AVISO_BOM in s, f"{rotulo}: arranca como el último bueno (y lo dice)")


def poner_byte(img, pos, val):
    with open(img, "r+b") as f:
        f.seek(512 + pos)
        f.write(bytes([val]))


def reiniciar(m, rotulo):
    m.comando("sync")
    m.digitar("reiniciar\n", 0.04)
    fim = time.time() + 90
    while m.proc.poll() is None and time.time() < fim:
        time.sleep(0.3)
    verificar(m.proc.poll() is not None, f"[{rotulo}] reiniciar: el QEMU sale (-no-reboot)")


class Disco:
    def __init__(self, img, pen, nome):
        self.img, self.pen, self.nome, self.n = img, pen, nome, 0

    def maquina(self):
        self.n += 1
        extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(self.pen)
        return Maquina(f"{self.nome}-{self.n}", "ide", imagem=self.img, extras=extras, snapshot=False)


def arrancar(m, rotulo, tempo=240):
    m.esperar_serial("sagui:/$", tempo)
    out = m.comando("montar", 30)
    if "montado" not in out:
        m.comando("ls /usb", 30)


def estado(m):
    return m.comando("arranque estado", 120)


def parte_a():
    e2 = open(E2, "rb").read()
    verificar(e2[0] == 0xEB and e2[2:6] == b"SGE2", "la etapa 2 empieza por un salto y la marca SGE2 (D392)")
    verificar(len(e2) <= 32768, f"la etapa 2 cabe en 32 KB ({len(e2)} bytes)")
    img = imagem_de_teste("", ["--vesa", "--console"])
    c = cab(img)
    verificar(c[256:] == bytes(256), "mkimagem deja a cero la ranura de prueba (bytes 256-511)")
    with open(img, "rb") as f:
        mbr = f.read(512)
    ini, n = struct.unpack("<II", mbr[446 + 8:446 + 16])
    total = struct.unpack("<I", c[48:52])[0]
    k_set, r_set = struct.unpack("<I", c[20:24])[0], struct.unpack("<I", c[36:40])[0]
    verificar(mbr[446 + 4] == 0xDA and ini + n - total >= k_set + r_set,
              f"la partición de SaguiOS tiene hueco para un kernel y un ramdisk de prueba ({ini + n - total} sectores libres)")


def parte_b(ks, cuelga, vieja):
    """Partición de SaguiOS (la ranura en su hueco)."""
    origem = imagem_de_teste("", ["--vesa", "--console"])
    img = os.path.join(SAIDA, "h396-disco.img")
    subprocess.run(["cp", "--sparse=always", origem, img], check=True)
    poner_etapa2_vieja(img, vieja)
    pen = criar_pendrive("h396-pen", 64, "H396", {"ka.bin": ks["A"], "kb.bin": ks["B"], "kcc.bin": ks["C"], "kh.bin": cuelga,
                                                  "e2.bin": open(E2, "rb").read()})
    d = Disco(img, pen, "h396")

    def imagem_de(c):
        """Kernel y ramdisk de la imagen (16-47 y 52-63; no CAB_IMG_SETORES, que cambia «etapa2»)."""
        return c[16:48] + c[52:64]
    imagem = imagem_de(cab(img))

    # 1. etapa 2 vieja: probar se niega; etapa2 la cambia
    with d.maquina() as m:
        arrancar(m, "B1")
        out = estado(m)
        verificar("ANTIGUA" in out and "el kernel original" in out, f"B1: estado ve la etapa 2 antigua ({out.strip()[-500:]})")
        out = m.comando("arranque probar /usb/ka.bin", 120)
        verificar("es antigua" in out, f"B1: probar se niega con la etapa 2 antigua ({out.strip()[-300:]})")
        e2_antes = struct.unpack("<I", cab(img)[8:12])[0]
        out = m.comando("arranque etapa2 /usb/e2.bin", 120)
        verificar("Etapa 2 nueva" in out, f"B1: arranque etapa2 la pone ({out.strip()[-300:]})")
        reiniciar(m, "B1")
    e2_lba = struct.unpack("<I", cab(img)[8:12])[0]
    verificar(e2_lba != e2_antes, f"B1: la cabecera apunta a la etapa 2 nueva en otro sitio (LBA {e2_antes} -> {e2_lba})")
    with open(img, "rb") as f:
        f.seek(e2_lba * 512)
        verificar(f.read(6)[2:6] == b"SGE2", "B1: en ese sitio está la etapa 2 nueva")

    # 2. probar A y confirmarlo: queda como último bueno (la cabecera de la imagen no cambia)
    with d.maquina() as m:
        arranca(m, "B2", "normal", "original")
        out = estado(m)
        verificar("sabe arrancar el kernel de prueba y el último bueno" in out, f"B2: estado ve la etapa 2 nueva ({out.strip()[-400:]})")
        out = m.comando("arranque probar /usb/ka.bin", 120)
        verificar("Kernel de prueba preparado" in out and "vuelve el original" in out, f"B2: probar prepara A ({out.strip()[-400:]})")
        verificar("PENDIENTE" in estado(m), "B2: estado la ve pendiente")
        reiniciar(m, "B2")
    verificar(cab(img)[PR + 4] & 1, "B2: la marca de una vez está en el disco")
    with d.maquina() as m:
        arranca(m, "B3", "A", "una")
        out = estado(m)
        verificar("KERNEL DE PRUEBA (una vez)" in out and "ya se arrancó una vez" in out, f"B3: estado lo sabe ({out.strip()[-400:]})")
        out = m.comando("arranque confirmar", 120)
        verificar("es ahora el último bueno" in out, f"B3: confirmar A ({out.strip()[-300:]})")
        reiniciar(m, "B3")
    c = cab(img)
    verificar(c[PB:PB + 4] == b"SGPR" and c[PB + 4] == 4 and c[PR:PR + 4] == bytes(4),
              "B3: A está en la ranura del último bueno (fijado) y la de prueba, vacía")
    verificar(imagem_de(c) == imagem, "B3: la cabecera de la imagen (kernel y ramdisk originales) no cambia")

    # 3. probar B sin confirmar: arranca una vez y el siguiente vuelve A (no el original)
    with d.maquina() as m:
        arranca(m, "B4", "A", "bom")
        out = estado(m)
        verificar("Este arranque: el último bueno" in out and "Último bueno: confirmado" in out and "ka.bin" in out,
                  f"B4: estado ve A como último bueno ({out.strip()[-600:]})")
        out = m.comando("arranque probar /usb/kb.bin", 120)
        verificar("Kernel de prueba preparado" in out and "vuelve el último bueno" in out, f"B4: probar B con A fijado ({out.strip()[-400:]})")
        reiniciar(m, "B4")
    with d.maquina() as m:
        arranca(m, "B5", "B", "una")
        reiniciar(m, "B5")
    with d.maquina() as m:
        arranca(m, "B6", "A", "bom")
        # 4. un kernel que se cuelga: el siguiente arranque, A
        out = m.comando("arranque probar /usb/kh.bin", 120)
        verificar("Kernel de prueba preparado" in out, f"B6: probar acepta el kernel que se cuelga ({out.strip()[-300:]})")
        reiniciar(m, "B6")
    with d.maquina() as m:
        time.sleep(40)
        verificar(m.proc.poll() is None and "[BOOT]" not in m.serial(), "B7: el kernel de prueba se cuelga (sin una línea del kernel)")
    with d.maquina() as m:
        arranca(m, "B8", "A", "bom")
        # 5. una prueba con los datos estropeados: en ese mismo arranque, A
        out = m.comando("arranque probar /usb/kb.bin", 120)
        verificar("preparado" in out, "B8: probar B otra vez")
        reiniciar(m, "B8")
    c = cab(img)
    lba = struct.unpack("<I", c[PR + 32:PR + 36])[0]
    with open(img, "r+b") as f:
        f.seek(lba * 512 + 4096)
        b = f.read(1)
        f.seek(lba * 512 + 4096)
        f.write(bytes([b[0] ^ 0x55]))
    with d.maquina() as m:
        arranca(m, "B9", "A", "bom")
        verificar(not cab(img)[PR + 4] & 1, "B9: la marca de una vez se gastó")
        # 6. probar C y confirmarlo: C pasa a ser el último bueno
        out = m.comando("arranque probar /usb/kcc.bin", 120)
        verificar("preparado" in out, "B9: probar C")
        reiniciar(m, "B9")
    with d.maquina() as m:
        arranca(m, "B10", "C", "una")
        out = m.comando("arranque confirmar", 120)
        verificar("es ahora el último bueno" in out, f"B10: confirmar C ({out.strip()[-300:]})")
        reiniciar(m, "B10")
    with d.maquina() as m:
        arranca(m, "B11", "C", "bom")
        out = estado(m)
        verificar("kcc.bin" in out and "SUMA MAL" not in out and "En prueba: ninguno" in out, f"B11: estado, C fijo ({out.strip()[-600:]})")
        out = m.comando("arranque cancelar", 120)
        verificar("No hay kernel de prueba" in out, f"B11: cancelar sin prueba no cambia nada ({out.strip()[-200:]})")
        # 7. original: vuelve el de la imagen
        out = m.comando("arranque original", 120)
        verificar("kernel original" in out, f"B11: arranque original ({out.strip()[-300:]})")
        reiniciar(m, "B11")
    c = cab(img)
    verificar(c[256:] == bytes(256) and imagem_de(c) == imagem, "B11: sin ranuras y la cabecera de la imagen intacta")
    with d.maquina() as m:
        arranca(m, "B12", "normal", "original")
        out = estado(m)
        verificar("SUMA MAL" not in out and "Último bueno: ninguno" in out and "En prueba: ninguno" in out,
                  f"B12: estado, todo bien ({out.strip()[-500:]})")


def parte_c(ks):
    """Sin hueco en la partición (--ranura-mb 0): las dos ranuras como ficheros de la ESP. Empieza con
    un fijado del primer «arranque confirmar» (PR_FIXA en la ranura de prueba), que pasa a último bueno."""
    origem = imagem_de_teste("", ["--vesa", "--console", "--uefi", os.path.join(BUILD, "boot", "uefi"), "--ranura-mb", "0"])
    img = os.path.join(SAIDA, "h396-esp.img")
    subprocess.run(["cp", "--sparse=always", origem, img], check=True)
    pen = criar_pendrive("h396-pen-c", 64, "H396C", {"ka.bin": ks["A"], "kb.bin": ks["B"], "kcc.bin": ks["C"]})
    d = Disco(img, pen, "h396c")
    with d.maquina() as m:
        arrancar(m, "C1")
        out = m.comando("arranque probar /usb/ka.bin", 180)
        verificar("en la ESP" in out and "preparado" in out, f"C1: sin sitio en la partición, la prueba va a la ESP ({out.strip()[-400:]})")
        reiniciar(m, "C1")
    verificar(cab(img)[PR + 7] == 1, "C1: la ranura dice ESP (fichero 1)")
    poner_byte(img, PR + 4, 2 | 4)   # como lo dejaba el primer «confirmar» en la ESP
    with d.maquina() as m:
        arranca(m, "C2", "A", "bom")
        out = estado(m)
        verificar("formato de antes" in out, f"C2: estado ve el fijado de antes como último bueno ({out.strip()[-500:]})")
        out = m.comando("arranque probar /usb/kb.bin", 180)
        verificar("sagui-prueba2-kernel.bin" in out and "preparado" in out, f"C2: B va al otro fichero de la ESP ({out.strip()[-400:]})")
        reiniciar(m, "C2")
    c = cab(img)
    verificar(c[PB:PB + 4] == b"SGPR" and c[PB + 4] == 4 and c[PB + 7] == 1 and c[PR + 7] == 2 and c[PR + 4] == 1,
              "C2: A pasó a la ranura del último bueno (fichero 1) y B está pendiente (fichero 2)")
    with d.maquina() as m:
        arranca(m, "C3", "B", "una")
        reiniciar(m, "C3")
    with d.maquina() as m:
        arranca(m, "C4", "A", "bom")
        out = m.comando("arranque probar /usb/kcc.bin", 180)
        verificar("preparado" in out, f"C4: probar C ({out.strip()[-300:]})")
        reiniciar(m, "C4")
    with d.maquina() as m:
        arranca(m, "C5", "C", "una")
        out = m.comando("arranque confirmar", 120)
        verificar("es ahora el último bueno" in out, f"C5: confirmar C ({out.strip()[-300:]})")
        reiniciar(m, "C5")
    verificar(cab(img)[PB + 7] == 2, "C5: el último bueno es C, en el fichero 2")
    with d.maquina() as m:
        arranca(m, "C6", "C", "bom")
        out = m.comando("arranque original", 120)
        verificar("kernel original" in out, f"C6: arranque original ({out.strip()[-300:]})")
        reiniciar(m, "C6")
    with d.maquina() as m:
        arranca(m, "C7", "normal", "original")


def main():
    os.makedirs(SAIDA, exist_ok=True)
    parte_a()
    ks, cuelga = kernels()
    vieja = etapa2_vieja()
    solo = os.environ.get("H396_PARTE", "BC")
    if "B" in solo:
        parte_b(ks, cuelga, vieja)
    if "C" in solo:
        parte_c(ks)
    print("H396: OK")


if __name__ == "__main__":
    main()
