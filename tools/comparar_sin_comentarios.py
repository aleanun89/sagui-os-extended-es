#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Comprueba que un archivo solo difiere de su versión en HEAD en los comentarios.

Uso:
    python3 tools/comparar_sin_comentarios.py ARCHIVO [ARCHIVO...]

Para cada archivo compara con `git show HEAD:ruta`:
  - que tenga el mismo número de líneas (así no cambian los __LINE__ ni la depuración);
  - que el contenido sin comentarios sea idéntico, línea a línea:
      .c, .h, .ld      -> se quitan los comentarios /* */ y // respetando cadenas y caracteres;
      .asm, .inc       -> se quita lo que va tras ';' fuera de cadenas;
      .py              -> se compara el flujo de tokenize sin COMMENT/NL y el ast sin docstrings
                          (los docstrings son documentación y se ignoran);
      Makefile, .mk, .sh -> se quita lo que va tras '#' fuera de cadenas.

Devuelve 0 si todo cuadra y 1 si algún archivo difiere (dice archivo y línea).
Los espacios al final de línea no cuentan.
"""

import ast
import io
import os
import subprocess
import sys
import tokenize


def raiz_del_repositorio():
    """Devuelve la carpeta raíz del repositorio git donde está este script."""
    carpeta = os.path.dirname(os.path.abspath(__file__))
    return subprocess.run(
        ["git", "rev-parse", "--show-toplevel"], cwd=carpeta,
        capture_output=True, text=True, check=True).stdout.strip()


def version_en_head(raiz, ruta_abs):
    """Lee el contenido del archivo tal como está en HEAD (bytes)."""
    ruta_rel = os.path.relpath(ruta_abs, raiz).replace(os.sep, "/")
    resultado = subprocess.run(
        ["git", "show", "HEAD:" + ruta_rel], cwd=raiz, capture_output=True)
    if resultado.returncode != 0:
        raise RuntimeError("no existe en HEAD: " + ruta_rel)
    return resultado.stdout


def quitar_comentarios_c(texto):
    """Sustituye cada comentario /* */ o // por un espacio (y conserva los saltos de línea)."""
    salida = []
    i = 0
    n = len(texto)
    while i < n:
        c = texto[i]
        siguiente = texto[i + 1] if i + 1 < n else ""
        if c == "/" and siguiente == "*":
            fin = texto.find("*/", i + 2)
            fin = n if fin < 0 else fin + 2
            trozo = texto[i:fin]
            salida.append(" " + "\n" * trozo.count("\n"))
            i = fin
        elif c == "/" and siguiente == "/":
            fin = texto.find("\n", i)
            fin = n if fin < 0 else fin
            salida.append(" ")
            i = fin
        elif c == '"' or c == "'":
            # Una cadena o un carácter termina en la misma comilla o al acabar la línea
            # (así una directiva como #error don't no rompe el análisis).
            j = i + 1
            while j < n and texto[j] != c and texto[j] != "\n":
                if texto[j] == "\\":
                    j += 1
                j += 1
            salida.append(texto[i:j + 1])
            i = j + 1
        else:
            salida.append(c)
            i += 1
    return "".join(salida)


def quitar_comentarios_por_marca(texto, marca, ignorar_si_precedido_por=""):
    """Quita lo que va tras 'marca' (';' o '#') fuera de cadenas, línea a línea."""
    lineas = []
    for linea in texto.split("\n"):
        comilla = None
        corte = len(linea)
        i = 0
        while i < len(linea):
            c = linea[i]
            if comilla:
                if c == "\\":
                    i += 1
                elif c == comilla:
                    comilla = None
            elif c in "\"'":
                comilla = c
            elif c == marca:
                anterior = linea[i - 1] if i > 0 else ""
                if not (anterior and anterior in ignorar_si_precedido_por):
                    corte = i
                    break
            i += 1
        lineas.append(linea[:corte])
    return lineas


def lineas_normalizadas(lineas):
    """Quita los espacios sobrantes al final de cada línea."""
    return [linea.rstrip() for linea in lineas]


