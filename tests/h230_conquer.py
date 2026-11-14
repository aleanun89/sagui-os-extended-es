#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H230 - Command & Conquer (Vanilla Conquer, C++ con la SDL 2 y la OpenAL del sistema) dentro del SaguiOS (D230).

Partes (por defecto, todas; o las que se pidan: h230_conquer.py unidad juego sin-datos):
  unidad     en el PC: el mezclador de la OpenAL (tests/openal_unidad.c: búferes estáticos,
             remuestreo, ganancia, la cola de streaming como la usa Vanilla Conquer, una pausa
             larga sin quedarse parada); fnmatch/basename/dirname de la libc común contra la
             glibc (tests/padroes_unidad.c); las cabeceras de C++ nuevas (<string>, <chrono>,
             <stdexcept>, std:: de las de C) con tests/cxx_biblioteca_unidad.cpp; el
             descompresor de InstallShield de tools/cnc_extraer.py y, si están las ISO en
             input_share/games/CnC, lo que saca de ellas.
  juego      la imagen con SAGUI-JOGOS (/jogos) y SAGUI-APPS (/apps) y la tarjeta de sonido:
             Inicio > Juegos tiene «Command & Conquer»; desde el menú abre su ventana de 640x400
             con los datos de /jogos/CnC (los dos discos), el sonido por la OpenAL; el menú
             principal se ve (captura no negra); «Start New Game» con el teclado, la dificultad,
             el lado GDI con un clic; la primera misión de GDI carga; se seleccionan unidades
             con el ratón (un recuadro) y se mandan a otro sitio con un clic (la pantalla cambia
             mucho más que quieta); suena (el .wav del QEMU); se sale con Alt+F4 sin faltas de
             página ni std::terminate; la configuración queda en /home/cnc.
             Sin datos de C&C en build/jogos/raiz, se salta con un aviso.
  sin-datos  la imagen sin datos de C&C: el lanzador abre su ventana de ayuda («faltan los
             datos») en vez de fallar.
  cursor     el puntero del sistema oculto sobre una ventana con JAN_SEM_CURSOR (D230), con
             /testes/sem-cursor (una ventana verde lisa): sobre ella no se dibuja la flecha;
             fuera de ella, sí; con un menú contextual abierto, sí; con «v» (visible), sí; con
             «o» vuelve a ocultarse; al cerrar la ventana, la flecha vuelve. No necesita datos.
Capturas en docs/capturas/H230-*.png.
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

JOGOS = os.path.join(BUILD, "jogos", "raiz")   # la raíz de SAGUI-JOGOS que monta el make (D228)
CONQUER = os.path.join(APPS_RAIZ, "bin", "conquer")
# Las ISO (fuera de git): las de input_share/, o las de la carpeta que diga INPUT_SHARE (como el make).
ISOS = os.path.join(os.path.join(RAIZ, os.environ.get("INPUT_SHARE") or "input_share"), "games", "CnC")
TITULO = "Vanilla Conquer"   # el título que pone el motor a su ventana
BORDA, BARRA = 2, 24          # el marco de las ventanas del kernel (kernel/gui/janelas.c)


# ------------------------------------------------------------------ unidad

def compilar_e_correr(rotulo, ordem, exe):
    env = dict(os.environ)
    r = subprocess.run(ordem, capture_output=True, text=True, env=env, cwd=RAIZ)
    if r.returncode != 0:
        print(r.stdout[-2000:], r.stderr[-2000:])
    verificar(r.returncode == 0, f"{rotulo}: compila en el PC")
    r = subprocess.run([exe], capture_output=True, text=True, timeout=120)
    print("    " + r.stdout.strip().replace("\n", "\n    ")[-1500:])
    verificar(r.returncode == 0 and "todo bien" in r.stdout, f"{rotulo}: todo bien")


