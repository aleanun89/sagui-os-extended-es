# SPDX-License-Identifier: GPL-2.0-or-later
# vps.autoprueba_grafico - comprobaciones del componente de gráficos (D469) dentro de SaguiOS, sin ratón real:
# el módulo de C `sagui_grafico` (escalas, formato, tipos, colores por píxeles, animación, tooltip, clic,
# leyenda, datos en vivo, exportar) y el control Grafico de VisualPythonS (propiedades, métodos, eventos,
# .vpf, ejemplo). Escribe «vps-autoprueba: ok ...» o «FALLO ...». Lo usa tests/h469_grafico.py.
#
#     python /lib/python/vps/autoprueba_grafico.py
import sagui
import sagui_grafico as gr
import vps
from vps import proyecto

AZUL, NARANJA, VERDE, ROJO = 0x2A6FD8, 0xE5832B, 0x1E9E6B, 0xC8453C


def comprobar(cond, msg):
    print("vps-autoprueba: " + ("ok " if cond else "FALLO ") + msg)


def contar(img, color, l, a, paso=2):
    n = 0
    for y in range(0, a, paso):
        for x in range(0, l, paso):
            if img.pixel(x, y) == color:
                n += 1
    return n


def nuevo(tipo, series, cats=None, anim=0, l=320, a=220, **opc):
    g = gr.Grafico()
    g.tipo(tipo)
    g.opcion("animado", anim)
    if cats:
        g.categorias(cats)
    for n, d in series:
        g.agregar_serie(n, d)
    for k, v in opc.items():
        g.opcion(k, v)
    img = sagui.Imagen.nueva(l, a, 0xFFFFFF)
    return g, img


def calculo():
    comprobar(gr.escala(0, 97, 6) == (0.0, 100.0, 20.0, 6), "calculo: escala 1-2-5 de 0..97")
    comprobar(gr.formatear(1234567, "#,##0") == "1.234.567" and gr.formatear(0.256, "0.0%") == "25,6%", "calculo: formato de números")
    comprobar(gr.color_serie(0) == AZUL and gr.color_serie(0, True) != AZUL, "calculo: paleta clara y oscura")


def tipos():
    L, A = 320, 220
    g, img = nuevo("barras", [("a", [10, 20, 30]), ("b", [15, 25, 5])], ["x", "y", "z"], l=L, a=A)
    g.dibujar(img, L, A)
    na, nb = contar(img, AZUL, L, A), contar(img, NARANJA, L, A)
    comprobar(na > 150 and nb > 150, "barras: las dos series se dibujan con los colores de la paleta (%d, %d)" % (na, nb))
    g.opcion("apilado", 1)
    g.dibujar(img, L, A)
    comprobar(contar(img, AZUL, L, A) > 150, "barras apiladas se dibujan")
    for tipo in ("barras_h", "lineas", "area", "histograma", "dispersion"):
        g, img = nuevo(tipo, [("a", [3, 8, 5, 9, 4, 7, 6]), ("b", [1, 4, 2, 6, 3, 5, 2])], l=L, a=A)
        if tipo == "dispersion":
            g.quitar_series()
            g.agregar_serie("p", [3, 8, 5], 0, [1, 2, 3], [1, 5, 9])
        g.dibujar(img, L, A)
        comprobar(contar(img, AZUL, L, A, 1) > 20, "%s: dibuja la serie en azul" % tipo)
    g, img = nuevo("velas", [("a", [5, 6, 7]), ("m", [8, 9, 9]), ("n", [4, 5, 6]), ("c", [7, 5, 8])], ["l", "m", "x"], l=L, a=A)
    g.dibujar(img, L, A)
    comprobar(contar(img, VERDE, L, A, 1) > 20 and contar(img, ROJO, L, A, 1) > 10, "velas: verde si sube y roja si baja")
    g, img = nuevo("tarta", [("r", [40, 25, 20, 15])], ["N", "S", "E", "O"], l=L, a=A)
    g.dibujar(img, L, A)
    cols = [contar(img, c, L, A) for c in (AZUL, NARANJA, VERDE, ROJO)]
    comprobar(min(cols) > 100 and cols[0] > cols[1] > cols[3], "tarta: cuatro porciones con tamaños proporcionales %s" % cols)
    g.tipo("anillo")
    g.dibujar(img, L, A)
    cols2 = [contar(img, c, L, A) for c in (AZUL, NARANJA, VERDE, ROJO)]
    comprobar(min(cols2) > 50 and sum(cols2) < sum(cols), "anillo: las porciones se dibujan y dejan un hueco %s" % cols2)


