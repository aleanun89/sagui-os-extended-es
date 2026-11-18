#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H391 - DOOM (Chocolate Doom 3.1.1 con Freedoom 0.13.0) dentro del SaguiOS (D383).

Partes (por defecto todas; o las que se pidan: h391_doom.py licencia os):
  licencia   fuentes y datos fijados con SHA-256 (tools/doom_fuentes.sh, tools/doom_datos.sh), filas en
             docs/terceros.md, D383 en docs/decisoes.md; ningún WAD comercial en git.
  os         en SaguiOS: Inicio > Juegos > DOOM arranca con Freedoom, pasa del menú a una partida, se mueve y
             dispara; sin WAD sale un mensaje claro; timedemo con resultado razonable.
Sin las fuentes (tools/doom_fuentes.sh) o sin Freedoom (tools/doom_datos.sh) se salta con un aviso.
Capturas en docs/capturas/H391-*.png (solo con Freedoom).
"""
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))

FUENTES = os.path.join(RAIZ, "userland", "terceros", "chocolatedoom", "fuentes")
DATOS = os.path.join(RAIZ, "userland", "terceros", "chocolatedoom", "datos")


def licencia():
    from qemu import verificar
    s = open(os.path.join(RAIZ, "tools", "doom_fuentes.sh"), encoding="utf-8").read()
    verificar(re.search(r"SHA256=[0-9a-f]{64}", s), "las fuentes de Chocolate Doom están fijadas con SHA-256")
    s = open(os.path.join(RAIZ, "tools", "doom_datos.sh"), encoding="utf-8").read()
    verificar(re.search(r"SHA256=[0-9a-f]{64}", s), "Freedoom está fijado con SHA-256")
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("Chocolate Doom" in t and "Freedoom" in t, "docs/terceros.md tiene las filas de Chocolate Doom y Freedoom")
    d = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D383" in d, "docs/decisoes.md tiene D383")
    out = subprocess.run(["git", "-C", RAIZ, "ls-files"], capture_output=True, text=True).stdout.lower().split("\n")
    malos = [f for f in out if f.endswith(".wad") or "doom.wad" in f or "doom2.wad" in f]
    verificar(not malos, "ningún WAD en git (%s)" % malos)
    print("  ok: licencia y decisión registradas")


def preparar():
    return os.path.isfile(os.path.join(FUENTES, ".preparado")) and os.path.isfile(os.path.join(DATOS, ".preparado"))


def arrancar(m, entrada, espera=60):
    """Inicio > Juegos > <entrada>."""
    from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui
    esperar(m, r"sagui:/\$", 150)
    time.sleep(4)
    marca = len(m.serial())
    m.clique(40, 12)
    p = painel(m, marca)
    sub = submenu_pelo_mouse(m, p, "Juegos", entrada)
    return marca, sub, clicar_aqui


def os_():
    if not preparar():
        print("AVISO: sin las fuentes de Chocolate Doom o sin Freedoom (tools/doom_fuentes.sh, tools/doom_datos.sh): se salta «os»")
        return
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "chocolate-doom")):
        print("AVISO: la imagen no lleva chocolate-doom (make): se salta la parte «os»")
        return
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h391-juego", "ide", imagem=img, memoria=os.environ.get("H391_MEM", "512M"), som="hda-output") as m:
        marca, sub, clicar_aqui = arrancar(m, "DOOM")
        verificar("DOOM" in sub["linhas"] and "Freedoom" in sub["linhas"], "Inicio > Juegos tiene DOOM y Freedoom (%s)" % ", ".join(sub["linhas"]))
        m.captura("H391-1-menu-juegos")
        marca = len(m.serial())
        clicar_aqui(m)
        esperar(m, r"inicio -> /apps/bin/chocolate-doom", desde=marca)
        try:
            esperar(m, r"\[SDL2\] primer cuadro dibujado", 60, desde=marca)
        except AssertionError:
            m.captura("H391-x-fallo", guardar=False)
            print(m.serial()[marca:][-1500:])
            raise
        time.sleep(8)
        m.captura("H391-2-titulo")
        esperar(m, r"\[MIXER\] música reproducida", 30, desde=marca)   # la música MIDI sale por el sintetizador del sistema
        print("  ok: suena la música (MIDI)")
        # el menú: una tecla pasa del título al menú principal; «Nuevo juego» (Intro) y la dificultad (Intro)
        m.tecla("ret", 0.5)
        time.sleep(2)
        m.captura("H391-3-menu")
        m.tecla("ret", 0.5)
        time.sleep(1)
        m.tecla("ret", 0.5)       # dificultad por defecto
        time.sleep(12)            # carga del mapa y fundido
        m.captura("H391-4-partida")
        a = hash_region(m, "ant", (0, 0, 800, 600))
        # mover: avanzar y girar (el mapa cambia); 1,5 s con la tecla apretada
        m.monitor("sendkey up 1500")
        time.sleep(2)
        m.monitor("sendkey left 700")
        time.sleep(1.5)
        m.captura("H391-5-movido")
        b = hash_region(m, "mov", (0, 0, 800, 600))
        verificar(a != b, "al avanzar y girar la vista cambia")
        # disparar: la pistola gasta munición (el contador de la barra cambia)
        barra_a = hash_region(m, "barra-a", (0, 540, 800, 60))
        for _ in range(3):
            m.monitor("sendkey ctrl 300")
            time.sleep(1.2)
        m.captura("H391-6-disparo")
        barra_b = hash_region(m, "barra-b", (0, 540, 800, 60))
        verificar(barra_a != barra_b, "al disparar cambia la barra de estado (munición)")
        verificar("saving config" in m.serial() or "Using /home/.config/chocolate-doom/" in m.serial(),
                  "la configuración va a /home/.config/chocolate-doom/")
        print("  ok: arranca, pasa del menú a una partida, se mueve y dispara")


def hash_region(m, nombre, rect):
    """Suma de una zona de la ventana de DOOM (ventana en (218,134)) en una captura PPM, para ver si cambió."""
    from qemu import SAIDA, ler_ppm
    import hashlib
    ppm = os.path.join(SAIDA, "h391-%s.ppm" % nombre)
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor("screendump %s" % ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    l, a, px = ler_ppm(ppm)
    x, y, w, h = rect
    h_ = hashlib.sha256()
    for fila in range(134 + y, 134 + y + h):
        h_.update(px[(fila * l + 218 + x) * 3:(fila * l + 218 + x + w) * 3])
    return h_.hexdigest()


def sinwad():
    """Sin ningún WAD (se mueve /apps/doom): una ventana explica dónde ponerlos, no un error críptico."""
    if not preparar():
        print("AVISO: sin fuentes o sin Freedoom: se salta «sinwad»")
        return
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ, SAIDA
    from h60_barra_tareas import esperar
    import shutil
    # /apps sin los WAD: la misma raíz con enlaces, salvo /apps/doom (solo el leeme)
    sin = os.path.join(SAIDA, "h391-apps-sin-wad")
    if os.path.isdir(sin):
        shutil.rmtree(sin)
    shutil.copytree(APPS_RAIZ, sin, ignore=shutil.ignore_patterns("doom"), copy_function=os.link)
    os.makedirs(os.path.join(sin, "doom"))
    shutil.copy(os.path.join(APPS_RAIZ, "doom", "leeme.txt"), os.path.join(sin, "doom", "leeme.txt"))
    img = imagem_de_teste("sinwad", ["--vesa", "--apps", sin])
    with Maquina("h391-sinwad", "ide", imagem=img, memoria="512M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        marca = len(m.serial())
        m.digitar("chocolate-doom", 0.05)
        m.tecla("ret")
        esperar(m, r"mensaje: DOOM: No se encuentra ning.n WAD de DOOM", 60, desde=marca)
        time.sleep(2)
        m.captura("H391-7-sin-wad")
        print("  ok: sin WAD sale el mensaje que dice dónde ponerlos")


def timedemo():
    """timedemo de Freedoom (demo1 y demo2): cuadros por segundo del motor entero (dibujo incluido)."""
    if not preparar():
        print("AVISO: sin fuentes o sin Freedoom: se salta «timedemo»")
        return
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    kvm = os.environ.get("SAGUI_KVM") == "1"
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h391-timedemo", "ide", imagem=img, memoria="512M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        for demo in ("demo1", "demo2"):
            marca = len(m.serial())
            m.digitar("chocolate-doom --freedoom -nosound -timedemo %s" % demo, 0.05)
            m.tecla("ret")
            r = esperar(m, r"timed (\d+) gametics in (\d+) realtics \(([\d.]+) fps\)", 900, desde=marca)
            gt, rt, fps = int(r[0]), int(r[1]), float(r[2])
            print("  %s: %d gametics en %d realtics: %.1f fps (%s)" % (demo, gt, rt, fps, "KVM" if kvm else "TCG"))
            verificar(gt > 500, "el timedemo %s corre %d gametics" % (demo, gt))
            verificar(fps > (100 if kvm else 20), "el timedemo %s va a %.1f fps" % (demo, fps))
            time.sleep(3)
        m.captura("H391-8-timedemo")


PARTES = {"licencia": licencia, "os": os_, "sinwad": sinwad, "timedemo": timedemo}

if __name__ == "__main__":
    for p in (sys.argv[1:] or list(PARTES)):
        print("== H391 %s" % p)
        PARTES[p]()
    print("H391: ok")
