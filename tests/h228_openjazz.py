#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H228 - OpenJazz (el motor libre de Jazz Jackrabbit, C++) dentro del SaguiOS (D228).

Partes (por defecto, todas; o las que se pidan: h228_openjazz.py unidad juego sin-datos):
  unidad     en el PC: el runtime de C++ (userland/libcxx/rt) con tests/cxxrt_unidad.cpp, a
             -Os, -O0 y -O2: throw/catch en el mismo cuadro y a través de cuadros con
             destructores, relanzar, catch (...), clase derivada por su base, un new cuyo
             constructor lanza (se libera la memoria), excepciones anidadas, punteros,
             argumentos empujados (DW_CFA_GNU_args_size), pila realineada (CFI con
             expresiones), un catch que no coincide, y sin fugas; una excepción sin
             capturar termina con std::terminate (código 134). Sin compilador de C++ se salta.
  juego      la imagen con SAGUI-JOGOS (/jogos) y SAGUI-APPS (/apps) y la tarjeta de sonido:
             Inicio > Juegos tiene «OpenJazz»; desde el menú abre su ventana de 640x400
             (320x200 al doble) con los datos de /jogos (el juego completo de input_share o
             el shareware de tools/openjazz_datos.sh); se salta la escena de entrada y se ve
             el menú (captura); «load game» (que lanza excepciones de C++ por cada partida
             que no existe) no lo tumba; se empieza una partida (New game, episodio 1,
             dificultad), Jazz se mueve con la flecha derecha mantenida (la pantalla cambia
             mucho más que quieta); suena (la música del menú en el .wav del QEMU); se sale
             cerrando la ventana sin faltas de página ni std::terminate; la configuración
             queda en /home/openjazz.
             Sin datos de Jazz Jackrabbit en build/jogos/raiz, se salta con un aviso.
  sin-datos  la imagen sin datos de Jazz: el lanzador abre su ventana de ayuda («faltan
             los datos») en vez de fallar.
  cheia      «OpenJazz (pantalla completa)»: la ventana pasa a pantalla completa.
Capturas en docs/capturas/H228-*.png.
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

JOGOS = os.path.join(BUILD, "jogos", "raiz")   # la raíz de SAGUI-JOGOS que monta el make (D228)
OPENJAZZ = os.path.join(APPS_RAIZ, "bin", "openjazz")


# ------------------------------------------------------------------ unidad

def compilador_cxx():
    """El g++ del entorno (env.sh); None si no hay compilador de C++."""
    for c in ("g++",):
        r = subprocess.run([c, "-m32", "-x", "c++", "-fsyntax-only", "/dev/null"], capture_output=True)
        if r.returncode == 0:
            return c
    return None


def unidad():
    cxx = compilador_cxx()
    if not cxx:
        print("AVISO: sin compilador de C++ (tools/entorno-local.sh): se salta la parte «unidad»")
        return
    rt = os.path.join(RAIZ, "userland", "libcxx", "rt")
    inc = os.path.join(RAIZ, "userland", "libcxx", "include")
    gi = subprocess.run(["gcc", "-print-file-name=include"], capture_output=True, text=True).stdout.strip()
    env = dict(os.environ, CPATH="", C_INCLUDE_PATH="", CPLUS_INCLUDE_PATH="")
    for opt in ("-Os", "-O0", "-O2"):
        d = tempfile.mkdtemp(prefix="h228-", dir=SAIDA if os.path.isdir(SAIDA) else None)
        try:
            base = ["-m32", "-march=i686", opt, "-ffreestanding", "-fno-pic", "-fno-pie", "-fno-stack-protector",
                    "-nostdinc", "-isystem", gi, "-I", rt, "-I", inc]
            cxxf = base + ["-std=gnu++17", "-nostdinc++", "-fexceptions", "-fno-rtti"]
            objs = []
            for f, comp, fl in (("desenrolar.c", "gcc", base + ["-std=gnu11"]), ("abi_tipos.c", "gcc", base + ["-std=gnu11"]),
                                ("excecoes.cpp", cxx, cxxf), ("abi.cpp", cxx, cxxf)):
                o = os.path.join(d, f + ".o")
                subprocess.run([comp, *fl, "-c", os.path.join(rt, f), "-o", o], check=True, env=env)
                objs.append(o)
            o = os.path.join(d, "prueba.o")
            subprocess.run([cxx, *cxxf, "-c", os.path.join(RAIZ, "tests", "cxxrt_unidad.cpp"), "-o", o], check=True, env=env)
            elf = os.path.join(d, "prueba")
            subprocess.run(["ld", "-m", "elf_i386", "-nostdlib", "-T", os.path.join(RAIZ, "userland", "programa_cxx.ld"),
                            "--gc-sections", "-z", "noexecstack", "--no-warn-rwx-segments", "-o", elf, o, *objs], check=True)
            r = subprocess.run([elf], capture_output=True, text=True, timeout=30)
            print(r.stdout.strip())
            verificar(r.returncode == 0 and "FIN 0 fallos" in r.stdout,
                      f"unidad {opt}: las excepciones de C++ funcionan (runtime propio)")
            r = subprocess.run([elf, "sin-captura"], capture_output=True, text=True, timeout=30)
            verificar(r.returncode == 134 and "std::terminate" in r.stderr,
                      f"unidad {opt}: una excepción sin capturar acaba en std::terminate (134)")
        finally:
            shutil.rmtree(d, ignore_errors=True)


