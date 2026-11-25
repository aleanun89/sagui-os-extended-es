# SPDX-License-Identifier: GPL-2.0-or-later
# vps._autocompletar - autocompletado y ayuda breve del editor de código (mejora 124, D299).
#
# Tras `Boton1.` ofrece las propiedades, los métodos y los eventos del control; tras `vps.`, la
# API del módulo; tras el nombre de un formulario, sus controles. Con `(` abierto, la firma.
# Desde la v8.3.2 (D303) también ofrece las palabras clave y las funciones integradas de Python, los
# nombres del propio código (funciones, clases, variables, parámetros), plantillas (def, if, for...)
# y la firma de las funciones del usuario. Todo son funciones sin ventanas, para poder probarlas con
# la autoprueba.
import vps

METODOS = {
    "Control": [("enfocar", "enfocar()", "Da el foco del teclado al control"),
                ("conectar", "conectar(evento, funcion)", "Conecta una función a un evento: conectar(\"click\", f)"),
                ("destruir", "destruir()", "Quita el control del formulario")],
    "Boton": [("pulsar", "pulsar()", "Simula una pulsación del botón (dispara click)")],
    "Lista": [("agregar", "agregar(texto)", "Añade un elemento al final"),
              ("quitar", "quitar(indice)", "Quita el elemento de ese índice"),
              ("limpiar", "limpiar()", "Vacía la lista"),
              ("contar", "contar()", "Cuántos elementos hay")],
    "Combo": [("agregar", "agregar(texto)", "Añade un elemento al desplegable"),
              ("limpiar", "limpiar()", "Vacía el desplegable")],
    "CuadroTexto": [("seleccionar_todo", "seleccionar_todo()", "Selecciona todo el texto")],
    "Tabla": [("agregar", "agregar(fila)", "Añade una fila (una lista o un texto «a;b;c»)"),
              ("quitar", "quitar(indice)", "Quita una fila"), ("limpiar", "limpiar()", "Vacía la tabla"),
              ("contar", "contar()", "Cuántas filas hay"), ("celda", "celda(fila, columna)", "El valor de una celda"),
              ("poner_celda", "poner_celda(fila, columna, valor)", "Cambia una celda"),
              ("ordenar", "ordenar(columna, descendente=False)", "Ordena por una columna"),
              ("cargar_csv", "cargar_csv(ruta, separador=\";\", encabezado=True)", "Lee un CSV (la primera línea son las columnas)"),
              ("guardar_csv", "guardar_csv(ruta, separador=\";\")", "Escribe las columnas y filas en un CSV")],
    "Lienzo": [("linea", "linea(x1, y1, x2, y2, color=None, grosor=1)", "Dibuja una línea"),
               ("rectangulo", "rectangulo(x, y, ancho, alto, color=None, relleno=None, grosor=1)", "Dibuja un rectángulo"),
               ("circulo", "circulo(cx, cy, radio, color=None, relleno=None, grosor=1)", "Dibuja un círculo"),
               ("texto", "texto(x, y, texto, color=None)", "Escribe un texto"),
               ("imagen", "imagen(x, y, archivo)", "Pone una imagen (BMP, PNG o PPM)"),
               ("limpiar", "limpiar()", "Borra todo el dibujo")],
    "BarraEstado": [("poner", "poner(i, texto)", "Cambia el panel de la derecha número i")],
    "SelectorColor": [("elegir", "elegir()", "Abre la paleta, como al pulsar el control")],
    "Reloj": [("ahora", "ahora()", "(anio, mes, dia, hora, minuto, segundo, dia_semana)"),
              ("formatear", "formatear(plantilla)", "Sustituye %Y %m %d %H %M %S por la fecha y hora actuales")],
    "Archivo": [("existe", "existe()", "Si el archivo existe"), ("leer", "leer()", "Todo el texto del archivo"),
                ("escribir", "escribir(texto)", "Sustituye el contenido"), ("agregar", "agregar(texto)", "Añade al final"),
                ("leer_lineas", "leer_lineas()", "Las líneas del archivo"),
                ("escribir_lineas", "escribir_lineas(lineas)", "Escribe una línea por elemento"),
                ("leer_csv", "leer_csv()", "El CSV como lista de listas"),
                ("escribir_csv", "escribir_csv(filas)", "Escribe una lista de listas como CSV"),
                ("borrar", "borrar()", "Borra el archivo")],
    "Formulario": [("mostrar", "mostrar(modal=False, datos=None)",
                    "Abre el formulario; con modal=True espera a que se cierre y devuelve su resultado"),
                   ("ocultar", "ocultar()", "Esconde el formulario sin cerrarlo"),
                   ("cerrar", "cerrar(resultado=None)", "Cierra el formulario; si era modal, mostrar() devuelve el resultado"),
                   ("enfocar", "enfocar()", "Da el foco a la ventana"),
                   ("control", "control(nombre)", "El control con ese nombre")],
}

