#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H321 - emuladores de consolas (LaiNES, Gearsystem) y Recreativa multisistema (D321).

Partes (por defecto todas; o las que se pidan: h321_emuladores.py unidad nes sms gg lanzador archivos):
  unidad     en el PC: las ROM de prueba generadas por tools/nes_rom_teste.py y tools/sms_rom_teste.py (cabecera
             iNES; «TMR SEGA» con su suma; tamaño); los .conf del registro de Recreativa (rootfs/etc/recreativa);
             las licencias (docs/terceros.md y los LICENSE de userland/terceros); y las fuentes fijadas
             (tools/laines_fuentes.sh y tools/gearsystem_fuentes.sh --comprobar; sin red ni tarball se saltan).
  nes, sms, gg
             un emulador con su ROM de prueba (generada aquí, no hay ninguna ROM comercial ni de terceros) en
             QEMU con KVM: la ventana tiene el tamaño esperado; los cuatro cuadrantes salen en sus colores; el
             sonido (el .wav del QEMU) es un tono de unos 1000 Hz; el mando de prueba de D307 (mandos
             --inyectar) mueve cosas (botón 1/A: el color 0 pasa a blanco; cruceta derecha: se desplaza la
             imagen); el segundo botón (SMS/GG) o Start (NES) escribe en la SRAM del cartucho, que al cerrar queda
             en /home/.config/<emulador>/ y la segunda ejecución la encuentra; velocidad (cuadros/s) y CPU.
  lanzador   Recreativa con DOS, NES, Master System y Game Gear: lista los sistemas, filtra por sistema, lanza un
             juego de cada uno, encuentra una carpeta de ROM añadida y no muestra emuladores sin binario.
  archivos   Archivos abre .nes, .sms y .gg con su emulador.
