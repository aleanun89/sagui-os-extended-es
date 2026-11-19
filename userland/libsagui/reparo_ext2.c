// SPDX-License-Identifier: GPL-3.0-or-later
/* reparo_ext2.c - reparar un ext2 que quedó mal tras un corte (D397, v8.5.4.10r2).
 *
 * Implementación propia, escrita a partir de la descripción del formato ext2 (no lleva código de
 * e2fsprogs). La usa la orden «reparar» (y la ventana de Discos). Un ext2 no tiene diario: tras un
 * cuelgue pueden quedar mapas de bits y contadores que no cuadran, inodos a medio escribir, bloques
 * que usan dos archivos, entradas de carpeta que apuntan a inodos borrados, carpetas o archivos sin
 * carpeta, y contadores de enlaces mal. El comprobador del kernel (fsck_basico, ext2.c) solo rehace
 * mapas y contadores; con algo más, el volumen se monta de solo lectura. Esto lo arregla.
 *
 * Cómo trabaja (nada se escribe hasta el final):
 *   1. Se analiza TODO en memoria: cada bloque que habría que cambiar se copia a un mapa de «bloques
 *      sucios» y se cambia ahí. El disco no se toca. Con «reparar -n» se queda así: dice lo que haría.
 *   2. Si se confirma, se puede guardar antes una copia de los bloques que se van a cambiar tal y como
 *      están ahora en el disco (reparo_guardar_copia; «reparar --deshacer» la vuelve a poner).
 *   3. Se escribe en un orden seguro (reparo_gravar): primero los bloques que estaban libres (copias
 *      de bloques compartidos, lost+found nueva), después carpetas, indirectos e inodos, después los
 *      mapas de bits, la tabla de grupos y, lo último, el superbloque con la marca «desmontado bien».
 *      Si se corta a medias, el volumen sigue marcado como «no se desmontó bien» y se repite.
 *
 * Los pasos (como los de cualquier comprobador de ext2):
 *   0. Superbloque (si el principal está mal, se usa una copia de los grupos 1, 3, 5, 7, 9...) y los
 *      descriptores de grupo (si uno apunta fuera de su grupo, el de la copia de la tabla).
 *   1. Inodos: tipo, fechas, campos que deben ser cero; punteros a bloques fuera del volumen (se quitan:
 *      el archivo queda con un hueco o más corto; con más de 12 es que el inodo es basura y se borra);
 *      enlaces simbólicos y dispositivos que no tienen sentido (se borran); tamaño y número de bloques
 *      (i_blocks); el bloque de atributos y su cuenta de referencias; resize_inode (el 7) se comprueba
 *      y, si está mal, se rehace con la geometría actual.
 *   1B. Bloques que usan dos sitios: se copian (el bloque compartido se duplica en uno libre y cada
 *      inodo se queda con su copia; el de número más alto conserva el original). No se sabe de quién
 *      era de verdad el bloque, así que así no se pierde ninguna de las dos versiones: el informe dice
 *      qué archivos eran para revisarlos. Si es un bloque de los metadatos (mapas, tabla de inodos...),
 *      el archivo recibe la copia y los metadatos se quedan el suyo. Sin sitio libre para copiarlo,
 *      se suelta del archivo (queda un hueco).
 *   2. Carpetas: cada bloque se recorre; las entradas rotas se descartan (las que siguen bien en el
 *      mismo bloque se conservan); «.» y «..» en su sitio; entradas a inodos que no existen o no están
 *      en uso, a inodos reservados, segundos enlaces a una carpeta: fuera; el tipo de la entrada se
 *      corrige; los nombres con «/» o un cero se arreglan.
 *   3. Conexión: cada carpeta tiene que llegar a la raíz; las que no (o los ciclos) van a /lost+found
 *      (se crea si no está); «..» se corrige.
 *   4. Enlaces: cada inodo con los enlaces que tiene de verdad; los archivos en uso sin ninguna carpeta
 *      van a /lost+found con el nombre «#<inodo>» (los vacíos se borran).
 *   5. Mapas de bits de bloques y de inodos (con el relleno del final a 1), libres de cada grupo, de
 *      carpetas, y los totales del superbloque; la marca «desmontado bien», sin errores.
 *
 * Tablas de inodos con basura en grupos vacíos (D397, ampliación del 2026-11-19, el /home del Amilo): si la
 * tabla de inodos de un grupo no se puso a cero al crearlo (un ext2 ampliado sobre restos de otro sistema),
 * tiene «inodos» que son restos de un ext4 viejo o bytes sin sentido. Tratarlos como archivos (lo que hace
 * e2fsck) es peligroso: sus «punteros» pueden caer en bloques de archivos de verdad, que se marcarían como
 * compartidos o, peor, se leerían como bloques indirectos y se «arreglarían» (se cambiaría el contenido de un
 * archivo real). Por eso, antes del paso 1 se buscan los grupos de los que se puede fiar que están vacíos: su
 * mapa de inodos a cero, su descriptor dice todos libres y ninguna carpeta, y ningún inodo del primer bloque
 * de su tabla parece de verdad (el kernel da siempre el inodo libre más bajo del grupo: un archivo creado
 * justo antes de un corte, con el mapa sin grabar, estaría ahí). En esos grupos los inodos no se interpretan:
 * los bloques de la tabla que no son todo ceros se ponen a cero al escribir (sin pasar por los «sucios»: en
 * un disco grande podrían ser cientos de MB) y sus inodos cuentan como libres. Si después una carpeta de
 * verdad tiene una entrada a uno de esos inodos, el grupo deja de ser de fiar y el análisis se repite entero
 * tratándolo como cualquier otro.
 *
 * Solo ext2 «puro», lo que hace mke2fs para ext2 y lo que escribe SaguiOS: sin diario, extents,
 * flex_bg, sumas ni 64 bits (eso, e2fsck desde Linux). */
#include "particoes.h"

#define REP_LINHAS      40        /* avisos detallados por paso; el resto solo se cuentan */
#define REP_ILEGAIS_MAX 12        /* con más punteros fuera del volumen, el inodo es basura */

#define CP_COMPAT_OK    (0x0001u | 0x0002u | 0x0008u | 0x0010u | 0x0020u)   /* prealloc, imagic, xattr, resize_inode, dir_index */
#define CP_RESIZE       0x0010u
#define CP_DIR_INDEX    0x0020u
#define IC_FILETYPE     0x0002u
#define IC_OK           IC_FILETYPE
#define RO_SPARSE       0x0001u
#define RO_LARGE        0x0002u
#define RO_OK           (0x0001u | 0x0002u | 0x0004u | 0x0040u)              /* sparse, large_file, btree_dir, extra_isize */

#define FL_INDEX        0x00001000u
#define FL_EXTENTS      0x00080000u

enum { CL_NOVO, CL_DADOS, CL_MAPA, CL_GD, CL_N };
enum { E_LIVRE = 0, E_ARQ = 1, E_PASTA = 2, E_OUTRO = 3, E_RESERV = 4, E_TIPO = 7, F_AFETADO = 0x10, F_DUP = 0x20, F_NOMEADO = 0x40,
       F_LIXO = 0x80   /* un inodo que no es todo ceros en un grupo vacío (basura que se pone a cero) */ };

typedef struct { uint32_t b; uint8_t classe; uint8_t *d; } sujo_t;
typedef struct {
    uint32_t ino, pai, dotdot, pp_bloco, pp_desl, marca;
    uint16_t nsub;
    bool conectada, tem_pp;
    char nome[48];
} pasta_t;
typedef struct { uint32_t b, refs; } ea_t;
typedef struct { uint32_t b, restantes; bool fixo; } dupl_t;
typedef struct { uint32_t ino, pasta; char nome[48]; } nomeado_t;

struct reparo {
    char vol[16];
    reparo_aviso_t aviso;
    void *ctx;
    uint8_t sb[1024], sb_orig[1024];
    bool sb_copia;
    uint32_t sb_grupo;
    uint32_t setores, bs, spb, fdb, bpg, ipg, isz, itb, blocos, grupos, inodes, gdt, rgdt, ino1, apb;
    uint32_t compat, incompat, ro;
    bool sparse, resize, filetype, dir_index, large;
    uint8_t *gd, *gd_orig;
    uint8_t *usado, *dup, *fixo;    /* mapas por bloque (número absoluto) */
    uint32_t ndup;
    uint8_t *est;                   /* por inodo (n - 1): E_* | F_* */
    uint16_t *refs;
    pasta_t *pastas; uint32_t np, cap_p;
    ea_t *ea; uint32_t nea, cap_ea;
    dupl_t *dupl; uint32_t ndupl;
    nomeado_t *nomes; uint32_t nn, cap_n;
    sujo_t *suj; uint32_t ns, cap_s;
    uint32_t *hash; uint32_t cap_h;
    uint32_t cursor, cursor_i, lf, agora;
    uint32_t problemas, linhas;
    int paso;
    uint32_t c_fora, c_clon, c_solto, c_borr, c_ent, c_lf, c_enl, c_mapa, c_cont, c_outros;
    uint8_t *tmp, *lixo;
    uint8_t *ft;                    /* por inodo: el tipo de entrada de carpeta (1 archivo, 2 carpeta...) */
    bool fatal, resize_pendente;
    char erro[200];
    /* D397 (2026-11-19): grupos vacíos con basura en la tabla de inodos */
    uint8_t *vazio;                 /* por grupo: 1 = se fía de que no tiene inodos en uso */
    uint8_t *desconfiar;            /* por grupo: 1 = no fiarse (de un análisis anterior que se repite) */
    uint8_t *zerar;                 /* por bloque de tabla (g * itb + k): no es todo ceros y se pone a cero */
    uint32_t nzerar, c_lixo, g_lixo;
    bool repetir, copia_lixo;
};

static uint32_t le16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void po16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void po32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }
static bool bit_de(const uint8_t *m, uint32_t i) { return (m[i / 8] >> (i % 8)) & 1; }
static void bit_pon(uint8_t *m, uint32_t i) { m[i / 8] |= (uint8_t)(1u << (i % 8)); }
static size_t comprimento(const char *s) { size_t n = 0; while (s[n]) n++; return n; }

static void txt(char *m, int max, const char *fmt, ...)
{
    if (!m || max <= 0) return;
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(m, (size_t)max, fmt, ap);
    va_end(ap);
}

/* Crecer un vector (sin realoca: así compila igual en el anfitrión, h397). */
static bool crescer(void **v, uint32_t *cap, uint32_t n, size_t tam)
{
    if (n < *cap) return true;
    uint32_t novo = *cap ? *cap * 2 : 64;
    void *p = aloca((size_t)novo * tam);
    if (!p) return false;
    if (*v) { memcpy(p, *v, (size_t)*cap * tam); libera(*v); }
    *v = p;
    *cap = novo;
    return true;
}

static void falhar(reparo_t *r, const char *fmt, ...)
{
    if (r->fatal) return;
    r->fatal = true;
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(r->erro, sizeof r->erro, fmt, ap);
    va_end(ap);
}

static void dizer(reparo_t *r, const char *linha)
{
    if (r->aviso) r->aviso(linha, r->ctx);
}

/* Un problema encontrado (y lo que se hace con él): cuenta siempre; se escribe hasta REP_LINHAS por paso. */
static void problema(reparo_t *r, uint32_t *contador, const char *fmt, ...)
{
    r->problemas++;
    if (contador) (*contador)++;
    if (!r->aviso) return;
    r->linhas++;
    if (r->linhas > REP_LINHAS + 1) return;
    char s[300];
    if (r->linhas == REP_LINHAS + 1) { dizer(r, "  (hay más; no se escriben uno a uno)"); return; }
    s[0] = ' ';
    s[1] = ' ';
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(s + 2, sizeof s - 2, fmt, ap);
    va_end(ap);
    dizer(r, s);
}

static void comecar_paso(reparo_t *r, int paso, const char *titulo)
{
    r->paso = paso;
    r->linhas = 0;
    dizer(r, titulo);
}

/* ------------------------------------------------------------ bloques: el disco y los sucios */

static int ler_disco(reparo_t *r, uint32_t b, uint32_t n, void *buf)
{
    return blocos_ler(r->vol, b * r->spb, n * r->spb, buf);
}

static sujo_t *achar_sujo(reparo_t *r, uint32_t b)
{
    if (!r->cap_h) return NULL;
    uint32_t m = r->cap_h - 1, i = (b * 2654435761u) & m;
    while (r->hash[i]) {
        sujo_t *s = &r->suj[r->hash[i] - 1];
        if (s->b == b) return s;
        i = (i + 1) & m;
    }
    return NULL;
}

static bool hash_por(reparo_t *r, uint32_t indice)
{
    uint32_t m = r->cap_h - 1, i = (r->suj[indice].b * 2654435761u) & m;
    while (r->hash[i]) i = (i + 1) & m;
    r->hash[i] = indice + 1;
    return true;
}

static bool bloco_a_zerar(reparo_t *r, uint32_t b);

static int ler_bloco(reparo_t *r, uint32_t b, uint8_t *buf)
{
    sujo_t *s = achar_sujo(r, b);
    if (s) { memcpy(buf, s->d, r->bs); return 0; }
    if (bloco_a_zerar(r, b)) { memset(buf, 0, r->bs); return 0; }   /* basura que se pone a cero: se ve ya a cero */
    int x = ler_disco(r, b, 1, buf);
    if (x < 0) falhar(r, "error de lectura en el bloque %u de %s", b, r->vol);
    return x;
}

/* El bloque b para cambiarlo (se lee del disco la primera vez; CL_NOVO: estaba libre, empieza a cero).
 * Con un error devuelve un búfer de desecho y el análisis se para (r->fatal). */
