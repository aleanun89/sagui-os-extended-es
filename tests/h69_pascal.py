#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H69 - compilador de Pascal (pascal, mejora 040, D085-D087).

Criterio: dentro de SaguiOS, `pascal` compila los ejemplos de
/ejemplos/pascal y los programas de /testes/pascal y su salida es la
esperada (enteros, reales, cadenas, conjuntos, registros variantes,
punteros, archivos de texto, {$IFDEF}); un error de compilación muestra
archivo:línea:columna y termina con 1; {$R+} detiene el programa con el
error 201 y la línea; la unidad crt funciona sin tocar el kernel (gotoxy
hacia arriba con la pantalla virtual, readkey con flechas, keypressed con
un pipe); en Mico, F5 en un .pas compila y ejecuta y un error va a
Problemas; con -cpu pentium3 (sin SSE2) los reales salen iguales con la
x87; y el hola mundo ocupa menos de 4 KB."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console  # noqa: E402

ESPERADA = {
    "/testes/pascal/t1.pas": """suma=55 M=21
fib(20)=6765
j=8
b=250
ABC
TRUE FALSE
3 2 -3 -2
   10|55 |  x|
   3.142 6.28   0.333333
hola 4 hola mundo
 3.14159000000000E+0000
-3.14159000000000E+0000
1.4142135624 49 5 2.5
7 8 -8 2
TRUE 97 1024 128 6
0 1 2
210
54321""",
    "/testes/pascal/r1.pas": """3.50 -0.50 3.00 0.7500 0.666667
3.5 0.75
3.5 10.50 6.5
0.0000 1.0000 3.14159265 3.1415926536
0.000 2.718282 10.000000 4.0
TRUE TRUE FALSE  -3.50
0.30000000000000000 FALSE
10000000000 123456789000 0.00250
-3 3 -1 0.25 -3.0
 1.00000000000000E+0000  0.00000000000000E+0000 -1.23456000000000E+0002
 1.000E+0301  1.235E+0004
1.000000000000000
2.25 0.001 0 1.0""",
    "/testes/pascal/m1.pas": """disco 12.566
caja 12.0 12
SOiugaS 5
7 8 2
TRUE FALSE TRUE
3 6 9 12 15 28 31 34 37 40 987
123
alfa beta mu zeta
499500
definido
-2147483648
2147483647 0 TRUE 20""",
    "/ejemplos/pascal/hola.pas": "¡Hola, mundo! Compilado dentro de SaguiOS.",
    "/ejemplos/pascal/primos.pas": """2 3 5 7 11 13 17 19 23 29 31 37 41 43 47 53 59 61 67 71 73 79 83 89 97
25 primos hasta 100""",
    "/ejemplos/pascal/fibonacci.pas": """0 1 1 2 3 5 8 13 21 34 55 89 144 233 377 610
fib(25) = 75025
fib(46) = 1836311903 (el mayor que cabe en un integer)""",
    "/ejemplos/pascal/cadenas.pas": """Pascal en SaguiOS tiene 17 letras
Al revés: SOiugaS ne lacsaP
Mayúsculas: PASCAL EN SAGUIOS
copy(s, 11, 7) = SaguiOS
pos('en', s) = 8
Sin "en ": Pascal SaguiOS
Con "Turbo ": Turbo Pascal SaguiOS
string[8]: demasiad
[uno][dos][tres][cuatro] = 4 palabras
TRUE
str: "2026"
str real: "   3.142"
val: 1235 (código 0)
val con error en la posición 3""",
    "/ejemplos/pascal/conjuntos.pas": """Trabajo: lunes martes miércoles jueves viernes
Gimnasio: martes jueves sábado
Libres: sábado domingo
Trabajo y gimnasio: martes jueves
Algo que hacer: lunes martes miércoles jueves viernes sábado
¿El gimnasio cabe en la semana laboral? FALSE
"el murciélago comía ñame": 11 vocales y 10 consonantes""",
    "/ejemplos/pascal/ordenar.pas": """Por nota:
   1. Ana           9.75
   2. Lucía         8.50
   3. Marta         7.50
   4. Pedro         6.25
   5. Tomás         5.00
   6. Bruno         4.75
Por nombre:
   1. Ana           9.75
   2. Bruno         4.75
   3. Lucía         8.50
   4. Marta         7.50
   5. Pedro         6.25
   6. Tomás         5.00
Media: 6.958""",
    "/ejemplos/pascal/lista.pas": """[1, 2, 5, 6, 10, 11, 14, 15, 19, 20]
Elementos: 10
[1, 2, 5, 6, 10, 11, 15, 19, 20]
Lista vacía: TRUE""",
    "/ejemplos/pascal/archivos.pas": """Escrito /tmp/tabla.txt
Título: Tabla de cuadrados y cubos
10 líneas; suma de cuadrados = 385, de cubos = 3025
Con la línea añadida: 12 líneas; la última es "fin\"""",
}

