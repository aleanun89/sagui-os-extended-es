# Validar una entrada: si el dato falta o no vale, se avisa con msgbox y el foco vuelve al cuadro.

def Boton1_click():
    texto = CuadroTexto1.texto.strip()
    if not texto:
        msgbox("Escribe tu edad.", "Falta un dato", icono="aviso")
        CuadroTexto1.enfocar()
        return
    try:
        edad = int(texto)
    except ValueError:
        msgbox("«%s» no es un número entero." % texto, "Dato no válido", icono="error")
        CuadroTexto1.enfocar()
        return
    if edad < 0 or edad > 120:
        r = msgbox("%d años parece raro. ¿Lo usamos de todos modos?" % edad, "Comprueba el dato",
                   botones="si_no", icono="pregunta")
        if r != "si":
            return
    Etiqueta2.texto = "Edad guardada: %d" % edad
