#include "calculos.h"

long long factorial(int n)
{
    long long r = 1;
    for (int i = 2; i <= n; i++) r *= i;
    return r;
}

int es_primo(int n)
{
    if (n < 2) return 0;
    for (int d = 2; d * d <= n; d++)
        if (n % d == 0) return 0;
    return 1;
}
