/* pilhas_gp.h - comprobación por software de las pilas de la WAM (D189).
 *
 * En Linux, GNU Prolog pone una página protegida tras cada pila y detecta el
 * desbordamiento con SIGSEGV. SaguiOS no tiene señales ni mprotect: sin esto,
 * una recursión sin fin escribiría fuera de las pilas (en el resto del heap,
 * sin fallo de página que lo pare). En su lugar, los puntos por donde crecen
 * las pilas (crear un entorno o un punto de elección, poner una lista o una
 * estructura, cada llamada a un predicado consultado y las copias grandes)
 * miran el tope contra un límite. Al pasarlo se lanza
 * error(resource_error(global_stack), _) (o local_stack, trail_stack), que el
 * top-level muestra y del que se vuelve; para tratarlo queda una reserva al
 * final de cada pila. Si se pasa también la reserva, error fatal (como el
 * GNU Prolog de Linux). Incluir después de engine_pl.h. */
#ifndef PILHAS_GP_H
#define PILHAS_GP_H

extern WamWord *pl_sg_lim_trail, *pl_sg_lim_global, *pl_sg_lim_local;
extern int pl_sg_reserva;
extern volatile unsigned *pl_sg_canario;   /* la guarda de la pila de C (principal_gp.c) */
#define PL_SG_CANARIO 0x5A6E1C0Du

void Pl_SG_Iniciar(void);
void Pl_SG_Estouro(void);
void Pl_SG_Reservar_Global(PlLong n);

/* Con pl_sg_reserva también: tras un desbordamiento hay que volver al modo
 * normal en cuanto las pilas bajan, aunque no se llame a nada consultado
 * (el top-level y los predicados en C no pasan por el emulador); si no, el
 * siguiente desbordamiento, ya en la reserva, sería un error fatal. */
#define Pl_SG_Verificar()                                               \
  do {                                                                  \
    if (__builtin_expect(pl_sg_reserva || H > pl_sg_lim_global ||       \
                         TR > pl_sg_lim_trail || Local_Top > pl_sg_lim_local, 0)) \
      Pl_SG_Estouro();                                                  \
  } while (0)

/* En cada llamada del emulador de byte-code (el código consultado): además
 * vuelve al modo normal tras un desbordamiento y mira la pila de C. */
#define Pl_SG_Verificar_Bc()                                            \
  do {                                                                  \
    if (__builtin_expect(pl_sg_reserva || H > pl_sg_lim_global ||       \
                         TR > pl_sg_lim_trail || Local_Top > pl_sg_lim_local || \
                         *pl_sg_canario != PL_SG_CANARIO, 0))           \
      Pl_SG_Estouro();                                                  \
  } while (0)

#endif
