#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""H389 - AoEN, RTS estilo Age of Empires (D382, mejora 140).

Partes (por defecto todas; o las que se pidan: h389_aoen.py generador sin-datos con-datos):
  generador  en el PC: tools/aoen_drs_sintetico.py genera unos DRS mínimos SINTÉTICOS (nada del juego original) con la
             estructura correcta (cabecera, tablas, SLP con bordes y órdenes de dibujo y paleta JASC).
  licencia   las fuentes de userland/juegos/aoen llevan SPDX GPL-3.0-or-later, stb_image está en terceros, hay D382,
             la mejora 140 y la fila de stb_image en docs/terceros.md; el Makefile no copia texturas ni música a git.
  sin-datos  en SaguiOS: Inicio > Juegos tiene AoEN; sin los DRS abre su ventana con las instrucciones.
  con-datos  en SaguiOS con los DRS sintéticos en /jogos/AOE/DATA (lo que busca por defecto): arranca una escaramuza,
             selecciona unidades, las mueve con el botón derecho y guarda (F5) y carga (F9) la partida.
  lectores   en el PC con ASan: DRS, SLP y .sav manipulados dan error limpio (tests/aoen_lectores.c).
  efectos    perfil por defecto, obras y ruinas, y la interfaz escalada a 1920x1080.
  funciones  v8.5.4.7: asedio, murallas, muelle y flota con las escenas AOEN_ESCENA (capturas H389-10 a H389-13).
  reglas     en el PC: victoria compartida, recursos del mapa y comercio (barcos y carros mercantes).
  ia         en el PC: partidas de IA contra IA aceleradas (tests/aoen_ia.c) que terminan, con la difícil ganando a la fácil.
  interfaz   v8.5.4.7: la interfaz con los guiones AOEN_PRUEBA (configuración, ayudas, reunión, cola, patrulla, alerta,
             diplomacia, pantalla final) y sin solapes a 800x600 y 1920x1080 (capturas H389-20 a H389-29).
  presentacion v8.5.5.3: rótulo del menú, opciones en pestañas (interfaz y teclas), cursor propio según la orden con el
             rótulo de lo que hay bajo el ratón, la hoja de cursores y la resolución de dibujo al 75 % en pantalla completa
             (capturas H389-33 a H389-38; H389-8 enseña el fuego de los edificios muy dañados).
  bench      (aparte) fps a 800x600 y 1024x768, con efectos 0, 1 y 2; H389_RES, H389_EFECTOS, H389_BENCH, H389_RENDER,
             H389_DIBUJO (resolución de dibujo en %, v8.5.5.3).
