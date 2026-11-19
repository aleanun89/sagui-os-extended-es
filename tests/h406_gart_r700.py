#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""h406_gart_r700.py - la GART de las Radeon R7xx y la vuelta del cuadro del 3D por ella (D406).

QEMU no emula ninguna Radeon: aquí se prueba la lógica en el PC y las reglas de seguridad en las fuentes.
  fuentes  la GART con «gpu3d encender gtt» o sola en las R7xx confirmadas (salvo «sin-gtt»), solo R7xx, pasos con /sistema/conf/gart-r700.paso,
           aparcada si el motor no para, bus maestro después de la GART, decisión y documentos
  unidad   tests/gart_r700_unidad.c (ventana, entradas, encender y apagar contra los valores de Linux 6.6),
           también con ASan/UBSan
Uso: h406_gart_r700.py [fuentes] [unidad]
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import FalhaTeste, verificar, RAIZ  # noqa: E402

DIR = os.path.join(RAIZ, "build", "h406")


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def fuentes():
    d = ler("kernel/dispositivos/gpu3d_r600.c")
    verificar("quer_gart = (variante & GPU3D_CAB_GTT) != 0" in d, "la GART se pide con la variante gtt («gpu3d encender gtt»)")
    # v8.5.5: en las R7xx confirmadas (solo la RV710 del HP dv7) se enciende sola, salvo «gpu3d encender sin-gtt».
    import re
    conf = re.search(r"gart_confirmadas\[\]\s*=\s*\{(.*?)\};", d, re.S).group(1)
    ids = re.findall(r"\{\s*0x([0-9A-Fa-f]{4}),\s*0x([0-9A-Fa-f]{4})\s*\}", conf)
    verificar(ids == [("1002", "9553")], f"solo la RV710 del dv7 (1002:9553) enciende la GART sola ({ids})")
    verificar("if (!quer_gart && r7 && !(variante & GPU3D_CAB_SEM_GTT) && gart_confirmada(d))" in d,
              "sola solo en R7xx confirmadas y sin «sin-gtt»")
    g = ler("kernel/gui/gpu3d.c")
    verificar("(op >= 5 ? GPU3D_CAB_SEM_GTT : 0)" in g and "op > 6" in g, "INFO_GPU3D_CTL 5/6: encender sin la GART")
    verificar("sin-gtt" in ler("userland/programas/gpu3d.c"), "«gpu3d encender sin-gtt»")
    verificar("if (g == 0) return G3D_OK;" in d, "si la GART no se enciende, el cuadro vuelve por la CPU (el 3D sigue)")
    verificar('solo en R7xx' in d, "solo en R7xx")
    verificar("/sistema/conf/gart-r700.paso" in d, "los pasos de uno en uno por fichero")
    verificar("aparcada" in d and "r7g_pte_relleno(gart.relleno)" in d, "si el motor no para, aparcada con todo al relleno")
    i_bm = d.index("| 0x04u); gart.bm_nosso = true;")
    i_regs = d.index("for (int i = 0; i < np; i++) wr(p[i].reg, p[i].valor);")
    verificar(i_regs < i_bm, "el bus maestro se enciende después de la GART")
    verificar("gart_desligar();   /* D406: antes de parar el CP" in d, "al apagar el 3D, la GART primero")
    verificar("r63d_cp_dma(&l, (uint32_t)b_cor.gpu, (uint32_t)gart.ini, bytes_cor)" in d, "la vuelta del cuadro por CP_DMA a la ventana")
    verificar("gpu3d_r600_gart_nossa()" in ler("kernel/dispositivos/acel_r600.c"), "la sonda del 2D no toma la GART del 3D por ajena")
    verificar("gpu3d_r600_estado" in ler("kernel/gui/gpu3d.c"), "«gpu3d» enseña la GART de las R7xx")
    verificar("## D406." in ler("docs/decisoes.md"), "la decisión D406 está en docs/decisoes.md")
    verificar("D406" in ler("docs/gpus.md") and "D406" in ler("docs/gpu3d.md"), "docs/gpus.md y docs/gpu3d.md citan la D406")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    fonte = os.path.join(RAIZ, "tests", "gart_r700_unidad.c")
    for nome, flags in (("gart_r700_unidad", ["-O2", "-Wall", "-Wextra", "-Werror"]),
                        ("gart_r700_unidad_asan", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"])):
        exe = os.path.join(DIR, nome)
        r = subprocess.run(["gcc"] + flags + ["-o", exe, fonte], capture_output=True, text=True)
        verificar(r.returncode == 0, f"{nome} compila\n" + r.stderr)
        r = subprocess.run([exe], capture_output=True, text=True)
        print("   ", (r.stdout + r.stderr).strip().replace("\n", "\n    "))
        verificar(r.returncode == 0 and "todo bien" in r.stdout, f"{nome}: ventana, entradas y registros")


def main():
    todas = {"fuentes": fuentes, "unidad": unidad}
    quais = [a for a in sys.argv[1:] if a in todas] or list(todas)
    try:
        for q in quais:
            print(f"== {q} ==")
            todas[q]()
    except FalhaTeste as e:
        print("FALHOU:", e)
        return 1
    print("H406: GART de las R7xx: BIEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
