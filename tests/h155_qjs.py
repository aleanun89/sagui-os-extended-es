#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H155 - qjs, el intérprete de JavaScript en la terminal (D188).

Criterio: qjs ejecuta un script (aritmética, cadenas, JSON, Map/Set, clases,
regex, async/Promise con el bucle de trabajos vaciado al acabar, leer y
escribir ficheros, std/os, Date), evalúa con -e, carga módulos ES locales,
muestra un error con su traza (fichero:línea) y devuelve código distinto de 0,
aguanta un cálculo pesado (primos, fibonacci, recursión profunda, 200 mil
objetos) y abre un REPL con varias líneas, valor de cada expresión, historial
con la flecha arriba y salida con .exit y con Ctrl+D."""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402


def esperar_prompt(m, antes, texto="> ", tempo=40):
    fim = time.time() + tempo
    while m.serial().count(texto) <= antes:
        if time.time() > fim:
            raise Exception(f"el REPL no mostró {texto!r}")
        time.sleep(0.2)


def linea_repl(m, texto, prompt="> "):
    antes = m.serial().count(prompt)
    m.digitar(texto + "\n", 0.04)
    esperar_prompt(m, antes, prompt)


def main():
    with Maquina("h155", "ide", imagem=imagem_console(apps=True)) as m:
        m.esperar_serial("sagui:/$", 120)

        out = m.comando("qjs /testes/qjs/basico.js uno dos", tempo=90)
        for esperado, nombre in [
            ("aritm 1027 0.30000000000000004", "aritmética (**, %, doble precisión)"),
            ("cadena SAGUI-xxx 5", "cadenas"),
            ('json {"a":[1,2,{"b":null}],"c":"d"}', "JSON.stringify"),
            ("json2 3", "JSON.parse"),
            ("mapset 2 3 1,2,3", "Map y Set"),
            ("clase Tito hace ruido (guau)", "clases con herencia y super"),
            ("regex 555-1234|555-9876 sagui", "expresiones regulares"),
            ("args 3 uno,dos", "scriptArgs"),
            ("fichero 25 3 null", "std.writeFile / appendFile / loadFile"),
            ("env string undefined", "std.getenv"),
            ("fecha 1970-01-01T00:00:00.000Z true", "Date con el reloj del sistema"),
            ("sleep true", "os.sleep y os.now"),
            ("dir true 26", "os.readdir y os.stat"),
            ("fin script", "el script llega al final"),
            ("async sync>micro>await>timer>tarde", "async/await, Promise y temporizadores en orden"),
        ]:
            verificar(esperado in out, nombre)
        verificar("(qjs) terminou com codigo 0" in out or "terminou com codigo" not in out, "código de salida 0")
        m.captura("H155-script")

        out = m.comando("qjs -e 'console.log(6*7, [1,2,3].map(x => x*x)); print(\"hola\")'")
        verificar("42 [ 1, 4, 9 ]" in out and "hola" in out, "qjs -e evalúa código")
        out = m.comando("qjs -e 'std.exit(7)'")
        verificar("terminou com codigo 7" in out, "std.exit(7) devuelve el código 7")

        out = m.comando("qjs /testes/qjs/modulos.mjs")
        verificar("modulo modulo 42 5 function function" in out, "módulos ES: import de ficheros locales, 'std' y 'os'")
        verificar("tla 7" in out, "await de nivel superior en un módulo")

        out = m.comando("qjs /testes/qjs/erro.js")
        verificar("antes" in out and "nunca" not in out, "el script se detiene en el error")
        verificar("TypeError: fallo adrede" in out, "el error muestra su mensaje")
        verificar("/testes/qjs/erro.js:1" in out and "interna" in out and "externa" in out, "traza con fichero:línea")
        verificar("terminou com codigo 1" in out, "un error termina con código 1")
        out = m.comando("qjs /no/existe.js")
        verificar("terminou com codigo 1" in out, "fichero inexistente: código 1")
        out = m.comando("qjs -e 'Promise.reject(new Error(\"rechazo\"))'")
        verificar("rechazo" in out and "terminou com codigo 1" in out, "promesa rechazada sin tratar: código 1")

        out = m.comando("qjs /testes/qjs/pesado.js", tempo=240)
        for esperado, nombre in [("primos 148933", "criba de 2 millones"), ("fib 75025", "fibonacci recursivo"),
                                 ("prof 20000", "recursión de 20000 niveles"), ("memoria 200000 x199999", "200 mil objetos"),
                                 ("pila true", "el desbordamiento de pila es un error capturable (InternalError), no un fallo")]:
            verificar(esperado in out, nombre)

        # REPL
        antes = m.serial().count("> ")
        m.digitar("qjs\n", 0.04)
        esperar_prompt(m, antes)
        linea_repl(m, "let x = 6 * 7")
        linea_repl(m, "x")
        linea_repl(m, "function f(a) {", "... ")
        linea_repl(m, "  return a * x", "... ")
        linea_repl(m, "}", "> ")
        linea_repl(m, "f(2)")
        linea_repl(m, "({a: 1, b: [1, 2], c: 'hola'})")
        linea_repl(m, "new Map([[1, 'a']])")
        linea_repl(m, "nope.x")
        linea_repl(m, "'fin'")
        s = m.serial().replace("\r", "")
        verificar("\n42\n" in s, "el REPL muestra el valor de una expresión")
        verificar("\n84\n" in s, "función de varias líneas en el REPL")
        verificar("{ a: 1, b: [ 1, 2 ], c: 'hola' }" in s, "el REPL muestra objetos")
        verificar("Map(1) { 1 => 'a' }" in s, "el REPL muestra un Map")
        verificar("ReferenceError" in s, "un error en el REPL no lo cierra")
        verificar("'fin'" in s, "el REPL sigue vivo tras el error")
        # historial: flecha arriba trae la última línea
        antes = m.serial().count("> ")
        m.tecla("up")
        m.tecla("ret")
        esperar_prompt(m, antes)
        verificar(m.serial().replace("\r", "").count("'fin'") >= 2, "la flecha arriba recupera la línea anterior")
        m.captura("H155-repl")
        m.digitar(".exit\n", 0.04)
        m.esperar_serial("(qjs) terminou com codigo 0", 30)
        verificar(True, ".exit cierra el REPL")
        time.sleep(1)
        antes = m.serial().count("> ")
        m.digitar("qjs\n", 0.04)
        esperar_prompt(m, antes)
        m.tecla("ctrl-d")
        m.esperar_serial("(qjs) terminou com codigo 0", 30)
        verificar(True, "Ctrl+D cierra el REPL")
    return 0


if __name__ == "__main__":
    sys.exit(main())
