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


def dialogo_modal():
    """Dos formularios: el principal abre el segundo como modal y recibe lo que éste devuelve."""
    f1 = _f("Formularios modales", 320, 130, [
        _c("Etiqueta", "Etiqueta1", 16, 16, 280, 20, texto="Pulsa para pedir un nombre"),
        _c("Boton", "Boton1", 16, 48, 130, 28, texto="Preguntar..."),
        _c("Etiqueta", "Etiqueta2", 16, 88, 280, 20, texto="")])[0]
    f2 = _f("¿Cómo te llamas?", 280, 130, [
        _c("Etiqueta", "Etiqueta3", 16, 12, 240, 20, texto="Escribe tu nombre:"),
        _c("CuadroTexto", "CuadroTexto1", 16, 38, 240, 22),
        _c("Boton", "Boton2", 40, 80, 88, 28, texto="Aceptar"),
        _c("Boton", "Boton3", 148, 80, 88, 28, texto="Cancelar")], nombre="Formulario2")[0]
    codigo = '''# Formulario modal: Formulario2.mostrar(modal=True) espera a que se cierre y devuelve
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
'''
    return [f1, f2], codigo


def validar_entrada():
    c = [_c("Etiqueta", "Etiqueta1", 16, 16, 260, 20, texto="Tu edad (de 0 a 120):"),
         _c("CuadroTexto", "CuadroTexto1", 16, 40, 100, 22),
         _c("Boton", "Boton1", 128, 38, 100, 26, texto="Comprobar"),
         _c("Etiqueta", "Etiqueta2", 16, 80, 260, 20, texto="")]
    codigo = '''# Validar una entrada: si el dato falta o no vale, se avisa con msgbox y el foco vuelve al cuadro.

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
'''
    return _f("Validar una entrada", 290, 120, c), codigo


def abrir_guardar():
    c = [_c("Boton", "Boton1", 8, 8, 110, 26, texto="Abrir..."),
         _c("Boton", "Boton2", 126, 8, 130, 26, texto="Guardar como..."),
         _c("Etiqueta", "Etiqueta1", 8, 40, 400, 20, texto="Sin archivo"),
         _c("CuadroTexto", "CuadroTexto1", 8, 64, 400, 190, multilinea=True)]
    codigo = '''# Abrir y guardar archivos con los diálogos del sistema.

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
'''
    return _f("Abrir y guardar", 416, 262, c), codigo


def ejemplo_pestanas():
    c = [_c("Pestanas", "Pestanas1", 12, 12, 290, 150, elementos=["Datos", "Opciones", "Acerca de"]),
         _c("Etiqueta", "Etiqueta1", 12, 14, 80, 20, padre="Pestanas1", texto="Nombre:"),
         _c("CuadroTexto", "CuadroTexto1", 96, 10, 160, 22, padre="Pestanas1"),
         _c("Casilla", "Casilla1", 12, 20, 200, 20, padre="Pestanas1", pagina=1, texto="Recibir avisos"),
         _c("Casilla", "Casilla2", 12, 48, 200, 20, padre="Pestanas1", pagina=1, texto="Modo oscuro"),
         _c("Etiqueta", "Etiqueta2", 12, 20, 240, 20, padre="Pestanas1", pagina=2, texto="VisualPythonS 8.3.2"),
         _c("Etiqueta", "Etiqueta3", 12, 172, 290, 20, texto="Página: Datos")]
    codigo = '''# Pestañas: cada control está en una página (su propiedad `pagina`); `cambio` salta al cambiar de pestaña.

def Pestanas1_cambio():
    Etiqueta3.texto = "Página: %s (%d)" % (Pestanas1.valor, Pestanas1.seleccion)


def Casilla2_click():
    Pestanas1.colorfondo = 0x404040 if Casilla2.valor else None
'''
    return _f("Pestañas", 316, 200, c), codigo


def ejemplo_tabla():
    c = [_c("Tabla", "Tabla1", 8, 8, 384, 150, columnas=["Nombre", "Edad", "Ciudad"], editable=True,
            filas=["Ana;31;Madrid", "Luis;25;Sevilla", "Marta;42;Bilbao", "Pablo;19;Cadiz"]),
         _c("Boton", "Boton1", 8, 166, 80, 26, texto="Añadir"),
         _c("Boton", "Boton2", 96, 166, 80, 26, texto="Quitar"),
         _c("Boton", "Boton3", 184, 166, 100, 26, texto="Guardar CSV"),
         _c("Etiqueta", "Etiqueta1", 8, 200, 384, 20, texto="Elige una fila")]
    codigo = '''# Tabla: filas y columnas. Un clic elige una fila, un clic en la cabecera ordena y, con `editable`,
# un doble clic edita la celda.

def Tabla1_seleccion():
    Etiqueta1.texto = "Elegida: " + ", ".join([str(x) for x in Tabla1.valor])


def Tabla1_cambio():
    Etiqueta1.texto = "Celda cambiada"


def Boton1_click():
    Tabla1.agregar(["Nueva", 0, "-"])


def Boton2_click():
    i = Tabla1.seleccion
    if i >= 0:
        Tabla1.quitar(i)
        Etiqueta1.texto = "Fila quitada"


def Boton3_click():
    Tabla1.guardar_csv("/tmp/personas.csv")
    Etiqueta1.texto = "Guardado en /tmp/personas.csv"
'''
    return _f("Tabla", 400, 232, c), codigo


