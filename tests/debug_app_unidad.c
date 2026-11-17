// SPDX-License-Identifier: GPL-2.0-or-later
/* debug_app_unidad.c - pruebas en el anfitrión de la lógica de DEBUG-APP (D362,
 * userland/programas/debug_app_logica.h): el código, su suma de control, la
 * semilla de la clave de un solo uso y las direcciones permitidas. La lanza
 * tests/h364_debug_app.py, que además compara con tools/depurar-remoto.py
 * usando el modo de órdenes:
 *
 *   debug_app_unidad                 las comprobaciones fijas
 *   debug_app_unidad ordenes         una orden por línea en la entrada:
 *       codificar IP PUERTO SECRETO HUELLA   -> el código
 *       decodificar CODIGO                    -> IP PUERTO SECRETO HUELLA, o «error N»
 *       semilla SECRETO                       -> la semilla (hex)
 *       permitido LISTA IP MI_IP MASCARA      -> 1, 0 o «error»
 *   (secreto y huella en hexadecimal)
 *
 * Compilar: gcc -DSAGUI_HOST -Iuserland/inc -Ikernel/inc -Iuserland/programas tests/debug_app_unidad.c kernel/lib/sha256.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "debug_app_logica.h"

/* Lo que la lógica pide a la libsagui. */
size_t tam_texto(const char *s) { return strlen(s); }
int compara_texto_n(const char *a, const char *b, size_t n) { return strncmp(a, b, n); }
int formatar_texto(char *dst, size_t max, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(dst, max, fmt, ap);
    va_end(ap);
    return r;
}

static int total, falhas;
#define OK(c, m) do { total++; if (!(c)) { falhas++; printf("FALLO: %s\n", m); } } while (0)

static uint32_t ip(const char *t)
{
    uint32_t v = 0;
    OK(dapp_ip_texto(t, (unsigned)strlen(t), &v), t);
    return v;
}

static bool perm(const char *lista, const char *cli, const char *meu, int prefixo)
{
    dapp_permitir_t p;
    char e[120];
    if (dapp_permitir_ler(lista, &p, e, sizeof e) < 0) return false;
    return dapp_permitido(&p, ip(cli), ip(meu), dapp_mascara(prefixo));
}

static bool lista_mal(const char *lista)
{
    dapp_permitir_t p;
    char e[120];
    return dapp_permitir_ler(lista, &p, e, sizeof e) < 0 && e[0];
}

