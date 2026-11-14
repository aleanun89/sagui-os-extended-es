/* principal_gp.c - el punto de entrada de GNU Prolog en SaguiOS (D189).
 *
 * Los programas de SaguiOS empiezan en principal(argc, argv) (inicio.asm de la
 * libsagui). Aquí:
 *
 *   1. Los constructores (.init_array y .ctors), con los que GNU Prolog
 *      registra cada módulo enlazado (obj_chain.c), ya los ha llamado
 *      inicio.asm de la libsagui antes de principal (mejora 094), en el orden
 *      del enlace, del primero al último, como espera la configuración
 *      (OBJ_CHAIN_REVERSE_ORDER). Llamarlos aquí otra vez registraba cada
 *      módulo dos veces («Flag table full»).
 *   2. El main() de GNU Prolog (el top-level o el de pl2wam) corre sobre una
 *      pila de C propia de 4 MB reservada en el heap, como OpenTyrian (D183):
 *      la del sistema es de 64 KB y el motor recursa en C con los términos
 *      muy anidados (aritmética, comparación, escritura). Los 64 KB del fondo
 *      son una guarda: si se escribe en ellos, el emulador lo ve y para con un
 *      error fatal en vez de pisar el heap (pilhas_gp.c).
 *      Cuando la pila de usuario crezca bajo demanda (D186) esto sobra.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(int argc, char *argv[]);

#define PILHA_C   (4u * 1024u * 1024u)
#define GUARDA_C  (64u * 1024u)
#define PL_SG_CANARIO 0x5A6E1C0Du

extern volatile unsigned *pl_sg_canario;

static int g_argc;
static char **g_argv;
static int g_resultado;

static void correr_main(void)
{
  g_resultado = main(g_argc, g_argv);
}

int principal(int argc, char **argv)
{
  g_argc = argc;
  g_argv = argv;
  uint8_t *pilha = malloc(PILHA_C);
  if (!pilha)
    {
      correr_main();
      exit(g_resultado);
    }
  pl_sg_canario = (volatile unsigned *) (pilha + GUARDA_C);
  *pl_sg_canario = PL_SG_CANARIO;
  uint32_t topo = ((uint32_t) (pilha + PILHA_C)) & ~15u;   /* alineada a 16 */
  __asm__ volatile(
    "mov %%esp, %%esi\n\t"
    "mov %0, %%esp\n\t"
    "call *%1\n\t"
    "mov %%esi, %%esp\n\t"
    : : "r"(topo), "r"(correr_main) : "esi", "eax", "ecx", "edx", "memory");
  exit(g_resultado);   /* vacía los FILE */
}
