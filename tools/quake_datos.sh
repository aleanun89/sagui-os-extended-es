#!/usr/bin/env bash
# quake_datos.sh - baja el shareware de Quake (versión 1.06) para quien compila su imagen (D229).
#
# El motor de Quake es libre (GPL), pero los datos (mapas, gráficos, sonidos:
# id1/pak0.pak) son de id Software. El shareware (el episodio 1) lo
# distribuía id gratis; su licencia (SLICNSE.TXT, junio de 1996) deja al
# usuario usarlo y darlo «gratis y entero», y a los proveedores de Internet
# distribuirlo comprimido y con la licencia al lado, pero prohíbe venderlo,
# alquilarlo o explotarlo comercialmente de cualquier forma, y reproducirlo o
# exhibirlo públicamente. Una imagen del SaguiOS que se publica no es «el
# shareware entero» (llevaría solo el pak0.pak dentro de otro sistema), así
# que el shareware NO va en git ni en las imágenes que se publican
# (gerar_versao.sh compila con QUAKE_DATOS=0): este guion lo baja, para uso
# propio, a una carpeta ignorada por git.
#
#   tools/quake_datos.sh [--zip FICHERO] [--forzar]
#
#   --zip FICHERO   usar un quake106.zip ya descargado (se comprueba igual el SHA-256)
#   --forzar        rehacer aunque ya esté preparado
#
# Deja todos los ficheros del shareware, sin cambios (ID1/PAK0.PAK, las
# licencias, los textos y los ejecutables de DOS), en
# userland/terceros/quake/datos/quake/ y la marca .preparado; el make los
# copia a /jogos/quake de la partición de juegos (QUAKE_DATOS=0 lo evita).
set -euo pipefail

# quake106.zip, el de ftp.idsoftware.com/idstuff/quake/ (la réplica de la GWDG
# y, si falla, la de youfailit.net; las dos dan el mismo fichero).
URLS=(
    "https://ftp.gwdg.de/pub/misc/ftp.idsoftware.com/idstuff/quake/quake106.zip"
    "https://youfailit.net/pub/idgames/idstuff/quake/quake106.zip"
)
SHA256=ec6c9d34b1ae0252ac0066045b6611a7919c2a0d78a3a66d9387a8f597553239
# El pak0.pak del shareware 1.06 (18.689.235 bytes).
SHA256_PAK=35a9c55e5e5a284a159ad2a62e0e8def23d829561fe2f54eb402dbc0a9a946af

RAIZ="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$RAIZ/userland/terceros/quake/datos"
ZIP=""
FORZAR=0

while [ $# -gt 0 ]; do
    case "$1" in
        --zip)    ZIP="$2"; shift 2 ;;
        --forzar) FORZAR=1; shift ;;
        -h|--help|--ayuda) sed -n '2,24p' "$0"; exit 0 ;;
        *) echo "quake_datos.sh: opción desconocida: $1" >&2; exit 2 ;;
    esac
done

if [ -f "$DEST/.preparado" ] && [ "$FORZAR" = 0 ]; then
    echo "quake_datos.sh: ya preparado en $DEST (--forzar para rehacer)"
    exit 0
fi
for h in curl sha256sum python3; do
    command -v "$h" >/dev/null 2>&1 || { echo "quake_datos.sh: falta $h" >&2; exit 1; }
done

mkdir -p "$DEST"
TMP="$(mktemp -d "$DEST/tmp.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

if [ -z "$ZIP" ]; then
    ZIP="$TMP/quake106.zip"
    for u in "${URLS[@]}"; do
        echo "quake_datos.sh: descargando $u"
        if curl -fsSL -o "$ZIP" "$u"; then break; fi
        rm -f "$ZIP"
    done
    [ -f "$ZIP" ] || { echo "quake_datos.sh: no se pudo descargar quake106.zip" >&2; exit 1; }
fi
echo "$SHA256  $ZIP" | sha256sum -c --quiet - || {
    echo "quake_datos.sh: el SHA-256 de $ZIP no es el esperado" >&2
    exit 1
}

# El zip trae el instalador de DOS y resource.1, un autoextraíble LHA con el
# juego: se saca resource.1 con el zipfile de Python y se extrae con nuestro
# descompresor (tools/lha_extraer.py), que comprueba el CRC de cada fichero.
python3 -I - "$ZIP" "$TMP" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    with z.open('resource.1') as o, open(sys.argv[2] + '/resource.1', 'wb') as d:
        d.write(o.read())
PY
mkdir "$TMP/quake"
python3 -I "$RAIZ/tools/lha_extraer.py" "$TMP/resource.1" "$TMP/quake" >/dev/null
echo "$SHA256_PAK  $TMP/quake/ID1/PAK0.PAK" | sha256sum -c --quiet - || {
    echo "quake_datos.sh: el ID1/PAK0.PAK extraído no es el del shareware 1.06" >&2
    exit 1
}
test -f "$TMP/quake/SLICNSE.TXT"

rm -rf "$DEST/quake"
mv "$TMP/quake" "$DEST/quake"
echo "$SHA256 quake106.zip" > "$DEST/.preparado"
echo "quake_datos.sh: shareware en $DEST/quake (va a /jogos/quake al hacer make)"
