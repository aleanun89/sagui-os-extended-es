/* hashlib_sagui.c - el módulo hashlib: md5, sha1, sha256, sha384 y sha512.
 *
 * El extmod/modhashlib.c de MicroPython solo tiene SHA-256 sin una biblioteca
 * de terceros (su SHA-1 y MD5 vienen de axTLS o de mbedTLS). Aquí:
 * SHA-256 es el mismo del kernel y de libtls (kernel/lib/sha256.c), SHA-384 y
 * SHA-512 vienen de libtls por la ponte (ponte_sagui.c), y MD5 (RFC 1321) y
 * SHA-1 (FIPS 180-4) están escritos abajo. Los dos últimos están aquí solo
 * por compatibilidad (sumas de archivos descargados, protocolos antiguos):
 * no sirven para seguridad y libtls no los acepta (D060).
 *
 * La interfaz es la de CPython: hashlib.sha256(b"...") o hashlib.new("sha256"),
 * update(), digest() y hexdigest() (que pueden llamarse más de una vez),
 * copy(), name, digest_size y block_size. */
#include <string.h>
#include "py/runtime.h"
#include "py/objstr.h"
#include "sagui_mp.h"
#include "sha256.h"
#include "ponte_sagui.h"

/* ------------------------------------------------------------ MD5 y SHA-1 */

typedef struct {
    uint32_t h[5];
    uint8_t  bloco[64];
    uint32_t usados;
    uint32_t total_lo, total_hi;   /* bytes procesados (64 bits sin división de 64 bits) */
} md_t;

static uint32_t rotl(uint32_t x, int n) { return (x << n) | (x >> (32 - n)); }

static const uint32_t md5_k[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
};
static const uint8_t md5_r[16] = { 7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21 };

static void md5_bloco(md_t *c, const uint8_t *p)
{
    uint32_t m[16];
    for (int i = 0; i < 16; i++) m[i] = p[4 * i] | p[4 * i + 1] << 8 | p[4 * i + 2] << 16 | (uint32_t)p[4 * i + 3] << 24;
    uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3];
    for (int i = 0; i < 64; i++) {
        uint32_t f;
        int g;
        switch (i >> 4) {
        case 0: f = (b & cc) | (~b & d); g = i; break;
        case 1: f = (d & b) | (~d & cc); g = (5 * i + 1) & 15; break;
        case 2: f = b ^ cc ^ d; g = (3 * i + 5) & 15; break;
        default: f = cc ^ (b | ~d); g = (7 * i) & 15; break;
        }
        uint32_t t = d;
        d = cc;
        cc = b;
        b = b + rotl(a + f + md5_k[i] + m[g], md5_r[(i >> 4) * 4 + (i & 3)]);
        a = t;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d;
}

static void sha1_bloco(md_t *c, const uint8_t *p)
{
    uint32_t w[80];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | p[4 * i + 1] << 16 | p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 80; i++) w[i] = rotl(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = c->h[0], b = c->h[1], cc = c->h[2], d = c->h[3], e = c->h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (b & cc) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ cc ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & cc) | (b & d) | (cc & d); k = 0x8F1BBCDC; }
        else { f = b ^ cc ^ d; k = 0xCA62C1D6; }
        uint32_t t = rotl(a, 5) + f + e + k + w[i];
        e = d; d = cc; cc = rotl(b, 30); b = a; a = t;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e;
}

static void md_iniciar(md_t *c, bool sha1)
{
    static const uint32_t ini[5] = { 0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0 };
    memcpy(c->h, ini, sizeof ini);
    c->usados = c->total_lo = c->total_hi = 0;
    (void)sha1;
}

