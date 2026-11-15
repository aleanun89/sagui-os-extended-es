#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# turno.sh ORDEN...: ejecuta ORDEN cuando haya un turno libre de compilación/QEMU.
#
# Limita cuántos agentes compilan o prueban a la vez (AGENTS.md, «La batería»). Espera
# además a que no haya una batería completa en marcha (fichero bateria.lock).
#   SAGUI_TURNOS   número de turnos (por defecto 2; en el PC principal del mantenedor, 6)
#   PRIORIDAD=1    puede usar también el último turno, reservado a lo prioritario
#   SAGUI_TURNOS_DIR  carpeta de los cerrojos (por defecto /tmp/saguios-turnos)
# El coordinador crea "$SAGUI_TURNOS_DIR/bateria.lock" mientras pasa la batería completa.
D="${SAGUI_TURNOS_DIR:-/tmp/saguios-turnos}"
N="${SAGUI_TURNOS:-2}"
mkdir -p "$D"
while [ -f "$D/bateria.lock" ]; do sleep 20; done
if [ "${PRIORIDAD:-0}" = 1 ]; then LISTA="$N $(seq 1 $((N - 1)))"; else LISTA="$(seq 1 $((N > 1 ? N - 1 : 1)))"; fi
while true; do
  for t in $LISTA; do
    exec 9>"$D/turno$t.lock"
    if flock -n 9; then "$@"; r=$?; flock -u 9; exit $r; fi
  done
  sleep 10
done
