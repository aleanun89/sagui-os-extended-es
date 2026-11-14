#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H156 - GNU Prolog en SaguiOS: el intérprete gprolog (D189).

Criterio, en el terminal (imagen --console), escribiendo como un usuario:
  - gprolog arranca y muestra el top-level («| ?-»);
  - consultas con varias soluciones (se pide la siguiente con «;»):
    append/3 al revés, member/2;
  - consult del ejemplo /ejemplos/familia.pl (lanza /apps/bin/pl2wam) y consultas
    sobre él: abuelo/2 con dos soluciones, findall/3, sort/2, listing/1;
  - aritmética entera y real (los reales salen como en el GNU Prolog de
    Linux: 1/3.0 = 0.33333333333333331, conversión exacta), atom_codes/2,
    format/2 con ~w ~a ~d ~n, read/1 desde el terminal, assertz/1;
  - un error ISO (X is foo + 1) da su mensaje y el top-level sigue vivo;
  - recursión profunda: una lista de 100 000 elementos con length/2 y una
    suma no final sobre ella, sin fallo de página;
  - desbordamientos (D189, pilas comprobadas por software): una recursión sin
    fin da resource_error(local_stack) y length(L, 10000000)
    resource_error(global_stack), y después el top-level sigue funcionando;
  - halt vuelve al shell.
Al final: ni «Fallo de página» ni pánico en la serie. Captura en
docs/capturas/H156-gprolog.png.
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_console, FalhaTeste  # noqa: E402

PROMPT = "| ?- "


class TopLevel:
    """Escribe consultas en el top-level y devuelve lo que salió."""

    def __init__(self, m):
        self.m = m

    def esperar_prompt(self, antes, tempo=60):
        """Hasta que haya más «| ?- » que antes. Si gprolog pregunta por otra
        solución («... ? »), contesta «;» (todas las soluciones)."""
        fim = time.time() + tempo
        while True:
            s = self.m.serial().replace("\r", "")
            if s.count(PROMPT) > antes:
                return
            if s.rstrip(" ").endswith("?") and not s.endswith(PROMPT):
                self.m.digitar(";\n", 0.04)
                time.sleep(0.5)
                continue
            if time.time() > fim:
                raise FalhaTeste("gprolog no volvió al «| ?-» (últimas líneas: %r)" % s[-300:])
            time.sleep(0.3)

    def consulta(self, texto, tempo=60):
        antes_txt = self.m.serial().replace("\r", "")
        antes = antes_txt.count(PROMPT)
        self.m.digitar(texto + "\n", 0.04)
        self.esperar_prompt(antes, tempo)
        return self.m.serial().replace("\r", "")[len(antes_txt):]


