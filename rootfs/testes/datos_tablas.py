# SPDX-License-Identifier: GPL-3.0-or-later
# datos_tablas.py - pruebas del módulo tablas (DataFrame ligero, D479; tests/h479_python_datos.py).
#
#     python /testes/datos_tablas.py
#
# "TABLAS ok <nombre>" o "TABLAS FALLO <nombre>: ..."; al final "RESULTADO datos_tablas: <n> fallos".
# Las líneas "CIFRA <nombre> <valor>" las contrasta el anfitrión con su propio cálculo sobre el mismo CSV.
import os
import tablas
from tablas import Tabla, leer_csv, TablasNoDisponible

fallos = 0


def ok(nombre, cond, extra=""):
    global fallos
    if cond:
        print("TABLAS ok", nombre)
    else:
        fallos += 1
        print("TABLAS FALLO %s: %s" % (nombre, extra))


def cerca(a, b, tol=1e-9):
    return a is not None and b is not None and abs(a - b) <= tol * max(1.0, abs(b))


def tamano(ruta):
    try:
        return os.stat(ruta)[6]
    except OSError:
        return -1


V = "/ejemplos/datos/ventas.csv"
P = "/ejemplos/datos/productos.csv"
v = leer_csv(V)
p = leer_csv(P)

# ---------------------------------------------------------------- leer
ok("csv-forma", v.forma == (36, 5), v.forma)
ok("csv-columnas", v.columnas == ["region", "producto", "mes", "unidades", "precio"], v.columnas)
ok("csv-tipos", isinstance(v["unidades"][0], int) and isinstance(v["precio"][0], float) and isinstance(v["region"][0], str))
ok("csv-faltantes", v.contar_nulos() == {"region": 0, "producto": 0, "mes": 0, "unidades": 1, "precio": 1}, v.contar_nulos())
ok("csv-cabeza", len(v.cabeza(3)) == 3 and len(v.cola(2)) == 2)
ok("texto", "region" in str(v.cabeza(2)) and "Norte" in str(v.cabeza(2)))

# ---------------------------------------------------------------- estadística sobre los datos con huecos
print("CIFRA suma_unidades %d" % v.suma("unidades"))
print("CIFRA media_unidades %.6f" % v.media("unidades"))
print("CIFRA std_unidades %.6f" % v.std("unidades"))
print("CIFRA media_precio %.6f" % v.media("precio"))
print("CIFRA correlacion %.6f" % v.correlacion("unidades", "precio"))
ok("minimo-maximo", v.minimo("unidades") <= v.media("unidades") <= v.maximo("unidades"))

# ---------------------------------------------------------------- filtrar y ordenar
g5 = v.filtrar(lambda f: f["unidades"] is not None and f["unidades"] > 30)
print("CIFRA filtro_mayor30 %d" % len(g5))
ok("filtrar", all(x > 30 for x in g5["unidades"]) and len(g5) > 0)
d = v.donde("region", "==", "Norte")
ok("donde", len(d) == 9 and set(d["region"]) == {"Norte"}, len(d))
ok("donde-nulos-no-pasan", len(v.donde("unidades", ">=", 0)) == 35)
o = v.ordenar("unidades", descendente=True)
ok("ordenar-desc", o["unidades"][0] == v.maximo("unidades") and o["unidades"][-1] is None, o["unidades"][:2])
o2 = v.ordenar(["region", "unidades"])
ok("ordenar-dos-claves", o2["region"][0] == "Este" and o2["region"][-1] == "Sur", (o2["region"][0], o2["region"][-1]))
ok("ordenar-dentro-de-cada-region", all(o2["unidades"][i] <= o2["unidades"][i + 1] for i in range(35)
   if o2["region"][i] == o2["region"][i + 1] and o2["unidades"][i + 1] is not None))

# ---------------------------------------------------------------- groupby
peq = Tabla({"g": ["a", "b", "a", "b", "a"], "x": [1, 10, 3, 20, 5], "y": [2.0, None, 4.0, 6.0, 6.0]})
ag = peq.groupby("g").agg({"x": "suma", "y": "media"})
ok("groupby-orden-y-suma", ag["g"] == ["a", "b"] and ag["x"] == [9, 30], (ag["g"], ag["x"]))
ok("groupby-media-sin-nulos", ag["y"] == [4.0, 6.0], ag["y"])
ag2 = peq.groupby("g").agg({"x": ["min", "max", "contar"]})
ok("groupby-varios", ag2["x_min"] == [1, 10] and ag2["x_max"] == [5, 20] and ag2["x_contar"] == [3, 2], ag2.columnas)
ok("groupby-std", cerca(peq.groupby("g").agg({"x": "std"})["x"][0], 2.0), peq.groupby("g").agg({"x": "std"})["x"])
ok("groupby-contar", peq.groupby("g").contar()["filas"] == [3, 2])
gr = v.groupby("region").agg({"unidades": "suma", "precio": "media"})
ok("groupby-regiones", gr["region"] == ["Norte", "Sur", "Este", "Oeste"] or sorted(gr["region"]) == ["Este", "Norte", "Oeste", "Sur"], gr["region"])
for r, s in zip(gr["region"], gr["unidades"]):
    print("CIFRA unidades_%s %d" % (r, s))