def animacion():
    L, A = 320, 220
    g, img = nuevo("barras", [("a", [10, 20, 30])], ["x", "y", "z"], anim=1, l=L, a=A)
    g.opcion("duracion", 400)
    g.avanzar(1000)
    g.dibujar(img, L, A)
    n0 = contar(img, AZUL, L, A)
    g.avanzar(1150)
    g.dibujar(img, L, A)
    n1 = contar(img, AZUL, L, A)
    g.avanzar(1500)
    g.dibujar(img, L, A)
    n2 = contar(img, AZUL, L, A)
    comprobar(n0 < n1 < n2, "animación: las barras crecen con el tiempo (%d, %d, %d)" % (n0, n1, n2))
    comprobar(g.avanzar(1500) is False, "animación: termina y avanzar() devuelve False")
    # cambio de datos: de los valores que se veían a los nuevos
    g.datos(0, [30, 10, 5])
    g.avanzar(2000)
    g.avanzar(2100)
    comprobar(g.avanzar(2100) is True, "animación: al cambiar los datos vuelve a animar")
    g.avanzar(2600)
    comprobar(g.valor(0, 0) == 30.0 and g.avanzar(2600) is False, "animación: el cambio termina en los datos nuevos")
    g2, img2 = nuevo("barras", [("a", [10, 20])], anim=0)
    comprobar(g2.avanzar(5) is False, "animación: con animado=0 no hay nada que animar")


def interaccion():
    L, A = 320, 220
    g, img = nuevo("barras", [("Ventas", [10, 20, 30])], ["Ene", "Feb", "Mar"], l=L, a=A)
    g.dibujar(img, L, A)
    # la barra más alta (Mar) está en el último tercio del área de dibujo, cerca del borde de abajo del eje
    hallado = None
    for x in range(40, L - 10, 6):
        for y in range(A - 60, 20, -6):
            if g.mover(x, y) or g.resaltado():
                if g.resaltado() and g.resaltado()[1] == 2:
                    hallado = (x, y)
                    break
        if hallado:
            break
    comprobar(hallado is not None, "ratón: se resalta la barra de Mar al pasar por encima")
    g.dibujar(img, L, A)
    t = g.tooltip()
    comprobar("Ventas" in t and "Mar" in t and "30" in t, "ratón: tooltip con serie, categoría y valor: %r" % t)
    r = g.pulsar(hallado[0], hallado[1]) if hallado else None
    comprobar(r is not None and r[0] == 0 and r[1] == 2 and r[2] == 30.0 and not r[3], "ratón: el clic da serie, índice y valor %s" % (r,))
    g.salir()
    comprobar(g.resaltado() is None and g.tooltip() == "", "ratón: al salir se quita el resalte")
    # leyenda: oculta la serie
    g2, img2 = nuevo("barras", [("uno", [10, 20]), ("dos", [5, 8])], ["a", "b"], l=L, a=A)
    g2.dibujar(img2, L, A)
    ocultada = False
    for x in range(60, 260, 4):
        r = g2.pulsar(x, A - 18)
        if r is not None and r[3]:
            ocultada = True
            break
    g2.dibujar(img2, L, A)
    comprobar(ocultada and g2.oculto(r[0]), "leyenda: pulsarla oculta la serie %s" % (r,))
    comprobar(contar(img2, NARANJA, L, A) < 5 or r[0] == 0, "leyenda: la serie oculta desaparece del dibujo")


def vivo():
    L, A = 300, 200
    g, img = nuevo("lineas", [("c", [])], l=L, a=A, ventana=5)
    for v in (1, 2, 3, 4, 5, 6, 7):
        g.agregar_punto(0, v)
    comprobar(g.n_puntos(0) == 5 and g.valor(0, 0) == 3.0 and g.valor(0, 4) == 7.0, "en vivo: la ventana deslizante conserva los últimos 5")
    g.dibujar(img, L, A)
    comprobar(contar(img, AZUL, L, A, 1) > 20, "en vivo: la línea se dibuja")


def exportar():
    g, img = nuevo("lineas", [("a", [1, 3, 2])], ["x", "y", "z"], anim=1)
    g.exportar("/tmp/grafico-aut.png", 400, 250)
    g.exportar("/tmp/grafico-aut.bmp", 400, 250)
    f = open("/tmp/grafico-aut.png", "rb")
    cab = f.read(8)
    f.close()
    comprobar(cab == b"\x89PNG\r\n\x1a\n", "exportar: PNG válido")
    f = open("/tmp/grafico-aut.bmp", "rb")
    cab = f.read(2)
    f.close()
    comprobar(cab == b"BM", "exportar: BMP válido")
    im = sagui.Imagen.cargar("/tmp/grafico-aut.png")
    comprobar(im.ancho == 400 and im.alto == 250 and contar(im, AZUL, 400, 250, 1) > 20, "exportar: el PNG se lee y trae la línea en azul")