def ejemplo_numerico():
    c = [_c("Etiqueta", "Etiqueta1", 16, 16, 90, 20, texto="Cantidad:"),
         _c("Numerico", "Numerico1", 112, 12, 80, 22, valor=2, minimo=1, maximo=10),
         _c("Etiqueta", "Etiqueta2", 16, 48, 90, 20, texto="Precio:"),
         _c("Numerico", "Numerico2", 112, 44, 80, 22, valor=9.5, minimo=0, maximo=100, paso=0.5, decimales=2),
         _c("Etiqueta", "Etiqueta3", 16, 84, 240, 24, texto="")]
    codigo = '''# Numerico: un cuadro con flechas. Aquí el total se recalcula con cada cambio.

def calcular():
    Etiqueta3.texto = "Total: %.2f" % (Numerico1.valor * Numerico2.valor)


def Formulario_cargar():
    calcular()


def Numerico1_cambio():
    calcular()


def Numerico2_cambio():
    calcular()
'''
    return _f("Numérico", 270, 124, c), codigo


def ejemplo_lienzo():
    c = [_c("Lienzo", "Lienzo1", 8, 8, 344, 220),
         _c("Boton", "Boton1", 8, 236, 80, 26, texto="Borrar"),
         _c("Etiqueta", "Etiqueta1", 96, 240, 250, 20, texto="Arrastra el ratón para dibujar")]
    codigo = '''# Lienzo: dibuja con linea, rectangulo, circulo, texto e imagen; los eventos de ratón dan raton_x y raton_y.
ultimo = None


def dibujo_inicial():
    Lienzo1.rectangulo(16, 16, 120, 70, "azul", "amarillo", 2)
    Lienzo1.circulo(220, 60, 40, "rojo", "rosa", 2)
    Lienzo1.linea(16, 110, 330, 110, "verde", 3)
    Lienzo1.texto(16, 120, "Dibuja aquí con el ratón")


def Formulario_cargar():
    dibujo_inicial()


def Lienzo1_click():
    global ultimo
    ultimo = (Lienzo1.raton_x, Lienzo1.raton_y)


def Lienzo1_arrastrar():
    global ultimo
    x, y = Lienzo1.raton_x, Lienzo1.raton_y
    if ultimo is not None:
        Lienzo1.linea(ultimo[0], ultimo[1], x, y, "negro", 2)
    ultimo = (x, y)


def Lienzo1_soltar():
    global ultimo
    ultimo = None


def Boton1_click():
    Lienzo1.limpiar()
    dibujo_inicial()
'''
    return _f("Lienzo", 360, 272, c), codigo


def ejemplo_enlace():
    c = [_c("Enlace", "Enlace1", 16, 16, 240, 20, texto="Documentación de VisualPythonS", url="/ayuda/visualpythons.md"),
         _c("Enlace", "Enlace2", 16, 44, 160, 20, texto="Acerca de...", url=""),
         _c("Etiqueta", "Etiqueta1", 16, 80, 260, 20, texto="Pulsa un enlace")]
    codigo = '''# Enlace: una etiqueta que se pulsa. `url` es un dato que guarda el control para tu programa.

def Enlace1_click():
    Etiqueta1.texto = "Abrirías: " + Enlace1.url


def Enlace2_click():
    msgbox("VisualPythonS para SaguiOS", "Acerca de")
'''
    return _f("Enlace", 290, 120, c), codigo


def ejemplo_barra_estado():
    c = [_c("CuadroTexto", "CuadroTexto1", 8, 8, 344, 130, multilinea=True),
         _c("BarraEstado", "BarraEstado1", 0, 158, 360, 22, texto="Listo", elementos=["0 letras", "1 línea"],
            anclaje="izq|der|abj")]
    codigo = '''# BarraEstado: un mensaje a la izquierda y paneles a la derecha (poner(i, texto) cambia uno).

def CuadroTexto1_cambio():
    t = CuadroTexto1.texto
    BarraEstado1.poner(0, "%d letras" % len(t))
    BarraEstado1.poner(1, "%d líneas" % (t.count("\\n") + 1))
    BarraEstado1.texto = "Editando"
'''
    return _f("Barra de estado", 360, 180, c), codigo


