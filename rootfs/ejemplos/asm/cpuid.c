// SPDX-License-Identifier: GPL-2.0-or-later
#include <stdio.h>

/* Lee CPUID con la hoja 0: EAX = hoja máxima, EBX/EDX/ECX = el fabricante. */
int main(void)
{
    unsigned int a, b, c, d;
    __asm__ volatile("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(0), "c"(0));
    printf("hoja maxima: %u\n", a);
    printf("ebx=%08x edx=%08x ecx=%08x\n", b, d, c);
    return 0;
}
