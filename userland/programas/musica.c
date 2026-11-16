// SPDX-License-Identifier: GPL-2.0-or-later
/* musica [archivo | carpeta]... - el Reproductor de música de SaguiOS (D115, mejora 041/053).
 *
 * Reproduce WAV (PCM e IMA ADPCM), MP3, FLAC y MIDI (sintetizado, D190; el programa `midi` añade la vista de canales) con los decodificadores de la libsagui
 * (audio_formato.h). Ogg Vorbis todavía no existe: el programa lo dice. Una lista de
 * reproducción (archivos sueltos o una carpeta entera), anterior/siguiente, reproducir/pausa/
 * parar, barra de progreso con el tiempo (un clic o arrastrar salta: exacto en WAV/ADPCM, al
 * cuadro más cercano en FLAC y MP3) y volumen. Muestra el formato, la frecuencia, los canales,
 * los bits y el título, artista y álbum de las etiquetas.
 *
 * Teclas: espacio = reproducir/pausa, flechas izquierda/derecha = -/+ 5 s, N = siguiente,
 * P = anterior, S = parar, +/- = volumen, Ctrl+O = abrir, Intro = tocar la pista elegida.
 *
 * No bloquea la interfaz: el bucle de eventos espera como mucho 15 ms y, entre evento y
 * evento, decodifica por bloques de 1152 cuadros a un anillo y lo entrega al flujo de audio sin
 * bloquear (audio_escrever_sem_bloquear). Solo se deja ~0,4 s por delante en el flujo del kernel:
 * así la pausa y el salto cortan el sonido casi al instante. La posición que se enseña es
 *     (posición del decodificador al empezar el anillo) + cuadros entregados - lo que aún no ha sonado,
 * y el anillo conserva lo ya entregado: al pausar se descarta lo pendiente del flujo y se
 * vuelve a entregar desde el anillo al seguir, sin saltos ni nuevo salto en el archivo.
 * Sin tarjeta de sonido avisa y no se cuelga. Las cajas de diálogo (Abrir) pausan el sonido. */
#include "sagui.h"
#include "gui.h"
#include "audio_formato.h"
#include "instancia_unica.h"

#define LARG_INICIO 560
#define ALT_INICIO  440
#define LARG_MIN 460
#define ALT_MIN  340
#define MARGEM 12
#define RODAPE 22
#define MAX_PISTAS 256
#define PED_QUADROS 1152
#define ANEL_Q 65536u          /* cuadros del anillo (potencia de 2) */
#define ADELANTE_MS 400        /* lo que se deja por delante en el flujo del kernel */
#define SALTO_MS 5000

enum { PARADO, TOCANDO, PAUSADO };

static jan_t *j;
static char rutas[MAX_PISTAS][200];
static char nomes[MAX_PISTAS][64];
static int qtd, atual = -1;
static lista_t lista;

static audio_arquivo_t *arq;
static const audio_info_t *inf;
static int estado = PARADO;
static int fluxo = -1;
static int volume = 100;
static char aviso[160];

static int16_t anel[ANEL_Q * 2];
static uint32_t n_dec, n_esc;   /* cuadros decodificados y entregados al flujo, desde base_q */
static uint32_t base_q;         /* cuadro del archivo en que empieza la cuenta */
static bool fim_dec;

static bool arrastando_barra, arrastando_vol;
static uint32_t previa_ms;      /* instante bajo el cursor al arrastrar la barra */
static int bx, by, bw;          /* barra de progreso */
static int vx, vy, vw;          /* barra de volumen */
static uint32_t seg_mostrado = 0xFFFFFFFFu;

static botao_t bt_arq[] = {
    { 12, 8, 64, 24, "Abrir", false },
    { 84, 8, 72, 24, "Añadir", false },
    { 164, 8, 64, 24, "Vaciar", false },
};
static botao_t bt_ctl[] = {
    { 12, 150, 36, 26, "|<", false },
    { 56, 150, 100, 26, N_("Reproducir"), false },
    { 164, 150, 64, 26, N_("Parar"), false },
    { 236, 150, 36, 26, ">|", false },
};
#define BT_ANT 0
#define BT_PLAY 1
#define BT_PARAR 2
#define BT_SIG 3

