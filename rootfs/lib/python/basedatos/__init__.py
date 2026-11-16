# SPDX-License-Identifier: GPL-2.0-or-later
# basedatos - una API pequeña y la misma para SQLite y lite-monkey (mejora 127, D304).
#
#     import basedatos
#     bd = basedatos.conectar("sqlite:/home/datos.db")              # o ":memory:"
#     bd = basedatos.conectar("litemonkey://127.0.0.1:7979/mibase")
#     bd.tablas()                              # nombres (en lite-monkey, también bd.colecciones())
#     bd.esquema("contactos")                  # [{"nombre", "tipo", "clave", ...}]
#     bd.consultar("contactos", {"edad": {"$gt": 18}}, orden="-nombre", limite=20)  # [dict, ...]
#     bd.contar("contactos"); bd.insertar(t, fila); bd.actualizar(t, valores, filtro | clave=)
#     bd.borrar(t, filtro | clave=)
#     bd.ejecutar("SELECT * FROM t WHERE id = ?", [3])          # solo SQLite, parámetros enlazados
#     with bd.transaccion(): ...                                # solo SQLite
#
# Funciona en el MicroPython de SaguiOS y en CPython (para las pruebas en el PC). Los valores
# NUNCA se concatenan en el SQL: van como parámetros enlazados. Los nombres de tablas y columnas
# se entrecomillan. Referencia completa: docs/bases-de-datos.md.
import json

__all__ = ["conectar", "ErrorBD", "ErrorConexion", "ErrorConsulta", "ErrorIntegridad",
           "valor_desde_texto", "texto_de_valor", "a_csv", "TIPOS_SQL"]

TIPOS_SQL = ("INTEGER", "REAL", "TEXT", "BLOB", "NUMERIC")


# ------------------------------------------------------------ errores

class ErrorBD(Exception):
    """Base de todos los errores de la biblioteca."""


class ErrorConexion(ErrorBD):
    """No se pudo abrir la base o el servidor no contesta."""


class ErrorConsulta(ErrorBD):
    """Una consulta mal hecha, una tabla que no existe, una sentencia que falla."""


class ErrorIntegridad(ErrorConsulta):
    """Clave repetida, campo obligatorio vacío u otra restricción que no se cumple."""


# ------------------------------------------------------------ utilidades comunes

def _orden_lista(orden):
    """El orden como [(campo, 1|-1)]: acepta "campo", "-campo", ["a", "-b"] o [("a", 1)]."""
    if not orden:
        return []
    if isinstance(orden, str):
        orden = [orden]
    r = []
    for o in orden:
        if isinstance(o, str):
            if o.startswith("-"):
                r.append((o[1:], -1))
            elif o.startswith("+"):
                r.append((o[1:], 1))
            else:
                r.append((o, 1))
        else:
            campo, d = o[0], o[1]
            if isinstance(d, str):
                d = -1 if d.lower().startswith("d") else 1
            r.append((campo, -1 if d < 0 else 1))
    return r


def texto_de_valor(v):
    """Un valor de la base como texto para enseñarlo (None se ve vacío)."""
    if v is None:
        return ""
    if isinstance(v, bool):
        return "true" if v else "false"
    if isinstance(v, (bytes, bytearray)):
        return "<binario %d bytes>" % len(v)
    if isinstance(v, (list, dict)):
        try:
            return json.dumps(v)
        except Exception:
            return str(v)
    return str(v)


def valor_desde_texto(texto, tipo=None):
    """Lo que el usuario escribió en una celda, como valor de Python. `tipo` es el tipo de la
    columna (SQLite: INTEGER, REAL...; lite-monkey: entero, real, logico, texto...) o None para
    adivinarlo (número, true/false/null o texto). Un texto vacío en una columna numérica es None."""
    t = (tipo or "").upper()
    if t.startswith("ENTERO") or "INT" in t:
        modo = "int"
    elif t.startswith("REAL") or any(k in t for k in ("FLOA", "DOUB", "NUM", "DEC")):
        modo = "float"
    elif t.startswith("LOGICO") or t.startswith("BOOL"):
        modo = "bool"
    elif t.startswith("TEXTO") or "CHAR" in t or "TEXT" in t or "CLOB" in t:
        return texto
    else:
        modo = "auto"
    s = texto.strip() if isinstance(texto, str) else texto
    if modo != "auto" and s == "":
        return None
    try:
        if modo == "int":
            return int(s)
        if modo == "float":
            return float(s)
        if modo == "bool":
            return s.lower() in ("true", "1", "si", "sí", "s", "t", "yes", "y")
    except ValueError:
        raise ErrorConsulta("«%s» no es un número válido" % texto)
    if s == "":
        return texto
    if s == "null":
        return None
    if s in ("true", "false"):
        return s == "true"
    try:
        return int(s)
    except ValueError:
        pass
    try:
        return float(s)
    except ValueError:
        pass
    return texto


