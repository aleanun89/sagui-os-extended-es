# SPDX-License-Identifier: GPL-2.0-or-later
# vps.plantillas - los proyectos de ejemplo de VisualPythonS (mejora 123, D297).
# Cada plantilla es (formularios, código): el modelo de proyecto.leer_vpf y el codigo.py.


def _c(clase, nombre, x, y, ancho=None, alto=None, padre=None, **props):
    import vps
    p = dict(props)
    p["x"], p["y"] = x, y
    tam = vps.TIPOS[clase].TAM
    p["ancho"] = ancho if ancho is not None else tam[0]
    p["alto"] = alto if alto is not None else tam[1]
    return {"tipo": clase, "nombre": nombre, "padre": padre, "props": p}


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


def ejemplo_selectores():
    c = [_c("SelectorDeArchivos", "SelectorDeArchivos1", 8, 8, 32, 32, titulo="Abrir una nota",
            filtro="Texto (*.txt)|*.txt|Todos (*.*)|*", extension_por_defecto="txt"),
         _c("Boton", "Boton1", 48, 8, 80, 26, texto="Abrir"),
         _c("Boton", "Boton2", 136, 8, 80, 26, texto="Guardar"),
         _c("CampoCarpeta", "CampoCarpeta1", 224, 10, 190, 22, titulo="Carpeta de trabajo"),
         _c("Etiqueta", "Etiqueta1", 8, 44, 406, 20, texto="Sin archivo"),
         _c("CuadroTexto", "CuadroTexto1", 8, 68, 406, 186, multilinea=True)]
    codigo = '''# Un editor de notas con los selectores de archivos y de carpetas (como el CommonDialog de Visual Basic).
# Los selectores abren por debajo los diálogos comunes del sistema; `archivo` guarda la ruta elegida.

def Boton1_click():
    if SelectorDeArchivos1.abrir():
        f = open(SelectorDeArchivos1.archivo)
        CuadroTexto1.texto = f.read()
        f.close()


def Boton2_click():
    if SelectorDeArchivos1.guardar():
        f = open(SelectorDeArchivos1.archivo, "w")
        f.write(CuadroTexto1.texto)
        f.close()


def SelectorDeArchivos1_seleccionado():
    Etiqueta1.texto = SelectorDeArchivos1.archivo


def SelectorDeArchivos1_cancelado():
    Etiqueta1.texto = "Cancelado"


def CampoCarpeta1_seleccionado():
    SelectorDeArchivos1.carpeta = CampoCarpeta1.texto
'''
    return _f("Editor de notas", 424, 262, c), codigo


def ejemplo_fecha_fuente():
    c = [_c("Calendario", "Calendario1", 8, 8, 196, 170),
         _c("SelectorFecha", "SelectorFecha1", 212, 8, 130, 22),
         _c("ComboFuentes", "ComboFuentes1", 212, 40, 160, 22),
         _c("SelectorDeColor", "SelectorDeColor1", 212, 72, 32, 32, titulo="Color del texto"),
         _c("SelectorDeFuente", "SelectorDeFuente1", 252, 72, 32, 32, titulo="Fuente del texto"),
         _c("Boton", "Boton1", 292, 76, 80, 26, texto="Fuente..."),
         _c("Etiqueta", "Etiqueta1", 212, 116, 170, 60, texto="Hola, mundo")]
    codigo = '''# Calendario, SelectorFecha, ComboFuentes y los selectores de color y de fuente.

def Calendario1_cambio():
    SelectorFecha1.valor = Calendario1.valor


def SelectorFecha1_cambio():
    Calendario1.valor = SelectorFecha1.valor


def ComboFuentes1_seleccion():
    SelectorDeFuente1.familia = ComboFuentes1.texto
    SelectorDeFuente1.aplicar_a(Etiqueta1)


def Boton1_click():
    SelectorDeColor1.mostrar()
    if SelectorDeFuente1.mostrar():
        SelectorDeFuente1.color = SelectorDeColor1.color
        SelectorDeFuente1.aplicar_a(Etiqueta1)
'''
    return _f("Fecha y fuente", 392, 190, c), codigo


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
def ejemplo_guardar_cerrar():
    c = [_c("Etiqueta", "Etiqueta1", 8, 8, 330, 20, texto="Escribe algo y cierra la ventana"),
         _c("CuadroTexto", "CuadroTexto1", 8, 32, 330, 100, multilinea=True),
         _c("Boton", "Boton1", 8, 142, 100, 26, texto="Guardar"),
         _c("Etiqueta", "Etiqueta2", 120, 146, 220, 20, texto="")]
    codigo = '''# Guardar al cerrar: «cerrando» salta al pulsar la X y se puede cancelar con Formulario.cancelar = True.
guardado = True


def CuadroTexto1_cambio():
    global guardado
    guardado = False
    Etiqueta2.texto = "Hay cambios sin guardar"


def Boton1_click():
    global guardado
    guardado = True
    Etiqueta2.texto = "Guardado"


def Formulario_cerrando():
    if not guardado:
        r = msgbox("¿Guardar los cambios?", "Cerrar", botones="si_no_cancelar", icono="pregunta")
        if r == "cancelar":
            Formulario.cancelar = True      # no se cierra
        elif r == "si":
            Boton1_click()


def Formulario_cerrado():
    print("cerrado")
'''
    return _f("Guardar al cerrar", 350, 184, c), codigo


