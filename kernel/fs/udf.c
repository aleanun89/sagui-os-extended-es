// SPDX-License-Identifier: GPL-2.0-or-later
/* udf.c - lectura de los CD/DVD con UDF (D233), solo lectura.
 *
 * Muchos DVD de datos y los CD/DVD grabados con ciertos programas (Windows con
 * «Sistema de archivos activo», o con UDF elegido) solo llevan UDF, o UDF y un
 * ISO 9660 mínimo ("UDF bridge"). Aquí se lee lo habitual de las revisiones 1.02
 * a 2.01 (y las 2.50/2.60 que no usan partición de metadatos), de la norma
 * ECMA-167 con el perfil UDF de OSTA:
 *   - el Anchor Volume Descriptor Pointer (etiqueta 2) en el bloque 256 de la
 *     última sesión (o del disco), o en el último bloque, o 256 antes;
 *   - la secuencia de descriptores del volumen (la principal y, si falla, la de
 *     reserva) hasta el terminador (8): el primario (1), las particiones (5) y el
 *     volumen lógico (6) con sus mapas de partición (tipo 1 y la "Sparable" del
 *     tipo 2, que en un disco sin sectores reemplazados es igual que el tipo 1);
 *   - el File Set Descriptor (256) y, de él, el ICB de la raíz;
 *   - File Entry (261) y Extended File Entry (266), con sus descriptores de
 *     extensión short_ad y long_ad, la continuación (Allocation Extent
 *     Descriptor, 258) y los datos dentro de la propia entrada (embebidos);
 *   - directorios de File Identifier Descriptor (257), que pueden cruzar el borde
 *     de un bloque; nombres OSTA CS0 de 8 bits (Latin-1) o de 16 (UTF-16, lo que no
 *     cabe en Latin-1 sale como "?").
 * Lo que no se lee (y se dice en el log): la partición virtual (VAT, CD-R grabados
 * por paquetes), la de metadatos (UDF 2.50 en adelante, Blu-ray), la estrategia
 * 4096 de los ICB y los archivos de más de 4 GB (se ven de 4 GB - 1).
 * Toda etiqueta se comprueba (suma y CRC) antes de usarla: un disco estropeado
 * da "formato no reconocido", nunca una lectura fuera de la partición. */
#include "udf.h"
#include "abi.h"
#include "console.h"
#include "fuso.h"
#include "memoria.h"
#include "montagem.h"
#include "texto.h"

#define BLOCO 2048u
#define MAX_MAPAS 4
#define MAX_PARTS 4
#define MAX_DESC_VDS 64u          /* descriptores de la secuencia del volumen, como mucho */
#define MAX_BLOCOS_LEITURA 16u    /* bloques por lectura directa (32 KB, el búfer del AHCI) */
#define MAX_CONTINUACOES 256      /* bloques de continuación de una lista de extensiones */
#define NOME_UDF 256              /* un nombre UDF descodificado (255 caracteres) */

enum { TAG_PVD = 1, TAG_AVDP = 2, TAG_VDP = 3, TAG_PD = 5, TAG_LVD = 6, TAG_TD = 8,
       TAG_FSD = 256, TAG_FID = 257, TAG_AED = 258, TAG_FE = 261, TAG_EFE = 266 };

typedef struct {
    disco_t *d;
    uint32_t blocos;                       /* del disco (d->setores / 4) */
    int      qtd_mapas;
    uint32_t mapa_ini[MAX_MAPAS], mapa_tam[MAX_MAPAS];   /* referencia de partición -> bloques físicos */
    uint32_t raiz_bloco;                   /* el ICB de la raíz (lógico) */
    uint16_t raiz_ref;
    uint16_t revisao;                      /* 0x0102, 0x0150, 0x0201... */
    char     rotulo[64];
    uint8_t *buf;  uint32_t buf_fis;       /* un bloque de datos (0xFFFFFFFF = ninguno) */
    uint8_t *fe;   uint32_t fe_fis;        /* la última File Entry leída */
    uint8_t *fed;  uint32_t fed_fis;       /* la File Entry del directorio que se recorre */
    uint8_t *aed;                          /* un bloque de continuación de extensiones */
} udf_t;

/* Una entrada ya encontrada: dónde está su File Entry y lo que dice de ella. */
typedef struct {
    uint32_t bloco;                        /* ICB (lógico) */
    uint16_t ref;
    bool     pasta;
    uint32_t tam;                          /* bytes (los de más de 4 GB, 4 GB - 1) */
    uint8_t  data[12];                     /* fecha de modificación (timestamp de ECMA-167) */
} udf_ent_t;