CRT = """  AB     fila1
         fila2
         fi-
         fila4
         fila5

x=3 y=7 color
fin crt"""


def esperar(m, padrao, tempo=30, desde=0):
    fim = time.time() + tempo
    while time.time() < fim:
        g = re.findall(padrao, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.2)
    raise AssertionError(f"tiempo agotado esperando {padrao!r}")


def salida_programa(out):
    """Las líneas que escribió el programa: sin el eco del comando, los
    avisos del kernel ni el prompt (y sin espacios al final)."""
    lineas = out.split("\n")[1:]
    return [l.rstrip() for l in lineas if l and not l.startswith("[SAGUI]") and not l.startswith("sagui:")]


def nombre(fuente):
    return os.path.splitext(os.path.basename(fuente))[0]


def compilar_y_comparar(m, fuente, esperada, opcion="", sufijo=""):
    prog = nombre(fuente) + sufijo
    out = m.comando(f"pascal {opcion}{fuente} -o {prog}", tempo=200)
    if "(pascal) terminou com codigo 0" not in out:
        print(out)
        return False
    nuestra = salida_programa(m.comando(f"./{prog}", tempo=120))
    esperada = [l.rstrip() for l in esperada.split("\n")]
    if nuestra != esperada:
        for a, b in zip(nuestra + [""] * len(esperada), esperada + [""] * len(nuestra)):
            if a != b:
                print(f"    SaguiOS:  {a!r}\n    esperada: {b!r}")
                break
    return nuestra == esperada


