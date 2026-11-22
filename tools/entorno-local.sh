#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# entorno-local.sh - monta el entorno de compilación de SaguiOS sin sudo
# (mejora 005). Para WSL o para una máquina donde no se pueda instalar nada
# con apt.
#
# Qué hace:
#   1. descarga con "apt-get download" los paquetes que faltan (nasm, el
#      soporte de 32 bits de gcc, QEMU, mtools, dosfstools...) y los extrae
#      en ~/.local/saguitools/root;
#   2. crea ~/.local/saguitools/env.sh, con las rutas, y los envoltorios de
#      gcc (que conserva -m32) y de QEMU (con la ruta de datos y de módulos);
#   3. pone convert e identify (sustitutos mínimos del ImageMagick) en el
#      PATH, para lo que usan los tests.
#   4. pone ffmpeg, zip y sqlite3 (los usan las pruebas para generar el
#      material de prueba) en ~/.local/saguitools/extra, aparte de root para
#      que sus bibliotecas no se mezclen con las del compilador y el QEMU.
#
# Uso:  bash tools/entorno-local.sh
# Después:  . ~/.local/saguitools/env.sh && make
set -e

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BASE="$HOME/.local/saguitools"
ROOT="$BASE/root"
PKGS="$BASE/pkgs"
DADOS="$BASE/dados"
BIN="$BASE/bin"

echo "Entorno local de SaguiOS en $BASE"

# ---------------------------------------------------------------- 1. paquetes
mkdir -p "$PKGS" "$ROOT" "$DADOS" "$BIN"

# Lo que hace falta para compilar y para las pruebas.
QUERIDOS="nasm mtools dosfstools exfatprogs e2fsprogs binutils gcc-multilib qemu-system-x86 qemu-utils"

# Un paquete está bien si está instalado en el sistema o si ya se extrajo aquí
# (para no volver a descargar lo de una vez anterior).
EXTRAIDO="$ROOT/usr/bin/qemu-system-i386"

echo "Calculando lo que falta..."
FALTAN=""
for p in $QUERIDOS; do
    if ! dpkg -s "$p" >/dev/null 2>&1; then
        if [ -x "$EXTRAIDO" ] || [ -f "$ROOT/usr/bin/$p" ] || [ -f "$ROOT/usr/sbin/$p" ]; then
            continue        # ya está en el entorno local
        fi
        FALTAN="$FALTAN $p"
    fi
done

