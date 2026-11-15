# SPDX-License-Identifier: GPL-2.0-or-later
# vps._fuente - tamaños de fuente en los controles (mejora 124, D299).
#
# El sistema tiene una sola tipografía de mapa de bits (8x16). Los tipos de letra vectoriales de
# la libfontes (los del WordArt de Pluma) están en C y no llegan a `sagui`: enseñarlos a Python
# pide un nombre nuevo en el módulo y regenerar los qstr de MicroPython (D034), que no se puede
# sin su repositorio. Lo que sí se hace aquí, en Python:
#
#   fuente = "DejaVu Sans 14 negrita subrayado"
#
#   - tamaño: el texto se pinta a 8x16 en una imagen y se escala con suavizado (12 es el
#     tamaño normal; 9 lo reduce, 24 lo duplica);
#   - negrita (el trazo se engorda un píxel) y subrayado;
#   - la familia se guarda y se muestra en el diseñador, pero hoy no cambia el dibujo.
#
# El mismo código pinta el control en ejecución (Etiqueta y Boton) y en el diseñador.

NORMAL = 12
_cache = {}

NEGRITA = ("negrita", "bold")
CURSIVA = ("cursiva", "italic")
SUBRAYADO = ("subrayado", "underline")


def analizar(fuente):
    """'DejaVu Sans 14 negrita' -> (familia, tamaño, negrita, cursiva, subrayado)."""
    familia = []
    tam = NORMAL
    neg = cur = sub = False
    for p in str(fuente or "").split():
        pl = p.lower()
        if pl.isdigit():
            tam = max(4, min(96, int(pl)))
        elif pl in NEGRITA:
            neg = True
        elif pl in CURSIVA:
            cur = True
        elif pl in SUBRAYADO:
            sub = True
        else:
            familia.append(p)
    return " ".join(familia), tam, neg, cur, sub


def es_nativa(fuente):
    """True si la fuente no cambia nada: se usa la 8x16 de toda la vida."""
    if not fuente:
        return True
    f, t, n, c, s = analizar(fuente)
    return t == NORMAL and not (n or s)


def medir(texto, fuente):
    """(ancho, alto) en píxeles del texto con esa fuente."""
    f, t, n, c, s = analizar(fuente)
    lineas = str(texto).split("\n")
    w = max([len(l) for l in lineas] or [0]) * 8 + (1 if n else 0)
    h = len(lineas) * 16
    return max(1, w * t // NORMAL), max(1, h * t // NORMAL)


def imagen_texto(texto, color, fondo, fuente):
    """Una sagui.Imagen con el texto pintado con esa fuente sobre `fondo` (0xRRGGBB)."""
    import sagui
    clave = (str(texto), color, fondo, str(fuente))
    img = _cache.get(clave)
    if img is not None:
        return img
    f, t, neg, cur, sub = analizar(fuente)
    lineas = str(texto).split("\n")
    w = max([len(l) for l in lineas] or [0]) * 8 + (1 if neg else 0)
    w = max(1, w)
    h = len(lineas) * 16
    base = sagui.Imagen.nueva(w, h, fondo)
    for i, l in enumerate(lineas):
        base.texto(0, i * 16, l, color)
        if neg:
            base.texto(1, i * 16, l, color)
        if sub and l:
            base.rect(0, i * 16 + 14, len(l) * 8 + (1 if neg else 0), 1, color)
    if t != NORMAL:
        base = base.escalar(max(1, w * t // NORMAL), max(1, h * t // NORMAL), True)
    if len(_cache) > 64:
        _cache.clear()
    _cache[clave] = base
    return base
