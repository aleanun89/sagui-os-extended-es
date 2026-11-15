#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""sms_rom_teste.py - una ROM de Master System / Game Gear mínima, nuestra, para probar Gearsystem (D321).

No se baja nada ni hay código de terceros: el programa es un Z80 de unas 100 instrucciones escrito con el
ensamblador de unas líneas de este fichero. 64 KB (dos bancos), cabecera «TMR SEGA» en $7FF0 con la suma
de comprobación, y mapper Sega (la RAM del cartucho se enciende con $FFFC = 8).

Qué hace (se ve y se oye, para tests/h321_emuladores.py):
  - pinta la pantalla de 256x192 en cuatro cuadrantes con los cuatro colores de la paleta: arriba a la
    izquierda el color 1 (rojo), arriba a la derecha el 2 (verde), abajo a la izquierda el 3 (amarillo) y
    abajo a la derecha el 0 (azul claro). En la Game Gear solo se ve la ventana central de 160x144, que
    contiene el cruce de los cuatro cuadrantes;
  - toca un tono cuadrado de unos 1000 Hz por el canal 0 del PSG, a todo volumen;
  - si el principio de la RAM del cartucho ($8000) lleva la marca «S» de una partida anterior, el
    cuadrante de abajo a la derecha sale rojo en vez de azul;
  - botones (cada cuadro, en el vblank): el 1 cambia el color 0 a blanco mientras se mantiene; Derecha
    desplaza la imagen (registro 8 del VDP); el 2 escribe «S» en la RAM del cartucho.

