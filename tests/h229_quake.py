#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H229 - Quake (el motor de id Software, renderizador por software) dentro del SaguiOS (D229).

Partes (por defecto, todas; o las que se pidan: h229_quake.py unidad juego sin-datos cheia):
  unidad     en el PC: la superposición de ficheros (plataforma/arquivos_quake.c) con
             tests/quake_arquivos_unidad.c: id1/pak0.pak se busca en los datos sin
             distinguir mayúsculas (ID1/PAK0.PAK del shareware) o suelto en la carpeta
             (input_share/games/Quake/pak0.pak), los mods, lo de /home/quake manda, y lo
             que se escribe va siempre a /home/quake. Y el extractor LHA del shareware
             (tools/lha_extraer.py) con un archivo -lh0- hecho aquí. Más
             tools/quake_fuentes.sh --comprobar si hay red (si no, se avisa).
  juego      la imagen con SAGUI-JOGOS (/jogos) y SAGUI-APPS (/apps) y la tarjeta de sonido:
             Inicio > Juegos tiene «Quake» junto a OpenJazz y OpenTyrian; desde el menú abre
             su ventana de 640x400 (320x200 de 8 bits, al doble) con los datos de /jogos;
             se ve la demo (captura con muchos colores); Esc abre el menú; «Single Player» >
             «New Game» carga el mapa de entrada («Introduction»); el jugador avanza con la
             flecha arriba mantenida (la pantalla cambia mucho más que quieta); la
             dificultad y el primer mapa por la consola («skill 1», «map e1m1»: «the
             Slipgate Complex») y se vuelve a mover; suena (el .wav del QEMU); se sale
             cerrando la ventana (Alt+F4) sin faltas de página ni Sys_Error, y config.cfg
             queda en /home/quake/id1.
             Sin datos de Quake en build/jogos/raiz, se salta con un aviso.
  sin-datos  la imagen sin datos de Quake: el lanzador abre su ventana de ayuda («faltan
             los datos») en vez de fallar.
  cheia      «Quake (pantalla completa)»: la ventana pasa a pantalla completa y se ve la demo.
Capturas en docs/capturas/H229-*.png.
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
QUAKE = os.path.join(APPS_RAIZ, "bin", "quake")


# ------------------------------------------------------------------ unidad

def lha_minimo(ruta, nome, conteudo):
    """Un archivo LHA de nivel 0 con un fichero guardado sin comprimir (-lh0-)."""
    sys.path.insert(0, os.path.join(RAIZ, "tools"))
    import lha_extraer
    n = nome.encode("latin-1")
    corpo = (b"-lh0-" + len(conteudo).to_bytes(4, "little") + len(conteudo).to_bytes(4, "little")
             + b"\0\0\0\0" + b"\x20" + b"\0" + bytes([len(n)]) + n
             + lha_extraer.crc16(conteudo).to_bytes(2, "little"))
    cab = bytes([len(corpo), sum(corpo) & 0xFF]) + corpo
    with open(ruta, "wb") as f:
        f.write(b"MZ codigo del autoextraible" + cab + conteudo + b"\0")


def unidad():
    d = tempfile.mkdtemp(prefix="h229-", dir=SAIDA if os.path.isdir(SAIDA) else None)
    try:
        plat = os.path.join(RAIZ, "userland", "terceros", "quake", "plataforma")
        exe = os.path.join(d, "quake_arquivos_unidad")
        subprocess.run(["gcc", "-std=gnu99", "-Wall", "-Wextra", "-I", plat, "-o", exe,
                        os.path.join(RAIZ, "tests", "quake_arquivos_unidad.c"),
                        os.path.join(plat, "arquivos_quake.c")], check=True)
        arv = os.path.join(d, "arvore")
        os.makedirs(arv)
        r = subprocess.run([exe, arv], capture_output=True, text=True, timeout=30)
        print(r.stdout.strip())
        verificar(r.returncode == 0 and "FIN 0 fallos" in r.stdout,
                  "unidad: /home/quake sobre los datos de /jogos (sin mayúsculas, pak suelto, mods, escrituras)")

        lha = os.path.join(d, "prueba.lha")
        lha_minimo(lha, "ID1\\PAK0.PAK", b"PACK de prueba")
        sal = os.path.join(d, "extraido")
        os.makedirs(sal)
        r = subprocess.run([sys.executable, "-I", os.path.join(RAIZ, "tools", "lha_extraer.py"), lha, sal],
                           capture_output=True, text=True, timeout=30)
        p = os.path.join(sal, "ID1", "PAK0.PAK")
        verificar(r.returncode == 0 and os.path.isfile(p) and open(p, "rb").read() == b"PACK de prueba",
                  "unidad: lha_extraer.py saca un fichero -lh0- con su CRC y pasa ID1\\ a ID1/")
        lha_minimo(lha, "..\\fuera.txt", b"x")
        r = subprocess.run([sys.executable, "-I", os.path.join(RAIZ, "tools", "lha_extraer.py"), lha, sal],
                           capture_output=True, text=True, timeout=30)
        verificar(r.returncode != 0 and not os.path.exists(os.path.join(d, "fuera.txt")),
                  "unidad: lha_extraer.py no escribe fuera de la carpeta de destino")
    finally:
        shutil.rmtree(d, ignore_errors=True)

    r = subprocess.run([os.path.join(RAIZ, "tools", "quake_fuentes.sh"), "--comprobar"],
                       capture_output=True, text=True, timeout=300)
    if r.returncode != 0 and "descargando" in r.stdout and "SHA-256" not in r.stderr and "NO coincide" not in r.stderr:
        print("AVISO: sin red para tools/quake_fuentes.sh --comprobar: se salta")
    else:
        verificar(r.returncode == 0, "las fuentes de jogo/ son las del WinQuake fijado (%s)" % (r.stdout + r.stderr).strip()[-160:])


