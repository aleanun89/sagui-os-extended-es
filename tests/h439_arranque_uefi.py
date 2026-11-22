#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H439 (D439) - «arranque probar» también arrancando por UEFI (OVMF).

El cargador UEFI (boot/uefi/cargador.c) lee las mismas ranuras de la cabecera del disco que la etapa 2
(D392): borra la marca de «una vez» en el disco ANTES de cargar la prueba y la lee por sus sectores.

Parte A (anfitrión): los cargadores llevan el código nuevo (sin él no hay nada que probar).
Parte B (OVMF x64, imagen híbrida con hueco en la partición de SaguiOS):
  1. «arranque estado» ve el cargador UEFI nuevo; «probar» un kernel distinto; el reinicio arranca la
     prueba UNA vez (y lo dice, también «estado»); el siguiente vuelve al original.
  2. Un «kernel» que se cuelga nada más entrar: ese arranque se queda colgado y el siguiente es el
     original (la marca ya estaba borrada).
  3. Probar y «confirmar»: queda como último bueno; «probar» otro y «cancelar» antes de reiniciar:
     vuelve el último bueno; «original»: vuelve el de la imagen.
  4. Datos de la ranura estropeados: en ese mismo arranque, el original.
  5. El mismo disco preparado por UEFI arranca la prueba por BIOS (SeaBIOS) y el siguiente arranque
     por UEFI ya es el original: el disco se comporta igual con los dos firmwares.
Parte C (sin hueco en la partición, --ranura-mb 0): la prueba va como fichero de la ESP y el cargador
  la lee por sectores (x64 y, si hay OVMF de 32 bits, también IA32).
