// SPDX-License-Identifier: GPL-2.0-or-later
/* uvc_testar.c - pruebas del anfitrión de la parte pura del driver UVC (D277).
 *
 * Compila uvc_nucleo.c (kernel/usb), isoent.h, la conversión YUY2 de la libsagui y el
 * decodificador JPEG de la libsagui en el PC (con ASan) y los ejecuta con descriptores y
 * paquetes que genera tests/h277_uvc.py:
 *   desc ARCH                       recorre un descriptor de configuración y lo resume
 *   negociar ARCH FMT QUADRO [mal]  PROBE/COMMIT contra una cámara simulada
 *   montar PAQ SAL L A TIPO         monta cuadros de una lista de paquetes [u16 len][bytes]
 *   padrao L A N SAL                patrón YUY2 -> paquetes -> montador -> el mismo cuadro
 *   isoent                          cola de paquetes recibidos (aleatorio, con invariantes)
 *   fuzz SEMILLA N                  descriptores y paquetes al azar (para ASan)
 *   jpeg ARCH REF.rgb               decodifica un JPEG y da el PSNR contra RGB24 de ffmpeg
 *   yuy2 ARCH L A REF.rgb           convierte YUY2 y da el PSNR contra RGB24 de ffmpeg */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "uvc_nucleo.h"
#include "isoent.h"
#include "camara.h"

/* Lo que camara.c espera de la libsagui (en el anfitrión no hay sistema debajo). */
int info_sistema(int t, void *b, unsigned n) { (void)t; (void)b; (void)n; return -1; }
size_t tam_texto(const char *s) { return strlen(s); }
int compara_texto(const char *a, const char *b) { return strcmp(a, b); }
int abrir(const char *c, int m);
int escrever(int fd, const void *b, unsigned n) { (void)fd; (void)b; return (int)n; }
int imagem_gravar_png(const char *c, const imagem_t *i) { (void)c; (void)i; return 0; }
int imagem_ler_jpeg(const uint8_t *d, int n, imagem_t *img);

static uint8_t *ler_arq(const char *c, long *n)
{
    FILE *f = fopen(c, "rb");
    if (!f) { fprintf(stderr, "no se abre %s\n", c); exit(2); }
    fseek(f, 0, SEEK_END);
    *n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = malloc((size_t)*n + 1);
    if (fread(b, 1, (size_t)*n, f) != (size_t)*n) exit(2);
    fclose(f);
    return b;
}

static int cmd_desc(const char *arq)
{
    long n;
    uint8_t *b = ler_arq(arq, &n);
    uvc_info_t *i = malloc(sizeof *i);
    int r = uvc_analizar(b, (int)n, i);
    printf("formatos %d alts %d bcd %04x vc %d vs %d pu %d controles %x\n", r, i->nalt, i->bcd, i->iface_vc, i->iface_vs, i->pu_id, i->pu_controles);
    for (int k = 0; k < i->nf; k++) {
        printf("f %d tipo %d bits %d nq %d\n", i->f[k].idx, i->f[k].tipo, i->f[k].bits, i->f[k].nq);
        for (int j = 0; j < i->f[k].nq; j++) {
            const uvc_quadro_t *q = &i->f[k].q[j];
            printf("q %d %ux%u max %u def %u n_int %d", q->idx, q->larg, q->alt, q->tam_max, q->intervalo_def, q->n_int);
            for (int t = 0; t < q->n_int; t++) printf(" %u", q->ints[t]);
            printf("\n");
        }
    }
    for (int a = 0; a < i->nalt; a++) printf("alt %d ep %d mps %d mult %d ivl %d\n", i->alt[a].alt, i->alt[a].ep, i->alt[a].mps, i->alt[a].mult, i->alt[a].intervalo);
    const uvc_alt_t *e = uvc_elegir_alt(i, 1500), *g = uvc_elegir_alt(i, 999999);
    if (e) printf("elegir 1500 -> alt %d\n", e->alt);
    if (g) printf("elegir enorme -> alt %d\n", g->alt);
    const uvc_formato_t *f; const uvc_quadro_t *q;
    if (uvc_buscar(i, UVC_FMT_MJPEG, 1280, 720, &f, &q)) printf("buscar mjpeg 1280x720 -> %ux%u\n", q->larg, q->alt);
    if (uvc_buscar(i, UVC_FMT_YUY2, 1000, 600, &f, &q)) printf("buscar yuy2 1000x600 -> %ux%u\n", q->larg, q->alt);
    free(i); free(b);
    return 0;
}

