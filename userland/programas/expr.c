// SPDX-License-Identifier: GPL-2.0-or-later
/* expr - evalúa una expresión de sus argumentos (D232), como el de Linux.
 *   expr 2 + 3      expr 7 \* 6      expr length hola      expr substr hola 2 2      expr abc : 'a.' */
#include "expr_logica.h"

static const char *AYUDA =
    "uso: expr EXPRESIÓN\n"
    "  Operadores (cada uno es un argumento; los símbolos del shell se escriben \\* \\( \\) \\< \\> \\| \\&):\n"
    "    A | B   A si no es vacío ni 0, si no B      A & B   A si ninguno es vacío ni 0, si no 0\n"
    "    = != < <= > >=  comparan (números si lo son, si no texto) y dan 1 o 0\n"
    "    + - * / %  aritmética de enteros           CADENA : REGEX   cuántos caracteres del principio coinciden\n"
    "    length CADENA   substr CADENA POS LARGO   index CADENA CARACTERES   match CADENA REGEX\n"
    "  Código de salida: 0 si el resultado no es vacío ni 0, 1 si lo es, 2 si la expresión es errónea.\n";

int principal(int argc, char **argv)
{
    if (argc == 2 && (compara_texto(argv[1], "--help") == 0 || compara_texto(argv[1], "--ayuda") == 0)) {
        escrever_texto(SAIDA, AYUDA);
        return 0;
    }
    char sal[256], err[96];
    int r = expr_avaliar(argc - 1, argv + 1, sal, sizeof sal, err, sizeof err);
    if (r == 2) { imprimir_em(ERROS, "expr: %s\n", err); return 2; }
    imprimir("%s\n", sal);
    return r;
}
