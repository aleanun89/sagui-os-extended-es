// SPDX-License-Identifier: GPL-2.0-or-later
/* vnc_cliente_unidad.c - pruebas en el anfitrión de los decodificadores del cliente VNC
 * (mejora 097 fase 2, D254). La lanza tests/h253_vnc_cliente.py.
 *
 *   vnc_cliente_unidad                comprobaciones fijas (teclas, inflado continuo básico)
 *   vnc_cliente_unidad ordenes        lee órdenes de la entrada, una por línea; datos en hexadecimal
 *       fmt FW FH BPP BIG RMAX GMAX BMAX RS GS BS   -> formato del servidor y pantalla nueva negra (reinicia flujos)
 *       rect ENC X Y W H TROZO DATOS               -> descodifica un rectángulo dando los datos de TROZO en TROZO bytes
 *                                                     (0 = de una vez); contesta "ok CONSUMIDOS" o "err CODIGO"
 *       fb                                         -> la pantalla: un 0xRRGGBB de 6 cifras por píxel
 *       z DATOS                                    -> un trozo por el inflado continuo ("zr" lo reinicia); contesta los bytes
 *       cursor W H DATOS                           -> forma del cursor: "ok N" y píxeles y máscara
 *       jpeg DATOS                                 -> el JPEG con el decodificador de la libsagui: "ok W H" y los píxeles
 *       tecla CODIGO                               -> keysym de SaguiOS -> X11 (hexadecimal)
 *
 * Compilar: gcc -DSAGUI_HOST -Iuserland/inc -Ikernel/inc -Iuserland/programas tests/vnc_cliente_unidad.c
 *   userland/libsagui/inflado_continuo.c userland/libsagui/imagem_jpeg.c userland/libsagui/imagem_idct.c tools/video_host.c */
#include <stdio.h>
#include <stdlib.h>
#include "imagem.h"
#include "vnc_decod.h"

static int total, falhas;
static void ok(int cond, const char *msg)
{
    total++;
    if (!cond) { falhas++; printf("FALLA: %s\n", msg); }
}

