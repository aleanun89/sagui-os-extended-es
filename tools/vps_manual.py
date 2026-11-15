#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Mantiene docs/visualpythons-manual.md igual que los ejemplos de /ejemplos/vps/ (D303).

El manual lleva, entre marcas, el código y los controles de cada ejemplo:

    <!-- ejemplo:calculadora -->  ...el codigo.py del ejemplo...  <!-- /ejemplo -->
    <!-- controles:calculadora -->  ...la tabla de controles del formulario.vpf...  <!-- /controles -->

Uso:
    python3 tools/vps_manual.py              reescribe esos bloques desde rootfs/ejemplos/vps/ y
                                             genera la copia de la ayuda del sistema
                                             (rootfs/ayuda/visualpythons-manual.md, sin enlaces a capturas)
    python3 tools/vps_manual.py --comprobar  no escribe; sale con 1 si el manual no coincide con los
                                             ejemplos o si falta alguna captura enlazada
"""
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.append(os.path.join(RAIZ, "rootfs", "lib", "python"))   # al final: no tapar el tkinter del anfitrión
import vps  # noqa: E402
from vps import proyecto  # noqa: E402

MANUAL = os.path.join(RAIZ, "docs", "visualpythons-manual.md")
AYUDA = os.path.join(RAIZ, "rootfs", "ayuda", "visualpythons-manual.md")
EJEMPLOS = os.path.join(RAIZ, "rootfs", "ejemplos", "vps")


def leer_latin1(ruta):
    with open(ruta, "rb") as f:
        return f.read().decode("latin-1")


def bloque_codigo(nombre):
    return "```python\n" + leer_latin1(os.path.join(EJEMPLOS, nombre, "codigo.py")).rstrip("\n") + "\n```"


def _val(v):
    if isinstance(v, str):
        return '"%s"' % v
    if isinstance(v, int) and not isinstance(v, bool) and v > 255:
        return "0x%06X" % v
    return str(v)


def tabla_controles(nombre):
    forms = proyecto.leer_vpf(leer_latin1(os.path.join(EJEMPLOS, nombre, "formulario.vpf")))
    filas = ["| Control | Nombre | x, y, ancho, alto | Propiedades |", "| --- | --- | --- | --- |"]
    for f in forms:
        p = f["props"]
        extra = ", ".join("`%s = %s`" % (k, _val(v)) for k, v in p.items())
        filas.append("| Formulario | `%s` | %s x %s | %s |" % (f["nombre"], p.get("ancho", 320), p.get("alto", 200), extra))
        for c in f["controles"]:
            cls = vps.TIPOS[c["tipo"]]
            q = c["props"]
            tam = cls.TAM
            pos = "%s, %s, %s, %s" % (q.get("x", 0), q.get("y", 0), q.get("ancho", tam[0]), q.get("alto", tam[1]))
            if c["tipo"] in ("Temporizador", "Menu", "Reloj", "Archivo", "ConectorBD", "SelectorDeArchivos", "SelectorDeCarpetas", "SelectorDeColor", "SelectorDeFuente"):
                pos = "(sin ventana)"
            extra = ", ".join("`%s = %s`" % (k, _val(v)) for k, v in q.items() if k not in ("x", "y", "ancho", "alto"))
            if c["padre"]:
                extra = ("dentro de `%s`; " % c["padre"]) + extra
            filas.append("| %s | `%s` | %s | %s |" % (c["tipo"], c["nombre"], pos, extra))
    return "\n".join(filas)


SIN_VENTANA = ("Temporizador", "Reloj", "Menu", "Archivo", "ConectorBD", "SelectorDeArchivos", "SelectorDeCarpetas", "SelectorDeColor", "SelectorDeFuente")
DATO_COMUN = {"doble_click": "`raton_x`, `raton_y`", "raton_abajo": "`raton_x`, `raton_y`, `boton`",
              "raton_arriba": "`raton_x`, `raton_y`, `boton`", "raton_entra": "`raton_x`, `raton_y`",
              "raton_sale": "`raton_x`, `raton_y`", "raton_mueve": "`raton_x`, `raton_y`",
              "rueda": "`rueda_delta`, `raton_x`, `raton_y`", "foco_gana": "", "foco_pierde": "",
              "tecla_abajo": "`tecla`, `caracter`, `ctrl`, `mayus`",
              "tecla_arriba": "`tecla`, `caracter`, `ctrl`, `mayus` (se dispara tras `tecla_abajo`)",
              "menu_contextual": "`raton_x`, `raton_y`, `boton` (3)"}
CUANDO_COMUN = {"doble_click": "Doble clic", "raton_abajo": "Se pulsa un botón del ratón sobre el control",
                "raton_arriba": "Se suelta el botón", "raton_entra": "El puntero entra en el control",
                "raton_sale": "El puntero sale del control", "raton_mueve": "El puntero se mueve sobre el control",
                "rueda": "Gira la rueda con el puntero encima", "foco_gana": "El control recibe el foco",
                "foco_pierde": "El control pierde el foco", "tecla_abajo": "Se pulsa una tecla con el foco en el control",
                "tecla_arriba": "Tras `tecla_abajo` (el sistema no avisa de soltar la tecla)",
                "menu_contextual": "Clic derecho"}


def tabla_comunes():
    filas = ["| Evento | Cuándo | Datos en el control |", "| --- | --- | --- |"]
    for e in vps.EVENTOS_COMUNES:
        filas.append("| `%s` | %s | %s |" % (e, CUANDO_COMUN[e], DATO_COMUN[e]))
    return "\n".join(filas)


def tabla_eventos():
    """Los eventos propios de cada tipo de control, sacados de vps.TIPOS (EVENTOS menos los comunes)."""
    filas = ["| Control | Eventos propios | Comunes |", "| --- | --- | --- |"]
    filas.append("| `Formulario` | %s | no |" % ", ".join("`%s`" % e for e in vps.Formulario.EVENTOS))
    for t, cls in vps.TIPOS.items():
        if t in ("Menu",):
            filas.append("| `Menu` | uno por opción: `MnuAbrir_click` | no |")
            continue
        propios = [e for e in cls.EVENTOS if e not in vps.EVENTOS_COMUNES or (cls.DOBLE_PROPIO and e == "doble_click")]
        comunes = "no" if t in SIN_VENTANA else "sí"
        filas.append("| `%s` | %s | %s |" % (t, ", ".join("`%s`" % e for e in propios) or "(ninguno)", comunes))
    return "\n".join(filas)


def reescribir(texto):
    def uno(m):
        return "<!-- ejemplo:%s -->\n%s\n<!-- /ejemplo -->" % (m.group(1), bloque_codigo(m.group(1)))

    def dos(m):
        return "<!-- controles:%s -->\n%s\n<!-- /controles -->" % (m.group(1), tabla_controles(m.group(1)))
    texto = re.sub(r"<!-- ejemplo:([a-z0-9-]+) -->\n.*?\n<!-- /ejemplo -->", uno, texto, flags=re.S)
    texto = re.sub(r"<!-- controles:([a-z0-9-]+) -->\n.*?\n<!-- /controles -->", dos, texto, flags=re.S)
    texto = re.sub(r"<!-- eventos-comunes -->\n.*?<!-- /eventos-comunes -->",
                   lambda m: "<!-- eventos-comunes -->\n%s\n<!-- /eventos-comunes -->" % tabla_comunes(), texto, flags=re.S)
    texto = re.sub(r"<!-- eventos-tabla -->\n.*?<!-- /eventos-tabla -->",
                   lambda m: "<!-- eventos-tabla -->\n%s\n<!-- /eventos-tabla -->" % tabla_eventos(), texto, flags=re.S)
    return texto


def capturas_enlazadas(texto):
    return re.findall(r"!\[[^\]]*\]\((\.\./capturas/[^)]+|capturas/[^)]+)\)", texto)


def para_ayuda(texto):
    """La copia de la ayuda del sistema: las capturas no van en la imagen de SaguiOS."""
    texto = re.sub(r"!\[([^\]]*)\]\([^)]*\)", lambda m: "*[captura: %s]*" % m.group(1), texto)
    return texto


def main():
    comprobar = "--comprobar" in sys.argv
    with open(MANUAL, encoding="utf-8") as f:
        actual = f.read()
    nuevo = reescribir(actual)
    errores = []
    if nuevo != actual:
        errores.append("el manual no coincide con los ejemplos de rootfs/ejemplos/vps/ (ejecuta tools/vps_manual.py)")
    for ruta in capturas_enlazadas(nuevo):
        if not os.path.exists(os.path.join(RAIZ, "docs", ruta.replace("../", "", 1) if ruta.startswith("../") else ruta)):
            errores.append("falta la captura " + ruta)
    if comprobar:
        for e in errores:
            print("vps_manual: " + e)
        print("vps_manual: %s" % ("FALLO" if errores else "ok"))
        sys.exit(1 if errores else 0)
    with open(MANUAL, "w", encoding="utf-8") as f:
        f.write(nuevo)
    with open(AYUDA, "w", encoding="utf-8") as f:
        f.write(para_ayuda(nuevo))
    for e in errores:
        if e.startswith("falta"):
            print("vps_manual: " + e)
    print("vps_manual: manual y ayuda al día")


if __name__ == "__main__":
    main()
