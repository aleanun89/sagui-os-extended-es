#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""tzar_ia_sintetico.py - una instalación de Tzar SINTÉTICA y jugable para probar la IA (D417, fase 6; tests/h417_tzar.py ia).

Uso: tzar_ia_sintetico.py <carpeta>

Escribe en <carpeta> una carpeta suelta DATA/ con un catálogo mínimo hecho aquí (tablas de unidades y edificios,
ORDERS.INI, MENU.INI, IMAGES.INI con las huellas, DATA/OBJECTS/*.INI, tecnologías, árbol, RES.INI), guiones de IA
sintéticos (DATA/AI/RANDOM.AIS y los *.AI que incluye) y un mapa HPFS (Maps/IA.wmp) de 48x48 con dos reinos con
castillo y campesinos, minas de oro y piedra y bosques. Nada sale de los datos del juego: los nombres de clase y de
sección son los del formato (docs/tzar.md) y los números, inventados.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from tzar_sintetico import contenedor, casilla, ERVA, AGUA  # noqa: E402

LADO = 48

UNIDADES = """UNIT\tCLASS\tCOST\tTIME\tSIGHT\tRANGE\tHEALTH\tATTACK\tDEFENCE\tSPEED\tRATE\tSPECIAL\tDEPEND ON
\t\t\t\t\t\t\t\t\t\t\t\t
Peon\tpeon\t50f\t6\t5\t\t50\t3\t0\t4\t3\tCastle\t*
Footman\tfootman\t"40f, 20g"\t8\t5\t\t60\t6\t2\t4\t5\tBarracks\t*
Archer\tarcher\t"30f, 30w"\t8\t8\t6\t45\t5\t0\t4\t4\tBarracks\t*
Cow\tcow\t50f\t10\t3\t\t80\t0\t0\t2\t1\tFarm\t*
Mage\tmage\t"60g, 40f"\t10\t8\t6\t40\t2\t0\t4\t4\tCastle\t*
Priest\tpriest\t"40g, 40f"\t10\t8\t\t40\t1\t0\t4\t4\tCastle\t*
Galleon\tgalleon\t"150w, 50g"\t20\t8\t\t600\t0\t5\t5\t4\tHarbor\t*
Fishing Boat\tfishing_boat\t60w\t10\t5\t\t200\t0\t2\t5\t4\tHarbor\t*
"""

EDIFICIOS = """BUILDING\tCLASS\tCOST\tTIME\tSIGHT\tRESISTANCE\tHEALTH\tEMIT (own/other)\tDEPENDS ON
\t\t\t\t\t\t\t\t
Castle\tcastle\t"600w,600s"\t60\t7\t200\t2000\t*\t*
House\thouse\t60w\t10\t4\t50\t400\t*\t*
Lumber Mill\tlumber\t80w\t12\t4\t80\t600\t*\t*
Storage\tstorage\t120w\t12\t5\t80\t600\t*\t*
Farm\tfarm\t120w\t12\t5\t80\t600\t*\t*
Barracks\tbarrack\t"150w,50s"\t20\t5\t100\t900\t*\t*
Field\tfield\t40w\t6\t2\t10\t100\t*\t*
Wooden Tower\ttower_wood\t100w\t15\t8\t80\t500\t*\t*
Harbor\tharbor\t150w\t20\t6\t80\t800\t*\t*
Stone Wall\twall_stone\t5s\t2\t1\t100\t300\t*\t*
"""

# huellas (object = x, y, ancho, alto en píxeles; casillas de 32)
IMAGES = """[s_castle]
object=0,0,160,160
[s_house]
object=0,0,64,64
[s_lumber]
object=0,0,96,64
[s_storage]
object=0,0,96,64
[s_farm]
object=0,0,96,96
[s_barrack]
object=0,0,128,96
[s_field]
object=0,0,64,64
[s_tower_wood]
object=0,0,64,64
[s_res_gold]
object=0,0,64,64
[s_res_stone]
object=0,0,64,64
[s_harbor]
object=0,0,96,96
"""

OBJETOS = {
    "PEON": "[general]\nclass=peon\n[comm]\nmenu=s_peon\n",
    "FOOTMAN": "[general]\nclass=footman\n[comm]\nmenu=s_soldado\n",
    "ARCHER": "[general]\nclass=archer\n[comm]\nmenu=s_soldado\n",
    "COW": "[general]\nclass=cow\n[comm]\nmenu=s_soldado\n",
    "CASTLE": "[general]\nclass=castle\n[comm]\nimage=s_castle\nmenu=s_castle\npop_count=10\nmax_obj_in_holder=10\n",
    "HOUSE": "[general]\nclass=house\n[comm]\nimage=s_house\npop_count=6\n",
    "LUMBER": "[general]\nclass=lumber\n[comm]\nimage=s_lumber\nmenu=s_lumber\n",
    "STORAGE": "[general]\nclass=storage\n[comm]\nimage=s_storage\n",
    "FARM": "[general]\nclass=farm\n[comm]\nimage=s_farm\n",
    "BARRACK": "[general]\nclass=barrack\n[comm]\nimage=s_barrack\nmenu=s_barrack\n",
    "FIELD": "[general]\nclass=field\n[comm]\nimage=s_field\n",
    "TOWER_WOOD": "[general]\nclass=tower_wood\n[comm]\nimage=s_tower_wood\n",
    "RES_GOLD": "[general]\nclass=res_gold\n[comm]\nimage=s_res_gold\n",
    "RES_STONE": "[general]\nclass=res_stone\n[comm]\nimage=s_res_stone\n",
    # D470: magos, flota y muros (los usan el mapa IA2 y tests/tzar_ia_casos.c)
    "MAGE": "[general]\nclass=mage\n[comm]\nmenu=s_mage\nmax_manna=200\n",
    "PRIEST": "[general]\nclass=priest\n[comm]\nmenu=s_priest\nmax_manna=100\n",
    "GALLEON": "[general]\nclass=galleon\n[comm]\nmenu=s_navio\nmax_obj_in_holder=8\n",
    "FISHING_BOAT": "[general]\nclass=fishing_boat\n[comm]\nmenu=s_soldado\n",
    "HARBOR": "[general]\nclass=harbor\n[comm]\nimage=s_harbor\nmenu=s_harbor\n",
    "WALL_STONE": "[general]\nclass=wall_stone\n[comm]\n",
}

MENU = """[s_peon]
move = M
harvest = H
attack = A
repair = R
stop = S
s_build = B
[s_build]
s_house = H
s_lumber = L
s_storage = S
s_farm = F
s_barrack = B
s_field = I
s_tower = T
s_castle_b = C
s_harbor_b = P
[s_castle]
s_peon_c = P
s_mage_c = M
s_priest_c = R
[s_harbor]
s_galleon = G
s_fboat = F
[s_mage]
move = M
attack = A
spell_firerain = R
spell_lightning = L
[s_priest]
move = M
spell_heal = H
spell_bless = B
[s_navio]
move = M
unload = U
[s_barrack]
s_footman = F
s_archer = A
[s_lumber]
te_sierra = S
[s_soldado]
move = M
attack = A
stop = S
"""

ORDENS = """[move]
ACTION = move
[harvest]
ACTION = harvest
[attack]
ACTION = attack
[repair]
ACTION = repair
[stop]
ACTION = stop
[unload]
ACTION = unload
[s_build]
ACTION =
SUBMENU = s_build
"""
for chave, acao, classe in (("s_house", "BUILD house", "house"), ("s_lumber", "BUILD lumber", "lumber"),
                            ("s_storage", "BUILD storage", "storage"), ("s_farm", "BUILD farm", "farm"),
                            ("s_barrack", "BUILD barrack", "barrack"), ("s_field", "BUILD field", "field"),
                            ("s_tower", "BUILD tower_wood", "tower_wood"), ("s_castle_b", "BUILD castle", "castle"),
                            ("s_peon_c", "CREATE peon", "peon"), ("s_footman", "CREATE footman", "footman"),
                            ("s_archer", "CREATE archer", "archer"), ("te_sierra", "technology", "te_sierra"),
                            ("s_harbor_b", "BUILD harbor", "harbor"), ("s_mage_c", "CREATE mage", "mage"),
                            ("s_priest_c", "CREATE priest", "priest"), ("s_galleon", "CREATE galleon", "galleon"),
                            ("s_fboat", "CREATE fishing_boat", "fishing_boat"),
                            ("spell_firerain", "spell spell_firerain", "spell_firerain"),
                            ("spell_lightning", "spell spell_lightning", "spell_lightning"),
                            ("spell_heal", "spell spell_heal", "spell_heal"), ("spell_bless", "spell spell_bless", "spell_bless")):
    ORDENS += "[%s]\nACTION = %s\nCLASSNAME = %s\n" % (chave, acao, classe)

TECNOLOGIAS = "[te_sierra]\nwood = 50\nfood = 50\ntime = 8000\n"
UPGRADES = "[to_wtower2]\nwood = 100\ntime = 5000\n"
ARVORE = "[depend]\narcher = te_sierra\n[required]\nfootman = barrack\n"
RES = "[Low]\ngold=0\nstone=0\nfood=300\nwood=300\n[Others]\ngolden rocks = 5000\nrocks = 5000\n"

# los guiones: lo que usan los de verdad (grupos críticos, gatillos, INCLUDE, NOT, %, TIME, RANDOM, CIV, DIFFICULTY)
GUIAO = """; guion sintético de la IA (pruebas de SaguiOS)
include "casas.ai"
rule have castle end
{ critical rule when castle end
  rule have 6 peon end
  rule when peon 4 have lumber end
  rule when not trigger Exercito, random 50% trigger Sorte say "sorte" end
  rule when peon 6 have te_sierra end
  rule when peon 6, civ european have barracks end
  { critical rule when barracks end
    rule have 4 footman end
    rule when footman 4 trigger Exercito say "exercito" end
    rule when trigger Exercito, difficulty normal have 10% peon end
    rule when time 120000 have 8 footman end
    rule when te_sierra have 2 archers end
  }
}
"""
CASAS = """; las casas cuando falta población
rule Casa when peon 3 have 2 house end
rule when rule Casa, units 12 have 4 houses end
"""


# D470: un guion que se rinde en cuanto no tiene campesinos (como resign.ai: el grupo con HAVE RESIGN)
RENDIR = """; rendición (pruebas de SaguiOS)
{ rule when not peon have resign end
}
"""


def objeto(classe, dono, x, y, ident, civ=1, vida=100):
    base = 6 if classe >= 74 else 5 if classe >= 44 else 4 if classe >= 39 else 1
    bases = [0, 0, 1, 3, 39, 44, 74]
    cod = (base << 16) | (classe - bases[base])
    p = bytearray(80)
    struct.pack_into("<HHIHHBBHHHH", p, 0, 80, classe, 0, x, y, dono, civ, ident, 0, vida, vida)
    return bytes([0]) + struct.pack("<II", cod, len(p)) + bytes(p)


def mapa():
    arvores = set()
    for (x0, y0, x1, y1) in ((1, 18, 9, 28), (38, 18, 46, 28), (18, 1, 28, 6), (18, 41, 28, 46)):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                if (x * 7 + y * 13) % 5 != 0:
                    arvores.add((x, y))
    c = []
    for y in range(LADO):
        for x in range(LADO):
            c.append(casilla(ERVA, ERVA, arvore=8 + (x + y) % 20 if (x, y) in arvores else 0))
    tiles = b"".join(struct.pack("<H", v) for v in c)
    objs, ident = [], [0]

    def obj(classe, dono, x, y, vida=100):
        ident[0] += 1
        objs.append(objeto(classe, dono, x, y, ident[0], vida=vida))
    for dono, (cx, cy) in ((1, (6, 6)), (2, (37, 37))):
        obj(44, dono, cx, cy, 2000)                       # castillo
        for j in range(5):
            obj(1, dono, cx + 6 + j % 3, cy + 1 + j // 3, 50)   # campesinos
    obj(76, 0, 16, 10, 1)                                  # minas de oro y piedra
    obj(78, 0, 10, 15, 1)
    obj(76, 0, 30, 36, 1)
    obj(78, 0, 36, 31, 1)
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
        struct.pack_into("<IIII", r, 68, 300, 300, 600, 900)
        r[88] = 1
        reinos += r
    return contenedor({
        "MAPS/IA.header": bytes(cab),
        "MAPS/IA.tiles": tiles,
        "MAPS/IA.objects": b"".join(objs),
        "MAPS/IA.kingdominfo": bytes(reinos),
    })


def mapa_agua():
    """D470: 64x64, dos islas separadas por un canal de agua (x 27-36, con costa en 26 y 37); los reinos 1 (izquierda) y
    2 (derecha) con su castillo y el 1 con tres campesinos. Lo demás (barcos, muros, magos) lo pone tests/tzar_ia_casos.c."""
    lado = 64
    c = []
    for y in range(lado):
        for x in range(lado):
            if 27 <= x <= 36:
                v = casilla(AGUA, AGUA)
            elif x in (26, 37):
                v = casilla(AGUA, ERVA, (x + y) % 16)   # costa: ni barcos ni unidades de tierra
            else:
                v = casilla(ERVA, ERVA)
            c.append(v)
    tiles = b"".join(struct.pack("<H", v) for v in c)
    objs = [objeto(44, 1, 4, 4, 1, vida=2000), objeto(44, 2, 50, 8, 2, vida=2000)]
    for j in range(3):
        objs.append(objeto(1, 1, 10 + j, 6, 3 + j, vida=50))
    cab = bytearray(0x200)
    cab[0:4] = b"2mzt"
    struct.pack_into("<HI", cab, 6, lado, len(objs))
    cab[0x4c], cab[0x4d] = 2, 2
    cab[0xd6:0xd6 + 5] = b"grass"
    reinos = bytearray()
    for i in range(8):
        r = bytearray(152)
        r[0:7] = b"Reino %d" % (i + 1)
        r[64] = 1 if i < 2 else 0
        r[66] = i + 1
        struct.pack_into("<IIII", r, 68, 3000, 3000, 3000, 3000)
        r[88] = 1
        reinos += r
    return contenedor({
        "MAPS/IA2.header": bytes(cab),
        "MAPS/IA2.tiles": tiles,
        "MAPS/IA2.objects": b"".join(objs),
        "MAPS/IA2.kingdominfo": bytes(reinos),
    })


def escribir(ruta, dados):
    os.makedirs(os.path.dirname(ruta), exist_ok=True)
    with open(ruta, "wb") as f:
        f.write(dados if isinstance(dados, bytes) else dados.encode("latin-1"))


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    d = sys.argv[1]
    escribir(os.path.join(d, "DATA", "COMM UNITS.TXT"), UNIDADES.replace("\n", "\r\n"))
    escribir(os.path.join(d, "DATA", "COMM BUILDINGS.TXT"), EDIFICIOS.replace("\n", "\r\n"))
    escribir(os.path.join(d, "DATA", "IMAGES.INI"), IMAGES)
    for n, t in OBJETOS.items():
        escribir(os.path.join(d, "DATA", "OBJECTS", n + ".INI"), t)
    escribir(os.path.join(d, "DATA", "MENU.INI"), MENU)
    escribir(os.path.join(d, "DATA", "ORDERS.INI"), ORDENS)
    escribir(os.path.join(d, "DATA", "TECHNOLOGIES.INI"), TECNOLOGIAS)
    escribir(os.path.join(d, "DATA", "UPGRADES.INI"), UPGRADES)
    escribir(os.path.join(d, "DATA", "TECHTREE EURO.INI"), ARVORE)
    escribir(os.path.join(d, "DATA", "RES.INI"), RES)
    escribir(os.path.join(d, "DATA", "AI", "RANDOM.AIS"), GUIAO)
    escribir(os.path.join(d, "DATA", "AI", "CASAS.AI"), CASAS)
    escribir(os.path.join(d, "Maps", "IA.wmp"), mapa())
    escribir(os.path.join(d, "Maps", "IA2.wmp"), mapa_agua())
    escribir(os.path.join(d, "DATA", "AI", "RENDIR.AIS"), RENDIR)
    print("instalación sintética de la IA de Tzar en", d)
    return 0


if __name__ == "__main__":
    sys.exit(main())
