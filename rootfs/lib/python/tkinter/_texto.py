# SPDX-License-Identifier: GPL-2.0-or-later
# tkinter/_texto.py - o componente Text, básico (mejora 033, D084).
#
# Texto com várias linhas: insert, get, delete, índices "linha.coluna" com
# os modificadores do Tk (+5c, -1l, linestart, lineend, wordstart...),
# marcas, a seleção (tag "sel"), rolagem (yview e yscrollcommand), quebra
# de linhas (wrap) e edição com o teclado e o mouse. As outras tags são
# guardadas mas não mudam o desenho.
from tkinter import (Widget, YView, XView, TclError, Event, _int, _cor, _relevo, _recorte_dentro, _foco, _moldura_campo,
                     BRANCO, TEXTO, FRACO, SELECAO, DESTAQUE, BOTAO, LETRA_L, LETRA_A, ESTADO_SHIFT,
                     ESTADO_CONTROL, ESTADO_B1)
import sagui


class Text(Widget, XView, YView):
    _classe = "Text"
    _foca_no_clique = True
    _padrao = {"background": BRANCO, "foreground": TEXTO, "borderwidth": 2, "relief": "sunken",
               "width": 80, "height": 24, "wrap": "char", "state": "normal", "font": None,
               "padx": 2, "pady": 2, "yscrollcommand": None, "xscrollcommand": None,
               "insertbackground": TEXTO, "selectbackground": SELECAO, "selectforeground": BRANCO,
               "highlightthickness": 0, "highlightbackground": "", "highlightcolor": "",
               "spacing1": 0, "spacing2": 0, "spacing3": 0, "tabs": "", "undo": 0, "maxundo": 0,
               "autoseparators": 1, "exportselection": 1, "setgrid": 0, "cursor": "",
               "takefocus": "", "insertwidth": 2, "insertontime": 600, "insertofftime": 300,
               "inactiveselectbackground": SELECAO, "blockcursor": 0, "insertunfocussed": "none",
               "startline": "", "endline": "", "tabstyle": "tabular", "insertborderwidth": 0,
               "selectborderwidth": 0}

    def _iniciar(self):
        self._l = [""]                 # as linhas, sem o '\n'
        self._marcas = {"insert": (1, 0), "current": (1, 0)}
        self._sel = None               # ((l, c), (l, c))
        self._ancora = (1, 0)
        self._topo = 0                 # primeira linha lógica visível (0 = linha 1)
        self._tags = {}
        self._modificado = False
        self._segmentos = []           # [(y, linha, col_inicial, n)] do último desenho
        self._realce = None            # D297: f(linea) -> lista de colores, uno por carácter (resaltado)
        self._fondo_linea = {}         # D297: {n de línea: color de fondo} (la línea del error)
        self._cache_realce = {}

    def _foca_tab(self):
        return True

    def _tab_navega(self):
        return False

    def _natural(self):
        b = _int(self._op.get("borderwidth", 2))
        px, py = _int(self._op.get("padx", 2)), _int(self._op.get("pady", 2))
        return (_int(self._op.get("width", 80)) * LETRA_L + 2 * (b + px),
                _int(self._op.get("height", 24)) * LETRA_A + 2 * (b + py))

    # ---- índices
    def _fixar(self, l, c):
        if l < 1:
            return 1, 0
        if l > len(self._l):
            return len(self._l), len(self._l[-1])   # "end": depois do último caractere
        return l, max(0, min(c, len(self._l[l - 1])))

    def _deslocar(self, pos, n):
        """Anda n caracteres (os '\\n' contam)."""
        l, c = pos
        if n > 0:
            while n > 0:
                resto = len(self._l[l - 1]) - c
                if n <= resto:
                    return l, c + n
                if l >= len(self._l):
                    return l, len(self._l[l - 1])
                n -= resto + 1
                l, c = l + 1, 0
            return l, c
        n = -n
        while n > 0:
            if n <= c:
                return l, c - n
            if l <= 1:
                return 1, 0
            n -= c + 1
            l = l - 1
            c = len(self._l[l - 1])
        return l, c

    def _base(self, s):
        if s.startswith("@"):
            x, y = s[1:].split(",")
            return self._pos_xy(int(x) + self._ax, int(y) + self._ay)
        if s == "end":
            return len(self._l) + 1, 0
        if s in self._marcas:
            return self._marcas[s]
        if s in ("sel.first", "sel.last"):
            if not self._sel:
                raise TclError('text doesn\'t contain any characters tagged with "sel"')
            return self._sel[0] if s == "sel.first" else self._sel[1]
        if "." in s:
            a, b = s.split(".", 1)
            l = int(a)
            if b == "end":
                l2 = max(1, min(l, len(self._l)))
                return l, len(self._l[l2 - 1])
            return l, int(b)
        if s.endswith(".first") or s.endswith(".last"):
            raise TclError('bad text index "%s"' % s)
        raise TclError('bad text index "%s"' % s)

    def _indice(self, index):
        """Índice do Tk -> (linha, coluna) dentro do texto."""
        if isinstance(index, tuple):
            return self._fixar(*index)
        if isinstance(index, float):
            index = repr(index)
        s = str(index).strip()
        # a base vai até o primeiro modificador (+, - ou espaço)
        i = 0
        if s.startswith("@"):
            while i < len(s) and s[i] not in " +-" or (i > 0 and s[i] == "-" and s[i - 1] == ","):
                i += 1
        else:
            while i < len(s) and s[i] not in " +-":
                i += 1
        base = s[:i]
        if base.startswith("@") and "," not in base:
            raise TclError('bad text index "%s"' % index)
        l, c = self._base(base)
        fim_texto = l > len(self._l)
        l, c = self._fixar(l, c)
        resto = s[i:].strip()
        while resto:
            if resto[0] in "+-":
                sinal = 1 if resto[0] == "+" else -1
                resto = resto[1:].strip()
                j = 0
                while j < len(resto) and resto[j].isdigit():
                    j += 1
                n = int(resto[:j] or "0") * sinal
                resto = resto[j:].strip()
                k = 0
                while k < len(resto) and resto[k].isalpha():
                    k += 1
                unidade = resto[:k]
                resto = resto[k:].strip()
                if unidade.startswith("l"):
                    l, c = self._fixar(l + n, c)
                else:
                    if fim_texto and n < 0:
                        # end-1c: o último caractere antes do '\n' final
                        l, c = len(self._l), len(self._l[-1])
                        n += 1
                    l, c = self._deslocar((l, c), n)
                fim_texto = False
            else:
                k = 0
                while k < len(resto) and (resto[k].isalpha() or resto[k] == "_"):
                    k += 1
                palavra = resto[:k]
                resto = resto[k:].strip()
                linha = self._l[l - 1]
                if palavra == "linestart":
                    c = 0
                elif palavra == "lineend":
                    c = len(linha)
                elif palavra == "wordstart":
                    while c > 0 and (linha[c - 1].isalpha() or linha[c - 1].isdigit() or linha[c - 1] == "_"):
                        c -= 1
                elif palavra == "wordend":
                    while c < len(linha) and (linha[c].isalpha() or linha[c].isdigit() or linha[c] == "_"):
                        c += 1
                else:
                    raise TclError('bad text index "%s"' % index)
        return l, c

    def _fim(self):
        return len(self._l), len(self._l[-1])

    def index(self, index):
        l, c = self._indice(index)
        if str(index) == "end":
            return "%d.0" % (len(self._l) + 1)
        return "%d.%d" % (l, c)

    def compare(self, index1, op, index2):
        a, b = self._indice(index1), self._indice(index2)
        return {"<": a < b, "<=": a <= b, "==": a == b, ">=": a >= b, ">": a > b, "!=": a != b}[op]

    # ---- conteúdo
    def get(self, index1, index2=None):
        a = self._indice(index1)
        if index2 is None:
            if a == self._fim():
                return "\n" if str(index1) != "end" else ""
            return self._texto_entre(a, self._deslocar(a, 1))
        fim_total = str(index2).strip() == "end"
        b = self._indice(index2)
        if b < a:
            return ""
        t = self._texto_entre(a, b)
        if fim_total or (b == self._fim() and self._passa_fim(index2)):
            t += "\n"
        return t

    def _passa_fim(self, index):
        s = str(index).replace(" ", "")
        return s.startswith("end") and not s.startswith("end-")

    def _texto_entre(self, a, b):
        (l1, c1), (l2, c2) = a, b
        if l1 == l2:
            return self._l[l1 - 1][c1:c2]
        partes = [self._l[l1 - 1][c1:]]
        for l in range(l1 + 1, l2):
            partes.append(self._l[l - 1])
        partes.append(self._l[l2 - 1][:c2])
        return "\n".join(partes)

    def insert(self, index, chars, *args):
        if self._op.get("state") == "disabled":
            return
        texto = str(chars)
        for k in range(1, len(args), 2):   # insert(i, "a", tags, "b", tags...)
            texto += str(args[k])
        if not texto:
            return
        l, c = self._indice(index)
        self._inserir((l, c), texto)

    def _inserir(self, pos, texto):
        l, c = pos
        linha = self._l[l - 1]
        partes = texto.replace("\r\n", "\n").split("\n")
        if len(partes) == 1:
            self._l[l - 1] = linha[:c] + texto + linha[c:]
            fim = (l, c + len(texto))
        else:
            novas = [linha[:c] + partes[0]] + partes[1:-1] + [partes[-1] + linha[c:]]
            self._l[l - 1:l] = novas
            fim = (l + len(partes) - 1, len(partes[-1]))
        n = len(texto)
        for nome, p in list(self._marcas.items()):
            if p > pos or (p == pos and nome == "insert"):
                if p[0] == l:
                    self._marcas[nome] = (fim[0], fim[1] + p[1] - c)
                else:
                    self._marcas[nome] = (p[0] + len(partes) - 1, p[1])
        self._sel = None
        self._mudou_texto()
        return fim

    def delete(self, index1, index2=None):
        if self._op.get("state") == "disabled":
            return
        a = self._indice(index1)
        b = self._deslocar(a, 1) if index2 is None else self._indice(index2)
        if b <= a:
            return
        self._apagar(a, b)

    def _apagar(self, a, b):
        (l1, c1), (l2, c2) = a, b
        self._l[l1 - 1:l2] = [self._l[l1 - 1][:c1] + self._l[l2 - 1][c2:]]
        for nome, p in list(self._marcas.items()):
            if p >= b:
                if p[0] == l2:
                    self._marcas[nome] = (l1, c1 + p[1] - c2)
                else:
                    self._marcas[nome] = (p[0] - (l2 - l1), p[1])
            elif p > a:
                self._marcas[nome] = a
        self._sel = None
        self._mudou_texto()

    def replace(self, index1, index2, chars, *args):
        self.delete(index1, index2)
        self.insert(index1, chars, *args)

    def _mudou_texto(self):
        self._modificado = True
        self._topo = max(0, min(self._topo, len(self._l) - 1))
        self._redesenhar()
        self._avisar_rolagem()
        self.event_generate("<<Modified>>")

    def edit_modified(self, arg=None):
        if arg is None:
            return self._modificado
        self._modificado = bool(arg)

    def edit_reset(self):
        pass

    def edit_separator(self):
        pass

    def edit_undo(self):
        raise TclError("nothing to undo")

    def edit_redo(self):
        raise TclError("nothing to redo")

    def count(self, index1, index2, *args):
        return (len(self.get(index1, index2)),)

    # ---- marcas e seleção
    def mark_set(self, markName, index):
        self._marcas[markName] = self._indice(index)
        if markName == "insert":
            self._redesenhar()

    def mark_unset(self, *markNames):
        for m in markNames:
            if m not in ("insert", "current"):
                self._marcas.pop(m, None)

    def mark_names(self):
        return tuple(self._marcas.keys())

    def mark_gravity(self, markName, direction=None):
        return "right"

    def tag_add(self, tagName, index1, *args):
        if tagName == "sel":
            a = self._indice(index1)
            b = self._deslocar(a, 1) if not args else self._indice(args[0])
            self._sel = (a, b) if a < b else None
            self._redesenhar()
            return
        self._tags.setdefault(tagName, {"ranges": [], "op": {}, "binds": {}})["ranges"].append(
            (self.index(index1), self.index(args[0]) if args else None))

    def tag_remove(self, tagName, index1, index2=None):
        if tagName == "sel":
            self._sel = None
            self._redesenhar()
        elif tagName in self._tags:
            self._tags[tagName]["ranges"] = []

    def tag_delete(self, *tagNames):
        for t in tagNames:
            self._tags.pop(t, None)

    def tag_configure(self, tagName, cnf=None, **kw):
        t = self._tags.setdefault(tagName, {"ranges": [], "op": {}, "binds": {}})
        if cnf:
            kw = dict(cnf, **kw)
        t["op"].update(kw)
        return None

    tag_config = tag_configure

    def tag_cget(self, tagName, option):
        return self._tags.get(tagName, {"op": {}})["op"].get(option, "")

    def tag_names(self, index=None):
        return ("sel",) + tuple(self._tags.keys())

    def tag_ranges(self, tagName):
        if tagName == "sel":
            if not self._sel:
                return ()
            return ("%d.%d" % self._sel[0], "%d.%d" % self._sel[1])
        r = []
        for a, b in self._tags.get(tagName, {"ranges": []})["ranges"]:
            r.extend([a, b or a])
        return tuple(r)

    def tag_bind(self, tagName, sequence, func, add=None):
        return ""

    def tag_unbind(self, tagName, sequence, funcid=None):
        pass

    def tag_raise(self, tagName, aboveThis=None):
        pass

    def tag_lower(self, tagName, belowThis=None):
        pass

    def tag_nextrange(self, tagName, index1, index2=None):
        return self.tag_ranges(tagName)[:2]

    def search(self, pattern, index, stopindex=None, forwards=None, backwards=None, exact=None,
               regexp=None, nocase=None, count=None, elide=None):
        texto = self.get("1.0", "end")
        ini = self._para_offset(self._indice(index))
        p = pattern.lower() if nocase else pattern
        t = texto.lower() if nocase else texto
        if backwards:
            k = t.rfind(p, 0, ini)
        else:
            k = t.find(p, ini)
            if k < 0:
                k = t.find(p, 0)
        if k < 0:
            return ""
        if count is not None:
            count.set(len(pattern))
        return self.index("1.0+%dc" % k)

    def _para_offset(self, pos):
        l, c = pos
        return sum(len(x) + 1 for x in self._l[:l - 1]) + c

    # ---- rolagem
    def _visiveis(self):
        b = _int(self._op.get("borderwidth", 2))
        py = _int(self._op.get("pady", 2))
        return max(1, (self._wa - 2 * (b + py)) // LETRA_A) if self._wa else _int(self._op.get("height", 24))

    def _yview(self, *args):
        n = len(self._l)
        if not args:
            return (self._topo / n, min(1.0, (self._topo + self._visiveis()) / n))
        if args[0] == "moveto":
            self._topo = int(float(args[1]) * n)
        elif args[0] == "scroll":
            k = int(args[1])
            self._topo += k * (self._visiveis() if str(args[2]).startswith("page") else 1)
        else:
            self._topo = self._indice(args[0])[0] - 1
        self._topo = max(0, min(self._topo, n - 1))
        self._redesenhar()
        self._avisar_rolagem()

    def see(self, index):
        l, c = self._indice(index)
        vis = self._visiveis()
        if l - 1 < self._topo:
            self._topo = l - 1
        elif l - 1 >= self._topo + vis:
            self._topo = l - vis
        self._topo = max(0, self._topo)
        self._redesenhar()
        self._avisar_rolagem()

    def _avisar_rolagem(self):
        cmd = self._op.get("yscrollcommand")
        if cmd:
            a, b = self._yview()
            try:
                cmd(a, b)
            except Exception as e:
                from tkinter import _relatar
                _relatar(e)

    def _colocar(self, x, y, w, h):
        Widget._colocar(self, x, y, w, h)
        self.after_idle(self._avisar_rolagem)

    def bbox(self, index):
        l, c = self._indice(index)
        for y, li, c0, n in self._segmentos:
            if li == l and c0 <= c <= c0 + n:
                return (self._x_texto() - self._ax + (c - c0) * LETRA_L, y - self._ay, LETRA_L, LETRA_A)
        return None

    def dlineinfo(self, index):
        b = self.bbox(index)
        return None if b is None else (b[0], b[1], self._wl, LETRA_A, 12)

    # ---- desenho
    def _x_texto(self):
        return self._ax + _int(self._op.get("borderwidth", 2)) + _int(self._op.get("padx", 2))

    def _cabem(self):
        b = _int(self._op.get("borderwidth", 2))
        px = _int(self._op.get("padx", 2))
        return max(1, (self._wl - 2 * (b + px)) // LETRA_L)

    def _pedacos(self, linha):
        """Partes de uma linha lógica conforme o wrap: [(col, texto)]."""
        cabem = self._cabem()
        modo = self._op.get("wrap", "char")
        if modo == "none" or len(linha) <= cabem:
            return [(0, linha)]
        r = []
        c = 0
        while c < len(linha):
            fim = c + cabem
            if modo == "word" and fim < len(linha):
                k = linha.rfind(" ", c, fim)
                if k > c:
                    fim = k + 1
            r.append((c, linha[c:fim]))
            c = fim
        return r

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        desativado = self._op.get("state") == "disabled"
        fundo = self._cor_op("background", BRANCO)
        frente = self._cor_op("foreground", TEXTO)
        v.rect(x, y, w, h, fundo)
        b = _int(self._op.get("borderwidth", 2))
        focado = _foco[0] is self
        _moldura_campo(v, x, y, w, h, self._op.get("relief", "sunken"), b, focado and not desativado)
        ant = v.recorte()
        _recorte_dentro(v, ant, x + b, y + b, w - 2 * b, h - 2 * b)
        tx = self._x_texto()
        ty = y + b + _int(self._op.get("pady", 2))
        fim_y = y + h - b
        sel = self._sel
        sb, sf = self._cor_op("selectbackground", SELECAO), self._cor_op("selectforeground", BRANCO)
        cur = self._marcas["insert"]
        self._segmentos = []
        l = self._topo + 1
        while l <= len(self._l) and ty < fim_y:
            linha = self._l[l - 1].replace("\t", " ")
            for c0, parte in self._pedacos(linha):
                if ty >= fim_y:
                    break
                fl = self._fondo_linea.get(l)
                if fl is not None:
                    v.rect(x + b, ty, w - 2 * b, LETRA_A, fl)
                self._segmentos.append((ty, l, c0, len(parte)))
                if sel and sel[0] <= (l, c0 + len(parte)) and (l, c0) <= sel[1]:
                    a = c0 if sel[0] < (l, c0) else sel[0][1]
                    z = c0 + len(parte) if sel[1] > (l, c0 + len(parte)) else sel[1][1]
                    v.texto(tx, ty, parte[:a - c0], frente)
                    larg_sel = max(z - a, 0) * LETRA_L + (LETRA_L // 2 if sel[1][0] > l and z == c0 + len(parte) else 0)
                    v.rect(tx + (a - c0) * LETRA_L, ty, larg_sel, LETRA_A, sb)
                    v.texto(tx + (a - c0) * LETRA_L, ty, parte[a - c0:z - c0], sf)
                    v.texto(tx + (z - c0) * LETRA_L, ty, parte[z - c0:], frente)
                elif self._realce is not None:
                    self._texto_realce(v, tx, ty, linha, c0, parte, frente)
                else:
                    v.texto(tx, ty, parte, frente)
                if focado and not desativado and cur[0] == l and c0 <= cur[1] <= c0 + len(parte):
                    ultimo = c0 + len(parte) == len(linha)
                    if cur[1] < c0 + len(parte) or ultimo:
                        v.rect(tx + (cur[1] - c0) * LETRA_L - 1, ty, 2, LETRA_A, self._cor_op("insertbackground", TEXTO))
                ty += LETRA_A
            l += 1
        v.recorte(*ant)

    def _texto_realce(self, v, tx, ty, linha, c0, parte, frente):
        """Dibuja `parte` (que empieza en la columna c0 de `linha`) con los colores que da _realce."""
        cols = self._cache_realce.get(linha)
        if cols is None:
            if len(self._cache_realce) > 400:
                self._cache_realce = {}
            try:
                cols = self._realce(linha)
            except Exception:
                cols = None
            if not cols or len(cols) < len(linha):
                cols = [frente] * len(linha)
            self._cache_realce[linha] = cols
        i = 0
        n = len(parte)
        while i < n:
            c = cols[c0 + i]
            j = i + 1
            while j < n and cols[c0 + j] == c:
                j += 1
            v.texto(tx + i * LETRA_L, ty, parte[i:j], c)
            i = j

    def _pos_xy(self, xa, ya):
        """Posição (linha, coluna) no ponto (x, y) da janela."""
        if not self._segmentos:
            return self._fim()
        seg = self._segmentos[-1]
        for s in self._segmentos:
            if ya < s[0] + LETRA_A:
                seg = s
                break
        y, l, c0, n = seg
        col = (xa - self._x_texto() + LETRA_L // 2) // LETRA_L
        col = max(0, min(col, n))
        if col == n and c0 + n < len(self._l[l - 1]):
            col = n - 1 if n else 0
        return l, c0 + col

    # ---- eventos
    def _selecionar_ate(self, pos):
        a, b = min(self._ancora, pos), max(self._ancora, pos)
        self._sel = (a, b) if a < b else None

    def _tratar(self, tipo, ev):
        desativado = self._op.get("state") == "disabled"
        if tipo == "ButtonPress" and ev.num == 1:
            pos = self._pos_xy(ev.x + self._ax, ev.y + self._ay)
            if ev.state & ESTADO_SHIFT:
                self._selecionar_ate(pos)
            elif getattr(ev, "_duplo", False):   # duplo clique: a palavra
                a = self._indice("%d.%d wordstart" % pos)
                b = self._indice("%d.%d wordend" % pos)
                self._sel = (a, b) if a < b else None
                self._ancora = a
                pos = b
            else:
                self._ancora = pos
                self._sel = None
            self._marcas["insert"] = pos
            self._redesenhar()
        elif tipo == "Motion":
            pos = self._pos_xy(ev.x + self._ax, ev.y + self._ay)
            self._marcas["current"] = pos
            if ev.state & ESTADO_B1:
                self._marcas["insert"] = pos
                self._selecionar_ate(pos)
                self._redesenhar()
        elif tipo == "MouseWheel":
            self._yview("scroll", -3 if ev.delta > 0 else 3, "units")
            return "break"
        elif tipo == "KeyPress":
            return self._tecla(ev, desativado)
        return None

    def _usa_roda(self):
        return True

    def _tecla(self, ev, desativado):
        k = ev.keysym
        ctrl = ev.state & ESTADO_CONTROL
        shift = ev.state & ESTADO_SHIFT
        cur = self._marcas["insert"]
        l, c = cur
        novo = None
        if k == "Left":
            novo = self._deslocar(cur, -1)
        elif k == "Right":
            novo = self._deslocar(cur, 1)
        elif k == "Up":
            novo = self._fixar(l - 1, c) if l > 1 else (1, 0)
        elif k == "Down":
            novo = self._fixar(l + 1, c) if l < len(self._l) else self._fim()
        elif k == "Home":
            novo = (1, 0) if ctrl else (l, 0)
        elif k == "End":
            novo = self._fim() if ctrl else (l, len(self._l[l - 1]))
        elif k == "Prior":
            novo = self._fixar(l - self._visiveis(), c)
        elif k == "Next":
            novo = self._fixar(l + self._visiveis(), c)
        if novo is not None:
            if shift:
                if not self._sel:
                    self._ancora = cur
                self._selecionar_ate(novo)
            else:
                self._sel = None
            self._marcas["insert"] = novo
            self.see("insert")
            return None
        if ctrl and k in ("a", "slash"):
            self._ancora = (1, 0)
            self._sel = ((1, 0), self._fim()) if self._fim() != (1, 0) else None
            self._redesenhar()
            return "break"
        if ctrl and k == "c":
            if self._sel:
                sagui.portapapeles(self._texto_entre(*self._sel))
            return "break"
        if desativado:
            return None
        if ctrl and k == "x":
            if self._sel:
                sagui.portapapeles(self._texto_entre(*self._sel))
                self._apagar(*self._sel)
                self.see("insert")
            return "break"
        if ctrl and k == "v":
            t = sagui.portapapeles()
            if t:
                self._substituir_sel(t)
            return "break"
        if k == "BackSpace":
            if self._sel:
                self._apagar(*self._sel)
            elif cur > (1, 0):
                self._apagar(self._deslocar(cur, -1), cur)
            self.see("insert")
            return None
        if k == "Delete":
            if self._sel:
                self._apagar(*self._sel)
            elif cur < self._fim():
                self._apagar(cur, self._deslocar(cur, 1))
            self.see("insert")
            return None
        if k == "Return":
            self._substituir_sel("\n")
            return None
        if k == "Tab":
            self._substituir_sel("\t")
            return "break"
        ch = ev.char
        if ch and not ctrl and len(ch) == 1 and ch >= " " and ch != "\x7f":
            self._substituir_sel(ch)
        return None

    def _substituir_sel(self, t):
        if self._sel:
            self._apagar(*self._sel)
        self._inserir(self._marcas["insert"], t)
        self.see("insert")

    def _foco_mudou(self, ganhou):
        self._redesenhar()

    # ---- coisas que não existem aqui
    def window_create(self, index, cnf={}, **kw):
        raise TclError("Text.window_create no está disponible en SaguiOS")

    def image_create(self, index, cnf={}, **kw):
        raise TclError("Text.image_create no está disponible en SaguiOS")
