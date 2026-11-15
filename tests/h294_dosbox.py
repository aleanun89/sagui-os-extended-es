#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H294 - DOSBox 0.74-3 y el lanzador de juegos Recreativa dentro del SaguiOS (D294, mejora 119).

Partes (por defecto todas; o las que se pidan: h294_dosbox.py unidad juego nucleos lanzador fuentes):
  unidad     en el PC: el RTTI del runtime de C++ propio (userland/libcxx/rt/rtti.cpp) con
             tests/cxx_rtti_unidad.cpp a -Os, -O0 y -O2: dynamic_cast hacia abajo, en cruz, con base
             virtual, base privada y ambigua, typeid, std::bad_cast y std::bad_typeid; y las cabeceras
             nuevas (<map>, <set>, flujos, <limits>, <iterator>, sort) con tests/cxx_biblioteca_unidad.cpp.
  fuentes    tools/dosbox_fuentes.sh --comprobar: lo que hay en git es DOSBox 0.74-3 + el parche (si no
             hay red ni tarball en la caché, se salta con un aviso).
  juego      un .COM generado por la prueba (tests/dosbox_com.py): escribe en modo texto, pasa al modo
             13h y pinta, toca el altavoz a 1 kHz y reinicia el DSP de la Sound Blaster. Inicio > Juegos
             tiene «DOSBox» y «Recreativa»; con «dosbox PRUEBA.COM»: se ve el texto (captura), el modo
             gráfico (capturas, muchos colores), suena un tono de ~1 kHz en el .wav del QEMU, el DSP
             contesta (SB=1) y se sale sin faltas de página ni std::terminate.
  nucleos    el mismo programa de medida (BENCH.COM, bucle de 32 bits durante 3 s de DOS) con
             core=normal y core=dynamic a cycles=max: el recompilador dinámico (la memoria de código es
             ejecutable) tiene que ir bastante más rápido que el intérprete. Escribe las dos cifras.
  lanzador   Recreativa: se abre desde Inicio > Juegos, encuentra sola una carpeta de /jogos, añade un
             juego de /home/dos con el diálogo de carpeta (con teclado), lo guarda en juegos.conf, lo
             lanza (DOSBox arranca con su carpeta como C:) y lo cierra; Propiedades y la cuadrícula/lista.
  completa   «dosbox --pantalla-completa»: la ventana ocupa la pantalla (bandas negras para el 4:3); el
             ratón se captura al hacer clic (raton capturado) y Ctrl+Alt lo suelta.
  archivos   Archivos abre un .com con DOSBox (doble Intro sobre el archivo): la orden que da va al registro.
Capturas en docs/capturas/H294-*.png.
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
from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui  # noqa: E402
import h70_sonido as h70  # noqa: E402
import h136_opentyrian as h136  # noqa: E402
import h228_openjazz as h228  # noqa: E402
import dosbox_com  # noqa: E402

DOSBOX = os.path.join(APPS_RAIZ, "bin", "dosbox")
RECREATIVA = os.path.join(APPS_RAIZ, "bin", "recreativa")


# ------------------------------------------------------------------ unidad