static void md_somar(md_t *c, bool sha1, const uint8_t *d, size_t n)
{
    uint32_t antes = c->total_lo;
    c->total_lo += n;
    if (c->total_lo < antes) c->total_hi++;
    while (n) {
        size_t k = 64 - c->usados;
        if (k > n) k = n;
        memcpy(c->bloco + c->usados, d, k);
        c->usados += k;
        d += k;
        n -= k;
        if (c->usados == 64) {
            if (sha1) sha1_bloco(c, c->bloco); else md5_bloco(c, c->bloco);
            c->usados = 0;
        }
    }
}

static void md_fim(md_t *c, bool sha1, uint8_t *saida)
{
    uint32_t bits_lo = c->total_lo << 3, bits_hi = c->total_hi << 3 | c->total_lo >> 29;
    uint8_t fim[72];
    size_t pad = (c->usados < 56 ? 56 : 120) - c->usados;
    memset(fim, 0, sizeof fim);
    fim[0] = 0x80;
    for (int i = 0; i < 4; i++) {
        if (sha1) { fim[pad + i] = bits_hi >> (24 - 8 * i); fim[pad + 4 + i] = bits_lo >> (24 - 8 * i); }
        else { fim[pad + i] = bits_lo >> (8 * i); fim[pad + 4 + i] = bits_hi >> (8 * i); }
    }
    md_somar(c, sha1, fim, pad + 8);
    for (int i = 0; i < (sha1 ? 5 : 4); i++)
        for (int j = 0; j < 4; j++)
            saida[4 * i + j] = sha1 ? c->h[i] >> (24 - 8 * j) : c->h[i] >> (8 * j);
}

/* ------------------------------------------------------------ objeto */

enum { ALG_MD5, ALG_SHA1, ALG_SHA256, ALG_SHA384, ALG_SHA512 };

static const struct { qstr nome; uint8_t tam, bloco; } algoritmos[] = {
    { MP_QSTR_md5, 16, 64 },
    { MP_QSTR_sha1, 20, 64 },
    { MP_QSTR_sha256, 32, 64 },
    { MP_QSTR_sha384, 48, 128 },
    { MP_QSTR_sha512, 64, 128 },
};

typedef struct {
    mp_obj_base_t base;
    int alg;
    union {
        md_t md;
        sha256_t s256;
        uint64_t s512[PONTE_HASH_CTX / 8];
    } c;
} hash_obj_t;

static const mp_obj_type_t tipo_hash;

static void hash_somar(hash_obj_t *h, mp_obj_t dados)
{
    mp_buffer_info_t b;
    mp_get_buffer_raise(dados, &b, MP_BUFFER_READ);
    switch (h->alg) {
    case ALG_MD5: md_somar(&h->c.md, false, b.buf, b.len); break;
    case ALG_SHA1: md_somar(&h->c.md, true, b.buf, b.len); break;
    case ALG_SHA256: sha256_somar(&h->c.s256, b.buf, b.len); break;
    default: ponte_sha512_somar(h->c.s512, b.buf, b.len); break;
    }
}

static mp_obj_t hash_novo(int alg, size_t n_args, const mp_obj_t *args, mp_map_t *kw)
{
    hash_obj_t *h = mp_obj_malloc(hash_obj_t, &tipo_hash);
    h->alg = alg;
    switch (alg) {
    case ALG_MD5: md_iniciar(&h->c.md, false); break;
    case ALG_SHA1: md_iniciar(&h->c.md, true); break;
    case ALG_SHA256: sha256_iniciar(&h->c.s256); break;
    default: ponte_sha512_iniciar(h->c.s512, alg == ALG_SHA384 ? 384 : 512); break;
    }
    mp_obj_t dados = n_args ? args[0] : MP_OBJ_NULL;
    if (kw) {   /* data= (CPython) o string= (versiones antiguas) */
        for (size_t i = 0; i < kw->alloc; i++) {
            if (!mp_map_slot_is_filled(kw, i)) continue;
            qstr k = mp_obj_str_get_qstr(kw->table[i].key);
            if (k == MP_QSTR_data || k == MP_QSTR_string) dados = kw->table[i].value;
            else if (k != MP_QSTR_usedforsecurity) mp_raise_TypeError(MP_ERROR_TEXT("unexpected keyword argument"));
        }
    }
    if (dados != MP_OBJ_NULL) hash_somar(h, dados);
    return MP_OBJ_FROM_PTR(h);
}

