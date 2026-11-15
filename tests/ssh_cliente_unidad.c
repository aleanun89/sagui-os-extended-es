// SPDX-License-Identifier: GPL-2.0-or-later
/* ssh_cliente_unidad.c - pruebas en el anfitrión del cliente ssh (D317):
 *
 *   - el transporte común (ssh_transporte.h): paquetes buenos, cortados,
 *     de tamaño imposible, con relleno malo, con la etiqueta cambiada, y miles
 *     de paquetes al azar (lo que mandaría un servidor hostil): nunca se sale
 *     del búfer ni se acepta lo que no vale;
 *   - los mensajes del servidor con estructura (KEXINIT, ECDH_REPLY), enteros,
 *     cortados en cada byte, con bytes de más y al azar;
 *   - known_hosts (nombres, patrones, puertos, @revoked, clave cambiada);
 *   - las claves privadas de OpenSSH (ida y vuelta, dañadas, cifradas, al azar);
 *   - «~.», las teclas del Terminal hacia xterm y el UTF-8 hacia el Terminal.
 *
 * La lanza tests/h317_ssh_cliente.py (también con -fsanitize=address,undefined).
 *
 *   ssh_cliente_unidad                    las comprobaciones
 *   ssh_cliente_unidad escribir SEM COM   una clave privada (semilla en hex) en la salida
 *   ssh_cliente_unidad leer ARCHIVO       la clave pública (hex) y el comentario de una privada
 *
 * Compilar: gcc -DSAGUI_HOST -Iuserland/inc -Ikernel/inc -Iuserland/programas tests/ssh_cliente_unidad.c
 *   userland/libtls/{ed25519,hash,chacha,sshcifra,x25519}.c kernel/lib/sha256.c */
#include <stdio.h>
#include <stdlib.h>
#include "cripto.h"
#include "sshd_logica.h"
#include "ssh_transporte.h"
#include "ssh_cliente_logica.h"

int bytes_aleatorios(void *buf, unsigned n)
{
    unsigned char *b = buf;
    for (unsigned i = 0; i < n; i++) b[i] = (unsigned char)rand();
    return (int)n;
}
int imprimir_em(int fd, const char *fmt, ...) { (void)fd; (void)fmt; return 0; }
void sair(int codigo) { exit(codigo); }
size_t tam_texto(const char *s) { return strlen(s); }

#define PKT_MAX_PRUEBA 35000u

static int total, falhas;

static void conferir(const char *nome, bool ok)
{
    total++;
    if (!ok) { falhas++; printf("FALLA: %s\n", nome); }
}

static uint32_t aleat(void) { return (uint32_t)rand() << 16 ^ (uint32_t)rand(); }

/* ------------------------------------------------------------ paquetes */

