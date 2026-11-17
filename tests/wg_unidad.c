// SPDX-License-Identifier: GPL-3.0-only
/* wg_unidad.c - prueba en el anfitrión de la criptografía y el protocolo WireGuard
 * de SaguiOS (D363). La compila y la maneja tests/h365_wireguard.py.
 *
 *   wg_unidad vectores           lee órdenes por la entrada y escribe el resultado en hex:
 *                                  blake2s TAM CLAVE|- MENSAJE, hmac CLAVE MENSAJE,
 *                                  kdf N CLAVE ENTRADA, x25519 ESCALAR PUNTO,
 *                                  chapoly CLAVE NONCE AAD TEXTO, xchapoly CLAVE NONCE AAD TEXTO,
 *                                  hchacha CLAVE NONCE, b64 CLAVE (ida y vuelta), pub PRIVADA
 *   wg_unidad par PRIV PUB PUERTO_REF [PSK|-] [corto]
 *                                el motor de wireguard.c como par de 10.66.0.2, con un socket UDP
 *                                en 127.0.0.1 hacia el par de referencia; órdenes por la entrada:
 *                                  ping SEQ (eco a 10.66.0.1), saludar, estado, salir
 *                                y por la salida lo que pasa (ECO, SALUDO, ESTADO ...).
 * Las funciones de la libsagui que usa la libtls se ponen aquí (SAGUI_HOST). */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <time.h>
#include "cripto.h"
#include "wireguard.h"

int bytes_aleatorios(void *buf, unsigned n)
{
    static int fd = -1;
    if (fd < 0) fd = open("/dev/urandom", O_RDONLY);
    return (int)read(fd, buf, n);
}
int imprimir_em(int fd, const char *fmt, ...) { (void)fd; (void)fmt; return 0; }
void sair(int codigo) { exit(codigo); }
size_t tam_texto(const char *s) { return strlen(s); }

static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static unsigned hex(const char *s, uint8_t *b)
{
    unsigned n = 0;
    if (!strcmp(s, "-")) return 0;
    while (s[0] && s[1]) { b[n++] = (uint8_t)((unsigned)(hexv(s[0]) & 15) << 4 | (unsigned)(hexv(s[1]) & 15)); s += 2; }
    return n;
}
static void ph(const uint8_t *b, unsigned n) { for (unsigned i = 0; i < n; i++) printf("%02x", b[i]); printf("\n"); }

static int vectores(void)
{
    static char l[70000], a[5][20000];
    static uint8_t x[5][10000], o[20000];
    while (fgets(l, sizeof l, stdin)) {
        int k = sscanf(l, "%19999s %19999s %19999s %19999s %19999s", a[0], a[1], a[2], a[3], a[4]);
        if (k < 1) continue;
        unsigned n1 = k > 1 ? hex(a[1], x[1]) : 0, n2 = k > 2 ? hex(a[2], x[2]) : 0;
        unsigned n3 = k > 3 ? hex(a[3], x[3]) : 0, n4 = k > 4 ? hex(a[4], x[4]) : 0;
        if (!strcmp(a[0], "blake2s")) {
            unsigned tam = (unsigned)atoi(a[1]);
            blake2s(o, tam, x[2], n2, x[3], n3);
            ph(o, tam);
        } else if (!strcmp(a[0], "blake2s_trozos")) {   /* el mismo hash, dando los datos de 1, 63, 64, 65... bytes */
            blake2s_t c;
            blake2s_iniciar_chave(&c, 32, x[1], n1);
            unsigned i = 0, passo = 1;
            while (i < n2) { unsigned q = n2 - i < passo ? n2 - i : passo; blake2s_somar(&c, x[2] + i, q); i += q; passo = passo * 3 + 1; }
            blake2s_fim(&c, o);
            ph(o, 32);
        } else if (!strcmp(a[0], "hmac")) {
            hmac(&HASH_BLAKE2S, x[1], n1, x[2], n2, o);
            ph(o, 32);
        } else if (!strcmp(a[0], "kdf")) {
            uint8_t prk[32];
            unsigned qtd = (unsigned)atoi(a[1]);
            hkdf_extrair(&HASH_BLAKE2S, x[2], n2, x[3], n3, prk);
            hkdf_expandir(&HASH_BLAKE2S, prk, "", 0, o, 32 * qtd);
            ph(o, 32 * qtd);
        } else if (!strcmp(a[0], "x25519")) {
            x25519(o, x[1], x[2]);
            ph(o, 32);
        } else if (!strcmp(a[0], "chapoly")) {
            chachapoly_cifrar(x[1], x[2], x[3], n3, x[4], o, n4, o + n4);
            ph(o, n4 + 16);
            uint8_t v[10000];
            if (!chachapoly_decifrar(x[1], x[2], x[3], n3, o, v, n4, o + n4) || memcmp(v, x[4], n4)) printf("ERRO volta\n");
            o[0] ^= 1;
            if (n4 && chachapoly_decifrar(x[1], x[2], x[3], n3, o, v, n4, o + n4)) printf("ERRO adulterado\n");
        } else if (!strcmp(a[0], "xchapoly")) {
            xchachapoly_cifrar(x[1], x[2], x[3], n3, x[4], o, n4, o + n4);
            ph(o, n4 + 16);
            uint8_t v[10000];
            if (!xchachapoly_decifrar(x[1], x[2], x[3], n3, o, v, n4, o + n4) || memcmp(v, x[4], n4)) printf("ERRO volta\n");
            o[n4] ^= 0x80;
            if (xchachapoly_decifrar(x[1], x[2], x[3], n3, o, v, n4, o + n4)) printf("ERRO adulterado\n");
        } else if (!strcmp(a[0], "hchacha")) {
            hchacha20(x[1], x[2], o);
            ph(o, 32);
        } else if (!strcmp(a[0], "b64")) {
            char s[45];
            uint8_t v[32];
            wg_base64_escrever(x[1], s);
            printf("%s %s\n", s, wg_base64_ler(s, v) && !memcmp(v, x[1], 32) ? "ok" : "ERRO");
        } else if (!strcmp(a[0], "b64ler")) {   /* una clave en base64 que tiene que fallar o no */
            uint8_t v[32];
            printf("%s\n", wg_base64_ler(a[1], v) ? "valida" : "invalida");
        } else if (!strcmp(a[0], "pub")) {
            wg_publica_de(x[1], o);
            ph(o, 32);
        } else if (!strcmp(a[0], "prefixo")) {
            wg_prefixo_t p;
            if (wg_prefixo_ler(a[1], &p)) printf("%08x/%u\n", p.ip, p.bits); else printf("invalido\n");
        } else printf("ERRO orden %s\n", a[0]);
        fflush(stdout);
    }
    return 0;
}

