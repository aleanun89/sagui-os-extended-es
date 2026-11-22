// SPDX-License-Identifier: GPL-2.0-or-later
/* reproductor [archivo | carpeta | lista.m3u]... - el Reproductor de SaguiOS (mejora 048, D129; rediseño D342).
 *
 * Reproduce vídeo (MJPEG dentro de un AVI, MPEG-1 en MPEG-PS y MP4 con H.264 Baseline y Main y AAC-LC,
 * mejora 100, D276: el decodificador es el mismo que usa el Cipó para el <video>, vive en la libsagui)
 * y audio (MP3, FLAC, WAV, ADPCM y MIDI, con los decodificadores de la libsagui, los mismos de `musica`).
 *
 * Aspecto (D342): barra de menús (Archivo, Ver, Reproducir, Ayuda), el vídeo escalado conservando la
 * proporción y, abajo, un panel de controles oscuro: barra de posición arrastrable (con el tiempo al
 * pasar el ratón), anterior, parar, un botón redondo de reproducir/pausa, siguiente, silencio y un
 * deslizador de volumen; a la derecha, aleatorio, repetir, la lista y la pantalla completa. La lista de
 * reproducción va a la derecha y se oculta o muestra (Ctrl+L): añadir archivos o una carpeta, quitar,
 * reordenar, guardar y abrir .m3u, aleatorio y repetir; al acabar una pista pasa sola a la siguiente.
 * En el audio se ve la carátula (ID3 APIC o FLAC PICTURE) o barras de espectro (FFT entera, 256 puntos).
 *
 * Controles:
 *   Espacio / Ctrl+P   reproducir y pausa          Ctrl+S   parar
 *   Ctrl+B / Ctrl+F    pista anterior / siguiente  Ctrl+L   mostrar u ocultar la lista
 *   F8                 silencio (también M)        F9 / F10 volumen -/+ (también + -)
 *   <- ->              atrás y adelante 5 segundos
 *   , .                un cuadro atrás o adelante (con la pausa puesta)
 *   Inicio Fin         el principio y el final
 *   F11 o doble clic   pantalla completa (los controles salen al mover el ratón y se ocultan a los 3 s)
 *   L                  repetir (la lista entera, o la pista si es la única)
 *   A                  aleatorio
 *   I                  estadísticas (cuadros por segundo, saltos y tiempo de decodificar)
 *   Ctrl+O             abrir otro archivo         Supr      quitar de la lista la pista elegida
 *
 * El sonido: si el vídeo trae audio y hay tarjeta de sonido, el audio es el reloj maestro (el cuadro se
 * muestra cuando le toca al sonido) y el vídeo va en silencio si no hay tarjeta. Si la CPU no llega, se
 * saltan cuadros (para que el sonido no se corte) en vez de ir a cámara lenta.
 *
 * Pendiente: la velocidad (0,5x a 2x). Con el audio como reloj maestro haría falta remuestrearlo.
 *
 * Modos para las pruebas:
 *   reproductor -t archivo            decodifica sin ventana y dice las sumas
 *                                     de cada cuadro (y cuánto tardó)
 *   reproductor --volcar arquivo salida [desde [cuadros]]
 *                                     escribe los cuadros decodificados en crudo
 *                                     (RGB24) para compararlos con ffmpeg
 */
#include "sagui.h"
#include "gui.h"
#include "video.h"
#include "audio_formato.h"
#include "energia.h"

#define LARG_INICIO 800
#define ALT_INICIO  600
#define LARG_MIN 480
#define ALT_MIN  320
#define MENU_A 20                 /* alto de la barra de menús */
#define PAN_A  72                 /* alto del panel de controles */
#define PCM_CUADROS 4096          /* muestras de PCM que se llevan de golpe */
#define SALTO_MS 5000             /* lo que saltan las flechas */
#define MAX_LISTA 256
#define OCULTAR_MS 3000           /* en pantalla completa, los controles se ocultan a los 3 s */
#define ADELANTE_MS 400           /* audio: lo que se deja por delante en el flujo del kernel */
#define ESPEC_N 256               /* puntos de la FFT */
#define ESPEC_BARRAS 24
#define ANEL_ESP 32768            /* muestras (mono) que se guardan para el espectro */

/* Colores propios del panel (el verde de SaguiOS es el acento). */
#define PAN_FUNDO   0x1F272C
#define PAN_BORDA   0x3A4750
#define PAN_ICONE   0xE4ECEF
#define PAN_APAGADO 0x7A868D
#define PAN_TRILHO  0x46545D
#define PAN_TEXTO   0xCDD8DD
#define ACENTO      COR_DESTAQUE
#define ACENTO_CL   0x1FB864
#define VIDEO_FUNDO 0x0E1418

enum { MED_NENHUM, MED_VIDEO, MED_AUDIO };

typedef struct { int x, y, l, a; } rect_t;

/* ------------------------------------------------------------ estado */

static jan_t *j;
static int  med = MED_NENHUM;
static video_t *vid;
static audio_arquivo_t *aud;
static const audio_info_t *ainf;
static imagem_t img;
static imagem_t capa;             /* carátula del audio (px NULL = no hay) */
static bool tem_quadro;
static bool primeiro_desenhado;   /* ya se enseñó un cuadro de verdad */
static bool tocando = true;
static bool fim;              /* la pista se acabó: hay que volver a empezar */
static bool bucle;            /* repetir */
static bool aleatorio;
static bool mudo;
static int  volume = 80;
static bool tela_cheia;
static int  audio_fd = -1;
static uint32_t taxa_audio, canais_audio;
static uint32_t ms_escritos;     /* ms de audio ya escritos en la tarjeta (vídeo) */
static unsigned t_inicio;        /* tempo_ms() cuando empezó la reproducción */
static int  quadro_atual = -1;   /* el cuadro que se está viendo */
static int  quadros_vistos, quadros_saltados;
static uint32_t ms_decodificar;  /* ms gastados en decodificar */
static char aviso[140];
static int  fps = 30, quadros_total;
static char caminho_video[200];

/* Audio (solo sonido) */
static uint32_t aud_base_ms;     /* instante del archivo en que se abrió el flujo */
static uint32_t aud_quadros;     /* cuadros escritos en el flujo desde entonces */
static bool aud_fim_dec;         /* el decodificador ya no da más */
static int  aud_pos;             /* última posición conocida (ms) */
static int16_t espec_anel[ANEL_ESP];
static uint32_t espec_n;         /* muestras guardadas en total */
static uint8_t barras[ESPEC_BARRAS];   /* altura de cada barra, 0-100 */
static int  espec_borda[ESPEC_BARRAS + 1];
static bool espec_logado, capa_logada;

/* Interfaz */
static barra_menu_t menu;
static char menu_txt[4][480];
static bool lista_vis;
static bool ctrl_visivel = true; /* en pantalla completa: los controles se ven */
static unsigned t_mover;         /* última vez que se movió el ratón */
static int  hover_seek = -1;     /* ms bajo el ratón en la barra de posición (-1 = ninguno) */
static bool arrasta_seek, arrasta_vol;
static int  arrasta_ms;          /* posición que se está eligiendo al arrastrar */
static rect_t r_video, r_lista, r_pan, r_seek, r_prev, r_stop, r_play, r_next, r_mute, r_vol;
static rect_t r_alea, r_rep, r_listab, r_full;
static int menu_a;
static botao_t bt_lista[5];

/* Lista de reproducción */
static char lista_c[MAX_LISTA][200];
static int  lista_n, lista_atual = -1, lista_sel = -1, lista_topo;
static bool lista_vista[MAX_LISTA];   /* aleatorio: ya sonó */
static unsigned semente = 12345;
static unsigned geracao;         /* sube al abrir o saltar: el bucle de vídeo sabe que el reloj cambió */
static char ultimo_erro[80];

/* ------------------------------------------------------------ ayudas */

static const char *nome_curto(const char *c)
{
    const char *b = acha_ultimo_char(c, '/');
    return b ? b + 1 : c;
}

static void formatar_tempo(char *s, int max, int ms)
{
    if (ms < 0) ms = 0;
    int s_ = ms / 1000;
    if (s_ >= 3600) formatar_texto(s, max, "%d:%02d:%02d", s_ / 3600, s_ / 60 % 60, s_ % 60);
    else formatar_texto(s, max, "%d:%02d", s_ / 60, s_ % 60);
}

