# Pestañas: cada control está en una página (su propiedad `pagina`); `cambio` salta al cambiar de pestaña.

def Pestanas1_cambio():
    Etiqueta3.texto = "Página: %s (%d)" % (Pestanas1.valor, Pestanas1.seleccion)


def Casilla2_click():
    Pestanas1.colorfondo = 0x404040 if Casilla2.valor else None
