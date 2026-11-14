#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H136 - o OpenTyrian corre dentro do SaguiOS (D156, D181).

Critério:
  - o jogo (terceiros, GPL-2.0, isolado em userland/terceros/opentyrian/) abre
    a sua janela e desenha o ecrã de título;
  - parte A (D181, D228): os dados do jogo vão na partição SAGUI-JOGOS da
    própria imagem; o /etc/fstab monta-a de leitura e escrita em /jogos ao
    arrancar (um jogo por pasta, D228) e o jogo encontra-os em
    /jogos/opentyrian, sem nenhum disco aparte;
  - parte B (D156): sem essa partição (a imagem arranca igual), os dados num
    pendrive montado em /usb: o jogo encontra-os em /usb/jogos/opentyrian;
  - as teclas chegam ao jogo: as setas e o Enter chegam como setas e Enter
    (e não como letras), para que o menu responda;
  - o jogo não se cai (nenhuma falta de página).

Os dados vão no repositório numa imagem comprimida
(userland/terceros/opentyrian/dados.img.xz): a parte B descomprime-a e liga-a
ao QEMU como pendrive USB; a parte A usa a raiz da partição que o make
monta (build/jogos/raiz, com opentyrian/ dentro).
"""
import os
import re
import subprocess
import sys
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))
from qemu import Maquina, verificar, imagem_de_teste, SAIDA, BUILD  # noqa: E402

DADOS_XZ = os.path.join(RAIZ, "userland", "terceros", "opentyrian", "dados.img.xz")
PENDRAIVE = os.path.join(SAIDA, "h133-tyrian.img")
JOGOS = os.path.join(BUILD, "jogos", "raiz")   # a raiz de SAGUI-JOGOS que monta o make (D181, D228)


def preparar_pendrive():
    """Descomprime a imagem dos dados e deixa-a como pendrive."""
    if not os.path.exists(DADOS_XZ):
        raise AssertionError(
            "faltam os dados do jogo (%s); com a pasta do shareware do Tyrian "
            "se pode refazer com tools/imagem_tyrian.sh --dados PASTA" % DADOS_XZ)
    if os.path.exists(PENDRAIVE):
        os.remove(PENDRAIVE)
    with open(DADOS_XZ, "rb") as f:
        bruto = f.read()
    xz = subprocess.run(["xz", "-dc"], input=bruto, capture_output=True, check=True)
    with open(PENDRAIVE, "wb") as f:
        f.write(xz.stdout)
    return PENDRAIVE


def esperar(m, patron, tempo=40, desde=0):
    """Espera um texto no registo (a serial) e devolve a última ocorrência."""
    limite = time.time() + tempo
    while time.time() < limite:
        encontrados = re.findall(patron, m.serial()[desde:])
        if encontrados:
            return encontrados[-1]
        time.sleep(0.2)
    raise AssertionError("tempo esgotado esperando %r" % patron)


def focar_terminal(m):
    """Clica na janela do Terminal para que tenha o foco do teclado.

    Ao ligar, o sistema abre o Terminal com o sagsh dentro, mas a notificacao
    do pendrive (que aparece quando se liga um disco) rouba o foco, e as teclas
    que se enviam enquanto isso nao chegam ao shell.
    """
    s = m.serial().replace("\r", "")
    janela = re.search(r'janela \d+ criada por \d+: "Terminal" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
    if not janela:
        return False
    largura, altura, x, y = map(int, janela.groups())
    m.clique(x + largura // 2, y + altura // 2)
    time.sleep(0.5)
    return True


def focar_jogo(m):
    """Clica dentro de la ventana del juego para que tenga el foco del teclado.

    La ventana del Terminal queda con el foco (y la notificacion del pendrive
    tambien lo roba), y las teclas del juego solo llegan con su ventana
    enfocada. Se espera a que la notificacion se vaya y se hace clic dentro
    de la ventana (no en la barra del titulo, que la moveria).
    """
    time.sleep(5)
    s = m.serial().replace("\r", "")
    janela = re.search(r'janela \d+ criada por \d+: "OpenTyrian" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
    if not janela:
        return False
    largura, altura, x, y = map(int, janela.groups())
    m.clique(x + largura // 2, y + altura // 2)
    time.sleep(0.5)
    return True


def jogar(m):
    """Arranca o jogo desde o Terminal e comprova a janela, os dados e as teclas."""
    desde = len(m.serial())
    m.digitar("/bin/opentyrian")
    m.tecla("ret")
    esperar(m, r"\[OPENTYRIAN\] janela 640x400 criada", 90, desde=desde)
    verificar(True, "o jogo abre a sua janela de 640x400")
    s = m.serial()[desde:].replace("\r", "")
    verificar("failed to open" not in s.split("janela 640x400 criada")[-1],
              "o jogo encontra os seus dados (nenhum ficheiro em falta)")
    verificar("Fallo de página" not in s, "o jogo não se cai ao arrancar")
    m.captura("H133-opentyrian-titulo", guardar=False)

    # ---- as teclas chegam bem ao jogo ----
    focar_jogo(m)
    desde = len(m.serial())
    m.tecla("down")
    time.sleep(0.5)
    m.tecla("ret")
    # O jogo le os eventos no seu proprio ciclo: espera-se pelo registo.
    esperar(m, r"\[OPENTYRIAN\] tecla \d+ \(SDL 13\)", 30, desde=desde)
    s = m.serial()[desde:].replace("\r", "")
    teclas = re.findall(r"\[OPENTYRIAN\] tecla (\d+) \(SDL (\d+)\)", s)
    verificar(bool(teclas), "o jogo recebe as teclas")
    if teclas:
        print("    teclas:", teclas)
        # SDL: 274 = baixo (SDLK_DOWN), 13 = Enter (SDLK_RETURN)
        verificar(any(t[1] == "274" for t in teclas),
                  "a seta para baixo chega como seta (SDL 274)")
        verificar(any(t[1] == "13" for t in teclas),
                  "o Enter chega como Enter (SDL 13), e não como uma letra")
    m.captura("H133-opentyrian-menu", guardar=False)


def leer_ppm(ruta):
    """Lee un PPM (P6) y devuelve (ancho, alto, bytes RGB)."""
    with open(ruta, "rb") as f:
        datos = f.read()
    campos, pos = [], 0
    while len(campos) < 4:
        while datos[pos:pos + 1].isspace():
            pos += 1
        if datos[pos:pos + 1] == b"#":
            pos = datos.index(b"\n", pos)
            continue
        fin = pos
        while not datos[fin:fin + 1].isspace():
            fin += 1
        campos.append(datos[pos:fin])
        pos = fin
    return int(campos[1]), int(campos[2]), datos[pos + 1:]


def area_de_juego_negra(m):
    """Porcentaje de pixeles NO negros del area de juego (la parte izquierda de
    la ventana, sin el panel lateral) y numero de colores distintos."""
    s = m.serial().replace("\r", "")
    j = re.findall(r'janela \d+ criada por \d+: "OpenTyrian" (\d+)x(\d+) em \((\d+),(\d+)\)', s)
    larg, alto, x0, y0 = map(int, j[-1])
    ppm = os.path.join(SAIDA, "h136-area.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor("screendump " + ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    w, h, rgb = leer_ppm(ppm)
    os.remove(ppm)
    # El motor dibuja 320x200 al doble: el area de juego son 264x184 pixeles
    # desde (24,0); el panel lateral queda fuera. Se deja un margen.
    xa, xb = x0 + 2 * 24 + 4, x0 + 2 * 24 + 2 * 264 - 4
    ya, yb = y0 + 4, y0 + 2 * 184 - 4
    total = no_negros = 0
    colores = set()
    for y in range(ya, yb, 2):
        fila = rgb[(y * w + xa) * 3:(y * w + xb) * 3]
        for i in range(0, len(fila), 6):
            px = fila[i:i + 3]
            total += 1
            if px != b"\x00\x00\x00":
                no_negros += 1
                colores.add(px)
    print("    area de juego: %d%% no negro, %d colores (ventana %dx%d en %d,%d)"
          % (100 * no_negros // max(total, 1), len(colores), larg, alto, x0, y0))
    return 100.0 * no_negros / max(total, 1), len(colores)


def partida(m):
    """Empieza una partida de verdad: título, nueva partida, modo, episodio y
    dificultad con Enter, y unos segundos de juego. Con la pila de 64 KB del
    sistema, el motor se salía de ella justo al elegir la dificultad
    («Fallo de página: escritura 0xffbefd..»): la plataforma le da una pila
    propia (D183)."""
    focar_jogo(m)
    m.tecla("esc")          # volver al menú principal si quedó en otro
    time.sleep(1.5)
    desde = len(m.serial())
    for _ in range(4):      # jugadores, episodio, dificultad... hasta el Game Menu
        m.tecla("ret")
        time.sleep(2.5)
    for _ in range(4):      # Data, Ship Specs, Upgrade Ship, Options -> Play Next Level
        m.tecla("down")
        time.sleep(0.5)
    m.tecla("ret")          # Play Next Level: elige el nivel (Tyrian)
    time.sleep(2.5)
    m.tecla("ret")          # y empieza el nivel
    time.sleep(10)          # unos segundos de partida
    s = m.serial()[desde:].replace("\r", "")
    verificar("Fallo de página" not in s and "Fallo de p" not in s,
              "elegir la dificultad y jugar no se sale de la pila (sin fallo de página)")
    verificar("(opentyrian) terminou" not in s, "el juego sigue vivo tras empezar la partida")
    # El fondo (el terreno que se desplaza) se ve: el area de juego no es
    # casi toda negra (D187). Antes solo salia un rectangulo de ~145x110 px.
    m.captura("H136-opentyrian-partida", guardar=True)
    pct, colores = area_de_juego_negra(m)
    verificar(pct > 50 and colores > 8,
              "el fondo del juego se ve: el area de juego no es casi toda negra (%d%%)" % pct)


def parte_particao():
    """Parte A (D181): os dados na partição SAGUI-JOGOS da própria imagem."""
    if not os.path.exists(os.path.join(JOGOS, "opentyrian", "tyrian1.lvl")):
        raise AssertionError("falta %s/opentyrian (o make extrai-a de dados.img.xz)" % JOGOS)
    imagem = imagem_de_teste("", ["--vesa", "--jogos", JOGOS])
    with Maquina("h136-part", "ide", imagem=imagem) as m:
        esperar(m, r"sagui:/\$", 90)
        s = m.serial().replace("\r", "")
        verificar(re.search(r"\[FSTAB\] LABEL=SAGUI-JOGOS -> /jogos \(auto, rw\): 0", s),
                  "o /etc/fstab monta a partição SAGUI-JOGOS em /jogos (leitura e escrita) ao arrancar")
        verificar(not re.search(r"\[FSTAB\] arranque -> /home", s),
                  "a partição de jogos não é tomada pelo /home (arranque)")
        time.sleep(3)
        focar_terminal(m)
        out = m.comando("ls /jogos")
        verificar("chip8" in out and "opentyrian" in out and "sagui3d" in out,
                  "/jogos tem os jogos da base (chip8, sagui3d: o make copia-os à partição, D228) ao lado de opentyrian")
        out = m.comando("ls /jogos/opentyrian")
        verificar("tyrian1.lvl" in out and "newsh~.shp" in out, "os dados estão em /jogos/opentyrian")
        out = m.comando("touch /jogos/opentyrian/escrita.txt")
        out2 = m.comando("ls /jogos/opentyrian")
        verificar("escrita.txt" in out2, "a partição de jogos é de leitura e escrita (D228)")
        m.comando("rm /jogos/opentyrian/escrita.txt")
        jogar(m)
        partida(m)


def parte_pendrive():
    """Parte B (D156): sem a partição, os dados num pendrive em /usb."""
    pendrive = preparar_pendrive()
    # A imagem sem a partição de jogos: o sistema tem de arrancar igual.
    imagem = imagem_de_teste("", ["--vesa"])
    try:
        with Maquina("h136", "ide", imagem=imagem,
                     extras=["-device", "usb-ehci,id=ehci",
                             "-drive", "if=none,id=pen,format=raw,file=" + pendrive,
                             "-device", "usb-storage,bus=ehci.0,drive=pen,removable=on,id=dev-pen"]) as m:
            # ---- o pendrive aparece e monta-se ----
            esperar(m, r"\[DISCO\] usb0: .*TYRIAN", 60)
            verificar(True, "o pendrive com os dados do jogo aparece no sistema")
            # A notificação do pendrive rouba o foco: espera-se que passe e
            # dá-se o foco à janela do Terminal com um clique.
            esperar(m, r"sagui:/\$", 60)
            verificar("SAGUI-JOGOS -> " not in m.serial(), "sem a partição de jogos o sistema arranca igual")
            time.sleep(4)
            focar_terminal(m)
            desde = len(m.serial())
            m.digitar("montar usb0")
            m.tecla("ret")
            esperar(m, r"\[MONTAGEM\] usb0.*montado em /usb", 40, desde=desde)
            verificar(True, "o pendrive monta-se em /usb")
            jogar(m)
    finally:
        if os.path.exists(PENDRAIVE):
            os.remove(PENDRAIVE)


def main():
    parte_particao()
    parte_pendrive()
    return 0


if __name__ == "__main__":
    sys.exit(main())