static uint8_t *sujar(reparo_t *r, uint32_t b, int classe)
{
    sujo_t *s = achar_sujo(r, b);
    if (s) {
        if (classe > s->classe && s->classe != CL_NOVO) s->classe = (uint8_t)classe;
        return s->d;
    }
    if (r->fatal) return r->lixo;
    if (!crescer((void **)&r->suj, &r->cap_s, r->ns, sizeof(sujo_t))) { falhar(r, "sin memoria"); return r->lixo; }
    if ((r->ns + 1) * 2 > r->cap_h) {   /* la tabla de búsqueda, al doble */
        uint32_t cap = r->cap_h ? r->cap_h * 2 : 256;
        uint32_t *h = aloca_zerado(cap * sizeof(uint32_t));
        if (!h) { falhar(r, "sin memoria"); return r->lixo; }
        if (r->hash) libera(r->hash);
        r->hash = h;
        r->cap_h = cap;
        for (uint32_t i = 0; i < r->ns; i++) hash_por(r, i);
    }
    uint8_t *d = aloca(r->bs);
    if (!d) { falhar(r, "sin memoria"); return r->lixo; }
    if (classe == CL_NOVO || bloco_a_zerar(r, b)) memset(d, 0, r->bs);
    else if (ler_disco(r, b, 1, d) < 0) { libera(d); falhar(r, "error de lectura en el bloque %u de %s", b, r->vol); return r->lixo; }
    r->suj[r->ns].b = b;
    r->suj[r->ns].classe = (uint8_t)classe;
    r->suj[r->ns].d = d;
    hash_por(r, r->ns);
    r->ns++;
    return d;
}

/* ------------------------------------------------------------ geometría */

static bool pot(uint32_t g, uint32_t b) { while (g > 1 && g % b == 0) g /= b; return g == 1; }
static bool tem_sb(const reparo_t *r, uint32_t g) { return !r->sparse || g == 0 || g == 1 || pot(g, 3) || pot(g, 5) || pot(g, 7); }
static uint32_t ini_grupo(const reparo_t *r, uint32_t g) { return r->fdb + g * r->bpg; }
static uint32_t tam_grupo(const reparo_t *r, uint32_t g)
{
    uint32_t i = ini_grupo(r, g);
    return r->blocos - i < r->bpg ? r->blocos - i : r->bpg;
}
static uint8_t *gd(reparo_t *r, uint32_t g) { return r->gd + g * 32; }
static uint32_t cab_grupo(const reparo_t *r, uint32_t g) { return tem_sb(r, g) ? 1 + r->gdt + r->rgdt : 0; }

/* ¿El bloque b es de la tabla de inodos de un grupo vacío y tiene basura (se pondrá a cero al escribir)? */
static bool bloco_a_zerar(reparo_t *r, uint32_t b)
{
    if (!r->nzerar || b < r->fdb) return false;
    uint32_t g = (b - r->fdb) / r->bpg;
    if (g >= r->grupos || !r->vazio[g]) return false;
    uint32_t it = le32(gd(r, g) + 8);
    return b >= it && b < it + r->itb && bit_de(r->zerar, g * r->itb + (b - it));
}

static void pos_inodo(reparo_t *r, uint32_t n, uint32_t *b, uint32_t *desl)
{
    uint32_t g = (n - 1) / r->ipg, i = (n - 1) % r->ipg;
    *b = le32(gd(r, g) + 8) + (i * r->isz) / r->bs;
    *desl = (i * r->isz) % r->bs;
}

static int ler_inodo(reparo_t *r, uint32_t n, uint8_t *in)
{
    uint32_t b, d;
    pos_inodo(r, n, &b, &d);
    if (ler_bloco(r, b, r->tmp) < 0) return -1;
    memcpy(in, r->tmp + d, r->isz);
    return 0;
}

static void gravar_inodo(reparo_t *r, uint32_t n, const uint8_t *in)
{
    uint32_t b, d;
    pos_inodo(r, n, &b, &d);
    memcpy(sujar(r, b, CL_DADOS) + d, in, r->isz);
}

#define I_MODO(in)    le16((in) + 0)
#define I_TAM(in)     le32((in) + 4)
#define I_DTIME(in)   le32((in) + 20)
#define I_LIG(in)     le16((in) + 26)
#define I_SETORES(in) le32((in) + 28)
#define I_FLAGS(in)   le32((in) + 32)
#define I_BLOCO(in, k) le32((in) + 40 + 4 * (k))
#define I_ACL(in)     le32((in) + 104)
#define I_TAM_ALTO(in) le32((in) + 108)

static int tipo_entrada(uint32_t modo)
{
    switch (modo & 0xF000) {
    case 0x8000: return 1;
    case 0x4000: return 2;
    case 0x2000: return 3;
    case 0x6000: return 4;
    case 0x1000: return 5;
    case 0xC000: return 6;
    case 0xA000: return 7;
    }
    return 0;
}

/* ------------------------------------------------------------ asignar bloques e inodos libres */

static bool marcar(reparo_t *r, uint32_t b, uint32_t ino)
{
    if (bit_de(r->usado, b)) {
        if (!bit_de(r->dup, b)) { bit_pon(r->dup, b); r->ndup++; }
        if (ino) r->est[ino - 1] |= F_DUP;
        return false;
    }
    bit_pon(r->usado, b);
    return true;
}

static uint32_t alocar_bloco(reparo_t *r)
{
    if (r->cursor < r->fdb || r->cursor >= r->blocos) r->cursor = r->fdb;
    for (uint32_t k = 0, b = r->cursor; k < r->blocos - r->fdb; k++, b++) {
        if (b >= r->blocos) b = r->fdb;
        if (!bit_de(r->usado, b)) {
            bit_pon(r->usado, b);
            r->cursor = b + 1;
            return b;
        }
    }
    return 0;
}

static uint32_t alocar_inodo(reparo_t *r)
{
    for (uint32_t n = r->ino1; n <= r->inodes; n++)
        if ((r->est[n - 1] & E_TIPO) == E_LIVRE) return n;
    return 0;
}

/* ------------------------------------------------------------ superbloque y descriptores (paso 0) */

static bool sb_coerente(const uint8_t *sb, uint32_t setores)
{
    if (le16(sb + 56) != 0xEF53) return false;
    uint32_t log = le32(sb + 24), rev = le32(sb + 76);
    if (log > 2) return false;
    uint32_t bs = 1024u << log, bpg = le32(sb + 32), ipg = le32(sb + 40), isz = rev ? le16(sb + 88) : 128;
    uint32_t blocos = le32(sb + 4), fdb = le32(sb + 20);
    if (fdb != (bs == 1024 ? 1u : 0u) || !bpg || bpg > bs * 8 || bpg % 8 || !ipg || ipg > bs * 8) return false;
    if (isz < 128 || isz > bs || (isz & (isz - 1))) return false;
    if (blocos <= fdb + 1 || (uint64_t)blocos * (bs / 512) > setores) return false;
    uint32_t grupos = (blocos - fdb + bpg - 1) / bpg;
    if ((uint64_t)ipg * grupos != le32(sb + 0)) return false;
    if (rev && (le32(sb + 84) < 11 || le32(sb + 84) >= le32(sb + 0))) return false;
    return true;
}

/* El principal no vale: se busca una copia en los grupos 1, 3, 5, 7, 9, 25, 27, 49 (con cada tamaño de
 * bloque posible y los bloques por grupo de mke2fs). */
static bool buscar_copia_sb(reparo_t *r, uint8_t *sb)
{
    static const uint32_t gs[] = { 1, 3, 5, 7, 9, 25, 27, 49, 125, 243, 343 };
    for (uint32_t log = 0; log <= 2; log++) {
        uint32_t bs = 1024u << log, bpg = bs * 8, fdb = log == 0 ? 1 : 0;
        for (unsigned k = 0; k < sizeof gs / sizeof gs[0]; k++) {
            uint64_t setor = ((uint64_t)fdb + (uint64_t)gs[k] * bpg) * (bs / 512);
            if (setor + 2 > r->setores) break;
            if (blocos_ler(r->vol, (uint32_t)setor, 2, sb) < 0) continue;
            if (sb_coerente(sb, r->setores) && le32(sb + 24) == log && le32(sb + 32) == bpg && le16(sb + 90) == gs[k]) {
                r->sb_grupo = gs[k];
                return true;
            }
        }
    }
    return false;
}

static bool gd_valido(reparo_t *r, const uint8_t *d, uint32_t g)
{
    uint32_t ini = ini_grupo(r, g), fim = ini + tam_grupo(r, g), cab = ini + cab_grupo(r, g);
    uint32_t bb = le32(d), ib = le32(d + 4), it = le32(d + 8);
    if (bb < cab || bb >= fim || ib < cab || ib >= fim || it < cab || it + r->itb > fim || it + r->itb < it) return false;
    if (bb == ib || (bb >= it && bb < it + r->itb) || (ib >= it && ib < it + r->itb)) return false;
    return true;
}

static int abrir_vol(reparo_t *r, char *m, int max)
{
    info_bloco_t inf;
    int x = blocos_info(r->vol, &inf);
    if (x < 0) { txt(m, max, "%s: %s", r->vol, nome_erro(x)); return x; }
    r->setores = inf.setores;
    if ((x = blocos_ler(r->vol, 2, 2, r->sb_orig)) < 0) { txt(m, max, "no se puede leer el superbloque de %s: %s", r->vol, nome_erro(x)); return x; }
    memcpy(r->sb, r->sb_orig, 1024);
    if (!sb_coerente(r->sb, r->setores)) {
        if (le16(r->sb + 56) != 0xEF53 && le16(r->sb + 56) != 0) {
            /* ni siquiera la firma: puede no ser un ext2; solo se sigue si hay una copia buena */
        }
        if (!buscar_copia_sb(r, r->sb)) {
            txt(m, max, le16(r->sb_orig + 56) == 0xEF53 ? "el superbloque de %s está dañado y no se encuentra ninguna copia buena"
                                                         : "%s no es un ext2 (ni hay copias de un superbloque de ext2)", r->vol);
            return ERRO_FORMATO;
        }
        r->sb_copia = true;
        po16(r->sb + 90, 0);
    }
    uint8_t *sb = r->sb;
    uint32_t rev = le32(sb + 76);
    r->compat = rev ? le32(sb + 92) : 0;
    r->incompat = rev ? le32(sb + 96) : 0;
    r->ro = rev ? le32(sb + 100) : 0;
    if (r->compat & 0x0004u) { txt(m, max, "%s es un ext3/ext4 (con diario): este reparador solo sabe de ext2; usa e2fsck desde Linux", r->vol); return ERRO_NAO_SUPORTADO; }
    if ((r->compat & ~CP_COMPAT_OK) || (r->incompat & ~IC_OK) || (r->ro & ~RO_OK)) {
        txt(m, max, "%s tiene características que este reparador no conoce (ext4 u otras: %x/%x/%x): usa e2fsck desde Linux",
            r->vol, r->compat, r->incompat, r->ro);
        return ERRO_NAO_SUPORTADO;
    }
    r->bs = 1024u << le32(sb + 24);
    r->spb = r->bs / 512;
    r->apb = r->bs / 4;
    r->fdb = le32(sb + 20);
    r->bpg = le32(sb + 32);
    r->ipg = le32(sb + 40);
    r->isz = rev ? le16(sb + 88) : 128;
    r->ino1 = rev ? le32(sb + 84) : 11;
    r->blocos = le32(sb + 4);
    r->inodes = le32(sb + 0);
    r->sparse = rev && (r->ro & RO_SPARSE);
    r->large = (r->ro & RO_LARGE) != 0;
    r->resize = (r->compat & CP_RESIZE) != 0;
    r->dir_index = (r->compat & CP_DIR_INDEX) != 0;
    r->filetype = (r->incompat & IC_FILETYPE) != 0;
    r->rgdt = r->resize ? le16(sb + 206) : 0;
    r->itb = (r->ipg * r->isz + r->bs - 1) / r->bs;
    r->grupos = (r->blocos - r->fdb + r->bpg - 1) / r->bpg;
    r->gdt = (r->grupos * 32 + r->bs - 1) / r->bs;
    if (r->rgdt > r->apb) { txt(m, max, "el superbloque de %s dice demasiados bloques reservados para la tabla", r->vol); return ERRO_FORMATO; }
    r->gd = aloca(r->gdt * r->bs);
    r->gd_orig = aloca(r->gdt * r->bs);
    if (!r->gd || !r->gd_orig) { txt(m, max, "sin memoria"); return ERRO_SEM_MEMORIA; }
    uint32_t de = r->sb_copia ? ini_grupo(r, r->sb_grupo) + 1 : r->fdb + 1;
    if ((x = ler_disco(r, r->fdb + 1, r->gdt, r->gd_orig)) < 0) { txt(m, max, "no se puede leer la tabla de grupos de %s", r->vol); return x; }
    if ((x = ler_disco(r, de, r->gdt, r->gd)) < 0) { txt(m, max, "no se puede leer la tabla de grupos de %s", r->vol); return x; }
    /* Un descriptor que no cuadra: el de la primera copia de la tabla que sí. */
    uint8_t *copia = NULL;
    for (uint32_t g = 0; g < r->grupos; g++) {
        if (gd_valido(r, gd(r, g), g)) continue;
        if (!copia) {
            copia = aloca(r->gdt * r->bs);
            if (!copia) { txt(m, max, "sin memoria"); return ERRO_SEM_MEMORIA; }
            bool achou = false;
            for (uint32_t c = 1; c < r->grupos && !achou; c++) {
                if (!tem_sb(r, c) || ler_disco(r, ini_grupo(r, c) + 1, r->gdt, copia) < 0) continue;
                achou = gd_valido(r, copia + g * 32, g);
            }
            if (!achou) {
                libera(copia);
                txt(m, max, "el descriptor del grupo %u de %s apunta fuera de su grupo y ninguna copia de la tabla sirve "
                    "(o el volumen usa flex_bg): usa e2fsck desde Linux", g, r->vol);
                return ERRO_FORMATO;
            }
        }
        if (!gd_valido(r, copia + g * 32, g)) {
            libera(copia);
            txt(m, max, "el descriptor del grupo %u de %s está dañado también en la copia: usa e2fsck desde Linux", g, r->vol);
            return ERRO_FORMATO;
        }
        memcpy(gd(r, g), copia + g * 32, 32);
    }
    if (copia) libera(copia);
    return 0;
}

