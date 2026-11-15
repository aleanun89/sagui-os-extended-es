#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H319 (D319) - prueba de unidad en el anfitrión del subconjunto TrueType y de la lectura de la
tabla name/head para el PDF (userland/libfontes/subconjunto.c, compilado con gcc para el PC).

Comprueba, para las DejaVu del sistema: sumas de las tablas y checksumAdjustment, loca creciente,
la cmap (3,1) nueva da el mismo glifo que la original para cada carácter usado, los glifos
copiados son idénticos a los originales (también los compuestos: Á, é, ñ), los no usados quedan
vacíos y el tamaño baja de verdad. Sin dependencias externas."""
import os
import struct
import subprocess
import sys
import tempfile

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FUENTES = os.path.join(RAIZ, "rootfs", "fontes")
TEXTO = "Hola, \xf1and\xfa \xc1gil \xc0\xc9\xee\xfc \xbf?\xa1!"


def tablas(d):
    n = struct.unpack(">H", d[4:6])[0]
    t = {}
    for i in range(n):
        tag, cs, off, ln = struct.unpack(">4sIII", d[12 + 16 * i:28 + 16 * i])
        t[tag] = (cs, off, ln)
    return t


def suma(b):
    b = b + b"\0" * (-len(b) % 4)
    return sum(struct.unpack(">%dI" % (len(b) // 4), b)) & 0xFFFFFFFF


def cmap31(d, t):
    _, off, ln = t[b"cmap"]
    c = d[off:off + ln]
    ns = struct.unpack(">H", c[2:4])[0]
    sub = None
    for i in range(ns):
        plat, enc, o = struct.unpack(">HHI", c[4 + 8 * i:12 + 8 * i])
        if (plat, enc) == (3, 1):
            sub = c[o:]
    assert sub is not None, "falta la cmap (3,1)"
    fmt, ln2, _, segx2 = struct.unpack(">HHHH", sub[:8])
    assert fmt == 4
    seg = segx2 // 2
    fin = struct.unpack(">%dH" % seg, sub[14:14 + segx2])
    ini = struct.unpack(">%dH" % seg, sub[16 + segx2:16 + 2 * segx2])
    dl = struct.unpack(">%dH" % seg, sub[16 + 2 * segx2:16 + 3 * segx2])
    ro = struct.unpack(">%dH" % seg, sub[16 + 3 * segx2:16 + 4 * segx2])
    assert ln2 == 16 + 4 * segx2, "la longitud de la subtabla cmap no cuadra"

    def gid(cp):
        for i in range(seg):
            if cp <= fin[i]:
                if cp < ini[i]:
                    return 0
                if ro[i] == 0:
                    return (cp + dl[i]) & 0xFFFF
                pos = 16 + 3 * segx2 + 2 * i + ro[i] + 2 * (cp - ini[i])
                g = struct.unpack(">H", sub[pos:pos + 2])[0]
                return (g + dl[i]) & 0xFFFF if g else 0
        return 0
    return gid


def cmap_orig(d, t):
    """Lee la cmap de la fuente original (formato 4 o 12)."""
    _, off, ln = t[b"cmap"]
    c = d[off:off + ln]
    ns = struct.unpack(">H", c[2:4])[0]
    for i in range(ns):
        plat, enc, o = struct.unpack(">HHI", c[4 + 8 * i:12 + 8 * i])
        if (plat, enc) == (3, 1):
            sub = c[o:]
            fmt, ln2, _, segx2 = struct.unpack(">HHHH", sub[:8])
            seg = segx2 // 2
            fin = struct.unpack(">%dH" % seg, sub[14:14 + segx2])
            ini = struct.unpack(">%dH" % seg, sub[16 + segx2:16 + 2 * segx2])
            dl = struct.unpack(">%dH" % seg, sub[16 + 2 * segx2:16 + 3 * segx2])
            ro = struct.unpack(">%dH" % seg, sub[16 + 3 * segx2:16 + 4 * segx2])

            def gid(cp):
                for k in range(seg):
                    if cp <= fin[k]:
                        if cp < ini[k]:
                            return 0
                        if ro[k] == 0:
                            return (cp + dl[k]) & 0xFFFF
                        pos = 16 + 3 * segx2 + 2 * k + ro[k] + 2 * (cp - ini[k])
                        g = struct.unpack(">H", sub[pos:pos + 2])[0]
                        return (g + dl[k]) & 0xFFFF if g else 0
                return 0
            return gid
    raise AssertionError("la fuente original no tiene cmap (3,1)")


def glifos(d, t):
    head = d[t[b"head"][1]:t[b"head"][1] + 54]
    largo = struct.unpack(">h", head[50:52])[0]
    ng = struct.unpack(">H", d[t[b"maxp"][1] + 4:t[b"maxp"][1] + 6])[0]
    lo, ll = t[b"loca"][1], t[b"loca"][2]
    if largo:
        offs = struct.unpack(">%dI" % (ng + 1), d[lo:lo + 4 * (ng + 1)])
    else:
        offs = [x * 2 for x in struct.unpack(">%dH" % (ng + 1), d[lo:lo + 2 * (ng + 1)])]
    g0 = t[b"glyf"][1]
    return ng, offs, lambda g: d[g0 + offs[g]:g0 + offs[g + 1]]


def comprobar(cond, msg):
    print(("ok   " if cond else "FALLA ") + msg)
    if not cond:
        sys.exit(1)


def main():
    tmp = tempfile.mkdtemp(prefix="h319-")
    exe = os.path.join(tmp, "subconj")
    r = subprocess.run(["gcc", "-O1", "-Wall", "-Werror", "-I" + os.path.join(RAIZ, "userland", "inc"), "-o", exe,
                        os.path.join(RAIZ, "tests", "h319_subconj_anfitrion.c")], capture_output=True, text=True)
    comprobar(r.returncode == 0, "el subconjunto compila en el anfitrión (%s)" % r.stderr.strip()[:200])
    usados = TEXTO.encode("latin-1")
    for nombre in ("DejaVuSans.ttf", "DejaVuSerif.ttf", "DejaVuSansMono-Bold.ttf", "DejaVuSerif-Italic.ttf"):
        ruta = os.path.join(FUENTES, nombre)
        sal = os.path.join(tmp, "s-" + nombre)
        r = subprocess.run([exe, ruta, sal, usados], capture_output=True)
        comprobar(r.returncode == 0, "%s: subconjunto escrito" % nombre)
        info = r.stdout.decode().splitlines()[0]
        comprobar(info.startswith("info DejaVu") and "cff=0" in info, "%s: la info de PDF lleva el nombre PostScript (%s)" % (nombre, info[:60]))
        if "Mono" in nombre:
            comprobar("flags=33" in info or "flags=35" in info or "flags=97" in info, "%s: FixedPitch en las banderas (%s)" % (nombre, info))
        if "Italic" in nombre:
            comprobar("flags=96" in info or "flags=97" in info, "%s: Italic en las banderas (%s)" % (nombre, info))
        orig = open(ruta, "rb").read()
        sub = open(sal, "rb").read()
        comprobar(len(sub) < len(orig) // 5, "%s: el subconjunto es mucho menor (%d de %d bytes)" % (nombre, len(sub), len(orig)))
        to, ts = tablas(orig), tablas(sub)
        comprobar(suma(sub) == 0xB1B0AFBA, "%s: checksumAdjustment correcto" % nombre)
        for tag, (cs, off, ln) in ts.items():
            b = bytearray(sub[off:off + ln])
            if tag == b"head":
                b[8:12] = b"\0\0\0\0"
            comprobar(suma(bytes(b)) == cs, "%s: suma de la tabla %s" % (nombre, tag.decode()))
        comprobar(set(ts) >= {b"head", b"hhea", b"maxp", b"hmtx", b"loca", b"glyf", b"cmap", b"post"}, "%s: tablas necesarias" % nombre)
        ngo, offo, go = glifos(orig, to)
        ngs, offs, gs = glifos(sub, ts)
        comprobar(all(offs[i] <= offs[i + 1] for i in range(ngs)), "%s: loca creciente" % nombre)
        comprobar(struct.unpack(">h", sub[ts[b"head"][1] + 50:ts[b"head"][1] + 52])[0] == 1, "%s: loca larga" % nombre)
        comprobar(ngs < ngo, "%s: menos glifos (%d de %d)" % (nombre, ngs, ngo))
        nh = struct.unpack(">H", sub[ts[b"hhea"][1] + 34:ts[b"hhea"][1] + 36])[0]
        comprobar(nh == ngs and ts[b"hmtx"][2] == 4 * ngs, "%s: hmtx a 4 bytes por glifo" % nombre)
        gid_o, gid_s = cmap_orig(orig, to), cmap31(sub, ts)
        cps = {}
        for b in usados:
            cp = {0x80: 0x20AC}.get(b, b)
            if b >= 32:
                cps[b] = cp
        for b, cp in sorted(cps.items()):
            go_id, gs_id = gid_o(cp), gid_s(cp)
            if go_id == 0:
                continue
            comprobar(gs_id == go_id, "%s: cmap U+%04X -> glifo %d" % (nombre, cp, go_id))
            a, b2 = gs(gs_id), go(go_id)
            comprobar(a[:len(b2)] == b2 and not any(a[len(b2):]), "%s: glifo %d idéntico (salvo el relleno)" % (nombre, go_id))
        # los glifos no usados (p. ej. la 'z' si no se usó) quedan vacíos
        z = gid_o(ord("z"))
        if z < ngs:
            comprobar(len(gs(z)) == 0, "%s: el glifo de 'z' (no usado) está vacío" % nombre)
        # los compuestos: cada componente referenciado existe
        for cp in (0xC1, 0xC0, 0xC9, 0xEE, 0xFC, 0xF1):
            g = gid_s(cp)
            d = gs(g)
            if len(d) > 10 and struct.unpack(">h", d[:2])[0] < 0:
                p, hay = 10, True
                while hay:
                    fl, gi = struct.unpack(">HH", d[p:p + 4])
                    comprobar(gi < ngs and len(gs(gi)) > 0, "%s: componente %d de U+%04X incluido" % (nombre, gi, cp))
                    p += 4 + (4 if fl & 1 else 2)
                    p += 2 if fl & 8 else 4 if fl & 0x40 else 8 if fl & 0x80 else 0
                    hay = bool(fl & 0x20)
    # una fuente que no es sfnt se rechaza
    malo = os.path.join(tmp, "malo.ttf")
    open(malo, "wb").write(b"no soy una fuente" * 10)
    r = subprocess.run([exe, malo, os.path.join(tmp, "x"), b"a"], capture_output=True)
    comprobar(r.returncode == 4, "un archivo que no es una fuente se rechaza")
    print("h319 subconjunto: todo bien")


if __name__ == "__main__":
    main()