/* Un archivo abierto: su entrada y la última extensión usada (lectura seguida). */
typedef struct {
    uint32_t bloco;
    uint16_t ref;
    uint32_t tam;
    uint32_t ext_ini, ext_tam, ext_fis;    /* [ext_ini, ext_ini + ext_tam) del archivo en ext_fis */
    uint8_t  ext_tipo;                     /* MAPA_* (0 = no hay extensión guardada) */
} udf_arq_t;

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }
static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint64_t le64(const uint8_t *p) { return (uint64_t)le32(p) | (uint64_t)le32(p + 4) << 32; }

/* ------------------------------------------------------------ bloques y etiquetas */

static int ler_fis(udf_t *v, uint32_t fis, uint32_t n, void *buf)
{
    if (fis >= v->blocos || n > v->blocos - fis) return ERRO_ACESSO;
    return disco_ler(v->d, fis * 4, n * 4, buf) < 0 ? ERRO_ACESSO : 0;
}

/* CRC de ECMA-167 7.2.6: CRC-ITU-T (polinomio 0x1021), valor inicial 0. */
static uint16_t crc_itu(const uint8_t *p, uint32_t n)
{
    uint16_t crc = 0;
    for (uint32_t i = 0; i < n; i++) {
        crc ^= (uint16_t)(p[i] << 8);
        for (int b = 0; b < 8; b++) crc = (crc & 0x8000) ? (uint16_t)(crc << 1 ^ 0x1021) : (uint16_t)(crc << 1);
    }
    return crc;
}

/* La etiqueta de 16 bytes de cada descriptor: identificador, suma de los bytes
 * 0-3 y 5-15, versión 2 o 3 y el CRC de los "crc_tam" bytes que la siguen. */
static bool etiqueta_ok(const uint8_t *b, uint16_t id, uint32_t max)
{
    if (le16(b) != id) return false;
    uint8_t soma = 0;
    for (int i = 0; i < 16; i++) if (i != 4) soma = (uint8_t)(soma + b[i]);
    if (soma != b[4]) return false;
    uint16_t versao = le16(b + 2);
    if (versao != 2 && versao != 3) return false;
    uint32_t crc_tam = le16(b + 10);
    if (crc_tam > max - 16) return false;
    return crc_itu(b + 16, crc_tam) == le16(b + 8);
}

/* Solo la suma de la etiqueta (un FID puede cruzar el borde de un bloque y su CRC
 * cubriría bytes que aún no se han leído). */
static bool soma_ok(const uint8_t *b, uint16_t id)
{
    uint8_t soma = 0;
    for (int i = 0; i < 16; i++) if (i != 4) soma = (uint8_t)(soma + b[i]);
    return le16(b) == id && soma == b[4];
}

/* Bloque lógico (partición "ref", bloque "lbn") -> físico. */
static bool fisico(udf_t *v, uint16_t ref, uint32_t lbn, uint32_t n, uint32_t *fis)
{
    if (ref >= v->qtd_mapas || lbn >= v->mapa_tam[ref] || n > v->mapa_tam[ref] - lbn) return false;
    *fis = v->mapa_ini[ref] + lbn;
    return true;
}

/* ------------------------------------------------------------ nombres (OSTA CS0) */

/* "n" bytes de d-characters (con el byte de compresión delante) -> Latin-1. */
static int cs0_para_latin1(const uint8_t *p, uint32_t n, char *s, int max)
{
    int k = 0;
    if (n == 0 || max <= 0) { if (max > 0) s[0] = 0; return 0; }
    uint8_t comp = p[0];
    if (comp == 8 || comp == 254) {
        for (uint32_t i = 1; i < n && k < max - 1; i++) s[k++] = p[i] ? (char)p[i] : '?';
    } else if (comp == 16 || comp == 255) {
        for (uint32_t i = 1; i + 1 < n && k < max - 1; i += 2) {
            uint16_t c = (uint16_t)(p[i] << 8 | p[i + 1]);
            s[k++] = (c && c < 0x100) ? (char)c : '?';
        }
    }
    s[k] = 0;
    return k;
}

/* Un dstring (campo de tamaño fijo cuyo último byte dice cuántos se usan). */
static void dstring(const uint8_t *p, uint32_t campo, char *s, int max)
{
    uint32_t n = p[campo - 1];
    if (n > campo - 1) n = campo - 1;
    cs0_para_latin1(p, n, s, max);
    int k = (int)txt_tamanho(s);
    while (k > 0 && s[k - 1] == ' ') s[--k] = 0;
}

/* ------------------------------------------------------------ el volumen */