METODOS.update(vps._datos.METODOS)      # mejora 127: ConectorBD y RejillaDatos
METODOS.update(vps._selectores.METODOS)  # v8.4.3: selectores de archivos y carpetas
METODOS.update(vps._fechas.METODOS)
METODOS.update(vps._arbol.METODOS)       # v8.5.6, D466
METODOS.update(vps._notificar.METODOS)
METODOS.update(vps._grafico.METODOS)    # v8.5.6, D469

FUNCIONES = [
    ("ejecutar", "ejecutar(ns=None)", "Muestra el primer formulario y no vuelve hasta cerrarlo"),
    ("salir", "salir()", "Cierra todos los formularios"),
    ("msgbox", "msgbox(texto, titulo=\"\", botones=\"aceptar\", icono=\"info\")", "Mensaje modal; devuelve el botón pulsado"),
    ("inputbox", "inputbox(texto, titulo=\"\", defecto=\"\")", "Pide un texto; None si se cancela"),
    ("dialogo_abrir", "dialogo_abrir(titulo, carpeta, tipos)", "Diálogo de abrir; devuelve la ruta o \"\""),
    ("dialogo_guardar", "dialogo_guardar(titulo, carpeta, tipos, nombre)", "Diálogo de guardar; devuelve la ruta o \"\""),
    ("dialogo_carpeta", "dialogo_carpeta(titulo, carpeta)", "Diálogo de elegir carpeta"),
    ("pausa", "pausa(ms)", "Espera sin congelar la ventana"),
    ("evento", "evento(control, nombre)", "Decorador: conecta la función a un evento del control"),
    ("notificar", "notificar(texto, tipo=\"info\", duracion=3000, titulo=\"\")",
     "Aviso no modal en una esquina (tipo: info, exito, aviso o error; duracion 0 = hasta cerrarlo)"),
    ("notificaciones", "notificaciones(formulario=None)", "Las notificaciones que se ven ahora"),
    ("cerrar_notificaciones", "cerrar_notificaciones(formulario=None)", "Cierra todas las notificaciones"),
    ("depurar", "depurar(variables=None)", "Para el programa y enseña las variables en la ventana del depurador"),
]

