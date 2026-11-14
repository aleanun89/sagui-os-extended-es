// SPDX-License-Identifier: GPL-2.0-or-later
/* cdda_unidad.c - prueba en el PC del CD de audio como carpeta de WAV (D233).
 *
 * El QEMU no emula CD de audio (ni -cdrom ni un .cue tienen pistas de audio para el
 * ATAPI), así que la lógica se prueba aquí con un lector simulado:
 *   - la tabla de contenido (READ TOC formato 0): CD de audio, CD mixto (datos en la
 *     pista 1) y Enhanced CD (datos en la segunda sesión: se quitan 11400 sectores);
 *   - el CD-TEXT (formato 5): título del disco, títulos por pista, intérprete, "\t" =
 *     el de antes, textos partidos entre paquetes;
 *   - los nombres ("Pista01.wav", "02 - Dos_Tres.wav") y la cabecera WAV;
 *   - el sistema de archivos cdda de verdad (kernel/fs/cdda.c): la carpeta, Lista.m3u,
 *     lecturas de tamaños y desplazamientos raros (la cabecera, el borde de los sectores,
 *     el final de la pista), cuántas órdenes llegan al lector con lecturas de 4 KB y el
 *     error si se saca el disco;
 *   - que Sintonía entiende la lista y la duración (sintonia_logica.h).
 * Se compila en dos partes (tests/h233_cd_en_caliente.py, parte "unidad"): con
 * -DCDDA_PONTE y las cabeceras del kernel (junto a cdda.c y cdda_logica.c), y sin ella,
 * con la libc del PC. La parte del PC escribe Pista01.wav y Lista.m3u en el directorio
 * que se le da, para que la prueba en Python los mire con su módulo "wave". */
#ifdef CDDA_PONTE
/* ================================================================ con las cabeceras del kernel */
#include "cdda.h"
#include "disco.h"
#include "montagem.h"
#include "texto.h"

void anf_comprobar(int ok, const char *que, int linha);
void anf_gravar(const char *nome, const void *b, uint32_t n);
#define C(c) anf_comprobar((c) ? 1 : 0, #c, __LINE__)

static uint8_t byte_sim(uint32_t lba, uint32_t i) { return (uint8_t)(lba * 31u + i * 7u + (i >> 8)); }

static int sem_disco;
static uint32_t leituras;

static int sim_ler(disco_t *d, uint32_t lba, uint32_t n, void *buf)
{
    (void)d;
    leituras++;
    if (sem_disco || n == 0 || n > 13) return -1;
    uint8_t *p = buf;
    for (uint32_t s = 0; s < n; s++)
        for (uint32_t i = 0; i < CDDA_SETOR; i++) p[s * CDDA_SETOR + i] = byte_sim(lba + s, i);
    return 0;
}

/* Una entrada de READ TOC formato 0. */
static uint32_t entrada(uint8_t *r, uint32_t off, uint8_t num, uint8_t ctl, uint32_t lba)
{
    r[off] = 0; r[off + 1] = (uint8_t)(0x10 | ctl); r[off + 2] = num; r[off + 3] = 0;
    r[off + 4] = (uint8_t)(lba >> 24); r[off + 5] = (uint8_t)(lba >> 16); r[off + 6] = (uint8_t)(lba >> 8); r[off + 7] = (uint8_t)lba;
    return off + 8;
}

static void cabeca(uint8_t *r, uint32_t fim) { r[0] = (uint8_t)((fim - 2) >> 8); r[1] = (uint8_t)(fim - 2); r[2] = 1; r[3] = 1; }

/* Paquetes de CD-TEXT de un tipo: los textos seguidos con su 0, en trozos de 12 bytes. */
static uint32_t paquetes(uint8_t *r, uint32_t off, uint8_t tipo, const char *const *textos, int n)
{
    char todo[512];
    uint32_t k = 0;
    uint8_t pista_de[512];
    for (int t = 0; t < n; t++) {
        for (const char *c = textos[t]; ; c++) {
            pista_de[k] = (uint8_t)t;
            todo[k++] = *c;
            if (!*c) break;
        }
    }
    static uint8_t seq;
    for (uint32_t i = 0; i < k; i += 12) {
        uint8_t *p = r + off;
        memset(p, 0, 18);
        p[0] = tipo; p[1] = pista_de[i]; p[2] = seq++; p[3] = 0;
        for (uint32_t j = 0; j < 12 && i + j < k; j++) p[4 + j] = (uint8_t)todo[i + j];
        off += 18;
    }
    return off;
}