def a_csv(columnas, filas, separador=","):
    """Texto CSV (comillas dobles, sin dependencias) de una lista de listas o de diccionarios."""
    def celda(v):
        s = texto_de_valor(v)
        if any(c in s for c in (separador, '"', "\n", "\r")):
            s = '"' + s.replace('"', '""') + '"'
        return s
    sal = [separador.join(celda(c) for c in columnas)]
    for f in filas:
        if isinstance(f, dict):
            f = [f.get(c) for c in columnas]
        sal.append(separador.join(celda(v) for v in f))
    return "\n".join(sal) + "\n"


class _Transaccion:
    def __init__(self, bd):
        self.bd = bd

    def __enter__(self):
        self.bd.empezar()
        return self.bd

    def __exit__(self, tipo, valor, tb):
        if tipo is None:
            self.bd.confirmar()
        else:
            self.bd.revertir()
        return False


class BaseDatos:
    """Lo común de los dos motores."""
    tipo = ""
    origen = ""

    def __enter__(self):
        return self

    def __exit__(self, tipo, valor, tb):
        self.cerrar()
        return False

    def __repr__(self):
        return "<BaseDatos %s>" % self.origen

    def colecciones(self):
        return self.tablas()

    def transaccion(self):
        return _Transaccion(self)

    def columnas(self, tabla):
        """Los nombres de las columnas, en el orden en que se enseñan."""
        return [c["nombre"] for c in self.esquema(tabla)]

    def existe(self, tabla):
        return tabla in self.tablas()


# ------------------------------------------------------------ SQLite

def _q(nombre):
    """Un nombre de tabla o columna entrecomillado para SQLite."""
    return '"' + str(nombre).replace('"', '""') + '"'


def _literal_sql(v, volcado=False):
    """Un valor como literal SQL (solo para DEFAULT del asistente y para el volcado; los datos del usuario
    siempre van enlazados)."""
    if v is None:
        return "NULL"
    if isinstance(v, bool):
        return "1" if v else "0"
    if isinstance(v, (int, float)):
        return repr(v)
    if isinstance(v, (bytes, bytearray)):
        return "X'" + "".join("%02x" % b for b in v) + "'"
    t = str(v)
    if not volcado:
        try:
            float(t)
            return t
        except ValueError:
            if t.upper() in ("NULL", "CURRENT_TIMESTAMP", "CURRENT_DATE", "CURRENT_TIME"):
                return t.upper()
    return "'" + t.replace("'", "''") + "'"


_OPS_SQL = {"$eq": "=", "$ne": "<>", "$gt": ">", "$gte": ">=", "$lt": "<", "$lte": "<="}


def _condicion(campo, v, params):
    """La condición SQL de `campo` frente a un valor o un diccionario de operadores."""
    c = _q(campo)
    if isinstance(v, dict):
        partes = []
        for op, x in v.items():
            if op in ("$eq", "$ne") and x is None:
                partes.append(c + (" IS NULL" if op == "$eq" else " IS NOT NULL"))
            elif op in _OPS_SQL:
                partes.append("%s %s ?" % (c, _OPS_SQL[op]))
                params.append(x)
            elif op in ("$in", "$nin"):
                x = list(x)
                if not x:
                    partes.append("0" if op == "$in" else "1")
                else:
                    partes.append("%s %s (%s)" % (c, "IN" if op == "$in" else "NOT IN", ",".join("?" * len(x))))
                    params.extend(x)
            elif op == "$like":
                partes.append(c + " LIKE ?")
                params.append(x)
            elif op == "$contiene":
                partes.append(c + " LIKE ? ESCAPE '\\'")
                params.append("%" + str(x).replace("\\", "\\\\").replace("%", "\\%").replace("_", "\\_") + "%")
            elif op == "$existe":
                partes.append(c + (" IS NOT NULL" if x else " IS NULL"))
            else:
                raise ErrorConsulta("operador de filtro no soportado en SQLite: " + str(op))
        return "(" + " AND ".join(partes) + ")" if partes else "1"
    if v is None:
        return c + " IS NULL"
    params.append(v)
    return c + " = ?"


def _donde_dict(f, params):
    partes = []
    for k, v in f.items():
        if k in ("$and", "$or"):
            sub = [_donde_dict(x, params) for x in v]
            partes.append("(" + (" AND " if k == "$and" else " OR ").join(sub) + ")" if sub else "1")
        else:
            partes.append(_condicion(k, v, params))
    return " AND ".join(partes) if partes else "1"


def _donde(filtro):
    """(" WHERE ...", parámetros). El filtro puede ser: None; un diccionario {"campo": valor,
    "otro": {"$gt": 3}, "$or": [...]}; una tupla ("edad > ? AND x = ?", [18, "a"]); o un texto
    que es la condición SQL tal cual (solo para código del programador, no para lo que escribe un
    usuario: ahí usa la tupla con parámetros)."""
    if filtro is None or filtro == "" or filtro == {}:
        return "", []
    if isinstance(filtro, str):
        return " WHERE " + filtro, []
    if isinstance(filtro, tuple):
        return " WHERE " + filtro[0], list(filtro[1]) if len(filtro) > 1 else []
    params = []
    return " WHERE " + _donde_dict(filtro, params), params


