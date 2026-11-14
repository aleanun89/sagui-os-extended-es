// SPDX-License-Identifier: GPL-2.0-or-later
/* sshcifra.c - el cifrado de paquetes chacha20-poly1305@openssh.com (D250).
 *
 * No es el AEAD de la RFC 8439: es el de PROTOCOL.chacha20poly1305 de OpenSSH.
 * La clave son 64 bytes: K_2 (los 32 primeros) cifra el cuerpo y K_1 (los 32
 * últimos) cifra los 4 bytes del tamaño, cada una con el número de secuencia
 * del paquete como nonce de 64 bits (big-endian) y contador de 64 bits. El
 * bloque 0 de K_2 da la clave del Poly1305 y el cuerpo se cifra desde el
 * bloque 1. La etiqueta cubre el tamaño cifrado y el cuerpo cifrado, y se
 * comprueba ANTES de descifrar el cuerpo.
 *
 * El ChaCha20 de la libtls es el del IETF (contador de 32 bits + nonce de 96);
 * con el nonce {0, 0, 0, 0, seq en 8 bytes} da el mismo estado inicial que el
 * original (contador de 64 bits con la mitad alta a cero), y un paquete SSH
 * nunca llega a 2^32 bloques. */
#include "cripto.h"

static void nonce_de(uint8_t nonce[12], uint32_t seq)
{
    memset(nonce, 0, 12);
    nonce[8] = (uint8_t)(seq >> 24);
    nonce[9] = (uint8_t)(seq >> 16);
    nonce[10] = (uint8_t)(seq >> 8);
    nonce[11] = (uint8_t)seq;
}

uint32_t ssh_chapoly_tamanho(const uint8_t chave[64], uint32_t seq, const uint8_t cifrado[4])
{
    uint8_t nonce[12], t[4];
    nonce_de(nonce, seq);
    chacha20_xor(chave + 32, 0, nonce, cifrado, t, 4);
    return (uint32_t)t[0] << 24 | (uint32_t)t[1] << 16 | (uint32_t)t[2] << 8 | t[3];
}

void ssh_chapoly_selar(const uint8_t chave[64], uint32_t seq, uint8_t *pkt, unsigned n)
{
    uint8_t nonce[12], bloco[64];
    nonce_de(nonce, seq);
    chacha20_xor(chave + 32, 0, nonce, pkt, pkt, 4);
    chacha20_xor(chave, 1, nonce, pkt + 4, pkt + 4, n);
    chacha20_bloco(chave, 0, nonce, bloco);
    poly1305(bloco, pkt, 4 + n, pkt + 4 + n);
    cripto_apagar(bloco, sizeof bloco);
}

bool ssh_chapoly_abrir(const uint8_t chave[64], uint32_t seq, uint8_t *pkt, unsigned n)
{
    uint8_t nonce[12], bloco[64], tag[16];
    nonce_de(nonce, seq);
    chacha20_bloco(chave, 0, nonce, bloco);
    poly1305(bloco, pkt, 4 + n, tag);
    cripto_apagar(bloco, sizeof bloco);
    if (!cripto_igual(tag, pkt + 4 + n, 16)) return false;
    chacha20_xor(chave, 1, nonce, pkt + 4, pkt + 4, n);
    return true;
}
