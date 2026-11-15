# Abrir y guardar archivos con los diálogos del sistema.

def Boton1_click():
    ruta = dialogo_abrir("Abrir un texto", tipos=[("Texto", "*.txt"), ("Todo", "*")])
    if ruta:
        f = open(ruta)
        CuadroTexto1.texto = f.read()
        f.close()
        Etiqueta1.texto = ruta


def Boton2_click():
    ruta = dialogo_guardar("Guardar como", nombre="nota.txt", tipos=[("Texto", "*.txt")])
    if ruta:
        f = open(ruta, "w")
        f.write(CuadroTexto1.texto)
        f.close()
        Etiqueta1.texto = "Guardado en " + ruta
