#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# glmark2_pc.sh - compila glmark2-es2 (flavor X11 + EGL + OpenGL ES 2.0) en el PC, sin meson, con las MISMAS fuentes
# que la versión de SaguiOS (userland/terceros/glmark2/fuentes, tras tools/glmark2_fuentes.sh), para tener la
# referencia con Mesa en la misma máquina (D481). Hace falta g++, libx11-dev, libpng-dev, libjpeg-dev y las
# cabeceras de EGL/GLES2 (libegl-dev, libgles-dev).
#
#   tools/glmark2_pc.sh            -> build/glmark2-pc/glmark2-es2
#   build/glmark2-pc/glmark2-es2 -b :duration=2.0 -s 320x240 -f userland/terceros/glmark2/benchmarks.txt
set -euo pipefail
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
S="$RAIZ/userland/terceros/glmark2/fuentes/src"
D="$RAIZ/userland/terceros/glmark2/fuentes/data"
O="$RAIZ/build/glmark2-pc/obj"
[ -e "$S/../.preparado" ] || { echo "glmark2_pc.sh: faltan las fuentes (tools/glmark2_fuentes.sh)" >&2; exit 1; }
mkdir -p "$O"
SRC="benchmark-collection benchmark canvas-generic gl-headers gl-visual-config image-reader main-loop mesh model options results-file scene-buffer scene-build scene-bump scene-clear scene-conditionals scene scene-default-options scene-desktop scene-effect-2d scene-function scene-grid scene-ideas scene-jellyfish scene-loop scene-pulsar scene-refract scene-shading scene-shadow scene-terrain scene-texture shared-library text-renderer texture gl-state-egl main native-state-x11 scene-terrain/base-renderer scene-terrain/blur-renderer scene-terrain/copy-renderer scene-terrain/luminance-renderer scene-terrain/normal-from-height-renderer scene-terrain/overlay-renderer scene-terrain/renderer-chain scene-terrain/simplex-noise-renderer scene-terrain/terrain-renderer scene-terrain/texture-renderer"
CC="libmatrix/log libmatrix/mat libmatrix/program libmatrix/shader-source libmatrix/util scene-ideas/a scene-ideas/d scene-ideas/e scene-ideas/i scene-ideas/lamp scene-ideas/logo scene-ideas/m scene-ideas/n scene-ideas/o scene-ideas/s scene-ideas/splines scene-ideas/table scene-ideas/t"
F="-O2 -std=c++14 -w -DGLMARK2_USE_GLESv2 -DGLMARK2_USE_EGL -DGLMARK2_USE_X11 -DUSE_EXCEPTIONS -DGLMARK_VERSION=\"2023.01\" -DGLMARK_DATA_PATH=\"$D\" -DGLMARK2_EXECUTABLE=\"glmark2-es2\" -I$S/glad/include -I$S/libmatrix -I$S/scene-ideas -I$S/scene-terrain -I$S"
for f in $SRC $CC; do
    mkdir -p "$O/$(dirname "$f")"
    ext=cpp; [ -f "$S/$f.cpp" ] || ext=cc
    if [ ! -f "$O/$f.o" ] || [ "$S/$f.$ext" -nt "$O/$f.o" ]; then
        g++ $F -c -o "$O/$f.o" "$S/$f.$ext" &
        while [ "$(jobs -r | wc -l)" -ge "${GLMARK2_J:-3}" ]; do sleep 0.2; done
    fi
done
wait
for f in egl gles2; do gcc -O2 -w -I"$S/glad/include" -c -o "$O/glad_$f.o" "$S/glad/src/$f.c"; done
g++ -o "$RAIZ/build/glmark2-pc/glmark2-es2" $(find "$O" -name '*.o') -ldl -lX11 -lpng -ljpeg -lm
echo "glmark2_pc.sh: listo en build/glmark2-pc/glmark2-es2"
