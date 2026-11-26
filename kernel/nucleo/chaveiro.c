// SPDX-License-Identifier: GPL-3.0-only
/* chaveiro.c - la clave del llavero de cada cuenta, solo en la memoria del kernel (D485).
 *
 * El llavero de una cuenta (~/.config/llavero, userland/libtls/chaveiro.c) se cifra con una clave que sale de
 * la contraseña de la cuenta. Esa clave no se guarda en ningún disco: el gestor de servicios la calcula al
 * comprobar la contraseña al entrar en la sesión y la deja aquí, por uid; los programas de la cuenta la toman
 * para abrir su llavero. Una cuenta sin contraseña puede poner la de su llavero (contraseña propia).
 *   - poner: root (el gestor de servicios) para cualquier uid, o la propia cuenta para el suyo;
 *   - tomar: solo la propia cuenta (root no lee las de las demás por aquí);
 *   - olvidar: root o la propia cuenta; al cerrar o cambiar de sesión se olvidan las de las demás cuentas.
 * Nunca se escribe en el registro ni en la serie. */
#include "abi.h"
#include "console.h"
#include "processo.h"
#include "sistema.h"
#include "texto.h"

#define CHAVEIRO_CONTAS 8

static struct {
    bool     usado;
    uint32_t uid;
    uint8_t  chave[CHAVEIRO_TAM];
} tabela[CHAVEIRO_CONTAS];

static void apagar(int i)
{
    volatile uint8_t *p = tabela[i].chave;
    for (int k = 0; k < CHAVEIRO_TAM; k++) p[k] = 0;
    tabela[i].usado = false;
    tabela[i].uid = 0;
}

static int achar(uint32_t uid)
{
    for (int i = 0; i < CHAVEIRO_CONTAS; i++) if (tabela[i].usado && tabela[i].uid == uid) return i;
    return -1;
}

/* Al cambiar de sesión: solo se queda la de "uid" (0xFFFFFFFF = ninguna). */
void chaveiro_esquecer_menos(uint32_t uid)
{
    for (int i = 0; i < CHAVEIRO_CONTAS; i++) if (tabela[i].usado && tabela[i].uid != uid) apagar(i);
}

int32_t sis_chaveiro(uint32_t op, uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    (void)c; (void)d;
    uint32_t eu = processo_recursos()->uid;
    switch (op) {
    case CHAVEIRO_POR: {
        if (eu != UID_ROOT && eu != a) return ERRO_ACESSO;
        if (!usuario_ok((void *)b, CHAVEIRO_TAM, false)) return ERRO_ENDERECO;
        int i = achar(a);
        for (int k = 0; i < 0 && k < CHAVEIRO_CONTAS; k++) if (!tabela[k].usado) i = k;
        if (i < 0) return ERRO_LIMITE;
        memcpy(tabela[i].chave, (const void *)b, CHAVEIRO_TAM);
        tabela[i].uid = a;
        tabela[i].usado = true;
        return 0;
    }
    case CHAVEIRO_TOMAR: {
        if (!usuario_ok((void *)a, CHAVEIRO_TAM, true)) return ERRO_ENDERECO;
        int i = achar(eu);
        if (i < 0) return ERRO_NAO_EXISTE;
        memcpy((void *)a, tabela[i].chave, CHAVEIRO_TAM);
        return 0;
    }
    case CHAVEIRO_ESQUECER: {
        if (eu != UID_ROOT && eu != a) return ERRO_ACESSO;
        int i = achar(a);
        if (i >= 0) apagar(i);
        return 0;
    }
    default:
        return ERRO_INVALIDO;
    }
}
