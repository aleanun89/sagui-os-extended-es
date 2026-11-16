# SPDX-License-Identifier: GPL-2.0-or-later
# Explorador lite-monkey (mejora 131, D333): la aplicación documental, con el espíritu de MongoDB Compass pero con
# nombre y aspecto propios.
#
#     python /lib/python/basedatos/explorador_litemonkey.py [litemonkey://host:7979/base]
#
# Árbol de bases y colecciones (con su recuento y tamaño) a la izquierda y, a la derecha, las pestañas Documentos
# (árbol, JSON o tabla, con barra de consulta), Esquema (por muestreo), Índices y Agregación (etapas con vista previa).
# F12 imprime las posiciones de los componentes para las pruebas automáticas.
import os
import sys
import json
import tkinter as tk
from tkinter import messagebox, filedialog, simpledialog
import basedatos
from basedatos import ErrorBD, ErrorConsulta
from basedatos._cuadricula import Cuadricula
from basedatos._comun import (VentanaBD, ahora_ms, pausa, leer_config, guardar_config, confirmar_peligro, PUERTO_LMK)
from basedatos._idioma import _
from basedatos import _consulta, _json, _muestreo, _csv

CONFIG = "/home/.config/exploradorlmk.json"
LIM_DEFECTO = 20
ETAPAS = ("$match", "$group", "$sort", "$project", "$limit", "$skip", "$unwind", "$count")
PLANTILLAS = {"$match": "{}", "$group": '{"_id": "$campo", "n": {"$sum": 1}}', "$sort": '{"_id": 1}',
              "$project": '{"campo": 1}', "$limit": "10", "$skip": "0", "$unwind": '"$campo"', "$count": '"total"'}


def describir(v):
    """Un valor como lo enseña el árbol: JSON, con los tipos propios de lite-monkey reconocibles."""
    n = type(v).__name__
    if n == "ObjectId":
        return 'ObjectId("%s")' % v
    if n == "Fecha":
        return "Fecha(%d)" % v.ms
    if isinstance(v, dict):
        return "{%d}" % len(v)
    if isinstance(v, (list, tuple)):
        return "[%d]" % len(v)
    if isinstance(v, (bytes, bytearray)) or n == "Binary":
        return "<binario>"
    try:
        return json.dumps(v)
    except Exception:
        return str(v)


def es_contenedor(v):
    return isinstance(v, (dict, list, tuple))


def hijos(v):
    """Los (clave, valor) de un documento o lista."""
    if isinstance(v, dict):
        return list(v.items())
    return [(i, x) for i, x in enumerate(v)]