/* ------------------------------------------------------------ utilidades */

static const char *nome_curto(const char *c)
{
    const char *b = acha_ultimo_char(c, '/');
    return b ? b + 1 : c;
}

static uint32_t quadros_ms(uint32_t q, uint32_t taxa)
{
    return taxa ? q / taxa * 1000 + q % taxa * 1000 / taxa : 0;
}

static uint32_t duracao_ms(void) { return inf ? inf->duracao_ms : 0; }

/* Cuadros que el flujo todavía no ha sonado (lo ya entregado). */
static uint32_t pendentes_q(void)
{
    if (fluxo < 0 || !inf) return 0;
    int ms = audio_pendente(fluxo);
    if (ms <= 0) return 0;
    uint32_t q = (uint32_t)ms * (inf->taxa / 100) / 10;
    return q > n_esc ? n_esc : q;
}

static uint32_t posicao_q(void)
{
    return base_q + n_esc - (estado == TOCANDO ? pendentes_q() : 0);
}

static uint32_t posicao_ms(void)
{
    if (!inf) return 0;
    uint32_t ms = quadros_ms(posicao_q(), inf->taxa), d = duracao_ms();
    return d && ms > d ? d : ms;
}

static void formatar_tempo(char *s, size_t max, uint32_t ms)
{
    uint32_t t = ms / 1000;
    if (t >= 3600) formatar_texto(s, max, "%u:%02u:%02u", t / 3600, t / 60 % 60, t % 60);
    else formatar_texto(s, max, "%u:%02u", t / 60, t % 60);
}

static bool extensao_audio(const char *c)
{
    unsigned n = (unsigned)tam_texto(c), p = n;
    while (p && c[p - 1] != '.' && c[p - 1] != '/') p--;
    if (!p || c[p - 1] != '.') return false;
    char e[5] = { 0 };
    for (unsigned k = 0; k < 4 && p + k < n; k++) e[k] = (char)(c[p + k] | 0x20);
    return compara_texto(e, "wav") == 0 || compara_texto(e, "mp3") == 0 || compara_texto(e, "flac") == 0 ||
           compara_texto(e, "ogg") == 0 || compara_texto(e, "mid") == 0 || compara_texto(e, "midi") == 0 || compara_texto(e, "kar") == 0;
}

static bool extensao_ogg(const char *c)
{
    size_t n = tam_texto(c);
    return n > 4 && c[n - 4] == '.' && (c[n - 3] | 0x20) == 'o' && (c[n - 2] | 0x20) == 'g' && (c[n - 1] | 0x20) == 'g';
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
    bt_ctl[BT_PLAY].texto = estado == TOCANDO ? _("Pausa") : _("Reproducir");
    vw = 100;
    vx = l - MARGEM - vw - 40;
    vy = 158;
    lista.x = MARGEM;
    lista.y = 190;
    lista.l = l - 2 * MARGEM;
    lista.a = j->alt - RODAPE - lista.y - 6;
    if (lista.a < 40) lista.a = 40;
}

static void texto_linha(int y, const char *t, uint32_t cor)
{
    g_texto_n(j, MARGEM, y, t, (j->larg - 2 * MARGEM) / LETRA_L, cor);
}

