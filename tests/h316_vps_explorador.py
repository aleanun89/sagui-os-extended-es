#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H316 - VisualPythonS: explorador del proyecto y más eventos (v8.4.1, D315).

Criterio:
  1. las autopruebas de vps (todos los eventos nuevos, con event_generate sobre los componentes) y del
     diseñador (el explorador: árbol, clic entre formularios y páginas, doble clic al código, filtro,
     menú contextual, orden z y sincronía con deshacer);
  2. el explorador con el ratón y el teclado: Ctrl+R lo muestra; un clic en un control de otro
     formulario lo selecciona (cambia de formulario); un clic en un control de otra pestaña cambia de
     página; el doble clic abre el editor de código; el filtro; renombrar con F2 (refactoriza el código);
  3. eventos con el ratón real: raton_entra y raton_sale (el botón cambia de color), doble clic, rueda;
     cerrando (se cancela con Esc en «¿Guardar los cambios?» y la segunda vez cierra y dispara cerrado);
     Intro en un CuadroTexto y validar (cancelado: el foco vuelve al cuadro).
Capturas: docs/capturas/H316-*.png
Con H316_SOLO=auto,explorador,pestanas,eventos,cerrar,validar solo se hacen esos apartados.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA  # noqa: E402

BORDA, TITULO = 2, 24
SOLO = [x for x in os.environ.get("H316_SOLO", "").split(",") if x]


def janela(m, titulo, desde=0, tempo=40):
    """La última ventana con ese título (empieza por): dict con la posición del contenido."""
    fim = time.time() + tempo
    while time.time() < fim:
        achada = re.findall(rf'criada por \d+: "{re.escape(titulo)}[^"]*" (\d+)x(\d+) em \((\d+),(\d+)\)', m.serial()[desde:])
        if achada:
            g = achada[-1]
            return dict(l=int(g[0]), a=int(g[1]), fx=int(g[2]), fy=int(g[3]), x=int(g[2]) + BORDA, y=int(g[3]) + TITULO)
        time.sleep(0.3)
    print(m.serial()[desde:][-1800:])
    m.captura("H316-fallo", guardar=False)
    raise AssertionError(f"no apareció la ventana {titulo!r}")


def esperar(m, texto, desde=0, tempo=40):
    fim = time.time() + tempo
    while time.time() < fim:
        if texto in m.serial()[desde:]:
            return True
        time.sleep(0.3)
    print(m.serial()[desde:][-1800:])
    raise AssertionError(f"tiempo agotado esperando {texto!r}")


def cerrar(m, j):
    m.clique(j["fx"] + j["l"] + 4 - 2 - 20 + 8, j["fy"] + 12)


def sonda(m, desde):
    """Las posiciones que imprime el diseñador (F12): {clave: (x, y)} relativas a la ventana."""
    esperar(m, "vps-sonda: fin", desde)
    s = m.serial()[desde:]
    fin = s.rfind("vps-sonda: fin")
    ini = s.rfind("vps-sonda: cliente", 0, fin)
    r = {}
    for linea in s[ini:fin].splitlines():
        p = linea.strip().split("|")
        if len(p) >= 3 and p[0].startswith("vps-sonda: "):
            clave = p[0][len("vps-sonda: "):]
            if len(p) == 3:
                r[clave] = (int(p[1]), int(p[2]))
            else:
                r[clave + ":" + p[1]] = (int(p[2]), int(p[3]))
    return r


def ver(m):
    """Pide la sonda con F12 y la lee."""
    marca = len(m.serial())
    m.tecla("f12", 0.7)
    return sonda(m, marca)


