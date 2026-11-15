# SPDX-License-Identifier: GPL-2.0-or-later
# vps._selectores - selectores de archivos y carpetas de VisualPythonS (v8.4.3, D303): como el
# CommonDialog de Visual Basic. `SelectorDeArchivos` y `SelectorDeCarpetas` no tienen ventana (como el
# Temporizador) y abren por debajo los diálogos comunes de D272 (los mismos de vps.dialogo_abrir,
# vps.dialogo_guardar y vps.dialogo_carpeta). `CampoArchivo` y `CampoCarpeta` son su variante visible:
# un cuadro de texto con el botón «...» que abre el selector y deja la ruta en el cuadro.
#
#     Selector = SelectorDeArchivos(f, filtro="Texto (*.txt)|*.txt|Todos|*", modo="abrir")
#     ruta = Selector.abrir()          # la ruta o None si se cancela
#     def Selector_seleccionado(): print(Selector.archivo)
import vps
from vps import Control, _props, _ev


def _tipos(filtro):
    """«Texto (*.txt)|*.txt|Todos|*» -> [("Texto (*.txt)", "*.txt"), ("Todos", "*")] (para el respaldo de tkinter)."""
    p = [x for x in str(filtro or "").split("|")]
    r = []
    for i in range(0, len(p) - 1, 2):
        nombre = p[i].lstrip(">")
        r.append((nombre, " ".join(p[i + 1].split(";"))))
    return r


def _carpeta_de(ruta):
    i = str(ruta).rfind("/")
    return ruta[:i] if i > 0 else ("/" if i == 0 else "")


def _con_extension(ruta, ext):
    """Añade la extensión por defecto si el nombre no tiene punto."""
    ext = str(ext or "").lstrip(".")
    nombre = ruta[ruta.rfind("/") + 1:]
    if ext and "." not in nombre:
        return ruta + "." + ext
    return ruta


class _Sin(Control):
    """Base de los componentes sin ventana."""
    TAM = (32, 32)

    def _colocar(self):
        pass

    def _escribir(self, n, v):
        self._p[n] = v

    def _aplicar(self, n):
        pass


@_props
class SelectorDeArchivos(_Sin):
    """Sin ventana: abre el diálogo de abrir o de guardar del sistema. `abrir()`, `guardar()` y `mostrar()`
    (según `modo`) devuelven la ruta o None si se cancela; la ruta queda también en `archivo`. Dispara
    «seleccionado» o «cancelado». `filtro` es «Texto (*.txt)|*.txt|Todos (*.*)|*»; `carpeta` y `archivo`
    son lo que se propone al abrir el diálogo."""
    tipo = "SelectorDeArchivos"
    PROPS = (("titulo", "str", ""), ("filtro", "str", ""), ("carpeta", "str", ""), ("archivo", "str", ""),
             ("modo", "enum:abrir|guardar", "abrir"), ("confirmar_sobrescribir", "bool", True),
             ("extension_por_defecto", "str", ""))
    EVENTOS = ("seleccionado", "cancelado")
    EVENTO_DEFECTO = "seleccionado"
    ORDEN = ("titulo", "filtro")

    def _elegir(self, modo):
        p = self._p
        carpeta = p["carpeta"] or _carpeta_de(p["archivo"])
        titulo = p["titulo"] or ("Guardar como" if modo == "guardar" else "Abrir")
        tipos = _tipos(p["filtro"])
        if modo == "guardar":
            nombre = p["archivo"][p["archivo"].rfind("/") + 1:]
            ini = (carpeta or "/") if not nombre else (carpeta.rstrip("/") + "/" + nombre)
            r = vps._dialogo(1, titulo, p["filtro"], ini, vps.filedialog.asksaveasfilename,
                             initialdir=carpeta, initialfile=nombre or None, filetypes=tipos or [("Todo", "*")])
            if r:
                r = _con_extension(r, p["extension_por_defecto"])
        else:
            r = vps._dialogo(0, titulo, p["filtro"], carpeta, vps.filedialog.askopenfilename,
                             initialdir=carpeta, filetypes=tipos or [("Todo", "*")])
        if r:
            p["archivo"] = r
            self._disparar("seleccionado")
            return r
        self._disparar("cancelado")
        return None

    def abrir(self):
        """Diálogo de abrir; devuelve la ruta elegida o None."""
        return self._elegir("abrir")

    def guardar(self):
        """Diálogo de guardar (el sistema pide confirmación si el archivo ya existe); la ruta o None."""
        return self._elegir("guardar")

    def mostrar(self):
        """Abre el diálogo que diga `modo`; devuelve la ruta o None."""
        return self._elegir("guardar" if self._p["modo"] == "guardar" else "abrir")


