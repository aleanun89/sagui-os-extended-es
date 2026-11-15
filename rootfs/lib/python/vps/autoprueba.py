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
    avanzado()
    componentes()
    eventos()
    explorador()
    print("vps-autoprueba: fin")


def avanzado():
    """Lo de la v8.3.2 (D299): formularios modales, anclajes, orden de tabulación, fuentes,
    autocompletado, refactorización y depurador."""
    import tkinter as tk
    from vps import _refactor, _autocompletar as AC, _depurador as D, _fuente, _historial

    # --- formulario modal: devuelve un valor y recibe datos
    ns = proyecto_de("formularios-modales")
    ns["Formulario"].mostrar()
    f2 = ns["Formulario2"]
    comprobar(f2.CuadroTexto1 is ns["CuadroTexto1"], "formulario: los controles son atributos (Formulario2.CuadroTexto1)")
    vistos = []

    def aceptar():
        vistos.append(ns["CuadroTexto1"].texto)       # lo que puso Formulario2_cargar desde `datos`
        ns["CuadroTexto1"].texto = "Luis"
        ns["Boton2"].pulsar()
    tk._raiz_padrao().after(300, aceptar)
    ns["Boton1"].pulsar()                                  # Boton1_click abre Formulario2 como modal
    comprobar(vistos == ["Ana"], "modal: los datos de ida llegan a Formulario2_cargar %r" % (vistos,))
    comprobar(ns["Etiqueta2"].texto == "Hola, Luis", "modal: mostrar(modal=True) devuelve lo de cerrar(valor): %r" % ns["Etiqueta2"].texto)
    tk._raiz_padrao().after(300, lambda: ns["Boton3"].pulsar())
    ns["Boton1"].pulsar()
    comprobar(ns["Etiqueta2"].texto == "(cancelado)", "modal: cerrar() sin valor devuelve None y se puede volver a abrir")
    tk._raiz_padrao().after(300, lambda: f2._cerrar())
    comprobar(f2.mostrar(modal=True) is None, "modal: la X de la ventana devuelve None")
    comprobar(f2._oculto and not f2._cerrado, "modal: un formulario secundario se oculta, no se destruye")
    cerrar_todo()

    # --- anclajes: el formulario crece y los controles se ajustan
    f = vps.Formulario("Anclas", 300, 200)
    marco = vps.Marco(f, "M", 10, 10, 200, 100, nombre="M1", anclaje="izq|arr|der|abj")
    dentro = vps.Boton(marco, "ok", 150, 70, 40, 20, nombre="BD", anclaje="der|abj")
    abajo = vps.Boton(f, "abajo", 10, 170, 80, 24, nombre="B1", anclaje="izq|abj")
    ancho = vps.CuadroTexto(f, "", 10, 150, 280, 20, nombre="T1", anclaje="izq|der|arr")
    quieto = vps.Etiqueta(f, "q", 5, 5, 40, 16, nombre="Q")
    f.ancho = 400
    f.alto = 300
    comprobar((marco.ancho, marco.alto) == (300, 200), "anclaje: el Marco con las cuatro se estira %r" % ((marco.ancho, marco.alto),))
    comprobar((dentro.x, dentro.y) == (250, 170), "anclaje: el hijo del Marco sigue la esquina %r" % ((dentro.x, dentro.y),))
    comprobar((abajo.x, abajo.y) == (10, 270), "anclaje: izq|abj baja con el borde %r" % ((abajo.x, abajo.y),))
    comprobar((ancho.ancho, ancho.y) == (380, 150), "anclaje: izq|der|arr se estira a lo ancho %r" % ((ancho.ancho, ancho.y),))
    comprobar((quieto.x, quieto.y, quieto.ancho) == (5, 5, 40), "anclaje: sin anclaje no cambia")
    f.ancho = 300
    f.alto = 200
    comprobar((abajo.y, ancho.ancho, dentro.x) == (170, 280, 150), "anclaje: al volver a achicar vuelve donde estaba")
    cerrar_todo()

    # --- orden de tabulación
    f = vps.Formulario("Tab", 300, 120)
    b1 = vps.Boton(f, "uno", 10, 10, nombre="B1", orden_tab=2)
    b2 = vps.Boton(f, "dos", 100, 10, nombre="B2", orden_tab=1)
    b3 = vps.Boton(f, "tres", 190, 10, nombre="B3", orden_tab=3)
    f.mostrar()
    orden = [c.nombre for w in tk._focaveis(f.win, []) for c in (b1, b2, b3) if c.w is w]
    comprobar(orden == ["B2", "B1", "B3"], "orden de tabulación: por orden_tab %r" % (orden,))
    cerrar_todo()

    # --- fuentes: tamaño con escalado suave, negrita
    comprobar(_fuente.es_nativa("") and _fuente.es_nativa("DejaVu Sans 12") and not _fuente.es_nativa("DejaVu Sans 14"),
              "fuente: 12 es la normal")
    fam, tam, neg, cur, sub = _fuente.analizar("DejaVu Sans 18 negrita subrayado")
    comprobar((fam, tam, neg, sub) == ("DejaVu Sans", 18, True, True), "fuente: se analiza familia, tamaño y estilo")
    f = vps.Formulario("Fuentes", 300, 120)
    e0 = vps.Etiqueta(f, "Hola", 10, 10, 120, 40, nombre="E0")
    e1 = vps.Etiqueta(f, "Hola", 10, 50, 120, 40, nombre="E1", fuente="DejaVu Sans 24 negrita")
    b = vps.Boton(f, "Grande", 140, 10, 140, 50, nombre="B", fuente="Sans 20")
    comprobar(e0._foto is None and e1._foto is not None, "fuente: la Etiqueta con fuente usa una imagen")
    comprobar(e1._foto.width() == 2 * (4 * 8 + 1) and e1._foto.height() == 32,
              "fuente: 24 duplica el tamaño (%dx%d)" % (e1._foto.width(), e1._foto.height()))
    comprobar(b._foto is not None and b._foto.height() > 16, "fuente: el Boton escala su texto")
    e1.fuente = ""
    comprobar(e1._foto is None, "fuente: vaciarla devuelve el texto normal")
    cerrar_todo()

    # --- refactorización por tokens
    cod = ('def Boton1_click():\n    Boton1.texto = "Boton1 listo"  # Boton1\n    otro.Boton1 = 3\n'
           '    print(f"{Boton1.texto} {{Boton1}}")\n    g(Boton1=2)\n')
    nuevo, cambios, conf = _refactor.renombrar(cod, "Boton1", "Aceptar", ["click"])
    comprobar("def Aceptar_click():" in nuevo and "Aceptar.texto = " in nuevo, "renombrar: manejador y referencia")
    comprobar('"Boton1 listo"' in nuevo and "# Boton1" in nuevo, "renombrar: respeta cadenas y comentarios")
    comprobar("otro.Boton1" in nuevo and "g(Boton1=2)" in nuevo and "{{Boton1}}" in nuevo, "renombrar: respeta atributos, argumentos y llaves dobles")
    comprobar('f"{Aceptar.texto}' in nuevo, "renombrar: cambia lo de dentro de una f-cadena")
    comprobar([c[0] for c in cambios] == [1, 2, 4], "renombrar: lista de líneas cambiadas %r" % ([c[0] for c in cambios],))
    comprobar(_refactor.renombrar("Boton1.x = 1\n", "Boton1", "Aceptar", ["click"])[2] == [], "renombrar: sin conflicto")
    comprobar(_refactor.renombrar("Aceptar.x = 1\nBoton1.x = 2\n", "Boton1", "Aceptar", [])[2] == ["Aceptar"],
              "renombrar: avisa si el nombre nuevo ya existe")
    comprobar(_refactor.renombrar("def Aceptar_click(): pass\ndef Boton1_click(): pass\n", "Boton1", "Aceptar", ["click"])[2] == ["Aceptar_click"],
              "renombrar: avisa si el manejador nuevo ya existe")

    # --- autocompletado
    forms = [{"nombre": "Formulario", "props": {}, "controles": [
        {"tipo": "Boton", "nombre": "Boton1", "padre": None, "props": {}},
        {"tipo": "Lista", "nombre": "Lista1", "padre": None, "props": {}}]}]
    pre, cand = AC.candidatos("    Boton1.", forms, "Formulario")
    nombres = [c[0] for c in cand]
    comprobar(pre == "" and "texto" in nombres and "pulsar" in nombres and "conectar(\"click\", " in nombres,
              "autocompletar: Boton1. ofrece propiedades, métodos y eventos")
    pre, cand = AC.candidatos("x = Boton1.te", forms, "Formulario")
    comprobar(pre == "te" and [c[0] for c in cand] == ["texto"], "autocompletar: filtra por lo escrito")
    pre, cand = AC.candidatos("vps.ms", forms, "Formulario")
    comprobar([c[0] for c in cand] == ["msgbox"], "autocompletar: vps. ofrece la API")
    pre, cand = AC.candidatos("    Formulario.", forms, "Formulario")
    comprobar("Boton1" in [c[0] for c in cand] and "mostrar" in [c[0] for c in cand], "autocompletar: un formulario ofrece sus controles y métodos")
    comprobar("agregar(texto)" in AC.ayuda_llamada("Lista1.agregar(", forms), "ayuda: la firma de Lista1.agregar")
    comprobar("msgbox(texto" in AC.ayuda_llamada("x = vps.msgbox(", forms), "ayuda: la firma de vps.msgbox")
    comprobar(AC.candidatos("# Boton1.", forms, "Formulario")[1] == [], "autocompletar: nada en un comentario")

    # --- depurador: instrumentar, parar en un punto y paso a paso
    fuente = ('def f(a):\n    b = a * 2\n    c = (b +\n         1)\n    """texto\n    largo"""\n    return c\n'
              '\nr = f(3)\ns = r + 1\n')
    marcadas = D.lineas_de_sentencia(fuente)
    comprobar(marcadas == [2, 3, 5, 7, 9, 10], "depurador: solo las sentencias, no las continuaciones %r" % (marcadas,))
    ses = D.Sesion(fuente, puntos=(3,), decisor=lambda n, v: "continuar")
    ns2 = {D.NOMBRE: ses.hook}
    exec(compile(ses.codigo, "codigo.py", "exec"), ns2)
    comprobar(ns2["s"] == 8 and [p[0] for p in ses.paradas] == [3], "depurador: el código instrumentado funciona y para en la línea 3")
    vs = dict(ses.paradas[0][1])
    comprobar(vs.get("a") == "3" and vs.get("b") == "6", "depurador: enseña las variables locales %r" % (vs,))
    ses = D.Sesion(fuente, paso=True, decisor=lambda n, v: "paso")
    ns2 = {D.NOMBRE: ses.hook}
    exec(compile(ses.codigo, "codigo.py", "exec"), ns2)
    comprobar([p[0] for p in ses.paradas] == [9, 2, 3, 5, 7, 10], "depurador: paso a paso recorre las sentencias en orden %r" % ([p[0] for p in ses.paradas],))
    ses = D.Sesion(fuente)
    comprobar(D.linea_original(ses.mapa, 3) == 2 and D.linea_original(ses.mapa, 1) == 1, "depurador: el mapa devuelve las líneas originales")
    h = _historial.Historial()
    h.punto("a")
    h.punto("b")
    comprobar(h.paso_atras("c") == "b" and h.paso_adelante("b") == "c" and h.paso_atras("d") == "b", "historial: deshacer y rehacer")


