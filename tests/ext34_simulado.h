// SPDX-License-Identifier: GPL-2.0-or-later
/* ext34_simulado.h - lo mínimo del kernel para compilar ext2.c, jbd2.c y crc32c.c en el
 * PC (tests/ext34_unidad.c, D326/D337). Se incluye con -include antes de todo: marca como
 * ya incluidas las cabeceras del kernel que no hacen falta y declara lo que usan. */
#ifndef EXT34_SIMULADO_H
#define EXT34_SIMULADO_H
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

/* tipos.h del kernel: los de la libc del PC. */
#define SAGUI_TIPOS_H
#define EMPACOTADO  __attribute__((packed))
#include "abi.h"

#define SAGUI_MONTAGEM_H
#define SAGUI_PERMISOS_H
#define SAGUI_CONSOLE_H
#define SAGUI_MEMORIA_H
#define SAGUI_RELOGIO_H
#define SAGUI_TEXTO_H
#define SAGUI_DISCO_H
#define SAGUI_VFS_H

typedef struct disco {
    char     nome[12];
    uint32_t setores;
} disco_t;

typedef struct {
    uint32_t uid, gid;
    uint16_t modo;
    uint8_t  tipo;
    bool     com_donos;
} meta_t;

typedef struct sistema_arquivos {
    const char *nome;
    uint32_t    tam_arq;
    int  (*montar)(disco_t *d, uint32_t *opcoes, void **priv);
    int  (*desmontar)(void *priv, bool gravar);
    int  (*info)(void *priv, const char *rel, info_arquivo_t *inf);
    int  (*listar)(void *priv, const char *rel, int indice, entrada_pasta_t *e);
    int  (*abrir)(void *priv, const char *rel, int modo, void *arq, uint32_t *tamanho);
    int  (*ler)(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n);
    int  (*escrever)(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tamanho);
    int  (*truncar)(void *priv, void *arq, uint32_t tamanho);
    void (*fechar)(void *priv, void *arq);
    int  (*sincronizar)(void *priv, void *arq);
    int  (*criar_pasta)(void *priv, const char *rel);
    int  (*remover_pasta)(void *priv, const char *rel);
    int  (*apagar)(void *priv, const char *rel);
    int  (*renomear)(void *priv, const char *de, const char *para);
    void (*espaco)(void *priv, uint32_t *total_kb, uint32_t *livre_kb);
    int  (*datas)(void *priv, const char *rel, datas_arquivo_t *d);
    int  (*meta)(void *priv, const char *rel, meta_t *m);
    int  (*fixar_meta)(void *priv, const char *rel, int32_t uid, int32_t gid, int32_t modo);
    int  (*fixar_datas)(void *priv, const char *rel, uint32_t modificado, uint32_t acessado);   /* D341 */
} sistema_arquivos_t;

#define AVISO_INFO 5
#define CAMINHO_MAX 128
#define txt_tamanho(s) strlen(s)
/* Como la del kernel: copia hasta n - 1 caracteres y termina siempre en 0. */
static inline char *txt_copia_n(char *dst, const char *src, size_t n)
{
    if (n) { strncpy(dst, src, n - 1); dst[n - 1] = 0; }
    return dst;
}
#define TIQUES_POR_SEGUNDO 100

int      disco_ler(disco_t *d, uint32_t lba, uint32_t n, void *buf);
int      disco_escrever(disco_t *d, uint32_t lba, uint32_t n, const void *buf);
int      disco_sincronizar(disco_t *d);
void     disco_avisar(int tipo, const char *fmt, ...);
void     klog(const char *fmt, ...);
int      snformatar(char *dst, size_t max, const char *fmt, ...);
uint32_t hora_utc(void);
uint32_t relogio_tiques(void);
void     permisos_dono_novo(bool pasta, uint32_t *uid, uint32_t *gid, uint16_t *modo);
#define kmalloc(n)  malloc(n)
#define kcalloc(n)  calloc((n), 1)
#define kfree(p)    free(p)

#endif
