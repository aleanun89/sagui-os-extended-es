// SPDX-License-Identifier: GPL-2.0-or-later
/* ext2.c - ext2 (lectura y escritura) y ext3/ext4 (sólo lectura). Ver D040.
 *
 * Estructura de un volumen ext2 (documentación de Linux, "The Second Extended
 * Filesystem"):
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
 * Escritura sólo en ext2 "puro": sin diario (ext3), sin extents/64 bits/sumas de
 * verificación (ext4) y sin características que este código no conoce. Fuera
 * de eso el volumen se monta sólo para lectura, con un aviso. Nunca se graba en un
 * ext3/ext4.
 *
 * Los bloques de metadatos pasan por una caché por volumen, grabada al final de
 * cada operación (como en el FAT32, D026); los datos de bloques enteros van directo
 * al disco. Los nombres se graban en UTF-8 (como Linux) y se muestran en
 * Latin-1 (D027). */
#include "montagem.h"
#include "permisos.h"
#include "console.h"
#include "memoria.h"
#include "relogio.h"
#include "texto.h"

/* Características (superbloque). */
#define COMPAT_DIARIO        0x0004
#define INCOMPAT_TIPO_DIR    0x0002
#define INCOMPAT_RECUPERAR   0x0004
#define INCOMPAT_EXTENTS     0x0040
#define INCOMPAT_64BIT       0x0080
#define INCOMPAT_MMP         0x0100
#define INCOMPAT_FLEX_BG     0x0200
#define INCOMPAT_EA_INODE    0x0400
#define INCOMPAT_CSUM_SEED   0x2000
#define INCOMPAT_LARGEDIR    0x4000
#define INCOMPAT_INLINE      0x8000
#define INCOMPAT_CASEFOLD    0x20000
#define INCOMPAT_LEITURA     (INCOMPAT_TIPO_DIR | INCOMPAT_RECUPERAR | INCOMPAT_EXTENTS | INCOMPAT_64BIT | INCOMPAT_MMP | \
                              INCOMPAT_FLEX_BG | INCOMPAT_EA_INODE | INCOMPAT_CSUM_SEED | INCOMPAT_LARGEDIR | \
                              INCOMPAT_INLINE | INCOMPAT_CASEFOLD)
#define RO_SPARSE            0x0001
#define RO_ARQ_GRANDE        0x0002
#define RO_BTREE             0x0004
#define RO_BIGALLOC          0x0200
#define RO_ESCRITA           (RO_SPARSE | RO_ARQ_GRANDE | RO_BTREE)

/* Inodo */
#define MODO_TIPO     0xF000
#define MODO_ARQUIVO  0x8000
#define MODO_PASTA    0x4000
#define MODO_LIGACAO  0xA000
#define FL_INDICE     0x00001000   /* carpeta con htree */
#define FL_EXTENTS    0x00080000
#define FL_INLINE     0x10000000
#define RAIZ          2

#define CACHE 24
#define MAX_ABERTOS 32

