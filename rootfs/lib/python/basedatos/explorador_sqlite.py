# SPDX-License-Identifier: GPL-2.0-or-later
# Explorador SQLite (mejora 131, D333): la aplicación relacional, al estilo de DB Browser for SQLite.
#
#     python /lib/python/basedatos/explorador_sqlite.py [fichero.db ...]
#
# Árbol de tablas, vistas, índices y disparadores a la izquierda y, a la derecha, las pestañas Datos (rejilla
# editable con filtro por columna y NULL distinguido del texto vacío), Estructura (asistente de columnas) y SQL
# (con autocompletado, historial y EXPLAIN QUERY PLAN). Los cambios se escriben o se deshacen a mano (botones
# «Escribir cambios» y «Deshacer cambios») salvo que esté marcada la escritura automática. F12 imprime las
# posiciones de los componentes para las pruebas automáticas.
import os
import sys
import tkinter as tk
from tkinter import messagebox, filedialog
import basedatos
from basedatos import ErrorBD
from basedatos._cuadricula import Cuadricula
from basedatos._seguridad import PALABRAS, sentencias, peligrosa
from basedatos._comun import VentanaBD, Cancelado, ahora_ms, leer_config, guardar_config, confirmar_peligro
from basedatos._idioma import _, N_
from basedatos import _filtros, _csv

CONFIG = "/home/.config/exploradorsqlite.json"
TAM_PAGINA = 50
LIM_CONSULTA = 500
GRUPOS = (("table", N_("Tablas")), ("view", N_("Vistas")), ("index", N_("Índices")), ("trigger", N_("Disparadores")))
SOLO_LECTURA = ("select", "pragma", "explain", "with", "values")

# ------------------------------------------------------------ resaltado de SQL

COL_NEGRO, COL_PALABRA, COL_CADENA, COL_NUMERO, COL_COMENTARIO = 0x000000, 0x0000C0, 0xA00000, 0x008080, 0x008000


def realzar_sql(linea):
    """Un color por carácter de una línea de SQL: palabras clave, cadenas, números y comentarios."""
    n = len(linea)
    cols = [COL_NEGRO] * n
    i = 0
    while i < n:
        c = linea[i]
        if c in ("'", '"'):
            j = i + 1
            while j < n and linea[j] != c:
                j += 1
            j = min(n, j + 1)
            for k in range(i, j):
                cols[k] = COL_CADENA
            i = j
        elif linea[i:i + 2] == "--":
            for k in range(i, n):
                cols[k] = COL_COMENTARIO
            break
        elif c.isalpha() or c == "_":
            j = i
            while j < n and (linea[j].isalpha() or linea[j].isdigit() or linea[j] == "_"):
                j += 1
            if linea[i:j].lower() in PALABRAS:
                for k in range(i, j):
                    cols[k] = COL_PALABRA
            i = j
        elif c.isdigit():
            j = i
            while j < n and (linea[j].isdigit() or linea[j] == "."):
                j += 1
            for k in range(i, j):
                cols[k] = COL_NUMERO
            i = j
        else:
            i += 1
    return cols


def candidatos(prefijo, nombres):
    """Las palabras que completan `prefijo` (sin distinguir mayúsculas): las de `nombres` (tablas y columnas) y
    las palabras clave de SQL, sin repetir, con los nombres de la base primero."""
    p = prefijo.lower()
    if not p:
        return []
    r, vistos = [], set()
    for n in list(nombres) + [w.upper() for w in PALABRAS]:
        if n.lower().startswith(p) and n.lower() != p and n.lower() not in vistos:
            vistos.add(n.lower())
            r.append(n)
    return r


def etiqueta_de(ruta):
    return ruta.split("/")[-1] or ruta


# ------------------------------------------------------------ el explorador

