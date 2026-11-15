// SPDX-License-Identifier: GPL-2.0-or-later
/* ssh_transporte.h - la capa de transporte de SSH-2 (RFC 4253) que comparten el
 * servidor (sshd.c, D250) y el cliente (ssh.c, D317), sin tocar el sistema:
 *
 *   - montar un paquete binario (tamaño, relleno, carga) y cifrarlo con
 *     chacha20-poly1305@openssh.com;
 *   - sacar un paquete de un búfer de entrada, con todas las comprobaciones de
 *     tamaño y relleno, y la etiqueta (MAC) comprobada ANTES de mirar el
 *     contenido;
 *   - el resumen del intercambio de claves curve25519-sha256 (H) y la
 *     derivación de las claves de cifrado.
 *
 * Todo trabaja sobre búferes que pasa quien llama; nada lee ni escribe en
 * sockets. Se prueba en el anfitrión (tests/ssh_unidad.c y
 * tests/ssh_cliente_unidad.c), también con paquetes malformados. */
#ifndef SSH_TRANSPORTE_H
#define SSH_TRANSPORTE_H
#include "sagui.h"
#include "cripto.h"
#include "sshd_logica.h"

/* Resultados de ssh_pacote_extrair. */
#define SSH_PKT_FALTA     0    /* aún no ha llegado entero */
#define SSH_PKT_OK        1
#define SSH_PKT_TAMANHO  -1    /* tamaño imposible (o manipulado, si va cifrado) */
#define SSH_PKT_MAC      -2    /* la etiqueta no coincide */
#define SSH_PKT_RELLENO  -3    /* relleno de menos de 4 bytes o más largo que el paquete */

/* Cuánto relleno lleva una carga de n bytes (entre 4 y 11; el total, múltiplo
 * de 8 sin contar el tamaño si va cifrado con chacha20-poly1305). */
static inline unsigned ssh_pacote_relleno(unsigned n, bool cifrado)
{
    unsigned base = cifrado ? 1 + n : 5 + n;
    unsigned pad = 8 - base % 8;
    if (pad < 4) pad += 8;
    return pad;
}

/* Monta en "tx" el paquete con la carga "pl" (n bytes) y el relleno "aleat"
 * (ssh_pacote_relleno(n) bytes al azar) y, con "chave" (64 bytes), lo cifra
 * con el número de secuencia "seq". Devuelve los bytes que hay que mandar, o
 * 0 si no cabe en "max". */
static inline unsigned ssh_pacote_montar(uint8_t *tx, unsigned max, const uint8_t *pl, unsigned n,
                                         const uint8_t *aleat, const uint8_t *chave, uint32_t seq)
{
    unsigned pad = ssh_pacote_relleno(n, chave != NULL);
    unsigned tam = 1 + n + pad;
    unsigned total = 4 + tam + (chave ? 16 : 0);
    if (n > max || total > max) return 0;
    tx[0] = (uint8_t)(tam >> 24); tx[1] = (uint8_t)(tam >> 16); tx[2] = (uint8_t)(tam >> 8); tx[3] = (uint8_t)tam;
    tx[4] = (uint8_t)pad;
    memmove(tx + 5, pl, n);
    memcpy(tx + 5 + n, aleat, pad);
    if (chave) ssh_chapoly_selar(chave, seq, tx, tam);
    return total;
}

/* Saca un paquete de los "tem" bytes de "p" (descifrándolo en su sitio si hay
 * "chave"). "tam_ok"/"tam" guardan el tamaño ya descifrado entre llamadas,
 * para no descifrarlo dos veces mientras falta el resto: empiezan en false y
 * quien llama no los toca. Con SSH_PKT_OK, la carga queda en *pl (*pn bytes,
 * al menos 1: el tipo del mensaje) y *usados dice cuánto ocupaba el paquete
 * en el búfer. "max" es el tamaño máximo admitido (sin contar los 4 del
 * tamaño). Un resultado negativo es una conexión que hay que cortar. */
