// SPDX-License-Identifier: GPL-2.0-or-later
/* wpa.c - criptografía y formatos del Wi-Fi cliente (D215). Ver wpa.h.
 *
 * Escrito a partir de las normas: SHA-1 (FIPS 180-4, 6.1), HMAC (RFC 2104),
 * PBKDF2 (RFC 2898, 5.2), AES (FIPS 197), envoltura de claves AES (RFC 3394)
 * y la 802.11i tal como está en IEEE 802.11-2016 (12.7: PRF, jerarquía de
 * claves, EAPOL-Key y el saludo de cuatro pasos). Las pruebas del anfitrión
 * (tests/h215_wifi.py) lo comprueban con los vectores de esas normas.
 *
 * El AES usa tablas (la caja S se calcula la primera vez): solo sirve para
 * desenvolver la GTK, una vez por saludo; el cifrado de las tramas lo hace el
 * chip (CCMP por hardware).
 *
 * D220 (v7.3.2): el saludo también con WPA1 (descriptor 254), TKIP
 * (descriptor 1: HMAC-MD5 y RC4), PSK-SHA256 y SAE (descriptor 3 y 0:
 * AES-CMAC y el KDF con SHA-256), la IGTK de la 802.11w y el RSNXE; las
 * primitivas nuevas están en wpa_cifra.c. */
#include "wpa.h"
#include "wpa_cifra.h"

/* ------------------------------------------------------------ utilidades */

static void copiar(void *d, const void *o, uint32_t n)
{
    uint8_t *a = d;
    const uint8_t *b = o;
    while (n--) *a++ = *b++;
}

static void zerar(void *d, uint32_t n)
{
    uint8_t *a = d;
    while (n--) *a++ = 0;
}

bool wpa_igual(const void *a, const void *b, uint32_t n)
{
    const volatile uint8_t *x = a, *y = b;
    uint8_t d = 0;
    for (uint32_t i = 0; i < n; i++) d |= x[i] ^ y[i];
    return d == 0;
}

void wpa_apagar(void *p, uint32_t n)
{
    volatile uint8_t *v = p;
    while (n--) *v++ = 0;
}

static uint16_t le_be16(const uint8_t *p) { return (uint16_t)(p[0] << 8 | p[1]); }
static void poe_be16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v >> 8); p[1] = (uint8_t)v; }

/* ------------------------------------------------------------ SHA-1 */

#define ROTE(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static void sha1_comprimir(uint32_t h[5], const uint8_t *b)
{
    uint32_t w[80];
    for (int t = 0; t < 16; t++)
        w[t] = (uint32_t)b[4 * t] << 24 | (uint32_t)b[4 * t + 1] << 16 | (uint32_t)b[4 * t + 2] << 8 | b[4 * t + 3];
    for (int t = 16; t < 80; t++) w[t] = ROTE(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
    uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4];
    for (int t = 0; t < 80; t++) {
        uint32_t f, k;
        if (t < 20) { f = (bb & c) | (~bb & d); k = 0x5A827999u; }
        else if (t < 40) { f = bb ^ c ^ d; k = 0x6ED9EBA1u; }
        else if (t < 60) { f = (bb & c) | (bb & d) | (c & d); k = 0x8F1BBCDCu; }
        else { f = bb ^ c ^ d; k = 0xCA62C1D6u; }
        uint32_t tmp = ROTE(a, 5) + f + e + k + w[t];
        e = d; d = c; c = ROTE(bb, 30); bb = a; a = tmp;
    }
    h[0] += a; h[1] += bb; h[2] += c; h[3] += d; h[4] += e;
}

void sha1_iniciar(sha1_t *c)
{
    c->h[0] = 0x67452301u; c->h[1] = 0xEFCDAB89u; c->h[2] = 0x98BADCFEu; c->h[3] = 0x10325476u; c->h[4] = 0xC3D2E1F0u;
    c->usados = 0;
    c->total = 0;
}

void sha1_somar(sha1_t *c, const void *dados, uint32_t n)
{
    const uint8_t *p = dados;
    c->total += n;
    while (n) {
        uint32_t k = SHA1_BLOCO - c->usados;
        if (k > n) k = n;
        copiar(c->bloco + c->usados, p, k);
        c->usados += k;
        p += k;
        n -= k;
        if (c->usados == SHA1_BLOCO) { sha1_comprimir(c->h, c->bloco); c->usados = 0; }
    }
}

