# SPDX-License-Identifier: GPL-2.0-or-later
# vps._refactor - renombrar un control y actualizar su código (mejora 124, D299).
#
# Un análisis de tokens, no un reemplazo ciego: solo se cambian los NOMBRES de Python, nunca el
# texto de una cadena ni de un comentario, ni un atributo con el mismo nombre (`otro.Boton1`).
# Se renombran:
#   - la referencia al control:          Boton1.texto      ->  Aceptar.texto
#   - sus manejadores de eventos:        def Boton1_click  ->  def Aceptar_click
#   - lo que haya dentro de una f-cadena: f"{Boton1.texto}" ->  f"{Aceptar.texto}"


def _es_letra(c):
    return c == "_" or c.isalpha() or ord(c) > 127


def _es_cifra_o_letra(c):
    return _es_letra(c) or c.isdigit()


def tokens(texto):
    """[(clase, inicio, fin)] con clase en «nombre», «cadena», «comentario», «numero», «signo».
    Las cadenas triples y con prefijo (r, b, f...) van enteras; de las f-cadenas hay que mirar
    dentro: usa `nombres_en_cadena`. Los espacios y saltos de línea no son tokens."""
    r = []
    n = len(texto)
    i = 0
    while i < n:
        c = texto[i]
        if c == " " or c == "\t" or c == "\n" or c == "\r":
            i += 1
        elif c == "\\" and i + 1 < n and texto[i + 1] == "\n":
            i += 2
        elif c == "#":
            j = texto.find("\n", i)
            j = n if j < 0 else j
            r.append(("comentario", i, j))
            i = j
        elif c == '"' or c == "'":
            j = _fin_cadena(texto, i)
            r.append(("cadena", i, j))
            i = j
        elif _es_letra(c):
            j = i
            while j < n and _es_cifra_o_letra(texto[j]):
                j += 1
            # prefijo de cadena: f"..", rb'..'
            if j < n and (texto[j] == '"' or texto[j] == "'") and j - i <= 2 and texto[i:j].lower() in (
                    "r", "b", "f", "u", "rb", "br", "fr", "rf"):
                k = _fin_cadena(texto, j)
                r.append(("cadena", i, k))
                i = k
            else:
                r.append(("nombre", i, j))
                i = j
        elif c.isdigit():
            j = i
            while j < n and (_es_cifra_o_letra(texto[j]) or texto[j] == "."):
                j += 1
            r.append(("numero", i, j))
            i = j
        else:
            r.append(("signo", i, i + 1))
            i += 1
    return r


def _fin_cadena(texto, i):
    """Índice justo detrás de la cadena que empieza en texto[i] (una comilla)."""
    n = len(texto)
    q = texto[i]
    if texto[i:i + 3] == q * 3:
        j = i + 3
        while j < n:
            if texto[j] == "\\":
                j += 2
                continue
            if texto[j:j + 3] == q * 3:
                return j + 3
            j += 1
        return n
    j = i + 1
    while j < n and texto[j] != q and texto[j] != "\n":
        if texto[j] == "\\":
            j += 1
        j += 1
    return min(j + 1, n)


def _prefijo_f(texto, ini):
    j = ini
    while j < len(texto) and texto[j] not in "\"'":
        j += 1
    return "f" in texto[ini:j].lower()


def nombres_en_cadena(texto, ini, fin):
    """Los nombres dentro de las llaves {..} de una f-cadena: (inicio, fin, token anterior)."""
    r = []
    i = ini
    while i < fin:
        c = texto[i]
        if c == "{":
            if i + 1 < fin and texto[i + 1] == "{":
                i += 2
                continue
            prof = 1
            j = i + 1
            while j < fin and prof:
                if texto[j] == "{":
                    prof += 1
                elif texto[j] == "}":
                    prof -= 1
                j += 1
            expr = texto[i + 1:j - 1]
            tk = tokens(expr)
            for k in range(len(tk)):
                clase, a, b = tk[k]
                if clase == "nombre":
                    previo = expr[tk[k - 1][1]:tk[k - 1][2]] if k > 0 else ""
                    r.append((i + 1 + a, i + 1 + b, previo))
            i = j
        else:
            i += 1
    return r


def nombres_de_codigo(texto):
    """[(inicio, fin, texto del nombre, nombre anterior-significativo, signo siguiente)] de todos los
    nombres de Python del código, incluidos los de dentro de las f-cadenas. El anterior es el token
    de atrás (por ejemplo «.»); el siguiente, el de delante."""
    toks = tokens(texto)
    r = []
    prof = 0
    for k, (clase, a, b) in enumerate(toks):
        if clase == "signo":
            if texto[a] in "([{":
                prof += 1
            elif texto[a] in ")]}":
                prof -= 1
        if clase == "nombre":
            previo = texto[toks[k - 1][1]:toks[k - 1][2]] if k > 0 else ""
            sig = texto[toks[k + 1][1]:toks[k + 1][2]] if k + 1 < len(toks) else ""
            sig2 = texto[toks[k + 2][1]:toks[k + 2][2]] if k + 2 < len(toks) else ""
            r.append((a, b, texto[a:b], previo, sig, sig2, prof))
        elif clase == "cadena" and _prefijo_f(texto, a):
            for x, y, previo in nombres_en_cadena(texto, a, b):
                r.append((x, y, texto[x:y], previo, "", "", 1))
    return r


def renombrar(codigo, viejo, nuevo, eventos):
    """Cambia `viejo` por `nuevo` (y `viejo_evento` por `nuevo_evento` para cada evento de
    `eventos`) en el código. Devuelve (codigo_nuevo, cambios, conflictos):
       cambios    [(numero de línea, línea antes, línea después)], una por línea cambiada
       conflictos [nombre ya definido en el código que chocaría con el nuevo]
    No toca cadenas, comentarios ni atributos (`algo.viejo`), ni argumentos con nombre (`f(viejo=1)`)."""
    mapa = {viejo: nuevo}
    for ev in eventos:
        mapa["%s_%s" % (viejo, ev)] = "%s_%s" % (nuevo, ev)
    cambios_pos = []
    existentes = set()
    for a, b, nom, previo, sig, sig2, prof in nombres_de_codigo(codigo):
        existentes.add(nom)
        if nom not in mapa:
            continue
        if previo == ".":
            continue
        if sig == "=" and sig2 != "=" and prof > 0 and previo in ("(", ","):
            continue
        cambios_pos.append((a, b, mapa[nom]))
    conflictos = [nuevo_nom for viejo_nom, nuevo_nom in mapa.items()
                  if nuevo_nom in existentes and nuevo_nom not in mapa and viejo_nom != nuevo_nom]
    if not cambios_pos:
        return codigo, [], conflictos
    partes = []
    ult = 0
    for a, b, nom in cambios_pos:
        partes.append(codigo[ult:a])
        partes.append(nom)
        ult = b
    partes.append(codigo[ult:])
    nuevo_codigo = "".join(partes)
    antes = codigo.split("\n")
    despues = nuevo_codigo.split("\n")
    cambios = []
    for i in range(len(antes)):
        if i < len(despues) and antes[i] != despues[i]:
            cambios.append((i + 1, antes[i], despues[i]))
    return nuevo_codigo, cambios, conflictos
