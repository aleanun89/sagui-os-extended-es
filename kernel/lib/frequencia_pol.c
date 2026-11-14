// SPDX-License-Identifier: GPL-2.0-or-later
/* frequencia_pol.c - políticas y límite de temperatura del driver de
 * frecuencia (D106, mejora 061).
 *
 * Sin acceso al hardware, para probarlo en el anfitrión (tests/h78_frecuencia.py
 * lo compila con -DSAGUI_HOST). Las reglas de seguridad van después de la
 * política y siempre ganan:
 *   1. con la CPU a 79 °C (o al límite que haya puesto el usuario, que solo
 *      puede ser menor) se pide el estado más lento, y no se vuelve a subir
 *      hasta que baje 7 °C por debajo del límite (72 °C con el de serie);
 *   2. sin una lectura válida de temperatura no se pasa del estado que dejó
 *      la BIOS (salvo que el usuario lo fuerce);
 *   3. nunca más rápido que lo que permite la plataforma (PStateCurLim). */
#include "frequencia_pol.h"

void fpol_iniciar(fpol_t *p, int qtd, int bios)
{
    if (qtd < 1) qtd = 1;
    if (bios < 0 || bios >= qtd) bios = qtd - 1;
    p->politica = FPOL_RENDIMENTO;
    p->limite_mc = FPOL_LIMITE_MAX_MC;
    p->forcado = false;
    p->coordenacao = FPOL_POR_NUCLEO;
    p->qtd = qtd;
    p->rapido = 0;
    p->bios = bios;
    p->limitada = false;
    p->alvo_pol = bios;
    p->alvo = bios;
    p->calmos = 0;
    p->vezes_limitada = 0;
}

bool fpol_mudar_limite(fpol_t *p, int limite_mc)
{
    if (limite_mc > FPOL_LIMITE_MAX_MC || limite_mc < FPOL_LIMITE_MIN_MC) return false;
    p->limite_mc = limite_mc;
    return true;
}

int fpol_retorno_mc(const fpol_t *p)
{
    return p->limite_mc - FPOL_HISTERESE_MC;
}

bool fpol_temp_valida(int mc)
{
    /* 0 °C o menos: registro vacío o sensor sin iniciar; 125 °C o más: lectura
     * imposible (un 0xFFFFFFFF del PCI da 255,875 °C en la escala de AMD). */
    return mc > 0 && mc < 125000;
}

/* El límite de temperatura con histéresis. Sin lectura, el estado no cambia:
 * si estaba limitada, sigue limitada. Devuelve si la lectura vale. */
static bool passo_temperatura(fpol_t *p, bool temp_ok, int temp_mc)
{
    if (temp_ok && !fpol_temp_valida(temp_mc)) temp_ok = false;
    if (temp_ok) {
        if (!p->limitada && temp_mc >= p->limite_mc) {
            p->limitada = true;
            p->vezes_limitada++;
        } else if (p->limitada && temp_mc < fpol_retorno_mc(p)) {
            p->limitada = false;
        }
    }
    return temp_ok;
}

/* La política sola, sobre el estado de un dominio. */
static void passo_politica(const fpol_t *p, int rapido, int lento, int *alvo_pol, int *calmos, unsigned uso)
{
    switch (p->politica) {
    case FPOL_ECONOMIA:
        *alvo_pol = lento;
        *calmos = 0;
        break;
    case FPOL_DEMANDA:
        if (uso >= FPOL_SOBE_USO) {
            *alvo_pol = rapido;
            *calmos = 0;
        } else if (uso < FPOL_DESCE_USO) {
            if (++*calmos >= FPOL_DESCE_PERIODOS) {
                if (*alvo_pol < lento) (*alvo_pol)++;
                *calmos = 0;
            }
        } else {
            *calmos = 0;
        }
        break;
    default:
        *alvo_pol = rapido;
        *calmos = 0;
        break;
    }
    if (*alvo_pol < rapido) *alvo_pol = rapido;
    if (*alvo_pol > lento) *alvo_pol = lento;
}