def region(m, nombre, x, y, w, h):
    ppm = os.path.join(SAIDA, "h316-%s.ppm" % nombre)
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor("screendump " + ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    l, a, px = ler_ppm(ppm)
    return b"".join(bytes(px[((y + k) * l + x) * 3:((y + k) * l + x + w) * 3]) for k in range(h))


def pantalla(j, xy):
    return j["x"] + xy[0], j["y"] + xy[1]


def autoprueba(m, script, minimo):
    marca = len(m.serial())
    m.digitar("python /lib/python/vps/%s\n" % script, 0.03)
    esperar(m, "vps-autoprueba: fin", marca, 240)
    s = m.serial()[marca:]
    fallos = re.findall(r"vps-autoprueba: FALLO (.*)", s)
    oks = re.findall(r"vps-autoprueba: ok (.*)", s)
    verificar(not fallos, "%s sin fallos: %s" % (script, fallos))
    verificar(len(oks) >= minimo, "%s: %d comprobaciones" % (script, len(oks)))
    return oks


def lanzar(m, ejemplo, titulo, espera=1.5):
    """Lanza un ejemplo de /ejemplos/vps/ y devuelve su ventana."""
    time.sleep(1.5)
    marca = len(m.serial())
    m.digitar("python /ejemplos/vps/%s/lanzador.py\n" % ejemplo, 0.03)
    j = janela(m, titulo, marca)
    time.sleep(espera)
    return j


def clic(m, j, x, y, **kw):
    m.clique(j["x"] + x, j["y"] + y, **kw)


def reg(m, j, nombre, x, y, w, h):
    return region(m, nombre, j["x"] + x, j["y"] + y, w, h)


def quiere(nombre):
    return not SOLO or nombre in SOLO


def abrir_disenador(m, proyecto):
    marca = len(m.serial())
    m.digitar("python /lib/python/vps/disenador.py --sonda %s\n" % proyecto, 0.03)
    d = janela(m, "VisualPythonS", marca)
    sonda(m, marca)
    time.sleep(0.5)
    return d


def cerrar_disenador(m, d):
    """Sale del diseñador: la X y «Sí» a guardar los cambios (en la copia de /ejemplos, que es de RAM)."""
    cerrar(m, d)
    time.sleep(1.5)
    m.tecla("ret", 1.0)
    time.sleep(3.0)


def principal():
    with Maquina("h316", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---------------------------------------------------------- 1. las autopruebas
        if quiere("auto"):
            oks = autoprueba(m, "autoprueba.py", 140)
            for clave in ("eventos: doble_click y raton_abajo", "eventos: raton_entra, raton_mueve y raton_sale",
                          "eventos: rueda con rueda_delta", "eventos: tecla_abajo y tecla_arriba", "eventos: intro al pulsar Intro",
                          "eventos: validar cancelable", "eventos: cambio en Casilla y Opcion", "eventos: ordenar al pulsar la cabecera",
                          "eventos: celda_cambio con fila y columna", "eventos: cerrando cancelado", "eventos: activar y desactivar",
                          "eventos: menu_contextual", "eventos: raton_mueve sin manejador no calcula nada",
                          "explorador: árbol Proyecto > Formularios", "explorador: el filtro deja los nodos"):
                verificar(any(clave in o for o in oks), "autoprueba: " + clave)
            oks = autoprueba(m, "autoprueba_disenador.py", 100)
            for clave in ("explorador: Ctrl+R lo muestra", "explorador: un clic cambia de formulario y selecciona el control",
                          "explorador: un clic cambia de formulario y de pestaña", "explorador: doble clic abre Boton1_click",
                          "explorador: doble clic en un manejador salta a su línea", "explorador: el filtro por nombre",
                          "explorador: renombrar desde el menú cambia el código y el árbol", "explorador: deshacer lo sincroniza",
                          "explorador: traer al frente", "explorador: eliminar desde el menú", "explorador: agregar formulario desde el menú",
                          "explorador: se sincroniza al añadir un control"):
                verificar(any(clave in o for o in oks), "autoprueba del diseñador: " + clave)

        # ---------------------------------------------------------- 2. el explorador con el ratón
        if quiere("explorador"):
            d = abrir_disenador(m, "/ejemplos/vps/formularios-modales")
            m.tecla("ctrl-r", 1.0)
            s = ver(m)
            verificar("nodo:f:Formulario2" in s and "nodo:c:CuadroTexto1" in s, "Ctrl+R muestra el explorador con los dos formularios")
            verificar("ctl:Boton1" in s and "ctl:CuadroTexto1" not in s, "el diseñador enseña el primer formulario")
            m.captura("H316-1-explorador")
            # un clic en un control de otro formulario
            m.clique(*pantalla(d, s["nodo:c:CuadroTexto1"]))
            time.sleep(0.5)
            s = ver(m)
            verificar("ctl:CuadroTexto1" in s and "ctl:Boton1" not in s, "un clic en CuadroTexto1 salta al formulario Formulario2")
            # doble clic: el editor de código
            marca = len(m.serial())
            m.clique(*pantalla(d, s["nodo:c:Boton2"]), duplo=True)
            ed = janela(m, "Código", marca)
            esperar(m, "vps: explorador doble|ctl|Boton2", marca, 10)
            time.sleep(0.8)
            m.captura("H316-1b-explorador-codigo", guardar=False)
            cerrar(m, ed)
            time.sleep(1.0)
            # el filtro
            s = ver(m)
            m.clique(*pantalla(d, s["filtro"]))
            m.digitar("boton", 0.12)
            time.sleep(0.5)
            s = ver(m)
            verificar("nodo:c:Boton1" in s and "nodo:c:Boton3" in s and "nodo:c:Etiqueta1" not in s and "nodo:c:CuadroTexto1" not in s,
                      "el filtro «boton» deja solo los nodos que coinciden")
            m.captura("H316-2-explorador-filtro")
            m.tecla("ctrl-a", 0.1)
            m.tecla("backspace", 0.3)
            s = ver(m)
            verificar("nodo:c:Etiqueta1" in s, "sin filtro vuelve el árbol entero")
            # renombrar con F2 (refactoriza Boton2_click)
            m.clique(*pantalla(d, s["nodo:c:Boton2"]))
            time.sleep(0.4)
            marca = len(m.serial())
            m.tecla("f2", 1.0)
            janela(m, "Renombrar", marca)
            time.sleep(0.5)
            m.tecla("ctrl-a", 0.1)
            m.digitar("Aceptar", 0.08)
            marca2 = len(m.serial())
            m.tecla("ret", 1.0)
            janela(m, "Renombrar Boton2", marca2)
            esperar(m, "vps: lista de cambios|", marca2)
            m.tecla("ret", 1.0)
            esperar(m, "vps: renombrado Boton2 -> Aceptar", marca2)
            time.sleep(0.5)
            s = ver(m)
            verificar("nodo:c:Aceptar" in s and "nodo:c:Boton2" not in s, "renombrar desde el explorador (F2) cambia el nodo")
            cerrar_disenador(m, d)

        if quiere("pestanas"):
            d = abrir_disenador(m, "/ejemplos/vps/pestanas")
            m.tecla("ctrl-r", 1.0)
            s = ver(m)
            verificar("pest:Pestanas1" in s and s["pest:Pestanas1"][0] == 0, "las Pestanas empiezan en la página 0")
            verificar("nodo:c:Casilla2" in s, "el explorador enseña los controles de las páginas")
            m.clique(*pantalla(d, s["nodo:c:Casilla2"]))
            time.sleep(0.5)
            s = ver(m)
            verificar(s["pest:Pestanas1"][0] == 1, "un clic en un control de otra pestaña cambia de página")
            m.captura("H316-1c-explorador-pestanas", guardar=False)
            cerrar_disenador(m, d)

        # ---------------------------------------------------------- 3. eventos con el ratón real
        if quiere("eventos"):
            j = lanzar(m, "boton-raton", "Bot")
            fuera = (j["x"] + 300, j["y"] + 150)
            m.mouse_para(*fuera)
            time.sleep(0.5)
            normal = reg(m, j, "bot0", 24, 24, 160, 40)
            m.mouse_para(j["x"] + 24 + 40, j["y"] + 24 + 20)      # el puntero entra en el botón
            time.sleep(0.8)
            dentro = reg(m, j, "bot1", 24, 24, 160, 40)
            verificar(dentro != normal and b"\xff\xff\x00" in dentro, "raton_entra: el botón se pone amarillo")
            m.captura("H316-4-boton-raton")
            m.mouse_para(*fuera)
            time.sleep(0.8)
            verificar(reg(m, j, "bot2", 24, 24, 160, 40) == normal, "raton_sale: el botón vuelve a su color")
            m.mouse_para(j["x"] + 24 + 40, j["y"] + 24 + 20)
            time.sleep(0.5)
            antes = reg(m, j, "bot3", 24, 104, 300, 20)
            m.clique(j["x"] + 24 + 40, j["y"] + 24 + 20, duplo=True)
            time.sleep(0.8)
            doble = reg(m, j, "bot4", 24, 104, 300, 20)
            verificar(doble != antes, "doble_click: la etiqueta enseña la posición")
            antes = reg(m, j, "bot5", 24, 128, 300, 20)
            m.monitor("mouse_move 0 0 1")
            time.sleep(0.8)
            verificar(reg(m, j, "bot6", 24, 128, 300, 20) != antes, "rueda: la etiqueta enseña rueda_delta")
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("cerrar"):
            marca = len(m.serial())
            j = lanzar(m, "guardar-al-cerrar", "Guardar")
            clic(m, j, 100, 80)
            m.digitar("hola", 0.1)
            time.sleep(0.5)
            m.captura("H316-3-guardar-al-cerrar", guardar=False)
            marca = len(m.serial())
            cerrar(m, j)
            jd = janela(m, "Cerrar", marca)
            time.sleep(0.8)
            m.captura("H316-3-guardar-al-cerrar")
            m.tecla("esc", 1.0)                                    # «Cancelar»: Formulario.cancelar = True
            verificar("\ncerrado" not in m.serial()[marca:], "cerrando cancelado: la ventana sigue abierta")
            verificar(janela(m, "Guardar", 0)["l"] > 0, "la ventana sigue ahí")
            marca = len(m.serial())
            cerrar(m, j)
            janela(m, "Cerrar", marca)
            time.sleep(0.5)
            m.tecla("ret", 1.5)                                    # «Sí»: guarda y cierra
            esperar(m, "cerrado", marca, 15)
            time.sleep(0.5)

        if quiere("validar"):
            j = lanzar(m, "validar-foco", "Validar")
            clic(m, j, 16 + 40, 40 + 11)                           # el cuadro de la edad
            m.digitar("abc", 0.1)
            clic(m, j, 16 + 100, 100 + 11)                         # otro cuadro: validar cancela y el foco vuelve
            time.sleep(0.8)
            t1 = reg(m, j, "val0", 16, 40, 100, 22)
            t2 = reg(m, j, "val1", 16, 100, 200, 22)
            m.digitar("x", 0.15)
            time.sleep(0.5)
            verificar(reg(m, j, "val2", 16, 40, 100, 22) != t1 and reg(m, j, "val3", 16, 100, 200, 22) == t2,
                      "validar cancelado: lo que se escribe después va al cuadro de la edad")
            m.captura("H316-5-validar-foco")
            m.tecla("ctrl-a", 0.1)
            m.digitar("30", 0.1)
            clic(m, j, 16 + 100, 100 + 11)
            time.sleep(0.8)
            antes = reg(m, j, "val4", 16, 160, 300, 20)
            m.digitar("Ana", 0.1)
            m.tecla("ret", 0.8)                                    # el evento intro
            verificar(reg(m, j, "val5", 16, 160, 300, 20) != antes, "intro: la etiqueta saluda al pulsar Intro")
            cerrar(m, j)
            time.sleep(0.8)

        # ---------------------------------------------------------- 4. el manual
        import subprocess
        raiz = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        r = subprocess.run([sys.executable, os.path.join(raiz, "tools", "vps_manual.py"), "--comprobar"],
                           capture_output=True, text=True, env=dict(os.environ, PYTHONDONTWRITEBYTECODE="1"))
        verificar(r.returncode == 0, "el manual coincide con el código y enlaza capturas que existen: " + r.stdout.strip())
        print("H316 OK")


if __name__ == "__main__":
    principal()