static void testar_pacotes(void)
{
    static uint8_t tx[40000], pl[33000], copia[40000];
    uint8_t chave[64], pad[16];
    for (int i = 0; i < 64; i++) chave[i] = (uint8_t)(i * 7 + 1);
    memset(pad, 0xAB, sizeof pad);

    for (int cif = 0; cif < 2; cif++) {
        const uint8_t *k = cif ? chave : NULL;
        for (unsigned n = 1; n < 300; n += 7) {
            for (unsigned i = 0; i < n; i++) pl[i] = (uint8_t)(i ^ n);
            unsigned t = ssh_pacote_montar(tx, sizeof tx, pl, n, pad, k, 77 + n);
            memcpy(copia, tx, t);
            bool tam_ok = false;
            uint32_t tam = 0;
            const uint8_t *p;
            unsigned pn, us;
            /* todos los cortes: falta (nunca otra cosa) */
            bool sempre_falta = true;
            for (unsigned c = 0; c < t; c++) {
                memcpy(tx, copia, t);
                tam_ok = false;
                if (ssh_pacote_extrair(tx, c, k, 77 + n, PKT_MAX_PRUEBA, &tam_ok, &tam, &p, &pn, &us) != SSH_PKT_FALTA) sempre_falta = false;
            }
            conferir("paquete cortado: siempre «falta»", sempre_falta);
            memcpy(tx, copia, t);
            tam_ok = false;
            int r = ssh_pacote_extrair(tx, t, k, 77 + n, PKT_MAX_PRUEBA, &tam_ok, &tam, &p, &pn, &us);
            conferir("paquete entero: ida y vuelta", r == SSH_PKT_OK && pn == n && us == t && memcmp(p, pl, n) == 0);
            if (cif) {
                /* cada bit cambiado: se rechaza (etiqueta o tamaño), nunca se acepta */
                bool ok = true;
                for (unsigned b = 0; b < t * 8; b += 13) {
                    memcpy(tx, copia, t);
                    tx[b / 8] ^= (uint8_t)(1 << (b % 8));
                    tam_ok = false;
                    int r2 = ssh_pacote_extrair(tx, t, k, 77 + n, PKT_MAX_PRUEBA, &tam_ok, &tam, &p, &pn, &us);
                    if (r2 == SSH_PKT_OK) ok = false;
                }
                conferir("un bit cambiado en un paquete cifrado nunca se acepta", ok);
                memcpy(tx, copia, t);
                tam_ok = false;
                r = ssh_pacote_extrair(tx, t, k, 78 + n, PKT_MAX_PRUEBA, &tam_ok, &tam, &p, &pn, &us);
                conferir("otro número de secuencia (paquete repetido) se rechaza", r != SSH_PKT_OK);
            }
        }
    }
    /* tamaños imposibles y rellenos malos, sin cifrar */
    struct { uint32_t tam; uint8_t pad; int esperado; const char *nome; } casos[] = {
        { 0, 4, SSH_PKT_TAMANHO, "tamaño 0" },
        { 4, 4, SSH_PKT_TAMANHO, "tamaño 4" },
        { 0x7FFFFFFC, 4, SSH_PKT_TAMANHO, "tamaño 2 GB" },
        { 0xFFFFFFFC, 4, SSH_PKT_TAMANHO, "tamaño 4 GB" },
        { PKT_MAX_PRUEBA + 4, 4, SSH_PKT_TAMANHO, "tamaño por encima del máximo" },
        { 13, 4, SSH_PKT_TAMANHO, "tamaño no múltiplo de 8" },
        { 12, 3, SSH_PKT_RELLENO, "relleno de 3 bytes" },
        { 12, 11, SSH_PKT_RELLENO, "relleno tan largo como el paquete" },
        { 12, 255, SSH_PKT_RELLENO, "relleno de 255 bytes en un paquete de 12" },
    };
    for (unsigned i = 0; i < sizeof casos / sizeof *casos; i++) {
        memset(tx, 0, 64);
        tx[0] = (uint8_t)(casos[i].tam >> 24); tx[1] = (uint8_t)(casos[i].tam >> 16);
        tx[2] = (uint8_t)(casos[i].tam >> 8); tx[3] = (uint8_t)casos[i].tam;
        tx[4] = casos[i].pad;
        bool tam_ok = false;
        uint32_t tam = 0;
        const uint8_t *p;
        unsigned pn, us;
        int r = ssh_pacote_extrair(tx, 64, NULL, 0, PKT_MAX_PRUEBA, &tam_ok, &tam, &p, &pn, &us);
        char nome[120];
        snprintf(nome, sizeof nome, "paquete malformado: %s", casos[i].nome);
        conferir(nome, r == casos[i].esperado);
    }
    /* al azar (un servidor hostil): nunca fuera del búfer; si se acepta, dentro */
    bool ok = true;
    for (int i = 0; i < 20000; i++) {
        unsigned n = aleat() % 600;
        uint8_t *b = malloc(n ? n : 1);
        for (unsigned j = 0; j < n; j++) b[j] = (uint8_t)aleat();
        if (n >= 4 && i % 3 == 0) { b[0] = 0; b[1] = 0; b[2] = (uint8_t)(aleat() % 3); }   /* tamaños plausibles */
        bool tam_ok = false;
        uint32_t tam = 0;
        const uint8_t *p;
        unsigned pn, us;
        int r = ssh_pacote_extrair(b, n, (i & 1) ? chave : NULL, (uint32_t)i, PKT_MAX_PRUEBA, &tam_ok, &tam, &p, &pn, &us);
        if (r == SSH_PKT_OK && (us > n || p < b || p + pn > b + n || pn < 1)) ok = false;
        if (r == SSH_PKT_OK && (i & 1)) ok = false;   /* con cifrado, el azar no puede tener la etiqueta buena */
        free(b);
    }
    conferir("20000 paquetes al azar: nada fuera del búfer y ninguno cifrado aceptado", ok);
}

