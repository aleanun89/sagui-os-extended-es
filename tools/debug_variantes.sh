#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# debug_variantes.sh - imágenes de DEPURACIÓN para probar en portátiles por SSH (D418).
#
# Lo que hace «make debug_variantes DEBUG_DESTINO=DIR [DEBUG_CLAVE=PUB] [JOGOS_MB=N] [DEBUG_SIN_ROOT=1]»:
# las mismas imágenes que priv_variantes (todo lo privado, multinúcleo, uACPI...), pero con SAGUI-CONF
# llevando la clave pública del PC para root, sshd y ntp activados, zona Europe/Madrid, la clave FIJA del
# servidor (fuera de git, en ~/SO/private/debug-ssh) y un aviso rojo en el escritorio.
# NUNCA se publican: publicar.sh las rechaza. Al acabar se borran de build/ las imágenes con la clave,
# para que un «make» normal no las reutilice.
set -euo pipefail
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$RAIZ"
DESTINO="${DEBUG_DESTINO:?falta DEBUG_DESTINO (carpeta de salida)}"
CLAVE="${DEBUG_CLAVE:-$HOME/.ssh/saguios_amilo.pub}"
PRIVADO="${DEBUG_PRIVADO:-$HOME/SO/private/debug-ssh}"
case "$DESTINO" in /|"$RAIZ"|"$HOME") echo "debug_variantes: destino no válido" >&2; exit 1;; esac
[ -f "$CLAVE" ] || { echo "debug_variantes: no existe la clave pública $CLAVE (DEBUG_CLAVE=...)" >&2; exit 1; }
CONF="$(mktemp -d "${TMPDIR:-/tmp}/saguios-debug-conf.XXXXXX")"
trap 'rm -rf "$CONF"' EXIT
python3 tools/debug_conf.py --salida "$CONF" --clave "$CLAVE" --privado "$PRIVADO"
limpiar_imagenes() { rm -f build/saguios*.img build/saguios*.img.xz build/saguios*.vdi build-sem-gpl/saguios*.img; }
limpiar_imagenes   # make no ve el cambio de entorno: se regeneran todas con la clave
trap 'limpiar_imagenes; rm -rf "$CONF"' EXIT
export SAGUI_CONF_EXTRA="$CONF"
[ "${DEBUG_SIN_ROOT:-0}" = 1 ] && export SAGUI_DEBUG_SIN_ROOT=1
make priv_descargas
./gerar_versao.sh --todas --con-input-share --destino "$DESTINO" ${GERAR_OPC:-}
{
    echo
    echo "IMAGEN DE DEPURACIÓN: entra por SSH con la clave del PC; no distribuir."
    echo "Lleva la clave pública del PC como root (SAGUI-CONF), sshd y ntp activados, zona Europe/Madrid,"
    echo "la huella del servidor fija y un aviso rojo en el escritorio (D418). Ver docs/debug-ssh.md."
    [ "${DEBUG_SIN_ROOT:-0}" = 1 ] && echo "Sin pedir la contraseña de root en el primer arranque (DEBUG_SIN_ROOT=1)."
} >> "$DESTINO/VERSION.txt"
echo
echo "debug_variantes: en $DESTINO (NO distribuir). Huella del servidor y known_hosts en $PRIVADO:"
ssh-keygen -lf "$PRIVADO/ssh_host_ed25519_key.pub" 2>/dev/null || true
echo "  ssh -o UserKnownHostsFile=$PRIVADO/known_hosts -i ${CLAVE%.pub} root@IP_DEL_PORTATIL"
