// SPDX-License-Identifier: GPL-2.0-or-later
/* video_mp4.c - el MP4 (H.264 + AAC-LC) detrás de la API de video.h (mejora 100, D276).
 *
 * El lector de contenedor (mp4.c), el H.264 (h264.c) y el AAC (aac.c) son de
 * robson-apk (PR #6 de erigobeli/sagui-os); aquí está solo el pegamento:
 *
 *  - la entrada: un archivo (se lee únicamente el «moov» y cada muestra con
 *    posicionar+leer, así un vídeo grande no ocupa la RAM) o un búfer en
 *    memoria (el Cipó, que ya lo bajó);
 *  - el vídeo: cada muestra pasa a h264_amostra(); los cuadros salen en orden
 *    de exhibición (los B se reordenan dentro del decodificador) y se pasan
 *    de YUV 4:2:0 a 0x00RRGGBB (BT.601), enteros o a la mitad de tamaño;
 *  - el audio: AAC-LC a estéreo de 16 bits a la frecuencia del archivo; el
 *    PCM se entrega «por delante» del cuadro que se muestra (unos 250 ms) para
 *    que la tarjeta no se quede sin datos;
 *  - la búsqueda: el decodificador no tiene «vaciar», así que se crea de nuevo
 *    y se decodifica desde el principio hasta el cuadro pedido (barato para
 *    volver a 0, que es lo que hace el bucle; lento para saltos largos).
 *
 * Solo hay un vídeo abierto por programa (como en video.c). Usa x87 por el AAC
 * (aac.o se compila con la FPU, ver el Makefile). */
#include "video_mp4.h"
#ifdef SAGUI_SEM_GPL
/* Variante sin GPL de terceros (D109, D258): las tablas del H.264 (FFmpeg, LGPL) y del AAC (FAAD2,
 * GPL) no entran, así que no hay MP4: video.c lo trata como un formato desconocido. */
bool vmp4_reconhece(const uint8_t *c) { (void)c; return false; }
vmp4_t *vmp4_abrir_arquivo(const char *c, video_info_t *i, int *e) { (void)c; (void)i; *e = VIDEO_ERRO_NAO_SUPORTADO; return NULL; }
vmp4_t *vmp4_abrir_memoria(const uint8_t *d, uint32_t n, video_info_t *i, int *e) { (void)d; (void)n; (void)i; *e = VIDEO_ERRO_NAO_SUPORTADO; return NULL; }
int  vmp4_quadro(vmp4_t *v, imagem_t *img) { (void)v; (void)img; return VIDEO_ERRO_FIM; }
int  vmp4_audio(vmp4_t *v, int16_t *pcm, int max) { (void)v; (void)pcm; (void)max; return 0; }
int  vmp4_buscar(vmp4_t *v, int q) { (void)v; (void)q; return VIDEO_ERRO_NAO_SUPORTADO; }
int  vmp4_quadro_atual(const vmp4_t *v) { (void)v; return -1; }
void vmp4_reducao(vmp4_t *v, int f) { (void)v; (void)f; }
void vmp4_fechar(vmp4_t *v) { (void)v; }
#else
#include "mp4.h"
#include "h264.h"
#include "aac.h"

#define MP4_LEAD_AUDIO_MS 250        /* cuánto audio se entrega por delante del cuadro */
#define MP4_MOOV_MAX (64u << 20)     /* moov más grande que esto: se rechaza */

struct vmp4 {
    int fd;                      /* -1 = en memoria */
    const uint8_t *mem;          /* datos en memoria (o NULL) */
    uint32_t mem_n;
    uint8_t *moov;               /* copia del moov leído de archivo (avcc/asc apuntan aquí) */
    uint8_t *tmp;                /* búfer de una muestra leída de archivo */
    uint32_t tmp_cap;
    mp4_t m;
    uint8_t avcc[512], asc[64];
    int avcc_n, asc_n;

    h264_t *h;
    int tam_len;
    aac_t *a;
    int iv;                      /* próxima muestra de vídeo que se alimenta */
    int quadro;                  /* cuadros entregados - 1 */
    bool esvaziado;
    int fps;
    int reducao;

    int ia;                      /* próximo cuadro AAC por decodificar */
    int16_t pcm[2048];           /* un cuadro AAC: 1024 muestras estéreo */
    int pcm_n, pcm_pos;          /* cuadros estéreo en pcm y cuántos ya se entregaron */
    uint32_t taxa;
};

static uint32_t be32(const uint8_t *p) { return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3]; }

