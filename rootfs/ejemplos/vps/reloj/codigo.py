# Reloj: sin ventana; da la fecha y la hora y dispara «tic» cada segundo.

def mostrar():
    Etiqueta1.texto = Reloj1.hora_texto
    Etiqueta2.texto = "%s %d de %s de %d" % (Reloj1.nombre_dia, Reloj1.dia, Reloj1.nombre_mes, Reloj1.anio)


def Formulario_cargar():
    mostrar()


def Reloj1_tic():
    mostrar()
