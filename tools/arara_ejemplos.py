#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Genera los ejemplos de Arara (D465) en rootfs/ejemplos/arara/: un diagrama de flujo, un diagrama de red y un
organigrama (datos sintéticos). Escribe el mismo formato `.arara` que la aplicación (JSON, una figura por línea)."""
import json
import os

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEST = os.path.join(RAIZ, "rootfs/ejemplos/arara")

# tamaño y colores por defecto de cada tipo (igual que TIPOS[] de arara_modelo.h)
TIPOS = {
    "rect": (112, 64, "#DCEBFA", "#2F5F8F"), "redond": (112, 64, "#E3F2E1", "#3F7F3F"),
    "elipse": (112, 64, "#FDE8D0", "#B5651D"), "rombo": (112, 80, "#FFF2B8", "#9A7B00"),
    "triangulo": (96, 80, "#F3DDF3", "#8A3F8A"), "texto": (112, 32, None, None),
    "inicio": (112, 48, "#D5F0DD", "#2E7D4B"), "proceso": (112, 64, "#DCEBFA", "#2F5F8F"),
    "decision": (128, 80, "#FFF2B8", "#9A7B00"), "es": (128, 48, "#E8E0F5", "#5B3F9A"),
    "documento": (112, 64, "#FFFFFF", "#555555"), "bd": (80, 96, "#D6E4F0", "#35566F"),
    "pc": (80, 80, "#C9D6E2", "#3E4C59"), "servidor": (80, 96, "#C9D6E2", "#3E4C59"),
    "router": (96, 64, "#BFD9F2", "#2F5F8F"), "nube": (112, 80, "#E6F1FB", "#5B8DB8"),
    "impresora": (80, 80, "#D9D9D9", "#444444"), "persona": (176, 64, "#FFFFFF", "#2F5F8F"),
}
N, E, S, O = 0, 1, 2, 3


class Pagina:
    def __init__(self, nombre):
        self.nombre, self.figs, self.cons, self.sig = nombre, [], [], 1

    def fig(self, tipo, x, y, texto="", l=None, a=None, relleno=None):
        dl, da, rel, lin = TIPOS[tipo]
        f = {"id": self.sig, "tipo": tipo, "x": x, "y": y, "l": l or dl, "a": a or da,
             "relleno": (relleno or rel or "#00FFFFFF").lower(), "linea": (lin or "#00000000").lower(), "grosor": 1 if tipo == "texto" else 2, "texto": texto}
        self.sig += 1
        self.figs.append(f)
        return f["id"]

    def con(self, de, pde, a, pa, tipo="recto", ini=False, fin=True):
        c = {"id": self.sig, "tipo": tipo, "de": de, "pto_de": pde, "a": a, "pto_a": pa,
             "flecha_ini": ini, "flecha_fin": fin, "linea": "#2f4f6f", "grosor": 2}
        self.sig += 1
        self.cons.append(c)


def linea(d):
    return "{ " + ", ".join('"%s": %s' % (k, json.dumps(v, ensure_ascii=False)) for k, v in d.items()) + " }"


def guardar(nombre, paginas):
    out = ['{', '  "arara": 1,', '  "paginas": [']
    for i, p in enumerate(paginas):
        out += ['    {', '      "nombre": %s,' % json.dumps(p.nombre, ensure_ascii=False), '      "figuras": [']
        out += ["        " + linea(f) + ("," if k + 1 < len(p.figs) else "") for k, f in enumerate(p.figs)]
        out += ['      ],', '      "conectores": [']
        out += ["        " + linea(c) + ("," if k + 1 < len(p.cons) else "") for k, c in enumerate(p.cons)]
        out += ['      ]', '    }' + ("," if i + 1 < len(paginas) else "")]
    out += ['  ]', '}']
    os.makedirs(DEST, exist_ok=True)
    with open(os.path.join(DEST, nombre), "w", encoding="utf-8") as fh:
        fh.write("\n".join(out) + "\n")


def flujo():
    p = Pagina("Pedido")
    ini = p.fig("inicio", 184, 16, "Inicio")
    leer = p.fig("es", 176, 96, "Leer los datos del pedido")
    dec = p.fig("decision", 184, 192, "¿Datos válidos?")
    err = p.fig("documento", 400, 200, "Mostrar el error")
    proc = p.fig("proceso", 184, 320, "Procesar el pedido")
    bd = p.fig("bd", 200, 416, "Pedidos")
    fin = p.fig("inicio", 400, 336, "Fin")
    p.fig("texto", 312, 192, "No", l=48, a=32)
    p.fig("texto", 248, 280, "Sí", l=48, a=32)
    p.con(ini, S, leer, N)
    p.con(leer, S, dec, N)
    p.con(dec, E, err, O)
    p.con(dec, S, proc, N)
    p.con(proc, S, bd, N)
    p.con(proc, E, fin, O)
    p.con(err, N, leer, E, "angulo")
    q = Pagina("Notas")
    q.fig("texto", 32, 32, "Diagrama de flujo de ejemplo de Arara.\nLas figuras se conectan por sus puntos de conexión.", l=384, a=48)
    q.fig("redond", 32, 112, "Mueve una figura:\nlos conectores la siguen")
    guardar("flujo.arara", [p, q])


def red():
    p = Pagina("Red de la oficina")
    nube = p.fig("nube", 224, 16, "Internet", l=128, a=96)
    rt = p.fig("router", 240, 160, "Router")
    sv = p.fig("servidor", 64, 320, "Servidor")
    pcs = [p.fig("pc", 208 + 112 * i, 320, "PC %d" % (i + 1)) for i in range(3)]
    imp = p.fig("impresora", 544, 320, "Impresora")
    p.con(nube, S, rt, N)
    p.con(rt, O, sv, N, "angulo", fin=False)
    for pc in pcs:
        p.con(rt, S, pc, N, "angulo", fin=False)
    p.con(rt, E, imp, N, "angulo", fin=False)
    guardar("red.arara", [p])


def organigrama():
    p = Pagina("Empresa")
    d = p.fig("persona", 208, 16, "Dirección")
    a = p.fig("persona", 16, 144, "Administración")
    t = p.fig("persona", 208, 144, "Tecnología")
    c = p.fig("persona", 400, 144, "Comercial")
    for h, x in ((p.fig("persona", 112, 272, "Sistemas"), t), (p.fig("persona", 304, 272, "Desarrollo"), t)):
        p.con(x, S, h, N, "angulo", fin=False)
    for h in (a, t, c):
        p.con(d, S, h, N, "angulo", fin=False)
    guardar("organigrama.arara", [p])


if __name__ == "__main__":
    flujo()
    red()
    organigrama()
    print("ejemplos de Arara en", DEST)
