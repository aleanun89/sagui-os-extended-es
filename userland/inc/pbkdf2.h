// SPDX-License-Identifier: GPL-2.0-or-later
/* pbkdf2.h - contraseñas: PBKDF2-HMAC-SHA256 (RFC 8018 y RFC 2104) para la sombra (D255).
 *
 * Solo cabecera (funciones static): la usan el gestor de servicios, que es quien
 * comprueba las contraseñas, y la prueba. Quien la incluya enlaza el SHA-256 del
 * kernel (build/userland/libtls/sha256.o, como el benchmark).
 *
 * Formato en la sombra: "pbkdf2-sha256$ITERACIONES$SAL$HASH", con la sal (16
 * bytes) y el hash (32 bytes) en hexadecimal. Las iteraciones van en el texto:
 * se pueden subir sin invalidar las contraseñas que ya hay.
 *
 * Iteraciones: 20 000 son unos 0,2-0,3 s en un Pentium M de 2006 (dos bloques
 * de SHA-256 por iteración, con los estados de ipad y opad precalculados) y
 * menos de 0,05 s en un PC actual: la espera la nota quien prueba contraseñas a
 * ciegas, no quien escribe la suya. */
#ifndef SAGUI_PBKDF2_H
#define SAGUI_PBKDF2_H
#include "sagui.h"
#include "sha256.h"

#define PBKDF2_ITERACOES   20000
#define PBKDF2_ITER_MIN    1000
#define PBKDF2_ITER_MAX    1000000
#define PBKDF2_SAL         16
#define PBKDF2_PREFIXO     "pbkdf2-sha256$"
#define PBKDF2_TEXTO_MAX   160   /* el texto del hash cabe holgado */

typedef struct {
    sha256_t interno, externo;   /* con la clave ya mezclada (ipad, opad) */
} hmac_sha256_t;

static void hmac_sha256_chave(hmac_sha256_t *h, const void *chave, unsigned n)
{
    unsigned char k[SHA256_BLOCO], bloco[SHA256_BLOCO];
    memset(k, 0, sizeof k);
    if (n > SHA256_BLOCO) sha256(chave, n, k);
    else memcpy(k, chave, n);
    for (int i = 0; i < SHA256_BLOCO; i++) bloco[i] = (unsigned char)(k[i] ^ 0x36);
    sha256_iniciar(&h->interno);
    sha256_somar(&h->interno, bloco, SHA256_BLOCO);
    for (int i = 0; i < SHA256_BLOCO; i++) bloco[i] = (unsigned char)(k[i] ^ 0x5C);
    sha256_iniciar(&h->externo);
    sha256_somar(&h->externo, bloco, SHA256_BLOCO);
    memset(k, 0, sizeof k);
    memset(bloco, 0, sizeof bloco);
}

/* HMAC(clave, a || b) con la clave ya preparada en "h" (que no cambia). */
static void hmac_sha256_calc(const hmac_sha256_t *h, const void *a, unsigned na, const void *b, unsigned nb,
                             unsigned char saida[SHA256_TAM])
{
    sha256_t c = h->interno;
    unsigned char meio[SHA256_TAM];
    sha256_somar(&c, a, na);
    if (nb) sha256_somar(&c, b, nb);
    sha256_fim(&c, meio);
    c = h->externo;
    sha256_somar(&c, meio, SHA256_TAM);
    sha256_fim(&c, saida);
}

/* PBKDF2-HMAC-SHA256 con un solo bloque de salida (32 bytes: lo que se guarda). */
static void pbkdf2_sha256(const char *senha, unsigned ns, const unsigned char *sal, unsigned nsal, unsigned iter,
                          unsigned char saida[SHA256_TAM])
{
    hmac_sha256_t h;
    hmac_sha256_chave(&h, senha, ns);
    static const unsigned char um[4] = { 0, 0, 0, 1 };
    unsigned char u[SHA256_TAM];
    hmac_sha256_calc(&h, sal, nsal, um, 4, u);
    memcpy(saida, u, SHA256_TAM);
    for (unsigned i = 1; i < iter; i++) {
        hmac_sha256_calc(&h, u, SHA256_TAM, NULL, 0, u);
        for (int k = 0; k < SHA256_TAM; k++) saida[k] ^= u[k];
    }
    memset(&h, 0, sizeof h);
    memset(u, 0, sizeof u);
}

