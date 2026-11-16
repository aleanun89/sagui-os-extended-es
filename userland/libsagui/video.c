// SPDX-License-Identifier: GPL-2.0-or-later
/* video.c - decodificador de vídeo de la libsagui (mejora 048, D129).
 *
 * Aquí está el lector del AVI con vídeo MJPEG: se lee la estructura RIFF
 * (cabecera 'avih', las listas de flujo 'strl' con su 'strh' y su 'strf', y
 * la lista 'movi' con los trozos), se guarda dónde está cada cuadro y cada
 * trozo de audio, y luego cada cuadro se decodifica con el lector de JPEG
 * que ya había (imagem_jpeg.c, la mejora 047).
 *
 * Cómo es el AVI (la especificación es pública y vieja):
 *
 *   RIFF <tamaño> "AVI "
 *     LIST "hdrl"
 *       "avih"  <cabecera principal: microsegundos por cuadro, cuadros, ...>
 *       LIST "strl"                       (un flujo: el de vídeo y el de audio)
 *         "strh" <cabecera del flujo: 'vids' o 'auds', escala y velocidad>
 *         "strf" <formato: BITMAPINFOHEADER (vídeo) o WAVEFORMATEX (audio)>
 *     LIST "movi"                        (aquí están los datos)
 *       "00dc" <un cuadro JPEG>          (vídeo comprimido)
 *       "01wb" <muestras PCM>            (audio sin comprimir)
 *     "idx1" <índice de los trozos>      (aquí no se usa: se lee 'movi' entero)
 *
 * Los trozos van intercalados: entre dos cuadros hay un trozo de audio con
 * el sonido de ese intervalo. El reproductor usa ese audio como reloj
 * maestro (si hay tarjeta de sonido); sin tarjeta, el vídeo va en silencio
 * y manda el reloj del sistema.
 *
 * Lo que NO se lee todavía:
 *
 *  - Vídeo sin comprimir ('db') y otros códecs dentro del AVI: se avisa
 *    igual, con el mismo error.
 *  - Audio comprimido dentro del AVI (MP2, por ejemplo): el AVI con MP2 es
 *    raro; sólo se lee el PCM de 16 bits.
 *
 * El MPEG-1 en MPEG-PS ('.mpg') sí se descodifica: lo lleva el módulo de
 * MPEG-1 (D144), que abre el archivo y da los cuadros ya en orden.
 *
 * El estado es estático: sólo hay un vídeo abierto por programa a la vez
 * (como en los decodificadores de audio). */
#include "video.h"
#include "mpeg1_interno.h"
#include "video_mp4.h"

#define VID_MAX_QUADRO   (256 * 1024)   /* tamaño máximo de un JPEG */
#define VID_AUDIO_MAX    (64 * 1024)    /* muestras PCM guardadas (16 bits) */
#define VID_PEDACOS_MIN  64

/* Un trozo del archivo: los datos de vídeo o de audio. */
typedef struct {
    uint32_t offset;   /* dónde empiezan los datos */
    uint32_t tam;      /* cuántos bytes tienen */
    uint8_t  tipo;     /* 0 = vídeo, 1 = audio */
} vid_pedaco_t;

typedef struct video {
    int      fd;
    bool     aberto;
    video_info_t info;

    vid_pedaco_t *pedacos;   /* los trozos del archivo, en orden */
    int      npedacos, cap_pedacos;

    int      cursor;         /* el trozo que se mira ahora */
    int      avih_quadros;   /* los cuadros que dice la cabecera (sólo como pista) */
    int      quadro;         /* el cuadro que se acaba de decodificar (-1 = ninguno) */
    int      proximo;        /* el cuadro que sale ahora */
    int      total_quadros;

    uint8_t *buf_video;      /* el JPEG del cuadro, mientras se decodifica */
    uint32_t cap_video;

    int16_t *buf_audio;      /* el PCM pendiente de entregar */
    uint32_t cap_audio;

    vmp4_t  *mp4;            /* el MP4 con H.264/AAC (mejora 100, D276), si el archivo lo es */
    mpeg1_t *mpeg1;          /* el decodificador de MPEG-1 (si el archivo es MPEG-PS) */
    bool     formato_mpeg1;  /* el archivo es un MPEG-PS con vídeo MPEG-1 */
    uint32_t audio_qtd, audio_pos;
} video_t;

