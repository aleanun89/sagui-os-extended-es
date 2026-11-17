#!/bin/sh
# SPDX-License-Identifier: GPL-2.0-or-later
# depurar-remoto.sh: el lado del PC de DEBUG-APP (D362). Todo está en depurar-remoto.py.
exec python3 "$(dirname "$0")/depurar-remoto.py" "$@"
