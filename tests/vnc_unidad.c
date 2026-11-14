// SPDX-License-Identifier: GPL-2.0-or-later
/* vnc_unidad.c - pruebas en el anfitrión del servidor VNC (mejora 097, D252).
 * La lanza tests/h250_vnc.py.
 *
 *   vnc_unidad            comprobaciones fijas: DES con vectores conocidos (FIPS 46 / NBS),
 *                         la clave y la respuesta del RFB, teclas, agrupación de bloques
 *   vnc_unidad ordenes    lee órdenes de la entrada, una por línea (todo en hexadecimal):
 *       des CLAVE8 BLOQUE8 [d]                 -> bloque cifrado (o descifrado con d)
 *       resp CONTRASEÑA_HEX RETO16             -> respuesta del RFB (16 bytes)
 *       z DATOS                                -> trozo comprimido por el flujo continuo
 *                                                 (el estado se conserva entre órdenes; "zreiniciar" lo borra)
 *       enc TIPO W H BPP BIG RMAX GMAX BMAX RS GS BS PIXELES
 *                                              -> rectángulo codificado; TIPO: raw, hextile o zrle
 *                                                 (datos de ZRLE sin comprimir); PIXELES: 0x00RRGGBB de 8 cifras
 *
 * Compilar: gcc -DSAGUI_HOST -Iuserland/inc -Ikernel/inc -Iuserland/programas tests/vnc_unidad.c
 *   userland/libsagui/des.c userland/libsagui/deflado_continuo.c */
#include <stdio.h>
#include <stdlib.h>
#include "des.h"
#include "compressao.h"
#include "vnc_codec.h"

static int total, falhas;
static void ok(int cond, const char *msg)
{
    total++;
    if (!cond) { falhas++; printf("FALLA: %s\n", msg); }
}

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

static void a_hex(const uint8_t *b, unsigned n, char *s)
{
    for (unsigned i = 0; i < n; i++) sprintf(s + 2 * i, "%02x", b[i]);
    if (!n) strcpy(s, "-");
}

static void vector_des(const char *k, const char *p, const char *c)
{
    uint8_t kk[8], pp[8], cc[8], r[8];
    de_hex(k, kk, 8); de_hex(p, pp, 8); de_hex(c, cc, 8);
    des_bloque(kk, pp, r, true);
    char m[100];
    snprintf(m, sizeof m, "DES cifrar %s %s", k, p);
    ok(memcmp(r, cc, 8) == 0, m);
    des_bloque(kk, cc, r, false);
    snprintf(m, sizeof m, "DES descifrar %s %s", k, c);
    ok(memcmp(r, pp, 8) == 0, m);
}

