#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H361 (D359) - 2D por el blitter de Intel Gen7 (Ivy Bridge), Gen7.5 (Haswell) y
Gen8 (Broadwell) por el anillo heredado del BCS; Gen9 y Gen11-Gen12 reconocidas y
sin tocar (necesitan execlists).

QEMU no emula ninguna GPU de Intel: el driver solo se prueba en hardware real
(la HD 4000 del L850; el mini PC i3-1220P tiene que quedarse en la CPU). Aquí:
  anfitriao  compila tests/recursos/acel_intel_anfitriao.c con el MISMO
             kernel/dispositivos/acel_intel_logica.h: modelos exactos, familias sin
             2D, órdenes por generación (las de Gen3-6 iguales que antes), el plano
             de la pantalla, las entradas del GTT, la zona reservada y el anillo;
  fuentes    cada modelo con 2D cae en una sola fila de acel2d.c, la de acel_intel
             con su generación; ninguna fila de acel_intel cubre una familia sin 2D;
             el driver mira la lista exacta antes de leer la tarjeta y comprueba el
             plano y el GTT antes de escribir nada; no escribe en los registros de
             la pantalla ni en el GTT; no carga firmware ni activa execlists; los
             plazos están acotados; la D359 y docs/gpus.md lo registran;
  std        con -vga std nada cambia: la CPU, sin ninguna línea del driver intel;
  forzado    --acel2d=intel sin Intel: la CPU, lo dice el log, y el escritorio funciona.

Uso: h361_acel2d_intel_moderna.py [anfitriao] [fuentes] [std] [forzado]
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import RAIZ, SAIDA, Maquina, FalhaTeste, verificar, imagem_de_teste  # noqa: E402
import h161_acel2d as h161  # noqa: E402


def ler(rel):
    with open(os.path.join(RAIZ, rel), encoding="utf-8") as f:
        return f.read()


