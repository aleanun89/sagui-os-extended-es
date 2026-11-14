// SPDX-License-Identifier: GPL-2.0-or-later
/* supervisor.h - el gestor de servicios visto desde el kernel (D216).
 *
 * El supervisor es un programa común (/bin/servicios) que el kernel lanza al
 * arrancar y relanza si muere. El kernel solo hace tres cosas por él: lanzarlo,
 * vigilarlo y, al apagar o reiniciar, avisarle y esperar (con plazo) a que pare
 * los servicios antes de desmontar los discos. Todo lo demás (definiciones,
 * dependencias, reinicios, registros) vive en el programa. */
#ifndef SAGUI_SUPERVISOR_H
#define SAGUI_SUPERVISOR_H
#include "tipos.h"

/* principal.c: lanza /bin/servicios, salvo con --sin-servicios o en una autoprueba. */
void supervisor_lancar(void);
/* El proceso 0, en su espera: si el supervisor murió, lo relanza (con espera
 * creciente y como mucho 5 veces seguidas). Barato: mira una vez por segundo. */
void supervisor_vigiar(void);
/* La tarea "apagar" (acpi.c): avisa al supervisor y espera, como mucho 20 s, a
 * que diga que ya paró los servicios. true = terminó a tiempo o no había nada. */
bool supervisor_parar_servicos(int motivo);   /* SERV_MOTIVO_* de abi.h */
/* ¿Hay un supervisor vivo con servicios en marcha? (el reinicio pasa entonces por la tarea "apagar") */
bool supervisor_ativo(void);
/* D251: pid del supervisor registrado si vive y es /bin/servicios; si no, 0. */
int supervisor_pid_confiavel(void);

#endif
