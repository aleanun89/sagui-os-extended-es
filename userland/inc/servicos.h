// SPDX-License-Identifier: GPL-2.0-or-later
/* servicos.h - el gestor de servicios (D216, mejora 074): lo que comparten el
 * supervisor (/bin/servicios), el comando "servicio", el cron y el
 * Administrador de tareas.
 *
 * Cómo hablan entre ellos (sin llamadas al sistema nuevas, solo archivos en /tmp):
 *   - el supervisor escribe el estado de todos los servicios en
 *     SERV_ARQ_ESTADO cada vez que algo cambia: "servicios <pid> <arranque>",
 *     una línea por servicio con 11 campos separados por tabuladores (nombre,
 *     estado, pid, reinicios, desde_ms, arrancar, usuario, descripción,
 *     detalle, registro, programa) y una última línea "fin" (quien lee sin
 *     ella, vuelve a leer);
 *   - una orden ("parar cron") se deja en SERV_DIR_PEDIDOS/<pid>.ped (escrita
 *     con otro nombre y renombrada, para que nunca se lea a medias); el
 *     supervisor la atiende en menos de 0,2 s y responde en <pid>.res.
 * El kernel solo sabe quién es el supervisor (SERV_SUPERVISOR). */
#ifndef SAGUI_SERVICOS_H
#define SAGUI_SERVICOS_H
#include "sagui.h"

#define SERV_DIR_SISTEMA   "/etc/servicios"            /* los del sistema (el ramdisk) */
#define SERV_DIR_USUARIO   "/home/.config/servicios"   /* los del usuario (persisten si /home es persistente) */
#define SERV_ARQ_ACTIVADOS "/home/.config/servicios/activados"   /* "nombre=si|no": servicio activar/desactivar */
#define SERV_DIR_COMUM     "/tmp/.servicios"
#define SERV_ARQ_ESTADO    "/tmp/.servicios/estado"
#define SERV_DIR_PEDIDOS   "/tmp/.servicios/pedidos"

/* Estados (la palabra que va en el archivo de estado). */
#define SERV_E_PARADO      "parado"
#define SERV_E_ESPERANDO   "esperando"     /* a sus dependencias (red, /home, otro servicio) */
#define SERV_E_MARCHA      "marcha"
#define SERV_E_REINICIANDO "reiniciando"   /* murió: espera creciente antes de volver a lanzarlo */
#define SERV_E_PARANDO     "parando"
#define SERV_E_FALLIDO     "fallido"       /* demasiados fallos seguidos: ya no se reinicia solo */

typedef struct {
    char     nome[32];
    char     estado[16];      /* SERV_E_* */
    int      pid;             /* 0 si no está en marcha */
    unsigned reinicios;       /* veces que se ha vuelto a lanzar solo */
    unsigned desde_ms;        /* tempo_ms() del último cambio de estado */
    bool     arrancar;        /* arranque automático (con activar/desactivar aplicados) */
    bool     usuario;         /* definido en /home/.config/servicios */
    char     descricao[64];
    char     detalhe[64];     /* "espera: red", "terminó con código 1"... */
    char     registro[96];    /* el archivo donde va su salida */
} servico_info_t;

/* Lee el estado que publica el supervisor. Devuelve cuántos servicios, o
 * ERRO_NAO_EXISTE si el supervisor no está en marcha. */
int  servicos_ler_estado(servico_info_t *v, int max);
/* Manda una orden al supervisor (iniciar, parar, reiniciar, activar,
 * desactivar, recargar) y espera su respuesta (como mucho 5 s). Devuelve 0 si
 * la aceptó, 1 si la rechazó (el motivo en "resposta"), o un ERRO_* si no hay
 * supervisor o no contestó. */
int  servicos_pedir(const char *ordem, const char *nome, char *resposta, int max);
/* "marcha" -> "en marcha"... (para mostrar). */
const char *servicos_texto_estado(const char *estado);
/* Dónde van los registros: /logs si existe (la partición SAGUI-LOGS, D120); si no, /tmp. */
const char *servicos_pasta_registros(void);
/* ¿Hay algo montado exactamente en esa carpeta? ("/home" persistente, "/logs"...) */
bool servicos_montado(const char *ponto);
/* Crea la carpeta y las que falten por encima (como mkdir -p). */
void servicos_criar_pastas(const char *caminho);

#endif
