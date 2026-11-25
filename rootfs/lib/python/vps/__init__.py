# SPDX-License-Identifier: GPL-2.0-or-later
# vps - VisualPythonS: formularios y controles al estilo del Visual Basic clásico
# (mejora 123, D297), sobre el tkinter del SaguiOS.
#
#     from vps import *
#     f = Formulario("Hola", 300, 160)
#     b = Boton(f, "Pulsa", 20, 20, nombre="Boton1")
#     e = Etiqueta(f, "", 20, 60, nombre="Etiqueta1")
#     def Boton1_click():
#         e.texto = "¡Hola!"
#     ejecutar(globals())
#
# Los eventos se conectan solos por el nombre de la función (Nombre_evento) o con
# @evento(control, "click"). Los controles se parecen a los del Visual Basic pero más
# sencillos: propiedades en español (.texto, .x, .y, .ancho, .alto, .visible, .activo,
# .color, .valor, .elementos...) y manejadores sin parámetros (los datos del evento
# están en el propio control: Formulario.tecla, Formulario.caracter...).
# Referencia completa: docs/visualpythons.md y /ayuda/visualpythons.md.
import sys
import time
import tkinter as tk
from tkinter import messagebox, simpledialog, filedialog, ttk
from vps import _fuente

__all__ = ["Formulario", "Etiqueta", "Boton", "CuadroTexto", "Casilla", "Opcion", "Lista",
           "Combo", "Imagen", "Marco", "Deslizador", "BarraDesplazamiento", "Temporizador",
           "BarraProgreso", "Menu", "Pestanas", "Tabla", "Numerico", "Lienzo", "Enlace",
           "BarraEstado", "SelectorColor", "Reloj", "Archivo", "evento", "ejecutar", "salir", "msgbox", "inputbox",
           "notificar", "notificaciones", "cerrar_notificaciones", "Notificacion", "Arbol", "Grafico",
           "dialogo_abrir", "dialogo_guardar", "dialogo_carpeta", "pausa", "depurar", "NEGRO", "BLANCO", "ROJO", "VERDE",
           "AZUL", "AMARILLO", "GRIS", "PLATA", "NARANJA", "MORADO", "MARRON", "ROSA", "CIAN"]

NEGRO, BLANCO, ROJO, VERDE = 0x000000, 0xFFFFFF, 0xFF0000, 0x008000
AZUL, AMARILLO, GRIS, PLATA = 0x0000FF, 0xFFFF00, 0x808080, 0xC0C0C0
NARANJA, MORADO, MARRON, ROSA, CIAN = 0xFFA500, 0x800080, 0x804020, 0xFFC0CB, 0x00FFFF

# Las formas de los colores: entero 0xRRGGBB, "#rrggbb" o un nombre en español o inglés.
_NOMBRES_COLOR = {"negro": NEGRO, "blanco": BLANCO, "rojo": ROJO, "verde": VERDE, "azul": AZUL,
                  "amarillo": AMARILLO, "gris": GRIS, "plata": PLATA, "naranja": NARANJA,
                  "morado": MORADO, "marron": MARRON, "rosa": ROSA, "cian": CIAN}


def _color_int(c, defecto):
    """Un color de VisualPythonS como entero 0xRRGGBB (`defecto` si no hay)."""
    t = _a_tk(c)
    if t is None:
        return defecto
    try:
        return int(t[1:], 16)
    except Exception:
        return defecto


def _a_tk(c):
    """Un color de VisualPythonS al «#rrggbb» de tkinter (None = el de siempre)."""
    if c is None or c == "":
        return None
    if isinstance(c, str):
        c = c.strip()
        if c.lower() in _NOMBRES_COLOR:
            c = _NOMBRES_COLOR[c.lower()]
        else:
            return c
    return "#%06x" % (int(c) & 0xFFFFFF)


# ------------------------------------------------------------ errores y eventos

error_hook = None     # f(excepcion, donde): lo usa el diseñador para saltar a la línea
depurar_hook = None   # f(variables): el diseñador enseña las variables en su depurador
_salida = [False]


def _informar(e, donde):
    """Un error en un manejador: lo informa y la aplicación sigue (como el VB sin On Error)."""
    if error_hook is not None:
        error_hook(e, donde)
        return
    sys.stderr.write("Error en %s:\n" % donde)
    sys.print_exception(e)
    try:
        messagebox.showerror("Error en " + donde, "%s: %s" % (type(e).__name__, e))
    except Exception:
        pass


def _llamar(f, donde):
    try:
        return f()
    except Exception as e:
        _informar(e, donde)
    return None


def evento(control, nombre):
    """Decorador: @evento(Boton1, "click") conecta la función a ese evento."""
    def deco(f):
        control.conectar(nombre, f)
        return f
    return deco


def _propiedad(n):
    def leer(self):
        return self._leer(n)

    def escribir(self, v):
        self._escribir(n, v)
    return property(leer, escribir)


# ------------------------------------------------------------ controles

def _props(c):
    """Decorador: una propiedad de Python por cada entrada de PROPS de la clase. Tiene que
    aplicarse justo al definirla (MicroPython no deja añadir propiedades a una clase que ya
    tiene subclases)."""
    for n, t, d in c.PROPS:
        if n not in c.__dict__:
            setattr(c, n, _propiedad(n))
    return c


# Eventos comunes a todos los controles con ventana (v8.4.1, D315). Los datos del evento quedan en el
# propio control (D297): raton_x, raton_y y boton (ratón), rueda_delta (rueda), tecla, caracter, ctrl y
# mayus (teclado). Solo se calculan si hay un manejador conectado (ver Control._hay).
EVENTOS_COMUNES = ("doble_click", "raton_abajo", "raton_arriba", "raton_entra", "raton_sale", "raton_mueve",
                   "rueda", "foco_gana", "foco_pierde", "tecla_abajo", "tecla_arriba", "menu_contextual")


_cableado = [False]


def _despachar(metodo):
    def f(ev):
        c = getattr(ev.widget, "_vps", None)
        if c is not None:
            getattr(c, metodo)(ev)
    return f


def _ev(*propios):
    """Los eventos propios de un control seguidos de los comunes (sin repetir)."""
    return tuple(propios) + tuple(e for e in EVENTOS_COMUNES if e not in propios)


