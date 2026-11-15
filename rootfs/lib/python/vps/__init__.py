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
import tkinter as tk
from tkinter import messagebox, simpledialog, filedialog, ttk

__all__ = ["Formulario", "Etiqueta", "Boton", "CuadroTexto", "Casilla", "Opcion", "Lista",
           "Combo", "Imagen", "Marco", "Deslizador", "BarraDesplazamiento", "Temporizador",
           "BarraProgreso", "Menu", "evento", "ejecutar", "salir", "msgbox", "inputbox",
           "dialogo_abrir", "dialogo_guardar", "dialogo_carpeta", "pausa", "NEGRO", "BLANCO", "ROJO", "VERDE",
           "AZUL", "AMARILLO", "GRIS", "PLATA", "NARANJA", "MORADO", "MARRON", "ROSA", "CIAN"]

NEGRO, BLANCO, ROJO, VERDE = 0x000000, 0xFFFFFF, 0xFF0000, 0x008000
AZUL, AMARILLO, GRIS, PLATA = 0x0000FF, 0xFFFF00, 0x808080, 0xC0C0C0
NARANJA, MORADO, MARRON, ROSA, CIAN = 0xFFA500, 0x800080, 0x804020, 0xFFC0CB, 0x00FFFF

# Las formas de los colores: entero 0xRRGGBB, "#rrggbb" o un nombre en español o inglés.
_NOMBRES_COLOR = {"negro": NEGRO, "blanco": BLANCO, "rojo": ROJO, "verde": VERDE, "azul": AZUL,
                  "amarillo": AMARILLO, "gris": GRIS, "plata": PLATA, "naranja": NARANJA,
                  "morado": MORADO, "marron": MARRON, "rosa": ROSA, "cian": CIAN}


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


@_props
class Control:
    """Base de los controles. PROPS = (nombre, tipo, valor por defecto); los tipos son
    int, str, bool, color, lista y enum:a|b|c (el diseñador los usa para editar)."""
    tipo = "Control"
    PROPS = (("x", "int", 0), ("y", "int", 0), ("ancho", "int", 100), ("alto", "int", 24),
             ("visible", "bool", True), ("activo", "bool", True), ("color", "color", None),
             ("colorfondo", "color", None), ("fuente", "str", ""))
    EVENTOS = ("click",)
    ORDEN = ("texto", "x", "y", "ancho", "alto")   # argumentos sin nombre
    TAM = (100, 24)
    CONTENEDOR = False

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
            if n not in ("x", "y", "ancho", "alto", "visible"):
                self._aplicar(n)

    def _aplicar(self, n):
        if n in ("x", "y", "ancho", "alto", "visible"):
            self._colocar()
        elif n == "activo":
            self._cfg(state="normal" if self._p["activo"] else "disabled")
        elif n == "color":
            self._cfg(foreground=_a_tk(self._p["color"]))
        elif n == "colorfondo":
            self._aplicar_fondo()

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
        if self._p["visible"]:
            self.w.place(x=int(self._p["x"]), y=int(self._p["y"]),
                         width=max(1, int(self._p["ancho"])), height=max(1, int(self._p["alto"])))
        else:
            self.w.place_forget()

    def _crear(self):
        pass

    def _contenedor_tk(self):
        return self.padre._contenedor_tk()

    # --- eventos
    def conectar(self, nombre, f):
        self._eventos.setdefault(nombre, []).append(f)

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
        return "<%s %s>" % (self.tipo, self.nombre)


@_props
class Etiqueta(Control):
    FONDO_HEREDADO = True
    tipo = "Etiqueta"
    PROPS = (("texto", "str", ""), ("alineacion", "enum:izquierda|centro|derecha", "izquierda"))
    TAM = (80, 20)

    def _crear(self):
        self.w = tk.Label(self._contenedor_tk(), text="", anchor="w", justify="left")
        self.w.bind("<Button-1>", lambda e: self._disparar("click"))

    def _aplicar(self, n):
        if n == "texto":
            self._cfg(text=str(self._p["texto"]))
        elif n == "alineacion":
            a = {"izquierda": "w", "centro": "center", "derecha": "e"}.get(self._p["alineacion"], "w")
            self._cfg(anchor=a, justify={"w": "left", "center": "center", "e": "right"}[a])
        else:
            Control._aplicar(self, n)


