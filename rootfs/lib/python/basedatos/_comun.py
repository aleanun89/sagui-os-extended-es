# SPDX-License-Identifier: GPL-2.0-or-later
# basedatos._comun - lo que comparten el Explorador SQLite y el Explorador lite-monkey (D333): la ventana con su
# árbol a la izquierda y las pestañas a la derecha, la configuración, los diálogos de confirmación y la sonda
# (F12) que imprime las posiciones de los componentes para las pruebas automáticas.
import os
import json
import tkinter as tk
from tkinter import messagebox
from basedatos._idioma import _

try:
    import sagui
except ImportError:
    sagui = None

PUERTO_LMK = 7979


def ahora_ms():
    if sagui is not None:
        return sagui.ticks_ms()
    import time
    return int(time.time() * 1000)


def pausa(ms):
    if sagui is not None:
        sagui.dormir(ms)
    else:
        import time
        time.sleep(ms / 1000)


def leer_config(ruta, defecto):
    try:
        with open(ruta) as f:
            c = json.loads(f.read())
        if isinstance(c, dict):
            for k, v in defecto.items():
                c.setdefault(k, v)
            return c
    except Exception:
        pass
    return dict(defecto)


def guardar_config(ruta, c):
    try:
        d = os.path.dirname(ruta)
        try:
            os.mkdir(d)
        except OSError:
            pass
        with open(ruta, "w") as f:
            f.write(json.dumps(c))
    except Exception:
        pass


def confirmar_peligro(titulo, texto):
    """Pregunta por algo que no se puede deshacer: el botón por defecto es «No» (D304)."""
    return messagebox.askyesno(titulo, texto, default="no")


class Cancelado(Exception):
    pass