/* Busca el Anchor Volume Descriptor Pointer y devuelve las dos secuencias. */
static bool achar_ancora(udf_t *v, uint32_t sessao, uint32_t *vds_ini, uint32_t *vds_n, uint32_t *res_ini, uint32_t *res_n)
{
    uint32_t sitios[4] = { sessao + 256, 256, v->blocos - 1, v->blocos - 257 };
    for (int i = 0; i < 4; i++) {
        uint32_t s = sitios[i];
        if (s >= v->blocos || (i == 1 && sessao == 0) || (i >= 2 && v->blocos < 512)) continue;
        if (ler_fis(v, s, 1, v->buf) < 0 || !etiqueta_ok(v->buf, TAG_AVDP, BLOCO)) continue;
        *vds_n = le32(v->buf + 16) / BLOCO;
        *vds_ini = le32(v->buf + 20);
        *res_n = le32(v->buf + 24) / BLOCO;
        *res_ini = le32(v->buf + 28);
        return true;
    }
    return false;
}

typedef struct {
    int      qtd_parts;
    uint16_t part_num[MAX_PARTS];
    uint32_t part_ini[MAX_PARTS], part_tam[MAX_PARTS];
    bool     tem_lvd;
    uint8_t  lvd[512];                     /* lo que hace falta del volumen lógico (mapas hasta 440+72) */
    char     pvd_rotulo[33];
} vds_t;

/* Recorre una secuencia de descriptores (siguiendo los punteros, etiqueta 3). */
static bool ler_vds(udf_t *v, uint32_t ini, uint32_t n, vds_t *q)
{
    uint32_t lidos = 0;
    while (n && lidos < MAX_DESC_VDS) {
        if (ler_fis(v, ini, 1, v->buf) < 0) return false;
        lidos++;
        uint16_t id = le16(v->buf);
        if (!etiqueta_ok(v->buf, id, BLOCO)) return false;
        if (id == TAG_TD) break;
        if (id == TAG_VDP) {                    /* la secuencia sigue en otra parte */
            n = le32(v->buf + 20) / BLOCO;
            ini = le32(v->buf + 24);
            continue;
        }
        if (id == TAG_PVD && !q->pvd_rotulo[0]) dstring(v->buf + 24, 32, q->pvd_rotulo, sizeof q->pvd_rotulo);
        else if (id == TAG_PD && q->qtd_parts < MAX_PARTS) {
            uint16_t num = le16(v->buf + 22);
            int k = 0;
            while (k < q->qtd_parts && q->part_num[k] != num) k++;   /* una más nueva sustituye a la vieja */
            if (k == q->qtd_parts) q->qtd_parts++;
            q->part_num[k] = num;
            q->part_ini[k] = le32(v->buf + 188);
            q->part_tam[k] = le32(v->buf + 192);
        } else if (id == TAG_LVD) {
            memcpy(q->lvd, v->buf, sizeof q->lvd);
            q->tem_lvd = true;
        }
        ini++;
        n--;
    }
    return q->tem_lvd && q->qtd_parts > 0;
}

/* Lee los descriptores y deja el volumen listo (mapas y raíz). En "porque" queda
 * el motivo si no se puede leer. */
