// SPDX-License-Identifier: GPL-2.0-or-later
/* ext2.c - ext2, ext3 y ext4: lectura y escritura (D040, D326).
 *
 * Estructura de un volumen ext2 (documentación de Linux, "The Second Extended
 * Filesystem", y "ext4 Disk Layout"):
 *   - superbloque a 1024 bytes del inicio: tamaños, contadores, estado;
 *   - el volumen se divide en grupos de bloques; justo después del superbloque viene
 *     la tabla de descriptores de grupo (donde quedan el mapa de bits de bloques,
 *     el de inodos y la tabla de inodos de cada grupo, y cuántos libres hay);
 *   - cada archivo o carpeta es un inodo (número desde 1; la raíz es el 2),
 *     con 15 punteros: 12 bloques directos, un indirecto simple, uno doble y
 *     uno triple. El ext4 cambia los punteros por un árbol de "extents";
 *   - una carpeta es un archivo con entradas (inodo, tamaño del registro,
 *     tamaño del nombre, tipo, nombre). Las carpetas con índice (htree) siguen
 *     siendo legibles como lista: el índice queda escondido en entradas vacías.
 *
 * Escritura (D326):
 *   - ext2: como siempre, los metadatos en una caché por volumen grabada al final de
 *     cada operación y los datos de bloques enteros directo al disco;
 *   - ext3/ext4 con diario: al montar rw se reproduce lo pendiente (jbd2.c). Después, cada
 *     grabación de la caché es una transacción JBD2 en modo "ordered": los datos ya
 *     fueron al disco, los metadatos sucios van al diario con su commit, luego a su sitio,
 *     y el diario se da por vacío. Los bloques liberados no se reutilizan hasta el commit;
 *   - ext4: extents (el árbol se reconstruye entero al cambiar, salvo el caso de
 *     alargar el último extent), descriptores de 64 bytes, flex_bg, grupos sin
 *     inicializar (BLOCK_UNINIT/INODE_UNINIT) y las sumas CRC32c de metadata_csum
 *     (superbloque, descriptores, mapas de bits, inodos, extents y carpetas) o la CRC16
 *     de uninit_bg. Las carpetas con índice htree que se modifican pierden el índice de
 *     forma válida (sus bloques del índice pasan a ser bloques de entradas vacíos).
 *   - Lo que no se conoce (inline_data, cifrado, casefold, bigalloc, cuotas...) deja el
 *     volumen en solo lectura con el nombre de la característica.
 * Los nombres se graban en UTF-8 (como Linux) y se muestran en Latin-1 (D027). */
#include "montagem.h"
#include "permisos.h"
#include "console.h"
#include "memoria.h"
#include "relogio.h"
#include "texto.h"
#include "crc32c.h"
#include "jbd2.h"

/* Características (superbloque). */
#define COMPAT_DIARIO        0x0004
#define COMPAT_INDICE_DIR    0x0020
#define COMPAT_SPARSE2       0x0200
#define INCOMPAT_COMPRESSAO  0x0001
#define INCOMPAT_TIPO_DIR    0x0002
#define INCOMPAT_RECUPERAR   0x0004
#define INCOMPAT_DIARIO_DEV  0x0008
#define INCOMPAT_META_BG     0x0010
#define INCOMPAT_EXTENTS     0x0040
#define INCOMPAT_64BIT       0x0080
#define INCOMPAT_MMP         0x0100
#define INCOMPAT_FLEX_BG     0x0200
#define INCOMPAT_EA_INODE    0x0400
#define INCOMPAT_DIRDATA     0x1000
#define INCOMPAT_CSUM_SEED   0x2000
#define INCOMPAT_LARGEDIR    0x4000
#define INCOMPAT_INLINE      0x8000
#define INCOMPAT_CIFRADO     0x10000
#define INCOMPAT_CASEFOLD    0x20000
#define INCOMPAT_LEITURA     (INCOMPAT_TIPO_DIR | INCOMPAT_RECUPERAR | INCOMPAT_EXTENTS | INCOMPAT_64BIT | INCOMPAT_MMP | \
                              INCOMPAT_FLEX_BG | INCOMPAT_EA_INODE | INCOMPAT_CSUM_SEED | INCOMPAT_LARGEDIR | \
                              INCOMPAT_INLINE | INCOMPAT_CASEFOLD)
/* Las que se saben escribir (D326). */
#define INCOMPAT_ESCRITA     (INCOMPAT_TIPO_DIR | INCOMPAT_RECUPERAR | INCOMPAT_EXTENTS | INCOMPAT_64BIT | \
                              INCOMPAT_FLEX_BG | INCOMPAT_CSUM_SEED)
#define RO_SPARSE            0x0001
#define RO_ARQ_GRANDE        0x0002
#define RO_BTREE             0x0004
#define RO_HUGE_FILE         0x0008
#define RO_GDT_CSUM          0x0010
#define RO_DIR_NLINK         0x0020
#define RO_EXTRA_ISIZE       0x0040
#define RO_BIGALLOC          0x0200
#define RO_METADATA_CSUM     0x0400
#define RO_ESCRITA           (RO_SPARSE | RO_ARQ_GRANDE | RO_BTREE | RO_HUGE_FILE | RO_GDT_CSUM | RO_DIR_NLINK | \
                              RO_EXTRA_ISIZE | RO_METADATA_CSUM)

/* Descriptor de grupo (offsets; los "_alto" sólo con descriptores de 64 bytes). */
#define GD_LIVRES_B     0x0C
#define GD_LIVRES_I     0x0E
#define GD_PASTAS       0x10
#define GD_FLAGS        0x12
#define GD_SOMA_MB      0x18
#define GD_SOMA_MI      0x1A
#define GD_NAO_USADOS   0x1C
#define GD_SOMA         0x1E
#define GD_LIVRES_B_A   0x2C
#define GD_LIVRES_I_A   0x2E
#define GD_PASTAS_A     0x30
#define GD_NAO_USADOS_A 0x32
#define GD_SOMA_MB_A    0x38
#define GD_SOMA_MI_A    0x3A
#define BG_INODE_UNINIT 0x0001
#define BG_BLOCK_UNINIT 0x0002

/* Inodo */
#define MODO_TIPO     0xF000
#define MODO_ARQUIVO  0x8000
#define MODO_PASTA    0x4000
#define MODO_LIGACAO  0xA000
#define FL_INDICE     0x00001000   /* carpeta con htree */
#define FL_EXTENTS    0x00080000
#define FL_INLINE     0x10000000
#define RAIZ          2
#define EXT_MAGIA     0xF30A
#define EXT_MAX_LEN   32768u        /* extent inicializado más largo */

#define CACHE_BASE 24               /* ext2 y ext4 sin diario: como siempre */
#define CACHE_MAX  320              /* con diario: los sucios no se pueden expulsar (transacción) */
#define CACHE_SUAVE 256             /* con diario: a partir de aquí se confirma antes de crecer */
#define MAX_ABERTOS 32
#define MAX_EXTENTS 16384           /* un archivo con más extents se lee pero no se modifica */
#ifndef ESCRITA_EXT34
#define ESCRITA_EXT34 1             /* D326/D337: 0 = ext3/ext4 en solo lectura (como hasta la 8.4.3) */
#endif

/* Tipo de bloque en la caché (para su suma al grabarlo, D326). */
#define TB_OUTRO   0
#define TB_PASTA   1
#define TB_EXTENT  2

/* Marcas por grupo. */
#define GR_DESCR   0x01             /* el descriptor cambió: recalcular su suma */
#define GR_MB      0x02             /* el mapa de bloques cambió */
#define GR_MI      0x04
#define GR_MB_OK   0x08             /* la suma del mapa de bloques ya se comprobó */
#define GR_MI_OK   0x10

typedef struct { uint32_t ini, n; } faixa_t;

typedef struct {
    disco_t *d;
    uint8_t  sb[1024];
    uint32_t bloco, spb;              /* bytes y sectores por bloque */
    uint32_t blocos, inodes, primeiro_bloco, bpg, ipg, grupos;
    uint32_t tam_inode, primeiro_ino, desc_size;
    uint32_t compat, incompat, ro_compat;
    bool     so_leitura, tipo_dir;
    bool     csum, gdt_csum, extents;   /* metadata_csum, uninit_bg, archivos nuevos con extents */
    uint32_t semente;                   /* semilla de las sumas CRC32c */
    uint16_t estado_original;
    uint8_t *gd;                      /* descriptores de grupo (bloques enteros) */
    uint32_t gd_blocos;
    bool     gd_sujo, sb_sujo;
    uint8_t *gdb_sujo;                /* por bloque de descriptores */
    uint8_t *grupo;                   /* marcas GR_* por grupo */
    struct { uint32_t num; bool valido, sujo; uint8_t tipo; uint32_t uso, semente; uint8_t *dados; } cache[CACHE_MAX];
    uint32_t ncache, ncache_max;
    uint32_t relogio;
    int      erro;                    /* primer error de E/S de la operación */
    uint32_t dica;                    /* siguiente bloque a intentar asignar */
    uint32_t ultima_gravacao;         /* marca de la última grabación de la caché */
    /* Diario (D326). */
    bool     diario, confirmando;
    jbd2_t   j;
    struct { uint32_t lb, pb, n; } jmapa[64];
    uint32_t jmapa_n;
    uint32_t *tx_fis;                 /* listas para una transacción */
    uint8_t **tx_dados;
    faixa_t *liberados;               /* bloques liberados en la transacción abierta */
    uint32_t n_liberados, cap_liberados;
    /* Continuación del "listar" (ls de carpetas grandes sin releer todo). */
    uint32_t lst_ino, lst_indice, lst_lb, lst_desl;
    /* Archivos abiertos: un inodo borrado mientras está abierto sólo desaparece al final. */
    struct { uint32_t ino, refs; bool orfao; } abertos[MAX_ABERTOS];
    uint32_t geracao;                 /* para i_generation de los inodos nuevos */
} e2_t;

typedef struct { uint32_t ino; } e2_arq_t;

typedef struct {
    uint32_t num;
    union { uint8_t b[256]; uint32_t w[64]; };
} inode_t;

static inline uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static inline uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static inline void st16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static inline void st32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

#define I_MODO(i)     le16((i)->b + 0)
#define I_TAM(i)      le32((i)->b + 4)
#define I_LIGACOES(i) le16((i)->b + 26)
#define I_SETORES(i)  le32((i)->b + 28)
#define I_FLAGS(i)    le32((i)->b + 32)
#define I_BLOCO(i)    (&(i)->w[10])           /* i_block[15], byte 40 */
#define I_GERACAO(i)  le32((i)->b + 100)
#define I_ACL(i)      le32((i)->b + 104)
/* D282: dueño y grupo de 32 bits (los 16 altos en el osd2 de Linux). */
#define I_UID(i)      ((uint32_t)le16((i)->b + 2) | (uint32_t)le16((i)->b + 120) << 16)
#define I_GID(i)      ((uint32_t)le16((i)->b + 24) | (uint32_t)le16((i)->b + 122) << 16)
#define EH_PASTA(i)   ((I_MODO(i) & MODO_TIPO) == MODO_PASTA)
#define EH_ARQUIVO(i) ((I_MODO(i) & MODO_TIPO) == MODO_ARQUIVO)

static int confirmar(e2_t *v);

/* ------------------------------------------------------------ hora */

/* Segundos desde 1/1/1970 en UTC, como manda el ext2: la hora del sistema
 * (D080), que ya descontó la zona si el RTC está en hora local. */
static uint32_t agora(void) { return hora_utc(); }

/* ------------------------------------------------------------ sumas (D326) */

static bool com_soma_grupo(e2_t *v) { return v->csum || v->gdt_csum; }

static uint32_t soma_u32(uint32_t crc, uint32_t x)
{
    uint8_t b[4];
    st32(b, x);
    return crc32c(crc, b, 4);
}

/* Semilla de las sumas de los bloques de un inodo (extents, carpetas). */
static uint32_t semente_inode(e2_t *v, uint32_t num, uint32_t geracao)
{
    return soma_u32(soma_u32(v->semente, num), geracao);
}

static uint32_t soma_sb(const uint8_t *sb) { return crc32c(~0u, sb, 0x3FC); }

static uint16_t soma_gd(e2_t *v, uint32_t g, const uint8_t *d)
{
    uint8_t zero[2] = { 0, 0 };
    if (v->csum) {
        uint32_t c = soma_u32(v->semente, g);
        c = crc32c(c, d, GD_SOMA);
        c = crc32c(c, zero, 2);
        if (v->desc_size > GD_SOMA + 2) c = crc32c(c, d + GD_SOMA + 2, v->desc_size - GD_SOMA - 2);
        return (uint16_t)c;
    }
    uint8_t gb[4];
    st32(gb, g);
    uint16_t c = crc16_ext4(0xFFFF, v->sb + 104, 16);   /* s_uuid */
    c = crc16_ext4(c, gb, 4);
    c = crc16_ext4(c, d, GD_SOMA);
    if ((v->incompat & INCOMPAT_64BIT) && v->desc_size > GD_SOMA + 2) c = crc16_ext4(c, d + GD_SOMA + 2, v->desc_size - GD_SOMA - 2);
    return c;
}

/* ¿El inodo (en el bloque, con su tamaño real) tiene sitio para i_checksum_hi? */
static bool inode_soma_alta(e2_t *v, const uint8_t *x)
{
    return v->tam_inode > 128 && le16(x + 128) >= 4;
}

static uint32_t soma_inode(e2_t *v, uint32_t num, const uint8_t *x)
{
    uint8_t c[1024];
    uint32_t n = v->tam_inode <= sizeof c ? v->tam_inode : sizeof c;
    memcpy(c, x, n);
    st16(c + 0x7C, 0);
    if (inode_soma_alta(v, x)) st16(c + 0x82, 0);
    return crc32c(semente_inode(v, num, le32(x + 100)), c, n);
}

static void selar_inode(e2_t *v, uint32_t num, uint8_t *x)
{
    if (!v->csum) return;
    uint32_t s = soma_inode(v, num, x);
    st16(x + 0x7C, s & 0xFFFF);
    if (inode_soma_alta(v, x)) st16(x + 0x82, s >> 16);
}

static bool inode_soma_ok(e2_t *v, uint32_t num, const uint8_t *x)
{
    if (!v->csum) return true;
    uint32_t s = soma_inode(v, num, x);
    if (le16(x + 0x7C) != (s & 0xFFFF)) return false;
    return !inode_soma_alta(v, x) || le16(x + 0x82) == (s >> 16);
}

/* La "entrada" de 12 bytes al final de un bloque de carpeta con la suma (metadata_csum). */
static uint32_t cauda(e2_t *v) { return v->csum ? 12 : 0; }

static bool tem_cauda(e2_t *v, const uint8_t *b)
{
    const uint8_t *t = b + v->bloco - 12;
    return le32(t) == 0 && le16(t + 4) == 12 && t[6] == 0 && t[7] == 0xDE;
}

static void por_cauda(e2_t *v, uint8_t *b)
{
    uint8_t *t = b + v->bloco - 12;
    memset(t, 0, 12);
    st16(t + 4, 12);
    t[7] = 0xDE;
}

static uint32_t ext_cauda_off(e2_t *v, const uint8_t *b)
{
    uint32_t o = 12 + 12 * (uint32_t)le16(b + 4);
    return o + 4 <= v->bloco ? o : 0;
}

/* Suma de un bloque de la caché antes de grabarlo, según lo que es. */
static void selar_bloco(e2_t *v, int i)
{
    if (!v->csum) return;
    uint8_t *b = v->cache[i].dados;
    if (v->cache[i].tipo == TB_PASTA) {
        if (tem_cauda(v, b)) st32(b + v->bloco - 4, crc32c(v->cache[i].semente, b, v->bloco - 12));
    } else if (v->cache[i].tipo == TB_EXTENT) {
        uint32_t o = ext_cauda_off(v, b);
        if (o && le16(b) == EXT_MAGIA) st32(b + o, crc32c(v->cache[i].semente, b, o));
    }
}

/* ------------------------------------------------------------ caché de bloques */

static int erro_es(e2_t *v) { if (!v->erro) v->erro = ERRO_ACESSO; return ERRO_ACESSO; }

static int gravar_slot(e2_t *v, int i)
{
    if (!v->cache[i].sujo) return 0;
    selar_bloco(v, i);
    if (disco_escrever(v->d, v->cache[i].num * v->spb, v->spb, v->cache[i].dados) < 0) return erro_es(v);
    v->cache[i].sujo = false;
    return 0;
}

static int escolher_vitima(e2_t *v, int *livre)
{
    int velho = -1;
    *livre = -1;
    for (uint32_t i = 0; i < v->ncache; i++) {
        if (!v->cache[i].valido) { if (*livre < 0) *livre = (int)i; continue; }
        if (v->diario && v->cache[i].sujo) continue;    /* parte de la transacción abierta */
        if (velho < 0 || v->cache[i].uso < v->cache[velho].uso) velho = (int)i;
    }
    return velho;
}