# ------------------------------------------------------------------ imágenes

def tem_pak(pasta):
    """¿Tiene «pasta» los datos de Quake (id1/pak0.pak o pak0.pak suelto, sin mayúsculas)?"""
    def filho(p, nome):
        if not os.path.isdir(p):
            return None
        for n in os.listdir(p):
            if n.lower() == nome:
                return os.path.join(p, n)
        return None
    id1 = filho(pasta, "id1") or pasta
    return filho(id1, "pak0.pak") is not None


def carpeta_quake(raiz):
    """La carpeta de /jogos con datos de Quake (o None)."""
    if not os.path.isdir(raiz):
        return None
    for n in sorted(os.listdir(raiz)):
        if tem_pak(os.path.join(raiz, n)):
            return n
    return None


def jogos_sem_quake():
    """Una copia de la raíz de SAGUI-JOGOS sin ningún dato de Quake."""
    d = os.path.join(SAIDA, "h229-jogos-sin-quake")
    if os.path.isdir(d):
        shutil.rmtree(d)
    os.makedirs(d)
    for n in os.listdir(JOGOS):
        p = os.path.join(JOGOS, n)
        if os.path.isdir(p) and n in ("chip8", "sagui3d", "jclic", "opentyrian"):
            shutil.copytree(p, os.path.join(d, n))
    os.makedirs(os.path.join(d, "quake"))
    return d


def imagem(jogos):
    return imagem_de_teste("", ["--vesa", "--jogos", jogos, "--apps", APPS_RAIZ])


def terminal(m):
    esperar(m, r"sagui:/\$", 150)
    time.sleep(4)
    h136.focar_terminal(m)


