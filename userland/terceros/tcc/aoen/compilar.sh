# SPDX-License-Identifier: GPL-3.0-or-later
# compilar.sh - compila AoEN dentro de SaguiOS con TCC (D384, /ayuda/modificar-aoen.md).
#
#   cd /apps/aoen/fuentes
#   sh compilar.sh            deja ./aoen
#   ./aoen                    lo arranca (con los datos en /jogos/AOE, como /apps/bin/aoen)
#
# Con gcc (en el PC) AoEN se compila igual con estas definiciones; aquí sin -O2 (TCC no optimiza)
# y sin las rutas SSE2 de stb_image (STBI_NO_SIMD); las de la libsagui (rapido.c) siguen.
FUENTES="@FUENTES@"
OPCIONES="-DSAGUI_AOEN -DAOEN_FONTE_SAGUI -DSTBI_NO_THREAD_LOCALS -DSTBI_NO_SIMD -DNDEBUG -I."
echo "compilando AoEN con tcc..."
tcc $OPCIONES $* -o aoen $FUENTES -lSDL2
if [ $? -ne 0 ]; then
    echo "compilar.sh: error al compilar"
    exit 1
fi
echo "listo: ./aoen"
