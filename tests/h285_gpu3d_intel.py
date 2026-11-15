#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H285 - 3D por la GPU: la capa común y el driver de Intel Gen3 (D285).

QEMU no emula el 3D de ninguna GPU de Intel, así que el driver de verdad
(kernel/dispositivos/gpu3d_intel.c) solo se puede probar en una máquina con
915/945/G33. Aquí se prueba lo que sí se puede:

  unidad    en el PC (tests/gen3_3d_unidad.c): las órdenes de Gen3 recorridas
            como lo haría la GPU, los dos programas de píxeles contra sus
            palabras escritas a mano, el tamaño del vértice que declara el
            estado, la memoria de GPU por bloques y la coma flotante con enteros;
  fuentes   lo que tiene que estar en el árbol: lista exacta de modelos, sin
            escribir el GTT, decisión D285, docs/gpu3d.md, docs/terceros.md;
  std       QEMU normal (sin GPU con 3D): la capa no toca nada, «gpu3d» dice
            por qué va el software, «gpu3d probar» y «sgldemo -g» lo dicen y
            SaguiGL por software da lo de siempre;
  simulado  --gpu3d=simulado (driver de mentira por la CPU): el camino entero:
            autoprueba (8 pruebas), medida, «sgldemo -g» (SaguiGL por software
            contra la GPU en cuatro escenas), contadores, apagar y encender,
            infopc;
  fallos    autoprueba con un píxel distinto y GPU que deja de responder
            (simulados): el 3D se apaga con su motivo, SaguiGL sigue por
            software y el sistema no se cuelga; --gpu3d=no.

Uso: h285_gpu3d_intel.py [unidad] [fuentes] [std] [simulado] [fallos]
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste, SAIDA  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(RAIZ, "build", "h285")


def ler(caminho):
    with open(os.path.join(RAIZ, caminho), encoding="utf-8") as f:
        return f.read()


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


def sano(s, que):
    verificar("EXCECAO" not in s and "PANICO" not in s, f"{que}: sin excepciones ni pánicos")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    exe = os.path.join(DIR, "gen3_3d_unidad")
    r = subprocess.run(["gcc", "-O1", "-fno-builtin", "-Wall", "-Wno-builtin-declaration-mismatch",
                        "-I", os.path.join(RAIZ, "kernel", "inc"), "-I", os.path.join(RAIZ, "kernel", "dispositivos"),
                        "-o", exe, os.path.join(RAIZ, "tests", "gen3_3d_unidad.c"),
                        os.path.join(RAIZ, "kernel", "gui", "gpu3d_mem.c")], capture_output=True, text=True)
    verificar(r.returncode == 0, "la prueba de unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", r.stdout.strip().replace("\n", "\n    ")[-600:])
    verificar(r.returncode == 0 and "0 fallos: BIEN" in r.stdout,
              "órdenes de Gen3, programas de píxeles, memoria de GPU y coma flotante con enteros")


def fuentes():
    drv = ler("kernel/dispositivos/gpu3d_intel.c")
    for modelo in ("0x2582", "0x2592", "0x2772", "0x27A2", "0x27AE", "0x29B2", "0x29C2", "0x29D2", "0xA001", "0xA011"):
        verificar(modelo in drv, f"gpu3d_intel.c: el modelo {modelo} está en la lista exacta")
    verificar("0x2A02" not in drv and "0x0102" not in drv, "gpu3d_intel.c: ni Gen4 ni Gen6 en la lista")
    verificar("modelo_ok(d->modelo)" in drv and "geracao != 3" in drv, "gpu3d_intel.c: comprueba el modelo antes de tocar nada")
    verificar("wr(" not in drv and "pci_escreve32(d, 0x04" in drv,
              "gpu3d_intel.c: no escribe registros MMIO por su cuenta (solo el ring de acel_intel.c)")
    verificar("GPU3D_PRAZO_MS" in drv and "while (1)" not in drv and "for (;;)" not in drv, "gpu3d_intel.c: esperas con plazo")
    pac = ler("kernel/dispositivos/gen3_pacotes.h")
    verificar("GPL-2.0-or-later" in pac and "MIT" in pac, "gen3_pacotes.h: código propio con sus fuentes anotadas")
    for f in ("kernel/gui/gpu3d.c", "kernel/gui/gpu3d_mem.c", "kernel/dispositivos/gpu3d_intel.c",
              "kernel/dispositivos/gpu3d_sim.c", "kernel/inc/gpu3d.h", "kernel/inc/acel_intel.h", "userland/programas/gpu3d.c"):
        t = ler(f)
        verificar("SPDX-License-Identifier: GPL-2.0-or-later" in t.split("\n")[0], f"{f}: marca SPDX")
        verificar("GPL-2.0-only" not in t, f"{f}: nada «GPL-2.0-only»")
    capa = ler("kernel/gui/gpu3d.c")
    verificar("OPC_BOOT_SGL_SOFTWARE" in capa and "vertices_ok" in capa and "usuario_ok" in capa,
              "gpu3d.c: opción de arranque, validación de vértices y de punteros del programa")
    h = ler("kernel/inc/gpu3d.h")
    for pieza in ("g3d_arena_t", "g3d_buffer_t", "g3d_fila_t", "g3d_vala_t", "gpu3d_driver_t"):
        verificar(pieza in h, f"gpu3d.h: {pieza}")
    dec = ler("docs/decisoes.md")
    verificar("## D285." in dec, "la decisión D285 está en docs/decisoes.md")
    doc = ler("docs/gpu3d.md")
    verificar("Qué probar en una máquina con 945/G33" in doc and "Cómo añadir un driver" in doc,
              "docs/gpu3d.md: qué probar en hardware y cómo añadir un driver")
    verificar("gen3_pacotes.h" in ler("docs/terceros.md"), "docs/terceros.md registra las referencias (Mesa, MIT)")


