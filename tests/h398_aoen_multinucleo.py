#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H398 - AoEN en varios núcleos (D393, mejora 140): hilos de la SDL 2, terreno por franjas e IA en paralelo.

Partes (por defecto: pc determinismo-pc tcc saguios; «bench» aparte):
  pc             en el PC: hilos.c/hilos.h existen, la SDL 2 del sistema declara hilos, semáforos y atómicas, D393 y la
                 mejora 140 lo cuentan, y las texturas sintéticas (tools/aoen_texturas_sintetico.py) son PNG de 512x512.
  determinismo-pc en el PC (tests/aoen_ia.c con la SDL del anfitrión): partidas de 8 jugadores de la máquina con
                 AOEN_HILOS=1, 2 y 4 dan la misma suma del mundo en cada punto de control, y la IA pensada en orden
                 inverso (AOEN_IA_ORDEN=inverso) también: ninguna IA lee lo que otra cambia mientras piensan.
  tcc            en el PC: el TCC cruzado compila AoEN (hilos.c incluido) a un ELF de SaguiOS.
  saguios        en SaguiOS con --smp y 4 CPU (DRS y texturas sintéticos): AoEN ve los 4 núcleos y crea 3 hilos de
                 trabajo; con AOEN_HILOS=1 no crea ninguno; la simulación (AOEN_SIM) da la misma suma con 1 y 4 hilos;
                 el cuadro dibujado (terreno HD, terreno clásico con efectos) es idéntico bit a bit con 1 y 4 hilos;
                 sin --smp ve un núcleo. Captura H398-1.
  bench          (aparte, mejor con SAGUI_KVM=1) AOEN_BENCH con 1, 2 y 4 CPU (H398_CPUS="1,2,4"), terreno HD
                 (texturas sintéticas) y clásico, en una partida cargada (8 jugadores, 10 minutos adelantados).
