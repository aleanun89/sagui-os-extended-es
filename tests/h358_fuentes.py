#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H358 (D356) - fuentes extra en Pluma y Macaco, e instalador de fuentes («fuentes»).

Parte 1, consola:
  - las fuentes extra de la imagen (Typodermic en /apps/fuentes, si el make encontró el zip) salen en
    `fuentes`, `pluma --fuentes` y `macaco --fuentes` (antes Macaco no ofrecía ninguna);
  - `fuentes instalar` rechaza lo dañado (truncado, basura, vacío, texto con extensión .ttf) sin copiar nada;
  - instala una TrueType buena en /home/fuentes y aparece en Pluma y Macaco; `fuentes quitar` la retira de las dos;
  - con root, `--sistema` la pone en /apps/fuentes (SAGUI-APPS: persistente) y se quita igual;
  - XLSX y ODS de Macaco guardan la fuente por su nombre y la reconocen al abrir.
Parte 2, ventanas:
  - Macaco dibuja una celda con la fuente instalada con los mismos píxeles que la DejaVu Serif de la que sale, y
    distinto de la 8x16 (comparación de píxeles); su combo de fuentes lista las del sistema;
  - Pluma, igual (la línea con la fuente extra coincide con la de DejaVu Serif);
  - `fuentes --ventana`: lista con muestra; con un archivo, la vista previa con «Instalar» (lo que abre Archivos);
    un archivo dañado muestra el rechazo y no se copia; Instalar lo copia y la lista de Pluma/Macaco lo ve.