void sha1_fim(sha1_t *c, uint8_t saida[SHA1_TAM])
{
    uint64_t bits = c->total * 8;
    uint8_t um = 0x80, zero = 0;
    sha1_somar(c, &um, 1);
    while (c->usados != SHA1_BLOCO - 8) sha1_somar(c, &zero, 1);
    uint8_t t[8];
    for (int i = 0; i < 8; i++) t[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha1_somar(c, t, 8);
    for (int i = 0; i < 5; i++) {
        saida[4 * i] = (uint8_t)(c->h[i] >> 24); saida[4 * i + 1] = (uint8_t)(c->h[i] >> 16);
        saida[4 * i + 2] = (uint8_t)(c->h[i] >> 8); saida[4 * i + 3] = (uint8_t)c->h[i];
    }
}

void sha1(const void *dados, uint32_t n, uint8_t saida[SHA1_TAM])
{
    sha1_t c;
    sha1_iniciar(&c);
    sha1_somar(&c, dados, n);
    sha1_fim(&c, saida);
}

/* ------------------------------------------------------------ HMAC-SHA1, PBKDF2 y PRF */

void hmac_sha1_partes(const uint8_t *chave, uint32_t nchave, int partes, const uint8_t *const *p,
                      const uint32_t *n, uint8_t saida[SHA1_TAM])
{
    uint8_t k[SHA1_BLOCO], pad[SHA1_BLOCO], interno[SHA1_TAM];
    zerar(k, sizeof k);
    if (nchave > SHA1_BLOCO) sha1(chave, nchave, k);
    else copiar(k, chave, nchave);
    sha1_t c;
    for (int i = 0; i < SHA1_BLOCO; i++) pad[i] = k[i] ^ 0x36;
    sha1_iniciar(&c);
    sha1_somar(&c, pad, SHA1_BLOCO);
    for (int i = 0; i < partes; i++) sha1_somar(&c, p[i], n[i]);
    sha1_fim(&c, interno);
    for (int i = 0; i < SHA1_BLOCO; i++) pad[i] = k[i] ^ 0x5C;
    sha1_iniciar(&c);
    sha1_somar(&c, pad, SHA1_BLOCO);
    sha1_somar(&c, interno, SHA1_TAM);
    sha1_fim(&c, saida);
    wpa_apagar(k, sizeof k);
    wpa_apagar(pad, sizeof pad);
    wpa_apagar(&c, sizeof c);
}

void hmac_sha1(const uint8_t *chave, uint32_t nchave, const uint8_t *dados, uint32_t n, uint8_t saida[SHA1_TAM])
{
    const uint8_t *p[1] = { dados };
    uint32_t t[1] = { n };
    hmac_sha1_partes(chave, nchave, 1, p, t, saida);
}

void pbkdf2_sha1(const uint8_t *senha, uint32_t nsenha, const uint8_t *sal, uint32_t nsal, uint32_t iteracoes,
                 uint8_t *saida, uint32_t nsaida)
{
    for (uint32_t bloco = 1; nsaida; bloco++) {
        uint8_t ind[4] = { (uint8_t)(bloco >> 24), (uint8_t)(bloco >> 16), (uint8_t)(bloco >> 8), (uint8_t)bloco };
        const uint8_t *p[2] = { sal, ind };
        uint32_t n[2] = { nsal, 4 };
        uint8_t u[SHA1_TAM], t[SHA1_TAM];
        hmac_sha1_partes(senha, nsenha, 2, p, n, u);
        copiar(t, u, SHA1_TAM);
        for (uint32_t i = 1; i < iteracoes; i++) {
            hmac_sha1(senha, nsenha, u, SHA1_TAM, u);
            for (int j = 0; j < SHA1_TAM; j++) t[j] ^= u[j];
        }
        uint32_t k = nsaida < SHA1_TAM ? nsaida : SHA1_TAM;
        copiar(saida, t, k);
        saida += k;
        nsaida -= k;
        wpa_apagar(u, sizeof u);
        wpa_apagar(t, sizeof t);
    }
}

bool wpa_pmk(const char *senha, const uint8_t *ssid, uint32_t nssid, uint8_t pmk[WPA_PMK_TAM])
{
    uint32_t n = 0;
    while (senha[n]) n++;
    if (n < 8 || n > 63 || nssid < 1 || nssid > 32) return false;
    for (uint32_t i = 0; i < n; i++)   /* la norma pide ASCII imprimible (32..126) */
        if ((uint8_t)senha[i] < 32 || (uint8_t)senha[i] > 126) return false;
    pbkdf2_sha1((const uint8_t *)senha, n, ssid, nssid, 4096, pmk, WPA_PMK_TAM);
    return true;
}

void wpa_prf(const uint8_t *chave, uint32_t nchave, const char *rotulo, const uint8_t *b, uint32_t nb,
             uint8_t *saida, uint32_t nsaida)
{
    uint32_t nr = 0;
    while (rotulo[nr]) nr++;
    uint8_t zero = 0;
    for (uint8_t i = 0; nsaida; i++) {
        const uint8_t *p[4] = { (const uint8_t *)rotulo, &zero, b, &i };
        uint32_t n[4] = { nr, 1, nb, 1 };
        uint8_t h[SHA1_TAM];
        hmac_sha1_partes(chave, nchave, 4, p, n, h);
        uint32_t k = nsaida < SHA1_TAM ? nsaida : SHA1_TAM;
        copiar(saida, h, k);
        saida += k;
        nsaida -= k;
        wpa_apagar(h, sizeof h);
    }
}

/* ------------------------------------------------------------ AES-128 (FIPS 197) */

static uint8_t sbox[256], isbox[256];
static bool aes_pronto;

static uint8_t rotl8(uint8_t x, int n) { return (uint8_t)((x << n) | (x >> (8 - n))); }

/* La caja S: inverso en GF(2^8) seguido de la transformación afín (5.1.1).
 * p recorre el grupo multiplicativo con el generador 3; q = 1/p. */
static void aes_tabelas(void)
{
    uint8_t p = 1, q = 1;
    do {
        p = (uint8_t)(p ^ (uint8_t)(p << 1) ^ ((p & 0x80) ? 0x1B : 0));
        q ^= (uint8_t)(q << 1);
        q ^= (uint8_t)(q << 2);
        q ^= (uint8_t)(q << 4);
        if (q & 0x80) q ^= 0x09;
        sbox[p] = (uint8_t)(q ^ rotl8(q, 1) ^ rotl8(q, 2) ^ rotl8(q, 3) ^ rotl8(q, 4) ^ 0x63);
    } while (p != 1);
    sbox[0] = 0x63;
    for (int i = 0; i < 256; i++) isbox[sbox[i]] = (uint8_t)i;
    aes_pronto = true;
}

uint8_t aes_caixa_s(uint8_t x)
{
    if (!aes_pronto) aes_tabelas();
    return sbox[x];
}

static uint8_t xtime(uint8_t b) { return (uint8_t)((b << 1) ^ ((b & 0x80) ? 0x1B : 0)); }

static uint8_t gmul(uint8_t a, uint8_t b)
{
    uint8_t r = 0;
    while (b) { if (b & 1) r ^= a; a = xtime(a); b >>= 1; }
    return r;
}

void aes128_chave(aes128_t *a, const uint8_t chave[16])
{
    if (!aes_pronto) aes_tabelas();
    uint8_t *w = &a->rk[0][0];
    copiar(w, chave, 16);
    uint8_t rcon = 1;
    for (int i = 4; i < 44; i++) {
        uint8_t t[4];
        copiar(t, w + 4 * (i - 1), 4);
        if (i % 4 == 0) {
            uint8_t x = t[0];
            t[0] = (uint8_t)(sbox[t[1]] ^ rcon); t[1] = sbox[t[2]]; t[2] = sbox[t[3]]; t[3] = sbox[x];
            rcon = xtime(rcon);
        }
        for (int j = 0; j < 4; j++) w[4 * i + j] = w[4 * (i - 4) + j] ^ t[j];
    }
}

static void somar_chave(uint8_t s[16], const uint8_t *k) { for (int i = 0; i < 16; i++) s[i] ^= k[i]; }

void aes128_cifrar(const aes128_t *a, const uint8_t entrada[16], uint8_t saida[16])
{
    uint8_t s[16], t[16];
    copiar(s, entrada, 16);
    somar_chave(s, a->rk[0]);
    for (int r = 1; r <= 10; r++) {
        for (int i = 0; i < 16; i++) s[i] = sbox[s[i]];
        /* ShiftRows: la fila f (byte f de cada columna) gira f posiciones */
        for (int c = 0; c < 4; c++)
            for (int f = 0; f < 4; f++) t[4 * c + f] = s[4 * ((c + f) % 4) + f];
        if (r < 10)
            for (int c = 0; c < 4; c++) {
                uint8_t *v = t + 4 * c, a0 = v[0], a1 = v[1], a2 = v[2], a3 = v[3], x = a0 ^ a1 ^ a2 ^ a3;
                v[0] ^= x ^ xtime(a0 ^ a1); v[1] ^= x ^ xtime(a1 ^ a2);
                v[2] ^= x ^ xtime(a2 ^ a3); v[3] ^= x ^ xtime(a3 ^ a0);
            }
        copiar(s, t, 16);
        somar_chave(s, a->rk[r]);
    }
    copiar(saida, s, 16);
}

void aes128_decifrar(const aes128_t *a, const uint8_t entrada[16], uint8_t saida[16])
{
    uint8_t s[16], t[16];
    copiar(s, entrada, 16);
    somar_chave(s, a->rk[10]);
    for (int r = 9; r >= 0; r--) {
        /* InvShiftRows y InvSubBytes */
        for (int c = 0; c < 4; c++)
            for (int f = 0; f < 4; f++) t[4 * ((c + f) % 4) + f] = isbox[s[4 * c + f]];
        somar_chave(t, a->rk[r]);
        if (r > 0)
            for (int c = 0; c < 4; c++) {
                uint8_t *v = t + 4 * c, a0 = v[0], a1 = v[1], a2 = v[2], a3 = v[3];
                v[0] = (uint8_t)(gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9));
                v[1] = (uint8_t)(gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13));
                v[2] = (uint8_t)(gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11));
                v[3] = (uint8_t)(gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14));
            }
        copiar(s, t, 16);
    }
    copiar(saida, s, 16);
}

