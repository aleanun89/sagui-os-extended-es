# SPDX-License-Identifier: GPL-2.0-or-later
# sagui_ventana.py - ventanas, dibujo y eventos con el módulo sagui.
# Ejecuta con: python /ejemplos/sagui_ventana.py
#
# Haz clic para dejar círculos, arrastra para dibujar, la rueda cambia el
# color, el botón derecho abre un menú y las flechas mueven el cuadrado.
# La ventana duerme mientras no pasa nada: el reloj se redibuja una vez por
# segundo con evento(timeout).
import sagui
import time

COLORES = [0xD83A30, 0x0B7A3E, 0x2A6FD8, 0xD9A400, 0x7A3EB0]
v = sagui.Ventana(480, 340, "sagui: ventana de ejemplo")
color = 0
cx, cy = 220, 150
ultimo = None


def fondo():
    an, al = v.tamano()
    v.limpiar()
    v.rect(0, 0, an, 28, 0x0B7A3E)
    v.texto(8, 6, "Módulo sagui: clic, arrastrar, rueda, flechas", 0xFFFFFF)
    for i, c in enumerate(COLORES):
        v.rect(8 + i * 26, 36, 22, 22, c)
        if i == color:
            v.rect(6 + i * 26, 34, 26, 26, 0x1E2328, relleno=False, grosor=2)
    v.elipse(an - 90, 40, 70, 50, 0xFFD52E)
    v.elipse(an - 90, 40, 70, 50, 0x1E2328, relleno=False)
    v.poligono([(an - 160, 90), (an - 130, 40), (an - 100, 90)], 0x2A6FD8)
    cuadrado()


def cuadrado():
    v.rect(cx, cy, 30, 30, COLORES[color])


def reloj():
    an, al = v.tamano()
    t = time.localtime()
    v.texto(8, al - 22, "%02d:%02d:%02d   ticks %d" % (t[3], t[4], t[5], sagui.ticks_ms()), 0x1E2328, 0xF4F4F0)


fondo()
reloj()
v.actualizar()
proximo = sagui.ticks_ms() + 1000
while True:
    e = v.evento(max(0, proximo - sagui.ticks_ms()))
    if e is None:                      # pasó un segundo sin eventos
        reloj()
        proximo += 1000
    elif e.tipo == sagui.CERRAR:
        break
    elif e.tipo == sagui.REDIMENSIONAR:
        fondo()
        reloj()
    elif e.tipo == sagui.CLIC and e.botones == sagui.BOTON_DERECHO:
        r = v.menu_contextual(e.x, e.y, ["Limpiar", "-", ("Color", ["Rojo", "Verde", "Azul"]), "Salir"])
        if r == 0:
            fondo()
        elif r in (3, 4, 5):
            color = r - 3
            fondo()
        elif r == 7:
            break
    elif e.tipo == sagui.CLIC:
        if 36 <= e.y < 58 and e.x < 8 + 26 * len(COLORES):
            color = (e.x - 8) // 26
            fondo()
        else:
            v.elipse(e.x - 8, e.y - 8, 16, 16, COLORES[color])
            ultimo = (e.x, e.y)
    elif e.tipo == sagui.MOVER and e.botones and ultimo:
        v.linea(ultimo[0], ultimo[1], e.x, e.y, COLORES[color], 3)
        ultimo = (e.x, e.y)
    elif e.tipo == sagui.SOLTAR:
        ultimo = None
    elif e.tipo == sagui.RUEDA:
        color = (color + (1 if e.cliques > 0 else -1)) % len(COLORES)
        fondo()
    elif e.tipo == sagui.TECLA:
        paso = {sagui.TECLA_IZQUIERDA: (-10, 0), sagui.TECLA_DERECHA: (10, 0),
                sagui.TECLA_ARRIBA: (0, -10), sagui.TECLA_ABAJO: (0, 10)}.get(e.tecla)
        if paso:
            v.rect(cx, cy, 30, 30, sagui.COLOR_FONDO)
            cx += paso[0]
            cy += paso[1]
            cuadrado()
        elif e.caracter == "q":
            break
    v.actualizar()
v.cerrar()