static inline int ssh_pacote_extrair(uint8_t *p, unsigned tem, const uint8_t *chave, uint32_t seq, unsigned max,
                                     bool *tam_ok, uint32_t *tam_c, const uint8_t **pl, unsigned *pn, unsigned *usados)
{
    if (tem < 4) return SSH_PKT_FALTA;
    uint32_t tam;
    if (chave) {
        if (!*tam_ok) { *tam_c = ssh_chapoly_tamanho(chave, seq, p); *tam_ok = true; }
        tam = *tam_c;
    } else tam = (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    if (tam < 8 || tam > max || (chave ? tam : tam + 4) % 8 || (!chave && tam < 12)) return SSH_PKT_TAMANHO;
    unsigned total = 4 + tam + (chave ? 16 : 0);
    if (tem < total) return SSH_PKT_FALTA;
    if (chave && !ssh_chapoly_abrir(chave, seq, p, tam)) return SSH_PKT_MAC;
    *tam_ok = false;
    unsigned pad = p[4];
    if (pad < 4 || pad + 1 >= tam) return SSH_PKT_RELLENO;
    *pl = p + 5;
    *pn = tam - pad - 1;
    *usados = total;
    return SSH_PKT_OK;
}

static inline void ssh_h_texto(sha256_t *h, const void *p, unsigned n)
{
    uint8_t t[4] = { (uint8_t)(n >> 24), (uint8_t)(n >> 16), (uint8_t)(n >> 8), (uint8_t)n };
    sha256_somar(h, t, 4);
    sha256_somar(h, p, n);
}

/* H = SHA-256(V_C, V_S, I_C, I_S, K_S, Q_C, Q_S, K)   (RFC 5656, 4 / RFC 8731).
 * Las versiones sin el «\r\n»; K ya como mpint (con su tamaño delante). */
static inline void ssh_hash_troca(uint8_t hh[32], const char *v_c, const char *v_s,
                                  const uint8_t *i_c, unsigned n_ic, const uint8_t *i_s, unsigned n_is,
                                  const uint8_t *k_s, unsigned n_ks, const uint8_t q_c[32], const uint8_t q_s[32],
                                  const uint8_t *k_mp, unsigned n_k)
{
    sha256_t h;
    unsigned a = 0, b = 0;
    while (v_c[a]) a++;
    while (v_s[b]) b++;
    sha256_iniciar(&h);
    ssh_h_texto(&h, v_c, a);
    ssh_h_texto(&h, v_s, b);
    ssh_h_texto(&h, i_c, n_ic);
    ssh_h_texto(&h, i_s, n_is);
    ssh_h_texto(&h, k_s, n_ks);
    ssh_h_texto(&h, q_c, 32);
    ssh_h_texto(&h, q_s, 32);
    sha256_somar(&h, k_mp, n_k);
    sha256_fim(&h, hh);
}

/* Claves de 64 bytes (RFC 4253, 7.2): HASH(K || H || letra || id) y su prolongación. */
static inline void ssh_derivar(uint8_t saida[64], const uint8_t *k_mp, unsigned nk, const uint8_t hh[32], char letra,
                               const uint8_t sessao_id[32])
{
    sha256_t h;
    sha256_iniciar(&h);
    sha256_somar(&h, k_mp, nk);
    sha256_somar(&h, hh, 32);
    sha256_somar(&h, &letra, 1);
    sha256_somar(&h, sessao_id, 32);
    sha256_fim(&h, saida);
    sha256_iniciar(&h);
    sha256_somar(&h, k_mp, nk);
    sha256_somar(&h, hh, 32);
    sha256_somar(&h, saida, 32);
    sha256_fim(&h, saida + 32);
}

/* La firma ssh-ed25519 de un ECDH_REPLY o de una petición publickey:
 * string "ssh-ed25519", string firma(64), y nada más. */
static inline bool ssh_firma_ler(const uint8_t *f, unsigned n, uint8_t assin[64])
{
    ssh_leitor_t l;
    unsigned nt, na;
    sl_iniciar(&l, f, n);
    const uint8_t *t = sl_texto(&l, &nt);
    const uint8_t *a = sl_texto(&l, &na);
    if (l.erro || l.pos != n || !sl_igual(t, nt, SSH_ED25519) || na != 64) return false;
    memcpy(assin, a, 64);
    return true;
}

#endif