static bool abrir_volume(udf_t *v, disco_t *d, char *porque, int max)
{
    v->d = d;
    v->blocos = d->setores / 4;
    v->buf_fis = v->fe_fis = v->fed_fis = 0xFFFFFFFFu;
    porque[0] = 0;
    uint32_t vi, vn, ri, rn;
    if (v->blocos < 258 || !achar_ancora(v, d->sessao, &vi, &vn, &ri, &rn)) return false;   /* no es UDF */
    vds_t *q = kcalloc(sizeof *q);
    if (!q) return false;
    bool ok = ler_vds(v, vi, vn, q);
    if (!ok) { memset(q, 0, sizeof *q); ok = ler_vds(v, ri, rn, q); }
    if (!ok) { snformatar(porque, (size_t)max, "UDF con los descriptores dañados"); goto fim; }
    const uint8_t *l = q->lvd;
    ok = false;
    if (le32(l + 212) != BLOCO) { snformatar(porque, (size_t)max, "UDF con bloques de %u bytes: no se lee", le32(l + 212)); goto fim; }
    if (memcmp(l + 216 + 1, "*OSTA UDF Compliant", 19) == 0) v->revisao = le16(l + 216 + 24);
    dstring(l + 84, 128, v->rotulo, sizeof v->rotulo);
    if (!v->rotulo[0]) txt_copia_n(v->rotulo, q->pvd_rotulo, sizeof v->rotulo);
    /* Los mapas de partición: la referencia que usan los long_ad es su índice. */
    uint32_t nmapas = le32(l + 268), tam_tab = le32(l + 264), off = 440;
    if (nmapas == 0 || nmapas > MAX_MAPAS || tam_tab > sizeof q->lvd - 440) {
        snformatar(porque, (size_t)max, "UDF con %u mapas de partición: no se lee", nmapas);
        goto fim;
    }
    for (uint32_t i = 0; i < nmapas; i++) {
        if (off + 2 > 440 + tam_tab) goto mapa_mal;
        const uint8_t *m = l + off;
        uint8_t tipo = m[0], tam = m[1];
        uint16_t num;
        if (tam < 6 || off + tam > 440 + tam_tab) goto mapa_mal;
        if (tipo == 1 && tam == 6) num = le16(m + 4);
        else if (tipo == 2 && tam == 64) {
            if (memcmp(m + 5, "*UDF Sparable Partition", 23) == 0) {
                num = le16(m + 38);
                klog("[UDF] %s: partición de reemplazo (Sparable): se lee sin su tabla (disco sin sectores reemplazados)\n", d->nome);
            } else if (memcmp(m + 5, "*UDF Virtual Partition", 22) == 0) {
                snformatar(porque, (size_t)max, "UDF con VAT (CD-R por paquetes): no se lee");
                goto fim;
            } else if (memcmp(m + 5, "*UDF Metadata Partition", 23) == 0) {
                snformatar(porque, (size_t)max, "UDF %x.%02x con metadatos: no se lee", v->revisao >> 8, v->revisao & 0xFF);
                goto fim;
            } else goto mapa_mal;
        } else goto mapa_mal;
        int k = 0;
        while (k < q->qtd_parts && q->part_num[k] != num) k++;
        if (k == q->qtd_parts) goto mapa_mal;
        if (q->part_ini[k] >= v->blocos) { snformatar(porque, (size_t)max, "UDF con la partición fuera del disco"); goto fim; }
        v->mapa_ini[i] = q->part_ini[k];
        v->mapa_tam[i] = q->part_tam[k];
        if (v->mapa_tam[i] > v->blocos - v->mapa_ini[i]) v->mapa_tam[i] = v->blocos - v->mapa_ini[i];   /* grabado a medias */
        off += tam;
    }
    v->qtd_mapas = (int)nmapas;
    /* El File Set Descriptor (long_ad en el "uso del contenido" del volumen lógico). */
    {
        uint32_t fsd_lbn = le32(l + 248 + 4), fis;
        uint16_t fsd_ref = le16(l + 248 + 8);
        if (!fisico(v, fsd_ref, fsd_lbn, 1, &fis) || ler_fis(v, fis, 1, v->buf) < 0 || !etiqueta_ok(v->buf, TAG_FSD, BLOCO)) {
            snformatar(porque, (size_t)max, "UDF sin File Set Descriptor");
            goto fim;
        }
        v->raiz_bloco = le32(v->buf + 400 + 4);
        v->raiz_ref = le16(v->buf + 400 + 8);
    }
    ok = true;
    goto fim;
mapa_mal:
    snformatar(porque, (size_t)max, "UDF con un mapa de partición desconocido");
fim:
    kfree(q);
    v->buf_fis = 0xFFFFFFFFu;
    return ok;
}

/* ------------------------------------------------------------ File Entry y extensiones */

/* Lee la File Entry del ICB (ref, bloco) en "dest" (cache "*cache_fis"). */
static int ler_fe(udf_t *v, uint16_t ref, uint32_t bloco, uint8_t *dest, uint32_t *cache_fis)
{
    uint32_t fis;
    if (!fisico(v, ref, bloco, 1, &fis)) return ERRO_FORMATO;
    if (*cache_fis == fis) return 0;
    *cache_fis = 0xFFFFFFFFu;
    if (ler_fis(v, fis, 1, dest) < 0) return ERRO_ACESSO;
    uint16_t id = le16(dest);
    if ((id != TAG_FE && id != TAG_EFE) || !etiqueta_ok(dest, id, BLOCO)) return ERRO_FORMATO;
    if (le16(dest + 16 + 4) != 4) {   /* estrategia del ICB: solo la 4 (la 4096 es rara) */
        klog("[UDF] %s: ICB con estrategia %u: no se lee\n", v->d->nome, le16(dest + 16 + 4));
        return ERRO_FORMATO;
    }
    *cache_fis = fis;
    return 0;
}