static void desenhar_progresso(void)
{
    uint32_t pos = arrastando_barra ? previa_ms : posicao_ms(), dur = duracao_ms();
    seg_mostrado = pos / 1000;
    g_retangulo(j, 0, 100, j->larg, 44, COR_FUNDO);
    char a[24], b[24], t[64];
    formatar_tempo(a, sizeof a, pos);
    if (dur) formatar_tempo(b, sizeof b, dur); else copia_texto(b, "--:--", sizeof b);
    formatar_texto(t, sizeof t, "%s / %s", a, b);
    g_texto(j, bx, 102, t, COR_TEXTO);
    g_retangulo(j, bx, by, bw, 12, COR_BOTAO_APERTADO);
    g_moldura(j, bx - 1, by - 1, bw + 2, 14, COR_BORDA);
    if (dur && inf) {
        int f = (int)((pos / 64) * (uint32_t)bw / (dur / 64 | 1));
        if (f > bw) f = bw;
        if (f > 0) g_retangulo(j, bx, by, f, 12, COR_DESTAQUE);
        g_retangulo(j, bx + f - 1 < bx ? bx : bx + f - 1, by - 3, 3, 18, COR_TEXTO);
    }
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
        texto_linha(44, "Ninguna pista abierta", COR_TEXTO);
        texto_linha(62, "Pulsa Abrir o Añadir (Ctrl+O), o usa en el terminal: musica archivo.mp3", COR_TEXTO_FRACO);
    } else {
        char t[200];
        texto_linha(44, inf->titulo[0] ? inf->titulo : nome_curto(rutas[atual]), COR_TEXTO);
        if (inf->artista[0] && inf->album[0]) formatar_texto(t, sizeof t, "%s - %s", inf->artista, inf->album);
        else if (inf->artista[0]) copia_texto(t, inf->artista, sizeof t);
        else if (inf->album[0]) copia_texto(t, inf->album, sizeof t);
        else t[0] = 0;
        texto_linha(62, t, COR_TEXTO_FRACO);
        int n = formatar_texto(t, sizeof t, "%s, %u Hz, %s", inf->nome_formato, inf->taxa, inf->canais == 2 ? _("estéreo") : "mono");
        if (inf->bits_origem > 4 && n < (int)sizeof t) n += formatar_texto(t + n, sizeof t - (size_t)n, ", %u bits", inf->bits_origem);
        if (inf->kbps && n < (int)sizeof t) formatar_texto(t + n, sizeof t - (size_t)n, ", %u kbps%s", inf->kbps, inf->vbr ? _(" (VBR)") : "");
        texto_linha(80, t, COR_TEXTO);
    }
    jan_mostrar_area(j, 0, 38, j->larg, 62);
}

static void desenhar_rodape(void)
{
    int y = j->alt - RODAPE;
    g_retangulo(j, 0, y, j->larg, RODAPE, 0xE6E8EA);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);
    char t[200];
    if (aviso[0]) copia_texto(t, aviso, sizeof t);
    else if (qtd == 0) copia_texto(t, _("Lista vacía"), sizeof t);
    else formatar_texto(t, sizeof t, _("%s  |  pista %d de %d"), estado == TOCANDO ? _("Reproduciendo") : estado == PAUSADO ? _("En pausa") : _("Detenido"),
                        atual + 1, qtd);
    g_texto_n(j, 8, y + 3, t, (j->larg - 16) / LETRA_L, aviso[0] ? 0x9A1B1B : COR_TEXTO);
    jan_mostrar_area(j, 0, y, j->larg, RODAPE);
}

static void atualizar_titulo(void)
{
    char t[120];
    if (inf && atual >= 0) formatar_texto(t, sizeof t, _("%s - Música"), inf->titulo[0] ? inf->titulo : nome_curto(rutas[atual]));
    else copia_texto(t, _("Música"), sizeof t);
    titulo_janela(j->id, t);
}

static void desenhar_tudo(void)
{
    layout();
    g_retangulo(j, 0, 0, j->larg, j->alt, COR_FUNDO);
    g_retangulo(j, 0, 36, j->larg, 1, COR_BORDA);
    for (int i = 0; i < 3; i++) botao_desenhar(j, &bt_arq[i]);
    desenhar_info();
    desenhar_progresso();
    for (int i = 0; i < 4; i++) botao_desenhar(j, &bt_ctl[i]);
    desenhar_volume();
    atualizar_nomes();
    if (qtd) lista_desenhar(j, &lista);
    else {
        g_retangulo(j, lista.x, lista.y, lista.l, lista.a, COR_BRANCO);
        g_moldura(j, lista.x, lista.y, lista.l, lista.a, COR_BORDA);
        g_texto(j, lista.x + 8, lista.y + 8, _("La lista está vacía."), COR_TEXTO_FRACO);
    }
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
    if (arq) { audio_arquivo_fechar(arq); arq = NULL; inf = NULL; }
    estado = PARADO;
    n_dec = n_esc = base_q = 0;
    fim_dec = false;
}

static void reiniciar_anel(void)
{
    base_q = audio_arquivo_posicao(arq);
    n_dec = n_esc = 0;
    fim_dec = false;
}

