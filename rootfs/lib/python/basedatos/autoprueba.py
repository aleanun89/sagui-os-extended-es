# SPDX-License-Identifier: GPL-2.0-or-later
# basedatos.autoprueba - comprobaciones de la biblioteca `basedatos` y de los componentes de datos de
# VisualPythonS dentro de SaguiOS (mejora 127, D304). Escribe «bd-autoprueba: ok ...» o «FALLO ...».
# Lo usa tests/h304_bd_visual.py.
#
#     python /lib/python/basedatos/autoprueba.py [--lmk PUERTO]
import os
import sys
import basedatos as B

RUTA = "/tmp/bd_autoprueba.db"


def ok(msg):
    print("bd-autoprueba: ok " + msg)


def fallo(msg):
    print("bd-autoprueba: FALLO " + msg)


def comprobar(cond, msg):
    (ok if cond else fallo)(msg)


def lanza(exc, f, *a, **kw):
    try:
        f(*a, **kw)
    except exc:
        return True
    except Exception as e:
        print("bd-autoprueba: (otra excepción %r)" % (e,))
        return False
    return False


def preparar():
    try:
        os.remove(RUTA)
    except OSError:
        pass
    bd = B.conectar("sqlite:" + RUTA)
    bd.ejecutar("CREATE TABLE contactos (id INTEGER PRIMARY KEY, nombre TEXT NOT NULL, ciudad TEXT, edad INTEGER, activo INTEGER DEFAULT 0)")
    bd.ejecutar("CREATE INDEX i_ciudad ON contactos(ciudad)")
    ciudades = ["Madrid", "Sevilla", "Lima", "Bogota"]
    with bd.transaccion():
        for i in range(120):
            bd.insertar("contactos", {"nombre": "Persona %03d" % i, "ciudad": ciudades[i % 4], "edad": 20 + i % 40})
    bd.cerrar()


def biblioteca():
    bd = B.conectar("sqlite:" + RUTA)
    comprobar(bd.tablas() == ["contactos"], "sqlite: tablas()")
    es = bd.esquema("contactos")
    comprobar([c["nombre"] for c in es] == ["id", "nombre", "ciudad", "edad", "activo"] and es[0]["clave"], "sqlite: esquema")
    comprobar(bd.indices("contactos")[0]["columnas"] == ["ciudad"], "sqlite: indices")
    comprobar(bd.contar("contactos") == 120, "sqlite: contar")
    f = bd.consultar("contactos", {"ciudad": "Lima"}, orden="-nombre", limite=3, desplazamiento=1)
    comprobar(len(f) == 3 and f[0]["nombre"] == "Persona 114", "sqlite: filtro, orden, limite y desplazamiento")
    bd.insertar("contactos", {"nombre": "O'Brien \"el\" 'x'; DROP TABLE contactos; --", "edad": 1})
    comprobar("contactos" in bd.tablas() and bd.contar("contactos", {"edad": 1}) == 1, "sqlite: un valor hostil es solo un dato")
    f = bd.consultar("contactos", {"edad": 1})
    comprobar(f[0]["nombre"].startswith("O'Brien"), "sqlite: las comillas vuelven intactas")
    comprobar(bd.borrar("contactos", {"edad": 1}) == 1, "sqlite: borrar con filtro")
    comprobar(bd.actualizar("contactos", {"edad": 99}, clave=5) == 1 and bd.consultar("contactos", {"id": 5})[0]["edad"] == 99, "sqlite: actualizar por clave")
    comprobar(lanza(B.ErrorConsulta, bd.borrar, "contactos"), "sqlite: borrar sin filtro se niega")
    comprobar(lanza(B.ErrorIntegridad, bd.insertar, "contactos", {"edad": 3}), "sqlite: NOT NULL da ErrorIntegridad")
    antes = bd.contar("contactos")
    try:
        with bd.transaccion():
            bd.insertar("contactos", {"nombre": "T"})
            raise RuntimeError("x")
    except RuntimeError:
        pass
    comprobar(bd.contar("contactos") == antes, "sqlite: la transaccion con error se deshace")
    r = bd.ejecutar("SELECT nombre FROM contactos WHERE edad > ? ORDER BY id LIMIT 2", [90])
    comprobar(len(r) == 1 and r[0]["nombre"] == "Persona 004", "sqlite: ejecutar con parametros")
    comprobar(lanza(B.ErrorConsulta, bd.ejecutar, "SELEC 1"), "sqlite: SQL mal escrito da ErrorConsulta")
    bd.cerrar()
    comprobar(lanza(B.ErrorConexion, B.conectar, "mysql://x"), "origen desconocido: ErrorConexion")
    comprobar(B.valor_desde_texto("12", "INTEGER") == 12 and B.valor_desde_texto("", "REAL") is None, "valor_desde_texto")
    comprobar(B.a_csv(["a", "b"], [[1, 'x,"y"']]) == 'a,b\n1,"x,""y"""\n', "a_csv")


