# SPDX-License-Identifier: GPL-3.0-only
# compilar.sh - TCC se compila a sí mismo dentro de SaguiOS (D384, /ayuda/tcc.md).
#
#   cp -r /apps/tcc/fuentes /tmp/tcc
#   cd /tmp/tcc
#   sh compilar.sh            deja ./tcc, que compila igual que /apps/bin/tcc
#   ./tcc -run /apps/tcc/ejemplos/hola.c
#
# Son los fuentes de TCC (LGPL 2.1 o posterior, COPYING) con los parches de SaguiOS ya aplicados,
# su config.h y la tabla de símbolos para -run (tcc_simbolos.c, generada al compilar SaguiOS).
echo "compilando TCC con tcc..."
tcc -DONE_SOURCE=1 -I. $* -o tcc tcc.c tcc_libc_extra.c tcc_simbolos.c
if [ $? -ne 0 ]; then
    echo "compilar.sh: error al compilar"
    exit 1
fi
echo "listo: ./tcc"
