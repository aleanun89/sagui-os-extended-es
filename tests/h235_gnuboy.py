#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H235 - Game Boy con SDL2-GNUBoy (C, sobre la SDL 2 del sistema) dentro del SaguiOS (D235).

Partes (por defecto, todas; o las que se pidan: h235_gnuboy.py unidad juego sin-roms libre):
  unidad     en el PC, sin QEMU: la ROM de prueba propia (tools/gnuboy_rom_teste.py: cabecera con
             logo y sumas correctas), el emulador del upstream compilado con tests/gnuboy_unidad.c
             (vídeo, sonido y teclas por guion): los cuatro tonos, A (invierte la paleta), Derecha y
             B (desplazan), un tono de 1000 Hz, Select (silencia), Start (escribe la batería) y una
             segunda ejecución que encuentra el .sav; y las fuentes fijadas (tools/gnuboy_fuentes.sh:
             SHA-256 y VERSAO). El escalado de texturas de la SDL 2 se prueba dentro del sistema, en
             «juego»: los límites entre cuadrantes caen en el píxel exacto (x=216, y=192 con x3).
  juego      la imagen con SAGUI-JOGOS (con /jogos/gb/saguitest.gb) y SAGUI-APPS y la tarjeta de
             sonido: Inicio > Juegos tiene «Game Boy (GNUBoy)»; abre la ventana para elegir ROM
             (con el nombre de la cabecera del cartucho), Intro la arranca en una ventana de
             480x432 (160x144 por 3); los cuatro cuadrantes con sus colores; Z (A) invierte la
             paleta, Derecha y X (B) desplazan la imagen, Mayús derecha (Select) silencia el
             sonido (el .wav), Intro (Start) guarda en la batería; F11 a pantalla completa con la
             proporción de 10:9 y bandas negras; se sale con Alt+F4 y el .sav queda en /home/gnuboy;
             al abrir otra vez desde el terminal con la ROM, la ROM encuentra la partida (el
             cuadrante de abajo a la derecha, oscuro); la configuración (gnuboy.rc) queda en
             /home/gnuboy.
  sin-roms   la imagen sin ROM: el lanzador abre su ventana de ayuda («faltan las ROM»).
  libre      si tools/gnuboy_roms_libres.sh bajó Libbet (zlib) a userland/terceros/gnuboy/roms-libres:
             sale en la lista y arranca con imagen y sonido. Sin ella, se salta con un aviso.