typedef struct {
    disco_t *d;
    uint8_t  sb[1024];
    uint32_t bloco, spb;              /* bytes y sectores por bloque */
    uint32_t blocos, inodes, primeiro_bloco, bpg, ipg, grupos;
    uint32_t tam_inode, primeiro_ino, desc_size;
    uint32_t compat, incompat, ro_compat;
    bool     so_leitura, tipo_dir;
    uint16_t estado_original;
    uint8_t *gd;                      /* descriptores de grupo (bloques enteros) */
    uint32_t gd_blocos;
    bool     gd_sujo, sb_sujo;
    struct { uint32_t num; bool valido, sujo; uint32_t uso; uint8_t *dados; } cache[CACHE];
    uint32_t relogio;
    int      erro;                    /* primer error de E/S de la operación */
    uint32_t dica;                    /* siguiente bloque a intentar asignar */
    uint32_t ultima_gravacao;         /* marca de la última grabación de la caché */
    /* Continuación del "listar" (ls de carpetas grandes sin releer todo). */
    uint32_t lst_ino, lst_indice, lst_lb, lst_desl;
    /* Archivos abiertos: un inodo borrado mientras está abierto sólo desaparece al final. */
    struct { uint32_t ino, refs; bool orfao; } abertos[MAX_ABERTOS];
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
#define I_ACL(i)      le32((i)->b + 104)
/* D282: dueño y grupo de 32 bits (los 16 altos en el osd2 de Linux). */
#define I_UID(i)      ((uint32_t)le16((i)->b + 2) | (uint32_t)le16((i)->b + 120) << 16)
#define I_GID(i)      ((uint32_t)le16((i)->b + 24) | (uint32_t)le16((i)->b + 122) << 16)
#define EH_PASTA(i)   ((I_MODO(i) & MODO_TIPO) == MODO_PASTA)
#define EH_ARQUIVO(i) ((I_MODO(i) & MODO_TIPO) == MODO_ARQUIVO)

/* ------------------------------------------------------------ hora */

/* Segundos desde 1/1/1970 en UTC, como manda el ext2: la hora del sistema
 * (D080), que ya descontó la zona si el RTC está en hora local. */
static uint32_t agora(void) { return hora_utc(); }

/* ------------------------------------------------------------ caché de bloques */

static int erro_es(e2_t *v) { if (!v->erro) v->erro = ERRO_ACESSO; return ERRO_ACESSO; }

static int gravar_slot(e2_t *v, int i)
{
    if (!v->cache[i].sujo) return 0;
    if (disco_escrever(v->d, v->cache[i].num * v->spb, v->spb, v->cache[i].dados) < 0) return erro_es(v);
    v->cache[i].sujo = false;
    return 0;
}

static int achar_slot(e2_t *v, uint32_t b, bool *achou)
{
    int livre = -1, velho = 0;
    for (int i = 0; i < CACHE; i++) {
        if (v->cache[i].valido && v->cache[i].num == b) { *achou = true; v->cache[i].uso = ++v->relogio; return i; }
        if (!v->cache[i].valido && livre < 0) livre = i;
        if (v->cache[i].uso < v->cache[velho].uso) velho = i;
    }
    *achou = false;
    int i = livre >= 0 ? livre : velho;
    if (v->cache[i].valido && gravar_slot(v, i) < 0) return -1;
    if (!v->cache[i].dados && !(v->cache[i].dados = kmalloc(v->bloco))) return -1;
    v->cache[i].valido = false;
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
    bool achou;
    int i = achar_slot(v, b, &achou);
    if (i < 0) return NULL;
    memset(v->cache[i].dados, 0, v->bloco);
    v->cache[i].num = b;
    v->cache[i].valido = true;
    v->cache[i].sujo = true;
    v->cache[i].uso = ++v->relogio;
    return v->cache[i].dados;
}

static void sujar(e2_t *v, uint32_t b)
{
    for (int i = 0; i < CACHE; i++)
        if (v->cache[i].valido && v->cache[i].num == b) v->cache[i].sujo = true;
}

static void esquecer(e2_t *v, uint32_t b)
{
    for (int i = 0; i < CACHE; i++)
        if (v->cache[i].valido && v->cache[i].num == b) v->cache[i].valido = v->cache[i].sujo = false;
}

/* Bloques de datos enteros, directo del disco (con lo que esté sucio en la caché encima). */
static int ler_direto(e2_t *v, uint32_t b, uint32_t n, uint8_t *dest)
{
    if (b + n > v->blocos || b + n < b) return erro_es(v);
    if (disco_ler(v->d, b * v->spb, n * v->spb, dest) < 0) return erro_es(v);
    for (int i = 0; i < CACHE; i++)
        if (v->cache[i].valido && v->cache[i].sujo && v->cache[i].num >= b && v->cache[i].num < b + n)
            memcpy(dest + (v->cache[i].num - b) * v->bloco, v->cache[i].dados, v->bloco);
    return 0;
}

static int gravar_direto(e2_t *v, uint32_t b, const uint8_t *orig)
{
    for (int i = 0; i < CACHE; i++)
        if (v->cache[i].valido && v->cache[i].num == b) { memcpy(v->cache[i].dados, orig, v->bloco); v->cache[i].sujo = false; }
    if (disco_escrever(v->d, b * v->spb, v->spb, orig) < 0) return erro_es(v);
    return 0;
}

static int gravar_sb(e2_t *v)
{
    v->sb_sujo = false;
    if (disco_escrever(v->d, 2, 2, v->sb) < 0) return erro_es(v);
    return 0;
}

/* Fin de cada operación: graba la caché, los descriptores y el superbloque. */
static int terminar(e2_t *v, int r)
{
    if (!v->so_leitura) {
        v->ultima_gravacao = relogio_tiques();
        for (int i = 0; i < CACHE; i++) if (v->cache[i].valido) gravar_slot(v, i);
        if (v->gd_sujo) {
            v->gd_sujo = false;
            if (disco_escrever(v->d, (v->primeiro_bloco + 1) * v->spb, v->gd_blocos * v->spb, v->gd) < 0) erro_es(v);
        }
        if (v->sb_sujo) gravar_sb(v);
    }
    if (v->erro) { if (r >= 0) r = v->erro; v->erro = 0; }
    return r;
}

/* ------------------------------------------------------------ grupos */

static uint8_t *gd(e2_t *v, uint32_t g) { return v->gd + g * v->desc_size; }
static uint32_t gd_mapa_blocos(e2_t *v, uint32_t g) { return le32(gd(v, g) + 0); }
static uint32_t gd_mapa_inodes(e2_t *v, uint32_t g) { return le32(gd(v, g) + 4); }
static uint32_t gd_tabela(e2_t *v, uint32_t g) { return le32(gd(v, g) + 8); }

/* Suma "delta" a un contador del grupo (y al del superbloque, si off_sb >= 0). */
static void contar(e2_t *v, uint32_t g, int off_gd, int off_sb, int delta)
{
    uint8_t *x = gd(v, g) + off_gd;
    st16(x, (uint32_t)(le16(x) + delta));
    if (off_sb >= 0) st32(v->sb + off_sb, le32(v->sb + off_sb) + (uint32_t)delta);
    v->gd_sujo = v->sb_sujo = true;
}

/* Cuántos bloques tiene el grupo g (el último puede ser menor). */
static uint32_t blocos_no_grupo(e2_t *v, uint32_t g)
{
    uint32_t ini = v->primeiro_bloco + g * v->bpg;
    return v->blocos - ini < v->bpg ? v->blocos - ini : v->bpg;
}

static uint32_t alocar_bloco(e2_t *v, uint32_t perto)
{
    if (perto < v->primeiro_bloco || perto >= v->blocos) perto = v->dica;
    if (perto < v->primeiro_bloco || perto >= v->blocos) perto = v->primeiro_bloco;
    uint32_t g0 = (perto - v->primeiro_bloco) / v->bpg;
    for (uint32_t k = 0; k <= v->grupos; k++) {
        uint32_t g = (g0 + k) % v->grupos;
        if (le16(gd(v, g) + 12) == 0) continue;
        uint32_t qtd = blocos_no_grupo(v, g);
        uint32_t ini = (k == 0) ? (perto - v->primeiro_bloco) % v->bpg : 0;
        uint32_t mb = gd_mapa_blocos(v, g);
        uint8_t *m = bloco(v, mb);
        if (!m) return 0;
        for (uint32_t j = 0; j < qtd; j++) {
            uint32_t bit = (ini + j) % qtd;
            if (m[bit / 8] & (1 << (bit % 8))) continue;
            m[bit / 8] |= (uint8_t)(1 << (bit % 8));
            sujar(v, mb);
            contar(v, g, 12, 12, -1);
            uint32_t b = v->primeiro_bloco + g * v->bpg + bit;
            v->dica = b + 1;
            return b;
        }
    }
    return 0;   /* volumen lleno */
}

static void liberar_bloco(e2_t *v, uint32_t b)
{
    if (b < v->primeiro_bloco || b >= v->blocos) return;
    uint32_t g = (b - v->primeiro_bloco) / v->bpg, bit = (b - v->primeiro_bloco) % v->bpg;
    uint32_t mb = gd_mapa_blocos(v, g);
    uint8_t *m = bloco(v, mb);
    if (!m || !(m[bit / 8] & (1 << (bit % 8)))) return;
    m[bit / 8] &= (uint8_t)~(1 << (bit % 8));
    sujar(v, mb);
    esquecer(v, b);
    contar(v, g, 12, 12, +1);
}

static uint32_t alocar_inode(e2_t *v, uint32_t g0, bool pasta)
{
    for (uint32_t k = 0; k < v->grupos; k++) {
        uint32_t g = (g0 + k) % v->grupos;
        if (le16(gd(v, g) + 14) == 0) continue;
        uint32_t mi = gd_mapa_inodes(v, g);
        uint8_t *m = bloco(v, mi);
        if (!m) return 0;
        for (uint32_t bit = 0; bit < v->ipg; bit++) {
            uint32_t num = g * v->ipg + bit + 1;
            if (num < v->primeiro_ino || num > v->inodes) continue;
            if (m[bit / 8] & (1 << (bit % 8))) continue;
            m[bit / 8] |= (uint8_t)(1 << (bit % 8));
            sujar(v, mi);
            contar(v, g, 14, 16, -1);
            if (pasta) contar(v, g, 16, -1, +1);
            return num;
        }
    }
    return 0;
}

static void liberar_inode_bitmap(e2_t *v, uint32_t num, bool pasta)
{
    uint32_t g = (num - 1) / v->ipg, bit = (num - 1) % v->ipg;
    uint32_t mi = gd_mapa_inodes(v, g);
    uint8_t *m = bloco(v, mi);
    if (!m || !(m[bit / 8] & (1 << (bit % 8)))) return;
    m[bit / 8] &= (uint8_t)~(1 << (bit % 8));
    sujar(v, mi);
    contar(v, g, 14, 16, +1);
    if (pasta) contar(v, g, 16, -1, -1);
}

/* ------------------------------------------------------------ inodos */

static bool inode_onde(e2_t *v, uint32_t num, uint32_t *b, uint32_t *off)
{
    if (num < 1 || num > v->inodes) return false;
    uint32_t g = (num - 1) / v->ipg, idx = (num - 1) % v->ipg;
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
    sujar(v, b);
    return 0;
}

static void inode_tempos(inode_t *i, bool acesso, bool mudanca)
{
    uint32_t t = agora();
    if (acesso) st32(i->b + 8, t);
    if (mudanca) { st32(i->b + 12, t); st32(i->b + 16, t); }
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

/* ------------------------------------------------------------ bloques de un archivo */

/* Árbol de extents del ext4 (sólo lectura). 0 = agujero (o no inicializado). */
static uint32_t mapear_extent(e2_t *v, const uint8_t *no, uint32_t lb)
{
    for (int nivel = 0; nivel < 6; nivel++) {
        if (le16(no) != 0xF30A) { erro_es(v); return 0; }
        uint32_t qtd = le16(no + 2), prof = le16(no + 6);
        const uint8_t *e = no + 12;
        if (prof == 0) {
            for (uint32_t k = 0; k < qtd; k++, e += 12) {
                uint32_t ini = le32(e), len = le16(e + 4);
                bool nao_inic = len > 32768;
                if (nao_inic) len -= 32768;
                if (lb >= ini && lb - ini < len) {
                    if (nao_inic || le16(e + 6)) return 0;
                    return le32(e + 8) + (lb - ini);
                }
            }
            return 0;
        }
        const uint8_t *esc = NULL;
        for (uint32_t k = 0; k < qtd; k++, e += 12)
            if (le32(e) <= lb) esc = e;
        if (!esc || le16(esc + 8)) return 0;
        no = bloco(v, le32(esc + 4));
        if (!no) return 0;
    }
    return 0;
}

static uint32_t potencia(uint32_t p, int n) { uint32_t r = 1; while (n-- > 0) r *= p; return r; }

/* Bloque físico del bloque lógico "lb". Con "alocar", crea lo que falte
 * (bloques indirectos en cero); *novo indica si el bloque de datos es nuevo (el
 * que llama lo rellena). 0 = agujero o fallo. */
static uint32_t mapear(e2_t *v, inode_t *ino, uint32_t lb, bool alocar, bool *novo)
{
    if (novo) *novo = false;
    if (I_FLAGS(ino) & FL_EXTENTS) return alocar ? 0 : mapear_extent(v, ino->b + 40, lb);
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
    if (tem_blocos(ino) && !(I_FLAGS(ino) & FL_EXTENTS) && tam < I_TAM(ino)) {
        uint32_t P = v->bloco / 4;
        uint64_t corte = tam / v->bloco + (tam % v->bloco ? 1 : 0);
        uint32_t *blk = I_BLOCO(ino);
        for (uint32_t k = 0; k < 12; k++)
            if (k >= corte && blk[k]) { liberar_bloco(v, blk[k]); somar_setores(v, ino, -1); blk[k] = 0; }
        cortar(v, &blk[12], 1, 12, corte, ino);
        cortar(v, &blk[13], 2, 12 + P, corte, ino);
        cortar(v, &blk[14], 3, 12 + P + (uint64_t)P * P, corte, ino);
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
        if (x && le32(x) == 0xEA020000 && le32(x + 4) > 1) { st32(x + 4, le32(x + 4) - 1); sujar(v, acl); }
        else if (x) liberar_bloco(v, acl);
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

/* Siguiente entrada usada de la carpeta: 1 encontrada, 0 terminó, <0 error. */
static int dir_ler(e2_t *v, inode_t *dir, dcursor_t *c, dentrada_t *e)
{
    uint32_t nblocos = (I_TAM(dir) + v->bloco - 1) / v->bloco;
    while (c->lb < nblocos) {
        if (c->desl + 8 > v->bloco) { c->lb++; c->desl = 0; c->ant = 0xFFFFFFFFu; continue; }
        uint32_t fis = mapear(v, dir, c->lb, false, NULL);
        if (!fis) { c->lb++; c->desl = 0; c->ant = 0xFFFFFFFFu; continue; }
        uint8_t *x = bloco(v, fis);
        if (!x) return ERRO_ACESSO;
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

/* Añade una entrada en la carpeta "num_dir" (relee el inodo: puede haber cambiado). */
static int dir_adicionar(e2_t *v, uint32_t num_dir, const char *nome_l1, uint32_t ino, uint8_t tipo)
{
    inode_t dir;
    int r = inode_ler(v, num_dir, &dir);
    if (r < 0) return r;
    uint8_t nome[256];
    int nl = nome_para_disco(nome_l1, nome, 255);
    if (nl <= 0) return ERRO_INVALIDO;
    uint32_t precisa = (8 + (uint32_t)nl + 3) & ~3u;
    uint32_t nblocos = I_TAM(&dir) / v->bloco;
    for (uint32_t lb = 0; lb < nblocos; lb++) {
        uint32_t fis = mapear(v, &dir, lb, false, NULL);
        if (!fis) continue;
        uint8_t *x = bloco(v, fis);
        if (!x) return ERRO_ACESSO;
        for (uint32_t d = 0; d + 8 <= v->bloco; ) {
            uint8_t *p = x + d;
            uint32_t rec = le16(p + 4), eino = le32(p);
            uint32_t enl = v->tipo_dir ? p[6] : le16(p + 6);
            if (rec < 8 || rec % 4 || d + rec > v->bloco) break;
            uint32_t real = eino ? ((8 + enl + 3) & ~3u) : 0;
            if (rec >= real && rec - real >= precisa) {
                if (eino) {           /* divide la entrada: ella se queda con lo que usa */
                    st16(p + 4, real);
                    p += real;
                    rec -= real;
                }
                gravar_entrada(v, p, ino, rec, nome, (uint32_t)nl, tipo);
                sujar(v, fis);
                goto feito;
            }
            d += rec;
        }
    }
    {   /* Sin espacio: un bloque más al final de la carpeta. */
        bool novo;
        uint32_t fis = mapear(v, &dir, nblocos, true, &novo);
        if (!fis) return ERRO_SEM_MEMORIA;
        uint8_t *x = bloco_novo(v, fis);
        if (!x) return ERRO_ACESSO;
        gravar_entrada(v, x, ino, v->bloco, nome, (uint32_t)nl, tipo);
        st32(dir.b + 4, (nblocos + 1) * v->bloco);
    }
feito:
    /* Una carpeta con índice htree modificada como lista pierde el índice (el Linux
     * pasa a leerla como lista; el e2fsck -D recrea el índice). */
    st32(dir.b + 32, I_FLAGS(&dir) & ~(uint32_t)FL_INDICE);
    inode_tempos(&dir, false, true);
    return inode_gravar(v, &dir);
}

/* Quita la entrada "nome" de la carpeta; devuelve el inodo al que apuntaba. */
static int dir_remover(e2_t *v, uint32_t num_dir, const char *nome, uint32_t *ino)
{
    inode_t dir;
    int r = inode_ler(v, num_dir, &dir);
    if (r < 0) return r;
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
    sujar(v, e.fis);
    *ino = e.ino;
    st32(dir.b + 32, I_FLAGS(&dir) & ~(uint32_t)FL_INDICE);
    inode_tempos(&dir, false, true);
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
    inode_tempos(&i, false, true);
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
    if (l > 1 && !EH_PASTA(&i)) { st16(i.b + 26, l - 1); inode_tempos(&i, false, true); inode_gravar(v, &i); return; }
    int k = aberto(v, num);
    if (k >= 0) { v->abertos[k].orfao = true; return; }
    liberar_inode(v, &i);
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
    inf->tipo = EH_PASTA(&i) ? TIPO_PASTA : TIPO_ARQUIVO;
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
            saida->tipo = EH_PASTA(&f) ? TIPO_PASTA : TIPO_ARQUIVO;
            saida->tamanho = EH_PASTA(&f) ? 0 : I_TAM(&f);
        } else {
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
    if (!num) return ERRO_SEM_MEMORIA;
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
    inode_tempos(novo, true, true);
    if (v->tam_inode > 128) st16(novo->b + 128, v->tam_inode >= 160 ? 32 : 0);   /* i_extra_isize */
    if (pasta) {
        bool n;
        uint32_t fis = mapear(v, novo, 0, true, &n);
        uint8_t *d = fis ? bloco_novo(v, fis) : NULL;
        if (!d) { liberar_inode_bitmap(v, num, true); return fis ? ERRO_ACESSO : ERRO_SEM_MEMORIA; }
        gravar_entrada(v, d, num, 12, (const uint8_t *)".", 1, 2);
        gravar_entrada(v, d + 12, pai->num, v->bloco - 12, (const uint8_t *)"..", 2, 2);
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
        if (r == 0) r = criar(v, &pai, nome, false, &i);
        criou = r == 0;
    }
    if (r < 0) return terminar(v, r);
    if (EH_PASTA(&i)) return terminar(v, ERRO_E_PASTA);
    if ((modo & ABRIR_ESCREVER) && (!EH_ARQUIVO(&i) || (I_FLAGS(&i) & (FL_EXTENTS | FL_INLINE)))) return terminar(v, ERRO_ACESSO);
    if ((modo & ABRIR_TRUNCAR) && I_TAM(&i)) {
        if (v->so_leitura) return terminar(v, ERRO_SO_LEITURA);
        truncar_inode(v, &i, 0);
        inode_tempos(&i, false, true);
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
    inode_tempos(&i, false, true);
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
    truncar_inode(v, &i, tam);
    inode_tempos(&i, false, true);
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
    const char *nome;
    int r = separar(v, rel, &pai, &nome);
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
    if (!pasta_vazia(v, &i)) return terminar(v, ERRO_NAO_VAZIA);
    r = separar(v, rel, &pai, &nome);
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
    r = separar(v, rel, &pai, &nome);
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
    r = separar(v, de, &pai_de, &nome_de);
    if (r == 0) r = separar(v, final, &pai_para, &nome_para);
    if (r < 0) return terminar(v, r);
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
            if (x) { st32(x + e.desl, pai_para.num); sujar(v, e.fis); }
        }
        ligacoes(v, pai_de.num, -1);
        ligacoes(v, pai_para.num, +1);
    }
    if (inode_ler(v, src.num, &src) == 0) { st32(src.b + 12, agora()); inode_gravar(v, &src); }
    return terminar(v, 0);
}

static void e2_espaco(void *priv, uint32_t *total_kb, uint32_t *livre_kb)
{
    e2_t *v = priv;
    *total_kb = v->blocos * (v->bloco / 1024);
    *livre_kb = le32(v->sb + 12) * (v->bloco / 1024);
}

/* ------------------------------------------------------------ montaje */

static void liberar(e2_t *v)
{
    for (int i = 0; i < CACHE; i++) if (v->cache[i].dados) kfree(v->cache[i].dados);
    if (v->gd) kfree(v->gd);
    kfree(v);
}

static int recusar(e2_t *v, disco_t *d, const char *motivo, uint32_t valor)
{
    klog("[EXT2] %s: %s (%x)\n", d->nome, motivo, valor);
    disco_avisar(AVISO_INFO, "%s: %s", d->nome, motivo);
    liberar(v);
    return ERRO_FORMATO;
}

static int e2_montar(disco_t *d, uint32_t *opcoes, void **priv)
{
    e2_t *v = kcalloc(sizeof(e2_t));
    if (!v) return ERRO_SEM_MEMORIA;
    v->d = d;
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
        v->blocos <= v->primeiro_bloco || v->inodes < RAIZ || v->bpg > v->bloco * 8 || v->ipg > v->bloco * 8)
        return recusar(v, d, "ext: superbloque no válido", 0);
    if ((uint64_t)v->blocos * v->spb > d->setores) return recusar(v, d, "ext: el volumen es mayor que la partición", v->blocos);
    v->grupos = (v->blocos - v->primeiro_bloco + v->bpg - 1) / v->bpg;
    v->gd_blocos = (v->grupos * v->desc_size + v->bloco - 1) / v->bloco;
    v->gd = kmalloc(v->gd_blocos * v->bloco);
    if (!v->gd) { liberar(v); return ERRO_SEM_MEMORIA; }
    if (disco_ler(d, (v->primeiro_bloco + 1) * v->spb, v->gd_blocos * v->spb, v->gd) < 0) { liberar(v); return ERRO_ACESSO; }

    /* Escritura sólo en ext2 que este código conoce por completo (D040). */
    uint16_t estado = le16(sb + 58);
    const char *motivo = NULL;
    if (v->incompat & INCOMPAT_RECUPERAR) motivo = "diario pendiente de recuperar (apagado brusco): solo lectura";
    else if (v->incompat & ~(uint32_t)INCOMPAT_TIPO_DIR) motivo = "ext4: SaguiOS solo lo lee (solo lectura)";
    else if (v->compat & COMPAT_DIARIO) motivo = "ext3 con diario: solo lectura";
    else if (v->ro_compat & ~(uint32_t)RO_ESCRITA) motivo = "características desconocidas: solo lectura";
    else if (rev > 1) motivo = "revisión desconocida: solo lectura";
    else if (estado & 2) motivo = "el sistema de archivos tiene errores (usa e2fsck): solo lectura";
    v->so_leitura = (*opcoes & MONTAR_SO_LEITURA) != 0;
    if (!v->so_leitura && motivo) {
        v->so_leitura = true;
        *opcoes |= MONTAR_SO_LEITURA;
        disco_avisar(AVISO_INFO, "%s: %s", d->nome, motivo);
    }
    if (motivo) klog("[EXT2] %s: %s\n", d->nome, motivo);
    v->estado_original = estado;
    if (!v->so_leitura) {
        if (!(estado & 1)) klog("[EXT2] %s: nao foi desmontado direito da ultima vez (convem e2fsck)\n", d->nome);
        /* Marca "en uso": si el disco sale sin desmontar, el e2fsck lo sabrá. */
        st16(sb + 58, estado & ~1u);
        st32(sb + 44, agora());                       /* s_mtime */
        st16(sb + 52, le16(sb + 52) + 1);             /* s_mnt_count */
        if (gravar_sb(v) < 0) { liberar(v); return ERRO_ACESSO; }
    }
    v->dica = v->primeiro_bloco;
    char rotulo[17];
    memcpy(rotulo, sb + 120, 16);
    rotulo[16] = 0;
    klog("[EXT2] %s montado: rev %u, blocos de %u, %u blocos (%u livres), %u inodes de %u bytes, %u grupos, "
         "incompat %x ro_compat %x, %s, rotulo \"%s\"\n",
         d->nome, rev, v->bloco, v->blocos, le32(sb + 12), v->inodes, v->tam_inode, v->grupos, v->incompat, v->ro_compat,
         v->so_leitura ? "so leitura" : "leitura e escrita", rotulo);
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
        st16(v->sb + 58, v->estado_original);
        st32(v->sb + 48, agora());                    /* s_wtime */
        if (gravar_sb(v) < 0 && !r) r = ERRO_ACESSO;
        klog("[EXT2] %s desmontado (tudo gravado)\n", v->d->nome);
    } else {
        klog("[EXT2] %s desmontado%s\n", v->d->nome, v->so_leitura ? "" : " sem gravar");
    }
    liberar(v);
    return r;
}

/* ext2 retrasa hasta 1 s la escritura de datos (terminar_dados): aqu� se fuerza ya. */
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
};
