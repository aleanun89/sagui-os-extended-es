#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-2.0-or-later
# publicar.sh - prepara y sube una publicación al repositorio PÚBLICO de SaguiOS (D260).
#
# El repositorio de trabajo (privado) conserva todo su historial. El público tiene historial
# limpio: cada publicación es UN commit nuevo «Publicación VERSION» encima de las anteriores,
# con la etiqueta VERSION, solo con lo que cumple la GPL v3 y sin binarios (las imágenes van a
# la GitHub Release). Procedimiento completo: docs/publicar.md.
#
# NOMBRES (D261): el repo público tiene su propia nomenclatura, distinta de la del privado: la
# primera publicación es «beta1» (betaN) y las siguientes «AAAAMM-VERSION» con el año-mes y la
# versión del repo de trabajo (p. ej. 202611-8.0). Es la etiqueta y el título de la Release; el
# commit y las notas dicen de qué versión y commit del repo de trabajo sale («beta1 = v8.0b7,
# commit X»). Repositorio por defecto: PUBLICAR_URL o https://github.com/aleanun89/sagui-os-extended-es
#
#   tools/publicar.sh preparar VERSION DESTINO [--commit REF] [--origen VERSION_TRABAJO] [--compilar]
#       Exporta los ficheros VERSIONADOS de REF (por defecto HEAD, que debe estar en main) a
#       DESTINO con «git archive», aplica tools/publicar-excluir.txt y pasa todas las
#       comprobaciones. Si una falla no deja DESTINO. --compilar compila el árbol preparado
#       con «make variantes» (hace falta PUBLICAR_TURNO=/ruta/turno.sh en este proyecto).
#   tools/publicar.sh subir VERSION DESTINO URL_REPO_PUBLICO [--imagenes DIR]
#       Con DESTINO ya preparado: lo vuelca en un clon del repo público, hace el commit
#       «Publicación VERSION» (autor: el de git config del repo de trabajo), la etiqueta, y pide
#       escribir PUBLICAR antes del push. Después prepara (o ejecuta) «gh release create».
#
# Variables (para pruebas): PUBLICAR_RAIZ (repo de trabajo), PUBLICAR_RAMA (main),
# PUBLICAR_TRABAJO (donde se clona el público, por defecto ~/.cache/saguios-publicar).
# Nunca hace push --force, nunca reescribe historia, nunca crea repositorios.
set -euo pipefail

AQUI="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RAIZ="$(cd "${PUBLICAR_RAIZ:-$AQUI/..}" && pwd)"
RAMA="${PUBLICAR_RAMA:-main}"
EXCLUIR="${PUBLICAR_EXCLUIR:-$AQUI/publicar-excluir.txt}"
TAM_MAX=$((5 * 1024 * 1024))

die() { echo "publicar: $*" >&2; exit 1; }
info() { echo "publicar: $*"; }

