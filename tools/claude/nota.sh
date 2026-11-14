#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# nota.sh "texto": añade una línea con fecha y hora a notas-revisor/RESPUESTA_CLAUDE.md
F="$(cd "$(dirname "$0")/../.." && pwd)/../notas-revisor/RESPUESTA_CLAUDE.md"
echo "- [$(date '+%Y-%m-%d %H:%M')] $*" >> "$F"