static void fijas(void)
{
    /* vectores clásicos (Stallings, FIPS 81, NBS) */
    vector_des("133457799bbcdff1", "0123456789abcdef", "85e813540f0ab405");
    vector_des("0123456789abcdef", "4e6f772069732074", "3fa40e8a984d4815");   /* "Now is t" */
    vector_des("0000000000000000", "0000000000000000", "8ca64de9c1b123a7");
    vector_des("ffffffffffffffff", "ffffffffffffffff", "7359b2163e4edc58");
    vector_des("3000000000000000", "1000000000000001", "958e6e627a05557b");
    vector_des("1111111111111111", "1111111111111111", "f40379ab9e0ec533");
    vector_des("0123456789abcdef", "1111111111111111", "17668dfc7292532d");
    vector_des("fedcba9876543210", "0123456789abcdef", "ed39d950fa74bcc4");
    /* la clave del RFB: bits invertidos, relleno con ceros, corte a 8 */
    uint8_t k[8];
    vnc_clave_des("a", k);
    ok(k[0] == 0x86 && k[1] == 0 && k[7] == 0, "clave RFB de «a» (0x61 invertido = 0x86)");
    vnc_clave_des("password", k);
    static const uint8_t esperada[8] = { 0x0E, 0x86, 0xCE, 0xCE, 0xEE, 0xF6, 0x4E, 0x26 };   /* 'p','a','s','s','w','o','r','d' con los bits invertidos */
    ok(memcmp(k, esperada, 8) == 0, "clave RFB de «password»");
    uint8_t k2[8];
    vnc_clave_des("passwordXYZ", k2);
    ok(memcmp(k, k2, 8) == 0, "la contraseña se corta a 8 caracteres");
    /* respuesta del RFB = dos bloques cifrados con la clave invertida */
    uint8_t reto[16], r1[16], r2[16], b1[8];
    for (int i = 0; i < 16; i++) reto[i] = (uint8_t)(i * 17 + 3);
    vnc_respuesta("password", reto, r1);
    des_bloque(k, reto, b1, true);
    ok(memcmp(r1, b1, 8) == 0, "la respuesta empieza por el primer bloque cifrado");
    des_bloque(k, reto + 8, b1, true);
    ok(memcmp(r1 + 8, b1, 8) == 0, "y sigue con el segundo");
    vnc_respuesta("passworX", reto, r2);
    ok(memcmp(r1, r2, 16) != 0, "otra contraseña, otra respuesta");
    vnc_respuesta("", reto, r2);
    uint8_t cero[8] = { 0 };
    des_bloque(cero, reto, b1, true);
    ok(memcmp(r2, b1, 8) == 0, "contraseña vacía = clave de ceros");

    /* teclas */
    ok(vnc_tecla('a') == 'a' && vnc_tecla(0xE9) == 0xE9, "keysyms Latin-1");
    ok(vnc_tecla(0xFF0D) == '\n' && vnc_tecla(0xFF08) == '\b', "Enter y Retroceso");
    ok(vnc_tecla(0xFF51) == TECLA_ESQUERDA && vnc_tecla(0xFF53) == TECLA_DIREITA, "flechas");
    ok(vnc_tecla(0xFFBE) == 0x10A && vnc_tecla(0xFFC9) == 0x115, "F1 y F12");
    ok(vnc_tecla(0xFFFF) == TECLA_DELETE && vnc_tecla(0xFF1B) == TECLA_ESC, "Supr y Esc");
    ok(vnc_tecla(0x01000041) == 'A' && vnc_tecla(0x010020AC) == 0, "Unicode directo solo si cabe en Latin-1");
    ok(vnc_tecla(0xFFE1) == 0 && vnc_modificador(0xFFE1) == INY_MOD_SHIFT && vnc_modificador(0xFFE4) == INY_MOD_CTRL, "modificadores");

    /* formato de píxel */
    vnc_pf_t pf;
    vnc_pf_nativo(&pf);
    ok(vnc_pf_preparar(&pf) && pf.cp == 3 && !pf.cp_alto, "formato nativo: CPIXEL de 3 bytes");
    ok(vnc_valor(&pf, 0x123456) == 0x123456, "conversión nativa = identidad");
    pf.bpp = 16; pf.profundidad = 16; pf.max[0] = 31; pf.max[1] = 63; pf.max[2] = 31; pf.desp[0] = 11; pf.desp[1] = 5; pf.desp[2] = 0;
    ok(vnc_pf_preparar(&pf) && pf.cp == 2, "RGB565");
    ok(vnc_valor(&pf, 0xFFFFFF) == 0xFFFF && vnc_valor(&pf, 0xFF0000) == 0xF800 && vnc_valor(&pf, 0) == 0, "RGB565: blanco, rojo, negro");
    pf.bpp = 8; pf.profundidad = 8; pf.max[0] = 7; pf.max[1] = 7; pf.max[2] = 3; pf.desp[0] = 0; pf.desp[1] = 3; pf.desp[2] = 6;
    ok(vnc_pf_preparar(&pf) && vnc_valor(&pf, 0xFFFFFF) == 0xFF, "BGR233");
    pf.color_verdadero = 0;
    ok(!vnc_pf_preparar(&pf), "con paleta no se admite");
    vnc_pf_nativo(&pf);
    pf.desp[0] = 24; pf.desp[1] = 16; pf.desp[2] = 8;   /* 0xRRGGBB00: los tres bytes altos */
    ok(vnc_pf_preparar(&pf) && pf.cp == 3 && pf.cp_alto, "formato con colores en los 3 bytes altos: CPIXEL alto");

    /* agrupación de bloques */
    uint8_t suc[4 * 3] = { 1, 1, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0 }, usado[12];
    vnc_rect_t r[8];
    int n = vnc_agrupar(suc, usado, 4, 3, 64, 48, r, 8);
    ok(n == 2 && r[0].x == 0 && r[0].y == 0 && r[0].w == 32 && r[0].h == 16, "agrupar: racha de dos bloques");
    ok(n == 2 && r[1].x == 16 && r[1].y == 16 && r[1].w == 32 && r[1].h == 16, "agrupar: segunda racha");
    uint8_t todo[4 * 3] = { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 };
    n = vnc_agrupar(todo, usado, 4, 3, 60, 40, r, 8);
    ok(n == 1 && r[0].w == 60 && r[0].h == 40, "agrupar: todo sucio = un rectángulo recortado a la pantalla");
    uint8_t salp[12] = { 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0 };
    n = vnc_agrupar(salp, usado, 4, 3, 64, 48, r, 2);
    ok(n == 1 && r[0].x == 0 && r[0].y == 0 && r[0].w == 64 && r[0].h == 48, "agrupar: si hay demasiados, el contorno");

    /* hash de bloque: cambia con un píxel, no con otro bloque */
    uint32_t img[32 * 32];
    memset(img, 0, sizeof img);
    uint32_t h0 = vnc_hash_bloque(img, 32, 0, 0, 32), h1 = vnc_hash_bloque(img, 32, 1, 1, 32);
    img[5 * 32 + 5] = 1;
    ok(vnc_hash_bloque(img, 32, 0, 0, 32) != h0 && vnc_hash_bloque(img, 32, 1, 1, 32) == h1, "hash de bloque sensible solo a su bloque");

    printf("vnc_unidad: %d/%d comprobaciones correctas\n", total - falhas, total);
}

