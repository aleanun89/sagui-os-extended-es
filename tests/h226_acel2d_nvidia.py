#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H226 (D224) - 2D por la GPU en las NVIDIA GeForce 6/7 (NV4x/G7x) y las
integradas de los nForce (GeForce 6100/6150, 7025/7050/7100/7150).

El QEMU no emula ninguna GeForce: el driver solo se prueba en hardware real.
Aquí:
  anfitriao  compila tests/recursos/acel_nv_anfitriao.c con el MISMO
             kernel/dispositivos/acel_nv_logica.h y kernel/terceros/mit/nv/nv_2d.h:
             formato de las órdenes del FIFO (cabeceras, subcanales, puntos y
             tamaños del blit y del rectángulo, salto), las palabras que atan los
             objetos, la tabla hash y los objetos de PRAMIN, el anillo, el pitch
             del CRTC, dónde va el búfer de órdenes, los modelos y los chips;
  fuentes    la tabla de acel2d.c tiene las familias de NVIDIA enganchadas a
             acel_nv, cada identificador exacto de la lista cae en una de ellas,
             --acel2d=nvidia existe, el driver no lee la tarjeta antes de mirar
             la lista y no escribe en PFB, PRAMDAC, PTIMER ni el CRTC (solo el
             candado CR1F y el índice), y docs/terceros.md registra xf86-video-nv;
  std        con la tarjeta de siempre (-vga std) nada cambia: la CPU, sin
             ninguna línea del driver nvidia;
  forzado    --acel2d=nvidia sin GeForce: la CPU, lo dice el log y el infopc, y
             el escritorio funciona;
  vmware     con la VMware SVGA II el acelerador de la VMware sigue igual y el
             de NVIDIA no se mira.

Uso: h226_acel2d_nvidia.py [anfitriao] [fuentes] [std] [forzado] [vmware]
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