static int achar_slot(e2_t *v, uint32_t b, bool *achou)
{
    for (uint32_t i = 0; i < v->ncache; i++)
        if (v->cache[i].valido && v->cache[i].num == b) { *achou = true; v->cache[i].uso = ++v->relogio; return (int)i; }
    *achou = false;
    int livre, velho = escolher_vitima(v, &livre);
    int i = livre;
    if (i < 0 && v->diario) {
        /* Con diario: crecer hasta CACHE_SUAVE; después confirmar la transacción (si no se
         * está confirmando ya) y, en último caso, crecer hasta CACHE_MAX. */
        if (v->ncache < CACHE_SUAVE && v->ncache < v->ncache_max) i = (int)v->ncache++;
        else if (velho < 0 && !v->confirmando) {
            klog("[EXT2] %s: transacción llena: se confirma a mitad de operación\n", v->d->nome);
            if (confirmar(v) < 0) return -1;
            velho = escolher_vitima(v, &livre);
        }
        if (i < 0 && velho < 0 && v->ncache < v->ncache_max) i = (int)v->ncache++;
    }
    if (i < 0) i = velho;
    if (i < 0) { erro_es(v); return -1; }
    if (v->cache[i].valido && gravar_slot(v, i) < 0) return -1;
    if (!v->cache[i].dados && !(v->cache[i].dados = kmalloc(v->bloco))) { erro_es(v); return -1; }
    v->cache[i].valido = false;
    v->cache[i].tipo = TB_OUTRO;
    return i;
}

/* El contenido de un bloque (NULL si falló). Vale hasta la próxima llamada que
 * toque la caché. */
static uint8_t *bloco(e2_t *v, uint32_t b)
{
    if (b == 0 || b >= v->blocos) { erro_es(v); return NULL; }
    bool achou;
    int i = achar_slot(v, b, &achou);
    if (i < 0) return NULL;
    if (achou) return v->cache[i].dados;
    if (disco_ler(v->d, b * v->spb, v->spb, v->cache[i].dados) < 0) { erro_es(v); return NULL; }
    v->cache[i].num = b;
    v->cache[i].valido = true;
    v->cache[i].sujo = false;
    v->cache[i].uso = ++v->relogio;
    return v->cache[i].dados;
}

/* Un bloque recién asignado: en cero en la caché, sin leer el disco. */
static uint8_t *bloco_novo(e2_t *v, uint32_t b)
{
    if (b == 0 || b >= v->blocos) { erro_es(v); return NULL; }
    bool achou;
    int i = achar_slot(v, b, &achou);
    if (i < 0) return NULL;
    memset(v->cache[i].dados, 0, v->bloco);
    v->cache[i].num = b;
    v->cache[i].valido = true;
    v->cache[i].sujo = true;
    v->cache[i].tipo = TB_OUTRO;
    v->cache[i].uso = ++v->relogio;
    return v->cache[i].dados;
}

static void sujar(e2_t *v, uint32_t b)
{
    for (uint32_t i = 0; i < v->ncache; i++)
        if (v->cache[i].valido && v->cache[i].num == b) v->cache[i].sujo = true;
}

/* Sucio y con lo necesario para su suma (bloque de carpeta o de extents de un inodo). */
static void sujar_t(e2_t *v, uint32_t b, uint8_t tipo, uint32_t semente)
{
    for (uint32_t i = 0; i < v->ncache; i++)
        if (v->cache[i].valido && v->cache[i].num == b) { v->cache[i].sujo = true; v->cache[i].tipo = tipo; v->cache[i].semente = semente; }
}

static bool esta_sujo(e2_t *v, uint32_t b)
{
    for (uint32_t i = 0; i < v->ncache; i++)
        if (v->cache[i].valido && v->cache[i].num == b) return v->cache[i].sujo;
    return false;
}

static void esquecer(e2_t *v, uint32_t b)
{
    for (uint32_t i = 0; i < v->ncache; i++)
        if (v->cache[i].valido && v->cache[i].num == b) v->cache[i].valido = v->cache[i].sujo = false;
}

/* Bloques de datos enteros, directo del disco (con lo que esté sucio en la caché encima). */
static int ler_direto(e2_t *v, uint32_t b, uint32_t n, uint8_t *dest)
{
    if (b + n > v->blocos || b + n < b) return erro_es(v);
    if (disco_ler(v->d, b * v->spb, n * v->spb, dest) < 0) return erro_es(v);
    for (uint32_t i = 0; i < v->ncache; i++)
        if (v->cache[i].valido && v->cache[i].sujo && v->cache[i].num >= b && v->cache[i].num < b + n)
            memcpy(dest + (v->cache[i].num - b) * v->bloco, v->cache[i].dados, v->bloco);
    return 0;
}

/* Datos de un bloque entero directo al disco. Con diario es el modo "ordered": van antes
 * del commit de los metadatos que los apuntan. */
static int gravar_direto(e2_t *v, uint32_t b, const uint8_t *orig)
{
    for (uint32_t i = 0; i < v->ncache; i++)
        if (v->cache[i].valido && v->cache[i].num == b) { memcpy(v->cache[i].dados, orig, v->bloco); v->cache[i].sujo = false; }
    if (disco_escrever(v->d, b * v->spb, v->spb, orig) < 0) return erro_es(v);
    return 0;
}

static int gravar_sb(e2_t *v)
{
    v->sb_sujo = false;
    if (v->csum) st32(v->sb + 0x3FC, soma_sb(v->sb));
    if (disco_escrever(v->d, 2, 2, v->sb) < 0) return erro_es(v);
    return 0;
}

/* ------------------------------------------------------------ grupos */

static uint8_t *gd(e2_t *v, uint32_t g) { return v->gd + g * v->desc_size; }
static uint32_t gd_mapa_blocos(e2_t *v, uint32_t g) { return le32(gd(v, g) + 0); }
static uint32_t gd_mapa_inodes(e2_t *v, uint32_t g) { return le32(gd(v, g) + 4); }
static uint32_t gd_tabela(e2_t *v, uint32_t g) { return le32(gd(v, g) + 8); }
static bool     gd64(e2_t *v) { return v->desc_size >= 64; }

static uint32_t gd_campo(e2_t *v, uint32_t g, int lo, int alto)
{
    uint32_t x = le16(gd(v, g) + lo);
    if (gd64(v)) x |= (uint32_t)le16(gd(v, g) + alto) << 16;
    return x;
}

static void gd_por_campo(e2_t *v, uint32_t g, int lo, int alto, uint32_t x)
{
    st16(gd(v, g) + lo, x & 0xFFFF);
    if (gd64(v)) st16(gd(v, g) + alto, x >> 16);
}

static void marcar_grupo(e2_t *v, uint32_t g, uint8_t bits)
{
    v->grupo[g] |= bits | GR_DESCR;
    v->gdb_sujo[(g * v->desc_size) / v->bloco] = 1;
    v->gd_sujo = true;
}

static uint16_t gd_flags(e2_t *v, uint32_t g) { return com_soma_grupo(v) ? le16(gd(v, g) + GD_FLAGS) : 0; }

/* Suma "delta" a un contador del grupo (y al del superbloque, si off_sb >= 0). */
static void contar(e2_t *v, uint32_t g, int lo, int alto, int off_sb, int delta)
{
    gd_por_campo(v, g, lo, alto, gd_campo(v, g, lo, alto) + (uint32_t)delta);
    if (off_sb >= 0) st32(v->sb + off_sb, le32(v->sb + off_sb) + (uint32_t)delta);
    marcar_grupo(v, g, 0);
    v->sb_sujo = true;
}

/* Cuántos bloques tiene el grupo g (el último puede ser menor). */
static uint32_t blocos_no_grupo(e2_t *v, uint32_t g)
{
    uint32_t ini = v->primeiro_bloco + g * v->bpg;
    return v->blocos - ini < v->bpg ? v->blocos - ini : v->bpg;
}

static bool potencia_de(uint32_t g, uint32_t p)
{
    uint32_t x = p;
    while (x < g) x *= p;
    return x == g;
}

/* ¿El grupo g lleva copia del superbloque y de los descriptores? */
static bool tem_super(e2_t *v, uint32_t g)
{
    if (g == 0) return true;
    if (v->compat & COMPAT_SPARSE2) return g == le32(v->sb + 0x24C) || g == le32(v->sb + 0x250);
    if (!(v->ro_compat & RO_SPARSE)) return true;
    return g == 1 || potencia_de(g, 3) || potencia_de(g, 5) || potencia_de(g, 7);
}

static void marcar_bit_grupo(e2_t *v, uint8_t *m, uint32_t base, uint32_t qtd, uint32_t b, uint32_t n)
{
    (void)v;
    for (uint32_t k = 0; k < n; k++)
        if (b + k >= base && b + k < base + qtd) m[(b + k - base) / 8] |= (uint8_t)(1 << ((b + k - base) % 8));
}

/* Mapa de bloques de un grupo BLOCK_UNINIT: sólo sus metadatos (copia del superbloque, de
 * los descriptores y los reservados) y los de cualquier grupo que caigan aquí (flex_bg). */
static void iniciar_mapa_blocos(e2_t *v, uint32_t g, uint8_t *m)
{
    memset(m, 0, v->bloco);
    uint32_t base = v->primeiro_bloco + g * v->bpg, qtd = blocos_no_grupo(v, g);
    if (tem_super(v, g)) marcar_bit_grupo(v, m, base, qtd, base, 1 + v->gd_blocos + le16(v->sb + 0xCE));
    uint32_t tab = (v->ipg * v->tam_inode + v->bloco - 1) / v->bloco;
    for (uint32_t h = 0; h < v->grupos; h++) {
        marcar_bit_grupo(v, m, base, qtd, gd_mapa_blocos(v, h), 1);
        marcar_bit_grupo(v, m, base, qtd, gd_mapa_inodes(v, h), 1);
        marcar_bit_grupo(v, m, base, qtd, gd_tabela(v, h), tab);
    }
    for (uint32_t bit = qtd; bit < v->bloco * 8; bit++) m[bit / 8] |= (uint8_t)(1 << (bit % 8));
}

static bool mapa_soma_ok(e2_t *v, uint32_t g, const uint8_t *m, bool inodes)
{
    if (!v->csum) return true;
    uint32_t s = crc32c(v->semente, m, inodes ? v->ipg / 8 : v->bpg / 8);
    int lo = inodes ? GD_SOMA_MI : GD_SOMA_MB, alto = inodes ? GD_SOMA_MI_A : GD_SOMA_MB_A;
    if (le16(gd(v, g) + lo) != (s & 0xFFFF)) return false;
    return !gd64(v) || le16(gd(v, g) + alto) == (s >> 16);
}

/* El mapa de bits de bloques (o de inodos) de un grupo, inicializado si hacía falta y con
 * la suma comprobada la primera vez. NULL si falla. */
static uint8_t *mapa(e2_t *v, uint32_t g, bool inodes)
{
    uint32_t mb = inodes ? gd_mapa_inodes(v, g) : gd_mapa_blocos(v, g);
    uint16_t sem_iniciar = inodes ? BG_INODE_UNINIT : BG_BLOCK_UNINIT;
    if (gd_flags(v, g) & sem_iniciar) {
        uint8_t *m = bloco_novo(v, mb);
        if (!m) return NULL;
        if (inodes) for (uint32_t bit = v->ipg; bit < v->bloco * 8; bit++) m[bit / 8] |= (uint8_t)(1 << (bit % 8));
        else iniciar_mapa_blocos(v, g, m);
        st16(gd(v, g) + GD_FLAGS, le16(gd(v, g) + GD_FLAGS) & ~sem_iniciar);
        marcar_grupo(v, g, (uint8_t)((inodes ? GR_MI | GR_MI_OK : GR_MB | GR_MB_OK)));
        return m;
    }
    uint8_t *m = bloco(v, mb);
    if (!m) return NULL;
    uint8_t ok = inodes ? GR_MI_OK : GR_MB_OK;
    if (!(v->grupo[g] & ok)) {
        if (!esta_sujo(v, mb) && !mapa_soma_ok(v, g, m, inodes)) {
            klog("[EXT2] %s: el mapa de %s del grupo %u tiene la suma mal\n", v->d->nome, inodes ? "inodos" : "bloques", g);
            erro_es(v);
            return NULL;
        }
        v->grupo[g] |= ok;
    }
    return m;
}

static uint32_t alocar_bloco(e2_t *v, uint32_t perto)
{
    if (perto < v->primeiro_bloco || perto >= v->blocos) perto = v->dica;
    if (perto < v->primeiro_bloco || perto >= v->blocos) perto = v->primeiro_bloco;
    uint32_t g0 = (perto - v->primeiro_bloco) / v->bpg;
    for (uint32_t k = 0; k <= v->grupos; k++) {
        uint32_t g = (g0 + k) % v->grupos;
        if (gd_campo(v, g, GD_LIVRES_B, GD_LIVRES_B_A) == 0) continue;
        uint32_t qtd = blocos_no_grupo(v, g);
        uint32_t ini = (k == 0) ? (perto - v->primeiro_bloco) % v->bpg : 0;
        uint8_t *m = mapa(v, g, false);
        if (!m) return 0;
        uint32_t mb = gd_mapa_blocos(v, g);
        for (uint32_t j = 0; j < qtd; j++) {
            uint32_t bit = (ini + j) % qtd;
            if (m[bit / 8] & (1 << (bit % 8))) continue;
            m[bit / 8] |= (uint8_t)(1 << (bit % 8));
            sujar(v, mb);
            v->grupo[g] |= GR_MB;
            contar(v, g, GD_LIVRES_B, GD_LIVRES_B_A, 12, -1);
            uint32_t b = v->primeiro_bloco + g * v->bpg + bit;
            v->dica = b + 1;
            return b;
        }
    }
    return 0;   /* volumen lleno */
}

static void liberar_bloco_ja(e2_t *v, uint32_t b)
{
    if (b < v->primeiro_bloco || b >= v->blocos) return;
    uint32_t g = (b - v->primeiro_bloco) / v->bpg, bit = (b - v->primeiro_bloco) % v->bpg;
    uint8_t *m = mapa(v, g, false);
    if (!m || !(m[bit / 8] & (1 << (bit % 8)))) return;
    m[bit / 8] &= (uint8_t)~(1 << (bit % 8));
    sujar(v, gd_mapa_blocos(v, g));
    v->grupo[g] |= GR_MB;
    contar(v, g, GD_LIVRES_B, GD_LIVRES_B_A, 12, +1);
}

/* Con diario, un bloque liberado no vuelve al mapa hasta el commit: si se reutilizara
 * para datos (que van directo al disco) y se cortara la luz antes del commit, los
 * metadatos viejos todavía lo apuntarían. */
static void liberar_bloco(e2_t *v, uint32_t b)
{
    if (b < v->primeiro_bloco || b >= v->blocos) return;
    esquecer(v, b);
    if (!v->diario || v->confirmando) { liberar_bloco_ja(v, b); return; }
    if (v->n_liberados && v->liberados[v->n_liberados - 1].ini + v->liberados[v->n_liberados - 1].n == b) {
        v->liberados[v->n_liberados - 1].n++;
        return;
    }
    if (v->n_liberados == v->cap_liberados) {
        uint32_t cap = v->cap_liberados ? v->cap_liberados * 2 : 256;
        faixa_t *n = cap <= 65536 ? kmalloc(cap * sizeof *n) : NULL;
        if (!n) {
            /* Sin sitio para apuntarlo: se libera ya (sólo pierde la protección de arriba). */
            klog("[EXT2] %s: demasiados bloques liberados en una transacción\n", v->d->nome);
            liberar_bloco_ja(v, b);
            return;
        }
        if (v->liberados) { memcpy(n, v->liberados, v->n_liberados * sizeof *n); kfree(v->liberados); }
        v->liberados = n;
        v->cap_liberados = cap;
    }
    v->liberados[v->n_liberados].ini = b;
    v->liberados[v->n_liberados].n = 1;
    v->n_liberados++;
}

static uint32_t alocar_inode(e2_t *v, uint32_t g0, bool pasta)
{
    for (uint32_t k = 0; k < v->grupos; k++) {
        uint32_t g = (g0 + k) % v->grupos;
        if (gd_campo(v, g, GD_LIVRES_I, GD_LIVRES_I_A) == 0) continue;
        uint8_t *m = mapa(v, g, true);
        if (!m) return 0;
        uint32_t mi = gd_mapa_inodes(v, g);
        for (uint32_t bit = 0; bit < v->ipg; bit++) {
            uint32_t num = g * v->ipg + bit + 1;
            if (num < v->primeiro_ino || num > v->inodes) continue;
            if (m[bit / 8] & (1 << (bit % 8))) continue;
            m[bit / 8] |= (uint8_t)(1 << (bit % 8));
            sujar(v, mi);
            v->grupo[g] |= GR_MI;
            contar(v, g, GD_LIVRES_I, GD_LIVRES_I_A, 16, -1);
            if (pasta) contar(v, g, GD_PASTAS, GD_PASTAS_A, -1, +1);
            if (com_soma_grupo(v)) {
                /* bg_itable_unused: los inodos del final de la tabla que nunca se usaron */
                uint32_t nu = gd_campo(v, g, GD_NAO_USADOS, GD_NAO_USADOS_A);
                if (bit + 1 > v->ipg - nu) gd_por_campo(v, g, GD_NAO_USADOS, GD_NAO_USADOS_A, v->ipg - bit - 1);
            }
            return num;
        }
    }
    return 0;
}

static void liberar_inode_bitmap(e2_t *v, uint32_t num, bool pasta)
{
    uint32_t g = (num - 1) / v->ipg, bit = (num - 1) % v->ipg;
    uint8_t *m = mapa(v, g, true);
    if (!m || !(m[bit / 8] & (1 << (bit % 8)))) return;
    m[bit / 8] &= (uint8_t)~(1 << (bit % 8));
    sujar(v, gd_mapa_inodes(v, g));
    v->grupo[g] |= GR_MI;
    contar(v, g, GD_LIVRES_I, GD_LIVRES_I_A, 16, +1);
    if (pasta) contar(v, g, GD_PASTAS, GD_PASTAS_A, -1, -1);
}

