// SPDX-License-Identifier: GPL-2.0-or-later
/* reproductor [archivo] - el Reproductor de vídeo de SaguiOS (mejora 048, D129).
 *
 * Abre un vídeo y lo reproduce en una ventana: MJPEG dentro de un AVI (cada
 * cuadro es un JPEG, que decodifica la libsagui), MPEG-1 en MPEG-PS y MP4 con
 * H.264 (Baseline y Main) y AAC-LC (mejora 100, D276: el decodificador es el
 * mismo que usa el Cipó para el <video>, vive en la libsagui).
 *
 * Controles:
 *   Espacio      pausa y seguir
 *   <- ->        adelante y atrás 5 segundos
 *   , .          un cuadro atrás o adelante (con la pausa puesta)
 *   Inicio Fin   el principio y el final
 *   F11          pantalla completa
 *   L            bucle
 *   + -          volumen
 *   M            silencio
 *   I            estadísticas (cuadros por segundo, saltos y tiempo de decodificar)
 *   Ctrl+O       abrir otro vídeo
 *
 * En la ventana hay una barra de botones (ir al principio, +-5 s, pausa, el
 * volumen, el bucle y la pantalla completa) y abajo una barra de posición con
 * el tiempo; al pulsar en la barra de posición se salta a ese punto.
 *
 * El sonido: si el vídeo trae audio y hay tarjeta de sonido, el audio es el
 * reloj maestro (el cuadro se muestra cuando le toca al sonido) y el vídeo va
 * en silencio si no hay tarjeta. Si la CPU no llega, se saltan cuadros (para
 * que el sonido no se corte) en vez de ir a cámara lenta.
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

#define LARG_INICIO 800
#define ALT_INICIO  600
#define LARG_MIN 480
#define ALT_MIN  320
#define BARRA  34
#define RODAPE 46
#define PCM_CUADROS 4096          /* muestras de PCM que se llevan de golpe */
#define SALTO_MS 5000             /* lo que saltan las flechas */

/* ------------------------------------------------------------ estado */

static jan_t *j;
static video_t *vid;
static imagem_t img;
static bool tem_quadro;
static bool primeiro_desenhado;   /* ya se enseñó un cuadro de verdad */
static bool tocando = true;
static bool fim;              /* el vídeo se acabo: hay que volver a empezar */
static bool bucle;
static bool mudo;
static int  volume = 80;
static bool tela_cheia;
static int  audio_fd = -1;
static uint32_t taxa_audio, canais_audio;
static uint32_t ms_escritos;     /* ms de audio ya escritos en la tarjeta */
static unsigned t_inicio;        /* tempo_ms() cuando empezó la reproducción */
static int  quadro_atual = -1;   /* el cuadro que se está viendo */
static int  quadros_vistos, quadros_saltados;
static uint32_t ms_decodificar;  /* ms gastados en decodificar */
static char aviso[140];
static progresso_t *barra;
static int  fps = 30, quadros_total;
static char caminho_video[200];

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
    formatar_texto(s, max, "%d:%02d", s_ / 60, s_ % 60);
}

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

static unsigned periodo_ms(void) { return fps > 0 ? 1000u / (unsigned)fps : 33u; }

/* ------------------------------------------------------------ dibujo */

static int area_x(void) { return 0; }
static int area_y(void) { return tela_cheia ? 0 : BARRA; }
static int area_l(void) { return j->larg; }
static int area_a(void) { int a = j->alt - (tela_cheia ? 0 : BARRA + RODAPE); return a > 1 ? a : 1; }

static botao_t bt[] = {
    {   8, 5,  44, 24, "|<",       false },
    {  56, 5,  44, 24, "<",        false },
    { 104, 5,  44, 24, ">",        false },
    { 152, 5,  60, 24, "pausa",    false },
    { 216, 5,  56, 24, "vol -",    false },
    { 276, 5,  56, 24, "vol +",    false },
    { 336, 5,  56, 24, "bucle",    false },
    { 396, 5, 100, 24, "tela cheia", false },
};
#define QTD_BT (int)(sizeof bt / sizeof bt[0])