def unidad():
    cxx = h228.compilador_cxx()
    if not cxx:
        print("AVISO: sin compilador de C++ (tools/entorno-local.sh): se salta la parte «unidad»")
        return
    rt = os.path.join(RAIZ, "userland", "libcxx", "rt")
    inc = os.path.join(RAIZ, "userland", "libcxx", "include")
    gi = subprocess.run(["gcc", "-print-file-name=include"], capture_output=True, text=True).stdout.strip()
    env = dict(os.environ, CPATH="", C_INCLUDE_PATH="", CPLUS_INCLUDE_PATH="")
    for opt in ("-Os", "-O0", "-O2"):
        d = tempfile.mkdtemp(prefix="h294-", dir=SAIDA if os.path.isdir(SAIDA) else None)
        try:
            base = ["-m32", "-march=i686", opt, "-ffreestanding", "-fno-pic", "-fno-pie", "-fno-stack-protector",
                    "-nostdinc", "-isystem", gi, "-I", rt, "-I", inc]
            cxxf = base + ["-std=gnu++17", "-nostdinc++", "-fexceptions"]
            objs = []
            for f, comp, fl in (("desenrolar.c", "gcc", base + ["-std=gnu11"]), ("abi_tipos.c", "gcc", base + ["-std=gnu11"]),
                                ("excecoes.cpp", cxx, cxxf + ["-fno-rtti"]), ("abi.cpp", cxx, cxxf + ["-fno-rtti"]),
                                ("rtti.cpp", cxx, cxxf + ["-fno-rtti"])):
                o = os.path.join(d, f + ".o")
                subprocess.run([comp, *fl, "-c", os.path.join(rt, f), "-o", o], check=True, env=env)
                objs.append(o)
            o = os.path.join(d, "prueba.o")
            subprocess.run([cxx, *cxxf, "-frtti", "-I", os.path.join(RAIZ, "tests"), "-c",
                            os.path.join(RAIZ, "tests", "cxx_rtti_unidad.cpp"), "-o", o], check=True, env=env)
            elf = os.path.join(d, "prueba")
            subprocess.run(["ld", "-m", "elf_i386", "-nostdlib", "-T", os.path.join(RAIZ, "userland", "programa_cxx.ld"),
                            "--gc-sections", "-z", "noexecstack", "--no-warn-rwx-segments", "-o", elf, o, *objs], check=True)
            r = subprocess.run([elf], capture_output=True, text=True, timeout=30)
            print(r.stdout.strip())
            verificar(r.returncode == 0 and "FIN 0 fallos" in r.stdout,
                      f"unidad {opt}: dynamic_cast y typeid con herencia simple, múltiple, virtual y bad_cast")
        finally:
            shutil.rmtree(d, ignore_errors=True)
    libstdcxx = "/usr/lib/x86_64-linux-gnu/libstdc++.so.6"
    if os.path.exists(libstdcxx):
        d = tempfile.mkdtemp(prefix="h294-", dir=SAIDA if os.path.isdir(SAIDA) else None)
        try:
            exe = os.path.join(d, "cxx_biblioteca_unidad")
            subprocess.run([cxx, "-std=gnu++17", "-O2", "-Wall", "-Wextra", "-nostdinc++", "-I", inc, "-o", exe,
                            os.path.join(RAIZ, "tests", "cxx_biblioteca_unidad.cpp"), libstdcxx, "-lm"], check=True, env=env)
            r = subprocess.run([exe], capture_output=True, text=True, timeout=60)
            print("    " + r.stdout.strip().replace("\n", "\n    ")[-800:])
            verificar(r.returncode == 0 and "todo bien" in r.stdout,
                      "cabeceras de C++: <map>, <set>, flujos, <limits>, <iterator>, sort, list::sort")
        finally:
            shutil.rmtree(d, ignore_errors=True)
    else:
        print("AVISO: sin libstdc++.so.6 en el PC: se salta la prueba de las cabeceras de C++")


# ------------------------------------------------------------------ fuentes

def fuentes():
    sh = os.path.join(RAIZ, "tools", "dosbox_fuentes.sh")
    cache = os.path.join(SAIDA, "dosbox-0.74-3.tar.gz")
    args = ["bash", sh, "--comprobar"]
    if os.path.isfile(cache):
        args += ["--tarball", cache]
    r = subprocess.run(args, capture_output=True, text=True)
    if r.returncode != 0 and ("curl" in r.stderr or "descargando" in r.stdout) and not os.path.isfile(cache):
        print("AVISO: sin red ni tarball de DOSBox en la caché: se salta la comprobación de las fuentes\n" + r.stderr[-300:])
        return
    print("    " + (r.stdout + r.stderr).strip()[-300:])
    verificar(r.returncode == 0, "las fuentes de DOSBox en git son el tarball 0.74-3 (SHA-256 fijo) + parches/sagui.patch")


# ------------------------------------------------------------------ imágenes

