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

echo
echo "Copia lista: $DESTINO"
echo "  rama: $NOME (base: $BASE)"
echo
echo "  cd $DESTINO"
echo "  . ~/.local/saguitools/env.sh && make"
echo "  python3 tests/rodar_paralelo.py --kvm -j 2"
