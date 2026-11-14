// SPDX-License-Identifier: GPL-2.0-or-later
/* sintonia_unidad.c - prueba de unidad en el PC de la lógica de Sintonía (D225, h225).
 *
 *   gcc -O2 -Wall -Wextra -DSINTONIA_ANFITRION -I userland/programas -o sintonia_unidad tests/sintonia_unidad.c
 *
 * Comprueba: FFT y espectro, listas M3U y PLS (leer, escribir y volver a leer), metadatos (ID3v2.2/2.3/2.4,
 * ID3v1, Vorbis de FLAC, WAV, duración Xing y por tasa), filtro ICY, corte de MP3 en cuadros, tema y
 * orden aleatorio. Escribe "todo bien" y sale con 0 si pasa todo. */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "sintonia_logica.h"

static int fallos, total;
#define COMPROBAR(c, ...) do { total++; if (!(c)) { fallos++; printf("FALLO %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---------------------------------------------------------------- FFT */

static void prueba_fft(void)
{
    int32_t re[SL_FFT_N], im[SL_FFT_N];
    /* senoide exacta en el compartimento 16 (periodo de 16 muestras) */
    for (int n = 0; n < SL_FFT_N; n++) { re[n] = (int32_t)lround(16000.0 * sin(2 * M_PI * 16 * n / SL_FFT_N)); im[n] = 0; }
    sl_fft(re, im);
    int mejor = 0;
    int32_t mm = 0;
    for (int k = 1; k < SL_FFT_N / 2; k++) {
        int32_t m = abs(re[k]) + abs(im[k]);
        if (m > mm) { mm = m; mejor = k; }
    }
    COMPROBAR(mejor == 16, "el pico de la senoide cae en el compartimento %d (esperado 16)", mejor);
    /* amplitud 16000: la FFT/256 de una senoide da A/2 = 8000 en su compartimento */
    COMPROBAR(mm > 7600 && mm < 8400, "magnitud del pico %d (esperado ~8000)", (int)mm);
    int32_t resto = 0;
    for (int k = 1; k < SL_FFT_N / 2; k++) if (k != 16 && abs(re[k]) + abs(im[k]) > resto) resto = abs(re[k]) + abs(im[k]);
    COMPROBAR(resto < 80, "el resto del espectro queda bajo (%d)", (int)resto);

    /* el seno de la tabla frente a libm */
    int peor = 0;
    for (int i = 0; i < 256; i++) {
        int d = abs(sl_sen(i) - (int)lround(32767.0 * sin(2 * M_PI * i / 256)));
        if (d > peor) peor = d;
    }
    COMPROBAR(peor <= 2, "tabla de seno: error máximo %d", peor);

    /* espectro: 44100 Hz, 1 kHz -> compartimento ~5,8; con 24 barras tiene que ser una de las bajas */
    int16_t m[SL_FFT_N];
    uint8_t niv[SL_BARRAS_MAX];
    for (int n = 0; n < SL_FFT_N; n++) m[n] = (int16_t)lround(30000.0 * sin(2 * M_PI * 1000.0 * n / 44100.0));
    sl_espectro(m, 24, niv);
    int bmax = 0;
    for (int i = 1; i < 24; i++) if (niv[i] > niv[bmax]) bmax = i;
    COMPROBAR(bmax <= 6 && niv[bmax] > 70, "1 kHz: barra %d nivel %d", bmax, niv[bmax]);
    COMPROBAR(niv[23] < 30, "1 kHz: la última barra casi vacía (%d)", niv[23]);
    /* silencio */
    memset(m, 0, sizeof m);
    sl_espectro(m, 24, niv);
    int sum = 0;
    for (int i = 0; i < 24; i++) sum += niv[i];
    COMPROBAR(sum == 0, "silencio: niveles a cero (%d)", sum);
    /* 10 kHz -> barras altas */
    for (int n = 0; n < SL_FFT_N; n++) m[n] = (int16_t)lround(30000.0 * sin(2 * M_PI * 10000.0 * n / 44100.0));
    sl_espectro(m, 24, niv);
    bmax = 0;
    for (int i = 1; i < 24; i++) if (niv[i] > niv[bmax]) bmax = i;
    COMPROBAR(bmax >= 14, "10 kHz: barra %d (esperada alta)", bmax);
    COMPROBAR(sl_log2x8(1) == 0 && sl_log2x8(8) == 24 && sl_log2x8(16) == 32 && sl_log2x8(12) == 28, "log2x8");
}

