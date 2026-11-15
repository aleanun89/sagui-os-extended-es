#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H287 - 3D por la GPU en las Radeon R5xx (X1300 a X1950), sobre la capa gpu3d (D287).

QEMU no emula ninguna Radeon, así que el driver de verdad
(kernel/dispositivos/gpu3d_r500.c) solo se puede probar en una máquina con una
R5xx (el Amilo con la Mobility X1400). Aquí se prueba lo que sí se puede:

  unidad    en el PC (tests/r500_3d_unidad.c): las órdenes del CP recorridas como
            lo haría la tarjeta, el formato del vértice, el programa de vértices y
            los dos de fragmentos contra sus palabras escritas a mano, el destino,
            el Z, la mezcla, la textura, la valla y los triángulos;
  fuentes   lo que tiene que estar en el árbol: lista de modelos, esperas con
            plazo, sin bus maestro ni reinicios, el microcódigo con su licencia y
            su SHA-256, la opción --gpu3d=radeon, D287 y los documentos;
  firmware  el microcódigo entra en la imagen (y no con SEM_FIRMWARE=1, que deja
            fuera FIRMWARE_RADEON);
  std       QEMU normal (sin Radeon): la capa no elige el driver de R5xx ni toca
            nada, también con --gpu3d=radeon, y SaguiGL por software sigue igual.