def unidad():
    d = tempfile.mkdtemp(prefix="h230-", dir=SAIDA if os.path.isdir(SAIDA) else None)
    try:
        exe = os.path.join(d, "openal_unidad")
        compilar_e_correr("OpenAL del SaguiOS (mezclador)",
                          ["gcc", "-O2", "-Wall", "-Wextra", "-I", "userland/libopenal", "-I", "userland/inc",
                           "-o", exe, "tests/openal_unidad.c"], exe)
        exe = os.path.join(d, "padroes_unidad")
        compilar_e_correr("fnmatch, basename y dirname de la libc común",
                          ["gcc", "-O2", "-Wall", "-Wextra", "-o", exe, "tests/padroes_unidad.c"], exe)
        libstdcxx = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6"
        if os.path.exists(libstdcxx) and h228.compilador_cxx():
            exe = os.path.join(d, "cxx_biblioteca_unidad")
            compilar_e_correr("cabeceras de C++ (<string>, <chrono>, <stdexcept>...)",
                              ["g++", "-std=gnu++17", "-O2", "-nostdinc++", "-I", "userland/libcxx/include",
                               "-o", exe, "tests/cxx_biblioteca_unidad.cpp", libstdcxx, "-lm"], exe)
        else:
            print("AVISO: sin g++ o sin libstdc++.so.6 en el PC: se salta la prueba de las cabeceras de C++")
        r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "cnc_extraer.py"), "--comprobar"],
                           capture_output=True, text=True)
        verificar(r.returncode == 0, "cnc_extraer: el descompresor de InstallShield (el ejemplo de blast.c) " + r.stdout.strip())
        isos = [f for f in os.listdir(ISOS)] if os.path.isdir(ISOS) else []
        if any(f.lower().endswith(".iso") for f in isos):
            dest = os.path.join(d, "cnc")
            t = time.time()
            r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "cnc_extraer.py"), "--buscar", ISOS, dest],
                               capture_output=True, text=True, timeout=600)
            print("    " + r.stdout.strip(), "(%.1f s)" % (time.time() - t))
            verificar(r.returncode == 0, "cnc_extraer saca los datos de las ISO de input_share/games/CnC")
            faltan = [f for f in ("CONQUER.MIX", "UPDATE.MIX", "UPDATEC.MIX", "CCLOCAL.MIX", "SPEECH.MIX", "TRANSIT.MIX",
                                  "TEMPERAT.MIX", "TEMPICNH.MIX", "SOUNDS.MIX", "gdi/GENERAL.MIX")
                      if not os.path.isfile(os.path.join(dest, f))]
            verificar(not faltan, "están los .MIX de la raíz, los del instalador y gdi/GENERAL.MIX (faltan: %s)" % faltan)
            verificar(not any(f.upper().endswith((".EXE", ".DLL", ".ISO")) for _, _, fs in os.walk(dest) for f in fs),
                      "no se copia nada de Windows (EXE, DLL) ni las ISO")
            # La cabecera de un .MIX: el número de ficheros (16 bits) y el tamaño de los datos.
            with open(os.path.join(dest, "UPDATE.MIX"), "rb") as f:
                cab = f.read(6)
            n = int.from_bytes(cab[0:2], "little")
            verificar(0 < n < 5000, f"UPDATE.MIX (descomprimido de SETUP.Z) tiene una cabecera de MIX válida ({n} ficheros)")
        else:
            print("AVISO: no hay ISO de C&C en input_share/games/CnC: no se prueba la extracción")
    finally:
        shutil.rmtree(d, ignore_errors=True)


# ------------------------------------------------------------------ imágenes

def tem_dados_cnc(pasta):
    """La carpeta de /jogos que tiene datos de C&C (o None)."""
    if not os.path.isdir(pasta):
        return None
    for n in sorted(os.listdir(pasta)):
        p = os.path.join(pasta, n)
        if os.path.isdir(p) and any(os.path.exists(os.path.join(p, f)) for f in ("CONQUER.MIX", "conquer.mix")):
            return n
    return None


def jogos_sem_cnc():
    """Una copia de la raíz de SAGUI-JOGOS sin ningún dato de C&C (ni de los otros juegos grandes)."""
    d = os.path.join(SAIDA, "h230-jogos-sin-cnc")
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    for n in os.listdir(JOGOS):
        p = os.path.join(JOGOS, n)
        if os.path.isdir(p) and n in ("chip8", "sagui3d", "jclic", "opentyrian"):
            shutil.copytree(p, os.path.join(d, n))
    return d


def imagem(jogos):
    return imagem_de_teste("", ["--vesa", "--jogos", jogos, "--apps", APPS_RAIZ])


def abrir_pelo_menu(m, capturar=None):
    marca = len(m.serial())
    m.clique(40, 12)
    p = painel(m, marca)
    sub = submenu_pelo_mouse(m, p, "Juegos", "Command & Conquer")
    verificar("Command & Conquer" in sub["linhas"] and "OpenJazz" in sub["linhas"],
              "Inicio > Juegos tiene «Command & Conquer» junto a OpenJazz (%s)" % ", ".join(sub["linhas"]))
    time.sleep(0.4)
    if capturar:
        m.captura(capturar)
    marca = len(m.serial())
    clicar_aqui(m)
    esperar(m, r"inicio -> /apps/bin/conquer", desde=marca)
    return marca


