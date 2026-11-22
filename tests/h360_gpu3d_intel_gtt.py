#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H360 - 3D de Intel Gen3 completo: encendido por etapas, RAM del sistema en el GTT (D358).

QEMU no emula el 3D de ninguna GPU de Intel: el driver de verdad
(kernel/dispositivos/gpu3d_intel.c) solo se prueba en una máquina con
915/945/G33. Aquí se prueba lo que sí se puede:

  unidad    en el PC: tests/gen3_gtt_unidad.c (memoria robada de GMCH_CTRL,
            ventana del GTT elegida y libre, formato de la entrada, tamaño según la
            memoria libre, un GTT de BIOS simulado, la copia del blitter) y
            tests/gen3_3d_unidad.c (las órdenes 3D siguen bien formadas);
  fuentes   lo que tiene que cumplir el driver: modelos exactos, siete etapas en el
            log, el GTT solo se escribe en la ventana comprobada y sus páginas no se
            liberan nunca una vez escritas, plazos, EIR tras cada espera, la
            variante en la ABI y en mkimagem, D358 y la documentación;
  std       QEMU sin GPU con 3D: «gpu3d encender gtt» no hace nada y lo dice;
  simulado  --gpu3d=simulado: «gpu3d encender gtt» y «encender anillo gtt» pasan
            por la capa (ctl 3 y 4), la autoprueba y la medida siguen bien, el
            estado lo enseña, una orden mal escrita da el uso, y una imagen con
            --gpu3d-gtt lo pide desde el primer uso.

Uso: h360_gpu3d_intel_gtt.py [unidad] [fuentes] [std] [simulado]
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(RAIZ, "build", "h360")


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


def compilar_e_correr(fonte, nome, marca):
    exe = os.path.join(DIR, nome)
    r = subprocess.run(["gcc", "-O1", "-fno-builtin", "-Wall", "-Wno-builtin-declaration-mismatch",
                        "-I", os.path.join(RAIZ, "kernel", "inc"), "-I", os.path.join(RAIZ, "kernel", "dispositivos"),
                        "-o", exe, os.path.join(RAIZ, "tests", fonte),
                        os.path.join(RAIZ, "kernel", "gui", "gpu3d_mem.c")], capture_output=True, text=True)
    verificar(r.returncode == 0, f"{fonte} compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", r.stdout.strip().replace("\n", "\n    ")[-500:])
    verificar(r.returncode == 0 and marca in r.stdout, f"{fonte}: BIEN")


def unidad():
    os.makedirs(DIR, exist_ok=True)
    compilar_e_correr("gen3_gtt_unidad.c", "gen3_gtt_unidad", "0 fallos: BIEN")
    compilar_e_correr("gen3_3d_unidad.c", "gen3_3d_unidad", "0 fallos: BIEN")