uso() { sed -n '2,26p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit "${1:-1}"; }

URL_PUBLICO="${PUBLICAR_URL:-https://github.com/aleanun89/sagui-os-extended-es}"

# Nombre de la publicación en el repo público: betaN o AAAAMM-VERSION (D261).
validar_version() {
    [[ "$1" =~ ^beta[0-9]+$ ]] || [[ "$1" =~ ^[0-9]{6}-[0-9]+(\.[0-9]+)*(b[0-9]*)?$ ]] \
        || die "nombre de publicación no válido: «$1» (formatos: beta1, 202611-8.0, 202612-8.0.1, 202611-8.0b7)"
}
# Versión del repo de trabajo (vX.Y...): se anota en el commit y en las notas de la Release.
validar_origen() {
    [[ "$1" =~ ^v[0-9]+\.[0-9]+(\.[0-9]+)?([a-z]+[0-9]*)?$ ]] || die "versión de trabajo no válida: «$1» (ejemplos: v8.0, v8.0b7, v7.4.3)"
}

# ---------------------------------------------------------------- comprobaciones
FALLOS=()
AVISOS=()
fallo() { FALLOS+=("$1"); echo "  FALLO: $1"; }
aviso() { AVISOS+=("$1"); echo "  aviso: $1"; }
ok() { echo "  ok: $1"; }

# Ficheros de texto del árbol (sin binarios), con ruta relativa a $1, separados por NUL.
# Se omiten los ficheros que citan los patrones buscados a propósito (esta herramienta, su
# prueba, su documentación y el guion de los agentes).
PROPIOS_PATRONES=(tools/publicar.sh tools/publicar-excluir.txt tests/h260_publicar.py docs/publicar.md AGENTS.md tools/gh-agentes.sh tools/entorno-local.sh tools/auditar_licencias.py tools/gnuboy_fuentes.sh tools/netsurf_fuentes.sh)
buscar_texto() { # $1 = árbol; resto = argumentos de grep -E
    local arbol="$1"; shift
    local excl=() f
    for f in "${PROPIOS_PATRONES[@]}"; do excl+=(--exclude="$(basename "$f")"); done
    (cd "$arbol" && grep -rInE "${excl[@]}" --exclude-dir=.git "$@" . 2>/dev/null) | sed 's#^\./##' || true
}

comp_secretos() {
    local a="$1" r
    r="$(buscar_texto "$a" -e 'ghp_[A-Za-z0-9]{20,}' -e 'github_pat_[A-Za-z0-9_]{20,}' -e '(^|[^A-Za-z0-9_])GT=[^ ]' \
        -e '-----BEGIN [A-Z ]*PRIVATE KEY' -e 'PRIVATE KEY-----' -e 'AKIA[0-9A-Z]{16}' | cut -c1-110)"
    # Las claves PRIVATE KEY de pruebas o de ejemplo de los ports son frecuentes: se listan igual.
    if [ -n "$r" ]; then fallo "posibles secretos/tokens ($(echo "$r" | wc -l) líneas):"; echo "$r" | head -8 | sed 's/^/      /'
    else ok "sin tokens ni claves privadas"; fi
}

comp_correos() {
    local a="$1" mio="" permitidos r
    mio="$(git -C "$RAIZ" config user.email 2>/dev/null || true)"
    permitidos="$(mktemp)"
    echo "noreply@anthropic.com" > "$permitidos"
    [ -f "$a/AUTORES.md" ] && grep -oE '[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}' "$a/AUTORES.md" >> "$permitidos" || true
    # Direcciones que no son personales: ejemplos de pruebas, nombres de algoritmos SSH
    # (chacha20-poly1305@openssh.com), SSH de GitHub, localhost.
    printf '%s\n' 'git@github.com' >> "$permitidos"
    r="$(cd "$a" && grep -rIoE --exclude-dir=.git --exclude-dir=terceros --exclude-dir=micropython --exclude='LICENSE*' --exclude='LICENCA*' --exclude='COPYING*' \
        --exclude=publicar.sh --exclude=h260_publicar.py --exclude=publicar.md \
        '[A-Za-z0-9._%+-]+@[A-Za-z0-9-]+(\.[A-Za-z0-9-]+)*\.[A-Za-z]{2,}' . 2>/dev/null \
        | sed 's#^\./##' | grep -viE '@(example\.(com|org|net)|exemplo\.org|sagui\.com|openssh\.com|libssh\.org|localhost|ejemplo\.)' || true)"
    local malos="" l dir
    while IFS= read -r l; do
        [ -n "$l" ] || continue
        dir="${l#*:}"
        grep -qixF "$dir" "$permitidos" && continue
        malos+="$l"$'\n'
    done <<< "$r"
    rm -f "$permitidos"
    if [ -n "$mio" ] && echo "$r" | grep -qiF ":$mio"; then
        fallo "aparece el correo del mantenedor (git config user.email) en: $(echo "$r" | grep -iF ":$mio" | head -3 | tr '\n' ' ')"
    fi
    malos="$(printf '%s' "$malos" | sort -u)"
    if [ -n "$malos" ]; then fallo "direcciones de correo no permitidas ($(echo "$malos" | wc -l)):"; echo "$malos" | head -8 | sed 's/^/      /'
    else ok "sin correos personales"; fi
}

comp_mac_ip_serie() {
    local a="$1" r
    # MAC reales (75:65:74:65:73:20 es texto ASCII citado en decisoes.md): se aceptan las de ejemplo (QEMU 52:54:00, ceros, difusión, dead:beef, 00:11:22...)
    r="$(buscar_texto "$a" '(^|[^0-9A-Fa-f:])([0-9A-Fa-f]{2}:){5}[0-9A-Fa-f]{2}([^0-9A-Fa-f:]|$)' \
        | { grep -vE '^tests/' || true; } \
        | { grep -viE '(52:54:00|00:00:00:00:00:00|ff:ff:ff:ff:ff:ff|de:ad:be:ef|00:11:22|12:34:56|aa:bb:cc|01:00:5e|33:33:|02:00:00|01:80:c2|75:65:74:65:73:20)' || true; } \
        | { grep -vE '^[^:]*terceros/' || true; } | cut -c1-110)"
    if [ -n "$r" ]; then fallo "posibles direcciones MAC reales ($(echo "$r" | wc -l)):"; echo "$r" | head -6 | sed 's/^/      /'
    else ok "sin MAC reales"; fi
    # IP públicas en documentación y fuentes propias (se aceptan privadas, loopback, DNS conocidos y de ejemplo)
    r="$(buscar_texto "$a" '(^|[^0-9.])([0-9]{1,3}\.){3}[0-9]{1,3}([^0-9.]|$)' \
        | { grep -vE '^[^:]*terceros/|^tests/|^firmware/|^userland/web/|^userland/(programas/(cc|pascal)|libsagui/(mpeg|jpeg|png))' || true; } \
        | python3 -c '
import re, sys
ok_dns = {"1.1.1.1", "1.0.0.1", "8.8.8.8", "8.8.4.4", "9.9.9.9", "208.67.222.222", "208.67.220.220"}
for l in sys.stdin:
    cuerpo = l.split(":", 2)[2] if l.count(":") >= 2 else l
    for m in re.finditer(r"(?<![\d.])(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})(?![\d.])", cuerpo):
        o = [int(x) for x in m.groups()]
        ip = m.group(0)
        if any(x > 255 for x in o) or ip in ok_dns: continue
        a, b = o[0], o[1]
        if a in (0, 10, 127, 255) or a >= 224 or (a == 172 and 16 <= b <= 31) or (a == 192 and b == 168) or (a == 169 and b == 254):
            continue
        if a == 100 and 64 <= b <= 127: continue
        if (a, b, o[2]) in ((192, 0, 2), (198, 51, 100), (203, 0, 113)): continue
        if re.search(r"(version|versi|v\d|ver\.|OID|oid|\.\d{1,3}\.\d{1,3}\.\d)", cuerpo, re.I) and a < 20: continue
        print(l.rstrip()[:110]); break
' || true)"
    if [ -n "$r" ]; then fallo "posibles IP públicas ($(echo "$r" | wc -l)):"; echo "$r" | head -6 | sed 's/^/      /'
    else ok "sin IP públicas"; fi
    # Números de serie en informes y documentos (no en código: allí «serial» es el puerto COM)
    r="$(cd "$a" && grep -rInE --include='*.md' --include='*.txt' --exclude-dir=.git --exclude-dir=terceros \
        --exclude=publicar.md -i '(serial( number)?|s/n)[ ]*[:=#][ ]*[A-Za-z0-9-]{6,}' . 2>/dev/null | sed 's#^\./##' \
        | { grep -viE 'serial: *(ttyS|com[0-9]|\$|<|x{3})' || true; } | cut -c1-110 || true)"
    if [ -n "$r" ]; then fallo "posibles números de serie ($(echo "$r" | wc -l)):"; echo "$r" | head -6 | sed 's/^/      /'
    else ok "sin números de serie"; fi
}

comp_tamano() {
    local a="$1" r
    # Excepción: la amalgama de SQLite (dominio público, docs/terceros.md) pesa 9,5 MB.
    r="$(cd "$a" && find . -type f -size +"${TAM_MAX}"c -printf '%s %p\n' | grep -v ' ./userland/terceros/sqlite/sqlite3.c$' | sort -rn || true)"
    if [ -n "$r" ]; then fallo "ficheros de más de 5 MB:"; echo "$r" | head -8 | sed 's/^/      /'
    else ok "ningún fichero de más de 5 MB"; fi
    info "  tamaño total: $(du -sh "$a" | cut -f1), $(cd "$a" && find . -type f | wc -l) ficheros"
}

# D261: ni código de NetSurf (y sus bibliotecas) ni de GNUBoy en lo público: solo el pegamento propio.
comp_terceros_v2() {
    local a="$1" r
    r="$(buscar_texto "$a" -e 'This file is part of (NetSurf|libdom|LibCSS|Hubbub|LibParserUtils|libnsfb|nsgenbind|RUfl|Libsvgtiny|libnsutils)' \
        -e 'Copyright.{0,40}(NetSurf|John-Mark Bell|Rob Kendrick|Vincent Sanders)' -e '\<(Laguna|Gilgamesh)\>' \
        | { grep -vE '\.md:' || true; } | cut -c1-110)"
    if [ -n "$r" ]; then fallo "código de NetSurf o GNUBoy en el árbol público ($(echo "$r" | wc -l) líneas):"; echo "$r" | head -6 | sed 's/^/      /'
    else ok "sin código de NetSurf ni de GNUBoy (solo el pegamento propio)"; fi
}

comp_licencia() {
    local a="$1" r
    # D259: de kernel/terceros/linux/ solo puede ir lo GPL-2.0-or-later (IDT, k10temp); el r8169
    # (GPL-2.0-only) y cualquier fichero de esa carpeta sin «or later» hacen fallar la comprobación.
    if [ -d "$a/kernel/terceros/linux/r8169" ]; then fallo "existe kernel/terceros/linux/r8169/ (GPL-2.0-only)"
    elif [ -d "$a/userland/terceros/gnuboy/jogo" ]; then fallo "existe userland/terceros/gnuboy/jogo/ (código de GNUBoy, GPL-2.0 solo v2: se descarga, no se publica)"
    elif [ -d "$a/userland/terceros/netsurf/fuentes" ]; then fallo "existen userland/terceros/netsurf/fuentes/ (código de NetSurf, GPL-2.0 solo v2: se descarga, no se publica)"
    else
        local solo2="" g
        if [ -d "$a/kernel/terceros/linux" ]; then
            while IFS= read -r g; do
                grep -qiE 'GPL-2\.0-or-later|any later version|or later' "$a/$g" || solo2+="$g"$'\n'
            done < <(cd "$a" && find kernel/terceros/linux -type f)
        fi
        if [ -n "$solo2" ]; then fallo "ficheros de kernel/terceros/linux/ sin «or later»:"; echo "$solo2" | head -6 | sed "s/^/      /"
        else ok "kernel/terceros/linux/: solo GPL-2.0-or-later (sin r8169, NetSurf ni GNUBoy)"; fi
    fi
    # GPL-2.0 solo (sin «o posterior») en código propio (los ports de terceros/ tienen su licencia)
    local malos="" f
    while IFS= read -r f; do
        grep -qiE 'any later version|or later' "$a/$f" || malos+="$f"$'\n'
    done < <(cd "$a" && grep -rIlE --exclude-dir=.git --exclude-dir=terceros --exclude='LICENSE*' --exclude='COPYING*' \
        --exclude=publicar.sh --exclude=h260_publicar.py --exclude=publicar.md --exclude='*.md' --exclude=auditar_licencias.py \
        --exclude=spdx.py --exclude=h81_licencias.py -e 'version 2 of the License' . 2>/dev/null | sed 's#^\./##')
    if [ -n "$malos" ]; then fallo "texto GPL-2.0-only (sin «or later») en código propio:"; echo "$malos" | head -6 | sed "s/^/      /"
    else ok "sin texto GPL-2.0-only en código propio"; fi
    if [ -f "$a/LICENSE" ] && grep -q 'GNU GENERAL PUBLIC LICENSE' "$a/LICENSE" && grep -qE 'Version 3, 29 June 2007' "$a/LICENSE"; then
        ok "LICENSE es la GPL v3"
    else fallo "LICENSE falta o no es el texto de la GPL v3 (¿falta fusionar v8.0b7-licencia-gplv3?)"; fi
    if [ -f "$a/AUTORES.md" ] && grep -q 'https://github.com/erigobeli/sagui-os' "$a/AUTORES.md"; then ok "AUTORES.md enlaza al proyecto original"
    else fallo "AUTORES.md falta o no enlaza a https://github.com/erigobeli/sagui-os"; fi
    # Auditoría y SPDX: se usan las del propio árbol preparado (cada una toma su raíz de su ubicación)
    if [ -f "$a/tools/auditar_licencias.py" ]; then
        local opc="--sin-make --publico"
        # --estricto solo existe cuando se fusionó la rama de licencia
        grep -q -- '--estricto' "$a/tools/auditar_licencias.py" && opc="$opc --estricto" \
            || aviso "auditar_licencias.py aún no tiene --estricto (falta fusionar v8.0b7-licencia-gplv3)"
        if r="$(python3 "$a/tools/auditar_licencias.py" $opc 2>&1)"; then ok "auditar_licencias.py $opc"
        else fallo "auditar_licencias.py $opc falla:"; echo "$r" | tail -8 | sed 's/^/      /'; fi
    else aviso "no existe tools/auditar_licencias.py en el árbol"; fi
    if [ -f "$a/tools/spdx.py" ]; then
        # spdx.py necesita el historial git (para saber qué es de Eli): se ejecuta en el repo de
        # trabajo, del que sale el árbol exportado, y debe decir que no queda nada por marcar.
        if r="$(python3 "$RAIZ/tools/spdx.py" --comprobar 2>&1)" && [ "$(echo "$r" | grep -c 'irían a .*: 0$')" -eq 2 ]; then ok "spdx.py --comprobar"
        else fallo "spdx.py --comprobar falla:"; echo "$r" | tail -6 | sed 's/^/      /'; fi
    else aviso "no existe tools/spdx.py (falta fusionar la rama de licencia); se sigue"; fi
}

comp_compilar() {
    local a="$1" tmp
    [ -n "${PUBLICAR_TURNO:-}" ] || { fallo "--compilar necesita PUBLICAR_TURNO=/ruta/turno.sh (nunca make fuera del turno)"; return; }
    tmp="$(mktemp -d)"
    cp -a "$a/." "$tmp/"
    if "$PUBLICAR_TURNO" bash -c ". ~/.local/saguitools/env.sh && make -C '$tmp' -j2 variantes DESTINO='$tmp/salida'" >"$tmp/make.log" 2>&1 \
        && [ -f "$tmp/salida/saguios.img" ]; then ok "make variantes compila el árbol preparado"
    else fallo "make variantes falla en el árbol preparado (registro: $tmp/make.log)"; tail -5 "$tmp/make.log" | sed 's/^/      /'; return; fi
    rm -rf "$tmp"
}

# ---------------------------------------------------------------- preparar
aplicar_exclusiones() {
    local a="$1" pat n total=0 sin=0
    while IFS= read -r pat || [ -n "$pat" ]; do
        pat="${pat%%[[:space:]]#*}"; pat="${pat%"${pat##*[![:space:]]}"}"
        [ -n "$pat" ] || continue
        case "$pat" in \#*) continue;; esac
        pat="${pat%/}"
        if [[ "$pat" == /* ]]; then
            n="$(cd "$a" && find . -path "./${pat#/}" -prune -print -exec rm -rf {} + | wc -l)"
        else
            n="$(cd "$a" && find . -name "$pat" -prune -print -exec rm -rf {} + | wc -l)"
        fi
        total=$((total + n)); [ "$n" -eq 0 ] && sin=$((sin + 1))
    done < "$EXCLUIR"
    info "exclusiones: $total elementos retirados ($sin patrones sin coincidencias, preventivos)"
}

cmd_preparar() {
    local version="" destino="" ref="HEAD" compilar=0 origen=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --commit) ref="${2:?falta REF}"; shift 2;;
            --compilar) compilar=1; shift;;
            --origen) origen="${2:?falta VERSION_TRABAJO}"; shift 2;;
            -*) die "opción desconocida: $1";;
            *) if [ -z "$version" ]; then version="$1"; elif [ -z "$destino" ]; then destino="$1"; else die "sobran argumentos"; fi; shift;;
        esac
    done
    [ -n "$version" ] && [ -n "$destino" ] || uso
    validar_version "$version"
    [ -z "$origen" ] || validar_origen "$origen"
    [ -f "$EXCLUIR" ] || die "falta la lista de exclusión $EXCLUIR"
    git -C "$RAIZ" rev-parse --verify -q "$ref^{commit}" >/dev/null || die "no existe el commit $ref en $RAIZ"
    local sha; sha="$(git -C "$RAIZ" rev-parse "$ref^{commit}")"
    git -C "$RAIZ" rev-parse --verify -q "$RAMA" >/dev/null || die "no existe la rama $RAMA en $RAIZ"
    git -C "$RAIZ" merge-base --is-ancestor "$sha" "$RAMA" || die "el commit ${sha:0:8} no está en $RAMA: solo se publica lo que ya está integrado"
    destino="$(realpath -m "$destino")"
    case "$destino" in "$RAIZ"|"$RAIZ"/*|/|"$HOME") die "DESTINO no puede estar dentro del repositorio de trabajo ni ser / o ~";; esac
    if [ -e "$destino" ] && [ -n "$(ls -A "$destino" 2>/dev/null)" ]; then die "DESTINO ya existe y no está vacío: bórralo tú ($destino)"; fi
    rm -f "$destino.ok"
    local parcial="$destino.parcial"
    rm -rf "$parcial"; mkdir -p "$parcial"
    trap "rm -rf '$parcial'" EXIT
    info "exportando ${sha:0:8} ($version) con git archive (solo ficheros versionados)"
    git -C "$RAIZ" archive --format=tar "$sha" | tar -x -C "$parcial"
    aplicar_exclusiones "$parcial"
    echo "comprobaciones:"
    comp_secretos "$parcial"
    comp_correos "$parcial"
    comp_mac_ip_serie "$parcial"
    comp_tamano "$parcial"
    comp_terceros_v2 "$parcial"
    comp_licencia "$parcial"
    [ "$compilar" -eq 1 ] && comp_compilar "$parcial"
    if [ "${#FALLOS[@]}" -gt 0 ]; then
        echo
        echo "publicar: ${#FALLOS[@]} comprobación(es) fallida(s); no se deja nada en $destino:" >&2
        printf '  - %s\n' "${FALLOS[@]}" >&2
        exit 1
    fi
    mkdir -p "$(dirname "$destino")"
    rmdir "$destino" 2>/dev/null || true
    mv "$parcial" "$destino"; trap - EXIT
    # Marca de lo comprobado: «subir» verifica que el árbol no cambió después.
    [ -n "$origen" ] || origen="$(git -C "$RAIZ" describe --tags --always "$sha" 2>/dev/null || echo "${sha:0:8}")"
    printf '%s\n%s\n%s\n%s\n' "$version" "$sha" "$(huella "$destino")" "$origen" > "$destino.ok"
    info "preparado en $destino (${#AVISOS[@]} avisos). Siguiente: tools/publicar.sh subir $version $destino URL"
}

huella() { (cd "$1" && find . -type f -print0 | sort -z | xargs -0 sha256sum | sha256sum | cut -d' ' -f1); }

# ---------------------------------------------------------------- subir
cmd_subir() {
    local version="" destino="" url="" imagenes=""
    while [ $# -gt 0 ]; do
        case "$1" in
            --imagenes) imagenes="${2:?falta DIR}"; shift 2;;
            -*) die "opción desconocida: $1";;
            *) if [ -z "$version" ]; then version="$1"; elif [ -z "$destino" ]; then destino="$1"; elif [ -z "$url" ]; then url="$1"; else die "sobran argumentos"; fi; shift;;
        esac
    done
    [ -n "$url" ] || url="$URL_PUBLICO"
    [ -n "$version" ] && [ -n "$destino" ] || uso
    validar_version "$version"
    destino="$(realpath "$destino")"
    [ -f "$destino.ok" ] || die "$destino no está preparado: ejecuta antes «preparar»"
    [ "$(sed -n 1p "$destino.ok")" = "$version" ] || die "el directorio se preparó para otra versión ($(sed -n 1p "$destino.ok"))"
    [ "$(sed -n 3p "$destino.ok")" = "$(huella "$destino")" ] || die "el contenido de $destino cambió después de preparar: vuelve a ejecutar «preparar»"
    local sha origen mensaje
    sha="$(sed -n 2p "$destino.ok")"; origen="$(sed -n 4p "$destino.ok")"
    mensaje="Publicación $version ($version = $origen, commit ${sha:0:12} del repo de trabajo)"
    local nombre email
    nombre="$(git -C "$RAIZ" config user.name)" && email="$(git -C "$RAIZ" config user.email)" || die "falta user.name/user.email en git config del repo de trabajo"
    [ -n "$nombre" ] && [ -n "$email" ] || die "falta user.name/user.email en git config del repo de trabajo"
    local base="${PUBLICAR_TRABAJO:-$HOME/.cache/saguios-publicar}"
    local clon="$base/repo-publico"
    mkdir -p "$base"
    if [ -d "$clon/.git" ]; then
        [ "$(git -C "$clon" remote get-url origin)" = "$url" ] || die "$clon es el clon de otro repositorio; bórralo o usa otro PUBLICAR_TRABAJO"
        info "actualizando el clon del repo público"
        git -C "$clon" fetch origin --tags
        if git -C "$clon" rev-parse --verify -q origin/main >/dev/null; then git -C "$clon" checkout -q -B main origin/main; fi
    else
        info "clonando $url"
        git clone "$url" "$clon"
    fi
    git -C "$clon" rev-parse --verify -q "refs/tags/$version" >/dev/null && die "la etiqueta $version ya existe en el repo público: las publicaciones no se reescriben; corrige con una versión nueva"
    [ -n "$(git -C "$clon" status --porcelain)" ] && die "el clon $clon tiene cambios sin commitear"
    git -C "$clon" checkout -q -B main "$(git -C "$clon" rev-parse --verify -q origin/main || echo HEAD)" 2>/dev/null || git -C "$clon" checkout -q -B main
    # Sustituye el contenido (borra lo que ya no está) conservando solo .git
    find "$clon" -mindepth 1 -maxdepth 1 ! -name .git -exec rm -rf {} +
    cp -a "$destino/." "$clon/"
    git -C "$clon" add -A
    if git -C "$clon" rev-parse --verify -q HEAD >/dev/null && git -C "$clon" diff --cached --quiet; then
        die "no hay cambios respecto a la última publicación"
    fi
    GIT_AUTHOR_NAME="$nombre" GIT_AUTHOR_EMAIL="$email" GIT_COMMITTER_NAME="$nombre" GIT_COMMITTER_EMAIL="$email" \
        git -C "$clon" commit -q -m "$mensaje"
    GIT_COMMITTER_NAME="$nombre" GIT_COMMITTER_EMAIL="$email" git -C "$clon" tag -a "$version" -m "$mensaje"
    git -C "$clon" show --stat --format='%h %an: %s' HEAD | tail -n 6
    echo
    echo "Se va a hacer push de main y de la etiqueta $version a: $url"
    local resp=""
    if [ -t 0 ]; then read -r -p "escribe PUBLICAR para continuar: " resp || true; fi
    if [ "$resp" != "PUBLICAR" ]; then
        info "cancelado: el commit y la etiqueta quedan solo en el clon local ($clon); no se subió nada"
        exit 1
    fi
    git -C "$clon" push origin main
    git -C "$clon" push origin "$version"
    release "$version" "$url" "$imagenes" "$origen" "$sha"
}

release() {
    local version="$1" url="$2" imagenes="$3" origen="${4:-?}" sha="${5:-?}" repo
    repo="$(echo "$url" | sed -E 's#^(git@github\.com:|https://github\.com/)##; s#\.git$##')"
    # Nunca entrega/latest: son las imágenes PRIVADAS (input_share, NetSurf, GNUBoy, r8169 de Linux).
    [ -n "$imagenes" ] || for imagenes in "$RAIZ/entrega/$origen" ""; do [ -f "$imagenes/saguios.img" ] && break; done
    if [ -n "$imagenes" ]; then
        case "$(cd "$imagenes" 2>/dev/null && pwd)" in */entrega/latest*) die "$imagenes son imágenes privadas: no se publican";; esac
        if [ -f "$imagenes/VERSION.txt" ] && ! grep -q "^input_share: *no incluido" "$imagenes/VERSION.txt"; then
            die "$imagenes/VERSION.txt no dice «input_share: no incluido»: no son imágenes publicables (make variantes)"
        fi
        grep -i "^Aplicaciones:" "$imagenes/VERSION.txt" 2>/dev/null | grep -qi "netsurf\|gnuboy" && die "$imagenes lleva NetSurf o GNUBoy: no se publica"
    fi
    local notas="${PUBLICAR_TRABAJO:-$HOME/.cache/saguios-publicar}/notas-$version.md"
    cat > "$notas" <<NOTAS
SaguiOS-Extended $version ($version = $origen, commit ${sha:0:12} del repositorio de trabajo).

Licencia: GPL v3 (ver LICENSE y docs/licencia.md). Código fuente correspondiente de estas imágenes
(GPL v3, sección 6): el commit etiquetado \`$version\` de este repositorio. Las imágenes se
generan con \`make variantes\` (sin NetSurf, GNUBoy ni input_share); las sumas están en SHA256SUMS.txt.
NetSurf y GNUBoy (GPL v2) no se distribuyen: quien los quiera los compila para uso propio
(ver compilar_versiones.md).
NOTAS
    local ficheros=() f
    for f in saguios.img saguios-smp.img SHA256SUMS.txt VERSION.txt; do [ -n "$imagenes" ] && [ -f "$imagenes/$f" ] && ficheros+=("$imagenes/$f"); done
    local orden=(gh release create "$version" --title "SaguiOS-Extended $version" --notes-file "$notas" "${ficheros[@]}")
    echo
    echo "Release (binarios de la versión):"
    if [ "${#ficheros[@]}" -eq 0 ]; then
        echo "  no se encontraron imágenes: genera con «make variantes DESTINO=entrega/$version» o pasa --imagenes DIR; orden:"
    fi
    printf '  GH_REPO=%q ' "$repo"; printf '%q ' "${orden[@]}"; echo
    if [ "${#ficheros[@]}" -gt 0 ] && [ -x "$AQUI/gh-agentes.sh" ] && [ -t 0 ]; then
        local r; read -r -p "¿ejecutar la Release con tools/gh-agentes.sh? (escribe RELEASE): " r || true
        if [ "$r" = "RELEASE" ]; then GH_REPO="$repo" "$AQUI/gh-agentes.sh" "${orden[@]:1}"; fi
    fi
}

case "${1:-}" in
    preparar) shift; cmd_preparar "$@";;
    subir) shift; cmd_subir "$@";;
    -h|--help|"") uso 0;;
    *) uso;;
esac