@_props
class Boton(Control):
    tipo = "Boton"
    PROPS = (("texto", "str", ""), ("defecto", "bool", False))
    EVENTOS = ("click",)
    TAM = (88, 26)

    def _crear(self):
        self.w = tk.Button(self._contenedor_tk(), text="", command=lambda: self._disparar("click"))

    def _aplicar(self, n):
        if n == "texto":
            self._cfg(text=str(self._p["texto"]))
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
             ("solo_lectura", "bool", False))
    EVENTOS = ("cambio", "tecla", "click")
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

    def _tecla(self, ev):
        self.tecla = ev.keysym
        self.caracter = ev.char
        self._disparar("tecla")

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
    PROPS = (("texto", "str", ""), ("valor", "bool", False))
    EVENTOS = ("click",)
    TAM = (120, 20)

    def _crear(self):
        self._var = tk.IntVar(value=0)
        self.w = tk.Checkbutton(self._contenedor_tk(), text="", variable=self._var,
                                command=self._pulsado, anchor="w")

    def _pulsado(self):
        self._p["valor"] = bool(self._var.get())
        self._disparar("click")

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
    EVENTOS = ("click",)
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
    EVENTOS = ("seleccion", "doble_click", "click")
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
    PROPS = (("elementos", "lista", []), ("texto", "str", ""), ("editable", "bool", False))
    EVENTOS = ("seleccion", "cambio")
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
    EVENTOS = ("click",)
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
    EVENTOS = ("click",)
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
    EVENTOS = ("cambio",)
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
    EVENTOS = ()
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


# ------------------------------------------------------------ formulario

@_props
class Formulario:
    tipo = "Formulario"
    PROPS = (("texto", "str", "Formulario"), ("ancho", "int", 320), ("alto", "int", 200),
             ("color", "color", 0xC0C0C0), ("redimensionable", "bool", False))
    EVENTOS = ("cargar", "cerrar", "tecla", "click", "redimensionar")
    EVENTO_DEFECTO = "cargar"
    ORDEN = ("texto", "ancho", "alto")

    def __init__(self, *args, **kw):
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
        self.controles = {}
        self._contadores = {}
        self._eventos = {}
        self._ns = None
        self._cerrado = False
        self._cargado = False
        self._grupos = {}
        self.tecla = ""
        self.caracter = ""
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
        for n in self._p:
            self._aplicar(n)
        _formularios.append(self)

    # propiedades (texto, ancho, alto, color...)
    def _leer(self, n):
        return self._p.get(n)

    def _escribir(self, n, v):
        self._p[n] = v
        self._aplicar(n)

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

    # ciclo de vida
    def mostrar(self):
        self.win.deiconify()
        self._arrancar()

    def ocultar(self):
        self.win.withdraw()

    def _arrancar(self):
        if not self._cargado:
            self._cargado = True
            self._disparar("cargar")
            for c in list(self.controles.values()):
                if isinstance(c, Temporizador):
                    c._reprogramar()

    def _cerrar(self):
        if self._cerrado:
            return
        donde = "%s_cerrar" % self.nombre
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

    def cerrar(self):
        """Cierra el formulario (sin preguntar). Si es el principal, termina la aplicación."""
        if self._cerrado:
            return
        self._cerrado = True
        for c in list(self.controles.values()):
            if isinstance(c, Temporizador):
                c.destruir()
        try:
            self.win.destroy()
        except Exception:
            pass
        if self in _formularios:
            _formularios.remove(self)
        if self.principal:
            for f in list(_formularios):
                f.cerrar()

    def enfocar(self):
        self.win.focus_set()


TIPOS = {"Etiqueta": Etiqueta, "Boton": Boton, "CuadroTexto": CuadroTexto, "Casilla": Casilla,
         "Opcion": Opcion, "Lista": Lista, "Combo": Combo, "Imagen": Imagen, "Marco": Marco,
         "Deslizador": Deslizador, "BarraProgreso": BarraProgreso, "Temporizador": Temporizador,
         "Menu": Menu}
_formularios = []


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
    for f in list(_formularios):
        f.cerrar()


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
