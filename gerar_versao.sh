#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# gerar_versao.sh - genera una versión de SaguiOS y deja la entrega montada.
#
# Es el hermano de run_in_qemu.sh: aquel arranca el sistema ya compilado en el
# QEMU; este compila la versión, arma las imágenes de arranque (disco, VDI,
# ISO...) y las deja en una carpeta de entrega con sus sumas SHA-256 y una
# hoja de datos (VERSION.txt) con lo que se compiló.
#
# La carpeta de entrega por defecto es entrega/latest: siempre apunta a la
# última versión generada, sinversionar. Con --publicar se copia además a
# entrega/v<versión> (por ejemplo entrega/v6.4), que es donde vive la entrega
# congelada de cada versión.
#
# Uso: ./gerar_versao.sh [opciones]
#   --destino DIR      carpeta donde queda la entrega (por defecto: entrega/latest)
#   --publicar         copia además el resultado a entrega/v<versión>
#   --iso              incluye la ISO de CD (arranque desde el CD, D077)
#   --fat32 [MB]       incluye la imagen persistente con FAT32 (por defecto 2048 MB)
#   --ext2 [MB]        incluye la imagen persistente con ext2 (por defecto 64 MB)
#   --sin-gpl          incluye la variante sin licencia GPL (D109)
#   --smp              incluye la imagen multinúcleo (saguios-smp.img: --smp, D164; sin IOAPIC)
#                      y, con --ext2/--fat32, también las persistentes multinúcleo
#   --uacpi            incluye las imágenes con el intérprete de AML (uACPI, D192: batería):
#                      saguios-uacpi.img y, con --smp y --ext2, saguios-persistente-smp-uacpi.img
#   --particiones      incluye el disco con /home, /tmp y /logs en particiones (D120), comprimido
#   --todas            todo lo anterior: ISO, FAT32, ext2, sin GPL, multinúcleo, uACPI y particiones
#   --variante V       variante de la imagen principal: gpl (por defecto),
#                      sem-gpl o sem-firmware
#   --sin-python       compila sin MicroPython (una imagen bastante más pequeña)
#   --otimizacao O     nivel de optimización: -O0, -O1, -O2 (por defecto), -O3 u -Os
#   --jobs N           trabajos en paralelo de make (por defecto: los núcleos del PC)
#   --limpiar          borra la carpeta de compilación antes de compilar (desde cero)
#   --sin-compilar     no compila: usa lo que ya haya en la carpeta de compilación
#   --sin-notas        no escribe el fichero VERSION.txt
#   --notas FICHERO    añade al final de VERSION.txt el texto de ese fichero
#   --fijar-version V  deja la versión V en kernel/inc/versao.h antes de compilar
#   --probar           arranca la imagen en el QEMU y comprueba que llega al escritorio
#   --con-input-share  mete lo de input_share/, los datos de OpenJazz y de Quake bajados con
#                      tools/openjazz_datos.sh y tools/quake_datos.sh y el navegador NetSurf
#                      (para uso propio: NO publicar, D228, D229, D258)
#   -h, --help         esta ayuda
#
# Tamaños de las particiones por variables de entorno (los valores por
# defecto entre paréntesis; una opción en la línea de órdenes manda sobre la
# variable):
#   DATOS_FAT32_MB  /home de la imagen persistente FAT32 (2048; también --fat32 N)
#   DATOS_MB        /home de la imagen persistente ext2 (64; también --ext2 N)
#   HOME_MB TMP_MB LOGS_MB   el disco con particiones (2048, 256, 128)
#   PART_FS         sistema de ficheros de esas particiones (fat32; o ext2)
#   PART_EXT        «--primarias»: el formato de antes de la v7.3 (por defecto, todas las de datos como lógicas en una extendida, D213)
#   JOGOS_MB        partición de juegos SAGUI-JOGOS de las persistentes y la de
#                   particiones (800; 0 = el justo para los datos, D228)
#   JOGOS_IMG_MB    la de saguios.img y saguios-smp.img, que van a git (0 = el justo)
#   JOGOS=0         todas las imágenes sin la partición de juegos
#   INPUT_SHARE     por defecto vacío: lo de input_share/ (juegos completos de quien
#                   compila, D228) NO entra en la entrega; con --con-input-share, sí
#   APPS_MB         partición de aplicaciones SAGUI-APPS en /apps (0 = lo justo
#                   más un margen para instalar otras: el doble, al menos 4 MB más)
#   APPS=0          sin la partición SAGUI-APPS: las apps (NetSurf, sqlite3) van
#                   dentro del ramdisk, en /apps (imagen de un solo bloque, D194)
#   RAMDISK=completo  lo mismo que APPS=0 (D324); por defecto «base»: el ramdisk solo
#                   lleva lo fundamental y las aplicaciones van en SAGUI-APPS. La ISO
#                   es siempre «completo» (el kernel no lee el CD)
# Por ejemplo:  HOME_MB=8192 JOGOS_MB=64 APPS_MB=64 ./gerar_versao.sh --particiones --smp
#
# Lo que se compila se puede parametrizar (variante de licencia, MicroPython,
# nivel de optimización); lo que se entrega se puede elegir con las opciones
# de arriba. El script no toca el código más que en --fijar-version.
#
# Ejemplos:
#   ./gerar_versao.sh --iso --fat32 --publicar
#   ./gerar_versao.sh --todas --destino entrega/v7.0b
#   ./gerar_versao.sh --variante sem-gpl --destino entrega/sin-gpl
#   ./gerar_versao.sh --sin-python --otimizacao -Os
set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$RAIZ"

