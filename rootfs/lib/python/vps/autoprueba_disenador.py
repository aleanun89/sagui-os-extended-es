# SPDX-License-Identifier: GPL-2.0-or-later
# vps.autoprueba_disenador - comprobaciones del diseñador de VisualPythonS sin ratón (mejora 124,
# D299): deshacer y rehacer, renombrar con refactorización, autocompletado en el editor, puntos
# de parada, orden de tabulación y diseño fiel. Escribe «vps-autoprueba: ok ...» o «FALLO ...».
# Lo usa tests/h299_vps_avanzado.py.
#
#     python /lib/python/vps/autoprueba_disenador.py
import sagui
import vps
from vps import disenador as DS, plantillas, _modelo as M, _depurador as D


def comprobar(cond, msg):
    print("vps-autoprueba: " + ("ok " if cond else "FALLO ") + msg)


def n_ctl(app):
    return len(app.form["controles"])


def principal():
    app = DS.App()
    app.confirmar_auto = True
    ed = app.editor

    # --- deshacer y rehacer del diseñador
    comprobar(n_ctl(app) == 0, "diseñador: proyecto vacío")
    app.crear_control("Boton", 16, 16)
    comprobar(n_ctl(app) == 1, "colocar: un botón")
    app.deshacer()
    comprobar(n_ctl(app) == 0, "deshacer colocar")
    app.rehacer()
    comprobar(n_ctl(app) == 1 and app.form["controles"][0]["nombre"] == "Boton1", "rehacer colocar")
    app.punto()
    app.form["controles"][0]["props"]["x"] = 96
    app.cambio(True)
    app.deshacer()
    comprobar(M.val(app.form["controles"][0], "x") == 16, "deshacer mover")
    app.rehacer()
    comprobar(M.val(app.form["controles"][0], "x") == 96, "rehacer mover")
    app.punto()
    app.form["controles"][0]["props"]["ancho"] = 200
    app.deshacer()
    comprobar(M.val(app.form["controles"][0], "ancho") == 88, "deshacer redimensionar")
    app.seleccionar([app.form["controles"][0]])
    app.aplicar("texto", "Hola")
    comprobar(M.val(app.form["controles"][0], "texto") == "Hola", "propiedad: texto")
    app.deshacer()
    comprobar(M.val(app.form["controles"][0], "texto") == "Boton1", "deshacer propiedad")
    app.rehacer()
    comprobar(M.val(app.form["controles"][0], "texto") == "Hola", "rehacer propiedad")
    app.seleccionar([app.form["controles"][0]])
    app.borrar_seleccion()
    comprobar(n_ctl(app) == 0, "borrar")
    app.deshacer()
    comprobar(n_ctl(app) == 1 and app.form["controles"][0]["nombre"] == "Boton1", "deshacer borrar")
    app.seleccionar([app.form["controles"][0]])
    app.copiar()
    app.pegar()
    comprobar(n_ctl(app) == 2, "pegar")
    app.deshacer()
    comprobar(n_ctl(app) == 1, "deshacer pegar")
    app.crear_control("Etiqueta", 8, 8)
    app.deshacer()
    app.crear_control("Casilla", 8, 8)
    comprobar(not app.hist.puede_rehacer(), "un cambio nuevo borra lo que se podía rehacer")

    # --- deshacer y rehacer del editor
    ed.poner("a = 1\n")
    ed._historial("teclear")
    ed.t.insert("1.0", "x")
    ed._historial("teclear")                    # seguida: se agrupa con la anterior
    ed.t.insert("1.1", "y")
    ed._historial("intro")
    ed.t.insert("end-1c", "b = 2\n")
    ed.deshacer()
    comprobar(ed.texto() == "xya = 1\n", "editor: deshacer el último paso %r" % ed.texto())
    ed.deshacer()
    comprobar(ed.texto() == "a = 1\n", "editor: las teclas seguidas son un solo paso %r" % ed.texto())
    ed.rehacer()
    comprobar(ed.texto() == "xya = 1\n", "editor: rehacer")

    # --- renombrar un control actualiza su código
    app.nuevo(plantillas.calculadora)
    app.confirmar_auto = True
    b = M.por_nombre(app.form, "Boton1")
    app.seleccionar([b])
    app.confirmar_auto = False
    app.aplicar("nombre", "Calcular")
    comprobar(M.por_nombre(app.form, "Boton1") is not None and "def Boton1_click():" in ed.texto(), "renombrar: cancelar no cambia nada")
    app.confirmar_auto = True
    app.aplicar("nombre", "Calcular")
    comprobar(M.por_nombre(app.form, "Calcular") is not None and M.por_nombre(app.form, "Boton1") is None, "renombrar: el control")
    comprobar("def Calcular_click():" in ed.texto() and "Boton1" not in ed.texto(), "renombrar: el manejador del código")
    comprobar(len(app.ultima_lista) >= 1 and app.ultima_lista[0][1] == "def Boton1_click():", "renombrar: se enseñó la lista de cambios")
    app.deshacer()
    comprobar(M.por_nombre(app.form, "Boton1") is not None and "def Boton1_click():" in ed.texto(), "renombrar: deshacer devuelve el control y el código")
    app.rehacer()
    comprobar("def Calcular_click():" in ed.texto(), "renombrar: rehacer")
    app.seleccionar([M.por_nombre(app.form, "Calcular")])
    app.aplicar("nombre", "Etiqueta1")
    comprobar(M.por_nombre(app.form, "Calcular") is not None, "renombrar: un nombre repetido se rechaza")
    app.aplicar("nombre", "1mal")
    comprobar(M.por_nombre(app.form, "Calcular") is not None, "renombrar: un nombre no válido se rechaza")

    # --- autocompletado en el editor
    ed.poner("Calcular.")
    ed.t.mark_set("insert", "end-1c")
    ed._sugerir(False)
    nombres = [i[0] for i in ed._sug_items]
    comprobar(ed._sug is not None and "texto" in nombres and "pulsar" in nombres, "editor: Calcular. abre la lista de sugerencias")
    ed._sug_sel = nombres.index("texto")
    ed._aceptar_sug()
    comprobar(ed.texto() == "Calcular.texto" and ed._sug is None, "editor: aceptar una sugerencia %r" % ed.texto())
    ed.poner("x = Calcular.pul")
    ed.t.mark_set("insert", "end-1c")
    ed._sugerir(False)
    ed._aceptar_sug()
    comprobar(ed.texto() == "x = Calcular.pulsar()", "editor: completa el prefijo y cierra el método %r" % ed.texto())
    ed.poner("Lista1.agregar(")
    ed.t.mark_set("insert", "end-1c")
    app.proy["forms"][0]["controles"].append({"tipo": "Lista", "nombre": "Lista1", "padre": None, "props": {}})
    ed._ayuda_llamada()
    comprobar("agregar(texto)" in ed.estado.cget("text"), "editor: la firma aparece al abrir el paréntesis")

    # --- puntos de parada del editor
    ed.poner("a = 1\nb = 2\nc = 3\n")
    ed.alternar_punto(2)
    comprobar(ed.puntos == set([2]) and ed.t._fondo_linea.get(2) == 0xFFD890, "editor: F9 marca la línea")
    ed._desplazar_puntos(1, 1, False)
    comprobar(ed.puntos == set([3]), "editor: añadir una línea arriba baja el punto de parada")
    ed.alternar_punto(3)
    comprobar(not ed.puntos and 3 not in ed.t._fondo_linea, "editor: F9 otra vez lo quita")

    # --- ejecutar con un punto de parada
    app.nuevo(plantillas.hola_mundo)
    ed.alternar_punto(4)
    paradas = []
    app.decisor_depurador = lambda n, v: paradas.append((n, v)) or "continuar"
    app.ejecutar()
    f = vps._formularios[0]
    f._ns["Boton1"].pulsar()
    comprobar(len(paradas) == 1 and paradas[0][0] == 4, "depurar: para en el punto de parada de Boton1_click %r" % ([p[0] for p in paradas],))
    comprobar(f._ns["Etiqueta1"].texto == "¡Hola, mundo!", "depurar: y sigue ejecutando")
    app.detener()
    ed.quitar_puntos()

    # --- orden de tabulación
    app.nuevo(plantillas.calculadora)
    nombres = app.orden_tab_lista()
    comprobar(nombres[0] == "Etiqueta1", "tabulación: por omisión, el orden de creación")
    app.fijar_orden_tab(list(reversed(nombres)))
    comprobar(app.orden_tab_lista() == list(reversed(nombres)), "tabulación: se puede cambiar")
    app.deshacer()
    comprobar(app.orden_tab_lista() == nombres, "tabulación: deshacer")

    # --- anclaje válido
    comprobar(app._anclaje_valido("izq|der") and app._anclaje_valido("arr") and not app._anclaje_valido("foo")
              and not app._anclaje_valido(""), "anclaje: se valida lo escrito")

    # --- diseño fiel: el control real se dibuja en la superficie
    img = sagui.Imagen.nueva(160, 60, 0xFFFFFF)
    sup = app.superficie
    boton = M.por_nombre(app.form, "Boton1")
    boton["props"]["fuente"] = "Sans 16 negrita"
    dibujado = sup.vista.pintar(img, boton, 4, 4, 120, 40, app.form)
    mv = memoryview(img)
    distintos = len([1 for p in mv if p != 0xFFFFFF])
    comprobar(dibujado and distintos > 400, "diseño fiel: el Boton se dibuja con el código de ejecución (%d píxeles)" % distintos)
    sup.vista.podar()

    explorador(app)
    componentes_y_sintaxis(app)
    app.proy["modificado"] = False
    app.raiz.destroy()
    print("vps-autoprueba: fin")


