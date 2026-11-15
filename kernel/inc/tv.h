// SPDX-License-Identifier: GPL-2.0-or-later
/* tv.h - terminal virtual (pseudoterminal): un terminal para el Terminal en ventana (D036). */
#ifndef SAGUI_TV_H
#define SAGUI_TV_H
#include "tipos.h"
#include "pipe.h"

typedef struct tv {
    pipe_t  *entrada;        /* del maestro al esclavo: líneas listas o teclas en bruto */
    pipe_t  *saida;          /* del esclavo (y del eco) al maestro: lo que va a la pantalla */
    int      modo;           /* TERM_LINHA o TERM_BRUTO */
    char     linha[256];     /* línea que se edita en modo línea */
    uint32_t tam;
    bool     fim_pendente;   /* Ctrl+D en una línea vacía: la siguiente lectura devuelve 0 */
    int      esc;            /* 1 = el byte anterior era ESC (tecla especial en modo línea) */
    int      dono_bruto;     /* quién puso el modo en bruto (el shell editando la línea) */
    bool     redesenhar;     /* otro proceso escribió: el dueño recibe ESC 'R' */
    int      pendente;       /* segundo byte del ESC 'R' para quien lee de 1 en 1 (-1 = ninguno) */
    bool     coluna0;        /* el último byte enviado a la pantalla fue '\n' */
    int      primeiro_plano; /* grupo que termina Ctrl+C (0 = ninguno; D037) */
    uint32_t interrupcoes;   /* cuántos Ctrl+C han llegado */
    bool     cancelar_leitura; /* Ctrl+C sin primer plano: la lectura pendiente vuelve vacía */
    bool     mestre_aberto, escravo_aberto;
} tv_t;

tv_t   *tv_criar(void);
int32_t tv_mestre_ler(tv_t *t, char *destino, uint32_t n);         /* la pantalla lee la salida */
int32_t tv_mestre_escrever(tv_t *t, const char *origem, uint32_t n); /* teclas pulsadas */
int32_t tv_mestre_disponivel(tv_t *t);
int32_t tv_ler(tv_t *t, char *destino, uint32_t n);                 /* el programa lee */
int32_t tv_escravo_disponivel(tv_t *t);                             /* D317: cuánto lee sin bloquearse */
int32_t tv_escrever(tv_t *t, const char *origem, uint32_t n);       /* el programa escribe */
void    tv_modo(tv_t *t, int modo);
void    tv_limpar(tv_t *t);
void    tv_fechar(tv_t *t, bool mestre);

#endif
