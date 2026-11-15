// SPDX-License-Identifier: GPL-2.0-or-later
// cxxrt_sistema_pc.h - el «sistema» mínimo de las pruebas de unidad del runtime de C++ en el PC
// (D228, D294): malloc, free, abort, strcmp, la salida por int 0x80 de Linux de 32 bits y el punto
// de entrada. Lo comparten cxxrt_unidad.cpp y cxx_rtti_unidad.cpp, que se enlazan sin la libc del PC.
#ifndef CXXRT_SISTEMA_PC_H
#define CXXRT_SISTEMA_PC_H
#include <stddef.h>
#include <stdint.h>

// ------------------------------------------------------------ el "sistema"

extern "C" {

static long llamada(long n, long a, long b, long c)
{
    long r;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"(n), "b"(a), "c"(b), "d"(c) : "memory");
    return r;
}

[[noreturn]] static void salir(int codigo)
{
    llamada(1, codigo, 0, 0);
    for (;;) {}
}

static size_t largo(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
static void escribir(int fd, const char *s) { llamada(4, fd, (long)s, (long)largo(s)); }

static unsigned char monton[1 << 20];
static size_t usado = 0;
static int vivos = 0;   // bloques reservados y no liberados

void *malloc(size_t n)
{
    n = (n + 15) & ~(size_t)15;
    if (usado + n + 16 > sizeof monton) return nullptr;
    void *p = monton + usado + 16;
    usado += n + 16;
    vivos++;
    return p;
}

void free(void *p) { if (p) vivos--; }

void abort(void)
{
    escribir(2, "abort\n");
    salir(134);
}

int strcmp(const char *a, const char *b)
{
    while (*a && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

int atexit(void (*)(void)) { return 0; }

void cxxrt_aviso(const char *texto)
{
    escribir(2, texto);
    escribir(2, "\n");
}

int main(int argc, char **argv);

// El punto de entrada: argc y argv donde los deja Linux; la pila, a 16.
__asm__(
    ".text\n"
    ".globl _inicio\n"
    "_inicio:\n"
    "    xorl %ebp, %ebp\n"
    "    movl (%esp), %eax\n"
    "    leal 4(%esp), %ecx\n"
    "    andl $-16, %esp\n"
    "    subl $8, %esp\n"
    "    pushl %ecx\n"
    "    pushl %eax\n"
    "    call main\n"
    "    movl %eax, %ebx\n"
    "    movl $1, %eax\n"
    "    int $0x80\n");

} // extern "C"

#endif
