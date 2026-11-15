# BarraEstado: un mensaje a la izquierda y paneles a la derecha (poner(i, texto) cambia uno).

def CuadroTexto1_cambio():
    t = CuadroTexto1.texto
    BarraEstado1.poner(0, "%d letras" % len(t))
    BarraEstado1.poner(1, "%d líneas" % (t.count("\n") + 1))
    BarraEstado1.texto = "Editando"