/* Una cámara simulada: guarda el último PROBE y lo devuelve ajustado. */
typedef struct { int tam_ctl_visto; int rechaza; uint8_t probe[34]; int n_pedidos; uint32_t payload; } dev_t_;

static int pedido(void *ctx, uint8_t tipo, uint8_t req, uint16_t valor, uint16_t indice, void *dados, uint16_t n)
{
    dev_t_ *d = ctx;
    d->n_pedidos++;
    printf("pedido tipo %02x req %02x valor %04x indice %04x n %u", tipo, req, valor, indice, n);
    if (req == 0x01) {                                   /* SET_CUR */
        printf(" datos ");
        for (int k = 0; k < n; k++) printf("%02x", ((uint8_t *)dados)[k]);
        if (valor == 0x0100) memcpy(d->probe, dados, n);
    }
    printf("\n");
    if (req == 0x81 && valor == 0x0100) {                /* GET_CUR(PROBE): lo mismo con tamaños */
        uvc_ctl_t c;
        uvc_ctl_leer(d->probe, n, &c);
        if (d->rechaza) c.quadro ^= 1;                   /* la cámara cambia de cuadro: no acepta */
        c.tam_quadro = 614400;
        c.tam_payload = d->payload;
        c.retardo = 32;
        c.reloj = 48000000;
        c.framing = 3;
        uvc_ctl_serializar(&c, dados, n >= 34 ? 34 : 26);
        return n >= 34 ? 34 : 26;
    }
    return n;
}

static int cmd_negociar(int argc, char **argv)
{
    long n;
    uint8_t *b = ler_arq(argv[0], &n);
    uvc_info_t *i = malloc(sizeof *i);
    uvc_analizar(b, (int)n, i);
    dev_t_ d = { 0 };
    d.payload = 1024;
    d.rechaza = argc > 3;
    uvc_ctl_t res;
    int fmt = atoi(argv[1]), quadro = atoi(argv[2]);
    int tam = i->bcd >= 0x0110 ? 34 : 26;
    int r = uvc_negociar(pedido, &d, i->iface_vs, tam, (uint8_t)fmt, (uint8_t)quadro, 333333, &res);
    printf("resultado %d", r);
    if (r == 0) printf(" formato %d quadro %d intervalo %u quadro_tam %u payload %u", res.formato, res.quadro, res.intervalo, res.tam_quadro, res.tam_payload);
    printf("\n");
    free(i); free(b);
    return 0;
}

static int cmd_montar(char **a)
{
    long n;
    uint8_t *b = ler_arq(a[0], &n);
    FILE *out = fopen(a[1], "wb");
    unsigned larg = (unsigned)atoi(a[2]), alt = (unsigned)atoi(a[3]);
    uint8_t tipo = (uint8_t)atoi(a[4]);
    uint32_t cap = 4u << 20;
    uint8_t *b0 = malloc(cap), *b1 = malloc(cap);
    uvc_montador_t m;
    uvc_montador_iniciar(&m, b0, b1, cap, tipo, (uint16_t)larg, (uint16_t)alt);
    uint32_t vistos = 0;
    for (long p = 0; p + 2 <= n;) {
        unsigned len = b[p] | b[p + 1] << 8;
        p += 2;
        if (p + len > n) break;
        uvc_montador_paquete(&m, b + p, len);
        p += len;
        if (m.seq != vistos) {
            vistos = m.seq;
            uint32_t l = m.ult_len;
            fwrite(&l, 4, 1, out);
            fwrite(uvc_montador_ultimo(&m), 1, l, out);
        }
    }
    fclose(out);
    printf("seq %u descartados %u errores %u paquetes %u\n", m.seq, m.descartados, m.errores, m.paquetes);
    free(b0); free(b1); free(b);
    return 0;
}

