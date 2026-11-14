// SPDX-License-Identifier: GPL-2.0-or-later
/* midi [archivo | carpeta]... - el Reproductor de MIDI de SaguiOS (D190).
 *
 * Toca archivos MIDI estándar (.mid, .midi, .kar, .rmi) sintetizándolos a PCM con audio_midi.h (la
 * síntesis vive en la libsagui y la comparten `musica`, `reproducir` y los demás). Ventana con el título y
 * el autor/copyright del archivo, barra de progreso con tiempo y salto, reproducir/pausa/parar,
 * anterior/siguiente, volumen, la lista de reproducción (los archivos MIDI de una carpeta), la
 * letra de los .kar (sílaba por sílaba, con la ya cantada resaltada) y, sobre todo, una vista de
 * los 16 canales: el instrumento de GM de cada uno, un teclado de 88 notas que enciende las que
 * suenan y un medidor de actividad. La vista sigue al sonido que se oye (no al que se está
 * sintetizando: la síntesis va ~0,4 s por delante) usando las instantáneas de midi_estado_em.
 *
 * Teclas: espacio = reproducir/pausa, flechas izquierda/derecha = -/+ 5 s, N = siguiente, P = anterior,
 * S = parar, +/- = volumen, Ctrl+O = abrir, Intro = tocar la pista elegida.
 *
 * Órdenes de terminal (sin ventana):
 *   midi --a-wav archivo.mid salida.wav   sintetiza a un WAV de 16 bits estéreo de 44100 Hz
 *   midi --medir archivo.mid...           sintetiza sin tarjeta y dice cuánto tardó (CPU)
 *   midi --info archivo.mid               título, formato, duración, notas, programas usados
 *
 * Reproducción y pausa: mismo esquema que `musica` (D115): un anillo de cuadros sintetizados, el
 * flujo del kernel con ~0,4 s por delante y la pausa que descarta lo no sonado y lo vuelve a entregar. */
#include "sagui.h"
#include "gui.h"
#include "audio_midi.h"

#define LARG_INICIO 920
#define ALT_INICIO  612
#define LARG_MIN 800
#define ALT_MIN  592
#define MARGEM 12
#define RODAPE 22
#define MAX_PISTAS 256
#define PED_QUADROS 1024
#define ANEL_Q 32768u          /* cuadros del anillo (potencia de 2) */
#define ADELANTE_MS 400
#define SALTO_MS 5000

/* vista de canales */
#define CAN_Y     190
#define CAN_H     20
#define NOME_X    34
#define NOME_L    20           /* caracteres del nombre del instrumento */
#define TEC_X     (NOME_X + NOME_L * LETRA_L + 4)
#define NOTA_MIN  21
#define NOTA_MAX  108
#define NOTA_L    5            /* píxeles por nota */
#define TEC_L     ((NOTA_MAX - NOTA_MIN + 1) * NOTA_L)
#define MED_X     (TEC_X + TEC_L + 8)
#define MED_L     56
#define LISTA_X   (MED_X + MED_L + 16)
#define LETRA_Y   (CAN_Y + 16 * CAN_H + 8)

enum { PARADO, TOCANDO, PAUSADO };

static jan_t *j;
static char rutas[MAX_PISTAS][200];
static char nomes[MAX_PISTAS][64];
static int qtd, atual = -1;
static lista_t lista;

static midi_t *mid;
static const midi_info_t *inf;
static int estado = PARADO;
static int fluxo = -1;
static int volume = 100;
static char aviso[160];

static int16_t anel[ANEL_Q * 2];
static uint32_t n_dec, n_esc;   /* cuadros sintetizados y entregados al flujo, desde base_q */
static uint32_t base_q;
static bool fim_dec;

static bool arrastando_barra, arrastando_vol;
static uint32_t previa_ms;
static int bx, by, bw;
static int vx, vy, vw;
static int px_mostrado = -1;

/* lo que se ve de cada canal, para no redibujar lo que no cambió */
static midi_estado_t est, est_desenhado;
static bool visto[MIDI_CANAIS];
static bool can_desenhado[MIDI_CANAIS];
static int letra_desenhada = -2;

static const uint32_t cores[MIDI_CANAIS] = {
    0xE0483E, 0xF0861E, 0xE6B91A, 0xA2C62A, 0x42B54C, 0x1FB38A, 0x1EA2CE, 0x2A74DC,
    0x4D54DC, 0x8C4FD6, 0xD0A04A, 0xC04BBE, 0xE04D8C, 0x7C8C96, 0x9A6B3E, 0x5FA8A0,
};

static botao_t bt_arq[] = {
    { 12, 8, 64, 24, "Abrir", false },
    { 84, 8, 72, 24, "Añadir", false },
    { 164, 8, 64, 24, "Vaciar", false },
};
static botao_t bt_ctl[] = {
    { 12, 150, 36, 26, "|<", false },
    { 56, 150, 100, 26, "Reproducir", false },
    { 164, 150, 64, 26, "Parar", false },
    { 236, 150, 36, 26, ">|", false },
};
#define BT_ANT 0
#define BT_PLAY 1
#define BT_PARAR 2
#define BT_SIG 3

