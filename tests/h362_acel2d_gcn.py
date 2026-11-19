#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H362 (D360) - 2D por el motor DMA de las Radeon GCN 1.0 (Southern Islands) y
diagnóstico de GCN 1.1 en adelante.

QEMU no emula ninguna Radeon: aquí se comprueba que el driver está bien
enganchado, la lógica pura en el PC y que con las tarjetas que QEMU sí emula no
se activa ni se toca nada. Lo demás se mira en el hardware (docs/decisoes.md,
D360).

  fuentes  sin QEMU: la tabla de la capa común lleva las familias GCN con el
           driver acel_gcn (generaciones 10-13); cada modelo de la lista exacta
           (gcn_dma.c) cae en una sola fila y con su generación; ninguna fila
           nueva pisa otra; sin Hainan/Topaz, sin APU, sin 684C; el driver no
           carga firmware, no usa interrupciones, no escribe SRBM_SOFT_RESET,
           no escribe DMA_MODE, solo toca el DMA 0, comprueba el modelo antes
           de leer nada, y en GCN 1.1+ no mapea el MMIO; ninguna GCN está en
           «confirmadas»; D360, docs/terceros.md y docs/gpus.md lo registran;
  unidad   en el PC (tests/gcn_dma_unidad.c): paquetes de SI, detección por
           identificador PCI, controladores de pantalla, copias como memmove y
           rellenos con CONSTANT_FILL;
  std      -vga std: la CPU, ninguna línea del driver en el log, y
           «acel2d probar --detalle» sigue funcionando sin tocar nada.

