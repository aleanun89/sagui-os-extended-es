#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H389 - AoEN, RTS estilo Age of Empires (D382, mejora 140).

Partes (por defecto todas; o las que se pidan: h389_aoen.py generador sin-datos con-datos):
  generador  en el PC: tools/aoen_drs_sintetico.py genera unos DRS mínimos SINTÉTICOS (nada del juego original) con la
             estructura correcta (cabecera, tablas, SLP con bordes y órdenes de dibujo y paleta JASC).
  licencia   las fuentes de userland/juegos/aoen llevan SPDX GPL-3.0-or-later, stb_image está en terceros, hay D382,
             la mejora 140 y la fila de stb_image en docs/terceros.md; el Makefile no copia texturas ni música a git.
  sin-datos  en SaguiOS: Inicio > Juegos tiene AoEN; sin los DRS abre su ventana con las instrucciones.
  con-datos  en SaguiOS con los DRS sintéticos en /jogos/AOE/DATA (lo que busca por defecto): arranca una escaramuza,
             selecciona unidades, las mueve con el botón derecho y guarda (F5) y carga (F9) la partida.
  lectores   en el PC con ASan: DRS, SLP y .sav manipulados dan error limpio (tests/aoen_lectores.c).
  efectos    perfil por defecto, obras y ruinas, y la interfaz escalada a 1920x1080.
  bench      (aparte) fps a 800x600 y 1024x768, con efectos 0, 1 y 2; H389_RES, H389_EFECTOS, H389_BENCH, H389_RENDER.
