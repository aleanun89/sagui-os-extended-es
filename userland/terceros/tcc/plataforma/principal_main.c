/* SPDX-License-Identifier: GPL-3.0-only */
/* principal_main.c - para los programas que compila TCC en SaguiOS (D384): va en /apps/tcc/lib/libc.a.
 * Los programas de SaguiOS empiezan en principal(argc, argv) (inicio.asm, en crt1.o); los de C estándar
 * tienen main(). Si el programa no define principal, el enlazador saca esta del archivo: llama a main
 * y sale con exit(), que vacía los búferes de stdio y llama a lo registrado con atexit. */
#include <stdlib.h>

extern char **environ;
int main(int argc, char **argv, char **envp);

int principal(int argc, char **argv)
{
    exit(main(argc, argv, environ));
}
