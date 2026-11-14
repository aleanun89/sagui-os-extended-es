# SPDX-License-Identifier: GPL-2.0-or-later
# tkinter/_tela.py - o Canvas (mejora 033, D084).
#
# Itens: line, rectangle, oval, polygon, arc, text, image e window, com
# tags, coords, move, itemconfig, delete, tag_bind, bbox e as buscas
# (find_*). Os itens são desenhados em C pelo módulo sagui. Quando só alguns
# itens mudam, o Canvas redesenha só a área que eles ocupavam e ocupam
# (o fundo e os itens que cruzam essa área), não o desenho todo.
import math
from tkinter import (Widget, XView, YView, TclError, Event, EventType, _int, _cor, _relevo, _quebrar,
                     _recorte_dentro, _imagens, _chamar, _analisar, _casa, _relatar, FUNDO, TEXTO,
                     LETRA_L, LETRA_A, ESTADO_B1)

_PADRAO_ITEM = {
    "line": {"fill": "black", "width": 1, "arrow": "none", "arrowshape": (8, 10, 3), "dash": "",
             "capstyle": "butt", "joinstyle": "round", "smooth": 0, "splinesteps": 12,
             "activefill": "", "disabledfill": "", "activewidth": 0},
    "rectangle": {"fill": "", "outline": "black", "width": 1, "dash": "", "activefill": "",
                  "activeoutline": "", "disabledfill": "", "activewidth": 0},
    "oval": {"fill": "", "outline": "black", "width": 1, "dash": "", "activefill": "",
             "activeoutline": "", "disabledfill": "", "activewidth": 0},
    "polygon": {"fill": "black", "outline": "", "width": 1, "smooth": 0, "dash": "", "activefill": "",
                "activeoutline": "", "joinstyle": "round", "splinesteps": 12, "activewidth": 0},
    "arc": {"fill": "", "outline": "black", "width": 1, "start": 0.0, "extent": 90.0, "style": "pieslice",
            "dash": "", "activefill": "", "activeoutline": "", "activewidth": 0},
    "text": {"text": "", "fill": "black", "anchor": "center", "font": None, "justify": "left", "width": 0,
             "angle": 0, "activefill": "", "underline": -1},
    "image": {"image": "", "anchor": "center", "activeimage": "", "disabledimage": ""},
    "window": {"window": None, "anchor": "center", "width": 0, "height": 0},
}
_COMUNS = {"tags": (), "state": "normal"}


def _achatar(args):
    """create_line(1, 2, 3, 4), ((1, 2), (3, 4)) ou [1, 2, 3, 4] -> [1.0, 2.0, ...]"""
    r = []
    for a in args:
        if isinstance(a, (list, tuple)):
            r.extend(_achatar(a))
        else:
            r.append(float(a))
    return r