def control():
    f = vps.Formulario("Gráficos", 420, 320, nombre="FormG")
    g1 = vps.Grafico(f, 8, 8, 300, 200, tipo="barras", titulo="Ventas", categorias=["Ene", "Feb", "Mar"],
                     series=["2026: 3; 5; 2", "2025: 1; 4; 6"], nombre="G1")
    comprobar(g1.tipo == "barras" and g1.series == ["2026: 3; 5; 2", "2025: 1; 4; 6"], "control: tipo y series como texto")
    comprobar(len(g1._ser) == 2 and g1.g.n_series() == 2, "control: las series llegan al componente de C")
    r = []
    g1.conectar("click", lambda: r.append((g1.serie, g1.indice, g1.valor, g1.categoria)))
    g1.conectar("leyenda", lambda: r.append(("ley", g1.serie, g1.oculta)))
    g1.w._redesenhar()
    g1.agregar_serie("2024", [2, 2, 2])
    comprobar(g1.g.n_series() == 3 and g1.valores_de(2) == [2.0, 2.0, 2.0], "control: agregar_serie")
    g1.poner_datos([[1, 1, 1], [2, 2, 2], [3, 3, 3]])
    comprobar(g1.valores_de(0) == [1.0, 1.0, 1.0], "control: poner_datos con varias series")
    g1.tipo = "tarta"
    comprobar(g1._sincronizar() and g1.g.tipo() == "tarta", "control: cambiar tipo por código")
    g1.tipo = "lineas"
    g1._sincronizar()
    g1.ventana = 4
    for v in range(7):
        g1.agregar_punto(v, 1)
    comprobar(len(g1.valores_de(1)) == 4 and g1.valores_de(1)[-1] == 6.0, "control: agregar_punto con ventana")
    g1.exportar("/tmp/grafico-ctl.png", 300, 200)
    comprobar(open("/tmp/grafico-ctl.png", "rb").read(4) == b"\x89PNG", "control: exportar(ruta)")
    g1.limpiar()
    comprobar(g1.g.n_series() == 0 and g1.series == [], "control: limpiar")
    # .vpf y construcción
    modelo = [{"nombre": "Formulario", "props": {"texto": "x", "ancho": 200, "alto": 200}, "controles": [
        {"tipo": "Grafico", "nombre": "Grafico1", "padre": None,
         "props": {"x": 4, "y": 4, "ancho": 180, "alto": 150, "tipo": "tarta", "series": ["R: 1; 2; 3"], "valores": True}}]}]
    txt = proyecto.escribir_vpf(modelo)
    comprobar("control Grafico Grafico1" in txt and "tipo = 'tarta'" in txt, "vpf: el control y su tipo se guardan")
    vps.salir()
    ns = proyecto.ejecutar_proyecto("/ejemplos/vps/graficos", solo_construir=True)
    comprobar(all(n in ns for n in ("Grafico1", "Grafico2", "Grafico3", "Grafico4")) and ns["Grafico3"].tipo == "tarta", "ejemplo: construye los cuatro gráficos")
    ns["Temporizador1_tic"]()
    ns["Temporizador1_tic"]()
    comprobar(ns["Grafico4"].valores_de(0)[-2:] == [12.0, 18.0], "ejemplo: el temporizador alimenta el gráfico en vivo")
    ns["Grafico1"]._dar(1, 2, 240.0)
    ns["Grafico1_click"]()
    comprobar("2026" in ns["Etiqueta1"].texto and "240" in ns["Etiqueta1"].texto, "ejemplo: el clic escribe serie, categoría y valor")
    ns["Boton2_click"]()
    comprobar(ns["Grafico1"].valores_de(1)[0] == 90.0, "ejemplo: «Nuevos datos» cambia la serie")
    vps.salir()
    # autocompletado
    from vps import _autocompletar as AC
    comprobar("agregar_punto" in [m[0] for m in AC.METODOS.get("Grafico", [])] and "categoria" in AC.EXTRA_PROPS.get("Grafico", []), "autocompletado: métodos y datos del evento")
    comprobar("Grafico" in vps.TIPOS and "Grafico" in vps.__all__, "vps.TIPOS y __all__ llevan Grafico")


def principal():
    calculo()
    tipos()
    animacion()
    interaccion()
    vivo()
    exportar()
    control()
    print("vps-autoprueba: fin")


if __name__ == "__main__":
    principal()
