# SPDX-License-Identifier: GPL-3.0-or-later
# tablas.py - «DataFrame» ligero para el Python de SaguiOS, con la forma de Pandas (D479, v8.5.6.1).
#
#     from tablas import Tabla, leer_csv
#     t = leer_csv("/ejemplos/datos/ventas.csv")
#     t.filtrar(lambda f: f["unidades"] > 5).ordenar("total", descendente=True)
#     t.groupby("region").agg({"total": "sum", "unidades": "mean"})
#     t.merge(otra, en="producto", como="left")
#     t.pivot(indice="region", columnas="mes", valores="total")
#     t.grafico("barras", x="region", y="total", ruta="/tmp/ventas.png")
#
# Una Tabla guarda cada columna como una lista (columnas con el mismo número de filas). Falta un valor
# cuando es None o NaN; las medias, desviaciones y demás saltan los valores que faltan. Las cuentas con
# muchos números usan ulab (el «NumPy» de MicroPython) si está.
#
# Formatos: CSV y JSON (propios), .xlsx y .mxs (con Macaco, que ya sabe leerlos y escribirlos:
# `macaco --convertir`), SQLite (el módulo sqlite3 del sistema) y Valkey (opcional: si no está el módulo
# cliente, la función lo dice con TablasNoDisponible). Los gráficos usan sagui_grafico (D469).
import json
import math

try:
    from ulab import numpy as _np
except ImportError:           # sin ulab, las mismas cuentas en Python puro
    _np = None


class TablasNoDisponible(Exception):
    """Falta algo que la función necesita (un módulo cliente, un programa del sistema)."""


# ------------------------------------------------------------------------------ valores
def es_nulo(v):
    """True si falta el valor: None, NaN o texto vacío."""
    return v is None or (isinstance(v, float) and v != v) or v == ""


def _num(v):
    return isinstance(v, (int, float)) and not isinstance(v, bool) and not (isinstance(v, float) and v != v)


def _convertir(texto):
    """Texto de un CSV -> None, int, float o str."""
    t = texto.strip()
    if t == "":
        return None
    try:
        return int(t)
    except ValueError:
        pass
    try:
        if t[-1] not in "0123456789.":      # «nan», «inf»... se quedan como texto
            return texto
        return float(t)
    except ValueError:
        return texto


def _validos(lista):
    return [v for v in lista if _num(v)]


def _media(v):
    return sum(v) / len(v) if v else None


def _desviacion(v, ddof=1):
    n = len(v)
    if n - ddof <= 0:
        return None
    if _np is not None and n > 8:
        a = _np.array(v, dtype=_np.float)
        m = _np.mean(a)
        d = a - m
        return math.sqrt(float(_np.sum(d * d)) / (n - ddof))
    m = sum(v) / n
    return math.sqrt(sum((x - m) * (x - m) for x in v) / (n - ddof))