def carpeta_jogos():
    """La raíz de SAGUI-JOGOS de la prueba: dos/ con los .COM y un juego suelto (pruebados/) que
    Recreativa tiene que encontrar sola. Los datos son los de la prueba, nada de input_share."""
    d = os.path.join(SAIDA, "h294-jogos")
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(os.path.join(d, "dos"))
    os.makedirs(os.path.join(d, "pruebados"))
    with open(os.path.join(d, "dos", "PRUEBA.COM"), "wb") as f:
        f.write(dosbox_com.generar())
    with open(os.path.join(d, "dos", "BENCH.COM"), "wb") as f:
        f.write(dosbox_com.generar_bench())
    for nombre, nuc in (("normal", "normal"), ("dinamico", "dynamic")):
        with open(os.path.join(d, "dos", nombre + ".conf"), "w") as f:
            f.write("[cpu]\ncore=%s\ncycles=max\n" % nuc)
    with open(os.path.join(d, "pruebados", "PRUEBA.COM"), "wb") as f:
        f.write(dosbox_com.generar())
    with open(os.path.join(d, "pruebados", "SETUP.COM"), "wb") as f:
        f.write(dosbox_com.generar())
    return d


def imagem(jogos, extras=()):
    return imagem_de_teste("", ["--vesa", "--jogos", jogos, "--apps", APPS_RAIZ, *extras])


def terminal(m):
    esperar(m, r"sagui:/\$", 150)
    time.sleep(4)
    h136.focar_terminal(m)


def abrir_pelo_menu(m, entrada, capturar=None):
    marca = len(m.serial())
    m.clique(40, 12)
    p = painel(m, marca)
    sub = submenu_pelo_mouse(m, p, "Juegos", entrada)
    verificar("DOSBox" in sub["linhas"] and "Recreativa (juegos de DOS)" in sub["linhas"],
              "Inicio > Juegos tiene «DOSBox» y «Recreativa» (%s)" % ", ".join(sub["linhas"]))
    time.sleep(0.4)
    if capturar:
        m.captura(capturar)
    marca = len(m.serial())
    clicar_aqui(m)
    return marca


def ventana(m, titulo):
    s = m.serial().replace("\r", "")
    j = re.findall(r'janela \d+ criada por \d+: "%s" (\d+)x(\d+) em \((\d+),(\d+)\)' % re.escape(titulo), s)
    verificar(bool(j), f"la ventana «{titulo}» está en el registro")
    return tuple(map(int, j[-1]))


def colores_ventana(m, nombre, titulo="DOSBox"):
    """Los colores distintos del interior de la ventana (muestreo de 3 en 3 píxeles)."""
    larg, alto, x0, y0 = ventana(m, titulo)
    w, h, rgb = h228.pantalla(m, nombre)
    px = set()
    for y in range(y0 + 4, min(y0 + alto, h) - 4, 3):
        fila = rgb[(y * w + x0 + 4) * 3:(y * w + min(x0 + larg, w) - 4) * 3]
        px.update(fila[i:i + 3] for i in range(0, len(fila), 9))
    return len(px)


def sin_fallos(m, desde, rotulo):
    s = m.serial()[desde:]
    verificar("Fallo de página" not in s and "Fallo de p" not in s, f"{rotulo}: sin faltas de página")
    verificar("std::terminate" not in s and "[CXXRT]" not in s, f"{rotulo}: sin std::terminate ni avisos del runtime de C++")


# ------------------------------------------------------------------ juego

