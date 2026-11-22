#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H432 - Presentar por la GPU sin volver a la RAM (D432, docs/gpu3d.md).

Un programa de la SDL 2 a pantalla completa con el renderizador por GPU presenta con
GPU3D_OP_APRESENTAR: la GPU copia (y escala) el cuadro a la superficie visible de la pantalla y el
compositor le cede la pantalla mientras dure.

Partes (por defecto, todas; o las pedidas: h432_presentar_gpu.py unidad fuentes std simulado):
  unidad     en el PC: las bandas negras y los vértices de la copia (gpu3d_mem.c), la pantalla como
             destino en R5xx (r500_pacotes.h), y en R6xx/R7xx y Evergreen con direcciones de 40 bits
             (r600_3d_pacotes.c, evergreen_3d_pacotes.c); los destinos de siempre no cambian.
  fuentes    lo que tiene que estar en el árbol (la operación, el bit de capacidad, la función de los
             cuatro drivers, la exclusiva del compositor, la SDL, la decisión y los documentos).
  std        QEMU sin GPU 3D: /apps/bin/sdl2presentar va por software y nada cambia.
  simulado   --gpu3d=simulado: la «GPU» escribe en el framebuffer de la tarjeta (lo ve una captura del
             QEMU): el cuadro aparece escalado y con bandas negras; el compositor no pinta encima
             mientras dura; la pantalla vuelve con Alt+Tab (y se cede otra vez al volver a pantalla completa), con un aviso
             y al salir; en ventana, con SDL_SAGUI_GPU_PRESENTAR=0 o con un driver que dice «no puedo»
             todo sigue por el camino de siempre.
