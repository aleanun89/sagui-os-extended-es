# SPDX-License-Identifier: GPL-2.0-or-later
# vps._widgets - los componentes de tkinter que dibujan los controles nuevos de vps (v8.3.2, D303):
# Pestanas, Tabla, Enlace, BarraEstado y SelectorColor. Son subclases de los de tkinter con su
# propio `_desenhar`, de modo que el diseñador los dibuja con el mismo código que la aplicación.
# Se importa al crear el primer control de cada tipo (necesita el tkinter del sistema).
import sagui
import tkinter as tk
from vps import TAB_H, _pest_tramos
from vps import _fechas as FE


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
            self.ctl.fila, self.ctl.columna = f, c
            self.ctl._disparar("cambio")
            self.ctl._disparar("celda_cambio")
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




class _CampoW(tk.Entry):
    """Cuadro de texto con un botón «...» a la derecha (CampoArchivo y CampoCarpeta, D303): el botón
    llama a `self.al_pulsar`. Como el Spinbox, el botón se dibuja dentro del propio componente."""
    _classe = "CampoRuta"
    _BOTAO = 26
    al_pulsar = None
    etiqueta = "..."

    def _natural(self):
        l, a = tk.Entry._natural(self)
        return l + self._BOTAO, a

    def _cabem(self):
        b = tk._int(self._op.get("borderwidth", 2))
        return max(1, (self._wl - self._BOTAO - 2 * (b + self._PAD)) // tk.LETRA_L)

    def _desenhar(self, v):
        tk.Entry._desenhar(self, v)
        x = self._ax + self._wl - self._BOTAO - 2
        y = self._ay + 2
        h = self._wa - 4
        v.rect(x, y, self._BOTAO, h, tk.BOTAO)
        tk._relevo(v, x, y, self._BOTAO, h, "raised", 1)
        v.texto(x + (self._BOTAO - len(self.etiqueta) * tk.LETRA_L) // 2, y + (h - tk.LETRA_A) // 2, self.etiqueta, tk.TEXTO)

    def _tratar(self, tipo, ev):
        if self._op.get("state", "normal") == "disabled":
            return None
        if tipo == "ButtonPress" and ev.num == 1 and ev.x >= self._wl - self._BOTAO - 2:
            f = self.al_pulsar
            if f is not None:
                f()
            return "break"
        return tk.Entry._tratar(self, tipo, ev)


class _CalendarioW(tk.Widget):
    """El calendario dibujado: cabecera con flechas (« y » cambian de año, < y > de mes), días de la semana y
    la rejilla del mes. `minimo` y `maximo` (tuplas (a, m, d) o None) dejan fuera los días de fuera del rango."""
    _classe = "Calendario"
    _foca_no_clique = True
    _padrao = {"width": 196, "height": 170, "takefocus": 1, "cursor": "", "highlightthickness": 0,
               "borderwidth": 2, "background": tk.BRANCO, "foreground": tk.TEXTO}

    def _iniciar(self):
        self.anio, self.mes, self.dia = FE.hoy()
        self.visible = (self.anio, self.mes)    # el mes que se ve (puede no ser el del día elegido)
        self.al_elegir = None
        self.al_doble = None
        self.minimo = None
        self.maximo = None
        self.bind("<Button-1>", self._pulsar)
        self.bind("<Double-Button-1>", self._doble)
        self.bind("<Key>", self._tecla)

    def _natural(self):
        return (tk._int(self._op.get("width", 196)), tk._int(self._op.get("height", 170)))

    def _geom(self):
        w, h = self._wl, self._wa
        cw = max(8, (w - 4) // 7)
        ch = max(8, (h - 4 - FE.ALTO_CAB - FE.ALTO_SEM) // 6)
        return cw, ch

    def en_rango(self, a, m, d):
        f = (a, m, d)
        return (self.minimo is None or f >= self.minimo) and (self.maximo is None or f <= self.maximo)

    def mover(self, delta):
        a, m = self.visible
        m += delta
        while m < 1:
            m += 12
            a -= 1
        while m > 12:
            m -= 12
            a += 1
        self.visible = (max(1, a), m)
        self._redesenhar()

    def _dia_en(self, x, y):
        """(a, m, d) del día bajo (x, y), o None."""
        cw, ch = self._geom()
        fila = (y - 2 - FE.ALTO_CAB - FE.ALTO_SEM) // ch
        col = (x - 2) // cw
        if y < 2 + FE.ALTO_CAB + FE.ALTO_SEM or not (0 <= col < 7 and 0 <= fila < 6):
            return None
        a, m = self.visible
        for f, c, d in FE.celdas(a, m):
            if f == fila and c == col:
                return (a, m, d)
        return None

    def _elegir(self, f):
        """Elige el día f = (a, m, d) si está en el rango; True si lo eligió."""
        if f is None or not self.en_rango(*f):
            return False
        self.anio, self.mes, self.dia = f
        self.visible = (f[0], f[1])
        self._redesenhar()
        if self.al_elegir is not None:
            self.al_elegir()
        return True

    def _pulsar(self, ev):
        x, y = ev.x, ev.y
        if y < FE.ALTO_CAB + 2:
            if x < 24:
                self.mover(-1)
            elif x < 40:
                self.mover(-12)
            elif x >= self._wl - 24:
                self.mover(1)
            elif x >= self._wl - 40:
                self.mover(12)
            return
        f = self._dia_en(x, y)
        if f is not None:
            self._elegir(f)

    def _doble(self, ev):
        f = self._dia_en(ev.x, ev.y)
        if f is not None and self.en_rango(*f):
            if (self.anio, self.mes, self.dia) != f:
                self._elegir(f)
            if self.al_doble is not None:
                self.al_doble()

    def _tecla(self, ev):
        k = ev.keysym
        if k in ("Prior", "Next"):
            self.mover(-1 if k == "Prior" else 1)
        elif k in ("Left", "Right", "Up", "Down"):
            paso = {"Left": -1, "Right": 1, "Up": -7, "Down": 7}[k]
            n = FE.sumar_dias((self.anio, self.mes, self.dia), paso)
            if self.en_rango(*n):
                self._elegir(n)
        elif k == "Return" and self.al_doble is not None:
            self.al_doble()

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        fondo = self._cor_op("background", tk.BRANCO)
        frente = self._cor_op("foreground", tk.TEXTO)
        v.rect(x, y, w, h, fondo)
        tk._moldura_campo(v, x, y, w, h, "sunken", 2, False)
        a, m = self.visible
        v.rect(x + 2, y + 2, w - 4, FE.ALTO_CAB, 0x000080)
        titulo = "%s %d" % (FE.MESES[m - 1], a)
        ty = y + 2 + (FE.ALTO_CAB - 16) // 2
        v.texto(x + (w - len(titulo) * 8) // 2, ty, titulo, 0xFFFFFF)
        cy0 = y + 2 + FE.ALTO_CAB // 2
        for dx, n in ((8, 1), (26, 2)):         # < mes anterior, << año anterior
            for k in range(n):
                v.poligono([(x + dx + 5 + 5 * k, cy0 - 4), (x + dx + 5 + 5 * k, cy0 + 4), (x + dx + 5 * k, cy0)], 0xFFFFFF if n == 1 else 0xB0B8FF)
        for dx, n in ((w - 14, 1), (w - 36, 2)):    # > mes siguiente, >> año siguiente
            for k in range(n):
                v.poligono([(x + dx + 5 * k, cy0 - 4), (x + dx + 5 * k, cy0 + 4), (x + dx + 5 + 5 * k, cy0)], 0xFFFFFF if n == 1 else 0xB0B8FF)
        cw, ch = self._geom()
        for c in range(7):
            v.texto(x + 2 + c * cw + (cw - 8) // 2, y + 2 + FE.ALTO_CAB + 1, FE.DIAS[c], 0x808080)
        ya = y + 2 + FE.ALTO_CAB + FE.ALTO_SEM
        h_a, h_m, h_d = FE.hoy()
        for f, c, d in FE.celdas(a, m):
            cx, cy = x + 2 + c * cw, ya + f * ch
            elegido = (a, m, d) == (self.anio, self.mes, self.dia)
            libre = self.en_rango(a, m, d)
            if elegido:
                v.rect(cx + 1, cy + 1, cw - 2, ch - 2, 0x2A6FD8 if libre else 0x9AA3AC)
            elif (a, m, d) == (h_a, h_m, h_d):
                v.rect(cx + 1, cy + 1, cw - 2, ch - 2, 0x2A6FD8, relleno=False)
            t = str(d)
            col = 0xFFFFFF if elegido else (0xC4CACF if not libre else (0x900000 if c == 6 else frente))
            v.texto(cx + (cw - len(t) * 8) // 2, cy + (ch - 16) // 2, t, col)


class _ArbolW(tk.Frame):
    """La vista en árbol dibujada (v8.5.6, D466): una fila por nodo visible, con la sangría de su nivel y un
    cuadrito [+] / [-] en los que tienen hijos. Los nodos son diccionarios {clave, texto, padre, nivel, abierto}
    en orden de recorrido (cada nodo va antes que sus hijos); los guarda y cambia el control (vps._arbol)."""
    _classe = "Arbol"
    _foca_no_clique = True
    _padrao = dict(tk.Frame._padrao, borderwidth=2, relief="sunken", takefocus=1, background=tk.BRANCO)
    ALTO_FILA, SANGRIA = 18, 16

    def _iniciar(self):
        self.nodos = []
        self.sel = None         # la clave del nodo elegido
        self.arriba = 0         # la primera fila visible que se ve
        self.ctl = None
        self._ver = None        # el nodo que tiene que quedar a la vista (se aplica al dibujar, con el tamaño real)
        self.bind("<Button-1>", self._pulsar)
        self.bind("<Double-Button-1>", self._doble)
        self.bind("<MouseWheel>", self._rueda)
        self.bind("<Key>", self._tecla)

    def visibles(self):
        """Los nodos que se ven (los que no tienen un ascendiente contraído), en orden."""
        r = []
        cerrado = None
        for n in self.nodos:
            if cerrado is not None:
                if n["nivel"] > cerrado:
                    continue
                cerrado = None
            r.append(n)
            if not n["abierto"]:
                cerrado = n["nivel"]
        return r

    def tiene_hijos(self, i):
        return i + 1 < len(self.nodos) and self.nodos[i + 1]["nivel"] > self.nodos[i]["nivel"]

    def con_hijos(self):
        """Las claves de los nodos que tienen hijos."""
        return set(n["clave"] for i, n in enumerate(self.nodos) if self.tiene_hijos(i))

    def filas_que_caben(self):
        return max(1, (self._wa - 4) // self.ALTO_FILA)

    def _donde(self, ev):
        """(nodo, en_caja): la fila bajo el ratón y si el clic cae en su cuadrito de expandir."""
        vis = self.visibles()
        f = self.arriba + (ev.y - 2) // self.ALTO_FILA
        if ev.y < 2 or not (0 <= f < len(vis)):
            return None, False
        n = vis[f]
        x0 = 2 + 4 + n["nivel"] * self.SANGRIA
        return n, x0 <= ev.x < x0 + 12

    def _pulsar(self, ev):
        n, caja = self._donde(ev)
        if n is not None and self.ctl is not None:
            self.ctl._pulsado(n, caja)

    def _doble(self, ev):
        n, caja = self._donde(ev)
        if n is not None and not caja and self.ctl is not None:
            self.ctl._doble(n)

    def _rueda(self, ev):
        mx = max(0, len(self.visibles()) - self.filas_que_caben())
        self.arriba = max(0, min(self.arriba + (3 if ev.delta < 0 else -3), mx))
        self._redesenhar()

    def _tecla(self, ev):
        if self.ctl is not None:
            self.ctl._tecla(ev.keysym)

    def ver(self, clave):
        """Desplaza para que el nodo de esa clave se vea (al dibujar: el control puede no tener aún su tamaño)."""
        self._ver = clave
        self._redesenhar()

    def _ajustar(self, clave):
        vis = self.visibles()
        for i, n in enumerate(vis):
            if n["clave"] == clave:
                if i < self.arriba:
                    self.arriba = i
                elif i >= self.arriba + self.filas_que_caben():
                    self.arriba = i - self.filas_que_caben() + 1
                return

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        fondo = self._cor_op("background", tk.BRANCO)
        frente = self._cor_op("foreground", tk.TEXTO)
        v.rect(x, y, w, h, fondo)
        if self._ver is not None:
            clave, self._ver = self._ver, None
            self._ajustar(clave)
        vis = self.visibles()
        self.arriba = max(0, min(self.arriba, len(vis) - self.filas_que_caben()))
        ant = v.recorte()
        v.recorte(x + 2, y + 2, w - 4, h - 4)
        padres = self.con_hijos()
        for k in range(self.filas_que_caben() + 1):
            f = self.arriba + k
            if f >= len(vis):
                break
            n = vis[f]
            fy = y + 2 + k * self.ALTO_FILA
            elegida = n["clave"] == self.sel
            if elegida:
                v.rect(x + 2, fy, w - 4, self.ALTO_FILA, tk.SELECAO)
            cx = x + 2 + 4 + n["nivel"] * self.SANGRIA
            if n["clave"] in padres:
                by = fy + (self.ALTO_FILA - 9) // 2
                v.rect(cx + 1, by, 9, 9, tk.BRANCO)
                v.rect(cx + 1, by, 9, 9, 0x808080, relleno=False)
                v.linea(cx + 3, by + 4, cx + 7, by + 4, tk.TEXTO)
                if not n["abierto"]:
                    v.linea(cx + 5, by + 2, cx + 5, by + 6, tk.TEXTO)
            v.texto(cx + 16, fy + (self.ALTO_FILA - tk.LETRA_A) // 2 + 1, str(n["texto"]), tk.BRANCO if elegida else frente)
        v.recorte(*ant)
        if len(vis) > self.filas_que_caben():
            th = h - 4
            ph = max(10, th * self.filas_que_caben() // len(vis))
            py = y + 2 + (th - ph) * self.arriba // max(1, len(vis) - self.filas_que_caben())
            v.rect(x + w - 8, py, 5, ph, tk.POLEGAR)
        tk._relevo(v, x, y, w, h, "sunken", 2, fondo)


class _GraficoW(tk.Widget):
    """El gráfico (v8.5.6, D469): el módulo de C `sagui_grafico` pinta en una Imagen del tamaño del control y
    aquí solo se enseña. La animación avanza por tiempo: mientras el gráfico dice que sigue animándose, el
    control pide otro dibujo cada ~30 ms con `after` (no bloquea nada). El ratón pasa al gráfico con
    coordenadas del propio control."""
    _classe = "Grafico"
    _foca_no_clique = False
    _padrao = {"width": 360, "height": 240, "takefocus": 0, "cursor": "", "highlightthickness": 0,
               "borderwidth": 0, "background": tk.BRANCO}

    def _iniciar(self):
        self.ctl = None
        self._img = None
        self._tam = (0, 0)
        self._tic = False
        self._vigila = False
        self.bind("<Motion>", self._mueve)
        self.bind("<Leave>", self._sale)
        self.bind("<Button-1>", self._pulsa)

    def _natural(self):
        return (tk._int(self._op.get("width", 360)), tk._int(self._op.get("height", 240)))

    def _desenhar(self, v):
        c = self.ctl
        if c is None or c.g is None:
            return
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        if w < 16 or h < 16:
            return
        c._sincronizar()
        if self._img is None or self._tam != (w, h):
            self._img = sagui.Imagen.nueva(w, h, 0xFFFFFF)
            self._tam = (w, h)
        activo = c.g.avanzar(sagui.ticks_ms())
        c.g.dibujar(self._img, w, h)
        v.imagen(x, y, self._img)
        if activo and not self._tic:
            self._tic = True
            self.after(30, self._tick)
        if not self._vigila:
            self._vigila = True
            self.after(150, self._vigilar)

    def _tick(self):
        self._tic = False
        if not self._destruido:
            self._redesenhar()

    def _vigilar(self):
        """Mira si el programa cambió `tipo` (un atributo del objeto, ver vps._grafico) y lo aplica."""
        self._vigila = False
        if self._destruido or self.ctl is None:
            return
        if self.ctl._sincronizar():
            self._redesenhar()
        self._vigila = True
        self.after(150, self._vigilar)

    def _mueve(self, ev):
        c = self.ctl
        if c is None or c.g is None:
            return
        if c.g.mover(ev.x, ev.y):
            self._redesenhar()
        c._hover(c.g.resaltado())

    def _sale(self, ev):
        c = self.ctl
        if c is None or c.g is None:
            return
        c.g.salir()
        self._redesenhar()
        c._hover(None)

    def _pulsa(self, ev):
        c = self.ctl
        if c is not None and c.g is not None:
            c._pulsado(ev.x, ev.y)
            self._redesenhar()