static bool ext_igual(const char *nome, const char *ext)
{
    int n = (int)tam_texto(nome), e = (int)tam_texto(ext);
    if (n < e) return false;
    for (int i = 0; i < e; i++) {
        char c = nome[n - e + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if (c != ext[i]) return false;
    }
    return true;
}

static bool eh_audio(const char *nome)
{
    return ext_igual(nome, ".mp3") || ext_igual(nome, ".flac") || ext_igual(nome, ".wav") ||
           ext_igual(nome, ".mid") || ext_igual(nome, ".kar") || ext_igual(nome, ".midi");
}

static bool eh_media(const char *nome)
{
    return eh_audio(nome) || ext_igual(nome, ".mp4") || ext_igual(nome, ".m4v") || ext_igual(nome, ".avi") ||
           ext_igual(nome, ".mpg") || ext_igual(nome, ".mpeg");
}

static unsigned aleatorio_n(unsigned n)
{
    semente ^= semente << 13; semente ^= semente >> 17; semente ^= semente << 5;
    return n ? semente % n : 0;
}

static unsigned periodo_ms(void) { return fps > 0 ? 1000u / (unsigned)fps : 33u; }

/* El reloj del vídeo, en ms desde el primer cuadro. Con tarjeta de sonido
 * manda el audio (lo que se está oyendo); sin ella, el reloj del sistema. */
static unsigned relogio(void)
{
    if (audio_fd < 0) return tempo_ms() - t_inicio;
    int pend = audio_pendente(audio_fd);
    if (pend < 0) pend = 0;
    uint32_t ms = ms_escritos > (uint32_t)pend ? ms_escritos - (uint32_t)pend : 0;
    return ms;
}

/* Duración y posición de la pista, en ms (0 = no se sabe). */
static int duracao_ms(void)
{
    if (med == MED_VIDEO) return quadros_total * (int)periodo_ms();
    if (med == MED_AUDIO && ainf) return (int)ainf->duracao_ms;
    return 0;
}

static int audio_posicao_agora(void)
{
    if (!ainf || ainf->taxa == 0) return aud_pos;
    if (!tocando) return aud_pos;
    int p;
    if (audio_fd >= 0) {
        int pend = audio_pendente(audio_fd);
        if (pend < 0) pend = 0;
        p = (int)(aud_base_ms + (uint32_t)((uint64_t)aud_quadros * 1000u / ainf->taxa)) - pend;
    } else {
        p = (int)aud_base_ms + (int)(tempo_ms() - t_inicio);
    }
    int d = (int)ainf->duracao_ms;
    if (p < 0) p = 0;
    if (d > 0 && p > d) p = d;
    aud_pos = p;
    return p;
}

static int posicao_ms(void)
{
    if (med == MED_VIDEO) return (quadro_atual < 0 ? 0 : quadro_atual) * (int)periodo_ms();
    if (med == MED_AUDIO) return audio_posicao_agora();
    return 0;
}

/* ------------------------------------------------------------ carátula del audio */

static int ler_em(int fd, int off, void *buf, int n)
{
    if (posicionar(fd, off, POS_INICIO) < 0) return -1;
    int t = 0;
    while (t < n) {
        int r = ler(fd, (uint8_t *)buf + t, (unsigned)(n - t));
        if (r <= 0) break;
        t += r;
    }
    return t;
}

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static uint32_t sincro32(const uint8_t *p) { return (uint32_t)(p[0] & 127) << 21 | (uint32_t)(p[1] & 127) << 14 | (uint32_t)(p[2] & 127) << 7 | (p[3] & 127); }

#define CAPA_MAX (6 * 1024 * 1024)

/* ID3v2.2/2.3/2.4: el cuadro APIC (PIC en la 2.2). */
static bool capa_id3(int fd, imagem_t *saida)
{
    uint8_t h[10];
    if (ler_em(fd, 0, h, 10) != 10 || memcmp(h, "ID3", 3) != 0) return false;
    int ver = h[3];
    if (ver < 2 || ver > 4) return false;
    int fimtag = 10 + (int)sincro32(h + 6);
    int pos = 10;
    if (h[5] & 0x40) {   /* cabecera extendida */
        uint8_t x[4];
        if (ler_em(fd, pos, x, 4) != 4) return false;
        pos += ver == 4 ? (int)sincro32(x) : (int)be32(x) + 4;
    }
    int cab = ver == 2 ? 6 : 10;
    while (pos + cab <= fimtag) {
        uint8_t f[10];
        if (ler_em(fd, pos, f, cab) != cab || f[0] == 0) break;
        int tam;
        bool apic;
        if (ver == 2) {
            tam = (int)((uint32_t)f[3] << 16 | (uint32_t)f[4] << 8 | f[5]);
            apic = f[0] == 'P' && f[1] == 'I' && f[2] == 'C';
        } else {
            tam = ver == 4 ? (int)sincro32(f + 4) : (int)be32(f + 4);
            apic = memcmp(f, "APIC", 4) == 0;
        }
        if (tam <= 0 || pos + cab + tam > fimtag) break;
        if (apic && tam < CAPA_MAX) {
            uint8_t *b = aloca((size_t)tam);
            if (b && ler_em(fd, pos + cab, b, tam) == tam) {
                int enc = b[0], p = 1;
                if (ver == 2) p += 3;
                else while (p < tam && b[p]) p++;
                p++;          /* fin del tipo MIME */
                p++;          /* tipo de imagen */
                if (enc == 1 || enc == 2) { while (p + 1 < tam && (b[p] || b[p + 1])) p += 2; p += 2; }
                else { while (p < tam && b[p]) p++; p++; }
                if (p < tam && imagem_decodificar(b + p, tam - p, saida) == 0) { libera(b); return true; }
            }
            if (b) libera(b);
        }
        pos += cab + tam;
    }
    return false;
}

/* FLAC: el bloque de metadatos PICTURE (tipo 6). */
static bool capa_flac(int fd, imagem_t *saida)
{
    uint8_t h[4];
    if (ler_em(fd, 0, h, 4) != 4 || memcmp(h, "fLaC", 4) != 0) return false;
    int pos = 4;
    for (int n = 0; n < 64; n++) {
        if (ler_em(fd, pos, h, 4) != 4) return false;
        int tipo = h[0] & 127, ultimo = h[0] & 128;
        int tam = (int)((uint32_t)h[1] << 16 | (uint32_t)h[2] << 8 | h[3]);
        if (tipo == 6 && tam > 32 && tam < CAPA_MAX) {
            uint8_t *b = aloca((size_t)tam);
            if (b && ler_em(fd, pos + 4, b, tam) == tam) {
                uint32_t mime = be32(b + 4);
                if (8 + mime + 4 <= (uint32_t)tam) {
                    uint32_t desc = be32(b + 8 + mime);
                    uint32_t q = 8 + mime + 4 + desc + 16;
                    if (q + 4 <= (uint32_t)tam) {
                        uint32_t dn = be32(b + q);
                        if (q + 4 + dn <= (uint32_t)tam && imagem_decodificar(b + q + 4, (int)dn, saida) == 0) {
                            libera(b);
                            return true;
                        }
                    }
                }
            }
            if (b) libera(b);
        }
        if (ultimo) break;
        pos += 4 + tam;
    }
    return false;
}

static void carregar_capa(const char *caminho)
{
    if (capa.px) imagem_liberar(&capa);
    memset(&capa, 0, sizeof capa);
    capa_logada = false;
    int fd = abrir(caminho, ABRIR_LER);
    if (fd < 0) return;
    if (!capa_id3(fd, &capa)) capa_flac(fd, &capa);
    fechar(fd);
}

/* ------------------------------------------------------------ espectro (FFT entera) */

static const int16_t cos_q14[ESPEC_N] = {
    16384, 16379, 16364, 16340, 16305, 16261, 16207, 16143, 16069, 15986, 15893, 15791, 15679, 15557, 15426, 15286,
    15137, 14978, 14811, 14635, 14449, 14256, 14053, 13842, 13623, 13395, 13160, 12916, 12665, 12406, 12140, 11866,
    11585, 11297, 11003, 10702, 10394, 10080, 9760, 9434, 9102, 8765, 8423, 8076, 7723, 7366, 7005, 6639,
    6270, 5897, 5520, 5139, 4756, 4370, 3981, 3590, 3196, 2801, 2404, 2006, 1606, 1205, 804, 402,
    0, -402, -804, -1205, -1606, -2006, -2404, -2801, -3196, -3590, -3981, -4370, -4756, -5139, -5520, -5897,
    -6270, -6639, -7005, -7366, -7723, -8076, -8423, -8765, -9102, -9434, -9760, -10080, -10394, -10702, -11003, -11297,
    -11585, -11866, -12140, -12406, -12665, -12916, -13160, -13395, -13623, -13842, -14053, -14256, -14449, -14635, -14811, -14978,
    -15137, -15286, -15426, -15557, -15679, -15791, -15893, -15986, -16069, -16143, -16207, -16261, -16305, -16340, -16364, -16379,
    -16384, -16379, -16364, -16340, -16305, -16261, -16207, -16143, -16069, -15986, -15893, -15791, -15679, -15557, -15426, -15286,
    -15137, -14978, -14811, -14635, -14449, -14256, -14053, -13842, -13623, -13395, -13160, -12916, -12665, -12406, -12140, -11866,
    -11585, -11297, -11003, -10702, -10394, -10080, -9760, -9434, -9102, -8765, -8423, -8076, -7723, -7366, -7005, -6639,
    -6270, -5897, -5520, -5139, -4756, -4370, -3981, -3590, -3196, -2801, -2404, -2006, -1606, -1205, -804, -402,
    0, 402, 804, 1205, 1606, 2006, 2404, 2801, 3196, 3590, 3981, 4370, 4756, 5139, 5520, 5897,
    6270, 6639, 7005, 7366, 7723, 8076, 8423, 8765, 9102, 9434, 9760, 10080, 10394, 10702, 11003, 11297,
    11585, 11866, 12140, 12406, 12665, 12916, 13160, 13395, 13623, 13842, 14053, 14256, 14449, 14635, 14811, 14978,
    15137, 15286, 15426, 15557, 15679, 15791, 15893, 15986, 16069, 16143, 16207, 16261, 16305, 16340, 16364, 16379
};

static int fft_re[ESPEC_N], fft_im[ESPEC_N];

static void espec_iniciar(void)
{
    /* Bordes de las barras en bins 1..127, repartidos en escala logarítmica (razón 127^(1/24) en Q16). */
    uint32_t e = 1u << 16;
    espec_borda[0] = 1;
    for (int k = 1; k <= ESPEC_BARRAS; k++) {
        e = (uint32_t)((uint64_t)e * 80146u >> 16);
        int b = (int)(e >> 16);
        if (b <= espec_borda[k - 1]) b = espec_borda[k - 1] + 1;
        if (b > 128) b = 128;
        espec_borda[k] = b;
    }
}

static int log_q(uint32_t v)
{
    /* 4 pasos por octava: ~1,5 dB cada uno */
    if (v < 2) return (int)v * 2;
    int b = 31 - __builtin_clz(v);
    int frac = b >= 2 ? (int)((v >> (b - 2)) & 3) : 0;
    return b * 4 + frac;
}

/* Calcula las barras con las últimas ESPEC_N muestras que ya han sonado. */
static void espec_calcular(void)
{
    if (!ainf || ainf->taxa == 0 || espec_n < ESPEC_N) return;
    int pend = audio_fd >= 0 ? audio_pendente(audio_fd) : 0;
    if (pend < 0) pend = 0;
    uint32_t atras = (uint32_t)((uint64_t)pend * ainf->taxa / 1000u) + ESPEC_N;
    if (atras > espec_n || atras > ANEL_ESP) atras = espec_n < ANEL_ESP ? espec_n : ANEL_ESP;
    if (atras < ESPEC_N) return;
    uint32_t ini = espec_n - atras;
    /* orden de bits invertido al cargar, con ventana de Hann (Q14) */
    for (int n = 0; n < ESPEC_N; n++) {
        int r = 0;
        for (int b = 0; b < 8; b++) if (n & (1 << b)) r |= 1 << (7 - b);
        int w = (16384 - cos_q14[n]) / 2;                       /* 0..16384 */
        int s = espec_anel[(ini + (uint32_t)n) & (ANEL_ESP - 1)];
        fft_re[r] = s * w >> 14;
        fft_im[r] = 0;
    }
    for (int tam = 2; tam <= ESPEC_N; tam <<= 1) {
        int meio = tam / 2, passo = ESPEC_N / tam;
        for (int i = 0; i < ESPEC_N; i += tam)
            for (int k = 0; k < meio; k++) {
                int c = cos_q14[k * passo], s = -cos_q14[(k * passo + ESPEC_N - ESPEC_N / 4) & (ESPEC_N - 1)];
                int a = i + k, b = a + meio;
                int tr = (fft_re[b] * c - fft_im[b] * s) >> 14;
                int ti = (fft_re[b] * s + fft_im[b] * c) >> 14;
                fft_re[b] = (fft_re[a] - tr) >> 1; fft_im[b] = (fft_im[a] - ti) >> 1;
                fft_re[a] = (fft_re[a] + tr) >> 1; fft_im[a] = (fft_im[a] + ti) >> 1;
            }
    }
    for (int k = 0; k < ESPEC_BARRAS; k++) {
        uint32_t mx = 0;
        for (int b = espec_borda[k]; b < espec_borda[k + 1]; b++) {
            uint32_t re = (uint32_t)(fft_re[b] < 0 ? -fft_re[b] : fft_re[b]);
            uint32_t im = (uint32_t)(fft_im[b] < 0 ? -fft_im[b] : fft_im[b]);
            uint32_t m = re > im ? re + im / 2 : im + re / 2;
            if (m > mx) mx = m;
        }
        int nivel = (log_q(mx) - 10) * 100 / 38;     /* ~ 0 con ruido, 100 con casi toda la escala */
        if (nivel < 0) nivel = 0;
        if (nivel > 100) nivel = 100;
        if (nivel >= barras[k]) barras[k] = (uint8_t)nivel;
        else barras[k] = (uint8_t)(barras[k] > 6 ? barras[k] - 6 : 0);   /* cae despacio */
    }
}

/* ------------------------------------------------------------ dibujo */

static bool dentro(const rect_t *r, int x, int y)
{
    return r->l > 0 && x >= r->x && x < r->x + r->l && y >= r->y && y < r->y + r->a;
}

static void layout(void)
{
    int w = j->larg, h = j->alt;
    menu_a = tela_cheia ? 0 : MENU_A;
    int lw = (lista_vis && !tela_cheia && w >= 520) ? (w >= 700 ? 220 : 180) : 0;
    int pan = tela_cheia ? 0 : PAN_A;
    r_lista = (rect_t){ w - lw, menu_a, lw, h - menu_a - pan };
    r_video = (rect_t){ 0, menu_a, w - lw, h - menu_a - pan };
    if (r_video.a < 1) r_video.a = 1;
    int y0 = h - PAN_A;
    r_pan = (rect_t){ 0, y0, w, PAN_A };
    r_seek = (rect_t){ 12, y0 + 3, w - 24, 18 };
    int cy = y0 + 44;
    int total = 30 + 30 + 44 + 30 + 30 + 80 + 5 * 8;
    int x = (w - total) / 2;
    r_prev = (rect_t){ x, cy - 15, 30, 30 };  x += 38;
    r_stop = (rect_t){ x, cy - 15, 30, 30 };  x += 38;
    r_play = (rect_t){ x, cy - 22, 44, 44 };  x += 52;
    r_next = (rect_t){ x, cy - 15, 30, 30 };  x += 38;
    r_mute = (rect_t){ x, cy - 15, 30, 30 };  x += 38;
    r_vol  = (rect_t){ x, cy - 10, 80, 20 };
    int fimc = x + 80;
    /* A la derecha: aleatorio, repetir, lista y pantalla completa, los que quepan. */
    int dx = w - 12;
    r_full = (rect_t){ dx - 28, cy - 14, 28, 28 };  dx -= 28 + 6;
    r_listab = (rect_t){ dx - 48, cy - 14, 48, 28 }; dx -= 48 + 6;
    r_rep = (rect_t){ dx - 56, cy - 14, 56, 28 };   dx -= 56 + 6;
    r_alea = (rect_t){ dx - 56, cy - 14, 56, 28 };
    if (r_alea.x < fimc + 10) r_alea.l = 0;
    if (r_rep.x < fimc + 10) r_rep.l = 0;
    if (r_listab.x < fimc + 10) r_listab.l = 0;
    if (r_full.x < fimc + 10) r_full.l = 0;
    /* botones de la lista */
    if (lw) {
        int pw = lw - 12;
        int by = r_lista.y + r_lista.a - 54;
        int b2 = (pw - 4) / 2, b3 = (pw - 8) / 3;
        bt_lista[0] = (botao_t){ r_lista.x + 6, by, b2, 24, N_("Añadir..."), false };
        bt_lista[1] = (botao_t){ r_lista.x + 6 + b2 + 4, by, b2, 24, N_("Carpeta..."), false };
        bt_lista[2] = (botao_t){ r_lista.x + 6, by + 28, b3, 24, N_("Quitar"), false };
        bt_lista[3] = (botao_t){ r_lista.x + 6 + b3 + 4, by + 28, b3, 24, N_("Subir"), false };
        bt_lista[4] = (botao_t){ r_lista.x + 6 + 2 * (b3 + 4), by + 28, b3, 24, N_("Bajar"), false };
    }
}

static int lista_linhas(void)
{
    int n = (r_lista.a - 20 - 58) / 18;
    return n > 1 ? n : 1;
}

/* ---- primitivas ---- */

static void disco(int cx, int cy, int r, uint32_t cor)
{
    for (int dy = -r; dy <= r; dy++) {
        int dx = 0;
        while ((dx + 1) * (dx + 1) + dy * dy <= r * r + r) dx++;
        g_retangulo(j, cx - dx, cy + dy, 2 * dx + 1, 1, cor);
    }
}

/* Triángulo que apunta a la derecha (dir = 1) o a la izquierda (dir = -1), de alto 2*s. */
static void triangulo(int x, int cy, int s, int dir, uint32_t cor)
{
    for (int i = 0; i < s; i++) {
        int h = 2 * (s - i) - 1;
        g_retangulo(j, dir > 0 ? x + i : x + s - 1 - i, cy - h / 2, 1, h, cor);
    }
}

static void icone_anterior(const rect_t *r, uint32_t cor)
{
    int cx = r->x + r->l / 2, cy = r->y + r->a / 2;
    g_retangulo(j, cx - 8, cy - 7, 3, 14, cor);
    triangulo(cx - 4, cy, 9, -1, cor);
}

static void icone_proximo(const rect_t *r, uint32_t cor)
{
    int cx = r->x + r->l / 2, cy = r->y + r->a / 2;
    triangulo(cx - 5, cy, 9, 1, cor);
    g_retangulo(j, cx + 5, cy - 7, 3, 14, cor);
}

static void icone_parar(const rect_t *r, uint32_t cor)
{
    int cx = r->x + r->l / 2, cy = r->y + r->a / 2;
    g_retangulo(j, cx - 6, cy - 6, 12, 12, cor);
}

static void icone_som(const rect_t *r, uint32_t cor, bool cortado)
{
    int cx = r->x + r->l / 2, cy = r->y + r->a / 2;
    g_retangulo(j, cx - 9, cy - 3, 5, 7, cor);
    triangulo(cx - 4, cy, 7, -1, cor);
    g_retangulo(j, cx - 4, cy - 6, 1, 1, cor);
    if (cortado) {
        g_linha(j, cx + 1, cy - 5, cx + 9, cy + 5, 0xE0605A);
        g_linha(j, cx + 1, cy + 5, cx + 9, cy - 5, 0xE0605A);
    } else {
        g_retangulo(j, cx + 4, cy - 2, 1, 5, cor);
        g_retangulo(j, cx + 7, cy - 5, 1, 11, cor);
    }
}

static void interruptor(const rect_t *r, const char *t, bool ligado)
{
    if (r->l <= 0) return;
    g_retangulo(j, r->x, r->y, r->l, r->a, ligado ? ACENTO : PAN_TRILHO);
    g_moldura(j, r->x, r->y, r->l, r->a, ligado ? ACENTO_CL : PAN_BORDA);
    int tw = g_largura_texto(t);
    g_texto(j, r->x + (r->l - tw) / 2, r->y + (r->a - LETRA_A) / 2, t, ligado ? COR_BRANCO : PAN_TEXTO);
}

static int seek_x_de_ms(int ms)
{
    int d = duracao_ms();
    if (d <= 0) return r_seek.x;
    if (ms > d) ms = d;
    if (ms < 0) ms = 0;
    return r_seek.x + (int)((int64_t)ms * (r_seek.l - 1) / d);
}

static int seek_ms_de_x(int x)
{
    int d = duracao_ms();
    int v = x - r_seek.x;
    if (v < 0) v = 0;
    if (v > r_seek.l - 1) v = r_seek.l - 1;
    return r_seek.l > 1 ? (int)((int64_t)v * d / (r_seek.l - 1)) : 0;
}

/* El panel de controles (sin mostrarlo en la pantalla: lo hace quien llama). */
static void pintar_controles(void)
{
    g_retangulo(j, r_pan.x, r_pan.y, r_pan.l, r_pan.a, PAN_FUNDO);
    g_retangulo(j, r_pan.x, r_pan.y, r_pan.l, 1, PAN_BORDA);
    int d = duracao_ms();
    int pos = arrasta_seek ? arrasta_ms : posicao_ms();
    /* barra de posición */
    int ty = r_seek.y + 6;
    g_retangulo(j, r_seek.x, ty, r_seek.l, 6, PAN_TRILHO);
    int kx = seek_x_de_ms(pos);
    if (d > 0) g_retangulo(j, r_seek.x, ty, kx - r_seek.x, 6, ACENTO);
    if (med != MED_NENHUM) disco(kx, ty + 3, 6, d > 0 ? COR_BRANCO : PAN_APAGADO);
    /* tiempo */
    char a[16], b[16], t[48];
    formatar_tempo(a, sizeof a, pos);
    if (d > 0) formatar_tempo(b, sizeof b, d); else copia_texto(b, "--:--", sizeof b);
    formatar_texto(t, sizeof t, "%s / %s", a, b);
    int cy = r_play.y + r_play.a / 2;
    g_texto(j, 12, cy - 12, t, PAN_TEXTO);
    int cabe = (r_prev.x - 12 - 12) / LETRA_L;
    if (aviso[0] && cabe > 4) g_texto_n(j, 12, cy + 6, aviso, cabe, PAN_APAGADO);
    /* botones */
    uint32_t ic = med == MED_NENHUM ? PAN_APAGADO : PAN_ICONE;
    icone_anterior(&r_prev, ic);
    icone_parar(&r_stop, ic);
    int pcx = r_play.x + r_play.l / 2;
    disco(pcx, cy, 22, PAN_BORDA);
    disco(pcx, cy, 20, med == MED_NENHUM ? PAN_TRILHO : ACENTO);
    if (tocando && med != MED_NENHUM) {
        g_retangulo(j, pcx - 7, cy - 8, 5, 16, COR_BRANCO);
        g_retangulo(j, pcx + 2, cy - 8, 5, 16, COR_BRANCO);
    } else {
        triangulo(pcx - 5, cy, 10, 1, COR_BRANCO);
    }
    icone_proximo(&r_next, ic);
    icone_som(&r_mute, mudo ? PAN_APAGADO : PAN_ICONE, mudo);
    /* volumen */
    int vy = r_vol.y + r_vol.a / 2;
    g_retangulo(j, r_vol.x, vy - 2, r_vol.l, 4, PAN_TRILHO);
    int vl = (r_vol.l - 1) * (mudo ? 0 : volume) / 100;
    g_retangulo(j, r_vol.x, vy - 2, vl, 4, ACENTO);
    disco(r_vol.x + (r_vol.l - 1) * volume / 100, vy, 5, mudo ? PAN_APAGADO : PAN_ICONE);
    /* derecha */
    interruptor(&r_alea, _("Aleat."), aleatorio);
    interruptor(&r_rep, _("Repet."), bucle);
    interruptor(&r_listab, _("Lista"), lista_vis);
    if (r_full.l > 0) {
        g_moldura(j, r_full.x, r_full.y, r_full.l, r_full.a, PAN_BORDA);
        g_moldura(j, r_full.x + 7, r_full.y + 7, 14, 14, PAN_ICONE);
    }
}

static void desenhar_controles(void)
{
    if (tela_cheia && !ctrl_visivel) return;
    pintar_controles();
    jan_mostrar_area(j, r_pan.x, r_pan.y, r_pan.l, r_pan.a);
}

static void pintar_lista(void)
{
    if (r_lista.l <= 0) return;
    g_retangulo(j, r_lista.x, r_lista.y, r_lista.l, r_lista.a, COR_FUNDO);
    g_retangulo(j, r_lista.x, r_lista.y, 1, r_lista.a, COR_BORDA);
    g_retangulo(j, r_lista.x + 1, r_lista.y, r_lista.l - 1, 20, PAN_FUNDO);
    g_texto(j, r_lista.x + 8, r_lista.y + 2, _("Lista de reproducción"), PAN_TEXTO);
    int linhas = lista_linhas();
    if (lista_topo > lista_n - linhas) lista_topo = lista_n - linhas;
    if (lista_topo < 0) lista_topo = 0;
    int cols = (r_lista.l - 20) / LETRA_L;
    for (int i = 0; i < linhas && lista_topo + i < lista_n; i++) {
        int k = lista_topo + i, y = r_lista.y + 20 + i * 18;
        bool sel = k == lista_sel, atual = k == lista_atual;
        if (sel) g_retangulo(j, r_lista.x + 1, y, r_lista.l - 1, 18, COR_SELECAO);
        uint32_t cor = sel ? COR_BRANCO : atual ? ACENTO : COR_TEXTO;
        if (atual) g_texto(j, r_lista.x + 5, y + 1, ">", cor);
        g_texto_n(j, r_lista.x + 15, y + 1, nome_curto(lista_c[k]), cols, cor);
    }
    if (lista_n == 0)
        g_texto_n(j, r_lista.x + 8, r_lista.y + 28, _("Lista vacía"), cols, COR_TEXTO_FRACO);
    for (int i = 0; i < 5; i++) botao_desenhar(j, &bt_lista[i]);
}

static void desenhar_lista(void)
{
    if (r_lista.l <= 0) return;
    pintar_lista();
    jan_mostrar_area(j, r_lista.x, r_lista.y, r_lista.l, r_lista.a);
}

/* Centra un texto en la zona, a la altura y. */
static void texto_centrado(const rect_t *z, int y, const char *t, uint32_t cor)
{
    int cols = z->l / LETRA_L;
    int n = (int)tam_texto(t);
    if (n > cols) n = cols;
    g_texto_n(j, z->x + (z->l - n * LETRA_L) / 2, y, t, n, cor);
}

static void pintar_audio(void)
{
    rect_t z = r_video;
    g_retangulo(j, z.x, z.y, z.l, z.a, VIDEO_FUNDO);
    char t1[200] = "", t2[200] = "";
    if (ainf) {
        copia_texto(t1, ainf->titulo[0] ? ainf->titulo : nome_curto(caminho_video), sizeof t1);
        if (ainf->artista[0]) copia_texto(t2, ainf->artista, sizeof t2);
        if (ainf->album[0]) { if (t2[0]) junta_texto(t2, " - ", sizeof t2); junta_texto(t2, ainf->album, sizeof t2); }
    }
    int texto_a = 44;
    if (capa.px) {
        int al = z.l - 20, aa = z.a - texto_a - 10;
        if (al < 8) al = 8;
        if (aa < 8) aa = 8;
        unsigned ex = (unsigned)al * 65536u / (unsigned)capa.larg, ey = (unsigned)aa * 65536u / (unsigned)capa.alt;
        unsigned es = ex < ey ? ex : ey;
        int l = (int)((unsigned)capa.larg * es / 65536u), a = (int)((unsigned)capa.alt * es / 65536u);
        if (l < 1) l = 1;
        if (a < 1) a = 1;
        int x = z.x + (z.l - l) / 2, y = z.y + 6 + (aa - a) / 2;
        imagem_desenhar(j->px, j->larg, z.x, z.y, z.l, z.a, &capa, x, y, l, a, es < 65536u);
        if (!capa_logada) { capa_logada = true; registrar("reproductor: carátula %dx%d\n", capa.larg, capa.alt); }
    } else {
        /* barras de espectro */
        int area_a = z.a - texto_a - 16;
        int bw = (z.l - 40) / ESPEC_BARRAS;
        if (bw < 3) bw = 3;
        int total = bw * ESPEC_BARRAS;
        int x0 = z.x + (z.l - total) / 2, base = z.y + 8 + area_a;
        for (int k = 0; k < ESPEC_BARRAS; k++) {
            int h = area_a * barras[k] / 100;
            if (h < 2) h = 2;
            uint32_t cor = barras[k] > 70 ? 0x8CF0B8 : barras[k] > 35 ? ACENTO_CL : ACENTO;
            g_retangulo(j, x0 + k * bw + 1, base - h, bw - 2, h, cor);
        }
        if (!espec_logado) { espec_logado = true; registrar("reproductor: visualización de espectro\n"); }
    }
    rect_t zt = { z.x, z.y + z.a - texto_a, z.l, texto_a };
    texto_centrado(&zt, zt.y + 4, t1, COR_BRANCO);
    if (t2[0]) texto_centrado(&zt, zt.y + 4 + LETRA_A + 2, t2, PAN_APAGADO);
}

static void pintar_video(void)
{
    rect_t z = r_video;
    if (med == MED_AUDIO) { pintar_audio(); return; }
    g_retangulo(j, z.x, z.y, z.l, z.a, 0);
    if (med == MED_NENHUM) {
        rect_t zt = z;
        texto_centrado(&zt, z.y + z.a / 2 - 20, _("Reproductor de SaguiOS"), COR_BRANCO);
        texto_centrado(&zt, z.y + z.a / 2, _("Abre un archivo o una carpeta con Ctrl+O"), PAN_APAGADO);
        return;
    }
    if (!tem_quadro || !img.px) {
        g_texto(j, z.x + 16, z.y + 10, _("Sin cuadro"), COR_TEXTO);
        return;
    }
    /* Se ajusta a la ventana guardando la proporción (como el visor). */
    unsigned escala_l = (unsigned)z.l * 65536u / (unsigned)img.larg;
    unsigned escala_a = (unsigned)z.a * 65536u / (unsigned)img.alt;
    unsigned escala = escala_l < escala_a ? escala_l : escala_a;
    int l = (int)((unsigned)img.larg * escala / 65536u);
    int a = (int)((unsigned)img.alt * escala / 65536u);
    if (l < 1) l = 1;
    if (a < 1) a = 1;
    int x = z.x + (z.l - l) / 2, y = z.y + (z.a - a) / 2;
    bool suave = escala < 65536u;   /* al reducir, el filtro bilineal */
    imagem_desenhar(j->px, j->larg, z.x, z.y, z.l, z.a, &img, x, y, l, a, suave);
    if (!primeiro_desenhado) {
        primeiro_desenhado = true;
        registrar("reproductor: primer cuadro en la ventana\n");
    }
}

/* El tiempo al pasar el ratón por la barra de posición (flota sobre el vídeo). */
static void pintar_dica(void)
{
    if (hover_seek < 0 || (tela_cheia && !ctrl_visivel)) return;
    char t[16];
    formatar_tempo(t, sizeof t, hover_seek);
    int w = (int)tam_texto(t) * LETRA_L + 10;
    int x = seek_x_de_ms(hover_seek) - w / 2;
    if (x < 2) x = 2;
    if (x + w > j->larg - 2) x = j->larg - 2 - w;
    int y = r_pan.y - 22;
    g_retangulo(j, x, y, w, 20, 0x000000);
    g_moldura(j, x, y, w, 20, ACENTO_CL);
    g_texto(j, x + 5, y + 2, t, COR_BRANCO);
}

static void desenhar_quadro(void)
{
    pintar_video();
    if (tela_cheia && ctrl_visivel) pintar_controles();
    pintar_dica();
    int ay = r_video.y, aa = r_video.a;
    if (tela_cheia && ctrl_visivel) aa = j->alt - ay;
    jan_mostrar_area(j, r_video.x, ay, r_video.l, aa);
}

static void desenhar_menu(void)
{
    if (tela_cheia) return;
    barra_menu_desenhar(j, &menu);
}

static void montar_menus(void);
static void desenhar_tudo(void)
{
    layout();
    if (tela_cheia) g_retangulo(j, 0, 0, j->larg, j->alt, 0);
    desenhar_quadro();
    if (!tela_cheia) {
        pintar_lista();
        jan_mostrar_area(j, r_lista.x, r_lista.y, r_lista.l, r_lista.a);
        desenhar_controles();
        montar_menus();
        barra_menu_iniciar(&menu, 0, 0, j->larg);
        desenhar_menu();
    }
}

static void atualizar_titulo(void)
{
    char t[140];
    if (med == MED_NENHUM) copia_texto(t, _("Reproductor de vídeo"), sizeof t);
    else formatar_texto(t, sizeof t, "%s - %s", nome_curto(caminho_video), _("Reproductor de vídeo"));
    titulo_janela(j->id, t);
}

/* ------------------------------------------------------------ el sonido del vídeo */

/* Abre la tarjeta de sonido (si la hay) para el audio del vídeo. */
static void abrir_som(void)
{
    audio_fd = -1;
    if (!vid) return;
    const video_info_t *info = video_info(vid);
    if (!info->tem_audio) return;
    taxa_audio = info->taxa_audio;
    canais_audio = (uint32_t)info->canais_audio;
    audio_fd = audio_abrir((int)taxa_audio, (int)canais_audio, 16);
    if (audio_fd < 0) {
        audio_fd = -1;
        registrar("reproductor: no hay tarjeta de sonido; el video va en silencio\n");
        return;
    }
    audio_volume(audio_fd, mudo ? 0 : volume);
}

/* Escribe el audio del intervalo que se acaba de decodificar. */
static void escrever_audio(void)
{
    if (audio_fd < 0) return;
    static int16_t pcm[PCM_CUADROS * 2];
    int n;
    while ((n = video_audio(vid, pcm, PCM_CUADROS)) > 0) {
        if (audio_escrever(audio_fd, pcm, (unsigned)n * canais_audio * 2) <= 0) break;
        ms_escritos += (uint32_t)n * 1000u / taxa_audio;
    }
}

/* ------------------------------------------------------------ el sonido del audio */

static void abrir_som_audio(void)
{
    if (audio_fd >= 0) { audio_fechar(audio_fd, 0); audio_fd = -1; }
    if (!ainf) return;
    taxa_audio = ainf->taxa;
    canais_audio = ainf->canais;
    audio_fd = audio_abrir((int)taxa_audio, (int)canais_audio, 16);
    if (audio_fd < 0) {
        audio_fd = -1;
        registrar("reproductor: no hay tarjeta de sonido; el audio va en silencio\n");
        return;
    }
    audio_volume(audio_fd, mudo ? 0 : volume);
}

/* Coloca el audio en el instante ms y reabre el flujo (así lo que sonaba se corta). */
static void audio_colocar(int ms)
{
    int d = (int)ainf->duracao_ms;
    if (ms < 0) ms = 0;
    if (d > 0 && ms > d) ms = d;
    audio_arquivo_buscar(aud, (uint32_t)ms);
    aud_base_ms = (uint32_t)ms;
    aud_quadros = 0;
    aud_fim_dec = false;
    aud_pos = ms;
    t_inicio = tempo_ms();
    if (tocando) abrir_som_audio();
    else if (audio_fd >= 0) { audio_fechar(audio_fd, 0); audio_fd = -1; }
}

/* Un paso del audio: lo que falta para llegar a ADELANTE_MS por delante del flujo. */
static void passo_audio(void)
{
    if (!aud || !tocando) return;
    if (audio_fd >= 0) {
        static int16_t pcm[1152 * 2];
        int pend = audio_pendente(audio_fd);
        if (pend < 0) pend = 0;
        for (int vez = 0; vez < 8 && !aud_fim_dec && pend < ADELANTE_MS; vez++) {
            int n = audio_arquivo_ler(aud, pcm, 1152);
            if (n <= 0) { aud_fim_dec = true; break; }
            for (int k = 0; k < n; k++) {
                int m = canais_audio == 2 ? (pcm[2 * k] + pcm[2 * k + 1]) / 2 : pcm[k];
                espec_anel[(espec_n + (uint32_t)k) & (ANEL_ESP - 1)] = (int16_t)m;
            }
            espec_n += (uint32_t)n;
            if (audio_escrever(audio_fd, pcm, (unsigned)n * canais_audio * 2) <= 0) { aud_fim_dec = true; break; }
            aud_quadros += (uint32_t)n;
            pend = audio_pendente(audio_fd);
            if (pend < 0) pend = 0;
        }
    }
}

/* ------------------------------------------------------------ lista de reproducción */

static void lista_limpiar(void)
{
    lista_n = 0; lista_atual = lista_sel = -1; lista_topo = 0;
    memset(lista_vista, 0, sizeof lista_vista);
}

static bool lista_anadir(const char *c)
{
    if (lista_n >= MAX_LISTA || !c[0]) return false;
    copia_texto(lista_c[lista_n], c, sizeof lista_c[0]);
    lista_vista[lista_n] = false;
    lista_n++;
    return true;
}

static int lista_anadir_carpeta(const char *pasta)
{
    entrada_pasta_t *v = NULL;
    int n = listar_tudo(pasta, &v);
    int add = 0;
    for (int i = 0; i < n; i++) {
        if (v[i].tipo != TIPO_ARQUIVO || !eh_media(v[i].nome)) continue;
        char c[200];
        caminho_juntar(c, sizeof c, pasta, v[i].nome);
        if (lista_anadir(c)) add++;
    }
    if (v) libera(v);
    return add;
}

/* Lee un .m3u: una ruta por línea (las relativas, respecto a la carpeta de la lista). */
static int lista_anadir_m3u(const char *m3u)
{
    int fd = abrir(m3u, ABRIR_LER);
    if (fd < 0) return -1;
    unsigned tam = 0;
    char *t = ler_todo_fd(fd, &tam);
    fechar(fd);
    if (!t) return -1;
    char pasta[200];
    copia_texto(pasta, m3u, sizeof pasta);
    char *b = acha_ultimo_char(pasta, '/');
    if (b && b != pasta) *b = 0; else copia_texto(pasta, "/", sizeof pasta);
    int add = 0;
    char *p = t;
    while (*p) {
        char *e = p;
        while (*e && *e != '\n') e++;
        char sv = *e;
        *e = 0;
        char *fim_l = e;
        while (fim_l > p && (fim_l[-1] == '\r' || fim_l[-1] == ' ')) *--fim_l = 0;
        if (*p && *p != '#') {
            char c[200];
            if (*p == '/') copia_texto(c, p, sizeof c); else caminho_juntar(c, sizeof c, pasta, p);
            if (lista_anadir(c)) add++;
        }
        if (!sv) break;
        p = e + 1;
    }
    libera(t);
    return add;
}

static int lista_gravar_m3u(const char *m3u)
{
    int fd = abrir(m3u, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) return -1;
    escrever_texto(fd, "#EXTM3U\n");
    for (int i = 0; i < lista_n; i++) { escrever_texto(fd, lista_c[i]); escrever_texto(fd, "\n"); }
    fechar(fd);
    return 0;
}

/* Añade lo que sea (archivo, carpeta o lista .m3u). */
static int lista_anadir_lo_que_sea(const char *c)
{
    info_arquivo_t ia;
    if (info_arquivo(c, &ia) == 0 && ia.tipo == TIPO_PASTA) return lista_anadir_carpeta(c);
    if (ext_igual(c, ".m3u") || ext_igual(c, ".m3u8")) return lista_anadir_m3u(c);
    return lista_anadir(c) ? 1 : 0;
}

/* ------------------------------------------------------------ abrir y cerrar pistas */

static void fechar_medio(void)
{
    if (audio_fd >= 0) { audio_fechar(audio_fd, 0); audio_fd = -1; }
    if (vid) { video_fechar(vid); vid = NULL; }
    if (aud) { audio_arquivo_fechar(aud); aud = NULL; ainf = NULL; }
    if (img.px) imagem_liberar(&img);
    memset(&img, 0, sizeof img);
    if (capa.px) imagem_liberar(&capa);
    memset(&capa, 0, sizeof capa);
    tem_quadro = false;
    med = MED_NENHUM;
}

/* Abre el archivo (sin dibujar nada). false = no se pudo; *erro_txt dice por qué. */
static bool abrir_arquivo(const char *c, const char **erro_txt)
{
    fechar_medio();
    geracao++;
    bool primeiro_audio = eh_audio(c);
    int erro = 0;
    for (int tentativa = 0; tentativa < 2; tentativa++) {
        bool como_audio = tentativa == 0 ? primeiro_audio : !primeiro_audio;
        if (como_audio) {
            aud = audio_arquivo_abrir(c, &erro);
            if (aud) {
                ainf = audio_arquivo_info(aud);
                med = MED_AUDIO;
                copia_texto(caminho_video, c, sizeof caminho_video);
                carregar_capa(c);
                break;
            }
            if (erro != AUDIO_ERRO_FORMATO) { *erro_txt = audio_erro_texto(erro); return false; }
            *erro_txt = audio_erro_texto(erro);
        } else {
            vid = video_abrir(c, &erro);
            if (vid) {
                med = MED_VIDEO;
                copia_texto(caminho_video, c, sizeof caminho_video);
                break;
            }
            if (erro != VIDEO_ERRO_FORMATO && erro != VIDEO_ERRO_NAO_SUPORTADO) { *erro_txt = video_erro_texto(erro); return false; }
            *erro_txt = video_erro_texto(erro);
        }
    }
    if (med == MED_NENHUM) { copia_texto(ultimo_erro, *erro_txt, sizeof ultimo_erro); return false; }
    quadro_atual = -1;
    quadros_vistos = quadros_saltados = 0;
    ms_decodificar = 0;
    ms_escritos = 0;
    t_inicio = tempo_ms();
    fim = false;
    aviso[0] = 0;
    if (med == MED_VIDEO) {
        const video_info_t *info = video_info(vid);
        fps = info->fps > 0 ? info->fps : 30;
        quadros_total = info->quadros;
        abrir_som();
    } else {
        aud_base_ms = 0; aud_quadros = 0; aud_fim_dec = false; aud_pos = 0;
        espec_n = 0;
        memset(barras, 0, sizeof barras);
        espec_logado = false;
        abrir_som_audio();
    }
    return true;
}

static void terminou(void);

/* Abre la pista idx de la lista y la pone a tocar. false = no se pudo (se avisa). */
static bool abrir_pista(int idx)
{
    if (idx < 0 || idx >= lista_n) return false;
    const char *erro_txt = "";
    tocando = true;
    if (!abrir_arquivo(lista_c[idx], &erro_txt)) {
        formatar_texto(aviso, sizeof aviso, "No se ha podido abrir %s: %s", nome_curto(lista_c[idx]), erro_txt);
        registrar("reproductor: error al abrir %s (%s)\n", lista_c[idx], erro_txt);
        lista_atual = idx;
        lista_vista[idx] = true;
        tocando = false;
        return false;
    }
    lista_atual = idx;
    lista_vista[idx] = true;
    lista_sel = idx;
    if (med == MED_VIDEO) {
        const video_info_t *info = video_info(vid);
        registrar("reproductor: abierto %s (%s) %dx%d %d fps, %d cuadros\n", caminho_video, info->nome_formato,
                  info->larg, info->alt, fps, quadros_total);
    } else {
        registrar("reproductor: abierto %s (%s) %u Hz, %u canales, %u ms\n", caminho_video, ainf->nome_formato,
                  ainf->taxa, ainf->canais, ainf->duracao_ms);
    }
    if (lista_n > 1) registrar("reproductor: pista %d de %d\n", idx + 1, lista_n);
    return true;
}

/* La pista que sigue (aleatoria o la siguiente); -1 = no hay. */
static int proxima_pista(void)
{
    if (lista_n == 0) return -1;
    if (aleatorio && lista_n > 1) {
        int libres = 0;
        for (int i = 0; i < lista_n; i++) if (!lista_vista[i] && i != lista_atual) libres++;
        if (libres == 0) {
            if (!bucle) return -1;
            memset(lista_vista, 0, sizeof lista_vista);
            if (lista_atual >= 0) lista_vista[lista_atual] = true;
            libres = lista_n - 1;
        }
        int k = (int)aleatorio_n((unsigned)libres);
        for (int i = 0; i < lista_n; i++)
            if (!lista_vista[i] && i != lista_atual && k-- == 0) return i;
        return -1;
    }
    int i = lista_atual + 1;
    if (i >= lista_n) return bucle ? 0 : -1;
    return i;
}

static void passar_a(int idx)
{
    if (idx < 0) return;
    if (!abrir_pista(idx)) return;
    atualizar_titulo();
    desenhar_tudo();
}

/* Al acabar la pista: la siguiente de la lista, o parar. */
static void reiniciar_pista(void);
static void terminou(void)
{
    int n = proxima_pista();
    if (n >= 0 && n != lista_atual) {
        /* si una pista no se abre, se prueba con las siguientes (como mucho una vuelta) */
        for (int t = 0; t < lista_n && n >= 0; t++) {
            if (abrir_pista(n)) { atualizar_titulo(); desenhar_tudo(); return; }
            lista_atual = n;
            n = proxima_pista();
            if (n == lista_atual) break;
        }
        atualizar_titulo();
        desenhar_tudo();
        return;
    }
    if (n >= 0 && bucle) { reiniciar_pista(); return; }
    tocando = false;
    fim = true;
    formatar_texto(aviso, sizeof aviso, med == MED_AUDIO ? "Fin del audio" : "Fin del vídeo");
    registrar("reproductor: fin del video\n");
    desenhar_controles();
}

/* ------------------------------------------------------------ búsqueda */

/* Salta al instante pedido (ms desde el principio). */
static void buscar_ms(int ms)
{
    geracao++;
    if (med == MED_AUDIO) {
        audio_colocar(ms);
        char t[16];
        formatar_tempo(t, sizeof t, aud_pos);
        formatar_texto(aviso, sizeof aviso, "En el segundo %s", t);
        registrar("reproductor: salto a %s\n", t);
        desenhar_controles();
        return;
    }
    if (!vid) return;
    int q = ms * fps / 1000;
    if (q < 0) q = 0;
    if (q >= quadros_total) q = quadros_total - 1;
    if (video_buscar(vid, q) != 0) return;
    fim = false;
    quadro_atual = q - 1;
    t_inicio = tempo_ms() - (unsigned)q * periodo_ms();
    ms_escritos = (uint32_t)q * periodo_ms();
    /* El audio que sonaba era del punto anterior: se suelta y se abre otra
     * vez la tarjeta, para que el sonido y la imagen vuelvan a ir juntos. */
    if (audio_fd >= 0) {
        audio_fechar(audio_fd, 0);
        audio_fd = -1;
        abrir_som();
    }
    char t[80];
    formatar_tempo(t, sizeof t, q * (int)periodo_ms());
    formatar_texto(aviso, sizeof aviso, "En el segundo %s (cuadro %d de %d)", t, q + 1, quadros_total);
    registrar("reproductor: salto a %s (cuadro %d)\n", t, q);
    desenhar_controles();
}

static void reiniciar_pista(void)
{
    geracao++;
    fim = false;
    if (med == MED_VIDEO) {
        video_buscar(vid, 0);
        quadro_atual = -1;
        t_inicio = tempo_ms();
        ms_escritos = 0;
        if (audio_fd >= 0) { audio_fechar(audio_fd, 0); audio_fd = -1; abrir_som(); }
    } else if (med == MED_AUDIO) {
        audio_colocar(0);
    }
}

static void mudar_volume(int delta)
{
    volume += delta;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (audio_fd >= 0) audio_volume(audio_fd, mudo ? 0 : volume);
    formatar_texto(aviso, sizeof aviso, "Volumen %d%%%s", volume, mudo ? " (silencio)" : "");
    registrar("reproductor: volumen %d\n", volume);
    desenhar_controles();
}

static void mudar_mudo(void)
{
    mudo = !mudo;
    if (audio_fd >= 0) audio_volume(audio_fd, mudo ? 0 : volume);
    formatar_texto(aviso, sizeof aviso, mudo ? "Silencio" : "Con sonido");
    registrar("reproductor: %s\n", mudo ? "silencio" : "con sonido");
    desenhar_controles();
}

/* ------------------------------------------------------------ estadísticas */

static void mostrar_estatisticas(void)
{
    uint32_t ms = tempo_ms() - t_inicio;
    int fps_medio = ms ? quadros_vistos * 1000 / ms : 0;
    formatar_texto(aviso, sizeof aviso, "%d fps medios, %d cuadros saltados, %u ms de decodificar (%u de %u)",
                   fps_medio, quadros_saltados, ms_decodificar, ms_decodificar, ms);
    registrar("reproductor: %d fps medios, %d cuadros saltados, %u ms de decodificar\n",
              fps_medio, quadros_saltados, ms_decodificar);
    desenhar_controles();
}

/* ------------------------------------------------------------ acciones */

static void mudar_pausa(void)
{
    if (med == MED_NENHUM) return;
    /* Si la pista se había acabado, el espacio la vuelve a empezar. */
    if (fim) {
        reiniciar_pista();
        tocando = true;
        if (med == MED_AUDIO) abrir_som_audio();
        formatar_texto(aviso, sizeof aviso, "En marcha");
        registrar("reproductor: en marcha\n");
        desenhar_controles();
        return;
    }
    if (med == MED_AUDIO) {
        if (tocando) {
            audio_posicao_agora();
            tocando = false;
            if (audio_fd >= 0) { audio_fechar(audio_fd, 0); audio_fd = -1; }
        } else {
            tocando = true;
            audio_colocar(aud_pos);
        }
    } else {
        tocando = !tocando;
        /* Al seguir, el reloj del sistema no debe contar lo que duró la pausa. */
        if (tocando && audio_fd < 0) t_inicio = tempo_ms() - (unsigned)(quadro_atual < 0 ? 0 : quadro_atual) * periodo_ms();
    }
    formatar_texto(aviso, sizeof aviso, tocando ? "En marcha" : "En pausa");
    registrar("reproductor: %s\n", tocando ? "en marcha" : "en pausa");
    desenhar_controles();
}

/* Parar: vuelve al principio y se queda en pausa, con el primer cuadro a la vista. */
static void parar(void)
{
    if (med == MED_NENHUM) return;
    geracao++;
    tocando = false;
    fim = false;
    if (med == MED_VIDEO) {
        imagem_t nova = { 0 };
        if (video_buscar(vid, 0) == 0 && video_quadro(vid, &nova) == 0) {
            if (img.px) imagem_liberar(&img);
            img = nova;
            tem_quadro = true;
        } else if (nova.px) imagem_liberar(&nova);
        video_buscar(vid, 0);
        quadro_atual = -1;
        ms_escritos = 0;
        t_inicio = tempo_ms();
        if (audio_fd >= 0) { audio_fechar(audio_fd, 0); audio_fd = -1; abrir_som(); }
    } else {
        audio_colocar(0);
        memset(barras, 0, sizeof barras);
    }
    formatar_texto(aviso, sizeof aviso, "Parado");
    registrar("reproductor: parado\n");
    desenhar_quadro();
    desenhar_controles();
}

static void anterior(void)
{
    if (med != MED_NENHUM && posicao_ms() > 3000) { buscar_ms(0); return; }
    int i = lista_atual - 1;
    if (i < 0) i = bucle ? lista_n - 1 : 0;
    if (lista_n == 0) return;
    if (i == lista_atual) { buscar_ms(0); return; }
    passar_a(i);
}

static void siguiente(void)
{
    int n = proxima_pista();
    if (n < 0) { formatar_texto(aviso, sizeof aviso, "No hay más pistas"); desenhar_controles(); return; }
    if (n == lista_atual) { reiniciar_pista(); return; }
    passar_a(n);
}

/* Un cuadro hacia delante o hacia atrás (con la pausa puesta). */
static void passo_quadro(int passo)
{
    if (med != MED_VIDEO) return;
    int q = quadro_atual + passo;
    if (q < 0) q = 0;
    if (q >= quadros_total) q = quadros_total - 1;
    if (!tocando) {
        /* Con la pausa puesta se lee el cuadro y se muestra. */
        imagem_t nova = { 0 };
        if (video_buscar(vid, q) == 0 && video_quadro(vid, &nova) == 0) {
            if (img.px) imagem_liberar(&img);
            img = nova;
            tem_quadro = true;
            quadro_atual = q;
            formatar_texto(aviso, sizeof aviso, "Cuadro %d de %d", q + 1, quadros_total);
            registrar("reproductor: cuadro %d\n", q);
            desenhar_quadro();
            desenhar_controles();
            return;
        }
        if (nova.px) imagem_liberar(&nova);
    }
    buscar_ms(q * (int)periodo_ms());
}

#define FILTRO_MEDIA "Vídeo y audio (*.mp4;*.m4v;*.avi;*.mpg;*.mpeg;*.mp3;*.flac;*.wav;*.mid)|*.mp4;*.m4v;*.avi;*.mpg;*.mpeg;*.mp3;*.flac;*.wav;*.mid|Todos los archivos (*.*)|*"

/* Los diálogos paran la reproducción (el bucle no atiende al sonido mientras esperan). */
static void antes_dialogo(bool *estaba)
{
    *estaba = tocando && med != MED_NENHUM;
    if (*estaba) mudar_pausa();
}

static void despues_dialogo(bool estaba)
{
    if (estaba && !tocando) mudar_pausa();
    desenhar_tudo();
}

static void pedir_arquivo(void)
{
    char c[200];
    if (caminho_video[0]) copia_texto(c, caminho_video, sizeof c);
    else { copia_texto(c, "/apps/videos", sizeof c); junta_texto(c, "/", sizeof c); }
    bool estaba;
    antes_dialogo(&estaba);
    bool ok = dialogo_abrir(_("Abrir vídeo o audio"), FILTRO_MEDIA, c, c, sizeof c) && c[0];
    if (!ok) { despues_dialogo(estaba); return; }
    int antes = lista_n;
    if (lista_anadir_lo_que_sea(c) <= 0 && lista_n == antes) { despues_dialogo(false); return; }
    registrar("reproductor: lista de %d pistas\n", lista_n);
    if (!abrir_pista(antes)) { desenhar_tudo(); return; }
    atualizar_titulo();
    desenhar_tudo();
}

static void lista_anadir_dialogo(void)
{
    char c[200];
    copia_texto(c, caminho_video[0] ? caminho_video : "/apps/videos/", sizeof c);
    bool estaba;
    antes_dialogo(&estaba);
    if (dialogo_abrir(_("Añadir a la lista"), FILTRO_MEDIA, c, c, sizeof c) && c[0]) {
        int a = lista_anadir_lo_que_sea(c);
        registrar("reproductor: lista de %d pistas (+%d)\n", lista_n, a);
        if (med == MED_NENHUM && lista_n > 0) { despues_dialogo(false); abrir_pista(lista_n - a); atualizar_titulo(); desenhar_tudo(); return; }
    }
    despues_dialogo(estaba);
}

static void lista_carpeta_dialogo(void)
{
    char c[200];
    copia_texto(c, "/apps/videos", sizeof c);
    bool estaba;
    antes_dialogo(&estaba);
    if (dialogo_carpeta(_("Añadir carpeta"), c, c, sizeof c) && c[0]) {
        int antes = lista_n;
        int a = lista_anadir_carpeta(c);
        registrar("reproductor: lista de %d pistas (+%d)\n", lista_n, a);
        if (med == MED_NENHUM && a > 0) { despues_dialogo(false); abrir_pista(antes); atualizar_titulo(); desenhar_tudo(); return; }
    }
    despues_dialogo(estaba);
}

static void lista_abrir_m3u_dialogo(void)
{
    char c[200];
    copia_texto(c, "/home/", sizeof c);
    bool estaba;
    antes_dialogo(&estaba);
    if (dialogo_abrir(_("Abrir lista"), "Listas M3U (*.m3u)|*.m3u;*.m3u8|Todos los archivos (*.*)|*", c, c, sizeof c) && c[0]) {
        lista_limpiar();
        int a = lista_anadir_m3u(c);
        registrar("reproductor: lista %s: %d pistas\n", c, a);
        if (a > 0) { despues_dialogo(false); abrir_pista(0); atualizar_titulo(); desenhar_tudo(); return; }
        formatar_texto(aviso, sizeof aviso, "La lista está vacía");
    }
    despues_dialogo(estaba);
}

static void lista_gravar_dialogo(void)
{
    char c[200];
    copia_texto(c, "/home/lista.m3u", sizeof c);
    bool estaba;
    antes_dialogo(&estaba);
    if (dialogo_guardar(_("Guardar lista"), "Listas M3U (*.m3u)|*.m3u", c, c, sizeof c) && c[0]) {
        if (lista_gravar_m3u(c) == 0) {
            registrar("reproductor: lista guardada en %s (%d pistas)\n", c, lista_n);
            formatar_texto(aviso, sizeof aviso, "Lista guardada");
        } else {
            formatar_texto(aviso, sizeof aviso, "No se pudo guardar la lista");
        }
    }
    despues_dialogo(estaba);
}

static void lista_quitar(void)
{
    int k = lista_sel;
    if (k < 0 || k >= lista_n) return;
    bool era_atual = k == lista_atual;
    for (int i = k; i + 1 < lista_n; i++) {
        copia_texto(lista_c[i], lista_c[i + 1], sizeof lista_c[0]);
        lista_vista[i] = lista_vista[i + 1];
    }
    lista_n--;
    if (lista_atual > k) lista_atual--;
    if (lista_sel >= lista_n) lista_sel = lista_n - 1;
    registrar("reproductor: pista quitada, quedan %d\n", lista_n);
    if (era_atual) {
        lista_atual = -1;
        if (lista_n > 0) { abrir_pista(k < lista_n ? k : lista_n - 1); atualizar_titulo(); }
        else { fechar_medio(); atualizar_titulo(); }
    }
    desenhar_tudo();
}

static void lista_mover(int d)
{
    int k = lista_sel, o = k + d;
    if (k < 0 || o < 0 || o >= lista_n) return;
    char tmp[200];
    copia_texto(tmp, lista_c[k], sizeof tmp);
    copia_texto(lista_c[k], lista_c[o], sizeof lista_c[0]);
    copia_texto(lista_c[o], tmp, sizeof lista_c[0]);
    bool v = lista_vista[k]; lista_vista[k] = lista_vista[o]; lista_vista[o] = v;
    if (lista_atual == k) lista_atual = o; else if (lista_atual == o) lista_atual = k;
    lista_sel = o;
    registrar("reproductor: pista movida a la posición %d\n", o + 1);
    desenhar_lista();
}

static void mostrar_lista(bool ver)
{
    lista_vis = ver;
    registrar("reproductor: lista %s\n", ver ? "visible" : "oculta");
    desenhar_tudo();
}

static void alternar_aleatorio(void)
{
    aleatorio = !aleatorio;
    memset(lista_vista, 0, sizeof lista_vista);
    if (lista_atual >= 0) lista_vista[lista_atual] = true;
    formatar_texto(aviso, sizeof aviso, aleatorio ? "Aleatorio activado" : "Aleatorio desactivado");
    registrar("reproductor: aleatorio %s\n", aleatorio ? "activado" : "desactivado");
    desenhar_controles();
}

static void alternar_bucle(void)
{
    bucle = !bucle;
    formatar_texto(aviso, sizeof aviso, bucle ? "Bucle activado" : "Bucle desactivado");
    registrar("reproductor: bucle %s\n", bucle ? "activado" : "desactivado");
    desenhar_controles();
}

static void propiedades(void)
{
    if (med == MED_NENHUM) return;
    char t[700], a[16];
    info_arquivo_t ia = { 0 };
    info_arquivo(caminho_video, &ia);
    char tam[24];
    formatar_tamanho(tam, sizeof tam, ia.tamanho);
    int n = formatar_texto(t, sizeof t, "%s: %s\n%s: %s\n", _("Archivo"), nome_curto(caminho_video), _("Tamaño"), tam);
    if (med == MED_VIDEO) {
        const video_info_t *i = video_info(vid);
        char fmt[80];
        copia_texto(fmt, i->nome_formato, sizeof fmt);
        char *en = NULL;
        for (char *p = fmt; *p; p++) if (p[0] == ' ' && p[1] == 'e' && p[2] == 'n' && p[3] == ' ') { en = p; break; }
        formatar_tempo(a, sizeof a, (int)i->duracao_ms);
        if (en) {
            *en = 0;
            n += formatar_texto(t + n, sizeof t - (size_t)n, "%s: %s\n%s: %s\n", _("Formato"), en + 4, _("Códecs"), fmt);
        } else {
            n += formatar_texto(t + n, sizeof t - (size_t)n, "%s: %s\n", _("Formato"), fmt);
        }
        n += formatar_texto(t + n, sizeof t - (size_t)n, "%s: %d x %d, %d fps\n%s: %s\n", _("Resolución"), i->larg, i->alt, i->fps,
                            _("Duración"), a);
        if (i->tem_audio)
            formatar_texto(t + n, sizeof t - (size_t)n, "%s: %u Hz, %d %s", _("Audio"), i->taxa_audio, i->canais_audio, _("canales"));
        else
            formatar_texto(t + n, sizeof t - (size_t)n, "%s: %s", _("Audio"), _("sin audio"));
    } else {
        formatar_tempo(a, sizeof a, (int)ainf->duracao_ms);
        n += formatar_texto(t + n, sizeof t - (size_t)n, "%s: %s\n%s: %u Hz, %u %s\n%s: %s\n", _("Formato"), ainf->nome_formato,
                            _("Audio"), ainf->taxa, ainf->canais, _("canales"), _("Duración"), a);
        if (ainf->kbps) n += formatar_texto(t + n, sizeof t - (size_t)n, "%s: %u kbit/s\n", _("Tasa de bits"), ainf->kbps);
        if (ainf->titulo[0]) n += formatar_texto(t + n, sizeof t - (size_t)n, "%s: %s\n", _("Título"), ainf->titulo);
        if (ainf->artista[0]) n += formatar_texto(t + n, sizeof t - (size_t)n, "%s: %s\n", _("Artista"), ainf->artista);
        if (ainf->album[0]) formatar_texto(t + n, sizeof t - (size_t)n, "%s: %s\n", _("Álbum"), ainf->album);
    }
    registrar("reproductor: propiedades\n");
    bool estaba;
    antes_dialogo(&estaba);
    mensagem(_("Propiedades"), t);
    despues_dialogo(estaba);
}

static void ayuda_atajos(void)
{
    bool estaba;
    antes_dialogo(&estaba);
    mensagem(_("Atajos de teclado"),
             "Espacio o Ctrl+P: reproducir y pausa\nCtrl+S: parar\nCtrl+B y Ctrl+F: anterior y siguiente\n"
             "F8: silencio   F9 y F10: volumen\nFlechas: 5 segundos   Inicio y Fin\nF11 o doble clic: pantalla completa\n"
             "Ctrl+L: lista   Ctrl+O: abrir   A: aleatorio   L: repetir");
    despues_dialogo(estaba);
}

static void acerca_de(void)
{
    bool estaba;
    antes_dialogo(&estaba);
    mensagem(_("Acerca del reproductor"), "Reproductor de SaguiOS\nVídeo MJPEG, MPEG-1 y H.264/AAC en MP4; audio MP3, FLAC, WAV y MIDI.");
    despues_dialogo(estaba);
}

/* ------------------------------------------------------------ menús */

static void menu_item(char *b, int max, const char *t, const char *atajo, bool ativo, int marca)
{
    char l[100];
    formatar_texto(l, sizeof l, "%s%s%s%s%s", ativo ? "" : "~", marca == 1 ? "[x] " : marca == 0 ? "[ ] " : "", t,
                   atajo[0] ? "\t" : "", atajo);
    if (b[0]) junta_texto(b, "\n", (size_t)max);
    junta_texto(b, l, (size_t)max);
}

static void menu_sep(char *b, int max) { junta_texto(b, "\n-", (size_t)max); }

static void montar_menus(void)
{
    for (int i = 0; i < 4; i++) menu_txt[i][0] = 0;
    char *m;
    int mx = (int)sizeof menu_txt[0];
    bool tem = med != MED_NENHUM, lis = lista_n > 0;
    m = menu_txt[0];
    menu_item(m, mx, _("Abrir archivo..."), "Ctrl+O", true, -1);
    menu_item(m, mx, _("Abrir carpeta..."), "", true, -1);
    menu_item(m, mx, _("Abrir lista (.m3u)..."), "", true, -1);
    menu_item(m, mx, _("Guardar lista..."), "", lis, -1);
    menu_sep(m, mx);
    menu_item(m, mx, _("Propiedades..."), "", tem, -1);
    menu_sep(m, mx);
    menu_item(m, mx, _("Salir"), "", true, -1);
    m = menu_txt[1];
    menu_item(m, mx, _("Lista de reproducción"), "Ctrl+L", true, lista_vis);
    menu_item(m, mx, _("Pantalla completa"), "F11", true, tela_cheia);
    menu_item(m, mx, _("Estadísticas"), "I", tem, -1);
    m = menu_txt[2];
    menu_item(m, mx, _("Reproducir / Pausa"), "Ctrl+P", tem, -1);
    menu_item(m, mx, _("Parar"), "Ctrl+S", tem, -1);
    menu_item(m, mx, _("Anterior"), "Ctrl+B", lis, -1);
    menu_item(m, mx, _("Siguiente"), "Ctrl+F", lis, -1);
    menu_sep(m, mx);
    menu_item(m, mx, _("Aleatorio"), "A", true, aleatorio);
    menu_item(m, mx, _("Repetir"), "L", true, bucle);
    menu_sep(m, mx);
    menu_item(m, mx, _("Silencio"), "F8", true, mudo);
    menu_item(m, mx, _("Subir volumen"), "F10", true, -1);
    menu_item(m, mx, _("Bajar volumen"), "F9", true, -1);
    m = menu_txt[3];
    menu_item(m, mx, _("Atajos de teclado"), "", true, -1);
    menu_item(m, mx, _("Acerca del reproductor"), "", true, -1);
    menu.n = 4;
    menu.nomes[0] = _("Archivo");
    menu.nomes[1] = _("Ver");
    menu.nomes[2] = _("Reproducir");
    menu.nomes[3] = _("Ayuda");
    for (int i = 0; i < 4; i++) menu.itens[i] = menu_txt[i];
}

static void carpeta_dialogo_abrir(void)
{
    char c[200];
    copia_texto(c, "/apps/videos", sizeof c);
    bool estaba;
    antes_dialogo(&estaba);
    if (dialogo_carpeta(_("Abrir carpeta"), c, c, sizeof c) && c[0]) {
        lista_limpiar();
        int a = lista_anadir_carpeta(c);
        registrar("reproductor: carpeta %s: %d pistas\n", c, a);
        if (a > 0) { despues_dialogo(false); abrir_pista(0); atualizar_titulo(); desenhar_tudo(); return; }
        formatar_texto(aviso, sizeof aviso, "La carpeta no tiene vídeo ni audio");
    }
    despues_dialogo(estaba);
}

static bool tratar_menu(int r)
{
    int m = r / 100, k = r % 100;
    switch (m) {
    case 0:
        switch (k) {
        case 0: pedir_arquivo(); break;
        case 1: carpeta_dialogo_abrir(); break;
        case 2: lista_abrir_m3u_dialogo(); break;
        case 3: lista_gravar_dialogo(); break;
        case 5: propiedades(); break;
        case 7: return false;
        }
        break;
    case 1:
        switch (k) {
        case 0: mostrar_lista(!lista_vis); break;
        case 1: jan_tela_cheia(j, !tela_cheia); break;
        case 2: mostrar_estatisticas(); break;
        }
        break;
    case 2:
        switch (k) {
        case 0: mudar_pausa(); break;
        case 1: parar(); break;
        case 2: anterior(); break;
        case 3: siguiente(); break;
        case 5: alternar_aleatorio(); break;
        case 6: alternar_bucle(); break;
        case 8: mudar_mudo(); break;
        case 9: mudar_volume(5); break;
        case 10: mudar_volume(-5); break;
        }
        break;
    case 3:
        if (k == 0) ayuda_atajos(); else if (k == 1) acerca_de();
        break;
    }
    return true;
}

/* ------------------------------------------------------------ eventos */

static void ocultar_ou_mostrar_controles(bool ver)
{
    if (!tela_cheia || ctrl_visivel == ver) return;
    ctrl_visivel = ver;
    jan_cursor_visivel(j, ver);
    desenhar_quadro();
}

static void definir_volume_de_x(int x)
{
    int v = (x - r_vol.x) * 100 / (r_vol.l - 1);
    if (v < 0) v = 0;
    if (v > 100) v = 100;
    volume = v;
    if (mudo && v > 0) mudo = false;
    if (audio_fd >= 0) audio_volume(audio_fd, mudo ? 0 : volume);
    desenhar_controles();
}

static bool tratar_clique(evento_t *e)
{
    if (!(e->botoes & 1)) return true;
    int x = e->x, y = e->y;
    if (!tela_cheia && y < MENU_A) {
        montar_menus();
        int r = barra_menu_tratar(j, &menu, e);
        if (r >= 0) return tratar_menu(r);
        return true;
    }
    if (tela_cheia && !ctrl_visivel) {
        if (e->cliques == 2) jan_tela_cheia(j, false);
        return true;
    }
    if ((!tela_cheia || ctrl_visivel) && y >= r_pan.y) {
        aviso[0] = 0;
        if (dentro(&r_seek, x, y) && duracao_ms() > 0) {
            arrasta_seek = true;
            arrasta_ms = seek_ms_de_x(x);
            desenhar_controles();
        } else if (dentro(&r_vol, x, y)) {
            arrasta_vol = true;
            definir_volume_de_x(x);
        } else if (dentro(&r_play, x, y)) mudar_pausa();
        else if (dentro(&r_prev, x, y)) anterior();
        else if (dentro(&r_stop, x, y)) parar();
        else if (dentro(&r_next, x, y)) siguiente();
        else if (dentro(&r_mute, x, y)) mudar_mudo();
        else if (dentro(&r_alea, x, y)) alternar_aleatorio();
        else if (dentro(&r_rep, x, y)) alternar_bucle();
        else if (dentro(&r_listab, x, y)) mostrar_lista(!lista_vis);
        else if (dentro(&r_full, x, y)) jan_tela_cheia(j, !tela_cheia);
        return true;
    }
    if (dentro(&r_lista, x, y)) {
        for (int i = 0; i < 5; i++) {
            rect_t rb = { bt_lista[i].x, bt_lista[i].y, bt_lista[i].l, bt_lista[i].a };
            if (dentro(&rb, x, y)) {
                bt_lista[i].apertado = true;
                botao_desenhar(j, &bt_lista[i]);
                switch (i) {
                case 0: lista_anadir_dialogo(); break;
                case 1: lista_carpeta_dialogo(); break;
                case 2: lista_quitar(); break;
                case 3: lista_mover(-1); break;
                case 4: lista_mover(1); break;
                }
                for (int b = 0; b < 5; b++) bt_lista[b].apertado = false;
                desenhar_lista();
                return true;
            }
        }
        int fila = (y - r_lista.y - 20) / 18;
        if (y >= r_lista.y + 20 && fila < lista_linhas() && lista_topo + fila < lista_n) {
            lista_sel = lista_topo + fila;
            if (e->cliques == 2) { passar_a(lista_sel); return true; }
            desenhar_lista();
        }
        return true;
    }
    if (dentro(&r_video, x, y) && e->cliques == 2) jan_tela_cheia(j, !tela_cheia);
    return true;
}

static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_REDIMENSIONAR) {
        bool cheia = e->tecla == JAN_ESTADO_TELA_CHEIA;
        bool ok = jan_redimensionar(j, e);
        if (!ok && (int)e->x == j->larg && (int)e->y == j->alt) ok = true;   /* mismo tamaño: solo cambia el estado */
        if (ok) {
            tela_cheia = cheia;
            ctrl_visivel = true;
            t_mover = tempo_ms();
            if (!cheia) jan_cursor_visivel(j, true);
            registrar("reproductor: ventana %dx%d%s\n", j->larg, j->alt, tela_cheia ? " (pantalla completa)" : "");
            desenhar_tudo();
        }
        return true;
    }
    if (e->tipo == EV_MENU) {
        int r = barra_menu_tratar(j, &menu, e);
        if (r >= 0) return tratar_menu(r);
        return true;
    }
    if (e->tipo == EV_MOVER) {
        t_mover = tempo_ms();
        if (tela_cheia && !ctrl_visivel) { ocultar_ou_mostrar_controles(true); return true; }
        if (arrasta_seek && (e->botoes & 1)) {
            arrasta_ms = seek_ms_de_x(e->x);
            hover_seek = arrasta_ms;
            desenhar_quadro();
            desenhar_controles();
            return true;
        }
        if (arrasta_vol && (e->botoes & 1)) { definir_volume_de_x(e->x); return true; }
        int h = -1;
        if (dentro(&r_seek, e->x, e->y) && duracao_ms() > 0 && (!tela_cheia || ctrl_visivel)) h = seek_ms_de_x(e->x);
        if (h != hover_seek) {
            hover_seek = h;
            desenhar_quadro();
        }
        return true;
    }
    if (e->tipo == EV_SOLTOU) {
        if (arrasta_seek) {
            arrasta_seek = false;
            int ms = seek_ms_de_x(e->x);
            hover_seek = -1;
            buscar_ms(ms);
            desenhar_quadro();
            desenhar_controles();
        }
        if (arrasta_vol) {
            arrasta_vol = false;
            registrar("reproductor: volumen %d\n", volume);
        }
        return true;
    }
    if (e->tipo == EV_RODA) {
        int n = (int)e->cliques;
        if (dentro(&r_lista, e->x, e->y)) {
            lista_topo += n * RODA_LINHAS;
            desenhar_lista();
        } else {
            mudar_volume(n > 0 ? -5 : 5);
        }
        return true;
    }
    if (e->tipo == EV_TECLA) {
        aviso[0] = 0;
        t_mover = tempo_ms();
        int pos = posicao_ms();
        switch (e->tecla) {
        case ' ': case 16: mudar_pausa(); return true;           /* Ctrl+P */
        case 19: parar(); return true;                            /* Ctrl+S */
        case 2: anterior(); return true;                          /* Ctrl+B */
        case 6: siguiente(); return true;                         /* Ctrl+F */
        case 12: mostrar_lista(!lista_vis); return true;          /* Ctrl+L */
        case 0x111: mudar_mudo(); return true;                    /* F8 */
        case 0x112: mudar_volume(-5); return true;                /* F9 */
        case 0x113: mudar_volume(5); return true;                 /* F10 */
        case TECLA_ESQUERDA: buscar_ms(pos - SALTO_MS); return true;
        case TECLA_DIREITA: buscar_ms(pos + SALTO_MS); return true;
        case TECLA_PGUP: buscar_ms(pos - 1000); return true;
        case TECLA_PGDN: buscar_ms(pos + 1000); return true;
        case TECLA_HOME: buscar_ms(0); return true;
        case TECLA_END: buscar_ms(duracao_ms()); return true;
        case TECLA_CIMA:
            if (lista_vis && lista_sel > 0) { lista_sel--; if (lista_sel < lista_topo) lista_topo = lista_sel; desenhar_lista(); }
            else if (!lista_vis) mudar_volume(5);
            return true;
        case TECLA_BAIXO:
            if (lista_vis && lista_sel < lista_n - 1) {
                lista_sel++;
                if (lista_sel >= lista_topo + lista_linhas()) lista_topo = lista_sel - lista_linhas() + 1;
                desenhar_lista();
            } else if (!lista_vis) mudar_volume(-5);
            return true;
        case '\n': if (lista_vis && lista_sel >= 0) passar_a(lista_sel); return true;
        case TECLA_DELETE: if (lista_vis) lista_quitar(); return true;
        case ',': passo_quadro(-1); return true;
        case '.': passo_quadro(1); return true;
        case 'l': case 'L': alternar_bucle(); return true;
        case 'a': case 'A': alternar_aleatorio(); return true;
        case 'm': case 'M': mudar_mudo(); return true;
        case '+': case '=': mudar_volume(5); return true;
        case '-': mudar_volume(-5); return true;
        case 'i': case 'I': mostrar_estatisticas(); return true;
        case 15: pedir_arquivo(); return true;   /* Ctrl+O */
        default: return true;
        }
    }
    if (e->tipo == EV_CLIQUE) return tratar_clique(e);
    return true;
}