def ejemplo_boton_raton():
    c = [_c("Boton", "Boton1", 24, 24, 160, 40, texto="Pasa el ratón por aquí"),
         _c("Etiqueta", "Etiqueta1", 24, 80, 300, 20, texto=""),
         _c("Etiqueta", "Etiqueta2", 24, 104, 300, 20, texto=""),
         _c("Etiqueta", "Etiqueta3", 24, 128, 300, 20, texto="")]
    codigo = '''# Un botón que cambia de color con el ratón: raton_entra y raton_sale.
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
'''
    return _f("Botón y ratón", 350, 170, c), codigo


def ejemplo_validar_foco():
    c = [_c("Etiqueta", "Etiqueta1", 16, 16, 300, 20, texto="Tu edad (de 0 a 120):"),
         _c("CuadroTexto", "CuadroTexto1", 16, 40, 100, 22),
         _c("Etiqueta", "Etiqueta2", 16, 76, 300, 20, texto="Tu nombre (acaba con Intro):"),
         _c("CuadroTexto", "CuadroTexto2", 16, 100, 200, 22),
         _c("Etiqueta", "Etiqueta3", 16, 136, 300, 20, texto=""),
         _c("Etiqueta", "Etiqueta4", 16, 160, 300, 20, texto="")]
    codigo = '''# Validar al perder el foco: «validar» se puede cancelar y el foco vuelve al cuadro.
# «intro» salta al pulsar Intro dentro de un cuadro de texto.

def CuadroTexto1_validar():
    t = CuadroTexto1.texto.strip()
    if not t.isdigit() or int(t) > 120:
        Etiqueta3.texto = "«%s» no es una edad válida" % t
        CuadroTexto1.cancelar = True        # el foco vuelve al cuadro
    else:
        Etiqueta3.texto = "Edad correcta: %s" % t


def CuadroTexto2_intro():
    Etiqueta4.texto = "Hola, " + CuadroTexto2.texto
'''
    return _f("Validar al perder el foco", 330, 200, c), codigo


