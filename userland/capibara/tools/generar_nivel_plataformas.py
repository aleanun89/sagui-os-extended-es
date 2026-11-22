#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Genera escenas/nivel1.esc del ejemplo de plataformas (Capibara). El resultado es texto legible y se puede editar a
mano; este guion solo ahorra teclear las casillas. Uso: generar_nivel_plataformas.py carpeta_del_ejemplo"""
import os, sys

W, H = 80, 14           # casillas
SUELO = 11              # fila de la hierba
G, D = 0, 1             # tipos internos: hierba, tierra

def cargar():
    suelo = [[None] * W for _ in range(H)]
    deco = [[None] * W for _ in range(H)]
    def tramo(x0, x1, fila=SUELO, grosor=None):
        """un tramo de terreno con hierba arriba y tierra debajo (hasta el fondo si grosor es None)"""
        for x in range(x0, x1 + 1):
            izq, der = x == x0, x == x1
            suelo[fila][x] = 0 if izq else 3 if der else (1 if x % 2 else 2)
            y = fila + 1
            while y < H and (grosor is None or y < fila + grosor):
                suelo[y][x] = 20 if izq else 23 if der else (21 if (x + y) % 3 else 22)
                y += 1
    tramo(0, 17); tramo(21, 34); tramo(38, 55); tramo(59, 79)
    tramo(26, 29, 8, 2)          # plataforma flotante
    tramo(44, 47, 9)             # escalón alto
    tramo(64, 68, 7, 2)          # plataforma alta
    tramo(71, 73, 9, 2)
    for x, y, t in ((4, 10, 126), (9, 10, 124), (13, 10, 128), (24, 10, 125), (31, 10, 126), (40, 10, 127), (50, 10, 124), (53, 10, 126), (61, 10, 128), (66, 6, 125), (76, 10, 126)):
        deco[y][x] = t
    deco[10][2] = 85; deco[10][36] = 87     # carteles
    return suelo, deco

def capa(nombre, m, solida):
    s = "[capa %s solida=%s]\n" % (nombre, "si" if solida else "no")
    for fila in m:
        s += " ".join("." if c is None else str(c) for c in fila) + "\n"
    return s

MONEDAS = [(6, 9), (7, 9), (8, 9), (15, 9), (19, 8), (20, 8), (24, 7), (27, 6), (28, 6), (30, 9), (33, 9), (36, 8), (41, 9), (45, 7), (46, 7), (52, 9), (57, 8),
           (62, 9), (65, 5), (66, 5), (67, 5), (72, 7), (74, 9), (75, 9)]
ENEMIGOS = [(28, 10), (47, 8), (50, 10), (62, 10), (70, 10)]
MURCIELAGOS = [(22, 6), (42, 6), (57, 5)]

def main(carpeta):
    suelo, deco = cargar()
    t = "# Nivel 1 del ejemplo de plataformas (generado por tools/generar_nivel_plataformas.py y editable a mano).\n"
    t += "# Las casillas son índices de img/tilemap.png (18x18, 20 columnas); «.» es una celda vacía.\n"
    t += "[Nodo2D Nivel]\nguion = \"guiones/nivel.lua\"\nvar.monedas_total = %d\n" % len(MONEDAS)
    t += "[Nodo2D Mundo padre=.]\n"
    t += "[MapaCasillas Terreno padre=Mundo]\ntextura = \"img/tilemap.png\"\ncasilla = 18 18\ntamano = %d %d\nsolidas = 0 1 2 3 20 21 22 23 4 5 24 25\ncapa = 1\nmascara = 1\n" % (W, H)
    t += capa("decoracion", deco, False) + capa("suelo", suelo, True)
    t += "[Instancia Jugador escena=\"escenas/jugador.esc\" padre=Mundo]\nposicion = 30 170\n"
    for i, (x, y) in enumerate(MONEDAS):
        t += "[Instancia Moneda%d escena=\"escenas/moneda.esc\" padre=Mundo]\nposicion = %d %d\n" % (i + 1, x * 18 + 9, y * 18 + 9)
    for i, (x, y) in enumerate(ENEMIGOS):
        t += "[Instancia Limo%d escena=\"escenas/enemigo.esc\" padre=Mundo]\nposicion = %d %d\n" % (i + 1, x * 18 + 9, y * 18 + 6)
    for i, (x, y) in enumerate(MURCIELAGOS):
        t += "[Instancia Murcielago%d escena=\"escenas/murcielago.esc\" padre=Mundo]\nposicion = %d %d\n" % (i + 1, x * 18 + 9, y * 18 + 9)
    t += "[Area Meta padre=Mundo]\nposicion = %d %d\nforma = \"caja\" 18 60\nmascara = 1\nguion = \"guiones/meta.lua\"\n" % (76 * 18 + 9, 9 * 18)
    t += "[Sprite Bandera padre=Mundo/Meta]\ntextura = \"img/tilemap.png\"\ncuadricula = 20 9\nfotograma = 111\nposicion = 0 18\nanimacion = \"ondear\" 3 si 111 112\nanimar = \"ondear\"\n"
    t += "[Camara2D Camara padre=Mundo]\nactual = si\nposicion = 160 126\nseguir = \"../Jugador\"\nsuavizado = 6\nusar_limites = si\nlimites = 0 0 %d %d\n" % (W * 18, H * 18)
    t += "[ReproductorAudio Musica padre=.]\nsonido = \"audio/musica.wav\"\nmusica = si\nbucle = si\nvolumen = 0.35\nautoreproducir = si\n"
    t += "[Etiqueta Puntos padre=.]\nposicion = 6 5\ntexto = \"Monedas: 0\"\nfijo = si\nz = 10\n"
    t += "[Etiqueta Vidas padre=.]\nposicion = 6 17\ntexto = \"Vidas: 3\"\nfijo = si\nz = 10\n"
    t += "[Etiqueta Mensaje padre=.]\nposicion = 160 70\ntexto = \"\"\nalineado = \"centro\"\nescala_texto = 2\nfijo = si\nz = 10\n"
    t += "[Boton Reiniciar padre=.]\nposicion = 120 100\ntamano = 80 20\ntexto = \"Reiniciar\"\nvisible = no\nz = 10\n"
    t += "[Conexion desde=Reiniciar senal=presionado hacia=. metodo=_reiniciar]\n"
    os.makedirs(os.path.join(carpeta, "escenas"), exist_ok=True)
    with open(os.path.join(carpeta, "escenas", "nivel1.esc"), "w", encoding="utf-8") as f:
        f.write(t)

if __name__ == "__main__":
    main(sys.argv[1])