DESTINO="$RAIZ/entrega/latest"
PUBLICAR=0
QUERER_ISO=0
QUERER_FAT32=0
QUERER_EXT2=0
QUERER_SINGPL=0
QUERER_SMP=0
QUERER_UACPI=0
QUERER_PART=0
VARIANTE=gpl
SIN_PYTHON=0
OTIMIZACAO="-O2"
FAT32_MB="${DATOS_FAT32_MB:-2048}"
EXT2_MB="${DATOS_MB:-64}"
export HOME_MB="${HOME_MB:-2048}" TMP_MB="${TMP_MB:-256}" LOGS_MB="${LOGS_MB:-128}"
export PART_FS="${PART_FS:-fat32}" PART_EXT="${PART_EXT:-}" JOGOS_MB="${JOGOS_MB:-800}" JOGOS="${JOGOS:-1}"
export JOGOS_IMG_MB="${JOGOS_IMG_MB:-0}"
# D228: lo que quien compila pone en input_share/ (un juego completo comprado...) y el
# shareware de Jazz Jackrabbit no se pueden publicar: la entrega se hace sin ellos.
CON_INPUT_SHARE=0
export APPS_MB="${APPS_MB:-0}" APPS="${APPS:-1}" RAMDISK="${RAMDISK:-base}"
JOBS="$(nproc 2>/dev/null || echo 2)"
LIMPIAR=0
COMPILAR=1
NOTAS=1
FICHERO_NOTAS=""
FIJAR_VERSION=""
PROBAR=0

ajuda() { sed -n '2,/^set -e/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'; }

error() { echo "gerar_versao: $*" >&2; exit 1; }

# Un número opcional detrás de la opción: --fat32 2048, pero --fat32 --iso
# también vale (entonces sale el tamaño por defecto).
opcion_numero() {
    case "${2:-}" in
        ''|*[!0-9]*) return 1 ;;
        *) return 0 ;;
    esac
}