def ejemplo_notificaciones():
    c = [_c("Etiqueta", "Etiqueta1", 12, 12, 60, 20, texto="Nota:"),
         _c("CuadroTexto", "CuadroTexto1", 76, 8, 292, 22, texto="Comprar pan"),
         _c("Boton", "Boton1", 12, 44, 110, 28, texto="Guardar"),
         _c("Boton", "Boton2", 130, 44, 110, 28, texto="Guardar mal"),
         _c("Boton", "Boton3", 12, 80, 110, 28, texto="Aviso"),
         _c("Boton", "Boton4", 130, 80, 110, 28, texto="Informar"),
         _c("Etiqueta", "Etiqueta2", 12, 124, 356, 20, texto="Último aviso cerrado: ninguno"),
         _c("Notificacion", "Notificacion1", 340, 76, tipo="exito", titulo="Guardado", duracion=8000,
            accion="Deshacer", texto="Nota guardada")]
    codigo = '''# Notificaciones: avisos tipo toast, no modales, que aparecen en una esquina del formulario, se apilan
# y se van solos. notificar(texto, tipo, duracion, titulo) los crea desde el código; el control
# Notificacion los deja ya configurados (con un botón «Deshacer» que dispara su evento accion).
import os

ARCHIVO = "/home/nota-toast.txt" if os.path.isdir("/home") else "/tmp/nota-toast.txt"
anterior = ""


def leer():
    try:
        f = open(ARCHIVO)
        t = f.read()
        f.close()
        return t
    except OSError:
        return ""


def escribir(ruta, texto):
    f = open(ruta, "w")
    f.write(texto)
    f.close()


def Boton1_click():
    global anterior
    anterior = leer()
    escribir(ARCHIVO, CuadroTexto1.texto)
    Notificacion1.mostrar("Nota guardada")


def Boton2_click():
    try:
        escribir("/no-existe/nota.txt", CuadroTexto1.texto)
    except Exception:
        notificar("No se pudo guardar la nota", "error", 0, "Error al guardar")


def Boton3_click():
    notificar("La nota está vacía", "aviso", 6000, "Atención")


def Boton4_click():
    notificar("Los avisos se apilan en la esquina", "info", 4000)


def Notificacion1_accion():
    escribir(ARCHIVO, anterior)
    CuadroTexto1.texto = anterior
    notificar("Cambio deshecho", "info", 2000)


def Notificacion1_cerrado():
    Etiqueta2.texto = "Último aviso cerrado: " + Notificacion1.motivo
'''
    return _f("Notificaciones", 384, 280, c), codigo


def ejemplo_agenda_carpetas():
    c = [_c("Arbol", "Arbol1", 8, 8, 170, 176,
            nodos=["Trabajo", "  Reuniones", "  Informes", "Personal", "  Familia", "  Salud"]),
         _c("Boton", "Boton2", 8, 190, 170, 26, texto="Carpeta nueva..."),
         _c("Calendario", "Calendario1", 186, 8, 196, 170, minimo="2026-01-01"),
         _c("Boton", "Boton1", 186, 190, 196, 26, texto="Guardar nota"),
         _c("Etiqueta", "Etiqueta1", 392, 8, 160, 20, texto="Elige una carpeta"),
         _c("Etiqueta", "Etiqueta2", 392, 28, 160, 20),
         _c("CuadroTexto", "CuadroTexto1", 392, 52, 160, 164, multilinea=True),
         _c("BarraEstado", "BarraEstado1", 0, 240, 560, 22, texto="Notas guardadas: 0")]
    codigo = '''# Agenda por carpetas: un Arbol con las carpetas, un Calendario con el día y una nota por cada pareja.
notas = {}


def clave_nota():
    return "%s|%s" % (Arbol1.clave, Calendario1.valor)


def mostrar_nota():
    if Arbol1.clave is None:
        Etiqueta1.texto = "Elige una carpeta"
        Etiqueta2.texto = ""
        CuadroTexto1.texto = ""
        return
    Etiqueta1.texto = Arbol1.ruta()
    Etiqueta2.texto = Calendario1.valor
    CuadroTexto1.texto = notas.get(clave_nota(), "")
    BarraEstado1.texto = "Notas guardadas: %d" % len(notas)


def Formulario_cargar():
    Calendario1.hoy()
    Arbol1.elegir("Reuniones")
    mostrar_nota()


def Arbol1_seleccion():
    mostrar_nota()


def Arbol1_doble_click():
    Arbol1.alternar(Arbol1.clave)


def Calendario1_cambio():
    mostrar_nota()


def Boton1_click():
    if Arbol1.clave is None:
        notificar("Elige una carpeta primero", "aviso")
        return
    texto = CuadroTexto1.texto.strip()
    if texto:
        notas[clave_nota()] = texto
        notificar("Nota guardada en " + Arbol1.ruta(), "exito")
    else:
        notas.pop(clave_nota(), None)
        notificar("Nota vacía: no se guarda", "info")
    mostrar_nota()


def Boton2_click():
    nombre = inputbox("Nombre de la carpeta nueva:", "Carpeta nueva")
    if nombre:
        Arbol1.elegir(Arbol1.agregar(Arbol1.clave, nombre))
        mostrar_nota()
'''
    return _f("Agenda por carpetas", 560, 262, c), codigo


