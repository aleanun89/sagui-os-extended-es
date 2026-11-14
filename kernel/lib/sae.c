// SPDX-License-Identifier: GPL-2.0-or-later
/* sae.c - SAE de WPA3-Personal con P-256 (D220). Ver sae.h.
 *
 * Escrito a partir de IEEE 802.11-2020, 12.4.4 (PWE por cazar y picotear y
 * por hash-to-element), 12.4.5 (Commit y Confirm) y 12.4.5.4 (KCK, PMK y
 * PMKID). Se comprueba con los vectores del anexo J.10 de la norma (los mismos
 * que usa hostapd en sus pruebas de módulo) en tests/h215_wifi.py. */
#include "sae.h"
#include "wpa_cifra.h"

static const uint8_t PRIMO[32] = {
    0xff,0xff,0xff,0xff,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff };

static void copiar(void *d, const void *o, uint32_t n)
{
    uint8_t *a = d;
    const uint8_t *b = o;
    while (n--) *a++ = *b++;
}

/* MAX(a, b) || MIN(a, b) */
static void macs_ordenadas(const uint8_t a[6], const uint8_t b[6], uint8_t m[12])
{
    int c = 0;
    for (int i = 0; i < 6 && !c; i++) if (a[i] != b[i]) c = a[i] > b[i] ? 1 : -1;
    copiar(m, c >= 0 ? a : b, 6);
    copiar(m + 6, c >= 0 ? b : a, 6);
}

bool sae_pwe_hp(const uint8_t *senha, uint32_t nsenha, const uint8_t mac_a[6], const uint8_t mac_b[6],
                uint8_t pwe[SAE_PONTO_TAM])
{
    uint8_t macs[12], semente[32], valor[32];
    macs_ordenadas(mac_a, mac_b, macs);
    p256_ponto_t achado, cand;
    bool tem = false;
    /* Siempre 40 vueltas aunque salga antes (tiempo que no depende de la
     * contraseña), y más solo si no salió ninguno (probabilidad 2^-40). */
    for (unsigned contador = 1; contador <= 200 && (contador <= 40 || !tem); contador++) {
        uint8_t c = (uint8_t)contador;
        const uint8_t *p[2] = { senha, &c };
        uint32_t n[2] = { nsenha, 1 };
        hmac_sha256_partes(macs, 12, 2, p, n, semente);
        kdf_sha256(semente, 32, "SAE Hunting and Pecking", PRIMO, 32, valor, 32);
        if (p256_de_x(&cand, valor, semente[31] & 1) && !tem) {
            achado = cand;
            tem = true;
        }
    }
    wpa_apagar(semente, sizeof semente);
    wpa_apagar(valor, sizeof valor);
    if (!tem) return false;
    bool ok = p256_gravar(&achado, pwe);
    wpa_apagar(&achado, sizeof achado);
    wpa_apagar(&cand, sizeof cand);
    return ok;
}

bool sae_pt(const uint8_t *ssid, uint32_t nssid, const uint8_t *senha, uint32_t nsenha, const uint8_t *id,
            uint32_t nid, uint8_t pt[SAE_PONTO_TAM])
{
    static uint8_t ikm[256 + 64];
    if (nsenha > 256 || nid > 64) return false;
    copiar(ikm, senha, nsenha);
    if (nid) copiar(ikm + nsenha, id, nid);
    uint8_t semente[32], u[48];
    hkdf_extrair(ssid, nssid, ikm, nsenha + nid, semente);
    wpa_apagar(ikm, sizeof ikm);
    p256_ponto_t p1, p2;
    static const char r1[] = "SAE Hash to Element u1 P1", r2[] = "SAE Hash to Element u2 P2";
    hkdf_expandir(semente, (const uint8_t *)r1, sizeof r1 - 1, u, sizeof u);
    p256_sswu(&p1, u, sizeof u);
    hkdf_expandir(semente, (const uint8_t *)r2, sizeof r2 - 1, u, sizeof u);
    p256_sswu(&p2, u, sizeof u);
    p256_somar(&p1, &p1, &p2);
    bool ok = p256_gravar(&p1, pt);
    wpa_apagar(semente, sizeof semente);
    wpa_apagar(u, sizeof u);
    wpa_apagar(&p1, sizeof p1);
    wpa_apagar(&p2, sizeof p2);
    return ok;
}