bool vmp4_reconhece(const uint8_t *c)
{
    const uint8_t *t = c + 4;
    return (t[0] == 'f' && t[1] == 't' && t[2] == 'y' && t[3] == 'p')
        || (t[0] == 'm' && t[1] == 'o' && t[2] == 'o' && t[3] == 'v')
        || (t[0] == 'm' && t[1] == 'd' && t[2] == 'a' && t[3] == 't')
        || (t[0] == 'f' && t[1] == 'r' && t[2] == 'e' && t[3] == 'e');
}

/* milisegundos de un tiempo en la escala de la pista, sin 64 bits */
static uint32_t en_ms(uint32_t t, uint32_t esc)
{
    if (!esc) esc = 1000;
    return t / esc * 1000 + t % esc * 1000 / esc;
}

/* Los bytes de una muestra: de memoria directo, de archivo a un búfer propio. */
static const uint8_t *muestra(vmp4_t *v, uint32_t pos, uint32_t tam)
{
    if (v->mem) return (pos <= v->mem_n && tam <= v->mem_n - pos) ? v->mem + pos : NULL;
    if (tam > v->tmp_cap) {
        uint8_t *n = realoca(v->tmp, tam + 64);
        if (!n) return NULL;
        v->tmp = n;
        v->tmp_cap = tam + 64;
    }
    if (posicionar(v->fd, (int)pos, POS_INICIO) < 0) return NULL;
    uint32_t hecho = 0;
    while (hecho < tam) {
        int r = ler(v->fd, v->tmp + hecho, tam - hecho);
        if (r <= 0) return NULL;
        hecho += (uint32_t)r;
    }
    return v->tmp;
}

static bool crear_h264(vmp4_t *v)
{
    if (v->h) { h264_fim(v->h); v->h = NULL; }
    v->h = h264_novo();
    if (!v->h) return false;
    if (h264_config(v->h, v->avcc, v->avcc_n, &v->tam_len) < 0) { h264_fim(v->h); v->h = NULL; return false; }
    v->iv = 0;
    v->esvaziado = false;
    return true;
}

/* Rellena info y arranca los decodificadores; v->m ya está leído. */
static bool arrancar(vmp4_t *v, video_info_t *info, int *erro)
{
    mp4_t *m = &v->m;
    if (m->avcc && m->avcc_n) {
        v->avcc_n = (int)(m->avcc_n < sizeof v->avcc ? m->avcc_n : sizeof v->avcc);
        memcpy(v->avcc, m->avcc, (size_t)v->avcc_n);
    }
    if (m->asc && m->asc_n) {
        v->asc_n = (int)(m->asc_n < sizeof v->asc ? m->asc_n : sizeof v->asc);
        memcpy(v->asc, m->asc, (size_t)v->asc_n);
    }
    if (!m->n_video || !v->avcc_n) { *erro = VIDEO_ERRO_NAO_SUPORTADO; return false; }
    if (!crear_h264(v)) { *erro = VIDEO_ERRO_NAO_SUPORTADO; return false; }   /* H.264 High, etc. */
    if (m->n_audio && v->asc_n) v->a = aac_novo(v->asc, v->asc_n);   /* sin AAC-LC: va mudo */

    memset(info, 0, sizeof *info);
    info->larg = m->larg;
    info->alt = m->alt;
    info->quadros = m->n_video;
    /* cuadros por segundo: de la duración real de las marcas de tiempo */
    uint32_t t0 = m->video[0].tempo, t1 = m->video[m->n_video - 1].tempo;
    uint32_t esc = m->escala_video ? m->escala_video : 1000;
    uint32_t dur = en_ms(t1 - t0, esc);
    if (m->n_video > 1 && dur) {
        uint32_t paso_ms10 = dur * 10 / (uint32_t)(m->n_video - 1);   /* décimas de ms por cuadro */
        v->fps = (int)((10000u + paso_ms10 / 2) / (paso_ms10 ? paso_ms10 : 1));
    }
    if (v->fps < 1) v->fps = 25;
    info->fps = v->fps;
    info->duracao_ms = (uint32_t)m->n_video * 1000u / (uint32_t)v->fps;
    if (v->a) {
        info->tem_audio = true;
        info->taxa_audio = v->taxa = (uint32_t)aac_taxa(v->a);
        info->canais_audio = 2;   /* aac_quadro siempre da estéreo */
    }
    info->nome_formato = "H.264/AAC en MP4";
    v->quadro = -1;
    v->reducao = 1;
    return true;
}

vmp4_t *vmp4_abrir_memoria(const uint8_t *dados, uint32_t n, video_info_t *info, int *erro)
{
    int e = 0;
    vmp4_t *v = aloca_zerado(sizeof *v);
    if (!v) { *erro = VIDEO_ERRO_MEMORIA; return NULL; }
    v->fd = -1;
    v->mem = dados;
    v->mem_n = n;
    int r = mp4_ler(dados, n, &v->m);
    if (r != 0) { e = r > 0 ? VIDEO_ERRO_CORROMPIDO : VIDEO_ERRO_NAO_SUPORTADO; goto falha; }
    if (!arrancar(v, info, &e)) { mp4_fim(&v->m); goto falha; }
    return v;
falha:
    *erro = e;
    libera(v);
    return NULL;
}