static const char *const gm_nomes[128] = {
    "Piano de cola", "Piano brillante", "Piano eléctrico", "Piano de bar", "Piano eléctr. 1", "Piano eléctr. 2", "Clavicémbalo", "Clavinet",
    "Celesta", "Glockenspiel", "Caja de música", "Vibráfono", "Marimba", "Xilófono", "Campanas tubular.", "Dulcimer",
    "Órgano Hammond", "Órgano percusivo", "Órgano de rock", "Órgano de iglesia", "Armonio", "Acordeón", "Armónica", "Bandoneón",
    "Guitarra nylon", "Guitarra acústica", "Guitarra jazz", "Guitarra limpia", "Guitarra sorda", "Guitarra saturada", "Guitarra distors.", "Armónicos guitarra",
    "Contrabajo", "Bajo con dedos", "Bajo con púa", "Bajo sin trastes", "Bajo slap 1", "Bajo slap 2", "Bajo sintet. 1", "Bajo sintet. 2",
    "Violín", "Viola", "Violonchelo", "Contrabajo (arco)", "Cuerdas trémolo", "Cuerdas pizzicato", "Arpa", "Timbales",
    "Cuerdas 1", "Cuerdas 2", "Cuerdas sintet. 1", "Cuerdas sintet. 2", "Coro aah", "Voces ooh", "Voz sintética", "Golpe orquestal",
    "Trompeta", "Trombón", "Tuba", "Trompeta sorda", "Trompa", "Sección de metales", "Metales sintet. 1", "Metales sintet. 2",
    "Saxo soprano", "Saxo alto", "Saxo tenor", "Saxo barítono", "Oboe", "Corno inglés", "Fagot", "Clarinete",
    "Flautín", "Flauta", "Flauta dulce", "Flauta de pan", "Botella soplada", "Shakuhachi", "Silbato", "Ocarina",
    "Lead cuadrada", "Lead diente sierra", "Lead calíope", "Lead chiff", "Lead charang", "Lead voz", "Lead quintas", "Lead bajo+lead",
    "Pad new age", "Pad cálido", "Pad polisintet.", "Pad coro", "Pad arco", "Pad metálico", "Pad halo", "Pad barrido",
    "FX lluvia", "FX banda sonora", "FX cristal", "FX atmósfera", "FX brillo", "FX duendes", "FX ecos", "FX ciencia ficción",
    "Sitar", "Banjo", "Shamisen", "Koto", "Kalimba", "Gaita", "Violín popular", "Shehnai",
    "Campanilla", "Agogó", "Tambor de acero", "Bloque de madera", "Taiko", "Tom melódico", "Tambor sintético", "Platillo invertido",
    "Ruido de traste", "Respiración", "Oleaje", "Pájaros", "Teléfono", "Helicóptero", "Aplausos", "Disparo",
};

/* ------------------------------------------------------------ utilidades */

static const char *nome_curto(const char *c)
{
    const char *b = acha_ultimo_char(c, '/');
    return b ? b + 1 : c;
}

static uint32_t quadros_ms(uint32_t q)
{
    return q / MIDI_TAXA * 1000 + q % MIDI_TAXA * 1000 / MIDI_TAXA;
}

static uint32_t duracao_ms(void) { return inf ? inf->duracao_ms : 0; }

static uint32_t pendentes_q(void)
{
    if (fluxo < 0) return 0;
    int ms = audio_pendente(fluxo);
    if (ms <= 0) return 0;
    uint32_t q = (uint32_t)ms * (MIDI_TAXA / 100) / 10;
    return q > n_esc ? n_esc : q;
}

static uint32_t posicao_q(void)
{
    return base_q + n_esc - (estado == TOCANDO ? pendentes_q() : 0);
}

static uint32_t posicao_ms(void)
{
    uint32_t ms = quadros_ms(posicao_q()), d = duracao_ms();
    return d && ms > d ? d : ms;
}

static void formatar_tempo(char *s, size_t max, uint32_t ms)
{
    uint32_t t = ms / 1000;
    if (t >= 3600) formatar_texto(s, max, "%u:%02u:%02u", t / 3600, t / 60 % 60, t % 60);
    else formatar_texto(s, max, "%u:%02u", t / 60, t % 60);
}

static bool extensao_midi(const char *c)
{
    unsigned n = (unsigned)tam_texto(c), p = n;
    while (p && c[p - 1] != '.' && c[p - 1] != '/') p--;
    if (!p || c[p - 1] != '.') return false;
    char e[5] = { 0 };
    for (unsigned k = 0; k < 4 && p + k < n; k++) e[k] = (char)(c[p + k] | 0x20);
    return compara_texto(e, "mid") == 0 || compara_texto(e, "midi") == 0 || compara_texto(e, "kar") == 0 || compara_texto(e, "rmi") == 0;
}

/* ------------------------------------------------------------ dibujo */

static void atualizar_nomes(void)
{
    for (int i = 0; i < qtd; i++) {
        char t[64];
        formatar_texto(t, sizeof t, "%s%s", i == atual ? "> " : "  ", nome_curto(rutas[i]));
        copia_texto(nomes[i], t, sizeof nomes[i]);
    }
    lista.itens = nomes;
    lista.qtd = qtd;
}

static void layout(void)
{
    int l = j->larg;
    bx = MARGEM;
    bw = l - 2 * MARGEM;
    by = 124;
    bt_ctl[BT_PLAY].texto = estado == TOCANDO ? "Pausa" : "Reproducir";
    vw = 100;
    vx = l - MARGEM - vw - 40;
    vy = 158;
    lista.x = LISTA_X;
    lista.y = CAN_Y;
    lista.l = l - MARGEM - LISTA_X;
    lista.a = j->alt - RODAPE - lista.y - 6;
    if (lista.l < 40) lista.l = 40;
    if (lista.a < 40) lista.a = 40;
}

static void texto_linha(int y, const char *t, uint32_t cor)
{
    g_texto_n(j, MARGEM, y, t, (j->larg - 2 * MARGEM) / LETRA_L, cor);
}

