// SPDX-License-Identifier: GPL-2.0-or-later
/* iso9660.c - lectura de los CD/DVD con ISO 9660 (D207).
 *
 * Solo lectura (un CD no se graba). El volumen se lee en bloques de 2048 bytes
 * por la capa de discos, que ve el lector en sectores de 512 (atapi.c): el
 * bloque B son los sectores 4B..4B+3.
 *
 * Lo que se lee:
 *   - descriptores de volumen desde el bloque 16 hasta el terminador (255):
 *     el primario (1), el suplementario Joliet (2, con las secuencias de escape
 *     %/@, %/C o %/E) y el de arranque El Torito (0);
 *   - directorios: registros de longitud variable que nunca cruzan un bloque
 *     (un byte 0 = el resto del bloque está vacío); "." y ".." no se listan;
 *   - nombres, por este orden de preferencia:
 *       Rock Ridge (entradas NM del área de uso del sistema, SUSP; el "SP" del
 *         "." de la raíz dice que lo hay), en UTF-8, pasado a Latin-1;
 *       Joliet (UCS-2 big-endian, pasado a Latin-1);
 *       el nombre 8.3 del árbol primario, sin la versión ";1" ni el punto final
 *         y en minúsculas (como hace Linux con map=normal).
 *     Las búsquedas no distinguen mayúsculas si no hay coincidencia exacta.
 * Lo que no se lee: archivos de más de una extensión (más de 4 GB), los
 * archivos entrelazados, las continuaciones CE de Rock Ridge (nombres de más
 * de ~200 caracteres) y la relocalización de directorios profundos (RE/CL).
 * Son raros y el archivo sale con su nombre 8.3 o con el contenido parcial.
 */
#include "iso9660.h"
#include "abi.h"
#include "console.h"
#include "fuso.h"
#include "memoria.h"
#include "montagem.h"
#include "texto.h"

#define BLOCO 2048u
#define DESC_INICIO 16u
#define DESC_MAX    64u          /* como mucho, 64 descriptores antes del terminador */
#define MAX_BLOCOS_LEITURA 16u   /* bloques por lectura directa (32 KB, el búfer del AHCI) */

enum { NOMES_8_3, NOMES_JOLIET, NOMES_RR };

typedef struct {
    disco_t *d;
    uint32_t blocos;             /* tamaño del volumen */
    uint32_t raiz_lba, raiz_tam;
    int      nomes;              /* NOMES_* */
    uint8_t  rr_salto;           /* bytes que saltar en el área de uso del sistema (SP) */
    uint8_t *buf;                /* un bloque de directorio */
    uint32_t buf_lba;            /* el bloque que hay en buf (0xFFFFFFFF = ninguno) */
} iso_t;

typedef struct {
    uint32_t lba, tam;
    bool     pasta;
    uint8_t  data[7];
} iso_ent_t;

typedef struct {
    uint32_t lba, tam;
} iso_arq_t;

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | p[1] << 8); }

static int ler_blocos(disco_t *d, uint32_t lba, uint32_t n, void *buf)
{
    if (lba > 0x3FFFFFFFu || n > 0x3FFFFFFFu) return ERRO_ACESSO;
    return disco_ler(d, lba * 4, n * 4, buf);
}

static bool eh_cd001(const uint8_t *b) { return memcmp(b + 1, "CD001", 5) == 0 && b[6] == 1; }

static bool eh_joliet(const uint8_t *b)
{
    return b[0] == 2 && b[88] == '%' && b[89] == '/' && (b[90] == '@' || b[90] == 'C' || b[90] == 'E');
}

/* Recorre los descriptores desde el bloque base + 16. Devuelve en "pvd" el primario y,
 * si hay, en "svd" el Joliet. */
