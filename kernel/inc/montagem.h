// SPDX-License-Identifier: GPL-2.0-or-later
/* montagem.h - tabla de montajes y la interfaz de los sistemas de archivos (D040).
 *
 * Cada sistema de archivos (FAT32, ext2/ext3/ext4) se presenta como una
 * tabla de operaciones. Un montaje liga un volumen (disco_t) a una carpeta del
 * árbol en RAM; el VFS y las llamadas a archivos desvían hacia él toda
 * ruta que empieza en esa carpeta. Las rutas que llegan a las operaciones son
 * relativas al volumen, sin barra inicial ("" = raíz). */
#ifndef SAGUI_MONTAGEM_H
#define SAGUI_MONTAGEM_H
#include "tipos.h"
#include "abi.h"
#include "caminho.h"
#include "disco.h"
#include "trava.h"
#include "vfs.h"

#define MAX_MONTAGENS 8

typedef struct sistema_arquivos {
    const char *nome;                 /* "fat32", "ext2" */
    uint32_t    tam_arq;              /* bytes que ocupa un archivo abierto (asignados por la capa) */
    /* Monta: puede añadir MONTAR_SO_LEITURA en *opcoes (ej.: ext4). */
    int  (*montar)(disco_t *d, uint32_t *opcoes, void **priv);
    int  (*desmontar)(void *priv, bool gravar);   /* libera priv */
    int  (*info)(void *priv, const char *rel, info_arquivo_t *inf);
    int  (*listar)(void *priv, const char *rel, int indice, entrada_pasta_t *e);
    int  (*abrir)(void *priv, const char *rel, int modo, void *arq, uint32_t *tamanho);
    int  (*ler)(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n);
    int  (*escrever)(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tamanho);
    int  (*truncar)(void *priv, void *arq, uint32_t tamanho);
    void (*fechar)(void *priv, void *arq);
    int  (*sincronizar)(void *priv, void *arq);   /* D116; NULL = nada que hacer */
    int  (*criar_pasta)(void *priv, const char *rel);
    int  (*remover_pasta)(void *priv, const char *rel);
    int  (*apagar)(void *priv, const char *rel);
    int  (*renomear)(void *priv, const char *de, const char *para);
    void (*espaco)(void *priv, uint32_t *total_kb, uint32_t *livre_kb);
    int  (*datas)(void *priv, const char *rel, datas_arquivo_t *d);   /* D080; NULL = no guarda fechas */
    /* D282: dueño, grupo y modo (solo los que los guardan: ext2). NULL = los del montaje.
     * Al crear, el driver pone los de quien crea (permisos_dono_novo). */
    int  (*meta)(void *priv, const char *rel, meta_t *m);
    int  (*fixar_meta)(void *priv, const char *rel, int32_t uid, int32_t gid, int32_t modo);   /* -1 = no cambia */
    /* D341: fija las fechas de modificación y de acceso (UTC; 0 = no cambia). NULL = no las guarda
     * (ISO 9660, UDF). El driver pone también la del cambio del inodo (ext2) a la hora actual. */
    int  (*fixar_datas)(void *priv, const char *rel, uint32_t modificado, uint32_t acessado);
} sistema_arquivos_t;


typedef struct montagem {
    bool     usado;
    char     ponto[CAMINHO_MAX];      /* "/usb", "/home" */
    uint32_t tam_ponto;
    disco_t *disco;
    const sistema_arquivos_t *fs;
    void    *priv;
    uint32_t opcoes;                  /* MONTAR_SO_LEITURA... */
    uint32_t geracao;                 /* cambia con cada montaje/desmontaje */
    bool     criou_ponto;             /* la carpeta la creó el montaje: desaparece al desmontar */
    char     tipo[8];                 /* "fat32", "ext4", "iso9660", "udf" (D233: el que se usó) */
    uint32_t uid, gid, umask;         /* D282: dueño y máscara en los sistemas sin dueños (FAT32, ISO, UDF) */
} montagem_t;

/* Un archivo abierto en un montaje (arquivos.c). */
typedef struct {
    montagem_t *m;
    uint32_t    geracao;
    uint32_t    tamanho;
    uint8_t     dados[];              /* fs->tam_arq bytes del sistema de archivos */
} arq_montado_t;

extern const sistema_arquivos_t fs_fat32, fs_exfat, fs_ext2, fs_iso9660, fs_udf, fs_cdda;
extern trava_t trava_fs;   /* D095: una operación de sistema de archivos a la vez (ver montagem.c) */

/* Si la ruta absoluta está dentro de un montaje, devuelve este y el resto en
 * *rel ("" para el propio punto). */
montagem_t *montagem_de(const char *caminho, const char **rel);
montagem_t *montagem_indice(int i);
bool        montagem_viva(const arq_montado_t *a);
/* ¿Hay algún punto de montaje dentro de "caminho" (o igual)? */
bool        montagem_abaixo(const char *caminho);

/* tipo: "auto", "fat32", "exfat", "ext2", "ext3", "ext4", "iso9660", "udf" (NULL = auto). */
int  montagem_montar(disco_t *d, const char *ponto, const char *tipo, uint32_t opcoes);
/* D282: con dueño (uid | gid << 16) si opcoes lleva MONTAR_DONO, y la máscara si lleva MONTAR_COM_UMASK. */
int  montagem_montar_dono(disco_t *d, const char *ponto, const char *tipo, uint32_t opcoes, uint32_t dono);
/* D282: los permisos de "rel" dentro del montaje "m". Sin "olhar", en los sistemas
 * sin dueños no se lee el disco (no se sabe si existe ni el tipo). */
int  montagem_meta(montagem_t *m, const char *rel, meta_t *saida, bool olhar);
int  montagem_desmontar(montagem_t *m, bool gravar);
void montagem_desmontar_tudo(void);           /* antes de reiniciar */
void logs_disco_gravar(const char *motivo);   /* D120: anillo de klog -> /logs/kernel-*.log (si hay /logs) */

/* /etc/fstab: monta lo que esté listado para los discos presentes (al arrancar)
 * o para un disco recién conectado. */
void fstab_aplicar(disco_t *so_este);
void fstab_liberar(void);                     /* después del arranque, la conexión en caliente también consulta */

#endif