class Canvas(Widget, XView, YView):
    _classe = "Canvas"
    _padrao = {"background": FUNDO, "width": 378, "height": 265, "borderwidth": 0, "relief": "flat",
               "highlightthickness": 0, "highlightbackground": "", "highlightcolor": "",
               "scrollregion": "", "xscrollcommand": None, "yscrollcommand": None,
               "xscrollincrement": 0, "yscrollincrement": 0, "confine": 1, "cursor": "",
               "takefocus": "", "closeenough": 1.0, "selectbackground": "", "selectforeground": "",
               "insertbackground": "", "insertwidth": 2, "state": "normal", "offset": "0,0",
               "selectborderwidth": 0, "insertborderwidth": 0, "insertofftime": 300, "insertontime": 600}

    def _iniciar(self):
        self._itens = {}
        self._ordem = []
        self._prox = 1
        self._zona = None
        self._binds_tag = {}        # tag ou id -> {seq: (padrão, [funcs])}
        self._atual = None          # item debaixo do mouse ("current")
        self._agarrado_item = None
        self._ox = self._oy = 0     # rolagem (scrollregion)

    def _natural(self):
        b = _int(self._op.get("borderwidth", 0)) + _int(self._op.get("highlightthickness", 0))
        return _int(self._op.get("width", 378)) + 2 * b, _int(self._op.get("height", 265)) + 2 * b

    # ---- criação
    def _criar(self, tipo, args, kw):
        coords = _achatar(args)
        op = dict(_PADRAO_ITEM[tipo])
        op.update(_COMUNS)
        for k, v in kw.items():
            k = k.rstrip("_")
            if k not in op:
                raise TclError('unknown option "-%s"' % k)
            op[k] = v
        op["tags"] = self._tags_de(op["tags"])
        i = self._prox
        self._prox += 1
        item = {"tipo": tipo, "c": coords, "op": op}
        self._itens[i] = item
        self._ordem.append(i)
        if tipo == "window" and op.get("window") is not None:
            self._colocar_janela(item)
        self._sujar_item(i)
        return i

    def _tags_de(self, t):
        if t is None or t == "":
            return []
        if isinstance(t, str):
            return t.split()
        return [str(x) for x in t]

    def create_line(self, *args, **kw):
        return self._criar("line", args, kw)

    def create_rectangle(self, *args, **kw):
        return self._criar("rectangle", args, kw)

    def create_oval(self, *args, **kw):
        return self._criar("oval", args, kw)

    def create_polygon(self, *args, **kw):
        return self._criar("polygon", args, kw)

    def create_arc(self, *args, **kw):
        return self._criar("arc", args, kw)

    def create_text(self, *args, **kw):
        return self._criar("text", args, kw)

    def create_image(self, *args, **kw):
        return self._criar("image", args, kw)

    def create_window(self, *args, **kw):
        return self._criar("window", args, kw)

    def create_bitmap(self, *args, **kw):
        raise TclError("create_bitmap no está disponible en SaguiOS: usa create_image")

    def _colocar_janela(self, item):
        w = item["op"]["window"]
        x, y = item["c"][0], item["c"][1]
        kw = {"in_": self, "x": int(x) - self._ox, "y": int(y) - self._oy, "anchor": item["op"]["anchor"]}
        if item["op"]["width"]:
            kw["width"] = item["op"]["width"]
        if item["op"]["height"]:
            kw["height"] = item["op"]["height"]
        w.place(**kw)

    # ---- quais itens
    def _ids(self, tagOrId):
        if tagOrId is None:
            return []
        if isinstance(tagOrId, int):
            return [tagOrId] if tagOrId in self._itens else []
        s = str(tagOrId)
        if s.isdigit():
            i = int(s)
            return [i] if i in self._itens else []
        if s == "all":
            return list(self._ordem)
        if s == "current":
            return [self._atual] if self._atual in self._itens else []
        return [i for i in self._ordem if s in self._itens[i]["op"]["tags"]]

    def find_all(self):
        return tuple(self._ordem)

    def find_withtag(self, tagOrId):
        return tuple(self._ids(tagOrId))

    def find_overlapping(self, x1, y1, x2, y2):
        r = []
        for i in self._ordem:
            b = self._bbox_item(i)
            if b and b[0] <= x2 and b[2] >= x1 and b[1] <= y2 and b[3] >= y1 and \
                    self._itens[i]["op"]["state"] != "hidden":
                r.append(i)
        return tuple(r)

    def find_enclosed(self, x1, y1, x2, y2):
        r = []
        for i in self._ordem:
            b = self._bbox_item(i)
            if b and b[0] >= x1 and b[2] <= x2 and b[1] >= y1 and b[3] <= y2:
                r.append(i)
        return tuple(r)

    def find_closest(self, x, y, halo=None, start=None):
        melhor, dist = None, None
        for i in reversed(self._ordem):
            if self._itens[i]["op"]["state"] == "hidden":
                continue
            d = self._distancia(i, x, y)
            if d is not None and (dist is None or d < dist):
                melhor, dist = i, d
                if d == 0:
                    break
        return (melhor,) if melhor is not None else ()

    def find_above(self, tagOrId):
        ids = self._ids(tagOrId)
        if not ids:
            return ()
        k = self._ordem.index(ids[-1])
        return (self._ordem[k + 1],) if k + 1 < len(self._ordem) else ()

    def find_below(self, tagOrId):
        ids = self._ids(tagOrId)
        if not ids:
            return ()
        k = self._ordem.index(ids[0])
        return (self._ordem[k - 1],) if k > 0 else ()

    def gettags(self, tagOrId):
        ids = self._ids(tagOrId)
        return tuple(self._itens[ids[0]]["op"]["tags"]) if ids else ()

    def type(self, tagOrId):
        ids = self._ids(tagOrId)
        return self._itens[ids[0]]["tipo"] if ids else None

    def addtag_withtag(self, newtag, tagOrId):
        for i in self._ids(tagOrId):
            if newtag not in self._itens[i]["op"]["tags"]:
                self._itens[i]["op"]["tags"].append(newtag)

    def addtag_all(self, newtag):
        self.addtag_withtag(newtag, "all")

    def addtag_overlapping(self, newtag, x1, y1, x2, y2):
        for i in self.find_overlapping(x1, y1, x2, y2):
            self.addtag_withtag(newtag, i)

    def addtag_enclosed(self, newtag, x1, y1, x2, y2):
        for i in self.find_enclosed(x1, y1, x2, y2):
            self.addtag_withtag(newtag, i)

    def addtag_closest(self, newtag, x, y, halo=None, start=None):
        for i in self.find_closest(x, y):
            self.addtag_withtag(newtag, i)

    def addtag(self, newtag, how, *args):
        getattr(self, "addtag_" + how)(newtag, *args)

    def dtag(self, tagOrId, tagToDelete=None):
        t = tagOrId if tagToDelete is None else tagToDelete
        for i in self._ids(tagOrId):
            tags = self._itens[i]["op"]["tags"]
            if t in tags:
                tags.remove(t)

    # ---- mudanças
    def coords(self, tagOrId, *args):
        ids = self._ids(tagOrId)
        if not ids:
            return []
        if not args:
            return list(self._itens[ids[0]]["c"])
        novas = _achatar(args)
        i = ids[0]
        self._sujar_item(i)
        self._itens[i]["c"] = novas
        if self._itens[i]["tipo"] == "window":
            self._colocar_janela(self._itens[i])
        self._sujar_item(i)
        return None

    def move(self, tagOrId, dx, dy):
        dx, dy = float(dx), float(dy)
        for i in self._ids(tagOrId):
            self._sujar_item(i)
            c = self._itens[i]["c"]
            for k in range(0, len(c) - 1, 2):
                c[k] += dx
                c[k + 1] += dy
            if self._itens[i]["tipo"] == "window":
                self._colocar_janela(self._itens[i])
            self._sujar_item(i)

    def moveto(self, tagOrId, x="", y=""):
        ids = self._ids(tagOrId)
        if not ids:
            return
        b = self.bbox(*ids)
        dx = 0 if x == "" else float(x) - b[0]
        dy = 0 if y == "" else float(y) - b[1]
        self.move(tagOrId, dx, dy)

    def scale(self, tagOrId, xOrigin, yOrigin, xScale, yScale):
        x0, y0, sx, sy = float(xOrigin), float(yOrigin), float(xScale), float(yScale)
        for i in self._ids(tagOrId):
            self._sujar_item(i)
            c = self._itens[i]["c"]
            for k in range(0, len(c) - 1, 2):
                c[k] = x0 + (c[k] - x0) * sx
                c[k + 1] = y0 + (c[k + 1] - y0) * sy
            self._sujar_item(i)

    def itemconfigure(self, tagOrId, cnf=None, **kw):
        ids = self._ids(tagOrId)
        if cnf:
            kw = dict(cnf, **kw)
        if not ids:
            return None
        if not kw:
            return {k: (k, "", "", "", v) for k, v in self._itens[ids[0]]["op"].items()}
        for i in ids:
            op = self._itens[i]["op"]
            self._sujar_item(i)
            for k, v in kw.items():
                k = k.rstrip("_")
                if k not in op:
                    raise TclError('unknown option "-%s"' % k)
                op[k] = self._tags_de(v) if k == "tags" else v
            if self._itens[i]["tipo"] == "window":
                self._colocar_janela(self._itens[i])
            self._sujar_item(i)
        return None

    itemconfig = itemconfigure

    def itemcget(self, tagOrId, option):
        ids = self._ids(tagOrId)
        if not ids:
            return ""
        v = self._itens[ids[0]]["op"].get(option.rstrip("_"), "")
        return " ".join(v) if option == "tags" else v

    def delete(self, *args):
        for a in args:
            for i in self._ids(a):
                self._sujar_item(i)
                item = self._itens.pop(i)
                self._ordem.remove(i)
                if item["tipo"] == "window" and item["op"].get("window") is not None:
                    try:
                        item["op"]["window"].place_forget()
                    except Exception:
                        pass
                if self._atual == i:
                    self._atual = None
                if self._agarrado_item == i:
                    self._agarrado_item = None

    def tag_raise(self, tagOrId, aboveThis=None):
        ids = self._ids(tagOrId)
        for i in ids:
            self._ordem.remove(i)
        if aboveThis is None:
            self._ordem.extend(ids)
        else:
            ref = self._ids(aboveThis)
            k = self._ordem.index(ref[-1]) + 1 if ref else len(self._ordem)
            self._ordem[k:k] = ids
        for i in ids:
            self._sujar_item(i)

    lift = tkraise = tag_raise

    def tag_lower(self, tagOrId, belowThis=None):
        ids = self._ids(tagOrId)
        for i in ids:
            self._ordem.remove(i)
        if belowThis is None:
            self._ordem[0:0] = ids
        else:
            ref = self._ids(belowThis)
            k = self._ordem.index(ref[0]) if ref else 0
            self._ordem[k:k] = ids
        for i in ids:
            self._sujar_item(i)

    lower = tag_lower

    # itens de texto
    def insert(self, tagOrId, beforeThis, string):
        for i in self._ids(tagOrId):
            it = self._itens[i]
            if it["tipo"] == "text":
                t = str(it["op"]["text"])
                k = len(t) if beforeThis == "end" else int(beforeThis)
                self.itemconfigure(i, text=t[:k] + str(string) + t[k:])

    def dchars(self, tagOrId, first, last=None):
        for i in self._ids(tagOrId):
            it = self._itens[i]
            if it["tipo"] == "text":
                t = str(it["op"]["text"])
                a = len(t) if first == "end" else int(first)
                b = a if last is None else (len(t) - 1 if last == "end" else int(last))
                self.itemconfigure(i, text=t[:a] + t[b + 1:])

    def index(self, tagOrId, index):
        ids = self._ids(tagOrId)
        if not ids:
            return 0
        t = str(self._itens[ids[0]]["op"].get("text", ""))
        return len(t) if index == "end" else int(index)

    def focus(self, *args):
        return ""

    def icursor(self, *args):
        pass

    def select_clear(self):
        pass

    def select_item(self):
        return None

    def postscript(self, cnf={}, **kw):
        raise TclError("postscript no está disponible en SaguiOS")

    def canvasx(self, screenx, gridspacing=None):
        return float(screenx) + self._ox

    def canvasy(self, screeny, gridspacing=None):
        return float(screeny) + self._oy

    # ---- rolagem com scrollregion
    def _regiao(self):
        sr = self._op.get("scrollregion")
        if not sr:
            return None
        p = sr.split() if isinstance(sr, str) else list(sr)
        return [_int(v) for v in p]

    def _xview(self, *args):
        return self._rolar(args, True)

    def _yview(self, *args):
        return self._rolar(args, False)

    def _rolar(self, args, horizontal):
        r = self._regiao()
        tam = self._wl if horizontal else self._wa
        if r is None:
            return (0.0, 1.0)
        ini, fim = (r[0], r[2]) if horizontal else (r[1], r[3])
        total = max(1, fim - ini)
        atual = self._ox if horizontal else self._oy
        if not args:
            return ((atual - ini) / total, min(1.0, (atual - ini + tam) / total))
        if args[0] == "moveto":
            novo = ini + int(float(args[1]) * total)
        else:
            inc = _int(self._op.get("xscrollincrement" if horizontal else "yscrollincrement", 0)) or max(1, tam // 10)
            n = int(args[1])
            novo = atual + n * (int(tam * 0.9) if str(args[2]).startswith("page") else inc)
        novo = max(ini, min(novo, fim - tam))
        if horizontal:
            self._ox = novo
        else:
            self._oy = novo
        for item in self._itens.values():
            if item["tipo"] == "window":
                self._colocar_janela(item)
        self._redesenhar()
        cmd = self._op.get("xscrollcommand" if horizontal else "yscrollcommand")
        if cmd:
            a, b = self._rolar((), horizontal)
            cmd(a, b)
        return None

    def _opcoes_mudaram(self, kw):
        if "scrollregion" in kw:
            for h in (True, False):
                cmd = self._op.get("xscrollcommand" if h else "yscrollcommand")
                if cmd:
                    a, b = self._rolar((), h)
                    cmd(a, b)

    def _colocar(self, x, y, w, h):
        Widget._colocar(self, x, y, w, h)
        if self._regiao() is not None:
            self.after_idle(self._opcoes_mudaram, {"scrollregion": 1})

    # ---- caixas e acertos
    def _bbox_item(self, i):
        it = self._itens.get(i)
        if it is None:
            return None
        c, op, t = it["c"], it["op"], it["tipo"]
        if not c:
            return None
        if t == "text":
            linhas = _quebrar(op["text"], _int(op.get("width", 0)))
            l = max([len(x) for x in linhas] or [0]) * LETRA_L
            a = len(linhas) * LETRA_A
            x, y = self._ancorado(op["anchor"], c[0], c[1], l, a)
            return (x, y, x + l, y + a)
        if t == "image":
            img = op["image"]
            if isinstance(img, str):
                img = _imagens.get(img)
            if not img:
                return None
            l, a = img.width(), img.height()
            x, y = self._ancorado(op["anchor"], c[0], c[1], l, a)
            return (x, y, x + l, y + a)
        if t == "window":
            w = op.get("window")
            if w is None:
                return None
            l, a = w._wl or w._pedido()[0], w._wa or w._pedido()[1]
            x, y = self._ancorado(op["anchor"], c[0], c[1], l, a)
            return (x, y, x + l, y + a)
        xs, ys = c[0::2], c[1::2]
        m = float(op.get("width", 1) or 1) / 2 + 1
        if t == "line" and op.get("arrow", "none") != "none":
            m += float(op.get("arrowshape", (8, 10, 3))[2]) + 2
        return (int(min(xs) - m), int(min(ys) - m), int(max(xs) + m + 1), int(max(ys) + m + 1))

    def _ancorado(self, anchor, x, y, l, a):
        a_ = anchor or "center"
        if a_ == "center":
            a_ = ""
        px = x if "w" in a_ else (x - l if "e" in a_ else x - l // 2)
        py = y if "n" in a_ else (y - a if "s" in a_ else y - a // 2)
        return int(px), int(py)

    def bbox(self, *args):
        caixas = []
        for a in args:
            for i in self._ids(a):
                b = self._bbox_item(i)
                if b:
                    caixas.append(b)
        if not caixas:
            return None
        return (min(b[0] for b in caixas), min(b[1] for b in caixas), max(b[2] for b in caixas), max(b[3] for b in caixas))

    def _distancia(self, i, x, y):
        """0 se o ponto está no item; senão a distância até a caixa dele."""
        it = self._itens[i]
        b = self._bbox_item(i)
        if b is None:
            return None
        t, c, op = it["tipo"], it["c"], it["op"]
        dentro_caixa = b[0] <= x <= b[2] and b[1] <= y <= b[3]
        if not dentro_caixa:
            dx = max(b[0] - x, 0, x - b[2])
            dy = max(b[1] - y, 0, y - b[3])
            return math.sqrt(dx * dx + dy * dy)
        if t == "oval":
            cx, cy = (c[0] + c[2]) / 2, (c[1] + c[3]) / 2
            rx, ry = abs(c[2] - c[0]) / 2 + 1, abs(c[3] - c[1]) / 2 + 1
            d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
            if d > 1:
                return 1.0
            if not op.get("fill") and d < 0.8 * 0.8 and min(rx, ry) > 4:
                return 1.0   # oval vazio: só a borda acerta
            return 0
        if t == "rectangle" and not op.get("fill"):
            m = float(op.get("width", 1)) + 1
            x0, y0, x1, y1 = min(c[0], c[2]), min(c[1], c[3]), max(c[0], c[2]), max(c[1], c[3])
            if x0 + m < x < x1 - m and y0 + m < y < y1 - m:
                return 1.0
            return 0
        if t == "polygon" and len(c) >= 6:
            return 0 if self._no_poligono(c, x, y) else 1.0
        if t == "line":
            m = float(op.get("width", 1)) / 2 + 2
            melhor = None
            for k in range(0, len(c) - 3, 2):
                d = self._dist_segmento(x, y, c[k], c[k + 1], c[k + 2], c[k + 3])
                melhor = d if melhor is None else min(melhor, d)
            return 0 if melhor is not None and melhor <= m else (melhor or 0)
        return 0

    def _dist_segmento(self, px, py, x0, y0, x1, y1):
        dx, dy = x1 - x0, y1 - y0
        if dx == 0 and dy == 0:
            return math.sqrt((px - x0) ** 2 + (py - y0) ** 2)
        t = max(0, min(1, ((px - x0) * dx + (py - y0) * dy) / (dx * dx + dy * dy)))
        qx, qy = x0 + t * dx, y0 + t * dy
        return math.sqrt((px - qx) ** 2 + (py - qy) ** 2)

    def _no_poligono(self, c, x, y):
        dentro = False
        n = len(c) // 2
        j = n - 1
        for i in range(n):
            xi, yi, xj, yj = c[2 * i], c[2 * i + 1], c[2 * j], c[2 * j + 1]
            if (yi > y) != (yj > y) and x < (xj - xi) * (y - yi) / (yj - yi) + xi:
                dentro = not dentro
            j = i
        return dentro

    def _item_em(self, x, y):
        for i in reversed(self._ordem):
            if self._itens[i]["op"]["state"] in ("hidden", "disabled"):
                continue
            if self._itens[i]["tipo"] == "window":
                continue
            if self._distancia(i, x, y) == 0:
                return i
        return None

    # ---- desenho
    def _sujar_item(self, i):
        b = self._bbox_item(i)
        if b is None:
            return
        x0 = self._ax + b[0] - self._ox - 2
        y0 = self._ay + b[1] - self._oy - 2
        x1 = self._ax + b[2] - self._ox + 2
        y1 = self._ay + b[3] - self._oy + 2
        if self._zona is None:
            self._zona = [x0, y0, x1, y1]
        else:
            z = self._zona
            z[0], z[1], z[2], z[3] = min(z[0], x0), min(z[1], y0), max(z[2], x1), max(z[3], y1)
        self._redesenhar(1)

    def _desenhar(self, v):
        self._zona = None
        self._pintar(v, None)

    def _desenhar_parcial(self, v):
        z = self._zona
        self._zona = None
        if z is None:
            return
        ant = v.recorte()
        _recorte_dentro(v, ant, z[0], z[1], z[2] - z[0], z[3] - z[1])
        self._pintar(v, (z[0] - self._ax + self._ox, z[1] - self._ay + self._oy,
                         z[2] - self._ax + self._ox, z[3] - self._ay + self._oy))
        v.recorte(*ant)

    def _pintar(self, v, zona):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        fundo = self._cor_op("background", FUNDO)
        v.rect(x, y, w, h, fundo)
        b = _int(self._op.get("borderwidth", 0))
        ant = v.recorte()
        _recorte_dentro(v, ant, x + b, y + b, w - 2 * b, h - 2 * b)
        ox, oy = x - self._ox, y - self._oy
        for i in self._ordem:
            it = self._itens[i]
            if it["op"]["state"] == "hidden" or it["tipo"] == "window":
                continue
            if zona is not None:
                bb = self._bbox_item(i)
                if bb is None or bb[0] > zona[2] or bb[2] < zona[0] or bb[1] > zona[3] or bb[3] < zona[1]:
                    continue
            try:
                self._pintar_item(v, it, ox, oy, i == self._atual)
            except Exception as e:
                _relatar(e)
        v.recorte(*ant)
        _relevo(v, x, y, w, h, self._op.get("relief", "flat"), b, fundo)

    def _cores_item(self, op, ativo):
        fill = op.get("activefill") if ativo and op.get("activefill") else op.get("fill")
        outline = op.get("activeoutline") if ativo and op.get("activeoutline") else op.get("outline")
        largura = op.get("activewidth") if ativo and op.get("activewidth") else op.get("width", 1)
        return _cor(fill), _cor(outline), max(1, int(float(largura or 1)))

    def _pintar_item(self, v, it, ox, oy, ativo):
        t, c, op = it["tipo"], it["c"], it["op"]
        if t == "line":
            fill, _, g = self._cores_item(op, ativo)
            if fill is None or len(c) < 4:
                return
            pts = c
            if op.get("smooth") and len(c) >= 6:
                pts = self._suavizar(c, int(op.get("splinesteps", 12)))
            for k in range(0, len(pts) - 3, 2):
                v.linea(ox + pts[k], oy + pts[k + 1], ox + pts[k + 2], oy + pts[k + 3], fill, g)
            seta = op.get("arrow", "none")
            if seta in ("last", "both"):
                self._seta(v, ox, oy, c[-4], c[-3], c[-2], c[-1], fill, op)
            if seta in ("first", "both"):
                self._seta(v, ox, oy, c[2], c[3], c[0], c[1], fill, op)
        elif t in ("rectangle", "oval"):
            if len(c) < 4:
                return
            fill, outline, g = self._cores_item(op, ativo)
            x0, y0 = min(c[0], c[2]), min(c[1], c[3])
            l, a = abs(c[2] - c[0]), abs(c[3] - c[1])
            f = v.rect if t == "rectangle" else v.elipse
            if fill is not None:
                f(ox + x0, oy + y0, int(l) + (0 if outline is not None else 1), int(a) + (0 if outline is not None else 1), fill)
            if outline is not None:
                f(ox + x0, oy + y0, int(l) + 1, int(a) + 1, outline, relleno=False, grosor=g)
        elif t == "polygon":
            fill, outline, g = self._cores_item(op, ativo)
            pts = c
            if op.get("smooth") and len(c) >= 6:
                pts = self._suavizar(c + c[:2], int(op.get("splinesteps", 12)))
            desl = [pts[k] + (ox if k % 2 == 0 else oy) for k in range(len(pts))]
            if fill is not None and len(desl) >= 6:
                v.poligono(desl, fill)
            if outline is not None and len(desl) >= 4:
                v.poligono(desl, outline, relleno=False, grosor=g)
        elif t == "arc":
            self._pintar_arco(v, c, op, ox, oy, ativo)
        elif t == "text":
            fill, _, _ = self._cores_item(op, ativo)
            if fill is None:
                return
            linhas = _quebrar(op["text"], _int(op.get("width", 0)))
            l = max([len(x) for x in linhas] or [0]) * LETRA_L
            px, py = self._ancorado(op["anchor"], c[0], c[1], l, len(linhas) * LETRA_A)
            for k, linha in enumerate(linhas):
                lx = px
                if op.get("justify") == "center":
                    lx += (l - len(linha) * LETRA_L) // 2
                elif op.get("justify") == "right":
                    lx += l - len(linha) * LETRA_L
                v.texto(ox + lx, oy + py + k * LETRA_A, linha, fill)
        elif t == "image":
            img = op["image"]
            if isinstance(img, str):
                img = _imagens.get(img)
            if img:
                px, py = self._ancorado(op["anchor"], c[0], c[1], img.width(), img.height())
                img._desenhar_em(v, ox + px, oy + py)

    def _seta(self, v, ox, oy, x0, y0, x1, y1, cor, op):
        d1, d2, d3 = [float(k) for k in op.get("arrowshape", (8, 10, 3))]
        ang = math.atan2(y1 - y0, x1 - x0)
        ca, sa = math.cos(ang), math.sin(ang)
        bx, by = x1 - d2 * ca, y1 - d2 * sa
        pts = [x1, y1, bx - d3 * sa, by + d3 * ca, x1 - d1 * ca, y1 - d1 * sa, bx + d3 * sa, by - d3 * ca]
        v.poligono([pts[k] + (ox if k % 2 == 0 else oy) for k in range(8)], cor)

    def _suavizar(self, c, passos):
        """Curva quadrática pelos pontos médios (como o smooth do Tk)."""
        pts = [(c[k], c[k + 1]) for k in range(0, len(c) - 1, 2)]
        r = [pts[0][0], pts[0][1]]
        for k in range(1, len(pts) - 1):
            p0 = pts[k - 1] if k == 1 else ((pts[k - 1][0] + pts[k][0]) / 2, (pts[k - 1][1] + pts[k][1]) / 2)
            p2 = pts[k + 1] if k == len(pts) - 2 else ((pts[k][0] + pts[k + 1][0]) / 2, (pts[k][1] + pts[k + 1][1]) / 2)
            p1 = pts[k]
            for s in range(1, passos + 1):
                t = s / passos
                x = (1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0]
                y = (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]
                r.extend((x, y))
        if len(pts) == 2:
            r.extend(pts[1])
        return r

    def _pintar_arco(self, v, c, op, ox, oy, ativo):
        if len(c) < 4:
            return
        fill, outline, g = self._cores_item(op, ativo)
        cx, cy = (c[0] + c[2]) / 2, (c[1] + c[3]) / 2
        rx, ry = abs(c[2] - c[0]) / 2, abs(c[3] - c[1]) / 2
        ini = math.radians(float(op.get("start", 0)))
        ext = math.radians(float(op.get("extent", 90)))
        n = max(4, int(abs(ext) * max(rx, ry) / 4))
        pts = []
        for k in range(n + 1):
            a = ini + ext * k / n
            pts.extend((cx + rx * math.cos(a), cy - ry * math.sin(a)))
        estilo = op.get("style", "pieslice")
        if estilo == "pieslice":
            fechado = [cx, cy] + pts
        else:
            fechado = pts
        d = [fechado[k] + (ox if k % 2 == 0 else oy) for k in range(len(fechado))]
        if fill is not None and estilo != "arc" and len(d) >= 6:
            v.poligono(d, fill)
        if outline is not None:
            if estilo == "arc":
                for k in range(0, len(d) - 3, 2):
                    v.linea(d[k], d[k + 1], d[k + 2], d[k + 3], outline, g)
            else:
                v.poligono(d, outline, relleno=False, grosor=g)

    # ---- eventos dos itens (tag_bind)
    def tag_bind(self, tagOrId, sequence=None, func=None, add=None):
        chave = str(tagOrId)
        tabela = self._binds_tag.setdefault(chave, {})
        if func is None:
            r = tabela.get(sequence)
            return "" if r is None else r[1]
        padrao = _analisar(sequence)
        if add and sequence in tabela:
            tabela[sequence][1].append(func)
        else:
            tabela[sequence] = (padrao, [func])
        return "%d" % id(func)

    def tag_unbind(self, tagOrId, sequence, funcid=None):
        tabela = self._binds_tag.get(str(tagOrId), {})
        tabela.pop(sequence, None)

    def _disparar_item(self, i, tipo, detalhe, ev, estado, duplo=False):
        if i is None or i not in self._itens:
            return None
        chaves = ["all"] + list(self._itens[i]["op"]["tags"]) + [str(i)]
        for k in chaves:
            tabela = self._binds_tag.get(k)
            if not tabela:
                continue
            melhor, pontos = None, -1
            for padrao, funcs in tabela.values():
                p = _casa(padrao, tipo, detalhe, estado, duplo)
                if p > pontos:
                    melhor, pontos = funcs, p
            if melhor is not None and _chamar(melhor, ev) == "break":
                return "break"
        return None

    def _disparar(self, tipo, detalhe, ev, estado, duplo=False):
        if self._binds_tag and tipo in ("ButtonPress", "ButtonRelease", "Motion", "Enter", "Leave",
                                         "MouseWheel", "KeyPress"):
            cx, cy = ev.x + self._ox, ev.y + self._oy
            if tipo == "ButtonPress":
                self._mudar_atual(self._item_em(cx, cy), ev)
                self._agarrado_item = self._atual
                alvo = self._atual
            elif tipo in ("Motion", "ButtonRelease") and self._agarrado_item is not None and \
                    (tipo == "ButtonRelease" or estado & ESTADO_B1):
                alvo = self._agarrado_item
                if tipo == "ButtonRelease":
                    self._agarrado_item = None
            elif tipo == "Leave":
                self._mudar_atual(None, ev)
                alvo = None
            else:
                self._mudar_atual(self._item_em(cx, cy), ev)
                alvo = self._atual
            if alvo is not None and tipo not in ("Enter", "Leave"):
                ev.widget = self
                if self._disparar_item(alvo, tipo, detalhe, ev, estado, duplo) == "break":
                    return "break"
        return Widget._disparar(self, tipo, detalhe, ev, estado, duplo)

    def _mudar_atual(self, i, ev):
        if i == self._atual:
            return
        antes = self._atual
        self._atual = i
        ativo = False
        for k in (antes, i):
            if k is not None and k in self._itens:
                op = self._itens[k]["op"]
                if op.get("activefill") or op.get("activeoutline") or op.get("activewidth"):
                    self._sujar_item(k)
        if antes is not None:
            e = Event(EventType.Leave, self, ev.x, ev.y)
            self._disparar_item(antes, "Leave", None, e, 0)
        if i is not None:
            e = Event(EventType.Enter, self, ev.x, ev.y)
            self._disparar_item(i, "Enter", None, e, 0)

    def _tratar(self, tipo, ev):
        if tipo == "MouseWheel" and self._regiao() is not None:
            self._yview("scroll", -1 if ev.delta > 0 else 1, "units")
            return "break"
        return None

    def _usa_roda(self):
        return self._regiao() is not None