def ejemplo_graficos():
    c = [_c("Grafico", "Grafico1", 8, 8, 350, 240, tipo="barras", titulo="Ventas por mes",
            categorias=["Ene", "Feb", "Mar", "Abr", "May", "Jun"], formato="#,##0",
            series=["2025: 120; 150; 90; 180; 140; 200", "2026: 150; 170; 130; 210; 190; 240"]),
         _c("Grafico", "Grafico2", 366, 8, 340, 240, tipo="lineas", titulo="Evolución", suave=True,
            categorias=["L", "M", "X", "J", "V", "S", "D"],
            series=["Visitas: 20; 35; 30; 50; 45; 70; 65", "Altas: 5; 12; 9; 20; 18; 30; 28"]),
         _c("Grafico", "Grafico3", 8, 256, 350, 220, tipo="tarta", titulo="Reparto por zona", valores=True,
            leyenda="derecha", duracion=2500, categorias=["Norte", "Sur", "Este", "Oeste"], series=["Reparto: 40; 25; 20; 15"]),
         _c("Grafico", "Grafico4", 366, 256, 340, 180, tipo="area", titulo="En vivo", ventana=30, puntos=False,
            leyenda="ninguna", minimo="0", maximo="100", series=["Carga: 10"]),
         _c("Etiqueta", "Etiqueta1", 366, 444, 340, 20, texto="Pulsa una barra, un punto o una porción"),
         _c("Boton", "Boton1", 366, 468, 100, 26, texto="Exportar"),
         _c("Boton", "Boton2", 474, 468, 120, 26, texto="Nuevos datos"),
         _c("Temporizador", "Temporizador1", 640, 468, intervalo=400, activo=True)]
    codigo = '''# Gráficos: cuatro Grafico (barras, líneas suaves, tarta y un monitor en vivo con ventana deslizante).
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
'''
    return _f("Gráficos", 720, 520, c), codigo


PLANTILLAS = [("Proyecto vacío", vacia), ("Hola mundo", hola_mundo),
              ("Calculadora sencilla", calculadora), ("Lista de tareas", lista_tareas),
              ("Cronómetro", cronometro), ("Visor de imágenes", visor_imagenes),
              ("Formularios modales", dialogo_modal),
              ("Validar una entrada", validar_entrada), ("Abrir y guardar", abrir_guardar),
              ("Pestañas", ejemplo_pestanas), ("Tabla", ejemplo_tabla), ("Numérico", ejemplo_numerico),
              ("Lienzo", ejemplo_lienzo), ("Enlace", ejemplo_enlace), ("Barra de estado", ejemplo_barra_estado),
              ("Selector de color", ejemplo_selector_color), ("Reloj", ejemplo_reloj), ("Archivo", ejemplo_archivo),
              ("Guardar al cerrar", ejemplo_guardar_cerrar), ("Botón y ratón", ejemplo_boton_raton),
              ("Validar al perder el foco", ejemplo_validar_foco), ("Editor de notas", ejemplo_selectores),
              ("Fecha y fuente", ejemplo_fecha_fuente),
              ("Notificaciones", ejemplo_notificaciones), ("Agenda por carpetas", ejemplo_agenda_carpetas),
              ("Gráficos", ejemplo_graficos)]