class ExploradorSQLite(VentanaBD):
    TITULO = "Explorador SQLite"
    PAGINAS = ("Datos", "Estructura", "SQL")

    def __init__(self, raiz):
        self.cfg = leer_config(CONFIG, {"historial": [], "recientes": []})
        self.bd = None
        self.ruta = ""
        self.etq = ""
        self.cur = None              # nombre de la tabla o vista elegida
        self.es_vista = False
        self.exp_tabla = {}          # tabla -> ¿desplegada con sus columnas?
        self.exp_grupo = {}          # tipo -> ¿plegado?
        self.pagina = 0
        self.total = 0
        self.orden = ""
        self.filtros = []            # [(texto, condición)] del filtro por columna
        self.claves = []
        self.historial = list(self.cfg.get("historial", []))[-30:]
        self.res_cols, self.res_filas, self.res_nulos = [], [], set()
        VentanaBD.__init__(self, raiz)
        self._refrescar_arbol()

    # ---------------------------------------------------------- menús y barra
    def _menus(self):
        m = tk.Menu(self.raiz)
        a = tk.Menu(m, tearoff=0)
        a.add_command(label=_("Abrir base SQLite..."), command=self.abrir_dialogo)
        a.add_command(label=_("Nueva base..."), command=self.nueva_base)
        a.add_command(label=_("Cerrar base"), command=self.cerrar_base)
        a.add_separator()
        a.add_command(label=_("Importar CSV..."), command=self.importar_csv)
        a.add_command(label=_("Exportar tabla a CSV..."), command=self.exportar_csv)
        a.add_command(label=_("Exportar base a SQL (volcado)..."), command=self.exportar_volcado)
        a.add_command(label=_("Ejecutar archivo SQL..."), command=self.ejecutar_archivo)
        a.add_separator()
        a.add_command(label=_("Salir"), command=self.salir)
        m.add_cascade(label=_("Archivo"), menu=a)
        t = tk.Menu(m, tearoff=0)
        t.add_command(label=_("Nueva tabla..."), command=self.nueva_tabla)
        t.add_command(label=_("Modificar tabla..."), command=self.modificar_tabla)
        t.add_command(label=_("Borrar tabla"), command=self.borrar_tabla)
        t.add_separator()
        t.add_command(label=_("Actualizar"), command=self.actualizar)
        m.add_cascade(label=_("Tabla"), menu=t)
        h = tk.Menu(m, tearoff=0)
        h.add_command(label=_("Comprobar integridad"), command=self.comprobar_integridad)
        h.add_command(label=_("Compactar (VACUUM)"), command=self.compactar)
        m.add_cascade(label=_("Herramientas"), menu=h)
        y = tk.Menu(m, tearoff=0)
        y.add_command(label=_("Acerca de..."), command=lambda: messagebox.showinfo(
            _("Explorador SQLite"), _("Explorador SQLite") + "\n" + _("Bases relacionales (mejora 131)")))
        m.add_cascade(label=_("Ayuda"), menu=y)
        self.raiz.config(menu=m)

    def TITULO_ARBOL(self):
        return _("Base de datos")

    def _paginas(self):
        b = self.barra_herr
        self.b_escribir = tk.Button(b, text=_("Escribir cambios"), command=self.escribir_cambios)
        self.b_escribir.pack(side="left")
        self.b_deshacer = tk.Button(b, text=_("Deshacer cambios"), command=self.deshacer_cambios)
        self.b_deshacer.pack(side="left", padx=4)
        self.v_auto = tk.IntVar(value=1)
        tk.Checkbutton(b, text=_("Escritura automática"), variable=self.v_auto, command=self._cambio_auto).pack(side="left", padx=8)
        self.l_pend = tk.Label(b, text="", anchor="w")
        self.l_pend.pack(side="left")
        self._pag_datos()
        self._pag_estructura()
        self._pag_sql()
        self._pendiente_ui()
        self.raiz.bind("<F5>", lambda e: self.ejecutar())

    def al_cambiar_pestana(self, n):
        if n == "SQL":
            self._preparar_sql()

    # ---------------------------------------------------------- transacciones
    @property
    def auto(self):
        return bool(self.v_auto.get())

    def _pendiente_ui(self):
        pend = self.bd is not None and self.bd.en_transaccion
        self.b_escribir.config(state="normal" if pend else "disabled")
        self.b_deshacer.config(state="normal" if pend else "disabled")
        self.l_pend.config(text=_("Hay cambios sin escribir") if pend else "")

    def _asegurar_tx(self):
        """Con la escritura automática desactivada, abre la transacción antes del primer cambio."""
        if self.bd is not None and not self.auto and not self.bd.en_transaccion:
            self.bd.empezar()

    def _cambio_auto(self):
        if self.auto and self.bd is not None and self.bd.en_transaccion:
            self.bd.confirmar()
            self.msg(_("Cambios escritos"))
        self._pendiente_ui()

    def escribir_cambios(self):
        if self.bd is not None and self.bd.en_transaccion:
            self.bd.confirmar()
            self.msg(_("Cambios escritos en la base"))
        self._pendiente_ui()

    def deshacer_cambios(self):
        if self.bd is not None and self.bd.en_transaccion:
            self.bd.revertir()
            self.msg(_("Cambios deshechos"))
            self.actualizar()
        self._pendiente_ui()

    def _resolver_pendientes(self):
        """Antes de cerrar o cambiar de base: ¿escribir lo pendiente? Devuelve False si el usuario cancela."""
        if self.bd is None or not self.bd.en_transaccion:
            return True
        r = messagebox.askyesnocancel(_("Cambios sin escribir"), _("Hay cambios sin escribir.\n¿Quieres escribirlos en la base?"))
        if r is None:
            return False
        if r:
            self.bd.confirmar()
        else:
            self.bd.revertir()
        return True

    # ---------------------------------------------------------- pestaña Datos
    def _pag_datos(self):
        f = self.pags["Datos"] = tk.Frame(self.zona)
        fila = tk.Frame(f)
        fila.pack(fill="x", padx=4, pady=(4, 0))
        tk.Label(fila, text=_("Filtro:")).pack(side="left")
        self.e_filtro = tk.Entry(fila)
        self.e_filtro.pack(side="left", fill="x", expand=True, padx=4)
        self.e_filtro.bind("<Return>", lambda e: self.aplicar_filtro())
        self.cajas["filtro"] = self.e_filtro
        tk.Button(fila, text=_("Añadir filtro"), command=self.aplicar_filtro).pack(side="left")
        tk.Button(fila, text=_("Quitar filtros"), command=self.quitar_filtro).pack(side="left", padx=(4, 0))
        self.l_pista = tk.Label(f, text="", anchor="w")
        self.l_pista.pack(fill="x", padx=4)
        self.l_filtros = tk.Label(f, text="", anchor="w")
        self.l_filtros.pack(fill="x", padx=4)
        baj = tk.Frame(f)
        baj.pack(side="bottom", fill="x", padx=4, pady=4)
        tk.Button(baj, text=_("Borrar filtradas"), command=self.borrar_filtradas).pack(side="right")
        tk.Button(baj, text=_("Borrar fila"), command=self.borrar_fila).pack(side="right", padx=4)
        tk.Button(baj, text=_("Fila nueva..."), command=self.fila_nueva).pack(side="right")
        for t, c in (("|<", self.pag_primera), ("<", self.pag_anterior), (">", self.pag_siguiente), (">|", self.pag_ultima)):
            tk.Button(baj, text=t, width=3, command=c).pack(side="left")
        self.l_pag = tk.Label(baj, text="", anchor="w")
        self.l_pag.pack(side="left", padx=8)
        self.rej = Cuadricula(f, ancho=520, alto=300)
        self.rej.pack(fill="both", expand=True, padx=4)
        self.rej.editable = True
        self.rej.al_ordenar = self._ordenar
        self.rej.al_editar = self._celda_editada
        self.rejillas["rejilla"] = self.rej

    def _filtro_actual(self):
        return _filtros.combinar([c for _t, c in self.filtros])

    def mostrar_datos(self, pagina=None):
        t = self.cur
        if self.bd is None or not t:
            self.rej.poner([], [])
            self.l_pag.config(text="")
            return
        bd = self.bd
        if pagina is not None:
            self.pagina = pagina
        self.l_pista.config(text=_("Filtro por columna: «columna:valor» o el valor para la columna elegida") + " (" + _filtros.AYUDA + ")")
        self.l_filtros.config(text=(_("Filtros activos:") + " " + "  AND  ".join(t2 for t2, _c in self.filtros)) if self.filtros else "")
        try:
            f = self._filtro_actual()
            self.total = bd.contar(t, f)
            paginas = max(1, (self.total + TAM_PAGINA - 1) // TAM_PAGINA)
            self.pagina = max(0, min(self.pagina, paginas - 1))
            filas = bd.consultar(t, f, [self.orden] if self.orden else None, TAM_PAGINA, self.pagina * TAM_PAGINA)
            cols = list(bd.columnas(t))
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            messagebox.showerror(_("Datos"), str(e))
            return
        clave = bd.clave(t)
        self.claves = [fila.get(clave) for fila in filas] if clave else []
        self.rej.editable = not self.es_vista
        self.rej.col_editables = [x for x in cols if x != clave]
        nulos, datos = set(), []
        for i, fila in enumerate(filas):
            r = []
            for j, x in enumerate(cols):
                v = fila.get(x)
                if v is None:
                    nulos.add((i, j))
                    r.append("NULL")
                else:
                    r.append(basedatos.texto_de_valor(v))
            datos.append(r)
        self.rej.poner(cols, datos, nulos=nulos)
        o = self.orden.lstrip("-+")
        self.rej.orden_col = cols.index(o) if o in cols else -1
        self.rej.orden_dir = -1 if self.orden.startswith("-") else 1
        ini = self.pagina * TAM_PAGINA
        self.l_pag.config(text="%d-%d de %d (pág. %d/%d)" % (ini + 1 if filas else 0, ini + len(filas), self.total,
                                                                  self.pagina + 1, paginas))
        self.msg("%s: %d filas" % (t, self.total))

    def pag_primera(self):
        self.mostrar_datos(0)

    def pag_anterior(self):
        self.mostrar_datos(self.pagina - 1)

    def pag_siguiente(self):
        self.mostrar_datos(self.pagina + 1)

    def pag_ultima(self):
        self.mostrar_datos(10 ** 9)

    def aplicar_filtro(self):
        """Añade el filtro escrito: «columna:valor» o solo el valor (para la columna elegida en la rejilla)."""
        if self.bd is None or not self.cur:
            return
        texto = self.e_filtro.get().strip()
        if not texto:
            return
        cols = list(self.rej.columnas)
        col, val = None, texto
        if ":" in texto:
            a, _s, b = texto.partition(":")
            if a.strip() in cols:
                col, val = a.strip(), b
        if col is None:
            if not cols:
                return
            col = cols[max(0, min(self.rej.col, len(cols) - 1))]
        tipo = None
        for x in self.bd.esquema(self.cur):
            if x["nombre"] == col:
                tipo = x["tipo"]
        try:
            cond = _filtros.condicion(col, val, tipo)
        except ErrorBD as e:
            messagebox.showerror(_("Filtro"), str(e))
            return
        if cond is None:
            return
        self.filtros.append((_filtros.describir(col, val), cond))
        self.e_filtro.delete(0, "end")
        self.mostrar_datos(0)

    def quitar_filtro(self):
        self.e_filtro.delete(0, "end")
        self.filtros = []
        self.mostrar_datos(0)

    def _ordenar(self, j):
        col = self.rej.columnas[j]
        self.orden = col if self.orden != col else "-" + col
        self.mostrar_datos(0)

    def _tipo_de(self, col):
        for x in self.bd.esquema(self.cur):
            if x["nombre"] == col:
                return x["tipo"]
        return None

    def _valor(self, texto, tipo):
        """El texto de una celda como valor: «NULL» es NULL; lo demás, según el tipo de la columna."""
        if texto == "NULL":
            return None
        return basedatos.valor_desde_texto(texto, tipo)

    def _celda_editada(self, i, j, texto):
        """El usuario cambió una celda: se escribe en la base con parámetros enlazados."""
        t = self.cur
        col = self.rej.columnas[j]
        try:
            valor = self._valor(texto, self._tipo_de(col))
            self._asegurar_tx()
            n = self.bd.actualizar(t, {col: valor}, clave=self.claves[i])
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            messagebox.showerror(_("No se pudo guardar"), str(e))
            return False
        self._pendiente_ui()
        self.msg("%s: fila actualizada (%s = %s)" % (t, col, texto) if n else "No se encontró la fila")
        if valor is None:
            self.rej.nulos.add((i, j))
        return n > 0

    def fila_nueva(self):
        t = self.cur
        if self.bd is None or not t or self.es_vista:
            return
        bd = self.bd
        try:
            esq = bd.esquema(t)
        except ErrorBD as e:
            messagebox.showerror(_("Fila nueva"), str(e))
            return
        cols = esq[:12]
        d = self.dialogo(_("Fila nueva en %s") % t)
        ents = {}
        for k, x in enumerate(cols):
            tk.Label(d, text="%s (%s)" % (x["nombre"], x["tipo"] or "?"), anchor="w").grid(row=k, column=0, sticky="w", padx=8, pady=2)
            e = tk.Entry(d, width=30)
            e.grid(row=k, column=1, padx=8, pady=2)
            ents[x["nombre"]] = e
        tk.Label(d, text=_("Vacío = valor por defecto; NULL = nulo"), anchor="w").grid(row=len(cols), column=0, columnspan=2, sticky="w", padx=8)
        ok = []

        def aceptar():
            ok.append(True)
            d.destroy()
        fb = tk.Frame(d, pady=6)
        fb.grid(row=len(cols) + 1, column=0, columnspan=2)
        tk.Button(fb, text=_("Insertar"), width=10, command=aceptar).pack(side="left", padx=4)
        tk.Button(fb, text=_("Cancelar"), width=10, command=d.destroy).pack(side="left", padx=4)
        self.esperar_dialogo(d, list(ents.values())[0] if ents else None)
        if not ok:
            return
        fila = {}
        try:
            for x in cols:
                txt = ents[x["nombre"]].get()
                if txt != "":
                    fila[x["nombre"]] = self._valor(txt, x["tipo"])
            self._asegurar_tx()
            bd.insertar(t, fila)
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            messagebox.showerror(_("No se pudo insertar"), str(e))
            return
        self._pendiente_ui()
        self.msg("%s: fila insertada" % t)
        self.pag_ultima()

    def borrar_fila(self):
        t = self.cur
        i = self.rej.fila
        if self.bd is None or not t or self.es_vista or i < 0 or i >= len(self.claves):
            self.msg(_("Elige primero una fila"))
            return
        if not confirmar_peligro(_("Borrar fila"), _("¿Borrar la fila %d de %s?") % (self.pagina * TAM_PAGINA + i + 1, t)):
            return
        try:
            self._asegurar_tx()
            self.bd.borrar(t, clave=self.claves[i])
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo borrar"), str(e))
            return
        self._pendiente_ui()
        self.msg("%s: fila borrada" % t)
        self.mostrar_datos()

    def borrar_filtradas(self):
        """Borra todas las filas que casan con los filtros, enseñando antes cuántas son (D304)."""
        t = self.cur
        if self.bd is None or not t or self.es_vista:
            return
        f = self._filtro_actual()
        if not f:
            messagebox.showinfo(_("Borrar filtradas"), _("Añade antes un filtro: así se sabe qué filas se borran.\nPara vaciar toda la tabla usa la pestaña SQL (DELETE FROM)."))
            return
        try:
            n = self.bd.contar(t, f)
        except ErrorBD as e:
            messagebox.showerror(_("Borrar filtradas"), str(e))
            return
        if n == 0:
            self.msg(_("Ninguna fila casa con el filtro"))
            return
        if not confirmar_peligro(_("Borrar filtradas"), _("Se van a borrar %d filas de %s.\nNo se puede deshacer salvo con «Deshacer cambios».") % (n, t)):
            return
        try:
            self._asegurar_tx()
            n = self.bd.borrar(t, f)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo borrar"), str(e))
            return
        self._pendiente_ui()
        self.msg("%s: %d filas borradas" % (t, n))
        self.mostrar_datos(0)

    # ---------------------------------------------------------- pestaña Estructura
    def _pag_estructura(self):
        f = self.pags["Estructura"] = tk.Frame(self.zona)
        fila = tk.Frame(f)
        fila.pack(fill="x", padx=4, pady=(4, 0))
        tk.Button(fila, text=_("Nueva tabla..."), command=self.nueva_tabla).pack(side="left")
        tk.Button(fila, text=_("Modificar tabla..."), command=self.modificar_tabla).pack(side="left", padx=4)
        tk.Button(fila, text=_("Nuevo índice..."), command=self.nuevo_indice).pack(side="left")
        tk.Button(fila, text=_("Borrar índice"), command=self.borrar_indice).pack(side="left", padx=4)
        self.l_estr = tk.Label(f, text="", anchor="w")
        self.l_estr.pack(fill="x", padx=4, pady=(4, 0))
        self.txt_sql = tk.Text(f, height=4, width=60, wrap="none")
        self.txt_sql.pack(fill="x", padx=4)
        self.txt_sql._realce = realzar_sql
        self.rej_cols = Cuadricula(f, ancho=500, alto=170)
        self.rej_cols.pack(fill="both", expand=True, padx=4, pady=(4, 0))
        self.rejillas["rejilla_cols"] = self.rej_cols
        tk.Label(f, text=_("Índices"), anchor="w").pack(fill="x", padx=4, pady=(4, 0))
        self.rej_idx = Cuadricula(f, ancho=500, alto=100)
        self.rej_idx.pack(fill="x", padx=4, pady=(0, 4))
        self.rejillas["rejilla_idx"] = self.rej_idx

    def _poner_sql(self, texto):
        self.txt_sql.delete("1.0", "end")
        self.txt_sql.insert("1.0", texto)

    def mostrar_estructura(self):
        t = self.cur
        if self.bd is None or not t:
            self.l_estr.config(text="")
            self.rej_cols.poner([], [])
            self.rej_idx.poner([], [])
            self._poner_sql("")
            return
        bd = self.bd
        try:
            d = bd.definicion(t)
            idx = bd.indices(t)
            sql = bd.sql_de(t)
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            return
        self.l_estr.config(text="%s: %d columnas%s" % (t, len(d), " (vista)" if self.es_vista else ""))
        si = _("sí")
        self.rej_cols.poner([_("Columna"), _("Tipo"), _("Clave"), _("Obligatoria"), _("Por defecto"), _("Único"), _("Clave foránea")],
                            [[x["nombre"], x["tipo"], si if x["pk"] else "", si if x["obligatorio"] else "",
                              "" if x["defecto"] is None else x["defecto"], si if x["unico"] else "", x["fk"]] for x in d])
        self.rej_idx.poner([_("Índice"), _("Único"), _("Columnas")],
                           [[x["nombre"], si if x["unico"] else "", ", ".join(x["columnas"])] for x in idx])
        self._poner_sql(sql)

    # ---------------------------------------------------------- asistente de columnas
    def asistente_tabla(self, titulo, nombre, defs, es_nueva):
        """El asistente de columnas: nombre, tipo, PK, NOT NULL, UNIQUE, DEFAULT y FK por fila. Devuelve
        (nombre, definiciones, origen) o None si se cancela. `origen` dice de qué columna vieja viene cada una
        (para conservar los datos al renombrar)."""
        n = max(8, min(14, len(defs) + 3))
        d = self.dialogo(titulo)
        tk.Label(d, text=_("Nombre de la tabla:"), anchor="w").grid(row=0, column=0, sticky="w", padx=8, pady=4)
        en = tk.Entry(d, width=20)
        en.grid(row=0, column=1, columnspan=3, sticky="w", padx=4)
        en.insert(0, nombre)
        cab = (_("Columna"), _("Tipo"), "PK", "NOT NULL", "UNIQUE", _("Por defecto"), _("Clave foránea (tabla(col))"))
        for k, c in enumerate(cab):
            tk.Label(d, text=c, anchor="w").grid(row=1, column=k, sticky="w", padx=4)
        filas = []
        for k in range(n):
            x = defs[k] if k < len(defs) else None
            e = tk.Entry(d, width=16)
            e.grid(row=2 + k, column=0, padx=4, pady=1)
            tv = tk.StringVar(value=(x["tipo"] if x and x["tipo"] in basedatos.TIPOS_SQL else "TEXT"))
            if x is None and k == 0 and es_nueva:
                e.insert(0, "id")
                tv.set("INTEGER")
            elif x is not None:
                e.insert(0, x["nombre"])
            tk.OptionMenu(d, tv, *basedatos.TIPOS_SQL).grid(row=2 + k, column=1, padx=2)
            vpk = tk.IntVar(value=1 if (x and x["pk"]) or (x is None and k == 0 and es_nueva) else 0)
            vnn = tk.IntVar(value=1 if x and x["obligatorio"] and not x["pk"] else 0)
            vuq = tk.IntVar(value=1 if x and x["unico"] else 0)
            tk.Checkbutton(d, variable=vpk).grid(row=2 + k, column=2)
            tk.Checkbutton(d, variable=vnn).grid(row=2 + k, column=3)
            tk.Checkbutton(d, variable=vuq).grid(row=2 + k, column=4)
            ed = tk.Entry(d, width=10)
            ed.grid(row=2 + k, column=5, padx=4)
            if x and x["defecto"] not in (None, ""):
                ed.insert(0, str(x["defecto"]).strip("'") if str(x["defecto"]).startswith("'") else str(x["defecto"]))
            ef = tk.Entry(d, width=16)
            ef.grid(row=2 + k, column=6, padx=4)
            if x and x["fk"]:
                ef.insert(0, x["fk"])
            filas.append((e, tv, vpk, vnn, vuq, ed, ef, x["nombre"] if x else None))
        res = []

        def leer():
            nuevas, origen = [], {}
            for e, tv, vpk, vnn, vuq, ed, ef, viejo in filas:
                nm = e.get().strip()
                if not nm:
                    continue
                nuevas.append({"nombre": nm, "tipo": tv.get(), "pk": bool(vpk.get()), "obligatorio": bool(vnn.get()),
                               "unico": bool(vuq.get()), "defecto": ed.get().strip(), "fk": ef.get().strip()})
                if viejo is not None:
                    origen[nm] = viejo
            return en.get().strip(), nuevas, origen

        def aceptar():
            res.append(leer())
            d.destroy()

        def ver_sql():
            try:
                nm, nuevas, _o = leer()
                messagebox.showinfo(_("SQL de la tabla"), basedatos.SQLite.sql_crear(nm or "tabla", nuevas))
            except ErrorBD as e:
                messagebox.showerror(_("SQL de la tabla"), str(e))
        fb = tk.Frame(d, pady=6)
        fb.grid(row=3 + n, column=0, columnspan=7)
        tk.Button(fb, text=_("Crear") if es_nueva else _("Aplicar"), width=10, command=aceptar).pack(side="left", padx=4)
        tk.Button(fb, text=_("Ver SQL"), width=10, command=ver_sql).pack(side="left", padx=4)
        tk.Button(fb, text=_("Cancelar"), width=10, command=d.destroy).pack(side="left", padx=4)
        self.esperar_dialogo(d, en if es_nueva else filas[0][0])
        return res[0] if res else None

    def nueva_tabla(self):
        if self.bd is None:
            messagebox.showinfo(_("Nueva tabla"), _("Abre primero una base."))
            return
        r = self.asistente_tabla(_("Nueva tabla"), "", [], True)
        if r is None:
            return
        nombre, defs, _o = r
        try:
            if not nombre:
                raise ErrorBD(_("falta el nombre"))
            self._asegurar_tx()
            self.bd.crear_tabla_def(nombre, defs)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo crear"), str(e))
            return
        self._pendiente_ui()
        self.msg("Creada %s" % nombre)
        self.cur = nombre
        self.es_vista = False
        self._refrescar_arbol()
        self.elegir_tabla(nombre)

    def modificar_tabla(self):
        if self.bd is None or not self.cur or self.es_vista:
            messagebox.showinfo(_("Modificar tabla"), _("Elige primero una tabla."))
            return
        t = self.cur
        try:
            defs = self.bd.definicion(t)
        except ErrorBD as e:
            messagebox.showerror(_("Modificar tabla"), str(e))
            return
        r = self.asistente_tabla(_("Modificar tabla"), t, defs, False)
        if r is None:
            return
        nombre, nuevas, origen = r
        try:
            if not nuevas:
                raise ErrorBD(_("una tabla necesita al menos una columna"))
            self._asegurar_tx()
            self.bd.reconstruir(t, nuevas, origen)
            if nombre and nombre != t:
                self.bd.ejecutar('ALTER TABLE "%s" RENAME TO "%s"' % (t.replace('"', '""'), nombre.replace('"', '""')))
                t = nombre
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo modificar"), str(e))
            return
        self._pendiente_ui()
        self.msg("Modificada %s" % t)
        self.cur = t
        self._refrescar_arbol()
        self.elegir_tabla(t)

    def nuevo_indice(self):
        if self.bd is None or not self.cur or self.es_vista:
            messagebox.showinfo(_("Nuevo índice"), _("Elige primero una tabla."))
            return
        t = self.cur
        d = self.dialogo(_("Nuevo índice en %s") % t)
        tk.Label(d, text=_("Nombre:"), anchor="w").grid(row=0, column=0, sticky="w", padx=8, pady=4)
        en = tk.Entry(d, width=24)
        en.grid(row=0, column=1, padx=8)
        en.insert(0, "idx_%s_" % t)
        tk.Label(d, text=_("Columnas (separadas por comas):"), anchor="w").grid(row=1, column=0, sticky="w", padx=8, pady=4)
        ec = tk.Entry(d, width=24)
        ec.grid(row=1, column=1, padx=8)
        vu = tk.IntVar(value=0)
        tk.Checkbutton(d, text=_("Único"), variable=vu).grid(row=2, column=1, sticky="w", padx=8)
        ok = []

        def aceptar():
            ok.append((en.get().strip(), [c.strip() for c in ec.get().split(",") if c.strip()], bool(vu.get())))
            d.destroy()
        fb = tk.Frame(d, pady=6)
        fb.grid(row=3, column=0, columnspan=2)
        tk.Button(fb, text=_("Crear"), width=10, command=aceptar).pack(side="left", padx=4)
        tk.Button(fb, text=_("Cancelar"), width=10, command=d.destroy).pack(side="left", padx=4)
        self.esperar_dialogo(d, ec)
        if not ok:
            return
        try:
            nombre, cols, unico = ok[0]
            if not nombre:
                raise ErrorBD(_("falta el nombre"))
            self._asegurar_tx()
            self.bd.crear_indice(t, nombre, cols, unico)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo crear"), str(e))
            return
        self._pendiente_ui()
        self.msg("Creado el índice %s" % nombre)
        self._refrescar_arbol()
        self.mostrar_estructura()

    def borrar_indice(self, nombre=None):
        if self.bd is None:
            return
        if nombre is None:
            i = self.rej_idx.fila
            if i < 0 or i >= len(self.rej_idx.filas):
                messagebox.showinfo(_("Borrar índice"), _("Elige primero un índice en la lista de índices."))
                return
            nombre = self.rej_idx.filas[i][0]
        if nombre.startswith("sqlite_"):
            messagebox.showinfo(_("Borrar índice"), _("Ese índice lo crea SQLite y no se puede borrar."))
            return
        if not confirmar_peligro(_("Borrar índice"), _("¿Borrar el índice «%s»?") % nombre):
            return
        try:
            self._asegurar_tx()
            self.bd.borrar_indice(nombre)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo borrar"), str(e))
            return
        self._pendiente_ui()
        self.msg("Borrado el índice %s" % nombre)
        self._refrescar_arbol()
        self.mostrar_estructura()

    # ---------------------------------------------------------- pestaña SQL
    def _pag_sql(self):
        f = self.pags["SQL"] = tk.Frame(self.zona)
        self.l_cons = tk.Label(f, text=_("Consulta SQL (F5 para ejecutar):"), anchor="w")
        self.l_cons.pack(fill="x", padx=4, pady=(4, 0))
        self.txt = tk.Text(f, height=7, width=60, wrap="none")
        self.txt.pack(fill="x", padx=4)
        self.txt._realce = realzar_sql          # el Text del sistema pide a esta función un color por carácter
        self.cajas["sql"] = self.txt
        self.txt.bind("<Control-space>", lambda e: (self.autocompletar(), "break")[1])
        fila = tk.Frame(f)
        fila.pack(fill="x", padx=4, pady=4)
        tk.Button(fila, text=_("Ejecutar (F5)"), command=self.ejecutar).pack(side="left")
        tk.Button(fila, text=_("Plan"), command=self.plan).pack(side="left", padx=4)
        tk.Button(fila, text=_("Autocompletar"), command=self.autocompletar).pack(side="left")
        self.b_hist = tk.Button(fila, text=_("Historial"), command=self._menu_historial)
        self.b_hist.pack(side="left", padx=4)
        tk.Button(fila, text=_("Limpiar"), command=lambda: self.poner_consulta("")).pack(side="left")
        tk.Button(fila, text=_("Exportar resultado..."), command=self.exportar_resultado).pack(side="right")
        self.l_res = tk.Label(f, text="", anchor="w")
        self.l_res.pack(side="bottom", fill="x", padx=4, pady=2)
        self.rej_res = Cuadricula(f, ancho=500, alto=180)
        self.rej_res.pack(fill="both", expand=True, padx=4)
        self.rejillas["rejilla_res"] = self.rej_res

    def poner_consulta(self, t):
        self.txt.delete("1.0", "end")
        self.txt.insert("1.0", t)

    def texto_consulta(self):
        return self.txt.get("1.0", "end-1c")

    def _preparar_sql(self):
        if self.bd is not None and self.cur and not self.texto_consulta().strip():
            self.poner_consulta('SELECT * FROM "%s" LIMIT 100;' % self.cur.replace('"', '""'))

    def _nombres_completables(self):
        if self.bd is None:
            return []
        nombres = []
        try:
            for t in self.bd.tablas():
                nombres.append(t)
                for c in self.bd.esquema(t):
                    if c["nombre"] not in nombres:
                        nombres.append(c["nombre"])
        except ErrorBD:
            pass
        return nombres

    def autocompletar(self):
        """Completa la palabra que hay antes del cursor con tablas, columnas y palabras de SQL: si solo hay una,
        la pone; si hay varias, ofrece un menú."""
        ln, col = [int(x) for x in self.txt.index("insert").split(".")]
        antes = self.txt.get("%d.0" % ln, "%d.%d" % (ln, col))
        k = len(antes)
        while k > 0 and (antes[k - 1].isalpha() or antes[k - 1].isdigit() or antes[k - 1] == "_"):
            k -= 1
        pref = antes[k:]
        cand = candidatos(pref, self._nombres_completables())
        if not cand:
            self.msg(_("Sin sugerencias para «%s»") % pref)
            return

        def poner(palabra):
            self.txt.delete("%d.%d" % (ln, col - len(pref)), "%d.%d" % (ln, col))
            self.txt.insert("%d.%d" % (ln, col - len(pref)), palabra)
            self.txt.focus_set()
        if len(cand) == 1:
            poner(cand[0])
            return
        m = tk.Menu(self.raiz, tearoff=0)
        for c in cand[:14]:
            m.add_command(label=c, command=lambda c=c: poner(c))
        m.tk_popup(self.raiz.winfo_pointerx(), self.raiz.winfo_pointery())

    def _menu_historial(self):
        m = tk.Menu(self.raiz, tearoff=0)
        if not self.historial:
            m.add_command(label=_("(vacío)"), state="disabled")
        for h in reversed(self.historial[-15:]):
            lab = h.replace("\n", " ")
            m.add_command(label=lab if len(lab) < 60 else lab[:57] + "...", command=lambda h=h: self.poner_consulta(h))
        m.tk_popup(self.raiz.winfo_pointerx(), self.raiz.winfo_pointery())

    def _recordar(self, texto):
        if texto in self.historial:
            self.historial.remove(texto)
        self.historial.append(texto)
        self.historial = self.historial[-30:]
        self.cfg["historial"] = self.historial
        guardar_config(CONFIG, self.cfg)

    def _confirmar_sentencias(self, sents):
        for s in sents:
            motivo = peligrosa(s)
            if motivo and not confirmar_peligro(_("Confirmar"), "%s\n\n%s\n\n%s" % (motivo, s[:200], _("¿Ejecutarla de todos modos?"))):
                raise Cancelado()

    def _ejecutar_sentencias(self, sents):
        cols, filas, n = [], [], 0
        for s in sents:
            primera = (s.split(None, 1) or [""])[0].lower()
            if primera not in SOLO_LECTURA:
                self._asegurar_tx()
            cols, filas, n = self.bd.sql(s)
        return cols, [[("NULL" if v is None else basedatos.texto_de_valor(v)) for v in f] for f in filas[:LIM_CONSULTA]], n, \
            [(i, j) for i, f in enumerate(filas[:LIM_CONSULTA]) for j, v in enumerate(f) if v is None]

    def ejecutar(self):
        """F5: ejecuta la consulta de la pestaña SQL (con confirmación si es peligrosa)."""
        if self.pestana != "SQL":
            self.ir_pestana("SQL")
        if self.bd is None:
            self.msg(_("Abre primero una base"))
            return
        texto = self.texto_consulta().strip()
        if not texto:
            return
        t0 = ahora_ms()
        try:
            sents = sentencias(texto)
            self._confirmar_sentencias(sents)
            cols, filas, afectadas, nulos = self._ejecutar_sentencias(sents)
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            self.l_res.config(text="Error: %s" % e)
            self._pendiente_ui()
            return
        except Cancelado:
            self.msg("Consulta cancelada")
            return
        ms = ahora_ms() - t0
        self._recordar(texto)
        self.res_cols, self.res_filas, self.res_nulos = cols, filas, set(nulos)
        self.rej_res.poner(cols, filas, nulos=nulos)
        r = ("%d filas en %d ms" % (len(filas), ms)) if cols else ("%d filas afectadas en %d ms" % (max(0, afectadas), ms))
        self.l_res.config(text=r)
        self.msg(r)
        self._pendiente_ui()
        if not cols:
            self.actualizar()
            self.msg(r)

    def plan(self):
        """EXPLAIN QUERY PLAN de la última sentencia de la pestaña."""
        if self.bd is None:
            return
        sents = sentencias(self.texto_consulta())
        if not sents:
            return
        try:
            filas = self.bd.plan(sents[-1])
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            self.l_res.config(text="Error: %s" % e)
            return
        self.res_nulos = set()
        self.res_cols = [_("id"), _("padre"), _("detalle")]
        self.res_filas = [[str(a), str(b), c] for a, b, c in filas]
        self.rej_res.poner(self.res_cols, self.res_filas)
        self.l_res.config(text="EXPLAIN QUERY PLAN: %d pasos" % len(filas))
        self.msg("Plan de consulta: %d pasos" % len(filas))

    # ---------------------------------------------------------- el árbol
    def _refrescar_arbol(self):
        L = []
        if self.bd is None:
            self.poner_arbol([])
            return
        L.append(("[-] %s" % self.etq, ("con",)))
        try:
            objs = self.bd.objetos()
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            objs = []
        for tipo, nombre in GRUPOS:
            lista = [o for o in objs if o["tipo"] == tipo]
            plegado = self.exp_grupo.get(tipo)
            L.append(("   [%s] %s (%d)" % ("+" if plegado else "-", _(nombre), len(lista)), ("grupo", tipo)))
            if plegado:
                continue
            for o in lista:
                if tipo in ("table", "view"):
                    exp = self.exp_tabla.get(o["nombre"])
                    L.append(("      [%s] %s" % ("-" if exp else "+", o["nombre"]), ("tabla", o["nombre"], tipo == "view")))
                    if exp:
                        try:
                            esq = self.bd.esquema(o["nombre"])
                        except ErrorBD:
                            esq = []
                        for x in esq:
                            L.append(("          %s  %s%s" % (x["nombre"], x["tipo"], "  (clave)" if x["clave"] else ""), ("col", o["nombre"])))
                elif tipo == "index":
                    L.append(("      # %s (%s)" % (o["nombre"], o["tabla"]), ("indice", o["nombre"], o["tabla"])))
                else:
                    L.append(("      ! %s (%s)" % (o["nombre"], o["tabla"]), ("disparador", o["nombre"], o["tabla"])))
        self.poner_arbol(L)

    def al_elegir_nodo(self, n):
        tipo = n[0]
        if tipo == "con":
            self.msg(self.ruta)
        elif tipo == "tabla":
            self.elegir_tabla(n[1], n[2])
        elif tipo == "col":
            self.elegir_tabla(n[1], self.es_vista)
        elif tipo in ("indice", "disparador"):
            self.l_estr.config(text="%s %s (%s)" % (_("Índice") if tipo == "indice" else _("Disparador"), n[1], n[2]))
            self._poner_sql(self.bd.sql_de(n[1]))
            self.ir_pestana("Estructura")

    def al_alternar_nodo(self, n):
        tipo = n[0]
        if tipo == "grupo":
            self.exp_grupo[n[1]] = not self.exp_grupo.get(n[1])
            self._refrescar_arbol()
            self.elegir_nodo(lambda x: x == n)
        elif tipo == "tabla":
            self.exp_tabla[n[1]] = not self.exp_tabla.get(n[1])
            self._refrescar_arbol()
            self.elegir_nodo(lambda x: x[0] == "tabla" and x[1] == n[1])
        elif tipo == "con":
            self.cerrar_base()

    def menu_nodo(self, n, m):
        if n is not None and n[0] == "tabla":
            m.add_command(label=_("Abrir datos"), command=lambda: self.ir_pestana("Datos"))
            m.add_command(label=_("Modificar tabla..."), command=self.modificar_tabla)
            m.add_command(label=_("Exportar a CSV..."), command=self.exportar_csv)
            m.add_command(label=_("Borrar tabla"), command=self.borrar_tabla)
        elif n is not None and n[0] == "indice":
            m.add_command(label=_("Borrar índice"), command=lambda: self.borrar_indice(n[1]))
        elif n is not None and n[0] == "con":
            m.add_command(label=_("Cerrar base"), command=self.cerrar_base)
        m.add_separator()
        m.add_command(label=_("Abrir base SQLite..."), command=self.abrir_dialogo)

    def elegir_tabla(self, t, vista=False):
        self.cur = t
        self.es_vista = vista
        self.orden, self.pagina = "", 0
        self.filtros = []
        self.e_filtro.delete(0, "end")
        self.mostrar_datos(0)
        self.mostrar_estructura()

    def actualizar(self):
        if self.bd is not None:
            self._refrescar_arbol()
            if self.cur:
                try:
                    if self.cur not in self.bd.tablas():
                        self.cur = None
                except ErrorBD:
                    pass
            self.mostrar_datos()
            self.mostrar_estructura()

    # ---------------------------------------------------------- abrir y cerrar
    def abrir_dialogo(self):
        r = filedialog.askopenfilename(title=_("Abrir una base SQLite"), initialdir="/home",
                                       filetypes=[(_("Bases SQLite"), "*.db *.sqlite *.sqlite3"), (_("Todos"), "*")])
        if r:
            self.abrir(r)

    def nueva_base(self):
        r = filedialog.asksaveasfilename(title=_("Nueva base SQLite"), initialdir="/home", initialfile="nueva.db",
                                         defaultextension=".db", filetypes=[(_("Bases SQLite"), "*.db *.sqlite *.sqlite3")])
        if r:
            self.abrir(r)

    def abrir(self, ruta):
        """Abre una base (cierra la anterior). No ejecuta nada más que las lecturas del esquema."""
        if not self._resolver_pendientes():
            return False
        try:
            bd = basedatos.conectar("sqlite:" + ruta)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo abrir"), str(e))
            self.msg("Error: %s" % e)
            return False
        self._cerrar_silencioso()
        self.bd, self.ruta, self.etq = bd, ruta, etiqueta_de(ruta)
        self.cur = None
        self.exp_tabla = {}
        self.exp_grupo = {}
        self.rej.poner([], [])
        self.l_pag.config(text="")
        rec = [r for r in self.cfg.get("recientes", []) if r != ruta] + [ruta]
        self.cfg["recientes"] = rec[-8:]
        guardar_config(CONFIG, self.cfg)
        self.raiz.title("%s - %s" % (_("Explorador SQLite"), self.etq))
        self._refrescar_arbol()
        self._pendiente_ui()
        self.msg("Abierta %s (%d tablas)" % (self.etq, len(self.bd.tablas())))
        return True

    def _cerrar_silencioso(self):
        if self.bd is not None:
            try:
                self.bd.cerrar()
            except Exception:
                pass
        self.bd = None

    def cerrar_base(self):
        if self.bd is None or not self._resolver_pendientes():
            return
        self._cerrar_silencioso()
        self.cur = None
        self.rej.poner([], [])
        self.raiz.title(_("Explorador SQLite"))
        self._refrescar_arbol()
        self._pendiente_ui()

    # ---------------------------------------------------------- tablas
    def borrar_tabla(self):
        t = self.cur
        if self.bd is None or not t:
            return
        if not confirmar_peligro(_("Borrar tabla"), _("¿Borrar «%s» con todos sus datos?\nNo se puede deshacer salvo con «Deshacer cambios».") % t):
            return
        try:
            self._asegurar_tx()
            if self.es_vista:
                self.bd.ejecutar('DROP VIEW "%s"' % t.replace('"', '""'))
            else:
                self.bd.borrar_tabla(t)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo borrar"), str(e))
            return
        self._pendiente_ui()
        self.cur = None
        self.rej.poner([], [])
        self._refrescar_arbol()
        self.mostrar_estructura()
        self.msg("Borrada %s" % t)

    # ---------------------------------------------------------- herramientas
    def comprobar_integridad(self):
        if self.bd is None:
            return
        try:
            r = self.bd.integridad()
        except ErrorBD as e:
            messagebox.showerror(_("Integridad"), str(e))
            return
        if r == ["ok"]:
            self.msg("integrity_check: ok")
            messagebox.showinfo(_("Integridad"), _("La base está sana: integrity_check dice «ok»."))
        else:
            self.msg("integrity_check: %d problemas" % len(r))
            messagebox.showwarning(_("Integridad"), "\n".join(r[:12]))

    def compactar(self):
        if self.bd is None:
            return
        if self.bd.en_transaccion:
            messagebox.showinfo(_("Compactar"), _("Escribe o deshaz primero los cambios pendientes."))
            return
        try:
            antes = os.stat(self.ruta)[6]
        except OSError:
            antes = 0
        try:
            self.bd.compactar()
        except ErrorBD as e:
            messagebox.showerror(_("Compactar"), str(e))
            return
        try:
            despues = os.stat(self.ruta)[6]
        except OSError:
            despues = 0
        self.msg("VACUUM: %d -> %d bytes" % (antes, despues))
        messagebox.showinfo(_("Compactar"), "VACUUM: %d -> %d bytes" % (antes, despues))

    # ---------------------------------------------------------- exportar e importar
    def csv_de_tabla(self):
        """Todo el contenido de la tabla actual (con los filtros) como texto CSV."""
        bd, t = self.bd, self.cur
        cols = list(bd.columnas(t))
        filas = []
        pos = 0
        f = self._filtro_actual()
        while True:
            lote = bd.consultar(t, f, [self.orden] if self.orden else None, 500, pos)
            if not lote:
                break
            filas += lote
            pos += 500
            if len(lote) < 500 or pos >= 200000:
                break
        return basedatos.a_csv(cols, filas)

    def exportar_csv(self, ruta=None, abrir=True):
        """Tabla > Exportar a CSV: guarda la tabla actual y ofrece abrirla en Macaco."""
        if self.bd is None or not self.cur:
            messagebox.showinfo(_("Exportar"), _("Elige primero una tabla."))
            return None
        try:
            texto = self.csv_de_tabla()
        except ErrorBD as e:
            messagebox.showerror(_("Exportar"), str(e))
            return None
        return self._guardar_texto(texto, self.cur + ".csv", ruta, abrir, "CSV", "*.csv")

    def exportar_resultado(self):
        if not self.res_cols:
            messagebox.showinfo(_("Exportar"), _("No hay ningún resultado que exportar."))
            return
        filas = [[("" if (i, j) in self.res_nulos else v) for j, v in enumerate(f)] for i, f in enumerate(self.res_filas)]   # NULL sale vacío
        self._guardar_texto(basedatos.a_csv(self.res_cols, filas), "consulta.csv", None, True, "CSV", "*.csv")

    def exportar_volcado(self):
        if self.bd is None:
            return
        try:
            texto = "\n".join(self.bd.volcado()) + "\n"
        except ErrorBD as e:
            messagebox.showerror(_("Exportar"), str(e))
            return
        self._guardar_texto(texto, self.etq.rsplit(".", 1)[0] + ".sql", None, False, "SQL", "*.sql")

    def _guardar_texto(self, texto, nombre, ruta, abrir, tipo, patron):
        if ruta is None:
            ruta = filedialog.asksaveasfilename(title=_("Exportar a %s") % tipo, initialdir="/home", initialfile=nombre,
                                                defaultextension=patron[1:], filetypes=[(tipo, patron), (_("Todos"), "*")])
        if not ruta:
            return None
        try:
            with open(ruta, "w") as f:
                f.write(texto)
        except OSError as e:
            messagebox.showerror(_("Exportar"), "No se pudo escribir %s: %s" % (ruta, e))
            return None
        self.msg("Exportado: %s" % ruta)
        if abrir and messagebox.askyesno(_("Exportar"), _("Guardado en %s.\n¿Abrirlo en Macaco?") % ruta):
            os.system("/apps/bin/macaco %s &" % ruta)
            self.msg("Abriendo %s en Macaco" % ruta)
        return ruta

    def importar_csv(self, ruta=None):
        """Archivo > Importar CSV: la primera fila son los nombres de columna. Entra en la tabla elegida (por nombre de
        columna) o, si no hay ninguna, crea una tabla con el nombre del archivo."""
        if self.bd is None:
            messagebox.showinfo(_("Importar"), _("Abre primero una base."))
            return
        if ruta is None:
            ruta = filedialog.askopenfilename(title=_("Importar CSV"), initialdir="/home",
                                              filetypes=[("CSV", "*.csv"), (_("Todos"), "*")])
        if not ruta:
            return
        try:
            with open(ruta) as f:
                filas = _csv.de_csv(f.read())
        except OSError as e:
            messagebox.showerror(_("Importar"), "No se pudo leer %s: %s" % (ruta, e))
            return
        if len(filas) < 2:
            messagebox.showinfo(_("Importar"), _("El archivo no tiene datos (hace falta una fila de cabecera y al menos una de datos)."))
            return
        cab, datos = [c.strip() for c in filas[0]], filas[1:]
        try:
            nueva = False
            t = self.cur
            if not t or self.es_vista:
                t = etiqueta_de(ruta).rsplit(".", 1)[0].replace(" ", "_") or "importada"
                nueva = True
            if not messagebox.askyesno(_("Importar"), _("¿Importar %d filas en la tabla «%s»%s?") % (len(datos), t, _(" (nueva)") if nueva else "")):
                return
            self._asegurar_tx()
            if nueva:
                self.bd.crear_tabla_def(t, [{"nombre": c, "tipo": "TEXT"} for c in cab])
                tipos = dict((c, "TEXT") for c in cab)
            else:
                tipos = dict((x["nombre"], x["tipo"]) for x in self.bd.esquema(t))
                bajos = dict((k.lower(), k) for k in tipos)
                cab = [bajos.get(c.lower(), c) for c in cab]
                malas = [c for c in cab if c not in tipos]
                if malas:
                    raise ErrorBD(_("la tabla no tiene la columna «%s»") % malas[0])
            conv = []
            for fila in datos:
                r = []
                for j in range(len(cab)):
                    v = fila[j] if j < len(fila) else None
                    tp = (tipos[cab[j]] or "").upper()
                    if v is not None and v == "" and tp not in ("", "TEXT"):
                        v = None          # un campo vacío en una columna numérica es NULL
                    elif v is not None:
                        v = basedatos.valor_desde_texto(v, tipos[cab[j]])
                    r.append(v)
                conv.append(r)
            n = self.bd.importar_filas(t, cab, conv)
        except ErrorBD as e:
            messagebox.showerror(_("Importar"), str(e))
            return
        self._pendiente_ui()
        self.msg("Importadas %d filas en %s" % (n, t))
        self.cur = t
        self.es_vista = False
        self._refrescar_arbol()
        self.elegir_tabla(t)

    def ejecutar_archivo(self, ruta=None):
        """Ejecuta un archivo SQL (por ejemplo un volcado) en la base abierta; pide confirmación si trae DROP o DELETE sin WHERE."""
        if self.bd is None:
            messagebox.showinfo(_("Ejecutar archivo SQL"), _("Abre primero una base."))
            return
        if ruta is None:
            ruta = filedialog.askopenfilename(title=_("Ejecutar archivo SQL"), initialdir="/home",
                                              filetypes=[("SQL", "*.sql"), (_("Todos"), "*")])
        if not ruta:
            return
        try:
            with open(ruta) as f:
                sents = sentencias(f.read())
            self._confirmar_sentencias(sents)
            n = 0
            for s in sents:
                if s.split(None, 1)[0].lower() in ("begin", "commit", "end"):
                    continue
                self._asegurar_tx()
                self.bd.sql(s)
                n += 1
        except (OSError, ErrorBD) as e:
            messagebox.showerror(_("Ejecutar archivo SQL"), str(e))
            self._pendiente_ui()
            return
        except Cancelado:
            self.msg("Consulta cancelada")
            return
        self._pendiente_ui()
        self.msg("Ejecutadas %d sentencias de %s" % (n, ruta))
        self.actualizar()

    # ---------------------------------------------------------- salida
    def salir(self):
        if not self._resolver_pendientes():
            return
        self._cerrar_silencioso()
        self.raiz.destroy()


def principal(argv):
    raiz = tk.Tk()
    ex = ExploradorSQLite(raiz)
    for a in argv[1:]:
        if a == "--sonda":
            ex.sonda = True
            continue
        if a.startswith("sqlite:"):
            a = a[7:]
        if os.path.exists(a) or a.endswith(".db") or a.endswith(".sqlite") or a.endswith(".sqlite3"):
            ex.abrir(a if a.startswith("/") else os.getcwd().rstrip("/") + "/" + a)
    raiz.mainloop()


if __name__ == "__main__":
    principal(sys.argv)
