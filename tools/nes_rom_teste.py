#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""nes_rom_teste.py - una ROM de NES mínima, nuestra, para probar LaiNES (D321).

No se baja nada ni hay código de terceros: el programa es un 6502 de unas 100 instrucciones escrito
con el ensamblador de unas líneas de este fichero. Cabecera iNES de 16 bytes, mapper 1 (MMC1, SxROM: 16 KB
de PRG y 8 KB de CHR; el único que escribe la RAM de $6000 entre los mappers de la base), con batería (byte 6, bit 1).

Qué hace (se ve y se oye, para tests/h321_emuladores.py):
  - pinta la pantalla en cuatro cuadrantes (256x240: 128x120 cada uno) con los cuatro colores de la
    paleta de fondo: arriba a la izquierda el color 1 (rojo, $16), arriba a la derecha el 2 (verde,
    $2A), abajo a la izquierda el 3 (amarillo, $28) y abajo a la derecha el 0 (azul claro, $21);
  - toca por el canal de pulso 1 un cuadrado de unos 1000 Hz a todo volumen;
  - si el principio de la RAM de la batería ($6000) lleva la marca «S» de una partida anterior, el
    cuadrante de abajo a la derecha sale rojo (tile 0) en vez de azul;
  - botones (cada cuadro, en el vblank): A cambia el color 0 a blanco ($30) mientras se mantiene;
    Derecha desplaza la imagen 64 píxeles a la izquierda; Start escribe «S» en $6000 (la batería).

