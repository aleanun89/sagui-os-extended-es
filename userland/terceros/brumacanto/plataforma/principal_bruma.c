// SPDX-License-Identifier: MIT
/* principal_bruma.c - el punto de entrada de SaguiOS (D444): los programas empiezan en principal(argc, argv)
 * (userland/libsagui/inicio.asm) y Brumacanto, en C estándar, tiene main(). */
int main(int argc, char **argv);
int principal(int argc, char **argv) { return main(argc, argv); }
