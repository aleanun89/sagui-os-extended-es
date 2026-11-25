# SPDX-License-Identifier: GPL-2.0-or-later
# vps._fechas - Calendario y SelectorFecha de VisualPythonS (v8.4.3, D303).
#
#     Cal = Calendario(f, 8, 8, 196, 170)          # un mes con flechas; clic en un día lo elige
#     def Cal_cambio(): Etiqueta1.texto = Cal.fecha
#     Fecha = SelectorFecha(f, 8, 190, 130, 22)    # cuadro con la fecha y un desplegable con el calendario
#
# `valor` es la fecha como texto «AAAA-MM-DD»; también hay `anio`, `mes` y `dia`.
import time
import vps
import tkinter as tk
from vps import Control, _props, _ev

MESES = ("Enero", "Febrero", "Marzo", "Abril", "Mayo", "Junio", "Julio", "Agosto", "Septiembre", "Octubre",
         "Noviembre", "Diciembre")
DIAS = ("L", "M", "X", "J", "V", "S", "D")
ALTO_CAB, ALTO_SEM = 22, 18


def dias_del_mes(a, m):
    if m == 2:
        return 29 if (a % 4 == 0 and (a % 100 != 0 or a % 400 == 0)) else 28
    return 30 if m in (4, 6, 9, 11) else 31