def std():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h285s", "ide", imagem=img) as m:
        arrancar(m)
        s = m.serial()
        verificar("[GPU3D] software:" in s and "[GPU3D] encender" not in s, "sin GPU con 3D: la capa no enciende nada al arrancar")
        out = m.comando("gpu3d", 30)
        verificar("3D por GPU: sin GPU con driver 3D (software)" in out, "«gpu3d» dice que no hay GPU con driver 3D")
        out = m.comando("gpu3d probar", 30)
        verificar("gpu3d probar: sin 3D por GPU" in out, "«gpu3d probar» sin GPU: lo dice y no hace nada")
        out = m.comando("sgldemo -g 3", 120)
        verificar("sin 3D por GPU, SaguiGL por software" in out, "«sgldemo -g» sin GPU: SaguiGL por software")
        out = m.comando("sgldemo -t 5", 120)
        verificar(re.search(r"sgldemo: cubo, 5 cuadros.*fps", out), "SaguiGL por software sigue dibujando")
        out = m.comando("gpu3d encender", 30)
        verificar("sin GPU con driver 3D" in out, "«gpu3d encender» sin driver no hace nada")
        sano(m.serial(), "std")


def simulado():
    img = imagem_de_teste("", ["--vesa", "--gpu3d=simulado"])
    with Maquina("h285m", "ide", imagem=img) as m:
        arrancar(m)
        s = m.serial()
        verificar("[GPU3D] simulado: driver simulado" in s and "[GPU3D] encender" not in s,
                  "--gpu3d=simulado: elegido al arrancar, sin encender hasta el primer uso")
        out = m.comando("gpu3d", 30)
        verificar("3D por GPU: sin probar" in out and "driver simulado" in out, "«gpu3d»: sin probar todavía")
        out = m.comando("gpu3d probar --detalle", 300)
        print("   ", "\n    ".join(l for l in out.splitlines() if "gpu3d probar" in l)[:1500])
        verificar("gpu3d probar: BIEN" in out, "«gpu3d probar»: autoprueba y medida bien")
        verificar(len(re.findall(r"gpu3d probar:\s+\d \S.*\bbien\s*$", out, re.M)) == 8,
                  "las ocho pruebas (limpiar, color, Z, mezcla, textura, interpolado, especular, Z de 16 bits)")
        p = re.search(r"puntuacion 3D: (\d+)", out)
        verificar(p and int(p.group(1)) > 0, f"hay puntuación 3D ({p.group(1) if p else '?'})")
        time.sleep(1)
        s = m.serial()
        verificar("[GPU3D] autoprueba: BIEN" in s and "[GPU3D] activo:" in s and "[GPU3D] medida:" in s
                  and "[GPU3D] simulado tras la prueba" in s, "el log [GPU3D] tiene autoprueba, activación, medida y detalle")
        out = m.comando("sgldemo -g 4", 300)
        print("   ", "\n    ".join(l for l in out.splitlines() if l.startswith("gpu ") or "sgldemo" in l)[:1200])
        for cena in ("triangulo", "cubo", "zcruzado", "textura"):
            verificar(re.search(rf"^gpu {cena}: .*: BIEN$", out, re.M), f"«sgldemo -g»: la escena {cena} por la GPU se parece a la de software")
        verificar(re.search(r"cuadros por la GPU: \d+ ms", out) and "sgldemo -g: BIEN" in out,
                  "«sgldemo -g»: el cubo de 320x240 por la GPU y veredicto BIEN")
        out = m.comando("gpu3d", 30)
        t = re.search(r"triangulos (\d+) en (\d+) entregas, cuadros (\d+), fallos (\d+)", out)
        verificar("3D por GPU: activo" in out and "motor libre" in out, "«gpu3d»: activo y el motor libre al terminar el programa")
        verificar(t and int(t.group(1)) >= 20 and int(t.group(3)) >= 8 and int(t.group(4)) == 0,
                  f"contadores: {t.group(0) if t else '?'}")
        out = m.comando("infopc --bench-gpu", 600)
        verificar(re.search(r"3D por GPU:\s+activo: driver simulado", out) and re.search(r"Puntuación 3D:\s+\d+", out)
                  and "Puntuación 2D total:" in out, "infopc --bench-gpu: 3D activo y puntuación 3D junto a la 2D")
        out = m.comando("gpu3d apagar", 30)
        verificar("apagado a mano" in out, "«gpu3d apagar»")
        out = m.comando("sgldemo -g 2", 120)
        verificar("sin 3D por GPU, SaguiGL por software" in out, "apagado: SaguiGL por software")
        out = m.comando("gpu3d encender", 120)
        verificar("gpu3d encender: activo" in out, "«gpu3d encender»: se vuelve a probar y queda activo")
        out = m.comando("gpu3d encender anillo", 120)
        verificar("gpu3d encender: activo" in out and "anillo" in out, "«gpu3d encender anillo»: la otra variante")
        out = m.comando("sgldemo -g 2", 300)
        verificar("sgldemo -g: BIEN" in out, "y sigue dibujando igual")
        m.captura("H285-simulado")
        sano(m.serial(), "simulado")