AYUDA_PROP = {
    "nombre": "El nombre con el que se conectan los eventos (Nombre_evento)",
    "x": "Posición horizontal en píxeles", "y": "Posición vertical en píxeles",
    "ancho": "Ancho en píxeles", "alto": "Alto en píxeles",
    "visible": "Si se ve", "activo": "Si responde al ratón y al teclado",
    "color": "Color del texto (0xRRGGBB, \"#rrggbb\" o un nombre)", "colorfondo": "Color de fondo",
    "fuente": "Fuente: \"DejaVu Sans 14 negrita\"", "anclaje": "Anclaje al redimensionar: \"izq|der|arr|abj\"",
    "texto": "El texto que muestra", "valor": "El valor actual", "elementos": "La lista de elementos",
    "resultado": "El valor que devolvió cerrar(valor) en un formulario modal",
    "datos": "Diccionario para pasar datos entre formularios",
    "origen": "Cadena de conexión: \"sqlite:/home/datos.db\" o \"litemonkey://127.0.0.1:7979/base\"",
    "tabla": "Tabla (SQLite) o colección (lite-monkey) que lee el conector",
    "filtro": "Condición: SQL (WHERE sin la palabra) en SQLite, JSON en lite-monkey",
    "campo": "El campo de la fuente al que está enlazado el control",
    "pagina": "En qué página de unas Pestanas está el control (0 es la primera)",
    "seleccion": "El elemento o la fila elegidos (-1 si ninguno)",
    "columnas": "Los títulos de las columnas", "filas": "Las filas (listas de celdas)",
    "minimo": "El valor más pequeño", "maximo": "El valor más grande", "paso": "Lo que sube o baja cada flecha",
    "decimales": "Cifras decimales que se enseñan (0 = enteros)", "url": "Dirección asociada (la usa tu programa)",
    "ruta": "Ruta del archivo", "titulo": "Título de la ventana del diálogo", "filtro": "Tipos de archivo: \"Texto (*.txt)|*.txt|Todos|*\"",
    "carpeta": "Carpeta inicial del diálogo (al elegir, la elegida)", "archivo": "Ruta del archivo elegido (o propuesto al guardar)",
    "modo": "\"abrir\" o \"guardar\"", "confirmar_sobrescribir": "Si se pregunta antes de pisar un archivo (el diálogo del sistema lo hace siempre)",
    "extension_por_defecto": "Extensión que se añade si el nombre no la lleva: \"txt\"", "separador": "Separador de las columnas del CSV",
    "intervalo": "Milisegundos entre dos «tic»", "raton_x": "X del ratón en el último evento de ratón",
    "raton_y": "Y del ratón en el último evento de ratón", "editable": "Si se pueden editar las celdas",
    "ordenable": "Si un clic en la cabecera ordena por esa columna",
    "tipo": "Tipo de aviso: info, exito, aviso o error", "duracion": "Milisegundos que se ve el aviso (0 = hasta que se cierre)",
    "posicion": "Esquina del formulario: abajo_derecha, arriba_derecha, abajo_centro o arriba_centro",
    "accion": "Texto del botón del aviso (p. ej. Deshacer); su evento es «accion»",
    "nodos": "Los nodos del árbol: una línea por nodo, dos espacios de sangría por nivel, «texto|clave»",
    "expandido": "Si los nodos nuevos empiezan abiertos", "clave": "La clave del nodo elegido",
    "nodo": "La clave del nodo que se abrió o se cerró en el último evento", "motivo": "Por qué se cerró el aviso: tiempo, click, boton, accion o api",
}

PALABRAS = [("False", "Falso"), ("None", "Nada: la ausencia de valor"), ("True", "Verdadero"),
            ("and", "Y lógico"), ("as", "Da un nombre: import x as y, with ... as f"), ("assert", "Comprueba una condición"),
            ("break", "Sale del bucle"), ("class", "Define una clase (Tab: plantilla)"), ("continue", "Pasa a la siguiente vuelta"),
            ("def", "Define una función (Tab: plantilla)"), ("del", "Borra una variable o un elemento"),
            ("elif", "Si no, y además... (Tab: plantilla)"), ("else", "Si no (Tab: plantilla)"),
            ("except", "Recoge un error"), ("finally", "Se ejecuta siempre al final de un try"),
            ("for", "Recorre una secuencia (Tab: plantilla)"), ("from", "from modulo import nombre"),
            ("global", "Usa una variable global dentro de una función"), ("if", "Si... (Tab: plantilla)"),
            ("import", "Importa un módulo"), ("in", "Pertenece a; recorre"), ("is", "Es el mismo objeto"),
            ("lambda", "Función anónima"), ("nonlocal", "Usa una variable de la función de fuera"),
            ("not", "Negación lógica"), ("or", "O lógico"), ("pass", "No hace nada"), ("raise", "Lanza un error"),
            ("return", "Devuelve un valor y sale de la función"), ("try", "Intenta algo que puede fallar (Tab: plantilla)"),
            ("while", "Repite mientras se cumpla (Tab: plantilla)"), ("with", "Bloque con un recurso: with open(...) as f"),
            ("yield", "Devuelve un valor y sigue luego")]