class VentanaBD:
    """Ventana base: menú, árbol a la izquierda, botones de pestañas y una zona de páginas a la derecha, y
    una línea de estado. Las subclases rellenan `PAGINAS`, `_menus()` y las `_pag_*()`."""
    TITULO = ""
    PAGINAS = ()
    GEOMETRIA = "880x580"
    ANCHO_ARBOL = 250

    def __init__(self, raiz):
        self.raiz = raiz
        self.nodos = []              # lo que enseña el árbol: tuplas cuyo primer elemento es el tipo
        self._lineas_arbol = []
        self.pags = {}
        self.rejillas = {}           # nombre -> Cuadricula (la sonda las describe)
        self.cajas = {}              # nombre -> Entry o Text (la sonda da su posición)
        self.listas = {}             # nombre -> (Listbox, [textos]) (la sonda da la posición de cada línea)
        self.pestana = ""
        self.sonda = False
        self._construir()

    # ---------------------------------------------------------- ventana
    def _construir(self):
        r = self.raiz
        r.title(self.TITULO)
        r.geometry(self.GEOMETRIA)
        r.minsize(600, 380)
        self._menus()
        self.estado = tk.Label(r, text="", anchor="w", relief="sunken", bd=1)
        self.estado.pack(side="bottom", fill="x")
        self.barra_herr = tk.Frame(r)
        self.barra_herr.pack(side="top", fill="x", padx=4, pady=(2, 0))
        cuerpo = tk.Frame(r)
        cuerpo.pack(fill="both", expand=True)
        izq = tk.Frame(cuerpo, width=self.ANCHO_ARBOL)
        izq.pack(side="left", fill="y")
        izq.pack_propagate(False)
        tk.Label(izq, text=self.TITULO_ARBOL(), anchor="w").pack(fill="x", padx=4)
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
        self.arbol.bind("<Button-3>", self._arbol_clic_derecho)
        der = tk.Frame(cuerpo)
        der.pack(side="left", fill="both", expand=True)
        barra = tk.Frame(der)
        barra.pack(fill="x", padx=4, pady=(4, 0))
        self.botones_pest = {}
        for n in self.PAGINAS:
            b = tk.Button(barra, text=_(n), width=11, command=lambda n=n: self.ir_pestana(n))
            b.pack(side="left")
            self.botones_pest[n] = b
        self.zona = tk.Frame(der, relief="sunken", bd=2)
        self.zona.pack(fill="both", expand=True, padx=4, pady=(0, 4))
        self._paginas()
        self.ir_pestana(self.PAGINAS[0])
        r.bind("<F12>", lambda e: self.imprimir_sonda())
        r.protocol("WM_DELETE_WINDOW", self.salir)

    def TITULO_ARBOL(self):
        return _("Conexiones")

    def _menus(self):
        pass

    def _paginas(self):
        pass

    def ir_pestana(self, n):
        self.pestana = n
        for k, f in self.pags.items():
            f.pack_forget()
        self.pags[n].pack(fill="both", expand=True)
        for k, b in self.botones_pest.items():
            b.config(relief="sunken" if k == n else "raised")
        self.al_cambiar_pestana(n)

    def al_cambiar_pestana(self, n):
        pass

    def msg(self, t):
        self.estado.config(text=t)
        print("exploradorbd:", t)

    # ---------------------------------------------------------- el árbol
    def poner_arbol(self, lineas):
        """lineas: [(texto, nodo)]; el nodo es una tupla cuyo primer elemento es su tipo."""
        self.nodos = [n for _t, n in lineas]
        self._lineas_arbol = [t for t, _n in lineas]
        self.arbol.delete(0, "end")
        for t in self._lineas_arbol:
            self.arbol.insert("end", t)

    def nodo_actual(self):
        s = self.arbol.curselection()
        if not s or s[0] >= len(self.nodos):
            return None
        return self.nodos[s[0]]

    def elegir_nodo(self, pred):
        """Deja elegido el primer nodo para el que `pred(nodo)` es verdad."""
        for k, n in enumerate(self.nodos):
            if pred(n):
                self.arbol.selection_clear(0, "end")
                self.arbol.selection_set(k)
                self.arbol.see(k)
                return True
        return False

    def _arbol_elegido(self):
        n = self.nodo_actual()
        if n is not None:
            self.al_elegir_nodo(n)

    def _arbol_alternar(self):
        n = self.nodo_actual()
        if n is not None:
            self.al_alternar_nodo(n)

    def _arbol_clic_derecho(self, ev):
        k = self.arbol.nearest(ev.y)
        if 0 <= k < len(self.nodos):
            self.arbol.selection_clear(0, "end")
            self.arbol.selection_set(k)
            self._arbol_elegido()
        m = tk.Menu(self.raiz, tearoff=0)
        self.menu_nodo(self.nodo_actual(), m)
        m.tk_popup(self.raiz.winfo_pointerx(), self.raiz.winfo_pointery())

    def al_elegir_nodo(self, n):
        pass

    def al_alternar_nodo(self, n):
        pass

    def menu_nodo(self, n, m):
        pass

    # ---------------------------------------------------------- diálogos
    def dialogo(self, titulo):
        """Una ventana de diálogo modal con su propia sonda (F12)."""
        d = tk.Toplevel(self.raiz, _dialogo=True)
        d.title(titulo)
        d.transient(self.raiz)
        d.resizable(False, False)
        d.bind("<F12>", lambda e: self.imprimir_sonda_dialogo(d))
        return d

    def esperar_dialogo(self, d, foco=None):
        if foco is not None:
            foco.focus_set()
        d.grab_set()
        self.raiz.wait_window(d)

    # ---------------------------------------------------------- sonda (F12)
    def _botones(self, w, L, base):
        for h in w.winfo_children():
            if isinstance(h, tk.Button) and h.winfo_ismapped():
                L.append("sonda: boton|%s|%d|%d" % (h.cget("text").replace("|", "I"),
                                                     h.winfo_rootx() + h.winfo_width() // 2 - base[0],
                                                     h.winfo_rooty() + h.winfo_height() // 2 - base[1]))
            elif isinstance(h, tk.Checkbutton) and h.winfo_ismapped() and h.cget("text"):
                L.append("sonda: boton|%s|%d|%d" % (h.cget("text").replace("|", "I"),
                                                     h.winfo_rootx() + 8 - base[0], h.winfo_rooty() + h.winfo_height() // 2 - base[1]))
            self._botones(h, L, base)

    def imprimir_sonda(self):
        """F12: las posiciones (relativas al contenido de la ventana) de lo que las pruebas pulsan. Se escribe de
        una vez para que el terminal no la parta con su línea de órdenes."""
        L = ["vps-sonda: inicio"]
        ax, ay = self.arbol.winfo_rootx(), self.arbol.winfo_rooty()
        L.append("sonda: arbol|%d|%d|%d|%d" % (ax, ay, self.arbol.winfo_width(), self.arbol.winfo_height()))
        for k, l in enumerate(self._lineas_arbol):
            L.append("sonda: nodo|%d|%d|%d|%s" % (k, ax + 60, ay + 12 + 18 * k, l.strip()))
        self._botones(self.raiz, L, (0, 0))
        for nombre, g in self.rejillas.items():
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
        for nombre, c in self.cajas.items():
            if c.winfo_ismapped():
                x, y = c.winfo_rootx() + min(40, c.winfo_width() // 2), c.winfo_rooty() + min(12, c.winfo_height() // 2)
                L.append("sonda: caja|%s|%d|%d" % (nombre, x, y))
                if nombre == "sql":
                    L.append("sonda: texto|%d|%d" % (x, y))      # nombre que usan las pruebas de la mejora 127
        for nombre, (lb, textos) in self.listas.items():
            if lb.winfo_ismapped():
                lx, ly = lb.winfo_rootx(), lb.winfo_rooty()
                for k, t in enumerate(textos[:14]):
                    L.append("sonda: linea|%s|%d|%d|%d|%s" % (nombre, k, lx + 60, ly + 12 + 18 * k, t.strip()))
        L.append("sonda: fin")
        print("\n".join(L))

    def imprimir_sonda_dialogo(self, d):
        """F12 en un diálogo: botones, entradas y casillas con su posición dentro del diálogo."""
        L = ["vps-sonda: inicio"]
        self._botones(d, L, (0, 0))
        k = [0, 0]

        def recorrer(w):
            for h in w.winfo_children():
                if not h.winfo_ismapped():
                    recorrer(h)
                    continue
                if isinstance(h, tk.Entry):
                    L.append("sonda: entrada|%d|%d|%d|%s" % (k[0], h.winfo_rootx() + 8, h.winfo_rooty() + 8, h.get()))
                    k[0] += 1
                elif isinstance(h, tk.Checkbutton):
                    L.append("sonda: casilla|%d|%d|%d|%s" % (k[1], h.winfo_rootx() + 8, h.winfo_rooty() + 8, h.cget("text")))
                    k[1] += 1
                elif isinstance(h, tk.Text):
                    L.append("sonda: texto|%d|%d" % (h.winfo_rootx() + 40, h.winfo_rooty() + 20))
                recorrer(h)
        recorrer(d)
        L.append("sonda: fin")
        print("\n".join(L))

    def salir(self):
        self.raiz.destroy()