def firma_python(texto):
    """Devuelve (flujo de tokens sin comentarios ni docstrings, ast sin docstrings)."""
    arbol = ast.parse(texto)
    lineas = texto.split("\n")

    # Posiciones (inicio, fin) en caracteres de los docstrings.
    # El ast da columnas en bytes UTF-8, así que se pasan a caracteres.
    def a_caracteres(linea_n, col_bytes):
        linea = lineas[linea_n - 1].encode("utf-8")
        return len(linea[:col_bytes].decode("utf-8"))

    rangos_doc = []
    nodos_doc = []
    for nodo in ast.walk(arbol):
        if isinstance(nodo, (ast.Module, ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)):
            if nodo.body and isinstance(nodo.body[0], ast.Expr) \
                    and isinstance(nodo.body[0].value, ast.Constant) \
                    and isinstance(nodo.body[0].value.value, str):
                expr = nodo.body[0].value
                inicio = (expr.lineno, a_caracteres(expr.lineno, expr.col_offset))
                fin = (expr.end_lineno, a_caracteres(expr.end_lineno, expr.end_col_offset))
                rangos_doc.append((inicio, fin))
                nodos_doc.append(nodo.body[0])

    # Flujo de tokens sin COMMENT ni NL; los STRING dentro de un docstring cuentan como 'DOC'.
    tokens = []
    for tok in tokenize.generate_tokens(io.StringIO(texto).readline):
        if tok.type in (tokenize.COMMENT, tokenize.NL, tokenize.ENCODING):
            continue
        if tok.type == tokenize.STRING:
            if any(ini <= tok.start and tok.end <= fin for ini, fin in rangos_doc):
                tokens.append(("DOC", ""))
                continue
        tokens.append((tokenize.tok_name[tok.type], tok.string))

    # ast sin docstrings: se quita la primera sentencia de cada bloque si es un docstring.
    for nodo in ast.walk(arbol):
        if isinstance(nodo, (ast.Module, ast.ClassDef, ast.FunctionDef, ast.AsyncFunctionDef)):
            if nodo.body and any(nodo.body[0] is d for d in nodos_doc):
                nodo.body = nodo.body[1:]

    return tokens, ast.dump(arbol)


def primera_diferencia(a, b):
    """Devuelve el número (1-based) de la primera línea que difiere, o None."""
    for i, (x, y) in enumerate(zip(a, b), start=1):
        if x != y:
            return i
    if len(a) != len(b):
        return min(len(a), len(b)) + 1
    return None


def comparar(ruta_abs, raiz):
    """Devuelve una lista de problemas (vacía si el archivo está bien)."""
    problemas = []
    antes = version_en_head(raiz, ruta_abs).decode("utf-8", errors="surrogateescape")
    with open(ruta_abs, "rb") as f:
        ahora = f.read().decode("utf-8", errors="surrogateescape")

    if antes.count("\n") != ahora.count("\n"):
        problemas.append("distinto número de líneas: HEAD %d, ahora %d"
                         % (antes.count("\n") + 1, ahora.count("\n") + 1))
        return problemas

    nombre = os.path.basename(ruta_abs)
    extension = os.path.splitext(nombre)[1].lower()

    if extension == ".py":
        tokens_a, ast_a = firma_python(antes)
        tokens_b, ast_b = firma_python(ahora)
        if tokens_a != tokens_b:
            # Se busca la primera diferencia para dar la línea.
            for i, (x, y) in enumerate(zip(tokens_a, tokens_b)):
                if x != y:
                    problemas.append("código distinto (tokens) cerca del token %d: %r ≠ %r"
                                     % (i, x, y))
                    break
            else:
                problemas.append("código distinto (tokens): longitudes %d y %d"
                                 % (len(tokens_a), len(tokens_b)))
        if ast_a != ast_b:
            problemas.append("código distinto (ast sin docstrings)")
        return problemas

    if extension in (".c", ".h", ".ld"):
        lineas_a = lineas_normalizadas(quitar_comentarios_c(antes).split("\n"))
        lineas_b = lineas_normalizadas(quitar_comentarios_c(ahora).split("\n"))
    elif extension in (".asm", ".inc"):
        lineas_a = lineas_normalizadas(quitar_comentarios_por_marca(antes, ";"))
        lineas_b = lineas_normalizadas(quitar_comentarios_por_marca(ahora, ";"))
    elif extension in (".sh", ".mk") or nombre == "Makefile":
        lineas_a = lineas_normalizadas(quitar_comentarios_por_marca(antes, "#", "$\\"))
        lineas_b = lineas_normalizadas(quitar_comentarios_por_marca(ahora, "#", "$\\"))
    else:
        return ["tipo de archivo no soportado: " + nombre]

    linea = primera_diferencia(lineas_a, lineas_b)
    if linea is not None:
        a = lineas_a[linea - 1] if linea <= len(lineas_a) else "<fin>"
        b = lineas_b[linea - 1] if linea <= len(lineas_b) else "<fin>"
        problemas.append("código distinto en la línea %d:\n    HEAD: %r\n    ahora: %r"
                         % (linea, a, b))
    return problemas


def main(argv):
    if not argv:
        print("Uso: python3 tools/comparar_sin_comentarios.py ARCHIVO [ARCHIVO...]",
              file=sys.stderr)
        return 2
    raiz = raiz_del_repositorio()
    codigo = 0
    for ruta in argv:
        ruta_abs = os.path.abspath(ruta)
        try:
            problemas = comparar(ruta_abs, raiz)
        except RuntimeError as e:
            problemas = [str(e)]
        except (SyntaxError, UnicodeDecodeError, tokenize.TokenError) as e:
            problemas = ["no se puede analizar: %s" % e]
        if problemas:
            codigo = 1
            print("FALLA %s" % ruta)
            for p in problemas:
                print("    " + p)
        else:
            print("ok    %s" % ruta)
    return codigo


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
