#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H436 - AoEN dibujado por la GPU («fase 2», D436) y el HUD de rendimiento.

Partes (por defecto todas menos «bench»; o las pedidas: h436_aoen_gpu.py unidad fuentes std simulado):
  unidad     en el PC: tests/aoen_gpu_unidad.c compila userland/juegos/aoen/gpu.c con la SDL del PC (por software, con
             ASan): lotes (200 sprites en uno o pocos), atlas (colores, sombra, color de jugador, espejo, escala, página
             de paso y desalojos), orden de dibujo, recorte y la capa de la interfaz.
  fuentes    lo que tiene que estar: SDL_RenderGeometry, el recorte y los modos de mezcla en la SDL de SaguiOS, los
             32 huecos y los lotes de 3072 vértices, la opción y la variable de AoEN, la vuelta a la CPU, D436 y los
             documentos; ninguna dependencia nueva de terceros.
  std        QEMU sin GPU 3D: con AOEN_GPU=1 AoEN dice que sigue por la CPU y la partida va normal.
  simulado   QEMU con --gpu3d=simulado y datos SINTÉTICOS (DRS de tools/aoen_drs_sintetico.py y texturas HD de
             tools/aoen_texturas_sintetico.py): la partida se dibuja por la GPU, el cuadro se parece al de la CPU
             (AOEN_COMPARAR: el mismo mundo dibujado por los dos caminos), con terreno HD y con losetas; el HUD de
             rendimiento aparece con Ctrl+F y sus campos no salen vacíos; fps por GPU y por CPU (el driver simulado
             dibuja con la CPU del QEMU: los fps no dicen nada del hardware). Captura H436-aoen-gpu (datos sintéticos).
  bench      (aparte) fps por GPU y por CPU con el driver simulado, más cuadros.
