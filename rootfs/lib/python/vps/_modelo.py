# SPDX-License-Identifier: GPL-2.0-or-later
# vps._modelo - el modelo de proyecto que edita el diseñador (mejora 123, D297).
#
# Un formulario es {"nombre", "props", "controles": [...]} y un control {"tipo", "nombre",
# "padre", "props"} (el mismo modelo de vps.proyecto.leer_vpf). Las coordenadas de un control
# son relativas a su contenedor (el formulario o un Marco), como en el programa que se ejecuta.
import vps

GRID = 8
_cache = {}
SIN_VENTANA = ("Temporizador", "Menu", "Reloj", "Archivo", "ConectorBD")    # componentes sin ventana: solo un icono
CONTENEDORES = ("Marco", "Pestanas")


def es_contenedor(c):
    return c["tipo"] in CONTENEDORES


def desplazamiento_hijos(c):
    """(dx, dy) del rincón de dentro de un contenedor respecto al suyo: el borde y, en el Marco, el
    título; en las Pestanas, la tira de pestañas."""
    if c["tipo"] == "Pestanas":
        return 2, 2 + vps.TAB_H
    return 2, 2 + (8 if val(c, "texto") else 0)


def en_pagina(form, c):
    """False si `c` está en una página de unas Pestanas que no es la elegida (o en un hijo de ella)."""
    while c["padre"]:
        p = por_nombre(form, c["padre"])
        if p is None:
            return True
        if p["tipo"] == "Pestanas" and int(val(c, "pagina") or 0) != int(val(p, "seleccion") or 0):
            return False
        c = p
    return True


def defectos(tipo):
    """{propiedad: valor por defecto} de un tipo de control (o de "Formulario")."""
    d = _cache.get(tipo)
    if d is None:
        if tipo == "Formulario":
            d = dict((n, v) for n, t, v in vps.Formulario.PROPS)
        else:
            cls = vps.TIPOS[tipo]
            d = dict((n, v) for n, t, v in cls.todas())
            d["ancho"], d["alto"] = cls.TAM
        _cache[tipo] = d
    return d


def val(c, n, tipo=None):
    """El valor de una propiedad del control (o formulario) `c`: el suyo o el de por defecto."""
    p = c["props"]
    if n in p:
        return p[n]
    return defectos(tipo or c.get("tipo", "Formulario")).get(n)


def props_de(tipo):
    """[(nombre, tipo de dato, defecto)] en el orden en que se enseñan en la ventana de propiedades."""
    if tipo == "Formulario":
        return list(vps.Formulario.PROPS)
    cls = vps.TIPOS[tipo]
    _comunes = ("x", "y", "ancho", "alto", "visible", "activo", "color", "colorfondo", "fuente",
                "anclaje", "orden_tab", "pagina")
    propias = [p for p in cls.todas() if p[0] not in _comunes]
    comunes = [p for p in cls.todas() if p[0] in _comunes]
    return propias + comunes


def copiar(c):
    return {"tipo": c["tipo"], "nombre": c["nombre"], "padre": c["padre"],
            "props": dict((k, list(v) if isinstance(v, list) else v) for k, v in c["props"].items())}


def todos_los_nombres(forms):
    r = set()
    for f in forms:
        r.add(f["nombre"])
        for c in f["controles"]:
            r.add(c["nombre"])
    return r


def nombre_nuevo(forms, tipo):
    usados = todos_los_nombres(forms)
    n = 1
    while "%s%d" % (tipo, n) in usados:
        n += 1
    return "%s%d" % (tipo, n)


def crear(forms, form, tipo, x, y, ancho=None, alto=None, padre=None):
    cls = vps.TIPOS[tipo]
    nombre = nombre_nuevo(forms, tipo)
    p = {"x": int(x), "y": int(y)}
    tam = cls.TAM
    p["ancho"] = int(ancho if ancho else tam[0])
    p["alto"] = int(alto if alto else tam[1])
    if "texto" in defectos(tipo) and tipo not in ("CuadroTexto", "Combo", "BarraEstado"):
        p["texto"] = nombre
    if padre:
        pa = por_nombre(form, padre)
        if pa is not None and pa["tipo"] == "Pestanas" and int(val(pa, "seleccion") or 0):
            p["pagina"] = int(val(pa, "seleccion"))      # el control nuevo va a la página que se ve
    c = {"tipo": tipo, "nombre": nombre, "padre": padre, "props": p}
    form["controles"].append(c)
    return c


def por_nombre(form, nombre):
    for c in form["controles"]:
        if c["nombre"] == nombre:
            return c
    return None


def descendientes(form, c):
    r = []
    for h in form["controles"]:
        if h["padre"] == c["nombre"]:
            r.append(h)
            r += descendientes(form, h)
    return r


def origen_hijos(form, c):
    """(x, y) del rincón de dentro de un Marco, en coordenadas del cliente del formulario."""
    x, y = _rect_local(form, c)[:2]
    dx, dy = desplazamiento_hijos(c)
    return x + dx, y + dy


def _rect_local(form, c):
    """(x, y, ancho, alto) del control en coordenadas del cliente del formulario."""
    x, y = int(val(c, "x")), int(val(c, "y"))
    pad = c["padre"]
    while pad:
        p = por_nombre(form, pad)
        if p is None:
            break
        dx, dy = desplazamiento_hijos(p)
        x += int(val(p, "x")) + dx
        y += int(val(p, "y")) + dy
        pad = p["padre"]
    return x, y, int(val(c, "ancho")), int(val(c, "alto"))


rect_abs = _rect_local


def borrar(form, c):
    quitar = [c] + descendientes(form, c)
    form["controles"] = [x for x in form["controles"] if not any(x is q for q in quitar)]


def renombrar_valido(forms, c, nuevo, es_form=False):
    """None si `nuevo` sirve de nombre para `c` (un control o, con es_form, un formulario); si no,
    el motivo."""
    if not nuevo or not (nuevo[0].isalpha() or nuevo[0] == "_"):
        return "el nombre tiene que empezar por una letra"
    for ch in nuevo:
        if not (ch.isalpha() or ch.isdigit() or ch == "_") or ord(ch) > 127:
            return "solo letras sin acento, cifras y _"
    if nuevo == "vps" or nuevo in vps.__all__ or (nuevo == "Formulario" and not es_form):
        return "ese nombre ya lo usa VisualPythonS"
    if nuevo in ("None", "True", "False", "def", "class", "if", "else", "for", "while", "import", "from", "return"):
        return "es una palabra reservada de Python"
    if nuevo != c["nombre"] and nuevo in todos_los_nombres(forms):
        return "ya hay otro control o formulario con ese nombre"
    return None


def mover_z(form, c, frente):
    """Trae `c` (con lo que contiene) al frente o lo envía atrás entre los controles de su mismo
    contenedor. Los controles se dibujan en el orden de la lista: el último queda encima."""
    bloque = [c] + descendientes(form, c)
    resto = [x for x in form["controles"] if not any(x is b for b in bloque)]
    if frente:
        resto += bloque
    else:
        pos = 0
        if c["padre"]:
            for i, x in enumerate(resto):
                if x["nombre"] == c["padre"]:
                    pos = i + 1
        resto[pos:pos] = bloque
    form["controles"] = resto
