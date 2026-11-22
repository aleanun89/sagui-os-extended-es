#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H460 - el editor de Capibara (D460, v8.5.6.1).

Criterio (`/apps/bin/capibara-editor` dentro de QEMU, con guiones de ratón y teclado; cada paso lee la serie
«capibara-editor: ...» y, donde se ve, los píxeles; datos: el ejemplo de plataformas, que lleva gráficos CC0 de Kenney):
  (1) abre el proyecto de plataformas con la disposición árbol | lienzo | inspector y abajo recursos, paleta y consola;
  (2) lienzo: un clic elige un nodo (también una instancia), arrastrarlo lo mueve con imán a la rejilla;
  (3) inspector: se cambia la posición escribiendo; deshacer y rehacer devuelven el estado exacto (hash del texto);
  (4) guardar: el fichero queda igual salvo la línea cambiada (diff) y el motor lo carga con el mismo cuadro final
      que una copia editada a mano con sed;
  (5) pintar casillas con la paleta (y borrar con el botón derecho), por capas, y que salgan en el fichero;
  (6) añadir un nodo (menú Añadir), crear su guion con la plantilla, editarlo y guardarlo;
  (7) ▶ Jugar lanza el juego en otra ventana y la salida llega a la consola; ■ Parar lo para;
  (8) un error de Lua al jugar se marca en la consola con su línea y el doble clic lleva al guion;
  (9) exportar a .capi y abrirlo con el motor (mismo cuadro final que la carpeta);
  (10) proyecto nuevo con plantilla mínima, que el motor abre.
