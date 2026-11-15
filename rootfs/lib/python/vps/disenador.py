#!/bin/python
# SPDX-License-Identifier: GPL-2.0-or-later
# vps.disenador - el diseñador de VisualPythonS (mejora 123, D297): un entorno al estilo del
# Visual Basic clásico para hacer aplicaciones sencillas en Python, con el aspecto de Win98.
#
#   python /lib/python/vps/disenador.py [carpeta-del-proyecto] [--sonda]
#
# Caja de herramientas a la izquierda, el formulario con su rejilla en el centro, la ventana de
# propiedades a la derecha y, con doble clic en un control (o F7), el editor de código con los
# combos «control» y «evento». F5 ejecuta la aplicación; «Generar aplicación» la deja en
# Inicio > Mis aplicaciones. Un proyecto es una carpeta con formulario.vpf y codigo.py.
import io
import os
import sys
import tkinter as tk
from tkinter import ttk
import vps
from vps import proyecto as P, plantillas, _modelo as M
from vps._superficie import Superficie, Caja, Propiedades, GRIS
from vps._editor import Editor, nombres_de_codigo, eventos_de, evento_defecto

VERSION = "1.0"
NOMBRES_COLOR = [("Negro", 0x000000), ("Blanco", 0xFFFFFF), ("Gris", 0x808080), ("Plata", 0xC0C0C0),
                 ("Rojo", 0xFF0000), ("Verde", 0x008000), ("Azul", 0x0000FF), ("Amarillo", 0xFFFF00),
                 ("Naranja", 0xFFA500), ("Morado", 0x800080), ("Marrón", 0x804020), ("Rosa", 0xFFC0CB),
                 ("Cian", 0x00FFFF)]
PROPS_LISTA_MULTI = ("elementos",)


def carpeta_apps():
    """Dónde van las aplicaciones generadas: /apps/vps si hay partición de apps; si no, /home/vps."""
    if os.path.isdir("/apps"):
        try:
            os.mkdir("/apps/vps")
        except OSError:
            pass
        if os.path.isdir("/apps/vps"):
            return "/apps/vps"
    try:
        os.mkdir("/home/vps")
    except OSError:
        pass
    return "/home/vps"


def carpeta_proyectos():
    """Dónde se proponen los proyectos: /home si existe y, si no, /tmp."""
    return "/home" if os.path.isdir("/home") else "/tmp"


def mkdirs(ruta):
    partes = ruta.strip("/").split("/")
    cur = ""
    for p in partes:
        cur += "/" + p
        if not os.path.isdir(cur):
            os.mkdir(cur)


def traza(e):
    b = io.StringIO()
    sys.print_exception(e, b)
    return b.getvalue()


def linea_del_error(texto, archivo="codigo.py"):
    """La última línea de codigo.py que aparece en un traceback de MicroPython, o None."""
    marca = 'File "%s", line ' % archivo
    n = None
    pos = 0
    while True:
        i = texto.find(marca, pos)
        if i < 0:
            return n
        j = i + len(marca)
        k = j
        while k < len(texto) and texto[k].isdigit():
            k += 1
        if k > j:
            n = int(texto[j:k])
        pos = k


def texto_de(valor, tipo):
    if tipo == "bool":
        return "Sí" if valor else "No"
    if tipo == "color":
        if valor is None or valor == "":
            return "(por defecto)"
        if isinstance(valor, int):
            return "#%06X" % valor
        return str(valor)
    if tipo == "lista":
        return "; ".join(str(x).strip() for x in (valor or []))
    return "" if valor is None else str(valor)


def analizar(texto, tipo):
    """El texto escrito en la ventana de propiedades -> valor (ValueError si no vale)."""
    t = texto.strip()
    if tipo == "int":
        return int(t)
    if tipo == "bool":
        if t.lower() in ("si", "sí", "true", "verdadero", "1"):
            return True
        if t.lower() in ("no", "false", "falso", "0"):
            return False
        raise ValueError("escribe Sí o No")
    if tipo == "color":
        if not t or t.startswith("("):
            return None
        for n, c in NOMBRES_COLOR:
            if n.lower() == t.lower():
                return c
        if t[0] == "#":
            return int(t[1:], 16)
        if t.lower().startswith("0x"):
            return int(t, 16)
        raise ValueError("color no válido (usa #RRGGBB o un nombre)")
    if tipo == "lista":
        return [x.strip() for x in texto.split(";") if x.strip()]
    return texto


