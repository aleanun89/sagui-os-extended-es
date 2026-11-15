# Formulario modal: Formulario2.mostrar(modal=True) espera a que se cierre y devuelve
# lo que se le dio a cerrar(valor), como ShowDialog. Los datos de ida van en `datos`.

def Boton1_click():
    nombre = Formulario2.mostrar(modal=True, datos={"sugerido": "Ana"})
    if nombre:
        Etiqueta2.texto = "Hola, " + nombre
    else:
        Etiqueta2.texto = "(cancelado)"


def Formulario2_cargar():
    CuadroTexto1.texto = Formulario2.datos.get("sugerido", "")


def Boton2_click():
    Formulario2.cerrar(CuadroTexto1.texto)


def Boton3_click():
    Formulario2.cerrar()
