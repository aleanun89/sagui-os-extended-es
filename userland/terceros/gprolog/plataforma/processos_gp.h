/* processos_gp.h - lanzar programas desde GNU Prolog en SaguiOS (D189):
 * spawn/3, system/1, exec/5 y el pl2wam de consult/1. */
#ifndef PROCESSOS_GP_H
#define PROCESSOS_GP_H
#include <stdio.h>
int Pl_SG_Spawn(char *arg[]);
int Pl_SG_Spawn_Redirect(char *arg[], int detach, FILE **f_in, FILE **f_out, FILE **f_err);
#endif