static void desenhar_progresso(void)
{
    uint32_t pos = arrastando_barra ? previa_ms : posicao_ms(), dur = duracao_ms();
    g_retangulo(j, 0, 100, j->larg, 44, COR_FUNDO);
    char a[24], b[24], t[64];
    formatar_tempo(a, sizeof a, pos);
    if (dur) formatar_tempo(b, sizeof b, dur); else copia_texto(b, "--:--", sizeof b);
    formatar_texto(t, sizeof t, "%s / %s", a, b);
    g_texto(j, bx, 102, t, COR_TEXTO);
    g_retangulo(j, bx, by, bw, 12, COR_BOTAO_APERTADO);
    g_moldura(j, bx - 1, by - 1, bw + 2, 14, COR_BORDA);
    int f = 0;
    if (dur && inf) {
        f = (int)((pos / 64) * (uint32_t)bw / (dur / 64 | 1));
        if (f > bw) f = bw;
        if (f > 0) g_retangulo(j, bx, by, f, 12, COR_DESTAQUE);
        g_retangulo(j, bx + f - 1 < bx ? bx : bx + f - 1, by - 3, 3, 18, COR_TEXTO);
    }
    px_mostrado = f;   /* se redibuja cuando cambia el píxel */
    jan_mostrar_area(j, 0, 100, j->larg, 44);
}

static void desenhar_volume(void)
{
    g_retangulo(j, vx - 44, 150, j->larg - vx + 44, 26, COR_FUNDO);
    g_texto(j, vx - 40, vy - 6, "Vol", COR_TEXTO_FRACO);
    g_retangulo(j, vx, vy - 1, vw, 8, COR_BOTAO_APERTADO);
    g_moldura(j, vx - 1, vy - 2, vw + 2, 10, COR_BORDA);
    int f = volume * vw / 100;
    if (f > 0) g_retangulo(j, vx, vy - 1, f, 8, COR_DESTAQUE);
    g_retangulo(j, vx + f - 2 < vx ? vx : vx + f - 2, vy - 5, 4, 16, COR_TEXTO);
    char t[8];
    formatar_texto(t, sizeof t, "%d%%", volume);
    g_texto(j, vx + vw + 6, vy - 6, t, COR_TEXTO);
    jan_mostrar_area(j, vx - 44, 150, j->larg - vx + 44, 26);
}

static void desenhar_info(void)
{
    g_retangulo(j, 0, 38, j->larg, 62, COR_FUNDO);
    if (!inf || atual < 0) {
        texto_linha(44, "Ningún archivo MIDI abierto", COR_TEXTO);
        texto_linha(62, "Pulsa Abrir o Añadir (Ctrl+O), o usa en el terminal: midi archivo.mid", COR_TEXTO_FRACO);
    } else {
        char t[200];
        texto_linha(44, inf->titulo[0] ? inf->titulo : nome_curto(rutas[atual]), COR_TEXTO);
        if (inf->autor[0] && inf->copyright[0]) formatar_texto(t, sizeof t, "%s - %s", inf->autor, inf->copyright);
        else if (inf->autor[0]) copia_texto(t, inf->autor, sizeof t);
        else if (inf->copyright[0]) copia_texto(t, inf->copyright, sizeof t);
        else t[0] = 0;
        texto_linha(62, t, COR_TEXTO_FRACO);
        uint32_t bpm = inf->tempo_ini ? 60000000u / inf->tempo_ini : 0;
        formatar_texto(t, sizeof t, "%s, %d %s, %u notas, %u tics/negra%s%u bpm, %u voces", inf->karaoke ? "MIDI karaoke" : inf->formato ? "MIDI formato 1" : "MIDI formato 0",
                       inf->trilhas, inf->trilhas == 1 ? "pista" : "pistas", inf->notas, inf->divisao, bpm ? ", " : "", bpm, est.vozes);
        texto_linha(80, t, COR_TEXTO);
    }
    jan_mostrar_area(j, 0, 38, j->larg, 62);
}

static bool nota_tecla_negra(int n)
{
    int p = n % 12;
    return p == 1 || p == 3 || p == 6 || p == 8 || p == 10;
}

static uint32_t escurecer(uint32_t c)
{
    return (c >> 1) & 0x7F7F7F;
}

/* Un canal: número, instrumento, teclado y medidor. */
static void desenhar_canal(int c)
{
    int y = CAN_Y + c * CAN_H;
    g_retangulo(j, 0, y, LISTA_X - 8, CAN_H, COR_FUNDO);
    bool perc = c == 9;
    uint32_t cor = cores[c];
    bool vivo = visto[c];
    char t[8];
    formatar_texto(t, sizeof t, "%02d", c + 1);
    g_texto(j, MARGEM, y + 2, t, vivo ? cor : COR_TEXTO_FRACO);
    const char *nome = perc ? "Percusión (GM)" : gm_nomes[est.programa[c] & 127];
    g_texto_n(j, NOME_X, y + 2, vivo ? nome : "-", NOME_L, vivo ? COR_TEXTO : COR_TEXTO_FRACO);
    /* teclado: celdas claras y oscuras (blancas y negras), y las notas que suenan */
    g_retangulo(j, TEC_X, y + 1, TEC_L, CAN_H - 3, 0xFAFAF8);
    for (int n = NOTA_MIN; n <= NOTA_MAX; n++)
        if (nota_tecla_negra(n)) g_retangulo(j, TEC_X + (n - NOTA_MIN) * NOTA_L, y + 1, NOTA_L, CAN_H - 3, 0xC4C8CC);
    for (int n = NOTA_MIN; n <= NOTA_MAX; n += 12) g_retangulo(j, TEC_X + (n - NOTA_MIN) * NOTA_L, y + 1, 1, CAN_H - 3, 0x9AA2AA);
    for (int n = 0; n < 128; n++) {
        if (!(est.ativas[c][n >> 5] & (1u << (n & 31)))) continue;
        int k = n < NOTA_MIN ? NOTA_MIN : n > NOTA_MAX ? NOTA_MAX : n;
        g_retangulo(j, TEC_X + (k - NOTA_MIN) * NOTA_L, y + 1, NOTA_L, CAN_H - 3, nota_tecla_negra(k) ? escurecer(cor) : cor);
    }
    g_moldura(j, TEC_X - 1, y, TEC_L + 2, CAN_H - 1, COR_BORDA);
    /* medidor de actividad */
    g_retangulo(j, MED_X, y + 3, MED_L, CAN_H - 8, 0xD6DADE);
    int f = est.nivel[c] * MED_L / 255;
    if (f > 0) {
        g_retangulo(j, MED_X, y + 3, f, CAN_H - 8, cor);
        if (est.nivel[c] > 200) g_retangulo(j, MED_X + f - 4 < MED_X ? MED_X : MED_X + f - 4, y + 3, 4, CAN_H - 8, 0xE0483E);
    }
    g_moldura(j, MED_X - 1, y + 2, MED_L + 2, CAN_H - 6, COR_BORDA);
    jan_mostrar_area(j, 0, y, LISTA_X - 8, CAN_H);
    est_desenhado.programa[c] = est.programa[c];
    est_desenhado.nivel[c] = est.nivel[c];
    memcpy(est_desenhado.ativas[c], est.ativas[c], sizeof est.ativas[c]);
    can_desenhado[c] = vivo;
}

