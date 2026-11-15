#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# 86box_instalar.sh - instala 86Box (emulador de PC antiguos, GPL v2) sin sudo (D301).
#
# 86Box sirve para probar SaguiOS en hardware de la época que no tenemos (Pentium,
# Pentium II, K6, gráficas 3dfx/S3/ATI/Matrox, Sound Blaster, NE2000...). Se usa solo
# en pruebas concretas, a petición; NUNCA forma parte de la batería (docs/86box.md).
#
# Qué hace (todo en ~/.local/saguitools/86box/, fuera del repositorio):
#   1. descarga la AppImage estable de 86Box (versión fija, SHA-256) y la extrae con
#      --appimage-extract (en WSL no hay FUSE);
#   2. descarga las ROM del repositorio 86Box/roms (etiqueta que casa con la versión).
#      Son BIOS con copyright: nunca van a git ni al repositorio público;
#   3. trae sin sudo (apt-get download + dpkg-deb -x) lo que falta en el anfitrión:
#      Xvfb, xdotool, xwd y libpcre3 (la AppImage lo pide);
#   4. crea los envoltorios en ~/.local/saguitools/86box/bin (86box, xvfb, xdotool, xwd).
#      No toca el PATH ni env.sh: los guiones llaman a estas rutas directamente.
#
# Uso:  bash tools/86box_instalar.sh [--comprobar] [--windows]
#   --comprobar  solo informa de lo que hay y de lo que falta (no descarga nada)
#   --windows    instala además (o solo, con --windows-solo) el 86Box NATIVO de Windows x64 en
#                %LOCALAPPDATA%\saguitools\86box\ (fuera del repositorio), con las mismas ROM;
#                sirve para ./run_in_86box.sh --windows (el ratón captura bien, a diferencia de WSLg).
#                Necesita la interoperabilidad de WSL (cmd.exe responde). Las ROM, igual que en
#                Linux, NO SE SUBEN NUNCA.
set -eu

VERSION_86BOX="v6.0"
COMPILACION_86BOX="b9001"
APPIMAGE="86Box-Linux-x86_64-${COMPILACION_86BOX}.AppImage"
SHA_APPIMAGE="535d3c329f8c32b601103970c86d80aa80298f345452c3524ea2f433624727a4"
URL_APPIMAGE="https://github.com/86Box/86Box/releases/download/${VERSION_86BOX}/${APPIMAGE}"

VERSION_ROMS="v6.0"
SHA_ROMS="38fccaf4a7b48e581ccdfee2dc3cc0545e7d3a778bc25611e902a117eec571e6"
URL_ROMS="https://github.com/86Box/roms/archive/refs/tags/${VERSION_ROMS}.tar.gz"

BASE="$HOME/.local/saguitools/86box"
DESC="$BASE/descargas"
APP="$BASE/app"
ROMS="$BASE/roms"
DEPS="$BASE/deps"
BIN="$BASE/bin"

# Paquetes de Ubuntu que se extraen sin instalar (los que no estén ya en el sistema).
PAQUETES="libpcre3 xvfb xdotool libxdo3 x11-apps x11-xkb-utils xkb-data libxfont2 libfontenc1
          libxkbfile1 libxmuu1 libunwind8 libpixman-1-0 libxshmfence1 libgl1 libglx0
          libxcb-xtest0 libxcb-cursor0 libxcb-util1 libxcb-icccm4 libxcb-image0
          libxcb-keysyms1 libxcb-randr0 libxcb-render-util0 libxcb-shape0 libxcb-xkb1
          libxcb-xinerama0 libxcb-xfixes0 libxcb-render0 libxcb-sync1 libxcb-shm0 libxcb-xv0"

ZIP_WIN="86Box-Windows-64-${COMPILACION_86BOX}.zip"
SHA_ZIP_WIN="b86722ed547089f095e8875805411f83c76aafd7d318f67286a5a69aecdd7644"
URL_ZIP_WIN="https://github.com/86Box/86Box/releases/download/${VERSION_86BOX}/${ZIP_WIN}"

