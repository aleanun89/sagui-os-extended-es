// SPDX-License-Identifier: GPL-2.0-or-later
/* reproducir [-v volumen] archivo... - reproduce archivos de sonido por la tarjeta de sonido (D088, D098).
 *
 * Entiende WAV PCM (formato 1, o el "extensible" con subformato PCM) de 8 o
 * 16 bits, mono o estéreo, de 4 a 96 kHz: el kernel convierte a la salida
 * de la tarjeta (48 kHz estéreo) y mezcla con lo que más esté sonando. Los
 * otros formatos (WAV IMA ADPCM, MP3 y FLAC, mejora 053) pasan por los
 * decodificadores de la libsagui (audio_formato.h), que dan PCM de 16 bits; el
 * formato se reconoce por el contenido, no por la extensión. Muestra formato,
 * tasa, canales, duración y etiquetas (título, artista, álbum).
 *
 *   reproducir --volcar entrada saida.pcm   decodifica a PCM de 16 bits (pruebas)
 *   reproducir --medir archivo...           sólo decodifica y mide el tiempo (sin tarjeta)
 *
 * Streaming: el sonido empieza en cuanto hay ~0,3 s leídos, y el resto del archivo
 * sigue leyéndose mientras suena, con un anillo de 1 MB por delante del
 * flujo (el "segundo búfer"; el primero es el anillo del flujo en el kernel). El bucle
 * alterna: lee un trozo si cabe en el anillo, entrega al flujo sin bloquear lo
 * que acepte y, con el anillo lleno, espera a que el flujo consuma 64 KB. Si la
 * lectura no da abasto (un pendrive lento), el sonido se corta un instante en vez
 * de esperar a que se lea el archivo entero. Al final espera a que salga la última muestra de la
 * tarjeta. Matar el programa cierra el flujo y el sonido para al instante. */
#include "sagui.h"
#include "audio_formato.h"

/* Trozos pequeños: la lectura de un pendrive ocurre dentro de la llamada de
 * sistema, con las interrupciones desactivadas; con un trozo grande el mezclador
 * puede quedarse sin ejecutarse demasiado tiempo (D089). */
#define PEDACO 4096
#define ANEL   (1024u * 1024u)    /* múltiplo de cualquier tamaño de cuadro (1, 2 o 4) */
#define BLOCO  65536u             /* lo que se espera que consuma el flujo con el anillo lleno */

static uint8_t anel[ANEL];
static uint32_t anel_ini, anel_qtd;

/* Lee un trozo del archivo al anillo; devuelve los bytes leídos (0 = fin/error). */
static uint32_t ler_pedaco(int fd, uint32_t falta)
{
    uint32_t fim = (anel_ini + anel_qtd) % ANEL;
    uint32_t n = PEDACO;
    if (n > falta) n = falta;
    if (n > ANEL - anel_qtd) n = ANEL - anel_qtd;
    if (n > ANEL - fim) n = ANEL - fim;
    int r = ler(fd, anel + fim, n);
    if (r <= 0) return 0;
    anel_qtd += (uint32_t)r;
    return (uint32_t)r;
}

/* Entrega al flujo hasta max bytes desde el inicio del anillo (sólo cuadros completos). */
static void entregar(int a, uint32_t bq, uint32_t max, bool esperar)
{
    while (anel_qtd >= bq && max >= bq) {
        uint32_t n = anel_qtd < ANEL - anel_ini ? anel_qtd : ANEL - anel_ini;
        if (n > max) n = max;
        n -= n % bq;
        if (!n) break;
        int r = esperar ? audio_escrever(a, anel + anel_ini, n) : audio_escrever_sem_bloquear(a, anel + anel_ini, n);
        if (r <= 0) return;
        anel_ini = (anel_ini + (uint32_t)r) % ANEL;
        anel_qtd -= (uint32_t)r;
        max -= (uint32_t)r;
        if (!esperar && (uint32_t)r < n) return;   /* el flujo se llenó */
    }
}