@_props
class Control:
    """Base de los controles. PROPS = (nombre, tipo, valor por defecto); los tipos son
    int, str, bool, color, lista y enum:a|b|c (el diseñador los usa para editar)."""
    tipo = "Control"
    PROPS = (("x", "int", 0), ("y", "int", 0), ("ancho", "int", 100), ("alto", "int", 24),
             ("visible", "bool", True), ("activo", "bool", True), ("color", "color", None),
             ("colorfondo", "color", None), ("fuente", "str", ""), ("anclaje", "str", "izq|arr"),
             ("orden_tab", "int", 0), ("pagina", "int", 0))
    EVENTOS = ("click",)
    ORDEN = ("texto", "x", "y", "ancho", "alto")   # argumentos sin nombre
    TAM = (100, 24)
    CONTENEDOR = False
    DOBLE_PROPIO = False      # True si el control ya dispara doble_click por su cuenta (Lista, Tabla)
    # datos de los eventos comunes (D297: los datos del evento están en el control)
    raton_x = 0
    raton_y = 0
    boton = 0                 # 1 izquierdo, 2 central, 3 derecho
    rueda_delta = 0           # entalles de la rueda: positivo hacia arriba, negativo hacia abajo
    tecla = ""
    caracter = ""
    ctrl = False
    mayus = False
    cancelar = False          # en «validar»: ponerlo a True devuelve el foco al control

    def __init__(self, padre, *args, **kw):
        if isinstance(padre, Formulario):
            self.form = padre
        else:
            self.form = padre.form
        self.padre = padre
        self._eventos = {}
        self._p = {}
        for n, t, d in self.todas():
            self._p[n] = list(d) if isinstance(d, list) else d
        self._p["ancho"], self._p["alto"] = self.TAM
        self._p["texto"] = self._p.get("texto", "")
        for i in range(len(args)):
            kw[self.ORDEN[i]] = args[i]
        nombre = kw.pop("nombre", None) or self.form._nombre_nuevo(self.tipo)
        self.nombre = nombre
        for n, v in kw.items():
            if n not in self._p:
                raise TypeError("%s no tiene la propiedad «%s»" % (self.tipo, n))
            self._p[n] = list(v) if isinstance(v, list) else v
        self.w = None
        self._crear()
        self._cablear()
        self._aplicar_todo()
        self.form._registrar(self)

    @classmethod
    def todas(cls):
        r = list(Control.PROPS)
        if cls.PROPS is not Control.PROPS:
            r += list(cls.PROPS)
        return r

    # --- propiedades
    def _leer(self, n):
        return self._p.get(n)

    def _escribir(self, n, v):
        self._p[n] = v
        if self.w is not None:
            self._aplicar(n)

    def _aplicar_todo(self):
        self._colocar()
        for n in self._p:
            if n not in ("x", "y", "ancho", "alto", "visible", "pagina"):
                self._aplicar(n)

    def _aplicar(self, n):
        if n in ("x", "y", "ancho", "alto", "visible", "pagina"):
            self._colocar()
        elif n == "activo":
            self._cfg(state="normal" if self._p["activo"] else "disabled")
        elif n == "color":
            self._cfg(foreground=_a_tk(self._p["color"]))
        elif n == "colorfondo":
            self._aplicar_fondo()
        elif n in ("fuente", "campo") and self.tipo in ("CuadroTexto", "Casilla", "Numerico", "Combo"):
            from vps import _datos     # mejora 127: controles de datos (fuente = un ConectorBD)
            _datos.enlazar(self)

    FONDO_HEREDADO = False    # sin colorfondo propio, toma el color del formulario

    def _aplicar_fondo(self):
        c = self._p["colorfondo"]
        if c is None and self.FONDO_HEREDADO:
            c = self.form._p["color"]
        self._cfg(background=_a_tk(c))

    def _cfg(self, **kw):
        for k, v in kw.items():
            if v is None:
                continue
            try:
                self.w.configure(**{k: v})
            except Exception:
                pass

    def _colocar(self):
        if self.w is None:
            return
        if self._p["visible"] and self._en_pagina():
            self.w.place(x=int(self._p["x"]), y=int(self._p["y"]),
                         width=max(1, int(self._p["ancho"])), height=max(1, int(self._p["alto"])))
        else:
            self.w.place_forget()

    def _en_pagina(self):
        """False si el control está en una página de Pestanas que no es la que se ve."""
        f = getattr(self.padre, "_hijo_visible", None)
        return f is None or f(self)

    def _crear(self):
        pass

    def _contenedor_tk(self):
        return self.padre._contenedor_tk()

    def _anclar(self, dx, dy):
        """El formulario (o el Marco que lo contiene) cambió de tamaño en (dx, dy): mueve o
        estira el control según su `anclaje` (izq|der|arr|abj; por defecto izq|arr: no cambia)."""
        a = str(self._p.get("anclaje") or "").split("|")
        x, y, w, h = int(self._p["x"]), int(self._p["y"]), int(self._p["ancho"]), int(self._p["alto"])
        izq, der, arr, abj = "izq" in a, "der" in a, "arr" in a, "abj" in a
        if der and not izq:
            x += dx
        elif der and izq:
            w += dx
        if abj and not arr:
            y += dy
        elif abj and arr:
            h += dy
        w, h = max(1, w), max(1, h)
        antes = (int(self._p["ancho"]), int(self._p["alto"]))
        self._p["x"], self._p["y"], self._p["ancho"], self._p["alto"] = x, y, w, h
        self._colocar()
        if self.CONTENEDOR:
            for c in list(self.form.controles.values()):
                if c.padre is self:
                    c._anclar(w - antes[0], h - antes[1])

    # --- eventos
    def conectar(self, nombre, f):
        self._eventos.setdefault(nombre, []).append(f)

    def _hay(self, nombre):
        """True si el evento tiene algún manejador (conectado o con el nombre Control_evento): los
        eventos de ratón y teclado no hacen nada más si nadie los escucha."""
        if nombre in self._eventos:
            return True
        ns = self.form._ns
        return ns is not None and ("%s_%s" % (self.nombre, nombre)) in ns

    def _cablear(self):
        """Engancha los eventos comunes (D315). Los enlaces son de `bind_all` y no de cada componente:
        tkinter entrega solo el enlace más específico de cada tabla, y así no pisan a los propios
        del control (Button-1, Return...). Cada componente apunta a su control en `_vps`."""
        w = self.w
        if w is None:
            return
        w._vps = self
        if _cableado[0]:
            return
        _cableado[0] = True
        for seq, metodo in (("<ButtonPress>", "_e_abajo"), ("<ButtonRelease>", "_e_arriba"),
                            ("<Enter>", "_e_entra"), ("<Leave>", "_e_sale"), ("<Motion>", "_e_mueve"),
                            ("<MouseWheel>", "_e_rueda"), ("<FocusIn>", "_e_foco_gana"),
                            ("<FocusOut>", "_e_foco_pierde"), ("<KeyPress>", "_e_tecla")):
            w.bind_all(seq, _despachar(metodo), add="+")

    def _pos(self, ev):
        self.raton_x, self.raton_y = ev.x, ev.y

    def _e_abajo(self, ev):
        n = ev.num if isinstance(ev.num, int) else 1
        if n == 1 and getattr(ev, "_duplo", False) and not self.DOBLE_PROPIO and self._hay("doble_click"):
            self._pos(ev)
            self.boton = 1
            self._disparar("doble_click")
        if n == 3 and self._hay("menu_contextual"):
            self._pos(ev)
            self.boton = 3
            self._disparar("menu_contextual")
        if self._hay("raton_abajo"):
            self._pos(ev)
            self.boton = n
            self._disparar("raton_abajo")

    def _e_arriba(self, ev):
        if self._hay("raton_arriba"):
            self._pos(ev)
            self.boton = ev.num if isinstance(ev.num, int) else 1
            self._disparar("raton_arriba")

    def _e_entra(self, ev):
        if self._hay("raton_entra"):
            self._pos(ev)
            self._disparar("raton_entra")

    def _e_sale(self, ev):
        if self._hay("raton_sale"):
            self._pos(ev)
            self._disparar("raton_sale")

    def _e_mueve(self, ev):
        if self._hay("raton_mueve"):
            self._pos(ev)
            self._disparar("raton_mueve")

    def _e_rueda(self, ev):
        if self._hay("rueda"):
            self._pos(ev)
            self.rueda_delta = ev.delta // 120
            self._disparar("rueda")

    def _e_foco_gana(self, ev):
        if self._hay("foco_gana"):
            self._disparar("foco_gana")

    def _e_foco_pierde(self, ev):
        if self._hay("foco_pierde"):
            self._disparar("foco_pierde")

    def _e_tecla(self, ev):
        abajo, arriba = self._hay("tecla_abajo"), self._hay("tecla_arriba")
        if abajo or arriba:
            self.tecla = ev.keysym
            self.caracter = ev.char
            self.ctrl = bool(ev.state & 4)
            self.mayus = bool(ev.state & 1)
            if abajo:
                self._disparar("tecla_abajo")
            if arriba:
                # el sistema no avisa de cuándo se suelta una tecla: tecla_arriba se dispara justo
                # después de tecla_abajo, con los mismos datos (D315)
                self._disparar("tecla_arriba")

    def _disparar(self, nombre, *a):
        for f in self._eventos.get(nombre, ()):
            _llamar(f, "%s_%s" % (self.nombre, nombre))
        ns = self.form._ns
        if ns is not None:
            f = ns.get("%s_%s" % (self.nombre, nombre))
            if f is not None and callable(f):
                _llamar(f, "%s_%s" % (self.nombre, nombre))

    def enfocar(self):
        if self.w is not None:
            self.w.focus_set()

    def destruir(self):
        if self.w is not None:
            self.w.destroy()
            self.w = None
        self.form.controles.pop(self.nombre, None)

    def __repr__(self):
        return "<%s %s>" % (type(self).tipo, self.nombre)


def _texto_con_fuente(c, color_defecto, fondo_defecto):
    """Etiqueta y Boton con `fuente`: el texto se pinta en una imagen (vps._fuente) y se pone en
    el componente; con la fuente normal se vuelve al texto de siempre."""
    if c.w is None:
        return
    p = c._p
    try:
        if _fuente.es_nativa(p["fuente"]):
            if c._foto is not None:
                c._foto = None
                c.w.configure(image="", text=str(p["texto"]))
            return
        col = _color_int(p["color"], color_defecto)
        fondo = _color_int(p["colorfondo"], fondo_defecto if fondo_defecto is not None else 0xC0C0C0)
        img = _fuente.imagen_texto(p["texto"], col, fondo, p["fuente"])
        foto = tk.PhotoImage(width=1, height=1)
        foto._img = img
        c._foto = foto
        c.w.configure(image=foto, text="")
    except Exception as e:
        sys.print_exception(e)


@_props
class Etiqueta(Control):
    FONDO_HEREDADO = True
    tipo = "Etiqueta"
    PROPS = (("texto", "str", ""), ("alineacion", "enum:izquierda|centro|derecha", "izquierda"))
    EVENTOS = _ev("click")
    TAM = (80, 20)

    def _crear(self):
        self.w = tk.Label(self._contenedor_tk(), text="", anchor="w", justify="left")
        self.w.bind("<Button-1>", lambda e: self._disparar("click"))
        self._foto = None

    def _aplicar(self, n):
        if n == "texto":
            self._cfg(text=str(self._p["texto"]))
            self._con_fuente()
        elif n == "alineacion":
            a = {"izquierda": "w", "centro": "center", "derecha": "e"}.get(self._p["alineacion"], "w")
            self._cfg(anchor=a, justify={"w": "left", "center": "center", "e": "right"}[a])
        elif n in ("fuente", "color"):
            Control._aplicar(self, n)
            self._con_fuente()
        else:
            Control._aplicar(self, n)

    def _aplicar_fondo(self):
        Control._aplicar_fondo(self)
        self._con_fuente()

    def _con_fuente(self):
        _texto_con_fuente(self, 0x1E2328, _color_int(self.form._p["color"], 0xC0C0C0))


@_props
class Boton(Control):
    tipo = "Boton"
    PROPS = (("texto", "str", ""), ("defecto", "bool", False))
    EVENTOS = _ev("click")
    TAM = (88, 26)

    def _crear(self):
        self.w = tk.Button(self._contenedor_tk(), text="", command=lambda: self._disparar("click"))
        self._foto = None

    def _aplicar(self, n):
        if n == "texto":
            self._cfg(text=str(self._p["texto"]))
            _texto_con_fuente(self, 0x1E2328, 0xE2E6EA if self._p["colorfondo"] is None else None)
        elif n in ("fuente", "color", "colorfondo"):
            Control._aplicar(self, n)
            _texto_con_fuente(self, 0x1E2328, 0xE2E6EA if self._p["colorfondo"] is None else None)
        elif n == "defecto":
            if self._p["defecto"]:
                self.form.win.bind("<Return>", lambda e: self._disparar("click"))
        else:
            Control._aplicar(self, n)

    def pulsar(self):
        self._disparar("click")