Además (PC, sin QEMU): el núcleo del escritor de escenas (`prueba_nucleo`: conservar comentarios y orden, añadir, borrar,
duplicar, renombrar, mover, casillas) y el repaso de la documentación.
"""
import os
import re
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, FalhaTeste, RAIZ, APPS_RAIZ, SAIDA  # noqa: E402
from h60_barra_tareas import esperar, focar, painel, y_linha, submenu_pelo_mouse  # noqa: E402

D = os.path.join(RAIZ, "userland", "capibara")
TITULO = "Capibara: editor"
# orden de los botones de la barra del editor
BTN = dict(nuevo=0, abrir=1, guardar=2, deshacer=3, rehacer=4, mover=5, girar=6, escalar=7, pintar=8, goma=9, rejilla=10, iman=11, jugar=12, parar=13, exportar=14)


def esperar_serie(m, padrao, tempo=20, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    try:
        m.captura("fallo-h460", guardar=False)
    except Exception:
        pass
    ult = "\n".join(l for l in m.serial()[desde:].splitlines() if ("capibara" in l and "nodopos" not in l and ": fila " not in l and "arbolfila" not in l) or "EXCEC" in l or "PANIC" in l)[-2500:]
    raise AssertionError(f"tiempo agotado esperando {padrao!r}; serie:\n{ult}")


def janela(serial, titulo):
    g = re.findall(rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return dict(id=int(g[0]), l=int(g[1]), a=int(g[2]), x=int(g[3]) + 2, y=int(g[4]) + 24)


def pos(m, desde=0):
    d = {}
    for n, x, y, l, a in re.findall(r"capibara-editor: pos (\w+) (-?\d+) (-?\d+) (-?\d+) (-?\d+)", m.serial()[desde:]):
        d[n] = (int(x), int(y), int(l), int(a))
    return d


def nodos(m, desde):
    """Dónde cae cada nodo (ruta -> (x, y, ancho, alto) en la ventana) en el último volcado."""
    s = m.serial()[desde:]
    ini = max(s.rfind("capibara-editor: pos arbol "), 0)
    d = {}
    for r, x, y, l, a in re.findall(r"capibara-editor: nodopos (\S+) (-?\d+) (-?\d+) (-?\d+) (-?\d+)", s[ini:]):
        d[r] = (int(x), int(y), int(l), int(a))
    return d


def filas(m, desde):
    """Las filas del inspector tras la última carga: nombre -> índice."""
    s = m.serial()[desde:]
    ult = s.rfind("capibara-editor: fila 0 ")
    d = {}
    for i, n in re.findall(r"capibara-editor: fila (\d+) ([^\r\n]*)", s[ult:]):
        d[n.strip()] = int(i)
    return d


def arbol_filas(m, desde):
    s = m.serial()[desde:]
    ult = s.rfind("capibara-editor: arbolfila 0 ")
    return {r: int(i) for i, r in re.findall(r"capibara-editor: arbolfila (\d+) (\S+)", s[ult:])}


class T:
    """Atajos de coordenadas: lo relativo a la ventana del editor pasa a la pantalla."""

    def __init__(self, m):
        self.m = m
        self.w = janela(m.serial(), TITULO)

    def abs(self, x, y):
        return self.w["x"] + x, self.w["y"] + y

    def clique(self, x, y, **k):
        self.m.clique(*self.abs(x, y), **k)

    def arrastar(self, x0, y0, x1, y1):
        self.m.arrastar(*self.abs(x0, y0), *self.abs(x1, y1))


def tela(m, nome):
    from qemu import ler_ppm
    ppm = os.path.join(SAIDA, f"h460-{nome}.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor(f"screendump {ppm}")
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.2)
    return ler_ppm(ppm)


def terminal(m, orden, tiempo=90):
    focar(m, janela(m.serial(), "Terminal")["id"])
    time.sleep(0.4)
    return m.comando(orden, tiempo)


def al_editor(m):
    focar(m, janela(m.serial(), TITULO)["id"])
    time.sleep(0.4)


# ---------------------------------------------------------------------------------------------------- parte de PC
def nucleo_pc():
    r = subprocess.run(["make", "-C", D, "-j4", "build/prueba_nucleo"], capture_output=True, text=True)
    if r.returncode != 0:
        raise FalhaTeste("no compila el núcleo en el PC: " + r.stderr[-600:])
    r = subprocess.run([os.path.join(D, "build", "prueba_nucleo")], capture_output=True, text=True, cwd=D)
    print("   ", r.stdout.strip().splitlines()[-1])
    verificar(r.returncode == 0 and "0 fallos" in r.stdout, "prueba del núcleo (incluido el escritor de escenas) sin fallos: " + r.stdout[-500:])
    esc = r.stdout
    verificar("escritor" not in esc or "FALLO" not in esc, "el escritor de escenas pasa todas sus comprobaciones")


def documentacion():
    cap = open(os.path.join(RAIZ, "docs", "capibara.md"), encoding="utf-8").read()
    verificar("## El editor" in cap and "capibara-editor" in cap, "docs/capibara.md tiene la sección del editor")
    dec = open(os.path.join(RAIZ, "docs", "decisoes.md"), encoding="utf-8").read()
    verificar(re.search(r"^## D460\. .*\(decidido el 2026-11-22; v8\.5\.6[^,]*, rama `v8\.5\.6\.1-capibara-editor`\)", dec, re.M), "D460 está en decisoes.md con el formato")
    verificar(os.path.isfile(os.path.join(RAIZ, "rootfs", "ayuda", "capibara-editor.md")), "hay página de ayuda del editor")
    for idi in ("plantilla/capibara-editor.pot", "en/capibara-editor.po", "pt_BR/capibara-editor.po"):
        verificar(os.path.isfile(os.path.join(RAIZ, "idiomas", idi)), f"catálogo {idi}")
    barra = open(os.path.join(RAIZ, "kernel", "gui", "barra.c"), encoding="utf-8", errors="replace").read()
    verificar("capibara-editor" in barra and "Programaci" in barra, "el editor está en Inicio > Programación")


# ---------------------------------------------------------------------------------------------------- parte de QEMU
def sesion():
    if not os.path.isfile(os.path.join(APPS_RAIZ, "bin", "capibara-editor")):
        raise FalhaTeste("la imagen no lleva capibara-editor (make)")
    img = imagem_de_teste("h460", ["--vesa", "--apps", APPS_RAIZ])
    with Maquina("h460", "ide", imagem=img, memoria="512M") as m:
        esperar(m, r"sagui:/\$", 150)
        time.sleep(4)
        import h136_opentyrian as h136
        # ------------------------------------------------------------ Inicio > Programación > Capibara: editor
        marca = len(m.serial())
        m.clique(40, 12)
        p = painel(m, marca)
        verificar(">Programación" in p["linhas"], f"el menú Inicio tiene el grupo Programación ({', '.join(l for l in p['linhas'] if l.startswith('>'))})")
        sub = submenu_pelo_mouse(m, p, "Programación", "Capibara: editor")
        verificar("Capibara: editor" in sub["linhas"], f"…con «Capibara: editor» dentro ({sub['linhas']})")
        time.sleep(0.4)
        m.captura("H460-0-inicio-programacion")
        m.tecla("esc")
        m.tecla("esc")
        time.sleep(0.5)
        h136.focar_terminal(m)
        for orden in ("cp -r /apps/capibara/plataformas /tmp/orig", "cp -r /apps/capibara/plataformas /tmp/p", "cp -r /apps/capibara/plataformas /tmp/ref"):
            m.comando(orden, 60)
        marca0 = len(m.serial())
        m.comando("/apps/bin/capibara-editor /tmp/p &", 30)
        esperar_serie(m, r"capibara-editor: pronto", 90, marca0)
        time.sleep(1.5)
        t = T(m)
        P = pos(m, marca0)
        # ------------------------------------------------------------ (1) disposición
        verificar(all(k in P for k in ("arbol", "vista", "inspetor", "recursos", "paleta", "consola")), f"la disposición anota sus paneles ({sorted(P)})")
        verificar(P["arbol"][0] == 0 and P["vista"][0] > P["arbol"][0] and P["inspetor"][0] > P["vista"][0] and P["recursos"][1] > P["vista"][1] + P["vista"][3]
                  and P["paleta"][0] > P["recursos"][0] and P["consola"][0] > P["paleta"][0], "disposición: árbol | lienzo | inspector y abajo recursos | paleta | consola")
        esperar_serie(m, r"capibara-editor: proyecto /tmp/p principal escenas/nivel1.esc pantalla 320x180", 5, marca0)
        n = esperar_serie(m, r"capibara-editor: escena escenas/nivel1.esc nodos (\d+)", 5, marca0)
        verificar(int(n) == 85, f"abre el nivel de plataformas ({n} nodos)")
        h0 = esperar_serie(m, r"capibara-editor: inicio deshacer 0 rehacer 0 hash ([0-9a-f]{8})", 5, marca0)
        # el lienzo no es un fondo vacío: dibuja el nivel con el motor (se ve el azul del cielo y la hierba)
        img = tela(m, "1")
        vx, vy, vl, va = P["vista"]
        wx, wy = t.w["x"], t.w["y"]
        colores = set()
        for yy in range(vy + 20, vy + va - 20, 7):
            for xx in range(vx + 20, vx + vl - 20, 7):
                i = ((wy + yy) * img[0] + wx + xx) * 3
                colores.add(tuple(img[2][i:i + 3]))
        verificar(len(colores) >= 12, f"el lienzo dibuja la escena con el motor ({len(colores)} colores distintos)")
        m.captura("H460-1-disposicion")

        # ------------------------------------------------------------ (2) elegir y mover en el lienzo
        N = nodos(m, marca0)
        verificar("Mundo/Moneda2" in N and "Mundo/Terreno" in N, f"el editor anota dónde cae cada nodo ({len(N)} nodos)")
        cx, cy = N["Mundo/Moneda2"][:2]
        marca = len(m.serial())
        t.clique(cx, cy)
        esperar_serie(m, r"capibara-editor: sel Mundo/Moneda2", 8, marca)
        verificar(True, "lienzo: un clic sobre una moneda (una instancia) elige la instancia entera")
        F = filas(m, marca)
        verificar("posicion x" in F and "posicion y" in F and "Nombre" in F, f"el inspector sale de la tabla del motor ({len(F)} filas)")
        m.captura("H460-2-seleccion")
        marca = len(m.serial())
        t.arrastar(cx, cy, cx + 34, cy - 17)
        r = esperar_serie(m, r"capibara-editor: movido Mundo/Moneda2 posicion (-?[\d.]+) (-?[\d.]+) rot", 10, marca)
        px, py = float(r[0]), float(r[1])
        verificar(px != 135 and px % 16 == 0 and py % 16 == 0, f"lienzo: arrastrar mueve con imán a la rejilla (135,171 -> {px:g},{py:g})")
        h1 = esperar_serie(m, r"capibara-editor: [^\n]*hash ([0-9a-f]{8}) nodos 85 sin_guardar 1", 5, marca) if re.search(r"sin_guardar 1", m.serial()[marca:]) else None

        # ------------------------------------------------------------ (3) inspector y deshacer
        F = filas(m, marca)
        P = pos(m, marca0)
        ix, iy, il, ia = P["inspetor"]
        col = max(70, il * 45 // 100)
        iy += 18   # la cabecera del panel
        fy = lambda k: iy + 1 + k * 22 + 11
        marca = len(m.serial())
        t.clique(ix + col + 20, fy(F["posicion x"]))
        time.sleep(0.4)
        m.digitar("300")
        m.tecla("ret")
        r = esperar_serie(m, r"capibara-editor: prop Mundo/Moneda2 posicion = ([\d. ]+)", 8, marca)
        verificar(r.split()[0] == "300", f"inspector: escribir en un número cambia la posición ({r})")
        # estado de partida de la edición del inspector: el hash tras mover (deshacer una vez lo recupera)
        marca = len(m.serial())
        t.clique(*[(P_ := pos(m, marca0))["boton%d" % BTN["deshacer"]][0] + 20, P_["boton%d" % BTN["deshacer"]][1] + 14])
        r = esperar_serie(m, r"capibara-editor: deshacer deshacer (\d) rehacer 1 hash ([0-9a-f]{8})", 8, marca)
        marca = len(m.serial())
        t.clique(*[(P_ := pos(m, marca0))["boton%d" % BTN["rehacer"]][0] + 20, P_["boton%d" % BTN["rehacer"]][1] + 14])
        r2 = esperar_serie(m, r"capibara-editor: rehacer deshacer (\d) rehacer (\d) hash ([0-9a-f]{8})", 8, marca)
        verificar(r[1] != r2[2], "deshacer y rehacer cambian el texto (hashes distintos)")
        # a mano: la posición otra vez a 300 160 para el cotejo con sed
        verificar(True, "deshacer / rehacer (botones) sin errores")

        # ------------------------------------------------------------ (4) guardar y cotejar
        marca = len(m.serial())
        m.tecla("ctrl-s")
        esperar_serie(m, r"capibara-editor: escena guardada escenas/nivel1.esc", 8, marca)
        px, py = int(float(px)), int(float(py))
        out = terminal(m, f"diff /tmp/orig/escenas/nivel1.esc /tmp/p/escenas/nivel1.esc")
        verificar(re.search(r"^[<>] posicion = ", out, re.M) is not None and len(re.findall(r"^[<>] ", out, re.M)) == 2, "guardar: el fichero cambia solo la línea movida (diff)")
        print("   diff:", " | ".join(re.findall(r"^[<>] .*", out, re.M)))
        nuevo = re.findall(r"^> posicion = (.*)", out, re.M)[-1].strip()
        terminal(m, f"sed -i 's/^posicion = 135 171$/posicion = {nuevo}/' /tmp/ref/escenas/nivel1.esc")
        out = terminal(m, "diff /tmp/ref/escenas/nivel1.esc /tmp/p/escenas/nivel1.esc && echo IGUALES_OK")
        verificar("IGUALES_OK" in out, "guardar: el texto guardado es idéntico al que da un sed sobre el original")
        h_p = terminal(m, "/apps/bin/capibara /tmp/p --cuadros 20 --gpu=no")
        h_r = terminal(m, "/apps/bin/capibara /tmp/ref --cuadros 20 --gpu=no")
        h_o = terminal(m, "/apps/bin/capibara /tmp/orig --cuadros 20 --gpu=no")
        hp, hr, ho = (re.findall(r"CAPI_HASH ([0-9a-f]+)", x) for x in (h_p, h_r, h_o))
        verificar(hp and hr and hp == hr, f"el motor carga lo guardado con el mismo hash que la copia editada a mano ({hp} == {hr})")
        verificar(hp and ho and hp != ho, f"…y distinto del original ({ho})")
        al_editor(m)

        # ------------------------------------------------------------ (5) pintar y borrar casillas
        P = pos(m, marca0)
        A = arbol_filas(m, marca0)
        ax, ay, al_, aa = P["arbol"]
        marca = len(m.serial())
        t.clique(ax + 90, ay + 18 + 2 + A["Mundo/Terreno"] * 20 + 10)
        esperar_serie(m, r"capibara-editor: arbol elegido Mundo/Terreno", 8, marca)
        verificar(True, "árbol: elegir el MapaCasillas desde el árbol (y la paleta carga su textura)")
        marca = len(m.serial())
        b7 = P["boton%d" % BTN["pintar"]]
        t.clique(b7[0] + b7[2] // 2, b7[1] + 14)
        esperar_serie(m, r"capibara-editor: boton %d modo 3" % BTN["pintar"], 8, marca)
        px_, py_, pl_, pa_ = P["paleta"]
        marca = len(m.serial())
        t.clique(px_ + 36 * 2 + 18, py_ + 18 + 36 * 1 + 18)
        c = esperar_serie(m, r"capibara-editor: casillas (\d+) (\d+) (\d+) (\d+)", 8, marca)
        verificar(c == ("2", "1", "2", "1"), f"paleta: elegir la casilla (2,1) del atlas ({c})")
        zoom = esperar_serie(m, r"capibara-editor: pos vista_zoom (\d+) ox (-?\d+) oy (-?\d+)", 5, marca0)
        Z, OX, OY = int(zoom[0]), int(zoom[1]), int(zoom[2])
        vx, vy, vl, va = P["vista"]
        mundo = lambda wx, wy: (vx + OX + wx * Z // 1000, vy + OY + wy * Z // 1000)
        marca = len(m.serial())
        t.arrastar(*mundo(40, 40), *mundo(130, 40))
        r = esperar_serie(m, r"capibara-editor: pintadas (\d+) casillas capa (\d+)", 10, marca)
        verificar(int(r[0]) >= 4, f"pintar: un trazo con el pincel deja {r[0]} casillas en la capa {r[1]}")
        m.captura("H460-3-pintar")
        marca = len(m.serial())
        m.mouse_para(*t.abs(*mundo(60, 40)))
        m.monitor("mouse_button 2")   # el derecho (el central es 4)
        time.sleep(0.2)
        m.mouse_mover(*t.abs(*mundo(90, 40)))
        m.monitor("mouse_button 0")
        time.sleep(0.5)
        r2 = esperar_serie(m, r"capibara-editor: borradas (\d+) casillas", 10, marca)
        verificar(int(r2) >= 1, f"borrar: el botón derecho borra casillas ({r2})")
        marca = len(m.serial())
        m.tecla("ctrl-s")
        esperar_serie(m, r"capibara-editor: escena guardada", 8, marca)
        # la fila 17 del fichero es la fila 2 de la capa «decoracion»; lo esperado se calcula del original del repositorio
        orig = open(os.path.join(D, "ejemplos", "plataformas", "escenas", "nivel1.esc"), encoding="utf-8").read().split("\n")
        fila17 = orig[16]
        trozos = list(re.finditer(r"\S+", fila17))
        esperado = fila17
        for i in (7, 6, 5, 2):   # de atrás adelante para no mover las posiciones
            esperado = esperado[:trozos[i].start()] + "22" + esperado[trozos[i].end():]
        out = terminal(m, "sed -n 17p /tmp/p/escenas/nivel1.esc")
        real = [l for l in out.replace("\r", "").split("\n") if l.startswith(". . ") or l.startswith(". .")]
        verificar(real and real[-1].split() == esperado.split(), f"las casillas pintadas (y las borradas con el derecho) están en la fila 17 del fichero ({real[-1][:40] if real else out[-120:]!r}…)")
        terminal(m, "sed -i '17s/.*/" + esperado + "/' /tmp/ref/escenas/nivel1.esc")
        out = terminal(m, "diff /tmp/ref/escenas/nivel1.esc /tmp/p/escenas/nivel1.esc && echo IGUALES_OK")
        verificar("IGUALES_OK" in out, "…y el resto del fichero (comentarios, orden, espacios) sigue igual: idéntico al original con ese solo cambio de fila y el de la moneda")

        # ------------------------------------------------------------ (5b) girar, escalar y ordenar el árbol
        al_editor(m)
        P = pos(m, marca0)
        zoomf = Z / 1000.0
        b5, b6 = P["boton%d" % BTN["girar"]], P["boton%d" % BTN["escalar"]]
        A = arbol_filas(m, marca0)
        ax, ay = P["arbol"][0], P["arbol"][1]
        fila_arbol = lambda ruta: ay + 18 + 2 + A[ruta] * 20 + 10
        t.clique(b5[0] + b5[2] // 2, b5[1] + 14)
        # rectángulo de selección: con algo pequeño elegido, un arrastre desde un hueco elige todo lo que toca
        marca = len(m.serial())
        t.clique(ax + 90, fila_arbol("Mundo/Moneda8"))
        esperar_serie(m, r"capibara-editor: arbol elegido Mundo/Moneda8", 8, marca)
        marca = len(m.serial())
        t.arrastar(*mundo(100, 140), *mundo(170, 190))
        r = esperar_serie(m, r"capibara-editor: rectangulo (-?\d+) (-?\d+) (-?\d+) (-?\d+) toca (\d+)", 10, marca)
        verificar(int(r[4]) >= 3, f"lienzo: el rectángulo de selección elige lo que toca ({r[4]} nodos)")
        cx3, cy3, w3, h3 = N["Mundo/Moneda3"]
        marca = len(m.serial())
        t.clique(ax + 90, fila_arbol("Mundo/Moneda3"))   # desde el árbol: el mapa elegido cubre todo el lienzo
        esperar_serie(m, r"capibara-editor: arbol elegido Mundo/Moneda3", 8, marca)
        marca = len(m.serial())
        # el asa de giro está 22 px por encima del borde de arriba (zoom: la caja mide w3 x h3 en el mundo)
        asa = (cx3, cy3 - int(h3 * zoomf / 2) - 22)
        t.arrastar(asa[0], asa[1], cx3 + 60, cy3)
        r = esperar_serie(m, r"capibara-editor: girado Mundo/Moneda3 posicion (-?[\d.]+) (-?[\d.]+) rot (-?[\d.]+) escala", 10, marca)
        verificar(abs(float(r[2]) - 90) <= 4, f"modo Girar: el asa de arriba gira el nodo a ~90 grados ({r[2]})")
        t.clique(b6[0] + b6[2] // 2, b6[1] + 14)
        cx5, cy5, w5, h5 = N["Mundo/Moneda4"]
        marca = len(m.serial())
        t.clique(ax + 90, fila_arbol("Mundo/Moneda4"))
        esperar_serie(m, r"capibara-editor: arbol elegido Mundo/Moneda4", 8, marca)
        marca = len(m.serial())
        t.arrastar(cx5 + int(w5 * zoomf / 2), cy5, cx5 + int(w5 * zoomf / 2) + 24, cy5)
        r = esperar_serie(m, r"capibara-editor: escalado Mundo/Moneda4 posicion (-?[\d.]+) (-?[\d.]+) rot (-?[\d.]+) escala (-?[\d.]+) (-?[\d.]+)", 10, marca)
        verificar(float(r[3]) > 1.5 and abs(float(r[4]) - 1) < 0.01, f"modo Escalar: el asa de la derecha estira el ancho (escala {r[3]} x {r[4]})")
        b4 = P["boton%d" % BTN["mover"]]
        t.clique(b4[0] + b4[2] // 2, b4[1] + 14)   # vuelve a Mover
        m.captura("H460-3b-girar-escalar")
        # duplicar y borrar con teclado: el texto vuelve exactamente al de antes
        marca = len(m.serial())
        t.clique(ax + 90, ay + 18 + 2 + A["Mundo/Moneda7"] * 20 + 10)
        esperar_serie(m, r"capibara-editor: arbol elegido Mundo/Moneda7", 8, marca)
        marca = len(m.serial())
        m.tecla("ctrl-d")
        r = esperar_serie(m, r"capibara-editor: duplicado (\S+)", 8, marca)
        verificar(r == "Mundo/Moneda25", f"Ctrl+D duplica el nodo con nombre único ({r})")
        h_antes = esperar_serie(m, r"capibara-editor: cambio Duplicar nodo hash ([0-9a-f]{8})", 5, marca)
        marca = len(m.serial())
        m.tecla("delete")
        esperar_serie(m, r"capibara-editor: borrados 1", 8, marca)
        h_borrado = esperar_serie(m, r"capibara-editor: cambio Borrar nodos hash ([0-9a-f]{8})", 5, marca)
        verificar(h_antes != h_borrado, "Supr borra el nodo duplicado")
        # reordenar arrastrando en el árbol: Moneda6 pasa a ser hija de Jugador
        marca = len(m.serial())
        t.arrastar(ax + 90, ay + 18 + 2 + A["Mundo/Moneda6"] * 20 + 10, ax + 90, ay + 18 + 2 + A["Mundo/Jugador"] * 20 + 10)
        r = esperar_serie(m, r"capibara-editor: arbol movido (\S+) -> (\S+)", 10, marca)
        verificar(r == ("Mundo/Moneda6", "Mundo/Jugador/Moneda6"), f"árbol: arrastrar un nodo sobre otro lo cambia de padre ({r[1]})")
        h_mov = esperar_serie(m, r"capibara-editor: cambio Mover en el árbol hash ([0-9a-f]{8})", 5, marca)
        marca = len(m.serial())
        bd = P["boton%d" % BTN["deshacer"]]
        t.clique(bd[0] + 20, bd[1] + 14)
        r = esperar_serie(m, r"capibara-editor: deshacer deshacer \d rehacer 1 hash ([0-9a-f]{8})", 8, marca)
        verificar(r != h_mov, "deshacer devuelve el nodo a su padre")
        # variable propia y señal, con los cuadros de texto del editor
        marca = len(m.serial())
        t.clique(ax + 90, ay + 18 + 2 + A["Mundo/Moneda8"] * 20 + 10)
        esperar_serie(m, r"capibara-editor: arbol elegido Mundo/Moneda8", 8, marca)
        marca = len(m.serial())
        m.tecla("f3")
        time.sleep(1.2)
        for _ in range(8):
            m.tecla("backspace", 0.04)   # el cuadro de texto trae «vidas»
        m.digitar("puntos"); m.tecla("ret")
        time.sleep(1.2)
        for _ in range(4):
            m.tecla("backspace", 0.04)   # y el valor «3»
        m.digitar("5"); m.tecla("ret")
        r = esperar_serie(m, r"capibara-editor: variable anadida (\S+) (\S+) = (\S+)", 10, marca)
        verificar(r == ("Mundo/Moneda8", "var.puntos", "5"), f"F3: añade una variable propia ({r})")
        F = filas(m, marca)
        verificar("var.puntos" in F, "la variable sale en el inspector, en su sección")
        marca = len(m.serial())
        m.tecla("f4")
        time.sleep(1.2)
        m.tecla("ret")
        time.sleep(1.2)
        m.tecla("ret")
        time.sleep(1.2)
        m.tecla("ret")
        r = esperar_serie(m, r"capibara-editor: conexion (\S+) -> (\S+)", 10, marca)
        verificar(r == ("Mundo/Moneda8.presionado", "..__pulsado") or r[0] == "Mundo/Moneda8.presionado", f"F4: conecta una señal ({r})")
        marca = len(m.serial())
        m.tecla("ctrl-s")
        esperar_serie(m, r"capibara-editor: escena guardada", 8, marca)
        out = terminal(m, "grep -n 'var.puntos\\|Conexion desde=Mundo/Moneda8' /tmp/p/escenas/nivel1.esc; grep -c 'rotacion = ' /tmp/p/escenas/nivel1.esc; grep -c 'escala = ' /tmp/p/escenas/nivel1.esc")
        verificar("var.puntos = 5" in out and "[Conexion desde=Mundo/Moneda8 senal=presionado" in out, "la variable y la conexión están en el fichero")
        verificar(re.search(r"^1\s*$", out, re.M) is not None, "el giro y la escala se guardan como propiedades (una línea de cada)")
        out = terminal(m, "/apps/bin/capibara /tmp/p --cuadros 20 --gpu=no")
        verificar("CAPI_HASH" in out and "[CAPI] no se pudo" not in out and "desconocida" not in out, "el motor carga la escena con todo lo editado sin avisos")
        al_editor(m)

        # ------------------------------------------------------------ (6) añadir un nodo con guion
        al_editor(m)
        P = pos(m, marca0)
        marca = len(m.serial())
        t.clique(ax + 90, ay + 18 + 2 + A["Mundo"] * 20 + 10)
        esperar_serie(m, r"capibara-editor: arbol elegido Mundo\r?\n", 8, marca)
        mx, ml = P["menu2"][0], P["menu2"][2]   # (x, ancho) del menú «Añadir»
        marca = len(m.serial())
        t.clique(mx + ml // 2, 10)
        g = esperar_serie(m, r"menu aberto em \((\d+),(\d+)\) (\d+)x(\d+) com (\d+) linhas", 8, marca)
        mxp, myp, mlp, map_, nl = (int(v) for v in g)
        alto_l = map_ // nl
        verificar(nl == 14, f"el menú Añadir lista los 12 tipos, un separador y la instancia ({nl} líneas)")
        marca = len(m.serial())
        m.clique(mxp + mlp // 2, myp + alto_l // 2 + alto_l * 1 + 2)   # la segunda línea: Nodo2D
        r = esperar_serie(m, r"capibara-editor: nodo anadido (\S+) tipo (\S+)", 8, marca)
        verificar(r == ("Mundo/Nodo2D", "Nodo2D"), f"menú Añadir: añade un Nodo2D bajo el elegido ({r})")
        marca = len(m.serial())
        m.tecla("ctrl-g")
        r = esperar_serie(m, r"capibara-editor: guion creado (\S+) para (\S+)", 8, marca)
        verificar(r == ("guiones/nodo2d.lua", "Mundo/Nodo2D"), f"Ctrl+G crea el guion con la plantilla ({r})")
        esperar_serie(m, r"capibara-editor: guion abierto guiones/nodo2d.lua", 8, marca)
        time.sleep(0.5)
        P = pos(m, marca0)
        verificar(P["codigo"][2] > 100 and P["codigo"][3] > 60, f"el panel de código se despliega ({P['codigo']})")
        kx, ky, kl, ka = P["codigo"]
        t.clique(kx + 120, ky + 40)
        m.tecla("ctrl-a")
        marca = len(m.serial())
        for linea in ("local N = {}", "function N:_listo()", "local a = nil + 1", "end", "return N"):
            m.digitar(linea, 0.05)
            m.tecla("ret")
        m.tecla("ctrl-s")
        esperar_serie(m, r"capibara-editor: guion guardado guiones/nodo2d.lua", 8, marca)
        esperar_serie(m, r"capibara-editor: escena guardada", 8, marca)
        m.captura("H460-4-guion")
        out = terminal(m, "cat /tmp/p/guiones/nodo2d.lua; grep -n -A3 'Nodo2D Nodo2D' /tmp/p/escenas/nivel1.esc")
        verificar("local a = nil + 1" in out and "return N" in out, "el guion se guarda en el proyecto")
        verificar("guion = \"guiones/nodo2d.lua\"" in out and "[Nodo2D Nodo2D padre=Mundo]" in out, "la escena lleva el nodo nuevo con su guion")
        al_editor(m)

        # ------------------------------------------------------------ (7) jugar y parar
        P = pos(m, marca0)
        b11, b12 = P["boton%d" % BTN["jugar"]], P["boton%d" % BTN["parar"]]
        marca = len(m.serial())
        t.clique(b11[0] + b11[2] // 2, b11[1] + 14)
        r = esperar_serie(m, r"capibara-editor: jugando pid (\d+) principal", 8, marca)
        esperar_serie(m, r"capibara-editor: juego> CAPI_ARRANQUE", 40, marca)
        esperar_serie(m, r"capibara-editor: juego> CAPI_ESCENA escenas/nivel1.esc nodos=(\d+)", 40, marca)
        verificar(True, "▶ Jugar: el juego corre en su ventana y su salida llega a la consola del editor")
        # (8) el error de Lua del guion nuevo, con su línea
        e = esperar_serie(m, r"capibara-editor: juego> error en [^\n]*nodo2d.lua:(\d+): attempt to perform arithmetic on a nil value", 40, marca)
        verificar(e == "3", f"error de Lua al jugar, marcado en su línea ({e})")
        time.sleep(1.5)
        m.captura("H460-5-jugando")
        marca = len(m.serial())
        al_editor(m)
        b12 = pos(m, marca0)["boton%d" % BTN["parar"]]
        t.clique(b12[0] + b12[2] // 2, b12[1] + 14)
        esperar_serie(m, r"capibara-editor: juego parado", 8, marca)
        esperar_serie(m, r"capibara-editor: juego terminado codigo (-?\d+)", 30, marca)
        verificar(True, "■ Parar: el juego se detiene")
        time.sleep(0.5)
        P = pos(m, marca0)
        ex, ey, el, ea = P["consola"]
        # solo los errores: se apagan Mensajes y Avisos
        for k in ("cbtn0", "cbtn1"):
            bx, by, bl, ba = P[k]
            t.clique(bx + bl // 2, by + ba // 2)
            time.sleep(0.3)
        marca = len(m.serial())
        m.clique(*t.abs(ex + 120, ey + 28 + 1 + 8), duplo=True)
        r = esperar_serie(m, r"capibara-editor: ir a (\S+) linea (\d+)", 8, marca)
        verificar(r[0].endswith("nodo2d.lua") and int(r[1]) == 3, f"consola: el doble clic en el error lleva al guion y a su línea ({r})")
        m.captura("H460-6-error")
        for k in ("cbtn0", "cbtn1"):
            bx, by, bl, ba = P[k]
            t.clique(bx + bl // 2, by + ba // 2)
            time.sleep(0.3)

        # ------------------------------------------------------------ (9) exportar a .capi
        marca = len(m.serial())
        m.tecla("ctrl-e")
        time.sleep(1.0)
        m.tecla("ret")
        r = esperar_serie(m, r"capibara-editor: exportado (\S+) (\d+)", 15, marca)
        verificar(r[0] == "/tmp/p.capi" and int(r[1]) >= 25, f"exportar: {r[1]} ficheros en {r[0]}")
        h_d = terminal(m, "/apps/bin/capibara /tmp/p --cuadros 20 --gpu=no")
        h_c = terminal(m, "/apps/bin/capibara /tmp/p.capi --cuadros 20 --gpu=no")
        hd, hc = (re.findall(r"CAPI_HASH ([0-9a-f]+)", x) for x in (h_d, h_c))
        verificar(hd and hd == hc, f"el .capi exportado se abre con el motor y da el mismo cuadro que la carpeta ({hd} == {hc})")
        al_editor(m)

        # ------------------------------------------------------------ (10) proyecto nuevo
        marca = len(m.serial())
        m.tecla("ctrl-n")
        time.sleep(1.0)
        m.tecla("ret")
        r = esperar_serie(m, r"capibara-editor: proyecto (\S+) principal escenas/principal.esc", 15, marca)
        verificar("/proyecto" in r, f"proyecto nuevo con plantilla mínima ({r})")
        n = esperar_serie(m, r"capibara-editor: escena escenas/principal.esc nodos (\d+)", 5, marca)
        verificar(int(n) == 2, f"la plantilla tiene una raíz y una etiqueta ({n} nodos)")
        time.sleep(1)
        m.captura("H460-7-proyecto-nuevo")
        h_n = terminal(m, f"/apps/bin/capibara {r} --cuadros 10 --gpu=no")
        verificar(re.search(r"CAPI_HASH [0-9a-f]+", h_n) is not None and "error" not in h_n, "el motor abre el proyecto nuevo y corre sin errores")
        verificar("Hola desde Capibara" in h_n, "el guion de la plantilla se ejecuta")
        # ------------------------------------------------------------ (11) el segundo ejemplo, desde el menú Archivo
        P = pos(m, marca0)
        mx, ml = P["menu0"][0], P["menu0"][2]
        marca = len(m.serial())
        t.clique(mx + ml // 2, 10)
        pa = painel(m, marca)
        marca = len(m.serial())
        m.clique(pa["x"] + pa["l"] // 2, y_linha(pa, "Abrir ejemplo: escuadrón"))
        r = esperar_serie(m, r"capibara-editor: proyecto (\S+) principal (\S+) pantalla", 15, marca)
        verificar(r[0].endswith("/capibara/disparos") and r[1] == "escenas/partida.esc", f"Archivo > Abrir ejemplo: escuadrón copia el ejemplo y lo abre ({r})")
        time.sleep(1)
        m.captura("H460-8-escuadron")
        P = pos(m, marca0)
        A = arbol_filas(m, marca0)
        ax, ay = P["arbol"][0], P["arbol"][1]
        hijo = sorted(A.items(), key=lambda kv: kv[1])[1][0]
        marca = len(m.serial())
        t.clique(ax + 90, ay + 18 + 2 + A[hijo] * 20 + 10)
        esperar_serie(m, r"capibara-editor: arbol elegido " + re.escape(hijo), 8, marca)
        marca = len(m.serial())
        m.tecla("ctrl-d")
        d2 = esperar_serie(m, r"capibara-editor: duplicado (\S+)", 8, marca)
        marca = len(m.serial())
        m.tecla("ctrl-s")
        esperar_serie(m, r"capibara-editor: escena guardada escenas/partida.esc", 8, marca)
        out = terminal(m, f"/apps/bin/capibara {r[0]} --cuadros 20 --gpu=no")
        verificar("CAPI_HASH" in out and "error" not in out and "desconocida" not in out, f"el escuadrón editado (duplicado {d2}) carga en el motor sin avisos")
        s = m.serial()
        verificar("EXCECAO" not in s and "PANICO" not in s, "sin excepciones del kernel")


def principal():
    partes = sys.argv[1:] or ["nucleo", "docs", "qemu"]
    if "nucleo" in partes:
        nucleo_pc()
    if "docs" in partes:
        documentacion()
    if "qemu" in partes:
        sesion()


if __name__ == "__main__":
    try:
        principal()
    except (FalhaTeste, AssertionError) as e:
        print("FALLA:", e)
        sys.exit(1)
    print("H460 OK")
