#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Genera las páginas de prueba de compatibilidad del Cipó (D271).

Cada página de tests/web/NN-tema.html es HTML+CSS pequeño, con una fila por
función. Una fila es una casilla de 40x16 px (clase «f», roja de fábrica) y su
rótulo. La función probada es la que pone la casilla verde (#008000): si el
motor no la entiende, se ve el rojo. La casilla lleva en `data-p` las sondas
que la prueba (tests/h271_cipo_css.py, con tests/web/comprobar.py) mira en la
captura; sin `data-p`, toda la casilla tiene que ser verde y nada puede
salirse por la derecha ni por abajo.

Las páginas generadas se guardan en git: este guion solo hace falta para
cambiarlas (python3 tests/web/generar.py).
"""
import os
import struct
import zlib

AQUI = os.path.dirname(os.path.abspath(__file__))
V = "#008000"     # verde: la función se entiende
A = "#000080"     # azul marino: segunda caja, para ver el orden
PASO = 24         # alto de una fila
X0, Y0 = 10, 10   # la primera casilla

BASE = """body{margin:0;font:12px sans-serif;background:#fff;color:#000}
#o{position:absolute;left:0;top:0;width:4px;height:4px;background:#f0f}
.f{position:absolute;left:10px;width:40px;height:16px;background:red}
.e{position:absolute;left:64px;height:16px;line-height:16px;white-space:nowrap}
.g{background:#008000}
.w{background:#fff}
"""


def png(larg, alt, pixel):
    """Un PNG RGB; pixel(x, y) devuelve (r, g, b)."""
    bruto = b"".join(b"\0" + b"".join(bytes(pixel(x, y)) for x in range(larg)) for y in range(alt))

    def trozo(tipo, dados):
        return struct.pack(">I", len(dados)) + tipo + dados + struct.pack(">I", zlib.crc32(tipo + dados) & 0xFFFFFFFF)

    return (b"\x89PNG\r\n\x1a\n" + trozo(b"IHDR", struct.pack(">IIBBBBB", larg, alt, 8, 2, 0, 0, 0)) +
            trozo(b"IDAT", zlib.compress(bruto, 9)) + trozo(b"IEND", b""))


def fila(rotulo, css="", html="", sondas="", clase=""):
    return (rotulo, css, html, sondas, clase)


def pagina(nombre, titulo, filas, css="", cabeza="", pie=""):
    est, cuerpo = [], []
    for i, (rotulo, c, h, sondas, clase) in enumerate(filas):
        t = Y0 + PASO * i
        ident = "f%d" % (i + 1)
        c = c.replace("F", "#" + ident).replace("{T}", str(t))
        h = h.replace("{T}", str(t))
        if c:
            est.append(c)
        cuerpo.append('<div class="f%s" id="%s" style="top:%dpx"%s>%s</div><div class="e" style="top:%dpx">%s</div>' % (
            (" " + clase) if clase else "", ident, t, (' data-p="%s"' % sondas) if sondas else "", h, t, rotulo))
    doc = ('<!doctype html>\n<html lang="es"><head><meta charset="utf-8"><title>%s</title>\n%s<style>\n%s%s%s\n</style></head>\n'
           '<body data-filas="%d"><div id="o"></div>\n%s\n%s</body></html>\n') % (
               titulo, cabeza, BASE, css, "\n".join(est), len(filas), "\n".join(cuerpo), pie)
    with open(os.path.join(AQUI, nombre + ".html"), "w", encoding="utf-8") as f:
        f.write(doc)


# ------------------------------------------------------------------ sondas
G, N, R, B = "008000", "000080", "ff0000", "ffffff"


def s(*p):
    """Sondas: s((x, y, color), ...)."""
    return " ".join(",".join(str(v) for v in q) for q in p)


# ------------------------------------------------- 01 variables y calc()
I = "F i{display:block;height:16px;background:#008000;width:0;"
pagina("01-variables-calc", "01 Variables CSS y calc()", [
    fila("var() de una variable de :root", ":root{--verde:#008000;--w:40px}F{background:var(--verde)}"),
    fila("var() con valor de reserva", "F{background:var(--no-existe,#008000)}"),
    fila("variable definida con otra var()", ":root{--otra:var(--verde)}F{background:var(--otra)}"),
    fila("variable heredada del padre (no de :root)", "F{--k:#008000}F i{display:block;height:16px;background:var(--k)}", "<i></i>"),
    fila("la variable del elemento gana a la de :root", ":root{--r:red}F{--r:#008000;background:var(--r)}"),
    fila("var() dentro de un atajo (border)", "F{box-sizing:border-box;border:8px solid var(--verde)}"),
    fila("var() como medida", I + "width:var(--w)}", "<i></i>"),
    fila("var() con reserva que es otra var()", "F{background:var(--nada,var(--verde))}"),
    fila("calc() con px + px", I + "width:calc(15px + 25px)}", "<i></i>"),
    fila("calc() con % y px", I + "width:calc(50% + 20px)}", "<i></i>"),
    fila("calc() con resta de % y px", I + "margin-left:10px;width:calc(100% - 10px)}", "<i></i>",
         s((5, 8, R), (12, 8, G), (38, 8, G), (44, 8, "!" + G))),
    fila("calc() con multiplicación, división y paréntesis", I + "width:calc(2 * (10px + 30px) / 2)}", "<i></i>"),
    fila("calc() con var()", I + "width:calc(var(--w) / 2 + 20px)}", "<i></i>"),
    fila("min() y max()", I + "width:max(10px, min(40px, 100px))}", "<i></i>"),
    fila("clamp()", I + "width:clamp(10px, 500px, 40px)}", "<i></i>"),
    fila("calc() con em", "F{font-size:10px}" + I + "width:calc(2em + 20px)}", "<i></i>"),
])

# ------------------------------------------------- 02 selectores
CSS_B = ".f b,.f u,.f s{float:left;width:10px;height:16px;text-decoration:none}\n.v{background:#008000}\n"
B4 = '<b %s></b><b class="v"></b><b %s></b><b class="v"></b>'
pagina("02-selectores", "02 Selectores: atributos y combinadores", [
    fila("[atributo]", "F [title]{background:#008000}", B4 % ('title="a"', 'title=""')),
    fila('[atributo="valor"]', 'F [data-k="uno"]{background:#008000}', B4 % ('data-k="uno"', "data-k=uno")),
    fila('[atributo^="prefijo"]', 'F [title^="ab"]{background:#008000}', B4 % ('title="abc"', 'title="ab"')),
    fila('[atributo$="sufijo"]', 'F [title$=".pdf"]{background:#008000}', B4 % ('title="a.pdf"', 'title=".pdf"')),
    fila('[atributo*="trozo"]', 'F [title*="mid"]{background:#008000}', B4 % ('title="amidb"', 'title="mid"')),
    fila('[atributo~="palabra"]', 'F [title~="dos"]{background:#008000}', B4 % ('title="uno dos tres"', 'title="dos"')),
    fila('[atributo|="es"]', 'F [lang|="es"]{background:#008000}', B4 % ('lang="es-ES"', 'lang="es"')),
    fila('[atributo="VALOR" i] (sin distinguir mayúsculas)', 'F [title="UNO" i]{background:#008000}', B4 % ('title="uno"', 'title="Uno"')),
    fila("hijo: a &gt; b", "F > b{background:#008000}", "<b></b><b></b><b></b><b></b>"),
    fila("hermano adyacente: a + b", "F b + b{background:#008000}", '<b class="v"></b><b></b><b></b><b></b>'),
    fila("hermanos siguientes: a ~ b", "F .p ~ b{background:#008000}", '<b class="v p"></b><b></b><u class="v"></u><b></b>'),
    fila("dos clases: .a.b", "F .p.q{background:#008000}", B4 % ('class="p q"', 'class="q r p"')),
    fila("universal: *", "F > *{background:#008000}", "<b></b><u></u><s></s><b></b>"),
    fila("lista: a, b", "F .p, F .q{background:#008000}", B4 % ('class="p"', 'class="q"')),
    fila("etiqueta y clase sin distinguir mayúsculas en la etiqueta", "F B.p{background:#008000}", B4 % ('class="p"', 'class="p"')),
    fila("no deben coincidir (trampas)",
         'F [title="ab"]{background:red}F [title^="b"]{background:red}F [title$="b"]{background:red}'
         'F [title~="ab"]{background:red}F b + u{background:red}F > b > b{background:red}F .p.zz{background:red}'
         'F [data-k]{background:red}', '<b title="abc" class="p"></b><b title="abc"></b>', "", "g"),
], CSS_B)

# ------------------------------------------------- 03 pseudoclases
pagina("03-pseudoclases", "03 Pseudoclases y pseudoelementos", [
    fila(":first-child y :last-child", "F b:first-child{background:#008000}F b:last-child{background:#008000}",
         '<b></b><b class="v"></b><b class="v"></b><b></b>'),
    fila(":nth-child(2n+1)", "F b:nth-child(2n+1){background:#008000}", '<b></b><b class="v"></b><b></b><b class="v"></b>'),
    fila(":nth-child(even) y (odd)", "F b:nth-child(even){background:#008000}F b:nth-child(odd){background:#008000}", "<b></b><b></b><b></b><b></b>"),
    fila(":nth-child(n+3) y (-n+2)", "F b:nth-child(n+3){background:#008000}F b:nth-child(-n+2){background:#008000}", "<b></b><b></b><b></b><b></b>"),
    fila(":nth-last-child(2)", "F b:nth-last-child(2){background:#008000}", '<b class="v"></b><b class="v"></b><b></b><b class="v"></b>'),
    fila(":nth-of-type(2)", "F b:nth-of-type(2){background:#008000}", '<b class="v"></b><u class="v"></u><b></b><b class="v"></b>'),
    fila(":first-of-type y :last-of-type", "F b:first-of-type{background:#008000}F b:last-of-type{background:#008000}",
         '<u class="v"></u><b></b><b></b><u class="v"></u>'),
    fila(":only-child", "F b:only-child{background:#008000;width:40px}", "<b></b>"),
    fila(":empty", "F b:empty{background:#008000}", "<b></b><b></b><b></b><b></b>"),
    fila(":not(.x)", "F b:not(.x){background:#008000}", '<b></b><b class="v x"></b><b></b><b class="v x"></b>'),
    fila(":not() con lista", "F b:not(.x, .y){background:#008000}", '<b></b><b class="v x"></b><b></b><b class="v y"></b>'),
    fila(":is(.p, .q)", "F :is(.p, .q){background:#008000}", B4 % ('class="p"', 'class="q"')),
    fila(":where(.p, .q)", "F :where(.p, .q){background:#008000}", B4 % ('class="p"', 'class="q"')),
    fila(":has(&gt; hijo)", "F b:has(> u){background:#008000}", '<b><u></u></b><b class="v"></b><b><u></u></b><b class="v"></b>'),
    fila(":has(+ hermano)", "F b:has(+ .x){background:#008000}", '<b></b><b class="v x"></b><b></b><b class="v x"></b>'),
    fila(":root", ":root F{background:#008000}"),
], CSS_B)

pagina("04-pseudoclases-2", "04 Pseudoclases de formulario y pseudoelementos", [
    fila(":checked", "F :checked + b{background:#008000;width:40px}", '<input type="checkbox" checked style="display:none"><b></b>'),
    fila(":disabled y :enabled", "F :disabled + b{background:#008000;width:20px}F :enabled + b{background:#008000;width:20px}",
         '<input disabled style="display:none"><b></b><input style="display:none"><b></b>'),
    fila(":required y :optional", "F :required + b{background:#008000;width:20px}F :optional + b{background:#008000;width:20px}",
         '<input required style="display:none"><b></b><input style="display:none"><b></b>'),
    fila(":link y :any-link", "F a{float:left;width:20px;height:16px}F a:link{background:#008000}F a:any-link{width:20px;background:#008000}",
         '<a href="#a"></a><a href="#b"></a>'),
    fila("::before con content", 'F::before{content:"";display:block;height:16px;background:#008000}'),
    fila("::after con content", 'F::after{content:"";display:block;height:16px;background:#008000}'),
    fila(":before (sintaxis antigua)", 'F:before{content:"";display:block;height:16px;background:#008000}'),
    fila("::before y ::after a la vez", 'F::before,F::after{content:"";display:block;height:8px;background:#008000}'),
    fila("content: attr()", 'F::before{content:attr(data-t);font:bold 14px sans-serif;color:#008000}', "", s((0, 0, 40, 16, "?" + G)), "w\" data-t=\"MMM"),
    fila("::first-letter", "F{font:bold 14px sans-serif;color:#fff}F::first-letter{color:#008000}", "MMM", s((0, 0, 14, 16, "?" + G)), "w"),
    fila("::marker", "F ul{margin:0;padding:0 0 0 20px;font:bold 14px sans-serif;color:#fff}F li::marker{color:#008000}", "<ul><li>M</li></ul>",
         s((0, 0, 20, 16, "?" + G)), "w"),
    fila("especificidad: #id gana a .clase", "F{background:#008000}div.f.g4x{background:red}", "", "", "g4x"),
    fila("!important", "F{background:#008000 !important}F{background:red}div.f.g4y{background:red}", "", "", "g4y"),
    fila(":is() suma especificidad; :where() no", "F :where(#zz, b){background:red}F b{background:#008000}F :is(b, #zz){width:10px}F b{width:0}",
         "<b></b><b></b><b></b><b></b>"),
    fila("no deben coincidir (trampas)",
         "F b:nth-child(5){background:red}F b:first-child:last-child{background:red}F i:empty{background:red}"
         "F b:not(b){background:red}F b:has(s){background:red}F :is(.zz){background:red}F b:nth-of-type(3){background:red}"
         "F :checked{background:red}F b:only-child{background:red}",
         '<b></b><b></b><i style="float:left;width:10px;height:16px"><u></u></i><input type="checkbox" style="float:left;margin:0;width:0;height:0;border:0;padding:0;appearance:none;-webkit-appearance:none">', "", "g"),
], CSS_B)

# ------------------------------------------------- 05 @media y otras reglas @
pagina("05-media-reglas", "05 @media, @supports, @import, @layer", [
    fila("@media (min-width: 200px) se aplica", "@media (min-width:200px){F{background:#008000}}"),
    fila("@media (max-width: 200px) no se aplica", "@media (max-width:200px){F{background:red}}", "", "", "g"),
    fila("@media screen and (min-width: 200px)", "@media screen and (min-width:200px){F{background:#008000}}"),
    fila("@media print no se aplica", "@media print{F{background:red}}", "", "", "g"),
    fila("@media con «and» entre dos condiciones", "@media (min-width:200px) and (max-width:9000px){F{background:#008000}}"),
    fila("@media con lista (coma)", "@media (max-width:200px), (min-width:201px){F{background:#008000}}"),
    fila("@media con rango: (width &gt;= 200px)", "@media (width >= 200px){F{background:#008000}}"),
    fila("@media con rango que no se cumple: (width &lt; 200px)", "@media (width < 200px){F{background:red}}", "", "", "g"),
    fila("@media not print", "@media not print{F{background:#008000}}"),
    fila("@media (prefers-color-scheme: light)", "@media (prefers-color-scheme:light){F{background:#008000}}@media (prefers-color-scheme:dark){F{background:red}}"),
    fila("@media (orientation: landscape) y (min-height)", "@media (orientation:landscape) and (min-height:100px){F{background:#008000}}"),
    fila("@supports (display: grid)", "@supports (display:grid){F{background:#008000}}"),
    fila("@supports not / propiedad desconocida no se aplican", "@supports not (display:grid){F{background:red}}@supports (cosa-rara:1){F{background:red}}", "", "", "g"),
    fila("@import de otra hoja", ""),
    fila("&lt;link rel=stylesheet&gt;", ""),
    fila("@layer", "@layer base{F i{display:block;height:16px;background:#008000}}", "<i></i>"),
], "", '<style>@import url("importada.css");</style>\n<link rel="stylesheet" href="enlazada.css">\n')
with open(os.path.join(AQUI, "importada.css"), "w") as f:
    f.write("/* hoja traída con @import desde 05-media-reglas.html */\n#f14{background:#008000}\n")
with open(os.path.join(AQUI, "enlazada.css"), "w") as f:
    f.write("/* hoja enlazada con <link> desde 05-media-reglas.html */\n#f15{background:#008000}\n")

# ------------------------------------------------- 06 unidades
pagina("06-unidades", "06 Unidades de medida", [
    fila("px", I + "width:40px}", "<i></i>"),
    fila("em", "F{font-size:10px}" + I + "width:4em}", "<i></i>"),
    fila("rem", "F{font-size:3px}" + I + "width:2.5rem}", "<i></i>"),
    fila("%", I + "width:100%}", "<i></i>"),
    fila("pt (30pt = 40px)", I + "width:30pt}", "<i></i>"),
    fila("in (0.25in = 24px)", I + "width:.25in}", "<i></i>", s((2, 8, G), (22, 8, G), (26, 8, R))),
    fila("cm y mm (10mm = 1cm = 37,8px)", "F i{display:block;height:8px;background:#008000;width:0}F i.c{width:1cm}F i.m{width:10mm}", '<i class="c"></i><i class="m"></i>',
         s((2, 4, G), (35, 4, G), (39, 4, R), (2, 12, G), (35, 12, G), (39, 12, R))),
    fila("vw (10vw, ventana de 400 px o más)", I + "width:10vw}", "<i></i>", s((2, 8, G), (38, 8, G))),
    fila("vh (10vh, ventana de 160 px o más)", "F i{display:block;width:40px;background:#008000;height:0;height:10vh;max-height:16px}", "<i></i>"),
    fila("vmin y vmax", "F i{display:block;height:8px;background:#008000;width:0;max-width:40px}F i.c{width:25vmin}F i.m{width:10vmax}", '<i class="c"></i><i class="m"></i>'),
    fila("ch (4ch en monoespaciada de 16px)", "F{font:16px monospace}" + I + "width:4ch}", "<i></i>", s((2, 8, G), (34, 8, G), (44, 8, "!" + G))),
    fila("ex (aprox. medio em)", "F{font:40px sans-serif}" + I + "width:1ex}", "<i></i>", s((2, 8, G), (14, 8, G), (30, 8, R))),
    fila("dvh, svh, lvh", "F i{display:block;width:40px;background:#008000;height:0;max-height:16px}F i.c{height:10dvh}", '<i class="c"></i>'),
    fila("medida sin unidad distinta de 0 no vale", I + "width:40px;width:10}", "<i></i>"),
    fila("números con signo y exponente (+40px, 4e1px)", "F i{display:block;height:8px;background:#008000;width:0}F i.c{width:+40px}F i.m{width:4e1px}", '<i class="c"></i><i class="m"></i>'),
])

# ------------------------------------------------- 07 flex
FB = ".f b{background:#008000;height:16px}\n"
pagina("07-flex", "07 Flexbox", [
    fila("display:flex en fila", "F{display:flex}F b{width:20px}", "<b></b><b></b>"),
    fila("flex-grow", "F{display:flex}F b{width:10px}F .c{flex-grow:1}", '<b></b><b class="c"></b>'),
    fila("flex: 1 (cuatro iguales)", "F{display:flex}F b{flex:1}", "<b></b><b></b><b></b><b></b>"),
    fila("flex-shrink (encogen para caber)", "F{display:flex}F b{width:30px}", "<b></b><b></b>"),
    fila("flex-basis", "F{display:flex}F b{flex:0 0 30px}F .c{flex-basis:10px}", '<b></b><b class="c"></b>'),
    fila("gap", "F{display:flex;gap:8px}F b{width:16px}", "<b></b><b></b>", s((8, 8, G), (20, 8, R), (32, 8, G), (38, 8, G))),
    fila("justify-content: space-between", "F{display:flex;justify-content:space-between}F b{width:10px}", "<b></b><b></b>", s((5, 8, G), (20, 8, R), (35, 8, G))),
    fila("justify-content: center", "F{display:flex;justify-content:center}F b{width:20px}", "<b></b>", s((5, 8, R), (20, 8, G), (35, 8, R))),
    fila("justify-content: space-evenly", "F{display:flex;justify-content:space-evenly}F b{width:8px}", "<b></b><b></b>",
         s((4, 8, R), (12, 8, G), (20, 8, R), (28, 8, G), (36, 8, R))),
    fila("align-items: center", "F{display:flex;align-items:center}F b{width:40px;height:8px}", "<b></b>", s((20, 1, R), (20, 8, G), (20, 14, R))),
    fila("align-items: stretch (de fábrica)", "F{display:flex}F b{width:40px;height:auto}", "<b></b>"),
    fila("align-self: flex-end", "F{display:flex}F b{width:40px;height:8px;align-self:flex-end}", "<b></b>", s((20, 3, R), (20, 12, G))),
    fila("flex-direction: column", "F{display:flex;flex-direction:column}F b{height:8px}", "<b></b><b></b>"),
    fila("flex-wrap: wrap", "F{display:flex;flex-wrap:wrap}F b{width:20px;height:8px}", "<b></b><b></b><b></b><b></b>"),
    fila("order", "F{display:flex}F b{width:20px}F .c{order:2;background:#000080}", '<b class="c"></b><b></b>', s((10, 8, G), (30, 8, N))),
    fila("flex-direction: row-reverse", "F{display:flex;flex-direction:row-reverse}F b{width:20px}F .c{background:#000080}", '<b class="c"></b><b></b>', s((10, 8, G), (30, 8, N))),
], FB)

pagina("08-flex-2", "08 Flexbox (2)", [
    fila("margin-left: auto en un elemento flexible", "F{display:flex}F b{width:10px;margin-left:auto}", "<b></b>", s((5, 8, R), (25, 8, R), (35, 8, G))),
    fila("gap con flex-wrap (row-gap y column-gap)", "F{display:flex;flex-wrap:wrap;gap:4px 8px}F b{width:16px;height:6px}", "<b></b><b></b><b></b><b></b>",
         s((8, 3, G), (20, 3, R), (32, 3, G), (8, 8, R), (8, 13, G), (32, 13, G))),
    fila("flex-flow: column wrap", "F{display:flex;flex-flow:column wrap}F b{width:20px;height:8px}", "<b></b><b></b><b></b><b></b>"),
    fila("justify-content: flex-end", "F{display:flex;justify-content:flex-end}F b{width:10px}", "<b></b>", s((5, 8, R), (35, 8, G))),
    fila("justify-content: space-around", "F{display:flex;justify-content:space-around}F b{width:10px}", "<b></b><b></b>",
         s((2, 8, R), (10, 8, G), (20, 8, R), (30, 8, G), (38, 8, R))),
    fila("columna con justify-content: center", "F{display:flex;flex-direction:column;justify-content:center}F b{height:8px}", "<b></b>", s((20, 1, R), (20, 8, G), (20, 14, R))),
    fila("columna con align-items: center", "F{display:flex;flex-direction:column;align-items:center}F b{width:20px}", "<b></b>", s((5, 8, R), (20, 8, G), (35, 8, R))),
    fila("display: inline-flex", "F{font-size:0}F span{display:inline-flex;width:40px}F b{flex:1}", "<span><b></b><b></b></span>"),
    fila("flex: 2 y flex: 1 (reparto 2 a 1)", "F{display:flex}F b{flex:1}F .c{flex:2;background:#000080}", '<b class="c"></b><b></b>', s((12, 8, N), (24, 8, N), (30, 8, G), (38, 8, G))),
    fila("min-width impide encoger", "F{display:flex;overflow:hidden}F b{width:30px;min-width:30px}F .c{background:#000080}", '<b></b><b class="c"></b>', s((28, 8, G), (32, 8, N))),
    fila("align-content: center (con wrap)", "F{display:flex;flex-wrap:wrap;align-content:center}F b{width:40px;height:4px}", "<b></b><b></b>", s((20, 2, R), (20, 6, G), (20, 10, G), (20, 14, R))),
    fila("flex anidado", "F{display:flex}F span{display:flex;flex:1;flex-direction:column}F b{flex:1;height:auto}", "<span><b></b><b></b></span><span><b></b></span>"),
    fila("% de ancho en hijos flexibles", "F{display:flex}F b{width:50%}", "<b></b><b></b>"),
    fila("display: contents", "F{display:flex}F span{display:contents}F b{flex:1}", "<span><b></b><b></b></span>"),
], FB)

# ------------------------------------------------- 09 grid
pagina("09-grid", "09 Grid", [
    fila("grid-template-columns en px", "F{display:grid;grid-template-columns:10px 30px}", "<b></b><b></b>"),
    fila("unidad fr (1fr 3fr)", "F{display:grid;grid-template-columns:1fr 3fr}F .c{background:#000080}", '<b class="c"></b><b></b>', s((5, 8, N), (15, 8, G), (38, 8, G))),
    fila("repeat(4, 1fr)", "F{display:grid;grid-template-columns:repeat(4,1fr)}", "<b></b><b></b><b></b><b></b>"),
    fila("gap", "F{display:grid;grid-template-columns:16px 16px;gap:8px}", "<b></b><b></b>", s((8, 8, G), (20, 8, R), (32, 8, G))),
    fila("grid-template-rows", "F{display:grid;grid-template-columns:20px 20px;grid-template-rows:8px 8px}F b{height:auto}", "<b></b><b></b><b></b><b></b>"),
    fila("minmax(0, 1fr)", "F{display:grid;grid-template-columns:repeat(2,minmax(0,1fr))}", "<b></b><b></b>"),
    fila("repeat(auto-fill, minmax(20px, 1fr))", "F{display:grid;grid-template-columns:repeat(auto-fill,minmax(20px,1fr))}", "<b></b><b></b>"),
    fila("repeat(auto-fit, minmax(10px, 1fr))", "F{display:grid;grid-template-columns:repeat(auto-fit,minmax(10px,1fr))}F .c{background:#000080}", '<b></b><b class="c"></b>',
         s((10, 8, G), (30, 8, N))),
    fila("grid-template-areas y grid-area", 'F{display:grid;grid-template-columns:10px 30px;grid-template-areas:"a b"}F .c{grid-area:b;background:#000080}F .d{grid-area:a}',
         '<b class="c"></b><b class="d"></b>', s((5, 8, G), (25, 8, N))),
    fila("grid-column: span 2", "F{display:grid;grid-template-columns:20px 20px}F b{height:8px}F .c{grid-column:span 2}", '<b class="c"></b><b></b><b></b>'),
    fila("grid-column y grid-row con número", "F{display:grid;grid-template-columns:20px 20px}F .c{grid-column:2;grid-row:1;background:#000080}F .d{grid-column:1;grid-row:1}",
         '<b class="c"></b><b class="d"></b>', s((10, 8, G), (30, 8, N))),
    fila("grid-column: 1 / -1", "F{display:grid;grid-template-columns:20px 20px}F b{height:8px}F .c{grid-column:1 / -1}", '<b class="c"></b><b></b><b></b>'),
    fila("place-items: center", "F{display:grid;grid-template-rows:16px;place-items:center}F b{width:20px;height:8px}", "<b></b>",
         s((20, 8, G), (5, 8, R), (35, 8, R), (20, 2, R), (20, 14, R))),
    fila("grid-auto-flow: column", "F{display:grid;grid-auto-flow:column}F b{width:20px}", "<b></b><b></b>"),
    fila("grid-auto-rows", "F{display:grid;grid-auto-rows:8px}F b{height:auto}", "<b></b><b></b>"),
    fila("áreas en varias filas", 'F{display:grid;grid-template-columns:20px 20px;grid-template-rows:8px 8px;grid-template-areas:"c c" "l r"}F b{height:auto}'
         'F .c{grid-area:c}F .l{grid-area:l;background:#000080}F .r{grid-area:r}', '<b class="r"></b><b class="l"></b><b class="c"></b>',
         s((10, 4, G), (30, 4, G), (10, 12, N), (30, 12, G))),
], FB)

# ------------------------------------------------- 10 posición y caja
IB = "F i{display:block;height:16px;background:#008000;"
pagina("10-posicion-caja", "10 Posición y modelo de caja", [
    fila("position: relative con left", IB + "width:40px;position:relative;left:10px;margin-left:-10px}", "<i></i>"),
    fila("position: absolute con right y bottom", "F i{position:absolute;right:0;bottom:0;width:40px;height:16px;background:#008000}", "<i></i>"),
    fila("inset: 0", "F i{position:absolute;inset:0;background:#008000}", "<i></i>"),
    fila("absolute estirado con left+right+top+bottom", "F i{position:absolute;left:0;right:0;top:0;bottom:0;background:#008000}", "<i></i>"),
    fila("z-index", "F i{position:absolute;inset:0}F .c{z-index:2;background:#008000}F .d{z-index:1;background:red}", '<i class="c"></i><i class="d"></i>'),
    fila("position: fixed", "F i{position:fixed;left:10px;top:{T}px;width:40px;height:16px;background:#008000}", "<i></i>"),
    fila("position: sticky (sin desplazar queda en su sitio)", IB + "position:sticky;top:0}", "<i></i>"),
    fila("float: left y right", "F i{width:20px;height:16px;background:#008000}F .c{float:left}F .d{float:right}", '<i class="c"></i><i class="d"></i>'),
    fila("clear: both", "F i{display:block;height:8px;background:#008000}F .c{float:left;width:40px}F .d{clear:both}", '<i class="c"></i><i class="d"></i>'),
    fila("overflow: hidden recorta", "F{overflow:hidden}F i{display:block;width:80px;height:40px;background:#008000}", "<i></i>"),
    fila("overflow: auto recorta", "F{overflow:auto}F i{display:block;width:40px;height:40px;background:#008000}", "<i></i>", s((2, 2, G), (2, 14, G), (20, 19, "!" + G))),
    fila("box-sizing: border-box", IB + "width:40px;padding:0 10px;box-sizing:border-box}", "<i></i>"),
    fila("margin: 0 auto", IB + "width:20px;margin:0 auto}", "<i></i>", s((5, 8, R), (20, 8, G), (35, 8, R))),
    fila("max-width y min-height", "F i{display:block;background:#008000;width:100px;max-width:40px;height:2px;min-height:16px}", "<i></i>"),
    fila("aspect-ratio", "F i{display:block;background:#008000;width:40px;aspect-ratio:5 / 2}", "<i></i>"),
    fila("display: inline-block", "F{font-size:0}F i{display:inline-block;width:20px;height:16px;background:#008000}", "<i></i><i></i>"),
])

pagina("11-caja-2", "11 Caja, visibilidad y transformaciones", [
    fila("display: none y visibility: hidden", "F i{display:block;height:8px;background:red}F .c{display:none}F .d{visibility:hidden}", '<i class="c"></i><i class="d"></i>', "", "g"),
    fila("width: fit-content", "F b{display:block;width:fit-content;margin-left:auto;background:#008000}F u{display:inline-block;width:20px;height:16px}F{font-size:0}",
         "<b><u></u></b>", s((10, 8, R), (30, 8, G))),
    fila("width: max-content y min-content", "F{font-size:0}F b{display:block;height:8px;background:#008000}F .c{width:max-content}F .d{width:min-content}F u{display:inline-block;width:20px;height:8px}F{overflow:hidden}",
         '<b class="c"><u></u><u></u></b><b class="d"><u></u><u></u></b>', s((10, 4, G), (30, 4, G), (10, 12, G), (30, 12, R))),
    fila("transform: translate()", IB + "width:40px;margin-left:-10px;transform:translate(10px,0)}", "<i></i>"),
    fila("transform: scale()", "F i{display:block;width:20px;height:8px;margin:4px 10px;background:#008000;transform:scale(2)}", "<i></i>"),
    fila("transform: rotate(180deg)", "F i{display:block;height:16px;background:#008000;border-left:20px solid #000080;transform:rotate(180deg)}", "<i></i>", s((10, 8, G), (30, 8, N))),
    fila("propiedades lógicas (inline-size, margin-inline)", "F i{display:block;background:#008000;inline-size:20px;block-size:16px;margin-inline:auto}", "<i></i>",
         s((5, 8, R), (20, 8, G), (35, 8, R))),
    fila("padding-block y padding-inline", "F i{display:block;background:#008000;width:0;height:0;padding-block:8px;padding-inline:20px}", "<i></i>"),
    fila("height: 100%", "F i{display:block;height:100%;background:#008000}", "<i></i>"),
    fila("márgenes negativos", IB + "margin-left:-10px;margin-right:10px}F{overflow:hidden}", "<i></i>",
         s((2, 8, G), (28, 8, G), (35, 8, R))),
    fila("los márgenes verticales se funden", "F i{display:block;height:4px;background:#008000;margin:4px 0}F{overflow:hidden}", "<i></i><i></i>",
         s((20, 2, R), (20, 6, G), (20, 10, R), (20, 14, G))),
    fila("border por lados y atajo", "F{box-sizing:border-box;border:8px solid red;border-color:#008000;border-left:20px solid #008000;border-right-width:20px}"),
    fila("width en % de un padre con padding", "F{box-sizing:border-box;padding:0 10px}F i{display:block;height:16px;width:100%;background:#008000}", "<i></i>",
         s((5, 8, R), (12, 8, G), (28, 8, G), (35, 8, R))),
    fila("overflow-x y overflow-y", "F{overflow-x:hidden;overflow-y:hidden}F i{display:block;width:80px;height:40px;background:#008000}", "<i></i>"),
    fila("float con texto alrededor (inline-block)", "F{font-size:0}F i{float:right;width:20px;height:16px;background:#008000}F u{display:inline-block;width:20px;height:16px;background:#000080}",
         "<i></i><u></u>", s((10, 8, N), (30, 8, G))),
    fila("tabla con display: table / table-cell", "F span{display:table;width:40px}F b{display:table-cell;height:16px;background:#008000}F .c{background:#000080}",
         '<span><b></b><b class="c"></b></span>', s((10, 8, G), (30, 8, N))),
])

# ------------------------------------------------- 12 colores, bordes y fondos
MEDIO = "80c080"   # verde al 50 % sobre blanco
pagina("12-colores-fondos", "12 Colores, bordes, sombras y fondos", [
    fila("border-radius", "F{border-radius:8px}", "", s((0, 0, B), (1, 1, B), (20, 8, G), (39, 15, B), (8, 1, G)), "g"),
    fila("border-radius: 50% (círculo)", "F i{display:block;width:16px;height:16px;border-radius:50%;background:#008000}", "<i></i>",
         s((8, 8, G), (0, 0, B), (15, 0, B), (0, 15, B), (8, 1, G), (30, 8, B)), "w"),
    fila("box-shadow con extensión (spread)", "F{box-shadow:0 0 0 3px #000080}", "", s((-2, 8, N), (42, 8, N), (20, -2, N), (20, 8, G)), "g"),
    fila("box-shadow desplazada", "F{box-shadow:4px 4px 0 #000080}", "", s((42, 10, N), (20, 18, N), (2, 18, B), (20, 8, G)), "g"),
    fila("box-shadow inset", "F{box-shadow:inset 0 0 0 3px #000080}", "", s((1, 8, N), (38, 8, N), (20, 8, G)), "g"),
    fila("box-shadow difuminada", "F{box-shadow:0 0 4px 2px #000080}", "", s((-2, 8, "!" + B), (41, 8, "!" + B), (20, 8, G), (-9, 8, B)), "g"),
    fila("outline", "F{outline:2px solid #000080}", "", s((-1, 8, N), (40, 8, N), (20, -2, N), (20, 8, G)), "g"),
    fila("opacity", "F{opacity:.5}", "", s((20, 8, MEDIO), (2, 2, MEDIO)), "g"),
    fila("rgba() y hsl()", "F i{display:block;height:8px}F .c{background:rgba(0,128,0,.5)}F .d{background:hsl(120,100%,25%)}", '<i class="c"></i><i class="d"></i>',
         s((20, 4, MEDIO), (20, 12, G)), "w"),
    fila("rgb() moderno: rgb(0 128 0) y rgb(0 128 0 / 50%)", "F i{display:block;height:8px}F .c{background:rgb(0 128 0 / 50%)}F .d{background:rgb(0 128 0)}",
         '<i class="c"></i><i class="d"></i>', s((20, 4, MEDIO), (20, 12, G)), "w"),
    fila("hexadecimal con alfa (#rrggbbaa y #rgba)", "F i{display:block;height:8px}F .c{background:#00800080}F .d{background:#008000ff}", '<i class="c"></i><i class="d"></i>',
         s((20, 4, MEDIO), (20, 12, G)), "w"),
    fila("currentColor e inherit", "F{color:#008000}F i{display:block;height:8px;color:red;color:inherit;background:currentColor}",
         "<i></i><i></i>"),
    fila("linear-gradient", "F{background:linear-gradient(to right,#008000,#000080)}", "", s((1, 8, G + "~16"), (38, 8, N + "~16"), (20, 8, "004040~16"))),
    fila("linear-gradient con paradas duras", "F{background:linear-gradient(90deg,#008000 50%,#000080 50%)}", "", s((10, 8, G), (30, 8, N))),
    fila("radial-gradient", "F{background:radial-gradient(circle,#008000 0,#008000 4px,#000080 5px)}", "", s((20, 8, G), (2, 8, N), (38, 2, N))),
    fila("transparent y color con nombre", "F{background:green}F i{display:block;height:16px;background:transparent}", "<i></i>"),
])

pagina("13-fondos-imagenes", "13 Fondos con imagen, object-fit y varios", [
    fila("background-image con url()", "F{background-image:url(verde.png)}"),
    fila("background con data: URI", "F{background:url(data:image/png;base64,__VERDE__)}"),
    fila("background-repeat, -position y -size", "F{background:#000080 url(verde.png) no-repeat right center / 20px 16px}", "", s((10, 8, N), (30, 8, G))),
    fila("background-size: cover", "F{background:url(franjas.png) center / cover no-repeat}F{width:20px}", "", s((5, 8, N), (15, 8, B), (30, 8, "!" + N))),
    fila("background-size: contain", "F{background:#000080 url(franjas.png) center / contain no-repeat;height:8px}", "", s((5, 4, N), (12, 4, G), (17, 4, N), (22, 4, B), (27, 4, "000000"), (35, 4, N))),
    fila("varios fondos", "F{background:url(verde.png) no-repeat 0 0 / 20px 16px, #000080}", "", s((10, 8, G), (30, 8, N))),
    fila("object-fit: cover", "F{width:20px}F img{display:block;width:20px;height:16px;object-fit:cover}", '<img src="franjas.png" alt="">', s((5, 8, N), (15, 8, B))),
    fila("object-fit: contain", "F img{display:block;width:40px;height:16px;object-fit:contain;background:#008000}",
         '<img src="franjas2.png" alt="">', s((4, 8, G), (17, 8, N), (23, 8, B), (36, 8, G))),
    fila("object-position", "F{width:20px}F img{display:block;width:20px;height:16px;object-fit:none;object-position:left top}", '<img src="franjas.png" alt="">',
         s((5, 8, G), (15, 8, N))),
    fila("img con width/height en CSS", "F img{display:block;width:40px;height:16px}", '<img src="verde.png" alt="">'),
    fila("img con max-width: 100% y height: auto", "F img{display:block;max-width:100%;height:auto}", '<img src="verde80.png" width="80" height="32" alt="">'),
    fila("filter: grayscale()", "F{filter:grayscale(1)}", "", s((20, 8, "555555~24")), "g"),
    fila("background-clip: content-box", "F{box-sizing:border-box;padding:0 10px;background-color:#008000;background-clip:content-box}", "",
         s((5, 8, "!" + G), (20, 8, G), (35, 8, "!" + G)), "w"),
    fila("border dashed / dotted se pintan", "F{box-sizing:border-box;border:4px dashed #008000}", "", s((0, 0, 40, 4, "?" + G), (0, 0, 40, 4, "?" + B)), "w"),
    fila("cursor, pointer-events, user-select no molestan", "F{cursor:pointer;pointer-events:none;user-select:none;background:#008000}"),
    fila("propiedad desconocida no rompe la regla", "F{cosa-rara:1;background:#008000;-webkit-otra:2}"),
])

# ------------------------------------------------- 14 texto
pagina("14-texto", "14 Texto", [
    fila("text-align: center", "F{font-size:0;text-align:center}F u{display:inline-block;width:20px;height:16px;background:#008000}", "<u></u>", s((5, 8, R), (20, 8, G), (35, 8, R))),
    fila("text-align: right", "F{font-size:0;text-align:right}F u{display:inline-block;width:20px;height:16px;background:#008000}", "<u></u>", s((10, 8, R), (30, 8, G))),
    fila("color del texto", "F{font:bold 14px sans-serif;color:#008000}", "MMM", s((0, 0, 40, 16, "?" + G)), "w"),
    fila("line-height", "F i{display:block;font-size:6px;line-height:16px;color:#008000;background:#008000}", "<i>x</i>"),
    fila("font: tamaño/interlínea familia", "F i{display:block;font:6px/16px sans-serif;color:#008000;background:#008000}", "<i>x</i>"),
    fila("text-indent", "F{font-size:0;text-indent:20px}F u{display:inline-block;width:20px;height:16px;background:#008000}", "<u></u>", s((10, 8, R), (30, 8, G))),
    fila("letter-spacing", "F{font-size:0}F u{display:inline-block;height:16px;background:#008000;letter-spacing:10px}", "<u>....</u>"),
    fila("white-space: nowrap", "F{font-size:8px;white-space:nowrap;overflow:hidden}F u{display:inline-block;width:20px;height:16px;background:#008000;vertical-align:top}",
         "<u></u> <u></u>", s((10, 8, G), (38, 8, G), (10, 19, "!" + G))),
    fila("overflow-wrap: anywhere", "F i{display:block;font:8px/8px monospace;overflow-wrap:anywhere;color:#008000;background:#008000}", "<i>MMMMMMMMMMMMMMMM</i>"),
    fila("word-break: break-all", "F i{display:block;font:8px/8px monospace;word-break:break-all;color:#008000;background:#008000}", "<i>MMMMMMMMMMMMMMMM</i>"),
    fila("text-decoration con color", "F{font:bold 12px sans-serif;color:#fff;text-decoration:underline #008000}", "MMMM", s((0, 0, 40, 16, "?" + G)), "w"),
    fila("text-shadow", "F{font:bold 14px sans-serif;color:#fff;text-shadow:1px 1px 0 #008000}", "MMM", s((0, 0, 40, 16, "?" + G)), "w"),
    fila("word-spacing", "F{font-size:0;word-spacing:20px}F u{display:inline-block;width:10px;height:16px;background:#008000}", "<u></u> <u></u>",
         s((5, 8, G), (20, 8, R), (35, 8, G))),
    fila("vertical-align: middle", "F{font-size:0;line-height:0}F u{display:inline-block;width:20px;background:#008000;vertical-align:middle}F .c{height:16px}F .d{height:8px}",
         '<u class="c"></u><u class="d"></u>', s((10, 8, G), (30, 2, R), (30, 8, G), (30, 14, R))),
    fila("@font-face con url()", '@font-face{font-family:"Prueba";src:url(/fontes/DejaVuSansMono-Bold.ttf) format("truetype")}'
         'F{font-size:0}F u{display:inline-block;font:16px/16px "Prueba",serif;color:#008000;background:#008000}', "<u>MMMM</u>",
         s((2, 8, G), (36, 8, G), (44, 8, "!" + G))),
    fila("text-overflow: ellipsis (recorta con overflow)", "F{font:bold 14px sans-serif;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;color:#008000}", "MMMMMMMMMM",
         s((0, 0, 40, 16, "?" + G), (44, 8, B)), "w"),
])

# ------------------------------------------------- 15 HTML5
U2 = "F{font-size:0}F u{display:inline-block;width:20px;height:8px;background:#008000}"
P2 = s((10, 4, G), (10, 12, G), (30, 4, R), (30, 12, R))
pagina("15-html5", "15 Elementos HTML5", [
    fila("&lt;header&gt; y &lt;footer&gt; son bloques", U2, "<header><u></u></header><footer><u></u></footer>", P2),
    fila("&lt;nav&gt; y &lt;main&gt;", U2, "<nav><u></u></nav><main><u></u></main>", P2),
    fila("&lt;section&gt; y &lt;article&gt;", U2, "<section><u></u></section><article><u></u></article>", P2),
    fila("&lt;aside&gt; y &lt;figcaption&gt;", U2, "<aside><u></u></aside><figcaption><u></u></figcaption>", P2),
    fila("&lt;hgroup&gt; y &lt;search&gt;", U2, "<hgroup><u></u></hgroup><search><u></u></search>", P2),
    fila("&lt;details&gt; cerrado esconde el contenido", "F{font-size:0}F i{display:block;height:16px;background:red}", "<details><summary></summary><i></i></details>",
         s((30, 4, G), (30, 12, G)), "g"),
    fila("&lt;details open&gt; lo muestra", "F{font-size:0}F i{display:block;height:16px;background:#008000}", "<details open><summary></summary><i></i></details>",
         s((30, 4, G), (30, 12, G))),
    fila("&lt;dialog&gt; cerrado no se ve", "F i{display:block;height:16px;background:red}", "<dialog><i></i></dialog>", "", "g"),
    fila("&lt;dialog open&gt; se ve", "F dialog{position:static;border:0;padding:0;margin:0;width:40px;height:16px;background:#008000}", "<dialog open></dialog>"),
    fila("&lt;template&gt; no se pinta", "F i{display:block;height:16px;background:red}", "<template><i></i></template>", "", "g"),
    fila("atributo hidden", "F i{height:16px;background:red}", "<i hidden></i><div hidden><i style=\"display:block\"></i></div>", "", "g"),
    fila("&lt;picture&gt; con &lt;source&gt;", "F img{display:block}", '<picture><source srcset="verde.png" type="image/png"><img src="rojo.png" width="40" height="16" alt=""></picture>'),
    fila("&lt;img srcset&gt;", "F img{display:block}", '<img srcset="verde.png 1x, rojo.png 2x" src="rojo.png" width="40" height="16" alt="">'),
    fila("&lt;img&gt; con data: URI y loading=lazy", "F img{display:block;float:left}",
         '<img src="data:image/png;base64,__VERDE__" width="20" height="16" alt=""><img loading="lazy" src="verde.png" width="20" height="16" alt="">'),
    fila("&lt;svg&gt; en línea", "F svg{display:block}", '<svg width="40" height="16" viewBox="0 0 40 16"><rect width="40" height="16" fill="#008000"/></svg>'),
    fila("elemento personalizado con estilo", "F x-caja{display:block;height:16px;background:#008000}", "<x-caja></x-caja>"),
])

CTL = ("display:block;box-sizing:border-box;width:40px;height:16px;margin:0;padding:0;border:2px solid #008000;border-radius:0;"
       "appearance:none;-webkit-appearance:none;background:#fff;font-size:0")
PB = s((0, 8, G), (20, 0, G), (39, 8, G), (20, 15, G), (1, 1, G), (38, 14, G), (44, 8, B), (20, 19, B))
pagina("16-html5-medios", "16 Medios y otros elementos", [
    fila("&lt;video poster&gt; como marcador con el póster", "F video{display:block}", '<video width="40" height="16" poster="verde80.png"></video>'),
    fila("&lt;video&gt; sin póster ocupa su caja", "F video{display:block;width:40px;height:16px;background:#008000}", '<video src="no-hay.mp4"></video>'),
    fila("&lt;video&gt; no pinta su contenido alternativo", "F video{display:block;width:40px;height:16px;background:#008000}F i{display:block;height:16px;background:red}",
         "<video><i></i></video>"),
    fila("&lt;audio controls&gt; ocupa una caja", "F audio{display:block;width:40px;height:16px;background:#008000}", "<audio controls></audio>", s((1, 1, "!" + R), (20, 8, "!" + R), (38, 14, "!" + R))),
    fila("&lt;audio&gt; sin controls no se ve", "F audio{background:red;width:40px;height:16px}", "<audio></audio>", "", "g"),
    fila("&lt;canvas&gt; ocupa su tamaño", "F canvas{display:block;background:#008000}", '<canvas width="40" height="16"></canvas>'),
    fila("&lt;iframe&gt; ocupa su caja", "F iframe{display:block;box-sizing:border-box;width:40px;height:16px;border:2px solid #008000}", '<iframe src="about:blank"></iframe>',
         s((0, 8, G), (20, 0, G), (39, 8, G), (20, 15, G))),
    fila("&lt;progress&gt; ocupa su caja", "F progress{" + CTL + "}", '<progress value="1" max="2"></progress>', s((0, 8, "!" + R), (20, 8, "!" + R), (39, 8, "!" + R), (44, 8, B))),
    fila("&lt;meter&gt; ocupa su caja", "F meter{" + CTL + "}", '<meter value="1" max="2"></meter>', s((0, 8, "!" + R), (20, 8, "!" + R), (39, 8, "!" + R), (44, 8, B))),
    fila("&lt;mark&gt; resalta en amarillo", "F{font:bold 14px sans-serif}", "<mark>MMM</mark>", s((0, 0, 40, 16, "?ffff00~10")), "w"),
    fila("&lt;hr&gt; con estilo", "F hr{margin:0;border:0;height:16px;background:#008000}", "<hr>"),
    fila("&lt;table&gt; con atributos antiguos", "", '<table cellspacing="0" cellpadding="0" border="0"><tr><td width="20" height="16" bgcolor="#008000"></td><td width="20" bgcolor="#008000"></td></tr></table>'),
    fila("&lt;font color&gt; y &lt;center&gt;", "F{font:bold 14px sans-serif}", '<center><font color="#008000">MM</font></center>', s((6, 0, 34, 16, "?" + G), (0, 0, 3, 16, "?" + B)), "w"),
    fila("&lt;br&gt; y &lt;wbr&gt;", "F{font-size:0;line-height:0}F u{display:inline-block;width:40px;height:8px;background:#008000}", "<u></u><br><wbr><u></u>"),
    fila("&lt;noscript&gt; no se ve con JavaScript", "F i{display:block;height:16px;background:red}", "<noscript><i></i></noscript>", "", "g"),
    fila("&lt;figure&gt; con sus márgenes", "F{font-size:0;overflow:hidden}F figure{margin:0 10px}F i{display:block;height:16px;background:#008000}", "<figure><i></i></figure>",
         s((5, 8, R), (20, 8, G), (35, 8, R))),
])

# ------------------------------------------------- 17 formularios
def control(etq):
    return "F %s{%s}" % (etq, CTL)


TIPOS = ["text", "search", "email", "url", "tel", "number", "password", "date", "time", "color", "range", "checkbox", "radio", "file", "submit"]
pagina("17-formularios", "17 Formularios", [
    fila('&lt;input type="%s"&gt;' % t, control("input"), '<input type="%s"%s>' % (t, ' value=""' if t == "submit" else ""),
         PB if t != "number" else s((0, 8, G), (5, 0, G), (5, 15, G), (1, 1, G), (39, 8, G), (44, 8, B), (20, 19, B))) for t in TIPOS
] + [fila('&lt;input type="datetime-local"&gt;', control("input"), '<input type="datetime-local">', PB)])

pagina("18-formularios-2", "18 Formularios (2)", [
    fila("&lt;button&gt;", control("button"), "<button></button>", PB),
    fila("&lt;select&gt;", control("select"), "<select><option>a</option></select>", PB),
    fila("&lt;textarea&gt;", control("textarea") + "F textarea{resize:none;overflow:hidden}", "<textarea></textarea>", PB),
    fila('&lt;input type="month"&gt; y "week"', "F input{" + CTL + ";height:8px;border-width:1px}", '<input type="month"><input type="week">',
         s((0, 4, G), (39, 4, G), (0, 12, G), (39, 12, G), (20, 0, G), (20, 15, G))),
    fila("&lt;input&gt; de texto: fondo y borde del autor", "F input{display:block;box-sizing:border-box;width:40px;height:16px;margin:0;padding:0;border:0;background:#008000}", '<input type="text">'),
    fila("&lt;button&gt;: fondo y borde del autor", "F button{display:block;box-sizing:border-box;width:40px;height:16px;margin:0;padding:0;border:0;background:#008000}", "<button></button>"),
    fila("placeholder (se pinta en gris)", "F input{display:block;box-sizing:border-box;width:40px;height:16px;margin:0;padding:0;border:0;background:#fff;font:bold 12px sans-serif}",
         '<input type="text" placeholder="MMMM">', s((0, 0, 40, 16, "!?" + B)), "w"),
    fila("::placeholder con color", "F input{display:block;box-sizing:border-box;width:40px;height:16px;margin:0;padding:0;border:0;background:#fff;font:bold 12px sans-serif}"
         "F input::placeholder{color:#008000;opacity:1}", '<input type="text" placeholder="MMMM">', s((0, 0, 40, 16, "?" + G)), "w"),
    fila("value de un campo de texto se pinta", "F input{display:block;box-sizing:border-box;width:40px;height:16px;margin:0;padding:0;border:0;background:#fff;color:#008000;font:bold 12px sans-serif}",
         '<input type="text" value="MMMM">', s((0, 0, 40, 16, "?" + G)), "w"),
    fila("&lt;fieldset&gt; y &lt;legend&gt;", "F fieldset{box-sizing:border-box;width:40px;height:16px;margin:0;padding:0;border:2px solid #008000;min-width:0}F legend{display:none}",
         "<fieldset><legend>x</legend></fieldset>", s((0, 8, G), (39, 8, G), (20, 15, G))),
    fila("&lt;label&gt; es en línea", "F{font-size:0}F u{display:inline-block;width:20px;height:16px;background:#008000}", "<label><u></u></label><label><u></u></label>"),
    fila("&lt;datalist&gt; no se ve", "F datalist i{display:block;height:16px;background:red}", '<datalist id="dl"><option value="a"><i></i></datalist>', "", "g"),
    fila("casilla marcada: accent-color no molesta", "F input{" + CTL + ";accent-color:#008000}", '<input type="checkbox" checked>', s((0, 8, G), (39, 8, G), (20, 0, G), (20, 15, G))),
    fila("&lt;output&gt; y &lt;time&gt; son en línea", "F{font-size:0}F u{display:inline-block;width:20px;height:16px;background:#008000}",
         "<output><u></u></output><time><u></u></time>"),
    fila("campo deshabilitado conserva su caja", control("input"), '<input type="text" disabled>', s((0, 8, G), (39, 8, G), (20, 0, G), (20, 15, G))),
    fila("&lt;input type=hidden&gt; no ocupa", "F input{display:block;width:40px;height:16px;background:red;border:0}", '<input type="hidden" value="x">', "", "g"),
])

# ------------------------------------------------- 19 JavaScript y DOM
def js(rotulo, codigo, html="", css="", sondas="", clase=""):
    return (rotulo, css, html, sondas, clase, codigo)


def pagina_js(nombre, titulo, filas, css=""):
    pie = []
    for i, f in enumerate(filas):
        pie.append("<script>try{(function(el){%s})(document.getElementById('f%d'))}catch(e){console.log('f%d: '+e)}</script>" % (
            f[5].replace("%d", str(i + 1)), i + 1, i + 1))
    pagina(nombre, titulo, [f[:5] for f in filas], css, "", "\n".join(pie) + "\n")


VERDE = "el.style.background='#008000'"
pagina_js("19-js-dom", "19 JavaScript: DOM", [
    js("getElementById y style.background", VERDE),
    js("querySelector", "document.querySelector('#f%d').style.backgroundColor='#008000'"),
    js("querySelectorAll y forEach", "document.querySelectorAll('#f%d b').forEach(function(b){b.style.background='#008000'})", "<b></b><b></b><b></b><b></b>"),
    js("classList.add", "el.classList.add('g')"),
    js("classList.remove y contains", "if(el.classList.contains('rojo')){el.classList.remove('rojo');el.classList.add('g')}", "", "F.rojo{background:red !important}", "", "rojo"),
    js("classList.toggle", "el.classList.toggle('g');el.classList.toggle('rojo')", "", "F.rojo{background:red !important}", "", "rojo"),
    js("className", "el.className='f g'"),
    js("dataset", "if(el.dataset.color==='verde'&&el.dataset.dosPalabras==='si')" + VERDE, "", "", "", 'f" data-color="verde" data-dos-palabras="si'),
    js("setAttribute y getAttribute", "el.setAttribute('title','x');if(el.getAttribute('title')==='x')el.setAttribute('class','f g')"),
    js("createElement y appendChild", "var i=document.createElement('i');i.className='lleno';el.appendChild(i)", "", ".lleno{display:block;height:16px;background:#008000}"),
    js("textContent de un elemento", "el.textContent=''", "<i></i>", "F i{display:block;height:16px;background:red}", "", "g"),
    js("innerHTML de un elemento", "el.innerHTML='<i class=\"lleno\"></i>'"),
    js("remove() y removeChild", "el.querySelector('.c').remove();el.removeChild(el.querySelector('.d'))", '<i class="c"></i><i class="d"></i>',
       "F i{display:block;height:8px;background:red}", "", "g"),
    js("insertAdjacentHTML", "el.insertAdjacentHTML('beforeend','<i class=\"lleno\"></i>')"),
    js("closest y matches", "var b=el.querySelector('b');if(b.matches('.c')&&b.closest('.f')===el)" + VERDE, '<b class="c" style="display:none"></b>'),
    js("style.cssText y style.setProperty", "el.style.cssText=el.style.cssText+';background:red';el.style.setProperty('background','#008000')"),
], ".f b{float:left;width:10px;height:16px}\n")

pagina_js("20-js-api", "20 JavaScript: API de la ventana", [
    js("setTimeout", "setTimeout(function(){" + VERDE + "},10)"),
    js("requestAnimationFrame", "requestAnimationFrame(function(t){if(typeof t==='number')" + VERDE + "})"),
    js("DOMContentLoaded", "document.addEventListener('DOMContentLoaded',function(){" + VERDE + "})"),
    js("window.onload y addEventListener('load')", "var n=0;window.addEventListener('load',function(){n++;if(n==2)" + VERDE + "});window.onload=function(){n++;if(n==2)" + VERDE + "}"),
    js("localStorage", "localStorage.setItem('k','v');if(localStorage.getItem('k')==='v'&&localStorage.getItem('nada')===null){localStorage.removeItem('k');" + VERDE + "}"),
    js("sessionStorage", "sessionStorage.setItem('k','v');if(sessionStorage.getItem('k')==='v')" + VERDE),
    js("fetch() de un texto", "fetch('dato.txt').then(function(r){return r.text()}).then(function(t){if(t.indexOf('verde')>=0)" + VERDE + "})"),
    js("fetch() con json()", "fetch('dato.json').then(function(r){return r.json()}).then(function(o){if(o.color==='verde')" + VERDE + "})"),
    js("XMLHttpRequest", "var x=new XMLHttpRequest();x.open('GET','dato.txt');x.onload=function(){if(x.status==200&&x.responseText.indexOf('verde')>=0)" + VERDE + "};x.send()"),
    js("IntersectionObserver", "new IntersectionObserver(function(es){es.forEach(function(e){if(e.isIntersecting)e.target.style.background='#008000'})}).observe(el)"),
    js("matchMedia", "if(window.matchMedia('(min-width: 200px)').matches&&!window.matchMedia('(max-width: 200px)').matches)" + VERDE),
    js("getComputedStyle", "if(getComputedStyle(el).position==='absolute')" + VERDE),
    js("getBoundingClientRect, offsetWidth, innerWidth", "var r=el.getBoundingClientRect();if(r.width===40&&r.left===10&&el.offsetWidth===40&&window.innerWidth>=300)" + VERDE),
    js("Promise, async/await y funciones flecha", "(async()=>{const v=await Promise.resolve(1);if(v===1)" + VERDE + "})()"),
    js("eventos: dispatchEvent y CustomEvent", "el.addEventListener('cosa',function(e){if(e.detail===7)" + VERDE + "});el.dispatchEvent(new CustomEvent('cosa',{detail:7}))"),
    js("template.content y cloneNode", "var t=document.getElementById('tpl');el.appendChild(t.content.cloneNode(true))",
       '<template id="tpl"><i class="lleno"></i></template>', ".lleno{display:block;height:16px;background:#008000}"),
])
with open(os.path.join(AQUI, "dato.txt"), "w") as f:
    f.write("color verde\n")
with open(os.path.join(AQUI, "dato.json"), "w") as f:
    f.write('{"color": "verde"}\n')

# ------------------------------------------------- imágenes
FRANJAS = [(0, 128, 0), (0, 0, 128), (255, 255, 255), (0, 0, 0)]
IMGS = {
    "verde.png": png(8, 8, lambda x, y: (0, 128, 0)),
    "rojo.png": png(8, 8, lambda x, y: (255, 0, 0)),
    "verde80.png": png(80, 32, lambda x, y: (0, 128, 0)),
    "franjas.png": png(40, 16, lambda x, y: FRANJAS[x // 10]),             # verde, azul, blanco, negro
    "franjas2.png": png(16, 32, lambda x, y: FRANJAS[1 + x // 8]),          # alta y estrecha: azul, blanco
}
for nombre, datos in IMGS.items():
    with open(os.path.join(AQUI, nombre), "wb") as f:
        f.write(datos)

# el data: URI de las páginas que lo llevan
import base64  # noqa: E402
B64 = base64.b64encode(IMGS["verde.png"]).decode()
for nombre in sorted(os.listdir(AQUI)):
    if nombre.endswith(".html"):
        camino = os.path.join(AQUI, nombre)
        with open(camino, encoding="utf-8") as f:
            t = f.read()
        if "__VERDE__" in t:
            with open(camino, "w", encoding="utf-8") as f:
                f.write(t.replace("__VERDE__", B64))
