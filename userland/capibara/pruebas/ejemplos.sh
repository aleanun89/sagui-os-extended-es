#!/bin/sh
# SPDX-License-Identifier: MIT
# Corre los dos ejemplos sin ventana (SDL_VIDEODRIVER=dummy): cuadros fijos, entrada guionada y el mismo hash del último
# cuadro en dos ejecuciones. Lo llama «make prueba»; tests/h452_capibara.py hace comprobaciones más completas.
cd "$(dirname "$0")/.." || exit 1
fallo=0
for ej in "plataformas 150 5:derecha+;60:saltar+;70:saltar-" "disparos 300 3:disparar+"; do
	set -- $ej
	a=$(CAPI_ENTRADA="$3" ./build/capibara ejemplos/$1 --cuadros $2 | grep CAPI_HASH)
	b=$(CAPI_ENTRADA="$3" ./build/capibara ejemplos/$1 --cuadros $2 | grep CAPI_HASH)
	if [ -n "$a" ] && [ "$a" = "$b" ]; then echo "ok: $1 determinista ($a)"; else echo "FALLO: $1 ($a / $b)"; fallo=1; fi
done
exit $fallo