Uso:  tools/sms_rom_teste.py [--gg] SALIDA.sms
"""
import sys


class Asm:
    def __init__(self):
        self.c, self.etq, self.pend = bytearray(), {}, []

    def aqui(self):
        return len(self.c)

    def e(self, n):
        self.etq[n] = self.aqui()

    def b(self, *x):
        self.c.extend(x)

    def rel(self, op, n):
        self.b(op, 0)
        self.pend.append((len(self.c) - 1, n, "rel"))

    def abs_(self, op, n):
        self.b(op, 0, 0)
        self.pend.append((len(self.c) - 2, n, "abs"))

    def cerrar(self):
        for pos, n, t in self.pend:
            d = self.etq[n]
            if t == "abs":
                self.c[pos], self.c[pos + 1] = d & 0xFF, d >> 8
            else:
                off = d - (pos + 1)
                assert -128 <= off <= 127, (n, off)
                self.c[pos] = off & 0xFF
        return bytes(self.c)


COLORES = (0x30, 0x03, 0x0C, 0x0F)                # SMS: 00BBGGRR: azul claro, rojo, verde, amarillo
COLORES_GG = ((0x0, 0x8, 0xF), (0xF, 0x0, 0x0), (0x0, 0xF, 0x0), (0xF, 0xF, 0x0))  # (R, G, B)


def programa(gg):
    a = Asm()
    a.b(0xF3, 0xED, 0x56, 0x31, 0xF0, 0xDF)           # DI IM 1 LD SP,$DFF0
    a.b(0x21); a.pend.append((len(a.c), "vdpregs", "abs")); a.b(0, 0)   # LD HL,vdpregs
    a.b(0x06, 22, 0x0E, 0xBF, 0xED, 0xB3)             # LD B,22 LD C,$BF OTIR
    # sprites: Y = $D0 en $3F00 (fin de la lista)
    a.b(0x3E, 0x00, 0xD3, 0xBF, 0x3E, 0x7F, 0xD3, 0xBF, 0x3E, 0xD0, 0xD3, 0xBE)
    # paleta (CRAM $00)
    a.b(0x3E, 0x00, 0xD3, 0xBF, 0x3E, 0xC0, 0xD3, 0xBF)
    a.b(0x21); a.pend.append((len(a.c), "pal", "abs")); a.b(0, 0)
    npal = 8 if gg else 4
    a.b(0x06, npal, 0x0E, 0xBE, 0xED, 0xB3)           # LD B,n LD C,$BE OTIR
    # tiles en VRAM $0000
    a.b(0x3E, 0x00, 0xD3, 0xBF, 0x3E, 0x40, 0xD3, 0xBF)
    a.b(0x21); a.pend.append((len(a.c), "tiles", "abs")); a.b(0, 0)
    a.b(0x06, 128, 0x0E, 0xBE, 0xED, 0xB3)
    # marca de la RAM del cartucho: encender, leer $8000
    a.b(0x3E, 0x08, 0x32, 0xFC, 0xFF)                 # LD A,8 LD ($FFFC),A
    a.b(0x3E, 0x00, 0x32, 0x01, 0xC0)                 # LD A,0 LD ($C001),A
    a.b(0x3A, 0x00, 0x80, 0xFE, 0x53)                 # LD A,($8000) CP 'S'
    a.rel(0x20, "nof")                                # JR NZ
    a.b(0x3E, 0x01, 0x32, 0x01, 0xC0)                 # flag = 1
    a.e("nof")
    # nametable en $3800
    a.b(0x3E, 0x00, 0xD3, 0xBF, 0x3E, 0x78, 0xD3, 0xBF)
    a.b(0x16, 0x00)                                   # LD D,0
    a.e("fila"); a.b(0x1E, 0x00)                      # LD E,0
    a.e("col")
    a.b(0x7A, 0xFE, 12, 0x3E, 0x00); a.rel(0x38, "s1"); a.b(0x3E, 0x02)        # LD A,D CP 12 LD A,0 JR C LD A,2
    a.e("s1"); a.b(0x47, 0x7B, 0xFE, 16, 0x78); a.rel(0x38, "s2"); a.b(0x3C)  # LD B,A LD A,E CP 16 LD A,B JR C INC A
    a.e("s2"); a.b(0xFE, 0x03); a.rel(0x20, "poner")                           # CP 3 JR NZ
    a.b(0x3A, 0x01, 0xC0, 0xB7, 0x3E, 0x03); a.rel(0x28, "poner"); a.b(0xAF)   # LD A,($C001) OR A LD A,3 JR Z XOR A
    a.e("poner"); a.b(0xD3, 0xBE, 0xAF, 0xD3, 0xBE)                            # OUT ($BE),A XOR A OUT ($BE),A
    a.b(0x1C, 0x7B, 0xFE, 32); a.rel(0x20, "col")                              # INC E LD A,E CP 32 JR NZ
    a.b(0x14, 0x7A, 0xFE, 28); a.rel(0x20, "fila")                             # INC D LD A,D CP 28 JR NZ
    # PSG: canal 0 a unos 1000 Hz (N = 112), volumen máximo
    a.b(0x3E, 0x80, 0xD3, 0x7F, 0x3E, 0x07, 0xD3, 0x7F, 0x3E, 0x90, 0xD3, 0x7F)
    # pantalla encendida (registro 1 = $40)
    a.b(0x3E, 0x40, 0xD3, 0xBF, 0x3E, 0x81, 0xD3, 0xBF)
    a.e("loop"); a.b(0xDB, 0xBF, 0xCB, 0x7F); a.rel(0x28, "loop")              # IN A,($BF) BIT 7,A JR Z
    a.b(0xDB, 0xDC, 0x47)                                                      # IN A,($DC) LD B,A
    # color 0: blanco si el botón 1 (bit 4 a cero)
    a.b(0x3E, 0x00, 0xD3, 0xBF, 0x3E, 0xC0, 0xD3, 0xBF)
    if gg:
        a.b(0xCB, 0x60, 0x3E, 0x80); a.rel(0x20, "g1"); a.b(0x3E, 0xFF)       # BIT 4,B LD A,$80 JR NZ LD A,$FF
        a.e("g1"); a.b(0xD3, 0xBE, 0x3E, 0x0F, 0xD3, 0xBE)                     # OUT; LD A,$0F OUT
    else:
        a.b(0xCB, 0x60, 0x3E, 0x30); a.rel(0x20, "np"); a.b(0x3E, 0x3F)        # BIT 4,B LD A,$30 JR NZ LD A,$3F
        a.e("np"); a.b(0xD3, 0xBE)
    # Derecha (bit 3 a cero): scroll horizontal (registro 8)
    a.b(0xCB, 0x58, 0x3E, 0x00); a.rel(0x20, "nr"); a.b(0x3E, 0xC0)            # BIT 3,B LD A,0 JR NZ LD A,$C0
    a.e("nr"); a.b(0xD3, 0xBF, 0x3E, 0x88, 0xD3, 0xBF)
    # botón 2 (bit 5 a cero): marca en la RAM del cartucho
    a.b(0xCB, 0x68); a.rel(0x20, "loop")                                       # BIT 5,B JR NZ loop
    a.b(0x3E, 0x08, 0x32, 0xFC, 0xFF, 0x3E, 0x53, 0x32, 0x00, 0x80)            # RAM on; ($8000) = 'S'
    a.abs_(0xC3, "loop")
    a.e("vdpregs")
    regs = [0x04, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0xFF]
    for i, v in enumerate(regs):
        a.b(v, 0x80 | i)
    a.e("pal")
    if gg:
        for r, g, bl in COLORES_GG:
            a.b((g << 4) | r, bl)
    else:
        a.b(*COLORES)
    a.e("tiles")
    for k in (1, 2, 3, 0):                      # tile q = color (q+1) & 3: rojo, verde, amarillo, azul
        for _ in range(8):
            a.b(*[0xFF if (k >> p) & 1 else 0x00 for p in range(4)])
    return a.cerrar()


def rom(gg=False):
    prg = programa(gg)
    assert len(prg) < 0x7FF0 - 0x100
    data = bytearray(0x10000)
    data[:len(prg)] = prg
    cab = bytearray(16)
    cab[0:8] = b"TMR SEGA"
    region = 0x6 if gg else 0x4                  # 6 = Game Gear exportación, 4 = SMS exportación
    cab[15] = (region << 4) | 0xF                # 64 KB
    cab[14] = 0x00
    data[0x7FF0:0x8000] = cab
    suma = (sum(data[:0x7FF0]) + sum(data[0x8000:])) & 0xFFFF
    data[0x7FFA], data[0x7FFB] = suma & 0xFF, suma >> 8
    return bytes(data)


if __name__ == "__main__":
    args = sys.argv[1:]
    gg = "--gg" in args
    args = [x for x in args if x != "--gg"]
    if len(args) != 1:
        print(__doc__)
        sys.exit(2)
    d = rom(gg)
    with open(args[0], "wb") as f:
        f.write(d)
    print("ROM de %s de prueba: %d bytes" % ("Game Gear" if gg else "Master System", len(d)))
