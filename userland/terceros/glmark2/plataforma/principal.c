// SPDX-License-Identifier: GPL-3.0-only
/* principal.c - el punto de entrada de glmark2-es2 en SaguiOS (D481).
 * Los programas del SaguiOS empiezan en principal(argc, argv) (userland/libsagui/inicio.asm, que antes llama a los
 * constructores de C++): este fichero lo une con el main() de glmark2, que no se toca. */
int main(int argc, char *argv[]);

int principal(int argc, char **argv)
{
    return main(argc, argv);
}
