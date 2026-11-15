# SPDX-License-Identifier: GPL-2.0-or-later
# vps._editor - la ventana de código del diseñador (mejora 123, D297): un editor de Python con
# resaltado y, encima, los dos combos «control» y «evento» del Visual Basic.
import tkinter as tk
from tkinter import ttk
import vps
from vps import _modelo as M

NEGRO, AZUL, ROJO, VERDE, TEAL, MORADO = 0x000000, 0x0000A0, 0xA00000, 0x007000, 0x007070, 0x800080

PALABRAS = set(("False None True and as assert break class continue def del elif else except finally "
                "for from global if import in is lambda nonlocal not or pass raise return try while "
                "with yield").split())
INCORPORADOS = set(("print len range int float str list dict set tuple open abs min max sum sorted "
                    "enumerate zip isinstance bool round input type").split()) | set(vps.__all__)


def realzar(linea):
    """Un color por carácter de una línea de Python (comentarios, textos, números, palabras)."""
    n = len(linea)
    cols = [NEGRO] * n
    i = 0
    previa = ""
    while i < n:
        c = linea[i]
        if c == "#":
            for k in range(i, n):
                cols[k] = VERDE
            break
        if c == '"' or c == "'":
            j = i + 1
            if linea[i:i + 3] == c * 3:
                fin = linea.find(c * 3, i + 3)
                j = n if fin < 0 else fin + 3
            else:
                while j < n and linea[j] != c:
                    if linea[j] == "\\":
                        j += 1
                    j += 1
                j = min(j + 1, n)
            for k in range(i, j):
                cols[k] = ROJO
            i = j
            continue
        if c.isdigit():
            j = i
            while j < n and (linea[j].isalpha() or linea[j].isdigit() or linea[j] == "."):
                j += 1
            for k in range(i, j):
                cols[k] = TEAL
            i = j
            continue
        if c.isalpha() or c == "_":
            j = i
            while j < n and (linea[j].isalpha() or linea[j].isdigit() or linea[j] == "_"):
                j += 1
            w = linea[i:j]
            col = NEGRO
            if w in PALABRAS:
                col = AZUL
            elif w in INCORPORADOS and previa not in ("def", "class"):
                col = MORADO
            for k in range(i, j):
                cols[k] = col
            previa = w
            i = j
            continue
        i += 1
    return cols


def eventos_de(form, nombre):
    """Los eventos de un control (o formulario) por su nombre; los de un elemento de menú: click."""
    if nombre == form["nombre"]:
        return list(vps.Formulario.EVENTOS)
    for c in form["controles"]:
        if c["nombre"] == nombre:
            return list(vps.TIPOS[c["tipo"]].EVENTOS)
    return ["click"]


def evento_defecto(form, nombre):
    if nombre == form["nombre"]:
        return vps.Formulario.EVENTO_DEFECTO
    for c in form["controles"]:
        if c["nombre"] == nombre:
            cls = vps.TIPOS[c["tipo"]]
            return getattr(cls, "EVENTO_DEFECTO", cls.EVENTOS[0] if cls.EVENTOS else "click")
    return "click"


def nombres_de_codigo(form):
    """Formulario, controles y elementos de menú: lo que sale en el combo «control»."""
    r = [form["nombre"]]
    for c in form["controles"]:
        r.append(c["nombre"])
        if c["tipo"] == "Menu":
            for titulo, items in vps._parsear_menu(M.val(c, "elementos") or []):
                for it in items:
                    if it is not None and it[1]:
                        r.append(it[1])
    return r


