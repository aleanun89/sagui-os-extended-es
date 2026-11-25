# SPDX-License-Identifier: GPL-2.0-or-later
# vps._arbol - el control Arbol de VisualPythonS (v8.5.6, D466): una vista en árbol de nodos con
# texto y clave, que se expanden y se contraen.
#
#     Arbol1 = Arbol(f, 8, 8, 200, 160)
#     Arbol1.agregar(None, "Trabajo", "trabajo")            # nodo de primer nivel
#     Arbol1.agregar("trabajo", "Reuniones", "reuniones")   # hijo de «trabajo»
#     def Arbol1_seleccion(): Etiqueta1.texto = Arbol1.clave
#
# En el diseñador los nodos se escriben en `nodos`, una línea por nodo, con dos espacios de sangría por
# nivel y, si hace falta, «texto|clave» (sin clave, la clave es el texto). El dibujo está en
# vps/_widgets.py (_ArbolW); aquí está la lógica y los eventos.
from vps import Control, _props, _ev, _a_tk


@_props
class Arbol(Control):
    """Vista en árbol. `agregar(padre, texto, clave)` añade un nodo (padre None = primer nivel) y devuelve
    su clave; `quitar(clave)`, `limpiar()`, `expandir(clave)`, `contraer(clave)`. `clave` y `valor` son los
    del nodo elegido (`valor` es su texto); `seleccion` es su posición en el orden de recorrido. Eventos:
    `seleccion` y `click` (al elegir), `doble_click`, `expandir` y `contraer` (al abrir o cerrar un nodo con
    el ratón o el teclado; `nodo` es su clave)."""
    tipo = "Arbol"
    PROPS = (("nodos", "lista", []), ("expandido", "bool", True), ("seleccion", "int", -1))
    EVENTOS = _ev("seleccion", "click", "doble_click", "expandir", "contraer")
    DOBLE_PROPIO = True
    EVENTO_DEFECTO = "seleccion"
    ORDEN = ("x", "y", "ancho", "alto")
    TAM = (200, 140)
    nodo = ""      # la clave del último nodo que se expandió o se contrajo (eventos expandir y contraer)

    def _crear(self):
        from vps import _widgets
        self.w = _widgets._ArbolW(self._contenedor_tk())
        self.w.ctl = self

    # --- el modelo
    def _indice(self, clave):
        for i, n in enumerate(self.w.nodos):
            if n["clave"] == clave:
                return i
        return -1

    def _fin_rama(self, i):
        """El índice siguiente al último descendiente del nodo i."""
        nodos = self.w.nodos
        j = i + 1
        while j < len(nodos) and nodos[j]["nivel"] > nodos[i]["nivel"]:
            j += 1
        return j

    def _clave_libre(self, texto):
        c = str(texto)
        k = 2
        while self._indice(c) >= 0:
            c = "%s#%d" % (texto, k)
            k += 1
        return c

    def agregar(self, padre, texto, clave=None):
        """Añade un nodo bajo `padre` (la clave de otro nodo; None o "" = primer nivel). Devuelve su clave."""
        if clave is None or clave == "":
            clave = self._clave_libre(texto)
        else:
            clave = str(clave)
            if self._indice(clave) >= 0:
                raise ValueError("Arbol: ya hay un nodo con la clave «%s»" % clave)
        nodos = self.w.nodos
        if padre is None or padre == "":
            pos, nivel = len(nodos), 0
        else:
            i = self._indice(padre)
            if i < 0:
                raise ValueError("Arbol: no hay ningún nodo con la clave «%s»" % (padre,))
            pos, nivel = self._fin_rama(i), nodos[i]["nivel"] + 1
        nodos.insert(pos, {"clave": clave, "texto": str(texto), "padre": padre or None, "nivel": nivel,
                           "abierto": bool(self._p["expandido"])})
        self.w._redesenhar()
        return clave

    def quitar(self, clave):
        """Quita el nodo y todo lo que cuelga de él."""
        i = self._indice(clave)
        if i < 0:
            raise ValueError("Arbol: no hay ningún nodo con la clave «%s»" % (clave,))
        fin = self._fin_rama(i)
        if self.w.sel is not None and self.w.sel in [n["clave"] for n in self.w.nodos[i:fin]]:
            self.w.sel = None
        del self.w.nodos[i:fin]
        self.w._redesenhar()

    def limpiar(self):
        self.w.nodos = []
        self.w.sel = None
        self.w.arriba = 0
        self.w._redesenhar()

    def contar(self):
        return len(self.w.nodos)

    def claves(self):
        return [n["clave"] for n in self.w.nodos]

    def existe(self, clave):
        return self._indice(clave) >= 0

    def texto_de(self, clave):
        i = self._indice(clave)
        return self.w.nodos[i]["texto"] if i >= 0 else None

    def padre_de(self, clave):
        i = self._indice(clave)
        return self.w.nodos[i]["padre"] if i >= 0 else None

    def hijos(self, clave=None):
        """Las claves de los hijos directos de un nodo (None = los de primer nivel)."""
        return [n["clave"] for n in self.w.nodos if n["padre"] == (clave or None)]

    def ruta(self, clave=None):
        """Los textos desde el primer nivel hasta el nodo («Trabajo/Reuniones»); por defecto, el elegido."""
        clave = self.clave if clave is None else clave
        r = []
        while clave:
            i = self._indice(clave)
            if i < 0:
                break
            r.insert(0, self.w.nodos[i]["texto"])
            clave = self.w.nodos[i]["padre"]
        return "/".join(r)

    def tiene_hijos(self, clave):
        i = self._indice(clave)
        return i >= 0 and self.w.tiene_hijos(i)

    def esta_expandido(self, clave):
        i = self._indice(clave)
        return i >= 0 and self.w.nodos[i]["abierto"]

    def _abrir(self, clave, abierto):
        for n in self.w.nodos:
            if clave is None or n["clave"] == clave:
                n["abierto"] = abierto
        if clave is not None and abierto:
            self._abrir_ascendientes(clave)
        self.w._redesenhar()

    def _abrir_ascendientes(self, clave):
        """Abre los nodos de arriba de `clave`, para que se vea."""
        i = self._indice(clave)
        while i >= 0 and self.w.nodos[i]["padre"]:
            i = self._indice(self.w.nodos[i]["padre"])
            self.w.nodos[i]["abierto"] = True

    def expandir(self, clave=None):
        """Abre el nodo de esa clave (y los de arriba, para que se vea); sin clave, todos."""
        self._abrir(clave, True)

    def contraer(self, clave=None):
        """Cierra el nodo de esa clave; sin clave, todos."""
        self._abrir(clave, False)

    def alternar(self, clave):
        self._abrir(clave, not self.esta_expandido(clave))

    # --- la selección
    def elegir(self, clave):
        """Elige un nodo (None lo deja sin elegir), abriendo sus ascendientes; no dispara eventos."""
        if clave is None or clave == "":
            self.w.sel = None
        else:
            i = self._indice(clave)
            if i < 0:
                raise ValueError("Arbol: no hay ningún nodo con la clave «%s»" % (clave,))
            self._abrir_ascendientes(clave)
            self.w.sel = clave
            self.w.ver(clave)
        self.w._redesenhar()

    def _elegir_usuario(self, clave):
        self.elegir(clave)
        self._disparar("seleccion")

    def _leer(self, n):
        if n == "nodos":
            r = []
            for x in self.w.nodos:
                r.append("  " * x["nivel"] + x["texto"] + ("" if x["clave"] == x["texto"] else "|" + x["clave"]))
            return r
        if n == "seleccion":
            return self._indice(self.w.sel) if self.w is not None and self.w.sel is not None else -1
        if n == "clave":
            return self.w.sel
        if n == "valor":
            return self.texto_de(self.w.sel) if self.w.sel is not None else None
        return self._p.get(n)

    clave = property(lambda self: self._leer("clave"), lambda self, v: self.elegir(v))
    valor = property(lambda self: self._leer("valor"))

    def _cargar(self, lineas):
        self.limpiar()
        pila = []        # (nivel, clave) de los ascendientes
        for l in lineas or []:
            l = str(l).replace("\t", "  ")
            nivel = (len(l) - len(l.lstrip(" "))) // 2
            t = l.strip()
            if not t:
                continue
            texto, _, clave = t.partition("|")
            while pila and pila[-1][0] >= nivel:
                pila.pop()
            padre = pila[-1][1] if pila else None
            c = self.agregar(padre, texto.strip(), clave.strip() or None)
            pila.append((nivel, c))

    def _aplicar_todo(self):
        # el orden de un diccionario de MicroPython no es el de inserción: los nodos van antes que la selección
        self._colocar()
        previos = ("nodos", "expandido", "seleccion")
        for n in previos:
            self._aplicar(n)
        for n in self._p:
            if n not in previos and n not in ("x", "y", "ancho", "alto", "visible", "pagina"):
                self._aplicar(n)

    def _aplicar(self, n):
        if n == "nodos":
            self._cargar(self._p["nodos"])
        elif n == "expandido":
            for x in self.w.nodos:
                x["abierto"] = bool(self._p["expandido"])
            self.w._redesenhar()
        elif n == "seleccion":
            i = int(self._p["seleccion"])
            self.w.sel = self.w.nodos[i]["clave"] if 0 <= i < len(self.w.nodos) else None
            self.w._redesenhar()
        else:
            Control._aplicar(self, n)

    def _aplicar_fondo(self):
        self._cfg(background=_a_tk(self._p["colorfondo"]))
        self.w._redesenhar()

    # --- el ratón y el teclado (los llama _ArbolW)
    def _pulsado(self, n, en_caja):
        i = self._indice(n["clave"])
        if en_caja and self.w.tiene_hijos(i):
            self._alternar_usuario(n)
            return
        self._elegir_usuario(n["clave"])
        self._disparar("click")

    def _doble(self, n):
        if self.w.sel != n["clave"]:
            self._elegir_usuario(n["clave"])
        self._disparar("doble_click")

    def _alternar_usuario(self, n):
        n["abierto"] = not n["abierto"]
        self.nodo = n["clave"]
        if not n["abierto"] and self.w.sel is not None:
            # si el nodo elegido queda escondido, la selección pasa a este nodo
            i = self._indice(n["clave"])
            ocultos = [x["clave"] for x in self.w.nodos[i + 1:self._fin_rama(i)]]
            if self.w.sel in ocultos:
                self.w.sel = n["clave"]
        self.w._redesenhar()
        self._disparar("expandir" if n["abierto"] else "contraer")

    def _tecla(self, tecla):
        vis = self.w.visibles()
        if not vis:
            return
        k = None
        for j, n in enumerate(vis):
            if n["clave"] == self.w.sel:
                k = j
        if tecla in ("Up", "Down"):
            j = 0 if k is None else max(0, min(len(vis) - 1, k + (1 if tecla == "Down" else -1)))
            self._elegir_usuario(vis[j]["clave"])
        elif tecla == "Home":
            self._elegir_usuario(vis[0]["clave"])
        elif tecla == "End":
            self._elegir_usuario(vis[-1]["clave"])
        elif k is None:
            return
        elif tecla == "Right":
            n = vis[k]
            if self.w.tiene_hijos(self._indice(n["clave"])):
                if not n["abierto"]:
                    self._alternar_usuario(n)
                else:
                    self._elegir_usuario(vis[k + 1]["clave"])
        elif tecla == "Left":
            n = vis[k]
            if self.w.tiene_hijos(self._indice(n["clave"])) and n["abierto"]:
                self._alternar_usuario(n)
            elif n["padre"]:
                self._elegir_usuario(n["padre"])
        elif tecla in ("space", "plus", "minus"):
            n = vis[k]
            if self.w.tiene_hijos(self._indice(n["clave"])):
                self._alternar_usuario(n)
        elif tecla == "Return":
            self._disparar("doble_click")