int ponte_rodar(void)
{
    static cd_toc_t t;
    static uint8_t r[2048];
    uint32_t o;

    /* --- 1. CD de audio: 3 pistas */
    memset(&t, 0, sizeof t);
    o = 4;
    o = entrada(r, o, 1, 0, 0);
    o = entrada(r, o, 2, 0, 40);
    o = entrada(r, o, 3, 0, 115);
    o = entrada(r, o, 0xAA, 0, 128);
    cabeca(r, o);
    C(cdda_toc_ler(r, o, &t) == 3);
    C(t.dados == 0);
    C(t.pista[0].lba == 0 && t.pista[0].setores == 40);
    C(t.pista[1].lba == 40 && t.pista[1].setores == 75);
    C(t.pista[2].lba == 115 && t.pista[2].setores == 13);

    /* --- 2. CD mixto (datos en la pista 1) y Enhanced CD (datos en otra sesión) */
    cd_toc_t m;
    memset(&m, 0, sizeof m);
    o = 4;
    o = entrada(r, o, 1, 4, 0);
    o = entrada(r, o, 2, 0, 20000);
    o = entrada(r, o, 0xAA, 0, 40000);
    cabeca(r, o);
    C(cdda_toc_ler(r, o, &m) == 1 && m.dados == 1 && m.pista[0].numero == 2 && m.pista[0].setores == 20000);
    memset(&m, 0, sizeof m);
    o = 4;
    o = entrada(r, o, 1, 0, 0);
    o = entrada(r, o, 2, 0, 15000);
    o = entrada(r, o, 3, 4, 60000);
    o = entrada(r, o, 0xAA, 4, 80000);
    cabeca(r, o);
    C(cdda_toc_ler(r, o, &m) == 2 && m.dados == 1);
    C(m.pista[1].setores == 60000 - 11400 - 15000);   /* sin el cambio de sesión */
    /* Una respuesta cortada no lee fuera. */
    C(cdda_toc_ler(r, 10, &m) == 0);

    /* --- 3. CD-TEXT */
    const char *titulos[] = { "Mi disco", "Uno", "Dos/Tres", "\t" };
    const char *interpretes[] = { "La banda", "", "Invitada", "" };
    memset(r, 0, sizeof r);
    o = 4;
    o = paquetes(r, o, 0x80, titulos, 4);
    o = paquetes(r, o, 0x81, interpretes, 4);
    o = paquetes(r, o, 0x8F, titulos, 1);     /* otros tipos: se ignoran */
    cabeca(r, o);
    cdda_cdtext_ler(r, o, &t);
    C(txt_compara(t.album, "Mi disco") == 0);
    C(txt_compara(t.artista, "La banda") == 0);
    C(txt_compara(t.titulo[0], "Uno") == 0);
    C(txt_compara(t.titulo[1], "Dos/Tres") == 0);
    C(txt_compara(t.titulo[2], "Dos/Tres") == 0);   /* "\t": el mismo que el anterior */
    C(txt_compara(t.interprete[1], "Invitada") == 0);
    char nome[NOME_MAX];
    cdda_nome(&t, 1, nome, sizeof nome);
    C(txt_compara(nome, "02 - Dos_Tres.wav") == 0);
    cd_toc_t sem = t;
    memset(sem.titulo, 0, sizeof sem.titulo);
    cdda_nome(&sem, 0, nome, sizeof nome);
    C(txt_compara(nome, "Pista01.wav") == 0);
    /* Un título larguísimo cabe con su ".wav". */
    memset(sem.titulo[0], 'x', CDDA_TEXTO - 1);
    sem.titulo[0][CDDA_TEXTO - 1] = 0;
    cdda_nome(&sem, 0, nome, 20);
    C(txt_tamanho(nome) == 19 && txt_compara(nome + 15, ".wav") == 0);

    /* --- 4. la cabecera WAV */
    uint8_t c[CDDA_CABECALHO];
    cdda_cabecalho(c, 75);
    C(memcmp(c, "RIFF", 4) == 0 && memcmp(c + 8, "WAVEfmt ", 8) == 0 && memcmp(c + 36, "data", 4) == 0);
    C(c[40] == (uint8_t)(75 * 2352) && c[41] == (uint8_t)((75 * 2352) >> 8) && c[42] == (uint8_t)((75 * 2352) >> 16));
    C(cdda_tamanho(75) == 44 + 75 * 2352);
    C(cdda_segundos(&t) == 128 / 75);

    /* --- 5. el sistema de archivos, sobre el disco sin CD-TEXT */
    static disco_t d;
    memset(&d, 0, sizeof d);
    txt_copia_n(d.nome, "cd0audio", sizeof d.nome);
    d.presente = true;
    d.audio_cd = true;
    d.toc = &sem;
    memset(sem.titulo, 0, sizeof sem.titulo);
    memset(sem.interprete, 0, sizeof sem.interprete);
    sem.album[0] = sem.artista[0] = 0;
    d.ler_audio = sim_ler;
    uint32_t op = 0;
    void *v = NULL;
    C(fs_cdda.montar(&d, &op, &v) == 0 && (op & MONTAR_SO_LEITURA));
    entrada_pasta_t e;
    C(fs_cdda.listar(v, "", 0, &e) == 1 && txt_compara(e.nome, "Pista01.wav") == 0 && e.tamanho == 44 + 40 * 2352);
    C(fs_cdda.listar(v, "", 3, &e) == 1 && txt_compara(e.nome, "Lista.m3u") == 0);
    C(fs_cdda.listar(v, "", 4, &e) == 0);
    info_arquivo_t inf;
    C(fs_cdda.info(v, "", &inf) == 0 && inf.tipo == TIPO_PASTA);
    C(fs_cdda.info(v, "pista02.WAV", &inf) == 0 && inf.tamanho == 44 + 75 * 2352);   /* sin distinguir mayúsculas */
    C(fs_cdda.info(v, "Pista09.wav", &inf) == ERRO_NAO_EXISTE);
    C(fs_cdda.listar(v, "Pista01.wav", 0, &e) == ERRO_NAO_E_PASTA);
    static uint8_t arq[64];
    uint32_t tam;
    C(fs_cdda.abrir(v, "Pista01.wav", ABRIR_ESCREVER, arq, &tam) == ERRO_SO_LEITURA);
    C(fs_cdda.apagar(v, "Pista01.wav") == ERRO_SO_LEITURA);
    /* La pista 2 entera, en trozos de tamaños raros. */
    C(fs_cdda.abrir(v, "Pista02.wav", 0, arq, &tam) == 0 && tam == 44 + 75 * 2352);
    static uint8_t buf[44 + 75 * 2352 + 100];
    uint32_t pasos[] = { 1, 43, 2, 2351, 2353, 4096, 7, 30000, 9999 };
    uint32_t pos = 0;
    int k = 0, err = 0;
    while (pos < tam) {
        int n = fs_cdda.ler(v, arq, pos, buf + pos, pasos[k++ % 9]);
        if (n <= 0) { err = 1; break; }
        pos += (uint32_t)n;
    }
    C(!err && pos == tam);
    C(fs_cdda.ler(v, arq, tam, buf, 10) == 0);   /* al final: nada */
    cdda_cabecalho(c, 75);
    C(memcmp(buf, c, 44) == 0);
    int malo = 0;
    for (uint32_t i = 0; i < 75 * 2352; i++)
        if (buf[44 + i] != byte_sim(40 + i / 2352, i % 2352)) { malo = 1; break; }
    C(!malo);
    /* Saltos: bytes sueltos en sitios arbitrarios. */
    uint32_t sitios[] = { 44 + 2351, 44 + 2352, 44 + 13 * 2352 - 1, 44 + 74 * 2352 + 2351, 44 + 5 };
    for (int i = 0; i < 5; i++) {
        uint8_t b = 0;
        uint32_t p = sitios[i] - 44;
        C(fs_cdda.ler(v, arq, sitios[i], &b, 1) == 1 && b == byte_sim(40 + p / 2352, p % 2352));
    }
    /* De 4 KB en 4 KB (como el reproductor): una orden al lector cada 13 sectores. */
    leituras = 0;
    for (pos = 44; pos < tam; pos += 4096) fs_cdda.ler(v, arq, pos, buf, 4096);
    C(leituras >= 5 && leituras <= (75 + 12) / 13);   /* la primera ventana puede estar ya en la caché */
    /* La pista 1 entera y la lista, al PC. */
    C(fs_cdda.abrir(v, "Pista01.wav", 0, arq, &tam) == 0);
    C(fs_cdda.ler(v, arq, 0, buf, tam) == (int)tam);
    anf_gravar("Pista01.wav", buf, tam);
    C(fs_cdda.abrir(v, "Lista.m3u", 0, arq, &tam) == 0 && tam > 20);
    C(fs_cdda.ler(v, arq, 0, buf, tam) == (int)tam);
    anf_gravar("Lista.m3u", buf, tam);
    /* Sin disco: la lectura falla (no inventa silencio). */
    sem_disco = 1;
    C(fs_cdda.abrir(v, "Pista03.wav", 0, arq, &tam) == 0);
    C(fs_cdda.ler(v, arq, 100, buf, 100) < 0);
    C(fs_cdda.ler(v, arq, 0, buf, 44) == 44);     /* la cabecera no necesita el disco */
    sem_disco = 0;
    uint32_t total_kb, livre;
    fs_cdda.espaco(v, &total_kb, &livre);
    C(total_kb == 128 * 2352 / 1024 && livre == 0);
    C(fs_cdda.desmontar(v, false) == 0);

    /* --- 6. con CD-TEXT: los nombres y la lista */
    d.toc = &t;
    C(fs_cdda.montar(&d, &op, &v) == 0);
    C(fs_cdda.listar(v, "", 1, &e) == 1 && txt_compara(e.nome, "02 - Dos_Tres.wav") == 0);
    C(fs_cdda.abrir(v, "Lista.m3u", 0, arq, &tam) == 0);
    C(fs_cdda.ler(v, arq, 0, buf, tam) == (int)tam);
    anf_gravar("Lista-cdtext.m3u", buf, tam);
    fs_cdda.desmontar(v, false);
    d.toc = NULL;
    C(fs_cdda.montar(&d, &op, &v) == ERRO_FORMATO);
    return 0;
}

