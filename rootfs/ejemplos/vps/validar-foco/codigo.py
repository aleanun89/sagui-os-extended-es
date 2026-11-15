# Validar al perder el foco: «validar» se puede cancelar y el foco vuelve al cuadro.
# «intro» salta al pulsar Intro dentro de un cuadro de texto.

def CuadroTexto1_validar():
    t = CuadroTexto1.texto.strip()
    if not t.isdigit() or int(t) > 120:
        Etiqueta3.texto = "«%s» no es una edad válida" % t
        CuadroTexto1.cancelar = True        # el foco vuelve al cuadro
    else:
        Etiqueta3.texto = "Edad correcta: %s" % t


def CuadroTexto2_intro():
    Etiqueta4.texto = "Hola, " + CuadroTexto2.texto