class SQLite(BaseDatos):
    tipo = "sqlite"

    def __init__(self, ruta):
        try:
            import sqlite3
        except ImportError:
            raise ErrorConexion("este Python no tiene el módulo sqlite3")
        self._m = sqlite3
        self.ruta = ruta
        self.origen = "sqlite:" + ruta
        self._tx = False
        self._clave = {}
        try:
            self._con = sqlite3.connect(ruta)
            # comprueba que es una base de datos (un fichero que no lo es falla al leerla)
            self._con.execute("SELECT count(*) FROM sqlite_master").fetchall()
        except sqlite3.Error as e:
            raise ErrorConexion("no se pudo abrir %s: %s" % (ruta, e))

    # --- núcleo: ejecuta y cierra el cursor
    def _sql(self, sql, params=()):
        try:
            return self._con.execute(sql, list(params))
        except self._m.IntegrityError as e:
            self._deshacer_implicita()
            raise ErrorIntegridad(str(e))
        except self._m.Error as e:
            self._deshacer_implicita()
            raise ErrorConsulta(str(e))

    def _deshacer_implicita(self):
        """Una sentencia que falla deja abierta la transacción que abrió sola: se deshace (si el
        programa no abrió una explícita con empezar())."""
        if not self._tx:
            try:
                self._con.rollback()
            except self._m.Error:
                pass

    def _fin_escritura(self):
        if not self._tx:
            self._con.commit()

    def consulta_sql(self, sql, parametros=()):
        """(columnas, filas) de un SELECT: columnas en orden y filas como listas."""
        cur = self._sql(sql, parametros)
        try:
            cols = [d[0] for d in cur.description] if cur.description else []
            filas = [list(f) for f in cur.fetchall()] if cols else []
        except self._m.Error as e:
            raise ErrorConsulta(str(e))
        return cols, filas

    def sql(self, sql, parametros=()):
        """Ejecuta una sentencia con parámetros enlazados (`?` o `:nombre`) y devuelve (columnas,
        filas como listas, filas afectadas). Un SELECT o PRAGMA trae columnas y filas; lo demás
        trae solo el número de filas afectadas (y se confirma si no hay transacción abierta)."""
        if isinstance(parametros, dict):
            try:
                cur = self._con.execute(sql, parametros)
            except self._m.IntegrityError as e:
                self._deshacer_implicita()
                raise ErrorIntegridad(str(e))
            except self._m.Error as e:
                self._deshacer_implicita()
                raise ErrorConsulta(str(e))
        else:
            cur = self._sql(sql, parametros)
        try:
            if cur.description:
                cols = [d[0] for d in cur.description]
                filas = [list(f) for f in cur.fetchall()]
                return cols, filas, len(filas)
            n = cur.rowcount
        except self._m.Error as e:
            raise ErrorConsulta(str(e))
        self._clave = {}
        self._fin_escritura()
        return [], [], n

    def ejecutar(self, sql, parametros=()):
        """Una sentencia SQL con parámetros enlazados. Un SELECT (o PRAGMA) devuelve la lista de
        diccionarios; lo demás, el número de filas afectadas. `bd.columnas_ultima` guarda el orden
        de las columnas del último SELECT."""
        cols, filas, n = self.sql(sql, parametros)
        if cols:
            self.columnas_ultima = cols
            return [dict(zip(cols, f)) for f in filas]
        return n

    columnas_ultima = []

    # --- transacciones
    def empezar(self):
        if not self._tx:
            self._sql("BEGIN")
            self._tx = True

    def confirmar(self):
        self._tx = False
        self._con.commit()

    def revertir(self):
        self._tx = False
        self._con.rollback()

    @property
    def en_transaccion(self):
        return self._tx

    # --- esquema
    def tablas(self):
        _, f = self.consulta_sql("SELECT name FROM sqlite_master WHERE type IN ('table','view') "
                                 "AND name NOT LIKE 'sqlite_%' ORDER BY name")
        return [x[0] for x in f]

    def es_vista(self, tabla):
        _, f = self.consulta_sql("SELECT type FROM sqlite_master WHERE name = ?", [tabla])
        return bool(f) and f[0][0] == "view"

    def esquema(self, tabla):
        _, f = self.consulta_sql("PRAGMA table_info(%s)" % _q(tabla))
        if not f:
            raise ErrorConsulta("no existe la tabla «%s»" % tabla)
        return [{"nombre": x[1], "tipo": x[2] or "", "obligatorio": bool(x[3]), "defecto": x[4],
                 "clave": x[5] > 0} for x in f]

    def indices(self, tabla):
        _, f = self.consulta_sql("PRAGMA index_list(%s)" % _q(tabla))
        r = []
        for x in f:
            _, cs = self.consulta_sql("PRAGMA index_info(%s)" % _q(x[1]))
            r.append({"nombre": x[1], "unico": bool(x[2]), "columnas": [c[2] for c in cs]})
        return r

    def clave(self, tabla):
        """La columna que identifica una fila: la clave primaria si es una, si no «rowid»."""
        k = self._clave.get(tabla)
        if k is None:
            if self.es_vista(tabla):
                k = ""
            else:
                pk = [c["nombre"] for c in self.esquema(tabla) if c["clave"]]
                k = pk[0] if len(pk) == 1 else "rowid"
            self._clave[tabla] = k
        return k

    def columnas(self, tabla):
        cols = [c["nombre"] for c in self.esquema(tabla)]
        return (["rowid"] + cols) if self.clave(tabla) == "rowid" else cols

    def crear_tabla(self, nombre, columnas):
        """columnas: [(nombre, tipo)] o [(nombre, tipo, clave)]; una clave INTEGER se autonumera."""
        if not columnas:
            raise ErrorConsulta("una tabla necesita al menos una columna")
        defs = []
        for c in columnas:
            tipo = str(c[1] or "TEXT").upper()
            if tipo not in TIPOS_SQL:
                raise ErrorConsulta("tipo de columna no válido: " + str(c[1]))
            d = "%s %s" % (_q(c[0]), tipo)
            if len(c) > 2 and c[2]:
                d += " PRIMARY KEY"
            defs.append(d)
        self._sql("CREATE TABLE %s (%s)" % (_q(nombre), ", ".join(defs)))
        self._clave = {}
        self._fin_escritura()

    def borrar_tabla(self, nombre):
        self._sql("DROP TABLE %s" % _q(nombre))
        self._clave = {}
        self._fin_escritura()

    # --- v8.4.4 (D333): lo que necesita el Explorador SQLite
    def objetos(self):
        """Todo lo que hay en la base: [{"tipo": table|view|index|trigger, "nombre", "tabla", "sql"}]."""
        _, f = self.consulta_sql("SELECT type, name, tbl_name, sql FROM sqlite_master WHERE name NOT LIKE 'sqlite_%' "
                                 "ORDER BY type, name")
        return [{"tipo": x[0], "nombre": x[1], "tabla": x[2], "sql": x[3] or ""} for x in f]

    def sql_de(self, nombre):
        _, f = self.consulta_sql("SELECT sql FROM sqlite_master WHERE name = ?", [nombre])
        return (f[0][0] or "") if f else ""

    def claves_foraneas(self, tabla):
        _, f = self.consulta_sql("PRAGMA foreign_key_list(%s)" % _q(tabla))
        return [{"desde": x[3], "tabla": x[2], "a": x[4]} for x in f]

    def definicion(self, tabla):
        """Las columnas con todo lo que el asistente edita: [{"nombre", "tipo", "pk", "obligatorio", "defecto",
        "unico", "fk"}] (fk = «tabla(columna)» o "")."""
        esq = self.esquema(tabla)
        fks = dict((x["desde"], "%s(%s)" % (x["tabla"], x["a"] or "")) for x in self.claves_foraneas(tabla))
        unicos = set()
        for ix in self.indices(tabla):
            if ix["unico"] and len(ix["columnas"]) == 1:
                unicos.add(ix["columnas"][0])
        npk = len([c for c in esq if c["clave"]])
        r = []
        for c in esq:
            r.append({"nombre": c["nombre"], "tipo": c["tipo"] or "", "pk": c["clave"] and npk == 1,
                      "obligatorio": c["obligatorio"], "defecto": c["defecto"],
                      "unico": c["nombre"] in unicos and not c["clave"], "fk": fks.get(c["nombre"], "")})
        return r

    @staticmethod
    def sql_crear(nombre, defs):
        """El CREATE TABLE de una lista de definiciones (las del asistente de columnas)."""
        if not defs:
            raise ErrorConsulta("una tabla necesita al menos una columna")
        partes, vistos = [], set()
        for d in defs:
            n = str(d.get("nombre", "")).strip()
            if not n:
                raise ErrorConsulta("una columna no tiene nombre")
            if n.lower() in vistos:
                raise ErrorConsulta("la columna «%s» está repetida" % n)
            vistos.add(n.lower())
            tipo = str(d.get("tipo") or "TEXT").upper()
            if tipo not in TIPOS_SQL:
                raise ErrorConsulta("tipo de columna no válido: " + tipo)
            t = "%s %s" % (_q(n), tipo)
            if d.get("pk"):
                t += " PRIMARY KEY"
            if d.get("obligatorio") and not d.get("pk"):
                t += " NOT NULL"
            if d.get("unico") and not d.get("pk"):
                t += " UNIQUE"
            df = d.get("defecto")
            if df not in (None, ""):
                t += " DEFAULT " + _literal_sql(df)
            fk = str(d.get("fk") or "").strip()
            if fk:
                ft, _, fc = fk.partition("(")
                t += " REFERENCES %s%s" % (_q(ft.strip()), "(%s)" % _q(fc.rstrip(")").strip()) if fc.strip(") ") else "")
            partes.append(t)
        return "CREATE TABLE %s (%s)" % (_q(nombre), ", ".join(partes))

    def crear_tabla_def(self, nombre, defs):
        self._sql(self.sql_crear(nombre, defs))
        self._clave = {}
        self._fin_escritura()

    def reconstruir(self, tabla, defs, origen=None):
        """Cambia la estructura de una tabla (el recurso estándar de SQLite): crea la nueva, copia los datos de las
        columnas que siguen (`origen` = {nombre nuevo: nombre viejo}; por defecto, el mismo nombre), borra la vieja y
        renombra. Conserva los índices que siguen siendo válidos. Todo en una transacción."""
        viejas = [c["nombre"] for c in self.esquema(tabla)]
        origen = origen or {}
        copiar = []
        for d in defs:
            v = origen.get(d["nombre"], d["nombre"])
            if v in viejas:
                copiar.append((d["nombre"], v))
        idx = [x for x in self.objetos() if x["tipo"] == "index" and x["tabla"] == tabla and x["sql"]]
        temp = tabla + "__nueva"
        propia = not self._tx
        if propia:
            self.empezar()
        try:
            self._sql(self.sql_crear(temp, defs))
            if copiar:
                self._sql("INSERT INTO %s (%s) SELECT %s FROM %s" % (
                    _q(temp), ", ".join(_q(a) for a, _ in copiar), ", ".join(_q(b) for _, b in copiar), _q(tabla)))
            self._sql("DROP TABLE %s" % _q(tabla))
            self._sql("ALTER TABLE %s RENAME TO %s" % (_q(temp), _q(tabla)))
            for x in idx:
                try:
                    self._sql(x["sql"])
                except ErrorBD:
                    pass
            if propia:
                self.confirmar()
        except ErrorBD:
            if propia:
                self.revertir()
            raise
        self._clave = {}

    def crear_indice(self, tabla, nombre, columnas, unico=False):
        if not columnas:
            raise ErrorConsulta("un índice necesita al menos una columna")
        self._sql("CREATE %sINDEX %s ON %s (%s)" % ("UNIQUE " if unico else "", _q(nombre), _q(tabla),
                                                     ", ".join(_q(c) for c in columnas)))
        self._fin_escritura()

    def borrar_indice(self, nombre):
        self._sql("DROP INDEX %s" % _q(nombre))
        self._fin_escritura()

    def integridad(self):
        """PRAGMA integrity_check: la lista de problemas; ["ok"] si la base está sana."""
        _, f = self.consulta_sql("PRAGMA integrity_check")
        return [str(x[0]) for x in f]

    def compactar(self):
        """VACUUM (hay que estar fuera de una transacción)."""
        if self._tx:
            raise ErrorConsulta("escribe o deshaz los cambios pendientes antes de compactar")
        self._sql("VACUUM")

    def plan(self, sql, parametros=()):
        """EXPLAIN QUERY PLAN de una sentencia: [(id, padre, detalle)]."""
        _, f = self.consulta_sql("EXPLAIN QUERY PLAN " + sql, parametros)
        return [(x[0], x[1], str(x[3])) for x in f]

    def volcado(self):
        """Una copia en SQL de toda la base (esquema y datos), como el .dump de sqlite3, en una lista de líneas."""
        L = ["BEGIN TRANSACTION;"]
        objs = self.objetos()
        for o in objs:
            if o["tipo"] == "table":
                L.append(o["sql"] + ";")
                cols, filas = self.consulta_sql("SELECT * FROM %s" % _q(o["nombre"]))
                for f in filas:
                    L.append("INSERT INTO %s VALUES(%s);" % (_q(o["nombre"]), ",".join(_literal_sql(v, True) for v in f)))
        for o in objs:
            if o["tipo"] in ("index", "view", "trigger") and o["sql"]:
                L.append(o["sql"] + ";")
        L.append("COMMIT;")
        return L

    def importar_filas(self, tabla, columnas, filas):
        """Inserta muchas filas (listas de textos o valores) con parámetros enlazados. Devuelve cuántas."""
        if not columnas:
            raise ErrorConsulta("no hay columnas que importar")
        sql = "INSERT INTO %s (%s) VALUES (%s)" % (_q(tabla), ", ".join(_q(c) for c in columnas), ",".join("?" * len(columnas)))
        n = 0
        for f in filas:
            self._sql(sql, [(f[i] if i < len(f) else None) for i in range(len(columnas))])
            n += 1
        self._fin_escritura()
        return n

    # --- datos
    def consultar(self, tabla, filtro=None, orden=None, limite=None, desplazamiento=0):
        """Filas como lista de diccionarios. Con clave «rowid» (tablas sin clave primaria
        sencilla) cada fila lleva también "rowid"."""
        donde, params = _donde(filtro)
        sel = "rowid AS rowid, *" if self.clave(tabla) == "rowid" else "*"
        sql = "SELECT %s FROM %s%s" % (sel, _q(tabla), donde)
        o = _orden_lista(orden)
        if o:
            sql += " ORDER BY " + ", ".join("%s %s" % (_q(c), "DESC" if d < 0 else "ASC") for c, d in o)
        if limite is not None or desplazamiento:
            sql += " LIMIT ? OFFSET ?"
            params = params + [-1 if limite is None else int(limite), int(desplazamiento or 0)]
        cols, filas = self.consulta_sql(sql, params)
        if sel != "*":
            cols = ["rowid"] + cols[1:]
        return [dict(zip(cols, f)) for f in filas]

    def contar(self, tabla, filtro=None):
        donde, params = _donde(filtro)
        return self.consulta_sql("SELECT count(*) FROM %s%s" % (_q(tabla), donde), params)[1][0][0]

    def insertar(self, tabla, fila):
        """Inserta un diccionario (los campos que falten toman su valor por defecto). Devuelve la
        clave de la fila nueva (el rowid)."""
        if fila:
            cols = list(fila.keys())
            sql = "INSERT INTO %s (%s) VALUES (%s)" % (_q(tabla), ", ".join(_q(c) for c in cols),
                                                        ", ".join("?" * len(cols)))
            cur = self._sql(sql, [fila[c] for c in cols])
        else:
            cur = self._sql("INSERT INTO %s DEFAULT VALUES" % _q(tabla))
        try:
            nuevo = cur.lastrowid
        except Exception:
            nuevo = None
        self._fin_escritura()
        return nuevo

    def _filtro_o_clave(self, tabla, filtro, clave, todos):
        if clave is not None:
            k = self.clave(tabla)
            if not k:
                raise ErrorConsulta("«%s» es una vista: no se puede escribir en ella" % tabla)
            return {k: clave}
        if filtro is None or filtro == "" or filtro == {}:
            if not todos:
                raise ErrorConsulta("hace falta un filtro o una clave (o todos=True para afectar a toda la tabla)")
            return None
        return filtro

    def actualizar(self, tabla, valores, filtro=None, clave=None, todos=False):
        """Cambia los campos de `valores` en las filas del filtro o de la clave. Devuelve cuántas."""
        if not valores:
            return 0
        f = self._filtro_o_clave(tabla, filtro, clave, todos)
        donde, params = _donde(f)
        cols = list(valores.keys())
        sql = "UPDATE %s SET %s%s" % (_q(tabla), ", ".join("%s = ?" % _q(c) for c in cols), donde)
        n = self._sql(sql, [valores[c] for c in cols] + params).rowcount
        self._fin_escritura()
        return n

    def borrar(self, tabla, filtro=None, clave=None, todos=False):
        f = self._filtro_o_clave(tabla, filtro, clave, todos)
        donde, params = _donde(f)
        n = self._sql("DELETE FROM %s%s" % (_q(tabla), donde), params).rowcount
        self._fin_escritura()
        return n

    def cerrar(self):
        if self._con is not None:
            try:
                if self._tx:
                    self._con.rollback()
                self._con.close()
            except Exception:
                pass
            self._con = None


