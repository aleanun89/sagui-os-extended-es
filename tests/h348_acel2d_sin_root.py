#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H348 (D346) - la aceleración 2D por la GPU, vista y medida sin ser administrador.

La autoprueba del acelerador (INFO_ACEL2D_PROVA) y encender/apagar (INFO_ACEL2D_CTL) escriben en la memoria de
vídeo y en los registros de la GPU: siguen siendo del administrador. Lo demás es de lectura y no pide permisos.
Con la VMware SVGA II del QEMU y la sesión como sagui (usuario normal), en el escritorio:
  * «acel2d» dice que la 2D está activa y da el reparto GPU / software (contadores);
  * arrastrar una ventana hace crecer los contadores de copias por la GPU (la 2D se aplica al usuario normal);
  * «acel2d probar» y «acel2d apagar» siguen negados (administrador), y el estado no cambia;
  * «infopc --bench-gpu» no pide nada: banco fuera de pantalla por software, con el motivo y las sumas bien;
  * infopc -t gpu enseña el reparto.
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, FalhaTeste, verificar, imagem_de_teste  # noqa: E402
from h161_acel2d import VMWARE, arrancar, arrastar_lento, TITULO, DESTINO  # noqa: E402


def contadores(m):
    out = m.comando("acel2d", 30).replace("\r", "")
    g = re.search(r"GPU: copias (\d+) \((\d+) kpx\), rellenos (\d+) \((\d+) kpx\)", out)
    s = re.search(r"software: mover (\d+) \((\d+) kpx\), consola (\d+) \((\d+) kpx\), rellenos (\d+) \((\d+) kpx\), volcados (\d+) \((\d+) kpx\)", out)
    verificar(g and s, "«acel2d» da el reparto GPU / software")
    return out, [int(x) for x in g.groups()], [int(x) for x in s.groups()]


def main():
    img = imagem_de_teste("", ["--vesa", "--sesion-sagui"])
    with Maquina("h348", "ide", imagem=img, extras=VMWARE) as m:
        arrancar(m)
        verificar("[PERMISOS] sesion como sagui" in m.serial(), "la sesión es sagui (usuario normal)")
        out, g0, s0 = contadores(m)
        print("  ", out.strip().splitlines()[0])
        verificar(re.search(r"Aceleracion 2D: vmware", out), "la 2D está activa (vmware) para el usuario normal")
        # Con un aviso a la vista (el de «root sin contraseña») el gestor arrastra por el camino de siempre (la CPU):
        # se espera a que se vaya para medir la GPU.
        time.sleep(20)
        arrastar_lento(m, *TITULO, *DESTINO)
        out, g1, s1 = contadores(m)
        print("   GPU antes", g0, "después", g1, "; software antes", s0, "después", s1)
        verificar(g1[0] > g0[0], f"arrastrar la ventana hace más copias por la GPU ({g0[0]} -> {g1[0]})")
        verificar(g1[1] > g0[1], "y mueve píxeles por la GPU")
        verificar(s1[6] > s0[6], "el volcado de pantalla por la CPU también se cuenta (software)")
        # Lo que cambia el estado sigue pidiendo ser administrador.
        out = m.comando("acel2d probar", 60).replace("\r", "")
        verificar(re.search(r"administrador|admin", out, re.I) and "BIEN" not in out, "«acel2d probar» sigue pidiendo administrador")
        out = m.comando("acel2d apagar", 60).replace("\r", "")
        verificar(re.search(r"administrador|admin", out, re.I), "«acel2d apagar» sigue pidiendo administrador")
        out, g2, s2 = contadores(m)
        verificar("Aceleracion 2D: vmware" in out, "la 2D sigue activa tras los intentos sin permiso")
        # El banco sin permisos: sin diálogo, fuera de pantalla, sumas bien.
        out = m.comando("infopc --bench-gpu", 600).replace("\r", "")
        g = re.search(r"^Camino GPU: (.*)$", out, re.M)
        verificar(g and "FALL" not in g.group(0) and re.search(r"administrador", g.group(0)), "el banco dice por qué no usa la autoprueba y no declara fallo")
        filas = re.findall(r"^(?:Rellenos de rect|Copias de bloques|Copias solapadas|Líneas|Texto|Ventana).*\sOK\s+\d+$", out, re.M)
        verificar(len(filas) >= 5, f"las pruebas fuera de pantalla con suma correcta ({len(filas)})")
        verificar(re.search(r"Reparto GPU / software", out), "infopc enseña el reparto GPU / software")
        verificar(re.search(r"GPU:\s+\d+ copias", out) and re.search(r"Software:\s+\d+ mover", out), "con las dos filas de contadores")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except FalhaTeste as e:
        print("FALHOU:", e)
        sys.exit(1)