def abrir_pelo_menu(m, entrada="Quake"):
    marca = len(m.serial())
    m.clique(40, 12)
    p = painel(m, marca)
    sub = submenu_pelo_mouse(m, p, "Juegos", entrada)
    linhas = sub["linhas"]
    verificar("Quake" in linhas and "Quake (pantalla completa)" in linhas and "OpenTyrian" in linhas,
              "Inicio > Juegos tiene Quake junto a los otros juegos (%s)" % ", ".join(linhas))
    s = m.serial()[marca:]
    verificar("Reiniciar..." in s, "el menú Inicio cabe entero (llega hasta «Reiniciar...»)")
    time.sleep(0.4)
    if entrada == "Quake":
        m.captura("H229-1-menu-juegos")
    marca = len(m.serial())
    clicar_aqui(m)
    esperar(m, r"inicio -> /apps/bin/quake", desde=marca)
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
    j = re.findall(r'janela \d+ criada por \d+: "Quake" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
    verificar(bool(j), "la ventana de Quake está en el registro")
    return tuple(map(int, j[-1]))


def area(m, nome, cheia=False):
    """Los píxeles del interior de la ventana del juego (o de la pantalla), de 2 en 2."""
    w, h, rgb = pantalla(m, nome)
    if cheia:
        larg, alto, x0, y0 = w, h, 0, 0
    else:
        larg, alto, x0, y0 = janela_jogo(m)
    px = []
    for y in range(y0 + 4, min(y0 + alto, h) - 4, 2):
        fila = rgb[(y * w + x0 + 4) * 3:(y * w + min(x0 + larg, w) - 4) * 3]
        px.extend(fila[i:i + 3] for i in range(0, len(fila), 6))
    return px


def diferentes(a, b):
    n = min(len(a), len(b))
    return 100.0 * sum(1 for i in range(n) if a[i] != b[i]) / max(n, 1)


def no_negros(px):
    return 100.0 * sum(1 for p in px if sum(p) > 60) / max(len(px), 1)


def focar_jogo(m):
    larg, alto, x0, y0 = janela_jogo(m)
    m.clique(x0 + larg // 2, y0 + alto // 2)
    time.sleep(0.5)


def sem_falhas(m, desde, rotulo):
    s = m.serial()[desde:]
    verificar("Fallo de página" not in s and "Fallo de p" not in s, f"{rotulo}: sin faltas de página")
    verificar("Sys_Error" not in s, f"{rotulo}: sin errores fatales del motor (Sys_Error)")


def mover(m, tecla, rotulo, nome):
    quieto1 = area(m, nome + "-q1")
    time.sleep(1.5)
    quieto2 = area(m, nome + "-q2")
    m.monitor("sendkey %s 2000" % tecla)
    time.sleep(2.3)
    movido = area(m, nome + "-m")
    dq, dm = diferentes(quieto1, quieto2), diferentes(quieto2, movido)
    print("    cambio quieto %.1f%%, con «%s» %.1f%%" % (dq, tecla, dm))
    verificar(dm > 15 and dm > dq + 8, rotulo)


def consola(m, orden, cerrar=True):
    """Escribe una orden en la consola de Quake (la abre y, si cerrar, la cierra;
    «map» la cierra sola: Host_Map_f pasa a key_game)."""
    m.tecla("grave_accent", 0.6)
    m.digitar(orden, 0.25)
    m.tecla("ret", 0.5)
    if cerrar:
        m.tecla("grave_accent", 0.6)


# ------------------------------------------------------------------ juego

def juego():
    if not os.path.isfile(QUAKE):
        raise AssertionError("falta %s (make con QUAKE=1, fuera de la variante sin GPL)" % QUAKE)
    carpeta = carpeta_quake(JOGOS)
    if not carpeta:
        print("AVISO: no hay datos de Quake en %s (tools/quake_datos.sh o input_share/games/Quake): "
              "se salta la parte «juego»" % JOGOS)
        return
    print("datos de Quake en /jogos/%s" % carpeta)
    img = imagem(JOGOS)
    with Maquina("h229-juego", "ide", imagem=img, som="hda-output") as m:
        terminal(m)
        marca = abrir_pelo_menu(m)
        esperar(m, r"\[QUAKE\] datos en /jogos/\S+", 60, desde=marca)
        esperar(m, r"\[QUAKE\] ventana 640x400 creada \(superficie 320x200, 8 bits\)", 120, desde=marca)
        verificar(True, "Quake abre su ventana de 640x400 (320x200 de 8 bits, al doble)")
        esperar(m, r"\[QUAKE\] sonido 22050 Hz, 2 canales, 16 bits", 60, desde=marca)
        verificar(True, "abre el sonido (22050 Hz, estéreo, 16 bits)")
        esperar(m, r"\[QUAKE\] motor iniciado", 120, desde=marca)
        esperar(m, r"\[QUAKE\] primer cuadro dibujado", 60, desde=marca)
        s = m.serial()[marca:]
        verificar(re.search(r"\[QUAKE\] (Playing shareware version|Playing registered version)", s),
                  "el motor reconoce los datos (shareware o registrada)")
        time.sleep(12)          # la demo de la presentación (demo1)
        demo = area(m, "h229-demo")
        cores, luz = len(set(demo)), no_negros(demo)
        verificar(cores > 24 and luz > 20, f"se ve la demo ({cores} colores, {luz:.0f}% no negro)")
        m.captura("H229-2-demo", guardar=True)
        sem_falhas(m, marca, "hasta la demo")

        # El menú y una partida nueva: Single Player > New Game (el mapa «start»).
        focar_jogo(m)
        m.tecla("esc", 1.5)
        m.captura("H229-3-menu", guardar=True)
        desde = len(m.serial())
        m.tecla("ret", 1.5)     # Single Player
        m.tecla("ret", 1.0)     # New Game
        esperar(m, r"\[QUAKE\] Introduction", 120, desde=desde)
        verificar(True, "«New Game» carga el mapa de entrada («Introduction»)")
        time.sleep(6)
        mover(m, "up", "el jugador avanza con la flecha arriba: la pantalla cambia mucho más que quieta",
              "h229-start")
        m.captura("H229-4-partida", guardar=True)
        sem_falhas(m, marca, "la partida")

        # La dificultad y el primer mapa por la consola.
        desde = len(m.serial())
        consola(m, "skill 1")
        consola(m, "map e1m1", cerrar=False)
        esperar(m, r"\[QUAKE\] the Slipgate Complex", 120, desde=desde)
        verificar(True, "con la consola, dificultad 1 y el mapa e1m1 («the Slipgate Complex»)")
        time.sleep(6)
        mover(m, "left", "en e1m1 el jugador gira con la flecha izquierda", "h229-e1m1")
        m.captura("H229-5-e1m1", guardar=True)
        sem_falhas(m, marca, "e1m1")

        # Salir: cerrar la ventana (SDL_QUIT -> Sys_Quit, que guarda config.cfg).
        desde = len(m.serial())
        m.monitor("sendkey alt-f4")
        try:
            esperar(m, r"\[QUAKE\] fin \(0\)", 40, desde=desde)
        except AssertionError:
            consola(m, "quit")
            esperar(m, r"\[QUAKE\] fin \(0\)", 40, desde=desde)
        verificar(True, "Quake sale y vuelve el control al sistema")
        sem_falhas(m, marca, "toda la sesión")
        time.sleep(2)
        h136.focar_terminal(m)
        out = m.comando("ls /home/quake/id1")
        verificar("config.cfg" in out, f"config.cfg queda en /home/quake/id1 ({out.strip()[-120:]})")
    esq, dir_ = h70.ler_gravacao(m.wav)
    rms = h70.rms(esq[:48000 * 60])
    verificar(rms > 100, f"suena: los sonidos de la demo y de la partida llegan a la tarjeta (RMS {rms:.0f})")


# ------------------------------------------------------------------ sin datos

def sin_datos():
    if not os.path.isfile(QUAKE):
        raise AssertionError("falta %s" % QUAKE)
    img = imagem(jogos_sem_quake())
    with Maquina("h229-sin", "ide", imagem=img) as m:
        terminal(m)
        marca = abrir_pelo_menu(m)
        esperar(m, r"\[QUAKE\] faltan los datos: ventana de ayuda", 60, desde=marca)
        time.sleep(2)
        j = janela(m.serial(), "Quake: faltan los datos")
        verificar(j["l"] >= 600, "sin datos sale la ventana que explica cómo conseguirlos (%dx%d)" % (j["l"], j["a"]))
        m.captura("H229-6-sin-datos", guardar=True)
        # «Cerrar»: x 340..450, y = alto - 46 (alto = 60 + 12 líneas * 18 + 50 = 326).
        m.clique(j["x"] + 340 + 55, j["y"] + 24 + (326 - 46) + 14)
        time.sleep(2)
        s = m.serial()[marca:]
        verificar("ventana 640x400 creada" not in s, "sin datos el juego no arranca")
        verificar("Fallo de página" not in s, "sin datos no hay faltas de página")


# ------------------------------------------------------------------ pantalla completa

def cheia():
    if not carpeta_quake(JOGOS) or not os.path.isfile(QUAKE):
        print("AVISO: sin datos de Quake o sin Quake: se salta la parte «cheia»")
        return
    img = imagem(JOGOS)
    with Maquina("h229-cheia", "ide", imagem=img) as m:
        terminal(m)
        marca = abrir_pelo_menu(m, "Quake (pantalla completa)")
        esperar(m, r"\[QUAKE\] pantalla completa: activada", 120, desde=marca)
        verificar(True, "«Quake (pantalla completa)» abre a pantalla completa")
        esperar(m, r"\[QUAKE\] motor iniciado", 120, desde=marca)
        time.sleep(12)
        px = area(m, "h229-cheia", cheia=True)
        verificar(no_negros(px) > 15, "a pantalla completa se ve la demo (%.0f%% no negro)" % no_negros(px))
        m.captura("H229-7-pantalla-completa", guardar=True)
        sem_falhas(m, marca, "pantalla completa")


PARTES = {"unidad": unidad, "juego": juego, "sin-datos": sin_datos, "cheia": cheia}


def main():
    os.makedirs(SAIDA, exist_ok=True)
    pedidas = sys.argv[1:] or list(PARTES)
    for p in pedidas:
        if p not in PARTES:
            print("uso: h229_quake.py [%s]" % " ".join(PARTES))
            return 2
    for p in pedidas:
        print("== %s" % p)
        PARTES[p]()
    return 0


if __name__ == "__main__":
    sys.exit(main())
