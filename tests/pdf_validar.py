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
    mb = re.search(rb"/MediaBox \[0 0 ([\d.]+) ([\d.]+)\]", objetos[paginas[0]])
    return dict(paginas=len(paginas), objetos=objetos, contenidos=textos,
                mediabox=(float(mb.group(1)), float(mb.group(2))),
                fuentes=sorted(set(re.findall(rb"/BaseFont /([A-Za-z-]+)", datos))))


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
