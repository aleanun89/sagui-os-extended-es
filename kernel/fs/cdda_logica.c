// SPDX-License-Identifier: GPL-2.0-or-later
/* cdda_logica.c - lo que no toca el hardware del CD de audio (D233): la tabla de
 * contenido, el CD-TEXT, los nombres de los archivos, la cabecera WAV y la lista
 * M3U. Sin dependencias del kernel (solo tipos.h y cdda.h): tests/cdda_unidad.c lo
 * compila en el PC con un lector simulado. */
#include "cdda.h"

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }

static void zerar(void *p, uint32_t n) { uint8_t *b = p; while (n--) *b++ = 0; }

static void copiar(char *d, const char *s, int max)
{
    int k = 0;
    while (s[k] && k < max - 1) { d[k] = s[k]; k++; }
    d[k] = 0;
}

/* Añade texto a s (de tamaño max); devuelve la nueva longitud. */
static int juntar(char *s, int k, int max, const char *t)
{
    while (*t && k < max - 1) s[k++] = *t++;
    s[k] = 0;
    return k;
}

static int juntar_num(char *s, int k, int max, uint32_t v, int digitos)
{
    char b[12];
    int n = 0;
    do { b[n++] = (char)('0' + v % 10); v /= 10; } while (v && n < 11);
    while (n < digitos) b[n++] = '0';
    while (n && k < max - 1) s[k++] = b[--n];
    s[k] = 0;
    return k;
}

/* ------------------------------------------------------------ tabla de contenido */

int cdda_toc_ler(const uint8_t *r, uint32_t n, cd_toc_t *t)
{
    t->qtd = 0;
    t->dados = 0;
    if (n < 4) return 0;
    uint32_t fim = ((uint32_t)r[0] << 8 | r[1]) + 2;
    if (fim > n) fim = n;
    /* Las entradas: [reservado, ADR/control, pista, reservado, LBA]; la 0xAA es el final. */
    struct { uint8_t num, ctl; uint32_t lba; } e[CDDA_MAX_PISTAS + 1];
    int ne = 0;
    for (uint32_t off = 4; off + 8 <= fim && ne < CDDA_MAX_PISTAS + 1; off += 8) {
        e[ne].num = r[off + 2];
        e[ne].ctl = r[off + 1] & 0x0F;
        e[ne].lba = be32(r + off + 4);
        if (e[ne].lba >= 0x00FFFFFFu) continue;   /* LBA negativo o absurdo */
        ne++;
        if (r[off + 2] == 0xAA) break;
    }
    for (int i = 0; i + 1 < ne; i++) {
        if (e[i].num == 0xAA || e[i].num == 0 || e[i].num > CDDA_MAX_PISTAS) continue;
        if (e[i].ctl & 0x04) { t->dados++; continue; }      /* pista de datos */
        uint32_t fim_p = e[i + 1].lba;
        if (fim_p <= e[i].lba) continue;
        /* Enhanced CD: entre la sesión de audio y la de datos hay 11400 sectores sin audio. */
        if (e[i + 1].num != 0xAA && (e[i + 1].ctl & 0x04) && fim_p - e[i].lba > CDDA_LEADOUT_SESSAO)
            fim_p -= CDDA_LEADOUT_SESSAO;
        if (t->qtd >= CDDA_MAX_PISTAS) break;
        cdda_pista_t *p = &t->pista[t->qtd++];
        p->numero = e[i].num;
        p->lba = e[i].lba;
        p->setores = fim_p - e[i].lba;
    }
    return t->qtd;
}

/* ------------------------------------------------------------ CD-TEXT */

/* Paquetes de 18 bytes: tipo (0x80 título, 0x81 intérprete...), pista (bit 7: extensión),
 * secuencia, bloque y posición (bit 7: dos bytes por carácter), 12 bytes de texto y CRC.
 * Los textos van seguidos con un 0 entre uno y otro, empezando por la pista que dice el
 * primer paquete; "\t" quiere decir "lo mismo que el anterior". Solo el bloque 0 (el
 * idioma principal) y los de un byte por carácter (ISO-8859-1 o ASCII). */
