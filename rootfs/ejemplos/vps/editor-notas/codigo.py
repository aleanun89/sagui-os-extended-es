# Un editor de notas con los selectores de archivos y de carpetas (como el CommonDialog de Visual Basic).
# Los selectores abren por debajo los diálogos comunes del sistema; `archivo` guarda la ruta elegida.

def Boton1_click():
    if SelectorDeArchivos1.abrir():
        f = open(SelectorDeArchivos1.archivo)
        CuadroTexto1.texto = f.read()
        f.close()


def Boton2_click():
    if SelectorDeArchivos1.guardar():
        f = open(SelectorDeArchivos1.archivo, "w")
        f.write(CuadroTexto1.texto)
        f.close()


def SelectorDeArchivos1_seleccionado():
    Etiqueta1.texto = SelectorDeArchivos1.archivo


def SelectorDeArchivos1_cancelado():
    Etiqueta1.texto = "Cancelado"


def CampoCarpeta1_seleccionado():
    SelectorDeArchivos1.carpeta = CampoCarpeta1.texto
