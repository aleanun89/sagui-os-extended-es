/* SPDX-License-Identifier: GPL-3.0-only */
/* principal_main.c - los programas de SaguiOS empiezan en principal(argc, argv) (inicio.asm); Valkey tiene
 * main(). Esta llama a main y sale con exit(), que vacía stdio y llama a lo registrado con atexit. */
#include <stdlib.h>

extern char **environ;
int main(int argc, char **argv, char **envp);

int principal(int argc, char **argv)
{
    exit(main(argc, argv, environ));
}
