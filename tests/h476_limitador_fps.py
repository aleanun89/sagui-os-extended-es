#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H476 (D476) - limitador de FPS: 30, 60 o ilimitado (v8.5.6.1).

Una espera que DUERME (SDL_Delay: no gasta CPU y sigue bombeando el audio) tras presentar el cuadro. Tres sitios:
la SDL de SaguiOS (SDL_SAGUI_FPS_MAX, en SDL_RenderPresent y SDL_GL_SwapWindow; la SDL 1.2, por variable, en SDL_Flip),
y los juegos con ajuste propio: AoEN (Opciones > Gráficos, clave fps_max), Brumacanto (F6, clave fps de ajustes.txt) y
Capibara (F5 o --fps=, fichero ~/.config/capibara/ajustes.cfg).

Partes (por defecto todas; o las que se pidan: h476_limitador_fps.py unidad fuentes sdl aoen brumacanto capibara):
  unidad      en el PC: tests/ajustes_test.c de Brumacanto (clave fps) y Capibara con --bench y --fps=30/60/ilimitado.
  fuentes     el limitador en la SDL 2 (RenderPresent y SwapWindow) y en la SDL 1.2, los tres juegos, D476, ROADMAP y manuales.
  sdl         en SaguiOS, /apps/bin/sdl2fps cuenta los cuadros presentados: 30 por la pista, 60 y 30 por la variable de
              entorno, y sin tope; con tope baja el uso de CPU (la espera duerme) y sin él el cuadro corre libre.
  aoen        la opción de Opciones > Gráficos (dos clics: 60 -> 30 -> ilimitado) se aplica y se guarda, vale al arrancar
              otra vez y AOEN_FPS_MAX manda; el ritmo medido en una escena. Captura H476-1.
  brumacanto  F6 cambia 60 -> 30 -> ilimitado y lo guarda en ajustes.txt; la vez siguiente arranca con él.
  capibara    --fps=30 mide ~30 cuadros por segundo con --bench; F5 cambia el tope y lo guarda; la vez siguiente arranca con él.