def conteudo(m):
    """La esquina del contenido de la ventana del juego en la pantalla y su tamaño."""
    j = janela(m.serial().replace("\r", ""), TITULO)
    return j["x"] + BORDA, j["y"] + BARRA, j["l"], j["a"]


def area(m, nome, caixa=None):
    """Los píxeles del contenido de la ventana del juego (muestreados de 2 en 2); caixa =
    (x0, y0, x1, y1) en coordenadas del juego para mirar solo una parte."""
    x0, y0, larg, alto = conteudo(m)
    w, h, rgb = h228.pantalla(m, nome)
    cx0, cy0, cx1, cy1 = caixa or (4, 4, larg - 4, alto - 4)
    px = []
    for y in range(y0 + cy0, min(y0 + cy1, h), 2):
        fila = rgb[(y * w + x0 + cx0) * 3:(y * w + min(x0 + cx1, w)) * 3]
        px.extend(fila[i:i + 3] for i in range(0, len(fila), 6))
    return px


def clique_jogo(m, x, y, botao=1):
    x0, y0, _, _ = conteudo(m)
    m.clique(x0 + x, y0 + y, botao)


def sem_falhas(m, desde, rotulo):
    s = m.serial()[desde:]
    verificar("Fallo de página" not in s and "Fallo de p" not in s, f"{rotulo}: sin faltas de página")
    verificar("std::terminate" not in s and "[CXXRT]" not in s, f"{rotulo}: sin std::terminate ni avisos del runtime de C++")


# ------------------------------------------------------------------ juego

def juego():
    if not os.path.isfile(CONQUER):
        raise AssertionError("falta %s (make: hace falta el compilador de C++, tools/entorno-local.sh)" % CONQUER)
    carpeta = tem_dados_cnc(JOGOS)
    if not carpeta:
        print("AVISO: no hay datos de Command & Conquer en %s (las ISO de C&C Gold en input_share/games/CnC): "
              "se salta la parte «juego»" % JOGOS)
        return
    print("datos de Command & Conquer en /jogos/%s" % carpeta)
    img = imagem(JOGOS)
    with Maquina("h230-juego", "ide", imagem=img, som="hda-output") as m:
        h228.terminal(m)
        marca = abrir_pelo_menu(m, capturar="H230-1-menu-juegos")
        esperar(m, r"\[CNC\] datos en /jogos/\S+ \(los dos discos\)", 60, desde=marca)
        verificar(True, "el lanzador encuentra los datos de los dos discos (gdi/ y nod/)")
        esperar(m, r"\[CNC\] ventana 640x400 creada \(lienzo 640x400\)", 120, desde=marca)
        verificar(True, "Vanilla Conquer abre su ventana de 640x400 con la SDL 2")
        esperar(m, r"\[OPENAL\] dispositivo abierto: 22050 Hz", 60, desde=marca)
        verificar(True, "abre el sonido por la OpenAL (22050 Hz, estéreo, 16 bits)")
        esperar(m, r"\[CNC\] primer cuadro dibujado", 60, desde=marca)
        esperar(m, r"\[CNC\] puntero del sistema oculto", 10, desde=marca)
        verificar(True, "el motor oculta el puntero del sistema (SDL_ShowCursor): solo se ve el del juego")
        time.sleep(8)
        menu = area(m, "h230-menu")
        cores = len(set(menu))
        escuros = sum(1 for p in menu if p[0] < 16 and p[1] < 16 and p[2] < 16) * 100.0 / max(len(menu), 1)
        m.captura("H230-2-menu", guardar=True)
        verificar(cores > 16 and escuros < 90, f"el menú principal se ve ({cores} colores, {escuros:.0f}% negro)")
        sem_falhas(m, marca, "hasta el menú")

        # Start New Game (el botón marcado al abrir el menú) y la dificultad, con Intro.
        clique_jogo(m, 320, 380)   # el foco del teclado, en una zona sin botones
        m.tecla("ret")
        time.sleep(4)
        m.captura("H230-3-dificultad", guardar=True)
        m.tecla("ret")
        time.sleep(6)
        # Elegir lado: la mitad izquierda (GDI) con el ratón (intro.cpp, Choose_Side: x 36..296, y 96..300).
        m.captura("H230-4-lado", guardar=True)
        clique_jogo(m, 160, 200)
        time.sleep(20)   # la voz de «GDI», sin película (sin MOVIES.MIX), y la carga de la misión
        m.tecla("ret")   # por si queda algún cartel
        time.sleep(8)
        m.captura("H230-5-mision", guardar=True)
        sem_falhas(m, marca, "la carga de la primera misión")

        # Mover unidades: un recuadro con el ratón sobre la zona de juego (la barra lateral
        # va a la derecha) y un clic en otro sitio; se compara con la pantalla quieta.
        tactica = (8, 30, 470, 390)
        quieto1 = area(m, "h230-quieto1", tactica)
        time.sleep(2)
        quieto2 = area(m, "h230-quieto2", tactica)
        x0, y0, _, _ = conteudo(m)
        m.arrastar(x0 + 20, y0 + 40, x0 + 460, y0 + 380)
        time.sleep(1)
        clique_jogo(m, 240, 110)
        time.sleep(4)
        movido = area(m, "h230-movido", tactica)
        d_quieto, d_movido = h228.diferentes(quieto1, quieto2), h228.diferentes(quieto2, movido)
        print("    cambio quieto %.1f%%, tras la orden %.1f%%" % (d_quieto, d_movido))
        m.captura("H230-6-unidades", guardar=True)
        verificar(d_movido > d_quieto + 2,
                  "seleccionar y mandar unidades con el ratón cambia la pantalla más que quieta")
        sem_falhas(m, marca, "la partida")

        # Salir: Alt+F4 (EV_FECHAR -> SDL_QUIT -> exit del motor).
        desde = len(m.serial())
        m.monitor("sendkey alt-f4")
        esperar(m, r"\[CNC\] fin", 30, desde=desde)
        verificar(True, "Command & Conquer sale con Alt+F4 y vuelve el control al sistema")
        sem_falhas(m, marca, "toda la sesión")
        time.sleep(2)
        h136.focar_terminal(m)
        out = m.comando("ls /home/cnc")
        verificar("CONQUER.INI" in out.upper() and "VANILLATD.LOG" in out.upper(),
                  f"la configuración y el registro quedan en /home/cnc ({out.strip()[-160:]})")
    esq, dir_ = h70.ler_gravacao(m.wav)
    rms = h70.rms(esq[:48000 * 90])
    verificar(rms > 50, f"suena: la música y las voces llegan a la tarjeta (RMS {rms:.0f})")


