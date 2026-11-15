#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H300 (D300) - detector de problemas de la GUI de la libsagui.

Usa /testes/gui-revisar (userland/testes/gui-revisar.c): dos botones solapados, un texto que no cabe
y un botón más bajo que la fuente. Comprueba que:
  apagado   sin activar no sale ningún [GUI-REVISAR] (coste cero);
  solape    con /etc/gui-revisar.modo = 1 avisa del solape, del texto cortado y del control pequeño,
            con coordenadas, sin repetir el mismo aviso;
  limpio    el caso bien colocado no avisa de nada;
  ignorado  gui_revisar_ignorar() en uno de los botones silencia el solape;
  patron    /etc/gui-revisar.ignorar con «programa:tipo:texto» silencia el solape pero no el resto;
  entorno   SAGUI_GUI_REVISAR=1 en el entorno también lo activa;
  visual    el modo 2 dibuja el borde rojo (captura H300-1-modo-visual.png).
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402


def avisos(texto):
    return [l for l in texto.splitlines() if "[GUI-REVISAR]" in l and "detector activo" not in l and "cobertura" not in l]


def main():
    with Maquina("h300", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        out = m.comando("/testes/gui-revisar solape", 60)
        verificar("gui-revisar: pronto solape" in out, "el programa de prueba arranca")
        verificar(not avisos(out) and "[GUI-REVISAR]" not in out, "apagado no avisa de nada")

        m.comando("echo 1 > /etc/gui-revisar.modo")
        ser0 = m.serial()
        out = m.comando("/testes/gui-revisar solape", 60)
        av = avisos(out)
        verificar("detector activo (modo 1)" in out, "el detector se activa con /etc/gui-revisar.modo")
        solapes = [l for l in av if " solape " in l]
        verificar(len(solapes) == 1 and 'botao(20,20 120x28 "Aceptar")' in solapes[0] and
                  'botao(100,30 120x28 "Cancelar")' in solapes[0] and "zona 40 px x 18 px" in solapes[0],
                  "avisa del solape con coordenadas y zona: %s" % solapes)
        verificar("gui-revisar Prueba GUI" in solapes[0], "el aviso lleva programa y ventana")
        cortados = [l for l in av if "texto cortado" in l]
        verificar(len(cortados) == 1 and '"Texto larguisimo"' in cortados[0] and "128" in cortados[0],
                  "avisa del texto que no cabe: %s" % cortados)
        pequenos = [l for l in av if "demasiado pequeño" in l or "demasiado peque" in l]
        verificar(len(pequenos) == 1 and "botao(290,20" in pequenos[0], "avisa del control demasiado pequeño: %s" % pequenos)
        verificar(len(av) == 3, "ni más ni menos de 3 avisos, sin repetir (%d)" % len(av))

        out = m.comando("/testes/gui-revisar limpio", 60)
        verificar("gui-revisar: pronto limpio" in out and not avisos(out), "el caso limpio no avisa: %s" % avisos(out))

        out = m.comando("/testes/gui-revisar ignorado", 60)
        verificar("gui-revisar: pronto ignorado" in out and not avisos(out),
                  "gui_revisar_ignorar silencia el solape: %s" % avisos(out))

        m.comando("echo gui-revisar:botao:Cancelar > /etc/gui-revisar.ignorar")
        out = m.comando("/testes/gui-revisar solape", 60)
        av = avisos(out)
        verificar(not [l for l in av if " solape " in l], "el patrón programa:tipo:texto silencia el solape: %s" % av)
        verificar(len([l for l in av if "texto cortado" in l]) == 1, "pero el texto cortado sigue avisando")
        m.comando("rm /etc/gui-revisar.ignorar")

        m.comando("rm /etc/gui-revisar.modo")
        out = m.comando("SAGUI_GUI_REVISAR=1 /testes/gui-revisar solape", 60)
        if "detector activo" not in out:
            out = m.comando("export SAGUI_GUI_REVISAR=1", 20) + m.comando("/testes/gui-revisar solape", 60)
        verificar("detector activo (modo 1)" in out and len([l for l in avisos(out) if " solape " in l]) == 1,
                  "la variable de entorno también lo activa")

        # modo visual: se lanza en segundo plano y se captura con la ventana abierta
        m.comando("export SAGUI_GUI_REVISAR=2", 20)
        m.digitar("/testes/gui-revisar solape\n", 0.04)
        m.esperar_serial("gui-revisar: pronto solape", 30)
        time.sleep(0.8)
        m.captura("H300-1-modo-visual")
        m.esperar_serial("sagui:/$", 30)
    print("h300: OK")


if __name__ == "__main__":
    main()
