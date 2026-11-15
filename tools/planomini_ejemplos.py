#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# PlanoMini, © 2026 Alejandro Anuncibay (prototipo original); versión nativa de SaguiOS.
"""Genera los ejemplos de PlanoMini (D312) en rootfs/ejemplos/planomini/: una planta con cotas, una pieza
mecánica con empalmes y un DXF (R2000 con LWPOLYLINE, capas y una DIMENSION con su bloque) creado a mano."""
import math
import os

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = os.path.join(RAIZ, "rootfs/ejemplos/planomini")


def f(v):
    s = ("%.9f" % v).rstrip("0").rstrip(".")
    return "0" if s in ("-0", "") else s


class Dibujo:
    def __init__(self, alt, fl, dec):
        self.cab = ["PLANOMINI 2", "ESTILO %s %s %d" % (f(alt), f(fl), dec)]
        self.capas = []
        self.ents = []

    def capa(self, nombre, color, lt=0):
        self.capas.append('CAPA "%s" %06x %d 1 0' % (nombre, color, lt))
        return len(self.capas)       # la capa 0 ya existe: las propias empiezan en 1

    def linea(self, c, x0, y0, x1, y1, lt=255):
        self.ents.append("L %d -1 %d %s %s %s %s" % (c, lt, f(x0), f(y0), f(x1), f(y1)))

    def circulo(self, c, x, y, r):
        self.ents.append("C %d -1 255 %s %s %s" % (c, f(x), f(y), f(r)))

    def arco(self, c, x, y, r, a0, a1):
        self.ents.append("A %d -1 255 %s %s %s %s %s" % (c, f(x), f(y), f(r), f(math.radians(a0)), f(math.radians(a1))))

    def poli(self, c, vs, cerrada=True, lt=255):
        v = " ".join("%s %s %s" % (f(x), f(y), f(b)) for x, y, b in vs)
        self.ents.append("W %d -1 %d %d %d %s" % (c, lt, 1 if cerrada else 0, len(vs), v))

    def texto(self, c, x, y, alt, giro, t):
        self.ents.append('T %d -1 255 %s %s %s %s "%s"' % (c, f(x), f(y), f(alt), f(math.radians(giro)), t))

    def cota(self, c, sub, alt, p, r=0.0, a0=0.0, txt=""):
        p = list(p) + [0] * (8 - len(p))
        self.ents.append('D %d -1 255 %d %s %s %s %s "%s"' % (c, sub, f(alt), f(r), f(a0), " ".join(f(x) for x in p), txt))

    def sombra(self, c, vs, sub, ang, esp):
        v = " ".join("%s %s %s" % (f(x), f(y), f(b)) for x, y, b in vs)
        self.ents.append("S %d -1 255 %d %s %s %d %s" % (c, sub, f(math.radians(ang)), f(esp), len(vs), v))

    def guardar(self, nombre):
        lineas = self.cab + self.capas + ["ACTUAL 0"] + self.ents
        with open(os.path.join(DEST, nombre), "w", encoding="latin-1", newline="\n") as fh:
            fh.write("\n".join(lineas) + "\n")


