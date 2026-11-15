// SPDX-License-Identifier: GPL-2.0-or-later
/* grabadora [-p] - la Grabadora de sonido (D292).
 *
 * Graba el micrófono (HDA o AC'97; en QEMU, silencio) en memoria a 44100 Hz mono, 16 bits; permite
 * escucharlo y guardarlo en WAV con el diálogo de guardar común (D272). El medidor de nivel
 * funciona también sin grabar («habla para probar»): la captura está abierta mientras la ventana
 * lo está. Con "<" y ">" se elige la entrada (micrófono interno, externo, línea) y con "Prueba" la fuente de prueba del
 * kernel (un tono de 1 kHz), que sirve para probar sin micrófono.
 *   -p, --prueba   empieza con la fuente de prueba
 * Los servicios no pueden grabar. Una grabación dura como mucho 120 s (10 MB). */
#include "sagui.h"
#include "gui.h"
#include "captura.h"

#define LARG 520
#define ALT  300
#define TAXA 44100
#define MAX_S 120
#define HIST 200

enum { PARADO, GRABANDO, TOCANDO };

static jan_t *j;
static unsigned fuente = CAP_FUENTE_REAL;
static info_captura_t inf;
static bool aberta;
static int estado = PARADO;
static uint8_t *dados;                 /* PCM mono 16 bits */
static unsigned dados_n, dados_cap;
static unsigned tocado;                /* bytes ya entregados al sonido */
static int fd_audio = -1;
static unsigned hist[HIST];
static unsigned hist_n, hist_t;
static char aviso[160];
static bool guardado;

static botao_t bt[] = {
    { 12, 12, 92, 28, "Grabar", false },
    { 112, 12, 92, 28, "Reproducir", false },
    { 212, 12, 92, 28, "Guardar", false },
    { 340, 12, 28, 28, "<", false },
    { 372, 12, 28, 28, ">", false },
    { 408, 12, 100, 28, "Prueba", false },
};
#define QTD_BT (int)(sizeof bt / sizeof bt[0])
enum { B_GRABAR, B_TOCAR, B_GUARDAR, B_ANT, B_SIG, B_PRUEBA };

static void parar_audio(void)
{
    if (fd_audio >= 0) { fechar(fd_audio); fd_audio = -1; }
}

static void abrir_captura(void)
{
    if (aberta) { captura_cerrar(); aberta = false; }
    aviso[0] = 0;
    int r = captura_estado(fuente, &inf);
    if (r < 0 || !inf.presente) {
        copia_texto(aviso, r == ERRO_ACESSO ? "Los servicios no pueden grabar" : "No hay micrófono (ninguna tarjeta con entrada de sonido). Prueba la fuente de prueba", sizeof aviso);
        return;
    }
    r = captura_abrir(fuente, TAXA, 1);
    if (r < 0) {
        formatar_texto(aviso, sizeof aviso, r == ERRO_OCUPADO ? "Otro programa está usando el micrófono" : "No se pudo abrir el micrófono (error %d)", r);
        return;
    }
    aberta = true;
}

/* ------------------------------------------------------------ dibujo */

static void formato_tempo(char *s, size_t max, unsigned bytes)
{
    unsigned seg = bytes / (TAXA * 2);
    formatar_texto(s, max, "%u:%02u", seg / 60, seg % 60);
}

