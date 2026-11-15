#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H322 - mGBA (GBA, Game Boy y Game Boy Color) dentro del SaguiOS (D322).

Partes (por defecto todas; o las que se pidan: h322_mgba.py unidad licencia os):
  unidad    en el PC, sin QEMU: las ROM de prueba propias (tools/mgba_rom_teste.py: GBA con la
            cabecera correcta y el logo a CEROS, GB y GBC con logo y sumas correctas), el núcleo de
            mGBA compilado con la misma lista de fuentes que mgba.mk y tests/mgba_unidad.c: los
            cuatro cuadrantes de cada ROM, la tecla A (permuta los colores), el estado de
            guardado (guardar y cargar vuelven a la imagen guardada) y la batería del cartucho.
  licencia  las fuentes fijadas (SHA-256 y VERSAO), la MPL-2.0 en el árbol, ningún fichero con
            una licencia incompatible con la GPL v3, la fila de docs/terceros.md, que no hay
            ninguna ROM ni BIOS en git y que el port entra con PUBLICABLE=1.
  os        en SaguiOS con una imagen con /jogos/{gba,gb,gbc} (ROM generadas aquí) y las apps:
            GBA: ventana de 720x480 (240x160 por 3), los cuatro colores en sus cuadrantes
            (captura), los FPS que da el propio emulador, el mando de prueba de D307 (botón B del
            Logitech = A de la GBA: permuta los colores), el estado (F5 guarda, Select cambia el
            modo, F7 carga y vuelve la imagen guardada), Start escribe en la SRAM y Esc sale y
            deja .gba.sav y .gba.ss0 en /home/.config/mgba; GB y GBC: los cuadrantes (grises y colores);
            «mgba --medir»: velocidad sin límite y a 60 Hz, con y sin escalado.
Capturas en docs/capturas/H322-*.png.
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
from h60_barra_tareas import esperar, janela  # noqa: E402
import h228_openjazz as h228  # noqa: E402
import h136_opentyrian as h136  # noqa: E402
from h307_mandos import da  # noqa: E402

MGBA = os.path.join(APPS_RAIZ, "bin", "mgba")
FUENTES = os.path.join(RAIZ, "userland", "terceros", "mgba")
JOGO = os.path.join(FUENTES, "jogo")
BORDA, BARRA = 2, 24          # el marco de las ventanas del kernel (kernel/gui/janelas.c)
AZUL, ROJO, VERDE, BLANCO = (0, 0, 255), (255, 0, 0), (0, 255, 0), (255, 255, 255)

sys.path.insert(0, os.path.join(RAIZ, "tools"))
import mgba_rom_teste as roms  # noqa: E402


def parecido(a, b, tol=14):
    return all(abs(p - q) <= tol for p, q in zip(a, b))


def hexc(c):
    return "%02x%02x%02x" % tuple(c)


# ------------------------------------------------------------------ unidad

def compilar_unidad(d):
    mk = open(os.path.join(FUENTES, "mgba.mk"), encoding="utf-8").read()
    bloque = re.search(r"MG_SRC := \$\(addprefix \$\(MG_J\)/src/, \\\n(.*?)\)\nMG_OBJ", mk, re.S).group(1)
    fuentes = [os.path.join(JOGO, "src", x) for x in bloque.replace("\\", "").split()]
    exe = os.path.join(d, "mgba_unidad")
    orden = (["gcc", "-O1", "-w", "-o", exe, "-I", os.path.join(JOGO, "include"), "-I", os.path.join(JOGO, "src"),
              "-DM_CORE_GBA", "-DM_CORE_GB", "-DDISABLE_THREADING", "-DUSE_VFS_FILE", "-DHAVE_STRDUP",
              "-DHAVE_LOCALTIME_R", "-DHAVE_STRLCPY", "-DHAVE_STRNDUP", "-DHAVE_LOCALE", "-DHAVE_STRTOF_L",
              '-DBINARY_NAME="mgba"', os.path.join(RAIZ, "tests", "mgba_unidad.c"),
              os.path.join(FUENTES, "plataforma", "version.c"), os.path.join(FUENTES, "plataforma", "memoria_mgba.c")]
             + fuentes + ["-lm"])
    r = subprocess.run(orden, capture_output=True, text=True, cwd=RAIZ)
    if r.returncode != 0:
        print(r.stdout[-2000:], r.stderr[-2000:])
    verificar(r.returncode == 0, "el núcleo de mGBA (misma lista de fuentes que mgba.mk) compila en el PC con el arnés")
    return exe


