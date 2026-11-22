#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""tzar_sintetico.py - una instalación de Tzar SINTÉTICA para las pruebas (D417, tests/h417_tzar.py).

Uso: tzar_sintetico.py <carpeta>

Construye desde cero, con los formatos descritos en docs/tzar.md, lo mínimo para probar los lectores y el visor
sin los datos del juego (que nunca van a las pruebas): paquetes Packs/*.wdt (HMMSYS PackFile comprimido con LZSS
por trozos), una carpeta suelta DATA/, IMAGES.INI y DATA/OBJECTS/*.INI, BMP de 8 y 24 bits, sprites RLE de los
formatos 3 y 7, un bioma «grass» con agua, costa animada, árboles y acantilados, un mapa HPFS (Maps/PRUEBA.wmp,
con los bloques desordenados) y una campaña con ese mapa anidado (Campaign/PRUEBA.wcm, con indirección).
En prueba/ deja casos para el arnés: LZSS con su texto, un HPFS con un ciclo y un paquete con una ruta «..».
Todos los dibujos son figuras geométricas hechas aquí.
"""
import os
import struct
import sys

# ------------------------------------------------------------------ LZSS (algoritmo 0xc4: distancias de 12 bits, longitudes de 4)

DBITS, LBITS = 12, 4


class Bits:
    def __init__(self):
        self.out, self.acc, self.n = bytearray(), 0, 0

    def put(self, v, c):
        for i in range(c - 1, -1, -1):
            self.acc = (self.acc << 1) | ((v >> i) & 1)
            self.n += 1
            if self.n == 8:
                self.out.append(self.acc)
                self.acc = self.n = 0

    def fim(self):
        if self.n:
            self.out.append(self.acc << (8 - self.n))
        return bytes(self.out)


def lzss_trozo(d):
    """Codificador voraz: ceros (distancia codificada 0), referencias de hasta 17 bytes y literales."""
    b = Bits()
    i, maxl, janela = 0, (1 << LBITS) + 1, (1 << DBITS) - 1
    pos = {}
    while i < len(d):
        melhor, dist = 0, 0
        if i + 1 < len(d) and d[i] == 0 and d[i + 1] == 0:
            n = 0
            while i + n < len(d) and d[i + n] == 0 and n < maxl:
                n += 1
            melhor, dist = n, 0
        else:
            for j in reversed(pos.get(bytes(d[i:i + 3]), [])[-16:]):
                if i - j > janela:
                    continue
                n = 0
                while i + n < len(d) and n < maxl and d[j + n] == d[i + n]:
                    n += 1
                if n > melhor:
                    melhor, dist = n, i - j
        if melhor >= 2:
            b.put(0, 1)
            b.put(0 if dist == 0 else (1 << DBITS) - dist, DBITS)
            b.put(melhor - 2, LBITS)
            passo = melhor
        else:
            b.put(1, 1)
            b.put(d[i], 8)
            passo = 1
        for k in range(i, i + passo):
            pos.setdefault(bytes(d[k:k + 3]), []).append(k)
        i += passo
    return b.fim()


def lzss(d, trozo=32768, crus=()):
    """Fichero LZSS por trozos; los trozos de `crus` (o los que no ganan nada) van sin comprimir."""
    n = (len(d) + trozo - 1) // trozo
    partes = []
    for i in range(n):
        p = d[i * trozo:(i + 1) * trozo]
        c = lzss_trozo(p)
        if i in crus and len(p) == trozo:
            c = p
        elif len(c) == trozo:   # un trozo comprimido del tamaño exacto se leería como crudo
            c = c + b"\0"
        partes.append(c)
    base = 14 + 4 * n
    cab = b"LZSS" + struct.pack("<II", len(d), trozo) + bytes([0xc4, 0])
    offs, a = [], base
    for p in partes:
        offs.append(a)
        a += len(p)
    return cab + b"".join(struct.pack("<I", o) for o in offs) + b"".join(partes)


# ------------------------------------------------------------------ HMMSYS PackFile

def hmmsys(fich):
    """fich: lista de (nombre con '\\', bytes), ordenada como el original (prefijos compartidos)."""
    fich = sorted(fich, key=lambda x: x[0].upper())
    dirb = bytearray()
    ant = b""
    nomes = []
    for nome, _ in fich:
        n = nome.encode("latin-1")
        pre = 0
        while pre < min(len(n), len(ant), 255) and n[pre] == ant[pre]:
            pre += 1
        nomes.append((n, pre))
        ant = n
    tam_dir = 0x28 + sum(2 + len(n) - p + 8 for n, p in nomes)
    a = tam_dir
    for (n, pre), (_, dados) in zip(nomes, fich):
        dirb += bytes([len(n), pre]) + n[pre:] + struct.pack("<II", a, len(dados))
        a += len(dados)
    cab = b"HMMSYS PackFile".ljust(0x20, b"\0") + struct.pack("<II", len(fich), 0)
    return cab + bytes(dirb) + b"".join(d for _, d in fich)


# ------------------------------------------------------------------ imágenes

def bmp24(w, h, px):
    """px(x, y) -> (r, g, b); de abajo arriba, filas alineadas a 4."""
    stride = (w * 3 + 3) // 4 * 4
    filas = []
    for y in range(h - 1, -1, -1):
        f = bytearray()
        for x in range(w):
            r, g, b = px(x, y)
            f += bytes((b, g, r))
        filas.append(bytes(f.ljust(stride, b"\0")))
    dados = b"".join(filas)
    return b"BM" + struct.pack("<IHHIIiiHHIIiiII", 54 + len(dados), 0, 0, 54, 40, w, h, 1, 24, 0, len(dados), 2835, 2835, 0, 0) + dados


def paleta_tzar():
    """256 colores BGRA: 1 verde puro (transparente), 112-239 las rampas de los 8 reinos (16 tonos; la del color 1
    empieza en (128, 0, 0)), 240-255 los grises claros que se cambian por el color del reino."""
    p = [(0, 0, 0)] * 256
    p[1] = (0, 255, 0)
    p[2] = (120, 80, 40)
    p[3] = (200, 200, 60)
    for k in range(8):
        for t in range(16):
            base = [(40, 40, 200), (128, 0, 0), (0, 128, 0), (200, 200, 0), (0, 160, 160), (160, 0, 160), (220, 120, 0), (90, 90, 90)][k]
            p[112 + k * 16 + t] = tuple(min(255, c + t * 4) if t else c for c in base)
    for t in range(16):
        p[240 + t] = (224 - t * 4,) * 3
    return b"".join(bytes((b, g, r, 0)) for r, g, b in p)


def bmp8(w, h, idx):
    stride = (w + 3) // 4 * 4
    filas = [bytes(idx(x, y) for x in range(w)).ljust(stride, b"\0") for y in range(h - 1, -1, -1)]
    dados = b"".join(filas)
    pal = paleta_tzar()
    off = 54 + len(pal)
    return b"BM" + struct.pack("<IHHIIiiHHIIiiII", off + len(dados), 0, 0, off, 40, w, h, 1, 8, 0, len(dados), 2835, 2835, 256, 0) + pal + dados


def rle(cols, rows, w, fh, formato, filas):
    """filas: lista (h = cols*rows*fh) de listas de tramos (salto, [píxeles]); los cuadros, dirección antes que tiempo."""
    h = cols * rows * fh
    assert len(filas) == h
    cab = bytearray(38)
    cab[0:4] = b" elr"
    struct.pack_into("<II", cab, 4, cols, rows)
    cab[16:20] = b"RLE\0"
    struct.pack_into("<HHH", cab, 20, w, h, formato)
    struct.pack_into("<I", cab, 30, 0)
    corpo, tabela = bytearray(), bytearray()
    for tramos in filas:
        tabela += struct.pack("<I", h * 4 + len(corpo))
        x = 0
        for salto, px in tramos:
            corpo += bytes((salto, len(px)))
            for v in px:
                corpo += struct.pack("<H", v) if formato == 3 else b""
            x += salto + len(px)
        if x < w:
            corpo += bytes((w - x, 0))
    return bytes(cab) + bytes(tabela) + bytes(corpo)


# ------------------------------------------------------------------ HPFS

class Hpfs:
    """Bloques de 512: 0 cabecera, 1 inodo del directorio raíz. Los datos de cada fichero se reparten en orden
    inverso (para que el lector siga la tabla y no los desplazamientos)."""

    def __init__(self):
        self.blocos = [bytearray(512), bytearray(512)]

    def novo(self):
        self.blocos.append(bytearray(512))
        return len(self.blocos) - 1

    def ficheiro(self, dados, inodo=None, inverso=True):
        n = (len(dados) + 511) // 512
        dad = [self.novo() for _ in range(n)]
        ordem = list(reversed(dad)) if inverso else dad
        for i, b in enumerate(ordem):
            self.blocos[b][:] = dados[i * 512:(i + 1) * 512].ljust(512, b"\0")
        no = inodo if inodo is not None else self.novo()
        prof, ptr = 0, ordem
        if n > 126:   # una página indirecta por cada 128 bloques
            prof = 1
            paginas = []
            for k in range(0, n, 128):
                pg = self.novo()
                self.blocos[pg][:] = b"".join(struct.pack("<I", b) for b in ordem[k:k + 128]).ljust(512, b"\0")
                paginas.append(pg)
            ptr = paginas
        assert len(ptr) <= 126
        self.blocos[no][:] = (struct.pack("<II", len(dados), prof) + b"".join(struct.pack("<I", b) for b in ptr)).ljust(512, b"\0")
        return no

    def diretorio(self, entradas, inodo=None):
        """entradas: (nombre, inodo, tipo)."""
        d = b"".join(struct.pack("<IHH", ino, tipo, len(n.encode("latin-1"))) + n.encode("latin-1") for n, ino, tipo in entradas)
        return self.ficheiro(d, inodo, inverso=False)

    def bytes(self):
        self.blocos[0][:] = (b"HPFS" + struct.pack("<II", 512, len(self.blocos))).ljust(512, b"\0")
        return b"".join(bytes(b) for b in self.blocos)


def contenedor(ficheiros):
    """ficheiros: {'MAPS/x': bytes}; todos en la carpeta MAPS."""
    h = Hpfs()
    ent = []
    for nome, dados in ficheiros.items():
        ent.append((nome.split("/", 1)[1], h.ficheiro(dados), 0))
    pasta = h.diretorio(ent)
    h.diretorio([("MAPS", pasta, 1)], inodo=1)
    return h.bytes()


# ------------------------------------------------------------------ el mapa

AGUA, ERVA = 0, 1
LADO = 32


def casilla(a, b, forma=0, penhasco=False, arvore=0):
    return (arvore << 11) | (0x400 if penhasco else 0) | (a << 7) | (b << 4) | forma


def mapa_prueba():
    c = []
    for y in range(LADO):
        for x in range(LADO):
            if x < 6 and y < 6:
                v = casilla(AGUA, AGUA)
            elif x == 6 and y < 7 or y == 6 and x < 7:
                v = casilla(ERVA, AGUA, (x + y) % 16)
            else:
                v = casilla(ERVA, ERVA)
            if (x, y) in ((10, 10), (11, 10), (12, 11), (25, 26), (26, 27)):
                v = casilla(ERVA, ERVA, arvore=1 + (x + y) % 31)
            if 16 <= x <= 18 and y == 16:
                v = casilla(ERVA, x - 16, forma=2, penhasco=True)
            c.append(v)
    tiles = b"".join(struct.pack("<H", v) for v in c)

    def objeto(classe, dono, x, y, flags=0, civ=1, variante=0, ident=1):
        base = 6 if classe >= 74 else 5 if classe >= 44 else 4 if classe >= 39 else 1
        bases = [0, 0, 1, 3, 39, 44, 74]
        cod = (base << 16) | (classe - bases[base])
        guardada = 82 if classe in (107, 108) else 79 if classe in (81, 103, 104, 106) else 67 if classe == 68 else classe
        p = bytearray(60)
        struct.pack_into("<HHIHHBBHHHH", p, 0, 60, guardada, flags, x, y, dono, civ, ident, variante, 100, 100)
        return bytes([0]) + struct.pack("<II", cod, len(p)) + bytes(p)

    objs = [
        objeto(1, 1, 4, 5, ident=1),                       # peón del reino 1 (color 3)
        objeto(8, 2, 8, 8, variante=0x8003, ident=2),     # caballero del reino 2, mirando a la dirección 3
        objeto(66, 1, 12, 4, ident=3),                    # casa con el estandarte del reino
        objeto(43, 2, 20, 20, ident=4),                   # torre de cohetes: sin definición (cuenta «sin dibujo»)
        objeto(1, 1, 2, 2, flags=0x4000, ident=5),        # inactiva: no se dibuja
        objeto(1, 3, 14, 14, ident=6),                    # de un reino que no juega: no se crea
        objeto(77, 0, 15, 15, ident=7),                   # marca del motor (fog_revealer): sin dibujo propio
    ]
    cab = bytearray(0x200)
    cab[0:4] = b"2mzt"
    struct.pack_into("<HI", cab, 6, LADO, len(objs))
    cab[0x4c], cab[0x4d] = 2, 2
    cab[0xd6:0xd6 + 5] = b"grass"
    reinos = bytearray()
    for i in range(8):
        r = bytearray(152)
        r[0:7] = b"Reino %d" % (i + 1)
        r[64] = 1 if i < 2 else 0
        r[66] = 3 if i == 0 else i + 1
        r[88] = 1
        reinos += r
    return contenedor({
        "MAPS/PRUEBA.header": bytes(cab),
        "MAPS/PRUEBA.tiles": tiles,
        "MAPS/PRUEBA.objects": b"".join(objs),
        "MAPS/PRUEBA.kingdominfo": bytes(reinos),
    })


# ------------------------------------------------------------------ animación (fase 3)

INI_ANIM = b"""
[knight_walk]
image=anims\\knight_walk.bmp
cols=8
rows=4
object=0,0,16,16
anim_time=400
delay=100,100,100,100
[knight_attack]
image=anims\\knight_attack.bmp
cols=8
rows=3
object=0,0,16,16
anim_time=600
[knight_die]
image=anims\\knight_die.bmp
cols=8
rows=3
object=0,0,16,16
anim_time=300
[flame]
image=anims\\flame.bmp
cols=1
rows=4
fps=8
object=0,0,16,16
[fish_1]
image=anims\\fish.bmp
rows=5
object=0,0,16,16
[fish_2]
image=anims\\fish.bmp
rows=5
object=0,0,16,16
[fish_3]
image=anims\\fish.bmp
rows=5
object=0,0,16,16
[fish_4]
image=anims\\fish.bmp
rows=5
object=0,0,16,16
[fish_5]
image=anims\\fish.bmp
rows=5
object=0,0,16,16
[eagle_fly]
image=anims\\eagle.bmp
cols=8
rows=2
object=0,0,16,16
anim_time=400
[gw0_anim]
image=anims\\gate.bmp
cols=2
rows=9
object=0,0,32,32
[gates c]
image=anims\\gates_c.bmp
cols=8
rows=16
object=0,0,8,8
[build3x3]
image=anims\\build3x3.bmp
rows=4
object=0,0,96,96
[fire_small]
image=anims\\fire.bmp
rows=5
hotspot=2,2
"""


def cuadros(cols, rows, w, h, cor):
    """Hoja BMP de 24 bits de cols x rows cuadros de w x h: cor(c, r) da el color del cuadro; x == 0 verde (transparente)."""
    return bmp24(cols * w, rows * h, lambda x, y: (0, 255, 0) if x % w == 0 else cor(x // w, y // h))


def fnt():
    """Fuente de cobertura de 8x8 con tres glifos: 0 vacío, 1 lleno (7) y 2 a medias (3); 'A' = 1, 'B' = 2 (avance 6)."""
    cab = bytearray(562)
    struct.pack_into("<H", cab, 32, 8)
    for i in range(256):
        cab[50 + i] = 8
        cab[306 + i] = 0
    cab[50 + ord("B")] = 6
    cab[306 + ord("A")] = 1
    cab[306 + ord("B")] = 2
    return bytes(cab) + bmp8(8, 24, lambda x, y: 0 if y < 8 else 7 if y < 16 else 3)


def animacion(data, images):
    images.append(("IMAGES\\ANIMS\\KNIGHT_WALK.BMP", cuadros(8, 4, 16, 16, lambda c, r: (r * 60 + 10, c * 30 + 10, 100))))
    images.append(("IMAGES\\ANIMS\\KNIGHT_ATTACK.BMP", cuadros(8, 3, 16, 16, lambda c, r: (200, r * 60 + 10, c * 30 + 10))))
    images.append(("IMAGES\\ANIMS\\KNIGHT_DIE.BMP", cuadros(8, 3, 16, 16, lambda c, r: (r * 90 + 10, 10, c * 30 + 10))))
    images.append(("IMAGES\\ANIMS\\FLAME.BMP", cuadros(1, 4, 16, 16, lambda c, r: (200 + r * 10, 80, 0))))
    images.append(("IMAGES\\ANIMS\\FISH.BMP", cuadros(1, 5, 16, 16, lambda c, r: (10, 20 + r * 40, 200))))
    images.append(("IMAGES\\ANIMS\\EAGLE.BMP", cuadros(8, 2, 16, 16, lambda c, r: (100 + r * 50, 100, c * 30 + 10))))
    images.append(("IMAGES\\ANIMS\\GATE.BMP", cuadros(2, 9, 32, 32, lambda c, r: (90, 60 + r * 10, 30 + c * 40))))
    images.append(("IMAGES\\ANIMS\\GATES_C.BMP", bmp8(8 * 8, 16 * 8, lambda x, y: 245)))
    images.append(("IMAGES\\ANIMS\\BUILD3X3.BMP", cuadros(1, 4, 96, 96, lambda c, r: (50 + r * 50, 50, 50))))
    images.append(("IMAGES\\ANIMS\\FIRE.BMP", cuadros(1, 5, 16, 16, lambda c, r: (250, 40 * r, 0))))
    data.append(("LOCAL\\STRINGS.INI", b"[general]\nyes = S\xed\n[objects]\nknight e = Caballero\nhouse e = Casa \x96 grande\nhouse a = Casa asi\xe1tica\n"
                 b"peon e = Campesino\\nde prueba\n"))
    data.append(("LOCAL\\STRINGS2.INI", b"[objects]\nknight e = Caballero andante\n"))
    data.append(("LOCAL\\FONTS\\SMALL.FNT", fnt()))


def objeto_reg(classe, dono, x, y, flags=0, civ=1, variante=0, ident=1, imagem=b"", extra=()):
    base = 6 if classe >= 74 else 5 if classe >= 44 else 4 if classe >= 39 else 1
    bases = [0, 0, 1, 3, 39, 44, 74]
    cod = (base << 16) | (classe - bases[base])
    guardada = 82 if classe in (107, 108) else 79 if classe in (81, 103, 104, 106) else 67 if classe == 68 else classe
    p = bytearray(96)
    struct.pack_into("<HHIHHBBHHHH", p, 0, 60, guardada, flags, x, y, dono, civ, ident, variante, 100, 100)
    p[56:56 + len(imagem)] = imagem
    for pos, val in extra:
        p[pos] = val
    return bytes([0]) + struct.pack("<II", cod, len(p)) + bytes(p)


def mapa_anim():
    """Mapa de pruebas de la fase 3 (en prueba/, no cuenta como mapa de la instalación): hierba con un caballero, una
    llama, un parche de peces, un águila, una puerta de madera y una casa."""
    tiles = b"".join(struct.pack("<H", casilla(ERVA, ERVA)) for _ in range(LADO * LADO))
    objs = [
        objeto_reg(8, 1, 4, 4, ident=1),
        objeto_reg(82, 0, 6, 6, ident=2, imagem=b"flame"),
        objeto_reg(115, 0, 8, 8, ident=9),
        objeto_reg(119, 0, 10, 10, ident=10),
        objeto_reg(67, 1, 12, 12, ident=11),
        objeto_reg(66, 1, 14, 14, ident=12),
    ]
    cab = bytearray(0x200)
    cab[0:4] = b"2mzt"
    struct.pack_into("<HI", cab, 6, LADO, len(objs))
    cab[0x4c], cab[0x4d] = 2, 2
    cab[0xd6:0xd6 + 5] = b"grass"
    reinos = bytearray()
    for i in range(8):
        r = bytearray(152)
        r[0:7] = b"Reino %d" % (i + 1)
        r[64] = 1 if i < 2 else 0
        r[66] = 3 if i == 0 else i + 1
        r[88] = 1
        reinos += r
    return contenedor({"MAPS/ANIM.header": bytes(cab), "MAPS/ANIM.tiles": tiles, "MAPS/ANIM.objects": b"".join(objs),
                       "MAPS/ANIM.kingdominfo": bytes(reinos)})


# ------------------------------------------------------------------ los datos

COR_AGUA, COR_ERVA = (32, 64, 192), (32, 160, 32)


def dados_simulacao(data):
    """Tablas y menús mínimos para la simulación (fase 5): unidades, edificios, órdenes, menús, una tecnología y RES.INI.
    Las clases nuevas (castillo, cuartel, mina, infante, arquero) solo están en prueba/sim.wmp."""
    t = "\t"
    unid = [["UNIT", "CLASS", "COST", "TIME", "SIGHT", "RANGE", "HEALTH", "ATTACK", "DEFENCE", "SPEED", "RATE", "BUILD AT", "DEPEND ON"], [""] * 13,
            ["Peon", "peon", "50f", "8", "5", "", "50", "5", "0", "4", "3", "Castle", "*"],
            ["Footman", "footman", "50f", "12", "5", "", "65", "6", "3", "4", "5", "Barracks", "*"],
            ["Archer", "archer", '"40f, 20w"', "12", "10", "6", "50", "5", "0", "4", "4", "Barracks", "*"],
            ["Knight", "knight", '"70g,120f"', "22", "5", "", "120", "12", "6", "6", "4", "Barracks", "*"],
            # v8.5.5.2: comercio, objetos, taberna y hechizos (tests/tzar_extras.c)
            ["Trader", "trader", "100g", "10", "5", "", "40", "0", "0", "4", "3", "Castle", "*"],
            ["Mage", "mage", '"60g,20f"', "20", "6", "6", "40", "8", "0", "4", "4", "Castle", "*"],
            ["Monk", "monk", "40g", "15", "5", "", "40", "4", "0", "4", "4", "Castle", "*"],
            ["Priest", "priest", "80g", "20", "5", "", "40", "4", "0", "4", "4", "Castle", "*"],
            ["Ninja", "ninja", "90g", "20", "6", "", "50", "10", "0", "5", "4", "Castle", "*"],
            ["Spy", "spy", "70g", "20", "6", "", "30", "2", "0", "5", "4", "Castle", "*"],
            ["Skeleton", "skeleton", "10g", "5", "5", "", "30", "5", "0", "4", "4", "Castle", "*"],
            ["Crusader", "crusader", "10g", "5", "5", "", "60", "8", "2", "4", "4", "Castle", "*"],
            ["Jihad warrior", "jihad_warrior", "10g", "5", "5", "", "60", "8", "2", "4", "4", "Castle", "*"],
            ["Catapult", "catapult", "100g", "20", "6", "8", "60", "30", "0", "3", "6", "Castle", "*"]]
    edif = [["BUILDING", "CLASS", "COST", "TIME", "SIGHT", "RESISTANCE", "HEALTH", "EMIT (own/other)", "DEPENDS ON"], [""] * 9,
            ["Castle", "castle", '"1500s,1500w"', "120", "12", "1000", "8000", '"+400,-200"', "*"],
            ["House", "house", "75w", "20", "4", "50", "500", "*", "*"],
            ["Barracks", "barrack", '"200s,200f"', "60", "6", "500", "1400", "*", "*"],
            ["Market", "market", "150w", "40", "5", "200", "800", "*", "*"],
            ["Tavern", "tavern", "150w", "40", "5", "200", "800", "*", "*"],
            ["Cathedral", "cathedral", "300w", "60", "6", "400", "1200", "*", "*"],
            ["Academy", "academy", "300w", "60", "6", "400", "1200", "*", "*"]]
    data.append(("DATA\\COMM UNITS.TXT", "\r\n".join(t.join(f) for f in unid).encode() + b"\r\n"))
    data.append(("DATA\\COMM BUILDINGS.TXT", "\r\n".join(t.join(f) for f in edif).encode() + b"\r\n"))
    data.append(("DATA\\OBJECTS\\CASTLE.INI", b"[general]\nclass=castle\ntype=building\n[euro]\nimage=castle\nmenu=e_castle\npop_count=20\n"))
    data.append(("DATA\\OBJECTS\\BARRACK.INI", b"[general]\nclass=barrack\n[comm]\nmenu=e_barrack\n[euro]\nimage=barrack\nmenu=e_barrack\n"))
    data.append(("DATA\\OBJECTS\\MAGE.INI", b"[general]\nclass=mage\n[comm]\nmax_manna=100\nmax_obj_in_holder=4\n"))
    data.append(("DATA\\OBJECTS\\CRUSADE_ANCHOR.INI", b"[general]\nclass=crusadeanchor\n[euro]\ncost=100g\n"))
    data.append(("DATA\\OBJECTS\\RES_GOLD.INI", b"[general]\nclass=res_gold\n[comm]\nimage=res_gold\n"))
    data.append(("DATA\\OBJECTS\\FOOTMAN.INI", b"[general]\nclass=footman\n[comm]\nimage_idle=footman_idle\nmax_obj_in_holder=4\n"))
    data.append(("DATA\\OBJECTS\\ARCHER.INI", b"[general]\nclass=archer\n[comm]\nimage_idle=archer_idle\n"))
    data.append(("DATA\\ORDERS.INI", b"""[move]
ACTION = move
[e_peon]
ACTION = CREATE peon
CLASSNAME = peon
[e_build]
ACTION =
SUBMENU = e_build
[e_house]
ACTION = BUILD house
CLASSNAME = house
[e_barrack]
ACTION = BUILD barrack
CLASSNAME = barrack
[e_footman]
ACTION = CREATE footman
CLASSNAME = footman
[e_archer]
ACTION = CREATE archer
CLASSNAME = archer
[te_goldplus]
ACTION = TECHNOLOGY
CLASSNAME = te_goldplus
""" + b"[spell_firerain]\nACTION = spell spell_firerain\nCLASSNAME = spell_firerain\n\n[spell_lightning]\nACTION = spell spell_lightning\nCLASSNAME = spell_lightning\n\n[spell_heal]\nACTION = spell spell_heal\nCLASSNAME = spell_heal\n\n[spell_spawncarpet]\nACTION = spell spell_spawncarpet\nCLASSNAME = spell_spawncarpet\n\n[spell_spawnbat]\nACTION = spell spell_spawnbat\nCLASSNAME = spell_spawnbat\n\n[spell_bless]\nACTION = spell spell_bless\nCLASSNAME = spell_bless\n\n[spell_freeze]\nACTION = spell spell_freeze\nCLASSNAME = spell_freeze\n\n[spell_berserk]\nACTION = spell spell_berserk\nCLASSNAME = spell_berserk\n\n[spell_cloak]\nACTION = spell spell_cloak\nCLASSNAME = spell_cloak\n\n[spell_spawnstonegolem]\nACTION = spell spell_spawnstonegolem\nCLASSNAME = spell_spawnstonegolem\n\n[spell_magicvision]\nACTION = spell spell_magicvision\nCLASSNAME = spell_magicvision\n\n[spell_raisedead]\nACTION = spell spell_raisedead\nCLASSNAME = spell_raisedead\n\n[spell_hypnotize]\nACTION = spell spell_hypnotize\nCLASSNAME = spell_hypnotize\n\n[spell_turntostone]\nACTION = spell spell_turntostone\nCLASSNAME = spell_turntostone\n\n[spell_whirlpool]\nACTION = spell spell_whirlpool\nCLASSNAME = spell_whirlpool\n\n[spell_spawndragon]\nACTION = spell spell_spawndragon\nCLASSNAME = spell_spawndragon\n\n[spell_flamebolt]\nACTION = spell spell_flamebolt\nCLASSNAME = spell_flamebolt\n\n[spell_raiseexp]\nACTION = spell spell_raiseexp\nCLASSNAME = spell_raiseexp\n\n[spell_spawnjin]\nACTION = spell spell_spawnjin\nCLASSNAME = spell_spawnjin\n\n[spell_curse]\nACTION = spell spell_curse\nCLASSNAME = spell_curse\n"))
    data.append(("DATA\\MENU.INI", b"""[e_peon]
move = M
e_build = B
[e_build]
e_house = H
e_barrack = B
[e_castle]
e_peon = P
te_goldplus = G
[e_barrack]
e_footman = F
e_archer = A
"""))
    data.append(("DATA\\TECHNOLOGIES.INI", b"[te_goldplus]\ndisplayname = Mas oro\ntime = 20000\ngold = 100\n" +
                 b"".join(b"[%s]\ndisplayname = %s\ntime = 10000\ngold = 10\n" % (t, t) for t in
                          [b"te_mercenaries", b"te_heroesquest", b"te_learning", b"te_heroism", b"te_fame", b"te_tradetaxes", b"te_banking",
                           b"te_nobility", b"te_bodybuilding", b"te_tournaments", b"te_warfare"])))
    data.append(("DATA\\RES.INI", b"[Others]\ngolden rocks = 2000\nrocks = 2000\nfish patch = 500\n[Low]\ngold=500\nstone=500\nfood=300\nwood=300\n[Medium]\ngold=1500\nstone=1000\nfood=500\nwood=500\n[High]\ngold=5000\nstone=3000\nfood=1000\nwood=1000\n"))


def mapa_simulacao():
    """prueba/sim.wmp (fase 5): dos reinos con castillo, peones, una mina de oro, árboles, soldados y arqueros."""
    c = []
    for y in range(LADO):
        for x in range(LADO):
            v = casilla(ERVA, ERVA)
            if 2 <= x <= 7 and 12 <= y <= 14:
                v = casilla(ERVA, ERVA, arvore=5 + (x + y) % 20)
            c.append(v)
    tiles = b"".join(struct.pack("<H", v) for v in c)
    objs = [
        objeto_reg(44, 1, 1, 1, ident=1),        # castillo del reino 1
        objeto_reg(1, 1, 8, 3, ident=2),
        objeto_reg(1, 1, 8, 4, ident=3),
        objeto_reg(1, 1, 8, 5, ident=4),
        objeto_reg(76, 0, 12, 2, ident=5),       # mina de oro
        objeto_reg(8, 1, 10, 10, ident=6),       # caballero del reino 1
        objeto_reg(44, 2, 24, 24, ident=7),      # castillo del reino 2
        objeto_reg(3, 2, 20, 20, ident=8),       # infantes del reino 2
        objeto_reg(3, 2, 21, 20, ident=9),
        objeto_reg(4, 2, 18, 12, ident=10),      # arquero del reino 2
        objeto_reg(66, 1, 2, 8, ident=11),       # casa del reino 1
    ]
    cab = bytearray(0x200)
    cab[0:4] = b"2mzt"
    struct.pack_into("<HI", cab, 6, LADO, len(objs))
    cab[0x4c], cab[0x4d] = 2, 2
    cab[0xd6:0xd6 + 5] = b"grass"
    reinos = bytearray()
    for i in range(8):
        r = bytearray(152)
        r[0:7] = b"Reino %d" % (i + 1)
        r[64] = 1 if i < 2 else 0
        r[66] = i + 1
        r[88] = 1
        r[122] = 1 if i == 1 else 0
        struct.pack_into("<IIII", r, 68, 500, 500, 500, 500)
        reinos += r
    return contenedor({"MAPS/SIM.header": bytes(cab), "MAPS/SIM.tiles": tiles, "MAPS/SIM.objects": b"".join(objs),
                       "MAPS/SIM.kingdominfo": bytes(reinos)})


def dados_graficos():
    data, images, terrains = [], [], []
    data.append(("DATA\\SOBRE.TXT", b"data"))
    images.append(("DATA\\SOBRE.TXT", b"images"))
    data.append(("DATA\\IMAGES.INI", b"""; dibujos sint\xe9ticos
[peon_idle]
image=units\\peon.bmp
object=8,40,32,32
[knight_idle]
image=units\\knight.bmp
object=0,0,32,32
[knight_idle_s]
image=units\\knight_s.bmp
[house]
image=building\\house.bmp
object=16,48,64,64
[house_c]
image=building\\house_c.bmp
object=40,0
[castle]
object=0,0,128,128
[barrack]
object=0,0,96,96
[res_gold]
object=0,0,64,64
""" + INI_ANIM))
    data.append(("DATA\\OBJECTS\\PEON.INI", b"[general]\nclass=peon\n[euro]\nimage_idle=peon_idle\nmenu=e_peon\n"))
    dados_simulacao(data)
    data.append(("DATA\\OBJECTS\\KNIGHT.INI", b"[General]\nClass=Knight\n[comm]\nimage_idle=knight_idle\nimage=knight_walk\nimage_attack=knight_attack\nimage_die=knight_die\n"))
    data.append(("DATA\\OBJECTS\\EAGLE.INI", b"[general]\nclass=eagle\n[comm]\nimage_idle=eagle_fly\n"))
    data.append(("DATA\\OBJECTS\\HOUSE.INI", b"[general]\nclass=house\n[euro]\nimage=house\nimage_c=house_c\n"))
    # peón: 8 bits, fondo verde (índice 1), cuerpo marrón y un recuadro gris 240 (el color del reino) en 16..31
    images.append(("IMAGES\\UNITS\\PEON.BMP", bmp8(48, 48, lambda x, y: 240 if 16 <= x < 32 and 16 <= y < 32 else 2 if 12 <= x < 36 and 4 <= y < 44 else 1)))
    # caballero: RLE RGB555 de 2 direcciones x 3 cuadros de 12x10; un píxel rojo en la fila 5, columna 1 + 3*dir + cuadro
    filas = []
    for c in range(2):
        for f in range(3):
            for y in range(10):
                x = 1 + c * 3 + f
                filas.append([(x, [0x7c00])] if y == 5 else [(2, [0x03e0] * 8)] if y in (1, 8) else [])
    images.append(("IMAGES\\UNITS\\KNIGHT.RLE", rle(2, 3, 12, 10, 3, filas)))
    filas = [[(3, [0, 0])] if y == 2 else [] for y in range(10)]
    images.append(("IMAGES\\UNITS\\KNIGHT_S.RLE", rle(1, 1, 12, 10, 7, filas)))
    images.append(("IMAGES\\BUILDING\\HOUSE.BMP", bmp24(64, 64, lambda x, y: (0, 255, 0) if x < 4 or y < 8 else (150, 90, 50) if y > 24 else (180, 40, 30))))
    images.append(("IMAGES\\BUILDING\\HOUSE_C.BMP", bmp8(12, 16, lambda x, y: 1 if x == 0 else 245)))
    animacion(data, images)
    # bioma: agua (2 páginas), hierba, costa con 4 cuadros horizontales, árboles y acantilados
    terrains.append(("IMAGES\\TERRAIN\\GRASS.TERRAIN.INI", b"""[terrain]
dir=grass
clouds=1
[terrainfiles]
00=water.bmp
11=grass.bmp
10=coast.bmp
[terrainsize]
00=2,2
11=2,2
10=2,16,4
[minidatargb]
0=32,64,192
[trees]
img_trees=trees.bmp
img_shadows=trees_s.bmp
pos_tree=0,1
pos_shadow=0,0
[randomterrainset]
base=128,0,3,1
21=100,255,3,1
[randomtreeset]
64=1,255,11,5,1,1,40,235
96=1,255,11,5,1,1,40,235
128=1,255,11,5,1,1,40,235
[treename]
pine=3
oak=2
[treeterrain]
pine=1,2
oak=2,3
"""))
    # mapa aleatorio (v8.5.5.2): formas de inicio (el juego completo las pide con los datos sintéticos)
    def forma_ini(x, y):
        if 9 <= x <= 17 and 9 <= y <= 17:
            return (0, 255, 0)
        if 3 <= x <= 24 and 3 <= y <= 24:
            return (0, 0, 0)
        return (255, 255, 255)
    for fam in (30, 40, 60):
        for n in (1, 2, 3):
            images.append(("IMAGES\\RANDOMMAP\\%d_RND_SHAPE%d.BMP" % (fam, n), bmp24(28, 28, forma_ini)))
    terrains.append(("IMAGES\\TERRAIN\\GRASS\\WATER.BMP", bmp24(64, 128, lambda x, y: COR_AGUA if y < 64 else (40, 70, 200))))
    terrains.append(("IMAGES\\TERRAIN\\GRASS\\GRASS.BMP", bmp24(64, 64, lambda x, y: COR_ERVA if (x * 7 + y * 3) % 11 else (40, 150, 40) if x else COR_ERVA)))
    # máscaras de costa: magenta (terreno a) a la izquierda, verde (terreno b) a la derecha, borde marrón
    terrains.append(("IMAGES\\TERRAIN\\GRASS\\COAST.BMP", bmp24(2 * 4 * 32, 16 * 32, lambda x, y: (255, 0, 255) if x % 32 < 15 else (110, 80, 40) if x % 32 < 17 else (0, 255, 0))))
    terrains.append(("IMAGES\\TERRAIN\\GRASS\\TREES.BMP", bmp24(31 * 32, 64, lambda x, y: (100, 60, 20) if 13 <= x % 32 < 19 and y >= 40 else (20, 100 + (x // 32) * 4, 20) if abs(x % 32 - 16) < 14 - y // 4 + 6 and y < 44 else (0, 255, 0))))
    terrains.append(("IMAGES\\TERRAIN\\GRASS\\TREES_S.BMP", bmp24(31 * 32, 32, lambda x, y: (1, 1, 1) if 4 <= x % 32 < 28 and 8 <= y < 24 else (0, 255, 0))))
    terrains.append(("IMAGES\\TERRAIN\\GRASS\\18.BMP", bmp24(6 * 32, 14 * 32, lambda x, y: (120, 110, 100) if y % 32 > 12 else (0, 255, 0))))
    return data, images, terrains


# ------------------------------------------------------------------ campañas con guion (fase 7)

def _reg(nome, tipo, corpo):
    return nome.encode("latin-1").ljust(32, b"\x5a")[:32].replace(b"\x5a", b"\0", 1) + struct.pack("<HH", tipo, len(corpo)) + corpo


def _z(t):
    return t.encode("latin-1") + b"\0"


def _sel_ids(ids):
    return bytes([0, 0]) + struct.pack("<HH", 0, 0) + bytes([0, 0]) + struct.pack("<H", len(ids)) + b"".join(struct.pack("<H", i) for i in ids) + _z("")


def _area(x1, y1, x2, y2):
    return bytes([0]) + struct.pack("<iiii", x1, y1, x2, y2) + struct.pack("<H", 0) + _z("0")


def _regla(nome, cond, acciones):
    return _reg(nome, len(acciones), _z(cond) + b"".join(_z(a) for a in acciones))


def _objeto_g(classe, dono, x, y, ident, flags=0):
    bases = [0, 0, 1, 3, 39, 44, 74]
    base = 6 if classe >= 74 else 5 if classe >= 44 else 4 if classe >= 39 else 1
    cod = (base << 16) | (classe - bases[base])
    p = bytearray(60)
    struct.pack_into("<HHIHHBBHHHH", p, 0, 60, classe, flags, x, y, dono, 1, ident, 0, 100, 100)
    return bytes([0]) + struct.pack("<II", cod, len(p)) + bytes(p)


def mapa_con_guion(nome, objs, acciones, reglas, extras=None, sucesos=b"", nsucesos=0):
    """Un mapa de 32x32 de hierba con su guion: acciones y reglas ya codificadas (listas de bytes)."""
    tiles = b"".join(struct.pack("<H", casilla(ERVA, ERVA)) for _ in range(LADO * LADO))
    cab = bytearray(0x200)
    cab[0:4] = b"2mzt"
    struct.pack_into("<HI", cab, 6, LADO, len(objs))
    struct.pack_into("<HHHH", cab, 12, len(reglas), len(acciones), nsucesos, 255)
    cab[0x4c], cab[0x4d] = 2, 2
    cab[0xd6:0xd6 + 5] = b"grass"
    cab[0x56:0x56 + len(nome)] = nome.encode("latin-1")
    reinos = bytearray()
    for i in range(8):
        r = bytearray(152)
        r[0:7] = b"Reino %d" % (i + 1)
        r[64] = 1 if i < 2 else 0
        r[66] = 3 if i == 0 else i + 1
        r[68:72] = struct.pack("<I", 1000)
        r[88] = 1
        reinos += r
    f = {
        "MAPS/G.header": bytes(cab), "MAPS/G.tiles": tiles, "MAPS/G.objects": b"".join(objs), "MAPS/G.kingdominfo": bytes(reinos),
        "MAPS/G.events": sucesos, "MAPS/G.actions": b"".join(acciones), "MAPS/G.rules": b"".join(reglas),
        "MAPS/G.description": nome.encode("latin-1") + b" - descripci\xf3n de la misi\xf3n",
    }
    f.update(extras or {})
    return contenedor(f)


def instalacion_campanhas(dst):
    """Una instalación con una campaña de dos misiones con guion (conversación, héroe, variable «#», paso de una a otra) y un
    mapa suelto. Sin gráficos: solo para el catálogo, los guiones y el progreso."""
    hero = _objeto_g(1, 1, 4, 5, 1, flags=0x1000 | 0x02000000)
    cast = _objeto_g(44, 2, 20, 20, 2)
    # misión 1
    acc1 = [
        _reg("Quest", 0, _z("Reune 1500 de oro") + _z("")),
        _reg("Hist", 14, _z("Primera misi\xf3n") + _z("") + _z("USER\\voz1.wav") + _z("") + _z("0") + b"\0"),
        _reg("Marca", 5, _z("#kept = 9\r\n$iniciado = 1")),
        _reg("Ganha", 9, struct.pack("<H", 1) + _z("Victoria|Pasa al final")),
        _reg("Siguiente", 11, _z("MAPS\\02 Final.wmp")),
        _reg("Conv", 20, _z("Cv1")),
    ]
    reg1 = [_regla("Inicio", "1", ["Quest", "Marca", "Hist", "Conv"]), _regla("Fin", "@gold(1) >= 1500", ["Ganha", "Siguiente"])]
    conv = struct.pack("<H", 1) + _z("Cv1") + struct.pack("<H", 1)
    conv += struct.pack("<HHB", 1, 1, 1) + _z("F1") + _z("") + _z("") + _z("") + _z("USER\\f1.wav") + _z("Mi se\xf1or, el reino os espera.") + struct.pack("<H", 0)
    objext = struct.pack("<H", 1) + b"Arturo".ljust(32, b"\0") + b"Rey".ljust(32, b"\0") + b"USER\\GA.bmp".ljust(64, b"\0") + b"USER\\GAL.bmp".ljust(64, b"\0")
    objext = objext.ljust(258, b"\0")
    derrota = b"".join((_z("Los orcos han ca\xeddo") + b"\x01") if i == 1 else (b"\0\x01") for i in range(8))
    dip = bytearray(256)
    struct.pack_into("<I", dip, (0 * 8 + 1) * 4, 0x10)
    struct.pack_into("<I", dip, (1 * 8 + 0) * 4, 0x10)
    m1 = mapa_con_guion("Inicio", [hero, cast], acc1, reg1,
                        {"MAPS/G.conv": conv, "MAPS/G.objext": objext, "MAPS/G.defeat.messages": derrota, "MAPS/G.diplomacy": bytes(dip)})
    # misión 2: hereda la variable y el héroe
    acc2 = [_reg("Herda", 5, _z("$herdou = #kept"))]
    reg2 = [_regla("Herencia", "#kept == 9", ["Herda"])]
    m2 = mapa_con_guion("Final", [hero, cast], acc2, reg2, {"MAPS/G.objext": objext})
    maps_ini = b"[maps]\r\n01 Inicio=yes\r\n02 Final=no\r\n"
    escribir(os.path.join(dst, "Campaign", "HISTORIA.wcm"), contenedor({
        "MAPS/01 Inicio.wmp": m1, "MAPS/02 Final.wmp": m2, "MAPS/maps.ini": maps_ini,
        "MAPS/campaign description.ini": b"La historia de prueba\r\nUna campa\xf1a sint\xe9tica.",
    }))
    escribir(os.path.join(dst, "Maps", "SUELTO.wmp"), mapa_con_guion("Suelto", [cast], [], []))

    # datos para el mapa aleatorio: formas de inicio, el INI del terreno y RES.INI (sintéticos)
    def forma(x, y):
        if 9 <= x <= 17 and 9 <= y <= 17:
            return (0, 255, 0)
        if 3 <= x <= 24 and 3 <= y <= 24:
            return (0, 0, 0)
        return (255, 255, 255)
    imagenes = []
    for fam in (30, 40, 60):
        for n in (1, 2, 3):
            imagenes.append(("IMAGES\\RANDOMMAP\\%d_RND_SHAPE%d.BMP" % (fam, n), bmp24(28, 28, forma)))
    terreno = (b"[terrain]\r\nwater=1\r\nground=7\r\n[randomterrainset]\r\nbase=128,0,3,1\r\n21=100,255,3,1\r\n"
               b"[randomtreeset]\r\n64=1,255,11,5,1,1,40,235\r\n96=1,255,11,5,1,1,40,235\r\n128=1,255,11,5,1,1,40,235\r\n"
               b"[treename]\r\npine=3\r\noak=2\r\n[treeterrain]\r\npine=1,2\r\noak=2,3\r\n")
    imagenes.append(("IMAGES\\TERRAIN\\GRASS.TERRAIN.INI", terreno))
    escribir(os.path.join(dst, "Packs", "images.wdt"), lzss(hmmsys(imagenes), trozo=8192))
    # sonido: WAV y MIDI sintéticos en sounds.wdt (sin LZSS) y los grupos de DATA\\SOUNDS.INI
    import math

    def wav16(frec, seg, taxa=22050, canales=1, bits=16):
        n = int(taxa * seg)
        if bits == 16:
            dat = b"".join(struct.pack("<h", int(12000 * math.sin(2 * math.pi * frec * i / taxa))) * canales for i in range(n))
        else:
            dat = b"".join(bytes([128 + int(100 * math.sin(2 * math.pi * frec * i / taxa))]) * canales for i in range(n))
        fmt = struct.pack("<HHIIHH", 1, canales, taxa, taxa * canales * bits // 8, canales * bits // 8, bits)
        return b"RIFF" + struct.pack("<I", 36 + len(dat)) + b"WAVEfmt " + struct.pack("<I", 16) + fmt + b"data" + struct.pack("<I", len(dat)) + dat

    def midi(notas):
        trk = b"\x00\xc0\x00"
        for n in notas:
            trk += b"\x00\x90" + bytes([n, 100]) + b"\x60\x80" + bytes([n, 0])
        trk += b"\x00\xff\x2f\x00"
        return b"MThd" + struct.pack(">IHHH", 6, 0, 1, 96) + b"MTrk" + struct.pack(">I", len(trk)) + trk

    sons = [
        ("SOUND\\NOTIFY\\CHAT.WAV", wav16(440, 0.4)),
        ("SOUND\\ATTACKS\\HAND1.WAV", wav16(300, 0.3)),
        ("SOUND\\ATTACKS\\HAND2.WAV", wav16(500, 0.3, canales=2)),
        ("SOUND\\ATTACKS\\HAND3.WAV", wav16(700, 0.3, taxa=11025, bits=8)),
        ("MUSIC\\INTRO.MID", midi([60, 64, 67, 72])),
        ("MUSIC\\1.MID", midi([62, 65])),
        ("MUSIC\\2.MID", midi([64, 67])),
        ("MUSIC\\WIN1.MID", midi([72])),
        ("MUSIC\\WIN2.MID", midi([76])),
    ]
    escribir(os.path.join(dst, "Packs", "sounds.wdt"), hmmsys(sons))
    sounds_ini = (b"[priority]\r\nattack_close=170\r\nobj_select=255\r\nobj_order=255\r\nobj_unit_ready=255\r\nchop=120\r\n"
                  b"[hand_attack]\r\nattacks\\hand1.wav = 500\r\nattacks\\hand2.wav = 250\r\nattacks\\hand3.wav = 250;\r\n")
    escribir(os.path.join(dst, "Packs", "data.wdt"), lzss(hmmsys([
        ("DATA\\RES.INI", b"[Low]\r\ngold=500\r\nstone=500\r\nfood=300\r\nwood=300\r\n[Medium]\r\ngold=1500\r\nstone=1000\r\nfood=500\r\nwood=500\r\n[High]\r\ngold=5000\r\nstone=3000\r\nfood=1000\r\nwood=1000\r\n"),
        ("DATA\\SOUNDS.INI", sounds_ini)])))


def escribir(ruta, dados):
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    with open(ruta, "wb") as f:
        f.write(dados)


def main():
    if len(sys.argv) == 3 and sys.argv[1] == "--campanhas":
        instalacion_campanhas(sys.argv[2])
        print("instalación sintética de campañas de Tzar en", sys.argv[2])
        return 0
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    dst = sys.argv[1]
    data, images, terrains = dados_graficos()
    escribir(os.path.join(dst, "Packs", "data.wdt"), lzss(hmmsys(data)))
    escribir(os.path.join(dst, "Packs", "images.wdt"), lzss(hmmsys(images), trozo=4096))
    escribir(os.path.join(dst, "Packs", "terrains.wdt"), hmmsys(terrains))   # sin LZSS, como sounds.wdt
    escribir(os.path.join(dst, "DATA", "SUELTO.TXT"), b"suelto")
    escribir(os.path.join(dst, "DATA", "SOBRE.TXT"), b"suelto")              # pierde frente a los paquetes
    mapa = mapa_prueba()
    escribir(os.path.join(dst, "Maps", "PRUEBA.wmp"), mapa)
    escribir(os.path.join(dst, "Campaign", "PRUEBA.wcm"), contenedor({"MAPS/Mision 1.wmp": mapa, "MAPS/relleno.bin": bytes(range(256)) * 300}))
    # casos del arnés
    texto = b"".join(b"Linea %05d del texto de prueba de Tzar; " % i + (b"\0" * (i % 40)) for i in range(2500))
    import random
    rnd = random.Random(417)
    crudo = bytes(rnd.randrange(256) for _ in range(4096))
    texto = texto[:20000] + crudo + texto[20000:]
    # trozos de 4096: el que empieza en 20480 cae en datos al azar y va crudo
    escribir(os.path.join(dst, "prueba", "lzss.txt"), texto)
    escribir(os.path.join(dst, "prueba", "lzss.bin"), lzss(texto, trozo=4096, crus=(5,)))
    h = Hpfs()
    h.diretorio([("BUCLE", 1, 1)], inodo=1)
    escribir(os.path.join(dst, "prueba", "ciclo.wmp"), h.bytes())
    escribir(os.path.join(dst, "prueba", "ruta_mala.wdt"), hmmsys([("DATA\\..\\..\\FUERA.TXT", b"x")]))
    escribir(os.path.join(dst, "prueba", "anim.wmp"), mapa_anim())
    escribir(os.path.join(dst, "prueba", "sim.wmp"), mapa_simulacao())
    print("instalación sintética de Tzar en", dst)
    return 0


if __name__ == "__main__":
    sys.exit(main())
