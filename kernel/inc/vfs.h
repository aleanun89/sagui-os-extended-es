// SPDX-License-Identifier: GPL-3.0-only
/* vfs.h - capa de archivos virtual: una ruta sola para todo (PRD 4.3). */
#ifndef SAGUI_VFS_H
#define SAGUI_VFS_H
#include "tipos.h"
#include "abi.h"
#include "caminho.h"

struct processo;

/* Un archivo o carpeta en el árbol en memoria. */
typedef struct no {
    char           nome[NOME_MAX];
    uint8_t        tipo;           /* TIPO_ARQUIVO, TIPO_PASTA, TIPO_PROGRAMA */
    struct no     *pai, *filho, *irmao;
    uint32_t       tamanho;
    const uint8_t *original;       /* bytes en el ramdisk, hasta la primera escritura */
    uint8_t       *dados;          /* copia propia, después de la primera escritura */
    uint32_t       capacidade;
    uint32_t       refs;           /* cuántos archivos abiertos apuntan a él */
    bool           removido;       /* borrado mientras está abierto: desaparece al último cierre */
    uint32_t       modificado;     /* UTC (D080) */
} no_t;

void     vfs_iniciar(void);
no_t    *vfs_raiz(void);
no_t    *vfs_achar(const char *caminho);          /* ruta absoluta simplificada */
int      vfs_criar(const char *caminho, uint8_t tipo, no_t **saida);
uint32_t vfs_ler(no_t *x, uint32_t desloc, void *destino, uint32_t n);
int      vfs_escrever(no_t *x, uint32_t desloc, const void *origem, uint32_t n);
void     vfs_truncar(no_t *x);
int      vfs_redimensionar(no_t *x, uint32_t tamanho);   /* D116: reduce o aumenta (con ceros) */
int      vfs_apagar(const char *caminho);
int      vfs_remover_pasta(const char *caminho);
int      vfs_renomear(const char *origem, const char *destino);
int      vfs_listar(const char *caminho, int indice, entrada_pasta_t *e);
void     vfs_segurar(no_t *x);
void     vfs_soltar(no_t *x);
/* Lee un archivo entero a un búfer nuevo del kernel (liberar con kfree). */
int      vfs_ler_inteiro(const char *caminho, uint8_t **dados, uint32_t *tam);
/* D194: ¿existe el archivo (no carpeta)? También dentro de un volumen montado (/apps...). */
bool     vfs_existe_arquivo(const char *caminho);

void arquivos_herdar(struct processo *filho, struct processo *pai);
void arquivos_fechar_todos(struct processo *p);

#endif
