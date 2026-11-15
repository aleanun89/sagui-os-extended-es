// SPDX-License-Identifier: GPL-2.0-or-later
/* gpt.c - lectura y escritura de tablas GPT (D266). Ver gpt.h.
 *
 * Formato (UEFI 2.x, cap. 5): cabecera de 92 bytes en el LBA 1 y copia de respaldo en el último
 * sector; 128 entradas de 128 bytes en el LBA 2 (principal) y justo antes del respaldo; CRC32
 * de la cabecera (con su campo a cero) y de la matriz de entradas; MBR protector en el LBA 0.
 *
 * Escritura sin dejar el disco a medias: primero se valida todo (nada se escribe si algo no
 * cuadra); después, el respaldo (entradas y cabecera), luego la principal y por último el MBR
 * protector; al final se relee, se comprueban los CRC y se compara con lo que se quería. */
#include "gpt.h"
#include "particoes.h"
#include "compressao.h"

const uint8_t GPT_TIPO_ESP[16]      = { 0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B };
const uint8_t GPT_TIPO_LINUX[16]    = { 0xAF, 0x3D, 0xC6, 0x0F, 0x83, 0x84, 0x72, 0x47, 0x8E, 0x79, 0x3D, 0x69, 0xD8, 0x47, 0x7D, 0xE4 };
const uint8_t GPT_TIPO_DADOS_MS[16] = { 0xA2, 0xA0, 0xD0, 0xEB, 0xE5, 0xB9, 0x33, 0x44, 0x87, 0xC0, 0x68, 0xB6, 0xB7, 0x26, 0x99, 0xC7 };
const uint8_t GPT_TIPO_SAGUI[16]    = { 0x55, 0x47, 0x41, 0x53, 0x4F, 0x49, 0x53, 0x4F, 0x80, 0x01, 0x53, 0x41, 0x47, 0x55, 0x49, 0x53 };
static const uint8_t GPT_TIPO_SWAP[16] = { 0x6D, 0xFD, 0x57, 0x06, 0xAB, 0xA4, 0xC4, 0x43, 0x84, 0xE5, 0x09, 0x33, 0xC8, 0x4B, 0x4F, 0x4F };
static const uint8_t GPT_TIPO_MSR[16]  = { 0x16, 0xE3, 0xC9, 0xE3, 0x5C, 0x0B, 0xB8, 0x4D, 0x81, 0x7D, 0xF9, 0x2D, 0xF0, 0x02, 0x15, 0xAE };
static const uint8_t GPT_TIPO_RECUP[16] = { 0xA4, 0xBB, 0x94, 0xDE, 0xD1, 0x06, 0x40, 0x4D, 0xA1, 0x6A, 0xBF, 0xD5, 0x01, 0x79, 0xD6, 0xAC };
static const uint8_t GPT_TIPO_BIOS[16]  = { 0x48, 0x61, 0x68, 0x21, 0x49, 0x64, 0x6F, 0x6E, 0x74, 0x4E, 0x65, 0x65, 0x64, 0x45, 0x46, 0x49 };

