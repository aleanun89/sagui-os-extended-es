# SPDX-License-Identifier: GPL-2.0-or-later
# tkinter/ttk.py - um ttk mínimo (mejora 033, D084).
#
# O SaguiOS não tem temas: estes são os componentes normais do tkinter com
# o aspecto do sistema, aceitando (e ignorando) as opções próprias do ttk
# (style, padding em alguns...). Há também Progressbar, Separator e um
# Combobox simples. Notebook, Treeview e os outros dão um erro claro.
import tkinter
from tkinter import _int, TclError, FUNDO, BRANCO, TEXTO, BORDA, DESTAQUE, TRILHO, _relevo

_IGNORADAS = ("style", "class_", "cursor")


def _limpar(kw):
    for k in _IGNORADAS:
        kw.pop(k, None)
    p = kw.pop("padding", None)
    return p


def _estado(w, statespec=None):
    """w.state(["disabled"]) / w.state(["!disabled"]) / w.state()"""
    if statespec is None:
        return ("disabled",) if w._op.get("state") == "disabled" else ()
    for s in statespec:
        if s == "disabled":
            w.configure(state="disabled")
        elif s == "!disabled":
            w.configure(state="normal")
    return ()


def _instate(w, statespec, callback=None, *args):
    ok = True
    for s in statespec:
        neg = s.startswith("!")
        tem = (w._op.get("state") == "disabled") if s.strip("!") == "disabled" else False
        ok = ok and (tem != neg)
    if ok and callback:
        return callback(*args)
    return ok


def _fazer(base, com_padding=False):
    class Ttk(base):
        def __init__(self, master=None, **kw):
            p = _limpar(kw)
            if p is not None and com_padding:
                px, py = tkinter._pares(p) if not isinstance(p, (tuple, list)) or len(p) <= 2 else (p[0], p[1])
                kw.setdefault("padx", px if isinstance(px, int) else px[0])
                kw.setdefault("pady", py if isinstance(py, int) else py[0])
            base.__init__(self, master, **kw)

        def configure(self, cnf=None, **kw):
            _limpar(kw)
            return base.configure(self, cnf, **kw)

        config = configure

        def state(self, statespec=None):
            return _estado(self, statespec)

        def instate(self, statespec, callback=None, *args):
            return _instate(self, statespec, callback, *args)

    return Ttk


class Frame(_fazer(tkinter.Frame, True)):
    pass


class Label(_fazer(tkinter.Label, True)):
    pass


class Button(_fazer(tkinter.Button, True)):
    pass


class Checkbutton(_fazer(tkinter.Checkbutton)):
    pass


class Radiobutton(_fazer(tkinter.Radiobutton)):
    pass


class Entry(_fazer(tkinter.Entry)):
    pass


class Spinbox(_fazer(tkinter.Spinbox)):
    pass


class Scale(_fazer(tkinter.Scale)):
    def __init__(self, master=None, **kw):
        kw.setdefault("orient", "horizontal")
        kw.setdefault("showvalue", 0)
        kw.setdefault("resolution", 0.01)
        _limpar(kw)
        tkinter.Scale.__init__(self, master, **kw)


class Scrollbar(_fazer(tkinter.Scrollbar)):
    pass


class LabelFrame(_fazer(tkinter.LabelFrame, True)):
    pass


Labelframe = LabelFrame


class Menubutton(_fazer(tkinter.Menubutton)):
    pass


class OptionMenu(tkinter.OptionMenu):
    def __init__(self, master, variable, default=None, *values, **kw):
        _limpar(kw)
        if default is None and values:
            default = values[0]
        vals = values if values else (default,)
        tkinter.OptionMenu.__init__(self, master, variable, vals[0], *vals[1:], **kw)
        if default is not None:
            variable.set(default)


class PanedWindow(_fazer(tkinter.PanedWindow)):
    pass


Panedwindow = PanedWindow


