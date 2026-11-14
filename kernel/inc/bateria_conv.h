// SPDX-License-Identifier: GPL-2.0-or-later
/* bateria_conv.h - de los números de _BIF/_BIX/_BST a porcentaje y tiempo (D192).
 *
 * Solo cálculo, sin hardware ni uACPI: el driver (bateria.c) lee los paquetes
 * con el intérprete y deja aquí los números tal cual los da la BIOS; esto
 * suma las baterías, decide el porcentaje y el tiempo restante y descarta lo
 * que no tiene sentido. Las pruebas compilan el mismo archivo en el anfitrión
 * (tests/h158_bateria.py, -DSAGUI_HOST) con casos raros de BIOS reales. */
#ifndef SAGUI_BATERIA_CONV_H
#define SAGUI_BATERIA_CONV_H
#ifdef SAGUI_HOST
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#else
#include "tipos.h"
#endif
#include "abi.h"

#define BATERIA_MAX 2   /* casi todos los portátiles tienen una; algunos, dos */

/* Lo que devuelve la BIOS de una batería, sin tocar. "Desconocido" en ACPI es
 * 0xFFFFFFFF (BATERIA_DESCONHECIDO); algunas BIOS ponen 0 o valores absurdos. */
typedef struct {
    bool     existe;          /* hay un dispositivo PNP0C0A */
    bool     presente;        /* _STA bit 4: la batería está puesta */
    bool     info_ok;         /* se leyó _BIX o _BIF */
    bool     estado_ok;       /* se leyó _BST */
    uint32_t unidade;         /* 0 = mW/mWh; 1 = mA/mAh */
    uint32_t projeto, total, projeto_mv, aviso, baixo, ciclos;
    uint32_t estado, taxa, restante, tensao_mv;
} bateria_bruta_t;

/* Rellena en "saida" los campos de batería de info_bateria_t (presente,
 * estado, porcentagem, minutos, unidade_ma, projeto, total, restante, taxa,
 * tensao_mv, projeto_mv, aviso, baixo, ciclos) a partir de n baterías. Los
 * demás campos (uacpi, baterias, adaptador, textos, contadores) no los toca.
 * "adaptador" es el del cargador (0 no hay, 1 desenchufado, 2 enchufado):
 * sirve para no dar un tiempo de descarga con el cargador puesto. */
void bateria_calcular(const bateria_bruta_t *b, int n, uint32_t adaptador, info_bateria_t *saida);

/* El texto del tiempo: "2 h 05 min", "45 min" o "" si no se sabe. */
void bateria_texto_tempo(int minutos, char *t, int max);

#endif
