// SPDX-License-Identifier: GPL-2.0-or-later
/* redimensao.c - comprobar (sin escribir) un FAT32 o un ext2 y cambiar el tamaño de un
 * ext2 (D213, v7.3). Lo usa "particiones redimensionar" y el botón Tamaño de Discos >
 * Particiones a través de pt_executar.
 *
 * Comprobar (fs_verificar) es un fsck de solo lectura: no arregla nada, solo dice si el
 * volumen está como para tocarlo. Antes de redimensionar se comprueba, y después también.
 *   - ext2: el superbloque (firma, estado "desmontado bien" y sin errores, características
 *     conocidas, tamaño dentro de la partición), cada descriptor de grupo (sus mapas y su
 *     tabla de inodos dentro del grupo) y que los libres de cada grupo coinciden con sus
 *     mapas de bits;
 *   - FAT32: el sector de arranque, que las dos FAT son iguales y que cada entrada es libre,
 *     fin de cadena, defectuosa o un clúster que existe.
 *
 * Redimensionar un ext2 (fs_redimensionar_ext2), sin mover datos nunca:
 *   - crecer: el último grupo se completa y se añaden grupos nuevos al final (superbloque y
 *     tabla de grupos de copia en los que tocan con sparse_super, mapas de bits y tabla de
 *     inodos a cero). Si la tabla de descriptores necesita más bloques, salen de los
 *     reservados para crecer (resize_inode, los que deja mke2fs: el inodo 7 se rehace con la
 *     geometría nueva, como ext2fs_create_resize_inode); sin ellos, el ext2 solo crece hasta
 *     donde da su tabla (con bloques de 1 KB, 256 MB por bloque de la tabla). Si se gastan
 *     todos los reservados, resize_inode se quita (el inodo 7 a cero);
 *   - encoger: solo si los grupos que desaparecen no tienen ningún inodo en uso ni ningún
 *     bloque que no sea suyo (sus metadatos), y el trozo que se corta del último grupo que
 *     queda está libre. Si no, se niega y dice hasta dónde se podría encoger.
 * Orden de escritura: primero lo que nadie mira todavía (grupos nuevos), luego los mapas de
 * los grupos que cambian, las copias de la tabla y del superbloque, y el superbloque
 * principal el último. La partición se agranda antes de crecer y se encoge después de
 * encoger (particoes_acoes.c): si algo se corta a medias, el sistema de archivos cabe
 * siempre en su partición. */
#include "particoes.h"

