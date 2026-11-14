// SPDX-License-Identifier: GPL-2.0-or-later
/* saguimark.h - el motor de SaguiMark (D093, mejora 051).
 *
 * El mismo motor sirve al comando "saguimark" y a la pestaña Benchmark de infopc. Él
 * está en libsagui (userland/libsagui/saguimark.c), compilado sin
 * -mgeneral-regs-only porque usa la FPU x87 y, en las funciones del grupo SIMD,
 * SSE2 con __attribute__((target)) (como simd, D076): el código SSE2 sólo corre
 * después de que CPUID lo confirme. */
#ifndef SAGUI_SAGUIMARK_H
#define SAGUI_SAGUIMARK_H
#include "sagui.h"

/* Cambiar las pruebas cambia la versión: sólo se comparan puntuaciones de la misma. */
#define SM_VERSAO       2          /* 2: entra el grupo Gráficos 2D (D102); el total cambia */
#define SM_REFERENCIAS  "/lib/saguimark/referencias.txt"

enum { SM_ENTEROS, SM_FPU, SM_SIMD, SM_MEMORIA, SM_2D, SM_SISTEMA, SM_GRUPOS };
#define SM_GRUPOS_TOTAL 5          /* el grupo Sistema aparece, pero queda fuera del total */
#define SM_TODOS        0x3F       /* máscara de grupos */

enum { SM_ND, SM_OK, SM_ERRO };    /* estado de una prueba o grupo */
enum { SM_RAPIDO, SM_PRECISO };

#define SM_MAX_PROVAS 24

typedef struct {
    const char *nome;
    int         grupo;
    int         estado;
    unsigned    tempo_us;          /* mediana */
    unsigned    var_x10;           /* ± % x 10 entre las repeticiones */
    unsigned    pontos;            /* referencia / tiempo x 1000 */
    uint32_t    obtido, esperado;  /* verificación */
    const char *motivo;            /* n/d o ERROR: por qué */
    const char *unidade1, *unidade2;   /* pruebas 2D: lo que se cuenta por segundo (D102) */
    unsigned    taxa1, taxa2;      /* ... y cuánto (por segundo); 0 = nada */
} sm_prova_t;

typedef struct {
    int        modo, repeticoes;
    unsigned   grupos;             /* máscara pedida */
    int        qtd;
    sm_prova_t prova[SM_MAX_PROVAS];
    int        estado_grupo[SM_GRUPOS];
    unsigned   pontos_grupo[SM_GRUPOS];
    unsigned   total;              /* 0 = sin total */
    bool       total_parcial;      /* algún grupo del total quedó fuera */
    bool       erro;               /* alguna prueba dio ERROR */
    unsigned   tsc_khz;
    unsigned   duracao_ms;         /* por el reloj del sistema */
    unsigned   tsc_desvio_x10;     /* diferencia % x 10 entre el TSC y el reloj */
    char       cpu[49];
    char       hipervisor[13];     /* "" = máquina real */
    data_t     data;
} sm_resultado_t;

/* Llamada antes de cada prueba (hecho de total) y una vez al final (hecho = total). */
typedef void (*sm_progresso_t)(void *ctx, int feito, int total, const sm_prova_t *p);

const char *sm_nome_grupo(int g);
int  sm_grupo_por_nome(const char *nome);        /* "enteros", "fpu", "simd", "memoria", "2d", "sistema"; -1 */
/* El grupo 2D abre una ventana de 128x128 en una esquina de la pantalla: sin escritorio, sale n/d. */
bool sm_executar(sm_resultado_t *r, int modo, unsigned grupos, sm_progresso_t cb, void *ctx);
int  sm_texto(const sm_resultado_t *r, char *buf, int max);            /* tabla completa */
int  sm_comparar(const sm_resultado_t *r, char *buf, int max);         /* barras con las referencias */
int  sm_salvar(const sm_resultado_t *r, char *caminho, int max);       /* /home/saguimark-AAAAMMDD-HHMM.txt o /tmp */
const char *sm_aviso_maquina(const sm_resultado_t *r);                 /* TCG etc.; NULL = nada */

#endif