/* ---------------------------------------------------------------- listas */

typedef struct { char b[8192]; unsigned n; } buf_t;
static int emitir(void *c, const char *s, unsigned n)
{
    buf_t *b = c;
    if (b->n + n >= sizeof b->b) return -1;
    memcpy(b->b + b->n, s, n);
    b->n += n;
    b->b[b->n] = 0;
    return 0;
}

static void prueba_listas(void)
{
    sl_pista_t p[8];
    const char *m3u = "\xEF\xBB\xBF#EXTM3U\r\n#EXTINF:215,Los Monos - Banana\r\nmusica\\banana.mp3\r\n\r\n"
                      "# comentario\r\n/abs/otra.flac\r\n#EXTINF:-1,Radio Selva\r\nhttp://radio.example/stream\r\n";
    int n = sl_lista_leer(m3u, (unsigned)strlen(m3u), "/home/ana", p, 8);
    COMPROBAR(n == 3, "M3U: 3 pistas (%d)", n);
    COMPROBAR(!strcmp(p[0].ruta, "/home/ana/musica/banana.mp3"), "M3U: ruta relativa con \\ -> '%s'", p[0].ruta);
    COMPROBAR(p[0].dur_s == 215 && !strcmp(p[0].artista, "Los Monos") && !strcmp(p[0].titulo, "Banana"), "M3U: EXTINF '%s' '%s' %d", p[0].artista, p[0].titulo, p[0].dur_s);
    COMPROBAR(!strcmp(p[1].ruta, "/abs/otra.flac") && p[1].dur_s == -1 && !p[1].titulo[0], "M3U: absoluta sin EXTINF");
    COMPROBAR(!strcmp(p[2].ruta, "http://radio.example/stream") && p[2].dur_s == -1 && !strcmp(p[2].titulo, "Radio Selva"), "M3U: URL de radio");
    COMPROBAR(sl_es_url(p[2].ruta) && !sl_es_url(p[0].ruta), "es_url");

    const char *pls = "[playlist]\nNumberOfEntries=3\nFile2=b.ogg\nTitle2=Segunda\nLength2=61\nFile1=http://x/y.mp3\nTitle1=Primera - Emisora\n"
                      "Length1=-1\nFile4=\nFile3=C:\\musica\\c.wav\nVersion=2\n";
    n = sl_lista_leer(pls, (unsigned)strlen(pls), "/mnt", p, 8);
    COMPROBAR(n == 3, "PLS: 3 pistas (%d)", n);
    COMPROBAR(!strcmp(p[0].ruta, "http://x/y.mp3") && !strcmp(p[0].artista, "Primera") && !strcmp(p[0].titulo, "Emisora"), "PLS: orden por número, 1");
    COMPROBAR(!strcmp(p[1].ruta, "/mnt/b.ogg") && p[1].dur_s == 61 && !strcmp(p[1].titulo, "Segunda"), "PLS: 2 '%s' %d", p[1].ruta, p[1].dur_s);
    COMPROBAR(!strcmp(p[2].ruta, "/mnt/C:/musica/c.wav"), "PLS: 3 '%s'", p[2].ruta);

    /* escribir y volver a leer, en los dos formatos */
    for (int pl = 0; pl < 2; pl++) {
        sl_pista_t a[3];
        for (int i = 0; i < 3; i++) sl_pista_limpiar(&a[i]);
        strcpy(a[0].ruta, "/a/uno.mp3"); strcpy(a[0].titulo, "Uno"); strcpy(a[0].artista, "Ana"); a[0].dur_s = 100;
        strcpy(a[1].ruta, "/a/dos.wav"); a[1].dur_s = 7;
        strcpy(a[2].ruta, "https://r.example/s"); strcpy(a[2].titulo, "Radio"); a[2].dur_s = -1;
        buf_t b = { { 0 }, 0 };
        COMPROBAR(sl_lista_escribir(a, 3, pl, emitir, &b) == 0, "escribir %s", pl ? "PLS" : "M3U");
        COMPROBAR(pl ? !strncmp(b.b, "[playlist]", 10) : !strncmp(b.b, "#EXTM3U", 7), "cabecera");
        sl_pista_t c[8];
        int k = sl_lista_leer(b.b, b.n, "/", c, 8);
        COMPROBAR(k == 3, "releer %s: %d pistas", pl ? "PLS" : "M3U", k);
        if (k == 3) {
            COMPROBAR(!strcmp(c[0].ruta, "/a/uno.mp3") && !strcmp(c[0].artista, "Ana") && !strcmp(c[0].titulo, "Uno") && c[0].dur_s == 100, "releer 1");
            COMPROBAR(!strcmp(c[1].ruta, "/a/dos.wav") && c[1].dur_s == 7 && !strcmp(c[1].titulo, "dos.wav"), "releer 2 (%s)", c[1].titulo);
            COMPROBAR(!strcmp(c[2].ruta, "https://r.example/s") && c[2].dur_s == -1, "releer 3");
        }
    }
    /* máximo respetado */
    sl_pista_t pequeno[2];
    n = sl_lista_leer("a.mp3\nb.mp3\nc.mp3\n", 18, "/", pequeno, 2);
    COMPROBAR(n == 2, "M3U: respeta max (%d)", n);
    COMPROBAR(sl_tipo_lista("/x/lista.M3U") == 1 && sl_tipo_lista("a.m3u8") == 1 && sl_tipo_lista("a.pls") == 2 && sl_tipo_lista("a.mp3") == 0 && sl_tipo_lista("m3u") == 0, "tipo_lista");
    char t[16];
    sl_tiempo(t, sizeof t, 65); COMPROBAR(!strcmp(t, "1:05"), "tiempo 65 -> %s", t);
    sl_tiempo(t, sizeof t, 3725); COMPROBAR(!strcmp(t, "1:02:05"), "tiempo 3725 -> %s", t);
    sl_tiempo(t, sizeof t, -1); COMPROBAR(!strcmp(t, "--:--"), "tiempo -1");
    COMPROBAR(sl_contiene("Los Monos - Banana", "monos b") == 0 && sl_contiene("Los Monos - Banana", "MONOS") && sl_contiene("x", ""), "buscar");
}