SOLO_COMPROBAR=0
WINDOWS=0
SOLO_WINDOWS=0
for a in "$@"; do
    case "$a" in
        --comprobar) SOLO_COMPROBAR=1 ;;
        --windows) WINDOWS=1 ;;
        --windows-solo) WINDOWS=1; SOLO_WINDOWS=1 ;;
        *) echo "opción desconocida: $a" >&2; exit 2 ;;
    esac
done

mkdir -p "$DESC" "$BIN"

verificar() {   # verificar fichero sha256
    local f="$1" esperado="$2" real
    real=$(sha256sum "$f" | cut -d' ' -f1)
    if [ "$real" != "$esperado" ]; then
        echo "86box_instalar: SHA-256 incorrecto en $f" >&2
        echo "  esperado $esperado" >&2
        echo "  obtenido $real" >&2
        return 1
    fi
}

descargar() {   # descargar url destino sha
    if [ -f "$2" ] && verificar "$2" "$3" 2>/dev/null; then return 0; fi
    echo "Descargando $1"
    curl -fL --retry 3 -o "$2.tmp" "$1"
    mv "$2.tmp" "$2"
    verificar "$2" "$3"
}


# ------------------------------------------------------------- 86Box de Windows (--windows)
# Devuelve (en WIN_BASE) la ruta WSL de %LOCALAPPDATA%\saguitools\86box.
ruta_windows() {
    command -v cmd.exe >/dev/null 2>&1 || { echo "86box_instalar: no hay interoperabilidad con Windows (cmd.exe no está en el PATH de WSL; revisa [interop] en /etc/wsl.conf)" >&2; return 1; }
    local l
    # cmd.exe se ejecuta desde /mnt/c: con un directorio \\wsl.localhost\ como actual se niega.
    l=$(cd /mnt/c && cmd.exe /c 'echo %LOCALAPPDATA%' 2>/dev/null | tr -d '\r') || true
    [ -n "$l" ] && [ "$l" != '%LOCALAPPDATA%' ] || { echo "86box_instalar: cmd.exe no responde o no da %LOCALAPPDATA%" >&2; return 1; }
    WIN_BASE="$(wslpath -u "$l")/saguitools/86box"
}

instalar_windows() {
    ruta_windows || exit 1
    local W="$WIN_BASE"
    mkdir -p "$DESC" "$W"
    if [ ! -f "$W/86Box.exe" ]; then
        descargar "$URL_ZIP_WIN" "$DESC/$ZIP_WIN" "$SHA_ZIP_WIN"
        python3 -I - "$DESC/$ZIP_WIN" "$W" <<'PY'
import sys, zipfile
zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])
PY
    fi
    [ -f "$W/86Box.exe" ] || { echo "86box_instalar: el zip no contenía 86Box.exe" >&2; exit 1; }
    echo "86Box ${VERSION_86BOX} (${COMPILACION_86BOX}) de Windows en $W"
    # ROM: las mismas del tarball de Linux (nunca dentro del repositorio).
    if [ ! -d "$W/roms/machines" ]; then
        if [ ! -d "$ROMS/machines" ]; then
            descargar "$URL_ROMS" "$DESC/roms-${VERSION_ROMS}.tar.gz" "$SHA_ROMS"
            mkdir -p "$ROMS"; tar xzf "$DESC/roms-${VERSION_ROMS}.tar.gz" -C "$ROMS" --strip-components=1
        fi
        mkdir -p "$W/roms"
        cp -a "$ROMS/." "$W/roms/"
    fi
    echo "ROM ${VERSION_ROMS} en $W/roms (con copyright: nunca a git)"
    mkdir -p "$W/vm"
}