# ------------------------------------------------------------------ sin datos

def sin_datos():
    if not os.path.isfile(CONQUER):
        raise AssertionError("falta %s" % CONQUER)
    img = imagem(jogos_sem_cnc())
    with Maquina("h230-sin", "ide", imagem=img) as m:
        h228.terminal(m)
        marca = abrir_pelo_menu(m)
        esperar(m, r"\[CNC\] faltan los datos: ventana de ayuda", 60, desde=marca)
        time.sleep(2)
        j = janela(m.serial(), "Command & Conquer: faltan los datos")
        verificar(j["l"] >= 600, "sin datos sale la ventana que explica cómo conseguirlos (%dx%d)" % (j["l"], j["a"]))
        m.captura("H230-7-sin-datos", guardar=True)
        m.clique(j["x"] + BORDA + 350 + 55, j["y"] + BARRA + 244 + 14)   # «Cerrar»
        time.sleep(2)
        s = m.serial()[marca:]
        verificar("ventana 640x400 creada" not in s, "sin datos el juego no arranca")
        verificar("Fallo de página" not in s, "sin datos no hay faltas de página")


# ------------------------------------------------------------------ el puntero del sistema

VERDE = (0x3C, 0xDC, 0x6E)   # el color de /testes/sem-cursor


def tem_seta(rgb, w, h, x, y, raio=8):
    """¿Hay una flecha del sistema cerca de (x, y)? La punta es negra, la de debajo
    también, y el interior (x+1, y+3) blanco (la seta de kernel/dispositivos/video.c)."""
    def px(a, b):
        if not (0 <= a < w and 0 <= b < h):
            return None
        i = (b * w + a) * 3
        return tuple(rgb[i:i + 3])
    for yy in range(y - raio, y + raio + 1):
        for xx in range(x - raio, x + raio + 1):
            if px(xx, yy) == (0, 0, 0) and px(xx, yy + 1) == (0, 0, 0) and px(xx + 1, yy + 3) == (255, 255, 255):
                return True
    return False


def nao_verdes(rgb, w, j):
    """Cuántos píxeles del contenido de la ventana de la prueba no son el verde liso."""
    x0, y0 = j["x"] + BORDA, j["y"] + BARRA
    n = 0
    for y in range(y0 + 2, y0 + j["a"] - 2):
        fila = rgb[(y * w + x0 + 2) * 3:(y * w + x0 + j["l"] - 2) * 3]
        for i in range(0, len(fila), 3):
            if tuple(fila[i:i + 3]) != VERDE:
                n += 1
    return n