def main():
    with Maquina("h69", "ide", imagem=imagem_console()) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("ajuda pascal")
        verificar("Uso: pascal" in out, "ajuda pascal explica el compilador")
        m.comando("cd /tmp")

        for fuente, esperada in ESPERADA.items():
            verificar(compilar_y_comparar(m, fuente, esperada), f"pascal {fuente}: la salida es la esperada")

        # Los ejemplos interactivos (crt y la ventana de sagui) al menos compilan.
        for f in ("colores", "fractal"):
            out = m.comando(f"pascal /ejemplos/pascal/{f}.pas", tempo=200)
            verificar("(pascal) terminou com codigo 0" in out, f"pascal /ejemplos/pascal/{f}.pas compila")

        # El hola mundo es pequeño (solo la parte de la biblioteca que usa).
        out = m.comando("ls")
        tam = re.search(r"^hola\s+(\d+) bytes", out, re.M)
        print(f"    hola: {tam.group(1) if tam else '?'} bytes")
        verificar(tam and int(tam.group(1)) < 4096, "hola.pas ocupa menos de 4 KB")

        # Error de compilación: archivo:línea:columna.
        out = m.comando("pascal /testes/pascal/e1.pas", tempo=120)
        verificar("/testes/pascal/e1.pas:5:3: error: 'b' no está declarado" in out,
                  "un error muestra archivo:línea:columna y el mensaje")
        verificar("(pascal) terminou com codigo 1" in out, "pascal termina con código 1")
        m.comando("echo 'program p; begin writeln(1 +) end.' > mal.pas")
        out = m.comando("pascal mal.pas")
        verificar(re.search(r"mal\.pas:1:\d+: error:", out), "error de sintaxis con línea y columna")

        # {$R+}: fallo de rango con el número de línea.
        out = m.comando("pascal /testes/pascal/e2.pas", tempo=120)
        out = m.comando("./e2")
        verificar("25\n" in out and "Error de ejecución 201: valor fuera de rango (línea 6)" in out,
                  "{$R+}: el índice fuera de rango para el programa con el error 201 y la línea")
        verificar("(e2) terminou com codigo 201" in out, "el código de salida es 201")

        # crt: gotoxy hacia arriba, clreol, wherex/wherey (pantalla virtual).
        out = m.comando("pascal /testes/pascal/c1.pas", tempo=120)
        verificar("(pascal) terminou com codigo 0" in out, "compila un programa con uses crt")
        out = m.comando("./c1")
        verificar(salida_programa(out) == [l for l in CRT.split("\n") if l], "crt: clrscr, gotoxy (también hacia arriba), clreol, wherex/wherey")
        tela = "\n".join(l.rstrip() for l in m.tela_texto())
        verificar(CRT in tela, "crt: la pantalla muestra lo mismo")
        m.captura("H69-crt")

        # crt: readkey con teclas normales y flechas (#0 + código).
        out = m.comando("pascal /testes/pascal/c2.pas", tempo=120)
        antes = len(m.serial())
        m.digitar("./c2\n", 0.04)
        m.esperar_serial("sin tecla", 30)
        time.sleep(0.5)
        m.digitar("a", 0.1)
        m.tecla("up", 0.1)
        m.digitar("q", 0.1)
        m.esperar_serial("(c2) terminou", 30)
        out = m.serial()[antes:].replace("\r", "")
        verificar("tecla 97\nespecial 72\ntecla 113\nfin" in out, "crt: readkey (a, flecha arriba = #0 #72, q) y delay")
        out = m.comando("echo q | ./c2", tempo=30)
        verificar("hay tecla" in out and "tecla 113" in out, "crt: keypressed con la entrada en un pipe")

    # Mico: F5 en un .pas compila con pascal y ejecuta; un error va a Problemas.
    with Maquina("h69mico", "ide", memoria="512M") as m:
        m.esperar_serial("sagui:/$", 120)
        time.sleep(1)
        m.comando("cp /ejemplos/pascal/hola.pas /tmp/hola.pas")
        k = len(m.serial())
        m.comando("mico /tmp/hola.pas")
        esperar(m, r"mico: pronto", 30, k)
        time.sleep(1)
        k = len(m.serial())
        m.tecla("f5")
        esperar(m, r"mico: F5 terminado en \d+ ms, 0 problemas", 90, k)
        s = m.serial()[k:]
        verificar("mico: F5 pascal /tmp/hola.pas -o /tmp/hola && /tmp/hola" in s and "Compilado dentro de SaguiOS" in s,
                  "Mico: F5 en un .pas compila con pascal y lo ejecuta")
        m.captura("H69-mico")
        # Un error al principio: F5 guarda, pascal falla y el error va a Problemas.
        m.tecla("ctrl-home")
        m.digitar("x ", 0.05)
        k = len(m.serial())
        m.tecla("f5")
        esperar(m, r"mico: F5 terminado en \d+ ms, [1-9]\d* problemas", 90, k)
        verificar("mico: problema /tmp/hola.pas:1:" in m.serial()[k:], "Mico: el error de pascal va a Problemas (hola.pas:1)")
        m.captura("H69-mico-problemas")

    # Sin SSE2 (Pentium III): pascal elige la x87 y los reales dan lo mismo.
    with Maquina("h69x87", "ide", imagem=imagem_console(), extras=["-cpu", "pentium3"]) as m:
        m.esperar_serial("sagui:/$", 180)
        m.comando("cd /tmp")
        out = m.comando("pascal /testes/pascal/r1.pas -o r1x", tempo=200)
        verificar("coma flotante con x87" in out, "en un Pentium III pascal usa la x87")
        nuestra = salida_programa(m.comando("./r1x", tempo=120))
        verificar(nuestra == ESPERADA["/testes/pascal/r1.pas"].split("\n"), "r1.pas con la x87 = con SSE2")
        verificar(compilar_y_comparar(m, "/testes/pascal/t1.pas", ESPERADA["/testes/pascal/t1.pas"]),
                  "t1.pas con la x87 = con SSE2")
    return 0


if __name__ == "__main__":
    sys.exit(main())