/* ------------------------------------------------------------ el par */

static int sock;
static wg_t w;
static uint32_t meu_ip = 0x0A420002u;   /* 10.66.0.2 */

static uint32_t agora_ms(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint32_t)(t.tv_sec * 1000 + t.tv_nsec / 1000000);
}

static void enviar_udp(void *ctx, uint32_t ip, uint16_t porta, const uint8_t *d, unsigned n)
{
    (void)ctx;
    struct sockaddr_in a = { 0 };
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(ip);
    a.sin_port = htons(porta);
    sendto(sock, d, n, 0, (struct sockaddr *)&a, sizeof a);
}

static uint16_t soma16(const uint8_t *p, unsigned n)
{
    uint32_t s = 0;
    for (unsigned i = 0; i + 1 < n; i += 2) s += (uint32_t)p[i] << 8 | p[i + 1];
    if (n & 1) s += (uint32_t)p[n - 1] << 8;
    while (s >> 16) s = (s & 0xFFFF) + (s >> 16);
    return (uint16_t)~s;
}

static void ip_montar(uint8_t *p, uint32_t de, uint32_t para, uint8_t proto, unsigned carga)
{
    memset(p, 0, 20);
    p[0] = 0x45; p[2] = (uint8_t)((20 + carga) >> 8); p[3] = (uint8_t)(20 + carga);
    p[6] = 0x40; p[8] = 64; p[9] = proto;
    for (int i = 0; i < 4; i++) { p[12 + i] = (uint8_t)(de >> (24 - 8 * i)); p[16 + i] = (uint8_t)(para >> (24 - 8 * i)); }
    uint16_t s = soma16(p, 20);
    p[10] = (uint8_t)(s >> 8); p[11] = (uint8_t)s;
}

static void icmp_eco(uint32_t para, uint8_t tipo, uint16_t id, uint16_t seq, const uint8_t *dados, unsigned n)
{
    uint8_t p[1500];
    uint8_t *c = p + 20;
    c[0] = tipo; c[1] = 0; c[2] = c[3] = 0;
    c[4] = (uint8_t)(id >> 8); c[5] = (uint8_t)id; c[6] = (uint8_t)(seq >> 8); c[7] = (uint8_t)seq;
    memcpy(c + 8, dados, n);
    uint16_t s = soma16(c, 8 + n);
    c[2] = (uint8_t)(s >> 8); c[3] = (uint8_t)s;
    ip_montar(p, meu_ip, para, 1, 8 + n);
    wg_enviar_pacote(&w, p, 28 + n, agora_ms());
}

static void entregar(void *ctx, const uint8_t *p, unsigned n)
{
    (void)ctx;
    uint32_t de = (uint32_t)p[12] << 24 | (uint32_t)p[13] << 16 | (uint32_t)p[14] << 8 | p[15];
    const uint8_t *c = p + (p[0] & 15) * 4;
    unsigned nc = n - (unsigned)(p[0] & 15) * 4;
    if (p[9] == 1 && c[0] == 0) printf("ECO %u\n", (unsigned)(c[6] << 8 | c[7]));
    else if (p[9] == 1 && c[0] == 8) {   /* nos hacen ping: contestamos */
        printf("PING %u\n", (unsigned)(c[6] << 8 | c[7]));
        icmp_eco(de, 0, (uint16_t)(c[4] << 8 | c[5]), (uint16_t)(c[6] << 8 | c[7]), c + 8, nc - 8);
    } else printf("ENTREGUE %u %u\n", p[9], n);
    fflush(stdout);
}

