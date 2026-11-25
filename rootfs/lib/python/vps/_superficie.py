# SPDX-License-Identifier: GPL-2.0-or-later
# vps._superficie - los componentes propios del diseñador (mejora 123, D297): la caja de
# herramientas, la superficie del formulario con su rejilla y la ventana de propiedades.
# Son componentes de tkinter (Widget) que se dibujan con el módulo sagui, al estilo Win98.
import sagui
import tkinter as tk
from vps import _modelo as M

GRIS, BLANCO, SOMBRA, NEGRO = 0xC0C0C0, 0xFFFFFF, 0x808080, 0x000000
LUZ, AZUL, AZUL2 = 0xDFDFDF, 0x000080, 0x1084D0
AMARILLO = 0xFFFFE1
ROJO_I = 0xC00000

HERRAMIENTAS = ["Puntero", "Etiqueta", "Boton", "CuadroTexto", "Casilla", "Opcion", "Lista", "Combo",
                "Imagen", "Marco", "Deslizador", "BarraProgreso", "Temporizador", "Menu",
                "Pestanas", "Tabla", "Numerico", "Lienzo", "Enlace", "BarraEstado", "SelectorColor",
                "Reloj", "Archivo", "ConectorBD", "RejillaDatos", "SelectorDeArchivos", "SelectorDeCarpetas",
                "CampoArchivo", "CampoCarpeta", "SelectorDeColor", "SelectorDeFuente", "ComboFuentes", "Calendario", "SelectorFecha", "Arbol", "Notificacion", "Grafico"]
NOMBRES_HERR = {"Puntero": "Puntero (seleccionar)"}

# geometría de la superficie: el formulario se dibuja en (OX, OY); su cliente en (FX, FY)
OX, OY, TB = 12, 12, 22
FX, FY = OX + 4, OY + TB + 2


# ------------------------------------------------------------ dibujo Win98

def elevado(v, x, y, w, h, fondo=GRIS):
    """Relieve de botón (Win98): blanco arriba-izquierda, negro abajo-derecha."""
    if fondo is not None:
        v.rect(x, y, w, h, fondo)
    v.linea(x, y, x + w - 1, y, BLANCO)
    v.linea(x, y, x, y + h - 1, BLANCO)
    v.linea(x, y + h - 1, x + w - 1, y + h - 1, NEGRO)
    v.linea(x + w - 1, y, x + w - 1, y + h - 1, NEGRO)
    v.linea(x + 1, y + 1, x + w - 2, y + 1, LUZ)
    v.linea(x + 1, y + 1, x + 1, y + h - 2, LUZ)
    v.linea(x + 1, y + h - 2, x + w - 2, y + h - 2, SOMBRA)
    v.linea(x + w - 2, y + 1, x + w - 2, y + h - 2, SOMBRA)


def hundido(v, x, y, w, h, fondo=BLANCO):
    """Relieve de campo de texto: sombra arriba-izquierda, blanco abajo-derecha."""
    if fondo is not None:
        v.rect(x, y, w, h, fondo)
    v.linea(x, y, x + w - 1, y, SOMBRA)
    v.linea(x, y, x, y + h - 1, SOMBRA)
    v.linea(x, y + h - 1, x + w - 1, y + h - 1, BLANCO)
    v.linea(x + w - 1, y, x + w - 1, y + h - 1, BLANCO)
    v.linea(x + 1, y + 1, x + w - 2, y + 1, NEGRO)
    v.linea(x + 1, y + 1, x + 1, y + h - 2, NEGRO)
    v.linea(x + 1, y + h - 2, x + w - 2, y + h - 2, LUZ)
    v.linea(x + w - 2, y + 1, x + w - 2, y + h - 2, LUZ)


def grabado(v, x, y, w, h):
    """Marco grabado (el borde de un Marco)."""
    v.rect(x, y, w, h, SOMBRA, relleno=False)
    v.rect(x + 1, y + 1, w, h, BLANCO, relleno=False)