/* ------------------------------------------------------------ paso 1: inodos y sus bloques */

typedef struct {
    reparo_t *r;
    uint32_t n;
    uint8_t *in;
    bool marcar, mudou;
    uint32_t ilegais, nblocos, fim;   /* fim: el último bloque lógico con datos + 1 */
    uint8_t *buf[3];
} percurso_t;

static void por_ponteiro(percurso_t *p, uint32_t pai, uint32_t idx, uint32_t v)
{
    if (!pai) { po32(p->in + 40 + 4 * idx, v); p->mudou = true; }
    else po32(sujar(p->r, pai, CL_DADOS) + 4 * idx, v);
}

static void percorrer(percurso_t *p, uint32_t pai, uint32_t idx, uint32_t b, int nivel, uint32_t logico)
{
    reparo_t *r = p->r;
    if (!b || r->fatal) return;
    if (b < r->fdb || b >= r->blocos) {
        p->ilegais++;
        if (p->marcar) {
            problema(r, &r->c_fora, "inodo %u: apunta al bloque %u, fuera del volumen (%u bloques): se quita%s", p->n, b, r->blocos,
                     nivel ? " (con lo que colgaba de él)" : "");
            r->est[p->n - 1] |= F_AFETADO;
            por_ponteiro(p, pai, idx, 0);
        }
        return;
    }
    if (p->marcar) {
        marcar(r, b, p->n >= r->ino1 ? p->n : 0);
        if (p->n < r->ino1) bit_pon(r->fixo, b);   /* bloques malos y otros reservados: no se copian */
    }
    p->nblocos++;
    if (!nivel) { if (logico + 1 > p->fim) p->fim = logico + 1; return; }
    uint8_t *x = p->buf[nivel - 1];
    if (ler_bloco(r, b, x) < 0) return;
    uint32_t passo = nivel == 1 ? 1 : nivel == 2 ? r->apb : r->apb * r->apb;
    for (uint32_t i = 0; i < r->apb && !r->fatal; i++) {
        uint32_t f = le32(x + 4 * i);
        if (f) percorrer(p, b, i, f, nivel - 1, logico + i * passo);
    }
}

static void percorrer_inodo(percurso_t *p)
{
    reparo_t *r = p->r;
    p->ilegais = p->nblocos = p->fim = 0;
    for (uint32_t k = 0; k < 12; k++) percorrer(p, 0, k, I_BLOCO(p->in, k), 0, k);
    percorrer(p, 0, 12, I_BLOCO(p->in, 12), 1, 12);
    percorrer(p, 0, 13, I_BLOCO(p->in, 13), 2, 12 + r->apb);
    percorrer(p, 0, 14, I_BLOCO(p->in, 14), 3, 12 + r->apb + r->apb * r->apb);
}

static void apagar_inodo(reparo_t *r, uint32_t n, uint8_t *in, const char *motivo)
{
    problema(r, &r->c_borr, "inodo %u: %s: se borra", n, motivo);
    po16(in + 26, 0);
    po32(in + 20, r->agora);
    gravar_inodo(r, n, in);
    r->est[n - 1] = (uint8_t)((r->est[n - 1] & F_AFETADO) | E_LIVRE);
}

static ea_t *achar_ea(reparo_t *r, uint32_t b)
{
    for (uint32_t i = 0; i < r->nea; i++) if (r->ea[i].b == b) return &r->ea[i];
    return NULL;
}

static bool pasta_nova(reparo_t *r, uint32_t n)
{
    if (!crescer((void **)&r->pastas, &r->cap_p, r->np, sizeof(pasta_t))) { falhar(r, "sin memoria"); return false; }
    uint32_t i = r->np;   /* ordenadas por inodo (casi siempre van al final) */
    while (i > 0 && r->pastas[i - 1].ino > n) i--;
    memmove(&r->pastas[i + 1], &r->pastas[i], (r->np - i) * sizeof(pasta_t));
    memset(&r->pastas[i], 0, sizeof(pasta_t));
    r->pastas[i].ino = n;
    r->np++;
    return true;
}

static pasta_t *achar_pasta(reparo_t *r, uint32_t n)
{
    uint32_t a = 0, b = r->np;
    while (a < b) {
        uint32_t m = (a + b) / 2;
        if (r->pastas[m].ino == n) return &r->pastas[m];
        if (r->pastas[m].ino < n) a = m + 1; else b = m;
    }
    return NULL;
}

/* El bloque de atributos extendidos de un inodo: válido (en el volumen, con la firma y un solo
 * bloque) y no compartido con algo que no sea otro bloque de atributos. */
static void verificar_ea(reparo_t *r, uint32_t n, uint8_t *in, bool *mudou)
{
    uint32_t b = I_ACL(in);
    if (!b) return;
    bool ok = b >= r->fdb && b < r->blocos;
    if (ok) {
        ea_t *e = achar_ea(r, b);
        if (e) { e->refs++; return; }
        ok = ler_bloco(r, b, r->tmp) == 0 && le32(r->tmp) == 0xEA020000u && le32(r->tmp + 8) == 1 && !bit_de(r->usado, b);
        if (ok) {
            if (!crescer((void **)&r->ea, &r->cap_ea, r->nea, sizeof(ea_t))) { falhar(r, "sin memoria"); return; }
            r->ea[r->nea].b = b;
            r->ea[r->nea].refs = 1;
            r->nea++;
            marcar(r, b, 0);
            bit_pon(r->fixo, b);
            return;
        }
    }
    problema(r, &r->c_borr, "inodo %u: su bloque de atributos (%u) no es válido: se quita", n, b);
    po32(in + 104, 0);
    *mudou = true;
}

/* resize_inode (el 7): el doble indirecto apunta a los bloques reservados de la tabla del grupo 0 (en la
 * posición de cada uno dentro de la tabla, módulo bloques/4) y cada uno de ellos lista sus copias en los
 * grupos con copia del superbloque. Si no cuadra con la geometría, se rehace. */
static bool resize_valido(reparo_t *r, const uint8_t *in, uint8_t *a, uint8_t *b)
{
    for (int k = 0; k < 15; k++) if (k != 13 && I_BLOCO(in, k)) return false;
    uint32_t dind = I_BLOCO(in, 13);
    if (!dind || dind < r->fdb || dind >= r->blocos || bit_de(r->usado, dind)) return false;
    if (ler_bloco(r, dind, a) < 0) return false;
    for (uint32_t k = 0; k < r->rgdt; k++) {
        uint32_t pos = r->gdt + k, pblk = r->fdb + 1 + pos;
        if (le32(a + (pos % r->apb) * 4) != pblk) return false;
        if (ler_bloco(r, pblk, b) < 0) return false;
        uint32_t j = 0;
        for (uint32_t g = 1; g < r->grupos; g++) {
            if (!tem_sb(r, g)) continue;
            if (j >= r->apb || le32(b + 4 * j) != ini_grupo(r, g) + 1 + pos) return false;
            j++;
        }
    }
    return true;
}

static void rehacer_resize(reparo_t *r, uint8_t *in)
{
    uint32_t dind = I_BLOCO(in, 13), nb = 0;
    for (uint32_t g = 1; g < r->grupos; g++) if (tem_sb(r, g)) nb++;
    if (nb > r->apb) { falhar(r, "resize_inode: demasiados grupos con copia"); return; }
    if (!dind || dind < r->fdb || dind >= r->blocos || bit_de(r->usado, dind)) {
        dind = alocar_bloco(r);
        if (!dind) { falhar(r, "no queda ni un bloque libre para rehacer resize_inode"); return; }
    } else marcar(r, dind, 0);
    bit_pon(r->fixo, dind);
    uint8_t *d = sujar(r, dind, CL_NOVO);
    memset(d, 0, r->bs);
    for (uint32_t k = 0; k < r->rgdt; k++) {
        uint32_t pos = r->gdt + k, pblk = r->fdb + 1 + pos, j = 0;
        po32(d + (pos % r->apb) * 4, pblk);
        uint8_t *x = sujar(r, pblk, CL_DADOS);
        memset(x, 0, r->bs);
        for (uint32_t g = 1; g < r->grupos; g++) if (tem_sb(r, g)) po32(x + 4 * j++, ini_grupo(r, g) + 1 + pos);
    }
    memset(in + 40, 0, 60);
    po32(in + 40 + 13 * 4, dind);
    if (!I_MODO(in)) {   /* el inodo entero se había perdido */
        po16(in + 0, 0x8180);
        po16(in + 26, 1);
        uint64_t tam = ((uint64_t)r->apb * r->apb + r->apb + 12) * r->bs;
        po32(in + 4, (uint32_t)tam);
        po32(in + 108, (uint32_t)(tam >> 32));
        po32(in + 8, r->agora); po32(in + 12, r->agora); po32(in + 16, r->agora);
    }
    po32(in + 28, (1 + r->rgdt * (1 + nb)) * r->spb);
    po32(in + 20, 0);
    gravar_inodo(r, 7, in);
}

/* Un inodo en uso: lo que se comprueba y se arregla de él (paso 1). */
static void inodo_vivo(reparo_t *r, uint32_t n, uint8_t *in, percurso_t *p)
{
    bool mudou = false;
    uint32_t modo = I_MODO(in), tipo = modo & 0xF000;
    if (I_DTIME(in)) {
        problema(r, &r->c_outros, "inodo %u: en uso pero con fecha de borrado: se le quita", n);
        po32(in + 20, 0);
        mudou = true;
    }
    if (!tipo_entrada(modo)) { apagar_inodo(r, n, in, "tipo desconocido (el inodo está estropeado)"); return; }
    r->ft[n - 1] = (uint8_t)tipo_entrada(modo);
    if (I_FLAGS(in) & FL_EXTENTS) { apagar_inodo(r, n, in, "dice usar extents (ext4) en un ext2"); return; }
    if ((I_FLAGS(in) & FL_INDEX) && (tipo != 0x4000 || !r->dir_index)) {
        problema(r, &r->c_outros, "inodo %u: marca de carpeta indexada donde no toca: se quita", n);
        po32(in + 32, I_FLAGS(in) & ~FL_INDEX);
        mudou = true;
    }
    if (le32(in + 112) || le16(in + 116) || le16(in + 118)) {   /* i_faddr, i_blocks_hi, i_file_acl_high: cero en ext2 */
        problema(r, &r->c_outros, "inodo %u: campos que deben ser cero no lo son: se ponen a cero", n);
        po32(in + 112, 0); po16(in + 116, 0); po16(in + 118, 0);
        mudou = true;
    }
    if (tipo == 0x4000 && I_TAM_ALTO(in)) {
        problema(r, &r->c_outros, "inodo %u: carpeta con un tamaño de más de 4 GB: se corrige", n);
        po32(in + 108, 0);
        mudou = true;
    }
    if (tipo == 0x2000 || tipo == 0x6000 || tipo == 0x1000 || tipo == 0xC000) {
        for (int k = 4; k < 15; k++)
            if (I_BLOCO(in, k)) { apagar_inodo(r, n, in, "dispositivo, tubería o socket con datos que no tienen sentido"); return; }
    }
    uint32_t acl_set = I_ACL(in) ? r->spb : 0;
    if (tipo == 0xA000) {
        uint32_t tam = I_TAM(in);
        if (I_SETORES(in) == acl_set) {   /* enlace rápido: el destino va dentro del inodo */
            size_t l = 0;
            while (l < 60 && in[40 + l]) l++;
            if (l == 60 || l != tam) { apagar_inodo(r, n, in, "enlace simbólico con un tamaño que no cuadra"); return; }
            verificar_ea(r, n, in, &mudou);
            r->est[n - 1] = (uint8_t)((r->est[n - 1] & ~E_TIPO) | E_OUTRO);
            if (mudou) gravar_inodo(r, n, in);
            return;
        }
        bool ok = tam > 0 && tam < r->bs && I_BLOCO(in, 0) >= r->fdb && I_BLOCO(in, 0) < r->blocos;
        for (int k = 1; k < 15 && ok; k++) if (I_BLOCO(in, k)) ok = false;
        if (ok && ler_bloco(r, I_BLOCO(in, 0), r->tmp) == 0) {
            size_t l = 0;
            while (l < r->bs && r->tmp[l]) l++;
            ok = l == tam;
        } else ok = false;
        if (!ok) { apagar_inodo(r, n, in, "enlace simbólico estropeado"); return; }
    }
    if (tipo == 0x8000 || tipo == 0x4000 || tipo == 0xA000) {
        p->n = n;
        p->in = in;
        p->marcar = false;
        p->mudou = false;
        percorrer_inodo(p);   /* primero sin tocar nada: ¿cuántos punteros fuera del volumen? */
        if (r->fatal) return;
        if (p->ilegais > REP_ILEGAIS_MAX) {
            char m[80];
            formatar_texto(m, sizeof m, "%u punteros fuera del volumen (el inodo es basura)", p->ilegais);
            apagar_inodo(r, n, in, m);
            return;
        }
        if (tipo == 0x4000 && p->fim == 0) {   /* sin ningún bloque de datos no se sabe qué tenía */
            apagar_inodo(r, n, in, "carpeta sin ningún bloque");
            return;
        }
        p->marcar = true;
        percorrer_inodo(p);
        if (p->mudou) mudou = true;
        verificar_ea(r, n, in, &mudou);
        uint32_t esperado = p->nblocos * r->spb + (I_ACL(in) ? r->spb : 0);
        if (I_SETORES(in) != esperado) {
            problema(r, &r->c_cont, "inodo %u: dice ocupar %u sectores y ocupa %u: se corrige", n, I_SETORES(in), esperado);
            po32(in + 28, esperado);
            mudou = true;
        }
        if (tipo == 0x4000) {
            uint32_t tam = p->fim * r->bs;
            if (I_TAM(in) != tam) {
                problema(r, &r->c_cont, "carpeta %u: tamaño %u, por sus bloques %u: se corrige", n, I_TAM(in), tam);
                po32(in + 4, tam);
                mudou = true;
            }
        } else if (tipo == 0x8000 && p->fim) {
            uint64_t tam = (uint64_t)I_TAM(in) | (r->large ? (uint64_t)I_TAM_ALTO(in) << 32 : 0);
            if (tam < (uint64_t)(p->fim - 1) * r->bs) {
                uint64_t novo = (uint64_t)p->fim * r->bs;
                problema(r, &r->c_cont, "inodo %u: el tamaño (%u) es menor que sus bloques: pasa a %u", n, (uint32_t)tam, (uint32_t)novo);
                po32(in + 4, (uint32_t)novo);
                po32(in + 108, (uint32_t)(novo >> 32));
                mudou = true;
            }
        }
        if (tipo == 0x4000 && !pasta_nova(r, n)) return;
        r->est[n - 1] = (uint8_t)((r->est[n - 1] & ~E_TIPO) | (tipo == 0x4000 ? E_PASTA : tipo == 0x8000 ? E_ARQ : E_OUTRO));
    } else {
        verificar_ea(r, n, in, &mudou);
        uint32_t esperado = I_ACL(in) ? r->spb : 0;
        if (I_SETORES(in) != esperado) {
            problema(r, &r->c_cont, "inodo %u: dice ocupar %u sectores y ocupa %u: se corrige", n, I_SETORES(in), esperado);
            po32(in + 28, esperado);
            mudou = true;
        }
        r->est[n - 1] = (uint8_t)((r->est[n - 1] & ~E_TIPO) | E_OUTRO);
    }
    if (mudou) gravar_inodo(r, n, in);
}

