// SPDX-License-Identifier: GPL-2.0-or-later
/* kconf.h - opciones del kernel que se cambian desde SaguiOS y valen en el siguiente arranque (D353).
 *
 * Las opciones viven en la cabecera de arranque (LBA 1): los bits que pone mkimagem.py son los
 * «valores de la imagen» y no se tocan nunca; lo que guarda «kconfig» va en una zona aparte de la
 * misma cabecera (bytes 192-255, KCONF_*), que el kernel aplica encima al arrancar. Así:
 *   - lo que se lee antes de montar nada (PAE, NX, DMA, caché) se puede decidir desde el sistema;
 *   - volver a los valores de la imagen es borrar esa zona (kconfig --por-defecto);
 *   - el modo rescate (Ctrl al arrancar, D255) no la aplica: si lo guardado impide arrancar, se
 *     arranca con Ctrl y se borra desde el shell de root.
 * Ver docs/decisoes.md, D353. */
#ifndef KCONF_H
#define KCONF_H

#include "tipos.h"

/* Zona guardada en la cabecera (desplazamientos dentro del sector LBA 1). El kernel compara los
 * 256 primeros bytes para reconocer el disco de arranque: quien graba la zona actualiza también
 * la copia en memoria (cabecalho_boot). */
#define KCONF_ZONA      192
#define KCONF_ASSINATURA 192           /* u32 'SGKC' */
#define KCONF_PRESENCA  196            /* u32: bit i = la opción del hueco i está guardada */
#define KCONF_VALORES   200            /* u16 valores[KCONF_HUECOS] */
#define KCONF_HUECOS    24
#define KCONF_SOMA      248            /* u32: suma de los bytes 192-247 */
#define KCONF_FIM       256
#define KCONF_ASSINATURA_VAL 0x434B4753u   /* 'SGKC' en little-endian */

/* Huecos fijos de cada opción (nunca se reutilizan: un número es una opción para siempre). */
enum {
    KC_PAE = 0, KC_NX, KC_CACHE_DISCO, KC_CACHE_ALTA, KC_ESCRITURA_DIFERIDA, KC_DMA,
    KC_CACHE_MAX_MB, KC_SUCIOS_MAX_MB, KC_VACIADO_S, KC_FRECUENCIA, KC_GOBERNADOR,
    KC_SWAP, KC_SWAP_MB,   /* D352: 11 y 12 */
    KC_CAPA_RAM_PCT, KC_RAIZ_MODO,   /* v8.5.4.2 (D379): 13 y 14 */
    KC_SERIE_TERMINAL,               /* v8.5.4.10 (D385): 15 */
    KC_VOLCADO_GPU,                  /* v8.5.4.10r2 (D387): 16 */
    KC_VUELTA_GPU,                   /* v8.5.4.10r2 (D391): 17 */
    KC_AVX,                          /* v8.5.5 (D381): 18 */
    KC_GPU_ENERGIA,                  /* v8.5.5 (D399): 19 */
};

/* Al principio del arranque, con cabecalho_boot ya copiada y antes de la paginación: calcula los
 * valores efectivos de este arranque. No escribe en el registro (aún no hay memoria). */
void kconf_iniciar(void);
/* Más tarde, con el registro en marcha: lo aplicado, lo ignorado y por qué. */
void kconf_registrar(void);

/* El byte de la cabecera "desl" con lo guardado aplicado encima (para los bytes de opciones:
 * CAB_MEM_OPC, CAB_DISCO_OPC, CAB_FREQ, CAB_TROCA_MB). Vale todo el arranque, aunque luego se guarde otra cosa. */
uint8_t kconf_cab(uint32_t desl);
/* El valor efectivo de una opción numérica o de lista en este arranque (0 = automático). */
uint32_t kconf_valor(int hueco);

/* D323: Configuración cambió bits de la imagen (INFO_CAB_OPC) en el sector "s": se olvida lo que
 * kconfig guardó para esos mismos bits (manda lo último que se pidió). Devuelve si cambió algo. */
bool kconf_esquecer_bits(uint8_t *s, uint32_t desl, uint32_t mascara);

int32_t kconf_info(uint32_t ptr, uint32_t n);    /* INFO_KCONF */
int32_t kconf_gravar(uint32_t pedido);           /* INFO_KCONF_GRAVAR (solo administrador) */

#endif
