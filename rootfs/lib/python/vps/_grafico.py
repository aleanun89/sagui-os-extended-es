# SPDX-License-Identifier: GPL-2.0-or-later
# vps._grafico - el control Grafico de VisualPythonS (v8.5.6, D469): gráficos de líneas, áreas, barras,
# tarta, anillo, dispersión, histograma y velas, con animación, tooltip, leyenda interactiva y datos en vivo.
#
#     Grafico1 = Grafico(f, 8, 8, 360, 220, tipo="barras", titulo="Ventas")
#     Grafico1.categorias = ["Ene", "Feb", "Mar"]
#     Grafico1.agregar_serie("2026", [3, 5, 2])
#     def Grafico1_click(): Etiqueta1.texto = "%s: %s" % (Grafico1.categoria, Grafico1.valor)
#
# El dibujo es el del componente de C de la libsagui (userland/libsagui/gui_grafico.c), usado por el módulo
# `sagui_grafico` de MicroPython; el widget que lo enseña es vps._widgets._GraficoW. Aquí está el modelo
# (series como texto para el diseñador: una línea por serie, «nombre: 1; 2; 3»), los métodos y los eventos.
from vps import Control, _props, _ev, _color_int

TIPOS_GRAFICO = "lineas|area|barras|barras_h|tarta|anillo|dispersion|histograma|velas"
LEYENDAS = "ninguna|abajo|arriba|derecha|izquierda"


def _num(t):
    t = str(t).strip().replace(",", ".")
    return float(t)


def parsear_serie(linea, tipo):
    """«nombre: 1; 2; 3» -> (nombre, datos, xs, tam). En dispersión cada punto es «x,y» o «x,y,tamaño»."""
    nombre, sep, resto = str(linea).partition(":")
    if not sep:
        nombre, resto = "", nombre
    nombre = nombre.strip()
    datos, xs, tam = [], None, None
    for item in resto.split(";"):
        item = item.strip()
        if not item:
            continue
        if tipo == "dispersion":
            p = item.split(",")
            if len(p) >= 2:
                if xs is None:
                    xs, tam = [], []
                xs.append(_num(p[0]))
                datos.append(_num(p[1]))
                tam.append(_num(p[2]) if len(p) > 2 else 0)
                continue
        datos.append(_num(item))
    return nombre, datos, xs, tam


def _txt(v):
    s = "%g" % v
    return s


def serie_a_texto(s, tipo):
    if tipo == "dispersion" and s.get("xs") is not None:
        t = []
        for i, y in enumerate(s["datos"]):
            p = "%s,%s" % (_txt(s["xs"][i]), _txt(y))
            if s["tam"] and s["tam"][i]:
                p += "," + _txt(s["tam"][i])
            t.append(p)
        cuerpo = "; ".join(t)
    else:
        cuerpo = "; ".join(_txt(y) for y in s["datos"])
    return "%s: %s" % (s["nombre"], cuerpo)


