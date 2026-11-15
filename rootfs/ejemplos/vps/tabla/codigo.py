# Tabla: filas y columnas. Un clic elige una fila, un clic en la cabecera ordena y, con `editable`,
# un doble clic edita la celda.

def Tabla1_seleccion():
    Etiqueta1.texto = "Elegida: " + ", ".join([str(x) for x in Tabla1.valor])


def Tabla1_cambio():
    Etiqueta1.texto = "Celda cambiada"


def Boton1_click():
    Tabla1.agregar(["Nueva", 0, "-"])


def Boton2_click():
    i = Tabla1.seleccion
    if i >= 0:
        Tabla1.quitar(i)
        Etiqueta1.texto = "Fila quitada"


def Boton3_click():
    Tabla1.guardar_csv("/tmp/personas.csv")
    Etiqueta1.texto = "Guardado en /tmp/personas.csv"
