// SPDX-License-Identifier: GPL-2.0-or-later
/* plataforma.h - lo que lite-monkey pide al sistema operativo.
 *
 * Dos implementaciones: plataforma_posix.c (Linux/WSL, con la libc y los
 * sockets BSD) y plataforma_sagui.c (SaguiOS, con la libsagui). El núcleo
 * solo ve estas funciones. Los descriptores son enteros; < 0 = error. */
#ifndef LMK_PLATAFORMA_H
#define LMK_PLATAFORMA_H
#include "nucleo/base.h"

/* Reloj y azar */
uint64_t plat_ms(void);                 /* milisegundos monótonos */
uint64_t plat_epoca_ms(void);           /* ms desde 1970 (UTC) */
void     plat_azar(void *buf, uint32_t n);
void     plat_dormir(uint32_t ms);
void     plat_escribir_log(const char *texto);   /* una línea a la salida de errores */

/* Archivos. Todos los accesos van con desplazamiento explícito. */
#define PLAT_LEER     1
#define PLAT_ESCRIBIR 2
#define PLAT_CREAR    4
#define PLAT_TRUNCAR  8
int  plat_abrir(const char *ruta, int modo);
int  plat_leer_en(int fd, uint32_t off, void *buf, uint32_t n);          /* bytes leídos */
bool plat_escribir_en(int fd, uint32_t off, const void *buf, uint32_t n);
bool plat_sincronizar(int fd);
void plat_cerrar(int fd);
bool plat_tamano(int fd, uint32_t *n);
bool plat_renombrar(const char *de, const char *a);    /* reemplaza el destino */
bool plat_borrar(const char *ruta);
bool plat_crear_carpeta(const char *ruta);              /* true también si ya existe */
bool plat_borrar_carpeta(const char *ruta);
bool plat_existe_carpeta(const char *ruta);
/* Lista una carpeta: f(ctx, nombre, es_carpeta) por cada entrada (sin . ni ..). */
typedef void (*plat_listar_fn)(void *ctx, const char *nombre, bool carpeta);
bool plat_listar(const char *ruta, plat_listar_fn f, void *ctx);

/* Red (TCP) */
int  plat_escuchar(const char *direccion, int puerto);   /* direccion NULL = todas */
int  plat_aceptar(int fd, char *quien, uint32_t max);     /* -1 = ninguna esperando */
int  plat_recibir(int fd, void *buf, uint32_t n);         /* >0 bytes, 0 = cerrada, -1 = nada aún, -2 = error */
int  plat_enviar(int fd, const void *buf, uint32_t n);    /* bytes enviados (0 = luego), -2 = error */
void plat_cerrar_socket(int fd);
/* Espera hasta que algún descriptor esté listo o pase el plazo. listo[i]:
 * bit 1 = leer, bit 2 = escribir. quiere_escribir[i] pide vigilar la escritura. */
int  plat_esperar(const int *fds, const uint8_t *quiere_escribir, uint8_t *listo, int n, int plazo_ms);

/* Señales de parada (Ctrl+C): true cuando hay que cerrar. */
bool plat_parar(void);

/* Datos por defecto de la plataforma */
const char *plat_carpeta_datos(void);
uint32_t    plat_memoria_mb(void);

int lmk_principal(int argc, char **argv);   /* principal.c */

#endif
