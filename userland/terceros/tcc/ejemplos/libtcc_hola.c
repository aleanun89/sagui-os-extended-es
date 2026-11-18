/* SPDX-License-Identifier: GPL-3.0-only */
/* libtcc_hola.c - compilar y ejecutar C en memoria desde un programa, con libtcc (D384).
 *   tcc libtcc_hola.c -o libtcc_hola -ltcc && ./libtcc_hola
 * El programa compila una cadena con dos funciones, les da una función suya (doble) y llama
 * a lo compilado. Lo compilado usa la misma libc que el programa (printf, malloc...). */
#include <stdio.h>
#include <libtcc.h>

static const char *codigo =
    "#include <stdio.h>\n"
    "int doble(int x);\n"
    "int cuadrado(int x) { return x * x; }\n"
    "int calcula(int n) {\n"
    "    int i, suma = 0;\n"
    "    for (i = 1; i <= n; i++) suma += doble(cuadrado(i));\n"
    "    printf(\"compilado en memoria: suma de 2*i*i hasta %d\\n\", n);\n"
    "    return suma;\n"
    "}\n";

static int doble(int x)
{
    return 2 * x;
}

int main(void)
{
    TCCState *s = tcc_new();
    if (!s) {
        printf("tcc_new ha fallado\n");
        return 1;
    }
    tcc_set_output_type(s, TCC_OUTPUT_MEMORY);
    if (tcc_compile_string(s, codigo) < 0)
        return 1;
    tcc_add_symbol(s, "doble", doble);      /* lo compilado puede llamar al programa */
    if (tcc_relocate(s) < 0)
        return 1;
    int (*calcula)(int) = (int (*)(int))tcc_get_symbol(s, "calcula");
    if (!calcula)
        return 1;
    printf("calcula(10) = %d\n", calcula(10));
    tcc_delete(s);
    return 0;
}
