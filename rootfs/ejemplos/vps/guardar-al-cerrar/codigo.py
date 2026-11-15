# Guardar al cerrar: «cerrando» salta al pulsar la X y se puede cancelar con Formulario.cancelar = True.
guardado = True


def CuadroTexto1_cambio():
    global guardado
    guardado = False
    Etiqueta2.texto = "Hay cambios sin guardar"


def Boton1_click():
    global guardado
    guardado = True
    Etiqueta2.texto = "Guardado"


def Formulario_cerrando():
    if not guardado:
        r = msgbox("¿Guardar los cambios?", "Cerrar", botones="si_no_cancelar", icono="pregunta")
        if r == "cancelar":
            Formulario.cancelar = True      # no se cierra
        elif r == "si":
            Boton1_click()


def Formulario_cerrado():
    print("cerrado")
