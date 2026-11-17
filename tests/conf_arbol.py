# SPDX-License-Identifier: GPL-2.0-or-later
"""Geometría de la ventana de Configuración con árbol (D367), para las pruebas que la manejan con el ratón.

Todo se calcula como en userland/programas/configuracion.c: layout(), y el orden de la tabla seccoes[].
Las coordenadas de sección son LOCALES del panel (como las del programa: las filas empiezan en x = 24);
`panel()` las pasa a coordenadas de la ventana, y `ventana()` + `v["x"]/v["y"]` a las de la pantalla.
"""
import re

BORDA, TITULO = 2, 24
# El orden del enum de secciones (y de seccoes[]); el árbol las enseña todas desplegadas.
SECCIONES = ("sistema", "tela", "aceleracion", "kernel", "zona", "idioma", "usuarios",
             "escritorio-cat", "escritorio", "barra", "widgets", "fuentes", "capturas",
             "dispositivos", "teclado", "mandos", "sonido",
             "redes", "red", "vpn", "vnc", "ia")


def ventana(m, desde=0):
    g = re.findall(r'janela (\d+) criada por \d+: "Configuraci.n" (\d+)x(\d+) em \((-?\d+),(-?\d+)\)', m.serial()[desde:])
    assert g, "no se creó la ventana de Configuración"
    g = g[-1]
    return dict(l=int(g[1]), a=int(g[2]), x=int(g[3]) + BORDA, y=int(g[4]) + TITULO)


def geometria(l, a):
    """(PX, PY, PW, PH) del panel para una ventana de l x a."""
    tw = 214 if l >= 900 else 192
    px = 8 + tw + 12
    return px, 40, l - px - 8, a - 40 - 72


def arbol(v, chave):
    """Centro, en la pantalla, de la fila del árbol de la sección `chave`."""
    i = SECCIONES.index(chave)
    nivel = 0 if chave in ("sistema", "escritorio-cat", "dispositivos", "redes", "ia") else 1
    return (v["x"] + 8 + 6 + nivel * 14 + 40, v["y"] + 8 + 2 + i * 20 + 10)


def panel(v, xl, yl, rolagem=0):
    """Un punto local del panel (x desde el margen de 16, y desde su borde superior) en la pantalla."""
    px, py, pw, ph = geometria(v["l"], v["a"])
    return (v["x"] + px - 16 + xl, v["y"] + py + yl - rolagem)


def boton_pie(v, nombre):
    """Centro de Aceptar / Cancelar / Aplicar / Predeterminado."""
    l, a = v["l"], v["a"]
    px = geometria(l, a)[0]
    ok = l - 8 - 3 * 88 - 16
    xs = {"aceptar": ok + 44, "cancelar": ok + 96 + 44, "aplicar": ok + 192 + 44, "predeterminado": px + 75}
    return (v["x"] + xs[nombre], v["y"] + a - 40 + 14)


def ir(m, v, chave):
    x, y = arbol(v, chave)
    m.clique(x, y)


def ir_ox(m, ox, oy, chave):
    """Como ir(), con el origen (ox, oy) del área de cliente en vez del diccionario de la ventana."""
    ir(m, dict(x=ox, y=oy), chave)


def panel_ox(ox, oy, xl, yl, l=788, a=500, rolagem=0):
    """Punto local del panel (xl, yl) en la pantalla, con el origen (ox, oy) del área de cliente."""
    return panel(dict(x=ox, y=oy, l=l, a=a), xl, yl, rolagem)


def clic_panel(m, ox, oy, xl, yl, l=788, a=500, rolagem=0):
    m.clique(*panel_ox(ox, oy, xl, yl, l, a, rolagem))


# Las secciones por su nombre de siempre («configuracion: pagina X»): clave -> nombre del registro
LOG = {"tela": "Pantalla", "aceleracion": "Aceleración gráfica", "kernel": "Kernel", "zona": "Fecha y hora",
       "idioma": "Idioma", "usuarios": "Usuarios", "escritorio": "Escritorio", "barra": "Barra y menú Inicio", "widgets": "Widgets", "fuentes": "Fuentes", "capturas": "Capturas",
       "teclado": "Teclado", "mandos": "Mandos", "sonido": "Sonido", "red": "Red", "vpn": "VPN", "vnc": "Escritorio remoto", "ia": "IA"}
HOJAS = tuple(LOG)
# Sección de Colores: las muestras de acento (26 px, paso 32) empiezan en y = 48 (tras la línea de texto)
def muestra_acento(i):
    return (30 + i * 32 + 13, 48 + 13)


def al_final(m, v, veces=4):
    """Desplaza el panel hasta el final con la barra (clic bajo el pulgar). Devuelve nada: la cantidad es cont_h - PH."""
    px, py, pw, ph = geometria(v["l"], v["a"])
    for _ in range(veces):
        m.clique(v["x"] + px + pw - 12 + 6, v["y"] + py + ph - 4)


def rol_max(total, a):
    """Cuánto queda desplazado el panel al final, para un contenido de alto `total` y una ventana de alto `a`."""
    return max(0, total - (a - 112))


# Alto del contenido de Sonido con micrófono y una sola salida (la tarjeta HDA de las pruebas)
SND_TOTAL = 610
# y de cada parte de Sonido (locales, sin desplazar)
SND = dict(vol_y=48, bal_y=158, eq_y0=472, eq_x0=70, mic_y=276)


def al_inicio(m, v, veces=4):
    """Vuelve al principio del panel (clic sobre el carril, encima del pulgar)."""
    px, py, pw, ph = geometria(v["l"], v["a"])
    for _ in range(veces):
        m.clique(v["x"] + px + pw - 12 + 6, v["y"] + py + 4)


def xr_silenciar(v):
    """x local del centro de la casilla «Silenciar» de Sonido (XR - 110 = ancho - 14 - 110)."""
    return geometria(v["l"], v["a"])[2] - 14 - 110 + 7