/* ------------------------------------------------------------ envoltura de claves (RFC 3394) */

void aes_envolver(const uint8_t kek[16], const uint8_t *claro, uint32_t n, uint8_t *envolto)
{
    aes128_t a;
    aes128_chave(&a, kek);
    uint32_t m = n / 8;
    uint8_t b[16];
    for (int i = 0; i < 8; i++) envolto[i] = 0xA6;
    copiar(envolto + 8, claro, n);
    for (uint32_t j = 0; j < 6; j++)
        for (uint32_t i = 1; i <= m; i++) {
            copiar(b, envolto, 8);
            copiar(b + 8, envolto + 8 * i, 8);
            aes128_cifrar(&a, b, b);
            uint32_t t = m * j + i;
            for (int k = 0; k < 4; k++) b[7 - k] ^= (uint8_t)(t >> (8 * k));
            copiar(envolto, b, 8);
            copiar(envolto + 8 * i, b + 8, 8);
        }
    wpa_apagar(&a, sizeof a);
    wpa_apagar(b, sizeof b);
}

bool aes_desenvolver(const uint8_t kek[16], const uint8_t *envolto, uint32_t n_envolto, uint8_t *claro)
{
    if (n_envolto < 24 || n_envolto % 8) return false;
    aes128_t a;
    aes128_chave(&a, kek);
    uint32_t m = n_envolto / 8 - 1;
    uint8_t A[8], b[16];
    copiar(A, envolto, 8);
    copiar(claro, envolto + 8, 8 * m);
    for (int j = 5; j >= 0; j--)
        for (uint32_t i = m; i >= 1; i--) {
            uint32_t t = m * (uint32_t)j + i;
            copiar(b, A, 8);
            for (int k = 0; k < 4; k++) b[7 - k] ^= (uint8_t)(t >> (8 * k));
            copiar(b + 8, claro + 8 * (i - 1), 8);
            aes128_decifrar(&a, b, b);
            copiar(A, b, 8);
            copiar(claro + 8 * (i - 1), b + 8, 8);
        }
    static const uint8_t iv[8] = { 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6, 0xA6 };
    bool ok = wpa_igual(A, iv, 8);
    wpa_apagar(&a, sizeof a);
    wpa_apagar(b, sizeof b);
    if (!ok) wpa_apagar(claro, 8 * m);
    return ok;
}

/* ------------------------------------------------------------ balizas */

static const uint8_t OUI_IEEE[3] = { 0x00, 0x0F, 0xAC };
static const uint8_t OUI_MS[3] = { 0x00, 0x50, 0xF2 };

static bool oui(const uint8_t *p, const uint8_t o[3]) { return p[0] == o[0] && p[1] == o[1] && p[2] == o[2]; }

static uint8_t akm_bit(uint8_t t)
{
    switch (t) {
    case 2: return WPA_AKM_PSK;
    case 6: return WPA_AKM_PSK256;
    case 8: return WPA_AKM_SAE;
    case 1: case 3: case 5: case 11: case 12: case 13: return WPA_AKM_8021X;
    default: return WPA_AKM_OUTRA;
    }
}