# ------------------------------------------------------------------ imágenes

def tem_dados_jazz(pasta):
    """La carpeta de /jogos que tiene datos de Jazz Jackrabbit (o None)."""
    if not os.path.isdir(pasta):
        return None
    for n in sorted(os.listdir(pasta)):
        p = os.path.join(pasta, n)
        if os.path.isdir(p) and any(os.path.exists(os.path.join(p, f)) for f in ("MAINCHAR.000", "mainchar.000")):
            return n
    return None


def jogos_sem_jazz():
    """Una copia de la raíz de SAGUI-JOGOS sin ningún dato de Jazz Jackrabbit."""
    d = os.path.join(SAIDA, "h228-jogos-sin-jazz")
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    for n in os.listdir(JOGOS):
        p = os.path.join(JOGOS, n)
        if os.path.isdir(p) and n in ("chip8", "sagui3d", "jclic", "opentyrian"):
            shutil.copytree(p, os.path.join(d, n))
    os.makedirs(os.path.join(d, "openjazz"))
    return d


def jogos_so_jazz():
    """La raíz de SAGUI-JOGOS solo con los juegos de la base, OpenTyrian y la carpeta de
    Jazz Jackrabbit: input_share/games puede traer otros juegos de muchos MB (o GB) que
    esta prueba no necesita. Con enlaces duros: no se copia nada."""
    carpeta = tem_dados_jazz(JOGOS)
    d = os.path.join(SAIDA, "h228-jogos-jazz")
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    for n in ("chip8", "sagui3d", "jclic", "opentyrian", "openjazz", carpeta):
        p = os.path.join(JOGOS, n)
        if n and os.path.isdir(p) and not os.path.exists(os.path.join(d, n)):
            shutil.copytree(p, os.path.join(d, n), copy_function=os.link)
    return d


def imagem(jogos):
    return imagem_de_teste("", ["--vesa", "--jogos", jogos, "--apps", APPS_RAIZ])


def terminal(m):
    esperar(m, r"sagui:/\$", 150)
    time.sleep(4)
    h136.focar_terminal(m)


def abrir_pelo_menu(m, entrada="OpenJazz", capturar=None):
    marca = len(m.serial())
    m.clique(40, 12)
    p = painel(m, marca)
    sub = submenu_pelo_mouse(m, p, "Juegos", entrada)
    verificar("OpenJazz" in sub["linhas"] and "OpenTyrian" in sub["linhas"],
              "Inicio > Juegos tiene OpenJazz junto a OpenTyrian (%s)" % ", ".join(sub["linhas"]))
    time.sleep(0.4)
    if capturar:
        m.captura(capturar)
    marca = len(m.serial())
    clicar_aqui(m)
    esperar(m, r"inicio -> /apps/bin/openjazz", desde=marca)
    return marca