def cursor():
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h230-cursor", "ide", imagem=img) as m:
        h228.terminal(m)
        marca = len(m.serial())
        m.digitar("/testes/sem-cursor\n", 0.04)
        esperar(m, r"sem-cursor: cursor oculto", 30, desde=marca)
        time.sleep(1)
        j = janela(m.serial().replace("\r", ""), "Sem cursor")
        cx, cy = j["x"] + BORDA + j["l"] // 2, j["y"] + BARRA + j["a"] // 2
        # Un clic dentro: da el foco y dice dónde está de verdad la flecha.
        m.clique(cx, cy)
        g = esperar(m, r"sem-cursor: clique em (\d+) (\d+)", 10, desde=marca)
        px, py = j["x"] + BORDA + int(g[0]), j["y"] + BARRA + int(g[1])
        esperar(m, r"\[GUI\] cursor oculto sobre a janela", 10, desde=marca)
        time.sleep(0.5)
        w, h, rgb = h228.pantalla(m, "h230-cursor-dentro")
        m.captura("H230-8-cursor-oculto", guardar=True)
        verificar(nao_verdes(rgb, w, j) == 0 and not tem_seta(rgb, w, h, px, py),
                  "sobre la ventana con JAN_SEM_CURSOR el puntero del sistema no se dibuja")

        # Fuera de la ventana (a su derecha, o debajo si no cabe), la flecha se ve.
        fx = j["x"] + BORDA + j["l"] + 60
        fy = py
        if fx >= w - 20:
            fx, fy = px, j["y"] + BARRA + j["a"] + 40
        m.mouse_mover(fx, fy)
        time.sleep(0.5)
        w, h, rgb = h228.pantalla(m, "h230-cursor-fora")
        m.captura("H230-9-cursor-fuera", guardar=True)
        verificar(tem_seta(rgb, w, h, fx, fy, 12), "fuera de la ventana el puntero del sistema se ve")

        # Dentro otra vez, con un menú contextual abierto: se ve.
        m.mouse_mover(px, py)
        time.sleep(0.5)
        w, h, rgb = h228.pantalla(m, "h230-cursor-volta")
        verificar(not tem_seta(rgb, w, h, px, py), "al volver sobre la ventana se oculta otra vez")
        desde = len(m.serial())
        m.tecla("m")
        esperar(m, r"sem-cursor: menu aberto", 10, desde=desde)
        esperar(m, r"\[GUI\] cursor visivel", 10, desde=desde)
        time.sleep(0.5)
        w, h, rgb = h228.pantalla(m, "h230-cursor-menu")
        verificar(tem_seta(rgb, w, h, px, py), "con un menú abierto el puntero se ve sobre la ventana")
        m.tecla("esc")
        time.sleep(1)

        # «v»: el programa lo vuelve a pedir visible; «o»: oculto.
        desde = len(m.serial())
        m.tecla("v")
        esperar(m, r"sem-cursor: cursor visivel", 10, desde=desde)
        time.sleep(0.5)
        w, h, rgb = h228.pantalla(m, "h230-cursor-v")
        verificar(tem_seta(rgb, w, h, px, py), "cursor_janela(id, true): el puntero vuelve a verse")
        m.tecla("o")
        esperar(m, r"sem-cursor: cursor oculto", 10, desde=desde)
        time.sleep(0.5)
        w, h, rgb = h228.pantalla(m, "h230-cursor-o")
        verificar(not tem_seta(rgb, w, h, px, py), "cursor_janela(id, false): se oculta otra vez")

        # Al cerrar la ventana, la flecha vuelve donde estaba.
        desde = len(m.serial())
        m.tecla("q")
        esperar(m, r"sem-cursor: fim", 10, desde=desde)
        time.sleep(1)
        w, h, rgb = h228.pantalla(m, "h230-cursor-fim")
        m.captura("H230-10-cursor-al-cerrar", guardar=True)
        verificar(tem_seta(rgb, w, h, px, py), "al cerrar la ventana el puntero del sistema vuelve")
        s = m.serial()[marca:]
        verificar("Fallo de página" not in s, "sin faltas de página")


PARTES = {"unidad": unidad, "juego": juego, "sin-datos": sin_datos, "cursor": cursor}


def main():
    os.makedirs(SAIDA, exist_ok=True)
    pedidas = sys.argv[1:] or list(PARTES)
    for p in pedidas:
        if p not in PARTES:
            print("uso: h230_conquer.py [%s]" % " ".join(PARTES))
            return 2
    for p in pedidas:
        print("== %s" % p)
        PARTES[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
