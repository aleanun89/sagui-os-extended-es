#!/apps/bin/python
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
from vps import proyecto as P, plantillas, _modelo as M, _historial as H, _refactor as R, _depurador as D
from vps import _datos, _explorador as EX
from vps._superficie import Superficie, Caja, Propiedades, GRIS
from vps._editor import Editor, nombres_de_codigo, eventos_de, evento_defecto

VERSION = "2.0"
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
    if tipo == "num":
        return float(t) if ("." in t or "e" in t.lower()) else int(t)
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
        self.hist = H.Historial()
        self.confirmar_auto = None    # para las pruebas: True o False en vez de preguntar
        self.sesion = None            # la ejecución depurada en curso
        self.mapa_lineas = None
        self.ejecutando = False
        self.expl_visible = False
        self._cod_visto = None
        self.texto_auto = None        # para las pruebas: el texto que devuelve pedir_texto en vez de preguntar
        self._crear_menus()
        self._crear_ventana()
        self.editor = Editor(self)
        vps.error_hook = self.error_en_ejecucion
        self.raiz.protocol("WM_DELETE_WINDOW", self.salir)
        for k, f in (("<F5>", self.ejecutar), ("<F7>", self.ver_codigo), ("<F12>", self.sonda_f12),
                     ("<F8>", self.depurar_paso), ("<F9>", self.punto_parada),
                     ("<Control-s>", self.guardar), ("<Control-o>", self.abrir), ("<Control-r>", self.alternar_explorador),
                     ("<Control-z>", self.deshacer), ("<Control-y>", self.rehacer)):
            self.raiz.bind(k, lambda e, f=f: f())
        self.nuevo(plantillas.vacia)
        if abrir:
            try:
                self.cargar(abrir)
            except Exception as e:
                self.aviso("No se pudo abrir el proyecto:\n%s: %s" % (type(e).__name__, e))
        self.raiz.after(300, self.imprimir_sonda)
        self.raiz.after(800, self._tic_explorador)

    # ------------------------------------------------------------ propiedades del proyecto
    @property
    def form(self):
        return self.proy["forms"][self.proy["idx"]]

    def titulo_proyecto(self):
        c = self.proy["carpeta"]
        return c.rstrip("/").rpartition("/")[2] if c else "Proyecto1"

    # ------------------------------------------------------------ deshacer y rehacer (D299)
    def foto(self):
        """Una copia de todo lo editable: formularios, formulario actual, selección y código."""
        return {"forms": H.copiar_forms(self.proy["forms"]), "idx": self.proy["idx"],
                "sel": [c["nombre"] for c in self.sel], "codigo": self.editor.texto()}

    def punto(self):
        """Se llama ANTES de cada cambio: guarda la foto a la que volverá Ctrl+Z."""
        self.hist.punto(self.foto())

    def descartar_si_igual(self):
        """Tras arrastrar: si no se movió nada, quita el paso vacío del historial."""
        u = self.hist.ultimo()
        if u is not None and u["forms"] == self.proy["forms"] and u["codigo"] == self.editor.texto():
            self.hist.descartar_ultimo()

    def _restaurar(self, f):
        self.proy["forms"] = f["forms"]
        self.proy["idx"] = min(f["idx"], len(f["forms"]) - 1)
        if f["codigo"] != self.editor.texto():
            self.editor.poner(f["codigo"], registrar=True)
        self.sel = [c for c in self.form["controles"] if c["nombre"] in f["sel"]]
        self.cerrar_edicion()
        self.cambio(True)
        self.refrescar_props()

    def deshacer(self, ev=None):
        f = self.hist.paso_atras(self.foto())
        if f is None:
            self.estado("No hay nada que deshacer")
            return False
        self._restaurar(f)
        self.estado("Deshacer")
        return True

    def rehacer(self, ev=None):
        f = self.hist.paso_adelante(self.foto())
        if f is None:
            self.estado("No hay nada que rehacer")
            return False
        self._restaurar(f)
        self.estado("Rehacer")
        return True

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
        e.add_command(label="Deshacer", command=self.deshacer, accelerator="Ctrl+Z")
        e.add_command(label="Rehacer", command=self.rehacer, accelerator="Ctrl+Y")
        e.add_separator()
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
        v = tk.Menu(barra, tearoff=0)
        v.add_command(label="Ver código", command=self.ver_codigo, accelerator="F7")
        v.add_command(label="Explorador", command=self.alternar_explorador, accelerator="Ctrl+R")
        v.add_command(label="Orden de tabulación...", command=self.orden_tabulacion)
        v.add_command(label="Formulario siguiente", command=self.formulario_siguiente)
        barra.add_cascade(label="Ver", menu=v)
        p = tk.Menu(barra, tearoff=0)
        p.add_command(label="Ver código", command=self.ver_codigo, accelerator="F7")
        p.add_command(label="Agregar formulario", command=self.agregar_formulario)
        p.add_command(label="Quitar formulario actual", command=self.quitar_formulario)
        p.add_command(label="Formulario siguiente", command=self.formulario_siguiente)
        barra.add_cascade(label="Proyecto", menu=p)
        x = tk.Menu(barra, tearoff=0)
        x.add_command(label="Ejecutar", command=self.ejecutar, accelerator="F5")
        x.add_command(label="Paso a paso", command=self.depurar_paso, accelerator="F8")
        x.add_command(label="Punto de parada", command=self.punto_parada, accelerator="F9")
        x.add_command(label="Quitar los puntos de parada", command=lambda: self.editor.quitar_puntos())
        x.add_separator()
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
        # el explorador del proyecto (D315): oculto hasta Ver > Explorador o Ctrl+R
        self.expl = tk.Frame(der, height=232)
        self.expl.pack_propagate(False)
        busca = tk.Frame(self.expl)
        busca.pack(side="top", fill="x")
        tk.Label(busca, text="Buscar:", anchor="w").pack(side="left", padx=2)
        self.filtro_var = tk.StringVar()
        self.filtro_ent = tk.Entry(busca, textvariable=self.filtro_var, width=16)
        self.filtro_ent.pack(side="left", fill="x", expand=True, padx=2)
        self.filtro_var.trace_add("write", lambda *a: self._filtro_cambiado())
        self.arbol = EX.Arbol(self.expl, self)
        self.arbol.pack(side="top", fill="both", expand=True, padx=2, pady=2)
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
            self.refrescar_explorador()
        elif self.sel:
            self.refrescar_props()

    def crear_control(self, tipo, x, y, ancho=None, alto=None, padre=None):
        """Coloca un control nuevo en el formulario actual (con su paso de deshacer)."""
        self.punto()
        c = M.crear(self.proy["forms"], self.form, tipo, x, y, ancho, alto, padre)
        self.herramienta = "Puntero"
        self.seleccionar([c])
        self.cambio(True)
        return c

    def seleccionar(self, lista):
        self.sel = list(lista)
        self.cerrar_edicion()
        self.refrescar_props()
        self.superficie._redesenhar()
        self.caja._redesenhar()
        self.refrescar_explorador()

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
        en_pestanas = o is not self.form and o["padre"] and \
            M.por_nombre(self.form, o["padre"])["tipo"] == "Pestanas"
        for n, t, d in M.props_de(tipo):
            if n == "pagina" and not en_pestanas:
                continue        # solo tiene sentido para los controles que están dentro de unas Pestanas
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
        if _datos.editar_propiedad(self, nombre, o):      # mejora 127: origen, tabla, fuente y campo
            return
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
            nuevo = self.editar_lista(nombre, list(actual), o["tipo"] == "Menu", o["tipo"] == "Arbol")
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

    def renombrar(self, o, valor):
        """Renombra un control (o el formulario) y actualiza su código: los manejadores
        (Boton1_click -> Aceptar_click) y las referencias (Boton1.texto -> Aceptar.texto), con un
        análisis de tokens. Antes de aplicar, enseña la lista de cambios. True si se hizo."""
        viejo = o["nombre"]
        if valor == viejo:
            return False
        if o is self.form:
            motivo = M.renombrar_valido(self.proy["forms"], o, valor, es_form=True)
            eventos = list(vps.Formulario.EVENTOS)
        else:
            motivo = M.renombrar_valido(self.proy["forms"], o, valor)
            eventos = list(vps.TIPOS[o["tipo"]].EVENTOS)
        if motivo:
            self.estado("Nombre no válido: " + motivo, True)
            return False
        codigo = self.editor.texto()
        nuevo, cambios, conflictos = R.renombrar(codigo, viejo, valor, eventos)
        if conflictos:
            self.estado("El código ya tiene %s: elige otro nombre" % ", ".join(conflictos), True)
            return False
        if cambios and not self.confirmar_cambios(viejo, valor, cambios):
            self.estado("Renombrado cancelado")
            return False
        self.punto()
        if o is not self.form:
            for h in self.form["controles"]:
                if h["padre"] == viejo:
                    h["padre"] = valor
        o["nombre"] = valor
        if cambios:
            self.editor.poner(nuevo, registrar=True)
        self.cambio(True)
        print("vps: renombrado %s -> %s (%d líneas)" % (viejo, valor, len(cambios)))
        return True

    def confirmar_cambios(self, viejo, nuevo, cambios):
        """La lista de cambios que hará el renombrado, para aceptarla o cancelarla."""
        if self.confirmar_auto is not None:
            self.ultima_lista = cambios
            return self.confirmar_auto
        d = tk.Toplevel(self.raiz, _dialogo=True)
        d.title("Renombrar %s" % viejo)
        d.transient(self.raiz)
        d.resizable(False, False)
        tk.Label(d, text="«%s» pasa a llamarse «%s». Se cambian %d líneas del código:" % (viejo, nuevo, len(cambios)),
                 anchor="w").pack(fill="x", padx=8, pady=4)
        t = tk.Text(d, width=72, height=14)
        t.pack(padx=8)
        for n, antes, despues in cambios:
            t.insert("end", "%3d: %s\n   -> %s\n" % (n, antes.strip()[:64], despues.strip()[:64]))
        res = []

        def ok():
            res.append(True)
            d.destroy()
        fila = tk.Frame(d, pady=6)
        fila.pack()
        b = tk.Button(fila, text="Aceptar", width=10, command=ok)
        b.pack(side="left", padx=4)
        tk.Button(fila, text="Cancelar", width=10, command=d.destroy).pack(side="left", padx=4)
        d.bind("<Return>", lambda e: ok())
        d.bind("<Escape>", lambda e: d.destroy())
        d.focus_set()
        d.grab_set()
        print("vps: lista de cambios|%d" % len(cambios))
        d.wait_window(d)
        return bool(res)

    def aplicar(self, nombre, valor):
        objs = self.sel if self.sel else [self.form]
        if nombre == "nombre":
            if not self.renombrar(objs[0], valor):
                return
        elif nombre == "anclaje" and not self._anclaje_valido(valor):
            self.estado("Anclaje no válido: usa izq, der, arr, abj separados por |", True)
            return
        else:
            self.punto()
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

    @staticmethod
    def _anclaje_valido(v):
        return all(p in ("izq", "der", "arr", "abj") for p in str(v).replace(",", "|").replace(" ", "").split("|") if p) \
            and str(v).strip() != ""

    def cerrar_edicion(self):
        self.prop_actual = None
        self.lbl_prop.configure(text="")
        self.entrada.delete(0, "end")

    def editar_lista(self, titulo, lineas, menu=False, arbol=False):
        """Un cuadro con una línea por elemento. Devuelve la lista o None si se cancela."""
        d = tk.Toplevel(self.raiz, _dialogo=True)
        d.title("Elementos de " + titulo)
        d.transient(self.raiz)
        d.resizable(False, False)
        ayuda = ("Una línea por elemento. Cada menú sin sangría y, debajo, sus opciones con sangría\n"
                 "como «  Abrir|MnuAbrir» (el nombre da el evento MnuAbrir_click; «  -» es una línea)."
                 if menu else ("Un nodo por línea, con dos espacios de sangría por nivel;\n«Texto|clave» pone la clave (sin ella, la clave es el texto)."
                               if arbol else "Una línea por elemento."))
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
        self.punto()
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
        self.punto()
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
        self.punto()
        paso = 1 if fino else M.GRID
        for c in self.sel:
            c["props"]["x"] = max(0, int(M.val(c, "x")) + dx * paso)
            c["props"]["y"] = max(0, int(M.val(c, "y")) + dy * paso)
        self.cambio(True)

    def alinear(self, modo):
        if len(self.sel) < 2:
            self.estado("Selecciona dos o más controles (Mayús+clic) para alinear")
            return
        self.punto()
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
        self.punto()
        n = len(self.proy["forms"]) + 1
        nombres = M.todos_los_nombres(self.proy["forms"])
        while "Formulario%d" % n in nombres:
            n += 1
        self.proy["forms"].append({"nombre": "Formulario%d" % n, "props": {"texto": "Formulario%d" % n}, "controles": []})
        self.proy["idx"] = len(self.proy["forms"]) - 1
        self.seleccionar([])
        self.editor.actualizar_combos()
        self.marcar_modificado()
        self.refrescar_explorador()

    def quitar_formulario(self):
        if len(self.proy["forms"]) < 2:
            self.estado("Tiene que quedar al menos un formulario", True)
            return
        if self.proy["idx"] == 0:
            self.estado("El formulario principal no se puede quitar", True)
            return
        self.punto()
        del self.proy["forms"][self.proy["idx"]]
        self.proy["idx"] = 0
        self.seleccionar([])
        self.editor.actualizar_combos()
        self.marcar_modificado()
        self.refrescar_explorador()

    def formulario_siguiente(self):
        self.proy["idx"] = (self.proy["idx"] + 1) % len(self.proy["forms"])
        self.seleccionar([])
        self.editor.actualizar_combos()

    # ------------------------------------------------------------ explorador del proyecto (D315)
    def alternar_explorador(self, ev=None):
        self.mostrar_explorador(not self.expl_visible)

    def mostrar_explorador(self, visible=True):
        """Muestra u oculta el explorador (encima de las propiedades)."""
        if visible == self.expl_visible:
            return
        self.expl_visible = visible
        if visible:
            self.props.pack_forget()
            self.expl.pack(side="top", fill="x", padx=2)
            self.props.pack(side="top", fill="both", expand=True, padx=2)
            self.refrescar_explorador()
        else:
            self.expl.pack_forget()
        self.estado("Explorador " + ("visible" if visible else "oculto"))
        print("vps: explorador %s" % ("visible" if visible else "oculto"))

    def _id_sel(self):
        return "c:" + self.sel[0]["nombre"] if self.sel else "f:" + self.form["nombre"]

    def _abrir_hasta(self, c):
        """Quita de «cerrados» los nodos que esconden al control `c` del formulario actual."""
        ab = self.arbol.cerrados
        for i in ("p", "fs", "f:" + self.form["nombre"]):
            ab.discard(i)
        hijo, pad = c, c["padre"]
        while pad:
            p = M.por_nombre(self.form, pad)
            if p is None:
                break
            ab.discard("c:" + p["nombre"])
            if p["tipo"] == "Pestanas":
                ab.discard("pg:%s/%d" % (p["nombre"], int(M.val(hijo, "pagina") or 0)))
            hijo, pad = p, p["padre"]

    def refrescar_explorador(self):
        """Vuelve a construir el árbol (solo si se ve) con el estado actual del proyecto."""
        if not self.expl_visible or not hasattr(self, "editor"):
            return
        if self.sel:
            self._abrir_hasta(self.sel[0])
        codigo = self.editor.texto()
        self._cod_visto = codigo
        self.arbol.reconstruir(self.proy["forms"], self.titulo_proyecto(), codigo, self._id_sel())
        self.arbol.ver(self._id_sel())

    def _tic_explorador(self):
        """Cada poco, mira si cambió el código para actualizar la lista de manejadores."""
        try:
            if self.expl_visible and self.editor.texto() != self._cod_visto:
                self.refrescar_explorador()
            self.raiz.after(800, self._tic_explorador)
        except Exception:
            pass

    def _filtro_cambiado(self):
        self.arbol.filtro = self.filtro_var.get()
        self.arbol.arriba = 0
        self.refrescar_explorador()

    def ir_a_form(self, i):
        """Cambia al formulario número i del proyecto (con su selección vacía)."""
        if 0 <= i < len(self.proy["forms"]):
            if i != self.proy["idx"]:
                self.proy["idx"] = i
                self.editor.actualizar_combos()
            self.seleccionar([])

    def ir_a_control(self, fi, nombre):
        """Selecciona un control en el diseñador: cambia de formulario y, si está dentro de unas
        Pestanas, de página; sus propiedades pasan a la ventana de propiedades."""
        if fi != self.proy["idx"]:
            self.proy["idx"] = fi
            self.editor.actualizar_combos()
        c = M.por_nombre(self.form, nombre)
        if c is None:
            return None
        hijo, pad, cambiada = c, c["padre"], False
        while pad:
            p = M.por_nombre(self.form, pad)
            if p is None:
                break
            if p["tipo"] == "Pestanas":
                pg = int(M.val(hijo, "pagina") or 0)
                if int(M.val(p, "seleccion") or 0) != pg:
                    if not cambiada:
                        self.punto()
                        cambiada = True
                    p["props"]["seleccion"] = pg
            hijo, pad = p, p["padre"]
        if cambiada:
            self.cambio(True)
        self.seleccionar([c])
        return c

    def explorador_nodo(self, nodo, doble):
        """Lo que pasa al pulsar un nodo del explorador (un clic selecciona; doble clic abre el código)."""
        k = nodo["k"]
        if k == "form":
            self.ir_a_form(nodo["f"])
            if doble:
                self.codigo_de(None)
        elif k == "ctl":
            c = self.ir_a_control(nodo["f"], nodo["c"])
            if doble and c is not None:
                self.codigo_de(c)
        elif k == "pagina":
            c = self.ir_a_control(nodo["f"], nodo["c"])
            if c is not None and int(M.val(c, "seleccion") or 0) != nodo["pag"]:
                self.punto()
                c["props"]["seleccion"] = nodo["pag"]
                self.cambio(True)
        elif k == "codigo":
            if doble:
                self.ver_codigo()
        elif k == "manejador" and doble:
            self.editor.actualizar_combos()
            self.editor.mostrar()
            self.editor.ir_a_linea(nodo["linea"] + 1, 4)
        self.arbol.sel = nodo["id"]
        self.arbol._redesenhar()
        print("vps: explorador %s|%s|%s" % ("doble" if doble else "clic", k, nodo["t"]))

    def acciones_explorador(self, nodo):
        """[(texto, función) | None] del menú contextual de un nodo."""
        k = nodo["k"]
        r = []
        if k in ("form", "ctl"):
            r.append(("Ver código", lambda: self.explorador_nodo(nodo, True)))
            r.append(("Renombrar", lambda: self.renombrar_desde_explorador(nodo)))
            r.append(("Eliminar", lambda: self.borrar_desde_explorador(nodo)))
        if k == "ctl":
            r.append(None)
            r.append(("Traer al frente", lambda: self.orden_z_explorador(nodo, True)))
            r.append(("Enviar atrás", lambda: self.orden_z_explorador(nodo, False)))
        if k == "manejador":
            r.append(("Ir al código", lambda: self.explorador_nodo(nodo, True)))
        if r:
            r.append(None)
        r.append(("Agregar formulario", self.agregar_formulario))
        return r

    def menu_explorador(self, nodo, x, y):
        m = tk.Menu(self.raiz, tearoff=0)
        for a in self.acciones_explorador(nodo):
            if a is None:
                m.add_separator()
            else:
                m.add_command(label=a[0], command=a[1])
        m.tk_popup(x, y)

    def pedir_texto(self, texto, titulo, defecto=""):
        if self.texto_auto is not None:
            return self.texto_auto
        return vps.inputbox(texto, titulo, defecto)

    def renombrar_desde_explorador(self, nodo):
        if nodo is None or nodo["k"] not in ("form", "ctl"):
            return False
        if nodo["k"] == "form":
            self.ir_a_form(nodo["f"])
            o = self.form
        else:
            o = self.ir_a_control(nodo["f"], nodo["c"])
        if o is None:
            return False
        nuevo = self.pedir_texto("Nuevo nombre de «%s»:" % o["nombre"], "Renombrar", o["nombre"])
        if not nuevo:
            return False
        return self.renombrar(o, nuevo.strip())

    def borrar_desde_explorador(self, nodo):
        if nodo is None:
            return
        if nodo["k"] == "ctl":
            if self.ir_a_control(nodo["f"], nodo["c"]) is not None:
                self.borrar_seleccion()
        elif nodo["k"] == "form":
            self.ir_a_form(nodo["f"])
            self.quitar_formulario()

    def orden_z_explorador(self, nodo, frente):
        c = self.ir_a_control(nodo["f"], nodo["c"])
        if c is None:
            return
        self.punto()
        M.mover_z(self.form, c, frente)
        self.cambio(True)

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
        self.hist.vaciar()
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
        self.hist.vaciar()
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

    # ------------------------------------------------------------ ejecutar y depurar
    def ejecutar(self, paso=False):
        """F5: construye y ejecuta la aplicación dentro de este proceso. Con puntos de parada
        (F9) o con paso=True (F8), el código se instrumenta y para donde toca."""
        self.detener()
        self.editor.quitar_error()
        ns = P.espacio_nombres()
        fuente = self.editor.texto()
        self.sesion = None
        self.mapa_lineas = None
        a_ejecutar = fuente
        if paso or self.editor.puntos:
            ses = D.Sesion(fuente, self.editor.puntos, paso, self.raiz, getattr(self, "decisor_depurador", None))
            ses.al_parar = self.editor.marcar_actual
            ses.globales = ns
            self.sesion = ses
            ns[D.NOMBRE] = ses.hook
            a_ejecutar = ses.codigo
            self.mapa_lineas = ses.mapa
        try:
            todos = P.construir(self.proy["forms"], ns)
            exec(compile(a_ejecutar, "codigo.py", "exec"), ns)
            for f in todos:
                f._ns = ns
            todos[0].principal = True
            todos[0].mostrar()
            self.ejecutando = True
            self.estado("Ejecutando...  (cierra la ventana de la aplicación para volver)")
            print("vps: ejecutando" + (" (depurando)" if self.sesion else ""))
        except Exception as e:
            vps.salir()
            self.error(e, "al arrancar")

    def depurar_paso(self):
        """F8: ejecuta parando en la primera sentencia y sigue paso a paso."""
        self.ejecutar(paso=True)

    def punto_parada(self):
        """F9: punto de parada en la línea del cursor del editor de código."""
        self.editor.alternar_punto()
        self.ver_codigo()

    def detener(self):
        try:
            vps.salir()
        except Exception:
            pass
        if self.sesion is not None:
            self.sesion.cerrar()
            self.sesion = None
        self.ejecutando = False
        if hasattr(self, "editor"):
            self.editor.marcar_actual(None)

    # ------------------------------------------------------------ orden de tabulación
    def orden_tab_lista(self):
        """Los nombres de los controles del formulario actual en su orden de tabulación."""
        cs = self.form["controles"]
        par = sorted([(int(M.val(c, "orden_tab") or 0), i, c["nombre"]) for i, c in enumerate(cs)],
                     key=lambda t: (t[0], t[1]))
        return [t[2] for t in par]

    def fijar_orden_tab(self, nombres):
        """Asigna orden_tab = 1, 2, 3... según la lista de nombres (los que falten, al final)."""
        self.punto()
        resto = [c["nombre"] for c in self.form["controles"] if c["nombre"] not in nombres]
        for i, n in enumerate(list(nombres) + resto):
            c = M.por_nombre(self.form, n)
            if c is not None:
                c["props"]["orden_tab"] = i + 1
        self.cambio(True)

    def orden_tabulacion(self):
        """Ver > Orden de tabulación: una lista de los controles que se sube y se baja."""
        nombres = self.orden_tab_lista()
        if not nombres:
            self.estado("El formulario no tiene controles")
            return
        d = tk.Toplevel(self.raiz, _dialogo=True)
        d.title("Orden de tabulación")
        d.transient(self.raiz)
        d.resizable(False, False)
        tk.Label(d, text="El orden en que el Tab recorre los controles:", anchor="w").pack(fill="x", padx=8, pady=4)
        lb = tk.Listbox(d, width=34, height=12)
        lb.pack(padx=8)
        tipos = dict((c["nombre"], c["tipo"]) for c in self.form["controles"])

        def llenar(sel=0):
            lb.delete(0, "end")
            for i, n in enumerate(nombres):
                lb.insert("end", "%2d  %s (%s)" % (i + 1, n, tipos[n]))
            lb.selection_set(sel)

        def mover(dd):
            sel = lb.curselection()
            if not sel:
                return
            i = sel[0]
            j = i + dd
            if 0 <= j < len(nombres):
                nombres[i], nombres[j] = nombres[j], nombres[i]
                llenar(j)
        res = []

        def ok():
            res.append(True)
            d.destroy()
        fila = tk.Frame(d, pady=6)
        fila.pack()
        tk.Button(fila, text="Subir", width=8, command=lambda: mover(-1)).pack(side="left", padx=3)
        tk.Button(fila, text="Bajar", width=8, command=lambda: mover(1)).pack(side="left", padx=3)
        tk.Button(fila, text="Aceptar", width=8, command=ok).pack(side="left", padx=3)
        tk.Button(fila, text="Cancelar", width=8, command=d.destroy).pack(side="left", padx=3)
        llenar()
        d.grab_set()
        d.wait_window(d)
        if res:
            self.fijar_orden_tab(nombres)

    def error_en_ejecucion(self, e, donde):
        self.error(e, donde)

    def error(self, e, donde):
        t = traza(e)
        n = linea_del_error(t)
        if n is not None:
            n = D.linea_original(self.mapa_lineas, n)
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
        from vps._superficie import HERRAMIENTAS
        for t in HERRAMIENTAS:
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
            if c["tipo"] == "Pestanas":
                print("vps-sonda: pest|%s|%d|0" % (c["nombre"], int(M.val(c, "seleccion") or 0)))
        if self.expl_visible:
            for n, nodo in self.arbol.filas:
                xy = self.arbol.fila_centro(nodo["id"])
                if xy:
                    print("vps-sonda: nodo|%s|%d|%d" % (nodo["id"], xy[0], xy[1]))
            fe = self.filtro_ent
            print("vps-sonda: filtro|%d|%d" % (fe.winfo_rootx() + 20, fe.winfo_rooty() + 8))
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
