# SPDX-License-Identifier: GPL-2.0-or-later
# vps.plantillas - los proyectos de ejemplo de VisualPythonS (mejora 123, D297).
# Cada plantilla es (formularios, código): el modelo de proyecto.leer_vpf y el codigo.py.


def _c(tipo, nombre, x, y, ancho=None, alto=None, padre=None, **props):
    import vps
    p = dict(props)
    p["x"], p["y"] = x, y
    tam = vps.TIPOS[tipo].TAM
    p["ancho"] = ancho if ancho is not None else tam[0]
    p["alto"] = alto if alto is not None else tam[1]
    return {"tipo": tipo, "nombre": nombre, "padre": padre, "props": p}


def _f(texto, ancho, alto, controles, nombre="Formulario"):
    return [{"nombre": nombre, "props": {"texto": texto, "ancho": ancho, "alto": alto},
             "controles": controles}]


def vacia():
    return _f("Formulario", 320, 200, []), ""


def hola_mundo():
    c = [_c("Etiqueta", "Etiqueta1", 20, 20, 250, 20, texto="Pulsa el botón"),
         _c("Boton", "Boton1", 20, 56, 120, 28, texto="Saludar")]
    codigo = '''# Hola mundo: al pulsar el botón cambia el texto de la etiqueta.

def Boton1_click():
    Etiqueta1.texto = "¡Hola, mundo!"
'''
    return _f("Hola mundo", 300, 130, c), codigo


def calculadora():
    c = [_c("Etiqueta", "Etiqueta1", 16, 16, 80, 20, texto="Primero:"),
         _c("CuadroTexto", "CuadroTexto1", 104, 12, 120, 22, texto="0"),
         _c("Etiqueta", "Etiqueta2", 16, 48, 80, 20, texto="Segundo:"),
         _c("CuadroTexto", "CuadroTexto2", 104, 44, 120, 22, texto="0"),
         _c("Etiqueta", "Etiqueta3", 16, 80, 80, 20, texto="Operación:"),
         _c("Combo", "Combo1", 104, 76, 120, 22, elementos=["+", "-", "*", "/"], texto="+"),
         _c("Boton", "Boton1", 16, 112, 100, 28, texto="Calcular"),
         _c("Etiqueta", "Etiqueta4", 16, 152, 210, 20, texto="Resultado: ")]
    codigo = '''# Calculadora sencilla: dos números, una operación y el botón Calcular.

def Boton1_click():
    try:
        a = float(CuadroTexto1.texto)
        b = float(CuadroTexto2.texto)
    except ValueError:
        msgbox("Escribe dos números.", "Calculadora", icono="aviso")
        return
    op = Combo1.texto
    if op == "+":
        r = a + b
    elif op == "-":
        r = a - b
    elif op == "*":
        r = a * b
    else:
        r = a / b
    Etiqueta4.texto = "Resultado: %g" % r
'''
    return _f("Calculadora", 250, 190, c), codigo


def lista_tareas():
    c = [_c("CuadroTexto", "CuadroTexto1", 12, 12, 170, 22),
         _c("Boton", "Boton1", 190, 10, 80, 26, texto="Añadir"),
         _c("Lista", "Lista1", 12, 44, 258, 130),
         _c("Boton", "Boton2", 12, 184, 120, 26, texto="Quitar la elegida"),
         _c("Etiqueta", "Etiqueta1", 140, 188, 130, 20, texto="0 tareas")]
    codigo = '''# Lista de tareas: se guarda en tareas.txt (en /home, o en /tmp si no hay /home) cada vez que cambia.
import os

ARCHIVO = "/home/tareas.txt" if os.path.isdir("/home") else "/tmp/tareas.txt"


def guardar():
    f = open(ARCHIVO, "w")
    f.write("\\n".join(Lista1.elementos))
    f.close()
    Etiqueta1.texto = "%d tareas" % Lista1.contar()


def Formulario_cargar():
    try:
        f = open(ARCHIVO)
        for linea in f.read().split("\\n"):
            if linea:
                Lista1.agregar(linea)
        f.close()
    except OSError:
        pass
    Etiqueta1.texto = "%d tareas" % Lista1.contar()


def Boton1_click():
    texto = CuadroTexto1.texto.strip()
    if texto:
        Lista1.agregar(texto)
        CuadroTexto1.texto = ""
        guardar()


def Boton2_click():
    i = Lista1.seleccion
    if i >= 0:
        Lista1.quitar(i)
        guardar()
'''
    return _f("Lista de tareas", 284, 224, c), codigo


def cronometro():
    c = [_c("Etiqueta", "Etiqueta1", 40, 16, 180, 24, texto="00:00.0", alineacion="centro"),
         _c("Boton", "Boton1", 12, 52, 80, 28, texto="Iniciar"),
         _c("Boton", "Boton2", 100, 52, 80, 28, texto="Reiniciar"),
         _c("Temporizador", "Temporizador1", 190, 52, intervalo=100, activo=False)]
    codigo = '''# Cronómetro: un Temporizador que dispara «tic» cada 100 ms.
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
'''
    return _f("Cronómetro", 260, 100, c), codigo


def visor_imagenes():
    c = [_c("Boton", "Boton1", 8, 8, 100, 26, texto="Abrir imagen..."),
         _c("Etiqueta", "Etiqueta1", 116, 12, 260, 20, texto="Ninguna imagen"),
         _c("Imagen", "Imagen1", 8, 42, 368, 268, ajustar=True)]
    codigo = '''# Visor de imágenes: el diálogo de abrir del sistema y un control Imagen.

def Boton1_click():
    ruta = dialogo_abrir("Abrir imagen", tipos=[("Imágenes", "*.bmp *.png *.ppm"), ("Todo", "*")])
    if ruta:
        Imagen1.archivo = ruta
        Etiqueta1.texto = ruta
'''
    return _f("Visor de imágenes", 384, 318, c), codigo


# nombre del menú -> función (los nombres son los de Archivo > Nuevo desde plantilla)
PLANTILLAS = [("Proyecto vacío", vacia), ("Hola mundo", hola_mundo),
              ("Calculadora sencilla", calculadora), ("Lista de tareas", lista_tareas),
              ("Cronómetro", cronometro), ("Visor de imágenes", visor_imagenes)]
