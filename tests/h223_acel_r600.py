#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H223 (D223) - 2D por el motor DMA de las Radeon R6xx/R7xx (la HD 4550 del mantenedor).

QEMU no emula ninguna Radeon: lo que se puede comprobar aquí es que el driver
está bien enganchado y que con las tarjetas que sí emula no se activa ni cambia
el arranque. Lo demás se mira en el hardware (docs/decisoes.md, D223).

  fuentes  sin QEMU: la tabla de la capa común (acel2d.c) lleva las familias
           R6xx/R7xx con el driver acel_r600 y la generación (7 sola, 6 solo
           forzada); cada modelo de la lista exacta del driver cae en una fila
           suya y en ninguna de otro acelerador; están los de la HD 4550
           (0x9552/0x9553/0x9555) y los de portátil pedidos (RV620, RV635, RV710,
           RV730: M82, M86, M92, M93, M96); el driver no usa firmware (el DMA de
           las R6xx/R7xx no lleva microcódigo) ni interrupciones, ni toca
           SRBM_SOFT_RESET; la D223 y docs/terceros.md lo registran;
  unidad   en el PC (tests/r600_dma_unidad.c con r600_dma_pacotes.c del kernel):
           formato de los paquetes y que copiar un rectángulo da lo mismo que
           memmove aunque el motor copie cada paquete en cualquier orden;
  std      -vga std: la CPU, ninguna línea del driver r600 en el log, el
           escritorio llega; con --acel2d=radeon, «la pantalla no es de una
           tarjeta suya» y tampoco se toca nada;
  vmware   con la VMware SVGA II sigue eligiéndose la VMware (o la CPU con KVM,
           D195) y el driver r600 no aparece.

Uso: h223_acel_r600.py [fuentes] [unidad] [std] [vmware]
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste, RAIZ, SAIDA  # noqa: E402

DIR = os.path.join(SAIDA, "h223")


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def tabela_comum():
    """Filas de la tabla de acel2d.c: (fab, de, ate, driver, generacion)."""
    s = ler("kernel/gui/acel2d.c")
    filas = re.findall(r"\{\s*0x([0-9A-Fa-f]{4}),\s*0x([0-9A-Fa-f]{4}),\s*0x([0-9A-Fa-f]{4}),\s*&(acel_\w+),\s*(\d+),", s)
    return [(int(f, 16), int(a, 16), int(b, 16), d, int(g)) for f, a, b, d, g in filas]


def modelos_driver():
    s = ler("kernel/dispositivos/acel_r600.c")
    return [(int(i, 16), int(g), n) for i, g, n in re.findall(r"\{\s*0x([0-9A-Fa-f]{4}),\s*(\d),\s*\"([^\"]+)\"\s*\}", s)]