def habitacion():
    d = Dibujo(110, 110, 0)
    c0 = 0
    muros = d.capa("Muros", 0xFFFFFF)
    puertas = d.capa("Puertas", 0x00FF00)
    ventanas = d.capa("Ventanas", 0x00FFFF)
    cotas = d.capa("Cotas", 0xFFFF00)
    textos = d.capa("Textos", 0xFF00FF)
    ejes = d.capa("Ejes", 0xFF0000, 3)
    mob = d.capa("Mobiliario", 0x0000FF)
    W, H, E = 4000, 3000, 150
    # muros con huecos: puerta en el muro inferior (600..1500) y ventana en el derecho (1000..2200)
    def tramo(x0, y0, x1, y1):
        d.linea(muros, x0, y0, x1, y1)
    # interior
    tramo(0, 0, 600, 0); tramo(1500, 0, W, 0); tramo(W, 0, W, 1000); tramo(W, 2200, W, H); tramo(W, H, 0, H); tramo(0, H, 0, 0)
    # exterior
    tramo(-E, -E, 600, -E); tramo(1500, -E, W + E, -E); tramo(W + E, -E, W + E, 1000); tramo(W + E, 2200, W + E, H + E); tramo(W + E, H + E, -E, H + E); tramo(-E, H + E, -E, -E)
    # jambas
    tramo(600, -E, 600, 0); tramo(1500, -E, 1500, 0); tramo(W, 1000, W + E, 1000); tramo(W, 2200, W + E, 2200)
    # puerta: hoja y arco de giro
    d.linea(puertas, 600, 0, 600, 900)
    d.arco(puertas, 600, 0, 900, 0, 90)
    # ventana: tres líneas en el hueco
    for x in (W, W + E / 2, W + E):
        d.linea(ventanas, x, 1000, x, 2200)
    # ejes
    d.linea(ejes, -600, H / 2, W + 600, H / 2, 3)
    d.linea(ejes, W / 2, -700, W / 2, H + 700, 3)
    # mobiliario
    d.poli(mob, [(200, 1800, 0), (1800, 1800, 0), (1800, 2800, 0), (200, 2800, 0)])
    d.poli(mob, [(300, 2500, 0), (800, 2500, 0), (800, 2700, 0), (300, 2700, 0)])
    d.circulo(mob, 3000, 1500, 400)
    d.sombra(mob, [(3000 + 400, 1500, 1), (3000 - 400, 1500, 1)], 0, 45, 60)
    # cotas
    d.cota(cotas, 0, 110, (0, 0, W, 0, 0, -700))                    # ancho interior
    d.cota(cotas, 1, 110, (0, 0, 0, H, -700, 1500))                 # alto interior
    d.cota(cotas, 0, 110, (-E, H + E, W + E, H + E, 0, H + 700))   # ancho exterior
    d.cota(cotas, 0, 110, (600, 0, 1500, 0, 0, -350))               # puerta
    d.cota(cotas, 1, 110, (W + E, 1000, W + E, 2200, W + 600, 1600))  # ventana
    d.cota(cotas, 3, 110, (3000, 1500), r=400, a0=math.radians(40))  # radio de la mesa
    d.texto(textos, 1100, 1100, 250, 0, "SALON")
    d.texto(textos, 1100, 800, 130, 0, "12 m2 utiles")
    d.texto(textos, -150, -900, 160, 0, "PLANTA DE UNA HABITACION  1:50")
    d.guardar("habitacion.pmini")


def pieza():
    d = Dibujo(3.5, 3.5, 1)
    c0 = 0
    cont = d.capa("Contorno", 0xFFFFFF)
    ejes = d.capa("Ejes", 0xFF0000, 3)
    agu = d.capa("Agujeros", 0x00FFFF)
    cot = d.capa("Cotas", 0xFFFF00)
    tex = d.capa("Textos", 0xFF00FF)
    b = math.tan(math.pi / 8)
    d.poli(cont, [(10, 0, 0), (110, 0, b), (120, 10, 0), (120, 70, b), (110, 80, 0), (10, 80, b), (0, 70, 0), (0, 10, b)])
    for x, y in ((20, 20), (100, 20), (20, 60), (100, 60)):
        d.circulo(agu, x, y, 5)
    d.circulo(agu, 60, 40, 15)
    d.circulo(agu, 60, 40, 22, )
    d.linea(ejes, -10, 40, 130, 40, 3)
    d.linea(ejes, 60, -10, 60, 90, 3)
    d.cota(cot, 0, 3.5, (0, 0, 120, 0, 0, -14))                      # largo
    d.cota(cot, 1, 3.5, (120, 0, 120, 80, 134, 40))                  # alto
    d.cota(cot, 0, 3.5, (20, 20, 100, 20, 0, -6), txt="<> (entre agujeros)")
    d.cota(cot, 4, 3.5, (60, 40), r=15, a0=math.radians(60))        # diametro del taladro central
    d.cota(cot, 3, 3.5, (10, 10), r=10, a0=math.radians(225))       # radio del empalme
    d.cota(cot, 5, 3.5, (60, 40, 100, 40, 60, 80, 85, 65))           # angulo entre los ejes del taladro y la esquina
    d.texto(tex, 0, -28, 5, 0, "PLACA DE ANCLAJE - empalmes R10")
    d.texto(tex, 0, -36, 3.5, 0, "Cotas en mm")
    d.guardar("pieza.pmini")