static void marcar_metadados(reparo_t *r)
{
    for (uint32_t g = 0; g < r->grupos; g++) {
        uint32_t ini = ini_grupo(r, g), cab = cab_grupo(r, g);
        if (g == 0 && r->fdb) { bit_pon(r->usado, 0); bit_pon(r->fixo, 0); }   /* el bloque 0 de 1 KB: arranque */
        for (uint32_t k = 0; k < cab; k++) { marcar(r, ini + k, 0); bit_pon(r->fixo, ini + k); }
        uint32_t bb = le32(gd(r, g)), ib = le32(gd(r, g) + 4), it = le32(gd(r, g) + 8);
        marcar(r, bb, 0); bit_pon(r->fixo, bb);
        marcar(r, ib, 0); bit_pon(r->fixo, ib);
        for (uint32_t k = 0; k < r->itb; k++) { marcar(r, it + k, 0); bit_pon(r->fixo, it + k); }
    }
}

/* Recorre la tabla de inodos de cada grupo por trozos y llama a f con cada inodo (su copia). */
typedef void (*por_inodo_t)(reparo_t *r, uint32_t n, uint8_t *in, void *ctx);
static void cada_inodo(reparo_t *r, por_inodo_t f, void *ctx)
{
    uint32_t trozo = 65536 / r->bs;
    if (trozo > r->itb) trozo = r->itb;
    uint8_t *buf = aloca(trozo * r->bs), *in = aloca(r->isz);
    if (!buf || !in) { if (buf) libera(buf); if (in) libera(in); falhar(r, "sin memoria"); return; }
    for (uint32_t g = 0; g < r->grupos && !r->fatal; g++) {
        if (r->vazio[g]) continue;   /* un grupo vacío: sus inodos son libres (y la basura, a cero) */
        uint32_t it = le32(gd(r, g) + 8), por_bloco = r->bs / r->isz;
        for (uint32_t b0 = 0; b0 < r->itb && !r->fatal; b0 += trozo) {
            uint32_t q = r->itb - b0 < trozo ? r->itb - b0 : trozo;
            if (ler_disco(r, it + b0, q, buf) < 0) { falhar(r, "error de lectura en la tabla de inodos del grupo %u", g); break; }
            for (uint32_t k = 0; k < q; k++) {
                sujo_t *s = achar_sujo(r, it + b0 + k);
                if (s) memcpy(buf + k * r->bs, s->d, r->bs);
            }
            for (uint32_t i = 0; i < q * por_bloco && !r->fatal; i++) {
                uint32_t idx = b0 * por_bloco + i;
                if (idx >= r->ipg) break;
                memcpy(in, buf + i * r->isz, r->isz);
                f(r, g * r->ipg + idx + 1, in, ctx);
            }
        }
    }
    libera(buf);
    libera(in);
}

static void paso1_inodo(reparo_t *r, uint32_t n, uint8_t *in, void *ctx)
{
    percurso_t *p = ctx;
    uint32_t lig = I_LIG(in), modo = I_MODO(in);
    if (n < r->ino1) {
        r->est[n - 1] = E_RESERV;
        if (n == 2) {
            if (!lig || (modo & 0xF000) != 0x4000) {
                falhar(r, "la carpeta raíz (inodo 2) está destruida: este reparador no la rehace; usa e2fsck desde Linux");
                return;
            }
            inodo_vivo(r, n, in, p);
            if (!r->fatal && (r->est[n - 1] & E_TIPO) != E_PASTA)
                falhar(r, "la carpeta raíz (inodo 2) está destruida: este reparador no la rehace; usa e2fsck desde Linux");
            return;
        }
        if (n == 7 && r->resize) {
            if (!resize_valido(r, in, p->buf[0], p->buf[1])) {
                problema(r, &r->c_outros, "resize_inode (inodo 7) no cuadra con la geometría: se rehace");
                r->resize_pendente = true;   /* después del paso 1: antes no se sabe qué bloques están libres */
            } else {
                marcar(r, I_BLOCO(in, 13), 0);
                bit_pon(r->fixo, I_BLOCO(in, 13));
            }
            return;
        }
        /* 1 (bloques malos) y los demás reservados con datos: sus bloques están en uso y no se tocan */
        if (n == 1 || ((modo & 0xF000) == 0x8000 && lig)) {
            p->n = n; p->in = in; p->marcar = true; p->mudou = false;
            percorrer_inodo(p);
            if (p->mudou) gravar_inodo(r, n, in);
        }
        return;
    }
    if (!lig) {
        r->est[n - 1] = E_LIVRE;
        uint32_t dt = I_DTIME(in);
        if ((modo && !dt) || (dt && dt < r->inodes)) {
            problema(r, &r->c_outros, "inodo %u: borrado sin fecha de borrado: se le pone", n);
            po32(in + 20, r->agora);
            gravar_inodo(r, n, in);
        }
        return;
    }
    if (!modo) {   /* enlaces pero sin tipo: nada que salvar */
        apagar_inodo(r, n, in, "con enlaces pero vacío");
        return;
    }
    inodo_vivo(r, n, in, p);
}

/* ¿Este inodo podría ser de verdad (un archivo creado justo antes de un corte)? En uso, sin fecha de borrado,
 * de un tipo que existe, sin extents y con sus punteros dentro del volumen. La basura (restos de un ext4, bytes
 * sin sentido) casi nunca lo cumple todo. */
static bool parece_vivo(const reparo_t *r, const uint8_t *in)
{
    uint32_t modo = I_MODO(in), tipo = modo & 0xF000;
    if (!I_LIG(in) || I_DTIME(in) || !tipo_entrada(modo) || (I_FLAGS(in) & FL_EXTENTS)) return false;
    if (tipo == 0xA000 && I_SETORES(in) == 0) return I_TAM(in) > 0 && I_TAM(in) < 60;   /* enlace rápido */
    if (tipo == 0x8000 || tipo == 0x4000 || tipo == 0xA000) {
        for (int k = 0; k < 15; k++) {
            uint32_t b = I_BLOCO(in, k);
            if (b && (b < r->fdb || b >= r->blocos)) return false;
        }
        return tipo != 0x4000 || I_BLOCO(in, 0);
    }
    for (int k = 4; k < 15; k++) if (I_BLOCO(in, k)) return false;
    return true;
}

static bool bloco_vazio(const uint8_t *b, uint32_t n)
{
    const uint32_t *w = (const uint32_t *)b;
    uint32_t acc = 0;
    for (uint32_t i = 0; i < n / 4; i++) acc |= w[i];
    return acc == 0;
}

/* Los grupos vacíos (ver el comentario del principio): mapa de inodos a cero, descriptor con todos libres y
 * ninguna carpeta, y nada que parezca de verdad en el primer bloque de su tabla. De su tabla se anotan los
 * bloques que no son todo ceros (se pondrán a cero) y sus inodos cuentan como libres. El grupo 0 nunca (la raíz
 * y los reservados). */
static void grupos_vazios(reparo_t *r)
{
    uint32_t trozo = 65536 / r->bs, por_bloco = r->bs / r->isz;
    if (trozo > r->itb) trozo = r->itb;
    uint8_t *buf = aloca(trozo * r->bs);
    if (!buf) { falhar(r, "sin memoria"); return; }
    for (uint32_t g = 1; g < r->grupos && !r->fatal; g++) {
        uint8_t *d = gd(r, g);
        if (r->desconfiar[g] || le16(d + 14) != r->ipg || le16(d + 16) != 0 || g * r->ipg + 1 < r->ino1) continue;
        if (ler_disco(r, le32(d + 4), 1, buf) < 0) { falhar(r, "error de lectura en el mapa de inodos del grupo %u", g); break; }
        bool livre = true;
        for (uint32_t i = 0; i < r->ipg && livre; i++) if (bit_de(buf, i)) livre = false;
        if (!livre) continue;
        uint32_t it = le32(d + 8), sujos = 0, inodos = 0;
        bool de_verdade = false;
        for (uint32_t b0 = 0; b0 < r->itb && !r->fatal && !de_verdade; b0 += trozo) {
            uint32_t q = r->itb - b0 < trozo ? r->itb - b0 : trozo;
            if (ler_disco(r, it + b0, q, buf) < 0) { falhar(r, "error de lectura en la tabla de inodos del grupo %u", g); break; }
            for (uint32_t k = 0; k < q; k++) {
                const uint8_t *x = buf + k * r->bs;
                if (bloco_vazio(x, r->bs)) continue;
                if (b0 + k == 0)
                    for (uint32_t i = 0; i < por_bloco && !de_verdade; i++) de_verdade = parece_vivo(r, x + i * r->isz);
                if (de_verdade) break;
                bit_pon(r->zerar, g * r->itb + b0 + k);
                sujos++;
                for (uint32_t i = 0; i < por_bloco; i++) {
                    uint32_t idx = (b0 + k) * por_bloco + i;
                    if (idx < r->ipg && !bloco_vazio(x + i * r->isz, r->isz)) { r->est[g * r->ipg + idx] |= F_LIXO; inodos++; }
                }
            }
        }
        if (r->fatal) break;
        if (de_verdade) {   /* se deshace lo anotado: este grupo se comprueba inodo a inodo, como los demás */
            for (uint32_t k = 0; k < r->itb; k++) {
                r->zerar[(g * r->itb + k) / 8] &= (uint8_t)~(1u << ((g * r->itb + k) % 8));
            }
            for (uint32_t i = 0; i < r->ipg; i++) r->est[g * r->ipg + i] &= (uint8_t)~F_LIXO;
            char s[200];
            formatar_texto(s, sizeof s, "  grupo %u: su mapa dice que no tiene inodos en uso, pero el primero de su tabla parece de verdad: "
                                        "se comprueba inodo a inodo", g);
            dizer(r, s);
            continue;
        }
        r->vazio[g] = 1;
        if (!sujos) continue;
        r->nzerar += sujos;
        r->c_lixo += inodos;
        r->g_lixo++;
        problema(r, NULL, "grupo %u: no tiene inodos en uso, pero su tabla de inodos tiene basura (%u inodos, restos de otro sistema "
                 "de archivos): se pone a cero (%u KB)", g, inodos, sujos * (r->bs / 1024));
    }
    libera(buf);
}

static void paso1(reparo_t *r)
{
    comecar_paso(r, 1, "Paso 1: inodos y sus bloques");
    marcar_metadados(r);
    grupos_vazios(r);
    percurso_t p;
    memset(&p, 0, sizeof p);
    p.r = r;
    for (int i = 0; i < 3; i++) p.buf[i] = aloca(r->bs);
    if (!p.buf[0] || !p.buf[1] || !p.buf[2]) falhar(r, "sin memoria");
    else cada_inodo(r, paso1_inodo, &p);
    for (int i = 0; i < 3; i++) if (p.buf[i]) libera(p.buf[i]);
    if (r->resize_pendente && !r->fatal) {
        uint8_t *in = aloca(r->isz);
        if (!in) falhar(r, "sin memoria");
        else if (ler_inodo(r, 7, in) == 0) rehacer_resize(r, in);
        if (in) libera(in);
    }
    /* Las cuentas de referencias de los bloques de atributos. */
    for (uint32_t i = 0; i < r->nea && !r->fatal; i++) {
        uint8_t *x = r->tmp;
        if (ler_bloco(r, r->ea[i].b, x) < 0) break;
        if (le32(x + 4) != r->ea[i].refs) {
            problema(r, &r->c_cont, "bloque de atributos %u: dice %u referencias y tiene %u: se corrige", r->ea[i].b, le32(x + 4), r->ea[i].refs);
            po32(sujar(r, r->ea[i].b, CL_DADOS) + 4, r->ea[i].refs);
        }
    }
}

/* ------------------------------------------------------------ paso 1B: bloques compartidos */

