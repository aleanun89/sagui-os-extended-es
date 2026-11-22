#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H297 - VisualPythonS: formularios y controles en Python al estilo del Visual Basic (mejora 123, D297).

Criterio:
  1. el módulo `vps` sin diseñador (/ejemplos/vps/hola.py): un formulario con una etiqueta, un cuadro
     de texto y un botón; el evento Boton1_click cambia la etiqueta (convención por nombre) y el
     @evento del cuadro de texto desactiva el botón (decorador);
  2. los cinco proyectos de ejemplo se leen, construyen y ejecutan (calculadora: calcula 6 x 7);
  3. el diseñador: colocar un botón arrastrando y una etiqueta con un clic, doble clic en el botón
     (abre el editor de código con `def Boton1_click():`), escribir el evento, ejecutar con F5,
     pulsar el botón y ver el resultado; un error de Python salta a su línea en el editor;
  4. guardar el proyecto (diálogo D272) y reabrirlo;
  5. generar la aplicación y lanzarla desde Inicio > Mis aplicaciones.
Capturas: docs/capturas/H297-*.png
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA  # noqa: E402
from h60_barra_tareas import painel, submenu_pelo_mouse, clicar_aqui  # noqa: E402

BORDA, TITULO = 2, 24


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
    m.captura("H297-fallo", guardar=False)
    raise AssertionError(f"no apareció la ventana {titulo!r}")


def esperar(m, texto, desde=0, tempo=40):
    fim = time.time() + tempo
    while time.time() < fim:
        # el kernel puede escribir su «[SAGUI] processo N terminou» en mitad de una línea del programa
        s = m.serial()[desde:]
        if texto in s or texto in re.sub(r"\[SAGUI\][^\n]*\n", "", s):
            return True
        time.sleep(0.3)
    print(m.serial()[desde:][-1800:])
    raise AssertionError(f"tiempo agotado esperando {texto!r}")


def cerrar(m, j):
    m.clique(j["fx"] + j["l"] + 4 - 2 - 20 + 8, j["fy"] + 12)


def sonda(m, desde):
    """Las posiciones que imprime el diseñador (F12): {clave: [(x, y)...]} relativas a la ventana."""
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


def region(m, nombre, x, y, w, h):
    """Los píxeles de un rectángulo de la pantalla (para ver si algo cambió)."""
    ppm = os.path.join(SAIDA, "h297-%s.ppm" % nombre)
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