static void iniciar(void)
{
    if (!arq) return;
    fluxo = audio_abrir((int)inf->taxa, (int)inf->canais, 16);
    if (fluxo < 0) {
        if (fluxo == ERRO_NAO_EXISTE) copia_texto(aviso, _("No hay tarjeta de sonido (solo el altavoz del PC): no se puede reproducir"), sizeof aviso);
        else formatar_texto(aviso, sizeof aviso, _("No se puede abrir el sonido: %s"), nome_erro(fluxo));
        registrar("musica: sin sonido (%d)\n", fluxo);
        fluxo = -1;
        estado = PARADO;
        return;
    }
    audio_volume(fluxo, volume);
    estado = TOCANDO;
    aviso[0] = 0;
    registrar("musica: reproduciendo %s\n", nome_curto(rutas[atual]));
}

static bool abrir_faixa(int i, bool tocar)
{
    fechar_faixa();
    atual = i;
    lista.sel = i;
    int erro;
    arq = audio_arquivo_abrir(rutas[i], &erro);
    if (!arq) {
        if (erro == AUDIO_ERRO_FORMATO && extensao_ogg(rutas[i]))
            formatar_texto(aviso, sizeof aviso, _("%s: Ogg Vorbis todavía no está soportado"), nome_curto(rutas[i]));
        else formatar_texto(aviso, sizeof aviso, "%s: %s", nome_curto(rutas[i]), audio_erro_texto(erro));
        registrar("musica: error al abrir %s (%d)\n", rutas[i], erro);
        return false;
    }
    inf = audio_arquivo_info(arq);
    aviso[0] = 0;
    reiniciar_anel();
    registrar("musica: pista %d %s: %s %u Hz %u canales %u ms titulo '%s'\n", i + 1, nome_curto(rutas[i]), inf->nome_formato, inf->taxa,
              inf->canais, inf->duracao_ms, inf->titulo);
    if (tocar) iniciar();
    return true;
}

/* Siguiente pista que se pueda abrir (con vuelta al principio solo al pulsar). */
static void outra_faixa(int passo, bool tocar, bool dar_volta)
{
    if (!qtd) return;
    bool tocando = tocar;
    int i = atual;
    for (int tent = 0; tent < qtd; tent++) {
        i += passo;
        if (i < 0 || i >= qtd) {
            if (!dar_volta) {   /* fin de la lista: se queda detenido en la primera */
                fechar_faixa();
                abrir_faixa(0, false);
                registrar("musica: fin de la lista\n");
                return;
            }
            i = i < 0 ? qtd - 1 : 0;
        }
        if (abrir_faixa(i, tocando)) return;   /* sin tarjeta: abre la pista pero no salta a la siguiente */
    }
}

static void parar(void)
{
    if (!arq) return;
    uint32_t pos = posicao_ms();
    fechar_fluxo();
    audio_arquivo_buscar(arq, 0);
    reiniciar_anel();
    estado = PARADO;
    registrar("musica: parado en %u ms\n", pos);
}

static void pausar(void)
{
    if (estado != TOCANDO) return;
    uint32_t pend = pendentes_q(), lo = n_dec > ANEL_Q ? n_dec - ANEL_Q : 0;
    n_esc = n_esc - pend < lo ? lo : n_esc - pend;
    audio_controle(fluxo, AUDIO_DESCARTAR, 0);
    estado = PAUSADO;
    registrar("musica: pausa en %u ms\n", posicao_ms());
}

static void alternar(void)
{
    if (estado == TOCANDO) { pausar(); return; }
    if (estado == PAUSADO) { estado = TOCANDO; registrar("musica: sigue en %u ms\n", posicao_ms()); return; }
    if (!arq && qtd) abrir_faixa(atual >= 0 ? atual : 0, true);
    else if (arq) iniciar();
}