#else
/* ================================================================ en el PC */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define SINTONIA_ANFITRION
#include "sintonia_logica.h"

static int total, fallos;
static const char *dir_saida = ".";
int ponte_rodar(void);

void anf_comprobar(int ok, const char *que, int linha)
{
    total++;
    if (!ok) { fallos++; printf("FALLO cdda_unidad.c:%d: %s\n", linha, que); }
}

void anf_gravar(const char *nome, const void *b, unsigned n)
{
    char c[512];
    snprintf(c, sizeof c, "%s/%s", dir_saida, nome);
    FILE *f = fopen(c, "wb");
    if (!f) { anf_comprobar(0, c, __LINE__); return; }
    fwrite(b, 1, n, f);
    fclose(f);
}

/* Lo que el kernel pide a su alrededor. */
void klog(const char *fmt, ...) { va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }
int snformatar(char *d, unsigned max, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = vsnprintf(d, max, fmt, ap); va_end(ap); return r; }
void *kmalloc(unsigned n) { return malloc(n); }
void *kcalloc(unsigned n) { return calloc(1, n); }
void kfree(void *p) { free(p); }
unsigned txt_tamanho(const char *s) { return (unsigned)strlen(s); }
int txt_compara(const char *a, const char *b) { return strcmp(a, b); }
char *txt_copia_n(char *d, const char *s, unsigned n) { if (!n) return d; strncpy(d, s, n - 1); d[n - 1] = 0; return d; }