Capturas en docs/capturas/H389-*.png. Los DRS reales nunca se usan aquí.
"""
import os
import re
import subprocess
import sys
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))


def generador():
    from qemu import verificar
    d = tempfile.mkdtemp(prefix="aoen-drs-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), d], check=True, stdout=subprocess.DEVNULL)
    for n in ("GRAPHICS.DRS", "INTERFAC.DRS", "TERRAIN.DRS"):
        b = open(os.path.join(d, n), "rb").read()
        verificar(len(b) > 64 and b[40:44] == b"1.00", "%s tiene cabecera DRS (%d bytes)" % (n, len(b)))
    b = open(os.path.join(d, "INTERFAC.DRS"), "rb").read()
    verificar(b"JASC-PAL" in b, "INTERFAC.DRS lleva la paleta JASC")
    verificar(b"Copyright" in open(os.path.join(d, "TERRAIN.DRS"), "rb").read()[:64] and b"sintetico" in b, "los DRS se declaran sintéticos")


def licencia():
    from qemu import verificar
    d = os.path.join(RAIZ, "userland", "juegos", "aoen")
    fuentes = [f for f in os.listdir(d) if f.endswith((".c", ".h"))]
    verificar(len(fuentes) >= 15, "hay %d fuentes de AoEN" % len(fuentes))
    for f in fuentes:
        t = open(os.path.join(d, f), encoding="utf-8").readline()
        verificar("SPDX-License-Identifier: GPL-3.0-or-later" in t, "%s lleva SPDX GPL-3.0-or-later" % f)
    verificar(os.path.isfile(os.path.join(RAIZ, "userland", "terceros", "stbimage", "stb_image.h")), "stb_image.h vive en userland/terceros/stbimage")
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("stb_image" in t, "docs/terceros.md tiene la fila de stb_image")
    t = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D382" in t, "docs/decisoes.md tiene D382")
    verificar(os.path.isfile(os.path.join(RAIZ, "mejoras", "140-aoen.md")), "existe mejoras/140-aoen.md")
    ig = subprocess.run(["git", "ls-files", "userland/juegos", "input_share"], cwd=RAIZ, capture_output=True, text=True).stdout
    verificar(not re.search(r"\.(drs|DRS|mid|png|o)$", ig, re.M), "ninguna textura, música ni DRS en git")


def ventana(m, desde=0):
    g = re.findall(r'criada por \d+: "AoEN" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial()[desde:])
    if not g:
        return None
    l, a, x, y = [int(v) for v in g[-1]]
    return dict(l=l, a=a, x=x + 2, y=y + 24)


def sin_datos():
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta la parte «sin-datos»")
        return
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h389-sindatos", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        sub = submenu_pelo_mouse(m, p, "Juegos", "AoEN")
        verificar("AoEN" in sub["linhas"], "Inicio > Juegos tiene AoEN (%s)" % ", ".join(sub["linhas"]))
        time.sleep(0.4)
        m.captura("H389-1-menu-juegos")
        marca = len(m.serial())
        clicar_aqui(m)
        esperar(m, r"inicio -> /apps/bin/aoen", 30, desde=marca)
        esperar(m, r"\[AOEN\] sin datos del juego", 60, desde=marca)
        time.sleep(2)
        verificar(ventana(m, marca) is not None, "AoEN abre su ventana con las instrucciones")
        m.captura("H389-2-instrucciones")


def con_datos():
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ, SAIDA
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta la parte «con-datos»")
        return
    jogos = tempfile.mkdtemp(prefix="aoen-jogos-")
    drs = os.path.join(jogos, "AOE", "DATA")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), drs], check=True, stdout=subprocess.DEVNULL)
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h389-condatos", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        h136.focar_terminal(m)
        marca = len(m.serial())
        # AOEN_SCENE=game arranca una escaramuza de 4 jugadores sin pasar por los menús; AOEN_SELECT deja 3 aldeanos elegidos
        m.digitar("AOEN_SCENE=game AOEN_SELECT=1 /apps/bin/aoen", 0.04)
        m.tecla("ret")
        esperar(m, r"\[AOEN\] inicio: ventana \d+x\d+, datos /jogos/AOE/DATA", 90, desde=marca)
        verificar(True, "AoEN encuentra los datos en /jogos/AOE/DATA")
        esperar(m, r"\[AOEN\] partida iniciada: 4 jugadores", 120, desde=marca)
        e = esperar(m, r"\[AOEN\] estado t=\d+ ents=(\d+) sel=3 pos=(-?[\d.]+),(-?[\d.]+) fps=(\d+)", 120, desde=marca)
        verificar(int(e[0]) > 20, "la escaramuza tiene %s entidades y 3 aldeanos seleccionados" % e[0])
        x0, y0 = float(e[1]), float(e[2])
        time.sleep(3)
        m.captura("H389-3-escaramuza")
        v = ventana(m, marca)
        verificar(v is not None, "AoEN abre su ventana %dx%d" % (v["l"], v["a"]) if v else "AoEN abre su ventana")
        # enfocar el juego y dar la orden de mover con el botón derecho en un punto del mapa
        h136.focar_jogo = h136.focar_jogo
        m.clique(v["x"] + v["l"] // 2, v["y"] + v["a"] // 2 - 120, botao=1)   # clic izquierdo: enfoca (y deselecciona)
        time.sleep(1)
        # volver a elegir con una caja alrededor del centro urbano (donde empiezan los aldeanos)
        marca = len(m.serial())
        m.arrastar(v["x"] + v["l"] // 2 - 140, v["y"] + v["a"] // 2 - 110, v["x"] + v["l"] // 2 + 140, v["y"] + v["a"] // 2 + 110)
        esperar(m, r"\[AOEN\] selecci\S+n con caja: [1-9]\d*", 30, desde=marca)
        verificar(True, "la selección con caja elige unidades")
        time.sleep(1.5)
        m.captura("H389-4-seleccion")
        marca = len(m.serial())
        m.clique(v["x"] + v["l"] // 2 + 150, v["y"] + v["a"] // 2 + 60, botao=2)
        esperar(m, r"\[AOEN\] orden mover: \d+ unidades hacia", 30, desde=marca)
        verificar(True, "el botón derecho da la orden de mover")
        time.sleep(8)
        m.captura("H389-5-unidades-movidas")
        # la unidad elegida se ha movido de verdad (posición en el registro de estado)
        est = re.findall(r"\[AOEN\] estado t=\d+ ents=\d+ sel=[1-9]\d* pos=(-?[\d.]+),(-?[\d.]+)", m.serial()[marca:])
        verificar(len(est) >= 2, "hay registros de estado con unidades elegidas tras la orden")
        d = ((float(est[-1][0]) - float(est[0][0])) ** 2 + (float(est[-1][1]) - float(est[0][1])) ** 2) ** 0.5
        verificar(d > 1.0, "la unidad se movió %.1f casillas hacia el destino" % d)
        # guardar y cargar con F5 / F9
        marca = len(m.serial())
        m.tecla("f5")
        esperar(m, r"\[AOEN\] guardada ranura 1 \(\d+ entidades\)", 30, desde=marca)
        verificar(True, "F5 guarda la partida")
        marca = len(m.serial())
        m.tecla("f9")
        esperar(m, r"\[AOEN\] cargada ranura 1 \(\d+ entidades\)", 30, desde=marca)
        verificar(True, "F9 carga la partida")
        time.sleep(3)
        m.captura("H389-6-partida-cargada")
        fps = [int(f) for f in re.findall(r"\[AOEN\] estado .* fps=(\d+)", m.serial())]
        print("    fps observados (ventana 1000x700, TCG sin KVM):", fps[-5:])
        verificar(bool(fps) and max(fps) >= 1, "el juego dibuja cuadros (fps máx. %d)" % max(fps))


def lectores():
    """En el PC con ASan/UBSan (tests/aoen_lectores.c): DRS, SLP y partidas manipulados dan error limpio, sin escribir ni leer fuera."""
    from qemu import verificar, SAIDA
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "aoen_lectores")
    d0 = os.path.join(RAIZ, "userland", "juegos", "aoen")
    fuentes = [os.path.join(d0, f) for f in sorted(os.listdir(d0)) if f.endswith(".c") and f not in ("aoen_config.c", "puente.c", "save.c", "main.c")]
    sdl = subprocess.run(["sdl2-config", "--cflags"], capture_output=True, text=True).stdout.split()
    libs = subprocess.run(["sdl2-config", "--libs"], capture_output=True, text=True).stdout.split()
    subprocess.run(["gcc", "-std=gnu99", "-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-w", "-I", d0,
                    "-I", os.path.join(RAIZ, "userland", "terceros", "stbimage")] + sdl + ["-o", exe, os.path.join(RAIZ, "tests", "aoen_lectores.c")] + fuentes + libs + ["-lm"], check=True)
    d = tempfile.mkdtemp(prefix="aoen-malos-")
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    import struct
    sys.path.insert(0, os.path.join(RAIZ, "tools"))
    import aoen_drs_sintetico as g
    ok=g.drs([(1,g.slp([g.unidad(1)]))])
    open(d+'/bien.drs','wb').write(ok)
    def hdr(n=1,off=76,cnt=1):
        return (b"x"*40+b"1.00"+b"tribe".ljust(12,b" ")+struct.pack("<II",n,off)).ljust(64,b"\0")
    # tabla con n enorme
    open(d+'/n_enorme.drs','wb').write(hdr()+struct.pack("<4sII",b"slp ",76,0xFFFFFFFF)+b"\0"*64)
    open(d+'/n_overflow_total.drs','wb').write(hdr(2)+struct.pack("<4sII",b"slp ",88,0x7FFFFFFF)+struct.pack("<4sII",b"slp ",88,0x7FFFFFFF)+b"\0"*64)
    open(d+'/lista_fuera.drs','wb').write(hdr(1,76)+struct.pack("<4sII",b"slp ",0xFFFFFF00,3)+b"\0"*12)
    open(d+'/truncado.drs','wb').write(ok[:50])
    # elemento con offset y tamaño fuera
    b=bytearray(ok); n=struct.unpack("<I",b[76+8:76+12])[0]; it=76+12
    b[it+4:it+8]=struct.pack("<I",0xFFFFFFF0); open(d+'/offset_fuera.drs','wb').write(bytes(b))
    b=bytearray(ok); b[it+8:it+12]=struct.pack("<I",0xFFFFFFFF); open(d+'/size_enorme.drs','wb').write(bytes(b))
    # SLP con cabecera de cuadro hostil: left negativo (-5) en la primera fila y cmd_off/out_off enormes
    slp=bytearray(g.slp([g.unidad(1)])); 
    fi=32; co,oo=struct.unpack("<II",slp[fi:fi+8])
    bad=bytearray(slp); 
    for y in range(3): bad[oo+y*4:oo+y*4+4]=struct.pack("<hh",-5,-7)
    open(d+'/left_negativo.drs','wb').write(g.drs([(1,bytes(bad))]))
    bad=bytearray(slp); bad[fi:fi+8]=struct.pack("<II",0xFFFFFFF0,0xFFFFFFF0)
    open(d+'/offsets_slp.drs','wb').write(g.drs([(1,bytes(bad))]))
    bad=bytearray(slp); bad[4:8]=struct.pack("<I",0x7FFFFFFF)
    open(d+'/nframes.drs','wb').write(g.drs([(1,bytes(bad))]))
    bad=slp[:len(slp)-3]  # comandos truncados
    open(d+'/cmd_truncado.drs','wb').write(g.drs([(1,bytes(bad))]))

    esperado = {"bien": "ok", "n_enorme": "rechazado", "n_overflow_total": "rechazado", "lista_fuera": "rechazado", "truncado": "rechazado",
                "offset_fuera": "ok", "size_enorme": "ok", "left_negativo": "ok", "offsets_slp": "ok", "nframes": "ok", "cmd_truncado": "ok"}
    for nombre, r in esperado.items():
        p = subprocess.run([exe, "drs", os.path.join(d, nombre + ".drs")], capture_output=True, text=True, env=env, cwd=d)
        verificar(p.returncode == 0 and p.stdout.strip() == r, "DRS %s: %s (sin fallos de memoria)" % (nombre, p.stdout.strip() or p.stderr[:200]))
    for k in (0, 1, 2, 3, 4, 5, 6, 8, 10, 11):
        p = subprocess.run([exe, "sav-mutar", os.path.join(d, "m%d.sav" % k), str(k)], capture_output=True, text=True, env=env, cwd=d)
        verificar(p.returncode == 0 and p.stdout.strip() == "rechazada actual-intacta", "partida manipulada %d rechazada y la actual se conserva" % k)
    for k in (7, 9):   # estos campos se sanean al leer: no debe fallar nada
        p = subprocess.run([exe, "sav-mutar", os.path.join(d, "m%d.sav" % k), str(k)], capture_output=True, text=True, env=env, cwd=d)
        verificar(p.returncode == 0 and "actual-intacta" in p.stdout, "partida con campo %d saneado: sin fallos de memoria" % k)


def efectos():
    """En SaguiOS (TCG o KVM) con los DRS sintéticos: el perfil por defecto con SSE2 es «Normal»; la escena de efectos
    (obra con andamios, edificio dañado, derrumbe) arranca con el perfil completo; y a 1920x1080 la interfaz sale x2
    con el zoom inicial de pantalla grande. Capturas H389-7 a H389-9."""
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    jogos = tempfile.mkdtemp(prefix="aoen-jogos-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")], check=True, stdout=subprocess.DEVNULL)
    for res, cmd, cap in (("1024x768", "AOEN_SCENE=game AOEN_FULL=1 /apps/bin/aoen", "H389-7-efectos-normal"),
                          ("1024x768", "AOEN_SCENE=fx AOEN_EFECTOS=2 AOEN_FULL=1 /apps/bin/aoen", "H389-8-obras-y-ruinas"),
                          (os.environ.get("H389_GRANDE", "1920x1080"), "AOEN_SCENE=game AOEN_EFECTOS=2 AOEN_FULL=1 /apps/bin/aoen", "H389-9-1080p")):
        img = imagem_de_teste("h389e-" + res, ["--vesa", "--resolucao", res, "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
        with Maquina("h389-efectos-" + res, "ide", imagem=img, memoria="512M" if res.startswith("19") else "256M") as m:
            esperar(m, r"sagui:/\$", 150)
            time.sleep(4)
            h136.focar_terminal(m)
            marca = len(m.serial())
            m.digitar(cmd, 0.04)
            m.tecla("ret")
            g = esperar(m, r"\[AOEN\] efectos: perfil (\S+) \(transiciones=(\d) agua=(\d) niebla=(\d) sombras=(\d) particulas=(\d) construccion=(\d) diaynoche=(\d) suave=(\d)\) sse2=(\d)", 120, desde=marca)
            if "EFECTOS" not in cmd:
                verificar(g[0] == "Normal" and g[1] == "1" and g[3] == "1" and g[6] == "1" and g[7] == "0",
                          "con SSE2 el perfil por defecto es Normal: transiciones, niebla y obras sí; día y noche no (%s)" % (g,))
            else:
                verificar(g[0] == "Completo" and g[7] == "1" and g[8] == "1", "AOEN_EFECTOS=2 activa el perfil Completo (día y noche, zoom suave)")
            esperar(m, r"\[AOEN\] partida iniciada", 120, desde=marca)
            p = esperar(m, r"\[AOEN\] pantalla (%s)x(%s), interfaz x(\d)" % tuple(res.split("x")), 120, desde=marca)
            verificar(True, "a pantalla completa el búfer es de %sx%s (la resolución del escritorio) con la interfaz x%s" % p)
            if res.startswith("19"):
                verificar(p[2] == "2", "a 1920x1080 la interfaz se escala x2 (píxeles nítidos)")
            esperar(m, r"\[AOEN\] estado t=", 120, desde=marca)
            time.sleep(4)
            m.captura(cap)


def bench():
    """Mide los fps de la escaramuza (DRS sintéticos) a 800x600 y 1024x768 a pantalla completa, con y sin efectos
    (AOEN_EFECTOS=0/1: la partida gráfica completa o el perfil de equipos lentos). H389_RES="800x600,1024x768"
    y H389_EFECTOS="0,1" lo cambian; H389_BENCH son los cuadros de cada medida."""
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    jogos = tempfile.mkdtemp(prefix="aoen-jogos-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")], check=True, stdout=subprocess.DEVNULL)
    resultados = []
    for res in os.environ.get("H389_RES", "800x600,1024x768").split(","):
        img = imagem_de_teste("h389-" + res, ["--vesa", "--resolucao", res, "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
        with Maquina("h389-bench-" + res, "ide", imagem=img, memoria="256M") as m:
            esperar(m, r"sagui:/\$", 150)
            time.sleep(4)
            h136.focar_terminal(m)
            for ef in os.environ.get("H389_EFECTOS", "0,1").split(","):
                marca = len(m.serial())
                m.digitar("AOEN_FULL=1 AOEN_SCENE=game AOEN_EFECTOS=%s AOEN_RENDER=%s AOEN_BENCH=%s /apps/bin/aoen" % (ef, os.environ.get("H389_RENDER", "0"), os.environ.get("H389_BENCH", "600")), 0.04)
                m.tecla("ret")
                g = esperar(m, r"\[AOEN\] bench (\d+)x(\d+) cuadros=(\d+) ms=(\d+) fps=([\d.]+)", 300, desde=marca)
                print("    %s efectos=%s -> bench %sx%s: %s cuadros en %s ms = %s fps" % (res, ef, g[0], g[1], g[2], g[3], g[4]))
                resultados.append((res, ef, float(g[4])))
                time.sleep(2)
                if ef == os.environ.get("H389_EFECTOS", "0,1").split(",")[-1]:
                    m.captura("H389-bench-" + res)
    verificar(all(r[2] > 0 for r in resultados), "todas las medidas dan fps > 0")


if __name__ == "__main__":
    partes = sys.argv[1:] or ["generador", "licencia", "lectores", "sin-datos", "con-datos", "efectos"]  # "bench" se pide aparte
    for p in partes:
        print("== " + p)
        {"generador": generador, "licencia": licencia, "sin-datos": sin_datos, "con-datos": con_datos, "efectos": efectos, "lectores": lectores, "bench": bench}[p]()
    print("H389 ok")