/* Las reglas de seguridad, que siempre ganan a la política. */
static int passo_seguranca(const fpol_t *p, int alvo_pol, int bios, bool temp_ok, int rapido, int lento)
{
    int a = alvo_pol;
    if (p->limitada) a = lento;
    else if (!temp_ok && !p->forcado && a < bios) a = bios;
    if (a < rapido) a = rapido;
    if (a > lento) a = lento;
    return a;
}

int fpol_decidir(fpol_t *p, bool temp_ok, int temp_mc, unsigned uso)
{
    int lento = p->qtd - 1;
    int rapido = p->rapido < 0 ? 0 : p->rapido > lento ? lento : p->rapido;
    temp_ok = passo_temperatura(p, temp_ok, temp_mc);
    passo_politica(p, rapido, lento, &p->alvo_pol, &p->calmos, uso);
    p->alvo = passo_seguranca(p, p->alvo_pol, p->bios, temp_ok, rapido, lento);
    return p->alvo;
}

void fpol_nucleo_iniciar(fpol_nucleo_t *n, int bios)
{
    n->bios = bios;
    n->alvo_pol = bios;
    n->alvo = -1;
    n->calmos = 0;
}

void fpol_decidir_nucleos(fpol_t *p, fpol_nucleo_t *nuc, const int *dominio, int n, bool temp_ok, int temp_mc,
                          const unsigned *uso, int *alvo)
{
    int lento = p->qtd - 1;
    int rapido = p->rapido < 0 ? 0 : p->rapido > lento ? lento : p->rapido;
    if (n > FPOL_MAX_NUCLEOS) n = FPOL_MAX_NUCLEOS;
    temp_ok = passo_temperatura(p, temp_ok, temp_mc);
    for (int i = 0; i < n; i++) alvo[i] = -1;
    for (int i = 0; i < n; i++) {
        /* El dominio de la CPU i: lo decide su primera CPU con la BIOS leída
         * (el «líder»); las demás copian. */
        int dom_i = p->coordenacao == FPOL_PAQUETE ? 0 : dominio ? dominio[i] : i;
        if (nuc[i].bios < 0) continue;
        int lider = -1;
        for (int k = 0; k < i && lider < 0; k++) {
            int dom_k = p->coordenacao == FPOL_PAQUETE ? 0 : dominio ? dominio[k] : k;
            if (dom_k == dom_i && nuc[k].bios >= 0) lider = k;
        }
        if (lider >= 0) continue;   /* ya decidido con su líder */
        unsigned carga = 0;
        int bios = 0;
        for (int k = i; k < n; k++) {
            int dom_k = p->coordenacao == FPOL_PAQUETE ? 0 : dominio ? dominio[k] : k;
            if (dom_k != dom_i || nuc[k].bios < 0) continue;
            if (uso && uso[k] > carga) carga = uso[k];
            if (nuc[k].bios > bios) bios = nuc[k].bios;   /* sin temperatura: el más lento de la BIOS */
        }
        /* Una CPU recién leída empieza donde la dejó la BIOS. */
        if (nuc[i].alvo_pol < 0) nuc[i].alvo_pol = bios;
        int calmos = nuc[i].calmos;
        passo_politica(p, rapido, lento, &nuc[i].alvo_pol, &nuc[i].calmos, uso ? carga : FPOL_DESCE_USO);
        if (!uso) nuc[i].calmos = calmos;
        int a = passo_seguranca(p, nuc[i].alvo_pol, bios, temp_ok, rapido, lento);
        for (int k = i; k < n; k++) {
            int dom_k = p->coordenacao == FPOL_PAQUETE ? 0 : dominio ? dominio[k] : k;
            if (dom_k != dom_i || nuc[k].bios < 0) continue;
            nuc[k].alvo_pol = nuc[i].alvo_pol;
            nuc[k].calmos = nuc[i].calmos;
            nuc[k].alvo = a;
            alvo[k] = a;
        }
    }
    if (n > 0) {
        p->alvo = alvo[0];
        if (nuc[0].bios >= 0) {
            p->alvo_pol = nuc[0].alvo_pol;
            p->calmos = nuc[0].calmos;
        }
    }
}