Capturas en docs/capturas/H235-*.png.
"""
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA, BUILD, APPS_RAIZ  # noqa: E402
from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui, janela  # noqa: E402
import h70_sonido as h70  # noqa: E402
import h136_opentyrian as h136  # noqa: E402
import h228_openjazz as h228  # noqa: E402

GNUBOY = os.path.join(APPS_RAIZ, "bin", "gnuboy")
FUENTES = os.path.join(RAIZ, "userland", "terceros", "gnuboy", "jogo")
LIBRES = os.path.join(RAIZ, "userland", "terceros", "gnuboy", "roms-libres")
TITULO = "SDL2 GNUBoy"        # el título con que el emulador crea su ventana
BORDA, BARRA = 2, 24          # el marco de las ventanas del kernel (kernel/gui/janelas.c)

# Los tonos de la paleta verde de GNUBoy como píxel 0x00RRGGBB (los comprueba la unidad en el PC).
TONO0, TONO1, TONO2, TONO3 = (0x9c, 0xbd, 0x08), (0x8c, 0xad, 0x08), (0x31, 0x63, 0x31), (0x08, 0x39, 0x08)
# Centros de los cuadrantes de la ventana de 480x432 (la pantalla de 160x144 por 3).
TL, TR, BL, BR = (120, 108), (360, 108), (120, 324), (360, 324)


# ------------------------------------------------------------------ unidad

def compilar_ejecutable(d):
    """gnuboy_unidad: el emulador del upstream + tests/gnuboy_unidad.c, para el PC."""
    exe = os.path.join(d, "gnuboy_unidad")
    src = sorted(f for f in os.listdir(os.path.join(FUENTES, "src")) if f.endswith(".c") and f != "main.c")
    orden = (["gcc", "-O1", "-w", "-D_GNU_SOURCE=1", "-DIS_LITTLE_ENDIAN",
              "-I", os.path.join(FUENTES, "include"), "-I", os.path.join(FUENTES, "lib"), "-I", FUENTES,
              "-o", exe, os.path.join(RAIZ, "tests", "gnuboy_unidad.c")]
             + [os.path.join(FUENTES, "src", f) for f in src]
             + [os.path.join(FUENTES, "lib", "gz", "inflate.c")]
             + [os.path.join(FUENTES, "lib", "xz", f) for f in sorted(os.listdir(os.path.join(FUENTES, "lib", "xz")))
                if f.endswith(".c")]
             + [os.path.join(FUENTES, "sys", "dummy", "io.c"), "-lm"])
    r = subprocess.run(orden, capture_output=True, text=True, cwd=RAIZ)
    if r.returncode != 0:
        print(r.stdout[-2000:], r.stderr[-2000:])
    verificar(r.returncode == 0, "SDL2-GNUBoy del upstream compila en el PC con el arnés de la prueba")
    return exe


def generar_rom(destino):
    r = subprocess.run([sys.executable, "-I", os.path.join(RAIZ, "tools", "gnuboy_rom_teste.py"), destino],
                       capture_output=True, text=True)
    verificar(r.returncode == 0 and "sumas correctas" in r.stdout, "tools/gnuboy_rom_teste.py genera la ROM (%s)" % r.stdout.strip())
    with open(destino, "rb") as f:
        rom = f.read()
    verificar(len(rom) == 0x8000 and rom[0x147] == 0x03 and rom[0x134:0x13D] == b"SAGUITEST",
              "la ROM: 32 KB, cartucho MBC1+RAM+batería, título SAGUITEST")
    return rom


def unidad():
    d = tempfile.mkdtemp(prefix="h235-", dir=SAIDA if os.path.isdir(SAIDA) else None)
    try:
        rom = os.path.join(d, "saguitest.gb")
        generar_rom(rom)
        # Las sumas, calculadas aparte de la herramienta.
        with open(rom, "rb") as f:
            b = f.read()
        x = 0
        for i in range(0x134, 0x14D):
            x = (x - b[i] - 1) & 0xFF
        verificar(b[0x14D] == x, "suma de la cabecera (0x14D) correcta (%02x)" % x)
        g = (sum(b) - b[0x14E] - b[0x14F]) & 0xFFFF
        verificar(b[0x14E] == g >> 8 and b[0x14F] == g & 0xFF, "suma global (0x14E-0x14F) correcta (%04x)" % g)
        verificar(b[0x104:0x108] == bytes.fromhex("CEED6666") and len(b[0x104:0x134]) == 48, "el logo de la cabecera")
        exe = compilar_ejecutable(d)
        r = subprocess.run([exe, rom], capture_output=True, text=True, cwd=d, timeout=300)
        print("    " + r.stdout.strip().replace("\n", "\n    ")[-2500:])
        verificar(r.returncode == 0 and "FALLO" not in r.stdout, "el emulador del upstream corre la ROM: tonos, teclas, sonido y Start")
        verificar(os.path.isfile(os.path.join(d, "saguitest.sav")) and os.path.getsize(os.path.join(d, "saguitest.sav")) == 8192,
                  "al salir deja la batería (saguitest.sav, 8 KB)")
        with open(os.path.join(d, "saguitest.sav"), "rb") as f:
            verificar(f.read(5) == b"SAGUI", "el .sav lleva la marca que escribió la ROM con Start")
        r = subprocess.run([exe, rom, "segunda"], capture_output=True, text=True, cwd=d, timeout=300)
        print("    " + r.stdout.strip().replace("\n", "\n    ")[-800:])
        verificar(r.returncode == 0 and "FALLO" not in r.stdout, "una segunda ejecución encuentra la partida guardada")
        # Las fuentes fijadas.
        with open(os.path.join(FUENTES, "VERSAO")) as f:
            v = f.read().split()
        verificar(len(v) == 3 and len(v[0]) == 40 and len(v[2]) == 64, "VERSAO: commit, fecha y SHA-256 del tarball (%s)" % v[0][:7])
        with open(os.path.join(RAIZ, "tools", "gnuboy_fuentes.sh")) as f:
            sh = f.read()
        verificar(v[0] in sh and v[2] in sh, "tools/gnuboy_fuentes.sh fija el mismo commit y el mismo SHA-256")
        lic = open(os.path.join(FUENTES, "LICENSE.md")).read(300).upper()
        verificar("GENERAL PUBLIC LICENSE" in lic and "VERSION 2" in lic, "LICENSE.md del upstream: GPL versión 2")
        r = subprocess.run([os.path.join(RAIZ, "tools", "gnuboy_roms_libres.sh"), "--comprobar"], capture_output=True, text=True)
        print("    " + (r.stdout + r.stderr).strip().replace("\n", "\n    "))
    finally:
        shutil.rmtree(d, ignore_errors=True)


# ------------------------------------------------------------------ imágenes

def jogos_con(roms):
    """Una raíz de SAGUI-JOGOS solo con /gb y las ROM que se digan ({nombre: ruta})."""
    d = os.path.join(SAIDA, "h235-jogos")
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    if roms is None:
        # Sin ROM, pero no vacía: mkimagem.py no hace la partición de una carpeta vacía.
        with open(os.path.join(d, "leeme.txt"), "w") as f:
            f.write("SAGUI-JOGOS de la prueba h235 sin ROM\n")
    else:
        os.makedirs(os.path.join(d, "gb"))
        for nombre, ruta in roms.items():
            shutil.copy(ruta, os.path.join(d, "gb", nombre))
    return d


def imagem(jogos):
    return imagem_de_teste("", ["--vesa", "--jogos", jogos, "--apps", APPS_RAIZ])


def abrir_pelo_menu(m, capturar=None):
    marca = len(m.serial())
    m.clique(40, 12)
    p = painel(m, marca)
    sub = submenu_pelo_mouse(m, p, "Juegos", "Game Boy (GNUBoy)")
    verificar("Game Boy (GNUBoy)" in sub["linhas"] and "OpenJazz" in sub["linhas"],
              "Inicio > Juegos tiene «Game Boy (GNUBoy)» junto a OpenJazz (%s)" % ", ".join(sub["linhas"]))
    time.sleep(0.4)
    if capturar:
        m.captura(capturar)
    marca = len(m.serial())
    clicar_aqui(m)
    esperar(m, r"inicio -> /apps/bin/gnuboy", desde=marca)
    return marca


def conteudo(m):
    """La esquina del contenido de la ventana del emulador y su tamaño."""
    j = janela(m.serial().replace("\r", ""), TITULO)
    return j["x"] + BORDA, j["y"] + BARRA, j["l"], j["a"]


def cor(rgb, w, x, y):
    i = (y * w + x) * 3
    return tuple(rgb[i:i + 3])


def parecido(a, b, tol=14):
    return all(abs(p - q) <= tol for p, q in zip(a, b))


def cuadrantes(m, nome):
    """Los colores de los cuatro cuadrantes (centros) en la ventana, y la pantalla entera."""
    x0, y0, _, _ = conteudo(m)
    w, h, rgb = h228.pantalla(m, nome)
    return [cor(rgb, w, x0 + x, y0 + y) for x, y in (TL, TR, BL, BR)], (w, h, rgb), (x0, y0)


def hex3(c):
    return "%02x%02x%02x" % c


def comprobar_cuadrantes(m, nome, esperados, que):
    cs, _, _ = cuadrantes(m, nome)
    ok = all(parecido(c, e) for c, e in zip(cs, esperados))
    verificar(ok, "%s: %s (esperados %s)" % (que, " ".join(hex3(c) for c in cs), " ".join(hex3(e) for e in esperados)))


def segurar(m, tecla, ms):
    """Mantiene la tecla apretada ms milisegundos (el QEMU suelta solo): el emulador lee los
    botones una vez por cuadro, y una pulsación corta se le escaparía."""
    m.monitor("sendkey %s %d" % (tecla, ms))


def sem_falhas(m, desde, rotulo):
    s = m.serial()[desde:]
    verificar("Fallo de página" not in s and "Fallo de p" not in s, f"{rotulo}: sin faltas de página")


def linhas_janela(m, x, y, passo, n, cols):
    """Lee n líneas de texto de la fuente del sistema que empiezan en (x, y + passo*i)
    (fuera de la rejilla de 8x16 de tela_texto()), con hasta 4 píxeles de margen arriba o abajo."""
    from qemu import ler_ppm, reconhecer_texto
    ppm = os.path.join(SAIDA, "h235-linhas.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor("screendump " + ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.2)
    l, a, px = ler_ppm(ppm)
    res = []
    for i in range(n):
        melhor = ""
        for dy in range(-4, 5):
            y0 = y + passo * i + dy
            if y0 < 0 or y0 + 16 > a:
                continue
            w = min(cols * 8, l - x)
            recorte = b"".join(px[((y0 + k) * l + x) * 3:((y0 + k) * l + x + w) * 3] for k in range(16))
            txt = reconhecer_texto(w, 16, recorte)[0]
            if len(txt.replace(" ", "")) > len(melhor.replace(" ", "")):
                melhor = txt
        res.append(melhor)
    return res


# ------------------------------------------------------------------ juego

def juego():
    if not os.path.isfile(GNUBOY):
        raise AssertionError("falta %s (make: GNUBOY=1, sin SEM_GPL)" % GNUBOY)
    os.makedirs(SAIDA, exist_ok=True)
    rom = os.path.join(SAIDA, "saguitest.gb")
    generar_rom(rom)
    img = imagem(jogos_con({"saguitest.gb": rom}))
    with Maquina("h235-juego", "ide", imagem=img, som="hda-output") as m:
        h228.terminal(m)
        marca = abrir_pelo_menu(m, "H235-1-menu-juegos")
        esperar(m, r"\[gnuboy\] 1 ROM encontradas", 60, desde=marca)
        esperar(m, r"criada por \d+: \"Game Boy \(GNUBoy\): elegir ROM\"", 30, desde=marca)
        time.sleep(1.5)
        # La lista, por el registro del lanzador: el texto de la ventana no cae en la
        # rejilla de 8x16 de tela_texto() (la ventana va en (218,110)); la captura lo enseña.
        verificar(re.search(r"\[gnuboy\] lista: SAGUITEST  \(saguitest\.gb\)", m.serial()[marca:]) is not None,
                  "la lista de ROM muestra el nombre de la cabecera (SAGUITEST) y el archivo (saguitest.gb)")
        m.captura("H235-2-elegir-rom", guardar=True)
        m.tecla("ret")
        esperar(m, r"\[gnuboy\] ROM: /jogos/gb/saguitest\.gb", 30, desde=marca)
        esperar(m, r"\[GNUBOY\] ventana 480x432 creada \(lienzo 480x432\)", 60, desde=marca)
        verificar(True, "SDL2-GNUBoy abre su ventana de 480x432 (160x144 por 3) con la SDL 2")
        esperar(m, r"\[GNUBOY\] audio 44100 Hz, 2 canales, 8 bits", 30, desde=marca)
        verificar(True, "abre el sonido: 44100 Hz, estéreo, 8 bits (por la llamada de vuelta de la SDL 2)")
        esperar(m, r"\[GNUBOY\] primer cuadro dibujado", 30, desde=marca)
        time.sleep(3)
        x0, y0, l, a = conteudo(m)
        verificar((l, a) == (480, 432), f"la ventana del sistema es de 480x432 ({l}x{a})")
        m.clique(x0 + l // 2, y0 + a // 2)   # el foco del teclado
        time.sleep(1)
        normal = [TONO3, TONO1, TONO2, TONO0]
        comprobar_cuadrantes(m, "h235-normal", normal, "la ROM pinta los cuatro tonos en sus cuadrantes")
        m.captura("H235-3-pantalla", guardar=True)
        sem_falhas(m, marca, "arranque")

        # Las teclas, esperando a ver el efecto en vez de un tiempo fijo. El «sendkey TECLA
        # ms» del monitor del QEMU mete la pulsación en su cola de entrada detrás de las
        # esperas de los sendkey anteriores: cada tecla llega al invitado más tarde que la
        # anterior (medido: la tercera, unos 11 s después de mandarla). Las marcas del
        # kernel, de la SDL 2 y del emulador lo confirman: la tecla llega bien, pero tarde.
        # Por eso se espera a ver el efecto (hasta 40 s) y después a que la imagen vuelva a
        # la normal, para que la siguiente tecla empiece de cero.
        def ver(nome, cond, tempo=40):
            fim = time.time() + tempo
            while True:
                _, (w, h, rgb), (x0, y0) = cuadrantes(m, nome)
                if cond(w, rgb, x0, y0) or time.time() > fim:
                    return cond(w, rgb, x0, y0), (w, rgb, x0, y0)

        def e_normal(w, rgb, x0, y0):
            return all(parecido(cor(rgb, w, x0 + x, y0 + y), e) for (x, y), e in zip((TL, TR, BL, BR), normal))

        def tecla_e_volta(tecla, nome, cond, que, captura=None):
            segurar(m, tecla, 3500)
            ok, (w, rgb, x0, y0) = ver(nome, cond)
            if captura:
                m.captura(captura, guardar=True)
            verificar(ok, que(w, rgb, x0, y0))
            ok2, _ = ver(nome + "-suelta", e_normal, 40)
            return ok2

        # Z (A) invierte la paleta mientras está apretada.
        inv = [TONO0, TONO2, TONO1, TONO3]
        volta = tecla_e_volta(
            "z", "h235-z",
            lambda w, rgb, x0, y0: all(parecido(cor(rgb, w, x0 + x, y0 + y), e) for (x, y), e in zip((TL, TR, BL, BR), inv)),
            lambda w, rgb, x0, y0: "Z (A): la paleta se invierte: %s (esperados %s)" % (
                " ".join(hex3(cor(rgb, w, x0 + x, y0 + y)) for x, y in (TL, TR, BL, BR)), " ".join(hex3(e) for e in inv)),
            "H235-4-tecla-a")
        verificar(volta, "al soltar Z la paleta vuelve")

        # Derecha: la imagen se desplaza 8 píxeles de la pantalla del emulador (24 en la ventana).
        volta = tecla_e_volta(
            "right", "h235-der",
            lambda w, rgb, x0, y0: parecido(cor(rgb, w, x0 + 228, y0 + 108), TONO1) and parecido(cor(rgb, w, x0 + 204, y0 + 108), TONO3),
            lambda w, rgb, x0, y0: "Derecha: el límite entre los cuadrantes pasa de x=240 a x=216")
        verificar(volta, "al soltar Derecha la imagen vuelve")

        # X (B): 8 píxeles hacia arriba.
        volta = tecla_e_volta(
            "x", "h235-b",
            lambda w, rgb, x0, y0: parecido(cor(rgb, w, x0 + 120, y0 + 204), TONO2) and parecido(cor(rgb, w, x0 + 120, y0 + 180), TONO3),
            lambda w, rgb, x0, y0: "X (B): el límite entre arriba y abajo pasa de y=216 a y=192 (en x=120, de 12 en 12: %s)"
            % " ".join(hex3(cor(rgb, w, x0 + 120, y0 + y)) for y in range(0, 432, 12)))
        verificar(volta, "al soltar X la imagen vuelve")

        # Select (Mayús derecha): el sonido se corta 4 s.
        segurar(m, "shift_r", 4000)
        time.sleep(6)
        # Start (Intro): la ROM guarda «SAGUI» en la batería.
        segurar(m, "ret", 1500)
        time.sleep(3)
        sem_falhas(m, marca, "las teclas")

        # Pantalla completa (F11 del sistema): la proporción de 10:9 con bandas negras.
        desde = len(m.serial())
        m.tecla("f11")
        try:   # con la cola del monitor del QEMU (arriba), la tecla puede tardar
            esperar(m, r"\[GNUBOY\] escala: lienzo 480x432 en ventana \d+x\d+", 60, desde=desde)
        except Exception:
            pass
        time.sleep(2)
        esc = re.findall(r"\[GNUBOY\] escala: lienzo 480x432 en ventana (\d+)x(\d+) -> (\d+)x(\d+) en \((\d+),(\d+)\)",
                         m.serial()[desde:])
        verificar(bool(esc), "al pasar a pantalla completa la SDL 2 recalcula la escala")
        jl, ja, dl, da, dx, dy = map(int, esc[-1])
        print("    pantalla completa: ventana %dx%d, imagen %dx%d en (%d,%d)" % (jl, ja, dl, da, dx, dy))
        verificar(abs(dl * 432 - da * 480) <= max(dl, da) and (dx > 0 or dy > 0 or (dl, da) == (jl, ja)),
                  "la imagen guarda la proporción 480:432 (%dx%d en %dx%d, con bandas si no es igual)" % (dl, da, jl, ja))
        w, h, rgb = h228.pantalla(m, "h235-cheia")
        m.captura("H235-5-pantalla-completa", guardar=True)
        # Si la pantalla del QEMU es la de la ventana (lo normal en pantalla completa), la banda es negra.
        if dx > 0 and w == jl:
            verificar(cor(rgb, w, 2, h // 2) == (0, 0, 0) and parecido(cor(rgb, w, dx + dl // 4, dy + da // 4), TONO3),
                      "a pantalla completa hay bandas negras a los lados y la imagen en el centro")
        elif dy > 0 and h == ja:
            verificar(cor(rgb, w, w // 2, 2) == (0, 0, 0) and parecido(cor(rgb, w, dx + dl // 4, dy + da // 4), TONO3),
                      "a pantalla completa hay bandas negras arriba y abajo y la imagen en el centro")
        else:
            verificar(parecido(cor(rgb, w, dx + dl // 4, dy + da // 4), TONO3), "a pantalla completa se ve la imagen")
        m.tecla("f11")
        time.sleep(3)
        sem_falhas(m, marca, "pantalla completa")

        # Salir: Alt+F4 -> SDL_QUIT -> exit del emulador, que guarda la batería.
        desde = len(m.serial())
        m.monitor("sendkey alt-f4")
        esperar(m, r"\[gnuboy\] fin", 30, desde=desde)
        verificar(True, "GNUBoy sale con Alt+F4 y vuelve el control al sistema")
        time.sleep(2)
        h136.focar_terminal(m)
        out = m.comando("ls /home/gnuboy")
        verificar("saguitest.sav" in out, f"la batería queda en /home/gnuboy (saguitest.sav): {out.strip()[-120:]}")
        verificar("gnuboy.rc" in out, "la configuración inicial (gnuboy.rc) queda en /home/gnuboy")
        out = m.comando("cat /home/gnuboy/gnuboy.rc")
        verificar("bind z +a" in out and "bind x +b" in out and "bind shift +select" in out and "set scale 3" in out,
                  "gnuboy.rc lleva las teclas (Z/X, Intro, Mayús) y la escala x3")

        # La segunda vez, con la ROM por la línea de órdenes: encuentra la batería.
        marca2 = len(m.serial())
        m.digitar("gnuboy /jogos/gb/saguitest.gb\n", 0.04)
        esperar(m, r"\[GNUBOY\] primer cuadro dibujado", 60, desde=marca2)
        time.sleep(4)
        verificar("[gnuboy] ROM: /jogos/gb/saguitest.gb" in m.serial()[marca2:], "con la ROM en la línea de órdenes no pregunta: arranca")
        comprobar_cuadrantes(m, "h235-segunda", [TONO3, TONO1, TONO2, TONO3],
                             "la ROM encuentra la partida guardada: el cuadrante de abajo a la derecha, oscuro")
        m.captura("H235-6-partida-cargada", guardar=True)
        desde = len(m.serial())
        m.monitor("sendkey alt-f4")
        esperar(m, r"\[gnuboy\] fin", 30, desde=desde)
        sem_falhas(m, marca, "toda la sesión")
    # El sonido, del .wav del QEMU: el tono de 1000 Hz y el silencio de Select.
    esq, dir_ = h70.ler_gravacao(m.wav)
    ventana = 4800   # 100 ms a 48 kHz
    rms = [h70.rms(esq[i:i + ventana]) for i in range(0, len(esq) - ventana, ventana)]
    maximo = max(rms) if rms else 0
    verificar(maximo > 20, f"suena (RMS máximo {maximo:.0f})")
    activas = [k for k, v in enumerate(rms) if v > maximo * 0.3]
    verificar(len(activas) > 40, f"suena el tono muchas veces ({len(activas)} ventanas de 100 ms)")
    i0 = activas[0]
    # Por los cruces con la media, como la parte «unidad»: h70.dominante() barre de 20 en 20 Hz
    # y un tono de 990 Hz (el que sale: la cuadrada de la ROM, a 44100 Hz y llevada a 48 kHz)
    # cae justo entre dos pasos, donde la ventana de 0,1 s no lo ve; ganaba el tercer armónico.
    x = esq[i0 * ventana + ventana:i0 * ventana + 2 * ventana]
    media = sum(x) / len(x)
    cruces = sum(1 for a, b in zip(x, x[1:]) if (a - media) * (b - media) < 0)
    f = cruces / 2 / (len(x) / 48000)
    verificar(abs(f - 1000) <= 50, f"el tono es de unos 1000 Hz ({f:.0f} Hz por los cruces)")
    # El silencio de Select: 4 s sin tono entre el primer y el último sonido (al menos 0,8 s seguidos).
    mejor = actual = 0
    for v in rms[activas[0]:activas[-1]]:
        actual = actual + 1 if v < maximo * 0.05 else 0
        mejor = max(mejor, actual)
    verificar(mejor >= 8, f"Select silencia el sonido ({mejor / 10:.1f} s seguidos en silencio entre dos tonos)")


# ------------------------------------------------------------------ sin ROM

def sin_roms():
    if not os.path.isfile(GNUBOY):
        raise AssertionError("falta %s" % GNUBOY)
    os.makedirs(SAIDA, exist_ok=True)
    img = imagem(jogos_con(None))
    with Maquina("h235-sin", "ide", imagem=img) as m:
        h228.terminal(m)
        marca = abrir_pelo_menu(m)
        esperar(m, r"\[gnuboy\] sin ROM: ventana de ayuda", 60, desde=marca)
        time.sleep(2)
        j = janela(m.serial(), "Game Boy (GNUBoy): faltan las ROM")
        verificar(j["l"] >= 600, "sin ROM sale la ventana que explica dónde ponerlas (%dx%d)" % (j["l"], j["a"]))
        # El texto de la ventana no cae en la rejilla de 8x16 de tela_texto(): se lee cada
        # línea de la ayuda en su sitio (rotulo en x=16, y=14+18*i dentro de la ventana),
        # probando unos píxeles arriba y abajo.
        t = linhas_janela(m, j["x"] + BORDA + 16, j["y"] + BARRA + 14, 18, 14, 76)
        verificar(any("/jogos/gb" in l for l in t), "la ayuda dice la carpeta /jogos/gb (%s)" % " / ".join(l.strip() for l in t if l.strip())[:300])
        m.captura("H235-7-sin-roms", guardar=True)
        m.clique(j["x"] + BORDA + 340 + 55, j["y"] + BARRA + 296 + 14)   # «Cerrar»
        time.sleep(2)
        s = m.serial()[marca:]
        verificar("ventana 480x432 creada" not in s, "sin ROM el emulador no arranca")
        verificar("Fallo de página" not in s, "sin ROM no hay faltas de página")


# ------------------------------------------------------------------ ROM libre

def libre():
    ruta = os.path.join(LIBRES, "libbet.gb")
    if not os.path.isfile(GNUBOY):
        raise AssertionError("falta %s" % GNUBOY)
    if not os.path.isfile(ruta):
        print("AVISO: no hay ROM libres (tools/gnuboy_roms_libres.sh baja Libbet, zlib): se salta la parte «libre»")
        return
    os.makedirs(SAIDA, exist_ok=True)
    img = imagem(jogos_con({"libbet.gb": ruta}))
    with Maquina("h235-libre", "ide", imagem=img, som="hda-output") as m:
        h228.terminal(m)
        marca = abrir_pelo_menu(m)
        esperar(m, r"\[gnuboy\] 1 ROM encontradas", 60, desde=marca)
        time.sleep(2)
        verificar(re.search(r"\[gnuboy\] lista: LIBBET.*\[GBC\]", m.serial()[marca:]) is not None,
                  "la lista muestra Libbet con su marca de Game Boy Color (la cabecera 0x143)")
        m.tecla("ret")
        esperar(m, r"\[GNUBOY\] primer cuadro dibujado", 60, desde=marca)
        time.sleep(10)
        x0, y0, l, a = conteudo(m)
        w, h, rgb = h228.pantalla(m, "h235-libbet")
        colores = set(cor(rgb, w, x0 + x, y0 + y) for x in range(8, l - 8, 12) for y in range(8, a - 8, 12))
        m.captura("H235-8-libbet", guardar=True)
        verificar(len(colores) >= 3, f"Libbet pinta algo (colores distintos en la ventana: {len(colores)})")
        sem_falhas(m, marca, "Libbet")
        m.monitor("sendkey alt-f4")
        time.sleep(2)


PARTES = {"unidad": unidad, "juego": juego, "sin-roms": sin_roms, "libre": libre}


def main():
    os.makedirs(SAIDA, exist_ok=True)
    pedidas = sys.argv[1:] or list(PARTES)
    for p in pedidas:
        if p not in PARTES:
            print("uso: h235_gnuboy.py [%s]" % " ".join(PARTES))
            return 2
    for p in pedidas:
        print("== %s" % p)
        PARTES[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
