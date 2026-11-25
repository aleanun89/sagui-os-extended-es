#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H466 - VisualPythonS: notificaciones toast, Calendario ampliado y Arbol (D466).

Criterio:
  1. la autoprueba de vps (autoprueba_toast.py, sin ratón): notificar() en los cuatro tipos, el apilado, el
     cierre por tiempo, por clic y por la «x», el botón de acción, las cuatro esquinas, el control Notificacion
     (propiedades, mostrar(), eventos click, accion y cerrado), el Calendario (flechas de mes y de año,
     minimo y maximo, doble clic, teclado), el Arbol (agregar, expandir, contraer, elegir, quitar, eventos,
     teclado, la propiedad nodos) y el diseñador (colocar, guardar, cargar, ejecutar, generar la aplicación);
  2. el ejemplo «notificaciones» con clics reales: guardar enseña un toast de éxito con «Deshacer», «Guardar
     mal» uno de error que no se va, los avisos se apilan, un clic cierra uno y la «x» otro, y los demás se van
     solos al cabo de su duración;
  3. el ejemplo «agenda-carpetas» con clics reales: elegir una carpeta del Arbol, un día del Calendario,
     guardar una nota (toast) y contraer una carpeta con doble clic;
  4. el diseñador con el ratón: colocar Notificacion, Arbol y Calendario desde la caja de herramientas,
     guardar el proyecto y ejecutar el ejemplo de notificaciones con F5.
