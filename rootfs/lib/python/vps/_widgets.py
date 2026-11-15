# SPDX-License-Identifier: GPL-2.0-or-later
# vps._widgets - los componentes de tkinter que dibujan los controles nuevos de vps (v8.3.2, D303):
# Pestanas, Tabla, Enlace, BarraEstado y SelectorColor. Son subclases de los de tkinter con su
# propio `_desenhar`, de modo que el diseñador los dibuja con el mismo código que la aplicación.
# Se importa al crear el primer control de cada tipo (necesita el tkinter del sistema).
import tkinter as tk
from vps import TAB_H, _pest_tramos


class _PestanasW(tk.Frame):
    _classe = "Pestanas"
    _foca_no_clique = False
    _padrao = dict(tk.Frame._padrao, borderwidth=2)

    def _iniciar(self):
        self.titulos = []
        self.actual = 0

    def _extra_topo(self):
        return TAB_H

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        fondo = self._cor_op("background", tk.FUNDO)
        frente = self._cor_op("foreground", tk.TEXTO)
        v.rect(x, y, w, h, fondo)
        tk._relevo(v, x, y + TAB_H - 2, w, h - TAB_H + 2, "raised", 2, fondo)
        apagada = tk._misturar(fondo, 0x000000, 0.07)
        for i, (a, b) in enumerate(_pest_tramos(self.titulos)):
            sel = i == self.actual
            ya = y if sel else y + 3
            alto = TAB_H - (0 if sel else 3)
            v.rect(x + a, ya, b - a, alto, fondo if sel else apagada)
            tk._relevo(v, x + a, ya, b - a, alto + (0 if sel else 0), "raised", 1, fondo)
            if sel:
                v.rect(x + a + 1, y + TAB_H - 2, b - a - 2, 2, fondo)
            v.texto(x + a + 9, ya + (alto - tk.LETRA_A) // 2, str(self.titulos[i]), frente)


class _TablaW(tk.Frame):
    _classe = "Tabla"
    _foca_no_clique = True
    _padrao = dict(tk.Frame._padrao, borderwidth=2, relief="sunken", takefocus=1)
    ALTO_CAB, ALTO_FILA = 20, 18

    def _iniciar(self):
        self.columnas = []
        self.filas = []
        self.sel = -1
        self.arriba = 0
        self.orden = None       # (columna, descendente)
        self.ctl = None
        self.ent = None
        self.bind("<Button-1>", self._pulsar)
        self.bind("<Double-Button-1>", self._doble)
        self.bind("<MouseWheel>", self._rueda)
        self.bind("<Key>", self._tecla)

    def anchos(self):
        n = len(self.columnas)
        if not n:
            return []
        total = self._wl - 4
        b = total // n
        r = [b] * n
        r[-1] = total - b * (n - 1)
        return r

    def visibles(self):
        return max(1, (self._wa - 4 - self.ALTO_CAB) // self.ALTO_FILA)

    def _donde(self, ev):
        """(fila, columna) bajo el ratón; la fila -1 es la cabecera, None fuera de la tabla."""
        x, y = ev.x - 2, ev.y - 2
        col = -1
        acum = 0
        for i, a in enumerate(self.anchos()):
            if acum <= x < acum + a:
                col = i
            acum += a
        if y < 0 or col < 0:
            return None, col
        if y < self.ALTO_CAB:
            return -1, col
        f = self.arriba + (y - self.ALTO_CAB) // self.ALTO_FILA
        return (f if f < len(self.filas) else None), col

    def _pulsar(self, ev):
        self.cerrar_edicion()
        f, c = self._donde(ev)
        if f is None:
            return
        if self.ctl is not None:
            self.ctl._pulsado(f, c)

    def _doble(self, ev):
        f, c = self._donde(ev)
        if f is not None and f >= 0 and self.ctl is not None:
            self.ctl._doble(f, c)

    def _rueda(self, ev):
        mx = max(0, len(self.filas) - self.visibles())
        self.arriba = max(0, min(self.arriba + (3 if ev.delta < 0 else -3), mx))
        self._redesenhar()

    def _tecla(self, ev):
        if self.ent is not None or self.ctl is None:
            return
        if ev.keysym in ("Up", "Down") and self.filas:
            i = max(0, min(self.sel + (1 if ev.keysym == "Down" else -1), len(self.filas) - 1))
            self.ctl._elegir(i)
        elif ev.keysym == "Return" and self.sel >= 0:
            self.ctl._doble(self.sel, max(0, self.ctl.columna))

    def ver(self, i):
        """Desplaza para que la fila `i` se vea."""
        if i < self.arriba:
            self.arriba = i
        elif i >= self.arriba + self.visibles():
            self.arriba = i - self.visibles() + 1

    def editar(self, f, c):
        self.cerrar_edicion()
        self.ver(f)
        x = sum(self.anchos()[:c])
        y = self.ALTO_CAB + (f - self.arriba) * self.ALTO_FILA
        e = tk.Entry(self)
        e.insert(0, str(self.filas[f][c]) if c < len(self.filas[f]) else "")
        e.place(x=x, y=y, width=self.anchos()[c], height=self.ALTO_FILA)
        self.ent = (e, f, c)
        e.bind("<Return>", lambda ev: self.cerrar_edicion(True))
        e.bind("<Escape>", lambda ev: self.cerrar_edicion(False))
        e.focus_set()
        self._redesenhar()

    def cerrar_edicion(self, aceptar=True):
        if self.ent is None:
            return
        e, f, c = self.ent
        self.ent = None
        texto = e.get()
        e.destroy()
        if aceptar and self.ctl is not None:
            self.ctl.poner_celda(f, c, texto)
            self.ctl._disparar("cambio")
        self.focus_set()
        self._redesenhar()

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        fondo = self._cor_op("background", tk.BRANCO)
        frente = self._cor_op("foreground", tk.TEXTO)
        v.rect(x, y, w, h, fondo)
        anchos = self.anchos()
        x0, y0 = x + 2, y + 2
        # cabecera
        cx = x0
        for i, a in enumerate(anchos):
            v.rect(cx, y0, a, self.ALTO_CAB, tk.BOTAO)
            tk._relevo(v, cx, y0, a, self.ALTO_CAB, "raised", 1, tk.BOTAO)
            t = str(self.columnas[i])
            if self.orden is not None and self.orden[0] == i:
                t += " v" if self.orden[1] else " ^"
            t = t[:max(0, (a - 8) // 8)]
            v.texto(cx + 4, y0 + (self.ALTO_CAB - tk.LETRA_A) // 2, t, tk.TEXTO)
            cx += a
        # filas
        ant = v.recorte()
        v.recorte(x0, y0 + self.ALTO_CAB, w - 4, h - 4 - self.ALTO_CAB)
        for k in range(self.visibles() + 1):
            f = self.arriba + k
            fy = y0 + self.ALTO_CAB + k * self.ALTO_FILA
            if f >= len(self.filas):
                break
            elegida = f == self.sel
            if elegida:
                v.rect(x0, fy, w - 4, self.ALTO_FILA, tk.SELECAO)
            cx = x0
            for i, a in enumerate(anchos):
                t = str(self.filas[f][i]) if i < len(self.filas[f]) else ""
                t = t[:max(0, (a - 8) // 8)]
                v.texto(cx + 4, fy + (self.ALTO_FILA - tk.LETRA_A) // 2 + 1, t, tk.BRANCO if elegida else frente)
                if i:
                    v.linea(cx, fy, cx, fy + self.ALTO_FILA - 1, tk.TRILHO)
                cx += a
            v.linea(x0, fy + self.ALTO_FILA - 1, x0 + w - 5, fy + self.ALTO_FILA - 1, tk.TRILHO)
        v.recorte(*ant)
        if len(self.filas) > self.visibles():
            # una barra fina a la derecha: dónde está la parte que se ve
            th = h - 4 - self.ALTO_CAB
            n = len(self.filas)
            ph = max(10, th * self.visibles() // n)
            py = y0 + self.ALTO_CAB + (th - ph) * self.arriba // max(1, n - self.visibles())
            v.rect(x + w - 8, py, 5, ph, tk.POLEGAR)
        tk._relevo(v, x, y, w, h, "sunken", 2, fondo)


class _EnlaceW(tk.Label):
    _classe = "Label"

    def _desenhar(self, v):
        tk.Label._desenhar(self, v)
        t = self._texto()
        if t:
            _, frente = self._cores()
            y = self._ay + (self._wa - tk.LETRA_A) // 2 + tk.LETRA_A - 1
            x = self._ax + _int_tk(self._op.get("padx", 0)) + _int_tk(self._op.get("borderwidth", 0))
            v.linea(x, y, x + len(t) * tk.LETRA_L - 1, y, frente)


def _int_tk(v):
    try:
        return int(v)
    except (TypeError, ValueError):
        return 0


class _BarraW(tk.Widget):
    _classe = "BarraEstado"
    _foca_no_clique = False
    _padrao = {"width": 320, "height": 22, "background": tk.FUNDO, "foreground": tk.TEXTO,
               "takefocus": 0, "cursor": "", "highlightthickness": 0, "borderwidth": 0}

    def _iniciar(self):
        self.texto = ""
        self.paneles = []

    def _natural(self):
        return (tk._int(self._op.get("width", 320)), tk._int(self._op.get("height", 22)))

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        fondo = self._cor_op("background", tk.FUNDO)
        frente = self._cor_op("foreground", tk.TEXTO)
        v.rect(x, y, w, h, fondo)
        fin = x + w
        # los paneles de la derecha, desde el último
        for t in reversed(self.paneles):
            a = len(str(t)) * tk.LETRA_L + 16
            fin -= a
            tk._relevo(v, fin, y + 1, a, h - 2, "sunken", 1, fondo)
            v.texto(fin + 8, y + (h - tk.LETRA_A) // 2, str(t), frente)
        tk._relevo(v, x, y + 1, max(1, fin - x), h - 2, "sunken", 1, fondo)
        v.texto(x + 8, y + (h - tk.LETRA_A) // 2, str(self.texto)[:max(0, (fin - x - 12) // tk.LETRA_L)], frente)


class _SelectorW(tk.Widget):
    _classe = "SelectorColor"
    _foca_no_clique = False
    _padrao = {"width": 120, "height": 24, "takefocus": 0, "cursor": "", "highlightthickness": 0,
               "borderwidth": 0, "background": tk.BOTAO, "foreground": tk.TEXTO}

    def _iniciar(self):
        self.valor = 0

    def _natural(self):
        return (tk._int(self._op.get("width", 120)), tk._int(self._op.get("height", 24)))

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        fondo = self._cor_op("background", tk.BOTAO)
        v.rect(x, y, w, h, fondo)
        tk._relevo(v, x, y, w, h, "raised", 2, fondo)
        v.rect(x + 5, y + 5, 22, h - 10, self.valor)
        v.rect(x + 5, y + 5, 22, h - 10, tk.TEXTO, relleno=False)
        v.texto(x + 34, y + (h - tk.LETRA_A) // 2, "#%06X" % self.valor, self._cor_op("foreground", tk.TEXTO))


