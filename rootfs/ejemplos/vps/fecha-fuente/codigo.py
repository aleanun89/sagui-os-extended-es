# Calendario, SelectorFecha, ComboFuentes y los selectores de color y de fuente.

def Calendario1_cambio():
    SelectorFecha1.valor = Calendario1.valor


def SelectorFecha1_cambio():
    Calendario1.valor = SelectorFecha1.valor


def ComboFuentes1_seleccion():
    SelectorDeFuente1.familia = ComboFuentes1.texto
    SelectorDeFuente1.aplicar_a(Etiqueta1)


def Boton1_click():
    SelectorDeColor1.mostrar()
    if SelectorDeFuente1.mostrar():
        SelectorDeFuente1.color = SelectorDeColor1.color
        SelectorDeFuente1.aplicar_a(Etiqueta1)