/* El elemento RSN (IEEE 802.11-2020, 9.4.2.24): versión, grupo, pares, AKM,
 * capacidades, PMKID y el grupo de gestión. Lo que falta vale lo de la norma. */
static void analisar_rsn(const uint8_t *p, uint32_t n, wpa_bss_t *o)
{
    if (n < 2 || p[0] != 1 || p[1] != 0) return;   /* versión 1 */
    uint32_t k = 2;
    o->cifra_grupo = WPA_CIFRA_CCMP;                /* lo que vale si el elemento se corta aquí */
    o->cifras_par = 1u << WPA_CIFRA_CCMP;
    o->psk = false;
    o->akms = WPA_AKM_8021X;                        /* sin lista de AKM vale 802.1X */
    o->cifra_gestao = WPA_CIFRA_BIP;
    if (k + 4 <= n) {
        if (oui(p + k, OUI_IEEE)) o->cifra_grupo = p[k + 3];
        else o->cifra_grupo = 0xFF;
        k += 4;
    }
    if (k + 2 <= n) {
        uint32_t c = (uint32_t)(p[k] | p[k + 1] << 8);
        k += 2;
        o->cifras_par = 0;
        for (uint32_t i = 0; i < c && k + 4 <= n; i++, k += 4)
            if (oui(p + k, OUI_IEEE) && p[k + 3] < 8) o->cifras_par |= (uint8_t)(1u << p[k + 3]);
    }
    if (k + 2 <= n) {
        uint32_t c = (uint32_t)(p[k] | p[k + 1] << 8);
        k += 2;
        o->akms = 0;
        for (uint32_t i = 0; i < c && k + 4 <= n; i++, k += 4)
            o->akms |= oui(p + k, OUI_IEEE) ? akm_bit(p[k + 3]) : WPA_AKM_OUTRA;
    }
    o->psk = (o->akms & (WPA_AKM_PSK | WPA_AKM_PSK256)) != 0;
    if (k + 2 <= n) {
        o->mfpr = (p[k] & 0x40) != 0;
        o->mfpc = (p[k] & 0x80) != 0 || o->mfpr;
        k += 2;
    }
    if (k + 2 <= n) {                               /* PMKID: se saltan */
        uint32_t c = (uint32_t)(p[k] | p[k + 1] << 8);
        k += 2 + 16 * c;
    }
    if (k + 4 <= n) o->cifra_gestao = oui(p + k, OUI_IEEE) ? p[k + 3] : 0xFF;
}

/* El elemento WPA de Microsoft (WPA1): OUI 00-50-F2, tipo 1, versión 1 y
 * suites con el mismo OUI. Lo que falta vale TKIP y 802.1X. */
static void analisar_wpa1(const uint8_t *d, uint32_t n, wpa_bss_t *o)
{
    o->wpa1 = true;
    o->wpa1_grupo = WPA_CIFRA_TKIP;
    o->wpa1_pares = 1u << WPA_CIFRA_TKIP;
    o->wpa1_psk = false;
    uint32_t k = 6;                                 /* OUI, tipo y versión */
    if (n < 6 || d[4] != 1 || d[5] != 0) return;
    if (k + 4 <= n) {
        o->wpa1_grupo = oui(d + k, OUI_MS) ? d[k + 3] : 0xFF;
        k += 4;
    }
    if (k + 2 <= n) {
        uint32_t c = (uint32_t)(d[k] | d[k + 1] << 8);
        k += 2;
        o->wpa1_pares = 0;
        for (uint32_t i = 0; i < c && k + 4 <= n; i++, k += 4)
            if (oui(d + k, OUI_MS) && d[k + 3] < 8) o->wpa1_pares |= (uint8_t)(1u << d[k + 3]);
    }
    if (k + 2 <= n) {
        uint32_t c = (uint32_t)(d[k] | d[k + 1] << 8);
        k += 2;
        for (uint32_t i = 0; i < c && k + 4 <= n; i++, k += 4)
            if (oui(d + k, OUI_MS) && d[k + 3] == 2) o->wpa1_psk = true;
    }
}

void wpa_analisar_ies(const uint8_t *ie, uint32_t n, bool privacidade, wpa_bss_t *o)
{
    zerar(o, sizeof *o);
    bool rsn = false;
    for (uint32_t i = 0; i + 2 <= n;) {
        uint8_t id = ie[i], len = ie[i + 1];
        if (i + 2 + len > n) break;
        const uint8_t *d = ie + i + 2;
        switch (id) {
        case 0:
            if (len <= 32 && !o->nssid) { copiar(o->ssid, d, len); o->nssid = len; }
            break;
        case 1: case 50:
            for (int j = 0; j < len; j++) {
                if (d[j] == (0x80 | 123)) { o->so_h2e = true; continue; }   /* selector «solo H2E» */
                if ((d[j] & 0x7F) >= 122) continue;                           /* otros selectores */
                if (o->ntasas < sizeof o->tasas) o->tasas[o->ntasas++] = d[j];
            }
            break;
        case 3:
            if (len >= 1) o->canal = d[0];
            break;
        case 45:
            o->ht = true;
            break;
        case 48:
            if (!rsn) {
                rsn = true;
                analisar_rsn(d, len, o);
                if (len + 2u <= sizeof o->rsn) { copiar(o->rsn, ie + i, len + 2u); o->nrsn = (uint8_t)(len + 2); }
            }
            break;
        case 244:                                   /* RSNXE: el bit 5 es «SAE hash-to-element» */
            if (len >= 1) o->h2e = (d[0] & 0x20) != 0;
            if (len + 2u <= sizeof o->rsnxe) { copiar(o->rsnxe, ie + i, len + 2u); o->nrsnxe = (uint8_t)(len + 2); }
            break;
        case 221:
            if (len >= 4 && oui(d, OUI_MS) && d[3] == 1 && !o->wpa1) {
                analisar_wpa1(d, len, o);
                if (len + 2u <= sizeof o->wpa1_ie) { copiar(o->wpa1_ie, ie + i, len + 2u); o->nwpa1_ie = (uint8_t)(len + 2); }
            }
            break;
        }
        i += 2u + len;
    }
    if (rsn) {
        bool sae = (o->akms & WPA_AKM_SAE) != 0;
        if (sae && o->psk) o->seguranca = WPA_SEG_WPA2_WPA3;
        else if (sae) o->seguranca = WPA_SEG_WPA3;
        else if (o->psk) o->seguranca = (o->wpa1 && o->wpa1_psk) ? WPA_SEG_WPA1_WPA2 : WPA_SEG_WPA2;
        else o->seguranca = WPA_SEG_OUTRA;
    } else if (o->wpa1) o->seguranca = WPA_SEG_WPA1;
    else if (privacidade) o->seguranca = WPA_SEG_WEP;
    else o->seguranca = WPA_SEG_ABERTA;
}

