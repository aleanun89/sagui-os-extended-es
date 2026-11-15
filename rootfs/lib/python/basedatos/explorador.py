# SPDX-License-Identifier: GPL-2.0-or-later
# Explorador de bases de datos (mejora 127, D304): SQLite y lite-monkey con tkinter.
#
#     python /lib/python/basedatos/explorador.py [fichero.db | litemonkey://host:7979/base]
#
# Árbol a la izquierda (conexiones -> tablas -> columnas e índices) y, a la derecha, las pestañas
# Datos (paginada, con filtro y orden, editable), Estructura y Consulta (SQL en SQLite, filtro JSON
# en lite-monkey). Al abrir un fichero no se ejecuta nada. F12 imprime las posiciones de los
# componentes para las pruebas automáticas.
import os
import sys
import json
import tkinter as tk
from tkinter import messagebox, filedialog
import basedatos
from basedatos import ErrorBD
from basedatos._cuadricula import Cuadricula
from basedatos._seguridad import PALABRAS, sentencias, peligrosa

try:
    import sagui
except ImportError:
    sagui = None

CONFIG = "/home/.config/exploradorbd.json"
TAM_PAGINA = 50
LIM_CONSULTA = 500
PUERTO_LMK = 7979


def ahora_ms():
    if sagui is not None:
        return sagui.ticks_ms()
    import time
    return int(time.time() * 1000)


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


# ------------------------------------------------------------ configuración

def leer_config():
    try:
        with open(CONFIG) as f:
            c = json.loads(f.read())
        if isinstance(c.get("conexiones"), list):
            return c
    except Exception:
        pass
    return {"conexiones": [], "historial": []}


def guardar_config(c):
    try:
        d = os.path.dirname(CONFIG)
        try:
            os.mkdir(d)
        except OSError:
            pass
        with open(CONFIG, "w") as f:
            f.write(json.dumps(c))
    except Exception:
        pass


def etiqueta_de(origen):
    if origen.startswith("sqlite:"):
        return origen[7:].split("/")[-1] or origen[7:]
    if origen.startswith("litemonkey://"):
        return origen[13:]
    return origen


# ------------------------------------------------------------ el explorador