static void tai64n(void *ctx, uint8_t ts[12])
{
    (void)ctx;
    struct timespec t;
    clock_gettime(CLOCK_REALTIME, &t);
    uint64_t s = 0x400000000000000AULL + (uint64_t)t.tv_sec;
    for (int i = 0; i < 8; i++) ts[i] = (uint8_t)(s >> (56 - 8 * i));
    uint32_t ns = (uint32_t)t.tv_nsec;
    for (int i = 0; i < 4; i++) ts[8 + i] = (uint8_t)(ns >> (24 - 8 * i));
}

static uint32_t hora_unix(void *ctx) { (void)ctx; return (uint32_t)time(NULL); }

static int par(int argc, char **argv)
{
    uint8_t priv[32], pub[32], psk[32];
    if (argc < 5 || !wg_base64_ler(argv[2], priv) || !wg_base64_ler(argv[3], pub)) { fprintf(stderr, "uso\n"); return 2; }
    bool tem_psk = argc > 5 && strcmp(argv[5], "-") && wg_base64_ler(argv[5], psk);
    wg_iniciar(&w, priv);
    w.enviar_udp = enviar_udp; w.entregar = entregar; w.tai64n = tai64n; w.hora_unix = hora_unix;
    if (argc > 6 && !strcmp(argv[6], "corto")) {   /* rekey en 3 s, como en la prueba de QEMU */
        w.t.rekey_apos_ms = 3000; w.t.rejeitar_apos_ms = 8000; w.t.keepalive_ms = 1000; w.t.rekey_timeout_ms = 1000;
    }
    wg_par_t *p = wg_par_novo(&w, pub, tem_psk ? psk : NULL);
    if (!p) { fprintf(stderr, "par\n"); return 2; }
    wg_prefixo_ler("10.66.0.0/24", &p->ips[p->nips++]);
    p->endpoint_ip = 0x7F000001u;
    p->endpoint_porta = (uint16_t)atoi(argv[4]);
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = { 0 };
    a.sin_family = AF_INET;
    a.sin_addr.s_addr = htonl(0x7F000001u);
    bind(sock, (struct sockaddr *)&a, sizeof a);
    socklen_t la = sizeof a;
    getsockname(sock, (struct sockaddr *)&a, &la);
    printf("PORTA %u\n", ntohs(a.sin_port));
    fflush(stdout);
    char linha[256];
    uint32_t hs_visto = 0;
    for (;;) {
        fd_set f;
        FD_ZERO(&f);
        FD_SET(0, &f);
        FD_SET(sock, &f);
        uint32_t prox = wg_tique(&w, agora_ms());
        if (p->atual.valida && p->atual.indice_local != hs_visto) {   /* una sesión nueva en uso */
            hs_visto = p->atual.indice_local;
            printf("SALUDO %s\n", p->atual.iniciador ? "iniciador" : "respondedor");
            fflush(stdout);
        }
        struct timeval tv = { 0, (prox > 100 ? 100 : prox) * 1000 };
        if (select(sock + 1, &f, NULL, NULL, &tv) < 0) return 1;
        if (FD_ISSET(sock, &f)) {
            static uint8_t d[2048];
            struct sockaddr_in de;
            socklen_t ld = sizeof de;
            ssize_t n = recvfrom(sock, d, sizeof d, 0, (struct sockaddr *)&de, &ld);
            if (n > 0) wg_udp_chegou(&w, ntohl(de.sin_addr.s_addr), ntohs(de.sin_port), d, (unsigned)n, agora_ms());
        }
        if (FD_ISSET(0, &f)) {
            if (!fgets(linha, sizeof linha, stdin)) break;
            unsigned seq;
            if (sscanf(linha, "ping %u", &seq) == 1) icmp_eco(0x0A420001u, 8, 0x5347, (uint16_t)seq, (const uint8_t *)"sagui", 5);
            else if (!strncmp(linha, "saludar", 7)) wg_iniciar_saudacao(&w, p, agora_ms());
            else if (!strncmp(linha, "estado", 6)) {
                printf("ESTADO hs=%u rx=%llu tx=%llu repeticion=%u mac=%u origen=%u otros=%u cookies=%u envio=%llu\n",
                       p->hs_completo_unix ? 1 : 0, (unsigned long long)p->rx_bytes, (unsigned long long)p->tx_bytes,
                       w.descartes_repeticao, w.descartes_mac, w.descartes_origem, w.descartes_outros, w.cookies_recebidas,
                       (unsigned long long)p->atual.contador_envio);
                fflush(stdout);
            } else if (!strncmp(linha, "salir", 5)) break;
        }
    }
    wg_apagar(&w);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "vectores")) return vectores();
    if (argc >= 2 && !strcmp(argv[1], "par")) return par(argc, argv);
    fprintf(stderr, "uso: wg_unidad vectores | par ...\n");
    return 2;
}