/* ------------------------------------------------------------ lo que se negocia */

static uint8_t escolher_par(uint8_t mascara)
{
    if (mascara & (1u << WPA_CIFRA_CCMP)) return WPA_CIFRA_CCMP;
    if (mascara & (1u << WPA_CIFRA_TKIP)) return WPA_CIFRA_TKIP;
    return 0;
}

static bool grupo_ok(uint8_t par, uint8_t grupo)
{
    if (grupo == WPA_CIFRA_CCMP) return par == WPA_CIFRA_CCMP;   /* el grupo no puede ser más fuerte que el par */
    return grupo == WPA_CIFRA_TKIP;
}

bool wpa_escolher(const wpa_bss_t *b, bool sae, bool psk, wpa_par_t *p)
{
    zerar(p, sizeof *p);
    if (b->nrsn && b->seguranca != WPA_SEG_OUTRA) {
        uint8_t par = escolher_par(b->cifras_par);
        bool pmf_ok = b->cifra_gestao == WPA_CIFRA_BIP;
        p->cifra_par = par;
        p->cifra_grupo = b->cifra_grupo;
        if (par && grupo_ok(par, b->cifra_grupo)) {
            /* WPA3 (o transición): SAE con 802.11w obligatoria, solo con CCMP */
            if (sae && (b->akms & WPA_AKM_SAE) && par == WPA_CIFRA_CCMP && b->mfpc && pmf_ok) {
                p->akm = 8;
                p->pmf = 2;
                return true;
            }
            if (psk && (b->akms & WPA_AKM_PSK) && (!b->mfpr || pmf_ok)) {
                p->akm = 2;
                p->pmf = b->mfpr ? 2 : 0;   /* la 802.11w solo si el punto de acceso la exige */
                return true;
            }
            if (psk && (b->akms & WPA_AKM_PSK256) && (!b->mfpc || pmf_ok)) {
                p->akm = 6;
                p->pmf = b->mfpc ? 2 : 0;
                return true;
            }
        }
    }
    if (b->wpa1 && b->wpa1_psk && psk) {
        uint8_t par = escolher_par(b->wpa1_pares);
        if (!par || !grupo_ok(par, b->wpa1_grupo)) return false;
        zerar(p, sizeof *p);
        p->wpa1 = 1;
        p->akm = 2;
        p->cifra_par = par;
        p->cifra_grupo = b->wpa1_grupo;
        return true;
    }
    zerar(p, sizeof *p);
    return false;
}

uint32_t wpa_montar_ie(const wpa_par_t *p, uint8_t *s)
{
    uint32_t k = 0;
    if (p->wpa1) {
        s[k++] = 221; s[k++] = 22;
        s[k++] = 0x00; s[k++] = 0x50; s[k++] = 0xF2; s[k++] = 1;   /* WPA */
        s[k++] = 1; s[k++] = 0;                                      /* versión 1 */
        s[k++] = 0x00; s[k++] = 0x50; s[k++] = 0xF2; s[k++] = p->cifra_grupo;
        s[k++] = 1; s[k++] = 0; s[k++] = 0x00; s[k++] = 0x50; s[k++] = 0xF2; s[k++] = p->cifra_par;
        s[k++] = 1; s[k++] = 0; s[k++] = 0x00; s[k++] = 0x50; s[k++] = 0xF2; s[k++] = 2;   /* PSK */
        return k;
    }
    s[k++] = 48; s[k++] = 20;
    s[k++] = 1; s[k++] = 0;                                          /* versión 1 */
    s[k++] = 0x00; s[k++] = 0x0F; s[k++] = 0xAC; s[k++] = p->cifra_grupo;
    s[k++] = 1; s[k++] = 0; s[k++] = 0x00; s[k++] = 0x0F; s[k++] = 0xAC; s[k++] = p->cifra_par;
    s[k++] = 1; s[k++] = 0; s[k++] = 0x00; s[k++] = 0x0F; s[k++] = 0xAC; s[k++] = p->akm;
    s[k++] = p->pmf == 2 ? 0xC0 : p->pmf == 1 ? 0x80 : 0;          /* MFPC y MFPR */
    s[k++] = 0;
    if (p->h2e) { s[k++] = 244; s[k++] = 1; s[k++] = 0x20; }       /* RSNXE: SAE hash-to-element */
    return k;
}

void wpa_rsn_ie(uint8_t s[WPA_RSN_IE_TAM])
{
    wpa_par_t p;
    zerar(&p, sizeof p);
    p.akm = 2;
    p.cifra_par = p.cifra_grupo = WPA_CIFRA_CCMP;
    wpa_montar_ie(&p, s);
}

/* ------------------------------------------------------------ saludo de cuatro pasos */

/* Campos de la trama EAPOL-Key (desde el byte de versión de 802.1X). */
#define K_TIPO    4
#define K_INFO    5
#define K_TAM     7
#define K_REPLAY  9
#define K_NONCE   17
#define K_IV      49
#define K_RSC     65
#define K_MIC     81
#define K_NDADOS  97
#define K_DADOS   99