Uso:  tools/nes_rom_teste.py SALIDA.nes
"""
import sys

PALETA = (0x21, 0x16, 0x2A, 0x28)   # color 0 (fondo), 1, 2, 3


class Asm:
    def __init__(self, origen):
        self.origen, self.c, self.etq, self.pend = origen, bytearray(), {}, []

    def aqui(self):
        return self.origen + len(self.c)

    def e(self, n):
        self.etq[n] = self.aqui()

    def b(self, *x):
        self.c.extend(x)

    def rel(self, op, n):          # saltos relativos
        self.b(op, 0)
        self.pend.append((len(self.c) - 1, n))

    def abs_(self, op, n):         # JMP absoluto a etiqueta
        self.b(op, 0, 0)
        self.pend.append((len(self.c) - 2, n, "abs"))

    def cerrar(self):
        for p in self.pend:
            pos, n = p[0], p[1]
            d = self.etq[n]
            if len(p) == 3:
                self.c[pos], self.c[pos + 1] = d & 0xFF, d >> 8
            else:
                off = d - (self.origen + pos + 1)
                assert -128 <= off <= 127, (n, off)
                self.c[pos] = off & 0xFF
        return bytes(self.c)


def programa():
    a = Asm(0xC000)       # PRG de 16 KB espejado en $8000 y $C000
    a.e("reset")
    a.b(0x78, 0xD8, 0xA2, 0xFF, 0x9A)                  # SEI CLD LDX #$FF TXS
    a.b(0xA9, 0x00, 0x8D, 0x00, 0x20, 0x8D, 0x01, 0x20)  # LDA #0 STA $2000 STA $2001
    a.e("w1"); a.b(0x2C, 0x02, 0x20); a.rel(0x10, "w1")  # BIT $2002 BPL
    a.e("w2"); a.b(0x2C, 0x02, 0x20); a.rel(0x10, "w2")
    # marca de la batería -> $00
    a.b(0xA9, 0x00, 0x85, 0x00)                        # LDA #0 STA $00
    a.b(0xAD, 0x00, 0x60, 0xC9, 0x53)                  # LDA $6000 CMP #'S'
    a.rel(0xD0, "nf")                                  # BNE nf
    a.b(0xA9, 0x01, 0x85, 0x00)                        # LDA #1 STA $00
    a.e("nf")
    # paleta de fondo en $3F00
    a.b(0xA9, 0x3F, 0x8D, 0x06, 0x20, 0xA9, 0x00, 0x8D, 0x06, 0x20)
    a.b(0xA2, 0x00)
    a.e("pal"); a.abs_(0xBD, "paltab"); a.b(0x8D, 0x07, 0x20, 0xE8, 0xE0, 0x04); a.rel(0xD0, "pal")
    # tiles de CHR no hay que cargarlos: son ROM. Nametable $2000
    a.b(0xA9, 0x20, 0x8D, 0x06, 0x20, 0xA9, 0x00, 0x8D, 0x06, 0x20)
    a.b(0xA0, 0x00)                                    # LDY #0 (fila)
    a.e("fila"); a.b(0xA2, 0x00)                       # LDX #0 (columna)
    a.e("col"); a.b(0xA9, 0x00)                        # LDA #0
    a.b(0xC0, 0x0F); a.rel(0x90, "a1"); a.b(0xA9, 0x02)  # CPY #15 BCC a1 LDA #2
    a.e("a1"); a.b(0xE0, 0x10); a.rel(0x90, "a2"); a.b(0x09, 0x01)  # CPX #16 BCC a2 ORA #1
    a.e("a2"); a.b(0xC9, 0x03); a.rel(0xD0, "ok")      # CMP #3 BNE ok
    a.b(0xA5, 0x00); a.rel(0xF0, "es3")                # LDA $00 BEQ es3
    a.b(0xA9, 0x00); a.rel(0xF0, "ok")                 # LDA #0 (Z=1) BEQ ok
    a.e("es3"); a.b(0xA9, 0x03)
    a.e("ok"); a.b(0x8D, 0x07, 0x20, 0xE8, 0xE0, 0x20); a.rel(0xD0, "col")
    a.b(0xC8, 0xC0, 0x1E); a.rel(0xD0, "fila")         # INY CPY #30
    # atributos ($23C0): 64 bytes a 0
    a.b(0xA9, 0x23, 0x8D, 0x06, 0x20, 0xA9, 0xC0, 0x8D, 0x06, 0x20, 0xA9, 0x00, 0xA2, 0x40)
    a.e("atr"); a.b(0x8D, 0x07, 0x20, 0xCA); a.rel(0xD0, "atr")
    # sonido: pulso 1, cuadrado de unos 1000 Hz (periodo 111), volumen 15, longitud parada
    a.b(0xA9, 0x01, 0x8D, 0x15, 0x40, 0xA9, 0xBF, 0x8D, 0x00, 0x40, 0xA9, 0x6F, 0x8D, 0x02, 0x40, 0xA9, 0xF8, 0x8D, 0x03, 0x40)
    # scroll a 0 y encender el fondo
    a.b(0xAD, 0x02, 0x20, 0xA9, 0x00, 0x8D, 0x05, 0x20, 0x8D, 0x05, 0x20, 0x8D, 0x00, 0x20, 0xA9, 0x0A, 0x8D, 0x01, 0x20)
    # bucle principal
    a.e("loop"); a.b(0x2C, 0x02, 0x20); a.rel(0x10, "loop")
    a.b(0xA9, 0x01, 0x8D, 0x16, 0x40, 0xA9, 0x00, 0x8D, 0x16, 0x40, 0xA2, 0x08)   # strobe, X=8
    a.e("rd"); a.b(0xAD, 0x16, 0x40, 0x4A, 0x26, 0x01, 0xCA); a.rel(0xD0, "rd")   # LDA $4016 LSR ROL $01 DEX BNE
    # color 0: $30 si A (bit 7), si no $21
    a.b(0xAD, 0x02, 0x20, 0xA9, 0x3F, 0x8D, 0x06, 0x20, 0xA9, 0x00, 0x8D, 0x06, 0x20)
    a.b(0xA9, PALETA[0], 0x24, 0x01); a.rel(0x10, "noA")   # LDA #$21 BIT $01 (N = bit7) BPL noA
    a.b(0xA9, 0x30)
    a.e("noA"); a.b(0x8D, 0x07, 0x20)
    # Start (bit 4): marca en la batería
    a.b(0xA5, 0x01, 0x29, 0x10); a.rel(0xF0, "noS")
    a.b(0xA9, 0x53, 0x8D, 0x00, 0x60)
    a.e("noS")
    # scroll: Derecha (bit 0) -> X = 64
    a.b(0xA5, 0x01, 0x29, 0x01); a.rel(0xF0, "noR")
    a.b(0xA9, 0x40)
    a.e("noR"); a.b(0x8D, 0x05, 0x20, 0xA9, 0x00, 0x8D, 0x05, 0x20, 0x8D, 0x00, 0x20)   # y PPUCTRL=0: elige la tabla 0 (el $2006 deja bits de ella)
    a.abs_(0x4C, "loop")
    a.e("paltab")
    a.b(*PALETA)
    return a.cerrar(), a.etq["reset"]


def rom():
    prg, reset = programa()
    assert len(prg) < 0x4000 - 6
    prg = bytearray(prg + bytes(0x4000 - 6 - len(prg)))
    for v in (reset, reset, reset):      # NMI, RESET, IRQ
        prg += bytes([v & 0xFF, v >> 8])
    assert len(prg) == 0x4000
    # CHR: 4 tiles sólidos con los colores 1, 2, 3 y 0
    chr_ = bytearray(0x2000)
    solid = {0: (0xFF, 0x00), 1: (0x00, 0xFF), 2: (0xFF, 0xFF), 3: (0x00, 0x00)}
    for t in range(4):
        p0, p1 = solid[t]
        chr_[t * 16:t * 16 + 8] = bytes([p0] * 8)
        chr_[t * 16 + 8:t * 16 + 16] = bytes([p1] * 8)
    cab = bytes([0x4E, 0x45, 0x53, 0x1A, 1, 1, 0x12, 0, 0, 0, 0, 0, 0, 0, 0, 0])   # batería
    return cab + bytes(prg) + bytes(chr_)


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    d = rom()
    with open(sys.argv[1], "wb") as f:
        f.write(d)
    print("ROM de NES de prueba: %d bytes" % len(d))