/* ---------------------------------------------------------------- metadatos */

static unsigned marco(uint8_t *o, const char *id, int ver, const char *txt, int enc)
{
    unsigned n = (unsigned)strlen(txt), l = 1 + n;
    uint8_t cuerpo[200];
    cuerpo[0] = (uint8_t)enc;
    if (enc == 1) {   /* UTF-16 con BOM */
        unsigned k = 1;
        cuerpo[k++] = 0xFF; cuerpo[k++] = 0xFE;
        for (unsigned i = 0; i < n; i++) { cuerpo[k++] = (uint8_t)txt[i]; cuerpo[k++] = 0; }
        l = k;
    } else memcpy(cuerpo + 1, txt, n);
    unsigned p = 0;
    if (ver == 2) { memcpy(o, id, 3); o[3] = l >> 16; o[4] = l >> 8; o[5] = l; p = 6; }
    else {
        memcpy(o, id, 4);
        if (ver == 4) { o[4] = (l >> 21) & 127; o[5] = (l >> 14) & 127; o[6] = (l >> 7) & 127; o[7] = l & 127; }
        else { o[4] = l >> 24; o[5] = l >> 16; o[6] = l >> 8; o[7] = l; }
        o[8] = o[9] = 0;
        p = 10;
    }
    memcpy(o + p, cuerpo, l);
    return p + l;
}

