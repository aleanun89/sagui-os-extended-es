#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""mgba_rom_teste.py - ROM de prueba mínimas y propias (GBA, GB y GBC) para mGBA (D322).

Nada comercial y nada de terceros: el programa de cada ROM es de esta herramienta (un
ensamblador de unas pocas instrucciones de ARM, y el de la Game Boy de gnuboy_rom_teste.py).

  GBA  cabecera con el título y la suma de la cabecera (0xBD) correctos; el logo de Nintendo
       (0x04-0x9F) va a CEROS a propósito: mGBA no lo comprueba (arranca sin BIOS) y no se copia
       ningún logo. Sirve solo para emuladores con BIOS HLE; una GBA real la rechazaría.
       Programa ARM (modo 3, 240x160): cuatro cuadrantes (arriba izquierda azul, arriba derecha
       rojo, abajo izquierda verde, abajo derecha blanco); mientras se mantiene A se permutan los
       colores, y Select alterna un modo que también los permuta y queda en el estado (sirve para
       probar los estados de guardado); Start escribe «SAGUI» en la SRAM (la ROM lleva la marca «SRAM_V113»). Redibuja solo
       cuando cambian las teclas, y cada cambio también mueve un contador de EWRAM.
  GBA  (saguiestres.gba) una GBA «cargada» para medir el coste del vídeo: modo 0 con los cuatro
       fondos opacos desplazándose y 128 sprites de 64x64 que se pisan (mgba --medir).
  GB   la ROM de tools/gnuboy_rom_teste.py (cuadrantes de cuatro tonos, A invierte la paleta,
       Start escribe la batería, tono de 1000 Hz).
  GBC  la misma con la marca de Game Boy Color (0x143 = 0xC0) y un arranque que carga la paleta
       0 del fondo con cuatro colores (blanco, rojo, verde, azul).

Uso:  tools/mgba_rom_teste.py DIRECTORIO     (deja saguitest.gba, saguiestres.gba, saguitest.gb y saguitest.gbc)
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gnuboy_rom_teste as gb  # noqa: E402

AL, EQ, NE, GE, LT = 0xE, 0x0, 0x1, 0xA, 0xB
AND, SUB, ADD, TST, CMP, ORR, MOV, MVN = 0x0, 0x2, 0x4, 0x8, 0xA, 0xC, 0xD, 0xF


def imm_arm(v):
    """Un inmediato de ARM: 8 bits rotados a la derecha un número par de veces."""
    for rot in range(16):
        r = ((v << (2 * rot)) | (v >> (32 - 2 * rot))) & 0xFFFFFFFF if rot else v
        if r < 256:
            return (rot << 8) | r
    raise ValueError("no cabe como inmediato: %#x" % v)