def cuadrantes_unidad(exe, rom, teclas="", estado=False, cuadros=120):
    orden = [exe, rom, str(cuadros)] + ([teclas] if teclas or estado else []) + (["estado"] if estado else [])
    r = subprocess.run(orden, capture_output=True, text=True, cwd=os.path.dirname(rom), timeout=300)
    lineas = {}
    for l in r.stdout.splitlines():
        m = re.match(r"([\w ]+): ([0-9a-f]{6}) ([0-9a-f]{6}) ([0-9a-f]{6}) ([0-9a-f]{6})$", l)
        if m:
            lineas[m.group(1)] = m.groups()[1:]
    verificar("FALLO" not in r.stdout and r.returncode == 0, "%s: el núcleo la corre sin fallos" % os.path.basename(rom))
    return lineas


def unidad():
    import tempfile
    d = tempfile.mkdtemp(prefix="h322-", dir=SAIDA if os.path.isdir(SAIDA) else None)
    try:
        r = roms.generar(d)
        gba = open(r["saguitest.gba"], "rb").read()
        suma = (-(sum(gba[0xA0:0xBD]) + 0x19)) & 0xFF
        verificar(len(gba) == 0x8000 and gba[0xB2] == 0x96 and gba[0xBD] == suma and gba[0xA0:0xA9] == b"SAGUITEST",
                  "ROM GBA: 32 KB, cabecera con el título y la suma 0xBD correcta (%02x)" % suma)
        verificar(not any(gba[0x04:0xA0]), "ROM GBA: el logo de Nintendo (0x04-0x9F) va a ceros: no se copia ninguno")
        verificar(b"SRAM_V113" in gba, "ROM GBA: lleva la marca SRAM_V113 (la SRAM se detecta sola)")
        for n in ("saguitest.gb", "saguitest.gbc"):
            b = open(r[n], "rb").read()
            x = 0
            for i in range(0x134, 0x14D):
                x = (x - b[i] - 1) & 0xFF
            g = (sum(b) - b[0x14E] - b[0x14F]) & 0xFFFF
            verificar(b[0x14D] == x and b[0x14E] == g >> 8 and b[0x14F] == g & 0xFF and b[0x104:0x108] == bytes.fromhex("CEED6666"),
                      "ROM %s: logo y las dos sumas de la cabecera correctas" % n)
        verificar(open(r["saguitest.gbc"], "rb").read()[0x143] == 0xC0, "ROM GBC: marca de solo-Color (0x143 = 0xC0)")
        exe = compilar_unidad(d)

        normal = [hexc(c) for c in (AZUL, ROJO, VERDE, BLANCO)]
        inv = [hexc(c) for c in (BLANCO, VERDE, ROJO, AZUL)]
        c = cuadrantes_unidad(exe, r["saguitest.gba"], "0@0")["final"]
        verificar(list(c) == normal, "GBA: los cuatro cuadrantes azul, rojo, verde y blanco (%s)" % " ".join(c))
        c = cuadrantes_unidad(exe, r["saguitest.gba"], "1@30")["final"]
        verificar(list(c) == inv, "GBA: con A pulsada los colores se permutan (%s)" % " ".join(c))
        e = cuadrantes_unidad(exe, r["saguitest.gba"], "4@30:0@40:1@80:0@90", estado=True)
        verificar(list(e["antes de guardar"]) == normal and list(e["antes de cargar"]) == inv and list(e["despues de cargar"]) == normal,
                  "GBA: estado de guardado: guardado normal, cambiado a invertido con Select y la carga lo devuelve")
        gris = ["000000", "adadad", "525252", "ffffff"]
        c = cuadrantes_unidad(exe, r["saguitest.gb"], "0@0")["final"]
        verificar(list(c) == gris, "GB: cuatro tonos de gris en sus cuadrantes (%s)" % " ".join(c))
        c = cuadrantes_unidad(exe, r["saguitest.gb"], "1@30")["final"]
        verificar(list(c) == ["ffffff", "525252", "adadad", "000000"], "GB: A invierte la paleta (%s)" % " ".join(c))
        e = cuadrantes_unidad(exe, r["saguitest.gb"], "0@0", estado=True)
        verificar("despues de cargar" in e, "GB: guardar y cargar un estado funciona")
        c = cuadrantes_unidad(exe, r["saguitest.gbc"], "0@0")["final"]
        verificar(list(c) == normal, "GBC: paleta en color: azul, rojo, verde y blanco (%s)" % " ".join(c))
        e = cuadrantes_unidad(exe, r["saguitest.gbc"], "0@0", estado=True)
        verificar("despues de cargar" in e, "GBC: guardar y cargar un estado funciona")
        # La batería: Start escribe «SAGUI» y al salir queda en el .sav
        for n, esperado in (("saguitest.gb", 0x2000), ("saguitest.gba", None)):
            cuadrantes_unidad(exe, r[n], "8@10:0@40", cuadros=60)
            sav = r[n] + ".sav"
            verificar(os.path.isfile(sav) and open(sav, "rb").read(5) == b"SAGUI",
                      "%s: Start escribe «SAGUI» en la batería y queda en el .sav" % n)
    finally:
        shutil.rmtree(d, ignore_errors=True)


