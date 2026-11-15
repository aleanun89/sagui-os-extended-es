// SPDX-License-Identifier: GPL-2.0-or-later
/* camara.c - cámaras de la libsagui (D277). Ver camara.h. */
#include "camara.h"

static int llamar(info_camara_t *i)
{
    int r = info_sistema(INFO_CAMARA, i, sizeof *i);
    return r;
}

int camara_estado(unsigned fuente, info_camara_t *i)
{
    memset(i, 0, sizeof *i);
    i->orden = CAM_ESTADO;
    i->fuente = fuente;
    return llamar(i);
}

int camara_abrir(unsigned fuente, unsigned modo, info_camara_t *i)
{
    memset(i, 0, sizeof *i);
    i->orden = CAM_ABRIR;
    i->fuente = fuente;
    i->modo = modo;
    return llamar(i);
}

int camara_cuadro(info_camara_t *i, void *buf, unsigned tam, unsigned seq_visto)
{
    i->orden = CAM_CUADRO;
    i->buf = buf;
    i->buf_tam = tam;
    i->seq_visto = seq_visto;
    return llamar(i);
}

int camara_cerrar(void)
{
    info_camara_t i;
    memset(&i, 0, sizeof i);
    i.orden = CAM_CERRAR;
    return llamar(&i);
}

int camara_ajuste(unsigned control, bool escribir, int valor, info_camara_t *i)
{
    memset(i, 0, sizeof *i);
    i->orden = CAM_AJUSTE;
    i->control = control;
    i->escribir = escribir ? 1 : 0;
    i->valor = valor;
    return llamar(i);
}

const char *camara_nombre_formato(unsigned formato)
{
    return formato == CAM_FMT_MJPEG ? "MJPEG" : formato == CAM_FMT_YUY2 ? "YUY2" : "?";
}

static inline int fijar(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

void camara_yuy2_a_rgb(const uint8_t *yuy2, unsigned larg, unsigned alt, uint32_t *dst)
{
    for (unsigned y = 0; y < alt; y++) {
        const uint8_t *p = yuy2 + (size_t)y * larg * 2;
        uint32_t *o = dst + (size_t)y * larg;
        for (unsigned x = 0; x + 1 < larg + 1; x += 2) {
            int U = p[1] - 128, V = p[3] - 128;
            int rv = 409 * V + 128, gv = -100 * U - 208 * V + 128, bu = 516 * U + 128;
            for (int k = 0; k < 2 && x + (unsigned)k < larg; k++) {
                int Y = 298 * (p[k * 2] - 16);
                o[x + k] = (uint32_t)fijar((Y + rv) >> 8) << 16 | (uint32_t)fijar((Y + gv) >> 8) << 8 | (uint32_t)fijar((Y + bu) >> 8);
            }
            p += 4;
        }
    }
}

int camara_a_imagem(const void *dados, unsigned n, unsigned larg, unsigned alt, unsigned formato, imagem_t *img)
{
    if (formato == CAM_FMT_MJPEG) return imagem_ler_jpeg((const uint8_t *)dados, (int)n, img);
    if (formato != CAM_FMT_YUY2 || n < larg * alt * 2u) return IMG_ERRO_FORMATO;
    if (!imagem_criar(img, (int)larg, (int)alt, 0)) return IMG_ERRO_MEMORIA;
    camara_yuy2_a_rgb((const uint8_t *)dados, larg, alt, img->px);
    copia_texto(img->formato, "YUY2", sizeof img->formato);
    return 0;
}

int camara_guardar(const char *caminho, const void *dados, unsigned n, unsigned larg, unsigned alt, unsigned formato)
{
    size_t m = tam_texto(caminho);
    bool jpg = m > 4 && (!compara_texto(caminho + m - 4, ".jpg") || !compara_texto(caminho + m - 4, ".JPG"));
    if (!jpg && m > 5 && (!compara_texto(caminho + m - 5, ".jpeg") || !compara_texto(caminho + m - 5, ".JPEG"))) jpg = true;
    if (jpg) {
        if (formato != CAM_FMT_MJPEG) return IMG_ERRO_NAO_SUPORTA;       /* sin codificador JPEG */
        int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
        if (fd < 0) return fd;
        int r = escrever(fd, dados, n);
        fechar(fd);
        return r == (int)n ? 0 : ERRO_INVALIDO;
    }
    imagem_t img;
    int r = camara_a_imagem(dados, n, larg, alt, formato, &img);
    if (r < 0) return r;
    r = imagem_gravar_png(caminho, &img);
    imagem_liberar(&img);
    return r;
}
