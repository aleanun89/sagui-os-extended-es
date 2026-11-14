// SPDX-License-Identifier: GPL-2.0-or-later
/* memoria - teste de memória grande (H46, D049).
 *
 *   memoria N        reserva N MB com aumentar_memoria (em blocos de 16 MB,
 *                    até N ou até o kernel recusar), escreve um padrão que
 *                    depende do endereço e confere tudo
 *   memoria ler HEX  lê o endereço dado (para testar a falta de página)
 *   memoria estres   prueba de estrés de aloca/libera/realoca (D209): tiempo de
 *                    cada fase y memoria usada del sistema antes, en el pico y
 *                    después de liberarlo todo (lo liberado debe volver)
 *   memoria mmap     mapas de memoria anónimos y de fichero (D209) */
#include "sagui.h"

#define BLOCO   (16u * 1024u * 1024u)
#define PASSO   16u   /* palavras: confere uma a cada 64 bytes */

static uint32_t hex(const char *s)
{
    uint32_t v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    for (; *s; s++) {
        int d = *s >= '0' && *s <= '9' ? *s - '0' : *s >= 'a' && *s <= 'f' ? *s - 'a' + 10 :
                *s >= 'A' && *s <= 'F' ? *s - 'A' + 10 : -1;
        if (d < 0) break;
        v = v * 16 + (uint32_t)d;
    }
    return v;
}

static inline uint32_t padrao(uint32_t *p, uint32_t volta) { return ((uint32_t)p * 2654435761u) ^ volta; }

/* ---- D209: estrés del malloc de la libsagui ---- */
unsigned memoria_total(void);

static uint32_t semente = 12345;
static uint32_t aleatorio(void) { semente = semente * 1103515245u + 12345u; return semente >> 8; }

static unsigned usada_kb(void)
{
    info_memoria_t m;
    info_sistema(INFO_MEMORIA, &m, sizeof m);
    return m.usada_kb;
}

#define VIVOS 4000

/* Tamaño al azar: casi todo pequeño, algo mediano y unos pocos grandes. */
static size_t tamanho_aleatorio(void)
{
    uint32_t r = aleatorio() % 1000;
    if (r < 700) return 1 + aleatorio() % 128;
    if (r < 950) return 129 + aleatorio() % 4096;
    if (r < 995) return 4096 + aleatorio() % 65536;
    return 65536 + aleatorio() % (2u * 1024 * 1024);
}

static void marcar(uint8_t *p, size_t n, uint8_t v)
{
    if (!n) return;
    p[0] = v; p[n - 1] = v; p[n / 2] = v;
}

static bool conferir(const uint8_t *p, size_t n, uint8_t v)
{
    return !n || (p[0] == v && p[n - 1] == v && p[n / 2] == v);
}