static video_t g_vid;

/* ------------------------------------------------------------ LECTURA */

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t le16(const uint8_t *p) { return p[0] | p[1] << 8; }

/* Lee exactamente n bytes; false si el archivo se acaba antes. */
static bool ler_tudo(int fd, void *buf, unsigned n)
{
    uint8_t *p = buf;
    while (n) {
        int r = ler(fd, p, n);
        if (r <= 0) return false;
        p += r;
        n -= (unsigned)r;
    }
    return true;
}

/* ------------------------------------------------------------ LISTA DE TROZOS */

/* Añade un trozo a la lista (crece si hace falta). */
static bool vid_pon_pedaco(uint8_t tipo, uint32_t offset, uint32_t tam)
{
    if (g_vid.npedacos >= g_vid.cap_pedacos) {
        int nova = g_vid.cap_pedacos ? g_vid.cap_pedacos * 2 : VID_PEDACOS_MIN;
        vid_pedaco_t *p = realoca(g_vid.pedacos, (size_t)nova * sizeof(vid_pedaco_t));
        if (!p) return false;
        g_vid.pedacos = p;
        g_vid.cap_pedacos = nova;
    }
    vid_pedaco_t *p = &g_vid.pedacos[g_vid.npedacos++];
    p->offset = offset;
    p->tam = tam;
    p->tipo = tipo;
    return true;
}

/* ------------------------------------------------------------ CABECERAS */

/* La cabecera principal del AVI ('avih'): 56 bytes. */
typedef struct {
    uint32_t us_por_quadro;   /* microsegundos por cuadro */
    uint32_t total_quadros;
    uint32_t largura, altura;
} vid_avih_t;

/* La cabecera de un flujo ('strh'): 56 bytes. */
typedef struct {
    int      tipo;            /* 0 = vídeo, 1 = audio */
    uint32_t escala, taxa;    /* la frecuencia es taxa/escala */
    uint32_t comprimento;     /* cuadros (vídeo) o muestras (audio) */
} vid_strh_t;

static void vid_ler_avih(const uint8_t *h, vid_avih_t *a)
{
    a->us_por_quadro = le32(h);
    a->total_quadros = le32(h + 16);
    a->largura = le32(h + 32);
    a->altura = le32(h + 36);
}

static void vid_ler_strh(const uint8_t *h, vid_strh_t *s)
{
    s->tipo = (memcmp(h, "vids", 4) == 0) ? 0 : 1;
    s->escala = le32(h + 20);
    s->taxa = le32(h + 24);
    s->comprimento = le32(h + 32);
}

/* Lee la lista 'strl' de un flujo: su 'strh' y su 'strf'. */
static void vid_ler_strl(uint32_t ini, uint32_t fim)
{
    uint8_t ch[8];
    vid_strh_t strh;
    memset(&strh, 0, sizeof strh);
    strh.tipo = 0;
    while (ini + 8 <= fim) {
        if (posicionar(g_vid.fd, (int)ini, POS_INICIO) < 0) return;
        if (!ler_tudo(g_vid.fd, ch, 8)) return;
        uint32_t tam = le32(ch + 4);
        uint32_t dados = ini + 8;
        if (memcmp(ch, "strh", 4) == 0 && tam >= 56) {
            uint8_t h[56];
            if (posicionar(g_vid.fd, (int)dados, POS_INICIO) < 0) return;
            if (!ler_tudo(g_vid.fd, h, 56)) return;
            vid_ler_strh(h, &strh);
        } else if (memcmp(ch, "strf", 4) == 0 && tam >= 16) {
            uint8_t f[40];
            uint32_t n = tam > sizeof f ? sizeof f : tam;
            if (posicionar(g_vid.fd, (int)dados, POS_INICIO) < 0) return;
            if (!ler_tudo(g_vid.fd, f, n)) return;
            if (strh.tipo == 0) {
                /* BITMAPINFOHEADER: ancho, alto y compresión. */
                int32_t larg = (int32_t)le32(f + 4);
                int32_t alt = (int32_t)le32(f + 8);
                if (larg > 0 && alt > 0) {
                    g_vid.info.larg = larg;
                    g_vid.info.alt = alt < 0 ? -alt : alt;   /* la altura puede ser negativa (arriba abajo) */
                }
            } else {
                /* WAVEFORMATEX: formato, canales, frecuencia y bits. */
                uint16_t formato = le16(f);
                uint16_t canais = le16(f + 2);
                uint32_t taxa = le32(f + 4);
                uint16_t bits = le16(f + 14);
                if (formato == 1 && bits == 16 && (canais == 1 || canais == 2) && taxa >= 4000) {
                    g_vid.info.tem_audio = true;
                    g_vid.info.taxa_audio = taxa;
                    g_vid.info.canais_audio = canais;
                }
            }
        }
        ini = dados + tam + (tam & 1);
    }
}