@_props
class CuadroTexto(Control):
    tipo = "CuadroTexto"
    PROPS = (("texto", "str", ""), ("multilinea", "bool", False), ("clave", "bool", False),
             ("solo_lectura", "bool", False), ("campo", "str", ""))
    EVENTOS = _ev("cambio", "tecla", "click", "intro", "validar")
    TAM = (120, 22)
    _ignorar = False

    def _crear(self):
        c = self._contenedor_tk()
        if self._p["multilinea"]:
            self.w = tk.Text(c, width=10, height=3, wrap="word")
            self.w.bind("<Key>", lambda e: self.w.after_idle(self._texto_cambio))
            self._ult = ""
        else:
            self._var = tk.StringVar()
            self.w = tk.Entry(c, textvariable=self._var)
            self._var.trace_add("write", lambda *a: self._texto_cambio())
        self.w.bind("<Button-1>", lambda e: self._disparar("click"))
        self.w.bind("<Key>", self._tecla, add="+")
        self.w.bind("<FocusOut>", self._validar)

    def _intro(self, ev):
        """Intro (en un cuadro de varias líneas, Ctrl+Intro) dispara «intro»."""
        if self._p["multilinea"] and not ev.state & 4:
            return
        if self._hay("intro"):
            self._disparar("intro")

    def _validar(self, ev):
        """Al perder el foco: «validar». Si el manejador pone `cancelar = True`, el foco vuelve al cuadro."""
        if self.form._cerrado or not self._hay("validar"):
            return
        self.cancelar = False
        self._disparar("validar")
        if self.cancelar:
            self.cancelar = False
            self.w.after_idle(self.enfocar)

    def _tecla(self, ev):
        self.tecla = ev.keysym
        self.caracter = ev.char
        self._disparar("tecla")
        if ev.keysym == "Return":
            self._intro(ev)

    tecla = ""
    caracter = ""

    def _texto_cambio(self):
        if self._ignorar:
            return
        if self._p["multilinea"]:
            t = self.w.get("1.0", "end-1c")
            if t == self._ult:
                return
            self._ult = t
        else:
            t = self._var.get()
        self._p["texto"] = t
        self._disparar("cambio")

    def _leer(self, n):
        if n == "texto" and self.w is not None:
            return self.w.get("1.0", "end-1c") if self._p["multilinea"] else self._var.get()
        return self._p.get(n)

    def _aplicar(self, n):
        if n == "texto":
            t = str(self._p["texto"])
            if self._p["multilinea"]:
                if t != self.w.get("1.0", "end-1c"):
                    self.w.delete("1.0", "end")
                    self.w.insert("1.0", t)
                    self._texto_cambio()
            elif t != self._var.get():
                self._var.set(t)
        elif n == "clave":
            if not self._p["multilinea"]:
                self._cfg(show="*" if self._p["clave"] else "")
        elif n == "solo_lectura":
            self._cfg(state="disabled" if self._p["solo_lectura"] or not self._p["activo"] else "normal")
        elif n == "activo":
            self._cfg(state="normal" if self._p["activo"] and not self._p["solo_lectura"] else "disabled")
        else:
            Control._aplicar(self, n)

    def seleccionar_todo(self):
        try:
            self.w.selection_range(0, "end")
        except Exception:
            pass


@_props
class Casilla(Control):
    FONDO_HEREDADO = True
    tipo = "Casilla"
    PROPS = (("texto", "str", ""), ("valor", "bool", False), ("campo", "str", ""))
    EVENTOS = _ev("click", "cambio")
    TAM = (120, 20)

    def _crear(self):
        self._var = tk.IntVar(value=0)
        self.w = tk.Checkbutton(self._contenedor_tk(), text="", variable=self._var,
                                command=self._pulsado, anchor="w")

    def _pulsado(self):
        self._p["valor"] = bool(self._var.get())
        self._disparar("click")
        self._disparar("cambio")

    def _leer(self, n):
        if n == "valor" and self.w is not None:
            return bool(self._var.get())
        return self._p.get(n)

    def _aplicar(self, n):
        if n == "texto":
            self._cfg(text=str(self._p["texto"]))
        elif n == "valor":
            self._var.set(1 if self._p["valor"] else 0)
        else:
            Control._aplicar(self, n)


@_props
class Opcion(Control):
    FONDO_HEREDADO = True
    """Botón de radio. Las opciones con el mismo grupo dentro del mismo formulario o
    marco son excluyentes."""
    tipo = "Opcion"
    PROPS = (("texto", "str", ""), ("valor", "bool", False), ("grupo", "str", "1"))
    EVENTOS = _ev("click", "cambio")
    TAM = (120, 20)

    def _crear(self):
        g = self.padre._grupos
        gr = self._p["grupo"]
        if gr not in g:
            g[gr] = tk.StringVar(value="")
        self._var = g[gr]
        self.w = tk.Radiobutton(self._contenedor_tk(), text="", variable=self._var,
                                value=self.nombre, command=self._pulsado, anchor="w")

    def _pulsado(self):
        self._p["valor"] = True
        self._disparar("click")
        self._disparar("cambio")

    def _leer(self, n):
        if n == "valor" and self.w is not None:
            return self._var.get() == self.nombre
        return self._p.get(n)

    def _aplicar(self, n):
        if n == "texto":
            self._cfg(text=str(self._p["texto"]))
        elif n == "valor":
            if self._p["valor"]:
                self._var.set(self.nombre)
            elif self._var.get() == self.nombre:
                self._var.set("")
        else:
            Control._aplicar(self, n)


@_props
class Lista(Control):
    tipo = "Lista"
    PROPS = (("elementos", "lista", []), ("seleccion", "int", -1))
    EVENTOS = _ev("seleccion", "doble_click", "click")
    DOBLE_PROPIO = True
    EVENTO_DEFECTO = "seleccion"
    TAM = (120, 90)

    def _crear(self):
        self.w = tk.Listbox(self._contenedor_tk(), width=10, height=4)
        self.w.bind("<<ListboxSelect>>", lambda e: self._disparar("seleccion"))
        self.w.bind("<Double-Button-1>", lambda e: self._disparar("doble_click"))

    def _leer(self, n):
        if n == "elementos":
            return list(self._p["elementos"])
        if n == "seleccion":
            s = self.w.curselection() if self.w is not None else ()
            return int(s[0]) if s else -1
        if n == "valor":
            i = self._leer("seleccion")
            return self._p["elementos"][i] if 0 <= i < len(self._p["elementos"]) else ""
        return self._p.get(n)

    valor = property(lambda self: self._leer("valor"))

    def _aplicar(self, n):
        if n == "elementos":
            self.w.delete(0, "end")
            for t in self._p["elementos"]:
                self.w.insert("end", str(t))
        elif n == "seleccion":
            i = int(self._p["seleccion"])
            self.w.selection_clear(0, "end")
            if 0 <= i < len(self._p["elementos"]):
                self.w.selection_set(i)
        else:
            Control._aplicar(self, n)

    def agregar(self, texto):
        self._p["elementos"].append(str(texto))
        self.w.insert("end", str(texto))

    def quitar(self, i):
        del self._p["elementos"][i]
        self.w.delete(i)

    def limpiar(self):
        self._p["elementos"] = []
        self.w.delete(0, "end")

    def contar(self):
        return len(self._p["elementos"])


@_props
class Combo(Control):
    tipo = "Combo"
    PROPS = (("elementos", "lista", []), ("texto", "str", ""), ("editable", "bool", False), ("campo", "str", ""))
    EVENTOS = _ev("seleccion", "cambio")
    EVENTO_DEFECTO = "seleccion"
    TAM = (120, 22)

    def _crear(self):
        self._var = tk.StringVar()
        self.w = ttk.Combobox(self._contenedor_tk(), textvariable=self._var, values=[])
        self.w.bind("<<ComboboxSelected>>", lambda e: self._elegido())
        self._var.trace_add("write", lambda *a: self._escrito())

    def _elegido(self):
        self._p["texto"] = self._var.get()
        self._disparar("seleccion")

    def _escrito(self):
        self._p["texto"] = self._var.get()
        self._disparar("cambio")

    def _leer(self, n):
        if n == "texto" and self.w is not None:
            return self._var.get()
        if n == "elementos":
            return list(self._p["elementos"])
        if n == "seleccion":
            try:
                return self._p["elementos"].index(self._var.get())
            except ValueError:
                return -1
        return self._p.get(n)

    seleccion = property(lambda self: self._leer("seleccion"),
                         lambda self, i: self._escribir("texto", self._p["elementos"][i]))

    def _aplicar(self, n):
        if n == "elementos":
            self._cfg(values=[str(t) for t in self._p["elementos"]])
        elif n == "texto":
            if self._var.get() != str(self._p["texto"]):
                self._var.set(str(self._p["texto"]))
        elif n == "editable":
            self._cfg(state="normal" if self._p["editable"] else "readonly")
        else:
            Control._aplicar(self, n)

    def agregar(self, texto):
        self._p["elementos"].append(str(texto))
        self._aplicar("elementos")

    def limpiar(self):
        self._p["elementos"] = []
        self._aplicar("elementos")


@_props
class Imagen(Control):
    tipo = "Imagen"
    PROPS = (("archivo", "str", ""), ("ajustar", "bool", True))
    EVENTOS = _ev("click")
    TAM = (96, 96)

    def _crear(self):
        self.w = tk.Label(self._contenedor_tk(), text="", relief="sunken", borderwidth=1)
        self.w.bind("<Button-1>", lambda e: self._disparar("click"))
        self._foto = None

    def _aplicar(self, n):
        if n in ("archivo", "ajustar", "ancho", "alto"):
            if n in ("ancho", "alto"):
                self._colocar()
            self._cargar()
        else:
            Control._aplicar(self, n)

    def _cargar(self):
        a = self._p["archivo"]
        self._foto = None
        if not a:
            self._cfg(image="", text="")
            return
        try:
            f = tk.PhotoImage(file=a)
            if self._p["ajustar"]:
                k = 1
                while f.width() // k > self._p["ancho"] or f.height() // k > self._p["alto"]:
                    k += 1
                if k > 1:
                    f = f.subsample(k, k)
            self._foto = f
            self.w.configure(image=f, text="")
        except Exception:
            self._cfg(image="", text="(sin imagen)")


