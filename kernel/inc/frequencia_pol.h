// SPDX-License-Identifier: GPL-2.0-or-later
/* frequencia_pol.h - la decisión del driver de frecuencia (D106, mejora 061).
 *
 * Solo lógica, sin hardware: el driver (kernel/dispositivos/frequencia.c)
 * lee la temperatura y el uso de la CPU y pregunta aquí qué estado pedir; las
 * pruebas compilan el mismo archivo en el anfitrión (tests/h78_frecuencia.py,
 * -DSAGUI_HOST) y recorren las políticas y el límite de temperatura.
 *
 * Los estados se numeran como los P-states: 0 = el más rápido, qtd - 1 = el
 * más lento. Temperaturas en milésimas de grado. */
#ifndef SAGUI_FREQUENCIA_POL_H
#define SAGUI_FREQUENCIA_POL_H
#ifdef SAGUI_HOST
#include <stdint.h>
#include <stdbool.h>
#else
#include "tipos.h"
#endif

#define FPOL_RENDIMENTO 0   /* el más rápido permitido */
#define FPOL_ECONOMIA   1   /* el más lento */
#define FPOL_DEMANDA    2   /* sube con carga, baja en reposo */

#define FPOL_LIMITE_MAX_MC   79000   /* límite de seguridad (mantenedor, 2026-11-08): no se puede subir */
#define FPOL_LIMITE_MIN_MC   40000   /* por debajo no tiene sentido (la CPU en reposo ya está ahí) */
#define FPOL_HISTERESE_MC     7000   /* vuelve a subir por debajo de límite - 7 °C */
#define FPOL_SOBE_USO           60   /* a demanda: con un 60 % de uso o más, al máximo */
#define FPOL_DESCE_USO          25   /* ... y por debajo del 25 % ... */
#define FPOL_DESCE_PERIODOS      4   /* ... durante 4 periodos seguidos, un estado más lento */

/* Varios núcleos (D175). Cada CPU lógica tiene su propio registro de
 * P-state, pero el estado efectivo es de un «dominio» que puede ser más
 * grande: los dos hilos de un núcleo con SMT, el par de núcleos de un módulo
 * (AMD 15h) o el paquete entero (muchos Intel de Nehalem a Skylake). El
 * hardware aplica entonces el más rápido de los pedidos del dominio. La
 * política decide por dominio (la carga del dominio = la de su CPU más
 * cargada) y pide el mismo estado a todas sus CPU. */
#define FPOL_MAX_NUCLEOS     8
#define FPOL_POR_NUCLEO      0   /* de serie: un dominio por núcleo físico */
#define FPOL_PAQUETE         1   /* todas las CPU, un solo dominio */

typedef struct {
    /* Configuración */
    int      politica;      /* FPOL_* */
    int      limite_mc;     /* <= FPOL_LIMITE_MAX_MC */
    bool     forcado;       /* subir sin lectura de temperatura (el usuario lo pidió) */
    int      coordenacao;   /* FPOL_POR_NUCLEO o FPOL_PAQUETE (D175) */
    /* Lo que da la CPU */
    int      qtd;           /* número de estados (>= 1) */
    int      rapido;        /* el más rápido que la plataforma permite ahora (PStateCurLim) */
    int      bios;          /* el que dejó la BIOS */
    /* Estado */
    bool     limitada;      /* limitada por temperatura */
    int      alvo_pol;      /* lo que pide la política, antes de las reglas de seguridad */
    int      alvo;          /* la última decisión */
    int      calmos;        /* periodos seguidos con poco uso (a demanda) */
    uint32_t vezes_limitada;
} fpol_t;

void fpol_iniciar(fpol_t *p, int qtd, int bios);
/* Cambia el límite: false (sin cambiar nada) si es mayor que 79 °C o menor
 * que 40 °C. */
bool fpol_mudar_limite(fpol_t *p, int limite_mc);
int  fpol_retorno_mc(const fpol_t *p);   /* límite - 7 °C */
/* Un periodo: con la temperatura (temp_ok = false si no se pudo leer o es
 * absurda) y el uso de la CPU en % desde el periodo anterior, devuelve el
 * estado que hay que pedir. Nunca más rápido que p->rapido; con la CPU
 * limitada, siempre el más lento; sin temperatura (y sin forzar), nunca más
 * rápido que el de la BIOS. */
int  fpol_decidir(fpol_t *p, bool temp_ok, int temp_mc, unsigned uso_pct);
/* Una temperatura que se puede creer (ni un registro vacío ni 0xFFFFFFFF). */
bool fpol_temp_valida(int temp_mc);

/* Lo que la política guarda de cada CPU (D175). */
typedef struct {
    int bios;       /* el estado en que la BIOS dejó esta CPU (-1 = aún sin leer: no se decide) */
    int alvo_pol;   /* lo que pide la política, antes de las reglas de seguridad */
    int alvo;       /* la última decisión (-1 = ninguna) */
    int calmos;     /* periodos seguidos con poco uso (a demanda) */
} fpol_nucleo_t;

void fpol_nucleo_iniciar(fpol_nucleo_t *n, int bios);
/* Un periodo para n CPU (como mucho FPOL_MAX_NUCLEOS). dominio[i] es el
 * núcleo físico de la CPU i (NULL = cada CPU es su núcleo); con
 * p->coordenacao == FPOL_PAQUETE todas son un solo dominio. uso[i] es el % de
 * la CPU i; uso == NULL quiere decir «fuera de periodo» (una llamada al
 * sistema): se recalcula sin subir por carga ni contar periodos tranquilos.
 * Escribe en alvo[i] el estado de cada CPU (-1 si su BIOS aún no se leyó).
 * Las reglas de seguridad son las de fpol_decidir y las mismas para todas: el
 * límite de temperatura es uno (el del paquete) y, limitada, todas al más
 * lento; sin temperatura, ningún dominio pasa del estado más lento que la
 * BIOS dejó en sus CPU. Todas las CPU de un dominio reciben el mismo estado.
 * p->alvo queda con el de la CPU 0. */
void fpol_decidir_nucleos(fpol_t *p, fpol_nucleo_t *nuc, const int *dominio, int n, bool temp_ok, int temp_mc,
                          const unsigned *uso, int *alvo);

#endif