#define KI_VERSAO     0x0007
#define KI_PAR        0x0008
#define KI_INDICE     0x0030    /* WPA1: el id de la GTK */
#define KI_INSTALAR   0x0040
#define KI_ACK        0x0080
#define KI_MIC        0x0100
#define KI_SEGURA     0x0200
#define KI_ERRO       0x0400
#define KI_PEDIDO     0x0800
#define KI_CIFRADO    0x1000

static int compara(const uint8_t *a, const uint8_t *b, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    return 0;
}

/* B = Min(AA, SPA) || Max(AA, SPA) || Min(ANonce, SNonce) || Max(ANonce, SNonce) */
static void contexto_ptk(const uint8_t aa[6], const uint8_t spa[6], const uint8_t an[32], const uint8_t sn[32],
                         uint8_t b[76])
{
    bool a_menor = compara(aa, spa, 6) < 0;
    copiar(b, a_menor ? aa : spa, 6);
    copiar(b + 6, a_menor ? spa : aa, 6);
    bool n_menor = compara(an, sn, 32) < 0;
    copiar(b + 12, n_menor ? an : sn, 32);
    copiar(b + 44, n_menor ? sn : an, 32);
}

void wpa_ptk(const uint8_t pmk[WPA_PMK_TAM], const uint8_t aa[6], const uint8_t spa[6],
             const uint8_t anonce[WPA_NONCE_TAM], const uint8_t snonce[WPA_NONCE_TAM], uint8_t ptk[WPA_PTK_TAM])
{
    uint8_t b[76];
    contexto_ptk(aa, spa, anonce, snonce, b);
    wpa_prf(pmk, WPA_PMK_TAM, "Pairwise key expansion", b, sizeof b, ptk, WPA_PTK_TAM);
}

/* La PTK de lo negociado: PRF-SHA1 (PSK, WPA1) o el KDF con SHA-256 (PSK-SHA256 y SAE). */
static void derivar_ptk(wpa_sup_t *s)
{
    uint8_t b[76];
    contexto_ptk(s->aa, s->spa, s->anonce, s->snonce, b);
    if (s->par.akm == 6 || s->par.akm == 8) kdf_sha256(s->pmk, WPA_PMK_TAM, "Pairwise key expansion", b, sizeof b, s->ptk, s->nptk);
    else wpa_prf(s->pmk, WPA_PMK_TAM, "Pairwise key expansion", b, sizeof b, s->ptk, s->nptk);
}

void wpa_sup_preparar(wpa_sup_t *s, const wpa_par_t *par, const uint8_t pmk[WPA_PMK_TAM], const uint8_t aa[6],
                      const uint8_t spa[6], const uint8_t *ap_ie, uint32_t nap_ie, const uint8_t *ap_rsnxe,
                      uint32_t nap_rsnxe, const uint8_t *pmkid, const uint8_t snonce[WPA_NONCE_TAM])
{
    wpa_apagar(s, sizeof *s);
    s->par = *par;
    copiar(s->pmk, pmk, WPA_PMK_TAM);
    copiar(s->aa, aa, 6);
    copiar(s->spa, spa, 6);
    copiar(s->snonce, snonce, WPA_NONCE_TAM);
    s->nie = (uint8_t)wpa_montar_ie(par, s->ie);
    if (nap_ie <= sizeof s->ap_rsn) { copiar(s->ap_rsn, ap_ie, nap_ie); s->nap_rsn = (uint8_t)nap_ie; }
    if (ap_rsnxe && nap_rsnxe <= sizeof s->ap_rsnxe) { copiar(s->ap_rsnxe, ap_rsnxe, nap_rsnxe); s->nap_rsnxe = (uint8_t)nap_rsnxe; }
    if (pmkid) { copiar(s->pmkid, pmkid, 16); s->tem_pmkid = true; }
    s->nptk = par->cifra_par == WPA_CIFRA_TKIP ? 64 : 48;
    /* Versión del descriptor (12.7.2): la AKM manda en SAE (0) y PSK-SHA256 (3);
     * con PSK, 1 (HMAC-MD5 y RC4) si el par es TKIP y 2 (HMAC-SHA1 y AES) si es CCMP. */
    s->versao = par->akm == 8 ? 0 : par->akm == 6 ? 3 : par->cifra_par == WPA_CIFRA_TKIP ? 1 : 2;
    s->versao_8021x = 1;
}

void wpa_sup_iniciar(wpa_sup_t *s, const uint8_t pmk[WPA_PMK_TAM], const uint8_t aa[6], const uint8_t spa[6],
                     const uint8_t *ap_rsn, uint32_t nap_rsn, const uint8_t snonce[WPA_NONCE_TAM])
{
    wpa_par_t p;
    zerar(&p, sizeof p);
    p.akm = 2;
    p.cifra_par = p.cifra_grupo = WPA_CIFRA_CCMP;
    wpa_sup_preparar(s, &p, pmk, aa, spa, ap_rsn, nap_rsn, (const uint8_t *)0, 0, (const uint8_t *)0, snonce);
}

/* MIC de una trama EAPOL-Key con el campo del MIC a cero (12.7.2): HMAC-MD5
 * (versión 1), HMAC-SHA1 cortado a 16 bytes (2) o AES-128-CMAC (3 y 0). */
static void calcular_mic(const wpa_sup_t *s, const uint8_t *q, uint32_t n, uint8_t mic[16])
{
    static uint8_t copia[WPA_EAPOL_MAX + 512];
    uint8_t h[SHA1_TAM];
    if (n > sizeof copia) n = sizeof copia;
    copiar(copia, q, n);
    zerar(copia + K_MIC, 16);
    if (s->versao == 1) hmac_md5(s->ptk, 16, copia, n, mic);
    else if (s->versao == 2) { hmac_sha1(s->ptk, 16, copia, n, h); copiar(mic, h, 16); }
    else aes_cmac(s->ptk, copia, n, mic);
    wpa_apagar(h, sizeof h);
}

