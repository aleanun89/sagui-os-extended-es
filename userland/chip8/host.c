// SPDX-License-Identifier: GPL-2.0-or-later
/* host.c - o main() das versões do host do chip8asm e do chip8 (só -t),
 * compiladas com o gcc normal no build (D071). PRINCIPAL vem do Makefile. */
#include "chip8.h"

int PRINCIPAL(int argc, char **argv);

int main(int argc, char **argv) { return PRINCIPAL(argc, argv); }