static zflujo_t *zf;

static void ordenes(void)
{
    char linea[1 << 20];
    zf = malloc(sizeof *zf);
    zflujo_iniciar(zf);
    while (fgets(linea, sizeof linea, stdin)) {
        char *tok[16];
        int nt = 0;
        for (char *p = strtok(linea, " \r\n"); p && nt < 16; p = strtok(NULL, " \r\n")) tok[nt++] = p;
        if (!nt) continue;
        static uint8_t buf[1 << 19];
        static char sal[1 << 21];
        if (!strcmp(tok[0], "des") && nt >= 3) {
            uint8_t k[8], b[8], r[8];
            de_hex(tok[1], k, 8); de_hex(tok[2], b, 8);
            des_bloque(k, b, r, nt < 4);
            a_hex(r, 8, sal);
        } else if (!strcmp(tok[0], "resp") && nt >= 3) {
            uint8_t c[64], reto[16], r[16];
            unsigned n = de_hex(tok[1], c, 63);
            c[n] = 0;
            de_hex(tok[2], reto, 16);
            vnc_respuesta((const char *)c, reto, r);
            a_hex(r, 16, sal);
        } else if (!strcmp(tok[0], "zreiniciar")) {
            zflujo_iniciar(zf);
            strcpy(sal, "ok");
        } else if (!strcmp(tok[0], "z") && nt >= 2) {
            unsigned n = de_hex(tok[1], buf, sizeof buf);
            static uint8_t cz[1 << 19];
            int r = zflujo_comprimir(zf, buf, (int)n, cz, sizeof cz);
            if (r < 0) strcpy(sal, "error");
            else a_hex(cz, (unsigned)r, sal);
        } else if (!strcmp(tok[0], "enc") && nt >= 13) {
            int w = atoi(tok[2]), h = atoi(tok[3]);
            vnc_pf_t pf;
            memset(&pf, 0, sizeof pf);
            pf.bpp = (uint8_t)atoi(tok[4]); pf.profundidad = pf.bpp == 32 ? 24 : pf.bpp; pf.big = (uint8_t)atoi(tok[5]); pf.color_verdadero = 1;
            for (int c = 0; c < 3; c++) { pf.max[c] = (uint16_t)atoi(tok[6 + c]); pf.desp[c] = (uint8_t)atoi(tok[9 + c]); }
            if (!vnc_pf_preparar(&pf)) { strcpy(sal, "formato"); }
            else {
                uint32_t *px = malloc((size_t)w * h * 4), *v = malloc((size_t)w * h * 4);
                const char *hx = tok[12];
                for (int i = 0; i < w * h; i++) { unsigned q; sscanf(hx + 8 * i, "%8x", &q); px[i] = q; }
                vnc_convertir(&pf, px, w, w, h, v);
                uint8_t *o = malloc((size_t)vnc_limite(w, h));
                int n;
                if (!strcmp(tok[1], "raw")) n = vnc_raw(&pf, v, w, h, o);
                else if (!strcmp(tok[1], "hextile")) n = vnc_hextile(&pf, v, w, h, o);
                else n = vnc_zrle_datos(&pf, v, w, h, o);
                char *g = malloc((size_t)n * 2 + 8);
                a_hex(o, (unsigned)n, g);
                strcpy(sal, g);
                free(g); free(o); free(px); free(v);
            }
        } else strcpy(sal, "?");
        puts(sal);
        fflush(stdout);
    }
    free(zf);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "ordenes")) { ordenes(); return 0; }
    fijas();
    return falhas ? 1 : 0;
}