if [ -n "$FALTAN" ]; then
    # Con las dependencias, menos lo que ya está instalado.
    DEP=$(apt-cache depends --recurse --no-recommends --no-suggests \
              --no-conflicts --no-breaks --no-replaces --no-enhances $QUERIDOS 2>/dev/null |
          grep -E '^[a-zA-Z0-9]' | sort -u)
    for p in $DEP; do
        if ! dpkg -s "$p" >/dev/null 2>&1; then
            FALTAN="$FALTAN $p"
        fi
    done
    echo "Descargando:$FALTAN"
    ( cd "$PKGS" && apt-get download $FALTAN )
    for d in "$PKGS"/*.deb; do
        dpkg-deb -x "$d" "$ROOT/"
    done
else
    echo "Los paquetes ya están instalados en el sistema."
fi

# El soporte de 32 bits de gcc no está en una instalación normal, así que se
# copia aparte y el envoltorio de gcc lo indica con -B.
MULTI="$ROOT/usr/lib/gcc/x86_64-linux-gnu/13/32"
if [ ! -d "$MULTI" ]; then
    echo "AVISO: no se encuentra el soporte de 32 bits ($MULTI)."
    echo "       Las compilaciones con -m32 no funcionarán."
fi

# C++ (D228): el cc1plus de g++-13, el mismo gcc-13 del sistema. Sin él no se
# compilan los programas de C++ (OpenJazz; el sistema se construye igual, sin
# ellos). Se extrae aparte y el envoltorio g++ (abajo) lo indica con -B. El
# paquete tiene que ser de la misma versión que el gcc-13 instalado: apt-get
# download baja la del sistema.
CC1PLUS_SIS=/usr/libexec/gcc/x86_64-linux-gnu/13/cc1plus
CC1PLUS="$ROOT/usr/libexec/gcc/x86_64-linux-gnu/13/cc1plus"
if [ ! -x "$CC1PLUS_SIS" ] && [ ! -x "$CC1PLUS" ]; then
    echo "Descargando el compilador de C++ (g++-13-x86-64-linux-gnu)..."
    if ( cd "$PKGS" && apt-get download g++-13-x86-64-linux-gnu ); then
        for d in "$PKGS"/g++-13-x86-64-linux-gnu_*.deb; do dpkg-deb -x "$d" "$ROOT/"; done
    fi
    [ -x "$CC1PLUS" ] || echo "AVISO: sin cc1plus; los programas de C++ (OpenJazz) no se compilarán."
fi

# OVMF (D264): el firmware UEFI libre para QEMU, de 64 bits (ovmf) y de 32 (ovmf-ia32),
# para las pruebas del arranque UEFI (h264) y run_in_qemu.sh --uefi. Queda en
# $ROOT/usr/share/OVMF, donde lo buscan tests/qemu.py y run_in_qemu.sh.
if { [ ! -f /usr/share/OVMF/OVMF_CODE_4M.fd ] && [ ! -f "$ROOT/usr/share/OVMF/OVMF_CODE_4M.fd" ]; } ||
   { [ ! -f /usr/share/OVMF/OVMF32_CODE_4M.fd ] && [ ! -f "$ROOT/usr/share/OVMF/OVMF32_CODE_4M.fd" ]; }; then
    echo "Descargando el firmware UEFI de QEMU (ovmf, ovmf-ia32)..."
    if ( cd "$PKGS" && apt-get download ovmf ovmf-ia32 ); then
        for d in "$PKGS"/ovmf_*.deb "$PKGS"/ovmf-ia32_*.deb; do [ -f "$d" ] && dpkg-deb -x "$d" "$ROOT/"; done
    fi
    [ -f "$ROOT/usr/share/OVMF/OVMF_CODE_4M.fd" ] || [ -f /usr/share/OVMF/OVMF_CODE_4M.fd ] ||
        echo "AVISO: sin OVMF; la prueba h264 (arranque UEFI) no podrá correr."
fi

# xfsprogs (D446): mkfs.xfs, xfs_db y xfs_repair para crear y comprobar las imágenes XFS de la
# prueba h446. Con sus bibliotecas que falten (libinih1, liburcu8t64 en Ubuntu 24.04). Quedan en
# $ROOT/sbin y $ROOT/usr/sbin, que ya están en el PATH de env.sh.
if ! command -v mkfs.xfs >/dev/null 2>&1 && [ ! -x "$ROOT/sbin/mkfs.xfs" ] && [ ! -x "$ROOT/usr/sbin/mkfs.xfs" ]; then
    echo "Descargando xfsprogs (pruebas de XFS)..."
    FALTAN_XFS="xfsprogs"
    for p in $(apt-cache depends xfsprogs 2>/dev/null | sed -n 's/^ *Depends: \([a-z0-9][a-z0-9.+-]*\)$/\1/p'); do
        dpkg -s "$p" >/dev/null 2>&1 || FALTAN_XFS="$FALTAN_XFS $p"
    done
    mkdir -p "$PKGS/xfs"
    if ( cd "$PKGS/xfs" && apt-get download $FALTAN_XFS ); then
        for d in "$PKGS"/xfs/*.deb; do dpkg-deb -x "$d" "$ROOT/"; done
    fi
    [ -x "$ROOT/sbin/mkfs.xfs" ] || [ -x "$ROOT/usr/sbin/mkfs.xfs" ] || echo "AVISO: sin mkfs.xfs; la prueba h446 (XFS) no podrá correr."
fi

# ------------------------------------------------------- 2. datos de QEMU
# El BIOS (seabios) y las ROM de iPXE: QEMU los busca en /usr/share/qemu.
for f in "$ROOT"/usr/share/seabios/*.bin "$ROOT"/usr/share/qemu/*.bin; do
    [ -f "$f" ] && cp -n "$f" "$DADOS/" 2>/dev/null || true
done
for f in "$ROOT"/usr/lib/ipxe/qemu/*.rom; do
    [ -f "$f" ] && cp "$f" "$DADOS/" 2>/dev/null || true
done
[ -d "$ROOT/usr/share/qemu/keymaps" ] && cp -rn "$ROOT/usr/share/qemu/keymaps" "$DADOS/" 2>/dev/null || true

# El guion de enlazado de la libc local apunta a la libc_nonshared.a que
# viene en el libc6-dev; si el sistema no lo tiene, se deja apuntando a la
# copia extraída.
NONS="$ROOT/usr/lib/x86_64-linux-gnu/libc_nonshared.a"
if [ -f "$NONS" ] && [ ! -f /usr/lib/x86_64-linux-gnu/libc_nonshared.a ]; then
    cat > "$ROOT/usr/lib/x86_64-linux-gnu/libc.so" <<EOF
/* GNU ld script (copia local, apuntando a la libc_nonshared.a extraida). */
OUTPUT_FORMAT(elf64-x86-64)
GROUP ( /lib/x86_64-linux-gnu/libc.so.6 $NONS AS_NEEDED ( /lib64/ld-linux-x86-64.so.2 ) )
EOF
fi

