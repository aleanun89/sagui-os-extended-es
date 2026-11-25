# Gráficos: cuatro Grafico (barras, líneas suaves, tarta y un monitor en vivo con ventana deslizante).
# Cada Grafico se anima solo al aparecer y al cambiar los datos; el ratón resalta y enseña un tooltip, un clic
# lanza «click» con serie, indice, valor y categoria, y pulsar la leyenda oculta o muestra una serie.
vivo = [12, 18, 25, 40, 55, 70, 62, 48, 35, 30, 44, 60, 82, 90, 76, 58, 41, 28, 20, 33]
paso = 0
juego = 0
JUEGOS = [[200, 120, 260, 90, 300, 150], [90, 180, 120, 240, 160, 220]]


def Grafico1_click():
    Etiqueta1.texto = "%s, %s: %s" % (Grafico1.nombre_serie, Grafico1.categoria, Grafico1.valor)
    print("grafico-click: %s %d %d %g" % (Grafico1.nombre, Grafico1.serie, Grafico1.indice, Grafico1.valor))


def Grafico2_click():
    Etiqueta1.texto = "%s el %s: %s" % (Grafico2.nombre_serie, Grafico2.categoria, Grafico2.valor)


def Grafico3_click():
    Etiqueta1.texto = "%s: %s" % (Grafico3.categoria, Grafico3.valor)


def Grafico1_pasar():
    Etiqueta1.texto = "Sobre %s (%s)" % (Grafico1.categoria, Grafico1.nombre_serie)


def Grafico1_leyenda():
    print("grafico-leyenda: serie %d oculta=%s" % (Grafico1.serie, Grafico1.oculta))


def Temporizador1_tic():
    global paso
    Grafico4.agregar_punto(vivo[paso % len(vivo)])
    paso += 1


def Boton1_click():
    Grafico1.exportar("/tmp/grafico-ventas.png", 640, 400)
    Etiqueta1.texto = "Exportado a /tmp/grafico-ventas.png"
    print("grafico-exportado")


def Boton2_click():
    global juego
    juego = 1 - juego
    Grafico1.poner_datos(JUEGOS[juego], 1)
    Grafico3.poner_datos([10 + 30 * juego, 25, 20 + 20 * (1 - juego), 15])