class Arm:
    def __init__(self, origen):
        self.origen = origen
        self.w = []
        self.etq = {}
        self.pend = []

    def aqui(self):
        return self.origen + 4 * len(self.w)

    def e(self, nombre):
        self.etq[nombre] = self.aqui()

    def dp(self, op, rd, rn, v, cond=AL, s=0):
        self.w.append(cond << 28 | 1 << 25 | op << 21 | s << 20 | rn << 16 | rd << 12 | imm_arm(v))

    def mov(self, rd, v, cond=AL): self.dp(MOV, rd, 0, v, cond)
    def mvn(self, rd, v): self.dp(MVN, rd, 0, v)
    def add(self, rd, rn, v): self.dp(ADD, rd, rn, v)
    def orr(self, rd, rn, v): self.dp(ORR, rd, rn, v)
    def cmp(self, rn, v): self.dp(CMP, 0, rn, v, s=1)
    def tst(self, rn, v): self.dp(TST, 0, rn, v, s=1)
    def subs(self, rd, rn, v): self.dp(SUB, rd, rn, v, s=1)

    def dpr(self, op, rd, rn, rm, cond=AL, s=0):
        self.w.append(cond << 28 | op << 21 | s << 20 | rn << 16 | rd << 12 | rm)

    def movr(self, rd, rm, cond=AL): self.dpr(MOV, rd, 0, rm, cond)
    def andr(self, rd, rn, rm): self.dpr(AND, rd, rn, rm)
    def orrr(self, rd, rn, rm): self.dpr(ORR, rd, rn, rm)
    def cmpr(self, rn, rm): self.dpr(CMP, 0, rn, rm, s=1)
    def mvnr(self, rd, rm): self.dpr(MVN, rd, 0, rm)

    def dps(self, op, rd, rn, rm, tipo, cant, cond=AL, s=0):
        """Registro con desplazamiento inmediato (tipo 0 = LSL)."""
        self.w.append(cond << 28 | op << 21 | s << 20 | rn << 16 | rd << 12 | cant << 7 | tipo << 5 | rm)

    def strh(self, rd, rn, off=0):
        self.w.append(0xE1C000B0 | rn << 16 | rd << 12 | (off >> 4) << 8 | (off & 15))

    def ldrh(self, rd, rn, off=0):
        self.w.append(0xE1D000B0 | rn << 16 | rd << 12 | (off >> 4) << 8 | (off & 15))

    def strb(self, rd, rn, off=0):
        self.w.append(0xE5C00000 | rn << 16 | rd << 12 | off)

    def str_post(self, rd, rn, off):
        self.w.append(0xE4800000 | rn << 16 | rd << 12 | off)

    def ldr_lit(self, rd, nombre):
        self.pend.append((len(self.w), nombre, "lit", rd))
        self.w.append(0)

    def b(self, nombre, cond=AL):
        self.pend.append((len(self.w), nombre, "b", cond))
        self.w.append(0)

    def dato(self, v):
        self.w.append(v & 0xFFFFFFFF)

    def datos_etq(self, nombre, v):
        self.e(nombre)
        self.dato(v)

    def resolver(self):
        for i, nombre, tipo, x in self.pend:
            dest = self.etq[nombre]
            off = dest - (self.origen + 4 * i + 8)
            if tipo == "b":
                self.w[i] = x << 28 | 0x0A000000 | ((off >> 2) & 0xFFFFFF)
            else:
                assert 0 <= off < 4096
                self.w[i] = 0xE59F0000 | x << 12 | off
        return b"".join(struct.pack("<I", w) for w in self.w)


AZUL, ROJO, VERDE, BLANCO = 0x7C00, 0x001F, 0x03E0, 0x7FFF   # BGR555


def par(c):
    return c << 16 | c