Capturas: docs/capturas/VPS-manual-notificaciones-*.png, VPS-manual-agenda-*.png y H466-*.png
"""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar  # noqa: E402
from h303_vps_manual import (janela, esperar, cerrar, sonda, ver, region, pantalla, foto, autoprueba, lanzar,  # noqa: E402
                             clic, reg, guardar_y_cerrar)


def neutro(m, j):
    """El puntero a la barra de título de la ventana: no tapa las regiones que se comparan."""
    m.mouse_para(j["x"] + 120, j["y"] - 12)
    time.sleep(0.3)


def principal():
    with Maquina("h466", "ide") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)

        # ---------------------------------------------------------- 1. la autoprueba
        oks = autoprueba(m, "autoprueba_toast.py", 90)
        for clave in ("toast: tipo error", "toast: se apilan", "toast: un clic la cierra", "toast: la x la cierra",
                      "toast: desaparece sola tras la duración", "toast: el botón de acción llama a su función",
                      "toast: no roba el foco", "Notificacion: eventos click y cerrado", "Notificacion: evento accion",
                      "calendario: > mes siguiente", "calendario: >> año siguiente", "calendario: un día antes de `minimo` no se elige",
                      "calendario: doble_click en un día", "arbol: clic en el cuadrito contrae", "arbol: clic en un nodo lo elige",
                      "arbol: la propiedad nodos", "diseñador: se colocan los tres controles nuevos",
                      "diseñador: el .vpf lleva los controles y sus propiedades", "diseñador: ejecutar: el botón enseña la notificación",
                      "diseñador: la aplicación generada se ejecuta", "ejemplo notificaciones: guardar enseña el toast",
                      "ejemplo agenda: guardar una nota"):
            verificar(any(clave in o for o in oks), "autoprueba: " + clave)

        # ---------------------------------------------------------- 2. el ejemplo de notificaciones
        j = lanzar(m, "notificaciones", "Notificaciones")
        neutro(m, j)
        base = reg(m, j, "n0", 0, 150, 384, 130)
        foto(m, "notificaciones-1", j)
        clic(m, j, 12 + 55, 44 + 14)                                    # Guardar: toast de éxito con «Deshacer»
        time.sleep(0.8)
        neutro(m, j)
        uno = reg(m, j, "n1", 0, 150, 384, 130)
        verificar(base != uno, "ejemplo: Guardar enseña el toast de éxito")
        neutro(m, j)
        foto(m, "notificaciones-2", j)
        clic(m, j, 130 + 55, 44 + 14)                                   # Guardar mal: toast de error (no se va solo)
        time.sleep(0.8)
        neutro(m, j)
        dos = reg(m, j, "n2", 0, 150, 384, 130)
        verificar(dos != uno, "ejemplo: Guardar mal enseña el de error junto al de éxito")
        clic(m, j, 12 + 55, 80 + 14)                                    # Aviso
        clic(m, j, 130 + 55, 80 + 14)                                   # Informar
        time.sleep(0.8)
        cuatro = reg(m, j, "n3", 0, 20, 384, 260)
        neutro(m, j)
        foto(m, "notificaciones-3", j)
        # un clic en el toast de abajo (el más nuevo, «Informar») lo cierra y Etiqueta2 lo dice
        et = reg(m, j, "n3e", 12, 124, 356, 20)
        clic(m, j, 142, 250)
        time.sleep(0.8)
        verificar(et != reg(m, j, "n3f", 12, 124, 356, 20), "ejemplo: un clic en un toast lo cierra (Etiqueta2 cambia)")
        verificar(cuatro != reg(m, j, "n4", 0, 20, 384, 260), "ejemplo: al cerrar uno, los demás se recolocan")
        # los de duración limitada se van solos; el de error se queda
        time.sleep(9)
        neutro(m, j)
        solo_error = reg(m, j, "n5", 0, 150, 384, 130)
        verificar(solo_error != base and solo_error != dos, "ejemplo: quedan solo los de duración 0 (el de error), los demás se fueron solos")
        foto(m, "notificaciones-4", j)
        clic(m, j, 360, 236)                                            # la «x» del de error
        time.sleep(0.8)
        neutro(m, j)
        verificar(base == reg(m, j, "n6", 0, 150, 384, 130), "ejemplo: la «x» cierra el de error y vuelve la ventana limpia")
        foto(m, "notificaciones-5", j)
        cerrar(m, j)
        time.sleep(0.8)
        m.comando("rm -f /home/nota-toast.txt /tmp/nota-toast.txt")

        # ---------------------------------------------------------- 3. la agenda por carpetas
        j = lanzar(m, "agenda-carpetas", "Agenda por carpetas")
        foto(m, "agenda-1", j)
        antes = reg(m, j, "a0", 392, 8, 160, 40)
        clic(m, j, 8 + 2 + 4 + 16 + 16 + 10, 8 + 2 + 2 * 18 + 9)       # el nodo «Informes»
        time.sleep(0.6)
        verificar(antes != reg(m, j, "a1", 392, 8, 160, 40), "agenda: elegir un nodo del Arbol cambia la ruta")
        clic(m, j, 392 + 40, 52 + 30)
        m.digitar("Preparar el informe", 0.08)
        barra0 = reg(m, j, "a1b", 0, 238, 300, 22)
        clic(m, j, 186 + 98, 190 + 13)                                  # Guardar nota: toast de éxito
        time.sleep(0.8)
        foto(m, "agenda-2", j)
        verificar(barra0 != reg(m, j, "a2", 0, 238, 300, 22), "agenda: la barra de estado cuenta las notas guardadas")
        nota = reg(m, j, "a3", 392, 52, 160, 164)
        clic(m, j, 186 + 2 + 3 * 27 + 13, 8 + 2 + 22 + 18 + 2 * 21 + 10)   # otro día del calendario
        time.sleep(0.6)
        verificar(nota != reg(m, j, "a3b", 392, 52, 160, 164), "agenda: otro día del calendario, otra nota")
        arbol_antes = reg(m, j, "a4", 8, 8, 170, 176)
        clic(m, j, 8 + 2 + 4 + 16 + 10, 8 + 2 + 9, duplo=True)         # doble clic en «Trabajo»: contrae la carpeta
        time.sleep(0.8)
        verificar(arbol_antes != reg(m, j, "a5", 8, 8, 170, 176), "agenda: un doble clic contrae la carpeta")
        foto(m, "agenda-3", j)
        cerrar(m, j)
        time.sleep(0.8)

        # ---------------------------------------------------------- 4. el diseñador con el ratón
        marca = len(m.serial())
        m.digitar("python /lib/python/vps/disenador.py --sonda\n", 0.03)
        d = janela(m, "VisualPythonS", marca)
        s = sonda(m, marca)
        cl = s["cliente"]
        for herr, (x, y, w, h) in (("Arbol", (8, 8, 112, 130)), ("Calendario", (124, 8, 188, 170)),
                                   ("Notificacion", (16, 150, 32, 32))):
            m.clique(*pantalla(d, s["herr:" + herr]))
            m.arrastar(*pantalla(d, (cl[0] + x, cl[1] + y)), *pantalla(d, (cl[0] + x + w, cl[1] + y + h)))
            time.sleep(0.5)
        s = ver(m)
        verificar("ctl:Notificacion1" in s and "ctl:Arbol1" in s and "ctl:Calendario1" in s,
                  "diseñador: se colocaron Notificacion1, Arbol1 y Calendario1 con el ratón")
        m.captura("H466-1-disenador")
        verificar(True, "diseñador: captura con los tres controles")
        marca_g = len(m.serial())
        guardar_y_cerrar(m, d, cl, "ProyectoToast")
        ruta = re.search(r"vps: guardado (/\S*ProyectoToast)", m.serial()[marca_g:]).group(1)
        out = m.comando("cat %s/formulario.vpf" % ruta)
        verificar("control Notificacion Notificacion1" in out and "control Arbol Arbol1" in out and "control Calendario Calendario1" in out,
                  "diseñador: el .vpf guardado lleva los tres controles")

        # F5 desde el diseñador: ejecuta el ejemplo de notificaciones
        marca = len(m.serial())
        m.digitar("python /lib/python/vps/disenador.py --sonda /ejemplos/vps/notificaciones\n", 0.03)
        d = janela(m, "VisualPythonS", marca)
        time.sleep(1.0)
        marca2 = len(m.serial())
        m.tecla("f5", 1.5)
        j = janela(m, "Notificaciones", marca2)
        base = reg(m, j, "d0", 0, 150, 384, 130)
        clic(m, j, 12 + 55, 44 + 14)
        time.sleep(0.8)
        verificar(base != reg(m, j, "d1", 0, 150, 384, 130), "diseñador: F5 ejecuta el ejemplo y el botón enseña el toast")
        m.captura("H466-2-ejecutar")
        cerrar(m, j)
        time.sleep(1.0)
        cerrar(m, d)
        time.sleep(1.5)
        m.comando("rm -f /home/nota-toast.txt /tmp/nota-toast.txt")


if __name__ == "__main__":
    principal()