static int estres(void)
{
    static uint8_t *bloco[VIVOS];
    static size_t tam[VIVOS];
    unsigned erros = 0;
    unsigned u0 = usada_kb();
    imprimir("estres: usada al empezar %u KB\n", u0);

    /* 1. Fragmentación: 200.000 operaciones al azar con hasta 4.000 bloques vivos. */
    unsigned t = tempo_ms(), pico = 0;
    for (unsigned op = 0; op < 200000; op++) {
        unsigned i = aleatorio() % VIVOS;
        if (bloco[i]) {
            if (!conferir(bloco[i], tam[i], (uint8_t)i)) erros++;
            if (aleatorio() % 4 == 0) {
                size_t n = tamanho_aleatorio();
                uint8_t *q = realoca(bloco[i], n);
                if (!q) { erros++; continue; }
                size_t menor = n < tam[i] ? n : tam[i];
                if (!conferir(q, menor, (uint8_t)i) && menor == tam[i]) erros++;
                bloco[i] = q; tam[i] = n; marcar(q, n, (uint8_t)i);
            } else {
                libera(bloco[i]); bloco[i] = NULL;
            }
        } else {
            size_t n = tamanho_aleatorio();
            bloco[i] = aloca(n);
            if (!bloco[i]) { erros++; continue; }
            tam[i] = n; marcar(bloco[i], n, (uint8_t)i);
        }
        if (op % 10000 == 0) { unsigned u = usada_kb(); if (u > pico) pico = u; }
    }
    unsigned t1 = tempo_ms() - t;
    for (unsigned i = 0; i < VIVOS; i++) { libera(bloco[i]); bloco[i] = NULL; }
    unsigned u1 = usada_kb();
    imprimir("estres: fragmentacion %u ms, pico %u KB, tras liberar %u KB (programa %u KB)\n",
             t1, pico - u0, u1 - u0, memoria_total() / 1024);

    /* 2. Grandes: 64 MB en bloques de 1 MB, liberados. */
    t = tempo_ms();
    for (unsigned i = 0; i < 64; i++) { bloco[i] = aloca(1024 * 1024); if (bloco[i]) marcar(bloco[i], 1024 * 1024, 7); else erros++; }
    unsigned u2 = usada_kb();
    for (unsigned i = 0; i < 64; i++) { libera(bloco[i]); bloco[i] = NULL; }
    unsigned u3 = usada_kb();
    imprimir("estres: grandes %u ms, con 64 MB %u KB, tras liberar %u KB\n", tempo_ms() - t, u2 - u0, u3 - u0);

    /* 3. Muchos pequeños: 100.000 de 32 bytes, liberados al revés. */
    static uint8_t *peq[100000];
    t = tempo_ms();
    for (unsigned i = 0; i < 100000; i++) { peq[i] = aloca(32); if (!peq[i]) erros++; else marcar(peq[i], 32, 3); }
    for (unsigned i = 100000; i-- > 0;) libera(peq[i]);
    unsigned u4 = usada_kb();
    imprimir("estres: pequenos %u ms, tras liberar %u KB\n", tempo_ms() - t, u4 - u0);

    /* 4. realoca creciendo de 1 en 1 KB hasta 4 MB (como un búfer que se amplía). */
    t = tempo_ms();
    uint8_t *b = NULL;
    for (size_t n = 1024; n <= 4u * 1024 * 1024; n += 1024) {
        uint8_t *q = realoca(b, n);
        if (!q) { erros++; break; }
        if (b && q[n - 1025] != 9) erros++;
        b = q; b[n - 1] = 9; b[n - 1025] = 9;
    }
    libera(b);
    unsigned u5 = usada_kb();
    imprimir("estres: realoca %u ms, tras liberar %u KB\n", tempo_ms() - t, u5 - u0);
    imprimir("estres: %s (%u errores)\n", erros ? "FALLO" : "ok", erros);
    return erros ? 1 : 0;
}