/* ------------------------------------------------------------ grabación y transacciones */

/* Antes de grabar: las sumas de los mapas de bits y de los descriptores que cambiaron. */
static void selar_grupos(e2_t *v)
{
    if (!com_soma_grupo(v)) return;
    for (uint32_t g = 0; g < v->grupos; g++) {
        uint8_t f = v->grupo[g];
        if (!(f & (GR_DESCR | GR_MB | GR_MI))) continue;
        if (v->csum && (f & (GR_MB | GR_MI))) {
            for (int k = 0; k < 2; k++) {
                bool ino = k == 1;
                if (!(f & (ino ? GR_MI : GR_MB))) continue;
                uint8_t *m = bloco(v, ino ? gd_mapa_inodes(v, g) : gd_mapa_blocos(v, g));
                if (!m) continue;
                uint32_t s = crc32c(v->semente, m, ino ? v->ipg / 8 : v->bpg / 8);
                st16(gd(v, g) + (ino ? GD_SOMA_MI : GD_SOMA_MB), s & 0xFFFF);
                if (gd64(v)) st16(gd(v, g) + (ino ? GD_SOMA_MI_A : GD_SOMA_MB_A), s >> 16);
            }
        }
        st16(gd(v, g) + GD_SOMA, soma_gd(v, g, gd(v, g)));
        v->grupo[g] = (uint8_t)(f & (GR_MB_OK | GR_MI_OK));
        v->gdb_sujo[(g * v->desc_size) / v->bloco] = 1;
        v->gd_sujo = true;
    }
}

/* Graba todo lo pendiente: con diario, como una transacción JBD2 (D326). */
static int confirmar(e2_t *v)
{
    if (v->so_leitura || v->confirmando) return 0;
    v->confirmando = true;
    /* Los bloques liberados en esta transacción vuelven al mapa (en la misma transacción). */
    for (uint32_t k = 0; k < v->n_liberados; k++)
        for (uint32_t b = 0; b < v->liberados[k].n; b++) liberar_bloco_ja(v, v->liberados[k].ini + b);
    v->n_liberados = 0;
    selar_grupos(v);
    uint32_t n = 0;
    for (uint32_t i = 0; i < v->ncache; i++)
        if (v->cache[i].valido && v->cache[i].sujo) {
            selar_bloco(v, (int)i);
            v->tx_fis[n] = v->cache[i].num;
            v->tx_dados[n++] = v->cache[i].dados;
        }
    if (v->gd_sujo)
        for (uint32_t k = 0; k < v->gd_blocos; k++)
            if (v->gdb_sujo[k]) { v->tx_fis[n] = v->primeiro_bloco + 1 + k; v->tx_dados[n++] = v->gd + k * v->bloco; }
    int r = 0;
    if (v->diario && n) {
        v->j.hora = agora();
        if (jbd2_confirmar(&v->j, n, v->tx_fis, v->tx_dados) < 0) {
            /* Sin commit no se toca nada en su sitio: el volumen pasa a solo lectura. */
            klog("[EXT2] %s: no se pudo escribir en el diario (%s): solo lectura\n", v->d->nome,
                 v->j.motivo ? v->j.motivo : "error");
            disco_avisar(AVISO_INFO, "%s: error en el diario: solo lectura", v->d->nome);
            v->so_leitura = true;
            v->confirmando = false;
            return erro_es(v);
        }
    }
    for (uint32_t k = 0; k < n; k++)
        if (disco_escrever(v->d, v->tx_fis[k] * v->spb, v->spb, v->tx_dados[k]) < 0) r = erro_es(v);
    for (uint32_t i = 0; i < v->ncache; i++) v->cache[i].sujo = false;
    for (uint32_t k = 0; k < v->gd_blocos; k++) v->gdb_sujo[k] = 0;
    v->gd_sujo = false;
    if (v->diario && n) {
        if (disco_sincronizar(v->d) < 0) r = erro_es(v);
        if (jbd2_avancar(&v->j) < 0) r = erro_es(v);
    }
    if (v->sb_sujo && gravar_sb(v) < 0) r = ERRO_ACESSO;
    v->confirmando = false;
    return r;
}

/* Fin de cada operación: graba la caché, los descriptores y el superbloque. */
static int terminar(e2_t *v, int r)
{
    if (!v->so_leitura) {
        v->ultima_gravacao = relogio_tiques();
        confirmar(v);
    }
    if (v->erro) { if (r >= 0) r = v->erro; v->erro = 0; }
    return r;
}

/* ------------------------------------------------------------ inodos */

static bool inode_onde(e2_t *v, uint32_t num, uint32_t *b, uint32_t *off)
{
    if (num < 1 || num > v->inodes) return false;
    uint32_t g = (num - 1) / v->ipg, idx = (num - 1) % v->ipg;
    if (g >= v->grupos) return false;   /* v8.5.4.2: nunca un descriptor que no existe */
    uint32_t byte = idx * v->tam_inode;
    *b = gd_tabela(v, g) + byte / v->bloco;
    *off = byte % v->bloco;
    return true;
}

static int inode_ler(e2_t *v, uint32_t num, inode_t *i)
{
    uint32_t b, off;
    if (!inode_onde(v, num, &b, &off)) return ERRO_NAO_EXISTE;
    uint8_t *x = bloco(v, b);
    if (!x) return ERRO_ACESSO;
    if (!esta_sujo(v, b) && !inode_soma_ok(v, num, x + off)) {
        klog("[EXT2] %s: el inodo %u tiene la suma mal\n", v->d->nome, num);
        return erro_es(v);
    }
    memset(i->b, 0, sizeof i->b);
    memcpy(i->b, x + off, v->tam_inode < 256 ? v->tam_inode : 256);
    i->num = num;
    return 0;
}

static int inode_gravar(e2_t *v, inode_t *i)
{
    uint32_t b, off;
    if (v->so_leitura || !inode_onde(v, i->num, &b, &off)) return ERRO_ACESSO;
    uint8_t *x = bloco(v, b);
    if (!x) return ERRO_ACESSO;
    memcpy(x + off, i->b, v->tam_inode < 256 ? v->tam_inode : 256);
    selar_inode(v, i->num, x + off);
    sujar(v, b);
    return 0;
}

static uint32_t extra_isize(e2_t *v, const inode_t *i) { return v->tam_inode > 128 ? le16(i->b + 128) : 0; }

static void inode_tempos(e2_t *v, inode_t *i, bool acesso, bool mudanca)
{
    uint32_t t = agora();
    bool extra = extra_isize(v, i) >= 16;   /* i_ctime_extra, i_mtime_extra, i_atime_extra */
    if (acesso) { st32(i->b + 8, t); if (extra) st32(i->b + 0x8C, 0); }
    if (mudanca) {
        st32(i->b + 12, t); st32(i->b + 16, t);
        if (extra) { st32(i->b + 0x84, 0); st32(i->b + 0x88, 0); }
    }
}

static void somar_setores(e2_t *v, inode_t *i, int blocos)
{
    st32(i->b + 28, I_SETORES(i) + (uint32_t)(blocos * (int)v->spb));
}

static bool tem_blocos(const inode_t *i)
{
    /* Enlace simbólico corto: el destino vive en i_block, no hay bloques. */
    if ((I_MODO(i) & MODO_TIPO) == MODO_LIGACAO && I_TAM(i) < 60 && (I_SETORES(i) == 0 || I_ACL(i))) return false;
    if (I_FLAGS(i) & FL_INLINE) return false;
    uint16_t t = I_MODO(i) & MODO_TIPO;
    return t == MODO_ARQUIVO || t == MODO_PASTA || t == MODO_LIGACAO;
}

static uint32_t semente_de(e2_t *v, const inode_t *i) { return semente_inode(v, i->num, I_GERACAO(i)); }

/* ------------------------------------------------------------ extents (ext4) */

/* Comprueba un bloque del árbol leído del disco (cabecera y suma). */
static bool ext_bloco_ok(e2_t *v, uint32_t b, const uint8_t *x, uint32_t semente)
{
    if (le16(x) != EXT_MAGIA || le16(x + 2) > le16(x + 4) || 12 + 12 * (uint32_t)le16(x + 4) > v->bloco) return false;
    if (!v->csum || esta_sujo(v, b)) return true;
    uint32_t o = ext_cauda_off(v, x);
    return o && le32(x + o) == crc32c(semente, x, o);
}

/* Árbol de extents: el bloque físico de "lb" (0 = agujero o no inicializado). */
static uint32_t mapear_extent(e2_t *v, const inode_t *ino, uint32_t lb, bool *nao_inic)
{
    const uint8_t *no = ino->b + 40;
    if (nao_inic) *nao_inic = false;
    for (int nivel = 0; nivel < 6; nivel++) {
        if (le16(no) != 0xF30A) { erro_es(v); return 0; }
        uint32_t qtd = le16(no + 2), prof = le16(no + 6);
        const uint8_t *e = no + 12;
        if (prof == 0) {
            for (uint32_t k = 0; k < qtd; k++, e += 12) {
                uint32_t ini = le32(e), len = le16(e + 4);
                bool ni = len > 32768;
                if (ni) len -= 32768;
                if (lb >= ini && lb - ini < len) {
                    if (le16(e + 6)) return 0;
                    if (ni) { if (nao_inic) *nao_inic = true; return 0; }
                    return le32(e + 8) + (lb - ini);
                }
            }
            return 0;
        }
        const uint8_t *esc = NULL;
        for (uint32_t k = 0; k < qtd; k++, e += 12)
            if (le32(e) <= lb) esc = e;
        if (!esc || le16(esc + 8)) return 0;
        uint32_t b = le32(esc + 4);
        no = bloco(v, b);
        if (!no) return 0;
        if (!ext_bloco_ok(v, b, no, semente_de(v, ino))) {
            klog("[EXT2] %s: bloque de extents %u del inodo %u no válido\n", v->d->nome, b, ino->num);
            erro_es(v);
            return 0;
        }
    }
    return 0;
}

typedef struct { uint32_t lb, len, pb; bool ni; } ext_t;
typedef struct {
    ext_t    *e;    uint32_t n, cap;
    uint32_t *arv;  uint32_t narv, cap_arv;    /* bloques del árbol (fuera del inodo) */
} ext_lista_t;

static void ext_lista_liberar(ext_lista_t *l)
{
    if (l->e) kfree(l->e);
    if (l->arv) kfree(l->arv);
    memset(l, 0, sizeof *l);
}

static int ext_por(ext_lista_t *l, ext_t x)
{
    if (l->n == l->cap) {
        uint32_t cap = l->cap ? l->cap * 2 : 16;
        if (cap > MAX_EXTENTS) return ERRO_LIMITE;
        ext_t *n = kmalloc(cap * sizeof *n);
        if (!n) return ERRO_SEM_MEMORIA;
        if (l->e) { memcpy(n, l->e, l->n * sizeof *n); kfree(l->e); }
        l->e = n;
        l->cap = cap;
    }
    l->e[l->n++] = x;
    return 0;
}

static int arv_por(ext_lista_t *l, uint32_t b)
{
    if (l->narv == l->cap_arv) {
        uint32_t cap = l->cap_arv ? l->cap_arv * 2 : 8;
        uint32_t *n = kmalloc(cap * sizeof *n);
        if (!n) return ERRO_SEM_MEMORIA;
        if (l->arv) { memcpy(n, l->arv, l->narv * sizeof *n); kfree(l->arv); }
        l->arv = n;
        l->cap_arv = cap;
    }
    l->arv[l->narv++] = b;
    return 0;
}

/* Recorre un nodo (copiado: la caché puede cambiar en la recursión). */
static int ext_coletar_no(e2_t *v, const inode_t *ino, const uint8_t *no, int prof_esperada, ext_lista_t *l)
{
    if (le16(no) != EXT_MAGIA) return ERRO_ACESSO;
    uint32_t qtd = le16(no + 2), prof = le16(no + 6);
    if (prof_esperada >= 0 && prof != (uint32_t)prof_esperada) return ERRO_ACESSO;
    if (prof > 5) return ERRO_ACESSO;
    const uint8_t *e = no + 12;
    for (uint32_t k = 0; k < qtd; k++, e += 12) {
        int r;
        if (prof == 0) {
            ext_t x;
            x.lb = le32(e);
            x.len = le16(e + 4);
            x.ni = x.len > EXT_MAX_LEN;
            if (x.ni) x.len -= EXT_MAX_LEN;
            if (le16(e + 6)) return ERRO_LIMITE;     /* bloques de más de 32 bits */
            x.pb = le32(e + 8);
            if (!x.len) continue;
            if (l->n && l->e[l->n - 1].lb + l->e[l->n - 1].len > x.lb) return ERRO_ACESSO;   /* desordenado */
            r = ext_por(l, x);
        } else {
            if (le16(e + 8)) return ERRO_LIMITE;
            uint32_t b = le32(e + 4);
            if ((r = arv_por(l, b)) < 0) return r;
            uint8_t *c = kmalloc(v->bloco);
            if (!c) return ERRO_SEM_MEMORIA;
            uint8_t *x = bloco(v, b);
            if (!x || !ext_bloco_ok(v, b, x, semente_de(v, ino))) { kfree(c); return ERRO_ACESSO; }
            memcpy(c, x, v->bloco);
            r = ext_coletar_no(v, ino, c, (int)prof - 1, l);
            kfree(c);
        }
        if (r < 0) return r;
    }
    return 0;
}

static int ext_coletar(e2_t *v, const inode_t *ino, ext_lista_t *l)
{
    memset(l, 0, sizeof *l);
    int r = ext_coletar_no(v, ino, ino->b + 40, -1, l);
    if (r < 0) {
        klog("[EXT2] %s: árbol de extents del inodo %u no se puede modificar (%d)\n", v->d->nome, ino->num, r);
        ext_lista_liberar(l);
        if (r == ERRO_ACESSO) erro_es(v);
    }
    return r;
}

static void ext_cabecalho(uint8_t *p, uint32_t qtd, uint32_t max, uint32_t prof)
{
    st16(p, EXT_MAGIA);
    st16(p + 2, qtd);
    st16(p + 4, max);
    st16(p + 6, prof);
    st32(p + 8, 0);
}

static void ext_entrada(uint8_t *p, const ext_t *x)
{
    st32(p, x->lb);
    st16(p + 4, x->len + (x->ni ? EXT_MAX_LEN : 0));
    st16(p + 6, 0);
    st32(p + 8, x->pb);
}

/* Reescribe el árbol entero a partir de la lista (ordenada) de extents. Reutiliza los
 * bloques del árbol viejo, pide los que falten y libera los que sobren. */
static int ext_reconstruir(e2_t *v, inode_t *ino, ext_lista_t *l)
{
    uint32_t cap = (v->bloco - 12) / 12;           /* entradas por bloque (la cola de la suma cabe detrás) */
    uint32_t sem = semente_de(v, ino);
    uint32_t usados = 0;                            /* bloques del árbol viejo ya reutilizados */
    /* Nivel actual: n elementos; en el nivel 0 son los extents, después (lb, bloque). */
    uint32_t n = l->n, prof = 0;
    uint32_t *idx_lb = NULL, *idx_b = NULL;
    int r = 0;
    while (n > 4) {
        uint32_t nb = (n + cap - 1) / cap;
        uint32_t *nlb = kmalloc(nb * sizeof *nlb), *nbl = kmalloc(nb * sizeof *nbl);
        if (!nlb || !nbl) { if (nlb) kfree(nlb); if (nbl) kfree(nbl); r = ERRO_SEM_MEMORIA; break; }
        for (uint32_t k = 0; k < nb; k++) {
            uint32_t b;
            if (usados < l->narv) b = l->arv[usados++];
            else {
                b = alocar_bloco(v, prof == 0 ? l->e[0].pb : idx_b[0]);
                if (!b) { r = ERRO_SEM_MEMORIA; break; }
                somar_setores(v, ino, 1);
            }
            uint8_t *x = bloco_novo(v, b);
            if (!x) { r = ERRO_ACESSO; break; }
            uint32_t ini = k * cap, fim = ini + cap < n ? ini + cap : n;
            ext_cabecalho(x, fim - ini, cap, prof);
            for (uint32_t m = ini; m < fim; m++) {
                uint8_t *p = x + 12 + (m - ini) * 12;
                if (prof == 0) ext_entrada(p, &l->e[m]);
                else { st32(p, idx_lb[m]); st32(p + 4, idx_b[m]); st32(p + 8, 0); }
            }
            sujar_t(v, b, TB_EXTENT, sem);
            nlb[k] = prof == 0 ? l->e[ini].lb : idx_lb[ini];
            nbl[k] = b;
        }
        if (idx_lb) { kfree(idx_lb); kfree(idx_b); }
        idx_lb = nlb;
        idx_b = nbl;
        if (r < 0) break;
        n = nb;
        prof++;
    }
    if (r == 0) {
        uint8_t *raiz = ino->b + 40;
        memset(raiz, 0, 60);
        ext_cabecalho(raiz, n, 4, prof);
        for (uint32_t m = 0; m < n; m++) {
            uint8_t *p = raiz + 12 + m * 12;
            if (prof == 0) ext_entrada(p, &l->e[m]);
            else { st32(p, idx_lb[m]); st32(p + 4, idx_b[m]); st32(p + 8, 0); }
        }
        /* Los bloques del árbol viejo que sobran. */
        for (uint32_t k = usados; k < l->narv; k++) { liberar_bloco(v, l->arv[k]); somar_setores(v, ino, -1); }
    }
    if (idx_lb) { kfree(idx_lb); kfree(idx_b); }
    return r;
}