static void fixas(void)
{
    dapp_codigo_t c, d;
    char t[DAPP_CODIGO_MAX];
    memset(&c, 0, sizeof c);
    c.ip = ip("192.168.10.37");
    c.porta = 2222;
    for (int i = 0; i < 8; i++) { c.segredo[i] = (uint8_t)(0xA0 + i); c.huella[i] = (uint8_t)(0x10 * i + 3); }
    OK(dapp_codificar(&c, t, sizeof t), "codificar");
    OK(strlen(t) == 47 && t[5] == '-' && t[41] == '-', "40 letras en 8 grupos con guiones");
    OK(!dapp_codificar(&c, t, 20), "sin sitio: no codifica");
    dapp_codificar(&c, t, sizeof t);
    OK(dapp_decodificar(t, &d) == 0 && d.ip == c.ip && d.porta == c.porta && !memcmp(d.segredo, c.segredo, 8) &&
       !memcmp(d.huella, c.huella, 8), "ida y vuelta");
    /* minúsculas, espacios, sin guiones y las letras confusas */
    char u[64];
    int k = 0;
    for (int i = 0; t[i]; i++)
        if (t[i] != '-') u[k++] = (char)(t[i] >= 'A' && t[i] <= 'Z' ? t[i] + 32 : t[i]);
    u[k] = 0;
    OK(dapp_decodificar(u, &d) == 0 && d.porta == 2222, "minúsculas y sin guiones");
    for (int i = 0; u[i]; i++) {
        if (u[i] == '1') u[i] = 'l';
        else if (u[i] == '0') u[i] = 'O';
    }
    OK(dapp_decodificar(u, &d) == 0 && d.ip == c.ip, "l por 1 y O por 0");
    /* cada letra cambiada se detecta (la suma de 16 bits; con estos datos, todas) */
    int detectadas = 0, probadas = 0;
    for (int i = 0; t[i]; i++) {
        if (t[i] == '-') continue;
        char v = t[i];
        t[i] = v == 'Z' ? 'Y' : (char)(v == '9' ? 'A' : v + 1);
        if (t[i] == 'I' || t[i] == 'L' || t[i] == 'O' || t[i] == 'U') t[i]++;
        probadas++;
        detectadas += dapp_decodificar(t, &d) == DAPP_MAL_SUMA;
        t[i] = v;
    }
    OK(detectadas == probadas, "una letra cambiada: la suma no cuadra");
    /* dos letras intercambiadas */
    char a = t[0];
    t[0] = t[1];
    t[1] = a;
    OK(t[0] == t[1] || dapp_decodificar(t, &d) == DAPP_MAL_SUMA, "dos letras intercambiadas");
    t[1] = t[0];
    t[0] = a;
    OK(dapp_decodificar("ABC", &d) == DAPP_MAL_LETRA, "corto");
    OK(dapp_decodificar("UUUUU-UUUUU-UUUUU-UUUUU-UUUUU-UUUUU-UUUUU-UUUUU", &d) == DAPP_MAL_LETRA, "U no es del alfabeto");
    char largo[64];
    snprintf(largo, sizeof largo, "%sA", t);
    OK(dapp_decodificar(largo, &d) == DAPP_MAL_LETRA, "una letra de más");
    /* otra versión, con su suma bien hecha */
    uint8_t b[DAPP_BYTES];
    dapp_empacotar(&c, b);
    b[0] = DAPP_VERSAO + 1;
    dapp_suma(b, b + DAPP_CARGA);
    char t2[DAPP_CODIGO_MAX];
    unsigned acc = 0, nb = 0, n = 0;
    for (unsigned i = 0; i < DAPP_BYTES; i++) {
        acc = acc << 8 | b[i];
        nb += 8;
        while (nb >= 5) { t2[n++] = DAPP_ALFABETO[(acc >> (nb - 5)) & 31]; nb -= 5; }
    }
    t2[n] = 0;
    OK(dapp_decodificar(t2, &d) == DAPP_MAL_VERSAO, "otra versión");

    /* semilla: SHA-256(rótulo | secreto) */
    uint8_t s1[32], s2[32];
    dapp_semente(c.segredo, s1);
    c.segredo[7] ^= 1;
    dapp_semente(c.segredo, s2);
    OK(memcmp(s1, s2, 32) != 0, "otro secreto, otra semilla");

    /* huella atada al secreto: otro secreto u otra clave, otra huella */
    uint8_t hc[32], f1[DAPP_HUELLA], f2[DAPP_HUELLA];
    for (int i = 0; i < 32; i++) hc[i] = (uint8_t)(i * 7 + 1);
    dapp_huella(c.segredo, hc, f1);
    c.segredo[0] ^= 0x80;
    dapp_huella(c.segredo, hc, f2);
    OK(memcmp(f1, f2, DAPP_HUELLA) != 0, "huella: otro secreto, otra huella");
    c.segredo[0] ^= 0x80;
    hc[31] ^= 1;
    dapp_huella(c.segredo, hc, f2);
    OK(memcmp(f1, f2, DAPP_HUELLA) != 0, "huella: otra clave del servidor, otra huella");

    /* direcciones */
    OK(perm("subred", "192.168.10.121", "192.168.10.37", 24), "subred: la misma /24");
    OK(!perm("subred", "192.168.1.132", "192.168.10.37", 24), "subred: otra /24 no");
    OK(!perm("subred", "192.168.10.255", "192.168.10.37", 24), "subred: la difusión no");
    OK(!perm("subred", "192.168.10.0", "192.168.10.37", 24), "subred: la dirección de red no");
    OK(!perm("subred", "192.168.10.37", "192.168.10.37", 24), "subred: uno mismo no");
    OK(!perm("subred", "192.168.10.9", "192.168.10.37", 32), "subred /32: nada");
    OK(perm("", "10.0.2.2", "10.0.2.15", 24), "vacía = subred");
    OK(!perm("subred", "8.8.8.8", "8.8.8.1", 24), "subred pública sin «publicas»: no");
    OK(perm("subred,publicas", "8.8.8.8", "8.8.8.1", 24), "con «publicas»: sí");
    OK(perm("subred, 192.168.1.0/24", "192.168.1.132", "192.168.10.37", 24), "otra red añadida");
    OK(perm("10.8.0.7", "10.8.0.7", "192.168.10.37", 24), "una dirección");
    OK(!perm("10.8.0.7", "10.8.0.8", "192.168.10.37", 24), "solo esa dirección");
    OK(!perm("10.8.0.7", "192.168.10.121", "192.168.10.37", 24), "sin «subred», la subred no entra");
    OK(perm("172.16.0.0/12", "172.31.255.1", "192.168.10.37", 24), "172.16/12");
    OK(perm("169.254.0.0/16", "169.254.3.4", "169.254.3.5", 16), "enlace local");
    OK(lista_mal("8.8.8.0/24"), "red pública sin «publicas»: error");
    OK(lista_mal("172.0.0.0/8"), "172/8 es más que lo privado: error");
    OK(lista_mal("10.0.0.0/7"), "prefijo de 7: error");
    OK(lista_mal("192.168.1.0/33"), "prefijo de 33: error");
    OK(lista_mal("192.168.01.1"), "ceros de más: error");
    OK(lista_mal("192.168.1"), "tres partes: error");
    OK(lista_mal("cualquiera"), "palabra desconocida: error");
    OK(lista_mal("subred,subred,subred,subred,subred,subred,subred,subred,subred"), "más de 8 entradas: error");
    OK(!lista_mal("8.8.8.0/24 publicas"), "red pública con «publicas»: vale");
    OK(!perm("publicas,8.8.8.0/24", "127.0.0.1", "192.168.10.37", 24), "127.0.0.1 no está en la lista");
    OK(!perm("subred", "100.64.0.5", "100.64.0.1", 24), "CGNAT no es privada");
    OK(!perm("subred", "0.0.0.0", "192.168.10.37", 24), "0.0.0.0 no");
}

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

