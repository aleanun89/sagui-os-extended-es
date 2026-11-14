#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H205 - el visor de PDF con PDF reales (v7.1.1, D205).

H107 solo usa PDF escritos a mano (tests/pdf_exemplos.py) y no vio que, con
PDF reales, el visor sacaba la página en blanco o desplazada: el operador cm
se concatenaba al revés, las fuentes Type 1/CFF no se leían, la xref en
flujo con predictor no se entendía, etc. Esta prueba abre PDF de verdad,
de productores distintos (tests/recursos/pdf/LEEME):

  - OpenOffice (TrueType), Adobe Designer (CFF CID, xref en flujo), pdfTeX
    (Type 1, ObjStm), cairo (TrueType + CIDFontType2), un escaneado (CCITT
    grupo 4) y Word/Acrobat (JPEG con SMask, Indexed);

y comprueba en SaguiOS:

  - que se abren y dicen bien el número de páginas (visorpdf -t);
  - que la página dibujada (visorpdf --ppm) se parece a la que dibuja
    MuPDF: las dos se reducen a una rejilla de grises de 120 columnas y la
    diferencia media tiene que ser pequeña comparada con la de una página
    en blanco, y la cantidad de tinta parecida (tests/pdf_reales_comum.py;
    las referencias se generan con tools/pdf_referencias.py).
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import (Maquina, verificar, criar_pendrive, pendrive_qemu, mtools,  # noqa: E402
                  ler_ppm, SAIDA)
import pdf_reales_comum as comum  # noqa: E402


def main():
    arquivos = {}
    for nome, _pag, _n, _desc in comum.PDFS:
        with open(os.path.join(comum.RECURSOS, nome), "rb") as f:
            arquivos[nome] = f.read()
    pen = criar_pendrive("h205-pen", rotulo="PDFREAL", arquivos=arquivos)
    extras = ["-device", "usb-ehci,id=ehci"] + pendrive_qemu(pen)

    with Maquina("h205", "ide", extras=extras) as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        out = m.comando("montar usb0")
        verificar("Pendrive montado en /usb" in out, "el pendrive con los PDF está montado")

        for nome, pag, n_pag, desc in comum.PDFS:
            print("--", nome, "(%s)" % desc)
            out = m.comando("visorpdf -t /usb/%s" % nome, 60)
            verificar("paginas: %d" % n_pag in out,
                      "%s se abre y tiene %d páginas" % (nome, n_pag))

            saida = "r%d.ppm" % comum.PDFS.index((nome, pag, n_pag, desc))
            out = m.comando("visorpdf --ppm /usb/%s /usb/%s %d %d" % (nome, saida, pag, comum.ZOOM), 120)
            verificar("ppm:" in out, "%s: se escribió el PPM de la página %d" % (nome, pag))
            local = os.path.join(SAIDA, "h205-" + saida)
            if os.path.exists(local):
                os.remove(local)
            mtools(pen, "mcopy", "-o", "p:/" + saida, local)
            verificar(os.path.exists(local), "%s: el PPM salió del pendrive" % nome)

            larg, alt, px = ler_ppm(local)
            ra, rl, ref = comum.ler_pgm_gz(comum.referencia(nome, pag))
            _a, _l, val = comum.rejilla(larg, alt, comum.gris_de_rgb(larg, alt, px), ra, rl)
            dif, _muito = comum.comparar(val, ref)
            dif_branco, _ = comum.comparar([255] * len(ref), ref)
            t, t_ref = comum.tinta(val), comum.tinta(ref)
            print("   diferencia %.2f (página en blanco: %.2f), tinta %.4f (MuPDF %.4f)"
                  % (dif, dif_branco, t, t_ref))
            # Las esquinas son papel blanco (no basura).
            canto = px[0:3]
            verificar(min(canto) > 200, "%s: la esquina de arriba es papel blanco %r" % (nome, tuple(canto)))
            verificar(dif <= max(1.5, 0.45 * dif_branco),
                      "%s: la página se parece a la de MuPDF (diferencia %.2f)" % (nome, dif))
            verificar(t_ref > 0 and 0.6 <= t / t_ref <= 1.5,
                      "%s: la tinta es parecida (%.4f frente a %.4f)" % (nome, t, t_ref))

    print("H205: tudo bem")


if __name__ == "__main__":
    try:
        main()
    except Exception as e:  # noqa: BLE001
        print("H205 FALHOU:", e)
        sys.exit(1)
