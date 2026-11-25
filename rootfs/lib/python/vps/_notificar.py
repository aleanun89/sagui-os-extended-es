# SPDX-License-Identifier: GPL-2.0-or-later
# vps._notificar - notificaciones tipo toast de VisualPythonS (v8.5.6, D466).
#
#     notificar("Guardado", "exito")                       # un aviso que se va solo a los 3 s
#     notificar("No se pudo guardar", "error", 0, "Fallo")   # 0 = hasta que se cierre
#     n = notificar("Borrado", accion="Deshacer", al_accion=deshacer)
#
# Es un aviso no modal que aparece en una esquina del formulario (por defecto, abajo a la derecha),
# se apila si hay varios, desaparece solo tras `duracion` ms, se cierra con un clic o con su «x» y no
# roba el foco. El aspecto es el del toast de la libsagui (userland/libsagui/gui_moderno.c, toast_*):
# fondo oscuro de la paleta mod_tema (dica_fundo / dica_texto) y un punto de color según el tipo.
# La libsagui no lo ofrece al módulo `sagui` de MicroPython (añadir nombres pide regenerar los qstr
# de MicroPython, D034), así que se dibuja aquí con un componente de tkinter (_ToastW).
#
# `Notificacion` es el control sin ventana que lo usa desde el diseñador (propiedades texto, tipo,
# duracion, posicion, titulo y accion; método mostrar(); eventos click, cerrado y accion).
import tkinter as tk
import vps
from vps import Control, _props

TIPOS_TOAST = ("info", "exito", "aviso", "error")
POSICIONES = ("abajo_derecha", "arriba_derecha", "abajo_centro", "arriba_centro")
# los colores del toast de la libsagui (TOAST_INFO, TOAST_OK, TOAST_AVISO, TOAST_ERRO) y su fondo
FONDO, TEXTO = 0x263238, 0xFFFFFF
COLORES = {"info": 0x3CC8CF, "exito": 0x3DD17F, "aviso": 0xF2B134, "error": 0xFF6B60}
MARGEN, SEPARACION = 10, 6
ANCHO_MIN, ANCHO_MAX = 200, 340
SANGRIA = 34            # donde empieza el texto (el punto de color queda a la izquierda)
BORDE_CIERRE = 26       # ancho de la zona de la «x», a la derecha


def _redondo(v, x, y, w, h, r, color):
    """Rectángulo con las esquinas redondeadas de radio r."""
    v.rect(x + r, y, w - 2 * r, h, color)
    v.rect(x, y + r, w, h - 2 * r, color)
    d = 2 * r
    v.elipse(x, y, d, d, color)
    v.elipse(x + w - d, y, d, d, color)
    v.elipse(x, y + h - d, d, d, color)
    v.elipse(x + w - d, y + h - d, d, d, color)


