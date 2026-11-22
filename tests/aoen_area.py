#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""AoEN en el PC (D443): compila tests/aoen_area.c con el motor y lo ejecuta (daño en área de la catapulta, con y sin fuego amigo,
y recarga de la fe del sacerdote). Datos sintéticos: no hace falta ningún DRS."""
import os, subprocess, sys, tempfile
R = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
d0 = os.path.join(R, 'userland/juegos/aoen')
fuentes = [os.path.join(d0, f) for f in sorted(os.listdir(d0)) if f.endswith('.c') and f not in ('aoen_config.c', 'puente.c', 'main.c', 'rendimiento.c') and not f.startswith('ui_')]
sdl = subprocess.run(['sdl2-config', '--cflags'], capture_output=True, text=True).stdout.split()
libs = subprocess.run(['sdl2-config', '--libs'], capture_output=True, text=True).stdout.split()
exe = os.path.join(tempfile.mkdtemp(prefix='aoen-area-'), 'aoen_area')
subprocess.run(['gcc', '-std=gnu99', '-O1', '-w', '-I', d0, '-I', os.path.join(R, 'userland/terceros/stbimage')] + sdl +
               ['-o', exe, os.path.join(R, 'tests/aoen_area.c')] + fuentes + libs + ['-lm', '-lpthread'], check=True)
r = subprocess.run([exe], capture_output=True, text=True, timeout=300)
print(r.stdout, end='')
sys.exit(0 if r.returncode == 0 and 'aoen_area OK' in r.stdout else 1)