def fuentes():
    drv = ler("kernel/dispositivos/gpu3d_intel.c")
    for modelo in ("0x2582", "0x2592", "0x2772", "0x27A2", "0x27AE", "0x29B2", "0x29C2", "0x29D2", "0xA001", "0xA011"):
        verificar(modelo in drv, f"gpu3d_intel.c: el modelo {modelo} está en la lista exacta")
    verificar("modelo_ok(d->modelo)" in drv and "geracao != 3" in drv, "comprueba el modelo antes de tocar nada")
    for n in range(1, 8):
        verificar(f"etapa {n}:" in drv, f"el log dice la etapa {n}")
    verificar("wr(" not in drv and "while (1)" not in drv and "for (;;)" not in drv, "sin registros MMIO escritos ni bucles sin plazo")
    # El GTT: solo en la ventana comprobada (ram_montar) y al devolverla (ram_vetar).
    escrituras = re.findall(r"ram\.pte\[[^\]]+\]\s*=[^=]", drv)
    verificar(len(escrituras) == 2, f"el GTT se escribe en dos sitios: al montar la ventana y al devolverla ({len(escrituras)})")
    montar = drv[drv.index("static int ram_montar"):drv.index("static int ram_provar")]
    verificar(montar.index("g3_gtt_livre") < montar.index("ram.pte[i] = g3_gtt_entrada"),
              "la ventana se comprueba libre antes de escribir")
    verificar(montar.index("ram.pronta = true") < montar.index("ram.pte[i] = g3_gtt_entrada")
              and "memfisica_liberar" not in montar[montar.index("ram.pronta = true"):],
              "escritas las entradas, sus páginas no se liberan nunca")
    verificar("sonda_ler(gmadr + gfx + i * PAGINA)" in montar, "la CPU comprueba cada página por la apertura")
    verificar("GPU3D_CAB_GTT" in drv and "ram.vetada" in drv, "solo si se pide, y no se repite tras un fallo")
    verificar("g3_gtt_janela(robada, vbe_fim, bytes)" in montar and "a->fb_gfx + tela.memoria" in montar,
              "la ventana queda por encima del final del VBE (fb_gfx + memoria: ahí está el ring del 2D)")
    verificar("(void)*ram.ifp" not in drv and "ram_vetar_colgada(" in drv,
              "la página de vaciado solo se escribe; una GPU colgada en la etapa 6 deja la RAM vetada")
    verificar(drv.count("eir_bem(") >= 3 and "EIR_GRAVES" in drv, "EIR tras las esperas")
    verificar("G3_MI_FLUSH | G3_MI_FLUSH_LEITURA" in drv, "cada entrega invalida las cachés de lectura")
    verificar("por_anel = true" in drv, "si el lote no hace lo que lleva, se sigue en el anillo")
    verificar("cache_vaciar(b_cor.cpu" in drv and "chipset_vaciar()" in drv, "caché de la CPU vaciada antes de leer y al subir texturas")
    gtt = ler("kernel/dispositivos/gen3_gtt.h")
    verificar("GPL-2.0-or-later" in gtt.split("\n")[0] and "G3_GTT_LIMITE" in gtt, "gen3_gtt.h: SPDX y ventana por debajo de 128 MB")
    verificar("#define GPU3D_CAB_GTT       0x40" in ler("kernel/inc/gpu3d.h"), "gpu3d.h: bit 0x40 de CAB_GPU3D")
    abi = ler("kernel/inc/abi.h")
    verificar("GPU3D_VAR_GTT" in abi and "GPU3D_VAR_ANEL" in abi, "abi.h: los bits de la variante")
    verificar("--gpu3d-gtt" in ler("tools/mkimagem.py") and "0x40 if a.gpu3d_gtt" in ler("tools/mkimagem.py"),
              "mkimagem.py --gpu3d-gtt")
    capa = ler("kernel/gui/gpu3d.c")
    verificar(re.search(r"op < 1 \|\| op > [4-9]", capa) is not None and "variante_real" in capa,
              "gpu3d.c: ctl 3 y 4 (y las que añadieron D406 y siguientes), y la variante real")
    verificar("encender [anillo] [gtt" in ler("userland/programas/gpu3d.c"), "gpu3d: «encender [anillo] [gtt…]»")
    verificar("## D358." in ler("docs/decisoes.md"), "la decisión D358 está en docs/decisoes.md")
    doc = ler("docs/gpu3d.md")
    verificar("gpu3d encender gtt" in doc and "etapa 6" in doc and "D358" in doc, "docs/gpu3d.md: RAM en el GTT y etapas")
    verificar("D358" in ler("docs/gpus.md"), "docs/gpus.md: la fila de Intel Gen3 cita la D358")
    verificar("gen3_gtt.h" in ler("docs/terceros.md"), "docs/terceros.md: las referencias del GTT")


def std():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h360s", "ide", imagem=img) as m:
        arrancar(m)
        out = m.comando("gpu3d encender gtt", 30)
        verificar("sin GPU con driver 3D" in out, "«gpu3d encender gtt» sin driver no hace nada")
        out = m.comando("gpu3d", 30)
        verificar("3D por GPU: sin GPU con driver 3D (software)" in out, "«gpu3d» sigue igual")
        sano(m.serial(), "std")


