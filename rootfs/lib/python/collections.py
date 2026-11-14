# SPDX-License-Identifier: GPL-2.0-or-later
# collections.py - o collections do MicroPython (deque, namedtuple,
# OrderedDict) mais o defaultdict, que o MicroPython 1.24 não traz.
#
# "collections" é um módulo embutido "extensível": um collections.py no
# sys.path tem prioridade, e o embutido continua acessível como
# "ucollections" (D067).
from ucollections import *


class defaultdict(dict):
    """dict que cria o valor que falta com default_factory():
    d = defaultdict(list); d["x"].append(1)"""

    def __init__(self, default_factory=None, *args, **kw):
        super().__init__(*args, **kw)
        self.default_factory = default_factory

    def __getitem__(self, chave):
        try:
            return super().__getitem__(chave)
        except KeyError:
            return self.__missing__(chave)

    def __missing__(self, chave):
        if self.default_factory is None:
            raise KeyError(chave)
        v = self.default_factory()
        self[chave] = v
        return v

    def copy(self):
        return defaultdict(self.default_factory, self)

    def __repr__(self):
        return "defaultdict(%r, %s)" % (self.default_factory, dict.__repr__(self))