while [ $# -gt 0 ]; do
    case "$1" in
        --destino) DESTINO="${2:?falta la carpeta de destino}"; shift ;;
        --publicar) PUBLICAR=1 ;;
        --iso) QUERER_ISO=1 ;;
        --fat32) QUERER_FAT32=1
                 if opcion_numero "$2"; then FAT32_MB="$2"; shift; fi ;;
        --ext2) QUERER_EXT2=1
                if opcion_numero "$2"; then EXT2_MB="$2"; shift; fi ;;
        --sin-gpl) QUERER_SINGPL=1 ;;
        --smp) QUERER_SMP=1 ;;
        --uacpi) QUERER_UACPI=1 ;;
        --particiones) QUERER_PART=1 ;;
        --todas) QUERER_ISO=1; QUERER_FAT32=1; QUERER_EXT2=1; QUERER_SINGPL=1; QUERER_SMP=1; QUERER_UACPI=1; QUERER_PART=1 ;;
        --variante) VARIANTE="${2:?falta la variante}"; shift ;;
        --sin-python) SIN_PYTHON=1 ;;
        --otimizacao) OTIMIZACAO="${2:?falta el nivel de optimización}"; shift ;;
        --jobs|-j) JOBS="${2:?falta el número de trabajos}"; shift ;;
        --limpiar) LIMPIAR=1 ;;
        --sin-compilar) COMPILAR=0 ;;
        --sin-notas) NOTAS=0 ;;
        --notas) FICHERO_NOTAS="${2:?falta el fichero de notas}"; shift ;;
        --fijar-version) FIJAR_VERSION="${2:?falta la versión}"; shift ;;
        --probar) PROBAR=1 ;;
        --con-input-share) CON_INPUT_SHARE=1 ;;
        -h|--help) ajuda; exit 0 ;;
        *) error "opción desconocida: $1 (usa --help)" ;;
    esac
    shift
done

case "$VARIANTE" in
    gpl|sem-gpl|sem-firmware) ;;
    *) error "variante desconocida: $VARIANTE (usa gpl, sem-gpl o sem-firmware)" ;;
esac
case "$OTIMIZACAO" in
    -O0|-O1|-O2|-O3|-Os) ;;
    *) error "nivel de optimización desconocido: $OTIMIZACAO" ;;
esac
[ "$SIN_PYTHON" = 1 ] && [ "$VARIANTE" != gpl ] && \
    error "--sin-python no se puede combinar con --variante $VARIANTE"

# El entorno local (nasm, gcc de 32 bits, qemu-img) si está instalado.
# Ojo: env.sh se carga sin "set -u", porque alguna de sus variables puede no
# estar definida en el entorno (ver tools/entorno-local.sh).
if [ -f "$HOME/.local/saguitools/env.sh" ]; then
    set +u
    # shellcheck disable=SC1091
    . "$HOME/.local/saguitools/env.sh"
    set -u
fi

# ---------- versión ----------
ler_definicao() {
    sed -n "s/^#define[[:space:]]*$1[[:space:]]*\"\\([^\"]*\\)\".*/\\1/p" kernel/inc/versao.h
}

if [ -n "$FIJAR_VERSION" ]; then
    case "$FIJAR_VERSION" in
        v[0-9]*) ;;
        *) error "la versión debe empezar por v (por ejemplo v6.4): $FIJAR_VERSION" ;;
    esac
    echo "Fijando la versión $FIJAR_VERSION en kernel/inc/versao.h"
    sed -i "s/^#define[[:space:]]*SAGUI_VERSAO[[:space:]]*\".*\"/#define SAGUI_VERSAO       \"$FIJAR_VERSION\"/" \
        kernel/inc/versao.h
    grep -n "^#define SAGUI_VERSAO" kernel/inc/versao.h
fi

NOME="$(ler_definicao SAGUI_NOME)"
VERSAO="$(ler_definicao SAGUI_VERSAO)"
[ -n "$VERSAO" ] || error "no se encuentra SAGUI_VERSAO en kernel/inc/versao.h"

# ---------- carpeta de compilación ----------
# Cada variante de licencia compila en su propia carpeta (así lo hace el
# Makefile); la variante sin MicroPython, en otra, para no mezclar binarios.
case "$VARIANTE" in
    gpl)           B=build; MAKE_VAR=() ;;
    sem-gpl)       B=build-sem-gpl; MAKE_VAR=(SEM_GPL=1) ;;
    sem-firmware)  B=build-sem-firmware; MAKE_VAR=(SEM_FIRMWARE=1) ;;
esac
if [ "$SIN_PYTHON" = 1 ]; then
    B=build-sem-python
    MAKE_VAR=(MICROPYTHON=0)
fi
MAKE_VAR+=("OTIMIZACAO=$OTIMIZACAO")
ENTRADA_VAR=()
if [ "$CON_INPUT_SHARE" = 1 ]; then
    echo "gerar_versao: AVISO: con input_share/ y los datos de OpenJazz y Quake: esta entrega NO se puede publicar (D228)" >&2
