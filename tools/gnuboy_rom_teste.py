#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""gnuboy_rom_teste.py - una ROM de Game Boy mínima, nuestra, para probar GNUBoy (D235).

No se baja nada y no hay código de terceros: el programa es de esta herramienta (un
ensamblador de unas pocas instrucciones del LR35902 escrito aquí). La cabecera lleva el
logo de Nintendo que comprueba el arranque de una consola de verdad (son los 48 bytes que
publica toda la documentación de la Game Boy, 0x104-0x133) y las dos sumas correctas
(0x14D la de la cabecera, 0x14E-0x14F la global), así que sirve también en otros
emuladores. 32 KB, cartucho MBC1 con RAM de 8 KB y batería (tipo 0x03).

Lo que hace (todo se ve y se oye, para la prueba tests/h235_gnuboy.py):

  - pinta la pantalla en cuatro cuadrantes de 80x72 píxeles, con los cuatro tonos de la
    paleta (BGP = 0xE4):  arriba a la izquierda el tono 3 (el más oscuro), arriba a la
    derecha el 1, abajo a la izquierda el 2 y abajo a la derecha el 0 (el claro);
  - toca un tono cuadrado de 1000 Hz por el canal 1 del APU, a todo volumen;
  - si en el principio de la RAM de la batería (0xA000) está la marca «SAGUI», la
    ROM la encontró guardada de una partida anterior: el cuadrante de abajo a la derecha
    sale entonces del tono 3 (oscuro) en vez del 0;
  - botones (cada cuadro): A invierte la paleta (BGP = 0x1B) mientras se mantiene; Derecha
    desplaza la imagen 8 píxeles a la izquierda (SCX = 8); B la desplaza 8 hacia arriba
    (SCY = 8); Select silencia el sonido (NR51 = 0); Start escribe «SAGUI» y un contador
    en la RAM de la batería (que el emulador deja en el .sav al salir).