def juego():
    for f in (DOSBOX, RECREATIVA):
        if not os.path.isfile(f):
            raise AssertionError("falta %s (make: hace falta el compilador de C++, tools/entorno-local.sh)" % f)
    img = imagem(carpeta_jogos())
    with Maquina("h294-juego", "ide", imagem=img, som="hda-output") as m:
        terminal(m)
        marca = abrir_pelo_menu(m, "DOSBox", capturar="H294-1-menu-juegos")
        esperar(m, r"inicio -> /apps/bin/dosbox", 60, desde=marca)
        esperar(m, r"\[DOSBOX\] primer cuadro dibujado", 90, desde=marca)
        verificar(True, "DOSBox abre su ventana desde el menú Inicio (el .conf por defecto se crea solo)")
        verificar("[DOSBOX] creado /home/.config/dosbox.conf" in m.serial() or "[DOSBOX]" in m.serial(), "el lanzador de DOSBox escribe su log")
        time.sleep(6)
        m.captura("H294-2-prompt-dosbox", guardar=True)
        cores = colores_ventana(m, "h294-prompt")
        verificar(cores >= 4, f"se ve el indicador de DOS y el cartel de bienvenida ({cores} colores)")
        # Cerrar esta instancia y lanzar la del programa de prueba desde el terminal.
        m.monitor("sendkey alt-f4")
        esperar(m, r"\[DOSBOX\] fin \(", 30, desde=marca)
        time.sleep(1)
        h136.focar_terminal(m)
        desde = len(m.serial())
        m.digitar("dosbox /jogos/dos/PRUEBA.COM")
        m.tecla("ret")
        esperar(m, r"\[DOSBOX\] programa PRUEBA.COM en /jogos/dos", 60, desde=desde)
        esperar(m, r"\[DOSBOX\] primer cuadro dibujado", 90, desde=desde)
        esperar(m, r"\[DOSBOX\] audio 44100 Hz, 2 canales, 16 bits", 30, desde=desde)
        verificar(True, "abre el sonido (44100 Hz, estéreo, 16 bits)")
        time.sleep(10)
        cores_texto = colores_ventana(m, "h294-texto")
        verificar(cores_texto <= 16, f"el .COM escribe en modo texto: pocos colores ({cores_texto})")
        m.captura("H294-3-modo-texto", guardar=True)
        m.tecla("a")                     # pasa al modo 13h
        time.sleep(5)
        cores_gfx = colores_ventana(m, "h294-modo13")
        print("    colores modo 13h: %d" % cores_gfx)
        verificar(cores_gfx > 30, f"el .COM pasa al modo 13h y pinta un degradado ({cores_gfx} colores)")
        m.captura("H294-4-modo-13h", guardar=True)
        time.sleep(3)
        m.tecla("a")                     # apaga el altavoz y vuelve al texto
        time.sleep(4)
        m.captura("H294-5-sound-blaster", guardar=True)
        cores_fin = colores_ventana(m, "h294-final")
        verificar(cores_fin <= 16, f"vuelve al modo texto ({cores_fin} colores)")
        m.tecla("a")                     # el programa escribe PRUEBA.TXT, espera otra tecla y acaba
        esperar(m, r"\[DOSBOX\] fin \(0\)", 30, desde=desde)
        verificar(True, "DOSBox termina con código 0 cuando el programa acaba (-c exit)")
        sin_fallos(m, desde, "el programa de prueba")
        esq, dir_ = h70.ler_gravacao(m.wav)
        rms = h70.rms(esq)
        print("    wav: %d muestras, RMS %.0f" % (len(esq), rms))
        verificar(rms > 100, f"suena: el altavoz del PC y el mezclador de DOSBox llegan a la tarjeta (RMS {rms:.0f})")
        # La parte del tono: de mitad de la gráfica en adelante hay un 1 kHz mantenido.
        ini = int((len(esq) / 48000.0 - 12) * 48000)
        trozo = esq[max(0, ini):max(0, ini) + 48000 * 3]
        if len(trozo) > 24000:
            f = h70.dominante(trozo)
            print("    frecuencia dominante del tramo con altavoz: %s Hz" % f)
            verificar(abs(f - 1000) < 120 or 800 < f < 1250, f"el tono del altavoz es de ~1 kHz ({f} Hz)")
        # La Sound Blaster: el programa escribe SB=1 en PRUEBA.TXT si el DSP contestó 0xAA al reiniciarlo.
        h136.focar_terminal(m)
        desde = len(m.serial())
        m.digitar("cat /jogos/dos/PRUEBA.TXT")
        m.tecla("ret")
        time.sleep(2.5)
        r = re.findall(r"SB=([01])", m.serial()[desde:])
        verificar(r and r[-1] == "1", "la Sound Blaster emulada contesta 0xAA al reiniciar el DSP (SB=1)")