Capturas docs/capturas/H358-*.png.
"""
import io
import os
import re
import sys
import time
import zipfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, criar_pendrive, pendrive_qemu, mtools, SAIDA  # noqa: E402
from h291_pluma import esperar_desde, lanzar, janela as janela_pluma  # noqa: E402
from h162_fuentes import region  # noqa: E402
import h319_pluma_fuentes as h319  # noqa: E402

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXTRA = h319.NOMBRE_EXTRA          # «Sagui Prueba»: DejaVu Serif con otro nombre
GRID_X, GRID_Y, ROW_H = 40, 118, 20
TEXTO = "Hola Mundo gQy 123"

MXS = """MXS 1
hoja "Hoja1" 6 3
larguras 300,100,100
celda A1 "%(t)s"
celda A2 "%(t)s"
celda A3 "%(t)s"
celda A4 "%(t)s"
formato A1 nf=0 dec=0 mo=0 al=0 est=0 fam=0 tam=14 bor=0 cor=0 rel=0 aj=0 fn=Sagui_Prueba
formato A2 nf=0 dec=0 mo=0 al=0 est=0 fam=0 tam=14 bor=0 cor=0 rel=0 aj=0 fn=DejaVu_Serif
formato A3 nf=0 dec=0 mo=0 al=0 est=0 fam=0 tam=14 bor=0 cor=0 rel=0 aj=0
formato A4 nf=0 dec=0 mo=0 al=0 est=0 fam=0 tam=14 bor=0 cor=0 rel=0 aj=0 fn=Xirod
""" % {"t": TEXTO}


def janela(serial, titulo):
    g = re.findall(rf'criada por \d+: "{re.escape(titulo)}[^"]*" (\d+)x(\d+) em \((-?\d+),(-?\d+)\)', serial)
    verificar(bool(g), "ventana «%s» abierta" % titulo)
    g = g[-1]
    return dict(l=int(g[0]), a=int(g[1]), x=int(g[2]) + 2, y=int(g[3]) + 24)


def familias(out, clave):
    return dict((int(i), n.strip()) for i, n in re.findall(clave + r": familia (\d+) (.+)", out))


def listar(m, prog):
    out = m.comando(prog + " --fuentes", 60)
    if prog == "macaco":    # Macaco escribe en el registro (puerto serie)
        out = m.serial()[-6000:]
    return familias(out, prog)


def fuentes_ok(m, cmd, esperado=0):
    out = m.comando(cmd + "; echo RC=$?", 60)
    g = re.search(r"RC=(\d+)", out)
    return out, int(g.group(1)) if g else -1


def main():
    extra = h319.fuente_extra()
    dv = open(os.path.join(RAIZ, "rootfs", "fontes", "DejaVuSans.ttf"), "rb").read()
    azar = bytes((i * 131 + 7) & 255 for i in range(6000))
    arq = {"SaguiPrueba.ttf": extra, "danada.ttf": dv[:3000], "basura.otf": azar, "vacia.ttf": b"",
           "falsa.ttf": b"esto no es una fuente, solo un texto con extension .ttf " * 20,
           "f.mxs": MXS.encode("utf-8")}
    pen = criar_pendrive("h358-pen", rotulo="FUENTES", arquivos=arq)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)
    with Maquina("h358", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        verificar("Pendrive montado en /usb" in m.comando("montar usb0"), "el pendrive está montado")
        parte1(m, pen)
        parte2(m)
    print("H358 OK")


def parte1(m, pen):
    # ---- las extra de la imagen salen en los tres sitios
    out = m.comando("fuentes", 60)
    verificar("DejaVu Sans" in out, "`fuentes` lista las de la base (DejaVu)")
    typo = "Xirod" in out
    pl = familias(m.comando("pluma --fuentes", 60), "pluma")
    mc = listar(m, "macaco")
    verificar("DejaVu Serif" in pl.values() and "DejaVu Serif" in mc.values(), "Pluma y Macaco ofrecen las de /fontes")
    if typo:
        for n in ("Xirod", "Zorque", "Unispace"):
            verificar(n in pl.values() and n in mc.values(), "la extra «%s» sale en Pluma y en Macaco" % n)
        verificar(len(pl) == len(mc), "Pluma y Macaco ofrecen las mismas familias (%d y %d)" % (len(pl), len(mc)))
    else:
        print("  (la imagen no trae Typodermic: sin NEW_FONTS; se prueban solo las instaladas)")
    base = len(pl)

    # ---- lo dañado se rechaza sin copiar nada
    m.comando("rm /home/fuentes/danada.ttf")
    for nombre in ("danada.ttf", "basura.otf", "vacia.ttf", "falsa.ttf"):
        out, rc = fuentes_ok(m, "fuentes instalar /usb/" + nombre)
        verificar(rc != 0 and ("fuente válida" in out or "demasiado" in out or "no es una" in out), "«%s» se rechaza: %s" % (nombre, out.strip().splitlines()[-2:]))
    out = m.comando("ls /home/fuentes")
    verificar("danada" not in out and "basura" not in out and "falsa" not in out, "nada de lo dañado se copió a /home/fuentes")
    out, rc = fuentes_ok(m, "fuentes instalar /usb/f.mxs")
    verificar(rc != 0 and ".ttf" in out, "un archivo que no es .ttf/.otf se rechaza")
    verificar(len(familias(m.comando("pluma --fuentes", 60), "pluma")) == base, "tras los rechazos la lista no cambia")

    # ---- instalar la buena para la cuenta
    out, rc = fuentes_ok(m, "fuentes instalar /usb/SaguiPrueba.ttf")
    verificar(rc == 0 and ("«%s»" % EXTRA) in out, "se instala «%s»: %s" % (EXTRA, out.strip().splitlines()[-2:]))
    verificar("SaguiPrueba.ttf" in m.comando("ls /home/fuentes"), "está en /home/fuentes")
    pl = familias(m.comando("pluma --fuentes", 60), "pluma")
    mc = listar(m, "macaco")
    verificar(EXTRA in pl.values() and EXTRA in mc.values() and len(pl) == base + 1, "«%s» sale en Pluma y en Macaco" % EXTRA)
    verificar(EXTRA in m.comando("fuentes") and "esta cuenta" in m.comando("fuentes"), "`fuentes` la lista como de esta cuenta")

    # ---- XLSX y ODS guardan la fuente por su nombre y la reconocen al abrir
    out = m.comando("macaco --convertir /usb/f.mxs /usb/f.xlsx", 90)
    verificar("bien" in out, "Macaco exporta a XLSX: %s" % out.strip().splitlines()[-1:])
    out = m.comando("macaco --convertir /usb/f.mxs /usb/f.ods", 90)
    verificar("bien" in out, "Macaco exporta a ODS")
    out = m.comando("macaco --convertir /usb/f.xlsx /tmp/b.mxs", 90)
    out = re.sub(r"\[SAGUI\][^\n]*\n", "", m.comando("cat /tmp/b.mxs"))
    verificar("fn=Sagui_Prueba" in out and "fn=DejaVu_Serif" in out and "fn=Xirod" in out or not "Xirod" in "".join(pl.values()),
              "XLSX -> mxs: las fuentes vuelven por su nombre")
    m.comando("macaco --convertir /usb/f.ods /tmp/c.mxs", 90)
    out = re.sub(r"\[SAGUI\][^\n]*\n", "", m.comando("cat /tmp/c.mxs"))
    verificar("fn=Sagui_Prueba" in out, "ODS -> mxs: la fuente vuelve por su nombre")
    m.comando("sync")
    d = os.path.join(SAIDA, "h358-f.xlsx")
    mtools(pen, "mcopy", "-o", "p:/f.xlsx", d)
    z = zipfile.ZipFile(d)
    st = z.read("xl/styles.xml")
    verificar(b'<name val="Sagui Prueba"/>' in st and b'<name val="DejaVu Serif"/>' in st, "styles.xml del XLSX lleva los nombres reales")
    d = os.path.join(SAIDA, "h358-f.ods")
    mtools(pen, "mcopy", "-o", "p:/f.ods", d)
    verificar(b'style:font-name="Sagui Prueba"' in zipfile.ZipFile(d).read("content.xml"), "content.xml del ODS lleva el nombre real")

    # ---- PDF de Macaco con la fuente (si exporta por --convertir)
    out = m.comando("macaco --convertir /usb/f.mxs /usb/f.pdf", 120)
    if "bien" in out:
        m.comando("sync")
        d = os.path.join(SAIDA, "h358-f.pdf")
        mtools(pen, "mcopy", "-o", "p:/f.pdf", d)
        pdf = open(d, "rb").read()
        verificar(b"SaguiPrueba" in pdf and b"FontFile2" in pdf, "el PDF de Macaco incrusta la fuente instalada")
    else:
        print("  (macaco --convertir no exporta a PDF por consola; se prueba la exportación desde la ventana en otra prueba)")

    # ---- quitar
    out, rc = fuentes_ok(m, 'fuentes quitar "%s"' % EXTRA)
    verificar(rc == 0 and "quitada" in out, "fuentes quitar: %s" % out.strip().splitlines()[-2:])
    verificar("SaguiPrueba" not in m.comando("ls /home/fuentes"), "el archivo se borró")
    pl = familias(m.comando("pluma --fuentes", 60), "pluma")
    mc = listar(m, "macaco")
    verificar(EXTRA not in pl.values() and EXTRA not in mc.values(), "«%s» ya no sale en Pluma ni en Macaco" % EXTRA)
    out, rc = fuentes_ok(m, 'fuentes quitar "Familia Inexistente"')
    verificar(rc != 0, "quitar una familia que no existe da error")
    out, rc = fuentes_ok(m, 'fuentes quitar "DejaVu Sans"')
    verificar(rc != 0 and "no se quita" in out, "las de la base no se quitan desde aquí")

    # ---- para todas las cuentas (root, /apps/fuentes en SAGUI-APPS)
    out, rc = fuentes_ok(m, "fuentes instalar --sistema /usb/SaguiPrueba.ttf")
    verificar(rc == 0, "root instala para todas las cuentas: %s" % out.strip().splitlines()[-2:])
    verificar("SaguiPrueba.ttf" in m.comando("ls /apps/fuentes"), "está en /apps/fuentes (partición SAGUI-APPS)")
    verificar("todas las cuentas" in m.comando("fuentes"), "`fuentes` la marca como de todas las cuentas")
    verificar(EXTRA in familias(m.comando("pluma --fuentes", 60), "pluma").values(), "Pluma la ve")
    out, rc = fuentes_ok(m, 'fuentes quitar --sistema "%s"' % EXTRA)
    verificar(rc == 0 and "SaguiPrueba" not in m.comando("ls /apps/fuentes"), "se quita de /apps/fuentes")
    m.comando("rm /apps/fuentes/.instalando")
    out, rc = fuentes_ok(m, "fuentes instalar /usb/SaguiPrueba.ttf")
    verificar(rc == 0, "se vuelve a instalar para la cuenta (la ventana usa esta)")


def pagina_celda(png, w, fila):
    """Luminancias de la celda A<fila> de la hoja (la columna A mide 300)."""
    return region(png, w["x"] + GRID_X + 3, w["y"] + GRID_Y + (fila - 1) * ROW_H + 1, 280, ROW_H - 2)


def tinta(filas):
    return sum(1 for f in filas for v in f if v < 200)


def parte2(m):
    # ---- Macaco: la celda se dibuja con la fuente instalada
    n0 = len(m.serial())
    m.digitar("macaco --clasico /usb/f.mxs\n", 0.05)
    time.sleep(6)
    w = janela(m.serial()[n0:], "Macaco")
    time.sleep(0.8)
    png = m.captura("H358-1-macaco-celdas")
    a1, a2, a3, a4 = (pagina_celda(png, w, i) for i in (1, 2, 3, 4))
    verificar(tinta(a1) > 100, "A1 (fuente instalada) tiene tinta (%d)" % tinta(a1))
    verificar(a1 == a2, "A1 («%s») y A2 (DejaVu Serif) tienen exactamente los mismos píxeles" % EXTRA)
    verificar(a1 != a3 and a1 != a4, "A1 difiere de la 8x16 (A3) y de otra fuente extra (A4)")
    gris = len(set(v for f in a1 for v in f if 40 < v < 215))
    verificar(gris >= 8, "la fuente vectorial se dibuja con antialias en la celda (%d niveles de gris)" % gris)
    # el combo de fuentes de Macaco lista las del sistema
    k = len(m.serial())
    m.clique(w["x"] + 40, w["y"] + 76 + 12)    # cuadro de nombre
    m.digitar("A1", 0.05)
    m.tecla("ret", 0.5)
    m.clique(w["x"] + 70, w["y"] + 61)         # el combo de fuentes de la barra de formato (clásico)
    fin = time.time() + 15
    while "dialogo: fuentes" not in m.serial()[k:] and time.time() < fin:
        time.sleep(0.3)
    n = re.findall(r"dialogo: fuentes (\d+) familias", m.serial()[k:])
    verificar(bool(n) and int(n[-1]) >= 12, "el combo de Macaco lista las familias del sistema (%s)" % (n[-1:] if n else "ninguna"))
    time.sleep(0.6)
    m.captura("H358-2-macaco-combo")
    m.tecla("esc")
    time.sleep(0.5)
    # cerrar Macaco por su botón
    m.clique(w["x"] + w["l"] - 14, w["y"] - 12)
    time.sleep(1.5)

    # ---- la ventana de fuentes
    n0 = len(m.serial())
    m.digitar("fuentes --ventana\n", 0.05)
    time.sleep(3)
    f = janela(m.serial()[n0:], "Fuentes")
    m.captura("H358-3-fuentes-ventana")
    m.clique(f["x"] + f["l"] - 14, f["y"] - 12)
    time.sleep(1)

    # ---- vista previa de un archivo dañado: se rechaza
    n0 = len(m.serial())
    m.digitar("fuentes --ventana /usb/danada.ttf\n", 0.05)
    time.sleep(3)
    f = janela(m.serial()[n0:], "Instalar fuente")
    verificar("fuentes: vista previa /usb/danada.ttf (-4)" in m.serial()[n0:], "la vista previa valida el archivo y lo rechaza (-4: dañado)")
    m.captura("H358-4-previa-danada")
    m.clique(f["x"] + f["l"] - 14, f["y"] - 12)
    time.sleep(1)

    # ---- vista previa de la buena e instalar con el botón
    m.comando("rm /home/fuentes/SaguiPrueba.ttf")
    n0 = len(m.serial())
    m.digitar("fuentes --ventana /usb/SaguiPrueba.ttf\n", 0.05)
    time.sleep(3)
    f = janela(m.serial()[n0:], "Instalar fuente")
    m.captura("H358-5-previa-buena")
    m.clique(f["x"] + 12 + 50, f["y"] + f["a"] - 100 + 24 + 13)     # «Instalar»
    fin = time.time() + 15
    while "fuentes: instalada" not in m.serial()[n0:] and time.time() < fin:
        time.sleep(0.3)
    verificar("fuentes: instalada %s" % EXTRA in m.serial()[n0:], "el botón «Instalar» instala la fuente")
    time.sleep(0.8)
    m.captura("H358-6-previa-instalada")
    m.clique(f["x"] + f["l"] - 14, f["y"] - 12)
    time.sleep(1)
    m.clique(500, 150)       # el foco vuelve a la terminal
    time.sleep(0.5)
    verificar("SaguiPrueba.ttf" in m.comando("ls /home/fuentes"), "la fuente está en /home/fuentes")

    # ---- Pluma con la fuente: la línea coincide con la de DejaVu Serif
    pos = lanzar(m, "pluma --clasico")
    esperar_desde(m, "pluma: pronto", pos, 40)
    time.sleep(1.5)
    w = janela_pluma(m, "Documento1 - Pluma", pos)
    for nombre in (EXTRA, "DejaVu Serif"):
        h319.elegir_familia(m, w, 4, nombre)
        m.digitar("Hola Mundo gQy 123", 0.05)
        m.tecla("ret")
    time.sleep(0.6)
    b = h319.bandas(m, w, "H358-7-pluma")
    verificar(len(b) >= 2, "dos líneas de tinta en Pluma")
    png = os.path.join(RAIZ, "docs", "capturas", "H358-7-pluma.png")
    x0, y0 = w["x"] + 60, w["y"] + 51 + 21 + 20 + 20
    l1 = region(png, x0, y0 + b[0][0], 400, 22)
    l2 = region(png, x0, y0 + b[1][0], 400, 22)
    verificar(l1 == l2, "la línea con «%s» coincide píxel a píxel con la de DejaVu Serif en Pluma" % EXTRA)


if __name__ == "__main__":
    main()