Uso:  tools/gnuboy_rom_teste.py SALIDA.gb
"""
import struct
import sys

# El logo de Nintendo (0x104-0x133): lo comprueba el arranque de la Game Boy de verdad.
LOGO = bytes([
    0xCE, 0xED, 0x66, 0x66, 0xCC, 0x0D, 0x00, 0x0B, 0x03, 0x73, 0x00, 0x83, 0x00, 0x0C, 0x00, 0x0D,
    0x00, 0x08, 0x11, 0x1F, 0x88, 0x89, 0x00, 0x0E, 0xDC, 0xCC, 0x6E, 0xE6, 0xDD, 0xDD, 0xD9, 0x99,
    0xBB, 0xBB, 0x67, 0x63, 0x6E, 0x0E, 0xEC, 0xCC, 0xDD, 0xDC, 0x99, 0x9F, 0xBB, 0xB9, 0x33, 0x3E])
assert len(LOGO) == 48

MARCA = b"SAGUI"
TITULO = b"SAGUITEST"


class Ensamblador:
    """Lo justo para este programa: bytes, etiquetas y saltos relativos."""

    def __init__(self, origen):
        self.origen = origen
        self.c = bytearray()
        self.etq = {}
        self.pend = []   # (posición en self.c, etiqueta, tipo)

    def aqui(self):
        return self.origen + len(self.c)

    def etiqueta(self, nombre):
        assert nombre not in self.etq, nombre
        self.etq[nombre] = self.aqui()

    def b(self, *bytes_):
        self.c.extend(bytes_)

    def salto_rel(self, op, nombre):
        self.c.append(op)
        self.pend.append((len(self.c), nombre, "rel"))
        self.c.append(0)

    def abs16(self, op, nombre):
        self.c.append(op)
        self.pend.append((len(self.c), nombre, "abs"))
        self.c.extend(b"\0\0")

    def resolver(self):
        for pos, nombre, tipo in self.pend:
            destino = self.etq[nombre]
            if tipo == "rel":
                d = destino - (self.origen + pos + 1)
                assert -128 <= d <= 127, (nombre, d)
                self.c[pos] = d & 0xFF
            else:
                self.c[pos:pos + 2] = struct.pack("<H", destino)
        return bytes(self.c)


def programa():
    a = Ensamblador(0x150)
    b, jr, jp = a.b, a.salto_rel, a.abs16
    NZ, Z, NC, C = 0x20, 0x28, 0x30, 0x38

    def ld_a(n): b(0x3E, n)
    def ldh_n_a(n): b(0xE0, n)          # ldh [0xFF00+n], a
    def ldh_a_n(n): b(0xF0, n)          # ldh a, [0xFF00+n]

    b(0xF3)                              # di
    b(0x31, 0xFE, 0xFF)                  # ld sp, 0xFFFE

    # Esperar al vblank y apagar la pantalla para escribir en la VRAM.
    a.etiqueta("espera_vb")
    ldh_a_n(0x44)                        # LY
    b(0xFE, 144)                         # cp 144
    jr(NZ, "espera_vb")
    b(0xAF)                              # xor a
    ldh_n_a(0x40)                        # LCDC = 0

    # Activar la RAM de la batería y buscar la marca «SAGUI».
    ld_a(0x0A)
    b(0xEA, 0x00, 0x00)                  # ld [0x0000], a  (RAMG: activar)
    b(0x21, 0x00, 0xA0)                  # ld hl, 0xA000
    b(0x11); a.pend.append((len(a.c), "marca", "abs")); b(0, 0)   # ld de, marca
    b(0x0E, 5)                           # ld c, 5
    a.etiqueta("comprueba")
    b(0x2A)                              # ld a, [hl+]
    b(0x47)                              # ld b, a
    b(0x1A)                              # ld a, [de]
    b(0x13)                              # inc de
    b(0xB8)                              # cp b
    jr(NZ, "sin_marca")
    b(0x0D)                              # dec c
    jr(NZ, "comprueba")
    ld_a(1)
    jr(0x18, "guarda_marca")             # jr
    a.etiqueta("sin_marca")
    b(0xAF)                              # xor a
    a.etiqueta("guarda_marca")
    ldh_n_a(0x80)                        # [0xFF80] = hay marca (0 o 1)

    # Copiar los 4 tonos (64 bytes) a la VRAM, 0x8000.
    b(0x21, 0x00, 0x80)                  # ld hl, 0x8000
    b(0x11); a.pend.append((len(a.c), "tiles", "abs")); b(0, 0)   # ld de, tiles
    b(0x0E, 64)                          # ld c, 64
    a.etiqueta("copia")
    b(0x1A)                              # ld a, [de]
    b(0x22)                              # ld [hl+], a
    b(0x13)                              # inc de
    b(0x0D)                              # dec c
    jr(NZ, "copia")

    # Borrar el mapa de fondo (0x9800, 1024 bytes) con el tile 0.
    b(0x21, 0x00, 0x98)                  # ld hl, 0x9800
    b(0x01, 0x00, 0x04)                  # ld bc, 0x0400
    a.etiqueta("borra")
    b(0xAF)                              # xor a
    b(0x22)                              # ld [hl+], a
    b(0x0B)                              # dec bc
    b(0x78)                              # ld a, b
    b(0xB1)                              # or c
    jr(NZ, "borra")

    # Las 18 filas visibles: 10 tiles de la izquierda y 10 de la derecha.
    b(0x21, 0x00, 0x98)                  # ld hl, 0x9800
    b(0x06, 0)                           # ld b, 0   (fila)
    a.etiqueta("fila")
    b(0x78)                              # ld a, b
    b(0xFE, 9)                           # cp 9
    jr(C, "arriba")
    b(0x16, 3)                           # ld d, 3   (abajo a la izquierda: tono 2)
    ldh_a_n(0x80)                        # abajo a la derecha: tile 0, o el 1 si hay marca
    b(0x5F)                              # ld e, a
    jr(0x18, "rellena")
    a.etiqueta("arriba")
    b(0x16, 1)                           # ld d, 1   (arriba a la izquierda: tono 3)
    b(0x1E, 2)                           # ld e, 2   (arriba a la derecha: tono 1)
    a.etiqueta("rellena")
    b(0x0E, 10)                          # ld c, 10
    a.etiqueta("izq")
    b(0x7A)                              # ld a, d
    b(0x22)                              # ld [hl+], a
    b(0x0D)                              # dec c
    jr(NZ, "izq")
    b(0x0E, 10)                          # ld c, 10
    a.etiqueta("der")
    b(0x7B)                              # ld a, e
    b(0x22)                              # ld [hl+], a
    b(0x0D)                              # dec c
    jr(NZ, "der")
    b(0x7D)                              # ld a, l
    b(0xC6, 12)                          # add a, 12
    b(0x6F)                              # ld l, a
    jr(NC, "sin_acarreo")
    b(0x24)                              # inc h
    a.etiqueta("sin_acarreo")
    b(0x04)                              # inc b
    b(0x78)                              # ld a, b
    b(0xFE, 18)                          # cp 18
    jr(NZ, "fila")

    # Desplazamiento a cero, paleta y sonido (canal 1: cuadrada de 1000 Hz).
    b(0xAF)
    ldh_n_a(0x42)                        # SCY
    ldh_n_a(0x43)                        # SCX
    ld_a(0xE4)
    ldh_n_a(0x47)                        # BGP
    ld_a(0x80)
    ldh_n_a(0x26)                        # NR52: APU encendido
    ld_a(0x77)
    ldh_n_a(0x24)                        # NR50: volumen máximo
    ld_a(0xFF)
    ldh_n_a(0x25)                        # NR51: todos los canales a los dos lados
    b(0xAF)
    ldh_n_a(0x10)                        # NR10: sin barrido
    ld_a(0x80)
    ldh_n_a(0x11)                        # NR11: ciclo del 50 %
    ld_a(0xF0)
    ldh_n_a(0x12)                        # NR12: volumen 15, sin envolvente
    ld_a(0x7D)
    ldh_n_a(0x13)                        # NR13: 2048 - 131072/1000 = 1917 = 0x77D
    ld_a(0x87)
    ldh_n_a(0x14)                        # NR14: arranque, sin longitud, frecuencia alta 7
    ld_a(0x91)
    ldh_n_a(0x40)                        # LCDC: pantalla, fondo y tiles en 0x8000

    # Bucle principal: uno por cuadro.
    a.etiqueta("principal")
    a.etiqueta("espera1")
    ldh_a_n(0x44)
    b(0xFE, 144)
    jr(NZ, "espera1")

    # Botones: P1 con bit 5 a 0 elige los botones (A, B, Select, Start); con el 4, la cruceta.
    ld_a(0x10)
    ldh_n_a(0x00)
    for _ in range(4):
        ldh_a_n(0x00)
    b(0x2F)                              # cpl
    b(0xE6, 0x0F)                        # and 0x0F
    b(0x47)                              # ld b, a   (b: A=1 B=2 Select=4 Start=8, pulsado = 1)
    ld_a(0x20)
    ldh_n_a(0x00)
    for _ in range(4):
        ldh_a_n(0x00)
    b(0x2F)
    b(0xE6, 0x0F)
    b(0x4F)                              # ld c, a   (c: Derecha=1 Izquierda=2 Arriba=4 Abajo=8)

    # A: paleta invertida mientras se mantiene.
    b(0x78); b(0xE6, 0x01)               # ld a,b ; and 1
    jr(Z, "paleta_normal")
    ld_a(0x1B)
    jr(0x18, "paleta")
    a.etiqueta("paleta_normal")
    ld_a(0xE4)
    a.etiqueta("paleta")
    ldh_n_a(0x47)

    # Derecha: SCX = 8; B: SCY = 8.
    b(0x79); b(0xE6, 0x01)               # ld a,c ; and 1
    jr(Z, "scx0")
    ld_a(8)
    jr(0x18, "scx")
    a.etiqueta("scx0")
    b(0xAF)
    a.etiqueta("scx")
    ldh_n_a(0x43)
    b(0x78); b(0xE6, 0x02)               # ld a,b ; and 2
    jr(Z, "scy0")
    ld_a(8)
    jr(0x18, "scy")
    a.etiqueta("scy0")
    b(0xAF)
    a.etiqueta("scy")
    ldh_n_a(0x42)

    # Select: silencio (NR51 = 0); si no, todo a los dos lados.
    b(0x78); b(0xE6, 0x04)               # ld a,b ; and 4
    jr(Z, "con_sonido")
    b(0xAF)
    jr(0x18, "nr51")
    a.etiqueta("con_sonido")
    ld_a(0xFF)
    a.etiqueta("nr51")
    ldh_n_a(0x25)

    # Start: «SAGUI» y un contador en la RAM de la batería (0xA000-0xA005).
    b(0x78); b(0xE6, 0x08)               # ld a,b ; and 8
    jr(Z, "sin_start")
    b(0x21, 0x00, 0xA0)                  # ld hl, 0xA000
    b(0x11); a.pend.append((len(a.c), "marca", "abs")); b(0, 0)   # ld de, marca
    b(0x0E, 5)                           # ld c, 5
    a.etiqueta("escribe")
    b(0x1A)                              # ld a, [de]
    b(0x13)                              # inc de
    b(0x22)                              # ld [hl+], a
    b(0x0D)                              # dec c
    jr(NZ, "escribe")
    b(0x7E)                              # ld a, [hl]
    b(0x3C)                              # inc a
    b(0x77)                              # ld [hl], a   (contador en 0xA005)
    a.etiqueta("sin_start")

    # Salir del vblank antes de volver a esperarlo: un cuadro por vuelta.
    a.etiqueta("espera2")
    ldh_a_n(0x44)
    b(0xFE, 144)
    jr(Z, "espera2")
    jp(0xC3, "principal")

    # Datos.
    a.etiqueta("marca")
    b(*MARCA)
    a.etiqueta("tiles")
    b(*([0x00] * 16))                    # tile 0: color 0
    b(*([0xFF] * 16))                    # tile 1: color 3
    b(*([0xFF, 0x00] * 8))               # tile 2: color 1
    b(*([0x00, 0xFF] * 8))               # tile 3: color 2
    return a.resolver()


def construir():
    rom = bytearray(0x8000)
    rom[0x100:0x104] = bytes([0x00, 0xC3, 0x50, 0x01])      # nop ; jp 0x0150
    rom[0x104:0x134] = LOGO
    rom[0x134:0x134 + len(TITULO)] = TITULO
    rom[0x143] = 0x00                                         # solo Game Boy (DMG)
    rom[0x147] = 0x03                                         # MBC1 + RAM + batería
    rom[0x148] = 0x00                                         # 32 KB
    rom[0x149] = 0x02                                         # RAM de 8 KB
    rom[0x14A] = 0x01                                         # fuera de Japón
    rom[0x14B] = 0x00
    rom[0x14C] = 0x00
    prog = programa()
    rom[0x150:0x150 + len(prog)] = prog
    x = 0
    for i in range(0x134, 0x14D):
        x = (x - rom[i] - 1) & 0xFF
    rom[0x14D] = x
    suma = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
    rom[0x14E:0x150] = struct.pack(">H", suma)
    return bytes(rom)


def comprobar(rom):
    """Las dos sumas de la cabecera (y el logo): True si son correctas."""
    x = 0
    for i in range(0x134, 0x14D):
        x = (x - rom[i] - 1) & 0xFF
    suma = (sum(rom) - rom[0x14E] - rom[0x14F]) & 0xFFFF
    return rom[0x104:0x134] == LOGO and rom[0x14D] == x and struct.unpack(">H", rom[0x14E:0x150])[0] == suma


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(__doc__)
        sys.exit(2)
    datos = construir()
    with open(sys.argv[1], "wb") as f:
        f.write(datos)
    print("gnuboy_rom_teste: %s (%d bytes, sumas %s)" % (sys.argv[1], len(datos), "correctas" if comprobar(datos) else "MAL"))