def ejemplo_selector_color():
    c = [_c("Etiqueta", "Etiqueta1", 16, 16, 90, 20, texto="Texto:"),
         _c("SelectorColor", "SelectorColor1", 112, 12, 120, 24, valor=0x0000FF),
         _c("Etiqueta", "Etiqueta2", 16, 48, 90, 20, texto="Fondo:"),
         _c("SelectorColor", "SelectorColor2", 112, 44, 120, 24, valor=0xFFFF00),
         _c("Etiqueta", "Etiqueta3", 16, 84, 240, 36, texto="Texto de ejemplo", alineacion="centro", color=0x0000FF,
            colorfondo=0xFFFF00)]
    codigo = '''# SelectorColor: al pulsarlo abre una paleta; `valor` es el color y `cambio` salta al elegirlo.

def SelectorColor1_cambio():
    Etiqueta3.color = SelectorColor1.valor


def SelectorColor2_cambio():
    Etiqueta3.colorfondo = SelectorColor2.valor
'''
    return _f("Selector de color", 270, 136, c), codigo


def ejemplo_reloj():
    c = [_c("Etiqueta", "Etiqueta1", 16, 16, 240, 30, texto="00:00:00", alineacion="centro"),
         _c("Etiqueta", "Etiqueta2", 16, 54, 240, 20, texto="", alineacion="centro"),
         _c("Reloj", "Reloj1", 240, 80)]
    codigo = '''# Reloj: sin ventana; da la fecha y la hora y dispara «tic» cada segundo.

def mostrar():
    Etiqueta1.texto = Reloj1.hora_texto
    Etiqueta2.texto = "%s %d de %s de %d" % (Reloj1.nombre_dia, Reloj1.dia, Reloj1.nombre_mes, Reloj1.anio)


def Formulario_cargar():
    mostrar()


def Reloj1_tic():
    mostrar()
'''
    return _f("Reloj", 272, 108, c), codigo


def ejemplo_archivo():
    c = [_c("CuadroTexto", "CuadroTexto1", 8, 8, 304, 120, multilinea=True),
         _c("Boton", "Boton1", 8, 136, 80, 26, texto="Guardar"),
         _c("Boton", "Boton2", 96, 136, 80, 26, texto="Cargar"),
         _c("Boton", "Boton3", 184, 136, 128, 26, texto="Contar líneas"),
         _c("Etiqueta", "Etiqueta1", 8, 170, 304, 20, texto=""),
         _c("Archivo", "Archivo1", 270, 200, ruta="/tmp/nota-vps.txt")]
    codigo = '''# Archivo: sin ventana; lee y escribe el archivo de su propiedad `ruta` (texto, líneas o CSV).

def Formulario_cargar():
    if Archivo1.existe():
        CuadroTexto1.texto = Archivo1.leer()
        Etiqueta1.texto = "Cargado de " + Archivo1.ruta


def Boton1_click():
    Archivo1.escribir(CuadroTexto1.texto)
    Etiqueta1.texto = "Guardado (%d letras)" % len(CuadroTexto1.texto)


def Boton2_click():
    CuadroTexto1.texto = Archivo1.leer()
    Etiqueta1.texto = "Cargado"


def Boton3_click():
    Etiqueta1.texto = "El archivo tiene %d líneas" % len(Archivo1.leer_lineas())
'''
    return _f("Archivo", 320, 198, c), codigo


# nombre del menú -> función (los nombres son los de Archivo > Nuevo desde plantilla)
PLANTILLAS = [("Proyecto vacío", vacia), ("Hola mundo", hola_mundo),
              ("Calculadora sencilla", calculadora), ("Lista de tareas", lista_tareas),
              ("Cronómetro", cronometro), ("Visor de imágenes", visor_imagenes),
              ("Formularios modales", dialogo_modal),
              ("Validar una entrada", validar_entrada), ("Abrir y guardar", abrir_guardar),
              ("Pestañas", ejemplo_pestanas), ("Tabla", ejemplo_tabla), ("Numérico", ejemplo_numerico),
              ("Lienzo", ejemplo_lienzo), ("Enlace", ejemplo_enlace), ("Barra de estado", ejemplo_barra_estado),
              ("Selector de color", ejemplo_selector_color), ("Reloj", ejemplo_reloj), ("Archivo", ejemplo_archivo)]
