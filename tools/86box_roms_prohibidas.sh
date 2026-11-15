#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-or-later
# 86box_roms_prohibidas.sh: las ROM/BIOS de 86Box NO SE PUEDEN SUBIR NUNCA (D301, mantenedor).
# Son BIOS con copyright. Viven solo en ~/.local/saguitools/86box/roms (y en la copia de Windows,
# %LOCALAPPDATA%\saguitools\86box\roms), fuera del repositorio.
#
#   tools/86box_roms_prohibidas.sh --indice      mira lo que está en el índice de git (gancho pre-commit)
#   tools/86box_roms_prohibidas.sh --arbol DIR   mira un árbol (lo usa tools/publicar.sh)
# Sale con 1 si encuentra algo. Criterios, por nombre: carpetas roms/ con las de 86Box
# (machines, video, sound, scsi, network...), cualquier *.rom, y *.bin/*.bios/*.nvr/*.u* bajo una ruta con
# «86box». Por tamaño: un .bin de 8 KB a 2 MB o un fichero sin extensión de 64 KB a 2 MB
# (tamaños típicos de BIOS) bajo tools/86box/ o build/86box/.
# Instalar como gancho:  tools/86box_roms_prohibidas.sh --instalar-gancho
set -u
RAIZ="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PAT_NOMBRE='(^|/)roms/(machines|video|sound|scsi|network|hdd|floppy|printer|rtc|ports|keyboard|mouse|cdrom|other)(/|$)|\.rom$|[^/]*86box[^/]*/.*\.(bin|bios|nvr|rom)$|(^|/)86box[^/]*/roms(/|$)'

malos=""
case "${1:-}" in
    --indice)
        lista="$(git -C "$RAIZ" diff --cached --name-only --diff-filter=AM)"
        malos="$(echo "$lista" | grep -E "$PAT_NOMBRE" || true)"
        for f in $(echo "$lista" | grep -E '^(tools/86box|build/86box)/' || true); do
            [ -f "$RAIZ/$f" ] || continue
            t=$(stat -c %s "$RAIZ/$f")
            case "$f" in *.cfg|*.md|*.sh|*.py|*.txt|*.png|*.jpg|*.gif|*.ppm) continue;; esac
            [ "$t" -ge 8192 ] && [ "$t" -le 2097152 ] && malos="$malos"$'\n'"$f (tamaño $t, típico de una BIOS)"
        done ;;
    --arbol)
        d="${2:?uso: --arbol DIR}"
        malos="$(cd "$d" && find . -type f | sed 's#^\./##' | grep -E "$PAT_NOMBRE" || true)"
        for f in $(cd "$d" && find . -type f \( -path '*86box*' \) -size +8191c -size -2097153c ! -name '*.cfg' ! -name '*.md' ! -name '*.sh' ! -name '*.py' ! -name '*.txt' ! -name '*.png' ! -name '*.jpg' ! -name '*.gif' ! -name '*.ppm' | sed 's#^\./##'); do
            malos="$malos"$'\n'"$f (tamaño típico de una BIOS)"
        done ;;
    --instalar-gancho)
        h="$(git -C "$RAIZ" rev-parse --git-common-dir)/hooks/pre-commit"
        if [ -e "$h" ] && ! grep -q 86box_roms_prohibidas "$h"; then
            echo "Ya hay un pre-commit en $h: añade a mano:  $RAIZ/tools/86box_roms_prohibidas.sh --indice || exit 1" >&2; exit 1
        fi
        # Sin ruta fija: el gancho es común a todas las copias de trabajo (git worktree) y usa el
        # guion de la copia donde se hace el commit; si esa copia no lo tiene, no bloquea.
        cat > "$h" <<'GANCHO'
#!/bin/bash
# Gancho de SaguiOS (D301): las ROM de 86Box no se suben nunca.
# Usa el guion de la copia de trabajo en la que se hace el commit (main o worktree).
g="$(git rev-parse --show-toplevel)/tools/86box_roms_prohibidas.sh"
[ -x "$g" ] || exit 0
exec "$g" --indice
GANCHO
        chmod +x "$h"; echo "gancho instalado en $h"; exit 0 ;;
    *) sed -n 2,14p "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
malos="$(echo "$malos" | sed '/^$/d')"
if [ -n "$malos" ]; then
    echo "ROM DE 86BOX EN LO QUE SE VA A SUBIR (NO SE PUEDEN SUBIR NUNCA):" >&2
    echo "$malos" | head -20 | sed 's/^/  /' >&2
    exit 1
fi
exit 0
