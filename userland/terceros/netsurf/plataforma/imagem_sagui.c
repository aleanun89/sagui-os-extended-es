// SPDX-License-Identifier: GPL-2.0-or-later
/* imagem_sagui.c - PNG y JPEG en NetSurf con los decodificadores de la
 * libsagui (D184).
 *
 * NetSurf lee PNG con libpng y JPEG con libjpeg, que el SaguiOS no tiene.
 * La libsagui sí sabe leer los dos formatos (imagem.c, D054; imagem_jpeg.c),
 * pero con su propia interfaz: imagem_decodificar() da el dibujo entero en
 * 0x00RRGGBB. Este manejador de contenido es el de jpeg.c del upstream con
 * esa llamada en lugar de libjpeg: comprueba la cabecera al acabar la
 * descarga y decodifica cuando hace falta dibujar (image_cache).
 *
 * Limitación: la libsagui no conserva el canal alfa del PNG (las zonas
 * transparentes salen opacas), ni decodifica a medida que llegan los datos.
 */
#include "sagui.h"
#include "imagem.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "utils/utils.h"
#include "utils/messages.h"
#include "netsurf/bitmap.h"
#include "content/llcache.h"
#include "content/content.h"
#include "content/content_protected.h"
#include "content/content_factory.h"
#include "desktop/gui_internal.h"
#include "desktop/bitmap.h"
#include "image/image_cache.h"

static nserror nssagui_create(const content_handler *handler, lwc_string *imime_type,
                              const struct http_parameter *params, llcache_handle *llcache,
                              const char *fallback_charset, bool quirks, struct content **c)
{
    struct content *img = calloc(1, sizeof(struct content));
    if (img == NULL) return NSERROR_NOMEM;
    nserror error = content__init(img, handler, imime_type, params, llcache, fallback_charset, quirks);
    if (error != NSERROR_OK) {
        free(img);
        return error;
    }
    *c = img;
    return NSERROR_OK;
}

/* Ancho y alto sin decodificar: de la cabecera del PNG (IHDR) o de un SOFn
 * del JPEG. */
static bool medidas(const uint8_t *d, size_t n, int *l, int *a)
{
    if (n >= 24 && memcmp(d, "\x89PNG\r\n\x1a\n", 8) == 0) {
        *l = (int)((uint32_t)d[16] << 24 | d[17] << 16 | d[18] << 8 | d[19]);
        *a = (int)((uint32_t)d[20] << 24 | d[21] << 16 | d[22] << 8 | d[23]);
        return *l > 0 && *a > 0;
    }
    if (n >= 4 && d[0] == 0xFF && d[1] == 0xD8) {
        size_t p = 2;
        while (p + 9 < n) {
            if (d[p] != 0xFF) { p++; continue; }
            uint8_t m = d[p + 1];
            if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7)) { p += 2; continue; }
            size_t tam = (size_t)(d[p + 2] << 8 | d[p + 3]);
            if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
                *a = d[p + 5] << 8 | d[p + 6];
                *l = d[p + 7] << 8 | d[p + 8];
                return *l > 0 && *a > 0;
            }
            p += 2 + tam;
        }
    }
    return false;
}

static struct bitmap *nssagui_cache_convert(struct content *c)
{
    size_t n;
    const uint8_t *dados = content__get_source_data(c, &n);
    if (dados == NULL || n == 0) return NULL;
    imagem_t img;
    memset(&img, 0, sizeof img);
    int r = imagem_decodificar(dados, (int)n, &img);
    if (r != 0) {
        registrar("[NETSURF] imagen no decodificada: %s\n", imagem_erro(r));
        return NULL;
    }
    struct bitmap *bitmap = guit->bitmap->create(img.larg, img.alt, BITMAP_OPAQUE);
    if (bitmap == NULL) { imagem_liberar(&img); return NULL; }
    uint8_t *px = guit->bitmap->get_buffer(bitmap);
    if (px == NULL) {
        guit->bitmap->destroy(bitmap);
        imagem_liberar(&img);
        return NULL;
    }
    size_t passo = guit->bitmap->get_rowstride(bitmap);
    for (int y = 0; y < img.alt; y++) {
        uint8_t *linha = px + passo * (size_t)y;
        const uint32_t *o = img.px + (size_t)y * (size_t)img.larg;
        for (int x = 0; x < img.larg; x++) {
            uint32_t v = o[x];
            linha[4 * x + 0] = (uint8_t)(v >> 16);
            linha[4 * x + 1] = (uint8_t)(v >> 8);
            linha[4 * x + 2] = (uint8_t)v;
            linha[4 * x + 3] = 0xFF;
        }
    }
    imagem_liberar(&img);
    bitmap_fmt_t fmt = { .layout = BITMAP_LAYOUT_R8G8B8A8, .pma = false };
    bitmap_format_to_client(bitmap, &fmt);
    guit->bitmap->modified(bitmap);
    return bitmap;
}

static bool nssagui_convert(struct content *c)
{
    size_t n;
    const uint8_t *dados = content__get_source_data(c, &n);
    int l, a;
    if (!medidas(dados, n, &l, &a)) {
        union content_msg_data msg;
        msg.errordata.errorcode = NSERROR_UNKNOWN;
        msg.errordata.errormsg = "Imagen no válida";
        content_broadcast(c, CONTENT_MSG_ERROR, &msg);
        return false;
    }
    c->width = l;
    c->height = a;
    c->size = (size_t)l * (size_t)a * 4;
    image_cache_add(c, NULL, nssagui_cache_convert);
    content_set_ready(c);
    content_set_done(c);
    content_set_status(c, "");
    return true;
}

static nserror nssagui_clone(const struct content *old, struct content **newc)
{
    struct content *n = calloc(1, sizeof(struct content));
    if (n == NULL) return NSERROR_NOMEM;
    nserror error = content__clone(old, n);
    if (error != NSERROR_OK) {
        content_destroy(n);
        return error;
    }
    if ((old->status == CONTENT_STATUS_READY || old->status == CONTENT_STATUS_DONE) &&
        !nssagui_convert(n)) {
        content_destroy(n);
        return NSERROR_CLONE_FAILED;
    }
    *newc = n;
    return NSERROR_OK;
}

static const content_handler nssagui_content_handler = {
    .create = nssagui_create,
    .data_complete = nssagui_convert,
    .destroy = image_cache_destroy,
    .redraw = image_cache_redraw,
    .clone = nssagui_clone,
    .get_internal = image_cache_get_internal,
    .type = image_cache_content_type,
    .is_opaque = image_cache_is_opaque,
    .no_share = false,
};

static const char *nssagui_types[] = {
    "image/png", "image/x-png", "image/jpeg", "image/jpg", "image/pjpeg",
};

CONTENT_FACTORY_REGISTER_TYPES(nssagui, nssagui_types, nssagui_content_handler);