/* D209: mapear_memoria/desmapear_memoria y mapear_arquivo. */
static int mapas(void)
{
    unsigned erros = 0, u0 = usada_kb();
    const size_t T = 8u * 1024 * 1024;
    uint32_t *m = mapear_memoria(T);
    if (!m || ((uint32_t)m & 4095)) { imprimir("mmap: mapear_memoria fallo\n"); return 1; }
    for (size_t i = 0; i < T / 4; i += 1024) if (m[i]) erros++;     /* viene a cero */
    for (size_t i = 0; i < T / 4; i += 1024) m[i] = (uint32_t)i;
    unsigned u1 = usada_kb();
    /* Quitar el trozo del medio (4 MB): lo de antes y lo de después sigue valiendo. */
    if (desmapear_memoria((uint8_t *)m + 2u * 1024 * 1024, 4u * 1024 * 1024)) erros++;
    unsigned u2 = usada_kb();
    for (size_t i = 0; i < T / 16; i += 1024) if (m[i] != i) erros++;
    for (size_t i = 3 * T / 16; i < T / 4; i += 1024) if (m[i] != i) erros++;
    if (desmapear_memoria(m, 2u * 1024 * 1024)) erros++;
    if (desmapear_memoria((uint8_t *)m + 6u * 1024 * 1024, 2u * 1024 * 1024)) erros++;
    if (!desmapear_memoria(m, 4096)) erros++;   /* ya no es un mapa: debe fallar */
    unsigned u3 = usada_kb();
    imprimir("mmap: 8 MB anonimos: con ellos +%u KB, quitado el medio +%u KB, al final +%u KB\n",
             u1 - u0, u2 - u0, u3 - u0);
    if (u1 - u0 < 8000 || u2 - u0 > u1 - u0 - 4000 || u3 - u0 > 1024) erros++;

    /* Un fichero: el mapa tiene lo mismo que leerlo. */
    int fd = abrir("/bin/sagsh", ABRIR_LER);
    if (fd < 0) { imprimir("mmap: no se abre /bin/sagsh\n"); return 1; }
    uint8_t *lido = aloca(65536);
    int n = ler(fd, lido, 65536);
    uint8_t *f = mapear_arquivo(fd, 0, 65536 + 100);
    if (!f || n <= 0) erros++;
    else {
        for (int i = 0; i < n; i++) if (f[i] != lido[i]) { erros++; break; }
        uint8_t *g = mapear_arquivo(fd, 4096, 4096);
        if (!g || memcmp(g, lido + 4096, 4096)) erros++;
        if (g) desmapear_memoria(g, 4096);
        desmapear_memoria(f, 65536 + 100);
    }
    fechar(fd);
    libera(lido);
    imprimir("mmap: fichero /bin/sagsh %d bytes comparados\n", n);
    imprimir("mmap: %s (%u errores)\n", erros ? "FALLO" : "ok", erros);
    return erros ? 1 : 0;
}

int principal(int argc, char **argv)
{
    if (argc > 1 && !compara_texto(argv[1], "mmap")) return mapas();
    if (argc > 1 && !compara_texto(argv[1], "estres")) return estres();
    if (argc > 2 && !compara_texto(argv[1], "ler")) {
        volatile uint32_t *p = (volatile uint32_t *)hex(argv[2]);
        imprimir("memoria: leyendo 0x%08x...\n", (uint32_t)p);
        uint32_t v = *p;
        imprimir("memoria: leído 0x%08x (no debería llegar aquí)\n", v);
        return 1;
    }
    unsigned mb = argc > 1 ? (unsigned)texto_para_int(argv[1]) : 64;
    uint8_t *ini = NULL;
    unsigned feito = 0;
    while (feito < mb) {
        unsigned n = mb - feito >= 16 ? BLOCO : (mb - feito) * 1024u * 1024u;
        uint8_t *b = aumentar_memoria(n);
        if (!b) break;
        if (!ini) ini = b;
        feito += n / (1024u * 1024u);
    }
    if (!ini) { imprimir("memoria: el kernel no dio memoria\n"); return 2; }
    uint32_t *a = (uint32_t *)ini, *z = (uint32_t *)(ini + (size_t)feito * 1024u * 1024u);
    imprimir("memoria: %u MB reservados en 0x%08x-0x%08x\n", feito, (uint32_t)a, (uint32_t)z);

    info_memoria_t m;
    info_sistema(INFO_MEMORIA, &m, sizeof m);
    imprimir("memoria: kernel gestiona %u MB, usada %u MB\n", m.gerida_kb / 1024, m.usada_kb / 1024);

    unsigned erros = 0;
    for (uint32_t volta = 1; volta <= 2; volta++) {
        for (uint32_t *p = a; p < z; p += PASSO) *p = padrao(p, volta);
        for (uint32_t *p = a; p < z; p += PASSO)
            if (*p != padrao(p, volta)) erros++;
    }
    /* As páginas vieram zeradas: o que fica entre as palavras do padrão é 0. */
    for (uint32_t *p = a + 1; p < z; p += 4093)
        if ((p - a) % PASSO && *p) erros++;
    if (erros) { imprimir("memoria: %u errores en el patron\n", erros); return 3; }
    imprimir("memoria: patron ok en %u MB\n", feito);
    return 0;
}