def _mediana(v):
    s = sorted(v)
    n = len(s)
    if not n:
        return None
    return s[n // 2] if n % 2 else (s[n // 2 - 1] + s[n // 2]) / 2


def _ordenable(v):
    # los valores que faltan, al final; los números antes que los textos
    if es_nulo(v):
        return (2, 0)
    if _num(v):
        return (0, v)
    return (1, str(v))


AGREGADOS = {
    "suma": lambda v: sum(v) if v else 0,
    "sum": lambda v: sum(v) if v else 0,
    "media": _media,
    "mean": _media,
    "mediana": _mediana,
    "median": _mediana,
    "min": lambda v: min(v) if v else None,
    "max": lambda v: max(v) if v else None,
    "std": _desviacion,
    "desviacion": _desviacion,
    "contar": len,
    "count": len,
}


def _agregar(nombre, valores):
    f = nombre if callable(nombre) else AGREGADOS.get(nombre)
    if f is None:
        raise ValueError("agregado desconocido: %s" % (nombre,))
    if nombre in ("contar", "count"):
        return len([v for v in valores if not es_nulo(v)])
    return f(_validos(valores)) if not callable(nombre) else f(valores)


def _ordenar_estable(indices, clave, descendente):
    """El sort de MicroPython no es estable: desempata por la posición (y, en descendente, deja los empates en su orden)."""
    pares = sorted([(clave(i), pos, i) for pos, i in enumerate(indices)])
    if descendente:
        pares.reverse()
        k = 0
        while k < len(pares):                  # cada racha de empates, de nuevo en su orden original
            j = k
            while j + 1 < len(pares) and pares[j + 1][0] == pares[k][0]:
                j += 1
            pares[k:j + 1] = pares[k:j + 1][::-1]
            k = j + 1
    return [p[2] for p in pares]


def _der(s, n):
    return " " * (n - len(s)) + s


# ------------------------------------------------------------------------------ la tabla
class Tabla:
    """Columnas con nombre y el mismo número de filas. Los métodos devuelven una Tabla nueva (no cambian la
    original) salvo los que lo dicen (`agregar`, `poner`)."""

    def __init__(self, datos=None, columnas=None):
        self.columnas = []
        self._c = {}
        if datos is None:
            for n in columnas or []:
                self.columnas.append(n)
                self._c[n] = []
        elif isinstance(datos, dict):
            for n in (columnas or list(datos.keys())):
                self.columnas.append(n)
                self._c[n] = list(datos[n])
        else:                                   # lista de dicts o de listas
            datos = list(datos)
            if datos and isinstance(datos[0], dict):
                cols = list(columnas) if columnas else []
                if not cols:
                    for f in datos:
                        for k in f:
                            if k not in cols:
                                cols.append(k)
                for n in cols:
                    self.columnas.append(n)
                    self._c[n] = [f.get(n) for f in datos]
            else:
                if columnas is None:
                    raise ValueError("con listas de filas hay que dar los nombres de las columnas")
                for i, n in enumerate(columnas):
                    self.columnas.append(n)
                    self._c[n] = [f[i] if i < len(f) else None for f in datos]
        largos = set(len(c) for c in self._c.values())
        if len(largos) > 1:
            raise ValueError("las columnas tienen distinto número de filas")

    # --- forma y acceso
    def __len__(self):
        return len(self._c[self.columnas[0]]) if self.columnas else 0

    @property
    def forma(self):
        return (len(self), len(self.columnas))

    def __getitem__(self, clave):
        """t["col"] -> lista; t[["a", "b"]] -> Tabla; t[2] -> fila (dict); t[1:3] -> Tabla; t[[True, False...]] -> Tabla."""
        if isinstance(clave, str):
            return self._c[clave]
        if isinstance(clave, int):
            return self.fila(clave)
        if isinstance(clave, slice):
            return self._filas_en(range(*clave.indices(len(self))))
        if isinstance(clave, (list, tuple)):
            if clave and all(isinstance(k, bool) for k in clave):
                return self._filas_en([i for i, k in enumerate(clave) if k])
            return self.seleccionar(*clave)
        raise TypeError("clave no válida")

    def __setitem__(self, nombre, valores):
        self.poner(nombre, valores)

    def fila(self, i):
        return {n: self._c[n][i] for n in self.columnas}

    def filas(self):
        for i in range(len(self)):
            yield self.fila(i)

    def __iter__(self):
        return self.filas()

    def _filas_en(self, indices):
        indices = list(indices)
        t = Tabla()
        for n in self.columnas:
            t.columnas.append(n)
            c = self._c[n]
            t._c[n] = [c[i] for i in indices]
        return t

    def copia(self):
        return self._filas_en(range(len(self)))

    def cabeza(self, n=5):
        return self._filas_en(range(min(n, len(self))))

    def cola(self, n=5):
        return self._filas_en(range(max(0, len(self) - n), len(self)))

    # --- cambios en el sitio
    def poner(self, nombre, valores):
        """Añade o reemplaza una columna: una lista, un valor único o una función de la fila."""
        n = len(self)
        if callable(valores):
            valores = [valores(f) for f in self.filas()]
        elif not isinstance(valores, (list, tuple)):
            valores = [valores] * n
        valores = list(valores)
        if self.columnas and len(valores) != n:
            raise ValueError("la columna tiene %d valores y la tabla %d filas" % (len(valores), n))
        if nombre not in self._c:
            self.columnas.append(nombre)
        self._c[nombre] = valores
        return self

    def agregar(self, fila):
        """Añade una fila (dict) al final."""
        for n in self.columnas:
            self._c[n].append(fila.get(n))
        return self

    # --- selección
    def seleccionar(self, *nombres):
        if len(nombres) == 1 and isinstance(nombres[0], (list, tuple)):
            nombres = nombres[0]
        t = Tabla()
        for n in nombres:
            t.columnas.append(n)
            t._c[n] = list(self._c[n])
        return t

    def quitar(self, *nombres):
        return self.seleccionar([n for n in self.columnas if n not in nombres])

    def renombrar(self, cambios):
        t = Tabla()
        for n in self.columnas:
            m = cambios.get(n, n)
            t.columnas.append(m)
            t._c[m] = list(self._c[n])
        return t

    def filtrar(self, condicion):
        """Las filas para las que `condicion(fila_dict)` es verdadera."""
        return self._filas_en([i for i, f in enumerate(self.filas()) if condicion(f)])

    _OPS = {
        "==": lambda a, b: a == b, "!=": lambda a, b: a != b,
        ">": lambda a, b: a > b, ">=": lambda a, b: a >= b,
        "<": lambda a, b: a < b, "<=": lambda a, b: a <= b,
        "en": lambda a, b: a in b, "contiene": lambda a, b: b in str(a),
    }

    def donde(self, columna, op, valor):
        """t.donde("total", ">", 100): filtro sencillo (==, !=, >, >=, <, <=, en, contiene); los que faltan no pasan."""
        f = self._OPS[op]
        c = self._c[columna]
        return self._filas_en([i for i, v in enumerate(c) if not es_nulo(v) and f(v, valor)])

    def ordenar(self, por, descendente=False):
        """Ordena por una o varias columnas (estable; los que faltan, al final)."""
        claves = [por] if isinstance(por, str) else list(por)
        idx = list(range(len(self)))
        for n in reversed(claves):          # estable: de la clave menos importante a la más
            c = self._c[n]
            faltan = [i for i in idx if es_nulo(c[i])]
            hay = [i for i in idx if not es_nulo(c[i])]
            idx = _ordenar_estable(hay, lambda i: _ordenable(c[i]), descendente) + faltan
        return self._filas_en(idx)

    def unicos(self, columna):
        vistos = []
        for v in self._c[columna]:
            if v not in vistos and not es_nulo(v):
                vistos.append(v)
        return vistos

    def sin_duplicados(self):
        vistos = []
        idx = []
        for i in range(len(self)):
            clave = tuple(self._c[n][i] for n in self.columnas)
            if clave not in vistos:
                vistos.append(clave)
                idx.append(i)
        return self._filas_en(idx)

    # --- valores que faltan
    def contar_nulos(self):
        return {n: len([v for v in self._c[n] if es_nulo(v)]) for n in self.columnas}

    def quitar_nulos(self, columnas=None):
        cols = columnas or self.columnas
        return self._filas_en([i for i in range(len(self)) if not any(es_nulo(self._c[n][i]) for n in cols)])

    def rellenar(self, valor, columnas=None):
        """Rellena lo que falta: un valor, un dict {columna: valor} o "media"/"mediana" (columnas numéricas)."""
        t = self.copia()
        for n in (columnas or self.columnas):
            v = valor.get(n) if isinstance(valor, dict) else valor
            if v == "media":
                v = _media(_validos(self._c[n]))
            elif v == "mediana":
                v = _mediana(_validos(self._c[n]))
            if v is None:
                continue
            t._c[n] = [v if es_nulo(x) else x for x in self._c[n]]
        return t

    # --- estadística (con ulab si hay)
    def columna(self, nombre):
        """La columna numérica como array de ulab (sin los que faltan), o como lista sin ulab."""
        v = [float(x) for x in _validos(self._c[nombre])]
        return _np.array(v, dtype=_np.float) if _np is not None else v

    def media(self, nombre):
        return _media(_validos(self._c[nombre]))

    def suma(self, nombre):
        return sum(_validos(self._c[nombre]))

    def std(self, nombre):
        return _desviacion(_validos(self._c[nombre]))

    def minimo(self, nombre):
        v = _validos(self._c[nombre])
        return min(v) if v else None

    def maximo(self, nombre):
        v = _validos(self._c[nombre])
        return max(v) if v else None

    def correlacion(self, a, b):
        """Coeficiente de Pearson de dos columnas numéricas (solo las filas donde las dos tienen valor)."""
        pares = [(x, y) for x, y in zip(self._c[a], self._c[b]) if _num(x) and _num(y)]
        n = len(pares)
        if n < 2:
            return None
        mx = sum(p[0] for p in pares) / n
        my = sum(p[1] for p in pares) / n
        sxy = sum((p[0] - mx) * (p[1] - my) for p in pares)
        sxx = sum((p[0] - mx) ** 2 for p in pares)
        syy = sum((p[1] - my) ** 2 for p in pares)
        return sxy / math.sqrt(sxx * syy) if sxx and syy else None

    def describir(self):
        """Tabla con cuenta, media, desviación, mínimo, mediana y máximo de las columnas numéricas."""
        cols = [n for n in self.columnas if _validos(self._c[n])]
        t = Tabla({"estadistico": ["cuenta", "media", "std", "min", "mediana", "max"]})
        for n in cols:
            v = _validos(self._c[n])
            t.poner(n, [len(v), _media(v), _desviacion(v), min(v), _mediana(v), max(v)])
        return t

    # --- agrupar, combinar, girar
    def groupby(self, por):
        return Grupos(self, [por] if isinstance(por, str) else list(por))

    def merge(self, otra, en, como="inner", sufijos=("_x", "_y")):
        """Une dos tablas por una o varias columnas: como = "inner", "left", "right" o "outer"."""
        claves = [en] if isinstance(en, str) else list(en)
        propias = [n for n in self.columnas if n not in claves]
        ajenas = [n for n in otra.columnas if n not in claves]
        nombres = {}
        for n in propias:
            nombres[("a", n)] = n + sufijos[0] if n in ajenas else n
        for n in ajenas:
            nombres[("b", n)] = n + sufijos[1] if n in propias else n
        indice = {}
        for j in range(len(otra)):
            indice.setdefault(tuple(otra._c[k][j] for k in claves), []).append(j)
        salida = []
        usadas = set()
        for i in range(len(self)):
            k = tuple(self._c[c][i] for c in claves)
            casan = [] if any(es_nulo(x) for x in k) else indice.get(k, [])
            for j in casan:
                usadas.add(j)
                salida.append((i, j))
            if not casan and como in ("left", "outer"):
                salida.append((i, None))
        if como in ("right", "outer"):
            for j in range(len(otra)):
                if j not in usadas:
                    salida.append((None, j))
        if como == "right":
            salida = [(i, j) for i, j in salida if j is not None]
        t = Tabla(columnas=claves + [nombres[("a", n)] for n in propias] + [nombres[("b", n)] for n in ajenas])
        for i, j in salida:
            for c in claves:
                t._c[c].append(self._c[c][i] if i is not None else otra._c[c][j])
            for n in propias:
                t._c[nombres[("a", n)]].append(self._c[n][i] if i is not None else None)
            for n in ajenas:
                t._c[nombres[("b", n)]].append(otra._c[n][j] if j is not None else None)
        return t

    def pivot(self, indice, columnas, valores, agg="suma"):
        """Una fila por valor de `indice`, una columna por valor de `columnas`, celdas = agg(valores)."""
        filas = self.unicos(indice)
        cols = self.unicos(columnas)
        celdas = {}
        for i in range(len(self)):
            celdas.setdefault((self._c[indice][i], self._c[columnas][i]), []).append(self._c[valores][i])
        t = Tabla({indice: filas})
        for c in cols:
            col = []
            for f in filas:
                v = celdas.get((f, c))
                col.append(_agregar(agg, v) if v else None)
            t.poner(str(c), col)
        return t

    # --- salida
    def __str__(self):
        return self.texto()

    def texto(self, maximo=20):
        def fmt(v):
            if v is None or (isinstance(v, float) and v != v):
                return "NaN"
            if isinstance(v, float):
                return ("%.4f" % v).rstrip("0").rstrip(".") if v != int(v) or abs(v) < 1e15 else str(v)
            return str(v)
        n = len(self)
        ver = list(range(min(n, maximo)))
        ancho = [len(c) for c in self.columnas]
        celdas = [[fmt(self._c[c][i]) for c in self.columnas] for i in ver]
        for fila in celdas:
            for k, s in enumerate(fila):
                ancho[k] = max(ancho[k], len(s))
        lineas = ["  ".join(_der(c, ancho[k]) for k, c in enumerate(self.columnas))]
        for fila in celdas:
            lineas.append("  ".join(_der(s, ancho[k]) for k, s in enumerate(fila)))
        if n > maximo:
            lineas.append("... (%d filas)" % n)
        return "\n".join(lineas)

    def a_dicts(self):
        return list(self.filas())

    def a_csv(self, ruta, sep=","):
        _escribir_csv(self, ruta, sep)

    def a_json(self, ruta, orientacion="filas"):
        datos = self.a_dicts() if orientacion == "filas" else {n: self._c[n] for n in self.columnas}
        with open(ruta, "w") as f:
            f.write(json.dumps(_sin_nan(datos)))

    def a_xlsx(self, ruta):
        _por_macaco(self, ruta)

    def a_mxs(self, ruta):
        _por_macaco(self, ruta)

    def a_sqlite(self, ruta, tabla, si_existe="reemplazar"):
        """Guarda la tabla en una base SQLite. si_existe: "reemplazar", "agregar" o "fallar"."""
        import sqlite3
        con = sqlite3.connect(ruta)
        try:
            cur = con.cursor()
            q = _ident(tabla)
            existe = cur.execute("SELECT name FROM sqlite_master WHERE type='table' AND name=?", (tabla,)).fetchall()
            if existe and si_existe == "fallar":
                raise ValueError("la tabla %s ya existe" % tabla)
            if existe and si_existe == "reemplazar":
                cur.execute("DROP TABLE " + q)
                existe = False
            if not existe:
                defs = []
                for n in self.columnas:
                    v = _validos(self._c[n])
                    todos_int = v and all(isinstance(x, int) for x in v)
                    texto = any(isinstance(x, str) for x in self._c[n])
                    tipo = "TEXT" if texto or not v else ("INTEGER" if todos_int else "REAL")
                    defs.append(_ident(n) + " " + tipo)
                cur.execute("CREATE TABLE %s (%s)" % (q, ", ".join(defs)))
            marcas = ", ".join("?" for _ in self.columnas)
            cur.executemany("INSERT INTO %s VALUES (%s)" % (q, marcas),
                            [tuple(None if es_nulo(v) and not isinstance(v, str) else v for v in (self._c[n][i] for n in self.columnas))
                             for i in range(len(self))])
            con.commit()
        finally:
            con.close()

    def a_valkey(self, clave, cliente=None, host="127.0.0.1", puerto=6379):
        """Guarda la tabla (como JSON) en una clave de Valkey. Sin el módulo cliente: TablasNoDisponible."""
        c = _cliente_valkey(cliente, host, puerto)
        c.set(clave, json.dumps(_sin_nan({n: self._c[n] for n in self.columnas})))

    # --- gráficos (sagui_grafico, D469)
    def grafico(self, tipo="barras", x=None, y=None, titulo=None, ruta=None, ancho=640, alto=400, ventana=False,
                titulo_ventana="Tablas"):
        """Dibuja la tabla. `x`: columna de categorías (o, en dispersión, de abscisas); `y`: una columna o una lista.
        ruta: .png o .bmp (exporta); ventana=True abre una ventana (con ese título) y espera a que se cierre. Devuelve el gráfico."""
        g = grafico_de(self, tipo, x, y, titulo)
        if ruta:
            g.exportar(ruta, ancho, alto)
        if ventana:
            mostrar_grafico(g, ancho, alto, titulo_ventana)
        return g


class Grupos:
    """El resultado de Tabla.groupby: se pide con agg()."""

    def __init__(self, tabla, por):
        self.tabla = tabla
        self.por = por
        self.grupos = {}        # clave -> [índices]; el orden de aparición se conserva
        self.orden = []
        for i in range(len(tabla)):
            k = tuple(tabla._c[c][i] for c in por)
            if k not in self.grupos:
                self.grupos[k] = []
                self.orden.append(k)
            self.grupos[k].append(i)

    def __len__(self):
        return len(self.orden)

    def agg(self, que):
        """que: {columna: "suma"|"media"|"mediana"|"min"|"max"|"std"|"contar"|función} (o una lista de ellos)."""
        t = Tabla(columnas=list(self.por))
        columnas = []
        for col, f in que.items():
            for fn in ([f] if not isinstance(f, (list, tuple)) else f):
                nombre = col if not isinstance(f, (list, tuple)) else "%s_%s" % (col, fn if isinstance(fn, str) else "f")
                columnas.append((col, fn, nombre))
                t.columnas.append(nombre)
                t._c[nombre] = []
        for k in self.orden:
            for p, c in zip(self.por, k):
                t._c[p].append(c)
            idx = self.grupos[k]
            for col, fn, nombre in columnas:
                t._c[nombre].append(_agregar(fn, [self.tabla._c[col][i] for i in idx]))
        return t

    def contar(self):
        t = Tabla(columnas=list(self.por) + ["filas"])
        for k in self.orden:
            for p, c in zip(self.por, k):
                t._c[p].append(c)
            t._c["filas"].append(len(self.grupos[k]))
        return t

    def tabla_de(self, clave):
        if not isinstance(clave, tuple):
            clave = (clave,)
        return self.tabla._filas_en(self.grupos[clave])


# ------------------------------------------------------------------------------ CSV
def _partir_csv(texto, sep):
    """Filas de campos de un CSV con comillas (RFC 4180: «""» dentro de comillas, saltos de línea dentro)."""
    filas = []
    fila = []
    campo = []
    i = 0
    n = len(texto)
    entre = False
    hay = False             # ¿ha empezado algo en esta fila?
    while i < n:
        c = texto[i]
        if entre:
            if c == '"':
                if i + 1 < n and texto[i + 1] == '"':
                    campo.append('"')
                    i += 1
                else:
                    entre = False
            else:
                campo.append(c)
        elif c == '"':
            entre = True
            hay = True
        elif c == sep:
            fila.append("".join(campo))
            campo = []
            hay = True
        elif c == "\n" or c == "\r":
            if c == "\r" and i + 1 < n and texto[i + 1] == "\n":
                i += 1
            if hay or campo:
                fila.append("".join(campo))
                filas.append(fila)
            fila = []
            campo = []
            hay = False
        else:
            campo.append(c)
            hay = True
        i += 1
    if hay or campo:
        fila.append("".join(campo))
        filas.append(fila)
    return filas


def _detectar_sep(texto):
    linea = texto.split("\n", 1)[0]
    cuentas = {s: linea.count(s) for s in (",", ";", "\t")}
    return max(cuentas, key=lambda s: cuentas[s]) if max(cuentas.values()) else ","


def leer_csv(ruta, sep=None, encabezado=True, tipos=True):
    """Lee un CSV. sep=None lo detecta (coma, punto y coma o tabulador). Convierte a int/float lo que lo es."""
    with open(ruta, "r") as f:
        texto = f.read()
    return desde_csv(texto, sep, encabezado, tipos)


def desde_csv(texto, sep=None, encabezado=True, tipos=True):
    if texto.startswith("﻿"):
        texto = texto[1:]
    sep = sep or _detectar_sep(texto)
    filas = _partir_csv(texto, sep)
    if not filas:
        return Tabla()
    if encabezado:
        nombres = [c.strip() or "col%d" % (i + 1) for i, c in enumerate(filas[0])]
        filas = filas[1:]
    else:
        nombres = ["col%d" % (i + 1) for i in range(len(filas[0]))]
    vistos = {}
    for i, n in enumerate(nombres):             # repetidos: nombre, nombre.1...
        if n in vistos:
            vistos[n] += 1
            nombres[i] = "%s.%d" % (n, vistos[n])
        else:
            vistos[n] = 0
    t = Tabla(columnas=nombres)
    for k, n in enumerate(nombres):
        col = [f[k] if k < len(f) else "" for f in filas]
        t._c[n] = [_convertir(v) for v in col] if tipos else [v if v != "" else None for v in col]
    return t


def _campo_csv(v, sep):
    if v is None or (isinstance(v, float) and v != v):
        return ""
    s = repr(v) if isinstance(v, float) else str(v)
    if isinstance(v, float) and s.endswith(".0"):
        s = s[:-2]
    if sep in s or '"' in s or "\n" in s or "\r" in s:
        s = '"' + s.replace('"', '""') + '"'
    return s


def a_texto_csv(t, sep=","):
    lineas = [sep.join(_campo_csv(n, sep) for n in t.columnas)]
    for i in range(len(t)):
        lineas.append(sep.join(_campo_csv(t._c[n][i], sep) for n in t.columnas))
    return "\n".join(lineas) + "\n"


def _escribir_csv(t, ruta, sep=","):
    with open(ruta, "w") as f:
        f.write(a_texto_csv(t, sep))


# ------------------------------------------------------------------------------ JSON
def _sin_nan(o):
    if isinstance(o, float) and o != o:
        return None
    if isinstance(o, dict):
        return {k: _sin_nan(v) for k, v in o.items()}
    if isinstance(o, (list, tuple)):
        return [_sin_nan(v) for v in o]
    return o


def desde_json(texto):
    """Una lista de objetos (una fila cada uno) o un objeto de listas (una columna cada una)."""
    d = json.loads(texto)
    if isinstance(d, dict):
        if all(isinstance(v, list) for v in d.values()):
            return Tabla(d)
        return Tabla([d])
    return Tabla(d)


def leer_json(ruta):
    with open(ruta, "r") as f:
        return desde_json(f.read())


# ------------------------------------------------------------------------------ .xlsx y .mxs (Macaco)
_n_tmp = [0]


def _tmp(ext):
    _n_tmp[0] += 1
    import os
    try:
        pid = os.getpid()
    except Exception:
        pid = 0
    return "/tmp/tablas_%d_%d%s" % (pid, _n_tmp[0], ext)


def _macaco(entrada, salida):
    """Convierte con `macaco --convertir` (el mismo código que abre y guarda en la hoja de cálculo)."""
    import os
    import subprocess
    try:
        r = subprocess.run(["macaco", "--convertir", entrada, salida], capture_output=True)
    except OSError:
        raise TablasNoDisponible("el programa macaco no está en esta imagen")
    existe = True
    try:
        os.stat(salida)
    except OSError:
        existe = False
    if r.returncode != 0 or not existe:
        raise OSError("macaco no pudo convertir %s a %s" % (entrada, salida))


def _borrar(ruta):
    import os
    try:
        os.remove(ruta)
    except OSError:
        pass


def _por_macaco(t, destino):
    tmp = _tmp(".csv")
    try:
        _escribir_csv(t, tmp)
        _macaco(tmp, destino)
    finally:
        _borrar(tmp)


def _desde_macaco(ruta):
    tmp = _tmp(".csv")
    try:
        _macaco(ruta, tmp)
        return leer_csv(tmp)
    finally:
        _borrar(tmp)


def leer_xlsx(ruta):
    """Primera hoja de un .xlsx (con Macaco); la primera fila son los nombres de las columnas."""
    return _desde_macaco(ruta)


def leer_mxs(ruta):
    """Hoja activa de un libro .mxs de Macaco."""
    return _desde_macaco(ruta)


# ------------------------------------------------------------------------------ SQLite
def _ident(nombre):
    return '"' + str(nombre).replace('"', '""') + '"'


def leer_sqlite(ruta, consulta, parametros=()):
    """`consulta` es un SELECT, o el nombre de una tabla."""
    import sqlite3
    con = sqlite3.connect(ruta)
    try:
        cur = con.cursor()
        ini = consulta.lstrip().lower()
        es_consulta = ini.startswith("select") or ini.startswith("with") or ini.startswith("pragma")
        q = consulta if es_consulta else "SELECT * FROM " + _ident(consulta)
        cur.execute(q, tuple(parametros))
        nombres = [d[0] for d in cur.description]
        return Tabla(cur.fetchall(), nombres)
    finally:
        con.close()


# ------------------------------------------------------------------------------ Valkey (opcional)
def valkey_disponible():
    try:
        import valkey      # noqa: F401
        return True
    except ImportError:
        return False


def _cliente_valkey(cliente, host, puerto):
    if cliente is not None:
        return cliente
    try:
        import valkey
    except ImportError:
        raise TablasNoDisponible("Valkey no está disponible: falta el módulo cliente «valkey» en esta imagen")
    return valkey.Valkey(host=host, port=puerto)


def leer_valkey(clave, cliente=None, host="127.0.0.1", puerto=6379):
    c = _cliente_valkey(cliente, host, puerto)
    v = c.get(clave)
    if v is None:
        raise KeyError(clave)
    if isinstance(v, (bytes, bytearray)):
        v = bytes(v).decode()
    return desde_json(v)


# ------------------------------------------------------------------------------ gráficos
def grafico_de(t, tipo, x, y, titulo=None):
    """Prepara un sagui_grafico.Grafico con las columnas de la tabla."""
    import sagui_grafico
    if isinstance(y, str):
        y = [y]
    if y is None:
        y = [n for n in t.columnas if n != x and _validos(t._c[n])]
    g = sagui_grafico.Grafico()
    g.tipo(tipo)
    if titulo:
        g.titulo(titulo)
    g.opcion("animado", 0)
    if tipo == "dispersion":
        xs = [float(v) if _num(v) else 0.0 for v in t._c[x]]
        for n in y:
            g.agregar_serie(n, [float(v) if _num(v) else 0.0 for v in t._c[n]], 0, xs)
        g.ejes(x, y[0] if len(y) == 1 else "")
        return g
    if x is not None:
        g.categorias([str(v) for v in t._c[x]])
    if tipo == "histograma":
        n = y[0]
        g.agregar_serie(n, [float(v) for v in _validos(t._c[n])])
        return g
    for n in y:
        g.agregar_serie(n, [float(v) if _num(v) else 0.0 for v in t._c[n]])
    return g


def mostrar_grafico(g, ancho=640, alto=400, titulo="Tablas"):
    """Una ventana con el gráfico (se redibuja al cambiar el tamaño, resalta con el ratón); espera al cierre."""
    import sagui
    v = sagui.Ventana(ancho, alto, titulo)
    img = [None, 0, 0]

    def pintar():
        an, al = v.tamano()
        if img[0] is None or img[1] != an or img[2] != al:
            img[0] = sagui.Imagen.nueva(an, al, 0xFFFFFF)
            img[1], img[2] = an, al
        g.avanzar(sagui.ticks_ms())
        g.dibujar(img[0], an, al)
        v.imagen(0, 0, img[0])
        v.actualizar()

    pintar()
    while True:
        e = v.evento(40)
        if e is not None:
            if e.tipo == sagui.CERRAR:
                break
            if e.tipo == getattr(sagui, "MOVER", -99):
                g.mover(e.x, e.y)
        pintar()
    g.cerrar()
