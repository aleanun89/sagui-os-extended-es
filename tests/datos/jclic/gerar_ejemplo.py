#!/usr/bin/env python3
"""Genera el proyecto JClic de prueba `ejemplo.jclic.zip` (D185).

Es un proyecto propio, con imagenes dibujadas aqui (un PNG y dos GIF sin comprimir LZW de verdad,
suficientes para el lector) y todos los tipos de actividad que el visor juega, mas una de texto
(que el visor debe saltar). Nada viene del repositorio de Clic: sin problemas de licencia.

  python3 gerar_ejemplo.py [salida.jclic.zip]
"""
import struct
import sys
import zipfile
import zlib


def png(l, a, px):
    """px[y][x] = (r, g, b)."""
    raw = b"".join(b"\0" + bytes(c for p in fila for c in p) for fila in px)

    def trozo(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", l, a, 8, 2, 0, 0, 0)) +
            trozo(b"IDAT", zlib.compress(raw)) + trozo(b"IEND", b""))


def gif(l, a, indices, paleta):
    """GIF de 16 colores; el LZW emite un 'clear' cada 14 pixeles (sin compresion real, valido)."""
    paleta = list(paleta) + [(0, 0, 0)] * (16 - len(paleta))
    out = b"GIF89a" + struct.pack("<HHBBB", l, a, 0xF3, 0, 0)
    out += b"".join(bytes(c) for c in paleta)
    out += b"\x2c" + struct.pack("<HHHHB", 0, 0, l, a, 0) + bytes([4])
    bits, nb, datos = 0, 0, bytearray()

    def emitir(cod, n):
        nonlocal bits, nb
        bits |= cod << nb
        nb += n
        while nb >= 8:
            datos.append(bits & 255)
            bits >>= 8
            nb -= 8
    for i in range(0, len(indices), 14):
        emitir(16, 5)
        for v in indices[i:i + 14]:
            emitir(v, 5)
    emitir(16, 5)
    emitir(17, 5)
    if nb:
        datos.append(bits & 255)
    for i in range(0, len(datos), 255):
        trozo = datos[i:i + 255]
        out += bytes([len(trozo)]) + bytes(trozo)
    return out + b"\0\x3b"