# ------------------------------------------------------------- comprobación
if [ "$SOLO_COMPROBAR" = 1 ]; then
    for r in "$APP/AppRun" "$ROMS/machines" "$DEPS/root/usr/bin/Xvfb" "$DEPS/root/usr/bin/xdotool" \
             "$DEPS/root/usr/bin/xwd" "$BIN/86box" "$BIN/xvfb"; do
        if [ -e "$r" ]; then echo "ok      $r"; else echo "FALTA   $r"; fi
    done
    if unshare -rm true 2>/dev/null; then echo "ok      espacios de nombres de usuario (Xvfb sin xkbcomp del sistema)";
    else echo "FALTA   unshare -rm (sin él, Xvfb necesita: sudo apt install x11-xkb-utils)"; fi
    if ruta_windows 2>/dev/null; then
        for r in "$WIN_BASE/86Box.exe" "$WIN_BASE/roms/machines"; do
            if [ -e "$r" ]; then echo "ok      $r (Windows)"; else echo "FALTA   $r (bash tools/86box_instalar.sh --windows)"; fi
        done
    else
        echo "sin interoperabilidad con Windows: --windows no disponible"
    fi
    exit 0
fi

if [ "$WINDOWS" = 1 ] && [ "$SOLO_WINDOWS" = 1 ]; then
    instalar_windows
    "$(dirname "${BASH_SOURCE[0]}")/86box_roms_prohibidas.sh" --instalar-gancho || true
    exit 0
fi

# ------------------------------------------------------------- 1. 86Box
if [ ! -x "$APP/AppRun" ]; then
    descargar "$URL_APPIMAGE" "$DESC/$APPIMAGE" "$SHA_APPIMAGE"
    chmod +x "$DESC/$APPIMAGE"
    rm -rf "$DESC/squashfs-root" "$APP"
    # Sin FUSE (WSL): se extrae en vez de montarla.
    ( cd "$DESC" && "./$APPIMAGE" --appimage-extract >/dev/null )
    mv "$DESC/squashfs-root" "$APP"
fi
echo "86Box ${VERSION_86BOX} (${COMPILACION_86BOX}) en $APP"

# ------------------------------------------------------------- 2. ROM
if [ ! -d "$ROMS/machines" ]; then
    descargar "$URL_ROMS" "$DESC/roms-${VERSION_ROMS}.tar.gz" "$SHA_ROMS"
    rm -rf "$ROMS"
    mkdir -p "$ROMS"
    tar xzf "$DESC/roms-${VERSION_ROMS}.tar.gz" -C "$ROMS" --strip-components=1
fi
echo "ROM ${VERSION_ROMS} en $ROMS (con copyright: nunca a git)"

# ------------------------------------------------------------- 3. dependencias
mkdir -p "$DEPS/debs" "$DEPS/root"
FALTAN=""
for p in $PAQUETES; do
    dpkg -s "$p" >/dev/null 2>&1 || FALTAN="$FALTAN $p"