/* Lee la lista 'hdrl': la cabecera principal y los flujos. */
static void vid_ler_hdrl(uint32_t ini, uint32_t fim)
{
    uint8_t ch[8];
    vid_avih_t avih;
    bool tem_avih = false;
    memset(&avih, 0, sizeof avih);
    while (ini + 8 <= fim) {
        if (posicionar(g_vid.fd, (int)ini, POS_INICIO) < 0) return;
        if (!ler_tudo(g_vid.fd, ch, 8)) return;
        uint32_t tam = le32(ch + 4);
        uint32_t dados = ini + 8;
        if (memcmp(ch, "avih", 4) == 0 && tam >= 56) {
            uint8_t h[56];
            if (posicionar(g_vid.fd, (int)dados, POS_INICIO) < 0) return;
            if (!ler_tudo(g_vid.fd, h, 56)) return;
            vid_ler_avih(h, &avih);
            tem_avih = true;
        } else if (memcmp(ch, "LIST", 4) == 0 && tam >= 4) {
            uint8_t tipo[4];
            if (posicionar(g_vid.fd, (int)dados, POS_INICIO) < 0) return;
            if (!ler_tudo(g_vid.fd, tipo, 4)) return;
            if (memcmp(tipo, "strl", 4) == 0) vid_ler_strl(dados + 4, dados + tam);
        }
        ini = dados + tam + (tam & 1);
    }
    if (tem_avih) {
        if (g_vid.info.larg <= 0) g_vid.info.larg = (int)avih.largura;
        if (g_vid.info.alt <= 0) g_vid.info.alt = (int)avih.altura;
        if (avih.us_por_quadro) g_vid.info.fps = (int)(1000000u / avih.us_por_quadro);
        /* El número de cuadros del 'avih' es sólo una pista: el que manda es
         * la cuenta de los trozos de 'movi'. */
        g_vid.avih_quadros = (int)avih.total_quadros;
    }
}

/* Lee la lista 'movi': anota dónde está cada trozo. */
static void vid_ler_movi(uint32_t ini, uint32_t fim)
{
    uint8_t ch[8];
    while (ini + 8 <= fim) {
        if (posicionar(g_vid.fd, (int)ini, POS_INICIO) < 0) return;
        if (!ler_tudo(g_vid.fd, ch, 8)) return;
        uint32_t tam = le32(ch + 4);
        uint32_t dados = ini + 8;
        if (memcmp(ch, "LIST", 4) == 0 || memcmp(ch, "rec ", 4) == 0) {
            /* Una lista dentro de 'movi' (un 'rec '): se salta entera. */
            ini = dados + tam + (tam & 1);
            continue;
        }
        /* Los dos últimos caracteres del nombre dicen qué es: 'dc' vídeo
         * comprimido, 'wb' audio, 'db' vídeo sin comprimir, 'tx' texto. */
        uint8_t tipo = 2;
        if (memcmp(ch + 2, "dc", 2) == 0) tipo = 0;
        else if (memcmp(ch + 2, "wb", 2) == 0) tipo = 1;
        if (tipo < 2 && !vid_pon_pedaco(tipo, dados, tam)) return;   /* sin memoria */
        ini = dados + tam + (tam & 1);
    }
}