@_props
class SelectorDeCarpetas(_Sin):
    """Sin ventana: abre el diálogo de elegir carpeta. `mostrar()` devuelve la ruta o None; queda en
    `carpeta`. Dispara «seleccionado» o «cancelado»."""
    tipo = "SelectorDeCarpetas"
    PROPS = (("titulo", "str", ""), ("carpeta", "str", ""))
    EVENTOS = ("seleccionado", "cancelado")
    EVENTO_DEFECTO = "seleccionado"
    ORDEN = ("titulo", "carpeta")

    def mostrar(self):
        p = self._p
        r = vps.dialogo_carpeta(p["titulo"] or "Elegir carpeta", p["carpeta"] or None)
        if r:
            p["carpeta"] = r
            self._disparar("seleccionado")
            return r
        self._disparar("cancelado")
        return None


class _Campo(Control):
    """Base de CampoArchivo y CampoCarpeta: Entry con el botón «...»."""
    ORDEN = ("x", "y", "ancho", "alto")
    TAM = (260, 22)
    _ignorar = False

    def _crear(self):
        from vps import _widgets
        self._var = vps.tk.StringVar()
        self.w = _widgets._CampoW(self._contenedor_tk(), textvariable=self._var)
        self.w.al_pulsar = self.elegir
        self._var.trace_add("write", lambda *a: self._texto_cambio())

    def _texto_cambio(self):
        if self._ignorar:
            return
        self._p["texto"] = self._var.get()
        self._disparar("cambio")

    def _leer(self, n):
        if n == "texto" and self.w is not None:
            return self._var.get()
        return self._p.get(n)

    def _aplicar(self, n):
        if n == "texto":
            t = str(self._p["texto"])
            if t != self._var.get():
                self._var.set(t)
        elif n == "solo_lectura" or n == "activo":
            self._cfg(state="normal" if self._p["activo"] and not self._p.get("solo_lectura") else "disabled")
        else:
            Control._aplicar(self, n)

    def elegir(self):
        """Abre el selector (como al pulsar «...»): deja la ruta en el cuadro. Devuelve la ruta o None."""
        if not self._p["activo"]:
            return None
        r = self._abrir_dialogo()
        if r:
            self._p["texto"] = r
            self._var.set(r)
            self._disparar("seleccionado")
            return r
        self._disparar("cancelado")
        return None


@_props
class CampoArchivo(_Campo):
    """Cuadro de texto con el botón «...»: abre el selector de archivos y deja la ruta en `texto`.
    Evento `cambio` al cambiar el texto (a mano o con el selector)."""
    tipo = "CampoArchivo"
    PROPS = (("texto", "str", ""), ("titulo", "str", ""), ("filtro", "str", ""), ("carpeta", "str", ""),
             ("modo", "enum:abrir|guardar", "abrir"), ("extension_por_defecto", "str", ""),
             ("solo_lectura", "bool", False))
    EVENTOS = _ev("cambio", "seleccionado", "cancelado")
    EVENTO_DEFECTO = "cambio"

    def _abrir_dialogo(self):
        s = SelectorDeArchivos(self.form, nombre="_sel_" + self.nombre, titulo=self._p["titulo"],
                               filtro=self._p["filtro"], carpeta=self._p["carpeta"], archivo=self._p["texto"],
                               modo=self._p["modo"], extension_por_defecto=self._p["extension_por_defecto"])
        try:
            return s.mostrar()
        finally:
            s.destruir()