else
    # D258: NetSurf (GPL-2.0 solo) no puede ir en lo publicado: solo en --con-input-share.
    # D259: tampoco GNUBoy ni el r8169 derivado de Linux (PUBLICABLE=1).
    ENTRADA_VAR=("INPUT_SHARE=" "OPENJAZZ_DATOS=0" "QUAKE_DATOS=0" "NETSURF=0" "GNUBOY=0" "PUBLICABLE=1")   # D229: ni el shareware de Quake
    MAKE_VAR+=("${ENTRADA_VAR[@]}")
fi

# ---------- compilación ----------
if [ "$COMPILAR" = 1 ]; then
    if [ "$LIMPIAR" = 1 ]; then
        echo "Borrando $B/ ..."
        make "${MAKE_VAR[@]}" B="$B" limpar
    fi
    echo "Compilando ($VARIANTE, $OTIMIZACAO, $JOBS trabajos)..."
    make "${MAKE_VAR[@]}" B="$B" -j"$JOBS" all
    make "${MAKE_VAR[@]}" B="$B" -j"$JOBS" vdi
    [ "$QUERER_ISO" = 1 ] && make "${MAKE_VAR[@]}" B="$B" -j"$JOBS" iso
    if [ "$QUERER_FAT32" = 1 ]; then
        make "${MAKE_VAR[@]}" B="$B" -j"$JOBS" imagem-persistente-fat32 DATOS_FAT32_MB="$FAT32_MB"
    fi
    if [ "$QUERER_EXT2" = 1 ]; then
        make "${MAKE_VAR[@]}" B="$B" -j"$JOBS" imagem-persistente DATOS_MB="$EXT2_MB"
    fi
    [ "$QUERER_SMP" = 1 ] && make "${MAKE_VAR[@]}" B="$B" -j"$JOBS" imagem-smp
    # Con --smp y alguna persistente, también su versión multinúcleo.
    if [ "$QUERER_SMP" = 1 ] && [ "$QUERER_EXT2" = 1 ]; then
        make "${MAKE_VAR[@]}" B="$B" -j"$JOBS" $B/saguios-persistente-smp.img DATOS_MB="$EXT2_MB"
    fi
    if [ "$QUERER_SMP" = 1 ] && [ "$QUERER_FAT32" = 1 ]; then
        make "${MAKE_VAR[@]}" B="$B" -j"$JOBS" $B/saguios-persistente-fat32-smp.img DATOS_FAT32_MB="$FAT32_MB"
    fi
    [ "$QUERER_UACPI" = 1 ] && make "${MAKE_VAR[@]}" B="$B" -j"$JOBS" imagem-uacpi
    if [ "$QUERER_UACPI" = 1 ] && [ "$QUERER_SMP" = 1 ] && [ "$QUERER_EXT2" = 1 ]; then
        make "${MAKE_VAR[@]}" B="$B" -j"$JOBS" $B/saguios-persistente-smp-uacpi.img DATOS_MB="$EXT2_MB"
    fi
    [ "$QUERER_PART" = 1 ] && make "${MAKE_VAR[@]}" B="$B" -j"$JOBS" imagem-particoes-xz
    if [ "$QUERER_SINGPL" = 1 ]; then
        echo "Compilando la variante sin GPL (con auditoría de licencias)..."
        make ${ENTRADA_VAR[@]+"${ENTRADA_VAR[@]}"} entrega-sin-gpl
    fi
fi

IMAGEN="$B/saguios.img"
[ -f "$IMAGEN" ] || error "no se encuentra la imagen $IMAGEN (¿falta --sin-compilar?)"

# ---------- copia a la carpeta de entrega ----------
case "$DESTINO" in
    ""|"/"|"$RAIZ") error "destino no válido: $DESTINO" ;;
esac
mkdir -p "$DESTINO"
rm -f "$DESTINO"/saguios*.img "$DESTINO"/saguios*.vdi "$DESTINO"/saguios*.iso \
      "$DESTINO"/saguios*.img.xz "$DESTINO"/SHA256SUMS.txt "$DESTINO"/VERSION.txt

