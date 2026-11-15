// SPDX-License-Identifier: GPL-2.0-or-later
/* captura.c - entrada de sonido de la libsagui (D292). Ver captura.h. */
#include "captura.h"

static int llamar(info_captura_t *i) { return info_sistema(INFO_CAPTURA, i, sizeof *i); }

int captura_estado(unsigned fuente, info_captura_t *i)
{
    memset(i, 0, sizeof *i);
    i->orden = CAP_ESTADO;
    i->fuente = fuente;
    return llamar(i);
}

int captura_abrir(unsigned fuente, unsigned taxa, unsigned canais)
{
    info_captura_t i;
    memset(&i, 0, sizeof i);
    i.orden = CAP_ABRIR;
    i.fuente = fuente;
    i.taxa = taxa;
    i.canais = canais;
    return llamar(&i);
}

int captura_ler(void *buf, unsigned n)
{
    info_captura_t i;
    memset(&i, 0, sizeof i);
    i.orden = CAP_LER;
    i.buf = buf;
    i.buf_tam = n;
    return llamar(&i);
}

int captura_ler_espera(void *buf, unsigned n, unsigned ms)
{
    for (;;) {
        int r = captura_ler(buf, n);
        if (r != 0 || ms == 0) return r;
        unsigned d = ms < 10 ? ms : 10;
        dormir(d);
        ms -= d;
    }
}

int captura_cerrar(void)
{
    info_captura_t i;
    memset(&i, 0, sizeof i);
    i.orden = CAP_FECHAR;
    return llamar(&i);
}

int captura_entrada(unsigned indice)
{
    info_captura_t i;
    memset(&i, 0, sizeof i);
    i.orden = CAP_ENTRADA;
    i.escribir = 1;
    i.entrada = indice;
    return llamar(&i);
}

int captura_ganancia(unsigned pct, bool mudo)
{
    info_captura_t i;
    memset(&i, 0, sizeof i);
    i.orden = CAP_GANANCIA;
    i.escribir = 1;
    i.ganancia = pct;
    i.mudo = mudo ? 1 : 0;
    return llamar(&i);
}

static void p32(uint8_t *d, unsigned v) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); d[2] = (uint8_t)(v >> 16); d[3] = (uint8_t)(v >> 24); }
static void p16(uint8_t *d, unsigned v) { d[0] = (uint8_t)v; d[1] = (uint8_t)(v >> 8); }

void captura_wav_cabecera(uint8_t h[44], unsigned taxa, unsigned canais, unsigned bytes)
{
    memcpy(h, "RIFF", 4);
    p32(h + 4, 36 + bytes);
    memcpy(h + 8, "WAVEfmt ", 8);
    p32(h + 16, 16);
    p16(h + 20, 1);                      /* PCM */
    p16(h + 22, canais);
    p32(h + 24, taxa);
    p32(h + 28, taxa * canais * 2);
    p16(h + 32, canais * 2);
    p16(h + 34, 16);
    memcpy(h + 36, "data", 4);
    p32(h + 40, bytes);
}

int captura_gravar_wav(const char *caminho, unsigned fuente, unsigned taxa, unsigned canais, unsigned ms,
                       void (*progreso)(unsigned ms, unsigned pico))
{
    int r = captura_abrir(fuente, taxa, canais);
    if (r < 0) return r;
    int fd = abrir(caminho, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { captura_cerrar(); return fd; }
    uint8_t cab[44];
    captura_wav_cabecera(cab, taxa, canais, 0);
    escrever(fd, cab, sizeof cab);
    unsigned total = taxa * canais * 2 / 1000 * ms, feito = 0;   /* bytes */
    static uint8_t buf[8192];
    unsigned parado = 0;
    r = 0;
    while (feito < total) {
        unsigned q = total - feito < sizeof buf ? total - feito : (unsigned)sizeof buf;
        int n = captura_ler(buf, q);
        if (n < 0) { r = n; break; }
        if (n == 0) {
            if (++parado > 500) { r = ERRO_TEMPO; break; }       /* ~5 s sin un solo cuadro */
            dormir(10);
            continue;
        }
        parado = 0;
        if (escrever(fd, buf, (unsigned)n) != n) { r = ERRO_INVALIDO; break; }
        feito += (unsigned)n;
        if (progreso) {
            info_captura_t i;
            captura_estado(fuente, &i);
            progreso(feito / (taxa * canais * 2 / 1000), i.pico);
        }
    }
    captura_wav_cabecera(cab, taxa, canais, feito);
    posicionar(fd, 0, 0);
    escrever(fd, cab, sizeof cab);
    fechar(fd);
    captura_cerrar();
    return r;
}