class _Ev:
    """Un evento de ratón falso para llamar a los manejadores de los controles sin el ratón."""
    def __init__(self, x, y):
        self.x, self.y, self.delta, self.keysym, self.char, self.state = x, y, 0, "", "", 0


def componentes():
    """Los componentes de la v8.3.2 (D303): Pestanas, Tabla, Numerico, Lienzo, Enlace, BarraEstado,
    SelectorColor, Reloj y Archivo, sus ejemplos y el autocompletado de sintaxis."""
    from vps import _autocompletar as AC
    # --- el cronómetro: el Temporizador hace avanzar la hora
    import sagui
    import tkinter
    ns = proyecto_de("cronometro")
    ns["Formulario"].mostrar()
    ns["Boton1"].pulsar()
    fin = sagui.ticks_ms() + 700
    while sagui.ticks_ms() < fin:
        tkinter._iteracao(False)
        sagui.dormir(20)
    comprobar(ns["Etiqueta1"].texto != "00:00.0" and ns["Boton1"].texto == "Parar", "cronómetro: el Temporizador dispara tic %r" % ns["Etiqueta1"].texto)
    cerrar_todo()

    # --- Pestanas: solo se ven los controles de la página elegida
    ns = proyecto_de("pestanas")
    p = ns["Pestanas1"]
    comprobar(p.seleccion == 0 and ns["CuadroTexto1"].w._gestor is not None and ns["Casilla1"].w._gestor is None,
              "pestañas: al principio solo se ve la página 0")
    tr = vps._pest_tramos(p.elementos)
    p._clic(_Ev(tr[1][0] + 4, 8))
    comprobar(p.seleccion == 1 and ns["Casilla1"].w._gestor is not None and ns["CuadroTexto1"].w._gestor is None,
              "pestañas: un clic en la segunda cambia de página")
    comprobar(ns["Etiqueta3"].texto == "Página: Opciones (1)", "pestañas: el evento cambio %r" % ns["Etiqueta3"].texto)
    p.seleccion = 2
    comprobar(p.valor == "Acerca de" and ns["Etiqueta2"].w._gestor is not None, "pestañas: seleccion por código")
    p.seleccion = 9
    comprobar(p.seleccion == 2, "pestañas: la selección se limita a las páginas que hay")
    p._clic(_Ev(20, 80))        # un clic en el cuerpo no cambia nada
    comprobar(p.seleccion == 2, "pestañas: un clic en el cuerpo no cambia de página")
    cerrar_todo()

    # --- Tabla
    ns = proyecto_de("tabla")
    t = ns["Tabla1"]
    comprobar(t.contar() == 4 and t.columnas == ["Nombre", "Edad", "Ciudad"] and t.filas[1] == ["Luis", "25", "Sevilla"],
              "tabla: columnas y filas desde el diseñador")
    t._pulsado(2, 1)
    comprobar(t.seleccion == 2 and t.valor[0] == "Marta" and ns["Etiqueta1"].texto == "Elegida: Marta, 42, Bilbao",
              "tabla: un clic elige la fila y dispara seleccion %r" % ns["Etiqueta1"].texto)
    t._pulsado(-1, 1)
    comprobar([f[0] for f in t.filas] == ["Pablo", "Luis", "Ana", "Marta"] and t.seleccion == 3, "tabla: clic en la cabecera ordena (la elegida se sigue) %r" % ([f[0] for f in t.filas],))
    t._pulsado(-1, 1)
    comprobar(t.filas[0][0] == "Marta", "tabla: otro clic ordena al revés")
    t.agregar(["Eva", 7, "Leon"])
    t.poner_celda(0, 2, "Vigo")
    comprobar(t.contar() == 5 and t.celda(0, 2) == "Vigo", "tabla: agregar y poner_celda")
    t.w.editar(1, 0)
    t.w.ent[0].delete(0, "end")
    t.w.ent[0].insert(0, "Zoe")
    t.w.cerrar_edicion(True)
    comprobar(t.celda(1, 0) == "Zoe" and ns["Etiqueta1"].texto == "Celda cambiada", "tabla: editar una celda y evento cambio")
    t.quitar(0)
    comprobar(t.contar() == 4, "tabla: quitar")
    ns["Boton3"].pulsar()
    t.limpiar()
    t.cargar_csv("/tmp/personas.csv")
    comprobar(t.columnas == ["Nombre", "Edad", "Ciudad"] and t.contar() == 4, "tabla: guardar_csv y cargar_csv dan lo mismo %r" % (t.filas,))
    cerrar_todo()

    # --- Numerico
    ns = proyecto_de("numerico")
    n1, n2 = ns["Numerico1"], ns["Numerico2"]
    ns["Formulario"].mostrar()
    comprobar(n1.valor == 2 and n2.valor == 9.5 and ns["Etiqueta3"].texto == "Total: 19.00", "numerico: valores iniciales y total %r" % ns["Etiqueta3"].texto)
    n1.w.invoke("buttonup")
    comprobar(n1.valor == 3 and ns["Etiqueta3"].texto == "Total: 28.50", "numerico: la flecha de arriba sube y dispara cambio %r" % ns["Etiqueta3"].texto)
    n1.valor = 99
    comprobar(n1.w.get() == "99", "numerico: por código se escribe el valor tal cual")
    n1.w.invoke("buttonup")
    comprobar(n1.valor == 10, "numerico: no pasa del máximo")
    n2.w.invoke("buttondown")
    comprobar(n2.valor == 9.0, "numerico: paso 0.5 con decimales %r" % (n2.valor,))
    cerrar_todo()

    # --- Lienzo
    ns = proyecto_de("lienzo")
    ns["Formulario"].mostrar()
    lz = ns["Lienzo1"]
    antes = len(lz.w.find_all())
    lz.raton_x, lz.raton_y = 10, 10
    lz._raton(_Ev(10, 10), "click")
    lz._raton(_Ev(50, 60), "arrastrar")
    comprobar(len(lz.w.find_all()) == antes + 1 and lz.raton_x == 50, "lienzo: click y arrastrar dibujan una línea %d" % (len(lz.w.find_all()) - antes))
    lz._raton(_Ev(50, 60), "soltar")
    lz._raton(_Ev(80, 80), "arrastrar")
    comprobar(len(lz.w.find_all()) == antes + 1, "lienzo: tras soltar no se sigue dibujando")
    lz.imagen(4, 4, "/imagenes/sagui.bmp")
    lz.limpiar()
    comprobar(len(lz.w.find_all()) == 0, "lienzo: limpiar")
    ns["Boton1"].pulsar()
    comprobar(len(lz.w.find_all()) == 4, "lienzo: el botón Borrar vuelve al dibujo inicial")
    cerrar_todo()

    # --- Enlace, BarraEstado, SelectorColor
    ns = proyecto_de("enlace")
    ns["Enlace1"]._disparar("click")
    comprobar(ns["Etiqueta1"].texto == "Abrirías: /ayuda/visualpythons.md", "enlace: click y url")
    cerrar_todo()
    ns = proyecto_de("barra-estado")
    ns["CuadroTexto1"].texto = "hola\nmundo"
    comprobar(ns["BarraEstado1"].elementos == ["10 letras", "2 líneas"] and ns["BarraEstado1"].texto == "Editando",
              "barra de estado: poner cambia los paneles %r" % (ns["BarraEstado1"].elementos,))
    ns["BarraEstado1"].poner(3, "x")
    comprobar(len(ns["BarraEstado1"].elementos) == 4, "barra de estado: poner amplía los paneles")
    cerrar_todo()
    ns = proyecto_de("selector-color")
    s = ns["SelectorColor1"]
    comprobar(s.valor == 0x0000FF, "selector de color: valor inicial")
    s._p["valor"] = 0xFF0000
    s._aplicar("valor")
    s._disparar("cambio")
    comprobar(ns["Etiqueta3"].color == 0xFF0000 and s.valor == 0xFF0000, "selector de color: el evento cambio")
    cerrar_todo()

    # --- Reloj y Archivo
    ns = proyecto_de("reloj")
    r = ns["Reloj1"]
    comprobar(r.anio >= 2024 and 1 <= r.mes <= 12 and len(r.hora_texto) == 8 and len(r.fecha) == 10, "reloj: fecha y hora %s %s" % (r.fecha, r.hora_texto))
    comprobar(r.formatear("%d/%m/%Y").count("/") == 2 and r.nombre_dia in ("lunes", "martes", "miercoles", "jueves", "viernes", "sabado", "domingo"),
              "reloj: formatear y nombre_dia")
    ns["Formulario"].mostrar()
    comprobar(len(ns["Etiqueta1"].texto) == 8 and ns["Etiqueta1"].texto[2] == ":", "reloj: Formulario_cargar muestra la hora")
    cerrar_todo()
    ns = proyecto_de("archivo")
    a = ns["Archivo1"]
    a.borrar()
    comprobar(not a.existe() and a.leer() == "", "archivo: sin archivo, leer da texto vacío")
    ns["CuadroTexto1"].texto = "uno\ndos\ntres"
    ns["Boton1"].pulsar()
    comprobar(a.existe() and a.leer() == "uno\ndos\ntres" and ns["Etiqueta1"].texto == "Guardado (12 letras)", "archivo: escribir y leer")
    ns["Boton3"].pulsar()
    comprobar(ns["Etiqueta1"].texto == "El archivo tiene 3 líneas", "archivo: leer_lineas")
    a.agregar("\ncuatro")
    comprobar(a.leer_lineas() == ["uno", "dos", "tres", "cuatro"], "archivo: agregar")
    a.escribir_csv([["a", "b;c", "d"], [1, 2, 3]])
    comprobar(a.leer_csv() == [["a", "b;c", "d"], ["1", "2", "3"]], "archivo: CSV con comillas %r" % (a.leer_csv(),))
    a.borrar()
    cerrar_todo()
    for nombre in ("validar-entrada", "abrir-guardar"):
        ns = proyecto_de(nombre)
        comprobar(len(ns["Formulario"].controles) >= 4, "ejemplo %s: se construye" % nombre)
        cerrar_todo()

    # --- autocompletado de sintaxis
    forms = [{"nombre": "Formulario", "props": {}, "controles": [
        {"tipo": "Boton", "nombre": "Boton1", "padre": None, "props": {}}]}]
    codigo = 'def sumar(a, b):\n    """Suma dos numeros"""\n    total = a + b\n    return total\n\nresultado = 5\nclass Perro:\n    pass\n'
    pre, cand = AC.candidatos("wh", forms, "Formulario", codigo, 99)
    comprobar([c[0] for c in cand] == ["while"] and cand[0][1] == "palabra", "sintaxis: una palabra clave (wh -> while)")
    pre, cand = AC.candidatos("    pri", forms, "Formulario", codigo, 99)
    comprobar([c[0] for c in cand] == ["print"] and cand[0][1] == "integrada", "sintaxis: una función integrada (pri -> print)")
    pre, cand = AC.candidatos("x = res", forms, "Formulario", codigo, 99)
    comprobar([c[0] for c in cand] == ["resultado"] and cand[0][1] == "variable", "sintaxis: una variable del propio código")
    pre, cand = AC.candidatos("su", forms, "Formulario", codigo, 99)
    comprobar("sumar" in [c[0] for c in cand], "sintaxis: funciones del programa")
    pre, cand = AC.candidatos("Pe", forms, "Formulario", codigo, 99)
    comprobar([c[0] for c in cand] == ["Perro"], "sintaxis: una clase del propio código")
    pre, cand = AC.candidatos("t", forms, "Formulario", codigo, 99)
    comprobar(cand == [], "sintaxis: con un solo carácter no se ofrece nada")
    pre, cand = AC.candidatos("t", forms, "Formulario", codigo, 99, 0)
    comprobar(len(cand) > 3, "sintaxis: Ctrl+Espacio ofrece con un carácter")
    pre, cand = AC.candidatos('x = "while', forms, "Formulario", codigo, 99)
    comprobar(cand == [], "sintaxis: nada dentro de un texto")
    pre, cand = AC.candidatos("Boton1", forms, "Formulario", codigo, 99)
    comprobar(cand == [], "sintaxis: lo ya escrito entero no se vuelve a ofrecer")
    comprobar("sumar(a, b)" in AC.ayuda_llamada("x = sumar(", forms, codigo) and "Suma dos numeros" in AC.ayuda_llamada("sumar(1,", forms, codigo),
              "sintaxis: la firma de una función del usuario con su ayuda")
    comprobar("len(x)" in AC.ayuda_llamada("n = len(", forms, codigo), "sintaxis: la firma de una integrada")
    t, desp = AC.expandir_snippet("def", "    ")
    comprobar(t == "def ():\n        pass" and desp == 4, "sintaxis: la plantilla def deja el cursor en el nombre %r" % (t,))
    comprobar(AC.expandir_snippet("for", "")[0].startswith("for  in range(10):") and AC.expandir_snippet("xyz") is None, "sintaxis: plantillas for y desconocida")
    comprobar(AC.en_texto("a = 'b") and AC.en_texto("a # c") and not AC.en_texto("a = 'b'"), "sintaxis: dentro de un texto o comentario")
    comprobar(set(AC.SNIPPETS) >= set(["def", "if", "for", "while", "try", "class"]), "sintaxis: plantillas def if for while try class")
    comprobar("tecla" in [m[0] for m in AC.miembros("Formulario")] and "raton_x" in [m[0] for m in AC.miembros("Lienzo")]
              and "cargar_csv" in [m[0] for m in AC.miembros("Tabla")], "autocompletar: miembros de los componentes nuevos")