Capturas en docs/capturas/H389-*.png. Los DRS reales nunca se usan aquí.
"""
import os
import re
import subprocess
import sys
import tempfile
import time

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(RAIZ, "tests"))



def png_rgb(ruta):
    """Píxeles RGB (bytes r,g,b seguidos) de un PNG de 8 bits RGB o RGBA, en Python puro (sin ImageMagick)."""
    import struct
    import zlib
    datos = open(ruta, "rb").read()
    pos, idat, ancho, alto, tipo = 8, b"", 0, 0, 2
    while pos < len(datos):
        n, nombre = struct.unpack(">I4s", datos[pos:pos + 8])
        cuerpo = datos[pos + 8:pos + 8 + n]
        if nombre == b"IHDR":
            ancho, alto, _, tipo = struct.unpack(">IIBB", cuerpo[:10])
        elif nombre == b"IDAT":
            idat += cuerpo
        pos += 12 + n
    bpp = 4 if tipo == 6 else 3
    crudo = zlib.decompress(idat)
    fila, ant, out, i = ancho * bpp, bytearray(ancho * bpp), bytearray(), 0
    for _ in range(alto):
        f, linea = crudo[i], bytearray(crudo[i + 1:i + 1 + fila])
        i += 1 + fila
        for x in range(fila):
            a = linea[x - bpp] if x >= bpp else 0
            b = ant[x]
            c = ant[x - bpp] if x >= bpp else 0
            if f == 1: linea[x] = (linea[x] + a) & 255
            elif f == 2: linea[x] = (linea[x] + b) & 255
            elif f == 3: linea[x] = (linea[x] + (a + b) // 2) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                linea[x] = (linea[x] + (a if pa <= pb and pa <= pc else b if pb <= pc else c)) & 255
        ant = linea
        out += linea if bpp == 3 else bytes(v for k, v in enumerate(linea) if k % 4 != 3)
    return bytes(out)

def generador():
    from qemu import verificar
    d = tempfile.mkdtemp(prefix="aoen-drs-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), d], check=True, stdout=subprocess.DEVNULL)
    for n in ("GRAPHICS.DRS", "INTERFAC.DRS", "TERRAIN.DRS"):
        b = open(os.path.join(d, n), "rb").read()
        verificar(len(b) > 64 and b[40:44] == b"1.00", "%s tiene cabecera DRS (%d bytes)" % (n, len(b)))
    b = open(os.path.join(d, "INTERFAC.DRS"), "rb").read()
    verificar(b"JASC-PAL" in b, "INTERFAC.DRS lleva la paleta JASC")
    verificar(b"Copyright" in open(os.path.join(d, "TERRAIN.DRS"), "rb").read()[:64] and b"sintetico" in b, "los DRS se declaran sintéticos")


def licencia():
    from qemu import verificar
    d = os.path.join(RAIZ, "userland", "juegos", "aoen")
    fuentes = [f for f in os.listdir(d) if f.endswith((".c", ".h"))]
    verificar(len(fuentes) >= 15, "hay %d fuentes de AoEN" % len(fuentes))
    for f in fuentes:
        t = open(os.path.join(d, f), encoding="utf-8").readline()
        verificar("SPDX-License-Identifier: GPL-3.0-or-later" in t, "%s lleva SPDX GPL-3.0-or-later" % f)
    verificar(os.path.isfile(os.path.join(RAIZ, "userland", "terceros", "stbimage", "stb_image.h")), "stb_image.h vive en userland/terceros/stbimage")
    t = open(os.path.join(RAIZ, "docs", "terceros.md"), encoding="utf-8").read()
    verificar("stb_image" in t, "docs/terceros.md tiene la fila de stb_image")
    t = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar("## D382" in t, "docs/decisoes.md tiene D382")
    verificar(os.path.isfile(os.path.join(RAIZ, "mejoras", "140-aoen.md")), "existe mejoras/140-aoen.md")
    ig = subprocess.run(["git", "ls-files", "userland/juegos", "input_share"], cwd=RAIZ, capture_output=True, text=True).stdout
    verificar(not re.search(r"\.(drs|DRS|mid|png|o)$", ig, re.M), "ninguna textura, música ni DRS en git")


def ventana(m, desde=0):
    g = re.findall(r'criada por \d+: "AoEN" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial()[desde:])
    if not g:
        return None
    l, a, x, y = [int(v) for v in g[-1]]
    return dict(l=l, a=a, x=x + 2, y=y + 24)


def sin_datos():
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import painel, submenu_pelo_mouse, esperar, clicar_aqui
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta la parte «sin-datos»")
        return
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h389-sindatos", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        sub = submenu_pelo_mouse(m, p, "Juegos", "AoEN")
        verificar("AoEN" in sub["linhas"], "Inicio > Juegos tiene AoEN (%s)" % ", ".join(sub["linhas"]))
        time.sleep(0.4)
        m.captura("H389-1-menu-juegos")
        marca = len(m.serial())
        clicar_aqui(m)
        esperar(m, r"inicio -> /apps/bin/aoen", 30, desde=marca)
        esperar(m, r"\[AOEN\] sin datos del juego", 60, desde=marca)
        time.sleep(2)
        verificar(ventana(m, marca) is not None, "AoEN abre su ventana con las instrucciones")
        m.captura("H389-2-instrucciones")


def con_datos():
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ, SAIDA
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta la parte «con-datos»")
        return
    jogos = tempfile.mkdtemp(prefix="aoen-jogos-")
    drs = os.path.join(jogos, "AOE", "DATA")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), drs], check=True, stdout=subprocess.DEVNULL)
    img = imagem_de_teste("", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h389-condatos", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        h136.focar_terminal(m)
        marca = len(m.serial())
        # AOEN_SCENE=game arranca una escaramuza de 4 jugadores sin pasar por los menús; AOEN_SELECT deja 3 aldeanos elegidos
        m.digitar("AOEN_SCENE=game AOEN_SELECT=1 /apps/bin/aoen", 0.04)
        m.tecla("ret")
        esperar(m, r"\[AOEN\] inicio: ventana \d+x\d+, datos /jogos/AOE/DATA", 90, desde=marca)
        verificar(True, "AoEN encuentra los datos en /jogos/AOE/DATA")
        esperar(m, r"\[AOEN\] partida iniciada: 4 jugadores", 120, desde=marca)
        e = esperar(m, r"\[AOEN\] estado t=\d+ ents=(\d+) sel=3 pos=(-?[\d.]+),(-?[\d.]+) fps=(\d+)", 120, desde=marca)
        verificar(int(e[0]) > 20, "la escaramuza tiene %s entidades y 3 aldeanos seleccionados" % e[0])
        x0, y0 = float(e[1]), float(e[2])
        time.sleep(3)
        m.captura("H389-3-escaramuza")
        v = ventana(m, marca)
        verificar(v is not None, "AoEN abre su ventana %dx%d" % (v["l"], v["a"]) if v else "AoEN abre su ventana")
        # enfocar el juego y dar la orden de mover con el botón derecho en un punto del mapa
        h136.focar_jogo = h136.focar_jogo
        m.clique(v["x"] + v["l"] // 2, v["y"] + v["a"] // 2 - 120, botao=1)   # clic izquierdo: enfoca (y deselecciona)
        time.sleep(1)
        # volver a elegir con una caja alrededor del centro urbano (donde empiezan los aldeanos)
        marca = len(m.serial())
        m.arrastar(v["x"] + v["l"] // 2 - 140, v["y"] + v["a"] // 2 - 110, v["x"] + v["l"] // 2 + 140, v["y"] + v["a"] // 2 + 110)
        esperar(m, r"\[AOEN\] selecci\S+n con caja: [1-9]\d*", 30, desde=marca)
        verificar(True, "la selección con caja elige unidades")
        time.sleep(1.5)
        m.captura("H389-4-seleccion")
        marca = len(m.serial())
        m.clique(v["x"] + v["l"] // 2 + 150, v["y"] + v["a"] // 2 + 60, botao=2)
        esperar(m, r"\[AOEN\] orden mover: \d+ unidades hacia", 30, desde=marca)
        verificar(True, "el botón derecho da la orden de mover")
        # se sigue a una unidad fija (id) y se espera por tiempo de juego, no de reloj: con TCG lento 8 s de reloj pueden ser
        # pocos segundos de juego
        patron = r"\[AOEN\] estado t=\d+ ents=\d+ sel=[1-9]\d* pos=(-?[\d.]+),(-?[\d.]+) fps=\d+ id=(\d+) dest=(-?[\d.]+),(-?[\d.]+) st=(\d+) tj=([\d.]+)"
        esperar(m, patron, 30, desde=marca)
        primero = re.findall(patron, m.serial()[marca:])[0]
        uid, t0 = primero[2], float(primero[6])
        fin = time.time() + 120
        while time.time() < fin:
            est = [x for x in re.findall(patron, m.serial()[marca:]) if x[2] == uid]
            if est and float(est[-1][6]) >= t0 + 8:
                break
            time.sleep(1)
        m.captura("H389-5-unidades-movidas")
        est = [x for x in re.findall(patron, m.serial()[marca:]) if x[2] == uid]
        verificar(len(est) >= 2, "hay registros de estado de la unidad %s tras la orden" % uid)
        d = ((float(est[-1][0]) - float(est[0][0])) ** 2 + (float(est[-1][1]) - float(est[0][1])) ** 2) ** 0.5
        resto = ((float(est[-1][0]) - float(est[-1][3])) ** 2 + (float(est[-1][1]) - float(est[-1][4])) ** 2) ** 0.5
        verificar(d > 1.0 or resto < 1.0, "la unidad %s se movió %.1f casillas hacia el destino en %.1f s de juego (le quedan %.1f)" %
                  (uid, d, float(est[-1][6]) - t0, resto))
        # guardar y cargar con F5 / F9
        marca = len(m.serial())
        m.tecla("f5")
        esperar(m, r"\[AOEN\] guardada ranura 1 \(\d+ entidades\)", 30, desde=marca)
        verificar(True, "F5 guarda la partida")
        marca = len(m.serial())
        m.tecla("f9")
        esperar(m, r"\[AOEN\] cargada ranura 1 \(\d+ entidades\)", 30, desde=marca)
        verificar(True, "F9 carga la partida")
        time.sleep(3)
        m.captura("H389-6-partida-cargada")
        fps = [int(f) for f in re.findall(r"\[AOEN\] estado .* fps=(\d+)", m.serial())]
        print("    fps observados (ventana 1000x700, TCG sin KVM):", fps[-5:])
        verificar(bool(fps) and max(fps) >= 1, "el juego dibuja cuadros (fps máx. %d)" % max(fps))


def funciones():
    """v8.5.4.7, en SaguiOS con DRS sintéticos: asedio (catapultas con daño de área), murallas (Muralla media y Fortificación resisten más),
    flota (muelle solo en la orilla, barco de pesca que entrega comida, transporte que embarca y desembarca, trirreme que dispara a tierra).
    Usa las escenas AOEN_ESCENA del juego. Capturas H389-10 a H389-13."""
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta la parte «funciones»")
        return
    jogos = tempfile.mkdtemp(prefix="aoen-jogos-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")], check=True, stdout=subprocess.DEVNULL)
    img = imagem_de_teste("h389-funciones", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    base = "AOEN_SCENE=game AOEN_SIZE=4 AOEN_PLAYERS=2 AOEN_SEED=6 AOEN_FFWD=%d AOEN_ESCENA=%s AOEN_SHOT=/tmp/aoen.bmp AOEN_FRAMES=%d /apps/bin/aoen"

    def correr(m, escena, ffwd, frames, captura=None):
        marca = len(m.serial())
        m.digitar(base % (ffwd, escena, frames), 0.04)
        m.tecla("ret")
        esperar(m, r"\[AOEN\] escena %s" % escena, 240, desde=marca)
        if captura:
            esperar(m, r"\[AOEN\] estado t=", 240, desde=marca)
            time.sleep(2)
            m.captura(captura)
        esperar(m, r"sagui:/\$", 400, desde=marca)   # el juego sale solo tras AOEN_FRAMES cuadros
        return m.serial()[marca:]

    with Maquina("h389-funciones", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        h136.focar_terminal(m)

        t = correr(m, "asedio", 45, 150, "H389-10-asedio")
        verificar("asedio: Catapulta destruye Cuartel" in t or "asedio: Catapulta pesada destruye Cuartel" in t, "las catapultas destruyen el cuartel enemigo")
        verificar(bool(re.search(r"\[AOEN\] .{1,3}rea: Catapulta[ a-z]* alcanza [1-9]\d* objetivos m", t)), "la catapulta daña en área a los soldados de al lado")
        verificar("asedio: Balista destruye Clubman" in t, "la balista mata soldados")

        hp = {}
        for e in ("muralla", "muralla2"):
            t = correr(m, e, 22, 40, "H389-11-muralla" if e == "muralla" else None)
            g = re.search(r"\[AOEN\] %s: pv de la muralla (\d+) de (\d+)" % e, t)
            verificar(g is not None, "%s: la catapulta golpea la muralla (%s)" % (e, g.groups() if g else "sin registro"))
            hp[e] = int(g.group(1))
        verificar(hp["muralla2"] > hp["muralla"], "con Muralla media y Fortificación la muralla aguanta más (%d contra %d pv)" % (hp["muralla2"], hp["muralla"]))

        t = correr(m, "mar", 60, 150, "H389-12-mar")
        verificar("muelle: orilla=1 tierra_adentro=0" in t, "el muelle solo se puede poner en la orilla")
        verificar(len(re.findall(r"transporte: embarca Clubman", t)) == 3, "tres soldados embarcan en el transporte")
        verificar("transporte: desembarcan 3 unidades" in t, "el transporte desembarca los tres soldados en la otra orilla")
        e = re.findall(r"barco de pesca lleva \d+ de comida, entregado (\d+)", t)
        verificar(bool(e) and int(e[-1]) >= 10, "el barco de pesca entrega comida en el muelle (%s)" % (e[-1] if e else "nada"))

        t = correr(m, "minimo", 20, 60)
        d0 = re.search(r"minimo: distancia inicial ([\d.]+)", t); d1 = re.search(r"minimo: distancia final ([\d.]+)", t)
        verificar(d0 and d1 and float(d0.group(1)) < 2.0 and float(d1.group(1)) >= 2.4, "la catapulta empieza a %s casillas del enemigo y retrocede hasta %s (alcance mínimo 2,5)" % (d0.group(1) if d0 else "?", d1.group(1) if d1 else "?"))

        marca = len(m.serial())   # mapa de islas: cada jugador tiene costa y sitio para un muelle
        m.digitar("AOEN_SCENE=game AOEN_MAP=1 AOEN_SIZE=2 AOEN_PLAYERS=4 AOEN_SEED=6 AOEN_SHOT=/tmp/aoen.bmp AOEN_FRAMES=30 /apps/bin/aoen", 0.04)
        m.tecla("ret")
        esperar(m, r"sagui:/\$", 400, desde=marca)
        costas = re.findall(r"costa: el jugador (\d) tiene sitio para un muelle: (\d)", m.serial()[marca:])
        verificar(len(costas) == 4 and all(c[1] == "1" for c in costas), "en un mapa de islas los 4 jugadores tienen sitio para un muelle (%s)" % (costas,))

        t = correr(m, "flota", 45, 150, "H389-13-flota")
        verificar("flota: Trirreme destruye Clubman" in t, "el trirreme dispara a los soldados de la orilla")


def interfaz():
    """v8.5.4.7, en SaguiOS con DRS sintéticos: la interfaz nueva con los guiones AOEN_PRUEBA (ratón y teclado simulados
    cuadro a cuadro): configuración de la partida con el teclado y vista previa del mapa, ayuda emergente con el coste y el
    motivo de un botón gris, punto de reunión, órdenes en cola con Mayús, patrulla, alerta de ataque con Espacio,
    diplomacia y tributo, pantalla final con la gráfica; y a 800x600 y 1920x1080 a pantalla completa, sin solapes
    (AOEN_SOLAPES=1). Capturas H389-20 a H389-29."""
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta la parte «interfaz»")
        return
    jogos = tempfile.mkdtemp(prefix="aoen-jogos-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")], check=True, stdout=subprocess.DEVNULL)

    def correr(m, extra, frames, espera, captura=None):
        """lanza el juego con un guion; espera la línea `espera` del registro, hace la captura y espera a que salga solo"""
        marca = len(m.serial())
        m.digitar("AOEN_SHOT=/tmp/aoen.bmp AOEN_FRAMES=%d %s /apps/bin/aoen" % (frames, extra), 0.03)
        m.tecla("ret")
        g = esperar(m, espera, 300, desde=marca)
        if not isinstance(g, tuple):
            g = (g,)   # findall con un solo grupo devuelve la cadena
        if captura:
            time.sleep(0.6)
            m.captura(captura)
        esperar(m, r"\[AOEN\] captura guardada en el cuadro", 400, desde=marca)
        esperar(m, r"sagui:/\$", 60, desde=marca)
        return m.serial()[marca:], g

    img = imagem_de_teste("h389-interfaz", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h389-interfaz", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        h136.focar_terminal(m)
        juego = "AOEN_SCENE=game AOEN_SEED=7 AOEN_EFECTOS=1"

        t, g = correr(m, "AOEN_SCENE=setup AOEN_PRUEBA=teclado", 200, r"\[AOEN\] prueba teclado: civilizaci\S+n del jugador 1 = (\S+), foco", "H389-20-configuracion")
        verificar(g[0] == "Griegos", "con Abajo y Derecha se cambia la civilización del jugador 1 (%s)" % g[0])
        verificar(re.search(r"vista previa del mapa: \S+ \d+x\d+, semilla \d+, 4 jugadores", t) is not None, "la configuración genera la vista previa del mapa con los 4 inicios")

        t, g = correr(m, juego + " AOEN_PRUEBA=ayuda", 260, r"\[AOEN\] ayuda: Avanzar a la Edad de Herramientas \| coste 0/500/0/0 \| ([^\r\n]+)[\r\n]", "H389-21-ayuda")
        verificar("ayuda: Entrenar Aldeano | coste 0/50/0/0 | disponible" in t, "la ayuda del botón del aldeano dice su coste (50 de comida)")
        verificar("Faltan edificios" in g[0], "el botón de edad está gris y la ayuda dice por qué (%s)" % g[0])

        t, g = correr(m, juego + " AOEN_PRUEBA=reunion", 260, r"\[AOEN\] orden: Entrenar Aldeano", "H389-22-reunion")
        verificar(re.search(r"punto de reuni\S+n en", t) is not None, "el clic derecho con el centro urbano elegido pone el punto de reunión")

        t, g = correr(m, juego + " AOEN_PRUEBA=cola", 260, r"\[AOEN\] prueba cola: la unidad tiene (\d+) puntos de ruta pendientes", "H389-23-cola")
        verificar(len(re.findall(r"orden en cola: 4 unidades", t)) == 3, "tres clics derechos con Mayús encadenan tres órdenes")
        verificar(int(g[0]) >= 3, "la unidad guarda la ruta (%s puntos pendientes)" % g[0])

        t, g = correr(m, juego + " AOEN_PRUEBA=patrulla", 260, r"\[AOEN\] prueba patrulla: patrulla=(\d)[\r\n]", "H389-24-patrulla")
        verificar("orden: Patrullar" in t and "patrulla: 3 unidades" in t and g[0] == "1", "la tecla P y un clic ponen a patrullar a los 3 soldados")

        t, g = correr(m, juego + " AOEN_PRUEBA=alerta", 400, r"\[AOEN\] salta al aviso en", "H389-25-alerta")
        verificar(re.search(r"aviso: \S*Te atacan!", t) is not None, "un ataque da el aviso «¡Te atacan!»")
        verificar("salta al aviso en" in t, "Espacio lleva la vista al último aviso")

        t, g = correr(m, juego + " AOEN_PRUEBA=diplomacia", 260, r"\[AOEN\] prueba diplomacia: relaci\S+n con el jugador 2 = (\S+), madera de 2 = (\d+)[\r\n]", "H389-26-diplomacia")
        verificar(g[0] == "Neutral", "desde el panel de diplomacia se propone la paz y la máquina acepta")
        verificar("tributo: 100 de recurso 0 al jugador 2" in t, "el botón de tributo envía 100 de madera (llegan 80)")

        t, g = correr(m, "AOEN_SCENE=final AOEN_FFWD=300 AOEN_PEST=4", 120, r"\[AOEN\] pantalla final: VICTORIA, (\d) jugadores, (\d+) muestras en la gr\S+fica; Gana: ([^\r\n]+)[\r\n]", "H389-27-final")
        verificar(int(g[1]) >= 15, "la pantalla final dibuja la gráfica de la puntuación (%s muestras)" % g[1])
        verificar(g[2].startswith("T"), "con la victoria del jugador, el título es VICTORIA y gana él (%s)" % g[2])
        # el título sigue a quién ganó: derrota del jugador y modo espectador (la máquina juega también por el jugador 1)
        t, g = correr(m, "AOEN_SCENE=final AOEN_FINAL=derrota AOEN_FFWD=300", 60, r"\[AOEN\] pantalla final: (\S+), \d jugadores, \d+ muestras en la gr\S+fica; Gana: ([^\r\n]+)[\r\n]")
        verificar(g[0] == "DERROTA" and g[1].startswith("Jugador "), "si pierde el jugador, el título es DERROTA y dice qué rival gana (%s)" % g[1])
        t, g = correr(m, "AOEN_SCENE=final AOEN_FINAL=espectador AOEN_FFWD=300", 60, r"\[AOEN\] pantalla final: (.+?), \d jugadores, \d+ muestras en la gr\S+fica; Gana: ([^\r\n]+)[\r\n]")
        verificar(g[0] == "PARTIDA TERMINADA" and g[1].startswith("Jugador "), "en el modo espectador no hay victoria ni derrota propias: «Partida terminada» y quién gana (%s)" % g[1])

        # v8.5.4.10r2: el modo intercambio del mercante. Con la casilla «Comercio mejorado» marcada en la configuración, I, el
        # clic en «Dar» y M cambian el modo y los recursos; sin ella, el botón está gris y dice cómo activarlo
        t, g = correr(m, "AOEN_SCENE=setup AOEN_SEED=7 AOEN_PRUEBA=comercio", 300, r"\[AOEN\] prueba comercio: mejorado=(\d) modo=(\d) dar=(\d) recibir=(\d)", "H389-31-comercio")
        verificar("prueba comercio: casilla=1" in t, "el clic marca la casilla «Comercio mejorado» de la configuración")
        verificar(g == ("1", "1", "3", "0"), "con la opción, I pone el modo intercambio, el clic en Dar pasa a piedra y M a madera (%s)" % (g,))
        verificar(re.search(r"ayuda: Modo intercambio \| coste 0/0/0/0 \| disponible", t) is not None, "el botón «Modo intercambio» está disponible")
        t, g = correr(m, juego + " AOEN_PRUEBA=comercio", 260, r"\[AOEN\] prueba comercio: mejorado=(\d) modo=(\d) dar=(\d) recibir=(\d)", "H389-32-comercio-sin-opcion")
        verificar(g[0] == "0" and g[1] == "0", "sin la opción, el mercante sigue en modo oro (%s)" % (g,))
        verificar("ayuda: Modo intercambio | coste 0/0/0/0 | Activa Comercio mejorado al crear la partida" in t, "sin la opción, el botón gris explica cómo activarlo")

        # v8.5.5.2r2 (D447): casilla «Fuego amigo» de la configuración: apagada de serie y, marcada, llega a la partida
        t, g = correr(m, "AOEN_SCENE=setup AOEN_SEED=7 AOEN_PRUEBA=fuego", 300, r"\[AOEN\] prueba fuego: partida fuego_amigo=(\d) motor=(\d)", "H389-39-fuego-amigo")
        verificar("prueba fuego: casilla=1" in t, "el clic marca la casilla «Fuego amigo» de la configuración")
        verificar(g == ("1", "1"), "con la casilla marcada la partida y el motor llevan el fuego amigo (%s)" % (g,))

        # v8.5.4.10: sin los fondos originales (DRS sintéticos) el menú principal usa el paisaje propio, no el negro liso
        t, g = correr(m, "AOEN_FULL=1", 120, r"\[AOEN\] pantalla (\d+x\d+)", "H389-30-menu-fondo")
        pix = png_rgb(os.path.join(RAIZ, "docs", "capturas", "H389-30-menu-fondo.png"))   # sin ImageMagick
        n = len(pix) // 3
        claros = sum(1 for i in range(0, 3 * n, 3 * 4) if pix[i] + pix[i + 1] + pix[i + 2] > 45)
        muestras = len(range(0, 3 * n, 12))
        colores = len({(pix[i] >> 4, pix[i + 1] >> 4, pix[i + 2] >> 4) for i in range(0, 3 * n, 12)})
        verificar(claros * 100 // muestras >= 80, "el menú sin arte original no es negro: %d%% de los píxeles muestreados tienen color" % (claros * 100 // muestras))
        verificar(colores >= 40, "el paisaje propio tiene degradados y capas (%d colores distintos)" % colores)

    for res, cap in (("800x600", "H389-28-800x600"), (os.environ.get("H389_GRANDE", "1920x1080"), "H389-29-1080p")):
        img = imagem_de_teste("h389i-" + res, ["--vesa", "--resolucao", res, "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
        with Maquina("h389-interfaz-" + res, "ide", imagem=img, memoria="512M" if res.startswith("19") else "256M") as m:
            esperar(m, r"sagui:/\$", 150)
            time.sleep(4)
            h136.focar_terminal(m)
            t, g = correr(m, "AOEN_FULL=1 AOEN_SOLAPES=1 AOEN_REVEAL=1 AOEN_SCENE=game AOEN_SEED=7 AOEN_PRUEBA=ayuda", 300,
                          r"\[AOEN\] ayuda: Avanzar a la Edad", cap)
            sol = re.findall(r"\[AOEN\] solapes: (\d+) de (\d+) bloques, (\d+) fuera \(pantalla l\S+gica (\d+)x(\d+)\)", t)
            verificar(bool(sol) and all(s[0] == "0" and s[2] == "0" for s in sol[1:] or sol),
                      "a %s (interfaz lógica %sx%s) ningún bloque se pisa ni se sale (%s)" % (res, sol[-1][3] if sol else "?", sol[-1][4] if sol else "?", sol[-1] if sol else "sin registro"))


def presentacion():
    """v8.5.5.3, en SaguiOS con DRS sintéticos: lo nuevo de la presentación. Rótulo del menú principal, opciones en cuatro
    pestañas, cursor del juego según lo que hay debajo (atacar, extraer oro, no cabe) y rótulo de lo que hay bajo el ratón,
    la hoja con todos los cursores y la resolución de dibujo al 75 % en pantalla completa. Capturas H389-33 a H389-38."""
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "aoen")):
        print("AVISO: la imagen no lleva aoen (make): se salta la parte «presentacion»")
        return
    jogos = tempfile.mkdtemp(prefix="aoen-jogos-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")], check=True, stdout=subprocess.DEVNULL)

    def correr(m, extra, frames, espera, captura=None):
        marca = len(m.serial())
        m.digitar("AOEN_SHOT=/tmp/aoen.bmp AOEN_FRAMES=%d %s /apps/bin/aoen" % (frames, extra), 0.03)
        m.tecla("ret")
        g = esperar(m, espera, 300, desde=marca)
        if not isinstance(g, tuple):
            g = (g,)
        if captura:
            time.sleep(1.0)
            m.captura(captura)
        if "AOEN_SCENE=cursores" not in extra:
            esperar(m, r"\[AOEN\] captura guardada en el cuadro", 400, desde=marca)
        esperar(m, r"sagui:/\$", 60, desde=marca)
        return m.serial()[marca:], g

    img = imagem_de_teste("h389-presentacion", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h389-presentacion", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        h136.focar_terminal(m)
        t, g = correr(m, "AOEN_FULL=1", 120, r"\[AOEN\] pantalla (\d+x\d+)", "H389-33-menu-rotulo")
        pix = png_rgb(os.path.join(RAIZ, "docs", "capturas", "H389-33-menu-rotulo.png"))
        w = 1024   # la imagen de prueba es de 1024x768
        # el rótulo «AoEN» es de bronce: hay píxeles muy cálidos (rojo alto, azul bajo) en la franja de arriba
        franja = range(3 * w * 40, 3 * w * 250, 3)
        bronce = sum(1 for i in franja if pix[i] > 190 and pix[i + 2] < 110 and pix[i] - pix[i + 2] > 110)
        verificar(bronce > 2000, "el menú principal lleva el rótulo AoEN en bronce (%d píxeles)" % bronce)

        t, g = correr(m, "AOEN_SCENE=options AOEN_PAG=1", 60, r"\[AOEN\] opciones: p\S+gina (\S+)", "H389-34-opciones-interfaz")
        verificar(g[0] == "Interfaz", "las opciones abren en la pestaña Interfaz (%s)" % g[0])
        t, g = correr(m, "AOEN_SCENE=options AOEN_PAG=3", 60, r"\[AOEN\] opciones: p\S+gina (\S+)", "H389-35-opciones-teclas")
        verificar(g[0] == "Teclas", "la pestaña Teclas enseña la tabla de teclas (%s)" % g[0])

        t, g = correr(m, "AOEN_SCENE=game AOEN_SEED=7 AOEN_EFECTOS=1 AOEN_PRUEBA=cursor", 160,
                      r"\[AOEN\] prueba cursor: ataque=(\d+) oro=(\d+) obra=(\d+) \(atacar=(\d+) oro=(\d+) prohibido=(\d+)\)", "H389-36-cursor-mina")
        verificar(g[0] == g[3], "con soldados elegidos, sobre un enemigo el cursor es la espada (%s)" % g[0])
        verificar(g[1] == g[4], "con un aldeano elegido, sobre una mina de oro el cursor es el pico (%s)" % g[1])
        verificar(g[2] == g[5], "una casa sobre el centro urbano no cabe: cursor de prohibido (%s)" % g[2])
        t, g = correr(m, "AOEN_SCENE=cursores", 1, r"\[AOEN\] escena cursores: (\d+) cursores", "H389-37-cursores")
        verificar(int(g[0]) >= 10, "la hoja de cursores dibuja los %s cursores del juego" % g[0])

    # resolución de dibujo: en pantalla completa a 1024x768, al 75 % el búfer es de 768x576 y la SDL del sistema amplía
    img = imagem_de_teste("h389-dibujo", ["--vesa", "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
    with Maquina("h389-dibujo", "ide", imagem=img, memoria="256M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        h136.focar_terminal(m)
        marca = len(m.serial())
        m.digitar("AOEN_SHOT=/tmp/aoen.bmp AOEN_FRAMES=600 AOEN_FULL=1 AOEN_DIBUJO=75 AOEN_SCENE=game AOEN_SEED=7 /apps/bin/aoen", 0.03)
        m.tecla("ret")
        g = esperar(m, r"\[AOEN\] pantalla completa: escritorio (\d+)x(\d+), dibujo al (\d+)% \((\d+)x(\d+)\)", 300, desde=marca)
        verificar(g[2] == "75" and int(g[3]) * 4 == int(g[0]) * 3, "al 75 %% se dibuja a %sx%s en un escritorio de %sx%s" % (g[3], g[4], g[0], g[1]))
        esperar(m, r"\[AOEN\] pantalla %sx%s" % (g[3], g[4]), 120, desde=marca)
        time.sleep(1.2)
        m.captura("H389-38-dibujo-75")
        esperar(m, r"\[AOEN\] captura guardada en el cuadro", 400, desde=marca)


def lectores():
    """En el PC con ASan/UBSan (tests/aoen_lectores.c): DRS, SLP y partidas manipulados dan error limpio, sin escribir ni leer fuera."""
    from qemu import verificar, SAIDA
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "aoen_lectores")
    d0 = os.path.join(RAIZ, "userland", "juegos", "aoen")
    fuentes = [os.path.join(d0, f) for f in sorted(os.listdir(d0)) if f.endswith(".c") and f not in ("aoen_config.c", "puente.c", "save.c", "main.c", "rendimiento.c") and not f.startswith("ui_")]   # la interfaz (ui_*.c) necesita main.c
    sdl = subprocess.run(["sdl2-config", "--cflags"], capture_output=True, text=True).stdout.split()
    libs = subprocess.run(["sdl2-config", "--libs"], capture_output=True, text=True).stdout.split()
    subprocess.run(["gcc", "-std=gnu99", "-g", "-O1", "-fsanitize=address,undefined", "-fno-sanitize-recover=undefined", "-w", "-I", d0,
                    "-I", os.path.join(RAIZ, "userland", "terceros", "stbimage")] + sdl + ["-o", exe, os.path.join(RAIZ, "tests", "aoen_lectores.c")] + fuentes + libs + ["-lm"], check=True)
    d = tempfile.mkdtemp(prefix="aoen-malos-")
    env = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    import struct
    sys.path.insert(0, os.path.join(RAIZ, "tools"))
    import aoen_drs_sintetico as g
    ok=g.drs([(1,g.slp([g.unidad(1)]))])
    open(d+'/bien.drs','wb').write(ok)
    def hdr(n=1,off=76,cnt=1):
        return (b"x"*40+b"1.00"+b"tribe".ljust(12,b" ")+struct.pack("<II",n,off)).ljust(64,b"\0")
    # tabla con n enorme
    open(d+'/n_enorme.drs','wb').write(hdr()+struct.pack("<4sII",b"slp ",76,0xFFFFFFFF)+b"\0"*64)
    open(d+'/n_overflow_total.drs','wb').write(hdr(2)+struct.pack("<4sII",b"slp ",88,0x7FFFFFFF)+struct.pack("<4sII",b"slp ",88,0x7FFFFFFF)+b"\0"*64)
    open(d+'/lista_fuera.drs','wb').write(hdr(1,76)+struct.pack("<4sII",b"slp ",0xFFFFFF00,3)+b"\0"*12)
    open(d+'/truncado.drs','wb').write(ok[:50])
    # elemento con offset y tamaño fuera
    b=bytearray(ok); n=struct.unpack("<I",b[76+8:76+12])[0]; it=76+12
    b[it+4:it+8]=struct.pack("<I",0xFFFFFFF0); open(d+'/offset_fuera.drs','wb').write(bytes(b))
    b=bytearray(ok); b[it+8:it+12]=struct.pack("<I",0xFFFFFFFF); open(d+'/size_enorme.drs','wb').write(bytes(b))
    # SLP con cabecera de cuadro hostil: left negativo (-5) en la primera fila y cmd_off/out_off enormes
    slp=bytearray(g.slp([g.unidad(1)])); 
    fi=32; co,oo=struct.unpack("<II",slp[fi:fi+8])
    bad=bytearray(slp); 
    for y in range(3): bad[oo+y*4:oo+y*4+4]=struct.pack("<hh",-5,-7)
    open(d+'/left_negativo.drs','wb').write(g.drs([(1,bytes(bad))]))
    bad=bytearray(slp); bad[fi:fi+8]=struct.pack("<II",0xFFFFFFF0,0xFFFFFFF0)
    open(d+'/offsets_slp.drs','wb').write(g.drs([(1,bytes(bad))]))
    bad=bytearray(slp); bad[4:8]=struct.pack("<I",0x7FFFFFFF)
    open(d+'/nframes.drs','wb').write(g.drs([(1,bytes(bad))]))
    bad=slp[:len(slp)-3]  # comandos truncados
    open(d+'/cmd_truncado.drs','wb').write(g.drs([(1,bytes(bad))]))

    esperado = {"bien": "ok", "n_enorme": "rechazado", "n_overflow_total": "rechazado", "lista_fuera": "rechazado", "truncado": "rechazado",
                "offset_fuera": "ok", "size_enorme": "ok", "left_negativo": "ok", "offsets_slp": "ok", "nframes": "ok", "cmd_truncado": "ok"}
    for nombre, r in esperado.items():
        p = subprocess.run([exe, "drs", os.path.join(d, nombre + ".drs")], capture_output=True, text=True, env=env, cwd=d)
        verificar(p.returncode == 0 and p.stdout.strip() == r, "DRS %s: %s (sin fallos de memoria)" % (nombre, p.stdout.strip() or p.stderr[:200]))
    for k in (0, 1, 2, 3, 4, 5, 6, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19) + tuple(range(20, 36)) + tuple(range(38, 53)):
        p = subprocess.run([exe, "sav-mutar", os.path.join(d, "m%d.sav" % k), str(k)], capture_output=True, text=True, env=env, cwd=d)
        verificar(p.returncode == 0 and p.stdout.strip() == "rechazada actual-intacta", "partida manipulada %d rechazada y la actual se conserva" % k)
    for k in (36, 37, 53):   # partidas válidas (con un pasajero a bordo; sin cambios): se cargan
        p = subprocess.run([exe, "sav-mutar", os.path.join(d, "m%d.sav" % k), str(k)], capture_output=True, text=True, env=env, cwd=d)
        verificar(p.returncode == 0 and p.stdout.strip().startswith("cargada"), "partida válida %d se carga (%s)" % (k, p.stdout.strip() or p.stderr[:200]))
    for k in (7, 9):   # estos campos se sanean al leer: no debe fallar nada
        p = subprocess.run([exe, "sav-mutar", os.path.join(d, "m%d.sav" % k), str(k)], capture_output=True, text=True, env=env, cwd=d)
        verificar(p.returncode == 0 and "actual-intacta" in p.stdout, "partida con campo %d saneado: sin fallos de memoria" % k)


def compilar_arnes():
    from qemu import SAIDA
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "aoen_ia")
    d0 = os.path.join(RAIZ, "userland", "juegos", "aoen")
    fuentes = [os.path.join(d0, f) for f in sorted(os.listdir(d0)) if f.endswith(".c") and f not in ("aoen_config.c", "puente.c", "main.c", "rendimiento.c") and not f.startswith("ui_")]
    sdl = subprocess.run(["sdl2-config", "--cflags"], capture_output=True, text=True).stdout.split()
    libs = subprocess.run(["sdl2-config", "--libs"], capture_output=True, text=True).stdout.split()
    subprocess.run(["gcc", "-std=gnu99", "-O2", "-w", "-I", d0, "-I", os.path.join(RAIZ, "userland", "terceros", "stbimage")] + sdl +
                   ["-o", exe, os.path.join(RAIZ, "tests", "aoen_ia.c")] + fuentes + libs + ["-lm"], check=True)
    return exe


def reglas():
    """En el PC (tests/aoen_ia.c): victoria compartida, recursos del mapa y comercio (v8.5.4.7).
    - Victoria compartida: dos IA aliadas contra una tercera; con la opción ganan las dos sin romper la alianza, sin ella la
      alianza se rompe y gana una.
    - Recursos del mapa: los parches de expansión son los mismos para cada jugador y a distancias parecidas, y hay más oro y
      piedra de escasos a normales y a abundantes.
    - Comercio (escena AOEN_ESCENA=comercio, sin IA): el modo oro trae oro con el barco y con el carro; el intercambio quita 15
      al origen, no toca al destino y devuelve lo de la fórmula; sin 15 del recurso, el mercante espera."""
    from qemu import verificar
    exe = compilar_arnes()

    def correr(args, **env):
        return subprocess.run([exe] + [str(a) for a in args], capture_output=True, text=True, timeout=900,
                              env=dict(os.environ, **{k: str(v) for k, v in env.items()}))

    # victoria compartida
    for sem in (1, 2):
        r = correr([sem, 1, 1, 3, 100, 90, 3], AOEN_IA_FFA=1, AOEN_IA_ALIADOS=1, AOEN_IA_COMPARTIDA=1).stdout
        g = re.search(r"fin over=(\d) ganadores=(0x[0-9a-f]+|0) alianza01=(\S+)", r)
        verificar(g and int(g.group(2), 16) == 3 and g.group(3) == "Aliado", "victoria compartida (semilla %d): ganan los dos aliados juntos (%s)" % (sem, g.group(0) if g else r[-200:]))
        r = correr([sem, 1, 1, 3, 100, 90, 3], AOEN_IA_FFA=1, AOEN_IA_ALIADOS=1, AOEN_IA_COMPARTIDA=0).stdout
        g = re.search(r"fin over=(\d) ganadores=(0x[0-9a-f]+|0) alianza01=(\S+)", r)
        m = int(g.group(2), 16) if g else 0
        verificar(g and m in (1, 2, 4) and g.group(3) == "Enemigo", "sin victoria compartida (semilla %d): la alianza se rompe y gana uno solo (%s)" % (sem, g.group(0) if g else r[-200:]))

    # recursos del mapa: 1 escasos, 0 normales, 2 abundantes
    totales = []
    for nivel in (1, 0, 2):
        tot = 0
        for mapa in (0, 3):
            r = correr([7, 1, 1, mapa, 100, 60, 4], AOEN_IA_REPARTO=1, AOEN_IA_RECURSOS=nivel)
            parches = re.findall(r"parche de (oro|piedra) del jugador (\d) a ([\d.]+) casillas \((\d+)\)", r.stderr)
            por = {}
            for tipo, jug, d, n in parches:
                por.setdefault(jug, []).append((tipo, float(d), int(n)))
            cuentas = {j: (sorted(t for t, _, _ in v), sum(n for _, _, n in v)) for j, v in por.items()}
            medias = [sum(d for _, d, _ in v) / len(v) for v in por.values()]
            verificar(len(por) == 4 and len(set(map(str, cuentas.values()))) == 1,
                      "recursos %d, mapa %d: los 4 jugadores tienen los mismos parches de expansión (%s)" % (nivel, mapa, cuentas.get("1")))
            verificar(max(medias) - min(medias) <= 5.0, "recursos %d, mapa %d: distancias medias parecidas (%s)" % (nivel, mapa, ", ".join("%.1f" % x for x in medias)))
            g = re.search(r"mapa oro=(\d+) piedra=(\d+)", r.stdout)
            tot += int(g.group(1)) + int(g.group(2))
        totales.append(tot)
    print("    casillas de oro y piedra (escasos, normales, abundantes):", totales)
    verificar(totales[0] < totales[1] < totales[2], "más minas de escasos a normales y a abundantes (%s)" % totales)

    # comercio
    def escena(modo):
        r = correr([6, 1, 1, 3, 200, 4, 2], AOEN_IA_SIN_IA=1, AOEN_ESCENA="comercio", AOEN_COMERCIO=modo)
        rec = dict((int(j), list(map(int, v.split()))) for j, v in re.findall(r"recursos jugador (\d): ([\d ]+)", r.stdout))
        return r.stderr, rec
    log, rec = escena("oro")
    for quien in ("Barco", "Carro"):
        e = re.findall(quien + r" mercante entrega (\d+) de 2 \(modo 0, distancia ([\d.]+)", log)
        verificar(e and int(e[0][0]) == min(120, int(5 + 0.6 * float(e[0][1]))), "modo oro: el %s mercante trae %s de oro por viaje (%d viajes)" % (quien.lower(), e[0][0] if e else "?", len(e)))
    verificar(rec.get(2) == [500, 500, 500, 500], "modo oro: al otro jugador no se le toca nada (%s)" % rec.get(2))
    log, rec = escena("cambio")
    cargas = re.findall(r"(Barco|Carro) mercante carga 15 de 1", log)
    verificar(len(cargas) >= 2, "intercambio: los mercantes cargan 15 de comida en su puerto (%d cargas)" % len(cargas))
    ent = re.findall(r"(Barco|Carro) mercante entrega (\d+) de 3 \(modo 1, distancia ([\d.]+), total_pedido (\d+), total_dado (\d+)\)", log)
    verificar(len(ent) >= 2, "intercambio: los mercantes traen piedra (%d entregas)" % len(ent))
    import math
    for q, n, d, tp, td in ent:
        esperado = int(math.floor(15 * min(3.0, 1 + float(d) / 60) * min(4.0, max(0.25, int(tp) / max(1, int(td))))))
        verificar(abs(int(n) - esperado) <= 1, "intercambio (%s): recibe %s y la fórmula da %d" % (q.lower(), n, esperado))
    verificar(rec.get(2) == [500, 500, 500, 500], "intercambio: al jugador destino no se le quita ni se le da nada (%s)" % rec.get(2))
    # cambiar el trato a mitad de ruta la reinicia (sin carga gratis) y un puerto ajeno derribado deja libre al mercante
    r = correr([6, 1, 1, 3, 200, 4, 2], AOEN_IA_SIN_IA=1, AOEN_ESCENA="comercio", AOEN_COMERCIO="cambio", AOEN_IA_PRUEBA_CAMBIO=1).stdout
    cam = re.findall(r"cambio (\S+) mercante: antes carga (\d+) fase (\d) , ?después carga (\d+) fase (\d)".replace(" , ?", ", "), r)
    verificar(len(cam) == 2 and all(c[3] == "0" and c[4] == "0" for c in cam), "cambiar de modo a mitad de ruta la reinicia sin carga (%s)" % cam)
    rut = re.findall(r"ruta (\S+) mercante: destino (-?\d+) fase (\d) carga (\d+)", r)
    verificar(len(rut) == 2 and all(x[1] == "-1" and x[2] == "0" and x[3] == "0" for x in rut), "sin el puerto ajeno, la ruta se pierde y el mercante queda libre (%s)" % rut)
    log, rec = escena("espera")
    verificar("espera 15 de 3" in log and "carga 15" not in log, "sin 15 del recurso, el mercante espera en su puerto")
    verificar(rec.get(1, [0] * 4)[3] == 10, "y no se le quita nada al jugador (%s de piedra)" % rec.get(1, ["?"] * 4)[3])


def terreno():
    """v8.5.4.10r2, en el PC (tests/aoen_ia.c, AOEN_IA_AZAR): barcos y unidades de tierra con órdenes al azar cada medio
    segundo (moverse a cualquier punto, atacar o seguir a cualquier entidad, embarcar, desembarcar, cola, patrulla) en islas
    y en costa. Ningún barco pisa tierra y ninguna unidad de tierra el agua, y todos siguen moviéndose."""
    from qemu import verificar
    exe = compilar_arnes()
    for mapa, nombre in ((0, "islas"), (2, "costa")):
        total_emb = total_des = 0
        for sem in (1, 2, 3):
            r = subprocess.run([exe, str(sem), "1", "1", str(mapa), "100"], capture_output=True, text=True, timeout=600,
                               env=dict(os.environ, AOEN_IA_AZAR="150"))
            g = re.search(r"terreno malas=(\d+) comprobaciones=(\d+)", r.stdout)
            rec = re.search(r"azar recorrido barcos=(\d+) tierra=(\d+)", r.stdout)
            mal = re.findall(r"terreno: [^\n]+", r.stderr)
            verificar(g and int(g.group(1)) == 0 and int(g.group(2)) > 4000,
                      "%s, semilla %d: %s comprobaciones y ninguna unidad en el terreno que no es suyo (%s)" % (nombre, sem, g.group(2) if g else "?", mal[0] if mal else "bien"))
            verificar(rec and int(rec.group(1)) > 500 and int(rec.group(2)) > 1000,
                      "%s, semilla %d: los barcos navegan (%s casillas) y los de tierra andan (%s)" % (nombre, sem, rec.group(1) if rec else "?", rec.group(2) if rec else "?"))
            emb = len(re.findall(r"transporte: embarca", r.stderr)); des = len(re.findall(r"transporte: desembarcan", r.stderr))
            print("    %s, semilla %d: %d embarques, %d desembarcos" % (nombre, sem, emb, des))
            total_emb += emb; total_des += des
        # según la semilla el transporte puede quedar en un lago sin nadie cerca: se mira el total
        verificar(total_emb > 0 and total_des > 0, "%s: las unidades suben a los transportes y bajan a tierra (%d embarques, %d desembarcos)" % (nombre, total_emb, total_des))


def edificios():
    """v8.5.4.10r2, en el PC: qué entrena, qué investiga y qué recoge cada edificio, contra la tabla de docs/aoen.md (las
    reglas del Age of Empires original, con las diferencias anotadas allí)."""
    from qemu import verificar
    exe = compilar_arnes()
    r = subprocess.run([exe, "1", "1", "1"], capture_output=True, text=True, timeout=60, env=dict(os.environ, AOEN_IA_EDIFICIOS="1")).stdout
    tabla = {}
    for nom, ent, inv, rec in re.findall(r"edificio ([^|]+)\|entrena:([^|]*)\|investiga:([^|]*)\|recoge:([^\n]*)", r):
        tabla[nom] = tuple(set(x.strip() for x in c.split(";") if x.strip()) for c in (ent, inv, rec))
    esperado = {
        "Centro urbano": ({"Aldeano"}, set(), {"madera", "comida", "oro", "piedra", "carne"}),
        "Cuartel": ({"Clubman", "Hachero", "Espadachín", "Lanzador de honda"}, set(), set()),
        "Granero": (set(), {"Domesticación", "Arado", "Torre centinela", "Torre de guardia", "Torres de ballesta", "Muralla media", "Fortificación"}, {"comida", "carne"}),
        "Almacén": (set(), {"Herramientas", "Hachas de bronce", "Minería", "Armadura de cuero", "Armadura de escamas", "Armas de bronce", "Armas de hierro", "Armadura de caballería"}, {"madera", "oro", "piedra", "carne"}),
        "Campo de tiro": ({"Arquero", "Arquero mejorado", "Carro arquero", "Arquero a caballo", "Arquero de elefante"}, {"Flechas mejoradas", "Arcos compuestos"}, set()),
        "Establo": ({"Explorador", "Caballería", "Caballería pesada", "Elefante de guerra", "Catafracta"}, set(), set()),
        "Templo": ({"Sacerdote"}, {"Medicina", "Monoteísmo", "Fe", "Mística"}, set()),
        "Academia": ({"Hoplita", "Falange", "Centurión"}, set(), set()),
        "Centro de gobierno": (set(), {"Ingeniería", "Balística", "Arquitectura", "Logística"}, set()),
        "Mercado": ({"Carro mercante"}, {"Rueda", "Astronomía"}, set()),
        "Taller de asedio": ({"Balista", "Catapulta", "Catapulta pesada"}, set(), set()),
        "Muelle": ({"Barco de pesca", "Barco de transporte", "Trirreme", "Barco mercante"}, set(), {"pescado"}),
    }
    for nom, (ent, inv, rec) in esperado.items():
        t = tabla.get(nom)
        verificar(t is not None and t[0] == ent, "%s entrena %s" % (nom, ", ".join(sorted(t[0])) if t else "?"))
        verificar(t is not None and t[1] == inv, "%s investiga %s" % (nom, ", ".join(sorted(t[1])) if t and t[1] else "nada"))
        verificar(t is not None and t[2] == rec, "%s recoge %s" % (nom, ", ".join(sorted(t[2])) if t and t[2] else "nada"))
    for nom in ("Casa", "Granja", "Torre", "Muralla", "Maravilla"):
        t = tabla.get(nom)
        verificar(t is not None and not any(t), "%s no entrena, ni investiga, ni recoge" % nom)


def ia():
    """En el PC (tests/aoen_ia.c, sin ventana y a toda velocidad): partidas de IA contra IA en mapas con camino por tierra.
    Todas terminan antes de 60 minutos de juego; la difícil gana a la fácil en todas, la normal a la fácil casi siempre y la
    «muy difícil» (visión +20 %, recogida extra) a la difícil la mayoría; en islas la difícil acaba con la fácil antes de 60 min,
    y con la victoria por Maravilla (partida rica) todas terminan. H389_IA_PARTIDAS cambia cuántas de cada (6)."""
    from qemu import verificar, SAIDA
    os.makedirs(SAIDA, exist_ok=True)
    exe = os.path.join(SAIDA, "aoen_ia")
    d0 = os.path.join(RAIZ, "userland", "juegos", "aoen")
    fuentes = [os.path.join(d0, f) for f in sorted(os.listdir(d0)) if f.endswith(".c") and f not in ("aoen_config.c", "puente.c", "main.c", "rendimiento.c") and not f.startswith("ui_")]
    sdl = subprocess.run(["sdl2-config", "--cflags"], capture_output=True, text=True).stdout.split()
    libs = subprocess.run(["sdl2-config", "--libs"], capture_output=True, text=True).stdout.split()
    subprocess.run(["gcc", "-std=gnu99", "-O2", "-w", "-I", d0, "-I", os.path.join(RAIZ, "userland", "terceros", "stbimage")] + sdl +
                   ["-o", exe, os.path.join(RAIZ, "tests", "aoen_ia.c")] + fuentes + libs + ["-lm"], check=True)
    n = int(os.environ.get("H389_IA_PARTIDAS", "6"))
    mapas = (1, 2, 3, 4)   # continentes, costero, interior, tierras altas (las islas dependen de los transportes y pueden alargarse)

    def serie(da, db, islas=False, entorno=None):
        r = []
        for k in range(n):
            out = subprocess.run([exe, str(11 + k), str(da), str(db), "0" if islas else str(mapas[k % len(mapas)]), "100", "60"],
                                 capture_output=True, text=True, timeout=900, env=dict(os.environ, **(entorno or {}))).stdout
            g = re.search(r"resultado ganador=(\d) tiempo=(\d+)", out)
            tm = re.search(r"terreno malas=(\d+)", out)
            verificar(tm is not None and tm.group(1) == "0", "partida %d: ningún barco en tierra ni unidad de tierra en el agua (%s)" % (11 + k, tm.group(1) if tm else "?"))
            edades = re.findall(r"dif (\d) civ \S+ edades (\d+)/(\d+)/(\d+)", out)
            r.append((int(g.group(1)), int(g.group(2)), edades))
        gana_a = sum(1 for x in r if x[0] == 1)
        gana_b = sum(1 for x in r if x[0] == 2)
        print("    %sdificultad %d contra %d: %d-%d (%d sin terminar), duración media %d s, edades %s" %
              ("islas, " if islas else "", da, db, gana_a, gana_b, n - gana_a - gana_b, sum(x[1] for x in r) // n, [e for x in r[:2] for e in x[2]]))
        return gana_a, gana_b
    a, b = serie(2, 0)
    verificar(a == n, "la difícil gana a la fácil en las %d partidas (%d)" % (n, a))
    a, b = serie(1, 0)
    verificar(a >= n - 1, "la normal gana a la fácil casi siempre (%d de %d)" % (a, n))
    a, b = serie(3, 2)
    verificar(a > b, "la muy difícil gana a la difícil la mayoría (%d-%d)" % (a, b))
    a, b = serie(1, 1)
    verificar(a + b >= n - 1, "las partidas igualadas también terminan (%d de %d)" % (a + b, n))
    # islas: transportes, colonias y flota; la difícil termina con la fácil antes de una hora
    a, b = serie(2, 0, islas=True)
    verificar(a >= n - 1, "en islas la difícil gana a la fácil antes de 60 min (%d de %d)" % (a, n))
    # victoria por Maravilla (partida rica, Edad del Hierro): alguien la construye y gana con ella o por conquista
    a, b = serie(1, 1, entorno={"AOEN_IA_RICO": "1", "AOEN_IA_VICTORIA": "2"})
    verificar(a + b == n, "con la victoria por Maravilla todas las partidas terminan (%d de %d)" % (a + b, n))


def efectos():
    """En SaguiOS (TCG o KVM) con los DRS sintéticos: el perfil por defecto con SSE2 es «Normal»; la escena de efectos
    (obra con andamios, edificio dañado, derrumbe) arranca con el perfil completo; y a 1920x1080 la interfaz sale x2
    con el zoom inicial de pantalla grande. Capturas H389-7 a H389-9."""
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    jogos = tempfile.mkdtemp(prefix="aoen-jogos-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")], check=True, stdout=subprocess.DEVNULL)
    for res, cmd, cap in (("1024x768", "AOEN_SCENE=game AOEN_FULL=1 /apps/bin/aoen", "H389-7-efectos-normal"),
                          ("1024x768", "AOEN_SCENE=fx AOEN_EFECTOS=2 AOEN_FULL=1 /apps/bin/aoen", "H389-8-obras-y-ruinas"),
                          (os.environ.get("H389_GRANDE", "1920x1080"), "AOEN_SCENE=game AOEN_EFECTOS=2 AOEN_FULL=1 /apps/bin/aoen", "H389-9-1080p")):
        img = imagem_de_teste("h389e-" + res, ["--vesa", "--resolucao", res, "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
        with Maquina("h389-efectos-" + res, "ide", imagem=img, memoria="512M" if res.startswith("19") else "256M") as m:
            esperar(m, r"sagui:/\$", 150)
            time.sleep(4)
            h136.focar_terminal(m)
            marca = len(m.serial())
            m.digitar(cmd, 0.04)
            m.tecla("ret")
            g = esperar(m, r"\[AOEN\] efectos: perfil (\S+) \(transiciones=(\d) agua=(\d) niebla=(\d) sombras=(\d) particulas=(\d) construccion=(\d) diaynoche=(\d) suave=(\d)\) sse2=(\d)", 120, desde=marca)
            if "EFECTOS" not in cmd:
                verificar(g[0] == "Normal" and g[1] == "1" and g[3] == "1" and g[6] == "1" and g[7] == "0",
                          "con SSE2 el perfil por defecto es Normal: transiciones, niebla y obras sí; día y noche no (%s)" % (g,))
            else:
                verificar(g[0] == "Completo" and g[7] == "1" and g[8] == "1", "AOEN_EFECTOS=2 activa el perfil Completo (día y noche, zoom suave)")
            esperar(m, r"\[AOEN\] partida iniciada", 120, desde=marca)
            p = esperar(m, r"\[AOEN\] pantalla (%s)x(%s), interfaz x(\d)" % tuple(res.split("x")), 120, desde=marca)
            verificar(True, "a pantalla completa el búfer es de %sx%s (la resolución del escritorio) con la interfaz x%s" % p)
            if res.startswith("19"):
                verificar(p[2] == "2", "a 1920x1080 la interfaz se escala x2 (píxeles nítidos)")
            esperar(m, r"\[AOEN\] estado t=", 120, desde=marca)
            time.sleep(4)
            m.captura(cap)


def bench():
    """Mide los fps de la escaramuza (DRS sintéticos) a 800x600 y 1024x768 a pantalla completa, con y sin efectos
    (AOEN_EFECTOS=0/1: la partida gráfica completa o el perfil de equipos lentos). H389_RES="800x600,1024x768"
    y H389_EFECTOS="0,1" lo cambian; H389_BENCH son los cuadros de cada medida."""
    from qemu import Maquina, imagem_de_teste, verificar, APPS_RAIZ
    from h60_barra_tareas import esperar
    import h136_opentyrian as h136
    jogos = tempfile.mkdtemp(prefix="aoen-jogos-")
    subprocess.run([sys.executable, os.path.join(RAIZ, "tools", "aoen_drs_sintetico.py"), os.path.join(jogos, "AOE", "DATA")], check=True, stdout=subprocess.DEVNULL)
    resultados = []
    for res in os.environ.get("H389_RES", "800x600,1024x768").split(","):
        img = imagem_de_teste("h389-" + res, ["--vesa", "--resolucao", res, "--apps", APPS_RAIZ, "--jogos", jogos, "--jogos-mb", "64"])
        with Maquina("h389-bench-" + res, "ide", imagem=img, memoria="256M") as m:
            esperar(m, r"sagui:/\$", 150)
            time.sleep(4)
            h136.focar_terminal(m)
            for ef in os.environ.get("H389_EFECTOS", "0,1").split(","):
                marca = len(m.serial())
                m.digitar("AOEN_FULL=1 AOEN_SCENE=game AOEN_EFECTOS=%s AOEN_RENDER=%s AOEN_DIBUJO=%s AOEN_BENCH=%s /apps/bin/aoen" % (ef, os.environ.get("H389_RENDER", "0"), os.environ.get("H389_DIBUJO", "100"), os.environ.get("H389_BENCH", "600")), 0.04)
                m.tecla("ret")
                g = esperar(m, r"\[AOEN\] bench (\d+)x(\d+) cuadros=(\d+) ms=(\d+) fps=([\d.]+)", 300, desde=marca)
                print("    %s efectos=%s -> bench %sx%s: %s cuadros en %s ms = %s fps" % (res, ef, g[0], g[1], g[2], g[3], g[4]))
                resultados.append((res, ef, float(g[4])))
                time.sleep(2)
                if ef == os.environ.get("H389_EFECTOS", "0,1").split(",")[-1]:
                    m.captura("H389-bench-" + res)
    verificar(all(r[2] > 0 for r in resultados), "todas las medidas dan fps > 0")


if __name__ == "__main__":
    partes = sys.argv[1:] or ["generador", "licencia", "lectores", "sin-datos", "con-datos", "efectos", "funciones", "ia", "interfaz", "presentacion", "reglas", "terreno", "edificios"]  # "bench" se pide aparte
    for p in partes:
        print("== " + p)
        {"generador": generador, "licencia": licencia, "sin-datos": sin_datos, "con-datos": con_datos, "efectos": efectos, "lectores": lectores, "bench": bench, "funciones": funciones, "ia": ia, "interfaz": interfaz, "presentacion": presentacion, "reglas": reglas, "terreno": terreno, "edificios": edificios}[p]()
    print("H389 ok")
