# SPDX-License-Identifier: GPL-2.0-or-later
# vps.proyecto - proyectos de VisualPythonS (mejora 123, D297).
#
# Un proyecto es una carpeta con:
#   formulario.vpf   los formularios y sus controles, en texto legible
#   codigo.py        el código de los eventos (Boton1_click, Formulario_cargar...)
#
# Formato del .vpf (una propiedad por línea, el valor es un literal de Python):
#
#   # VisualPythonS 1
#   formulario Formulario
#     texto = "Calculadora"
#     ancho = 240
#   control Boton Boton1
#     texto = "Sumar"
#     x = 10
#   control Etiqueta Etiqueta1 en Marco1
#     texto = "Resultado"
#
# Solo se guardan las propiedades que no valen lo de por defecto. Varios `formulario`
# en el mismo archivo son varios formularios; el primero es el principal.
import sys
import vps


def _literal(v):
    """Lee el valor de una propiedad: un literal de Python (número, texto, True, lista...)."""
    return eval(v, {})


def leer_vpf(texto):
    """-> [{"nombre", "props", "controles": [{"tipo", "nombre", "padre", "props"}]}]"""
    formularios = []
    actual = None
    destino = None
    n = 0
    for linea in texto.split("\n"):
        n += 1
        t = linea.strip()
        if not t or t[0] == "#":
            continue
        if linea[0] not in " \t":
            partes = t.split()
            if partes[0] == "formulario" and len(partes) == 2:
                actual = {"nombre": partes[1], "props": {}, "controles": []}
                formularios.append(actual)
                destino = actual["props"]
            elif partes[0] == "control" and len(partes) in (3, 5) and actual is not None:
                c = {"tipo": partes[1], "nombre": partes[2], "padre": partes[4] if len(partes) == 5 else None,
                     "props": {}}
                if c["tipo"] not in vps.TIPOS:
                    raise ValueError("línea %d: control desconocido «%s»" % (n, partes[1]))
                actual["controles"].append(c)
                destino = c["props"]
            else:
                raise ValueError("línea %d: no se entiende «%s»" % (n, t))
        else:
            if destino is None or "=" not in t:
                raise ValueError("línea %d: propiedad fuera de un control" % n)
            k, _, v = t.partition("=")
            try:
                destino[k.strip()] = _literal(v.strip())
            except Exception:
                raise ValueError("línea %d: valor no válido «%s»" % (n, v.strip()))
    return formularios


def _defectos(clase):
    return dict((n, d) for n, t, d in clase.todas())


def escribir_vpf(formularios):
    """El texto del .vpf para una lista de formularios (el mismo modelo de leer_vpf)."""
    s = ["# VisualPythonS 1"]
    for f in formularios:
        s.append("formulario %s" % f["nombre"])
        defect = dict((n, d) for n, t, d in vps.Formulario.PROPS)
        for n, t, d in vps.Formulario.PROPS:
            if n in f["props"] and f["props"][n] != defect[n]:
                s.append("  %s = %r" % (n, f["props"][n]))
        for c in f["controles"]:
            cls = vps.TIPOS[c["tipo"]]
            s.append("control %s %s%s" % (c["tipo"], c["nombre"], " en " + c["padre"] if c["padre"] else ""))
            defect = _defectos(cls)
            defect["ancho"], defect["alto"] = cls.TAM
            for n, t, d in cls.todas():
                if n in c["props"] and c["props"][n] != defect[n]:
                    s.append("  %s = %r" % (n, c["props"][n]))
    return "\n".join(s) + "\n"


def leer_texto(ruta):
    f = open(ruta)
    try:
        return f.read()
    finally:
        f.close()


def escribir_texto(ruta, texto):
    f = open(ruta, "w")
    try:
        f.write(texto)
    finally:
        f.close()


def construir(formularios, ns):
    """Crea los formularios y sus controles reales y mete sus nombres en `ns`.
    Los formularios secundarios quedan ocultos hasta Nombre.mostrar()."""
    primero = True
    for m in formularios:
        f = vps.Formulario(nombre=m["nombre"], **m["props"])
        ns[m["nombre"]] = f
        if not primero:
            f.ocultar()
        primero = False
        for c in m["controles"]:
            padre = f
            if c["padre"]:
                padre = f.controles[c["padre"]]
            obj = vps.TIPOS[c["tipo"]](padre, nombre=c["nombre"], **c["props"])
            ns[c["nombre"]] = obj
    return [ns[m["nombre"]] for m in formularios]


def espacio_nombres():
    """El espacio de nombres del código del usuario: todo `from vps import *`."""
    ns = {"__name__": "__vps__"}
    for k in vps.__all__:
        ns[k] = getattr(vps, k)
    return ns


def cargar_codigo(carpeta, ns, nombre="codigo.py"):
    """Ejecuta el codigo.py del proyecto en `ns`. Un error de sintaxis o de ejecución
    sube con su línea (el traceback dice «codigo.py», line N)."""
    ruta = carpeta + "/" + nombre
    try:
        fuente = leer_texto(ruta)
    except OSError:
        return
    exec(compile(fuente, nombre, "exec"), ns)


def ejecutar_proyecto(carpeta, ns=None, solo_construir=False):
    """Lee la carpeta del proyecto, crea la aplicación y (salvo solo_construir) entra en su
    bucle de eventos. Devuelve el espacio de nombres."""
    ns = ns if ns is not None else espacio_nombres()
    forms = leer_vpf(leer_texto(carpeta + "/formulario.vpf"))
    construir(forms, ns)
    cargar_codigo(carpeta, ns)
    todos = [ns[m["nombre"]] for m in forms]
    for f in todos:
        f._ns = ns
    if not solo_construir:
        todos[0].mostrar()
        import tkinter
        tkinter.mainloop()
    return ns


def lanzar(carpeta):
    """El punto de entrada de las aplicaciones generadas (y de `python -m vps carpeta`):
    errores al arrancar por pantalla, nunca en silencio."""
    try:
        ejecutar_proyecto(carpeta)
    except Exception as e:
        sys.stderr.write("VisualPythonS: error en %s\n" % carpeta)
        sys.print_exception(e)
        try:
            from tkinter import messagebox
            messagebox.showerror("Error", "%s: %s" % (type(e).__name__, e))
        except Exception:
            pass
        sys.exit(1)