static int cmd_padrao(char **a)
{
    unsigned larg = (unsigned)atoi(a[0]), alt = (unsigned)atoi(a[1]), cuantos = (unsigned)atoi(a[2]);
    uint32_t tam = larg * alt * 2;
    uint8_t *q = malloc(tam), *b0 = malloc(tam), *b1 = malloc(tam), pac[1020];
    uvc_montador_t m;
    uvc_montador_iniciar(&m, b0, b1, tam, UVC_FMT_YUY2, (uint16_t)larg, (uint16_t)alt);
    unsigned bien = 0;
    (void)0;
    for (unsigned s = 0; s < cuantos; s++) {
        uvc_gerar_padrao(q, (uint16_t)larg, (uint16_t)alt, s);
        uint32_t pos = 0, k;
        uint32_t antes = m.seq;
        while ((k = uvc_gerar_paquete(q, tam, &pos, (uint8_t)(s & 1), sizeof pac, pac))) uvc_montador_paquete(&m, pac, k);
        if (m.seq == antes + 1 && m.ult_len == tam && !memcmp(uvc_montador_ultimo(&m), q, tam)) bien++;
        if (s == 0 && a[3]) { FILE *f = fopen(a[3], "wb"); fwrite(q, 1, tam, f); fclose(f); }
    }
    printf("padrao %u de %u cuadros identicos, descartados %u errores %u\n", bien, cuantos, m.descartados, m.errores);
    free(q); free(b0); free(b1);
    return bien == cuantos ? 0 : 1;
}

static uint32_t rng_estado = 1;
static uint32_t rng(void) { rng_estado = rng_estado * 1664525u + 1013904223u; return rng_estado >> 8; }

static int cmd_isoent(void)
{
    static isoent_t r;
    for (uint32_t ronda = 0; ronda < 200; ronda++) {
        uint32_t slot = 64 + rng() % 3000, nc = 2 + rng() % 500;
        if (!isoent_iniciar(&r, nc * slot, slot)) { printf("falla iniciar\n"); return 1; }
        uint32_t en_voo = 0, leidos = 0, enviados = 0;
        uint32_t esperado[ISOENT_CASILLAS_MAX * 4];
        uint32_t pos_off[ISOENT_CASILLAS_MAX * 4];
        for (uint32_t paso = 0; paso < 5000; paso++) {
            uint32_t op = rng() % 3;
            if (op == 0 && isoent_livres(&r) > 0) {
                uint32_t off = isoent_postar(&r);
                if (off % slot || off >= nc * slot) { printf("offset malo\n"); return 1; }
                pos_off[enviados % (ISOENT_CASILLAS_MAX * 4)] = off;
                enviados++; en_voo++;
            } else if (op == 1 && en_voo > 0) {
                uint32_t len = rng() % (slot + 50);
                esperado[(enviados - en_voo) % (ISOENT_CASILLAS_MAX * 4)] = len > slot ? slot : len;
                isoent_completar(&r, len, (rng() % 13) == 0);
                en_voo--;
            } else if (op == 2) {
                uint32_t off, len; bool erro;
                if (isoent_ler(&r, &off, &len, &erro)) {
                    if (off != pos_off[leidos % (ISOENT_CASILLAS_MAX * 4)]) { printf("orden roto\n"); return 1; }
                    if (!erro && len != esperado[leidos % (ISOENT_CASILLAS_MAX * 4)]) { printf("longitud rota\n"); return 1; }
                    leidos++;
                }
            }
            if (r.postados - r.lidos > r.ncasillas) { printf("desbordado\n"); return 1; }
            if (isoent_livres(&r) + (r.postados - r.lidos) != r.ncasillas) { printf("cuenta rota\n"); return 1; }
        }
    }
    printf("isoent ok\n");
    return 0;
}