class _Tecla:
    def __init__(self, ch, keysym=None):
        self.char = ch
        self.keysym = keysym or ch
        self.state = 0


def teclear(ed, texto):
    """Escribe `texto` en el editor como si se pulsaran las teclas (con el auto-cierre y la lista)."""
    for ch in texto:
        if ch == "\n":
            ed._intro(_Tecla("\r", "Return"))
            continue
        r = ed._tecla(_Tecla(ch))
        if r != "break":
            ed.t.insert("insert", ch)
        ed._tras_tecla(ch, ch)


def componentes_y_sintaxis(app):
    """Los componentes nuevos en el diseñador y el autocompletado de sintaxis del editor (D303)."""
    from vps._superficie import HERRAMIENTAS
    from vps import _autocompletar as AC
    ed = app.editor
    comprobar(all(t in HERRAMIENTAS for t in ("Pestanas", "Tabla", "Numerico", "Lienzo", "Enlace", "BarraEstado",
                                               "SelectorColor", "Reloj", "Archivo")), "caja de herramientas: los componentes nuevos")
    sup = app.superficie
    img = sagui.Imagen.nueva(300, 200, 0xFFFFFF)
    app.nuevo(plantillas.vacia)
    pest = app.crear_control("Pestanas", 16, 16, 200, 120)
    comprobar(pest["nombre"] == "Pestanas1" and M.val(pest, "elementos") == ["General", "Opciones"], "diseñador: se coloca unas Pestanas")
    hijo = app.crear_control("Boton", 8, 8, None, None, "Pestanas1")
    comprobar(hijo["padre"] == "Pestanas1" and M.rect_abs(app.form, hijo)[:2] == (16 + 2 + 8, 16 + 2 + vps.TAB_H + 8),
              "diseñador: un control dentro de las Pestanas se coloca bajo la tira de pestañas")
    pest["props"]["seleccion"] = 1
    comprobar(not M.en_pagina(app.form, hijo), "diseñador: el control de otra página no se ve")
    hijo2 = app.crear_control("Etiqueta", 8, 8, None, None, "Pestanas1")
    comprobar(M.val(hijo2, "pagina") == 1 and M.en_pagina(app.form, hijo2), "diseñador: un control nuevo va a la página que se ve")
    app.seleccionar([pest])
    app.refrescar_props()
    comprobar(not any(f[0] == "pagina" for f in app.props.filas), "propiedades: «pagina» no sale en las Pestanas")
    app.seleccionar([hijo2])
    app.refrescar_props()
    comprobar(any(f[0] == "pagina" for f in app.props.filas), "propiedades: «pagina» sale en un control de las Pestanas")
    for tipo in ("Pestanas", "Tabla", "Numerico", "Lienzo", "Enlace", "BarraEstado", "SelectorColor"):
        c = {"tipo": tipo, "nombre": tipo + "9", "padre": None, "props": {}}
        ok = sup.vista.pintar(img, c, 4, 4, 160, 100, app.form)
        distintos = len([1 for q in memoryview(img) if q != 0xFFFFFF])
        comprobar(ok and distintos > 150, "diseño fiel: %s se dibuja con el código de ejecución (%d píxeles)" % (tipo, distintos))
        img = sagui.Imagen.nueva(300, 200, 0xFFFFFF)
    sup.vista.podar()
    # el .vpf con las Pestanas y su página se guarda y se vuelve a leer
    from vps import proyecto
    otra = proyecto.leer_vpf(proyecto.escribir_vpf(app.proy["forms"]))
    h2 = [c for c in otra[0]["controles"] if c["nombre"] == hijo2["nombre"]][0]
    comprobar(h2["padre"] == "Pestanas1" and h2["props"].get("pagina") == 1, "vpf: la página del control se guarda")
    app.deshacer()
    app.deshacer()
    app.deshacer()
    app.deshacer()
    comprobar(n_ctl(app) == 0, "deshacer: quita los controles de las Pestanas")

    # --- sintaxis en el editor: palabra clave tras 2 caracteres, Tab y Esc
    ed.poner("")
    teclear(ed, "w")
    comprobar(ed._sug is None, "editor: con un carácter no sale la lista")
    teclear(ed, "h")
    nombres = [i[0] for i in ed._sug_items]
    comprobar(ed._sug is not None and nombres[0] == "while", "editor: «wh» ofrece la palabra clave while %r" % (nombres,))
    ed._escape(None)
    comprobar(ed._sug is None and ed.texto() == "wh", "editor: Esc cierra la lista")
    teclear(ed, "i")
    comprobar(ed._sug is None, "editor: tras Esc no se vuelve a abrir en la misma palabra")
    ed.poner("")
    teclear(ed, "pri")
    comprobar(ed._sug is not None and ed._sug_items[0][0] == "print", "editor: una función integrada (print)")
    ed._aceptar_sug()
    comprobar(ed.texto() == "print(", "editor: aceptar print deja el paréntesis abierto %r" % ed.texto())
    # nombres del propio código
    ed.poner("contador = 0\ndef sumar_uno(x):\n    return x + 1\n\nsu")
    ed.t.mark_set("insert", "end-1c")
    teclear(ed, "m")
    nombres = [i[0] for i in ed._sug_items]
    comprobar("sumar_uno" in nombres, "editor: ofrece las funciones del propio código %r" % (nombres,))
    comprobar(len(AC.integradas()) >= 20, "autocompletar: %d funciones integradas de este Python" % len(AC.integradas()))
    ed._cerrar_sug()
    ed.poner("contador = 0\n\nco")
    ed.t.mark_set("insert", "end-1c")
    teclear(ed, "n")
    comprobar(ed._sug is not None and ed._sug_items[0][0] == "contador", "editor: ofrece las variables del propio código")
    ed._cerrar_sug()
    # plantillas con Tab
    for palabra, esperado in (("def", "def ():\n    pass"), ("if", "if :\n    pass"), ("for", "for  in range(10):\n    pass"),
                              ("while", "while :\n    pass"), ("class", "class :\n    def __init__(self):\n        pass"),
                              ("try", "try:\n    pass\nexcept Exception as e:\n    print(e)")):
        ed.poner("")
        ed.t.mark_set("insert", "end-1c")
        ed.t.insert("insert", palabra)
        r = ed._tab(None)
        comprobar(r == "break" and ed.texto() == esperado, "editor: %s + Tab expande la plantilla %r" % (palabra, ed.texto()))
    ed.poner("")
    ed.t.insert("insert", "def")
    ed._tab(None)
    cur = ed.t.index("insert")
    comprobar(cur == "1.4", "editor: tras la plantilla def el cursor queda en el nombre (%s)" % cur)
    ed.poner("if x:\n    ")
    ed.t.mark_set("insert", "end-1c")
    ed.t.insert("insert", "for")
    ed._tab(None)
    comprobar(ed.texto() == "if x:\n    for  in range(10):\n        pass" and ed.t.index("insert") == "2.8", "editor: la plantilla respeta la sangría %r" % ed.texto())
    ed.poner("x = 1")
    ed.t.mark_set("insert", "end-1c")
    ed._tab(None)
    comprobar(ed.texto() == "x = 1    ", "editor: Tab sin plantilla ni lista pone cuatro espacios")
    # sangría automática
    ed.poner("def f():")
    ed.t.mark_set("insert", "end-1c")
    ed._intro(_Tecla("\r", "Return"))
    comprobar(ed.texto() == "def f():\n    ", "editor: Intro tras «:» sangra")
    teclear(ed, "return 1")
    ed._intro(_Tecla("\r", "Return"))
    comprobar(ed.texto() == "def f():\n    return 1\n", "editor: Intro tras return quita una sangría %r" % ed.texto())
    ed.poner("if x:  # nota")
    ed.t.mark_set("insert", "end-1c")
    ed._intro(_Tecla("\r", "Return"))
    comprobar(ed.texto() == "if x:  # nota\n    ", "editor: Intro tras «:» con comentario sangra")
    # cierre de paréntesis, corchetes y comillas
    ed.poner("")
    teclear(ed, "f(")
    comprobar(ed.texto() == "f()" and ed.t.index("insert") == "1.2", "editor: ( cierra el paréntesis %r" % ed.texto())
    teclear(ed, ")")
    comprobar(ed.texto() == "f()" and ed.t.index("insert") == "1.3", "editor: escribir ) salta el cierre que ya está")
    ed.poner("")
    teclear(ed, 'x = ["a')
    comprobar(ed.texto() == 'x = ["a"]', "editor: [ y comillas se cierran %r" % ed.texto())
    teclear(ed, '"]')
    comprobar(ed.texto() == 'x = ["a"]' and ed.t.index("insert") == "1.9", "editor: las comillas y el corchete se saltan al cerrar %r" % ed.texto())
    ed.poner("")
    ed.t.insert("insert", "()")
    ed.t.mark_set("insert", "1.1")
    ed._tecla(_Tecla("", "BackSpace"))
    comprobar(ed.texto() == "", "editor: BackSpace entre un par vacío borra los dos")
    ed.poner("")
    teclear(ed, "no")
    teclear(ed, "'")
    comprobar(ed.texto() == "no'", "editor: un apóstrofo tras una letra no cierra comillas")
    ed.poner("")
    teclear(ed, '# dice "hola')
    comprobar(ed.texto() == '# dice "hola', "editor: en un comentario no se cierra nada")
    # la firma de una función del usuario
    ed.poner("def doble(n):\n    \"\"\"Duplica n\"\"\"\n    return n * 2\n\nx = doble")
    ed.t.mark_set("insert", "end-1c")
    teclear(ed, "(")
    comprobar("doble(n)" in ed.estado.cget("text") and "Duplica n" in ed.estado.cget("text"), "editor: firma de la función del usuario %r" % ed.estado.cget("text"))
    # Ctrl+Espacio con un carácter
    ed.poner("p")
    ed.t.mark_set("insert", "end-1c")
    ed._sugerir(True)
    comprobar(ed._sug is not None and len(ed._sug_items) > 3, "editor: Ctrl+Espacio ofrece con un carácter")
    ed._cerrar_sug()
    ed.poner("")


