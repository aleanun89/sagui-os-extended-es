#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""mkcompilacao.py - gera o cabeçalho com os dados da compilação.

Escreve um cabeçalho C com:
  SAGUI_BUILD_DATA   - data da compilação (AAAA-MM-DD)
  SAGUI_BUILD_HORA   - hora da compilação (HH:MM:SS)
  SAGUI_BUILD_COMMIT - commit do git (o curto), ou "sem git"
  SAGUI_BUILD_SUJO   - "sim" se a árvore de trabalho tinha cambios, "nao"
  SAGUI_BUILD_TIPO   - "normal", "sem GPL" ou "sem GPL e sem firmware"

A data y la hora se pueden fijar con SAGUI_DATA_COMPILACAO (AAAA-MM-DD) y
SAGUI_HORA_COMPILACAO (HH:MM:SS), para hacer una compilación reproducible (el
mismo código da o mesmo binario).
"""
import os
import subprocess
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def correr(cmd):
    try:
        r = subprocess.run(cmd, cwd=RAIZ, capture_output=True, text=True, timeout=10)
        return r.stdout.strip() if r.returncode == 0 else ""
    except (OSError, subprocess.SubprocessError):
        return ""


def main():
    if len(sys.argv) < 2:
        print("uso: mkcompilacao.py saida.h", file=sys.stderr)
        return 2
    destino = sys.argv[1]

    data = os.environ.get("SAGUI_DATA_COMPILACAO") or correr(["date", "+%Y-%m-%d"]) or "????-??-??"
    hora = os.environ.get("SAGUI_HORA_COMPILACAO") or correr(["date", "+%H:%M:%S"]) or "??:??:??"

    commit = correr(["git", "rev-parse", "--short", "HEAD"]) or "sem git"
    sujo = "nao"
    if commit != "sem git":
        if correr(["git", "status", "--porcelain"]):
            sujo = "sim"

    tipo = "normal"
    if os.environ.get("SEM_GPL") == "1":
        tipo = "sem GPL"
        if os.environ.get("SEM_FIRMWARE") == "1":
            tipo = "sem GPL e sem firmware"
    elif os.environ.get("SEM_FIRMWARE") == "1":
        tipo = "sem firmware"

    with open(destino, "w", encoding="latin-1") as f:
        f.write("/* gerado por tools/mkcompilacao.py: nao editar a mao */\n")
        f.write("#ifndef SAGUI_COMPILACAO_H\n")
        f.write("#define SAGUI_COMPILACAO_H\n")
        f.write(f'#define SAGUI_BUILD_DATA   "{data}"\n')
        f.write(f'#define SAGUI_BUILD_HORA   "{hora}"\n')
        f.write(f'#define SAGUI_BUILD_COMMIT "{commit}"\n')
        f.write(f'#define SAGUI_BUILD_SUJO   "{sujo}"\n')
        f.write(f'#define SAGUI_BUILD_TIPO   "{tipo}"\n')
        f.write("#endif\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