def principal():
    with Maquina("h297", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---------------------------------------------------------- 1. el módulo sin diseñador
        marca = len(m.serial())
        m.digitar("python /ejemplos/vps/hola.py\n", 0.03)
        esperar(m, "vps: cargado", marca)
        j = janela(m, "Hola", marca)
        verificar((j["l"], j["a"]) == (300, 150), "el formulario mide 300x150 (%dx%d)" % (j["l"], j["a"]))
        time.sleep(1)
        marca = len(m.serial())
        m.clique(j["x"] + 20 + 60, j["y"] + 50 + 14)                    # Boton1
        esperar(m, "vps: click -> ¡Hola, mundo!", marca)
        m.clique(j["x"] + 20 + 100, j["y"] + 90 + 11)                   # CuadroTexto1
        m.digitar("Ana", 0.08)
        marca = len(m.serial())
        m.clique(j["x"] + 20 + 60, j["y"] + 50 + 14)
        esperar(m, "vps: click -> ¡Hola, Ana!", marca)
        verificar(True, "Boton1_click por nombre y @evento del cuadro de texto")
        cerrar(m, j)
        esperar(m, "terminou com codigo 0", marca)

        # ---------------------------------------------------------- 2. los ejemplos (autoprueba del módulo)
        marca = len(m.serial())
        m.digitar("python /lib/python/vps/autoprueba.py\n", 0.03)
        esperar(m, "vps-autoprueba: fin", marca, 90)
        s = m.serial()[marca:]
        fallos = re.findall(r"vps-autoprueba: FALLO (.*)", s)
        oks = re.findall(r"vps-autoprueba: ok (.*)", s)
        verificar(not fallos, "autoprueba sin fallos: %s" % fallos)
        verificar(len(oks) >= 20, "autoprueba: %d comprobaciones" % len(oks))
        for clave in ("calculadora: 6 * 7 = 42", "tareas: se recupera del fichero", "cronómetro: Iniciar activa el temporizador",
                      "script: @evento y .texto", "un error en un manejador"):
            verificar(any(clave in o for o in oks), "autoprueba: " + clave)
        # la calculadora de ejemplo, con el ratón
        marca = len(m.serial())
        m.digitar("python /ejemplos/vps/calculadora/lanzador.py\n", 0.03)
        j = janela(m, "Calculadora", marca)
        time.sleep(1.5)
        m.clique(j["x"] + 104 + 60, j["y"] + 12 + 11)
        m.tecla("ctrl-a", 0.1)
        m.digitar("6", 0.1)
        m.tecla("tab", 0.2)
        m.tecla("ctrl-a", 0.1)
        m.digitar("7", 0.1)
        m.tecla("tab", 0.2)
        m.tecla("down", 0.4)                                            # abre el combo por teclado
        m.tecla("down", 0.3)
        m.tecla("down", 0.3)
        m.tecla("ret", 0.5)                                             # «*»
        m.clique(j["x"] + 16 + 50, j["y"] + 112 + 14)                   # Calcular
        time.sleep(1.5)
        m.captura("H297-0-calculadora", guardar=False)
        cerrar(m, j)
        time.sleep(1)

        # ---------------------------------------------------------- 3. el diseñador
        marca = len(m.serial())
        m.digitar("python /lib/python/vps/disenador.py --sonda\n", 0.03)
        d = janela(m, "VisualPythonS", marca)
        s = sonda(m, marca)
        cl = s["cliente"]
        verificar("herr:Boton" in s and "boton:Ejecutar" in s, "el diseñador tiene su caja de herramientas y su barra")
        m.captura("H297-1-disenador-vacio", guardar=False)
        # un botón arrastrando
        m.clique(*pantalla(d, s["herr:Boton"]))
        m.arrastar(*pantalla(d, (cl[0] + 16, cl[1] + 16)), *pantalla(d, (cl[0] + 16 + 104, cl[1] + 16 + 32)))
        time.sleep(0.5)
        # una etiqueta con un clic
        m.clique(*pantalla(d, s["herr:Etiqueta"]))
        m.clique(*pantalla(d, (cl[0] + 16, cl[1] + 72)))
        time.sleep(0.5)
        m.tecla("f12", 0.5)
        marca2 = len(m.serial())
        m.tecla("f12", 0.5)
        s = sonda(m, marca2)
        verificar("ctl:Boton1" in s and "ctl:Etiqueta1" in s, "se crearon Boton1 y Etiqueta1")
        m.captura("H297-1-disenador")
        rel = (s["ctl:Boton1"][0] - cl[0], s["ctl:Boton1"][1] - cl[1])   # el centro de Boton1 en el formulario

        # doble clic en el botón: el editor de código en su evento por defecto
        marca3 = len(m.serial())
        m.clique(*pantalla(d, s["ctl:Boton1"]), duplo=True)
        ed = janela(m, "Código", marca3)
        time.sleep(1)
        for _ in range(4):
            m.tecla("delete", 0.1)                                     # quita el «pass»
        m.digitar('Etiqueta1.texto = "Hola"\n', 0.06)
        m.digitar('print("vps-click")', 0.06)
        time.sleep(0.5)
        m.captura("H297-2-codigo")
        # F5: ejecutar
        m.tecla("f5", 1.0)
        esperar(m, "vps: ejecutando", marca3)
        app = janela(m, "Formulario", marca3)
        time.sleep(1)
        marca4 = len(m.serial())
        m.clique(app["x"] + rel[0], app["y"] + rel[1])
        esperar(m, "vps-click", marca4)
        time.sleep(0.5)
        m.captura("H297-3-app-ejecutando")
        verificar(True, "F5 ejecuta la aplicación y el clic del botón corre Boton1_click")
        cerrar(m, app)
        time.sleep(1)

        # un error de Python: salta a su línea en el editor
        m.digitar("\n1/0", 0.06)
        m.tecla("f5", 1.0)
        marca5 = len(m.serial())
        app = janela(m, "Formulario", marca3)
        time.sleep(1)
        m.clique(app["x"] + rel[0], app["y"] + rel[1])
        esperar(m, "vps: error|Boton1_click|4|", marca5)
        time.sleep(1.0)
        m.captura("H297-5-error-linea")
        verificar(True, "el error de la línea 4 se muestra y el cursor salta a ella")
        cerrar(m, app)
        time.sleep(1)
        m.tecla("home", 0.2)
        m.digitar("#", 0.1)                                            # comenta el 1/0
        cerrar(m, ed)                                                  # esconde el editor
        time.sleep(1)

        # ---------------------------------------------------------- 4. guardar y reabrir
        m.clique(*pantalla(d, (cl[0] + 280, cl[1] + 300)))             # un hueco del diseñador (da el foco)
        marca6 = len(m.serial())
        m.tecla("ctrl-s", 1.0)
        dlg = janela(m, "Guardar proyecto", marca6)
        m.captura("H297-6-guardar", guardar=False)
        m.tecla("ret", 1.0)
        # El diálogo propone /home si existe (carpeta_proyectos) y, si no, /tmp: en la imagen de prueba
        # actual hay /home (D388), así que la carpeta se toma de lo que dice el log.
        esperar(m, "vps: guardado /", marca6)
        ruta = re.search(r"vps: guardado (/\S*/MiProyecto)", m.serial()[marca6:])
        verificar(ruta is not None, "el proyecto se guardó en una carpeta MiProyecto")
        ruta = ruta.group(1)

        # ---------------------------------------------------------- 5. generar la aplicación
        marca7 = len(m.serial())
        m.clique(*pantalla(d, s["boton:Generar"]))
        janela(m, "Generar aplicaci", marca7)
        time.sleep(0.8)
        m.tecla("ret", 1.0)                                            # el nombre propuesto: MiProyecto
        esperar(m, "vps: generada /apps/vps/MiProyecto", marca7)
        time.sleep(0.8)
        m.tecla("ret", 1.0)                                            # el aviso
        time.sleep(0.5)
        cerrar(m, d)
        time.sleep(1.5)
        out = m.comando("cat " + ruta + "/formulario.vpf; cat " + ruta + "/codigo.py; ls /apps/vps/MiProyecto")
        verificar("control Boton Boton1" in out and "control Etiqueta Etiqueta1" in out, "formulario.vpf guardado con los controles")
        verificar("def Boton1_click():" in out and 'Etiqueta1.texto = "Hola"' in out, "codigo.py guardado con el evento")
        verificar("lanzador.py" in out and "formulario.vpf" in out and "codigo.py" in out, "la aplicación generada tiene su carpeta")

        # reabrir el proyecto guardado
        marca8 = len(m.serial())
        m.digitar("python /lib/python/vps/disenador.py --sonda " + ruta + "\n", 0.03)
        d2 = janela(m, "VisualPythonS", marca8)
        s2 = sonda(m, marca8)
        verificar("ctl:Boton1" in s2 and "ctl:Etiqueta1" in s2, "el proyecto reabierto trae sus controles")
        time.sleep(0.5)
        cerrar(m, d2)
        time.sleep(1)

        # ---------------------------------------------------------- 6. lanzar la app generada desde el menú Inicio
        marca9 = len(m.serial())
        m.clique(40, 12)                                               # el botón Inicio
        p = painel(m, marca9)
        sub = submenu_pelo_mouse(m, p, "Mis aplicaciones", "MiProyecto")   # posiciones del log, no fijas
        time.sleep(0.5)
        m.captura("H297-4-menu-inicio")
        marca9 = len(m.serial())
        clicar_aqui(m)                                                 # la aplicación generada
        gen = janela(m, "Formulario", marca9)
        time.sleep(1.5)
        antes = region(m, "antes", gen["x"] + 16, gen["y"] + 72, 80, 20)
        m.clique(gen["x"] + rel[0], gen["y"] + rel[1])
        time.sleep(1.0)
        despues = region(m, "despues", gen["x"] + 16, gen["y"] + 72, 80, 20)
        verificar(antes != despues, "la aplicación generada, lanzada desde el menú, responde al botón (la etiqueta cambia)")
        m.captura("H297-7-app-generada")
        cerrar(m, gen)
        time.sleep(1)


if __name__ == "__main__":
    principal()
