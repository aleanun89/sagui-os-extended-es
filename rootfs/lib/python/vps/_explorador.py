# SPDX-License-Identifier: GPL-2.0-or-later
# vps._explorador - el explorador del proyecto de VisualPythonS (v8.4.1, D315): un árbol
# Proyecto > Formularios > controles (con sus Marcos y las páginas de las Pestanas) y, al final,
# «Código» con los manejadores de eventos que ya existen en codigo.py. Un clic selecciona en el
# diseñador, el doble clic abre el código y el clic derecho da el menú contextual. Aspecto Win98.
#
# `construir` es una función pura (sin tkinter) para poder probarla sin pantalla.
import sagui
import tkinter as tk
import vps
from vps import _modelo as M
from vps._superficie import hundido, elevado, recortar, icono, GRIS, BLANCO, SOMBRA, NEGRO, AZUL, LUZ

ALTO_FILA = 18
SANGRIA = 14


# ------------------------------------------------------------ el árbol (sin tkinter)

def _nodo(id, k, t, tipo="", f=-1, c=None, pag=0, linea=0):
    return {"id": id, "k": k, "t": t, "tipo": tipo, "f": f, "c": c, "pag": pag, "linea": linea, "hijos": []}


def manejadores(forms, codigo):
    """[(nombre_de_la_función, línea)] de las funciones «Control_evento» de codigo.py que corresponden
    a un formulario, un control o un elemento del menú del proyecto."""
    from vps._editor import nombres_de_codigo
    conocidos = {}
    for f in forms:
        evs = list(vps.Formulario.EVENTOS)
        conocidos[f["nombre"]] = evs
        for n in nombres_de_codigo(f):
            if n == f["nombre"]:
                continue
            c = M.por_nombre(f, n)
            conocidos[n] = list(vps.TIPOS[c["tipo"]].EVENTOS) if c is not None else ["click"]
    r = []
    orden = sorted(conocidos, key=len, reverse=True)       # el nombre más largo primero
    for i, l in enumerate(codigo.split("\n")):
        if not l.startswith("def "):
            continue
        nombre = l[4:].split("(")[0].strip()
        for n in orden:
            if nombre.startswith(n + "_") and nombre[len(n) + 1:] in conocidos[n]:
                r.append((nombre, i + 1))
                break
    return r


def arbol(forms, titulo, codigo):
    """La raíz del árbol completo (sin filtrar): Proyecto > Formularios > ..., y Código."""
    raiz = _nodo("p", "proyecto", titulo)
    grupo = _nodo("fs", "grupo", "Formularios")
    raiz["hijos"].append(grupo)
    for fi, f in enumerate(forms):
        nf = _nodo("f:" + f["nombre"], "form", f["nombre"], "Formulario", fi, f["nombre"])
        grupo["hijos"].append(nf)
        _hijos(f, fi, None, nf)
    cod = _nodo("cod", "codigo", "Código")
    for nombre, linea in manejadores(forms, codigo):
        cod["hijos"].append(_nodo("m:%s" % nombre, "manejador", nombre + "()", "", -1, nombre, 0, linea))
    raiz["hijos"].append(cod)
    return raiz


def _hijos(form, fi, padre, nodo):
    """Cuelga de `nodo` los controles del formulario cuyo contenedor es `padre` (nombre o None)."""
    for c in form["controles"]:
        if c["padre"] != padre:
            continue
        n = _nodo("c:" + c["nombre"], "ctl", c["nombre"], c["tipo"], fi, c["nombre"])
        nodo["hijos"].append(n)
        if c["tipo"] == "Pestanas":
            for pi, t in enumerate(M.val(c, "elementos") or []):
                pg = _nodo("pg:%s/%d" % (c["nombre"], pi), "pagina", str(t), "pagina", fi, c["nombre"], pi)
                n["hijos"].append(pg)
                for h in form["controles"]:
                    if h["padre"] == c["nombre"] and int(M.val(h, "pagina") or 0) == pi:
                        nh = _nodo("c:" + h["nombre"], "ctl", h["nombre"], h["tipo"], fi, h["nombre"])
                        pg["hijos"].append(nh)
                        _hijos(form, fi, h["nombre"], nh)
        else:
            _hijos(form, fi, c["nombre"], n)


def filtrar(nodo, texto):
    """Copia del árbol con solo los nodos cuyo texto contiene `texto` y sus ascendientes."""
    t = texto.strip().lower()
    if not t:
        return nodo
    hijos = [h2 for h2 in (filtrar(h, t) for h in nodo["hijos"]) if h2 is not None]
    if hijos or t in nodo["t"].lower():
        n = dict(nodo)
        n["hijos"] = hijos
        return n
    return None