/* ------------------------------------------------------------ ABRIR */

/* El archivo puede ser un AVI (se lee) o un MPEG-PS (se abre con el
 * demultiplexador y el decodificador de MPEG-1). */
static bool vid_formato_mpegps(const uint8_t *cab)
{
    /* Los paquetes del MPEG-PS empiezan por 0x000001BA. */
    return cab[0] == 0x00 && cab[1] == 0x00 && cab[2] == 0x01 && cab[3] == 0xBA;
}

video_t *video_abrir(const char *caminho, int *erro)
{
    int e = 0;
    if (erro) *erro = 0;
    if (g_vid.aberto) { if (erro) *erro = VIDEO_ERRO_NAO_SUPORTADO; return NULL; }

    memset(&g_vid, 0, sizeof g_vid);
    g_vid.fd = abrir(caminho, ABRIR_LER);
    if (g_vid.fd < 0) { if (erro) *erro = g_vid.fd; return NULL; }
    g_vid.quadro = -1;

    uint8_t cab[12];
    if (!ler_tudo(g_vid.fd, cab, 12)) { e = VIDEO_ERRO_CORROMPIDO; goto falha; }

    if (vmp4_reconhece(cab)) {
        /* MP4 con H.264 y AAC-LC (D276): lo lleva video_mp4.c. */
        fechar(g_vid.fd);
        g_vid.fd = -1;
        g_vid.mp4 = vmp4_abrir_arquivo(caminho, &g_vid.info, &e);
        if (!g_vid.mp4) goto falha;
        g_vid.aberto = true;
        g_vid.total_quadros = g_vid.info.quadros;
        return &g_vid;
    }
    if (vid_formato_mpegps(cab)) {
        /* MPEG-1 dentro de un MPEG-PS: lo lleva el módulo de MPEG-1. */
        fechar(g_vid.fd);
        g_vid.fd = -1;
        mpeg1_info_t mi;
        memset(&mi, 0, sizeof mi);
        mpeg1_t *m = mpeg1_abrir(caminho, &mi, &e);
        if (!m) goto falha;
        g_vid.mpeg1 = m;
        g_vid.aberto = true;
        g_vid.formato_mpeg1 = true;
        g_vid.info.larg = mi.larg;
        g_vid.info.alt = mi.alt;
        g_vid.info.fps = mi.fps > 0 ? mi.fps : 30;
        g_vid.info.quadros = mi.quadros;
        g_vid.info.duracao_ms = (uint32_t)mi.quadros * 1000u / (uint32_t)g_vid.info.fps;
        g_vid.info.tem_audio = mi.tem_audio;
        g_vid.info.taxa_audio = mi.taxa_audio;
        g_vid.info.canais_audio = mi.canais_audio;
        g_vid.info.nome_formato = "MPEG-1 en MPEG-PS";
        return &g_vid;
    }
    if (memcmp(cab, "RIFF", 4) != 0 || memcmp(cab + 8, "AVI ", 4) != 0) {
        e = VIDEO_ERRO_FORMATO;
        goto falha;
    }

    /* La posición del final del archivo, para saber hasta dónde leer. */
    int fim = posicionar(g_vid.fd, 0, POS_FIM);
    uint32_t tamanho = fim > 0 ? (uint32_t)fim : 0;

    /* Recorre los trozos de primer nivel. */
    uint32_t pos = 12;
    bool achou_movi = false;
    while (pos + 8 <= tamanho) {
        uint8_t ch[8];
        if (posicionar(g_vid.fd, (int)pos, POS_INICIO) < 0) break;
        if (!ler_tudo(g_vid.fd, ch, 8)) break;
        uint32_t tam = le32(ch + 4);
        uint32_t dados = pos + 8;
        if (memcmp(ch, "LIST", 4) == 0) {
            uint8_t tipo[4];
            if (posicionar(g_vid.fd, (int)dados, POS_INICIO) < 0) break;
            if (!ler_tudo(g_vid.fd, tipo, 4)) break;
            if (memcmp(tipo, "hdrl", 4) == 0) vid_ler_hdrl(dados + 4, dados + tam);
            else if (memcmp(tipo, "movi", 4) == 0) {
                vid_ler_movi(dados + 4, dados + tam);
                achou_movi = true;
            }
        }
        pos = dados + tam + (tam & 1);
    }
    if (!achou_movi) { e = VIDEO_ERRO_FORMATO; goto falha; }

    /* Cuenta los cuadros de vídeo: los trozos de 'movi' mandan sobre la
     * cabecera (que a veces no cuadra). */
    int contados = 0;
    for (int i = 0; i < g_vid.npedacos; i++)
        if (g_vid.pedacos[i].tipo == 0) contados++;
    if (contados) g_vid.total_quadros = contados;
    else if (!g_vid.total_quadros) g_vid.total_quadros = g_vid.avih_quadros;
    if (!g_vid.total_quadros) { e = VIDEO_ERRO_FORMATO; goto falha; }
    g_vid.info.quadros = g_vid.total_quadros;
    if (!g_vid.info.fps) g_vid.info.fps = 30;   /* por si el AVI no lo dice */
    g_vid.info.duracao_ms = (uint32_t)g_vid.info.quadros * 1000u / (uint32_t)g_vid.info.fps;
    g_vid.info.nome_formato = "MJPEG en AVI";

    g_vid.buf_video = aloca(VID_MAX_QUADRO);
    g_vid.buf_audio = aloca(VID_AUDIO_MAX * sizeof(int16_t));
    if (!g_vid.buf_video || !g_vid.buf_audio) { e = VIDEO_ERRO_MEMORIA; goto falha; }
    g_vid.cap_video = VID_MAX_QUADRO;
    g_vid.cap_audio = VID_AUDIO_MAX;

    g_vid.aberto = true;
    return &g_vid;

falha:
    if (g_vid.fd >= 0) fechar(g_vid.fd);
    if (g_vid.pedacos) libera(g_vid.pedacos);
    if (g_vid.buf_video) libera(g_vid.buf_video);
    if (g_vid.buf_audio) libera(g_vid.buf_audio);
    memset(&g_vid, 0, sizeof g_vid);
    g_vid.fd = -1;
    if (erro) *erro = e;
    return NULL;
}

