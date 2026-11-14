// SPDX-License-Identifier: GPL-2.0-or-later
/* ssh_unidad.c - pruebas en el anfitrión del SSH (D250):
 * Ed25519 con los vectores de la RFC 8032 (sección 7.1) y el cifrado de
 * paquetes chacha20-poly1305@openssh.com. La lanza tests/h248_ssh.py, que
 * además compara con una implementación independiente (la biblioteca
 * «cryptography» de Python, si está) usando el modo de órdenes:
 *
 *   ssh_unidad                      los vectores fijos
 *   ssh_unidad ordenes              lee órdenes de la entrada, una por línea:
 *       publica SEMILLA                     -> clave pública
 *       firmar SEMILLA MENSAJE              -> firma
 *       verificar PUBLICA FIRMA MENSAJE     -> 1 o 0
 *       selar CLAVE64 SEQ CUERPO            -> tamaño cifrado + cuerpo cifrado + etiqueta
 *       abrir CLAVE64 SEQ PAQUETE           -> tamaño y cuerpo en claro, o "mal"
 *   (todo en hexadecimal; "-" = vacío)
 *
 * También prueba la lógica del servidor que no toca el sistema
 * (userland/programas/sshd_logica.h).
 *
 * Compilar: gcc -DSAGUI_HOST -Iuserland/inc -Ikernel/inc -Iuserland/programas tests/ssh_unidad.c
 *   userland/libtls/{ed25519,hash,chacha,sshcifra,x25519}.c kernel/lib/sha256.c */
#include <stdio.h>
#include <stdlib.h>
#include "cripto.h"
#include "sshd_logica.h"

/* Lo que la libtls pide a la libsagui y aquí no hace falta de verdad. */
int bytes_aleatorios(void *buf, unsigned n) { memset(buf, 0x5a, n); return (int)n; }
int imprimir_em(int fd, const char *fmt, ...) { (void)fd; (void)fmt; return 0; }
void sair(int codigo) { exit(codigo); }
size_t tam_texto(const char *s) { return strlen(s); }

static int total, falhas;

static unsigned de_hex(const char *h, uint8_t *b, unsigned max)
{
    unsigned n = 0;
    if (h[0] == '-') return 0;
    while (h[0] && h[1] && n < max) {
        unsigned v;
        sscanf(h, "%2x", &v);
        b[n++] = (uint8_t)v;
        h += 2;
    }
    return n;
}

static void mostrar_hex(const uint8_t *b, unsigned n)
{
    for (unsigned i = 0; i < n; i++) printf("%02x", b[i]);
    printf("\n");
}

static void conferir(const char *nome, bool ok)
{
    total++;
    if (!ok) { falhas++; printf("FALLA: %s\n", nome); }
}

typedef struct { const char *semente, *publica, *msg, *assin; } vetor_t;

/* RFC 8032, 7.1: TEST 1, TEST 2, TEST 3 y TEST SHA(abc). */
static const vetor_t vetores[] = {
    { "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
      "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "-",
      "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
      "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b" },
    { "4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
      "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
      "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da"
      "085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00" },
    { "c5aa8df43f9f837bedb7442f31dcb7b166d38535076f094b85ce3a2e0b4458f7",
      "fc51cd8e6218a1a38da47ed00230f0580816ed13ba3303ac5deb911548908025", "af82",
      "6291d657deec24024827e69c3abe01a30ce548a284743a445e3680d7db5ac3ac"
      "18ff9b538d16f290ae67f760984dc6594a7c15e9716ed28dc027beceea1ec40a" },
    { "833fe62409237b9d62ec77587520911e9a759cec1d19755b7da901b96dca3d42",
      "ec172b93ad5e563bf4932c70e1245034c35467ef2efd4d64ebf819683467e2bf",
      "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
      "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f",
      "dc2a4459e7369633a52b1bf277839a00201009a3efbf3ecb69bea2186c26b589"
      "09351fc9ac90b3ecfdfbc7c66431e0303dca179c138ac17ad9bef1177331a704" },
};