def aplanar(nodo, cerrados, filtro=False, nivel=0, salida=None):
    """Las filas visibles: [(nivel, nodo)]. Con filtro, todo se enseña abierto."""
    if salida is None:
        salida = []
    salida.append((nivel, nodo))
    if nodo["hijos"] and (filtro or nodo["id"] not in cerrados):
        for h in nodo["hijos"]:
            aplanar(h, cerrados, filtro, nivel + 1, salida)
    return salida


# ------------------------------------------------------------ iconos pequeños (16x16)

def _icono_nodo(v, nodo, x, y):
    k = nodo["k"]
    if k == "proyecto":
        v.rect(x + 1, y + 4, 14, 10, 0xFFD040)
        v.rect(x + 1, y + 2, 6, 3, 0xFFD040)
        v.rect(x + 1, y + 4, 14, 10, NEGRO, relleno=False)
    elif k == "grupo":
        v.rect(x + 1, y + 4, 14, 10, 0xFFE890)
        v.rect(x + 1, y + 4, 14, 10, SOMBRA, relleno=False)
    elif k == "form":
        v.rect(x + 1, y + 2, 14, 13, GRIS)
        v.rect(x + 2, y + 3, 12, 3, AZUL)
        v.rect(x + 1, y + 2, 14, 13, NEGRO, relleno=False)
    elif k == "codigo":
        v.rect(x + 3, y + 1, 10, 14, BLANCO)
        v.rect(x + 3, y + 1, 10, 14, NEGRO, relleno=False)
        for yy in (4, 7, 10):
            v.linea(x + 5, y + yy, x + 10, y + yy, SOMBRA)
    elif k == "manejador":
        v.rect(x + 3, y + 2, 10, 12, 0xFFFFC0)
        v.rect(x + 3, y + 2, 10, 12, NEGRO, relleno=False)
        v.texto(x + 5, y, "f", 0x0000A0)
    elif k == "pagina":
        v.rect(x + 1, y + 6, 14, 9, GRIS)
        v.rect(x + 1, y + 3, 7, 4, GRIS)
        v.rect(x + 1, y + 3, 14, 12, NEGRO, relleno=False)
    else:
        ant = recortar(v, x, y, 16, 16)
        icono(v, nodo["tipo"], x - 4, y - 4)       # el centro de 16x16 del icono de 24x24 de la caja
        v.recorte(*ant)


# ------------------------------------------------------------ el componente