@_props
class Marco(Control):
    FONDO_HEREDADO = True
    tipo = "Marco"
    PROPS = (("texto", "str", ""),)
    EVENTOS = _ev("click")
    TAM = (160, 100)
    CONTENEDOR = True

    def _crear(self):
        self.w = tk.LabelFrame(self._contenedor_tk(), text="")
        self._grupos = {}
        self.w.bind("<Button-1>", lambda e: self._disparar("click"))

    def _contenedor_tk(self):
        return self.w if self.w is not None else self.padre._contenedor_tk()

    def _aplicar(self, n):
        if n == "texto":
            self._cfg(text=str(self._p["texto"]))
        else:
            Control._aplicar(self, n)


@_props
class Deslizador(Control):
    FONDO_HEREDADO = True
    """Barra para elegir un valor entre minimo y maximo (VB: HScrollBar/VScrollBar)."""
    tipo = "Deslizador"
    PROPS = (("valor", "int", 0), ("minimo", "int", 0), ("maximo", "int", 100),
             ("vertical", "bool", False))
    EVENTOS = _ev("cambio")
    EVENTO_DEFECTO = "cambio"
    TAM = (160, 36)

    def _crear(self):
        self._ignorar = True
        self.w = tk.Scale(self._contenedor_tk(), orient="vertical" if self._p["vertical"] else "horizontal",
                          command=self._movido)
        self._ignorar = False

    def _movido(self, v):
        if self._ignorar:
            return
        self._p["valor"] = int(float(v))
        self._disparar("cambio")

    def _leer(self, n):
        if n == "valor" and self.w is not None:
            return int(float(self.w.get()))
        return self._p.get(n)

    def _aplicar(self, n):
        self._ignorar = True
        if n == "valor":
            self.w.set(int(self._p["valor"]))
        elif n in ("minimo", "maximo"):
            self._cfg(**{"from": int(self._p["minimo"]), "to": int(self._p["maximo"])})
        else:
            Control._aplicar(self, n)
        self._ignorar = False


BarraDesplazamiento = Deslizador


@_props
class BarraProgreso(Control):
    tipo = "BarraProgreso"
    PROPS = (("valor", "int", 0), ("maximo", "int", 100))
    EVENTOS = _ev()
    TAM = (160, 20)

    def _crear(self):
        self.w = ttk.Progressbar(self._contenedor_tk(), orient="horizontal", maximum=100)

    def _aplicar(self, n):
        if n == "valor":
            self._cfg(value=float(self._p["valor"]))
        elif n == "maximo":
            self._cfg(maximum=float(self._p["maximo"]))
        else:
            Control._aplicar(self, n)


@_props
class Temporizador(Control):
    """Sin ventana propia: dispara «tic» cada `intervalo` milisegundos mientras esté activo."""
    tipo = "Temporizador"
    PROPS = (("intervalo", "int", 1000),)
    EVENTOS = ("tic",)
    EVENTO_DEFECTO = "tic"
    ORDEN = ("intervalo",)
    TAM = (32, 32)

    def __init__(self, padre, *args, **kw):
        kw.setdefault("activo", True)
        Control.__init__(self, padre, *args, **kw)

    def _crear(self):
        self._id = None
        self._viva = False

    def _colocar(self):
        pass

    def _escribir(self, n, v):
        # no tiene ventana (self.w es None): Control._escribir no llamaría a _aplicar y
        # `Temporizador1.activo = True` no pondría en marcha el temporizador
        self._p[n] = v
        self._aplicar(n)

    def _aplicar(self, n):
        if n in ("activo", "intervalo"):
            self._reprogramar()

    def _reprogramar(self):
        if self._id is not None:
            try:
                self.form.win.after_cancel(self._id)
            except Exception:
                pass
            self._id = None
        if self._p["activo"] and not self.form._cerrado and self.form.win is not None:
            self._id = self.form.win.after(max(10, int(self._p["intervalo"])), self._tic)

    def _tic(self):
        self._id = None
        if not self._p["activo"] or self.form._cerrado:
            return
        self._disparar("tic")
        if self._p["activo"] and not self.form._cerrado and self._id is None:
            self._id = self.form.win.after(max(10, int(self._p["intervalo"])), self._tic)

    def destruir(self):
        self._p["activo"] = False
        self._reprogramar()
        self.form.controles.pop(self.nombre, None)


def _parsear_menu(lineas):
    """['Archivo', '  Abrir|MnuAbrir', '  -'] -> [(titulo, [(texto, nombre) | None])]"""
    r = []
    for l in lineas:
        if not l.strip():
            continue
        if l[0] not in " \t":
            r.append((l.strip(), []))
        elif r:
            t = l.strip()
            if t == "-":
                r[-1][1].append(None)
            else:
                t, _, nom = t.partition("|")
                r[-1][1].append((t.strip(), nom.strip() or None))
    return r


@_props
class Menu(Control):
    """Barra de menús del formulario. `elementos` es una lista de líneas: el título de cada
    menú sin sangría, y debajo sus opciones con sangría («Abrir|MnuAbrir»; «-» es una
    línea). Cada opción con nombre dispara el evento click de ese nombre: MnuAbrir_click."""
    tipo = "Menu"
    PROPS = (("elementos", "lista", []),)
    EVENTOS = ()
    TAM = (32, 32)

    def _crear(self):
        self._barra = None

    def _colocar(self):
        pass

    def _aplicar(self, n):
        if n != "elementos":
            return
        barra = tk.Menu(self.form.win)
        for titulo, items in _parsear_menu(self._p["elementos"]):
            m = tk.Menu(barra, tearoff=0)
            for it in items:
                if it is None:
                    m.add_separator()
                else:
                    m.add_command(label=it[0], command=self._comando(it[1]))
            barra.add_cascade(label=titulo, menu=m)
        self._barra = barra
        self.form.win.configure(menu=barra)

    def _comando(self, nombre):
        def f():
            if nombre:
                self._disparar_nombre(nombre)
        return f

    def _disparar_nombre(self, nombre):
        for f in self._eventos.get(nombre, ()):
            _llamar(f, nombre + "_click")
        ns = self.form._ns
        if ns is not None:
            f = ns.get(nombre + "_click")
            if f is not None and callable(f):
                _llamar(f, nombre + "_click")


# ------------------------------------------------------------ componentes nuevos (v8.3.2, D303)
# Todos sobre el tkinter del sistema. Los que dibujan algo propio (Pestanas, Tabla, BarraEstado,
# Enlace, SelectorColor) son subclases de los componentes de tkinter con su propio `_desenhar`, de
# modo que el diseñador los dibuja con el mismo código que la aplicación.

def _csv_linea(linea, sep):
    """Una línea de CSV -> lista de textos (las comillas dobles protegen el separador)."""
    r = []
    cur = ""
    q = False
    i = 0
    while i < len(linea):
        c = linea[i]
        if q:
            if c == '"':
                if linea[i + 1:i + 2] == '"':
                    cur += '"'
                    i += 1
                else:
                    q = False
            else:
                cur += c
        elif c == '"':
            q = True
        elif c == sep:
            r.append(cur)
            cur = ""
        else:
            cur += c
        i += 1
    r.append(cur)
    return r


def _csv_texto(filas, sep):
    """Lista de listas -> texto CSV (con comillas donde hace falta)."""
    out = []
    for f in filas:
        celdas = []
        for c in f:
            c = str(c)
            if sep in c or '"' in c or "\n" in c:
                c = '"' + c.replace('"', '""') + '"'
            celdas.append(c)
        out.append(sep.join(celdas))
    return "\n".join(out) + ("\n" if out else "")


def _leer_texto(ruta):
    f = open(ruta)
    try:
        return f.read()
    finally:
        f.close()


# --- Pestanas

TAB_H = 24


def _pest_tramos(titulos):
    """[(x0, x1)] de cada pestaña, en píxeles desde el borde izquierdo del control."""
    r = []
    x = 2
    for t in titulos:
        w = len(str(t)) * 8 + 18
        r.append((x, x + w))
        x += w
    return r


@_props
class Pestanas(Control):
    """Contenedor con páginas (pestañas). Un control pertenece a una página con su propiedad
    `pagina` (0 es la primera); solo se ven los de la página elegida (`seleccion`)."""
    FONDO_HEREDADO = True
    tipo = "Pestanas"
    PROPS = (("elementos", "lista", ["General", "Opciones"]), ("seleccion", "int", 0))
    EVENTOS = _ev("cambio", "click")
    EVENTO_DEFECTO = "cambio"
    ORDEN = ("x", "y", "ancho", "alto")
    TAM = (240, 160)
    CONTENEDOR = True

    def _crear(self):
        from vps import _widgets
        self.w = _widgets._PestanasW(self._contenedor_tk())
        self._grupos = {}
        self.w.bind("<Button-1>", self._clic)

    def _contenedor_tk(self):
        return self.w if self.w is not None else self.padre._contenedor_tk()

    def _hijo_visible(self, c):
        return int(c._p.get("pagina") or 0) == int(self._p["seleccion"])

    def _clic(self, ev):
        if ev.y >= TAB_H:
            self._disparar("click")
            return
        for i, (a, b) in enumerate(_pest_tramos(self._p["elementos"])):
            if a <= ev.x < b:
                if i != self._p["seleccion"]:
                    self._p["seleccion"] = i
                    self._aplicar("seleccion")
                    self._disparar("cambio")
                return

    def _aplicar(self, n):
        if n == "elementos":
            self.w.titulos = [str(t) for t in self._p["elementos"]]
            self.w._redesenhar()
        elif n == "seleccion":
            i = max(0, min(int(self._p["seleccion"]), max(0, len(self._p["elementos"]) - 1)))
            self._p["seleccion"] = i
            self.w.actual = i
            self.w._redesenhar()
            for c in list(self.form.controles.values()):
                if c.padre is self:
                    c._colocar()
        else:
            Control._aplicar(self, n)

    def _leer(self, n):
        if n == "elementos":
            return list(self._p["elementos"])
        if n == "valor":
            i = int(self._p["seleccion"])
            return str(self._p["elementos"][i]) if 0 <= i < len(self._p["elementos"]) else ""
        return self._p.get(n)

    valor = property(lambda self: self._leer("valor"))