static uint32_t le32(const uint8_t *p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
static void po32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

static void txt(char *m, int max, const char *fmt, ...)
{
    if (!m || max <= 0) return;
    va_list ap;
    va_start(ap, fmt);
    formatar_lista(m, (size_t)max, fmt, ap);
    va_end(ap);
}

uint32_t gpt_crc(const void *dados, size_t n) { return crc32_somar(0, dados, n); }

static bool igual16(const uint8_t *a, const uint8_t *b) { return memcmp(a, b, 16) == 0; }
static bool cero16(const uint8_t *a) { for (int i = 0; i < 16; i++) if (a[i]) return false; return true; }

/* ------------------------------------------------------------ consulta */

static const uint8_t *ent_de(const gpt_t *g, int slot) { return g->ent + (unsigned)slot * GPT_TAM_ENT; }
bool gpt_usada(const gpt_t *g, int slot) { return slot >= 0 && slot < (int)GPT_ENTRADAS && !cero16(ent_de(g, slot)); }
const uint8_t *gpt_tipo(const gpt_t *g, int slot) { return ent_de(g, slot); }
/* D283: las entradas del disco cuentan en bloques reales (esc sectores de 512 cada uno); la API, en sectores. */
static uint32_t escala(const gpt_t *g) { return g->esc ? g->esc : 1; }
static uint64_t ent_ini64(const gpt_t *g, const uint8_t *e) { return (((uint64_t)le32(e + 36) << 32) | le32(e + 32)) * escala(g); }
static uint64_t ent_fim64(const gpt_t *g, const uint8_t *e) { return (((((uint64_t)le32(e + 44) << 32) | le32(e + 40)) + 1) * escala(g)) - 1; }
bool gpt_alem_32bits(const gpt_t *g, int slot)
{
    const uint8_t *e = ent_de(g, slot);
    return ent_ini64(g, e) > 0xFFFFFFFFull || ent_fim64(g, e) > 0xFFFFFFFFull;
}
uint32_t gpt_inicio(const gpt_t *g, int slot) { return (uint32_t)ent_ini64(g, ent_de(g, slot)); }
uint32_t gpt_setores_ent(const gpt_t *g, int slot)
{
    if (gpt_alem_32bits(g, slot)) return 0;
    const uint8_t *e = ent_de(g, slot);
    return (uint32_t)(ent_fim64(g, e) - ent_ini64(g, e) + 1);
}

void gpt_nome_ent(const gpt_t *g, int slot, char *s, int max)
{
    const uint8_t *x = ent_de(g, slot);
    int k = 0;
    for (int c = 0; c < 36 && k < max - 1; c++) {
        uint16_t u = (uint16_t)(x[56 + c * 2] | x[57 + c * 2] << 8);
        if (!u) break;
        s[k++] = u < 256 ? (char)u : '?';
    }
    s[k] = 0;
}

int gpt_num_usadas(const gpt_t *g)
{
    int n = 0;
    for (int i = 0; i < (int)GPT_ENTRADAS; i++) n += gpt_usada(g, i);
    return n;
}

int gpt_numero_kernel(const gpt_t *g, int slot)
{
    /* El kernel recorre las entradas por orden y se queda con las cuatro primeras usadas (y que
     * quepan en 32 bits); el nombre lleva el número de la entrada. */
    int vistas = 0;
    for (int i = 0; i < (int)GPT_ENTRADAS && vistas < GPT_VISIVEIS; i++) {
        if (!gpt_usada(g, i) || gpt_alem_32bits(g, i)) continue;
        vistas++;
        if (i == slot) return i + 1;
    }
    return 0;
}

const char *gpt_nome_tipo(const uint8_t *g)
{
    if (igual16(g, GPT_TIPO_ESP)) return "EFI (sistema)";
    if (igual16(g, GPT_TIPO_SAGUI)) return "SaguiOS (sistema)";
    if (igual16(g, GPT_TIPO_DADOS_MS)) return "datos de Windows";
    if (igual16(g, GPT_TIPO_MSR)) return "reservada de Microsoft";
    if (igual16(g, GPT_TIPO_RECUP)) return "recuperación de Windows";
    if (igual16(g, GPT_TIPO_LINUX)) return "Linux";
    if (igual16(g, GPT_TIPO_SWAP)) return "swap de Linux";
    if (igual16(g, GPT_TIPO_BIOS)) return "arranque BIOS";
    return NULL;
}

void gpt_guid_texto(const uint8_t *g, char *s, int max)
{
    formatar_texto(s, (size_t)max, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                   g[3], g[2], g[1], g[0], g[5], g[4], g[7], g[6], g[8], g[9], g[10], g[11], g[12], g[13], g[14], g[15]);
}

uint8_t gpt_tipo_mbr(const uint8_t *g)
{
    if (igual16(g, GPT_TIPO_ESP)) return 0xEF;
    if (igual16(g, GPT_TIPO_SAGUI)) return 0xDA;
    if (igual16(g, GPT_TIPO_LINUX)) return 0x83;
    if (igual16(g, GPT_TIPO_SWAP)) return 0x82;
    if (igual16(g, GPT_TIPO_DADOS_MS)) return 0x07;
    return 0x99;
}

const uint8_t *gpt_guid_de_tipo_mbr(uint8_t t)
{
    switch (t) {
    case 0xEF: return GPT_TIPO_ESP;
    case 0xDA: return GPT_TIPO_SAGUI;
    case 0x83: return GPT_TIPO_LINUX;
    case 0x82: return GPT_TIPO_SWAP;
    case 0x07: case 0x0B: case 0x0C: return GPT_TIPO_DADOS_MS;
    default: return NULL;
    }
}

/* ------------------------------------------------------------ lectura */

/* Lee y valida una cabecera en "lba"; si vale, deja en g los datos y lee las entradas. */
static int ler_uma(const char *disco, uint32_t setores, uint32_t lba, bool principal, gpt_t *g, char *erro, int max)
{
    uint8_t s[512];
    int r = blocos_ler(disco, lba, 1, s);
    if (r < 0) { txt(erro, max, "no se puede leer el sector %u: %s", lba, nome_erro(r)); return r; }
    const uint32_t f = escala(g);
    const char *quem = principal ? "la cabecera principal" : "la cabecera de respaldo";
    if (memcmp(s, "EFI PART", 8) != 0) { txt(erro, max, "%s no tiene la firma EFI PART", quem); return ERRO_FORMATO; }
    uint32_t rev = le32(s + 8), hsz = le32(s + 12);
    if (rev != 0x00010000u || hsz < 92 || hsz > 512) { txt(erro, max, "%s: revisión o tamaño no soportados", quem); return ERRO_FORMATO; }
    uint8_t c[512];
    memcpy(c, s, hsz);
    po32(c + 16, 0);
    if (gpt_crc(c, hsz) != le32(s + 16)) { txt(erro, max, "%s: el CRC no cuadra", quem); return ERRO_FORMATO; }
    if (le32(s + 24) * f != lba || le32(s + 28)) { txt(erro, max, "%s: no está en su sitio", quem); return ERRO_FORMATO; }
    uint32_t n = le32(s + 80), tam = le32(s + 84), lba_ent = le32(s + 72) * f;
    if (le32(s + 76)) { txt(erro, max, "%s: las entradas empiezan más allá de 2 TB", quem); return ERRO_FORMATO; }
    if (tam != GPT_TAM_ENT) { txt(erro, max, "%s: entradas de %u bytes (solo se admiten 128)", quem, tam); return ERRO_FORMATO; }
    if (n == 0 || n > GPT_ENTRADAS) { txt(erro, max, "%s: %u entradas (se admiten hasta %u)", quem, n, GPT_ENTRADAS); return ERRO_FORMATO; }
    uint32_t sec = (n * tam + 511) / 512;
    if (lba_ent < 2 || lba_ent > setores || sec > setores - lba_ent) { txt(erro, max, "%s: la tabla de entradas cae fuera del disco", quem); return ERRO_FORMATO; }
    memset(g->ent, 0, sizeof g->ent);
    r = blocos_ler(disco, lba_ent, sec, g->ent);
    if (r < 0) { txt(erro, max, "no se pueden leer las entradas: %s", nome_erro(r)); return r; }
    if (gpt_crc(g->ent, n * tam) != le32(s + 88)) { memset(g->ent, 0, sizeof g->ent); txt(erro, max, "%s: el CRC de las entradas no cuadra", quem); return ERRO_FORMATO; }
    memcpy(g->guid, s + 56, 16);
    g->primeiro = le32(s + 44) ? 0xFFFFFFFFu : le32(s + 40) * f;
    g->ultimo = le32(s + 52) ? 0xFFFFFFFFu : (le32(s + 48) + 1) * f - 1;
    if (g->ultimo >= setores) g->ultimo = setores - 1;
    return 0;
}

int gpt_ler(const char *disco, gpt_t *g, char *erro, int max)
{
    info_bloco_t inf;
    int r = blocos_info(disco, &inf);
    if (r < 0) { txt(erro, max, "%s: %s", disco, nome_erro(r)); return r; }
    memset(g, 0, sizeof *g);
    copia_texto(g->disco, disco, sizeof g->disco);
    g->setores = inf.setores;
    g->esc = inf.bloco_fisico == 4096 ? 8 : 1;   /* D283 */
    if (inf.inicio || inf.setores < 68 * g->esc) { txt(erro, max, "%s no es un disco entero con sitio para una GPT", disco); return ERRO_INVALIDO; }
    char e1[100], e2[100];
    r = ler_uma(disco, inf.setores, g->esc, true, g, e1, sizeof e1);
    if (r == 0) {
        g->origem = GPT_PRINCIPAL;
        gpt_t *t = aloca(sizeof *t);   /* ¿vale también el respaldo? (no se guarda, solo se avisa) */
        if (t) {
            memset(t, 0, sizeof *t);
            t->esc = g->esc;
            g->respaldo_ok = ler_uma(disco, inf.setores, inf.setores - g->esc, false, t, e2, sizeof e2) == 0;
            libera(t);
        }
        return 0;
    }
    int r2 = ler_uma(disco, inf.setores, inf.setores - g->esc, false, g, e2, sizeof e2);
    if (r2 == 0) { g->origem = GPT_RESPALDO; return 0; }
    txt(erro, max, "GPT no válida: %s; %s", e1, e2);
    return r;
}

/* ------------------------------------------------------------ edición en memoria */

void gpt_nova(gpt_t *g, const char *disco, uint32_t setores)
{
    memset(g, 0, sizeof *g);
    copia_texto(g->disco, disco, sizeof g->disco);
    g->setores = setores;
    info_bloco_t inf;
    g->esc = blocos_info(disco, &inf) == 0 && inf.bloco_fisico == 4096 ? 8 : 1;   /* D283 */
    g->primeiro = (2 + GPT_SETORES_ENT / g->esc) * g->esc;
    g->ultimo = setores - (1 + GPT_SETORES_ENT / g->esc) * g->esc - 1;
    bytes_aleatorios(g->guid, 16);
    g->guid[7] = (uint8_t)((g->guid[7] & 0x0F) | 0x40);   /* versión 4 */
    g->guid[8] = (uint8_t)((g->guid[8] & 0x3F) | 0x80);
    g->origem = GPT_PRINCIPAL;
}

bool gpt_validar(gpt_t *g, char *erro, int max)
{
    const uint32_t f = escala(g);
    if (g->setores < 68 * f) { txt(erro, max, "el disco es demasiado pequeño para una GPT"); return false; }
    g->primeiro = (2 + GPT_SETORES_ENT / f) * f;                  /* D283: en bloques de 4096, 6 bloques = 48 sectores */
    g->ultimo = g->setores - (1 + GPT_SETORES_ENT / f) * f - 1;
    for (int i = 0; i < (int)GPT_ENTRADAS; i++) {
        if (!gpt_usada(g, i)) continue;
        const uint8_t *e = ent_de(g, i);
        if (gpt_alem_32bits(g, i)) continue;   /* más allá de 2 TB: se conserva tal cual */
        uint32_t a = (uint32_t)ent_ini64(g, e), b = (uint32_t)ent_fim64(g, e);
        if (a % f || (b + 1) % f) { txt(erro, max, "la entrada %d no está alineada a los bloques de %u bytes del disco", i + 1, f * 512); return false; }
        if (b < a || a < g->primeiro || b > g->ultimo) {
            txt(erro, max, "la entrada %d (sectores %u a %u) cae fuera de la zona utilizable (%u a %u)", i + 1, a, b, g->primeiro, g->ultimo);
            return false;
        }
        for (int k = i + 1; k < (int)GPT_ENTRADAS; k++) {
            if (!gpt_usada(g, k) || gpt_alem_32bits(g, k)) continue;
            uint32_t a2 = (uint32_t)ent_ini64(g, ent_de(g, k)), b2 = (uint32_t)ent_fim64(g, ent_de(g, k));
            if (a <= b2 && a2 <= b) { txt(erro, max, "las entradas %d y %d se solapan", i + 1, k + 1); return false; }
        }
    }
    return true;
}

int gpt_adicionar(gpt_t *g, const uint8_t *tipo, uint32_t inicio, uint32_t setores, const char *nome, char *erro, int max)
{
    if (!tipo || cero16(tipo)) { txt(erro, max, "falta el tipo de la partición"); return ERRO_INVALIDO; }
    if (!setores) { txt(erro, max, "la partición está vacía"); return ERRO_INVALIDO; }
    if (gpt_num_usadas(g) >= GPT_VISIVEIS) {
        txt(erro, max, "SaguiOS solo gestiona las cuatro primeras particiones de una GPT: borra alguna antes");
        return ERRO_LIMITE;
    }
    int slot = -1;
    for (int i = 0; i < (int)GPT_ENTRADAS && slot < 0; i++) if (!gpt_usada(g, i)) slot = i;
    if (slot < 0) { txt(erro, max, "la tabla está llena"); return ERRO_LIMITE; }
    uint32_t fim = inicio + setores - 1;
    if (fim < inicio) { txt(erro, max, "la partición no cabe en 32 bits"); return ERRO_INVALIDO; }
    uint8_t *e = g->ent + (unsigned)slot * GPT_TAM_ENT;
    uint8_t copia[GPT_TAM_ENT];
    memcpy(copia, e, sizeof copia);
    memset(e, 0, GPT_TAM_ENT);
    memcpy(e, tipo, 16);
    bytes_aleatorios(e + 16, 16);
    e[16 + 7] = (uint8_t)((e[16 + 7] & 0x0F) | 0x40);
    e[16 + 8] = (uint8_t)((e[16 + 8] & 0x3F) | 0x80);
    po32(e + 32, inicio / escala(g));            /* D283: en el disco, bloques reales */
    po32(e + 40, (fim + 1) / escala(g) - 1);
    for (int c = 0; c < 35 && nome && nome[c]; c++) { e[56 + c * 2] = (uint8_t)nome[c]; e[57 + c * 2] = 0; }
    if (!gpt_validar(g, erro, max)) { memcpy(e, copia, sizeof copia); return ERRO_INVALIDO; }
    return slot;
}

int gpt_remover(gpt_t *g, int slot, char *erro, int max)
{
    if (!gpt_usada(g, slot)) { txt(erro, max, "la entrada %d no existe", slot + 1); return ERRO_NAO_EXISTE; }
    memset(g->ent + (unsigned)slot * GPT_TAM_ENT, 0, GPT_TAM_ENT);
    return 0;
}

/* ------------------------------------------------------------ escritura */

static void montar_cab(const gpt_t *g, uint32_t mi, uint32_t alt, uint32_t lba_ent, uint32_t crc_ent, uint8_t *s)
{
    memset(s, 0, 512);
    memcpy(s, "EFI PART", 8);
    po32(s + 8, 0x00010000u);
    po32(s + 12, 92);
    po32(s + 24, mi);
    po32(s + 32, alt);
    po32(s + 40, g->primeiro / escala(g));
    po32(s + 48, (g->ultimo + 1) / escala(g) - 1);
    memcpy(s + 56, g->guid, 16);
    po32(s + 72, lba_ent);
    po32(s + 80, GPT_ENTRADAS);
    po32(s + 84, GPT_TAM_ENT);
    po32(s + 88, crc_ent);
    po32(s + 16, gpt_crc(s, 92));
}

static bool mbr_protector_ok(const uint8_t *s0, uint32_t setores)   /* "setores" ya en bloques reales */
{
    if (s0[510] != 0x55 || s0[511] != 0xAA) return false;
    int ee = 0, outras = 0;
    for (int i = 0; i < 4; i++) {
        const uint8_t *e = s0 + 446 + i * 16;
        if (!e[4]) continue;
        if (e[4] == 0xEE && le32(e + 8) == 1 && le32(e + 12) == (setores - 1 > 0xFFFFFFFFu ? 0xFFFFFFFFu : setores - 1)) ee++;
        else outras++;
    }
    return ee == 1 && outras == 0;
}

int gpt_gravar(gpt_t *g, bool reescrever_mbr, const uint8_t *codigo, char *erro, int max)
{
    if (!gpt_validar(g, erro, max)) return ERRO_INVALIDO;
    info_bloco_t inf;
    int r = blocos_info(g->disco, &inf);
    if (r < 0) { txt(erro, max, "%s: %s", g->disco, nome_erro(r)); return r; }
    if (inf.setores != g->setores) { txt(erro, max, "el disco ha cambiado de tamaño"); return ERRO_INVALIDO; }
    const uint32_t f = escala(g), nblk = g->setores / f;
    uint32_t ultimo = g->setores - f, crc_ent = gpt_crc(g->ent, GPT_ENTRADAS * GPT_TAM_ENT);
    uint8_t cp[512], cb[512], s0[512];
    montar_cab(g, 1, nblk - 1, 2, crc_ent, cp);
    montar_cab(g, nblk - 1, 1, nblk - 1 - GPT_SETORES_ENT / f, crc_ent, cb);
    r = blocos_ler(g->disco, 0, 1, s0);
    if (r < 0) { txt(erro, max, "no se puede leer el MBR: %s", nome_erro(r)); return r; }
    bool mbr = reescrever_mbr || !mbr_protector_ok(s0, nblk);
    uint8_t m[512];
    if (mbr) {
        memset(m, 0, sizeof m);
        if (codigo) memcpy(m, codigo, 446);
        else if (s0[510] == 0x55 && s0[511] == 0xAA) memcpy(m, s0, 440);   /* conserva el código de arranque que hubiera */
        uint8_t *e = m + 446;
        e[1] = 0x00; e[2] = 0x02; e[3] = 0x00;   /* CHS del sector 1 */
        e[4] = 0xEE;
        e[5] = 0xFF; e[6] = 0xFF; e[7] = 0xFF;
        po32(e + 8, 1);
        po32(e + 12, nblk - 1);
        m[510] = 0x55; m[511] = 0xAA;
    }
    /* Respaldo primero, la principal después, el MBR protector al final. */
    if ((r = blocos_escrever(g->disco, ultimo - GPT_SETORES_ENT, GPT_SETORES_ENT, g->ent)) < 0 ||
        (r = blocos_escrever(g->disco, ultimo, 1, cb)) < 0 ||
        (r = blocos_escrever(g->disco, 2 * f, GPT_SETORES_ENT, g->ent)) < 0 ||
        (r = blocos_escrever(g->disco, f, 1, cp)) < 0 ||
        (mbr && (r = blocos_escrever(g->disco, 0, 1, m)) < 0)) {
        txt(erro, max, "error al escribir la GPT de %s (%s): la tabla puede haber quedado a medias", g->disco, nome_erro(r));
        return r;
    }
    /* Releer y comprobar: las dos cabeceras con su CRC, y las entradas iguales a las escritas. */
    gpt_t *l = aloca(sizeof *l);
    if (!l) { txt(erro, max, "sin memoria para comprobar la GPT"); return ERRO_SEM_MEMORIA; }
    char e1[120];
    int v = gpt_ler(g->disco, l, e1, sizeof e1);
    bool ok = v == 0 && l->origem == GPT_PRINCIPAL && l->respaldo_ok && memcmp(l->ent, g->ent, sizeof g->ent) == 0 &&
              memcmp(l->guid, g->guid, 16) == 0;
    uint8_t rb[512];
    if (ok && (blocos_ler(g->disco, ultimo, 1, rb) < 0 || memcmp(rb, cb, 512) != 0)) ok = false;
    if (ok && mbr) { uint8_t r0[512]; if (blocos_ler(g->disco, 0, 1, r0) < 0 || memcmp(r0, m, 512) != 0) ok = false; }
    libera(l);
    if (!ok) {
        txt(erro, max, "la GPT releída de %s no coincide con la escrita (%s)", g->disco, v < 0 ? e1 : "contenido distinto");
        return ERRO_FORMATO;
    }
    g->origem = GPT_PRINCIPAL;
    g->respaldo_ok = true;
    blocos_reler(g->disco);
    registrar("gpt: %s: tabla escrita (%u entradas usadas, crc entradas %x)", g->disco, (unsigned)gpt_num_usadas(g), crc_ent);
    return 0;
}
