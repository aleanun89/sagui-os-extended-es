# SPDX-License-Identifier: GPL-2.0-or-later
# basedatos._cuadricula - una rejilla de datos al estilo Win98 para el tkinter de SaguiOS
# (mejora 127, D304). La usan RejillaDatos (VisualPythonS) y el Explorador de bases de datos.
#
#     g = Cuadricula(padre, ancho=400, alto=200)
#     g.poner(["id", "nombre"], [["1", "Ana"], ["2", "Beto"]])
#     g.al_seleccionar = lambda fila: ...        # cambia la fila actual
#     g.al_ordenar = lambda col: ...             # clic en una cabecera
#     g.al_editar = lambda fila, col, texto: True  # una celda editada (False o error: no se acepta)
#
# Las celdas son texto. Teclado: flechas, Inicio/Fin, RePág/AvPág, Intro o F2 para editar la celda
# (o escribir directamente), Esc cancela la edición. Es un Frame con el cuerpo dibujado, dos barras
# y un Entry que aparece sobre la celda que se edita.
import tkinter as tk

GRIS, BLANCO, SOMBRA, NEGRO = 0xC0C0C0, 0xFFFFFF, 0x808080, 0x000000
LUZ, AZUL, CLARO = 0xDFDFDF, 0x000080, 0xE8E8F8

ALTO_FILA, ALTO_CAB, CAR = 18, 20, 8


def _elevado(v, x, y, w, h):
    v.rect(x, y, w, h, GRIS)
    v.linea(x, y, x + w - 1, y, BLANCO)
    v.linea(x, y, x, y + h - 1, BLANCO)
    v.linea(x, y + h - 1, x + w - 1, y + h - 1, NEGRO)
    v.linea(x + w - 1, y, x + w - 1, y + h - 1, NEGRO)
    v.linea(x + 1, y + h - 2, x + w - 2, y + h - 2, SOMBRA)
    v.linea(x + w - 2, y + 1, x + w - 2, y + h - 2, SOMBRA)


def _recortar(v, x, y, w, h):
    ax, ay, aw, ah = v.recorte()
    x0, y0 = max(ax, x), max(ay, y)
    x1, y1 = min(ax + aw, x + w), min(ay + ah, y + h)
    v.recorte(x0, y0, max(0, x1 - x0), max(0, y1 - y0))
    return (ax, ay, aw, ah)


class _Cuerpo(tk.Widget):
    _classe = "CuerpoCuadricula"
    _foca_no_clique = True
    _padrao = {"width": 300, "height": 160, "takefocus": 1, "cursor": "", "highlightthickness": 0,
               "borderwidth": 0}

    def __init__(self, master, rej):
        tk.Widget.__init__(self, master)
        self.rej = rej
        self.bind("<Double-Button-1>", self._doble)

    def _natural(self):
        return (int(self._op.get("width", 300)), int(self._op.get("height", 160)))

    def _colocar(self, x, y, w, h):
        tk.Widget._colocar(self, x, y, w, h)
        self.rej._cambio_tam()

    def _doble(self, ev):
        r = self.rej
        i, j = r._celda_en(ev.x, ev.y)
        if i >= 0 and j >= 0:
            r.seleccionar(i, j)
            r.editar()
        elif i == -2 and j >= 0 and r.al_doble_cabecera:
            r.al_doble_cabecera(j)

    def _desenhar(self, v):
        self.rej._pintar(v, self._ax, self._ay, self._wl, self._wa)

    def _tratar(self, tipo, ev):
        r = self.rej
        if tipo == "ButtonPress" and ev.num == 1:
            i, j = r._celda_en(ev.x, ev.y)
            if i == -2 and j >= 0:
                if r.al_ordenar:
                    r.al_ordenar(j)
            elif i >= 0 and j >= 0:
                r.seleccionar(i, j)
            elif i >= 0:
                r.seleccionar(i, r.col)
        elif tipo == "MouseWheel":
            r._desplazar(-3 if ev.delta > 0 else 3)
            return "break"
        elif tipo == "KeyPress":
            return r._tecla(ev)
        return None

    def _usa_roda(self):
        return True