# ------------------------------------------------------------------ licencia

def licencia():
    v = open(os.path.join(JOGO, "VERSAO")).read().split()
    verificar(v[0] == "0.10.5" and len(v[1]) == 64, "VERSAO: 0.10.5 y el SHA-256 del tarball")
    sh = open(os.path.join(RAIZ, "tools", "mgba_fuentes.sh")).read()
    verificar(v[1] in sh and 'VERSAO=0.10.5' in sh, "tools/mgba_fuentes.sh fija la misma versión y el mismo SHA-256")
    lic = open(os.path.join(JOGO, "LICENSE")).read(200)
    verificar("Mozilla Public License Version 2.0" in lic, "LICENSE del upstream: MPL-2.0")
    # Ningún fichero con otra licencia: se busca cualquier cabecera que no sea MPL, LGPL de blip_buf,
    # BSD de inih o dominio público.
    otras = []
    for dp, _, fs in os.walk(os.path.join(JOGO, "src")):
        for f in fs:
            if not f.endswith((".c", ".h")):
                continue
            t = open(os.path.join(dp, f), encoding="utf-8", errors="replace").read(1500)
            if re.search(r"GNU (Lesser )?General Public License|GPL|proprietary|All rights reserved", t) and \
               "blip_buf" not in dp and "inih" not in dp:
                otras.append(os.path.join(dp, f))
    verificar(not otras, "ningún fichero de src/ con licencia distinta de la MPL (salvo blip_buf y inih): %s" % otras)
    incompat = []
    for dp, _, fs in os.walk(JOGO):
        for f in fs:
            if f != "LICENSE" and f.endswith((".c", ".h", ".md", ".txt")):
                if "Incompatible With Secondary Licenses" in open(os.path.join(dp, f), encoding="utf-8", errors="replace").read():
                    incompat.append(os.path.join(dp, f))
    verificar(not incompat, "ningún fichero lleva la nota «Incompatible With Secondary Licenses» (Exhibit B de la MPL): %s" % incompat)
    blip = open(os.path.join(JOGO, "src", "third-party", "blip_buf", "blip_buf.c"), encoding="latin-1").read(1200)
    verificar("version 2.1 of the License, or (at your option) any later version" in blip,
              "blip_buf: LGPL-2.1 «o posterior» (compatible con la GPL v3)")
    inih = open(os.path.join(JOGO, "src", "third-party", "inih", "LICENSE.txt")).read(100)
    verificar("New BSD" in inih, "inih: BSD de 3 cláusulas")
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("**mGBA 0.10.5**" in t and "MPL-2.0" in t and "Secondary License" in t, "docs/terceros.md: fila de mGBA con la MPL-2.0 y la «Secondary License»")
    mk = open(os.path.join(RAIZ, "Makefile"), encoding="utf-8").read()
    verificar("MGBA ?= 1" in mk and "userland/terceros/mgba/mgba.mk" in mk, "el Makefile incluye mgba.mk (MGBA=0 lo deja fuera)")
    verificar("mgba" not in open(os.path.join(RAIZ, "tools", "publicar-excluir.txt")).read().lower(),
              "mGBA no está en tools/publicar-excluir.txt: el port va en el repositorio público")
    # Ninguna ROM ni BIOS comercial en git.
    ls = subprocess.run(["git", "ls-files"], capture_output=True, text=True, cwd=RAIZ).stdout.splitlines()
    malos = [f for f in ls if f.lower().endswith((".gba", ".gb", ".gbc", ".sgb", ".nes")) or "gba_bios" in f.lower()
             or f.lower().endswith("bios.bin")]
    verificar(not malos, "ninguna ROM ni BIOS en git: %s" % malos)
    r = subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "auditar_licencias.py")], capture_output=True, text=True, cwd=RAIZ)
    print("    " + (r.stdout + r.stderr).strip().replace("\n", "\n    ")[-600:])
    verificar(r.returncode == 0, "tools/auditar_licencias.py (h81) sin errores con mGBA")


