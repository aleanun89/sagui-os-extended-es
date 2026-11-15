# SPDX-License-Identifier: GPL-2.0-or-later
# vps._depurador - depurador sencillo de VisualPythonS (mejora 124, D299).
#
# El MicroPython del sistema se compila sin `sys.settrace` (activarlo pide recompilar el
# intérprete con el perfilador). Por eso el depurador **instrumenta** el código: delante de cada
# sentencia simple de codigo.py añade una línea `__vps_dbg__(N, locals())` con la sangría de la
# sentencia. Esa función decide si hay que parar (punto de parada en la línea N, o paso a paso) y,
# si para, enseña la línea y las variables locales en una ventana. Los números de línea de los
# errores (traceback) se devuelven a los del código original con el mapa que devuelve
# `instrumentar`.
#
# También sirve `vps.depurar()` desde el propio código: para el programa en ese punto y enseña
# las variables.
import sys
import tkinter as tk
from vps import _refactor

NOMBRE = "__vps_dbg__"
SALTAR = ("def", "class", "elif", "else", "except", "finally", "@", "global", "nonlocal")


def sentencias(fuente):
    """Las sentencias del código: [{"n": línea, "sangria": columnas, "toks": [(clase, a, b)]}]. Una
    sentencia empieza en una línea que no es continuación (paréntesis abierto, cadena triple o barra
    invertida) ni un comentario; sus tokens llegan hasta que empieza la siguiente."""
    lineas = fuente.split("\n")
    inicios = []
    pos = 0
    for l in lineas:
        inicios.append(pos)
        pos += len(l) + 1

    def linea_de(p):
        lo, hi = 0, len(inicios) - 1
        while lo < hi:
            m = (lo + hi + 1) // 2
            if inicios[m] <= p:
                lo = m
            else:
                hi = m - 1
        return lo + 1
    r = []
    prof = 0
    vista = set()
    for clase, a, b in _refactor.tokens(fuente):
        n = linea_de(a)
        if n not in vista:
            vista.add(n)
            if prof == 0 and clase != "comentario":
                anterior = lineas[n - 2].rstrip() if n >= 2 else ""
                if not anterior.endswith("\\"):
                    l = lineas[n - 1]
                    r.append({"n": n, "sangria": len(l) - len(l.lstrip(" \t")), "toks": []})
        if clase != "comentario" and r:
            r[-1]["toks"].append((clase, a, b))
        if clase == "signo":
            c = fuente[a]
            if c in "([{":
                prof += 1
            elif c in ")]}":
                prof -= 1
        elif clase == "cadena":
            # una cadena que cruza líneas marca como vistas las que cubre
            for k in range(n + 1, linea_de(max(a, b - 1)) + 1):
                vista.add(k)
    return r


def lineas_de_sentencia(fuente):
    """Los números de línea donde empieza una sentencia que admite una línea delante: no las de
    def/class/elif/else/except/finally/decoradores/global."""
    r = []
    for st in sentencias(fuente):
        a, b = st["toks"][0][1], st["toks"][0][2]
        if fuente[a:b] not in SALTAR:
            r.append(st["n"])
    return r


def _nombres_ligados(fuente, st):
    """(ahora, cuerpo, globales) de una sentencia: los nombres que liga al ejecutarse, los que
    liga solo para su cuerpo (for X in, with .. as X, except .. as X) y los declarados global."""
    toks = st["toks"]
    txt = [fuente[a:b] for _, a, b in toks]
    cl = [c for c, _, _ in toks]
    primero = txt[0]
    ahora, cuerpo, glob = set(), set(), set()
    if primero in ("global", "nonlocal"):
        glob = set(t for t, c in zip(txt[1:], cl[1:]) if c == "nombre")
    elif primero == "def" or primero == "class":
        if len(txt) > 1:
            ahora.add(txt[1])
    elif primero == "for":
        for t, c in zip(txt[1:], cl[1:]):
            if t == "in":
                break
            if c == "nombre":
                cuerpo.add(t)
    elif primero in ("with", "except"):
        for k in range(len(txt) - 1):
            if txt[k] == "as" and cl[k + 1] == "nombre":
                cuerpo.add(txt[k + 1])
    elif primero == "import" or primero == "from":
        i = txt.index("import") if "import" in txt else len(txt)
        k = i + 1
        while k < len(txt):
            if cl[k] == "nombre" and txt[k] != "as":
                if k + 2 < len(txt) + 0 and k + 1 < len(txt) and txt[k + 1] == "as" and k + 2 < len(txt):
                    ahora.add(txt[k + 2])
                    k += 3
                    continue
                if primero == "import" and k > i + 1 and txt[k - 1] == ".":
                    k += 1
                    continue
                ahora.add(txt[k])
            k += 1
    else:
        prof = 0
        for k in range(len(toks)):
            t = txt[k]
            if cl[k] == "signo":
                if t in "([{":
                    prof += 1
                elif t in ")]}":
                    prof -= 1
                elif t == "=" and prof == 0:
                    ant = fuente[toks[k][1] - 1] if toks[k][1] > 0 else ""
                    sig = fuente[toks[k][1] + 1] if toks[k][1] + 1 < len(fuente) else ""
                    if ant not in "=!<>+-*/%&|^@:" and sig != "=":
                        # lo anterior (desde el principio o desde el `=` anterior) son objetivos
                        for j in range(k):
                            if cl[j] == "nombre":
                                antes = txt[j - 1] if j > 0 else ""
                                despues = txt[j + 1] if j + 1 < len(txt) else ""
                                if antes != "." and despues not in (".", "[", "(") and txt[j] not in ("None", "True", "False"):
                                    ahora.add(txt[j])
                        break
    return ahora, cuerpo, glob


