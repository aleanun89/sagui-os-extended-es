# SPDX-License-Identifier: GPL-2.0-or-later
# basedatos._csv - lector de CSV sin dependencias (D333): comillas dobles, «""» dentro de un campo, saltos de
# línea dentro de las comillas y fin de línea \n o \r\n. Sin tkinter: se prueba en el anfitrión.


def detectar_separador(texto):
    """«,», «;» o tabulador: el que más veces aparece en la primera línea (fuera de comillas)."""
    primera = texto.split("\n", 1)[0]
    cuenta = {",": 0, ";": 0, "\t": 0}
    dentro = False
    for c in primera:
        if c == '"':
            dentro = not dentro
        elif not dentro and c in cuenta:
            cuenta[c] += 1
    mejor = max(cuenta, key=lambda k: cuenta[k])
    return mejor if cuenta[mejor] else ","


def de_csv(texto, separador=None):
    """Las filas del CSV como listas de textos (las filas totalmente vacías se saltan)."""
    if texto.startswith("﻿"):
        texto = texto[1:]
    sep = separador or detectar_separador(texto)
    filas, fila, campo = [], [], []
    dentro, quedo = False, False        # `quedo`: se vio algo en la fila (aunque sea un campo vacío)
    i, n = 0, len(texto)
    while i < n:
        c = texto[i]
        if dentro:
            if c == '"':
                if i + 1 < n and texto[i + 1] == '"':
                    campo.append('"')
                    i += 1
                else:
                    dentro = False
            else:
                campo.append(c)
        elif c == '"' and not campo:
            dentro = True
            quedo = True
        elif c == sep:
            fila.append("".join(campo))
            campo = []
            quedo = True
        elif c == "\r":
            pass
        elif c == "\n":
            if quedo or campo:
                fila.append("".join(campo))
                filas.append(fila)
            fila, campo, quedo = [], [], False
        else:
            campo.append(c)
            quedo = True
        i += 1
    if quedo or campo:
        fila.append("".join(campo))
        filas.append(fila)
    return filas