static int cmd_fuzz(char **a)
{
    rng_estado = (uint32_t)atoi(a[0]);
    int rondas = atoi(a[1]);
    uint8_t d[600], pac[200];
    uvc_info_t *i = malloc(sizeof *i);
    uint8_t *b0 = malloc(70000), *b1 = malloc(70000);
    for (int r = 0; r < rondas; r++) {
        int n = (int)(rng() % sizeof d);
        for (int k = 0; k < n; k++) d[k] = (uint8_t)rng();
        /* con estructura: a veces descriptores válidos de verdad con longitudes mentirosas */
        for (int k = 0; k + 8 < n; k += 8 + (int)(rng() % 20)) { d[k] = (uint8_t)(2 + rng() % 40); d[k + 1] = (uint8_t)((rng() & 1) ? 0x24 : (rng() % 6)); }
        uvc_analizar(d, n, i);
        uvc_montador_t m;
        uvc_montador_iniciar(&m, b0, b1, 70000, (rng() & 1) ? UVC_FMT_MJPEG : UVC_FMT_YUY2, 160, 120);
        for (int p = 0; p < 300; p++) {
            uint32_t l = rng() % sizeof pac;
            for (uint32_t k = 0; k < l; k++) pac[k] = (uint8_t)rng();
            if (l > 2 && (rng() & 3)) { pac[0] = (uint8_t)(2 + rng() % 12); pac[1] = (uint8_t)(rng() & 3 ? (rng() & 0x8F) : 0xFF); }
            uvc_montador_paquete(&m, pac, l);
        }
        uvc_ctl_t c; uint8_t buf[40];
        uvc_ctl_leer(d, n, &c);
        uvc_ctl_serializar(&c, buf, 26);
        uvc_ctl_serializar(&c, buf, 34);
        uvc_cuadro_valido(UVC_FMT_MJPEG, 16, 16, d, (uint32_t)n);
    }
    printf("fuzz ok %d rondas\n", rondas);
    free(i); free(b0); free(b1);
    return 0;
}

static double psnr(const uint8_t *a, const uint8_t *b, size_t n)
{
    double s = 0;
    for (size_t k = 0; k < n; k++) { double d = (double)a[k] - b[k]; s += d * d; }
    if (s == 0) return 99;
    return 10 * log10(255.0 * 255.0 * (double)n / s);
}

static void px_a_rgb24(const imagem_t *img, uint8_t *o)
{
    for (int k = 0; k < img->larg * img->alt; k++) { o[3 * k] = (uint8_t)(img->px[k] >> 16); o[3 * k + 1] = (uint8_t)(img->px[k] >> 8); o[3 * k + 2] = (uint8_t)img->px[k]; }
}

static int cmd_jpeg(char **a)
{
    long n, nr;
    uint8_t *d = ler_arq(a[0], &n), *ref = ler_arq(a[1], &nr);
    imagem_t img;
    int r = camara_a_imagem(d, (unsigned)n, 0, 0, CAM_FMT_MJPEG, &img);
    if (r < 0) { printf("jpeg error %d\n", r); return 1; }
    uint8_t *o = malloc((size_t)img.larg * img.alt * 3);
    px_a_rgb24(&img, o);
    printf("jpeg %dx%d psnr %.2f\n", img.larg, img.alt, (size_t)nr == (size_t)img.larg * img.alt * 3 ? psnr(o, ref, (size_t)nr) : -1.0);
    imagem_liberar(&img);
    free(o); free(d); free(ref);
    return 0;
}

static int cmd_yuy2(char **a)
{
    long n, nr;
    uint8_t *d = ler_arq(a[0], &n), *ref = ler_arq(a[3], &nr);
    unsigned larg = (unsigned)atoi(a[1]), alt = (unsigned)atoi(a[2]);
    imagem_t img;
    int r = camara_a_imagem(d, (unsigned)n, larg, alt, CAM_FMT_YUY2, &img);
    if (r < 0) { printf("yuy2 error %d\n", r); return 1; }
    uint8_t *o = malloc((size_t)larg * alt * 3);
    px_a_rgb24(&img, o);
    printf("yuy2 %ux%u psnr %.2f\n", larg, alt, (size_t)nr == (size_t)larg * alt * 3 ? psnr(o, ref, (size_t)nr) : -1.0);
    imagem_liberar(&img);
    free(o); free(d); free(ref);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) return 2;
    const char *c = argv[1];
    if (!strcmp(c, "desc") && argc >= 3) return cmd_desc(argv[2]);
    if (!strcmp(c, "negociar") && argc >= 5) return cmd_negociar(argc - 2, argv + 2);
    if (!strcmp(c, "montar") && argc >= 7) return cmd_montar(argv + 2);
    if (!strcmp(c, "padrao") && argc >= 5) { char *a[4] = { argv[2], argv[3], argv[4], argc > 5 ? argv[5] : NULL }; return cmd_padrao(a); }
    if (!strcmp(c, "isoent")) return cmd_isoent();
    if (!strcmp(c, "fuzz") && argc >= 4) return cmd_fuzz(argv + 2);
    if (!strcmp(c, "jpeg") && argc >= 4) return cmd_jpeg(argv + 2);
    if (!strcmp(c, "yuy2") && argc >= 6) return cmd_yuy2(argv + 2);
    return 2;
}
