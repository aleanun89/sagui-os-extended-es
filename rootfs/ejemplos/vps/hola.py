# Hola, VisualPythonS: una aplicación hecha solo con código, sin el diseñador.
# Los manejadores se conectan solos por el nombre (Nombre_evento) o con @evento(control, "evento").
#
#     python /ejemplos/vps/hola.py
from vps import *

f = Formulario("Hola", 300, 150)
etiqueta = Etiqueta(f, "Pulsa el botón", 20, 20, 250, 20, nombre="Etiqueta1")
boton = Boton(f, "Saludar", 20, 50, 120, 28, nombre="Boton1")
nombre = CuadroTexto(f, "", 20, 90, 200, 22, nombre="CuadroTexto1")


def Boton1_click():
    quien = nombre.texto or "mundo"
    etiqueta.texto = "¡Hola, %s!" % quien
    print("vps: click ->", etiqueta.texto)


@evento(nombre, "cambio")
def al_escribir():
    boton.activo = len(nombre.texto) < 20


def Formulario_cargar():
    print("vps: cargado")


ejecutar(globals())