Nunca se usan los datos del juego original.
"""
import os
import re
import subprocess
import sys
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import verificar  # noqa: E402

AOEN = os.path.join(RAIZ, "userland", "juegos", "aoen")


def ler(rel):
    return open(os.path.join(RAIZ, rel), encoding="utf-8").read()


def unidad():
    from qemu import SAIDA
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "aoen_gpu_unidad")
    sdl = subprocess.run(["sdl2-config", "--cflags"], capture_output=True, text=True).stdout.split()
    libs = subprocess.run(["sdl2-config", "--libs"], capture_output=True, text=True).stdout.split()
    r = subprocess.run(["gcc", "-std=gnu99", "-O1", "-g", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined",
                        "-Wall", "-Wno-unused-function", "-Wno-format-truncation", "-I", AOEN] + sdl +
                       ["-o", exe, os.path.join(RAIZ, "tests", "aoen_gpu_unidad.c"), os.path.join(AOEN, "gpu.c")] + libs + ["-lm"],
                       capture_output=True, text=True)
    verificar(r.returncode == 0, "aoen_gpu_unidad.c compila con gpu.c (ASan/UBSan): " + r.stderr[-800:])
    r = subprocess.run([exe], capture_output=True, text=True, timeout=300, env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))
    print("    " + r.stdout.strip().replace("\n", "\n    "))
    verificar(r.returncode == 0 and re.search(r"aoen_gpu_unidad: \d+ pruebas, 0 mal", r.stdout),
              "la prueba de unidad del dibujo por GPU sale bien:\n" + (r.stdout + r.stderr)[-2000:])
    verificar("ERROR: AddressSanitizer" not in r.stderr and "runtime error" not in r.stderr, "sin errores de ASan ni UBSan")


def fuentes():
    sdl = ler("userland/libsdl2/sdl2_sagui.c")
    g = ler("userland/libsdl2/sdl2gpu.h")
    h = ler("userland/libsdl2/SDL.h")
    for f in ("SDL_RenderGeometry", "SDL_RenderSetClipRect", "SDL_RenderGetClipRect", "SDL_RenderIsClipEnabled",
              "SDL_ComposeCustomBlendMode", "SDL_GetTextureColorMod", "SDL_GetTextureAlphaMod"):
        verificar(f in h and f in sdl, "la SDL de SaguiOS declara y tiene %s" % f)
    verificar("#define SDLG_MAXV 3072" in g and "#define SDLG_VAGAS 32" in g, "lotes de 3072 vértices y 32 huecos de textura")
    verificar("faltan huecos de textura" in sdl, "si faltan huecos de textura se dice en el registro")
    verificar("SDLG_M_SOMA" in g and "SDLG_M_PREMULT" in g and "G3D_TEX_REPETE_S" in g, "ADD, alfa premultiplicado y repetir texturas en la GPU")
    a = ler("userland/juegos/aoen/gpu.c")
    m = ler("userland/juegos/aoen/main.c")
    o = ler("userland/juegos/aoen/ui_menu.c")
    verificar('"AOEN_GPU"' in m and '"dibujo_gpu"' in m, "AoEN: AOEN_GPU=1 o dibujo_gpu en aoen.cfg")
    verificar("Dibujo por GPU" in o and "HUD de rendimiento (Ctrl+F)" in o, "Opciones > Gráficos: «Dibujo por GPU» y el HUD")
    verificar("gpu_apagar" in a and "el renderizador dejó la GPU" in a and "sin 3D" in a, "vuelta automática a la CPU con aviso")
    for f in ("gpu.c", "gpu.h", "rendimiento.c"):
        t = ler("userland/juegos/aoen/" + f)
        verificar(t.splitlines()[0].startswith("/* SPDX-License-Identifier: GPL-3.0-or-later"), "%s lleva SPDX GPL-3.0-or-later" % f)
    r = ler("userland/juegos/aoen/rendimiento.c")
    for i in ("INFO_NUCLEOS", "INFO_MEMORIA", "INFO_PROCESSOS", "INFO_SENSORES" if "INFO_SENSORES" in r else "sensores_ler", "INFO_GPU3D", "INFO_GPU_ENERGIA"):
        verificar(i in r, "el HUD usa %s (lo mismo que htop, sensores, infopc y el Administrador de tareas)" % i)
    d = ler("docs/decisoes.md")
    verificar("## D436" in d, "docs/decisoes.md tiene D436")
    verificar("D436" in ler("docs/aoen.md") and "D436" in ler("docs/sdl-gpu.md"), "docs/aoen.md y docs/sdl-gpu.md hablan de D436")
    verificar("SPDX-License-Identifier" in ler("tests/h436_aoen_gpu.py") and "SPDX-License-Identifier" in ler("tests/aoen_gpu_unidad.c"),
              "las pruebas llevan SPDX")
    for f in ("gpu.c", "gpu.h", "rendimiento.c"):
        t = ler("userland/juegos/aoen/" + f)
        verificar(not re.search(r"[Ͱ-ϿЀ-ӿ　-鿿]", t), "%s sin otros alfabetos" % f)


def jogos_sinteticos():
    jogos = tempfile.mkdtemp(prefix="aoen-gpu-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")],
                   check=True, stdout=subprocess.DEVNULL)
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_texturas_sintetico.py"), os.path.join(jogos, "AOE", "TEX")],
                   check=True, stdout=subprocess.DEVNULL)
    return jogos


def arrancar(m):
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    esperar(m, r"sagui:/\$", 200)
    time.sleep(4)
    h136.focar_terminal(m)


def correr(m, orden, patron, tempo=400):
    """Lanza AoEN con la orden y espera el patrón y el final (vuelve el intérprete). Devuelve (grupos, registro)."""
    from h60_barra_tareas import esperar
    marca = len(m.serial())
    m.digitar(orden, 0.03)
    m.tecla("ret")
    g = esperar(m, patron, tempo, desde=marca) if patron else None
    esperar(m, r"sagui:/\$", tempo, desde=marca)
    return g, m.serial()[marca:]


BASE = ("AOEN_FULL=1 AOEN_SCENE=game AOEN_PLAYERS=4 AOEN_SIZE=2 AOEN_SEED=7 AOEN_FFWD=60 AOEN_PASO_FIJO=1 "
        "AOEN_TIEMPO_ANIM=3 AOEN_NOSOUND=1 ")


def std():
    from qemu import Maquina, imagem_de_teste, APPS_RAIZ
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta «std»")
        return
    jogos = jogos_sinteticos()
    img = imagem_de_teste("h436-std", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h436-std", "ide", imagem=img, memoria="256M") as m:
        arrancar(m)
        g, s = correr(m, BASE + "AOEN_GPU=1 AOEN_SHOT=/tmp/a.bmp AOEN_FRAMES=40 /apps/bin/aoen", r"\[AOEN\] captura guardada en el cuadro (\d+)")
        verificar("dibujo por GPU: el renderizador no lleva la GPU (sin 3D); sigue por la CPU" in s,
                  "sin GPU 3D, AoEN lo dice y sigue por la CPU")
        verificar("dibujo por GPU (D436): renderizador" not in s, "no intenta dibujar por la GPU")
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones ni pánicos")


def simulado(bench_cuadros=0):
    from qemu import Maquina, imagem_de_teste, APPS_RAIZ
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta «simulado»")
        return
    jogos = jogos_sinteticos()
    img = imagem_de_teste("h436-sim", ["--vesa", "--gpu3d=simulado", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h436-sim", "ide", imagem=img, memoria="512M") as m:
        arrancar(m)
        gpu = "AOEN_GPU=1 AOEN_COMPARAR=1 "
        for nombre, extra, lim in (("terreno HD", "AOEN_TEX=/jogos/AOE/TEX ", 14.0), ("losetas", "AOEN_CLASSIC=1 AOEN_EFECTOS=0 ", 10.0)):
            g, s = correr(m, BASE + gpu + extra + "AOEN_SHOT=/tmp/a.bmp AOEN_FRAMES=30 /apps/bin/aoen",
                          r"\[AOEN\] comparar CPU/GPU: (\d+)x(\d+), diferencia media ([\d.]+) por canal, p\S+xeles lejos ([\d.]+)%, negros GPU ([\d.]+)% CPU ([\d.]+)%")
            verificar("[AOEN] dibujo por GPU (D436): renderizador saguios-gpu" in s, "%s: AoEN dibuja por la GPU (driver simulado)" % nombre)
            verificar(re.search(r"\[SDL2\] presentar por la GPU en la pantalla \(D432\): lienzo 1024x768", s), "%s: se presenta por la GPU" % nombre)
            verificar("dibujo por GPU apagado" not in s and "renderizador por GPU apagado" not in s, "%s: la GPU no se cae" % nombre)
            med, lejos, ng, nc = float(g[2]), float(g[3]), float(g[4]), float(g[5])
            print(f"    {nombre}: {g[0]}x{g[1]}, diferencia media {med} por canal, {lejos} % lejos, negros {ng} % / {nc} %")
            verificar(med <= lim and lejos <= 12.0, "%s: el cuadro de la GPU se parece al de la CPU (media %.2f <= %.1f, %.2f %% lejos)" % (nombre, med, lim, lejos))
            verificar(abs(ng - nc) <= 5.0, "%s: lo negro (sin explorar, fuera del mapa) coincide (%.2f %% / %.2f %%)" % (nombre, ng, nc))
            if nombre == "terreno HD":
                m.captura("H436-aoen-gpu")
        # el HUD: con Ctrl+F aparece y sus campos no salen vacíos (la partida acaba sola en el cuadro 400)
        from h60_barra_tareas import esperar
        marca = len(m.serial())
        m.digitar(BASE.replace("AOEN_PASO_FIJO=1 ", "") + "AOEN_GPU=1 AOEN_TEX=/jogos/AOE/TEX AOEN_SHOT=/tmp/h.bmp AOEN_FRAMES=400 /apps/bin/aoen", 0.03)
        m.tecla("ret")
        esperar(m, r"\[AOEN\] estado t=", 300, desde=marca)
        m.tecla("ctrl-f")
        esperar(m, r"\[AOEN\] HUD de rendimiento encendido", 60, desde=marca)
        esperar(m, r"\[AOEN\] HUD: Temp", 120, desde=marca)
        esperar(m, r"\[AOEN\] HUD: FPS", 120, desde=marca)
        time.sleep(1.0)
        m.captura("H436-hud")
        esperar(m, r"sagui:/\$", 900, desde=marca)
        s = m.serial()[marca:]
        hud = dict((k, v) for k, v in re.findall(r"\[AOEN\] HUD: (\w+)\.? ?(.*)", s))
        print("    HUD: " + "; ".join("%s %s" % (k, v.strip()) for k, v in hud.items()))
        for campo in ("FPS", "CPU", "RAM", "GPU", "VRAM", "Dibujo", "Temp"):
            verificar(campo in hud and hud[campo].strip(), "HUD: el campo %s está y no sale vacío" % campo)
        verificar(re.search(r"\[AOEN\] HUD: FPS \d+ \(media [\d.]+\)  [\d.]+ ms por cuadro", s), "HUD: fps, media y tiempo de cuadro")
        verificar(re.search(r"\[AOEN\] HUD: RAM \d+ MB usada, \d+ libre; AoEN \d+ MB", s), "HUD: la RAM del sistema y la de AoEN")
        verificar(re.search(r"\[AOEN\] HUD: Dibujo por GPU; presentaci\S+n por la GPU \(D432\)", s), "HUD: dibujo y presentación por la GPU")
        verificar(re.search(r"\[AOEN\] HUD: GPU simulado: \d+ cuadros/s", s), "HUD: el uso de la GPU (driver simulado)")
        # fps (sin más: el driver simulado dibuja con la CPU del QEMU)
        n = bench_cuadros or 60
        res = {}
        for modo, extra in (("GPU", "AOEN_GPU=1 "), ("CPU", "")):
            g, s = correr(m, "AOEN_FULL=1 AOEN_SCENE=game AOEN_PLAYERS=4 AOEN_SIZE=2 AOEN_SEED=7 AOEN_FFWD=60 AOEN_NOSOUND=1 "
                          "AOEN_TEX=/jogos/AOE/TEX AOEN_PERFIL=1 AOEN_BENCH=%d %s/apps/bin/aoen" % (n, extra),
                          r"\[AOEN\] bench 1024x768 cuadros=(\d+) ms=(\d+) fps=([\d.]+)", 900)
            res[modo] = float(g[2])
            gl = re.search(r"\[AOEN\] GPU por cuadro: (\d+) tri\S+ngulos en (\d+) lotes, (\d+) KB subidos", s)
            print(f"    fps por {modo} (QEMU, driver simulado): {g[2]}" + (f"; {gl.group(1)} triángulos en {gl.group(2)} lotes, {gl.group(3)} KB subidos por cuadro" if gl else ""))
            if modo == "GPU":
                verificar(gl and int(gl.group(2)) <= 40, "por GPU: pocos lotes por cuadro (%s)" % (gl.group(2) if gl else "?"))
        verificar(res["GPU"] > 0 and res["CPU"] > 0, "AoEN corre por los dos caminos")
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones ni pánicos")


def bench():
    simulado(int(os.environ.get("H436_BENCH", "300")))


PARTES = {"unidad": unidad, "fuentes": fuentes, "std": std, "simulado": simulado}


def main():
    pedidas = sys.argv[1:] or list(PARTES)
    for p in pedidas:
        if p not in PARTES and p != "bench":
            print("uso: h436_aoen_gpu.py [%s bench]" % " ".join(PARTES))
            return 2
    for p in pedidas:
        print("== %s" % p)
        (bench if p == "bench" else PARTES[p])()
    print("h436: BIEN")
    return 0


if __name__ == "__main__":
    sys.exit(main())