INTEGRADAS = [
    ("print", "print(*valores, sep=\" \", end=\"\\n\")", "Escribe en la salida (el terminal)"),
    ("len", "len(x)", "Cuántos elementos tiene x"),
    ("range", "range(fin) o range(inicio, fin, paso)", "Los números de una secuencia"),
    ("int", "int(x, base=10)", "Convierte a entero"), ("float", "float(x)", "Convierte a decimal"),
    ("str", "str(x)", "Convierte a texto"), ("bool", "bool(x)", "Convierte a verdadero o falso"),
    ("list", "list(x)", "Crea una lista"), ("dict", "dict()", "Crea un diccionario"),
    ("set", "set(x)", "Crea un conjunto"), ("tuple", "tuple(x)", "Crea una tupla"),
    ("open", "open(ruta, modo=\"r\")", "Abre un archivo (modos r, w, a; añade b para binario)"),
    ("abs", "abs(x)", "Valor absoluto"), ("min", "min(a, b, ...)", "El menor"), ("max", "max(a, b, ...)", "El mayor"),
    ("sum", "sum(x)", "Suma los elementos"), ("sorted", "sorted(x, key=None, reverse=False)", "Una lista ordenada"),
    ("enumerate", "enumerate(x, inicio=0)", "Pares (indice, elemento)"), ("zip", "zip(a, b, ...)", "Une secuencias elemento a elemento"),
    ("isinstance", "isinstance(x, tipo)", "Si x es de ese tipo"), ("round", "round(x, cifras=0)", "Redondea"),
    ("input", "input(mensaje=\"\")", "Lee una línea del terminal"), ("type", "type(x)", "El tipo de x"),
    ("map", "map(funcion, x)", "Aplica una función a cada elemento"), ("filter", "filter(funcion, x)", "Los elementos que cumplen"),
    ("any", "any(x)", "Si alguno es verdadero"), ("all", "all(x)", "Si todos son verdaderos"),
    ("reversed", "reversed(x)", "Recorre al revés"), ("chr", "chr(n)", "El carácter con ese código"),
    ("ord", "ord(c)", "El código de un carácter"), ("hex", "hex(n)", "En hexadecimal"), ("bin", "bin(n)", "En binario"),
    ("hasattr", "hasattr(obj, nombre)", "Si tiene ese atributo"), ("getattr", "getattr(obj, nombre, defecto)", "Lee un atributo"),
    ("setattr", "setattr(obj, nombre, valor)", "Escribe un atributo"), ("id", "id(x)", "Identidad del objeto"),
    ("iter", "iter(x)", "Un iterador"), ("next", "next(it, defecto)", "El siguiente elemento"),
    ("repr", "repr(x)", "El texto con el que se escribiría x en el código"), ("callable", "callable(x)", "Si se puede llamar"),
    ("divmod", "divmod(a, b)", "Cociente y resto"), ("pow", "pow(x, y)", "x elevado a y"),
    ("bytes", "bytes(x)", "Secuencia de bytes"), ("bytearray", "bytearray(x)", "Bytes que se pueden cambiar"),
    ("super", "super()", "La clase padre"), ("object", "object", "La clase base de todo"),
    ("dir", "dir(x)", "Los nombres de un objeto"), ("issubclass", "issubclass(a, b)", "Si a hereda de b"),
]

# Plantillas: al escribir la palabra y pulsar Tab. `$1` marca dónde queda el cursor.
SNIPPETS = {
    "def": "def $1():\n    pass",
    "if": "if $1:\n    pass",
    "elif": "elif $1:\n    pass",
    "else": "else:\n    $1pass",
    "for": "for $1 in range(10):\n    pass",
    "while": "while $1:\n    pass",
    "try": "try:\n    $1pass\nexcept Exception as e:\n    print(e)",
    "class": "class $1:\n    def __init__(self):\n        pass",
    "with": "with open($1) as f:\n    pass",
}


def expandir_snippet(clave, sangria=""):
    """(texto, desplazamiento del cursor desde el principio del texto) de la plantilla `clave`, con
    `sangria` delante de cada línea después de la primera; None si no hay plantilla."""
    t = SNIPPETS.get(clave)
    if t is None:
        return None
    lineas = t.split("\n")
    lineas = [lineas[0]] + [sangria + l for l in lineas[1:]]
    t = "\n".join(lineas)
    i = t.find("$1")
    return t.replace("$1", ""), i


def integradas():
    """Las funciones integradas que de verdad tiene este Python."""
    try:
        import builtins
        nombres = dir(builtins)
        return [x for x in INTEGRADAS if x[0] in nombres]
    except Exception:
        return INTEGRADAS