/* ------------------------------------------------------------ AUDIO */

/* Lee un trozo de audio y lo deja pendiente en el búfer. */
static void vid_acumular_audio(const vid_pedaco_t *p)
{
    uint32_t canais = (uint32_t)g_vid.info.canais_audio;
    if (!canais || !p->tam) return;
    uint32_t amostras = p->tam / (2 * canais);      /* muestras de 16 bits */
    if (!amostras) return;

    /* Se copia al búfer pendiente; si no cabe, se corta (el audio es más
     * rápido que el vídeo y no puede retrasarlo). */
    uint32_t espaco = g_vid.cap_audio - g_vid.audio_qtd;
    if (espaco > amostras) espaco = amostras;
    if (!espaco) return;
    uint8_t *dst = (uint8_t *)g_vid.buf_audio + g_vid.audio_qtd * canais * 2;
    if (posicionar(g_vid.fd, (int)p->offset, POS_INICIO) < 0) return;
    if (!ler_tudo(g_vid.fd, dst, espaco * canais * 2)) return;
    g_vid.audio_qtd += espaco;
}

int video_audio(video_t *v, int16_t *pcm, int max_quadros)
{
    (void)v;
    if (!g_vid.aberto || !g_vid.info.tem_audio) return 0;
    if (max_quadros <= 0) return 0;
    if (g_vid.mp4) return vmp4_audio(g_vid.mp4, pcm, max_quadros);

    /* Si no hay nada pendiente, se toma el siguiente trozo de audio, si lo
     * hay (sin pasar de largo por un cuadro de vídeo): así el sonido no se
     * corta entre cuadros. */
    if (g_vid.audio_pos >= g_vid.audio_qtd) {
        g_vid.audio_pos = g_vid.audio_qtd = 0;
        if (g_vid.cursor < g_vid.npedacos && g_vid.pedacos[g_vid.cursor].tipo == 1) {
            vid_acumular_audio(&g_vid.pedacos[g_vid.cursor]);
            g_vid.cursor++;
        }
        if (g_vid.audio_qtd == 0) return 0;
    }

    uint32_t canais = (uint32_t)g_vid.info.canais_audio;
    uint32_t disponivel = g_vid.audio_qtd - g_vid.audio_pos;
    uint32_t n = disponivel < (uint32_t)max_quadros ? disponivel : (uint32_t)max_quadros;
    memcpy(pcm, g_vid.buf_audio + g_vid.audio_pos * canais, (size_t)n * canais * 2);
    g_vid.audio_pos += n;
    if (g_vid.audio_pos >= g_vid.audio_qtd) g_vid.audio_pos = g_vid.audio_qtd = 0;
    return (int)n;
}