Uso: h287_gpu3d_r500.py [unidad] [fuentes] [firmware] [std]
"""
import hashlib
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(RAIZ, "build", "h287")
SHA_UCODE = "830ce72c50d53c0f324242cdffede681deae05e9b9778d7e75764ffc37647d22"
SHA_LIC = "fc6223d4bfe9f2f9e2eddc44b9fe5721d0caf49f01cb08d602906add686d8c6f"


def ler(caminho):
    with open(os.path.join(RAIZ, caminho), encoding="utf-8") as f:
        return f.read()


def sha(caminho):
    with open(os.path.join(RAIZ, caminho), "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()


def arrancar(m, tempo=240):
    fim = time.time() + tempo
    while time.time() < fim:
        s = m.serial()
        if "area de trabalho no ar" in s or "sagui:/$" in s:
            break
        if m.proc.poll() is not None:
            raise FalhaTeste("el QEMU terminó durante el arranque")
        time.sleep(0.5)
    else:
        raise FalhaTeste("el escritorio no llegó")
    time.sleep(3)


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "r500_3d_unidad")
    r = subprocess.run(["gcc", "-O1", "-fno-builtin", "-Wall", "-Wno-builtin-declaration-mismatch",
                        "-I", os.path.join(RAIZ, "kernel", "inc"), "-I", os.path.join(RAIZ, "kernel", "dispositivos"),
                        "-o", exe, os.path.join(RAIZ, "tests", "r500_3d_unidad.c"),
                        os.path.join(RAIZ, "kernel", "gui", "gpu3d_mem.c")], capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", r.stdout.strip().replace("\n", "\n    ")[-600:])
    verificar(r.returncode == 0 and re.search(r"\d+ comprobaciones, 0 fallos", r.stdout),
              "órdenes del CP de R5xx, formato del vértice, programa de vértices y programas de fragmentos")


def fuentes():
    drv = ler("kernel/dispositivos/gpu3d_r500.c")
    verificar("0x7140, 0x715F, F_RV515" in drv and "familia_de(d->modelo)" in drv and "d->fabricante == 0x1002" in drv,
              "gpu3d_r500.c: lista de modelos R5xx (con la X1400, 0x7145) comprobada antes de tocar nada")
    verificar("0x791E" not in drv and "0x7941" not in drv, "gpu3d_r500.c: las integradas RS600/RS690 no están en la lista")
    verificar("while (1)" not in drv and "for (;;)" not in drv and drv.count("prazo_vencido") >= 4,
              "gpu3d_r500.c: esperas con plazo")
    verificar("pci_escreve32" not in drv, "gpu3d_r500.c: no cambia el registro de órdenes del PCI (sin bus maestro)")
    verificar("wr(SCRATCH_UMSK, 0)" in drv and "RB_SEM_ATUALIZAR" in drv,
              "gpu3d_r500.c: sin escrituras de vuelta de la tarjeta a la memoria")
    verificar("SOFT_RESET" not in drv.replace("RBBM_SOFT_RESET;", ""), "gpu3d_r500.c: no reinicia el motor")
    verificar("SEM_FIRMWARE" in drv and "/lib/firmware/radeon/R520_cp.bin" in drv,
              "gpu3d_r500.c: el microcódigo es opcional (sin él, software)")
    verificar(drv.count("[GPU3D] r500: etapa") >= 7 and "r5_detalle" in drv, "gpu3d_r500.c: diagnóstico por etapas y volcado de registros")
    pac = ler("kernel/dispositivos/r500_pacotes.h")
    verificar("GPL-2.0-or-later" in pac.split("\n")[0] and "MIT" in pac, "r500_pacotes.h: código propio con sus fuentes anotadas")
    for f in ("kernel/dispositivos/gpu3d_r500.c", "kernel/dispositivos/r500_pacotes.h", "tests/r500_3d_unidad.c"):
        t = ler(f)
        verificar("SPDX-License-Identifier: GPL-2.0-or-later" in t.split("\n")[0], f"{f}: marca SPDX")
        verificar("GPL-2.0-only" not in t, f"{f}: nada «GPL-2.0-only»")
    capa = ler("kernel/gui/gpu3d.c")
    verificar("&gpu3d_r500" in capa and "GPU3D_CAB_RADEON" in capa and "estado = GPU3D_APAGADO;" in capa,
              "gpu3d.c: elige el driver de R5xx y lo deja apagado salvo --gpu3d=radeon")
    verificar('"radeon": 4' in ler("tools/mkimagem.py") and "GPU3D_CAB_RADEON    4" in ler("kernel/inc/gpu3d.h"),
              "la opción --gpu3d=radeon (4) en mkimagem.py y en gpu3d.h")
    verificar(sha("firmware/radeon/R520_cp.bin") == SHA_UCODE and os.path.getsize(os.path.join(RAIZ, "firmware/radeon/R520_cp.bin")) == 2048,
              "firmware/radeon/R520_cp.bin: 2048 bytes con su SHA-256")
    verificar(sha("firmware/radeon/LICENSE.radeon.txt") == SHA_LIC, "firmware/radeon/LICENSE.radeon.txt con su SHA-256")
    verificar(SHA_UCODE in ler("tools/firmware_r5xx.sh"), "tools/firmware_r5xx.sh lleva el SHA-256 del microcódigo")
    ter = ler("docs/terceros.md")
    verificar("r500_pacotes.h" in ter and "firmware/radeon/R520_cp.bin" in ter, "docs/terceros.md registra las referencias y el microcódigo")
    verificar("## D287." in ler("docs/decisoes.md"), "la decisión D287 está en docs/decisoes.md")
    doc = ler("docs/gpu3d.md")
    verificar("Radeon R5xx" in doc and "Qué probar en el Amilo" in doc, "docs/gpu3d.md: el driver de R5xx y qué probar en el Amilo")
    verificar(re.search(r"Radeon R5xx.*D287", ler("docs/gpus.md")), "docs/gpus.md: la columna 3D de R5xx")


def firmware():
    mk = ler("Makefile")
    verificar("FIRMWARE_RADEON := $(wildcard firmware/radeon/*.bin)" in mk and re.search(r"SEM_FIRMWARE\),1\)\nFIRMWARE_RAMDISK :=\nFIRMWARE_RADEON :=\n", mk),
              "Makefile: el microcódigo entra en el ramdisk y SEM_FIRMWARE=1 lo deja fuera")
    rf = os.path.join(RAIZ, "build", "rootfs", "lib", "firmware", "radeon", "R520_cp.bin")
    verificar(os.path.exists(rf), "el ramdisk lleva /lib/firmware/radeon/R520_cp.bin")
    with open(rf, "rb") as f:
        verificar(hashlib.sha256(f.read()).hexdigest() == SHA_UCODE, "y es el de firmware/radeon/")
    verificar(os.path.exists(os.path.join(os.path.dirname(rf), "LICENSE.radeon.txt")), "con su licencia al lado")


def std():
    for opcoes, nome in ((["--vesa"], "h287s"), (["--vesa", "--gpu3d=radeon"], "h287r")):
        img = imagem_de_teste("", opcoes)
        with Maquina(nome, "ide", imagem=img) as m:
            arrancar(m)
            s = m.serial()
            que = " ".join(opcoes)
            verificar("[GPU3D] software:" in s and "r500" not in s and "[GPU3D] encender" not in s,
                      f"{que}: sin Radeon R5xx la capa no elige su driver ni enciende nada")
            out = m.comando("gpu3d", 30)
            verificar("3D por GPU: sin GPU con driver 3D (software)" in out, f"{que}: «gpu3d» dice que no hay GPU con driver 3D")
            out = m.comando("gpu3d encender", 30)
            verificar("sin GPU con driver 3D" in out, f"{que}: «gpu3d encender» sin driver no hace nada")
            out = m.comando("sgldemo -t 5", 120)
            verificar(re.search(r"sgldemo: cubo, 5 cuadros.*fps", out), f"{que}: SaguiGL por software sigue dibujando")
            out = m.comando("ls /lib/firmware/radeon", 30)
            verificar("R520_cp.bin" in out, f"{que}: el microcódigo está en /lib/firmware/radeon")
            s = m.serial()
            verificar("EXCECAO" not in s and "PANICO" not in s, f"{que}: sin excepciones ni pánicos")


def main():
    todas = {"unidad": unidad, "fuentes": fuentes, "firmware": firmware, "std": std}
    quais = [a for a in sys.argv[1:] if a in todas] or list(todas)
    try:
        for q in quais:
            print(f"== {q} ==")
            todas[q]()
    except FalhaTeste as e:
        print("FALHOU:", e)
        return 1
    print("H287: 3D por la GPU en Radeon R5xx (ordenes, fuentes y sin cambios en QEMU): BIEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