/* ------------------------------------------------------------ modos de prueba */

/* -t: decodifica todo y dice las sumas de cada cuadro. */
static int modo_testar(const char *caminho)
{
    int erro = 0;
    video_t *v = video_abrir(caminho, &erro);
    if (!v) {
        imprimir_em(ERROS, "reproductor: %s: %s\n", caminho, video_erro_texto(erro));
        return 1;
    }
    const video_info_t *i = video_info(v);
    imprimir("video: %s %dx%d %d fps %d cuadros %u ms audio=%s\n", i->nome_formato, i->larg, i->alt, i->fps,
             i->quadros, i->duracao_ms, i->tem_audio ? "si" : "no");
    imagem_t img = { 0 };
    unsigned t0 = tempo_ms(), ms_dec = 0;
    int n = 0;
    for (;;) {
        unsigned td = tempo_ms();
        int rq = video_quadro(v, &img);
        ms_dec += tempo_ms() - td;
        if (rq != 0) break;
        uint32_t sr = 0, sg = 0, sb = 0;
        for (int p = 0; p < img.larg * img.alt; p++) {
            sr += (img.px[p] >> 16) & 255;
            sg += (img.px[p] >> 8) & 255;
            sb += img.px[p] & 255;
        }
        imprimir("cuadro %d %dx%d %u %u %u\n", n, img.larg, img.alt, sr, sg, sb);
        n++;
    }
    unsigned ms = tempo_ms() - t0;
    /* Los cuadros por segundo con dos decimales, sin números de 64 bits. */
    unsigned c100 = 0;
    if (ms) c100 = (unsigned)n * 1000 / ms * 100 + (unsigned)n * 1000 % ms * 100 / ms;
    imprimir("%d cuadros en %u ms (%u,%02u cuadros por segundo)\n", n, ms, c100 / 100, c100 % 100);
    /* Solo decodificar y convertir el color (sin la suma de arriba): lo que mide el rendimiento (D276). */
    unsigned d100 = 0;
    if (ms_dec) d100 = (unsigned)n * 1000 / ms_dec * 100 + (unsigned)n * 1000 % ms_dec * 100 / ms_dec;
    imprimir("decodificando %u ms (%u,%02u cuadros por segundo)\n", ms_dec, d100 / 100, d100 % 100);
    if (img.px) imagem_liberar(&img);
    video_fechar(v);
    return 0;
}