/* Inserta (lb, pb, 1) en la lista, uniéndolo a un vecino si es contiguo. lb no estaba. */
static int ext_inserir(ext_lista_t *l, uint32_t lb, uint32_t pb)
{
    uint32_t k = 0;
    while (k < l->n && l->e[k].lb < lb) k++;
    if (k > 0) {
        ext_t *a = &l->e[k - 1];
        if (!a->ni && a->lb + a->len == lb && a->pb + a->len == pb && a->len < EXT_MAX_LEN) {
            a->len++;
            if (k < l->n) {   /* ¿se junta también con el siguiente? */
                ext_t *s = &l->e[k];
                if (!s->ni && s->lb == lb + 1 && s->pb == pb + 1 && a->len + s->len <= EXT_MAX_LEN) {
                    a->len += s->len;
                    memmove(s, s + 1, (l->n - k - 1) * sizeof *s);
                    l->n--;
                }
            }
            return 0;
        }
    }
    if (k < l->n) {
        ext_t *s = &l->e[k];
        if (!s->ni && s->lb == lb + 1 && s->pb == pb + 1 && s->len < EXT_MAX_LEN) { s->lb--; s->pb--; s->len++; return 0; }
    }
    ext_t x = { lb, 1, pb, false };
    int r = ext_por(l, x);
    if (r < 0) return r;
    memmove(&l->e[k + 1], &l->e[k], (l->n - 1 - k) * sizeof x);
    l->e[k] = x;
    return 0;
}

/* Parte el extent que contiene lb para que [lb] quede solo (para convertir un bloque no
 * inicializado). Devuelve el índice del extent [lb] o <0. */
static int ext_isolar(ext_lista_t *l, uint32_t lb)
{
    uint32_t k = 0;
    while (k < l->n && !(lb >= l->e[k].lb && lb - l->e[k].lb < l->e[k].len)) k++;
    if (k == l->n) return ERRO_ACESSO;
    ext_t e = l->e[k];
    ext_t partes[3];
    int np = 0;
    if (lb > e.lb) { partes[np] = e; partes[np].len = lb - e.lb; np++; }
    int meio = np;
    partes[np].lb = lb; partes[np].len = 1; partes[np].pb = e.pb + (lb - e.lb); partes[np].ni = e.ni; np++;
    if (lb + 1 < e.lb + e.len) { partes[np] = e; partes[np].lb = lb + 1; partes[np].pb = e.pb + (lb + 1 - e.lb); partes[np].len = e.lb + e.len - lb - 1; np++; }
    uint32_t antes = l->n;
    for (int a = 1; a < np; a++) {
        int r = ext_por(l, e);
        if (r < 0) return r;
    }
    memmove(&l->e[k + (uint32_t)np], &l->e[k + 1], (antes - k - 1) * sizeof e);
    for (int a = 0; a < np; a++) l->e[k + (uint32_t)a] = partes[a];
    return (int)k + meio;
}

/* Caso rápido de la escritura secuencial: el último extent del archivo termina en lb - 1 y
 * el bloque pb es el siguiente en el disco: se alarga en su sitio. */
static bool ext_alongar(e2_t *v, inode_t *ino, uint32_t lb, uint32_t pb)
{
    uint8_t *no = ino->b + 40;
    uint32_t b = 0;
    for (int nivel = 0; nivel < 6; nivel++) {
        if (le16(no) != EXT_MAGIA) return false;
        uint32_t qtd = le16(no + 2), prof = le16(no + 6);
        if (!qtd) return false;
        uint8_t *e = no + 12 + (qtd - 1) * 12;
        if (prof == 0) {
            uint32_t len = le16(e + 4);
            if (len >= EXT_MAX_LEN || le16(e + 6) || le32(e) + len != lb || le32(e + 8) + len != pb) return false;
            st16(e + 4, len + 1);
            if (b) sujar_t(v, b, TB_EXTENT, semente_de(v, ino));
            return true;
        }
        if (le16(e + 8)) return false;
        b = le32(e + 4);
        no = bloco(v, b);
        if (!no || !ext_bloco_ok(v, b, no, semente_de(v, ino))) return false;
    }
    return false;
}

/* El bloque físico siguiente al de lb - 1 (para pedir el nuevo cerca). */
static uint32_t ext_perto(e2_t *v, inode_t *ino, uint32_t lb)
{
    if (lb == 0) return 0;
    uint32_t p = mapear_extent(v, ino, lb - 1, NULL);
    return p ? p + 1 : 0;
}

static uint32_t mapear_extent_alocar(e2_t *v, inode_t *ino, uint32_t lb, bool *novo)
{
    bool ni;
    uint32_t p = mapear_extent(v, ino, lb, &ni);
    if (p || v->erro) return p;
    ext_lista_t l;
    if (ni) {
        /* Bloque reservado sin inicializar (fallocate): pasa a inicializado; quien llama lo
         * rellena entero (para el lector, hasta ahora, eran ceros). */
        if (ext_coletar(v, ino, &l) < 0) return 0;
        int k = ext_isolar(&l, lb);
        uint32_t pb = 0;
        if (k >= 0) {
            l.e[k].ni = false;
            pb = l.e[k].pb;
            if (ext_reconstruir(v, ino, &l) < 0) pb = 0;
        }
        ext_lista_liberar(&l);
        if (pb && novo) *novo = true;
        return pb;
    }
    uint32_t perto = ext_perto(v, ino, lb);
    if (!perto) perto = v->primeiro_bloco + ((ino->num - 1) / v->ipg) * v->bpg;
    uint32_t pb = alocar_bloco(v, perto);
    if (!pb) return 0;
    somar_setores(v, ino, 1);
    if (!ext_alongar(v, ino, lb, pb)) {
        if (ext_coletar(v, ino, &l) < 0 || ext_inserir(&l, lb, pb) < 0 || ext_reconstruir(v, ino, &l) < 0) {
            ext_lista_liberar(&l);
            liberar_bloco(v, pb);
            somar_setores(v, ino, -1);
            erro_es(v);
            return 0;
        }
        ext_lista_liberar(&l);
    }
    if (novo) *novo = true;
    return pb;
}

/* Quita del archivo los bloques lógicos >= corte (libera los datos y lo que sobre del árbol). */
static int ext_cortar(e2_t *v, inode_t *ino, uint32_t corte)
{
    ext_lista_t l;
    int r = ext_coletar(v, ino, &l);
    if (r < 0) return r;
    uint32_t m = 0;
    for (uint32_t k = 0; k < l.n; k++) {
        ext_t *e = &l.e[k];
        uint32_t fica = corte <= e->lb ? 0 : (corte - e->lb < e->len ? corte - e->lb : e->len);
        for (uint32_t b = fica; b < e->len; b++) { liberar_bloco(v, e->pb + b); somar_setores(v, ino, -1); }
        if (fica) { e->len = fica; l.e[m++] = *e; }
    }
    l.n = m;
    r = ext_reconstruir(v, ino, &l);
    ext_lista_liberar(&l);
    return r;
}

static void ext_iniciar(inode_t *ino)
{
    memset(ino->b + 40, 0, 60);
    ext_cabecalho(ino->b + 40, 0, 4, 0);
    st32(ino->b + 32, I_FLAGS(ino) | FL_EXTENTS);
}

/* ------------------------------------------------------------ bloques de un archivo */

static uint32_t potencia(uint32_t p, int n) { uint32_t r = 1; while (n-- > 0) r *= p; return r; }

/* Bloque físico del bloque lógico "lb". Con "alocar", crea lo que falte
 * (bloques indirectos en cero); *novo indica si el bloque de datos es nuevo (el
 * que llama lo rellena). 0 = agujero o fallo. */
static uint32_t mapear(e2_t *v, inode_t *ino, uint32_t lb, bool alocar, bool *novo)
{
    if (novo) *novo = false;
    if (I_FLAGS(ino) & FL_EXTENTS) {
        if (!alocar) return mapear_extent(v, ino, lb, NULL);
        return v->extents ? mapear_extent_alocar(v, ino, lb, novo) : 0;
    }
    uint32_t *blk = I_BLOCO(ino);
    uint32_t P = v->bloco / 4;
    uint32_t perto = v->primeiro_bloco + ((ino->num - 1) / v->ipg) * v->bpg;
    if (v->dica > perto && v->dica - perto < v->bpg) perto = v->dica;
    if (lb < 12) {
        if (!blk[lb] && alocar) {
            blk[lb] = alocar_bloco(v, perto);
            if (blk[lb]) { somar_setores(v, ino, 1); if (novo) *novo = true; }
        }
        return blk[lb];
    }
    lb -= 12;
    int nivel;
    uint32_t *topo;
    if (lb < P) { nivel = 1; topo = &blk[12]; }
    else if ((lb -= P) < P * P) { nivel = 2; topo = &blk[13]; }
    else { lb -= P * P; nivel = 3; topo = &blk[14]; if ((uint64_t)lb >= (uint64_t)P * P * P) return 0; }
    if (!*topo) {
        if (!alocar) return 0;
        uint32_t b = alocar_bloco(v, perto);
        if (!b || !bloco_novo(v, b)) return 0;
        somar_setores(v, ino, 1);
        *topo = b;
    }
    uint32_t atual = *topo;
    for (int n = nivel; n >= 1; n--) {
        uint32_t div = potencia(P, n - 1), idx = lb / div;
        lb %= div;
        uint8_t *x = bloco(v, atual);
        if (!x) return 0;
        uint32_t prox = le32(x + idx * 4);
        if (!prox) {
            if (!alocar) return 0;
            prox = alocar_bloco(v, atual + 1);
            if (!prox) return 0;
            somar_setores(v, ino, 1);
            if (n > 1) { if (!bloco_novo(v, prox)) return 0; }
            else if (novo) *novo = true;
            x = bloco(v, atual);   /* la asignación pudo sacar el bloque de la caché */
            if (!x) return 0;
            st32(x + idx * 4, prox);
            sujar(v, atual);
        }
        atual = prox;
    }
    return atual;
}

/* Libera el bloque b y todo lo que cuelga de él (nivel 0 = bloque de datos). */
static void liberar_ramo(e2_t *v, uint32_t b, int nivel, inode_t *ino)
{
    if (!b) return;
    if (nivel > 0) {
        uint32_t P = v->bloco / 4;
        uint32_t *copia = kmalloc(v->bloco);
        uint8_t *x = copia ? bloco(v, b) : NULL;
        if (x) {
            memcpy(copia, x, v->bloco);
            for (uint32_t k = 0; k < P; k++) liberar_ramo(v, copia[k], nivel - 1, ino);
        }
        if (copia) kfree(copia);
    }
    liberar_bloco(v, b);
    somar_setores(v, ino, -1);
}

/* Mantiene los bloques lógicos < corte de la rama que empieza en "base". */
static void cortar(e2_t *v, uint32_t *ptr, int nivel, uint64_t base, uint64_t corte, inode_t *ino)
{
    if (!*ptr) return;
    if (corte <= base) { liberar_ramo(v, *ptr, nivel, ino); *ptr = 0; return; }
    if (nivel == 0) return;
    uint32_t P = v->bloco / 4;
    uint64_t passo = potencia(P, nivel - 1);
    if (corte >= base + passo * P) return;
    uint32_t *copia = kmalloc(v->bloco);
    uint8_t *x = copia ? bloco(v, *ptr) : NULL;
    if (!x) { if (copia) kfree(copia); return; }
    memcpy(copia, x, v->bloco);
    bool mudou = false;
    for (uint32_t k = 0; k < P; k++) {
        uint32_t antes = copia[k];
        cortar(v, &copia[k], nivel - 1, base + k * passo, corte, ino);
        if (copia[k] != antes) mudou = true;
    }
    if (mudou && (x = bloco(v, *ptr))) { memcpy(x, copia, v->bloco); sujar(v, *ptr); }
    kfree(copia);
}

/* Cambia el tamaño de un archivo (el inodo en memoria; quien llama graba). */
static void truncar_inode(e2_t *v, inode_t *ino, uint32_t tam)
{
    if (tem_blocos(ino) && tam < I_TAM(ino)) {
        uint64_t corte = tam / v->bloco + (tam % v->bloco ? 1 : 0);
        if (I_FLAGS(ino) & FL_EXTENTS) {
            if (v->extents) ext_cortar(v, ino, (uint32_t)corte);
        } else {
            uint32_t P = v->bloco / 4;
            uint32_t *blk = I_BLOCO(ino);
            for (uint32_t k = 0; k < 12; k++)
                if (k >= corte && blk[k]) { liberar_bloco(v, blk[k]); somar_setores(v, ino, -1); blk[k] = 0; }
            cortar(v, &blk[12], 1, 12, corte, ino);
            cortar(v, &blk[13], 2, 12 + P, corte, ino);
            cortar(v, &blk[14], 3, 12 + P + (uint64_t)P * P, corte, ino);
        }
        /* El resto del último bloque queda a cero (el archivo puede volver a crecer). */
        if (tam % v->bloco) {
            uint32_t fis = mapear(v, ino, tam / v->bloco, false, NULL);
            uint8_t *x = fis ? bloco(v, fis) : NULL;
            if (x) { memset(x + tam % v->bloco, 0, v->bloco - tam % v->bloco); sujar(v, fis); }
        }
    }
    st32(ino->b + 4, tam);
    if (EH_ARQUIVO(ino)) st32(ino->b + 108, 0);   /* tamaño alto: archivos de hasta 4 GB - 1 */
}

/* Libera un inodo sin enlaces: bloques, bloque de atributos y mapa de bits. */
static void liberar_inode(e2_t *v, inode_t *ino)
{
    bool pasta = EH_PASTA(ino);
    truncar_inode(v, ino, 0);
    uint32_t acl = I_ACL(ino);
    if (acl) {
        uint8_t *x = bloco(v, acl);
        if (x && le32(x) == 0xEA020000 && le32(x + 4) > 1) {
            /* Bloque de atributos compartido: una referencia menos (y su suma, con metadata_csum). */
            st32(x + 4, le32(x + 4) - 1);
            if (v->csum) {
                uint8_t nb[8] = { 0 };
                st32(nb, acl);
                st32(x + 0x10, 0);
                st32(x + 0x10, crc32c(crc32c(v->semente, nb, 8), x, v->bloco));
            }
            sujar(v, acl);
        } else if (x) { liberar_bloco(v, acl); somar_setores(v, ino, -1); }
        st32(ino->b + 104, 0);
    }
    st16(ino->b + 26, 0);
    st32(ino->b + 20, agora());   /* dtime */
    inode_gravar(v, ino);
    liberar_inode_bitmap(v, ino->num, pasta);
}

/* ------------------------------------------------------------ nomes */

static int nome_para_disco(const char *s, uint8_t *out, int max)
{
    int n = 0;
    for (; *s; s++) {
        uint8_t c = (uint8_t)*s;
        if (c < 0x80) { if (n + 1 > max) return -1; out[n++] = c; }
        else { if (n + 2 > max) return -1; out[n++] = (uint8_t)(0xC0 | (c >> 6)); out[n++] = (uint8_t)(0x80 | (c & 0x3F)); }
    }
    return n;
}

/* UTF-8 de 2 bytes que cabe en Latin-1 pasa a 1 byte; el resto queda como está. */
static void nome_do_disco(const uint8_t *s, uint32_t n, char *out, int max)
{
    int k = 0;
    for (uint32_t i = 0; i < n && k < max - 1; i++) {
        if ((s[i] == 0xC2 || s[i] == 0xC3) && i + 1 < n && (s[i + 1] & 0xC0) == 0x80) {
            out[k++] = (char)(((s[i] & 0x03) << 6) | (s[i + 1] & 0x3F));
            i++;
        } else out[k++] = s[i] ? (char)s[i] : '_';
    }
    out[k] = 0;
}

/* ------------------------------------------------------------ pastas */

typedef struct { uint32_t lb, desl, ant; } dcursor_t;
typedef struct {
    uint32_t ino, fis, desl, rec_len, ant;
    uint8_t  tipo, nome_len;
    uint8_t  nome[256];
} dentrada_t;

/* Comprueba la suma de un bloque de carpeta (si tiene la cola; los del índice htree no). */
static bool pasta_bloco_ok(e2_t *v, inode_t *dir, uint32_t fis, const uint8_t *x)
{
    if (!v->csum || esta_sujo(v, fis) || !tem_cauda(v, x)) return true;
    return le32(x + v->bloco - 4) == crc32c(semente_de(v, dir), x, v->bloco - 12);
}