echo "Copiando la entrega a $DESTINO"
cp "$IMAGEN" "$DESTINO/saguios.img"
[ -f "$B/saguios.vdi" ] && cp "$B/saguios.vdi" "$DESTINO/saguios.vdi"
[ "$QUERER_ISO" = 1 ] && [ -f "$B/saguios.iso" ] && cp "$B/saguios.iso" "$DESTINO/saguios.iso"
if [ "$QUERER_FAT32" = 1 ] && [ -f "$B/saguios-persistente-fat32.img" ]; then
    if command -v xz >/dev/null; then
        cp "$B/saguios-persistente-fat32.img" "$DESTINO/saguios-persistente-fat32.img"
        ( cd "$DESTINO" && xz -9 -f saguios-persistente-fat32.img && rm -f saguios-persistente-fat32.img )
    else
        cp "$B/saguios-persistente-fat32.img" "$DESTINO/saguios-persistente-fat32.img"
    fi
fi
if [ "$QUERER_EXT2" = 1 ] && [ -f "$B/saguios-persistente.img" ]; then
    cp "$B/saguios-persistente.img" "$DESTINO/saguios-persistente.img"
fi
if [ "$QUERER_SMP" = 1 ] && [ -f "$B/saguios-smp.img" ]; then
    cp "$B/saguios-smp.img" "$DESTINO/saguios-smp.img"
fi
if [ -f "$B/saguios-persistente-smp.img" ] && [ "$QUERER_SMP" = 1 ] && [ "$QUERER_EXT2" = 1 ]; then
    cp "$B/saguios-persistente-smp.img" "$DESTINO/saguios-persistente-smp.img"
fi
if [ -f "$B/saguios-persistente-fat32-smp.img" ] && [ "$QUERER_SMP" = 1 ] && [ "$QUERER_FAT32" = 1 ]; then
    cp "$B/saguios-persistente-fat32-smp.img" "$DESTINO/saguios-persistente-fat32-smp.img"
    if command -v xz >/dev/null; then ( cd "$DESTINO" && xz -9 -f saguios-persistente-fat32-smp.img ); fi
fi
if [ "$QUERER_UACPI" = 1 ] && [ -f "$B/saguios-uacpi.img" ]; then
    cp "$B/saguios-uacpi.img" "$DESTINO/saguios-uacpi.img"
fi
if [ -f "$B/saguios-persistente-smp-uacpi.img" ] && [ "$QUERER_UACPI" = 1 ] && [ "$QUERER_SMP" = 1 ] && [ "$QUERER_EXT2" = 1 ]; then
    cp "$B/saguios-persistente-smp-uacpi.img" "$DESTINO/saguios-persistente-smp-uacpi.img"
fi
if [ "$QUERER_PART" = 1 ]; then
    for f in "$B"/saguios-particiones.img.xz "$B"/saguios-particiones.img; do
        if [ -f "$f" ]; then cp "$f" "$DESTINO/"; break; fi
    done
fi
if [ "$QUERER_SINGPL" = 1 ] && [ -f "build-sem-gpl/saguios-sin-gpl.img" ]; then
    cp "build-sem-gpl/saguios-sin-gpl.img" "$DESTINO/saguios-sin-gpl.img"
fi

# ---------- sumas SHA-256 ----------
( cd "$DESTINO" && sha256sum saguios* > SHA256SUMS.txt )

