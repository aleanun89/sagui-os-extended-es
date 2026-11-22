#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""AoEN: la tabla de gráficos de unidades (render.c) es coherente y recoge las correcciones del mapeo.
Sin datos reales: solo se lee el código y el generador de DRS sintéticos. Los gráficos de referencia salen del EMPIRES.DAT
del juego original (estudio privado; los datos comerciales no se versionan)."""
import os, re, sys
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
src = open(os.path.join(R, 'userland/juegos/aoen/render.c'), encoding='utf-8').read()
m = re.search(r'unit_art_ids\[NUM_UNIT_ART\]\[5\] = \{(.*?)\n\};', src, re.S)
filas = {}
for l in m.group(1).split('\n'):
    mm = re.match(r'\s*\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\},\s*/\*\s*(.*?)\s*\*/', l)
    if mm:
        filas[mm.group(6).split(' (')[0].split(':')[0]] = [int(mm.group(i)) for i in range(1, 6)]
fallos = []
def ok(c, t):
    if not c: fallos.append(t)
ok(len(filas) == 29, 'la tabla debe tener 29 unidades, tiene %d' % len(filas))
n = int(re.search(r'#define NUM_UNIT_ART (\d+)', src).group(1))
ok(n == len(filas), 'NUM_UNIT_ART no coincide con la tabla')
# cada gráfico de la tabla y de las capas existe en el DRS sintético
sint = open(os.path.join(R, 'tools/aoen_drs_sintetico.py'), encoding='utf-8').read()
bloque = re.search(r'UNIDADES = \[(.*?)\]\nEDIFICIOS', sint, re.S).group(1)
ids = {int(x) for x in re.findall(r'\b\d+\b', re.sub(r'#.*', '', bloque))}
sup = {int(x) for x in re.findall(r'\{ (\d+), (\d+) \}', re.search(r'unit_art_sup.*?\};', src, re.S).group(0)) if False} 
sup = {int(x) for x in re.findall(r'\b\d{3}\b', re.search(r'unit_art_sup.*?\n\};', src, re.S).group(0))}
for nom, v in filas.items():
    for x in v + []:
        ok(x in ids, '%s: el gráfico %d no está en el DRS sintético' % (nom, x))
for x in sup:
    ok(x in ids, 'capa %d no está en el DRS sintético' % x)
# un caso por corrección (referencia: EMPIRES.DAT original)
casos = {
    'barco de pesca': [473, 473, 473, 262, 262], 'barco de transporte': [647, 647, 647, 262, 262],
    'trirreme': [693, 693, 693, 264, 264], 'barco mercante': [639, 639, 639, 262, 262],
    'arquero en carro': [412, 650, 202, 310, 369], 'explorador': [445, 651, 227, 343, 403],
    'arquero a caballo': [422, 661, 209, 318, 377], 'espadachín corto': [416, 655, 206, 312, 371],
    'hoplita': [442, 684, 225, 340, 401], 'balista': [417, 656, 207, 313, 372],
    'catapulta': [421, 660, 629, 317, 376], 'elefante de guerra': [429, 669, 216, 325, 387],
    'arquero de elefante': [427, 666, 214, 323, 385], 'elefante salvaje': [428, 667, 215, 324, 386],
}
for nom, v in casos.items():
    ok(filas.get(nom) == v, '%s: esperaba %s y hay %s' % (nom, v, filas.get(nom)))
# ningún barco repite el casco de otro
cascos = [filas[k][0] for k in ('barco de pesca', 'barco de transporte', 'trirreme', 'barco mercante')]
ok(len(set(cascos)) == 4, 'dos barcos comparten casco')
# velas y remos: 583 (transporte), 694 y 584 (trirreme)
ok(sup == {697, 583, 694, 584}, 'capas inesperadas %s' % sup)
for f in fallos: print('FALLO:', f)
print('aoen_sprites:', 'FALLA' if fallos else 'OK', '(%d unidades)' % len(filas))
sys.exit(1 if fallos else 0)
