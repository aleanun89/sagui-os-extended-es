# SPDX-License-Identifier: GPL-2.0-or-later
# basedatos._idioma - `_("texto")` para los exploradores (D313, D333): la cadena en español es la clave. Si hay
# un catálogo (`/lib/python/basedatos/idiomas/<idioma>.py` con un diccionario `TEXTOS`) y el idioma de `LANG` no
# es el español, se traduce; si no, sale el español tal cual, así que en español todo es idéntico a escribir
# la cadena directamente.
import os

_cat = None


def _cargar():
    global _cat
    _cat = {}
    try:
        lang = (os.getenv("LANG") or "").split(".")[0].replace("-", "_")
    except Exception:
        lang = ""
    if not lang or lang.startswith("es") or lang in ("C", "POSIX"):
        return
    for nombre in (lang, lang.split("_")[0]):
        try:
            mod = __import__("basedatos.idiomas." + nombre, None, None, ("TEXTOS",))
            _cat = mod.TEXTOS
            return
        except Exception:
            pass


def _(texto):
    if _cat is None:
        _cargar()
    return _cat.get(texto, texto)


def N_(texto):
    """Solo marca la cadena (tablas estáticas); se traduce al usarla con _()."""
    return texto
