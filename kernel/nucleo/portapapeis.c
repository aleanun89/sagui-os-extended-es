// SPDX-License-Identifier: GPL-2.0-or-later
/* portapapeis.c - el portapapeles del sistema (D074).
 *
 * Un único texto, el último que algún programa copió, de hasta 64 KB en
 * Latin-1 (como todo el texto del SaguiOS). Queda en el kernel porque es el único
 * lugar que vive más que los programas: el Editor copia, cierra, y el Mico
 * todavía pega. No hay formatos, ni dueño, ni aviso de cambio: quien pega
 * pregunta lo que hay en el momento (SIS_COLAR_TEXTO).
 *
 * El buffer es estático (64 KB en la BSS): asignar en el heap del kernel en cada copia
 * no gana nada con un límite tan pequeño, y así pegar nunca falla por
 * falta de memoria. Todo corre con las interrupciones desactivadas (llamada de
 * sistema), así que no hay bloqueos. */
#include "sistema.h"
#include "texto.h"
#include "console.h"
#include "memoria.h"

static char     texto[PORTAPAPEIS_MAX];
static uint32_t tamanho;
static uint32_t copias;   /* cuántas veces ya se copió (sólo para el log) */

/* SIS_COPIAR_TEXTO (buf, n) -> bytes guardados. Lo que pase de
 * PORTAPAPEIS_MAX se corta; n = 0 vacía. */
int32_t sis_copiar_texto(uint32_t buf, uint32_t n, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    if (n > PORTAPAPEIS_MAX) n = PORTAPAPEIS_MAX;
    if (!usuario_ok((const void *)buf, n, false)) return ERRO_ENDERECO;
    memcpy(texto, (const void *)buf, n);
    tamanho = n;
    copias++;
    klog("[PORTAPAPELES] copia %u: %u bytes\n", copias, n);
    return (int32_t)n;
}

/* SIS_COLAR_TEXTO (buf, max) -> tamaño del texto guardado (puede pasar de
 * max: copia sólo max bytes, sin cero al final). buf = 0 y max = 0 sólo pregunta
 * el tamaño. */
int32_t sis_colar_texto(uint32_t buf, uint32_t max, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    uint32_t n = tamanho < max ? tamanho : max;
    if (!usuario_ok((void *)buf, n, true)) return ERRO_ENDERECO;
    memcpy((void *)buf, texto, n);
    return (int32_t)tamanho;
}

/* ---- imagen (D125): la última imagen copiada (Recortes), en el heap del kernel.
 * Hasta PORTAPAPEIS_IMG_PX píxeles; si falta memoria, la copia falla y se conserva la anterior. */
static uint32_t *img_px;
static uint32_t  img_l, img_a;

/* SIS_COPIAR_IMAGEM (ancho | alto << 16, píxeles) -> 0. Ancho y alto 0 vacía. */
int32_t sis_copiar_imagem(uint32_t dims, uint32_t pix, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    uint32_t l = dims & 0xFFFF, a = dims >> 16;
    if (l == 0 || a == 0) {
        kfree(img_px); img_px = NULL; img_l = img_a = 0;
        return 0;
    }
    if (l > 8192 || a > 8192 || l * a > PORTAPAPEIS_IMG_PX) return ERRO_INVALIDO;
    if (!usuario_ok((const void *)pix, l * a * 4, false)) return ERRO_ENDERECO;
    uint32_t *n = kmalloc((size_t)l * a * 4);
    if (!n) return ERRO_SEM_MEMORIA;
    memcpy(n, (const void *)pix, (size_t)l * a * 4);
    kfree(img_px);
    img_px = n; img_l = l; img_a = a;
    klog("[PORTAPAPELES] imagen %ux%u\n", l, a);
    return 0;
}

/* SIS_COLAR_IMAGEM (destino, max_pixels) -> ancho | alto << 16; 0 = no hay imagen. Con destino 0 sólo pregunta. */
int32_t sis_colar_imagem(uint32_t dst, uint32_t maxpx, uint32_t c, uint32_t d, uint32_t e)
{
    (void)c; (void)d; (void)e;
    if (!img_px) return 0;
    if (dst) {
        if (img_l * img_a > maxpx) return ERRO_INVALIDO;
        if (!usuario_ok((void *)dst, img_l * img_a * 4, true)) return ERRO_ENDERECO;
        memcpy((void *)dst, img_px, (size_t)img_l * img_a * 4);
    }
    return (int32_t)(img_l | (img_a << 16));
}