# --- Tabla

@_props
class Tabla(Control):
    """Rejilla de texto: `columnas` (los títulos) y `filas` (cada una, una lista de celdas; en el
    diseñador, una línea «a;b;c» por fila). Un clic elige una fila (`seleccion`); un clic en la
    cabecera ordena por esa columna; con `editable`, un doble clic edita la celda."""
    tipo = "Tabla"
    PROPS = (("columnas", "lista", ["A", "B"]), ("filas", "lista", []), ("seleccion", "int", -1),
             ("editable", "bool", False), ("ordenable", "bool", True))
    EVENTOS = _ev("seleccion", "cambio", "doble_click", "click", "celda_cambio", "ordenar")
    DOBLE_PROPIO = True
    EVENTO_DEFECTO = "seleccion"
    ORDEN = ("x", "y", "ancho", "alto")
    TAM = (260, 120)
    columna = -1       # la columna de la última celda pulsada (o de la celda editada, u ordenada)
    fila = -1          # la fila de la última celda editada (evento celda_cambio)
    descendente = False   # el sentido del último orden (evento ordenar)

    def _crear(self):
        from vps import _widgets
        self.w = _widgets._TablaW(self._contenedor_tk())
        self.w.ctl = self
        self._filas = []

    @staticmethod
    def _normalizar(filas):
        r = []
        for f in filas or []:
            r.append(f.split(";") if isinstance(f, str) else list(f))
        return r

    def _pulsado(self, f, c):
        self.columna = c
        if f == -1:
            if self._p["ordenable"]:
                self.ordenar(c, self.w.orden is not None and self.w.orden[0] == c and not self.w.orden[1])
            return
        self._elegir(f)
        self._disparar("click")

    def _elegir(self, f):
        self.w.sel = f
        self.w.ver(f)
        self.w._redesenhar()
        self._disparar("seleccion")

    def _doble(self, f, c):
        self.columna = c
        self._disparar("doble_click")
        if self._p["editable"]:
            self.w.editar(f, c)

    def _leer(self, n):
        if n == "filas":
            return [list(f) for f in self._filas]
        if n == "columnas":
            return list(self._p["columnas"])
        if n == "seleccion":
            return self.w.sel if self.w is not None else -1
        if n == "valor":
            i = self.w.sel
            return list(self._filas[i]) if 0 <= i < len(self._filas) else []
        return self._p.get(n)

    valor = property(lambda self: self._leer("valor"))

    def _aplicar(self, n):
        if n == "columnas":
            self.w.columnas = [str(c) for c in self._p["columnas"]]
            self.w._redesenhar()
        elif n == "filas":
            self._filas = self._normalizar(self._p["filas"])
            self._p["filas"] = self._filas
            self._refrescar()
        elif n == "seleccion":
            i = int(self._p["seleccion"])
            self.w.sel = i if 0 <= i < len(self._filas) else -1
            self.w._redesenhar()
        else:
            Control._aplicar(self, n)

    def _aplicar_fondo(self):
        self._cfg(background=_a_tk(self._p["colorfondo"]))
        self.w._redesenhar()

    def _refrescar(self):
        self.w.filas = self._filas
        self.w.sel = -1
        self.w.arriba = 0
        self.w.cerrar_edicion(False)
        self.w._redesenhar()

    def agregar(self, fila):
        """Añade una fila (una lista, o un texto «a;b;c»)."""
        self._filas.append(fila.split(";") if isinstance(fila, str) else list(fila))
        self._p["filas"] = self._filas
        self.w.filas = self._filas
        self.w._redesenhar()

    def quitar(self, i):
        del self._filas[i]
        if self.w.sel == i:
            self.w.sel = -1
        elif self.w.sel > i:
            self.w.sel -= 1
        self.w._redesenhar()

    def limpiar(self):
        self._filas = []
        self._p["filas"] = self._filas
        self._refrescar()

    def contar(self):
        return len(self._filas)

    def celda(self, f, c):
        return self._filas[f][c]

    def poner_celda(self, f, c, valor):
        fila = self._filas[f]
        while len(fila) <= c:
            fila.append("")
        fila[c] = valor
        self.w._redesenhar()

    def ordenar(self, columna, descendente=False):
        """Ordena por una columna (números como números, el resto como texto)."""
        def clave(f):
            t = f[columna] if columna < len(f) else ""
            try:
                return (0, float(t), "")
            except (ValueError, TypeError):
                return (1, 0, str(t).lower())
        elegida = self._filas[self.w.sel] if 0 <= self.w.sel < len(self._filas) else None
        self._filas.sort(key=clave, reverse=bool(descendente))
        self.w.orden = (columna, bool(descendente))
        self.w.sel = -1
        if elegida is not None:
            for i, f in enumerate(self._filas):
                if f is elegida:
                    self.w.sel = i
        self.w._redesenhar()
        self.columna = columna
        self.descendente = bool(descendente)
        self._disparar("ordenar")

    def cargar_csv(self, ruta, separador=";", encabezado=True):
        """Lee un archivo CSV: la primera línea son las columnas (con encabezado=True)."""
        lineas = [l.rstrip("\r") for l in _leer_texto(ruta).split("\n") if l.strip()]
        filas = [_csv_linea(l, separador) for l in lineas]
        if encabezado and filas:
            self._p["columnas"] = filas.pop(0)
            self._aplicar("columnas")
        self._p["filas"] = filas
        self._aplicar("filas")

    def guardar_csv(self, ruta, separador=";"):
        """Escribe las columnas y las filas en un archivo CSV."""
        f = open(ruta, "w")
        try:
            f.write(_csv_texto([self._p["columnas"]] + self._filas, separador))
        finally:
            f.close()


# --- Numerico

@_props
class Numerico(Control):
    """Cuadro de número con flechas arriba y abajo: `valor` entre `minimo` y `maximo`, de `paso`
    en `paso`; con `decimales` cifras decimales (0 = enteros)."""
    tipo = "Numerico"
    PROPS = (("valor", "num", 0), ("minimo", "num", 0), ("maximo", "num", 100), ("paso", "num", 1),
             ("decimales", "int", 0), ("campo", "str", ""))
    EVENTOS = _ev("cambio")
    EVENTO_DEFECTO = "cambio"
    ORDEN = ("x", "y", "ancho", "alto")
    TAM = (80, 22)
    _ignorar = True

    def _crear(self):
        self._var = tk.StringVar()
        self.w = tk.Spinbox(self._contenedor_tk(), textvariable=self._var)
        self._var.trace_add("write", lambda *a: self._cambiado())

    def _cambiado(self):
        if self._ignorar:
            return
        self._p["valor"] = self._leer("valor")
        self._disparar("cambio")

    def _formato(self, x):
        d = int(self._p["decimales"] or 0)
        if d > 0:
            return ("%." + str(d) + "f") % x
        return str(int(round(x)))

    def _leer(self, n):
        if n == "valor" and self.w is not None:
            try:
                x = float(self._var.get().replace(",", "."))
            except ValueError:
                x = float(self._p["minimo"])
            x = max(float(self._p["minimo"]), min(float(self._p["maximo"]), x))
            return round(x, int(self._p["decimales"] or 0)) if self._p["decimales"] else int(round(x))
        return self._p.get(n)

    def _aplicar(self, n):
        ant = self._ignorar
        self._ignorar = True
        if n == "valor":
            self._var.set(self._formato(float(self._p["valor"])))
        elif n in ("minimo", "maximo", "paso"):
            self._cfg(**{"from": float(self._p["minimo"]), "to": float(self._p["maximo"]),
                         "increment": float(self._p["paso"])})
        elif n == "decimales":
            d = int(self._p["decimales"] or 0)
            self._cfg(format=("%%.%df" % d) if d else "")
            self._var.set(self._formato(float(self._p["valor"])))
        elif n == "activo":
            self._cfg(state="normal" if self._p["activo"] else "disabled")
        else:
            Control._aplicar(self, n)
        self._ignorar = ant

    def _aplicar_todo(self):
        self._ignorar = True
        Control._aplicar_todo(self)
        self._aplicar("minimo")
        self._aplicar("valor")
        self._ignorar = False


# --- Lienzo