static bool jpeg_real(const uint8_t *d, int n, int w, int h, uint32_t *px)
{
    imagem_t im;
    if (imagem_ler_jpeg(d, n, &im) != 0) return false;
    bool b = im.larg == w && im.alt == h;
    if (b) memcpy(px, im.px, (size_t)w * h * 4);
    imagem_liberar(&im);
    return b;
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

static vnc_dec_t *dec;

static void ordenes(void)
{
    char *linea = malloc(1 << 25);
    static uint8_t datos[1 << 24];
    uint8_t *salida = malloc(1 << 25);
    dec = calloc(1, sizeof *dec);
    dec->jpeg = jpeg_real;
    while (fgets(linea, 1 << 25, stdin)) {
        char cmd[16];
        if (sscanf(linea, "%15s", cmd) != 1) continue;
        if (!strcmp(cmd, "fmt")) {
            int fw, fh, bpp, big, m[3], s[3];
            sscanf(linea, "%*s %d %d %d %d %d %d %d %d %d %d", &fw, &fh, &bpp, &big, &m[0], &m[1], &m[2], &s[0], &s[1], &s[2]);
            vnc_pf_t pf;
            memset(&pf, 0, sizeof pf);
            pf.bpp = (uint8_t)bpp; pf.big = (uint8_t)big; pf.color_verdadero = 1;
            pf.profundidad = bpp == 32 ? 24 : (uint8_t)bpp;
            for (int c = 0; c < 3; c++) { pf.max[c] = (uint16_t)m[c]; pf.desp[c] = (uint8_t)s[c]; }
            bool b = vnc_dec_formato(dec, &pf) && vnc_dec_pantalla(dec, fw, fh);
            vnc_dec_reiniciar_flujos(dec);
            puts(b ? "ok" : "err formato");
        } else if (!strcmp(cmd, "rect")) {
            int enc, x, y, w, h, trozo;
            char *hex = malloc(1 << 25);
            if (sscanf(linea, "%*s %d %d %d %d %d %d %s", &enc, &x, &y, &w, &h, &trozo, hex) != 7) { puts("err orden"); free(hex); continue; }
            unsigned n = de_hex(hex, datos, sizeof datos);
            free(hex);
            vnc_dec_empezar(dec);
            unsigned avail = trozo ? (unsigned)trozo : n, ini = 0;
            if (avail > n) avail = n;
            bool hecho = false;
            int err = 0;
            for (;;) {
                int r = vnc_dec_rect(dec, enc, x, y, w, h, datos + ini, (int)(avail - ini), &hecho);
                if (r < 0) { err = r; break; }
                ini += (unsigned)r;
                if (hecho) break;
                if (r == 0) {   /* faltan datos: llegan otros TROZO bytes */
                    if (avail >= n) { err = 1000; break; }
                    avail += trozo ? (unsigned)trozo : n;
                    if (avail > n) avail = n;
                }
            }
            if (err) printf("err %d\n", err);
            else printf("ok %u\n", ini);
        } else if (!strcmp(cmd, "fb")) {
            for (long i = 0; i < (long)dec->fw * dec->fh; i++) printf("%06x", dec->fb[i] & 0xFFFFFF);
            putchar('\n');
        } else if (!strcmp(cmd, "z")) {
            static zinflar_t *z;
            char *hex = malloc(1 << 25);
            sscanf(linea, "%*s %s", hex);
            if (!z) { z = calloc(1, sizeof *z); }
            unsigned n = de_hex(hex, datos, sizeof datos);
            free(hex);
            int r = zinflar(z, datos, (int)n, salida, 1 << 25);
            if (r < 0) printf("err %d\n", r);
            else { for (int i = 0; i < r; i++) printf("%02x", salida[i]); if (!r) putchar('-'); putchar('\n'); }
        } else if (!strcmp(cmd, "zr")) {
            puts("ok");   /* lo hace "fmt": los flujos de la orden z son otros; se reabre con un z nuevo */
        } else if (!strcmp(cmd, "cursor")) {
            int w, h;
            char *hex = malloc(1 << 25);
            sscanf(linea, "%*s %d %d %s", &w, &h, hex);
            unsigned n = de_hex(hex, datos, sizeof datos);
            free(hex);
            uint32_t *px = malloc((size_t)w * h * 4 + 4);
            uint8_t *m = malloc((size_t)w * h + 1);
            int r = vnc_dec_cursor(dec, w, h, datos, (int)n, px, m);
            if (r <= 0) printf("err %d\n", r);
            else {
                printf("ok %d\n", r);
                for (int i = 0; i < w * h; i++) printf("%06x", px[i] & 0xFFFFFF);
                putchar('\n');
                for (int i = 0; i < w * h; i++) printf("%d", m[i]);
                putchar('\n');
            }
            free(px); free(m);
        } else if (!strcmp(cmd, "jpeg")) {
            char *hex = malloc(1 << 25);
            sscanf(linea, "%*s %s", hex);
            unsigned n = de_hex(hex, datos, sizeof datos);
            free(hex);
            imagem_t im;
            int r = imagem_ler_jpeg(datos, (int)n, &im);
            if (r) printf("err %d\n", r);
            else {
                printf("ok %d %d\n", im.larg, im.alt);
                for (int i = 0; i < im.larg * im.alt; i++) printf("%06x", im.px[i] & 0xFFFFFF);
                putchar('\n');
                imagem_liberar(&im);
            }
        } else if (!strcmp(cmd, "tecla")) {
            unsigned t;
            sscanf(linea, "%*s %x", &t);
            printf("%x\n", vnc_keysym(t));
        } else puts("err orden desconocida");
        fflush(stdout);
    }
    free(linea);
    free(salida);
    vnc_dec_liberar(dec);
    free(dec);
}

int main(int argc, char **argv)
{
    if (argc > 1 && !strcmp(argv[1], "ordenes")) { ordenes(); return 0; }
    ok(vnc_keysym('a') == 'a' && vnc_keysym(0xE9) == 0xE9, "letras y Latin-1 igual que el keysym");
    ok(vnc_keysym(TECLA_ESC) == 0xFF1B && vnc_keysym(TECLA_DELETE) == 0xFFFF && vnc_keysym('\b') == 0xFF08, "teclas especiales");
    ok(vnc_keysym(TECLA_CIMA) == 0xFF52 && vnc_keysym(TECLA_BAIXO) == 0xFF54 && vnc_keysym(0x10A) == 0xFFBE && vnc_keysym(0x115) == 0xFFC9, "flechas y F1 a F12");
    ok(vnc_keysym(TECLA_F11) == 0, "F11 es del sistema");
    /* la inversa de vnc_tecla */
    int mal = 0;
    for (unsigned t = 1; t < 0x120; t++) { uint32_t k = vnc_keysym(t); if (k && t != '\r' && t != 0x7F && vnc_tecla(k) != (int)t) mal++; }
    ok(mal == 0, "vnc_keysym es la inversa de vnc_tecla del servidor");
    printf("vnc_cliente_unidad: %d/%d comprobaciones correctas\n", total - falhas, total);
    return falhas ? 1 : 0;
}