def simulado():
    img = imagem_de_teste("", ["--vesa", "--gpu3d=simulado"])
    with Maquina("h360m", "ide", imagem=img) as m:
        arrancar(m)
        out = m.comando("gpu3d encender gtt", 120)
        verificar("gpu3d encender: activo" in out, "«gpu3d encender gtt»: activo (ctl 3)")
        time.sleep(1)
        s = m.serial()
        verificar("[GPU3D] encender simulado (simulada (pruebas)), RAM en el GTT" in s
                  and "RAM en el GTT pedida (D358)" in s, "el log dice que se pidió la RAM en el GTT")
        out = m.comando("gpu3d", 30)
        verificar("3D por GPU: activo" in out and "por lotes" in out and "RAM del sistema en el GTT" in out,
                  "«gpu3d»: activo, por lotes y con la RAM del GTT pedida")
        out = m.comando("gpu3d probar --detalle", 300)
        verificar("gpu3d probar: BIEN" in out, "la autoprueba y la medida siguen bien")
        out = m.comando("gpu3d encender anillo gtt", 120)
        verificar("gpu3d encender: activo" in out and "anillo" in out, "«gpu3d encender anillo gtt» (ctl 4)")
        out = m.comando("gpu3d", 30)
        verificar("en el anillo" in out and "RAM del sistema en el GTT" in out, "«gpu3d»: anillo y RAM en el GTT")
        out = m.comando("gpu3d encender raro", 30)
        verificar("uso: gpu3d encender [anillo] [gtt" in out, "una opción desconocida da el uso")
        out = m.comando("gpu3d encender", 120)
        verificar("gpu3d encender: activo" in out, "«gpu3d encender» vuelve a lotes sin la RAM del GTT")
        out = m.comando("gpu3d", 30)
        verificar("por lotes" in out and "RAM del sistema en el GTT" not in out, "«gpu3d»: sin la RAM del GTT")
        out = m.comando("sgldemo -g 2", 300)
        verificar("sgldemo -g: BIEN" in out, "SaguiGL por la GPU sigue igual")
        out = m.comando("gpu3d encender gtt", 120)
        out = m.comando("infopc -t gpu", 120)
        # El simulado no usa la RAM del GTT: infopc dice que está pedida, no que esté en uso (revisión de la D358).
        verificar(re.search(r"3D por GPU:\s+activo: driver simulado.*RAM del GTT pedida \(este driver no la usa\)", out, re.S)
                  and "destino en la RAM del GTT" not in out, "infopc -t gpu: el 3D activo y la RAM del GTT pedida")
        m.captura("H360-simulado")
        sano(m.serial(), "simulado")
    img = imagem_de_teste("", ["--vesa", "--gpu3d=simulado", "--gpu3d-gtt"])
    with Maquina("h360g", "ide", imagem=img) as m:
        arrancar(m)
        out = m.comando("gpu3d probar", 300)
        verificar("gpu3d probar: BIEN" in out, "--gpu3d-gtt: la autoprueba con el primer uso")
        time.sleep(1)
        s = m.serial()
        verificar("RAM en el GTT pedida (D358)" in s, "--gpu3d-gtt: la imagen lo pide desde el arranque")
        out = m.comando("gpu3d", 30)
        verificar("RAM del sistema en el GTT" in out, "--gpu3d-gtt: «gpu3d» lo enseña")
        sano(m.serial(), "--gpu3d-gtt")


def main():
    todas = {"unidad": unidad, "fuentes": fuentes, "std": std, "simulado": simulado}
    quais = [a for a in sys.argv[1:] if a in todas] or list(todas)
    try:
        for q in quais:
            print(f"== {q} ==")
            todas[q]()
    except FalhaTeste as e:
        print("FALHOU:", e)
        return 1
    print("H360: 3D de Intel Gen3 con la RAM en el GTT (D358): BIEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
