# SPDX-License-Identifier: GPL-2.0-or-later
"""Validador de PDF para las pruebas (mejora 116, D291): sin dependencias externas.

Comprueba la estructura (cabecera, tabla xref con los desplazamientos exactos, trailer,
startxref), descomprime los flujos Flate y extrae el texto de los operadores Tj. Si el
anfitrión tiene qpdf o pdftotext, también los usa (`validar_externo`).
"""
import re
import shutil
import subprocess
import zlib


def analizar(datos):
    """Devuelve un diccionario con paginas, objetos, textos (por página), fuentes, mediabox."""
    assert datos.startswith(b"%PDF-1."), "falta la cabecera %PDF"
    assert datos.rstrip().endswith(b"%%EOF"), "falta %%EOF"
    m = re.search(rb"startxref\s+(\d+)\s+%%EOF\s*$", datos)
    assert m, "falta startxref"
    xref_pos = int(m.group(1))
    assert datos[xref_pos:xref_pos + 4] == b"xref", "startxref no apunta a xref"
    cab = re.match(rb"xref\s+0 (\d+)\s", datos[xref_pos:])
    n = int(cab.group(1))
    filas = re.findall(rb"(\d{10}) (\d{5}) ([nf]) \r?\n", datos[xref_pos:xref_pos + 30 + 20 * n + 40])
    assert len(filas) == n, "la xref debe tener %d entradas, tiene %d" % (n, len(filas))
    objetos = {}
    for i, (off, gen, t) in enumerate(filas):
        if t == b"f":
            continue
        off = int(off)
        assert datos[off:off + len(b"%d 0 obj" % i)] == b"%d 0 obj" % i, "el objeto %d no está en su desplazamiento" % i
        fin = datos.index(b"endobj", off)
        objetos[i] = datos[off:fin]
    assert b"/Type /Catalog" in objetos[1]
    paginas = []
    for i, cuerpo in objetos.items():
        if b"/Type /Page " in cuerpo or cuerpo.rstrip().endswith(b"/Type /Page >>") or re.search(rb"/Type /Page[ >]", cuerpo) and b"/Pages" not in cuerpo.split(b"/Type /Page")[1][:1]:
            if b"/Parent" in cuerpo:
                paginas.append(i)
    cnt = int(re.search(rb"/Count (\d+)", objetos[2]).group(1))
    assert cnt == len(paginas), "/Count %d y %d páginas" % (cnt, len(paginas))
    textos = []
    for p in sorted(paginas):
        c = int(re.search(rb"/Contents (\d+) 0 R", objetos[p]).group(1))
        obj = objetos[c]
        ln = int(re.search(rb"/Length (\d+)", obj).group(1))
        ini = obj.index(b"stream\n") + 7
        flujo = obj[ini:ini + ln]
        assert obj[ini + ln:].startswith(b"\nendstream"), "Length del flujo incorrecto"
        if b"/FlateDecode" in obj:
            flujo = zlib.decompress(flujo)
        textos.append(flujo)
    imagenes = []
    for i, cuerpo in objetos.items():
        if b"/Subtype /Image" not in cuerpo:
            continue
        ln = int(re.search(rb"/Length (\d+)", cuerpo).group(1))
        ini = cuerpo.index(b"stream\n") + 7
        flujo = cuerpo[ini:ini + ln]
        assert cuerpo[ini + ln:].startswith(b"\nendstream"), "Length del flujo de imagen incorrecto"
        filtro = re.search(rb"/Filter /(\w+)", cuerpo).group(1).decode()
        w = int(re.search(rb"/Width (\d+)", cuerpo).group(1))
        h = int(re.search(rb"/Height (\d+)", cuerpo).group(1))
        cs = re.search(rb"/ColorSpace /(\w+)", cuerpo).group(1).decode()
        datos_img = flujo
        if filtro == "FlateDecode":
            datos_img = zlib.decompress(flujo)
            assert len(datos_img) == w * h * (1 if cs == "DeviceGray" else 3), "el flujo de la imagen no mide ancho x alto x componentes"
        else:
            assert flujo[:2] == b"\xff\xd8", "una imagen DCTDecode debe empezar por FFD8"
        imagenes.append(dict(obj=i, ancho=w, alto=h, filtro=filtro, espacio=cs, datos=datos_img, bruto=flujo))
    # cada imagen usada por una página debe estar en su /XObject
    for p in paginas:
        for m in re.finditer(rb"/Im(\d+) (\d+) 0 R", objetos[p]):
            assert int(m.group(2)) in objetos and b"/Subtype /Image" in objetos[int(m.group(2))], "XObject sin imagen"
    incrustadas = _fuentes_incrustadas(objetos)
    mb = re.search(rb"/MediaBox \[0 0 ([\d.]+) ([\d.]+)\]", objetos[paginas[0]])
    return dict(paginas=len(paginas), objetos=objetos, contenidos=textos, imagenes=imagenes,
                mediabox=(float(mb.group(1)), float(mb.group(2))),
                fuentes=sorted(set(re.findall(rb"/BaseFont /([A-Za-z0-9+-]+)", datos))), incrustadas=incrustadas)