def litemonkey(puerto):
    bd = B.conectar("litemonkey://127.0.0.1:%d/autoprueba" % puerto)
    for i in range(30):
        bd.insertar("personas", {"nombre": "Doc %02d" % i, "edad": 20 + i, "activo": i % 2 == 0})
    comprobar("personas" in bd.colecciones(), "lmk: colecciones()")
    comprobar(bd.contar("personas") == 30, "lmk: contar")
    es = dict((c["nombre"], c["tipo"]) for c in bd.esquema("personas"))
    comprobar(es.get("edad") == "entero" and es.get("activo") == "logico" and "_id" in es, "lmk: esquema deducido")
    f = bd.consultar("personas", {"edad": {"$gte": 40}}, orden="-edad", limite=2)
    comprobar(len(f) == 2 and f[0]["edad"] == 49, "lmk: filtro con operadores, orden y limite")
    oid = f[0]["_id"]
    comprobar(bd.actualizar("personas", {"edad": 77}, clave=str(oid)) == 1, "lmk: actualizar por _id (texto)")
    comprobar(bd.consultar("personas", '{"edad": 77}')[0]["_id"] == oid, "lmk: filtro como JSON")
    comprobar(bd.borrar("personas", clave=oid) == 1 and bd.contar("personas") == 29, "lmk: borrar por clave")
    comprobar(lanza(B.ErrorBD, bd.ejecutar, "SELECT 1"), "lmk: ejecutar() no existe")
    bd.cerrar()


