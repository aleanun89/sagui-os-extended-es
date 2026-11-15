# Numerico: un cuadro con flechas. Aquí el total se recalcula con cada cambio.

def calcular():
    Etiqueta3.texto = "Total: %.2f" % (Numerico1.valor * Numerico2.valor)


def Formulario_cargar():
    calcular()


def Numerico1_cambio():
    calcular()


def Numerico2_cambio():
    calcular()
