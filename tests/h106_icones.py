#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""H106 (D135) - la biblioteca de iconos del SaguiOS.

Critério:
  - la biblioteca generada es coherente: todos los iconos miden 16x16, cada
    línea tiene 16 caracteres, solo salen caracteres de la paleta y no hay
    iconos vacíos;
  - el programa `icones` arranca, enseña la rejilla de iconos con su nombre y
    responde al clic (captura en docs/capturas/).
"""
import os
import re
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from qemu import Maquina, verificar, imagem_de_teste  # noqa: E402

FICHEIRO_C = os.path.join(RAIZ, "userland", "libsagui", "icones.c")
FICHEIRO_H = os.path.join(RAIZ, "userland", "inc", "icones.h")
LADO = 16
MINIMO = 40          # iconos que hay que tener, por lo menos


def le_blocos(texto):
    """Los bloques de iconos del fichero C: (variable, [líneas])."""
    blocos = []
    for m in re.finditer(r"static const char \*const (ic_\w+)\[\] = \{(.*?)\};", texto, re.S):
        linhas = re.findall(r'"([^"]*)"', m.group(2))
        blocos.append((m.group(1), linhas))
    return blocos


def le_paleta(texto):
    """Los caracteres de la paleta, tal y como los devuelve icones_cor()."""
    corpo = re.search(r"uint32_t icones_cor\(char c\)\s*\{(.*?)\n\}", texto, re.S)
    if not corpo:
        return set()
    return set(re.findall(r"case '(.)'", corpo.group(1)))


def origem(serial):
    """El origen del área de contenido de la ventana, según el registro."""
    m = re.findall(r'criada por \d+: "Iconos del SaguiOS" (\d+)x(\d+) em \((\d+),(\d+)\)', serial)[-1]
    return int(m[2]) + 2, int(m[3]) + 24


def main():
    with open(FICHEIRO_C, encoding="utf-8") as f:
        codigo = f.read()
    with open(FICHEIRO_H, encoding="utf-8") as f:
        cabecalho = f.read()

    paleta = le_paleta(codigo)
    verificar(len(paleta) >= 8, f"la paleta tiene tonos ({len(paleta)})")
    verificar("icones_cor" in cabecalho and "icone_desenhar" in cabecalho,
              "la cabecera declara la paleta y el dibujo")

    blocos = le_blocos(codigo)
    verificar(len(blocos) >= MINIMO, f"la biblioteca tiene {len(blocos)} iconos (mínimo {MINIMO})")

    todos_caracteres = set()
    for nome, linhas in blocos:
        verificar(len(linhas) == LADO, f"{nome}: tiene {len(linhas)} líneas (deben ser {LADO})")
        for i, linha in enumerate(linhas):
            verificar(len(linha) == LADO, f"{nome}: la línea {i} tiene {len(linha)} caracteres")
            todos_caracteres.update(linha)
        verificar(any(c != "." for c in "".join(linhas)), f"{nome}: no está vacío")
        # ningún carácter fuera de la paleta (el punto es el transparente)
        fora = set("".join(linhas)) - paleta - {"."}
        verificar(not fora, f"{nome}: solo usa la paleta (raros: {''.join(sorted(fora))})")

    verificar(todos_caracteres <= paleta | {"."}, "todos los iconos usan la paleta compartida")
    # la tabla debe tener una entrada por icono
    entradas = re.findall(r'\{ "([^"]+)", ic_\w+ \}', codigo)
    verificar(len(entradas) == len(blocos), f"la tabla tiene {len(entradas)} entradas")
    verificar(len(set(entradas)) == len(entradas), "no hay nombres repetidos en la tabla")
    verificar(all(re.fullmatch(r"[a-z0-9-]+", n) for n in entradas), "los nombres son identificadores")

    # ---- el programa, en la máquina ----
    with Maquina("h106", "ide", imagem=imagem_de_teste("/bin/icones", ["--vesa"])) as m:
        m.esperar_serial("icones: pronto", 120)
        m.esperar_serial("icones: %d icones" % len(blocos), 30)
        verificar(True, "el programa arranca y dice cuántos iconos hay")
        m.captura("H106-icones")

        # un clic en el primer icono: el nombre sale en el registro
        ox, oy = origem(m.serial())
        m.clique(ox + 41, oy + 26)
        m.esperar_serial("icones: escolhido ", 10)
        nome = re.findall(r"icones: escolhido (\S+)", m.serial())[-1]
        verificar(nome in entradas, f"el clic elige un icono de la tabla ({nome})")
        m.captura("H106-icones-clic")

    return 0


if __name__ == "__main__":
    sys.exit(main())