bool sae_pwe_de_pt(const uint8_t pt[SAE_PONTO_TAM], const uint8_t mac_a[6], const uint8_t mac_b[6],
                   uint8_t pwe[SAE_PONTO_TAM])
{
    p256_ponto_t p;
    if (!p256_ler(&p, pt)) return false;
    uint8_t macs[12], val[32];
    macs_ordenadas(mac_a, mac_b, macs);
    hkdf_extrair((const uint8_t *)0, 0, macs, 12, val);   /* H(0^32, MAX || MIN) */
    p256_mod_n1_mais1(val, val);
    p256_mult(&p, &p, val);
    bool ok = p256_gravar(&p, pwe);
    wpa_apagar(&p, sizeof p);
    wpa_apagar(val, sizeof val);
    return ok;
}

bool sae_commit(sae_t *s, const uint8_t pwe[SAE_PONTO_TAM], const uint8_t rand[32], const uint8_t mask[32])
{
    if (!p256_ler(&s->pwe, pwe)) return false;
    if (!p256_escalar_valido(rand) || !p256_escalar_valido(mask)) return false;
    copiar(s->rand, rand, 32);
    copiar(s->mask, mask, 32);
    p256_somar_n(s->escalar, rand, mask);
    if (!p256_escalar_valido(s->escalar)) return false;
    p256_ponto_t e;
    p256_mult(&e, &s->pwe, mask);
    p256_negar(&e, &e);
    bool ok = p256_gravar(&e, s->elemento);
    wpa_apagar(&e, sizeof e);
    return ok;
}

int sae_processar_commit(sae_t *s, const uint8_t escalar[32], const uint8_t elemento[SAE_PONTO_TAM])
{
    if (!p256_escalar_valido(escalar)) return SAE_ERRO_ESCALAR;
    p256_ponto_t e, k;
    if (!p256_ler(&e, elemento)) return SAE_ERRO_ELEMENTO;
    if (wpa_igual(escalar, s->escalar, 32) && wpa_igual(elemento, s->elemento, SAE_PONTO_TAM)) return SAE_ERRO_REFLEXO;
    copiar(s->escalar_par, escalar, 32);
    copiar(s->elemento_par, elemento, SAE_PONTO_TAM);
    /* K = rand · (escalar_par · PWE + elemento_par); k = x(K) */
    p256_mult(&k, &s->pwe, escalar);
    p256_somar(&k, &k, &e);
    p256_mult(&k, &k, s->rand);
    uint8_t kxy[64];
    bool ok = p256_gravar(&k, kxy);
    wpa_apagar(&k, sizeof k);
    if (!ok) return SAE_ERRO_CHAVE;
    /* keyseed = H(0^32, k); KCK || PMK = KDF-512(keyseed, "SAE KCK and PMK", (escalar + escalar_par) mod r) */
    uint8_t semente[32], ctx[32], kp[64];
    hkdf_extrair((const uint8_t *)0, 0, kxy, 32, semente);
    p256_somar_n(ctx, s->escalar, escalar);
    kdf_sha256(semente, 32, "SAE KCK and PMK", ctx, 32, kp, 64);
    copiar(s->kck, kp, 32);
    copiar(s->pmk, kp + 32, 32);
    copiar(s->pmkid, ctx, 16);
    wpa_apagar(kxy, sizeof kxy);
    wpa_apagar(semente, sizeof semente);
    wpa_apagar(kp, sizeof kp);
    return 0;
}

static void confirm(const uint8_t kck[32], uint16_t enviado, const uint8_t *e1, const uint8_t *p1, const uint8_t *e2,
                    const uint8_t *p2, uint8_t saida[32])
{
    uint8_t sc[2] = { (uint8_t)enviado, (uint8_t)(enviado >> 8) };
    const uint8_t *p[5] = { sc, e1, p1, e2, p2 };
    uint32_t n[5] = { 2, 32, SAE_PONTO_TAM, 32, SAE_PONTO_TAM };
    hmac_sha256_partes(kck, 32, 5, p, n, saida);
}

void sae_confirmar(const sae_t *s, uint16_t enviado, uint8_t c[32])
{
    confirm(s->kck, enviado, s->escalar, s->elemento, s->escalar_par, s->elemento_par, c);
}

bool sae_verificar(const sae_t *s, uint16_t enviado_par, const uint8_t c[32])
{
    uint8_t esperado[32];
    confirm(s->kck, enviado_par, s->escalar_par, s->elemento_par, s->escalar, s->elemento, esperado);
    return wpa_igual(esperado, c, 32);
}
