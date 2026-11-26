// SPDX-License-Identifier: GPL-3.0-only
/* unidades_red.h - las unidades de red de la cuenta: ~/.config/unidades-red (D485).
 *
 * Una línea por unidad, campos separados por tabuladores:
 *   nombre  url  usuario  auto(0/1)  cacert(o -)  inseguro(0/1)
 * Sin contraseñas: esas van al llavero (chaveiro.h), con el servicio "webdav:<nombre>". El fichero es 0600. */
#ifndef SAGUI_UNIDADES_RED_H
#define SAGUI_UNIDADES_RED_H
#include "sagui.h"

#define UNIDADES_RED_ARQ  "/home/.config/unidades-red"
#define UNIDADES_RED_MAX  PONTE_MAX
#define UR_URL_MAX        200
#define UR_USUARIO_MAX    64

typedef struct {
    char nome[PONTE_NOME_MAX];
    char url[UR_URL_MAX];
    char usuario[UR_USUARIO_MAX];
    bool automatica;                /* conectar al iniciar la sesión */
    char cacert[128];               /* PEM con la autoridad (servidor propio); "" = las de siempre */
    bool inseguro;                  /* no comprobar el certificado (desaconsejado) */
} unidade_red_t;

int  unidades_red_ler(unidade_red_t *v, int max);          /* cuántas */
int  unidades_red_gravar(const unidade_red_t *v, int n);   /* 0 o error */
int  unidade_red_achar(const unidade_red_t *v, int n, const char *nome);   /* índice o -1 */
bool unidade_red_nome_valido(const char *nome);
/* La URL sin usuario ni contraseña, para enseñarla (y para el rótulo del kernel). */
void unidade_red_rotulo(const char *url, char *saida, int max);
/* El estado en el kernel de la unidad "nome" (PONTE_EST_*), o -1 si no está montada. */
int  unidade_red_estado(const char *nome, ponte_info_t *info);
const char *unidade_red_estado_texto(int estado);   /* «conectada», «desconectada», «conectando», «sin montar» */

#endif