@_props
class Grafico(Control):
    """Gráfico. `tipo` es lineas, area, barras, barras_h, tarta, anillo, dispersion, histograma o velas.
    `series` (una línea por serie, «nombre: 1; 2; 3»; en velas, cuatro: apertura, máximo, mínimo y cierre),
    `categorias`, `leyenda`, `rejilla`, `animado`, `apilado`, `suave`, `puntos`, `valores`, `formato`
    («0», «0.0», «#,##0», «0%», «$0.00», «auto»), `colores` y más. Métodos: `agregar_serie`, `poner_datos`,
    `agregar_punto` (datos en vivo con ventana deslizante), `limpiar`, `ocultar_serie`, `exportar(ruta)`.
    Eventos: `click` en una barra, punto o porción (`serie`, `indice`, `valor`, `categoria`), `pasar` al
    resaltar un elemento con el ratón y `leyenda` al ocultar o mostrar una serie pulsando la leyenda."""
    # `tipo` es a la vez la propiedad (el tipo de gráfico) y el nombre del tipo de control, como en
    # Notificacion (D466): la propiedad vive en el objeto y un vigilante del widget aplica los cambios
    tipo = "Grafico"
    PROPS = (("tipo", "enum:" + TIPOS_GRAFICO, "barras"), ("titulo", "str", ""), ("series", "lista", []),
             ("categorias", "lista", []), ("leyenda", "enum:" + LEYENDAS, "abajo"), ("rejilla", "bool", True),
             ("animado", "bool", True), ("apilado", "bool", False), ("suave", "bool", False), ("puntos", "bool", True),
             ("valores", "bool", False), ("formato", "str", "auto"), ("colores", "lista", []),
             ("eje_x", "str", ""), ("eje_y", "str", ""), ("ventana", "int", 0), ("duracion", "int", 450),
             ("hueco", "int", 55), ("bins", "int", 0), ("minimo", "str", ""), ("maximo", "str", ""))
    EVENTOS = _ev("click", "pasar", "leyenda")
    EVENTO_DEFECTO = "click"
    ORDEN = ("x", "y", "ancho", "alto")
    TAM = (360, 240)
    # datos del último evento
    serie = -1
    indice = -1
    valor = 0.0
    categoria = ""
    nombre_serie = ""
    oculta = False
    g = None

    def _crear(self):
        import sagui_grafico as _gr     # el módulo de C (aquí, para poder importar vps en el PC)
        from vps import _widgets
        self.tipo = self._p["tipo"]          # el atributo del objeto tapa al de la clase
        self._ser = []                       # [{"nombre", "datos", "xs", "tam"}]
        self._ult = None
        self.g = _gr.Grafico()
        self.w = _widgets._GraficoW(self._contenedor_tk())
        self.w.ctl = self

    # --- propiedades
    def _leer(self, n):
        if n == "tipo":
            return self.tipo
        if n == "series":
            return [serie_a_texto(s, self._p["tipo"]) for s in self._ser]
        return self._p.get(n)

    def _escribir(self, n, v):
        if n == "tipo":
            self.tipo = v
        self._p[n] = v
        if self.w is not None:
            self._aplicar(n)

    def _sincronizar(self):
        """Aplica un cambio de `tipo` hecho como atributo (Grafico1.tipo = "tarta"). True si cambió."""
        if self.tipo != self._p["tipo"]:
            self._p["tipo"] = self.tipo
            self._aplicar("tipo")
            return True
        return False

    def _aplicar_todo(self):
        self._colocar()
        orden = ("tipo", "categorias", "series")
        for n in orden:
            self._aplicar(n)
        for n in self._p:
            if n not in orden and n not in ("x", "y", "ancho", "alto", "visible", "pagina"):
                self._aplicar(n)

    def _opc(self, nombre, valor):
        self.g.opcion(nombre, valor)

    def _aplicar(self, n):
        p = self._p
        g = self.g
        if g is None:
            return
        if n == "tipo":
            g.tipo(str(p["tipo"]))
        elif n == "titulo":
            g.titulo(p["titulo"])
        elif n == "series":
            self._cargar_series(p["series"])
        elif n == "categorias":
            g.categorias([str(c) for c in p["categorias"]])
        elif n == "leyenda":
            self._opc("leyenda", str(p["leyenda"]))
        elif n in ("rejilla", "animado", "apilado", "suave", "puntos", "valores"):
            self._opc(n, 1 if p[n] else 0)
        elif n in ("ventana", "duracion", "hueco", "bins"):
            self._opc(n, int(p[n] or 0))
        elif n == "formato":
            g.formato(str(p["formato"] or "auto"))
        elif n == "colores":
            self._aplicar_colores()
        elif n in ("eje_x", "eje_y"):
            g.ejes(p["eje_x"], p["eje_y"])
        elif n in ("minimo", "maximo"):
            try:
                g.rango(_num(p["minimo"]), _num(p["maximo"]))
            except ValueError:
                g.rango(None, None)
        elif n == "colorfondo":
            pass
        else:
            Control._aplicar(self, n)
            return
        self.w._redesenhar()

    def _aplicar_fondo(self):
        pass

    def _cargar_series(self, lineas):
        self._ser = []
        self.g.quitar_series()
        tipo = str(self._p["tipo"])
        for l in lineas or []:
            if not str(l).strip():
                continue
            try:
                nombre, datos, xs, tam = parsear_serie(l, tipo)
            except ValueError:
                continue
            self._agregar(nombre or "Serie%d" % (len(self._ser) + 1), datos, xs, tam)
        self._aplicar_colores()

    def _aplicar_colores(self):
        for i, c in enumerate(self._p["colores"] or []):
            self.g.color(i, _color_int(c, 0))

    def _agregar(self, nombre, datos, xs=None, tam=None, color=0):
        i = self.g.agregar_serie(nombre, datos, color, xs, tam)
        self._ser.append({"nombre": nombre, "datos": list(datos), "xs": list(xs) if xs else None,
                          "tam": list(tam) if tam else None})
        return i

    # --- métodos
    def agregar_serie(self, nombre, datos=None, color=None, xs=None, tam=None):
        """Añade una serie (en dispersión, `xs` son las x y `tam` los tamaños). Devuelve su índice."""
        i = self._agregar(str(nombre), list(datos or []), xs, tam, _color_int(color, 0))
        self.w._redesenhar()
        return i

    def poner_datos(self, datos, serie=0):
        """Cambia los datos de una serie (con animación). Si `datos` es una lista de listas, cambia todas las series."""
        if datos and isinstance(datos[0], (list, tuple)):
            for i, d in enumerate(datos):
                self.poner_datos(d, i)
            return
        while serie >= len(self._ser):
            self._agregar("Serie%d" % (len(self._ser) + 1), [])
        datos = [float(v) for v in datos]
        self.g.datos(serie, datos)
        s = self._ser[serie]
        s["datos"] = datos
        s["xs"] = None
        s["tam"] = None
        self.w._redesenhar()

    def agregar_punto(self, valor, serie=0, x=None, tam=0):
        """Datos en vivo: añade un punto al final de la serie (con `ventana` > 0, el más antiguo se pierde)."""
        while serie >= len(self._ser):
            self._agregar("Serie%d" % (len(self._ser) + 1), [])
        s = self._ser[serie]
        if x is None:
            self.g.agregar_punto(serie, float(valor))
        else:
            self.g.agregar_punto(serie, float(valor), float(x), float(tam))
            if s["xs"] is None:
                s["xs"] = list(range(len(s["datos"])))
                s["tam"] = [0] * len(s["datos"])
            s["xs"].append(x)
            s["tam"].append(tam)
        s["datos"].append(float(valor))
        v = int(self._p["ventana"] or 0)
        while v > 0 and len(s["datos"]) > v:
            del s["datos"][0]
            if s["xs"] is not None:
                del s["xs"][0]
                del s["tam"][0]
        self.w._redesenhar()

    def limpiar(self):
        """Quita las series y las categorías."""
        self._ser = []
        self.g.limpiar()
        self._p["categorias"] = []
        self.w._redesenhar()

    def ocultar_serie(self, serie, oculta=True):
        """Oculta (o muestra) una serie, o una porción en tarta y anillo."""
        self.g.ocultar(serie, oculta)
        self.w._redesenhar()

    def serie_oculta(self, serie):
        return self.g.oculto(serie)

    def reiniciar_animacion(self):
        """Vuelve a hacer la animación de entrada."""
        self.g.reiniciar()
        self.w._redesenhar()

    def valores_de(self, serie=0):
        """Los datos de una serie."""
        return list(self._ser[serie]["datos"]) if 0 <= serie < len(self._ser) else []

    def exportar(self, ruta, ancho=None, alto=None):
        """Guarda el gráfico en un .png o .bmp (con el estado final de la animación)."""
        self.g.exportar(str(ruta), int(ancho or self._p["ancho"]), int(alto or self._p["alto"]))

    def tooltip(self):
        """El texto del tooltip que se ve ahora («Serie\\nCategoría: valor»), o ""."""
        return self.g.tooltip()

    # --- el ratón (los llama _GraficoW)
    def _dar(self, s, i, valor=None):
        self.serie, self.indice = s, i
        sr = self._ser[s] if 0 <= s < len(self._ser) else None
        if valor is None:
            valor = sr["datos"][i] if sr is not None and 0 <= i < len(sr["datos"]) else 0.0
        self.valor = valor
        self.nombre_serie = sr["nombre"] if sr is not None else ""
        cats = self._p["categorias"] or []
        self.categoria = str(cats[i]) if 0 <= i < len(cats) else str(i + 1)

    def _hover(self, h):
        if h == self._ult:
            return
        self._ult = h
        if h is not None and self._hay("pasar"):
            self._dar(h[0], h[1])
            self._disparar("pasar")

    def _pulsado(self, x, y):
        r = self.g.pulsar(x, y)
        if r is None:
            return
        s, i, v, ley = r
        self.raton_x, self.raton_y = x, y
        if ley:
            self._dar(s, i, 0.0)
            self.oculta = self.g.oculto(i if self._p["tipo"] in ("tarta", "anillo") else s)
            self._disparar("leyenda")
        else:
            self._dar(s, i, v)
            self._disparar("click")

    def destruir(self):
        if self.g is not None:
            self.g.cerrar()
            self.g = None


TIPOS = {"Grafico": Grafico}
METODOS = {"Grafico": [("agregar_serie", "agregar_serie(nombre, datos=None, color=None, xs=None, tam=None)", "Añade una serie; devuelve su índice"),
                       ("poner_datos", "poner_datos(datos, serie=0)", "Cambia los datos de una serie (o de todas con una lista de listas), con animación"),
                       ("agregar_punto", "agregar_punto(valor, serie=0, x=None, tam=0)", "Añade un punto al final (datos en vivo; con ventana, desplaza)"),
                       ("limpiar", "limpiar()", "Quita las series y las categorías"),
                       ("ocultar_serie", "ocultar_serie(serie, oculta=True)", "Oculta o muestra una serie (o una porción)"),
                       ("serie_oculta", "serie_oculta(serie)", "Si la serie está oculta"),
                       ("reiniciar_animacion", "reiniciar_animacion()", "Repite la animación de entrada"),
                       ("valores_de", "valores_de(serie=0)", "Los datos de una serie"),
                       ("exportar", "exportar(ruta, ancho=None, alto=None)", "Guarda el gráfico en un .png o .bmp"),
                       ("tooltip", "tooltip()", "El texto del tooltip que se ve ahora")]}