"""
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA, APPS_RAIZ  # noqa: E402
import h228_openjazz as h228  # noqa: E402

DIR = os.path.join(RAIZ, "build", "h432")
ROJO, VERDE, AZUL, BLANCO, NEGRO = (255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 255), (0, 0, 0)


def ler(caminho):
    with open(os.path.join(RAIZ, caminho), encoding="utf-8") as f:
        return f.read()


def unidad():
    os.makedirs(DIR, exist_ok=True)
    inc = ["-I", os.path.join(RAIZ, "kernel", "inc"), "-I", os.path.join(RAIZ, "kernel", "dispositivos")]
    exe = os.path.join(DIR, "g3d_apres_unidad")
    r = subprocess.run(["gcc", "-O1", "-fno-builtin", "-Wall", "-Wno-builtin-declaration-mismatch"] + inc +
                       ["-o", exe, os.path.join(RAIZ, "tests", "g3d_apres_unidad.c"), os.path.join(RAIZ, "kernel", "gui", "gpu3d_mem.c")],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "g3d_apres_unidad compila\n" + r.stderr)
    r = subprocess.run([exe], capture_output=True, text=True)
    print("   ", r.stdout.strip().replace("\n", "\n    ")[-600:])
    verificar(r.returncode == 0 and re.search(r"\d+ comprobaciones, 0 fallos", r.stdout),
              "bandas, vértices de la copia y la pantalla como destino en R5xx")
    fontes = [os.path.join(RAIZ, "tests", "g3d_apres_r600_unidad.c"),
              os.path.join(RAIZ, "kernel", "dispositivos", "r600_3d_pacotes.c"),
              os.path.join(RAIZ, "kernel", "dispositivos", "evergreen_3d_pacotes.c")]
    for nome, flags in (("g3d_apres_r600_unidad", ["-O2", "-Wall", "-Wextra", "-Werror"]),
                        ("g3d_apres_r600_unidad_asan", ["-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=all"])):
        exe = os.path.join(DIR, nome)
        r = subprocess.run(["gcc"] + flags + ["-DR600_ANFITRION", "-I", os.path.join(RAIZ, "kernel", "inc"), "-o", exe] + fontes + ["-lm"],
                           capture_output=True, text=True)
        verificar(r.returncode == 0, f"{nome} compila\n" + r.stderr)
        r = subprocess.run([exe], capture_output=True, text=True)
        print("   ", (r.stdout + r.stderr).strip().replace("\n", "\n    ")[-600:])
        verificar(r.returncode == 0 and re.search(r"\d+ comprobaciones, 0 fallos", r.stdout),
                  f"{nome}: la pantalla como destino en R6xx/R7xx y Evergreen (40 bits), sin Z")


def fuentes():
    abi = ler("kernel/inc/abi.h")
    verificar("GPU3D_OP_APRESENTAR  14" in abi and "G3D_CAP_APRESENTAR" in abi and "gpu3d_apresentar_t" in abi,
              "abi.h: la operación 14, su estructura y el bit de capacidad")
    verificar("int  (*apresentar)(const g3d_apres_t *p);" in ler("kernel/inc/gpu3d.h"), "gpu3d.h: la función del driver, al final")
    for f, nome in (("gpu3d_r500.c", "r5_apresentar"), ("gpu3d_r600.c", "r3_apresentar"), ("gpu3d_evergreen.c", "eg_apresentar"),
                    ("gpu3d_sim.c", "s_apresentar")):
        s = ler("kernel/dispositivos/" + f)
        verificar(re.search(r"\n    " + nome + r",", s) and "static int " + nome in s, f"{f}: {nome} en la tabla del driver")
    # Seguridad: de la pantalla, los drivers de verdad solo LEEN registros (ninguna escritura en 0x6000-0x7FFF).
    for f in ("gpu3d_r500.c", "gpu3d_r600.c", "gpu3d_evergreen.c"):
        s = ler("kernel/dispositivos/" + f)
        i = s.index("- D432: presentar en la pantalla")
        trozo = s[i:s.index("\nconst gpu3d_driver_t" if "evergreen" in f else "/* Lo que se anuncia", i)]
        verificar(not re.search(r"\bwr\(", trozo), f"{f}: presentar no escribe ningún registro por MMIO")
        verificar("prazo" in trozo or "sincronizar(GPU3D_PRAZO_MS)" in trozo, f"{f}: las esperas de presentar tienen plazo")
    s = ler("kernel/dispositivos/gpu3d_r500.c")
    i = s.index("static int r5_apresentar")
    trozo = s[i:s.index("/* Lo que se anuncia", i)]
    verificar(trozo.index("r5_vaciar(&f)") < trozo.index("desenhar_int("), "R5xx: vaciar las cachés antes de usar el destino como textura (D426)")
    j = ler("kernel/gui/janelas.c")
    for pedaco in ("excl_impedimento", "ocio_cobre()", "avisos_visivel()", "atalhos_visivel()", "sessao_bloqueada()", "gui_exclusiva_apresentar"):
        verificar(pedaco in j, f"janelas.c: {pedaco}")
    verificar("if (exclusiva) { suj_x0 = suj_x1 = 0; return; }" in ler("kernel/dispositivos/video.c"), "video.c: video_mostrar no escribe con la pantalla cedida")
    g = ler("userland/libsdl2/sdl2gpu.h")
    for pedaco in ("GPU3D_OP_APRESENTAR", "SDL_SAGUI_GPU", "SDL_SAGUI_GPU_PRESENTAR", "SDL_SAGUI_GPU_VSYNC", "sdlg_adiar", "SAGUI_SDL2_GPU"):
        verificar(pedaco in g, f"sdl2gpu.h: {pedaco}")
    verificar("int sagui_sdl2_gpu = 0;" in g, "el renderizador por GPU sigue siendo opcional (apagado por defecto)")
    verificar('cfg_get_int("render2d", 0)' in ler("userland/juegos/aoen/main.c"), "AoEN: el renderizador 2D sigue apagado por defecto (D394)")
    verificar("## D432." in ler("docs/decisoes.md"), "docs/decisoes.md tiene la D432")
    d = ler("docs/gpu3d.md")
    verificar("Presentar sin volver a la RAM" in d and "GPU3D_OP_APRESENTAR" in d, "docs/gpu3d.md: «Presentar sin volver a la RAM»")
    verificar("SDL_SAGUI_GPU" in ler("docs/sdl-gpu.md"), "docs/sdl-gpu.md dice cómo se enciende")
    verificar("SPDX-License-Identifier" in ler("tests/h432_presentar_gpu.py"), "la prueba lleva SPDX")


def lanzar(m, orden):
    """Escribe la orden en el terminal sin esperar a que termine. Devuelve la marca del registro."""
    marca = len(m.serial())
    m.digitar(orden, 0.04)
    m.tecla("ret", 0.04)
    return marca


def esperar(m, patron, tempo=60, desde=0):
    limite = time.time() + tempo
    while time.time() < limite:
        r = re.findall(patron, m.serial()[desde:])
        if r:
            return r[-1]
        time.sleep(0.2)
    verificar(False, f"no llegó «{patron}» al registro en {tempo} s: ..." + m.serial()[desde:][-600:])


def px(pant, x, y):
    w, h, rgb = pant
    return tuple(rgb[(y * w + x) * 3:(y * w + x) * 3 + 3])


def parecido(a, b, tol=6):
    return all(abs(a[i] - b[i]) <= tol for i in range(3))


def patron_en_pantalla(pant, y0, alto):
    """¿Están los cuatro cuartos del patrón (rojo, verde, azul, blanco) en la franja y0..y0+alto de la pantalla?"""
    w = pant[0]
    quer = ((w // 4, y0 + alto // 4, ROJO), (3 * w // 4, y0 + alto // 4, VERDE),
            (w // 4, y0 + 3 * alto // 4, AZUL), (3 * w // 4, y0 + 3 * alto // 4, BLANCO))
    return [(x, y, c, px(pant, x, y)) for x, y, c in quer if not parecido(px(pant, x, y), c)]


def medida(out):
    r = re.search(r"\[SDL2PRES\] medida: (\d+) cuadros en (\d+) ms: ([\d.]+) fps; directos (\d+), por la RAM (\d+)", out)
    verificar(r, "sdl2presentar escribe su medida (" + out.strip()[-300:] + ")")
    return int(r.group(1)), float(r.group(3)), int(r.group(4)), int(r.group(5))


def sano(s):
    verificar("EXCECAO" not in s and "PANICO" not in s and "falta de pagina" not in s.lower(), "sin excepciones ni pánicos")


def std():
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h432-std", "ide", imagem=img) as m:
        h228.terminal(m)
        out = m.comando("/apps/bin/sdl2presentar -n 40", 200)
        verificar("[SDL2PRES] renderizador saguios gpu=0" in out and "completa=1" in out, "sin GPU 3D: por software, a pantalla completa")
        n, fps, dirs, ram = medida(out)
        print(f"    sin GPU: {n} cuadros, {fps} fps; directos {dirs}, por la RAM {ram}")
        verificar(n == 40 and dirs == 0 and ram == 0, "sin GPU no hay presentaciones directas ni lecturas de la GPU")
        s = m.serial()
        verificar("pantalla exclusiva" not in s, "sin GPU el compositor no cede la pantalla")
        out = m.comando("gpu3d", 30)
        verificar("presentar en la pantalla" not in out, "«gpu3d» sin 3D activo no habla de presentar")
        sano(m.serial())


def simulado():
    img = imagem_de_teste("", ["--vesa", "--gpu3d=simulado", "--apps", APPS_RAIZ])
    with Maquina("h432-sim", "ide", imagem=img) as m:
        h228.terminal(m)
        out = m.comando("gpu3d probar", 300)
        verificar("gpu3d probar: BIEN" in out, "la autoprueba de gpu3d pasa")
        verificar("presentar sin volver a la RAM (D432): si" in m.serial(), "el log dice que el driver sabe presentar")
        out = m.comando("gpu3d", 30)
        verificar(re.search(r"presentar en la pantalla \(D432\): si; 0 cuadros directos, 0 por la RAM; pantalla en exclusiva: nadie", out),
                  "«gpu3d» lo enseña: sí, 0 cuadros, nadie con la pantalla")
        pant = h228.pantalla(m, "h432-antes")
        W, H = pant[0], pant[1]
        print(f"    pantalla de {W}x{H}")

        # 1. La medida: una textura por cuadro y un RenderCopy que cubre el lienzo (16:9 en 4:3: bandas arriba y abajo).
        marca = len(m.serial())
        out = m.comando("/apps/bin/sdl2presentar -n 60 -l 320x180", 200)
        verificar("[SDL2PRES] renderizador saguios-gpu gpu=1 lienzo 320x180 completa=1 modo=textura" in out, "renderizador por GPU a pantalla completa")
        n, fps, dirs, ram = medida(out)
        print(f"    textura por cuadro: {n} cuadros, {fps} fps; directos {dirs}, por la RAM {ram}")
        verificar(dirs == 60 and ram == 0, f"los 60 cuadros van directos a la pantalla y ninguno vuelve a la RAM ({dirs}, {ram})")
        s = m.serial()[marca:]
        da = W * 180 // 320
        dy = (H - da) // 2
        verificar(re.search(r"\[GUI\] pantalla exclusiva: cedida a la ventana \d+", s), "el compositor cede la pantalla")
        verificar(re.search(rf"\[GPU3D\] presentar \(D432\): fuente textura 320x180\+0\+0 de 320x180 -> pantalla {W}x{da}\+0\+{dy} de {W}x{H}", s),
                  "la capa copia la textura entera, escalada y centrada")
        verificar(re.search(r"\[GUI\] pantalla exclusiva: la ventana \d+ la devuelve tras 60 cuadros", s), "al salir, la pantalla vuelve al compositor")
        verificar("[SDL2PRES] presentar por la GPU en la pantalla (D432): lienzo 320x180" in s, "la SDL lo dice en su registro")
        out = m.comando("gpu3d", 30)
        verificar(re.search(r"presentar en la pantalla \(D432\): si; 60 cuadros directos, 0 por la RAM; pantalla en exclusiva: nadie", out),
                  "«gpu3d» cuenta los 60 cuadros directos")
        t = re.search(r"cuadros (\d+), fallos (\d+)", out)
        verificar(t and int(t.group(2)) == 0, "sin fallos de la GPU")

        # 2. Patrón fijo: está en la pantalla (escalado, con bandas negras) y el compositor no pinta encima.
        marca = lanzar(m, "/apps/bin/sdl2presentar -n 5 -l 320x180 -e 40")
        esperar(m, r"\[SDL2PRES\] patron fijo", 60, marca)
        time.sleep(1.5)
        devueltas = len(re.findall(r"la devuelve", m.serial()[marca:]))
        for k in range(3):
            pant = h228.pantalla(m, f"h432-patron{k}")
            mal = patron_en_pantalla(pant, dy, da)
            verificar(not mal, f"captura {k}: los cuatro cuartos del patrón, escalados a {W}x{da} en y={dy} ({mal})")
            verificar(parecido(px(pant, W // 2, dy // 2), NEGRO) and parecido(px(pant, W // 2, H - dy // 2), NEGRO) and
                      parecido(px(pant, 5, 5), NEGRO), f"captura {k}: bandas negras arriba y abajo")
            verificar(parecido(px(pant, 3, dy + 3), ROJO) and parecido(px(pant, W - 4, dy + da - 4), BLANCO),
                      f"captura {k}: el patrón llega a las esquinas del rectángulo de destino")
            m.monitor("mouse_move 30 20")   # el ratón no hace pintar al compositor
            time.sleep(1.2)
        verificar(len(re.findall(r"la devuelve", m.serial()[marca:])) == devueltas, "mientras dura, el compositor no recupera la pantalla")
        m.captura("H432-presentar")

        # 3. Alt+Tab: la pantalla vuelve al compositor (se ve el escritorio); otra vez Alt+Tab y se cede de nuevo.
        cedidas = len(re.findall(r"pantalla exclusiva: cedida", m.serial()[marca:]))
        m.tecla("alt-tab", 0.5)
        mot = esperar(m, r"pantalla exclusiva: la ventana \d+ la devuelve tras \d+ cuadros \(([^)]*)\)", 20, marca)
        print(f"    Alt+Tab: el compositor recupera la pantalla ({mot})")
        time.sleep(2.0)
        pant = h228.pantalla(m, "h432-alttab")
        verificar(not parecido(px(pant, W // 2, dy // 2), NEGRO) or not parecido(px(pant, 5, 5), NEGRO) or patron_en_pantalla(pant, dy, da),
                  "con otra ventana delante ya no está el cuadro a pantalla completa con sus bandas")
        m.captura("H432-alt-tab")
        # Alt+Tab saca a la ventana de la pantalla completa (como siempre): al volver sigue en ventana, presentando
        # por la RAM; con F11 vuelve a pantalla completa y la pantalla se le cede otra vez.
        m.tecla("alt-tab", 0.5)
        esperar(m, r"alt\+tab: ativada janela \d+\r?\n(?:.*\n)*?.*\[SDL2PRES\] foco ganado", 20, marca)
        time.sleep(1.0)
        verificar(len(re.findall(r"pantalla exclusiva: cedida", m.serial()[marca:])) == cedidas, "en ventana no se cede la pantalla")
        m.tecla("f11", 0.5)
        limite = time.time() + 20
        while time.time() < limite and len(re.findall(r"pantalla exclusiva: cedida", m.serial()[marca:])) <= cedidas:
            time.sleep(0.3)
        verificar(len(re.findall(r"pantalla exclusiva: cedida", m.serial()[marca:])) > cedidas, "al volver a pantalla completa la pantalla se cede otra vez")
        time.sleep(1.5)
        pant = h228.pantalla(m, "h432-vuelta")
        mal = patron_en_pantalla(pant, dy, da)
        verificar(not mal and parecido(px(pant, W // 2, dy // 2), NEGRO), f"y el patrón vuelve a estar en la pantalla con sus bandas ({mal})")
        r = esperar(m, r"\[SDL2PRES\] fin: directos (\d+), por la RAM (\d+)", 80, marca)
        print(f"    patrón fijo con Alt+Tab: directos {r[0]}, por la RAM {r[1]}")
        verificar(int(r[0]) > 100 and int(r[1]) > 0, "hubo cuadros directos y, sin la pantalla, cuadros por la RAM")
        time.sleep(2.5)
        pant = h228.pantalla(m, "h432-fin")
        verificar(patron_en_pantalla(pant, dy, da), "al salir el programa el escritorio vuelve a pintarse entero")
        out = m.comando("gpu3d", 30)
        verificar("pantalla en exclusiva: nadie" in out and "motor libre" in out, "nadie se queda con la pantalla ni con el motor")

        # 4. Un aviso en pantalla: el compositor la recupera, lo pinta y la vuelve a ceder cuando caduca.
        marca = lanzar(m, "/apps/bin/sdl2presentar -n 5 -e 30 -a")
        esperar(m, r"\[SDL2PRES\] aviso pedido", 60, marca)
        mot = esperar(m, r"pantalla exclusiva: la ventana \d+ la devuelve tras \d+ cuadros \(([^)]*)\)", 20, marca)
        verificar(mot == "aviso", f"el aviso hace que el compositor recupere la pantalla ({mot})")
        time.sleep(1.0)
        pant = h228.pantalla(m, "h432-aviso")
        verificar(parecido(px(pant, W // 4, 3 * H // 4), AZUL), "con el aviso, el juego sigue viéndose (presentado por la RAM)")
        m.captura("H432-aviso")
        r = esperar(m, r"\[SDL2PRES\] fin: directos (\d+), por la RAM (\d+)", 80, marca)
        s = m.serial()[marca:]
        n_ced = len(re.findall(r"pantalla exclusiva: cedida", s))
        print(f"    aviso: directos {r[0]}, por la RAM {r[1]}; la pantalla se cedió {n_ced} veces")
        verificar(n_ced >= 2 and int(r[1]) > 0, "caducado el aviso, la pantalla se cede de nuevo")
        time.sleep(2)
        h136_foco(m)

        # 5. Dibujo suelto (borrado, rectángulos, copias): la fuente es el destino del 3D.
        marca = lanzar(m, "/apps/bin/sdl2presentar -n 30 -m dibujo -e 8")
        esperar(m, r"\[SDL2PRES\] patron fijo", 60, marca)
        time.sleep(1.5)
        pant = h228.pantalla(m, "h432-dibujo")
        mal = patron_en_pantalla(pant, 0, H)
        verificar(not mal, f"modo dibujo: el patrón (320x240 en 4:3) llena la pantalla ({mal})")
        r = esperar(m, r"\[SDL2PRES\] fin: directos (\d+), por la RAM (\d+)", 60, marca)
        s = m.serial()[marca:]
        verificar(re.search(rf"\[GPU3D\] presentar \(D432\): fuente destino 320x240\+0\+0 de 320x240 -> pantalla {W}x{H}\+0\+0", s),
                  "modo dibujo: se copia el destino del 3D entero")
        verificar(int(r[0]) > 30 and int(r[1]) == 0, f"modo dibujo: todo directo ({r})")
        time.sleep(2)
        h136_foco(m)

        # 5b. Sin renderizador (superficie de la ventana, como Tzar): solo con SDL_SAGUI_GPU=1. Con el patrón quieto
        # el programa deja de presentar; a los 3 s el compositor recupera la pantalla y trae el último cuadro de la
        # memoria de vídeo a la ventana: se sigue viendo lo mismo.
        out = m.comando("/apps/bin/sdl2presentar -n 20 -m superficie -l 320x180", 200)
        n, fps, dirs, ram = medida(out)
        verificar("modo=superficie" in out and dirs == 0, f"superficie sin la variable: por el camino de siempre ({dirs})")
        m.comando("export SDL_SAGUI_GPU=1", tempo=30)
        marca = lanzar(m, "/apps/bin/sdl2presentar -n 20 -m superficie -l 320x180 -e 12")
        esperar(m, r"\[SDL2PRES\] patron fijo", 60, marca)
        time.sleep(1.2)
        pant = h228.pantalla(m, "h432-superficie")
        mal = patron_en_pantalla(pant, dy, da)
        verificar(not mal and parecido(px(pant, W // 2, dy // 2), NEGRO), f"superficie por la GPU: patrón y bandas en la pantalla ({mal})")
        s = m.serial()[marca:]
        verificar("superficie de la ventana por la GPU (D432): lienzo 320x180" in s, "la SDL dice que la superficie va por la GPU")
        mot = esperar(m, r"pantalla exclusiva: la ventana \d+ la devuelve tras \d+ cuadros \(([^)]*)\)(; ultimo cuadro traido a la ventana)?", 20, marca)
        verificar(mot[0] == "sin cuadros presentados en 3 s" and mot[1], f"quieto 3 s: el compositor la recupera y trae el último cuadro ({mot})")
        time.sleep(1.5)
        pant = h228.pantalla(m, "h432-traido")
        mal = patron_en_pantalla(pant, dy, da)
        verificar(not mal and parecido(px(pant, W // 2, dy // 2), NEGRO), f"lo que pinta ahora el compositor es el cuadro que dejó la GPU ({mal})")
        r = esperar(m, r"\[SDL2PRES\] fin: directos (\d+), por la RAM (\d+)", 60, marca)
        verificar(int(r[0]) >= 20, f"superficie: cuadros directos ({r})")
        time.sleep(2)
        h136_foco(m)
        m.comando("unset SDL_SAGUI_GPU", tempo=30)

        # 6. Lo que no debe presentar directo: en ventana, con la opción apagada y con un driver que no puede.
        out = m.comando("/apps/bin/sdl2presentar -n 20 -v", 200)
        n, fps, dirs, ram = medida(out)
        verificar("completa=0" in out and dirs == 0 and ram == 20, f"en ventana: 0 directos, 20 por la RAM ({dirs}, {ram})")
        m.comando("export SDL_SAGUI_GPU_PRESENTAR=0", tempo=30)
        out = m.comando("/apps/bin/sdl2presentar -n 20", 200)
        n, fps, dirs, ram = medida(out)
        verificar("gpu=1" in out and dirs == 0 and ram == 20, f"SDL_SAGUI_GPU_PRESENTAR=0: la GPU trae el cuadro como antes ({dirs}, {ram})")
        m.comando("unset SDL_SAGUI_GPU_PRESENTAR", tempo=30)
        m.comando("export SDL_SAGUI_GPU=0", tempo=30)
        out = m.comando("/apps/bin/sdl2presentar -n 20", 200)
        n, fps, dirs, ram = medida(out)
        verificar("gpu=0" in out and dirs == 0 and ram == 0, "SDL_SAGUI_GPU=0: por software aunque el programa pida el acelerado")
        m.comando("unset SDL_SAGUI_GPU", tempo=30)
        m.comando("echo 'inicio 1008 1' > /sistema/conf/gpu3d-simulado.conf")
        m.comando("gpu3d apagar", 30)
        out = m.comando("gpu3d encender", 120)
        verificar("gpu3d encender: activo" in out, "el driver se vuelve a encender con el ajuste «no puedo presentar»")
        out = m.comando("gpu3d", 30)
        verificar("presentar en la pantalla (D432): no" in out, "«gpu3d» dice que este driver no presenta")
        out = m.comando("/apps/bin/sdl2presentar -n 20", 200)
        n, fps, dirs, ram = medida(out)
        verificar("gpu=1" in out and dirs == 0 and ram == 20, f"driver sin presentar: como antes de la D432 ({dirs}, {ram})")
        m.comando("rm /sistema/conf/gpu3d-simulado.conf")
        m.comando("gpu3d apagar", 30)
        out = m.comando("gpu3d encender", 120)
        verificar("gpu3d encender: activo" in out, "sin el fichero, todo vuelve a estar bien")

        # 7. «gpu3d apagar» con la pantalla cedida: el compositor la recupera y el programa sigue por software.
        m.comando("sleep 14 && gpu3d apagar &", 30)
        marca = lanzar(m, "/apps/bin/sdl2presentar -n 5 -e 30")
        esperar(m, r"\[SDL2PRES\] patron fijo", 60, marca)
        esperar(m, r"\[GPU3D\] apagado: a mano", 60, marca)
        mot = esperar(m, r"pantalla exclusiva: la ventana \d+ la devuelve tras (\d+) cuadros \(([^)]*)\)", 20, marca)
        verificar(mot[1] == "el 3D se apaga" and int(mot[0]) > 0, f"al apagar el 3D el compositor recupera la pantalla ({mot})")
        esperar(m, r"renderizador por GPU apagado", 20, marca)
        time.sleep(1.5)
        pant = h228.pantalla(m, "h432-apagado")
        mal = patron_en_pantalla(pant, 0, H)
        verificar(not mal, f"apagado el 3D, el programa sigue a pantalla completa por software ({mal})")
        esperar(m, r"\[SDL2PRES\] fin: directos (\d+), por la RAM (\d+)", 80, marca)
        time.sleep(2)
        h136_foco(m)
        out = m.comando("gpu3d", 30)
        verificar("apagado" in out, "el 3D queda apagado a mano")
        sano(m.serial())


def h136_foco(m):
    import h136_opentyrian as h136
    h136.focar_terminal(m)


PARTES = {"unidad": unidad, "fuentes": fuentes, "std": std, "simulado": simulado}


def main():
    os.makedirs(SAIDA, exist_ok=True)
    pedidas = sys.argv[1:] or list(PARTES)
    for p in pedidas:
        if p not in PARTES:
            print("uso: h432_presentar_gpu.py [%s]" % " ".join(PARTES))
            return 2
    for p in pedidas:
        print("== %s" % p)
        PARTES[p]()
    print("h432: BIEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