@_props
class CampoCarpeta(_Campo):
    """Cuadro de texto con el botón «...»: abre el selector de carpetas y deja la ruta en `texto`."""
    tipo = "CampoCarpeta"
    PROPS = (("texto", "str", ""), ("titulo", "str", ""), ("solo_lectura", "bool", False))
    EVENTOS = _ev("cambio", "seleccionado", "cancelado")
    EVENTO_DEFECTO = "cambio"

    def _abrir_dialogo(self):
        return vps.dialogo_carpeta(self._p["titulo"] or "Elegir carpeta", self._p["texto"] or None) or None


TIPOS = {"SelectorDeArchivos": SelectorDeArchivos, "SelectorDeCarpetas": SelectorDeCarpetas,
         "CampoArchivo": CampoArchivo, "CampoCarpeta": CampoCarpeta}

METODOS = {
    "SelectorDeArchivos": [("abrir", "abrir()", "Diálogo de abrir; devuelve la ruta o None"),
                           ("guardar", "guardar()", "Diálogo de guardar; devuelve la ruta o None"),
                           ("mostrar", "mostrar()", "Abre el diálogo que diga modo; devuelve la ruta o None")],
    "SelectorDeCarpetas": [("mostrar", "mostrar()", "Diálogo de elegir carpeta; devuelve la ruta o None")],
    "CampoArchivo": [("elegir", "elegir()", "Abre el selector, como al pulsar «...»; devuelve la ruta o None")],
    "CampoCarpeta": [("elegir", "elegir()", "Abre el selector, como al pulsar «...»; devuelve la ruta o None")],
}


# ------------------------------------------------------------ color y fuente (v8.4.3, D303)
# Usan los diálogos comunes de color y de fuente (`sagui.dialogo(3, ...)` y `sagui.dialogo(4, ...)`, D327).
# Si el sistema no los tiene todavía, o falla la llamada, se usa un respaldo en Python: la paleta de 32 colores
# y un diálogo sencillo de fuente.

def _color_a_int(r):
    """El resultado de un diálogo de color (entero, «#rrggbb» o «0xrrggbb») -> entero, o None si se canceló."""
    if r is None or r == "" or r is False:
        return None
    if isinstance(r, int):
        return r
    s = str(r).strip().lstrip("#")
    if s[:2].lower() == "0x":
        s = s[2:]
    try:
        return int(s, 16)
    except ValueError:
        return None


def _dialogo_sagui(tipo, titulo, inicial):
    """Llama a sagui.dialogo(tipo, titulo, "", inicial); (True, resultado) o (False, None) si no existe."""
    try:
        import sagui
        return True, sagui.dialogo(tipo, titulo, "", inicial)
    except Exception:
        return False, None