def en_texto(linea):
    """True si el final de `linea` está dentro de un comentario o de un texto entre comillas."""
    q = None
    i = 0
    while i < len(linea):
        c = linea[i]
        if q:
            if c == "\\":
                i += 1
            elif c == q:
                q = None
        elif c == "#":
            return True
        elif c == '"' or c == "'":
            q = c
        i += 1
    return q is not None


def sin_comentario(linea):
    """La línea sin su comentario final (respeta los textos entre comillas)."""
    q = None
    i = 0
    while i < len(linea):
        c = linea[i]
        if q:
            if c == "\\":
                i += 1
            elif c == q:
                q = None
        elif c == "#":
            return linea[:i]
        elif c == '"' or c == "'":
            q = c
        i += 1
    return linea


def _ident(c):
    return c.isalpha() or c.isdigit() or c == "_" or ord(c) > 127


def _nombres_en(texto):
    """Los identificadores de una lista «a, b, *c» (None si hay algo que no es un nombre)."""
    r = []
    for p in texto.split(","):
        p = p.strip().lstrip("*").strip()
        if not p:
            continue
        if not _ident(p[0]) or p[0].isdigit() or not all(_ident(c) for c in p):
            return None
        r.append(p)
    return r


def _doc_de(lineas, i):
    """La primera línea del texto de documentación bajo la línea `i` (def), o ""."""
    for j in range(i + 1, min(i + 3, len(lineas))):
        t = lineas[j].strip()
        if not t:
            continue
        for q in ('\"\"\"', "\'\'\'", '"', "'"):
            if t.startswith(q):
                t = t[len(q):]
                f = t.find(q)
                return (t[:f] if f >= 0 else t).strip()
        break
    return ""


def analizar_codigo(texto, omitir=None):
    """Lo que define el código: {nombre: (clase, firma, ayuda)} con clase «función», «clase» o
    «variable». `omitir` es el número (desde 1) de una línea que no se mira (la que se está
    escribiendo)."""
    r = {}
    lineas = texto.split("\n")
    for i, l0 in enumerate(lineas):
        if omitir is not None and i + 1 == omitir:
            continue
        l = sin_comentario(l0).strip()
        if not l:
            continue
        if l.startswith("def "):
            a = l.find("(")
            if a > 4:
                nombre = l[4:a].strip()
                b = l.rfind(")")
                params = l[a + 1:b] if b > a else l[a + 1:]
                doc = _doc_de(lineas, i)
                r[nombre] = ("función", "%s(%s)" % (nombre, params), doc or "Función del programa")
                for p in params.split(","):
                    p = p.strip().lstrip("*").split("=")[0].split(":")[0].strip()
                    if p and p not in r and _nombres_en(p):
                        r[p] = ("variable", p, "Parámetro de %s" % nombre)
        elif l.startswith("class "):
            nombre = l[6:].split("(")[0].split(":")[0].strip()
            if nombre:
                r[nombre] = ("clase", nombre + "(...)", _doc_de(lineas, i) or "Clase del programa")
        elif l.startswith("import ") or l.startswith("from "):
            parte = l.split(" import ")[1] if l.startswith("from ") and " import " in l else (l[7:] if l.startswith("import ") else "")
            for p in parte.split(","):
                p = p.strip()
                if " as " in p:
                    p = p.split(" as ")[1].strip()
                else:
                    p = p.split(".")[0]
                if p and p != "*" and _nombres_en(p):
                    r[p] = ("módulo", p, "Importado")
        elif l.startswith("for ") and " in " in l:
            for n in _nombres_en(l[4:l.find(" in ")].replace("(", "").replace(")", "")) or []:
                r.setdefault(n, ("variable", n, "Variable del programa"))
        else:
            if " as " in l and (l.startswith("with ") or l.startswith("except")):
                n = l.split(" as ")[-1].split(":")[0].strip()
                if _nombres_en(n):
                    r.setdefault(n, ("variable", n, "Variable del programa"))
                continue
            k = -1
            j = 0
            while j < len(l):
                c = l[j]
                if c in "([{\"'":
                    break
                if c == "=" and l[j + 1:j + 2] != "=":
                    k = j
                    break
                j += 1
            if k > 0:
                izq = l[:k].rstrip()
                if izq and izq[-1] in "+-*/%&|^<>!":
                    izq = izq[:-1].rstrip()
                    if izq and izq[-1] in "*/<>":
                        izq = izq[:-1].rstrip()
                for n in _nombres_en(izq) or []:
                    r.setdefault(n, ("variable", n, "Variable del programa"))
    return r



