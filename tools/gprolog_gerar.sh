#!/usr/bin/env bash
# gprolog_gerar.sh - fuentes y código generado de GNU Prolog para SaguiOS (D189).
#
# GNU Prolog compila su biblioteca (los predicados escritos en Prolog) con sus
# propias herramientas: pl2wam (.pl -> .wam), wam2ma (.wam -> .ma, el
# «mini-ensamblador») y ma2asm (.ma -> .s para la CPU). Dentro de SaguiOS no
# hay ensamblador ni enlazador para eso, así que esta parte se hace en el
# anfitrión: este guion compila GNU Prolog en el PC para i686 de 32 bits (con
# la libc de 32 bits del entorno local) y genera con él los .s de i686 que el
# Makefile ensambla y enlaza con nuestra libc.
#
#   tools/gprolog_gerar.sh [--vendorizar] [--comprobar] [--trabajo CARPETA]
#
#   (sin opciones)  baja el tarball (si no está), comprueba su SHA-256, compila
#                   GNU Prolog en el anfitrión y regenera
#                   userland/terceros/gprolog/gerado/ (los .s y las cabeceras
#                   que produce la configuración) a partir de los .wam de
#                   userland/terceros/gprolog/fonte/.
#   --vendorizar    copia del tarball a fonte/ los ficheros del upstream que
#                   usamos. ¡Pisa nuestros cambios! (están listados en
#                   userland/terceros/gprolog/LEEME.txt): solo para subir de versión.
#   --comprobar     compara fonte/ con el tarball y lista lo que difiere (tiene
#                   que ser exactamente lo del LEEME.txt).
#   --trabajo DIR   dónde se baja y se compila (por omisión build/gprolog-anfitriao).
#
# El resultado es determinista: con el mismo tarball y los mismos .wam salen
# los mismos .s, y se guardan en git para que `make` no necesite nada de esto.
set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[ -f "$HOME/.local/saguitools/env.sh" ] && . "$HOME/.local/saguitools/env.sh"

VERSAO=1.5.0
URL="https://ftp.gnu.org/gnu/gprolog/gprolog-$VERSAO.tar.gz"
SHA256=670642b43c0faa27ebd68961efb17ebe707688f91b6809566ddd606139512c01
DEST="$RAIZ/userland/terceros/gprolog"
TRAB="$RAIZ/build/gprolog-anfitriao"
VENDORIZAR=0
COMPROBAR=0

while [ $# -gt 0 ]; do
    case "$1" in
        --vendorizar) VENDORIZAR=1; shift ;;
        --comprobar) COMPROBAR=1; shift ;;
        --trabajo) TRAB="$2"; shift 2 ;;
        -h|--help) sed -n '2,28p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "opción desconocida: $1" >&2; exit 2 ;;
    esac
done

mkdir -p "$TRAB"
TAR="$TRAB/gprolog-$VERSAO.tar.gz"
if [ ! -f "$TAR" ]; then
    echo "bajando $URL"
    curl -fsSL -o "$TAR.parcial" "$URL"
    mv "$TAR.parcial" "$TAR"
fi
echo "$SHA256  $TAR" | sha256sum -c --quiet - || { echo "SHA-256 distinto: $TAR" >&2; exit 1; }

# Siempre se descomprime en limpio: la compilación del anfitrión escribe en el árbol.
ORIG="$TRAB/orig"
rm -rf "$ORIG"
mkdir -p "$ORIG"
tar -xzf "$TAR" -C "$ORIG"
SRC="$ORIG/gprolog-$VERSAO/src"