def eventos():
    """Los eventos nuevos de la v8.4.1 (D315): ratón, foco, teclado, menú contextual, Intro y validar,
    cambio de Casilla y Opcion, celda_cambio y ordenar de Tabla, y los del formulario (cerrando
    cancelable, cerrado, activar y desactivar). Se disparan con event_generate sobre los componentes."""
    f = vps.Formulario("Ev", 320, 220, nombre="FormEv")
    b = vps.Boton(f, "b", 10, 10, nombre="B1")
    t = vps.CuadroTexto(f, 10, 50, 150, 22, nombre="T1")
    t2 = vps.CuadroTexto(f, 10, 80, 150, 22, nombre="T2")
    ca = vps.Casilla(f, "c", 10, 110, nombre="Ca1")
    op = vps.Opcion(f, "o", 100, 110, nombre="Op1")
    tb = vps.Tabla(f, 10, 140, 200, 60, nombre="Tb1")
    tb.columnas = ["A", "B"]
    tb.filas = ["2;x", "1;y"]
    reg = []
    comunes = vps.EVENTOS_COMUNES
    comprobar(len(comunes) == 12 and "raton_entra" in comunes and "menu_contextual" in comunes, "eventos: los doce comunes")
    comprobar(all(e in vps.Boton.EVENTOS for e in comunes) and "intro" in vps.CuadroTexto.EVENTOS
              and "validar" in vps.CuadroTexto.EVENTOS and "celda_cambio" in vps.Tabla.EVENTOS
              and "ordenar" in vps.Tabla.EVENTOS and "cambio" in vps.Casilla.EVENTOS and "cambio" in vps.Opcion.EVENTOS,
              "eventos: las listas EVENTOS de las clases incluyen los nuevos")
    comprobar(all(e in vps.Formulario.EVENTOS for e in ("redimensionar", "activar", "desactivar", "cerrando", "cerrado")),
              "eventos: los del formulario")
    comprobar(vps.Temporizador.EVENTOS == ("tic",) and vps.Reloj.EVENTOS == ("tic",) and "raton_mueve" in vps.RejillaDatos.EVENTOS
              and vps.ConectorBD.EVENTOS[0] == "conectado", "eventos: sin ventana no llevan los comunes; RejillaDatos y ConectorBD siguen")
    # sin manejador no se calcula nada (y no hay error)
    b.w.event_generate("<Motion>", x=3, y=4)
    comprobar(b.raton_x == 0, "eventos: raton_mueve sin manejador no calcula nada")
    for e in comunes:
        b.conectar(e, lambda e=e: reg.append(e))
    b.w.event_generate("<ButtonPress-1>", x=5, y=6, _duplo=True)
    comprobar(reg == ["doble_click", "raton_abajo"] and b.raton_x == 5 and b.raton_y == 6 and b.boton == 1,
              "eventos: doble_click y raton_abajo con boton, raton_x y raton_y %r" % (reg,))
    del reg[:]
    b.w.event_generate("<ButtonRelease-1>", x=7, y=8)
    comprobar(reg == ["raton_arriba"] and b.raton_x == 7, "eventos: raton_arriba")
    del reg[:]
    b.w.event_generate("<ButtonPress-3>", x=9, y=2)
    comprobar(reg == ["menu_contextual", "raton_abajo"] and b.boton == 3, "eventos: menu_contextual con el botón derecho %r" % (reg,))
    del reg[:]
    b.w.event_generate("<Enter>", x=1, y=1)
    b.w.event_generate("<Motion>", x=11, y=12)
    b.w.event_generate("<Leave>", x=0, y=0)
    comprobar(reg == ["raton_entra", "raton_mueve", "raton_sale"] and b.raton_x == 0 and b.raton_y == 0,
              "eventos: raton_entra, raton_mueve y raton_sale %r" % (reg,))
    del reg[:]
    b.w.event_generate("<MouseWheel>", x=1, y=1, delta=-240)
    comprobar(reg == ["rueda"] and b.rueda_delta == -2, "eventos: rueda con rueda_delta %r %r" % (reg, b.rueda_delta))
    del reg[:]
    b.w.event_generate("<FocusIn>")
    b.w.event_generate("<FocusOut>")
    comprobar(reg == ["foco_gana", "foco_pierde"], "eventos: foco_gana y foco_pierde")
    del reg[:]
    b.w.event_generate("<KeyPress-q>")
    comprobar(reg == ["tecla_abajo", "tecla_arriba"] and b.tecla == "q" and b.caracter == "q" and b.ctrl is False and b.mayus is False,
              "eventos: tecla_abajo y tecla_arriba con tecla y caracter %r" % (reg,))
    # por el nombre de la función, con el espacio de nombres
    llamadas = []
    f._ns = {"B1_raton_entra": lambda: llamadas.append("entra")}
    b.conectar("click", lambda: None)
    b.w.event_generate("<Enter>", x=0, y=0)
    comprobar("entra" in llamadas, "eventos: B1_raton_entra por el nombre")
    f._ns = None
    # CuadroTexto: intro y validar
    t.conectar("intro", lambda: reg.append("intro"))
    t.conectar("tecla", lambda: reg.append("tecla"))
    del reg[:]
    t.w.event_generate("<KeyPress-Return>")
    comprobar(reg == ["tecla", "intro"], "eventos: intro al pulsar Intro (y tecla sigue disparándose) %r" % (reg,))
    veces = []

    def validar():
        veces.append(t.texto)
        if t.texto == "mal":
            t.cancelar = True
    t.conectar("validar", validar)
    t.texto = "bien"
    t.w.event_generate("<FocusOut>")
    comprobar(veces == ["bien"] and t.cancelar is False, "eventos: validar al perder el foco")
    t.texto = "mal"
    t.w.event_generate("<FocusOut>")
    comprobar(veces[-1] == "mal" and t.cancelar is False, "eventos: validar cancelable (el foco vuelve)")
    # Casilla y Opcion: cambio
    ca.conectar("cambio", lambda: reg.append("ca"))
    op.conectar("cambio", lambda: reg.append("op"))
    del reg[:]
    ca._pulsado()
    op._pulsado()
    comprobar(reg == ["ca", "op"], "eventos: cambio en Casilla y Opcion")
    # Tabla: ordenar y celda_cambio
    tb.conectar("ordenar", lambda: reg.append("ordenar"))
    tb.conectar("celda_cambio", lambda: reg.append("celda"))
    del reg[:]
    tb._pulsado(-1, 0)
    comprobar(reg == ["ordenar"] and tb.columna == 0 and tb.celda(0, 0) == "1", "eventos: ordenar al pulsar la cabecera %r" % (reg,))
    del reg[:]
    tb.w.editar(0, 1)
    tb.w.ent[0].delete(0, "end")
    tb.w.ent[0].insert(0, "nuevo")
    tb.w.cerrar_edicion(True)
    comprobar(reg == ["celda"] and tb.fila == 0 and tb.columna == 1 and tb.celda(0, 1) == "nuevo", "eventos: celda_cambio con fila y columna %r" % (reg,))
    # formulario: cerrando cancelable, cerrado, activar y desactivar
    f.conectar("cerrando", lambda: (reg.append("cerrando"), setattr(f, "cancelar", True)))
    f.conectar("cerrado", lambda: reg.append("cerrado"))
    f.conectar("activar", lambda: reg.append("activar"))
    f.conectar("desactivar", lambda: reg.append("desactivar"))
    del reg[:]
    f._cargado = True
    f._activo_f = False
    f._activar()
    f._desactivar()
    comprobar(reg == ["activar", "desactivar"], "eventos: activar y desactivar %r" % (reg,))
    del reg[:]
    f._cerrar()
    comprobar(reg == ["cerrando"] and not f._cerrado and f.cancelar is False, "eventos: cerrando cancelado con cancelar = True %r" % (reg,))
    f._eventos["cerrando"] = []
    f.conectar("cerrando", lambda: reg.append("cerrando2"))
    del reg[:]
    f._cerrar()
    comprobar(reg == ["cerrando2", "cerrado"], "eventos: cerrando sin cancelar cierra y dispara cerrado %r" % (reg,))
    cerrar_todo()