# ------------------------------------------------------------ lite-monkey

def _tipo_lmk(v):
    if v is None:
        return "nulo"
    if isinstance(v, bool):
        return "logico"
    if isinstance(v, int):
        return "entero"
    if isinstance(v, float):
        return "real"
    if isinstance(v, str):
        return "texto"
    if isinstance(v, (list, tuple)):
        return "lista"
    if isinstance(v, dict):
        return "documento"
    if isinstance(v, (bytes, bytearray)):
        return "binario"
    n = type(v).__name__
    if n == "ObjectId":
        return "objectid"
    if n in ("Fecha", "datetime"):
        return "fecha"
    return n.lower()


def _es_hex24(s):
    if not isinstance(s, str) or len(s) != 24:
        return False
    for c in s:
        if c not in "0123456789abcdefABCDEF":
            return False
    return True


class LiteMonkey(BaseDatos):
    tipo = "litemonkey"

    def __init__(self, host, puerto, base, usuario=None, clave=None, timeout=10):
        try:
            import litemonkey
        except ImportError:
            raise ErrorConexion("no está el módulo litemonkey")
        self._m = litemonkey
        self.host, self.puerto, self.nombre_base = host, puerto, base
        self.origen = "litemonkey://%s:%d/%s" % (host, puerto, base)
        try:
            self._cli = litemonkey.Cliente(host, puerto, usuario=usuario, clave=clave, timeout=timeout)
        except litemonkey.errors.LiteMonkeyError as e:
            raise ErrorConexion("no se pudo conectar con lite-monkey en %s:%d (%s)" % (host, puerto, e))
        except OSError as e:
            raise ErrorConexion("no se pudo conectar con lite-monkey en %s:%d (%s)" % (host, puerto, e))
        self._db = self._cli[base]

    def _col(self, nombre):
        return self._db[nombre]

    def _envolver(self, e):
        E = self._m.errors
        if isinstance(e, E.DuplicateKeyError):
            return ErrorIntegridad(str(e))
        if isinstance(e, E.ConnectionFailure):
            return ErrorConexion(str(e))
        return ErrorConsulta(str(e))

    def _filtro(self, filtro):
        """El filtro como documento: None, diccionario o un texto JSON (con la sintaxis del cliente)."""
        if filtro is None or filtro == "":
            return {}
        if isinstance(filtro, str):
            try:
                filtro = json.loads(filtro)
            except ValueError as e:
                raise ErrorConsulta("el filtro no es un JSON válido: %s" % e)
        if not isinstance(filtro, dict):
            raise ErrorConsulta("el filtro de lite-monkey es un diccionario (o su JSON)")
        return self._ids(filtro)

    def _ids(self, f):
        """Un «_id» escrito como 24 cifras hexadecimales se convierte en ObjectId."""
        r = {}
        for k, v in f.items():
            if k == "_id":
                if _es_hex24(v):
                    v = self._m.ObjectId(v)
                elif isinstance(v, dict):
                    v = dict((o, [self._m.ObjectId(i) if _es_hex24(i) else i for i in x] if o in ("$in", "$nin") else
                             (self._m.ObjectId(x) if _es_hex24(x) else x)) for o, x in v.items())
            elif k in ("$and", "$or", "$nor"):
                v = [self._ids(x) for x in v]
            elif isinstance(v, dict) and "$contiene" in v:
                v = dict(v)
                x = v.pop("$contiene")
                esc = "".join("\\" + c if c in "^$.*+?[]\\()|{}" else c for c in str(x))
                v["$regex"] = esc
                v["$options"] = "i"
            r[k] = v
        return r

    def bases(self):
        return sorted(self._cli.list_database_names())

    def tablas(self):
        try:
            return sorted(self._db.list_collection_names())
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def esquema(self, tabla, muestra=200):
        """Los campos deducidos de una muestra de documentos: [{"nombre", "tipo", "clave",
        "frecuencia"}] (tipo «entero|texto» si el campo no es siempre del mismo tipo)."""
        try:
            docs = list(self._col(tabla).find({}).limit(muestra))
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)
        tipos, cuenta = {}, {}
        for d in docs:
            for k, v in d.items():
                t = _tipo_lmk(v)
                lst = tipos.setdefault(k, [])
                if t not in lst:
                    lst.append(t)
                cuenta[k] = cuenta.get(k, 0) + 1
        campos = ["_id"] + sorted(k for k in tipos if k != "_id") if "_id" in tipos else sorted(tipos)
        return [{"nombre": k, "tipo": "|".join(tipos[k]), "clave": k == "_id", "obligatorio": k == "_id",
                 "defecto": None, "frecuencia": cuenta[k] * 100 // max(1, len(docs))} for k in campos]

    def indices(self, tabla):
        try:
            info = self._col(tabla).index_information()
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)
        return [{"nombre": n, "unico": bool(i.get("unique")), "columnas": [k for k, _ in i["key"]]}
                for n, i in sorted(info.items())]

    def clave(self, tabla):
        return "_id"

    def crear_tabla(self, nombre, columnas=None):
        try:
            self._db.create_collection(nombre)
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def borrar_tabla(self, nombre):
        try:
            self._db.drop_collection(nombre)
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def consultar(self, tabla, filtro=None, orden=None, limite=None, desplazamiento=0):
        try:
            cur = self._col(tabla).find(self._filtro(filtro))
            o = _orden_lista(orden)
            if o:
                cur = cur.sort(o)
            if desplazamiento:
                cur = cur.skip(int(desplazamiento))
            if limite is not None:
                cur = cur.limit(int(limite))
            return list(cur)
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def contar(self, tabla, filtro=None):
        try:
            return self._col(tabla).count_documents(self._filtro(filtro))
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def insertar(self, tabla, fila):
        try:
            return self._col(tabla).insert_one(dict(fila)).inserted_id
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def _f(self, filtro, clave, todos):
        if clave is not None:
            return self._ids({"_id": clave})
        if filtro is None or filtro == "" or filtro == {}:
            if not todos:
                raise ErrorConsulta("hace falta un filtro o una clave (o todos=True para afectar a toda la colección)")
            return {}
        return self._filtro(filtro)

    def actualizar(self, tabla, valores, filtro=None, clave=None, todos=False):
        if not valores:
            return 0
        f = self._f(filtro, clave, todos)
        sets = dict((k, v) for k, v in valores.items() if k != "_id")
        try:
            return self._col(tabla).update_many(f, {"$set": sets}).matched_count
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def borrar(self, tabla, filtro=None, clave=None, todos=False):
        f = self._f(filtro, clave, todos)
        try:
            return self._col(tabla).delete_many(f).deleted_count
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    # --- v8.4.4 (D333): lo que necesita el Explorador lite-monkey
    def bases_con_colecciones(self):
        """[(base, [colecciones])] de todo el servidor."""
        r = []
        for b in self.bases():
            try:
                r.append((b, sorted(self._cli[b].list_collection_names())))
            except self._m.errors.LiteMonkeyError:
                r.append((b, []))
        return r

    def usar_base(self, base):
        """Cambia la base con la que se trabaja (el servidor las crea al escribir)."""
        self.nombre_base = base
        self._db = self._cli[base]
        self.origen = "litemonkey://%s:%d/%s" % (self.host, self.puerto, base)

    def estadisticas(self, tabla):
        """{"n": documentos, "tam": bytes de datos, "indices": cuántos}; lo que el servidor no dé, 0."""
        try:
            e = self._col(tabla).stats()
        except self._m.errors.LiteMonkeyError as ex:
            raise self._envolver(ex)
        return {"n": int(e.get("count", 0) or 0), "tam": int(e.get("size", e.get("storageSize", 0)) or 0),
                "indices": int(e.get("nindexes", 0) or 0)}

    def consultar_proy(self, tabla, filtro=None, proyeccion=None, orden=None, limite=None, desplazamiento=0):
        """Como consultar(), con proyección ({"campo": 1} o {"campo": 0})."""
        try:
            cur = self._col(tabla).find(self._filtro(filtro), proyeccion or None)
            o = _orden_lista(orden)
            if o:
                cur = cur.sort(o)
            if desplazamiento:
                cur = cur.skip(int(desplazamiento))
            if limite:
                cur = cur.limit(int(limite))
            return list(cur)
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def reemplazar(self, tabla, clave, documento):
        """Sustituye el documento de esa clave por `documento` (sin _id; conserva el de la clave)."""
        d = dict((k, v) for k, v in documento.items() if k != "_id")
        try:
            return self._col(tabla).replace_one(self._ids({"_id": clave}), d).matched_count
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def actualizar_op(self, tabla, filtro, operadores):
        """update_many con un documento de operadores ($set, $inc...). Devuelve cuántos casan."""
        try:
            return self._col(tabla).update_many(self._filtro(filtro), operadores).matched_count
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def insertar_muchos(self, tabla, documentos):
        try:
            if not documentos:
                return 0
            return len(self._col(tabla).insert_many([dict(d) for d in documentos]).inserted_ids)
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def agregar(self, tabla, etapas):
        """aggregate con una lista de etapas ($match, $group, $sort, $project, $limit...)."""
        try:
            return list(self._col(tabla).aggregate(list(etapas)))
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def crear_indice(self, tabla, campo, unico=False, descendente=False):
        try:
            return self._col(tabla).create_index([(campo, -1 if descendente else 1)], unique=bool(unico))
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def borrar_indice(self, tabla, nombre):
        try:
            self._col(tabla).drop_index(nombre)
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def borrar_base(self, base):
        try:
            self._cli.drop_database(base)
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def explicar(self, tabla, filtro=None):
        try:
            return self._col(tabla).find(self._filtro(filtro)).explain()
        except self._m.errors.LiteMonkeyError as e:
            raise self._envolver(e)

    def ejecutar(self, sql, parametros=()):
        raise ErrorBD("ejecutar() solo existe en SQLite; en lite-monkey usa consultar() con un filtro")

    def sql(self, sql, parametros=()):
        raise ErrorBD("las consultas SQL solo existen en SQLite")

    def consulta_sql(self, sql, parametros=()):
        raise ErrorBD("las consultas SQL solo existen en SQLite")

    def empezar(self):
        raise ErrorBD("lite-monkey no tiene transacciones")

    confirmar = revertir = empezar

    def cerrar(self):
        if self._cli is not None:
            try:
                self._cli.close()
            except Exception:
                pass
            self._cli = None


# ------------------------------------------------------------ conectar

def analizar(origen):
    """Una cadena de conexión como (tipo, partes): ("sqlite", ruta) o ("litemonkey", (host, puerto,
    base, usuario, clave)). Admite «sqlite:/ruta», «sqlite:///ruta», «:memory:», una ruta que
    acabe en .db/.sqlite/.sqlite3 y «litemonkey://[usuario:clave@]host[:puerto]/base»."""
    if not isinstance(origen, str) or not origen.strip():
        raise ErrorConexion("falta el origen de la conexión")
    o = origen.strip()
    if o.startswith("sqlite:"):
        r = o[7:]
        if r.startswith("//"):
            r = r[2:]
        if not r:
            raise ErrorConexion("falta la ruta del fichero en «%s»" % origen)
        return "sqlite", r
    if o == ":memory:" or o.endswith(".db") or o.endswith(".sqlite") or o.endswith(".sqlite3") or o.endswith(".db3"):
        return "sqlite", o
    if o.startswith("litemonkey://") or o.startswith("lmk://"):
        r = o.split("://", 1)[1]
        base = "test"
        if "/" in r:
            r, base = r.split("/", 1)
            base = base or "test"
        usuario = clave = None
        if "@" in r:
            cred, r = r.rsplit("@", 1)
            if ":" in cred:
                usuario, clave = cred.split(":", 1)
            else:
                usuario = cred
        puerto = 7979
        if ":" in r:
            r, p = r.rsplit(":", 1)
            try:
                puerto = int(p)
            except ValueError:
                raise ErrorConexion("puerto no válido en «%s»" % origen)
        return "litemonkey", (r or "127.0.0.1", puerto, base, usuario, clave)
    raise ErrorConexion("origen desconocido: «%s» (usa sqlite:/ruta o litemonkey://host:7979/base)" % origen)


def conectar(origen):
    """Abre la base de datos que dice la cadena de conexión y devuelve su objeto."""
    tipo, p = analizar(origen)
    if tipo == "sqlite":
        return SQLite(p)
    return LiteMonkey(*p)