/* Siguiente entrada usada de la carpeta: 1 encontrada, 0 terminó, <0 error. */
static int dir_ler(e2_t *v, inode_t *dir, dcursor_t *c, dentrada_t *e)
{
    uint32_t nblocos = (I_TAM(dir) + v->bloco - 1) / v->bloco;
    while (c->lb < nblocos) {
        if (c->desl + 8 > v->bloco) { c->lb++; c->desl = 0; c->ant = 0xFFFFFFFFu; continue; }
        uint32_t fis = mapear(v, dir, c->lb, false, NULL);
        if (!fis) { if (v->erro) return ERRO_ACESSO; c->lb++; c->desl = 0; c->ant = 0xFFFFFFFFu; continue; }
        uint8_t *x = bloco(v, fis);
        if (!x) return ERRO_ACESSO;
        if (c->desl == 0 && !pasta_bloco_ok(v, dir, fis, x)) {
            klog("[EXT2] %s: bloque %u de la carpeta %u con la suma mal\n", v->d->nome, fis, dir->num);
            return erro_es(v);
        }
        uint8_t *p = x + c->desl;
        uint32_t ino = le32(p), rec = le16(p + 4);
        uint32_t nl = v->tipo_dir ? p[6] : le16(p + 6);
        if (rec < 8 || rec % 4 || c->desl + rec > v->bloco || nl + 8 > rec || nl > 255) {
            c->desl = v->bloco;   /* bloque estropeado: salta el resto */
            continue;
        }
        e->ino = ino; e->fis = fis; e->desl = c->desl; e->rec_len = rec; e->ant = c->ant;
        e->tipo = v->tipo_dir ? p[7] : 0;
        e->nome_len = (uint8_t)nl;
        memcpy(e->nome, p + 8, nl);
        e->nome[nl] = 0;
        c->ant = c->desl;
        c->desl += rec;
        if (ino) return 1;
    }
    return 0;
}

static bool eh_ponto(const dentrada_t *e)
{
    return (e->nome_len == 1 && e->nome[0] == '.') || (e->nome_len == 2 && e->nome[0] == '.' && e->nome[1] == '.');
}

/* Busca "nome" (Latin-1) en la carpeta; primero como UTF-8, luego los bytes crudos. */
static int dir_buscar(e2_t *v, inode_t *dir, const char *nome, size_t n, dentrada_t *e)
{
    char tmp[NOME_MAX];
    if (n >= NOME_MAX) return ERRO_NAO_EXISTE;
    memcpy(tmp, nome, n);
    tmp[n] = 0;
    uint8_t u[256];
    int nu = nome_para_disco(tmp, u, 255);
    for (int volta = 0; volta < 2; volta++) {
        const uint8_t *alvo = volta == 0 ? u : (const uint8_t *)tmp;
        uint32_t na = volta == 0 ? (uint32_t)nu : (uint32_t)n;
        if (volta == 1 && na == (uint32_t)nu && memcmp(u, tmp, n) == 0) break;   /* sólo ASCII: ya buscó */
        dcursor_t c = { 0, 0, 0xFFFFFFFFu };
        int r;
        while ((r = dir_ler(v, dir, &c, e)) == 1)
            if (e->nome_len == na && memcmp(e->nome, alvo, na) == 0) return 0;
        if (r < 0) return r;
    }
    return ERRO_NAO_EXISTE;
}

/* Sigue una ruta relativa ("" = raíz). */
static int achar(e2_t *v, const char *rel, inode_t *ino)
{
    int r = inode_ler(v, RAIZ, ino);
    if (r < 0) return r;
    const char *p = rel;
    while (*p == '/') p++;
    for (int guarda = 0; *p && guarda < 64; guarda++) {
        const char *fim = p;
        while (*fim && *fim != '/') fim++;
        if (!EH_PASTA(ino)) return ERRO_NAO_EXISTE;
        dentrada_t e;
        r = dir_buscar(v, ino, p, (size_t)(fim - p), &e);
        if (r < 0) return r;
        r = inode_ler(v, e.ino, ino);
        if (r < 0) return r;
        p = *fim ? fim + 1 : fim;
    }
    return 0;
}

/* "a/b/c" -> inodo de la carpeta "a/b" y el nombre "c". */
static int separar(e2_t *v, const char *rel, inode_t *pasta, const char **nome)
{
    const char *barra = NULL;
    for (const char *p = rel; *p; p++) if (*p == '/') barra = p;
    char tmp[CAMINHO_MAX];
    if (barra) {
        size_t n = (size_t)(barra - rel);
        if (n >= sizeof tmp) return ERRO_INVALIDO;
        memcpy(tmp, rel, n);
        tmp[n] = 0;
        *nome = barra + 1;
    } else {
        tmp[0] = 0;
        *nome = rel;
    }
    if (!**nome || txt_tamanho(*nome) >= NOME_MAX) return ERRO_INVALIDO;
    int r = achar(v, tmp, pasta);
    if (r < 0) return r;
    return EH_PASTA(pasta) ? 0 : ERRO_NAO_E_PASTA;
}

static uint8_t tipo_dir_de(const inode_t *i)
{
    switch (I_MODO(i) & MODO_TIPO) {
    case MODO_ARQUIVO: return 1;
    case MODO_PASTA:   return 2;
    case 0x2000:       return 3;
    case 0x6000:       return 4;
    case 0x1000:       return 5;
    case 0xC000:       return 6;
    case MODO_LIGACAO: return 7;
    default:           return 0;
    }
}

static void gravar_entrada(e2_t *v, uint8_t *p, uint32_t ino, uint32_t rec, const uint8_t *nome, uint32_t nl, uint8_t tipo)
{
    st32(p, ino);
    st16(p + 4, rec);
    if (v->tipo_dir) { p[6] = (uint8_t)nl; p[7] = tipo; }
    else st16(p + 6, nl);
    memcpy(p + 8, nome, nl);
}

/* Un bloque de carpeta vacío: una entrada libre que lo ocupa todo y, con metadata_csum, la cola. */
static void bloco_pasta_vazio(e2_t *v, uint8_t *x)
{
    memset(x, 0, v->bloco);
    st16(x + 4, v->bloco - cauda(v));
    if (v->csum) por_cauda(v, x);
}

/* Una carpeta con índice htree que se va a modificar como lista pierde el índice: los
 * bloques del índice (la raíz y, con dos niveles, los nodos) pasan a ser bloques de
 * entradas vacíos y válidos; las hojas ya lo son. Linux la lee después como lista y el
 * e2fsck -D puede volver a indexarla. */
static int desindexar(e2_t *v, inode_t *dir)
{
    if (!(I_FLAGS(dir) & FL_INDICE)) return 0;
    uint32_t raiz = mapear(v, dir, 0, false, NULL);
    uint8_t *x = raiz ? bloco(v, raiz) : NULL;
    if (!x) return ERRO_ACESSO;
    uint32_t niveis = x[0x1E], qtd = le16(x + 0x22), sem = semente_de(v, dir);
    uint32_t nos[512], nn = 0;
    if (niveis == 1 && qtd <= 512 && le16(x + 0x20) >= qtd) {
        for (uint32_t k = 0; k < qtd; k++) nos[nn++] = le32(x + 0x20 + k * 8 + 4);
    } else if (niveis > 1) return ERRO_ACESSO;          /* largedir: no se modifica */
    /* La raíz: "." y ".." se quedan; ".." ocupa el resto del bloque. */
    if (le16(x + 4) != 12) return ERRO_ACESSO;
    st16(x + 12 + 4, v->bloco - 12 - cauda(v));
    memset(x + 0x18, 0, v->bloco - 0x18);
    if (v->csum) por_cauda(v, x);
    sujar_t(v, raiz, TB_PASTA, sem);
    for (uint32_t k = 0; k < nn; k++) {
        uint32_t fis = mapear(v, dir, nos[k], false, NULL);
        uint8_t *y = fis ? bloco(v, fis) : NULL;
        if (!y) return ERRO_ACESSO;
        bloco_pasta_vazio(v, y);
        sujar_t(v, fis, TB_PASTA, sem);
    }
    st32(dir->b + 32, I_FLAGS(dir) & ~(uint32_t)FL_INDICE);
    return 0;
}

/* Añade una entrada en la carpeta "num_dir" (relee el inodo: puede haber cambiado). */
static int dir_adicionar(e2_t *v, uint32_t num_dir, const char *nome_l1, uint32_t ino, uint8_t tipo)
{
    inode_t dir;
    int r = inode_ler(v, num_dir, &dir);
    if (r < 0) return r;
    uint8_t nome[256];
    int nl = nome_para_disco(nome_l1, nome, 255);
    if (nl <= 0) return ERRO_INVALIDO;
    if ((r = desindexar(v, &dir)) < 0) return r;
    uint32_t sem = semente_de(v, &dir);
    uint32_t precisa = (8 + (uint32_t)nl + 3) & ~3u;
    uint32_t nblocos = I_TAM(&dir) / v->bloco, util = v->bloco - cauda(v);
    for (uint32_t lb = 0; lb < nblocos; lb++) {
        uint32_t fis = mapear(v, &dir, lb, false, NULL);
        if (!fis) { if (v->erro) return ERRO_ACESSO; continue; }
        uint8_t *x = bloco(v, fis);
        if (!x) return ERRO_ACESSO;
        if (v->csum && !tem_cauda(v, x)) continue;    /* bloque sin sitio para la suma: no se toca */
        for (uint32_t d = 0; d + 8 <= util; ) {
            uint8_t *p = x + d;
            uint32_t rec = le16(p + 4), eino = le32(p);
            uint32_t enl = v->tipo_dir ? p[6] : le16(p + 6);
            if (rec < 8 || rec % 4 || d + rec > util) break;
            uint32_t real = eino ? ((8 + enl + 3) & ~3u) : 0;
            if (rec >= real && rec - real >= precisa) {
                if (eino) {           /* divide la entrada: ella se queda con lo que usa */
                    st16(p + 4, real);
                    p += real;
                    rec -= real;
                }
                gravar_entrada(v, p, ino, rec, nome, (uint32_t)nl, tipo);
                sujar_t(v, fis, TB_PASTA, sem);
                goto feito;
            }
            d += rec;
        }
    }
    {   /* Sin espacio: un bloque más al final de la carpeta. */
        bool novo;
        uint32_t fis = mapear(v, &dir, nblocos, true, &novo);
        if (!fis) return v->erro ? ERRO_ACESSO : ERRO_SEM_MEMORIA;
        uint8_t *x = bloco_novo(v, fis);
        if (!x) return ERRO_ACESSO;
        bloco_pasta_vazio(v, x);
        gravar_entrada(v, x, ino, util, nome, (uint32_t)nl, tipo);
        sujar_t(v, fis, TB_PASTA, sem);
        st32(dir.b + 4, (nblocos + 1) * v->bloco);
    }
feito:
    inode_tempos(v, &dir, false, true);
    return inode_gravar(v, &dir);
}

/* Quita la entrada "nome" de la carpeta; devuelve el inodo al que apuntaba. */
static int dir_remover(e2_t *v, uint32_t num_dir, const char *nome, uint32_t *ino)
{
    inode_t dir;
    int r = inode_ler(v, num_dir, &dir);
    if (r < 0) return r;
    if ((r = desindexar(v, &dir)) < 0) return r;
    dentrada_t e;
    r = dir_buscar(v, &dir, nome, txt_tamanho(nome), &e);
    if (r < 0) return r;
    uint8_t *x = bloco(v, e.fis);
    if (!x) return ERRO_ACESSO;
    if (e.ant != 0xFFFFFFFFu) {   /* une el espacio a la entrada anterior */
        uint8_t *a = x + e.ant;
        st16(a + 4, le16(a + 4) + e.rec_len);
    } else {
        st32(x + e.desl, 0);
    }
    sujar_t(v, e.fis, TB_PASTA, semente_de(v, &dir));
    *ino = e.ino;
    inode_tempos(v, &dir, false, true);
    return inode_gravar(v, &dir);
}

static bool pasta_vazia(e2_t *v, inode_t *dir)
{
    dcursor_t c = { 0, 0, 0xFFFFFFFFu };
    dentrada_t e;
    while (dir_ler(v, dir, &c, &e) == 1)
        if (!eh_ponto(&e)) return false;
    return true;
}

static void ligacoes(e2_t *v, uint32_t num, int delta)
{
    inode_t i;
    if (inode_ler(v, num, &i) < 0) return;
    uint32_t l = I_LIGACOES(&i);
    if (delta < 0 && l == 0) return;
    st16(i.b + 26, l + (uint32_t)delta);
    inode_tempos(v, &i, false, true);
    inode_gravar(v, &i);
}

/* ------------------------------------------------------------ archivos abiertos */

static int aberto(e2_t *v, uint32_t ino)
{
    for (int k = 0; k < MAX_ABERTOS; k++) if (v->abertos[k].refs && v->abertos[k].ino == ino) return k;
    return -1;
}

static int segurar(e2_t *v, uint32_t ino)
{
    int k = aberto(v, ino);
    if (k >= 0) { v->abertos[k].refs++; return 0; }
    for (k = 0; k < MAX_ABERTOS; k++)
        if (!v->abertos[k].refs) { v->abertos[k].ino = ino; v->abertos[k].refs = 1; v->abertos[k].orfao = false; return 0; }
    return ERRO_LIMITE;
}

/* Disminuye los enlaces de un inodo quitado de una carpeta; sin ninguno, libera
 * (o espera al último cierre, si está abierto). */
static void desligar_inode(e2_t *v, uint32_t num)
{
    inode_t i;
    if (inode_ler(v, num, &i) < 0) return;
    uint32_t l = I_LIGACOES(&i);
    if (l > 1 && !EH_PASTA(&i)) { st16(i.b + 26, l - 1); inode_tempos(v, &i, false, true); inode_gravar(v, &i); return; }
    int k = aberto(v, num);
    if (k >= 0) { v->abertos[k].orfao = true; return; }
    liberar_inode(v, &i);
}

/* ¿Se puede modificar el contenido de este inodo? (extents sólo si el volumen los sabe
 * escribir; nunca datos en el propio inodo). */
static bool modificavel(e2_t *v, const inode_t *i)
{
    if (I_FLAGS(i) & FL_INLINE) return false;
    if ((I_FLAGS(i) & FL_EXTENTS) && !v->extents) return false;
    return true;
}

/* ------------------------------------------------------------ operaciones */

/* Las escrituras de datos seguidas (un cp, por ejemplo) no graban los metadatos en
 * cada trozo: como mucho una vez por segundo y siempre al cerrar el archivo.
 * Los datos de bloques enteros ya fueron directo al disco. */
static int terminar_dados(e2_t *v, int r)
{
    if (relogio_tiques() - v->ultima_gravacao >= TIQUES_POR_SEGUNDO) return terminar(v, r);
    if (v->erro) { if (r >= 0) r = v->erro; v->erro = 0; }
    return r;
}

static int e2_info(void *priv, const char *rel, info_arquivo_t *inf)
{
    e2_t *v = priv;
    inode_t i;
    int r = achar(v, rel, &i);
    if (r < 0) return terminar_dados(v, r);
    inf->tipo = EH_PASTA(&i) ? TIPO_PASTA : (I_MODO(&i) & 0111) ? TIPO_PROGRAMA : TIPO_ARQUIVO;   /* D379: como e2_listar */
    inf->tamanho = EH_PASTA(&i) ? 0 : I_TAM(&i);
    return terminar_dados(v, 0);
}

/* D282: dueño, grupo y permisos del inodo (sin setuid ni setgid: SaguiOS no los usa). */
static int e2_meta(void *priv, const char *rel, meta_t *m)
{
    e2_t *v = priv;
    inode_t i;
    int r = achar(v, rel, &i);
    if (r < 0) return terminar_dados(v, r);
    m->uid = I_UID(&i);
    m->gid = I_GID(&i);
    m->modo = (uint16_t)(I_MODO(&i) & MODO_PERMISOS);
    m->tipo = EH_PASTA(&i) ? TIPO_PASTA : TIPO_ARQUIVO;
    return terminar_dados(v, 0);
}

static int e2_fixar_meta(void *priv, const char *rel, int32_t uid, int32_t gid, int32_t modo)
{
    e2_t *v = priv;
    if (v->so_leitura) return ERRO_SO_LEITURA;
    inode_t i;
    int r = achar(v, rel, &i);
    if (r < 0) return terminar(v, r);
    if (uid >= 0) { st16(i.b + 2, (uint32_t)uid & 0xFFFF); st16(i.b + 120, (uint32_t)uid >> 16); }
    if (gid >= 0) { st16(i.b + 24, (uint32_t)gid & 0xFFFF); st16(i.b + 122, (uint32_t)gid >> 16); }
    if (modo >= 0) st16(i.b + 0, (I_MODO(&i) & ~(uint32_t)07777) | ((uint32_t)modo & MODO_PERMISOS));
    st32(i.b + 12, agora());   /* i_ctime: cambió el inodo */
    if (extra_isize(v, &i) >= 8) st32(i.b + 0x84, 0);
    return terminar(v, inode_gravar(v, &i));
}

/* D341: una fecha del inodo y, si el inodo es grande (256 bytes) y tiene su campo _extra, los bits
 * de época de ese campo (sin nanosegundos: la resolución es de un segundo). Pasado 2038 el campo de
 * 32 bits, que es con signo, se queda negativo y la época 1 le suma 2^32. */
static void inode_fixar_tempo(e2_t *v, inode_t *i, uint32_t pos, uint32_t pos_extra, uint32_t t)
{
    st32(i->b + pos, t);
    if (extra_isize(v, i) >= pos_extra + 4 - 128) st32(i->b + pos_extra, t >= 0x80000000u ? 1 : 0);
}

/* D341: fija mtime y atime (0 = no cambia); ctime pasa a ser ahora, como en POSIX. Con diario, el
 * bloque del inodo va en la transacción como cualquier otro metadato (terminar). */
static int e2_fixar_datas(void *priv, const char *rel, uint32_t mod, uint32_t acesso)
{
    e2_t *v = priv;
    if (v->so_leitura) return ERRO_SO_LEITURA;
    inode_t i;
    int r = achar(v, rel, &i);
    if (r < 0) return terminar(v, r);
    if (mod) inode_fixar_tempo(v, &i, 16, 0x88, mod);      /* i_mtime, i_mtime_extra */
    if (acesso) inode_fixar_tempo(v, &i, 8, 0x8C, acesso); /* i_atime, i_atime_extra */
    inode_fixar_tempo(v, &i, 12, 0x84, agora());           /* i_ctime, i_ctime_extra */
    return terminar(v, inode_gravar(v, &i));
}