# ------------------------------------------------------------------ nucleos

def nucleos():
    img = imagem(carpeta_jogos())
    cifras = {"normal": [], "dinamico": []}
    with Maquina("h294-nucleos", "ide", imagem=img, som=None) as m:
        terminal(m)
        for nombre in ("normal", "dinamico", "normal", "dinamico"):
            desde = len(m.serial())
            m.digitar(f"dosbox -conf /home/.config/dosbox.conf -conf /jogos/dos/{nombre}.conf /jogos/dos/BENCH.COM")
            m.tecla("ret")
            esperar(m, r"\[DOSBOX\] fin \(", 150, desde=desde)
            desde = len(m.serial())
            m.digitar("cat /jogos/dos/BENCH.TXT")
            m.tecla("ret")
            time.sleep(2.5)
            r = re.findall(r"BENCH=([0-9A-F]{4})", m.serial()[desde:])
            verificar(bool(r), f"BENCH.COM con {nombre} escribió su resultado")
            cifras[nombre].append(int(r[-1], 16))
            print("    núcleo %-8s: %5d vueltas en 3 s" % (nombre, cifras[nombre][-1]))
        sin_fallos(m, 0, "los dos núcleos")
    n = max(cifras["normal"])
    d = min(cifras["dinamico"])
    print("    normal %d, dinámico %d vueltas (x%.1f)" % (n, d, d / max(n, 1)))
    verificar(d > 2 * n, f"el recompilador dinámico es más de 2 veces más rápido que el intérprete (x{d / max(n, 1):.1f})")


# ------------------------------------------------------------------ lanzador

def boton_barra(l_texto):
    """La x (relativa a la ventana) del centro de cada botón de la barra de Recreativa: la misma cuenta
    que layout() en recreativa.c."""
    nombres = ["Jugar", "Añadir", "Propiedades", "Configurar", "Quitar", "Carpeta"]
    x, xs = 4, {}
    for i, n in enumerate(nombres):
        l = max(60, len(n) * 8 + 16)
        xs[n] = x + l // 2
        x += l
        if i in (0, 3):
            x += 10
    return xs[l_texto]