static dupl_t *achar_dupl(reparo_t *r, uint32_t b)
{
    uint32_t a = 0, z = r->ndupl;
    while (a < z) {
        uint32_t m = (a + z) / 2;
        if (r->dupl[m].b == b) return &r->dupl[m];
        if (r->dupl[m].b < b) a = m + 1; else z = m;
    }
    return NULL;
}

typedef struct { reparo_t *r; uint32_t n; uint8_t *in; uint8_t *buf[3]; bool achou, mudou; } dup_t;

static void contar_dup(dup_t *d, uint32_t b, int nivel)
{
    reparo_t *r = d->r;
    if (!b || b < r->fdb || b >= r->blocos || r->fatal) return;
    if (bit_de(r->dup, b)) {
        dupl_t *x = achar_dupl(r, b);
        if (x) { x->restantes++; d->achou = true; }
    }
    if (!nivel) return;
    uint8_t *buf = d->buf[nivel - 1];
    if (ler_bloco(r, b, buf) < 0) return;
    for (uint32_t i = 0; i < r->apb; i++) contar_dup(d, le32(buf + 4 * i), nivel - 1);
}

static void contar_dup_inodo(reparo_t *r, uint32_t n, uint8_t *in, void *ctx)
{
    dup_t *d = ctx;
    uint32_t t = r->est[n - 1] & E_TIPO;
    if (n < r->ino1 || (t != E_ARQ && t != E_PASTA && t != E_OUTRO)) return;
    uint32_t tipo = I_MODO(in) & 0xF000;
    if (tipo != 0x8000 && tipo != 0x4000 && tipo != 0xA000) return;
    if (tipo == 0xA000 && I_SETORES(in) == (I_ACL(in) ? r->spb : 0)) return;
    d->achou = false;
    for (int k = 0; k < 12; k++) contar_dup(d, I_BLOCO(in, k), 0);
    contar_dup(d, I_BLOCO(in, 12), 1);
    contar_dup(d, I_BLOCO(in, 13), 2);
    contar_dup(d, I_BLOCO(in, 14), 3);
    if (d->achou) r->est[n - 1] |= F_DUP | F_AFETADO;
}

static void clonar(dup_t *d, uint32_t pai, uint32_t idx, uint32_t b, int nivel)
{
    reparo_t *r = d->r;
    if (!b || b < r->fdb || b >= r->blocos || r->fatal) return;
    uint32_t atual = b;
    dupl_t *x = bit_de(r->dup, b) ? achar_dupl(r, b) : NULL;
    if (x && (x->restantes > 1 || x->fixo)) {
        uint32_t nb = alocar_bloco(r);
        x->restantes--;
        if (!nb) {
            problema(r, &r->c_solto, "inodo %u: el bloque %u lo usa también %s y no queda sitio para copiarlo: se quita de este archivo",
                     d->n, b, x->fixo ? "el sistema de archivos" : "otro archivo");
            if (!pai) { po32(d->in + 40 + 4 * idx, 0); d->mudou = true; } else po32(sujar(r, pai, CL_DADOS) + 4 * idx, 0);
            return;
        }
        if (ler_bloco(r, b, r->tmp) < 0) return;
        memcpy(sujar(r, nb, CL_NOVO), r->tmp, r->bs);
        if (!pai) { po32(d->in + 40 + 4 * idx, nb); d->mudou = true; } else po32(sujar(r, pai, CL_DADOS) + 4 * idx, nb);
        problema(r, &r->c_clon, "inodo %u: el bloque %u lo usa también %s: este archivo se queda una copia (bloque %u)",
                 d->n, b, x->fixo ? "el sistema de archivos (metadatos)" : "otro archivo", nb);
        atual = nb;
    } else if (x) x->restantes--;
    if (!nivel) return;
    uint8_t *buf = d->buf[nivel - 1];
    if (ler_bloco(r, atual, buf) < 0) return;
    for (uint32_t i = 0; i < r->apb; i++) {
        uint32_t f = le32(buf + 4 * i);
        if (f) clonar(d, atual, i, f, nivel - 1);
    }
}

static void clonar_inodo(reparo_t *r, uint32_t n, uint8_t *in, void *ctx)
{
    dup_t *d = ctx;
    if (!(r->est[n - 1] & F_DUP) || n < r->ino1) return;
    uint32_t t = r->est[n - 1] & E_TIPO;
    if (t != E_ARQ && t != E_PASTA && t != E_OUTRO) return;
    d->n = n;
    d->in = in;
    d->mudou = false;
    for (uint32_t k = 0; k < 12; k++) clonar(d, 0, k, I_BLOCO(in, k), 0);
    clonar(d, 0, 12, I_BLOCO(in, 12), 1);
    clonar(d, 0, 13, I_BLOCO(in, 13), 2);
    clonar(d, 0, 14, I_BLOCO(in, 14), 3);
    if (d->mudou) gravar_inodo(r, n, in);
}

static void paso1b(reparo_t *r)
{
    if (!r->ndup || r->fatal) return;
    comecar_paso(r, 2, "Paso 1B: bloques que usan dos sitios");
    r->dupl = aloca_zerado(r->ndup * sizeof(dupl_t));
    if (!r->dupl) { falhar(r, "sin memoria"); return; }
    for (uint32_t b = 0; b < r->blocos && r->ndupl < r->ndup; b++)
        if (bit_de(r->dup, b)) {
            r->dupl[r->ndupl].b = b;
            r->dupl[r->ndupl].fixo = bit_de(r->fixo, b);
            r->dupl[r->ndupl].restantes = r->dupl[r->ndupl].fixo ? 1 : 0;
            r->ndupl++;
        }
    dup_t d;
    memset(&d, 0, sizeof d);
    d.r = r;
    for (int i = 0; i < 3; i++) d.buf[i] = aloca(r->bs);
    if (!d.buf[0] || !d.buf[1] || !d.buf[2]) falhar(r, "sin memoria");
    else {
        cada_inodo(r, contar_dup_inodo, &d);
        /* Los de más de un inodo (o de uno y los metadatos): cada inodo menos el último se queda una copia. */
        cada_inodo(r, clonar_inodo, &d);
    }
    for (int i = 0; i < 3; i++) if (d.buf[i]) libera(d.buf[i]);
}

/* ------------------------------------------------------------ bloques de una carpeta */

/* Los bloques de datos de un inodo por orden lógico: v[l] = físico (0 = hueco), *n = cuántos. */
static void mapa_rec(reparo_t *r, uint32_t b, int nivel, uint32_t logico, uint32_t *v, uint32_t max, uint8_t **buf)
{
    if (!b || b < r->fdb || b >= r->blocos || logico >= max || r->fatal) return;
    if (!nivel) { v[logico] = b; return; }
    uint8_t *x = buf[nivel - 1];
    if (ler_bloco(r, b, x) < 0) return;
    uint32_t passo = nivel == 1 ? 1 : nivel == 2 ? r->apb : r->apb * r->apb;
    for (uint32_t i = 0; i < r->apb; i++) {
        uint32_t f = le32(x + 4 * i);
        if (f) mapa_rec(r, f, nivel - 1, logico + i * passo, v, max, buf);
    }
}

static uint32_t *mapa_logico(reparo_t *r, const uint8_t *in, uint32_t n)
{
    uint32_t *v = aloca_zerado((n ? n : 1) * sizeof(uint32_t));
    uint8_t *buf[3] = { aloca(r->bs), aloca(r->bs), aloca(r->bs) };
    if (!v || !buf[0] || !buf[1] || !buf[2]) { falhar(r, "sin memoria"); goto fim; }
    for (uint32_t k = 0; k < 12; k++) if (k < n && I_BLOCO(in, k) >= r->fdb && I_BLOCO(in, k) < r->blocos) v[k] = I_BLOCO(in, k);
    mapa_rec(r, I_BLOCO(in, 12), 1, 12, v, n, buf);
    mapa_rec(r, I_BLOCO(in, 13), 2, 12 + r->apb, v, n, buf);
    mapa_rec(r, I_BLOCO(in, 14), 3, 12 + r->apb + r->apb * r->apb, v, n, buf);
fim:
    for (int i = 0; i < 3; i++) if (buf[i]) libera(buf[i]);
    return v;
}

/* Pone el bloque b como el lógico l de un inodo (y crea los indirectos que falten). Suma los sectores. */
static bool por_logico(reparo_t *r, uint8_t *in, uint32_t l, uint32_t b)
{
    if (l < 12) { po32(in + 40 + 4 * l, b); return true; }
    l -= 12;
    uint32_t nivel, k, idx;
    if (l < r->apb) { nivel = 1; k = 12; }
    else if ((l -= r->apb) < r->apb * r->apb) { nivel = 2; k = 13; }
    else return false;
    uint32_t topo = I_BLOCO(in, k);
    if (!topo) {
        if (!(topo = alocar_bloco(r))) return false;
        sujar(r, topo, CL_NOVO);
        po32(in + 40 + 4 * k, topo);
        po32(in + 28, I_SETORES(in) + r->spb);
    }
    if (nivel == 2) {
        idx = l / r->apb;
        uint32_t ind = le32(sujar(r, topo, CL_DADOS) + 4 * idx);
        if (!ind) {
            if (!(ind = alocar_bloco(r))) return false;
            sujar(r, ind, CL_NOVO);
            po32(sujar(r, topo, CL_DADOS) + 4 * idx, ind);
            po32(in + 28, I_SETORES(in) + r->spb);
        }
        topo = ind;
        l %= r->apb;
    }
    po32(sujar(r, topo, CL_DADOS) + 4 * l, b);
    return true;
}

static uint32_t tam_entrada(uint32_t nl) { return (8 + nl + 3) & ~3u; }

/* Añade la entrada «nome» -> ino a la carpeta d (en un hueco de sus bloques, o en un bloque nuevo). */
static bool adicionar_entrada(reparo_t *r, uint32_t d, const char *nome, uint32_t ino, int tipo)
{
    uint8_t *in = aloca(r->isz), *buf = aloca(r->bs);
    uint32_t *v = NULL;
    bool ok = false;
    if (!in || !buf) { falhar(r, "sin memoria"); goto fim; }
    if (ler_inodo(r, d, in) < 0) goto fim;
    uint32_t nl = (uint32_t)comprimento(nome), precisa = tam_entrada(nl), nblk = I_TAM(in) / r->bs;
    v = mapa_logico(r, in, nblk);
    if (!v) goto fim;
    for (uint32_t l = 0; l < nblk && !ok && !r->fatal; l++) {
        if (!v[l] || ler_bloco(r, v[l], buf) < 0) continue;
        for (uint32_t off = 0; off + 8 <= r->bs;) {
            uint8_t *e = buf + off;
            uint32_t rl = le16(e + 4), enl = r->filetype ? e[6] : le16(e + 6);
            if (rl < 8 || off + rl > r->bs) break;
            uint32_t usado = le32(e) ? tam_entrada(enl) : 0, novo = off + usado;
            if (rl - usado >= precisa) {
                uint8_t *x = buf + novo;
                if (usado) po16(e + 4, usado);
                po32(x, ino);
                po16(x + 4, rl - usado);
                x[6] = (uint8_t)nl;
                x[7] = r->filetype ? (uint8_t)tipo : 0;
                memcpy(x + 8, nome, nl);
                memcpy(sujar(r, v[l], CL_DADOS), buf, r->bs);
                ok = true;
                break;
            }
            off += rl;
        }
    }
    if (!ok && !r->fatal) {   /* un bloque más para la carpeta */
        uint32_t b = alocar_bloco(r);
        if (b && por_logico(r, in, nblk, b)) {
            uint8_t *x = sujar(r, b, CL_NOVO);
            memset(x, 0, r->bs);
            po32(x, ino);
            po16(x + 4, r->bs);
            x[6] = (uint8_t)nl;
            x[7] = r->filetype ? (uint8_t)tipo : 0;
            memcpy(x + 8, nome, nl);
            po32(in + 4, (nblk + 1) * r->bs);
            po32(in + 28, I_SETORES(in) + r->spb);
            ok = true;
        }
    }
    if (ok) {
        if (I_FLAGS(in) & FL_INDEX) po32(in + 32, I_FLAGS(in) & ~FL_INDEX);   /* el índice ya no tendría la entrada nueva */
        gravar_inodo(r, d, in);
    }
fim:
    if (in) libera(in);
    if (buf) libera(buf);
    if (v) libera(v);
    return ok;
}

/* ------------------------------------------------------------ paso 2: carpetas */

static bool entrada_ok(reparo_t *r, const uint8_t *buf, uint32_t off)
{
    if (off + 8 > r->bs) return false;
    const uint8_t *e = buf + off;
    uint32_t rl = le16(e + 4), nl = r->filetype ? e[6] : le16(e + 6);
    return rl >= 8 && !(rl & 3) && off + rl <= r->bs && nl + 8 <= rl && le32(e) <= r->inodes && (nl || !le32(e));
}

/* Rehace la cadena de entradas de un bloque: lo que no se entiende se junta con la entrada anterior
 * (o pasa a ser una entrada vacía), hasta la siguiente entrada que tenga sentido. */
static bool salvar_cadeia(reparo_t *r, uint32_t pasta, uint8_t *buf)
{
    bool mudou = false;
    int32_t ant = -1;
    for (uint32_t off = 0; off < r->bs;) {
        if (entrada_ok(r, buf, off)) { ant = (int32_t)off; off += le16(buf + off + 4); continue; }
        uint32_t prox = off + 8;
        while (prox < r->bs && !entrada_ok(r, buf, prox)) prox += 4;
        if (prox > r->bs) prox = r->bs;
        problema(r, &r->c_ent, "carpeta %u: entradas rotas (desde el byte %u de un bloque): se descartan", pasta, off);
        if (ant >= 0) po16(buf + ant + 4, prox - (uint32_t)ant);
        else {
            memset(buf + off, 0, 8);
            po16(buf + off + 4, prox - off);
            ant = (int32_t)off;
        }
        mudou = true;
        off = prox;
    }
    return mudou;
}