static void testar_vetores(void)
{
    for (unsigned i = 0; i < sizeof vetores / sizeof *vetores; i++) {
        const vetor_t *v = &vetores[i];
        uint8_t sem[32], pub[32], pub2[32], msg[128], sig[64], sig2[64];
        char nome[64];
        de_hex(v->semente, sem, 32);
        de_hex(v->publica, pub, 32);
        unsigned n = de_hex(v->msg, msg, sizeof msg);
        de_hex(v->assin, sig, 64);
        ed25519_publica(pub2, sem);
        snprintf(nome, sizeof nome, "RFC 8032 vector %u: clave publica", i + 1);
        conferir(nome, memcmp(pub, pub2, 32) == 0);
        ed25519_assinar(sig2, msg, n, sem, pub2);
        snprintf(nome, sizeof nome, "RFC 8032 vector %u: firma", i + 1);
        conferir(nome, memcmp(sig, sig2, 64) == 0);
        snprintf(nome, sizeof nome, "RFC 8032 vector %u: la firma se verifica", i + 1);
        conferir(nome, ed25519_verificar(sig, msg, n, pub));
        for (int bit = 0; bit < 512; bit += 37) {      /* un bit cambiado en la firma */
            memcpy(sig2, sig, 64);
            sig2[bit >> 3] ^= (uint8_t)(1 << (bit & 7));
            snprintf(nome, sizeof nome, "vector %u: firma con el bit %d cambiado", i + 1, bit);
            conferir(nome, !ed25519_verificar(sig2, msg, n, pub));
        }
        msg[0] ^= 1;                                   /* otro mensaje */
        snprintf(nome, sizeof nome, "vector %u: otro mensaje", i + 1);
        conferir(nome, !ed25519_verificar(sig, msg, n ? n : 1, pub));
        memcpy(pub2, pub, 32);                         /* otra clave */
        pub2[5] ^= 4;
        msg[0] ^= 1;
        snprintf(nome, sizeof nome, "vector %u: otra clave", i + 1);
        conferir(nome, !ed25519_verificar(sig, msg, n, pub2));
    }
    /* S >= L (firma maleable): S + L verifica la ecuación pero debe rechazarse. */
    {
        static const uint8_t L[32] = { 0xed, 0xd3, 0xf5, 0x5c, 0x1a, 0x63, 0x12, 0x58, 0xd6, 0x9c, 0xf7, 0xa2,
                                       0xde, 0xf9, 0xde, 0x14, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x10 };
        uint8_t pub[32], sig[64];
        de_hex(vetores[0].publica, pub, 32);
        de_hex(vetores[0].assin, sig, 64);
        unsigned vai = 0;
        for (int i = 0; i < 32; i++) { vai += sig[32 + i] + L[i]; sig[32 + i] = (uint8_t)vai; vai >>= 8; }
        conferir("S + L rechazada (maleabilidad)", !ed25519_verificar(sig, "", 0, pub));
    }
}

static void testar_chapoly(void)
{
    uint8_t chave[64], pkt[4 + 256 + 16], orig[4 + 256];
    for (int i = 0; i < 64; i++) chave[i] = (uint8_t)(i * 7 + 3);
    for (unsigned n = 8; n <= 256; n += 8) {
        uint32_t seq = n * 2654435761u;
        pkt[0] = 0; pkt[1] = 0; pkt[2] = (uint8_t)(n >> 8); pkt[3] = (uint8_t)n;
        for (unsigned i = 0; i < n; i++) pkt[4 + i] = (uint8_t)(i ^ n);
        memcpy(orig, pkt, 4 + n);
        ssh_chapoly_selar(chave, seq, pkt, n);
        conferir("chapoly: el cifrado cambia los datos", memcmp(orig, pkt, 4 + n) != 0);
        conferir("chapoly: el tamano se recupera", ssh_chapoly_tamanho(chave, seq, pkt) == n);
        uint8_t copia[sizeof pkt];
        memcpy(copia, pkt, 4 + n + 16);
        copia[4 + n / 2] ^= 0x10;
        conferir("chapoly: un bit cambiado se rechaza", !ssh_chapoly_abrir(chave, seq, copia, n));
        memcpy(copia, pkt, 4 + n + 16);
        conferir("chapoly: otra secuencia se rechaza", !ssh_chapoly_abrir(chave, seq + 1, copia, n));
        memcpy(copia, pkt, 4 + n + 16);
        copia[0] ^= 1;
        conferir("chapoly: el tamano cifrado tambien va en la etiqueta", !ssh_chapoly_abrir(chave, seq, copia, n));
        conferir("chapoly: se abre", ssh_chapoly_abrir(chave, seq, pkt, n));
        conferir("chapoly: ida y vuelta", memcmp(orig + 4, pkt + 4, n) == 0);
    }
}

