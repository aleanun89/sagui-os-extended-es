#!/bin/bash
# SPDX-License-Identifier: GPL-3.0-only
# tinygo_entorno.sh - TinyGo y Go oficial en el anfitrión, sin sudo (D364, docs/go.md).
#
#   tinygo_entorno.sh instalar   descarga TinyGo y Go (versión fija, SHA-256 comprobado) en
#                                ~/.local/saguitools/tinygo y ~/.local/saguitools/go, y prepara
#                                las copias parcheadas para SaguiOS. Es idempotente.
#   tinygo_entorno.sh preparar   solo las copias de SaguiOS (tools/go-sagui lo llama siempre).
#   tinygo_entorno.sh env        imprime las variables que usa tools/go-sagui (para «eval»).
#   tinygo_entorno.sh estado     qué hay instalado.
#
# Nada de esto va a git: es lo mismo que el resto de ~/.local/saguitools. Las versiones se cambian
# aquí (con su SHA-256, que se saca de las páginas de descargas de TinyGo y de go.dev).
#
# Copias parcheadas (no se toca la instalación original):
#   ~/.local/saguitools/go-sagui       GOROOT: igual que go/, pero src/syscall es el de SaguiOS
#                                      (userland/go/syscall/), que llama a la libc de SaguiOS en vez
#                                      de hacer int 0x80 con números de Linux.
#   ~/.local/saguitools/tinygo-sagui   TINYGOROOT: igual que tinygo/, pero src/runtime/runtime_unix.go
#                                      va parcheado para el montón de SaguiOS (parchear_tinygo.py).
set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASE="${SAGUI_TOOLS:-$HOME/.local/saguitools}"
DESC="$BASE/descargas"

TINYGO_VER=0.44.0
TINYGO_SHA=ed0f45543a49ff2a99120a5a1b8814a0750e1fda1a66f8e2a0a172576134aaf9
TINYGO_URL="https://github.com/tinygo-org/tinygo/releases/download/v${TINYGO_VER}/tinygo${TINYGO_VER}.linux-amd64.tar.gz"
GO_VER=1.26.11
GO_SHA=9849f4e04ee35303714e34fbc3ac8ab44ea3e448a95aa182f91e3b9b79c710ec
GO_URL="https://go.dev/dl/go${GO_VER}.linux-amd64.tar.gz"

TINYGO_DIR="$BASE/tinygo"
GO_DIR="$BASE/go"
GOSAGUI="$BASE/go-sagui"
TGSAGUI="$BASE/tinygo-sagui"

# bajar NOMBRE URL SHA256 DESTINO VERSION: descarga, comprueba y extrae (si no está ya).
bajar() {
    local nombre="$1" url="$2" sha="$3" destino="$4" ver="$5"
    if [ -f "$destino/.version" ] && [ "$(cat "$destino/.version")" = "$ver" ]; then
        echo "$nombre $ver: ya instalado en $destino"
        return 0
    fi
    mkdir -p "$DESC"
    local fich="$DESC/$(basename "$url")"
    if [ ! -f "$fich" ] || [ "$(sha256sum "$fich" | cut -d' ' -f1)" != "$sha" ]; then
        echo "Descargando $nombre $ver..."
        curl -fL --retry 3 -o "$fich.parte" "$url"
        mv "$fich.parte" "$fich"
    fi
    if [ "$(sha256sum "$fich" | cut -d' ' -f1)" != "$sha" ]; then
        echo "ERROR: el SHA-256 de $fich no es el esperado ($sha)." >&2
        exit 1
    fi
    echo "Extrayendo $nombre $ver en $destino..."
    local tmp
    tmp="$(mktemp -d "$BASE/.extraer.XXXXXX")"
    tar xzf "$fich" -C "$tmp" --strip-components=1
    if [ -d "$destino" ]; then mv "$destino" "$destino.viejo.$$"; fi
    mv "$tmp" "$destino"
    echo "$ver" > "$destino/.version"
    if [ -d "$destino.viejo.$$" ]; then rm -r "$destino.viejo.$$"; fi
}

