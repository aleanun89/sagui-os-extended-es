#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H286 (D286) - 2D por el motor DMA de las Radeon Evergreen y Northern Islands (HD 5000-6000).

QEMU no emula ninguna Radeon: aquí se comprueba que el driver está bien
enganchado, la lógica pura en el PC y que con las tarjetas que QEMU sí emula no
cambia nada. Lo demás se mira en el hardware (docs/decisoes.md, D286; el
objetivo es la Mobility HD 5000 1002:68e0 del Toshiba L635).

  fuentes  sin QEMU: la tabla de la capa común (acel2d.c) lleva las familias
           Evergreen/NI con el driver acel_r600 y la generación 8 (Cayman, 9);
           cada modelo de la lista exacta (evergreen_dma.c) cae en una sola fila
           y con su generación; ninguna fila nueva pisa las de otro acelerador
           ni las de R6xx/R7xx; está 0x68E0; no están las APU; el driver sigue
           sin firmware, sin interrupciones y sin SRBM_SOFT_RESET, usa el
           paquete de Evergreen, el vaciado del HDP por registro, no escribe
           DMA_MODE en Cayman; ninguna Evergreen/NI está en «confirmadas»; la
           D286, docs/terceros.md y docs/gpus.md lo registran;
  unidad   en el PC (tests/evergreen_dma_unidad.c): paquetes, detección por
           identificador PCI, controladores de pantalla y rectángulos como
           memmove;
  std      -vga std: la CPU, ninguna línea del driver en el log, el escritorio
           llega y «acel2d probar --detalle» sigue funcionando sin tocar nada.

Uso: h286_acel2d_evergreen.py [fuentes] [unidad] [std]
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste, RAIZ, SAIDA  # noqa: E402

DIR = os.path.join(SAIDA, "h286")
FAMILIAS = {"Cedar": 8, "Redwood": 8, "Juniper": 8, "Cypress": 8, "Hemlock": 8, "Barts": 8, "Turks": 8, "Caicos": 8,
            "Cayman": 9}


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def sem_comentarios(s):
    return re.sub(r"/\*.*?\*/", "", s, flags=re.S)


def tabela_comum():
    s = ler("kernel/gui/acel2d.c")
    filas = re.findall(r"\{\s*0x([0-9A-Fa-f]{4}),\s*0x([0-9A-Fa-f]{4}),\s*0x([0-9A-Fa-f]{4}),\s*&(acel_\w+),\s*(\d+),", s)
    return [(int(f, 16), int(a, 16), int(b, 16), d, int(g)) for f, a, b, d, g in filas]


def modelos():
    s = ler("kernel/dispositivos/evergreen_dma.c")
    gen = {"E": 8, "C": 9}
    return [(int(i, 16), gen[g], int(c), n)
            for i, g, c, n in re.findall(r"\{\s*0x([0-9A-Fa-f]{4}),\s*([EC]),\s*(\d),\s*\"([^\"]+)\"\s*\}", s)]