/* Redibuja solo los canales que cambiaron. */
static void atualizar_canais(bool tudo)
{
    for (int c = 0; c < MIDI_CANAIS; c++) {
        bool ativo = false;
        for (int k = 0; k < 4; k++) if (est.ativas[c][k]) ativo = true;
        if (ativo || est.nivel[c]) visto[c] = true;
        bool muda = tudo || visto[c] != can_desenhado[c] || est.programa[c] != est_desenhado.programa[c] ||
                    (est.nivel[c] >> 2) != (est_desenhado.nivel[c] >> 2) ||
                    memcmp(est.ativas[c], est_desenhado.ativas[c], sizeof est.ativas[c]) != 0;
        if (muda) desenhar_canal(c);
    }
}

/* ------------------------------------------------------------ letra (karaoke) */

#define MAX_LINHAS 1024
static uint16_t linha_ini[MAX_LINHAS];
static unsigned n_linhas;

static bool inicia_linha(const char *t) { return t[0] == '/' || t[0] == '\\'; }

static void montar_linhas(void)
{
    n_linhas = 0;
    unsigned n = midi_letra_qtd(mid);
    for (unsigned i = 0; i < n && n_linhas < MAX_LINHAS; i++) {
        const midi_letra_t *l = midi_letra(mid, i);
        if (i == 0 || inicia_linha(l->texto)) linha_ini[n_linhas++] = (uint16_t)i;
    }
}

static unsigned linha_de(unsigned idx)
{
    unsigned k = 0;
    for (unsigned i = 0; i < n_linhas; i++) if (linha_ini[i] <= idx) k = i;
    return k;
}

/* Dibuja una línea de la letra centrada; las sílabas hasta `ate` (inclusive) van resaltadas. */
static void desenhar_linha_letra(int y, unsigned linha, int ate, bool forte)
{
    unsigned n = midi_letra_qtd(mid);
    unsigned a = linha_ini[linha], b = linha + 1 < n_linhas ? linha_ini[linha + 1] : n;
    int len = 0;
    for (unsigned i = a; i < b; i++) {
        const char *t = midi_letra(mid, i)->texto;
        if (i == a && inicia_linha(t)) t++;
        len += (int)tam_texto(t);
    }
    int w = len * LETRA_L, x0 = (LISTA_X - 8 - w) / 2;
    if (x0 < MARGEM) x0 = MARGEM;
    int x = x0;
    for (unsigned i = a; i < b; i++) {
        const char *t = midi_letra(mid, i)->texto;
        if (i == a && inicia_linha(t)) t++;
        char s[48];
        int k = 0;
        for (; *t && k < 46; t++) s[k++] = (*t == '\r' || *t == '\n') ? ' ' : *t;
        s[k] = 0;
        uint32_t cor = !forte ? COR_TEXTO_FRACO : (int)i <= ate ? 0xC8341F : COR_TEXTO;
        g_texto(j, x, y, s, cor);
        x += k * LETRA_L;
    }
}

/* Registra en la serie la línea de letra que se muestra (con sus espacios), para las pruebas. */
static void registrar_linha(unsigned l)
{
    static int ultima = -1;
    if ((int)l == ultima) return;
    ultima = (int)l;
    unsigned n = midi_letra_qtd(mid);
    unsigned a = linha_ini[l], z = l + 1 < n_linhas ? linha_ini[l + 1] : n;
    char buf[200];
    unsigned k = 0;
    for (unsigned i = a; i < z; i++) {
        const char *t = midi_letra(mid, i)->texto;
        if (i == a && inicia_linha(t)) t++;
        for (; *t && k < sizeof buf - 1; t++) buf[k++] = (*t == '\r' || *t == '\n') ? ' ' : *t;
    }
    buf[k] = 0;
    registrar("midi: letra: [%s]\n", buf);
}

static void desenhar_letra(int idx)
{
    int h = j->alt - RODAPE - LETRA_Y - 2;
    g_retangulo(j, 0, LETRA_Y - 2, LISTA_X - 8, h + 2, COR_FUNDO);
    if (mid && midi_letra_qtd(mid) && n_linhas) {
        unsigned l = idx < 0 ? 0 : linha_de((unsigned)idx);
        registrar_linha(l);
        desenhar_linha_letra(LETRA_Y, l, idx, true);
        if (l + 1 < n_linhas) desenhar_linha_letra(LETRA_Y + 20, l + 1, -1, false);
    } else if (inf && atual >= 0) {
        g_texto(j, MARGEM, LETRA_Y + 4, "Este archivo no trae letra.", COR_TEXTO_FRACO);
    }
    letra_desenhada = idx;
    jan_mostrar_area(j, 0, LETRA_Y - 2, LISTA_X - 8, h + 2);
}