def dxf_plano():
    P = []

    def g(code, val):
        P.append("%d\n%s" % (code, val))

    def secc(n):
        g(0, "SECTION"); g(2, n)

    secc("HEADER"); g(9, "$ACADVER"); g(1, "AC1015"); g(0, "ENDSEC")
    secc("TABLES")
    g(0, "TABLE"); g(2, "LAYER"); g(70, 4)
    for nom, aci, lt in (("0", 7, "CONTINUOUS"), ("Parcela", 1, "DASHED"), ("Edificio", 3, "CONTINUOUS"), ("Jardin", 2, "DASHDOT")):
        g(0, "LAYER"); g(2, nom); g(70, 0); g(62, aci); g(6, lt)
    g(0, "ENDTAB"); g(0, "ENDSEC")
    secc("BLOCKS")
    g(0, "BLOCK"); g(8, "0"); g(2, "*D1"); g(70, 1); g(10, 0.0); g(20, 0.0); g(30, 0.0); g(3, "*D1")
    g(0, "LINE"); g(8, "0"); g(10, 0.0); g(20, -3.0); g(30, 0.0); g(11, 30.0); g(21, -3.0); g(31, 0.0)
    g(0, "SOLID"); g(8, "0"); g(10, 0.0); g(20, -3.0); g(30, 0.0); g(11, 1.5); g(21, -2.75); g(31, 0.0); g(12, 1.5); g(22, -3.25); g(32, 0.0); g(13, 1.5); g(23, -3.25); g(33, 0.0)
    g(0, "SOLID"); g(8, "0"); g(10, 30.0); g(20, -3.0); g(30, 0.0); g(11, 28.5); g(21, -2.75); g(31, 0.0); g(12, 28.5); g(22, -3.25); g(32, 0.0); g(13, 28.5); g(23, -3.25); g(33, 0.0)
    g(0, "TEXT"); g(8, "0"); g(10, 12.0); g(20, -2.0); g(30, 0.0); g(40, 1.5); g(1, "30.0")
    g(0, "ENDBLK"); g(8, "0")
    g(0, "ENDSEC")
    secc("ENTITIES")
    # parcela (LWPOLYLINE cerrada, trazos rojos) 30 x 20
    g(0, "LWPOLYLINE"); g(5, "20"); g(8, "Parcela"); g(100, "AcDbEntity"); g(100, "AcDbPolyline"); g(90, 4); g(70, 1)
    for x, y in ((0, 0), (30, 0), (30, 20), (0, 20)):
        g(10, float(x)); g(20, float(y))
    # edificio 10 x 6 con una esquina redondeada (bulge)
    g(0, "LWPOLYLINE"); g(5, "21"); g(8, "Edificio"); g(100, "AcDbEntity"); g(100, "AcDbPolyline"); g(90, 5); g(70, 1)
    for x, y, b in ((5, 4, 0), (15, 4, 0), (15, 8, 0), (13, 10, 0.4142135624), (5, 10, 0)):
        g(10, float(x)); g(20, float(y))
        if b:
            g(42, b)
    # arbol: circulo + arco de seto
    g(0, "CIRCLE"); g(8, "Jardin"); g(10, 24.0); g(20, 14.0); g(30, 0.0); g(40, 2.0)
    g(0, "CIRCLE"); g(8, "Jardin"); g(62, 3); g(10, 24.0); g(20, 14.0); g(30, 0.0); g(40, 0.4)
    g(0, "ARC"); g(8, "Jardin"); g(10, 5.0); g(20, 15.0); g(30, 0.0); g(40, 4.0); g(50, 270.0); g(51, 90.0)
    g(0, "LINE"); g(8, "Jardin"); g(10, 15.0); g(20, 4.0); g(30, 0.0); g(11, 24.0); g(21, 0.0); g(31, 0.0)
    g(0, "POINT"); g(8, "Parcela"); g(10, 0.0); g(20, 0.0); g(30, 0.0)
    g(0, "TEXT"); g(8, "Edificio"); g(10, 6.0); g(20, 6.5); g(30, 0.0); g(40, 1.2); g(1, "EDIFICIO"); g(50, 0.0)
    g(0, "TEXT"); g(8, "Jardin"); g(10, 22.0); g(20, 17.0); g(30, 0.0); g(40, 0.8); g(1, "ARBOL"); g(50, 20.0)
    g(0, "MTEXT"); g(8, "0"); g(10, 0.0); g(20, -3.0); g(30, 0.0); g(40, 1.0); g(1, "Plano de parcela - DXF de ejemplo de PlanoMini (m)")
    g(0, "DIMENSION"); g(8, "0"); g(2, "*D1"); g(10, 15.0); g(20, -3.0); g(30, 0.0); g(70, 1)
    g(0, "ENDSEC"); g(0, "EOF")
    with open(os.path.join(DEST, "plano.dxf"), "w", newline="") as fh:
        fh.write("\r\n".join(P) + "\r\n")


if __name__ == "__main__":
    os.makedirs(DEST, exist_ok=True)
    habitacion()
    pieza()
    dxf_plano()
    print("ejemplos de PlanoMini escritos en", DEST)