static bool ler_descritores_em(disco_t *d, uint32_t base, uint8_t *pvd, uint8_t *svd, bool *tem_joliet, uint32_t *catalogo)
{
    uint8_t *b = kmalloc(BLOCO);
    if (!b) return false;
    bool tem_pvd = false;
    if (tem_joliet) *tem_joliet = false;
    if (catalogo) *catalogo = 0;
    for (uint32_t i = 0; i < DESC_MAX; i++) {
        if (ler_blocos(d, base + DESC_INICIO + i, 1, b) < 0 || !eh_cd001(b)) break;
        if (b[0] == 255) break;                                   /* terminador */
        if (b[0] == 1 && !tem_pvd) { if (pvd) memcpy(pvd, b, BLOCO); tem_pvd = true; }
        else if (eh_joliet(b) && tem_joliet && !*tem_joliet) { if (svd) memcpy(svd, b, BLOCO); *tem_joliet = true; }
        else if (b[0] == 0 && catalogo && memcmp(b + 7, "EL TORITO SPECIFICATION", 23) == 0) *catalogo = le32(b + 71);
    }
    kfree(b);
    return tem_pvd;
}

/* D233: un CD multisesión lleva los descriptores al día en su última sesión (los
 * registros de directorio usan bloques absolutos, así que solo cambia dónde se
 * empiezan a buscar). Si ahí no hay nada, los de la primera. */
static bool ler_descritores(disco_t *d, uint8_t *pvd, uint8_t *svd, bool *tem_joliet, uint32_t *catalogo)
{
    if (d->sessao && ler_descritores_em(d, d->sessao, pvd, svd, tem_joliet, catalogo)) return true;
    return ler_descritores_em(d, 0, pvd, svd, tem_joliet, catalogo);
}

static void copiar_rotulo(const uint8_t *pvd, char *s, int max)
{
    int n = 32;
    while (n > 0 && (pvd[40 + n - 1] == ' ' || pvd[40 + n - 1] == 0)) n--;
    if (n > max - 1) n = max - 1;
    for (int i = 0; i < n; i++) s[i] = (char)pvd[40 + i];
    s[n] = 0;
}

/* ------------------------------------------------------------ Rock Ridge */

/* Área de uso del sistema de un registro de directorio (SUSP). */
static const uint8_t *area_sistema(const uint8_t *r, uint32_t *tam)
{
    uint32_t nl = r[32];
    uint32_t ini = 33 + nl + ((nl & 1) ? 0 : 1);   /* byte de relleno si el nombre es par */
    if (ini >= r[0]) { *tam = 0; return r; }
    *tam = r[0] - ini;
    return r + ini;
}

/* El "SP" del "." de la raíz: hay SUSP (y, en la práctica, Rock Ridge). */
static bool tem_susp(const uint8_t *raiz_ponto, uint8_t *salto)
{
    uint32_t t;
    const uint8_t *s = area_sistema(raiz_ponto, &t);
    if (t >= 7 && s[0] == 'S' && s[1] == 'P' && s[4] == 0xBE && s[5] == 0xEF) { *salto = s[6]; return true; }
    return false;
}

/* UTF-8 -> Latin-1 (lo que no cabe, "?"). */
static int utf8_para_latin1(const uint8_t *u, int n, char *s, int k, int max)
{
    for (int i = 0; i < n && k < max - 1; ) {
        uint8_t c = u[i];
        if (c < 0x80) { s[k++] = (char)c; i++; }
        else if ((c & 0xE0) == 0xC0 && i + 1 < n) {
            uint32_t cp = (uint32_t)(c & 0x1F) << 6 | (u[i + 1] & 0x3F);
            s[k++] = cp < 0x100 ? (char)cp : '?';
            i += 2;
        } else {
            int largo = (c & 0xF0) == 0xE0 ? 3 : (c & 0xF8) == 0xF0 ? 4 : 1;
            s[k++] = '?';
            i += largo;
        }
    }
    s[k] = 0;
    return k;
}

