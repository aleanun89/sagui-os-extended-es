# Cronómetro: un Temporizador que dispara «tic» cada 100 ms.
import sagui

inicio = 0
acumulado = 0


def mostrar():
    d = (acumulado + (sagui.ticks_ms() - inicio if Temporizador1.activo else 0)) // 100
    Etiqueta1.texto = "%02d:%02d.%d" % (d // 600, (d // 10) % 60, d % 10)


def Boton1_click():
    global inicio, acumulado
    if Temporizador1.activo:
        acumulado += sagui.ticks_ms() - inicio
        Temporizador1.activo = False
        Boton1.texto = "Iniciar"
    else:
        inicio = sagui.ticks_ms()
        Temporizador1.activo = True
        Boton1.texto = "Parar"
    mostrar()


def Boton2_click():
    global acumulado
    acumulado = 0
    Temporizador1.activo = False
    Boton1.texto = "Iniciar"
    mostrar()


def Temporizador1_tic():
    mostrar()