def paisaje():
    l, a = 240, 160
    px = []
    for y in range(a):
        fila = []
        for x in range(l):
            c = (60 + y // 3, 140 + y // 4, 230 - y // 5)      # cielo
            if (x - 190) ** 2 + (y - 36) ** 2 < 22 ** 2:
                c = (255, 220, 40)                              # sol
            if y > 110 + 12 * ((x // 40) % 2):
                c = (40, 150 + (x % 60), 60)                    # colinas
            if 40 < x < 110 and 70 < y < 125:
                c = (200, 80, 60)                               # casa
                if 64 < x < 86 and 95 < y:
                    c = (90, 50, 30)                            # puerta
            if 30 < x < 120 and 50 < y < 70 and (y - 50) > abs(x - 75) - 45 + 20:
                c = (120, 40, 30)                               # tejado
            if x % 60 == 0 or y % 80 == 0:
                c = tuple(max(0, v - 25) for v in c)            # una rejilla suave que ayuda a ver las piezas
            fila.append(c)
        px.append(fila)
    return png(l, a, px)


def disco(color, fondo, forma):
    n = 48
    idx = []
    for y in range(n):
        for x in range(n):
            dx, dy = x - 23.5, y - 23.5
            v = 1
            if forma == "sol":
                v = 0 if dx * dx + dy * dy < 18 ** 2 else 1
                if v == 1 and (abs(dx) < 3 or abs(dy) < 3 or abs(abs(dx) - abs(dy)) < 3) and dx * dx + dy * dy < 23 ** 2:
                    v = 0
            elif forma == "luna":
                v = 0 if dx * dx + dy * dy < 18 ** 2 and (dx - 8) ** 2 + (dy + 4) ** 2 > 15 ** 2 else 1
            elif forma == "estrella":
                v = 0 if (abs(dx) + abs(dy) < 20 and (abs(dx) < 6 or abs(dy) < 6 or abs(dx) + abs(dy) < 12)) else 1
            else:
                v = 0 if abs(dx) < 17 and abs(dy) < 17 else 1
            idx.append(v)
    return gif(n, n, idx, [color, fondo])


def estilo(fg="0x000000", bg="0xFFFFFF", tam=20):
    return f'<style><font family="Arial" size="{tam}" bold="true" /><color foreground="{fg}" background="{bg}" /></style>'


def celdas(textos=None, imgs=None, ids=None, cid="primary", cols=None, lin=None, bg="0xFFFFFF", cw=140, ch=80, fg="0x103060"):
    n = len(textos or imgs)
    cols = cols or n
    lin = lin or (n + cols - 1) // cols
    s = f'<cells rows="{lin}" cols="{cols}" cellWidth="{cw}.0" cellHeight="{ch}.0" border="true" id="{cid}">'
    s += estilo(fg, bg, 22) + f'<shaper class="@Rectangular" cols="{cols}" rows="{lin}" />'
    for i in range(n):
        idattr = f' id="{ids[i]}"' if ids else ""
        if imgs:
            s += f'<cell{idattr} image="{imgs[i]}" />'
        else:
            s += f'<cell{idattr}><p>{textos[i]}</p></cell>'
    return s + "</cells>"


def mensajes(inicial, final="Muy bien!"):
    return (f'<messages><cell type="initial">{estilo("0x000000", "0xFFFFCC", 22)}<p>{inicial}</p></cell>'
            f'<cell type="final" image="sol.gif">{estilo("0x006020", "0xCCFFCC", 40)}<p>{final}</p></cell></messages>')


AJUSTES = '<settings margin="8" report="false"><container bgColor="0x99CCFF"><counters time="true" actions="true" score="true" /></container><window bgColor="0xEEEEFF" border="true" /></settings>'


def act(nombre, clase, inicial, cuerpo, desc=""):
    return (f'<activity class="{clase}" name="{nombre}" code=""><description><p>{desc or inicial}</p></description>'
            f'{mensajes(inicial)}{AJUSTES}{cuerpo}</activity>')


def proyecto():
    a = []
    a.append(act("intro", "@panels.InformationScreen", "Proyecto de prueba de SaguiOS",
                 '<cells rows="2" cols="1" cellWidth="600.0" cellHeight="120.0" border="false">'
                 + estilo("0x103060", "0xFFFF99", 36) + '<shaper class="@Rectangular" cols="1" rows="2" />'
                 '<cell><p>Aprende jugando</p></cell><cell image="paisaje.png" /></cells>'))
    a.append(act("puzle", "@puzzles.DoublePuzzle", "Reconstruye el paisaje",
                 '<cells rows="2" cols="3" border="true" image="paisaje.png"><style /><shaper class="@Rectangular" cols="3" rows="2" /></cells>'
                 '<scramble times="31" /><layout position="AB" />'))
    a.append(act("intercambio", "@puzzles.ExchangePuzzle", "Intercambia las piezas",
                 '<cells rows="2" cols="2" border="true" image="paisaje.png"><style /><shaper class="@Rectangular" cols="2" rows="2" /></cells><scramble times="31" />'))
    a.append(act("agujero", "@puzzles.HolePuzzle", "Desliza las piezas hacia el agujero",
                 '<cells rows="3" cols="3" border="true" image="paisaje.png"><style /><shaper class="@Rectangular" cols="3" rows="3" /></cells><scramble times="31" />'))
    a.append(act("asociacion", "@associations.SimpleAssociation", "Une cada operacion con su resultado",
                 celdas(["1 + 1", "2 + 3", "4 + 4", "5 + 5"], cid="primary", cols=1, lin=4, bg="0xFFFFFF")
                 + celdas(["2", "5", "8", "10"], cid="secondary", cols=1, lin=4, bg="0xFFEEAA")
                 + '<scramble times="31" primary="true" secondary="true" /><layout position="AB" />'))
    a.append(act("compleja", "@associations.ComplexAssociation", "Une cada numero con su grupo",
                 celdas(["2", "7", "10", "13", "4", "9"], ids=[0, 1, 0, 1, 0, 1], cid="primary", cols=3, lin=2, cw=100)
                 + celdas(["Pares", "Impares"], cid="secondary", cols=2, lin=1, bg="0xCCEEFF")
                 + '<scramble times="31" primary="true" secondary="true" /><layout position="AUB" />'))
    a.append(act("memoria", "@memory.MemoryGame", "Encuentra las parejas",
                 celdas(imgs=["sol.gif", "luna.gif", "estrella.gif", "cuadro.gif"], cid="primary", cols=2, lin=2, cw=120, ch=120)
                 + celdas(textos=["Sol", "Luna", "Estrella", "Cuadro"], cid="secondary", cols=2, lin=2, cw=120, ch=120, bg="0xFFEEAA")
                 + '<scramble times="31" /><layout position="AB" />'))
    a.append(act("explorar", "@panels.Explore", "Pulsa un dibujo para ver su nombre",
                 celdas(imgs=["sol.gif", "luna.gif"], ids=[0, 1], cid="primary", cols=2, lin=1, cw=120, ch=120)
                 + celdas(textos=["El Sol", "La Luna"], cid="secondary", cols=2, lin=1, cw=120, ch=60, bg="0xFFEEAA")
                 + '<layout position="AUB" />'))
    a.append(act("identificar", "@panels.Identify", "Cual es la estrella?",
                 celdas(imgs=["sol.gif", "estrella.gif", "luna.gif", "cuadro.gif"], ids=[0, 1, 0, 0], cid="primary", cols=2, lin=2, cw=120, ch=120)
                 + '<scramble times="31" primary="true" />'))
    sopa = ["LUNAXXOP", "XXXXXXSA", "SOLXXXOP", "XXXXXXLA", "PANXXXXX", "XXXXPASO"]
    a.append(act("sopa", "@textGrid.WordSearch", "Encuentra las palabras: SOL, LUNA, PAN, PASO",
                 '<textGrid rows="6" columns="8" cellWidth="30.0" cellHeight="30.0" border="false">' + estilo(bg="0xFFFFCC") +
                 "<text>" + "".join(f"<row>{r}</row>" for r in sopa) + "</text></textGrid>"
                 '<clues><clue id="0">SOL</clue><clue id="1">LUNA</clue><clue id="2">PAN</clue><clue id="3">PASO</clue></clues>'))
    cruz = ["*SOL*", "*****", "PAN**", "*****"]
    a.append(act("crucigrama", "@textGrid.CrossWord", "Completa el crucigrama",
                 '<textGrid rows="4" columns="5" cellWidth="40.0" cellHeight="40.0" border="true">' + estilo(bg="0xFFFFFF") +
                 "<text>" + "".join(f"<row>{r}</row>" for r in cruz) + "</text></textGrid>"
                 + '<cells rows="1" cols="2" cellWidth="200.0" cellHeight="40.0" border="true" id="acrossClues">' + estilo()
                 + '<shaper class="@Rectangular" cols="2" rows="1" />'
                 '<cell id="0" item="0"><p>Estrella del dia</p></cell><cell id="2" item="0"><p>Se come con queso</p></cell></cells>'))
    a.append(act("texto", "@text.FillInBlanks", "Esta actividad es de texto: el visor debe saltarla",
                 '<document><section><p><text>Hola </text><target><answer>mundo</answer></target></p></section></document>'))
    seq = "".join(f'<item name="{n}" />' for n in ["intro", "puzle", "intercambio", "agujero", "asociacion", "compleja",
                                                    "memoria", "explorar", "identificar", "sopa", "crucigrama", "texto"])
    xml = ('<?xml version="1.0" encoding="UTF-8"?>\n<JClicProject name="ejemplo" version="0.1.3"><settings>'
           '<title>Aprende jugando (proyecto de prueba de SaguiOS)</title>'
           '<author name="SaguiOS" mail="" /><language>es</language>'
           '<description><p>Proyecto propio de prueba: un ejemplo de cada tipo de actividad que el visor juega.</p></description>'
           '<skin file="@default.xml" /></settings>'
           f'<sequence>{seq}</sequence><activities>{"".join(a)}</activities>'
           '<mediaBag><media name="paisaje.png" file="paisaje.png" /><media name="sol.gif" file="sol.gif" />'
           '<media name="luna.gif" file="luna.gif" /><media name="estrella.gif" file="estrella.gif" />'
           '<media name="cuadro.gif" file="cuadro.gif" /></mediaBag></JClicProject>\n')
    return xml


def main():
    salida = sys.argv[1] if len(sys.argv) > 1 else "ejemplo.jclic.zip"
    with zipfile.ZipFile(salida, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr(zipfile.ZipInfo("ejemplo.jclic", (2026, 1, 1, 0, 0, 0)), proyecto().encode("utf-8"), zipfile.ZIP_DEFLATED)
        z.writestr(zipfile.ZipInfo("paisaje.png", (2026, 1, 1, 0, 0, 0)), paisaje(), zipfile.ZIP_DEFLATED)
        z.writestr(zipfile.ZipInfo("sol.gif", (2026, 1, 1, 0, 0, 0)), disco((255, 190, 0), (200, 230, 255), "sol"), zipfile.ZIP_DEFLATED)
        z.writestr(zipfile.ZipInfo("luna.gif", (2026, 1, 1, 0, 0, 0)), disco((240, 240, 200), (20, 20, 80), "luna"), zipfile.ZIP_DEFLATED)
        z.writestr(zipfile.ZipInfo("estrella.gif", (2026, 1, 1, 0, 0, 0)), disco((255, 255, 60), (60, 0, 100), "estrella"), zipfile.ZIP_DEFLATED)
        z.writestr(zipfile.ZipInfo("cuadro.gif", (2026, 1, 1, 0, 0, 0)), disco((60, 160, 60), (230, 255, 230), "cuadro"), zipfile.ZIP_DEFLATED)
    print("escrito", salida)


if __name__ == "__main__":
    main()