static int ordenes(void)
{
    static char linha[70000], a[4][33000];
    static uint8_t b0[16500], b1[16500], b2[16500];
    while (fgets(linha, sizeof linha, stdin)) {
        int k = sscanf(linha, "%32999s %32999s %32999s %32999s", a[0], a[1], a[2], a[3]);
        if (k < 2) continue;
        if (!strcmp(a[0], "publica")) {
            uint8_t pub[32];
            de_hex(a[1], b0, 32);
            ed25519_publica(pub, b0);
            mostrar_hex(pub, 32);
        } else if (!strcmp(a[0], "firmar") && k >= 3) {
            uint8_t pub[32], sig[64];
            de_hex(a[1], b0, 32);
            unsigned n = de_hex(a[2], b1, sizeof b1);
            ed25519_publica(pub, b0);
            ed25519_assinar(sig, b1, n, b0, pub);
            mostrar_hex(sig, 64);
        } else if (!strcmp(a[0], "verificar") && k >= 4) {
            de_hex(a[1], b0, 32);
            de_hex(a[2], b1, 64);
            unsigned n = de_hex(a[3], b2, sizeof b2);
            printf("%d\n", ed25519_verificar(b1, b2, n, b0) ? 1 : 0);
        } else if (!strcmp(a[0], "selar") && k >= 4) {
            de_hex(a[1], b0, 64);
            uint32_t seq = (uint32_t)strtoul(a[2], NULL, 10);
            unsigned n = de_hex(a[3], b1 + 4, sizeof b1 - 20);
            b1[0] = (uint8_t)(n >> 24); b1[1] = (uint8_t)(n >> 16); b1[2] = (uint8_t)(n >> 8); b1[3] = (uint8_t)n;
            ssh_chapoly_selar(b0, seq, b1, n);
            mostrar_hex(b1, 4 + n + 16);
        } else if (!strcmp(a[0], "abrir") && k >= 4) {
            de_hex(a[1], b0, 64);
            uint32_t seq = (uint32_t)strtoul(a[2], NULL, 10);
            unsigned n = de_hex(a[3], b1, sizeof b1);
            if (n < 20) { printf("mal\n"); continue; }
            uint32_t t = ssh_chapoly_tamanho(b0, seq, b1);
            if (t != n - 20 || !ssh_chapoly_abrir(b0, seq, b1, t)) { printf("mal\n"); continue; }
            printf("%u ", t);
            mostrar_hex(b1 + 4, t);
        }
        fflush(stdout);
    }
    return 0;
}

/* ------------------------------------------------------------ la lógica del servidor (sshd_logica.h) */

#define IP(a, b, c, d) ((uint32_t)(a) << 24 | (b) << 16 | (c) << 8 | (d))