class Editor:
    def __init__(self, dis):
        self.dis = dis
        top = self.top = tk.Toplevel(dis.raiz)
        top.title("Código - " + dis.titulo_proyecto())
        top.geometry("760x540")
        barra = tk.Frame(top, padx=4, pady=4)
        barra.pack(fill="x")
        tk.Label(barra, text="Control:").pack(side="left")
        self.cb_ctl = ttk.Combobox(barra, width=20, state="readonly", values=["(General)"])
        self.cb_ctl.pack(side="left", padx=(4, 16))
        self.cb_ctl.set("(General)")
        tk.Label(barra, text="Evento:").pack(side="left")
        self.cb_ev = ttk.Combobox(barra, width=16, state="readonly", values=[])
        self.cb_ev.pack(side="left", padx=4)
        self.cb_ctl.bind("<<ComboboxSelected>>", lambda e: self._control_elegido())
        self.cb_ev.bind("<<ComboboxSelected>>", lambda e: self._evento_elegido())
        self.estado = tk.Label(top, text="", anchor="w", relief="sunken", bd=1)
        self.estado.pack(side="bottom", fill="x")
        cuerpo = tk.Frame(top)
        cuerpo.pack(fill="both", expand=True)
        self.sb = tk.Scrollbar(cuerpo, orient="vertical")
        self.sb.pack(side="right", fill="y")
        self.t = tk.Text(cuerpo, wrap="none", width=80, height=24, yscrollcommand=self.sb.set)
        self.t.pack(side="left", fill="both", expand=True)
        self.sb.configure(command=self.t.yview)
        self.t._realce = realzar
        self.t.bind("<Return>", self._intro)
        self.t.bind("<Tab>", self._tab)
        self.t.bind("<Key>", lambda e: self.t.after_idle(self._posicion))
        self.t.bind("<Button-1>", lambda e: self.t.after_idle(self._posicion))
        self.t.bind("<F5>", lambda e: dis.ejecutar())
        top.protocol("WM_DELETE_WINDOW", self.ocultar)
        self.linea_error = None
        self.ocultar()

    # --- ventana
    def mostrar(self):
        self.top.deiconify()
        self.t.focus_set()

    def ocultar(self):
        self.top.withdraw()

    def visible(self):
        return not self.top._retirada

    def titulo(self, nombre):
        self.top.title("Código - " + nombre)

    # --- texto
    def texto(self):
        return self.t.get("1.0", "end-1c")

    def poner(self, s):
        self.t.delete("1.0", "end")
        self.t.insert("1.0", s)
        self.t.mark_set("insert", "1.0")
        self.quitar_error()
        self._posicion()

    def _posicion(self):
        l, c = self.t.index("insert").split(".")
        self.estado.configure(text="Línea %s, columna %d" % (l, int(c) + 1))

    def _intro(self, ev):
        l, c = self.t.index("insert").split(".")
        linea = self.t.get("%s.0" % l, "%s.end" % l)[:int(c)]
        sangria = linea[:len(linea) - len(linea.lstrip(" "))]
        if linea.rstrip().endswith(":"):
            sangria += "    "
        self.t.insert("insert", "\n" + sangria)
        self.t.see("insert")
        self._posicion()
        return "break"

    def _tab(self, ev):
        self.t.insert("insert", "    ")
        return "break"

    # --- combos
    def actualizar_combos(self, conservar=True):
        form = self.dis.form
        nombres = nombres_de_codigo(form)
        self.cb_ctl.configure(values=["(General)"] + nombres)
        actual = self.cb_ctl.get()
        if actual not in nombres:
            self.cb_ctl.set("(General)")
            self.cb_ev.configure(values=[])
            self.cb_ev.set("")

    def _control_elegido(self):
        nombre = self.cb_ctl.get()
        if nombre == "(General)":
            self.cb_ev.configure(values=[])
            self.cb_ev.set("")
            return
        form = self.dis.form
        evs = eventos_de(form, nombre)
        self.cb_ev.configure(values=evs)
        self.ir_a(nombre, evento_defecto(form, nombre) if evento_defecto(form, nombre) in evs else (evs[0] if evs else "click"))

    def _evento_elegido(self):
        nombre = self.cb_ctl.get()
        if nombre != "(General)" and self.cb_ev.get():
            self.ir_a(nombre, self.cb_ev.get())

    def lineas(self):
        return self.texto().split("\n")

    def buscar_def(self, funcion):
        prefijo = "def %s(" % funcion
        for i, l in enumerate(self.lineas()):
            if l.startswith(prefijo):
                return i + 1
        return None

    def ir_a(self, nombre, evento, crear=True):
        """Lleva el cursor a `def Nombre_evento():` (la crea al final si no existe)."""
        form = self.dis.form
        self.cb_ctl.set(nombre)
        evs = eventos_de(form, nombre)
        self.cb_ev.configure(values=evs)
        self.cb_ev.set(evento)
        f = "%s_%s" % (nombre, evento)
        n = self.buscar_def(f)
        if n is None and crear:
            t = self.texto()
            if t and not t.endswith("\n"):
                t += "\n"
            if t.strip():
                t += "\n\n"
            t += "def %s():\n    pass\n" % f
            self.t.delete("1.0", "end")
            self.t.insert("1.0", t)
            n = self.buscar_def(f)
            self.dis.marcar_modificado()
        self.mostrar()
        if n is not None:
            self.ir_a_linea(n + 1, 4)
        return n

    def ir_a_linea(self, n, col=0):
        total = len(self.lineas())
        n = max(1, min(n, total))
        self.t.mark_set("insert", "%d.%d" % (n, col))
        self.t.see("insert")
        self.t._redesenhar()
        self._posicion()

    def marcar_error(self, n, mensaje):
        self.quitar_error()
        self.linea_error = n
        self.t._fondo_linea[n] = 0xFFC8C8
        self.estado.configure(text="Error en la línea %d: %s" % (n, mensaje))
        self.ir_a_linea(n)
        self.t._redesenhar()

    def quitar_error(self):
        if self.linea_error is not None:
            self.t._fondo_linea.pop(self.linea_error, None)
            self.linea_error = None
            self.t._redesenhar()