/* Nombre Rock Ridge (NM) del registro; false si no lo tiene. */
static bool nome_rr(iso_t *v, const uint8_t *r, char *s, int max)
{
    uint32_t t;
    const uint8_t *a = area_sistema(r, &t);
    if (t <= v->rr_salto) return false;
    a += v->rr_salto; t -= v->rr_salto;
    int k = 0;
    bool achou = false;
    for (uint32_t i = 0; i + 4 <= t; ) {
        uint8_t len = a[i + 2];
        if (len < 4 || i + len > t) break;
        if (a[i] == 'S' && a[i + 1] == 'T') break;                 /* fin de las entradas */
        if (a[i] == 'N' && a[i + 1] == 'M' && len >= 5 && !(a[i + 4] & 6)) {   /* 2 = ".", 4 = ".." */
            k = utf8_para_latin1(a + i + 5, len - 5, s, k, max);
            achou = true;
        }
        i += len;
    }
    return achou && k > 0;
}

/* ------------------------------------------------------------ directorios */

static void nome_do_registro(iso_t *v, const uint8_t *r, char *s, int max)
{
    if (v->nomes == NOMES_RR && nome_rr(v, r, s, max)) return;
    const uint8_t *n = r + 33;
    int nl = r[32], k = 0;
    if (v->nomes == NOMES_JOLIET) {
        for (int i = 0; i + 1 < nl && k < max - 1; i += 2) {
            uint16_t c = (uint16_t)(n[i] << 8 | n[i + 1]);
            if (c == ';') break;
            s[k++] = c < 0x100 ? (char)c : '?';
        }
    } else {
        for (int i = 0; i < nl && k < max - 1; i++) {
            char c = (char)n[i];
            if (c == ';') break;
            s[k++] = (c >= 'A' && c <= 'Z') ? (char)(c + 32) : c;
        }
        if (k > 0 && s[k - 1] == '.') k--;   /* "LEEME." -> "leeme" */
    }
    s[k] = 0;
}

static int bloco_dir(iso_t *v, uint32_t lba)
{
    if (v->buf_lba == lba) return 0;
    v->buf_lba = 0xFFFFFFFFu;
    if (ler_blocos(v->d, lba, 1, v->buf) < 0) return ERRO_ACESSO;
    v->buf_lba = lba;
    return 0;
}

