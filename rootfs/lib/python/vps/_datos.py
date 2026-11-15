# SPDX-License-Identifier: GPL-2.0-or-later
# vps._datos - bases de datos en VisualPythonS (mejora 127, D304): el conector `ConectorBD` (sin
# ventana, como el Temporizador), la rejilla `RejillaDatos` y el enlace de controles de datos
# (`CuadroTexto`, `Casilla`, `Numerico` y `Combo` con `fuente` y `campo`).
#
#     BD = ConectorBD(f, origen="sqlite:/home/datos.db", tabla="contactos")
#     Rejilla = RejillaDatos(f, 8, 8, 360, 200, fuente=BD, editable=True)
#     Nombre = CuadroTexto(f, 8, 220, 160, 22, fuente=BD, campo="nombre")
#     BD.abrir()
#
# En el diseñador `fuente` es el NOMBRE del conector (un texto). Todo el acceso a las bases va por
# la biblioteca `basedatos` (parámetros enlazados, nunca SQL concatenado).
import vps
import tkinter as tk
from vps import Control, _props
import basedatos
from basedatos import ErrorBD

ENLAZABLES = ("CuadroTexto", "Casilla", "Numerico", "Combo")


def _msg(texto, icono="error"):
    try:
        vps.msgbox(texto, "Base de datos", "aceptar", icono)
    except Exception:
        print("basedatos:", texto)


# ------------------------------------------------------------ enlace de los controles de datos

class _Enlace:
    """Lo que añade `fuente`/`campo` a un control: lo rellena con el registro actual y escribe en
    el conector lo que el usuario cambia."""

    def __init__(self, ctl):
        self.ctl = ctl
        self.cargando = False
        self.enganchado = False

    def conector(self):
        f = self.ctl._p.get("fuente")
        if isinstance(f, ConectorBD):
            return f
        if isinstance(f, str) and f:
            c = self.ctl.form.controles.get(f)
            if isinstance(c, ConectorBD):
                return c
        return None

    def campo(self):
        return self.ctl._p.get("campo") or ""

    def enganchar(self):
        if self.enganchado:
            return
        self.enganchado = True
        ev = "click" if self.ctl.tipo == "Casilla" else "cambio"
        self.ctl.conectar(ev, self.editado)

    def valor(self):
        c = self.ctl
        if c.tipo == "Casilla":
            return bool(c.valor)
        if c.tipo == "Numerico":
            return c.valor
        return c.texto

    def mostrar(self):
        cn, campo = self.conector(), self.campo()
        if cn is None or not campo:
            return
        v = cn.valor(campo) if cn.abierta else None
        c = self.ctl
        self.cargando = True
        try:
            if c.tipo == "Casilla":
                c.valor = bool(v)
            elif c.tipo == "Numerico":
                try:
                    c.valor = 0 if v is None or v == "" else v
                except Exception:
                    pass
            else:
                t = basedatos.texto_de_valor(v)
                if c.texto != t:
                    c.texto = t
        finally:
            self.cargando = False

    def editado(self):
        if self.cargando:
            return
        cn, campo = self.conector(), self.campo()
        if cn is not None and campo and cn.abierta:
            cn.poner(campo, self.valor(), origen=self)

    def evento(self, tipo, origen):
        if origen is not self:
            self.mostrar()


def enlazar(ctl):
    """Lo llama Control._aplicar cuando cambian `fuente` o `campo` de un control enlazable."""
    en = getattr(ctl, "_enl", None)
    if en is None:
        en = ctl._enl = _Enlace(ctl)
    if not ctl._p.get("fuente") or not ctl._p.get("campo"):
        return
    en.enganchar()
    cn = en.conector()
    if cn is not None and cn.abierta:
        en.mostrar()


# ------------------------------------------------------------ el conector