def pantalla(m, nome):
    """La pantalla del QEMU (ancho, alto, RGB)."""
    ppm = os.path.join(SAIDA, nome + ".ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor("screendump " + ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    r = h136.leer_ppm(ppm)
    os.remove(ppm)
    return r


def janela_jogo(m):
    s = m.serial().replace("\r", "")
    j = re.findall(r'janela \d+ criada por \d+: "OpenJazz" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
    verificar(bool(j), "la ventana de OpenJazz está en el registro")
    return tuple(map(int, j[-1]))


def area(m, nome, so_painel=False):
    """Los píxeles del interior de la ventana del juego (muestreados de 2 en 2).
    so_painel: solo la franja de abajo (el marcador de la partida: 2 x 30 px)."""
    larg, alto, x0, y0 = janela_jogo(m)
    w, h, rgb = pantalla(m, nome)
    px = []
    ya = y0 + alto - 2 * 30 + 4 if so_painel else y0 + 4
    for y in range(ya, min(y0 + alto, h) - 4, 2):
        fila = rgb[(y * w + x0 + 4) * 3:(y * w + min(x0 + larg, w) - 4) * 3]
        px.extend(fila[i:i + 3] for i in range(0, len(fila), 6))
    return px


def diferentes(a, b):
    n = min(len(a), len(b))
    return 100.0 * sum(1 for i in range(n) if a[i] != b[i]) / max(n, 1)


def focar_jogo(m):
    larg, alto, x0, y0 = janela_jogo(m)
    m.clique(x0 + larg // 2, y0 + alto // 2)
    time.sleep(0.5)


def sem_falhas(m, desde, rotulo):
    s = m.serial()[desde:]
    verificar("Fallo de página" not in s and "Fallo de p" not in s, f"{rotulo}: sin faltas de página")
    verificar("std::terminate" not in s and "[CXXRT]" not in s, f"{rotulo}: sin std::terminate ni avisos del runtime de C++")


# ------------------------------------------------------------------ juego

def juego():
    if not os.path.isfile(OPENJAZZ):
        raise AssertionError("falta %s (make: hace falta el compilador de C++, tools/entorno-local.sh)" % OPENJAZZ)
    carpeta = tem_dados_jazz(JOGOS)
    if not carpeta:
        print("AVISO: no hay datos de Jazz Jackrabbit en %s (tools/openjazz_datos.sh o input_share/games): "
              "se salta la parte «juego»" % JOGOS)
        return
    print("datos de Jazz Jackrabbit en /jogos/%s" % carpeta)
    img = imagem(jogos_so_jazz())
    with Maquina("h228-juego", "ide", imagem=img, som="hda-output") as m:
        terminal(m)
        s = m.serial().replace("\r", "")
        verificar(re.search(r"\[FSTAB\] LABEL=SAGUI-JOGOS -> /jogos \(auto, rw\): 0", s),
                  "el /etc/fstab monta SAGUI-JOGOS en /jogos (lectura y escritura)")
        marca = abrir_pelo_menu(m, capturar="H228-1-menu-juegos")
        esperar(m, r"\[OPENJAZZ\] datos en /jogos/\S+", 60, desde=marca)
        esperar(m, r"\[OPENJAZZ\] ventana 640x400 creada \(superficie 320x200, 8 bits\)", 90, desde=marca)
        verificar(True, "OpenJazz abre su ventana de 640x400 (320x200 de 8 bits, al doble)")
        esperar(m, r"\[OPENJAZZ\] primer cuadro dibujado", 60, desde=marca)
        esperar(m, r"\[OPENJAZZ\] audio 44100 Hz, 2 canales, 16 bits", 30, desde=marca)
        verificar(True, "abre el sonido (44100 Hz, estéreo, 16 bits)")
        time.sleep(4)
        m.captura("H228-2-escena", guardar=True)
        # La escena de entrada (STARTUP.0SC) se salta con Esc; queda el menú principal.
        focar_jogo(m)
        m.tecla("esc")
        time.sleep(4)
        a = area(m, "h228-menu")
        cores = len(set(a))
        verificar(cores > 8, f"el menú principal se ve ({cores} colores)")
        m.captura("H228-3-menu", guardar=True)
        sem_falhas(m, marca, "hasta el menú")

        # «load game»: el motor intenta abrir cada partida guardada y, si no existe,
        # el constructor de File lanza una excepción de C++ que recoge el menú.
        m.tecla("down")
        time.sleep(0.6)
        m.tecla("ret")
        time.sleep(3)
        m.captura("H228-4-cargar", guardar=True)
        m.tecla("esc")
        time.sleep(2)
        m.tecla("up")
        time.sleep(0.6)
        sem_falhas(m, marca, "el menú de cargar (excepciones de C++)")

        # Partida: New game -> episodio 1 -> dificultad.
        for _ in range(3):
            m.tecla("ret")
            time.sleep(3)
        time.sleep(6)          # carga del nivel
        m.tecla("ret")         # por si queda el cartel del planeta
        time.sleep(3)
        quieto1 = area(m, "h228-quieto1")
        time.sleep(1.5)
        quieto2 = area(m, "h228-quieto2")
        painel1 = area(m, "h228-painel1", so_painel=True)
        m.monitor("sendkey right 1500")
        time.sleep(1.8)
        movido = area(m, "h228-movido")
        painel2 = area(m, "h228-painel2", so_painel=True)
        # Si fuera la demostración (sale sola si se espera en el menú), la tecla la
        # cortaría y volvería el menú: el marcador de abajo desaparecería.
        d_painel = diferentes(painel1, painel2)
        print("    marcador: cambio %.1f%%" % d_painel)
        verificar(len(set(painel2)) > 8 and d_painel < 40,
                  "es una partida de verdad (el marcador sigue ahí tras mover a Jazz), no la demostración")
        d_quieto, d_movido = diferentes(quieto1, quieto2), diferentes(quieto2, movido)
        print("    cambio quieto %.1f%%, con la flecha derecha %.1f%%" % (d_quieto, d_movido))
        verificar(d_movido > 20 and d_movido > d_quieto + 10,
                  "Jazz se mueve con la flecha derecha: la pantalla cambia mucho más que quieta")
        m.captura("H228-5-partida", guardar=True)
        sem_falhas(m, marca, "la partida")

        # Salir: cerrar la ventana (SDL_QUIT).
        larg, alto, x0, y0 = janela_jogo(m)
        desde = len(m.serial())
        m.monitor("sendkey alt-f4")
        try:
            esperar(m, r"\[OPENJAZZ\] fin \(", 30, desde=desde)
        except AssertionError:
            # Sin Alt+F4 en el gestor de ventanas: Esc hasta el menú y «exit».
            for _ in range(3):
                m.tecla("esc")
                time.sleep(1.5)
            for _ in range(5):
                m.tecla("down")
                time.sleep(0.4)
            m.tecla("ret")
            esperar(m, r"\[OPENJAZZ\] fin \(", 30, desde=desde)
        verificar(True, "OpenJazz sale y vuelve el control al sistema")
        sem_falhas(m, marca, "toda la sesión")
        time.sleep(2)
        h136.focar_terminal(m)
        out = m.comando("ls /home/openjazz")
        verificar("openjazz.cfg" in out.lower(), f"la configuración queda en /home/openjazz ({out.strip()[-120:]})")
    esq, dir_ = h70.ler_gravacao(m.wav)
    rms = h70.rms(esq[:48000 * 30])
    verificar(rms > 100, f"suena: la música del menú llega a la tarjeta (RMS {rms:.0f})")


# ------------------------------------------------------------------ sin datos

def sin_datos():
    if not os.path.isfile(OPENJAZZ):
        raise AssertionError("falta %s" % OPENJAZZ)
    img = imagem(jogos_sem_jazz())
    with Maquina("h228-sin", "ide", imagem=img) as m:
        terminal(m)
        marca = abrir_pelo_menu(m)
        esperar(m, r"\[OPENJAZZ\] faltan los datos: ventana de ayuda", 60, desde=marca)
        time.sleep(2)
        j = janela(m.serial(), "OpenJazz: faltan los datos")
        verificar(j["l"] >= 600, "sin datos sale la ventana que explica cómo conseguirlos (%dx%d)" % (j["l"], j["a"]))
        m.captura("H228-6-sin-datos", guardar=True)
        m.clique(j["x"] + 340 + 55, j["y"] + 24 + 244 + 14)   # «Cerrar»
        time.sleep(2)
        s = m.serial()[marca:]
        verificar("ventana 640x400 creada" not in s, "sin datos el juego no arranca")
        verificar("Fallo de página" not in s, "sin datos no hay faltas de página")


# ------------------------------------------------------------------ pantalla completa

def cheia():
    if not tem_dados_jazz(JOGOS) or not os.path.isfile(OPENJAZZ):
        print("AVISO: sin datos de Jazz Jackrabbit o sin OpenJazz: se salta la parte «cheia»")
        return
    img = imagem(jogos_so_jazz())
    with Maquina("h228-cheia", "ide", imagem=img) as m:
        terminal(m)
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        submenu_pelo_mouse(m, p, "Juegos", "OpenJazz (pantalla completa)")
        time.sleep(0.4)
        marca = len(m.serial())
        clicar_aqui(m)
        esperar(m, r"\[OPENJAZZ\] pantalla completa: activada", 90, desde=marca)
        verificar(True, "«OpenJazz (pantalla completa)» abre a pantalla completa")
        time.sleep(5)
        m.captura("H228-7-pantalla-completa", guardar=True)
        sem_falhas(m, marca, "pantalla completa")


PARTES = {"unidad": unidad, "juego": juego, "sin-datos": sin_datos, "cheia": cheia}


def main():
    os.makedirs(SAIDA, exist_ok=True)
    pedidas = sys.argv[1:] or list(PARTES)
    for p in pedidas:
        if p not in PARTES:
            print("uso: h228_openjazz.py [%s]" % " ".join(PARTES))
            return 2
    for p in pedidas:
        print("== %s" % p)
        PARTES[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