static mp_obj_t hash_update(mp_obj_t o, mp_obj_t dados)
{
    hash_somar(MP_OBJ_TO_PTR(o), dados);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_2(hash_update_obj, hash_update);

/* Calcula el resumen en una copia: el objeto sigue aceptando update(). */
static size_t hash_resumo(hash_obj_t *h, uint8_t *saida)
{
    hash_obj_t c = *h;
    switch (h->alg) {
    case ALG_MD5: md_fim(&c.c.md, false, saida); break;
    case ALG_SHA1: md_fim(&c.c.md, true, saida); break;
    case ALG_SHA256: sha256_fim(&c.c.s256, saida); break;
    default: ponte_sha512_fim(c.c.s512, saida); break;
    }
    return algoritmos[h->alg].tam;
}

static mp_obj_t hash_digest(mp_obj_t o)
{
    uint8_t r[64];
    size_t n = hash_resumo(MP_OBJ_TO_PTR(o), r);
    return mp_obj_new_bytes(r, n);
}
static MP_DEFINE_CONST_FUN_OBJ_1(hash_digest_obj, hash_digest);

static mp_obj_t hash_hexdigest(mp_obj_t o)
{
    uint8_t r[64];
    size_t n = hash_resumo(MP_OBJ_TO_PTR(o), r);
    vstr_t v;
    vstr_init_len(&v, 2 * n);
    for (size_t i = 0; i < n; i++) {
        v.buf[2 * i] = "0123456789abcdef"[r[i] >> 4];
        v.buf[2 * i + 1] = "0123456789abcdef"[r[i] & 15];
    }
    return mp_obj_new_str_from_vstr(&v);
}
static MP_DEFINE_CONST_FUN_OBJ_1(hash_hexdigest_obj, hash_hexdigest);

static mp_obj_t hash_copy(mp_obj_t o)
{
    hash_obj_t *c = mp_obj_malloc(hash_obj_t, &tipo_hash);
    *c = *(hash_obj_t *)MP_OBJ_TO_PTR(o);
    return MP_OBJ_FROM_PTR(c);
}
static MP_DEFINE_CONST_FUN_OBJ_1(hash_copy_obj, hash_copy);

static void hash_atributo(mp_obj_t o, qstr nome, mp_obj_t *dest)
{
    if (dest[0] != MP_OBJ_NULL) return;
    hash_obj_t *h = MP_OBJ_TO_PTR(o);
    if (nome == MP_QSTR_name) dest[0] = MP_OBJ_NEW_QSTR(algoritmos[h->alg].nome);
    else if (nome == MP_QSTR_digest_size) dest[0] = MP_OBJ_NEW_SMALL_INT(algoritmos[h->alg].tam);
    else if (nome == MP_QSTR_block_size) dest[0] = MP_OBJ_NEW_SMALL_INT(algoritmos[h->alg].bloco);
    else dest[1] = MP_OBJ_SENTINEL;
}

static void hash_imprimir(const mp_print_t *print, mp_obj_t o, mp_print_kind_t tipo)
{
    (void)tipo;
    hash_obj_t *h = MP_OBJ_TO_PTR(o);
    mp_printf(print, "<%q HASH object>", algoritmos[h->alg].nome);
}

static const mp_rom_map_elem_t hash_metodos_tab[] = {
    { MP_ROM_QSTR(MP_QSTR_update), MP_ROM_PTR(&hash_update_obj) },
    { MP_ROM_QSTR(MP_QSTR_digest), MP_ROM_PTR(&hash_digest_obj) },
    { MP_ROM_QSTR(MP_QSTR_hexdigest), MP_ROM_PTR(&hash_hexdigest_obj) },
    { MP_ROM_QSTR(MP_QSTR_copy), MP_ROM_PTR(&hash_copy_obj) },
};
static MP_DEFINE_CONST_DICT(hash_metodos, hash_metodos_tab);

static MP_DEFINE_CONST_OBJ_TYPE(
    tipo_hash,
    MP_QSTR_HASH,
    MP_TYPE_FLAG_NONE,
    print, hash_imprimir,
    attr, hash_atributo,
    locals_dict, &hash_metodos
    );

/* ------------------------------------------------------------ módulo */

#define CONSTRUTOR(nome, alg) \
    static mp_obj_t hashlib_##nome(size_t n_args, const mp_obj_t *args, mp_map_t *kw) { \
        return hash_novo(alg, n_args, args, kw); \
    } \
    static MP_DEFINE_CONST_FUN_OBJ_KW(hashlib_##nome##_obj, 0, hashlib_##nome);

CONSTRUTOR(md5, ALG_MD5)
CONSTRUTOR(sha1, ALG_SHA1)
CONSTRUTOR(sha256, ALG_SHA256)
CONSTRUTOR(sha384, ALG_SHA384)
CONSTRUTOR(sha512, ALG_SHA512)

static mp_obj_t hashlib_new(size_t n_args, const mp_obj_t *args, mp_map_t *kw)
{
    char nome[16];
    sagui_str_para_latin1(args[0], nome, sizeof nome);
    for (char *p = nome; *p; p++) {
        if (*p >= 'A' && *p <= 'Z') *p += 32;
        if (*p == '-') memmove(p, p + 1, strlen(p));   /* "SHA-256" = "sha256" */
    }
    for (size_t i = 0; i < MP_ARRAY_SIZE(algoritmos); i++)
        if (!strcmp(nome, qstr_str(algoritmos[i].nome))) return hash_novo(i, n_args - 1, args + 1, kw);
    mp_raise_ValueError(MP_ERROR_TEXT("unsupported hash type"));
}
static MP_DEFINE_CONST_FUN_OBJ_KW(hashlib_new_obj, 1, hashlib_new);

static const mp_rom_obj_tuple_t hashlib_garantidos = {
    { &mp_type_tuple }, 5,
    { MP_ROM_QSTR(MP_QSTR_md5), MP_ROM_QSTR(MP_QSTR_sha1), MP_ROM_QSTR(MP_QSTR_sha256),
      MP_ROM_QSTR(MP_QSTR_sha384), MP_ROM_QSTR(MP_QSTR_sha512) },
};

static const mp_rom_map_elem_t hashlib_globais_tab[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_hashlib) },
    { MP_ROM_QSTR(MP_QSTR_new), MP_ROM_PTR(&hashlib_new_obj) },
    { MP_ROM_QSTR(MP_QSTR_md5), MP_ROM_PTR(&hashlib_md5_obj) },
    { MP_ROM_QSTR(MP_QSTR_sha1), MP_ROM_PTR(&hashlib_sha1_obj) },
    { MP_ROM_QSTR(MP_QSTR_sha256), MP_ROM_PTR(&hashlib_sha256_obj) },
    { MP_ROM_QSTR(MP_QSTR_sha384), MP_ROM_PTR(&hashlib_sha384_obj) },
    { MP_ROM_QSTR(MP_QSTR_sha512), MP_ROM_PTR(&hashlib_sha512_obj) },
    { MP_ROM_QSTR(MP_QSTR_algorithms_guaranteed), MP_ROM_PTR(&hashlib_garantidos) },
    { MP_ROM_QSTR(MP_QSTR_algorithms_available), MP_ROM_PTR(&hashlib_garantidos) },
};
static MP_DEFINE_CONST_DICT(hashlib_globais, hashlib_globais_tab);

const mp_obj_module_t modulo_hashlib = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&hashlib_globais,
};
MP_REGISTER_MODULE(MP_QSTR_hashlib, modulo_hashlib);