/* --volcar: escribe los cuadros decodificados en crudo (RGB24). */
#define VOLCA_LINHA (64 * 1024)   /* bytes por escrito: con 4 KB el pendrive
                                   * recibía un comando USB por cada uno y el
                                   * volcado tardaba más de 15 s */

static int modo_volcar(const char *caminho, const char *saida, int desde, int quantos)
{
    int erro = 0;
    video_t *v = video_abrir(caminho, &erro);
    if (!v) {
        imprimir_em(ERROS, "reproductor: %s: %s\n", caminho, video_erro_texto(erro));
        return 1;
    }
    const video_info_t *i = video_info(v);
    int fd = abrir(saida, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) {
        imprimir_em(ERROS, "reproductor: %s: %s\n", saida, nome_erro(fd));
        video_fechar(v);
        return 1;
    }
    char cab[64];
    int cn = formatar_texto(cab, sizeof cab, "SAGUIV1 %d %d\n", i->larg, i->alt);
    if (escrever_tudo(fd, cab, (unsigned)cn) != cn) {
        imprimir_em(ERROS, "reproductor: %s: no se pudo escribir la cabecera\n", saida);
        fechar(fd);
        video_fechar(v);
        return 1;
    }
    static uint8_t linha[VOLCA_LINHA];
    imagem_t img = { 0 };
    int n = 0, feitos = 0, falhou = 0;
    while (!falhou && video_quadro(v, &img) == 0) {
        if (n >= desde && (quantos < 0 || feitos < quantos)) {
            int x = 0;
            for (int p = 0; p < img.larg * img.alt; p++) {
                linha[x++] = (img.px[p] >> 16) & 255;
                linha[x++] = (img.px[p] >> 8) & 255;
                linha[x++] = img.px[p] & 255;
                if (x >= (int)sizeof linha - 3) {
                    /* escrever_tudo: un escribir puede escribir menos bytes de
                     * los pedidos, y un volcado a medias no vale. */
                    if (escrever_tudo(fd, linha, (unsigned)x) != x) { falhou = 1; break; }
                    x = 0;
                }
            }
            if (falhou) break;
            if (x && escrever_tudo(fd, linha, (unsigned)x) != x) falhou = 1;
            if (falhou) break;
            feitos++;
        }
        n++;
    }
    if (falhou) {
        imprimir_em(ERROS, "reproductor: %s: no se pudo escribir el volcado\n", saida);
        n = -1;
    }
    fechar(fd);
    imprimir("volcados %d cuadros de %d (%dx%d) en %s\n", feitos, n, i->larg, i->alt, saida);
    if (img.px) imagem_liberar(&img);
    video_fechar(v);
    return 0;
}