/* Una respuesta: el MIC se pone al final. */
static uint32_t montar(const wpa_sup_t *s, uint16_t info, const uint8_t replay[8], const uint8_t *nonce,
                       const uint8_t *dados, uint32_t ndados, uint16_t tam_chave, uint8_t *r)
{
    zerar(r, K_DADOS + ndados);
    uint32_t corpo = K_DADOS - 4 + ndados;
    r[0] = s->versao_8021x;
    r[1] = 3;                                 /* EAPOL-Key */
    poe_be16(r + 2, (uint16_t)corpo);
    r[K_TIPO] = s->par.wpa1 ? 254 : 2;        /* descriptor WPA o RSN */
    poe_be16(r + K_INFO, info);
    poe_be16(r + K_TAM, tam_chave);
    copiar(r + K_REPLAY, replay, 8);
    if (nonce) copiar(r + K_NONCE, nonce, WPA_NONCE_TAM);
    poe_be16(r + K_NDADOS, (uint16_t)ndados);
    if (ndados) copiar(r + K_DADOS, dados, ndados);
    calcular_mic(s, r, K_DADOS + ndados, r + K_MIC);
    return K_DADOS + ndados;
}

/* Descifra los datos de la clave: RC4 con IV || KEK saltando 256 bytes
 * (versión 1) o la envoltura AES (las demás). Devuelve los bytes en claro o -1. */
static int32_t abrir_dados(const wpa_sup_t *s, const uint8_t *q, const uint8_t *d, uint32_t n, uint8_t *claro, uint32_t max)
{
    if (s->versao == 1) {
        if (n > max) return -1;
        uint8_t k[32];
        copiar(k, q + K_IV, 16);
        copiar(k + 16, s->ptk + 16, 16);
        rc4_t r;
        rc4_chave(&r, k, 32);
        rc4_pular(&r, 256);
        copiar(claro, d, n);
        rc4_aplicar(&r, claro, n);
        wpa_apagar(k, sizeof k);
        wpa_apagar(&r, sizeof r);
        return (int32_t)n;
    }
    if (n < 24 || n % 8 || n - 8 > max) return -1;
    if (!aes_desenvolver(s->ptk + 16, d, n, claro)) return -1;
    return (int32_t)(n - 8);
}

/* Datos de los mensajes 3 y de grupo del RSN: el RSN (y el RSNXE) del punto
 * de acceso y las KDE de la GTK y de la IGTK (12.7.2, figura 12-35). */
static int ler_dados(wpa_sup_t *s, const uint8_t *q, const uint8_t *d, uint32_t n, bool conferir_rsn)
{
    static uint8_t claro[512];
    int32_t m = abrir_dados(s, q, d, n, claro, sizeof claro);
    if (m < 0) return WPA_ERRO;
    bool viu_gtk = false, viu_rsn = false, viu_rsnxe = false;
    int r = 0;
    for (uint32_t i = 0; i + 2 <= (uint32_t)m;) {
        uint8_t id = claro[i], len = claro[i + 1];
        if (id == 0xDD && len == 0) break;       /* relleno */
        if (i + 2 + len > (uint32_t)m) { r = WPA_ERRO; break; }
        const uint8_t *v = claro + i + 2;
        if (id == 48 && !viu_rsn) {
            viu_rsn = true;
            if (conferir_rsn && s->nap_rsn && (len + 2u != s->nap_rsn || !wpa_igual(claro + i, s->ap_rsn, s->nap_rsn))) {
                r = WPA_ERRO;                    /* el RSN no es el de la baliza: degradación */
                break;
            }
        } else if (id == 244 && !viu_rsnxe) {
            viu_rsnxe = true;
            if (conferir_rsn && s->nap_rsnxe && (len + 2u != s->nap_rsnxe || !wpa_igual(claro + i, s->ap_rsnxe, s->nap_rsnxe))) {
                r = WPA_ERRO;                    /* el RSNXE no es el de la baliza (degradación del H2E) */
                break;
            }
        } else if (id == 0xDD && len >= 6 && oui(v, OUI_IEEE) && v[3] == 1) {
            uint32_t ng = len - 6u;
            if (ng != 16 && ng != 32) { r = WPA_ERRO; break; }
            s->gtk_id = v[4] & 3;
            copiar(s->gtk, v + 6, ng);
            s->ngtk = (uint8_t)ng;
            viu_gtk = true;
        } else if (id == 0xDD && len == 4 + 2 + 6 + 16 && oui(v, OUI_IEEE) && v[3] == 9) {
            s->igtk_id = (uint16_t)(v[4] | v[5] << 8);    /* IGTK: id, IPN y la clave */
            copiar(s->ipn, v + 6, 6);
            copiar(s->igtk, v + 12, 16);
            s->tem_igtk = true;
        }
        i += 2u + len;
    }
    wpa_apagar(claro, sizeof claro);
    if (r < 0) return r;
    if (conferir_rsn && s->nap_rsn && !viu_rsn) return WPA_ERRO;
    if (conferir_rsn && s->nap_rsnxe && !viu_rsnxe) return WPA_ERRO;
    return viu_gtk ? 0 : WPA_ERRO;
}

/* WPA1, grupo, mensaje 1: la GTK en bruto (cifrada), del tamaño del campo «longitud de la clave». */
static int ler_gtk_wpa1(wpa_sup_t *s, const uint8_t *q, uint16_t info, const uint8_t *d, uint32_t n)
{
    static uint8_t claro[64];
    uint32_t ng = le_be16(q + K_TAM);
    if (ng != 16 && ng != 32) return WPA_ERRO;
    int32_t m = abrir_dados(s, q, d, n, claro, sizeof claro);
    if (m < (int32_t)ng) { wpa_apagar(claro, sizeof claro); return WPA_ERRO; }
    copiar(s->gtk, claro, ng);
    s->ngtk = (uint8_t)ng;
    s->gtk_id = (uint8_t)((info & KI_INDICE) >> 4);
    wpa_apagar(claro, sizeof claro);
    return 0;
}