class Arbol(tk.Widget):
    """El árbol dibujado a mano. Lleva su estado (cerrados, filtro, filas) y avisa al diseñador."""
    _classe = "Arbol"
    _foca_no_clique = True
    _padrao = {"width": 220, "height": 180, "takefocus": 1, "cursor": "", "highlightthickness": 0,
               "borderwidth": 0}

    def __init__(self, master, dis):
        tk.Widget.__init__(self, master)
        self.dis = dis
        self.cerrados = set()
        self.filtro = ""
        self.filas = []          # [(nivel, nodo)]
        self.sel = None          # id del nodo elegido
        self.arriba = 0
        self._ver_id = None
        self.bind("<Button-1>", self._clic)
        self.bind("<Double-Button-1>", self._doble)
        self.bind("<Button-3>", self._derecho)
        self.bind("<MouseWheel>", self._rueda)
        self.bind("<Up>", lambda e: self._mover(-1))
        self.bind("<Down>", lambda e: self._mover(1))
        self.bind("<Return>", lambda e: self.activar(self.nodo_elegido(), True))
        self.bind("<F2>", lambda e: self.dis.renombrar_desde_explorador(self.nodo_elegido()))
        self.bind("<Delete>", lambda e: self.dis.borrar_desde_explorador(self.nodo_elegido()))

    def _natural(self):
        return (int(self._op.get("width", 220)), int(self._op.get("height", 180)))

    # --- contenido
    def reconstruir(self, forms, titulo, codigo, sel_id):
        """Vuelve a calcular las filas. Devuelve True si cambió algo."""
        raiz = arbol(forms, titulo, codigo)
        f = filtrar(raiz, self.filtro)
        if f is None:
            self.filas = []
        else:
            self.filas = aplanar(f, self.cerrados, bool(self.filtro.strip()))
        self.sel = sel_id
        self._redesenhar()

    def visibles(self):
        return max(1, (self._wa - 4) // ALTO_FILA)

    def nodo_elegido(self):
        for n, nodo in self.filas:
            if nodo["id"] == self.sel:
                return nodo
        return None

    def indice_de(self, id):
        for i, (n, nodo) in enumerate(self.filas):
            if nodo["id"] == id:
                return i
        return -1

    def ver(self, id):
        """Pide que el nodo `id` quede a la vista (se aplica al dibujar, cuando ya se sabe el tamaño)."""
        self._ver_id = id
        self._redesenhar()

    def _aplicar_ver(self):
        i = self.indice_de(self._ver_id)
        self._ver_id = None
        if i < 0:
            return
        n = self.visibles()
        if i < self.arriba:
            self.arriba = i
        elif i >= self.arriba + n:
            self.arriba = i - n + 1
        self.arriba = max(0, min(self.arriba, max(0, len(self.filas) - n)))

    def fila_centro(self, id):
        """El centro de la fila de un nodo, en coordenadas de la ventana (para las pruebas)."""
        i = self.indice_de(id)
        if i < 0 or not (self.arriba <= i < self.arriba + self.visibles()):
            return None
        return self._ax + 60 + 0, self._ay + 2 + (i - self.arriba) * ALTO_FILA + ALTO_FILA // 2

    def texto_filas(self):
        """Las filas como texto con sangría (para las pruebas y la sonda)."""
        return ["%s%s" % ("  " * n, nodo["t"]) for n, nodo in self.filas]

    # --- ratón y teclado
    def _fila_de(self, ev):
        i = self.arriba + (ev.y - 2) // ALTO_FILA
        return i if 0 <= i < len(self.filas) else -1

    def _clic(self, ev):
        self.focus_set()
        i = self._fila_de(ev)
        if i < 0:
            return
        nivel, nodo = self.filas[i]
        bx = 4 + nivel * SANGRIA
        if nodo["hijos"] and bx <= ev.x < bx + 11 and not self.filtro.strip():
            self.alternar(nodo)
            return
        self.activar(nodo, False)

    def _doble(self, ev):
        i = self._fila_de(ev)
        if i < 0:
            return
        nivel, nodo = self.filas[i]
        if nodo["k"] in ("proyecto", "grupo") and nodo["hijos"]:
            self.alternar(nodo)
            return
        self.activar(nodo, True)

    def _derecho(self, ev):
        self.focus_set()
        i = self._fila_de(ev)
        if i >= 0:
            self.activar(self.filas[i][1], False)
            self.dis.menu_explorador(self.filas[i][1], self._ax + ev.x, self._ay + ev.y)

    def _rueda(self, ev):
        mx = max(0, len(self.filas) - self.visibles())
        self.arriba = max(0, min(self.arriba + (3 if ev.delta < 0 else -3), mx))
        self._redesenhar()

    def _mover(self, d):
        i = self.indice_de(self.sel) + d
        if 0 <= i < len(self.filas):
            self.activar(self.filas[i][1], False)

    def alternar(self, nodo):
        if nodo["id"] in self.cerrados:
            self.cerrados.discard(nodo["id"])
        else:
            self.cerrados.add(nodo["id"])
        self.dis.refrescar_explorador()

    def activar(self, nodo, doble):
        """Un clic (doble=False) o un doble clic en un nodo: lo trata el diseñador."""
        if nodo is None:
            return
        self.sel = nodo["id"]
        self.dis.explorador_nodo(nodo, doble)

    # --- dibujo
    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        if self._ver_id is not None:
            self._aplicar_ver()
        hundido(v, x, y, w, h)
        ant = recortar(v, x + 2, y + 2, w - 4, h - 4)
        for k in range(self.visibles() + 1):
            i = self.arriba + k
            if i >= len(self.filas):
                break
            nivel, nodo = self.filas[i]
            fy = y + 2 + k * ALTO_FILA
            fx = x + 4 + nivel * SANGRIA
            if nodo["hijos"] and not self.filtro.strip():
                v.rect(fx, fy + 4, 9, 9, BLANCO)
                v.rect(fx, fy + 4, 9, 9, SOMBRA, relleno=False)
                v.linea(fx + 2, fy + 8, fx + 6, fy + 8, NEGRO)
                if nodo["id"] in self.cerrados:
                    v.linea(fx + 4, fy + 6, fx + 4, fy + 10, NEGRO)
            _icono_nodo(v, nodo, fx + 12, fy + 1)
            tx = fx + 12 + 18
            elegido = nodo["id"] == self.sel
            ancho = len(nodo["t"]) * 8 + 4
            if elegido:
                v.rect(tx - 2, fy + 1, ancho, ALTO_FILA - 2, AZUL)
            v.texto(tx, fy + 1, nodo["t"], BLANCO if elegido else NEGRO)
        v.recorte(*ant)
