# Agenda por carpetas: un Arbol con las carpetas, un Calendario con el día y una nota por cada pareja.
notas = {}


def clave_nota():
    return "%s|%s" % (Arbol1.clave, Calendario1.valor)


def mostrar_nota():
    if Arbol1.clave is None:
        Etiqueta1.texto = "Elige una carpeta"
        Etiqueta2.texto = ""
        CuadroTexto1.texto = ""
        return
    Etiqueta1.texto = Arbol1.ruta()
    Etiqueta2.texto = Calendario1.valor
    CuadroTexto1.texto = notas.get(clave_nota(), "")
    BarraEstado1.texto = "Notas guardadas: %d" % len(notas)


def Formulario_cargar():
    Calendario1.hoy()
    Arbol1.elegir("Reuniones")
    mostrar_nota()


def Arbol1_seleccion():
    mostrar_nota()


def Arbol1_doble_click():
    Arbol1.alternar(Arbol1.clave)


def Calendario1_cambio():
    mostrar_nota()


def Boton1_click():
    if Arbol1.clave is None:
        notificar("Elige una carpeta primero", "aviso")
        return
    texto = CuadroTexto1.texto.strip()
    if texto:
        notas[clave_nota()] = texto
        notificar("Nota guardada en " + Arbol1.ruta(), "exito")
    else:
        notas.pop(clave_nota(), None)
        notificar("Nota vacía: no se guarda", "info")
    mostrar_nota()


def Boton2_click():
    nombre = inputbox("Nombre de la carpeta nueva:", "Carpeta nueva")
    if nombre:
        Arbol1.elegir(Arbol1.agregar(Arbol1.clave, nombre))
        mostrar_nota()
