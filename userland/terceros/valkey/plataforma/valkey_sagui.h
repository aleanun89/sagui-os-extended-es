/* SPDX-License-Identifier: GPL-3.0-only */
/* valkey_sagui.h - lo que Valkey espera de las cabeceras de glibc y la libc de SaguiOS reparte de otro modo
 * (D477). Se incluye con -include en todos los .c de Valkey. */
#ifndef VALKEY_SAGUI_H
#define VALKEY_SAGUI_H
#include <sys/types.h>
#include <sys/time.h>       /* struct timeval (io_threads.c lo da por incluido por glibc) */
#include <sys/select.h>
#include <sys/resource.h>
/* SaguiOS no tiene almacenamiento local de hilo (TLS): __thread pasa a ser una variable global. Solo lo usan
 * io_threads.c (id de hilo) y networking.c (búfer compartido de consultas), que con io-threads 1 (por defecto
 * y único valor admitido, ver docs/valkey.md) los usa un solo hilo. */
#define __thread
#define _Thread_local   /* zmalloc.c: el índice de hilo de las estadísticas; con hilos de bio comparten hueco (solo INFO/maxmemory, no datos) */
#endif
