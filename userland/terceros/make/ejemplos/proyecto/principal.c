#include <stdio.h>
#include <stdlib.h>
#include "calculos.h"
#include "util.h"

int main(int argc, char **argv)
{
    int n = argc > 1 ? atoi(argv[1]) : 5;
    titulo("proyecto " VERSION_PROYECTO);
    printf("factorial(%d) = %lld\n", n, factorial(n));
    printf("%d %s primo\n", n, es_primo(n) ? "es" : "no es");
    return 0;
}
