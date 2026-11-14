// SPDX-License-Identifier: GPL-2.0-or-later
/* principal_ns.c - el arranque de /bin/netsurf en el SaguiOS (D184).
 *
 * La libsagui empieza los programas por principal(); NetSurf (el frontend
 * framebuffer del upstream, que no se toca) tiene un main(). Aquí, antes:
 *
 *   - los constructores (.init_array, ver netsurf.ld): libnsfb registra con
 *     ellos sus superficies, entre ellas la nuestra (superficie_sagui.c);
 *   - HOME, si no está (NetSurf guarda en $HOME/.netsurf las cookies, el
 *     historial y las opciones);
 *   - el fetcher de http/https (fetch_http.c), que sustituye a libcurl, y
 *     el manejador de PNG y JPEG (imagem_sagui.c), que sustituye a libpng
 *     y libjpeg;
 *   - una pila propia: el sistema da a cada programa 64 KB de pila fijos
 *     (PILHA_USUARIO_TAM) y NetSurf es recursivo (el layout de las cajas, el
 *     árbol del DOM, el CSS). Como en la D183 del OpenTyrian, main() corre
 *     sobre una pila de 4 MB reservada en el heap. Carencia: una pila que
 *     crezca bajo demanda.
 */
#include "sagui.h"

#include <stdlib.h>

#define PILHA_NETSURF (4u * 1024u * 1024u)

extern void (*__init_array_start[])(void);
extern void (*__init_array_end[])(void);

int main(int argc, char **argv);
void ns_sagui_registrar_http(void);
int nssagui_init(void);   /* imagem_sagui.c: PNG y JPEG con la libsagui */

static int ns_argc;
static char **ns_argv;
static int ns_resultado;

static void correr_netsurf(void)
{
    ns_resultado = main(ns_argc, ns_argv);
}

static int main_com_pilha_propia(int argc, char **argv)
{
    ns_argc = argc;
    ns_argv = argv;
    uint8_t *pilha = malloc(PILHA_NETSURF);
    if (!pilha) return main(argc, argv);
    uint32_t topo = ((uint32_t)(pilha + PILHA_NETSURF)) & ~15u;   /* alineada a 16 (SSE) */
    __asm__ volatile(
        "mov %%esp, %%esi\n\t"
        "mov %0, %%esp\n\t"
        "call *%1\n\t"
        "mov %%esi, %%esp\n\t"
        : : "r"(topo), "r"(correr_netsurf) : "esi", "eax", "ecx", "edx", "memory");
    free(pilha);
    return ns_resultado;
}

int setenv(const char *nome, const char *valor, int substituir);

int principal(int argc, char **argv)
{
    /* Los constructores (libnsfb registra así sus superficies) ya los llamó
     * inicio.asm antes de principal (mejora 094, punto 10). */
    if (!obter_env("HOME")) setenv("HOME", "/home", 0);
    ns_sagui_registrar_http();
    nssagui_init();
    return main_com_pilha_propia(argc, argv);
}