static uint32_t le16(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8; }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void po16(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void po32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static void msg_txt(char *m, int max, const char *fmt, ...)
{
    if (!m || max <= 0) return;
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(m, (size_t)max, fmt, ap);
    va_end(ap);
}

static bool bit_de(const uint8_t *m, uint32_t i) { return (m[i / 8] >> (i % 8)) & 1; }
static void bit_pon(uint8_t *m, uint32_t i) { m[i / 8] |= (uint8_t)(1u << (i % 8)); }
static void bit_quita(uint8_t *m, uint32_t i) { m[i / 8] &= (uint8_t)~(1u << (i % 8)); }
static uint32_t ceros(const uint8_t *m, uint32_t n)
{
    uint32_t z = 0;
    for (uint32_t i = 0; i < n; i++) if (!bit_de(m, i)) z++;
    return z;
}

/* ------------------------------------------------------------ ext2: geometría */

#define EXT_COMPAT_OK     (0x0001u | 0x0002u | 0x0008u | 0x0010u | 0x0020u)   /* prealloc, imagic, xattr, resize_inode, dir_index */
#define EXT_COMPAT_RESIZE 0x0010u
#define EXT_INCOMPAT_OK   0x0002u                                           /* filetype */
#define EXT_RO_OK         (0x0001u | 0x0002u | 0x0004u)                     /* sparse_super, large_file, btree_dir */

typedef struct {
    const char *vol;
    uint8_t  sb[1024];
    uint32_t setores_part;       /* de la partición */
    uint32_t bs, spb, fdb, bpg, ipg, isz, itb;
    uint32_t blocos, grupos, gdt, rgdt;
    bool     sparse, resize;
    uint8_t *gd;                 /* los descriptores (32 bytes cada uno) */
    uint32_t gd_cap;             /* grupos que caben en gd */
    uint32_t gd_bytes;           /* tamaño de gd (bloques enteros) */
} ext_t;

static bool pot(uint32_t g, uint32_t b) { while (g > 1 && g % b == 0) g /= b; return g == 1; }
static bool ext_tem_sb(const ext_t *e, uint32_t g) { return !e->sparse || g == 0 || g == 1 || pot(g, 3) || pot(g, 5) || pot(g, 7); }
static uint32_t ext_ini(const ext_t *e, uint32_t g) { return e->fdb + g * e->bpg; }
static uint32_t ext_tam(const ext_t *e, uint32_t g, uint32_t blocos)
{
    uint32_t i = ext_ini(e, g);
    return blocos - i < e->bpg ? blocos - i : e->bpg;
}
static uint8_t *ext_gd(ext_t *e, uint32_t g) { return e->gd + g * 32; }

static int ext_ler(const ext_t *e, uint32_t b, uint32_t n, void *buf) { return blocos_ler(e->vol, b * e->spb, n * e->spb, buf); }
static int ext_escrever(const ext_t *e, uint32_t b, uint32_t n, const void *buf) { return blocos_escrever(e->vol, b * e->spb, n * e->spb, buf); }

static void ext_fechar(ext_t *e) { if (e->gd) libera(e->gd); e->gd = NULL; }

/* Lee el superbloque y los descriptores; comprueba que es un ext2 que se sabe tocar. */
static int ext_abrir(ext_t *e, const char *vol, uint32_t grupos_extra, char *m, int max)
{
    memset(e, 0, sizeof *e);
    e->vol = vol;
    info_bloco_t inf;
    int r = blocos_info(vol, &inf);
    if (r < 0) { msg_txt(m, max, "%s: %s", vol, nome_erro(r)); return r; }
    e->setores_part = inf.setores;
    if ((r = blocos_ler(vol, 2, 2, e->sb)) < 0) { msg_txt(m, max, "no se puede leer el superbloque de %s: %s", vol, nome_erro(r)); return r; }
    const uint8_t *sb = e->sb;
    if (le16(sb + 56) != 0xEF53) { msg_txt(m, max, "%s no es un ext2", vol); return ERRO_FORMATO; }
    uint32_t log = le32(sb + 24), rev = le32(sb + 76);
    uint32_t compat = rev ? le32(sb + 92) : 0, incompat = rev ? le32(sb + 96) : 0, ro = rev ? le32(sb + 100) : 0;
    if (compat & 0x0004u) { msg_txt(m, max, "%s es un ext3 (con diario): aquí solo se redimensiona ext2", vol); return ERRO_NAO_SUPORTADO; }
    if ((compat & ~EXT_COMPAT_OK) || (incompat & ~EXT_INCOMPAT_OK) || (ro & ~EXT_RO_OK)) {
        msg_txt(m, max, "%s tiene características que esta versión no sabe tocar (ext4 u otras: %x/%x/%x)", vol, compat, incompat, ro);
        return ERRO_NAO_SUPORTADO;
    }
    if (log > 2) { msg_txt(m, max, "%s: bloques de más de 4 KB", vol); return ERRO_NAO_SUPORTADO; }
    e->bs = 1024u << log;
    e->spb = e->bs / 512;
    e->fdb = le32(sb + 20);
    e->bpg = le32(sb + 32);
    e->ipg = le32(sb + 40);
    e->isz = rev ? le16(sb + 88) : 128;
    e->blocos = le32(sb + 4);
    e->sparse = rev && (ro & 0x0001u);
    e->resize = (compat & EXT_COMPAT_RESIZE) != 0;
    e->rgdt = e->resize ? le16(sb + 206) : 0;
    if (!e->bpg || e->bpg > e->bs * 8 || !e->ipg || e->ipg > e->bs * 8 || e->isz < 128 || e->isz > e->bs ||
        (e->isz & (e->isz - 1)) || e->fdb != (e->bs == 1024 ? 1u : 0u) || e->blocos <= e->fdb) {
        msg_txt(m, max, "el superbloque de %s tiene datos incoherentes", vol);
        return ERRO_FORMATO;
    }
    e->itb = (e->ipg * e->isz + e->bs - 1) / e->bs;
    e->grupos = (e->blocos - e->fdb + e->bpg - 1) / e->bpg;
    e->gdt = (e->grupos * 32 + e->bs - 1) / e->bs;
    if (le32(sb + 0) != e->ipg * e->grupos) { msg_txt(m, max, "el superbloque de %s no cuadra (inodos por grupo)", vol); return ERRO_FORMATO; }
    e->gd_cap = e->grupos + grupos_extra;
    uint32_t gd_blocos = (e->gd_cap * 32 + e->bs - 1) / e->bs;
    if (gd_blocos < e->gdt) gd_blocos = e->gdt;
    e->gd_bytes = gd_blocos * e->bs;
    e->gd = aloca_zerado(e->gd_bytes);
    if (!e->gd) { msg_txt(m, max, "sin memoria"); return ERRO_SEM_MEMORIA; }
    if ((r = ext_ler(e, e->fdb + 1, e->gdt, e->gd)) < 0) {
        msg_txt(m, max, "no se puede leer la tabla de grupos de %s: %s", vol, nome_erro(r));
        ext_fechar(e);
        return r;
    }
    return 0;
}

/* Bloques de metadatos al principio del grupo g (superbloque, tabla y reservados de la
 * tabla si los lleva; mapas y tabla de inodos van donde diga su descriptor). */
static uint32_t ext_cab(const ext_t *e, uint32_t g, uint32_t gdt, uint32_t rgdt)
{
    return ext_tem_sb(e, g) ? 1 + gdt + rgdt : 0;
}

/* ------------------------------------------------------------ ext2: comprobar */

static int verificar_ext2(const char *vol, char *m, int max)
{
    ext_t e;
    int r = ext_abrir(&e, vol, 0, m, max);
    if (r < 0) return r;
    uint32_t estado = le16(e.sb + 58);
    if (estado & 2) { msg_txt(m, max, "el ext2 de %s tiene errores marcados: pásale e2fsck antes", vol); r = ERRO_FORMATO; goto fim; }
    if (!(estado & 1)) { msg_txt(m, max, "el ext2 de %s no se desmontó bien la última vez: pásale e2fsck antes", vol); r = ERRO_FORMATO; goto fim; }
    if ((uint64_t)e.blocos * e.spb > e.setores_part) {
        msg_txt(m, max, "el ext2 de %s es más grande que su partición (%u MB en %u MB)", vol,
                (uint32_t)((uint64_t)e.blocos * e.spb / 2048), e.setores_part / 2048);
        r = ERRO_FORMATO; goto fim;
    }
    uint8_t *mapa = aloca(e.bs);
    if (!mapa) { r = ERRO_SEM_MEMORIA; goto fim; }
    uint32_t livres_b = 0, livres_i = 0;
    for (uint32_t g = 0; g < e.grupos && r == 0; g++) {
        uint8_t *d = ext_gd(&e, g);
        uint32_t ini = ext_ini(&e, g), tam = ext_tam(&e, g, e.blocos);
        uint32_t bb = le32(d), ib = le32(d + 4), it = le32(d + 8);
        if (bb < ini || bb >= ini + tam || ib < ini || ib >= ini + tam || it < ini || it + e.itb > ini + tam) {
            msg_txt(m, max, "el grupo %u de %s tiene sus mapas fuera de sitio (o usa flex_bg)", g, vol);
            r = ERRO_FORMATO; break;
        }
        if ((r = ext_ler(&e, bb, 1, mapa)) < 0) { msg_txt(m, max, "error de lectura en %s", vol); break; }
        uint32_t z = ceros(mapa, tam);
        if (z != le16(d + 12)) {
            msg_txt(m, max, "el grupo %u de %s dice %u bloques libres y su mapa tiene %u: pásale e2fsck antes", g, vol, le16(d + 12), z);
            r = ERRO_FORMATO; break;
        }
        livres_b += z;
        if ((r = ext_ler(&e, ib, 1, mapa)) < 0) { msg_txt(m, max, "error de lectura en %s", vol); break; }
        z = ceros(mapa, e.ipg);
        if (z != le16(d + 14)) {
            msg_txt(m, max, "el grupo %u de %s dice %u inodos libres y su mapa tiene %u: pásale e2fsck antes", g, vol, le16(d + 14), z);
            r = ERRO_FORMATO; break;
        }
        livres_i += z;
    }
    libera(mapa);
    if (r == 0) msg_txt(m, max, "ext2 de %s comprobado: %u grupos, %u de %u MB libres, %u inodos libres", vol, e.grupos,
                        (uint32_t)((uint64_t)livres_b * e.bs / 1048576), (uint32_t)((uint64_t)e.blocos * e.bs / 1048576), livres_i);
fim:
    ext_fechar(&e);
    return r;
}

/* ------------------------------------------------------------ FAT32: comprobar */

static int verificar_fat32(const char *vol, char *m, int max)
{
    info_bloco_t inf;
    uint8_t s[512];
    int r = blocos_info(vol, &inf);
    if (r < 0 || (r = blocos_ler(vol, 0, 1, s)) < 0) { msg_txt(m, max, "%s: %s", vol, nome_erro(r)); return r; }
    uint32_t bps = le16(s + 11), spc = s[13], rsvd = le16(s + 14), nf = s[16], tot = le32(s + 32), fatsz = le32(s + 36), raiz = le32(s + 44);
    if (bps != 512 || !spc || (spc & (spc - 1)) || !rsvd || !nf || nf > 2 || !fatsz || tot <= rsvd + nf * fatsz) {
        msg_txt(m, max, "el sector de arranque del FAT32 de %s tiene datos incoherentes", vol);
        return ERRO_FORMATO;
    }
    if (tot > inf.setores) { msg_txt(m, max, "el FAT32 de %s es más grande que su partición", vol); return ERRO_FORMATO; }
    uint32_t clusters = (tot - rsvd - nf * fatsz) / spc;
    if ((uint64_t)(clusters + 2) * 4 > (uint64_t)fatsz * 512) { msg_txt(m, max, "la FAT de %s es más pequeña que el volumen", vol); return ERRO_FORMATO; }
    if (raiz < 2 || raiz >= clusters + 2) { msg_txt(m, max, "la carpeta raíz de %s está fuera del volumen", vol); return ERRO_FORMATO; }
    const uint32_t lote = 64;
    uint8_t *a = aloca(lote * 512), *b = nf > 1 ? aloca(lote * 512) : NULL;
    if (!a || (nf > 1 && !b)) { if (a) libera(a); if (b) libera(b); return ERRO_SEM_MEMORIA; }
    uint32_t usados = (clusters + 2) * 4, setores = (usados + 511) / 512, livres = 0;
    r = 0;
    for (uint32_t sec = 0; sec < setores && r == 0; sec += lote) {
        uint32_t q = setores - sec > lote ? lote : setores - sec;
        if ((r = blocos_ler(vol, rsvd + sec, q, a)) < 0) { msg_txt(m, max, "error de lectura en la FAT de %s", vol); break; }
        if (b) {
            if ((r = blocos_ler(vol, rsvd + fatsz + sec, q, b)) < 0) { msg_txt(m, max, "error de lectura en la FAT de %s", vol); break; }
            if (memcmp(a, b, q * 512) != 0) {
                msg_txt(m, max, "las dos FAT de %s no son iguales (cerca del sector %u): pásale fsck.vfat o chkdsk antes", vol, sec);
                r = ERRO_FORMATO; break;
            }
        }
        for (uint32_t i = 0; i < q * 128; i++) {
            uint32_t c = sec * 128 + i;
            if (c < 2) continue;
            if (c >= clusters + 2) break;
            uint32_t v = le32(a + i * 4) & 0x0FFFFFFFu;
            if (!v) livres++;
            else if ((v < 2 || v >= clusters + 2) && v < 0x0FFFFFF7u) {
                msg_txt(m, max, "la FAT de %s tiene un enlace roto (clúster %u -> %u): pásale fsck.vfat o chkdsk antes", vol, c, v);
                r = ERRO_FORMATO; break;
            }
        }
    }
    libera(a);
    if (b) libera(b);
    if (r == 0) msg_txt(m, max, "FAT32 de %s comprobado: %u clústeres, %u MB libres", vol, clusters,
                        (uint32_t)((uint64_t)livres * spc / 2048));
    return r;
}

int fs_verificar(const char *vol, char *m, int max)
{
    char tipo[16];
    if (m && max) m[0] = 0;
    if (!fs_tamanho(vol, tipo, sizeof tipo)) { msg_txt(m, max, "%s no tiene un sistema de archivos conocido", vol); return ERRO_FORMATO; }
    if (compara_texto(tipo, "FAT32") == 0) return verificar_fat32(vol, m, max);
    return verificar_ext2(vol, m, max);
}

/* ------------------------------------------------------------ FAT32: encoger (D318) */

/* Un FAT32 encoge sin mover datos: el volumen se corta por el final solo si todos los clústeres
 * que desaparecen están libres, ninguna entrada de la FAT apunta a ellos y la carpeta raíz queda
 * dentro. La FAT no cambia de sitio ni de tamaño (queda más grande de lo necesario, como las que
 * se preparan para crecer, D213): solo cambian el total de sectores del sector de arranque (y su
 * copia) y el FSInfo. Por debajo de 65.525 clústeres el volumen dejaría de ser FAT32 (Windows lo
 * leería como FAT16): eso se niega. */
#define FAT32_MIN_CLUSTERS 65525u

typedef struct { uint32_t spc, rsvd, nf, tot, fatsz, raiz, dados, clusters; } fat_geo_t;

static int fat_geo(const char *vol, uint8_t *s, fat_geo_t *g, char *m, int max)
{
    int r = blocos_ler(vol, 0, 1, s);
    if (r < 0) { msg_txt(m, max, "%s: %s", vol, nome_erro(r)); return r; }
    if (s[510] != 0x55 || s[511] != 0xAA || memcmp(s + 82, "FAT32   ", 8) != 0) { msg_txt(m, max, "%s no es FAT32", vol); return ERRO_FORMATO; }
    g->spc = s[13]; g->rsvd = le16(s + 14); g->nf = s[16]; g->tot = le32(s + 32); g->fatsz = le32(s + 36); g->raiz = le32(s + 44);
    if (!g->spc || !g->nf || !g->fatsz || g->tot <= g->rsvd + g->nf * g->fatsz) { msg_txt(m, max, "%s: FAT32 con datos incoherentes", vol); return ERRO_FORMATO; }
    g->dados = g->rsvd + g->nf * g->fatsz;
    g->clusters = (g->tot - g->dados) / g->spc;
    return 0;
}

/* El clúster usado más alto (2.. clusters+1), o 0 si la FAT no se puede leer. Además, en *apunta,
 * el destino más alto de un enlace (para ver que nadie apunta a lo que se corta). */
static uint32_t fat_ultimo_usado(const char *vol, const fat_geo_t *g, uint32_t *apunta)
{
    const uint32_t lote = 64;
    uint8_t *a = aloca(lote * 512);
    if (!a) return 0;
    uint32_t setores = ((g->clusters + 2) * 4 + 511) / 512, ult = 2, alto = 0;
    for (uint32_t sec = 0; sec < setores; sec += lote) {
        uint32_t q = setores - sec > lote ? lote : setores - sec;
        if (blocos_ler(vol, g->rsvd + sec, q, a) < 0) { libera(a); return 0; }
        for (uint32_t i = 0; i < q * 128; i++) {
            uint32_t c = sec * 128 + i;
            if (c < 2) continue;
            if (c >= g->clusters + 2) break;
            uint32_t v = le32(a + i * 4) & 0x0FFFFFFFu;
            if (!v) continue;
            ult = c;
            if (v >= 2 && v < 0x0FFFFFF7u && v > alto) alto = v;
        }
    }
    libera(a);
    if (g->raiz > ult) ult = g->raiz;
    if (apunta) *apunta = alto;
    return ult;
}

/* Lo mínimo (sectores, múltiplo de 1 MB) a lo que se puede encoger un FAT32 sin mover datos:
 * hasta el último clúster en uso, más 1 MB de margen, y nunca por debajo de 65.525 clústeres. */
uint32_t fs_fat32_minimo(const char *vol)
{
    uint8_t s[512];
    fat_geo_t g;
    char m[8];
    if (fat_geo(vol, s, &g, m, sizeof m) < 0) return 0;
    uint32_t alto = 0, ult = fat_ultimo_usado(vol, &g, &alto);
    if (!ult) return 0;
    if (alto > ult) ult = alto;
    uint32_t cl = ult - 1;   /* clústeres 2..ult */
    if (cl < FAT32_MIN_CLUSTERS) cl = FAT32_MIN_CLUSTERS;
    uint64_t setores = (uint64_t)g.dados + (uint64_t)cl * g.spc + 2048;
    setores = (setores + 2047) / 2048 * 2048;
    if (setores > g.tot) setores = (uint64_t)g.tot / 2048 * 2048 + (g.tot % 2048 ? 2048 : 0);
    return (uint32_t)setores;
}

/* Lo mínimo de un FAT32 o un ext2 (0 si no se reconoce). */
uint32_t fs_minimo(const char *vol)
{
    char tipo[16];
    if (!fs_tamanho(vol, tipo, sizeof tipo)) return 0;
    return compara_texto(tipo, "FAT32") == 0 ? fs_fat32_minimo(vol) : fs_ext2_minimo(vol);
}

int fs_reduzir_fat32(const char *vol, uint32_t setores, char *m, int max)
{
    uint8_t s[512];
    fat_geo_t g;
    int r = fat_geo(vol, s, &g, m, max);
    if (r < 0) return r;
    if (setores >= g.tot) { msg_txt(m, max, "el FAT32 de %s ya mide %u MB o menos", vol, g.tot / 2048); return 0; }
    if (setores <= g.dados) { msg_txt(m, max, "%u MB no dan ni para la FAT de %s", setores / 2048, vol); return ERRO_INVALIDO; }
    uint32_t novos = (setores - g.dados) / g.spc;
    if (novos < FAT32_MIN_CLUSTERS) {
        uint32_t min = (uint32_t)(((uint64_t)g.dados + (uint64_t)FAT32_MIN_CLUSTERS * g.spc + 2047) / 2048);
        msg_txt(m, max, "por debajo de %u MB el volumen de %s dejaría de ser FAT32 (menos de 65.525 clústeres)", min, vol);
        return ERRO_INVALIDO;
    }
    uint32_t alto = 0, ult = fat_ultimo_usado(vol, &g, &alto);
    if (!ult) { msg_txt(m, max, "no se puede leer la FAT de %s", vol); return ERRO_FORMATO; }
    if (ult >= novos + 2 || alto >= novos + 2) {
        uint32_t min = fs_fat32_minimo(vol);
        msg_txt(m, max, "hay datos al final del FAT32 de %s: como mucho se puede encoger hasta %u MB (no mueve datos; "
                        "desfragmentar desde Windows puede juntarlos al principio)", vol, min / 2048);
        return ERRO_INVALIDO;
    }
    /* El sector de arranque: primero la copia (sector 6), después el principal; luego el FSInfo. Si
     * se corta entre los dos, el principal sigue diciendo el tamaño viejo, que cabe en la partición
     * (la tabla se cambia después, particoes_acoes.c). */
    uint32_t velhos = g.clusters;
    po32(s + 32, setores);
    if ((r = blocos_escrever(vol, 6, 1, s)) < 0 || (r = blocos_escrever(vol, 0, 1, s)) < 0) {
        msg_txt(m, max, "no se puede escribir el sector de arranque de %s: %s", vol, nome_erro(r));
        return r;
    }
    uint32_t fsi = le16(s + 48);
    if (fsi && fsi < g.rsvd && blocos_ler(vol, fsi, 1, s) == 0 && le32(s) == 0x41615252) {
        uint32_t livres = le32(s + 488), prox = le32(s + 492);
        if (livres != 0xFFFFFFFFu) po32(s + 488, livres >= velhos - novos ? livres - (velhos - novos) : 0xFFFFFFFFu);
        if (prox != 0xFFFFFFFFu && prox >= novos + 2) po32(s + 492, 0xFFFFFFFFu);
        blocos_escrever(vol, fsi, 1, s);
        if (fsi + 6 < g.rsvd) blocos_escrever(vol, fsi + 6, 1, s);
    }
    msg_txt(m, max, "el FAT32 de %s ha encogido de %u a %u MB", vol, g.tot / 2048, setores / 2048);
    registrar("redimensao: %s FAT32 encoge de %u a %u sectores", vol, g.tot, setores);
    return 1;
}

/* ------------------------------------------------------------ ext2: redimensionar */

/* Grupos y bloques de un ext2 de "blocos" bloques con esta geometría: el último grupo,
 * si no da para sus metadatos y 50 bloques, se descarta (como mke2fs). "reserva" = bloques de
 * la tabla más sus reservados (resize_inode), que no cambian; 0 = solo la tabla. */
static void ext_geometria(const ext_t *e, uint32_t reserva, uint32_t *blocos, uint32_t *grupos, uint32_t *gdt)
{
    for (int volta = 0; volta < 3; volta++) {
        *grupos = (*blocos - e->fdb + e->bpg - 1) / e->bpg;
        *gdt = (*grupos * 32 + e->bs - 1) / e->bs;
        uint32_t ult = *grupos - 1, tam = *blocos - ext_ini(e, ult);
        uint32_t cab = ext_tem_sb(e, ult) ? 1 + (reserva > *gdt ? reserva : *gdt) : 0;
        if (*grupos > 1 && tam < cab + 2 + e->itb + 50) { *blocos = ext_ini(e, ult); continue; }
        break;
    }
}

/* El bloque doble indirecto del inodo 7 (el de resize_inode); 0 si no tiene. */
static int inodo7(ext_t *e, uint8_t *bloco, uint32_t *b_inodo, uint32_t *desl, uint32_t *dind)
{
    uint32_t it = le32(ext_gd(e, 0) + 8), pos = 6 * e->isz;
    *b_inodo = it + pos / e->bs;
    *desl = pos % e->bs;
    int r = ext_ler(e, *b_inodo, 1, bloco);
    if (r < 0) return r;
    *dind = le32(bloco + *desl + 40 + 13 * 4);
    if (*dind >= e->blocos) *dind = 0;
    return 0;
}

/* Lo mínimo a lo que se puede encoger sin mover datos (sectores, redondeado a 1 MB). */
uint32_t fs_ext2_minimo(const char *vol)
{
    ext_t e;
    char m[8];
    if (ext_abrir(&e, vol, 0, m, sizeof m) < 0) return 0;
    uint8_t *mapa = aloca(e.bs);
    uint32_t minimo = 0;
    if (mapa) {
        for (uint32_t g = e.grupos; g-- > 0 && !minimo;) {
            uint8_t *d = ext_gd(&e, g);
            uint32_t ini = ext_ini(&e, g), tam = ext_tam(&e, g, e.blocos);
            bool inodos = false;
            if (ext_ler(&e, le32(d + 4), 1, mapa) < 0) break;
            for (uint32_t i = 0; i < e.ipg && !inodos; i++) inodos = bit_de(mapa, i);
            if (ext_ler(&e, le32(d), 1, mapa) < 0) break;
            uint32_t meta = le32(d + 8) + e.itb - ini, ult = 0;   /* los metadatos del grupo van al principio */
            for (uint32_t i = tam; i-- > meta;) if (bit_de(mapa, i)) { ult = i + 1; break; }
            if (ult) minimo = ini + ult;
            else if (inodos || g == 0) minimo = ini + meta;
        }
        libera(mapa);
    }
    uint32_t s = minimo ? minimo + 50 + e.itb : 0;   /* que el último grupo dé para algo */
    s = (uint32_t)(((uint64_t)s * e.spb + 2047) / 2048 * 2048);
    ext_fechar(&e);
    return s;
}

static int escrever_metadados(ext_t *e, uint32_t grupos, uint32_t gdt, uint8_t *sb_novo)
{
    int r;
    /* Las copias de la tabla y del superbloque, de atrás adelante; el principal, el último. */
    for (uint32_t g = grupos; g-- > 0;) {
        if (!ext_tem_sb(e, g)) continue;
        if ((r = ext_escrever(e, ext_ini(e, g) + 1, gdt, e->gd)) < 0) return r;
        if (g) {
            po16(sb_novo + 90, g);
            if ((r = blocos_escrever(e->vol, ext_ini(e, g) * e->spb, 2, sb_novo)) < 0) return r;
        }
    }
    po16(sb_novo + 90, 0);
    return blocos_escrever(e->vol, 2, 2, sb_novo);
}

/* resize_inode (como ext2fs_create_resize_inode): el doble indirecto del inodo 7 apunta a los
 * bloques reservados de la tabla del grupo 0 (en la posición de cada uno dentro de la tabla,
 * módulo bloques/4), y cada uno de esos es un indirecto con la lista de sus copias en los
 * grupos con copia del superbloque, por orden. Se rehace entero con la geometría nueva. */
static int refazer_resize(ext_t *e, uint32_t grupos, uint32_t gdt, uint32_t rgdt, uint32_t dind,
                          uint8_t *ino, uint32_t b7, uint32_t d7)
{
    uint32_t apb = e->bs / 4, nb = 0;
    uint8_t *dbuf = aloca_zerado(e->bs), *ibuf = aloca(e->bs);
    int r = 0;
    if (!dbuf || !ibuf) { r = ERRO_SEM_MEMORIA; goto fim; }
    for (uint32_t g = 1; g < grupos; g++) if (ext_tem_sb(e, g)) nb++;
    if (nb > apb) { r = ERRO_LIMITE; goto fim; }
    for (uint32_t k = 0; k < rgdt && r == 0; k++) {
        uint32_t pos = gdt + k, pblk = e->fdb + 1 + pos, n = 0;
        po32(dbuf + (pos % apb) * 4, pblk);
        memset(ibuf, 0, e->bs);
        for (uint32_t g = 1; g < grupos; g++)
            if (ext_tem_sb(e, g)) po32(ibuf + 4 * n++, ext_ini(e, g) + 1 + pos);
        r = ext_escrever(e, pblk, 1, ibuf);
    }
    if (r == 0) r = ext_escrever(e, dind, 1, dbuf);
    if (r == 0) {
        po32(ino + d7 + 28, (1 + rgdt * (1 + nb)) * e->spb);   /* i_blocks, en sectores de 512 */
        r = ext_escrever(e, b7, 1, ino);
    }
fim:
    if (dbuf) libera(dbuf);
    if (ibuf) libera(ibuf);
    return r;
}

/* D388: cuánto escribirá fs_redimensionar_ext2 al crecer hasta "setores": grupos nuevos y MB de sus tablas de
 * inodos y mapas (lo que casi todo el tiempo se va en poner a cero). Solo lee el superbloque. Es lo que se escribe
 * (desde D388 todo, aunque ya sea cero). 0 si el volumen no es un ext2 que se pueda leer. */
int fs_ext2_estimar(const char *vol, uint32_t setores, fs_ext2_est_t *est)
{
    uint8_t sb[1024];
    memset(est, 0, sizeof *est);
    if (blocos_ler(vol, 2, 2, sb) < 0 || le16(sb + 56) != 0xEF53 || le32(sb + 24) > 2) return ERRO_FORMATO;
    uint32_t bs = 1024u << le32(sb + 24), bpg = le32(sb + 32), ipg = le32(sb + 40), fdb = le32(sb + 20), blocos = le32(sb + 4);
    uint32_t isz = le32(sb + 76) ? le16(sb + 88) : 128;
    if (!bpg || !ipg || !isz || blocos <= fdb) return ERRO_FORMATO;
    uint32_t novos = setores / (bs / 512);
    if (novos <= blocos) return 0;
    uint32_t g0 = (blocos - fdb + bpg - 1) / bpg, g1 = (novos - fdb + bpg - 1) / bpg;
    uint32_t itb = (uint32_t)(((uint64_t)ipg * isz + bs - 1) / bs);
    est->grupos = g1 > g0 ? g1 - g0 : 0;
    est->mb = (uint32_t)((uint64_t)est->grupos * (itb + 2) * bs / 1048576);
    return 0;
}

/* D388: hasta dónde puede crecer el ext2 sin rehacerlo (sectores, tope de 32 bits = 2 TB, múltiplo de 1 MB): lo que da
 * la tabla de grupos (con resize_inode, los GDT reservados; sin él, los que hay). 0 si no se reconoce. */
uint32_t fs_ext2_maximo(const char *vol)
{
    uint8_t sb[1024];
    if (blocos_ler(vol, 2, 2, sb) < 0 || le16(sb + 56) != 0xEF53 || le32(sb + 24) > 2) return 0;
    uint32_t rev = le32(sb + 76), bs = 1024u << le32(sb + 24), bpg = le32(sb + 32), fdb = le32(sb + 20), blocos = le32(sb + 4);
    if (!bpg || blocos <= fdb) return 0;
    uint32_t grupos = (blocos - fdb + bpg - 1) / bpg, gdt = (grupos * 32 + bs - 1) / bs;
    uint32_t cap = gdt + ((rev && (le32(sb + 92) & EXT_COMPAT_RESIZE)) ? le16(sb + 206) : 0);
    uint64_t sec = ((uint64_t)fdb + (uint64_t)cap * (bs / 32) * bpg) * (bs / 512);
    if (sec > 0xFFFFFFFFull) sec = 0xFFFFFFFFull;
    return (uint32_t)(sec / 2048 * 2048);
}

int fs_redimensionar_ext2(const char *vol, uint32_t setores, fs_progresso_t prog, void *ctx, char *m, int max)
{
    ext_t e;
    uint32_t extra = 0;
    {   /* cuántos grupos nuevos como mucho: para reservar sitio para sus descriptores */
        uint8_t sb[1024];
        if (blocos_ler(vol, 2, 2, sb) == 0 && le16(sb + 56) == 0xEF53 && le32(sb + 32) && le32(sb + 24) <= 2)
            extra = setores / ((1024u << le32(sb + 24)) / 512) / le32(sb + 32) + 2;
    }
    int r = ext_abrir(&e, vol, extra, m, max);
    if (r < 0) return r;
    uint8_t *mapa = aloca(e.bs), *ino = aloca(e.bs), *sbn = aloca(1024);
    uint8_t *zeros = NULL;
    if (!mapa || !ino || !sbn) { r = ERRO_SEM_MEMORIA; msg_txt(m, max, "sin memoria"); goto fim; }
    if (setores > e.setores_part) setores = e.setores_part;
    /* Con resize_inode, la tabla y sus reservados ocupan siempre lo mismo ("reserva"): lo que
     * crece la tabla sale de los reservados, y lo que encoge vuelve a ellos. */
    uint32_t reserva = e.resize ? e.gdt + e.rgdt : 0;
    uint32_t novos = setores / e.spb, G1, gdt1;
    ext_geometria(&e, reserva, &novos, &G1, &gdt1);
    uint32_t G0 = e.grupos, velhos = e.blocos;
    if (novos == velhos) { msg_txt(m, max, "el ext2 de %s ya mide eso", vol); r = 0; goto fim; }
    if (G1 > e.gd_cap) { msg_txt(m, max, "demasiados grupos"); r = ERRO_LIMITE; goto fim; }
    bool crescer = novos > velhos;
    if (gdt1 > (e.resize ? reserva : e.gdt)) {
        uint32_t gmax = (e.resize ? reserva : e.gdt) * (e.bs / 32);
        msg_txt(m, max, "la tabla de grupos del ext2 de %s no tiene sitio para tantos: como mucho crece hasta unos %u MB "
                "(crecer más pide resize2fs en otro sistema)", vol, (uint32_t)(((uint64_t)e.fdb + (uint64_t)gmax * e.bpg) * e.bs / 1048576));
        r = ERRO_LIMITE; goto fim;
    }
    uint32_t rgdt1 = e.resize ? reserva - gdt1 : 0;
    bool manter = e.resize && rgdt1 > 0, tirar = e.resize && !rgdt1;   /* sin reservados ya no hace falta */
    uint32_t b7 = 0, d7 = 0, dind = 0;
    if (e.resize) {
        if ((r = inodo7(&e, ino, &b7, &d7, &dind)) < 0) { msg_txt(m, max, "error de lectura en %s", vol); goto fim; }
        if (!dind) { msg_txt(m, max, "el resize_inode del ext2 de %s está mal: pásale e2fsck antes", vol); r = ERRO_FORMATO; goto fim; }
        if (manter && dind >= novos) { msg_txt(m, max, "el ext2 de %s no se puede encoger tanto (el bloque %u de resize_inode quedaría fuera)", vol, dind); r = ERRO_LIMITE; goto fim; }
    }

    /* --- Encoger: ¿está libre todo lo que se corta? --- */
    if (!crescer) {
        for (uint32_t g = G1; g < G0; g++) {
            uint8_t *d = ext_gd(&e, g);
            uint32_t ini = ext_ini(&e, g), tam = ext_tam(&e, g, velhos);
            uint32_t bb = le32(d), ib = le32(d + 4), it = le32(d + 8);
            if ((r = ext_ler(&e, ib, 1, mapa)) < 0) goto erro_es;
            for (uint32_t i = 0; i < e.ipg; i++)
                if (bit_de(mapa, i)) {
                    msg_txt(m, max, "hay archivos al final del ext2 de %s (el inodo %u, en el grupo %u): encoger tanto pide moverlos, "
                            "y esta versión no mueve datos (como mucho, hasta %u MB)", vol, g * e.ipg + i + 1, g, fs_ext2_minimo(vol) / 2048);
                    r = ERRO_OCUPADO; goto fim;
                }
            if ((r = ext_ler(&e, bb, 1, mapa)) < 0) goto erro_es;
            uint32_t cab = ext_cab(&e, g, e.gdt, e.rgdt);
            for (uint32_t i = 0; i < tam; i++) {
                uint32_t b = ini + i;
                if (!bit_de(mapa, i) || i < cab || b == bb || b == ib || (b >= it && b < it + e.itb) || (tirar && b == dind)) continue;
                msg_txt(m, max, "hay datos al final del ext2 de %s (el bloque %u): encoger tanto pide moverlos, "
                        "y esta versión no mueve datos (como mucho, hasta %u MB)", vol, b, fs_ext2_minimo(vol) / 2048);
                r = ERRO_OCUPADO; goto fim;
            }
        }
        /* El trozo que se corta del último grupo que queda. */
        uint32_t g = G1 - 1, ini = ext_ini(&e, g), t1 = ext_tam(&e, g, novos), t0 = ext_tam(&e, g, velhos);
        uint8_t *d = ext_gd(&e, g);
        if (le32(d) >= ini + t1 || le32(d + 4) >= ini + t1 || le32(d + 8) + e.itb > ini + t1) {
            msg_txt(m, max, "el ext2 de %s no se puede encoger tanto (los metadatos del grupo %u quedarían fuera)", vol, g);
            r = ERRO_LIMITE; goto fim;
        }
        if ((r = ext_ler(&e, le32(d), 1, mapa)) < 0) goto erro_es;
        for (uint32_t i = t1; i < t0; i++)
            if (bit_de(mapa, i) && !(tirar && ini + i == dind)) {
                msg_txt(m, max, "hay datos al final del ext2 de %s (el bloque %u): encoger tanto pide moverlos, "
                        "y esta versión no mueve datos (como mucho, hasta %u MB)", vol, ini + i, fs_ext2_minimo(vol) / 2048);
                r = ERRO_OCUPADO; goto fim;
            }
    }

    /* --- Crecer: los grupos nuevos, que nadie mira todavía --- */
    uint32_t cab1 = manter ? gdt1 + rgdt1 : gdt1;   /* tras el superbloque, en los grupos con copia */
    fs_zerar_t z;   /* la tabla de inodos de los grupos nuevos, a cero siempre (D388: aunque parezca que ya lo está) */
    memset(&z, 0, sizeof z);
    if (crescer && G1 > G0) {
        zeros = aloca_zerado(e.bs * 2);   /* aquí, los dos mapas de un grupo nuevo (van seguidos) */
        if (!zeros) { r = ERRO_SEM_MEMORIA; goto fim; }
        if (prog) prog(0, ctx);   /* D388: de aquí en adelante, el avance es lineal en los grupos (0..99 %) */
        for (uint32_t g = G0; g < G1; g++) {
            uint32_t ini = ext_ini(&e, g), tam = ext_tam(&e, g, novos);
            uint32_t bb = ini + (ext_tem_sb(&e, g) ? 1 + cab1 : 0), ib = bb + 1, it = bb + 2;
            if ((r = fs_zerar(vol, it * e.spb, e.itb * e.spb, &z)) < 0) goto erro_es;
            uint8_t *mb = zeros, *mi = zeros + e.bs;
            memset(zeros, 0, e.bs * 2);
            for (uint32_t i = 0; i < it + e.itb - ini; i++) bit_pon(mb, i);
            for (uint32_t i = tam; i < e.bs * 8; i++) bit_pon(mb, i);
            uint32_t livres = ceros(mb, tam);
            for (uint32_t i = e.ipg; i < e.bs * 8; i++) bit_pon(mi, i);
            if ((r = ext_escrever(&e, bb, 2, zeros)) < 0) goto erro_es;   /* los dos mapas de una vez */
            uint8_t *d = ext_gd(&e, g);
            memset(d, 0, 32);
            po32(d, bb); po32(d + 4, ib); po32(d + 8, it);
            po16(d + 12, livres); po16(d + 14, e.ipg);
            if (prog) prog((int)((uint64_t)(g - G0 + 1) * 99 / (G1 - G0)), ctx);
        }
    }

    /* --- Los grupos de antes que siguen: el último (que crece o se corta), los que pierden
     *     bloques de la tabla (sin resize_inode, al encoger) y el del bloque de resize_inode
     *     si este desaparece. --- */
    uint32_t Gmin = G0 < G1 ? G0 : G1;
    for (uint32_t g = 0; g < Gmin; g++) {
        bool ultimo = g == Gmin - 1;
        uint32_t livra_de = 0, livra_ate = 0;
        if (ext_tem_sb(&e, g) && !e.resize && e.gdt > gdt1) { livra_de = 1 + gdt1; livra_ate = 1 + e.gdt; }
        uint32_t ini = ext_ini(&e, g);
        bool dind_aqui = tirar && dind >= ini && dind < ini + e.bpg;
        if (!ultimo && !livra_ate && !dind_aqui) continue;
        uint8_t *d = ext_gd(&e, g);
        if ((r = ext_ler(&e, le32(d), 1, mapa)) < 0) goto erro_es;
        for (uint32_t i = livra_de; i < livra_ate; i++) bit_quita(mapa, i);
        if (dind_aqui) bit_quita(mapa, dind - ini);
        uint32_t tam = ext_tam(&e, g, novos);
        if (ultimo) {
            uint32_t t0 = ext_tam(&e, g, velhos);
            for (uint32_t i = t0; i < tam; i++) bit_quita(mapa, i);       /* crece: el relleno pasa a ser bloques libres */
            for (uint32_t i = tam; i < e.bs * 8; i++) bit_pon(mapa, i);   /* encoge: lo de fuera, relleno */
        }
        po16(d + 12, ceros(mapa, tam));
        if ((r = ext_escrever(&e, le32(d), 1, mapa)) < 0) goto erro_es;
    }
    if (tirar) {   /* sin reservados: el inodo 7, a cero */
        memset(ino + d7, 0, e.isz);
        if ((r = ext_escrever(&e, b7, 1, ino)) < 0) goto erro_es;
    } else if (manter && (r = refazer_resize(&e, G1, gdt1, rgdt1, dind, ino, b7, d7)) < 0) goto erro_es;

    /* --- El superbloque nuevo y todas las copias --- */
    memcpy(sbn, e.sb, 1024);
    uint32_t livres_b = 0, livres_i = 0;
    for (uint32_t g = 0; g < G1; g++) { livres_b += le16(ext_gd(&e, g) + 12); livres_i += le16(ext_gd(&e, g) + 14); }
    memset(e.gd + G1 * 32, 0, e.gd_bytes - G1 * 32);   /* los descriptores de grupos que ya no existen */
    uint32_t rsv = le32(e.sb + 8);
    po32(sbn + 0, e.ipg * G1);
    po32(sbn + 4, novos);
    po32(sbn + 8, (uint32_t)((uint64_t)rsv * novos / velhos));
    po32(sbn + 12, livres_b);
    po32(sbn + 16, livres_i);
    if (e.resize) po16(sbn + 206, rgdt1);
    if (tirar) po32(sbn + 92, le32(sbn + 92) & ~EXT_COMPAT_RESIZE);
    if ((r = escrever_metadados(&e, G1, gdt1, sbn)) < 0) goto erro_es;
    if (prog) prog(100, ctx);
    msg_txt(m, max, "el ext2 de %s pasa de %u a %u MB (%u grupos)%s", vol, (uint32_t)((uint64_t)velhos * e.bs / 1048576),
            (uint32_t)((uint64_t)novos * e.bs / 1048576), G1, tirar ? "; ya no le quedan bloques reservados para crecer" : "");
    registrar("redimensao: %s ext2 de %u a %u bloques, grupos %u -> %u, tabla %u -> %u, reservados %u -> %u; tablas de inodos nuevas: "
              "%u MB escritos (siempre, sin fiarse de lo que ya hubiera: D388)", vol, velhos, novos, G0, G1, e.gdt, gdt1, e.rgdt, rgdt1,
              z.escritos / 2048);
    r = 0;
    goto fim;
erro_es:
    msg_txt(m, max, "error de E/S en %s: %s (pásale e2fsck)", vol, nome_erro(r));
fim:
    if (mapa) libera(mapa);
    if (ino) libera(ino);
    if (sbn) libera(sbn);
    if (zeros) libera(zeros);
    ext_fechar(&e);
    return r;
}