def explorador():
    """El árbol del explorador del proyecto (sin pantalla): estructura, filtro y manejadores."""
    from vps import _explorador as EX
    forms = [{"nombre": "Form1", "props": {}, "controles": [
                {"tipo": "Marco", "nombre": "Marco1", "padre": None, "props": {}},
                {"tipo": "Boton", "nombre": "Boton1", "padre": "Marco1", "props": {}},
                {"tipo": "Pestanas", "nombre": "Pestanas1", "padre": None, "props": {"elementos": ["Uno", "Dos"]}},
                {"tipo": "Etiqueta", "nombre": "Etiqueta1", "padre": "Pestanas1", "props": {"pagina": 1}},
                {"tipo": "Temporizador", "nombre": "Temporizador1", "padre": None, "props": {}}]},
             {"nombre": "Form2", "props": {}, "controles": [
                {"tipo": "Boton", "nombre": "Boton2", "padre": None, "props": {}}]}]
    codigo = "def Boton1_click():\n    pass\n\ndef ayuda():\n    pass\n\ndef Form2_cargar():\n    pass\ndef Boton2_raton_entra():\n    pass\n"
    raiz = EX.arbol(forms, "Proj", codigo)
    filas = [("  " * n) + nodo["t"] for n, nodo in EX.aplanar(raiz, set())]
    esperado = ["Proj", "  Formularios", "    Form1", "      Marco1", "        Boton1", "      Pestanas1", "        Uno",
                "        Dos", "          Etiqueta1", "      Temporizador1", "    Form2", "      Boton2", "  Código",
                "    Boton1_click()", "    Form2_cargar()", "    Boton2_raton_entra()"]
    comprobar(filas == esperado, "explorador: árbol Proyecto > Formularios > controles, páginas y manejadores %r" % (filas,))
    comprobar([m[0] for m in EX.manejadores(forms, codigo)] == ["Boton1_click", "Form2_cargar", "Boton2_raton_entra"]
              and EX.manejadores(forms, codigo)[1][1] == 7, "explorador: solo los manejadores de eventos, con su línea")
    f = EX.filtrar(raiz, "boton")
    filas = [nodo["t"] for n, nodo in EX.aplanar(f, set(), True)]
    comprobar(filas == ["Proj", "Formularios", "Form1", "Marco1", "Boton1", "Form2", "Boton2", "Código", "Boton1_click()",
                        "Boton2_raton_entra()"], "explorador: el filtro deja los nodos que coinciden y sus ascendientes %r" % (filas,))
    cerr = set(["f:Form1"])
    comprobar(len(EX.aplanar(raiz, cerr)) == 9, "explorador: un nodo cerrado esconde a sus hijos")
    comprobar(EX.filtrar(raiz, "zzzz") is None, "explorador: un filtro sin resultados")
    M.mover_z(forms[0], forms[0]["controles"][0], True)
    comprobar([c["nombre"] for c in forms[0]["controles"]] == ["Pestanas1", "Etiqueta1", "Temporizador1", "Marco1", "Boton1"],
              "explorador: traer al frente mueve el contenedor con sus hijos")
    M.mover_z(forms[0], forms[0]["controles"][4], False)
    comprobar([c["nombre"] for c in forms[0]["controles"]] == ["Pestanas1", "Etiqueta1", "Temporizador1", "Marco1", "Boton1"],
              "explorador: enviar atrás un hijo lo deja justo detrás de su contenedor")
    M.mover_z(forms[0], forms[0]["controles"][3], False)
    comprobar(forms[0]["controles"][0]["nombre"] == "Marco1" and forms[0]["controles"][1]["nombre"] == "Boton1", "explorador: enviar atrás")


if __name__ == "__main__":
    principal()