static void anotar_nome(reparo_t *r, uint32_t ino, uint32_t pasta, const uint8_t *nome, uint32_t nl)
{
    if (!(r->est[ino - 1] & F_AFETADO) || (r->est[ino - 1] & F_NOMEADO)) return;
    if (!crescer((void **)&r->nomes, &r->cap_n, r->nn, sizeof(nomeado_t))) return;
    nomeado_t *x = &r->nomes[r->nn++];
    x->ino = ino;
    x->pasta = pasta;
    if (nl > sizeof x->nome - 1) nl = sizeof x->nome - 1;
    memcpy(x->nome, nome, nl);
    x->nome[nl] = 0;
    r->est[ino - 1] |= F_NOMEADO;
}

static void remover_entrada(uint8_t *buf, int32_t ant, uint32_t off)
{
    if (ant >= 0) po16(buf + ant + 4, le16(buf + ant + 4) + le16(buf + off + 4));
    else po32(buf + off, 0);
}

/* «.» y «..» al principio del primer bloque. Devuelve el desplazamiento donde siguen las entradas normales. */
static uint32_t pontos(reparo_t *r, pasta_t *pa, uint32_t fis, uint8_t *buf, bool *mudou)
{
    uint8_t *e = buf;
    uint32_t rl = le16(e + 4);
    if (!(e[6] == 1 && e[8] == '.' && le32(e) == pa->ino)) {
        if (le32(e) && !(e[6] == 1 && e[8] == '.'))
            problema(r, &r->c_ent, "carpeta %u: la primera entrada no es «.»: se pone (lo que había irá a lost+found si no tiene otra)", pa->ino);
        else problema(r, &r->c_ent, "carpeta %u: «.» mal: se corrige", pa->ino);
        po32(e, pa->ino);
        e[6] = 1; e[7] = r->filetype ? 2 : 0; e[8] = '.';
        if (!r->filetype) e[7] = 0;
        *mudou = true;
    }
    while (rl < 12 && rl < r->bs) {   /* demasiado pequeña para «.»: se junta con la siguiente */
        rl += le16(buf + rl + 4);
        po16(e + 4, rl);
        *mudou = true;
    }
    if (e[7] != (r->filetype ? 2 : 0)) { e[7] = r->filetype ? 2 : 0; *mudou = true; }
    if (rl > 12 && rl - 12 >= 12) {   /* «.» ocupa todo: se parte para hacer sitio a «..» */
        po16(e + 4, 12);
        uint8_t *x = buf + 12;
        memset(x, 0, 12);
        po16(x + 4, rl - 12);
        rl = 12;
        *mudou = true;
    }
    uint32_t off = rl;
    if (off + 12 > r->bs) {
        problema(r, &r->c_ent, "carpeta %u: sin sitio para «..» en su primer bloque: se descarta lo que hay detrás", pa->ino);
        po16(e + 4, 12);
        off = 12;
        memset(buf + 12, 0, 12);
        po16(buf + 16, r->bs - 12);
        *mudou = true;
    }
    uint8_t *p = buf + off;
    uint32_t prl = le16(p + 4);
    while (prl < 12 && off + prl < r->bs) {   /* una entrada demasiado pequeña: se junta con la siguiente */
        prl += le16(buf + off + prl + 4);
        po16(p + 4, prl);
        *mudou = true;
    }
    if (!(p[6] == 2 && p[8] == '.' && p[9] == '.')) {
        if (le32(p))
            problema(r, &r->c_ent, "carpeta %u: la segunda entrada no es «..»: se pone (lo que había irá a lost+found si no tiene otra)", pa->ino);
        else problema(r, &r->c_ent, "carpeta %u: falta «..»: se pone", pa->ino);
        po32(p, 0);
        p[6] = 2; p[7] = r->filetype ? 2 : 0; p[8] = '.'; p[9] = '.';
        *mudou = true;
    }
    if (p[7] != (r->filetype ? 2 : 0)) { p[7] = r->filetype ? 2 : 0; *mudou = true; }
    pa->dotdot = le32(p);
    pa->pp_bloco = fis;
    pa->pp_desl = off;
    pa->tem_pp = true;
    return off + le16(p + 4);
}

static void paso2_pasta(reparo_t *r, uint32_t ip, uint8_t *in, uint8_t *buf)
{
    uint32_t n = r->pastas[ip].ino;
    if (ler_inodo(r, n, in) < 0) return;
    uint32_t nblk = I_TAM(in) / r->bs;
    uint32_t *v = mapa_logico(r, in, nblk);
    if (!v) return;
    bool inodo_mudou = false, algum = false;
    for (uint32_t l = 0; l < nblk && !r->fatal; l++) {
        bool mudou = false;
        uint32_t fis = v[l];
        if (!fis) {   /* un hueco dentro de la carpeta: un bloque vacío en su sitio */
            fis = alocar_bloco(r);
            if (!fis || !por_logico(r, in, l, fis)) { falhar(r, "no queda sitio para rellenar un hueco de la carpeta %u", n); break; }
            problema(r, &r->c_ent, "carpeta %u: le falta el bloque %u: se pone uno vacío", n, l);
            uint8_t *x = sujar(r, fis, CL_NOVO);
            memset(x, 0, r->bs);
            po16(x + 4, r->bs);
            if (l == 0) { /* las dos entradas las pone «pontos» */ }
            po32(in + 28, I_SETORES(in) + r->spb);
            inodo_mudou = true;
        }
        if (ler_bloco(r, fis, buf) < 0) break;
        if (salvar_cadeia(r, n, buf)) mudou = true;
        pasta_t *pa = &r->pastas[ip];
        uint32_t off = l == 0 ? pontos(r, pa, fis, buf, &mudou) : 0;
        int32_t ant = l == 0 ? (int32_t)pa->pp_desl : -1;
        while (off < r->bs) {
            uint8_t *e = buf + off;
            uint32_t rl = le16(e + 4), alvo = le32(e), nl = r->filetype ? e[6] : le16(e + 6);
            if (!alvo) { ant = (int32_t)off; off += rl; continue; }
            const char *por_que = NULL;
            uint32_t t = r->est[alvo - 1] & E_TIPO;
            if (alvo <= r->inodes && (r->est[alvo - 1] & F_LIXO) && !r->desconfiar[(alvo - 1) / r->ipg]) {
                /* Una carpeta de verdad apunta a un inodo que se tomó por basura: ese grupo no está vacío. Se repite
                 * el análisis entero sin fiarse de él (reparo_analisar); mientras, la entrada no se toca. */
                r->desconfiar[(alvo - 1) / r->ipg] = 1;
                r->repetir = true;
            }
            if (alvo <= r->inodes && (r->est[alvo - 1] & F_LIXO)) { ant = (int32_t)off; off += rl; continue; }
            if (alvo < r->ino1 && alvo != 2) por_que = "un inodo reservado";
            else if (t == E_LIVRE) por_que = "un inodo que no está en uso";
            else if (nl == 1 && e[8] == '.') por_que = "otro «.»";
            else if (nl == 2 && e[8] == '.' && e[9] == '.') por_que = "otro «..»";
            else if (alvo == 2) por_que = "la raíz";
            else if (alvo == n) por_que = "la propia carpeta";
            else if (t == E_PASTA) {
                pasta_t *f = achar_pasta(r, alvo);
                if (!f) por_que = "una carpeta perdida";
                else if (f->pai) por_que = "una carpeta que ya tiene su sitio en otra";
            }
            if (por_que) {
                char nome[64];
                uint32_t k = nl < sizeof nome - 1 ? nl : sizeof nome - 1;
                memcpy(nome, e + 8, k);
                nome[k] = 0;
                problema(r, &r->c_ent, "carpeta %u: la entrada «%s» apunta a %s (%u): se quita", n, nome, por_que, alvo);
                remover_entrada(buf, ant, off);
                mudou = true;
                if (ant >= 0) off = (uint32_t)ant + le16(buf + ant + 4); else { ant = (int32_t)off; off += rl; }
                continue;
            }
            for (uint32_t k = 0; k < nl; k++)
                if (e[8 + k] == '/' || e[8 + k] == 0) {
                    if (k == 0 || e[8 + k] == '/') problema(r, &r->c_ent, "carpeta %u: un nombre con «/» o un cero: se cambia por «.»", n);
                    e[8 + k] = '.';
                    mudou = true;
                }
            uint8_t esperado = r->ft[alvo - 1];
            if (r->filetype) {
                if (e[7] != esperado) {
                    problema(r, &r->c_ent, "carpeta %u: el tipo de la entrada del inodo %u no es el suyo: se corrige", n, alvo);
                    e[7] = esperado;
                    mudou = true;
                }
            }
            if (t == E_PASTA) {
                pasta_t *f = achar_pasta(r, alvo);
                f->pai = n;
                uint32_t k = nl < sizeof f->nome - 1 ? nl : sizeof f->nome - 1;
                memcpy(f->nome, e + 8, k);
                f->nome[k] = 0;
                if (n == 2 && nl == 10 && memcmp(e + 8, "lost+found", 10) == 0) r->lf = alvo;
            } else if (n == 2 && nl == 10 && memcmp(e + 8, "lost+found", 10) == 0) {
                problema(r, &r->c_ent, "/lost+found no es una carpeta: se quita su entrada (el archivo irá dentro de la nueva)");
                remover_entrada(buf, ant, off);
                mudou = true;
                if (ant >= 0) off = (uint32_t)ant + le16(buf + ant + 4); else { ant = (int32_t)off; off += rl; }
                continue;
            }
            anotar_nome(r, alvo, n, e + 8, nl);
            if (r->refs[alvo - 1] < 0xFFFF) r->refs[alvo - 1]++;
            ant = (int32_t)off;
            off += rl;
        }
        if (mudou) {
            memcpy(sujar(r, fis, CL_DADOS), buf, r->bs);
            algum = true;
        }
    }
    if (algum && (I_FLAGS(in) & FL_INDEX)) {   /* el índice de la carpeta ya no vale: se lee entera */
        po32(in + 32, I_FLAGS(in) & ~FL_INDEX);
        inodo_mudou = true;
    }
    if (inodo_mudou) gravar_inodo(r, n, in);
    libera(v);
}

static void paso2(reparo_t *r)
{
    if (r->fatal) return;
    comecar_paso(r, 3, "Paso 2: carpetas");
    uint8_t *in = aloca(r->isz), *buf = aloca(r->bs);
    if (!in || !buf) falhar(r, "sin memoria");
    pasta_t *raiz = achar_pasta(r, 2);
    if (raiz) raiz->pai = 2;
    for (uint32_t i = 0; i < r->np && !r->fatal; i++) paso2_pasta(r, i, in, buf);
    if (raiz && (raiz = achar_pasta(r, 2))) raiz->pai = 2;
    if (in) libera(in);
    if (buf) libera(buf);
}

/* ------------------------------------------------------------ paso 3: carpetas conectadas a la raíz */

static uint32_t criar_lf(reparo_t *r)
{
    uint32_t n = alocar_inodo(r), b = alocar_bloco(r);
    if (!n || !b) return 0;
    uint8_t *in = aloca_zerado(r->isz);
    if (!in) { falhar(r, "sin memoria"); return 0; }
    po16(in + 0, 0x41C0);   /* carpeta, 0700 */
    po32(in + 4, r->bs);
    po32(in + 8, r->agora); po32(in + 12, r->agora); po32(in + 16, r->agora);
    po16(in + 26, 2);
    po32(in + 28, r->spb);
    po32(in + 40, b);
    if (r->isz >= 160) po16(in + 128, 32);
    uint8_t *x = sujar(r, b, CL_NOVO);
    memset(x, 0, r->bs);
    po32(x, n); po16(x + 4, 12); x[6] = 1; x[7] = r->filetype ? 2 : 0; x[8] = '.';
    po32(x + 12, 2); po16(x + 16, r->bs - 12); x[18] = 2; x[19] = r->filetype ? 2 : 0; x[20] = '.'; x[21] = '.';
    gravar_inodo(r, n, in);
    libera(in);
    r->est[n - 1] = E_PASTA;
    r->ft[n - 1] = 2;
    if (!pasta_nova(r, n)) return 0;
    pasta_t *p = achar_pasta(r, n);
    p->pai = 2; p->dotdot = 2; p->pp_bloco = b; p->pp_desl = 12; p->tem_pp = true; p->conectada = true;
    copia_texto(p->nome, "lost+found", sizeof p->nome);
    if (!adicionar_entrada(r, 2, "lost+found", n, 2)) { falhar(r, "no se puede añadir lost+found a la raíz"); return 0; }
    r->refs[n - 1]++;
    return n;
}

static bool garantir_lf(reparo_t *r)
{
    if (r->lf) return true;
    problema(r, &r->c_lf, "no hay /lost+found: se crea");
    r->lf = criar_lf(r);
    if (!r->lf) falhar(r, "no se puede crear /lost+found (sin inodos o bloques libres)");
    return r->lf != 0;
}

/* Una carpeta (o un archivo) a /lost+found con el nombre «#<inodo>». */
static bool para_lf(reparo_t *r, uint32_t n, int tipo)
{
    if (!garantir_lf(r)) return false;
    char nome[16];
    formatar_texto(nome, sizeof nome, "#%u", n);
    if (!adicionar_entrada(r, r->lf, nome, n, tipo)) { falhar(r, "no queda sitio en /lost+found"); return false; }
    r->est[n - 1] |= F_AFETADO;
    if (r->refs[n - 1] < 0xFFFF) r->refs[n - 1]++;
    return true;
}

