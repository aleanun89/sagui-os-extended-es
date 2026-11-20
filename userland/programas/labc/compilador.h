/* SPDX-License-Identifier: GPL-3.0-only */
/* compilador.h - el Laboratorio de C (D414) compila con libtcc en memoria y ejecuta lo compilado en un
 * proceso hijo. Este módulo solo usa la libc (sin sagui.h): lo llama principal.c. */
#ifndef LABC_COMPILADOR_H
#define LABC_COMPILADOR_H

#define LAB_MSG_MAX 200

typedef struct {
    int linea;               /* 0 = el mensaje no tiene línea (p. ej. un símbolo sin definir al enlazar) */
    int aviso;               /* 1 = warning, 0 = error */
    char msg[LAB_MSG_MAX];   /* sin «<string>:N: error:» delante */
} lab_error_t;

/* Compila 'fuente' en memoria. limites = 1: comprobación de límites (-b). Rellena errs (hasta max) y
 * *n. Devuelve 0 si se puede ejecutar (aunque haya avisos), -1 si hubo errores. *ms = tiempo. */
int lab_compilar(const char *fuente, int limites, lab_error_t *errs, int max, int *n, int *ms);

/* Ejecuta lo último compilado en un proceso hijo. Su salida y errores (y los de bcheck) se leen con
 * lab_leer (sin bloqueo) y lab_enviar escribe en su entrada estándar. Devuelve el pid o -1. */
int lab_lanzar(void);
int lab_leer(char *b, int n);              /* bytes leídos; 0 = nada por ahora */
void lab_enviar(const char *s, int n);
void lab_cerrar(void);                     /* suelta las tuberías */

/* 0 = sigue; 1 = terminó (*codigo); 2 = lo mató la señal *codigo. */
int lab_estado(int pid, int *codigo);
void lab_detener(int pid);

#endif