static void desenhar_rodape(void)
{
    int y = j->alt - RODAPE;
    g_retangulo(j, 0, y, j->larg, RODAPE, 0xE6E8EA);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);
    char t[200];
    if (aviso[0]) copia_texto(t, aviso, sizeof t);
    else if (qtd == 0) copia_texto(t, "Lista vacía", sizeof t);
    else formatar_texto(t, sizeof t, "%s  |  pista %d de %d", estado == TOCANDO ? "Reproduciendo" : estado == PAUSADO ? "En pausa" : "Detenido",
                        atual + 1, qtd);
    g_texto_n(j, 8, y + 3, t, (j->larg - 16) / LETRA_L, aviso[0] ? 0x9A1B1B : COR_TEXTO);
    jan_mostrar_area(j, 0, y, j->larg, RODAPE);
}

static void atualizar_titulo(void)
{
    char t[120];
    if (inf && atual >= 0) formatar_texto(t, sizeof t, "%s - MIDI", inf->titulo[0] ? inf->titulo : nome_curto(rutas[atual]));
    else copia_texto(t, "MIDI", sizeof t);
    titulo_janela(j->id, t);
}

static void ler_estado(void)
{
    if (mid) midi_estado_em(mid, posicao_q(), &est);
    else {
        memset(&est, 0, sizeof est);
        for (int c = 0; c < MIDI_CANAIS; c++) { est.volume[c] = 100; est.pan[c] = 64; }
    }
}

static void desenhar_tudo(void)
{
    layout();
    g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
    g_retangulo(j, 0, 36, j->larg, 1, COR_BORDA);
    for (int i = 0; i < 3; i++) botao_desenhar(j, &bt_arq[i]);
    ler_estado();
    desenhar_info();
    desenhar_progresso();
    for (int i = 0; i < 4; i++) botao_desenhar(j, &bt_ctl[i]);
    desenhar_volume();
    atualizar_canais(true);
    atualizar_nomes();
    if (qtd) lista_desenhar(j, &lista);
    else {
        g_retangulo(j, lista.x, lista.y, lista.l, lista.a, COR_BRANCO);
        g_moldura(j, lista.x, lista.y, lista.l, lista.a, COR_BORDA);
        g_texto(j, lista.x + 8, lista.y + 8, "Lista vacía.", COR_TEXTO_FRACO);
    }
    desenhar_letra(letra_desenhada < -1 ? -1 : letra_desenhada);
    desenhar_rodape();
    jan_mostrar(j);
}

/* ------------------------------------------------------------ reproducción */

static void fechar_fluxo(void)
{
    if (fluxo >= 0) { fechar(fluxo); fluxo = -1; }
}

static void fechar_faixa(void)
{
    fechar_fluxo();
    if (mid) { midi_fechar(mid); mid = NULL; inf = NULL; }
    estado = PARADO;
    n_dec = n_esc = base_q = 0;
    fim_dec = false;
    n_linhas = 0;
    letra_desenhada = -2;
    memset(visto, 0, sizeof visto);
}

static void reiniciar_anel(uint32_t ms)
{
    base_q = ms / 1000 * MIDI_TAXA + ms % 1000 * MIDI_TAXA / 1000;
    n_dec = n_esc = 0;
    fim_dec = false;
}

static void iniciar(void)
{
    if (!mid) return;
    fluxo = audio_abrir(MIDI_TAXA, 2, 16);
    if (fluxo < 0) {
        if (fluxo == ERRO_NAO_EXISTE) copia_texto(aviso, "No hay tarjeta de sonido (solo el altavoz del PC): no se puede reproducir", sizeof aviso);
        else formatar_texto(aviso, sizeof aviso, "No se puede abrir el sonido: %s", nome_erro(fluxo));
        registrar("midi: sin sonido (%d)\n", fluxo);
        fluxo = -1;
        estado = PARADO;
        return;
    }
    audio_volume(fluxo, volume);
    estado = TOCANDO;
    aviso[0] = 0;
    registrar("midi: reproduciendo %s\n", nome_curto(rutas[atual]));
}

static bool abrir_faixa(int i, bool tocar)
{
    fechar_faixa();
    atual = i;
    lista.sel = i;
    int erro;
    mid = midi_abrir(rutas[i], &erro);
    if (!mid) {
        formatar_texto(aviso, sizeof aviso, "%s: %s", nome_curto(rutas[i]), erro == AUDIO_ERRO_FORMATO ? "no es un archivo MIDI" : audio_erro_texto(erro));
        registrar("midi: error al abrir %s (%d)\n", rutas[i], erro);
        return false;
    }
    inf = midi_info(mid);
    aviso[0] = 0;
    reiniciar_anel(0);
    montar_linhas();
    registrar("midi: pista %d %s: %s %u ms %d pistas %u notas %u letras titulo '%s'\n", i + 1, nome_curto(rutas[i]),
              inf->karaoke ? "karaoke" : inf->formato ? "formato 1" : "formato 0", inf->duracao_ms, inf->trilhas, inf->notas, inf->letras, inf->titulo);
    if (tocar) iniciar();
    return true;
}

static void outra_faixa(int passo, bool tocar, bool dar_volta)
{
    if (!qtd) return;
    int i = atual;
    for (int tent = 0; tent < qtd; tent++) {
        i += passo;
        if (i < 0 || i >= qtd) {
            if (!dar_volta) {
                fechar_faixa();
                abrir_faixa(0, false);
                registrar("midi: fin de la lista\n");
                return;
            }
            i = i < 0 ? qtd - 1 : 0;
        }
        if (abrir_faixa(i, tocar)) return;
    }
}

static void parar(void)
{
    if (!mid) return;
    uint32_t pos = posicao_ms();
    fechar_fluxo();
    midi_buscar(mid, 0);
    reiniciar_anel(0);
    estado = PARADO;
    registrar("midi: parado en %u ms\n", pos);
}