Capturas en docs/capturas/H321-*.png.
"""
import os
import re
import shutil
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA, APPS_RAIZ  # noqa: E402
from h60_barra_tareas import esperar  # noqa: E402
import h70_sonido as h70  # noqa: E402
import h136_opentyrian as h136  # noqa: E402
import h228_openjazz as h228  # noqa: E402
import h294_dosbox as h294  # noqa: E402
import dosbox_com  # noqa: E402

BIN = os.path.join(APPS_RAIZ, "bin")

# nombre: (ventana, contenido de la ventana (l, a), ROM, binario, carpeta de /jogos, prefijo del registro, SRAM, cuadrantes, punto del scroll)
COLORES_NES = [(0xF8, 0x38, 0x00), (0x58, 0xD8, 0x54), (0xF8, 0xB8, 0x00), (0x3C, 0xBC, 0xFC)]   # rojo, verde, amarillo, azul claro
COLORES_SMS = [(0xFF, 0x00, 0x00), (0x00, 0xFF, 0x00), (0xFF, 0xFF, 0x00), (0x00, 0x00, 0xFF)]
COLORES_GG = [(0xFF, 0x00, 0x00), (0x00, 0xFF, 0x00), (0xFF, 0xFF, 0x00), (0x00, 0x88, 0xFF)]
BLANCO = (0xFF, 0xFF, 0xFF)
SISTEMAS = {
    "nes": dict(titulo="LaiNES", tam=(512, 480), rom="prueba.nes", binario="laines", carpeta="nes", log="LAINES",
                sram="/home/.config/laines/prueba.sav", colores=COLORES_NES, scroll=(192, 120), boton2=1 << 9),
    "sms": dict(titulo="Gearsystem", tam=(512, 384), rom="prueba.sms", binario="gearsystem", carpeta="sms", log="GEARSYSTEM",
                sram="/home/.config/gearsystem/prueba.sav", colores=COLORES_SMS, scroll=(192, 96), boton2=1 << 1),
    "gg": dict(titulo="Gearsystem", tam=(480, 432), rom="prueba.gg", binario="gearsystem", carpeta="gg", log="GEARSYSTEM",
               sram="/home/.config/gearsystem/prueba.sav", colores=COLORES_GG, scroll=(120, 108), boton2=1 << 1),
}


# ------------------------------------------------------------------ utilidades

def generar_roms(d):
    """Las tres ROM de prueba en d/nes, d/sms y d/gg."""
    for sub in ("nes", "sms", "gg"):
        os.makedirs(os.path.join(d, sub), exist_ok=True)
    for args, destino in ((["tools/nes_rom_teste.py"], "nes/prueba.nes"), (["tools/sms_rom_teste.py"], "sms/prueba.sms"),
                          (["tools/sms_rom_teste.py", "--gg"], "gg/prueba.gg")):
        r = subprocess.run([sys.executable, "-I", os.path.join(RAIZ, args[0])] + args[1:] + [os.path.join(d, destino)],
                           capture_output=True, text=True, cwd=RAIZ)
        verificar(r.returncode == 0, "%s genera %s (%s)" % (args[0], destino, r.stdout.strip()))


def carpeta_jogos(dos=False):
    d = os.path.join(SAIDA, "h321-jogos")
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    generar_roms(d)
    if dos:
        os.makedirs(os.path.join(d, "dos"))
        with open(os.path.join(d, "dos", "PRUEBA.COM"), "wb") as f:
            f.write(dosbox_com.generar())
    return d


def imagem(jogos):
    return imagem_de_teste("", ["--vesa", "--jogos", jogos, "--apps", APPS_RAIZ, "--mandos-prueba", "--home-mb", "64"])


def da(**k):
    import h307_mandos as h307
    return h307.da(**k)


def inyectar_cmd(hexa):
    return "mandos --inyectar 0 %s" % hexa


def ventana_q(m, titulo):
    """(l, a, x, y) de la última ventana con ese título, sin ruido en la salida."""
    sr = m.serial().replace("\r", "")
    j = re.findall(r'janela \d+ criada por \d+: "%s" (\d+)x(\d+) em \((\d+),(\d+)\)' % re.escape(titulo), sr)
    verificar(bool(j), "") if not j else None
    return tuple(map(int, j[-1]))


def parecido(a, b, tol=22):
    return all(abs(p - q) <= tol for p, q in zip(a, b))


def hex3(c):
    return "%02x%02x%02x" % tuple(c)


def pixel(rgb, w, x, y):
    i = (y * w + x) * 3
    return tuple(rgb[i:i + 3])


def muestrear(m, nombre, titulo, puntos):
    """Los colores de la pantalla del QEMU en los puntos (x, y) relativos al contenido de la ventana `titulo`."""
    l, a, x0, y0 = ventana_q(m, titulo)
    w, h, rgb = h228.pantalla(m, nombre)
    return [pixel(rgb, w, x0 + 2 + x, y0 + 24 + y) for x, y in puntos]


def cuadrantes(m, nombre, s):
    l, a = s["tam"]
    return muestrear(m, nombre, s["titulo"], [(l // 4, a // 4), (3 * l // 4, a // 4), (l // 4, 3 * a // 4), (3 * l // 4, 3 * a // 4)])


def esperar_color(m, nombre, s, puntos, esperados, tiempo=25):
    """Espera a que los puntos tengan los colores esperados; devuelve (ok, colores)."""
    fin = time.time() + tiempo
    while True:
        cs = muestrear(m, nombre, s["titulo"], puntos)
        ok = all(parecido(c, e) for c, e in zip(cs, esperados))
        if ok or time.time() > fin:
            return ok, cs


def cerrar(m, marca, log):
    m.monitor("sendkey alt-f4")
    esperar(m, r"\[%s\] fin" % log, 30, desde=marca)


# ------------------------------------------------------------------ unidad

def unidad():
    d = os.path.join(SAIDA, "h321-unidad")
    shutil.rmtree(d, ignore_errors=True)
    os.makedirs(d)
    generar_roms(d)
    nes = open(os.path.join(d, "nes", "prueba.nes"), "rb").read()
    verificar(nes[:4] == b"NES\x1a" and nes[4] == 1 and nes[5] == 1 and nes[6] & 2 and nes[6] >> 4 == 1 and len(nes) == 16 + 0x4000 + 0x2000,
              "ROM de NES: cabecera iNES, MMC1 (mapper 1), 8 KB de CHR, batería, %d bytes" % len(nes))
    reset = nes[16 + 0x4000 - 4] | nes[16 + 0x4000 - 3] << 8
    verificar(0xC000 <= reset < 0xC100, "ROM de NES: el vector de RESET ($%04X) apunta al principio del PRG" % reset)
    for ext, gg in (("sms", False), ("gg", True)):
        r = open(os.path.join(d, ext, "prueba." + ext), "rb").read()
        verificar(len(r) == 0x10000 and r[0x7FF0:0x7FF8] == b"TMR SEGA", "ROM de %s: 64 KB con «TMR SEGA» en $7FF0" % ext)
        suma = (sum(r[:0x7FF0]) + sum(r[0x8000:])) & 0xFFFF
        verificar(r[0x7FFA] | r[0x7FFB] << 8 == suma, "ROM de %s: la suma de la cabecera es correcta ($%04X)" % (ext, suma))
        verificar(r[0x7FFF] >> 4 == (6 if gg else 4), "ROM de %s: la región de la cabecera" % ext)
    # El registro de Recreativa: cada .conf de sistema tiene su emulador, sus extensiones y carpetas.
    reg = os.path.join(RAIZ, "rootfs", "etc", "recreativa")
    for f, ext, emu in (("dos", None, "dosbox"), ("nes", "nes", "laines"), ("sms", "sms", "gearsystem"), ("gg", "gg", "gearsystem")):
        t = open(os.path.join(reg, "sistemas", f + ".conf"), encoding="utf-8").read()
        verificar("[emulador %s]" % emu in t and ("extensiones=" + ext in t if ext else True) and "binario=/apps/bin/%s" % emu in t,
                  "sistemas/%s.conf registra el emulador %s" % (f, emu))
    t = open(os.path.join(reg, "emuladores", "gnuboy.conf"), encoding="utf-8").read()
    verificar("sistemas=gb,gbc" in t and "binario=/apps/bin/gnuboy" in t and "solo_privado=si" in t,
              "emuladores/gnuboy.conf: alternativa de GB/GBC, solo en la imagen privada")
    # Licencias.
    terceros = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("LaiNES" in terceros and "BSD-2-Clause" in terceros and "Gearsystem" in terceros and "GPL-3.0" in terceros,
             "docs/terceros.md registra LaiNES (BSD-2-Clause) y Gearsystem (GPL-3.0)")
    verificar("BSD 2-Clause" in open(os.path.join(RAIZ, "userland/terceros/laines/jogo/LICENSE")).read(), "LaiNES: LICENSE BSD de 2 cláusulas")
    verificar("GNU GENERAL PUBLIC LICENSE" in open(os.path.join(RAIZ, "userland/terceros/gearsystem/jogo/LICENSE")).read().upper()
              and "Version 3" in open(os.path.join(RAIZ, "userland/terceros/gearsystem/jogo/LICENSE")).read(), "Gearsystem: LICENSE GPL v3")
    # SMS Plus no se usa (la licencia de su Z80 es no comercial): constancia en terceros.md.
    verificar("SMS Plus" in terceros, "terceros.md explica por qué no se usa SMS Plus")
    # Fuentes fijadas.
    for emu, tar in (("laines", None), ("gearsystem", None)):
        cache = os.path.join(SAIDA, emu + ".tar.gz")
        args = ["bash", os.path.join(RAIZ, "tools", emu + "_fuentes.sh"), "--comprobar"]
        if os.path.isfile(cache):
            args += ["--tarball", cache]
        r = subprocess.run(args, capture_output=True, text=True)
        if r.returncode != 0 and "descargando" in r.stdout and not os.path.isfile(cache):
            print("AVISO: sin red ni tarball de %s en la caché: se salta la comprobación de las fuentes" % emu)
            continue
        verificar(r.returncode == 0, "las fuentes de %s en git son el tarball fijado (SHA-256) + parches/sagui.patch" % emu)


# ------------------------------------------------------------------ un emulador

def consola(clave):
    s = SISTEMAS[clave]
    if not os.path.isfile(os.path.join(BIN, s["binario"])):
        raise AssertionError("falta %s (make: hace falta el compilador de C++)" % s["binario"])
    d = carpeta_jogos()
    img = imagem(d)
    rom = "/jogos/%s/%s" % (s["carpeta"], s["rom"])
    l, a = s["tam"]
    pt = [(l // 4, a // 4), (3 * l // 4, a // 4), (l // 4, 3 * a // 4), (3 * l // 4, 3 * a // 4)]
    with Maquina("h321-" + clave, "ide", imagem=img, som="hda-output") as m:
        h294.terminal(m)
        marca = len(m.serial())
        # Todo el guion del mando va en la misma línea de órdenes (la ventana del emulador toma el foco del
        # teclado al abrirse): botón A, soltar, cruceta derecha, soltar, el segundo botón (o Start), soltar.
        neutro = inyectar_cmd(da())
        guion = ("%s %s & sleep 40; %s; sleep 15; %s; sleep 8; %s; sleep 15; %s; sleep 6; %s; sleep 8; %s\n"
                 % (s["binario"], rom, inyectar_cmd(da(botoes=1 << 2)), neutro, inyectar_cmd(da(hat=2)), neutro,
                    inyectar_cmd(da(botoes=s["boton2"])), neutro))
        m.digitar(guion, 0.03)
        esperar(m, r"\[%s\] primer cuadro dibujado" % s["log"], 90, desde=marca)
        esperar(m, r"\[%s\] sonido abierto" % s["log"], 30, desde=marca)
        ll, aa, x0, y0 = ventana_q(m, s["titulo"])
        verificar((ll, aa) == (l, a), "%s: la ventana es de %dx%d (%dx%d)" % (clave, l, a, ll, aa))
        time.sleep(3)
        ok, cs = esperar_color(m, "h321-" + clave, s, pt, s["colores"], 30)
        m.captura("H321-%s-1-pantalla" % clave, guardar=True)
        verificar(ok, "%s: la ROM pinta los cuatro cuadrantes en sus colores: %s (esperados %s)"
                  % (clave, " ".join(hex3(c) for c in cs), " ".join(hex3(c) for c in s["colores"])))
        # Mando de prueba (D307): botón 3 del Dual Action = B del mapeo = A de la consola -> color 0 blanco.
        ok, cs = esperar_color(m, "h321-" + clave + "-a", s, [pt[3]], [BLANCO], 40)
        m.captura("H321-%s-2-boton-a" % clave, guardar=True)
        verificar(ok, "%s: el botón A del mando de prueba cambia el color 0 a blanco (%s)" % (clave, hex3(cs[0])))
        ok, cs = esperar_color(m, "h321-" + clave + "-a2", s, [pt[3]], [s["colores"][3]], 30)
        verificar(ok, "%s: al soltar el botón vuelve el color (%s)" % (clave, hex3(cs[0])))
        # Cruceta derecha (hat): la imagen se desplaza y el punto de muestra pasa de rojo a verde.
        ok, cs = esperar_color(m, "h321-" + clave + "-r", s, [s["scroll"]], [s["colores"][1]], 40)
        m.captura("H321-%s-3-derecha" % clave, guardar=True)
        verificar(ok, "%s: la cruceta del mando desplaza la imagen (%s, esperado %s)" % (clave, hex3(cs[0]), hex3(s["colores"][1])))
        ok, cs = esperar_color(m, "h321-" + clave + "-r2", s, [s["scroll"]], [s["colores"][0]], 30)
        verificar(ok, "%s: al soltar la cruceta la imagen vuelve (%s)" % (clave, hex3(cs[0])))
        # El segundo botón (SMS/GG) o Start (NES) escribió la marca en la RAM del cartucho: se espera a que acabe el guion.
        time.sleep(20)
        # Sonido: el .wav del QEMU.
        esq, dir_ = h70.ler_gravacao(m.wav)
        ventana = 4800
        rms = [h70.rms(esq[i:i + ventana]) for i in range(0, len(esq) - ventana, ventana)]
        maximo = max(rms) if rms else 0
        verificar(maximo > 50, "%s: suena (RMS máximo %.0f)" % (clave, maximo))
        activas = [k for k, v in enumerate(rms) if v > maximo * 0.3]
        verificar(len(activas) > 40, "%s: el tono suena muchas veces (%d ventanas de 100 ms)" % (clave, len(activas)))
        x = esq[activas[len(activas) // 2] * ventana:(activas[len(activas) // 2] + 1) * ventana]
        media = sum(x) / len(x)
        cruces = sum(1 for p, q in zip(x, x[1:]) if (p - media) * (q - media) < 0)
        f = cruces / 2 / (len(x) / 48000)
        verificar(abs(f - 1000) <= 60, "%s: el tono es de unos 1000 Hz (%.0f Hz por los cruces)" % (clave, f))
        # Velocidad.
        veloc = re.findall(r"\[%s\] ([\d.]+) cuadros/s, CPU (\d+)%%, (\d+) saltados" % s["log"], m.serial()[marca:])
        fps = [float(v[0]) for v in veloc]
        verificar(fps and min(fps) >= 49.0, "%s: velocidad correcta: %s cuadros/s (CPU %s%% en esta máquina)"
                  % (clave, " ".join("%.1f" % v for v in fps[:6]), "/".join(v[1] for v in veloc[:6])))
        sin_f = m.serial()[marca:]
        verificar("Fallo de p" not in sin_f and "std::terminate" not in sin_f, "%s: sin faltas de página ni std::terminate" % clave)
        cerrar(m, marca, s["log"])
        time.sleep(2)
        h136.focar_terminal(m)
        desde = len(m.serial())
        m.digitar("ls -l %s\n" % s["sram"], 0.03)
        time.sleep(3)
        verificar(re.search(r"\s\d{4,}\s[^\n]*prueba\.sav", m.serial()[desde:]) is not None, "%s: al cerrar queda la SRAM en %s" % (clave, s["sram"]))
        # Segunda ejecución: la ROM encuentra la marca y el cuadrante de abajo a la derecha sale del color 1 (rojo).
        marca = len(m.serial())
        m.digitar("%s %s &\n" % (s["binario"], rom), 0.03)
        esperar(m, r"\[%s\] primer cuadro dibujado" % s["log"], 90, desde=marca)
        time.sleep(3)
        esp = list(s["colores"])
        esp[3] = s["colores"][0]
        ok, cs = esperar_color(m, "h321-" + clave + "-2", s, pt, esp)
        m.captura("H321-%s-4-segunda" % clave, guardar=True)
        verificar(ok, "%s: la segunda ejecución encuentra la SRAM guardada (abajo a la derecha: %s)" % (clave, hex3(cs[3])))
        cerrar(m, marca, s["log"])


def nes():
    consola("nes")


def sms():
    consola("sms")


def gg():
    consola("gg")


# ------------------------------------------------------------------ Recreativa

def lanzador():
    for f in ("recreativa", "laines", "gearsystem", "dosbox"):
        if not os.path.isfile(os.path.join(BIN, f)):
            raise AssertionError("falta %s" % f)
    d = carpeta_jogos(dos=True)
    img = imagem(d)
    with Maquina("h321-lanzador", "ide", imagem=img, som=None) as m:
        h294.terminal(m)
        # Una carpeta de ROM «añadida» (roms.conf, como la deja el diálogo de carpeta): tiene otra ROM de NES.
        for c in ("mkdir -p /home/extra", "cp /jogos/nes/prueba.nes /home/extra/otra.nes", "mkdir -p /home/.config/recreativa",
                  "echo '[@/home/extra]' > /home/.config/recreativa/roms.conf", "echo 'sistema=nes' >> /home/.config/recreativa/roms.conf"):
            m.comando(c)
        marca = h294.abrir_pelo_menu(m, "Recreativa (juegos y consolas)", capturar="H321-lanzador-0-menu")
        esperar(m, r"inicio -> /apps/bin/recreativa", 60, desde=marca)
        esperar(m, r"\[RECREATIVA\] \d+ juego", 30, desde=marca)
        n0 = int(re.findall(r"\[RECREATIVA\] (\d+) juego", m.serial()[marca:])[-1])
        verificar(n0 == 5, "Recreativa encuentra sola la carpeta de DOS, una ROM de cada consola y la de la carpeta añadida (%d juegos)" % n0)
        time.sleep(1.5)
        ll, aa, x0, y0 = h294.ventana(m, "Recreativa")
        ox, oy = x0 + 2, y0 + 24
        m.captura("H321-lanzador-1-todos", guardar=True)
        fila0 = oy + 20 + 46 + 2 + 2 + 11      # centro de la primera fila de la lista de sistemas («Todos»)
        def fila(i):
            return fila0 + 22 * (i + 1)

        # Cada sistema, por la lista de la izquierda: 1 juego en DOS, NES, Master System y Game Gear.
        for i, (clave, nombre, n) in enumerate((("dos", "DOS", 1), ("nes", "NES", 2), ("sms", "Master System", 1), ("gg", "Game Gear", 1))):
            marca = len(m.serial())
            m.clique(ox + 60, fila(i))
            esperar(m, r"\[RECREATIVA\] sistema %s: %d juego" % (clave, n), 20, desde=marca)
            verificar(True, "la lista de sistemas filtra: %s -> %d juego(s)" % (nombre, n))
            time.sleep(0.8)
            m.captura("H321-lanzador-2-%s" % clave, guardar=True)
        # «Todos» otra vez.
        marca = len(m.serial())
        m.clique(ox + 60, fila0)
        esperar(m, r"\[RECREATIVA\] sistema todos: 5 juego", 20, desde=marca)

        # Lanzar un juego de cada sistema: sistema -> Intro -> el emulador arranca -> se cierra.
        for i, (clave, esperado) in enumerate((("nes", r"\[LAINES\] primer cuadro dibujado"), ("sms", r"\[GEARSYSTEM\] primer cuadro dibujado"),
                                               ("gg", r"\[GEARSYSTEM\] primer cuadro dibujado"), ("dos", r"\[DOSBOX\] primer cuadro dibujado"))):
            idx = {"dos": 0, "nes": 1, "sms": 2, "gg": 3}[clave]
            marca = len(m.serial())
            m.clique(ox + 60, fila(idx))
            esperar(m, r"\[RECREATIVA\] sistema %s" % clave, 20, desde=marca)
            time.sleep(0.8)
            m.clique(ox + 148 + 60, oy + 20 + 46 + 2 + 2 + 40)     # el primer juego de la cuadrícula
            time.sleep(0.5)
            m.tecla("ret")
            if clave == "dos":
                esperar(m, r"\[RECREATIVA\] jugar PRUEBA\.COM en /jogos/dos", 40, desde=marca)
            else:
                esperar(m, r"\[RECREATIVA\] jugar /jogos/%s/prueba\.%s con %s" % (clave, clave, "laines" if clave == "nes" else "gearsystem"),
                        40, desde=marca)
            esperar(m, esperado, 90, desde=marca)
            verificar(True, "Recreativa lanza el juego de %s" % clave)
            time.sleep(4)
            m.captura("H321-lanzador-3-jugando-%s" % clave, guardar=True)
            h294.sin_fallos(m, marca, "jugar " + clave)
            m.monitor("sendkey alt-f4")
            esperar(m, r"\[RECREATIVA\] (LaiNES|Gearsystem|DOSBox) terminó con \d+", 40, desde=marca)
            time.sleep(1)

        m.captura("H321-lanzador-4-final", guardar=True)
        # El registro: un emulador sin binario no se muestra; un sistema sin emulador, tampoco (aquí: ninguno de gb).
        verificar("sistema gb" not in m.serial(), "no hay sistema de Game Boy sin emulador instalado: no aparece")
        marca = len(m.serial())
        m.monitor("sendkey alt-f4")
        esperar(m, r"\[RECREATIVA\] fin", 30, desde=marca)


# ------------------------------------------------------------------ Archivos: Abrir

def archivos():
    d = carpeta_jogos()
    img = imagem(d)
    with Maquina("h321-archivos", "ide", imagem=img, som=None) as m:
        h294.terminal(m)
        for carpeta, prog, log in (("nes", "laines", "LAINES"), ("sms", "gearsystem", "GEARSYSTEM"), ("gg", "gearsystem", "GEARSYSTEM")):
            h136.focar_terminal(m)
            desde = len(m.serial())
            m.digitar("arquivos /jogos/%s\n" % carpeta, 0.03)
            esperar(m, r'criada por \d+: "[^"]*Archivos[^"]*"', 40, desde=desde)
            time.sleep(2)
            marca = len(m.serial())
            m.tecla("down")
            time.sleep(0.5)
            m.tecla("ret")
            esperar(m, r"arquivos: abrindo no %s /jogos/%s/prueba\.%s" % (prog, carpeta, carpeta), 20, desde=marca)
            esperar(m, r"\[%s\] primer cuadro dibujado" % log, 90, desde=marca)
            verificar(True, "Archivos abre la ROM .%s con %s" % (carpeta, prog))
            m.captura("H321-archivos-%s" % carpeta, guardar=True)
            m.monitor("sendkey alt-f4")
            esperar(m, r"\[%s\] fin" % log, 30, desde=marca)
            m.monitor("sendkey alt-f4")      # la ventana de Archivos
            time.sleep(2)


PARTES = {"unidad": unidad, "nes": nes, "sms": sms, "gg": gg, "lanzador": lanzador, "archivos": archivos}


def main():
    os.makedirs(SAIDA, exist_ok=True)
    partes = sys.argv[1:] or list(PARTES)
    for p in partes:
        if p not in PARTES:
            print("uso: h321_emuladores.py [%s]" % " ".join(PARTES))
            return 2
    for p in partes:
        print("== %s" % p)
        PARTES[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