def _parametros(fuente, st):
    toks = st["toks"]
    r = set()
    prof = 0
    for k in range(len(toks)):
        c, a, b = toks[k]
        t = fuente[a:b]
        if c == "signo":
            if t in "([{":
                prof += 1
            elif t in ")]}":
                prof -= 1
                if prof == 0:
                    break
        elif c == "nombre" and prof == 1 and k > 0 and fuente[toks[k - 1][1]:toks[k - 1][2]] in ("(", ",", "*"):
            r.add(t)
    return r


def instrumentar(fuente):
    """(código instrumentado, mapa) con mapa[número de línea nueva] = número de línea original.
    Dentro de una función la llamada lleva las variables que seguro existen en ese punto (los
    parámetros y lo asignado antes en bloques que encierran la sentencia): el `locals()` de
    MicroPython no devuelve las locales de una función. Fuera de ellas usa `locals()`."""
    lineas = fuente.split("\n")
    llamadas = {}
    pila = []          # ámbitos: {"sangria", "tipo": «def» o «class», "niveles": {clave: set}}
    for st in sentencias(fuente):
        i = st["sangria"]
        while pila and pila[-1]["sangria"] >= i:
            pila.pop()
        if pila:
            niv = pila[-1]["niveles"]
            for k in [k for k in niv if k > i]:
                del niv[k]
        a, b = st["toks"][0][1], st["toks"][0][2]
        primero = fuente[a:b]
        n = st["n"]
        if primero not in SALTAR:
            if pila and pila[-1]["tipo"] == "def":
                vis = set()
                for conj in pila[-1]["niveles"].values():
                    vis |= conj
                vis -= pila[-1]["globales"]
                llamadas[n] = "{%s}" % ", ".join('"%s": %s' % (v, v) for v in sorted(vis))
            else:
                llamadas[n] = "locals()"
        ahora, cuerpo, glob = _nombres_ligados(fuente, st)
        if pila:
            niv = pila[-1]["niveles"]
            niv.setdefault(i, set()).update(ahora)
            niv.setdefault(i + 0.5, set()).update(cuerpo)
            pila[-1]["globales"] |= glob
        if primero == "def":
            pila.append({"sangria": i, "tipo": "def", "niveles": {i + 0.5: _parametros(fuente, st)}, "globales": set()})
        elif primero == "class":
            pila.append({"sangria": i, "tipo": "class", "niveles": {}, "globales": set()})
    salida = []
    mapa = {}
    for k, l in enumerate(lineas):
        n = k + 1
        if n in llamadas:
            sangria = l[:len(l) - len(l.lstrip(" \t"))]
            salida.append("%s%s(%d, %s)" % (sangria, NOMBRE, n, llamadas[n]))
            mapa[len(salida)] = n
        salida.append(l)
        mapa[len(salida)] = n
    return "\n".join(salida), mapa


def linea_original(mapa, n):
    return mapa.get(n, n) if mapa else n


def _visible(nombre, valor):
    if nombre[:1] == "_":
        return False
    try:
        if callable(valor):
            return False
        t = type(valor).__name__
        if t in ("module", "Control", "Formulario") or hasattr(valor, "_p"):
            return False
    except Exception:
        pass
    return True


def variables_visibles(loc, excluir=()):
    """[(nombre, texto del valor)] ordenado, sin lo que no son datos (funciones, módulos, controles)
    ni los nombres de `excluir` (los de `from vps import *`)."""
    r = []
    for k in sorted(loc.keys()):
        v = loc[k]
        if k not in excluir and _visible(k, v):
            try:
                t = repr(v)
            except Exception:
                t = "?"
            if len(t) > 80:
                t = t[:77] + "..."
            r.append((k, t))
    return r


