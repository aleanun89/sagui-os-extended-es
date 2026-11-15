# SPDX-License-Identifier: GPL-2.0-or-later
# Lista de tareas: se guarda en tareas.txt (en /home, o en /tmp si no hay /home) cada vez que cambia.
import os

ARCHIVO = "/home/tareas.txt" if os.path.isdir("/home") else "/tmp/tareas.txt"


def guardar():
    f = open(ARCHIVO, "w")
    f.write("\n".join(Lista1.elementos))
    f.close()
    Etiqueta1.texto = "%d tareas" % Lista1.contar()


def Formulario_cargar():
    try:
        f = open(ARCHIVO)
        for linea in f.read().split("\n"):
            if linea:
                Lista1.agregar(linea)
        f.close()
    except OSError:
        pass
    Etiqueta1.texto = "%d tareas" % Lista1.contar()


def Boton1_click():
    texto = CuadroTexto1.texto.strip()
    if texto:
        Lista1.agregar(texto)
        CuadroTexto1.texto = ""
        guardar()


def Boton2_click():
    i = Lista1.seleccion
    if i >= 0:
        Lista1.quitar(i)
        guardar()