/* Lo que interesa de una File Entry ya leída. */
static void dados_fe(const uint8_t *fe, uint16_t ref, uint32_t bloco, udf_ent_t *e)
{
    bool ext = le16(fe) == TAG_EFE;
    uint64_t t = le64(fe + 56);
    e->bloco = bloco;
    e->ref = ref;
    e->pasta = fe[16 + 11] == 4;
    e->tam = t > 0xFFFFFFFFull ? 0xFFFFFFFFu : (uint32_t)t;
    memcpy(e->data, fe + (ext ? 92 : 84), 12);
}

enum { MAPA_FIM = 0, MAPA_DADOS = 1, MAPA_ZEROS = 2, MAPA_EMBUTIDO = 3 };

/* Dónde está el byte "pos" del archivo de la File Entry "fe" (de la partición "ref").
 * Devuelve MAPA_DADOS (con el bloque físico de inicio de la extensión, su comienzo
 * en el archivo y su tamaño), MAPA_ZEROS (extensión sin grabar), MAPA_EMBUTIDO (los
 * datos están en la propia entrada) o MAPA_FIM; < 0 si la lista está mal. */
static int mapear(udf_t *v, const uint8_t *fe, uint16_t ref, uint32_t pos,
                  uint32_t *ext_ini, uint32_t *ext_tam, uint32_t *ext_fis)
{
    bool ext = le16(fe) == TAG_EFE;
    uint32_t base = ext ? 216 : 176;
    uint32_t l_ea = le32(fe + (ext ? 208 : 168)), l_ad = le32(fe + (ext ? 212 : 172));
    if (l_ea > BLOCO || l_ad > BLOCO || base + l_ea + l_ad > BLOCO) return ERRO_FORMATO;
    uint16_t tipo_ad = le16(fe + 16 + 18) & 7;
    if (tipo_ad == 3) {   /* embebido: los datos son los l_ad bytes de la entrada */
        *ext_ini = 0;
        *ext_tam = l_ad;
        *ext_fis = base + l_ea;   /* desplazamiento dentro de la entrada */
        return pos < l_ad ? MAPA_EMBUTIDO : MAPA_FIM;
    }
    if (tipo_ad != 0 && tipo_ad != 1) return ERRO_FORMATO;   /* ext_ad: no lo usa UDF */
    uint32_t tam_ad = tipo_ad == 0 ? 8 : 16;
    const uint8_t *ad = fe + base + l_ea;
    uint32_t n = l_ad;
    uint64_t off = 0;
    int continuacoes = 0;
    while (n >= tam_ad) {
        uint32_t len = le32(ad) & 0x3FFFFFFFu, tipo = le32(ad) >> 30;
        uint32_t lbn = le32(ad + 4);
        uint16_t r = tipo_ad == 0 ? ref : le16(ad + 8);
        if (len == 0) break;   /* fin de la lista */
        if (tipo == 3) {       /* la lista sigue en un Allocation Extent Descriptor */
            uint32_t fis;
            if (++continuacoes > MAX_CONTINUACOES || !fisico(v, r, lbn, 1, &fis) || ler_fis(v, fis, 1, v->aed) < 0 ||
                !etiqueta_ok(v->aed, TAG_AED, BLOCO))
                return ERRO_FORMATO;
            n = le32(v->aed + 20);
            if (n > BLOCO - 24) return ERRO_FORMATO;
            ad = v->aed + 24;
            continue;
        }
        if (pos < off + len) {
            *ext_ini = (uint32_t)off;
            *ext_tam = len;
            if (tipo != 0) return MAPA_ZEROS;   /* reservada sin grabar, o sin reservar */
            if (!fisico(v, r, lbn, (len + BLOCO - 1) / BLOCO, ext_fis)) return ERRO_FORMATO;
            return MAPA_DADOS;
        }
        off += len;
        if (off > 0xFFFFFFFFull) return MAPA_FIM;
        ad += tam_ad;
        n -= tam_ad;
    }
    return MAPA_FIM;
}

/* Copia "n" bytes desde "pos" de los datos del archivo. "fe" es su entrada (ya
 * leída, en v->fe o v->fed); "c" guarda la última extensión (puede ser NULL). */