void cdda_cdtext_ler(const uint8_t *r, uint32_t n, cd_toc_t *t)
{
    if (n < 4) return;
    uint32_t fim = ((uint32_t)r[0] << 8 | r[1]) + 2;
    if (fim > n) fim = n;
    for (int tipo = 0x80; tipo <= 0x81; tipo++) {
        char buf[CDDA_TEXTO];
        int k = 0, pista = -1;
        char anterior[CDDA_TEXTO] = "";
        for (uint32_t off = 4; off + 18 <= fim; off += 18) {
            const uint8_t *p = r + off;
            if (p[0] != tipo || (p[3] & 0x70) || (p[3] & 0x80)) continue;
            if (pista < 0) { pista = p[1] & 0x7F; k = 0; }
            for (int i = 4; i < 16; i++) {
                uint8_t c = p[i];
                if (c) {
                    if (k < CDDA_TEXTO - 1) buf[k++] = (char)(c < 0x20 && c != '\t' ? '?' : c);
                    continue;
                }
                buf[k] = 0;
                const char *txt = (k == 1 && buf[0] == '\t') ? anterior : buf;
                if (pista == 0) copiar(tipo == 0x80 ? t->album : t->artista, txt, CDDA_TEXTO);
                else
                    for (int j = 0; j < t->qtd; j++)
                        if (t->pista[j].numero == pista) copiar(tipo == 0x80 ? t->titulo[j] : t->interprete[j], txt, CDDA_TEXTO);
                if (txt != anterior) copiar(anterior, buf, CDDA_TEXTO);
                pista++;
                k = 0;
            }
        }
    }
}

/* ------------------------------------------------------------ archivos */

void cdda_nome(const cd_toc_t *t, int i, char *s, int max)
{
    int k = 0;
    s[0] = 0;
    if (t->titulo[i][0]) {
        k = juntar_num(s, k, max, t->pista[i].numero, 2);
        k = juntar(s, k, max, " - ");
        /* Lo que no puede ir en un nombre de archivo, fuera; sin espacios ni puntos al final. */
        int lim = max - 5;   /* deja sitio para ".wav" */
        for (const char *c = t->titulo[i]; *c && k < lim; c++) {
            unsigned char x = (unsigned char)*c;
            bool malo = x < 0x20 || x == '/' || x == '\\' || x == ':' || x == '*' || x == '?' || x == '"' ||
                        x == '<' || x == '>' || x == '|' || x == 0x7F;
            s[k++] = malo ? '_' : (char)x;
        }
        while (k > 5 && (s[k - 1] == ' ' || s[k - 1] == '.')) k--;
        s[k] = 0;
    } else {
        k = juntar(s, k, max, "Pista");
        k = juntar_num(s, k, max, t->pista[i].numero, 2);
    }
    juntar(s, k, max, ".wav");
}

static void le32_poner(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

uint32_t cdda_tamanho(uint32_t setores) { return CDDA_CABECALHO + setores * CDDA_SETOR; }

void cdda_cabecalho(uint8_t c[CDDA_CABECALHO], uint32_t setores)
{
    uint32_t dados = setores * CDDA_SETOR;
    zerar(c, CDDA_CABECALHO);
    c[0] = 'R'; c[1] = 'I'; c[2] = 'F'; c[3] = 'F';
    le32_poner(c + 4, 36 + dados);
    c[8] = 'W'; c[9] = 'A'; c[10] = 'V'; c[11] = 'E';
    c[12] = 'f'; c[13] = 'm'; c[14] = 't'; c[15] = ' ';
    le32_poner(c + 16, 16);
    c[20] = 1;                       /* PCM */
    c[22] = 2;                       /* estéreo */
    le32_poner(c + 24, 44100);
    le32_poner(c + 28, 44100 * 4);   /* bytes por segundo */
    c[32] = 4;                       /* bytes por cuadro */
    c[34] = 16;                      /* bits por muestra */
    c[36] = 'd'; c[37] = 'a'; c[38] = 't'; c[39] = 'a';
    le32_poner(c + 40, dados);
}

uint32_t cdda_segundos(const cd_toc_t *t)
{
    uint32_t s = 0;
    for (int i = 0; i < t->qtd; i++) s += t->pista[i].setores;
    return s / CDDA_SETORES_SEG;
}

uint32_t cdda_m3u(const cd_toc_t *t, char *s, uint32_t max)
{
    int k = juntar(s, 0, (int)max, "#EXTM3U\n");
    for (int i = 0; i < t->qtd; i++) {
        char nome[64];
        cdda_nome(t, i, nome, sizeof nome);
        k = juntar(s, k, (int)max, "#EXTINF:");
        k = juntar_num(s, k, (int)max, t->pista[i].setores / CDDA_SETORES_SEG, 1);
        k = juntar(s, k, (int)max, ",");
        const char *art = t->interprete[i][0] ? t->interprete[i] : t->artista;
        if (t->titulo[i][0]) {
            if (art[0]) { k = juntar(s, k, (int)max, art); k = juntar(s, k, (int)max, " - "); }
            k = juntar(s, k, (int)max, t->titulo[i]);
        } else {
            k = juntar(s, k, (int)max, "Pista ");
            k = juntar_num(s, k, (int)max, t->pista[i].numero, 2);
        }
        k = juntar(s, k, (int)max, "\n");
        k = juntar(s, k, (int)max, nome);
        k = juntar(s, k, (int)max, "\n");
    }
    return (uint32_t)k;
}
