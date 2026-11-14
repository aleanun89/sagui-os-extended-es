#!/usr/bin/env bash
# gnuboy_roms_libres.sh - baja ROM de Game Boy libres, solo para uso propio y para probar (D235).
#
# Las ROM NUNCA van a git ni a las imágenes publicadas: este guion las baja (con su SHA-256) a
# userland/terceros/gnuboy/roms-libres/ (ignorada por git) y de ahí las toma quien compila: copiar
# a input_share/games/gb/ (docs/input_share.md) las mete en /jogos/gb de su imagen.
#
# Lo que baja (la licencia de cada una, comprobada en el repositorio de su autor):
#
#   libbet.gb  «Libbet and the Magic Floor» v0.08, un juego de puzles de Damian Yerrick (Pino).
#              https://github.com/pinobatch/libbet (versión v0.08, libbet.gb de la publicación).
#              Licencia: zlib (el LICENSE del repositorio; el README, en «Legal», dice que el
#              programa se puede usar bajo los términos de la licencia zlib; copyright 2002, 2012
#              Martin Korth y 2018, 2024 Damian Yerrick). La zlib permite usarla, modificarla y
#              redistribuirla con la condición de no tergiversar el origen y mantener el aviso.
#              Es una ROM de 32 KB sin batería. El aviso (LICENSE) se baja a su lado.
#
# No se incluye ninguna comercial. Sin red, nada se baja: la prueba (tests/h235_gnuboy.py) usa
# entonces su ROM propia (tools/gnuboy_rom_teste.py).
#
#   tools/gnuboy_roms_libres.sh [--comprobar]
#     --comprobar   solo dice si las ROM ya están bajadas y con el SHA-256 bueno
set -euo pipefail

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/userland/terceros/gnuboy/roms-libres"
COMPROBAR=0
[ "${1:-}" = "--comprobar" ] && COMPROBAR=1

# nombre  URL  SHA-256
ROMS=(
  "libbet.gb|https://github.com/pinobatch/libbet/releases/download/v0.08/libbet.gb|3607412031c8287cf878299ce96e581e85b852dde703806343b95576fa3ff1a9"
)
AVISOS=(
  "libbet-LICENSE.txt|https://raw.githubusercontent.com/pinobatch/libbet/v0.08/LICENSE"
)

for h in curl sha256sum; do
    command -v "$h" >/dev/null 2>&1 || { echo "gnuboy_roms_libres.sh: falta $h" >&2; exit 1; }
done

mal=0
for e in "${ROMS[@]}"; do
    IFS='|' read -r nombre url sha <<<"$e"
    f="$DEST/$nombre"
    if [ -f "$f" ] && echo "$sha  $f" | sha256sum -c --quiet - 2>/dev/null; then
        echo "gnuboy_roms_libres.sh: $nombre bien (SHA-256 $sha)"
        continue
    fi
    if [ "$COMPROBAR" = 1 ]; then
        echo "gnuboy_roms_libres.sh: $nombre falta o no coincide" >&2
        mal=1
        continue
    fi
    mkdir -p "$DEST"
    echo "gnuboy_roms_libres.sh: descargando $url"
    curl -fsSL -o "$f.nuevo" "$url"
    if ! echo "$sha  $f.nuevo" | sha256sum -c --quiet -; then
        rm -f "$f.nuevo"
        echo "gnuboy_roms_libres.sh: el SHA-256 de $nombre no es el esperado" >&2
        exit 1
    fi
    mv "$f.nuevo" "$f"
    echo "gnuboy_roms_libres.sh: $nombre guardada en $DEST"
done
if [ "$COMPROBAR" = 0 ]; then
    for e in "${AVISOS[@]}"; do
        IFS='|' read -r nombre url <<<"$e"
        curl -fsSL -o "$DEST/$nombre" "$url" || echo "gnuboy_roms_libres.sh: no se pudo bajar el aviso $nombre" >&2
    done
fi
exit $mal