# --------------------------------------------------------- 3. envoltorios
cat > "$BIN/gcc" <<'EOF'
#!/bin/bash
# Envoltorio de gcc: usa el gcc-13 del sistema y le indica dónde está el
# soporte de 32 bits (gcc-13-multilib) del entorno local, solo cuando compila
# para i386 (las herramientas del anfitrión son de 64 bits).
#
# IMPORTANTE: -m32 no puede perderse. Si se cae, gcc compila para x86-64 con
# -march=i686 y falla con "CPU you selected does not support x86-64
# instruction set"; y si no hubiera -march=i686, el binario saldría de 64 bits
# sin que nadie se entere. Aquí se conserva siempre y se comprueba.
R="$HOME/.local/saguitools/root"
args=()
m32=0
for a in "$@"; do
  args+=("$a")
  if [ "$a" = "-m32" ]; then
    m32=1
    args+=(-B"$R/usr/lib/gcc/x86_64-linux-gnu/13/32/")
  fi
done
if [ "$m32" = "1" ]; then
  if ! /usr/bin/gcc-13 -m32 -E -x c /dev/null >/dev/null 2>&1; then
    echo "gcc: el gcc-13 del sistema no acepta -m32" >&2
    exit 1
  fi
fi
exec /usr/bin/gcc-13 "${args[@]}"
EOF
cp "$BIN/gcc" "$BIN/cpp"
chmod +x "$BIN/gcc" "$BIN/cpp"

# cc: algunas Makefiles (la del litemonkey, h88) llaman a "cc", que en un
# Linux mínimo no existe (solo gcc). Mismo compilador que el de gcc.
cat > "$BIN/cc" <<'EOF'
#!/bin/bash
# Envoltorio de cc: en un Linux mínimo solo está gcc, y las Makefiles que
# llaman a "cc" fallan. Se usa el mismo compilador que el envoltorio de gcc.
exec "$HOME/.local/saguitools/bin/gcc" "$@"
EOF
chmod +x "$BIN/cc"

# g++ (D228): el mismo envoltorio de gcc, con la carpeta del cc1plus del
# entorno local (si el sistema no lo tiene). Solo compila (-c): los programas
# de C++ del SaguiOS se enlazan con ld, sin la libstdc++ del anfitrión.
cat > "$BIN/g++" <<'EOF'
#!/bin/bash
# Envoltorio de g++ (D228): el gcc-13 del sistema (por el envoltorio de gcc,
# que conserva el -m32) con el cc1plus del entorno local si el sistema no lo
# tiene. Los ficheros .cpp/.cc se compilan como C++; no enlaza la libstdc++.
L=/usr/libexec/gcc/x86_64-linux-gnu/13
[ -x "$L/cc1plus" ] || L="$HOME/.local/saguitools/root/usr/libexec/gcc/x86_64-linux-gnu/13"
exec "$HOME/.local/saguitools/bin/gcc" -B"$L/" "$@"
EOF
chmod +x "$BIN/g++"