/* Siguiente entrada de un directorio (sin "." ni ".."). 1 = hay, 0 = fin, < 0 = error. */
static int dir_proxima(iso_t *v, uint32_t lba, uint32_t tam, uint32_t *pos, iso_ent_t *e, char *nome, int max)
{
    while (*pos < tam) {
        uint32_t b = lba + *pos / BLOCO, off = *pos % BLOCO;
        int r = bloco_dir(v, b);
        if (r < 0) return r;
        const uint8_t *reg = v->buf + off;
        if (reg[0] == 0 || off + 34 > BLOCO) { *pos = (*pos / BLOCO + 1) * BLOCO; continue; }   /* resto del bloque vacío */
        if (reg[0] < 34 || off + reg[0] > BLOCO || 33u + reg[32] > reg[0]) return ERRO_FORMATO;
        *pos += reg[0];
        if (reg[32] == 1 && (reg[33] == 0 || reg[33] == 1)) continue;   /* "." y ".." */
        e->lba = le32(reg + 2);
        e->tam = le32(reg + 10);
        e->pasta = (reg[25] & 2) != 0;
        memcpy(e->data, reg + 18, 7);
        nome_do_registro(v, reg, nome, max);
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
static int achar(iso_t *v, const char *rel, iso_ent_t *e)
{
    e->lba = v->raiz_lba; e->tam = v->raiz_tam; e->pasta = true;
    memset(e->data, 0, sizeof e->data);
    const char *p = rel;
    while (*p) {
        while (*p == '/') p++;
        if (!*p) break;
        const char *fim = p;
        while (*fim && *fim != '/') fim++;
        size_t n = (size_t)(fim - p);
        if (!e->pasta) return ERRO_NAO_E_PASTA;
        if (n >= NOME_MAX) return ERRO_NAO_EXISTE;
        uint32_t pos = 0;
        iso_ent_t x, quase = { 0, 0, false, { 0 } };
        bool tem_quase = false, achou = false;
        char nome[NOME_MAX];
        int r;
        while ((r = dir_proxima(v, e->lba, e->tam, &pos, &x, nome, sizeof nome)) == 1) {
            if (txt_tamanho(nome) != n) continue;
            if (memcmp(nome, p, n) == 0) { achou = true; break; }
            if (!tem_quase && igual_sem_caixa(nome, p, n)) { quase = x; tem_quase = true; }
        }
        if (r < 0) return r;
        if (!achou && !tem_quase) return ERRO_NAO_EXISTE;
        *e = achou ? x : quase;
        p = fim;
    }
    return 0;
}

/* ------------------------------------------------------------ operaciones */

static int iso_info(void *priv, const char *rel, info_arquivo_t *inf)
{
    iso_ent_t e;
    int r = achar(priv, rel, &e);
    if (r < 0) return r;
    inf->tipo = e.pasta ? TIPO_PASTA : TIPO_ARQUIVO;
    inf->tamanho = e.pasta ? 0 : e.tam;
    return 0;
}

static uint32_t data_iso(const uint8_t *d)
{
    if (!d[1] || d[1] > 12 || !d[2] || d[2] > 31) return 0;
    uint32_t t = fuso_compor(1900 + d[0], d[1], d[2], d[3], d[4], d[5]);
    int32_t desvio = (int8_t)d[6] * 15 * 60;   /* en cuartos de hora respecto a UTC */
    return (uint32_t)((int32_t)t - desvio);
}

static int iso_datas(void *priv, const char *rel, datas_arquivo_t *d)
{
    iso_ent_t e;
    int r = achar(priv, rel, &e);
    if (r < 0) return r;
    d->modificado = d->criado = d->acessado = data_iso(e.data);
    return 0;
}

static int iso_listar(void *priv, const char *rel, int indice, entrada_pasta_t *saida)
{
    iso_t *v = priv;
    iso_ent_t dir, e;
    int r = achar(v, rel, &dir);
    if (r < 0) return r;
    if (!dir.pasta) return ERRO_NAO_E_PASTA;
    uint32_t pos = 0;
    int k = 0;
    while ((r = dir_proxima(v, dir.lba, dir.tam, &pos, &e, saida->nome, NOME_MAX)) == 1) {
        if (k++ != indice) continue;
        saida->tipo = e.pasta ? TIPO_PASTA : TIPO_ARQUIVO;
        saida->tamanho = e.pasta ? 0 : e.tam;
        return 1;
    }
    return r < 0 ? r : 0;
}

static int iso_abrir(void *priv, const char *rel, int modo, void *arq, uint32_t *tamanho)
{
    iso_t *v = priv;
    iso_arq_t *f = arq;
    iso_ent_t e;
    int r = achar(v, rel, &e);
    if (r == ERRO_NAO_EXISTE && (modo & ABRIR_CRIAR)) return ERRO_SO_LEITURA;
    if (r < 0) return r;
    if (e.pasta) return ERRO_E_PASTA;
    if (modo & (ABRIR_ESCREVER | ABRIR_TRUNCAR | ABRIR_ACRESCENTAR)) return ERRO_SO_LEITURA;
    if (e.tam && (e.lba >= v->blocos || (e.tam + BLOCO - 1) / BLOCO > v->blocos - e.lba)) {
        klog("[ISO9660] %s: \"%s\" sale del volumen (bloque %u)\n", v->d->nome, rel, e.lba);
        return ERRO_FORMATO;
    }
    f->lba = e.lba;
    f->tam = e.tam;
    *tamanho = e.tam;
    return 0;
}

static int iso_ler(void *priv, void *arq, uint32_t pos, void *buf, uint32_t n)
{
    iso_t *v = priv;
    iso_arq_t *f = arq;
    if (pos >= f->tam) return 0;
    if (n > f->tam - pos) n = f->tam - pos;
    uint8_t *dest = buf;
    uint32_t feito = 0;
    while (feito < n) {
        uint32_t p = pos + feito, b = f->lba + p / BLOCO, off = p % BLOCO;
        uint32_t resto = n - feito;
        if (off == 0 && resto >= BLOCO) {   /* bloques enteros: directos al destino */
            uint32_t q = resto / BLOCO;
            if (q > MAX_BLOCOS_LEITURA) q = MAX_BLOCOS_LEITURA;
            if (ler_blocos(v->d, b, q, dest + feito) < 0) break;
            feito += q * BLOCO;
            continue;
        }
        if (bloco_dir(v, b) < 0) break;
        uint32_t parte = BLOCO - off;
        if (parte > resto) parte = resto;
        memcpy(dest + feito, v->buf + off, parte);
        feito += parte;
    }
    if (!feito && n) return ERRO_ACESSO;
    return (int)feito;
}

static int iso_escrever(void *priv, void *arq, uint32_t pos, const void *buf, uint32_t n, uint32_t *tam)
{ (void)priv; (void)arq; (void)pos; (void)buf; (void)n; (void)tam; return ERRO_SO_LEITURA; }
static int iso_truncar(void *priv, void *arq, uint32_t tam) { (void)priv; (void)arq; (void)tam; return ERRO_SO_LEITURA; }
static void iso_fechar(void *priv, void *arq) { (void)priv; (void)arq; }
static int iso_so_leitura(void *priv, const char *rel) { (void)priv; (void)rel; return ERRO_SO_LEITURA; }
static int iso_renomear(void *priv, const char *de, const char *para) { (void)priv; (void)de; (void)para; return ERRO_SO_LEITURA; }

static void iso_espaco(void *priv, uint32_t *total_kb, uint32_t *livre_kb)
{
    iso_t *v = priv;
    *total_kb = v->blocos * 2;
    *livre_kb = 0;
}

static int iso_desmontar(void *priv, bool gravar)
{
    (void)gravar;
    iso_t *v = priv;
    klog("[ISO9660] %s desmontado\n", v->d->nome);
    kfree(v->buf);
    kfree(v);
    return 0;
}

static int iso_montar(disco_t *d, uint32_t *opcoes, void **priv)
{
    *opcoes |= MONTAR_SO_LEITURA;
    *opcoes &= ~(uint32_t)MONTAR_LEITURA_ESCRITA;
    uint8_t *pvd = kmalloc(BLOCO), *svd = kmalloc(BLOCO);
    iso_t *v = kcalloc(sizeof *v);
    uint8_t *buf = kmalloc(BLOCO);
    if (!pvd || !svd || !v || !buf) {
        if (pvd) kfree(pvd);
        if (svd) kfree(svd);
        if (v) kfree(v);
        if (buf) kfree(buf);
        return ERRO_SEM_MEMORIA;
    }
    bool joliet = false;
    int r = 0;
    if (!ler_descritores(d, pvd, svd, &joliet, NULL)) { r = ERRO_FORMATO; goto fim; }
    if (le16(pvd + 128) != BLOCO) {
        klog("[ISO9660] %s: bloques de %u bytes no soportados\n", d->nome, le16(pvd + 128));
        r = ERRO_FORMATO;
        goto fim;
    }
    v->d = d;
    v->buf = buf;
    v->buf_lba = 0xFFFFFFFFu;
    v->blocos = le32(pvd + 80);
    if (v->blocos > d->setores / 4) v->blocos = d->setores / 4;   /* un CD grabado a medias */
    v->raiz_lba = le32(pvd + 156 + 2);
    v->raiz_tam = le32(pvd + 156 + 10);
    v->nomes = NOMES_8_3;
    /* ¿Rock Ridge? Mira el "." de la raíz primaria. */
    if (v->raiz_lba < v->blocos && bloco_dir(v, v->raiz_lba) == 0 && v->buf[0] >= 34 && tem_susp(v->buf, &v->rr_salto)) {
        v->nomes = NOMES_RR;
    } else if (joliet) {
        v->nomes = NOMES_JOLIET;
        v->raiz_lba = le32(svd + 156 + 2);
        v->raiz_tam = le32(svd + 156 + 10);
    }
    if (v->raiz_lba >= v->blocos || !v->raiz_tam) {
        klog("[ISO9660] %s: la raíz está fuera del volumen\n", d->nome);
        r = ERRO_FORMATO;
        goto fim;
    }
    char rot[33];
    copiar_rotulo(pvd, rot, sizeof rot);
    klog("[ISO9660] %s: volumen \"%s\", %u bloques de 2048, nombres %s\n", d->nome, rot, v->blocos,
         v->nomes == NOMES_RR ? "Rock Ridge" : v->nomes == NOMES_JOLIET ? "Joliet" : "8.3");
    *priv = v;
fim:
    kfree(pvd);
    kfree(svd);
    if (r < 0) { kfree(v); kfree(buf); }
    return r;
}

const sistema_arquivos_t fs_iso9660 = {
    .nome = "iso9660", .tam_arq = sizeof(iso_arq_t),
    .montar = iso_montar, .desmontar = iso_desmontar, .info = iso_info, .datas = iso_datas, .listar = iso_listar,
    .abrir = iso_abrir, .ler = iso_ler, .escrever = iso_escrever, .truncar = iso_truncar, .fechar = iso_fechar,
    .sincronizar = NULL, .criar_pasta = iso_so_leitura, .remover_pasta = iso_so_leitura, .apagar = iso_so_leitura,
    .renomear = iso_renomear, .espaco = iso_espaco,
};

/* ------------------------------------------------------------ reconocer y El Torito */

bool iso_reconhecer(disco_t *d, char *rotulo, int max_rotulo, char *formato, int max_formato)
{
    if (d->setores < (DESC_INICIO + 2) * 4) return false;
    uint8_t *pvd = kmalloc(BLOCO);
    if (!pvd) return false;
    bool joliet = false;
    bool ok = ler_descritores(d, pvd, NULL, &joliet, NULL) && le16(pvd + 128) == BLOCO;
    if (ok) {
        copiar_rotulo(pvd, rotulo, max_rotulo);
        /* ¿Rock Ridge? El "." de la raíz primaria. */
        bool rr = false;
        uint8_t salto;
        uint32_t raiz = le32(pvd + 156 + 2);
        if (raiz < d->setores / 4 && ler_blocos(d, raiz, 1, pvd) == 0 && pvd[0] >= 34) rr = tem_susp(pvd, &salto);
        snformatar(formato, (size_t)max_formato, "ISO 9660%s", rr ? " + Rock Ridge" : joliet ? " + Joliet" : "");
    }
    kfree(pvd);
    return ok;
}

/* El catálogo de arranque: entrada de validación (cabecera 1, 55 AA, suma 0),
 * la entrada por defecto y, detrás, secciones (cabecera 0x90/0x91) con más
 * entradas. Vale la primera arrancable (0x88) con emulación de disco duro (4). */
bool iso_el_torito(disco_t *d, uint32_t *inicio512)
{
    uint32_t cat = 0;
    /* El catálogo de arranque es el de la primera sesión (la que lee la BIOS). */
    if (d->setores < (DESC_INICIO + 2) * 4 || !ler_descritores_em(d, 0, NULL, NULL, NULL, &cat) || !cat || cat >= d->setores / 4)
        return false;
    uint8_t *c = kmalloc(BLOCO);
    if (!c) return false;
    bool ok = false;
    if (ler_blocos(d, cat, 1, c) == 0 && c[0] == 1 && c[30] == 0x55 && c[31] == 0xAA) {
        uint16_t soma = 0;
        for (int i = 0; i < 32; i += 2) soma = (uint16_t)(soma + le16(c + i));
        if (soma != 0) klog("[ISO9660] %s: catálogo de El Torito con la suma mal (%x), se sigue\n", d->nome, soma);
        for (uint32_t off = 32; off + 32 <= BLOCO && !ok; off += 32) {
            const uint8_t *e = c + off;
            if (e[0] == 0x90 || e[0] == 0x91) continue;            /* cabecera de sección */
            if (e[0] == 0x88 && (e[1] & 0x0F) == 4) {
                uint32_t lba = le32(e + 8);
                if (lba && lba < d->setores / 4) { *inicio512 = lba * 4; ok = true; }
            }
        }
    }
    kfree(c);
    return ok;
}