/* ------------------------------------------------------------ CUADROS */

int video_quadro(video_t *v, imagem_t *img)
{
    (void)v;
    if (!g_vid.aberto) return VIDEO_ERRO_FORMATO;
    if (g_vid.mp4) {
        /* img == NULL: se decodifica y se descarta (saltar un cuadro). */
        int r = vmp4_quadro(g_vid.mp4, img);
        if (r == 0) g_vid.quadro = vmp4_quadro_atual(g_vid.mp4);
        return r;
    }
    if (!img) return VIDEO_ERRO_FORMATO;

    if (g_vid.formato_mpeg1) {
        /* El MPEG-1 lleva su propio camino (los cuadros B se reordenan
         * dentro del módulo). */
        int r = mpeg1_quadro(g_vid.mpeg1, img);
        if (r == 0) {
            g_vid.quadro = g_vid.proximo++;
            if (img->px) { /* la imagen anterior la suelta el propio módulo */ }
        }
        return r;
    }

    while (g_vid.cursor < g_vid.npedacos) {
        vid_pedaco_t *p = &g_vid.pedacos[g_vid.cursor];
        if (p->tipo == 1) {           /* audio: se guarda para después */
            vid_acumular_audio(p);
            g_vid.cursor++;
            continue;
        }
        /* Un cuadro de vídeo: su JPEG entero. */
        if (p->tam > g_vid.cap_video) { g_vid.cursor++; return VIDEO_ERRO_CORROMPIDO; }
        if (posicionar(g_vid.fd, (int)p->offset, POS_INICIO) < 0) { g_vid.cursor++; return VIDEO_ERRO_CORROMPIDO; }
        if (!ler_tudo(g_vid.fd, g_vid.buf_video, p->tam)) { g_vid.cursor++; return VIDEO_ERRO_CORROMPIDO; }
        g_vid.cursor++;
        g_vid.quadro = g_vid.proximo++;
        /* La imagen anterior se suelta aquí: el bucle del reproductor no
         * tiene que acordarse de liberarla. */
        if (img->px) imagem_liberar(img);
        int r = imagem_ler_jpeg(g_vid.buf_video, (int)p->tam, img);
        if (r < 0) return r;
        return 0;
    }
    return VIDEO_ERRO_FIM;
}

int video_buscar(video_t *v, int quadro)
{
    (void)v;
    if (!g_vid.aberto) return VIDEO_ERRO_FORMATO;
    if (g_vid.mp4) {
        int r = vmp4_buscar(g_vid.mp4, quadro);
        if (r == 0) g_vid.quadro = vmp4_quadro_atual(g_vid.mp4);
        return r;
    }
    if (quadro < 0) quadro = 0;
    if (quadro >= g_vid.total_quadros) quadro = g_vid.total_quadros - 1;
    /* Se busca el trozo de vídeo que va después del número pedido. */
    int n = 0;
    for (int i = 0; i < g_vid.npedacos; i++) {
        if (g_vid.pedacos[i].tipo != 0) continue;
        if (n == quadro) {
            g_vid.cursor = i;
            g_vid.proximo = quadro;
            g_vid.audio_qtd = g_vid.audio_pos = 0;
            return 0;
        }
        n++;
    }
    return VIDEO_ERRO_FIM;
}

