#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H374 - Freeciv 3.1.6 (cliente SDL2 y servidor local, en español) dentro del SaguiOS (D374).

Partes (por defecto todas; o las que se pidan: h374_freeciv.py catalogos licencia os):
  catalogos  en el PC: tools/freeciv_mo.py compila es.po (con el suplemento es-sagui.po) a .mo con
             la cabecera de plurales y las cadenas de la barra de estado del cliente SDL2.
  licencia   las fuentes están fijadas con SHA-256 en tools/freeciv_fuentes.sh, hay fila en
             docs/terceros.md y D374 en docs/decisoes.md.
  os         en SaguiOS (512 MB): Inicio > Juegos > Freeciv; el cliente abre su menú principal en
             español; «Empezar una nueva partida» lanza el servidor local (otro proceso, TCP en
             127.0.0.1) y el cliente se conecta; «Comenzar» genera el mapa (partida de un jugador
             contra la IA) y se ve el mapa; se termina un turno (Intro); el servidor guarda la
             partida (.sav.gz) y se carga con «freeciv-sdl2 --file».
Sin las fuentes (tools/freeciv_fuentes.sh) se salta con un aviso.
Capturas en docs/capturas/H374-*.png.
"""
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))

FUENTES = os.path.join(RAIZ, "userland", "terceros", "freeciv", "fuentes")
PO = os.path.join(FUENTES, "translations", "core", "es.po")
EXTRA = os.path.join(RAIZ, "userland", "terceros", "freeciv", "traducciones", "es-sagui.po")


def catalogos():
    if not os.path.isfile(PO):
        print("AVISO: sin las fuentes de Freeciv (tools/freeciv_fuentes.sh): se salta «catalogos»")
        return
    from qemu import verificar, SAIDA
    os.makedirs(SAIDA, exist_ok=True)
    mo = os.path.join(SAIDA, "h374-core.mo")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "freeciv_mo.py"), PO, EXTRA, mo], check=True,
                   stdout=subprocess.DEVNULL)
    d = open(mo, "rb").read()
    verificar(d[:4] == b"\xde\x12\x04\x95", "el .mo tiene la firma de GNU gettext")
    verificar(b"Plural-Forms" in d, "conserva la cabecera con las formas de plural")
    verificar("Población: %s  Año: %s  Oro %d".encode() in d, "la barra de estado del cliente SDL2 está traducida")
    verificar("Empezar una nueva partida".encode() in d, "el menú principal está en el catálogo")
    print("  ok: catálogo es compilado (%d bytes)" % len(d))


def licencia():
    from qemu import verificar
    s = open(os.path.join(RAIZ, "tools", "freeciv_fuentes.sh"), encoding="utf-8").read()
    verificar(re.search(r"SHA256=[0-9a-f]{64}", s) and re.search(r"ZSHA256=[0-9a-f]{64}", s),
              "las fuentes de Freeciv y de zlib están fijadas con SHA-256")
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("Freeciv" in t, "docs/terceros.md tiene la fila de Freeciv")
    d = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D374" in d, "docs/decisoes.md tiene D374")
    print("  ok: licencia y decisión registradas")


def os_():
    if not os.path.isfile(os.path.join(FUENTES, ".preparado")):
        print("AVISO: sin las fuentes de Freeciv (tools/freeciv_fuentes.sh): se salta la parte «os»")
        return
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "freeciv-sdl2")):
        print("AVISO: la imagen no lleva freeciv-sdl2 (make): se salta la parte «os»")
        return
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h374-juego", "ide", imagem=img, memoria=os.environ.get("H374_MEM", "512M"), som="hda-output") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        sub = submenu_pelo_mouse(m, p, "Juegos", "Freeciv")
        verificar("Freeciv" in sub["linhas"], "Inicio > Juegos tiene Freeciv (%s)" % ", ".join(sub["linhas"]))
        time.sleep(0.4)
        m.captura("H374-1-menu-juegos")
        marca = len(m.serial())
        clicar_aqui(m)
        esperar(m, r"inicio -> /apps/bin/freeciv-sdl2", desde=marca)
        esperar(m, r"\[SDL2\] primer cuadro dibujado", 120, desde=marca)
        esperar(m, r"Se usa la salida de v", 30, desde=marca)   # el cliente habla español (catálogo es)
        time.sleep(3)
        m.captura("H374-2-menu-principal")
        # sonido: la SDL2_mixer del sistema abre el audio y suena la música del menú (stb_vorbis)
        esperar(m, r"\[MIXER\] audio abierto: \d+ Hz, 2 canales", 30, desde=marca)
        esperar(m, r"\[MIXER\] música reproducida: \d+ Hz, 2 canales", 60, desde=marca)
        print("  ok: el audio está abierto y suena la música de Freeciv (Ogg Vorbis)")
        # el menú principal: «Empezar una nueva partida» (la ventana es de 1000x700)
        marca = len(m.serial())
        m.clique(*PUNTOS["nueva"])
        esperar(m, r"Aceptando ahora nuevas conexiones de clientes en el puerto", 180, desde=marca)
        esperar(m, r"se ha conectado desde 127\.0\.0\.1", 60, desde=marca)
        time.sleep(3)
        m.captura("H374-3-pagina-inicio")
        # el turno acaba solo a los 10 s (la línea de charla de la página de inicio manda órdenes al servidor)
        m.clique(*PUNTOS["charla"])
        m.digitar("/set timeout 10", 0.05)
        m.tecla("ret")
        esperar(m, r"timeout' ahora vale 10|'timeout' has been set to 10|timeout.*10", 30)
        marca = len(m.serial())
        for _ in range(3):   # el primer clic puede perderse si la línea de charla tenía el foco
            m.clique(*PUNTOS["comenzar"])
            try:
                esperar(m, r"Empezando la partida", 15, desde=marca)
                break
            except AssertionError:
                pass
        esperar(m, r"Partida salvada en el fichero /home/\.freeciv/saves/\S+\.sav\.gz", 300, desde=marca)
        time.sleep(10)
        m.captura("H374-4-mapa")
        verificar("Creando un mapa" in m.serial()[marca:].replace("Ã±", "ñ"), "el servidor crea el mapa (en español)")
        # terminar el turno: con el tiempo límite de 10 s el servidor pasa al turno 2 y lo guarda solo
        guardada = esperar(m, r"Partida salvada en el fichero (/home/\.freeciv/saves/freeciv-T0002\S+\.sav\.gz)", 300, desde=marca)
        time.sleep(5)
        m.captura("H374-5-turno-2")
        print("  ok: partida jugada, turno terminado y guardado")
        for tecla in ("kp_6", "kp_4", "kp_8", "kp_2"):   # mover la unidad: suena el efecto de movimiento
            try:
                esperar(m, r"\[MIXER\] primer efecto reproducido", 40)
                break
            except AssertionError:
                try:
                    m.tecla(tecla)
                except Exception:
                    pass
        if re.search(r"\[MIXER\] primer efecto reproducido", m.serial()):
            print("  ok: se reprodujo un efecto de sonido de la partida (Ogg Vorbis por la SDL2_mixer)")
        else:
            print("  AVISO: en esta pasada no sonó ningún efecto de la partida (dependen de los sucesos); la música sí")
        # cerrar el cliente y volver a cargar la partida guardada
        m.clique(*PUNTOS["cerrar"])
        time.sleep(5)
        import h136_opentyrian as h136
        h136.focar_terminal(m)
        marca = len(m.serial())
        m.digitar("/apps/bin/freeciv-sdl2 --file " + guardada, 0.04)
        m.tecla("ret")
        esperar(m, r"se ha conectado desde 127\.0\.0\.1", 300, desde=marca)
        time.sleep(15)
        m.captura("H374-6-partida-cargada")
        print("  ok: la partida guardada se carga")


# Coordenadas (pantalla) de los botones: la ventana nace de 1000x700 y el escritorio de 1024x768 la reduce a su tamaño.
PUNTOS = {"nueva": (880, 606), "comenzar": (958, 742), "cerrar": (1009, 54), "charla": (500, 712)}


if __name__ == "__main__":
    partes = sys.argv[1:] or ["catalogos", "licencia", "os"]
    for p in partes:
        print("== " + p)
        {"catalogos": catalogos, "licencia": licencia, "os": os_}[p]()
    print("H374 ok")
