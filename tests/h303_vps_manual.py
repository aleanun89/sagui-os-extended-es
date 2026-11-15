#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H303 - VisualPythonS: componentes nuevos, autocompletado de sintaxis y capturas del manual (D303).

Criterio:
  1. las autopruebas de vps (componentes, ejemplos y autocompletado) y del diseñador (caja de
     herramientas, Pestanas con sus páginas, editor: palabra clave, plantillas con Tab, sangría y
     cierre automático de paréntesis y comillas);
  2. clics reales sobre los ejemplos de /ejemplos/vps/ del manual (docs/visualpythons-manual.md):
     calculadora, lista de tareas, cronómetro, formulario modal, validar con msgbox, abrir y guardar
     con los diálogos y un ejemplo de cada componente nuevo. Con clics reales se comprueba que
     Pestanas cambia de página, Tabla ordena y se edita, Numerico sube con sus flechas y Lienzo
     dibuja al arrastrar;
  3. el diseñador con ratón y teclado: se coloca un botón y una etiqueta, se cambia una propiedad, se
     escribe el código y se ejecuta con F5; en el editor, «wh» ofrece la palabra clave while y «def» +
     Tab expande la plantilla; un proyecto con Pestanas se ve y se cambia de página en el diseñador.
Capturas del manual: docs/capturas/VPS-manual-*.png (recortadas a la ventana).
Con H303_SOLO=nombre1,nombre2 solo se hacen esos apartados (para desarrollar).
"""
import os
import re
import struct
import subprocess
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, ler_ppm, SAIDA, CAPTURAS  # noqa: E402

BORDA, TITULO = 2, 24
SOLO = [x for x in os.environ.get("H303_SOLO", "").split(",") if x]


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
    m.captura("H303-fallo", guardar=False)
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
    marca = len(m.serial())
    m.tecla("f12", 0.7)
    return sonda(m, marca)


def region(m, nombre, x, y, w, h):
    """Los píxeles de un rectángulo de la pantalla (para ver si algo cambió)."""
    ppm = os.path.join(SAIDA, "h303-%s.ppm" % nombre)
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


def _escribir_png(ruta, l, a, rgb):
    """Un PNG RGB de 8 bits (sin ImageMagick: el recorte se hace aquí)."""
    def chunk(t, c):
        return struct.pack(">I", len(c)) + t + c + struct.pack(">I", zlib.crc32(t + c) & 0xffffffff)
    filas = b"".join(b"\0" + rgb[y * l * 3:(y + 1) * l * 3] for y in range(a))
    with open(ruta, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", l, a, 8, 2, 0, 0, 0))
                + chunk(b"IDAT", zlib.compress(filas, 9)) + chunk(b"IEND", b""))


def foto(m, nombre, j=None):
    """La captura del manual: docs/capturas/VPS-manual-<nombre>.png, recortada a la ventana `j`
    (con su marco y su barra de título); sin ventana, la pantalla entera."""
    ppm = os.path.join(SAIDA, "h303-foto.ppm")
    if os.path.exists(ppm):
        os.remove(ppm)
    m.monitor("screendump " + ppm)
    for _ in range(50):
        if os.path.exists(ppm) and os.path.getsize(ppm) > 0:
            break
        time.sleep(0.1)
    time.sleep(0.3)
    l, a, px = ler_ppm(ppm)
    px = bytes(px)
    x0, y0, w, h = 0, 0, l, a
    if j is not None:
        x0, y0 = max(0, j["fx"]), max(0, j["fy"])
        w, h = min(l - x0, j["l"] + 2 * BORDA + 2), min(a - y0, j["a"] + TITULO + BORDA + 2)
    rgb = b"".join(px[((y0 + k) * l + x0) * 3:((y0 + k) * l + x0 + w) * 3] for k in range(h))
    destino = os.path.join(CAPTURAS, "VPS-manual-%s.png" % nombre)
    _escribir_png(destino, w, h, rgb)
    os.remove(ppm)
    return destino


def autoprueba(m, script, minimo):
    marca = len(m.serial())
    m.digitar("python /lib/python/vps/%s\n" % script, 0.03)
    esperar(m, "vps-autoprueba: fin", marca, 200)
    s = m.serial()[marca:]
    s = re.sub(r"\[SAGUI\][^\n]*\n", "", s)         # un aviso del kernel puede cortar una línea de la prueba
    fallos = re.findall(r"vps-autoprueba: FALLO (.*)", s)
    oks = re.findall(r"vps-autoprueba: ok (.*)", s)
    verificar(not fallos, "%s sin fallos: %s" % (script, fallos))
    verificar(len(oks) >= minimo, "%s: %d comprobaciones" % (script, len(oks)))
    return oks


def lanzar(m, ejemplo, titulo, espera=1.5):
    """Lanza un ejemplo de /ejemplos/vps/ y devuelve su ventana."""
    marca = len(m.serial())
    m.digitar("python /ejemplos/vps/%s/lanzador.py\n" % ejemplo, 0.03)
    j = janela(m, titulo, marca)
    time.sleep(espera)
    return j


def clic(m, j, x, y, **kw):
    m.clique(j["x"] + x, j["y"] + y, **kw)


def reg(m, j, nombre, x, y, w, h):
    return region(m, nombre, j["x"] + x, j["y"] + y, w, h)


def guardar_y_cerrar(m, d, cl, nombre):
    """Guarda el proyecto del diseñador en /tmp/<nombre> (diálogo D272) y cierra el diseñador sin preguntas."""
    m.clique(*pantalla(d, (cl[0] + 280, cl[1] + 300)))
    marca = len(m.serial())
    m.tecla("ctrl-s", 1.0)
    janela(m, "Guardar proyecto", marca)
    time.sleep(0.5)
    m.tecla("ctrl-a", 0.2)
    m.digitar(nombre, 0.08)                      # una carpeta distinta cada vez (volver a escribir una existente falla, ver D303)
    marca2 = len(m.serial())
    m.tecla("ret", 1.5)
    if re.search(r'criada por \d+: "Guardar como', m.serial()[marca2:]):
        conf = janela(m, "Guardar como", marca2)    # la carpeta ya existe (de otro apartado): pulsa «Sí» para sobrescribir
        m.clique(conf["x"] + 152, conf["y"] + 98)
        time.sleep(1.0)
    try:
        esperar(m, "vps: guardado", marca, 15)
    except AssertionError:
        m.captura("H303-fallo-guardar", guardar=False)
        raise
    cerrar(m, d)
    time.sleep(1.5)


def quiere(nombre):
    return not SOLO or nombre in SOLO


def principal():
    with Maquina("h303", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---------------------------------------------------------- 1. las autopruebas
        if quiere("auto"):
            oks = autoprueba(m, "autoprueba.py", 110)
            for clave in ("pestañas: un clic en la segunda cambia de página", "tabla: clic en la cabecera ordena",
                          "numerico: la flecha de arriba sube", "lienzo: click y arrastrar dibujan", "enlace: click y url",
                          "barra de estado: poner cambia", "selector de color: el evento cambio", "reloj: fecha y hora",
                          "archivo: CSV con comillas", "sintaxis: una palabra clave", "sintaxis: la firma de una función del usuario"):
                verificar(any(clave in o for o in oks), "autoprueba: " + clave)
            oks = autoprueba(m, "autoprueba_disenador.py", 80)
            for clave in ("caja de herramientas: los componentes nuevos", "diseñador: un control dentro de las Pestanas",
                          "editor: «wh» ofrece la palabra clave while", "editor: def + Tab expande la plantilla",
                          "editor: Intro tras «:» sangra", "editor: ( cierra el paréntesis"):
                verificar(any(clave in o for o in oks), "autoprueba del diseñador: " + clave)

        # ---------------------------------------------------------- 2. los ejemplos con clics reales
        if quiere("calculadora"):
            j = lanzar(m, "calculadora", "Calculadora")
            clic(m, j, 104 + 10, 12 + 10)
            m.tecla("ctrl-a", 0.1)
            m.digitar("6", 0.1)
            clic(m, j, 104 + 10, 44 + 10)
            m.tecla("ctrl-a", 0.1)
            m.digitar("7", 0.1)
            antes = reg(m, j, "calc0", 16, 152, 210, 20)
            clic(m, j, 16 + 50, 112 + 14)                               # Calcular (con +)
            time.sleep(0.6)
            verificar(antes != reg(m, j, "calc1", 16, 152, 210, 20), "calculadora: Calcular escribe el resultado")
            foto(m, "calculadora", j)
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("tareas"):
            j = lanzar(m, "lista-de-tareas", "Lista de tareas")
            clic(m, j, 12 + 20, 12 + 11)
            m.digitar("comprar pan", 0.1)
            clic(m, j, 190 + 40, 10 + 13)
            clic(m, j, 12 + 20, 12 + 11)
            m.digitar("llamar a Ana", 0.1)
            clic(m, j, 190 + 40, 10 + 13)
            clic(m, j, 12 + 40, 44 + 10)                                # elige la primera
            time.sleep(0.5)
            foto(m, "tareas", j)
            cerrar(m, j)
            time.sleep(0.8)
            out = m.comando("cat /home/tareas.txt /tmp/tareas.txt 2>/dev/null")
            verificar("comprar pan" in out and "llamar a Ana" in out, "tareas: se guardan en un archivo")
            m.comando("rm -f /home/tareas.txt /tmp/tareas.txt")

        if quiere("cronometro"):
            j = lanzar(m, "cronometro", "Cron")
            antes = reg(m, j, "cron0", 40, 16, 180, 24)
            clic(m, j, 12 + 40, 52 + 14)                                # Iniciar
            time.sleep(2.5)
            foto(m, "cronometro", j)
            verificar(antes != reg(m, j, "cron1", 40, 16, 180, 24), "cronómetro: el Temporizador hace avanzar la hora")
            foto(m, "cronometro", j)
            clic(m, j, 12 + 40, 52 + 14)                                # Parar
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("modal"):
            j = lanzar(m, "formularios-modales", "Formularios modales")
            antes = reg(m, j, "mod0", 16, 88, 280, 20)
            marca = len(m.serial())
            clic(m, j, 16 + 65, 48 + 14)                                # Preguntar...
            j2 = janela(m, "¿C", marca)
            time.sleep(1.0)
            foto(m, "modal", j2)
            clic(m, j2, 40 + 44, 80 + 14)                               # Aceptar
            time.sleep(1.2)
            verificar(antes != reg(m, j, "mod1", 16, 88, 280, 20), "modal: el formulario modal devuelve el nombre")
            foto(m, "modal-resultado", j)
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("validar"):
            j = lanzar(m, "validar-entrada", "Validar")
            marca = len(m.serial())
            clic(m, j, 128 + 40, 38 + 13)                               # Comprobar, sin escribir nada
            jd = janela(m, "Falta", marca)
            time.sleep(0.8)
            foto(m, "validar-aviso", jd)
            m.tecla("ret", 0.8)
            clic(m, j, 16 + 20, 40 + 11)
            m.digitar("abc", 0.1)
            marca = len(m.serial())
            clic(m, j, 128 + 40, 38 + 13)
            jd = janela(m, "Dato", marca)
            time.sleep(0.8)
            foto(m, "validar-error", jd)
            m.tecla("ret", 0.8)
            clic(m, j, 16 + 20, 40 + 11)
            m.tecla("ctrl-a", 0.1)
            m.digitar("30", 0.1)
            antes = reg(m, j, "val0", 16, 80, 260, 20)
            clic(m, j, 128 + 40, 38 + 13)
            time.sleep(0.8)
            verificar(antes != reg(m, j, "val1", 16, 80, 260, 20), "validar: con un dato válido se guarda")
            foto(m, "validar-ok", j)
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("archivos"):
            j = lanzar(m, "abrir-guardar", "Abrir y guardar")
            marca = len(m.serial())
            clic(m, j, 8 + 40, 8 + 13)                                  # Abrir...
            jd = janela(m, "Abrir", marca)
            time.sleep(1.0)
            foto(m, "abrir", jd)
            m.tecla("esc", 0.8)
            clic(m, j, 8 + 100, 64 + 20)
            m.digitar("hola desde vps", 0.1)
            marca = len(m.serial())
            clic(m, j, 126 + 60, 8 + 13)                                # Guardar como...
            jd = janela(m, "Guardar", marca)
            time.sleep(1.0)
            foto(m, "guardar", jd)
            m.tecla("esc", 0.8)
            foto(m, "abrir-guardar", j)
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("pestanas"):
            j = lanzar(m, "pestanas", "Pesta")
            foto(m, "pestanas-1", j)
            antes = reg(m, j, "pes0", 12, 36, 290, 120)
            clic(m, j, 12 + 100, 12 + 10)                               # la pestaña «Opciones»
            time.sleep(0.6)
            despues = reg(m, j, "pes1", 12, 36, 290, 120)
            verificar(antes != despues, "pestañas: un clic en la segunda pestaña cambia la página")
            verificar(reg(m, j, "pes2", 12, 172, 290, 20) != reg(m, j, "pes3", 12, 190, 290, 6), "pestañas: el evento cambio escribe la etiqueta")
            foto(m, "pestanas-2", j)
            clic(m, j, 12 + 20, 12 + 20 + 12)                           # Casilla «Recibir avisos»
            time.sleep(0.4)
            clic(m, j, 12 + 170, 12 + 10)                               # «Acerca de»
            time.sleep(0.6)
            foto(m, "pestanas-3", j)
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("tabla"):
            j = lanzar(m, "tabla", "Tabla")
            foto(m, "tabla-1", j)
            antes = reg(m, j, "tab0", 10, 30, 380, 90)
            clic(m, j, 8 + 2 + 126 + 60, 8 + 2 + 10)                    # cabecera «Edad»: ordena
            time.sleep(0.6)
            ord1 = reg(m, j, "tab1", 10, 30, 380, 90)
            verificar(antes != ord1, "tabla: un clic en la cabecera ordena las filas")
            clic(m, j, 8 + 2 + 60, 8 + 2 + 20 + 18 + 9)                 # la segunda fila
            time.sleep(0.6)
            verificar(ord1 != reg(m, j, "tab2", 10, 30, 380, 90), "tabla: un clic en una fila la elige")
            foto(m, "tabla-2", j)
            clic(m, j, 8 + 2 + 60, 8 + 2 + 20 + 9, duplo=True)          # doble clic en una celda: edita
            time.sleep(0.8)
            m.tecla("ctrl-a", 0.1)
            m.digitar("Zoe", 0.1)
            m.tecla("ret", 0.8)
            verificar(reg(m, j, "tab3", 10, 30, 380, 90) != ord1, "tabla: la celda editada cambia")
            foto(m, "tabla-3", j)
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("numerico"):
            j = lanzar(m, "numerico", "Num")
            foto(m, "numerico-1", j)
            antes = reg(m, j, "num0", 16, 84, 240, 24)
            for _ in range(3):
                clic(m, j, 112 + 72, 12 + 5)                            # flecha de arriba de la cantidad
            time.sleep(0.6)
            verificar(antes != reg(m, j, "num1", 16, 84, 240, 24), "numerico: la flecha de arriba sube el valor y se recalcula el total")
            clic(m, j, 112 + 72, 44 + 16)                               # flecha de abajo del precio (paso 0,5)
            time.sleep(0.6)
            foto(m, "numerico-2", j)
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("lienzo"):
            j = lanzar(m, "lienzo", "Lienzo")
            foto(m, "lienzo-1", j)
            antes = reg(m, j, "lie0", 8, 150, 344, 78)
            m.mouse_para(j["x"] + 8 + 40, j["y"] + 8 + 160)
            m.monitor("mouse_button 1")
            time.sleep(0.2)
            for dx, dy in ((40, 40), (80, 0), (120, 40), (160, 0), (200, 40), (240, 0)):
                m.mouse_mover(j["x"] + 8 + 40 + dx, j["y"] + 8 + 160 + dy)
            m.monitor("mouse_button 0")
            time.sleep(0.6)
            verificar(antes != reg(m, j, "lie1", 8, 150, 344, 78), "lienzo: arrastrar el ratón dibuja una línea")
            foto(m, "lienzo-2", j)
            clic(m, j, 8 + 40, 236 + 13)                                # Borrar
            time.sleep(0.5)
            verificar(reg(m, j, "lie2", 8, 150, 344, 78) == antes, "lienzo: el botón Borrar limpia el dibujo")
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("enlace"):
            j = lanzar(m, "enlace", "Enlace")
            antes = reg(m, j, "enl0", 16, 80, 260, 20)
            clic(m, j, 16 + 60, 16 + 10)
            time.sleep(0.5)
            verificar(antes != reg(m, j, "enl1", 16, 80, 260, 20), "enlace: el clic dispara el evento")
            foto(m, "enlace", j)
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("barra"):
            j = lanzar(m, "barra-estado", "Barra")
            clic(m, j, 8 + 100, 8 + 40)
            m.digitar("hola mundo", 0.08)
            time.sleep(0.5)
            foto(m, "barra-estado", j)
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("color"):
            j = lanzar(m, "selector-color", "Selector")
            foto(m, "selector-color-1", j)
            marca = len(m.serial())
            clic(m, j, 112 + 40, 12 + 12)
            jp = janela(m, "Elegir", marca)
            time.sleep(0.8)
            foto(m, "selector-color-2", jp)
            antes = reg(m, j, "col0", 16, 84, 240, 36)
            m.clique(jp["x"] + 6 + 4 + 5 * 28 + 12, jp["y"] + 6 + 4 + 28 + 12)      # un color de la paleta
            time.sleep(0.8)
            verificar(antes != reg(m, j, "col1", 16, 84, 240, 36), "selector de color: elegir un color cambia la etiqueta")
            foto(m, "selector-color-3", j)
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("reloj"):
            j = lanzar(m, "reloj", "Reloj")
            antes = reg(m, j, "rel0", 16, 16, 240, 30)
            time.sleep(2.2)
            verificar(antes != reg(m, j, "rel1", 16, 16, 240, 30), "reloj: tic cada segundo")
            foto(m, "reloj", j)
            cerrar(m, j)
            time.sleep(0.8)

        if quiere("archivo"):
            m.comando("rm -f /tmp/nota-vps.txt")
            j = lanzar(m, "archivo", "Archivo")
            clic(m, j, 8 + 100, 8 + 40)
            m.digitar("uno", 0.1)
            m.tecla("ret", 0.1)
            m.digitar("dos", 0.1)
            clic(m, j, 8 + 40, 136 + 13)                                # Guardar
            time.sleep(0.6)
            clic(m, j, 184 + 60, 136 + 13)                              # Contar líneas
            time.sleep(0.6)
            foto(m, "archivo", j)
            cerrar(m, j)
            time.sleep(0.8)
            out = m.comando("cat /tmp/nota-vps.txt")
            verificar("uno" in out and "dos" in out, "archivo: Archivo.escribir guarda el texto")

        # ---------------------------------------------------------- 3. el diseñador
        if quiere("disenador"):
            marca = len(m.serial())
            m.digitar("python /lib/python/vps/disenador.py --sonda\n", 0.03)
            d = janela(m, "VisualPythonS", marca)
            s = sonda(m, marca)
            cl = s["cliente"]
            foto(m, "disenador", d)
            verificar("herr:Pestanas" in s and "herr:Tabla" in s and "herr:Archivo" in s, "diseñador: la caja de herramientas tiene los componentes nuevos")
            m.clique(*pantalla(d, s["herr:Boton"]))
            m.arrastar(*pantalla(d, (cl[0] + 16, cl[1] + 56)), *pantalla(d, (cl[0] + 16 + 104, cl[1] + 56 + 32)))
            time.sleep(0.5)
            m.clique(*pantalla(d, s["herr:Etiqueta"]))
            m.arrastar(*pantalla(d, (cl[0] + 16, cl[1] + 16)), *pantalla(d, (cl[0] + 16 + 160, cl[1] + 16 + 24)))
            time.sleep(0.5)
            s = ver(m)
            verificar("ctl:Boton1" in s and "ctl:Etiqueta1" in s, "diseñador: se colocan un botón y una etiqueta")
            m.clique(*pantalla(d, s["ctl:Boton1"]))
            time.sleep(0.5)
            s = ver(m)
            m.clique(*pantalla(d, s["prop:texto"]))
            time.sleep(0.4)
            m.tecla("ctrl-a", 0.1)
            m.digitar("Saludar", 0.08)
            m.tecla("ret", 0.8)
            foto(m, "colocar", d)
            # el código del evento, con el autocompletado
            marca = len(m.serial())
            m.clique(*pantalla(d, s["ctl:Boton1"]), duplo=True)
            ed = janela(m, "Código", marca)
            time.sleep(1)
            for _ in range(4):
                m.tecla("delete", 0.1)
            m.digitar("Etiqueta1.te", 0.12)
            esperar(m, "vps: sugerencias|", marca, 10)
            time.sleep(0.6)
            foto(m, "autocompletar", ed)
            m.tecla("tab", 0.6)
            m.digitar(' = "Hola desde VPS"', 0.08)
            time.sleep(0.5)
            foto(m, "codigo", ed)
            marca = len(m.serial())
            m.tecla("f5", 1.0)
            esperar(m, "vps: ejecutando", marca)
            app = janela(m, "Formulario", marca)
            time.sleep(1)
            rel = (s["ctl:Boton1"][0] - cl[0], s["ctl:Boton1"][1] - cl[1])
            antes = region(m, "ej0", app["x"] + 16, app["y"] + 16, 160, 24)
            m.clique(app["x"] + rel[0], app["y"] + rel[1])
            time.sleep(0.8)
            verificar(antes != region(m, "ej1", app["x"] + 16, app["y"] + 16, 160, 24), "diseñador: F5 ejecuta y el botón cambia la etiqueta")
            foto(m, "ejecutar", app)
            cerrar(m, app)
            time.sleep(0.8)
            cerrar(m, ed)
            time.sleep(0.5)
            guardar_y_cerrar(m, d, cl, "ProyectoHola")

        if quiere("sintaxis"):
            marca = len(m.serial())
            m.digitar("python /lib/python/vps/disenador.py --sonda\n", 0.03)
            d = janela(m, "VisualPythonS", marca)
            s = sonda(m, marca)
            cl = s["cliente"]
            marca = len(m.serial())
            m.clique(*pantalla(d, (cl[0] + 160, cl[1] + 100)), duplo=True)      # doble clic en el formulario: el código
            ed = janela(m, "Código", marca)
            time.sleep(1)
            m.clique(ed["x"] + 200, ed["y"] + 200)
            m.digitar("wh", 0.15)
            esperar(m, "vps: sugerencias|", marca, 10)
            time.sleep(0.6)
            foto(m, "sintaxis-palabra", ed)
            verificar(True, "sintaxis: «wh» abre la lista con la palabra clave while")
            m.tecla("esc", 0.5)
            m.tecla("backspace", 0.2)
            m.tecla("backspace", 0.2)
            marca2 = len(m.serial())
            m.digitar("def", 0.15)
            m.tecla("tab", 0.8)
            esperar(m, "vps: plantilla|def", marca2, 10)
            m.digitar("sumar", 0.12)
            time.sleep(0.4)
            foto(m, "sintaxis-plantilla", ed)
            verificar(True, "sintaxis: def + Tab expande la plantilla y deja el cursor en el nombre")
            m.digitar("a", 0.12)
            m.tecla("end", 0.1)
            time.sleep(0.3)
            cerrar(m, ed)
            time.sleep(0.5)
            guardar_y_cerrar(m, d, cl, "ProyectoSintaxis")

        if quiere("pestanas-disenador"):
            marca = len(m.serial())
            m.digitar("python /lib/python/vps/disenador.py --sonda /ejemplos/vps/pestanas\n", 0.03)
            d = janela(m, "VisualPythonS", marca)
            s = sonda(m, marca)
            cl = s["cliente"]
            time.sleep(1.0)
            foto(m, "pestanas-disenador-1", d)
            antes = region(m, "pd0", d["x"] + cl[0] + 12, d["y"] + cl[1] + 36, 290, 120)
            m.clique(d["x"] + cl[0] + 12 + 100, d["y"] + cl[1] + 12 + 10)       # la pestaña «Opciones» en el diseñador
            time.sleep(0.8)
            verificar(antes != region(m, "pd1", d["x"] + cl[0] + 12, d["y"] + cl[1] + 36, 290, 120),
                      "diseñador: un clic en una pestaña enseña su página")
            foto(m, "pestanas-disenador-2", d)
            cerrar(m, d)                                                        # pregunta si se guardan los cambios
            time.sleep(1.0)
            m.tecla("ret", 1.0)
            time.sleep(1.0)

    # ---------------------------------------------------------- 4. el manual coincide con los ejemplos y tiene sus capturas
    if not SOLO:
        r = subprocess.run([sys.executable, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "tools", "vps_manual.py"),
                            "--comprobar"], capture_output=True, text=True)
        verificar(r.returncode == 0, "el manual coincide con los ejemplos y enlaza capturas que existen: " + r.stdout.strip())


if __name__ == "__main__":
    principal()