static void saltar(uint32_t ms)
{
    if (!arq) return;
    uint32_t d = duracao_ms();
    if (d && ms > d) ms = d;
    uint32_t desde = posicao_ms();
    if (fluxo >= 0) audio_controle(fluxo, AUDIO_DESCARTAR, 0);
    int r = audio_arquivo_buscar(arq, ms);
    if (r < 0) { formatar_texto(aviso, sizeof aviso, _("No se puede saltar: %s"), audio_erro_texto(r)); return; }
    reiniciar_anel();
    aviso[0] = 0;
    registrar("musica: salto a %u ms (cuadro %u, %u ms) desde %u ms\n", ms, base_q, quadros_ms(base_q, inf->taxa), desde);
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

/* Decodifica y entrega un poco. Se llama entre evento y evento. Devuelve true si cambió algo que pida redibujar. */
static bool bombear(void)
{
    if (estado != TOCANDO || fluxo < 0) return false;
    uint32_t canais = inf->canais, bq = canais * 2;
    for (int k = 0; k < 2 && !fim_dec && n_dec - n_esc < 8192; k++) {
        static int16_t tmp[PED_QUADROS * 2];
        int n = audio_arquivo_ler(arq, tmp, PED_QUADROS);
        if (n <= 0) {
            fim_dec = true;
            if (n < 0) {
                formatar_texto(aviso, sizeof aviso, "%s: %s", nome_curto(rutas[atual]), audio_erro_texto(n));
                registrar("musica: error al decodificar (%d)\n", n);
            }
            break;
        }
        uint32_t ini = n_dec % ANEL_Q, a = (uint32_t)n < ANEL_Q - ini ? (uint32_t)n : ANEL_Q - ini;
        memcpy(anel + ini * canais, tmp, a * bq);
        if ((uint32_t)n > a) memcpy(anel, tmp + a * canais, ((uint32_t)n - a) * bq);
        n_dec += (uint32_t)n;
    }
    while (n_esc < n_dec) {
        int pend = audio_pendente(fluxo);
        if (pend >= ADELANTE_MS) break;
        uint32_t ini = n_esc % ANEL_Q, q = n_dec - n_esc;
        if (q > ANEL_Q - ini) q = ANEL_Q - ini;
        int r = audio_escrever_sem_bloquear(fluxo, anel + ini * canais, q * bq);
        if (r <= 0) break;
        n_esc += (uint32_t)r / bq;
        if ((uint32_t)r < q * bq) break;
    }
    if (fim_dec && n_esc == n_dec && audio_pendente(fluxo) <= 0) {   /* la pista terminó: la siguiente */
        registrar("musica: fin de la pista %d\n", atual + 1);
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
    if (qtd >= MAX_PISTAS) { copia_texto(aviso, _("Lista llena"), sizeof aviso); return false; }
    copia_texto(rutas[qtd++], c, sizeof rutas[0]);
    return true;
}

/* Añade un archivo o todos los de sonido de una carpeta (en orden alfabético). Devuelve las pistas añadidas. */
static int adicionar(const char *c)
{
    info_arquivo_t i;
    if (info_arquivo(c, &i) < 0) {
        formatar_texto(aviso, sizeof aviso, _("%s: no existe"), c);
        return 0;
    }
    if (i.tipo != TIPO_PASTA) return adicionar_arquivo(c) ? 1 : 0;
    static char ns[MAX_PISTAS][64];
    int n = 0;
    entrada_pasta_t e;
    for (int k = 0; listar(c, k, &e) > 0 && n < MAX_PISTAS; k++) {
        if (e.tipo == TIPO_PASTA || !extensao_audio(e.nome)) continue;
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
    if (!add) formatar_texto(aviso, sizeof aviso, _("No hay archivos de sonido en %s"), c);
    return add;
}

/* Pregunta una ruta (con el sonido en pausa mientras el diálogo está abierto). */
/* Archivo de sonido (diálogo de abrir, D272) o carpeta entera (diálogo de carpeta, según la respuesta). */
#define FILTROS_SONIDO "Sonido (*.wav;*.mp3;*.flac;*.ogg;*.mid;*.midi;*.kar)|*.wav;*.mp3;*.flac;*.ogg;*.mid;*.midi;*.kar|Todos los archivos (*.*)|*"

static bool preguntar(const char *titulo, char *c, int max)
{
    bool tocaba = estado == TOCANDO;
    if (tocaba) pausar();
    if (c[0] == 0) copia_texto(c, "/", (size_t)max);
    bool carpeta = confirmar(titulo, _("¿Añadir una carpeta entera?\nSí: elegir una carpeta.  No: elegir un archivo de sonido."));
    bool ok = (carpeta ? dialogo_carpeta(titulo, c, c, max) : dialogo_abrir(titulo, FILTROS_SONIDO, c, c, max)) && c[0];
    if (tocaba && estado == PAUSADO) alternar();
    return ok;
}

static void dir_inicial(char *c, size_t max)
{
    if (atual >= 0) {
        copia_texto(c, rutas[atual], max);
        char *b = acha_ultimo_char(c, '/');
        if (b) b[1] = 0;
    } else c[0] = 0;
}

static void vaciar(void)
{
    fechar_faixa();
    qtd = 0;
    atual = -1;
    lista.sel = -1;
    lista.topo = 0;
    aviso[0] = 0;
    registrar("musica: lista vaciada\n");
}

static void abrir_novo(bool anadir)
{
    char c[200];
    dir_inicial(c, sizeof c);
    if (!preguntar(anadir ? "Añadir a la lista" : "Abrir", c, sizeof c)) return;
    if (!anadir) vaciar();
    int antes = qtd, n = adicionar(c);
    registrar("musica: %d pistas añadidas desde %s\n", n, c);
    if (n && (!anadir || atual < 0)) { lista.sel = antes; abrir_faixa(antes, true); }
    else if (n && lista.sel < 0) lista.sel = 0;
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

/* D339: rutas que otro proceso nos entrega (doble clic en Archivos con el reproductor ya abierto): se añaden a la
 * lista (o se reutiliza la que ya está) y suena la primera; lo que sonaba se para antes en abrir_faixa. */
static bool recoger_pedidos(void)
{
    static char buf[2048];
    int n = iu_recoger("musica", buf, sizeof buf);
    if (n <= 0) return false;
    int primeira = -1;
    const char *r = buf;
    for (int k = 0; k < n; k++, r += tam_texto(r) + 1) {
        int ya = -1;
        for (int i = 0; i < qtd; i++) if (compara_texto(rutas[i], r) == 0) { ya = i; break; }
        int antes = qtd;
        if (ya < 0) adicionar(r);
        if (primeira < 0) primeira = ya >= 0 ? ya : (qtd > antes ? antes : -1);
    }
    registrar("musica: pedido de otro proceso (%d rutas), pista %d\n", n, primeira + 1);
    if (primeira >= 0) abrir_faixa(primeira, true);
    return true;
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
    case 15: abrir_novo(false); break;   /* Ctrl+O */
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

/* Devuelve false para cerrar. */
static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_REDIMENSIONAR) {
        if (jan_redimensionar(j, e)) {
            registrar("musica: ventana %dx%d\n", j->larg, j->alt);
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

int principal(int argc, char **argv)
{
    if (iu_entregar("musica", argc - 1, argv + 1)) {   /* D339: ya hay un reproductor sonando: se lo pasa y no abre otro flujo */
        registrar("musica: entregado a la instancia viva\n");
        return 0;
    }
    j = jan_abrir(_("Música"), LARG_INICIO, ALT_INICIO);
    if (!j) return 1;
    iu_registrar("musica");
    jan_estilo(j, JAN_REDIMENSIONAVEL, LARG_MIN, ALT_MIN);
    lista.sel = -1;
    lista.icone = NULL;
    for (int i = 1; i < argc; i++) adicionar(argv[i]);
    if (qtd) {
        lista.sel = 0;
        abrir_faixa(0, true);
    }
    atualizar_titulo();
    desenhar_tudo();
    registrar("musica: pronto (%d pistas)\n", qtd);

    evento_t e;
    bool continuar = true;
    while (continuar) {
        if (esperar_evento(j->id, &e, estado == TOCANDO ? 15 : IU_PERIODO_MS) == 1) {
            continuar = tratar(&e);
            while (continuar && proximo_evento(j->id, &e, false) == 1) continuar = tratar(&e);
        }
        if (!continuar) break;
        if (recoger_pedidos()) { atualizar_titulo(); desenhar_tudo(); }
        if (bombear()) { atualizar_titulo(); desenhar_tudo(); }
        else if (estado == TOCANDO && !arrastando_barra && posicao_ms() / 1000 != seg_mostrado) desenhar_progresso();
    }
    registrar("musica: cerrado\n");
    iu_liberar("musica");
    fechar_faixa();
    jan_fechar(j);
    return 0;
}