def centrado(v, x, y, w, h, s, col=NEGRO):
    tw = len(s) * 8
    v.texto(x + max(0, (w - tw) // 2), y + max(0, (h - 16) // 2), s, col)


def a_int(c, defecto=NEGRO):
    if c is None or c == "":
        return defecto
    if isinstance(c, str):
        import vps
        t = vps._a_tk(c)
        try:
            return int(t[1:], 16)
        except Exception:
            return defecto
    return int(c)


def recortar(v, x, y, w, h):
    ant = v.recorte()
    ax, ay, aw, ah = ant
    x0, y0 = max(ax, x), max(ay, y)
    x1, y1 = min(ax + aw, x + w), min(ay + ah, y + h)
    v.recorte(x0, y0, max(0, x1 - x0), max(0, y1 - y0))
    return ant


def icono(v, tipo, x, y):
    """El icono de 24x24 de una herramienta, en (x, y)."""
    if tipo == "Puntero":
        v.poligono([(x + 7, y + 3), (x + 7, y + 19), (x + 11, y + 15), (x + 14, y + 21), (x + 16, y + 20),
                    (x + 13, y + 14), (x + 18, y + 14)], NEGRO)
        v.poligono([(x + 8, y + 6), (x + 8, y + 15), (x + 11, y + 12), (x + 15, y + 12)], BLANCO)
    elif tipo == "Etiqueta":
        v.texto(x + 8, y + 4, "A", NEGRO)
        v.texto(x + 9, y + 4, "A", NEGRO)
    elif tipo == "Boton":
        elevado(v, x + 2, y + 6, 20, 13)
        v.rect(x + 7, y + 12, 10, 2, NEGRO)
    elif tipo == "CuadroTexto":
        hundido(v, x + 2, y + 6, 20, 13)
        v.rect(x + 5, y + 9, 1, 7, NEGRO)
    elif tipo == "Casilla":
        hundido(v, x + 5, y + 5, 14, 14)
        v.linea(x + 8, y + 12, x + 10, y + 15, NEGRO)
        v.linea(x + 10, y + 15, x + 15, y + 8, NEGRO)
    elif tipo == "Opcion":
        v.elipse(x + 4, y + 4, 16, 16, BLANCO)
        v.elipse(x + 4, y + 4, 16, 16, SOMBRA, relleno=False)
        v.elipse(x + 9, y + 9, 6, 6, NEGRO)
    elif tipo == "Lista":
        hundido(v, x + 3, y + 3, 18, 18)
        v.rect(x + 5, y + 6, 14, 4, AZUL)
        v.rect(x + 5, y + 12, 10, 2, NEGRO)
        v.rect(x + 5, y + 16, 12, 2, NEGRO)
    elif tipo in ("Combo", "ComboFuentes"):
        hundido(v, x + 2, y + 6, 20, 13)
        elevado(v, x + 14, y + 8, 7, 9)
        v.poligono([(x + 15, y + 11), (x + 19, y + 11), (x + 17, y + 14)], NEGRO)
    elif tipo == "Imagen":
        hundido(v, x + 2, y + 4, 20, 16, 0x87CEEB)
        v.poligono([(x + 3, y + 19), (x + 10, y + 10), (x + 15, y + 19)], 0x208020)
        v.poligono([(x + 11, y + 19), (x + 17, y + 12), (x + 21, y + 19)], 0x30A030)
    elif tipo == "Marco":
        grabado(v, x + 2, y + 6, 19, 14)
        v.rect(x + 5, y + 4, 8, 5, GRIS)
        v.rect(x + 6, y + 6, 6, 1, NEGRO)
    elif tipo == "Deslizador":
        hundido(v, x + 2, y + 10, 20, 4, GRIS)
        elevado(v, x + 9, y + 6, 6, 12)
    elif tipo == "BarraProgreso":
        hundido(v, x + 2, y + 8, 20, 9, GRIS)
        for i in range(4):
            v.rect(x + 4 + i * 4, y + 10, 3, 5, AZUL)
    elif tipo == "Temporizador":
        v.elipse(x + 3, y + 3, 18, 18, BLANCO)
        v.elipse(x + 3, y + 3, 18, 18, NEGRO, relleno=False)
        v.linea(x + 12, y + 12, x + 12, y + 6, NEGRO)
        v.linea(x + 12, y + 12, x + 16, y + 14, NEGRO)
    elif tipo == "SelectorDeArchivos":      # una hoja con la esquina doblada y una flecha de «abrir»
        v.poligono([(x + 5, y + 3), (x + 14, y + 3), (x + 18, y + 7), (x + 18, y + 21), (x + 5, y + 21)], BLANCO)
        v.linea(x + 5, y + 3, x + 5, y + 21, NEGRO)
        v.linea(x + 5, y + 21, x + 18, y + 21, NEGRO)
        v.linea(x + 18, y + 7, x + 18, y + 21, NEGRO)
        v.linea(x + 14, y + 3, x + 18, y + 7, NEGRO)
        v.poligono([(x + 9, y + 11), (x + 15, y + 11), (x + 15, y + 8), (x + 21, y + 14), (x + 15, y + 20),
                    (x + 15, y + 17), (x + 9, y + 17)], 0x2A9D3C)
    elif tipo == "SelectorDeCarpetas":      # una carpeta amarilla con una flecha
        v.rect(x + 3, y + 5, 8, 4, 0xE8C040)
        v.rect(x + 3, y + 8, 18, 12, 0xF4D060)
        v.rect(x + 3, y + 8, 18, 12, NEGRO, relleno=False)
        v.poligono([(x + 8, y + 12), (x + 13, y + 12), (x + 13, y + 10), (x + 18, y + 14), (x + 13, y + 18),
                    (x + 13, y + 16), (x + 8, y + 16)], 0x2A9D3C)
    elif tipo == "SelectorDeColor":
        v.elipse(x + 3, y + 3, 18, 18, BLANCO)
        v.elipse(x + 3, y + 3, 18, 18, NEGRO, relleno=False)
        for k, c in enumerate((0xFF0000, 0x00A000, 0x0000FF, 0xFFFF00)):
            v.elipse(x + 6 + (k % 2) * 6, y + 6 + (k // 2) * 6, 5, 5, c)
    elif tipo == "SelectorDeFuente":
        elevado(v, x + 2, y + 4, 20, 16, BLANCO)
        v.texto(x + 5, y + 4, "Aa", NEGRO)
    elif tipo == "ComboFuentes":
        hundido(v, x + 1, y + 6, 22, 13, BLANCO)
        v.texto(x + 3, y + 3, "A", NEGRO)
        elevado(v, x + 15, y + 7, 7, 11)
    elif tipo == "Calendario":
        elevado(v, x + 3, y + 3, 18, 18, BLANCO)
        v.rect(x + 4, y + 4, 16, 4, AZUL)
        for fila in range(3):
            for col in range(4):
                v.rect(x + 5 + col * 4, y + 10 + fila * 3, 2, 1, NEGRO)
    elif tipo == "Arbol":
        hundido(v, x + 2, y + 3, 20, 18, BLANCO)
        v.rect(x + 4, y + 5, 5, 5, BLANCO)
        v.rect(x + 4, y + 5, 5, 5, NEGRO, relleno=False)
        v.rect(x + 11, y + 7, 8, 1, NEGRO)
        v.linea(x + 6, y + 10, x + 6, y + 17, SOMBRA)
        v.linea(x + 6, y + 14, x + 10, y + 14, SOMBRA)
        v.linea(x + 6, y + 17, x + 10, y + 17, SOMBRA)
        v.rect(x + 12, y + 13, 7, 1, NEGRO)
        v.rect(x + 12, y + 16, 7, 1, NEGRO)
    elif tipo == "Grafico":
        hundido(v, x + 1, y + 2, 22, 20, BLANCO)
        v.linea(x + 4, y + 19, x + 21, y + 19, NEGRO)
        for i, (a, c) in enumerate(((8, 0x2A6FD8), (6, 0xE5832B), (11, 0x1E9E6B), (4, 0xC8453C))):
            v.rect(x + 5 + i * 4, y + 19 - a, 3, a, c)
    elif tipo == "Notificacion":
        v.rect(x + 2, y + 7, 20, 11, 0x263238)
        v.rect(x + 2, y + 7, 20, 11, NEGRO, relleno=False)
        v.elipse(x + 4, y + 10, 5, 5, 0x3DD17F)
        v.rect(x + 11, y + 11, 8, 1, BLANCO)
        v.rect(x + 11, y + 14, 6, 1, BLANCO)
    elif tipo == "SelectorFecha":
        hundido(v, x + 1, y + 6, 22, 13, BLANCO)
        v.rect(x + 3, y + 11, 6, 1, SOMBRA)
        elevado(v, x + 15, y + 7, 7, 11)
        v.rect(x + 17, y + 9, 3, 2, AZUL)
    elif tipo in ("CampoArchivo", "CampoCarpeta"):
        hundido(v, x + 1, y + 6, 22, 13, BLANCO)
        elevado(v, x + 15, y + 7, 7, 11)
        v.rect(x + 17, y + 12, 1, 1, NEGRO)
        v.rect(x + 19, y + 12, 1, 1, NEGRO)
        v.rect(x + 4, y + 12, 6, 1, SOMBRA)
    elif tipo == "ConectorBD":      # mejora 127: un cilindro de base de datos
        for k in range(3):
            v.elipse(x + 4, y + 4 + k * 5, 16, 7, 0xE8E8A0)
            v.elipse(x + 4, y + 4 + k * 5, 16, 7, NEGRO, relleno=False)
        v.rect(x + 4, y + 8, 16, 10, 0xE8E8A0)
        v.linea(x + 4, y + 8, x + 4, y + 18, NEGRO)
        v.linea(x + 19, y + 8, x + 19, y + 18, NEGRO)
        v.elipse(x + 4, y + 4, 16, 7, 0xF4F4C0)
        v.elipse(x + 4, y + 4, 16, 7, NEGRO, relleno=False)
        v.elipse(x + 4, y + 15, 16, 7, 0xE8E8A0)
        v.elipse(x + 4, y + 15, 16, 7, NEGRO, relleno=False)
    elif tipo == "RejillaDatos":    # mejora 127: una rejilla con cabecera
        hundido(v, x + 2, y + 4, 20, 16)
        v.rect(x + 3, y + 5, 18, 4, GRIS)
        for k in range(3):
            v.linea(x + 3, y + 12 + k * 3, x + 20, y + 12 + k * 3, SOMBRA)
        v.linea(x + 10, y + 5, x + 10, y + 19, SOMBRA)
        v.linea(x + 16, y + 5, x + 16, y + 19, SOMBRA)
    elif tipo == "Menu":
        v.rect(x + 2, y + 4, 20, 4, AZUL)
        elevado(v, x + 5, y + 8, 14, 11, BLANCO)
        v.rect(x + 7, y + 11, 10, 1, NEGRO)
        v.rect(x + 7, y + 14, 10, 1, NEGRO)
    elif tipo == "Pestanas":
        v.rect(x + 2, y + 9, 20, 11, GRIS)
        v.rect(x + 2, y + 9, 20, 11, SOMBRA, relleno=False)
        elevado(v, x + 3, y + 5, 9, 6)
        v.rect(x + 12, y + 7, 8, 3, SOMBRA)
        v.linea(x + 4, y + 12, x + 19, y + 12, BLANCO)
    elif tipo == "Tabla":
        hundido(v, x + 2, y + 4, 20, 16)
        v.rect(x + 3, y + 5, 18, 4, 0xE0E0E0)
        v.linea(x + 3, y + 12, x + 20, y + 12, SOMBRA)
        v.linea(x + 3, y + 16, x + 20, y + 16, SOMBRA)
        v.linea(x + 9, y + 5, x + 9, y + 19, SOMBRA)
        v.linea(x + 15, y + 5, x + 15, y + 19, SOMBRA)
    elif tipo == "Numerico":
        hundido(v, x + 2, y + 6, 20, 13)
        v.texto(x + 4, y + 4, "7", NEGRO)
        elevado(v, x + 15, y + 7, 6, 5)
        elevado(v, x + 15, y + 12, 6, 6)
        v.poligono([(x + 16, y + 10), (x + 20, y + 10), (x + 18, y + 8)], NEGRO)
        v.poligono([(x + 16, y + 14), (x + 20, y + 14), (x + 18, y + 16)], NEGRO)
    elif tipo == "Lienzo":
        hundido(v, x + 2, y + 3, 20, 18)
        v.linea(x + 5, y + 17, x + 11, y + 8, ROJO_I)
        v.linea(x + 11, y + 8, x + 18, y + 15, ROJO_I)
        v.elipse(x + 13, y + 6, 6, 6, AZUL, relleno=False)
    elif tipo == "Enlace":
        v.texto(x + 3, y + 3, "Ab", 0x0000EE)
        v.linea(x + 3, y + 18, x + 20, y + 18, 0x0000EE)
        v.linea(x + 3, y + 19, x + 20, y + 19, 0x0000EE)
    elif tipo == "BarraEstado":
        v.rect(x + 2, y + 14, 20, 7, GRIS)
        hundido(v, x + 3, y + 15, 11, 5, None)
        hundido(v, x + 15, y + 15, 6, 5, None)
    elif tipo == "SelectorColor":
        elevado(v, x + 2, y + 6, 20, 13)
        v.rect(x + 5, y + 9, 6, 7, 0xFF0000)
        v.rect(x + 11, y + 9, 5, 7, 0x00A000)
        v.rect(x + 16, y + 9, 3, 7, 0x0000FF)
    elif tipo == "Reloj":
        v.rect(x + 3, y + 4, 18, 16, BLANCO)
        v.rect(x + 3, y + 4, 18, 16, NEGRO, relleno=False)
        v.rect(x + 3, y + 4, 18, 4, 0xB22222)
        v.texto(x + 6, y + 6, "15", NEGRO)
    elif tipo == "Archivo":
        v.poligono([(x + 6, y + 3), (x + 15, y + 3), (x + 19, y + 7), (x + 19, y + 21), (x + 6, y + 21)], BLANCO)
        v.linea(x + 6, y + 3, x + 6, y + 21, NEGRO)
        v.linea(x + 6, y + 21, x + 19, y + 21, NEGRO)
        v.linea(x + 19, y + 7, x + 19, y + 21, NEGRO)
        v.linea(x + 15, y + 3, x + 19, y + 7, NEGRO)
        for k in range(3):
            v.rect(x + 9, y + 10 + k * 3, 7, 1, SOMBRA)


# ------------------------------------------------------------ pintar un control del formulario

def pintar_grafico(v, c, x, y, w, h):
    """Vista previa del gráfico en el diseñador: ejes y unas barras, líneas o una tarta (no es el dibujo real)."""
    tipo = str(M.val(c, "tipo") or "barras")
    v.rect(x, y, w, h, BLANCO)
    v.rect(x, y, w, h, SOMBRA, relleno=False)
    titulo = str(M.val(c, "titulo") or "")
    if titulo:
        v.texto(x + max(2, (w - len(titulo) * 8) // 2), y + 3, titulo[:max(1, (w - 4) // 8)], NEGRO)
    ay = y + (22 if titulo else 8)
    ah = h - (ay - y) - 10
    if ah < 16 or w < 40:
        return
    cols = (0x2A6FD8, 0xE5832B, 0x1E9E6B, 0xC8453C)
    if tipo in ("tarta", "anillo"):
        d = min(w - 20, ah)
        cx, cy = x + w // 2 - d // 2, ay + (ah - d) // 2
        v.elipse(cx, cy, d, d, cols[0])
        v.poligono([(cx + d // 2, cy + d // 2), (cx + d, cy + d // 2), (cx + d // 2 + d // 3, cy + d // 2 + d // 3), (cx + d // 2, cy + d)], cols[1])
        v.poligono([(cx + d // 2, cy + d // 2), (cx + d // 2, cy + d), (cx, cy + d // 2)], cols[2])
        if tipo == "anillo":
            v.elipse(cx + d // 4, cy + d // 4, d // 2, d // 2, BLANCO)
        return
    v.linea(x + 8, ay, x + 8, ay + ah, SOMBRA)
    v.linea(x + 8, ay + ah, x + w - 8, ay + ah, SOMBRA)
    n = 5
    paso = max(4, (w - 24) // n)
    alturas = (0.45, 0.8, 0.6, 0.95, 0.7)
    if tipo in ("lineas", "area", "dispersion", "velas"):
        pts = [(x + 14 + i * paso, ay + ah - int(ah * alturas[i])) for i in range(n)]
        for i in range(n - 1):
            v.linea(pts[i][0], pts[i][1], pts[i + 1][0], pts[i + 1][1], cols[0], 2)
        for px, py in pts:
            v.elipse(px - 2, py - 2, 5, 5, cols[0])
    elif tipo == "barras_h":
        for i in range(4):
            v.rect(x + 9, ay + 4 + i * (ah // 4), int((w - 30) * alturas[i]), max(3, ah // 4 - 5), cols[i % 4])
    else:
        bw = max(3, paso - 6)
        for i in range(n):
            hh = int(ah * alturas[i])
            v.rect(x + 12 + i * paso, ay + ah - hh, bw, hh, cols[i % 4])


def pintar_control(v, c, x, y, w, h, form):
    """Dibuja un control como lo verá el programa (aproximado), con su rincón en (x, y)."""
    tipo = c["tipo"]
    col = a_int(M.val(c, "color"), NEGRO)
    fondo_form = a_int(M.val(form, "color", "Formulario"), GRIS)
    fondo = a_int(M.val(c, "colorfondo"), None) if M.val(c, "colorfondo") is not None else None
    texto = str(M.val(c, "texto") or "")
    ant = recortar(v, x, y, w, h)
    if tipo == "Etiqueta":
        if fondo is not None:
            v.rect(x, y, w, h, fondo)
        al = M.val(c, "alineacion")
        tw = len(texto) * 8
        tx = x if al == "izquierda" else (x + (w - tw) // 2 if al == "centro" else x + w - tw)
        v.texto(tx, y + max(0, (h - 16) // 2), texto, col)
    elif tipo == "Boton":
        elevado(v, x, y, w, h, fondo if fondo is not None else GRIS)
        centrado(v, x, y, w, h, texto, col)
    elif tipo == "CuadroTexto":
        hundido(v, x, y, w, h, fondo if fondo is not None else BLANCO)
        if M.val(c, "clave"):
            texto = "*" * len(texto)
        v.texto(x + 4, y + 3 if M.val(c, "multilinea") else y + max(0, (h - 16) // 2), texto.split("\n")[0], col)
    elif tipo == "Casilla":
        if fondo is not None:
            v.rect(x, y, w, h, fondo)
        by = y + (h - 13) // 2
        hundido(v, x, by, 13, 13)
        if M.val(c, "valor"):
            v.linea(x + 3, by + 6, x + 5, by + 9, NEGRO)
            v.linea(x + 5, by + 9, x + 10, by + 3, NEGRO)
        v.texto(x + 20, y + max(0, (h - 16) // 2), texto, col)
    elif tipo == "Opcion":
        if fondo is not None:
            v.rect(x, y, w, h, fondo)
        by = y + (h - 13) // 2
        v.elipse(x, by, 13, 13, BLANCO)
        v.elipse(x, by, 13, 13, SOMBRA, relleno=False)
        if M.val(c, "valor"):
            v.elipse(x + 4, by + 4, 5, 5, NEGRO)
        v.texto(x + 20, y + max(0, (h - 16) // 2), texto, col)
    elif tipo == "Lista":
        hundido(v, x, y, w, h)
        for i, t in enumerate(M.val(c, "elementos") or []):
            if 3 + i * 16 > h:
                break
            v.texto(x + 4, y + 3 + i * 16, str(t), col)
    elif tipo == "Combo":
        hundido(v, x, y, w, h, fondo if fondo is not None else BLANCO)
        v.texto(x + 4, y + max(0, (h - 16) // 2), texto, col)
        elevado(v, x + w - 18, y + 2, 16, h - 4)
        v.poligono([(x + w - 14, y + h // 2 - 2), (x + w - 6, y + h // 2 - 2), (x + w - 10, y + h // 2 + 2)], NEGRO)
    elif tipo == "Imagen":
        hundido(v, x, y, w, h, 0xE8E8E8)
        v.poligono([(x + 4, y + h - 5), (x + w // 2, y + 8), (x + w - 4, y + h - 5)], 0x30A030)
        v.texto(x + 4, y + 2, str(M.val(c, "archivo") or ""), SOMBRA)
    elif tipo == "Marco":
        v.rect(x, y + 8, w, h - 8, fondo if fondo is not None else fondo_form)
        grabado(v, x, y + 8, w - 1, h - 9)
        if texto:
            v.rect(x + 6, y, len(texto) * 8 + 4, 16, fondo_form)
            v.texto(x + 8, y, texto, col)
    elif tipo == "Deslizador":
        if M.val(c, "vertical"):
            hundido(v, x + w // 2 - 2, y + 4, 4, h - 8, GRIS)
            elevado(v, x + w // 2 - 8, y + h // 2 - 5, 16, 10)
        else:
            hundido(v, x + 4, y + h // 2 - 2, w - 8, 4, GRIS)
            elevado(v, x + w // 2 - 5, y + h // 2 - 8, 10, 16)
    elif tipo == "BarraProgreso":
        hundido(v, x, y, w, h, GRIS)
        n = max(0, (w - 6) // 9)
        mx = int(M.val(c, "maximo") or 100) or 100
        n = n * max(0, min(int(M.val(c, "valor") or 0), mx)) // mx
        for i in range(n):
            v.rect(x + 3 + i * 9, y + 3, 7, h - 6, AZUL)
    elif tipo == "Grafico":
        pintar_grafico(v, c, x, y, w, h)
    elif tipo not in M.SIN_VENTANA:
        hundido(v, x, y, w, h, 0xF4F4F0)
        v.texto(x + 4, y + 2, tipo, NEGRO)
    elif tipo in M.SIN_VENTANA:
        elevado(v, x, y, 32, 32)
        icono(v, tipo, x + 4, y + 4)
    v.recorte(*ant)


class Vista:
    """Diseño fiel (mejora 124, D299): el diseñador dibuja cada control con el MISMO código que
    la aplicación al ejecutarse. Para eso crea los controles reales de `vps` en un formulario
    oculto y llama a su `_desenhar` sobre la superficie, con el rectángulo del diseño. Si algo
    falla con un control, se cae al dibujo aproximado de `pintar_control`."""

    def __init__(self):
        self.form = None
        self.ctl = {}
        self.color = None
        self.vistos = set()
        self.fallos = 0

    def _asegurar(self, form_modelo):
        import vps
        if self.form is None or self.form.win._destruido:
            f = vps.Formulario(nombre="__vista__", ancho=1000, alto=700)
            if f in vps._formularios:
                vps._formularios.remove(f)      # no se cierra con vps.salir()
            f.win.withdraw()
            self.form = f
            self.ctl = {}
            self.color = None
        col = M.val(form_modelo, "color", "Formulario")
        if col != self.color:
            self.form._escribir("color", col)
            self.color = col
        return self.form

    def pintar(self, v, c, x, y, w, h, form_modelo):
        """True si lo dibujó el control real."""
        import vps
        tipo = c["tipo"]
        if tipo in M.SIN_VENTANA:
            return False
        try:
            f = self._asegurar(form_modelo)
            props = dict((k, val) for k, val in c["props"].items() if k not in ("x", "y", "ancho", "alto"))
            clave = tipo + repr(sorted(props.items(), key=lambda kv: kv[0]))
            nombre = "%s.%s" % (form_modelo["nombre"], c["nombre"])
            self.vistos.add(nombre)
            ent = self.ctl.get(nombre)
            if ent is None or ent[0] != clave:
                if ent is not None:
                    ent[1].destruir()
                ctl = vps.TIPOS[tipo](f, nombre=nombre, **props)
                self.ctl[nombre] = ent = (clave, ctl)
            wd = ent[1].w
            wd._ax, wd._ay, wd._wl, wd._wa = x, y, w, h
            ant = recortar(v, x, y, w, h)
            try:
                wd._desenhar(v)
            finally:
                v.recorte(*ant)
            return True
        except Exception as e:
            self.fallos += 1
            if self.fallos < 3:
                import sys
                sys.print_exception(e)
            return False

    def podar(self):
        """Quita los controles de la vista que ya no están en el modelo."""
        for n in list(self.ctl.keys()):
            if n not in self.vistos:
                try:
                    self.ctl[n][1].destruir()
                except Exception:
                    pass
                del self.ctl[n]
        self.vistos = set()


def etiqueta_nombre(v, x, y, nombre):
    """El nombre de un componente sin ventana (Temporizador, Menu) bajo su icono."""
    v.texto(x, y + 34, nombre, NEGRO)


# ------------------------------------------------------------ la caja de herramientas

class Caja(tk.Widget):
    _classe = "Caja"
    _foca_no_clique = False
    _padrao = {"width": 104, "height": 300, "takefocus": 0, "cursor": "", "highlightthickness": 0,
               "borderwidth": 0}
    COL, FIL, TAM = 3, 8, 32
    X0, Y0 = 4, 22

    def __init__(self, master, dis):
        tk.Widget.__init__(self, master)
        self.dis = dis
        self.bind("<Button-1>", self._clic)
        self.bind("<Motion>", self._mover)

    def _natural(self):
        return (self.X0 * 2 + self.COL * self.TAM, self.Y0 + ((len(HERRAMIENTAS) + self.COL - 1) // self.COL) * self.TAM + 8)

    def _caja(self, i):
        return (self.X0 + (i % self.COL) * self.TAM, self.Y0 + (i // self.COL) * self.TAM)

    def centro(self, tipo):
        """El centro del botón de una herramienta, en coordenadas de la ventana."""
        i = HERRAMIENTAS.index(tipo)
        x, y = self._caja(i)
        return self._ax + x + self.TAM // 2, self._ay + y + self.TAM // 2

    def _indice(self, ev):
        for i in range(len(HERRAMIENTAS)):
            x, y = self._caja(i)
            if x <= ev.x < x + self.TAM and y <= ev.y < y + self.TAM:
                return i
        return -1

    def _clic(self, ev):
        i = self._indice(ev)
        if i >= 0:
            self.dis.elegir_herramienta(HERRAMIENTAS[i])

    def _mover(self, ev):
        i = self._indice(ev)
        self.dis.estado(NOMBRES_HERR.get(HERRAMIENTAS[i], HERRAMIENTAS[i]) if i >= 0 else "")

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        v.rect(x, y, w, h, GRIS)
        v.rect(x, y, w, 18, AZUL)
        v.texto(x + 4, y + 1, "General", BLANCO)
        for i, t in enumerate(HERRAMIENTAS):
            bx, by = self._caja(i)
            bx += x
            by += y
            if t == self.dis.herramienta:
                hundido(v, bx, by, self.TAM, self.TAM, 0xE0E0E0)
                icono(v, t, bx + 5, by + 5)
            else:
                elevado(v, bx, by, self.TAM, self.TAM)
                icono(v, t, bx + 4, by + 4)


# ------------------------------------------------------------ la ventana de propiedades

class Propiedades(tk.Widget):
    _classe = "Propiedades"
    _foca_no_clique = False
    _padrao = {"width": 220, "height": 300, "takefocus": 0, "cursor": "", "highlightthickness": 0,
               "borderwidth": 0}
    ALTO_FILA, COL_NOMBRE = 18, 104

    def __init__(self, master, dis):
        tk.Widget.__init__(self, master)
        self.dis = dis
        self.filas = []        # [(nombre, tipo, valor)]
        self.sel = -1
        self.arriba = 0
        self.bind("<Button-1>", self._clic)
        self.bind("<MouseWheel>", self._rueda)

    def _natural(self):
        return (_int(self._op.get("width", 220)), _int(self._op.get("height", 300)))

    def poner(self, filas):
        """Cambia las filas (conserva la propiedad elegida si sigue existiendo)."""
        nombre = self.filas[self.sel][0] if 0 <= self.sel < len(self.filas) else None
        self.filas = filas
        self.sel = -1
        for i, f in enumerate(filas):
            if f[0] == nombre:
                self.sel = i
        self._redesenhar()

    def visibles(self):
        return max(1, (self._wa - 2) // self.ALTO_FILA)

    def _rueda(self, ev):
        self.arriba = max(0, min(self.arriba + (3 if ev.delta < 0 else -3), max(0, len(self.filas) - self.visibles())))
        self._redesenhar()

    def fila_centro(self, nombre):
        """El centro de la fila de una propiedad, en coordenadas de la ventana (para las pruebas)."""
        for i, f in enumerate(self.filas):
            if f[0] == nombre:
                return self._ax + self.COL_NOMBRE + 40, self._ay + 1 + (i - self.arriba) * self.ALTO_FILA + 9
        return None

    def _clic(self, ev):
        i = self.arriba + (ev.y - 1) // self.ALTO_FILA
        if 0 <= i < len(self.filas):
            self.sel = i
            self._redesenhar()
            self.dis.fila_elegida(self.filas[i], self._ax + ev.x, self._ay + ev.y)

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        hundido(v, x, y, w, h)
        ant = recortar(v, x + 2, y + 2, w - 4, h - 4)
        for k in range(self.visibles()):
            i = self.arriba + k
            if i >= len(self.filas):
                break
            nombre, tipo, valor = self.filas[i]
            fy = y + 1 + k * self.ALTO_FILA
            elegido = i == self.sel
            if elegido:
                v.rect(x + 2, fy, w - 4, self.ALTO_FILA, AZUL)
            v.rect(x + 2, fy, self.COL_NOMBRE - 2, self.ALTO_FILA, AZUL if elegido else GRIS)
            v.texto(x + 4, fy + 1, nombre if len(nombre) <= 12 else nombre[:11] + "~", BLANCO if elegido else NEGRO)
            v.linea(x + self.COL_NOMBRE, fy, x + self.COL_NOMBRE, fy + self.ALTO_FILA - 1, SOMBRA)
            v.linea(x + 2, fy + self.ALTO_FILA - 1, x + w - 3, fy + self.ALTO_FILA - 1, LUZ)
            cab = (w - self.COL_NOMBRE - 6) // 8
            t = valor if len(valor) <= cab else valor[:max(0, cab - 1)] + "~"
            v.texto(x + self.COL_NOMBRE + 3, fy + 1, t, BLANCO if elegido else NEGRO)
        v.recorte(*ant)


def vps_tab_h():
    import vps
    return vps.TAB_H


def _int(v, d=0):
    try:
        return int(v)
    except (TypeError, ValueError):
        return d


# ------------------------------------------------------------ la superficie del formulario

class Superficie(tk.Widget):
    """El formulario que se está diseñando: rejilla de puntos, controles, tiradores, selección."""
    _classe = "Superficie"
    _foca_no_clique = True
    _padrao = {"width": 560, "height": 460, "takefocus": 1, "cursor": "", "highlightthickness": 0,
               "borderwidth": 0}
    # tiradores: nw n ne e se s sw w
    TIR = ((0, 0), (1, 0), (2, 0), (2, 1), (2, 2), (1, 2), (0, 2), (0, 1))
    TIR_L = 6

    def __init__(self, master, dis):
        tk.Widget.__init__(self, master)
        self.dis = dis
        self.modo = None
        self.p0 = (0, 0)
        self.p1 = (0, 0)
        self.orig = []
        self.tir = -1
        self._rejilla = None
        self._rejilla_clave = None
        self.vista = Vista()
        self.fiel = True        # diseño fiel: los controles reales; False, el dibujo aproximado
        self.bind("<Button-1>", self._pulsar)
        self.bind("<B1-Motion>", self._arrastrar)
        self.bind("<ButtonRelease-1>", self._soltar)
        self.bind("<Motion>", self._mover)
        for k, f in (("<Delete>", self.dis.borrar_seleccion), ("<BackSpace>", self.dis.borrar_seleccion),
                     ("<Control-c>", self.dis.copiar), ("<Control-v>", self.dis.pegar),
                     ("<Control-x>", self.dis.cortar), ("<Control-a>", self.dis.seleccionar_todo),
                     ("<Escape>", self.dis.cancelar)):
            self.bind(k, f)
        for k, dx, dy in (("<Left>", -1, 0), ("<Right>", 1, 0), ("<Up>", 0, -1), ("<Down>", 0, 1)):
            self.bind(k, lambda e, dx=dx, dy=dy: self.dis.empujar(dx, dy, e.state & 4 != 0))

    def _natural(self):
        return (_int(self._op.get("width", 560)), _int(self._op.get("height", 460)))

    # --- geometría
    def form(self):
        return self.dis.form

    def tam_form(self):
        f = self.form()
        return int(M.val(f, "ancho", "Formulario")), int(M.val(f, "alto", "Formulario"))

    def a_cliente(self, ev):
        return ev.x - FX, ev.y - FY

    def centro_control(self, c):
        """El centro de un control en coordenadas de la ventana (para las pruebas)."""
        x, y, w, h = M.rect_abs(self.form(), c)
        return self._ax + FX + x + w // 2, self._ay + FY + y + h // 2

    def origen_cliente(self):
        return self._ax + FX, self._ay + FY

    def _snap(self, n):
        return (n // M.GRID) * M.GRID

    def _snap_cerca(self, n):
        return ((n + M.GRID // 2) // M.GRID) * M.GRID

    def control_en(self, fx, fy):
        f = self.form()
        for c in reversed(f["controles"]):
            if c["tipo"] in M.SIN_VENTANA:
                x, y, w, h = M.rect_abs(f, c)
                w = h = 32
            else:
                x, y, w, h = M.rect_abs(f, c)
            if M.val(c, "visible") is False and not self.dis.mostrar_ocultos:
                continue
            if not M.en_pagina(f, c):
                continue
            if x <= fx < x + w and y <= fy < y + h:
                return c
        return None

    def marco_en(self, fx, fy):
        f = self.form()
        for c in reversed(f["controles"]):
            if M.es_contenedor(c) and M.en_pagina(f, c):
                x, y, w, h = M.rect_abs(f, c)
                if c["tipo"] == "Pestanas":
                    y, h = y + vps_tab_h(), h - vps_tab_h()     # solo el cuerpo, no la tira de pestañas
                if x <= fx < x + w and y <= fy < y + h:
                    return c
        return None

    def tirador_en(self, fx, fy):
        sel = self.dis.sel
        if len(sel) != 1:
            return -1
        c = sel[0]
        x, y, w, h = M.rect_abs(self.form(), c)
        if c["tipo"] in M.SIN_VENTANA:
            return -1
        for i, (gx, gy) in enumerate(self.TIR):
            tx = x + gx * (w // 2) - self.TIR_L // 2
            ty = y + gy * (h // 2) - self.TIR_L // 2
            if tx - 1 <= fx < tx + self.TIR_L + 1 and ty - 1 <= fy < ty + self.TIR_L + 1:
                return i
        return -1

    # --- ratón
    def _pulsar(self, ev):
        self.focus_set()
        fx, fy = self.a_cliente(ev)
        dis = self.dis
        dis.cerrar_edicion()
        duplo = getattr(ev, "_duplo", False)
        self.p0 = self.p1 = (fx, fy)
        if dis.herramienta != "Puntero":
            self.modo = "crear"
            self.p0 = self.p1 = (self._snap(fx), self._snap(fy))
            return
        t = self.tirador_en(fx, fy)
        if t >= 0:
            self.modo = "redim"
            dis.punto()
            self.tir = t
            self.orig = [(dis.sel[0], M.rect_abs(self.form(), dis.sel[0])[2:], (int(M.val(dis.sel[0], "x")), int(M.val(dis.sel[0], "y"))))]
            return
        c = self.control_en(fx, fy)
        if c is not None:
            if c["tipo"] == "Pestanas":
                self._clic_pestana(c, fx, fy)
            multi = bool(ev.state & 1) or bool(ev.state & 4)
            if multi:
                dis.alternar(c)
            elif not any(c is s for s in dis.sel):
                dis.seleccionar([c])
            if duplo:
                dis.codigo_de(c)
                self.modo = None
                return
            self.modo = "mover"
            dis.punto()
            self.orig = [(s, (int(M.val(s, "x")), int(M.val(s, "y")))) for s in dis.sel]
            return
        # vacío: el formulario (o la marquesina)
        dentro = 0 <= fx < self.tam_form()[0] and 0 <= fy < self.tam_form()[1]
        dis.seleccionar([])
        if duplo and dentro:
            dis.codigo_de(None)
            self.modo = None
            return
        self.modo = "marq" if dentro else None

    def _clic_pestana(self, c, fx, fy):
        """Un clic en la tira de pestañas de unas Pestanas elige esa página (para colocar controles en ella)."""
        import vps
        x, y, w, h = M.rect_abs(self.form(), c)
        if not (y <= fy < y + vps.TAB_H):
            return
        for i, (a, b) in enumerate(vps._pest_tramos(M.val(c, "elementos") or [])):
            if x + a <= fx < x + b and i != int(M.val(c, "seleccion") or 0):
                self.dis.punto()
                c["props"]["seleccion"] = i
                self.dis.cambio(True)
                return

    def _arrastrar(self, ev):
        if self.modo is None:
            return
        fx, fy = self.a_cliente(ev)
        self.p1 = (fx, fy)
        dis = self.dis
        if self.modo == "mover":
            dx = self._snap_cerca(fx - self.p0[0])
            dy = self._snap_cerca(fy - self.p0[1])
            for s, (ox, oy) in self.orig:
                s["props"]["x"] = max(0, ox + dx)
                s["props"]["y"] = max(0, oy + dy)
            dis.cambio(False)
        elif self.modo == "redim":
            c, (w0, h0), (x0, y0) = self.orig[0]
            gx, gy = self.TIR[self.tir]
            dx = self._snap_cerca(fx - self.p0[0])
            dy = self._snap_cerca(fy - self.p0[1])
            x, y, w, h = x0, y0, w0, h0
            if gx == 0:
                x, w = x0 + dx, w0 - dx
            elif gx == 2:
                w = w0 + dx
            if gy == 0:
                y, h = y0 + dy, h0 - dy
            elif gy == 2:
                h = h0 + dy
            if w < 8:
                x, w = (x0 + w0 - 8 if gx == 0 else x0), 8
            if h < 8:
                y, h = (y0 + h0 - 8 if gy == 0 else y0), 8
            c["props"].update({"x": x, "y": y, "ancho": w, "alto": h})
            dis.cambio(False)
        else:
            self._redesenhar()

    def _soltar(self, ev):
        modo, self.modo = self.modo, None
        if modo is None:
            return
        fx, fy = self.a_cliente(ev)
        dis = self.dis
        if modo == "crear":
            x0, y0 = self.p0
            x1, y1 = self._snap_cerca(fx), self._snap_cerca(fy)
            marco = self.marco_en(x0, y0)
            ox = oy = 0
            if marco is not None:
                ox, oy = M.origen_hijos(self.form(), marco)
            w, h = abs(x1 - x0), abs(y1 - y0)
            if w < 8 or h < 8:
                w = h = 0
            dis.crear_control(dis.herramienta, min(x0, x1) - ox, min(y0, y1) - oy,
                              w or None, h or None, marco["nombre"] if marco else None)
        elif modo == "marq":
            x0, y0 = self.p0
            x1, y1 = fx, fy
            if abs(x1 - x0) > 3 or abs(y1 - y0) > 3:
                a, b = min(x0, x1), min(y0, y1)
                z, d = max(x0, x1), max(y0, y1)
                r = []
                for c in self.form()["controles"]:
                    x, y, w, h = M.rect_abs(self.form(), c)
                    if c["tipo"] in M.SIN_VENTANA:
                        w = h = 32
                    if x >= a and y >= b and x + w <= z and y + h <= d:
                        r.append(c)
                dis.seleccionar(r)
            self._redesenhar()
        else:
            dis.descartar_si_igual()
            dis.cambio(True)
        self._redesenhar()

    def _mover(self, ev):
        fx, fy = self.a_cliente(ev)
        self.dis.estado("%d, %d" % (fx, fy) if fx >= 0 and fy >= 0 else "")

    # --- dibujo
    def _rejilla_img(self, w, h, color):
        clave = (w, h, color)
        if self._rejilla_clave != clave:
            img = sagui.Imagen.nueva(w, h, color)
            mv = memoryview(img)
            for yy in range(0, h, M.GRID):
                base = yy * w
                for xx in range(0, w, M.GRID):
                    mv[base + xx] = 0x000000
            self._rejilla = img
            self._rejilla_clave = clave
        return self._rejilla

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        dis = self.dis
        form = dis.form
        v.rect(x, y, w, h, 0x808080)
        ant = recortar(v, x, y, w, h)
        fw, fh = self.tam_form()
        fw, fh = max(16, min(fw, w - FX - 4)), max(16, min(fh, h - FY - 4))
        # ventana del formulario
        elevado(v, x + OX, y + OY, fw + 8, fh + TB + 6)
        v.rect(x + OX + 3, y + OY + 3, fw + 2, TB - 6, AZUL)
        v.texto(x + OX + 6, y + OY + 3, str(M.val(form, "texto", "Formulario"))[:max(1, (fw - 30) // 8)], BLANCO)
        elevado(v, x + OX + fw - 12, y + OY + 5, 14, 12)
        v.texto(x + OX + fw - 10, y + OY + 3, "x", NEGRO)
        # cliente con rejilla
        color = a_int(M.val(form, "color", "Formulario"), GRIS)
        v.imagen(x + FX, y + FY, self._rejilla_img(fw, fh, color))
        # controles
        ant2 = recortar(v, x + FX, y + FY, fw, fh)
        for c in form["controles"]:
            rx, ry, rw, rh = M.rect_abs(form, c)
            if c["tipo"] in M.SIN_VENTANA:
                rw = rh = 32
            if M.val(c, "visible") is False or not M.en_pagina(form, c):
                continue
            if not (self.fiel and self.vista.pintar(v, c, x + FX + rx, y + FY + ry, rw, rh, form)):
                pintar_control(v, c, x + FX + rx, y + FY + ry, rw, rh, form)
            if c["tipo"] in M.SIN_VENTANA:
                etiqueta_nombre(v, x + FX + rx, y + FY + ry, c["nombre"])
        v.recorte(*ant2)
        self.vista.podar()
        # selección
        sel = dis.sel
        for c in sel:
            rx, ry, rw, rh = M.rect_abs(form, c)
            if c["tipo"] in M.SIN_VENTANA:
                rw = rh = 32
            bx, by = x + FX + rx, y + FY + ry
            col = NEGRO if len(sel) == 1 else SOMBRA
            if len(sel) == 1 and c["tipo"] not in M.SIN_VENTANA:
                for gx, gy in self.TIR:
                    tx = bx + gx * (rw // 2) - self.TIR_L // 2
                    ty = by + gy * (rh // 2) - self.TIR_L // 2
                    v.rect(tx, ty, self.TIR_L, self.TIR_L, col)
            else:
                v.rect(bx - 2, by - 2, 5, 5, col)
                v.rect(bx + rw - 3, by - 2, 5, 5, col)
                v.rect(bx - 2, by + rh - 3, 5, 5, col)
                v.rect(bx + rw - 3, by + rh - 3, 5, 5, col)
        # rectángulo elástico (crear o marquesina)
        if self.modo in ("crear", "marq"):
            x0, y0 = self.p0
            x1, y1 = self.p1
            if self.modo == "crear":
                x1, y1 = self._snap_cerca(x1), self._snap_cerca(y1)
            v.rect(x + FX + min(x0, x1), y + FY + min(y0, y1), abs(x1 - x0), abs(y1 - y0), NEGRO, relleno=False)
        v.recorte(*ant)