done
if [ ! -x "$DEPS/root/usr/bin/Xvfb" ] && [ -n "$FALTAN" ]; then
    echo "Descargando paquetes sin sudo:$FALTAN"
    ( cd "$DEPS/debs" && apt-get download $FALTAN )
    for d in "$DEPS"/debs/*.deb; do dpkg-deb -x "$d" "$DEPS/root"; done
fi
# Qt5 y SDL2 de 86Box vienen dentro de la AppImage; solo se miran las bibliotecas del sistema
# que pida (X11/xcb). Lo que falte y no esté en deps se dice con la orden apt exacta.
export LD_LIBRARY_PATH="$APP/usr/lib:$APP/usr/lib/x86_64-linux-gnu:$DEPS/root/usr/lib/x86_64-linux-gnu:$DEPS/root/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
SINLIB=$(cd "$APP" && ldd usr/local/bin/86Box usr/lib/x86_64-linux-gnu/qt5/plugins/platforms/libqxcb.so 2>/dev/null | grep "not found" | sort -u || true)
if [ -n "$SINLIB" ]; then
    echo "Faltan bibliotecas en el anfitrión:" >&2
    echo "$SINLIB" >&2
    echo "Para el mantenedor (no se ejecuta aquí):  sudo apt install libpcre3 libxcb-cursor0 xvfb xdotool x11-apps x11-xkb-utils" >&2
fi
if ! unshare -rm true 2>/dev/null; then
    echo "Aviso: no hay espacios de nombres de usuario; Xvfb necesitará xkbcomp del sistema." >&2
    echo "Para el mantenedor (no se ejecuta aquí):  sudo apt install xvfb xdotool x11-apps x11-xkb-utils" >&2
fi

# ------------------------------------------------------------- 4. envoltorios
LIBS="$APP/usr/lib:$APP/usr/lib/x86_64-linux-gnu:$DEPS/root/usr/lib/x86_64-linux-gnu:$DEPS/root/lib/x86_64-linux-gnu"

cat > "$BIN/86box" <<EOF
#!/bin/bash
# Generado por tools/86box_instalar.sh. El intérprete de la AppImage es relativo: hay que
# arrancar desde su carpeta; las rutas de los argumentos deben ser absolutas.
export LD_LIBRARY_PATH="$LIBS\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
cd "$APP" && exec ./AppRun "\$@"
EOF

for h in xdotool xwd; do
cat > "$BIN/$h" <<EOF
#!/bin/bash
export LD_LIBRARY_PATH="$LIBS\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
exec "$DEPS/root/usr/bin/$h" "\$@"
EOF
done

# Xvfb: el servidor llama a /usr/bin/xkbcomp (ruta fija) y quiere /tmp/.X11-unix con 1777.
# Sin sudo se resuelve con un espacio de nombres propio: una superposición sobre /usr/bin con
# nuestro xkbcomp y un tmpfs en /tmp/.X11-unix. Los clientes conectan por el socket abstracto.
# Uso: xvfb FICHERO_DISPLAY [ANCHOxALTOxPROF]  (escribe el número de pantalla en FICHERO_DISPLAY;
# el PID del guion es el del servidor: se para con kill PID).
cat > "$BIN/xvfb-interno" <<EOF
#!/bin/bash
# Se ejecuta dentro del espacio de nombres (lo lanza xvfb): TMP, FICH y RES vienen del entorno.
set -e
if [ ! -x /usr/bin/xkbcomp ]; then
    mount -t overlay overlay -o lowerdir=/usr/bin,upperdir=\$TMP/up,workdir=\$TMP/work /usr/bin
    cp $DEPS/root/usr/bin/xkbcomp /usr/bin/xkbcomp
fi
mount -t tmpfs -o mode=1777 tmpfs /tmp/.X11-unix
echo "\$NUM" > "\$FICH"
# Las bibliotecas se ponen después de los mount (las de la AppImage rompen a mount).
export LD_LIBRARY_PATH="$DEPS/root/usr/lib/x86_64-linux-gnu:$DEPS/root/lib/x86_64-linux-gnu"
exec $DEPS/root/usr/bin/Xvfb ":\$NUM" -screen 0 "\$RES" -nolisten tcp -xkbdir $DEPS/root/usr/share/X11/xkb
EOF

cat > "$BIN/xvfb" <<EOF
#!/bin/bash
export FICH="\${1:?uso: xvfb FICHERO_DISPLAY [RESOLUCION]}"
# Número de pantalla libre desde el 90 (el :0 es el de WSLg): sin cerrojo y sin socket abstracto.
for NUM in \$(seq 90 150); do
    [ -e "/tmp/.X\$NUM-lock" ] && continue
    grep -q "X\$NUM\\\$" /proc/net/unix && continue
    break
done
export NUM
export RES="\${2:-1280x1024x24}"
export TMP="\$(mktemp -d)"
mkdir -p "\$TMP/up" "\$TMP/work"
exec unshare -rm bash "$BIN/xvfb-interno" 2>>"\$TMP/xvfb.log"
EOF
chmod +x "$BIN"/*
echo "Envoltorios en $BIN (86box, xvfb, xdotool, xwd)"
# Gancho pre-commit: las ROM de 86Box NO SE PUEDEN SUBIR NUNCA (si ya hay otro gancho, lo dice).
"$(dirname "${BASH_SOURCE[0]}")/86box_roms_prohibidas.sh" --instalar-gancho || true
[ "$WINDOWS" = 1 ] && instalar_windows
echo "Listo. Comprobación: bash tools/86box_instalar.sh --comprobar"