def anfitriao_saida():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "acel_intel_anfitriao")
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-o", exe,
                    os.path.join(RAIZ, "tests", "recursos", "acel_intel_anfitriao.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    return r.returncode, r.stdout.splitlines()


def parte_anfitriao():
    rc, linhas = anfitriao_saida()
    for l in linhas:
        if l.startswith("FALHA"):
            print("  " + l)
    verificar(rc == 0 and linhas and linhas[-1] == "TUDO OK",
              f"lógica del blitter de Intel en el anfitrión ({sum(l.startswith('ok') for l in linhas)} comprobaciones)")


def tabela_comum():
    s = ler("kernel/gui/acel2d.c")
    filas = re.findall(r"\{\s*0x([0-9A-Fa-f]{4}),\s*0x([0-9A-Fa-f]{4}),\s*0x([0-9A-Fa-f]{4}),\s*&(acel_\w+),\s*(\d+),", s)
    return [(int(f, 16), int(a, 16), int(b, 16), d, int(g)) for f, a, b, d, g in filas]


def parte_fuentes():
    _, linhas = anfitriao_saida()
    ids = [(int(m.group(1), 16), int(m.group(2))) for m in (re.match(r"id ([0-9A-F]{4}) (\d+)$", l) for l in linhas) if m]
    verificar(len(ids) == 90, f"el anfitrión lista los {len(ids)} modelos con 2D de Gen7/7.5/8")
    tab = tabela_comum()
    for i, g in ids:
        filas = [f for f in tab if f[0] == 0x8086 and f[1] <= i <= f[2]]
        if not (len(filas) == 1 and filas[0][3] == "acel_intel" and filas[0][4] == g):
            raise FalhaTeste(f"8086:{i:04x} (Gen{g}) no cae en una sola fila de acel_intel con su generación: {filas}")
    verificar(True, "cada modelo cae en una sola fila de acel2d.c, la de acel_intel con su generación")
    verificar(any(f[1] == 0x0166 and f[3] == "acel_intel" and f[4] == 7 for f in tab), "la HD 4000 del L850 (0x0166) va a Gen7")
    logica = ler("kernel/dispositivos/acel_intel_logica.h")
    sem = [(int(a, 16), int(b, 16)) for a, b in re.findall(r"\{ 0x([0-9A-F]{4}), 0x([0-9A-F]{4}), \d+, \"", logica)]
    verificar(len(sem) >= 20, f"{len(sem)} intervalos de familias sin 2D")
    malas = [(f[1], f[2]) for f in tab if f[3] == "acel_intel" for a, b in sem if f[1] <= b and a <= f[2]]
    verificar(not malas, "ninguna fila de acel_intel cubre una familia sin 2D (Atom, Gen9, Gen11-12)" +
              (f": {malas}" if malas else ""))
    verificar(any(a <= 0x46A8 <= b for a, b in sem), "Alder Lake-P (el i3-1220P) se reconoce como sin 2D")
    capa = ler("kernel/gui/acel2d.c")
    verificar("intel_familia_sem_2d(" in capa, "acel2d.c dice por qué una Intel moderna se queda en la CPU")
    drv = ler("kernel/dispositivos/acel_intel.c")
    ini = drv[drv.index("static bool iniciar_moderna("):]
    ini = ini[:ini.index("\n}\n")]
    verificar(ini.index("intel_variante(") < ini.index("pci_le32(") and ini.index("intel_variante(") < ini.index("rd("),
              "iniciar_moderna mira la lista exacta antes de leer la tarjeta")
    primera_escrita = min(ini.index("pci_escreve32("), ini.index("acordar_moderna("), ini.index("wr("))
    verificar(ini.index("plano_da_tela(") < primera_escrita and ini.index("conferir_gtt(") < primera_escrita,
              "el plano de la pantalla y el GTT se comprueban antes de escribir nada")
    verificar(ini.index("GFX_RUN_LIST") < ini.index("wr(base + R_START"), "si el firmware dejó execlists, no se toca")
    codigo = re.sub(r"/\*.*?\*/", "", drv, flags=re.S)
    escritas = re.findall(r"\bwr\(([^,]+),", codigo)
    malas = [e for e in escritas if re.match(r"\s*(0x7|INTEL_DSP|INTEL_STOLEN)", e)]
    verificar(not malas, f"no escribe en la pantalla ni en la zona reservada ({len(escritas)} escrituras)" +
              (f": {malas}" if malas else ""))
    verificar(not re.search(r"\b(gtt|e)\[[^\]]*\]\s*=[^=]", codigo), "no escribe en el GTT (solo lo lee)")
    verificar("/lib/firmware" not in codigo and "vfs_" not in codigo and not re.search(r"\b\w*(guc|huc)\w*\b", codigo, re.I),
              "sin firmware (ni GuC ni HuC)")
    verificar(not re.search(r"elsp", codigo, re.I) and "wr(base + R_GFX_MODE" not in codigo,
              "no activa execlists ni envía nada por ellas (GFX_MODE solo se lee)")
    verificar(re.search(r"acel_prazo\(&p, 50\)", codigo) and re.search(r"acel_prazo\(&p, 5\)", codigo),
              "las esperas del forcewake están acotadas (50 ms y 5 ms)")
    verificar("in_detalle," in drv, "«acel2d probar --detalle» vuelca el estado del anillo")
    verificar("## D359." in ler("docs/decisoes.md"), "docs/decisoes.md tiene la D359")
    gpus = ler("docs/gpus.md")
    verificar(re.search(r"Intel Gen7\*\*.*D359", gpus) and re.search(r"Gen9.*D359", gpus), "docs/gpus.md recoge la D359")


def sin_intel(s):
    s = s.replace("se pidio el acelerador intel", "")
    verificar("[ACEL2D] intel" not in s and "acelerador intel" not in s,
              "ninguna línea del driver intel: no se toca ninguna tarjeta")
    verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones")


def parte_std():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h361s", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar(re.search(r"\[ACEL2D\] CPU: la tarjeta 1234:1111 no es de una familia con acelerador", s),
                  "-vga std: ninguna tarjeta del acelerador, la CPU (como antes)")
        sin_intel(s)
        h161.arrastar_lento(m, *h161.TITULO, *h161.DESTINO)
        verificar(h161.medida(m.serial())[2] == "cpu", "el arrastre va por la CPU")


def parte_forzado():
    img = imagem_de_teste("", ["--vesa", "--acel2d=intel"])
    with Maquina("h361f", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar("se pidio el acelerador intel y la pantalla no es de una tarjeta suya (1234:1111)" in s,
                  "--acel2d=intel sin Intel: la CPU, y el log dice por qué")
        sin_intel(s)
        m.comando("infopc -t video", 60)
        time.sleep(1)
        verificar(re.search(r"2D: +CPU \(se pidio el acelerador intel", m.serial()), "infopc dice que va la CPU")
        h161.arrastar_lento(m, *h161.TITULO, *h161.DESTINO)
        verificar(h161.medida(m.serial())[2] == "cpu", "el escritorio funciona con la CPU")


def main():
    partes = sys.argv[1:] or ["anfitriao", "fuentes", "std", "forzado"]
    for p in partes:
        print(f"== {p}")
        {"anfitriao": parte_anfitriao, "fuentes": parte_fuentes, "std": parte_std, "forzado": parte_forzado}[p]()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