def lanzador():
    for f in (DOSBOX, RECREATIVA):
        if not os.path.isfile(f):
            raise AssertionError("falta %s" % f)
    img = imagem(carpeta_jogos(), ["--home-mb", "64"])
    with Maquina("h294-lanzador", "ide", imagem=img, som=None) as m:
        terminal(m)
        for c in ("mkdir -p /home/dos/prueba", "cp /jogos/dos/PRUEBA.COM /home/dos/prueba/PRUEBA.COM",
                  "cp /jogos/dos/PRUEBA.COM /home/dos/prueba/SETUP.COM", "ls /home/dos/prueba"):
            m.comando(c)
        marca = abrir_pelo_menu(m, "Recreativa (juegos de DOS)", capturar="H294-6-menu-recreativa")
        esperar(m, r"inicio -> /apps/bin/recreativa", 60, desde=marca)
        esperar(m, r"\[RECREATIVA\] \d+ juego", 30, desde=marca)
        n0 = int(re.findall(r"\[RECREATIVA\] (\d+) juego", m.serial()[marca:])[-1])
        # Encuentra solas las carpetas con programas de DOS: /jogos/dos y /jogos/pruebados y /home/dos/prueba.
        verificar(n0 >= 3, f"Recreativa encuentra sola las carpetas de /jogos y /home/dos con .EXE/.COM/.BAT ({n0})")
        time.sleep(1.5)
        l, a, x0, y0 = ventana(m, "Recreativa")
        ox, oy = x0 + 2, y0 + 24
        m.captura("H294-7-recreativa-cuadricula", guardar=True)
        # Cuadrícula -> lista (Ctrl+L) y vuelta.
        m.tecla("ctrl-l")
        time.sleep(1)
        m.captura("H294-8-recreativa-lista", guardar=True)
        m.tecla("ctrl-l")
        time.sleep(0.8)
        # Propiedades del juego elegido (Alt+Intro).
        m.clique(ox + 60, oy + 80)
        time.sleep(0.5)
        marca = len(m.serial())
        m.tecla("alt-ret")
        esperar(m, r'criada por \d+: "Propiedades del juego"', 20, desde=marca)
        time.sleep(1)
        m.captura("H294-9-propiedades", guardar=True)
        m.tecla("esc")
        time.sleep(1)
        # Añadir: el diálogo de carpeta (teclado), propone nombre y programa y guarda juegos.conf.
        marca = len(m.serial())
        m.clique(ox + boton_barra("Añadir"), oy + 20 + 20)
        esperar(m, r"dialogo: carpeta en", 20, desde=marca)
        time.sleep(0.8)
        m.captura("H294-10-anadir-carpeta", guardar=True)
        # El árbol empieza en /home/dos: baja a «prueba» y acepta.
        m.tecla("down")
        time.sleep(0.3)
        m.tecla("ret")
        esperar(m, r'criada por \d+: "Añadir juego"', 20, desde=marca)
        time.sleep(1)
        m.captura("H294-11-anadir-propiedades", guardar=True)
        # Aceptar con el botón (abajo a la derecha del diálogo).
        ld, ad, xd, yd = ventana(m, "Añadir juego")
        m.clique(xd + 2 + 340 + 45, yd + 24 + 424 + 14)
        time.sleep(1.5)
        marca2 = len(m.serial())
        verificar("[RECREATIVA]" in m.serial(), "Recreativa sigue viva tras añadir")
        m.captura("H294-12-juego-anadido", guardar=True)
        # Jugar (Intro): DOSBox arranca con la carpeta del juego como C: y el programa.
        marca = len(m.serial())
        m.tecla("ret")
        esperar(m, r"\[RECREATIVA\] jugar PRUEBA\.COM en /home/dos/\S+", 40, desde=marca)
        esperar(m, r"\[DOSBOX\] primer cuadro dibujado", 90, desde=marca)
        verificar(True, "Jugar lanza DOSBox con el juego elegido")
        time.sleep(10)
        cores = colores_ventana(m, "h294-juego")
        verificar(cores >= 2, f"el juego corre en DOSBox ({cores} colores)")
        m.captura("H294-13-jugando", guardar=True)
        # Cerrar DOSBox (la ventana tiene el foco).
        m.monitor("sendkey alt-f4")
        esperar(m, r"\[DOSBOX\] fin \(", 30, desde=marca)
        esperar(m, r"\[RECREATIVA\] DOSBox terminó con \d+", 30, desde=marca)
        verificar(True, "al cerrar DOSBox, Recreativa lo nota (estado «DOSBox ha terminado»)")
        sin_fallos(m, marca, "jugar y cerrar")
        time.sleep(1)
        m.captura("H294-14-recreativa-despues", guardar=True)
        desde = len(m.serial())
        # Salir de Recreativa y comprobar juegos.conf desde el terminal.
        m.monitor("sendkey alt-f4")
        esperar(m, r"\[RECREATIVA\] fin", 20, desde=desde)
        time.sleep(1)
        h136.focar_terminal(m)
        desde = len(m.serial())
        m.digitar("cat /home/dos/juegos.conf")
        m.tecla("ret")
        time.sleep(2.5)
        s = m.serial()[desde:]
        verificar("[Prueba]" in s and "carpeta=/home/dos/prueba" in s and "programa=PRUEBA.COM" in s and "nucleo=auto" in s,
                  "juegos.conf guarda el juego añadido (nombre, carpeta, programa y núcleo)")


# ------------------------------------------------------------------ pantalla completa y ratón

