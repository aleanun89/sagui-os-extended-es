/* fonte_sistema.c - as fontes do sistema (DejaVu em /fontes) para o motor da web.
 * O motor de fontes em si vive em userland/libfontes/ (D196). */
#include "web.h"

/* Fontes do sistema (DejaVu em /fontes): família genérica x negrito x itálico.
 * Carregadas uma vez por processo e nunca liberadas (servem a todas as páginas). */
static fonte_t *sist[3][2][2]; static uint8_t sist_tentou[3][2][2];
fonte_t *fonte_sistema(int g, int negrito, int italico)
{
    /* NULL no itálico: inclinado na hora a partir da regular/negrito (fonte_obliqua) */
    static const char *const nomes[3][2][2] = {
        { { "/fontes/DejaVuSans.ttf", NULL }, { "/fontes/DejaVuSans-Bold.ttf", NULL } },
        { { "/fontes/DejaVuSerif.ttf", "/fontes/DejaVuSerif-Italic.ttf" }, { "/fontes/DejaVuSerif-Bold.ttf", NULL } },
        { { "/fontes/DejaVuSansMono.ttf", NULL }, { "/fontes/DejaVuSansMono-Bold.ttf", NULL } } };
    if (g < 0 || g > 2) g = 0;
    negrito = !!negrito; italico = !!italico;
    if (sist[g][negrito][italico] || sist_tentou[g][negrito][italico]) return sist[g][negrito][italico];
    sist_tentou[g][negrito][italico] = 1;
    const char *c = nomes[g][negrito][italico];
    if (!c) {
        fonte_t *reta = fonte_sistema(g, negrito, 0);
        sist[g][negrito][italico] = reta ? fonte_obliqua(reta) : NULL;
        return sist[g][negrito][italico];
    }
#ifdef MOTOR_HOST
    char cam[300]; formatar_texto(cam, sizeof cam, "../../rootfs%s", c);
    FILE *fp = fopen(cam, "rb");
    if (!fp) return NULL;
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    uint8_t *d = malloc((size_t)n);
    if (fread(d, 1, (size_t)n, fp) != (size_t)n) { fclose(fp); free(d); return NULL; }
    fclose(fp);
#else
    int fd = abrir(c, 0);
    if (fd < 0) return NULL;
    unsigned cap = 1u << 20, n = 0;
    uint8_t *d = aloca(cap);
    int r;
    while (d && (r = ler(fd, d + n, cap - n)) > 0) {
        n += (unsigned)r;
        if (n == cap) { uint8_t *q = realoca(d, cap * 2); if (!q) break; d = q; cap *= 2; }
    }
    fechar(fd);
    if (!d) return NULL;
#endif
    sist[g][negrito][italico] = fonte_abrir(d, (unsigned)n);
    libera(d);
    return sist[g][negrito][italico];
}