Uso: h362_acel2d_gcn.py [fuentes] [unidad] [std]
"""
import os
import re
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste, RAIZ, SAIDA  # noqa: E402

DIR = os.path.join(SAIDA, "h362")
GEN = {"S": 10, "C": 11, "V": 12, "G": 13}
FAMILIAS = {"Tahiti": 10, "Pitcairn": 10, "Cape": 10, "Oland": 10, "Bonaire": 11, "Hawaii": 11, "Tonga": 12,
            "Fiji": 12, "Polaris": 12, "Vega M": 12, "Vega 10": 13, "Vega 12": 13, "Vega 20": 13}


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
    s = ler("kernel/dispositivos/gcn_dma.c")
    return [(int(i, 16), GEN[g], int(c), n)
            for i, g, c, n in re.findall(r"\{\s*0x([0-9A-Fa-f]{4}),\s*([SCVG]),\s*(\d),\s*\"([^\"]+)\"\s*\}", s)]


def familia(nome):
    for k in sorted(FAMILIAS, key=len, reverse=True):
        if nome.startswith(k):
            return k
    return None


def fuentes():
    tab = tabela_comum()
    novas = [f for f in tab if f[3] == "acel_gcn"]
    verificar(len(novas) == 16 and all(f[0] == 0x1002 and 10 <= f[4] <= 13 for f in novas),
              f"la tabla común lleva {len(novas)} filas GCN de AMD (generaciones 10-13)")
    verificar(sum(1 for f in novas if f[4] == 10) == 4, "cuatro filas de GCN 1.0 (Tahiti, Pitcairn, Cape Verde, Oland)")
    for f in novas:
        outras = [g for g in tab if g is not f and g[0] == f[0] and g[1] <= f[2] and f[1] <= g[2]]
        verificar(not outras, f"la fila {f[1]:04x}-{f[2]:04x} no pisa ninguna otra")
    mods = modelos()
    ids = [m[0] for m in mods]
    verificar(len(mods) == 160 and len(ids) == len(set(ids)), f"lista exacta de {len(mods)} modelos sin repetidos")
    mal = []
    for i, g, c, n in mods:
        filas = [f for f in tab if f[0] == 0x1002 and f[1] <= i <= f[2]]
        if not (len(filas) == 1 and filas[0][3] == "acel_gcn" and filas[0][4] == g):
            mal.append(f"{i:04x} ({n}): fila")
        fam = familia(n)
        if not (fam and FAMILIAS[fam] == g):
            mal.append(f"{i:04x} ({n}): generación {g}")
        if not ((c in (2, 6)) if g == 10 else c == 0):
            mal.append(f"{i:04x} ({n}): {c} controladores")
    verificar(not mal, "cada modelo cae en una sola fila (acel_gcn, su generación) y con sus controladores: " + ", ".join(mal[:8]))
    for fam in FAMILIAS:
        verificar(any(familia(n) == fam for _, _, _, n in mods), f"{fam} está en la lista")
    verificar(not any(0x6660 <= i <= 0x666F or 0x6900 <= i <= 0x6907 for i in ids), "sin Hainan ni Topaz (sin pantalla)")
    verificar(not any(0x1300 <= i <= 0x131F or 0x9830 <= i <= 0x98FF or i in (0x15D8, 0x15DD) for i in ids), "sin las APU")
    verificar(0x684C not in ids, "1002:684C fuera (cae en la fila de Turks)")
    evg = {int(i, 16) for i in re.findall(r"\{\s*0x([0-9A-Fa-f]{4}),\s*[EC],\s*\d,", ler("kernel/dispositivos/evergreen_dma.c"))}
    verificar(not (evg & set(ids)), "ningún modelo está también en la lista de Evergreen/NI")

    codigo = sem_comentarios(ler("kernel/dispositivos/acel_gcn.c"))
    puro = sem_comentarios(ler("kernel/dispositivos/gcn_dma.c"))
    verificar("/lib/firmware" not in codigo and "vfs_" not in codigo and "vfs_" not in puro, "sin firmware (el DMA de SI no lleva microcódigo)")
    verificar(not re.search(r"wr\(SRBM_SOFT_RESET", codigo), "SRBM_SOFT_RESET solo se lee")
    verificar("rd(SRBM_SOFT_RESET)" in codigo and "SOFT_RESET_DMA" in codigo, "se comprueba que el DMA no está retenido en reinicio")
    verificar(not re.search(r"\b(irq\w*|msi\w*|pci_msi\w*)\b", codigo, re.I), "sin interrupciones ni MSI")
    verificar("DMA_MODE" not in codigo, "sin DMA_MODE (no existe en SI)")
    verificar(not re.search(r"0xD8[0-9A-Fa-f]{2}\b", codigo), "el segundo motor DMA (0xD800) no se toca")
    verificar("DMA_IB_ENABLE | DMA_CMD_VMID_FORCE" in codigo, "CMD_VMID_FORCE como cayman_dma_resume")
    i_mod = codigo.index("gcn_modelo(d->modelo)")
    i_rd = codigo.index("rd(CONFIG_MEMSIZE)", i_mod)
    i_map = codigo.index("janela_mapear_tipo(mmio_fis", i_mod)
    i_si = codigo.index("modelo->gen != GCN_GEN_SI", i_mod)
    verificar(i_mod < i_si < i_map < i_rd, "el modelo exacto se comprueba antes de mapear o leer nada; GCN 1.1+ no mapea el MMIO")
    # Revisión D360: GCN 1.1+ sale antes de escribir nada (ni el PCI) y el bus maestro se apaga antes de
    # encender el anillo (una dirección que el MC no tome por VRAM no puede llegar a la RAM del PC).
    i_pci = codigo.index("pci_escreve32(d, 0x04", i_mod)
    i_on = codigo.index("cntl | DMA_RB_ENABLE", i_mod)
    verificar(i_si < i_pci < i_on and "& ~0x04u" in codigo[i_pci:i_pci + 80],
              "GCN 1.1+ no escribe nada; en SI el bus maestro se apaga antes de encender el anillo")
    verificar("rd(MC_VM_SYS_APER_BAIXO)" in codigo and "L1_TLB %x" in codigo,
              "el log y el motivo dan la apertura del sistema y L1_TLB_CNTL (plan B de la D360)")
    verificar("gcn_crtc_mostra(&e, mc_tela, t->pitch / 4)" in codigo, "se comprueba qué controlador muestra la pantalla del VBE/GOP")
    verificar("vm0 & 1" in codigo and "rb & DMA_RB_ENABLE" in codigo and "st & DMA_IDLE" in codigo,
              "memoria virtual apagada, anillo apagado y motor parado antes de encender")
    verificar("LIMITE_BAR0" in codigo and "cruza un limite de 4 GB" in codigo, "límite de 64 MB del BAR 0 y sin cruzar 4 GB del MC")
    verificar("acel_prazo(&" in codigo and not re.search(r"while\s*\([^)]*\)\s*;", codigo), "toda espera tiene plazo")
    verificar("usar_encher = bem" in codigo, "CONSTANT_FILL solo si su prueba al encender da lo esperado")
    comum = ler("kernel/gui/acel2d.c")
    conf = re.search(r"confirmadas\[\]\s*=\s*\{(.*?)\};", comum, re.S).group(1)
    verificar(re.sub(r"\{\s*0x1002,\s*0x(?:7145|9553)\s*\}", "", sem_comentarios(conf)).count("0x1002") == 0, "ninguna Radeon (salvo la X1400 del Amilo, D386, y la HD 4530 del dv7, D402) está en «confirmadas»: autoprueba completa")
    verificar("## D360" in ler("docs/decisoes.md"), "la decisión D360 está en docs/decisoes.md")
    verificar("gcn_dma.c" in ler("docs/terceros.md"), "docs/terceros.md registra las referencias")
    verificar("D360" in ler("docs/gpus.md"), "docs/gpus.md lleva la fila de GCN")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "gcn_dma_unidad")
    r = subprocess.run(["gcc", "-O2", "-Wall", "-DR600_ANFITRION", "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe,
                        os.path.join(RAIZ, "tests", "gcn_dma_unidad.c"),
                        os.path.join(RAIZ, "kernel", "dispositivos", "gcn_dma.c"),
                        os.path.join(RAIZ, "kernel", "dispositivos", "r600_dma_pacotes.c")],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", r.stdout.strip().replace("\n", "\n    "))
    verificar(r.returncode == 0 and "todo bien" in r.stdout, "paquetes, modelos, controladores de pantalla, copias y rellenos")


def std():
    import h161_acel2d as h161
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h362s", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar(re.search(r"\[ACEL2D\] CPU: la tarjeta 1234:1111 no es de una familia con acelerador", s),
                  "-vga std: la CPU, como antes")
        out = m.comando("acel2d probar --detalle", 60)
        s = m.serial()
        verificar("[ACEL2D] gcn" not in s and "gcndma" not in s and "gcndma" not in out, "el driver no se toca (ni con --detalle)")
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