static void desenhar_tudo(void)
{
    g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
    bt[B_GRABAR].texto = estado == GRABANDO ? "Parar" : "Grabar";
    bt[B_TOCAR].texto = estado == TOCANDO ? "Detener" : "Reproducir";
    bt[B_PRUEBA].texto = fuente == CAP_FUENTE_PRUEBA ? "Micrófono" : "Prueba";
    for (int i = 0; i < QTD_BT; i++) botao_desenhar(j, &bt[i]);
    char t[160];
    formatar_texto(t, sizeof t, "Entrada: %s", inf.presente && inf.n_entradas ? inf.entradas[inf.entrada < CAP_ENTRADAS_MAX ? inf.entrada : 0] : "-");
    g_texto(j, 12, 52, t, COR_TEXTO);

    /* medidor: barra con el pico y marca del RMS */
    int mx = 12, my = 78, ml = j->larg - 24, ma = 22;
    g_retangulo(j, mx, my, ml, ma, 0x20262C);
    unsigned pico = inf.pico > 32767 ? 32767 : inf.pico, rms = inf.rms > 32767 ? 32767 : inf.rms;
    int lp = (int)((unsigned long long)pico * (unsigned)(ml - 4) / 32767);
    int lr = (int)((unsigned long long)rms * (unsigned)(ml - 4) / 32767);
    uint32_t cor = pico > 29000 ? 0xD83A2A : pico > 20000 ? 0xE0B020 : 0x2EB872;
    if (lp > 0) g_retangulo(j, mx + 2, my + 2, lp, ma - 4, cor);
    if (lr > 0) g_retangulo(j, mx + 2 + lr, my + 2, 2, ma - 4, 0xFFFFFF);
    g_moldura(j, mx, my, ml, ma, COR_BORDA);
    formatar_texto(t, sizeof t, "Nivel: %u %%", pico * 100 / 32767);
    g_texto(j, 12, my + ma + 4, t, COR_TEXTO_FRACO);

    /* historial de niveles (lo último a la derecha) */
    int hx = 12, hy = 138, hl = j->larg - 24, ha = 72;
    g_retangulo(j, hx, hy, hl, ha, 0x20262C);
    for (unsigned k = 0; k < hist_n && k < (unsigned)hl; k++) {
        unsigned v = hist[(hist_t + HIST - 1 - k) % HIST];
        int h = (int)((unsigned long long)v * (unsigned)(ha - 4) / 32767);
        int x = hx + hl - 2 - (int)k * 2;
        if (x < hx + 1) break;
        if (h > 0) g_retangulo(j, x, hy + (ha - h) / 2, 1, h, 0x4FD08C);
    }
    g_moldura(j, hx, hy, hl, ha, COR_BORDA);

    char a[24], b[24];
    formato_tempo(a, sizeof a, estado == TOCANDO ? tocado : dados_n);
    formato_tempo(b, sizeof b, dados_n);
    formatar_texto(t, sizeof t, "%s / %s   %s", a, b, estado == GRABANDO ? "grabando..." : estado == TOCANDO ? "reproduciendo..." : dados_n ? (guardado ? "guardada" : "sin guardar") : "");
    g_texto(j, 12, hy + ha + 8, t, COR_TEXTO);
    if (aviso[0]) g_texto_n(j, 12, j->alt - 22, aviso, (j->larg - 24) / LETRA_L, 0xA02020);
    jan_mostrar_area(j, 0, 0, j->larg, j->alt);
}

/* ------------------------------------------------------------ acciones */

static void gravar_ou_parar(void)
{
    if (estado == GRABANDO) { estado = PARADO; return; }
    parar_audio();
    estado = PARADO;
    if (!aberta) abrir_captura();
    if (!aberta) return;
    if (!dados) {
        dados_cap = TAXA * 2u * MAX_S;
        dados = aloca(dados_cap);
        if (!dados) { copia_texto(aviso, "Sin memoria", sizeof aviso); return; }
    }
    dados_n = 0;
    guardado = false;
    aviso[0] = 0;
    estado = GRABANDO;
}

static void reproducir(void)
{
    if (estado == TOCANDO) { parar_audio(); estado = PARADO; return; }
    if (estado == GRABANDO || !dados_n) return;
    fd_audio = audio_abrir(TAXA, 1, 16);
    if (fd_audio < 0) { copia_texto(aviso, "No hay tarjeta de sonido para reproducir", sizeof aviso); return; }
    tocado = 0;
    estado = TOCANDO;
}