# ------------------------------------------------------------------ os

def jogos_con(directorio):
    d = os.path.join(SAIDA, "h322-jogos")
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    r = roms.generar(os.path.join(SAIDA, "h322-roms"))
    for ext, nome in (("gba", "saguitest.gba"), ("gba", "saguiestres.gba"), ("gb", "saguitest.gb"), ("gbc", "saguitest.gbc")):
        os.makedirs(os.path.join(d, ext), exist_ok=True)
        shutil.copy(r[nome], os.path.join(d, ext, nome))
    return d


def imagem():
    return imagem_de_teste("", ["--vesa", "--mandos-prueba", "--jogos", jogos_con(None), "--apps", APPS_RAIZ])


def pantalla(m, nome):
    return h228.pantalla(m, nome)


def centros(m, esc):
    j = janela(m.serial().replace("\r", ""), "mGBA")
    return j, j["x"] + BORDA, j["y"] + BARRA


def ler_cuadrantes(m, nome, w_jogo, a_jogo):
    j, x0, y0 = centros(m, 3)
    w, h, rgb = pantalla(m, nome)
    res = []
    for fx, fy in ((0.25, 0.25), (0.75, 0.25), (0.25, 0.75), (0.75, 0.75)):
        x, y = x0 + int(j["l"] * fx), y0 + int(j["a"] * fy)
        i = (y * w + x) * 3
        res.append(tuple(rgb[i:i + 3]))
    return res


def ver(m, nome, esperado, que, tempo=20, tol=14):
    fim = time.time() + tempo
    while True:
        cs = ler_cuadrantes(m, nome, 0, 0)
        ok = all(parecido(c, e, tol) for c, e in zip(cs, esperado))
        if ok or time.time() > fim:
            verificar(ok, "%s: %s (esperados %s)" % (que, " ".join(hexc(c) for c in cs), " ".join(hexc(e) for e in esperado)))
            return cs
        time.sleep(0.7)


def en_terminal(m, texto, espera=0.03):
    """Escribe en el Terminal (hay que darle el foco: la ventana del emulador lo tiene)."""
    h136.focar_terminal(m)
    m.digitar(texto, espera)