static void paso3(reparo_t *r)
{
    if (r->fatal) return;
    comecar_paso(r, 4, "Paso 3: carpetas sin camino hasta la raíz");
    if (!garantir_lf(r)) return;   /* antes: crearla añade una carpeta a la lista */
    uint32_t marca = 0;
    for (uint32_t i = 0; i < r->np && !r->fatal; i++) {
        pasta_t *p = &r->pastas[i];
        if (p->ino == 2) { p->conectada = true; continue; }
        if (p->conectada) continue;
        marca++;
        uint32_t ino = p->ino;
        for (;;) {
            pasta_t *a = achar_pasta(r, ino);
            if (a->conectada) break;
            a->marca = marca;
            pasta_t *q = a->pai ? achar_pasta(r, a->pai) : NULL;
            if (q && (q->conectada || q->ino == 2)) break;
            if (!q || q->marca == marca) {   /* sin padre, o un ciclo: esta va a lost+found */
                problema(r, &r->c_lf, q ? "carpeta %u: forma un ciclo con otras: va a /lost+found" : "carpeta %u: no está en ninguna carpeta: va a /lost+found",
                         a->ino);
                uint32_t ai = a->ino;
                if (!para_lf(r, ai, 2)) return;
                a = achar_pasta(r, ai);   /* pastas puede haber crecido (lost+found nueva) */
                a->pai = r->lf;
                break;
            }
            ino = q->ino;
        }
        /* todo el camino recorrido queda conectado */
        for (uint32_t k = 0; k < r->np; k++) if (r->pastas[k].marca == marca) r->pastas[k].conectada = true;
    }
    /* «..» de cada carpeta = su padre de verdad. */
    for (uint32_t i = 0; i < r->np && !r->fatal; i++) {
        pasta_t *p = &r->pastas[i];
        if (!p->tem_pp || p->dotdot == p->pai) continue;
        problema(r, &r->c_ent, "carpeta %u: «..» apunta a %u y su carpeta es la %u: se corrige", p->ino, p->dotdot, p->pai);
        po32(sujar(r, p->pp_bloco, CL_DADOS) + p->pp_desl, p->pai);
        p->dotdot = p->pai;
    }
    for (uint32_t i = 0; i < r->np; i++) {
        pasta_t *p = &r->pastas[i];
        if (p->ino != 2 && p->pai) { pasta_t *q = achar_pasta(r, p->pai); if (q) q->nsub++; }
    }
}

/* ------------------------------------------------------------ paso 4: enlaces */

static void paso4(reparo_t *r)
{
    if (r->fatal) return;
    comecar_paso(r, 5, "Paso 4: archivos sin carpeta y número de enlaces");
    uint8_t *in = aloca(r->isz);
    if (!in) { falhar(r, "sin memoria"); return; }
    for (uint32_t n = 2; n <= r->inodes && !r->fatal; n++) {
        if (n < r->ino1 && n != 2) continue;
        uint32_t t = r->est[n - 1] & E_TIPO;
        if (t == E_LIVRE || t == E_RESERV) continue;
        if (ler_inodo(r, n, in) < 0) break;
        if (t != E_PASTA && !r->refs[n - 1]) {
            if (!I_TAM(in) && !I_SETORES(in)) {
                apagar_inodo(r, n, in, "vacío y sin ninguna carpeta");
                continue;
            }
            problema(r, &r->c_lf, "inodo %u (%u bytes): no está en ninguna carpeta: va a /lost+found/#%u", n, I_TAM(in), n);
            if (!para_lf(r, n, tipo_entrada(I_MODO(in)))) break;
            if (ler_inodo(r, n, in) < 0) break;
        }
        uint32_t esperado = r->refs[n - 1];
        if (t == E_PASTA) {
            pasta_t *p = achar_pasta(r, n);
            esperado += 1 + (p ? p->nsub : 0) + (n == 2 ? 1 : 0);
        }
        if (I_LIG(in) != esperado) {
            problema(r, &r->c_enl, "inodo %u: dice %u enlaces y tiene %u: se corrige", n, I_LIG(in), esperado);
            po16(in + 26, esperado);
            gravar_inodo(r, n, in);
        }
    }
    libera(in);
}

/* ------------------------------------------------------------ paso 5: mapas y contadores */

static void paso5(reparo_t *r)
{
    if (r->fatal) return;
    comecar_paso(r, 6, "Paso 5: mapas de bits y contadores");
    uint8_t *m = aloca(r->bs), *disco = aloca(r->bs);
    if (!m || !disco) { falhar(r, "sin memoria"); goto fim; }
    uint32_t livres_b = 0, livres_i = 0;
    for (uint32_t g = 0; g < r->grupos && !r->fatal; g++) {
        uint8_t *d = gd(r, g);
        uint32_t ini = ini_grupo(r, g), tam = tam_grupo(r, g), lb = 0, li = 0, pastas = 0;
        memset(m, 0, r->bs);
        for (uint32_t i = 0; i < tam; i++) if (bit_de(r->usado, ini + i)) bit_pon(m, i); else lb++;
        for (uint32_t i = tam; i < r->bs * 8; i++) bit_pon(m, i);   /* el relleno, a 1 */
        if (ler_bloco(r, le32(d), disco) < 0) break;
        if (memcmp(m, disco, r->bs)) {
            uint32_t dif = 0;
            for (uint32_t i = 0; i < r->bs * 8; i++) if (bit_de(m, i) != bit_de(disco, i)) dif++;
            problema(r, &r->c_mapa, "grupo %u: el mapa de bloques no cuadra (%u bloques): se rehace", g, dif);
            memcpy(sujar(r, le32(d), CL_MAPA), m, r->bs);
        }
        memset(m, 0, r->bs);
        for (uint32_t i = 0; i < r->ipg; i++) {
            uint32_t n = g * r->ipg + i + 1;
            uint32_t t = r->est[n - 1] & E_TIPO;
            if (n < r->ino1 || t != E_LIVRE) { bit_pon(m, i); if (t == E_PASTA) pastas++; } else li++;
        }
        for (uint32_t i = r->ipg; i < r->bs * 8; i++) bit_pon(m, i);
        if (ler_bloco(r, le32(d + 4), disco) < 0) break;
        if (memcmp(m, disco, r->bs)) {
            uint32_t dif = 0;
            for (uint32_t i = 0; i < r->bs * 8; i++) if (bit_de(m, i) != bit_de(disco, i)) dif++;
            problema(r, &r->c_mapa, "grupo %u: el mapa de inodos no cuadra (%u inodos): se rehace", g, dif);
            memcpy(sujar(r, le32(d + 4), CL_MAPA), m, r->bs);
        }
        if (le16(d + 12) != lb || le16(d + 14) != li || le16(d + 16) != pastas) {
            problema(r, &r->c_cont, "grupo %u: libres %u bloques/%u inodos y %u carpetas; de verdad %u/%u y %u: se corrige",
                     g, le16(d + 12), le16(d + 14), le16(d + 16), lb, li, pastas);
            po16(d + 12, lb); po16(d + 14, li); po16(d + 16, pastas);
        }
        livres_b += lb;
        livres_i += li;
    }
    if (r->fatal) goto fim;
    if (memcmp(r->gd, r->gd_orig, r->gdt * r->bs)) {
        for (uint32_t k = 0; k < r->gdt; k++) memcpy(sujar(r, r->fdb + 1 + k, CL_GD), r->gd + k * r->bs, r->bs);
    }
    uint8_t *sb = r->sb;
    if (le32(sb + 12) != livres_b || le32(sb + 16) != livres_i) {
        problema(r, &r->c_cont, "superbloque: %u bloques y %u inodos libres; de verdad %u y %u: se corrige", le32(sb + 12), le32(sb + 16), livres_b, livres_i);
        po32(sb + 12, livres_b);
        po32(sb + 16, livres_i);
    }
    if (r->sb_copia) problema(r, &r->c_outros, "el superbloque principal estaba dañado: se rehace con la copia del grupo %u", r->sb_grupo);
    if (le32(sb + 0xE8)) { problema(r, &r->c_outros, "superbloque: lista de huérfanos pendiente: se vacía"); po32(sb + 0xE8, 0); }
    uint32_t estado = le16(sb + 58);
    if (!(estado & 1)) problema(r, &r->c_outros, "no se desmontó bien la última vez: se marca como desmontado bien");
    if (estado & 2) problema(r, &r->c_outros, "tenía marcados errores: se quitan");
    po16(sb + 58, 1);
    if (r->problemas) {   /* comprobado ahora: el contador de montajes y la fecha de la comprobación */
        po16(sb + 52, 0);
        po32(sb + 64, r->agora);
    }
fim:
    if (m) libera(m);
    if (disco) libera(disco);
}

/* ------------------------------------------------------------ la interfaz */

void reparo_fechar(reparo_t *r)
{
    if (!r) return;
    for (uint32_t i = 0; i < r->ns; i++) libera(r->suj[i].d);
    void *v[] = { r->gd, r->gd_orig, r->usado, r->dup, r->fixo, r->est, r->refs, r->pastas, r->ea, r->dupl, r->nomes, r->suj,
                  r->hash, r->tmp, r->lixo, r->ft, r->vazio, r->desconfiar, r->zerar };
    for (unsigned i = 0; i < sizeof v / sizeof v[0]; i++) if (v[i]) libera(v[i]);
    libera(r);
}

#define REP_TENTATIVAS 4   /* análisis completos como mucho (D397: grupos vacíos de los que se deja de fiar) */

/* Un análisis completo. "desconfiar" (por grupo, o NULL) pasa a ser de r. */
static int analisar_uma(const char *vol, reparo_aviso_t aviso, void *ctx, uint8_t *desconfiar, reparo_t **saida, char *msg, int max)
{
    *saida = NULL;
    reparo_t *r = aloca_zerado(sizeof *r);
    if (!r) { if (desconfiar) libera(desconfiar); txt(msg, max, "sin memoria"); return ERRO_SEM_MEMORIA; }
    r->desconfiar = desconfiar;
    copia_texto(r->vol, vol, sizeof r->vol);
    r->aviso = aviso;
    r->ctx = ctx;
    r->agora = hora_utc(NULL);
    int x = abrir_vol(r, msg, max);
    if (x < 0) { reparo_fechar(r); return x; }
    if (r->agora <= r->inodes) r->agora = r->inodes + 1;   /* una fecha de borrado «baja» parece un huérfano */
    r->tmp = aloca(r->bs > 1024 ? r->bs : 1024);
    r->lixo = aloca(r->bs);
    r->usado = aloca_zerado(r->blocos / 8 + 1);
    r->dup = aloca_zerado(r->blocos / 8 + 1);
    r->fixo = aloca_zerado(r->blocos / 8 + 1);
    r->est = aloca_zerado(r->inodes);
    r->refs = aloca_zerado(r->inodes * sizeof(uint16_t));
    r->ft = aloca_zerado(r->inodes);
    r->vazio = aloca_zerado(r->grupos);
    if (!r->desconfiar) r->desconfiar = aloca_zerado(r->grupos);
    r->zerar = aloca_zerado(((uint64_t)r->grupos * r->itb + 7) / 8);
    if (!r->tmp || !r->lixo || !r->usado || !r->dup || !r->fixo || !r->est || !r->refs || !r->ft || !r->vazio || !r->desconfiar || !r->zerar) {
        txt(msg, max, "sin memoria para comprobar %s (%u bloques, %u inodos)", vol, r->blocos, r->inodes);
        reparo_fechar(r);
        return ERRO_SEM_MEMORIA;
    }
    {
        char s[200];
        formatar_texto(s, sizeof s, "%s: ext2 de %u MB, bloques de %u bytes, %u grupos, %u inodos%s", vol,
                       (uint32_t)((uint64_t)r->blocos * r->bs / 1048576), r->bs, r->grupos, r->inodes,
                       (le16(r->sb + 58) & 1) ? "" : " (no se desmontó bien)");
        dizer(r, s);
    }
    paso1(r);
    paso1b(r);
    paso2(r);
    if (!r->repetir) {
        paso3(r);   /* como e2fsck: siempre tiene que haber /lost+found (la crea si falta) */
        paso4(r);
        paso5(r);
    }
    if (r->fatal) {
        txt(msg, max, "%s: %s. No se ha cambiado nada", vol, r->erro);
        reparo_fechar(r);
        return ERRO_FORMATO;
    }
    *saida = r;
    return (int)r->problemas;
}

int reparo_analisar(const char *vol, reparo_aviso_t aviso, void *ctx, reparo_t **saida, char *msg, int max)
{
    *saida = NULL;
    if (msg && max) msg[0] = 0;
    uint8_t *desconfiar = NULL;
    for (int t = 1; ; t++) {
        reparo_t *r;
        int x = analisar_uma(vol, aviso, ctx, desconfiar, &r, msg, max);
        if (x < 0 || !r->repetir) { *saida = r; return x; }
        /* D397: una carpeta apunta a un inodo de un grupo que se tomó por vacío: otra vez, sin fiarse de él (y, en el
         * último intento, de ninguno: todo inodo a inodo, como antes). */
        uint32_t n = 0;
        for (uint32_t g = 0; g < r->grupos; g++) n += r->desconfiar[g];
        desconfiar = r->desconfiar;
        r->desconfiar = NULL;
        if (t + 1 >= REP_TENTATIVAS) memset(desconfiar, 1, r->grupos);
        char s[200];
        formatar_texto(s, sizeof s, "Una carpeta apunta a inodos de %u grupos que parecían vacíos: se repite el análisis sin fiarse de %s",
                       n, t + 1 >= REP_TENTATIVAS ? "ningún grupo" : "ellos");
        if (aviso) aviso(s, ctx);
        registrar("reparar: %s: %s", vol, s);
        reparo_fechar(r);
    }
}

uint32_t reparo_problemas(const reparo_t *r) { return r->problemas; }

/* D397: la basura de las tablas de inodos de los grupos vacíos va a la copia de seguridad (para que «deshacer»
 * deje el volumen exacto) mientras no pase de esto; si es más, no se guarda (es basura: nada la usa). */
#define COPIA_LIXO_MAX (64u * 1024 * 1024)

static uint32_t lixo_na_copia(const reparo_t *r)
{
    return (uint64_t)r->nzerar * r->bs <= COPIA_LIXO_MAX ? r->nzerar : 0;
}