static void guardar(void)
{
    if (!dados_n || estado != PARADO) return;
    data_t d;
    ler_data(&d);
    char ini[120], ruta[200];
    criar_pasta("/home/grabaciones");
    formatar_texto(ini, sizeof ini, "/home/grabaciones/grabacion-%04u%02u%02u-%02u%02u%02u.wav", d.ano, d.mes, d.dia, d.hora, d.minuto, d.segundo);
    if (!dialogo_guardar("Guardar grabación", "WAV (*.wav)|*.wav", ini, ruta, sizeof ruta)) return;
    int fd = abrir(ruta, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { formatar_texto(aviso, sizeof aviso, "No se pudo crear %s (%d)", ruta, fd); return; }
    uint8_t cab[44];
    captura_wav_cabecera(cab, TAXA, 1, dados_n);
    bool ok = escrever(fd, cab, sizeof cab) == (int)sizeof cab;
    for (unsigned o = 0; ok && o < dados_n; ) {
        unsigned n = dados_n - o < 16384 ? dados_n - o : 16384;
        ok = escrever(fd, dados + o, n) == (int)n;
        o += n;
    }
    fechar(fd);
    if (!ok) formatar_texto(aviso, sizeof aviso, "Error al escribir %s", ruta);
    else { guardado = true; formatar_texto(aviso, sizeof aviso, "Guardada en %s", ruta); registrar("grabadora: guardada %s (%u bytes)\n", ruta, dados_n); }
}

static void cambiar_entrada(int d)
{
    if (!inf.presente || inf.n_entradas < 2 || fuente != CAP_FUENTE_REAL) return;
    unsigned n = inf.n_entradas;
    unsigned nueva = d < 0 ? (inf.entrada + n - 1) % n : (inf.entrada + 1) % n;
    if (captura_entrada(nueva) < 0) copia_texto(aviso, "No se pudo cambiar de entrada", sizeof aviso);
    captura_estado(fuente, &inf);
}

/* Cada ~20 ms: lee lo capturado (a la grabación o a la basura), actualiza el nivel y alimenta la reproducción. */
static bool ciclo(void)
{
    bool cambio = false;
    if (aberta) {
        static uint8_t tmp[8192];
        int n;
        while ((n = captura_ler(tmp, sizeof tmp)) > 0) {
            if (estado == GRABANDO) {
                unsigned c = (unsigned)n;
                if (dados_n + c > dados_cap) c = dados_cap - dados_n;
                memcpy(dados + dados_n, tmp, c);
                dados_n += c;
                if (dados_n >= dados_cap) { estado = PARADO; copia_texto(aviso, "Se alcanzó el máximo de 120 s", sizeof aviso); }
            }
        }
        captura_estado(fuente, &inf);
        hist[hist_t] = inf.pico;
        hist_t = (hist_t + 1) % HIST;
        if (hist_n < HIST) hist_n++;
        cambio = true;
    }
    if (estado == TOCANDO) {
        int livre = audio_livre(fd_audio);
        while (livre > 1 && tocado < dados_n) {
            unsigned n = dados_n - tocado;
            if (n > (unsigned)livre) n = (unsigned)livre & ~1u;
            if (n > 8192) n = 8192;
            int w = audio_escrever_sem_bloquear(fd_audio, dados + tocado, n);
            if (w <= 0) break;
            tocado += (unsigned)w;
            livre = audio_livre(fd_audio);
        }
        if (tocado >= dados_n && audio_pendente(fd_audio) <= 0) { parar_audio(); estado = PARADO; }
        cambio = true;
    }
    return cambio;
}

static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_TECLA) {
        if (e->tecla == ' ') { gravar_ou_parar(); desenhar_tudo(); }
        return true;
    }
    if (e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU)
        for (int i = 0; i < QTD_BT; i++)
            if (botao_tratar(j, &bt[i], e)) {
                aviso[0] = 0;
                switch (i) {
                case B_GRABAR: gravar_ou_parar(); break;
                case B_TOCAR: reproducir(); break;
                case B_GUARDAR: guardar(); break;
                case B_ANT: cambiar_entrada(-1); break;
                case B_SIG: cambiar_entrada(1); break;
                case B_PRUEBA:
                    if (estado == GRABANDO) estado = PARADO;
                    fuente = fuente == CAP_FUENTE_PRUEBA ? CAP_FUENTE_REAL : CAP_FUENTE_PRUEBA;
                    abrir_captura();
                    break;
                }
                desenhar_tudo();
                return true;
            }
    return true;
}

int principal(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
        if (!compara_texto(argv[i], "-p") || !compara_texto(argv[i], "--prueba")) fuente = CAP_FUENTE_PRUEBA;
        else { imprimir("uso: grabadora [-p]\n"); return 1; }
    j = jan_abrir("Grabadora", LARG, ALT);
    if (!j) return 1;
    abrir_captura();
    desenhar_tudo();
    registrar("grabadora: lista (%s)\n", inf.presente ? inf.nome : "sin micrófono");
    evento_t e;
    bool continuar = true;
    while (continuar) {
        if (esperar_evento(j->id, &e, 20) == 1) {
            continuar = tratar(&e);
            while (continuar && proximo_evento(j->id, &e, false) == 1) continuar = tratar(&e);
        }
        if (continuar && ciclo()) desenhar_tudo();
    }
    parar_audio();
    if (aberta) captura_cerrar();
    if (dados) libera(dados);
    jan_fechar(j);
    return 0;
}