def fallos():
    img = imagem_de_teste("", ["--vesa", "--gpu3d=simulado", "--gpu3d-prueba=pixel"])
    with Maquina("h285fp", "ide", imagem=img) as m:
        arrancar(m)
        out = m.comando("sgldemo -g 2", 120)
        verificar("sin 3D por GPU, SaguiGL por software" in out and "autoprueba fallida" in out,
                  "(pixel) el primer uso pasa la autoprueba, falla y SaguiGL sigue por software")
        time.sleep(1)
        s = m.serial()
        verificar("[GPU3D] apagado: autoprueba fallida: prueba 1 (rectangulo de color)" in s and "(simulado)" in s,
                  "(pixel) el log dice qué prueba y qué píxel")
        out = m.comando("gpu3d", 30)
        verificar("apagado por un fallo (software)" in out and re.search(r"fallos [1-9]", out), "(pixel) «gpu3d» dice apagado por un fallo")
        out = m.comando("gpu3d probar", 120)
        verificar("gpu3d probar: MAL" in out and re.search(r"1 rectangulo de color\s+MAL", out) and "se esperaba" in out,
                  "(pixel) «gpu3d probar» repite, dice la prueba y el píxel")
        out = m.comando("sgldemo -t 3", 120)
        verificar(re.search(r"sgldemo: cubo, 3 cuadros.*fps", out), "(pixel) SaguiGL por software funciona")
        sano(m.serial(), "fallo de píxel")
    img = imagem_de_teste("", ["--vesa", "--gpu3d=simulado", "--gpu3d-prueba=tiempo"])
    with Maquina("h285ft", "ide", imagem=img) as m:
        arrancar(m)
        out = m.comando("sgldemo -g 2", 300)
        time.sleep(1)
        s = m.serial()
        verificar("[GPU3D] activo:" in s and "[GPU3D] apagado: tiempo agotado esperando a la GPU (simulado)" in s,
                  "(tiempo) la autoprueba pasa y después el vigilante apaga el 3D")
        verificar("se apago a medias" in out or "sin 3D por GPU" in out, "(tiempo) el programa lo nota y no se cuelga")
        out = m.comando("gpu3d", 30)
        verificar("apagado por un fallo (software)" in out and "motor libre" not in out, "(tiempo) «gpu3d» dice apagado por un fallo")
        out = m.comando("sgldemo -g 2", 120)
        verificar("sin 3D por GPU, SaguiGL por software" in out, "(tiempo) después todo va por software")
        out = m.comando("sgldemo -t 3", 120)
        verificar(re.search(r"sgldemo: cubo, 3 cuadros.*fps", out), "(tiempo) SaguiGL por software funciona")
        sano(m.serial(), "vigilante")
    img = imagem_de_teste("", ["--vesa", "--gpu3d=no"])
    with Maquina("h285fn", "ide", imagem=img) as m:
        arrancar(m)
        out = m.comando("gpu3d", 30)
        verificar("desactivado por la opcion de arranque" in out and "--gpu3d=no" in out, "--gpu3d=no: desactivado")
    img = imagem_de_teste("", ["--vesa", "--gpu3d=simulado", "--sgl-motor", "software"])
    with Maquina("h285fs", "ide", imagem=img) as m:
        arrancar(m)
        out = m.comando("gpu3d", 30)
        verificar("desactivado por la opcion de arranque" in out and "SGL_MOTOR=software" in out,
                  "SGL_MOTOR=software (D158) manda sobre todo lo demás")


def main():
    todas = {"unidad": unidad, "fuentes": fuentes, "std": std, "simulado": simulado, "fallos": fallos}
    quais = [a for a in sys.argv[1:] if a in todas] or list(todas)
    try:
        for q in quais:
            print(f"== {q} ==")
            todas[q]()
    except FalhaTeste as e:
        print("FALHOU:", e)
        return 1
    print("H285: 3D por la GPU (capa común e Intel Gen3): BIEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