def fuentes():
    tab = tabela_comum()
    novas = [f for f in tab if f[3] == "acel_r600" and f[4] >= 8]
    verificar(len(novas) == 9 and all(f[0] == 0x1002 for f in novas), f"la tabla común lleva {len(novas)} familias Evergreen/NI de AMD")
    verificar(sum(1 for f in novas if f[4] == 9) == 1, "una sola fila de generación 9 (Cayman)")
    for f in novas:
        outras = [g for g in tab if g is not f and g[0] == f[0] and g[1] <= f[2] and f[1] <= g[2]]
        verificar(not outras, f"la fila {f[1]:04x}-{f[2]:04x} no pisa ninguna otra")
    mods = modelos()
    ids = [m[0] for m in mods]
    verificar(len(mods) == 112 and len(ids) == len(set(ids)), f"lista exacta de {len(mods)} modelos sin repetidos")
    for i, g, c, n in mods:
        filas = [f for f in tab if f[0] == 0x1002 and f[1] <= i <= f[2]]
        verificar(len(filas) == 1 and filas[0][3] == "acel_r600" and filas[0][4] == g,
                  f"{i:04x} ({n}) cae en una sola fila, la de acel_r600 con generación {g}")
        fam = n.split()[0]
        verificar(FAMILIAS.get(fam) == g and c in (4, 6), f"{i:04x}: {fam} es de la generación {g}")
    for fam in FAMILIAS:
        verificar(any(n.startswith(fam) for _, _, _, n in mods), f"{fam} está en la lista")
    verificar(dict((m[0], m) for m in mods)[0x68E0][1:3] == (8, 4), "1002:68e0 (Toshiba L635): Cedar, generación 8")
    r600 = ler("kernel/dispositivos/acel_r600.c")
    viejos = {int(i, 16) for i in re.findall(r"\{\s*0x([0-9A-Fa-f]{4}),\s*\d,\s*\"", r600)}
    verificar(not (viejos & set(ids)), "ningún modelo está también en la lista de R6xx/R7xx")
    verificar(not any(0x9640 <= i <= 0x964F or 0x9800 <= i <= 0x99FF for i in ids), "sin las integradas de las APU")

    codigo = sem_comentarios(r600)
    puro = sem_comentarios(ler("kernel/dispositivos/evergreen_dma.c"))
    verificar("/lib/firmware" not in codigo and "vfs_" not in codigo and "vfs_" not in puro, "sin firmware (el DMA no lleva microcódigo)")
    verificar("SRBM_SOFT_RESET" not in codigo, "sin reiniciar ningún bloque de la GPU")
    verificar(not re.search(r"\b(irq\w*|msi\w*|pci_msi\w*)\b", codigo, re.I), "sin interrupciones ni MSI")
    verificar("evg_modelo(d->modelo)" in codigo and "d->fabricante != 0x1002" in codigo, "el modelo exacto se comprueba antes de tocar nada")
    verificar(re.search(r"gen >= EVG_GEN_EVERGREEN \? evg_pkt_copiar(_alto)?\(", codigo), "las Evergreen/NI usan su paquete de copia")
    verificar(re.search(r"if \(gen == 7 && !agp\)", codigo), "HDP_DEBUG1 solo en las R7xx; Evergreen/NI vacían el HDP por registro")
    verificar("evg_crtc_mostra(&e, mc64, t->pitch / 4)" in codigo, "se comprueba qué controlador muestra la pantalla del VBE/GOP")
    # D408: direcciones del MC de 40 bits (la HD 7600M del Toshiba tiene la VRAM en 0xF_0000_0000).
    verificar("if (mc64 >> 40)" in codigo and "if (mc64 >> 32)" not in codigo, "D408: la pantalla puede estar por encima de 4 GB en el MC (40 bits)")
    verificar("wr(DMA_RB_RPTR_ADDR_HI, alto)" in codigo and "(((uint64_t)alto << 32) | mc_ring) >> 8" in codigo,
              "D408: el anillo con sus bits 39:32 (RPTR_ADDR_HI y la base de 39:8)")
    verificar("evg_pkt_copiar_alto(p, dst, src, ndw, alto)" in codigo and "r600_pkt_valla_alto(" in codigo
              and "r600_pkt_escrever_alto(" in codigo, "D408: copiar, escribir y valla llevan el byte alto")
    verificar("cruza un limite de 4 GB del MC" in codigo, "D408: la zona del DMA no cruza un límite de 4 GB del MC (un solo byte alto)")
    verificar("hi == mc_alto && sup == mc_tela" in codigo, "D408: R7xx compara los bits 39:32 de la superficie")
    verificar("if (base.mc_alto)" in sem_comentarios(ler("kernel/dispositivos/gpu3d_r600.c")), "D408: el 3D R6xx/R7xx no acepta la VRAM por encima de 4 GB")
    verificar(re.search(r"tem_dma_mode\(void\)\s*\{\s*return gen == 7 \|\| gen == EVG_GEN_EVERGREEN;", codigo)
              and "if (tem_dma_mode()) wr(DMA_MODE, 1);" in codigo and "wr(DMA_MODE, 1)" in codigo
              and codigo.count("wr(DMA_MODE") == 2, "DMA_MODE solo donde existe (ni R6xx ni Cayman)")
    verificar("gen == EVG_GEN_CAYMAN ? DMA_IB_ENABLE | DMA_CMD_VMID_FORCE : DMA_IB_ENABLE" in codigo, "Cayman: CMD_VMID_FORCE")
    verificar("evg_memsize_bytes(memsize)" in codigo and "MMIO_TAM_EVG" in codigo, "CONFIG_MEMSIZE en MB y MMIO de 128 KB")
    verificar(re.search(r"gen < 7 && !acel2d_forcado\(\)", codigo), "las R6xx siguen solo forzadas")
    comum = ler("kernel/gui/acel2d.c")
    conf = re.search(r"confirmadas\[\]\s*=\s*\{(.*?)\};", comum, re.S).group(1)
    verificar(re.sub(r"\{\s*0x1002,\s*0x(?:7145|9553|6840)\s*\}", "", sem_comentarios(conf)).count("0x1002") == 0, "ninguna Radeon (salvo la X1400 del Amilo, D386, la HD 4530 del dv7, D402, y la HD 7600M del Toshiba, D408) está en «confirmadas»: autoprueba completa hasta que el mantenedor la pruebe")
    verificar("## D286" in ler("docs/decisoes.md"), "la decisión D286 está en docs/decisoes.md")
    verificar("evergreen_dma.c" in ler("docs/terceros.md"), "docs/terceros.md registra las referencias")
    verificar("D286" in ler("docs/gpus.md"), "docs/gpus.md lleva la fila de Evergreen/NI")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "evergreen_dma_unidad")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-DR600_ANFITRION", "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe,
                        os.path.join(RAIZ, "tests", "evergreen_dma_unidad.c"),
                        os.path.join(RAIZ, "kernel", "dispositivos", "evergreen_dma.c"),
                        os.path.join(RAIZ, "kernel", "dispositivos", "r600_dma_pacotes.c")],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", r.stdout.strip().replace("\n", "\n    "))
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "paquetes, modelos, controladores de pantalla y rectángulos como memmove")


def std():
    import h161_acel2d as h161
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h286s", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar(re.search(r"\[ACEL2D\] CPU: la tarjeta 1234:1111 no es de una familia con acelerador", s),
                  "-vga std: la CPU, como antes")
        out = m.comando("acel2d probar --detalle", 60)
        s = m.serial()
        verificar("[ACEL2D] r600" not in s and "r600dma" not in s and "r600dma" not in out, "el driver no se toca (ni con --detalle)")
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones")


def main():
    partes = sys.argv[1:] or ["fuentes", "unidad", "std"]
    for p in partes:
        print(f"== {p}")
        {"fuentes": fuentes, "unidad": unidad, "std": std}[p]()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
