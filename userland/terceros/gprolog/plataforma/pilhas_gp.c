/* pilhas_gp.c - los límites de las pilas de la WAM por software (D189).
 * Ver pilhas_gp.h. */
#include <stdio.h>
#include <string.h>
#include "engine_pl.h"
#include "../BipsPl/error_supp.h"
#include "pilhas_gp.h"

#define SEM_LIMITE ((WamWord *) 0xFFFFFFFFu)

/* Hasta que las pilas existen, nada que comprobar. */
WamWord *pl_sg_lim_trail = SEM_LIMITE, *pl_sg_lim_global = SEM_LIMITE, *pl_sg_lim_local = SEM_LIMITE;
int pl_sg_reserva;

static unsigned canario_inicial = PL_SG_CANARIO;
volatile unsigned *pl_sg_canario = &canario_inicial;

/* Índices de pl_stk_tbl (wam_stacks.h): 0 trail, 1 cstr, 2 global, 3 local. */
static WamWord *suave[NB_OF_STACKS], *duro[NB_OF_STACKS];

static WamWord *topo(int i)
{
  return Stack_Top(i);
}

/* La reserva: un octavo de la pila (al menos 64 KB) para tratar el error;
 * el límite duro deja 8 KB antes del final. */
void
Pl_SG_Iniciar(void)
{
  int i;

  for (i = 0; i < NB_OF_STACKS; i++)
    {
      PlLong tam = pl_stk_tbl[i].size;
      PlLong reserva = tam / 8;

      if (reserva < 16384)
	reserva = 16384;
      if (reserva > tam / 2)
	reserva = tam / 2;
      suave[i] = pl_stk_tbl[i].stack + tam - reserva;
      duro[i] = pl_stk_tbl[i].stack + tam - 2048;
      if (duro[i] < suave[i])
	duro[i] = suave[i];
    }
  pl_sg_reserva = 0;
  pl_sg_lim_trail = suave[0];
  pl_sg_lim_global = suave[2];
  pl_sg_lim_local = suave[3];
}

static void
Limites(WamWord **l)
{
  pl_sg_lim_trail = l[0];
  pl_sg_lim_global = l[2];
  pl_sg_lim_local = l[3];
}

static const char *nome_recurso[NB_OF_STACKS] = { "trail_stack", "cstr_stack", "global_stack", "local_stack" };

static void
Fatal(int i)
{
  InfStack *s = pl_stk_tbl + i;

  Pl_Fatal_Error("%s stack overflow (size: %d Kb, reached: %d Kb, environment variable used: %s)",
		 s->name, (int) Wam_Words_To_KBytes(s->size),
		 (int) Wam_Words_To_KBytes((topo(i) - s->stack)), s->env_var_name);
}

void
Pl_SG_Estouro(void)
{
  int i, excede = -1;

  if (*pl_sg_canario != PL_SG_CANARIO)
    {
      *pl_sg_canario = PL_SG_CANARIO;	/* para que el mensaje pueda salir */
      Pl_Fatal_Error("C stack overflow (SaguiOS: the C stack of gprolog is full)");
    }

  for (i = 0; i < NB_OF_STACKS; i++)
    if (i != 1 && topo(i) > (pl_sg_reserva ? duro[i] : suave[i]))
      {
	excede = i;
	break;
      }

  if (excede < 0)
    {
      /* sin exceso: si se estaba en la reserva y ya se bajó, modo normal */
      if (pl_sg_reserva)
	{
	  for (i = 0; i < NB_OF_STACKS; i++)
	    if (i != 1 && topo(i) > suave[i])
	      return;
	  pl_sg_reserva = 0;
	  Limites(suave);
	}
      return;
    }

  if (pl_sg_reserva)
    Fatal(excede);

  pl_sg_reserva = 1;
  Limites(duro);
  Pl_Err_Resource(Pl_Create_Atom((char *) nome_recurso[excede]));
}

/* Antes de copiar n palabras de golpe en la pila global. */
void
Pl_SG_Reservar_Global(PlLong n)
{
  if (pl_sg_reserva)
    Pl_SG_Estouro();		/* vuelve al modo normal si las pilas ya bajaron */
  if (n < 0 || H + n > pl_sg_lim_global)
    {
      if (pl_sg_reserva)
	{
	  if (n >= 0 && H + n <= duro[2])
	    return;
	  Fatal(2);
	}
      pl_sg_reserva = 1;
      Limites(duro);
      Pl_Err_Resource(Pl_Create_Atom("global_stack"));
    }
}
