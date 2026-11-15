#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H319 (D319) - Pluma lee las fuentes extra instaladas en el sistema.

Fuentes: las DejaVu de /fontes (ramdisk) y, si la imagen las trae, las Typodermic de /apps/fuentes. La
fuente "extra" de la prueba es una DejaVu Serif con el nombre de familia cambiado a «Sagui Prueba» (mismo
largo, en la tabla name) que se copia a /home/fuentes con Pluma ya en marcha.

Parte 1, sin ventana (pluma --fuentes, --medir, --convertir, --info):
  - la lista de familias (clásicas, DejaVu, la nueva tras copiarla);
  - anchos reales: DejaVu Sans no mide como Arial, la monoespaciada mide igual para i que para W;
  - RTF -> PDF: las fuentes del sistema van incrustadas (FontFile2 con subconjunto, WinAnsi, Widths), validadas
    con pdf_validar y FreeType (fc-scan); las tres clásicas siguen siendo Helvetica/Times/Courier sin incrustar;
  - RTF -> RTF: \\fonttbl con el nombre real de la familia; y una fuente que no existe se sustituye y se avisa.
Parte 2, con ventana: el combo y el diálogo Fuente listan las familias (cada nombre con su fuente), escribir con
una fuente nueva cambia el dibujo, «Actualizar lista» encuentra la fuente copiada con Pluma abierto, guardar y
reabrir el RTF conserva el nombre, y al quitar la fuente el RTF se abre con aviso en la barra de estado.
Capturas docs/capturas/H319-*.png.
"""
import os
import re
import sys
import tempfile
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, mtools, SAIDA  # noqa: E402
import pdf_validar  # noqa: E402
from h291_pluma import esperar_desde, janela, borrar, lanzar  # noqa: E402
from h162_fuentes import region  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NOMBRE_EXTRA = "Sagui Prueba"


def fuente_extra():
    """DejaVu Serif con el nombre de familia cambiado a «Sagui Prueba» (mismo largo, en ASCII y UTF-16BE)."""
    d = open(os.path.join(RAIZ, "rootfs", "fontes", "DejaVuSerif.ttf"), "rb").read()
    viejo, nuevo = "DejaVu Serif", NOMBRE_EXTRA
    assert len(viejo) == len(nuevo)
    n1 = d.count(viejo.encode("ascii")) + d.count(viejo.encode("utf-16-be"))
    assert n1 >= 1, "la tabla name de DejaVu Serif no trae el nombre esperado"
    for a, b in ((viejo, nuevo), ("DejaVuSerif", "SaguiPrueba"), ("DejaVu-Serif", "Sagui-Prueba")):
        d = d.replace(a.encode("ascii"), b.encode("ascii")).replace(a.encode("utf-16-be"), b.encode("utf-16-be"))
    return d


def rtf_fuentes():
    r = (r"{\rtf1\ansi\ansicpg1252\deff0\deflang1034{\fonttbl{\f0\fswiss\fcharset0 Arial;}"
         r"{\f1\froman\fcharset0 Times New Roman;}{\f2\fmodern\fcharset0 Courier New;}"
         r"{\f3\froman\fcharset0 DejaVu Serif;}{\f4\froman\fcharset0 " + NOMBRE_EXTRA + r";}"
         r"{\f5\fswiss\fcharset0 Fuente Fantasma;}{\f6\fmodern\fcharset0 DejaVu Sans Mono;}}"
         r"\paperw11906\paperh16838\margl1417\margr1417\margt1417\margb1417" "\n"
         r"\pard{\f0\fs24 Clasica Arial: Hola mundo.}\par" "\n"
         r"\pard{\f3\fs24 Serif del sistema: ni\'f1o y \'e1rbol \'c1gil.}\par" "\n"
         r"\pard{\f4\fs28\b Extra en negrita: Hola mundo.}\par" "\n"
         r"\pard{\f4\fs28\i Extra en cursiva: Hola mundo.}\par" "\n"
         r"\pard{\f6\fs20 Monoespaciada: iiii WWWW.}\par" "\n"
         r"\pard{\f5\fs24 Texto en una fuente que no existe.}\par" "\n}")
    return r.encode("latin-1")


def main():
    extra = fuente_extra()
    rtf = rtf_fuentes()
    pen = criar_pendrive("h319-pen", rotulo="FUENTES", arquivos={"prueba.rtf": rtf, "SaguiPrueba.ttf": extra})
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    tmp = tempfile.mkdtemp(prefix="h319-")

    def sacar(nome):
        local = os.path.join(SAIDA, "h319-" + nome)
        mtools(pen, "mcopy", "-o", "p:/" + nome, local)
        verificar(os.path.exists(local), "%s salió del pendrive" % nome)
        return open(local, "rb").read()

    with Maquina("h319", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        verificar("Pendrive montado en /usb" in m.comando("montar usb0"), "el pendrive está montado")
        m.comando("mkdir /home")
        parte1(m, sacar, tmp)
        parte2(m, sacar, tmp)
        m.comando("umount /usb")
    print("H319 OK")


def parte1(m, sacar, tmp):
    # ---- la lista de familias
    out = m.comando("pluma --fuentes")
    fam = dict((int(i), n.strip()) for i, n in re.findall(r"pluma: familia (\d+) (.+)", out))
    verificar(fam.get(0) == "Arial" and fam.get(1) == "Times New Roman" and fam.get(2) == "Courier New", "las tres clásicas van al principio: %s" % fam)
    nombres = list(fam.values())
    for n in ("DejaVu Sans", "DejaVu Serif", "DejaVu Sans Mono"):
        verificar(n in nombres, "/fontes aporta la familia %s" % n)
    verificar(NOMBRE_EXTRA not in nombres, "«%s» todavía no está instalada" % NOMBRE_EXTRA)

    # ---- anchos reales
    def ancho(f, est, pt, t):
        o = m.comando('pluma --medir "%s" %d %d "%s"' % (f, est, pt, t))
        g = re.search(r"= (\d+) twips", o)
        verificar(g is not None, "pluma --medir %s: %s" % (f, o.strip().splitlines()[-1:]))
        return int(g.group(1))
    wa, wd = ancho("Arial", 0, 12, "Hola mundo"), ancho("DejaVu Sans", 0, 12, "Hola mundo")
    verificar(wd > wa, "DejaVu Sans mide más que Helvetica (%d > %d twips): usa las métricas reales" % (wd, wa))
    wi, wW = ancho("DejaVu Sans Mono", 0, 12, "iiii"), ancho("DejaVu Sans Mono", 0, 12, "WWWW")
    verificar(wi == wW, "la monoespaciada mide igual iiii que WWWW (%d)" % wi)
    verificar(ancho("DejaVu Sans", 1, 12, "Hola mundo") > wd, "la negrita real de DejaVu Sans es más ancha que la normal")

    # ---- instalar la fuente extra en /home/fuentes y volver a listar
    m.comando("mkdir /home/fuentes")
    out = m.comando("cp /usb/SaguiPrueba.ttf /home/fuentes/SaguiPrueba.ttf")
    out = m.comando("pluma --fuentes")
    verificar(NOMBRE_EXTRA in out, "tras copiarla a /home/fuentes, Pluma encuentra «%s»" % NOMBRE_EXTRA)
    verificar(ancho(NOMBRE_EXTRA, 0, 12, "Hola mundo") == ancho("DejaVu Serif", 0, 12, "Hola mundo"), "la fuente extra mide como la DejaVu Serif de la que sale")

    # ---- RTF -> PDF con las fuentes incrustadas
    out = m.comando("pluma --convertir /usb/prueba.rtf /usb/fuentes.pdf", 120)
    verificar("rc=0" in out, "RTF -> PDF: %s" % out.strip().splitlines()[-1:])
    pdf = sacar("fuentes.pdf")
    info = pdf_validar.analizar(pdf)
    inc = info["incrustadas"]
    verificar(len(inc) >= 4, "el PDF incrusta las fuentes del sistema usadas (%d: %s)" % (len(inc), [f["base"] for f in inc]))
    bases = " ".join(f["base"] for f in inc)
    verificar("DejaVuSerif" in bases and "SaguiPrueba" in bases and "DejaVuSansMono" in bases, "nombres PostScript de las incrustadas: %s" % bases)
    verificar(all(f["tipo_archivo"] == "FontFile2" and f["subtipo"] == "TrueType" for f in inc), "TrueType con FontFile2")
    for f in inc:
        cs = pdf_validar.validar_fuente_incrustada(f, tmp)
        verificar(len(f["datos"]) < 60000, "%s va como subconjunto (%d bytes)" % (f["base"], len(f["datos"])))
        if cs is not None:
            verificar("6e" in cs.replace(" ", "") or "20" in cs, "FreeType abre %s y su charset es %s" % (f["base"], cs[:40]))
    mono = [f for f in inc if "Mono" in f["base"]][0]
    verificar(mono["anchos"][ord("i") - 32] == mono["anchos"][ord("W") - 32] > 500, "Widths de la monoespaciada: i = W = %d" % mono["anchos"][ord("i") - 32])
    serif = [f for f in inc if f["base"].endswith("DejaVuSerif")][0]
    verificar(serif["anchos"][ord("a") - 32] > 0 and serif["anchos"][ord("\xf1") - 32] > 0, "Widths cubren a y ñ")
    t1 = " ".join(pdf_validar.texto_de(info["contenidos"][0]))
    verificar("Clasica Arial: Hola mundo." in t1 and "ni\xf1o y \xe1rbol \xc1gil" in t1, "el texto (con acentos Latin-1) está en el PDF")
    verificar(b"Helvetica" in b" ".join(info["fuentes"]), "las clásicas siguen siendo Helvetica estándar: %s" % info["fuentes"])
    verificar(not any(b"Arial" in f for f in info["fuentes"]), "Arial no se incrusta")
    ext = pdf_validar.validar_externo(os.path.join(SAIDA, "h319-fuentes.pdf"))
    if ext:
        verificar(all(rc == 0 for rc, _ in ext.values()), "qpdf/pdftotext aceptan el PDF: %s" % ext)
    else:
        print("  (el anfitrión no tiene qpdf ni pdftotext: solo pdf_validar y fc-scan)")
    # la cursiva de la fuente extra (sin cursiva real) se inclina: matriz de texto con 0.2126
    verificar(b"1 0 0.2126 1" in info["contenidos"][0], "la cursiva sintética de «%s» se inclina en el PDF" % NOMBRE_EXTRA)

    # ---- RTF -> RTF: nombre real de familia y sustitución
    out = m.comando("pluma --convertir /usb/prueba.rtf /usb/sal.rtf", 60)
    verificar("rc=0" in out, "RTF -> RTF")
    sal = sacar("sal.rtf").decode("latin-1")
    tabla = re.search(r"\{\\fonttbl(.*?)\}\n", sal, re.S).group(1)
    verificar("DejaVu Serif" in tabla and NOMBRE_EXTRA in tabla and "DejaVu Sans Mono" in tabla, "\\fonttbl lleva los nombres reales: %s" % tabla)
    verificar("Fuente Fantasma" not in sal, "la fuente inexistente no se escribe: se sustituyó por una clásica")
    m.comando("rm /home/fuentes/SaguiPrueba.ttf")
    out = m.comando("pluma --info /usb/prueba.rtf")
    verificar("fuentes sustituidas 2: Fuente Fantasma, %s" % NOMBRE_EXTRA in out or ("fuentes sustituidas" in out and "Fuente Fantasma" in out and NOMBRE_EXTRA in out),
              "al abrir sin la fuente extra se sustituye y se avisa: %s" % out.strip().splitlines()[-2:])
    out = m.comando("pluma --convertir /usb/prueba.rtf /usb/sin.pdf", 120)
    verificar("rc=0" in out, "el PDF sin la fuente extra se genera (sustituida)")
    inc2 = pdf_validar.analizar(sacar("sin.pdf"))["incrustadas"]
    verificar(not any("SaguiPrueba" in f["base"] for f in inc2), "sin la fuente no se incrusta")
    m.comando("cp /usb/SaguiPrueba.ttf /home/fuentes/SaguiPrueba.ttf")

    # ---- rendimiento: una tecla en un documento de 15 páginas (maquetar todo + redibujar la vista)
    medidas = {}
    for f in ("8x16", "Arial", "DejaVu Sans", NOMBRE_EXTRA):
        out = m.comando('pluma --bench "%s" 15' % f, 180)
        g = re.search(r"bench fam=.* paginas=(\d+) maquetar=([\d.]+) ms/tecla tecla_completa=([\d.]+) ms/tecla", out)
        verificar(g is not None, "pluma --bench %s: %s" % (f, out.strip().splitlines()[-2:]))
        medidas[f] = (int(g.group(1)), float(g.group(2)), float(g.group(3)))
        print("  bench %-14s %2d págs: maquetar %.2f ms, tecla completa %.2f ms" % ((f,) + medidas[f]))
    verificar(medidas["DejaVu Sans"][0] >= 15, "el documento de la medida tiene varias páginas (%d)" % medidas["DejaVu Sans"][0])
    base = medidas["8x16"][2]
    for f in ("Arial", "DejaVu Sans", NOMBRE_EXTRA):
        verificar(medidas[f][2] < 60 and medidas[f][2] < max(base, 1.0) * 4,
                  "con la fuente real (%s) una tecla cuesta %.2f ms contra %.2f ms con la 8x16" % (f, medidas[f][2], base))


# ------------------------------------------------------------ con ventana

def cerrar(m, pos):
    w = janela(m, "Documento1 - Pluma", pos)
    m.clique(w["x"] + w["l"] + 4 - 2 - 20 + 8 - 2, w["y"] - 12)
    esperar_desde(m, "pluma: fechado", pos, 20)
    time.sleep(0.8)


def bandas(m, w, nombre):
    """Capturas la pantalla y devuelve las líneas de tinta de la hoja: lista de (y, x_min, x_max)."""
    ruta = m.captura(nombre)
    x0, y0 = w["x"] + 60, w["y"] + 51 + 21 + 20 + 20
    filas = region(ruta, x0, y0, w["l"] - 120, w["a"] - (y0 - w["y"]) - 60)
    res, cur = [], None
    for dy, f in enumerate(filas):
        xs = [x for x, v in enumerate(f) if v < 90]
        if xs:
            if cur is None:
                cur = [dy, min(xs), max(xs)]
            else:
                cur[1], cur[2] = min(cur[1], min(xs)), max(cur[2], max(xs))
        elif cur is not None:
            res.append(tuple(cur))
            cur = None
    return res


def elegir_familia(m, w_pluma, fila, esperado):
    """Abre el selector de fuentes del combo, hace clic en la fila y acepta; comprueba que se aplicó."""
    k = len(m.serial())
    m.clique(w_pluma["x"] + 100, w_pluma["y"] + 51 + 10)
    esperar_desde(m, "dialogo: fuentes", k, 20)
    time.sleep(0.8)
    m.digitar(esperado, 0.06)       # D327: el combo tiene buscador
    time.sleep(0.5)
    m.tecla("ret")
    esperar_desde(m, "pluma: fuente aplicada %s" % esperado, k, 20)
    time.sleep(0.5)


def parte2(m, sacar, tmp):
    texto = "Hola mundo ABC iiii WWWW"
    out = m.comando('pluma --medir "DejaVu Sans Mono" 0 12 "%s"' % texto)
    wm = int(re.search(r"= (\d+) twips", out).group(1))
    out = m.comando('pluma --medir "%s" 0 12 "%s"' % (NOMBRE_EXTRA, texto))
    we = int(re.search(r"= (\d+) twips", out).group(1))
    out = m.comando('pluma --medir "Times New Roman" 0 12 "%s"' % texto)
    wt = int(re.search(r"= (\d+) twips", out).group(1))
    m.comando("rm /home/fuentes/SaguiPrueba.ttf")
    k0 = len(m.serial())
    m.comando("sleep 55 && cp /usb/SaguiPrueba.ttf /home/fuentes/SaguiPrueba.ttf && echo H319-copiada &")
    # ---- un RTF con fuentes que faltan: sustitución con aviso en la barra de estado
    pos = lanzar(m, "pluma /usb/prueba.rtf")
    esperar_desde(m, "pluma: pronto", pos, 40)
    time.sleep(1.5)
    verificar("pluma: aviso Fuente no instalada, sustituida: " in m.serial()[pos:] and "Fuente Fantasma" in m.serial()[pos:], "al abrir el RTF se avisa de las fuentes sustituidas")
    w = janela(m, "Documento1 - Pluma", pos)
    m.captura("H319-1-aviso-sustituida")

    # ---- el selector del combo, con la fuente nueva aún sin copiar
    k = len(m.serial())
    m.clique(w["x"] + 100, w["y"] + 51 + 10)
    esperar_desde(m, "dialogo: fuentes", k, 20)
    time.sleep(0.8)
    n1 = int(re.findall(r"dialogo: fuentes (\d+) familias", m.serial()[k:])[-1])
    verificar(n1 >= 6, "el selector lista las clásicas y las del sistema (%d familias)" % n1)
    m.captura("H319-2-selector")
    m.tecla("esc")
    time.sleep(0.6)
    # ---- la fuente se copia a /home/fuentes con Pluma abierto: el combo vuelve a mirar al abrirse (D327)
    fin = time.time() + 100
    while not re.search(r"(^|\n)H319-copiada", m.serial()[k0:].replace("\r", "")) and time.time() < fin:
        time.sleep(0.5)
    verificar(re.search(r"(^|\n)H319-copiada", m.serial()[k0:].replace("\r", "")) is not None, "la fuente se copió a /home/fuentes con Pluma abierto")
    time.sleep(1.0)
    k = len(m.serial())
    m.clique(w["x"] + 100, w["y"] + 51 + 10)
    esperar_desde(m, "dialogo: fuentes", k, 20)
    time.sleep(0.8)
    n2 = int(re.findall(r"dialogo: fuentes (\d+) familias", m.serial()[k:])[-1])
    m.captura("H319-3-selector-actualizado")
    verificar(n2 == n1 + 1, "el combo encuentra la fuente copiada con Pluma abierto (%d -> %d)" % (n1, n2))
    m.tecla("esc")
    time.sleep(0.6)
    cerrar(m, pos)

    # ---- escribir con fuentes distintas: el dibujo y el maquetado cambian
    pos = lanzar(m, "pluma")
    esperar_desde(m, "pluma: pronto", pos, 40)
    time.sleep(1.5)
    w = janela(m, "Documento1 - Pluma", pos)
    m.digitar(texto, 0.06)
    m.tecla("ret")
    elegir_familia(m, w, 4, "DejaVu Sans Mono")
    m.digitar(texto, 0.06)
    m.tecla("ret")
    elegir_familia(m, w, n2 - 1, NOMBRE_EXTRA)
    m.digitar(texto, 0.06)
    m.tecla("ret")
    time.sleep(0.6)
    b = bandas(m, w, "H319-4-escrito")
    verificar(len(b) >= 3, "tres líneas de tinta (Times clásica, monoespaciada y la extra): %s" % (b,))
    ancho = [x1 - x0 for _, x0, x1 in b[:3]]
    # la monoespaciada mide más que la serif; las métricas de la pantalla siguen las medidas reales (--medir)
    verificar(ancho[1] != ancho[0] and ancho[2] != ancho[1], "cada fuente dibuja y maqueta distinto (anchos en píxeles %s)" % ancho)
    for nom, i, esp in (("extra/mono", 2, we / wm), ("mono/times", 1, wm / wt)):
        obs = ancho[i] / ancho[i - 1 if i == 2 else 0]
        verificar(abs(obs - esp) < 0.06, "el ancho en pantalla sigue las métricas reales: %s %.3f (esperado %.3f)" % (nom, obs, esp))
    # antialias: la línea con la fuente vectorial tiene grises intermedios
    ruta = os.path.join(RAIZ, "docs", "capturas", "H319-4-escrito.png")
    y0 = w["y"] + 51 + 21 + 20 + 20
    filas = region(ruta, w["x"] + 60, y0 + b[2][0], w["l"] - 120, 24)
    grises = len(set(v for f in filas for v in f if 40 < v < 215))
    verificar(grises >= 12, "la fuente vectorial se dibuja con antialias (%d niveles de gris)" % grises)

    # ---- guardar como RTF y exportar a PDF desde la ventana
    k = len(m.serial())
    m.tecla("ctrl-s")
    esperar_desde(m, "dialogo: guardar en", k, 20)
    time.sleep(0.6)
    borrar(m)
    m.digitar("/usb/gui319\n", 0.05)
    esperar_desde(m, "pluma: guardado /usb/gui319.rtf", k, 20)
    time.sleep(0.8)
    k = len(m.serial())
    m.tecla("ctrl-p")
    esperar_desde(m, "dialogo: guardar en", k, 20)
    time.sleep(0.4)
    borrar(m)
    m.digitar("/usb/gui319\n", 0.05)
    esperar_desde(m, "pluma: pdf exportado /usb/gui319.pdf", k, 30)
    time.sleep(1.0)
    m.tecla("ret")
    time.sleep(0.6)
    rtf = sacar("gui319.rtf").decode("latin-1")
    tabla = re.search(r"\{\\fonttbl(.*?)\}\n", rtf, re.S).group(1)
    verificar(NOMBRE_EXTRA in tabla and "DejaVu Sans Mono" in tabla, "el RTF guardado desde la ventana lleva los nombres reales: %s" % tabla)
    info = pdf_validar.analizar(sacar("gui319.pdf"))
    bases = " ".join(f["base"] for f in info["incrustadas"])
    verificar("SaguiPrueba" in bases and "DejaVuSansMono" in bases, "el PDF de la ventana incrusta las dos fuentes: %s" % bases)
    for f in info["incrustadas"]:
        pdf_validar.validar_fuente_incrustada(f, tmp)
    cerrar(m, pos)

    # ---- reabrir con la fuente: sin aviso; quitarla y reabrir: sustituida con aviso
    pos = lanzar(m, "pluma /usb/gui319.rtf")
    esperar_desde(m, "pluma: pronto", pos, 40)
    time.sleep(1.5)
    verificar("pluma: aviso" not in m.serial()[pos:], "con la fuente instalada el RTF se abre sin aviso")
    b2 = bandas(m, janela(m, "Documento1 - Pluma", pos), "H319-5-reabierto")
    a2 = [x1 - x0 for _, x0, x1 in b2[:3]]
    verificar(len(a2) == 3 and all(abs(p - q) <= 2 for p, q in zip(a2, ancho)), "el RTF reabierto se dibuja igual (%s contra %s)" % (a2, ancho))
    cerrar(m, pos)
    m.comando("rm /home/fuentes/SaguiPrueba.ttf")
    pos = lanzar(m, "pluma /usb/gui319.rtf")
    esperar_desde(m, "pluma: pronto", pos, 40)
    time.sleep(1.5)
    verificar("pluma: aviso Fuente no instalada, sustituida: %s" % NOMBRE_EXTRA in m.serial()[pos:], "sin la fuente, el RTF se abre sustituida y con aviso")
    m.captura("H319-6-sustituida")
    cerrar(m, pos)
    m.comando("cp /usb/SaguiPrueba.ttf /home/fuentes/SaguiPrueba.ttf")


if __name__ == "__main__":
    main()