static void pausar(void)
{
    if (estado != TOCANDO) return;
    uint32_t pend = pendentes_q(), lo = n_dec > ANEL_Q ? n_dec - ANEL_Q : 0;
    n_esc = n_esc - pend < lo ? lo : n_esc - pend;
    audio_controle(fluxo, AUDIO_DESCARTAR, 0);
    estado = PAUSADO;
    registrar("midi: pausa en %u ms\n", posicao_ms());
}

static void alternar(void)
{
    if (estado == TOCANDO) { pausar(); return; }
    if (estado == PAUSADO) { estado = TOCANDO; registrar("midi: sigue en %u ms\n", posicao_ms()); return; }
    if (!mid && qtd) abrir_faixa(atual >= 0 ? atual : 0, true);
    else if (mid) iniciar();
}

static void saltar(uint32_t ms)
{
    if (!mid) return;
    uint32_t d = duracao_ms();
    if (d && ms > d) ms = d;
    uint32_t desde = posicao_ms();
    if (fluxo >= 0) audio_controle(fluxo, AUDIO_DESCARTAR, 0);
    midi_buscar(mid, ms);
    reiniciar_anel(ms);
    aviso[0] = 0;
    letra_desenhada = -2;
    registrar("midi: salto a %u ms desde %u ms\n", ms, desde);
}

static void saltar_relativo(int delta_ms)
{
    int p = (int)posicao_ms() + delta_ms;
    saltar(p < 0 ? 0 : (uint32_t)p);
}

static void mudar_volume(int v)
{
    volume = v < 0 ? 0 : v > 100 ? 100 : v;
    if (fluxo >= 0) audio_volume(fluxo, volume);
    desenhar_volume();
}

static bool bombear(void)
{
    if (estado != TOCANDO || fluxo < 0) return false;
    for (int k = 0; k < 2 && !fim_dec && n_dec - n_esc < 8192; k++) {
        static int16_t tmp[PED_QUADROS * 2];
        int n = midi_ler(mid, tmp, PED_QUADROS);
        if (n <= 0) { fim_dec = true; break; }
        uint32_t ini = n_dec % ANEL_Q, a = (uint32_t)n < ANEL_Q - ini ? (uint32_t)n : ANEL_Q - ini;
        memcpy(anel + ini * 2, tmp, a * 4);
        if ((uint32_t)n > a) memcpy(anel, tmp + a * 2, ((uint32_t)n - a) * 4);
        n_dec += (uint32_t)n;
    }
    while (n_esc < n_dec) {
        int pend = audio_pendente(fluxo);
        if (pend >= ADELANTE_MS) break;
        uint32_t ini = n_esc % ANEL_Q, q = n_dec - n_esc;
        if (q > ANEL_Q - ini) q = ANEL_Q - ini;
        int r = audio_escrever_sem_bloquear(fluxo, anel + ini * 2, q * 4);
        if (r <= 0) break;
        n_esc += (uint32_t)r / 4;
        if ((uint32_t)r < q * 4) break;
    }
    if (fim_dec && n_esc == n_dec && audio_pendente(fluxo) <= 0) {
        registrar("midi: fin de la pista %d\n", atual + 1);
        outra_faixa(1, true, false);
        return true;
    }
    return false;
}

/* ------------------------------------------------------------ lista de reproducción */

static void juntar(char *saida, size_t max, const char *pasta, const char *nome)
{
    copia_texto(saida, pasta, max);
    if (compara_texto(pasta, "/") != 0) junta_texto(saida, "/", max);
    junta_texto(saida, nome, max);
}

static bool adicionar_arquivo(const char *c)
{
    if (qtd >= MAX_PISTAS) { copia_texto(aviso, "Lista llena", sizeof aviso); return false; }
    copia_texto(rutas[qtd++], c, sizeof rutas[0]);
    return true;
}

static int adicionar_pasta(const char *c)
{
    static char ns[MAX_PISTAS][64];
    int n = 0;
    entrada_pasta_t e;
    for (int k = 0; listar(c, k, &e) > 0 && n < MAX_PISTAS; k++) {
        if (e.tipo == TIPO_PASTA || !extensao_midi(e.nome)) continue;
        int p = n++;
        while (p > 0 && compara_texto(ns[p - 1], e.nome) > 0) { copia_texto(ns[p], ns[p - 1], 64); p--; }
        copia_texto(ns[p], e.nome, 64);
    }
    int add = 0;
    for (int k = 0; k < n; k++) {
        char r[200];
        juntar(r, sizeof r, c, ns[k]);
        if (adicionar_arquivo(r)) add++;
    }
    if (!add) formatar_texto(aviso, sizeof aviso, "No hay archivos MIDI en %s", c);
    return add;
}

/* Añade un archivo o todos los MIDI de una carpeta. Con `pasta_do_arquivo`, un archivo suelto añade los
 * de su carpeta (la lista de reproducción) y deja elegido el pedido. Devuelve el índice del primero pedido o -1. */
static int adicionar(const char *c, bool pasta_do_arquivo)
{
    info_arquivo_t i;
    if (info_arquivo(c, &i) < 0) {
        formatar_texto(aviso, sizeof aviso, "%s: no existe", c);
        return -1;
    }
    if (i.tipo == TIPO_PASTA) {
        int antes = qtd;
        return adicionar_pasta(c) ? antes : -1;
    }
    if (!pasta_do_arquivo) return adicionar_arquivo(c) ? qtd - 1 : -1;
    char pasta[200];
    copia_texto(pasta, c, sizeof pasta);
    char *b = acha_ultimo_char(pasta, '/');
    if (!b) return adicionar_arquivo(c) ? qtd - 1 : -1;
    if (b == pasta) b[1] = 0; else b[0] = 0;
    int antes = qtd;
    adicionar_pasta(pasta);
    for (int k = antes; k < qtd; k++)
        if (compara_texto(rutas[k], c) == 0) return k;
    return adicionar_arquivo(c) ? qtd - 1 : -1;   /* no está entre los que lista (otra extensión): se añade aparte */
}