"""
import os
import re
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, FalhaTeste, RAIZ, APPS_RAIZ, SAIDA  # noqa: E402


def leer(ruta):
    return open(os.path.join(RAIZ, ruta), encoding="utf-8").read()


def unidad():
    D = os.path.join(RAIZ, "userland", "terceros", "brumacanto", "fuentes", "tests")
    os.makedirs(os.path.join(SAIDA, "h476"), exist_ok=True)
    exe = os.path.join(SAIDA, "h476", "ajustes_test")
    subprocess.run(["gcc", "-std=c11", "-O1", "-Wall", "-Wextra", "-o", exe, os.path.join(D, "ajustes_test.c")], check=True)
    r = subprocess.run([exe], capture_output=True, text=True)
    verificar(r.returncode == 0, "ajustes_test de Brumacanto (clave fps) sin fallos: " + r.stdout[-300:])
    import h452_capibara as h452
    h452.compilar_pc()
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", HOME=tempfile.mkdtemp(prefix="capi-home-"))
    res = {}
    for fps in ("30", "60", "ilimitado"):
        r = subprocess.run([os.path.join(h452.D, "build", "capibara"), os.path.join(h452.D, "ejemplos", "plataformas"), "--bench", "90",
                            "--fps=" + fps], capture_output=True, text=True, env=env, timeout=300)
        m = re.search(r"CAPI_BENCH cuadros (\d+) tiempo_ms (\d+) fps_x10 (\d+)", r.stdout)
        verificar(r.returncode == 0 and m is not None, "capibara --bench --fps=%s termina: %s" % (fps, r.stdout[-200:] + r.stderr[-200:]))
        res[fps] = int(m.group(3)) / 10.0
    print("    Capibara en el PC (bench 90 cuadros): 30 -> %.1f, 60 -> %.1f, ilimitado -> %.1f fps" % (res["30"], res["60"], res["ilimitado"]))
    verificar(27 <= res["30"] <= 31, "--fps=30 da unos 30 cuadros por segundo (%.1f)" % res["30"])
    verificar(54 <= res["60"] <= 62, "--fps=60 da unos 60 cuadros por segundo (%.1f)" % res["60"])
    verificar(res["ilimitado"] > 150, "--fps=ilimitado no frena el bucle (%.1f)" % res["ilimitado"])


def fuentes():
    s = leer("userland/libsdl2/sdl2_sagui.c")
    verificar("void sagui_sdl_fps_limitar(void)" in s and "SDL_HINT_SAGUI_FPS_MAX" in s and "SDL_Delay((Uint32)falta)" in s,
              "la SDL 2 tiene el limitador y duerme con SDL_Delay (que bombea el audio)")
    verificar("presentar_sin_limite(r);" in s and "sagui_sdl_fps_limitar();" in s, "SDL_RenderPresent llama al limitador")
    verificar("sagui_sdl_fps_limitar();" in leer("userland/libsdl2/sdl2_gl.c"), "SDL_GL_SwapWindow llama al limitador")
    verificar("SDL_HINT_SAGUI_FPS_MAX" in leer("userland/libsdl2/SDL.h"), "la pista está en SDL.h")
    verificar("SDL_SAGUI_FPS_MAX" in leer("userland/libsdl12/sdl_sagui.c") and "fps_limitar();" in leer("userland/libsdl12/sdl_sagui.c"),
              "la SDL 1.2 limita en SDL_Flip (por variable)")
    verificar("fps_max" in leer("userland/juegos/aoen/ui_menu.c") and "Límite de FPS" in leer("userland/juegos/aoen/ui_menu.c"),
              "AoEN: selector en Opciones > Gráficos")
    verificar("SDLK_F6" in leer("userland/terceros/brumacanto/fuentes/src/main.c"), "Brumacanto: F6")
    verificar("SDLK_F5" in leer("userland/capibara/motor/plataforma_sdl.c") and "--fps=" in leer("userland/capibara/motor/plataforma_sdl.c"),
              "Capibara: F5 y --fps=")
    d = leer("docs/decisoes.md")
    verificar("## D476." in d and "tests/h476_limitador_fps.py" in d, "decisión D476 con su prueba")
    verificar("SDL_SAGUI_FPS_MAX" in leer("docs/ROADMAP.md"), "ROADMAP al día")
    for f in ("docs/aoen.md", "docs/brumacanto.md", "docs/capibara.md"):
        verificar("D476" in leer(f), "manual: " + f)
    for ruta in ("userland/libsdl2/sdl2_sagui.c", "userland/libsdl2/sdl2fpsprova.c", "userland/juegos/aoen/ui_menu.c", "tests/h476_limitador_fps.py",
                 "userland/capibara/motor/plataforma_sdl.c", "userland/terceros/brumacanto/fuentes/src/main.c"):
        verificar(not re.search("[\u0370-\u04ff\u3000-\u9fff]", leer(ruta)), "sin otros alfabetos en " + os.path.basename(ruta))


def ejecutar_fps(m, entorno, argumentos, tiempo=200):
    """Lanza sdl2fps en el terminal del escritorio (necesita ventana) y lee su línea del registro."""
    from h60_barra_tareas import esperar
    marca = len(m.serial())
    m.digitar("%s/apps/bin/sdl2fps %s" % (entorno, argumentos), 0.03)
    m.tecla("ret")
    g = esperar(m, r"SDLFPS cuadros=(\d+) ms=(\d+) fps10=(\d+) cpu=(-?\d+)", tiempo, desde=marca)
    esperar(m, r"sagui:/\$", 60, desde=marca)
    return [int(x) for x in g]


def sdl():
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "sdl2fps")):
        raise FalhaTeste("la imagen no lleva sdl2fps (make)")
    img = imagem_de_teste("h476-sdl", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h476-sdl", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        h136.focar_terminal(m)
        libre = ejecutar_fps(m, "", "4")
        t30 = ejecutar_fps(m, "", "4 30")                                   # por la pista
        e30 = ejecutar_fps(m, "SDL_SAGUI_FPS_MAX=30 ", "4")                 # por la variable de entorno
        e60 = ejecutar_fps(m, "SDL_SAGUI_FPS_MAX=60 ", "4")
        cero = ejecutar_fps(m, "SDL_SAGUI_FPS_MAX=30 ", "3 0")              # la pista (0 = ilimitado) manda sobre la variable
        for nombre, v in (("sin tope", libre), ("pista 30", t30), ("variable 30", e30), ("variable 60", e60), ("pista 0 con variable 30", cero)):
            print("    %-24s %4d cuadros en %5d ms: %6.1f fps, CPU %d %%" % (nombre, v[0], v[1], v[2] / 10.0, v[3]))
        verificar(260 <= t30[2] <= 330, "la pista SDL_SAGUI_FPS_MAX=30 da unos 30 fps (%.1f)" % (t30[2] / 10.0))
        verificar(260 <= e30[2] <= 330, "la variable SDL_SAGUI_FPS_MAX=30 da unos 30 fps (%.1f)" % (e30[2] / 10.0))
        verificar(520 <= e60[2] <= 660, "la variable SDL_SAGUI_FPS_MAX=60 da unos 60 fps (%.1f)" % (e60[2] / 10.0))
        verificar(libre[2] > 700, "sin tope el cuadro corre libre (%.1f fps)" % (libre[2] / 10.0))
        verificar(cero[2] > 700, "la pista 0 vale ilimitado aunque la variable diga 30 (%.1f fps)" % (cero[2] / 10.0))
        verificar(0 <= e30[3] <= 25, "con 30 fps la espera duerme: CPU %d %% (bajo)" % e30[3])
        verificar(libre[3] >= 60, "sin tope el bucle gasta la CPU (%d %%)" % libre[3])
        verificar("EXCECAO" not in m.serial() and "PANICO" not in m.serial(), "sin excepciones del kernel")


def jogos_sinteticos():
    jogos = tempfile.mkdtemp(prefix="aoen-fps-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")],
                   check=True, stdout=subprocess.DEVNULL)
    return jogos


def aoen():
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        raise FalhaTeste("la imagen no lleva aoen (make)")
    jogos = jogos_sinteticos()
    img = imagem_de_teste("h476-aoen", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h476-aoen", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        h136.focar_terminal(m)

        def correr(extra, frames, patron, captura=None):
            marca = len(m.serial())
            m.digitar("AOEN_NOSOUND=1 AOEN_SHOT=/tmp/aoen.bmp AOEN_FRAMES=%d %s /apps/bin/aoen" % (frames, extra), 0.03)
            m.tecla("ret")
            g = esperar(m, patron, 400, desde=marca) if patron else None
            if captura:
                time.sleep(1.0)
                m.captura(captura)
            esperar(m, r"sagui:/\$", 400, desde=marca)
            return g, m.serial()[marca:]

        # 1) de serie 60; las opciones: dos clics en el selector (60 -> 30 -> ilimitado), guardado en aoen.cfg
        g, s = correr("AOEN_SCENE=options AOEN_PAG=0 AOEN_PRUEBA=fps", 40, r"\[AOEN\] prueba fps: fps_max=(\d+) guardado=(-?\d+)[\s\S]*?\[AOEN\] prueba fps: fps_max=(\d+) guardado=(-?\d+)", "H476-1-aoen-opciones-fps")
        verificar(re.search(r"\[AOEN\] l\S+mite de FPS: 60", s) is not None, "de serie el límite es de 60 FPS")
        verificar(g[0] == "30" and g[1] == "30", "primer clic: 30 FPS y guardado (%s/%s)" % (g[0], g[1]))
        verificar(g[2] == "0" and g[3] == "0", "segundo clic: ilimitado (0) y guardado (%s/%s)" % (g[2], g[3]))
        # 2) la vez siguiente arranca con lo guardado (ilimitado), y AOEN_FPS_MAX manda
        g, s = correr("AOEN_SCENE=options AOEN_PAG=0", 40, r"\[AOEN\] l\S+mite de FPS: (\d+)")
        verificar(g == "0", "la vez siguiente arranca con el límite guardado (ilimitado, %s)" % g)
        g, s = correr("AOEN_FPS_MAX=30 AOEN_SCENE=options AOEN_PAG=0", 40, r"\[AOEN\] l\S+mite de FPS: (\d+)")
        verificar(g == "30", "AOEN_FPS_MAX manda sobre lo guardado (%s)" % g)
        # 3) el ritmo en una escena barata de cuadros, con cada límite
        ritmo = {}
        for lim in (30, 60, 0):
            g, s = correr("AOEN_FPS_MAX=%d AOEN_SCENE=options AOEN_PAG=0" % lim, 150, r"\[AOEN\] ritmo: (\d+) cuadros en (\d+) ms")
            ritmo[lim] = int(g[0]) * 1000.0 / max(1, int(g[1]))
        print("    AoEN, ritmo medido en Opciones: 30 -> %.1f, 60 -> %.1f, ilimitado -> %.1f fps" % (ritmo[30], ritmo[60], ritmo[0]))
        verificar(27 <= ritmo[30] <= 32, "con 30 FPS, unos 30 cuadros por segundo (%.1f)" % ritmo[30])
        verificar(54 <= ritmo[60] <= 65, "con 60 FPS, hasta 60 (%.1f)" % ritmo[60])
        verificar(ritmo[0] > ritmo[30] * 1.3 or ritmo[0] > 66, "ilimitado no frena (%.1f)" % ritmo[0])
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones del kernel")


def cerrar(m, esperar, marca):
    """Cierra la ventana del juego (Alt+F4: SDL_QUIT): no depende del menú de pausa, que en QEMU es lento y a veces se queda al guardar."""
    m.tecla("alt-f4", 1.0)
    esperar(m, r"processo \d+ \(brumacanto\) terminou com codigo (\d+)", 400, desde=marca)
    esperar(m, r"sagui:/\$", 60, desde=marca)
    time.sleep(2)


def brumacanto():
    from h60_barra_tareas import esperar
    import h444_brumacanto as h444
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "brumacanto")):
        raise FalhaTeste("la imagen no lleva brumacanto (make)")
    img = imagem_de_teste("h476-bruma", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h476-bruma", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        marca, _ = h444.lanzar(m, "--seed Prueba --size 160x160x48", r"BRUMA_MUNDO (nuevo|cargado) ")
        verificar("BRUMA_FPS 60" in m.serial()[marca:], "de serie el límite es de 60 FPS")
        time.sleep(6)
        m.tecla("f6", 1.5)
        esperar(m, r"BRUMA_FPS 30 \(F6, guardado\)", 60, desde=marca)
        m.tecla("f6", 1.5)
        esperar(m, r"BRUMA_FPS 0 \(F6, guardado\)", 60, desde=marca)
        verificar(True, "F6 cambia el límite: 60 -> 30 -> ilimitado, y lo guarda")
        cerrar(m, esperar, marca)
        marca, _ = h444.lanzar(m, "--seed Prueba --size 160x160x48", r"BRUMA_MUNDO (nuevo|cargado) ")
        verificar("BRUMA_FPS 0\n" in m.serial()[marca:].replace("\r", ""), "la vez siguiente arranca con el límite guardado (ilimitado)")
        time.sleep(3)
        m.tecla("f6", 1.5)
        esperar(m, r"BRUMA_FPS 60 \(F6, guardado\)", 60, desde=marca)
        verificar(True, "F6 desde ilimitado vuelve a 60")
        cerrar(m, esperar, marca)
        marca, _ = h444.lanzar(m, "--seed Prueba --size 160x160x48", r"BRUMA_MUNDO (nuevo|cargado) ")
        s = m.serial()[marca:]
        verificar("BRUMA_VISTA_AJUSTE" in s and "BRUMA_FPS 60" in s, "el ajuste de la vista sigue y el límite guardado vale")
        time.sleep(3)
        cerrar(m, esperar, marca)
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones del kernel")


def capibara():
    from h60_barra_tareas import esperar
    import h452_capibara as h452
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "capibara")):
        raise FalhaTeste("la imagen no lleva capibara (make)")
    img = imagem_de_teste("h476-capi", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h476-capi", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        marca, r = h452.lanzar(m, "/apps/bin/capibara /apps/capibara/plataformas --fps=30 --bench 90 --gpu=no",
                               r"CAPI_BENCH cuadros (\d+) tiempo_ms (\d+) fps_x10 (\d+)", 600)
        f30 = int(r[2]) / 10.0
        esperar(m, r"sagui:/\$", 60, desde=marca)
        marca, r = h452.lanzar(m, "/apps/bin/capibara /apps/capibara/plataformas --bench 150 --gpu=no",
                               r"CAPI_BENCH cuadros (\d+) tiempo_ms (\d+) fps_x10 (\d+)", 600)
        libre = int(r[2]) / 10.0
        esperar(m, r"sagui:/\$", 60, desde=marca)
        print("    Capibara en SaguiOS (software): --fps=30 -> %.1f fps; bench sin tope -> %.1f fps" % (f30, libre))
        verificar(25 <= f30 <= 31, "--fps=30 da unos 30 cuadros por segundo (%.1f)" % f30)
        verificar(libre > f30 * 1.5, "sin tope el bench corre libre (%.1f)" % libre)
        # F5: 60 -> 30 -> ilimitado, guardado
        marca, _ = h452.lanzar(m, "/apps/bin/capibara /apps/capibara/plataformas --gpu=no", r"CAPI_ESCENA escenas/nivel1.esc nodos=(\d+)")
        time.sleep(3)
        m.tecla("f5", 1.0)
        esperar(m, r"CAPI_FPS_MAX 30 \(F5, guardado\)", 60, desde=marca)
        m.tecla("f5", 1.0)
        esperar(m, r"CAPI_FPS_MAX 0 \(F5, guardado\)", 60, desde=marca)
        verificar(True, "F5 cambia el tope: 60 -> 30 -> ilimitado, y lo guarda")
        m.tecla("esc")
        esperar(m, r"processo \d+ \(capibara\) terminou com codigo 0", 120, desde=marca)
        esperar(m, r"sagui:/\$", 60, desde=marca)
        # la vez siguiente arranca con el guardado: ilimitado, así que el bench corre libre sin --fps
        marca, r = h452.lanzar(m, "CAPI_FPS=30 /apps/bin/capibara /apps/capibara/plataformas --bench 90 --gpu=no",
                               r"CAPI_BENCH cuadros (\d+) tiempo_ms (\d+) fps_x10 (\d+)", 600)
        verificar(25 <= int(r[2]) / 10.0 <= 31, "CAPI_FPS=30 manda sobre lo guardado (%.1f fps)" % (int(r[2]) / 10.0))
        esperar(m, r"sagui:/\$", 60, desde=marca)
        marca, _ = h452.lanzar(m, "/apps/bin/capibara /apps/capibara/plataformas --gpu=no --hud", r"CAPI_ESCENA escenas/nivel1.esc nodos=(\d+)")
        time.sleep(3)
        m.tecla("f5", 1.0)
        esperar(m, r"CAPI_FPS_MAX 60 \(F5, guardado\)", 60, desde=marca)
        verificar(True, "la vez siguiente arrancó con el tope guardado (ilimitado) y F5 lo pasa a 60")
        m.tecla("esc")
        esperar(m, r"processo \d+ \(capibara\) terminou com codigo 0", 120, desde=marca)
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones del kernel")


def main():
    pedidas = sys.argv[1:] or ["unidad", "fuentes", "sdl", "aoen", "brumacanto", "capibara"]
    for p in pedidas:
        print("==", p)
        globals()[p]()
    print("H476 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