def foco_juego(m):
    j, x0, y0 = centros(m, 3)
    m.clique(x0 + j["l"] // 2, y0 + j["a"] // 2)
    time.sleep(0.6)


def lanzar(m, comando, patron_ventana=r"\[MGBA\] primer cuadro dibujado", tempo=90):
    marca = len(m.serial())
    m.digitar(comando + "\n", 0.03)
    esperar(m, patron_ventana, tempo, desde=marca)
    time.sleep(2)
    return marca


def salir(m, marca):
    foco_juego(m)
    m.tecla("esc")
    esperar(m, r"\[MGBA\] fin", 40, desde=marca)
    time.sleep(1.5)
    h136.focar_terminal(m)


def fps(m, marca):
    f = re.findall(r"\[MGBA\] fps: ([0-9.]+) \(presentados ([0-9.]+), salto (\d+)\)", m.serial()[marca:])
    return [(float(a), float(b), int(c)) for a, b, c in f]


def os_():
    if not os.path.isfile(MGBA):
        raise AssertionError("falta %s (make: MGBA=1, sin SEM_GPL)" % MGBA)
    normal = [AZUL, ROJO, VERDE, BLANCO]
    inv = [BLANCO, VERDE, ROJO, AZUL]
    with Maquina("h322", "ide", imagem=imagem(), som="hda-output") as m:
        h228.terminal(m)
        # ---------------- GBA
        # La ventana del emulador tapa el Terminal y se lleva el foco: las órdenes del mando de prueba se
        # escriben ANTES de lanzarlo, todas en una línea con sus esperas (la prueba se sincroniza con
        # las líneas «teclas:» del emulador).
        def inyectar(botones):
            return "mandos --inyectar 0 %s" % da(botoes=botones)
        guion = ("mgba /jogos/gba/saguitest.gba & sleep 40; %s; sleep 8; %s; sleep 45; %s; sleep 8; %s; sleep 45; %s; sleep 3; %s; "
                 "echo FIN-MANDOS" % (inyectar(1 << 2), inyectar(0), inyectar(1 << 8), inyectar(0), inyectar(1 << 9), inyectar(0)))
        marca = lanzar(m, guion)
        s = m.serial()[marca:]
        verificar(re.search(r'\[MGBA\] ROM: /jogos/gba/saguitest\.gba "SAGUITEST" \(GBA, 240x160\)', s) is not None,
                  "mgba reconoce la ROM GBA de prueba (título SAGUITEST, 240x160)")
        j, x0, y0 = centros(m, 3)
        verificar((j["l"], j["a"]) == (720, 480), "la ventana es de 720x480 (240x160 por 3): %dx%d" % (j["l"], j["a"]))
        foco_juego(m)
        ver(m, "h322-gba-normal", normal, "GBA: los cuatro cuadrantes azul, rojo, verde y blanco")
        m.captura("H322-1-gba", guardar=True)
        # FPS del propio emulador (cada 5 s)
        time.sleep(11)
        f = fps(m, marca)
        print("    FPS del emulador (GBA): %s" % ", ".join("%.1f" % a for a, _, _ in f))
        verificar(len(f) >= 1, "el emulador informa de sus FPS cada 5 s")
        verificar(min(a for a, _, _ in f) >= 30, "GBA: al menos 30 FPS en QEMU (%s)" % ", ".join("%.1f" % a for a, _, _ in f))
        # Mando de prueba (D307): botón 3 del Logitech (B) = A de la GBA
        esperar(m, r"mando abierto como SDL_GameController: Mando de prueba \(Dual Action\)", 30, desde=marca)
        verificar(True, "mgba abre el Dual Action de prueba como SDL_GameController")
        esperar(m, r"\[MGBA\] teclas: 001", 90, desde=marca)
        ver(m, "h322-gba-mando", inv, "GBA: el botón del mando (B del Logitech) pulsa A y permuta los colores", tempo=12)
        m.captura("H322-2-gba-mando", guardar=True)
        ver(m, "h322-gba-suelto", normal, "GBA: al soltar el mando vuelven los colores", tempo=20)
        # Estado de guardado: F5 (guardar), Select (cambia el modo, a la hora fijada), F7 (cargar)
        foco_juego(m)
        m.tecla("f5")
        esperar(m, r"\[MGBA\] estado guardado: /home/\.config/mgba/saguitest\.gba\.ss0 \(ranura 0\)", 30, desde=marca)
        verificar(True, "F5 guarda el estado en /home/.config/mgba/saguitest.gba.ss0")
        esperar(m, r"\[MGBA\] teclas: 004", 90, desde=marca)
        ver(m, "h322-gba-modo", inv, "GBA: Select (Back) cambia el modo: colores permutados", tempo=12)
        marca2 = len(m.serial())
        esperar(m, r"\[MGBA\] teclas: 000", 60, desde=marca2)   # suelto Select: si no, la carga lo volvería a pulsar
        foco_juego(m)
        m.tecla("f7")
        esperar(m, r"\[MGBA\] estado cargado: /home/\.config/mgba/saguitest\.gba\.ss0", 30, desde=marca)
        ver(m, "h322-gba-cargado", normal, "GBA: F7 carga el estado y vuelve la imagen guardada", tempo=20)
        m.captura("H322-3-gba-estado", guardar=True)
        # Start escribe en la SRAM
        esperar(m, r"\[MGBA\] teclas: 008", 90, desde=marca)
        esperar(m, r"FIN-MANDOS", 90, desde=marca)
        time.sleep(2)
        salir(m, marca)
        out = m.comando("ls -l /home/.config/mgba")
        verificar("saguitest.gba.sav" in out and "saguitest.gba.ss0" in out and "mgba.conf" in out,
                  "al salir quedan saguitest.gba.sav, saguitest.gba.ss0 y mgba.conf en /home/.config/mgba (%s)" % out.replace("\n", " | ")[-200:])
        out = m.comando("cat /home/.config/mgba/saguitest.gba.sav")
        verificar("SAGUI" in out, "la SRAM guardada lleva «SAGUI» (Start de la ROM)")
        # ---------------- GB
        marca = lanzar(m, "mgba /jogos/gb/saguitest.gb &")
        verificar(re.search(r'\[MGBA\] ROM: /jogos/gb/saguitest\.gb "SAGUITEST" \(GB/GBC, 160x144\)', m.serial()[marca:]) is not None,
                  "mgba reconoce la ROM GB (160x144)")
        j, x0, y0 = centros(m, 3)
        verificar((j["l"], j["a"]) == (480, 432), "GB: ventana de 480x432 (160x144 por 3): %dx%d" % (j["l"], j["a"]))
        gris = [(0, 0, 0), (0xAD, 0xAD, 0xAD), (0x52, 0x52, 0x52), (255, 255, 255)]
        ver(m, "h322-gb", gris, "GB: cuatro tonos de gris en sus cuadrantes")
        m.captura("H322-4-gb", guardar=True)
        f = fps(m, marca)
        time.sleep(6)
        f = fps(m, marca)
        print("    FPS del emulador (GB): %s" % ", ".join("%.1f" % a for a, _, _ in f))
        verificar(f and min(a for a, _, _ in f) >= 50, "GB: 50 FPS o más en QEMU (sobra)")
        salir(m, marca)
        # ---------------- GBC
        marca = lanzar(m, "mgba /jogos/gbc/saguitest.gbc &")
        ver(m, "h322-gbc", normal, "GBC: paleta en color: azul, rojo, verde y blanco")
        m.captura("H322-5-gbc", guardar=True)
        salir(m, marca)
        # ---------------- --medir (con la ROM ligera y con la cargada: 4 fondos y 128 sprites)
        for rom, seg, que in (("gba/saguitest.gba", 3, "ROM ligera"), ("gba/saguiestres.gba", 3, "ROM cargada (4 fondos, 128 sprites de 64x64)"),
                              ("gb/saguitest.gb", 3, "Game Boy"), ("gbc/saguitest.gbc", 3, "Game Boy Color")):
            desde = len(m.serial())
            m.digitar("mgba --medir /jogos/%s %d\n" % (rom, seg), 0.03)
            esperar(m, r"\[MGBA\] fin", 300, desde=desde)
            s = m.serial()[desde:]
            med = re.findall(r"\[MGBA-MEDIDA\] (.+?) fps_medios=([0-9.]+) minimos=([0-9.]+) cpu=([0-9.]+)% x_consola=([0-9.]+) mticks=([0-9.]+)", s)
            print("    mgba --medir, %s:" % que)
            for q, a_, b_, c_, d_, t_ in med:
                print("      %-34s medios %8s  minimos %8s  CPU %5s%%  x%-7s %s Mticks/cuadro" % (q, a_, b_, c_, d_, t_))
            nombres = [x[0] for x in med]
            verificar(any("nucleo solo, sin limite" in n for n in nombres) and any("escalado, a 60 Hz" in n for n in nombres)
                      and any("salto 2" in n for n in nombres),
                      "--medir (%s) da las fases: núcleo solo, 1x y escalado, sin límite, con salto y a 60 Hz" % rom)
            libre = [float(x[1]) for x in med if x[0] == "nucleo solo, sin limite"][0]
            verificar(libre > 60, "%s: el núcleo solo va más rápido que la consola (%.0f FPS sin límite)" % (rom, libre))
            a60 = [float(x[1]) for x in med if x[0] == "escalado, a 60 Hz"][0]
            verificar(55 <= a60 <= 63, "%s: a 60 Hz el emulador da la velocidad de la consola (%.1f FPS)" % (rom, a60))
            h136.focar_terminal(m)


# ------------------------------------------------------------------ principal

PARTES = {"unidad": unidad, "licencia": licencia, "os": os_}

if __name__ == "__main__":
    pedidas = sys.argv[1:] or list(PARTES)
    for p in pedidas:
        print("== %s" % p)
        PARTES[p]()
    print("H322 OK")