static void testar_logica(void)
{
    /* campos: lectura con límites */
    {
        static const uint8_t p[] = { 0, 0, 0, 3, 'a', 'b', 'c', 7, 0, 0, 1, 0, 0, 0, 0, 9, 'x' };
        ssh_leitor_t l;
        unsigned n;
        sl_iniciar(&l, p, sizeof p);
        const uint8_t *t = sl_texto(&l, &n);
        conferir("leer string", n == 3 && !memcmp(t, "abc", 3) && sl_igual(t, n, "abc") && !sl_igual(t, n, "abcd"));
        conferir("leer byte y uint32", sl_u8(&l) == 7 && sl_u32(&l) == 256 && !l.erro);
        sl_texto(&l, &n);   /* dice 9 bytes y queda 1 */
        conferir("string mas larga que el paquete: error, sin leer fuera", l.erro && n == 0);
        conferir("tras un error todo sale vacio", sl_u32(&l) == 0 && sl_u8(&l) == 0 && l.erro);
        static const uint8_t g[] = { 0xff, 0xff, 0xff, 0xff, 1, 2 };
        sl_iniciar(&l, g, sizeof g);
        sl_texto(&l, &n);
        conferir("tamano 0xFFFFFFFF: error (sin desbordar)", l.erro && n == 0);
        static const uint8_t z[] = { 0, 0, 0, 3, 'a', 0, 'c' };
        char c[8];
        sl_iniciar(&l, z, sizeof z);
        sl_cstr(&l, c, sizeof c);
        conferir("texto con un cero dentro: rechazado", l.erro && c[0] == 0);
        sl_iniciar(&l, p, sizeof p);
        sl_cstr(&l, c, 3);
        conferir("texto que no cabe: rechazado", l.erro);
    }
    /* escritura y mpint */
    {
        uint8_t b[16];
        ssh_escritor_t e;
        static const uint8_t v1[] = { 0, 0, 0x7f, 1 }, v2[] = { 0x80, 1 }, v3[] = { 0, 0, 0 };
        se_iniciar(&e, b, sizeof b);
        se_mpint(&e, v1, 4);
        conferir("mpint sin ceros a la izquierda", e.n == 6 && !memcmp(b, "\0\0\0\2\x7f\1", 6));
        se_iniciar(&e, b, sizeof b);
        se_mpint(&e, v2, 2);
        conferir("mpint con el bit alto: un cero delante", e.n == 7 && !memcmp(b, "\0\0\0\3\0\x80\1", 7));
        se_iniciar(&e, b, sizeof b);
        se_mpint(&e, v3, 3);
        conferir("mpint de cero: vacio", e.n == 4 && !memcmp(b, "\0\0\0\0", 4));
        se_iniciar(&e, b, 6);
        se_cstr(&e, "abc");
        conferir("escribir lo que no cabe: error", e.erro);
    }
    /* listas de algoritmos */
    {
        const char *cl = "aes128-ctr,chacha20-poly1305@openssh.com,aes256-gcm@openssh.com";
        const uint8_t *c = (const uint8_t *)cl;
        unsigned n = (unsigned)strlen(cl);
        char s[48];
        conferir("lista: tiene", ssh_lista_tem(c, n, "aes128-ctr") && ssh_lista_tem(c, n, "aes256-gcm@openssh.com"));
        conferir("lista: no vale un prefijo ni un trozo", !ssh_lista_tem(c, n, "aes128") && !ssh_lista_tem(c, n, "chacha20-poly1305"));
        conferir("lista vacia", !ssh_lista_tem(c, 0, "aes128-ctr"));
        conferir("escoger: el primero del cliente que tenga el servidor",
                 ssh_escolher(c, n, "aes256-gcm@openssh.com,chacha20-poly1305@openssh.com", s, sizeof s) &&
                 !strcmp(s, "chacha20-poly1305@openssh.com"));
        conferir("escoger: ninguno en comun", !ssh_escolher(c, n, "3des-cbc", s, sizeof s) && s[0] == 0);
        conferir("primero de la lista", ssh_primeiro_eh(c, n, "aes128-ctr") && !ssh_primeiro_eh(c, n, "aes128") &&
                 !ssh_primeiro_eh(c, n, "chacha20-poly1305@openssh.com"));
    }
    /* authorized_keys */
    {
        uint8_t pub[32], esperada[32], blob[SSH_BLOB_ED25519];
        char com[16], linha[300], b64[100];
        for (int i = 0; i < 32; i++) esperada[i] = (uint8_t)(i * 3 + 1);
        ssh_blob_montar(blob, esperada);
        b64_codificar(blob, sizeof blob, b64, sizeof b64);
        snprintf(linha, sizeof linha, "ssh-ed25519 %s ana@pc con espacios\n", b64);
        conferir("authorized_keys: linea buena", ssh_linha_autorizada(linha, pub, com, sizeof com) == 1 &&
                 !memcmp(pub, esperada, 32) && !strcmp(com, "ana@pc con espa"));
        snprintf(linha, sizeof linha, "  ssh-ed25519\t%s", b64);
        conferir("authorized_keys: sin comentario, con tabulador", ssh_linha_autorizada(linha, pub, com, sizeof com) == 1 && !com[0]);
        conferir("authorized_keys: comentario y vacia", ssh_linha_autorizada("# hola", pub, NULL, 0) == 0 &&
                 ssh_linha_autorizada("", pub, NULL, 0) == 0 && ssh_linha_autorizada("   \n", pub, NULL, 0) == 0);
        conferir("authorized_keys: ssh-rsa se ignora", ssh_linha_autorizada("ssh-rsa AAAAB3NzaC1yc2E= x", pub, NULL, 0) == 0);
        snprintf(linha, sizeof linha, "command=\"ls\" ssh-ed25519 %s x", b64);
        conferir("authorized_keys: con opciones delante se rechaza", ssh_linha_autorizada(linha, pub, NULL, 0) == -1);
        conferir("authorized_keys: base64 roto", ssh_linha_autorizada("ssh-ed25519 AAAA!!!! x", pub, NULL, 0) == -1);
        b64[20] = b64[20] == 'A' ? 'B' : 'A';   /* estropea el nombre del tipo de dentro */
        snprintf(linha, sizeof linha, "ssh-ed25519 %s", b64);
        conferir("authorized_keys: blob que no es ed25519", ssh_linha_autorizada(linha, pub, NULL, 0) == -1);
        conferir("authorized_keys: clave corta", ssh_linha_autorizada("ssh-ed25519 AAAAC3NzaC1lZDI1NTE5AAAAIA==", pub, NULL, 0) == -1);
        conferir("authorized_keys: solo el tipo", ssh_linha_autorizada("ssh-ed25519", pub, NULL, 0) == 0 &&
                 ssh_linha_autorizada("ssh-ed25519 ", pub, NULL, 0) == -1);
    }
    /* red local */
    {
        uint32_t eu = IP(88, 10, 20, 5), masc = IP(255, 255, 255, 0);
        conferir("red local: 10/8, 172.16/12, 192.168/16, 169.254/16, 127/8",
                 ssh_ip_local(IP(10, 0, 2, 2), eu, masc) && ssh_ip_local(IP(172, 16, 0, 1), eu, masc) &&
                 ssh_ip_local(IP(172, 31, 255, 254), eu, masc) && ssh_ip_local(IP(192, 168, 1, 40), eu, masc) &&
                 ssh_ip_local(IP(169, 254, 3, 3), eu, masc) && ssh_ip_local(IP(127, 0, 0, 1), eu, masc));
        conferir("red local: las publicas no", !ssh_ip_local(IP(8, 8, 8, 8), eu, masc) && !ssh_ip_local(IP(172, 15, 0, 1), eu, masc) &&
                 !ssh_ip_local(IP(172, 32, 0, 1), eu, masc) && !ssh_ip_local(IP(192, 169, 0, 1), eu, masc) &&
                 !ssh_ip_local(IP(11, 0, 0, 1), eu, masc) && !ssh_ip_local(IP(88, 10, 21, 5), eu, masc));
        conferir("red local: la propia subred si", ssh_ip_local(IP(88, 10, 20, 77), eu, masc));
        conferir("red local: sin direccion o mascara, la subred no cuenta",
                 !ssh_ip_local(IP(88, 10, 20, 77), 0, 0) && !ssh_ip_local(IP(88, 10, 20, 77), eu, 0) &&
                 !ssh_ip_local(IP(8, 8, 8, 8), IP(8, 8, 8, 9), 0));
    }
    /* redirección de puertos (D253): a dónde se deja abrir un túnel */
    {
        uint32_t eu = IP(192, 168, 1, 50), masc = IP(255, 255, 255, 0), ip = 0;
        conferir("tunel: localhost y 127.x, de serie",
                 ssh_destino_tunel("localhost", SSH_RED_LOCAL, eu, masc, &ip) == 0 && ip == IP(127, 0, 0, 1) &&
                 ssh_destino_tunel("LocalHost", SSH_RED_LOCAL, eu, masc, &ip) == 0 && ip == IP(127, 0, 0, 1) &&
                 ssh_destino_tunel("127.0.0.1", SSH_RED_LOCAL, eu, masc, &ip) == 0 && ip == IP(127, 0, 0, 1) &&
                 ssh_destino_tunel("127.1.2.3", SSH_RED_LOCAL, eu, masc, &ip) == 0 && ip == IP(127, 1, 2, 3));
        ip = 7;
        conferir("tunel: de serie, ni la red local ni la propia direccion ni Internet",
                 ssh_destino_tunel("192.168.1.1", SSH_RED_LOCAL, eu, masc, &ip) == -1 &&
                 ssh_destino_tunel("192.168.1.50", SSH_RED_LOCAL, eu, masc, &ip) == -1 &&
                 ssh_destino_tunel("10.0.2.2", SSH_RED_LOCAL, eu, masc, &ip) == -1 &&
                 ssh_destino_tunel("8.8.8.8", SSH_RED_LOCAL, eu, masc, &ip) == -1 && ip == 7);
        conferir("tunel: redirigir=no, nada",
                 ssh_destino_tunel("localhost", SSH_RED_NO, eu, masc, &ip) == -1 &&
                 ssh_destino_tunel("127.0.0.1", SSH_RED_NO, eu, masc, &ip) == -1 && ip == 7);
        conferir("tunel: red-local, las privadas y la subred si; Internet no",
                 ssh_destino_tunel("192.168.1.1", SSH_RED_REDE, eu, masc, &ip) == 0 && ip == IP(192, 168, 1, 1) &&
                 ssh_destino_tunel("10.0.2.2", SSH_RED_REDE, eu, masc, &ip) == 0 && ip == IP(10, 0, 2, 2) &&
                 ssh_destino_tunel("localhost", SSH_RED_REDE, eu, masc, &ip) == 0 && ip == IP(127, 0, 0, 1) &&
                 ssh_destino_tunel("8.8.8.8", SSH_RED_REDE, eu, masc, &ip) == -1 &&
                 ssh_destino_tunel("1.1.1.1", SSH_RED_REDE, eu, masc, &ip) == -1);
        conferir("tunel: ni 0.0.0.0, ni difusion, ni multidifusion",
                 ssh_destino_tunel("0.0.0.0", SSH_RED_REDE, eu, masc, &ip) == -1 &&
                 ssh_destino_tunel("255.255.255.255", SSH_RED_REDE, eu, masc, &ip) == -1 &&
                 ssh_destino_tunel("192.168.1.255", SSH_RED_REDE, eu, masc, &ip) == -1 &&
                 ssh_destino_tunel("224.0.0.1", SSH_RED_REDE, eu, masc, &ip) == -1);
        conferir("tunel: los nombres no se resuelven, ni IPv6, ni numeros raros",
                 ssh_destino_tunel("ejemplo.com", SSH_RED_REDE, eu, masc, &ip) == -2 &&
                 ssh_destino_tunel("localhost.ejemplo.com", SSH_RED_LOCAL, eu, masc, &ip) == -2 &&
                 ssh_destino_tunel("localhos", SSH_RED_LOCAL, eu, masc, &ip) == -2 &&
                 ssh_destino_tunel("::1", SSH_RED_LOCAL, eu, masc, &ip) == -2 &&
                 ssh_destino_tunel("", SSH_RED_LOCAL, eu, masc, &ip) == -2 &&
                 ssh_destino_tunel("127.1", SSH_RED_LOCAL, eu, masc, &ip) == -2 &&
                 ssh_destino_tunel("0x7f.0.0.1", SSH_RED_LOCAL, eu, masc, &ip) == -2 &&
                 ssh_destino_tunel("0177.0.0.1", SSH_RED_LOCAL, eu, masc, &ip) == -2 &&
                 ssh_destino_tunel("127.0.0.1 ", SSH_RED_LOCAL, eu, masc, &ip) == -2 &&
                 ssh_destino_tunel("127.0.0.256", SSH_RED_LOCAL, eu, masc, &ip) == -2 &&
                 ssh_destino_tunel("127.0.0.1.5", SSH_RED_LOCAL, eu, masc, &ip) == -2 &&
                 ssh_destino_tunel("2130706433", SSH_RED_LOCAL, eu, masc, &ip) == -2);
        conferir("tunel: valores de redirigir",
                 ssh_modo_redirigir("no") == SSH_RED_NO && ssh_modo_redirigir("local") == SSH_RED_LOCAL &&
                 ssh_modo_redirigir("red-local") == SSH_RED_REDE && ssh_modo_redirigir("si") == -1 &&
                 ssh_modo_redirigir("") == -1 && ssh_modo_redirigir("locales") == -1);
    }
    /* teclas del cliente -> PTY */
    {
        ssh_tecla_t t;
        uint8_t d[64];
        memset(&t, 0, sizeof t);
        unsigned n = ssh_teclas(&t, (const uint8_t *)"ls\r", 3, d);
        conferir("teclas: Intro (CR) -> \\n", n == 3 && !memcmp(d, "ls\n", 3));
        n = ssh_teclas(&t, (const uint8_t *)"\n", 1, d);
        conferir("teclas: el LF de un CR LF no cuenta dos veces", n == 0);
        n = ssh_teclas(&t, (const uint8_t *)"a\nb", 3, d);
        conferir("teclas: LF solo -> \\n", n == 3 && !memcmp(d, "a\nb", 3));
        n = ssh_teclas(&t, (const uint8_t *)"\x1b[A\x1b[D\x1bOC\x1b[3~\x1b[H\x1b[5~\x1b[1;5C", 26, d);
        conferir("teclas: flechas, Supr, Inicio, RePag (xterm -> ESC + letra)",
                 n == 14 && !memcmp(d, "\x1b" "A\x1b" "D\x1b" "C\x1b" "X\x1b" "H\x1b" "P\x1b" "C", 14));
        n = ssh_teclas(&t, (const uint8_t *)"\x1b", 1, d);
        unsigned n2 = ssh_teclas(&t, (const uint8_t *)"[", 1, d + n);
        unsigned n3 = ssh_teclas(&t, (const uint8_t *)"B", 1, d + n + n2);
        conferir("teclas: secuencia partida en tres lecturas", n == 0 && n2 == 0 && n3 == 2 && d[0] == 27 && d[1] == 'B');
        n = ssh_teclas(&t, (const uint8_t *)"\xc3\xb1\x7f\xe2\x82\xac" "z", 7, d);
        conferir("teclas: UTF-8 -> Latin-1, borrar, y lo que no existe en Latin-1",
                 n == 4 && d[0] == 0xf1 && d[1] == '\b' && d[2] == '?' && d[3] == 'z');
        n = ssh_teclas(&t, (const uint8_t *)"\xc3", 1, d);
        n2 = ssh_teclas(&t, (const uint8_t *)"\xa9", 1, d);
        conferir("teclas: caracter UTF-8 partido", n == 0 && n2 == 1 && d[0] == 0xe9);
        n = ssh_teclas(&t, (const uint8_t *)"\x1bq\x03", 3, d);
        conferir("teclas: ESC suelto y Ctrl+C pasan", n == 3 && d[0] == 27 && d[1] == 'q' && d[2] == 3);
    }
    /* PTY -> terminal del cliente, y conversiones */
    {
        uint8_t d[64];
        unsigned n = ssh_tela((const uint8_t *)"a\xf1o\n\b", 5, d);
        conferir("pantalla: Latin-1 -> UTF-8, \\n -> CR LF, retroceso que borra",
                 n == 9 && !memcmp(d, "a\xc3\xb1o\r\n\b \b", 9));
        n = ssh_para_utf8((const uint8_t *)"\xe1\n", 2, d);
        conferir("solo Latin-1 -> UTF-8", n == 3 && !memcmp(d, "\xc3\xa1\n", 3));
        n = ssh_de_utf8((const uint8_t *)"ni\xc3\xb1o \xe2\x82\xac \xc3", 11, d);
        conferir("UTF-8 -> Latin-1 con lo que no cabe y un final roto", n == 8 && !memcmp(d, "ni\xf1o ? ?", 8));
    }
    /* rutas del SFTP */
    {
        char d[64];
        conferir("ruta relativa: desde /home", ssh_caminho("/home", "a.txt", d, sizeof d) && !strcmp(d, "/home/a.txt"));
        conferir("ruta .: /home", ssh_caminho("/home", ".", d, sizeof d) && !strcmp(d, "/home"));
        conferir("ruta absoluta", ssh_caminho("/home", "/logs/x.log", d, sizeof d) && !strcmp(d, "/logs/x.log"));
        conferir("ruta con .., . y barras dobles", ssh_caminho("/home", "/a//b/./c/../d/", d, sizeof d) && !strcmp(d, "/a/b/d"));
        conferir("ruta: .. por encima de la raiz se queda en /", ssh_caminho("/home", "../../../..", d, sizeof d) && !strcmp(d, "/"));
        conferir("ruta: ../etc", ssh_caminho("/home", "../etc/ssh", d, sizeof d) && !strcmp(d, "/etc/ssh"));
        conferir("ruta que no cabe: rechazada", !ssh_caminho("/home", "/abcdefghij/abcdefghij", d, 12));
        char larga[700];
        memset(larga, 'a', sizeof larga - 1);
        larga[sizeof larga - 1] = 0;
        conferir("ruta enorme: rechazada", !ssh_caminho("/home", larga, d, sizeof d));
    }
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "ordenes")) return ordenes();
    testar_vetores();
    testar_chapoly();
    testar_logica();
    printf("ssh_unidad: %d/%d comprobaciones correctas\n", total - falhas, total);
    return falhas ? 1 : 0;
}
