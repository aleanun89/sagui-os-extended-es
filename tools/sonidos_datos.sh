#!/bin/sh
# sonidos_datos.sh - rehace el esquema de sonidos «Kenney» (D421) a partir del paquete «Interface Sounds» de Kenney.
#
# Origen:    https://kenney.nl/assets/interface-sounds (la misma ZIP, copia en OpenGameArt:
#            https://opengameart.org/content/interface-sounds). Licencia: CC0 1.0 (dominio público), comprobada
#            en la página de Kenney («Creative Commons CC0») y en el License.txt de la propia ZIP.
# Resultado: userland/terceros/kenney-sonidos/ con los diez WAV elegidos (mono, 22050 Hz, 16 bits), el esquema.conf,
#            CREDITOS.txt y LICENCIA.txt. Esa carpeta SÍ está en git (son ~170 KB y CC0): la compilación no necesita
#            red ni ffmpeg. Este guion solo hace falta para regenerarla o cambiar la elección.
# Uso:       sh tools/sonidos_datos.sh        (necesita curl, unzip, ffmpeg y sha256sum)
set -eu
cd "$(dirname "$0")/.."
URL=https://opengameart.org/sites/default/files/kenney_interfaceSounds.zip
SHA256=f2193d072726d6758a5f7871b2dcc54dcce0d5c35c6f0a62f92549b327c81232
DESTINO=userland/terceros/kenney-sonidos
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT

curl -fsSL -A "Mozilla/5.0" -o "$T/is.zip" "$URL"
[ "$(sha256sum "$T/is.zip" | cut -d' ' -f1)" = "$SHA256" ] || { echo "sonidos_datos: la suma SHA-256 no coincide (¿cambió el paquete?)" >&2; exit 1; }
unzip -q "$T/is.zip" -d "$T/z"
grep -q "CC0" "$T/z/License.txt" || { echo "sonidos_datos: el License.txt no dice CC0" >&2; exit 1; }

# evento = fichero de Interface Sounds
ELECCION="arranque:glass_004 apagar:close_004 reiniciar:switch_001 aviso:glass_001 error:error_005 pregunta:question_001
usb_conectar:maximize_001 usb_desconectar:minimize_001 bateria:error_003 papelera:drop_004"
rm -rf "$DESTINO"; mkdir -p "$DESTINO"
{
  echo "# esquema.conf del esquema «Kenney» (D421): evento=archivo"
  echo "nombre=Kenney"
  echo "creditos=Sonidos: Kenney (kenney.nl), CC0"
} > "$DESTINO/esquema.conf"
for par in $ELECCION; do
  ev=${par%%:*}; of=${par#*:}
  ffmpeg -v error -y -i "$T/z/Audio/$of.ogg" -ac 1 -ar 22050 -acodec pcm_s16le -map_metadata -1 -fflags +bitexact -flags:a +bitexact "$DESTINO/$ev.wav"
  echo "$ev=$ev.wav" >> "$DESTINO/esquema.conf"
done
cp "$T/z/License.txt" "$DESTINO/LICENCIA.txt"
FECHA="$(date +%Y-%m-%d)"
cat > "$DESTINO/CREDITOS.txt" <<EOF
Sonidos del esquema «Kenney» de SaguiOS-Extended
================================================
Paquete: «Interface Sounds» de Kenney (www.kenney.nl)
Página:  https://kenney.nl/assets/interface-sounds
         (copia: https://opengameart.org/content/interface-sounds)
Licencia: Creative Commons Zero (CC0 1.0), dominio público: http://creativecommons.org/publicdomain/zero/1.0/
          La atribución no es obligatoria; se da por cortesía. Ver LICENCIA.txt.
Descargado: $FECHA (ZIP con SHA-256 $SHA256)
Cambios: convertidos de OGG a WAV mono de 22050 Hz y 16 bits (tools/sonidos_datos.sh); sin más edición.
Elección (evento: fichero original): $(echo $ELECCION | tr '\n' ' ')
EOF
echo "sonidos_datos: listo en $DESTINO"