class Ventana:
    """La ventana «Depurador»: la línea, las variables y los botones Continuar, Paso y Detener."""

    def __init__(self, raiz):
        self.raiz = raiz
        self.top = None
        self.accion = None

    def _crear(self):
        d = self.top = tk.Toplevel(self.raiz)
        d.title("Depurador")
        d.geometry("440x330")
        self.linea = tk.Label(d, text="", anchor="w", justify="left")
        self.linea.pack(side="top", fill="x", padx=6, pady=4)
        fila = tk.Frame(d)
        fila.pack(side="bottom", fill="x", pady=6)
        for t, a in (("Continuar (F5)", "continuar"), ("Paso (F8)", "paso"), ("Detener", "detener")):
            tk.Button(fila, text=t, padx=6, command=lambda a=a: self._elegir(a)).pack(side="left", padx=6)
        self.lista = tk.Listbox(d)
        self.lista.pack(side="top", fill="both", expand=True, padx=6)
        d.bind("<F8>", lambda e: self._elegir("paso"))
        d.bind("<F5>", lambda e: self._elegir("continuar"))
        d.protocol("WM_DELETE_WINDOW", lambda: self._elegir("continuar"))

    def _elegir(self, a):
        self.accion = a

    def mostrar(self, n, texto_linea, vars_, globales=()):
        """Enseña el estado y espera la decisión del usuario: «continuar», «paso» o «detener»."""
        if self.top is None or self.top._destruido:
            self._crear()
        self.top.deiconify()
        self.linea.configure(text="Línea %d:  %s" % (n, texto_linea.strip()))
        self.lista.delete(0, "end")
        for k, v in vars_:
            self.lista.insert("end", "%s = %s" % (k, v))
        if not vars_:
            self.lista.insert("end", "(sin variables locales)")
        if globales:
            self.lista.insert("end", "---- globales ----")
            for k, v in globales:
                self.lista.insert("end", "%s = %s" % (k, v))
        self.accion = None
        print("vps: parada|%d|%d" % (n, len(vars_)))
        tk._laco(lambda: self.accion is None and not self.top._destruido)
        a = self.accion or "continuar"
        self.top.withdraw()
        return a

    def cerrar(self):
        if self.top is not None and not self.top._destruido:
            self.top.destroy()
        self.top = None


class Sesion:
    """Una ejecución depurada: puntos de parada, modo paso a paso y la función que se cuela en
    el código (`hook`). `decisor` (para las pruebas) sustituye a la ventana: f(n, vars) -> acción."""

    def __init__(self, fuente_original, puntos=(), paso=False, raiz=None, decisor=None):
        self.original = fuente_original.split("\n")
        self.puntos = set(puntos)
        self.paso = paso
        self.raiz = raiz
        self.decisor = decisor
        self.ventana = None
        self.paradas = []         # [(línea, [(nombre, valor)])]  (lo que se vio)
        self.detenida = False
        self.al_parar = None      # f(n) y f(None) al seguir: el diseñador marca la línea
        self.globales = None      # el espacio de nombres del programa (para enseñar sus variables)
        self.codigo, self.mapa = instrumentar(fuente_original)

    def hook(self, n, loc):
        if self.detenida:
            return
        if self.paso or n in self.puntos:
            self._parar(n, loc)

    def _parar(self, n, loc):
        self.paso = False
        vars_ = variables_visibles(loc)
        glob = []
        if self.globales is not None and loc is not self.globales:
            import vps
            glob = variables_visibles(self.globales, vps.__all__)
        self.paradas.append((n, vars_))
        texto = self.original[n - 1] if 0 < n <= len(self.original) else ""
        if self.al_parar:
            self.al_parar(n)
        if self.decisor is not None:
            a = self.decisor(n, vars_)
        else:
            if self.ventana is None:
                self.ventana = Ventana(self.raiz)
            a = self.ventana.mostrar(n, texto, vars_, glob)
        if self.al_parar:
            self.al_parar(None)
        if a == "paso":
            self.paso = True
        elif a == "detener":
            self.detenida = True
            import vps
            vps.salir()

    def cerrar(self):
        if self.ventana is not None:
            self.ventana.cerrar()
            self.ventana = None


def parar_aqui(variables=None):
    """`vps.depurar(variables)`: para y enseña las variables (por defecto, las globales de la
    aplicación)."""
    import vps
    if variables is None:
        variables = {}
        for f in vps._formularios:
            if f._ns is not None:
                variables = f._ns
                break
    if vps.depurar_hook is not None:
        vps.depurar_hook(variables)
        return
    raiz = tk._raiz_padrao()
    Ventana(raiz).mostrar(0, "vps.depurar()", variables_visibles(variables))