# sello: resumen de lo que entra en las copias de SaguiOS (para no tocarlas si no ha cambiado
# nada: cambiar la fecha de los ficheros haría que TinyGo recompilase la biblioteca estándar).
sello() {
    { cat "$RAIZ"/userland/go/syscall/*.go "$RAIZ/userland/go/runtime/parchear_tinygo.py"
      echo "$TINYGO_VER $GO_VER"; } | sha256sum | cut -d' ' -f1
}

# enlazar ORIGEN DESTINO EXCEPTO: DESTINO con un enlace a cada entrada de ORIGEN salvo EXCEPTO.
enlazar() {
    local origen="$1" destino="$2" salvo="$3" e b
    mkdir -p "$destino"
    for e in "$origen"/* "$origen"/.[!.]*; do
        [ -e "$e" ] || continue
        b="$(basename "$e")"
        [ "$b" = "$salvo" ] && continue
        ln -sfn "$e" "$destino/$b"
    done
}

preparar() {
    [ -x "$TINYGO_DIR/bin/tinygo" ] || { echo "TinyGo no está instalado: tools/tinygo_entorno.sh instalar" >&2; exit 1; }
    [ -x "$GO_DIR/bin/go" ] || { echo "Go no está instalado: tools/tinygo_entorno.sh instalar" >&2; exit 1; }
    local s; s="$(sello)"
    if [ -f "$GOSAGUI/.sello" ] && [ "$(cat "$GOSAGUI/.sello")" = "$s" ] \
       && [ -f "$TGSAGUI/.sello" ] && [ "$(cat "$TGSAGUI/.sello")" = "$s" ]; then
        return 0
    fi
    # GOROOT: todo enlazado salvo src/. TinyGo mezcla su src/ con el de Go mirando qué entradas son
    # directorios (un enlace a un directorio no cuenta): por eso src/ lleva directorios de verdad
    # (cp -rs) con enlaces a los ficheros, y src/syscall es el nuestro.
    enlazar "$GO_DIR" "$GOSAGUI" src
    if [ -d "$GOSAGUI/src" ]; then rm -r "$GOSAGUI/src"; fi
    cp -rs "$GO_DIR/src" "$GOSAGUI/src"
    rm -r "$GOSAGUI/src/syscall"
    mkdir "$GOSAGUI/src/syscall"
    cp "$RAIZ"/userland/go/syscall/*.go "$GOSAGUI/src/syscall/"
    # TINYGOROOT: igual, con src/runtime/runtime_unix.go parcheado (copia de verdad) y sin los
    # 890 MB de src/device (un enlace).
    enlazar "$TINYGO_DIR" "$TGSAGUI" src
    if [ -d "$TGSAGUI/src" ]; then rm -r "$TGSAGUI/src"; fi
    mkdir "$TGSAGUI/src"
    for e in "$TINYGO_DIR"/src/*; do
        if [ "$(basename "$e")" = device ]; then ln -s "$e" "$TGSAGUI/src/device"
        else cp -rs "$e" "$TGSAGUI/src/"; fi
    done
    rm "$TGSAGUI/src/runtime/runtime_unix.go"
    cp "$TINYGO_DIR/src/runtime/runtime_unix.go" "$TGSAGUI/src/runtime/runtime_unix.go"
    python3 -I "$RAIZ/userland/go/runtime/parchear_tinygo.py" "$TGSAGUI/src/runtime/runtime_unix.go"
    echo "$s" > "$GOSAGUI/.sello"
    echo "$s" > "$TGSAGUI/.sello"
    echo "Copias de SaguiOS preparadas: $GOSAGUI y $TGSAGUI"
}

case "${1:-}" in
instalar)
    mkdir -p "$BASE"
    bajar TinyGo "$TINYGO_URL" "$TINYGO_SHA" "$TINYGO_DIR" "$TINYGO_VER"
    bajar Go "$GO_URL" "$GO_SHA" "$GO_DIR" "$GO_VER"
    preparar
    echo "Listo. Compilar un programa: tools/go-sagui build -o /tmp/hola ./userland/go/ejemplos/hola"
    ;;
preparar)
    preparar
    ;;
env)
    preparar
    echo "export GOROOT='$GOSAGUI'"
    echo "export TINYGOROOT='$TGSAGUI'"
    echo "export PATH='$TINYGO_DIR/bin:$GOSAGUI/bin'\${PATH:+:\$PATH}"
    echo "export GOTOOLCHAIN=local GOFLAGS=-mod=mod GOWORK=off GO111MODULE=on GOPROXY=off"
    ;;
estado)
    for d in "$TINYGO_DIR" "$GO_DIR"; do
        if [ -f "$d/.version" ]; then echo "$(basename "$d") $(cat "$d/.version"): $d"; else echo "$(basename "$d"): no instalado"; fi
    done
    ;;
*)
    sed -n '3,12p' "$0" | sed 's/^# \{0,1\}//'
    exit 2
    ;;
esac