static void pbkdf2_hex(const unsigned char *b, int n, char *s)
{
    static const char d[] = "0123456789abcdef";
    for (int i = 0; i < n; i++) { s[2 * i] = d[b[i] >> 4]; s[2 * i + 1] = d[b[i] & 15]; }
    s[2 * n] = 0;
}

static int pbkdf2_de_hex(const char *s, unsigned char *b, int n)
{
    for (int i = 0; i < n; i++) {
        int v = 0;
        for (int k = 0; k < 2; k++) {
            char c = s[2 * i + k];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else return -1;
        }
        b[i] = (unsigned char)v;
    }
    return s[2 * n] == 0 || s[2 * n] == '$' ? 0 : -1;
}

/* Prepara el texto que va a la sombra para "senha", con sal nueva. 0 = bien. */
static int pbkdf2_gerar(const char *senha, char *texto, int max)
{
    unsigned char sal[PBKDF2_SAL], h[SHA256_TAM];
    if (bytes_aleatorios(sal, sizeof sal) != (int)sizeof sal) return ERRO_INVALIDO;
    pbkdf2_sha256(senha, (unsigned)tam_texto(senha), sal, sizeof sal, PBKDF2_ITERACOES, h);
    char hs[2 * PBKDF2_SAL + 1], hh[2 * SHA256_TAM + 1];
    pbkdf2_hex(sal, PBKDF2_SAL, hs);
    pbkdf2_hex(h, SHA256_TAM, hh);
    formatar_texto(texto, (size_t)max, PBKDF2_PREFIXO "%u$%s$%s", (unsigned)PBKDF2_ITERACOES, hs, hh);
    memset(h, 0, sizeof h);
    return 0;
}

/* ¿Es un texto de hash que se entiende? */
static bool pbkdf2_valido(const char *t, unsigned *iter, unsigned char sal[PBKDF2_SAL], unsigned char h[SHA256_TAM])
{
    int np = tam_texto(PBKDF2_PREFIXO);
    if (compara_texto_n(t, PBKDF2_PREFIXO, (size_t)np) != 0) return false;
    t += np;
    unsigned it = 0;
    int dig = 0;
    while (*t >= '0' && *t <= '9' && dig < 8) { it = it * 10 + (unsigned)(*t++ - '0'); dig++; }
    if (!dig || *t++ != '$' || it < PBKDF2_ITER_MIN || it > PBKDF2_ITER_MAX) return false;
    if (pbkdf2_de_hex(t, sal, PBKDF2_SAL) < 0 || t[2 * PBKDF2_SAL] != '$') return false;
    t += 2 * PBKDF2_SAL + 1;
    if (pbkdf2_de_hex(t, h, SHA256_TAM) < 0 || t[2 * SHA256_TAM] != 0) return false;
    *iter = it;
    return true;
}

/* ¿"senha" es la del texto? Comparación en tiempo constante. */
static bool pbkdf2_conferir(const char *senha, const char *texto)
{
    unsigned iter;
    unsigned char sal[PBKDF2_SAL], esperado[SHA256_TAM], h[SHA256_TAM];
    if (!pbkdf2_valido(texto, &iter, sal, esperado)) return false;
    pbkdf2_sha256(senha, (unsigned)tam_texto(senha), sal, sizeof sal, iter, h);
    unsigned dif = 0;
    for (int i = 0; i < SHA256_TAM; i++) dif |= (unsigned)(h[i] ^ esperado[i]);
    memset(h, 0, sizeof h);
    return dif == 0;
}

#endif