static int ler_dados(udf_t *v, const uint8_t *fe, uint16_t ref, udf_arq_t *c, uint32_t tam, uint32_t pos,
                     uint8_t *dest, uint32_t n)
{
    udf_arq_t local;
    if (!c) { memset(&local, 0, sizeof local); c = &local; }
    if (pos >= tam) return 0;
    if (n > tam - pos) n = tam - pos;
    uint32_t feito = 0;
    while (feito < n) {
        uint32_t p = pos + feito, resto = n - feito;
        if (!c->ext_tipo || p < c->ext_ini || p - c->ext_ini >= c->ext_tam) {
            int r = mapear(v, fe, ref, p, &c->ext_ini, &c->ext_tam, &c->ext_fis);
            if (r <= 0) { c->ext_tipo = 0; break; }
            c->ext_tipo = (uint8_t)r;
        }
        uint32_t dentro = p - c->ext_ini, cabe = c->ext_tam - dentro;
        if (cabe > resto) cabe = resto;
        if (c->ext_tipo == MAPA_ZEROS) { memset(dest + feito, 0, cabe); feito += cabe; continue; }
        if (c->ext_tipo == MAPA_EMBUTIDO) { memcpy(dest + feito, fe + c->ext_fis + dentro, cabe); feito += cabe; continue; }
        uint32_t b = c->ext_fis + dentro / BLOCO, off = dentro % BLOCO;
        if (off == 0 && cabe >= BLOCO) {   /* bloques enteros: directos al destino */
            uint32_t q = cabe / BLOCO;
            if (q > MAX_BLOCOS_LEITURA) q = MAX_BLOCOS_LEITURA;
            if (ler_fis(v, b, q, dest + feito) < 0) break;
            feito += q * BLOCO;
            continue;
        }
        if (v->buf_fis != b) {
            v->buf_fis = 0xFFFFFFFFu;
            if (ler_fis(v, b, 1, v->buf) < 0) break;
            v->buf_fis = b;
        }
        uint32_t parte = BLOCO - off;
        if (parte > cabe) parte = cabe;
        memcpy(dest + feito, v->buf + off, parte);
        feito += parte;
    }
    if (!feito && n) return ERRO_ACESSO;
    return (int)feito;
}

/* ------------------------------------------------------------ directorios */

/* Siguiente File Identifier Descriptor de un directorio (sin el padre ni los
 * borrados). 1 = hay, 0 = fin, < 0 = error. La entrada del directorio está en v->fed. */
static int dir_proxima(udf_t *v, const udf_ent_t *dir, uint32_t *pos, uint32_t *icb_bloco, uint16_t *icb_ref,
                       bool *pasta, char *nome, int max)
{
    uint8_t fid[38];
    while (*pos + 38 <= dir->tam) {
        int r = ler_fe(v, dir->ref, dir->bloco, v->fed, &v->fed_fis);
        if (r < 0) return r;
        if (ler_dados(v, v->fed, dir->ref, NULL, dir->tam, *pos, fid, 38) != 38) return ERRO_ACESSO;
        if (!soma_ok(fid, TAG_FID)) return ERRO_FORMATO;
        uint32_t l_fi = fid[19], l_iu = le16(fid + 36);
        uint32_t total = (38 + l_iu + l_fi + 3) & ~3u;
        if (l_iu > BLOCO || *pos + 38 + l_iu + l_fi > dir->tam) return ERRO_FORMATO;
        uint32_t ini = *pos;
        *pos += total;
        uint8_t car = fid[18];
        if (car & (4 | 8)) continue;   /* borrado, o el padre */
        /* El nombre está después del uso de la implementación. */
        uint8_t bruto[256];
        if (l_fi && ler_dados(v, v->fed, dir->ref, NULL, dir->tam, ini + 38 + l_iu, bruto, l_fi) != (int)l_fi)
            return ERRO_ACESSO;
        if (!cs0_para_latin1(bruto, l_fi, nome, max)) continue;   /* sin nombre: no se lista */
        *icb_bloco = le32(fid + 20 + 4);
        *icb_ref = le16(fid + 20 + 8);
        *pasta = (car & 2) != 0;
        return 1;
    }
    return 0;
}

static bool igual_sem_caixa(const char *a, const char *b, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        char x = a[i], y = b[i];
        if (x >= 'A' && x <= 'Z') x = (char)(x + 32);
        if (y >= 'A' && y <= 'Z') y = (char)(y + 32);
        if (x != y) return false;
    }
    return true;
}