def main():
    with Maquina("h156", "ide", imagem=imagem_console(apps=True)) as m:
        m.esperar_serial("sagui:/$", 120)
        out = m.comando("ls /apps/bin/gprolog /apps/bin/pl2wam /ejemplos/familia.pl")
        verificar("no existe" not in out.lower() and "/apps/bin/pl2wam" in out, "gprolog, pl2wam y el ejemplo están en la imagen")

        p = TopLevel(m)
        antes = m.serial().count(PROMPT)
        m.digitar("gprolog\n", 0.04)
        p.esperar_prompt(antes, 90)
        verificar("GNU Prolog 1.5.0" in m.serial(), "el top-level arranca (GNU Prolog 1.5.0)")

        out = p.consulta("X is 6 * 7.")
        verificar("X = 42" in out, "aritmética entera")
        out = p.consulta("append(X, Y, [1,2]).")
        verificar("X = []" in out and "Y = [1,2]" in out and "X = [1]" in out and "Y = [2]" in out
                  and "X = [1,2]" in out and "Y = []" in out, "append/3 da las tres soluciones con «;»")
        out = p.consulta("member(X, [a,b,c]).")
        verificar("X = a" in out and "X = b" in out and "X = c" in out, "member/2 con varias soluciones")

        out = p.consulta("['/ejemplos/familia.pl'].", tempo=180)
        verificar("familia.pl" in out and "compiled" in out and "yes" in out and "error" not in out,
                  "consult del ejemplo (pl2wam lanzado desde gprolog)")
        out = p.consulta("abuelo(X, ana).")
        verificar("X = pedro" in out and "X = maria" in out, "abuelo/2 con dos soluciones")
        out = p.consulta("descendientes(pedro, L).")
        verificar("L = [ana,carlos,juan,luis,sofia]" in out, "findall/3 y sort/2 sobre el ejemplo")
        out = p.consulta("findall(P-H, progenitor(P, H), L), length(L, N).")
        verificar("N = 9" in out, "findall/3 y length/2")
        out = p.consulta("invertir([a,b,c], R), suma([3,4,5], S).")
        verificar("R = [c,b,a]" in out and "S = 12" in out, "listas del ejemplo")
        out = p.consulta("listing(abuelo/2).")
        verificar("abuelo(A, B) :-" in out and "progenitor(A, C)" in out, "listing/1")

        out = p.consulta("X is 1 / 3.0, Y is sqrt(2), Z is 2.5 * 4.")
        verificar("X = 0.33333333333333331" in out and "Y = 1.4142135623730951" in out and "Z = 10.0" in out,
                  "reales escritos como en Linux (conversión exacta)")
        out = p.consulta("X = 0.1, Y is X * 3.")
        verificar("X = 0.10000000000000001" in out and "Y = 0.30000000000000004" in out, "0.1 y 0.1*3 exactos")
        out = p.consulta("atom_codes(A, [104,111,108,97]), atom_length(A, N).")
        verificar("A = hola" in out and "N = 4" in out, "atom_codes/2 y atom_length/2")
        out = p.consulta("format(\"~w y ~a: ~d~n\", [f(x), hola, 42]).")
        verificar("f(x) y hola: 42" in out, "format/2")
        out = p.consulta("assertz(cuenta(7)), cuenta(X).")
        verificar("X = 7" in out, "assertz/1")

        antes_txt = m.serial().replace("\r", "")
        antes = antes_txt.count(PROMPT)
        m.digitar("read(T).\n", 0.04)
        time.sleep(2)
        m.digitar("dato(1, [x]).\n", 0.04)
        p.esperar_prompt(antes)
        out = m.serial().replace("\r", "")[len(antes_txt):]
        verificar("T = dato(1,[x])" in out, "read/1 desde el terminal")

        out = p.consulta("X is foo + 1.")
        verificar("type_error(evaluable,foo/0)" in out, "error ISO: type_error(evaluable, foo/0)")
        out = p.consulta("X is 1 + 1.")
        verificar("X = 2" in out, "el top-level sigue vivo tras el error")

        out = p.consulta("length(_L, 100000), length(_L, N), maplist(=(1), _L), suma(_L, S).", tempo=240)
        verificar("N = 100000" in out and "S = 100000" in out, "lista de 100 000 y recursión no final de 100 000 niveles")

        out = p.consulta("assertz((sin_fin(N) :- N1 is N + 1, sin_fin(N1), true)).")
        out = p.consulta("sin_fin(0).", tempo=600)
        verificar("resource_error(local_stack)" in out, "una recursión sin fin da resource_error(local_stack)")
        out = p.consulta("length(L, 10000000).", tempo=600)
        verificar("resource_error(global_stack)" in out, "una lista enorme da resource_error(global_stack)")
        out = p.consulta("statistics(runtime, [T|_]), integer(T), X = despues.")
        verificar("X = despues" in out, "el top-level sigue tras los desbordamientos; statistics/2")
        m.captura("H156-gprolog")

        out = m.comando("halt.", tempo=60)
        verificar("sagui:" in out, "halt vuelve al shell")

        s = m.serial()
        verificar("Fallo de p" not in s and "PANICO" not in s and "Fatal Error" not in s,
                  "sin fallo de página, pánico ni error fatal")


if __name__ == "__main__":
    main()