/* ------------------------------------------------------------ KEXINIT y ECDH_REPLY */

static unsigned kexinit_bom(uint8_t *b, unsigned max)
{
    ssh_escritor_t e;
    se_iniciar(&e, b, max);
    se_u8(&e, 20);
    for (int i = 0; i < 16; i++) se_u8(&e, (unsigned)i);
    se_cstr(&e, "curve25519-sha256,kex-strict-s-v00@openssh.com");
    se_cstr(&e, "ssh-ed25519");
    se_cstr(&e, "chacha20-poly1305@openssh.com");
    se_cstr(&e, "chacha20-poly1305@openssh.com");
    se_cstr(&e, "hmac-sha2-256");
    se_cstr(&e, "hmac-sha2-256");
    se_cstr(&e, "none");
    se_cstr(&e, "none");
    se_cstr(&e, "");
    se_cstr(&e, "");
    se_u8(&e, 0);
    se_u32(&e, 0);
    return e.n;
}

static void testar_mensagens(void)
{
    uint8_t b[600];
    const uint8_t *lst[10];
    unsigned nl[10];
    bool segue;
    unsigned n = kexinit_bom(b, sizeof b);
    conferir("KEXINIT bueno", ssh_kexinit_ler(b, n, lst, nl, &segue) && !segue &&
                              ssh_lista_tem(lst[0], nl[0], "kex-strict-s-v00@openssh.com") && ssh_lista_tem(lst[1], nl[1], "ssh-ed25519"));
    bool ok = true;
    for (unsigned c = 0; c < n; c++) if (ssh_kexinit_ler(b, c, lst, nl, &segue)) ok = false;
    conferir("KEXINIT cortado en cualquier byte: rechazado", ok);
    /* una lista con un tamaño enorme */
    b[17] = 0x7F;
    conferir("KEXINIT con una lista de 2 GB: rechazado", !ssh_kexinit_ler(b, n, lst, nl, &segue));

    /* ECDH_REPLY */
    uint8_t sem[32], pub[32], qs[32], firma[64], blob[SSH_BLOB_ED25519];
    for (int i = 0; i < 32; i++) { sem[i] = (uint8_t)(i + 3); qs[i] = (uint8_t)(200 - i); }
    ed25519_publica(pub, sem);
    ed25519_assinar(firma, "h", 1, sem, pub);
    ssh_blob_montar(blob, pub);
    ssh_escritor_t e;
    se_iniciar(&e, b, sizeof b);
    se_u8(&e, 31);
    se_texto(&e, blob, sizeof blob);
    se_texto(&e, qs, 32);
    se_u32(&e, 4 + 11 + 4 + 64);
    se_cstr(&e, "ssh-ed25519");
    se_texto(&e, firma, 64);
    n = e.n;
    const uint8_t *ks;
    unsigned nks;
    uint8_t p2[32], q2[32], f2[64];
    conferir("ECDH_REPLY bueno", ssh_ecdh_resposta_ler(b, n, &ks, &nks, p2, q2, f2) && memcmp(p2, pub, 32) == 0 &&
                                 memcmp(q2, qs, 32) == 0 && memcmp(f2, firma, 64) == 0 && nks == sizeof blob);
    ok = true;
    for (unsigned c = 0; c < n; c++) if (ssh_ecdh_resposta_ler(b, c, &ks, &nks, p2, q2, f2)) ok = false;
    conferir("ECDH_REPLY cortado en cualquier byte: rechazado", ok);
    b[n] = 0;
    conferir("ECDH_REPLY con un byte de más: rechazado", !ssh_ecdh_resposta_ler(b, n + 1, &ks, &nks, p2, q2, f2));
    /* una clave ssh-rsa en lugar de ed25519 */
    uint8_t c2[600];
    se_iniciar(&e, c2, sizeof c2);
    se_u8(&e, 31);
    se_u32(&e, 4 + 7 + 4 + 32);
    se_cstr(&e, "ssh-rsa");
    se_texto(&e, qs, 32);
    se_texto(&e, qs, 32);
    se_u32(&e, 4 + 11 + 4 + 64);
    se_cstr(&e, "ssh-ed25519");
    se_texto(&e, firma, 64);
    conferir("ECDH_REPLY con clave ssh-rsa: rechazado", !ssh_ecdh_resposta_ler(c2, e.n, &ks, &nks, p2, q2, f2));
    /* Q_S de 31 bytes */
    se_iniciar(&e, c2, sizeof c2);
    se_u8(&e, 31);
    se_texto(&e, blob, sizeof blob);
    se_texto(&e, qs, 31);
    se_u32(&e, 4 + 11 + 4 + 64);
    se_cstr(&e, "ssh-ed25519");
    se_texto(&e, firma, 64);
    conferir("ECDH_REPLY con Q_S de 31 bytes: rechazado", !ssh_ecdh_resposta_ler(c2, e.n, &ks, &nks, p2, q2, f2));
    /* al azar */
    ok = true;
    for (int i = 0; i < 20000; i++) {
        unsigned m = aleat() % 300;
        uint8_t *r = malloc(m ? m : 1);
        for (unsigned j = 0; j < m; j++) r[j] = (uint8_t)aleat();
        if (m > 20 && i % 2) memcpy(r, b, m < n ? m : n), r[aleat() % m] ^= (uint8_t)aleat();
        bool a = ssh_kexinit_ler(r, m, lst, nl, &segue);
        if (a) for (int k = 0; k < 10; k++) if (lst[k] < r || lst[k] + nl[k] > r + m) ok = false;
        if (ssh_ecdh_resposta_ler(r, m, &ks, &nks, p2, q2, f2) && (ks < r || ks + nks > r + m)) ok = false;
        free(r);
    }
    conferir("20000 mensajes al azar: nada fuera del búfer", ok);
}