/* Busca la ruta relativa ("" = raíz). */
static int achar(udf_t *v, const char *rel, udf_ent_t *e)
{
    int r = ler_fe(v, v->raiz_ref, v->raiz_bloco, v->fe, &v->fe_fis);
    if (r < 0) return r;
    dados_fe(v->fe, v->raiz_ref, v->raiz_bloco, e);
    if (!e->pasta) return ERRO_FORMATO;
    const char *p = rel;
    char *nome = kmalloc(NOME_UDF);
    if (!nome) return ERRO_SEM_MEMORIA;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *fim = p;
        while (*fim && *fim != '/') fim++;
        size_t n = (size_t)(fim - p);
        if (!e->pasta) { r = ERRO_NAO_E_PASTA; goto sai; }
        if (n >= NOME_UDF) { r = ERRO_NAO_EXISTE; goto sai; }
        udf_ent_t dir = *e;
        uint32_t pos = 0, bl = 0, qbl = 0;
        uint16_t rf = 0, qrf = 0;
        bool pasta, tem_quase = false, achou = false;
        while ((r = dir_proxima(v, &dir, &pos, &bl, &rf, &pasta, nome, NOME_UDF)) == 1) {
            size_t t = txt_tamanho(nome);
            /* El listado corta los nombres a NOME_MAX - 1: el corte también vale. */
            bool cortado = n == NOME_MAX - 1 && t > n;
            if (t != n && !cortado) continue;
            if (memcmp(nome, p, n) == 0) { achou = true; break; }
            if (!tem_quase && igual_sem_caixa(nome, p, n)) { qbl = bl; qrf = rf; tem_quase = true; }
        }
        if (r < 0) goto sai;
        if (!achou && !tem_quase) { r = ERRO_NAO_EXISTE; goto sai; }
        if (!achou) { bl = qbl; rf = qrf; }
        r = ler_fe(v, rf, bl, v->fe, &v->fe_fis);
        if (r < 0) goto sai;
        dados_fe(v->fe, rf, bl, e);
        p = fim;
    }
    r = 0;
sai:
    kfree(nome);
    return r;
}

/* ------------------------------------------------------------ operaciones */

static int udf_info(void *priv, const char *rel, info_arquivo_t *inf)
{
    udf_ent_t e;
    int r = achar(priv, rel, &e);
    if (r < 0) return r;
    inf->tipo = e.pasta ? TIPO_PASTA : TIPO_ARQUIVO;
    inf->tamanho = e.pasta ? 0 : e.tam;
    return 0;
}

/* Timestamp de ECMA-167 (1/7.3): tipo y huso (12 bits con signo, minutos), año, mes... */
static uint32_t data_udf(const uint8_t *d)
{
    int ano = (int16_t)le16(d + 2);
    if (ano < 1970 || ano > 2105 || !d[4] || d[4] > 12 || !d[5] || d[5] > 31) return 0;
    uint32_t t = fuso_compor(ano, d[4], d[5], d[6], d[7], d[8]);
    uint16_t th = le16(d);
    int16_t fuso = (int16_t)(th << 4) >> 4;   /* signo de los 12 bits */
    if ((th >> 12) == 1 && fuso != -2047 && fuso >= -1440 && fuso <= 1440) t = (uint32_t)((int32_t)t - fuso * 60);
    return t;
}

static int udf_datas(void *priv, const char *rel, datas_arquivo_t *d)
{
    udf_ent_t e;
    int r = achar(priv, rel, &e);
    if (r < 0) return r;
    d->modificado = d->criado = d->acessado = data_udf(e.data);
    return 0;
}

static int udf_listar(void *priv, const char *rel, int indice, entrada_pasta_t *saida)
{
    udf_t *v = priv;
    udf_ent_t dir;
    int r = achar(v, rel, &dir);
    if (r < 0) return r;
    if (!dir.pasta) return ERRO_NAO_E_PASTA;
    uint32_t pos = 0, bl;
    uint16_t rf;
    bool pasta;
    int k = 0;
    while ((r = dir_proxima(v, &dir, &pos, &bl, &rf, &pasta, saida->nome, NOME_MAX)) == 1) {
        if (k++ != indice) continue;
        saida->tipo = pasta ? TIPO_PASTA : TIPO_ARQUIVO;
        saida->tamanho = 0;
        if (!pasta) {   /* el tamaño está en su File Entry */
            udf_ent_t e;
            if (ler_fe(v, rf, bl, v->fe, &v->fe_fis) == 0) { dados_fe(v->fe, rf, bl, &e); saida->tamanho = e.tam; }
        }
        return 1;
    }
    return r < 0 ? r : 0;
}

static int udf_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tamanho)
{
    udf_t *v = priv;
    udf_arq_t *f = arq;
    udf_ent_t e;
    int r = achar(v, rel, &e);
    if (r == ERRO_NAO_EXISTE && (modo & ABRIR_CRIAR)) return ERRO_SO_LEITURA;
    if (r < 0) return r;
    if (e.pasta) return ERRO_E_PASTA;
    if (modo & (ABRIR_ESCREVER | ABRIR_TRUNCAR | ABRIR_ACRESCENTAR)) return ERRO_SO_LEITURA;
    memset(f, 0, sizeof *f);
    f->bloco = e.bloco;
    f->ref = e.ref;
    f->tam = e.tam;
    *tamanho = e.tam;
    return 0;
}

