// SPDX-License-Identifier: GPL-2.0-or-later
/* particoes.h - tabla de particiones MBR (primarias, extendida y lógicas), GPT en
 * solo lectura, formateo FAT32/ext2 y deshacer (D179).
 *
 * Lo usan "particiones" (terminal), la ventana de "discos" y el instalador. Todo va
 * por SIS_DISCO_BLOCOS: el kernel pone las reglas de seguridad (reserva, nada
 * montado, disco de arranque, GPT, tabla coherente en el sector 0); aquí se
 * repiten y se amplían antes de escribir nada (pt_validar).
 *
 * Los sectores son de 512 bytes y absolutos dentro del disco, salvo donde se dice. */
#ifndef SAGUI_PARTICOES_H
#define SAGUI_PARTICOES_H
#include "sagui.h"

#define PT_MAX_LOG   16          /* como el kernel (MAX_LOGICAS) */
#define PT_MAX_GPT   16
#define PT_MB        2048u       /* sectores en 1 MB: la alineación de todo */

enum { PT_SEM_TABELA, PT_MBR, PT_GPT, PT_VOLUME };   /* esquema del disco (PT_VOLUME = FAT32/ext sin tabla) */

typedef struct {
    uint8_t  tipo;               /* tipo MBR; 0 = entrada libre */
    bool     ativa;              /* 0x80 */
    uint32_t inicio, setores;    /* absolutos */
    uint32_t ebr;                /* lógicas: dónde está su EBR */
} pt_part_t;

typedef struct {
    uint32_t inicio, setores;
    char     tipo[40];           /* el GUID de tipo, conocido o en texto */
    char     nome[40];           /* el nombre de la entrada (UTF-16 -> Latin-1) */
} pt_gpt_t;

typedef struct {
    char      disco[12];
    uint32_t  setores;
    int       esquema;
    uint8_t   setor0[512];       /* el sector 0 leído (el código de arranque se conserva) */
    pt_part_t prim[4];           /* por ranura: número 1 a 4 */
    int       nlog;
    pt_part_t log[PT_MAX_LOG];   /* en el orden de la cadena: números 5, 6... */
    int       ngpt;
    pt_gpt_t  gpt[PT_MAX_GPT];
} pt_tabela_t;

typedef struct {
    uint32_t inicio, setores;    /* ya alineados a 1 MB */
    bool     logica;             /* dentro de la extendida */
} pt_livre_t;

/* Acceso en bruto (chamadas.c). */
int blocos_info(const char *nome, info_bloco_t *i);
int blocos_reservar(const char *disco, int conf);
int blocos_liberar(const char *disco);
int blocos_reler(const char *disco);
int blocos_ler(const char *nome, uint32_t lba, uint32_t n, void *buf);        /* n sin límite: va por trozos */
int blocos_escrever(const char *nome, uint32_t lba, uint32_t n, const void *buf);
int blocos_descartar(const char *nome, uint32_t lba, uint32_t n);
int blocos_zerar(const char *nome, uint32_t lba, uint32_t n);                 /* escribe ceros */

/* Tabla. */
int  pt_ler(const char *disco, pt_tabela_t *t);
bool pt_validar(const pt_tabela_t *t, char *erro, int max);
int  pt_gravar(pt_tabela_t *t, const char *descricao, char *erro, int max);   /* el disco tiene que estar reservado */
int  pt_extendida(const pt_tabela_t *t);                      /* ranura (0..3) o -1 */
pt_part_t *pt_achar(pt_tabela_t *t, int numero);              /* 1..4 primarias, 5.. lógicas */
void pt_nome_particao(const char *disco, int numero, char *s, int max);       /* "hdb5", "usb0p1" */
int  pt_livres(const pt_tabela_t *t, pt_livre_t *v, int max);
const char *pt_nome_tipo(uint8_t tipo);
bool pt_tipo_extendida(uint8_t t);

/* Operaciones sobre la tabla en memoria (no escriben nada; luego pt_gravar).
 * Devuelven el número de la partición nueva o un error (y el texto en erro). */
