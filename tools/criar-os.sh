#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# Crea una copia de trabajo aislada para una característica.
#
#   tools/criar-os.sh <caracteristica> [rama_base] [nombre_rama]
#
# Deja la copia en ../miniOS_master_os_<caracteristica>, con su propia rama, de
# forma que los cambios de una característica no rompan el árbol principal ni
# las herramientas de las demás (la compilación va a build/, propio de cada
# copia).
#
# La copia se crea con «git worktree»: comparte el historial con el repo
# principal (no duplica el .git), así que los merges son normales.
#
#   $ tools/criar-os.sh progresso
#   $ cd ../miniOS_master_os_progresso && make && python3 tests/rodar_todos.py
#
# Para finish:
#   $ git worktree remove ../miniOS_master_os_progresso
#
set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$RAIZ"

if [ $# -lt 1 ]; then
    echo "uso: tools/criar-os.sh <caracteristica> [rama_base] [nombre_rama]" >&2
    exit 2
fi

CARACTERISTICA="$1"
BASE="${2:-$(git rev-parse --abbrev-ref HEAD)}"
NOME="${3:-v6.1-$CARACTERISTICA}"
DESTINO="$RAIZ/../miniOS_master_os_$CARACTERISTICA"

case "$CARACTERISTICA" in
    ""|*/*|.|..)
        echo "criar-os: característica inválida: '$CARACTERISTICA'" >&2
        exit 2
        ;;
esac

if [ -e "$DESTINO" ]; then
    echo "criar-os: ya existe $DESTINO" >&2
    echo "           (si está en desuso: git worktree remove $DESTINO)" >&2
    exit 1
fi

if git show-ref --verify --quiet "refs/heads/$NOME"; then
    echo "criar-os: la rama $NOME ya existe; se reutiliza" >&2
    git worktree add "$DESTINO" "$NOME"
else
    git worktree add -b "$NOME" "$DESTINO" "$BASE"
fi

# Fuentes de terceros y fuentes tipográficas que no están en git (las bajan
# tools/*_fuentes.sh): se enlazan a las del árbol principal. Solo DENTRO de la
# copia: un «ln» en el árbol principal deja un enlace a sí mismo y borra las
# fuentes (pasó el 2026-11-13). Los datos de juegos no se enlazan, salvo
# Freedoom (libre).
if [ "$(cd "$DESTINO" && pwd)" != "$RAIZ" ]; then
    for rel in NEW_FONTS userland/terceros/*/fuentes userland/terceros/chocolatedoom/datos; do
        [ -d "$RAIZ/$rel" ] && [ ! -L "$RAIZ/$rel" ] || continue
        [ -e "$DESTINO/$rel" ] && continue
        mkdir -p "$(dirname "$DESTINO/$rel")"
        ln -s "$RAIZ/$rel" "$DESTINO/$rel"
        echo "criar-os: enlazado $rel"
    done
fi

echo
echo "Copia lista: $DESTINO"
echo "  rama: $NOME (base: $BASE)"
echo
echo "  cd $DESTINO"
echo "  . ~/.local/saguitools/env.sh && make"
echo "  python3 tests/rodar_paralelo.py --kvm -j 2"
