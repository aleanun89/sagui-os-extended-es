#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Validador de PPTX/TCN para las pruebas de Tucán (D369): solo la biblioteca estándar de Python.

  validar_pptx(datos)        comprueba el paquete OOXML que escribe Tucán: ZIP correcto, todos los XML bien formados,
                             tipos de contenido y relaciones coherentes, diapositivas en orden, y devuelve un resumen
                             (textos por diapositiva, notas, imágenes, tamaño).
  crear_pptx_externo()       un PPTX escrito a mano (sin Tucán) con el estilo de PowerPoint: marcadores heredados de la
                             maqueta, formas, imagen, tabla, notas y una animación que se ignora; sirve para comprobar
                             el importador con un archivo que no hizo nuestro escritor.
  validar_tcn(datos)         el formato propio: ZIP con tucan.txt y media/.

Ejecutado como programa hace la prueba de unidad sobre un PPTX creado aquí (sin QEMU).
"""
import io
import posixpath
import struct
import sys
import xml.etree.ElementTree as ET
import zipfile
import zlib

NS = {
    "a": "http://schemas.openxmlformats.org/drawingml/2006/main",
    "p": "http://schemas.openxmlformats.org/presentationml/2006/main",
    "r": "http://schemas.openxmlformats.org/officeDocument/2006/relationships",
    "rel": "http://schemas.openxmlformats.org/package/2006/relationships",
    "ct": "http://schemas.openxmlformats.org/package/2006/content-types",
}
RT = "http://schemas.openxmlformats.org/officeDocument/2006/relationships/"
EMU = 12700


def _ruta(parte, destino):
    if destino.startswith("/"):
        return destino[1:]
    return posixpath.normpath(posixpath.join(posixpath.dirname(parte), destino))


def _rels(z, parte):
    d, b = posixpath.split(parte)
    nombre = posixpath.join(d, "_rels", b + ".rels")
    if nombre not in z.namelist():
        return {}
    raiz = ET.fromstring(z.read(nombre))
    out = {}
    for r in raiz.findall("rel:Relationship", NS):
        out[r.get("Id")] = (r.get("Type"), _ruta(parte, r.get("Target")))
    return out


def textos_de(elemento):
    """Los párrafos de un cuerpo de texto: lista de cadenas."""
    res = []
    for p in elemento.iter("{%s}p" % NS["a"]):
        res.append("".join(t.text or "" for t in p.iter("{%s}t" % NS["a"])))
    return res


def validar_pptx(datos):
    z = zipfile.ZipFile(io.BytesIO(datos))
    assert z.testzip() is None, "el ZIP tiene entradas dañadas"
    nombres = z.namelist()
    assert len(nombres) == len(set(nombres)), "nombres repetidos en el ZIP"
    # todos los XML bien formados
    for n in nombres:
        if n.endswith(".xml") or n.endswith(".rels"):
            ET.fromstring(z.read(n))
    # tipos de contenido: cada parte tiene uno
    ct = ET.fromstring(z.read("[Content_Types].xml"))
    defaults = {d.get("Extension").lower() for d in ct.findall("ct:Default", NS)}
    overrides = {o.get("PartName") for o in ct.findall("ct:Override", NS)}
    for n in nombres:
        if n == "[Content_Types].xml" or n.endswith("/"):
            continue
        ext = n.rsplit(".", 1)[-1].lower()
        assert ("/" + n) in overrides or ext in defaults, "sin tipo de contenido: " + n
    for o in overrides:
        assert o[1:] in nombres, "tipo de contenido de una parte que no existe: " + o
    # relaciones: todos los destinos internos existen
    for n in nombres:
        if n.endswith(".rels") and n != "_rels/.rels":
            parte = posixpath.join(posixpath.dirname(posixpath.dirname(n)), posixpath.basename(n)[:-5])
            for rid, (tipo, dest) in _rels(z, parte).items():
                assert dest in nombres, "%s: la relación %s apunta a %s, que no existe" % (n, rid, dest)
    raiz_rels = ET.fromstring(z.read("_rels/.rels"))
    pres = [r.get("Target") for r in raiz_rels if r.get("Type") == RT + "officeDocument"]
    assert pres == ["ppt/presentation.xml"], "falta el documento principal"
    px = ET.fromstring(z.read("ppt/presentation.xml"))
    sz = px.find("p:sldSz", NS)
    ancho, alto = int(sz.get("cx")) // EMU, int(sz.get("cy")) // EMU
    prels = _rels(z, "ppt/presentation.xml")
    diapos = []
    for sid in px.find("p:sldIdLst", NS):
        rid = sid.get("{%s}id" % NS["r"])
        tipo, dest = prels[rid]
        assert tipo == RT + "slide", "sldId no apunta a una diapositiva"
        sx = ET.fromstring(z.read(dest))
        textos = []
        n_pics = 0
        n_tablas = 0
        formas = []
        for sp in sx.iter("{%s}sp" % NS["p"]):
            tb = sp.find("p:txBody", NS)
            if tb is not None:
                textos.extend(t for t in textos_de(tb) if t)
            g = sp.find("p:spPr/a:prstGeom", NS)
            if g is not None:
                formas.append(g.get("prst"))
        for pic in sx.iter("{%s}pic" % NS["p"]):
            n_pics += 1
            emb = pic.find("p:blipFill/a:blip", NS).get("{%s}embed" % NS["r"])
            srels = _rels(z, dest)
            assert emb in srels and srels[emb][0] == RT + "image", "la imagen no tiene relación"
        n_tablas = len(list(sx.iter("{%s}tbl" % NS["a"])))
        for tc in sx.iter("{%s}tc" % NS["a"]):
            textos.extend(t for t in textos_de(tc) if t)
        notas = []
        for rid2, (tipo2, dest2) in _rels(z, dest).items():
            if tipo2 == RT + "notesSlide":
                nx = ET.fromstring(z.read(dest2))
                for sp in nx.iter("{%s}sp" % NS["p"]):
                    ph = sp.find("p:nvSpPr/p:nvPr/p:ph", NS)
                    if ph is not None and ph.get("type") == "body":
                        notas.extend(t for t in textos_de(sp.find("p:txBody", NS)) if t)
        tr = sx.find("p:transition", NS)
        diapos.append(dict(parte=dest, textos=textos, pics=n_pics, tablas=n_tablas, formas=formas, notas=notas,
                           transicion=None if tr is None else [c.tag.split("}")[1] for c in tr],
                           oculta=sx.get("show") == "0"))
    return dict(ancho=ancho, alto=alto, diapositivas=diapos, partes=nombres)


def validar_tcn(datos):
    z = zipfile.ZipFile(io.BytesIO(datos))
    assert z.testzip() is None
    txt = z.read("tucan.txt").decode("latin-1")
    assert txt.startswith("TUCAN 1\n")
    lineas = txt.splitlines()
    n_diap = sum(1 for l in lineas if l.startswith("diap "))
    n_obj = sum(1 for l in lineas if l.startswith("obj "))
    medios = [n for n in z.namelist() if n.startswith("media/")]
    return dict(diapositivas=n_diap, objetos=n_obj, medios=medios, texto=txt)


def _png(w, h, rgb):
    def trozo(t, d):
        c = struct.pack(">I", len(d)) + t + d
        return c + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    raw = b"".join(b"\x00" + bytes(rgb) * w for _ in range(h))
    return (b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
            trozo(b"IDAT", zlib.compress(raw)) + trozo(b"IEND", b""))


A = 'xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"'
R = 'xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"'
P = 'xmlns:p="http://schemas.openxmlformats.org/presentationml/2006/main"'
GRP = ('<p:nvGrpSpPr><p:cNvPr id="1" name=""/><p:cNvGrpSpPr/><p:nvPr/></p:nvGrpSpPr><p:grpSpPr><a:xfrm><a:off x="0" y="0"/>'
       '<a:ext cx="0" cy="0"/><a:chOff x="0" y="0"/><a:chExt cx="0" cy="0"/></a:xfrm></p:grpSpPr>')
CAB = '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n'


def crear_pptx_externo():
    """Un PPTX 'de PowerPoint' hecho a mano: 4:3, marcadores sin posición (la hereda de la maqueta), acentos en UTF-8,
    forma con relleno, imagen, tabla, notas, animación y transición."""
    e = EMU
    ct = (CAB + '<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">'
          '<Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>'
          '<Default Extension="xml" ContentType="application/xml"/><Default Extension="png" ContentType="image/png"/>'
          '<Override PartName="/ppt/presentation.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.presentation.main+xml"/>'
          '<Override PartName="/ppt/slideMasters/slideMaster1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideMaster+xml"/>'
          '<Override PartName="/ppt/slideLayouts/slideLayout1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slideLayout+xml"/>'
          '<Override PartName="/ppt/slides/slide1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>'
          '<Override PartName="/ppt/slides/slide2.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/>'
          '<Override PartName="/ppt/notesSlides/notesSlide1.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.notesSlide+xml"/>'
          '</Types>')
    rels0 = (CAB + '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
             '<Relationship Id="rId1" Type="%sofficeDocument" Target="ppt/presentation.xml"/></Relationships>' % RT)
    pres = (CAB + '<p:presentation %s %s %s><p:sldMasterIdLst><p:sldMasterId id="2147483648" r:id="rId1"/></p:sldMasterIdLst>'
            '<p:sldIdLst><p:sldId id="256" r:id="rId2"/><p:sldId id="257" r:id="rId3"/></p:sldIdLst>'
            '<p:sldSz cx="%d" cy="%d" type="screen4x3"/></p:presentation>' % (A, R, P, 720 * e, 540 * e))
    prels = (CAB + '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
             '<Relationship Id="rId1" Type="%sslideMaster" Target="slideMasters/slideMaster1.xml"/>'
             '<Relationship Id="rId2" Type="%sslide" Target="slides/slide1.xml"/>'
             '<Relationship Id="rId3" Type="%sslide" Target="slides/slide2.xml"/></Relationships>' % (RT, RT, RT))
    maestra = (CAB + '<p:sldMaster %s %s %s><p:cSld><p:spTree>%s'
               '<p:sp><p:nvSpPr><p:cNvPr id="2" name="Título"/><p:cNvSpPr/><p:nvPr><p:ph type="title"/></p:nvPr></p:nvSpPr><p:spPr><a:xfrm>'
               '<a:off x="%d" y="%d"/><a:ext cx="%d" cy="%d"/></a:xfrm></p:spPr></p:sp></p:spTree></p:cSld>'
               '<p:sldLayoutIdLst><p:sldLayoutId id="2147483649" r:id="rId1"/></p:sldLayoutIdLst></p:sldMaster>'
               % (A, R, P, GRP, 36 * e, 30 * e, 648 * e, 90 * e))
    maestra_rels = (CAB + '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
                    '<Relationship Id="rId1" Type="%sslideLayout" Target="../slideLayouts/slideLayout1.xml"/></Relationships>' % RT)
    # la maqueta da posición a un marcador de contenido (idx 1); el título la hereda de la maestra
    maqueta = (CAB + '<p:sldLayout %s %s %s type="obj"><p:cSld name="Titulo y objetos"><p:spTree>%s'
               '<p:sp><p:nvSpPr><p:cNvPr id="3" name="Contenido"/><p:cNvSpPr/><p:nvPr><p:ph idx="1"/></p:nvPr></p:nvSpPr><p:spPr><a:xfrm>'
               '<a:off x="%d" y="%d"/><a:ext cx="%d" cy="%d"/></a:xfrm></p:spPr></p:sp></p:spTree></p:cSld></p:sldLayout>'
               % (A, R, P, GRP, 36 * e, 140 * e, 648 * e, 340 * e))
    maqueta_rels = (CAB + '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
                    '<Relationship Id="rId1" Type="%sslideMaster" Target="../slideMasters/slideMaster1.xml"/></Relationships>' % RT)
    s1 = (CAB + '<p:sld %s %s %s><p:cSld><p:bg><p:bgPr><a:solidFill><a:srgbClr val="FFF2CC"/></a:solidFill><a:effectLst/></p:bgPr></p:bg><p:spTree>%s'
          '<p:sp><p:nvSpPr><p:cNvPr id="2" name="T"/><p:cNvSpPr><a:spLocks noGrp="1"/></p:cNvSpPr><p:nvPr><p:ph type="title"/></p:nvPr></p:nvSpPr><p:spPr/>'
          '<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:rPr lang="es-ES" sz="4000" b="1"/><a:t>Título desde PowerPoint ñandú</a:t></a:r></a:p></p:txBody></p:sp>'
          '<p:sp><p:nvSpPr><p:cNvPr id="3" name="C"/><p:cNvSpPr><a:spLocks noGrp="1"/></p:cNvSpPr><p:nvPr><p:ph idx="1"/></p:nvPr></p:nvSpPr><p:spPr/>'
          '<p:txBody><a:bodyPr/><a:lstStyle/>'
          '<a:p><a:pPr><a:buChar char="&#8226;"/></a:pPr><a:r><a:rPr lang="es-ES" sz="2800"/><a:t>Punto uno &amp; más</a:t></a:r></a:p>'
          '<a:p><a:pPr lvl="1"><a:buChar char="&#8226;"/></a:pPr><a:r><a:rPr lang="es-ES" sz="2400"/><a:t>Subpunto </a:t></a:r><a:r><a:rPr lang="es-ES"/><a:t>dos</a:t></a:r></a:p>'
          '</p:txBody></p:sp>'
          '<p:sp><p:nvSpPr><p:cNvPr id="4" name="Forma"/><p:cNvSpPr/><p:nvPr/></p:nvSpPr><p:spPr><a:xfrm><a:off x="%d" y="%d"/><a:ext cx="%d" cy="%d"/></a:xfrm>'
          '<a:prstGeom prst="ellipse"><a:avLst/></a:prstGeom><a:solidFill><a:srgbClr val="70AD47"/></a:solidFill><a:ln w="19050"><a:solidFill><a:srgbClr val="375623"/></a:solidFill></a:ln></p:spPr>'
          '<p:txBody><a:bodyPr anchor="ctr"/><a:lstStyle/><a:p><a:pPr algn="ctr"/><a:r><a:rPr lang="es-ES" sz="2000"/><a:t>Elipse verde</a:t></a:r></a:p></p:txBody></p:sp>'
          '<p:timing><p:tnLst><p:par><p:cTn id="1"/></p:par></p:tnLst></p:timing>'
          '</p:spTree></p:cSld><p:transition spd="med" advTm="4000"><p:fade/></p:transition></p:sld>'
          % (A, R, P, GRP, 480 * e, 380 * e, 180 * e, 100 * e))
    s1_rels = (CAB + '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
               '<Relationship Id="rId1" Type="%sslideLayout" Target="../slideLayouts/slideLayout1.xml"/>'
               '<Relationship Id="rId2" Type="%snotesSlide" Target="../notesSlides/notesSlide1.xml"/></Relationships>' % (RT, RT))
    notas = (CAB + '<p:notes %s %s %s><p:cSld><p:spTree>%s'
             '<p:sp><p:nvSpPr><p:cNvPr id="2" name="Img"/><p:cNvSpPr/><p:nvPr><p:ph type="sldImg"/></p:nvPr></p:nvSpPr><p:spPr/></p:sp>'
             '<p:sp><p:nvSpPr><p:cNvPr id="3" name="Notas"/><p:cNvSpPr/><p:nvPr><p:ph type="body" idx="1"/></p:nvPr></p:nvSpPr><p:spPr/>'
             '<p:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>Nota del orador externa</a:t></a:r></a:p></p:txBody></p:sp></p:spTree></p:cSld></p:notes>'
             % (A, R, P, GRP))
    s2 = (CAB + '<p:sld %s %s %s><p:cSld><p:spTree>%s'
          '<p:pic><p:nvPicPr><p:cNvPr id="2" name="Imagen"/><p:cNvPicPr/><p:nvPr/></p:nvPicPr><p:blipFill><a:blip r:embed="rId2"/><a:stretch><a:fillRect/></a:stretch></p:blipFill>'
          '<p:spPr><a:xfrm><a:off x="%d" y="%d"/><a:ext cx="%d" cy="%d"/></a:xfrm><a:prstGeom prst="rect"><a:avLst/></a:prstGeom></p:spPr></p:pic>'
          '<p:graphicFrame><p:nvGraphicFramePr><p:cNvPr id="3" name="Tabla"/><p:cNvGraphicFramePr/><p:nvPr/></p:nvGraphicFramePr><p:xfrm><a:off x="%d" y="%d"/><a:ext cx="%d" cy="%d"/></p:xfrm>'
          '<a:graphic><a:graphicData uri="http://schemas.openxmlformats.org/drawingml/2006/table"><a:tbl><a:tblPr/><a:tblGrid><a:gridCol w="%d"/><a:gridCol w="%d"/></a:tblGrid>'
          '<a:tr h="%d"><a:tc><a:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>A1</a:t></a:r></a:p></a:txBody><a:tcPr/></a:tc><a:tc><a:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>B1</a:t></a:r></a:p></a:txBody><a:tcPr/></a:tc></a:tr>'
          '<a:tr h="%d"><a:tc><a:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>A2</a:t></a:r></a:p></a:txBody><a:tcPr/></a:tc><a:tc><a:txBody><a:bodyPr/><a:lstStyle/><a:p><a:r><a:t>B2</a:t></a:r></a:p></a:txBody><a:tcPr/></a:tc></a:tr>'
          '</a:tbl></a:graphicData></a:graphic></p:graphicFrame></p:spTree></p:cSld></p:sld>'
          % (A, R, P, GRP, 60 * e, 120 * e, 200 * e, 150 * e, 320 * e, 120 * e, 340 * e, 100 * e, 170 * e, 170 * e, 50 * e, 50 * e))
    s2_rels = (CAB + '<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">'
               '<Relationship Id="rId1" Type="%sslideLayout" Target="../slideLayouts/slideLayout1.xml"/>'
               '<Relationship Id="rId2" Type="%simage" Target="../media/image1.png"/></Relationships>' % (RT, RT))
    buf = io.BytesIO()
    with zipfile.ZipFile(buf, "w", zipfile.ZIP_DEFLATED) as z:
        z.writestr("[Content_Types].xml", ct)
        z.writestr("_rels/.rels", rels0)
        z.writestr("ppt/presentation.xml", pres)
        z.writestr("ppt/_rels/presentation.xml.rels", prels)
        z.writestr("ppt/slideMasters/slideMaster1.xml", maestra)
        z.writestr("ppt/slideMasters/_rels/slideMaster1.xml.rels", maestra_rels)
        z.writestr("ppt/slideLayouts/slideLayout1.xml", maqueta)
        z.writestr("ppt/slideLayouts/_rels/slideLayout1.xml.rels", maqueta_rels)
        z.writestr("ppt/slides/slide1.xml", s1)
        z.writestr("ppt/slides/_rels/slide1.xml.rels", s1_rels)
        z.writestr("ppt/slides/slide2.xml", s2)
        z.writestr("ppt/slides/_rels/slide2.xml.rels", s2_rels)
        z.writestr("ppt/notesSlides/notesSlide1.xml", notas)
        z.writestr("ppt/media/image1.png", _png(40, 30, (200, 40, 40)))
    return buf.getvalue()


if __name__ == "__main__":
    datos = crear_pptx_externo()
    r = validar_pptx(datos)
    assert (r["ancho"], r["alto"]) == (720, 540)
    assert len(r["diapositivas"]) == 2
    assert "Título desde PowerPoint ñandú" in r["diapositivas"][0]["textos"]
    assert r["diapositivas"][0]["notas"] == ["Nota del orador externa"]
    assert r["diapositivas"][1]["pics"] == 1 and r["diapositivas"][1]["tablas"] == 1
    print("tucan_pptx_validar: el PPTX externo de prueba es válido")
    sys.exit(0)
