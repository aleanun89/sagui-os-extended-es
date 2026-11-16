// SPDX-License-Identifier: GPL-2.0-or-later
/* usuarios.c - SIS_USUARIO: la identidad de los procesos (D255, mejora 098, fase 1).
 *
 * Cada proceso lleva uid y gid en su líder (los hilos usan los del proceso).
 * Los hereda el hijo al crearse (carregador.c); los programas que lanza el
 * kernel (escritorio, Terminal, menú Inicio) reciben los de la sesión
 * (permisos_uid_inicial: sagui, o root en el modo rescate). Solo root cambia
 * su propia identidad (USU_FIJAR) o la del próximo hijo (USU_COMO: así el
 * gestor de servicios lanza cada servicio con su usuario sin dejar de ser root
 * y sin un momento en que el hijo corra con la identidad equivocada). Que la
 * operación sea de root lo comprueba permisos.c antes de llegar aquí; se
 * vuelve a mirar por si alguien llama a esto desde otro sitio. */
#include "sistema.h"
#include "abi.h"
#include "conf_sistema.h"
#include "console.h"
#include "permisos.h"
#include "processo.h"
#include "sessao.h"

static bool uid_valido(uint32_t uid, uint32_t gid)
{
    return uid <= UID_MAX && gid <= UID_MAX;
}

int32_t sis_usuario(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    (void)c; (void)d;
    processo_t *eu = processo_recursos();
    switch (op) {
    case USU_CONSULTAR:
        return (int32_t)(eu->uid | eu->gid << 16);
    case USU_DE_PID: {
        processo_t *p = a ? processo_por_pid((int)a) : eu;
        if (!p || p->estado == P_ZUMBI) return ERRO_SEM_PROCESSO;
        p = processo_lider(p);
        return (int32_t)(p->uid | p->gid << 16);
    }
    case USU_ESTADO:
        return (permisos_modo_rescate() ? USU_EST_RESCATE : 0) | (eu->de_servico ? USU_EST_SERVICIO : 0) |
               (permisos_sesion_raiz() ? USU_EST_PRUEBA : 0) | (conf_sistema_montada() ? 0 : USU_EST_EN_VIVO);
    case USU_FIJAR:
        if (eu->uid != 0) return ERRO_ACESSO;
        if (!uid_valido(a, b)) return ERRO_INVALIDO;
        if (a != eu->uid || b != eu->gid)
            klog("[USUARIOS] %s (pid %d) pasa de uid %u a uid %u (gid %u)\n", eu->nome, eu->pid, eu->uid, a, b);
        permisos_casa_mudar_uid(eu, a);   /* D340 */
        eu->uid = a;
        eu->gid = b;
        return 0;
    case USU_COMO:
        if (eu->uid != 0) return ERRO_ACESSO;
        if ((int32_t)a == -1) { eu->como_uid = eu->como_gid = -1; return 0; }
        if (!uid_valido(a, b)) return ERRO_INVALIDO;
        eu->como_uid = (int32_t)a;
        eu->como_gid = (int32_t)b;
        return 0;
    case USU_SESSAO:   /* D340 */
        return (int32_t)(permisos_uid_inicial() | permisos_sessao_gid() << 16);
    case USU_CERRAR_SESION:
        /* Solo la sesión local: un servicio (SSH, cron) no cierra la sesión de quien está delante. */
        if (eu->de_servico) return ERRO_ACESSO;
        if (a != USU_SAIR_CERRAR && a != USU_SAIR_CAMBIAR) return ERRO_INVALIDO;
        return sessao_pedir_fim((int)a, eu->pid);
    }
    return ERRO_INVALIDO;
}
