# SPDX-License-Identifier: GPL-2.0-or-later
# vps._editor - la ventana de código del diseñador (mejora 123, D297): un editor de Python con
# resaltado y, encima, los dos combos «control» y «evento» del Visual Basic. Desde la mejora 124
# (D299) también tiene deshacer y rehacer (Ctrl+Z, Ctrl+Y), autocompletado (tras un punto, o con
# Ctrl+Espacio), la firma de la función al abrir un paréntesis y puntos de parada (F9).
import tkinter as tk
from tkinter import ttk
import sagui
import vps
from vps import _modelo as M, _autocompletar as AC

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


FONDO_ERROR, FONDO_PUNTO, FONDO_ACTUAL = 0xFFC8C8, 0xFFD890, 0xB8D8FF
IDENT = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_"


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
        cuerpo = self.cuerpo = tk.Frame(top)
        cuerpo.pack(fill="both", expand=True)
        self.sb = tk.Scrollbar(cuerpo, orient="vertical")
        self.sb.pack(side="right", fill="y")
        self.t = tk.Text(cuerpo, wrap="none", width=80, height=24, yscrollcommand=self.sb.set)
        self.t.pack(side="left", fill="both", expand=True)
        self.sb.configure(command=self.t.yview)
        self.t._realce = realzar
        self.hist = []          # deshacer: [(texto, cursor)]
        self.rehist = []
        self._ult_tipo = None
        self._ult_t = 0
        self.puntos = set()     # líneas con punto de parada (F9)
        self._sug = None        # la lista de sugerencias (Listbox) cuando está abierta
        self._sug_items = []
        self._sug_prefijo = ""
        self._lineas_antes = 1
        self._linea_antes = 1
        self._silencio = False  # Esc cerró la lista: no se vuelve a abrir hasta empezar otra palabra
        t = self.t
        t.bind("<Return>", self._intro)
        t.bind("<Tab>", self._tab)
        t.bind("<Key>", self._tecla)
        t.bind("<Button-1>", lambda e: (self._cerrar_sug(), self._sin_silencio(), t.after_idle(self._posicion)))
        t.bind("<F5>", lambda e: dis.ejecutar())
        t.bind("<F8>", lambda e: dis.depurar_paso())
        t.bind("<F9>", lambda e: self.alternar_punto())
        t.bind("<Control-z>", self._atajo_deshacer)
        t.bind("<Control-y>", self._atajo_rehacer)
        t.bind("<Control-space>", lambda e: (self._sugerir(True), "break")[1])
        t.bind("<Up>", lambda e: self._flecha(-1))
        t.bind("<Down>", lambda e: self._flecha(1))
        t.bind("<Escape>", self._escape)
        top.protocol("WM_DELETE_WINDOW", self.ocultar)
        self.linea_error = None
        self.ocultar()

    def _sin_silencio(self):
        self._silencio = False

    # --- ventana
    def mostrar(self):
        self.top.deiconify()
        self.t.focus_set()

    def ocultar(self):
        self._cerrar_sug()
        self.top.withdraw()

    def visible(self):
        return not self.top._retirada

    def titulo(self, nombre):
        self.top.title("Código - " + nombre)

    # --- texto
    def texto(self):
        return self.t.get("1.0", "end-1c")

    def poner(self, s, registrar=False):
        """Pone el texto del código. Con registrar=True el anterior queda en el historial
        (deshacer del diseñador); si no, es un código nuevo y el historial se vacía."""
        self._silencio = False
        if registrar:
            self._guardar_historial()
        else:
            self.hist = []
            self.rehist = []
            self.puntos = set()
            self.t._fondo_linea.clear()
            self.linea_error = None
        self.t.delete("1.0", "end")
        self.t.insert("1.0", s)
        self.t.mark_set("insert", "1.0")
        self.quitar_error()
        self._pintar_puntos()
        self._posicion()

    def _posicion(self):
        l, c = self.t.index("insert").split(".")
        self.estado.configure(text="Línea %s, columna %d" % (l, int(c) + 1))

    def _linea_y_total(self):
        return int(self.t.index("insert").split(".")[0]), len(self.lineas())

    # --- deshacer y rehacer
    def _guardar_historial(self):
        self.hist.append((self.texto(), self.t.index("insert")))
        if len(self.hist) > 200:
            del self.hist[0]
        self.rehist = []

    def _historial(self, tipo):
        """Se llama antes de cada cambio del texto. Las teclas seguidas del mismo tipo
        (escribir, borrar) se agrupan en un solo paso mientras no pase más de un segundo."""
        ahora = sagui.ticks_ms()
        if tipo != self._ult_tipo or tipo in ("intro", "pegar") or ahora - self._ult_t > 1000 or not self.hist:
            self._guardar_historial()
        self._ult_tipo = tipo
        self._ult_t = ahora

    def _restaurar(self, texto, cursor):
        self.t.delete("1.0", "end")
        self.t.insert("1.0", texto)
        try:
            self.t.mark_set("insert", cursor)
        except Exception:
            self.t.mark_set("insert", "1.0")
        self.t.see("insert")
        n = len(self.lineas())
        self.puntos = set(p for p in self.puntos if p <= n)
        self.t._fondo_linea.clear()
        self.linea_error = None
        self._pintar_puntos()
        self._ult_tipo = None
        self._posicion()
        self.dis.marcar_modificado()
        self.actualizar_combos()

    def deshacer(self):
        if not self.hist:
            self.estado.configure(text="No hay nada que deshacer en el código")
            return False
        self.rehist.append((self.texto(), self.t.index("insert")))
        self._restaurar(*self.hist.pop())
        return True

    def rehacer(self):
        if not self.rehist:
            self.estado.configure(text="No hay nada que rehacer en el código")
            return False
        self.hist.append((self.texto(), self.t.index("insert")))
        self._restaurar(*self.rehist.pop())
        return True

    def _atajo_deshacer(self, ev):
        self._cerrar_sug()
        self.deshacer()
        return "break"

    def _atajo_rehacer(self, ev):
        self._cerrar_sug()
        self.rehacer()
        return "break"

    # --- teclas
    PARES = {"(": ")", "[": "]", "{": "}", '"': '"', "'": "'"}

    def _auto_cierre(self, ch):
        """Cierre automático de paréntesis, corchetes y comillas. True si ya se atendió la tecla
        (se inserta el par con el cursor en medio, o se salta el cierre que ya estaba)."""
        t = self.t
        l, c = t.index("insert").split(".")
        linea = t.get("%s.0" % l, "%s.end" % l)
        c = int(c)
        sig = linea[c:c + 1]
        ant = linea[c - 1:c] if c else ""
        if ch in (")", "]", "}", '"', "'") and sig == ch:
            if ch in ('"', "'") and not AC.en_texto(linea[:c]):
                return False        # unas comillas que abren, no que cierran
            t.mark_set("insert", "insert+1c")
            return True
        if ch not in self.PARES:
            return False
        if ch in ('"', "'"):
            if AC.en_texto(linea[:c]) or (ant and (ant.isalpha() or ant.isdigit() or ant == "_" or ant in "\\")):
                return False
        elif AC.en_texto(linea[:c]):
            return False
        if sig and not (sig.isspace() or sig in ")]},:;"):
            return False            # delante hay texto: no se cierra solo
        t.insert("insert", ch + self.PARES[ch])
        t.mark_set("insert", "insert-1c")
        return True

    def _borrar_par(self):
        """BackSpace entre un par vacío «()», «[]», «""»: se borran los dos."""
        t = self.t
        l, c = t.index("insert").split(".")
        linea = t.get("%s.0" % l, "%s.end" % l)
        c = int(c)
        if c and linea[c - 1:c] in self.PARES and linea[c:c + 1] == self.PARES[linea[c - 1:c]]:
            t.delete("insert-1c", "insert+1c")
            return True
        return False

    def _tecla(self, ev):
        k = ev.keysym
        ctrl = ev.state & 4
        self._linea_antes, self._lineas_antes = self._linea_y_total()
        self._k_antes = k
        if not ctrl and ev.char and len(ev.char) == 1 and ev.char in "()[]{}\"'":
            self._historial("teclear")
            self._cerrar_sug()
            if self._auto_cierre(ev.char):
                self.dis.marcar_modificado()
                self._posicion()
                if ev.char == "(":
                    self._ayuda_llamada()
                return "break"
        elif k == "BackSpace" and not ctrl:
            self._historial("borrar")
            self._cerrar_sug()
            if self._borrar_par():
                self.dis.marcar_modificado()
                self._posicion()
                return "break"
        if k in ("BackSpace", "Delete"):
            self._historial("borrar")
        elif ctrl and k in ("v", "x"):
            self._historial("pegar")
        elif not ctrl and ev.char and len(ev.char) == 1 and ev.char >= " " and ev.char != "\x7f":
            self._historial("teclear")
        self.t.after_idle(self._tras_tecla, ev.char, k)

    def _tras_tecla(self, ch, k):
        self._posicion()
        if k in ("BackSpace", "Delete", "Return") or (ch and len(ch) == 1 and ch >= " "):
            self.dis.marcar_modificado()
        l, total = self._linea_y_total()
        delta = total - self._lineas_antes
        if delta:
            self._desplazar_puntos(self._linea_antes, delta, k == "BackSpace")
        if ch == ".":
            self._silencio = False
            self._sugerir(False)
        elif self._sug is not None:
            if k == "BackSpace" or (ch and len(ch) == 1 and ch in IDENT):
                self._sugerir(False)
            else:
                self._cerrar_sug()
        elif ch == "(":
            self._ayuda_llamada()
        elif ch and len(ch) == 1 and ch in IDENT:
            if not self._silencio:
                self._sugerir(False, True)
        else:
            self._silencio = False

    def _intro(self, ev):
        if self._sug is not None:
            self._aceptar_sug()
            return "break"
        self._historial("intro")
        l, c = self.t.index("insert").split(".")
        linea = self.t.get("%s.0" % l, "%s.end" % l)[:int(c)]
        sangria = linea[:len(linea) - len(linea.lstrip(" "))]
        util = AC.sin_comentario(linea).rstrip()
        primera = util.strip().split(" ")[0].split("(")[0]
        if util.endswith(":"):
            sangria += "    "
        elif primera in ("return", "pass", "break", "continue", "raise") and len(sangria) >= 4:
            sangria = sangria[:-4]       # tras estas palabras el bloque suele acabar
        resto = self.t.get("insert", "insert lineend")
        if resto[:1] in (")", "]", "}") and util.endswith(("(", "[", "{")):
            # Intro entre un par «(|)»: el cierre baja a su propia línea
            self.t.insert("insert", "\n" + sangria + "    \n" + sangria)
            self.t.mark_set("insert", "insert-%dc" % (len(sangria) + 1))
            self.t.see("insert")
            self._desplazar_puntos(int(l), 2, False)
            self.dis.marcar_modificado()
            self._posicion()
            return "break"
        self.t.insert("insert", "\n" + sangria)
        self.t.see("insert")
        self._desplazar_puntos(int(l), 1, False)
        self.dis.marcar_modificado()
        self._posicion()
        return "break"

    def _snippet_en_cursor(self):
        """Si justo antes del cursor hay una palabra con plantilla y delante solo sangría, la expande
        (def, if, elif, else, for, while, try, class, with). True si lo hizo."""
        linea = self._linea_hasta_cursor()
        palabra = linea.strip()
        if palabra not in AC.SNIPPETS or not linea.endswith(palabra):
            return False
        sangria = linea[:len(linea) - len(linea.lstrip(" "))]
        texto, desp = AC.expandir_snippet(palabra, sangria)
        self._historial("pegar")
        self.t.delete("insert-%dc" % len(palabra), "insert")
        pos = self.t.index("insert")
        self.t.insert("insert", texto)
        self.t.mark_set("insert", "%s+%dc" % (pos, desp))
        self.t.see("insert")
        self._cerrar_sug()
        self.dis.marcar_modificado()
        self._posicion()
        print("vps: plantilla|%s" % palabra)
        return True

    def _tab(self, ev):
        if self._snippet_en_cursor():
            return "break"
        if self._sug is not None:
            self._aceptar_sug()
            return "break"
        self._historial("teclear")
        self.t.insert("insert", "    ")
        self.dis.marcar_modificado()
        return "break"

    def _flecha(self, d):
        if self._sug is None:
            self.t.after_idle(self._posicion)
            return None
        self._mover_sug(d)
        return "break"

    def _escape(self, ev):
        if self._sug is None:
            return None
        self._cerrar_sug()
        self._silencio = True
        return "break"

    # --- autocompletado
    def _linea_hasta_cursor(self):
        l, c = self.t.index("insert").split(".")
        return self.t.get("%s.0" % l, "%s.%s" % (l, c))

    def _sugerir(self, forzado, tecleado=False):
        """Abre (o actualiza) la lista de sugerencias para lo escrito hasta el cursor. Al ir
        escribiendo solo sale con 2 caracteres como mínimo; con Ctrl+Espacio (forzado), con cualquier
        cantidad."""
        n_linea = int(self.t.index("insert").split(".")[0])
        prefijo, cand = AC.candidatos(self._linea_hasta_cursor(), self.dis.proy["forms"], self.dis.form["nombre"],
                                      self.texto(), n_linea, 0 if forzado else 2)
        if not cand:
            self._cerrar_sug()
            if forzado:
                self.estado.configure(text="Sin sugerencias")
            return
        self._sug_items = cand
        self._sug_prefijo = prefijo
        n = min(len(cand), 10)
        ancho = max(len(c[2]) for c in cand)
        ancho = max(18, min(ancho, 46)) * 8 + 24
        b = self.t.bbox("insert")
        x = (b[0] if b else 0) - len(prefijo) * 8
        y = (b[1] if b else 0) + 18
        if self._sug is None:
            self._sug = tk.Listbox(self.cuerpo, height=n, width=10, takefocus=0, exportselection=0)
            self._sug.bind("<Double-Button-1>", lambda e: self._aceptar_sug())
            self._sug.bind("<Button-1>", lambda e: self._sug.after_idle(self._clic_sug))
        s = self._sug
        s.delete(0, "end")
        for texto, clase, firma, ayuda in cand:
            marca = {"propiedad": "p", "método": "m", "evento": "e", "control": "c", "palabra": "k",
                     "variable": "v", "clase": "C", "integrada": "i", "módulo": "M"}.get(clase, "f")
            s.insert("end", "%s %s" % (marca, firma if clase != "evento" else texto.split("(")[0] + " (evento)"))
        s.configure(height=n)
        tw = self.t.winfo_width() or 600
        s.place(x=max(0, min(x, tw - ancho)), y=y, width=ancho, height=n * 16 + 6)
        s.selection_clear(0, "end")
        s.selection_set(0)
        self._sug_sel = 0
        self._ayuda_sug()
        print("vps: sugerencias|%d" % len(cand))

    def _clic_sug(self):
        sel = self._sug.curselection() if self._sug is not None else ()
        if sel:
            self._sug_sel = sel[0]
            self._ayuda_sug()
        self.t.focus_set()

    def _mover_sug(self, d):
        n = len(self._sug_items)
        self._sug_sel = (self._sug_sel + d) % n
        self._sug.selection_clear(0, "end")
        self._sug.selection_set(self._sug_sel)
        self._sug.see(self._sug_sel)
        self._ayuda_sug()

    def _ayuda_sug(self):
        texto, clase, firma, ayuda = self._sug_items[self._sug_sel]
        self.estado.configure(text="%s - %s" % (firma, ayuda))

    def _cerrar_sug(self):
        if self._sug is not None:
            self._sug.place_forget()
            self._sug.destroy()
            self._sug = None
            self._sug_items = []
            self._posicion()

    def _aceptar_sug(self):
        if self._sug is None:
            return
        texto, clase, firma, ayuda = self._sug_items[self._sug_sel]
        self._historial("pegar")
        p = len(self._sug_prefijo)
        if p:
            self.t.delete("insert-%dc" % p, "insert")
        if clase == "método" or (clase in ("función", "integrada") and "(" in firma):
            texto += "()" if firma.endswith("()") else "("
        self.t.insert("insert", texto)
        self._cerrar_sug()
        if clase == "palabra" and texto in AC.SNIPPETS and self._snippet_en_cursor():
            return
        self.dis.marcar_modificado()
        self.t.focus_set()
        if clase in ("método", "función", "integrada") and not firma.endswith("()") and "(" in firma:
            self._ayuda_llamada()

    def _ayuda_llamada(self):
        h = AC.ayuda_llamada(self._linea_hasta_cursor(), self.dis.proy["forms"], self.texto())
        if h:
            self.estado.configure(text=h)

    # --- puntos de parada
    def _pintar_puntos(self):
        for n in list(self.t._fondo_linea.keys()):
            if self.t._fondo_linea[n] == FONDO_PUNTO:
                del self.t._fondo_linea[n]
        for n in self.puntos:
            if n != self.linea_error:
                self.t._fondo_linea[n] = FONDO_PUNTO
        self.t._redesenhar()

    def alternar_punto(self, n=None):
        """F9: pone o quita el punto de parada de la línea (la del cursor por defecto)."""
        if n is None:
            n = self._linea_y_total()[0]
        if n in self.puntos:
            self.puntos.discard(n)
            self.t._fondo_linea.pop(n, None)
            self.estado.configure(text="Punto de parada quitado de la línea %d" % n)
        else:
            self.puntos.add(n)
            self.estado.configure(text="Punto de parada en la línea %d" % n)
        print("vps: punto de parada|%d|%s" % (n, "si" if n in self.puntos else "no"))
        self._pintar_puntos()

    def quitar_puntos(self):
        self.puntos = set()
        self._pintar_puntos()

    def _desplazar_puntos(self, linea, delta, atras):
        """Se añadieron o quitaron líneas en `linea`: los puntos de parada de más abajo bajan o suben."""
        if not self.puntos:
            return
        if delta > 0:
            limite = linea
        else:
            limite = linea + delta if atras else linea
        nuevos = set()
        for p in self.puntos:
            nuevos.add(p + delta if p > limite else p)
        self.puntos = set(max(1, p) for p in nuevos)
        self._pintar_puntos()

    def marcar_actual(self, n):
        """La línea donde está parado el depurador (None la desmarca)."""
        for k in list(self.t._fondo_linea.keys()):
            if self.t._fondo_linea[k] == FONDO_ACTUAL:
                del self.t._fondo_linea[k]
        if n is not None:
            self.t._fondo_linea[n] = FONDO_ACTUAL
            self.mostrar_sin_foco()
            self.ir_a_linea(n)
        self._pintar_puntos()

    def mostrar_sin_foco(self):
        self.top.deiconify()

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
            self.dis.punto()
            self._guardar_historial()
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
        self.t._fondo_linea[n] = FONDO_ERROR
        self.estado.configure(text="Error en la línea %d: %s" % (n, mensaje))
        self.ir_a_linea(n)
        self.t._redesenhar()

    def quitar_error(self):
        if self.linea_error is not None:
            self.t._fondo_linea.pop(self.linea_error, None)
            self.linea_error = None
            self._pintar_puntos()
