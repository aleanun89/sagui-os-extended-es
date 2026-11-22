#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H452 (D452) - Capibara: motor de juegos 2D con guiones en Lua.

  nucleo     en el PC: userland/capibara/pruebas/prueba_nucleo.c (formato de texto, escenas e instancias, señales, física,
             guiones, entrada por acciones, mapas, animación, partículas, mezclador, paquetes .capi y determinismo), sin SDL.
  ejemplos   en el PC con la SDL «dummy»: los dos ejemplos corren cuadros fijos y de forma determinista (mismo hash del
             último cuadro dos veces y desde el paquete .capi), con entrada guionada: eventos del juego (saltos, monedas,
             enemigos), audio grabado no vacío, sin errores de Lua.
  fuentes    MIT en cada fuente del motor, créditos y licencias CC0 de los ejemplos, fila en docs/terceros.md, D452, ROADMAP,
             entrada en Inicio > Juegos, ayuda y documentación, sin otros alfabetos.
  saguios    QEMU por software: los dos ejemplos arrancan, se juegan con entrada guionada y con el teclado real, el HUD
             (F3) aparece, el paquete .capi se crea y se abre, capturas H452-1 y H452-2, fps medidos.
  audio      QEMU con tarjeta HDA: el hilo de audio propio escribe la música y los efectos en la tarjeta (el .wav no está en silencio).
  gpu        QEMU con --gpu3d=simulado: con --gpu=si el renderizador es «saguios-gpu», la imagen se parece a la de software
             (captura H452-3) y se miden los fps.

