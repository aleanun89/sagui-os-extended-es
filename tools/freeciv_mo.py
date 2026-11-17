#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
# freeciv_mo.py - compila un .po de Freeciv a .mo (el formato de GNU gettext) sin msgfmt (D374).
#
#   tools/freeciv_mo.py ENTRADA.po [EXTRA.po ...] SALIDA.mo   (las extras pisan a la primera)
#
# Entiende msgctxt, msgid_plural, msgstr[n], cadenas partidas en varias líneas y descarta las
# entradas «fuzzy» y las sin traducir (como msgfmt). La salida es UTF-8, con la tabla ordenada
# para la búsqueda binaria del lector propio (userland/terceros/freeciv/plataforma/libintl.c).
import struct
import sys


def desescapar(s):
    sal = []
    i = 0
    while i < len(s):
        c = s[i]
        if c == '\\' and i + 1 < len(s):
            i += 1
            n = s[i]
            sal.append({'n': '\n', 't': '\t', 'r': '\r', '"': '"', '\\': '\\', 'a': '\a', 'b': '\b',
                        'f': '\f', 'v': '\v'}.get(n, n))
        else:
            sal.append(c)
        i += 1
    return ''.join(sal)


def leer_po(ruta):
    ents = []
    cur = None
    campo = None
    fuzzy = False

    def cerrar():
        nonlocal cur, fuzzy
        if cur is not None and not fuzzy and 'msgid' in cur:
            ents.append(cur)
        cur = None
        fuzzy = False

    with open(ruta, encoding='utf-8') as f:
        for linea in f:
            linea = linea.rstrip('\n')
            if linea.startswith('#,') and 'fuzzy' in linea:
                if cur is not None and 'msgstr' in cur or cur is not None and 'msgstr[0]' in cur:
                    cerrar()
                fuzzy = True
                continue
            if linea.startswith('#') or not linea.strip():
                if not linea.strip() and cur is not None and ('msgstr' in cur or 'msgstr[0]' in cur):
                    cerrar()
                continue
            if linea.startswith('"'):
                if cur is not None and campo is not None:
                    cur[campo] += desescapar(linea.strip()[1:-1])
                continue
            nombre, _, resto = linea.partition(' ')
            resto = resto.strip()
            if nombre in ('msgctxt', 'msgid') and cur is not None and ('msgstr' in cur or 'msgstr[0]' in cur):
                cerrar()
            if cur is None:
                cur = {}
            campo = nombre
            cur[campo] = desescapar(resto[1:-1])
    cerrar()
    return ents


def main():
    if len(sys.argv) < 3:
        print(__doc__)
        sys.exit(2)
    tabla = {}
    ents = []
    for i, ruta in enumerate(sys.argv[1:-1]):
        ents += [e for e in leer_po(ruta) if i == 0 or e['msgid'] != '']
    for e in ents:
        clave = e['msgid']
        if 'msgctxt' in e:
            clave = e['msgctxt'] + '\x04' + clave
        if 'msgid_plural' in e:
            clave += '\0' + e['msgid_plural']
            n = 0
            trad = []
            while 'msgstr[%d]' % n in e:
                trad.append(e['msgstr[%d]' % n])
                n += 1
            if not trad or all(t == '' for t in trad):
                continue
            valor = '\0'.join(trad)
        else:
            valor = e.get('msgstr', '')
            if valor == '' and clave != '':
                continue
        tabla[clave.encode('utf-8')] = valor.encode('utf-8')
    claves = sorted(tabla)
    n = len(claves)
    off_orig = 28
    off_trad = off_orig + 8 * n
    ini = off_trad + 8 * n
    blob = b''
    ro = []
    for k in claves:
        ro.append((len(k), ini + len(blob)))
        blob += k + b'\0'
    rt = []
    for k in claves:
        v = tabla[k]
        rt.append((len(v), ini + len(blob)))
        blob += v + b'\0'
    with open(sys.argv[-1], 'wb') as f:
        f.write(struct.pack('<7I', 0x950412de, 0, n, off_orig, off_trad, 0, 0))
        for l, o in ro:
            f.write(struct.pack('<II', l, o))
        for l, o in rt:
            f.write(struct.pack('<II', l, o))
        f.write(blob)
    print('%s: %d mensajes' % (sys.argv[-1], n))


main()