uint32_t reparo_blocos(const reparo_t *r, uint32_t *bytes)
{
    if (bytes) *bytes = (r->ns + lixo_na_copia(r) + 1) * r->bs;
    return r->ns + r->nzerar;
}

/* D397 (modo «preen»): lo que NO es seguro hacer sin que alguien lo mire: inodos borrados, bloques soltados
 * del archivo, entradas de carpeta descartadas (nombres que se pierden) o un superbloque rehecho con su
 * copia. Mapas, contadores, enlaces, la marca de «no se desmontó bien», punteros a bloques que no existen
 * y bloques compartidos (cada archivo se queda una copia) no pierden nada que se pudiera leer. */
uint32_t reparo_arriscados(const reparo_t *r)
{
    /* Al fusionar D397 (basura) con el modo -p: poner a cero las tablas de inodos con basura de los grupos vacíos
     * no pierde nada si la heurística acierta, pero escribe metadatos por una suposición; sin nadie delante (el
     * servicio del arranque) se deja para «reparar -y» a mano. */
    return r->c_borr + r->c_solto + r->c_ent + (r->sb_copia ? 1u : 0u) + r->g_lixo;
}

/* Los nombres de las carpetas, para el informe: /a/b/c (hasta la raíz o hasta que no se sepa). */
static void caminho(reparo_t *r, uint32_t pasta, char *s, int max)
{
    char tmp[256];
    s[0] = 0;
    for (int prof = 0; pasta && pasta != 2 && prof < 32; prof++) {
        pasta_t *p = achar_pasta(r, pasta);
        if (!p) break;
        formatar_texto(tmp, sizeof tmp, "/%s%s", p->nome[0] ? p->nome : "?", s);
        copia_texto(s, tmp, (size_t)max);
        pasta = p->pai;
    }
}

void reparo_resumo(reparo_t *r)
{
    char s[400];
    if (!r->problemas) { dizer(r, "Todo cuadra: no hay nada que reparar."); return; }
    formatar_texto(s, sizeof s, "Resumen: %u problemas. Punteros fuera del volumen quitados: %u; bloques compartidos copiados: %u "
                   "(soltados sin sitio: %u); inodos borrados: %u; entradas de carpeta arregladas: %u; a lost+found: %u; "
                   "enlaces corregidos: %u; mapas rehechos: %u; contadores: %u; otros: %u.",
                   r->problemas, r->c_fora, r->c_clon, r->c_solto, r->c_borr, r->c_ent, r->c_lf, r->c_enl, r->c_mapa, r->c_cont, r->c_outros);
    dizer(r, s);
    if (r->g_lixo) {
        formatar_texto(s, sizeof s, "Tablas de inodos con basura en grupos vacíos: %u grupos, %u inodos, %u KB que se ponen a cero "
                       "(no son archivos: no se tocan los archivos de verdad ni sus bloques).", r->g_lixo, r->c_lixo,
                       (uint32_t)((uint64_t)r->nzerar * r->bs / 1024));
        dizer(r, s);
    }
    uint32_t k = 0;
    for (uint32_t n = 1; n <= r->inodes; n++) {
        if (!(r->est[n - 1] & F_AFETADO)) continue;
        if (!k++) dizer(r, "Archivos tocados (revísalos):");
        if (k > 30) { if (k == 31) dizer(r, "  (y más)"); continue; }
        char ruta[200] = "";
        for (uint32_t i = 0; i < r->nn; i++)
            if (r->nomes[i].ino == n) {
                caminho(r, r->nomes[i].pasta, ruta, sizeof ruta);
                junta_texto(ruta, "/", sizeof ruta);
                junta_texto(ruta, r->nomes[i].nome, sizeof ruta);
                break;
            }
        if (!ruta[0] && (r->est[n - 1] & E_TIPO) == E_PASTA) caminho(r, n, ruta, sizeof ruta);
        if (!ruta[0] && (r->est[n - 1] & E_TIPO) != E_LIVRE && r->lf) formatar_texto(ruta, sizeof ruta, "/lost+found/#%u", n);
        formatar_texto(s, sizeof s, "  inodo %u: %s%s", n, ruta[0] ? ruta : "(sin nombre)",
                       (r->est[n - 1] & E_TIPO) == E_LIVRE ? " (borrado)" : "");
        dizer(r, s);
    }
}

/* La copia de seguridad de lo que se va a cambiar (cómo está ahora en el disco):
 *   «SAGUIREP» (8) | versión 1 (4) | volumen (16) | tamaño de bloque (4) | bloques (4) | superbloque (1024)
 *   y, por cada bloque, su número (4) y su contenido. */
#define COPIA_MAGIA "SAGUIREP"

int reparo_guardar_copia(reparo_t *r, const char *arquivo, char *msg, int max)
{
    int fd = abrir(arquivo, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { txt(msg, max, "no se puede crear %s: %s", arquivo, nome_erro(fd)); return fd; }
    uint8_t cab[36];
    memset(cab, 0, sizeof cab);
    memcpy(cab, COPIA_MAGIA, 8);
    po32(cab + 8, 1);
    copia_texto((char *)cab + 12, r->vol, 16);
    po32(cab + 28, r->bs);
    uint32_t extra = 0;   /* D397: los bloques con basura que se ponen a cero (salvo los que ya van entre los sucios) */
    if (lixo_na_copia(r))
        for (uint32_t g = 0; g < r->grupos; g++) {
            if (!r->vazio[g]) continue;
            uint32_t it = le32(gd(r, g) + 8);
            for (uint32_t k = 0; k < r->itb; k++)
                if (bit_de(r->zerar, g * r->itb + k) && !achar_sujo(r, it + k)) extra++;
        }
    po32(cab + 32, r->ns + extra);
    uint8_t *buf = aloca(r->bs + 4);
    int x = buf ? 0 : ERRO_SEM_MEMORIA;
    if (!x && (escrever_tudo(fd, cab, sizeof cab) < 0 || escrever_tudo(fd, r->sb_orig, 1024) < 0)) x = ERRO_ACESSO;
    for (uint32_t i = 0; i < r->ns && !x; i++) {
        po32(buf, r->suj[i].b);
        if ((x = ler_disco(r, r->suj[i].b, 1, buf + 4)) < 0) break;
        if (escrever_tudo(fd, buf, r->bs + 4) < 0) x = ERRO_ACESSO;
    }
    for (uint32_t g = 0; g < r->grupos && extra && !x; g++) {
        if (!r->vazio[g]) continue;
        uint32_t it = le32(gd(r, g) + 8);
        for (uint32_t k = 0; k < r->itb && !x; k++) {
            if (!bit_de(r->zerar, g * r->itb + k) || achar_sujo(r, it + k)) continue;
            po32(buf, it + k);
            if ((x = ler_disco(r, it + k, 1, buf + 4)) < 0) break;
            if (escrever_tudo(fd, buf, r->bs + 4) < 0) x = ERRO_ACESSO;
        }
    }
    if (fechar(fd) < 0 && !x) x = ERRO_ACESSO;
    if (buf) libera(buf);
    uint32_t total = r->ns + extra;
    if (x < 0) txt(msg, max, "no se pudo guardar la copia en %s: %s", arquivo, nome_erro(x));
    else if (r->nzerar && !extra)
        txt(msg, max, "copia de los %u bloques que cambian en %s (%u KB; sin los %u MB de basura de las tablas de inodos vacías, "
            "que nada usa)", total, arquivo, (uint32_t)(((uint64_t)total * (r->bs + 4) + 1060) / 1024),
            (uint32_t)((uint64_t)r->nzerar * r->bs / 1048576));
    else txt(msg, max, "copia de los %u bloques que cambian en %s (%u KB)", total, arquivo, (uint32_t)(((uint64_t)total * (r->bs + 4) + 1060) / 1024));
    return x;
}

static void ordenar(uint32_t *v, uint32_t n, const sujo_t *s)
{
    for (uint32_t gap = n / 2; gap > 0; gap /= 2)
        for (uint32_t i = gap; i < n; i++) {
            uint32_t t = v[i], j = i;
            while (j >= gap && s[v[j - gap]].b > s[t].b) { v[j] = v[j - gap]; j -= gap; }
            v[j] = t;
        }
}

/* Escribe lo cambiado (el disco tiene que estar reservado). */
int reparo_gravar(reparo_t *r, char *msg, int max)
{
    uint32_t *ord = aloca((r->ns + 1) * sizeof(uint32_t));
    if (!ord) { txt(msg, max, "sin memoria"); return ERRO_SEM_MEMORIA; }
    int x = 0;
    uint32_t escritos = 0, zerados = 0;
    /* D397: primero la basura de las tablas de inodos de los grupos vacíos, a cero (nada la usa: si se corta aquí,
     * el volumen sigue como estaba, con menos basura). Por tramos seguidos de hasta 64 KB; los bloques que también
     * están entre los sucios se escriben después con su contenido. */
    if (r->nzerar) {
        uint32_t trozo = 65536 / r->bs;
        uint8_t *zeros = aloca_zerado(trozo * r->bs);
        if (!zeros) { libera(ord); txt(msg, max, "sin memoria"); return ERRO_SEM_MEMORIA; }
        for (uint32_t g = 0; g < r->grupos && !x; g++) {
            if (!r->vazio[g]) continue;
            uint32_t it = le32(gd(r, g) + 8);
            for (uint32_t k = 0; k < r->itb && !x; ) {
                uint32_t n = 0;
                while (k + n < r->itb && n < trozo && bit_de(r->zerar, g * r->itb + k + n) && !achar_sujo(r, it + k + n)) n++;
                if (!n) { k++; continue; }
                if ((x = blocos_escrever(r->vol, (it + k) * r->spb, n * r->spb, zeros)) < 0)
                    txt(msg, max, "error al poner a cero la tabla de inodos del grupo %u de %s (%s): vuelve a pasar reparar", g, r->vol, nome_erro(x));
                else zerados += n;
                k += n;
            }
        }
        libera(zeros);
        if (x < 0) { libera(ord); return x; }
    }
    for (int c = 0; c < CL_N && !x; c++) {
        uint32_t k = 0;
        for (uint32_t i = 0; i < r->ns; i++) if (r->suj[i].classe == c) ord[k++] = i;
        ordenar(ord, k, r->suj);
        for (uint32_t i = 0; i < k && !x; i++) {
            const sujo_t *s = &r->suj[ord[i]];
            if ((x = blocos_escrever(r->vol, s->b * r->spb, r->spb, s->d)) < 0)
                txt(msg, max, "error al escribir el bloque %u de %s (%s): el volumen ha quedado a medias, sigue marcado como "
                    "«no se desmontó bien»; vuelve a pasar reparar", s->b, r->vol, nome_erro(x));
            else escritos++;
        }
    }
    libera(ord);
    if (x < 0) return x;
    if ((x = blocos_escrever(r->vol, 2, 2, r->sb)) < 0) {
        txt(msg, max, "error al escribir el superbloque de %s (%s): vuelve a pasar reparar", r->vol, nome_erro(x));
        return x;
    }
    registrar("reparar: %s: %u problemas arreglados, %u bloques escritos y el superbloque; %u bloques de basura de tablas de inodos "
              "a cero", r->vol, r->problemas, escritos, zerados);
    txt(msg, max, "%s reparado: %u problemas arreglados (%u bloques escritos%s)", r->vol, r->problemas, escritos + zerados,
        zerados ? ", con la basura de las tablas de inodos vacías" : "");
    return 0;
}

/* Vuelve a poner una copia de reparo_guardar_copia (el disco tiene que estar reservado). */
int reparo_desfazer(const char *arquivo, const char *vol, char *msg, int max)
{
    int fd = abrir(arquivo, ABRIR_LER);
    if (fd < 0) { txt(msg, max, "no se puede abrir %s: %s", arquivo, nome_erro(fd)); return fd; }
    uint8_t cab[36], sb[1024];
    int x = 0;
    if (ler(fd, cab, sizeof cab) != (int)sizeof cab || memcmp(cab, COPIA_MAGIA, 8) || le32(cab + 8) != 1 ||
        ler(fd, sb, sizeof sb) != (int)sizeof sb) {
        fechar(fd);
        txt(msg, max, "%s no es una copia de «reparar»", arquivo);
        return ERRO_FORMATO;
    }
    char de[17];
    memcpy(de, cab + 12, 16);
    de[16] = 0;
    if (compara_texto(de, vol) != 0) {
        fechar(fd);
        txt(msg, max, "la copia %s es de %s, no de %s", arquivo, de, vol);
        return ERRO_INVALIDO;
    }
    uint32_t bs = le32(cab + 28), n = le32(cab + 32), spb = bs / 512;
    uint8_t *buf = (bs == 1024 || bs == 2048 || bs == 4096) ? aloca(bs + 4) : NULL;
    if (!buf) { fechar(fd); txt(msg, max, "copia dañada o sin memoria"); return ERRO_FORMATO; }
    uint32_t feitos = 0;
    for (uint32_t i = 0; i < n && !x; i++) {
        if (ler(fd, buf, bs + 4) != (int)(bs + 4)) { x = ERRO_FORMATO; txt(msg, max, "la copia %s está cortada", arquivo); break; }
        if ((x = blocos_escrever(vol, le32(buf) * spb, spb, buf + 4)) < 0) txt(msg, max, "error al escribir en %s: %s", vol, nome_erro(x));
        else feitos++;
    }
    fechar(fd);
    libera(buf);
    if (!x && (x = blocos_escrever(vol, 2, 2, sb)) < 0) txt(msg, max, "error al escribir el superbloque de %s", vol);
    if (!x) {
        registrar("reparar: %s: deshecho con %s (%u bloques)", vol, arquivo, feitos);
        txt(msg, max, "%s vuelve a estar como antes de reparar (%u bloques)", vol, feitos);
    }
    return x;
}