static unsigned ler_tudo(const char *nome, char *buf, unsigned max)
{
    char c[512];
    snprintf(c, sizeof c, "%s/%s", dir_saida, nome);
    FILE *f = fopen(c, "rb");
    if (!f) return 0;
    unsigned n = (unsigned)fread(buf, 1, max, f);
    fclose(f);
    return n;
}

int main(int argc, char **argv)
{
    if (argc > 1) dir_saida = argv[1];
    ponte_rodar();
    /* Sintonía con lo que ha salido del sistema de archivos. */
    static char txt[16384];
    static sl_pista_t p[8];
    unsigned n = ler_tudo("Lista.m3u", txt, sizeof txt - 1);
    int k = sl_lista_leer(txt, n, "/cd", p, 8);
    anf_comprobar(k == 3, "Sintonía lee las 3 pistas de Lista.m3u", __LINE__);
    anf_comprobar(k == 3 && !strcmp(p[0].ruta, "/cd/Pista01.wav") && !strcmp(p[2].ruta, "/cd/Pista03.wav"), "rutas de la lista", __LINE__);
    anf_comprobar(k == 3 && p[1].dur_s == 1 && !strcmp(p[1].titulo, "Pista 02"), "duración y título de la lista", __LINE__);
    n = ler_tudo("Lista-cdtext.m3u", txt, sizeof txt - 1);
    k = sl_lista_leer(txt, n, "/cd", p, 8);
    anf_comprobar(k == 3 && !strcmp(p[1].ruta, "/cd/02 - Dos_Tres.wav") && !strcmp(p[1].titulo, "Dos/Tres") &&
                  !strcmp(p[1].artista, "Invitada") && !strcmp(p[0].artista, "La banda"), "lista con CD-TEXT", __LINE__);
    static uint8_t wav[44 + 40 * 2352];
    n = ler_tudo("Pista01.wav", (char *)wav, sizeof wav);
    sl_meta_t m;
    anf_comprobar(n == sizeof wav && sl_meta_leer(wav, 16384, NULL, 0, NULL, n, &m) && m.formato == 3 && m.dur_s == 0,
                  "Sintonía reconoce el WAV y su duración", __LINE__);
    printf("cdda_unidad: %d comprobaciones, %d fallos\n", total, fallos);
    return fallos != 0;
}
#endif
