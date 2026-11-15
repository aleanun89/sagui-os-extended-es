#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H296 - Pluma: WordArt e imágenes (mejora 122, D296).

Parte 1, sin ventana (`pluma --wordart`, `pluma --imagem`, `--convertir`, `--info`):
  - 20 estilos de WordArt: cada uno sale en el RTF como \\pict con un PNG válido, con colores y sin ser
    una hoja en blanco; ida y vuelta del RTF estable (el WordArt se vuelve a dibujar de sus parámetros);
  - imágenes BMP, PNG, JPEG y GIF: el PNG y el JPEG se incrustan sin tocar (mismos bytes), BMP y GIF
    salen como PNG sin pérdida (se compara píxel a píxel); tamaño en cm;
  - lectura de RTF de Word y de LibreOffice con \\pict (incluido \\nonshppict);
  - PDF: XObject de imagen (Flate para RGB, DCTDecode para el JPEG tal cual), validado con pdf_validar.
Parte 2, con ventana: Insertar > WordArt (galería), Insertar > Imagen (diálogo de archivos), tirador de
tamaño, doble clic -> Formato de imagen (ajuste, borde, pie), guardar, exportar a PDF, reabrir.
Capturas docs/capturas/H296-*.png.
"""
import os
import re
import struct
import sys
import time
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, mtools, SAIDA  # noqa: E402
import pdf_validar  # noqa: E402
import jpeg_baseline  # noqa: E402
from h291_pluma import esperar_desde, janela, borrar, lanzar  # noqa: E402

CAPTURAS = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "docs", "capturas")


# ------------------------------------------------------------ imágenes de prueba (formatos escritos a mano)

def pixeles_prueba(w, h):
    """Un degradado con un círculo y una franja: colores variados y reconocibles."""
    px = []
    for y in range(h):
        fila = []
        for x in range(w):
            r, g, b = x * 255 // (w - 1), y * 255 // (h - 1), 160
            dx, dy = x - w // 2, y - h // 2
            if dx * dx + dy * dy < (min(w, h) // 3) ** 2:
                r, g, b = 250, 200, 20
            if h // 8 < y < h // 8 + 6:
                r, g, b = 20, 20, 200
            fila.append((r, g, b))
        px.append(fila)
    return px


def png_de(px):
    h, w = len(px), len(px[0])
    raw = b"".join(b"\x00" + bytes(c for p in fila for c in p) for fila in px)

    def trozo(t, d):
        return struct.pack(">I", len(d)) + t + d + struct.pack(">I", zlib.crc32(t + d) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)) +
            trozo(b"IDAT", zlib.compress(raw, 9)) + trozo(b"IEND", b""))


def bmp_de(px):
    h, w = len(px), len(px[0])
    fila_n = (w * 3 + 3) // 4 * 4
    datos = b""
    for fila in reversed(px):
        f = b"".join(bytes((b, g, r)) for r, g, b in fila)
        datos += f + b"\x00" * (fila_n - len(f))
    cab = b"BM" + struct.pack("<IHHI", 54 + len(datos), 0, 0, 54)
    dib = struct.pack("<IiiHHIIiiII", 40, w, h, 1, 24, 0, len(datos), 2835, 2835, 0, 0)
    return cab + dib + datos


def gif_de(w, h):
    """GIF de 4 colores (paleta global), LZW real."""
    pal = [(255, 255, 255), (220, 30, 30), (30, 160, 30), (30, 30, 220)]
    idx = [((x // 10 + y // 10) % 4) for y in range(h) for x in range(w)]
    min_code = 2
    clear, fin = 1 << min_code, (1 << min_code) + 1
    tabla = {bytes([i]): i for i in range(clear)}
    sig = fin + 1
    nbits = min_code + 1
    salida = bytearray()
    acc = 0
    nacc = 0

    def emitir(codigo):
        nonlocal acc, nacc
        acc |= codigo << nacc
        nacc += nbits
        while nacc >= 8:
            salida.append(acc & 255)
            acc >>= 8
            nacc -= 8
    emitir(clear)
    cadena = b""
    for v in idx:
        c = cadena + bytes([v])
        if c in tabla:
            cadena = c
        else:
            emitir(tabla[cadena])
            if sig < 4096:
                tabla[c] = sig
                sig += 1
                if sig > (1 << nbits) and nbits < 12:
                    nbits += 1
            else:
                emitir(clear)
                tabla = {bytes([i]): i for i in range(clear)}
                sig = fin + 1
                nbits = min_code + 1
            cadena = bytes([v])
    if cadena:
        emitir(tabla[cadena])
    emitir(fin)
    if nacc:
        salida.append(acc & 255)
    bloques = b""
    for i in range(0, len(salida), 255):
        trozo = bytes(salida[i:i + 255])
        bloques += bytes([len(trozo)]) + trozo
    g = b"GIF89a" + struct.pack("<HHBBB", w, h, 0xF1, 0, 0) + b"".join(bytes(c) for c in pal)
    g += b"\x2c" + struct.pack("<HHHHB", 0, 0, w, h, 0) + bytes([min_code]) + bloques + b"\x00" + b"\x3b"
    pixeles = [[pal[idx[y * w + x]] for x in range(w)] for y in range(h)]
    return g, pixeles


def png_decodificar(d):
    """PNG RGB 8 bits -> (w, h, filas de (r,g,b)). Valida firma, CRC y filtros."""
    assert d[:8] == b"\x89PNG\r\n\x1a\n", "firma PNG"
    i = 8
    idat = b""
    w = h = 0
    while i < len(d):
        n, t = struct.unpack(">I4s", d[i:i + 8])
        cuerpo = d[i + 8:i + 8 + n]
        crc = struct.unpack(">I", d[i + 8 + n:i + 12 + n])[0]
        assert crc == zlib.crc32(t + cuerpo) & 0xFFFFFFFF, "CRC de %r" % t
        if t == b"IHDR":
            w, h, prof, tipo = struct.unpack(">IIBB", cuerpo[:10])
            assert prof == 8 and tipo == 2, "RGB de 8 bits"
        elif t == b"IDAT":
            idat += cuerpo
        i += 12 + n
    raw = zlib.decompress(idat)
    ff = 3 * w + 1
    assert len(raw) == ff * h
    filas = []
    ant = bytes(3 * w)
    for y in range(h):
        f = raw[y * ff]
        s = bytearray(raw[y * ff + 1:(y + 1) * ff])
        for k in range(len(s)):
            a = s[k - 3] if k >= 3 else 0
            b = ant[k]
            c = ant[k - 3] if k >= 3 else 0
            if f == 1:
                s[k] = (s[k] + a) & 255
            elif f == 2:
                s[k] = (s[k] + b) & 255
            elif f == 3:
                s[k] = (s[k] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                s[k] = (s[k] + pr) & 255
        ant = bytes(s)
        filas.append([(s[3 * x], s[3 * x + 1], s[3 * x + 2]) for x in range(w)])
    return w, h, filas


def png_escribir(ruta, filas):
    open(ruta, "wb").write(png_de(filas))


def blobs_de_rtf(rtf):
    """Los \\pict de un RTF: lista de (tipo, bytes, texto del grupo previo)."""
    res = []
    for m in re.finditer(r"\{\\pict((?:\\[a-z]+-?\d*)*)\s*([0-9a-fA-F\s]+)\}", rtf):
        tipo = "png" if "pngblip" in m.group(1) else "jpeg" if "jpegblip" in m.group(1) else "otro"
        res.append((tipo, bytes.fromhex(re.sub(r"\s", "", m.group(2))), m.group(1)))
    return res


def info(m, ruta):
    out = m.comando("pluma --info " + ruta)
    mm = re.search(r"info paginas=(\d+) palabras=(\d+) caracteres=(\d+) paragrafos=(\d+) objetos=(\d+)", out)
    verificar(mm is not None, "pluma --info %s: %s" % (ruta, out.strip().splitlines()[-2:]))
    objs = re.findall(r"objeto tipo=(\d+) l=(\d+) a=(\d+) ajuste=(\d+) borde=(\d+) blob=(\d+)/(\d+) estilo=(\d+) texto='([^']*)' pie='([^']*)'", out)
    return int(mm.group(1)), int(mm.group(5)), objs


def main():
    px_png = pixeles_prueba(240, 160)
    png = png_de(px_png)
    px_bmp = pixeles_prueba(100, 60)
    bmp = bmp_de(px_bmp)
    gif, px_gif = gif_de(80, 50)
    jpg = jpeg_baseline.jpeg_solido(200, 120, 200, 80, 40)

    # RTF como los de Word (con \shppict / \nonshppict) y LibreOffice (\pict directo)
    hexpng = png.hex()
    hexl = "\n".join(hexpng[i:i + 78] for i in range(0, len(hexpng), 78))
    rtf_word = (r"{\rtf1\ansi\deff0{\fonttbl{\f0\fswiss Arial;}}\paperw11906\paperh16838\margl1417\margr1417\margt1417\margb1417" "\n"
                r"\pard\qc{\*\shppict{\pict\picscalex100\picscaley100\piccropl0\piccropr0\piccropt0\piccropb0"
                r"\picw240\pich160\picwgoal3000\pichgoal2000\pngblip{\*\blipuid 0123456789abcdef0123456789abcdef}" "\n" + hexl + "}}"
                r"{\nonshppict{\pict\picscalex100\picscaley100\picw100\pich100\picwgoal100\pichgoal100\wmetafile8 0102030405060708090a}}"
                r"\par" "\n" r"\pard\ql Texto tras la imagen de Word.\par" "\n}").encode("latin-1")
    rtf_lo = (r"{\rtf1\ansi\deff0{\fonttbl{\f0\fswiss Arial;}}" "\n"
              r"\pard {\pict\picscalex50\picscaley50\piccropl0\piccropr0\piccropt0\piccropb0\picw240\pich160\picwgoal3000\pichgoal2000\jpegblip" "\n" +
              jpg.hex() + r"}\par}").encode("latin-1")
    ejemplo_txt = b"Linea uno\n"
    pen = criar_pendrive("h296-pen", rotulo="PLUMA", arquivos={
        "foto.png": png, "foto.bmp": bmp, "foto.gif": gif, "foto.jpg": jpg,
        "word.rtf": rtf_word, "libre.rtf": rtf_lo, "nota.txt": ejemplo_txt})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    def sacar(nome):
        local = os.path.join(SAIDA, "h296-" + nome)
        mtools(pen, "mcopy", "-o", "p:/" + nome, local)
        verificar(os.path.exists(local), "%s salió del pendrive" % nome)
        with open(local, "rb") as f:
            return f.read()

    with Maquina("h296", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "el pendrive está montado")
        out = m.comando("ls /fontes")
        verificar("DejaVuSans.ttf" in out, "las fuentes DejaVu están en /fontes (letras vectoriales del WordArt)")

        if not os.environ.get("H296_SOLO_GUI"):
            # ------------------------------------------------ WordArt: los 20 estilos
            hoja = []
            for e in range(20):
                out = m.comando('pluma --wordart /usb/wa%d.rtf "Abc Pluma" %d 40' % (e, e), 90)
                verificar("rc=0" in out, "WordArt estilo %d guardado: %s" % (e, out.strip().splitlines()[-1:]))
                rtf = sacar("wa%d.rtf" % e).decode("latin-1")
                verificar(r"{\*\saguiwordart %d;" % e in rtf, "estilo %d: el RTF lleva los parámetros para reeditarlo" % e)
                bl = blobs_de_rtf(rtf)
                verificar(len(bl) == 1 and bl[0][0] == "png", "estilo %d: un \\pict con \\pngblip" % e)
                w, h, filas = png_decodificar(bl[0][1])
                planos = [p for f in filas for p in f]
                no_blanco = sum(1 for p in planos if p != (255, 255, 255))
                colores = len(set(planos))
                verificar(w >= 100 and h >= 40, "estilo %d: imagen de %dx%d" % (e, w, h))
                verificar(0.04 < no_blanco / len(planos) < 0.9, "estilo %d: el texto ocupa el %d %% de la imagen" % (e, 100 * no_blanco // len(planos)))
                verificar(colores >= (2 if e == 0 else 12), "estilo %d: %d colores" % (e, colores))
                hoja.append((w, h, filas))
            # hoja de contacto con los 20 estilos (para verlos de un vistazo)
            cw, ch = 360, 190
            lienzo = [[(255, 255, 255)] * (5 * cw) for _ in range(4 * ch)]
            for i, (w, h, filas) in enumerate(hoja):
                ox, oy = (i % 5) * cw + 4, (i // 5) * ch + 4
                for y in range(min(h // 2, ch - 8)):          # reducida a la mitad (promedio 2x2)
                    for x in range(min(w // 2, cw - 8)):
                        p4 = (filas[2 * y][2 * x], filas[2 * y][2 * x + 1], filas[2 * y + 1][2 * x], filas[2 * y + 1][2 * x + 1])
                        lienzo[oy + y][ox + x] = tuple(sum(c[k] for c in p4) // 4 for k in range(3))
            png_escribir(os.path.join(CAPTURAS, "H296-estilos.png"), lienzo)

            # ida y vuelta estable
            out = m.comando("pluma --info /usb/wa5.rtf")
            verificar("objetos=1" in out and "estilo=5 texto='Abc Pluma'" in out, "pluma --info ve el WordArt y su texto: %s" % out.strip().splitlines()[-1:])
            verificar(re.search(r"objeto tipo=2 l=(\d+) a=(\d+)", out) is not None, "el objeto es un WordArt (tipo 2)")
            out = m.comando("pluma --convertir /usb/wa5.rtf /usb/wa5b.rtf", 90)
            verificar("rc=0" in out, "WordArt RTF -> RTF")
            out = m.comando("pluma --convertir /usb/wa5b.rtf /usb/wa5c.rtf", 90)
            a = sacar("wa5.rtf")
            b = sacar("wa5b.rtf")
            c = sacar("wa5c.rtf")
            verificar(a == b == c, "leer y escribir un RTF con WordArt da el mismo archivo (estable, %d bytes)" % len(a))

            # WordArt a PDF
            out = m.comando('pluma --wordart /usb/wa.pdf "Titulo" 9 48', 90)
            verificar("rc=0" in out, "WordArt -> PDF: %s" % out.strip().splitlines()[-1:])
            pdf = pdf_validar.analizar(sacar("wa.pdf"))
            verificar(len(pdf["imagenes"]) == 1, "el PDF del WordArt tiene un XObject de imagen")
            im = pdf["imagenes"][0]
            verificar(im["filtro"] == "FlateDecode" and im["espacio"] == "DeviceRGB" and im["ancho"] >= 100, "imagen RGB con Flate de %dx%d" % (im["ancho"], im["alto"]))
            verificar(b"/Im1 Do" in pdf["contenidos"][0], "la página dibuja la imagen (Do)")
            verificar(len(set(im["datos"][i:i + 3] for i in range(0, len(im["datos"]), 3))) > 12, "la imagen del PDF tiene varios colores")
            verificar("Documento con un titulo WordArt" in " ".join(pdf_validar.texto_de(pdf["contenidos"][0])), "el texto del documento sigue en el PDF")
            ext = pdf_validar.validar_externo(os.path.join(SAIDA, "h296-wa.pdf"))
            if ext:
                for k, (rc, txt) in ext.items():
                    verificar(rc == 0, "%s acepta el PDF con la imagen (%s)" % (k, txt.strip()[:60]))
            out = m.comando("visorpdf -t /usb/wa.pdf", 60)
            verificar("paginas: 1" in out, "el Visor de PDF abre el PDF con el WordArt: %s" % out.strip().splitlines()[:2])

            # ------------------------------------------------ imágenes
            for nombre, datos, tipo in (("foto.png", png, "png"), ("foto.jpg", jpg, "jpeg")):
                out = m.comando("pluma --imagem /usb/%s /usb/i_%s.rtf 8" % (nombre, tipo), 90)
                verificar("rc=0" in out, "%s -> RTF: %s" % (nombre, out.strip().splitlines()[-1:]))
                rtf = sacar("i_%s.rtf" % tipo).decode("latin-1")
                bl = blobs_de_rtf(rtf)
                verificar(len(bl) == 1 and bl[0][0] == tipo, "%s: \\pict con \\%sblip" % (nombre, tipo))
                verificar(bl[0][1] == datos, "%s: se incrusta tal cual (mismos %d bytes, sin recomprimir)" % (nombre, len(datos)))
                verificar(r"\picwgoal4536" in rtf, "%s: ancho de 8 cm = 4536 twips" % nombre)
                verificar(re.search(r"\\pichgoal(\d+)", rtf) is not None, "alto proporcional")
                verificar(r"{\*\saguiobj 2;2;" in rtf, "ajuste «arriba y abajo», borde y pie guardados")
            h_png = int(re.search(r"\\pichgoal(\d+)", sacar("i_png.rtf").decode()).group(1))
            verificar(abs(h_png - 4536 * 160 // 240) <= 2, "el PNG de 240x160 a 8 cm mide %d twips de alto" % h_png)
            for nombre, esperado, tipo in (("foto.bmp", px_bmp, "bmp"), ("foto.gif", px_gif, "gif")):
                out = m.comando("pluma --imagem /usb/%s /usb/i_%s.rtf 5" % (nombre, tipo), 90)
                verificar("rc=0" in out, "%s -> RTF" % nombre)
                bl = blobs_de_rtf(sacar("i_%s.rtf" % tipo).decode("latin-1"))
                verificar(len(bl) == 1 and bl[0][0] == "png", "%s sale como PNG" % nombre)
                w, h, filas = png_decodificar(bl[0][1])
                verificar((w, h) == (len(esperado[0]), len(esperado)) and filas == esperado, "%s -> PNG sin pérdida (%dx%d, píxel a píxel)" % (nombre, w, h))
            # ida y vuelta con imagen
            out = m.comando("pluma --convertir /usb/i_png.rtf /usb/i_png2.rtf", 90)
            verificar("rc=0" in out, "RTF con imagen -> RTF")
            verificar(sacar("i_png2.rtf") == sacar("i_png.rtf"), "la imagen sobrevive a leer y escribir (RTF idéntico)")
            pag, nobj, objs = info(m, "/usb/i_png2.rtf")
            verificar(nobj == 1 and objs[0][3] == "2" and objs[0][4] == "2" and objs[0][9] == "Figura 1: imagen de prueba",
                      "ajuste, borde y pie se conservan: %s" % (objs,))

            # RTF de Word y de LibreOffice
            pag, nobj, objs = info(m, "/usb/word.rtf")
            verificar(nobj == 1 and objs[0][0] == "1" and objs[0][1] == "3000" and objs[0][2] == "2000",
                      "RTF de Word: una imagen PNG de 3000x2000 twips (se salta \\nonshppict con el metafile): %s" % (objs,))
            pag, nobj, objs = info(m, "/usb/libre.rtf")
            verificar(nobj == 1 and objs[0][5] == "1", "RTF de LibreOffice: imagen JPEG (blob jpeg)")
            verificar(objs[0][1] == "1500" and objs[0][2] == "1000", "\\picscalex50 reduce el tamaño a la mitad: %s" % (objs[0][1:3],))
            out = m.comando("pluma --convertir /usb/word.rtf /usb/word2.rtf", 90)
            verificar("rc=0" in out, "Word RTF -> RTF")
            bl = blobs_de_rtf(sacar("word2.rtf").decode("latin-1"))
            verificar(len(bl) == 1 and bl[0][1] == png, "la imagen del RTF de Word se reescribe con los mismos bytes")

            # imágenes en PDF
            out = m.comando("pluma --imagem /usb/foto.jpg /usb/i_jpg.pdf 8", 90)
            verificar("rc=0" in out, "JPEG -> PDF")
            pdf = pdf_validar.analizar(sacar("i_jpg.pdf"))
            verificar(len(pdf["imagenes"]) == 1 and pdf["imagenes"][0]["filtro"] == "DCTDecode", "el JPEG va con DCTDecode")
            verificar(pdf["imagenes"][0]["bruto"] == jpg, "el JPEG del PDF es el original, byte a byte")
            verificar((pdf["imagenes"][0]["ancho"], pdf["imagenes"][0]["alto"]) == (200, 120), "dimensiones 200x120 en el PDF")
            t = " ".join(pdf_validar.texto_de(pdf["contenidos"][0]))
            verificar("Figura 1: imagen de prueba" in t, "el pie de imagen está en el PDF")
            out = m.comando("pluma --imagem /usb/foto.png /usb/i_png.pdf 8", 90)
            verificar("rc=0" in out, "PNG -> PDF")
            pdf = pdf_validar.analizar(sacar("i_png.pdf"))
            im = pdf["imagenes"][0]
            verificar(im["filtro"] == "FlateDecode" and (im["ancho"], im["alto"]) == (240, 160), "el PNG va como RGB Flate de 240x160")
            esperado_rgb = b"".join(bytes(p) for fila in px_png for p in fila)
            verificar(im["datos"] == esperado_rgb, "los píxeles del PDF son los del PNG (sin pérdida)")
            for pdfn in ("i_jpg", "i_png"):
                ext = pdf_validar.validar_externo(os.path.join(SAIDA, "h296-%s.pdf" % pdfn))
                if ext:
                    for k, (rc, txt) in ext.items():
                        verificar(rc == 0, "%s acepta %s.pdf" % (k, pdfn))
        # ------------------------------------------------ con ventana
        m.comando("mkdir /home")
        parte2(m, sacar, png, jpg)
        m.comando("umount /usb")
    print("H296 OK")


def menu_item(m, w, menu_x, indices_y):
    pass


def parte2(m, sacar, png, jpg):
    pos = lanzar(m, "pluma")
    esperar_desde(m, "pluma: pronto", pos, 40)
    time.sleep(1.5)
    w = janela(m, "Documento1 - Pluma", pos)
    m.digitar("Informe con titulo WordArt e imagen", 0.05)
    m.tecla("ret")
    time.sleep(0.5)
    # ---- Insertar > WordArt (barra Dibujo, segundo botón)
    k = len(m.serial())
    m.captura("H296-0-pluma")
    m.clique(w["x"] + 36, w["y"] + w["a"] - 22 - 14)
    time.sleep(2.5)
    g = janela(m, "Insertar WordArt", pos)
    m.captura("H296-1-galeria")
    # elegir el estilo 5 (arco iris curvo): fila 1, columna 0
    m.clique(g["x"] + 14 + 0 * 104 + 40, g["y"] + 28 + 56 * 1 + 20)
    time.sleep(1.0)
    borrar(m, 12)
    m.digitar("Hola Pluma", 0.06)
    time.sleep(1.2)
    m.captura("H296-2-galeria-arco")
    m.tecla("ret")
    esperar_desde(m, "pluma: wordart inserido estilo=5 'Hola Pluma'", k, 40)
    time.sleep(1.0)
    m.captura("H296-3-wordart")
    # ---- Insertar > Imagen por el menú
    m.clique(w["x"] + 300, w["y"] + 420)                 # clic en la hoja: el cursor va al final
    time.sleep(0.5)
    m.tecla("ret")
    time.sleep(0.4)
    k = len(m.serial())
    m.clique(w["x"] + 217, w["y"] + 10)                  # menú Insertar
    time.sleep(0.8)
    m.captura("H296-4-menu-insertar")
    m.clique(w["x"] + 235, w["y"] + 86)                 # «Imagen...»
    esperar_desde(m, "dialogo: abrir en", k, 20)
    time.sleep(0.6)
    m.captura("H296-5-abrir-imagen")
    borrar(m, 30)
    m.digitar("/usb/foto.png\n", 0.05)
    esperar_desde(m, "pluma: imagem inserida /usb/foto.png 240x160", k, 40)
    time.sleep(1.2)
    m.captura("H296-6-imagen")

    def ultimo_objeto(desde):
        g = re.findall(r"objeto sel tipo=(\d+) x=(-?\d+) y=(-?\d+) l=(\d+) a=(\d+) tw=(\d+)x(\d+)", m.serial()[desde:])
        verificar(bool(g), "Pluma anota el objeto seleccionado")
        return [int(v) for v in g[-1]]

    # ---- cambiar el tamaño arrastrando el tirador de la esquina (se conserva la proporción)
    tipo, ox, oy, ol, oa, tw_l, tw_a = ultimo_objeto(k)
    verificar(tipo == 1 and abs(tw_l * 100 // tw_a - 150) <= 2, "imagen de proporción 3:2 (%dx%d twips)" % (tw_l, tw_a))
    k2 = len(m.serial())
    m.arrastar(w["x"] + ox + ol, w["y"] + oy + oa, w["x"] + ox + ol + 60, w["y"] + oy + oa + 5)
    time.sleep(1.0)
    tipo, ox2, oy2, ol2, oa2, tw_l2, tw_a2 = ultimo_objeto(k2)
    verificar(tw_l2 > tw_l + 600, "el tirador agranda la imagen (%d -> %d twips)" % (tw_l, tw_l2))
    verificar(abs(tw_l2 * 100 // tw_a2 - 150) <= 2, "y conserva la proporción 3:2 (%dx%d)" % (tw_l2, tw_a2))
    m.captura("H296-7-tirador")

    # ---- doble clic: Formato de imagen
    k3 = len(m.serial())
    m.clique(w["x"] + ox2 + ol2 // 2, w["y"] + oy2 + oa2 // 2, duplo=True)
    time.sleep(1.5)
    f = janela(m, "Formato de imagen", k3)
    m.captura("H296-8-formato-imagen")
    borrar(m, 12)
    m.digitar("6", 0.06)
    m.tecla("tab")                                       # aplica el ancho (alto proporcional)
    time.sleep(0.4)
    m.clique(f["x"] + 70, f["y"] + 148 + 34)             # ajuste: arriba y abajo
    m.clique(f["x"] + 400, f["y"] + 148 + 34)            # borde: fino
    m.clique(f["x"] + 200, f["y"] + 322 + 10)            # pie de imagen
    m.digitar("Figura 1: gradiente", 0.05)
    time.sleep(0.5)
    m.captura("H296-9-formato-imagen-lleno")
    m.clique(f["x"] + 250 + 48, f["y"] + 380 + 13)       # Aceptar
    esperar_desde(m, "pluma: formato de imagen", k3, 20)
    time.sleep(1.0)
    ult = re.findall(r"formato de imagen (\d+)x(\d+) ajuste=(\d) borde=(\d) pie='([^']*)'", m.serial()[k3:])[-1]
    verificar(abs(int(ult[0]) - 3402) <= 3, "ancho 6 cm = 3402 twips (%s)" % ult[0])
    verificar(abs(int(ult[1]) * 3 - int(ult[0]) * 2) <= 6, "alto proporcional (%s)" % ult[1])
    verificar(ult[2] == "2" and ult[3] == "1" and ult[4] == "Figura 1: gradiente", "ajuste, borde y pie aplicados: %s" % (ult,))
    m.captura("H296-10-imagen-formateada")

    # ---- doble clic en el WordArt: se vuelve a editar
    k4 = len(m.serial())
    m.clique(w["x"] + 115 + 118, w["y"] + 219 + 30, duplo=True)
    time.sleep(1.5)
    janela(m, "Editar WordArt", k4)
    m.captura("H296-11-editar-wordart")
    m.tecla("esc")
    time.sleep(0.8)

    # ---- guardar, exportar a PDF
    k5 = len(m.serial())
    m.tecla("ctrl-s")
    esperar_desde(m, "dialogo: guardar en", k5, 20)
    time.sleep(0.4)
    borrar(m, 30)
    m.digitar("/usb/gui296\n", 0.05)
    esperar_desde(m, "pluma: guardado /usb/gui296.rtf", k5, 30)
    time.sleep(1.0)
    rtf = sacar("gui296.rtf").decode("latin-1")
    bl = blobs_de_rtf(rtf)
    verificar(len(bl) == 2 and [b[0] for b in bl] == ["png", "png"], "el RTF guardado lleva dos imágenes PNG (WordArt e imagen)")
    verificar(bl[1][1] == png, "la imagen insertada va con los bytes del PNG original")
    verificar("\\picwgoal3402" in rtf, "ancho de 6 cm en el RTF")
    verificar("{\\*\\saguiwordart 5;" in rtf and "{\\*\\saguiobj 2;1;" in rtf, "parámetros del WordArt, ajuste y borde en el RTF")
    k6 = len(m.serial())
    m.tecla("ctrl-p")
    esperar_desde(m, "dialogo: guardar en", k6, 20)
    time.sleep(0.4)
    borrar(m, 30)
    m.digitar("/usb/gui296\n", 0.05)
    esperar_desde(m, "pluma: pdf exportado /usb/gui296.pdf", k6, 40)
    time.sleep(1.0)
    m.tecla("ret")
    time.sleep(0.6)
    pdf = pdf_validar.analizar(sacar("gui296.pdf"))
    verificar(len(pdf["imagenes"]) == 2, "el PDF lleva dos XObject de imagen")
    t = " ".join(pdf_validar.texto_de(pdf["contenidos"][0]))
    verificar("Informe con titulo WordArt e imagen" in t and "Figura 1: gradiente" in t, "texto y pie de imagen en el PDF")
    m.captura("H296-12-documento")
    # ---- cerrar y reabrir
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    esperar_desde(m, "pluma: fechado", pos, 20)
    time.sleep(0.8)
    k7 = len(m.serial())
    pos = lanzar(m, "pluma /usb/gui296.rtf")
    esperar_desde(m, "pluma: aberto /usb/gui296.rtf", pos, 40)
    esperar_desde(m, "pluma: pronto", pos, 40)
    time.sleep(2.0)
    m.captura("H296-13-reabierto")
    w = janela(m, "Documento1 - Pluma", pos)
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    esperar_desde(m, "pluma: fechado", pos, 20)
    time.sleep(0.8)
    out = m.comando("pluma --info /usb/gui296.rtf")
    verificar("objetos=2" in out, "el documento reabierto tiene los dos objetos: %s" % out.strip().splitlines()[-3:])
    verificar("estilo=5 texto='Hola Pluma'" in out and "pie='Figura 1: gradiente'" in out, "WordArt reeditable e imagen con su pie tras reabrir")


if __name__ == "__main__":
    main()
