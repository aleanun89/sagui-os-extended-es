# SPDX-License-Identifier: GPL-2.0-or-later
# Visor de imágenes: el diálogo de abrir del sistema y un control Imagen.

def Boton1_click():
    ruta = dialogo_abrir("Abrir imagen", tipos=[("Imágenes", "*.bmp *.png *.ppm"), ("Todo", "*")])
    if ruta:
        Imagen1.archivo = ruta
        Etiqueta1.texto = ruta
