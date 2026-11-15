# Un botón que cambia de color con el ratón: raton_entra y raton_sale.
# Los datos del evento están en el control: raton_x, raton_y, boton y rueda_delta.
clics = 0


def Boton1_raton_entra():
    Boton1.colorfondo = AMARILLO
    Etiqueta1.texto = "El ratón entró"


def Boton1_raton_sale():
    Boton1.colorfondo = 0xE2E6EA
    Etiqueta1.texto = "El ratón salió"


def Boton1_click():
    global clics
    clics += 1
    Etiqueta2.texto = "Clics: %d" % clics


def Boton1_doble_click():
    Etiqueta2.texto = "Doble clic en %d, %d" % (Boton1.raton_x, Boton1.raton_y)


def Boton1_rueda():
    Etiqueta3.texto = "Rueda: %d" % Boton1.rueda_delta
