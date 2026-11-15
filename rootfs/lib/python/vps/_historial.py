# SPDX-License-Identifier: GPL-2.0-or-later
# vps._historial - deshacer y rehacer del diseñador (mejora 124, D299).
#
# Una «foto» es una copia de todo lo que se edita: los formularios con sus controles y el texto
# de codigo.py (renombrar un control cambia las dos cosas a la vez y se deshace de una vez).
# Es lo más sencillo y lo más seguro: no hay que escribir la operación inversa de cada cambio.


def copiar_forms(forms):
    """Copia profunda de la lista de formularios del modelo (diccionarios y listas)."""
    return [{"nombre": f["nombre"], "props": _copiar_props(f["props"]),
             "controles": [{"tipo": c["tipo"], "nombre": c["nombre"], "padre": c["padre"],
                            "props": _copiar_props(c["props"])} for c in f["controles"]]}
            for f in forms]


def _copiar_props(p):
    return dict((k, list(v) if isinstance(v, list) else v) for k, v in p.items())


class Historial:
    """Dos pilas de fotos. `punto(foto)` se llama ANTES de cada cambio; deshacer devuelve la
    foto anterior (guardando la actual para rehacer) y rehacer la deshecha."""

    MAXIMO = 100

    def __init__(self):
        self.deshacer = []
        self.rehacer = []

    def punto(self, foto):
        self.deshacer.append(foto)
        if len(self.deshacer) > self.MAXIMO:
            del self.deshacer[0]
        self.rehacer = []

    def descartar_ultimo(self):
        if self.deshacer:
            self.deshacer.pop()

    def ultimo(self):
        return self.deshacer[-1] if self.deshacer else None

    def puede_deshacer(self):
        return bool(self.deshacer)

    def puede_rehacer(self):
        return bool(self.rehacer)

    def paso_atras(self, actual):
        """La foto a restaurar al deshacer (`actual` queda para rehacer), o None."""
        if not self.deshacer:
            return None
        self.rehacer.append(actual)
        return self.deshacer.pop()

    def paso_adelante(self, actual):
        if not self.rehacer:
            return None
        self.deshacer.append(actual)
        return self.rehacer.pop()

    def vaciar(self):
        self.deshacer = []
        self.rehacer = []