def _paleta(titulo, inicial):
    """Respaldo: la paleta de 32 colores en un diálogo modal. Devuelve el color o None."""
    import tkinter as tk
    res = [None]
    d = tk.Toplevel(tk._raiz_padrao(), _dialogo=True)
    d.title(titulo)
    d.resizable(False, False)
    cv = tk.Canvas(d, width=8 * 28 + 8, height=4 * 28 + 8, background="#f4f4f0", highlightthickness=0)
    cv.pack(padx=6, pady=6)
    for i, c in enumerate(vps.PALETA):
        cx, cy = 4 + (i % 8) * 28, 4 + (i // 8) * 28
        cv.create_rectangle(cx, cy, cx + 24, cy + 24, fill="#%06x" % c, outline="#000000")

    def pulsar(e):
        col, fil = (e.x - 4) // 28, (e.y - 4) // 28
        if 0 <= col < 8 and 0 <= fil < 4:
            res[0] = vps.PALETA[fil * 8 + col]
            d.destroy()
    cv.bind("<Button-1>", pulsar)
    try:
        d.grab_set()
        d.wait_window()
    except Exception:
        pass
    return res[0]


@_props
class SelectorDeColor(_Sin):
    """Sin ventana: abre el diálogo de color del sistema (espectro, HSL y RGB). `color` es el color inicial
    y, después, el elegido (0xRRGGBB). `mostrar()` devuelve el color o None; eventos `seleccionado` y `cancelado`."""
    tipo = "SelectorDeColor"
    PROPS = (("titulo", "str", ""),)
    EVENTOS = ("seleccionado", "cancelado")
    EVENTO_DEFECTO = "seleccionado"
    ORDEN = ("titulo",)

    def mostrar(self):
        c = self._p.get("color")
        ini = vps._color_int(c, 0) if c is not None else 0
        titulo = self._p["titulo"] or "Elegir color"
        hay, r = _dialogo_sagui(3, titulo, "#%06x" % ini)
        r = _color_a_int(r) if hay else _paleta(titulo, ini)
        if r is not None:
            self._p["color"] = r
            self._disparar("seleccionado")
            return r
        self._disparar("cancelado")
        return None


ESTILOS = ("normal", "negrita", "cursiva", "negrita cursiva")


@_props
class SelectorDeFuente(_Sin):
    """Sin ventana: abre el diálogo de fuente del sistema (familia, estilo, tamaño, efectos y muestra).
    Propiedades `familia`, `estilo` (normal, negrita, cursiva, negrita cursiva), `tamano`, `subrayado`,
    `tachado` y `color`. `mostrar()` devuelve True si se aceptó; `cadena()` es la fuente en el formato de la
    propiedad `fuente` y `aplicar_a(control)` se la pone (y el color) a un control de una vez."""
    tipo = "SelectorDeFuente"
    PROPS = (("titulo", "str", ""), ("familia", "str", "Sagui"), ("estilo", "enum:normal|negrita|cursiva|negrita cursiva", "normal"),
             ("tamano", "int", 12), ("subrayado", "bool", False), ("tachado", "bool", False))
    EVENTOS = ("seleccionado", "cancelado")
    EVENTO_DEFECTO = "seleccionado"
    ORDEN = ("familia", "tamano")

    def cadena(self):
        """«DejaVu Sans 14 negrita subrayado»: la fuente como la entiende la propiedad `fuente` de los controles."""
        p = self._p
        r = "%s %d" % (p["familia"], int(p["tamano"]))
        if "negrita" in str(p["estilo"]):
            r += " negrita"
        if "cursiva" in str(p["estilo"]):
            r += " cursiva"
        if p["subrayado"]:
            r += " subrayado"
        return r

    def aplicar_a(self, control):
        """Pone la fuente (y el color, si se ha elegido uno) en el control."""
        control.fuente = self.cadena()
        if self._p.get("color") is not None:
            control.color = self._p["color"]

    def _de_resultado(self, r):
        """El diálogo del sistema (D327) devuelve la tupla (familia, estilo, tamaño, efectos, color), con
        estilo 1 negrita + 2 cursiva, efectos 1 subrayado + 2 tachado y color 0xRRGGBB, o None si se cancela."""
        if not isinstance(r, (tuple, list)) or len(r) < 3 or not r[0]:
            return False
        q = self._p
        q["familia"] = str(r[0])
        q["estilo"] = ESTILOS[int(r[1]) & 3]
        q["tamano"] = int(r[2])
        if len(r) > 3:
            q["subrayado"], q["tachado"] = bool(int(r[3]) & 1), bool(int(r[3]) & 2)
        if len(r) > 4 and r[4] is not None:
            q["color"] = int(r[4]) & 0xFFFFFF
        return True

    def _respaldo(self, titulo):
        """Diálogo sencillo en Python: familia, estilo, tamaño y efectos."""
        import tkinter as tk
        from tkinter import ttk
        q = self._p
        ok = [False]
        d = tk.Toplevel(tk._raiz_padrao(), _dialogo=True)
        d.title(titulo)
        d.resizable(False, False)
        fam = tk.StringVar(value=str(q["familia"]))
        est = tk.StringVar(value=str(q["estilo"]))
        tam = tk.StringVar(value=str(q["tamano"]))
        sub = tk.IntVar(value=1 if q["subrayado"] else 0)
        tac = tk.IntVar(value=1 if q["tachado"] else 0)
        for i, (t, w) in enumerate((("Fuente:", ttk.Combobox(d, textvariable=fam, values=list(FAMILIAS), width=22)),
                                    ("Estilo:", ttk.Combobox(d, textvariable=est, values=list(ESTILOS), width=22)),
                                    ("Tamaño:", tk.Entry(d, textvariable=tam, width=8)))):
            tk.Label(d, text=t).grid(row=i, column=0, sticky="w", padx=6, pady=2)
            w.grid(row=i, column=1, sticky="w", padx=6, pady=2)
        tk.Checkbutton(d, text="Subrayado", variable=sub).grid(row=3, column=0, columnspan=2, sticky="w", padx=6)
        tk.Checkbutton(d, text="Tachado", variable=tac).grid(row=4, column=0, columnspan=2, sticky="w", padx=6)

        def aceptar():
            ok[0] = True
            d.destroy()
        b = tk.Frame(d)
        b.grid(row=5, column=0, columnspan=2, pady=6)
        tk.Button(b, text="Aceptar", width=10, command=aceptar).pack(side="left", padx=4)
        tk.Button(b, text="Cancelar", width=10, command=d.destroy).pack(side="left", padx=4)
        try:
            d.grab_set()
            d.wait_window()
        except Exception:
            pass
        if not ok[0]:
            return False
        q["familia"] = fam.get() or q["familia"]
        q["estilo"] = est.get() if est.get() in ESTILOS else "normal"
        try:
            q["tamano"] = max(4, min(96, int(tam.get())))
        except ValueError:
            pass
        q["subrayado"], q["tachado"] = bool(sub.get()), bool(tac.get())
        return True

    def mostrar(self):
        p = self._p
        titulo = p["titulo"] or "Elegir fuente"
        est = str(p["estilo"])
        estilo = (1 if "negrita" in est else 0) | (2 if "cursiva" in est else 0)
        efectos = (1 if p["subrayado"] else 0) | (2 if p["tachado"] else 0)
        # D327: «familia;tam;estilo;efectos;RRGGBB», números en decimal.
        ini = "%s;%d;%d;%d" % (p["familia"], int(p["tamano"]), estilo, efectos)
        if p.get("color") is not None:
            ini += ";%06x" % vps._color_int(p["color"], 0)
        hay, r = _dialogo_sagui(4, titulo, ini)
        ok = self._de_resultado(r) if hay else self._respaldo(titulo)
        self._disparar("seleccionado" if ok else "cancelado")
        return bool(ok)


FAMILIAS = ("Sagui", "DejaVu Sans", "DejaVu Sans Mono", "DejaVu Serif", "Sans", "Serif", "Monospace")


@_props
class ComboFuentes(vps.Combo):
    """Desplegable de familias de fuente con buscador: al escribir, la lista se reduce a las que contienen lo
    escrito. `texto` es la familia elegida; eventos `cambio` y `seleccion`. (El sistema solo tiene una letra de
    mapa de bits, así que los nombres salen en ella.)"""
    tipo = "ComboFuentes"
    PROPS = vps.Combo.PROPS
    ORDEN = ("x", "y", "ancho", "alto")
    TAM = (160, 22)

    def __init__(self, padre, *args, **kw):
        kw.setdefault("elementos", list(FAMILIAS))
        kw.setdefault("editable", True)
        vps.Combo.__init__(self, padre, *args, **kw)

    def filtrar(self, texto=None):
        """Las familias que contienen `texto` (por omisión, el escrito), sin distinguir mayúsculas."""
        t = str(self._p["texto"] if texto is None else texto).lower()
        return [f for f in FAMILIAS if t in f.lower()]

    def _escrito(self):
        self._p["texto"] = self._var.get()
        try:
            self.w.configure(values=self.filtrar() or list(FAMILIAS))
        except Exception:
            pass
        self._disparar("cambio")


TIPOS.update({"SelectorDeColor": SelectorDeColor, "SelectorDeFuente": SelectorDeFuente, "ComboFuentes": ComboFuentes})
METODOS.update({
    "SelectorDeColor": [("mostrar", "mostrar()", "Diálogo de color; devuelve el color (0xRRGGBB) o None")],
    "SelectorDeFuente": [("mostrar", "mostrar()", "Diálogo de fuente; True si se aceptó"),
                         ("cadena", "cadena()", "La fuente como texto para la propiedad fuente: \"DejaVu Sans 14 negrita\""),
                         ("aplicar_a", "aplicar_a(control)", "Pone la fuente y el color elegidos en un control")],
    "ComboFuentes": [("filtrar", "filtrar(texto)", "Las familias que contienen el texto")],
})
