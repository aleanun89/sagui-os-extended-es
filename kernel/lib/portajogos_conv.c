// SPDX-License-Identifier: GPL-2.0-or-later
/* portajogos_conv.c - las cuentas del puerto de juegos (D307). Ver portajogos_conv.h. */
#include "portajogos_conv.h"

uint32_t pj_tempo_esperado(uint32_t ohmios)
{
    /* 24,2 us + 0,011 us por ohmio = 242 + 0,11 * R décimas */
    return 242u + ohmios * 11u / 100u;
}

uint32_t pj_decimas_us(uint32_t ciclos, uint32_t tsc_khz)
{
    uint32_t mhz = tsc_khz / 1000u;
    if (!mhz) return PJ_SEM_EIXO;
    /* ciclos * 10 / MHz; se divide antes si el producto no cabe en 32 bits */
    if (ciclos > 0x7FFFFFFFu / 10u) return ciclos / mhz * 10u;
    return ciclos * 10u / mhz;
}

int16_t pj_normalizar(uint32_t t, const pj_cal_t *c)
{
    int32_t r;
    if (t == PJ_SEM_EIXO || !pj_cal_valida(c)) return 0;
    if (t <= c->min) return -32767;
    if (t >= c->max) return 32767;
    if (t < c->centro) r = -(int32_t)((c->centro - t) * 32767u / (uint32_t)(c->centro - c->min));
    else r = (int32_t)((t - c->centro) * 32767u / (uint32_t)(c->max - c->centro));
    return (int16_t)r;
}

void pj_cal_inicial(uint32_t centro, pj_cal_t *c)
{
    if (centro < 16) centro = 16;
    if (centro > 30000) centro = 30000;
    /* Un potenciómetro lineal en el centro está a media resistencia: el mínimo es el
     * tiempo fijo (~24 us, una octava parte del centro de un joystick de 100 kohmios) y el
     * máximo, el doble del centro menos ese mínimo. */
    c->centro = (uint16_t)centro;
    c->min = (uint16_t)(centro / 8u);
    c->max = (uint16_t)(centro * 2u - c->min);
}

void pj_cal_ampliar(uint32_t t, pj_cal_t *c)
{
    if (t == PJ_SEM_EIXO || t > 0xFFFE) return;
    if (t < c->min) c->min = (uint16_t)t;
    if (t > c->max) c->max = (uint16_t)t;
}

bool pj_cal_valida(const pj_cal_t *c)
{
    return c->min < c->centro && c->centro < c->max && c->max != PJ_SEM_EIXO;
}