static void hex(const uint8_t *b, unsigned n)
{
    for (unsigned i = 0; i < n; i++) printf("%02x", b[i]);
}

static int ordenes(void)
{
    char l[512];
    while (fgets(l, sizeof l, stdin)) {
        char a[200], b2[200], c2[200], d2[200], e2[200];
        int n = sscanf(l, "%199s %199s %199s %199s %199s", a, b2, c2, d2, e2);
        if (n >= 5 && !strcmp(a, "codificar")) {
            dapp_codigo_t c;
            char t[DAPP_CODIGO_MAX];
            c.ip = ip(b2);
            c.porta = (unsigned)atoi(c2);
            de_hex(d2, c.segredo, 8);
            de_hex(e2, c.huella, 8);
            dapp_codificar(&c, t, sizeof t);
            printf("%s\n", t);
        } else if (n >= 2 && !strcmp(a, "decodificar")) {
            dapp_codigo_t c;
            int r = dapp_decodificar(b2, &c);
            if (r) printf("error %d\n", r);
            else {
                printf("%u.%u.%u.%u %u ", c.ip >> 24, c.ip >> 16 & 255, c.ip >> 8 & 255, c.ip & 255, c.porta);
                hex(c.segredo, 8);
                printf(" ");
                hex(c.huella, 8);
                printf("\n");
            }
        } else if (n >= 3 && !strcmp(a, "huella")) {
            uint8_t s[8], hc[32], o[DAPP_HUELLA];
            de_hex(b2, s, 8);
            de_hex(c2, hc, 32);
            dapp_huella(s, hc, o);
            hex(o, DAPP_HUELLA);
            printf("\n");
        } else if (n >= 2 && !strcmp(a, "semilla")) {
            uint8_t s[8], o[32];
            de_hex(b2, s, 8);
            dapp_semente(s, o);
            hex(o, 32);
            printf("\n");
        } else if (n >= 5 && !strcmp(a, "permitido")) {
            dapp_permitir_t p;
            char e[120];
            if (dapp_permitir_ler(b2, &p, e, sizeof e) < 0) printf("error\n");
            else {
                uint32_t m = 0, i1 = 0, i2 = 0;
                dapp_ip_texto(c2, (unsigned)strlen(c2), &i1);
                dapp_ip_texto(d2, (unsigned)strlen(d2), &i2);
                dapp_ip_texto(e2, (unsigned)strlen(e2), &m);
                printf("%d\n", dapp_permitido(&p, i1, i2, m) ? 1 : 0);
            }
        } else printf("?\n");
        fflush(stdout);
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "ordenes")) return ordenes();
    fixas();
    printf("debug_app_unidad: %d/%d comprobaciones correctas\n", total - falhas, total);
    return falhas != 0;
}