vmp4_t *vmp4_abrir_arquivo(const char *caminho, video_info_t *info, int *erro)
{
    int e = 0;
    vmp4_t *v = aloca_zerado(sizeof *v);
    if (!v) { *erro = VIDEO_ERRO_MEMORIA; return NULL; }
    v->fd = abrir(caminho, ABRIR_LER);
    if (v->fd < 0) { *erro = v->fd; libera(v); return NULL; }
    int fim = posicionar(v->fd, 0, POS_FIM);
    uint32_t total = fim > 0 ? (uint32_t)fim : 0;

    /* Recorre las cajas de primer nivel hasta el moov (puede estar al final). */
    uint32_t pos = 0, moov_pos = 0, moov_n = 0;
    while (pos + 8 <= total) {
        uint8_t ch[16];
        if (posicionar(v->fd, (int)pos, POS_INICIO) < 0) break;
        if (ler(v->fd, ch, 8) != 8) break;
        uint32_t tam = be32(ch);
        if (tam == 1) {                      /* tamaño de 64 bits: solo la parte baja */
            if (ler(v->fd, ch + 8, 8) != 8 || be32(ch + 8)) break;
            tam = be32(ch + 12);
        } else if (tam == 0) tam = total - pos;
        if (tam < 8) break;
        if (ch[4] == 'm' && ch[5] == 'o' && ch[6] == 'o' && ch[7] == 'v') { moov_pos = pos; moov_n = tam; break; }
        if (tam > total - pos) break;
        pos += tam;
    }
    if (!moov_n || moov_n > MP4_MOOV_MAX || moov_n > total - moov_pos) { e = VIDEO_ERRO_FORMATO; goto falha; }
    v->moov = aloca(moov_n + 16);
    if (!v->moov) { e = VIDEO_ERRO_MEMORIA; goto falha; }
    if (posicionar(v->fd, (int)moov_pos, POS_INICIO) < 0) { e = VIDEO_ERRO_CORROMPIDO; goto falha; }
    uint32_t hecho = 0;
    while (hecho < moov_n) {
        int r = ler(v->fd, v->moov + hecho, moov_n - hecho);
        if (r <= 0) { e = VIDEO_ERRO_CORROMPIDO; goto falha; }
        hecho += (uint32_t)r;
    }
    int r = mp4_ler(v->moov, moov_n, &v->m);
    if (r != 0) { e = r > 0 ? VIDEO_ERRO_CORROMPIDO : VIDEO_ERRO_NAO_SUPORTADO; goto falha; }
    if (!arrancar(v, info, &e)) { mp4_fim(&v->m); goto falha; }
    return v;
falha:
    *erro = e;
    if (v->fd >= 0) fechar(v->fd);
    if (v->moov) libera(v->moov);
    libera(v);
    return NULL;
}

/* ---------------------------------------------------------------- vídeo */

static int sat8(int v) { return v < 0 ? 0 : v > 255 ? 255 : v; }

/* YUV 4:2:0 -> 0x00RRGGBB (BT.601, rango limitado), con reducción 1 o 2. */
static void yuv_a_rgb(const h264_imagem_t *im, uint32_t *dst, int fator)
{
    static int tab_y[256], tab_rv[256], tab_gu[256], tab_gv[256], tab_bu[256];
    static bool listo;
    if (!listo) {
        for (int i = 0; i < 256; i++) {
            tab_y[i] = 298 * (i - 16) + 128;
            tab_rv[i] = 409 * (i - 128);
            tab_gu[i] = -100 * (i - 128);
            tab_gv[i] = -208 * (i - 128);
            tab_bu[i] = 516 * (i - 128);
        }
        listo = true;
    }
    int l = im->larg / fator, a = im->alt / fator;
    for (int y = 0; y < a; y++) {
        int sy = y * fator;
        const uint8_t *py = im->y + sy * im->passo_y;
        const uint8_t *pu = im->u + (sy / 2) * im->passo_c, *pv = im->v + (sy / 2) * im->passo_c;
        uint32_t *d = dst + y * l;
        for (int x = 0; x < l; x++) {
            int sx = x * fator, Y = tab_y[py[sx]], U = pu[sx >> 1], V = pv[sx >> 1];
            d[x] = ((uint32_t)sat8((Y + tab_rv[V]) >> 8) << 16) | ((uint32_t)sat8((Y + tab_gu[U] + tab_gv[V]) >> 8) << 8)
                 | (uint32_t)sat8((Y + tab_bu[U]) >> 8);
        }
    }
}