# ---------- hoja de datos ----------
# D194: qué lleva la partición SAGUI-APPS de la imagen y cuánto mide (se lee de
# la propia saguios.img: la tabla, con sus lógicas, y la etiqueta de cada ext2).
texto_apps() {
    if [ "$APPS" = 0 ] || [ "$RAMDISK" = completo ]; then
        echo "sin la partición SAGUI-APPS: las aplicaciones van en el ramdisk (/apps)"
        return
    fi
    python3 - "$IMAGEN" "$B/apps/raiz/bin" <<'FIN'
import os, struct, sys
img, binarios = sys.argv[1], sys.argv[2]
with open(img, "rb") as f:
    def etiqueta(lba):
        f.seek(lba * 512 + 1024)
        sb = f.read(136)
        return sb[120:136].split(b"\0")[0].decode("latin-1") if sb[56:58] == b"\x53\xef" else ""
    parts = []
    f.seek(0)
    mbr = f.read(512)
    for k in range(4):
        e = mbr[446 + 16 * k:462 + 16 * k]
        ini, qtd = struct.unpack("<II", e[8:16])
        if e[4] in (0x05, 0x0F, 0x85):
            rel = 0
            while True:
                f.seek((ini + rel) * 512)
                ebr = f.read(512)
                l, nx = ebr[446:462], ebr[462:478]
                if l[4]:
                    parts.append((ini + rel + struct.unpack("<I", l[8:12])[0], struct.unpack("<I", l[12:16])[0]))
                if nx[4] not in (0x05, 0x0F, 0x85):
                    break
                rel = struct.unpack("<I", nx[8:12])[0]
        elif e[4]:
            parts.append((ini, qtd))
    for ini, qtd in parts:
        if etiqueta(ini) == "SAGUI-APPS":
            apps = sorted(os.listdir(binarios)) if os.path.isdir(binarios) else []
            print(f"SAGUI-APPS de {qtd // 2048} MB en /apps ({', '.join(apps) or 'vacía'})")
            break
    else:
        print("sin la partición SAGUI-APPS")
FIN
}

if [ "$NOTAS" = 1 ]; then
    COMMIT="$(git rev-parse --short HEAD 2>/dev/null || echo desconocido)"
    RAMA="$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo desconocido)"
    SUCIO=""
    if [ -n "$(git status --porcelain --untracked-files=no 2>/dev/null)" ]; then SUCIO=" (con cambios sin commitear)"; fi
    case "$VARIANTE" in
        gpl) TEXTO_VARIANTE="con GPL" ;;
        sem-gpl) TEXTO_VARIANTE="sin GPL (licencia permisiva)" ;;
        sem-firmware) TEXTO_VARIANTE="sin firmware" ;;
    esac
    {
        echo "$NOME $VERSAO"
        echo
        echo "Versión:            $VERSAO"
        echo "Nombre:             $NOME"
        echo "Commit:             $COMMIT$SUCIO"
        echo "Rama:               $RAMA"
        echo "Generado:           $(date '+%Y-%m-%d %H:%M:%S %Z')"
        echo "Variante:           $TEXTO_VARIANTE"
        echo "MicroPython:        $([ "$SIN_PYTHON" = 1 ] && echo "no" || echo "sí")"
        echo "Optimización:       $OTIMIZACAO"
        echo "Partición de datos: $([ "$QUERER_FAT32" = 1 ] && echo "FAT32 de $FAT32_MB MB" || echo "no incluida")"
        echo "Juegos:             $([ "$JOGOS" = 0 ] && echo "sin la partición SAGUI-JOGOS" || echo "SAGUI-JOGOS en /jogos: $([ "$JOGOS_IMG_MB" = 0 ] && echo "la justa" || echo "$JOGOS_IMG_MB MB") en saguios.img, $([ "$JOGOS_MB" = 0 ] && echo "la justa" || echo "$JOGOS_MB MB") en las persistentes")"
        echo "input_share:        $([ "$CON_INPUT_SHARE" = 1 ] && echo "INCLUIDO (no publicar)" || echo "no incluido")"
        echo "Aplicaciones:       $(texto_apps)"
        echo "Configuración:      SAGUI-CONF (ext2, 16 MB, con un LEEME; D256) en todas las imágenes de disco o pendrive; la ISO no la lleva"
        echo "Tamaños (MB):       persistente FAT32 $FAT32_MB, persistente ext2 $EXT2_MB; particiones: /home $HOME_MB, /tmp $TMP_MB, /logs $LOGS_MB ($PART_FS${PART_EXT:+, $PART_EXT})"
        echo
        echo "Contenido:"
        ( cd "$DESTINO" && for f in saguios*; do
              printf '  %-36s %s\n' "$f" "$(du -h "$f" | cut -f1)"
          done )
        echo
        echo "Qué es cada fichero:"
        ( cd "$DESTINO" && for f in saguios*; do
              case "$f" in
                  saguios.img) d="pendrive o disco (Rufus/dd), un procesador; juegos en SAGUI-JOGOS, aplicaciones en SAGUI-APPS y la configuración en SAGUI-CONF" ;;
                  saguios-smp.img) d="la misma, con todos los procesadores (--smp; sin IOAPIC)" ;;
                  saguios.vdi) d="disco para VirtualBox" ;;
                  saguios.iso) d="CD arrancable (sin la partición de juegos; las aplicaciones de /apps van dentro)" ;;
                  saguios-persistente.img) d="pendrive con /home persistente en ext2" ;;
                  saguios-uacpi.img) d="la básica con el intérprete de AML (uACPI): indicador de batería en portátiles (D192)" ;;
                  saguios-persistente-smp-uacpi.img) d="pendrive con /home persistente en ext2, todos los procesadores y uACPI (batería): la de los portátiles" ;;
                  saguios-persistente-smp.img) d="pendrive con /home persistente en ext2, con todos los procesadores" ;;
                  saguios-persistente-fat32-smp.img*) d="pendrive con /home persistente en FAT32, con todos los procesadores" ;;
                  saguios-persistente-fat32.img*) d="pendrive con /home persistente en FAT32 (también se lee desde Windows)" ;;
                  saguios-particiones.img*) d="disco con /home, /tmp y /logs en particiones (D120)" ;;
                  saguios-sin-gpl.img) d="variante sin código GPL (sin ports GPL de terceros: OpenTyrian, NetSurf, OpenJazz..., D109/D258)" ;;
                  *) d="" ;;
              esac
              printf '  %-36s %s\n' "$f" "$d"
          done )
        echo
        echo "Las sumas SHA-256 de cada fichero están en SHA256SUMS.txt."
        echo "Comprobarlas con:  sha256sum -c SHA256SUMS.txt"
    } > "$DESTINO/VERSION.txt"
    if [ -n "$FICHERO_NOTAS" ]; then
        [ -f "$FICHERO_NOTAS" ] || error "no existe el fichero de notas: $FICHERO_NOTAS"
        { echo; echo "Notas:"; cat "$FICHERO_NOTAS"; } >> "$DESTINO/VERSION.txt"
    fi