def componentes():
    import vps
    from vps import Formulario, ConectorBD, RejillaDatos, CuadroTexto, Casilla, Combo, Etiqueta
    from vps import _autocompletar as AC, _modelo as M
    comprobar("ConectorBD" in vps.TIPOS and "RejillaDatos" in vps.TIPOS, "vps: los tipos estan registrados")
    f = Formulario("Agenda", 560, 380)
    BD = ConectorBD(f, nombre="BD", origen="sqlite:" + RUTA, tabla="contactos", tam_pagina=25)
    Rej = RejillaDatos(f, 8, 8, 520, 190, nombre="Rej", fuente=BD, editable=True)
    TN = CuadroTexto(f, 8, 230, 160, 22, nombre="TN", fuente=BD, campo="nombre")
    CA = Casilla(f, "Activo", 8, 260, 100, 20, nombre="CA", fuente=BD, campo="activo")
    CC = Combo(f, 8, 290, 160, 22, nombre="CC", fuente=BD, campo="ciudad", elementos=["Madrid", "Sevilla", "Lima", "Bogota"], editable=True)
    sucesos = []
    BD.conectar("conectado", lambda: sucesos.append("conectado"))
    BD.conectar("error", lambda: sucesos.append("error"))
    comprobar(BD.abrir() and sucesos == ["conectado"], "conector: abrir() dispara conectado")
    comprobar(BD.total == 120 and len(BD.filas) == 25 and BD.paginas == 5, "conector: total, pagina de 25 y 5 paginas")
    comprobar(TN.texto == "Persona 000" and CC.texto == "Madrid", "enlazados: se rellenan con el primer registro")
    comprobar(len(Rej._g.filas) == 25 and Rej._g.columnas == ["id", "nombre", "ciudad", "edad", "activo"], "rejilla: 25 filas y las columnas en orden")
    BD.siguiente()
    comprobar(BD.posicion == 1 and TN.texto == "Persona 001" and CC.texto == "Sevilla", "navegar: siguiente rellena los controles")
    comprobar(Rej._g.fila == 1, "navegar: la rejilla sigue al conector")
    BD.ir_a(60)
    comprobar(BD.pagina == 2 and BD.filas[0]["nombre"] == "Persona 050" and TN.texto == "Persona 060", "paginacion: ir_a(60) carga la pagina 3")
    BD.ultimo()
    comprobar(BD.posicion == 119 and TN.texto == "Persona 119", "navegar: ultimo")
    BD.primero()
    # editar en un control enlazado y guardar
    TN.texto = "Editado desde la ficha"
    CA.w.invoke()                       # un clic del usuario en la casilla
    comprobar(BD.modificado and BD.registro["nombre"] == "Editado desde la ficha", "enlazado: escribir marca el registro como modificado")
    comprobar(BD.guardar(), "conector: guardar()")
    v = B.conectar("sqlite:" + RUTA)
    r = v.consultar("contactos", {"id": 1})[0]
    v.cerrar()
    comprobar(r["nombre"] == "Editado desde la ficha" and r["activo"] == 1, "conector: lo guardado esta en la base (%r)" % (r,))
    comprobar(Rej._g.filas[0][1] == "Editado desde la ficha", "rejilla: se actualiza tras guardar")
    # editar una celda de la rejilla
    g = Rej._g
    g.seleccionar(2, 1)
    comprobar(g.editar(), "rejilla: editar() abre el editor de la celda")
    g.texto_edicion("Celda editada")
    g.confirmar_edicion()
    v = B.conectar("sqlite:" + RUTA)
    r = v.consultar("contactos", {"id": 3})[0]
    v.cerrar()
    comprobar(r["nombre"] == "Celda editada", "rejilla: editar una celda escribe en la base")
    # una celda con un numero: se convierte por el tipo de la columna
    g.seleccionar(2, 3)
    g.editar()
    g.texto_edicion("44")
    g.confirmar_edicion()
    v = B.conectar("sqlite:" + RUTA)
    r = v.consultar("contactos", {"id": 3})[0]
    v.cerrar()
    comprobar(r["edad"] == 44 and isinstance(r["edad"], int), "rejilla: el texto de una celda INTEGER vuelve a ser entero")
    # la clave no se edita
    g.seleccionar(2, 0)
    comprobar(g.editar() is False, "rejilla: la columna clave no se edita")
    # orden por columna
    Rej._al_ordenar(1)
    comprobar(BD._p["orden"] == "nombre", "rejilla: clic en una cabecera ordena")
    Rej._al_ordenar(1)
    comprobar(BD._p["orden"] == "-nombre" and BD.filas[0]["nombre"] == "Persona 119", "rejilla: segundo clic invierte el orden")
    BD.orden = ""
    # filtro
    BD.filtro = "ciudad = 'Lima'"
    comprobar(BD.total == 30 and all(x["ciudad"] == "Lima" for x in BD.filas),
              "conector: el filtro reduce el total (total=%d, ciudades=%s)" % (BD.total, sorted(set(x["ciudad"] for x in BD.filas))))
    BD.filtro = ""
    comprobar(BD.total == 120, "conector: quitar el filtro")
    # nuevo, guardar, borrar
    BD.nuevo()
    comprobar(BD.es_nuevo and TN.texto == "", "nuevo: los controles se vacian")
    TN.texto = "Registro nuevo"
    CC.texto = "Lima"
    comprobar(BD.guardar() and BD.total == 121, "nuevo: guardar() inserta")
    v = B.conectar("sqlite:" + RUTA)
    n = v.contar("contactos", {"nombre": "Registro nuevo", "ciudad": "Lima"})
    v.cerrar()
    comprobar(n == 1 and BD.posicion == 120 and TN.texto == "Registro nuevo", "nuevo: esta en la base y es el registro actual")
    comprobar(BD.borrar() and BD.total == 120, "borrar: quita el registro actual")
    # una edicion no guardada se guarda al moverse
    BD.primero()
    TN.texto = "Guardado al moverse"
    BD.siguiente()
    v = B.conectar("sqlite:" + RUTA)
    r = v.consultar("contactos", {"id": 1})[0]
    v.cerrar()
    comprobar(r["nombre"] == "Guardado al moverse", "navegar: moverse guarda lo pendiente")
    # errores
    BD.nuevo()
    TN.texto = ""
    comprobar(BD.guardar() is True and not BD.es_nuevo, "nuevo vacio: guardar() lo descarta sin error")
    BD.nuevo()
    BD.poner("ciudad", "Lima")
    sucesos[:] = []
    comprobar(BD.guardar() is False and "error" in sucesos and "NOT NULL" in BD.error.upper(), "error: un NOT NULL incumplido dispara error")
    BD.recargar()
    BD.origen = "sqlite:/no/existe/x.db"
    sucesos[:] = []
    comprobar(BD.abrir() is False and sucesos == ["error"] and BD.error != "", "error: un origen que no abre dispara error")
    BD.origen = "sqlite:" + RUTA
    comprobar(BD.abrir(), "conector: se vuelve a abrir")
    # el diseñador conoce los controles nuevos
    nombres = [m[0] for m in AC.miembros("ConectorBD")]
    comprobar("abrir" in nombres and "guardar" in nombres and "origen" in nombres and "tabla" in nombres, "autocompletar: ConectorBD ofrece sus metodos y propiedades")
    comprobar("seleccion" in [m[0] for m in AC.miembros("RejillaDatos")], "autocompletar: RejillaDatos")
    comprobar(M.defectos("RejillaDatos")["editable"] is False and "campo" in M.defectos("CuadroTexto"), "modelo: propiedades por defecto")
    BD.cerrar()
    vps.salir()
    # el diseñador: tablas y columnas del origen, y el modelo del formulario
    from vps import _datos, proyecto
    comprobar(_datos.tablas_de("sqlite:" + RUTA) == ["contactos"], "diseñador: tablas_de(origen) lista las tablas")
    comprobar(_datos.columnas_de("sqlite:" + RUTA, "contactos") == ["id", "nombre", "ciudad", "edad", "activo"], "diseñador: columnas_de(origen, tabla)")
    comprobar(_datos.tablas_de("sqlite:/no/existe/x.db") == [], "diseñador: un origen que no abre da una lista vacía")
    forms = [{"nombre": "Formulario", "props": {}, "controles": []}]
    cb = M.crear(forms, forms[0], "ConectorBD", 20, 20)
    cb["props"].update({"origen": "sqlite:" + RUTA, "tabla": "contactos"})
    ct = M.crear(forms, forms[0], "CuadroTexto", 20, 80)
    ct["props"].update({"fuente": cb["nombre"], "campo": "nombre"})
    forms2 = proyecto.leer_vpf(proyecto.escribir_vpf(forms))
    p1 = forms2[0]["controles"][0]["props"]
    p2 = forms2[0]["controles"][1]["props"]
    comprobar(p1["tabla"] == "contactos" and p1["origen"] == "sqlite:" + RUTA and p2["fuente"] == "ConectorBD1" and p2["campo"] == "nombre"
              and proyecto.leer_vpf(proyecto.escribir_vpf(forms2)) == forms2, "diseñador: el .vpf guarda y lee origen, tabla, fuente y campo")
    # el proyecto de ejemplo
    try:
        os.remove("/tmp/agenda.db")
        os.remove("/home/agenda.db")
    except OSError:
        pass
    ns = proyecto.ejecutar_proyecto("/ejemplos/vps/agenda-bd", solo_construir=True)
    ns["Formulario_cargar"]()
    B1 = ns["ConectorBD1"]
    comprobar(B1.abierta and B1.total == 4 and ns["CuadroTexto1"].texto == "Ana Perez", "agenda-bd: abre y rellena la ficha")
    ns["RejillaDatos1"]._g.seleccionar(1)
    comprobar(ns["CuadroTexto1"].texto == "Beto Lopez" and ns["Combo1"].texto == "Sevilla", "agenda-bd: elegir una fila de la rejilla rellena la ficha")
    B1.cerrar()
    vps.salir()


def principal():
    preparar()
    biblioteca()
    if "--lmk" in sys.argv:
        litemonkey(int(sys.argv[sys.argv.index("--lmk") + 1]))
    componentes()
    print("bd-autoprueba: fin")


principal()