def tamano_legible(n):
    if n < 1024:
        return "%d B" % n
    if n < 1024 * 1024:
        return "%d KB" % (n // 1024)
    return "%d MB" % (n // (1024 * 1024))


class ExploradorLMK(VentanaBD):
    TITULO = "Explorador lite-monkey"
    PAGINAS = ("Documentos", "Esquema", "Índices", "Agregación")

    def __init__(self, raiz):
        self.cfg = leer_config(CONFIG, {"servidores": [], "historial": [], "favoritas": []})
        self.bd = None               # LiteMonkey de la base elegida
        self.servidor = ""           # host:puerto
        self.credenciales = (None, None)
        self.bases = []              # [(base, [colecciones])]
        self.stats = {}              # (base, colección) -> (documentos, bytes)
        self.exp_base = {}
        self.base = None
        self.col = None
        self.docs = []
        self.sel = -1
        self.vista = "arbol"
        self.exp_arbol = set()       # rutas desplegadas del árbol de documentos
        self.lineas_doc = []         # [(texto, índice de documento, ruta)]
        self.total = 0
        self.etapas = []             # [(operador, texto JSON del cuerpo)]
        self.sel_etapa = -1
        self.res_agr = ([], [])
        self.historial = list(self.cfg.get("historial", []))[-30:]
        VentanaBD.__init__(self, raiz)
        self._refrescar_arbol()

    # ---------------------------------------------------------- menús
    def _menus(self):
        m = tk.Menu(self.raiz)
        a = tk.Menu(m, tearoff=0)
        a.add_command(label=_("Conectar a lite-monkey..."), command=self.conectar_dialogo)
        a.add_command(label=_("Cerrar conexión"), command=self.cerrar_conexion)
        a.add_separator()
        a.add_command(label=_("Importar JSON..."), command=self.importar_json)
        a.add_command(label=_("Exportar a JSON..."), command=lambda: self.exportar("json"))
        a.add_command(label=_("Exportar a JSON por líneas..."), command=lambda: self.exportar("jsonl"))
        a.add_command(label=_("Exportar a CSV..."), command=lambda: self.exportar("csv"))
        a.add_separator()
        a.add_command(label=_("Salir"), command=self.salir)
        m.add_cascade(label=_("Archivo"), menu=a)
        c = tk.Menu(m, tearoff=0)
        c.add_command(label=_("Nueva colección..."), command=self.nueva_coleccion)
        c.add_command(label=_("Borrar colección"), command=self.borrar_coleccion)
        c.add_command(label=_("Borrar base"), command=self.borrar_base)
        c.add_separator()
        c.add_command(label=_("Actualizar"), command=self.actualizar)
        m.add_cascade(label=_("Colección"), menu=c)
        y = tk.Menu(m, tearoff=0)
        y.add_command(label=_("Acerca de..."), command=lambda: messagebox.showinfo(
            _("Explorador lite-monkey"), _("Explorador lite-monkey") + "\n" + _("Bases documentales (mejora 131)")))
        m.add_cascade(label=_("Ayuda"), menu=y)
        self.raiz.config(menu=m)

    def TITULO_ARBOL(self):
        return _("Servidor")

    def al_cambiar_pestana(self, n):
        if self.bd is None or not self.col:
            return
        if n == "Esquema":
            self.mostrar_esquema()
        elif n == "Índices":
            self.mostrar_indices()
        elif n == "Agregación":
            self._etapas_a_lista()

    # ---------------------------------------------------------- páginas
    def _paginas(self):
        self._pag_documentos()
        self._pag_esquema()
        self._pag_indices()
        self._pag_agregacion()

    def _entrada(self, padre, nombre, ancho, lado="left", texto=""):
        e = tk.Entry(padre, width=ancho)
        e.pack(side=lado, padx=2)
        if texto:
            e.insert(0, texto)
        self.cajas[nombre] = e
        return e

    def _pag_documentos(self):
        f = self.pags["Documentos"] = tk.Frame(self.zona)
        a = tk.Frame(f)
        a.pack(fill="x", padx=4, pady=(4, 0))
        tk.Label(a, text=_("Filtro")).pack(side="left")
        self.e_filtro = tk.Entry(a)
        self.e_filtro.pack(side="left", fill="x", expand=True, padx=4)
        self.e_filtro.bind("<Return>", lambda e: self.buscar(0))
        self.cajas["filtro"] = self.e_filtro
        tk.Button(a, text=_("Buscar"), command=lambda: self.buscar(0)).pack(side="left")
        tk.Button(a, text=_("Restablecer"), command=self.restablecer).pack(side="left", padx=(4, 0))
        b = tk.Frame(f)
        b.pack(fill="x", padx=4, pady=2)
        tk.Label(b, text=_("Proyección")).pack(side="left")
        self.e_proy = self._entrada(b, "proyeccion", 14)
        tk.Label(b, text=_("Orden")).pack(side="left")
        self.e_orden = self._entrada(b, "orden", 12)
        tk.Label(b, text=_("Saltar")).pack(side="left")
        self.e_saltar = self._entrada(b, "saltar", 5, texto="0")
        tk.Label(b, text=_("Límite")).pack(side="left")
        self.e_limite = self._entrada(b, "limite", 5, texto=str(LIM_DEFECTO))
        for e in (self.e_proy, self.e_orden, self.e_saltar, self.e_limite):
            e.bind("<Return>", lambda ev: self.buscar())
        tk.Button(b, text=_("Historial"), command=self._menu_historial).pack(side="left", padx=(6, 0))
        tk.Button(b, text=_("Favoritas"), command=self._menu_favoritas).pack(side="left", padx=2)
        c = tk.Frame(f)
        c.pack(fill="x", padx=4)
        self.botones_vista = {}
        for clave, rot in (("arbol", _("Árbol")), ("json", "JSON"), ("tabla", _("Tabla"))):
            bt = tk.Button(c, text=rot, width=8, command=lambda k=clave: self.ir_vista(k))
            bt.pack(side="left")
            self.botones_vista[clave] = bt
        tk.Button(c, text="<", width=3, command=lambda: self.mover(-1)).pack(side="left", padx=(10, 0))
        tk.Button(c, text=">", width=3, command=lambda: self.mover(1)).pack(side="left")
        self.l_pag = tk.Label(c, text="", anchor="w")
        self.l_pag.pack(side="left", padx=8)
        baj = tk.Frame(f)
        baj.pack(side="bottom", fill="x", padx=4, pady=4)
        fila1 = tk.Frame(baj)
        fila1.pack(fill="x")
        for t, cmd in ((_("Insertar JSON..."), self.insertar_json), (_("Formulario..."), self.insertar_formulario),
                       (_("Clonar"), self.clonar), (_("Borrar"), self.borrar_documento)):
            tk.Button(fila1, text=t, command=cmd).pack(side="left", padx=(0, 4))
        tk.Button(fila1, text=_("Borrar filtrados..."), command=self.borrar_filtrados).pack(side="right")
        tk.Button(fila1, text=_("Actualizar filtrados..."), command=self.actualizar_filtrados).pack(side="right", padx=4)
        self.cuerpo = tk.Frame(f)
        self.cuerpo.pack(fill="both", expand=True, padx=4, pady=(2, 0))
        # vista de árbol
        self.v_arbol = tk.Frame(self.cuerpo)
        self.lb_docs = tk.Listbox(self.v_arbol, height=10, selectmode="browse")
        sb = tk.Scrollbar(self.v_arbol, command=self.lb_docs.yview)
        self.lb_docs.config(yscrollcommand=sb.set)
        sb.pack(side="right", fill="y")
        self.lb_docs.pack(side="left", fill="both", expand=True)
        self.lb_docs.bind("<<ListboxSelect>>", lambda e: self._linea_elegida())
        self.lb_docs.bind("<Double-Button-1>", lambda e: self._linea_doble())
        self.lb_docs.bind("<Return>", lambda e: self._linea_doble())
        # vista JSON
        self.v_json = tk.Frame(self.cuerpo)
        fj = tk.Frame(self.v_json)
        fj.pack(side="bottom", fill="x", pady=2)
        tk.Button(fj, text=_("Guardar documento"), command=self.guardar_json).pack(side="left")
        tk.Button(fj, text=_("Revertir"), command=self.mostrar_json).pack(side="left", padx=4)
        self.l_json = tk.Label(fj, text="", anchor="w")
        self.l_json.pack(side="left", padx=8)
        self.txt_json = tk.Text(self.v_json, height=10, width=60, wrap="none")
        sj = tk.Scrollbar(self.v_json, command=self.txt_json.yview)
        self.txt_json.config(yscrollcommand=sj.set)
        sj.pack(side="right", fill="y")
        self.txt_json.pack(side="left", fill="both", expand=True)
        self.cajas["json"] = self.txt_json
        # vista de tabla
        self.v_tabla = tk.Frame(self.cuerpo)
        self.rej = Cuadricula(self.v_tabla, ancho=520, alto=240)
        self.rej.pack(fill="both", expand=True)
        self.rej.editable = True
        self.rej.al_editar = self._celda_editada
        self.rej.al_seleccionar = self._fila_elegida
        self.rejillas["rejilla"] = self.rej
        self.listas["docs"] = (self.lb_docs, [])
        self.ir_vista("arbol")

    def ir_vista(self, v):
        self.vista = v
        for k in (self.v_arbol, self.v_json, self.v_tabla):
            k.pack_forget()
        {"arbol": self.v_arbol, "json": self.v_json, "tabla": self.v_tabla}[v].pack(fill="both", expand=True)
        for k, b in self.botones_vista.items():
            b.config(relief="sunken" if k == v else "raised")
        if v == "json":
            self.mostrar_json()

    # ---------------------------------------------------------- barra de consulta
    def _leer_consulta(self):
        """(filtro, proyección, orden, saltar, límite) de la barra; ErrorConsulta si algo está mal escrito."""
        f = _consulta.filtro(self.e_filtro.get())
        p = _consulta.proyeccion(self.e_proy.get())
        o = _consulta.orden(self.e_orden.get())
        s = _consulta.entero(self.e_saltar.get(), 0, 0, _("saltar"))
        l = _consulta.entero(self.e_limite.get(), LIM_DEFECTO, 1, _("límite"))
        return f, p, o, s, l

    def buscar(self, saltar=None):
        """Ejecuta la consulta de la barra y enseña los documentos (en la vista elegida)."""
        if self.bd is None or not self.col:
            return
        try:
            f, p, o, s, l = self._leer_consulta()
            if saltar is not None:
                s = saltar
                self.e_saltar.delete(0, "end")
                self.e_saltar.insert(0, str(s))
            t0 = ahora_ms()
            self.total = self.bd.contar(self.col, f)
            self.docs = self.bd.consultar_proy(self.col, f, p, o, l, s)
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            messagebox.showerror(_("Consulta"), str(e))
            return
        ms = ahora_ms() - t0
        self._recordar(self.e_filtro.get(), self.e_proy.get(), self.e_orden.get(), s, l)
        self.sel = 0 if self.docs else -1
        self.exp_arbol = set()
        self.l_pag.config(text="%d-%d de %d" % (s + 1 if self.docs else 0, s + len(self.docs), self.total))
        self._dibujar()
        self.msg("%s: %d documentos" % (self.col, self.total))

    def restablecer(self):
        for e, v in ((self.e_filtro, ""), (self.e_proy, ""), (self.e_orden, ""), (self.e_saltar, "0"), (self.e_limite, str(LIM_DEFECTO))):
            e.delete(0, "end")
            e.insert(0, v)
        self.buscar(0)

    def mover(self, d):
        try:
            _f, _p, _o, s, l = self._leer_consulta()
        except ErrorBD as e:
            messagebox.showerror(_("Consulta"), str(e))
            return
        n = max(0, s + d * l)
        if d > 0 and n >= self.total:
            return
        self.buscar(n)

    def _recordar(self, f, p, o, s, l):
        r = _consulta.resumen(f, p, o, s, l)
        if r in self.historial:
            self.historial.remove(r)
        self.historial.append(r)
        self.historial = self.historial[-30:]
        self.cfg["historial"] = self.historial
        guardar_config(CONFIG, self.cfg)

    def _aplicar_resumen(self, r):
        d = json.loads(r)
        for e, v in ((self.e_filtro, d.get("f", "")), (self.e_proy, d.get("p", "")), (self.e_orden, d.get("o", "")),
                     (self.e_saltar, str(d.get("s", 0))), (self.e_limite, str(d.get("l", LIM_DEFECTO)))):
            e.delete(0, "end")
            e.insert(0, v)
        self.buscar()

    def _titulo_resumen(self, r):
        d = json.loads(r)
        t = d.get("f") or "{}"
        return t if len(t) < 56 else t[:53] + "..."

    def _menu_historial(self):
        m = tk.Menu(self.raiz, tearoff=0)
        if not self.historial:
            m.add_command(label=_("(vacío)"), state="disabled")
        for r in reversed(self.historial[-15:]):
            m.add_command(label=self._titulo_resumen(r), command=lambda r=r: self._aplicar_resumen(r))
        m.tk_popup(self.raiz.winfo_pointerx(), self.raiz.winfo_pointery())

    def _menu_favoritas(self):
        m = tk.Menu(self.raiz, tearoff=0)
        m.add_command(label=_("Guardar la consulta actual..."), command=self.guardar_favorita)
        m.add_separator()
        fav = self.cfg.get("favoritas", [])
        if not fav:
            m.add_command(label=_("(vacío)"), state="disabled")
        for nombre, r in fav:
            m.add_command(label=nombre, command=lambda r=r: self._aplicar_resumen(r))
        m.tk_popup(self.raiz.winfo_pointerx(), self.raiz.winfo_pointery())

    def guardar_favorita(self):
        nombre = simpledialog.askstring(_("Consulta favorita"), _("Nombre de la consulta:"))
        if not nombre:
            return
        try:
            f, p, o, s, l = self._leer_consulta()
        except ErrorBD as e:
            messagebox.showerror(_("Consulta"), str(e))
            return
        fav = [x for x in self.cfg.get("favoritas", []) if x[0] != nombre]
        fav.append([nombre, _consulta.resumen(self.e_filtro.get(), self.e_proy.get(), self.e_orden.get(), s, l)])
        self.cfg["favoritas"] = fav[-20:]
        guardar_config(CONFIG, self.cfg)
        self.msg("Guardada la consulta favorita «%s»" % nombre)

    # ---------------------------------------------------------- dibujo de las tres vistas
    def _dibujar(self):
        self._dibujar_arbol()
        self._dibujar_tabla()
        if self.vista == "json":
            self.mostrar_json()

    def _resumen_doc(self, d):
        partes = []
        for k, v in list(d.items())[:4]:
            if k == "_id":
                continue
            partes.append("%s: %s" % (k, describir(v)))
            if len(partes) == 3:
                break
        return ", ".join(partes)

    def _dibujar_arbol(self):
        L = []
        for i, d in enumerate(self.docs):
            exp = (i,) in self.exp_arbol
            idt = describir(d["_id"]) if "_id" in d else ""
            L.append(("[%s] %s  %s" % ("-" if exp else "+", idt, self._resumen_doc(d)), i, (i,)))
            if exp:
                self._campos_arbol(d, (i,), 1, L, i)
        self.lineas_doc = L
        self.lb_docs.delete(0, "end")
        for t, _i, _r in L:
            self.lb_docs.insert("end", t)
        self.listas["docs"] = (self.lb_docs, [t for t, _i, _r in L])
        if 0 <= self.sel < len(self.docs):
            for k, (t, i, r) in enumerate(L):
                if i == self.sel and len(r) == 1:
                    self.lb_docs.selection_set(k)
                    self.lb_docs.see(k)
                    break

    def _campos_arbol(self, valor, ruta, nivel, L, i):
        for k, v in hijos(valor):
            r = ruta + (k,)
            pad = "    " * nivel
            if es_contenedor(v):
                exp = r in self.exp_arbol
                L.append(("%s[%s] %s: %s" % (pad, "-" if exp else "+", k, describir(v)), i, r))
                if exp:
                    self._campos_arbol(v, r, nivel + 1, L, i)
            else:
                L.append(("%s%s: %s" % (pad, k, describir(v)), i, r))

    def _linea_elegida(self):
        s = self.lb_docs.curselection()
        if s and s[0] < len(self.lineas_doc):
            self.sel = self.lineas_doc[s[0]][1]

    def _linea_doble(self):
        s = self.lb_docs.curselection()
        if not s or s[0] >= len(self.lineas_doc):
            return
        t, i, r = self.lineas_doc[s[0]]
        v = self._valor_en(i, r)
        if es_contenedor(v) or len(r) == 1:
            self.exp_arbol ^= {r}
            self.sel = i
            self._dibujar_arbol()
            for k, (t2, i2, r2) in enumerate(self.lineas_doc):
                if r2 == r:
                    self.lb_docs.selection_clear(0, "end")
                    self.lb_docs.selection_set(k)
                    break
        else:
            self._editar_campo(i, r, v)

    def _valor_en(self, i, ruta):
        v = self.docs[i]
        for k in ruta[1:]:
            v = v[k]
        return v

    def _editar_campo(self, i, ruta, v):
        """Doble clic en un campo escalar del árbol: pide el valor nuevo (JSON o texto) y lo guarda."""
        if any(not isinstance(k, str) for k in ruta[1:]) or ruta[1] == "_id":
            self.msg(_("Ese campo se edita en la vista JSON"))
            return
        nuevo = simpledialog.askstring(_("Editar campo"), "%s:" % ".".join(ruta[1:]), initialvalue=describir(v))
        if nuevo is None:
            return
        try:
            try:
                val = _json.cargar(nuevo)
            except ValueError:
                val = nuevo
            self.bd.actualizar_op(self.col, {"_id": self.docs[i]["_id"]}, {"$set": {".".join(ruta[1:]): val}})
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo guardar"), str(e))
            return
        self.msg("%s: campo %s actualizado" % (self.col, ".".join(ruta[1:])))
        self.buscar()

    def _dibujar_tabla(self):
        cols = []
        for d in self.docs:
            for k in d:
                if k not in cols:
                    cols.append(k)
        if "_id" in cols:
            cols.remove("_id")
            cols.insert(0, "_id")
        nulos = set()
        filas = []
        for i, d in enumerate(self.docs):
            r = []
            for j, c in enumerate(cols):
                if c not in d:
                    r.append("")
                    continue
                v = d[c]
                if v is None:
                    nulos.add((i, j))
                    r.append("null")
                else:
                    r.append(describir(v) if es_contenedor(v) or type(v).__name__ in ("ObjectId", "Fecha") else basedatos.texto_de_valor(v))
            filas.append(r)
        self.rej.col_editables = [c for c in cols if c != "_id"]
        self.rej.poner(cols, filas, nulos=nulos)
        if 0 <= self.sel < len(self.docs):
            self.rej.seleccionar(self.sel)

    def _fila_elegida(self, i):
        if 0 <= i < len(self.docs):
            self.sel = i

    def _celda_editada(self, i, j, texto):
        """Una celda de la tabla: se guarda con $set (solo campos de primer nivel que no sean documentos ni listas)."""
        col = self.rej.columnas[j]
        d = self.docs[i]
        if es_contenedor(d.get(col)):
            self.msg(_("Los documentos y listas se editan en la vista JSON"))
            return False
        try:
            valor = basedatos.valor_desde_texto(texto, None)
            self.bd.actualizar_op(self.col, {"_id": d["_id"]}, {"$set": {col: valor}})
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo guardar"), str(e))
            return False
        self.msg("%s: documento actualizado (%s = %s)" % (self.col, col, texto))
        d[col] = valor
        return True

    # ---------------------------------------------------------- vista JSON y edición de un documento
    def doc_sel(self):
        return self.docs[self.sel] if 0 <= self.sel < len(self.docs) else None

    def mostrar_json(self):
        d = self.doc_sel()
        self.txt_json.delete("1.0", "end")
        if d is None:
            self.l_json.config(text=_("No hay ningún documento elegido"))
            return
        self.txt_json.insert("1.0", _json.texto_json(d))
        if _json.tiene_binarios(d):
            self.l_json.config(text=_("Tiene campos binarios: no se puede guardar desde aquí"))
        else:
            self.l_json.config(text=_("Edita y pulsa «Guardar documento»"))

    def guardar_json(self):
        """Valida el JSON (y que sea un documento) antes de sustituir el documento elegido."""
        d = self.doc_sel()
        if d is None:
            return
        if _json.tiene_binarios(d):
            messagebox.showwarning(_("Guardar documento"), _("El documento tiene campos binarios y JSON no los puede escribir."))
            return
        try:
            nuevo = _json.cargar(self.txt_json.get("1.0", "end-1c"))
        except ValueError as e:
            self.l_json.config(text="JSON no válido: %s" % e)
            messagebox.showerror(_("Guardar documento"), _("El texto no es un JSON válido:\n%s") % e)
            return
        if not isinstance(nuevo, dict):
            messagebox.showerror(_("Guardar documento"), _("Tiene que ser un documento JSON entre llaves."))
            return
        try:
            nuevo = _json.de_json(nuevo, self.bd._m)
            if "_id" in nuevo and nuevo["_id"] != d["_id"]:
                raise ErrorConsulta(_("El _id no se puede cambiar"))
            if self.bd.reemplazar(self.col, d["_id"], nuevo) != 1:
                raise ErrorConsulta(_("El documento ya no existe"))
        except ErrorBD as e:
            messagebox.showerror(_("Guardar documento"), str(e))
            return
        self.msg("%s: documento guardado" % self.col)
        self.buscar()

    # ---------------------------------------------------------- insertar, clonar, borrar
    def _plantilla(self):
        try:
            esq = _muestreo.muestrear(self.bd.consultar(self.col, None, None, 50))
        except ErrorBD:
            esq = []
        valores = {"texto": "", "entero": 0, "real": 0.0, "logico": False, "lista": [], "documento": {}}
        d = {}
        for c in esq:
            if c["campo"] == "_id" or "." in c["campo"]:
                continue
            d[c["campo"]] = valores.get(c["tipos"][0][0])
        return d

    def _dialogo_json(self, titulo, texto, ayuda):
        """Una ventana con un editor de JSON y botones Aceptar/Cancelar. Devuelve el texto o None."""
        d = self.dialogo(titulo)
        tk.Label(d, text=ayuda, anchor="w", justify="left").pack(fill="x", padx=8, pady=(8, 2))
        t = tk.Text(d, height=14, width=64, wrap="none")
        t.pack(padx=8)
        t.insert("1.0", texto)
        res = []

        def aceptar():
            res.append(t.get("1.0", "end-1c"))
            d.destroy()
        fb = tk.Frame(d, pady=8)
        fb.pack()
        tk.Button(fb, text=_("Aceptar"), width=10, command=aceptar).pack(side="left", padx=4)
        tk.Button(fb, text=_("Cancelar"), width=10, command=d.destroy).pack(side="left", padx=4)
        self.esperar_dialogo(d, t)
        return res[0] if res else None

    def insertar_json(self):
        if self.bd is None or not self.col:
            return
        texto = self._dialogo_json(_("Insertar documento en %s") % self.col, _json.texto_json(self._plantilla()),
                                   _("Un documento JSON entre llaves (o una lista de ellos). El _id es opcional."))
        if texto is None:
            return
        try:
            v = _json.cargar(texto)
        except ValueError as e:
            messagebox.showerror(_("Insertar"), _("El texto no es un JSON válido:\n%s") % e)
            return
        lista = v if isinstance(v, list) else [v]
        if not lista or not all(isinstance(x, dict) for x in lista):
            messagebox.showerror(_("Insertar"), _("Tiene que ser un documento JSON entre llaves (o una lista de documentos)."))
            return
        try:
            n = self.bd.insertar_muchos(self.col, [_json.de_json(x, self.bd._m) for x in lista])
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo insertar"), str(e))
            return
        self.msg("%s: %d documentos insertados" % (self.col, n))
        self._tras_escribir()

    def insertar_formulario(self):
        if self.bd is None or not self.col:
            return
        try:
            esq = [c for c in _muestreo.muestrear(self.bd.consultar(self.col, None, None, 50))
                   if c["campo"] != "_id" and "." not in c["campo"]][:10]
        except ErrorBD:
            esq = []
        d = self.dialogo(_("Insertar en %s") % self.col)
        ents = []
        for k, c in enumerate(esq):
            tk.Label(d, text="%s (%s)" % (c["campo"], c["tipos"][0][0]), anchor="w").grid(row=k, column=0, sticky="w", padx=8, pady=2)
            e = tk.Entry(d, width=30)
            e.grid(row=k, column=1, padx=8, pady=2)
            ents.append((c["campo"], e))
        n = len(ents)
        tk.Label(d, text=_("Campo nuevo:"), anchor="w").grid(row=n, column=0, sticky="w", padx=8)
        en = tk.Entry(d, width=14)
        en.grid(row=n, column=1, sticky="w", padx=8)
        ev = tk.Entry(d, width=30)
        ev.grid(row=n + 1, column=1, padx=8)
        tk.Label(d, text=_("Valores: números, true, false, null, texto o JSON"), anchor="w").grid(row=n + 2, column=0, columnspan=2, sticky="w", padx=8)
        ok = []

        def aceptar():
            ok.append(True)
            d.destroy()
        fb = tk.Frame(d, pady=6)
        fb.grid(row=n + 3, column=0, columnspan=2)
        tk.Button(fb, text=_("Insertar"), width=10, command=aceptar).pack(side="left", padx=4)
        tk.Button(fb, text=_("Cancelar"), width=10, command=d.destroy).pack(side="left", padx=4)
        self.esperar_dialogo(d, ents[0][1] if ents else en)
        if not ok:
            return
        doc = {}
        try:
            for campo, e in ents:
                if e.get() != "":
                    doc[campo] = self._valor_libre(e.get())
            if en.get().strip():
                doc[en.get().strip()] = self._valor_libre(ev.get())
            self.bd.insertar(self.col, doc)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo insertar"), str(e))
            return
        self.msg("%s: documento insertado" % self.col)
        self._tras_escribir()

    def _valor_libre(self, texto):
        t = texto.strip()
        if t[:1] in ("{", "[") or (t[:1] == '"' and t[-1:] == '"' and len(t) > 1):
            try:
                return _json.cargar(t)
            except ValueError:
                raise ErrorConsulta(_("«%s» no es un JSON válido") % t)
        return basedatos.valor_desde_texto(texto, None)

    def clonar(self):
        d = self.doc_sel()
        if d is None:
            self.msg(_("Elige primero un documento"))
            return
        if _json.tiene_binarios(d):
            messagebox.showwarning(_("Clonar"), _("El documento tiene campos binarios: no se puede clonar desde aquí."))
            return
        copia = dict((k, v) for k, v in d.items() if k != "_id")
        try:
            self.bd.insertar(self.col, copia)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo clonar"), str(e))
            return
        self.msg("%s: documento clonado" % self.col)
        self._tras_escribir()

    def borrar_documento(self):
        d = self.doc_sel()
        if d is None:
            self.msg(_("Elige primero un documento"))
            return
        if not confirmar_peligro(_("Borrar documento"), _("¿Borrar el documento %s de %s?\nNo se puede deshacer.") % (describir(d["_id"]), self.col)):
            return
        try:
            self.bd.borrar(self.col, clave=d["_id"])
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo borrar"), str(e))
            return
        self.msg("%s: documento borrado" % self.col)
        self._tras_escribir()

    def _filtro_para_masivo(self):
        try:
            return _consulta.filtro(self.e_filtro.get())
        except ErrorBD as e:
            messagebox.showerror(_("Consulta"), str(e))
            return None

    def borrar_filtrados(self):
        """Borra todos los documentos que casan con el filtro, enseñando antes cuántos son (D304)."""
        if self.bd is None or not self.col:
            return
        f = self._filtro_para_masivo()
        if f is None:
            return
        try:
            n = self.bd.contar(self.col, f)
        except ErrorBD as e:
            messagebox.showerror(_("Borrar filtrados"), str(e))
            return
        if n == 0:
            self.msg(_("Ningún documento casa con el filtro"))
            return
        todos = not f
        if not confirmar_peligro(_("Borrar filtrados"), (_("El filtro está vacío: se van a borrar TODOS los %d documentos de %s.") if todos
                                                          else _("Se van a borrar %d documentos de %s.")) % (n, self.col) + "\n" + _("No se puede deshacer.")):
            return
        try:
            n = self.bd.borrar(self.col, f, todos=True)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo borrar"), str(e))
            return
        self.msg("%s: %d documentos borrados" % (self.col, n))
        self._tras_escribir()

    def actualizar_filtrados(self):
        """Aplica unos operadores ({"$set": {...}}) a todos los documentos del filtro, enseñando antes cuántos son."""
        if self.bd is None or not self.col:
            return
        f = self._filtro_para_masivo()
        if f is None:
            return
        try:
            n = self.bd.contar(self.col, f)
        except ErrorBD as e:
            messagebox.showerror(_("Actualizar filtrados"), str(e))
            return
        if n == 0:
            self.msg(_("Ningún documento casa con el filtro"))
            return
        texto = self._dialogo_json(_("Actualizar %d documentos de %s") % (n, self.col), '{"$set": {"campo": 0}}',
                                   _("Se actualizarán %d documentos. Operadores: $set, $unset, $inc, $push, $pull, $rename...") % n)
        if texto is None:
            return
        try:
            op = _json.cargar(texto)
            if not isinstance(op, dict) or not op or not all(k.startswith("$") for k in op):
                raise ErrorConsulta(_("La actualización tiene que usar operadores ($set, $inc...)"))
        except ValueError as e:
            messagebox.showerror(_("Actualizar filtrados"), _("El texto no es un JSON válido:\n%s") % e)
            return
        except ErrorBD as e:
            messagebox.showerror(_("Actualizar filtrados"), str(e))
            return
        if not confirmar_peligro(_("Actualizar filtrados"), (_("El filtro está vacío: se van a actualizar TODOS los %d documentos de %s.") if not f
                                                              else _("Se van a actualizar %d documentos de %s.")) % (n, self.col)):
            return
        try:
            n = self.bd.actualizar_op(self.col, f, _json.de_json(op, self.bd._m))
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo actualizar"), str(e))
            return
        self.msg("%s: %d documentos actualizados" % (self.col, n))
        self._tras_escribir()

    def _tras_escribir(self):
        self.stats.pop((self.base, self.col), None)
        self.buscar()
        self._refrescar_arbol()

    # ---------------------------------------------------------- pestaña Esquema
    def _pag_esquema(self):
        f = self.pags["Esquema"] = tk.Frame(self.zona)
        a = tk.Frame(f)
        a.pack(fill="x", padx=4, pady=4)
        tk.Label(a, text=_("Muestra (documentos):")).pack(side="left")
        self.e_muestra = tk.Entry(a, width=6)
        self.e_muestra.pack(side="left", padx=4)
        self.e_muestra.insert(0, "200")
        self.cajas["muestra"] = self.e_muestra
        tk.Button(a, text=_("Muestrear"), command=self.mostrar_esquema).pack(side="left")
        self.l_esq = tk.Label(a, text="", anchor="w")
        self.l_esq.pack(side="left", padx=8)
        self.rej_esq = Cuadricula(f, ancho=520, alto=300)
        self.rej_esq.pack(fill="both", expand=True, padx=4, pady=(0, 4))
        self.rejillas["rejilla_esq"] = self.rej_esq

    def mostrar_esquema(self):
        if self.bd is None or not self.col:
            self.rej_esq.poner([], [])
            return
        try:
            n = _consulta.entero(self.e_muestra.get(), 200, 1, _("muestra"))
            docs = self.bd.consultar(self.col, None, None, n)
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            return
        res = _muestreo.muestrear(docs)
        filas = []
        for c in res:
            tipos = ", ".join("%s %d%%" % (t, p) for t, p in c["tipos"])
            filas.append([c["campo"], tipos, "%d%%" % c["pct"], str(c["ausente"]),
                          ", ".join("%s (%d)" % (v, k) for v, k in c["frecuentes"])])
        self.rej_esq.poner([_("Campo"), _("Tipos"), _("Presente"), _("Faltan"), _("Valores frecuentes")], filas)
        self.l_esq.config(text=_("Esquema de %s por muestreo de %d documentos") % (self.col, len(docs)))
        self.msg("%s: esquema de %d documentos (%d campos)" % (self.col, len(docs), len(res)))

    # ---------------------------------------------------------- pestaña Índices
    def _pag_indices(self):
        f = self.pags["Índices"] = tk.Frame(self.zona)
        a = tk.Frame(f)
        a.pack(fill="x", padx=4, pady=4)
        tk.Button(a, text=_("Nuevo índice..."), command=self.nuevo_indice).pack(side="left")
        tk.Button(a, text=_("Borrar índice"), command=self.borrar_indice).pack(side="left", padx=4)
        self.l_idx = tk.Label(a, text="", anchor="w")
        self.l_idx.pack(side="left", padx=8)
        self.rej_idx = Cuadricula(f, ancho=520, alto=300)
        self.rej_idx.pack(fill="both", expand=True, padx=4, pady=(0, 4))
        self.rejillas["rejilla_idx"] = self.rej_idx

    def mostrar_indices(self):
        if self.bd is None or not self.col:
            self.rej_idx.poner([], [])
            return
        try:
            idx = self.bd.indices(self.col)
        except ErrorBD as e:
            self.msg("Error: %s" % e)
            return
        si = _("sí")
        self.rej_idx.poner([_("Índice"), _("Campo"), _("Único")],
                           [[x["nombre"], ", ".join(x["columnas"]), si if x["unico"] else ""] for x in idx])
        self.l_idx.config(text="%s: %d índices" % (self.col, len(idx)))

    def nuevo_indice(self):
        if self.bd is None or not self.col:
            return
        d = self.dialogo(_("Nuevo índice en %s") % self.col)
        tk.Label(d, text=_("Campo:"), anchor="w").grid(row=0, column=0, sticky="w", padx=8, pady=4)
        ec = tk.Entry(d, width=24)
        ec.grid(row=0, column=1, padx=8)
        vu, vd = tk.IntVar(value=0), tk.IntVar(value=0)
        tk.Checkbutton(d, text=_("Único"), variable=vu).grid(row=1, column=1, sticky="w", padx=8)
        tk.Checkbutton(d, text=_("Descendente"), variable=vd).grid(row=2, column=1, sticky="w", padx=8)
        ok = []

        def aceptar():
            ok.append((ec.get().strip(), bool(vu.get()), bool(vd.get())))
            d.destroy()
        fb = tk.Frame(d, pady=6)
        fb.grid(row=3, column=0, columnspan=2)
        tk.Button(fb, text=_("Crear"), width=10, command=aceptar).pack(side="left", padx=4)
        tk.Button(fb, text=_("Cancelar"), width=10, command=d.destroy).pack(side="left", padx=4)
        self.esperar_dialogo(d, ec)
        if not ok:
            return
        try:
            campo, unico, desc = ok[0]
            if not campo:
                raise ErrorConsulta(_("falta el campo"))
            nombre = self.bd.crear_indice(self.col, campo, unico, desc)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo crear"), str(e))
            return
        self.msg("Creado el índice %s" % nombre)
        self.mostrar_indices()

    def borrar_indice(self):
        i = self.rej_idx.fila
        if self.bd is None or i < 0 or i >= len(self.rej_idx.filas):
            messagebox.showinfo(_("Borrar índice"), _("Elige primero un índice de la lista."))
            return
        nombre = self.rej_idx.filas[i][0]
        if nombre == "_id_":
            messagebox.showinfo(_("Borrar índice"), _("El índice de _id no se puede borrar."))
            return
        if not confirmar_peligro(_("Borrar índice"), _("¿Borrar el índice «%s»?") % nombre):
            return
        try:
            self.bd.borrar_indice(self.col, nombre)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo borrar"), str(e))
            return
        self.msg("Borrado el índice %s" % nombre)
        self.mostrar_indices()

    # ---------------------------------------------------------- pestaña Agregación
    def _pag_agregacion(self):
        f = self.pags["Agregación"] = tk.Frame(self.zona)
        a = tk.Frame(f)
        a.pack(fill="x", padx=4, pady=(4, 0))
        tk.Label(a, text=_("Añadir etapa:")).pack(side="left")
        for op in ETAPAS:
            tk.Button(a, text=op, command=lambda op=op: self.anadir_etapa(op)).pack(side="left", padx=1)
        m = tk.Frame(f)
        m.pack(fill="x", padx=4, pady=2)
        izq = tk.Frame(m)
        izq.pack(side="left", fill="x", expand=True)
        self.lb_etapas = tk.Listbox(izq, height=5, selectmode="browse")
        self.lb_etapas.pack(fill="x")
        self.lb_etapas.bind("<<ListboxSelect>>", lambda e: self._etapa_elegida())
        self.listas["etapas"] = (self.lb_etapas, [])
        self.txt_etapa = tk.Text(izq, height=4, width=50, wrap="none")
        self.txt_etapa.pack(fill="x", pady=(2, 0))
        self.cajas["etapa"] = self.txt_etapa
        der = tk.Frame(m)
        der.pack(side="left", padx=4, anchor="n")
        for t, cmd in ((_("Aplicar etapa"), self.aplicar_etapa), (_("Quitar etapa"), self.quitar_etapa), (_("Subir"), lambda: self.mover_etapa(-1)),
                       (_("Bajar"), lambda: self.mover_etapa(1)), (_("Ejecutar todo"), self.ejecutar_agregacion)):
            tk.Button(der, text=t, width=14, command=cmd).pack(pady=1)
        self.l_agr = tk.Label(f, text="", anchor="w")
        self.l_agr.pack(side="bottom", fill="x", padx=4, pady=2)
        self.rej_agr = Cuadricula(f, ancho=520, alto=200)
        self.rej_agr.pack(fill="both", expand=True, padx=4)
        self.rejillas["rejilla_agr"] = self.rej_agr

    def _etapas_a_lista(self):
        textos = ["%d. %s %s" % (i + 1, op, cuerpo if len(cuerpo) < 50 else cuerpo[:47] + "...") for i, (op, cuerpo) in enumerate(self.etapas)]
        self.lb_etapas.delete(0, "end")
        for t in textos:
            self.lb_etapas.insert("end", t)
        self.listas["etapas"] = (self.lb_etapas, textos)
        if 0 <= self.sel_etapa < len(self.etapas):
            self.lb_etapas.selection_set(self.sel_etapa)

    def anadir_etapa(self, op):
        self.etapas.append((op, PLANTILLAS[op]))
        self.sel_etapa = len(self.etapas) - 1
        self._etapas_a_lista()
        self._poner_etapa()
        self.vista_previa()

    def _poner_etapa(self):
        self.txt_etapa.delete("1.0", "end")
        if 0 <= self.sel_etapa < len(self.etapas):
            self.txt_etapa.insert("1.0", self.etapas[self.sel_etapa][1])

    def _etapa_elegida(self):
        s = self.lb_etapas.curselection()
        if s and s[0] < len(self.etapas):
            self.sel_etapa = s[0]
            self._poner_etapa()
            self.vista_previa()

    def aplicar_etapa(self):
        if not (0 <= self.sel_etapa < len(self.etapas)):
            return
        t = self.txt_etapa.get("1.0", "end-1c").strip()
        try:
            _json.cargar(t)
        except ValueError as e:
            self.l_agr.config(text="JSON no válido: %s" % e)
            messagebox.showerror(_("Etapa"), _("El texto no es un JSON válido:\n%s") % e)
            return
        self.etapas[self.sel_etapa] = (self.etapas[self.sel_etapa][0], t)
        self._etapas_a_lista()
        self.vista_previa()

    def quitar_etapa(self):
        if 0 <= self.sel_etapa < len(self.etapas):
            del self.etapas[self.sel_etapa]
            self.sel_etapa = min(self.sel_etapa, len(self.etapas) - 1)
            self._etapas_a_lista()
            self._poner_etapa()
            self.vista_previa()

    def mover_etapa(self, d):
        k = self.sel_etapa
        j = k + d
        if 0 <= k < len(self.etapas) and 0 <= j < len(self.etapas):
            self.etapas[k], self.etapas[j] = self.etapas[j], self.etapas[k]
            self.sel_etapa = j
            self._etapas_a_lista()
            self.vista_previa()

    def _pipeline(self, hasta):
        p = []
        for op, cuerpo in self.etapas[:hasta]:
            p.append({op: _json.de_json(_json.cargar(cuerpo), self.bd._m)})
        return p

    def vista_previa(self):
        """Resultado del pipeline hasta la etapa elegida (con un límite de 50 documentos)."""
        self._agregar(self.sel_etapa + 1, 50)

    def ejecutar_agregacion(self):
        self._agregar(len(self.etapas), 200)

    def _agregar(self, hasta, limite):
        if self.bd is None or not self.col:
            return
        if hasta <= 0:
            self.rej_agr.poner([], [])
            self.l_agr.config(text=_("Añade una etapa para ver su resultado"))
            return
        try:
            p = self._pipeline(hasta)
            t0 = ahora_ms()
            docs = self.bd.agregar(self.col, p + [{"$limit": limite}])
        except ValueError as e:
            self.l_agr.config(text="JSON no válido: %s" % e)
            return
        except ErrorBD as e:
            self.l_agr.config(text="Error: %s" % e)
            self.msg("Error: %s" % e)
            return
        cols = []
        for d in docs:
            for k in d:
                if k not in cols:
                    cols.append(k)
        self.rej_agr.poner(cols, [[describir(d[c]) if (c in d and es_contenedor(d[c])) else (basedatos.texto_de_valor(d[c]) if c in d else "") for c in cols] for d in docs])
        r = "%d documentos tras %d etapas (%d ms)" % (len(docs), hasta, ahora_ms() - t0)
        self.l_agr.config(text=r)
        self.msg("Agregación: %s" % r)

    # ---------------------------------------------------------- el árbol del servidor
    def _refrescar_arbol(self):
        if self.bd is None and not self.bases:
            self.poner_arbol([])
            return
        L = [("[-] %s  (lite-monkey)" % self.servidor, ("srv",))]
        for b, cols in self.bases:
            exp = self.exp_base.get(b)
            L.append(("   [%s] %s" % ("-" if exp else "+", b), ("base", b)))
            if exp:
                for c in cols:
                    n = self.stats.get((b, c))
                    suf = ""
                    if n is not None:
                        suf = "  (%d, %s)" % (n[0], tamano_legible(n[1]))
                    L.append(("        %s%s" % (c, suf), ("col", b, c)))
        self.poner_arbol(L)

    def _estadisticas(self, b, c):
        if (b, c) in self.stats:
            return
        try:
            self.bd.usar_base(b)
            e = self.bd.estadisticas(c)
            self.stats[(b, c)] = (e["n"], e["tam"])
        except ErrorBD:
            self.stats[(b, c)] = (0, 0)

    def al_elegir_nodo(self, n):
        if n[0] == "col":
            self.elegir_coleccion(n[1], n[2])
        elif n[0] == "base":
            self.base = n[1]
            self.msg("%s: %d colecciones" % (n[1], len(dict(self.bases).get(n[1], []))))
        elif n[0] == "srv":
            self.msg(self.servidor)

    def al_alternar_nodo(self, n):
        if n[0] == "base":
            self.exp_base[n[1]] = not self.exp_base.get(n[1])
            if self.exp_base[n[1]]:
                for c in dict(self.bases).get(n[1], []):
                    self._estadisticas(n[1], c)
            self._refrescar_arbol()
            self.elegir_nodo(lambda x: x == n)
        elif n[0] == "srv":
            self.cerrar_conexion()

    def menu_nodo(self, n, m):
        if n is not None and n[0] == "col":
            m.add_command(label=_("Documentos"), command=lambda: self.ir_pestana("Documentos"))
            m.add_command(label=_("Borrar colección"), command=self.borrar_coleccion)
        elif n is not None and n[0] == "base":
            m.add_command(label=_("Nueva colección..."), command=self.nueva_coleccion)
            m.add_command(label=_("Borrar base"), command=self.borrar_base)
        elif n is not None and n[0] == "srv":
            m.add_command(label=_("Cerrar conexión"), command=self.cerrar_conexion)
        m.add_separator()
        m.add_command(label=_("Conectar a lite-monkey..."), command=self.conectar_dialogo)

    def elegir_coleccion(self, b, c):
        self.base, self.col = b, c
        self.bd.usar_base(b)
        self.docs, self.sel = [], -1
        self.restablecer()
        if self.pestana == "Esquema":
            self.mostrar_esquema()
        elif self.pestana == "Índices":
            self.mostrar_indices()
        else:
            self.mostrar_esquema_silencioso()

    def mostrar_esquema_silencioso(self):
        """Prepara el esquema y los índices sin cambiar de pestaña (se enseñan al abrirlas)."""
        pass

    def actualizar(self):
        if self.bd is None:
            return
        self.stats = {}
        self.refrescar_servidor()
        if self.col:
            self.buscar()

    # ---------------------------------------------------------- conexión
    def conectar_dialogo(self):
        d = self.dialogo(_("Conectar a lite-monkey"))
        tk.Label(d, text=_("Servidor:")).grid(row=0, column=0, sticky="w", padx=8, pady=4)
        e1 = tk.Entry(d, width=24)
        e1.grid(row=0, column=1, padx=8)
        e1.insert(0, "127.0.0.1:%d" % PUERTO_LMK)
        tk.Label(d, text=_("Usuario:")).grid(row=1, column=0, sticky="w", padx=8, pady=4)
        e2 = tk.Entry(d, width=24)
        e2.grid(row=1, column=1, padx=8)
        tk.Label(d, text=_("Clave:")).grid(row=2, column=0, sticky="w", padx=8, pady=4)
        e3 = tk.Entry(d, width=24, show="*")
        e3.grid(row=2, column=1, padx=8)
        ok = []

        def aceptar(ev=None):
            ok.append((e1.get().strip(), e2.get().strip() or None, e3.get() or None))
            d.destroy()
        fb = tk.Frame(d, pady=6)
        fb.grid(row=3, column=0, columnspan=2)
        tk.Button(fb, text=_("Conectar"), width=10, command=aceptar).pack(side="left", padx=4)
        tk.Button(fb, text=_("Cancelar"), width=10, command=d.destroy).pack(side="left", padx=4)
        for e in (e1, e2, e3):
            e.bind("<Return>", aceptar)
        self.esperar_dialogo(d, e1)
        if ok:
            self.conectar(*ok[0])

    def _origen(self, servidor, usuario, clave, base="test"):
        cred = ""
        if usuario:
            cred = usuario + (":" + clave if clave else "") + "@"
        return "litemonkey://%s%s/%s" % (cred, servidor, base)

    def conectar(self, servidor, usuario=None, clave=None, base="test"):
        """Conecta con el servidor (y ofrece arrancar el servicio si no contesta) y lista sus bases."""
        origen = self._origen(servidor, usuario, clave, base)
        try:
            bd = basedatos.conectar(origen)
        except basedatos.ErrorConexion as e:
            if self._ofrecer_arrancar():
                try:
                    bd = basedatos.conectar(origen)
                except ErrorBD as e2:
                    messagebox.showerror("lite-monkey", str(e2))
                    return False
            else:
                self.msg("Error: %s" % e)
                return False
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo conectar"), str(e))
            self.msg("Error: %s" % e)
            return False
        self.cerrar_conexion()
        self.bd, self.servidor, self.credenciales = bd, servidor, (usuario, clave)
        self.base, self.col = None, None
        self.raiz.title("%s - %s" % (_("Explorador lite-monkey"), servidor))
        sv = [s for s in self.cfg.get("servidores", []) if s != servidor] + [servidor]
        self.cfg["servidores"] = sv[-8:]
        guardar_config(CONFIG, self.cfg)
        self.refrescar_servidor()
        self.msg("Abierta %s (%d bases)" % (servidor, len(self.bases)))
        return True

    def refrescar_servidor(self):
        try:
            self.bases = self.bd.bases_con_colecciones()
        except ErrorBD as e:
            self.bases = []
            self.msg("Error: %s" % e)
        for b, cols in self.bases:
            if self.exp_base.get(b):
                for c in cols:
                    self._estadisticas(b, c)
        self._refrescar_arbol()

    def _ofrecer_arrancar(self):
        """lite-monkey no contesta: ofrece arrancarlo con el gestor de servicios."""
        if not messagebox.askyesno("lite-monkey", _("El servidor lite-monkey no está arrancado.\n¿Quieres arrancarlo ahora?")):
            return False
        self.msg("Arrancando el servicio litemonkey...")
        os.system("servicio iniciar litemonkey")
        if self._esperar_servidor():
            return True
        os.system("litemonkey &")   # sin el servicio: a mano (los datos, en /home/litemonkey o en /tmp/litemonkey)
        if self._esperar_servidor():
            return True
        messagebox.showerror("lite-monkey", _("No se pudo arrancar el servidor.\nPrueba: servicio iniciar litemonkey"))
        return False

    def _esperar_servidor(self):
        import socket
        for _i in range(25):
            try:
                s = socket.socket()
                s.settimeout(0.5)
                s.connect(("127.0.0.1", PUERTO_LMK))
                s.close()
                return True
            except Exception:
                pausa(200)
        return False

    def cerrar_conexion(self):
        if self.bd is not None:
            try:
                self.bd.cerrar()
            except Exception:
                pass
        self.bd = None
        self.bases, self.stats, self.docs = [], {}, []
        self.base = self.col = None
        self.sel = -1
        self.servidor = ""
        self.poner_arbol([])
        self.rej.poner([], [])
        self.lb_docs.delete(0, "end")
        self.raiz.title(_("Explorador lite-monkey"))

    # ---------------------------------------------------------- colecciones y bases
    def nueva_coleccion(self):
        if self.bd is None:
            messagebox.showinfo(_("Nueva colección"), _("Conéctate primero a un servidor."))
            return
        txt = simpledialog.askstring(_("Nueva colección"), _("Nombre (base.colección, o solo la colección en la base elegida):"))
        if not txt:
            return
        b, _s, c = txt.strip().partition(".")
        if not c:
            b, c = self.base or "test", b
        try:
            self.bd.usar_base(b)
            self.bd.crear_tabla(c)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo crear"), str(e))
            return
        self.exp_base[b] = True
        self.msg("Creada %s.%s" % (b, c))
        self.refrescar_servidor()

    def borrar_coleccion(self):
        if self.bd is None or not self.col:
            return
        if not confirmar_peligro(_("Borrar colección"), _("¿Borrar la colección «%s.%s» con todos sus documentos?\nNo se puede deshacer.") % (self.base, self.col)):
            return
        try:
            self.bd.usar_base(self.base)
            self.bd.borrar_tabla(self.col)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo borrar"), str(e))
            return
        self.msg("Borrada %s.%s" % (self.base, self.col))
        self.col = None
        self.docs, self.sel = [], -1
        self.rej.poner([], [])
        self.lb_docs.delete(0, "end")
        self.refrescar_servidor()

    def borrar_base(self):
        if self.bd is None or not self.base:
            messagebox.showinfo(_("Borrar base"), _("Elige primero una base en el árbol."))
            return
        if not confirmar_peligro(_("Borrar base"), _("¿Borrar la base «%s» con todas sus colecciones?\nNo se puede deshacer.") % self.base):
            return
        try:
            self.bd.borrar_base(self.base)
        except ErrorBD as e:
            messagebox.showerror(_("No se pudo borrar"), str(e))
            return
        self.msg("Borrada la base %s" % self.base)
        self.base = self.col = None
        self.docs, self.sel = [], -1
        self.rej.poner([], [])
        self.refrescar_servidor()

    # ---------------------------------------------------------- importar y exportar
    def _todos_los_documentos(self):
        """Los documentos de la consulta actual (sin límite ni salto), hasta 100000."""
        f, p, o, _s, _l = self._leer_consulta()
        r, pos = [], 0
        while len(r) < 100000:
            lote = self.bd.consultar_proy(self.col, f, p, o, 200, pos)
            if not lote:
                break
            r += lote
            pos += 200
            if len(lote) < 200:
                break
        return r

    def exportar(self, formato, ruta=None):
        """Exporta la colección (con el filtro y el orden de la barra) a JSON, JSON por líneas o CSV aplanado."""
        if self.bd is None or not self.col:
            messagebox.showinfo(_("Exportar"), _("Elige primero una colección."))
            return None
        try:
            docs = self._todos_los_documentos()
        except ErrorBD as e:
            messagebox.showerror(_("Exportar"), str(e))
            return None
        if formato == "json":
            texto = "[\n" + ",\n".join(json.dumps(_json.a_json(d)) for d in docs) + "\n]\n"
            ext, tipo = ".json", "JSON"
        elif formato == "jsonl":
            texto = "".join(json.dumps(_json.a_json(d)) + "\n" for d in docs)
            ext, tipo = ".jsonl", "JSON por líneas"
        else:
            cols = []
            for d in docs:
                for k in d:
                    if k not in cols:
                        cols.append(k)
            texto = basedatos.a_csv(cols, [[basedatos.texto_de_valor(d[c]) if c in d and not type(d[c]).__name__ in ("ObjectId", "Fecha")
                                           else (str(d[c]) if c in d else None) for c in cols] for d in docs])
            ext, tipo = ".csv", "CSV"
        if ruta is None:
            ruta = filedialog.asksaveasfilename(title=_("Exportar a %s") % tipo, initialdir="/home", initialfile=self.col + ext,
                                                defaultextension=ext, filetypes=[(tipo, "*" + ext), (_("Todos"), "*")])
        if not ruta:
            return None
        try:
            with open(ruta, "w") as f:
                f.write(texto)
        except OSError as e:
            messagebox.showerror(_("Exportar"), "No se pudo escribir %s: %s" % (ruta, e))
            return None
        self.msg("Exportados %d documentos a %s" % (len(docs), ruta))
        return ruta

    def importar_json(self, ruta=None):
        """Archivo > Importar JSON: una lista JSON o un documento por línea; entra en la colección elegida."""
        if self.bd is None or not self.col:
            messagebox.showinfo(_("Importar"), _("Elige primero la colección donde importar."))
            return
        if ruta is None:
            ruta = filedialog.askopenfilename(title=_("Importar JSON"), initialdir="/home",
                                              filetypes=[("JSON", "*.json *.jsonl *.ndjson"), (_("Todos"), "*")])
        if not ruta:
            return
        try:
            with open(ruta) as f:
                texto = f.read()
        except OSError as e:
            messagebox.showerror(_("Importar"), "No se pudo leer %s: %s" % (ruta, e))
            return
        try:
            t = texto.strip()
            if t.startswith("["):
                docs = _json.cargar(t)
            else:
                docs = [_json.cargar(l) for l in t.split("\n") if l.strip()]
            if not all(isinstance(d, dict) for d in docs):
                raise ErrorConsulta(_("todos los elementos tienen que ser documentos entre llaves"))
        except ValueError as e:
            messagebox.showerror(_("Importar"), _("El archivo no es un JSON válido:\n%s") % e)
            return
        except ErrorBD as e:
            messagebox.showerror(_("Importar"), str(e))
            return
        if not docs:
            messagebox.showinfo(_("Importar"), _("El archivo no tiene documentos."))
            return
        if not messagebox.askyesno(_("Importar"), _("¿Importar %d documentos en «%s.%s»?") % (len(docs), self.base, self.col)):
            return
        try:
            n = 0
            for k in range(0, len(docs), 100):
                n += self.bd.insertar_muchos(self.col, [_json.de_json(d, self.bd._m) for d in docs[k:k + 100]])
        except ErrorBD as e:
            messagebox.showerror(_("Importar"), str(e))
            return
        self.msg("Importados %d documentos en %s" % (n, self.col))
        self._tras_escribir()

    # ---------------------------------------------------------- salida
    def salir(self):
        self.cerrar_conexion()
        self.raiz.destroy()


def principal(argv):
    raiz = tk.Tk()
    ex = ExploradorLMK(raiz)
    for a in argv[1:]:
        if a == "--sonda":
            ex.sonda = True
            continue
        if a.startswith("litemonkey://") or a.startswith("lmk://"):
            try:
                tipo, p = basedatos.analizar(a)
            except ErrorBD as e:
                print("exploradorbd: Error: %s" % e)
                continue
            host, puerto, base, usuario, clave = p
            if ex.conectar("%s:%d" % (host, puerto), usuario, clave, base):
                ex.exp_base[base] = True
                for c in dict(ex.bases).get(base, []):
                    ex._estadisticas(base, c)
                ex._refrescar_arbol()
    raiz.mainloop()


if __name__ == "__main__":
    principal(sys.argv)