def programa_gba():
    a = Arm(0x08000000 + 0xC0)
    a.e("inicio")
    a.mov(0, 0x04000000)                 # r0 = REG_BASE
    a.mov(1, 0x400); a.orr(1, 1, 3)      # DISPCNT = modo 3 + BG2
    a.w.append(0xE1C010B0)               # strh r1, [r0]
    a.mvn(6, 0)                          # r6 = teclas anteriores (ninguna válida)
    a.mov(14, 0)                         # modo de colores (Select lo alterna)
    a.mov(13, 0x03000000); a.add(13, 13, 0x7F00)   # sp (IWRAM)
    a.e("bucle")
    a.e("espera_vb")
    a.ldrh(2, 0, 6); a.cmp(2, 160); a.b("espera_vb", LT)
    a.add(3, 0, 0x130); a.ldrh(4, 3, 0)
    a.mvnr(4, 4); a.mov(7, 0x300); a.orr(7, 7, 0xFF); a.andr(4, 4, 7)
    a.cmpr(4, 6); a.b("sin_cambio", EQ)
    a.mvnr(7, 6); a.andr(7, 7, 4)       # r7 = teclas recién pulsadas
    a.tst(7, 4); a.dp(0x1, 14, 14, 1, cond=NE)    # Select recién pulsado: r14 ^= 1 (modo, queda en el estado)
    a.movr(6, 4)
    # contador de cambios en EWRAM (0x02000000)
    a.mov(8, 0x02000000); a.w.append(0xE5989000); a.add(9, 9, 1); a.w.append(0xE5889000)  # ldr r9,[r8]; add; str r9,[r8]
    # Start (bit 3): «SAGUI» en la SRAM
    a.tst(4, 8); a.b("sin_start", EQ)
    a.mov(8, 0x0E000000)
    for i, ch in enumerate(b"SAGUI"):
        a.mov(9, ch); a.strb(9, 8, i)
    a.e("sin_start")
    # colores (de dos en dos píxeles) en r8..r11 según A (bit 0)
    a.dp(AND, 12, 4, 1); a.dpr(0x1, 12, 12, 14); a.tst(12, 1); a.b("normal", EQ)   # A xor modo
    a.ldr_lit(8, "c_blanco"); a.ldr_lit(9, "c_verde"); a.ldr_lit(10, "c_rojo"); a.ldr_lit(11, "c_azul")
    a.b("pinta", AL)
    a.e("normal")
    a.ldr_lit(8, "c_azul"); a.ldr_lit(9, "c_rojo"); a.ldr_lit(10, "c_verde"); a.ldr_lit(11, "c_blanco")
    a.e("pinta")
    a.mov(5, 0x06000000)
    a.mov(2, 0)
    a.e("fila")
    a.cmp(2, 80)
    a.movr(12, 8, LT); a.movr(1, 9, LT); a.movr(12, 10, GE); a.movr(1, 11, GE)
    a.mov(7, 60)
    a.e("izq")
    a.str_post(12, 5, 4); a.subs(7, 7, 1); a.b("izq", NE)
    a.mov(7, 60)
    a.e("der")
    a.str_post(1, 5, 4); a.subs(7, 7, 1); a.b("der", NE)
    a.add(2, 2, 1); a.cmp(2, 160); a.b("fila", LT)
    a.e("sin_cambio")
    a.e("espera_fuera")
    a.ldrh(2, 0, 6); a.cmp(2, 160); a.b("espera_fuera", GE)
    a.b("bucle")
    a.datos_etq("c_azul", par(AZUL)); a.datos_etq("c_rojo", par(ROJO))
    a.datos_etq("c_verde", par(VERDE)); a.datos_etq("c_blanco", par(BLANCO))
    return a.resolver()


def programa_estres():
    """Una GBA «cargada»: modo 0 con los cuatro fondos opacos y 128 sprites de 64x64 que se pisan,
    con los fondos desplazándose cada cuadro. Sirve para medir el coste del vídeo (mgba --medir)."""
    a = Arm(0x08000000 + 0xC0)
    a.mov(0, 0x04000000)
    a.mov(1, 0x05000000); a.ldr_lit(2, "blanco"); a.strh(2, 1, 2)       # color 1 de la paleta: blanco
    a.mov(5, 0x06000000); a.ldr_lit(2, "pix"); a.mov(7, 0x1000)         # charblock 0 entero con el color 1
    a.e("rellena"); a.str_post(2, 5, 4); a.subs(7, 7, 1); a.b("rellena", NE)
    for n, v in enumerate((0x0800, 0x0901, 0x0A02, 0x0B03)):            # BGnCNT: prioridad n, mapa en el bloque 8+n
        a.mov(2, v & 0xF00); a.orr(2, 2, v & 0xFF); a.strh(2, 0, 8 + 2 * n)
    a.mov(5, 0x07000000); a.mov(6, 0)
    a.e("oam")
    a.dps(ADD, 2, 6, 6, 0, 1); a.dp(AND, 2, 2, 0x7F)                    # y = (3 i) & 127
    a.dps(0x3, 3, 6, 6, 0, 3); a.dp(AND, 3, 3, 0xFF); a.orr(3, 3, 0xC000)   # x = (7 i) & 255, tamaño 64x64
    a.dps(ORR, 2, 2, 3, 0, 16)                                          # atributo 0 | atributo 1 << 16
    a.str_post(2, 5, 4); a.mov(2, 0); a.str_post(2, 5, 4)
    a.add(6, 6, 1); a.cmp(6, 128); a.b("oam", LT)
    a.mov(1, 0x1F00); a.orr(1, 1, 0x40); a.strh(1, 0, 0)                # modo 0, BG0-3, OBJ, mapa de sprites 1D
    a.mov(4, 0)
    a.e("cuadro")
    a.e("vb")
    a.ldrh(2, 0, 6); a.cmp(2, 160); a.b("vb", LT)
    a.add(4, 4, 1)
    for off in (0x10, 0x14, 0x18, 0x1C):                                # desplazamientos horizontales de los cuatro fondos
        a.strh(4, 0, off)
    a.e("fuera")
    a.ldrh(2, 0, 6); a.cmp(2, 160); a.b("fuera", GE)
    a.b("cuadro")
    a.datos_etq("blanco", 0x7FFF); a.datos_etq("pix", 0x11111111)
    return a.resolver()


