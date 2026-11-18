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
    uint32_t       uid, gid;       /* D282: dueño y grupo */
    uint16_t       modo;           /* D282: 01777 (rwx de dueño, grupo y otros y el pegajoso) */
} no_t;

/* D282: dueño, grupo y permisos de una ruta, venga de donde venga (árbol en RAM,
 * ext2, o las opciones del montaje en FAT32/ISO/UDF). */
typedef struct {
    uint32_t uid, gid;
    uint16_t modo;                 /* 01777 */
    uint8_t  tipo;                 /* TIPO_*; 0 si el sistema no lo dice (FAT32: no hace falta mirarlo) */
    bool     com_donos;            /* el sistema de archivos los guarda (RAM, ext2) */
} meta_t;

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
/* D379: fecha de modificación (UTC) de una ruta, también en un volumen montado o en la raíz en disco; 0 = no se sabe. */
uint32_t vfs_data_modificacao(const char *caminho);
/* D282: los permisos de una ruta absoluta resuelta (0, o ERRO_NAO_EXISTE...), y
 * cambiarlos (sin comprobar quién: eso es de permisos.c). uid o gid -1 = no cambia. */
int      vfs_meta(const char *caminho, meta_t *m);
/* Lo mismo sin leer el disco en FAT32/ISO/UDF (todo tiene los permisos del montaje;
 * no dice si existe): para las carpetas de una ruta. */
int      vfs_meta_rapida(const char *caminho, meta_t *m);
void     vfs_home_de_root(bool sim);     /* D371: /home de root (sagui ya en /home/sagui) */
bool     vfs_home_e_de_root(void);
int      vfs_fixar_meta(const char *caminho, int32_t uid, int32_t gid, int32_t modo);
/* D341: fija las fechas (UTC; 0 = no cambia), sin comprobar quién (eso es de permisos.c). */
int      vfs_fixar_datas(const char *caminho, uint32_t modificado, uint32_t acessado);


void arquivos_herdar(struct processo *filho, struct processo *pai);
int  arquivos_caminho(uint32_t fd, char *saida, size_t max);   /* D341: la ruta de un descriptor */
/* D378 (fase 3): para MEM_COMPARTIR (imagens.c): ruta, tamaño y fecha de un fichero abierto para
 * leer, y leer de él a un búfer del kernel sin mover su posición. */
int  arquivos_imagem_info(uint32_t fd, char *caminho, size_t max, uint32_t *tamanho, uint32_t *modificado);
int32_t arquivos_ler_em(uint32_t fd, uint32_t pos, void *buf, uint32_t n);

void arquivos_fechar_todos(struct processo *p);

#endif