Los DRS y las texturas son sintéticos (tools/aoen_drs_sintetico.py, tools/aoen_texturas_sintetico.py): nunca los
datos del juego original.
"""
import os
import re
import subprocess
import sys
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))

KVM = os.environ.get("SAGUI_KVM") == "1"


def jogos_sinteticos():
    """Carpeta para --jogos con los DRS sintéticos en AOE/DATA y las texturas sintéticas en AOE/TEX."""
    jogos = tempfile.mkdtemp(prefix="aoen-mt-")
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


def orden(m, texto, patron, tempo):
    """Escribe una orden en el terminal y espera el patrón en el registro (desde esa orden)."""
    from h60_barra_tareas import esperar
    marca = len(m.serial())
    m.digitar(texto, 0.03)
    m.tecla("ret")
    g = esperar(m, patron, tempo, desde=marca)
    return g, m.serial()[marca:]


def pc():
    from qemu import verificar
    d = os.path.join(RAIZ, "userland", "juegos", "aoen")
    for f in ("hilos.c", "hilos.h"):
        verificar(os.path.isfile(os.path.join(d, f)), "existe userland/juegos/aoen/%s" % f)
        t = open(os.path.join(d, f), encoding="utf-8").read()
        verificar("SPDX-License-Identifier: GPL-3.0-or-later" in t.splitlines()[0], "%s lleva SPDX GPL-3.0-or-later" % f)
    h = open(os.path.join(RAIZ, "userland", "libsdl2", "SDL.h"), encoding="utf-8").read()
    for fn in ("SDL_CreateThread", "SDL_CondBroadcast", "SDL_CondWaitTimeout", "SDL_CreateSemaphore", "SDL_SemWaitTimeout",
               "SDL_AtomicAdd", "SDL_ThreadID", "SDL_TryLockMutex", "SDL_GetCPUCount"):
        verificar(fn in h, "SDL.h declara %s" % fn)
    c = open(os.path.join(RAIZ, "userland", "libsdl2", "sdl2_sagui.c"), encoding="utf-8").read()
    verificar("thread_create(inicio_hilo_sdl" in c and "futex_esperar(&c->avisos" in c, "la SDL 2 crea hilos de verdad y espera con futex")
    verificar("el SaguiOS no tiene hilos" not in c, "SDL_CreateThread ya no falla siempre")
    t = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D393" in t, "docs/decisoes.md tiene D393")
    t = open(os.path.join(RAIZ, "mejoras", "140-aoen.md"), encoding="utf-8").read()
    verificar("D393" in t and "AOEN_HILOS" in t, "la mejora 140 cuenta el multinúcleo (D393, AOEN_HILOS)")
    dtex = tempfile.mkdtemp(prefix="aoen-tex-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_texturas_sintetico.py"), dtex], check=True, stdout=subprocess.DEVNULL)
    for n in ("g_gr2", "g_gr6", "g_des", "g_pal", "g_wt5", "g_wt3"):
        b = open(os.path.join(dtex, n + "_00_color.png"), "rb").read()
        verificar(b[:8] == b"\x89PNG\r\n\x1a\n" and int.from_bytes(b[16:20], "big") == 512 and int.from_bytes(b[20:24], "big") == 512,
                  "textura sintética %s: PNG de 512x512" % n)


def determinismo_pc():
    from qemu import verificar
    import h389_aoen
    exe = h389_aoen.compilar_arnes()

    def correr(args, **env):
        t0 = time.time()
        r = subprocess.run([exe] + [str(a) for a in args], capture_output=True, text=True, timeout=1800,
                           env=dict(os.environ, AOEN_IA_SUMA="1", **{k: str(v) for k, v in env.items()}))
        sumas = re.findall(r"^suma (?:final )?t=(\d+) ([0-9a-f]{8})", r.stdout, re.M)
        return sumas, time.time() - t0, r.stdout

    # 8 jugadores de la máquina (difícil contra difícil), mapa costero de 150 y 15 minutos; y un mapa de islas (barcos)
    for args in ([7, 2, 2, 2, 150, 15, 8], [11, 2, 1, 0, 120, 12, 6]):
        base, t1, _ = correr(args, AOEN_HILOS=1)
        verificar(len(base) >= 5, "semilla %d: %d sumas del mundo con 1 hilo (%.1f s)" % (args[0], len(base), t1))
        inv, _, _ = correr(args, AOEN_HILOS=1, AOEN_IA_ORDEN="inverso")
        verificar(inv == base, "semilla %d: las IA pensando en orden inverso dan las mismas sumas (ninguna lee lo que cambia otra)" % args[0])
        for n in (2, 4):
            otro, tn, out = correr(args, AOEN_HILOS=n)
            verificar(otro == base, "semilla %d: con %d hilos las mismas %d sumas que con 1 (%.1f s contra %.1f s)" % (args[0], n, len(base), tn, t1))
            verificar(re.search(r"suma final t=\d+ [0-9a-f]{8} hilos=%d" % n, out) is not None, "semilla %d: el arnés usó %d hilos" % (args[0], n))


def tcc():
    from qemu import verificar, APPS_RAIZ, BUILD
    cruzado = os.path.join(BUILD, "tcc", "anfitrion", "i386-tcc")
    raiz = os.path.join(BUILD, "tcc", "raiz")
    ao = os.path.join(APPS_RAIZ, "aoen", "fuentes")
    if not os.path.isfile(cruzado) or not os.path.isdir(ao):
        print("AVISO: no hay TCC cruzado o fuentes de AoEN en la imagen (make): se salta «tcc»")
        return
    fontes = re.search(r'FUENTES="([^"]+)"', open(os.path.join(ao, "compilar.sh"), encoding="utf-8").read()).group(1).split()
    verificar("hilos.c" in fontes, "compilar.sh compila hilos.c")
    d = tempfile.mkdtemp(prefix="h398-tcc-")
    r = subprocess.run([cruzado, "-B" + raiz, "-DSAGUI_AOEN", "-DAOEN_FONTE_SAGUI", "-DSTBI_NO_THREAD_LOCALS", "-DSTBI_NO_SIMD", "-DNDEBUG",
                        "-I.", "-o", os.path.join(d, "aoen")] + fontes + ["-lSDL2"], cwd=ao, capture_output=True, text=True)
    verificar(r.returncode == 0, "el TCC cruzado compila AoEN con los hilos (%s)" % r.stderr.strip()[-300:])


def saguios():
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta «saguios»")
        return
    jogos = jogos_sinteticos()
    lento = 1 if KVM else 4
    img = imagem_de_teste("h398-smp", ["--vesa", "--smp", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h398-smp", "ide", imagem=img, memoria="512M", nucleos=4) as m:
        arrancar(m)
        # simulación sin dibujo: la misma suma con 1 hilo y con los 4 núcleos
        sim = "AOEN_NOSOUND=1 AOEN_SCENE=game AOEN_PLAYERS=4 AOEN_SIZE=2 AOEN_SEED=7 AOEN_DIF=2 AOEN_ESPECTADOR=1 AOEN_SIM=%d" % (300 if KVM else 120)
        pat = r"\[AOEN\] sim final t=(\d+) pasos=(\d+) ms=(\d+) suma=([0-9a-f]{8}) ents=(\d+) hilos=(\d+)"
        g1, t1 = orden(m, sim + " AOEN_HILOS=1 /apps/bin/aoen", pat, 600 * lento)
        # el registro llega por el serie en UTF-8 y se lee como Latin-1: «núcleos» no se compara letra a letra
        verificar(re.search(r"\[AOEN\] hilos: 1 \(n\S+cleos 4, AOEN_HILOS\)", t1) is not None, "con AOEN_HILOS=1 no crea hilos aunque haya 4 núcleos")
        g4, t4 = orden(m, sim + " /apps/bin/aoen", pat, 600 * lento)
        verificar(re.search(r"\[AOEN\] hilos: 4 \(n\S+cleos 4\)", t4) is not None, "con --smp y 4 CPU, AoEN ve 4 núcleos y trabaja con 4 hilos")
        s1 = re.findall(r"\[AOEN\] sim t=(\d+) suma=([0-9a-f]{8})", t1)
        s4 = re.findall(r"\[AOEN\] sim t=(\d+) suma=([0-9a-f]{8})", t4)
        verificar(g1[3] == g4[3] and s1 == s4 and len(s1) >= 2, "la simulación da la misma suma con 1 y con 4 hilos (%s, %d puntos de control; %s ms contra %s ms)"
                  % (g4[3], len(s1), g1[2], g4[2]))
        verificar("EXCECAO" not in m.serial(), "sin excepciones del kernel")
        # el mismo cuadro, bit a bit, con 1 y con 4 hilos: terreno HD (texturas sintéticas) y clásico con efectos
        # partida de 8 adelantada: terreno, entidades, sombras, barras, partículas (lista de dibujo por franjas) y, con el perfil
        # Completo, el día y la noche
        cuadro = ("AOEN_NOSOUND=1 AOEN_SCENE=game AOEN_PLAYERS=8 AOEN_SIZE=4 AOEN_SEED=7 AOEN_DIF=2 AOEN_ESPECTADOR=1 AOEN_FFWD="
                  + ("400" if KVM else "60") + " AOEN_REVEAL=1 AOEN_PASO_FIJO=1 AOEN_TIEMPO_ANIM=3.5 AOEN_SUMA_CUADRO=20 "
                  "AOEN_SHOT=/tmp/c.bmp AOEN_FRAMES=21 AOEN_EFECTOS=%s %s %s /apps/bin/aoen")
        pc = r"\[AOEN\] suma del cuadro 20: ([0-9a-f]{8}) \(t=([\d.]+), hilos (\d)(?:, mundo ([0-9a-f]{8}))?\)"
        for nombre, ef, tex in (("HD", "1", "AOEN_TEX=/jogos/AOE/TEX"), ("clásico", "1", ""), ("clásico completo", "2", "")):
            a, _ = orden(m, cuadro % (ef, tex, "AOEN_HILOS=1"), pc, 600 * lento)
            b, _ = orden(m, cuadro % (ef, tex, ""), pc, 600 * lento)
            verificar(a[0] == b[0] and a[2] == "1" and b[2] == "4", "terreno %s: el cuadro sale igual con 1 y con 4 hilos (%s; con 4 hilos %s; suma del mundo %s y %s)" % (nombre, a[0], b[0], a[3], b[3]))
        # captura: la partida de 8 jugadores con el terreno HD sintético, dibujada con 4 hilos
        marca = len(m.serial())
        m.digitar("AOEN_NOSOUND=1 AOEN_SCENE=game AOEN_PLAYERS=8 AOEN_SIZE=4 AOEN_SEED=7 AOEN_DIF=2 AOEN_ESPECTADOR=1 AOEN_REVEAL=1 "
                  "AOEN_FFWD=%d AOEN_TEX=/jogos/AOE/TEX AOEN_EFECTOS=1 /apps/bin/aoen" % (300 if KVM else 60), 0.03)
        m.tecla("ret")
        from h60_barra_tareas import esperar
        esperar(m, r"\[AOEN\] estado t=\d+ ents=\d+", 900 * lento, desde=marca)
        time.sleep(3)
        m.captura("H398-1-hd-sintetico-4-hilos")
        m.tecla("esc")
    img = imagem_de_teste("h398-sinsmp", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h398-sinsmp", "ide", imagem=img, memoria="512M", nucleos=4) as m:
        arrancar(m)
        g, t = orden(m, "AOEN_NOSOUND=1 AOEN_SCENE=game AOEN_SIM=30 /apps/bin/aoen", r"\[AOEN\] sim final t=(\d+)", 300 * lento)
        verificar(re.search(r"\[AOEN\] hilos: 1 \(n\S+cleos 1\)", t) is not None, "sin --smp el sistema usa una CPU y AoEN no crea hilos")


def bench():
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    jogos = jogos_sinteticos()
    cpus = [int(c) for c in os.environ.get("H398_CPUS", "1,2,4").split(",")]
    cuadros = os.environ.get("H398_BENCH", "600")
    ffwd = os.environ.get("H398_FFWD", "600")
    res = os.environ.get("H398_RES", "1024x768")
    extra = os.environ.get("H398_EXTRA", "")
    # H398_ANTES=1: también /apps/bin/aoen-antes (la versión de antes compilada a mano en la imagen, para comparar)
    casos = [("hd", "AOEN_TEX=/jogos/AOE/TEX AOEN_EFECTOS=1", "aoen"), ("clasico", "AOEN_EFECTOS=1", "aoen")]
    if os.environ.get("H398_ANTES") == "1":
        casos = [("antes-hd", casos[0][1], "aoen-antes"), ("antes-clasico", casos[1][1], "aoen-antes")] + casos
    casos = [c for c in casos if c[0].replace("antes-", "") in os.environ.get("H398_CASOS", "hd,clasico").split(",")]
    con_sim = os.environ.get("H398_SIM", "1500") != "0"
    resultados = []
    for n in cpus:
        img = imagem_de_teste("h398-bench", ["--vesa", "--resolucao", res, "--smp", "--apps", APPS_RAIZ,
                                             "--jogos", jogos, "--jogos-mb", "64"])
        with Maquina("h398-bench-%d" % n, "ide", imagem=img, memoria="512M", nucleos=n) as m:
            arrancar(m)
            for nombre, opc, binario in casos:
                # partida cargada: 8 jugadores de la máquina (difícil; también el 1, para que nadie pierda), mapa de 200
                cmd = ("AOEN_NOSOUND=1 AOEN_FULL=1 AOEN_SCENE=game AOEN_PLAYERS=8 AOEN_SIZE=4 AOEN_SEED=7 AOEN_DIF=2 AOEN_ESPECTADOR=1 "
                       "AOEN_REVEAL=1 AOEN_FFWD=%s AOEN_PERFIL=1 %s %s AOEN_BENCH=%s /apps/bin/%s" % (ffwd, opc, extra, cuadros, binario))
                g, t = orden(m, cmd, r"\[AOEN\] bench (\d+)x(\d+) cuadros=(\d+) ms=(\d+) fps=([\d.]+)", 1500 if KVM else 6000)
                perf = re.findall(r"\[AOEN\] perfil: ([^\r\n]+)", t)
                hilos = re.findall(r"\[AOEN\] hilos: ([^\r\n]+)", t)
                print("    %d CPU %-8s %s cuadros en %s ms = %s fps | %s | %s" % (n, nombre, g[2], g[3], g[4], hilos[-1] if hilos else "-",
                                                                         perf[-1] if perf else "-"))
                resultados.append((n, nombre, float(g[4])))
                time.sleep(2)
            # la simulación sola (sin dibujo): 25 minutos de la misma partida (H398_SIM=0: no)
            if not con_sim:
                continue
            g, t = orden(m, "AOEN_NOSOUND=1 AOEN_SCENE=game AOEN_PLAYERS=8 AOEN_SIZE=4 AOEN_SEED=7 AOEN_DIF=2 AOEN_ESPECTADOR=1 "
                         "AOEN_SIM=%s %s /apps/bin/aoen" % (os.environ.get("H398_SIM", "1500"), extra),
                         r"\[AOEN\] sim final t=(\d+) pasos=(\d+) ms=(\d+) suma=([0-9a-f]{8}) ents=(\d+) hilos=(\d+)", 3000 if KVM else 20000)
            print("    %d CPU sim      %s pasos en %s ms (%.3f ms por tic), suma %s, hilos %s" % (n, g[1], g[2], int(g[2]) / max(1, int(g[1])), g[3], g[5]))
            resultados.append((n, "sim", 1000.0 * int(g[1]) / max(1, int(g[2]))))
    for nombre in [c[0] for c in casos] + ["sim"]:
        base = [r[2] for r in resultados if r[1] == nombre and r[0] == 1]
        for r in resultados:
            if r[1] == nombre and base and r[0] > 1:
                print("    %s: %d CPU = %.2f veces 1 CPU" % (nombre, r[0], r[2] / base[0]))
    verificar(all(r[2] > 0 for r in resultados), "todas las medidas dan fps > 0")
    return resultados


if __name__ == "__main__":
    partes = sys.argv[1:] or ["pc", "determinismo-pc", "tcc", "saguios"]
    for p in partes:
        print("== " + p)
        globals()[p.replace("-", "_")]()
    print("H398 ok")