/* ------------------------------------------------------------ programa */

int principal(int argc, char **argv)
{
    int i = 1;
    if (i < argc && (compara_texto(argv[i], "-t") == 0 || compara_texto(argv[i], "--testar") == 0)) {
        if (i + 1 >= argc) { imprimir_em(ERROS, "uso: reproductor -t archivo\n"); return 2; }
        return modo_testar(argv[i + 1]);
    }
    if (i < argc && compara_texto(argv[i], "--volcar") == 0) {
        if (argc < i + 3) { imprimir_em(ERROS, "uso: reproductor --volcar archivo salida [desde [cuadros]]\n"); return 2; }
        int desde = i + 3 < argc ? texto_para_int(argv[i + 3]) : 0;
        int quantos = i + 4 < argc ? texto_para_int(argv[i + 4]) : -1;
        return modo_volcar(argv[i + 1], argv[i + 2], desde, quantos);
    }

    espec_iniciar();
    semente ^= tempo_ms() | 1;
    for (; i < argc; i++) lista_anadir_lo_que_sea(argv[i]);
    if (lista_n > 1) lista_vis = true;
    if (lista_n > 0) {
        for (int k = 0; k < lista_n && med == MED_NENHUM; k++) abrir_pista(k);
        if (med == MED_NENHUM) {
            imprimir_em(ERROS, "reproductor: %s: %s\n", lista_c[0], ultimo_erro);
            return 1;
        }
    }

    /* Que quepa en la pantalla (a 800x600 no cabe 800x600 con el marco y la barra de arriba). */
    int larg0 = LARG_INICIO, alt0 = ALT_INICIO;
    {
        info_telas_t t;
        memset(&t, 0, sizeof t);
        if (telas_sis(TELAS_OP_INFO, (unsigned)&t, sizeof t) >= 0 && t.qtd > 0) {
            int p = t.principal < t.qtd ? (int)t.principal : 0;
            if (larg0 > (int)t.t[p].largura - 4) larg0 = (int)t.t[p].largura - 4;
            if (alt0 > (int)t.t[p].altura - 52) alt0 = (int)t.t[p].altura - 52;
            if (larg0 < LARG_MIN) larg0 = LARG_MIN;
            if (alt0 < ALT_MIN) alt0 = ALT_MIN;
        }
    }
    j = jan_abrir("Reproductor de vídeo", larg0, alt0);
    if (!j) {
        fechar_medio();
        return 1;
    }
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, LARG_MIN, ALT_MIN);
    t_inicio = tempo_ms();
    atualizar_titulo();
    desenhar_tudo();
    if (med == MED_VIDEO) {
        const video_info_t *info = video_info(vid);
        registrar("reproductor: listo (%s, %dx%d, %d fps, %d cuadros)\n", caminho_video, info->larg, info->alt, fps, quadros_total);
    } else if (med == MED_AUDIO) {
        registrar("reproductor: listo (%s, audio, %u Hz)\n", caminho_video, ainf->taxa);
    } else {
        registrar("reproductor: listo (sin archivo)\n");
    }

    evento_t e;
    bool continuar = true;
    unsigned t_esp = 0, t_pan = 0;
    bool sin_molestar = false;
    while (continuar) {
        /* Con vídeo en marcha, ni salvapantallas ni bloqueo (D422); el kernel lo suelta si el programa acaba. */
        if (sin_molestar != (tocando && med == MED_VIDEO)) { sin_molestar = !sin_molestar; no_molestar(sin_molestar); }
        if (tocando && med == MED_VIDEO) {
            unsigned t0 = tempo_ms();
            unsigned g0 = geracao;
            int r = video_quadro(vid, &img);
            ms_decodificar += tempo_ms() - t0;
            if (r == 0) {
                tem_quadro = true;
                quadro_atual = video_quadro_atual(vid);
                quadros_vistos++;
                escrever_audio();
                unsigned alvo = (unsigned)(quadro_atual < 0 ? 0 : quadro_atual) * periodo_ms();
                /* Se espera a la hora del cuadro. */
                while (tocando && g0 == geracao && relogio() < alvo) {
                    if (esperar_evento(j->id, &e, 5) == 1) {
                        continuar = tratar(&e);
                        if (!continuar) break;
                    }
                }
                if (tocando && continuar && g0 == geracao) {
                    /* El cuadro se enseña nada más decodificarlo: si se
                     * esperaba a su hora y luego había que ir deprisa, el
                     * primer cuadro podía no verse nunca. */
                    desenhar_quadro();
                    desenhar_controles();
                    /* Si el reloj va atrasado, se salta un cuadro: mejor
                     * saltarlos que ir a cámara lenta (el sonido manda).
                     * Sólo se salta uno por vuelta: si la decodificación va
                     * más despacio que el vídeo, saltando sin límite se
                     * acababa el vídeo entero sin haber enseñado ni el
                     * primer cuadro. Sin tarjeta de sonido no hay reloj
                     * maestro, así que se enseñan todos los cuadros aunque
                     * el vídeo vaya un poco lento. */
                    if (audio_fd >= 0 && relogio() > alvo + periodo_ms()) {
                        unsigned t1 = tempo_ms();
                        int r2 = video_quadro(vid, &img);
                        ms_decodificar += tempo_ms() - t1;
                        if (r2 == 0) {
                            quadros_saltados++;
                            escrever_audio();
                            quadro_atual = video_quadro_atual(vid);
                        }
                    }
                }
            } else if (r == VIDEO_ERRO_FIM) {
                if (g0 == geracao) terminou();
            } else {
                tocando = false;
                formatar_texto(aviso, sizeof aviso, "Error: %s", video_erro_texto(r));
                registrar("reproductor: error %d\n", r);
                desenhar_controles();
            }
        } else if (tocando && med == MED_AUDIO) {
            passo_audio();
            unsigned agora = tempo_ms();
            if (agora - t_esp >= 40) {
                t_esp = agora;
                if (!capa.px) espec_calcular();
                if (!capa.px || agora - t_pan >= 250) { desenhar_quadro(); }
            }
            if (agora - t_pan >= 250) { t_pan = agora; desenhar_controles(); }
            int d = duracao_ms();
            bool acabou = audio_fd >= 0 ? aud_fim_dec && audio_pendente(audio_fd) <= 0
                                        : d > 0 && audio_posicao_agora() >= d;
            if (acabou) terminou();
        }
        if (continuar && tela_cheia && ctrl_visivel && !arrasta_seek && !arrasta_vol && tempo_ms() - t_mover > OCULTAR_MS)
            ocultar_ou_mostrar_controles(false);
        if (continuar && esperar_evento(j->id, &e, tocando && med == MED_AUDIO ? 15 : 20) == 1) continuar = tratar(&e);
    }
    registrar("reproductor: cerrado\n");
    if (audio_fd >= 0) audio_fechar(audio_fd, 1);
    audio_fd = -1;
    fechar_medio();
    jan_fechar(j);
    return 0;
}