static int entregar(vmp4_t *v, const h264_imagem_t *im, imagem_t *img)
{
    v->quadro++;
    if (!img) return 0;
    int l = im->larg / v->reducao, a = im->alt / v->reducao;
    if (l < 1 || a < 1) return VIDEO_ERRO_CORROMPIDO;
    if (!img->px || img->larg != l || img->alt != a) {
        if (img->px) imagem_liberar(img);
        if (!imagem_criar(img, l, a, 0)) return VIDEO_ERRO_MEMORIA;
    }
    yuv_a_rgb(im, img->px, v->reducao);
    copia_texto(img->formato, "H.264", sizeof img->formato);
    return 0;
}

int vmp4_quadro(vmp4_t *v, imagem_t *img)
{
    h264_imagem_t im;
    for (;;) {
        if (h264_imagem(v->h, &im)) return entregar(v, &im, img);
        if (v->iv < v->m.n_video) {
            mp4_amostra_t *s = &v->m.video[v->iv++];
            const uint8_t *d = muestra(v, s->pos, s->tam);
            if (!d) return VIDEO_ERRO_CORROMPIDO;
            h264_amostra(v->h, d, (int)s->tam, v->tam_len);   /* un cuadro dañado no corta el vídeo */
            continue;
        }
        if (!v->esvaziado) { v->esvaziado = true; h264_esvaziar(v->h); continue; }
        return VIDEO_ERRO_FIM;
    }
}

int vmp4_quadro_atual(const vmp4_t *v) { return v->quadro; }
void vmp4_reducao(vmp4_t *v, int fator) { v->reducao = fator >= 2 ? 2 : 1; }

/* ---------------------------------------------------------------- audio */

int vmp4_audio(vmp4_t *v, int16_t *pcm, int max)
{
    if (!v->a || max <= 0) return 0;
    if (v->pcm_pos >= v->pcm_n) {
        /* Hasta dónde se entrega: el cuadro mostrado más el avance. */
        uint32_t limite_ms = (uint32_t)((v->quadro + 1) * 1000 / v->fps) + MP4_LEAD_AUDIO_MS;
        uint32_t muestras = (uint32_t)v->ia * 1024u;   /* sin 64 bits: no hay libgcc */
        uint32_t hecho_ms = v->taxa ? muestras / v->taxa * 1000u + muestras % v->taxa * 1000u / v->taxa : 0;
        if (v->ia >= v->m.n_audio || hecho_ms >= limite_ms) return 0;
        mp4_amostra_t *s = &v->m.audio[v->ia++];
        const uint8_t *d = muestra(v, s->pos, s->tam);
        if (!d) return 0;
        int n = aac_quadro(v->a, d, (int)s->tam, v->pcm);
        if (n <= 0) { v->pcm_n = v->pcm_pos = 0; return 0; }
        v->pcm_n = n;
        v->pcm_pos = 0;
    }
    int disp = v->pcm_n - v->pcm_pos;
    int n = disp < max ? disp : max;
    memcpy(pcm, v->pcm + v->pcm_pos * 2, (size_t)n * 4);
    v->pcm_pos += n;
    return n;
}

/* ------------------------------------------------------------ búsqueda */

int vmp4_buscar(vmp4_t *v, int quadro)
{
    if (quadro < 0) quadro = 0;
    if (quadro >= v->m.n_video) quadro = v->m.n_video - 1;
    if (!crear_h264(v)) return VIDEO_ERRO_NAO_SUPORTADO;
    v->quadro = -1;
    /* Hasta el cuadro anterior al pedido: se decodifica sin convertir color. */
    while (v->quadro < quadro - 1) {
        int r = vmp4_quadro(v, NULL);
        if (r != 0) return r;
    }
    /* El audio vuelve al instante del cuadro: un cuadro AAC son 1024 muestras. */
    if (v->a && v->taxa) {
        uint32_t ms = (uint32_t)quadro * 1000u / (uint32_t)v->fps;
        v->ia = (int)((ms / 1000u * v->taxa + ms % 1000u * v->taxa / 1000u) / 1024u);
        if (v->ia > v->m.n_audio) v->ia = v->m.n_audio;
        v->pcm_n = v->pcm_pos = 0;
    }
    return 0;
}

void vmp4_fechar(vmp4_t *v)
{
    if (!v) return;
    if (v->h) h264_fim(v->h);
    if (v->a) aac_fim(v->a);
    mp4_fim(&v->m);
    if (v->fd >= 0) fechar(v->fd);
    if (v->moov) libera(v->moov);
    if (v->tmp) libera(v->tmp);
    libera(v);
}
#endif /* SAGUI_SEM_GPL */