class Cuadricula(tk.Frame):
    def __init__(self, master, ancho=300, alto=160, **kw):
        tk.Frame.__init__(self, master, width=ancho, height=alto)
        self.columnas = []
        self.filas = []
        self.anchos = []
        self.fila = -1           # fila actual
        self.col = 0             # columna actual
        self.arriba = 0          # primera fila visible
        self.izq = 0             # desplazamiento horizontal en píxeles
        self.editable = False
        self.col_editables = None    # None = todas
        self.orden_col = -1
        self.orden_dir = 1
        self.al_seleccionar = None
        self.al_ordenar = None
        self.al_editar = None
        self.al_doble_cabecera = None
        self.al_doble_fila = None
        self.nulos = set()       # celdas (fila, columna) que son NULL: se dibujan en gris (D333)
        self._ent = None
        self._ed = None
        self._cuerpo = _Cuerpo(self, self)
        self._sbv = tk.Scrollbar(self, orient="vertical", command=self._cmd_v)
        self._sbh = tk.Scrollbar(self, orient="horizontal", command=self._cmd_h)
        self._cuerpo.place(x=0, y=0, width=max(1, ancho - 16), height=max(1, alto - 16))
        self._sbv.place(x=ancho - 16, y=0, width=16, height=max(1, alto - 16))
        self._sbh.place(x=0, y=alto - 16, width=max(1, ancho - 16), height=16)
        self._ancho_tot, self._alto_tot = ancho, alto
        self.bind("<Configure>", self._configurado)

    # ---------------------------------------------------------- tamaño
    def _configurado(self, ev):
        w, h = self.winfo_width(), self.winfo_height()
        if w < 40 or h < 40 or (w, h) == (self._ancho_tot, self._alto_tot):
            return
        self._ancho_tot, self._alto_tot = w, h
        self._cuerpo.place(x=0, y=0, width=w - 16, height=h - 16)
        self._sbv.place(x=w - 16, y=0, width=16, height=h - 16)
        self._sbh.place(x=0, y=h - 16, width=w - 16, height=16)
        self._cambio_tam()

    def tamano(self, ancho, alto):
        """Cambia el tamaño (para los controles que lo gobiernan con place)."""
        self.configure(width=ancho, height=alto)
        self._configurado(None)

    def _cambio_tam(self):
        self._ajustar()

    def _visibles(self):
        return max(1, (self._cuerpo._wa - ALTO_CAB) // ALTO_FILA)

    def _ancho_cols(self):
        return sum(self.anchos)

    def _ajustar(self):
        vis = self._visibles()
        self.arriba = max(0, min(self.arriba, max(0, len(self.filas) - vis)))
        util = max(1, self._cuerpo._wl)
        self.izq = max(0, min(self.izq, max(0, self._ancho_cols() - util)))
        n = max(1, len(self.filas))
        self._sbv.set(self.arriba / n, min(1.0, (self.arriba + vis) / n))
        t = max(1, self._ancho_cols())
        self._sbh.set(self.izq / t, min(1.0, (self.izq + util) / t))
        self._cuerpo._redesenhar()

    def _cmd_v(self, *a):
        vis = self._visibles()
        if a[0] == "scroll":
            self._desplazar(int(a[1]) * (vis - 1 if a[2] == "pages" else 1))
        elif a[0] == "moveto":
            self.arriba = int(float(a[1]) * max(1, len(self.filas)))
            self._ajustar()

    def _cmd_h(self, *a):
        util = max(1, self._cuerpo._wl)
        if a[0] == "scroll":
            self.izq += int(a[1]) * (util - 20 if a[2] == "pages" else 24)
        elif a[0] == "moveto":
            self.izq = int(float(a[1]) * max(1, self._ancho_cols()))
        self._ajustar()

    def _desplazar(self, n):
        self.arriba += n
        self._ajustar()

    # ---------------------------------------------------------- datos
    def poner(self, columnas, filas, conservar=False, nulos=None):
        """Cambia columnas (nombres) y filas (listas de textos). Con conservar=True mantiene la fila
        y la columna actuales si siguen existiendo. `nulos` es el conjunto de celdas (fila, columna)
        que son NULL y se dibujan en gris (D333)."""
        self._cancelar_edicion()
        self.nulos = set(nulos) if nulos else set()
        f0, c0 = self.fila, self.col
        self.columnas = [str(c) for c in columnas]
        self.filas = [[("" if v is None else str(v)) for v in f] for f in filas]
        self.anchos = []
        for j, c in enumerate(self.columnas):
            m = len(c) + 3
            for f in self.filas[:200]:
                if j < len(f):
                    m = max(m, min(len(f[j]), 40) + 1)
            self.anchos.append(max(5, min(m, 40)) * CAR + 6)
        self.arriba = 0
        self.izq = 0
        if conservar and f0 < len(self.filas):
            self.fila, self.col = f0, min(c0, max(0, len(self.columnas) - 1))
        else:
            self.fila = 0 if self.filas else -1
            self.col = 0
        self._ajustar()
        self._ver_fila()

    def poner_celda(self, i, j, texto):
        self.filas[i][j] = texto
        self._cuerpo._redesenhar()

    def seleccionar(self, i, j=None):
        if not self.filas:
            self.fila = -1
            return
        i = max(0, min(i, len(self.filas) - 1))
        cambio = i != self.fila
        self.fila = i
        if j is not None:
            self.col = max(0, min(j, len(self.columnas) - 1))
        self._ver_fila()
        self._cuerpo._redesenhar()
        if cambio and self.al_seleccionar:
            self.al_seleccionar(i)

    def _ver_fila(self):
        if self.fila < 0:
            return
        vis = self._visibles()
        if self.fila < self.arriba:
            self.arriba = self.fila
        elif self.fila >= self.arriba + vis:
            self.arriba = self.fila - vis + 1
        x0 = sum(self.anchos[:self.col])
        x1 = x0 + (self.anchos[self.col] if self.col < len(self.anchos) else 0)
        util = max(1, self._cuerpo._wl)
        if x0 < self.izq:
            self.izq = x0
        elif x1 > self.izq + util:
            self.izq = x1 - util
        self._ajustar()

    def fila_actual(self):
        return self.fila

    # ---------------------------------------------------------- geometría
    def _celda_en(self, x, y):
        """(fila, columna) bajo un punto del cuerpo; fila -2 = cabecera, -1 = fuera."""
        col = -1
        px = -self.izq
        for j, a in enumerate(self.anchos):
            if px <= x < px + a:
                col = j
                break
            px += a
        if y < ALTO_CAB:
            return -2, col
        i = self.arriba + (y - ALTO_CAB) // ALTO_FILA
        return (i if 0 <= i < len(self.filas) else -1), col

    def centro_celda(self, i, j):
        """El centro de una celda, en coordenadas del cuerpo (para las pruebas)."""
        x = sum(self.anchos[:j]) - self.izq + self.anchos[j] // 2
        y = ALTO_CAB + (i - self.arriba) * ALTO_FILA + ALTO_FILA // 2
        return x, y

    def centro_cabecera(self, j):
        return sum(self.anchos[:j]) - self.izq + self.anchos[j] // 2, ALTO_CAB // 2

    # ---------------------------------------------------------- dibujo
    def _pintar(self, v, x, y, w, h):
        v.rect(x, y, w, h, BLANCO)
        ant = _recortar(v, x, y, w, h)
        vis = self._visibles() + 1
        for k in range(vis):
            i = self.arriba + k
            if i >= len(self.filas):
                break
            fy = y + ALTO_CAB + k * ALTO_FILA
            actual = i == self.fila
            if actual:
                v.rect(x, fy, w, ALTO_FILA, CLARO)
            elif i % 2:
                v.rect(x, fy, w, ALTO_FILA, 0xF6F6F6)
            px = x - self.izq
            f = self.filas[i]
            for j, a in enumerate(self.anchos):
                if px + a > x and px < x + w:
                    t = f[j] if j < len(f) else ""
                    cab = (a - 6) // CAR
                    if len(t) > cab:
                        t = t[:max(0, cab - 1)] + "~"
                    t = t.replace("\n", " ")
                    if actual and j == self.col:
                        v.rect(px, fy, a, ALTO_FILA, AZUL)
                        v.texto(px + 3, fy + 1, t, BLANCO)
                    else:
                        v.texto(px + 3, fy + 1, t, 0x808080 if (i, j) in self.nulos else NEGRO)
                    v.linea(px + a - 1, fy, px + a - 1, fy + ALTO_FILA - 1, LUZ)
                px += a
            v.linea(x, fy + ALTO_FILA - 1, x + w - 1, fy + ALTO_FILA - 1, LUZ)
        # cabecera (encima de las filas)
        px = x - self.izq
        for j, a in enumerate(self.anchos):
            if px + a > x and px < x + w:
                _elevado(v, px, y, a, ALTO_CAB)
                t = self.columnas[j]
                if j == self.orden_col:
                    t += " " + ("v" if self.orden_dir < 0 else "^")
                cab = (a - 6) // CAR
                if len(t) > cab:
                    t = t[:max(0, cab - 1)] + "~"
                v.texto(px + 3, y + 2, t, NEGRO)
            px += a
        if px < x + w:
            v.rect(px, y, x + w - px, ALTO_CAB, GRIS)
        v.recorte(*ant)

    # ---------------------------------------------------------- teclado
    def _tecla(self, ev):
        k = ev.keysym
        i, j = self.fila, self.col
        vis = self._visibles()
        if k == "Up":
            i -= 1
        elif k == "Down":
            i += 1
        elif k == "Left":
            j -= 1
        elif k == "Right":
            j += 1
        elif k == "Home":
            i = 0
        elif k == "End":
            i = len(self.filas) - 1
        elif k == "Prior":
            i -= vis
        elif k == "Next":
            i += vis
        elif k in ("Return", "F2"):
            self.editar()
            return "break"
        elif len(ev.char) == 1 and ev.char >= " " and ev.char != "\x7f" and self.editable:
            self.editar(ev.char)
            return "break"
        else:
            return None
        self.seleccionar(i, j)
        return "break"

    # ---------------------------------------------------------- edición
    def puede_editar(self, j):
        return self.editable and (self.col_editables is None or self.columnas[j] in self.col_editables)

    def editar(self, inicial=None):
        """Abre el editor sobre la celda actual (si la rejilla es editable y la columna también)."""
        if self.fila < 0 or not self.columnas or not self.puede_editar(self.col):
            return False
        self._cancelar_edicion()
        i, j = self.fila, self.col
        x = sum(self.anchos[:j]) - self.izq
        y = ALTO_CAB + (i - self.arriba) * ALTO_FILA
        a = max(24, min(self.anchos[j], self._cuerpo._wl - max(0, x)))
        e = tk.Entry(self)
        e.place(x=max(0, x), y=y, width=a, height=ALTO_FILA + 2)
        e.insert(0, self.filas[i][j] if inicial is None else inicial)
        e.focus_set()
        e.bind("<Return>", lambda ev: self._confirmar())
        e.bind("<Escape>", lambda ev: self._cancelar_edicion())
        e.bind("<Tab>", lambda ev: self._confirmar())
        self._ent, self._ed = e, (i, j)
        return True

    def editando(self):
        return self._ent is not None

    def texto_edicion(self, t):
        """Cambia el texto del editor abierto (para las pruebas)."""
        if self._ent is not None:
            self._ent.delete(0, "end")
            self._ent.insert(0, t)

    def _cancelar_edicion(self):
        if self._ent is not None:
            e, self._ent, self._ed = self._ent, None, None
            try:
                e.destroy()
            except Exception:
                pass
            self._cuerpo.focus_set()

    def confirmar_edicion(self):
        self._confirmar()

    def _confirmar(self):
        if self._ent is None:
            return
        texto = self._ent.get()
        i, j = self._ed
        self._cancelar_edicion()
        if texto == self.filas[i][j]:
            return
        ok = True
        if self.al_editar:
            ok = self.al_editar(i, j, texto)
        if ok is not False:
            self.nulos.discard((i, j))
            self.filas[i][j] = texto
            self._cuerpo._redesenhar()
