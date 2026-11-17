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

/* D269: lo que el diálogo de permisos (gestor de ventanas) ve de un pedido gráfico. */
typedef struct {
    uint32_t id;
    int      pid;
    char     nome[32];      /* el nombre real del programa, puesto por el kernel */
    char     motivo[80];    /* lo que el programa dice que quiere hacer (texto suyo, no de confianza) */
    bool     comprobando;   /* la contraseña ya está en manos del supervisor */
    int      erro;          /* último fallo (ERRO_ACESSO mala, ERRO_ESPERA bloqueo) o 0 */
    bool     primeira;      /* root aún no tiene contraseña: hay que elegirla */
} dialogo_vista_t;
bool supervisor_dialogo_vista(dialogo_vista_t *v);   /* el primer pedido gráfico que espera a la persona */
void supervisor_dialogo_enviar(uint32_t id, const char *senha, const char *nova);
void supervisor_dialogo_negar(uint32_t id);

/* D340: el inicio de sesión (kernel/gui/sessao.c) pide al supervisor que compruebe la contraseña de una
 * cuenta. pedir: id (> 0) o ERRO_NAO_EXISTE (sin supervisor), ERRO_OCUPADO; resultado: 0 sin respuesta,
 * 1 bien, o el error (ERRO_ACESSO mala, ERRO_ESPERA bloqueo); con la respuesta, el pedido se libera.
 * La contraseña se copia y se borra en cuanto se entrega. */
int  supervisor_entrar_pedir(uint32_t uid, const char *senha);
int  supervisor_raiz_pedir(const char *nova);   /* D371: la primera contraseña de root desde la pantalla */
int  supervisor_entrar_resultado(uint32_t id);
void supervisor_entrar_cancelar(uint32_t id);

#endif
