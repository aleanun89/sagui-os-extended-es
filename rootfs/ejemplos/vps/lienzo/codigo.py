# Lienzo: dibuja con linea, rectangulo, circulo, texto e imagen; los eventos de ratón dan raton_x y raton_y.
ultimo = None


def dibujo_inicial():
    Lienzo1.rectangulo(16, 16, 120, 70, "azul", "amarillo", 2)
    Lienzo1.circulo(220, 60, 40, "rojo", "rosa", 2)
    Lienzo1.linea(16, 110, 330, 110, "verde", 3)
    Lienzo1.texto(16, 120, "Dibuja aquí con el ratón")


def Formulario_cargar():
    dibujo_inicial()


def Lienzo1_click():
    global ultimo
    ultimo = (Lienzo1.raton_x, Lienzo1.raton_y)


def Lienzo1_arrastrar():
    global ultimo
    x, y = Lienzo1.raton_x, Lienzo1.raton_y
    if ultimo is not None:
        Lienzo1.linea(ultimo[0], ultimo[1], x, y, "negro", 2)
    ultimo = (x, y)


def Lienzo1_soltar():
    global ultimo
    ultimo = None


def Boton1_click():
    Lienzo1.limpiar()
    dibujo_inicial()
