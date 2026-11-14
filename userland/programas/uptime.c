// SPDX-License-Identifier: GPL-2.0-or-later
/* uptime - hora actual, cuánto lleva encendido el sistema y cuántos procesos hay (D097). */
#include "sagui.h"

static const char *AYUDA = "uso: uptime [-p]\n  -p  solo el tiempo encendido, en palabras («hasta 2 horas, 5 minutos»)\n";

int principal(int argc, char **argv)
{
    bool bonito = false;
    opcoes_t o = { 0 };
    int r;
    while ((r = obter_opcao(argc, argv, "p", &o)) >= 0) {
        if (r == 'p') bonito = true;
        else if (r == OPC_AJUDA) { escrever_texto(SAIDA, AYUDA); return 0; }
        else return opcao_invalida("uptime", r, &o);
    }
    info_versao_t v;
    if (info_sistema(INFO_VERSAO, &v, sizeof v) < 0) { imprimir_em(ERROS, "uptime: no se puede leer el reloj\n"); return 1; }
    unsigned s = v.segundos_ligado, dias = s / 86400, h = s / 3600 % 24, m = s / 60 % 60;
    char tiempo[80];
    if (dias) formatar_texto(tiempo, sizeof tiempo, "%u días, %u:%02u", dias, h, m);
    else if (h) formatar_texto(tiempo, sizeof tiempo, "%u:%02u", h, m);
    else formatar_texto(tiempo, sizeof tiempo, "%u min", m);
    if (bonito) {
        if (dias) imprimir("hasta %u días, %u horas, %u minutos\n", dias, h, m);
        else if (h) imprimir("hasta %u horas, %u minutos\n", h, m);
        else imprimir("hasta %u minutos\n", m);
        return 0;
    }
    fuso_data_t l;
    char hora[16];
    agora_local(&l);
    formatar_data(hora, sizeof hora, "%H:%M:%S", &l);
    static info_processo_t p[64];
    int np = info_sistema(INFO_PROCESSOS, p, 64);
    imprimir(" %s hasta %s, %d procesos\n", hora, tiempo, np < 0 ? 0 : np);
    return 0;
}