Lo de BIOS sigue igual: h396.
"""
import os
import struct
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, imagem_de_teste, criar_pendrive, pendrive_qemu, achar_ovmf,  # noqa: E402
                  BUILD, SAIDA)

KERNEL = os.path.join(BUILD, "kernel.bin")
UEFI = os.path.join(BUILD, "boot", "uefi")
# Cada kernel de prueba cambia las mayúsculas de la línea del firmware (mismo tamaño): así el registro
# dice cuál arrancó, por BIOS («[BOOT] firmware=bios») y por UEFI («[BOOT] firmware=uefi-x64 ...»).
NORMAL = (b"[BOOT] firmware=bios\n", b"[BOOT] firmware=uefi-")
MARCAS = {"A": (b"[BOOT] firmware=BIOS\n", b"[BOOT] firmware=UEFI-"),
          "B": (b"[BOOT] firmware=Bios\n", b"[BOOT] firmware=Uefi-")}
AVISO_PRUEBA = "[BOOT] kernel de prueba (arranque probar, D392)"
AVISO_UNA = "esta vez, el siguiente arranque es el ultimo bueno"
AVISO_BOM = "el ultimo bueno (confirmado)"
PR, PB = 256, 392


def cab(img):
    with open(img, "rb") as f:
        f.seek(512)
        return f.read(512)


def kernels():
    k = open(KERNEL, "rb").read()
    for m in NORMAL:
        verificar(k.count(m) == 1, f"el kernel lleva una vez {m!r}")
    ks = {}
    for n, (mb, mu) in MARCAS.items():
        ks[n] = k.replace(NORMAL[0], mb).replace(NORMAL[1], mu)
    cuelga = bytearray(k)
    cuelga[16:18] = b"\xEB\xFE"   # justo tras mov ecx,_bss_fim: jmp $ con las interrupciones cerradas
    return ks, bytes(cuelga)


def qual(m):
    """Qué kernel arrancó: «normal», «A» o «B»."""
    s = m.serial()
    vistos = [n for n, mc in MARCAS.items() if any(x.decode().strip() in s for x in mc)]
    if any(x.decode().strip() in s for x in NORMAL):
        vistos.append("normal")
    verificar(len(vistos) == 1, f"arranca un solo kernel reconocible ({vistos})")
    return vistos[0]


def arrancar(m, tempo=300):
    m.esperar_serial("sagui:/$", tempo)
    out = m.comando("montar", 30)
    if "montado" not in out:
        m.comando("ls /usb", 30)


def arranca(m, rotulo, quem, como, firmware="UEFI"):
    arrancar(m)
    k = qual(m)
    s = m.serial()
    verificar(k == quem, f"{rotulo}: arranca el kernel {quem} (arrancó {k})")
    if firmware == "UEFI":
        verificar("[boot] firmware=uefi-" in s.lower(), f"{rotulo}: arrancó por UEFI")
    if como == "original":
        verificar(AVISO_PRUEBA not in s, f"{rotulo}: es el kernel original, sin aviso de prueba")
    elif como == "una":
        verificar(AVISO_PRUEBA in s and AVISO_UNA in s, f"{rotulo}: arranca como prueba de una vez (y lo dice)")
    else:
        verificar(AVISO_PRUEBA in s and AVISO_BOM in s, f"{rotulo}: arranca como el último bueno (y lo dice)")


def reiniciar(m, rotulo):
    m.comando("sync")
    m.digitar("reiniciar\n", 0.04)
    fim = time.time() + 90
    while m.proc.poll() is None and time.time() < fim:
        time.sleep(0.3)
    verificar(m.proc.poll() is not None, f"[{rotulo}] reiniciar: el QEMU sale (-no-reboot)")


def estado(m):
    return m.comando("arranque estado", 120)


class Disco:
    def __init__(self, img, pen, nome):
        self.img, self.pen, self.nome, self.n = img, pen, nome, 0

    def maquina(self, firmware="x64"):
        self.n += 1
        extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(self.pen)
        return Maquina(f"{self.nome}-{self.n}", "ide", imagem=self.img, extras=extras, snapshot=False, firmware=firmware)


def parte_a():
    for nome in ("BOOTX64.EFI", "BOOTIA32.EFI"):
        efi = open(os.path.join(UEFI, nome), "rb").read()
        verificar("ranura".encode("utf-16-le") in efi and "ultimo bueno".encode("utf-16-le") in efi,
                  f"{nome}: el cargador sabe del kernel de prueba y del último bueno (D439)")


def copia(origem, nome):
    img = os.path.join(SAIDA, nome)
    subprocess.run(["cp", "--sparse=always", origem, img], check=True)
    return img


def parte_b(ks, cuelga):
    origem = imagem_de_teste("", ["--vesa", "--console", "--uefi", UEFI])
    img = copia(origem, "h439-disco.img")
    pen = criar_pendrive("h439-pen", 64, "H439", {"ka.bin": ks["A"], "kb.bin": ks["B"], "kh.bin": cuelga})
    d = Disco(img, pen, "h439")
    imagem = cab(img)[:256]

    # 1. una vez y vuelta al original
    with d.maquina() as m:
        arranca(m, "B1", "normal", "original")
        out = estado(m)
        verificar("Cargador UEFI (x64): sabe arrancar el kernel de prueba" in out and "(por UEFI)" in out,
                  f"B1: estado ve el cargador UEFI nuevo ({out.strip()[-600:]})")
        out = m.comando("arranque probar /usb/ka.bin", 120)
        verificar("Kernel de prueba preparado" in out and "vuelve el original" in out, f"B1: probar A por UEFI ({out.strip()[-400:]})")
        reiniciar(m, "B1")
    verificar(cab(img)[PR + 4] & 1, "B1: la marca de una vez está en el disco")
    with d.maquina() as m:
        arranca(m, "B2", "A", "una")
        verificar(not cab(img)[PR + 4] & 1 and cab(img)[PR + 4] & 2, "B2: el cargador UEFI borró la marca en el disco (y apuntó «arrancada»)")
        out = estado(m)
        verificar("KERNEL DE PRUEBA (una vez)" in out and "(por UEFI)" in out and "ya se arrancó una vez" in out,
                  f"B2: estado lo sabe ({out.strip()[-500:]})")
        reiniciar(m, "B2")
    with d.maquina() as m:
        arranca(m, "B3", "normal", "original")
        # 2. un kernel que se cuelga
        out = m.comando("arranque probar /usb/kh.bin", 120)
        verificar("Kernel de prueba preparado" in out, f"B3: probar acepta el kernel que se cuelga ({out.strip()[-300:]})")
        reiniciar(m, "B3")
    with d.maquina() as m:
        time.sleep(60)
        s = m.serial()
        verificar(m.proc.poll() is None and "[BOOT]" not in s, "B4: el kernel de prueba se cuelga (sin una línea del kernel)")
    verificar(not cab(img)[PR + 4] & 1, "B4: la marca ya no está (se borró antes de cargarlo)")
    with d.maquina() as m:
        arranca(m, "B5", "normal", "original")
        # 3. confirmar, cancelar y original
        out = m.comando("arranque probar /usb/ka.bin", 120)
        verificar("preparado" in out, "B5: probar A otra vez")
        reiniciar(m, "B5")
    with d.maquina() as m:
        arranca(m, "B6", "A", "una")
        out = m.comando("arranque confirmar", 120)
        verificar("es ahora el último bueno" in out, f"B6: confirmar A por UEFI ({out.strip()[-300:]})")
        reiniciar(m, "B6")
    c = cab(img)
    verificar(c[PB:PB + 4] == b"SGPR" and c[PB + 4] == 4 and c[PR:PR + 4] == bytes(4) and c[:256] == imagem,
              "B6: A en la ranura del último bueno; la de prueba vacía; la cabecera de la imagen intacta")
    with d.maquina() as m:
        arranca(m, "B7", "A", "bom")
        out = estado(m)
        verificar("Este arranque: el último bueno" in out, f"B7: estado ve el último bueno ({out.strip()[-400:]})")
        out = m.comando("arranque probar /usb/kb.bin", 120)
        verificar("preparado" in out and "vuelve el último bueno" in out, f"B7: probar B con A fijado ({out.strip()[-300:]})")
        out = m.comando("arranque cancelar", 120)
        verificar("el siguiente arranque es el último bueno" in out, f"B7: cancelar ({out.strip()[-300:]})")
        reiniciar(m, "B7")
    with d.maquina() as m:
        arranca(m, "B8", "A", "bom")
        out = m.comando("arranque original", 120)
        verificar("kernel original" in out, f"B8: arranque original ({out.strip()[-300:]})")
        reiniciar(m, "B8")
    c = cab(img)
    verificar(c[256:] == bytes(256) and c[:256] == imagem, "B8: sin ranuras y la cabecera de la imagen intacta")
    with d.maquina() as m:
        arranca(m, "B9", "normal", "original")
        # 4. datos estropeados
        out = m.comando("arranque probar /usb/kb.bin", 120)
        verificar("preparado" in out, "B9: probar B")
        reiniciar(m, "B9")
    c = cab(img)
    lba = struct.unpack("<I", c[PR + 32:PR + 36])[0]
    with open(img, "r+b") as f:
        f.seek(lba * 512 + 4096)
        b = f.read(1)
        f.seek(lba * 512 + 4096)
        f.write(bytes([b[0] ^ 0x55]))
    with d.maquina() as m:
        arranca(m, "B10", "normal", "original")
        verificar("Kernel de la ranura: datos corruptos o ilegibles" in m.serial(),
                  "B10: el cargador ve la ranura estropeada y arranca el original en ese mismo arranque")
        verificar(not cab(img)[PR + 4] & 1, "B10: la marca se gastó")
        m.comando("arranque cancelar", 120)
        # 5. el mismo disco: preparado por UEFI, arrancado por BIOS
        out = m.comando("arranque probar /usb/ka.bin", 120)
        verificar("preparado" in out, f"B10: probar A ({out.strip()[-300:]})")
        reiniciar(m, "B10")
    with d.maquina(firmware=None) as m:
        arranca(m, "B11", "A", "una", firmware="BIOS")
        verificar("firmware=BIOS" in m.serial(), "B11: por BIOS (SeaBIOS) arranca la prueba que se preparó por UEFI")
        reiniciar(m, "B11")
    with d.maquina() as m:
        arranca(m, "B12", "normal", "original")
        out = estado(m)
        verificar("SUMA MAL" not in out and "Último bueno: ninguno" in out, f"B12: estado, todo bien ({out.strip()[-500:]})")


def parte_c(ks):
    origem = imagem_de_teste("", ["--vesa", "--console", "--uefi", UEFI, "--ranura-mb", "0"])
    img = copia(origem, "h439-esp.img")
    pen = criar_pendrive("h439-pen-c", 64, "H439C", {"ka.bin": ks["A"], "kb.bin": ks["B"]})
    d = Disco(img, pen, "h439c")
    with d.maquina() as m:
        arranca(m, "C1", "normal", "original")
        out = m.comando("arranque probar /usb/ka.bin", 180)
        verificar("en la ESP" in out and "preparado" in out, f"C1: sin sitio en la partición, la prueba va a la ESP ({out.strip()[-400:]})")
        reiniciar(m, "C1")
    verificar(cab(img)[PR + 7] == 1, "C1: la ranura dice ESP (fichero 1)")
    with d.maquina() as m:
        arranca(m, "C2", "A", "una")
        reiniciar(m, "C2")
    fw32 = achar_ovmf("ia32") is not None
    with d.maquina() as m:
        arranca(m, "C3", "normal", "original")
        if fw32:
            out = m.comando("arranque probar /usb/kb.bin", 180)
            verificar("preparado" in out, f"C3: probar B ({out.strip()[-300:]})")
        reiniciar(m, "C3")
    if not fw32:
        print("AVISO: sin OVMF de 32 bits: no se prueba el cargador IA32")
        return
    with d.maquina(firmware="ia32") as m:
        arranca(m, "C4", "B", "una")
        verificar("uefi-ia32" in m.serial().lower(), "C4: por UEFI IA32 arranca la prueba (de la ESP)")
        reiniciar(m, "C4")
    with d.maquina(firmware="ia32") as m:
        arranca(m, "C5", "normal", "original")


def main():
    os.makedirs(SAIDA, exist_ok=True)
    if not achar_ovmf("x64"):
        print("AVISO: sin OVMF de 64 bits (bash tools/entorno-local.sh): H439 no se puede probar")
        return 1
    parte_a()
    ks, cuelga = kernels()
    solo = os.environ.get("H439_PARTE", "BC")
    if "B" in solo:
        parte_b(ks, cuelga)
    if "C" in solo:
        parte_c(ks)
    print("H439: OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())