static int udf_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{
    udf_t *v = priv;
    udf_arq_t *f = arq;
    if (pos >= f->tam) return 0;
    int r = ler_fe(v, f->ref, f->bloco, v->fe, &v->fe_fis);
    if (r < 0) return r;
    return ler_dados(v, v->fe, f->ref, f, f->tam, pos, buf, n);
}

static int udf_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tam)
{ (void)priv; (void)arq; (void)pos; (void)buf; (void)n; (void)tam; return ERRO_SO_LEITURA; }
static int udf_truncar(void *priv, void *arq, uint32_t tam) { (void)priv; (void)arq; (void)tam; return ERRO_SO_LEITURA; }
static void udf_fechar(void *priv, void *arq) { (void)priv; (void)arq; }
static int udf_so_leitura(void *priv, const char *rel) { (void)priv; (void)rel; return ERRO_SO_LEITURA; }
static int udf_renomear(void *priv, const char *de, const char *para) { (void)priv; (void)de; (void)para; return ERRO_SO_LEITURA; }

static void udf_espaco(void *priv, uint32_t *total_kb, uint32_t *livre_kb)
{
    udf_t *v = priv;
    *total_kb = v->blocos * 2;
    *livre_kb = 0;
}

static void liberar(udf_t *v)
{
    if (v->buf) kfree(v->buf);
    if (v->fe) kfree(v->fe);
    if (v->fed) kfree(v->fed);
    if (v->aed) kfree(v->aed);
    kfree(v);
}

static int udf_desmontar(void *priv, bool gravar)
{
    (void)gravar;
    udf_t *v = priv;
    klog("[UDF] %s desmontado\n", v->d->nome);
    liberar(v);
    return 0;
}

static udf_t *novo_volume(void)
{
    udf_t *v = kcalloc(sizeof *v);
    if (!v) return NULL;
    v->buf = kmalloc(BLOCO);
    v->fe = kmalloc(BLOCO);
    v->fed = kmalloc(BLOCO);
    v->aed = kmalloc(BLOCO);
    if (!v->buf || !v->fe || !v->fed || !v->aed) { liberar(v); return NULL; }
    return v;
}

static int udf_montar(disco_t *d, uint32_t *opcoes, void **priv)
{
    *opcoes |= MONTAR_SO_LEITURA;
    *opcoes &= ~(uint32_t)MONTAR_LEITURA_ESCRITA;
    udf_t *v = novo_volume();
    if (!v) return ERRO_SEM_MEMORIA;
    char porque[64];
    udf_ent_t raiz;
    if (!abrir_volume(v, d, porque, sizeof porque) || achar(v, "", &raiz) < 0) {
        klog("[UDF] %s: no se monta: %s\n", d->nome, porque[0] ? porque : "la raíz no se lee");
        liberar(v);
        return ERRO_FORMATO;
    }
    klog("[UDF] %s: volumen \"%s\", UDF %x.%02x, %u bloques de 2048, %d partición(es)\n", d->nome, v->rotulo,
         v->revisao >> 8, v->revisao & 0xFF, v->blocos, v->qtd_mapas);
    *priv = v;
    return 0;
}

const sistema_arquivos_t fs_udf = {
    .nome = "udf", .tam_arq = sizeof(udf_arq_t),
    .montar = udf_montar, .desmontar = udf_desmontar, .info = udf_info, .datas = udf_datas, .listar = udf_listar,
    .abrir = udf_abrir, .ler = udf_ler, .escrever = udf_escrever, .truncar = udf_truncar, .fechar = udf_fechar,
    .sincronizar = NULL, .criar_pasta = udf_so_leitura, .remover_pasta = udf_so_leitura, .apagar = udf_so_leitura,
    .renomear = udf_renomear, .espaco = udf_espaco,
};

bool udf_reconhecer(disco_t *d, char *rotulo, int max_rotulo, char *formato, int max_formato)
{
    if (d->setores / 4 < 258) return false;
    udf_t *v = novo_volume();
    if (!v) return false;
    char porque[64];
    udf_ent_t raiz;
    bool ok = abrir_volume(v, d, porque, sizeof porque) && achar(v, "", &raiz) == 0;
    if (ok) {
        txt_copia_n(rotulo, v->rotulo, (size_t)max_rotulo);
        if (v->revisao) snformatar(formato, (size_t)max_formato, "UDF %x.%02x", v->revisao >> 8, v->revisao & 0xFF);
        else txt_copia_n(formato, "UDF", (size_t)max_formato);
    } else if (porque[0]) {
        txt_copia_n(formato, porque, (size_t)max_formato);
        klog("[UDF] %s: %s\n", d->nome, porque);
    }
    liberar(v);
    return ok;
}