static uint32_t le32(const uint8_t *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint32_t le16(const uint8_t *p) { return p[0] | p[1] << 8; }

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

static bool pular(int fd, uint32_t n)
{
    return posicionar(fd, (int)n, POS_ATUAL) >= 0;
}

static int tocar_wav(const char *nome, int volume)
{
    int fd = abrir(nome, ABRIR_LER);
    if (fd < 0) {
        imprimir_em(ERROS, "reproducir: %s: %s\n", nome, nome_erro(fd));
        return 1;
    }
    uint8_t cab[12], ch[8], fmt[40];
    if (!ler_tudo(fd, cab, 12) || memcmp(cab, "RIFF", 4) != 0 || memcmp(cab + 8, "WAVE", 4) != 0) {
        imprimir_em(ERROS, "reproducir: %s: no es un archivo WAV\n", nome);
        fechar(fd);
        return 1;
    }
    uint32_t taxa = 0, canais = 0, bits = 0, dados = 0;
    bool tem_fmt = false;
    for (;;) {
        if (!ler_tudo(fd, ch, 8)) {
            imprimir_em(ERROS, "reproducir: %s: falta el bloque de datos\n", nome);
            fechar(fd);
            return 1;
        }
        uint32_t tam = le32(ch + 4);
        if (memcmp(ch, "fmt ", 4) == 0 && tam >= 16) {
            uint32_t n = tam > sizeof fmt ? sizeof fmt : tam;
            if (!ler_tudo(fd, fmt, n) || !pular(fd, tam - n + (tam & 1))) break;
            uint32_t tipo = le16(fmt);
            if (tipo == 0xFFFE && n >= 26) tipo = le16(fmt + 24);   /* WAVE_FORMAT_EXTENSIBLE: el subformato */
            canais = le16(fmt + 2);
            taxa = le32(fmt + 4);
            bits = le16(fmt + 14);
            if (tipo != 1) {
                imprimir_em(ERROS, "reproducir: %s: formato %u no soportado (solo PCM sin comprimir)\n", nome, tipo);
                fechar(fd);
                return 1;
            }
            tem_fmt = true;
        } else if (memcmp(ch, "data", 4) == 0) {
            dados = tam;
            break;
        } else if (!pular(fd, tam + (tam & 1))) break;
    }
    if (!tem_fmt || (bits != 8 && bits != 16) || (canais != 1 && canais != 2) ||
        taxa < AUDIO_TAXA_MIN || taxa > AUDIO_TAXA_MAX) {
        imprimir_em(ERROS, "reproducir: %s: formato no soportado (%u Hz, %u bits, %u canales)\n", nome, taxa, bits, canais);
        fechar(fd);
        return 1;
    }
    uint32_t bq = canais * bits / 8;
    uint32_t q = dados / bq, ms = q / taxa * 1000 + q % taxa * 1000 / taxa;   /* sin división de 64 bits */
    int a = audio_abrir((int)taxa, (int)canais, (int)bits);
    if (a < 0) {
        if (a == ERRO_NAO_EXISTE) imprimir_em(ERROS, "reproducir: no hay tarjeta de sonido (solo el altavoz del PC)\n");
        else imprimir_em(ERROS, "reproducir: %s\n", nome_erro(a));
        fechar(fd);
        return 1;
    }
    if (volume >= 0) audio_volume(a, volume);
    imprimir("Reproduciendo %s: %u Hz, %u bits, %s, %u,%u s\n", nome, taxa, bits, canais == 2 ? "estéreo" : "mono",
             ms / 1000, ms / 100 % 10);
    anel_ini = anel_qtd = 0;
    uint32_t falta = dados, lidos = 0, t0 = tempo_ms(), inicio = taxa * bq * 3 / 10;
    /* Antes de empezar: ~0,3 s de sonido (o el archivo, si es más corto). No
     * se intenta adivinar si la lectura va a alcanzar midiendo el tiempo: con
     * el pendrive, el kernel pasa tanto tiempo con las interrupciones desactivadas
     * que el reloj se atrasa (D089) y la cuenta saldría mal. Si no alcanza,
     * el sonido se corta un poco de vez en cuando. */
    while (falta && anel_qtd < inicio) {
        uint32_t r = ler_pedaco(fd, falta);
        if (!r) { falta = 0; break; }
        falta -= r;
        lidos += r;
    }
    registrar("reproducir: comeca a tocar com %u KB lidos em %u ms\n", lidos / 1024, tempo_ms() - t0);
    while (anel_qtd >= bq || falta) {
        entregar(a, bq, ANEL, false);
        if (falta && anel_qtd < ANEL) {
            uint32_t r = ler_pedaco(fd, falta);
            if (!r) falta = 0;   /* archivo cortado: toca lo que tenía */
            falta -= r;
            lidos += r;
            if (!falta) {
                uint32_t dt = tempo_ms() - t0;
                registrar("reproducir: %u KB lidos em %u ms (%u KB/s)\n", lidos / 1024, dt,
                          dt ? lidos / dt * 1000 / 1024 + lidos % dt * 1000 / dt / 1024 : 0);
            }
            continue;
        }
        entregar(a, bq, falta ? BLOCO : ANEL, true);   /* anillo lleno o archivo entero leído: espera al flujo */
    }
    fechar(fd);
    audio_fechar(a, 1);   /* espera a que termine de sonar */
    registrar("reproducir: %s terminado (%u ms)\n", nome, ms);
    return 0;
}

/* ------------------------------------------------------------ otros formatos (mejora 053) */

#define PED_QUADROS 1152          /* un cuadro de MP3 por vez */

static uint64_t rdtsc(void)
{
    uint32_t a, d;
    __asm__ volatile("rdtsc" : "=a"(a), "=d"(d));
    return (uint64_t)d << 32 | a;
}

static const char *texto_canais(uint32_t c) { return c == 2 ? "estéreo" : "mono"; }

static void mostrar_info(const char *nome, const audio_info_t *i)
{
    uint32_t ms = i->duracao_ms;
    imprimir("Reproduciendo %s: %s, %u Hz, %s", nome, i->nome_formato, i->taxa, texto_canais(i->canais));
    if (i->bits_origem > 4) imprimir(", %u bits", i->bits_origem);
    if (i->kbps) imprimir(", %u kbps%s", i->kbps, i->vbr ? " (VBR)" : "");
    if (ms) imprimir(", %u,%u s", ms / 1000, ms / 100 % 10);
    imprimir("\n");
    if (i->titulo[0]) imprimir("  Título:  %s\n", i->titulo);
    if (i->artista[0]) imprimir("  Artista: %s\n", i->artista);
    if (i->album[0]) imprimir("  Álbum:   %s\n", i->album);
}

/* Decodifica un trozo para el anillo; devuelve false al final o con error (erro_final recibe el código). */
static bool decodificar_pedaco(audio_arquivo_t *arq, uint32_t canais, int *erro_final)
{
    static int16_t tmp[PED_QUADROS * 2];
    int n = audio_arquivo_ler(arq, tmp, PED_QUADROS);
    if (n <= 0) {
        if (n < 0) *erro_final = n;
        return false;
    }
    uint32_t bytes = (uint32_t)n * canais * 2, fim = (anel_ini + anel_qtd) % ANEL;
    uint32_t p = bytes < ANEL - fim ? bytes : ANEL - fim;
    memcpy(anel + fim, tmp, p);
    if (bytes > p) memcpy(anel, (uint8_t *)tmp + p, bytes - p);
    anel_qtd += bytes;
    return true;
}

static void avisar_erro(const char *nome, int erro, audio_arquivo_t *arq)
{
    imprimir_em(ERROS, "reproducir: %s: %s\n", nome, audio_erro_texto(erro));
    if (arq && audio_arquivo_info(arq)->quadros_ruins)
        imprimir_em(ERROS, "reproducir: %s: %u cuadros dañados se saltaron\n", nome, audio_arquivo_info(arq)->quadros_ruins);
}

/* Reproduce un archivo ya abierto por la libsagui. */
static int tocar_decodificado(const char *nome, audio_arquivo_t *arq, int volume)
{
    const audio_info_t *i = audio_arquivo_info(arq);
    uint32_t canais = i->canais, bq = canais * 2;
    int a = audio_abrir((int)i->taxa, (int)canais, 16);
    if (a < 0) {
        if (a == ERRO_NAO_EXISTE) imprimir_em(ERROS, "reproducir: no hay tarjeta de sonido (solo el altavoz del PC)\n");
        else imprimir_em(ERROS, "reproducir: %s\n", nome_erro(a));
        audio_arquivo_fechar(arq);
        return 1;
    }
    if (volume >= 0) audio_volume(a, volume);
    mostrar_info(nome, i);
    anel_ini = anel_qtd = 0;
    bool fim = false;
    int erro = 0;
    uint32_t t0 = tempo_ms(), inicio = i->taxa * bq * 3 / 10;
    while (!fim && anel_qtd < inicio)
        if (!decodificar_pedaco(arq, canais, &erro)) fim = true;
    registrar("reproducir: comeca a tocar com %u KB decodificados em %u ms\n", anel_qtd / 1024, tempo_ms() - t0);
    while (anel_qtd >= bq || !fim) {
        entregar(a, bq, ANEL, false);
        if (!fim && anel_qtd + PED_QUADROS * bq <= ANEL) {
            if (!decodificar_pedaco(arq, canais, &erro)) fim = true;
            continue;
        }
        entregar(a, bq, fim ? ANEL : BLOCO, true);
    }
    audio_fechar(a, 1);
    if (erro) avisar_erro(nome, erro, arq);
    else if (i->quadros_ruins) avisar_erro(nome, AUDIO_ERRO_CORROMPIDO, arq);
    audio_arquivo_fechar(arq);
    registrar("reproducir: %s terminado (%u ms)\n", nome, tempo_ms() - t0);
    return erro ? 1 : 0;
}

/* --volcar: el PCM de 16 bits intercalado va a un archivo. */
static int volcar(const char *entrada, const char *saida)
{
    int erro;
    audio_arquivo_t *arq = audio_arquivo_abrir(entrada, &erro);
    if (!arq) {
        imprimir_em(ERROS, "reproducir: %s: %s\n", entrada, audio_erro_texto(erro));
        return 1;
    }
    const audio_info_t *i = audio_arquivo_info(arq);
    int fd = abrir(saida, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) {
        imprimir_em(ERROS, "reproducir: %s: %s\n", saida, nome_erro(fd));
        audio_arquivo_fechar(arq);
        return 1;
    }
    imprimir("%s: %s, %u Hz, %u canales, %u cuadros\n", entrada, i->nome_formato, i->taxa, i->canais, i->quadros);
    static int16_t buf[PED_QUADROS * 2];
    uint32_t total = 0;
    int n;
    while ((n = audio_arquivo_ler(arq, buf, PED_QUADROS)) > 0) {
        if (escrever(fd, buf, (unsigned)n * i->canais * 2) < 0) { n = ERRO_ACESSO; break; }
        total += (uint32_t)n;
    }
    fechar(fd);
    imprimir("volcados %u cuadros (%u dañados)\n", total, i->quadros_ruins);
    if (n < 0) avisar_erro(entrada, n, arq);
    audio_arquivo_fechar(arq);
    return n < 0 ? 1 : 0;
}

/* --medir: decodifica sin reproducir y dice cuánto cuesta (ms y millones de ciclos por segundo de audio). */
static int medir(const char *nome)
{
    int erro;
    audio_arquivo_t *arq = audio_arquivo_abrir(nome, &erro);
    if (!arq) {
        imprimir_em(ERROS, "reproducir: %s: %s\n", nome, audio_erro_texto(erro));
        return 1;
    }
    const audio_info_t *i = audio_arquivo_info(arq);
    static int16_t buf[PED_QUADROS * 2];
    uint32_t total = 0, t0 = tempo_ms();
    uint64_t c0 = rdtsc();
    int n;
    while ((n = audio_arquivo_ler(arq, buf, PED_QUADROS)) > 0) total += (uint32_t)n;
    uint32_t kc = (uint32_t)((rdtsc() - c0) >> 10), ms = tempo_ms() - t0;   /* kc: miles de ciclos (sin división de 64 bits) */
    uint32_t dur = total / i->taxa * 1000 + total % i->taxa * 1000 / i->taxa;   /* ms de audio */
    imprimir("%s: %s, %u Hz, %u canales: %u cuadros (%u ms) decodificados en %u ms\n", nome, i->nome_formato, i->taxa,
             i->canais, total, dur, ms);
    if (dur) {
        uint32_t x = kc / dur * 1000 + kc % dur * 1000 / dur;   /* miles de ciclos (x1024) por segundo de audio */
        uint32_t mc = x * 10 / 977;                              /* décimas de Mciclos por segundo */
        imprimir("  %u,%u Mciclos por segundo de audio", mc / 10, mc % 10);
        if (ms) imprimir("; %u,%u veces el tiempo real", dur * 10 / ms / 10, dur * 10 / ms % 10);
        imprimir("\n");
    }
    if (n < 0) avisar_erro(nome, n, arq);
    audio_arquivo_fechar(arq);
    return n < 0 ? 1 : 0;
}

static int tocar_arquivo(const char *nome, int volume)
{
    int erro;
    audio_arquivo_t *arq = audio_arquivo_abrir(nome, &erro);
    if (!arq) {
        imprimir_em(ERROS, "reproducir: %s: %s\n", nome, audio_erro_texto(erro));
        return 1;
    }
    if (audio_arquivo_info(arq)->formato == AUDIO_FMT_WAV) {   /* PCM: el kernel convierte; el camino antiguo */
        audio_arquivo_fechar(arq);
        return tocar_wav(nome, volume);
    }
    return tocar_decodificado(nome, arq, volume);
}

int principal(int argc, char **argv)
{
    int volume = -1, i = 1, erros = 0;
    if (i + 1 < argc && compara_texto(argv[i], "-v") == 0) {
        volume = texto_para_int(argv[i + 1]);
        if (volume < 0 || volume > 100) volume = 100;
        i += 2;
    }
    if (i + 2 < argc && compara_texto(argv[i], "--volcar") == 0) return volcar(argv[i + 1], argv[i + 2]);
    if (i + 1 < argc && compara_texto(argv[i], "--medir") == 0) {
        for (i++; i < argc; i++) erros += medir(argv[i]);
        return erros ? 1 : 0;
    }
    if (i >= argc) {
        imprimir_em(ERROS, "uso: reproducir [-v volumen 0-100] archivo.wav|mp3|flac...\n");
        return 1;
    }
    for (; i < argc; i++) erros += tocar_arquivo(argv[i], volume);
    return erros ? 1 : 0;
}