@_props
class ConectorBD(Control):
    """Conexión con una base de datos (SQLite o lite-monkey) y su tabla: sin ventana. Lleva el
    registro actual y la página de filas; los controles enlazados y la RejillaDatos le hacen caso."""
    tipo = "ConectorBD"
    PROPS = (("origen", "str", ""), ("tabla", "str", ""), ("filtro", "str", ""), ("orden", "str", ""),
             ("tam_pagina", "int", 50))
    EVENTOS = ("conectado", "error", "registro", "datos")
    EVENTO_DEFECTO = "conectado"
    ORDEN = ("origen", "tabla")
    TAM = (32, 32)

    def __init__(self, padre, *args, **kw):
        self._bd = None
        self.abierta = False
        self.error = ""
        self.filas = []          # la página actual: lista de diccionarios
        self.columnas = []       # nombres de las columnas, en orden
        self.tipos = {}          # nombre de columna -> tipo
        self.clave = ""          # columna que identifica una fila
        self.total = 0
        self.posicion = 0        # fila actual entre todas las del filtro (0..total-1)
        self._pend = {}
        self._nuevo = False
        self._oyentes = []
        kw.setdefault("activo", True)
        Control.__init__(self, padre, *args, **kw)
        if self._p["origen"]:
            # se abre al empezar el bucle: así ya están conectados los manejadores de eventos
            try:
                self.form.win.after(1, self._abrir_inicial)
            except Exception:
                pass

    def _abrir_inicial(self):
        if not self.abierta and not self.form._cerrado:
            self.abrir()

    def _crear(self):
        pass

    def _colocar(self):
        pass

    def _escribir(self, n, v):
        # sin ventana (w es None): Control._escribir no llamaría a _aplicar
        self._p[n] = v
        self._aplicar(n)

    def _aplicar(self, n):
        if n == "origen" and self.abierta and self._p["origen"] != getattr(self._bd, "origen", None):
            self.cerrar()
        elif n in ("tabla", "filtro", "orden", "tam_pagina") and self.abierta:
            if n == "tabla":
                self._reiniciar_tabla()
            self.recargar()

    def destruir(self):
        self.cerrar()
        self.form.controles.pop(self.nombre, None)

    # --- eventos y errores
    def _tiene_manejador(self, ev):
        if self._eventos.get(ev):
            return True
        ns = self.form._ns
        return ns is not None and callable(ns.get("%s_%s" % (self.nombre, ev)))

    def _fallo(self, e):
        self.error = str(e)
        self._disparar("error")
        if not self._tiene_manejador("error"):
            _msg(self.error)
        return False

    # --- conexión
    @property
    def bd(self):
        """El objeto de `basedatos` (para consultas propias: ConectorBD1.bd.ejecutar(...))."""
        return self._bd

    def abrir(self):
        """Abre la conexión y carga la primera página. True si salió bien; si no, dispara «error»."""
        self.cerrar()
        o = self._p["origen"]
        try:
            self._bd = basedatos.conectar(o)
            self.abierta = True
            self.error = ""
            if self._p["tabla"]:
                self._reiniciar_tabla()
                self._cargar(0)
        except ErrorBD as e:
            self.cerrar()
            return self._fallo(e)
        self._disparar("conectado")
        self._difundir("datos")
        return True

    def cerrar(self):
        if self._bd is not None:
            try:
                self._bd.cerrar()
            except Exception:
                pass
        self._bd = None
        was = self.abierta
        self.abierta = False
        self.filas, self.columnas, self.total, self.posicion = [], [], 0, 0
        self._pend, self._nuevo = {}, False
        if was:
            self._difundir("datos")

    def tablas(self):
        return self._bd.tablas() if self.abierta else []

    def _reiniciar_tabla(self):
        t = self._p["tabla"]
        self.tipos, self.clave, self.columnas = {}, "", []
        if not t or self._bd is None:
            return
        try:
            for c in self._bd.esquema(t):
                self.tipos[c["nombre"]] = c["tipo"]
            self.clave = self._bd.clave(t)
            self.columnas = self._bd.columnas(t)
        except ErrorBD as e:
            self.columnas = []
            self._fallo(e)

    # --- datos
    def _orden_lista(self):
        o = (self._p["orden"] or "").strip()
        return [x.strip() for x in o.split(",") if x.strip()] if o else None

    @property
    def pagina(self):
        n = max(1, int(self._p["tam_pagina"] or 50))
        return self.posicion // n

    @property
    def paginas(self):
        n = max(1, int(self._p["tam_pagina"] or 50))
        return max(1, (self.total + n - 1) // n)

    @property
    def indice(self):
        """La posición de la fila actual dentro de la página (-1 si no hay)."""
        n = max(1, int(self._p["tam_pagina"] or 50))
        i = self.posicion - self.pagina * n
        return i if 0 <= i < len(self.filas) else -1

    def _cargar(self, pos):
        t = self._p["tabla"]
        if not t or self._bd is None:
            return
        n = max(1, int(self._p["tam_pagina"] or 50))
        f = self._p["filtro"] or None
        self.total = self._bd.contar(t, f)
        pos = max(0, min(int(pos), max(0, self.total - 1)))
        self.posicion = pos
        self.filas = self._bd.consultar(t, f, self._orden_lista(), n, (pos // n) * n)
        if self._bd.tipo == "litemonkey":
            cols = list(self.columnas) if self.columnas else []
            extra = []
            for fila in self.filas:
                for k in fila:
                    if k not in cols and k not in extra:
                        extra.append(k)
            if not cols:
                cols = self._bd.columnas(t)
            self.columnas = cols + sorted(extra)

    def recargar(self):
        """Vuelve a leer la página actual (se descartan los cambios sin guardar)."""
        if not self.abierta or not self._p["tabla"]:
            return False
        self._pend, self._nuevo = {}, False
        try:
            self._cargar(self.posicion)
        except ErrorBD as e:
            return self._fallo(e)
        self._difundir("datos")
        return True

    @property
    def registro(self):
        """El registro actual (con los cambios sin guardar); {} si no hay o es nuevo."""
        r = {}
        if not self._nuevo and 0 <= self.indice < len(self.filas):
            r.update(self.filas[self.indice])
        r.update(self._pend)
        return r

    def valor(self, campo):
        return self.registro.get(campo)

    @property
    def modificado(self):
        return bool(self._pend)

    @property
    def es_nuevo(self):
        return self._nuevo

    def poner(self, campo, valor, origen=None):
        """Cambia un campo del registro actual (se escribe en la base al guardar o al moverse)."""
        if isinstance(valor, str):
            try:
                valor = basedatos.valor_desde_texto(valor, self.tipos.get(campo))
            except ErrorBD as e:
                self._fallo(e)
                return False
        if campo == self.clave and not self._nuevo:
            return False
        if not self._nuevo and 0 <= self.indice < len(self.filas) and self.filas[self.indice].get(campo) == valor:
            self._pend.pop(campo, None)
        else:
            self._pend[campo] = valor
        self._difundir("campo", origen)
        return True

    def guardar(self):
        """Escribe en la base lo pendiente (la fila nueva o los campos cambiados). True si bien."""
        if not self.abierta or not self._pend and not self._nuevo:
            return True
        t = self._p["tabla"]
        try:
            if self._nuevo:
                if self._pend:
                    self._bd.insertar(t, self._pend)
                    self._pend, self._nuevo = {}, False
                    self._cargar(10 ** 9 if not self._orden_lista() else self.posicion)
                else:
                    self._nuevo = False
            else:
                k = self.registro.get(self.clave) if self.clave == "rowid" else self.filas[self.indice].get(self.clave)
                if not self.clave:
                    raise basedatos.ErrorConsulta("«%s» es una vista: no se puede escribir en ella" % t)
                self._bd.actualizar(t, self._pend, clave=k)
                self._pend = {}
                self._cargar(self.posicion)
        except ErrorBD as e:
            return self._fallo(e)
        self._difundir("datos")
        return True

    def nuevo(self):
        """Empieza un registro nuevo y vacío (guardar() lo inserta). Guarda antes el pendiente."""
        if not self.abierta or not self._p["tabla"] or not self.guardar():
            return False
        self._nuevo = True
        self._pend = {}
        self._difundir("datos")
        return True

    def borrar(self):
        """Borra el registro actual (o cancela el nuevo). Sin pedir confirmación: la RejillaDatos
        la pide antes de llamar."""
        if not self.abierta or not self._p["tabla"]:
            return False
        if self._nuevo:
            self._nuevo, self._pend = False, {}
            self._difundir("datos")
            return True
        i = self.indice
        if i < 0 or not self.clave:
            return False
        try:
            self._bd.borrar(self._p["tabla"], clave=self.filas[i].get(self.clave))
            self._pend = {}
            self._cargar(self.posicion)
        except ErrorBD as e:
            return self._fallo(e)
        self._difundir("datos")
        return True

    # --- navegación
    def ir_a(self, pos):
        if not self.abierta or not self._p["tabla"] or not self.guardar():
            return False
        antes = self.pagina
        pos = max(0, min(int(pos), max(0, self.total - 1)))
        if pos == self.posicion and not self._nuevo:
            return True
        try:
            if pos // max(1, int(self._p["tam_pagina"] or 50)) != antes:
                self._cargar(pos)
                self._difundir("datos")
            else:
                self.posicion = pos
                self._difundir("registro")
        except ErrorBD as e:
            return self._fallo(e)
        self._disparar("registro")
        return True

    def primero(self):
        return self.ir_a(0)

    def ultimo(self):
        return self.ir_a(self.total - 1)

    def anterior(self):
        return self.ir_a(self.posicion - 1)

    def siguiente(self):
        return self.ir_a(self.posicion + 1)

    def ordenar(self, campo, descendente=None):
        """Ordena por esa columna (si ya lo estaba, invierte el sentido)."""
        actual = (self._p["orden"] or "").strip()
        if descendente is None:
            descendente = actual == campo
        self._p["orden"] = ("-" if descendente else "") + campo
        if self.abierta and self.guardar():
            try:
                self._cargar(0)
            except ErrorBD as e:
                return self._fallo(e)
            self._difundir("datos")

    # --- difusión a los controles enlazados
    def _escuchas(self):
        r = list(self._oyentes)
        for c in list(self.form.controles.values()):
            if c is self:
                continue
            en = getattr(c, "_enl", None)
            if en is not None and en.conector() is self:
                r.append(en)
            elif isinstance(c, RejillaDatos) and c._fuente() is self:
                r.append(c)
        return r

    def _difundir(self, tipo, origen=None):
        for o in self._escuchas():
            try:
                o.evento(tipo, origen)
            except Exception as e:
                vps._informar(e, "%s (enlace)" % self.nombre)


# ------------------------------------------------------------ la rejilla

class _MarcoRej(tk.Frame):
    """El Frame de la RejillaDatos. En el diseñador (formulario __vista__) se dibuja una
    maqueta; en ejecución lo dibujan sus hijos."""

    def __init__(self, master, ctl):
        tk.Frame.__init__(self, master)
        self.ctl = ctl

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        if self.ctl.form.nombre != "__vista__":
            tk.Frame._desenhar(self, v)
            return
        v.rect(x, y, w, h, 0xFFFFFF)
        v.rect(x, y, w, h, 0x808080, relleno=False)
        cn = self.ctl._fuente()
        cols = ["Campo1", "Campo2", "Campo3"]
        nombre = self.ctl._p.get("fuente")
        v.rect(x + 1, y + 1, w - 2, 20, 0xC0C0C0)
        cx = x + 1
        for c in cols:
            v.texto(cx + 4, y + 3, c, 0x000000)
            v.linea(cx + 80, y + 1, cx + 80, y + 20, 0x808080)
            cx += 80
        for i in range(max(0, (h - 44) // 18)):
            v.linea(x + 1, y + 21 + i * 18 + 17, x + w - 2, y + 21 + i * 18 + 17, 0xDFDFDF)
        v.rect(x + 1, y + h - 22, w - 2, 21, 0xC0C0C0)
        v.texto(x + 6, y + h - 20, "|<  <  >  >|   Nuevo  Borrar  Guardar", 0x000000)
        if nombre:
            v.texto(x + 6, y + 24, str(nombre), 0x000080)


@_props
class RejillaDatos(Control):
    """Rejilla de datos enlazada a un ConectorBD: paginada, ordenable por columna y editable."""
    tipo = "RejillaDatos"
    PROPS = (("editable", "bool", False), ("barra", "bool", True))
    EVENTOS = ("seleccion", "cambio")
    EVENTO_DEFECTO = "seleccion"
    TAM = (400, 200)
    NAV = 24

    def _crear(self):
        self.w = _MarcoRej(self._contenedor_tk(), self)
        self._g = None
        self._barra = []
        self._lbl = None
        self._modo = False        # estamos moviendo la selección por código
        self.w.bind("<Configure>", lambda e: self._relayout())
        self._construir()

    def _fuente(self):
        f = self._p.get("fuente")
        if isinstance(f, ConectorBD):
            return f
        if isinstance(f, str) and f:
            c = self.form.controles.get(f)
            if isinstance(c, ConectorBD):
                return c
        return None

    def _construir(self):
        from basedatos import _cuadricula
        w = self.w
        self._g = _cuadricula.Cuadricula(w, ancho=max(60, int(self._p["ancho"])), alto=max(60, int(self._p["alto"]) - self.NAV))
        self._g.al_seleccionar = self._al_seleccionar
        self._g.al_ordenar = self._al_ordenar
        self._g.al_editar = self._al_editar
        defs = [("|<", self._ir_primero, 28), ("<", self._ir_anterior, 28), (">", self._ir_siguiente, 28),
                (">|", self._ir_ultimo, 28), ("Nuevo", self._hacer_nuevo, 56), ("Borrar", self._hacer_borrar, 64),
                ("Guardar", self._hacer_guardar, 72)]
        for t, cmd, a in defs:
            b = tk.Button(w, text=t, command=cmd)
            self._barra.append((b, a))
        self._lbl = tk.Label(w, text="", anchor="w")
        self._relayout()

    def _relayout(self):
        if self._g is None:
            return
        W, H = int(self._p["ancho"]), int(self._p["alto"])
        con_barra = bool(self._p["barra"])
        hg = H - (self.NAV if con_barra else 0)
        self._g.place(x=0, y=0, width=max(40, W), height=max(40, hg))
        self._g.tamano(max(40, W), max(40, hg))
        x = 0
        for b, a in self._barra:
            if con_barra:
                b.place(x=x, y=H - self.NAV + 2, width=a, height=self.NAV - 2)
            else:
                b.place_forget()
            x += a + 1
        if con_barra and W - x > 40:
            self._lbl.place(x=x + 6, y=H - self.NAV + 3, width=W - x - 6, height=self.NAV - 4)
        else:
            self._lbl.place_forget()

    def _aplicar(self, n):
        if n in ("ancho", "alto", "visible", "x", "y"):
            Control._aplicar(self, n)
            self._relayout()
        elif n == "editable":
            if self._g is not None:
                self._g.editable = bool(self._p["editable"])
        elif n == "barra":
            self._relayout()
        elif n == "fuente":
            cn = self._fuente()
            if cn is not None and cn.abierta:
                self.evento("datos", None)
        else:
            Control._aplicar(self, n)

    # --- del conector a la rejilla
    def evento(self, tipo, origen):
        cn = self._fuente()
        if cn is None or self._g is None:
            return
        g = self._g
        self._modo = True
        try:
            if tipo == "datos":
                g.editable = bool(self._p["editable"])
                g.col_editables = [c for c in cn.columnas if c != cn.clave]
                cols = cn.columnas
                filas = [[f.get(c) for c in cols] for f in cn.filas]
                if cn.es_nuevo:
                    filas.append([None] * len(cols))
                g.poner(cols, filas)
                o = (cn._p["orden"] or "").strip().split(",")[0].strip()
                g.orden_dir = -1 if o.startswith("-") else 1
                o = o.lstrip("-+")
                g.orden_col = cols.index(o) if o in cols else -1
                i = len(filas) - 1 if cn.es_nuevo else cn.indice
                if i >= 0:
                    g.seleccionar(i)
                    g._cuerpo._redesenhar()
            elif tipo == "registro":
                if cn.indice >= 0:
                    g.seleccionar(cn.indice)
            elif tipo == "campo":
                i = len(g.filas) - 1 if cn.es_nuevo else cn.indice
                if 0 <= i < len(g.filas):
                    r = cn.registro
                    for j, c in enumerate(cn.columnas):
                        g.filas[i][j] = basedatos.texto_de_valor(r.get(c))
                    g._cuerpo._redesenhar()
        finally:
            self._modo = False
        self._etiqueta(cn)

    def _etiqueta(self, cn):
        if self._lbl is None:
            return
        if not cn.abierta:
            t = "sin conexión"
        elif cn.es_nuevo:
            t = "registro nuevo"
        else:
            t = "%d de %d" % (cn.posicion + 1 if cn.total else 0, cn.total)
            if cn.paginas > 1:
                t += "  (pág. %d/%d)" % (cn.pagina + 1, cn.paginas)
        if cn.modificado:
            t += " *"
        self._lbl.configure(text=t)

    # --- de la rejilla al conector
    def _al_seleccionar(self, i):
        if self._modo:
            return
        cn = self._fuente()
        if cn is None:
            return
        if cn.es_nuevo and i < len(cn.filas):
            cn.guardar()
            i = max(0, min(i, len(cn.filas) - 1))
        if i < len(cn.filas):
            n = max(1, int(cn._p["tam_pagina"] or 50))
            cn.ir_a(cn.pagina * n + i)
        self._disparar("seleccion")
        self._etiqueta(cn)

    def _al_ordenar(self, j):
        cn = self._fuente()
        if cn is not None and 0 <= j < len(cn.columnas):
            cn.ordenar(cn.columnas[j])

    def _al_editar(self, i, j, texto):
        """Una celda editada: escribe en la base si la rejilla es editable."""
        cn = self._fuente()
        if cn is None or not self._p["editable"] or j >= len(cn.columnas):
            return False
        if i != (len(cn.filas) if cn.es_nuevo else cn.indice):
            return False
        if not cn.poner(cn.columnas[j], texto):
            return False
        ok = cn.guardar()
        self._disparar("cambio")
        self._etiqueta(cn)
        return ok

    def _con(self, f):
        cn = self._fuente()
        if cn is not None and cn.abierta:
            f(cn)
            self._etiqueta(cn)

    def _ir_primero(self):
        self._con(lambda c: c.primero())

    def _ir_anterior(self):
        self._con(lambda c: c.anterior())

    def _ir_siguiente(self):
        self._con(lambda c: c.siguiente())

    def _ir_ultimo(self):
        self._con(lambda c: c.ultimo())

    def _hacer_nuevo(self):
        self._con(lambda c: c.nuevo())

    def _hacer_guardar(self):
        def g(c):
            c.guardar()
            self._disparar("cambio")
        self._con(g)

    def _hacer_borrar(self):
        def b(c):
            if c.es_nuevo:
                c.borrar()
            elif c.indice >= 0 and vps.msgbox("¿Borrar el registro actual?", "Borrar", "si_no", "pregunta") == "si":
                c.borrar()
                self._disparar("cambio")
        self._con(b)

    # --- para el programa
    @property
    def seleccion(self):
        """La fila actual de la página (-1 si no hay)."""
        return self._g.fila if self._g is not None else -1

    def destruir(self):
        Control.destruir(self)


TIPOS = {"ConectorBD": ConectorBD, "RejillaDatos": RejillaDatos}

METODOS = {
    "ConectorBD": [("abrir", "abrir()", "Abre la conexión y carga la primera página; dispara conectado o error"),
                   ("cerrar", "cerrar()", "Cierra la conexión"),
                   ("recargar", "recargar()", "Vuelve a leer la página actual (descarta lo no guardado)"),
                   ("guardar", "guardar()", "Escribe en la base el registro modificado o nuevo"),
                   ("nuevo", "nuevo()", "Empieza un registro nuevo"),
                   ("borrar", "borrar()", "Borra el registro actual"),
                   ("primero", "primero()", "Va al primer registro"),
                   ("anterior", "anterior()", "Va al registro anterior"),
                   ("siguiente", "siguiente()", "Va al registro siguiente"),
                   ("ultimo", "ultimo()", "Va al último registro"),
                   ("ir_a", "ir_a(posicion)", "Va al registro de esa posición (desde 0)"),
                   ("ordenar", "ordenar(campo, descendente=None)", "Ordena por una columna"),
                   ("valor", "valor(campo)", "El valor de un campo del registro actual"),
                   ("poner", "poner(campo, valor)", "Cambia un campo del registro actual (se guarda con guardar())"),
                   ("tablas", "tablas()", "Los nombres de las tablas o colecciones"),
                   ("registro", "registro", "Diccionario con el registro actual"),
                   ("filas", "filas", "Las filas de la página actual (lista de diccionarios)"),
                   ("total", "total", "Cuántas filas cumplen el filtro"),
                   ("posicion", "posicion", "Posición del registro actual (desde 0)"),
                   ("bd", "bd", "El objeto de basedatos, para consultas propias (bd.ejecutar...)")],
    "RejillaDatos": [("seleccion", "seleccion", "La fila actual de la página (-1 si no hay)")],
}


# ------------------------------------------------------------ ayudas para el diseñador

def tablas_de(origen):
    """Los nombres de las tablas de un origen (se conecta un momento); [] si no se puede."""
    try:
        bd = basedatos.conectar(origen)
    except ErrorBD:
        return []
    try:
        return bd.tablas()
    except ErrorBD:
        return []
    finally:
        bd.cerrar()


def columnas_de(origen, tabla):
    try:
        bd = basedatos.conectar(origen)
    except ErrorBD:
        return []
    try:
        return [c["nombre"] for c in bd.esquema(tabla)]
    except ErrorBD:
        return []
    finally:
        bd.cerrar()


def conectores(form):
    """Los conectores de un formulario del diseñador: [(nombre, props)]."""
    return [(c["nombre"], c["props"]) for c in form["controles"] if c["tipo"] == "ConectorBD"]


def editar_propiedad(dis, nombre, o):
    """Editores propios de las propiedades de datos (el diseñador los llama antes que los suyos):
    `origen` (diálogo para elegir un .db o escribir el servidor), `tabla` y `campo` (menú con las
    tablas o columnas), `fuente` (menú con los conectores del formulario). True si lo atendió."""
    if o is dis.form:
        return False
    tipo = o["tipo"]
    if tipo == "ConectorBD" and nombre == "origen":
        nuevo = dialogo_origen(dis, o["props"].get("origen", ""))
        if nuevo is not None:
            dis.aplicar("origen", nuevo)
        return True
    if tipo == "ConectorBD" and nombre == "tabla":
        tablas = tablas_de(o["props"].get("origen", ""))
        if not tablas:
            dis.estado("No se pudo conectar con el origen (o no tiene tablas): escribe el nombre de la tabla", True)
            return False
        m = tk.Menu(dis.raiz, tearoff=0)
        for t in tablas:
            m.add_command(label=t, command=lambda t=t: dis.aplicar("tabla", t))
        m.tk_popup(dis.raiz.winfo_pointerx(), dis.raiz.winfo_pointery())
        return True
    if (tipo in ENLAZABLES or tipo == "RejillaDatos") and nombre == "fuente":
        cs = [n for n, p in conectores(dis.form)]
        if not cs:
            dis.estado("Primero pon un ConectorBD en el formulario", True)
            return True
        m = tk.Menu(dis.raiz, tearoff=0)
        for n in cs:
            m.add_command(label=n, command=lambda n=n: dis.aplicar("fuente", n))
        m.tk_popup(dis.raiz.winfo_pointerx(), dis.raiz.winfo_pointery())
        return True
    if tipo in ENLAZABLES and nombre == "campo":
        cols = []
        for n, p in conectores(dis.form):
            if n == o["props"].get("fuente"):
                cols = columnas_de(p.get("origen", ""), p.get("tabla", ""))
        if not cols:
            dis.estado("Elige antes la fuente (un ConectorBD con origen y tabla); o escribe el campo", True)
            return False
        m = tk.Menu(dis.raiz, tearoff=0)
        for c in cols:
            m.add_command(label=c, command=lambda c=c: dis.aplicar("campo", c))
        m.tk_popup(dis.raiz.winfo_pointerx(), dis.raiz.winfo_pointery())
        return True
    return False


def dialogo_origen(dis, actual):
    """El diálogo de `origen`: un fichero SQLite (.db) o un servidor lite-monkey. Devuelve la
    cadena de conexión o None si se cancela."""
    d = tk.Toplevel(dis.raiz, _dialogo=True)
    d.title("Origen de datos")
    d.transient(dis.raiz)
    d.resizable(False, False)
    tipo = tk.StringVar(value="litemonkey" if actual.startswith("litemonkey") else "sqlite")
    tk.Label(d, text="Tipo de base de datos:", anchor="w").pack(fill="x", padx=8, pady=(8, 0))
    fila = tk.Frame(d)
    fila.pack(fill="x", padx=8)
    tk.Radiobutton(fila, text="SQLite (fichero .db)", variable=tipo, value="sqlite").pack(side="left")
    tk.Radiobutton(fila, text="lite-monkey (servidor)", variable=tipo, value="litemonkey").pack(side="left", padx=8)
    tk.Label(d, text="Fichero .db, o servidor (host:puerto/base):", anchor="w").pack(fill="x", padx=8, pady=(8, 0))
    e = tk.Entry(d, width=48)
    e.pack(padx=8, pady=2)
    ini = actual
    if actual.startswith("sqlite:"):
        ini = actual[7:]
    elif actual.startswith("litemonkey://"):
        ini = actual[13:]
    elif not actual:
        ini = ""
    e.insert(0, ini or "")
    res = []

    def elegir():
        r = vps.dialogo_abrir("Elegir una base SQLite", None, [("Bases SQLite", "*.db *.sqlite *.sqlite3"), ("Todos", "*")])
        if r:
            tipo.set("sqlite")
            e.delete(0, "end")
            e.insert(0, r)

    def ok():
        t = e.get().strip()
        if t:
            if tipo.get() == "sqlite":
                res.append(t if t.startswith("sqlite:") else "sqlite:" + t)
            else:
                res.append(t if t.startswith("litemonkey://") else "litemonkey://" + t)
        d.destroy()
    fb = tk.Frame(d, pady=8)
    fb.pack()
    tk.Button(fb, text="Elegir fichero...", width=16, command=elegir).pack(side="left", padx=4)
    tk.Button(fb, text="Aceptar", width=10, command=ok).pack(side="left", padx=4)
    tk.Button(fb, text="Cancelar", width=10, command=d.destroy).pack(side="left", padx=4)
    e.focus_set()
    e.bind("<Return>", lambda ev: ok())
    d.grab_set()
    dis.raiz.wait_window(d)
    return res[0] if res else None