def rom_gba_estres():
    rom = bytearray(0x8000)
    rom[0:4] = struct.pack("<I", 0xEA000000 | ((0xC0 - 8) >> 2))
    rom[0xA0:0xA0 + 11] = b"SAGUIESTRES"
    rom[0xAC:0xB0] = b"ASGE"
    rom[0xB0:0xB2] = b"01"
    rom[0xB2] = 0x96
    prog = programa_estres()
    rom[0xC0:0xC0 + len(prog)] = prog
    rom[0xBD] = (-(sum(rom[0xA0:0xBD]) + 0x19)) & 0xFF
    return bytes(rom)


def rom_gba():
    rom = bytearray(0x8000)
    rom[0:4] = struct.pack("<I", 0xEA000000 | ((0xC0 - 8) >> 2))      # b 0xC0
    rom[0xA0:0xA0 + 9] = b"SAGUITEST"
    rom[0xAC:0xB0] = b"ASGT"
    rom[0xB0:0xB2] = b"01"
    rom[0xB2] = 0x96
    prog = programa_gba()
    rom[0xC0:0xC0 + len(prog)] = prog
    marca = b"SRAM_V113\0\0\0"
    rom[0x200:0x200 + len(marca)] = marca
    rom[0xBD] = (-(sum(rom[0xA0:0xBD]) + 0x19)) & 0xFF
    return bytes(rom)


def rom_gb():
    return gb.construir()


def rom_gbc():
    rom = bytearray(gb.construir())
    # Arranque propio en 0x7F00 (el programa de 32 KB no usa bancos): paleta 0 del fondo.
    pal = struct.pack("<4H", 0x7FFF, 0x001F, 0x03E0, 0x7C00)
    stub = bytes([0x3E, 0x80, 0xE0, 0x68,          # ld a,0x80 ; ldh [0x68],a  (BCPS: autoincremento)
                  0x21, 0x20, 0x7F,                # ld hl, 0x7F20
                  0x0E, 0x08,                      # ld c, 8
                  0x2A, 0xE0, 0x69,                # ld a,[hl+] ; ldh [0x69],a
                  0x0D, 0x20, 0xFA,                # dec c ; jr nz, -6
                  0xC3, 0x50, 0x01])               # jp 0x0150
    # el jr relativo salta a 0x7F09 (el ld a,[hl+])
    rom[0x7F00:0x7F00 + len(stub)] = stub
    rom[0x7F20:0x7F20 + len(pal)] = pal
    rom[0x102:0x104] = struct.pack("<H", 0x7F00)    # nop ; jp 0x7F00
    rom[0x143] = 0xC0                               # solo Game Boy Color
    x = 0
    for i in range(0x134, 0x14D):
        x = (x - rom[i] - 1) & 0xFF
    rom[0x14D] = x
    rom[0x14E:0x150] = b"\0\0"
    suma = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
    rom[0x14E:0x150] = struct.pack(">H", suma)
    return bytes(rom)


def generar(directorio):
    os.makedirs(directorio, exist_ok=True)
    r = {}
    for nombre, f in (("saguitest.gba", rom_gba), ("saguiestres.gba", rom_gba_estres), ("saguitest.gb", rom_gb), ("saguitest.gbc", rom_gbc)):
        r[nombre] = os.path.join(directorio, nombre)
        with open(r[nombre], "wb") as fh:
            fh.write(f())
    return r


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    for k, v in generar(sys.argv[1]).items():
        print("mgba_rom_teste: %s (%d bytes)" % (v, os.path.getsize(v)))