static int e2_datas(void *priv, const char *rel, datas_arquivo_t *d)

{
    e2_t *v = priv;
    inode_t i;
    int r = achar(v, rel, &i);
    if (r < 0) return terminar_dados(v, r);
    d->acessado = le32(i.b + 8);
    d->criado = le32(i.b + 12);      /* i_ctime: el último cambio del inodo (el ext2 no guarda la creación) */
    d->modificado = le32(i.b + 16);
    return terminar_dados(v, 0);
}

static int e2_listar(void *priv, const char *rel, int indice, entrada_pasta_t *saida)
{
    e2_t *v = priv;
    inode_t dir;
    int r = achar(v, rel, &dir);
    if (r < 0) return terminar_dados(v, r);
    if (!EH_PASTA(&dir)) return terminar_dados(v, ERRO_NAO_E_PASTA);
    dcursor_t c = { 0, 0, 0xFFFFFFFFu };
    int k = 0;
    if (indice > 0 && v->lst_ino == dir.num && v->lst_indice + 1 == (uint32_t)indice) {
        c.lb = v->lst_lb; c.desl = v->lst_desl; k = indice;   /* continúa desde donde paró */
    }
    dentrada_t e;
    while ((r = dir_ler(v, &dir, &c, &e)) == 1) {
        if (eh_ponto(&e)) continue;
        if (k++ != indice) continue;
        nome_do_disco(e.nome, e.nome_len, saida->nome, NOME_MAX);
        inode_t f;
        if (inode_ler(v, e.ino, &f) == 0) {
            /* D379 (M1): un archivo con permiso de ejecución se lista como programa, como en el ramdisk
             * (allí, por ser un ELF): «ls» y Archivos lo marcan igual con la raíz en disco. */
            saida->tipo = EH_PASTA(&f) ? TIPO_PASTA : (I_MODO(&f) & 0111) ? TIPO_PROGRAMA : TIPO_ARQUIVO;
            saida->tamanho = EH_PASTA(&f) ? 0 : I_TAM(&f);
        } else {
            v->erro = 0;
            saida->tipo = e.tipo == 2 ? TIPO_PASTA : TIPO_ARQUIVO;
            saida->tamanho = 0;
        }
        v->lst_ino = dir.num; v->lst_indice = (uint32_t)indice; v->lst_lb = c.lb; v->lst_desl = c.desl;
        return terminar_dados(v, 1);
    }
    v->lst_ino = 0;
    return terminar_dados(v, r < 0 ? r : 0);
}

/* Crea un inodo nuevo (archivo o carpeta) y la entrada en la carpeta padre. */
static int criar(e2_t *v, inode_t *pai, const char *nome, bool pasta, inode_t *novo)
{
    uint32_t g = (pai->num - 1) / v->ipg;
    uint32_t num = alocar_inode(v, g, pasta);
    if (!num) return v->erro ? ERRO_ACESSO : ERRO_SEM_MEMORIA;
    uint32_t b, off;
    inode_onde(v, num, &b, &off);
    uint8_t *x = bloco(v, b);
    if (!x) return ERRO_ACESSO;
    memset(x + off, 0, v->tam_inode);   /* incluye el área extra (atributos antiguos) */
    sujar(v, b);
    memset(novo, 0, sizeof *novo);
    novo->num = num;
    /* D282: los de quien crea (su uid, su gid y su umask). */
    uint32_t uid, gid;
    uint16_t perm;
    permisos_dono_novo(pasta, &uid, &gid, &perm);
    st16(novo->b + 0, (pasta ? MODO_PASTA : MODO_ARQUIVO) | (perm & MODO_PERMISOS));
    st16(novo->b + 2, uid & 0xFFFF);
    st16(novo->b + 120, uid >> 16);
    st16(novo->b + 24, gid & 0xFFFF);
    st16(novo->b + 122, gid >> 16);
    st16(novo->b + 26, pasta ? 2 : 1);
    st32(novo->b + 100, agora() * 2654435761u + ++v->geracao);   /* i_generation */
    if (v->tam_inode > 128) st16(novo->b + 128, v->tam_inode >= 160 ? 32 : 0);   /* i_extra_isize */
    inode_tempos(v, novo, true, true);
    if (extra_isize(v, novo) >= 24) st32(novo->b + 0x90, agora());           /* i_crtime */
    if (v->extents) ext_iniciar(novo);
    if (pasta) {
        bool n;
        uint32_t fis = mapear(v, novo, 0, true, &n);
        uint8_t *d = fis ? bloco_novo(v, fis) : NULL;
        if (!d) { liberar_inode_bitmap(v, num, true); return fis ? ERRO_ACESSO : ERRO_SEM_MEMORIA; }
        bloco_pasta_vazio(v, d);
        gravar_entrada(v, d, num, 12, (const uint8_t *)".", 1, 2);
        gravar_entrada(v, d + 12, pai->num, v->bloco - 12 - cauda(v), (const uint8_t *)"..", 2, 2);
        sujar_t(v, fis, TB_PASTA, semente_de(v, novo));
        st32(novo->b + 4, v->bloco);
    }
    int r = inode_gravar(v, novo);
    if (r == 0) r = dir_adicionar(v, pai->num, nome, num, pasta ? 2 : 1);
    if (r < 0) { liberar_inode(v, novo); return r; }
    if (pasta) ligacoes(v, pai->num, +1);
    return 0;
}

static int e2_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tamanho)
{
    e2_t *v = priv;
    e2_arq_t *f = arq;
    inode_t i;
    v->lst_ino = 0;
    int r = achar(v, rel, &i);
    bool criou = false;
    if (r == ERRO_NAO_EXISTE && (modo & ABRIR_CRIAR)) {
        if (v->so_leitura) return terminar(v, ERRO_SO_LEITURA);
        inode_t pai;
        const char *nome;
        r = separar(v, rel, &pai, &nome);
        if (r == 0 && !modificavel(v, &pai)) r = ERRO_ACESSO;
        if (r == 0) r = criar(v, &pai, nome, false, &i);
        criou = r == 0;
    }
    if (r < 0) return terminar(v, r);
    if (EH_PASTA(&i)) return terminar(v, ERRO_E_PASTA);
    if ((modo & ABRIR_ESCREVER) && (!EH_ARQUIVO(&i) || !modificavel(v, &i))) return terminar(v, ERRO_ACESSO);
    if ((modo & ABRIR_TRUNCAR) && I_TAM(&i)) {
        if (v->so_leitura) return terminar(v, ERRO_SO_LEITURA);
        truncar_inode(v, &i, 0);
        inode_tempos(v, &i, false, true);
        inode_gravar(v, &i);
    }
    r = segurar(v, i.num);
    if (r < 0) return terminar(v, r);
    f->ino = i.num;
    *tamanho = I_TAM(&i);
    return terminar(v, criou ? 1 : 0);   /* D282: 1 = lo creó esta llamada (con el dueño de quien llama) */
}

static int e2_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{
    e2_t *v = priv;
    e2_arq_t *f = arq;
    inode_t i;
    int r = inode_ler(v, f->ino, &i);
    if (r < 0) return terminar_dados(v, r);
    uint32_t tam = I_TAM(&i);
    if (pos >= tam) return terminar_dados(v, 0);
    if (n > tam - pos) n = tam - pos;
    uint8_t *dest = buf;
    if (!tem_blocos(&i)) {   /* enlace corto o datos en el propio inodo */
        uint32_t cabe = pos < 60 ? 60 - pos : 0;
        if (n > cabe) n = cabe;
        memcpy(dest, i.b + 40 + pos, n);
        return terminar_dados(v, (int)n);
    }
    uint32_t feito = 0;
    while (feito < n) {
        uint32_t p = pos + feito, lb = p / v->bloco, off = p % v->bloco;
        uint32_t parte = v->bloco - off;
        if (parte > n - feito) parte = n - feito;
        uint32_t fis = mapear(v, &i, lb, false, NULL);
        if (v->erro) break;
        if (fis && off == 0 && parte == v->bloco) {
            /* Bloques enteros seguidos en el disco: una sola lectura. */
            uint32_t qtd = 1;
            while (qtd < 64 && feito + (qtd + 1) * v->bloco <= n && mapear(v, &i, lb + qtd, false, NULL) == fis + qtd) qtd++;
            if (ler_direto(v, fis, qtd, dest + feito) < 0) break;
            feito += qtd * v->bloco;
            continue;
        }
        if (!fis) memset(dest + feito, 0, parte);
        else {
            uint8_t *x = bloco(v, fis);
            if (!x) break;
            memcpy(dest + feito, x + off, parte);
        }
        feito += parte;
    }
    r = terminar_dados(v, 0);
    return feito ? (int)feito : r;
}

static int e2_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tamanho)
{
    e2_t *v = priv;
    e2_arq_t *f = arq;
    if (v->so_leitura) return ERRO_SO_LEITURA;
    if (n == 0) return 0;
    if (pos > 0xFFFFFFFFu - n) return ERRO_LIMITE;   /* hasta 4 GB - 1 */
    inode_t i;
    int r = inode_ler(v, f->ino, &i);
    if (r < 0) return terminar(v, r);
    if (!modificavel(v, &i)) return terminar(v, ERRO_ACESSO);
    const uint8_t *orig = buf;
    uint32_t feito = 0;
    while (feito < n) {
        uint32_t p = pos + feito, lb = p / v->bloco, off = p % v->bloco;
        uint32_t parte = v->bloco - off;
        if (parte > n - feito) parte = n - feito;
        bool novo;
        uint32_t fis = mapear(v, &i, lb, true, &novo);
        if (!fis) { if (!v->erro) r = ERRO_SEM_MEMORIA; break; }
        if (parte == v->bloco) {
            if (gravar_direto(v, fis, orig + feito) < 0) break;
        } else {
            uint8_t *x = novo ? bloco_novo(v, fis) : bloco(v, fis);
            if (!x) break;
            memcpy(x + off, orig + feito, parte);
            sujar(v, fis);
        }
        feito += parte;
    }
    if (pos + feito > I_TAM(&i)) {
        st32(i.b + 4, pos + feito);
        if (pos + feito >= 0x80000000u && !(v->ro_compat & RO_ARQ_GRANDE)) {
            v->ro_compat |= RO_ARQ_GRANDE;
            st32(v->sb + 100, v->ro_compat);
            v->sb_sujo = true;
        }
    }
    inode_tempos(v, &i, false, true);
    inode_gravar(v, &i);
    *tamanho = I_TAM(&i);
    r = terminar_dados(v, r);
    return feito ? (int)feito : (r < 0 ? r : 0);
}

static int e2_truncar(void *priv, void *arq, uint32_t tam)
{
    e2_t *v = priv;
    e2_arq_t *f = arq;
    if (v->so_leitura) return ERRO_SO_LEITURA;
    inode_t i;
    int r = inode_ler(v, f->ino, &i);
    if (r < 0) return terminar(v, r);
    if (!modificavel(v, &i)) return terminar(v, ERRO_ACESSO);
    truncar_inode(v, &i, tam);
    inode_tempos(v, &i, false, true);
    return terminar(v, inode_gravar(v, &i));
}

static void e2_fechar(void *priv, void *arq)
{
    e2_t *v = priv;
    e2_arq_t *f = arq;
    int k = aberto(v, f->ino);
    if (k < 0) return;
    if (--v->abertos[k].refs == 0 && v->abertos[k].orfao && !v->so_leitura) {
        inode_t i;
        if (inode_ler(v, f->ino, &i) == 0) liberar_inode(v, &i);
    }
    terminar(v, 0);
}

static int e2_criar_pasta(void *priv, const char *rel)
{
    e2_t *v = priv;
    if (v->so_leitura) return ERRO_SO_LEITURA;
    v->lst_ino = 0;
    inode_t i, pai;
    if (achar(v, rel, &i) == 0) return terminar(v, ERRO_JA_EXISTE);
    v->erro = 0;
    const char *nome;
    int r = separar(v, rel, &pai, &nome);
    if (r == 0 && !modificavel(v, &pai)) r = ERRO_ACESSO;
    if (r == 0) r = criar(v, &pai, nome, true, &i);
    return terminar(v, r);
}

static int e2_remover_pasta(void *priv, const char *rel)
{
    e2_t *v = priv;
    if (v->so_leitura) return ERRO_SO_LEITURA;
    v->lst_ino = 0;
    inode_t i, pai;
    const char *nome;
    if (!*rel) return terminar(v, ERRO_ACESSO);
    int r = achar(v, rel, &i);
    if (r < 0) return terminar(v, r);
    if (!EH_PASTA(&i)) return terminar(v, ERRO_NAO_E_PASTA);
    if (!modificavel(v, &i)) return terminar(v, ERRO_ACESSO);
    if (!pasta_vazia(v, &i)) return terminar(v, ERRO_NAO_VAZIA);
    r = separar(v, rel, &pai, &nome);
    if (r == 0 && !modificavel(v, &pai)) r = ERRO_ACESSO;
    uint32_t ino;
    if (r == 0) r = dir_remover(v, pai.num, nome, &ino);
    if (r < 0) return terminar(v, r);
    if (inode_ler(v, i.num, &i) == 0) liberar_inode(v, &i);
    ligacoes(v, pai.num, -1);
    return terminar(v, 0);
}

static int e2_apagar(void *priv, const char *rel)
{
    e2_t *v = priv;
    if (v->so_leitura) return ERRO_SO_LEITURA;
    v->lst_ino = 0;
    inode_t i, pai;
    const char *nome;
    int r = achar(v, rel, &i);
    if (r < 0) return terminar(v, r);
    if (EH_PASTA(&i)) return terminar(v, ERRO_E_PASTA);
    if (!modificavel(v, &i)) return terminar(v, ERRO_ACESSO);
    r = separar(v, rel, &pai, &nome);
    if (r == 0 && !modificavel(v, &pai)) r = ERRO_ACESSO;
    uint32_t ino;
    if (r == 0) r = dir_remover(v, pai.num, nome, &ino);
    if (r == 0) desligar_inode(v, ino);
    return terminar(v, r);
}

static int e2_renomear(void *priv, const char *de, const char *para)
{
    e2_t *v = priv;
    if (v->so_leitura) return ERRO_SO_LEITURA;
    v->lst_ino = 0;
    inode_t src, alvo, pai_de, pai_para;
    const char *nome_de, *nome_para;
    if (!*de) return terminar(v, ERRO_ACESSO);
    int r = achar(v, de, &src);
    if (r < 0) return terminar(v, r);
    char final[CAMINHO_MAX];
    txt_copia_n(final, para, sizeof final);
    bool substituir = false;
    if (achar(v, para, &alvo) == 0) {
        if (EH_PASTA(&alvo)) {                         /* mover adentro de la carpeta */
            size_t n = txt_tamanho(final);
            if (n) final[n++] = '/';
            const char *base = de;
            for (const char *p = de; *p; p++) if (*p == '/') base = p + 1;
            txt_copia_n(final + n, base, sizeof final - n);
            if (achar(v, final, &alvo) == 0) return terminar(v, alvo.num == src.num ? 0 : ERRO_JA_EXISTE);
        } else {
            if (EH_PASTA(&src)) return terminar(v, ERRO_JA_EXISTE);
            if (alvo.num == src.num) return terminar(v, 0);
            substituir = true;
        }
    }
    v->erro = 0;
    r = separar(v, de, &pai_de, &nome_de);
    if (r == 0) r = separar(v, final, &pai_para, &nome_para);
    if (r < 0) return terminar(v, r);
    if (!modificavel(v, &pai_de) || !modificavel(v, &pai_para) || (EH_PASTA(&src) && !modificavel(v, &src)))
        return terminar(v, ERRO_ACESSO);
    /* Una carpeta no puede ir dentro de sí misma. */
    if (EH_PASTA(&src)) {
        inode_t p = pai_para;
        for (int guarda = 0; p.num != RAIZ && guarda < 256; guarda++) {
            if (p.num == src.num) return terminar(v, ERRO_INVALIDO);
            dentrada_t e;
            if (dir_buscar(v, &p, "..", 2, &e) < 0 || inode_ler(v, e.ino, &p) < 0) break;
        }
    }
    uint32_t ino;
    if (substituir) {
        r = dir_remover(v, pai_para.num, nome_para, &ino);
        if (r == 0) desligar_inode(v, ino);
        if (r < 0) return terminar(v, r);
    }
    r = dir_adicionar(v, pai_para.num, nome_para, src.num, tipo_dir_de(&src));
    if (r < 0) return terminar(v, r);
    r = dir_remover(v, pai_de.num, nome_de, &ino);
    if (r < 0) return terminar(v, r);
    if (EH_PASTA(&src) && pai_de.num != pai_para.num) {
        /* El ".." de la carpeta movida apunta al nuevo padre. */
        inode_t d;
        dentrada_t e;
        if (inode_ler(v, src.num, &d) == 0 && dir_buscar(v, &d, "..", 2, &e) == 0) {
            uint8_t *x = bloco(v, e.fis);
            if (x) { st32(x + e.desl, pai_para.num); sujar_t(v, e.fis, TB_PASTA, semente_de(v, &d)); }
        }
        ligacoes(v, pai_de.num, -1);
        ligacoes(v, pai_para.num, +1);
    }
    if (inode_ler(v, src.num, &src) == 0) {
        st32(src.b + 12, agora());
        if (extra_isize(v, &src) >= 8) st32(src.b + 0x84, 0);
        inode_gravar(v, &src);
    }
    return terminar(v, 0);
}

