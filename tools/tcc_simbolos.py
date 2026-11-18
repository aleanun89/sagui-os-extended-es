#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""tcc_simbolos.py - la tabla de símbolos que «tcc -run» ofrece al programa (D384).

    tools/tcc_simbolos.py SALIDA.c OBJETO.o...

Lee con «nm» los símbolos globales definidos en los objetos (la libc común, su libm y la base de la
libsagui que van dentro de /apps/bin/tcc) y escribe un C con una tabla ordenada por nombre
{"nombre", &símbolo}. Cada símbolo se declara con una etiqueta de ensamblador (__asm__("nombre")),
así el C generado no choca con ninguna cabecera ni palabra reservada. Ver
userland/terceros/tcc/plataforma/tcc_sagui_run.c.
"""
import subprocess
import sys

# Lo que no se ofrece: el arranque y el main del propio tcc.
EXCLUIDOS = {"_inicio", "_start", "principal", "main", "__init_array_start", "__init_array_end"}


def simbolos(objetos):
    salida = subprocess.run(["nm", "--defined-only", "-g", "-P"] + objetos,
                            check=True, capture_output=True, text=True).stdout
    nomes = set()
    for linha in salida.splitlines():
        partes = linha.split()
        if len(partes) < 2 or linha.endswith(":"):
            continue
        nome, tipo = partes[0], partes[1]
        if tipo not in "TDBRWVCG" or nome in EXCLUIDOS:
            continue
        if not all(c.isalnum() or c in "_$." for c in nome):
            continue
        nomes.add(nome)
    return sorted(nomes, key=lambda n: n.encode())


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    nomes = simbolos(sys.argv[2:])
    linhas = ["/* Generado por tools/tcc_simbolos.py (D384): no editar. */",
              "typedef struct { const char *nome; void *endereco; } tcc_simbolo_sagui_t;"]
    for i, n in enumerate(nomes):
        linhas.append('extern char tcc_s%d __asm__("%s");' % (i, n))
    linhas.append("const tcc_simbolo_sagui_t tcc_simbolos_sagui[] = {")
    for i, n in enumerate(nomes):
        linhas.append('    { "%s", &tcc_s%d },' % (n, i))
    linhas.append("};")
    linhas.append("const int tcc_simbolos_sagui_n = %d;" % len(nomes))
    with open(sys.argv[1], "w", encoding="utf-8") as f:
        f.write("\n".join(linhas) + "\n")


if __name__ == "__main__":
    main()