def parte_anfitriao():
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "acel_nv_anfitriao")
    subprocess.run(["gcc", "-O2", "-Wall", "-Wextra", "-Werror", "-o", exe,
                    os.path.join(RAIZ, "tests", "recursos", "acel_nv_anfitriao.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    linhas = r.stdout.splitlines()
    for l in linhas:
        if l.startswith("FALHA"):
            print("  " + l)
    verificar(r.returncode == 0 and linhas and linhas[-1] == "TUDO OK",
              f"lógica del driver nvidia en el anfitrión ({sum(l.startswith('ok') for l in linhas)} comprobaciones)")


def parte_fuentes():
    capa = ler("kernel/gui/acel2d.c")
    faixas = [(int(a, 16), int(b, 16)) for a, b in
              re.findall(r"\{ 0x10DE, 0x([0-9A-F]{4}), 0x([0-9A-F]{4}), &acel_nv, 0, ", capa)]
    verificar(len(faixas) >= 15, f"acel2d.c: {len(faixas)} familias de NVIDIA enganchadas a acel_nv")
    verificar('case ACEL2D_NVIDIA: return "nvidia";' in capa, "acel2d.c: el modo forzado se llama nvidia")
    cab = ler("kernel/inc/acel2d.h")
    verificar(re.search(r"#define ACEL2D_NVIDIA\s+6\b", cab) and "extern const acel_driver_t acel_nv;" in cab,
              "acel2d.h: ACEL2D_NVIDIA = 6 y el acelerador acel_nv")
    verificar('"nvidia": 6' in ler("tools/mkimagem.py"), "mkimagem.py: --acel2d=nvidia")
    logica = ler("kernel/dispositivos/acel_nv_logica.h")
    ids = [int(x, 16) for x in re.findall(r"\{ 0x([0-9A-F]{4}), 0x[0-9A-F]{1,2}, ", logica)]
    ids += [int(x, 16) for x in re.findall(r"\{ 0x([0-9A-F]{4}), 0, 0, ", logica)]
    fora = [f"{i:04x}" for i in ids if not any(a <= i <= b for a, b in faixas)]
    verificar(len(ids) >= 120 and not fora,
              f"los {len(ids)} identificadores exactos caen en las familias de acel2d.c" + (f" (fuera: {fora})" if fora else ""))
    for i in (0x0240, 0x0241, 0x0242, 0x0244, 0x0245, 0x0247, 0x03D0, 0x03D1, 0x03D2, 0x03D5,
              0x0531, 0x0533, 0x053A, 0x053B, 0x053E, 0x07E0, 0x07E1, 0x07E2, 0x07E3, 0x07E5):
        verificar(i in ids, f"la integrada 10de:{i:04x} está en la lista")
    verificar(len(set(ids)) == len(ids), "sin identificadores repetidos")
    drv = ler("kernel/dispositivos/acel_nv.c")
    ini = drv[drv.index("static bool nv_iniciar("):]
    verificar(ini.index("nv_modelo(") < ini.index("rd("), "nv_iniciar mira la lista antes de leer ningún registro")
    verificar(ini.index("nv_chip_pmc1700(chip)") < ini.index("ligado = true;") and
              ini.index("cabeca_da_tela(") < ini.index("ligado = true;") and
              ini.index("nv_colocar_pb(") < ini.index("ligado = true;"),
              "PMC 0x1700, el CRTC y el sitio del búfer se comprueban antes de cambiar nada")
    escritas = re.findall(r"\bwr\(([^,]+),", drv)
    malas = [e for e in escritas if re.match(r"\s*(0x10[0-9A-Fa-f]{4}|0x68|0x009|0x60[0-9A-Fa-f]{4}|PFB|PCRTC)", e)]
    verificar(not malas, f"el driver no escribe en PFB, PRAMDAC, PTIMER ni PCRTC ({len(escritas)} escrituras)" +
              (f": {malas}" if malas else ""))
    terceros = ler("kernel/terceros/mit/nv/nv_2d.h")
    regs = [int(x, 16) for x in re.findall(r"\{ 0x([0-9A-F]{6}), ", terceros)]
    verificar(regs and all(0x2000 <= r < 0x4000 or 0x400000 <= r < 0x410000 for r in regs),
              f"las tablas de nv_2d.h solo tocan PFIFO y PGRAPH ({len(regs)} escrituras)")
    verificar(terceros.startswith("// SPDX-License-Identifier: MIT") and "NVIDIA, Corporation" in terceros,
              "nv_2d.h con su licencia MIT y el copyright de NVIDIA")
    verificar("kernel/terceros/mit/nv/" in ler("docs/terceros.md"), "docs/terceros.md registra xf86-video-nv")
    verificar("## D224." in ler("docs/decisoes.md"), "docs/decisoes.md tiene la D224")


def sin_nvidia(s):
    # con --acel2d=nvidia, el aviso «se pidio el acelerador nvidia y la pantalla no es
    # de una tarjeta suya» es lo esperado: no cuenta como una línea del driver
    s = s.replace("se pidio el acelerador nvidia", "")
    verificar("[ACEL2D] nvidia" not in s and "acelerador nvidia" not in s,
              "ninguna línea del driver nvidia: no se toca ninguna tarjeta suya")
    verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones")


def parte_std():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h225s", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar(re.search(r"\[ACEL2D\] CPU: la tarjeta 1234:1111 no es de una familia con acelerador", s),
                  "-vga std: ninguna tarjeta del acelerador, la CPU (como antes)")
        sin_nvidia(s)
        h161.arrastar_lento(m, *h161.TITULO, *h161.DESTINO)
        verificar(h161.medida(m.serial())[2] == "cpu", "el arrastre va por la CPU")


def parte_forzado():
    img = imagem_de_teste("", ["--vesa", "--acel2d=nvidia"])
    with Maquina("h225f", "ide", imagem=img) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar("se pidio el acelerador nvidia y la pantalla no es de una tarjeta suya (1234:1111)" in s,
                  "--acel2d=nvidia sin GeForce: la CPU, y el log dice por qué")
        sin_nvidia(s)
        m.comando("infopc -t video", 60)
        time.sleep(1)
        verificar(re.search(r"2D: +CPU \(se pidio el acelerador nvidia", m.serial()), "infopc dice que va la CPU")
        h161.arrastar_lento(m, *h161.TITULO, *h161.DESTINO)
        verificar(h161.medida(m.serial())[2] == "cpu", "el escritorio funciona con la CPU")


def parte_vmware():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h225v", "ide", imagem=img, extras=h161.VMWARE) as m:
        h161.arrancar(m)
        s = m.serial()
        verificar("[ACEL2D] la pantalla (fd000000) es de la tarjeta 15ad:0405" in s and "acelerador vmware" in s,
                  "la VMware se elige igual que antes")
        verificar("autoprueba del arranque" in s and "IGUAL que la CPU" in s, "su autoprueba sigue saliendo bien")
        sin_nvidia(s)


def main():
    partes = sys.argv[1:] or ["anfitriao", "fuentes", "std", "forzado", "vmware"]
    for p in partes:
        print(f"== {p}")
        {"anfitriao": parte_anfitriao, "fuentes": parte_fuentes, "std": parte_std,
         "forzado": parte_forzado, "vmware": parte_vmware}[p]()
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
