# SPDX-License-Identifier: GPL-2.0-or-later
# vps.autoprueba_toast - comprobaciones de las notificaciones (toast), el Calendario ampliado y el
# Arbol de VisualPythonS (v8.5.6, D466), sin ratón: el ratón se simula con event_generate. Escribe
# «vps-autoprueba: ok ...» o «FALLO ...». Lo usa tests/h466_vps_toast.py.
#
#     python /lib/python/vps/autoprueba_toast.py
import vps
from vps import proyecto, _modelo as M, _notificar as N, _fechas as FE


def comprobar(cond, msg):
    print("vps-autoprueba: " + ("ok " if cond else "FALLO ") + msg)


def pos(t):
    i = t.w.place_info()
    return int(i["x"]), int(i["y"])


def pulsar(w, x, y, doble=False):
    w.event_generate("<Double-Button-1>" if doble else "<ButtonPress-1>", x=x, y=y)


def toast():
    f = vps.Formulario("Toast", 420, 300, nombre="FormT")
    comprobar(all(n in vps.__all__ for n in ("notificar", "notificaciones", "cerrar_notificaciones", "Notificacion", "Arbol")),
              "toast: nombres en vps.__all__")
    # --- los cuatro tipos y el apilado (duracion 0: no se van solos)
    ts = []
    for tipo in ("info", "exito", "aviso", "error"):
        t = vps.notificar("Mensaje de tipo " + tipo, tipo, 0, formulario=f)
        ts.append(t)
        comprobar(t is not None and t.abierta and t.tipo == tipo and t.w.tipo == tipo, "toast: tipo %s" % tipo)
    comprobar(len(vps.notificaciones(f)) == 4, "toast: cuatro a la vez")
    ys = [pos(t)[1] for t in ts]
    comprobar(ys == sorted(ys) and len(set(ys)) == 4, "toast: se apilan (la más nueva junto al borde) %r" % (ys,))
    x, y = pos(ts[3])
    comprobar(x + ts[3].ancho <= 420 and y + ts[3].alto <= 300 and x + ts[3].ancho >= 420 - N.MARGEN - 1 and y + ts[3].alto >= 300 - N.MARGEN - 1, "toast: abajo a la derecha %r" % ((x, y, ts[3].ancho, ts[3].alto),))
    comprobar(set(N.COLORES.keys()) == set(N.TIPOS_TOAST) and len(set(N.COLORES.values())) == 4, "toast: un color por tipo")
    # --- cerrar por la API: las demás se recolocan
    ts[3].cerrar()
    comprobar(not ts[3].abierta and ts[3].motivo == "api" and len(vps.notificaciones(f)) == 3, "toast: cerrar() la quita")
    comprobar(pos(ts[2])[1] == ys[3] and pos(ts[0])[1] > ys[0], "toast: al cerrar la más nueva, las anteriores bajan hasta el borde")
    # --- cerrar con un clic (dispara al_click y al_cerrar) y con la x
    visto = []
    ts[1].al_click = lambda: visto.append("click")
    ts[1].al_cerrar = lambda: visto.append("cerrado")
    ts[1].w._wl, ts[1].w._wa = ts[1].ancho, ts[1].alto
    pulsar(ts[1].w, 30, 20)
    comprobar(not ts[1].abierta and ts[1].motivo == "click" and visto == ["click", "cerrado"], "toast: un clic la cierra %r" % (visto,))
    ts[2].w._wl, ts[2].w._wa = ts[2].ancho, ts[2].alto
    pulsar(ts[2].w, ts[2].ancho - 10, 8)
    comprobar(not ts[2].abierta and ts[2].motivo == "boton", "toast: la x la cierra")
    vps.cerrar_notificaciones(f)
    comprobar(vps.notificaciones(f) == [], "toast: cerrar_notificaciones")
    # --- se va sola; con 0 se queda
    corta = vps.notificar("Rápido", "info", 120, formulario=f)
    larga = vps.notificar("Se queda", "aviso", 0, formulario=f)
    vps.pausa(120)
    vps.pausa(250)
    comprobar(not corta.abierta and corta.motivo == "tiempo" and larga.abierta, "toast: desaparece sola tras la duración y con 0 se queda")
    larga.cerrar()
    # --- botón de acción
    hecho = []
    a = vps.notificar("Nota borrada", "exito", 0, "Borrado", accion="Deshacer", formulario=f, al_accion=lambda: hecho.append("deshacer"))
    a.w._wl, a.w._wa = a.ancho, a.alto
    r = a.w._rect_accion()
    comprobar(r is not None and a.alto >= 70, "toast: con título y acción es más alto (%d)" % a.alto)
    pulsar(a.w, r[0] + 4, r[1] + 4)
    comprobar(hecho == ["deshacer"] and not a.abierta and a.motivo == "accion", "toast: el botón de acción llama a su función")
    # --- las cuatro esquinas
    esq = {}
    for p in N.POSICIONES:
        esq[p] = vps.notificar("Esquina", "info", 0, posicion=p, formulario=f)
    xs = dict((p, pos(t)) for p, t in esq.items())
    comprobar(xs["abajo_derecha"][1] > 150 and xs["arriba_derecha"][1] < 50 and xs["abajo_derecha"][0] == xs["arriba_derecha"][0],
              "toast: abajo y arriba a la derecha %r" % (xs,))
    comprobar(abs(xs["abajo_centro"][0] + esq["abajo_centro"].ancho // 2 - 210) <= 1 and xs["arriba_centro"][1] < 50
              and xs["abajo_centro"][1] > 150, "toast: abajo y arriba en el centro %r" % (xs,))
    vps.cerrar_notificaciones(f)
    # --- texto largo: se parte en líneas; errores
    t = vps.notificar("Un texto muy largo que no cabe en una sola línea de la notificación, así que se parte", "info", 0, formulario=f)
    comprobar(len(t.w.lineas) > 1 and t.alto >= 14 + 16 * len(t.w.lineas), "toast: un texto largo se parte en líneas (%d)" % len(t.w.lineas))
    t.cerrar()
    try:
        vps.notificar("x", "otro", formulario=f)
        comprobar(False, "toast: un tipo desconocido da error")
    except ValueError:
        comprobar(True, "toast: un tipo desconocido da error")
    comprobar(not t.w._foca_no_clique and t.w._op.get("takefocus") == 0, "toast: no roba el foco")
    # --- el control Notificacion
    n = vps.Notificacion(f, "Hola", "exito", 0, nombre="Not1", titulo="Ey", accion="Vale", posicion="arriba_centro")
    comprobar(n.tipo == "exito" and n.texto == "Hola" and n.duracion == 0 and n.posicion == "arriba_centro", "Notificacion: propiedades")
    ev = []
    n.conectar("click", lambda: ev.append("click"))
    n.conectar("accion", lambda: ev.append("accion"))
    n.conectar("cerrado", lambda: ev.append("cerrado:" + n.motivo))
    t = n.mostrar()
    comprobar(t is not None and t.abierta and t.tipo == "exito" and t.titulo == "Ey" and t.accion == "Vale" and t.posicion == "arriba_centro",
              "Notificacion: mostrar() enseña el aviso con sus propiedades")
    t.w._wl, t.w._wa = t.ancho, t.alto
    pulsar(t.w, 20, 12)
    comprobar(ev == ["click", "cerrado:click"], "Notificacion: eventos click y cerrado %r" % (ev,))
    del ev[:]
    t = n.mostrar("Otro texto")
    comprobar(t.texto == "Otro texto" and n.texto == "Otro texto", "Notificacion: mostrar(texto)")
    t.w._wl, t.w._wa = t.ancho, t.alto
    r = t.w._rect_accion()
    pulsar(t.w, r[0] + 3, r[1] + 3)
    comprobar(ev == ["accion", "cerrado:accion"], "Notificacion: evento accion %r" % (ev,))
    del ev[:]
    n.tipo = "error"
    n.mostrar().cerrar()
    comprobar(ev == ["cerrado:api"] and n.ultima.tipo == "error", "Notificacion: tipo cambiado por código y cerrar()")
    n.destruir()
    comprobar("Not1" not in f.controles, "Notificacion: destruir")
    # --- registrada en el diseñador
    from vps import _superficie as SU, _autocompletar as AC
    comprobar(vps.TIPOS["Notificacion"] is vps.Notificacion and "Notificacion" in M.SIN_VENTANA and "Notificacion" in SU.HERRAMIENTAS,
              "Notificacion: en TIPOS, sin ventana y en la caja de herramientas")
    nom = [m[0] for m in AC.miembros("Notificacion")]
    comprobar("mostrar" in nom and "duracion" in nom and "posicion" in nom and "conectar(\"accion\", " in nom and "conectar(\"cerrado\", " in nom,
              "Notificacion: autocompletado %r" % (nom,))
    comprobar("notificar" in [x[0] for x in AC.FUNCIONES], "toast: notificar en el autocompletado de vps.")
    d = M.defectos("Notificacion")
    comprobar(d["tipo"] == "info" and d["duracion"] == 3000 and d["posicion"] == "abajo_derecha", "Notificacion: valores por defecto")
    vps.salir()


def calendario():
    f = vps.Formulario("Cal", 300, 260, nombre="FormC")
    cal = vps.Calendario(f, 8, 8, 196, 170, nombre="Cal1", valor="2026-11-15", minimo="2026-11-05", maximo="2026-12-20")
    cal.w._wl, cal.w._wa = 196, 170
    cw, ch = cal.w._geom()
    cam, dob = [], []
    cal.conectar("cambio", lambda: cam.append(cal.valor))
    cal.conectar("doble_click", lambda: dob.append(cal.valor))

    def celda(d, a=2026, m=11):
        fila, col = [(fi, co) for fi, co, di in FE.celdas(a, m) if di == d][0]
        return 2 + col * cw + cw // 2, 2 + FE.ALTO_CAB + FE.ALTO_SEM + fila * ch + ch // 2
    comprobar(FE.sumar_dias((2026, 12, 31), 1) == (2027, 1, 1) and FE.sumar_dias((2026, 3, 1), -1) == (2026, 2, 28), "calendario: sumar_dias")
    # navegación de mes y de año con las flechas de la cabecera
    pulsar(cal.w, 196 - 8, 8)
    comprobar(cal.w.visible == (2026, 12) and cal.valor == "2026-11-15", "calendario: > mes siguiente")
    pulsar(cal.w, 8, 8)
    pulsar(cal.w, 8, 8)
    comprobar(cal.w.visible == (2026, 10), "calendario: < mes anterior")
    pulsar(cal.w, 196 - 30, 8)
    comprobar(cal.w.visible == (2027, 10), "calendario: >> año siguiente")
    pulsar(cal.w, 30, 8)
    pulsar(cal.w, 30, 8)
    comprobar(cal.w.visible == (2025, 10), "calendario: << año anterior")
    cal.ir_a(2026, 11)
    comprobar(cal.mes_visible() == (2026, 11), "calendario: ir_a(anio, mes)")
    # elegir un día, dentro y fuera del rango
    x, y = celda(20)
    pulsar(cal.w, x, y)
    comprobar(cal.valor == "2026-11-20" and cam == ["2026-11-20"], "calendario: elegir un día %r" % (cal.valor,))
    x, y = celda(2)
    pulsar(cal.w, x, y)
    comprobar(cal.valor == "2026-11-20" and cam == ["2026-11-20"], "calendario: un día antes de `minimo` no se elige")
    comprobar(cal.en_rango("2026-11-05") and not cal.en_rango("2026-11-04") and not cal.en_rango("2026-12-21"), "calendario: en_rango()")
    cal.valor = "2030-01-01"
    comprobar(cal.valor == "2026-12-20", "calendario: un valor de fuera del rango se lleva al límite %r" % (cal.valor,))
    cal.valor = "2026-11-15"
    # doble clic
    x, y = celda(18)
    pulsar(cal.w, x, y, True)
    comprobar(dob == ["2026-11-18"] and cal.valor == "2026-11-18", "calendario: doble_click en un día %r" % (dob,))
    pulsar(cal.w, 60, 8, True)
    comprobar(dob == ["2026-11-18"], "calendario: un doble clic en la cabecera no es doble_click")
    # teclado
    cal.w.event_generate("<KeyPress-Right>")
    cal.w.event_generate("<KeyPress-Down>")
    comprobar(cal.valor == "2026-11-26", "calendario: flechas del teclado %r" % (cal.valor,))
    cal.w.event_generate("<KeyPress-Next>")
    comprobar(cal.w.visible == (2026, 12), "calendario: Av Pág cambia de mes")
    cal.fecha = "2026-11-30"
    comprobar(cal.dia == 30 and cal.anio == 2026 and cal.mes == 11, "calendario: fecha, anio, mes y dia")
    cal.minimo = ""
    cal.maximo = ""
    cal.valor = "2040-02-29"
    comprobar(cal.valor == "2040-02-29", "calendario: sin límites")
    # en el diseñador
    from vps import _autocompletar as AC
    nom = [m[0] for m in AC.miembros("Calendario")]
    comprobar("minimo" in nom and "maximo" in nom and "ir_a" in nom and "conectar(\"doble_click\", " in nom, "calendario: autocompletado")
    vps.salir()


def arbol():
    f = vps.Formulario("Arbol", 300, 260, nombre="FormA")
    ar = vps.Arbol(f, 8, 8, 200, 140, nombre="Ar1")
    ar.w._wl, ar.w._wa = 200, 140
    vistos = []
    for e in ("seleccion", "doble_click", "expandir", "contraer", "click"):
        ar.conectar(e, lambda e=e: vistos.append(e + ":" + str(ar.clave) + ":" + ar.nodo))
    ar.agregar(None, "Trabajo", "t")
    ar.agregar("t", "Reuniones", "r")
    ar.agregar("t", "Informes", "i")
    k = ar.agregar(None, "Personal")
    ar.agregar(k, "Salud", "s")
    comprobar(ar.claves() == ["t", "r", "i", "Personal", "s"] and ar.contar() == 5, "arbol: agregar %r" % (ar.claves(),))
    comprobar(ar.hijos("t") == ["r", "i"] and ar.hijos() == ["t", "Personal"] and ar.padre_de("s") == "Personal" and ar.texto_de("r") == "Reuniones",
              "arbol: hijos, padre_de y texto_de")
    comprobar(len(ar.w.visibles()) == 5, "arbol: empieza expandido")
    try:
        ar.agregar("no", "x")
        comprobar(False, "arbol: un padre que no existe da error")
    except ValueError:
        comprobar(True, "arbol: un padre que no existe da error")
    ar.contraer("t")
    comprobar([n["clave"] for n in ar.w.visibles()] == ["t", "Personal", "s"], "arbol: contraer un nodo esconde sus hijos")
    ar.expandir()
    # clic en el cuadrito de un nodo: contrae y dispara contraer; otro clic lo expande
    pulsar(ar.w, 2 + 4 + 5, 2 + 9)
    comprobar(vistos == ["contraer:None:t"] and [n["clave"] for n in ar.w.visibles()] == ["t", "Personal", "s"], "arbol: clic en el cuadrito contrae %r" % (vistos,))
    pulsar(ar.w, 2 + 4 + 5, 2 + 9)
    comprobar(vistos[-1] == "expandir:None:t" and len(ar.w.visibles()) == 5, "arbol: otro clic expande y dispara expandir %r" % (vistos,))
    del vistos[:]
    # clic en el texto de «Informes» (fila 2, nivel 1)
    pulsar(ar.w, 2 + 4 + 16 + 16 + 10, 2 + 2 * 18 + 9)
    comprobar(ar.clave == "i" and ar.valor == "Informes" and ar.seleccion == 2 and vistos[0].startswith("seleccion:i"),
              "arbol: clic en un nodo lo elige y dispara seleccion %r" % (vistos,))
    comprobar(ar.ruta() == "Trabajo/Informes", "arbol: ruta() %r" % (ar.ruta(),))
    del vistos[:]
    pulsar(ar.w, 2 + 4 + 16 + 16 + 10, 2 + 2 * 18 + 9, True)
    comprobar(vistos == ["doble_click:i:" + ar.nodo], "arbol: doble_click %r" % (vistos,))
    # teclado: Abajo, Izquierda (sube al padre), Izquierda (contrae), Derecha (expande)
    del vistos[:]
    ar.w.event_generate("<KeyPress-Down>")
    comprobar(ar.clave == "Personal", "arbol: la flecha abajo elige el siguiente")
    ar.w.event_generate("<KeyPress-Left>")
    comprobar(ar.clave == "Personal" and not ar.esta_expandido("Personal") and vistos[-1].startswith("contraer"), "arbol: izquierda contrae")
    ar.w.event_generate("<KeyPress-Right>")
    comprobar(ar.esta_expandido("Personal") and vistos[-1].startswith("expandir"), "arbol: derecha expande")
    ar.w.event_generate("<KeyPress-Up>")
    ar.w.event_generate("<KeyPress-Up>")
    ar.w.event_generate("<KeyPress-Left>")
    comprobar(ar.clave == "t", "arbol: izquierda en un hijo sube al padre %r" % (ar.clave,))
    # elegir por código no dispara eventos; seleccion como índice
    del vistos[:]
    ar.elegir("s")
    comprobar(ar.clave == "s" and ar.seleccion == 4 and vistos == [], "arbol: elegir() sin eventos")
    ar.seleccion = 1
    comprobar(ar.clave == "r", "arbol: seleccion por índice")
    # quitar
    ar.quitar("t")
    comprobar(ar.claves() == ["Personal", "s"] and ar.clave is None and ar.seleccion == -1, "arbol: quitar borra el nodo y sus hijos y la selección")
    ar.limpiar()
    comprobar(ar.contar() == 0, "arbol: limpiar")
    # la propiedad nodos (la del diseñador)
    ar2 = vps.Arbol(f, 8, 160, 200, 80, nombre="Ar2", nodos=["Uno", "  Dos|d", "    Tres", "Cuatro"], expandido=False, seleccion=1)
    comprobar(ar2.claves() == ["Uno", "d", "Tres", "Cuatro"] and ar2.padre_de("Tres") == "d" and ar2.hijos("Uno") == ["d"], "arbol: la propiedad nodos %r" % (ar2.claves(),))
    comprobar(len(ar2.w.visibles()) == 2 and ar2.clave == "d", "arbol: expandido=False y seleccion=1 %r" % (ar2.clave,))
    comprobar(ar2.nodos == ["Uno", "  Dos|d", "    Tres", "Cuatro"], "arbol: nodos se lee como se escribió %r" % (ar2.nodos,))
    from vps import _autocompletar as AC, _superficie as SU
    nom = [m[0] for m in AC.miembros("Arbol")]
    comprobar("agregar" in nom and "expandir" in nom and "clave" in nom and "conectar(\"expandir\", " in nom and "nodos" in nom,
              "arbol: autocompletado")
    comprobar("Arbol" in SU.HERRAMIENTAS and "Arbol" not in M.SIN_VENTANA, "arbol: en la caja de herramientas")
    vps.salir()


def disenador():
    """Notificacion, Calendario y Arbol desde el diseñador: colocar, guardar, generar y ejecutar."""
    from vps import disenador as DS, plantillas
    app = DS.App()
    app.confirmar_auto = True
    app.nuevo(plantillas.vacia)
    app.crear_control("Notificacion", 250, 8)
    app.crear_control("Arbol", 8, 8, 170, 120)
    app.crear_control("Calendario", 186, 8)
    app.crear_control("Boton", 8, 140)
    comprobar([c["tipo"] for c in app.form["controles"]] == ["Notificacion", "Arbol", "Calendario", "Boton"], "diseñador: se colocan los tres controles nuevos")
    nt = M.por_nombre(app.form, "Notificacion1")
    for c, n, v in (("Notificacion1", "texto", "Guardado"), ("Notificacion1", "tipo", "exito"), ("Notificacion1", "duracion", 0),
                    ("Notificacion1", "accion", "Deshacer"), ("Arbol1", "nodos", ["Casa", "  Cocina", "  Salón", "Oficina"]),
                    ("Calendario1", "minimo", "2026-01-01"), ("Boton1", "texto", "Avisar")):
        app.seleccionar([M.por_nombre(app.form, c)])
        app.aplicar(n, v)
    comprobar(M.val(nt, "tipo") == "exito" and M.val(M.por_nombre(app.form, "Arbol1"), "nodos")[1] == "  Cocina", "diseñador: las propiedades nuevas se editan")
    app.seleccionar([M.por_nombre(app.form, "Arbol1")])
    comprobar("nodos" in [f[0] for f in app.props.filas] and "expandido" in [f[0] for f in app.props.filas], "diseñador: Arbol1 muestra sus propiedades")
    app.seleccionar([nt])
    fil = [f[0] for f in app.props.filas]
    comprobar("posicion" in fil and "duracion" in fil and "accion" in fil and "tipo" in fil, "diseñador: Notificacion1 muestra sus propiedades %r" % (fil[:8],))
    # el explorador lo lista
    if not app.expl_visible:
        app.alternar_explorador()
    app.refrescar_explorador()
    filas = " ".join(app.arbol.texto_filas())
    comprobar("Notificacion1" in filas and "Arbol1" in filas and "Calendario1" in filas, "diseñador: el explorador del proyecto los lista")
    app.editor.poner("def Boton1_click():\n    Notificacion1.mostrar()\n    Arbol1.agregar(None, 'Garaje', 'g')\n\n\ndef Notificacion1_accion():\n    Boton1.texto = 'Deshecho'\n")
    # guardar y volver a cargar
    carpeta = "/tmp/vps-toast-proy"
    comprobar(app._escribir(carpeta) is True, "diseñador: guardar")
    t = proyecto.leer_texto(carpeta + "/formulario.vpf")
    comprobar("control Notificacion Notificacion1" in t and "control Arbol Arbol1" in t and "control Calendario Calendario1" in t
              and "accion = 'Deshacer'" in t and "minimo = '2026-01-01'" in t and "Cocina" in t, "diseñador: el .vpf lleva los controles y sus propiedades")
    forms = proyecto.leer_vpf(t)
    comprobar(proyecto.leer_vpf(proyecto.escribir_vpf(forms)) == forms and len(forms[0]["controles"]) == 4, "diseñador: .vpf leer y escribir dan lo mismo")
    app.cargar(carpeta)
    comprobar(M.por_nombre(app.form, "Arbol1") is not None and M.val(M.por_nombre(app.form, "Notificacion1"), "accion") == "Deshacer", "diseñador: al cargar vuelve todo")
    # ejecutar (F5): el botón enseña la notificación y mete un nodo
    app.ejecutar()
    f = vps._formularios[0]
    comprobar(f.Arbol1.contar() == 4 and f.Notificacion1.accion == "Deshacer", "diseñador: ejecutar construye los controles")
    f.Boton1.pulsar()
    n = vps.notificaciones(f)
    comprobar(len(n) == 1 and n[0].texto == "Guardado" and n[0].tipo == "exito" and f.Arbol1.existe("g"), "diseñador: ejecutar: el botón enseña la notificación y cambia el árbol")
    app.detener()
    # generar la aplicación
    orig_in, orig_ca, orig_av = vps.inputbox, DS.carpeta_apps, app.aviso
    vps.inputbox = lambda *a, **k: "prueba-toast"
    DS.carpeta_apps = lambda: "/tmp/vps-apps"
    app.aviso = lambda *a, **k: None
    try:
        app.generar()
    finally:
        vps.inputbox, DS.carpeta_apps, app.aviso = orig_in, orig_ca, orig_av
    ns = proyecto.ejecutar_proyecto("/tmp/vps-apps/prueba-toast", solo_construir=True)
    ns["Boton1"].pulsar()
    comprobar(ns["Arbol1"].existe("g") and len(vps.notificaciones(ns["Formulario"])) == 1 and ns["Notificacion1"].titulo == "",
              "diseñador: la aplicación generada se ejecuta con los controles nuevos")
    vps.salir()


def plantillas_nuevas():
    """Los dos ejemplos nuevos."""
    ns = proyecto.ejecutar_proyecto("/ejemplos/vps/notificaciones", solo_construir=True)
    f = ns["Formulario"]
    ns["Boton1"].pulsar()
    comprobar(len(vps.notificaciones(f)) == 1 and vps.notificaciones(f)[0].tipo == "exito" and vps.notificaciones(f)[0].accion == "Deshacer",
              "ejemplo notificaciones: guardar enseña el toast de éxito con «Deshacer»")
    ns["Boton2_click"]()
    comprobar(any(t.tipo == "error" and t.duracion == 0 for t in vps.notificaciones(f)), "ejemplo notificaciones: guardar mal enseña el toast de error")
    vps.cerrar_notificaciones(f)
    ns["Boton3"].pulsar()
    ns["Boton4"].pulsar()
    comprobar([t.tipo for t in vps.notificaciones(f)] == ["aviso", "info"], "ejemplo notificaciones: aviso e información se apilan")
    vps.salir()
    ns = proyecto.ejecutar_proyecto("/ejemplos/vps/agenda-carpetas", solo_construir=True)
    f = ns["Formulario"]
    ns["Formulario_cargar"]()
    ar, cal = ns["Arbol1"], ns["Calendario1"]
    comprobar(ar.clave == "Reuniones" and ns["Etiqueta1"].texto == "Trabajo/Reuniones" and ns["Etiqueta2"].texto == cal.valor, "ejemplo agenda: empieza en Trabajo/Reuniones")
    ns["CuadroTexto1"].texto = "Revisar el presupuesto"
    ns["Boton1"].pulsar()
    comprobar(len(ns["notas"]) == 1 and ns["BarraEstado1"].texto == "Notas guardadas: 1", "ejemplo agenda: guardar una nota")
    ar.elegir("Salud")
    ns["Arbol1_seleccion"]()
    comprobar(ns["CuadroTexto1"].texto == "" and ns["Etiqueta1"].texto == "Personal/Salud", "ejemplo agenda: otra carpeta, otra nota")
    ar.elegir("Reuniones")
    ns["Arbol1_seleccion"]()
    comprobar(ns["CuadroTexto1"].texto == "Revisar el presupuesto", "ejemplo agenda: la nota vuelve al elegir la carpeta")
    vps.salir()


def principal():
    toast()
    calendario()
    arbol()
    disenador()
    plantillas_nuevas()
    print("vps-autoprueba: fin")


if __name__ == "__main__":
    principal()
