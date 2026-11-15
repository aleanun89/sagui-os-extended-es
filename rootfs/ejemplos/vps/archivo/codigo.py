# Archivo: sin ventana; lee y escribe el archivo de su propiedad `ruta` (texto, líneas o CSV).

def Formulario_cargar():
    if Archivo1.existe():
        CuadroTexto1.texto = Archivo1.leer()
        Etiqueta1.texto = "Cargado de " + Archivo1.ruta


def Boton1_click():
    Archivo1.escribir(CuadroTexto1.texto)
    Etiqueta1.texto = "Guardado (%d letras)" % len(CuadroTexto1.texto)


def Boton2_click():
    CuadroTexto1.texto = Archivo1.leer()
    Etiqueta1.texto = "Cargado"


def Boton3_click():
    Etiqueta1.texto = "El archivo tiene %d líneas" % len(Archivo1.leer_lineas())
