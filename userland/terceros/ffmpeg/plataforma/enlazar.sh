#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-only
# enlazar.sh - hace de «ld» para el configure de FFmpeg (D458, docs/ffmpeg.md): recibe órdenes al estilo gcc
# (-o salida, objetos, bibliotecas, -lm...) y enlaza como un programa de SaguiOS: el script de enlace, el
# arranque y principal_main, la libc común y la libsagui (libsistema.a, en grupo porque se llaman entre sí) y libgcc.
# Variables: FF_SIS_LIB, FF_SIS_INICIO, FF_PRINCIPAL, FF_LDS.
set -e
saida=a.out; objs=(); dirs=(); libs=()
while [ $# -gt 0 ]; do
    case "$1" in
        -o) saida="$2"; shift 2 ;;
        -o*) saida="${1#-o}"; shift ;;
        *.o|*.a) objs+=("$1"); shift ;;
        -L) dirs+=("$2"); shift 2 ;;
        -L*) dirs+=("${1#-L}"); shift ;;
        -l*) n="${1#-l}"
             for d in "${dirs[@]}"; do [ -f "$d/lib$n.a" ] && { libs+=("$d/lib$n.a"); break; }; done
             shift ;;
        -Wl,*|-m32|-nostdlib|-static|-pthread|-Wno-*|-W*|-f*|-O*|-g|-march=*|-mfpmath=*) shift ;;
        *) shift ;;
    esac
done
exec ld -m elf_i386 -nostdlib -T "$FF_LDS" --gc-sections -z max-page-size=4096 -z noexecstack --no-warn-rwx-segments \
    -o "$saida" "$FF_SIS_INICIO" "$FF_PRINCIPAL" "${objs[@]}" --start-group "${libs[@]}" "$FF_SIS_LIB" --end-group