static void desenhar_barra(void)
{
    if (tela_cheia) return;
    g_retangulo(j, 0, 0, j->larg, BARRA, COR_FUNDO);
    g_retangulo(j, 0, BARRA - 1, j->larg, 1, COR_BORDA);
    for (int i = 0; i < QTD_BT; i++) botao_desenhar(j, &bt[i]);
    /* D300: la ayuda se elige según lo que cabe (con 800 px la larga se cortaba por el borde de la ventana) */
    const char *ayuda = j->larg >= 506 + 40 * LETRA_L + 4 ? "Espacio: pausa   F11: pantalla completa"
                      : j->larg >= 506 + 29 * LETRA_L + 4 ? "Espacio: pausa  F11: pantalla" : NULL;
    if (ayuda) g_texto(j, 506, 9, ayuda, COR_TEXTO_FRACO);
    jan_mostrar_area(j, 0, 0, j->larg, BARRA);
}

static void desenhar_rodape(void)
{
    if (tela_cheia) return;
    const video_info_t *info = vid ? video_info(vid) : NULL;
    int y = j->alt - RODAPE;
    g_retangulo(j, 0, y, j->larg, RODAPE, 0xE6E8EA);
    g_retangulo(j, 0, y, j->larg, 1, COR_BORDA);
    char t[200], a[16], b[16];
    formatar_tempo(a, sizeof a, quadro_atual < 0 ? 0 : quadro_atual * (int)periodo_ms());
    formatar_tempo(b, sizeof b, quadros_total * (int)periodo_ms());
    if (aviso[0]) copia_texto(t, aviso, sizeof t);
    else formatar_texto(t, sizeof t, "%s  |  %d x %d  |  %d fps  |  %s / %s  |  cuadro %d de %d%s",
                        info ? info->nome_formato : "", info ? info->larg : 0, info ? info->alt : 0, fps, a, b,
                        quadro_atual + 1, quadros_total, bucle ? "  |  bucle" : "");
    g_texto_n(j, 8, y + 4, t, (j->larg - 16) / LETRA_L, COR_TEXTO);
    if (barra) {
        int valor = quadros_total > 0 && quadro_atual >= 0 ? quadro_atual * 100 / quadros_total : 0;
        progresso_definir(barra, valor);
        progresso_desenhar(j, barra);
    }
    jan_mostrar_area(j, 0, y, j->larg, RODAPE);
}

static void desenhar_quadro(void)
{
    int ax = area_x(), ay = area_y(), al = area_l(), aa = area_a();
    g_retangulo(j, ax, ay, al, aa, 0);
    if (!tem_quadro || !img.px) {
        g_texto(j, ax + 16, ay + 10, "Sin cuadro", COR_TEXTO);
        jan_mostrar_area(j, ax, ay, al, aa);
        return;
    }
    /* Se ajusta a la ventana guardando la proporción (como el visor). */
    unsigned escala_l = (unsigned)al * 65536u / (unsigned)img.larg;
    unsigned escala_a = (unsigned)aa * 65536u / (unsigned)img.alt;
    unsigned escala = escala_l < escala_a ? escala_l : escala_a;
    int l = (int)((unsigned)img.larg * escala / 65536u);
    int a = (int)((unsigned)img.alt * escala / 65536u);
    if (l < 1) l = 1;
    if (a < 1) a = 1;
    int x = ax + (al - l) / 2, y = ay + (aa - a) / 2;
    bool suave = escala < 65536u;   /* al reducir, el filtro bilineal */
    imagem_desenhar(j->px, j->larg, ax, ay, al, aa, &img, x, y, l, a, suave);
    if (!primeiro_desenhado) {
        primeiro_desenhado = true;
        registrar("reproductor: primer cuadro en la ventana\n");
    }
    jan_mostrar_area(j, ax, ay, al, aa);
}

