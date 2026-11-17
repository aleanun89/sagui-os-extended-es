#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# java_entorno.sh - JDK (Temurin 17) en el anfitrión, sin sudo (D376, docs/java.md).
#
#   java_entorno.sh instalar   descarga el JDK (versión fija, SHA-256 comprobado) en
#                              ~/.local/saguitools/jdk. Idempotente.
#   java_entorno.sh estado     qué hay instalado.
#
# Hace falta para compilar la biblioteca de clases de miniJVM y los ejemplos (javac
# --release 8). Nada de esto va a git. env.sh añade jdk/bin al FINAL del PATH.
set -euo pipefail
BASE="${SAGUI_TOOLS:-$HOME/.local/saguitools}"
DESC="$BASE/descargas"
JDK_DIR="$BASE/jdk"
JDK_VER=17.0.21+9
JDK_SHA=8b9b611d917e3dfd8419d3566b5949226f69b052fed7e109c1368add1dfe8711
JDK_URL="https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.21%2B9/OpenJDK17U-jdk_x64_linux_hotspot_17.0.21_9.tar.gz"

instalar() {
    if [ -f "$JDK_DIR/.version" ] && [ "$(cat "$JDK_DIR/.version")" = "$JDK_VER" ]; then
        echo "JDK $JDK_VER: ya instalado en $JDK_DIR"; return 0
    fi
    mkdir -p "$DESC"
    local fich="$DESC/$(basename "$JDK_URL")"
    if [ ! -f "$fich" ] || [ "$(sha256sum "$fich" | cut -d' ' -f1)" != "$JDK_SHA" ]; then
        echo "Descargando JDK $JDK_VER..."
        curl -fL --retry 3 -o "$fich.parte" "$JDK_URL"
        mv "$fich.parte" "$fich"
    fi
    if [ "$(sha256sum "$fich" | cut -d' ' -f1)" != "$JDK_SHA" ]; then
        echo "ERROR: el SHA-256 de $fich no es el esperado ($JDK_SHA)." >&2; exit 1
    fi
    local tmp; tmp="$(mktemp -d "$BASE/.extraer.XXXXXX")"
    tar xzf "$fich" -C "$tmp" --strip-components=1
    if [ -d "$JDK_DIR" ]; then mv "$JDK_DIR" "$JDK_DIR.viejo.$$"; fi
    mv "$tmp" "$JDK_DIR"
    echo "$JDK_VER" > "$JDK_DIR/.version"
    if [ -d "$JDK_DIR.viejo.$$" ]; then rm -r "$JDK_DIR.viejo.$$"; fi
    echo "JDK instalado en $JDK_DIR"
}
# Janino (BSD-3-Clause): compilador de Java escrito en Java, para la orden «javac» de SaguiOS (D376).
JANINO_VER=3.1.12
JANINO_SHA=5699878c31e71c9a94cf472f95a2dad52ca7b0449c099276b8bd7e992a43595b
COMPILER_SHA=17ea25d860225f4b59d0a512b4995f2e4711bdae5e15736cbcd5f6ce4dda8f7c
janino() {
    local d="$BASE/janino"
    mkdir -p "$d"
    for par in "janino:$JANINO_SHA" "commons-compiler:$COMPILER_SHA"; do
        local n="${par%%:*}" sha="${par#*:}" f="$d/${par%%:*}-$JANINO_VER.jar"
        if [ ! -f "$f" ] || [ "$(sha256sum "$f" | cut -d' ' -f1)" != "$sha" ]; then
            curl -fL --retry 3 -o "$f.parte" "https://repo1.maven.org/maven2/org/codehaus/janino/$n/$JANINO_VER/$n-$JANINO_VER.jar"
            mv "$f.parte" "$f"
        fi
        [ "$(sha256sum "$f" | cut -d' ' -f1)" = "$sha" ] || { echo "ERROR: SHA-256 de $f" >&2; exit 1; }
    done
    echo "Janino $JANINO_VER en $d"
}
case "${1:-}" in
    instalar) instalar; janino ;;
    janino) janino ;;
    estado) if [ -f "$JDK_DIR/.version" ]; then echo "JDK $(cat "$JDK_DIR/.version") en $JDK_DIR"; else echo "JDK no instalado"; fi ;;
    *) sed -n '2,10p' "$0"; exit 2 ;;
esac
