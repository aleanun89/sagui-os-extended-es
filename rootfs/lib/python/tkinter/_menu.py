# SPDX-License-Identifier: GPL-2.0-or-later
# tkinter/_menu.py - Menu, Menubutton e OptionMenu (mejora 033, D084).
#
# Os menus usam o menu de contexto do SaguiOS com submenus (D051, D064): a
# barra de menus é desenhada pelo tkinter no alto da janela, e cada menu que
# abre (da barra, de um Menubutton ou com tk_popup) é o menu do sistema.
from tkinter import (Widget, Label, Button, Variable, StringVar, TclError, Event, EventType, _int, _bool,
                     _cor, _relevo, FUNDO, TEXTO, FRACO, BOTAO, BORDA, LETRA_L, LETRA_A, BRANCO, SELECAO,
                     ESCURO, _relatar, _igual)

ALTURA_BARRA = 22


class Menu(Widget):
    _classe = "Menu"
    _padrao = {"tearoff": 1, "background": FUNDO, "foreground": TEXTO, "activebackground": SELECAO,
               "activeforeground": BRANCO, "disabledforeground": FRACO, "font": None, "borderwidth": 1,
               "relief": "raised", "postcommand": None, "title": "", "type": "normal",
               "selectcolor": TEXTO, "tearoffcommand": None, "cursor": "", "takefocus": 0,
               "activeborderwidth": 1, "activerelief": "flat"}

    def _iniciar(self):
        self._itens = []
        if _bool(self._op.get("tearoff", 1)):
            self._itens.append({"type": "tearoff"})
        self._barra_de = None
        self._posicoes = []   # [(x0, x1, índice)] na barra

    # ---- itens
    def add(self, itemType, cnf={}, **kw):
        self.insert(len(self._itens), itemType, cnf, **kw)

    def add_command(self, cnf={}, **kw):
        self.add("command", cnf, **kw)

    def add_cascade(self, cnf={}, **kw):
        self.add("cascade", cnf, **kw)

    def add_checkbutton(self, cnf={}, **kw):
        self.add("checkbutton", cnf, **kw)

    def add_radiobutton(self, cnf={}, **kw):
        self.add("radiobutton", cnf, **kw)

    def add_separator(self, cnf={}, **kw):
        self.add("separator", cnf, **kw)

    def insert(self, index, itemType, cnf={}, **kw):
        kw = dict(cnf, **kw)
        item = {"type": itemType, "label": "", "command": None, "menu": None, "variable": None,
                "onvalue": 1, "offvalue": 0, "value": None, "state": "normal", "accelerator": "",
                "underline": -1, "image": None, "compound": "none", "foreground": None,
                "background": None, "hidemargin": 0, "columnbreak": 0, "selectcolor": None,
                "indicatoron": 1, "font": None, "bitmap": None, "activebackground": None,
                "activeforeground": None, "selectimage": None}
        for k, v in kw.items():
            k = k.rstrip("_")
            if k not in item:
                raise TclError('unknown option "-%s"' % k)
            item[k] = v
        if itemType == "checkbutton" and item["variable"] is None:
            from tkinter import IntVar
            item["variable"] = IntVar(value=item["offvalue"])
        if itemType == "radiobutton" and item["value"] is None:
            item["value"] = item["label"]
        i = len(self._itens) if index == "end" else self.index(index)
        if i is None:
            i = len(self._itens)
        self._itens.insert(i, item)
        self._mudou()

    def insert_command(self, index, cnf={}, **kw):
        self.insert(index, "command", cnf, **kw)

    def insert_cascade(self, index, cnf={}, **kw):
        self.insert(index, "cascade", cnf, **kw)

    def insert_checkbutton(self, index, cnf={}, **kw):
        self.insert(index, "checkbutton", cnf, **kw)

    def insert_radiobutton(self, index, cnf={}, **kw):
        self.insert(index, "radiobutton", cnf, **kw)

    def insert_separator(self, index, cnf={}, **kw):
        self.insert(index, "separator", cnf, **kw)

    def delete(self, index1, index2=None):
        a = self.index(index1)
        b = a if index2 is None else self.index(index2)
        if a is None:
            return
        if b is None:
            b = a
        del self._itens[a:b + 1]
        self._mudou()

    def index(self, index):
        if isinstance(index, int):
            return index
        s = str(index)
        if s in ("end", "last"):
            return len(self._itens) - 1 if self._itens else None
        if s == "none":
            return None
        if s == "active":
            return None
        try:
            return int(s)
        except ValueError:
            pass
        if s.startswith("@"):
            return None
        for i, it in enumerate(self._itens):
            if it.get("label") == s:
                return i
        for i, it in enumerate(self._itens):   # como o Tk: padrão glob simples
            if it.get("label") and str(it["label"]).startswith(s.rstrip("*")) and s.endswith("*"):
                return i
        raise TclError('bad menu entry index "%s"' % index)

    def entryconfigure(self, index, cnf=None, **kw):
        it = self._itens[self.index(index)]
        if cnf:
            kw = dict(cnf, **kw)
        if not kw:
            return {k: (k, "", "", "", v) for k, v in it.items()}
        for k, v in kw.items():
            k = k.rstrip("_")
            if k not in it:
                raise TclError('unknown option "-%s"' % k)
            it[k] = v
        self._mudou()

    entryconfig = entryconfigure

    def entrycget(self, index, option):
        return self._itens[self.index(index)].get(option.rstrip("_"), "")

    def type(self, index):
        return self._itens[self.index(index)]["type"]

    def invoke(self, index):
        i = self.index(index)
        if i is None:
            return None
        it = self._itens[i]
        if it.get("state") == "disabled":
            return None
        t = it["type"]
        var = it.get("variable")
        if t == "checkbutton" and isinstance(var, Variable):
            on = _igual(var._valor, it["onvalue"])
            var.set(it["offvalue"] if on else it["onvalue"])
        elif t == "radiobutton" and isinstance(var, Variable):
            var.set(it["value"])
        c = it.get("command")
        if c:
            return c()
        return None

    def yposition(self, index):
        return self.index(index) * LETRA_A

    def xposition(self, index):
        return 0

    def activate(self, index):
        pass

    def unpost(self):
        pass

    def _mudou(self):
        if self._barra_de is not None and not self._barra_de._destruido:
            self._barra_de._algum_sujo = True

    def _relayout(self):
        pass

    def _redesenhar(self, nivel=2):
        self._mudou()

    # ---- o menu do sistema
    def _montar(self, linhas, mapa):
        """Itens no formato do módulo sagui; mapa[i] = (menu, índice) de cada
        linha, contando as linhas como o kernel (">" e "<" inclusive)."""
        pc = self._op.get("postcommand")
        if pc:
            try:
                pc()
            except Exception as e:
                _relatar(e)
        for i, it in enumerate(self._itens):
            t = it["type"]
            if t == "tearoff":
                continue
            if t == "separator":
                linhas.append("-")
                mapa.append(None)
                continue
            texto = str(it.get("label") or "")
            if it.get("accelerator"):
                texto += "\t" + str(it["accelerator"])
            var = it.get("variable")
            if t == "checkbutton" and isinstance(var, Variable) and _igual(var._valor, it["onvalue"]):
                texto = "*" + texto
            elif t == "radiobutton" and isinstance(var, Variable) and _igual(var._valor, it["value"]):
                texto = "*" + texto
            if it.get("state") == "disabled":
                texto = "~" + texto
            if t == "cascade" and isinstance(it.get("menu"), Menu) and it.get("state") != "disabled":
                sub = []
                mapa.append(None)
                it["menu"]._montar(sub, mapa)
                mapa.append(None)
                linhas.append((texto, sub))
            else:
                linhas.append(texto)
                mapa.append((self, i))

    def tk_popup(self, x, y, entry=""):
        self.post(x, y)

    def post(self, x, y):
        top = self.master._top() if self.master is not None else None
        if self._barra_de is not None:
            top = self._barra_de
        if top is None or top._v is None:
            return
        self._abrir_em(top, int(x) - top._ax, int(y) - top._ay)

    def _abrir_em(self, top, x, y):
        linhas, mapa = [], []
        self._montar(linhas, mapa)
        if not linhas:
            return
        top._atualizar_tela()
        r = top._v.menu_contextual(x, y, linhas)
        if r is None or r >= len(mapa) or mapa[r] is None:
            return
        menu, i = mapa[r]
        try:
            menu.invoke(i)
        except Exception as e:
            _relatar(e)

    # ---- barra de menus (Tk/Toplevel config(menu=...))
    def _desenhar_barra(self, v, largura):
        v.rect(0, 0, largura, ALTURA_BARRA, BOTAO)
        v.rect(0, ALTURA_BARRA - 1, largura, 1, BORDA)
        x = 4
        self._posicoes = []
        for i, it in enumerate(self._itens):
            if it["type"] in ("tearoff", "separator"):
                continue
            t = str(it.get("label") or "")
            w = len(t) * LETRA_L + 16
            cor = FRACO if it.get("state") == "disabled" else TEXTO
            v.texto(x + 8, (ALTURA_BARRA - LETRA_A) // 2, t, cor)
            self._posicoes.append((x, x + w, i))
            x += w

    def _clique_barra(self, x, y):
        top = self._barra_de
        for x0, x1, i in self._posicoes:
            if x0 <= x < x1:
                it = self._itens[i]
                if it.get("state") == "disabled":
                    return
                if it["type"] == "cascade" and isinstance(it.get("menu"), Menu):
                    v = top._v
                    v.rect(x0, 0, x1 - x0, ALTURA_BARRA - 1, SELECAO)
                    v.texto(x0 + 8, (ALTURA_BARRA - LETRA_A) // 2, str(it.get("label") or ""), BRANCO)
                    v.actualizar()
                    it["menu"]._abrir_em(top, x0, ALTURA_BARRA)
                    top._algum_sujo = True
                    top._sujo = max(top._sujo, 1)
                else:
                    self.invoke(i)
                return


class Menubutton(Button):
    _classe = "Menubutton"
    _padrao = dict(Button._padrao, menu=None, direction="below", indicatoron=0, relief="raised")

    def invoke(self):
        m = self._op.get("menu")
        if isinstance(m, Menu) and self._ativo():
            top = self._top()
            m._abrir_em(top, self._ax, self._ay + self._wa)
            return None
        return None


class OptionMenu(Menubutton):
    """OptionMenu(master, variable, value, *values, command=None)"""

    def __init__(self, master, variable, value, *values, **kwargs):
        cmd = kwargs.pop("command", None)
        Menubutton.__init__(self, master, textvariable=variable, indicatoron=1, anchor="w",
                            relief="raised", padx=6)
        self._variavel = variable
        menu = Menu(self, tearoff=0)
        self._menu = menu
        self["menu"] = menu
        for v in (value,) + values:
            def escolher(v=v):
                variable.set(v)
                if cmd:
                    cmd(v)
            menu.add_radiobutton(label=str(v), variable=variable, value=v, command=escolher)
        if variable._valor in ("", None):
            variable.set(value)

    def _natural(self):
        l, a = Menubutton._natural(self)
        return l + 18, a

    def _desenhar(self, v):
        Menubutton._desenhar(self, v)
        x = self._ax + self._wl - 16
        y = self._ay + self._wa // 2
        v.poligono([(x, y - 2), (x + 8, y - 2), (x + 4, y + 3)], TEXTO)

    def __getitem__(self, name):
        if name == "menu":
            return self._menu
        return Menubutton.__getitem__(self, name)

    def destroy(self):
        Menubutton.destroy(self)
        self._menu = None