/* ------------------------------------------------------------ known_hosts */

static void testar_known_hosts(void)
{
    char nome[300];
    conferir("nombre con el puerto 22", ssh_kh_nome("Servidor.Local", 22, nome, sizeof nome) && strcmp(nome, "servidor.local") == 0);
    conferir("nombre con otro puerto", ssh_kh_nome("10.0.2.2", 2222, nome, sizeof nome) && strcmp(nome, "[10.0.2.2]:2222") == 0);
    conferir("nombre con espacios: no vale", !ssh_kh_nome("a b", 22, nome, sizeof nome));
    conferir("nombre con salto de línea: no vale", !ssh_kh_nome("a\nb", 22, nome, sizeof nome));
    conferir("nombre que no cabe", !ssh_kh_nome("aaaaaaaaaaaaaaaaaaaa", 22, nome, 8));

    uint8_t sem[32], pub[32], otra[32], k[32];
    memset(sem, 1, 32);
    ed25519_publica(pub, sem);
    memset(sem, 2, 32);
    ed25519_publica(otra, sem);
    char lp[200], lo[200], linha[400];
    ssh_linha_publica(pub, lp, sizeof lp);
    ssh_linha_publica(otra, lo, sizeof lo);

    snprintf(linha, sizeof linha, "servidor %s comentario", lp);
    conferir("línea de la máquina", ssh_kh_linha(linha, "servidor", k) == SSH_KH_ED25519 && memcmp(k, pub, 32) == 0);
    conferir("línea de otra máquina", ssh_kh_linha(linha, "otro", k) == SSH_KH_NADA);
    conferir("sin distinguir mayúsculas", ssh_kh_linha(linha, "SERVIDOR", k) == SSH_KH_ED25519);
    snprintf(linha, sizeof linha, "otro,servidor,10.0.0.1 %s", lp);
    conferir("lista de nombres", ssh_kh_linha(linha, "10.0.0.1", k) == SSH_KH_ED25519);
    snprintf(linha, sizeof linha, "*.casa,!malo.casa %s", lp);
    conferir("patrón con *", ssh_kh_linha(linha, "pc.casa", k) == SSH_KH_ED25519);
    conferir("patrón negado", ssh_kh_linha(linha, "malo.casa", k) == SSH_KH_NADA);
    snprintf(linha, sizeof linha, "pc?.casa %s", lp);
    conferir("patrón con ?", ssh_kh_linha(linha, "pc1.casa", k) == SSH_KH_ED25519 && ssh_kh_linha(linha, "pc12.casa", k) == SSH_KH_NADA);
    snprintf(linha, sizeof linha, "[10.0.2.2]:2222 %s", lp);
    conferir("máquina con puerto", ssh_kh_linha(linha, "[10.0.2.2]:2222", k) == SSH_KH_ED25519 && ssh_kh_linha(linha, "10.0.2.2", k) == SSH_KH_NADA);
    conferir("línea de otro tipo", ssh_kh_linha("servidor ssh-rsa AAAAB3NzaC1yc2E", "servidor", k) == SSH_KH_OUTRO);
    conferir("comentario", ssh_kh_linha("# servidor ssh-ed25519 AAAA", "servidor", k) == SSH_KH_NADA);
    conferir("nombre cifrado (|1|): no se reconoce", ssh_kh_linha("|1|abc=|def= ssh-ed25519 AAAA", "servidor", k) == SSH_KH_NADA);
    conferir("ed25519 rota", ssh_kh_linha("servidor ssh-ed25519 AAAAxx", "servidor", k) == SSH_KH_OUTRO);
    snprintf(linha, sizeof linha, "@revoked * %s", lp);
    conferir("@revoked", ssh_kh_linha(linha, "cualquiera", k) == SSH_KH_REVOGADA && memcmp(k, pub, 32) == 0);
    snprintf(linha, sizeof linha, "@cert-authority * %s", lp);
    conferir("@cert-authority: no se usa", ssh_kh_linha(linha, "servidor", k) == SSH_KH_NADA);

    /* el veredicto */
    ssh_kh_t v;
    char l1[400], l2[400];
    snprintf(l1, sizeof l1, "servidor %s", lp);
    snprintf(l2, sizeof l2, "servidor %s", lo);
    ssh_kh_iniciar(&v);
    ssh_kh_somar(&v, "# vacío", "servidor", pub);
    conferir("veredicto: desconocida", v.estado == SSH_KH_DESCONHECIDA);
    ssh_kh_somar(&v, l1, "servidor", pub);
    conferir("veredicto: conocida", v.estado == SSH_KH_CONHECIDA);
    ssh_kh_iniciar(&v);
    ssh_kh_somar(&v, "x y z", "servidor", otra);
    ssh_kh_somar(&v, l1, "servidor", otra);
    conferir("veredicto: cambiada (y en qué línea)", v.estado == SSH_KH_MUDOU && v.linha_mudou == 2);
    ssh_kh_iniciar(&v);
    ssh_kh_somar(&v, l1, "servidor", pub);
    ssh_kh_somar(&v, l2, "servidor", pub);
    conferir("veredicto: dos claves distintas guardadas = cambiada", v.estado == SSH_KH_MUDOU);
    snprintf(linha, sizeof linha, "@revoked * %s", lp);
    ssh_kh_iniciar(&v);
    ssh_kh_somar(&v, l1, "servidor", pub);
    ssh_kh_somar(&v, linha, "servidor", pub);
    conferir("veredicto: revocada gana", v.estado == SSH_KH_REVOCADA);
    ssh_kh_iniciar(&v);
    ssh_kh_somar(&v, "servidor ssh-rsa AAAAB3NzaC1yc2E", "servidor", pub);
    conferir("veredicto: solo otro tipo = desconocida con aviso", v.estado == SSH_KH_DESCONHECIDA && v.outro_tipo);

    /* al azar: nada se rompe */
    for (int i = 0; i < 5000; i++) {
        char r[200];
        unsigned m = aleat() % 199;
        for (unsigned j = 0; j < m; j++) r[j] = (char)(aleat() % 95 + 32);
        r[m] = 0;
        if (i % 3 == 0 && m > 20) memcpy(r, "a,*,!b ssh-ed25519 ", 19);
        ssh_kh_linha(r, "abc", k);
        ssh_padrao(r, r + m, "abcdefghijklmnopqrstuvwxyz");
    }
    conferir("5000 líneas al azar sin romperse", true);
    /* un patrón con muchos * no tarda (sin recursión) */
    conferir("patrón con muchos *", !ssh_padrao("*a*a*a*a*a*a*a*a*a*b", "*a*a*a*a*a*a*a*a*a*b" + 20, "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
}

/* ------------------------------------------------------------ claves privadas */

static void testar_chaves(void)
{
    uint8_t sem[32], pub[32], s2[32], p2[32];
    char texto[1200], com[80];
    for (int i = 0; i < 32; i++) sem[i] = (uint8_t)(i * 5 + 9);
    ed25519_publica(pub, sem);
    int n = ssh_chave_privada_escrever(sem, pub, "sagui@saguios", 0x12345678, texto, sizeof texto);
    conferir("escribir una clave privada", n > 0 && strncmp(texto, SSH_PRIV_INICIO "\n", sizeof SSH_PRIV_INICIO) == 0);
    int r = ssh_chave_privada_ler(texto, (unsigned)n, s2, p2, com, sizeof com);
    conferir("leerla otra vez", r == SSH_CH_OK && memcmp(s2, sem, 32) == 0 && memcmp(p2, pub, 32) == 0 && strcmp(com, "sagui@saguios") == 0);
    conferir("no cabe: -1", ssh_chave_privada_escrever(sem, pub, "x", 1, texto, 100) == -1);
    n = ssh_chave_privada_escrever(sem, pub, "sagui@saguios", 0x12345678, texto, sizeof texto);
    /* cada carácter de base64 cambiado: o falla, o (si cae en el comentario o en un relleno igual) sigue siendo la misma clave */
    bool ok = true;
    int lidas = 0;
    for (int i = (int)sizeof SSH_PRIV_INICIO; i < n - (int)sizeof SSH_PRIV_FIM - 1; i++) {
        char c = texto[i];
        if (c == '\n' || c == '=') continue;
        texto[i] = c == 'A' ? 'B' : 'A';
        r = ssh_chave_privada_ler(texto, (unsigned)n, s2, p2, NULL, 0);
        if (r == SSH_CH_OK) { lidas++; if (memcmp(p2, pub, 32) != 0 || memcmp(s2, sem, 32) != 0) ok = false; }
        texto[i] = c;
    }
    conferir("una clave dañada nunca da otra clave", ok);
    conferir("formato desconocido", ssh_chave_privada_ler("hola", 4, s2, p2, NULL, 0) == SSH_CH_FORMATO);
    /* una clave «cifrada» (aes256-ctr / bcrypt): se reconoce y se rechaza */
    uint8_t bin[200];
    ssh_escritor_t e;
    se_iniciar(&e, bin, sizeof bin);
    se_bytes(&e, SSH_PRIV_MAGIA, sizeof SSH_PRIV_MAGIA);
    se_cstr(&e, "aes256-ctr");
    se_cstr(&e, "bcrypt");
    se_cstr(&e, "xxxx");
    se_u32(&e, 1);
    se_cstr(&e, "x");
    se_cstr(&e, "y");
    char b64[400];
    b64_codificar(bin, (int)e.n, b64, sizeof b64);
    snprintf(texto, sizeof texto, "%s\n%s\n%s\n", SSH_PRIV_INICIO, b64, SSH_PRIV_FIM);
    conferir("clave con frase de paso: se dice", ssh_chave_privada_ler(texto, (unsigned)strlen(texto), s2, p2, NULL, 0) == SSH_CH_CIFRADA);
    /* al azar dentro del armazón */
    for (int i = 0; i < 3000; i++) {
        unsigned m = aleat() % 180;
        for (unsigned j = 0; j < m; j++) bin[j] = (uint8_t)aleat();
        if (i % 2 && m > 15) memcpy(bin, SSH_PRIV_MAGIA, sizeof SSH_PRIV_MAGIA);
        b64_codificar(bin, (int)m, b64, sizeof b64);
        snprintf(texto, sizeof texto, "%s\n%s\n%s\n", SSH_PRIV_INICIO, b64, SSH_PRIV_FIM);
        r = ssh_chave_privada_ler(texto, (unsigned)strlen(texto), s2, p2, com, sizeof com);
        if (r == SSH_CH_OK) ok = false;
    }
    conferir("3000 claves al azar: ninguna aceptada", ok);
    (void)lidas;
}

/* ------------------------------------------------------------ escape y terminal */

static int escape_texto(const char *in, char *out)
{
    ssh_escape_t e;
    ssh_escape_iniciar(&e);
    unsigned k = 0;
    for (const char *p = in; *p; p++) {
        uint8_t o[2];
        unsigned n;
        int a = ssh_escape_tecla(&e, (uint8_t)*p, o, &n);
        if (a == SSH_ESC_SAIR) { out[k] = 0; return 1; }
        if (a == SSH_ESC_AJUDA) { out[k++] = '#'; }
        for (unsigned j = 0; j < n; j++) out[k++] = (char)o[j];
    }
    out[k] = 0;
    return 0;
}

static void testar_terminal(void)
{
    char o[64];
    conferir("~. al empezar", escape_texto("~.", o) == 1);
    conferir("~. tras Intro", escape_texto("ls\n~.", o) == 1 && strcmp(o, "ls\n") == 0);
    conferir("a~. no es escape", escape_texto("a~.", o) == 0 && strcmp(o, "a~.") == 0);
    conferir("~~ manda un ~", escape_texto("~~x", o) == 0 && strcmp(o, "~x") == 0);
    conferir("~x manda ~x", escape_texto("~x", o) == 0 && strcmp(o, "~x") == 0);
    conferir("~? es la ayuda", escape_texto("~?", o) == 0 && strcmp(o, "#") == 0);
    conferir("~. tras Ctrl+C", escape_texto("abc\003~.", o) == 1);

    ssh_teclado_t t = { 0 };
    uint8_t x[64];
    unsigned n = ssh_teclas_xterm(&t, (const uint8_t *)"a\n\b\033A\033X\033\033\xE9", 10, x);
    conferir("teclas -> xterm", n == 13 && memcmp(x, "a\r\x7f\033[A\033[3~\033\xC3\xA9", 13) == 0);
    t.esc = 0;
    n = ssh_teclas_xterm(&t, (const uint8_t *)"\033", 1, x);
    n += ssh_teclas_xterm(&t, (const uint8_t *)"B", 1, x + n);
    conferir("tecla partida entre dos lecturas", n == 3 && memcmp(x, "\033[B", 3) == 0);
    n = ssh_teclas_xterm(&t, (const uint8_t *)"\033R", 2, x);
    conferir("ESC R (redibujar) no se manda", n == 0);

    ssh_utf8_t u = { 0 };
    n = ssh_utf8_latin1(&u, (const uint8_t *)"a\xC3", 2, x);
    n += ssh_utf8_latin1(&u, (const uint8_t *)"\xA9z", 2, x + n);
    conferir("UTF-8 partido entre dos paquetes", n == 3 && memcmp(x, "a\xE9z", 3) == 0);
    n = ssh_utf8_latin1(&u, (const uint8_t *)"\xE2\x94\x80\xE2\x94\x82\xE2\x94\x8C", 9, x);
    conferir("dibujos de cajas -> - | +", n == 3 && memcmp(x, "-|+", 3) == 0);
    {   /* D320: con el Terminal dibujando las cajas, el juego DEC (ESC ( 0) en vez de + - | */
        ssh_utf8_t d = { .caixas = true };
        uint8_t y[64];
        n = ssh_utf8_latin1(&d, (const uint8_t *)"\xE2\x94\x8C\xE2\x94\x80\xE2\x94\x90x", 10, y);
        conferir("cajas -> ESC ( 0 lqk ESC ( B x", n == 10 && memcmp(y, "\033(0lqk\033(Bx", 10) == 0);
        n = ssh_utf8_latin1(&d, (const uint8_t *)"\xE2\x95\x94\xE2", 4, y);
        n += ssh_utf8_latin1(&d, (const uint8_t *)"\x95\x90\xE2\x96\x88\n", 6, y + n);
        conferir("cajas dobles y bloque, partidas entre paquetes", n == 10 && memcmp(y, "\033(0lq0\033(B\n", 10) == 0 && !d.dec);
        n = ssh_utf8_latin1(&d, (const uint8_t *)"\xE2\x94\x82", 3, y);
        n += ssh_utf8_latin1(&d, (const uint8_t *)"\033[0m", 4, y + n);
        conferir("el juego DEC se cierra antes de una secuencia", n == 11 && memcmp(y, "\033(0x\033(B\033[0m", 11) == 0);
    }
    {   /* teclas del Terminal en modo xterm: pasan tal cual salvo Intro, borrar y Latin-1 */
        uint8_t y[64];
        n = ssh_teclas_xterm_cru((const uint8_t *)"\033OP\033\n\b\xE9", 8, y);
        conferir("modo xterm: F1 y Esc solos pasan, Intro -> CR, borrar -> DEL, Latin-1 -> UTF-8",
                 n == 9 && memcmp(y, "\033OP\033\r\x7F\xC3\xA9", 9) == 0);
    }
    n = ssh_utf8_latin1(&u, (const uint8_t *)"\xC0\x80\xFF\x80", 4, x);
    conferir("UTF-8 no válido -> ?", n == 4 && memcmp(x, "????", 4) == 0);
    n = ssh_utf8_latin1(&u, (const uint8_t *)"\xE0\x80\x80", 3, x);
    conferir("forma larga -> ?", n == 1 && x[0] == '?');
    n = ssh_utf8_latin1(&u, (const uint8_t *)"\033[1;32mv\033[0m", 13, x);
    conferir("las secuencias de color pasan", n == 13);
    n = ssh_retroceso_cursor((const uint8_t *)"ab\b", 3, x);
    conferir("\\b remoto -> ESC [ D", n == 5 && memcmp(x, "ab\033[D", 5) == 0);

    uint8_t r[64];
    memcpy(r, "x\033[8;28;94ty", 12);
    int l = 0, c = 0;
    bool achou = false;
    n = ssh_tamanho_resposta(r, 12, &l, &c, &achou);
    conferir("respuesta de tamaño del Terminal", achou && l == 28 && c == 94 && n == 2 && r[0] == 'x' && r[1] == 'y');
    memcpy(r, "\033[8;0;94t", 9);
    achou = false;
    ssh_tamanho_resposta(r, 9, &l, &c, &achou);
    conferir("tamaño 0: no se acepta", !achou);
    for (int i = 0; i < 5000; i++) {
        unsigned m = aleat() % 60;
        for (unsigned j = 0; j < m; j++) r[j] = (uint8_t)(j % 3 == 0 ? 27 : aleat() % 3 == 0 ? '8' : aleat());
        ssh_tamanho_resposta(r, m, &l, &c, &achou);
        ssh_utf8_latin1(&u, r, m, x);
    }
    conferir("5000 entradas al azar sin romperse", true);
}

/* ------------------------------------------------------------ órdenes para la prueba de Python */

static unsigned de_hex(const char *h, uint8_t *b, unsigned max)
{
    unsigned n = 0;
    while (h[0] && h[1] && n < max) {
        unsigned v;
        sscanf(h, "%2x", &v);
        b[n++] = (uint8_t)v;
        h += 2;
    }
    return n;
}

int main(int argc, char **argv)
{
    if (argc == 4 && strcmp(argv[1], "escribir") == 0) {
        uint8_t sem[32], pub[32];
        char t[1200];
        if (de_hex(argv[2], sem, 32) != 32) return 2;
        ed25519_publica(pub, sem);
        int n = ssh_chave_privada_escrever(sem, pub, argv[3], 0xC0FFEE11, t, sizeof t);
        if (n < 0) return 1;
        fwrite(t, 1, (size_t)n, stdout);
        return 0;
    }
    if (argc == 3 && strcmp(argv[1], "leer") == 0) {
        static char t[8192];
        FILE *f = fopen(argv[2], "rb");
        if (!f) return 2;
        size_t n = fread(t, 1, sizeof t - 1, f);
        fclose(f);
        uint8_t sem[32], pub[32];
        char com[80];
        int r = ssh_chave_privada_ler(t, (unsigned)n, sem, pub, com, sizeof com);
        if (r != SSH_CH_OK) { printf("error %d\n", r); return 1; }
        for (int i = 0; i < 32; i++) printf("%02x", pub[i]);
        printf(" %s\n", com);
        return 0;
    }
    srand(317);
    testar_pacotes();
    testar_mensagens();
    testar_known_hosts();
    testar_chaves();
    testar_terminal();
    printf("ssh_cliente_unidad: %d/%d comprobaciones correctas\n", total - falhas, total);
    return falhas ? 1 : 0;
}