cat > "$BIN/qemu-system-i386" <<'EOF'
#!/bin/bash
# Envoltorio de qemu-system-i386 para el entorno local sin sudo.
# Añade la ruta de los datos (BIOS, VGA, ROM) y la de los módulos (el
# acelerador TCG va como módulo en los paquetes de Ubuntu), que en una
# instalación normal están en /usr/share/qemu y /usr/lib/x86_64-linux-gnu/qemu.
# Se define aquí el LD_LIBRARY_PATH para que el envoltorio funcione aunque no
# se haya cargado env.sh (las pruebas lanzan QEMU como subproceso).
S="$HOME/.local/saguitools"
export LD_LIBRARY_PATH="$S/root/usr/lib/i386-linux-gnu:$S/root/usr/lib/x86_64-linux-gnu:$S/root/lib/x86_64-linux-gnu:$S/root/usr/lib:${LD_LIBRARY_PATH:-}"
export QEMU_MODULE_DIR="$S/root/usr/lib/x86_64-linux-gnu/qemu"
exec "$S/root/usr/bin/qemu-system-i386" -L "$S/dados" "$@"
EOF
chmod +x "$BIN/qemu-system-i386"

# D264: el mismo envoltorio para qemu-system-x86_64 (el OVMF de 64 bits necesita una CPU
# de 64 bits; las pruebas del arranque UEFI x64 lo usan).
sed 's/qemu-system-i386/qemu-system-x86_64/g' "$BIN/qemu-system-i386" > "$BIN/qemu-system-x86_64"
chmod +x "$BIN/qemu-system-x86_64"

# convert e identify: sustitutos mínimos del ImageMagick, en el repositorio.
# Se COPIAN, no se enlazan: un enlace simbólico se queda colgando en cuanto el
# checkout cambia de rama (y la rama donde no están), y entonces las pruebas
# dejan de poder sacar capturas.
for t in convert identify; do
    if [ -f "$RAIZ/tools/$t" ]; then
        rm -f "$BIN/$t"
        cp "$RAIZ/tools/$t" "$BIN/$t"
        chmod +x "$BIN/$t"
    fi
done

# ------------------------------------- 4. herramientas que usan las pruebas
# ffmpeg, zip y sqlite3 los necesitan las pruebas para generar el material de
# prueba (h75 y h86 codifican audio, h76 comprime y h87 compara con el
# sqlite3 del anfitrión). En un Linux mínimo no están, y sin ellos esas
# pruebas no llegan ni a empezar.
#
# Van en una carpeta aparte (extra) y no en root: las bibliotecas de ffmpeg
# (libav*, libx264...) no deben mezclarse con las del compilador y las de
# QEMU, que ya están en root.
EXTRA="$BASE/extra"
mkdir -p "$EXTRA" "$PKGS/extra"
QUERIDOS_EXTRA="ffmpeg zip sqlite3"
FALTAN_EXTRA=""
for p in $QUERIDOS_EXTRA; do
    if ! dpkg -s "$p" >/dev/null 2>&1; then
        FALTAN_EXTRA="$FALTAN_EXTRA $p"
    fi