/* Cuadro MPEG-1 Layer III de 128 kbps a 44100 Hz (417 bytes), estéreo, con Xing opcional. */
static unsigned cuadro_mp3(uint8_t *o, int xing_cuadros)
{
    memset(o, 0, 417);
    o[0] = 0xFF; o[1] = 0xFB; o[2] = 0x90; o[3] = 0x00;
    if (xing_cuadros) {
        memcpy(o + 4 + 32, "Xing", 4);
        o[4 + 32 + 7] = 1;
        o[4 + 32 + 8] = (uint8_t)(xing_cuadros >> 24); o[4 + 32 + 9] = (uint8_t)(xing_cuadros >> 16);
        o[4 + 32 + 10] = (uint8_t)(xing_cuadros >> 8); o[4 + 32 + 11] = (uint8_t)xing_cuadros;
    }
    return 417;
}

static unsigned etiqueta_v2(uint8_t *o, int ver, int enc, const char *tit, const char *art, const char *alb, const char *tlen)
{
    uint8_t cuerpo[800];
    unsigned n = 0;
    n += marco(cuerpo + n, ver == 2 ? "TT2" : "TIT2", ver, tit, enc);
    n += marco(cuerpo + n, ver == 2 ? "TP1" : "TPE1", ver, art, enc);
    n += marco(cuerpo + n, ver == 2 ? "TAL" : "TALB", ver, alb, enc);
    if (tlen) n += marco(cuerpo + n, ver == 2 ? "TLE" : "TLEN", ver, tlen, 0);
    memset(cuerpo + n, 0, 20); /* relleno */
    n += 20;
    memcpy(o, "ID3", 3);
    o[3] = (uint8_t)ver; o[4] = 0; o[5] = 0;
    o[6] = (n >> 21) & 127; o[7] = (n >> 14) & 127; o[8] = (n >> 7) & 127; o[9] = n & 127;
    memcpy(o + 10, cuerpo, n);
    return 10 + n;
}