@_props
class Lienzo(Control):
    """Superficie para dibujar: linea, rectangulo, circulo, texto, imagen y limpiar. Los
    eventos de ratón (click, arrastrar, soltar, mover) dejan la posición en raton_x y raton_y,
    en píxeles del lienzo."""
    tipo = "Lienzo"
    PROPS = ()
    EVENTOS = _ev("click", "arrastrar", "soltar", "mover")
    ORDEN = ("x", "y", "ancho", "alto")
    TAM = (200, 140)
    raton_x = 0
    raton_y = 0

    def _crear(self):
        self.w = tk.Canvas(self._contenedor_tk(), background="#ffffff", borderwidth=1, relief="sunken",
                           highlightthickness=0)
        self._fotos = []
        for seq, nombre in (("<Button-1>", "click"), ("<B1-Motion>", "arrastrar"),
                            ("<ButtonRelease-1>", "soltar"), ("<Motion>", "mover")):
            self.w.bind(seq, lambda e, nombre=nombre: self._raton(e, nombre))

    def _raton(self, e, nombre):
        self.raton_x, self.raton_y = e.x, e.y
        self._disparar(nombre)

    def _aplicar_fondo(self):
        self._cfg(background=_a_tk(self._p["colorfondo"]) or "#ffffff")

    def _col(self, c, defecto):
        return _a_tk(c) or _a_tk(self._p["color"]) or defecto

    def linea(self, x1, y1, x2, y2, color=None, grosor=1):
        self.w.create_line(x1, y1, x2, y2, fill=self._col(color, "#000000"), width=grosor)

    def rectangulo(self, x, y, ancho, alto, color=None, relleno=None, grosor=1):
        kw = {"outline": self._col(color, "#000000"), "width": grosor}
        if relleno is not None:
            kw["fill"] = _a_tk(relleno)
        self.w.create_rectangle(x, y, x + ancho, y + alto, **kw)

    def circulo(self, cx, cy, radio, color=None, relleno=None, grosor=1):
        kw = {"outline": self._col(color, "#000000"), "width": grosor}
        if relleno is not None:
            kw["fill"] = _a_tk(relleno)
        self.w.create_oval(cx - radio, cy - radio, cx + radio, cy + radio, **kw)

    def texto(self, x, y, texto, color=None):
        self.w.create_text(x, y, text=str(texto), anchor="nw", fill=self._col(color, "#000000"))

    def imagen(self, x, y, archivo):
        foto = tk.PhotoImage(file=archivo)
        self._fotos.append(foto)
        self.w.create_image(x, y, image=foto, anchor="nw")

    def limpiar(self):
        self.w.delete("all")
        self._fotos = []


# --- Enlace

@_props
class Enlace(Control):
    """Etiqueta clicable, en azul y subrayada. `url` es un dato libre para el programa."""
    FONDO_HEREDADO = True
    tipo = "Enlace"
    PROPS = (("texto", "str", ""), ("url", "str", ""))
    EVENTOS = _ev("click")
    TAM = (100, 20)

    def _crear(self):
        from vps import _widgets
        self.w = _widgets._EnlaceW(self._contenedor_tk(), text="", anchor="w", justify="left", borderwidth=0,
                          foreground="#0000ee", cursor="hand2")
        self.w.bind("<Button-1>", lambda e: self._disparar("click"))

    def _aplicar(self, n):
        if n == "texto":
            self._cfg(text=str(self._p["texto"]))
        elif n == "url":
            pass
        else:
            Control._aplicar(self, n)


# --- BarraEstado

@_props
class BarraEstado(Control):
    """Barra de estado: `texto` es el mensaje de la izquierda y `elementos`, los paneles de la
    derecha (poner(i, texto) cambia uno)."""
    FONDO_HEREDADO = True
    tipo = "BarraEstado"
    PROPS = (("texto", "str", ""), ("elementos", "lista", []))
    EVENTOS = _ev("click")
    TAM = (320, 22)

    def _crear(self):
        from vps import _widgets
        self.w = _widgets._BarraW(self._contenedor_tk())
        self.w.bind("<Button-1>", lambda e: self._disparar("click"))

    def _aplicar(self, n):
        if n == "texto":
            self.w.texto = str(self._p["texto"])
            self.w._redesenhar()
        elif n == "elementos":
            self.w.paneles = [str(t) for t in self._p["elementos"]]
            self.w._redesenhar()
        else:
            Control._aplicar(self, n)

    def _aplicar_fondo(self):
        self._cfg(background=_a_tk(self._p["colorfondo"] if self._p["colorfondo"] is not None else self.form._p["color"]))
        self.w._redesenhar()

    def poner(self, i, texto):
        """Cambia el texto del panel de la derecha número i."""
        while len(self._p["elementos"]) <= i:
            self._p["elementos"].append("")
        self._p["elementos"][i] = str(texto)
        self._aplicar("elementos")


# --- SelectorColor

PALETA = [0x000000, 0x808080, 0xC0C0C0, 0xFFFFFF, 0x800000, 0xFF0000, 0x808000, 0xFFFF00,
          0x008000, 0x00FF00, 0x008080, 0x00FFFF, 0x000080, 0x0000FF, 0x800080, 0xFF00FF,
          0xFFA500, 0xFFC0CB, 0x804020, 0xF5DEB3, 0x87CEEB, 0x90EE90, 0xDDA0DD, 0xFA8072,
          0x2A6FD8, 0x0B7A3E, 0xB22222, 0x4B0082, 0xD2B48C, 0x40E0D0, 0x696969, 0xE6E6FA]