int video_quadro_atual(const video_t *v) { (void)v; return g_vid.quadro; }
int video_total_quadros(const video_t *v) { (void)v; return g_vid.total_quadros; }

const video_info_t *video_info(video_t *v) { (void)v; return &g_vid.info; }

void video_fechar(video_t *v)
{
    (void)v;
    if (!g_vid.aberto) return;
    if (g_vid.mp4) { vmp4_fechar(g_vid.mp4); g_vid.mp4 = NULL; }
    if (g_vid.mpeg1) { mpeg1_fechar(g_vid.mpeg1); g_vid.mpeg1 = NULL; }
    if (g_vid.fd >= 0) fechar(g_vid.fd);
    if (g_vid.pedacos) libera(g_vid.pedacos);
    if (g_vid.buf_video) libera(g_vid.buf_video);
    if (g_vid.buf_audio) libera(g_vid.buf_audio);
    memset(&g_vid, 0, sizeof g_vid);
    g_vid.fd = -1;
    g_vid.quadro = -1;
}

/* ------------------------------------------------------------ TEXTOS */

const char *video_erro_texto(int erro)
{
    switch (erro) {
    case VIDEO_ERRO_FORMATO:       return "no es un vídeo que se lea (AVI con MJPEG, MPEG-1 o MP4 con H.264/AAC)";
    case VIDEO_ERRO_NAO_SUPORTADO: return "formato de vídeo que no se descodifica (sólo MJPEG en AVI, MPEG-1 en MPEG-PS y H.264 Baseline/Main con AAC-LC en MP4)";
    case VIDEO_ERRO_CORROMPIDO:    return "archivo de vídeo dañado o incompleto";
    case VIDEO_ERRO_MEMORIA:       return "no hay memoria suficiente";
    case VIDEO_ERRO_FIM:           return "se acabaron los cuadros";
    default:                       return nome_erro(erro);
    }
}

static int minuscula(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

bool video_extensao(const char *nome)
{
    const char *p = acha_ultimo_char(nome, '.');
    if (!p) return false;
    char a = (char)minuscula(p[1]);
    char b = p[2] ? (char)minuscula(p[2]) : 0;
    char c = p[3] ? (char)minuscula(p[3]) : 0;
    if (a == 'a' && b == 'v' && c == 'i') return true;                 /* .avi */
    if (a == 'm' && b == 'p' && c == 'g') return true;                 /* .mpg */
    if (a == 'm' && b == 'p' && c == '4') return true;                 /* .mp4 */
    if (a == 'm' && b == '4' && c == 'v') return true;                 /* .m4v (D342) */
    if (a == 'm' && b == 'p' && c == 'e' && p[4] && minuscula(p[4]) == 'g') return true;   /* .mpeg */
    return false;
}

/* Un MP4 que ya está en memoria (el Cipó lo baja entero). Los datos tienen que
 * seguir vivos hasta video_fechar(). */
video_t *video_abrir_memoria(const uint8_t *dados, uint32_t n, int *erro)
{
    int e = 0;
    if (erro) *erro = 0;
    if (g_vid.aberto) { if (erro) *erro = VIDEO_ERRO_NAO_SUPORTADO; return NULL; }
    memset(&g_vid, 0, sizeof g_vid);
    g_vid.fd = -1;
    g_vid.quadro = -1;
    if (n < 12 || !vmp4_reconhece(dados)) { if (erro) *erro = VIDEO_ERRO_FORMATO; return NULL; }
    g_vid.mp4 = vmp4_abrir_memoria(dados, n, &g_vid.info, &e);
    if (!g_vid.mp4) { memset(&g_vid, 0, sizeof g_vid); g_vid.fd = -1; g_vid.quadro = -1; if (erro) *erro = e; return NULL; }
    g_vid.aberto = true;
    g_vid.total_quadros = g_vid.info.quadros;
    return &g_vid;
}

void video_reducao(video_t *v, int fator)
{
    (void)v;
    if (g_vid.mp4) vmp4_reducao(g_vid.mp4, fator);
}