class App:
    def __init__(self, abrir=None, sonda=False):
        self.raiz = tk.Tk()
        self.raiz.title("VisualPythonS")
        self.raiz.geometry("1000x690")
        self.herramienta = "Puntero"
        self.sel = []
        self.mostrar_ocultos = False
        self.portapapeles = []
        self.prop_actual = None     # (nombre, tipo) de la fila que se edita
        self.proy = {"forms": [], "carpeta": None, "modificado": False, "idx": 0}
        self.sonda = sonda
        self._f12 = False
        self._crear_menus()
        self._crear_ventana()
        self.editor = Editor(self)
        vps.error_hook = self.error_en_ejecucion
        self.raiz.protocol("WM_DELETE_WINDOW", self.salir)
        for k, f in (("<F5>", self.ejecutar), ("<F7>", self.ver_codigo), ("<F12>", self.sonda_f12),
                     ("<Control-s>", self.guardar), ("<Control-o>", self.abrir)):
            self.raiz.bind(k, lambda e, f=f: f())
        self.nuevo(plantillas.vacia)
        if abrir:
            try:
                self.cargar(abrir)
            except Exception as e:
                self.aviso("No se pudo abrir el proyecto:\n%s: %s" % (type(e).__name__, e))
        self.raiz.after(300, self.imprimir_sonda)

    # ------------------------------------------------------------ propiedades del proyecto
    @property
    def form(self):
        return self.proy["forms"][self.proy["idx"]]

    def titulo_proyecto(self):
        c = self.proy["carpeta"]
        return c.rstrip("/").rpartition("/")[2] if c else "Proyecto1"

    def marcar_modificado(self):
        self.proy["modificado"] = True
        self._titulo()

    def _titulo(self):
        t = "VisualPythonS - " + self.titulo_proyecto() + (" *" if self.proy["modificado"] else "")
        self.raiz.title(t)
        if hasattr(self, "editor"):
            self.editor.titulo(self.titulo_proyecto())

    def estado(self, texto, error=False):
        self.barra.configure(text=texto, fg=0xC00000 if error else 0x000000)

    def aviso(self, texto, titulo="VisualPythonS", icono="info"):
        vps.msgbox(texto, titulo, "aceptar", icono)

    # ------------------------------------------------------------ interfaz
    def _crear_menus(self):
        r = self.raiz
        barra = tk.Menu(r)
        m = tk.Menu(barra, tearoff=0)
        sub = tk.Menu(m, tearoff=0)
        for nombre, f in plantillas.PLANTILLAS:
            sub.add_command(label=nombre, command=lambda f=f: self.nuevo_confirmado(f))
        m.add_cascade(label="Nuevo", menu=sub)
        m.add_command(label="Abrir proyecto...", command=self.abrir, accelerator="Ctrl+O")
        m.add_command(label="Guardar", command=self.guardar, accelerator="Ctrl+S")
        m.add_command(label="Guardar como...", command=self.guardar_como)
        m.add_separator()
        m.add_command(label="Generar aplicación...", command=self.generar)
        m.add_separator()
        m.add_command(label="Salir", command=self.salir)
        barra.add_cascade(label="Archivo", menu=m)
        e = tk.Menu(barra, tearoff=0)
        e.add_command(label="Cortar", command=self.cortar, accelerator="Ctrl+X")
        e.add_command(label="Copiar", command=self.copiar, accelerator="Ctrl+C")
        e.add_command(label="Pegar", command=self.pegar, accelerator="Ctrl+V")
        e.add_command(label="Eliminar", command=self.borrar_seleccion, accelerator="Supr")
        e.add_command(label="Seleccionar todo", command=self.seleccionar_todo, accelerator="Ctrl+A")
        e.add_separator()
        a = tk.Menu(e, tearoff=0)
        for etiqueta, modo in (("Izquierda", "izq"), ("Derecha", "der"), ("Arriba", "arriba"), ("Abajo", "abajo"),
                               ("Centrar horizontalmente", "ch"), ("Centrar verticalmente", "cv"),
                               ("Igualar ancho", "ancho"), ("Igualar alto", "alto")):
            a.add_command(label=etiqueta, command=lambda modo=modo: self.alinear(modo))
        e.add_cascade(label="Alinear", menu=a)
        barra.add_cascade(label="Edición", menu=e)
        p = tk.Menu(barra, tearoff=0)
        p.add_command(label="Ver código", command=self.ver_codigo, accelerator="F7")
        p.add_command(label="Agregar formulario", command=self.agregar_formulario)
        p.add_command(label="Quitar formulario actual", command=self.quitar_formulario)
        p.add_command(label="Formulario siguiente", command=self.formulario_siguiente)
        barra.add_cascade(label="Proyecto", menu=p)
        x = tk.Menu(barra, tearoff=0)
        x.add_command(label="Ejecutar", command=self.ejecutar, accelerator="F5")
        x.add_command(label="Detener", command=self.detener)
        barra.add_cascade(label="Ejecutar", menu=x)
        h = tk.Menu(barra, tearoff=0)
        h.add_command(label="Acerca de VisualPythonS", command=self.acerca)
        barra.add_cascade(label="Ayuda", menu=h)
        r.config(menu=barra)

    def _crear_ventana(self):
        r = self.raiz
        self.barra = tk.Label(r, text="", anchor="w", relief="sunken", bd=1)
        self.barra.pack(side="bottom", fill="x")
        herr = tk.Frame(r, padx=2, pady=2)
        herr.pack(side="top", fill="x")
        self.botones = {}
        for t, f in (("Nuevo", lambda: self.nuevo_confirmado(plantillas.vacia)), ("Abrir", self.abrir),
                     ("Guardar", self.guardar), ("Ejecutar", self.ejecutar), ("Código", self.ver_codigo),
                     ("Generar", self.generar)):
            b = tk.Button(herr, text=t, command=f, padx=4)
            b.pack(side="left", padx=2)
            self.botones[t] = b
        cuerpo = tk.Frame(r)
        cuerpo.pack(side="top", fill="both", expand=True)
        self.caja = Caja(cuerpo, self)
        self.caja.pack(side="left", fill="y")
        der = tk.Frame(cuerpo, width=232)
        der.pack(side="right", fill="y")
        der.pack_propagate(False)
        self.selector = ttk.Combobox(der, state="readonly", values=[], width=26)
        self.selector.pack(side="top", fill="x", padx=2, pady=2)
        self.selector.bind("<<ComboboxSelected>>", lambda e: self._selector_elegido())
        edit = tk.Frame(der, pady=2)
        edit.pack(side="bottom", fill="x")
        self.lbl_prop = tk.Label(edit, text="", anchor="w")
        self.lbl_prop.pack(side="top", fill="x", padx=2)
        fila = tk.Frame(edit)
        fila.pack(side="top", fill="x")
        self.entrada = tk.Entry(fila, width=18)
        self.entrada.pack(side="left", fill="x", expand=True, padx=2)
        self.entrada.bind("<Return>", lambda e: self.aplicar_entrada())
        self.btn_mas = tk.Button(fila, text="...", padx=2, command=self.mas_prop)
        self.btn_mas.pack(side="left", padx=2)
        self.props = Propiedades(der, self)
        self.props.pack(side="top", fill="both", expand=True, padx=2)
        self.superficie = Superficie(cuerpo, self)
        self.superficie.pack(side="left", fill="both", expand=True)

    # ------------------------------------------------------------ estado de la edición
    def elegir_herramienta(self, t):
        self.herramienta = t
        self.caja._redesenhar()
        self.estado("Herramienta: " + t + (" - haz clic o arrastra en el formulario" if t != "Puntero" else ""))

    def cambio(self, final=True):
        """El modelo cambió: redibuja; con final=True también marca el proyecto como modificado."""
        self.superficie._redesenhar()
        if final:
            self.marcar_modificado()
            self.refrescar_props()
            self.caja._redesenhar()
            self.editor.actualizar_combos()
        elif self.sel:
            self.refrescar_props()

    def seleccionar(self, lista):
        self.sel = list(lista)
        self.cerrar_edicion()
        self.refrescar_props()
        self.superficie._redesenhar()
        self.caja._redesenhar()

    def alternar(self, c):
        if any(c is s for s in self.sel):
            self.sel = [s for s in self.sel if s is not c]
        else:
            self.sel.append(c)
        self.refrescar_props()
        self.superficie._redesenhar()

    def seleccionar_todo(self, ev=None):
        self.seleccionar(list(self.form["controles"]))

    def cancelar(self, ev=None):
        self.herramienta = "Puntero"
        self.seleccionar([])

    # ------------------------------------------------------------ propiedades
    def objeto(self):
        return self.sel[0] if self.sel else self.form

    def refrescar_props(self):
        o = self.objeto()
        tipo = "Formulario" if o is self.form else o["tipo"]
        filas = [("nombre", "str", o["nombre"])]
        for n, t, d in M.props_de(tipo):
            filas.append((n, t, texto_de(M.val(o, n, tipo), t.split(":")[0])))
        self.props.poner(filas)
        nombres = ["%s %s" % (self.form["nombre"], "Formulario")] + ["%s %s" % (c["nombre"], c["tipo"]) for c in self.form["controles"]]
        self.selector.configure(values=nombres)
        self.selector.set("%s %s" % (o["nombre"], tipo))
        f = self.form
        self.estado("%s%s  -  formulario %s (%d de %d)" % (
            o["nombre"], " y %d más" % (len(self.sel) - 1) if len(self.sel) > 1 else "", f["nombre"],
            self.proy["idx"] + 1, len(self.proy["forms"])))

    def _selector_elegido(self):
        nombre = self.selector.get().split(" ")[0]
        if nombre == self.form["nombre"]:
            self.seleccionar([])
        else:
            c = M.por_nombre(self.form, nombre)
            if c is not None:
                self.seleccionar([c])

    def tipo_de(self, nombre):
        o = self.objeto()
        tipo = "Formulario" if o is self.form else o["tipo"]
        for n, t, d in M.props_de(tipo):
            if n == nombre:
                return t
        return "str"

    def fila_elegida(self, fila, xr, yr):
        nombre, tipo_fila, texto = fila
        t = self.tipo_de(nombre) if nombre != "nombre" else "str"
        self.prop_actual = (nombre, t)
        self.lbl_prop.configure(text=nombre + ":")
        self.entrada.delete(0, "end")
        self.entrada.insert(0, texto if not texto.startswith("(") else "")
        o = self.objeto()
        if t == "bool":
            self.aplicar(nombre, not M.val(o, nombre, "Formulario" if o is self.form else o["tipo"]))
        elif t.startswith("enum:"):
            m = tk.Menu(self.raiz, tearoff=0)
            for op in t[5:].split("|"):
                m.add_command(label=op, command=lambda op=op: self.aplicar(nombre, op))
            m.tk_popup(xr, yr)
        elif t == "color":
            self.mas_prop()
        elif t == "lista":
            self.mas_prop()
        else:
            self.entrada.focus_set()

    def mas_prop(self):
        """El botón «...»: elegir un color o editar una lista."""
        if not self.prop_actual:
            return
        nombre, t = self.prop_actual
        o = self.objeto()
        if t == "color":
            m = tk.Menu(self.raiz, tearoff=0)
            m.add_command(label="(por defecto)", command=lambda: self.aplicar(nombre, None))
            for n, c in NOMBRES_COLOR:
                m.add_command(label="%s  #%06X" % (n, c), command=lambda c=c: self.aplicar(nombre, c))
            m.tk_popup(self.raiz.winfo_pointerx(), self.raiz.winfo_pointery())
        elif t == "lista":
            actual = M.val(o, nombre, "Formulario" if o is self.form else o["tipo"]) or []
            nuevo = self.editar_lista(nombre, list(actual), o["tipo"] == "Menu")
            if nuevo is not None:
                self.aplicar(nombre, nuevo)

    def aplicar_entrada(self):
        if not self.prop_actual:
            return
        nombre, t = self.prop_actual
        try:
            valor = analizar(self.entrada.get(), t.split(":")[0])
        except ValueError as e:
            self.estado("Valor no válido: %s" % e, True)
            return
        self.aplicar(nombre, valor)

    def aplicar(self, nombre, valor):
        objs = self.sel if self.sel else [self.form]
        if nombre == "nombre":
            o = objs[0]
            if o is self.form:
                if valor == "" or not (valor[0].isalpha() or valor[0] == "_"):
                    self.estado("El nombre no es válido", True)
                    return
                o["nombre"] = valor
            else:
                motivo = M.renombrar_valido(self.proy["forms"], o, valor)
                if motivo:
                    self.estado("Nombre no válido: " + motivo, True)
                    return
                for h in self.form["controles"]:
                    if h["padre"] == o["nombre"]:
                        h["padre"] = valor
                o["nombre"] = valor
        else:
            for o in objs:
                tipo = "Formulario" if o is self.form else o["tipo"]
                if any(n == nombre for n, t, d in M.props_de(tipo)):
                    if valor is None or valor == M.defectos(tipo).get(nombre) and nombre not in ("x", "y", "ancho", "alto"):
                        o["props"].pop(nombre, None)
                    else:
                        o["props"][nombre] = list(valor) if isinstance(valor, list) else valor
        self.cambio(True)
        self.entrada.delete(0, "end")
        if self.prop_actual:
            o = objs[0]
            tipo = "Formulario" if o is self.form else o["tipo"]
            v = o["nombre"] if nombre == "nombre" else M.val(o, nombre, tipo)
            self.entrada.insert(0, texto_de(v, self.prop_actual[1].split(":")[0]) if not isinstance(v, bool) else "")

    def cerrar_edicion(self):
        self.prop_actual = None
        self.lbl_prop.configure(text="")
        self.entrada.delete(0, "end")

    def editar_lista(self, titulo, lineas, menu=False):
        """Un cuadro con una línea por elemento. Devuelve la lista o None si se cancela."""
        d = tk.Toplevel(self.raiz, _dialogo=True)
        d.title("Elementos de " + titulo)
        d.transient(self.raiz)
        d.resizable(False, False)
        ayuda = ("Una línea por elemento. Cada menú sin sangría y, debajo, sus opciones con sangría\n"
                 "como «  Abrir|MnuAbrir» (el nombre da el evento MnuAbrir_click; «  -» es una línea)."
                 if menu else "Una línea por elemento.")
        tk.Label(d, text=ayuda, justify="left", anchor="w").pack(fill="x", padx=8, pady=4)
        t = tk.Text(d, width=50, height=12)
        t.pack(padx=8)
        t.insert("1.0", "\n".join(lineas))
        res = []

        def ok():
            res.append(t.get("1.0", "end-1c").split("\n"))
            d.destroy()
        fila = tk.Frame(d, pady=6)
        fila.pack()
        tk.Button(fila, text="Aceptar", width=10, command=ok).pack(side="left", padx=4)
        tk.Button(fila, text="Cancelar", width=10, command=d.destroy).pack(side="left", padx=4)
        t.focus_set()
        d.grab_set()
        d.wait_window(d)
        if not res:
            return None
        return [l.rstrip() for l in res[0] if l.strip()]

    # ------------------------------------------------------------ edición de controles
    def borrar_seleccion(self, ev=None):
        if not self.sel:
            return
        for c in list(self.sel):
            M.borrar(self.form, c)
        self.seleccionar([])
        self.cambio(True)

    def copiar(self, ev=None):
        if not self.sel:
            return
        r = []
        for c in self.sel:
            r.append(c)
            for h in M.descendientes(self.form, c):
                if not any(h is x for x in r):
                    r.append(h)
        self.portapapeles = [M.copiar(c) for c in r]
        self.estado("%d control(es) copiados" % len(self.sel))

    def cortar(self, ev=None):
        self.copiar()
        self.borrar_seleccion()

    def pegar(self, ev=None):
        if not self.portapapeles:
            return
        nombres = set(c["nombre"] for c in self.portapapeles)
        mapa = {}
        nuevos = []
        for c in self.portapapeles:
            n = M.copiar(c)
            mapa[c["nombre"]] = M.nombre_nuevo(self.proy["forms"], c["tipo"])
            n["nombre"] = mapa[c["nombre"]]
            if c["padre"] in nombres:
                n["padre"] = mapa[c["padre"]]
            else:
                n["padre"] = None
                n["props"]["x"] = int(M.val(c, "x")) + 16
                n["props"]["y"] = int(M.val(c, "y")) + 16
            if "texto" in n["props"] and n["props"]["texto"] == c["nombre"]:
                n["props"]["texto"] = n["nombre"]
            self.form["controles"].append(n)
            nuevos.append(n)
            # reservar el nombre antes de crear el siguiente
        self.seleccionar([c for c in nuevos if c["padre"] is None or c["padre"] not in mapa.values()])
        self.cambio(True)

    def empujar(self, dx, dy, fino):
        if not self.sel:
            return
        paso = 1 if fino else M.GRID
        for c in self.sel:
            c["props"]["x"] = max(0, int(M.val(c, "x")) + dx * paso)
            c["props"]["y"] = max(0, int(M.val(c, "y")) + dy * paso)
        self.cambio(True)

    def alinear(self, modo):
        if len(self.sel) < 2:
            self.estado("Selecciona dos o más controles (Mayús+clic) para alinear")
            return
        ref = self.sel[0]
        rx, ry, rw, rh = int(M.val(ref, "x")), int(M.val(ref, "y")), int(M.val(ref, "ancho")), int(M.val(ref, "alto"))
        for c in self.sel[1:]:
            w, h = int(M.val(c, "ancho")), int(M.val(c, "alto"))
            p = c["props"]
            if modo == "izq":
                p["x"] = rx
            elif modo == "der":
                p["x"] = rx + rw - w
            elif modo == "arriba":
                p["y"] = ry
            elif modo == "abajo":
                p["y"] = ry + rh - h
            elif modo == "ch":
                p["x"] = rx + (rw - w) // 2
            elif modo == "cv":
                p["y"] = ry + (rh - h) // 2
            elif modo == "ancho":
                p["ancho"] = rw
            elif modo == "alto":
                p["alto"] = rh
        self.cambio(True)

    # ------------------------------------------------------------ formularios
    def agregar_formulario(self):
        n = len(self.proy["forms"]) + 1
        nombres = M.todos_los_nombres(self.proy["forms"])
        while "Formulario%d" % n in nombres:
            n += 1
        self.proy["forms"].append({"nombre": "Formulario%d" % n, "props": {"texto": "Formulario%d" % n}, "controles": []})
        self.proy["idx"] = len(self.proy["forms"]) - 1
        self.seleccionar([])
        self.editor.actualizar_combos()
        self.marcar_modificado()

    def quitar_formulario(self):
        if len(self.proy["forms"]) < 2:
            self.estado("Tiene que quedar al menos un formulario", True)
            return
        if self.proy["idx"] == 0:
            self.estado("El formulario principal no se puede quitar", True)
            return
        del self.proy["forms"][self.proy["idx"]]
        self.proy["idx"] = 0
        self.seleccionar([])
        self.editor.actualizar_combos()
        self.marcar_modificado()

    def formulario_siguiente(self):
        self.proy["idx"] = (self.proy["idx"] + 1) % len(self.proy["forms"])
        self.seleccionar([])
        self.editor.actualizar_combos()

    # ------------------------------------------------------------ código
    def ver_codigo(self):
        self.editor.actualizar_combos()
        self.editor.mostrar()

    def codigo_de(self, c):
        """Doble clic: el evento por defecto del control (o del formulario) en el editor de código."""
        nombre = c["nombre"] if c is not None else self.form["nombre"]
        self.editor.actualizar_combos()
        self.editor.ir_a(nombre, evento_defecto(self.form, nombre))

    # ------------------------------------------------------------ proyectos
    def confirmar_perder(self):
        if not self.proy["modificado"]:
            return True
        r = vps.msgbox("El proyecto tiene cambios sin guardar. ¿Guardarlos?", "VisualPythonS", "si_no_cancelar", "pregunta")
        if r == "cancelar":
            return False
        if r == "si":
            return self.guardar()
        return True

    def nuevo_confirmado(self, fabrica):
        if self.confirmar_perder():
            self.nuevo(fabrica)

    def nuevo(self, fabrica):
        self.detener()
        forms, codigo = fabrica()
        self.proy = {"forms": forms, "carpeta": None, "modificado": False, "idx": 0}
        self.sel = []
        self.editor.poner(codigo)
        self.editor.actualizar_combos()
        self.seleccionar([])
        self._titulo()

    def cargar(self, carpeta):
        carpeta = carpeta.rstrip("/") or "/"
        forms = P.leer_vpf(P.leer_texto(carpeta + "/formulario.vpf"))
        try:
            codigo = P.leer_texto(carpeta + "/codigo.py")
        except OSError:
            codigo = ""
        self.detener()
        self.proy = {"forms": forms, "carpeta": carpeta, "modificado": False, "idx": 0}
        self.editor.poner(codigo)
        self.editor.actualizar_combos()
        self.seleccionar([])
        self._titulo()
        self.estado("Proyecto abierto: " + carpeta)

    def abrir(self):
        if not self.confirmar_perder():
            return
        ruta = vps.dialogo_abrir("Abrir proyecto", carpeta_proyectos(), [("Proyecto VisualPythonS (formulario.vpf)", "*.vpf")])
        if not ruta:
            return
        try:
            self.cargar(ruta.rpartition("/")[0] or "/")
        except Exception as e:
            self.aviso("No se pudo abrir el proyecto:\n%s: %s" % (type(e).__name__, e), icono="error")

    def guardar(self):
        if not self.proy["carpeta"]:
            return self.guardar_como()
        return self._escribir(self.proy["carpeta"])

    def guardar_como(self):
        ruta = vps.dialogo_guardar("Guardar proyecto como (será una carpeta)", carpeta_proyectos(), [("Proyecto VisualPythonS", "*")],
                                   "MiProyecto")
        if not ruta:
            return False
        return self._escribir(ruta.rstrip("/"))

    def _escribir(self, carpeta):
        try:
            mkdirs(carpeta)
            P.escribir_texto(carpeta + "/formulario.vpf", P.escribir_vpf(self.proy["forms"]))
            P.escribir_texto(carpeta + "/codigo.py", self.editor.texto())
        except Exception as e:
            self.aviso("No se pudo guardar:\n%s: %s" % (type(e).__name__, e), icono="error")
            return False
        self.proy["carpeta"] = carpeta
        self.proy["modificado"] = False
        self._titulo()
        self.estado("Guardado en " + carpeta)
        print("vps: guardado %s" % carpeta)
        return True

    def generar(self):
        """Empaqueta el proyecto como aplicación: carpeta con lanzador, en Inicio > Mis aplicaciones."""
        nombre = vps.inputbox("Nombre de la aplicación (letras, cifras, - y _):", "Generar aplicación",
                              self.titulo_proyecto())
        if not nombre:
            return
        nombre = nombre.strip()
        if not nombre or not all(ch.isalpha() and ord(ch) < 128 or ch.isdigit() or ch in "-_" for ch in nombre):
            self.aviso("El nombre solo puede llevar letras sin acento, cifras, - y _.", icono="aviso")
            return
        base = carpeta_apps()
        destino = base + "/" + nombre
        try:
            mkdirs(destino)
            P.escribir_texto(destino + "/formulario.vpf", P.escribir_vpf(self.proy["forms"]))
            P.escribir_texto(destino + "/codigo.py", self.editor.texto())
            P.escribir_texto(destino + "/lanzador.py", LANZADOR)
        except Exception as e:
            self.aviso("No se pudo generar:\n%s: %s" % (type(e).__name__, e), icono="error")
            return
        self.estado("Aplicación generada en " + destino)
        print("vps: generada %s" % destino)
        self.aviso("Aplicación generada en %s.\nAparece en Inicio > Mis aplicaciones." % destino)

    # ------------------------------------------------------------ ejecutar
    def ejecutar(self):
        self.detener()
        self.editor.quitar_error()
        ns = P.espacio_nombres()
        try:
            todos = P.construir(self.proy["forms"], ns)
            fuente = self.editor.texto()
            exec(compile(fuente, "codigo.py", "exec"), ns)
            for f in todos:
                f._ns = ns
            todos[0].principal = True
            todos[0].mostrar()
            self.estado("Ejecutando...  (cierra la ventana de la aplicación para volver)")
            print("vps: ejecutando")
        except Exception as e:
            vps.salir()
            self.error(e, "al arrancar")

    def detener(self):
        try:
            vps.salir()
        except Exception:
            pass

    def error_en_ejecucion(self, e, donde):
        self.error(e, donde)

    def error(self, e, donde):
        t = traza(e)
        n = linea_del_error(t)
        msg = "%s: %s" % (type(e).__name__, e)
        sys.stderr.write("Error en %s:\n%s" % (donde, t))
        print("vps: error|%s|%s|%s" % (donde, n, msg))
        if n is not None:
            self.editor.actualizar_combos()
            self.editor.mostrar()
            self.editor.marcar_error(n, msg)
        self.estado("Error en %s%s: %s" % (donde, ", línea %d" % n if n else "", msg), True)

    # ------------------------------------------------------------ varios
    def acerca(self):
        self.aviso("VisualPythonS %s\nAplicaciones sencillas con formularios y Python,\nal estilo del Visual Basic clásico." % VERSION,
                   "Acerca de")

    def salir(self):
        if not self.confirmar_perder():
            return
        self.detener()
        self.raiz.destroy()

    def sonda_f12(self):
        self._f12 = True
        self.imprimir_sonda()

    def imprimir_sonda(self):
        """Con --sonda (o F12): las posiciones de los componentes, para las pruebas automáticas."""
        if not self.sonda and not getattr(self, "_f12", False):
            return
        s = self.superficie
        ox, oy = s.origen_cliente()
        print("vps-sonda: cliente|%d|%d" % (ox, oy))
        for t in ("Puntero", "Etiqueta", "Boton", "CuadroTexto", "Casilla", "Opcion", "Lista", "Combo", "Imagen",
                  "Marco", "Deslizador", "BarraProgreso", "Temporizador", "Menu"):
            x, y = self.caja.centro(t)
            print("vps-sonda: herr|%s|%d|%d" % (t, x, y))
        for n, b in self.botones.items():
            print("vps-sonda: boton|%s|%d|%d" % (n, b.winfo_rootx() + b.winfo_width() // 2, b.winfo_rooty() + b.winfo_height() // 2))
        for f in self.props.filas:
            xy = self.props.fila_centro(f[0])
            print("vps-sonda: prop|%s|%d|%d" % (f[0], xy[0], xy[1]))
        for c in self.form["controles"]:
            x, y = s.centro_control(c)
            print("vps-sonda: ctl|%s|%d|%d" % (c["nombre"], x, y))
        e = self.entrada
        print("vps-sonda: entrada|%d|%d" % (e.winfo_rootx() + 20, e.winfo_rooty() + 8))
        print("vps-sonda: fin")


LANZADOR = '''# Lanzador generado por VisualPythonS (D297): abre la aplicación de esta carpeta.
import sys
from vps import proyecto

carpeta = sys.argv[0].rpartition("/")[0] or "."
proyecto.lanzar(carpeta)
'''


def principal(argv):
    abrir = None
    sonda = False
    for a in argv[1:]:
        if a == "--sonda":
            sonda = True
        elif not a.startswith("-"):
            abrir = a
    App(abrir, sonda)
    tk.mainloop()


if __name__ == "__main__":
    principal(sys.argv)