static bool preguntar(const char *titulo, char *c, int max)
{
    bool tocaba = estado == TOCANDO;
    if (tocaba) pausar();
    if (c[0] == 0) copia_texto(c, "/", (size_t)max);
    bool ok = pedir_texto(titulo, "Ruta de un archivo MIDI o de una carpeta:", c, max) && c[0];
    if (tocaba && estado == PAUSADO) alternar();
    return ok;
}

static void dir_inicial(char *c, size_t max)
{
    if (atual >= 0) {
        copia_texto(c, rutas[atual], max);
        char *b = acha_ultimo_char(c, '/');
        if (b) b[1] = 0;
    } else copia_texto(c, "/ejemplos/musica", max);
}

static void vaciar(void)
{
    fechar_faixa();
    qtd = 0;
    atual = -1;
    lista.sel = -1;
    lista.topo = 0;
    aviso[0] = 0;
    registrar("midi: lista vaciada\n");
}

static void abrir_novo(bool anadir)
{
    char c[200];
    dir_inicial(c, sizeof c);
    if (!preguntar(anadir ? "Añadir a la lista" : "Abrir", c, sizeof c)) return;
    if (!anadir) vaciar();
    int antes = qtd;
    int primero = adicionar(c, !anadir);
    registrar("midi: %d pistas añadidas desde %s\n", qtd - antes, c);
    if (primero >= 0 && (!anadir || atual < 0)) { lista.sel = primero; abrir_faixa(primero, true); }
    else if (qtd > antes && lista.sel < 0) lista.sel = 0;
    atualizar_titulo();
}

/* ------------------------------------------------------------ eventos */

static bool na_barra(const evento_t *e)
{
    return e->x >= bx - 4 && e->x <= bx + bw + 4 && e->y >= by - 6 && e->y <= by + 18;
}

static bool no_volume(const evento_t *e)
{
    return e->x >= vx - 4 && e->x <= vx + vw + 4 && e->y >= vy - 8 && e->y <= vy + 10;
}

static uint32_t ms_da_barra(int x)
{
    int d = x - bx;
    if (d < 0) d = 0;
    if (d > bw) d = bw;
    uint32_t dur = duracao_ms();
    return (uint32_t)d * (dur / 64) / (uint32_t)bw * 64;
}

static void tecla(unsigned t)
{
    aviso[0] = 0;
    switch (t) {
    case ' ': alternar(); break;
    case 'n': case 'N': outra_faixa(1, estado != PARADO, true); break;
    case 'p': case 'P': outra_faixa(-1, estado != PARADO, true); break;
    case 's': case 'S': parar(); break;
    case TECLA_DIREITA: saltar_relativo(SALTO_MS); break;
    case TECLA_ESQUERDA: saltar_relativo(-SALTO_MS); break;
    case '+': case '=': mudar_volume(volume + 5); return;
    case '-': mudar_volume(volume - 5); return;
    case 15: abrir_novo(false); break;
    default: {
        evento_t e = { .tipo = EV_TECLA, .tecla = t };
        if (lista_tratar(j, &lista, &e) == LISTA_ATIVOU) abrir_faixa(lista.sel, true);
        else return;
    }
    }
    atualizar_titulo();
    desenhar_tudo();
}

static void botao(bool arquivo, int i)
{
    aviso[0] = 0;
    if (arquivo) {
        if (i == 0) abrir_novo(false);
        else if (i == 1) abrir_novo(true);
        else vaciar();
    } else if (i == BT_ANT) outra_faixa(-1, estado != PARADO, true);
    else if (i == BT_SIG) outra_faixa(1, estado != PARADO, true);
    else if (i == BT_PARAR) parar();
    else alternar();
    atualizar_titulo();
    desenhar_tudo();
}

static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_REDIMENSIONAR) {
        if (jan_redimensionar(j, e)) {
            registrar("midi: ventana %dx%d\n", j->larg, j->alt);
            desenhar_tudo();
        }
        return true;
    }
    if (e->tipo == EV_TECLA) { tecla(e->tecla); return true; }
    if (e->tipo == EV_RODA) { mudar_volume(volume - (int)e->cliques * 5); return true; }
    for (int i = 0; i < 3; i++)
        if (botao_tratar(j, &bt_arq[i], e)) { botao(true, i); return true; }
    for (int i = 0; i < 4; i++)
        if (botao_tratar(j, &bt_ctl[i], e)) { botao(false, i); return true; }
    if (e->tipo == EV_CLIQUE && (e->botoes & 1)) {
        if (inf && duracao_ms() && na_barra(e)) {
            arrastando_barra = true;
            previa_ms = ms_da_barra(e->x);
            desenhar_progresso();
            return true;
        }
        if (no_volume(e)) {
            arrastando_vol = true;
            mudar_volume((e->x - vx) * 100 / vw);
            return true;
        }
    }
    if (e->tipo == EV_MOVER && (e->botoes & 1)) {
        if (arrastando_barra) { previa_ms = ms_da_barra(e->x); desenhar_progresso(); return true; }
        if (arrastando_vol) { mudar_volume((e->x - vx) * 100 / vw); return true; }
    }
    if (e->tipo == EV_SOLTOU) {
        if (arrastando_barra) {
            arrastando_barra = false;
            saltar(ms_da_barra(e->x));
            desenhar_progresso();
            return true;
        }
        if (arrastando_vol) { arrastando_vol = false; return true; }
    }
    int r = lista_tratar(j, &lista, e);
    if (r == LISTA_ATIVOU) {
        abrir_faixa(lista.sel, true);
        atualizar_titulo();
        desenhar_tudo();
    }
    return true;
}

/* ------------------------------------------------------------ órdenes de terminal */

static void le32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24); }

