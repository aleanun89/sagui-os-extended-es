#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# confirmar-hardware.sh - registra que Eli probó una fase en hardware real.
#
# Uso: tools/confirmar-hardware.sh "fase 1 ok"
# Graba la confirmación en el diario y en el panel (dashboard/status.json).
set -e
cd "$(dirname "$0")/.."
frase="$1"
[ -n "$frase" ] || { echo 'uso: tools/confirmar-hardware.sh "fase N ok"'; exit 1; }
fase=$(echo "$frase" | sed -n 's/.*fase \([0-9]\+\).*/\1/p')
[ -n "$fase" ] || { echo "não achei o número da fase em: $frase"; exit 1; }
quando=$(date '+%Y-%m-%d %H:%M')
printf '\n## %s\n\n- **Confirmação do Eli no hardware:** "%s". Fase %s validada no VirtualBox e no G470.\n' \
    "$quando" "$frase" "$fase" >> docs/diario.md
python3 tools/status.py confirmar-fase "$fase" "$frase"
echo "Fase $fase confirmada e registrada."
