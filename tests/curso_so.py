#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Curso de fundamentos de sistemas operativos en la Ayuda (v8.5.6, D456): prueba en el PC, sin QEMU.

Comprueba la carpeta rootfs/ayuda/sistemas-operativos/ (índice y 12 capítulos, en español, _EN y _PT):
  - cada capítulo tiene la cabecera «<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: N; titulo: ... -->»
    en la primera línea, con el orden del capítulo, y un primer título «# ...» sin dos puntos ni punto y coma
    (la Ayuda corta el título tras «:» y la cabecera se separa por «;»);
  - cada fichero es UTF-8, no hay caracteres de otros alfabetos y todo cabe en Latin-1 (la Ayuda dibuja con la fuente
    Latin-1 del sistema: una flecha, un «≈» o un exponente Unicode salen como basura; hay que escribir «->», «aprox.» o «10^-4»);
  - los enlaces relativos a ficheros .md apuntan a ficheros que existen;
  - las imágenes ![...](img/NOMBRE.png) usan un nombre de la lista acordada: si no lo es, falla; si el PNG aún
    no existe en esta rama, solo avisa (lo genera otro agente);
  - las rutas de código citadas en el texto (kernel/..., userland/..., boot/..., docs/...) existen en el árbol;
  - las decisiones citadas (Dxxx) existen como «## Dxxx.» en docs/decisoes.md;
  - las traducciones tienen los mismos bloques de código y el mismo número de secciones que el original.
Uso: python3 tests/curso_so.py   (sale con 0 si todo bien, 1 si algo falla)
"""
import io
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DIR = os.path.join(RAIZ, "rootfs", "ayuda", "sistemas-operativos")
CAPITULOS = [
    "01-que-es-un-so", "02-arranque", "03-modo-nucleo-usuario", "04-procesos", "05-planificacion",
    "06-memoria", "07-concurrencia", "08-sistemas-de-archivos", "09-entrada-salida", "10-redes",
    "11-graficos", "12-seguridad-energia",
]
BASES = ["indice"] + CAPITULOS + ["anexo-sdl"]
SUFIJOS = ["", "_EN", "_PT"]
IMAGENES_PERMITIDAS = {
    "dinamico",
    "arranque", "anillos", "procesos-estados", "planificador", "paginacion", "mapa-memoria", "vfs",
    "es-drivers", "pila-red", "graficos", "sincronizacion",
    "cap-terminal-ps", "cap-tareas", "cap-visorlogs", "cap-discos", "cap-infopc",
}
OTROS_ALFABETOS = re.compile(
    "[Ͱ-ϿЀ-ӿ֐-׿؀-ۿऀ-ॿ"
    "぀-ヿ㐀-鿿가-힯]")
CABECERA = re.compile(
    r"^<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: (\d+); titulo: ([^;:]+?) -->$")
ENLACE = re.compile(r"(!?)\[([^\]]*)\]\(([^)\s]+)\)")
BLOQUE = re.compile(r"```[a-z]*\n(.*?)```", re.S)
RUTA_CODIGO = re.compile(r"`((?:kernel|userland|boot|tests|docs|tools)/[A-Za-z0-9_./-]+)`")
DECISION = re.compile(r"\bD(\d{3})\b")

fallos = []
avisos = []


def verificar(cond, texto):
    if cond:
        print("ok    " + texto)
    else:
        print("FALLO " + texto)
        fallos.append(texto)


def aviso(texto):
    print("AVISO " + texto)
    avisos.append(texto)


def leer(ruta):
    with io.open(ruta, encoding="utf-8") as f:
        return f.read()


def decisiones_existentes():
    texto = leer(os.path.join(RAIZ, "docs", "decisoes.md"))
    return set(re.findall(r"^## D(\d{3})\.", texto, re.M))


def main():
    decisiones = decisiones_existentes()
    for base in BASES:
        for sufijo in SUFIJOS:
            ruta = os.path.join(DIR, base + sufijo + ".md")
            verificar(os.path.isfile(ruta), "existe %s%s.md" % (base, sufijo))
    if fallos:
        print("\n%d comprobación(es) fallida(s)" % len(fallos))
        return 1

    for n, base in enumerate(BASES):
        original = None
        for sufijo in SUFIJOS:
            nombre = base + sufijo + ".md"
            texto = leer(os.path.join(DIR, nombre))
            lineas = texto.split("\n")

            m = CABECERA.match(lineas[0]) if lineas else None
            verificar(m is not None, "%s: cabecera de categoría en la primera línea" % nombre)
            if m is not None and base != "indice":
                verificar(int(m.group(1)) == n, "%s: orden %s = %d" % (nombre, m.group(1), n))

            titulo = next((l for l in lineas if l.startswith("# ")), None)
            verificar(titulo is not None and ":" not in titulo,
                      "%s: primer título «# ...» y sin dos puntos" % nombre)
            verificar(not OTROS_ALFABETOS.search(texto), "%s: sin caracteres de otros alfabetos" % nombre)
            fuera = sorted({c for c in texto if ord(c) > 255})
            verificar(not fuera, "%s: todo el texto cabe en Latin-1 (la Ayuda dibuja con esa fuente; fuera de ella sale basura: %s)"
                      % (nombre, " ".join("U+%04X" % ord(c) for c in fuera[:6])))

            for sigla, etiqueta, destino in ENLACE.findall(texto):
                if destino.startswith(("http://", "https://", "#", "mailto:")):
                    continue
                if sigla == "!":
                    if destino.startswith("img/") and destino[4:-4] not in IMAGENES_PERMITIDAS:
                        verificar(False, "%s: imagen «%s» no está en la lista acordada" % (nombre, destino))
                        continue
                    if destino.startswith("img/"):
                        if not os.path.isfile(os.path.join(DIR, destino)):
                            aviso("%s: falta la imagen %s (la genera otro agente)" % (nombre, destino))
                        continue
                ruta_destino = os.path.join(DIR, destino.split("#")[0])
                if destino.split("#")[0].endswith(".md"):
                    verificar(os.path.isfile(ruta_destino), "%s: enlace a %s existe" % (nombre, destino))

            for ruta in RUTA_CODIGO.findall(texto):
                if "*" in ruta or ruta.endswith("/"):
                    continue
                verificar(os.path.exists(os.path.join(RAIZ, ruta)),
                          "%s: la ruta citada %s existe en el árbol" % (nombre, ruta))

            for d in set(DECISION.findall(texto)):
                verificar(d in decisiones, "%s: la decisión D%s existe en decisoes.md" % (nombre, d))

            bloques = BLOQUE.findall(texto)
            secciones = len(re.findall(r"^## ", texto, re.M))
            if sufijo == "":
                original = (bloques, secciones)
            else:
                verificar(bloques == original[0], "%s: mismos bloques de código que el original" % nombre)
                verificar(secciones == original[1], "%s: mismo número de secciones que el original" % nombre)

    if fallos:
        print("\n%d comprobación(es) fallida(s)" % len(fallos))
        return 1
    if avisos:
        print("\n%d aviso(s): imágenes pendientes de la rama de imágenes" % len(avisos))
    print("\ncurso de sistemas operativos: todo bien")
    return 0


if __name__ == "__main__":
    sys.exit(main())
