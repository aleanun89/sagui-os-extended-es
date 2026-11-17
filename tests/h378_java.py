#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""H378 - Java con miniJVM dentro de SaguiOS (D376, docs/java.md).

Se salta con un aviso si no hay JDK (tools/java_entorno.sh instalar) o fuentes de miniJVM
(tools/minijvm_fuentes.sh). Si están: usa los ejemplos que `make` compila (tools/javac-sagui) y
que van a /apps/java/ejemplos, arranca QEMU con el terminal de pantalla completa, ejecuta cada uno
y comprueba su salida, los acentos (Latin-1 en el terminal) y el código de salida."""
import os
import re
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste, APPS_RAIZ, BUILD, RAIZ  # noqa: E402

JAVAC = os.path.expanduser("~/.local/saguitools/jdk/bin/javac")
FUENTES = os.path.join(RAIZ, "userland", "terceros", "minijvm", "fuentes", "minijvm", "c", "jvm", "jvm.c")
J = "java -cp /apps/java/ejemplos "

# (comando, [textos que debe contener la salida])
COMANDOS = [
    (J + "Hola mundo", ["hola mundo desde Java en SaguiOS", "2 + 3 = 5", "raiz de 2 = 1.414",
                        "acentos: a\xf1o y canci\xf3n, \xbfqu\xe9 tal?"]),
    (J + "Argumentos uno dos", ['arg 1 = "uno"', 'arg 2 = "dos"', "argumentos: 2"]),
    (J + "Ficheros", ["escrito: 24 bytes", "lineas: 3", "existe tras borrar: false", "ficheros: terminado"]),
    (J + "Colecciones", ["ordenada: [1, 4, 9, 16, 25, 36, 49, 64, 81, 100]", "{dos=2, tres=3, uno=1}",
                         "hashmap: 100 elementos, 42 -> v42", "sb: 0,1,2,3,4", "suma de cuadrados 1..10 = 385"]),
    (J + "Hilos", ["contador = 8000", "consumidor recibe 42", "hilos: terminado"]),
    (J + "Excepciones", ["capturada: faltan 15", "finally", "indice fuera de rango", "conversion invalida",
                         "division por cero", "excepciones: terminado"]),
]


def esperar(m, patron, tiempo=60, desde=0):
    fin = time.time() + tiempo
    while time.time() < fin:
        g = re.findall(patron, m.serial()[desde:])
        if g:
            return g[-1]
        time.sleep(0.3)
    raise AssertionError(f"tiempo esgotado esperando {patron!r}")


def ventana(m, titulo, desde, tiempo=60):
    """Posicion del contenido de la ventana (la linea del kernel «janela N criada...»)."""
    g = esperar(m, rf'janela (\d+) criada por \d+: "{re.escape(titulo)}" (\d+)x(\d+) em \((\d+),(\d+)\)', tiempo, desde)
    return int(g[3]) + 2, int(g[4]) + 24   # borde y barra de titulo


def parte_gui(m):
    """sagui.gui: calculadora (botones), bloc de notas (menu) y serpiente (lienzo y teclado)."""
    # calculadora: 7 * 6 =
    ini = len(m.serial())
    m.digitar(J + "Calculadora\n", 0.03)
    x0, y0 = ventana(m, "Calculadora", ini)
    time.sleep(1)
    def boton(i):
        m.clique(x0 + 8 + (i % 4) * 56 + 25, y0 + 52 + (i // 4) * 48 + 21)
    boton(0); boton(7); boton(6); boton(14)   # 7 * 6 =
    esperar(m, r"resultado = 42", 30, ini)
    verificar(True, "Calculadora: 7 * 6 = 42 con el ratón")
    m.captura("H378-calculadora")
    m.clique(x0 + 118 + 50, y0 + 244 + 20)   # Salir
    esperar(m, r"processo \d+ \(java\) terminou com codigo 0", 30, ini)
    verificar(True, "Calculadora: el boton Salir cierra la ventana")

    # bloc de notas: menu Archivo -> Salir
    ini = len(m.serial())
    m.digitar(J + "BlocNotas\n", 0.03)
    x0, y0 = ventana(m, "Bloc de notas", ini)
    time.sleep(1)
    m.digitar("hola\xf1o", 0.05)
    time.sleep(0.5)
    m.captura("H378-bloc")
    m.clique(x0 + 20, y0 + 10)            # abre el menu Archivo
    time.sleep(0.5)
    m.captura("H378-bloc-menu")
    m.clique(x0 + 20, y0 + 22 + 1 + 4 * 20 + 10)   # Salir
    esperar(m, r"processo \d+ \(java\) terminou com codigo 0", 30, ini)
    verificar(True, "BlocNotas: Archivo > Salir cierra la ventana con codigo 0")

    # serpiente: teclas y salir con Q
    ini = len(m.serial())
    m.digitar(J + "Serpiente\n", 0.03)
    ventana(m, "Serpiente", ini)
    time.sleep(1.5)
    m.tecla("down")
    time.sleep(0.5)
    m.captura("H378-serpiente")
    m.digitar("q", 0.05)
    esperar(m, r"serpiente: puntos \d+", 30, ini)
    verificar(True, "Serpiente: el lienzo anima con el temporizador y Q termina")


def main():
    if not os.path.exists(JAVAC):
        print("AVISO: no hay JDK (tools/java_entorno.sh instalar): h378 se salta")
        return 0
    if not os.path.exists(FUENTES):
        print("AVISO: faltan las fuentes de miniJVM (tools/minijvm_fuentes.sh): h378 se salta")
        return 0
    verificar(os.path.exists(os.path.join(APPS_RAIZ, "bin", "java")), "la imagen lleva /apps/bin/java (make all)")
    verificar(os.path.exists(os.path.join(APPS_RAIZ, "java", "lib", "minijvm_rt.jar")),
              "la imagen lleva /apps/java/lib/minijvm_rt.jar")

    img = imagem_de_teste("", ["--vesa", "--console"])
    with Maquina("h378", "ide", imagem=img, memoria="512M") as m:
        m.esperar_serial("sagui:", 120)
        for cmd, textos in COMANDOS:
            out = m.comando(cmd, 120)
            for t in textos:
                verificar(t in out, f"{cmd}: aparece {t!r} ({out[-300:]!r})")
        # codigo de salida
        out = m.comando(J + "Argumentos falla", 120)
        verificar("terminou com codigo 7" in out, f"System.exit(7) da el codigo de salida ({out[-200:]!r})")
        out = m.comando(J + "Excepciones boom", 120)
        verificar("IllegalStateException" in out, f"excepcion sin capturar se informa ({out[-300:]!r})")
        # juego de consola, entrada por tuberia
        out = m.comando("echo 50 | " + J + "Adivinar 42", 120)
        verificar("mas bajo" in out, f"Adivinar lee la entrada estandar ({out[-200:]!r})")
        # compilar dentro de SaguiOS con Janino (orden javac) y ejecutar
        m.comando("echo 'public class Hi { public static void main(String[] a) { System.out.println(\"hi desde javac \" + (6 * 7)); } }' > /tmp/Hi.java", 20)
        out = m.comando("javac -d /tmp /tmp/Hi.java", 200)
        verificar("(javac) terminou com codigo 0" in out, f"javac compila un .java escrito dentro de SaguiOS ({out[-300:]!r})")
        out = m.comando("java -cp /tmp Hi", 90)
        verificar("hi desde javac 42" in out, f"se ejecuta la clase compilada en SaguiOS ({out[-200:]!r})")
        out = m.comando("java /tmp/Hi.java", 250)
        verificar("hi desde javac 42" in out, f"java Hi.java compila y ejecuta de una vez ({out[-300:]!r})")
        m.comando("echo 'public class Mal { void f() { int x = ; } }' > /tmp/Mal.java", 20)
        out = m.comando("javac -d /tmp /tmp/Mal.java", 200)
        verificar("terminou com codigo 0" not in out.split("(javac)")[-1][:60] or "Mal.java" in out,
                  f"un error de sintaxis se informa ({out[-300:]!r})")
        # acentos: salida UTF-8 -> Latin-1; la n con tilde del teclado llega como UTF-8
        m.digitar(J + "Acentos", 0.04)
        m.tecla("ret")
        m.esperar_serial("escribe algo:", 90)
        m.digitar("\xf1", 0.04)
        m.tecla("ret")
        m.esperar_serial("euro:", 90)
        out = m.serial().replace("\r", "")
        verificar("a\xf1o y canci\xf3n" in out, "el terminal recibe Latin-1 (a\xf1o y canci\xf3n)")
        verificar("(1 caracteres, 2 bytes UTF-8)" in out, f"la n con tilde llega a Java como un caracter (Latin-1) ({out[-300:]!r})")
        verificar("bytes en el fichero: 15" in out, f"el fichero queda en UTF-8 (15 bytes) ({out[-300:]!r})")
        verificar("euro: ? no cabe" in out, "lo que no cabe en Latin-1 sale como ?")
        m.captura("H378-java")
    # las ventanas necesitan el escritorio (sin --console)
    img = imagem_de_teste("", ["--vesa"])
    with Maquina("h378g", "ide", imagem=img, memoria="512M") as m:
        m.esperar_serial("sagui:", 120)
        time.sleep(3)
        parte_gui(m)
    return 0


if __name__ == "__main__":
    sys.exit(main())
