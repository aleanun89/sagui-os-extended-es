#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# imagem_tyrian.sh - arma a imagem de dados do OpenTyrian (D156).
#
# Os dados do jogo (a versao shareware do Tyrian, que e de distribuicao livre)
# vao num disco aparte, e o jogo procura-os em /jogos/opentyrian (ou em
# /usb/jogos/opentyrian, se o disco for um pendrive). Desde a D181 a imagem do
# sistema leva-os tambem, numa particao ext2 SAGUI-JOGOS que o make extrai
# desta mesma imagem (userland/terceros/opentyrian/dados.img.xz).
#
#   tools/imagem_tyrian.sh [--dados PASTA] [--saida FICHEIRO] [--xz] [--listar]
#
#   --dados PASTA    onde estao os dados do shareware (por omissao
#                    $HOME/data_tyrian). O shareware distribui-se livre; os
#                    ficheiros que o jogo usa sao copiados daqui.
#   --saida FICHEIRO  a imagem que se arma (por omissao build/tyrian-dados.img)
#   --xz             compress a imagem (deixa tambem o .img)
#   --listar         mostra os ficheiros que o jogo usa e sai
#
# Para usar: no SaguiOS, "mount hdb1 /jogos" (el disco con los datos) y luego
# "/bin/opentyrian". El juego busca solo los ficheros que necesita.
set -euo pipefail

RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
[ -f "$HOME/.local/saguitools/env.sh" ] && . "$HOME/.local/saguitools/env.sh"

DADOS="$HOME/data_tyrian"
SAIDA="$RAIZ/build/tyrian-dados.img"
TAMANHO_MB=64
FAZER_XZ=0
LISTAR=0

while [ $# -gt 0 ]; do
    case "$1" in
        --dados) DADOS="$2"; shift 2 ;;
        --saida) SAIDA="$2"; shift 2 ;;
        --xz) FAZER_XZ=1; shift ;;
        --listar) LISTAR=1; shift ;;
        -h|--help)
            sed -n '2,25p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
            exit 0 ;;
        *) echo "imagem_tyrian.sh: opção desconhecida: $1" >&2; exit 2 ;;
    esac
done

# Os ficheiros que o jogo abre (a lista sale do proprio codigo do jogo:
# userland/terceros/opentyrian/jogo/). Faltam os newsh<letra>.shp, que sao
# as noticias, e os ficheiros dos menus de rede.
FICHEIROS=(
    tyrian.hdt tyrian.shp tyrian.pic tyrian.cdt tyrian.snd tyrianc.shp
    tyrian1.lvl tyrian2.lvl tyrian3.lvl tyrian4.lvl
    music.mus voices.snd voicesc.snd palette.dat loudness.awe
    shapesw.dat shapesx.dat shapesy.dat shapesz.dat "shapes).dat"
    levels1.dat levels2.dat levels3.dat levels4.dat
    cubetxt1.dat cubetxt2.dat cubetxt3.dat cubetxt4.dat
    user1.shp user2.shp tyrian.ico exitmsg.bin estsc.shp
    tshp2.pcx tyrset.pcx netfont1.pcx netfont2.pcx netset.pcx
    netarena.pcx netmega.pcx "newsh#.shp"
    demo.1 demo.2 demo.3 demo.4 demo.5
)
for c in 0 1 2 3 4 5 6 7 8 9 '^' a b c d e f g h i j k l m n o p q r s t u v w x y z '~'; do
    FICHEIROS+=("newsh$c.shp")
done

if [ "$LISTAR" = 1 ]; then
    printf '%s\n' "${FICHEIROS[@]}"
    exit 0
fi

if [ ! -d "$DADOS" ]; then
    echo "imagem_tyrian.sh: não há dados do jogo em '$DADOS'." >&2
    echo "  Usa --dados PASTA com a pasta do shareware do Tyrian." >&2
    exit 1
fi

faltam=()
presentes=()
for f in "${FICHEIROS[@]}"; do
    if [ -f "$DADOS/$f" ]; then presentes+=("$f"); else faltam+=("$f"); fi
done
if [ ${#presentes[@]} -eq 0 ]; then
    echo "imagem_tyrian.sh: em '$DADOS' não há nenhum ficheiro do jogo." >&2
    exit 1
fi
if [ ${#faltam[@]} -gt 0 ]; then
    echo "imagem_tyrian.sh: aviso: faltam ${#faltam[@]} ficheiro(s) do jogo:"
    printf '  %s\n' "${faltam[@]}"
fi

mkdir -p "$(dirname "$SAIDA")"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

# A imagem: uma FAT32 com os dados em /jogos/opentyrian.
truncate -s "${TAMANHO_MB}M" "$SAIDA"
mkfs.vfat -F 32 -n TYRIAN "$SAIDA" >/dev/null
cat > "$TMP/mtoolsrc" <<EOF
drive p: file="$SAIDA"
mtools_skip_check=1
EOF
export MTOOLSRC="$TMP/mtoolsrc"
mkdir -p "$TMP/jogos/opentyrian"
for f in "${presentes[@]}"; do
    cp "$DADOS/$f" "$TMP/jogos/opentyrian/$f"
done
mcopy -s -Q "$TMP/jogos" p:/

bytes=0
for f in "${presentes[@]}"; do bytes=$((bytes + $(stat -c%s "$DADOS/$f"))); done
echo "imagem_tyrian.sh: ${#presentes[@]} ficheiro(s), $((bytes / 1048576)) MB de dados -> $SAIDA"

if [ "$FAZER_XZ" = 1 ]; then
    xz -9 -T0 -f "$SAIDA"
    echo "imagem_tyrian.sh: comprimido -> $SAIDA.xz"
fi