/* --a-wav: sintetiza a un WAV PCM de 16 bits estéreo. */
static int para_wav(const char *entrada, const char *saida)
{
    int erro;
    midi_t *m = midi_abrir(entrada, &erro);
    if (!m) { imprimir_em(ERROS, "midi: %s: %s\n", entrada, erro == AUDIO_ERRO_FORMATO ? "no es un archivo MIDI" : audio_erro_texto(erro)); return 1; }
    int fd = abrir(saida, ABRIR_ESCREVER | ABRIR_CRIAR | ABRIR_TRUNCAR);
    if (fd < 0) { imprimir_em(ERROS, "midi: %s: %s\n", saida, nome_erro(fd)); midi_fechar(m); return 1; }
    uint8_t cab[44] = "RIFF\0\0\0\0WAVEfmt \x10\0\0\0\x01\0\x02\0\0\0\0\0\0\0\0\0\x04\0\x10\0data\0\0\0\0";
    le32(cab + 24, MIDI_TAXA);
    le32(cab + 28, MIDI_TAXA * 4);
    escrever(fd, cab, 44);
    static int16_t buf[PED_QUADROS * 2];
    uint32_t total = 0;
    int n;
    while ((n = midi_ler(m, buf, PED_QUADROS)) > 0) {
        if (escrever(fd, buf, (unsigned)n * 4) < 0) { n = -1; break; }
        total += (uint32_t)n;
    }
    le32(cab + 4, 36 + total * 4);
    le32(cab + 40, total * 4);
    posicionar(fd, 0, POS_INICIO);
    escrever(fd, cab, 44);
    fechar(fd);
    imprimir("%s: %u cuadros (%u ms) -> %s\n", entrada, total, quadros_ms(total), saida);
    midi_fechar(m);
    return n < 0 ? 1 : 0;
}

/* --medir: tiempo de CPU de la síntesis contra la duración. */
static int medir(const char *entrada)
{
    int erro;
    midi_t *m = midi_abrir(entrada, &erro);
    if (!m) { imprimir_em(ERROS, "midi: %s: %s\n", entrada, audio_erro_texto(erro)); return 1; }
    static int16_t buf[PED_QUADROS * 2];
    unsigned t0 = tempo_ms();
    uint32_t total = 0;
    int n;
    while ((n = midi_ler(m, buf, PED_QUADROS)) > 0) total += (uint32_t)n;
    unsigned dt = tempo_ms() - t0;
    uint32_t ms = quadros_ms(total);
    imprimir("%s: %u ms de audio sintetizados en %u ms (x%u tiempo real, %u%% de una CPU)\n", nome_curto(entrada), ms, dt,
             dt ? ms / dt : 0, ms ? dt * 100 / ms : 0);
    midi_fechar(m);
    return 0;
}

static int info(const char *entrada)
{
    int erro;
    midi_t *m = midi_abrir(entrada, &erro);
    if (!m) { imprimir_em(ERROS, "midi: %s: %s\n", entrada, audio_erro_texto(erro)); return 1; }
    const midi_info_t *i = midi_info(m);
    imprimir("%s: formato %d, %d pistas, %u tics por negra, %u ms, %u notas, tempo %u us/negra, %u letras%s\n", nome_curto(entrada), i->formato,
             i->trilhas, i->divisao, i->duracao_ms, i->notas, i->tempo_ini, i->letras, i->karaoke ? " (karaoke)" : "");
    imprimir("titulo: %s\ncopyright: %s\nautor: %s\n", i->titulo, i->copyright, i->autor);
    midi_fechar(m);
    return 0;
}

int principal(int argc, char **argv)
{
    if (argc >= 4 && compara_texto(argv[1], "--a-wav") == 0) return para_wav(argv[2], argv[3]);
    if (argc >= 3 && compara_texto(argv[1], "--medir") == 0) { int r = 0; for (int i = 2; i < argc; i++) r |= medir(argv[i]); return r; }
    if (argc >= 3 && compara_texto(argv[1], "--info") == 0) return info(argv[2]);
    j = jan_abrir("MIDI", LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    jan_estilo(j, JAN_REDIMENSIONAVEL, LARG_MIN, ALT_MIN);
    lista.sel = -1;
    lista.icone = NULL;
    int primero = -1;
    for (int i = 1; i < argc; i++) {
        int k = adicionar(argv[i], argc == 2);
        if (primero < 0) primero = k;
    }
    if (qtd) {
        if (primero < 0) primero = 0;
        lista.sel = primero;
        abrir_faixa(primero, true);
    }
    ler_estado();
    atualizar_titulo();
    desenhar_tudo();
    registrar("midi: pronto (%d pistas)\n", qtd);

    evento_t e;
    bool continuar = true;
    unsigned ultimo = 0;
    while (continuar) {
        if (esperar_evento(j->id, &e, estado == TOCANDO ? 15 : -1) == 1) {
            continuar = tratar(&e);
            while (continuar && proximo_evento(j->id, &e, false) == 1) continuar = tratar(&e);
        }
        if (!continuar) break;
        if (bombear()) { atualizar_titulo(); desenhar_tudo(); }
        else if (estado == TOCANDO && !arrastando_barra) {
            unsigned agora = tempo_ms();
            if (agora - ultimo >= 33) {   /* ~30 imágenes por segundo */
                ultimo = agora;
                ler_estado();
                atualizar_canais(false);
                int idx = mid ? midi_letra_indice(mid, posicao_ms()) : -1;
                if (idx != letra_desenhada && mid && midi_letra_qtd(mid)) desenhar_letra(idx);
                uint32_t pos = posicao_ms(), dur = duracao_ms();
                int f = dur ? (int)((pos / 64) * (uint32_t)bw / (dur / 64 | 1)) : 0;
                if (f != px_mostrado) desenhar_progresso();
            }
        }
    }
    registrar("midi: cerrado\n");
    fechar_faixa();
    jan_fechar(j);
    return 0;
}
