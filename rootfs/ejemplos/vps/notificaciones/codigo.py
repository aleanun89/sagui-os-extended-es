# Notificaciones: avisos tipo toast, no modales, que aparecen en una esquina del formulario, se apilan
# y se van solos. notificar(texto, tipo, duracion, titulo) los crea desde el código; el control
# Notificacion los deja ya configurados (con un botón «Deshacer» que dispara su evento accion).
import os

ARCHIVO = "/home/nota-toast.txt" if os.path.isdir("/home") else "/tmp/nota-toast.txt"
anterior = ""


def leer():
    try:
        f = open(ARCHIVO)
        t = f.read()
        f.close()
        return t
    except OSError:
        return ""


def escribir(ruta, texto):
    f = open(ruta, "w")
    f.write(texto)
    f.close()


def Boton1_click():
    global anterior
    anterior = leer()
    escribir(ARCHIVO, CuadroTexto1.texto)
    Notificacion1.mostrar("Nota guardada")


def Boton2_click():
    try:
        escribir("/no-existe/nota.txt", CuadroTexto1.texto)
    except Exception:
        notificar("No se pudo guardar la nota", "error", 0, "Error al guardar")


def Boton3_click():
    notificar("La nota está vacía", "aviso", 6000, "Atención")


def Boton4_click():
    notificar("Los avisos se apilan en la esquina", "info", 4000)


def Notificacion1_accion():
    escribir(ARCHIVO, anterior)
    CuadroTexto1.texto = anterior
    notificar("Cambio deshecho", "info", 2000)


def Notificacion1_cerrado():
    Etiqueta2.texto = "Último aviso cerrado: " + Notificacion1.motivo