static void prueba_meta(void)
{
    static uint8_t f[4000];
    sl_meta_t m;
    for (int ver = 2; ver <= 4; ver++) {
        int enc = ver == 4 ? 3 : ver == 3 ? 1 : 0;
        unsigned n = etiqueta_v2(f, ver, enc, ver == 4 ? "Canci\xC3\xB3n" : "Canci\xF3n", "Los Monos", ver == 4 ? "\xC3\x81lbum" : "\xC1lbum", NULL);
        if (ver == 4) { /* el artista y el álbum en UTF-8 también */ }
        n += cuadro_mp3(f + n, 0);
        n += cuadro_mp3(f + n, 0);
        bool ok = sl_meta_leer(f, n, NULL, 0, NULL, n, &m);
        COMPROBAR(ok && m.formato == 1, "ID3v2.%d: reconocido", ver);
        COMPROBAR(!strcmp(m.titulo, "Canci\xF3n"), "ID3v2.%d: título '%s'", ver, m.titulo);
        COMPROBAR(!strcmp(m.artista, "Los Monos"), "ID3v2.%d: artista '%s'", ver, m.artista);
        COMPROBAR(!strcmp(m.album, "\xC1lbum"), "ID3v2.%d: álbum '%s'", ver, m.album);
        /* 2 cuadros de 417 bytes a 128 kbps = 834 bytes: 0 s; sin Xing se calcula por la tasa */
        COMPROBAR(m.dur_s == 0, "ID3v2.%d: duración por tasa %d", ver, m.dur_s);
    }
    /* TLEN manda; el tamaño total del archivo cuenta en la tasa constante: 128 kbps = 16000 B/s */
    unsigned n = etiqueta_v2(f, 3, 0, "T", "A", "B", "183000");
    n += cuadro_mp3(f + n, 0);
    sl_meta_t m2;
    sl_meta_leer(f, n, NULL, 0, NULL, 16000 * 200 + n, &m2);
    COMPROBAR(m2.dur_s == 183, "TLEN 183000 ms -> %d s", m2.dur_s);
    n = etiqueta_v2(f, 3, 0, "T", "A", "B", NULL);
    uint32_t off = n;
    n += cuadro_mp3(f + n, 0);
    sl_meta_leer(f, n, NULL, 0, NULL, off + 16000 * 200, &m2);
    COMPROBAR(m2.dur_s == 200, "tasa constante: %d s (esperado 200)", m2.dur_s);
    /* Xing: 1000 cuadros de 1152 muestras a 44100 Hz = 26 s */
    n = etiqueta_v2(f, 3, 0, "T", "A", "B", NULL);
    n += cuadro_mp3(f + n, 1000);
    sl_meta_leer(f, n, NULL, 0, NULL, n + 5000000, &m2);
    COMPROBAR(m2.dur_s == 26, "Xing 1000 cuadros: %d s (esperado 26)", m2.dur_s);
    COMPROBAR(sl_id3v2_tam(f, n) == off, "id3v2_tam");
    /* etiqueta grande (portada): el cuadro cae fuera de cab; se pasa en aud */
    uint8_t aud[417];
    cuadro_mp3(aud, 1000);
    sl_meta_leer(f, 200, aud, 417, NULL, 100000, &m2);
    COMPROBAR(m2.dur_s == 26 && !strcmp(m2.titulo, "T"), "cuadro aparte (aud): %d s", m2.dur_s);

    /* ID3v1 sin v2 */
    static uint8_t g[2000];
    unsigned k = cuadro_mp3(g, 0);
    k += cuadro_mp3(g + k, 0);
    uint8_t cola[128];
    memset(cola, ' ', sizeof cola);
    memcpy(cola, "TAG", 3);
    memcpy(cola + 3, "Titulo v1", 9);
    memcpy(cola + 33, "Artista v1", 10);
    memcpy(cola + 63, "Album v1", 8);
    sl_meta_leer(g, k, NULL, 0, cola, k + 128, &m2);
    COMPROBAR(!strcmp(m2.titulo, "Titulo v1") && !strcmp(m2.artista, "Artista v1") && !strcmp(m2.album, "Album v1"), "ID3v1 '%s' '%s' '%s'", m2.titulo, m2.artista, m2.album);

    /* FLAC: STREAMINFO (44100 Hz, 44100*125 muestras = 125 s) + Vorbis */
    static uint8_t fl[1000];
    unsigned p = 0;
    memcpy(fl, "fLaC", 4); p = 4;
    fl[p] = 0; fl[p + 1] = 0; fl[p + 2] = 0; fl[p + 3] = 34; p += 4;
    memset(fl + p, 0, 34);
    uint32_t taxa = 44100, muestras = 44100u * 125u;
    fl[p + 10] = (uint8_t)(taxa >> 12); fl[p + 11] = (uint8_t)(taxa >> 4); fl[p + 12] = (uint8_t)((taxa & 15) << 4 | 1 << 1);
    fl[p + 13] = 0; fl[p + 14] = (uint8_t)(muestras >> 24); fl[p + 15] = (uint8_t)(muestras >> 16); fl[p + 16] = (uint8_t)(muestras >> 8); fl[p + 17] = (uint8_t)muestras;
    p += 34;
    uint8_t vc[300];
    unsigned v = 0;
    const char *vendor = "libFLAC";
    vc[v++] = (uint8_t)strlen(vendor); vc[v++] = 0; vc[v++] = 0; vc[v++] = 0;
    memcpy(vc + v, vendor, strlen(vendor)); v += (unsigned)strlen(vendor);
    const char *cm[] = { "TITLE=Ca\xC3\xB1" "\xC3\xB3n", "artist=Ana", "ALBUM=\xE2\x82\xAC" "uro" };
    vc[v++] = 3; vc[v++] = 0; vc[v++] = 0; vc[v++] = 0;
    for (int i = 0; i < 3; i++) { unsigned l = (unsigned)strlen(cm[i]); vc[v++] = (uint8_t)l; vc[v++] = 0; vc[v++] = 0; vc[v++] = 0; memcpy(vc + v, cm[i], l); v += l; }
    fl[p] = 0x80 | 4; fl[p + 1] = 0; fl[p + 2] = (uint8_t)(v >> 8); fl[p + 3] = (uint8_t)v; p += 4;
    memcpy(fl + p, vc, v); p += v;
    sl_meta_leer(fl, p, NULL, 0, NULL, 5000000, &m2);
    COMPROBAR(m2.formato == 2 && m2.dur_s == 125, "FLAC: duración %d", m2.dur_s);
    COMPROBAR(!strcmp(m2.titulo, "Ca\xF1\xF3n") && !strcmp(m2.artista, "Ana") && !strcmp(m2.album, "?uro"), "FLAC: '%s' '%s' '%s'", m2.titulo, m2.artista, m2.album);

    /* WAV: 48000 Hz estéreo 16 bits = 192000 B/s, 960000 bytes de datos = 5 s */
    uint8_t w[64];
    memset(w, 0, sizeof w);
    memcpy(w, "RIFF", 4); memcpy(w + 8, "WAVE", 4); memcpy(w + 12, "fmt ", 4);
    w[16] = 16; w[20] = 1; w[22] = 2;
    uint32_t sr = 48000, br = 192000, dl = 960000;
    memcpy(w + 24, &sr, 4); memcpy(w + 28, &br, 4); w[32] = 4; w[34] = 16;
    memcpy(w + 36, "data", 4); memcpy(w + 40, &dl, 4);
    sl_meta_leer(w, 44, NULL, 0, NULL, 1000000, &m2);
    COMPROBAR(m2.formato == 3 && m2.dur_s == 5, "WAV: %d s", m2.dur_s);
    uint8_t basura[64];
    memset(basura, 0x55, sizeof basura);
    COMPROBAR(!sl_meta_leer(basura, 64, NULL, 0, NULL, 64, &m2), "basura no se reconoce");
}