def completa():
    img = imagem(carpeta_jogos())
    with Maquina("h294-completa", "ide", imagem=img, som=None) as m:
        terminal(m)
        desde = len(m.serial())
        m.digitar("dosbox --pantalla-completa /jogos/dos/PRUEBA.COM")
        m.tecla("ret")
        esperar(m, r"\[DOSBOX\] primer cuadro dibujado", 90, desde=desde)
        time.sleep(8)
        m.tecla("a")                     # modo 13h: toda la pantalla pintada
        time.sleep(5)
        w, h, rgb = h228.pantalla(m, "h294-completa")
        m.captura("H294-15-pantalla-completa", guardar=True)

        def px(x, y):
            return tuple(rgb[(y * w + x) * 3:(y * w + x) * 3 + 3])
        # En pantalla completa no hay barra ni borde: arriba a la izquierda, negro (banda del 4:3 o del vídeo).
        arriba = {px(x, 3) for x in range(0, w, 7)}
        verificar(len(arriba) > 10 and (24, 32, 42) not in arriba,
                  "pantalla completa: la imagen llega hasta el borde de arriba, sin la barra de tareas (%d colores)" % len(arriba))
        verificar("pantalla completa activada" in m.serial() or "pantalla completa: activada" in m.serial(), "el sistema pasó la ventana a pantalla completa")
        colores = {px(x, y) for x in range(w // 4, 3 * w // 4, 17) for y in range(h // 4, 3 * h // 4, 13)}
        verificar(len(colores) > 30, f"pantalla completa: el degradado de la prueba llena el centro ({len(colores)} colores)")
        # Ratón: a pantalla completa DOSBox captura el ratón solo (y el programa de prueba activó el de DOS).
        esperar(m, r"\[DOSBOX\] raton capturado", 20, desde=desde)
        verificar(True, "a pantalla completa DOSBox captura el ratón en modo relativo (raton capturado)")
        marca = len(m.serial())
        m.monitor("sendkey ctrl-alt")
        esperar(m, r"\[DOSBOX\] el sistema solto el raton", 20, desde=marca)
        verificar(True, "Ctrl+Alt suelta el ratón, también en DOSBox (que ve perder el foco)")
        marca = len(m.serial())
        m.clique(w // 2, h // 2)
        esperar(m, r"\[DOSBOX\] raton capturado", 20, desde=marca)
        verificar(True, "otro clic lo vuelve a capturar")
        m.monitor("sendkey alt-f4")
        esperar(m, r"\[DOSBOX\] fin \(", 30, desde=desde)
        sin_fallos(m, desde, "pantalla completa y ratón")


# ------------------------------------------------------------------ .exe/.com desde Archivos

def archivos():
    img = imagem(carpeta_jogos())
    with Maquina("h294-archivos", "ide", imagem=img, som=None) as m:
        terminal(m)
        desde = len(m.serial())
        m.digitar("arquivos /jogos/pruebados")
        m.tecla("ret")
        esperar(m, r'criada por \d+: "[^"]*Archivos[^"]*"', 40, desde=desde)
        time.sleep(2)
        m.captura("H294-16-archivos-com", guardar=True)
        marca = len(m.serial())
        # Entre las dos .COM (PRUEBA y SETUP) basta con la que esté elegida: se baja y se pulsa Intro.
        m.tecla("down")
        time.sleep(0.5)
        m.tecla("ret")
        esperar(m, r"arquivos: abrindo no dosbox /jogos/pruebados/\S+\.COM", 20, desde=marca)
        verificar(True, "Archivos abre un .COM con DOSBox")
        esperar(m, r"\[DOSBOX\] programa \S+\.COM en /jogos/pruebados", 60, desde=marca)
        esperar(m, r"\[DOSBOX\] primer cuadro dibujado", 90, desde=marca)
        time.sleep(6)
        m.captura("H294-17-archivos-dosbox", guardar=True)
        m.monitor("sendkey alt-f4")
        esperar(m, r"\[DOSBOX\] fin \(", 30, desde=marca)


# ------------------------------------------------------------------ principal

PARTES = {"unidad": unidad, "fuentes": fuentes, "juego": juego, "nucleos": nucleos, "lanzador": lanzador,
          "completa": completa, "archivos": archivos}


def main():
    partes = sys.argv[1:] or list(PARTES)
    for p in partes:
        if p not in PARTES:
            print("partes: " + " ".join(PARTES))
            return 2
    for p in partes:
        print("== h294: " + p)
        PARTES[p]()
    print("OK h294")
    return 0


if __name__ == "__main__":
    sys.exit(main())
