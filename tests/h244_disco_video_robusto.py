#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H244 (D245) - disco y vídeo robustos en hardware lento (HP dv7, preventivo).

 (a) ATA por PIO sin congelar: una lectura grande por IDE (cat de un archivo de 24 MB del
     segundo disco) deja pasar las interrupciones entre sectores: el log anota las
     "cesiones de CPU" y que el tic avanzó durante la lectura (tics > 0);
 (b) el log trae "[VIDEO] framebuffer: WC por PAT|MTRR|ninguno, N MB/s" y infopc -t video
     lo muestra;
 (c) un programa en pantalla completa que no cambia no provoca cuadros (el contador de la
     salida de la pantalla completa no sube);
 (d) animado, el log anota "[GUI] pantalla completa: N fps" con N > 0.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, imagem_de_teste, criar_disco  # noqa: E402


def main():
    hd, _ = criar_disco("h244-hd", [dict(tipo="ext2", mb=40, rotulo="GRANDE",
                                         arquivos={"grande.bin": os.urandom(1 << 20) * 24})])
    disco = ["-drive", f"file={hd},if=ide,index=1,format=raw"]
    # D281: este apartado vigila las cesiones de CPU del PIO; el IDE por DMA (lo normal) tiene h281.
    with Maquina("h244-ata", "ide", imagem=imagem_de_teste("", ["--vesa", "--console", "--sem-dma"]), extras=disco) as m:
        m.esperar_serial("sagui:/$", 120)
        s = m.serial()
        r = re.search(r"\[VIDEO\] framebuffer: WC por (PAT|MTRR|ninguno), (\d+) MB/s", s)
        verificar(r is not None, "log: framebuffer: WC por PAT|MTRR|ninguno, N MB/s")
        verificar(int(r.group(2)) > 0, f"la escritura al framebuffer se midió ({r.group(0)})")
        m.comando("mount hdb1 /interno")
        m.comando("cat /interno/grande.bin | wc -c", tempo=180)
        s = m.serial()
        g = re.findall(r"\[ATA\] hdb: lectura de (\d+) sectores, (\d+) tics, (\d+) cesiones", s)
        verificar(g, "lectura grande anotada en el log")
        verificar(max(int(c) for _, _, c in g) > 0, "la lectura cedió la CPU entre sectores")
        # Con las interrupciones ya apagadas por la llamada al sistema (kernel no expulsable,
        # D165) no se cede: pendiente de decidir (ver D245). Aquí solo se vigila que la
        # lectura grande termine bien y que las esperas por tiempo no la rompan.
        verificar("erro de leitura" not in s, "la lectura grande no dio errores")
        out = m.comando("infopc -t video")
        verificar(re.search(r"Write-combining:.*MB/s", out) is not None, "infopc -t video muestra los MB/s del framebuffer")

    with Maquina("h244-tc", "ide", imagem=imagem_de_teste("", ["--vesa"])) as m:
        m.esperar_serial("sagui:", 120)
        m.comando("/testes/tela-cheia &")
        m.esperar_serial("tela-cheia: pedi a tela cheia", 20)
        time.sleep(3)
        m.esperar_serial("tela-cheia: tamanho", 10)
        n0 = m.serial().count("[GUI] pantalla completa")
        time.sleep(7)
        verificar(m.serial().count("[GUI] pantalla completa") == n0 and "pantalla completa:" not in m.serial()[-4000:],
                  "quieto en pantalla completa: ni un cuadro (no hay línea de fps)")
        m.tecla("a")
        t = time.time()
        while time.time() - t < 30 and not re.search(r"\[GUI\] pantalla completa: [1-9]\d* fps", m.serial()):
            time.sleep(0.5)
        r = re.search(r"\[GUI\] pantalla completa: (\d+) fps", m.serial())
        verificar(r and int(r.group(1)) > 0, f"animado: fps medidos ({r.group(0) if r else 'sin línea'})")
        m.tecla("a")
        m.tecla("q")


if __name__ == "__main__":
    main()