# propiedades que se leen pero no están en el diseñador
EXTRA_PROPS = {
    "Tabla": ["columna", "valor"], "Lista": ["valor"], "Pestanas": ["valor"], "Lienzo": ["raton_x", "raton_y"],
    "CuadroTexto": ["tecla", "caracter"], "Formulario": ["tecla", "caracter"],
    "Arbol": ["clave", "valor", "nodo"], "Notificacion": ["motivo"],
    "Grafico": ["serie", "indice", "valor", "categoria", "nombre_serie", "oculta"],
    "Calendario": ["fecha", "anio", "mes", "dia"],
    "Reloj": ["anio", "mes", "dia", "hora", "minuto", "segundo", "dia_semana", "fecha", "hora_texto",
              "nombre_dia", "nombre_mes"],
}


def tipo_de(forms, nombre):
    """El tipo de un control (o «Formulario») por su nombre, o None."""
    for f in forms:
        if f["nombre"] == nombre:
            return "Formulario"
        for c in f["controles"]:
            if c["nombre"] == nombre:
                return c["tipo"]
    return None


def _clase(tipo):
    return vps.Formulario if tipo == "Formulario" else vps.TIPOS[tipo]


def miembros(tipo, forms=None, nombre_form=None):
    """[(nombre, clase, firma, ayuda)] con clase en propiedad, método, evento, control."""
    r = []
    cls = _clase(tipo)
    if tipo == "Formulario":
        props = [p[0] for p in vps.Formulario.PROPS]
        props += ["nombre", "resultado", "datos"]
    else:
        props = ["nombre"] + [p[0] for p in cls.todas()] + ["anclaje"]
    props = props + EXTRA_PROPS.get(tipo, [])
    vistos = set()
    for p in props:
        if p in vistos:
            continue
        vistos.add(p)
        r.append((p, "propiedad", p, AYUDA_PROP.get(p, "Propiedad %s" % p)))
    mets = list(METODOS.get(tipo, []))
    if tipo != "Formulario":
        mets += METODOS["Control"]
    for n, firma, ayuda in mets:
        r.append((n, "método", firma, ayuda))
    for ev in cls.EVENTOS:
        r.append(("conectar(\"%s\", " % ev, "evento", "evento %s" % ev,
                  "Evento %s: define def Nombre_%s(): o conéctalo con conectar" % (ev, ev)))
    if tipo == "Formulario" and forms:
        for f in forms:
            if f["nombre"] == nombre_form:
                for c in f["controles"]:
                    r.append((c["nombre"], "control", "%s %s" % (c["nombre"], c["tipo"]), "Control %s del formulario" % c["tipo"]))
    return r


def _cadena_antes(linea):
    """(prefijo, [nombres de la cadena antes del último punto]) de la línea hasta el cursor."""
    i = len(linea)
    while i > 0 and (linea[i - 1].isalpha() or linea[i - 1].isdigit() or linea[i - 1] == "_" or ord(linea[i - 1]) > 127):
        i -= 1
    prefijo = linea[i:]
    cadena = []
    while i > 0 and linea[i - 1] == ".":
        j = i - 1
        k = j
        while k > 0 and (linea[k - 1].isalpha() or linea[k - 1].isdigit() or linea[k - 1] == "_"):
            k -= 1
        if k == j:
            break
        cadena.insert(0, linea[k:j])
        i = k
    return prefijo, cadena


