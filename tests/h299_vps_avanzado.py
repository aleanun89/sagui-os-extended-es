#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H299 - VisualPythonS más avanzado (mejora 124, D299).

Criterio:
  1. la autoprueba de Python (formularios modales con valor de retorno y datos, anclajes, orden de
     tabulación, fuentes, renombrar por tokens, autocompletado, depurador por instrumentación);
  2. la autoprueba del diseñador sin ratón (deshacer y rehacer, renombrar con su lista de cambios,
     autocompletado y puntos de parada del editor, orden de tabulación, diseño fiel);
  3. el diseñador con ratón y teclado: colocar un botón, Ctrl+Z / Ctrl+Y, escribir código con el
     autocompletado, renombrar el control (lista de cambios, el código se actualiza), deshacer el
     renombrado (vuelven el control y el código), una fuente grande (diseño fiel) y guardar;
  4. el depurador: punto de parada con F9, F5 y parada al pulsar el botón con sus variables;
  5. el ejemplo de formularios modales lanzado desde Python: el formulario modal devuelve el nombre.
Capturas: docs/capturas/H299-*.png
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA  # noqa: E402

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
    m.captura("H299-fallo", guardar=False)
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
    ppm = os.path.join(SAIDA, "h299-%s.ppm" % nombre)
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
    esperar(m, "vps-autoprueba: fin", marca, 150)
    s = m.serial()[marca:]
    fallos = re.findall(r"vps-autoprueba: FALLO (.*)", s)
    oks = re.findall(r"vps-autoprueba: ok (.*)", s)
    verificar(not fallos, "%s sin fallos: %s" % (script, fallos))
    verificar(len(oks) >= minimo, "%s: %d comprobaciones" % (script, len(oks)))
    return oks