int pt_criar(pt_tabela_t *t, uint32_t inicio, uint32_t setores, uint8_t tipo, bool logica, char *erro, int max);
int pt_apagar(pt_tabela_t *t, int numero, char *erro, int max);
int pt_redimensionar(pt_tabela_t *t, int numero, uint32_t setores, char *erro, int max);
int pt_ativar(pt_tabela_t *t, int numero, char *erro, int max);
int pt_mudar_tipo(pt_tabela_t *t, int numero, uint8_t tipo, char *erro, int max);

/* Deshacer: cada pt_gravar guarda los sectores de antes en /tmp/particiones/<disco>.deshacer. */
int  pt_deshacer(const char *disco, char *msg, int max);
int  pt_passos_deshacer(const char *disco);                   /* cuántos cambios se pueden deshacer */

/* Sistemas de archivos (formatacao.c). "volume" es el nombre de la partición. */
typedef void (*fs_progresso_t)(int por_cento, void *ctx);
int  fs_formatar_fat32(const char *volume, const char *rotulo, fs_progresso_t p, void *ctx, char *erro, int max);
int  fs_formatar_ext2(const char *volume, const char *rotulo, fs_progresso_t p, void *ctx, char *erro, int max);
/* Después de cambiar el tamaño de la partición: un FAT32 crece hasta donde le da la FAT; si no
 * se puede, se dice por qué. Encoger un sistema de archivos no se hace (D179). */
int  fs_crescer_fat32(const char *volume, char *msg, int max);
/* Tamaño que ocupa el sistema de archivos de un volumen (sectores), 0 si no se reconoce. */
uint32_t fs_tamanho(const char *volume, char *tipo, int max_tipo);

/* D213 (redimensao.c). Comprobar sin escribir nada (un fsck de solo lectura) un FAT32 o un
 * ext2: 0 si está como para tocarlo; si no, el error y el motivo en msg. */
int  fs_verificar(const char *volume, char *msg, int max);
/* Cambiar el tamaño de un ext2 para que quepa en "setores" (crecer o encoger, sin mover
 * datos: encoger se niega si lo que se corta está en uso). La partición ya tiene que medir
 * al menos eso. */
int  fs_redimensionar_ext2(const char *volume, uint32_t setores, fs_progresso_t p, void *ctx, char *msg, int max);
/* Lo mínimo (sectores, múltiplo de 1 MB) a lo que se puede encoger un ext2 sin mover datos. */
uint32_t fs_ext2_minimo(const char *volume);

/* Una operación completa (particoes_acoes.c): reserva el disco, cambia, verifica, escribe,
 * guarda el deshacer y suelta el disco (el kernel vuelve a leer la tabla). La usan el
 * terminal y la ventana, así las dos hacen exactamente lo mismo.
 * D213: la reserva se pide con DBLOCO_CONF_MONTADO: lo que esté montado (y la partición de
 * SaguiOS del disco de arranque) no se toca, pero el resto del disco sí; una operación sobre
 * una partición montada se niega con un mensaje que dice cuál desmontar. */
enum { PA_CRIAR, PA_APAGAR, PA_REDIMENSIONAR, PA_FORMATAR, PA_ATIVAR, PA_TRIM, PA_DESHACER, PA_TABELA_NOVA,
       PA_AMPLIAR };   /* D213: la extendida hasta el final del disco */
enum { PA_FS_NENHUM, PA_FS_FAT32, PA_FS_EXT2 };
typedef struct {
    int      op;
    char     disco[12];
    int      numero;             /* la partición (1..4 primarias, 5.. lógicas) */
    uint32_t inicio, setores;    /* crear: 0 = el primer hueco / todo el hueco; redimensionar: el tamaño nuevo */
    uint8_t  tipo;               /* crear: tipo MBR (0 = según el sistema de archivos) */
    bool     logica;             /* crear: dentro de la extendida */
    int      fs;                 /* crear (opcional) y formatear: PA_FS_* */
    char     rotulo[20];
    bool     trim;               /* borrar: además, descartar el contenido (sin vuelta atrás) */
    int      conf;               /* DBLOCO_CONF_* (el disco de arranque pide la confirmación doble) */
    fs_progresso_t progresso;
    void    *ctx;
    int      resultado;          /* crear: el número de la partición nueva */
} pt_acao_t;
int  pt_executar(pt_acao_t *a, char *msg, int max);
bool pt_separar_nome(const char *nome, char *disco, int max, int *numero);   /* "hdb5" -> "hdb", 5 */
const char *pt_nome_fs(int fs);

#endif
