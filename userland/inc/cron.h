// SPDX-License-Identifier: GPL-2.0-or-later
/* cron.h - las líneas de una tabla de cron (D216, mejora 074): análisis y
 * cálculo de la próxima hora. Lo comparten el servicio cron y crontab.
 *
 * Formato clásico de 5 campos: "minuto hora día mes día_semana orden".
 * Cada campo admite *, números, listas (1,3,5), rangos (1-5), pasos (*\/15,
 * 0-30/10) y, en mes y día de la semana, nombres en inglés (jan, mon...). El
 * día de la semana va de 0 a 7 (0 y 7 son el domingo). Como en Vixie cron, si
 * el día del mes y el de la semana están restringidos los dos, basta con que
 * coincida uno.
 *
 * Atajos: @reboot (al arrancar el cron), @hourly, @daily (= @midnight),
 * @weekly, @monthly y @yearly (= @annually).
 *
 * Tareas perdidas (como anacron): una línea @hourly, @daily, @weekly,
 * @monthly o @yearly que no se ejecutó en su periodo porque el PC estaba
 * apagado se ejecuta una vez al arrancar. Con "!" delante ("!@daily ...") no.
 *
 * Las líneas "NOMBRE=valor" son variables que reciben las tareas de esa tabla.
 * Las horas son las de la zona horaria del sistema (D080). */
#ifndef SAGUI_CRON_H
#define SAGUI_CRON_H
#include "sagui.h"

enum { CRON_NORMAL = 0, CRON_ARRANQUE, CRON_HORARIA, CRON_DIARIA, CRON_SEMANAL, CRON_MENSUAL, CRON_ANUAL };

typedef struct {
    uint32_t minutos_baixo, minutos_alto;   /* bits 0-31 y 32-59 */
    uint32_t horas;                         /* bits 0-23 */
    uint32_t dias;                          /* bits 1-31 */
    uint32_t meses;                         /* bits 1-12 */
    uint32_t semana;                        /* bits 0-6 (el 7 se guarda como 0) */
    bool     dia_livre, semana_livre;       /* el campo empezaba por "*" */
    int      especial;                      /* CRON_* */
    bool     recuperar;                     /* tareas perdidas (solo atajos periódicos) */
} cron_quando_t;

#define CRON_COMANDO_MAX 256
typedef struct {
    cron_quando_t q;
    char comando[CRON_COMANDO_MAX];
    int  linha;                             /* número de línea en la tabla (desde 1) */
} cron_tarefa_t;

/* Analiza una línea. Devuelve 1 si es una tarea (en *t), 2 si es una variable
 * (NOMBRE=valor), 0 si está vacía o es un comentario, y -1 si tiene un error
 * (el motivo, en español, en "erro"). */
int  cron_analisar_linha(const char *linha, cron_tarefa_t *t, char *erro, int max_erro);
/* ¿Toca en ese minuto (hora local ya calculada)? */
bool cron_casa(const cron_quando_t *q, const fuso_data_t *d);
/* El primer instante UTC (en un minuto exacto) después de "depois" en que toca,
 * con la zona del proceso. 0 = ninguno en los próximos 5 años (o @reboot). */
uint32_t cron_proxima(const cron_quando_t *q, uint32_t depois);
/* Segundos del periodo de un atajo periódico (para las tareas perdidas); 0 = no tiene. */
uint32_t cron_periodo(int especial);
/* Analiza una tabla entera; escribe los errores ("línea N: ...") en fd_erros
 * (o no, si es < 0). Devuelve cuántos errores encontró. */
int  cron_comprobar_tabla(const char *texto, int fd_erros);

#endif
