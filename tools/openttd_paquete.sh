#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# openttd_paquete.sh - receta que compila OpenTTD para SaguiOS como PAQUETE aparte (D375, docs/openttd.md).
#
# OpenTTD es GPL-2.0 solo: su código vive en OTRO repositorio (por defecto ../openttd-saguios, junto a la copia del sistema;
# rama «saguios»; cuando exista el repositorio público, OPENTTD_URL) y nunca entra en este repositorio ni
# en las imágenes públicas. Esta receta solo:
#   1. clona o actualiza ese repositorio (OPENTTD_REPO / OPENTTD_URL / OPENTTD_RAMA);
#   2. prepara el SDK de esta copia: «make sdk_externo» (objetos de la libc, libcxx, libsagui y SDL 2 y las
#      banderas exactas), con tools/turno.sh si SAGUI_TURNOS está definido;
#   3. llama a saguios/compilar.sh del otro repositorio, que deja el paquete openttd-<versión>-saguios.tar;
#   4. lo copia a SAGUI_PAQUETES (por defecto build/paquetes, fuera de git) con su .sha256.
#
#   tools/openttd_paquete.sh [-j N]     compila y empaqueta
#   tools/openttd_paquete.sh instalar   instala el último paquete en input_share/apps (que el make vuelca en
#                                       /apps de la imagen privada; input_share no se publica)
set -eu
RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
REPO="${OPENTTD_REPO:-$RAIZ/../openttd-saguios}"   # junto a la copia de trabajo del sistema
URL="${OPENTTD_URL:-}"
RAMA="${OPENTTD_RAMA:-saguios}"
PAQUETES="${SAGUI_PAQUETES:-$RAIZ/build/paquetes}"
J=2
if [ "${1:-}" = "instalar" ]; then
  p="$(ls -t "$PAQUETES"/openttd-*-saguios.tar 2>/dev/null | head -1)"
  [ -n "$p" ] || { echo "no hay paquete en $PAQUETES: ejecute primero tools/openttd_paquete.sh" >&2; exit 1; }
  exec python3 "$RAIZ/tools/paquete_instalar.py" --input-share "$p"
fi
[ "${1:-}" = "-j" ] && J="$2"

if [ ! -d "$REPO/.git" ]; then
  [ -n "$URL" ] || { echo "no existe $REPO: defina OPENTTD_URL (repositorio del porte) o OPENTTD_REPO" >&2; exit 1; }
  git clone --branch "$RAMA" "$URL" "$REPO"
elif [ -n "$URL" ]; then
  git -C "$REPO" fetch "$URL" "$RAMA" && git -C "$REPO" checkout "$RAMA" && git -C "$REPO" merge --ff-only FETCH_HEAD
fi

# SDK de esta copia (la libc, el runtime de C++, la libsagui y la SDL 2 compiladas).
if [ -n "${SAGUI_TURNOS:-}" ]; then
  (cd "$RAIZ" && tools/turno.sh make -j2 sdk_externo)
else
  (cd "$RAIZ" && make -j2 sdk_externo)
fi

mkdir -p "$PAQUETES"
SAGUI_SDK="$RAIZ/build/sdk" SALIDA="${OPENTTD_SALIDA:-$REPO/build-saguios}" "$REPO/saguios/compilar.sh" -j "$J"
cp "${OPENTTD_SALIDA:-$REPO/build-saguios}"/openttd-*-saguios.tar "${OPENTTD_SALIDA:-$REPO/build-saguios}"/openttd-*-saguios.tar.sha256 "$PAQUETES/"
echo "paquete en $PAQUETES:"; ls -l "$PAQUETES"/openttd-*-saguios.tar*