def dia_semana(a, m, d):
    """0 = lunes ... 6 = domingo (Sakamoto)."""
    t = (0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4)
    if m < 3:
        a -= 1
    return (a + a // 4 - a // 100 + a // 400 + t[m - 1] + d + 6) % 7


def analizar(texto):
    """«2026-11-15» -> (2026, 11, 15), o None si no es una fecha válida."""
    try:
        p = str(texto).split("-")
        a, m, d = int(p[0]), int(p[1]), int(p[2])
    except (ValueError, IndexError):
        return None
    if 1 <= m <= 12 and 1 <= d <= dias_del_mes(a, m) and a > 0:
        return a, m, d
    return None


def hoy():
    t = time.localtime()
    return t[0], t[1], t[2]


def sumar_dias(f, n):
    """La fecha (a, m, d) n días después (n puede ser negativo)."""
    a, m, d = f
    d += n
    while d < 1:
        m -= 1
        if m < 1:
            m, a = 12, a - 1
        d += dias_del_mes(a, m)
    while d > dias_del_mes(a, m):
        d -= dias_del_mes(a, m)
        m += 1
        if m > 12:
            m, a = 1, a + 1
    return a, m, d


def celdas(a, m):
    """[(fila, columna, dia)] de los días del mes."""
    pri = dia_semana(a, m, 1)
    return [((pri + d - 1) // 7, (pri + d - 1) % 7, d) for d in range(1, dias_del_mes(a, m) + 1)]


@_props
class Calendario(Control):
    """Un mes con flechas (< > cambian de mes y << >> de año; también Re Pág y Av Pág); un clic en un día lo
    elige y un doble clic dispara `doble_click`. `valor` es la fecha «AAAA-MM-DD» (`anio`, `mes` y `dia` por
    separado). `minimo` y `maximo` («AAAA-MM-DD», vacío = sin límite) dejan sin poder elegir los días de fuera.
    Evento `cambio` al elegir un día. `hoy()` elige el día de hoy; `ir_a(anio, mes)` cambia el mes que se ve."""
    tipo = "Calendario"
    PROPS = (("valor", "str", ""), ("minimo", "str", ""), ("maximo", "str", ""))
    EVENTOS = _ev("cambio")
    EVENTO_DEFECTO = "cambio"
    DOBLE_PROPIO = True
    ORDEN = ("x", "y", "ancho", "alto")
    TAM = (196, 170)

    def _crear(self):
        from vps import _widgets
        self.w = _widgets._CalendarioW(self._contenedor_tk())
        self.w.al_elegir = self._elegido
        self.w.al_doble = self._doble

    def _elegido(self):
        self._p["valor"] = self.valor
        self._disparar("cambio")

    def _doble(self):
        self._disparar("doble_click")

    def _fecha(self):
        return "%04d-%02d-%02d" % (self.w.anio, self.w.mes, self.w.dia)

    def _leer(self, n):
        if self.w is not None:
            if n == "valor":
                return self._fecha()
            if n == "anio":
                return self.w.anio
            if n == "mes":
                return self.w.mes
            if n == "dia":
                return self.w.dia
        return self._p.get(n)

    valor = property(lambda self: self._leer("valor"), lambda self, v: self._escribir("valor", v))
    fecha = property(lambda self: self._leer("valor"), lambda self, v: self._escribir("valor", v))
    anio = property(lambda self: self._leer("anio"))
    mes = property(lambda self: self._leer("mes"))
    dia = property(lambda self: self._leer("dia"))

    def _aplicar(self, n):
        if n == "valor":
            f = analizar(self._p["valor"])
            if f is not None:
                self._poner(f)
        elif n in ("minimo", "maximo"):
            setattr(self.w, n, analizar(self._p[n]) if self._p[n] else None)
            self._poner((self.w.anio, self.w.mes, self.w.dia))
        else:
            Control._aplicar(self, n)

    def _poner(self, f):
        """Pone el día elegido (llevándolo al rango, si hay límites) y enseña su mes."""
        if self.w.minimo is not None and f < self.w.minimo:
            f = self.w.minimo
        if self.w.maximo is not None and f > self.w.maximo:
            f = self.w.maximo
        self.w.anio, self.w.mes, self.w.dia = f
        self.w.visible = (f[0], f[1])
        self.w._redesenhar()

    def hoy(self):
        """Elige el día de hoy."""
        self._escribir("valor", "%04d-%02d-%02d" % hoy())

    def ir_a(self, anio, mes):
        """Enseña ese mes sin cambiar el día elegido."""
        self.w.visible = (int(anio), max(1, min(12, int(mes))))
        self.w._redesenhar()

    def mes_visible(self):
        """(anio, mes) del mes que se ve."""
        return self.w.visible

    def en_rango(self, fecha):
        """Si el día «AAAA-MM-DD» se puede elegir (está entre `minimo` y `maximo`)."""
        f = analizar(fecha)
        return f is not None and self.w.en_rango(*f)


@_props
class SelectorFecha(Control):
    """Cuadro con la fecha («AAAA-MM-DD») y un botón que despliega el calendario. `valor` es la fecha; evento
    `cambio` al elegir un día o al escribir una fecha válida."""
    tipo = "SelectorFecha"
    PROPS = (("valor", "str", ""),)
    EVENTOS = _ev("cambio")
    EVENTO_DEFECTO = "cambio"
    ORDEN = ("x", "y", "ancho", "alto")
    TAM = (130, 22)

    def _crear(self):
        from vps import _widgets
        self._var = tk.StringVar()
        self.w = _widgets._CampoW(self._contenedor_tk(), textvariable=self._var)
        self.w.etiqueta = "v"
        self.w.al_pulsar = self.desplegar
        self._var.trace_add("write", lambda *a: self._escrito())

    def _escrito(self):
        self._p["valor"] = self._var.get()
        if analizar(self._p["valor"]) is not None:
            self._disparar("cambio")

    def _leer(self, n):
        if n == "valor" and self.w is not None:
            return self._var.get()
        return self._p.get(n)

    valor = property(lambda self: self._leer("valor"), lambda self, v: self._escribir("valor", v))

    def _aplicar(self, n):
        if n == "valor":
            if self._var.get() != str(self._p["valor"]):
                self._var.set(str(self._p["valor"]))
        elif n == "activo":
            self._cfg(state="normal" if self._p["activo"] else "disabled")
        else:
            Control._aplicar(self, n)

    def desplegar(self):
        """Abre el calendario bajo el cuadro; un clic en un día lo elige y lo cierra."""
        if not self._p["activo"]:
            return
        d = tk.Toplevel(tk._raiz_padrao(), _dialogo=True)
        d.title("Fecha")
        d.resizable(False, False)
        from vps import _widgets
        cal = _widgets._CalendarioW(d, width=196, height=170)
        cal.pack(padx=2, pady=2)
        f = analizar(self._var.get()) or hoy()
        cal.anio, cal.mes, cal.dia = f
        cal.visible = (f[0], f[1])

        def elegido():
            self._var.set("%04d-%02d-%02d" % (cal.anio, cal.mes, cal.dia))
            d.destroy()
        cal.al_elegir = elegido
        try:
            d.grab_set()
        except Exception:
            pass


TIPOS = {"Calendario": Calendario, "SelectorFecha": SelectorFecha}
METODOS = {"Calendario": [("hoy", "hoy()", "Elige el día de hoy"),
                          ("ir_a", "ir_a(anio, mes)", "Enseña ese mes sin cambiar el día elegido"),
                          ("mes_visible", "mes_visible()", "(anio, mes) del mes que se ve"),
                          ("en_rango", "en_rango(fecha)", "Si ese día «AAAA-MM-DD» está entre minimo y maximo")],
           "SelectorFecha": [("desplegar", "desplegar()", "Abre el calendario")]}