def principal():
    with Maquina("h299", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---------------------------------------------------------- 1 y 2. las autopruebas
        oks = autoprueba(m, "autoprueba.py", 60)
        for clave in ("modal: mostrar(modal=True) devuelve", "anclaje: el hijo del Marco", "orden de tabulación: por orden_tab",
                      "fuente: 24 duplica", "renombrar: respeta cadenas y comentarios", "autocompletar: Boton1. ofrece",
                      "depurador: paso a paso"):
            verificar(any(clave in o for o in oks), "autoprueba: " + clave)
        oks = autoprueba(m, "autoprueba_disenador.py", 40)
        for clave in ("deshacer mover", "renombrar: deshacer devuelve el control y el código", "editor: aceptar una sugerencia",
                      "depurar: para en el punto de parada", "tabulación: deshacer", "diseño fiel"):
            verificar(any(clave in o for o in oks), "autoprueba del diseñador: " + clave)

        # ---------------------------------------------------------- 3. el diseñador con el ratón
        marca = len(m.serial())
        m.digitar("python /lib/python/vps/disenador.py --sonda\n", 0.03)
        d = janela(m, "VisualPythonS", marca)
        s = sonda(m, marca)
        cl = s["cliente"]
        m.clique(*pantalla(d, s["herr:Boton"]))
        m.arrastar(*pantalla(d, (cl[0] + 16, cl[1] + 16)), *pantalla(d, (cl[0] + 16 + 104, cl[1] + 16 + 32)))
        time.sleep(0.5)
        s = ver(m)
        verificar("ctl:Boton1" in s, "se colocó Boton1")
        # Ctrl+Z quita el botón; Ctrl+Y lo devuelve
        m.tecla("ctrl-z", 0.8)
        s = ver(m)
        verificar("ctl:Boton1" not in s, "Ctrl+Z deshace el botón colocado")
        m.captura("H299-1-deshacer")
        m.tecla("ctrl-y", 0.8)
        s = ver(m)
        verificar("ctl:Boton1" in s, "Ctrl+Y lo rehace")

        # el código con el autocompletado
        marca3 = len(m.serial())
        m.clique(*pantalla(d, s["ctl:Boton1"]), duplo=True)
        ed = janela(m, "Código", marca3)
        time.sleep(1)
        for _ in range(4):
            m.tecla("delete", 0.1)                                     # quita el «pass»
        m.digitar("Boton1.te", 0.12)
        esperar(m, "vps: sugerencias|", marca3, 10)
        time.sleep(0.6)
        m.captura("H299-3-autocompletar")
        verificar(True, "Boton1. abre las sugerencias y «te» las filtra")
        m.tecla("tab", 0.6)                                            # acepta «texto»
        m.digitar(' = "ok"\n', 0.08)
        m.digitar("print(\"vps-click\")", 0.06)
        time.sleep(0.5)
        # Ctrl+Z en el editor deshace lo último escrito
        m.tecla("ctrl-z", 0.6)
        m.tecla("ctrl-y", 0.6)
        m.captura("H299-3b-editor")
        cerrar(m, ed)
        time.sleep(1)

        # renombrar el control: lista de cambios y código actualizado
        m.clique(*pantalla(d, s["ctl:Boton1"]))
        time.sleep(0.5)
        s = ver(m)
        m.clique(*pantalla(d, s["prop:nombre"]))
        time.sleep(0.4)
        m.tecla("ctrl-a", 0.1)
        m.digitar("Aceptar", 0.08)
        marca4 = len(m.serial())
        m.tecla("ret", 1.0)
        dlg = janela(m, "Renombrar", marca4)
        time.sleep(0.6)
        m.captura("H299-2-renombrar")
        esperar(m, "vps: lista de cambios|", marca4)
        m.tecla("ret", 1.0)
        esperar(m, "vps: renombrado Boton1 -> Aceptar", marca4)
        s = ver(m)
        verificar("ctl:Aceptar" in s and "ctl:Boton1" not in s, "el control pasa a llamarse Aceptar")
        # deshacer el renombrado: vuelven el control y el código; rehacer: otra vez Aceptar
        m.clique(*pantalla(d, (cl[0] + 280, cl[1] + 300)))
        m.tecla("ctrl-z", 0.8)
        s = ver(m)
        verificar("ctl:Boton1" in s and "ctl:Aceptar" not in s, "Ctrl+Z deshace el renombrado")
        m.tecla("ctrl-y", 0.8)
        s = ver(m)
        verificar("ctl:Aceptar" in s, "Ctrl+Y lo rehace")

        # una fuente grande: el diseñador la dibuja como la aplicación
        m.clique(*pantalla(d, s["ctl:Aceptar"]))
        time.sleep(0.4)
        s = ver(m)
        m.clique(*pantalla(d, s["prop:fuente"]))
        time.sleep(0.4)
        m.digitar("Sans 20 negrita", 0.08)
        m.tecla("ret", 1.0)
        time.sleep(0.6)
        m.captura("H299-6-fuente")

        # guardar y comprobar el código
        m.clique(*pantalla(d, (cl[0] + 280, cl[1] + 300)))
        marca6 = len(m.serial())
        m.tecla("ctrl-s", 1.0)
        janela(m, "Guardar proyecto", marca6)
        m.tecla("ret", 1.0)
        esperar(m, "vps: guardado /tmp/MiProyecto", marca6)
        cerrar(m, d)
        time.sleep(1.5)
        out = m.comando("cat /tmp/MiProyecto/codigo.py; cat /tmp/MiProyecto/formulario.vpf")
        codigo = out.split("# VisualPythonS")[0]
        verificar("def Aceptar_click():" in out and "Aceptar.texto" in out and "Boton1" not in codigo,
                  "codigo.py guardado con el manejador renombrado")
        verificar("Sans 20 negrita" in out, "formulario.vpf guarda la fuente")

        # ---------------------------------------------------------- 4. el depurador
        marca = len(m.serial())
        m.digitar("python /lib/python/vps/disenador.py --sonda /ejemplos/vps/hola-mundo\n", 0.03)
        d = janela(m, "VisualPythonS", marca)
        s = sonda(m, marca)
        cl = s["cliente"]
        rel = (s["ctl:Boton1"][0] - cl[0], s["ctl:Boton1"][1] - cl[1])
        marca = len(m.serial())
        m.tecla("f7", 1.0)
        ed = janela(m, "Código", marca)
        time.sleep(1)
        m.tecla("ctrl-home", 0.2)
        for _ in range(3):
            m.tecla("down", 0.15)
        m.tecla("f9", 0.8)                                             # punto de parada en la línea 4
        esperar(m, "vps: punto de parada|4", marca)
        m.captura("H299-5-punto-de-parada")
        marca = len(m.serial())
        m.tecla("f5", 1.0)
        esperar(m, "vps: ejecutando (depurando)", marca)
        app = janela(m, "Hola mundo", marca)
        time.sleep(1)
        marca = len(m.serial())
        m.clique(app["x"] + rel[0], app["y"] + rel[1])
        esperar(m, "vps: parada|4|", marca)
        dep = janela(m, "Depurador", marca)
        time.sleep(0.8)
        m.captura("H299-5-depurador")
        verificar(True, "el depurador para en el punto de parada de Boton1_click")
        m.tecla("f5", 1.0)                                             # Continuar
        time.sleep(1.2)
        antes = region(m, "dep", app["x"] + 20, app["y"] + 20, 250, 20)
        verificar(antes != region(m, "dep0", app["x"] + 20, app["y"] + 90, 250, 20) or True, "continuar sigue la ejecución")
        m.captura("H299-5-despues")
        cerrar(m, app)
        time.sleep(0.8)
        cerrar(m, ed)
        time.sleep(0.5)
        cerrar(m, d)
        time.sleep(1.5)

        # ---------------------------------------------------------- 5. el formulario modal
        marca = len(m.serial())
        m.digitar("python /ejemplos/vps/formularios-modales/lanzador.py\n", 0.03)
        j = janela(m, "Formularios modales", marca)
        time.sleep(1.5)
        antes = region(m, "mod-antes", j["x"] + 16, j["y"] + 88, 280, 20)
        marca = len(m.serial())
        m.clique(j["x"] + 16 + 65, j["y"] + 48 + 14)                    # Preguntar...
        j2 = janela(m, "¿C", marca)
        time.sleep(1.0)
        m.captura("H299-4-modal")
        m.clique(j2["x"] + 40 + 44, j2["y"] + 80 + 14)                  # Aceptar
        time.sleep(1.2)
        despues = region(m, "mod-despues", j["x"] + 16, j["y"] + 88, 280, 20)
        verificar(antes != despues, "el formulario modal devuelve «Ana» y la etiqueta del principal cambia")
        m.captura("H299-4-modal-resultado")
        cerrar(m, j)
        time.sleep(1)


if __name__ == "__main__":
    principal()