static void e2_espaco(void *priv, uint32_t *total_kb, uint32_t *livre_kb)
{
    e2_t *v = priv;
    *total_kb = v->blocos * (v->bloco / 1024);
    *livre_kb = le32(v->sb + 12) * (v->bloco / 1024);
}

/* ------------------------------------------------------------ diario: enlace con jbd2.c */

static int j_ler(void *ctx, uint32_t fis, uint8_t *buf)
{
    e2_t *v = ctx;
    if (fis == 0 || fis >= v->blocos) return -1;
    return disco_ler(v->d, fis * v->spb, v->spb, buf) < 0 ? -1 : 0;
}

static int j_escrever(void *ctx, uint32_t fis, const uint8_t *buf)
{
    e2_t *v = ctx;
    if (fis >= v->blocos || (fis == 0 && v->primeiro_bloco == 0)) return -1;   /* el bloque 0 lleva el superbloque */
    return disco_escrever(v->d, fis * v->spb, v->spb, buf) < 0 ? -1 : 0;
}

static int j_sincronizar(void *ctx)
{
    e2_t *v = ctx;
    return disco_sincronizar(v->d) < 0 ? -1 : 0;
}

static uint32_t j_mapear(void *ctx, uint32_t lb)
{
    e2_t *v = ctx;
    for (uint32_t k = 0; k < v->jmapa_n; k++)
        if (lb >= v->jmapa[k].lb && lb - v->jmapa[k].lb < v->jmapa[k].n) return v->jmapa[k].pb + (lb - v->jmapa[k].lb);
    return 0;
}

/* Abre el diario interno (inodo s_journal_inum): su mapa de bloques en tramos y su
 * superbloque. NULL = bien; si no, el motivo. */
static const char *diario_abrir(e2_t *v)
{
    uint32_t num = le32(v->sb + 0xE0);
    if (!num) return "diario externo no soportado";
    inode_t ji;
    if (inode_ler(v, num, &ji) < 0) { v->erro = 0; return "no se puede leer el inodo del diario"; }
    uint32_t n = I_TAM(&ji) / v->bloco;
    if (n < 64) return "diario demasiado pequeño";
    v->jmapa_n = 0;
    for (uint32_t lb = 0; lb < n; lb++) {
        uint32_t pb = mapear(v, &ji, lb, false, NULL);
        if (!pb || v->erro) { v->erro = 0; return "el diario tiene agujeros"; }
        if (v->jmapa_n && v->jmapa[v->jmapa_n - 1].pb + v->jmapa[v->jmapa_n - 1].n == pb) { v->jmapa[v->jmapa_n - 1].n++; continue; }
        if (v->jmapa_n == sizeof v->jmapa / sizeof v->jmapa[0]) return "diario demasiado fragmentado";
        v->jmapa[v->jmapa_n].lb = lb;
        v->jmapa[v->jmapa_n].pb = pb;
        v->jmapa[v->jmapa_n].n = 1;
        v->jmapa_n++;
    }
    memset(&v->j, 0, sizeof v->j);
    v->j.ctx = v;
    v->j.ler = j_ler;
    v->j.escrever = j_escrever;
    v->j.sincronizar = j_sincronizar;
    v->j.mapear = j_mapear;
    v->j.bloco = v->bloco;
    v->j.blocos_inode = n;
    if (jbd2_abrir(&v->j) < 0) return v->j.motivo ? v->j.motivo : "diario no válido";
    return NULL;
}

/* ------------------------------------------------------------ montaje */

static void liberar(e2_t *v)
{
    for (int i = 0; i < CACHE_MAX; i++) if (v->cache[i].dados) kfree(v->cache[i].dados);
    if (v->gd) kfree(v->gd);
    if (v->gdb_sujo) kfree(v->gdb_sujo);
    if (v->grupo) kfree(v->grupo);
    if (v->tx_fis) kfree(v->tx_fis);
    if (v->tx_dados) kfree(v->tx_dados);
    if (v->liberados) kfree(v->liberados);
    jbd2_liberar(&v->j);
    kfree(v);
}

static int recusar(e2_t *v, disco_t *d, const char *motivo, uint32_t valor)
{
    klog("[EXT2] %s: %s (%x)\n", d->nome, motivo, valor);
    disco_avisar(AVISO_INFO, "%s: %s", d->nome, motivo);
    liberar(v);
    return ERRO_FORMATO;
}

/* El nombre de la primera característica que impide escribir (NULL = ninguna). */
static const char *caracteristica_nao_suportada(e2_t *v)
{
    static const struct { uint32_t bit; uint8_t qual; const char *nome; } t[] = {
        { INCOMPAT_INLINE, 1, "inline_data" },   { INCOMPAT_CIFRADO, 1, "encrypt" },
        { INCOMPAT_CASEFOLD, 1, "casefold" },    { INCOMPAT_MMP, 1, "mmp" },
        { INCOMPAT_EA_INODE, 1, "ea_inode" },    { INCOMPAT_LARGEDIR, 1, "large_dir" },
        { INCOMPAT_META_BG, 1, "meta_bg" },      { INCOMPAT_DIARIO_DEV, 1, "journal_dev" },
        { INCOMPAT_COMPRESSAO, 1, "compression" }, { INCOMPAT_DIRDATA, 1, "dirdata" },
        { RO_BIGALLOC, 2, "bigalloc" },          { 0x0100, 2, "quota" },
        { 0x2000, 2, "project" },                { 0x1000, 2, "readonly" },
        { 0x8000, 2, "verity" },                 { 0x10000, 2, "orphan_present" },
        { 0x4000, 2, "shared_blocks" },
    };
    for (uint32_t k = 0; k < sizeof t / sizeof t[0]; k++)
        if ((t[k].qual == 1 ? v->incompat : v->ro_compat) & t[k].bit) return t[k].nome;
    if (v->incompat & ~(uint32_t)INCOMPAT_ESCRITA) return "incompat desconocida";
    if (v->ro_compat & ~(uint32_t)RO_ESCRITA) return "ro_compat desconocida";
    return NULL;
}

static int ler_descritores(e2_t *v)
{
    return disco_ler(v->d, (v->primeiro_bloco + 1) * v->spb, v->gd_blocos * v->spb, v->gd) < 0 ? ERRO_ACESSO : 0;
}

/* ------------------------------------------------------------ fsck básico (v8.5.4.2, D379 F)
 *
 * Un ext2 que no se desmontó bien (s_state sin el bit de limpio: corte de luz, pendrive arrancado) puede
 * tener los mapas de bits y los contadores desfasados de lo que de verdad usan los inodos: lo peligroso
 * es un bloque en uso marcado como libre (se volvería a asignar y dos archivos compartirían datos). Antes
 * de montarlo con escritura se rehacen, como los pasos 1 y 5 de e2fsck:
 *   - se recorren todos los inodos en uso (enlaces > 0, sin fecha de borrado) y sus bloques (directos,
 *     indirectos simple/doble/triple y el de atributos), más los metadatos de cada grupo (copias del
 *     superbloque y de los descriptores, los reservados para crecer, los mapas y la tabla de inodos);
 *   - los mapas de bloques y de inodos se dejan como sale de eso, y los contadores de cada grupo (libres,
 *     carpetas) y del superbloque.
 * No mira los directorios ni los contadores de enlaces (lo que e2fsck hace en los pasos 2-4): eso no
 * produce asignaciones dobles. Solo para ext2 «puro» (sin diario, extents, sumas, 64 bits ni flex_bg);
 * con algo raro (un bloque fuera del volumen o usado dos veces, extents en un inodo) no se toca nada y
 * se monta de solo lectura. Devuelve NULL si lo hizo, o el motivo. */
static bool fsck_elegivel(e2_t *v)
{
    return !v->csum && !v->gdt_csum && !(v->compat & COMPAT_DIARIO) && !gd64(v) &&
           !(v->incompat & ~(uint32_t)INCOMPAT_TIPO_DIR) &&
           !(v->ro_compat & ~(uint32_t)(RO_SPARSE | RO_ARQ_GRANDE | RO_EXTRA_ISIZE));
}

typedef struct { uint8_t *uso; uint8_t *buf[3]; const char *falha; } fsck_t;

static bool fsck_marcar(e2_t *v, fsck_t *f, uint32_t b)
{
    if (b < v->primeiro_bloco || b >= v->blocos) { f->falha = "un inodo apunta fuera del volumen"; return false; }
    uint32_t k = b - v->primeiro_bloco;
    if (f->uso[k / 8] & (1 << (k % 8))) { f->falha = "un bloque lo usan dos sitios"; return false; }
    f->uso[k / 8] |= (uint8_t)(1 << (k % 8));
    return true;
}

static bool fsck_arvore(e2_t *v, fsck_t *f, uint32_t b, int nivel)
{
    if (!b) return true;
    if (!fsck_marcar(v, f, b)) return false;
    if (!nivel) return true;
    uint8_t *x = bloco(v, b);
    if (!x) { f->falha = "error de lectura"; return false; }
    uint8_t *meu = f->buf[nivel - 1];   /* copia: la caché puede soltar el bloque al bajar */
    memcpy(meu, x, v->bloco);
    for (uint32_t i = 0; i < v->bloco / 4; i++)
        if (!fsck_arvore(v, f, le32(meu + i * 4), nivel - 1)) return false;
    return true;
}

static const char *fsck_basico(e2_t *v, uint32_t *mudancas)
{
    *mudancas = 0;
    if (!fsck_elegivel(v)) return "sus características no las comprueba SaguiOS";
    if ((uint64_t)v->grupos * v->ipg != v->inodes) return "la cuenta de inodos no cuadra con los grupos";
    fsck_t f = { 0 };
    uint32_t nb = v->blocos - v->primeiro_bloco;
    f.uso = kcalloc((nb + 7) / 8);
    uint8_t *ino_uso = kcalloc((v->inodes + 7) / 8);
    uint32_t *pastas = kcalloc(v->grupos * sizeof(uint32_t));
    for (int i = 0; i < 3; i++) f.buf[i] = kmalloc(v->bloco);
    if (!f.uso || !ino_uso || !pastas || !f.buf[0] || !f.buf[1] || !f.buf[2]) { f.falha = "sin memoria"; goto fim; }
    /* Metadatos de cada grupo (lo mismo que iniciar_mapa_blocos). */
    uint32_t tab = (v->ipg * v->tam_inode + v->bloco - 1) / v->bloco;
    for (uint32_t g = 0; g < v->grupos && !f.falha; g++) {
        uint32_t base = v->primeiro_bloco + g * v->bpg;
        if (tem_super(v, g))
            for (uint32_t k = 0; k < 1 + v->gd_blocos + le16(v->sb + 0xCE) && !f.falha; k++) fsck_marcar(v, &f, base + k);
        if (!f.falha) fsck_marcar(v, &f, gd_mapa_blocos(v, g));
        if (!f.falha) fsck_marcar(v, &f, gd_mapa_inodes(v, g));
        for (uint32_t k = 0; k < tab && !f.falha; k++) fsck_marcar(v, &f, gd_tabela(v, g) + k);
    }
    /* Los inodos en uso y sus bloques. */
    inode_t in;
    for (uint32_t n = 1; n <= v->inodes && !f.falha; n++) {
        bool reservado = n < v->primeiro_ino;
        if (inode_ler(v, n, &in) < 0) { f.falha = "error de lectura de un inodo"; break; }
        bool vivo = I_MODO(&in) && I_LIGACOES(&in) > 0 && le32(in.b + 20) == 0;
        if (!vivo && !reservado) continue;
        ino_uso[(n - 1) / 8] |= (uint8_t)(1 << ((n - 1) % 8));
        if (n == 1) {   /* los bloques malos: los suyos están en uso aunque el inodo no tenga modo */
            const uint32_t *b = I_BLOCO(&in);
            for (int k = 0; k < 12 && !f.falha; k++) if (b[k]) fsck_marcar(v, &f, b[k]);
            if (!f.falha) fsck_arvore(v, &f, b[12], 1);
            if (!f.falha) fsck_arvore(v, &f, b[13], 2);
            if (!f.falha) fsck_arvore(v, &f, b[14], 3);
            continue;
        }
        if (!vivo) continue;
        if (n == 7) {   /* resize_inode: su bloque doble (los reservados ya van con los metadatos del grupo) */
            if (I_BLOCO(&in)[13] && !fsck_marcar(v, &f, I_BLOCO(&in)[13])) break;
            continue;
        }
        if (EH_PASTA(&in) && (n - 1) / v->ipg < v->grupos) pastas[(n - 1) / v->ipg]++;
        if (I_FLAGS(&in) & 0x80000) { f.falha = "un inodo con extents"; break; }
        if (I_ACL(&in) && !fsck_marcar(v, &f, I_ACL(&in))) break;
        bool simbolico_curto = (I_MODO(&in) & 0xF000) == 0xA000 && I_TAM(&in) < 60 && I_SETORES(&in) == (I_ACL(&in) ? v->spb : 0);
        if (simbolico_curto) continue;
        /* solo archivos, carpetas y enlaces simbólicos largos tienen bloques: en un dispositivo, una tubería o
         * un socket, i_block guarda otra cosa (el número del dispositivo) */
        uint16_t tipo = I_MODO(&in) & 0xF000;
        if (tipo != 0x8000 && tipo != 0x4000 && tipo != 0xA000) continue;
        const uint32_t *b = I_BLOCO(&in);
        for (int k = 0; k < 12 && !f.falha; k++) if (b[k]) fsck_marcar(v, &f, b[k]);
        if (!f.falha) fsck_arvore(v, &f, b[12], 1);
        if (!f.falha) fsck_arvore(v, &f, b[13], 2);
        if (!f.falha) fsck_arvore(v, &f, b[14], 3);
    }
    if (f.falha) goto fim;
    /* Los mapas y los contadores. */
    uint32_t livres_b = 0, livres_i = 0;
    for (uint32_t g = 0; g < v->grupos; g++) {
        uint8_t *m = mapa(v, g, false);
        if (!m) { f.falha = "error de lectura de un mapa"; goto fim; }
        uint32_t qtd = blocos_no_grupo(v, g), usados = 0, base = g * v->bpg;
        bool mudou = false;
        for (uint32_t bit = 0; bit < qtd; bit++) {
            uint32_t k = base + bit;
            bool u = f.uso[k / 8] & (1 << (k % 8)), em = m[bit / 8] & (1 << (bit % 8));
            if (u) usados++;
            if (u == em) continue;
            if (u) m[bit / 8] |= (uint8_t)(1 << (bit % 8)); else m[bit / 8] &= (uint8_t)~(1 << (bit % 8));
            mudou = true;
            (*mudancas)++;
        }
        if (mudou) { sujar(v, gd_mapa_blocos(v, g)); v->grupo[g] |= GR_MB; }
        uint8_t *mi = mapa(v, g, true);
        if (!mi) { f.falha = "error de lectura de un mapa"; goto fim; }
        uint32_t ocupados = 0;
        mudou = false;
        for (uint32_t bit = 0; bit < v->ipg; bit++) {
            uint32_t n = g * v->ipg + bit;
            bool u = ino_uso[n / 8] & (1 << (n % 8)), em = mi[bit / 8] & (1 << (bit % 8));
            if (u) ocupados++;
            if (u == em) continue;
            if (u) mi[bit / 8] |= (uint8_t)(1 << (bit % 8)); else mi[bit / 8] &= (uint8_t)~(1 << (bit % 8));
            mudou = true;
            (*mudancas)++;
        }
        if (mudou) { sujar(v, gd_mapa_inodes(v, g)); v->grupo[g] |= GR_MI; }
        if (gd_campo(v, g, GD_LIVRES_B, GD_LIVRES_B_A) != qtd - usados || gd_campo(v, g, GD_LIVRES_I, GD_LIVRES_I_A) != v->ipg - ocupados ||
            gd_campo(v, g, GD_PASTAS, GD_PASTAS_A) != pastas[g]) {
            gd_por_campo(v, g, GD_LIVRES_B, GD_LIVRES_B_A, qtd - usados);
            gd_por_campo(v, g, GD_LIVRES_I, GD_LIVRES_I_A, v->ipg - ocupados);
            gd_por_campo(v, g, GD_PASTAS, GD_PASTAS_A, pastas[g]);
            marcar_grupo(v, g, 0);
            (*mudancas)++;
        }
        livres_b += qtd - usados;
        livres_i += v->ipg - ocupados;
    }
    st32(v->sb + 12, livres_b);
    st32(v->sb + 16, livres_i);
    v->sb_sujo = true;
    if (confirmar(v) < 0 || v->erro) { v->erro = 0; f.falha = "error al grabar lo corregido"; }
fim:
    if (f.uso) kfree(f.uso);
    if (ino_uso) kfree(ino_uso);
    if (pastas) kfree(pastas);
    for (int i = 0; i < 3; i++) if (f.buf[i]) kfree(f.buf[i]);
    return f.falha;
}