class Separator(tkinter.Widget):
    _classe = "TSeparator"
    _padrao = {"orient": "horizontal", "takefocus": 0, "cursor": "", "style": "", "class": ""}

    def _natural(self):
        return (2, 2)

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        if str(self._op.get("orient")).startswith("h"):
            v.rect(x, y + h // 2 - 1, w, 1, BORDA)
            v.rect(x, y + h // 2, w, 1, BRANCO)
        else:
            v.rect(x + w // 2 - 1, y, 1, h, BORDA)
            v.rect(x + w // 2, y, 1, h, BRANCO)


class Sizegrip(tkinter.Frame):
    pass


class Progressbar(tkinter.Widget):
    _classe = "TProgressbar"
    _padrao = {"orient": "horizontal", "length": 100, "mode": "determinate", "maximum": 100.0,
               "value": 0.0, "variable": None, "phase": 0, "takefocus": 0, "cursor": "", "style": "",
               "class": ""}

    def _iniciar(self):
        self._passo_id = None
        self._fase = 0

    def _natural(self):
        c = _int(self._op.get("length", 100))
        return (c, 18) if str(self._op.get("orient")).startswith("h") else (18, c)

    def _valor(self):
        v = self._op.get("variable")
        if isinstance(v, tkinter.Variable):
            try:
                return float(v._valor)
            except (TypeError, ValueError):
                return 0.0
        return float(self._op.get("value", 0))

    def _variavel_mudou(self, var):
        self._redesenhar()

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        v.rect(x, y, w, h, TRILHO)
        _relevo(v, x, y, w, h, "sunken", 1)
        maximo = float(self._op.get("maximum", 100)) or 100
        hor = str(self._op.get("orient")).startswith("h")
        comp = (w if hor else h) - 4
        if self._op.get("mode") == "indeterminate":
            bloco = max(10, comp // 5)
            p = self._fase % max(1, comp - bloco)
            if hor:
                v.rect(x + 2 + p, y + 2, bloco, h - 4, DESTAQUE)
            else:
                v.rect(x + 2, y + 2 + p, w - 4, bloco, DESTAQUE)
            return
        f = max(0.0, min(1.0, self._valor() / maximo))
        n = int(comp * f)
        if hor:
            v.rect(x + 2, y + 2, n, h - 4, DESTAQUE)
        else:
            v.rect(x + 2, y + h - 2 - n, w - 4, n, DESTAQUE)

    def step(self, amount=1.0):
        if self._op.get("mode") == "indeterminate":
            self._fase += int(amount * 4)
            self._redesenhar()
            return
        maximo = float(self._op.get("maximum", 100))
        novo = (self._valor() + amount) % maximo if maximo else 0
        v = self._op.get("variable")
        if isinstance(v, tkinter.Variable):
            v.set(novo)
        else:
            self._op["value"] = novo
        self._redesenhar()

    def start(self, interval=50):
        self.stop()

        def tique():
            self.step()
            self._passo_id = self.after(interval, tique)
        self._passo_id = self.after(interval, tique)

    def stop(self):
        if self._passo_id:
            self.after_cancel(self._passo_id)
            self._passo_id = None

    def __setitem__(self, k, v):
        self.configure(**{k: v})


class Combobox(Entry):
    """Entry com uma lista de valores (o menu do sistema)."""
    _classe = "TCombobox"
    _padrao = dict(tkinter.Entry._padrao, values=(), postcommand=None, height=10, exportselection=1)
    _SETA = 16

    def _natural(self):
        l, a = tkinter.Entry._natural(self)
        return l + self._SETA, a

    def _cabem(self):
        b = _int(self._op.get("borderwidth", 2))
        return max(1, (self._wl - self._SETA - 2 * (b + self._PAD)) // 8)

    def _desenhar(self, v):
        tkinter.Entry._desenhar(self, v)
        x = self._ax + self._wl - self._SETA - 2
        y = self._ay + 2
        h = self._wa - 4
        v.rect(x, y, self._SETA, h, tkinter.BOTAO)
        _relevo(v, x, y, self._SETA, h, "raised", 1)
        cx, cy = x + self._SETA // 2, y + h // 2
        v.poligono([(cx - 4, cy - 2), (cx + 4, cy - 2), (cx, cy + 3)], TEXTO)

    def _valores(self):
        v = self._op.get("values") or ()
        return v.split() if isinstance(v, str) else list(v)

    def _abrir(self):
        pc = self._op.get("postcommand")
        if pc:
            pc()
        vals = self._valores()
        if not vals:
            return
        top = self._top()
        r = top._v.menu_contextual(self._ax, self._ay + self._wa, [str(x) for x in vals[:60]])
        if r is not None:
            self.set(vals[r])
            self.event_generate("<<ComboboxSelected>>")

    def _tratar(self, tipo, ev):
        if self._op.get("state") == "disabled":
            return None
        if tipo == "ButtonPress" and ev.num == 1 and (ev.x >= self._wl - self._SETA - 2 or self._op.get("state") == "readonly"):
            self._abrir()
            return None
        if tipo == "KeyPress" and ev.keysym == "Down":
            self._abrir()
            return None
        return tkinter.Entry._tratar(self, tipo, ev)

    def current(self, newindex=None):
        vals = self._valores()
        if newindex is None:
            try:
                return vals.index(self.get())
            except ValueError:
                return -1
        self.set(vals[newindex])

    def set(self, value):
        self._definir(str(value))
        self._cursor = len(self._valor)
        self._sel = None


class Style:
    def __init__(self, master=None):
        pass

    def configure(self, style, query_opt=None, **kw):
        return None

    def map(self, style, query_opt=None, **kw):
        return {}

    def lookup(self, style, option, state=None, default=None):
        return default

    def layout(self, style, layoutspec=None):
        return []

    def element_create(self, *args, **kw):
        pass

    def theme_names(self):
        return ("sagui",)

    def theme_use(self, themename=None):
        return "sagui"

    def theme_create(self, *args, **kw):
        pass

    def theme_settings(self, *args, **kw):
        pass


_FALTAM = ("Notebook", "Treeview", "LabeledScale", "Labeledscale")


def __getattr__(nome):
    if nome in _FALTAM:
        raise AttributeError("tkinter.ttk.%s no está disponible en SaguiOS (ver /ayuda/python-tk.md)" % nome)
    raise AttributeError("module 'tkinter.ttk' has no attribute '%s'" % nome)
