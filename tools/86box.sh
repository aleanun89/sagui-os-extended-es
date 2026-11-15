#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# 86box.sh MAQUINA IMAGEN [--sin-pantalla] [--segundos N] [--captura FICHERO] [--esperar TEXTO]
#
# Arranca una máquina de referencia de 86Box (tools/86box/maquinas/MAQUINA.cfg) con una imagen
# de SaguiOS (raw, BIOS) y la deja encendida N segundos (60 por defecto). Copia la configuración
# y la imagen a build/86box/<máquina>-<pid>/ (la imagen original no se toca), guarda el serie de
# COM1 en serie.txt, saca la captura pedida y lo para todo por PID (nunca con pkill -f).
# Con --sin-pantalla levanta un Xvfb propio; sin ella usa el DISPLAY actual (WSLg).
#
# Cuenta como una máquina más para tools/turno.sh: se envuelve solo. 86Box NUNCA va en la
# batería (docs/86box.md). Instalación: bash tools/86box_instalar.sh
set -eu
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if [ -z "${SAGUI_86BOX_EN_TURNO:-}" ]; then
    export SAGUI_86BOX_EN_TURNO=1
    exec "$RAIZ/tools/turno.sh" "${BASH_SOURCE[0]}" "$@"
fi
exec python3 "$RAIZ/tests/emu86box.py" "$@"