static void desenhar_tudo(void)
{
    if (tela_cheia) g_retangulo(j, 0, 0, j->larg, j->alt, 0);
    desenhar_barra();
    desenhar_quadro();
    desenhar_rodape();
}

static void atualizar_titulo(void)
{
    char t[140];
    formatar_texto(t, sizeof t, "%s - Reproductor de vídeo", vid ? nome_curto(caminho_video) : "Reproductor de vídeo");
    titulo_janela(j->id, t);
}

/* ------------------------------------------------------------ el sonido */

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
    if (volume >= 0) audio_volume(audio_fd, volume);
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

/* ------------------------------------------------------------ búsqueda */

/* Salta al instante pedido (ms desde el principio). */
static void buscar_ms(int ms)
{
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
        if (audio_fd >= 0) audio_volume(audio_fd, mudo ? 0 : volume);
    }
    char t[80];
    formatar_tempo(t, sizeof t, q * (int)periodo_ms());
    formatar_texto(aviso, sizeof aviso, "En el segundo %s (cuadro %d de %d)", t, q + 1, quadros_total);
    registrar("reproductor: salto a %s (cuadro %d)\n", t, q);
    desenhar_rodape();
}

static void mudar_volume(int delta)
{
    volume += delta;
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    if (audio_fd >= 0) audio_volume(audio_fd, mudo ? 0 : volume);
    formatar_texto(aviso, sizeof aviso, "Volumen %d%%%s", volume, mudo ? " (silencio)" : "");
    registrar("reproductor: volumen %d\n", volume);
    desenhar_rodape();
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
    desenhar_rodape();
}

/* ------------------------------------------------------------ eventos */

static void mudar_pausa(void)
{
    /* Si el vídeo se había acabado, el espacio lo vuelve a empezar. */
    if (fim) {
        fim = false;
        buscar_ms(0);
        tocando = true;
        formatar_texto(aviso, sizeof aviso, "En marcha");
        registrar("reproductor: en marcha\n");
        desenhar_rodape();
        return;
    }
    tocando = !tocando;
    formatar_texto(aviso, sizeof aviso, tocando ? "En marcha" : "En pausa");
    registrar("reproductor: %s\n", tocando ? "en marcha" : "en pausa");
    desenhar_rodape();
}

/* Un cuadro hacia delante o hacia atrás (con la pausa puesta). */
static void passo_quadro(int passo)
{
    if (!vid) return;
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
            desenhar_rodape();
            return;
        }
        if (nova.px) imagem_liberar(&nova);
    }
    buscar_ms(q * (int)periodo_ms());
}

static void pedir_arquivo(void)
{
    char c[200];
    if (caminho_video[0]) copia_texto(c, caminho_video, sizeof c);
    else { copia_texto(c, "/apps/videos", sizeof c); junta_texto(c, "/", sizeof c); }
    if (!dialogo_abrir("Abrir vídeo", "Vídeos (*.mp4;*.avi;*.mpg;*.mpeg)|*.mp4;*.avi;*.mpg;*.mpeg|Todos los archivos (*.*)|*", c, c, sizeof c) || !c[0]) return;
    int erro = 0;
    video_t *v = video_abrir(c, &erro);
    if (!v) {
        formatar_texto(aviso, sizeof aviso, "No se ha podido abrir %s: %s", nome_curto(c), video_erro_texto(erro));
        registrar("reproductor: error al abrir %s (%d)\n", c, erro);
        desenhar_rodape();
        return;
    }
    if (vid) video_fechar(vid);
    vid = v;
    copia_texto(caminho_video, c, sizeof caminho_video);
    const video_info_t *info = video_info(vid);
    fps = info->fps > 0 ? info->fps : 30;
    quadros_total = info->quadros;
    quadro_atual = -1;
    quadros_vistos = quadros_saltados = 0;
    ms_decodificar = 0;
    ms_escritos = 0;
    t_inicio = tempo_ms();
    if (audio_fd >= 0) { audio_fechar(audio_fd, 0); audio_fd = -1; }
    abrir_som();
    aviso[0] = 0;
    atualizar_titulo();
    registrar("reproductor: abierto %s (%s) %dx%d %d fps, %d cuadros\n", caminho_video, info->nome_formato,
              info->larg, info->alt, fps, quadros_total);
    desenhar_tudo();
}

