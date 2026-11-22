#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H417 - Tzar: lectores de datos traducidos de ReTzar y el visor de mapas «tzar» (D417, docs/tzar.md).

Partes (por defecto: git lectores ui simulacao extras campanhas ia cuelgue pc saguios; «privado» y «privado_ia» aparte y solo a mano):
  git       ningún dato de Tzar en git (paquetes .wdt, mapas .wmp, campañas .wcm, el Tzar.exe) y cada fichero
            traducido cita a ReTzar y su licencia.
  lectores  en el PC con ASan/UBSan (tests/tzar_lectores.c) sobre una instalación SINTÉTICA de tools/tzar_sintetico.py:
            LZSS por trozos (literales, referencias, ceros, trozo crudo, truncado), HMMSYS con prefijos, carpetas sueltas,
            orden de los paquetes, INI, BMP de 8/24 bits con el color del reino, RLE de los formatos 3 y 7, HPFS con
            bloques desordenados, indirección y mapas anidados, mapa, terreno con transiciones, minimapa y escena; y que
            lo dañado (LZSS truncado, paleta, tramo RLE, ciclo HPFS, ruta «..») se rechaza.
  ui        la interfaz (fase 4: fuentes FNT, HUD, selección, órdenes, colocación de edificios y menús) en el PC con
            ASan/UBSan (tests/tzar_ui.c) sobre un mundo de prueba (tests/tzar_ui_stub.c) y la letra del sistema, sin datos
            del juego: ratón y teclado simulados, comandos que salen, avisos de eventos y las pantallas a 800x600-1920x1080.
  simulacao la simulación (fase 5) sin cabeza en el PC con ASan/UBSan (tests/tzar_simulacao.c) sobre prueba/sim.wmp
            sintético: 3000 tics con el jugador de prueba (demo.c), economía y combate, determinismo y guardar/cargar.
  extras    la simulación de la v8.5.5.2 en el PC con ASan/UBSan (tests/tzar_extras.c, datos sintéticos): comercio, taberna, préstamos,
            objetos, hechizos pendientes, espías, cruzada, experiencia, prioridad de blancos, determinismo y guardar/cargar.
  campanhas campañas, guiones, mapas aleatorios y sonido (fase 7) en el PC con ASan/UBSan (tests/tzar_campanas.c) contra un
            mundo falso (tests/tzar_mundo_falso.c) y una instalación sintética (tools/tzar_sintetico.py --campanhas): el lenguaje
            de expresiones de los guiones, reglas, ventanas, temporizadores, conversaciones, final, héroes y variables «#» que
            pasan de una misión a otra, guardar y cargar, registros dañados, catálogo y progreso de campañas, mapas aleatorios
            (castillos, minas, determinismo) y la mezcladora de sonido (atenuación, huecos, habla, película, eventos) con el
            sintetizador MIDI del sistema.
  ia        la IA original (fase 6): el compilador de guiones con casos sintéticos y una partida IA contra IA con la
            simulación real sobre la instalación sintética de tools/tzar_ia_sintetico.py (ASan/UBSan): sin fallos,
            determinista, guardar/cargar igual, y las dos IA construyen, entrenan, investigan y pelean.
  privado_ia (a mano) los guiones de IA reales compilan y partidas IA contra IA en mapas reales.
  cuelgue   el «cuelgue» de las campañas (v8.5.5.2): una frase de conversación sin elecciones no se cerraba con un clic
            (tz_guiao_fechar no cierra conversaciones) y la partida se quedaba parada para siempre con la frase en pantalla y la
            música sonando. Con una campaña sintética que abre una historia y una conversación al empezar, el juego del PC cierra
            las ventanas como lo haría un jugador (TZAR_AUTO_FECHAR) y el mundo sigue; y el vigilante del bucle (vigia.c) avisa,
            con el módulo, cuando el bucle se para (TZAR_PRUEBA_PARADA) y cuando vuelve.
  pc        el visor en el PC (SDL sin pantalla) con los datos sintéticos y sin datos (pantalla de instrucciones), y
            300 tics de partida (TZAR_JOGAR, TZAR_DEMO) con su suma.
  saguios   el visor en SaguiOS (QEMU) con los datos sintéticos en /jogos/TZAR (captura H417-1), sin datos (H417-2) y la
            misma partida que en el PC con la misma suma (H417-3).
  privado   (solo a mano, nunca en la batería) con la instalación real del mantenedor, si existe: abre todos los paquetes,
            imágenes, mapas y campañas y cuenta fallos, interpreta los guiones de las 71 misiones, lee los 438 WAV y los 14 MIDI y
            genera mapas aleatorios (tests/tzar_campanas.c privado y real), y deja capturas del visor y de la interfaz (ui-*.png: menús y HUD
            con el arte, las fuentes y los textos del juego) en /home/aanun/SO/private/tzar/capturas/.
