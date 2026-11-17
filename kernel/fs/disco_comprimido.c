// SPDX-License-Identifier: GPL-2.0-or-later
/* disco_comprimido.c - la raíz del CD comprimida por bloques, vista como un disco ("rz0"; v8.5.4.2,
 * D379 P5).
 *
 * En el CD, la SAGUI-RAIZ (ext2) no va tal cual sino comprimida en bloques de 64 KB con DEFLATE, en
 * una partición de tipo 0x5A de la imagen El Torito (tools/mkimagem.py, comprimir_raiz). Este driver
 * la presenta como un disco de solo lectura con la ext2 descomprimida: el resto del sistema no sabe
 * nada de la compresión.
 *   - todo a la RAM (lo normal en el CD): disco_ram.c copia este disco entero a ram0, leyendo de 64 KB
 *     en 64 KB (cada bloque se lee del CD y se descomprime una sola vez);
 *   - con poca RAM: la live se monta encima de este disco y cada bloque se descomprime cuando hace
 *     falta (con una pequeña caché de bloques ya descomprimidos).
 *
 * Formato (todo little endian; los desplazamientos, en bytes desde el principio de la partición):
 *   sector 0:  "SGRZ", u32 versión (1), u32 bloque (65536), u32 bloques, u32 sectores de 512 de la
 *              ext2, u32 desplazamiento de la tabla, char[16] etiqueta, u32 CRC-32 de la tabla
 *   tabla:     por bloque, u32 desplazamiento, u32 bytes comprimidos (0 = bloque de ceros; con el
 *              bit 31, guardado sin comprimir) y u32 CRC-32 de los datos descomprimidos.
 * Un bloque que no se lee bien del CD, no se descomprime o no da su CRC es un error de E/S para quien
 * lee (nunca datos cambiados). */
#include "disco_comprimido.h"
#include "abi.h"
#include "console.h"
#include "inflar.h"
#include "memoria.h"
#include "texto.h"
#include "trava.h"

#define RZ_MAGIA       0x5A524753u   /* "SGRZ" */
#define RZ_CRU         0x80000000u
#define RZ_CACHE       4

typedef struct {
    disco_t *origem;              /* la partición del CD */
    uint32_t bloco, n_blocos, setores;
    uint8_t *tabela;              /* n_blocos * 12 bytes */
    uint8_t *comp;                /* un bloque comprimido + 1 KB (lectura por sectores) */
    inflar_ctx_t ctx;
    struct { int32_t num; uint32_t uso; uint8_t *dados; } cache[RZ_CACHE];
    uint32_t relogio;
    trava_t trava;
    bool usado;
} rz_t;

static rz_t rz;

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }

/* El bloque "b" descomprimido (en la caché). NULL si no se puede leer o está mal. */
static uint8_t *bloco_ler(uint32_t b)
{
    int livre = 0;
    for (int i = 0; i < RZ_CACHE; i++) {
        if (rz.cache[i].num == (int32_t)b) { rz.cache[i].uso = ++rz.relogio; return rz.cache[i].dados; }
        if (rz.cache[i].uso < rz.cache[livre].uso) livre = i;
    }
    uint8_t *saida = rz.cache[livre].dados;
    rz.cache[livre].num = -1;
    const uint8_t *e = rz.tabela + b * 12u;
    uint32_t desl = le32(e), tam = le32(e + 4), crc = le32(e + 8);
    uint32_t cru = tam & RZ_CRU;
    tam &= ~RZ_CRU;
    uint32_t esperado = b + 1 < rz.n_blocos ? rz.bloco : rz.setores * 512u - b * rz.bloco;
    if (tam == 0) {
        memset(saida, 0, esperado);
    } else {
        if (tam > rz.bloco + 1024u) return NULL;
        uint32_t lba = desl / 512u, fim = (desl + tam + 511u) / 512u;
        if (disco_ler(rz.origem, lba, fim - lba, rz.comp) < 0) {
            klog("[RZ] error al leer el bloque %u (sector %u)\n", b, lba);
            return NULL;
        }
        const uint8_t *dados = rz.comp + desl % 512u;
        if (cru) {
            if (tam != esperado) return NULL;
            memcpy(saida, dados, tam);
        } else {
            int n = inflar_bloco(&rz.ctx, dados, (int)tam, saida, (int)rz.bloco);
            if (n != (int)esperado) {
                klog("[RZ] el bloque %u no se descomprime (%d)\n", b, n);
                return NULL;
            }
        }
    }
    if (inflar_crc32(0, saida, esperado) != crc) {
        klog("[RZ] el bloque %u no da su CRC: se toma como error de lectura\n", b);
        return NULL;
    }
    rz.cache[livre].num = (int32_t)b;
    rz.cache[livre].uso = ++rz.relogio;
    return saida;
}