fi

# ---------- opcional: arrancar la imagen y comprobar que funciona ----------
if [ "$PROBAR" = 1 ]; then
    if ! command -v qemu-system-i386 >/dev/null; then
        echo "No se encuentra qemu-system-i386: no se puede probar la imagen." >&2
        exit 1
    fi
    LOG="$RAIZ/build/gerar_versao-serie.log"
    mkdir -p "$(dirname "$LOG")"
    rm -f "$LOG"
    echo "Arrancando la imagen en el QEMU para comprobarla..."
    qemu-system-i386 -m 512M -drive "format=raw,file=$IMAGEN,snapshot=on" \
        -display none -serial "file:$LOG" &
    PID_QEMU=$!
    LISTO=0
    for _ in $(seq 1 60); do
        if grep -q "Hola! Soy" "$LOG" 2>/dev/null; then LISTO=1; break; fi
        if ! kill -0 "$PID_QEMU" 2>/dev/null; then break; fi
        sleep 1
    done
    kill "$PID_QEMU" 2>/dev/null || true
    wait "$PID_QEMU" 2>/dev/null || true
    if [ "$LISTO" = 1 ]; then
        echo "La imagen arranca bien (el kernel llegó al escritorio)."
    else
        echo "AVISO: la imagen no llegó al escritorio en 60 s. Últimas líneas del log:" >&2
        tail -20 "$LOG" >&2 || true
        exit 1
    fi
fi

# ---------- opcional: publicar en entrega/v<versión> ----------
if [ "$PUBLICAR" = 1 ]; then
    PUBLICA="$RAIZ/entrega/$VERSAO"
    if [ "$PUBLICA" = "$DESTINO" ]; then
        echo "La entrega ya está en $PUBLICA."
    else
        echo "Publicando también en $PUBLICA"
        mkdir -p "$PUBLICA"
        cp -f "$DESTINO"/saguios* "$DESTINO"/SHA256SUMS.txt "$DESTINO"/VERSION.txt "$PUBLICA"/
    fi
fi

echo
echo "Entrega lista: $DESTINO"
ls -la "$DESTINO"