Los datos de Tzar nunca se usan en la batería ni en capturas públicas.
"""
import io
import os
import re
import struct
import subprocess
import sys
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
D0 = os.path.join(RAIZ, "userland", "juegos", "tzar")
LECTORES = ["wdt.c", "hpfs.c", "recursos.c", "imagem.c", "mapa.c", "terreno.c", "escena.c", "animacao.c", "dibujo.c", "textos.c", "selecao.c"]
# interfaz (fase 4): los .c con partes traducidas de ReTzar citan su origen y licencia; los otros son código propio
UI_TRADUCIDOS = ["ui_fonte.c", "ui_skin.c", "ui_hud.c", "ui_jogo.c", "ui_menu.c", "ui_selecao.c"]
UI_FUENTES = ["ui_fonte.c", "ui_hud.c", "ui_jogo.c", "ui_lienzo.c", "ui_mapa.c", "ui_menu.c", "ui_selecao.c", "ui_skin.c", "ui_janelas.c",
              "ui_cursor.c", "opcoes.c"]
# simulación (fase 5): traducida de ReTzar; demo.c y partida.c son propios
SIM_TRADUCIDOS = ["catalogo.c", "mundo.c", "acoes.c", "caminho.c", "gravar.c", "magia.c", "comercio.c", "itens.c"]
SIM_FUENTES = SIM_TRADUCIDOS + ["demo.c"]
CAMPANHAS_TRADUCIDOS = ["guiao.c", "guiao_expr.c", "guiao_ler.c", "guiao_gravar.c", "campanha.c", "aleatorio.c", "som.c", "musica.c", "intro.c"]
CAMPANHAS_FUENTES = ["guiao.c", "guiao_expr.c", "guiao_ler.c", "guiao_gravar.c", "campanha.c", "aleatorio.c", "som.c", "musica.c", "wdt.c", "hpfs.c", "recursos.c", "imagem.c", "mapa.c"]
REAL = os.environ.get("TZAR_REAL", "/home/aanun/SO/sagui-os-main/input_share/Tzar Anthology")
PRIVADO = "/home/aanun/SO/private/tzar/capturas"


def saida():
    from qemu import SAIDA
    os.makedirs(SAIDA, exist_ok=True)
    return SAIDA


def sintetico():
    d = os.path.join(tempfile.mkdtemp(prefix="tzar-sint-"), "TZAR")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "tzar_sintetico.py"), d], check=True, stdout=subprocess.DEVNULL)
    return d


def compilar_lectores():
    exe = os.path.join(saida(), "tzar_lectores")
    subprocess.run(["gcc", "-std=gnu99", "-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-Wall", "-Wno-format-truncation",
                    "-I", D0, "-o", exe, os.path.join(RAIZ, "tests", "tzar_lectores.c")] + [os.path.join(D0, f) for f in LECTORES], check=True)
    return exe


def compilar_visor():
    s = saida()
    fonte = os.path.join(s, "tzar_fonte.c")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkfonte.py"), os.path.join(RAIZ, "tools", "fonte", "sagui8x16.txt"), fonte],
                   check=True, stdout=subprocess.DEVNULL)
    cf = subprocess.run(["sdl2-config", "--cflags"], capture_output=True, text=True).stdout.split()
    lb = subprocess.run(["sdl2-config", "--libs"], capture_output=True, text=True).stdout.split()
    exe = os.path.join(s, "tzar_pc")
    # todo junto (también la interfaz, aunque el visor aún no la use): así se comprueba que enlaza como en SaguiOS
    fuentes = [os.path.join(D0, f) for f in sorted(os.listdir(D0)) if f.endswith(".c")]
    fuentes += [os.path.join(D0, "ia", f) for f in sorted(os.listdir(os.path.join(D0, "ia"))) if f.endswith(".c")]
    subprocess.run(["gcc", "-std=gnu99", "-O2", "-Wall", "-Wno-format-truncation", "-finput-charset=UTF-8", "-fexec-charset=ISO-8859-1", "-I", D0] + cf +
                   ["-o", exe] + fuentes + [fonte] + lb + ["-lm"], check=True)
    return exe


def bmp_pixels(ruta):
    """(ancho, alto, función (x, y) -> (r, g, b)) de un BMP de 24 o 32 bits."""
    d = open(ruta, "rb").read()
    off = struct.unpack_from("<I", d, 10)[0]
    w, h = struct.unpack_from("<ii", d, 18)
    bpp = struct.unpack_from("<H", d, 28)[0] // 8
    stride = (w * bpp + 3) // 4 * 4

    def px(x, y):
        sy = abs(h) - 1 - y if h > 0 else y
        p = off + sy * stride + x * bpp
        return d[p + 2], d[p + 1], d[p]
    return w, abs(h), px


def bmp_a_png(src, dst):
    import zlib
    w, h, px = bmp_pixels(src)
    raw = bytearray()
    for y in range(h):
        raw.append(0)
        for x in range(w):
            raw += bytes(px(x, y))

    def ch(t, b):
        return struct.pack(">I", len(b)) + t + b + struct.pack(">I", zlib.crc32(t + b) & 0xffffffff)
    with open(dst, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) + ch(b"IDAT", zlib.compress(bytes(raw), 6)) + ch(b"IEND", b""))


# ---------------------------------------------------------------- partes

def git():
    from qemu import verificar
    ls = subprocess.run(["git", "ls-files"], cwd=RAIZ, capture_output=True, text=True).stdout.splitlines()
    malos = [f for f in ls if re.search(r"\.(wdt|wmp|wcm|wdm)$", f, re.I) or re.search(r"(^|/)tzar(edit)?\.exe$", f, re.I) or f.lower().startswith("input_share/")]
    verificar(not malos, "ningún dato de Tzar en git (%s)" % (", ".join(malos[:5]) or "ninguno"))
    sin = []
    for f in LECTORES:
        t = open(os.path.join(D0, f), encoding="utf-8").read()
        if "ReTzar" not in t or "GPL-3.0-or-later" not in t:
            sin.append(f)
    verificar(not sin, "cada fichero traducido cita a ReTzar y la GPL-3.0-or-later (%s)" % (", ".join(sin) or "todos"))
    sin = []
    for f in UI_TRADUCIDOS + SIM_TRADUCIDOS:
        t = open(os.path.join(D0, f), encoding="utf-8").read()
        if "ReTzar" not in t or "GPL-3.0-or-later" not in t:
            sin.append(f)
    verificar(not sin, "la interfaz cita a ReTzar y la GPL-3.0-or-later en lo traducido (%s)" % (", ".join(sin) or "todos"))
    sin = []
    for f in CAMPANHAS_TRADUCIDOS:
        t = open(os.path.join(D0, f), encoding="utf-8").read()
        if "ReTzar" not in t or "GPL-3.0-or-later" not in t:
            sin.append(f)
    verificar(not sin, "guiones, campañas, mapas aleatorios y sonido citan a ReTzar y la GPL-3.0-or-later (%s)" % (", ".join(sin) or "todos"))
    sin = []
    for f in sorted(os.listdir(os.path.join(D0, "ia"))):
        t = open(os.path.join(D0, "ia", f), encoding="utf-8").read()
        if "ReTzar" not in t or "GPL-3.0-or-later" not in t:
            sin.append(f)
    verificar(not sin, "la IA cita a ReTzar y la GPL-3.0-or-later (%s)" % (", ".join(sin) or "todos"))
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("ReTzar" in t and "D417" in t, "ReTzar figura en docs/terceros.md")


def lectores():
    from qemu import verificar
    exe = compilar_lectores()
    d = sintetico()
    r = subprocess.run([exe, "sintetico", d], capture_output=True, text=True, errors="replace", env=dict(os.environ, ASAN_OPTIONS="detect_leaks=1"))
    for l in r.stdout.splitlines():
        if l.startswith("FALLO"):
            print("   ", l)
    oks = len(re.findall(r"^OK ", r.stdout, re.M))
    verificar(r.returncode == 0 and "OK sintético: 0 fallos" in r.stdout and oks >= 40,
              "lectores con datos sintéticos, ASan/UBSan y sin fugas: %d comprobaciones bien%s" % (oks, "" if r.returncode == 0 else "\n" + r.stdout[-2000:] + r.stderr[-2000:]))


def compilar_ui():
    s = saida()
    fonte = os.path.join(s, "tzar_fonte.c")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkfonte.py"), os.path.join(RAIZ, "tools", "fonte", "sagui8x16.txt"), fonte],
                   check=True, stdout=subprocess.DEVNULL)
    exe = os.path.join(s, "tzar_ui")
    fuentes = [os.path.join(D0, f) for f in UI_FUENTES + LECTORES]
    subprocess.run(["gcc", "-std=gnu99", "-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-Wall", "-Wno-format-truncation",
                    "-I", D0, "-I", os.path.join(RAIZ, "tests"), "-o", exe, os.path.join(RAIZ, "tests", "tzar_ui.c"),
                    os.path.join(RAIZ, "tests", "tzar_ui_stub.c")] + fuentes + [fonte, "-lm"], check=True)
    return exe


def ui():
    from qemu import verificar
    exe = compilar_ui()
    r = subprocess.run([exe, "unidad", saida()], capture_output=True, text=True, errors="replace", env=dict(os.environ, ASAN_OPTIONS="detect_leaks=1"))
    for l in r.stdout.splitlines():
        if l.startswith("FALLO"):
            print("   ", l)
    oks = len(re.findall(r"^OK ", r.stdout, re.M))
    verificar(r.returncode == 0 and "OK: 0 fallos" in r.stdout and oks >= 140,
              "interfaz con datos sintéticos, ASan/UBSan y sin fugas: %d comprobaciones bien%s" % (oks, "" if r.returncode == 0 else "\n" + r.stdout[-2500:] + r.stderr[-2500:]))
    # captura sintética: el HUD sin arte (marco liso) y el menú principal sin fondo, solo con la letra del sistema
    for src, dst in (("/tmp/tzar_ui_sint_1024.bmp", "H417-ui-hud.png"), ("/tmp/tzar_ui_sint_menu.bmp", "H417-ui-menu.png")):
        if os.path.exists(src):
            bmp_a_png(src, os.path.join(saida(), dst))


def compilar_simulacao():
    exe = os.path.join(saida(), "tzar_simulacao")
    fuentes = [os.path.join(D0, f) for f in LECTORES + SIM_FUENTES]
    subprocess.run(["gcc", "-std=gnu99", "-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-Wall", "-Wno-format-truncation",
                    "-I", D0, "-o", exe, os.path.join(RAIZ, "tests", "tzar_simulacao.c")] + fuentes + ["-lm"], check=True)
    return exe


def simulacao():
    """La simulación sin cabeza con el mapa sintético prueba/sim.wmp: el jugador de prueba (demo.c) en los dos reinos,
    3000 tics (2,5 minutos de juego), con ASan/UBSan y sin fugas; determinismo y guardar/cargar a mitad."""
    from qemu import verificar
    exe = compilar_simulacao()
    d = sintetico()
    sav = os.path.join(saida(), "tzar_sim.sav")
    r = subprocess.run([exe, d, "prueba/sim.wmp", "3000", "--gravar", sav], capture_output=True, text=True, errors="replace",
                       env=dict(os.environ, ASAN_OPTIONS="detect_leaks=1"), timeout=600)
    out = r.stdout
    for l in out.splitlines():
        print("   ", l)
    verificar(r.returncode == 0 and "las dos partidas dan las mismas sumas" in out, "simulación determinista: la misma partida dos veces da las mismas sumas" +
              ("" if r.returncode == 0 else "\n" + out[-1500:] + r.stderr[-2500:]))
    verificar(re.search(r"guardar/cargar en el tic 1500: .*: igual", out) is not None, "guardar a mitad, cargar y seguir da la misma suma final")
    g = re.search(r"\[SIM\] reino 1: recursos .* creadas (\d+) perdidas \d+ matadas (\d+) recogido (\d+)/\d+/\d+/(\d+)", out)
    verificar(g and int(g.group(1)) > 0 and int(g.group(3)) > 0 and int(g.group(4)) > 0,
              "el reino 1 entrena, mina oro y tala (creadas %s, oro %s, madera %s)" % (g.groups()[0], g.group(3), g.group(4)) if g else "falta el resumen del reino 1")
    verificar(g and int(g.group(2)) > 0, "hay combate: el reino 1 mata (%s)" % (g.group(2) if g else "?"))


def compilar_extras():
    exe = os.path.join(saida(), "tzar_extras")
    fuentes = [os.path.join(D0, f) for f in LECTORES + SIM_TRADUCIDOS]
    subprocess.run(["gcc", "-std=gnu99", "-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-Wall", "-Wno-format-truncation",
                    "-I", D0, "-o", exe, os.path.join(RAIZ, "tests", "tzar_extras.c")] + fuentes + ["-lm"], check=True)
    return exe


def extras():
    """La simulación de la v8.5.5.2 (tests/tzar_extras.c) con ASan/UBSan y datos sintéticos: rutas de mercaderes y su ganancia, un
    mercader por mercado, intercambio, préstamos, ofertas y contratos de la taberna, objetos (recoger, soltar, usar, vender, equipo,
    libros, soltar al morir), maldición, hipnosis, invisibilidad, remolino, alzar muertos, espías, golpe del ninja, cruzada,
    conversiones, experiencia y niveles, prioridad de blancos; determinismo y guardar/cargar con todo eso en marcha."""
    from qemu import verificar
    exe = compilar_extras()
    d = sintetico()
    r = subprocess.run([exe, d], capture_output=True, text=True, errors="replace", env=dict(os.environ, ASAN_OPTIONS="detect_leaks=1"), timeout=900)
    for l in r.stdout.splitlines():
        if l.startswith("FALLO"):
            print("   ", l)
    oks = len(re.findall(r"^OK ", r.stdout, re.M))
    verificar(r.returncode == 0 and "0 fallos" in r.stdout and oks >= 100,
              "simulación nueva (comercio, objetos, taberna, hechizos, espías, cruzada, experiencia): %d comprobaciones bien%s" % (oks, "" if r.returncode == 0 else "\n" + r.stdout[-2000:] + r.stderr[-2500:]))


def suma_visor(exe, d, frames):
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", TZAR_FRAMES=str(frames), TZAR_W="640", TZAR_H="480", TZAR_JOGAR="1", TZAR_DEMO="1", TZAR_SEM_IA="1", TZAR_MS="0")
    r = subprocess.run([exe, "--visor", d, "prueba/sim.wmp"], capture_output=True, text=True, env=env, timeout=120)
    g = re.search(r"\[TZAR\] partida: tic (\d+) suma ([0-9a-f]{8})", r.stderr)
    return (g.group(1), g.group(2)) if g else None, r.stderr


def compilar_campanhas():
    """El arnés de la fase 7 (con el mundo falso) y el sintetizador MIDI de la libsagui del anfitrión (sin UBSan: lo suyo no es de aquí)."""
    s = saida()
    midi_o = os.path.join(s, "tzar_audio_midi.o")
    base = ["-std=gnu99", "-DTZ_MIDI", "-DAUDIO_HOST", "-I", os.path.join(RAIZ, "tools"), "-I", os.path.join(RAIZ, "userland", "inc"),
            "-I", os.path.join(RAIZ, "userland", "libsagui"), "-I", D0]
    subprocess.run(["gcc", "-c", "-O1"] + base + ["-o", midi_o, os.path.join(RAIZ, "userland", "libsagui", "audio_midi.c")], check=True)
    cf = subprocess.run(["sdl2-config", "--cflags"], capture_output=True, text=True).stdout.split()
    lb = subprocess.run(["sdl2-config", "--libs"], capture_output=True, text=True).stdout.split()
    exe = os.path.join(s, "tzar_campanas")
    subprocess.run(["gcc", "-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-Wall", "-Wno-format-truncation"] + base + cf +
                   ["-o", exe, os.path.join(RAIZ, "tests", "tzar_campanas.c"), os.path.join(RAIZ, "tests", "tzar_mundo_falso.c")] +
                   [os.path.join(D0, f) for f in CAMPANHAS_FUENTES] + [midi_o] + lb + ["-lm"], check=True)
    return exe


def campanhas():
    from qemu import verificar
    exe = compilar_campanhas()
    d = os.path.join(tempfile.mkdtemp(prefix="tzar-camp-"), "TZAR")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "tzar_sintetico.py"), "--campanhas", d], check=True, stdout=subprocess.DEVNULL)
    r = subprocess.run([exe, "sintetico", d], capture_output=True, text=True, errors="replace", env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"), timeout=600)
    for l in r.stdout.splitlines():
        if l.startswith("FALLO"):
            print("   ", l)
    oks = len(re.findall(r"^OK ", r.stdout, re.M))
    verificar(r.returncode == 0 and "OK sintético: 0 fallos" in r.stdout and oks >= 140,
              "campañas, guiones, mapas aleatorios y sonido con datos sintéticos, ASan/UBSan: %d comprobaciones bien%s" % (oks, "" if r.returncode == 0 else "\n" + r.stdout[-2500:] + r.stderr[-2500:]))


def pc():
    from qemu import verificar
    exe = compilar_visor()
    d = sintetico()
    s = saida()
    shot = os.path.join(s, "tzar_pc.bmp")
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", TZAR_SHOT=shot, TZAR_FRAMES="3", TZAR_W="800", TZAR_H="600", TZAR_MS="0")
    if os.path.exists(shot):
        os.remove(shot)
    r = subprocess.run([exe, "--visor", d, "Maps/PRUEBA.wmp"], capture_output=True, text=True, env=env, timeout=60)
    log = r.stderr
    g = re.search(r"\[TZAR\] mapa Maps/PRUEBA.wmp: lado 32 bioma grass objetos 7 visuales (\d+) imagenes (\d+) sin dibujo (\d+) fallos (\d+)", log)
    verificar(r.returncode == 0 and g and g.group(1) == "18" and g.group(3) == "1" and g.group(4) == "0",
              "el visor abre el mapa sintético: 18 dibujos, 1 objeto sin definición, sin fallos" + ("" if g else "\n" + log[-1500:]))
    verificar("instalacion: 4 paquetes" in log and "2 mapas" in log, "encuentra los 4 paquetes y los 2 mapas (suelto y en la campaña)")
    w, h, px = bmp_pixels(shot)
    # (0,0)-(191,191) es agua; la casa del reino 1 tiene el tejado rojo en (12*32-16+20, 4*32-48+12)
    agua, tejado = px(60, 100), px(12 * 32 - 16 + 20, 4 * 32 - 48 + 12)
    verificar((w, h) == (800, 600) and agua == (32, 64, 192) and tejado == (180, 40, 30),
              "la captura tiene el agua y la casa en su sitio (%s %s)" % (agua, tejado))
    # el estandarte (gris 245) sale en el color 3 del reino 1: la rampa de índice 2 empieza en (0,128,0)
    est = px(12 * 32 - 16 + 40 + 6, 4 * 32 - 48 + 4)
    verificar(est[1] > 100 and est[0] < 60 and est[2] < 60, "el estandarte sale del color del reino (%s)" % (est,))
    # fase 3: el agua anima (tic = ms / 250: la segunda página es un poco más clara) y un clic selecciona con ficha
    shot2 = os.path.join(s, "tzar_pc_t.bmp")
    env2 = dict(env, TZAR_SHOT=shot2, TZAR_MS="250", TZAR_CLICK="%d,%d" % (4 * 32 + 16, 5 * 32 + 16))
    r2 = subprocess.run([exe, "--visor", d, "Maps/PRUEBA.wmp"], capture_output=True, text=True, env=env2, timeout=60)
    w2, h2, px2 = bmp_pixels(shot2)
    verificar(px2(60, 100) == (40, 70, 200) and px(60, 100) == (32, 64, 192), "el agua se anima: a los 250 ms la segunda página (%s)" % (px2(60, 100),))
    verificar("[TZAR] seleccion: objeto 0 peon" in r2.stderr, "un clic en el peón lo selecciona" + ("" if "seleccion" in r2.stderr else "\n" + r2.stderr[-800:]))
    # el marco blanco de esquinas de 6 píxeles rodea la caja del peón: sin `selection` es el dibujo, desde su punto (4*32, 5*32)
    ax, ay = 4 * 32, 5 * 32
    verificar(px2(ax, ay) == (255, 255, 255) and px2(ax + 5, ay) == (255, 255, 255), "marco de selección en la esquina de la caja (%s)" % (px2(ax, ay),))
    # fase 5: la partida en el visor (un tic por cuadro, jugador de prueba en los dos reinos)
    sm, log = suma_visor(exe, d, 300)
    verificar(sm is not None and sm[0] == "300", "el visor juega 300 tics de prueba/sim.wmp (suma %s)" % (sm[1] if sm else "\n" + log[-1200:]))
    # el juego completo (jogo.c): el menú y una escaramuza (castillo y tres peones por reino) con los datos sintéticos
    gj = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", TZAR_FRAMES="40", TZAR_GUARDAR=tempfile.mkdtemp(prefix="tzar-g-"))
    rj = subprocess.run([exe, d], capture_output=True, text=True, errors="replace", env=gj, timeout=120)
    verificar(rj.returncode == 0 and "[TZAR] juego: ventana" in rj.stderr and "[TZAR] fin" in rj.stderr, "el juego arranca en el menú y sale" + ("" if rj.returncode == 0 else "\n" + rj.stderr[-1500:]))
    rj = subprocess.run([exe, d], capture_output=True, text=True, errors="replace", env=dict(gj, TZAR_AUTO="escaramuza", TZAR_FRAMES="200", TZAR_SEM_IA="1"), timeout=120)
    ge = re.search(r"\[TZAR\] partida: tic (\d+) suma [0-9a-f]{8} objetos (\d+)", rj.stderr)
    verificar(rj.returncode == 0 and "escaramuza:" in rj.stderr and ge and ge.group(1) == "200", "escaramuza en el juego: 200 tics" + ("" if ge else "\n" + rj.stderr[-1500:]))
    # v8.5.5.2: el mapa aleatorio con las opciones de su pantalla (96 casillas, 4 reinos, equipos, civilización, continente) dentro del juego,
    # y guardar, cargar y recordar las opciones (tzar.cfg)
    lin = "ALEAT2 1 1 3 1 1 1 1 1 417 4 0 0 200 1 1 0 0 0 2 0 0 0 0 0 0 0 1 1 2 0 0 0 0"
    pg = tempfile.mkdtemp(prefix="tzar-g-")
    rj = subprocess.run([exe, d], capture_output=True, text=True, errors="replace",
                        env=dict(gj, TZAR_GUARDAR=pg, TZAR_AUTO="aleatorio", TZAR_ALEAT=lin, TZAR_PRUEBA_GUARDAR="1", TZAR_FRAMES="60", TZAR_SEM_IA="1"), timeout=120)
    verificar(rj.returncode == 0 and re.search(r"escaramuza: mapa aleatorio, 4 reinos, jugador \d+, lado 96", rj.stderr),
              "el mapa aleatorio del juego sale con los reinos y el tamaño elegidos" + ("" if rj.returncode == 0 else "\n" + rj.stderr[-1500:]))
    verificar("[TZAR] prueba guardar/cargar aleatorio: igual" in rj.stderr, "guardar y cargar la partida en mapa aleatorio da el mismo mapa y las mismas opciones" + ("" if "igual" in rj.stderr else "\n" + rj.stderr[-1500:]))
    txt = io.open(os.path.join(pg, "prueba.txt"), encoding="latin-1").read().splitlines() if os.path.exists(os.path.join(pg, "prueba.txt")) else []
    verificar(len(txt) >= 7 and txt[6] == "ALEAT 96 4 0 3 1 1 1 1 grass 417 0 1 1 2 0 0 0 0 0 2 0 0 0 0 0 0 1 1",
              "la partida guardada lleva la línea ALEAT con tamaño, reinos, tipo y equipos (%s)" % (txt[6][:70] if len(txt) > 6 else "falta"))
    r = subprocess.run([exe, "--visor", "/no/existe"], capture_output=True, text=True, env=env, timeout=60)
    verificar(r.returncode == 1 and "[TZAR] sin datos del juego" in r.stderr, "sin datos explica qué falta y sale con 1")
    w, h, px = bmp_pixels(shot)
    verificar(px(5, 5) == (0x14, 0x0d, 0x09), "la pantalla de instrucciones se dibuja")


def instalacion_juego():
    """La instalación sintética con gráficos más la campaña con guion (historia y conversación al empezar) de --campanhas."""
    import shutil
    d = sintetico()
    c = os.path.join(tempfile.mkdtemp(prefix="tzar-camp-"), "TZAR")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "tzar_sintetico.py"), "--campanhas", c], check=True, stdout=subprocess.DEVNULL)
    shutil.copy(os.path.join(c, "Campaign", "HISTORIA.wcm"), os.path.join(d, "Campaign", "HISTORIA.wcm"))
    return d


def cuelgue():
    from qemu import verificar
    exe = compilar_visor()
    d = instalacion_juego()
    base = dict(os.environ, SDL_VIDEODRIVER="dummy", SDL_AUDIODRIVER="dummy", TZAR_SEM_INTRO="1", TZAR_SEM_IA="1",
                TZAR_GUARDAR=tempfile.mkdtemp(prefix="tzar-g-"))
    # 1) la conversación del principio se cierra y la partida sigue (antes: tic 4 para siempre)
    r = subprocess.run([exe, d], capture_output=True, text=True, errors="replace", timeout=180,
                       env=dict(base, TZAR_AUTO="campanha:0", TZAR_AUTO_FECHAR="5", TZAR_FRAMES="300"))
    g = re.search(r"\[TZAR\] partida: tic (\d+)", r.stderr)
    cerradas = r.stderr.count("[TZAR] guion: ventana cerrada")
    verificar(r.returncode == 0 and "misi" in r.stderr and g and int(g.group(1)) >= 250 and 2 <= cerradas <= 4,
              "la historia y la frase de la conversación se cierran y el mundo sigue (tic %s, %d ventanas)" % (g.group(1) if g else "?", cerradas)
              + ("" if g else "\n" + r.stderr[-1500:]))
    # 2) el vigilante: el bucle se para 2,5 s en el cuadro 40 (vigilante a 1 s) -> aviso con el módulo y vuelta
    r = subprocess.run([exe, d], capture_output=True, text=True, errors="replace", timeout=180,
                       env=dict(base, TZAR_AUTO="escaramuza", TZAR_FRAMES="80", TZAR_VIGIA="1", TZAR_PRUEBA_PARADA="40:2500"))
    av = re.search(r"\[TZAR\] vigilante: el juego no avanza desde hace (\d+) ms \(en: ([^;]*);", r.stderr)
    verificar(r.returncode == 0 and av and "prueba de parada" in av.group(2) and int(av.group(1)) >= 1000,
              "el vigilante avisa del bucle parado y dice dónde (%s)" % (av.group(2) if av else "\n" + r.stderr[-1200:]))
    verificar(re.search(r"\[TZAR\] vigilante: el juego vuelve tras \d+ ms", r.stderr) is not None and "vigilante: 1 avisos" in r.stderr,
              "y apunta cuando vuelve (un aviso)")
    # 3) sin paradas no avisa
    r = subprocess.run([exe, d], capture_output=True, text=True, errors="replace", timeout=180,
                       env=dict(base, TZAR_AUTO="escaramuza", TZAR_FRAMES="80", TZAR_VIGIA="1"))
    verificar(r.returncode == 0 and "vigilante" not in r.stderr, "sin paradas el vigilante calla")


def saguios():
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "tzar")):
        print("AVISO: la imagen no lleva tzar (make): se salta la parte «saguios»")
        return
    d = instalacion_juego()
    jogos = os.path.dirname(d)
    img = imagem_de_teste("h417", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "32"])
    with Maquina("h417", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        h136.focar_terminal(m)
        marca = len(m.serial())
        m.digitar("TZAR_SHOT=/tmp/tzar.bmp TZAR_FRAMES=400 TZAR_W=800 TZAR_H=560 /apps/bin/tzar --visor", 0.04)
        m.tecla("ret")
        esperar(m, r"\[TZAR\] inicio: ventana 800x560, datos /jogos/TZAR", 90, desde=marca)
        verificar(True, "tzar encuentra los datos en /jogos/TZAR")
        g = esperar(m, r"\[TZAR\] mapa Maps/PRUEBA.wmp: lado 32 bioma grass objetos 7 visuales (\d+) imagenes \d+ sin dibujo (\d+) fallos (\d+)", 120, desde=marca)
        verificar(g[0] == "18" and g[1] == "1" and g[2] == "0", "abre el primer mapa con sus 18 dibujos y sin fallos")
        time.sleep(3)
        m.captura("H417-1-visor-sintetico")
        esperar(m, r"\[TZAR\] captura guardada en el cuadro 400", 300, desde=marca)
        esperar(m, r"sagui:/\$", 60, desde=marca)
        time.sleep(2)
        h136.focar_terminal(m)
        marca = len(m.serial())
        m.digitar("TZAR_SHOT=/tmp/tzar2.bmp TZAR_FRAMES=150 TZAR_W=800 TZAR_H=560 /apps/bin/tzar --visor /no/existe", 0.04)
        m.tecla("ret")
        esperar(m, r"\[TZAR\] sin datos del juego: no existe la carpeta /no/existe", 90, desde=marca)
        time.sleep(2)
        m.captura("H417-2-sin-datos")
        verificar(True, "sin datos, el visor explica dónde ponerlos")
        esperar(m, r"\[TZAR\] captura guardada", 300, desde=marca)
        # fase 5: la misma partida que en el PC da la misma suma (determinismo entre máquinas)
        sm, _ = suma_visor(compilar_visor(), d, 600)
        esperar(m, r"sagui:/\$", 60, desde=marca)
        time.sleep(2)
        h136.focar_terminal(m)
        marca = len(m.serial())
        m.digitar("TZAR_JOGAR=1 TZAR_DEMO=1 TZAR_SEM_IA=1 TZAR_MS=0 TZAR_FRAMES=600 TZAR_W=640 TZAR_H=480 /apps/bin/tzar --visor /jogos/TZAR prueba/sim.wmp", 0.04)
        m.tecla("ret")
        esperar(m, r"\[TZAR\] partida: jugador 1", 120, desde=marca)
        time.sleep(3)
        m.captura("H417-3-partida-sintetica")
        g = esperar(m, r"\[TZAR\] partida: tic (\d+) suma ([0-9a-f]{8})", 400, desde=marca)
        verificar(sm is not None and g[0] == "600" and g[1] == sm[1], "la partida da la misma suma en SaguiOS que en el PC (%s / %s)" % (g[1], sm[1] if sm else "?"))
        # el cuelgue de las campañas, dentro de SaguiOS: la conversación del principio se cierra y la partida sigue; el vigilante no salta
        esperar(m, r"sagui:/\$", 60, desde=marca)
        time.sleep(2)
        h136.focar_terminal(m)
        marca = len(m.serial())
        m.digitar("TZAR_SEM_INTRO=1 TZAR_SEM_IA=1 TZAR_AUTO=campanha:0 TZAR_AUTO_FECHAR=5 TZAR_FRAMES=300 TZAR_W=800 TZAR_H=600 /apps/bin/tzar", 0.04)
        m.tecla("ret")
        g = esperar(m, r"\[TZAR\] partida: tic (\d+)", 400, desde=marca)
        log = m.serial()[marca:]
        verificar(int(g) >= 250 and "guion: ventana cerrada" in log and "no avanza" not in log,
                  "en SaguiOS la conversación se cierra y la partida sigue (tic %s)" % g)


def compilar_ia():
    s = saida()
    prova = os.path.join(s, "tzar_ia_prova")
    ia = sorted(os.path.join(D0, "ia", f) for f in os.listdir(os.path.join(D0, "ia")) if f.endswith(".c"))
    flags = ["gcc", "-std=gnu99", "-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-Wall", "-Wno-format-truncation", "-I", D0]
    subprocess.run(flags + ["-o", prova, os.path.join(RAIZ, "tests", "tzar_ia_prova.c"), os.path.join(D0, "ia", "ia_guiao.c")] +
                   [os.path.join(D0, f) for f in ("wdt.c", "hpfs.c", "recursos.c", "mapa.c")], check=True)
    # la simulación con los lectores (sin el visor, la interfaz, el sonido ni los guiones, que traen la SDL)
    sim = [os.path.join(D0, f) for f in LECTORES + SIM_TRADUCIDOS]
    partida = os.path.join(s, "tzar_ia_partida")
    subprocess.run(flags + ["-o", partida, os.path.join(RAIZ, "tests", "tzar_ia_partida.c")] + sim + ia + ["-lm"], check=True)
    return prova, partida


def ia():
    """Fase 6: el compilador de guiones (unidad) y una partida IA contra IA con la simulación real y datos sintéticos."""
    from qemu import verificar
    prova, partida = compilar_ia()
    r = subprocess.run([prova], capture_output=True, text=True, errors="replace", env=dict(os.environ, ASAN_OPTIONS="detect_leaks=1"))
    m = re.search(r"tzar_ia_prova: (\d+) comprobaciones, (\d+) fallos", r.stdout)
    verificar(r.returncode == 0 and m is not None and m.group(2) == "0" and int(m.group(1)) >= 90,
              "compilador de guiones de IA: %s%s" % (m.group(0) if m else "?", "" if r.returncode == 0 else "\n" + r.stdout[-2000:] + r.stderr[-2000:]))
    d = tempfile.mkdtemp(prefix="tzar-ia-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "tzar_ia_sintetico.py"), d], check=True, stdout=subprocess.DEVNULL)
    r = subprocess.run([partida, d, "Maps/IA.wmp", "6000", "--gravar", os.path.join(d, "ia.sav")], capture_output=True, text=True,
                       errors="replace", env=dict(os.environ, ASAN_OPTIONS="detect_leaks=1"), timeout=900)
    out = r.stdout
    for l in out.splitlines():
        if l.startswith("[IA] final") or "determinismo" in l or "guardar/cargar" in l:
            print("   ", l)
    verificar(r.returncode == 0, "partida IA contra IA sin fallos (ASan/UBSan)%s" % ("" if r.returncode == 0 else "\n" + out[-2000:] + r.stderr[-3000:]))
    verificar("las dos partidas dan las mismas sumas" in out, "la IA es determinista: dos partidas iguales dan las mismas sumas")
    verificar(re.search(r"guardar/cargar en el tic \d+: .*: igual", out) is not None, "guardar y cargar con la IA sigue con la misma suma")
    cont = [tuple(map(int, x)) for x in re.findall(r"\[IA\] final ia \d+: miradas (\d+) comandos (\d+) obras (\d+) entrenadas (\d+) investigaciones (\d+) grupos (\d+)", out)]
    verificar(len(cont) == 2 and all(c[2] >= 3 and c[3] >= 5 and c[4] >= 1 for c in cont),
              "las dos IA construyen, entrenan e investigan: %s" % cont)
    verificar(any(c[5] >= 1 for c in cont) and re.search(r"matadas [1-9]", out) is not None, "la IA manda grupos y pelea")


def privado_ia():
    """A mano: los guiones de IA reales compilan y partidas IA contra IA en los mapas reales (nunca en la batería)."""
    from qemu import verificar
    if not os.path.isdir(REAL):
        print("AVISO: no está la instalación real (%s): se salta «privado_ia»" % REAL)
        return
    prova, partida = compilar_ia()
    r = subprocess.run([prova, "real", REAL], capture_output=True, text=True, errors="replace", env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))
    print("   ", r.stdout.strip().splitlines()[-1] if r.stdout.strip() else r.stderr[-500:])
    verificar(r.returncode == 0, "todos los guiones de IA reales compilan")
    for mapa in ("Maps/MP2.wmp", "Maps/MP3.wmp", "Maps/MP5.wmp"):
        sav = os.path.join(tempfile.mkdtemp(prefix="tzar-ia-real-"), "p.sav")
        r = subprocess.run([partida, REAL, mapa, "12000", "--castelo", "--gravar", sav], capture_output=True, text=True, errors="replace",
                           env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"), timeout=3000)
        for l in r.stdout.splitlines():
            if l.startswith("[IA] final") or "determinismo" in l or "guardar/cargar" in l:
                print("   ", l)
        verificar(r.returncode == 0, "%s: IA contra IA determinista y guardar/cargar igual" % mapa)


def privado():
    """A mano: los datos reales del mantenedor (nunca en la batería ni en capturas públicas)."""
    from qemu import verificar
    if not os.path.isdir(REAL):
        print("AVISO: no está la instalación real (%s): se salta «privado»" % REAL)
        return
    exe = compilar_lectores()
    r = subprocess.run([exe, "privado", REAL], capture_output=True, text=True, errors="replace", env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"))
    for l in r.stdout.splitlines():
        if not l.startswith("OK mapa"):
            print("   ", l)
    verificar(r.returncode == 0, "todos los paquetes, imágenes, mapas y campañas reales se leen sin fallos")
    os.makedirs(PRIVADO, exist_ok=True)
    vis = compilar_visor()
    for mapa, pos, nome in (("Maps/MP1.wmp", "20,20", "MP1"), ("Maps/MP4.wmp", "20,20", "MP4"),
                            ("Campaign/La Reconquista.wcm|MAPS/Barcelona.wmp", "60,60", "Barcelona"),
                            ("Campaign/La Reconquista.wcm|MAPS/Covadonga.wmp", "40,40", "Covadonga")):
        shot = os.path.join(saida(), "tzar_privado.bmp")
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", TZAR_SHOT=shot, TZAR_FRAMES="2", TZAR_POS=pos)
        r = subprocess.run([vis, "--visor", REAL, mapa], capture_output=True, text=True, errors="replace", env=env, timeout=120)
        print("   ", " ".join(l for l in r.stderr.splitlines() if "] mapa" in l))
        if os.path.exists(shot):
            bmp_a_png(shot, os.path.join(PRIVADO, "visor-%s.png" % nome))
    # fase 3: varios instantes del mismo punto (agua, costas, unidades, banderas) y una selección con ficha
    for mapa, pos, nome in (("Maps/MP1.wmp", "20,20", "mp1"), ("Campaign/La Reconquista.wcm|MAPS/Barcelona.wmp", "60,60", "barcelona")):
        for ms in (0, 250, 500, 750, 1000):
            shot = os.path.join(saida(), "tzar_privado.bmp")
            env = dict(os.environ, SDL_VIDEODRIVER="dummy", TZAR_SHOT=shot, TZAR_FRAMES="2", TZAR_POS=pos, TZAR_MS=str(ms))
            subprocess.run([vis, "--visor", REAL, mapa], capture_output=True, text=True, errors="replace", env=env, timeout=120)
            if os.path.exists(shot):
                bmp_a_png(shot, os.path.join(PRIVADO, "anim-%s-%04d.png" % (nome, ms)))
    shot = os.path.join(saida(), "tzar_privado.bmp")
    env = dict(os.environ, SDL_VIDEODRIVER="dummy", TZAR_SHOT=shot, TZAR_FRAMES="2", TZAR_POS="60,60", TZAR_MS="400", TZAR_CLICK="560,270", TZAR_BARRAS="1")
    r = subprocess.run([vis, "--visor", REAL, "Campaign/La Reconquista.wcm|MAPS/Barcelona.wmp"], capture_output=True, text=True, errors="replace", env=env, timeout=120)
    print("   ", " ".join(l for l in r.stderr.splitlines() if "seleccion" in l))
    if os.path.exists(shot):
        bmp_a_png(shot, os.path.join(PRIVADO, "anim-seleccion-barcelona.png"))
    # interfaz con el arte, las fuentes y los textos reales: menús y HUD
    ui_exe = compilar_ui()
    d = tempfile.mkdtemp(prefix="tzar-ui-")
    r = subprocess.run([ui_exe, "real", REAL, d], capture_output=True, text=True, errors="replace", env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"), timeout=900)
    for l in r.stdout.splitlines():
        if l.startswith("FALLO") or l.startswith("MP1"):
            print("   ", l)
    verificar(r.returncode == 0 and "OK captura" in r.stdout, "interfaz real: menús y HUD sin fallos")
    for f in sorted(os.listdir(d)):
        if f.endswith(".bmp"):
            bmp_a_png(os.path.join(d, f), os.path.join(PRIVADO, f[:-4] + ".png"))
    # capturas privadas de una historia y una conversación de tres campañas, sobre el inicio de la misión (tests/tzar_camp_captura.c)
    s = saida()
    fonte = os.path.join(s, "tzar_fonte.c")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "mkfonte.py"), os.path.join(RAIZ, "tools", "fonte", "sagui8x16.txt"), fonte], check=True, stdout=subprocess.DEVNULL)
    cap = os.path.join(s, "tzar_camp_captura")
    subprocess.run(["gcc", "-std=gnu99", "-O1", "-Wall", "-Wno-format-truncation", "-I", D0, "-o", cap, os.path.join(RAIZ, "tests", "tzar_camp_captura.c"),
                    os.path.join(RAIZ, "tests", "tzar_mundo_falso.c"), fonte] + [os.path.join(D0, f) for f in CAMPANHAS_FUENTES if f not in ("som.c", "musica.c", "aleatorio.c")], check=True)
    for camp, mision, nome in ((1, 0, "arturo"), (3, 0, "reconquista"), (4, 0, "dominios")):
        r = subprocess.run([cap, "pos", REAL, str(camp), str(mision)], capture_output=True, text=True)
        if r.returncode:
            continue
        pos, mapa = r.stdout.strip().splitlines()
        shot = os.path.join(s, "tzar_camp.bmp")
        env = dict(os.environ, SDL_VIDEODRIVER="dummy", TZAR_SHOT=shot, TZAR_FRAMES="2", TZAR_POS=pos, TZAR_W="1024", TZAR_H="720")
        subprocess.run([vis, "--visor", REAL, mapa], capture_output=True, text=True, errors="replace", env=env, timeout=120)
        for modo in ("historia", "conversa"):
            out = os.path.join(s, "tzar_camp_%s.bmp" % modo)
            if os.path.exists(shot) and subprocess.run([cap, modo, REAL, str(camp), str(mision), shot, out]).returncode == 0:
                bmp_a_png(out, os.path.join(PRIVADO, "camp-%s-%s.png" % (nome, modo)))
    # fase 7: los guiones de las 71 misiones, los sonidos y las músicas reales, los mapas aleatorios
    cx = compilar_campanhas()
    r = subprocess.run([cx, "privado", REAL], capture_output=True, text=True, errors="replace", env=dict(os.environ, ASAN_OPTIONS="detect_leaks=0"), timeout=1800)
    for l in r.stdout.splitlines():
        if not l.startswith("   ") and not l.startswith("campa"):
            print("   ", l)
    verificar(r.returncode == 0 and "OK privado:" in r.stdout and "OK sonido real:" in r.stdout, "campañas reales: guiones de todas las misiones, 438 WAV y 14 MIDI sin fallos")
    print("    capturas privadas en", PRIVADO)



def privado_saguios():
    """(a mano, nunca en la batería) El juego dentro de SaguiOS con la instalación REAL: imagen de prueba con los datos en
    /jogos/TZAR (TZAR_JOGOS=carpeta que contiene TZAR/, o se copia de REAL), 2 GB, SMP (con KVM: sg kvm -c "SAGUI_KVM=1 ...").
    Menú, escaramuza contra la IA unos minutos (el jugador de prueba lleva el reino propio) y la primera misión de una
    campaña con su historia. Capturas privadas en PRIVADO/saguios-*.png y los cuadros por segundo del registro."""
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    import shutil
    jogos = os.environ.get("TZAR_JOGOS")
    if not jogos:
        if not os.path.isdir(os.path.join(REAL, "Packs")):
            print("AVISO: no hay instalación real de Tzar: se salta «privado_saguios»")
            return
        jogos = tempfile.mkdtemp(prefix="tzar-jogos-")
        os.makedirs(os.path.join(jogos, "TZAR"))
        for x in ("Packs", "Maps", "Campaign"):
            shutil.copytree(os.path.join(REAL, x), os.path.join(jogos, "TZAR", x))
    os.makedirs(PRIVADO, exist_ok=True)
    img = imagem_de_teste("h417p", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "700", "--smp"])
    def cap(m, nome):
        src = m.captura("tzar-" + nome, guardar=False)
        shutil.move(src, os.path.join(PRIVADO, "saguios-%s.png" % nome))
    with Maquina("h417p", "ide", imagem=img, memoria="2048M") as m:
        esperar(m, r"sagui:/\$", 240)
        time.sleep(4)
        h136.focar_terminal(m)
        # 1) menú
        marca = len(m.serial())
        m.digitar("TZAR_SEM_INTRO=1 TZAR_FRAMES=300 /apps/bin/tzar", 0.04)
        m.tecla("ret")
        g = esperar(m, r"\[TZAR\] instalacion: (\d+) paquetes, (\d+) mapas sueltos, (\d+) campanas", 300, desde=marca)
        verificar(int(g[1]) == 6 and int(g[2]) == 6, "SaguiOS lee la instalación real: %s paquetes, %s mapas, %s campañas" % g)
        time.sleep(8)
        cap(m, "menu")
        esperar(m, r"\[TZAR\] fin", 300, desde=marca)
        # 2) escaramuza contra la IA, unos minutos
        time.sleep(3)
        h136.focar_terminal(m)
        marca = len(m.serial())
        m.digitar("TZAR_SEM_INTRO=1 TZAR_AUTO=escaramuza TZAR_DEMO=1 TZAR_FRAMES=6000 /apps/bin/tzar", 0.04)
        m.tecla("ret")
        esperar(m, r"\[TZAR\] escaramuza:", 300, desde=marca)
        for k in range(4):
            time.sleep(45)
            cap(m, "escaramuza-%d" % k)
        fps = re.findall(r"\[TZAR\] ([0-9.]+) cuadros por segundo \(tic (\d+), (\d+) objetos\)", m.serial()[marca:])
        print("    cuadros por segundo:", fps[-6:])
        verificar(len(fps) > 0, "la escaramuza corre en SaguiOS (%s cps)" % (fps[-1][0] if fps else "?"))
        m.tecla("esc"); time.sleep(2)
        # 3) la primera misión de la campaña 1 con su historia
        marca2 = len(m.serial())
        esperar(m, r"\[TZAR\] fin", 900, desde=marca)
        time.sleep(3)
        h136.focar_terminal(m)
        marca = len(m.serial())
        m.digitar("TZAR_SEM_INTRO=1 TZAR_AUTO=campanha:1 TZAR_FRAMES=2400 /apps/bin/tzar", 0.04)
        m.tecla("ret")
        g = esperar(m, r"\[TZAR\] campa.a (.*), misi.n (.*): jugador", 300, desde=marca)
        time.sleep(10)
        cap(m, "campanha-historia")
        for k in range(3):
            m.tecla("ret"); time.sleep(6)
        cap(m, "campanha-partida")
        verificar(True, "la campaña abre su primera misión con la historia (%s)" % g[1])
        esperar(m, r"\[TZAR\] fin", 900, desde=marca)

def privado_aleat():
    """(a mano, nunca en la batería) La pantalla del mapa aleatorio y cuatro mapas distintos dentro de SaguiOS con la instalación REAL
    (como privado_saguios: TZAR_JOGOS o se copia de REAL). Capturas privadas en PRIVADO/capturas/aleat-saguios-*.png (aquí PRIVADO/aleat-*)."""
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    import shutil
    jogos = os.environ.get("TZAR_JOGOS")
    if not jogos:
        if not os.path.isdir(os.path.join(REAL, "Packs")):
            print("AVISO: no hay instalación real de Tzar: se salta «privado_aleat»")
            return
        jogos = tempfile.mkdtemp(prefix="tzar-jogos-")
        os.makedirs(os.path.join(jogos, "TZAR"))
        for x in ("Packs", "Maps", "Campaign"):
            shutil.copytree(os.path.join(REAL, x), os.path.join(jogos, "TZAR", x))
    destino = os.path.join(os.path.dirname(PRIVADO), "capturas")
    os.makedirs(destino, exist_ok=True)
    # lado terreno tipo rmapa guardado animales objetos puertas semilla reinos tipo victoria pob dif rec niebla explorar civ*8 equipo*8
    casos = [("1-continente-europeo", "4 1 3 1 1 1 1 1 11 4 0 0 200 1 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0", 0),
             ("2-archipielagos-nieve", "4 3 1 2 1 1 1 1 22 3 0 0 200 1 2 0 0 0 2 0 0 0 0 0 0 0 1 2 0 0 0 0 0", 0),
             ("3-islas-desierto-8", "5 4 2 0 1 0 1 1 33 8 0 0 200 1 0 0 0 0 0 0 0 0 0 0 0 1 1 2 2 0 0 0 0", 0),
             ("4-lagos-verano-grande", "6 2 4 2 0 1 0 0 44 5 0 0 200 1 1 0 0 0 3 0 0 0 0 0 0 0 1 1 2 0 0 0 0", 1)]
    img = imagem_de_teste("h417p", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "700", "--smp"])
    with Maquina("h417p", "ide", imagem=img, memoria="2048M") as m:
        esperar(m, r"sagui:/\$", 240)
        time.sleep(4)
        h136.focar_terminal(m)
        for nome, lin, ampliar in casos:
            marca = len(m.serial())
            m.digitar("TZAR_SEM_INTRO=1 TZAR_MENU=13 TZAR_ALEAT=ALEAT2_%s %s TZAR_FRAMES=1500 TZAR_W=1024 TZAR_H=768 /apps/bin/tzar" % (lin.replace(" ", "_"), "TZAR_AMPLIAR=1" if ampliar else ""), 0.04)
            m.tecla("ret")
            esperar(m, r"\[TZAR\] instalacion: (\d+) paquetes", 300, desde=marca)
            time.sleep(10)
            src = m.captura("tzar-aleat-" + nome, guardar=False)
            shutil.move(src, os.path.join(destino, "aleat-saguios-%s.png" % nome))
            esperar(m, r"\[TZAR\] fin", 300, desde=marca)
            time.sleep(2)
            h136.focar_terminal(m)
            verificar(True, "SaguiOS dibuja la pantalla del mapa aleatorio con la vista previa (%s)" % nome)
        # una partida en un mapa aleatorio con esas opciones (sin IA para que no pese) y guardar/cargar
        marca = len(m.serial())
        m.digitar("TZAR_SEM_INTRO=1 TZAR_AUTO=aleatorio TZAR_ALEAT=ALEAT2_%s TZAR_PRUEBA_GUARDAR=1 TZAR_FRAMES=300 TZAR_W=1024 TZAR_H=768 /apps/bin/tzar" % casos[1][1].replace(" ", "_"), 0.04)
        m.tecla("ret")
        esperar(m, r"\[TZAR\] prueba guardar/cargar aleatorio: igual", 600, desde=marca)
        verificar(True, "SaguiOS guarda y carga una partida en mapa aleatorio con sus opciones")
        esperar(m, r"\[TZAR\] fin", 600, desde=marca)


PARTES = {"git": git, "lectores": lectores, "ui": ui, "simulacao": simulacao, "extras": extras, "cuelgue": cuelgue, "campanhas": campanhas, "ia": ia, "pc": pc, "saguios": saguios, "privado": privado, "privado_ia": privado_ia, "privado_saguios": privado_saguios, "privado_aleat": privado_aleat}

if __name__ == "__main__":
    pedidas = sys.argv[1:] or ["git", "lectores", "ui", "simulacao", "extras", "campanhas", "ia", "cuelgue", "pc", "saguios"]
    for p in pedidas:
        print("== H417", p)
        PARTES[p]()
    print("H417 ok")