# Lo que usamos del upstream (rutas relativas a src/). Los .wam los trae el
# tarball ya generados por pl2wam (son independientes de la máquina).
lista_upstream() {
    (cd "$SRC" && {
        ls EnginePl/*.c EnginePl/*.h EnginePl/wam_archi.def EnginePl/gp_config.h.in
        ls BipsPl/*.c BipsPl/*.h BipsPl/*.pl BipsPl/*.wam
        echo TopComp/top_level_main.c
        echo TopComp/prolog_path.c
        echo Linedit/linedit.h
        echo Linedit/ctrl_c.h
        echo Linedit/ctrl_c.c
        echo TopComp/copying.c
        for m in pl2wam read_file syn_sugar internal code_gen reg_alloc inst_codif first_arg indexing wam_emit; do
            echo "Pl2Wam/$m.pl"; echo "Pl2Wam/$m.wam"
        done
    } | grep -v -e 'EnginePl/gp_config.h$' -e 'EnginePl/.*_SIGSEGV\.c$' -e 'EnginePl/test_oc' \
                -e 'EnginePl/try_sigaction\.c$' -e 'EnginePl/cpp_headers\.c$' \
                -e 'BipsPl/sockets' -e 'BipsPl/le_interf' -e 'BipsPl/t_c\.c$' | sort)
}

if [ "$VENDORIZAR" = 1 ]; then
    echo "copiando el upstream a $DEST/fonte (pisa los cambios nuestros)"
    rm -rf "$DEST/fonte"
    lista_upstream | while read -r f; do
        mkdir -p "$DEST/fonte/$(dirname "$f")"
        cp "$SRC/$f" "$DEST/fonte/$f"
    done
    cp "$ORIG/gprolog-$VERSAO/COPYING" "$DEST/COPYING"
fi

if [ "$COMPROBAR" = 1 ]; then
    dif=0
    while read -r f; do
        if [ ! -f "$DEST/fonte/$f" ]; then echo "falta: $f"; dif=1
        elif ! cmp -s "$SRC/$f" "$DEST/fonte/$f"; then echo "cambiado: $f"; dif=1; fi
    done < <(lista_upstream)
    (cd "$DEST/fonte" && find . -type f | sed 's|^\./||' | sort) | while read -r f; do
        [ -f "$SRC/$f" ] || echo "nuestro (no está en el upstream): $f"
    done
    cmp -s "$ORIG/gprolog-$VERSAO/COPYING" "$DEST/COPYING" || echo "cambiado: COPYING"
    exit 0
fi

# ---- compilar GNU Prolog en el anfitrión (i686, 32 bits) ----
# El compilador del entorno local tiene el soporte de 32 bits (gcc-13-multilib)
# y la glibc de 32 bits en root/usr/lib32: se enlaza estático para no depender
# del cargador de 32 bits del sistema. La libm.a de 32 bits de esa glibc no
# trae fmod() (solo fmodl): se le da una de una línea.
R="$HOME/.local/saguitools/root"
GCC32="$TRAB/gcc32"
cat > "$GCC32" <<EOF
#!/bin/bash
export CPATH="$R/usr/include"
enl=1
for a in "\$@"; do case "\$a" in -c|-S|-E) enl=0;; esac; done
if [ \$enl = 1 ]; then set -- "\$@" "$TRAB/fmod32.o" -lm; fi
exec env -u LIBRARY_PATH gcc -m32 -B"$R/usr/lib32/" -L"$R/usr/lib32" -static "\$@"
EOF
chmod +x "$GCC32"
echo 'long double fmodl(long double, long double); double fmod(double x, double y) { return (double)fmodl(x, y); }' > "$TRAB/fmod32.c"
"$GCC32" -O2 -c "$TRAB/fmod32.c" -o "$TRAB/fmod32.o"

# Las mismas opciones que la versión de SaguiOS: sin registros de la máquina
# para la WAM (--disable-regs, ver D189), sin sockets, sin el resolvedor de
# dominios finitos, sin el editor de líneas (el terminal de SaguiOS ya edita)
# y sin tubería para la entrada de pl2wam en consult/1 (pl2wam hereda el terminal).
# Las pilas de SaguiOS son más pequeñas (fonte/EnginePl/wam_archi.def, D189):
# pl_config escribe sus tamaños en wam_stacks.h.
# Nuestro pl_config.c escribe en wam_archi.h las macros de marcas con la base
# de las direcciones (TAG_BASE_ENDERECO: 0x80000000 con __saguios__, 0 en el
# anfitrión, que así queda igual que el upstream), ver D189.
cp "$DEST/fonte/EnginePl/wam_archi.def" "$SRC/EnginePl/wam_archi.def"
cp "$DEST/fonte/EnginePl/pl_config.c" "$SRC/EnginePl/pl_config.c"
echo "configurando GNU Prolog $VERSAO para i686 (registro en $TRAB/configure.log)"
(cd "$SRC" && CC="$GCC32" ./configure --build=i686-pc-linux-gnu --host=i686-pc-linux-gnu \
    --disable-regs --disable-sockets --disable-fd-solver --disable-linedit --disable-piped-consult \
    > "$TRAB/configure.log" 2>&1)
echo "compilando (registro en $TRAB/make.log)"
(cd "$SRC" && make -j2 > "$TRAB/make.log" 2>&1)

# ---- generar los .s de i686 a partir de los .wam de fonte/ ----
GER="$DEST/gerado"
rm -rf "$GER"
mkdir -p "$GER/EnginePl"
for w in "$DEST"/fonte/BipsPl/*.wam "$DEST"/fonte/Pl2Wam/*.wam; do
    b=$(basename "$w" .wam)
    d=$(basename "$(dirname "$w")")
    "$SRC/Wam2Ma/wam2ma" "$w" -o "$TRAB/$b.ma"
    "$SRC/Ma2Asm/ma2asm" "$TRAB/$b.ma" -o "$TRAB/$b.s"
    {
        echo "# $d/$b.s - generado por tools/gprolog_gerar.sh (wam2ma y ma2asm de GNU Prolog $VERSAO)"
        echo "# a partir de fonte/$d/$b.wam. No se edita a mano. Licencia: la de GNU Prolog (COPYING)."
        cat "$TRAB/$b.s"
    } > "$GER/$d-$b.s"
done

# ---- las cabeceras que produce la configuración ----
# wam_archi.h, wam_regs.h, wam_stacks.h y gprolog_cst.h las escribe pl_config
# (de wam_archi.def) y son de la máquina (i686, 32 bits): se copian tal cual.
for h in wam_archi.h wam_regs.h wam_stacks.h gprolog_cst.h; do
    cp "$SRC/EnginePl/$h" "$GER/EnginePl/$h"
done
# gp_config.h es la del configure del anfitrión menos lo que SaguiOS no
# tiene: mmap/mprotect (las pilas de la WAM van en el heap), termios,
# malloc.h/mallopt, endian.h/byteswap.h y la ruta del gcc del anfitrión.
# sigaction y sigsetjmp se quedan: nuestra libc los tiene (sigaction no hace
# nada, porque no hay señales; los desbordamientos se miran por software).
sed -e 's|^#define HAVE_MMAP 1|/* SaguiOS: sin mmap (D189) */|' \
    -e 's|^#define HAVE_MPROTECT 1|/* SaguiOS: sin mprotect (D189) */|' \
    -e 's|^#define HAVE_MALLOPT 1|/* SaguiOS: sin mallopt (D189) */|' \
    -e 's|^#define HAVE_MALLOC_H 1|/* SaguiOS: sin malloc.h (D189) */|' \
    -e 's|^#define HAVE_TERMIOS_H 1|/* SaguiOS: sin termios (D189) */|' \
    -e 's|^#define HAVE_ENDIAN_H 1|/* SaguiOS: sin endian.h (D189) */|' \
    -e 's|^#define HAVE_BYTESWAP_H 1|/* SaguiOS: sin byteswap.h (D189) */|' \
    -e 's|^#define HAVE_DECL_HTOLE32 1|#define HAVE_DECL_HTOLE32 0 /* SaguiOS (D189) */|' \
    -e 's|^#define HAVE_DECL_BSWAP_32 1|#define HAVE_DECL_BSWAP_32 0 /* SaguiOS (D189) */|' \
    -e 's|^#define CC ".*"|#define CC "gcc"|' \
    -e 's|^#define PROLOG_DATE ".*"|#define PROLOG_DATE "2021"|' \
    -e 's|^#define PROLOG_COPYRIGHT ".*"|#define PROLOG_COPYRIGHT "Copyright (C) 1999-2021 Daniel Diaz"|' \
    -e 's|^#define M_OS ".*"|#define M_OS "saguios"|' \
    "$SRC/EnginePl/gp_config.h" > "$GER/EnginePl/gp_config.h"

echo "listo: $(ls "$GER"/*.s | wc -l) ficheros .s en $GER ($(du -sh "$GER" | cut -f1))"
echo "el gprolog del anfitrión (para comparar) está en $SRC/TopComp/gprolog"