/* ---------------------------------------------------------------- ICY y cortes de MP3 */

static void prueba_icy(void)
{
    /* metaint 16: 16 bytes de audio, bloque de 2*16 con el título, 16 bytes, bloque vacío, 5 bytes */
    uint8_t flujo[256];
    unsigned n = 0;
    for (int i = 0; i < 16; i++) flujo[n++] = (uint8_t)i;
    char meta[32];
    memset(meta, 0, sizeof meta);
    strcpy(meta, "StreamTitle='A - B';");
    flujo[n++] = 2;
    memcpy(flujo + n, meta, 32); n += 32;
    for (int i = 0; i < 16; i++) flujo[n++] = (uint8_t)(100 + i);
    flujo[n++] = 0;
    for (int i = 0; i < 5; i++) flujo[n++] = (uint8_t)(200 + i);
    /* se entrega en trozos de tamaños raros para probar los cortes dentro de los bloques */
    for (unsigned paso = 1; paso <= 40; paso += 3) {
        sl_icy_t s;
        sl_icy_iniciar(&s, 16);
        uint8_t sal[256];
        unsigned o = 0;
        for (unsigned i = 0; i < n; i += paso) o += sl_icy_filtrar(&s, flujo + i, n - i < paso ? n - i : paso, sal + o);
        COMPROBAR(o == 37, "ICY paso %u: %u bytes de audio (esperado 37)", paso, o);
        bool bien = o == 37;
        for (unsigned i = 0; bien && i < 16; i++) bien = sal[i] == i;
        for (unsigned i = 0; bien && i < 16; i++) bien = sal[16 + i] == 100 + i;
        for (unsigned i = 0; bien && i < 5; i++) bien = sal[32 + i] == 200 + i;
        COMPROBAR(bien, "ICY paso %u: el audio sale intacto", paso);
        COMPROBAR(s.nuevo && !strcmp(s.titulo, "A - B"), "ICY paso %u: título '%s'", paso, s.titulo);
    }
    sl_icy_t s;
    sl_icy_iniciar(&s, 0);
    uint8_t sal[16];
    COMPROBAR(sl_icy_filtrar(&s, flujo, 16, sal) == 16 && !memcmp(sal, flujo, 16), "ICY sin metaint: tal cual");
    /* comilla dentro del título y UTF-8 */
    sl_icy_iniciar(&s, 4);
    uint8_t f2[80];
    unsigned k = 0;
    memset(f2, 1, 4); k = 4;
    const char *t = "StreamTitle='Mu\xC3\xB1oz - Don't stop';StreamUrl='x';";
    unsigned l = (unsigned)strlen(t), bl = (l + 15) / 16;
    f2[k++] = (uint8_t)bl;
    memset(f2 + k, 0, bl * 16); memcpy(f2 + k, t, l); k += bl * 16;
    uint8_t s2[80];
    sl_icy_filtrar(&s, f2, k, s2);
    COMPROBAR(!strcmp(s.titulo, "Mu\xF1oz - Don't stop"), "ICY: título con comilla '%s'", s.titulo);
}