static int e2_montar(disco_t *d, uint32_t *opcoes, void **priv)
{
    e2_t *v = kcalloc(sizeof(e2_t));
    if (!v) return ERRO_SEM_MEMORIA;
    v->d = d;
    v->ncache = v->ncache_max = CACHE_BASE;
    if (disco_ler(d, 2, 2, v->sb) < 0) { liberar(v); return ERRO_ACESSO; }
    uint8_t *sb = v->sb;
    if (le16(sb + 56) != 0xEF53) { liberar(v); return ERRO_FORMATO; }
    uint32_t rev = le32(sb + 76), log = le32(sb + 24);
    if (log > 2) return recusar(v, d, "ext: bloques de más de 4 KB no soportados", log);
    v->bloco = 1024u << log;
    v->spb = v->bloco / 512;
    v->inodes = le32(sb + 0);
    v->blocos = le32(sb + 4);
    v->primeiro_bloco = le32(sb + 20);
    v->bpg = le32(sb + 32);
    v->ipg = le32(sb + 40);
    v->tam_inode = rev ? le16(sb + 88) : 128;
    v->primeiro_ino = rev ? le32(sb + 84) : 11;
    v->compat = rev ? le32(sb + 92) : 0;
    v->incompat = rev ? le32(sb + 96) : 0;
    v->ro_compat = rev ? le32(sb + 100) : 0;
    v->tipo_dir = (v->incompat & INCOMPAT_TIPO_DIR) != 0;
    v->desc_size = (v->incompat & INCOMPAT_64BIT) ? le16(sb + 254) : 32;
    if (v->desc_size < 32) v->desc_size = 32;
    if (v->incompat & ~(uint32_t)INCOMPAT_LEITURA)
        return recusar(v, d, "ext4 con características no soportadas: no se monta", v->incompat & ~(uint32_t)INCOMPAT_LEITURA);
    if (v->ro_compat & RO_BIGALLOC) return recusar(v, d, "ext4 con bigalloc: no se monta", v->ro_compat);
    if ((v->incompat & INCOMPAT_64BIT) && le32(sb + 0x150))
        return recusar(v, d, "ext4 de más de 2^32 bloques: no se monta", le32(sb + 0x150));
    if (!v->bpg || !v->ipg || v->tam_inode < 128 || v->tam_inode > v->bloco || (v->tam_inode & (v->tam_inode - 1)) ||
        v->blocos <= v->primeiro_bloco || v->inodes < RAIZ || v->bpg > v->bloco * 8 || v->ipg > v->bloco * 8 ||
        v->desc_size > 1024 || (v->desc_size & (v->desc_size - 1)))
        return recusar(v, d, "ext: superbloque no válido", 0);
    if ((uint64_t)v->blocos * v->spb > d->setores) return recusar(v, d, "ext: el volumen es mayor que la partición", v->blocos);
    v->grupos = (v->blocos - v->primeiro_bloco + v->bpg - 1) / v->bpg;
    /* v8.5.4.2 (revisión): como e2fsck, s_inodes_count tiene que ser grupos * inodos por grupo. Con más, un
     * número de inodo apuntaría a un descriptor de grupo que no existe (memoria de otro): no se monta. Con
     * menos (raro, nunca lo hace mke2fs) se monta pero solo para leer (ni fsck ni escritura con esa cuenta). */
    uint64_t ino_grupos = (uint64_t)v->grupos * v->ipg;
    if (ino_grupos < v->inodes) return recusar(v, d, "ext: más inodos que grupos * inodos por grupo", v->inodes);
    bool inodes_curto = ino_grupos != v->inodes;
    v->gd_blocos = (v->grupos * v->desc_size + v->bloco - 1) / v->bloco;
    v->gd = kmalloc(v->gd_blocos * v->bloco);
    v->gdb_sujo = kcalloc(v->gd_blocos);
    v->grupo = kcalloc(v->grupos);
    v->tx_fis = kmalloc((CACHE_MAX + v->gd_blocos) * sizeof *v->tx_fis);
    v->tx_dados = kmalloc((CACHE_MAX + v->gd_blocos) * sizeof *v->tx_dados);
    if (!v->gd || !v->gdb_sujo || !v->grupo || !v->tx_fis || !v->tx_dados) { liberar(v); return ERRO_SEM_MEMORIA; }
    if (ler_descritores(v) < 0) { liberar(v); return ERRO_ACESSO; }
    v->csum = (v->ro_compat & RO_METADATA_CSUM) != 0;
    v->gdt_csum = !v->csum && (v->ro_compat & RO_GDT_CSUM);
    v->semente = (v->incompat & INCOMPAT_CSUM_SEED) ? le32(sb + 0x270) : crc32c(~0u, sb + 104, 16);

    /* D326: ¿se puede escribir? El primer motivo que lo impida deja el volumen en solo lectura. */
    uint16_t estado = le16(sb + 58);
    const char *motivo = NULL;
    static char texto[176];   /* D397: cabe el «ejecuta reparar hdaN» */
    const char *falta = caracteristica_nao_suportada(v);
    if (falta) { snformatar(texto, sizeof texto, "característica %s no soportada para escribir: solo lectura", falta); motivo = texto; }
    else if (rev > 1) motivo = "revisión desconocida: solo lectura";
    else if (v->csum && sb[0x175] != 1) motivo = "tipo de suma desconocido: solo lectura";
    else if (v->csum && le32(sb + 0x3FC) != soma_sb(sb)) motivo = "el superbloque tiene la suma mal: solo lectura";
    else if (v->tam_inode > 1024) motivo = "inodos de más de 1 KB: solo lectura";
    if (!motivo && com_soma_grupo(v))
        for (uint32_t g = 0; g < v->grupos; g++)
            if (le16(gd(v, g) + GD_SOMA) != soma_gd(v, g, gd(v, g))) { motivo = "descriptores de grupo con la suma mal: solo lectura"; break; }
    if (!motivo && gd64(v))
        for (uint32_t g = 0; g < v->grupos; g++)
            if (le32(gd(v, g) + 0x20) || le32(gd(v, g) + 0x24) || le32(gd(v, g) + 0x28)) { motivo = "descriptores con direcciones de 64 bits: solo lectura"; break; }
    if (!motivo && (estado & 2)) motivo = "el sistema de archivos tiene errores (usa «reparar» o e2fsck): solo lectura";
    if (!motivo && inodes_curto) motivo = "la cuenta de inodos no cuadra con los grupos (usa «reparar» o e2fsck): solo lectura";
    bool quer_escrever = (*opcoes & MONTAR_SO_LEITURA) == 0;
    bool reproduziu = false;
    /* El diario: sólo se abre (y se reproduce lo pendiente) si se va a escribir (D040: un
     * montaje de solo lectura no toca el disco). */
    if (!motivo && quer_escrever && (v->compat & COMPAT_DIARIO)) {
        const char *m = diario_abrir(v);
        if (m) { snformatar(texto, sizeof texto, "%s: solo lectura", m); motivo = texto; }
        else if (v->incompat & INCOMPAT_RECUPERAR) {
            if (!jbd2_vazio(&v->j)) {
                klog("[EXT2] %s: diario pendiente: se reproduce (secuencia %u)\n", d->nome, v->j.seq);
                if (jbd2_recuperar(&v->j) < 0) {
                    snformatar(texto, sizeof texto, "no se pudo reproducir el diario (%s): solo lectura",
                               v->j.motivo ? v->j.motivo : "error");
                    motivo = texto;
                } else {
                    klog("[EXT2] %s: diario reproducido: %u transacciones, %u bloques, %u revocados\n", d->nome,
                         v->j.reproduzidas, v->j.blocos_reproduzidos, v->j.revogados);
                    disco_avisar(AVISO_INFO, "%s: diario reproducido (%u transacciones)", d->nome, v->j.reproduzidas);
                    reproduziu = true;
                    /* D337: la caché tiene bloques leídos antes de reproducir (la tabla de inodos
                     * del inodo 8, al abrir el diario): ya no valen. Sin esto se leía la versión
                     * vieja de los inodos reproducidos y, al escribir, se habría grabado encima. */
                    for (uint32_t i = 0; i < CACHE_MAX; i++) v->cache[i].valido = v->cache[i].sujo = false;
                    /* Lo reproducido pudo cambiar el superbloque y los descriptores. */
                    uint8_t novo[1024];
                    if (disco_ler(d, 2, 2, novo) < 0 || le16(novo + 56) != 0xEF53 || ler_descritores(v) < 0) {
                        liberar(v);
                        return ERRO_ACESSO;
                    }
                    memcpy(sb, novo, 1024);
                    estado = le16(sb + 58);
                }
            }
            if (!motivo) {
                v->incompat &= ~(uint32_t)INCOMPAT_RECUPERAR;
                st32(sb + 96, v->incompat);
                if (!(estado & 2)) estado |= 1;   /* con el diario reproducido vuelve a ser coherente */
            }
        }
        if (!motivo && jbd2_capacidade(&v->j) < 64 + v->gd_blocos) motivo = "diario demasiado pequeño: solo lectura";
        if (!motivo) {
            v->diario = true;
            uint32_t c = jbd2_capacidade(&v->j) - v->gd_blocos;
            v->ncache_max = c < CACHE_MAX ? c : CACHE_MAX;
        }
    }
    /* D326/D337: con ESCRITA_EXT34 a 0, ext3/ext4 quedan en solo lectura tras reproducir el
     * diario (lo de la 8.4.3). Desde la 8.4.4 está a 1: probado con tests/ext34_unidad.c (cortes
     * de luz en cada escritura) y h326. */
    if (!ESCRITA_EXT34 && !motivo && (v->incompat & ~(uint32_t)(INCOMPAT_TIPO_DIR | INCOMPAT_RECUPERAR)))
        motivo = "ext4: SaguiOS solo lo lee (solo lectura)";
    if (!ESCRITA_EXT34 && !motivo && ((v->compat & COMPAT_DIARIO) || (v->ro_compat & ~(uint32_t)(RO_SPARSE | RO_ARQ_GRANDE | RO_BTREE))))
        motivo = "ext3 con diario: solo lectura";
    if (!ESCRITA_EXT34) v->diario = false;
    if (!motivo && (v->incompat & INCOMPAT_RECUPERAR))
        motivo = quer_escrever ? "diario pendiente de recuperar: solo lectura"
                               : "diario pendiente de recuperar (apagado brusco): solo lectura, puede faltar lo último";
    v->extents = (v->incompat & INCOMPAT_EXTENTS) != 0;
    v->so_leitura = !quer_escrever;
    if (!v->so_leitura && motivo) {
        v->so_leitura = true;
        *opcoes |= MONTAR_SO_LEITURA;
        disco_avisar(AVISO_INFO, "%s: %s", d->nome, motivo);
    }
    if (v->so_leitura) v->diario = false;
    if (motivo) klog("[EXT2] %s: %s\n", d->nome, motivo);
    if (!v->so_leitura && !v->diario && !(v->compat & COMPAT_DIARIO) && !(estado & 1) && !(estado & 2)) {
        /* v8.5.4.2 (D379 F): no se desmontó bien: el fsck básico antes de escribir; si no se puede, solo lectura. */
        uint32_t t0 = relogio_tiques(), mud = 0;
        kprintf("Comprobando %s (no se desmontó bien la última vez)...\n", d->nome);   /* puede tardar en discos grandes */
        const char *falha = fsck_basico(v, &mud);
        if (falha) {
            snformatar(texto, sizeof texto, "no se desmontó bien y no se puede comprobar (%s): solo lectura; ejecuta «reparar %s»",
                       falha, d->nome);
            klog("[EXT2] %s: %s\n", d->nome, texto);
            disco_avisar(AVISO_INFO, "%s: %s", d->nome, texto);
            console_cor(COR_AMARELO, COR_PRETO);
            /* D397: «reparar» (userland, reparo_ext2.c) lo arregla desde el propio SaguiOS. */
            kprintf("[aviso] %s: %s.\nLo que hay se puede leer y copiar. Para repararlo: «reparar %s» (como root; la\n"
                    "desmonta, la repara y la vuelve a montar con escritura), o e2fsck desde Linux. Si es la raíz\n"
                    "instalada, también se puede reinstalar (se conservan /home y la configuración).\n", d->nome, texto, d->nome);
            console_cor(COR_CINZA, COR_PRETO);
            v->so_leitura = true;
            *opcoes |= MONTAR_SO_LEITURA;
        } else {
            klog("[EXT2] %s: no se desmonto bien: fsck basico hecho (%u correcciones, %u ms)\n", d->nome, mud,
                 (relogio_tiques() - t0) * (1000u / TIQUES_POR_SEGUNDO));
            if (mud) disco_avisar(AVISO_INFO, "%s: no se desmontó bien; mapas y contadores corregidos (%u)", d->nome, mud);
            estado |= 1;
        }
    }
    v->estado_original = estado;
    if (!v->so_leitura) {
        /* Los contadores del superbloque, de la suma de los grupos (tras un corte pueden no
         * estar al día: con diario sólo los descriptores van en las transacciones). */
        uint32_t lb = 0, li = 0;
        for (uint32_t g = 0; g < v->grupos; g++) {
            lb += gd_campo(v, g, GD_LIVRES_B, GD_LIVRES_B_A);
            li += gd_campo(v, g, GD_LIVRES_I, GD_LIVRES_I_A);
        }
        st32(sb + 12, lb);
        st32(sb + 16, li);
        if (!(estado & 1)) klog("[EXT2] %s: no se desmonto bien la ultima vez (con diario: lo ha recuperado el diario)\n", d->nome);
        if (v->diario && jbd2_comecar(&v->j) < 0) {
            klog("[EXT2] %s: no se pudo abrir el diario para escribir\n", d->nome);
            liberar(v);
            return ERRO_ACESSO;
        }
        /* Marca "en uso": si el disco sale sin desmontar, el e2fsck lo sabrá. Con diario,
         * además needs_recovery, como Linux: el diario puede tener una transacción. */
        st16(sb + 58, estado & ~1u);
        if (v->diario) { v->incompat |= INCOMPAT_RECUPERAR; st32(sb + 96, v->incompat); }
        st32(sb + 44, agora());                       /* s_mtime */
        st16(sb + 52, le16(sb + 52) + 1);             /* s_mnt_count */
        if (gravar_sb(v) < 0 || disco_sincronizar(d) < 0) { liberar(v); return ERRO_ACESSO; }
    }
    v->dica = v->primeiro_bloco;
    char rotulo[17];
    memcpy(rotulo, sb + 120, 16);
    rotulo[16] = 0;
    klog("[EXT2] %s montado: rev %u, blocos de %u, %u blocos (%u livres), %u inodes de %u bytes, %u grupos, "
         "incompat %x ro_compat %x, %s%s%s%s, rotulo \"%s\"\n",
         d->nome, rev, v->bloco, v->blocos, le32(sb + 12), v->inodes, v->tam_inode, v->grupos, v->incompat, v->ro_compat,
         v->so_leitura ? "so leitura" : "leitura e escrita", v->diario ? ", diario ordered" : "",
         v->csum ? ", metadata_csum" : "", reproduziu ? ", diario reproduzido" : "", rotulo);
    *priv = v;
    return 0;
}

static int e2_desmontar(void *priv, bool gravar)
{
    e2_t *v = priv;
    int r = 0;
    if (!v->so_leitura && gravar) {
        /* Borrados que seguían abiertos. */
        for (int k = 0; k < MAX_ABERTOS; k++)
            if (v->abertos[k].refs && v->abertos[k].orfao) {
                inode_t i;
                if (inode_ler(v, v->abertos[k].ino, &i) == 0) liberar_inode(v, &i);
                v->abertos[k].refs = 0;
            }
        r = terminar(v, 0);
        if (v->diario && !v->so_leitura) {
            /* Diario vacío y después el superbloque sin needs_recovery (como Linux). */
            if (jbd2_fechar(&v->j) < 0 && !r) r = ERRO_ACESSO;
            v->incompat &= ~(uint32_t)INCOMPAT_RECUPERAR;
            st32(v->sb + 96, v->incompat);
        }
        if (!v->so_leitura) {
            st16(v->sb + 58, v->estado_original);
            st32(v->sb + 48, agora());                    /* s_wtime */
            if (gravar_sb(v) < 0 && !r) r = ERRO_ACESSO;
            if (disco_sincronizar(v->d) < 0 && !r) r = ERRO_ACESSO;
            klog("[EXT2] %s desmontado (tudo gravado)\n", v->d->nome);
        } else {
            klog("[EXT2] %s desmontado tras un error de escritura (convem e2fsck)\n", v->d->nome);
        }
    } else {
        klog("[EXT2] %s desmontado%s\n", v->d->nome, v->so_leitura ? "" : " sem gravar");
    }
    liberar(v);
    return r;
}

/* ext2 retrasa hasta 1 s la escritura de datos (terminar_dados): aquí se fuerza ya. */
static int e2_sincronizar(void *priv, void *arq)
{
    e2_t *v = priv;
    (void)arq;
    return terminar(v, 0);
}

const sistema_arquivos_t fs_ext2 = {
    .nome = "ext2", .tam_arq = sizeof(e2_arq_t),
    .montar = e2_montar, .desmontar = e2_desmontar, .info = e2_info, .datas = e2_datas, .listar = e2_listar,
    .abrir = e2_abrir, .ler = e2_ler, .escrever = e2_escrever, .truncar = e2_truncar, .fechar = e2_fechar, .sincronizar = e2_sincronizar,
    .criar_pasta = e2_criar_pasta, .remover_pasta = e2_remover_pasta, .apagar = e2_apagar,
    .renomear = e2_renomear, .espaco = e2_espaco,
    .meta = e2_meta, .fixar_meta = e2_fixar_meta,   /* D282 */
    .fixar_datas = e2_fixar_datas,                  /* D341 */
};