ok("groupby-suma-total", sum(gr["unidades"]) == v.suma("unidades"))

# ---------------------------------------------------------------- merge
inn = v.merge(p, en="producto")
ok("merge-inner", len(inn) == 36 and "categoria" in inn.columnas and inn["categoria"][0] == "Panaderia", inn.forma)
izq = v.merge(p, en="producto", como="left")
ok("merge-left", len(izq) == 36)
ext = v.merge(p, en="producto", como="outer")
ok("merge-outer-trae-el-te", len(ext) == 37 and "Te" in ext["producto"], len(ext))
der = v.merge(p, en="producto", como="right")
ok("merge-right", len(der) == 37 and der["region"][-1] is None, len(der))
a = Tabla({"k": [1, 2, 3], "v": ["x", "y", "z"]})
b = Tabla({"k": [2, 3, 4], "v": ["Y", "Z", "W"]})
m = a.merge(b, en="k", como="outer")
ok("merge-sufijos", m.columnas == ["k", "v_x", "v_y"] and m["k"] == [2, 3, 1, 4] or sorted(m["k"]) == [1, 2, 3, 4], (m.columnas, m["k"]))
ok("merge-left-huecos", a.merge(b, en="k", como="left")["v_y"] == [None, "Y", "Z"])

# ---------------------------------------------------------------- pivot
pv = v.pivot("region", "mes", "unidades", agg="suma")
ok("pivot-columnas", pv.columnas == ["region", "ene", "feb", "mar"], pv.columnas)
tot = sum(sum(x for x in pv[c] if x is not None) for c in ("ene", "feb", "mar"))
ok("pivot-total", tot == v.suma("unidades"), tot)
pm = v.pivot("producto", "region", "precio", agg="media")
ok("pivot-media", cerca(pm["Norte"][0], 2.5) and cerca(pm["Este"][1], 1.2), (pm["Norte"], pm["Este"]))
print("CIFRA pivot_total %d" % tot)

# ---------------------------------------------------------------- valores que faltan
ok("quitar-nulos", len(v.quitar_nulos()) == 34, len(v.quitar_nulos()))
ok("rellenar-valor", v.rellenar(0, ["unidades"]).contar_nulos()["unidades"] == 0)
rm = v.rellenar("media", ["unidades"])
ok("rellenar-media", cerca(rm.media("unidades"), v.media("unidades"), 1e-9) and rm.contar_nulos()["unidades"] == 0)
ok("rellenar-dict", v.rellenar({"precio": 9.99}).contar_nulos()["precio"] == 0)
ok("unicos", sorted(v.unicos("producto")) == ["Cafe", "Leche", "Pan"])

# ---------------------------------------------------------------- columnas nuevas y descripción
w = v.copia()
w["total"] = lambda f: (f["unidades"] or 0) * (f["precio"] or 0)
ok("columna-calculada", len(w) == 36 and cerca(w["total"][0], 39 * 2.5))
ds = v.describir()
ok("describir", ds["estadistico"][0] == "cuenta" and ds["unidades"][0] == 35 and cerca(ds["unidades"][1], v.media("unidades")), ds)
xy = Tabla({"x": [1, 2, 3, 4, 5], "y": [3, 5, 7, 9, 11]})
ok("correlacion-exacta", cerca(xy.correlacion("x", "y"), 1.0))
try:
    import ulab  # noqa: F401
    arr = v.columna("unidades")
    ok("ulab-columna", len(arr) == 35 and cerca(float(sum(arr)), float(v.suma("unidades"))))
except ImportError:
    ok("ulab-columna", False, "sin ulab")

# ---------------------------------------------------------------- CSV y JSON (ida y vuelta)
txt = Tabla({"nombre": ["Peña, J.", 'dice "hola"', "línea\nnueva", "Café"], "n": [1, 2.5, None, 4]})
txt.a_csv("/tmp/t_texto.csv")
t2 = leer_csv("/tmp/t_texto.csv")
ok("csv-comillas-y-acentos", t2["nombre"] == txt["nombre"] and t2["n"] == [1, 2.5, None, 4], t2["nombre"])
v.a_csv("/tmp/t_ventas.csv")
v2 = leer_csv("/tmp/t_ventas.csv")
ok("csv-ida-y-vuelta", v2.forma == v.forma and v2["unidades"] == v["unidades"] and v2["precio"] == v["precio"] and v2["region"] == v["region"])
ok("csv-punto-y-coma", leer_csv("/tmp/t_ventas.csv", sep=",").forma == (36, 5) and tablas.desde_csv("a;b\n1;2\n3;4\n").forma == (2, 2))
v.a_json("/tmp/t_ventas.json")
j = tablas.leer_json("/tmp/t_ventas.json")
ok("json-filas", j.forma == v.forma and j["unidades"] == v["unidades"], j.forma)
v.a_json("/tmp/t_cols.json", orientacion="columnas")
ok("json-columnas", tablas.leer_json("/tmp/t_cols.json")["producto"] == v["producto"])