class Explorador:
    PAGINAS = ("Datos", "Estructura", "Consulta")

    def __init__(self, raiz):
        self.raiz = raiz
        self.cfg = leer_config()
        self.conexiones = []         # {"origen", "bd", "abierta", "tablas", "exp": {tabla: bool}, "etq"}
        self.nodos = []              # lo que enseña el árbol: (tipo, conexión, tabla, datos)
        self.cur = None              # (conexión, tabla) elegidas
        self.pagina = 0
        self.total = 0
        self.orden = ""
        self.filtro = ""
        self.claves = []             # la clave de cada fila de la rejilla de datos
        self.historial = list(self.cfg.get("historial", []))[-30:]
        self.res_cols, self.res_filas = [], []
        self.sonda = False
        self.pestana = "Datos"
        self._construir()
        for o in self.cfg["conexiones"]:
            self._anadir(o)
        self._refrescar_arbol()

    # ---------------------------------------------------------- ventana
    def _construir(self):
        r = self.raiz
        r.title("Explorador de bases de datos")
        r.geometry("820x540")
        r.minsize(560, 360)
        self._menus()
        self.estado = tk.Label(r, text="", anchor="w", relief="sunken", bd=1)
        self.estado.pack(side="bottom", fill="x")
        cuerpo = tk.Frame(r)
        cuerpo.pack(fill="both", expand=True)
        izq = tk.Frame(cuerpo, width=250)
        izq.pack(side="left", fill="y")
        izq.pack_propagate(False)
        tk.Label(izq, text="Conexiones", anchor="w").pack(fill="x", padx=4)
        marco = tk.Frame(izq)
        marco.pack(fill="both", expand=True, padx=(4, 0), pady=(0, 4))
        self.arbol = tk.Listbox(marco, width=28, height=10, selectmode="browse")
        sb = tk.Scrollbar(marco, command=self.arbol.yview)
        self.arbol.config(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self.arbol.pack(side="left", fill="both", expand=True)
        self.arbol.bind("<<ListboxSelect>>", lambda e: self._arbol_elegido())
        self.arbol.bind("<Double-Button-1>", lambda e: self._arbol_alternar())
        self.arbol.bind("<Return>", lambda e: self._arbol_alternar())
        self.arbol.bind("<Button-3>", self._arbol_menu)
        der = tk.Frame(cuerpo)
        der.pack(side="left", fill="both", expand=True)
        barra = tk.Frame(der)
        barra.pack(fill="x", padx=4, pady=(4, 0))
        self.botones_pest = {}
        for n in self.PAGINAS:
            b = tk.Button(barra, text=n, width=10, command=lambda n=n: self.ir_pestana(n))
            b.pack(side="left")
            self.botones_pest[n] = b
        self.zona = tk.Frame(der, relief="sunken", bd=2)
        self.zona.pack(fill="both", expand=True, padx=4, pady=(0, 4))
        self.pags = {}
        self._pag_datos()
        self._pag_estructura()
        self._pag_consulta()
        self.ir_pestana("Datos")
        r.bind("<F5>", lambda e: self.ejecutar())
        r.bind("<F12>", lambda e: self.imprimir_sonda())
        r.protocol("WM_DELETE_WINDOW", self.salir)

    def _menus(self):
        m = tk.Menu(self.raiz)
        a = tk.Menu(m, tearoff=0)
        a.add_command(label="Abrir base SQLite...", command=self.abrir_sqlite)
        a.add_command(label="Conectar a lite-monkey...", command=self.conectar_lmk)
        a.add_command(label="Cerrar conexión", command=self.cerrar_conexion)
        a.add_separator()
        a.add_command(label="Salir", command=self.salir)
        m.add_cascade(label="Archivo", menu=a)
        t = tk.Menu(m, tearoff=0)
        t.add_command(label="Nueva tabla...", command=self.nueva_tabla)
        t.add_command(label="Exportar a CSV...", command=self.exportar_csv)
        t.add_command(label="Borrar tabla", command=self.borrar_tabla)
        t.add_separator()
        t.add_command(label="Actualizar", command=self.actualizar)
        m.add_cascade(label="Tabla", menu=t)
        h = tk.Menu(m, tearoff=0)
        h.add_command(label="Acerca de...", command=lambda: messagebox.showinfo(
            "Explorador de bases de datos", "Explorador de bases de datos\nSQLite y lite-monkey (mejora 127)"))
        m.add_cascade(label="Ayuda", menu=h)
        self.raiz.config(menu=m)

    def ir_pestana(self, n):
        self.pestana = n
        for k, f in self.pags.items():
            f.pack_forget()
        self.pags[n].pack(fill="both", expand=True)
        for k, b in self.botones_pest.items():
            b.config(relief="sunken" if k == n else "raised")
        if n == "Consulta":
            self._preparar_consulta()

    def msg(self, t):
        self.estado.config(text=t)
        print("exploradorbd:", t)

    # ---------------------------------------------------------- pestaña Datos
    def _pag_datos(self):
        f = self.pags["Datos"] = tk.Frame(self.zona)
        fila = tk.Frame(f)
        fila.pack(fill="x", padx=4, pady=4)
        tk.Label(fila, text="Filtro:").pack(side="left")
        self.e_filtro = tk.Entry(fila)
        self.e_filtro.pack(side="left", fill="x", expand=True, padx=4)
        self.e_filtro.bind("<Return>", lambda e: self.aplicar_filtro())
        tk.Button(fila, text="Aplicar", command=self.aplicar_filtro).pack(side="left")
        tk.Button(fila, text="Quitar", command=self.quitar_filtro).pack(side="left", padx=(4, 0))
        self.l_pista = tk.Label(f, text="", anchor="w")
        self.l_pista.pack(fill="x", padx=4)
        baj = tk.Frame(f)
        baj.pack(side="bottom", fill="x", padx=4, pady=4)
        tk.Button(baj, text="Borrar fila", command=self.borrar_fila).pack(side="right")
        tk.Button(baj, text="Fila nueva...", command=self.fila_nueva).pack(side="right", padx=4)
        for t, c, a in (("|<", self.pag_primera, 3), ("<", self.pag_anterior, 3), (">", self.pag_siguiente, 3),
                        (">|", self.pag_ultima, 3)):
            tk.Button(baj, text=t, width=a, command=c).pack(side="left")
        self.l_pag = tk.Label(baj, text="", anchor="w")
        self.l_pag.pack(side="left", padx=8)
        self.rej = Cuadricula(f, ancho=520, alto=300)
        self.rej.pack(fill="both", expand=True, padx=4)
        self.rej.editable = True
        self.rej.al_ordenar = self._ordenar
        self.rej.al_editar = self._celda_editada

    def _conexion(self):
        return self.cur[0] if self.cur else None

    def _tabla(self):
        return self.cur[1] if self.cur else None

    def mostrar_datos(self, pagina=None):
        c, t = self._conexion(), self._tabla()
        if c is None or not t:
            self.rej.poner([], [])
            self.l_pag.config(text="")
            return
        bd = c["bd"]
        if pagina is not None:
            self.pagina = pagina
        lmk = bd.tipo == "litemonkey"
        self.l_pista.config(text=('JSON, p. ej. {"edad": {"$gt": 30}}' if lmk else "condición SQL, p. ej. edad > 30 AND nombre LIKE 'A%'"))
        try:
            f = self.filtro or None
            self.total = bd.contar(t, f)
            paginas = max(1, (self.total + TAM_PAGINA - 1) // TAM_PAGINA)
            self.pagina = max(0, min(self.pagina, paginas - 1))
            filas = bd.consultar(t, f, [self.orden] if self.orden else None, TAM_PAGINA, self.pagina * TAM_PAGINA)
            cols = list(bd.columnas(t))
            for fila in filas:
                for k in fila:
                    if k not in cols:
                        cols.append(k)
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            messagebox.showerror("Datos", str(e))
            return
        clave = bd.clave(t)
        self.claves = [fila.get(clave) for fila in filas]
        self.rej.col_editables = [x for x in cols if x != clave]
        self.rej.poner(cols, [[basedatos.texto_de_valor(fila.get(x)) for x in cols] for fila in filas])
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
        self.filtro = self.e_filtro.get().strip()
        self.mostrar_datos(0)

    def quitar_filtro(self):
        self.e_filtro.delete(0, "end")
        self.filtro = ""
        self.mostrar_datos(0)

    def _ordenar(self, j):
        col = self.rej.columnas[j]
        self.orden = col if self.orden != col else "-" + col
        self.mostrar_datos(0)

    def _celda_editada(self, i, j, texto):
        """El usuario cambió una celda: se escribe en la base con parámetros enlazados."""
        c, t = self._conexion(), self._tabla()
        col = self.rej.columnas[j]
        try:
            valor = basedatos.valor_desde_texto(texto, self._tipo_de(c, t, col))
            n = c["bd"].actualizar(t, {col: valor}, clave=self.claves[i])
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            messagebox.showerror("No se pudo guardar", str(e))
            return False
        self.msg("%s: fila actualizada (%s = %s)" % (t, col, texto) if n else "No se encontró la fila")
        return n > 0

    def _tipo_de(self, c, t, col):
        for x in c["bd"].esquema(t):
            if x["nombre"] == col:
                return x["tipo"]
        return None

    def fila_nueva(self):
        c, t = self._conexion(), self._tabla()
        if c is None or not t:
            return
        bd = c["bd"]
        try:
            esq = bd.esquema(t)
        except ErrorBD as e:
            messagebox.showerror("Fila nueva", str(e))
            return
        clave = bd.clave(t)
        cols = [x for x in esq if not (x["nombre"] == clave and bd.tipo == "litemonkey")]
        d = tk.Toplevel(self.raiz, _dialogo=True)
        d.title("Fila nueva en %s" % t)
        d.transient(self.raiz)
        d.resizable(False, False)
        ents = {}
        for k, x in enumerate(cols[:12]):
            tk.Label(d, text="%s (%s)" % (x["nombre"], x["tipo"] or "?"), anchor="w").grid(row=k, column=0, sticky="w", padx=8, pady=2)
            e = tk.Entry(d, width=30)
            e.grid(row=k, column=1, padx=8, pady=2)
            ents[x["nombre"]] = e
        if bd.tipo == "litemonkey":
            tk.Label(d, text="Campo nuevo:", anchor="w").grid(row=len(ents), column=0, sticky="w", padx=8)
            en, ev = tk.Entry(d, width=12), tk.Entry(d, width=30)
            en.grid(row=len(ents), column=1, sticky="w", padx=8)
            ev.grid(row=len(ents) + 1, column=1, padx=8)
        else:
            en = ev = None
        ok = []

        def aceptar():
            ok.append(True)
            d.destroy()
        fb = tk.Frame(d, pady=6)
        fb.grid(row=len(ents) + 3, column=0, columnspan=2)
        tk.Button(fb, text="Insertar", width=10, command=aceptar).pack(side="left", padx=4)
        tk.Button(fb, text="Cancelar", width=10, command=d.destroy).pack(side="left", padx=4)
        if ents:
            list(ents.values())[0].focus_set()
        d.grab_set()
        self.raiz.wait_window(d)
        if not ok:
            return
        fila = {}
        try:
            for x in cols[:12]:
                txt = ents[x["nombre"]].get()
                if txt != "":
                    fila[x["nombre"]] = basedatos.valor_desde_texto(txt, x["tipo"])
            if en is not None and en.get().strip():
                fila[en.get().strip()] = basedatos.valor_desde_texto(ev.get())
            bd.insertar(t, fila)
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            messagebox.showerror("No se pudo insertar", str(e))
            return
        self.msg("%s: fila insertada" % t)
        self.pag_ultima()

    def borrar_fila(self):
        c, t = self._conexion(), self._tabla()
        i = self.rej.fila
        if c is None or not t or i < 0 or i >= len(self.claves):
            self.msg("Elige primero una fila")
            return
        if not messagebox.askyesno("Borrar fila", "¿Borrar la fila %d de %s?\nNo se puede deshacer." % (self.pagina * TAM_PAGINA + i + 1, t)):
            return
        try:
            c["bd"].borrar(t, clave=self.claves[i])
        except ErrorBD as e:
            messagebox.showerror("No se pudo borrar", str(e))
            return
        self.msg("%s: fila borrada" % t)
        self.mostrar_datos()

    # ---------------------------------------------------------- pestaña Estructura
    def _pag_estructura(self):
        f = self.pags["Estructura"] = tk.Frame(self.zona)
        self.l_estr = tk.Label(f, text="", anchor="w")
        self.l_estr.pack(fill="x", padx=4, pady=4)
        self.rej_cols = Cuadricula(f, ancho=500, alto=190)
        self.rej_cols.pack(fill="both", expand=True, padx=4)
        tk.Label(f, text="Índices", anchor="w").pack(fill="x", padx=4, pady=(6, 0))
        self.rej_idx = Cuadricula(f, ancho=500, alto=110)
        self.rej_idx.pack(fill="x", padx=4, pady=(0, 4))

    def mostrar_estructura(self):
        c, t = self._conexion(), self._tabla()
        if c is None or not t:
            self.l_estr.config(text="")
            self.rej_cols.poner([], [])
            self.rej_idx.poner([], [])
            return
        bd = c["bd"]
        try:
            esq = bd.esquema(t)
            idx = bd.indices(t)
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            return
        if bd.tipo == "litemonkey":
            self.l_estr.config(text="%s: campos deducidos de una muestra de documentos" % t)
            self.rej_cols.poner(["Campo", "Tipos", "Clave", "En % de documentos"],
                                [[x["nombre"], x["tipo"], "sí" if x["clave"] else "", x.get("frecuencia", "")] for x in esq])
        else:
            self.l_estr.config(text="%s: %d columnas" % (t, len(esq)))
            self.rej_cols.poner(["Columna", "Tipo", "Clave", "Obligatoria", "Por defecto"],
                                [[x["nombre"], x["tipo"], "sí" if x["clave"] else "", "sí" if x["obligatorio"] else "",
                                  x["defecto"]] for x in esq])
        self.rej_idx.poner(["Índice", "Único", "Columnas"],
                           [[x["nombre"], "sí" if x["unico"] else "", ", ".join(x["columnas"])] for x in idx])

    # ---------------------------------------------------------- pestaña Consulta
    def _pag_consulta(self):
        f = self.pags["Consulta"] = tk.Frame(self.zona)
        self.l_cons = tk.Label(f, text="Consulta SQL (F5 para ejecutar):", anchor="w")
        self.l_cons.pack(fill="x", padx=4, pady=(4, 0))
        self.txt = tk.Text(f, height=7, width=60, wrap="none")
        self.txt.pack(fill="x", padx=4)
        self.txt._realce = realzar_sql          # el Text del sistema pide a esta función un color por carácter
        fila = tk.Frame(f)
        fila.pack(fill="x", padx=4, pady=4)
        tk.Button(fila, text="Ejecutar (F5)", command=self.ejecutar).pack(side="left")
        self.b_hist = tk.Button(fila, text="Historial", command=self._menu_historial)
        self.b_hist.pack(side="left", padx=4)
        tk.Button(fila, text="Limpiar", command=lambda: self.poner_consulta("")).pack(side="left")
        tk.Button(fila, text="Exportar resultado...", command=self.exportar_resultado).pack(side="right")
        self.l_res = tk.Label(f, text="", anchor="w")
        self.l_res.pack(side="bottom", fill="x", padx=4, pady=2)
        self.rej_res = Cuadricula(f, ancho=500, alto=180)
        self.rej_res.pack(fill="both", expand=True, padx=4)

    def poner_consulta(self, t):
        self.txt.delete("1.0", "end")
        self.txt.insert("1.0", t)

    def texto_consulta(self):
        return self.txt.get("1.0", "end-1c")

    def _preparar_consulta(self):
        c, t = self._conexion(), self._tabla()
        lmk = c is not None and c["bd"].tipo == "litemonkey"
        self.l_cons.config(text=("Filtro JSON de la colección (F5 para ejecutar):" if lmk else "Consulta SQL (F5 para ejecutar):"))
        if c is not None and t and not self.texto_consulta().strip():
            self.poner_consulta("{}" if lmk else 'SELECT * FROM "%s" LIMIT 100;' % t.replace('"', '""'))

    def _menu_historial(self):
        m = tk.Menu(self.raiz, tearoff=0)
        if not self.historial:
            m.add_command(label="(vacío)", state="disabled")
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
        guardar_config(self.cfg)

    def ejecutar(self):
        """F5: ejecuta la consulta de la pestaña (con confirmación si es peligrosa)."""
        if self.pestana != "Consulta":
            self.ir_pestana("Consulta")
        c = self._conexion()
        if c is None:
            self.msg("Elige primero una conexión o una tabla en el árbol")
            return
        texto = self.texto_consulta().strip()
        if not texto:
            return
        bd = c["bd"]
        t0 = ahora_ms()
        try:
            if bd.tipo == "litemonkey":
                cols, filas, afectadas = self._consulta_lmk(bd, texto)
            else:
                cols, filas, afectadas = self._consulta_sql(bd, texto)
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            self.l_res.config(text="Error: %s" % e)
            return
        except _Cancelado:
            self.msg("Consulta cancelada")
            return
        ms = ahora_ms() - t0
        self._recordar(texto)
        self.res_cols, self.res_filas = cols, filas
        self.rej_res.poner(cols, filas)
        if cols:
            r = "%d filas en %d ms" % (len(filas), ms)
        else:
            r = "%d filas afectadas en %d ms" % (afectadas, ms)
        self.l_res.config(text=r)
        self.msg(r)
        if not cols:
            self.refrescar_conexion(c)

    def _consulta_sql(self, bd, texto):
        sents = sentencias(texto)
        for s in sents:
            motivo = peligrosa(s)
            if motivo and not messagebox.askyesno("Confirmar", "%s\n\n%s\n\n¿Ejecutarla de todos modos?" % (motivo, s[:200])):
                raise _Cancelado()
        cols, filas, n = [], [], 0
        for s in sents:
            cols, filas, n = bd.sql(s)
        return cols, [[basedatos.texto_de_valor(v) for v in f] for f in filas[:LIM_CONSULTA]], n

    def _consulta_lmk(self, bd, texto):
        t = self._tabla()
        if not t:
            raise ErrorBD("elige una colección en el árbol")
        filas = bd.consultar(t, texto, None, LIM_CONSULTA)
        cols = list(bd.columnas(t))
        for f in filas:
            for k in f:
                if k not in cols:
                    cols.append(k)
        return cols, [[basedatos.texto_de_valor(f.get(c)) for c in cols] for f in filas], len(filas)

    # ---------------------------------------------------------- el árbol
    def _anadir(self, origen, abrir=False):
        for c in self.conexiones:
            if c["origen"] == origen:
                return c
        c = {"origen": origen, "bd": None, "abierta": False, "tablas": [], "exp": {}, "etq": etiqueta_de(origen)}
        self.conexiones.append(c)
        return c

    def _guardar_conexiones(self):
        self.cfg["conexiones"] = [c["origen"] for c in self.conexiones][-12:]
        guardar_config(self.cfg)

    def _refrescar_arbol(self):
        self.nodos = []
        lineas = []
        for c in self.conexiones:
            tipo = "lite-monkey" if c["origen"].startswith("litemonkey") else "SQLite"
            lineas.append("[%s] %s  (%s)" % ("-" if c["abierta"] else "+", c["etq"], tipo))
            self.nodos.append(("con", c, None))
            if not c["abierta"]:
                continue
            for t in c["tablas"]:
                exp = c["exp"].get(t)
                lineas.append("   [%s] %s" % ("-" if exp else "+", t))
                self.nodos.append(("tabla", c, t))
                if exp:
                    try:
                        esq = c["bd"].esquema(t)
                        idx = c["bd"].indices(t)
                    except ErrorBD:
                        esq, idx = [], []
                    for x in esq:
                        lineas.append("        %s  %s%s" % (x["nombre"], x["tipo"], "  (clave)" if x["clave"] else ""))
                        self.nodos.append(("col", c, t))
                    for x in idx:
                        lineas.append("        # %s (%s)" % (x["nombre"], ", ".join(x["columnas"])))
                        self.nodos.append(("idx", c, t))
        self.arbol.delete(0, "end")
        for l in lineas:
            self.arbol.insert("end", l)
        self._lineas_arbol = lineas

    def _nodo_actual(self):
        s = self.arbol.curselection()
        if not s or s[0] >= len(self.nodos):
            return None
        return self.nodos[s[0]]

    def _arbol_elegido(self):
        n = self._nodo_actual()
        if n is None:
            return
        tipo, c, t = n
        if tipo == "con":
            self.cur = (c, None)
            self.msg("%s%s" % (c["origen"], "" if c["abierta"] else "  (doble clic para abrir)"))
        else:
            self.elegir_tabla(c, t)

    def _arbol_alternar(self):
        n = self._nodo_actual()
        if n is None:
            return
        tipo, c, t = n
        if tipo == "con":
            if c["abierta"]:
                self.cerrar_conexion(c)
            else:
                self.abrir_conexion(c)
        elif tipo == "tabla":
            c["exp"][t] = not c["exp"].get(t)
            self._refrescar_arbol_conservando(c, t)

    def _refrescar_arbol_conservando(self, c, t):
        self._refrescar_arbol()
        for k, n in enumerate(self.nodos):
            if n[0] == ("tabla" if t else "con") and n[1] is c and n[2] == t:
                self.arbol.selection_set(k)
                self.arbol.see(k)
                break

    def _arbol_menu(self, ev):
        k = self.arbol.nearest(ev.y)
        if 0 <= k < len(self.nodos):
            self.arbol.selection_clear(0, "end")
            self.arbol.selection_set(k)
            self._arbol_elegido()
        n = self._nodo_actual()
        m = tk.Menu(self.raiz, tearoff=0)
        if n is not None and n[0] == "con":
            m.add_command(label="Cerrar" if n[1]["abierta"] else "Abrir", command=self._arbol_alternar)
            m.add_command(label="Quitar de la lista", command=self.quitar_conexion)
            m.add_command(label="Nueva tabla...", command=self.nueva_tabla)
        elif n is not None:
            m.add_command(label="Abrir datos", command=lambda: self.ir_pestana("Datos"))
            m.add_command(label="Exportar a CSV...", command=self.exportar_csv)
            m.add_command(label="Borrar tabla", command=self.borrar_tabla)
        m.add_separator()
        m.add_command(label="Abrir base SQLite...", command=self.abrir_sqlite)
        m.add_command(label="Conectar a lite-monkey...", command=self.conectar_lmk)
        m.tk_popup(self.raiz.winfo_pointerx(), self.raiz.winfo_pointery())

    # ---------------------------------------------------------- conexiones
    def abrir_sqlite(self):
        r = filedialog.askopenfilename(title="Abrir una base SQLite", initialdir="/home",
                                       filetypes=[("Bases SQLite", "*.db *.sqlite *.sqlite3"), ("Todos", "*")])
        if r:
            self.abrir_origen("sqlite:" + r)

    def conectar_lmk(self):
        d = tk.Toplevel(self.raiz, _dialogo=True)
        d.title("Conectar a lite-monkey")
        d.transient(self.raiz)
        d.resizable(False, False)
        tk.Label(d, text="Servidor:").grid(row=0, column=0, sticky="w", padx=8, pady=4)
        e1 = tk.Entry(d, width=22)
        e1.grid(row=0, column=1, padx=8)
        e1.insert(0, "127.0.0.1:%d" % PUERTO_LMK)
        tk.Label(d, text="Base:").grid(row=1, column=0, sticky="w", padx=8, pady=4)
        e2 = tk.Entry(d, width=22)
        e2.grid(row=1, column=1, padx=8)
        e2.insert(0, "test")
        ok = []

        def aceptar(ev=None):
            ok.append((e1.get().strip(), e2.get().strip() or "test"))
            d.destroy()
        fb = tk.Frame(d, pady=6)
        fb.grid(row=2, column=0, columnspan=2)
        tk.Button(fb, text="Conectar", width=10, command=aceptar).pack(side="left", padx=4)
        tk.Button(fb, text="Cancelar", width=10, command=d.destroy).pack(side="left", padx=4)
        e2.bind("<Return>", aceptar)
        e1.bind("<Return>", aceptar)
        e1.focus_set()
        d.grab_set()
        self.raiz.wait_window(d)
        if ok:
            self.abrir_origen("litemonkey://%s/%s" % ok[0])

    def abrir_origen(self, origen):
        c = self._anadir(origen)
        self._guardar_conexiones()
        self.abrir_conexion(c)

    def abrir_conexion(self, c):
        """Conecta (en lite-monkey, ofrece arrancar el servicio si no contesta). No ejecuta nada
        más que las lecturas del esquema."""
        try:
            c["bd"] = basedatos.conectar(c["origen"])
        except basedatos.ErrorConexion as e:
            if c["origen"].startswith("litemonkey") and self._ofrecer_arrancar():
                try:
                    c["bd"] = basedatos.conectar(c["origen"])
                except ErrorBD as e2:
                    messagebox.showerror("lite-monkey", str(e2))
                    return False
            else:
                messagebox.showerror("No se pudo abrir", str(e))
                self.msg("Error: %s" % e)
                return False
        c["abierta"] = True
        self.refrescar_conexion(c)
        self.msg("Abierta %s (%d tablas)" % (c["etq"], len(c["tablas"])))
        return True

    def _ofrecer_arrancar(self):
        """lite-monkey no contesta: ofrece arrancarlo con el gestor de servicios."""
        if not messagebox.askyesno("lite-monkey", "El servidor lite-monkey no está arrancado.\n¿Quieres arrancarlo ahora?"):
            return False
        self.msg("Arrancando el servicio litemonkey...")
        os.system("servicio iniciar litemonkey")
        for _ in range(25):
            try:
                import socket
                s = socket.socket()
                s.settimeout(0.5)
                s.connect(("127.0.0.1", PUERTO_LMK))
                s.close()
                return True
            except Exception:
                pause(200)
        os.system("litemonkey &")   # sin el servicio: a mano (los datos, en /home/litemonkey o en /tmp/litemonkey)
        for _ in range(25):
            try:
                import socket
                s = socket.socket()
                s.settimeout(0.5)
                s.connect(("127.0.0.1", PUERTO_LMK))
                s.close()
                return True
            except Exception:
                pause(200)
        messagebox.showerror("lite-monkey", "No se pudo arrancar el servidor.\nPrueba: servicio iniciar litemonkey")
        return False

    def refrescar_conexion(self, c):
        if not c["abierta"]:
            return
        try:
            c["tablas"] = c["bd"].tablas()
        except ErrorBD as e:
            c["tablas"] = []
            self.msg("Error: %s" % e)
        self._refrescar_arbol()

    def cerrar_conexion(self, c=None):
        if c is None:
            n = self._nodo_actual()
            c = n[1] if n else None
        if c is None or not c["abierta"]:
            return
        try:
            c["bd"].cerrar()
        except Exception:
            pass
        c["bd"], c["abierta"], c["tablas"] = None, False, []
        if self.cur and self.cur[0] is c:
            self.cur = None
            self.rej.poner([], [])
        self._refrescar_arbol()

    def quitar_conexion(self):
        n = self._nodo_actual()
        if n is None:
            return
        c = n[1]
        self.cerrar_conexion(c)
        self.conexiones.remove(c)
        self._guardar_conexiones()
        self._refrescar_arbol()

    def elegir_tabla(self, c, t):
        self.cur = (c, t)
        self.orden, self.filtro, self.pagina = "", "", 0
        self.e_filtro.delete(0, "end")
        if self.pestana == "Consulta":
            self.poner_consulta("")
            self._preparar_consulta()
        self.mostrar_datos(0)
        self.mostrar_estructura()

    def actualizar(self):
        c = self._conexion()
        if c is not None:
            self.refrescar_conexion(c)
            self.mostrar_datos()
            self.mostrar_estructura()

    # ---------------------------------------------------------- tablas
    def nueva_tabla(self):
        c = self._conexion()
        if c is None or not c["abierta"]:
            messagebox.showinfo("Nueva tabla", "Abre primero una conexión.")
            return
        lmk = c["bd"].tipo == "litemonkey"
        d = tk.Toplevel(self.raiz, _dialogo=True)
        d.title("Nueva %s" % ("colección" if lmk else "tabla"))
        d.transient(self.raiz)
        d.resizable(False, False)
        tk.Label(d, text="Nombre:", anchor="w").grid(row=0, column=0, sticky="w", padx=8, pady=4)
        en = tk.Entry(d, width=24)
        en.grid(row=0, column=1, columnspan=2, padx=8)
        filas = []
        if not lmk:
            tk.Label(d, text="Columna", anchor="w").grid(row=1, column=0, padx=8)
            tk.Label(d, text="Tipo", anchor="w").grid(row=1, column=1)
            tk.Label(d, text="Clave", anchor="w").grid(row=1, column=2)
            for k in range(5):
                e = tk.Entry(d, width=18)
                e.grid(row=2 + k, column=0, padx=8, pady=1)
                tv = tk.StringVar(value="TEXT" if k else "INTEGER")
                om = tk.OptionMenu(d, tv, *basedatos.TIPOS_SQL)
                om.grid(row=2 + k, column=1, padx=4)
                kv = tk.IntVar(value=1 if k == 0 else 0)
                ck = tk.Checkbutton(d, variable=kv)
                ck.grid(row=2 + k, column=2)
                if k == 0:
                    e.insert(0, "id")
                filas.append((e, tv, kv))
        ok = []
        fb = tk.Frame(d, pady=6)
        fb.grid(row=9, column=0, columnspan=3)

        def aceptar():
            ok.append(True)
            d.destroy()
        tk.Button(fb, text="Crear", width=10, command=aceptar).pack(side="left", padx=4)
        tk.Button(fb, text="Cancelar", width=10, command=d.destroy).pack(side="left", padx=4)
        en.focus_set()
        d.grab_set()
        self.raiz.wait_window(d)
        if not ok:
            return
        nombre = en.get().strip()
        try:
            if not nombre:
                raise ErrorBD("falta el nombre")
            if lmk:
                c["bd"].crear_tabla(nombre)
            else:
                cols = [(e.get().strip(), tv.get(), bool(kv.get())) for e, tv, kv in filas if e.get().strip()]
                c["bd"].crear_tabla(nombre, cols)
        except ErrorBD as e:
            messagebox.showerror("No se pudo crear", str(e))
            return
        self.msg("Creada %s" % nombre)
        self.refrescar_conexion(c)

    def borrar_tabla(self):
        c, t = self._conexion(), self._tabla()
        if c is None or not t:
            return
        if not messagebox.askyesno("Borrar tabla", "¿Borrar «%s» con todos sus datos?\nNo se puede deshacer." % t):
            return
        try:
            c["bd"].borrar_tabla(t)
        except ErrorBD as e:
            messagebox.showerror("No se pudo borrar", str(e))
            return
        self.cur = (c, None)
        self.rej.poner([], [])
        self.refrescar_conexion(c)
        self.msg("Borrada %s" % t)

    # ---------------------------------------------------------- exportar
    def csv_de_tabla(self):
        """Todo el contenido de la tabla actual (con el filtro) como texto CSV."""
        c, t = self._conexion(), self._tabla()
        bd = c["bd"]
        cols = list(bd.columnas(t))
        filas = []
        pos = 0
        while True:
            lote = bd.consultar(t, self.filtro or None, [self.orden] if self.orden else None, 500, pos)
            if not lote:
                break
            for f in lote:
                for k in f:
                    if k not in cols:
                        cols.append(k)
            filas += lote
            pos += 500
            if len(lote) < 500 or pos >= 200000:
                break
        return basedatos.a_csv(cols, filas)

    def exportar_csv(self, ruta=None, abrir=True):
        """Tabla > Exportar a CSV: guarda la tabla actual y ofrece abrirla en Macaco."""
        c, t = self._conexion(), self._tabla()
        if c is None or not t:
            messagebox.showinfo("Exportar", "Elige primero una tabla.")
            return None
        try:
            texto = self.csv_de_tabla()
        except ErrorBD as e:
            messagebox.showerror("Exportar", str(e))
            return None
        return self._guardar_csv(texto, t + ".csv", ruta, abrir)

    def exportar_resultado(self):
        if not self.res_cols:
            messagebox.showinfo("Exportar", "No hay ningún resultado que exportar.")
            return
        self._guardar_csv(basedatos.a_csv(self.res_cols, self.res_filas), "consulta.csv", None, True)

    def _guardar_csv(self, texto, nombre, ruta, abrir):
        if ruta is None:
            ruta = filedialog.asksaveasfilename(title="Exportar a CSV", initialdir="/home", initialfile=nombre,
                                                defaultextension=".csv", filetypes=[("CSV", "*.csv"), ("Todos", "*")])
        if not ruta:
            return None
        try:
            with open(ruta, "w") as f:
                f.write(texto)
        except OSError as e:
            messagebox.showerror("Exportar", "No se pudo escribir %s: %s" % (ruta, e))
            return None
        self.msg("Exportado: %s" % ruta)
        if abrir and messagebox.askyesno("Exportar", "Guardado en %s.\n¿Abrirlo en Macaco?" % ruta):
            os.system("/bin/macaco %s &" % ruta)
            self.msg("Abriendo %s en Macaco" % ruta)
        return ruta

    # ---------------------------------------------------------- pruebas y salida
    def imprimir_sonda(self):
        """F12: las posiciones (en pantalla) de lo que las pruebas pulsan. Se escribe de una vez
        para que el terminal no la parta con su línea de órdenes."""
        L = ["vps-sonda: inicio"]
        ax, ay = self.arbol.winfo_rootx(), self.arbol.winfo_rooty()
        L.append("sonda: arbol|%d|%d|%d|%d" % (ax, ay, self.arbol.winfo_width(), self.arbol.winfo_height()))
        for k, l in enumerate(self._lineas_arbol):
            L.append("sonda: nodo|%d|%d|%d|%s" % (k, ax + 60, ay + 12 + 18 * k, l.strip()))

        def recorrer(w):
            for h in w.winfo_children():
                if isinstance(h, tk.Button) and h.winfo_ismapped():
                    L.append("sonda: boton|%s|%d|%d" % (h.cget("text").replace("|", "I"), h.winfo_rootx() + h.winfo_width() // 2,
                                                         h.winfo_rooty() + h.winfo_height() // 2))
                recorrer(h)
        recorrer(self.raiz)
        for nombre, g in (("rejilla", self.rej), ("rejilla_res", self.rej_res), ("rejilla_cols", self.rej_cols)):
            if not g.winfo_ismapped():
                continue
            gx, gy = g._cuerpo.winfo_rootx(), g._cuerpo.winfo_rooty()
            L.append("sonda: %s|%d|%d" % (nombre, gx, gy))
            for j in range(len(g.columnas)):
                cx, cy = g.centro_cabecera(j)
                L.append("sonda: cab|%s|%d|%d" % (g.columnas[j], gx + cx, gy + cy))
            for i in range(min(len(g.filas), 12)):
                for j in range(len(g.columnas)):
                    cx, cy = g.centro_celda(i, j)
                    L.append("sonda: celda|%d|%d|%d|%d|%s" % (i, j, gx + cx, gy + cy, g.filas[i][j]))
        L.append("sonda: texto|%d|%d" % (self.txt.winfo_rootx() + 40, self.txt.winfo_rooty() + 20))
        L.append("sonda: fin")
        print("\n".join(L))

    def salir(self):
        for c in self.conexiones:
            if c["abierta"]:
                try:
                    c["bd"].cerrar()
                except Exception:
                    pass
        self.raiz.destroy()


class _Cancelado(Exception):
    pass


def pause(ms):
    if sagui is not None:
        sagui.dormir(ms)
    else:
        import time
        time.sleep(ms / 1000)


def principal(argv):
    raiz = tk.Tk()
    ex = Explorador(raiz)
    for a in argv[1:]:
        if a == "--sonda":
            ex.sonda = True
            continue
        if a.startswith("litemonkey://") or a.startswith("lmk://") or a.startswith("sqlite:"):
            ex.abrir_origen(a)
        elif os.path.exists(a) or a.endswith(".db"):
            ex.abrir_origen("sqlite:" + (a if a.startswith("/") else os.getcwd().rstrip("/") + "/" + a))
    raiz.mainloop()


if __name__ == "__main__":
    principal(sys.argv)