/* Datos del mensaje 1 del RSN: la KDE del PMKID (con SAE se compara). */
static void ler_pmkid(wpa_sup_t *s, const uint8_t *d, uint32_t n)
{
    for (uint32_t i = 0; i + 2 <= n;) {
        uint8_t id = d[i], len = d[i + 1];
        if (i + 2 + len > n) break;
        if (id == 0xDD && len == 20 && oui(d + i + 2, OUI_IEEE) && d[i + 5] == 4) {
            s->pmkid_visto = true;
            s->pmkid_ok = s->tem_pmkid && wpa_igual(d + i + 6, s->pmkid, 16);
        }
        i += 2u + len;
    }
}

int wpa_processar(wpa_sup_t *s, const uint8_t *q, uint32_t n, uint8_t *resp, uint32_t *nresp)
{
    *nresp = 0;
    if (n < K_DADOS || q[1] != 3) return WPA_IGNORAR;
    uint32_t total = 4u + le_be16(q + 2);
    if (total > n || total < K_DADOS) return WPA_ERRO;
    n = total;
    if (q[K_TIPO] != (s->par.wpa1 ? 254 : 2)) return WPA_IGNORAR;   /* el descriptor de lo negociado */
    uint16_t info = le_be16(q + K_INFO);
    uint32_t ndados = le_be16(q + K_NDADOS);
    if (K_DADOS + ndados > n) return WPA_ERRO;
    if ((info & KI_VERSAO) != s->versao || !(info & KI_ACK) || (info & (KI_ERRO | KI_PEDIDO))) return WPA_IGNORAR;
    const uint8_t *replay = q + K_REPLAY;
    uint8_t mic[16];
    uint16_t tam_resp = s->par.wpa1 ? le_be16(q + K_TAM) : 0;   /* WPA1 devuelve la longitud de la clave */
    s->versao_8021x = q[0];

    if ((info & KI_PAR) && !(info & KI_MIC)) {       /* mensaje 1 */
        copiar(s->anonce, q + K_NONCE, WPA_NONCE_TAM);
        s->tem_anonce = true;
        if (!s->par.wpa1) ler_pmkid(s, q + K_DADOS, ndados);
        derivar_ptk(s);
        s->ptk_pronta = true;
        *nresp = montar(s, (uint16_t)(s->versao | KI_PAR | KI_MIC), replay, s->snonce, s->ie, s->nie, tam_resp, resp);
        return WPA_RESPONDER;
    }
    if (!(info & KI_MIC) || !s->ptk_pronta) return WPA_IGNORAR;
    calcular_mic(s, q, n, mic);
    if (!wpa_igual(mic, q + K_MIC, 16)) return WPA_ERRO_MIC;
    if (s->tem_replay && compara(replay, s->replay, 8) <= 0) return WPA_ERRO;   /* repetida */

    if (info & KI_PAR) {                             /* mensaje 3 */
        if (!(info & KI_INSTALAR) || !s->tem_anonce || !wpa_igual(q + K_NONCE, s->anonce, WPA_NONCE_TAM)) return WPA_ERRO;
        uint16_t info4 = (uint16_t)(s->versao | KI_PAR | KI_MIC);
        if (s->par.wpa1) {
            /* WPA1: los datos van en claro y son el elemento WPA de la baliza; la GTK llega después */
            if (s->nap_rsn && (ndados != s->nap_rsn || !wpa_igual(q + K_DADOS, s->ap_rsn, s->nap_rsn))) return WPA_ERRO;
            s->ngtk = 0;
        } else {
            if (!(info & KI_CIFRADO)) return WPA_ERRO;
            if (ler_dados(s, q, q + K_DADOS, ndados, true) < 0) return WPA_ERRO;
            info4 |= KI_SEGURA;
        }
        copiar(s->replay, replay, 8);
        s->tem_replay = true;
        copiar(s->rsc, q + K_RSC, 8);
        *nresp = montar(s, info4, replay, (const uint8_t *)0, (const uint8_t *)0, 0, tam_resp, resp);
        return WPA_CHAVES;
    }
    /* grupo, mensaje 1: GTK nueva */
    if (!(info & KI_SEGURA)) return WPA_ERRO;
    uint16_t info2 = (uint16_t)(s->versao | KI_MIC | KI_SEGURA);
    if (s->par.wpa1) {
        if (ler_gtk_wpa1(s, q, info, q + K_DADOS, ndados) < 0) return WPA_ERRO;
        info2 |= info & KI_INDICE;
    } else {
        if (!(info & KI_CIFRADO)) return WPA_ERRO;
        if (ler_dados(s, q, q + K_DADOS, ndados, false) < 0) return WPA_ERRO;
    }
    copiar(s->replay, replay, 8);
    s->tem_replay = true;
    copiar(s->rsc, q + K_RSC, 8);
    *nresp = montar(s, info2, replay, (const uint8_t *)0, (const uint8_t *)0, 0, 0, resp);
    return WPA_GTK_NOVA;
}

uint32_t wpa_falha_mic(wpa_sup_t *s, bool par, uint8_t *resp)
{
    if (!s->ptk_pronta) return 0;
    /* el contador de las peticiones: mayor que todo lo visto */
    if (compara(s->pedido, s->replay, 8) < 0) copiar(s->pedido, s->replay, 8);
    for (int i = 7; i >= 0; i--) if (++s->pedido[i]) break;
    uint16_t info = (uint16_t)(s->versao | KI_MIC | KI_SEGURA | KI_ERRO | KI_PEDIDO | (par ? KI_PAR : 0));
    return montar(s, info, s->pedido, (const uint8_t *)0, (const uint8_t *)0, 0, 0, resp);
}