TIPOS = {"Arbol": Arbol}
METODOS = {"Arbol": [("agregar", "agregar(padre, texto, clave=None)", "Añade un nodo (padre None = primer nivel); devuelve su clave"),
                     ("quitar", "quitar(clave)", "Quita el nodo y sus hijos"),
                     ("limpiar", "limpiar()", "Vacía el árbol"),
                     ("expandir", "expandir(clave=None)", "Abre un nodo (sin clave, todos)"),
                     ("contraer", "contraer(clave=None)", "Cierra un nodo (sin clave, todos)"),
                     ("alternar", "alternar(clave)", "Abre o cierra un nodo"),
                     ("elegir", "elegir(clave)", "Elige un nodo (None = ninguno) sin disparar eventos"),
                     ("contar", "contar()", "Cuántos nodos hay"),
                     ("claves", "claves()", "Las claves de todos los nodos, en orden de recorrido"),
                     ("existe", "existe(clave)", "Si hay un nodo con esa clave"),
                     ("texto_de", "texto_de(clave)", "El texto de un nodo"),
                     ("padre_de", "padre_de(clave)", "La clave del padre de un nodo (None en el primer nivel)"),
                     ("hijos", "hijos(clave=None)", "Las claves de los hijos directos de un nodo"),
                     ("ruta", "ruta(clave=None)", "Los textos desde el primer nivel hasta el nodo: «Trabajo/Reuniones»")]}