static int rz_ler(disco_t *d, uint32_t lba, uint32_t n, void *buf)
{
    (void)d;
    uint8_t *o = buf;
    if (lba >= rz.setores || n > rz.setores - lba) return ERRO_INVALIDO;
    trava_pegar(&rz.trava);
    int r = 0;
    while (n) {
        uint64_t byte = (uint64_t)lba * 512u;
        uint32_t b = (uint32_t)(byte / rz.bloco), dentro = (uint32_t)(byte % rz.bloco);
        uint32_t k = (rz.bloco - dentro) / 512u;
        if (k > n) k = n;
        uint8_t *x = bloco_ler(b);
        if (!x) { r = -1; break; }
        memcpy(o, x + dentro, k * 512u);
        o += k * 512u;
        lba += k;
        n -= k;
    }
    trava_soltar(&rz.trava);
    return r;
}

static void soltar(void)
{
    if (rz.tabela) kfree(rz.tabela);
    if (rz.comp) kfree(rz.comp);
    for (int i = 0; i < RZ_CACHE; i++) if (rz.cache[i].dados) kfree(rz.cache[i].dados);
    memset(&rz, 0, sizeof rz);
}

/* ¿Es "p" una raíz comprimida con esa etiqueta? Si lo es, la registra (una sola vez) y devuelve el disco. */
static disco_t *abrir(disco_t *p, const char *rotulo)
{
    inflar_crc32(0, NULL, 0);   /* la tabla del CRC, ya aquí (arranque, un solo hilo), no en medio de una lectura */
    uint8_t *s = kmalloc(512);
    if (!s) return NULL;
    disco_t *d = NULL;
    if (disco_ler(p, 0, 1, s) < 0 || le32(s) != RZ_MAGIA || le32(s + 4) != 1) goto fim;
    char r[17];
    memcpy(r, s + 24, 16);
    r[16] = 0;
    if (txt_compara(r, rotulo) != 0) goto fim;
    uint32_t bloco = le32(s + 8), n = le32(s + 12), setores = le32(s + 16), desl_tab = le32(s + 20), crc_tab = le32(s + 40);
    if (bloco != 65536u || !n || !setores || (uint64_t)setores * 512u > (uint64_t)n * bloco ||
        (uint64_t)setores * 512u <= (uint64_t)(n - 1) * bloco || n > 1u << 20 || desl_tab % 512u) {
        klog("[RZ] %s: cabecera rara (bloque %u, %u bloques, %u sectores)\n", p->nome, bloco, n, setores);
        goto fim;
    }
    memset(&rz, 0, sizeof rz);
    rz.origem = p;
    rz.bloco = bloco;
    rz.n_blocos = n;
    rz.setores = setores;
    uint32_t tam_tab = n * 12u, set_tab = (tam_tab + 511u) / 512u;
    rz.tabela = kmalloc(set_tab * 512u);
    rz.comp = kmalloc(bloco + 2048u);
    bool ok = rz.tabela && rz.comp;
    for (int i = 0; ok && i < RZ_CACHE; i++) {
        rz.cache[i].dados = kmalloc(bloco);
        rz.cache[i].num = -1;
        ok = rz.cache[i].dados != NULL;
    }
    if (!ok) { klog("[RZ] %s: sin memoria para leerla\n", p->nome); soltar(); goto fim; }
    if (disco_ler(p, desl_tab / 512u, set_tab, rz.tabela) < 0 || inflar_crc32(0, rz.tabela, tam_tab) != crc_tab) {
        klog("[RZ] %s: la tabla de bloques no se lee bien\n", p->nome);
        soltar();
        goto fim;
    }
    rz.usado = true;
    d = disco_registrar("rz", "SaguiOS", "Raiz comprimida del CD", setores, 512, &rz, false, rz_ler, NULL);
    if (!d) { soltar(); goto fim; }   /* "rz0", sin la caché de bloques (discos.c): ya hay la suya */
    klog("[RZ] %s: raiz comprimida %s: %u MB en %u bloques de 64 KB (%u MB en el medio) -> %s\n", p->nome, rotulo,
         setores / 2048, n, p->setores / 2048, d->nome);
fim:
    kfree(s);
    return d;
}

disco_t *disco_comprimido_achar(const char *rotulo)
{
    static disco_t *aberto;
    /* v8.5.4.2 (revisión): raiz.c llama a esto en cada vuelta de la espera (hasta 60 s); una partición que ya
     * se probó y no valía no se vuelve a leer (ni a reservar su tabla ni a anotar el mismo error). */
    static disco_t *probadas[8];
    static int n_probadas;
    if (rz.usado) return aberto;
    for (int i = 0; i < MAX_DISCOS; i++) {
        disco_t *p = disco_indice(i);
        if (!p || !p->usado || !p->presente || !p->pai || p->tipo_mbr != DISCO_TIPO_RAIZ_COMPRIMIDA) continue;
        if (!disco_pode_ser_arranque(p->pai)) continue;
        bool ja = false;
        for (int k = 0; k < n_probadas; k++) if (probadas[k] == p) ja = true;
        if (ja) continue;
        aberto = abrir(p, rotulo);
        if (aberto) return aberto;
        if (n_probadas < (int)(sizeof probadas / sizeof probadas[0])) probadas[n_probadas++] = p;
    }
    return NULL;
}