def candidatos(linea, forms, nombre_form=None, codigo=None, num_linea=None, minimo=2):
    """Lo que se ofrece al escribir `linea` (el texto de la línea hasta el cursor).
    Devuelve (prefijo, [(texto a insertar, clase, firma, ayuda)]) ya filtrado por el prefijo
    y ordenado; lista vacía si no hay nada que ofrecer. `codigo` es todo el texto del programa (para
    ofrecer sus funciones y variables; `num_linea`, la línea que se escribe, no cuenta) y `minimo`,
    cuántos caracteres hay que haber escrito para que se ofrezcan palabras sueltas (0 con Ctrl+Espacio)."""
    prefijo, cadena = _cadena_antes(linea)
    if linea.rstrip().startswith("#") or en_texto(linea):
        return prefijo, []
    r = []
    if cadena:
        base = cadena[0]
        if base == "vps" and len(cadena) == 1:
            for n, firma, ayuda in FUNCIONES:
                r.append((n, "función", firma, ayuda))
            for n in vps.__all__:
                if n not in [x[0] for x in r] and n[0].isupper():
                    r.append((n, "clase", n, "Componente o color de VisualPythonS"))
        else:
            tipo = tipo_de(forms, base)
            if tipo is not None and len(cadena) == 1:
                r = miembros(tipo, forms, base)
            elif tipo == "Formulario" and len(cadena) == 2:
                t2 = tipo_de(forms, cadena[1])
                if t2 not in (None, "Formulario"):
                    r = miembros(t2, forms, base)
    else:
        if len(prefijo) < minimo:
            return prefijo, []
        for f in forms:
            r.append((f["nombre"], "formulario", f["nombre"], "Formulario"))
            for c in f["controles"]:
                r.append((c["nombre"], "control", "%s %s" % (c["nombre"], c["tipo"]), "Control %s" % c["tipo"]))
        r.append(("vps", "módulo", "vps", "La API de VisualPythonS"))
        for n, firma, ayuda in FUNCIONES:
            r.append((n, "función", firma, ayuda))
        for n, ayuda in PALABRAS:
            r.append((n, "palabra", n, ayuda))
        for n, firma, ayuda in integradas():
            r.append((n, "integrada", firma, ayuda))
        if codigo:
            usados = set(x[0] for x in r)
            propios = analizar_codigo(codigo, num_linea)
            for n in sorted(propios.keys()):
                if n not in usados:
                    cl, firma, ayuda = propios[n]
                    r.append((n, cl, firma, ayuda))
    p = prefijo.lower()
    r = [x for x in r if x[0].lower().startswith(p) and x[0] != prefijo]
    orden = {"propiedad": 0, "método": 1, "evento": 2, "control": 3, "variable": 4, "función": 5, "clase": 6,
             "módulo": 7, "palabra": 8, "integrada": 9}
    r.sort(key=lambda x: (orden.get(x[1], 9), x[0].lower()))
    return prefijo, r


def ayuda_llamada(linea, forms, codigo=None):
    """La firma de la función o método cuya llamada está abierta en `linea` (hasta el cursor),
    con su ayuda breve; "" si no se conoce."""
    prof = 0
    i = len(linea) - 1
    while i >= 0:
        c = linea[i]
        if c == ")":
            prof += 1
        elif c == "(":
            if prof == 0:
                break
            prof -= 1
        i -= 1
    if i < 0:
        return ""
    # el nombre que precede al paréntesis
    j = i
    while j > 0 and (linea[j - 1].isalpha() or linea[j - 1].isdigit() or linea[j - 1] == "_"):
        j -= 1
    nombre = linea[j:i]
    if not nombre:
        return ""
    _, previa = _cadena_antes(linea[:j])
    if previa == ["vps"] or not previa:
        for n, firma, ayuda in FUNCIONES:
            if n == nombre:
                return "%s - %s" % (firma, ayuda)
    if previa:
        tipo = tipo_de(forms, previa[-1])
        if tipo is not None:
            for n, cl, firma, ayuda in miembros(tipo):
                if n == nombre and cl == "método":
                    return "%s.%s - %s" % (previa[-1], firma, ayuda)
    if not previa:
        # las funciones del propio programa (con la primera línea de su texto de ayuda) y las integradas
        if codigo:
            p = analizar_codigo(codigo).get(nombre)
            if p is not None and p[0] in ("función", "clase"):
                return "%s - %s" % (p[1], p[2])
        for n, firma, ayuda in integradas():
            if n == nombre:
                return "%s - %s" % (firma, ayuda)
    return ""
