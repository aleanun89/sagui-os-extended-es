#!/usr/bin/env bash
# openjazz_datos.sh - baja el shareware de Jazz Jackrabbit para OpenJazz (D228).
#
# OpenJazz es libre, pero los datos del juego (gráficos, música, niveles) son
# de Epic MegaGames. La versión shareware (episodio 1, v1.0) la distribuía
# Epic gratis; su LICENSE.DOC deja copiarla sin cobrar «siempre que no se
# altere ni se quite ningún fichero» y pone condiciones a meterla en
# colecciones (no más de 10 programas en un CD, nada de discos con otros
# programas). Una imagen del SaguiOS es un disco con muchos programas, así que
# el shareware NO va en git ni en las imágenes que se publican
# (gerar_versao.sh compila con OPENJAZZ_DATOS=0): este guion lo baja a una
# carpeta ignorada por git, para quien compila su propia imagen.
#
#   tools/openjazz_datos.sh [--zip FICHERO] [--forzar]
#
#   --zip FICHERO   usar un jjrabbit.zip ya descargado (se comprueba igual el SHA-256)
#   --forzar        rehacer aunque ya esté preparado
#
# Deja los ficheros, todos y sin cambios, en userland/terceros/openjazz/datos/jazz/
# y la marca .preparado; el make los copia a /jogos/openjazz de la partición de
# juegos (OPENJAZZ_DATOS=0 lo evita).
set -euo pipefail

# El zip del shareware 1.0 que usa el paquete de OpenJazz de SlackBuilds.org.
URL="https://slackware.uk/~urchlay/src/jjrabbit.zip"
SHA256=b35114020f278432e1f06fc92b76e168e058ea1aecdc4b00bca76ab78499d455

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/userland/terceros/openjazz/datos"
ZIP=""
FORZAR=0

while [ $# -gt 0 ]; do
    case "$1" in
        --zip)    ZIP="$2"; shift 2 ;;
        --forzar) FORZAR=1; shift ;;
        -h|--help|--ayuda) sed -n '2,23p' "$0"; exit 0 ;;
        *) echo "openjazz_datos.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done

if [ -f "$DEST/.preparado" ] && [ "$FORZAR" = 0 ]; then
    echo "openjazz_datos.sh: ya preparado en $DEST (--forzar para rehacer)"
    exit 0
fi
for h in curl sha256sum python3; do
    command -v "$h" >/dev/null 2>&1 || { echo "openjazz_datos.sh: falta $h" >&2; exit 1; }
done

mkdir -p "$DEST"
TMP="$(mktemp -d "$DEST/tmp.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

if [ -z "$ZIP" ]; then
    ZIP="$TMP/jjrabbit.zip"
    echo "openjazz_datos.sh: descargando $URL"
    curl -fsSL -o "$ZIP" "$URL"
fi
echo "$SHA256  $ZIP" | sha256sum -c --quiet - || {
    echo "openjazz_datos.sh: el SHA-256 de $ZIP no es el esperado" >&2
    exit 1
}

# Se extrae con el zipfile de Python (no hace falta unzip), negándose a
# nombres con rutas: el zip trae los ficheros en su raíz.
mkdir "$TMP/jazz"
python3 -I - "$ZIP" "$TMP/jazz" <<'PY'
import os, sys, zipfile
zip_, dest = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(zip_) as z:
    for i in z.infolist():
        nome = i.filename
        if nome.endswith('/') or '/' in nome or '\\' in nome or nome.startswith('.'):
            sys.exit('openjazz_datos.sh: nombre inesperado en el zip: %r' % nome)
        with z.open(i) as o, open(os.path.join(dest, nome), 'wb') as d:
            d.write(o.read())
print('openjazz_datos.sh: %d ficheros' % len(os.listdir(dest)))
PY
test -f "$TMP/jazz/MAINCHAR.000" && test -f "$TMP/jazz/LICENSE.DOC"

rm -rf "$DEST/jazz"
mv "$TMP/jazz" "$DEST/jazz"
echo "$SHA256 $URL" > "$DEST/.preparado"
echo "openjazz_datos.sh: shareware en $DEST/jazz (va a /jogos/openjazz al hacer make)"