def _nodo(app, id):
    for n, nodo in app.arbol.filas:
        if nodo["id"] == id:
            return nodo
    return None


def _accion(app, nodo, texto):
    for a in app.acciones_explorador(nodo):
        if a is not None and a[0] == texto:
            return a[1]
    return None


def explorador(app):
    """El explorador del proyecto (D315): árbol, clic que salta de formulario y de página, doble clic al
    código, filtro, menú contextual (renombrar, borrar, orden z, agregar formulario) y sincronía con
    deshacer."""
    ed = app.editor
    app.nuevo(plantillas.vacia)
    app.confirmar_auto = True
    comprobar(not app.expl_visible, "explorador: oculto al empezar")
    app.alternar_explorador()
    comprobar(app.expl_visible, "explorador: Ctrl+R lo muestra")
    app.crear_control("Marco", 8, 8, 160, 100)
    marco = app.form["controles"][0]
    app.crear_control("Boton", 16, 24, None, None, marco["nombre"])
    app.crear_control("Pestanas", 8, 120, 200, 100)
    pest = M.por_nombre(app.form, "Pestanas1")
    pest["props"]["seleccion"] = 1
    app.crear_control("Etiqueta", 8, 8, None, None, "Pestanas1")
    etiqueta = M.por_nombre(app.form, "Etiqueta1")
    pest["props"]["seleccion"] = 0
    app.crear_control("Temporizador", 250, 8)
    ed.poner("def Boton1_click():\n    pass\n\ndef Formulario_cargar():\n    pass\n")
    app.agregar_formulario()
    app.crear_control("Casilla", 8, 8)
    app.ir_a_form(0)
    app.refrescar_explorador()
    filas = app.arbol.texto_filas()
    comprobar(filas[:3] == ["Proyecto1", "  Formularios", "    Formulario"], "explorador: Proyecto > Formularios > formulario %r" % (filas,))
    comprobar("      Marco1" in filas and "        Boton1" in filas and "        Opciones" in filas and "          Etiqueta1" in filas
              and "      Temporizador1" in filas and "    Formulario2" in filas and "      Casilla1" in filas,
              "explorador: contenedores, páginas de las Pestanas y controles sin ventana %r" % (filas,))
    comprobar(filas[-3:] == ["  Código", "    Boton1_click()", "    Formulario_cargar()"], "explorador: Código con los manejadores %r" % (filas[-3:],))
    # un clic salta a un control de otro formulario
    app.explorador_nodo(_nodo(app, "c:Casilla1"), False)
    comprobar(app.proy["idx"] == 1 and app.sel and app.sel[0]["nombre"] == "Casilla1", "explorador: un clic cambia de formulario y selecciona el control")
    comprobar(app.props.filas and app.arbol.sel == "c:Casilla1", "explorador: sus propiedades en la ventana de propiedades")
    # ... y a un control de otra pestaña
    pest["props"]["seleccion"] = 0
    etiqueta["props"]["pagina"] = 1
    app.refrescar_explorador()
    app.explorador_nodo(_nodo(app, "c:Etiqueta1"), False)
    comprobar(app.proy["idx"] == 0 and int(M.val(pest, "seleccion")) == 1 and app.sel[0]["nombre"] == "Etiqueta1",
              "explorador: un clic cambia de formulario y de pestaña")
    app.explorador_nodo(_nodo(app, "pg:Pestanas1/0"), False)
    comprobar(int(M.val(pest, "seleccion")) == 0, "explorador: un clic en una página la muestra")
    # doble clic: el evento por defecto en el editor
    ed.ocultar()
    app.explorador_nodo(_nodo(app, "c:Boton1"), True)
    comprobar(ed.visible() and ed.cb_ctl.get() == "Boton1" and ed.t.index("insert").startswith("2."), "explorador: doble clic abre Boton1_click en el editor %r" % (ed.t.index("insert"),))
    ed.ocultar()
    app.explorador_nodo(_nodo(app, "m:Formulario_cargar"), True)
    comprobar(ed.visible() and ed.t.index("insert").startswith("5."), "explorador: doble clic en un manejador salta a su línea %r" % (ed.t.index("insert"),))
    ed.ocultar()
    # el filtro
    app.filtro_var.set("cas")
    filas = app.arbol.texto_filas()
    comprobar("Casilla1" in " ".join(filas) and "Boton1" not in " ".join(filas), "explorador: el filtro por nombre %r" % (filas,))
    app.filtro_var.set("")
    comprobar("Boton1" in " ".join(app.arbol.texto_filas()), "explorador: sin filtro vuelve todo")
    # contraer un nodo
    app.explorador_nodo(_nodo(app, "c:Marco1"), False)      # con un hijo elegido la rama se abre sola
    app.arbol.alternar(_nodo(app, "c:Marco1"))
    comprobar("        Boton1" not in app.arbol.texto_filas(), "explorador: un nodo contraído esconde a sus hijos")
    app.arbol.alternar(_nodo(app, "c:Marco1"))
    # menú contextual: renombrar con la refactorización de D299
    nodo = _nodo(app, "c:Boton1")
    app.texto_auto = "Salir"
    _accion(app, nodo, "Renombrar")()
    app.texto_auto = None
    comprobar(M.por_nombre(app.form, "Salir") is not None and "def Salir_click():" in ed.texto() and "Salir" in " ".join(app.arbol.texto_filas())
              and "Salir_click()" in " ".join(app.arbol.texto_filas()), "explorador: renombrar desde el menú cambia el código y el árbol")
    app.deshacer()
    comprobar(M.por_nombre(app.form, "Boton1") is not None and "def Boton1_click():" in ed.texto() and "        Boton1" in app.arbol.texto_filas(),
              "explorador: deshacer lo sincroniza")
    # traer al frente y enviar atrás
    marco = M.por_nombre(app.form, "Marco1")
    _accion(app, _nodo(app, "c:Marco1"), "Traer al frente")()
    comprobar(app.form["controles"][-1]["nombre"] == "Boton1" and app.form["controles"][-2]["nombre"] == "Marco1", "explorador: traer al frente")
    _accion(app, _nodo(app, "c:Marco1"), "Enviar atrás")()
    comprobar(app.form["controles"][0]["nombre"] == "Marco1", "explorador: enviar atrás")
    # borrar desde el menú y deshacer
    n0 = n_ctl(app)
    _accion(app, _nodo(app, "c:Temporizador1"), "Eliminar")()
    comprobar(n_ctl(app) == n0 - 1 and _nodo(app, "c:Temporizador1") is None, "explorador: eliminar desde el menú")
    app.deshacer()
    comprobar(_nodo(app, "c:Temporizador1") is not None, "explorador: deshacer devuelve el nodo")
    # agregar formulario desde el menú
    nf = len(app.proy["forms"])
    _accion(app, _nodo(app, "c:Marco1"), "Agregar formulario")()
    comprobar(len(app.proy["forms"]) == nf + 1 and _nodo(app, "f:Formulario3") is not None, "explorador: agregar formulario desde el menú")
    # al añadir un control desde el diseñador, el árbol se actualiza
    app.ir_a_form(0)
    app.crear_control("Lista", 150, 150)
    comprobar(_nodo(app, "c:Lista1") is not None, "explorador: se sincroniza al añadir un control")
    app.mostrar_explorador(False)
    comprobar(not app.expl_visible, "explorador: Ctrl+R lo oculta")
    app.confirmar_auto = True


if __name__ == "__main__":
    principal()