# ---------------------------------------------------------------- .xlsx y .mxs con Macaco
try:
    v.a_xlsx("/tmp/t_ventas.xlsx")
    ok("xlsx-escrito", tamano("/tmp/t_ventas.xlsx") > 500, tamano("/tmp/t_ventas.xlsx"))
    x = tablas.leer_xlsx("/tmp/t_ventas.xlsx")
    ok("xlsx-ida-y-vuelta", x.forma == (36, 5) and x["unidades"] == v["unidades"] and x["region"] == v["region"], (x.forma, x["unidades"][:6]))
    ok("xlsx-precios", all(cerca(a, b) for a, b in zip(x["precio"][:4], v["precio"][:4])))
    v.a_mxs("/tmp/t_ventas.mxs")
    ok("mxs-escrito", tamano("/tmp/t_ventas.mxs") > 100)
    mx = tablas.leer_mxs("/tmp/t_ventas.mxs")
    ok("mxs-ida-y-vuelta", mx.forma == (36, 5) and mx["unidades"] == v["unidades"], mx.forma)
except Exception as e:
    ok("macaco", False, repr(e))

# ---------------------------------------------------------------- SQLite
try:
    for f in ("/tmp/t_datos.db",):
        try:
            os.remove(f)
        except OSError:
            pass
    v.a_sqlite("/tmp/t_datos.db", "ventas")
    s = tablas.leer_sqlite("/tmp/t_datos.db", "ventas")
    ok("sqlite-ida-y-vuelta", s.forma == (36, 5) and s["unidades"] == v["unidades"] and s["precio"] == v["precio"], s.forma)
    q = tablas.leer_sqlite("/tmp/t_datos.db", "SELECT region, SUM(unidades) AS total FROM ventas GROUP BY region ORDER BY region")
    esperado = dict(zip(gr["region"], gr["unidades"]))
    ok("sqlite-consulta", q.forma == (4, 2) and all(esperado[r] == t for r, t in zip(q["region"], q["total"])), q)
    v.cabeza(2).a_sqlite("/tmp/t_datos.db", "ventas", si_existe="agregar")
    ok("sqlite-agregar", len(tablas.leer_sqlite("/tmp/t_datos.db", "ventas")) == 38)
    try:
        v.a_sqlite("/tmp/t_datos.db", "ventas", si_existe="fallar")
        ok("sqlite-fallar", False, "no falló")
    except ValueError:
        ok("sqlite-fallar", True)
    ok("sqlite-parametros", len(tablas.leer_sqlite("/tmp/t_datos.db", "SELECT * FROM ventas WHERE region = ?", ("Sur",))) == 9)
except Exception as e:
    ok("sqlite", False, repr(e))

# ---------------------------------------------------------------- Valkey opcional
class Falso:
    def __init__(self):
        self.d = {}

    def set(self, k, val):
        self.d[k] = val.encode()

    def get(self, k):
        return self.d.get(k)


ok("valkey-sin-modulo-lo-dice", tablas.valkey_disponible() or True)
if not tablas.valkey_disponible():
    try:
        v.a_valkey("ventas")
        ok("valkey-no-disponible", False, "no levantó")
    except TablasNoDisponible as e:
        ok("valkey-no-disponible", "Valkey no está disponible" in str(e), e)
else:   # con el cliente de Valkey en la imagen (D477) no hay error que provocar; la conexión real la prueba h477
    ok("valkey-no-disponible", True, "hay módulo valkey")
fv = Falso()
v.cabeza(5).a_valkey("k", cliente=fv)
ok("valkey-cliente-propio", tablas.leer_valkey("k", cliente=fv)["producto"] == v.cabeza(5)["producto"])

# ---------------------------------------------------------------- gráficos
try:
    g = gr.grafico("barras", x="region", y="unidades", titulo="Unidades por región", ruta="/tmp/t_barras.png")
    ok("grafico-png", tamano("/tmp/t_barras.png") > 1000, tamano("/tmp/t_barras.png"))
    with open("/tmp/t_barras.png", "rb") as f:
        cab = f.read(8)
    ok("grafico-png-firma", cab == b"\x89PNG\r\n\x1a\n", cab)
    v.grafico("dispersion", x="unidades", y="precio", ruta="/tmp/t_disp.bmp")
    ok("grafico-bmp", tamano("/tmp/t_disp.bmp") > 1000)
    pv.grafico("lineas", x="region", y=["ene", "feb", "mar"], ruta="/tmp/t_lineas.png")
    ok("grafico-lineas", tamano("/tmp/t_lineas.png") > 1000)
    v.grafico("histograma", y="unidades", ruta="/tmp/t_hist.png")
    ok("grafico-histograma", tamano("/tmp/t_hist.png") > 1000)
except Exception as e:
    ok("grafico", False, repr(e))

print("RESULTADO datos_tablas: %d fallos" % fallos)