Uso: h452_capibara.py [nucleo] [ejemplos] [fuentes] [saguios] [audio] [gpu]   (sin nada: todo)
"""
import os
import re
import subprocess
import sys
import time
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, FalhaTeste, RAIZ, APPS_RAIZ, SAIDA  # noqa: E402

D = os.path.join(RAIZ, "userland", "capibara")
PLAT = os.path.join(D, "ejemplos", "plataformas")
DISP = os.path.join(D, "ejemplos", "disparos")
ENTRADA_PLAT = "5:derecha+;60:saltar+;70:saltar-"
ENTRADA_DISP = "3:disparar+"


def compilar_pc():
    r = subprocess.run(["make", "-C", D, "-j4", "build/capibara", "build/prueba_nucleo"], capture_output=True, text=True)
    if r.returncode != 0:
        raise FalhaTeste("no compila en el PC (¿SDL2, SDL2_image y las fuentes de Lua? tools/lua_fuentes.sh): " + r.stderr[-600:])


def nucleo():
    compilar_pc()
    r = subprocess.run([os.path.join(D, "build", "prueba_nucleo")], capture_output=True, text=True)
    print("   ", r.stdout.strip().splitlines()[-1])
    verificar(r.returncode == 0 and "0 fallos" in r.stdout, "prueba del núcleo sin fallos: " + r.stdout[-500:])


def correr_pc(proyecto, cuadros, entrada, extra=()):
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", CAPI_ENTRADA=entrada)
    r = subprocess.run([os.path.join(D, "build", "capibara"), proyecto, "--cuadros", str(cuadros)] + list(extra), capture_output=True, text=True, env=env, timeout=300)
    verificar(r.returncode == 0, "capibara termina bien: " + r.stdout[-400:] + r.stderr[-300:])
    return r.stdout


def ejemplos():
    compilar_pc()
    os.makedirs(os.path.join(SAIDA, "h452"), exist_ok=True)
    wav = os.path.join(SAIDA, "h452", "plat.wav")
    a = correr_pc(PLAT, 150, ENTRADA_PLAT, ["--grabar-audio", wav])
    b = correr_pc(PLAT, 150, ENTRADA_PLAT)
    ha, hb = re.search(r"CAPI_HASH (\w+)", a).group(1), re.search(r"CAPI_HASH (\w+)", b).group(1)
    verificar(ha == hb, "plataformas: el último cuadro es idéntico en dos ejecuciones (%s)" % ha)
    verificar("CAPI_JUEGO salto" in a and "CAPI_JUEGO moneda 1/24" in a, "plataformas: salta y recoge monedas con entrada guionada")
    verificar("error en" not in a, "plataformas: sin errores de Lua")
    w = wave.open(wav)
    datos = w.readframes(w.getnframes())
    verificar(w.getframerate() == 44100 and len(datos) > 4 * 44100 * 2 and any(datos), "audio grabado: no vacío (%d bytes)" % len(datos))
    # el mismo juego desde un paquete .capi
    pq = os.path.join(SAIDA, "h452", "plat.capi")
    r = subprocess.run([os.path.join(D, "build", "capibara"), "--empaquetar", PLAT, pq], capture_output=True, text=True)
    verificar(r.returncode == 0 and "ficheros empaquetados" in r.stdout, "empaquetar: " + r.stdout)
    c = correr_pc(pq, 150, ENTRADA_PLAT)
    hc = re.search(r"CAPI_HASH (\w+)", c).group(1)
    verificar(hc == ha, "el paquete .capi da el mismo cuadro que la carpeta (%s)" % hc)
    # disparos
    d1 = correr_pc(DISP, 400, ENTRADA_DISP)
    d2 = correr_pc(DISP, 400, ENTRADA_DISP)
    verificar(re.search(r"CAPI_HASH (\w+)", d1).group(1) == re.search(r"CAPI_HASH (\w+)", d2).group(1), "disparos: determinista")
    verificar(len(re.findall(r"CAPI_JUEGO enemigo_destruido", d1)) >= 3 and "CAPI_JUEGO herido" in d1 and "error en" not in d1, "disparos: derriba enemigos y recibe daño sin errores")
    # una escena con un error de guion no tumba el motor: la prueba del núcleo ya lo cubre; aquí, una escena inexistente
    r = subprocess.run([os.path.join(D, "build", "capibara"), os.path.join(SAIDA, "h452", "no_existe")], capture_output=True, text=True, timeout=60,
                       env=dict(os.environ, SDL_VIDEODRIVER="dummy"))
    verificar(r.returncode != 0 and "capibara.cfg" in r.stdout, "un proyecto sin capibara.cfg da un error claro")
    bench = subprocess.run([os.path.join(D, "build", "capibara"), PLAT, "--bench", "200", "--gpu=no"], capture_output=True, text=True, timeout=300,
                           env=dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", CAPI_ENTRADA=ENTRADA_PLAT)).stdout
    m = re.search(r"CAPI_BENCH cuadros (\d+) tiempo_ms (\d+) fps_x10 (\d+)", bench)
    verificar(m is not None, "bench en el PC: " + bench[-200:])
    print("    PC (software, SDL dummy): %.0f cuadros/s" % (int(m.group(3)) / 10.0))


def fuentes():
    for f in ("LICENSE", "LEEME.md", "Makefile", "capibara.mk"):
        verificar(os.path.isfile(os.path.join(D, f)), "existe " + f)
    verificar("MIT License" in open(os.path.join(D, "LICENSE")).read(), "LICENSE es la MIT")
    for dp, _, nombres in os.walk(os.path.join(D, "motor")):
        for n in nombres:
            if n.endswith((".c", ".h")):
                verificar("SPDX-License-Identifier: MIT" in open(os.path.join(dp, n), encoding="utf-8").read(), "MIT en " + n)
    for ej in (PLAT, DISP):
        cr = open(os.path.join(ej, "CREDITOS.txt"), encoding="utf-8").read()
        verificar("Kenney" in cr and "CC0" in cr, "créditos CC0 de Kenney en " + os.path.basename(ej))
        verificar("Creative Commons Zero" in open(os.path.join(ej, "LICENCIA-Kenney.txt"), errors="replace").read(), "licencia CC0 de Kenney en " + os.path.basename(ej))
    terc = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read().lower()
    verificar("capibara" in terc and "pixel platformer" in terc and "pixel shmup" in terc, "docs/terceros.md registra los gráficos de Kenney")
    dec = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D452." in dec, "decisión D452")
    verificar("Capibara" in open(os.path.join(RAIZ, "docs", "ROADMAP.md"), encoding="utf-8").read(), "fila en el ROADMAP")
    barra = open(os.path.join(RAIZ, "kernel", "gui", "barra.c"), encoding="latin-1").read()
    verificar("/apps/capibara/plataformas" in barra and "/apps/capibara/disparos" in barra, "entradas en Inicio > Juegos")
    verificar("arte_mini_capibara" in open(os.path.join(RAIZ, "kernel", "gui", "arte.c"), encoding="latin-1").read(), "icono propio en kernel/gui/arte.c")
    verificar(os.path.isfile(os.path.join(RAIZ, "rootfs", "ayuda", "capibara.md")), "ayuda en rootfs/ayuda")
    verificar(os.path.isfile(os.path.join(RAIZ, "docs", "capibara.md")), "docs/capibara.md")
    mk = open(os.path.join(D, "capibara.mk")).read()
    verificar("SEM_GPL" not in mk, "Capibara entra también en la variante sin GPL")
    textos = [os.path.join(RAIZ, "docs", "capibara.md"), os.path.join(RAIZ, "rootfs", "ayuda", "capibara.md"), os.path.join(D, "LEEME.md"),
              os.path.join(D, "capibara.mk")]
    for dp, _, nombres in os.walk(D):
        if "build" in dp.split(os.sep):
            continue
        for n in nombres:
            if n.endswith((".c", ".h", ".lua", ".esc", ".cfg", ".py", ".txt")):
                textos.append(os.path.join(dp, n))
    for t in textos:
        try:
            s = open(t, encoding="utf-8").read()
        except UnicodeDecodeError:
            continue
        verificar(not re.search("[Ͱ-ӿ　-鿿]", s), "sin otros alfabetos en " + os.path.relpath(t, RAIZ))


# ---------------------------------------------------------------------------------------------------- QEMU
def leer_png(p):
    from h444_brumacanto import leer_png as lp
    return lp(p)


def parecidas(p1, p2):
    w, h, b1, f1 = leer_png(p1)
    _, _, b2, f2 = leer_png(p2)
    bien = tot = 0
    for y in range(100, min(h, 700), 3):
        for x in range(10, min(w, 1000), 3):
            a, b = f1[y][x * b1:x * b1 + 3], f2[y][x * b2:x * b2 + 3]
            tot += 1
            if max(abs(a[0] - b[0]), abs(a[1] - b[1]), abs(a[2] - b[2])) <= 40:
                bien += 1
    return bien / tot


def lanzar(m, comando, patron, tiempo=300):
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    h136.focar_terminal(m)
    marca = len(m.serial())
    m.digitar(comando, 0.04)
    m.tecla("ret")
    return marca, esperar(m, patron, tiempo, desde=marca)


def sesion(nome, extras, gpu, captura):
    from h60_barra_tareas import esperar
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "capibara")):
        raise FalhaTeste("la imagen no lleva capibara (make)")
    img = imagem_de_teste("h452" + nome, ["--vesa", "--apps", APPS_RAIZ] + extras)
    res = {}
    with Maquina("h452" + nome, "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        opc = "--hud --gpu=si" if gpu else "--hud --gpu=no"
        marca, r = lanzar(m, "CAPI_ENTRADA=\"5:derecha+;60:saltar+;70:saltar-\" /apps/bin/capibara /apps/capibara/plataformas " + opc,
                          r"CAPI_ARRANQUE proyecto=\"[^\"]*\" pantalla=(\d+x\d+) renderizador=(\S+) gpu=(\d)")
        verificar(r[0] == "320x180", "pantalla lógica 320x180 (%s)" % r[0])
        res["rend"] = r[1]
        if gpu:
            verificar(r[1] == "saguios-gpu" and r[2] == "1", "con la GPU simulada el renderizador es saguios-gpu (%s)" % r[1])
        else:
            verificar(r[1] == "saguios" and r[2] == "0", "por software el renderizador es saguios (%s)" % r[1])
        esperar(m, r"CAPI_ESCENA escenas/nivel1.esc nodos=(\d+)", 60, desde=marca)
        esperar(m, r"CAPI_JUEGO salto", 120 if gpu else 90, desde=marca)
        esperar(m, r"CAPI_JUEGO moneda 1/24", 120 if gpu else 90, desde=marca)
        verificar(True, "plataformas: entrada guionada: salta y recoge una moneda")
        time.sleep(3 if gpu else 1.5)
        m.captura(captura)
        m.tecla("esc")
        esperar(m, r"processo \d+ \(capibara\) terminou com codigo (\d+)", 120, desde=marca)
        verificar(re.findall(r"\(capibara\) terminou com codigo (\d+)", m.serial()[marca:])[-1] == "0", "Esc sale limpio (código 0)")
        esperar(m, r"sagui:/\$", 60, desde=marca)
        s = m.serial()[marca:]
        verificar("[CAPI] error en" not in s and "CAPI_FIN" in s, "sin errores de Lua y con CAPI_FIN")
        time.sleep(1)
        if not gpu:
            # teclado real: sin entrada guionada, Espacio hace saltar al personaje (la acción «saltar» liga con Space)
            marca, r = lanzar(m, "/apps/bin/capibara /apps/capibara/plataformas --gpu=no", r"CAPI_ESCENA escenas/nivel1.esc nodos=(\d+)")
            time.sleep(3)
            m.tecla("spc", 0.3)
            esperar(m, r"CAPI_JUEGO salto", 60, desde=marca)
            verificar(True, "teclado real: Espacio salta")
            m.tecla("esc")
            esperar(m, r"processo \d+ \(capibara\) terminou com codigo 0", 120, desde=marca)
            esperar(m, r"sagui:/\$", 60, desde=marca)
            # el segundo ejemplo, con tecla real mantenida por la entrada guionada
            marca, r = lanzar(m, "CAPI_ENTRADA=\"3:disparar+\" /apps/bin/capibara /apps/capibara/disparos --hud --gpu=no", r"CAPI_ESCENA escenas/partida.esc nodos=(\d+)")
            esperar(m, r"CAPI_JUEGO enemigo_destruido", 200, desde=marca)
            time.sleep(1.5)
            m.captura("H452-2-capibara-escuadron")
            m.tecla("esc")
            esperar(m, r"processo \d+ \(capibara\) terminou com codigo 0", 120, desde=marca)
            esperar(m, r"sagui:/\$", 60, desde=marca)
            # paquete .capi dentro de SaguiOS
            marca, r = lanzar(m, "/apps/bin/capibara --empaquetar /apps/capibara/disparos /tmp/d.capi", r"capibara: (\d+) ficheros empaquetados")
            verificar(int(r) >= 15, "empaquetar en SaguiOS: %s ficheros" % r)
            esperar(m, r"sagui:/\$", 60, desde=marca)
            marca, r = lanzar(m, "CAPI_ENTRADA=\"3:disparar+\" /apps/bin/capibara /tmp/d.capi --bench 120 --gpu=no", r"CAPI_BENCH cuadros (\d+) tiempo_ms (\d+) fps_x10 (\d+)", 600)
            print("    cuadros/s del paquete .capi (disparos, software): %.1f" % (int(r[2]) / 10.0))
            verificar(int(r[2]) > 0, "el paquete .capi se abre y corre")
            esperar(m, r"sagui:/\$", 60, desde=marca)
        # fps medidos (el driver simulado dibuja con la CPU del QEMU: los fps no dicen nada del hardware real)
        opcb = "--gpu=si" if gpu else "--gpu=no"
        marca, r = lanzar(m, "CAPI_ENTRADA=\"5:derecha+\" /apps/bin/capibara /apps/capibara/plataformas --bench %d %s" % (60 if gpu else 150, opcb),
                          r"CAPI_BENCH cuadros (\d+) tiempo_ms (\d+) fps_x10 (\d+) peor_cuadro_ms ([\d.]+) quads (\d+) lotes (\d+) render (\S+) gpu (\d)", 900)
        print("    cuadros/s plataformas (%s): %.1f, peor cuadro %s ms, %s cuadriláteros, %s lotes, render %s" % ("GPU simulada" if gpu else "software", int(r[2]) / 10.0, r[3], r[4], r[5], r[6]))
        verificar(int(r[2]) > 0 and r[7] == ("1" if gpu else "0"), "mide los cuadros por segundo")
        res["fps"] = int(r[2]) / 10.0
        if gpu:
            # a pantalla completa, --gpu=auto (por defecto) pide el renderizador acelerado
            esperar(m, r"sagui:/\$", 60, desde=marca)
            marca, r = lanzar(m, "/apps/bin/capibara /apps/capibara/plataformas --pantalla-completa --bench 20",
                              r"CAPI_ARRANQUE proyecto=\"[^\"]*\" pantalla=(\d+x\d+) renderizador=(\S+) gpu=(\d)", 300)
            verificar(r[1] == "saguios-gpu" and r[2] == "1", "a pantalla completa, --gpu=auto elige la GPU (%s)" % r[1])
            esperar(m, r"CAPI_BENCH cuadros (\d+)", 900, desde=marca)
            verificar("[SDL2]" in m.serial()[marca:], "la SDL de SaguiOS deja su registro del renderizador por GPU")
            esperar(m, r"sagui:/\$", 60, desde=marca)
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones del kernel")
    return os.path.join(RAIZ, "docs", "capturas", captura + ".png"), res


def saguios():
    sesion("sw", [], False, "H452-1-capibara-plataformas")


def gpu():
    a = os.path.join(RAIZ, "docs", "capturas", "H452-1-capibara-plataformas.png")
    if not os.path.exists(a):
        a, _ = sesion("sw", [], False, "H452-1-capibara-plataformas")
    b, res = sesion("gpu", ["--gpu3d=simulado"], True, "H452-3-capibara-gpu-simulada")
    f = parecidas(a, b)
    print("    parecido software / GPU simulada: %.1f %%" % (f * 100))
    verificar(f >= 0.80, "la imagen por la GPU simulada se parece a la de software (%.1f %%)" % (f * 100))


def audio():
    """Con una placa de sonido (HDA) el hilo de audio propio escribe en la tarjeta: la música y los efectos salen en el .wav del QEMU."""
    from h60_barra_tareas import esperar
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "capibara")):
        raise FalhaTeste("la imagen no lleva capibara (make)")
    img = imagem_de_teste("h452audio", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h452audio", "ide", imagem=img, memoria="256M", som="hda-output") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        marca, _ = lanzar(m, "CAPI_ENTRADA=\"5:derecha+;60:saltar+;70:saltar-\" /apps/bin/capibara /apps/capibara/plataformas --gpu=no",
                          r"CAPI_ESCENA escenas/nivel1.esc nodos=(\d+)")
        esperar(m, r"CAPI_JUEGO moneda 1/24", 120, desde=marca)
        time.sleep(8)   # la música sigue sonando
        verificar("audio: hilo propio, con tarjeta" in m.serial()[marca:], "el hilo de audio propio abre la tarjeta")
        m.tecla("esc")
        esperar(m, r"processo \d+ \(capibara\) terminou com codigo 0", 120, desde=marca)
        verificar("[CAPI] error en" not in m.serial()[marca:], "sin errores de Lua")
        wav = m.wav
    # el QEMU no rellena los tamaños de la cabecera del .wav: se lee en bruto (PCM de 16 bits, estéreo, 48 kHz) tras los 44 bytes
    import struct
    crudo = open(wav, "rb").read()[44:]
    n = len(crudo) // 2
    muestras = struct.unpack("<%dh" % n, crudo[:n * 2])
    sonoras = sum(1 for v in muestras if abs(v) > 200)
    seg = n / 2 / 48000.0
    print("    sonido de la tarjeta: %.1f s, %.0f %% de muestras sonoras" % (seg, 100.0 * sonoras / max(1, n)))
    verificar(seg > 8 and sonoras > n * 0.05, "la música y los efectos llegan a la tarjeta (el .wav del QEMU no está en silencio)")


def main():
    pedidas = sys.argv[1:] or ["nucleo", "ejemplos", "fuentes", "saguios", "audio", "gpu"]
    for p in pedidas:
        print("==", p)
        globals()[p]()
    print("H452 OK")


if __name__ == "__main__":
    try:
        main()
    except FalhaTeste as e:
        print("FALLO:", e)
        sys.exit(1)