static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_REDIMENSIONAR) {
        bool cheia = e->tecla == JAN_ESTADO_TELA_CHEIA;
        if (jan_redimensionar(j, e)) {
            tela_cheia = cheia;
            if (barra) progresso_liberar(barra);
            barra = progresso_criar(8, j->alt - RODAPE + 26, j->larg - 16, 12, false);
            registrar("reproductor: ventana %dx%d%s\n", j->larg, j->alt, tela_cheia ? " (pantalla completa)" : "");
            desenhar_tudo();
        }
        return true;
    }
    if (e->tipo == EV_TECLA) {
        aviso[0] = 0;
        switch (e->tecla) {
        case ' ': mudar_pausa(); return true;
        case TECLA_ESQUERDA: buscar_ms((quadro_atual < 0 ? 0 : quadro_atual) * (int)periodo_ms() - SALTO_MS); return true;
        case TECLA_DIREITA: buscar_ms((quadro_atual < 0 ? 0 : quadro_atual) * (int)periodo_ms() + SALTO_MS); return true;
        case TECLA_PGUP: buscar_ms((quadro_atual < 0 ? 0 : quadro_atual) * (int)periodo_ms() - 1000); return true;
        case TECLA_PGDN: buscar_ms((quadro_atual < 0 ? 0 : quadro_atual) * (int)periodo_ms() + 1000); return true;
        case TECLA_HOME: buscar_ms(0); return true;
        case TECLA_END: buscar_ms(quadros_total * (int)periodo_ms()); return true;
        case ',': passo_quadro(-1); return true;
        case '.': passo_quadro(1); return true;
        case 'l': case 'L': bucle = !bucle; formatar_texto(aviso, sizeof aviso, bucle ? "Bucle activado" : "Bucle desactivado");
                        registrar("reproductor: bucle %s\n", bucle ? "activado" : "desactivado"); desenhar_rodape(); return true;
        case 'm': case 'M': mudo = !mudo; if (audio_fd >= 0) audio_volume(audio_fd, mudo ? 0 : volume);
                        formatar_texto(aviso, sizeof aviso, mudo ? "Silencio" : "Con sonido"); desenhar_rodape(); return true;
        case '+': case '=': mudar_volume(5); return true;
        case '-': mudar_volume(-5); return true;
        case 'i': case 'I': mostrar_estatisticas(); return true;
        case 15: pedir_arquivo(); return true;   /* Ctrl+O */
        default: return true;
        }
    }
    if (!tela_cheia && (e->tipo == EV_CLIQUE || e->tipo == EV_SOLTOU))
        for (int i = 0; i < QTD_BT; i++)
            if (botao_tratar(j, &bt[i], e)) {
                aviso[0] = 0;
                switch (i) {
                case 0: buscar_ms(0); return true;
                case 1: buscar_ms((quadro_atual < 0 ? 0 : quadro_atual) * (int)periodo_ms() - SALTO_MS); return true;
                case 2: buscar_ms((quadro_atual < 0 ? 0 : quadro_atual) * (int)periodo_ms() + SALTO_MS); return true;
                case 3: mudar_pausa(); return true;
                case 4: mudar_volume(-5); return true;
                case 5: mudar_volume(5); return true;
                case 6: bucle = !bucle; registrar("reproductor: bucle %s\n", bucle ? "activado" : "desactivado");
                        desenhar_rodape(); return true;
                case 7: jan_tela_cheia(j, !tela_cheia); return true;
                }
            }
    /* Un clic en la barra de posición: salta a ese punto. */
    if (e->tipo == EV_CLIQUE && barra && !tela_cheia) {
        retangulo_t r;
        progresso_rect(barra, &r);
        if (e->x >= r.x && e->x < r.x + r.largura && e->y >= r.y && e->y < r.y + r.altura) {
            int pct = (e->x - r.x) * 100 / (r.largura > 0 ? r.largura : 1);
            if (pct < 0) pct = 0;
            if (pct > 100) pct = 100;
            buscar_ms(pct * quadros_total * (int)periodo_ms() / 100);
            return true;
        }
    }
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
    if (i >= argc) {
        imprimir_em(ERROS, "uso: reproductor [-t|--volcar] archivo\n");
        return 2;
    }

    int erro = 0;
    vid = video_abrir(argv[i], &erro);
    if (!vid) {
        imprimir_em(ERROS, "reproductor: %s: %s\n", argv[i], video_erro_texto(erro));
        return 1;
    }
    copia_texto(caminho_video, argv[i], sizeof caminho_video);
    const video_info_t *info = video_info(vid);
    fps = info->fps > 0 ? info->fps : 30;
    quadros_total = info->quadros;

    j = jan_abrir("Reproductor de vídeo", LARG_INICIO, ALT_INICIO);
    if (!j) {
        video_fechar(vid);
        return 1;
    }
    jan_estilo(j, JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK, LARG_MIN, ALT_MIN);
    barra = progresso_criar(8, j->alt - RODAPE + 26, j->larg - 16, 12, false);
    abrir_som();
    t_inicio = tempo_ms();
    atualizar_titulo();
    desenhar_tudo();
    registrar("reproductor: listo (%s, %dx%d, %d fps, %d cuadros)\n", caminho_video, info->larg, info->alt, fps, quadros_total);

    evento_t e;
    bool continuar = true;
    while (continuar) {
        if (tocando) {
            unsigned t0 = tempo_ms();
            int r = video_quadro(vid, &img);
            ms_decodificar += tempo_ms() - t0;
            if (r == 0) {
                tem_quadro = true;
                quadro_atual = video_quadro_atual(vid);
                quadros_vistos++;
                escrever_audio();
                unsigned alvo = (unsigned)(quadro_atual < 0 ? 0 : quadro_atual) * periodo_ms();
                /* Se espera a la hora del cuadro. */
                while (tocando && relogio() < alvo) {
                    if (esperar_evento(j->id, &e, 5) == 1) {
                        continuar = tratar(&e);
                        if (!continuar) break;
                    }
                }
                if (tocando) {
                    /* El cuadro se enseña nada más decodificarlo: si se
                     * esperaba a su hora y luego había que ir deprisa, el
                     * primer cuadro podía no verse nunca. */
                    desenhar_quadro();
                    desenhar_rodape();
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
                if (bucle) {
                    video_buscar(vid, 0);
                    quadro_atual = -1;
                    t_inicio = tempo_ms();
                    ms_escritos = 0;
                    if (audio_fd >= 0) { audio_fechar(audio_fd, 0); audio_fd = -1; abrir_som(); }
                } else {
                    tocando = false;
                    fim = true;
                    formatar_texto(aviso, sizeof aviso, "Fin del vídeo");
                    registrar("reproductor: fin del video\n");
                    desenhar_rodape();
                }
            } else {
                tocando = false;
                formatar_texto(aviso, sizeof aviso, "Error: %s", video_erro_texto(r));
                registrar("reproductor: error %d\n", r);
                desenhar_rodape();
            }
        }
        if (continuar && esperar_evento(j->id, &e, 20) == 1) continuar = tratar(&e);
    }
    registrar("reproductor: cerrado\n");
    if (audio_fd >= 0) audio_fechar(audio_fd, 1);
    if (img.px) imagem_liberar(&img);
    if (barra) progresso_liberar(barra);
    if (vid) video_fechar(vid);
    jan_fechar(j);
    return 0;
}