class _ToastW(tk.Widget):
    """El aviso dibujado. No toma el foco al pulsarlo (_foca_no_clique = False, takefocus = 0)."""
    _classe = "Toast"
    _foca_no_clique = False
    _padrao = {"width": 240, "height": 40, "takefocus": 0, "cursor": "", "highlightthickness": 0,
               "borderwidth": 0}

    def _iniciar(self):
        self.toast = None
        self.tipo = "info"
        self.titulo = ""
        self.lineas = []
        self.accion = ""
        self.bind("<Button-1>", self._pulsar)

    def _natural(self):
        return (tk._int(self._op.get("width", 240)), tk._int(self._op.get("height", 40)))

    def _rect_accion(self):
        """(x, y, w, h) del botón de acción, o None."""
        if not self.accion:
            return None
        w = len(self.accion) * tk.LETRA_L + 16
        return self._wl - 12 - w, self._wa - 28, w, 22

    def _pulsar(self, ev):
        t = self.toast
        if t is None:
            return
        r = self._rect_accion()
        if ev.x >= self._wl - BORDE_CIERRE and ev.y < 26:
            t.cerrar("boton")
        elif r is not None and r[0] <= ev.x < r[0] + r[2] and r[1] <= ev.y < r[1] + r[3]:
            t._accion()
        else:
            t._click()

    def _desenhar(self, v):
        x, y, w, h = self._ax, self._ay, self._wl, self._wa
        _redondo(v, x, y, w, h, 6, FONDO)
        color = COLORES.get(self.tipo, COLORES["info"])
        v.elipse(x + 12, y + 13, 12, 12, color)
        ty = y + 10
        if self.titulo:
            v.texto(x + SANGRIA, ty, self.titulo[:max(0, (w - SANGRIA - BORDE_CIERRE) // tk.LETRA_L)], color)
            ty += 18
        for l in self.lineas:
            v.texto(x + SANGRIA, ty, l, TEXTO)
            ty += tk.LETRA_A
        v.texto(x + w - 18, y + 10, "x", 0xB0BEC5)
        r = self._rect_accion()
        if r is not None:
            v.rect(x + r[0], y + r[1], r[2], r[3], color, relleno=False)
            v.texto(x + r[0] + 8, y + r[1] + (r[3] - tk.LETRA_A) // 2, self.accion, color)


_activos = {}        # id(formulario) -> [_Toast] (los más antiguos primero)


class _Toast:
    """Una notificación en pantalla. `cerrar()` la quita; `abierta` dice si sigue visible."""

    def __init__(self, form, texto, tipo, duracion, titulo, accion, posicion):
        self.form = form
        self.texto = str(texto)
        self.tipo = tipo
        self.duracion = int(duracion)
        self.titulo = str(titulo or "")
        self.accion = str(accion or "")
        self.posicion = posicion
        self.abierta = True
        self.motivo = ""          # por qué se cerró: tiempo, click, boton, accion o api
        self.al_click = None
        self.al_accion = None
        self.al_cerrar = None
        self._id = None
        self.w = _ToastW(form.win)
        self.w.toast = self
        self.w.tipo = tipo
        self.w.titulo = self.titulo
        self.w.accion = self.accion
        self._medir()
        if self.duracion > 0:
            self._id = tk._agendar(self.duracion, self._expira, ())

    def _medir(self):
        fw = form_ancho(self.form)
        util = max(120, min(ANCHO_MAX, fw - 2 * MARGEN))
        cab = SANGRIA + BORDE_CIERRE
        self.w.lineas = tk._quebrar(self.texto, util - cab)
        mayor = max([len(l) for l in self.w.lineas] + [len(self.titulo), len(self.accion) + 3])
        ancho = max(ANCHO_MIN, min(util, mayor * tk.LETRA_L + cab))
        # con un ancho menor que el pedido, se vuelve a quebrar
        self.w.lineas = tk._quebrar(self.texto, ancho - cab)
        alto = 14 + len(self.w.lineas) * tk.LETRA_A + (18 if self.titulo else 0) + (30 if self.accion else 0)
        self.ancho, self.alto = ancho, max(40, alto)

    def _expira(self):
        self._id = None
        if self.abierta:
            self.cerrar("tiempo")

    def _click(self):
        if self.al_click is not None:
            vps._llamar(self.al_click, "notificacion_click")
        self.cerrar("click")

    def _accion(self):
        if self.al_accion is not None:
            vps._llamar(self.al_accion, "notificacion_accion")
        self.cerrar("accion")

    def cerrar(self, motivo="api"):
        """Quita la notificación (y avisa a `al_cerrar`). No hace nada si ya estaba cerrada."""
        if not self.abierta:
            return
        self.abierta = False
        self.motivo = motivo
        if self._id is not None:
            tk._cancelar(self._id)
            self._id = None
        lista = _activos.get(id(self.form))
        if lista is not None and self in lista:
            lista.remove(self)
        try:
            self.w.destroy()
        except Exception:
            pass
        reflujo(self.form)
        if self.al_cerrar is not None:
            vps._llamar(self.al_cerrar, "notificacion_cerrado")


def form_ancho(form):
    """El ancho útil del formulario (el pedido si aún no se ha dibujado)."""
    try:
        w = form.win.winfo_width()
    except Exception:
        w = 1
    return w if w > 50 else int(form._p["ancho"])


def form_alto(form):
    try:
        h = form.win.winfo_height() - form.win._extra_topo()
    except Exception:
        h = 1
    return h if h > 50 else int(form._p["alto"])


def reflujo(form):
    """Coloca las notificaciones de cada esquina: la más nueva junto al borde y las anteriores,
    apiladas hacia dentro."""
    lista = _activos.get(id(form)) or []
    fw, fh = form_ancho(form), form_alto(form)
    usado = {}
    for t in reversed(lista):
        arriba = t.posicion.startswith("arriba")
        sitio = usado.get(t.posicion, MARGEN)
        x = (fw - t.ancho) // 2 if t.posicion.endswith("centro") else fw - t.ancho - MARGEN
        y = sitio if arriba else fh - sitio - t.alto
        usado[t.posicion] = sitio + t.alto + SEPARACION
        t.w.place(x=max(0, x), y=max(0, y), width=t.ancho, height=t.alto)
        t.w.lift()
    for t in lista:
        t.w._redesenhar()


def _formulario_activo():
    for f in reversed(vps._formularios):
        if not f._oculto and not f._cerrado and f._activo_f:
            return f
    for f in reversed(vps._formularios):
        if not f._oculto and not f._cerrado:
            return f
    return None


def notificar(texto, tipo="info", duracion=3000, titulo="", accion="", posicion="abajo_derecha",
              formulario=None, al_click=None, al_accion=None, al_cerrar=None):
    """Enseña un aviso no modal en una esquina del formulario (por defecto, el activo). tipo: info,
    exito, aviso o error. `duracion` en ms (0 = hasta que se cierre). Con `accion` lleva un botón
    (p. ej. «Deshacer») que llama a `al_accion`. Devuelve la notificación (`.cerrar()`, `.abierta`)."""
    if tipo not in TIPOS_TOAST:
        raise ValueError("tipo de notificación desconocido: %s (info, exito, aviso o error)" % (tipo,))
    if posicion not in POSICIONES:
        raise ValueError("posición desconocida: %s" % (posicion,))
    f = formulario if formulario is not None else _formulario_activo()
    if f is None or f._cerrado:
        return None
    t = _Toast(f, texto, tipo, duracion, titulo, accion, posicion)
    t.al_click, t.al_accion, t.al_cerrar = al_click, al_accion, al_cerrar
    _activos.setdefault(id(f), []).append(t)
    reflujo(f)
    return t


def notificaciones(formulario=None):
    """Las notificaciones que se ven ahora en el formulario (más antiguas primero)."""
    f = formulario if formulario is not None else _formulario_activo()
    return list(_activos.get(id(f)) or [])


def cerrar_notificaciones(formulario=None):
    """Cierra todas las notificaciones del formulario."""
    for t in notificaciones(formulario):
        t.cerrar("api")


@_props
class Notificacion(Control):
    """Sin ventana: `mostrar()` enseña una notificación con `texto`, `tipo`, `duracion`, `posicion`,
    `titulo` y (si hay) un botón `accion`. Eventos: `click` (al pulsar el aviso), `accion` (al pulsar el
    botón) y `cerrado` (al irse: `motivo` dice por qué)."""
    # `tipo` es a la vez la propiedad (info, exito, aviso, error) y el nombre del tipo de control: la
    # propiedad vive en el objeto, no en la clase (ver _crear)
    tipo = "Notificacion"
    PROPS = (("texto", "str", ""), ("tipo", "enum:info|exito|aviso|error", "info"), ("duracion", "int", 3000),
             ("posicion", "enum:abajo_derecha|arriba_derecha|abajo_centro|arriba_centro", "abajo_derecha"),
             ("titulo", "str", ""), ("accion", "str", ""))
    EVENTOS = ("click", "accion", "cerrado")
    EVENTO_DEFECTO = "click"
    ORDEN = ("texto", "tipo", "duracion")
    TAM = (32, 32)
    motivo = ""        # por qué se cerró la última notificación (en el evento cerrado)
    ultima = None      # la última notificación que enseñó

    def _crear(self):
        self.tipo = self._p["tipo"]      # el atributo del objeto tapa al de la clase

    def _colocar(self):
        pass

    def _escribir(self, n, v):
        self._p[n] = v
        self._aplicar(n)

    def _aplicar(self, n):
        pass

    def _leer(self, n):
        if n == "tipo":
            return self.tipo
        return self._p.get(n)

    def mostrar(self, texto=None):
        """Enseña la notificación (con `texto`, si se da, en lugar del de la propiedad)."""
        if texto is not None:
            self._p["texto"] = str(texto)
        t = notificar(self._p["texto"], self.tipo, int(self._p["duracion"]), self._p["titulo"], self._p["accion"],
                      self._p["posicion"], self.form, self._pulsada, self._accionada, self._cerrada)
        self.ultima = t
        return t

    def _pulsada(self):
        self._disparar("click")

    def _accionada(self):
        self._disparar("accion")

    def _cerrada(self):
        t = self.ultima
        self.motivo = t.motivo if t is not None else ""
        self._disparar("cerrado")

    def cerrar(self):
        """Cierra la última notificación de este control, si sigue a la vista."""
        if self.ultima is not None:
            self.ultima.cerrar("api")

    def destruir(self):
        self.cerrar()
        self.form.controles.pop(self.nombre, None)


TIPOS = {"Notificacion": Notificacion}
METODOS = {"Notificacion": [("mostrar", "mostrar(texto=None)", "Enseña la notificación (con ese texto, si se da)"),
                            ("cerrar", "cerrar()", "Cierra la última notificación de este control")]}