done
if [ -n "$FALTAN_EXTRA" ]; then
    DEP_EXTRA=$(apt-cache depends --recurse --no-recommends --no-suggests \
                    --no-conflicts --no-breaks --no-replaces --no-enhances $QUERIDOS_EXTRA 2>/dev/null |
                grep -E '^[a-zA-Z0-9]' | sort -u)
    for p in $DEP_EXTRA; do
        dpkg -s "$p" >/dev/null 2>&1 && continue
        # Las fuentes y los instaladores no hacen falta para codificar audio.
        case "$p" in
            fonts-*|xfonts-*|cdebconf|debconf|libdebian-installer4|sysvinit-utils) continue ;;
        esac
        FALTAN_EXTRA="$FALTAN_EXTRA $p"
    done
    echo "Descargando (herramientas de las pruebas):$FALTAN_EXTRA"
    ( cd "$PKGS/extra" && apt-get download $FALTAN_EXTRA )
    for d in "$PKGS"/extra/*.deb; do
        dpkg-deb -x "$d" "$EXTRA"
    done
fi
if [ ! -x "$EXTRA/usr/bin/ffmpeg" ]; then
    echo "AVISO: no se encuentra ffmpeg en $EXTRA; h75, h86 y h87 fallarán."
fi

# -------------------------------------------------------------- 5. env.sh
cat > "$BASE/env.sh" <<'EOF'
# Entorno local de SaguiOS sin sudo (mejora 005, tools/entorno-local.sh).
# Uso: . ~/.local/saguitools/env.sh && make && python3 tests/rodar_todos.py
#
# Ojo: las variables que ya pueden existir en el entorno se ponen con ${VAR:-},
# porque run_in_qemu.sh usa "set -u" y al cargar este archivo una variable que
# no estaba definida abortaría el script.
SAGUI_ROOT="$HOME/.local/saguitools/root"
SAGUI_EXTRA="$HOME/.local/saguitools/extra"
export PATH="$HOME/.local/saguitools/bin:$SAGUI_ROOT/usr/bin:$SAGUI_ROOT/usr/sbin:$SAGUI_ROOT/sbin:$SAGUI_EXTRA/usr/bin${PATH:+:$PATH}"
# Go con TinyGo (D364, tools/tinygo_entorno.sh): solo si esta instalado.
for _d in "$HOME/.local/saguitools/tinygo/bin" "$HOME/.local/saguitools/go/bin"; do
    [ -d "$_d" ] && PATH="$_d:$PATH"
done
unset _d
# JDK para compilar Java en el PC (D376, tools/java_entorno.sh): al FINAL del PATH, solo si esta instalado.
[ -d "$HOME/.local/saguitools/jdk/bin" ] && PATH="$PATH:$HOME/.local/saguitools/jdk/bin"
export PATH
# Las herramientas del propio checkout (convert, identify) van delante: cada
# copia de trabajo (miniOS_master_os_*) usa las suyas, no las de otra copia.
if [ -x "${PWD:-}/tools/convert" ]; then
    export PATH="$PWD/tools:$PATH"
fi
# Las bibliotecas de extra (ffmpeg y compañía) van al final: las del sistema
# mandan y sólo se usan las suyas cuando el sistema no tiene esa biblioteca.
export LD_LIBRARY_PATH="$SAGUI_ROOT/usr/lib/i386-linux-gnu:$SAGUI_ROOT/usr/lib/x86_64-linux-gnu:$SAGUI_ROOT/lib/x86_64-linux-gnu:$SAGUI_ROOT/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}:$SAGUI_EXTRA/usr/lib/x86_64-linux-gnu:$SAGUI_EXTRA/usr/lib/x86_64-linux-gnu/samba:$SAGUI_EXTRA/lib/x86_64-linux-gnu"
# Sin el root/usr/lib32: los binarios del sistema enlazan con -nostdlib y las
# herramientas del anfitrión son de 64 bits (mezclarlos rompe el enlazado).
export LIBRARY_PATH="$SAGUI_ROOT/usr/lib/i386-linux-gnu:$SAGUI_ROOT/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:$LIBRARY_PATH}"
# CPATH es SOLO para las herramientas que corren en el PC (el anfitrión no trae
# libc6-dev: chip8, litemonkey...). GCC lo añade incluso con -nostdinc, así que
# el Makefile de SaguiOS lo anula para lo que va dentro del sistema (D191).
export CPATH="$SAGUI_ROOT/usr/include${CPATH:+:$CPATH}"
export QEMU_MODULE_DIR="$SAGUI_ROOT/usr/lib/x86_64-linux-gnu/qemu"
export QEMU_AUDIO_DRV=none
export SDL_AUDIODRIVER=dummy
export QT_QPA_PLATFORM=offscreen
EOF

echo
echo "Listo. Ahora:"
echo "  . ~/.local/saguitools/env.sh && make"
echo "  python3 tests/rodar_paralelo.py -j 2 --repetir-falhas"
