# SPDX-License-Identifier: GPL-2.0-or-later
# vps.autoprueba - comprobaciones del módulo vps sin pulsar nada con el ratón (mejora 123, D297):
# construye los proyectos de ejemplo, simula pulsaciones con Boton.pulsar() y escribe una línea
# «vps-autoprueba: ok ...» por cada cosa que sale bien (o «FALLO ...»). Lo usa tests/h297.
#
#     python /lib/python/vps/autoprueba.py
import os
import sys
import vps
from vps import proyecto, _modelo as M


def ok(msg):
    print("vps-autoprueba: ok " + msg)


def fallo(msg):
    print("vps-autoprueba: FALLO " + msg)


def comprobar(cond, msg):
    (ok if cond else fallo)(msg)


def cerrar_todo():
    vps.salir()


def proyecto_de(nombre):
    return proyecto.ejecutar_proyecto("/ejemplos/vps/" + nombre, solo_construir=True)


def principal():
    # --- formato .vpf: leer -> escribir -> leer
    for nombre in ("hola-mundo", "calculadora", "lista-de-tareas", "cronometro", "visor-de-imagenes"):
        t = proyecto.leer_texto("/ejemplos/vps/%s/formulario.vpf" % nombre)
        forms = proyecto.leer_vpf(t)
        again = proyecto.leer_vpf(proyecto.escribir_vpf(forms))
        comprobar(forms == again and len(forms[0]["controles"]) > 1, "vpf %s: leer y escribir dan lo mismo" % nombre)

    # --- calculadora: 6 * 7
    ns = proyecto_de("calculadora")
    ns["CuadroTexto1"].texto = "6"
    ns["CuadroTexto2"].texto = "7"
    ns["Combo1"].texto = "*"
    ns["Boton1"].pulsar()
    comprobar(ns["Etiqueta4"].texto == "Resultado: 42", "calculadora: 6 * 7 = 42")
    ns["Combo1"].texto = "/"
    ns["CuadroTexto2"].texto = "4"
    ns["Boton1"].pulsar()
    comprobar(ns["Etiqueta4"].texto == "Resultado: 1.5", "calculadora: 6 / 4 = 1.5")
    cerrar_todo()

    # --- hola mundo
    ns = proyecto_de("hola-mundo")
    ns["Boton1"].pulsar()
    comprobar(ns["Etiqueta1"].texto == "¡Hola, mundo!", "hola mundo: la etiqueta cambia")
    comprobar(ns["Formulario"].texto == "Hola mundo" and ns["Formulario"].ancho == 300, "formulario: propiedades")
    cerrar_todo()

    # --- lista de tareas: guarda en un fichero y lo vuelve a leer
    ns = proyecto_de("lista-de-tareas")
    archivo = ns["ARCHIVO"]
    try:
        os.remove(archivo)
    except OSError:
        pass
    cerrar_todo()
    ns = proyecto_de("lista-de-tareas")
    ns["CuadroTexto1"].texto = "comprar pan"
    ns["Boton1"].pulsar()
    ns["CuadroTexto1"].texto = "llamar a Ana"
    ns["Boton1"].pulsar()
    comprobar(ns["Lista1"].elementos == ["comprar pan", "llamar a Ana"], "tareas: dos elementos %r" % ns["Lista1"].elementos)
    ns["Lista1"].seleccion = 0
    ns["Boton2"].pulsar()
    comprobar(ns["Lista1"].elementos == ["llamar a Ana"], "tareas: quitar la elegida")
    cerrar_todo()
    ns = proyecto_de("lista-de-tareas")
    ns["Formulario"].mostrar()          # dispara Formulario_cargar, que lee el fichero
    comprobar(ns["Lista1"].elementos == ["llamar a Ana"], "tareas: se recupera del fichero")
    cerrar_todo()
    os.remove(archivo)

    # --- cronómetro: el temporizador
    ns = proyecto_de("cronometro")
    t = ns["Temporizador1"]
    comprobar(not t.activo and t.intervalo == 100, "cronómetro: el temporizador empieza parado")
    ns["Boton1"].pulsar()
    comprobar(t.activo and ns["Boton1"].texto == "Parar", "cronómetro: Iniciar activa el temporizador")
    ns["Temporizador1"]._disparar("tic")
    comprobar(ns["Etiqueta1"].texto.startswith("00:00"), "cronómetro: el tic actualiza la etiqueta")
    ns["Boton2"].pulsar()
    comprobar(not t.activo, "cronómetro: Reiniciar lo para")
    cerrar_todo()

    # --- visor de imágenes
    ns = proyecto_de("visor-de-imagenes")
    ns["Imagen1"].archivo = "/imagenes/sagui.bmp"
    comprobar(ns["Imagen1"].archivo == "/imagenes/sagui.bmp", "visor: el control Imagen acepta un archivo")
    cerrar_todo()

    # --- script sin diseñador: control, decorador y propiedades
    f = vps.Formulario("Prueba", 200, 120)
    e = vps.Etiqueta(f, "uno", 5, 5, nombre="E")
    b = vps.Boton(f, "Ok", 5, 30, nombre="B")
    casilla = vps.Casilla(f, "marca", 5, 60, nombre="C")
    r1 = vps.Opcion(f, "a", 5, 80, nombre="R1")
    r2 = vps.Opcion(f, "b", 80, 80, nombre="R2")
    llamadas = []

    @vps.evento(b, "click")
    def al_pulsar():
        llamadas.append("click")
        e.texto = "dos"
    b.pulsar()
    comprobar(llamadas == ["click"] and e.texto == "dos", "script: @evento y .texto")
    casilla.valor = True
    comprobar(casilla.valor is True, "casilla: valor")
    r1.valor = True
    r2.valor = True
    comprobar(r2.valor and not r1.valor, "opciones del mismo grupo: excluyentes")
    e.visible = False
    e.x = 40
    comprobar(e.visible is False and e.x == 40, "propiedades x y visible")
    errores = []
    vps.error_hook = lambda ex, donde: errores.append((type(ex).__name__, donde))

    def B_click():
        raise ValueError("a propósito")
    vps._formularios[0]._ns = {"B_click": B_click}
    b.pulsar()
    comprobar(errores and errores[0] == ("ValueError", "B_click"), "un error en un manejador se informa y sigue %r" % (errores,))
    vps.error_hook = None
    cerrar_todo()
    # --- el resaltado de Python del editor (usa str.isdigit, no isalnum: MicroPython no lo tiene)
    from vps._editor import realzar, AZUL, VERDE, ROJO, NEGRO
    c = realzar('def f(x): print("a") # fin')
    comprobar(c[0] == AZUL and c[9] == NEGRO and c[-1] == VERDE and ROJO in c, "editor: resaltado de Python")
    print("vps-autoprueba: fin")


if __name__ == "__main__":
    principal()