def fuentes():
    tab = tabela_comum()
    r600 = [f for f in tab if f[3] == "acel_r600"]
    verificar(len(r600) >= 10 and all(f[0] == 0x1002 for f in r600), f"la tabla común lleva {len(r600)} familias R6xx/R7xx de AMD")
    mods = modelos_driver()
    verificar(len(mods) >= 100, f"el driver tiene su lista exacta ({len(mods)} modelos)")
    ids = [m[0] for m in mods]
    verificar(len(ids) == len(set(ids)), "sin modelos repetidos")
    for i, g, n in mods:
        filas = [f for f in tab if f[0] == 0x1002 and f[1] <= i <= f[2]]
        verificar(len(filas) == 1 and filas[0][3] == "acel_r600" and filas[0][4] == g,
                  f"{i:04x} ({n}) cae en una sola fila de la tabla, la de acel_r600 con generación {g}")
    for fam, g in (("RV710", 7), ("RV730", 7), ("RV740", 7), ("RV770", 7), ("RV620", 6), ("RV635", 6), ("RV610", 6),
                   ("RV630", 6), ("RV670", 6)):
        verificar(any(n.startswith(fam) and gg == g for _, gg, n in mods), f"{fam} (generación {g}) está en la lista")
    for i in (0x9552, 0x9553, 0x9555):
        verificar(i in ids and dict((m[0], m[1]) for m in mods)[i] == 7, f"{i:04x} (Mobility HD 4550, RV710) se elige solo")
    for movil in ("M82", "M86", "M92", "M93", "M96"):
        verificar(any(movil in n for _, _, n in mods), f"modelos de portátil {movil}")
    d = ler("kernel/dispositivos/acel_r600.c")
    codigo = re.sub(r"/\*.*?\*/", "", d, flags=re.S)
    verificar("/lib/firmware" not in codigo and "vfs_" not in codigo, "el driver no carga firmware (el DMA no lleva microcódigo)")
    verificar("SRBM_SOFT_RESET" not in codigo, "el driver no reinicia ningún bloque de la GPU")
    verificar(not re.search(r"\b(irq\w*|msi\w*|pci_msi\w*)\b", codigo, re.I) and "DMA_INTERRUPCOES" in codigo,
              "sin interrupciones ni MSI (y las del DMA apagadas): sondeo con plazo")
    verificar("acel_prazo" in codigo and "VM_CONTEXT0_CNTL" in codigo and "MC_VM_FB_LOCATION" in codigo
              and "HDP_NONSURFACE_BASE" in codigo, "plazos por TSC y comprobaciones del MC antes de encender el DMA")
    verificar(re.search(r"gen < 7 && !acel2d_forcado\(\)", codigo), "las R6xx solo con --acel2d=radeon")
    h = ler("kernel/inc/acel2d.h")
    verificar("extern const acel_driver_t acel_r600;" in h and "acel2d_forcado" in h, "acel2d.h exporta el driver")
    dec = ler("docs/decisoes.md")
    verificar("## D223." in dec, "la decisión D223 está en docs/decisoes.md")
    verificar("acel_r600.c" in ler("docs/terceros.md"), "docs/terceros.md registra las referencias del driver")
    mk = ler("Makefile")
    verificar("find kernel -name '*.c'" in mk, "el Makefile compila todo kernel/ (acel_r600.c y r600_dma_pacotes.c entran)")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "r600_dma_unidad")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-DR600_ANFITRION", "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe,
                        os.path.join(RAIZ, "tests", "r600_dma_unidad.c"),
                        os.path.join(RAIZ, "kernel", "dispositivos", "r600_dma_pacotes.c")],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", r.stdout.strip().replace("\n", "\n    "))
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "paquetes y copias de rectángulos como memmove")


def sin_r600(s, que):
    verificar("[ACEL2D] r600:" not in s and "r600dma" not in s, f"{que}: el driver r600 no se toca")
    verificar("EXCECAO" not in s and "PANICO" not in s, f"{que}: sin excepciones")


def std():
    import h161_acel2d as h161
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h223s", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar(re.search(r"\[ACEL2D\] CPU: la tarjeta 1234:1111 no es de una familia con acelerador", s),
                  "-vga std: la CPU, como antes")
        sin_r600(s, "-vga std")
    img = imagem_de_teste("", ["--vesa", "--acel2d=radeon"])
    with Maquina("h223r", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar("se pidio el acelerador radeon y la pantalla no es de una tarjeta suya" in s,
                  "--acel2d=radeon sin Radeon: la CPU")
        sin_r600(s, "--acel2d=radeon")


def vmware():
    import h161_acel2d as h161
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h223v", "ide", imagem=img, extras=h161.VMWARE) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar("acelerador vmware" in s, "la VMware sigue eligiendo su acelerador")
        verificar("[ACEL2D] activo: VMware SVGA II" in s or (h161.KVM and "es mas lenta que la CPU" in s),
                  "y se activa (o, con KVM, se queda la CPU por velocidad)")
        sin_r600(s, "VMware")


def main():
    partes = sys.argv[1:] or ["fuentes", "unidad", "std", "vmware"]
    for p in partes:
        print(f"== {p}")
        {"fuentes": fuentes, "unidad": unidad, "std": std, "vmware": vmware}[p]()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
