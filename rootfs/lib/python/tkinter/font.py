# SPDX-License-Identifier: GPL-2.0-or-later
# tkinter/font.py - fontes (mejora 033, D084). O SaguiOS tem uma fonte só,
# 8x16 de largura fixa: as opções são aceitas e ignoradas, e as medidas são
# as dessa fonte.
NORMAL = "normal"
ROMAN = "roman"
BOLD = "bold"
ITALIC = "italic"

_nomes = {}


class Font:
    def __init__(self, root=None, font=None, name=None, exists=False, **options):
        self.name = name or "font%d" % (len(_nomes) + 1)
        self._op = {"family": "Sagui", "size": 12, "weight": "normal", "slant": "roman",
                    "underline": 0, "overstrike": 0}
        self._op.update(options)
        _nomes[self.name] = self

    def __str__(self):
        return self.name

    def actual(self, option=None, displayof=None):
        return self._op.get(option) if option else dict(self._op)

    def cget(self, option):
        return self._op.get(option)

    __getitem__ = cget

    def configure(self, **options):
        self._op.update(options)

    config = configure

    def copy(self):
        return Font(**self._op)

    def measure(self, text, displayof=None):
        return len(str(text)) * 8

    def metrics(self, *options, **kw):
        m = {"ascent": 13, "descent": 3, "linespace": 16, "fixed": 1}
        if options:
            return m.get(options[0])
        return m


def families(root=None, displayof=None):
    return ("Sagui",)


def names(root=None):
    return tuple(_nomes.keys())


def nametofont(name, root=None):
    return _nomes.get(name) or Font(name=name)