def _flujo(obj):
    ln = int(re.search(rb"/Length (\d+)", obj).group(1))
    ini = obj.index(b"stream\n") + 7
    assert obj[ini + ln:].startswith(b"\nendstream"), "Length del flujo de fuente incorrecto"
    f = obj[ini:ini + ln]
    return zlib.decompress(f) if b"/FlateDecode" in obj else f


def _fuentes_incrustadas(objetos):
    """Fuentes con FontDescriptor y archivo incrustado (D319): base, subtipo, FontFile2/3, Widths, bytes y
    la comprobación de que cada /Widths cubre 32..255 y de que todo objeto referenciado existe."""
    res = []
    for i, cuerpo in objetos.items():
        if b"/Type /Font " not in cuerpo and not re.search(rb"/Type /Font\b", cuerpo):
            continue
        d = re.search(rb"/FontDescriptor (\d+) 0 R", cuerpo)
        if not d:
            continue
        desc = objetos[int(d.group(1))]
        ff = re.search(rb"/(FontFile[23]?) (\d+) 0 R", desc)
        assert ff, "el FontDescriptor de la fuente %d no apunta a un archivo" % i
        obj = objetos[int(ff.group(2))]
        datos = _flujo(obj)
        w = re.search(rb"/Widths \[([^\]]*)\]", cuerpo)
        anchos = [int(x) for x in w.group(1).split()] if w else []
        fc, lc = int(re.search(rb"/FirstChar (\d+)", cuerpo).group(1)), int(re.search(rb"/LastChar (\d+)", cuerpo).group(1))
        assert len(anchos) == lc - fc + 1, "/Widths no cubre FirstChar..LastChar"
        assert b"/WinAnsiEncoding" in cuerpo, "la fuente incrustada debe ir en WinAnsi"
        l1 = re.search(rb"/Length1 (\d+)", obj)
        if l1:
            assert int(l1.group(1)) == len(datos), "Length1 no coincide con el tamaño real del archivo"
        res.append(dict(obj=i, base=re.search(rb"/BaseFont /([^ >/]+)", cuerpo).group(1).decode(),
                        subtipo=re.search(rb"/Subtype /(\w+)", cuerpo).group(1).decode(), tipo_archivo=ff.group(1).decode(),
                        anchos=anchos, primero=fc, datos=datos, flags=int(re.search(rb"/Flags (\d+)", desc).group(1))))
    return res


def validar_fuente_incrustada(f, tmp_dir):
    """Valida una fuente incrustada: TrueType (sfnt 0x00010000 con head/glyf/loca/cmap/hmtx y suma
    correcta) o OpenType CFF; y, si el anfitrión tiene fc-scan (FreeType), que la abre. Devuelve el
    charset de fc-scan o None."""
    import os
    import struct
    d = f["datos"]
    if f["tipo_archivo"] == "FontFile2":
        assert d[:4] == b"\x00\x01\x00\x00", "FontFile2 debe ser TrueType"
        n = struct.unpack(">H", d[4:6])[0]
        tags = [d[12 + 16 * k:16 + 16 * k] for k in range(n)]
        for t in (b"head", b"hhea", b"maxp", b"hmtx", b"loca", b"glyf", b"cmap"):
            assert t in tags, "falta la tabla %s en la fuente incrustada" % t
        tot = 0
        for k in range(0, len(d) - len(d) % 4, 4):
            tot += struct.unpack(">I", d[k:k + 4])[0]
        resto = d[len(d) - len(d) % 4:]
        if resto:
            tot += struct.unpack(">I", resto + b"\0" * (4 - len(resto)))[0]
        assert tot & 0xFFFFFFFF == 0xB1B0AFBA, "checksumAdjustment de la fuente incrustada incorrecto"
    else:
        assert d[:4] in (b"OTTO", b"\x00\x01\x00\x00", b"true"), "FontFile3 debe ser OpenType"
    if shutil.which("fc-scan"):
        ruta = os.path.join(tmp_dir, "emb-%d.font" % f["obj"])
        open(ruta, "wb").write(d)
        r = subprocess.run(["fc-scan", "--format", "%{charset}", ruta], capture_output=True, text=True)
        assert r.returncode == 0 and r.stdout.strip(), "FreeType (fc-scan) no abre la fuente incrustada %s: %s" % (f["base"], r.stderr)
        return r.stdout.strip()
    return None


def _desescapar(s):
    s = re.sub(rb"\\([0-7]{3})", lambda m: bytes([int(m.group(1), 8)]), s)
    return re.sub(rb"\\([()\\])", rb"\1", s)


def texto_de(contenido):
    """El texto (Latin-1) de los Tj de un flujo de página, uniendo trozos de una línea con espacio."""
    partes = re.findall(rb"\((.*?)(?<!\\)\) Tj", contenido, re.S)
    return [_desescapar(p).decode("latin-1") for p in partes]


def validar_externo(ruta):
    """Resultado de qpdf --check y pdftotext si existen; None si no están."""
    res = {}
    if shutil.which("qpdf"):
        r = subprocess.run(["qpdf", "--check", ruta], capture_output=True, text=True)
        res["qpdf"] = (r.returncode, r.stdout + r.stderr)
    if shutil.which("pdftotext"):
        r = subprocess.run(["pdftotext", ruta, "-"], capture_output=True, text=True)
        res["pdftotext"] = (r.returncode, r.stdout)
    return res or None
