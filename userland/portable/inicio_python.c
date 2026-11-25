// SPDX-License-Identifier: GPL-2.0-or-later
/* inicio_python.c - el punto de entrada del python portable (D459, VisualPythonS).
 * En SaguiOS lo hace userland/micropython/inicio_python.c (elige la FPU antes de nada); en el PC basta con llamar
 * al principal del MicroPython de SaguiOS desde el main de la capa (sistema_sdl.c). */
int mp_principal(int argc, char **argv);

int principal(int argc, char **argv) { return mp_principal(argc, argv); }
