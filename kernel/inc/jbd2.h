// SPDX-License-Identifier: GPL-2.0-or-later
/* jbd2.h - el diario JBD2 del ext3/ext4 (D326): reproducir lo pendiente al montar y
 * escribir transacciones.
 *
 * Formato (documentación pública del kernel, "Journal (jbd2)"): el diario es un archivo
 * (inodo 8) visto como un anillo de bloques. El bloque 0 es el superbloque del diario;
 * desde s_first van las transacciones: uno o más bloques descriptores (cada etiqueta dice
 * a qué bloque del volumen va la copia que la sigue), las copias, y un bloque de commit.
 * Los bloques de revocación anulan copias de transacciones anteriores. Todo en big endian.
 * Con csum v2/v3 cada bloque lleva su CRC32c (semilla: el UUID del diario).
 *
 * El módulo no sabe nada del ext: lee y escribe bloques físicos del volumen y traduce
 * los bloques del diario con las funciones que le pasa quien lo usa. Así también se
 * prueba en el PC (tests/ext4_unidad.c). */
#ifndef SAGUI_JBD2_H
#define SAGUI_JBD2_H
#include "tipos.h"

typedef struct jbd2 {
    /* Lo pone quien llama. */
    void     *ctx;
    int      (*ler)(void *ctx, uint32_t fis, uint8_t *buf);          /* bloque físico del volumen */
    int      (*escrever)(void *ctx, uint32_t fis, const uint8_t *buf);
    int      (*sincronizar)(void *ctx);                                /* barrera: lo escrito ya está en el disco */
    uint32_t (*mapear)(void *ctx, uint32_t lb);                       /* bloque del diario -> físico (0 = fallo) */
    uint32_t bloco;                                                   /* tamaño de bloque del volumen */
    uint32_t blocos_inode;                                            /* bloques que tiene el inodo del diario */
    uint32_t hora;                                                    /* para el commit (segundos UTC) */
    /* Lo rellena jbd2_abrir. */
    uint8_t  sb[1024];
    uint32_t primeiro, maxlen, seq, inicio;
    uint32_t compat, incompat, ro;
    bool     v2, csum2, csum3, b64;
    bool     escrita_ok;                     /* se sabe escribir en este diario */
    uint32_t semente, tam_tag;
    uint8_t *buf, *buf2;
    const char *motivo;                      /* por qué no se puede usar (texto para el aviso) */
    uint32_t reproduzidas, blocos_reproduzidos, revogados;
} jbd2_t;

int  jbd2_abrir(jbd2_t *j);                 /* lee y comprueba el superbloque del diario */
bool jbd2_vazio(const jbd2_t *j);           /* sin transacciones pendientes */
int  jbd2_recuperar(jbd2_t *j);             /* reproduce lo confirmado y deja el diario vacío */
int  jbd2_comecar(jbd2_t *j);               /* montaje rw: el diario pasa a "en uso" */
/* Una transacción: las copias de n bloques (fis[k] <- dados[k]) y el commit, con barreras.
 * Después quien llama graba los bloques en su sitio y llama a jbd2_avancar. */
int  jbd2_confirmar(jbd2_t *j, uint32_t n, const uint32_t *fis, uint8_t *const *dados);
int  jbd2_avancar(jbd2_t *j);               /* la transacción ya está en su sitio: deja de valer */
int  jbd2_fechar(jbd2_t *j);                /* desmontaje: diario vacío */
uint32_t jbd2_capacidade(const jbd2_t *j);  /* cuántos bloques caben en una transacción */
void jbd2_liberar(jbd2_t *j);

#endif
