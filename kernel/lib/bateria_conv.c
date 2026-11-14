// SPDX-License-Identifier: GPL-2.0-or-later
/* bateria_conv.c - porcentaje y tiempo restante de la batería (D192).
 *
 * Sin hardware: ver bateria_conv.h. Las BIOS reales dan de todo: capacidades
 * a 0 o a 0xFFFFFFFF («desconocido»), una capacidad restante algo mayor que
 * la de la última carga completa, un consumo de 0 o absurdo justo al
 * desenchufar, los bits de carga y descarga a la vez... Aquí se decide qué se
 * enseña y, ante la duda, se dice «desconocido» (-1) en vez de inventar. */
#include "bateria_conv.h"

#define MAX_RAZOAVEL 0x01000000u   /* 16 millones de mWh/mAh: por encima, la BIOS miente */
#define MINUTOS_MAX  (24 * 60)     /* más de un día de batería no es creíble */

static bool valido(uint32_t v) { return v != BATERIA_DESCONHECIDO && v < MAX_RAZOAVEL; }

void bateria_calcular(const bateria_bruta_t *b, int n, uint32_t adaptador, info_bateria_t *s)
{
    s->presente = 0;
    s->estado = 0;
    s->porcentagem = -1;
    s->minutos = -1;
    s->unidade_ma = 0;
    s->projeto = s->total = s->restante = s->taxa = BATERIA_DESCONHECIDO;
    s->tensao_mv = s->projeto_mv = s->aviso = s->baixo = s->ciclos = BATERIA_DESCONHECIDO;

    int unidade = -1;
    uint64_t soma_rest = 0, soma_total = 0, soma_proj = 0, soma_taxa = 0;
    bool tem_rest = false, tem_total = false, tem_proj = false, tem_taxa = false;
    uint32_t estado = 0;
    for (int i = 0; i < n; i++) {
        const bateria_bruta_t *x = &b[i];
        if (!x->existe || !x->presente) continue;
        s->presente = 1;
        /* Con dos baterías de unidades distintas (no debería pasar) solo
         * cuentan las de la primera unidad que se vea. */
        if (x->info_ok) {
            if (unidade < 0) unidade = (int)(x->unidade ? 1 : 0);
            else if ((int)(x->unidade ? 1 : 0) != unidade) continue;
        }
        if (x->info_ok) {
            uint32_t cheia = valido(x->total) && x->total ? x->total
                           : valido(x->projeto) && x->projeto ? x->projeto : 0;
            if (cheia) { soma_total += cheia; tem_total = true; }
            if (valido(x->projeto) && x->projeto) { soma_proj += x->projeto; tem_proj = true; }
            if (s->projeto_mv == BATERIA_DESCONHECIDO && valido(x->projeto_mv) && x->projeto_mv) s->projeto_mv = x->projeto_mv;
            if (s->aviso == BATERIA_DESCONHECIDO && valido(x->aviso)) s->aviso = x->aviso;
            if (s->baixo == BATERIA_DESCONHECIDO && valido(x->baixo)) s->baixo = x->baixo;
            if (s->ciclos == BATERIA_DESCONHECIDO && valido(x->ciclos)) s->ciclos = x->ciclos;
        }
        if (x->estado_ok) {
            estado |= x->estado & (BATERIA_DESCARREGANDO | BATERIA_CARREGANDO | BATERIA_CRITICA);
            if (valido(x->restante)) { soma_rest += x->restante; tem_rest = true; }
            if (valido(x->taxa) && x->taxa) { soma_taxa += x->taxa; tem_taxa = true; }
            if (s->tensao_mv == BATERIA_DESCONHECIDO && valido(x->tensao_mv) && x->tensao_mv) s->tensao_mv = x->tensao_mv;
        }
    }
    if (!s->presente) return;
    s->unidade_ma = unidade == 1 ? 1 : 0;

    /* Cargando y descargando a la vez: decide el cargador, si se sabe. */
    if ((estado & BATERIA_DESCARREGANDO) && (estado & BATERIA_CARREGANDO)) {
        if (adaptador == 2) estado &= ~(uint32_t)BATERIA_DESCARREGANDO;
        else estado &= ~(uint32_t)BATERIA_CARREGANDO;
    }
    s->estado = estado;
    if (tem_rest) s->restante = (uint32_t)soma_rest;
    if (tem_total) s->total = (uint32_t)soma_total;
    if (tem_proj) s->projeto = (uint32_t)soma_proj;
    if (tem_taxa) s->taxa = (uint32_t)soma_taxa;

    if (tem_rest && tem_total && soma_total) {
        uint64_t p = (soma_rest * 100 + soma_total / 2) / soma_total;
        s->porcentagem = p > 100 ? 100 : (int)p;   /* restante algo mayor que la carga completa: 100 % */
    }

    if (!tem_taxa || !tem_rest) return;
    uint64_t min = (uint64_t)-1;
    if ((estado & BATERIA_DESCARREGANDO) && adaptador != 2) {
        min = soma_rest * 60 / soma_taxa;
    } else if ((estado & BATERIA_CARREGANDO) && tem_total) {
        min = soma_total > soma_rest ? (soma_total - soma_rest) * 60 / soma_taxa : 0;
    }
    if (min <= MINUTOS_MAX) s->minutos = (int)min;
}

void bateria_texto_tempo(int minutos, char *t, int max)
{
    if (max <= 0) return;
    t[0] = 0;
    if (minutos < 0) return;
    char tmp[24];
    int k = 0, h = minutos / 60, m = minutos % 60;
    char dig[8];
    int nd;
    if (h > 0) {
        nd = 0;
        do { dig[nd++] = (char)('0' + h % 10); h /= 10; } while (h && nd < 7);
        while (nd) tmp[k++] = dig[--nd];
        tmp[k++] = ' '; tmp[k++] = 'h'; tmp[k++] = ' ';
        tmp[k++] = (char)('0' + m / 10);   /* "2 h 05 min" */
        tmp[k++] = (char)('0' + m % 10);
    } else {
        nd = 0;
        do { dig[nd++] = (char)('0' + m % 10); m /= 10; } while (m && nd < 7);
        while (nd) tmp[k++] = dig[--nd];
    }
    tmp[k++] = ' '; tmp[k++] = 'm'; tmp[k++] = 'i'; tmp[k++] = 'n';
    tmp[k] = 0;
    int i = 0;
    for (; i < max - 1 && tmp[i]; i++) t[i] = tmp[i];
    t[i] = 0;
}