static void prueba_corte(void)
{
    /* 10 cuadros de 417 bytes precedidos de 100 bytes de basura */
    static uint8_t b[100 + 417 * 10 + 50];
    memset(b, 0x11, 100);
    for (int i = 0; i < 10; i++) cuadro_mp3(b + 100 + 417 * i, 0);
    memset(b + 100 + 417 * 10, 0x22, 50);
    unsigned ini = 0, fin = sl_mp3_cortar(b, sizeof b, 1000, &ini);
    COMPROBAR(ini == 100, "corte: empieza en %u (esperado 100)", ini);
    COMPROBAR(fin == 100 + 417 * 3, "corte: acaba en %u (esperado %u)", fin, 100 + 417 * 3);
    fin = sl_mp3_cortar(b, 100 + 417 * 2 + 10, 1000, &ini);
    COMPROBAR(fin == 0, "corte: con pocos datos pide más (%u)", fin);
    fin = sl_mp3_cortar(b, sizeof b, 100000, &ini);
    COMPROBAR(fin == 0 || fin == 100 + 417 * 10, "corte: alvo mayor que los datos -> %u", fin);
    int sr, kb, can, spf, ver;
    COMPROBAR(sl_mp3_cuadro(b + 100, &sr, &kb, &can, &spf, &ver) == 417 && sr == 44100 && kb == 128 && can == 2 && spf == 1152, "cabecera MP3");
}

/* ---------------------------------------------------------------- tema, aleatorio */

static void prueba_tema(void)
{
    sl_tema_t t;
    sl_tema_defecto(&t);
    const char *txt = "# mi tema\nfondo = #102030\nacento=0xFF8800\nbarras=ABCDEF\nvisualizador=ambos\nlista=no\neq=si\nbotones=arriba\n"
                      "color_raro=#12\nfondo=#12345\nvisualizador=otro\n";
    int ok = sl_tema_leer(&t, txt, (unsigned)strlen(txt));
    COMPROBAR(ok == 7, "tema: %d líneas entendidas (esperado 7)", ok);
    COMPROBAR(t.fondo == 0x102030 && t.acento == 0xFF8800 && t.barras == 0xABCDEF, "tema: colores %06X %06X %06X", t.fondo, t.acento, t.barras);
    COMPROBAR(t.visualizador == 2 && t.lista == 0 && t.eq == 1 && t.botones_arriba == 1, "tema: disposición");
    char k[24], v[48];
    COMPROBAR(sl_clave_valor("  Volumen =  70  ", k, sizeof k, v, sizeof v) && !strcmp(k, "volumen") && !strcmp(v, "70"), "clave_valor '%s' '%s'", k, v);
    COMPROBAR(!sl_clave_valor("# nada", k, sizeof k, v, sizeof v) && !sl_clave_valor("sin igual", k, sizeof k, v, sizeof v), "clave_valor: comentarios");
}

static void prueba_aleatorio(void)
{
    int o[64];
    uint32_t sem = 12345;
    int moves = 0;
    for (int vez = 0; vez < 20; vez++) {
        sl_orden_aleatorio(o, 64, &sem);
        int visto[64] = { 0 };
        for (int i = 0; i < 64; i++) { COMPROBAR(o[i] >= 0 && o[i] < 64 && !visto[o[i]], "aleatorio: permutación válida"); visto[o[i]] = 1; if (o[i] != i) moves++; }
    }
    COMPROBAR(moves > 20 * 50, "aleatorio: mezcla de verdad (%d)", moves);
}

int main(void)
{
    prueba_fft();
    prueba_listas();
    prueba_meta();
    prueba_icy();
    prueba_corte();
    prueba_tema();
    prueba_aleatorio();
    printf("%d comprobaciones, %d fallos\n", total, fallos);
    if (!fallos) printf("todo bien\n");
    return fallos ? 1 : 0;
}