@_props
class SelectorColor(Control):
    """Botón que enseña un color y, al pulsarlo, abre una paleta para elegir otro: `valor` es
    el color (0xRRGGBB); el evento `cambio` salta al elegirlo."""
    tipo = "SelectorColor"
    PROPS = (("valor", "color", 0x2A6FD8),)
    EVENTOS = _ev("cambio", "click")
    EVENTO_DEFECTO = "cambio"
    ORDEN = ("x", "y", "ancho", "alto")
    TAM = (112, 24)

    def _crear(self):
        from vps import _widgets
        self.w = _widgets._SelectorW(self._contenedor_tk())
        self.w.bind("<Button-1>", lambda e: self.elegir())

    def _leer(self, n):
        if n == "valor":
            return _color_int(self._p["valor"], 0)
        return self._p.get(n)

    def _aplicar(self, n):
        if n == "valor":
            self.w.valor = _color_int(self._p["valor"], 0)
            self.w._redesenhar()
        elif n == "colorfondo":
            self._cfg(background=_a_tk(self._p["colorfondo"]))
            self.w._redesenhar()
        else:
            Control._aplicar(self, n)

    def elegir(self):
        """Abre la paleta (como al pulsar el control)."""
        if not self._p["activo"]:
            return
        self._disparar("click")
        d = tk.Toplevel(tk._raiz_padrao(), _dialogo=True)
        d.title("Elegir color")
        d.resizable(False, False)
        cv = tk.Canvas(d, width=8 * 28 + 8, height=4 * 28 + 8, background="#f4f4f0", highlightthickness=0)
        cv.pack(padx=6, pady=6)
        for i, c in enumerate(PALETA):
            cx, cy = 4 + (i % 8) * 28, 4 + (i // 8) * 28
            cv.create_rectangle(cx, cy, cx + 24, cy + 24, fill="#%06x" % c, outline="#000000")

        def pulsar(e):
            col, fil = (e.x - 4) // 28, (e.y - 4) // 28
            if 0 <= col < 8 and 0 <= fil < 4:
                self._p["valor"] = PALETA[fil * 8 + col]
                self._aplicar("valor")
                d.destroy()
                self._disparar("cambio")
        cv.bind("<Button-1>", pulsar)
        try:
            d.grab_set()
        except Exception:
            pass


# --- Reloj

_DIAS = ("lunes", "martes", "miercoles", "jueves", "viernes", "sabado", "domingo")
_MESES = ("enero", "febrero", "marzo", "abril", "mayo", "junio", "julio", "agosto", "septiembre",
          "octubre", "noviembre", "diciembre")


@_props
class Reloj(Temporizador):
    """Sin ventana: da la fecha y la hora del sistema (anio, mes, dia, hora, minuto, segundo,
    dia_semana, fecha, hora_texto, formatear) y, mientras esté activo, dispara «tic» cada
    `intervalo` milisegundos (por defecto, cada segundo)."""
    tipo = "Reloj"
    PROPS = (("intervalo", "int", 1000),)
    EVENTOS = ("tic",)
    EVENTO_DEFECTO = "tic"

    def ahora(self):
        """(anio, mes, dia, hora, minuto, segundo, dia_semana) con el lunes = 0."""
        t = time.localtime()
        return (t[0], t[1], t[2], t[3], t[4], t[5], t[6])

    anio = property(lambda self: self.ahora()[0])
    mes = property(lambda self: self.ahora()[1])
    dia = property(lambda self: self.ahora()[2])
    hora = property(lambda self: self.ahora()[3])
    minuto = property(lambda self: self.ahora()[4])
    segundo = property(lambda self: self.ahora()[5])
    dia_semana = property(lambda self: self.ahora()[6])
    fecha = property(lambda self: "%04d-%02d-%02d" % self.ahora()[:3])
    hora_texto = property(lambda self: "%02d:%02d:%02d" % self.ahora()[3:6])
    nombre_dia = property(lambda self: _DIAS[self.ahora()[6]])
    nombre_mes = property(lambda self: _MESES[self.ahora()[1] - 1])

    def formatear(self, plantilla="%d/%m/%Y %H:%M:%S"):
        """El texto con %Y %m %d %H %M %S sustituidos por la fecha y la hora actuales."""
        a = self.ahora()
        r = plantilla
        for k, v in (("%Y", "%04d" % a[0]), ("%m", "%02d" % a[1]), ("%d", "%02d" % a[2]),
                     ("%H", "%02d" % a[3]), ("%M", "%02d" % a[4]), ("%S", "%02d" % a[5])):
            r = r.replace(k, v)
        return r


# --- Archivo

@_props
class Archivo(Control):
    """Sin ventana: leer y escribir un archivo de texto o CSV (`ruta`). leer, escribir, agregar,
    leer_lineas, escribir_lineas, leer_csv, escribir_csv, existe y borrar."""
    tipo = "Archivo"
    PROPS = (("ruta", "str", ""), ("separador", "str", ";"))
    EVENTOS = ()
    ORDEN = ("ruta",)
    TAM = (32, 32)

    def _colocar(self):
        pass

    def _aplicar(self, n):
        pass

    def existe(self):
        import os
        try:
            os.stat(self._p["ruta"])
            return True
        except OSError:
            return False

    def leer(self):
        """Todo el texto del archivo ("" si no existe)."""
        try:
            return _leer_texto(self._p["ruta"])
        except OSError:
            return ""

    def _escribir_modo(self, texto, modo):
        f = open(self._p["ruta"], modo)
        try:
            f.write(texto)
        finally:
            f.close()

    def escribir(self, texto):
        """Sustituye el contenido del archivo por `texto`."""
        self._escribir_modo(str(texto), "w")

    def agregar(self, texto):
        """Añade `texto` al final del archivo."""
        self._escribir_modo(str(texto), "a")

    def leer_lineas(self):
        """Las líneas del archivo, sin el salto de línea."""
        return [l.rstrip("\r") for l in self.leer().split("\n") if l != ""]

    def escribir_lineas(self, lineas):
        self.escribir("".join(str(l) + "\n" for l in lineas))

    def leer_csv(self):
        """El CSV como lista de listas de texto (el separador es la propiedad `separador`)."""
        return [_csv_linea(l, self._p["separador"]) for l in self.leer_lineas()]

    def escribir_csv(self, filas):
        self.escribir(_csv_texto(filas, self._p["separador"]))

    def borrar(self):
        import os
        try:
            os.remove(self._p["ruta"])
        except OSError:
            pass


# ------------------------------------------------------------ formulario

@_props
class Formulario:
    tipo = "Formulario"
    PROPS = (("texto", "str", "Formulario"), ("ancho", "int", 320), ("alto", "int", 200),
             ("color", "color", 0xC0C0C0), ("redimensionable", "bool", False))
    # «cerrar» es el evento antiguo (devolver False cancelaba el cierre); «cerrando» lo sustituye (D315).
    # No hay «soltar_archivo»: el sistema aún no tiene arrastrar y soltar archivos sobre las ventanas.
    EVENTOS = ("cargar", "cerrando", "cerrado", "redimensionar", "activar", "desactivar", "tecla", "click", "cerrar")
    EVENTO_DEFECTO = "cargar"
    ORDEN = ("texto", "ancho", "alto")

    def __init__(self, *args, **kw):
        self.controles = {}       # primero: __getattr__ lo necesita
        self._p = {}
        for n, t, d in self.PROPS:
            self._p[n] = d
        for i in range(len(args)):
            kw[self.ORDEN[i]] = args[i]
        self.nombre = kw.pop("nombre", "Formulario")
        for n, v in kw.items():
            if n not in self._p:
                raise TypeError("Formulario no tiene la propiedad «%s»" % n)
            self._p[n] = v
        self.form = self
        self._contadores = {}
        self.resultado = None     # lo que devolvió cerrar(valor) (formulario modal)
        self.datos = {}           # para pasar datos entre formularios
        self._oculto = False
        self._prev = (int(self._p["ancho"]), int(self._p["alto"]))
        self._eventos = {}
        self._ns = None
        self._cerrado = False
        self._cargado = False
        self._grupos = {}
        self.tecla = ""
        self.caracter = ""
        self.cancelar = False     # en «cerrando»: ponerlo a True cancela el cierre
        self._activo_f = False    # el formulario está activo (tiene el foco): eventos activar y desactivar
        if tk._raiz is None:
            self.win = tk.Tk()
            self.principal = True
        else:
            self.win = tk.Toplevel(tk._raiz)
            self.principal = False
        self.win.protocol("WM_DELETE_WINDOW", self._cerrar)
        self.win.bind("<Key>", self._tecla)
        self.win.bind("<Button-1>", lambda e: self._disparar("click") if e.widget is self.win else None)
        self.win.bind("<Configure>", self._configurado)
        self.win.bind("<FocusIn>", self._foco_dentro, add="+")
        self.win.bind("<FocusOut>", self._foco_fuera, add="+")
        for n in self._p:
            self._aplicar(n)
        _formularios.append(self)

    def __getattr__(self, n):
        # los controles como atributos: Formulario2.CuadroTexto1.texto
        if n[:1] != "_" and n != "controles":
            c = self.controles.get(n)
            if c is not None:
                return c
        raise AttributeError(n)

    # propiedades (texto, ancho, alto, color...)
    def _leer(self, n):
        return self._p.get(n)

    def _escribir(self, n, v):
        self._p[n] = v
        self._aplicar(n)
        if n in ("ancho", "alto"):
            self._ajustar_anclajes()

    def _ajustar_anclajes(self):
        """Cambió el tamaño del formulario: los controles con anclaje se mueven o se estiran."""
        w, h = int(self._p["ancho"]), int(self._p["alto"])
        pw, ph = self._prev
        self._prev = (w, h)
        if w != pw or h != ph:
            for c in list(self.controles.values()):
                if c.padre is self:
                    c._anclar(w - pw, h - ph)

    def _aplicar(self, n):
        p = self._p
        if n == "texto":
            self.win.title(str(p["texto"]))
        elif n in ("ancho", "alto"):
            self.win.geometry("%dx%d" % (int(p["ancho"]), int(p["alto"])))
        elif n == "color":
            c = _a_tk(p["color"])
            if c:
                self.win.configure(background=c)
            for ctl in self.controles.values():
                if ctl.FONDO_HEREDADO and ctl.w is not None:
                    ctl._aplicar_fondo()
        elif n == "redimensionable":
            self.win.resizable(bool(p["redimensionable"]), bool(p["redimensionable"]))

    def _configurado(self, ev):
        if ev.widget is self.win and self._cargado:
            self._p["ancho"], self._p["alto"] = ev.width, ev.height
            self._ajustar_anclajes()
            self._disparar("redimensionar")

    # contenedor de los controles
    def _contenedor_tk(self):
        return self.win

    def _nombre_nuevo(self, tipo):
        n = self._contadores.get(tipo, 0) + 1
        while True:
            nombre = "%s%d" % (tipo, n)
            if nombre not in self.controles:
                break
            n += 1
        self._contadores[tipo] = n
        return nombre

    def _registrar(self, c):
        self.controles[c.nombre] = c

    def control(self, nombre):
        return self.controles[nombre]

    def conectar(self, nombre, f):
        self._eventos.setdefault(nombre, []).append(f)

    def _disparar(self, nombre):
        donde = "%s_%s" % (self.nombre, nombre)
        for f in self._eventos.get(nombre, ()):
            _llamar(f, donde)
        if self._ns is not None:
            f = self._ns.get(donde)
            if f is not None and callable(f):
                _llamar(f, donde)

    def _tecla(self, ev):
        self.tecla = ev.keysym
        self.caracter = ev.char
        self._disparar("tecla")

    # activar y desactivar: el sistema no avisa de ello, así que se deducen del foco de los controles
    def _hay(self, nombre):
        return nombre in self._eventos or (self._ns is not None and ("%s_%s" % (self.nombre, nombre)) in self._ns)

    def _activar(self):
        if not self._activo_f and not self._cerrado:
            self._activo_f = True
            if self._cargado and self._hay("activar"):
                self._disparar("activar")

    def _desactivar(self):
        if self._activo_f:
            self._activo_f = False
            if not self._cerrado and self._hay("desactivar"):
                self._disparar("desactivar")

    def _foco_dentro(self, ev):
        self._activar()

    def _foco_fuera(self, ev):
        self.win.after_idle(self._comprobar_foco)

    def _comprobar_foco(self):
        if self._cerrado or not self._activo_f:
            return
        try:
            f = self.win.focus_get()
            dentro = f is not None and f._top() is self.win
        except Exception:
            dentro = True
        if not dentro:
            self._desactivar()

    # ciclo de vida
    def mostrar(self, modal=False, datos=None):
        """Abre el formulario. Con modal=True no vuelve hasta que se cierre (como ShowDialog) y
        devuelve el valor que se le dio a cerrar(valor); `datos` (un diccionario) queda en
        `self.datos` para que el formulario lo lea al cargarse."""
        if datos is not None:
            self.datos = datos
        self.resultado = None
        self._oculto = False
        self._ordenar_tabulacion()
        self.win.deiconify()
        self._arrancar()
        self._activar()
        if modal and not self.principal:
            try:
                self.win.grab_set()
            except Exception:
                pass
            vivo = [True]

            def latido():
                # despierta el bucle de eventos cada 50 ms para ver si ya se cerró: si se cierra
                # desde un temporizador o desde otro código, ningún evento de ventana lo despertaría
                if vivo[0]:
                    tk._raiz_padrao().after(50, latido)
            latido()
            try:
                tk._laco(lambda: not self._oculto and not self._cerrado)
            finally:
                vivo[0] = False
                try:
                    self.win.grab_release()
                except Exception:
                    pass
            return self.resultado
        return None

    def ocultar(self):
        self._oculto = True
        self._desactivar()
        try:
            self.win.withdraw()
        except Exception:
            pass

    def _arrancar(self):
        if not self._cargado:
            self._cargado = True
            self._disparar("cargar")
            for c in list(self.controles.values()):
                if isinstance(c, Temporizador):
                    c._reprogramar()

    def _ordenar_tabulacion(self):
        """El orden del Tab: por `orden_tab` (de menor a mayor; los que valen 0 conservan el orden
        en que se crearon). Se reordena la lista de hijos de cada contenedor."""
        orden = {}
        for c in self.controles.values():
            if c.w is not None:
                orden[id(c.w)] = int(c._p.get("orden_tab") or 0)
        if not any(orden.values()):
            return
        conts = [self.win] + [c.w for c in self.controles.values() if c.CONTENEDOR and c.w is not None]
        for cont in conts:
            hijos = getattr(cont, "_filhos", None)
            if hijos:
                # la clave lleva la posición: la ordenación de MicroPython no es estable
                par = sorted([(orden.get(id(w), 0), i, w) for i, w in enumerate(hijos)], key=lambda t: (t[0], t[1]))
                hijos[:] = [t[2] for t in par]

    def _cerrar(self):
        if self._cerrado:
            return
        donde = "%s_cerrar" % self.nombre
        self.cancelar = False
        self._disparar("cerrando")
        if self.cancelar:
            self.cancelar = False
            return
        for f in self._eventos.get("cerrar", ()):
            try:
                if f() is False:
                    return
            except Exception as e:
                _informar(e, donde)
        if self._ns is not None:
            f = self._ns.get(donde)
            if f is not None and callable(f):
                try:
                    if f() is False:
                        return
                except Exception as e:
                    _informar(e, donde)
        self.cerrar()

    def cerrar(self, resultado=None):
        """Cierra el formulario (sin preguntar). Si es el principal, termina la aplicación.
        Un formulario secundario solo se esconde (se puede volver a abrir con mostrar()) y, si
        era modal, `resultado` es lo que devuelve mostrar(modal=True)."""
        if resultado is not None:
            self.resultado = resultado
        if not self.principal and not self._cerrado and not _saliendo[0]:
            self._cargado = False
            self.ocultar()
            self._disparar("cerrado")
            return
        self._destruir()

    def _destruir(self):
        if self._cerrado:
            return
        self._cerrado = True
        self._oculto = True
        self._disparar("cerrado")
        for c in list(self.controles.values()):
            if isinstance(c, Temporizador):
                c.destruir()
        try:
            from vps import _notificar
            _notificar._activos.pop(id(self), None)
        except Exception:
            pass
        try:
            self.win.destroy()
        except Exception:
            pass
        if self in _formularios:
            _formularios.remove(self)
        if self.principal:
            _saliendo[0] = True
            try:
                for f in list(_formularios):
                    f._destruir()
            finally:
                _saliendo[0] = False

    def enfocar(self):
        self.win.focus_set()


TIPOS = {"Etiqueta": Etiqueta, "Boton": Boton, "CuadroTexto": CuadroTexto, "Casilla": Casilla,
         "Opcion": Opcion, "Lista": Lista, "Combo": Combo, "Imagen": Imagen, "Marco": Marco,
         "Deslizador": Deslizador, "BarraProgreso": BarraProgreso, "Temporizador": Temporizador,
         "Menu": Menu, "Pestanas": Pestanas, "Tabla": Tabla, "Numerico": Numerico, "Lienzo": Lienzo,
         "Enlace": Enlace, "BarraEstado": BarraEstado, "SelectorColor": SelectorColor, "Reloj": Reloj,
         "Archivo": Archivo}
_formularios = []
_saliendo = [False]     # la aplicación termina: todos los formularios se destruyen de verdad


# ------------------------------------------------------------ aplicación

def ejecutar(ns=None):
    """Muestra el primer formulario, conecta los eventos por nombre (en `ns`, normalmente
    globals()) y no vuelve hasta que se cierra la aplicación."""
    if ns is None:
        ns = sys.modules["__main__"].__dict__
    if not _formularios:
        raise RuntimeError("ejecutar(): no hay ningún Formulario")
    for f in list(_formularios):
        f._ns = ns
    _formularios[0].mostrar()
    tk.mainloop()


def salir():
    """Cierra todos los formularios y termina la aplicación."""
    _saliendo[0] = True
    try:
        for f in list(_formularios):
            f._destruir()
    finally:
        _saliendo[0] = False


def depurar(variables=None):
    """Para el programa aquí y enseña las variables en la ventana del depurador (por defecto
    las globales de la aplicación; `depurar(locals())` enseña las de la función). Se sigue con
    Continuar."""
    from vps import _depurador
    _depurador.parar_aqui(variables)


def pausa(ms):
    """Espera ms milisegundos sin congelar la ventana."""
    import sagui
    fin = sagui.ticks_ms() + int(ms)
    while sagui.ticks_ms() < fin:
        tk._raiz_padrao().update()
        sagui.dormir(10)


# ------------------------------------------------------------ utilidades

def msgbox(texto, titulo="", botones="aceptar", icono="info"):
    """Mensaje modal. botones: aceptar, aceptar_cancelar, si_no, si_no_cancelar,
    reintentar_cancelar. icono: info, aviso, error, pregunta. Devuelve el botón pulsado
    como texto: «aceptar», «cancelar», «si», «no» o «reintentar»."""
    t = str(texto)
    if botones == "aceptar":
        f = {"aviso": messagebox.showwarning, "error": messagebox.showerror}.get(icono, messagebox.showinfo)
        f(titulo, t)
        return "aceptar"
    if botones == "aceptar_cancelar":
        return "aceptar" if messagebox.askokcancel(titulo, t) else "cancelar"
    if botones == "si_no":
        return "si" if messagebox.askyesno(titulo, t) else "no"
    if botones == "si_no_cancelar":
        r = messagebox.askyesnocancel(titulo, t)
        return "cancelar" if r is None else ("si" if r else "no")
    if botones == "reintentar_cancelar":
        return "reintentar" if messagebox.askretrycancel(titulo, t) else "cancelar"
    raise ValueError("botones desconocidos: " + str(botones))


def inputbox(texto, titulo="", defecto=""):
    """Pide un texto. Devuelve None si se cancela."""
    return simpledialog.askstring(titulo, texto, initialvalue=defecto)


def _filtros(tipos):
    """[("Imágenes", "*.bmp *.png"), ("Todo", "*")] -> "Imágenes (*.bmp;*.png)|*.bmp;*.png|Todo|*"."""
    if not tipos:
        return ""
    r = []
    for desc, pat in tipos:
        pats = ";".join(pat.split()) if isinstance(pat, str) else ";".join(pat)
        r.append("%s%s|%s" % (desc, "" if pats == "*" else " (%s)" % pats, pats))
    return "|".join(r)


def _dialogo(tipo, titulo, filtros, inicial, tk_fun, **kw):
    try:
        import sagui
        r = sagui.dialogo(tipo, titulo, filtros, inicial or "")
        return r if r is not None else ""
    except (ImportError, AttributeError):
        return tk_fun(title=titulo, **kw) or ""


def dialogo_abrir(titulo="Abrir", carpeta=None, tipos=None):
    """Diálogo del sistema (D272) para elegir un archivo que abrir. tipos:
    [("Imágenes", "*.bmp *.png"), ("Todo", "*")]. Devuelve la ruta o '' si se cancela."""
    return _dialogo(0, titulo, _filtros(tipos), carpeta, filedialog.askopenfilename,
                    initialdir=carpeta, filetypes=tipos or [("Todo", "*")])


def dialogo_guardar(titulo="Guardar como", carpeta=None, tipos=None, nombre=None):
    """Diálogo del sistema (D272) para elegir dónde guardar (pide confirmación si ya existe).
    Devuelve la ruta o '' si se cancela."""
    ini = (carpeta or "/") if not nombre else ((carpeta or "").rstrip("/") + "/" + nombre)
    return _dialogo(1, titulo, _filtros(tipos), ini, filedialog.asksaveasfilename,
                    initialdir=carpeta, initialfile=nombre, filetypes=tipos or [("Todo", "*")])


def dialogo_carpeta(titulo="Elegir carpeta", carpeta=None):
    """Diálogo del sistema para elegir una carpeta. Devuelve la ruta o '' si se cancela."""
    return _dialogo(2, titulo, "", carpeta, filedialog.askdirectory, initialdir=carpeta)


# ------------------------------------------------------------ bases de datos (mejora 127, D304)
from vps import _datos                                      # noqa: E402
from vps._datos import ConectorBD, RejillaDatos             # noqa: E402,F401
TIPOS.update(_datos.TIPOS)
__all__ += ["ConectorBD", "RejillaDatos"]
# selectores de archivos y carpetas (v8.4.3, D303)
from vps import _selectores                                 # noqa: E402
from vps._selectores import SelectorDeArchivos, SelectorDeCarpetas, CampoArchivo, CampoCarpeta   # noqa: E402,F401
TIPOS.update(_selectores.TIPOS)
__all__ += ["SelectorDeArchivos", "SelectorDeCarpetas", "CampoArchivo", "CampoCarpeta"]
from vps import _fechas                                     # noqa: E402
from vps._fechas import Calendario, SelectorFecha           # noqa: E402,F401
from vps._selectores import SelectorDeColor, SelectorDeFuente, ComboFuentes   # noqa: E402,F401
TIPOS.update(_fechas.TIPOS)
TIPOS.update({"SelectorDeColor": SelectorDeColor, "SelectorDeFuente": SelectorDeFuente, "ComboFuentes": ComboFuentes})
__all__ += ["SelectorDeColor", "SelectorDeFuente", "ComboFuentes", "Calendario", "SelectorFecha"]
# notificaciones tipo toast y vista en árbol (v8.5.6, D466)
from vps import _notificar, _arbol                          # noqa: E402
from vps._notificar import notificar, notificaciones, cerrar_notificaciones, Notificacion   # noqa: E402,F401
from vps._arbol import Arbol                                # noqa: E402,F401
TIPOS.update(_notificar.TIPOS)
TIPOS.update(_arbol.TIPOS)
# gráficos (v8.5.6, D469)
from vps import _grafico                                    # noqa: E402
from vps._grafico import Grafico                            # noqa: E402,F401
TIPOS.update(_grafico.TIPOS)
