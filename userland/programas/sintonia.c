// SPDX-License-Identifier: GPL-2.0-or-later
/* sintonia [archivo | carpeta | lista.m3u | lista.pls | http://emisora]... - Sintonía, el reproductor de música de SaguiOS (D225).
 *
 * Un reproductor «al estilo» de los clásicos de escritorio de los 2000, con diseño propio (nada de código,
 * nombre ni pieles de ningún otro): ventana principal compacta con título que se desplaza, tiempo (clic: pasa
 * de transcurrido a restante), visualizador (espectro por FFT u osciloscopio; clic para cambiar), barra de
 * posición, volumen, balance y los botones anterior, reproducir, pausa, parar, siguiente y abrir, más
 * aleatorio, repetir, ecualizador, lista y modo mini. Debajo, el ecualizador de 5 bandas del sistema (D201)
 * con sus preajustes y la lista de reproducción (añadir archivos, carpetas, listas M3U/PLS y emisoras;
 * quitar, subir, bajar, ordenar, buscar, guardar). Es una aplicación no esencial (D194): va a /apps.
 *
 * Por qué un programa nuevo y no ampliar `musica` (D115): `musica` es el reproductor sencillo del sistema
 * base, con sus pruebas (h86, h157...) y su ventana de listas a pantalla completa; Sintonía tiene otro diseño
 * (ventana compacta con paneles que se pliegan), otro ciclo de vida (red, hilo de radio, estado guardado) y
 * otras dependencias (libtls para HTTPS), que no deben engordar el /bin del ramdisk. Comparten los
 * decodificadores de la libsagui (audio_formato.h) y el ecualizador del mezclador.
 *
 * Reproducción: como en `musica`, se decodifica por bloques entre evento y evento a un anillo y se entrega
 * al flujo del kernel sin bloquear, con ~0,4 s por delante, de modo que la pausa y el salto cortan casi al
 * instante. El anillo sirve también para el visualizador: el espectro y el osciloscopio salen de las
 * últimas 256 muestras ya entregadas que están sonando ahora (el kernel no deja leer lo que se toca, así que
 * el propio reproductor lo calcula al decodificar). La FFT es entera (el userland no usa la FPU) y cuesta
 * unas mil mariposas cada 50 ms. El balance se aplica al entregar (el anillo conserva el original).
 *
 * Radio por internet: un hilo (el de red) baja el flujo MP3 por HTTP o HTTPS, quita los metadatos ICY
 * (StreamTitle) y lo corta en cuadros enteros en archivos pequeños de /tmp; el hilo principal los abre uno
 * tras otro con el decodificador de archivos de la libsagui (que solo lee de archivos) sin cortar el
 * flujo de audio. Se admiten emisoras en MP3 (las listas .pls/.m3u de una emisora se siguen); AAC y Ogg
 * no: lo dice.
 *
 * Archivos de /home/.config/sintonia/: estado.conf (volumen, balance, aleatorio, repetir, pista, posición,
 * modo mini, paneles), lista.m3u (la lista, con las duraciones) y tema.conf (colores y disposición; se crea
 * uno de ejemplo la primera vez; ver sintonia_logica.h, sl_tema_linea).
 *
 * Teclas: Z anterior, X reproducir, C pausa, V parar, B siguiente (las de siempre); espacio reproducir/pausa;
 * izquierda/derecha -/+ 5 s; arriba/abajo, +/- y la rueda, volumen (con la lista visible, arriba/abajo mueven
 * la selección y el volumen va con +/- y Ctrl+arriba/abajo); S aleatorio, R repetir, E ecualizador, L lista,
 * M mini; A añadir, O abrir (sustituye la lista), Supr quitar, Intro toca la elegida, Ctrl+V pega rutas del
 * portapapeles, Ctrl+S guarda la lista, / o Ctrl+F busca.
 *
 * Pendiente (no lo permite el sistema todavía): «siempre encima» (el gestor de ventanas no tiene esa
 * opción) y arrastrar archivos desde Archivos (no hay mecanismo de arrastrar y soltar entre programas): en su
 * lugar, Archivos abre los sonidos con doble clic en `musica`; para llenar Sintonía se escribe la ruta, se
 * pegan rutas (Ctrl+V) o se abre una lista M3U/PLS. */
#include "sagui.h"
#include "gui.h"
#include "tls.h"
#include "audio_formato.h"
#include "sintonia_logica.h"

#define LARG_INICIO 520
#define LARG_MIN    500
#define ALT_BASE    190          /* controles + pie, sin paneles */
#define ALT_EQ      108
#define ALT_LISTA_INICIO 220
#define ALT_LISTA_MIN    110
#define LARG_MINI   520
#define ALT_MINI    64
#define MAX_PISTAS  300
#define PED_QUADROS 1152
#define ANEL_Q      65536u       /* cuadros del anillo (potencia de 2) */
#define ADELANTE_MS 400
#define SALTO_MS    5000
#define ALTO_FILA   18
#define PIE_A       20
#define DIR_CONF    "/home/.config/sintonia"
#define VIS_N       256          /* muestras del visualizador (SL_FFT_N) */

enum { PARADO, TOCANDO, PAUSADO };

typedef struct { int x, y, w, h; } rc_t;

/* ------------------------------------------------------------ estado */

static jan_t *j;
static sl_tema_t tema;
static bool mini, ver_lista, ver_eq;
static int alt_lista = ALT_LISTA_INICIO;   /* alto de la ventana ocupado por la lista (normal) */

static sl_pista_t pistas[MAX_PISTAS];
static int qtd, atual = -1;
static int vista[MAX_PISTAS], nvista;      /* pistas que pasan el filtro de búsqueda */
static int sel = -1, topo;                 /* selección (índice de pista) y primera fila visible (índice en vista) */
static caixa_t busca;
static char busca_ant[128];
static int meta_sig;                       /* siguiente pista a la que leerle las etiquetas */

static int aleatorio, repetir;             /* repetir: 0 no, 1 lista, 2 pista */
static int orden[MAX_PISTAS], pos_orden = -1;
static bool orden_ok;
static uint32_t semilla;

static int volume = 80, balance;
static int eq_db[EQ_BANDAS];
static char aviso[160];
static char radio_titulo[96];

/* reproducción */
static audio_arquivo_t *arq;
static const audio_info_t *inf;
static audio_info_t inf_copia;             /* lo de la radio: la info del trozo ya cerrado */
static int estado = PARADO;
static int fluxo = -1;
static uint32_t fluxo_taxa, fluxo_can;
static int16_t anel[ANEL_Q * 2];
static uint32_t n_dec, n_esc, base_q;
static bool fim_dec;
static bool modo_radio;
static uint32_t radio_ms;                  /* tiempo escuchado de la emisora */
static char seg_actual[100];               /* trozo de radio abierto */
static uint32_t pos_pend_ms;               /* posición recordada de la sesión anterior */
static int pista_pend = -1;
static bool mostrar_restante;

/* visualizador */
static int16_t muestras[VIS_N];
static uint8_t barras[SL_BARRAS_MAX], picos[SL_BARRAS_MAX];
static int vis_modo;

/* animaciones */
static uint32_t t_vis, t_titulo, t_tick;
static int desplaza;                       /* desplazamiento del título, en letras */
static uint32_t seg_mostrado = 0xFFFFFFFFu;

/* ratón */
enum { ARR_NADA, ARR_SEEK, ARR_VOL, ARR_BAL, ARR_EQ };
static int arrastrando, arr_banda;
static uint32_t previa_ms;

/* ------------------------------------------------------------ geometría y controles */

enum { C_ANT, C_PLAY, C_PAUSA, C_PARAR, C_SIG, C_ABRIR, C_ALEA, C_REP, C_EQ, C_LISTA, C_MINI,
       C_ADD, C_QUITAR, C_SUBIR, C_BAJAR, C_ORDENAR, C_LISTAS, C_PRE0, C_NCTL = C_PRE0 + 4 };
enum { E_TRANS, E_TOGGLE, E_CLARO };
typedef struct { rc_t r; int id, estilo; const char *txt; bool on; } ctl_t;
static ctl_t ctl[C_NCTL];
static int nctl;

static struct {
    rc_t panel, titulo, tiempo, info1, info2, vis, seek, vol, bal, eq, banda[EQ_BANDAS], lista, pie, lb_busca;
    int y_toolbar, y_busca;
} ly;

static bool dentro(const rc_t *r, int x, int y) { return x >= r->x && x < r->x + r->w && y >= r->y && y < r->y + r->h; }

static void poner_ctl(int id, int x, int y, int w, int h, int estilo, const char *txt, bool on)
{
    ctl_t *c = &ctl[nctl++];
    c->r = (rc_t){ x, y, w, h };
    c->id = id; c->estilo = estilo; c->txt = txt; c->on = on;
}

static bool hay_pista_url(void) { return atual >= 0 && atual < qtd && sl_es_url(pistas[atual].ruta); }

static void calcular_layout(void)
{
    int L = j->larg;
    nctl = 0;
    if (mini) {
        ly.titulo = (rc_t){ 150, 10, L - 150 - 60, 16 };
        ly.tiempo = (rc_t){ 150, 30, 80, 16 };
        ly.vis = (rc_t){ 236, 28, L - 236 - 60, 20 };
        ly.seek = (rc_t){ 8, 52, L - 16, 8 };
        ly.panel = (rc_t){ 146, 6, L - 146 - 56, 44 };
        ly.info1 = ly.info2 = (rc_t){ 0, 0, 0, 0 };
        ly.vol = ly.bal = ly.eq = ly.lista = ly.pie = (rc_t){ 0, 0, 0, 0 };
        poner_ctl(C_ANT, 8, 10, 30, 28, E_TRANS, "|<", false);
        poner_ctl(C_PLAY, 42, 10, 30, 28, E_TRANS, estado == TOCANDO ? "||" : ">", estado == TOCANDO);
        poner_ctl(C_PARAR, 76, 10, 30, 28, E_TRANS, "[]", false);
        poner_ctl(C_SIG, 110, 10, 30, 28, E_TRANS, ">|", false);
        poner_ctl(C_MINI, L - 52, 10, 44, 28, E_TOGGLE, "Max", false);
        return;
    }
    ly.panel = (rc_t){ 8, 8, L - 16, 80 };
    ly.titulo = (rc_t){ 14, 12, L - 28, 16 };
    ly.tiempo = (rc_t){ 14, 34, 92, 18 };
    ly.info1 = (rc_t){ 14, 54, 92, 14 };
    ly.info2 = (rc_t){ 14, 68, 92, 14 };
    ly.vis = (rc_t){ 112, 32, L - 112 - 14, 50 };
    int yb = tema.botones_arriba ? 94 : 110;   /* el tema puede poner los botones encima de la barra de posición */
    ly.seek = tema.botones_arriba ? (rc_t){ 8, 122, L - 16, 8 } : (rc_t){ 8, 94, L - 16, 10 };
    poner_ctl(C_ANT, 8, yb, 44, 24, E_TRANS, "|<", false);
    poner_ctl(C_PLAY, 56, yb, 44, 24, E_TRANS, ">", estado == TOCANDO);
    poner_ctl(C_PAUSA, 104, yb, 44, 24, E_TRANS, "||", estado == PAUSADO);
    poner_ctl(C_PARAR, 152, yb, 44, 24, E_TRANS, "[]", false);
    poner_ctl(C_SIG, 200, yb, 44, 24, E_TRANS, ">|", false);
    poner_ctl(C_ABRIR, 248, yb, 56, 24, E_TRANS, "Abrir", false);
    ly.vol = (rc_t){ 36, 142, 100, 12 };
    ly.bal = (rc_t){ 176, 142, 80, 12 };
    int x = L - 8 - 5 * 44 + 4;
    poner_ctl(C_ALEA, x, 138, 40, 20, E_TOGGLE, "Alea", aleatorio);
    poner_ctl(C_REP, x + 44, 138, 40, 20, E_TOGGLE, repetir == 2 ? "Rep1" : "Rep", repetir != 0);
    poner_ctl(C_EQ, x + 88, 138, 40, 20, E_TOGGLE, "EQ", ver_eq);
    poner_ctl(C_LISTA, x + 132, 138, 40, 20, E_TOGGLE, "Lista", ver_lista);
    poner_ctl(C_MINI, x + 176, 138, 40, 20, E_TOGGLE, "Mini", false);
    int y0 = 166;
    ly.eq = (rc_t){ 0, 0, 0, 0 };
    if (ver_eq) {
        ly.eq = (rc_t){ 8, y0, L - 16, ALT_EQ - 4 };
        for (int b = 0; b < EQ_BANDAS; b++) ly.banda[b] = (rc_t){ 30 + b * 56, y0 + 8, 14, 64 };
        for (int p = 0; p < 4; p++) poner_ctl(C_PRE0 + p, 330, y0 + 6 + p * 24, 90, 20, E_CLARO, p == 0 ? "Plano" : p == 1 ? "Graves" : p == 2 ? "Voz" : "Agudos", false);
        y0 += ALT_EQ;
    }
    ly.pie = (rc_t){ 0, j->alt - PIE_A, L, PIE_A };
    ly.lista = (rc_t){ 0, 0, 0, 0 };
    if (ver_lista) {
        ly.y_toolbar = y0;
        poner_ctl(C_ADD, 8, y0, 70, 22, E_CLARO, "Añadir", false);
        poner_ctl(C_QUITAR, 82, y0, 60, 22, E_CLARO, "Quitar", false);
        poner_ctl(C_SUBIR, 146, y0, 52, 22, E_CLARO, "Subir", false);
        poner_ctl(C_BAJAR, 202, y0, 52, 22, E_CLARO, "Bajar", false);
        poner_ctl(C_ORDENAR, 258, y0, 70, 22, E_CLARO, "Ordenar", false);
        poner_ctl(C_LISTAS, 332, y0, 64, 22, E_CLARO, "Listas", false);
        ly.y_busca = y0 + 26;
        ly.lb_busca = (rc_t){ 8, ly.y_busca + 4, 56, 16 };
        busca.x = 66; busca.y = ly.y_busca; busca.l = L - 16 - 58;
        int ly0 = ly.y_busca + 28;
        ly.lista = (rc_t){ 8, ly0, L - 16, ly.pie.y - 4 - ly0 };
        if (ly.lista.h < ALTO_FILA * 2) ly.lista.h = ALTO_FILA * 2;
    }
}

/* Cambia el tamaño de la ventana (modo mini, paneles). */
static void cambiar_tam(int l, int a)
{
    uint32_t *px;
    if (redimensionar_janela(j->id, l, a, &px) < 0) return;
    j->px = px; j->larg = l; j->alt = a;
}

static int alt_normal(void) { return ALT_BASE + (ver_eq ? ALT_EQ : 0) + (ver_lista ? alt_lista : 0); }

static void estilo_ventana(void)
{
    if (mini) jan_estilo(j, 0, LARG_MINI, ALT_MINI);
    else jan_estilo(j, JAN_REDIMENSIONAVEL, LARG_MIN, ALT_BASE + (ver_eq ? ALT_EQ : 0) + (ver_lista ? ALT_LISTA_MIN : 0));
}

/* ------------------------------------------------------------ utilidades */

static const char *nome_curto(const char *c) { return sl_nombre_base(c); }

static uint32_t quadros_ms(uint32_t q, uint32_t taxa)
{
    return taxa ? q / taxa * 1000 + q % taxa * 1000 / taxa : 0;
}

static uint32_t duracao_ms(void) { return modo_radio || !inf ? 0 : inf->duracao_ms; }

static uint32_t pendentes_q(void)
{
    if (fluxo < 0 || !fluxo_taxa) return 0;
    int ms = audio_pendente(fluxo);
    if (ms <= 0) return 0;
    uint32_t q = (uint32_t)ms * (fluxo_taxa / 100) / 10;
    return q > n_esc ? n_esc : q;
}

static uint32_t posicao_q(void)
{
    return base_q + n_esc - (estado == TOCANDO ? pendentes_q() : 0);
}

static uint32_t posicao_ms(void)
{
    if (modo_radio) return radio_ms;
    if (!inf) return 0;
    uint32_t ms = quadros_ms(posicao_q(), inf->taxa), d = duracao_ms();
    return d && ms > d ? d : ms;
}

static bool extensao_audio(const char *c)
{
    unsigned n = (unsigned)tam_texto(c), p = n;
    while (p && c[p - 1] != '.' && c[p - 1] != '/') p--;
    if (!p || c[p - 1] != '.') return false;
    static const char *const ext[] = { "wav", "mp3", "flac", "mid", "midi", "kar" };
    for (unsigned i = 0; i < sizeof ext / sizeof ext[0]; i++)
        if (sl_prefijo_ci(c + p, ext[i]) && c[p + tam_texto(ext[i])] == 0) return true;
    return false;
}

static void juntar_ruta(char *saida, size_t max, const char *pasta, const char *nome)
{
    copia_texto(saida, pasta, max);
    if (compara_texto(pasta, "/") != 0) junta_texto(saida, "/", max);
    junta_texto(saida, nome, max);
}

static void dir_de(const char *ruta, char *d, size_t max)
{
    copia_texto(d, ruta, max);
    char *b = acha_ultimo_char(d, '/');
    if (b) { if (b == d) b[1] = 0; else *b = 0; } else copia_texto(d, "/", max);
}

static void formato_corto(char *d, size_t max)
{
    d[0] = 0;
    if (!inf) return;
    switch (inf->formato) {
    case AUDIO_FMT_MP3: copia_texto(d, "MP3", max); break;
    case AUDIO_FMT_FLAC: copia_texto(d, "FLAC", max); break;
    case AUDIO_FMT_WAV: copia_texto(d, "WAV", max); break;
    case AUDIO_FMT_ADPCM: copia_texto(d, "ADPCM", max); break;
    case AUDIO_FMT_MIDI: copia_texto(d, "MIDI", max); break;
    default: break;
    }
}

/* ------------------------------------------------------------ dibujo */

static void texto_en(const rc_t *r, int dx, int dy, const char *t, uint32_t cor)
{
    int n = (r->w - dx) / LETRA_L;
    if (n > 0) g_texto_n(j, r->x + dx, r->y + dy, t, n, cor);
}

/* Botón plano con los colores del tema. */
static void dib_boton(const ctl_t *c)
{
    uint32_t fondo, texto, borde;
    if (c->estilo == E_CLARO) { fondo = COR_BOTAO; texto = tema.texto; borde = COR_BORDA; }
    else if (c->on) { fondo = tema.acento; texto = tema.panel; borde = tema.acento; }
    else { fondo = tema.panel2; texto = tema.acento; borde = tema.panel; }
    g_retangulo(j, c->r.x, c->r.y, c->r.w, c->r.h, fondo);
    g_moldura(j, c->r.x, c->r.y, c->r.w, c->r.h, borde);
    int tw = (int)tam_texto(c->txt) * LETRA_L;
    g_texto(j, c->r.x + (c->r.w - tw) / 2, c->r.y + (c->r.h - LETRA_A) / 2, c->txt, texto);
}

static void dib_deslizador(const rc_t *r, int valor, int max, uint32_t cor)
{
    int cy = r->y + r->h / 2;
    g_retangulo(j, r->x, cy - 2, r->w, 4, tema.panel);
    int f = max ? valor * r->w / max : 0;
    if (f > 0) g_retangulo(j, r->x, cy - 2, f, 4, cor);
    g_retangulo(j, r->x + f - 3 < r->x ? r->x : r->x + f - 3, cy - 6, 6, 12, tema.panel2);
    g_moldura(j, r->x + f - 3 < r->x ? r->x : r->x + f - 3, cy - 6, 6, 12, cor);
}

static void texto_titulo(char *t, size_t max)
{
    if (atual < 0 || atual >= qtd) { copia_texto(t, "Sintonía - ninguna pista", max); return; }
    char n[160];
    sl_nombre_pista(&pistas[atual], n, sizeof n);
    if (modo_radio) {
        const char *e = radio_titulo[0] ? radio_titulo : n;
        if (radio_titulo[0] && n[0] && compara_texto(n, radio_titulo) != 0) formatar_texto(t, max, "%s  (%s)", radio_titulo, n);
        else copia_texto(t, e, max);
        return;
    }
    formatar_texto(t, max, "%d. %s", atual + 1, n);
}

static void dib_titulo(void)
{
    rc_t r = ly.titulo;
    g_retangulo(j, r.x - 2, r.y - 2, r.w + 4, r.h + 4, tema.panel);
    char t[300], v[160];
    texto_titulo(t, sizeof t);
    int cab = r.w / LETRA_L, n = (int)tam_texto(t);
    if (n <= cab) { g_texto_n(j, r.x, r.y, t, cab, tema.acento); return; }
    /* se desplaza en círculo, con un hueco entre vueltas */
    char largo[340];
    formatar_texto(largo, sizeof largo, "%s   ***   ", t);
    int ln = (int)tam_texto(largo);
    int ini = desplaza % ln, k = 0;
    while (k < cab && k < (int)sizeof v - 1) { v[k] = largo[(ini + k) % ln]; k++; }
    v[k] = 0;
    g_texto_n(j, r.x, r.y, v, cab, tema.acento);
}


static const char *rad_texto_estado(void);

static void dib_tiempo(void)
{
    uint32_t pos = arrastrando == ARR_SEEK ? previa_ms : posicao_ms(), dur = duracao_ms();
    seg_mostrado = pos / 1000;
    rc_t r = ly.tiempo;
    g_retangulo(j, r.x - 2, r.y - 2, r.w + 4, r.h + 4, tema.panel);
    char t[24];
    if (mostrar_restante && dur) {
        char a[16];
        sl_tiempo(a, sizeof a, (int)((dur - pos + 999) / 1000));
        formatar_texto(t, sizeof t, "-%s", a);
    } else sl_tiempo(t, sizeof t, (int)(pos / 1000));
    g_texto(j, r.x, r.y, t, tema.acento);
    g_texto(j, r.x + 1, r.y, t, tema.acento);   /* negrita falsa */
    if (mini && dur) {
        char a[16];
        sl_tiempo(a, sizeof a, (int)(dur / 1000));
        formatar_texto(t, sizeof t, "/%s", a);
        g_texto(j, r.x + 44, r.y, t, tema.texto_fraco);
    }
}

static void dib_info(void)
{
    if (mini) return;
    g_retangulo(j, ly.info1.x - 2, ly.info1.y - 1, ly.info1.w + 4, 30, tema.panel);
    char t[40];
    t[0] = 0;
    if (modo_radio) copia_texto(t, rad_texto_estado(), sizeof t);
    else if (duracao_ms()) {
        char a[16];
        sl_tiempo(a, sizeof a, (int)(duracao_ms() / 1000));
        formatar_texto(t, sizeof t, "de %s", a);
    }
    texto_en(&ly.info1, 0, 0, t, tema.texto_fraco);
    t[0] = 0;
    if (inf) {
        char f[12];
        formato_corto(f, sizeof f);
        if (inf->kbps) formatar_texto(t, sizeof t, "%s %uk", f, inf->kbps);
        else formatar_texto(t, sizeof t, "%s %ukHz", f, inf->taxa / 1000);
    }
    texto_en(&ly.info2, 0, 0, t, tema.texto_fraco);
}

static void dib_seek(void)
{
    rc_t r = ly.seek;
    g_retangulo(j, r.x - 2, r.y - 4, r.w + 4, r.h + 8, tema.fondo);
    g_retangulo(j, r.x, r.y, r.w, r.h, tema.panel);
    uint32_t dur = duracao_ms(), pos = arrastrando == ARR_SEEK ? previa_ms : posicao_ms();
    if (dur && inf) {
        int f = (int)((pos / 64) * (uint32_t)r.w / (dur / 64 | 1));
        if (f > r.w) f = r.w;
        if (f > 0) g_retangulo(j, r.x, r.y, f, r.h, tema.acento);
        int kx = r.x + f - 3;
        if (kx < r.x) kx = r.x;
        g_retangulo(j, kx, r.y - 3, 6, r.h + 6, tema.panel2);
        g_moldura(j, kx, r.y - 3, 6, r.h + 6, tema.acento);
    } else if (modo_radio && estado != PARADO) {
        int f = (int)(radio_ms / 100 % (uint32_t)(r.w - 24));   /* cinta que corre: no hay posición en un directo */
        g_retangulo(j, r.x + f, r.y, 24, r.h, tema.acento);
    }
}

/* El visualizador: barras de espectro, osciloscopio o las dos a la vez. */
static void dib_vis(void)
{
    rc_t r = ly.vis;
    g_retangulo(j, r.x, r.y, r.w, r.h, mini ? tema.panel : tema.panel2);
    int nb = r.w / 6;
    if (nb > SL_BARRAS_MAX) nb = SL_BARRAS_MAX;
    if (nb < 1) return;
    if (vis_modo == 0 || vis_modo == 2) {
        int paso = r.w / nb, ancho = paso > 3 ? paso - 2 : paso;
        for (int i = 0; i < nb; i++) {
            int h = barras[i] * (r.h - 2) / 100, x = r.x + i * paso + 1;
            if (h > 0) g_retangulo(j, x, r.y + r.h - 1 - h, ancho, h, tema.barras);
            int hp = picos[i] * (r.h - 2) / 100;
            if (hp > 1) g_retangulo(j, x, r.y + r.h - 2 - hp, ancho, 1, tema.picos);
        }
    }
    if (vis_modo == 1 || vis_modo == 2) {
        int cy = r.y + r.h / 2, ant_y = cy;
        for (int x = 0; x < r.w; x++) {
            int s = muestras[x * VIS_N / r.w];
            int y = cy - s * (r.h / 2 - 1) / 32768;
            if (x) g_linha(j, r.x + x - 1, ant_y, r.x + x, y, tema.osciloscopio);
            ant_y = y;
        }
    }
}

static void dib_volumen(void)
{
    if (mini) return;
    g_retangulo(j, 0, 134, ly.bal.x + ly.bal.w + 14, 28, tema.fondo);
    g_texto(j, 8, 140, "Vol", tema.texto);
    dib_deslizador(&ly.vol, volume, 100, tema.acento);
    char t[16];
    formatar_texto(t, sizeof t, "%d", volume);
    g_texto(j, ly.vol.x + ly.vol.w + 6, 140, t, tema.texto_fraco);
    g_texto(j, 144, 140, "Bal", tema.texto);
    dib_deslizador(&ly.bal, balance + 100, 200, tema.acento);
    g_retangulo(j, ly.bal.x + ly.bal.w / 2 - 1, ly.bal.y + 1, 2, 3, tema.texto_fraco);
}

static void dib_eq(void)
{
    if (!ver_eq || mini) return;
    g_retangulo(j, ly.eq.x, ly.eq.y, ly.eq.w, ly.eq.h, tema.panel);
    static const char *const nombres[EQ_BANDAS] = { "60", "250", "1k", "4k", "12k" };
    for (int b = 0; b < EQ_BANDAS; b++) {
        rc_t r = ly.banda[b];
        int cx = r.x + r.w / 2;
        g_retangulo(j, cx - 1, r.y, 3, r.h, tema.panel2);
        g_retangulo(j, cx - 5, r.y + r.h / 2, 11, 1, tema.texto_fraco);
        int ky = r.y + (12 - eq_db[b]) * r.h / 24;
        g_retangulo(j, r.x, ky - 3, r.w, 7, tema.acento);
        char t[8];
        eq_db_texto(t, eq_db[b]);
        g_texto(j, cx - (int)tam_texto(nombres[b]) * 4, r.y + r.h + 4, nombres[b], tema.texto_fraco);
        g_texto(j, cx + 12, ky - 8, t, tema.acento);
    }
    const char *p = eq_nome_preajuste(eq_db);
    char t[40];
    formatar_texto(t, sizeof t, "Ecualizador: %s", p ? p : "a medida");
    g_texto(j, 330, ly.eq.y + ly.eq.h - 20, t, tema.acento);
}

static void nombre_fila(int idx, char *d, size_t max)
{
    char n[160];
    sl_nombre_pista(&pistas[idx], n, sizeof n);
    formatar_texto(d, max, "%s%3d. %s", idx == atual ? ">" : " ", idx + 1, n);
}

static int filas_visibles(void) { return ly.lista.h > 0 ? (ly.lista.h - 2) / ALTO_FILA : 0; }

static void ajustar_topo(void)
{
    int f = filas_visibles();
    if (f <= 0) return;
    if (topo > nvista - f) topo = nvista - f;
    if (topo < 0) topo = 0;
}

/* Deja a la vista la pista idx (si pasa el filtro). */
static void mostrar_pista(int idx)
{
    int f = filas_visibles();
    if (f <= 0 || !ver_lista || mini) return;
    for (int k = 0; k < nvista; k++)
        if (vista[k] == idx) {
            if (k < topo) topo = k;
            else if (k >= topo + f) topo = k - f + 1;
            ajustar_topo();
            return;
        }
}

static void dib_lista(void)
{
    if (!ver_lista) return;
    rc_t r = ly.lista;
    g_retangulo(j, r.x, r.y, r.w, r.h, tema.lista_fondo);
    g_moldura(j, r.x, r.y, r.w, r.h, COR_BORDA);
    if (!qtd) {
        g_texto_n(j, r.x + 8, r.y + 6, "La lista está vacía.", (r.w - 16) / LETRA_L, COR_TEXTO_FRACO);
        g_texto_n(j, r.x + 8, r.y + 6 + ALTO_FILA, "Pulsa Añadir, pega rutas con Ctrl+V o abre una lista M3U/PLS.", (r.w - 16) / LETRA_L, COR_TEXTO_FRACO);
        return;
    }
    ajustar_topo();
    int f = filas_visibles();
    for (int k = 0; k < f && topo + k < nvista; k++) {
        int idx = vista[topo + k], y = r.y + 1 + k * ALTO_FILA;
        bool es_sel = idx == sel;
        if (es_sel) g_retangulo(j, r.x + 1, y, r.w - 2, ALTO_FILA, tema.seleccion);
        uint32_t cor = es_sel ? COR_BRANCO : idx == atual ? COR_DESTAQUE : COR_TEXTO;
        char t[220], d[16];
        nombre_fila(idx, t, sizeof t);
        int cab = (r.w - 16 - 6 * LETRA_L - 8) / LETRA_L;
        g_texto_n(j, r.x + 4, y + 1, t, cab, cor);
        if (pistas[idx].dur_s >= 0) {
            sl_tiempo(d, sizeof d, pistas[idx].dur_s);
            g_texto(j, r.x + r.w - 10 - (int)tam_texto(d) * LETRA_L, y + 1, d, es_sel ? COR_BRANCO : COR_TEXTO_FRACO);
        } else if (sl_es_url(pistas[idx].ruta)) g_texto(j, r.x + r.w - 10 - 4 * LETRA_L, y + 1, "radio", es_sel ? COR_BRANCO : COR_TEXTO_FRACO);
    }
    if (nvista > f) {   /* barra de desplazamiento */
        int th = r.h * f / nvista;
        if (th < 12) th = 12;
        int ty = r.y + (r.h - th) * topo / (nvista - f);
        g_retangulo(j, r.x + r.w - 5, ty, 4, th, COR_BORDA);
    }
}

static void dib_pie(void)
{
    if (mini) return;
    rc_t r = ly.pie;
    g_retangulo(j, r.x, r.y, r.w, r.h, 0xE6E8EA);
    g_retangulo(j, r.x, r.y, r.w, 1, COR_BORDA);
    char t[200];
    if (aviso[0]) copia_texto(t, aviso, sizeof t);
    else if (!qtd) copia_texto(t, "Lista vacía", sizeof t);
    else {
        int tot = 0, conocidas = 0;
        for (int i = 0; i < qtd; i++) if (pistas[i].dur_s > 0) { tot += pistas[i].dur_s; conocidas++; }
        char d[16];
        sl_tiempo(d, sizeof d, tot);
        const char *est = estado == TOCANDO ? (modo_radio ? "Radio" : "Reproduciendo") : estado == PAUSADO ? "En pausa" : "Detenido";
        if (nvista != qtd) formatar_texto(t, sizeof t, "%s  |  %d de %d pistas (filtro)  |  %s%s", est, nvista, qtd, d, conocidas < qtd ? "+" : "");
        else formatar_texto(t, sizeof t, "%s  |  pista %d de %d  |  %s%s", est, atual + 1, qtd, d, conocidas < qtd ? "+" : "");
    }
    g_texto_n(j, 8, r.y + 3, t, (r.w - 16) / LETRA_L, aviso[0] ? 0x9A1B1B : COR_TEXTO);
}

static void dib_panel(void)
{
    g_retangulo(j, ly.panel.x, ly.panel.y, ly.panel.w, ly.panel.h, tema.panel);
    g_moldura(j, ly.panel.x, ly.panel.y, ly.panel.w, ly.panel.h, tema.panel2);
}

static void dibujar_todo(void)
{
    calcular_layout();
    g_retangulo(j, 0, 0, j->larg, j->alt, tema.fondo);
    dib_panel();
    dib_titulo();
    dib_tiempo();
    dib_info();
    dib_vis();
    dib_seek();
    for (int i = 0; i < nctl; i++) dib_boton(&ctl[i]);
    dib_volumen();
    dib_eq();
    if (ver_lista && !mini) {
        g_texto(j, ly.lb_busca.x, ly.lb_busca.y, "Buscar", tema.texto);
        caixa_desenhar(j, &busca);
        dib_lista();
    }
    dib_pie();
    jan_mostrar(j);
    char tv[200], n[160];
    if (atual >= 0 && atual < qtd && !mini) {
        sl_nombre_pista(&pistas[atual], n, sizeof n);
        formatar_texto(tv, sizeof tv, "%s - Sintonía", n);
    } else copia_texto(tv, "Sintonía", sizeof tv);
    titulo_janela(j->id, tv);
}

static void mostrar_rc(const rc_t *r) { jan_mostrar_area(j, r->x - 2, r->y - 4, r->w + 4, r->h + 8); }
static void redib_tiempo(void) { dib_tiempo(); dib_seek(); mostrar_rc(&ly.tiempo); mostrar_rc(&ly.seek); if (!mini) mostrar_rc(&ly.info1); }
static void redib_titulo(void) { dib_titulo(); mostrar_rc(&ly.titulo); }
static void redib_vis(void) { dib_vis(); jan_mostrar_area(j, ly.vis.x, ly.vis.y, ly.vis.w, ly.vis.h); }
static void redib_lista(void) { if (ver_lista && !mini) { dib_lista(); dib_pie(); jan_mostrar_area(j, 0, ly.lista.y, j->larg, j->alt - ly.lista.y); } }

/* ------------------------------------------------------------ radio: el hilo de red */

#define SEG_PRIMERO 40000u     /* el primer trozo es corto para empezar a sonar pronto (~2,5 s a 128 kbps) */
#define SEG_RESTO   96000u
#define ACUM_MAX    (SEG_RESTO + 28000u)
#define SEG_ADELANTE 6u        /* trozos como máximo por delante del reproductor */

enum { R_CONECTANDO, R_ALMACENANDO, R_SONANDO, R_ERROR };

typedef struct rad_ses {
    volatile uint32_t cancelar;    /* el principal pide acabar */
    volatile uint32_t fin;         /* el hilo ha acabado */
    volatile uint32_t estado;      /* R_* */
    volatile uint32_t listos;      /* trozos terminados (los escribe el hilo) */
    volatile uint32_t usados;      /* trozos ya abiertos por el reproductor */
    volatile uint32_t tit_nuevo;
    volatile uint32_t ref;         /* dueños: el principal y el hilo; el último libera */
    uint32_t id;
    char url[400];
    char msg[140];
    char nombre[80];               /* icy-name de la emisora */
    char titulo[96];               /* último StreamTitle */
} rad_ses_t;

static rad_ses_t *rad;
static uint32_t rad_cont;

static void rad_soltar(rad_ses_t *s)
{
    if (atomico_dec32(&s->ref) == 1) libera(s);
}

static void rad_ruta(const rad_ses_t *s, uint32_t n, char *d, size_t max)
{
    formatar_texto(d, max, "/tmp/sintonia-%u-%u-%u.mp3", (unsigned)meu_pid(), (unsigned)s->id, (unsigned)n);
}

static void rad_msg(rad_ses_t *s, const char *t) { copia_texto(s->msg, t, sizeof s->msg); }

/* Conecta (siguiendo redirecciones y listas .pls/.m3u de emisoras) y deja leída la cabecera. 0 = bien. */
static int rad_conectar(rad_ses_t *s, conexao_t *con, http_resposta_t *resp, tls_config_t *cfg, bool *cfg_ok)
{
    char url[400];
    copia_texto(url, s->url, sizeof url);
    for (int salto = 0; salto < 8; salto++) {
        url_t u;
        if (!url_separar(url, &u)) { rad_msg(s, "Dirección no válida"); return -1; }
        if (u.https && !*cfg_ok) {
            char er[160];
            if (!tls_config_padrao(cfg, NULL, er, sizeof er)) { formatar_texto(s->msg, sizeof s->msg, "HTTPS: %s", er); return -1; }
            *cfg_ok = true;
        }
        if (s->cancelar) return -1;
        memset(con, 0, sizeof *con);
        con->fd = -1;
        int r = conexao_abrir(con, &u, cfg);
        if (r < 0) {
            formatar_texto(s->msg, sizeof s->msg, r == TLS_ERRO_CERT ? "Certificado no válido: %s" : "No se pudo conectar: %s", con->erro);
            conexao_fechar(con);
            return -1;
        }
        tempo_limite(con->fd, u.https ? 4000 : 800);
        char ped[700];
        int np = http_montar_pedido(&u, "GET", "Icy-MetaData: 1\r\nUser-Agent: Sintonia/1.0\r\nAccept: */*\r\n", ped, sizeof ped);
        if (np <= 0 || conexao_escrever(con, ped, (unsigned)np) < 0) { rad_msg(s, "No se pudo enviar el pedido"); conexao_fechar(con); return -1; }
        r = http_ler_cabecalho_de(resp, conexao_ler, con);
        if (r < 0) { rad_msg(s, "Respuesta no válida de la emisora"); conexao_fechar(con); return -1; }
        int c = resp->codigo;
        char loc[400];
        if ((c == 301 || c == 302 || c == 303 || c == 307 || c == 308) && http_campo(resp->cabecalho, "Location", loc, sizeof loc)) {
            conexao_fechar(con);
            char nueva[400];
            if (!url_juntar(url, loc, nueva, sizeof nueva)) { rad_msg(s, "Redirección no válida"); return -1; }
            copia_texto(url, nueva, sizeof url);
            continue;
        }
        if (c < 200 || c >= 300) {
            formatar_texto(s->msg, sizeof s->msg, "La emisora respondió %d", c);
            conexao_fechar(con);
            return -1;
        }
        char ct[80];
        ct[0] = 0;
        http_campo(resp->cabecalho, "Content-Type", ct, sizeof ct);
        if (sl_contiene(ct, "mpegurl") || sl_contiene(ct, "scpls")) {   /* una lista con la dirección de verdad */
            char *cuerpo = aloca(8192);
            if (!cuerpo) { rad_msg(s, "Sin memoria"); conexao_fechar(con); return -1; }
            int n = 0, k;
            while (n < 8191 && (k = http_ler_corpo(resp, cuerpo + n, 8191 - n)) > 0) n += k;
            conexao_fechar(con);
            sl_pista_t *l = aloca(sizeof *l * 4);
            if (!l) { rad_msg(s, "Sin memoria"); libera(cuerpo); return -1; }
            int cuenta = sl_lista_leer(cuerpo, (unsigned)n, NULL, l, 4);
            libera(cuerpo);
            bool ok = cuenta > 0 && sl_es_url(l[0].ruta);
            if (ok) copia_texto(url, l[0].ruta, sizeof url);
            libera(l);
            if (!ok) { rad_msg(s, "La lista de la emisora no tiene direcciones"); return -1; }
            continue;
        }
        if (sl_contiene(ct, "aac") || sl_contiene(ct, "ogg") || sl_contiene(ct, "opus") || sl_contiene(ct, "mp4") || sl_contiene(ct, "flac")) {
            formatar_texto(s->msg, sizeof s->msg, "Formato no soportado (%s): solo MP3", ct);
            conexao_fechar(con);
            return -1;
        }
        return 0;
    }
    rad_msg(s, "Demasiadas redirecciones");
    return -1;
}

static bool rad_escribir_trozo(rad_ses_t *s, const uint8_t *d, unsigned n)
{
    char ruta[100];
    rad_ruta(s, s->listos, ruta, sizeof ruta);
    int fd = abrir(ruta, ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ESCREVER);
    if (fd < 0) { rad_msg(s, "No se puede escribir en /tmp"); return false; }
    int r = escrever_tudo(fd, d, n);
    fechar(fd);
    if (r < 0) { rad_msg(s, "No se puede escribir en /tmp (¿sin espacio?)"); apagar(ruta); return false; }
    atomico_barreira();
    atomico_inc32(&s->listos);
    return true;
}

static void *rad_hilo(void *arg)
{
    rad_ses_t *s = arg;
    conexao_t *con = aloca_zerado(sizeof *con);
    http_resposta_t *resp = aloca_zerado(sizeof *resp);
    tls_config_t *cfg = aloca_zerado(sizeof *cfg);
    uint8_t *acum = aloca(ACUM_MAX), *rbuf = aloca(4096), *aud = aloca(4096);
    sl_icy_t *icy = aloca(sizeof *icy);
    bool cfg_ok = false, con_abierta = false;
    if (!con || !resp || !cfg || !acum || !rbuf || !aud || !icy) { rad_msg(s, "Sin memoria"); goto acabar; }
    if (rad_conectar(s, con, resp, cfg, &cfg_ok) < 0) goto acabar;
    con_abierta = true;
    {
        char v[100];
        unsigned metaint = 0;
        if (http_campo(resp->cabecalho, "icy-metaint", v, sizeof v)) { int m = texto_para_int(v); if (m > 0 && m <= 65536) metaint = (unsigned)m; }
        if (http_campo(resp->cabecalho, "icy-name", v, sizeof v)) {
            utf8_para_latin1(v, tam_texto(v), NULL);
            copia_texto(s->nombre, v, sizeof s->nombre);
        }
        sl_icy_iniciar(icy, metaint);
        s->estado = R_ALMACENANDO;
        unsigned nacum = 0, tos = 0;
        while (!s->cancelar) {
            if (s->listos - s->usados >= SEG_ADELANTE) { dormir(100); continue; }   /* el reproductor va por detrás: no se lee más */
            int n = http_ler_corpo(resp, rbuf, 4096);
            if (n == ERRO_TEMPO) {
                if (++tos > 12) { rad_msg(s, "La emisora no responde"); break; }
                continue;
            }
            if (n <= 0) { rad_msg(s, n == 0 ? "La emisora cortó la conexión" : "Error de red"); break; }
            tos = 0;
            unsigned k = sl_icy_filtrar(icy, rbuf, (unsigned)n, aud);
            if (icy->nuevo) {
                icy->nuevo = false;
                copia_texto(s->titulo, icy->titulo, sizeof s->titulo);
                s->tit_nuevo = 1;
            }
            if (nacum + k > ACUM_MAX) { rad_msg(s, "El flujo no parece MP3 (o la emisora usa otro formato)"); break; }
            memcpy(acum + nacum, aud, k);
            nacum += k;
            unsigned alvo = s->listos == 0 ? SEG_PRIMERO : SEG_RESTO;
            if (nacum >= alvo + 2500) {
                unsigned ini = 0, fin = sl_mp3_cortar(acum, nacum, alvo, &ini);
                if (fin) {
                    if (!rad_escribir_trozo(s, acum + ini, fin - ini)) break;
                    memmove(acum, acum + fin, nacum - fin);
                    nacum -= fin;
                } else if (nacum > alvo + 20000) { rad_msg(s, "El flujo no parece MP3 (o la emisora usa otro formato)"); break; }
            }
        }
    }
acabar:
    if (con_abierta && con) conexao_fechar(con);
    if (!s->msg[0] && !s->cancelar) rad_msg(s, "La emisora cortó la conexión");
    s->estado = R_ERROR;
    /* lo que no llegó a abrir el reproductor se borra */
    if (s->cancelar)
        for (uint32_t n = s->usados; n <= s->listos; n++) { char r[100]; rad_ruta(s, n, r, sizeof r); apagar(r); }
    if (con) libera(con);
    if (resp) libera(resp);
    if (cfg) libera(cfg);
    if (acum) libera(acum);
    if (rbuf) libera(rbuf);
    if (aud) libera(aud);
    if (icy) libera(icy);
    s->fin = 1;
    rad_soltar(s);
    return NULL;
}

static void rad_detener(void)
{
    if (!rad) return;
    rad->cancelar = 1;
    for (uint32_t n = rad->usados; n <= rad->listos; n++) { char r[100]; rad_ruta(rad, n, r, sizeof r); apagar(r); }
    rad_soltar(rad);
    rad = NULL;
}

static bool rad_iniciar(const char *url)
{
    rad_detener();
    rad_ses_t *s = aloca_zerado(sizeof *s);
    if (!s) { copia_texto(aviso, "Sin memoria para la radio", sizeof aviso); return false; }
    s->ref = 2;
    s->id = ++rad_cont;
    s->estado = R_CONECTANDO;
    copia_texto(s->url, url, sizeof s->url);
    if (!thread_create(rad_hilo, s, 0)) {
        libera(s);
        copia_texto(aviso, "No se pudo crear el hilo de red", sizeof aviso);
        return false;
    }
    rad = s;
    radio_titulo[0] = 0;
    radio_ms = 0;
    return true;
}

static const char *rad_texto_estado(void)
{
    if (!rad) return "";
    switch (rad->estado) {
    case R_CONECTANDO: return "Conectando";
    case R_ALMACENANDO: return "Cargando";
    case R_SONANDO: return "En directo";
    default: return "Error";
    }
}

/* ------------------------------------------------------------ reproducción */

static void cerrar_fluxo(void)
{
    if (fluxo >= 0) { fechar(fluxo); fluxo = -1; }
    fluxo_taxa = fluxo_can = 0;
}

static void cerrar_trozo(void)
{
    if (arq) { audio_arquivo_fechar(arq); arq = NULL; }
    if (seg_actual[0]) { apagar(seg_actual); seg_actual[0] = 0; }
}

static void cerrar_pista(void)
{
    cerrar_fluxo();
    cerrar_trozo();
    rad_detener();
    modo_radio = false;
    inf = NULL;
    estado = PARADO;
    n_dec = n_esc = base_q = 0;
    fim_dec = false;
    radio_titulo[0] = 0;
    desplaza = 0;
}

static void reiniciar_anel(void)
{
    base_q = audio_arquivo_posicao(arq);
    n_dec = n_esc = 0;
    fim_dec = false;
}

static bool abrir_fluxo(uint32_t taxa, uint32_t canais)
{
    fluxo = audio_abrir((int)taxa, (int)canais, 16);
    if (fluxo < 0) {
        if (fluxo == ERRO_NAO_EXISTE) copia_texto(aviso, "No hay tarjeta de sonido (solo el altavoz del PC): no se puede reproducir", sizeof aviso);
        else formatar_texto(aviso, sizeof aviso, "No se puede abrir el sonido: %s", nome_erro(fluxo));
        registrar("sintonia: sin sonido (%d)\n", fluxo);
        fluxo = -1;
        estado = PARADO;
        return false;
    }
    fluxo_taxa = taxa;
    fluxo_can = canais;
    audio_volume(fluxo, volume);
    return true;
}

static void iniciar(void)
{
    if (!arq || !inf) return;
    if (!abrir_fluxo(inf->taxa, inf->canais)) return;
    estado = TOCANDO;
    aviso[0] = 0;
    registrar("sintonia: reproduciendo %s\n", nome_curto(pistas[atual].ruta));
}

/* Etiquetas y duración que da el decodificador al abrir la pista (completan o corrigen las de la lista). */
static void aprender_de_inf(sl_pista_t *p)
{
    if (!inf) return;
    if (inf->duracao_ms) p->dur_s = (int)((inf->duracao_ms + 500) / 1000);
    if (!p->meta) {
        if (inf->titulo[0]) sl_copiar(p->titulo, sizeof p->titulo, inf->titulo);
        if (inf->artista[0]) sl_copiar(p->artista, sizeof p->artista, inf->artista);
        p->meta = 1;
    }
}

static void saltar(uint32_t ms)
{
    if (!arq || modo_radio) return;
    uint32_t d = duracao_ms();
    if (d && ms > d) ms = d;
    uint32_t desde = posicao_ms();
    if (fluxo >= 0) audio_controle(fluxo, AUDIO_DESCARTAR, 0);
    int r = audio_arquivo_buscar(arq, ms);
    if (r < 0) { formatar_texto(aviso, sizeof aviso, "No se puede saltar: %s", audio_erro_texto(r)); return; }
    reiniciar_anel();
    aviso[0] = 0;
    registrar("sintonia: salto a %u ms (cuadro %u) desde %u ms\n", ms, base_q, desde);
}

static bool abrir_pista(int i, bool tocar)
{
    cerrar_pista();
    atual = i;
    sel = i;
    sl_pista_t *p = &pistas[i];
    if (sl_es_url(p->ruta)) {
        if (!tocar) return true;   /* solo elegida: no se conecta hasta pulsar reproducir */
        modo_radio = true;
        if (!rad_iniciar(p->ruta)) { modo_radio = false; return false; }
        estado = TOCANDO;
        aviso[0] = 0;
        registrar("sintonia: sintonizando %s\n", p->ruta);
        return true;
    }
    int erro;
    arq = audio_arquivo_abrir(p->ruta, &erro);
    if (!arq) {
        formatar_texto(aviso, sizeof aviso, "%s: %s", nome_curto(p->ruta), audio_erro_texto(erro));
        registrar("sintonia: error al abrir %s (%d)\n", p->ruta, erro);
        return false;
    }
    inf = audio_arquivo_info(arq);
    aviso[0] = 0;
    reiniciar_anel();
    aprender_de_inf(p);
    registrar("sintonia: pista %d %s: %s %u Hz %u canales %u ms\n", i + 1, nome_curto(p->ruta), inf->nome_formato, inf->taxa, inf->canais, inf->duracao_ms);
    if (pista_pend == i && pos_pend_ms) { saltar(pos_pend_ms); }
    pista_pend = -1;
    pos_pend_ms = 0;
    if (tocar) iniciar();
    return true;
}

/* ---- orden aleatorio y siguiente pista ---- */

static void orden_nuevo(void)
{
    if (!semilla) semilla = tempo_ms() * 2654435761u + 12345u;
    sl_orden_aleatorio(orden, qtd, &semilla);
    pos_orden = 0;
    if (atual >= 0) {   /* la pista que suena va la primera */
        for (int k = 0; k < qtd; k++)
            if (orden[k] == atual) { orden[k] = orden[0]; orden[0] = atual; break; }
    }
    orden_ok = true;
}

/* Siguiente (dir = 1) o anterior (-1). automatico: acabó la pista (respeta «repetir»). -1 = no hay más. */
static int elegir_pista(int dir, bool automatico)
{
    if (!qtd) return -1;
    if (automatico && repetir == 2 && atual >= 0) return atual;
    if (aleatorio) {
        if (!orden_ok) orden_nuevo();
        int p = pos_orden + dir;
        if (p < 0) p = 0;
        if (p >= qtd) {
            if (repetir == 1 || !automatico) { int a = atual; atual = -1; orden_nuevo(); atual = a; p = 0; }
            else return -1;
        }
        pos_orden = p;
        return orden[p];
    }
    int i = atual + dir;
    if (i < 0 || i >= qtd) {
        if (repetir == 1 || !automatico) i = i < 0 ? qtd - 1 : 0;
        else return -1;
    }
    return i;
}

static void otra_pista(int dir, bool tocar, bool automatico)
{
    for (int tent = 0; tent < qtd; tent++) {
        int i = elegir_pista(dir, automatico);
        if (i < 0) {   /* fin de la lista: se queda detenido en la primera */
            cerrar_pista();
            if (qtd) abrir_pista(aleatorio && orden_ok ? orden[0] : 0, false);
            registrar("sintonia: fin de la lista\n");
            return;
        }
        if (abrir_pista(i, tocar)) { mostrar_pista(i); return; }
    }
}

static void parar(void)
{
    if (modo_radio) { cerrar_pista(); return; }
    if (!arq) return;
    uint32_t pos = posicao_ms();
    cerrar_fluxo();
    audio_arquivo_buscar(arq, 0);
    reiniciar_anel();
    estado = PARADO;
    registrar("sintonia: parado en %u ms\n", pos);
}

static void pausar(void)
{
    if (estado != TOCANDO) return;
    uint32_t pend = pendentes_q(), lo = n_dec > ANEL_Q ? n_dec - ANEL_Q : 0;
    n_esc = n_esc - pend < lo ? lo : n_esc - pend;
    if (fluxo >= 0) audio_controle(fluxo, AUDIO_DESCARTAR, 0);
    estado = PAUSADO;
    registrar("sintonia: pausa en %u ms\n", posicao_ms());
}

static void reanudar(void)
{
    estado = TOCANDO;
    registrar("sintonia: sigue en %u ms\n", posicao_ms());
}

static void reproducir(void)
{
    if (estado == PAUSADO) { reanudar(); return; }
    if (estado == TOCANDO) {   /* de nuevo desde el principio */
        if (!modo_radio) { saltar(0); }
        return;
    }
    if (!qtd) return;
    int i = sel >= 0 && sel != atual ? sel : atual >= 0 ? atual : 0;
    if (i == atual && arq && !modo_radio) iniciar();
    else abrir_pista(i, true);
}

static void alternar(void)
{
    if (estado == TOCANDO) pausar();
    else reproducir();
}

static void mudar_volume(int v)
{
    volume = v < 0 ? 0 : v > 100 ? 100 : v;
    if (fluxo >= 0) audio_volume(fluxo, volume);
}

static void saltar_relativo(int delta_ms)
{
    int p = (int)posicao_ms() + delta_ms;
    saltar(p < 0 ? 0 : (uint32_t)p);
}

/* El balance se aplica al entregar: el canal del lado contrario se atenúa (de 0 a -100 %). */
static void aplicar_balance(int16_t *d, const int16_t *o, uint32_t q)
{
    int gl = balance > 0 ? 256 * (100 - balance) / 100 : 256, gr = balance < 0 ? 256 * (100 + balance) / 100 : 256;
    for (uint32_t i = 0; i < q; i++) {
        d[2 * i] = (int16_t)((o[2 * i] * gl) >> 8);
        d[2 * i + 1] = (int16_t)((o[2 * i + 1] * gr) >> 8);
    }
}

static void llenar_anel(void)
{
    uint32_t canais = inf->canais, bq = canais * 2;
    for (int k = 0; k < 2 && !fim_dec && n_dec - n_esc < 8192; k++) {
        static int16_t tmp[PED_QUADROS * 2];
        int n = audio_arquivo_ler(arq, tmp, PED_QUADROS);
        if (n <= 0) {
            fim_dec = true;
            if (n < 0) {
                formatar_texto(aviso, sizeof aviso, "%s: %s", modo_radio ? "radio" : nome_curto(pistas[atual].ruta), audio_erro_texto(n));
                registrar("sintonia: error al decodificar (%d)\n", n);
            }
            break;
        }
        uint32_t ini = n_dec % ANEL_Q, a = (uint32_t)n < ANEL_Q - ini ? (uint32_t)n : ANEL_Q - ini;
        memcpy(anel + ini * canais, tmp, a * bq);
        if ((uint32_t)n > a) memcpy(anel, tmp + a * canais, ((uint32_t)n - a) * bq);
        n_dec += (uint32_t)n;
    }
}

static void entregar(void)
{
    uint32_t canais = inf->canais, bq = canais * 2;
    static int16_t ent[1024 * 2];
    while (n_esc < n_dec) {
        if (audio_pendente(fluxo) >= ADELANTE_MS) break;
        uint32_t ini = n_esc % ANEL_Q, q = n_dec - n_esc;
        if (q > ANEL_Q - ini) q = ANEL_Q - ini;
        const int16_t *origen = anel + ini * canais;
        if (canais == 2 && balance) {
            if (q > 1024) q = 1024;
            aplicar_balance(ent, origen, q);
            origen = ent;
        }
        int r = audio_escrever_sem_bloquear(fluxo, origen, q * bq);
        if (r <= 0) break;
        n_esc += (uint32_t)r / bq;
        if ((uint32_t)r < q * bq) break;
    }
}

/* Radio: abre el siguiente trozo que haya bajado el hilo de red. */
static bool abrir_trozo(void)
{
    if (rad->listos <= rad->usados) return false;
    char ruta[100];
    rad_ruta(rad, rad->usados, ruta, sizeof ruta);
    int erro;
    audio_arquivo_t *a = audio_arquivo_abrir(ruta, &erro);
    if (!a) { apagar(ruta); rad->usados++; registrar("sintonia: trozo de radio ilegible (%d)\n", erro); return false; }
    arq = a;
    copia_texto(seg_actual, ruta, sizeof seg_actual);
    inf_copia = *audio_arquivo_info(a);
    inf = &inf_copia;
    fim_dec = false;
    if (fluxo >= 0 && (fluxo_taxa != inf->taxa || fluxo_can != inf->canais)) cerrar_fluxo();
    if (fluxo < 0 && !abrir_fluxo(inf->taxa, inf->canais)) { cerrar_trozo(); return false; }
    if (rad->estado != R_ERROR) rad->estado = R_SONANDO;
    return true;
}

/* Se llama entre evento y evento. true = hay que redibujar todo (cambió la pista o el título). */
static bool bombear(void)
{
    bool cambio = false;
    if (modo_radio && rad) {
        if (rad->tit_nuevo) {
            copia_texto(radio_titulo, rad->titulo, sizeof radio_titulo);
            rad->tit_nuevo = 0;
            desplaza = 0;
            cambio = true;
            registrar("sintonia: radio: %s\n", radio_titulo);
        }
        if (!radio_titulo[0] && rad->nombre[0]) { copia_texto(radio_titulo, rad->nombre, sizeof radio_titulo); cambio = true; }
        if (rad->estado == R_ERROR && !arq && rad->listos <= rad->usados) {
            formatar_texto(aviso, sizeof aviso, "Radio: %s", rad->msg);
            registrar("sintonia: radio: %s\n", rad->msg);
            cerrar_pista();
            return true;
        }
        if (!arq && estado == TOCANDO && abrir_trozo()) cambio = true;
    }
    if (estado != TOCANDO || !arq || fluxo < 0) return cambio;
    llenar_anel();
    entregar();
    if (fim_dec && n_esc == n_dec) {
        if (modo_radio) { cerrar_trozo(); rad->usados++; return true; }   /* el siguiente trozo, sin cortar el sonido */
        if (audio_pendente(fluxo) <= 0) {   /* la pista terminó: la siguiente */
            registrar("sintonia: fin de la pista %d\n", atual + 1);
            otra_pista(1, true, true);
            return true;
        }
    }
    return cambio;
}

/* ---- visualizador: lo que suena ahora, sacado del anillo ---- */

static void recoger_muestras(void)
{
    memset(muestras, 0, sizeof muestras);
    if (!inf || (!arq && !modo_radio)) return;
    uint32_t canais = inf->canais, fin = n_esc - (estado == TOCANDO ? pendentes_q() : 0);
    uint32_t lo = n_dec > ANEL_Q ? n_dec - ANEL_Q : 0;
    for (uint32_t k = 0; k < VIS_N; k++) {
        if (fin + k < VIS_N) continue;
        uint32_t idx = fin - VIS_N + k;
        if (idx < lo || idx >= n_dec) continue;
        const int16_t *p = anel + (idx % ANEL_Q) * canais;
        muestras[k] = canais == 2 ? (int16_t)(((int)p[0] + p[1]) / 2) : p[0];
    }
}

static void actualizar_vis(void)
{
    if (estado == TOCANDO) {
        recoger_muestras();
        uint8_t niv[SL_BARRAS_MAX];
        int nb = ly.vis.w / 6;
        if (nb > SL_BARRAS_MAX) nb = SL_BARRAS_MAX;
        if (nb < 1) nb = 1;
        sl_espectro(muestras, nb, niv);
        for (int i = 0; i < nb; i++) {
            barras[i] = niv[i] >= barras[i] ? niv[i] : (uint8_t)(barras[i] > 6 ? barras[i] - 6 : 0);   /* sube de golpe, cae despacio */
            if (barras[i] >= picos[i]) picos[i] = barras[i];
            else picos[i] = (uint8_t)(picos[i] > 2 ? picos[i] - 2 : 0);
        }
    } else if (estado == PARADO) {
        for (int i = 0; i < SL_BARRAS_MAX; i++) {
            barras[i] = (uint8_t)(barras[i] > 8 ? barras[i] - 8 : 0);
            picos[i] = (uint8_t)(picos[i] > 4 ? picos[i] - 4 : 0);
        }
        memset(muestras, 0, sizeof muestras);
    }
}

static bool vis_en_reposo(void)
{
    if (estado != PARADO) return false;
    for (int i = 0; i < SL_BARRAS_MAX; i++) if (barras[i] || picos[i]) return false;
    return true;
}

/* ------------------------------------------------------------ lista de reproducción */

static char ult_dir[200] = "/";

static bool coincide(int i)
{
    char n[160];
    sl_nombre_pista(&pistas[i], n, sizeof n);
    return sl_contiene(n, busca.texto) || sl_contiene(pistas[i].ruta, busca.texto);
}

static void filtrar(void)
{
    nvista = 0;
    for (int i = 0; i < qtd; i++) if (!busca.texto[0] || coincide(i)) vista[nvista++] = i;
    copia_texto(busca_ant, busca.texto, sizeof busca_ant);
    ajustar_topo();
}

/* Lee las etiquetas y la duración de una pista de archivo sin abrir el decodificador (que solo admite un archivo). */
static void leer_meta(int i)
{
    sl_pista_t *p = &pistas[i];
    if (p->meta) return;
    p->meta = 1;
    if (sl_es_url(p->ruta)) return;
    static uint8_t cab[16384], aud[512], cola[128];
    int fd = abrir(p->ruta, ABRIR_LER);
    if (fd < 0) return;
    int n = ler(fd, cab, sizeof cab);
    info_arquivo_t ia;
    uint32_t tam = info_arquivo(p->ruta, &ia) >= 0 ? ia.tamanho : (uint32_t)(n > 0 ? n : 0);
    if (n < 12) { fechar(fd); return; }
    uint32_t id3 = sl_id3v2_tam(cab, (unsigned)n);
    const uint8_t *pa = NULL, *pc = NULL;
    unsigned na = 0;
    if (id3 >= (uint32_t)n && id3 < tam && posicionar(fd, (int)id3, POS_INICIO) >= 0) {
        int k = ler(fd, aud, sizeof aud);
        if (k > 0) { pa = aud; na = (unsigned)k; }
    }
    if (tam > 256 && posicionar(fd, (int)tam - 128, POS_INICIO) >= 0 && ler(fd, cola, 128) == 128) pc = cola;
    fechar(fd);
    sl_meta_t m;
    if (!sl_meta_leer(cab, (unsigned)n, pa, na, pc, tam, &m)) return;
    if (m.titulo[0]) { sl_copiar(p->titulo, sizeof p->titulo, m.titulo); sl_copiar(p->artista, sizeof p->artista, m.artista); }
    if (m.dur_s >= 0) p->dur_s = m.dur_s;
}

static bool hay_meta_pendiente(void)
{
    while (meta_sig < qtd && pistas[meta_sig].meta) meta_sig++;
    return meta_sig < qtd;
}

static bool agregar(const char *ruta)
{
    if (qtd >= MAX_PISTAS) { formatar_texto(aviso, sizeof aviso, "Lista llena (máximo %d pistas)", MAX_PISTAS); return false; }
    sl_pista_t *p = &pistas[qtd++];
    sl_pista_limpiar(p);
    sl_copiar(p->ruta, sizeof p->ruta, ruta);
    orden_ok = false;
    return true;
}

#define MAX_NOMES 160
static char nombres_dir[3][MAX_NOMES][64];

static int anadir_carpeta(const char *c, int prof)
{
    char (*ns)[64] = nombres_dir[prof];
    int n = 0, add = 0;
    entrada_pasta_t e;
    for (int pasada = 0; pasada < 2; pasada++) {
        n = 0;
        for (int k = 0; listar(c, k, &e) > 0 && n < MAX_NOMES; k++) {
            bool es_dir = e.tipo == TIPO_PASTA;
            if (e.nome[0] == '.' || es_dir != (pasada == 1)) continue;
            if (!es_dir && !extensao_audio(e.nome)) continue;
            int p = n++;
            while (p > 0 && compara_texto(ns[p - 1], e.nome) > 0) { copia_texto(ns[p], ns[p - 1], 64); p--; }
            copia_texto(ns[p], e.nome, 64);
        }
        for (int k = 0; k < n; k++) {
            char r[SL_RUTA_MAX];
            juntar_ruta(r, sizeof r, c, ns[k]);
            if (pasada == 0) { if (agregar(r)) add++; }
            else if (prof < 2) add += anadir_carpeta(r, prof + 1);
        }
    }
    return add;
}

/* Lee un archivo entero (hasta max-1 bytes) en buf; devuelve los bytes o -1. */
static int leer_archivo(const char *ruta, char *buf, int max)
{
    int fd = abrir(ruta, ABRIR_LER);
    if (fd < 0) return -1;
    int total = 0, r;
    while (total < max - 1 && (r = ler(fd, buf + total, (unsigned)(max - 1 - total))) > 0) total += r;
    fechar(fd);
    buf[total] = 0;
    return total;
}

static int anadir_lista(const char *ruta)
{
    int max = 160 * 1024;
    char *txt = aloca((size_t)max);
    sl_pista_t *tmp = aloca(sizeof *tmp * MAX_PISTAS);
    if (!txt || !tmp) { if (txt) libera(txt); if (tmp) libera(tmp); copia_texto(aviso, "Sin memoria", sizeof aviso); return 0; }
    int n = leer_archivo(ruta, txt, max), add = 0;
    if (n < 0) formatar_texto(aviso, sizeof aviso, "%s: no se puede leer", nome_curto(ruta));
    else {
        char base[200];
        dir_de(ruta, base, sizeof base);
        int k = sl_lista_leer(txt, (unsigned)n, base, tmp, MAX_PISTAS);
        for (int i = 0; i < k && qtd < MAX_PISTAS; i++) { pistas[qtd++] = tmp[i]; add++; }
        if (k > add) formatar_texto(aviso, sizeof aviso, "Lista llena: solo caben %d pistas", MAX_PISTAS);
        else if (!k) formatar_texto(aviso, sizeof aviso, "%s: la lista está vacía", nome_curto(ruta));
        orden_ok = false;
    }
    libera(txt);
    libera(tmp);
    return add;
}

/* Añade un archivo, una carpeta (con sus subcarpetas, hasta 3 niveles), una lista M3U/PLS o una emisora (URL). */
static int anadir_ruta(const char *c)
{
    if (sl_es_url(c)) return agregar(c) ? 1 : 0;
    info_arquivo_t i;
    if (info_arquivo(c, &i) < 0) { formatar_texto(aviso, sizeof aviso, "%s: no existe", c); return 0; }
    if (i.tipo == TIPO_PASTA) {
        int n = anadir_carpeta(c, 0);
        if (!n && !aviso[0]) formatar_texto(aviso, sizeof aviso, "No hay archivos de sonido en %s", c);
        copia_texto(ult_dir, c, sizeof ult_dir);
        return n;
    }
    dir_de(c, ult_dir, sizeof ult_dir);
    if (sl_tipo_lista(c)) return anadir_lista(c);
    return agregar(c) ? 1 : 0;
}

static void vaciar(void)
{
    cerrar_pista();
    qtd = 0; atual = -1; sel = -1; topo = 0;
    meta_sig = 0;
    orden_ok = false;
    aviso[0] = 0;
    filtrar();
    registrar("sintonia: lista vaciada\n");
}

static void quitar(void)
{
    if (sel < 0 || sel >= qtd) return;
    int i = sel;
    if (i == atual) { cerrar_pista(); atual = -1; }
    else if (i < atual) atual--;
    for (int k = i; k + 1 < qtd; k++) pistas[k] = pistas[k + 1];
    qtd--;
    if (meta_sig > i) meta_sig--;
    sel = qtd ? (i < qtd ? i : qtd - 1) : -1;
    orden_ok = false;
    filtrar();
    registrar("sintonia: pista quitada (quedan %d)\n", qtd);
}

static void mover(int dir)
{
    int a = sel, b = sel + dir;
    if (a < 0 || b < 0 || b >= qtd) return;
    sl_pista_t t = pistas[a];
    pistas[a] = pistas[b];
    pistas[b] = t;
    if (atual == a) atual = b; else if (atual == b) atual = a;
    sel = b;
    orden_ok = false;
    filtrar();
    mostrar_pista(sel);
}

static int cmp_ci(const char *a, const char *b)
{
    while (*a && sl_min((unsigned char)*a) == sl_min((unsigned char)*b)) { a++; b++; }
    return sl_min((unsigned char)*a) - sl_min((unsigned char)*b);
}

static int comparar(int modo, const sl_pista_t *a, const sl_pista_t *b)
{
    if (modo == 1) {   /* por duración (las desconocidas, al final) */
        int da = a->dur_s < 0 ? 0x7FFFFFF : a->dur_s, db = b->dur_s < 0 ? 0x7FFFFFF : b->dur_s;
        return da - db;
    }
    char na[160], nb[160];
    sl_nombre_pista(a, na, sizeof na);
    sl_nombre_pista(b, nb, sizeof nb);
    return cmp_ci(na, nb);
}

/* modo: 0 por nombre, 1 por duración, 2 invertir, 3 mezclar. La pista que suena y la elegida siguen siéndolo. */
static void ordenar(int modo)
{
    if (qtd < 2) return;
    char r_atual[SL_RUTA_MAX], r_sel[SL_RUTA_MAX];
    copia_texto(r_atual, atual >= 0 ? pistas[atual].ruta : "", sizeof r_atual);
    copia_texto(r_sel, sel >= 0 ? pistas[sel].ruta : "", sizeof r_sel);
    if (modo == 2) {
        for (int a = 0, b = qtd - 1; a < b; a++, b--) { sl_pista_t t = pistas[a]; pistas[a] = pistas[b]; pistas[b] = t; }
    } else if (modo == 3) {
        int *perm = aloca(sizeof *perm * (size_t)qtd);
        sl_pista_t *copia = aloca(sizeof *copia * (size_t)qtd);
        if (perm && copia) {
            if (!semilla) semilla = tempo_ms() * 2654435761u + 777u;
            sl_orden_aleatorio(perm, qtd, &semilla);
            memcpy(copia, pistas, sizeof *copia * (size_t)qtd);
            for (int i = 0; i < qtd; i++) pistas[i] = copia[perm[i]];
        }
        if (perm) libera(perm);
        if (copia) libera(copia);
    } else {   /* inserción: estable y la lista es corta */
        for (int i = 1; i < qtd; i++) {
            sl_pista_t t = pistas[i];
            int k = i;
            while (k > 0 && comparar(modo, &pistas[k - 1], &t) > 0) { pistas[k] = pistas[k - 1]; k--; }
            pistas[k] = t;
        }
    }
    for (int i = 0; i < qtd; i++) {
        if (r_atual[0] && !compara_texto(pistas[i].ruta, r_atual)) atual = i;
        if (r_sel[0] && !compara_texto(pistas[i].ruta, r_sel)) sel = i;
    }
    orden_ok = false;
    filtrar();
    registrar("sintonia: lista ordenada (%d)\n", modo);
}

static int emitir_fd(void *ctx, const char *s, unsigned n)
{
    return escrever_tudo(*(int *)ctx, s, n) < 0 ? -1 : 0;
}

static bool guardar_lista(const char *ruta, bool pls)
{
    int fd = abrir(ruta, ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ESCREVER);
    if (fd < 0) { formatar_texto(aviso, sizeof aviso, "%s: no se puede escribir (%s)", ruta, nome_erro(fd)); return false; }
    int r = sl_lista_escribir(pistas, qtd, pls, emitir_fd, &fd);
    fechar(fd);
    if (r < 0) { formatar_texto(aviso, sizeof aviso, "%s: error al escribir", ruta); return false; }
    registrar("sintonia: lista guardada en %s (%d pistas, %s)\n", ruta, qtd, pls ? "PLS" : "M3U");
    return true;
}

static void pegar_rutas(void)
{
    static char b[4096];
    colar_texto(b, sizeof b);
    int add = 0, antes = qtd;
    unsigned pos = 0, n = (unsigned)tam_texto(b);
    char lin[SL_RUTA_MAX];
    while (sl_linea(b, n, &pos, lin, sizeof lin)) {
        char *l = lin;
        while (*l == ' ' || *l == '\t') l++;
        unsigned k = (unsigned)tam_texto(l);
        if (k >= 2 && l[0] == '"' && l[k - 1] == '"') { l[k - 1] = 0; l++; }
        if (l[0]) add += anadir_ruta(l);
    }
    if (!add && !aviso[0]) copia_texto(aviso, "El portapapeles no tiene rutas de sonido", sizeof aviso);
    registrar("sintonia: %d pistas pegadas\n", add);
    if (add) { filtrar(); if (atual < 0) sel = antes; }
}

/* ------------------------------------------------------------ ecualizador del sistema (D201) */

static void eq_cargar(void)
{
    memset(eq_db, 0, sizeof eq_db);
    if (eq_conf_ler(eq_db) != 0) memset(eq_db, 0, sizeof eq_db);
}

static void eq_poner(int b, int v)
{
    v = v < -12 ? -12 : v > 12 ? 12 : v;
    if (eq_db[b] == v) return;
    eq_db[b] = v;
    eq_aplicar(eq_db);
}

static void eq_preset(int p)
{
    static const char *const n[4] = { "plano", "graves", "voz", "agudos" };
    if (eq_preajuste(n[p], eq_db) == 0) {
        eq_aplicar(eq_db);
        eq_conf_gravar(eq_db);
        registrar("sintonia: ecualizador %s\n", n[p]);
    }
}

/* ------------------------------------------------------------ estado y tema guardados */

static void asegurar_conf(void)
{
    criar_pasta("/home/.config");
    criar_pasta(DIR_CONF);
}

static void cargar_tema(void)
{
    sl_tema_defecto(&tema);
    static char b[2048];
    int n = leer_archivo(DIR_CONF "/tema.conf", b, sizeof b);
    if (n >= 0) { int ok = sl_tema_leer(&tema, b, (unsigned)n); registrar("sintonia: tema.conf, %d líneas válidas\n", ok); return; }
    /* la primera vez se deja uno de ejemplo, comentado, para que se pueda retocar */
    int fd = abrir(DIR_CONF "/tema.conf", ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ESCREVER);
    if (fd < 0) return;
    escrever_texto(fd,
        "# Tema de Sintonia (colores #RRGGBB y disposicion). Borra el signo # de una linea para usarla.\n"
        "# fondo=#DDE1E5\n# panel=#0E1A14\n# panel2=#1B2B22\n# texto=#1E2328\n# texto_fraco=#5A646E\n"
        "# acento=#2FD070\n# barras=#39E07F\n# picos=#F2E85A\n# osciloscopio=#7DF0AE\n# seleccion=#2A6FD8\n# lista_fondo=#FFFFFF\n"
        "# visualizador=barras      (barras, osciloscopio o ambos)\n"
        "# botones=abajo            (arriba o abajo: la fila de botones encima o debajo de la barra de posicion)\n"
        "# lista=si                 (si o no: la lista de reproduccion al abrir)\n"
        "# eq=no                    (si o no: el ecualizador al abrir)\n");
    fechar(fd);
}

static void guardar_estado(void)
{
    asegurar_conf();
    int fd = abrir(DIR_CONF "/estado.conf", ABRIR_CRIAR | ABRIR_TRUNCAR | ABRIR_ESCREVER);
    if (fd < 0) return;
    char t[400];
    uint32_t pos = modo_radio || !arq ? 0 : posicao_ms();
    formatar_texto(t, sizeof t,
                   "volumen=%d\nbalance=%d\naleatorio=%d\nrepetir=%d\npista=%d\nposicion=%u\nmini=%d\nlista=%d\neq=%d\nvis=%d\nrestante=%d\nalt_lista=%d\n",
                   volume, balance, aleatorio, repetir, atual, (unsigned)pos, mini, ver_lista, ver_eq, vis_modo, mostrar_restante, alt_lista);
    escrever_texto(fd, t);
    fechar(fd);
    guardar_lista(DIR_CONF "/lista.m3u", false);
}

static void cargar_estado(bool con_lista)
{
    static char b[1024];
    int n = leer_archivo(DIR_CONF "/estado.conf", b, sizeof b);
    if (n >= 0) {
        unsigned pos = 0;
        char lin[80], k[24], v[40];
        while (sl_linea(b, (unsigned)n, &pos, lin, sizeof lin)) {
            if (!sl_clave_valor(lin, k, sizeof k, v, sizeof v)) continue;
            int x;
            if (!sl_leer_int(v, &x)) continue;
            if (!compara_texto(k, "volumen")) volume = x < 0 ? 0 : x > 100 ? 100 : x;
            else if (!compara_texto(k, "balance")) balance = x < -100 ? -100 : x > 100 ? 100 : x;
            else if (!compara_texto(k, "aleatorio")) aleatorio = x != 0;
            else if (!compara_texto(k, "repetir")) repetir = x < 0 || x > 2 ? 0 : x;
            else if (!compara_texto(k, "pista")) pista_pend = x;
            else if (!compara_texto(k, "posicion")) pos_pend_ms = x > 0 ? (uint32_t)x : 0;
            else if (!compara_texto(k, "mini")) mini = x != 0;
            else if (!compara_texto(k, "lista")) ver_lista = x != 0;
            else if (!compara_texto(k, "eq")) ver_eq = x != 0;
            else if (!compara_texto(k, "vis")) vis_modo = x < 0 || x > 2 ? 0 : x;
            else if (!compara_texto(k, "restante")) mostrar_restante = x != 0;
            else if (!compara_texto(k, "alt_lista")) alt_lista = x < ALT_LISTA_MIN ? ALT_LISTA_MIN : x > 400 ? 400 : x;
        }
    }
    if (con_lista) {
        int add = anadir_lista(DIR_CONF "/lista.m3u");
        registrar("sintonia: lista recordada, %d pistas\n", add);
    }
    if (pista_pend >= qtd) { pista_pend = -1; pos_pend_ms = 0; }
}

/* ------------------------------------------------------------ acciones y eventos */

static void paneles(void)
{
    estilo_ventana();
    cambiar_tam(j->larg, alt_normal());
    registrar("sintonia: paneles eq=%d lista=%d\n", ver_eq, ver_lista);
}

static void alternar_mini(void)
{
    mini = !mini;
    if (mini) {
        if (busca.foco) busca.foco = false;
        estilo_ventana();
        cambiar_tam(LARG_MINI, ALT_MINI);
    } else {
        estilo_ventana();
        cambiar_tam(LARG_INICIO, alt_normal());
    }
    registrar("sintonia: modo %s\n", mini ? "mini" : "normal");
}

/* Pregunta un texto con el sonido en pausa mientras el diálogo está abierto. */
static bool preguntar(const char *titulo, const char *pregunta, char *c, int max)
{
    bool tocaba = estado == TOCANDO;
    if (tocaba) pausar();
    bool ok = pedir_texto(titulo, pregunta, c, max) && c[0];
    if (tocaba && estado == PAUSADO) reanudar();
    return ok;
}

static void accion_abrir(bool anadir)
{
    char c[200];
    copia_texto(c, ult_dir, sizeof c);
    if (!preguntar(anadir ? "Añadir a la lista" : "Abrir", "Archivo, carpeta, lista M3U/PLS o URL de una emisora:", c, sizeof c)) return;
    if (!anadir) vaciar();
    int antes = qtd;
    aviso[0] = 0;
    int n = anadir_ruta(c);
    filtrar();
    registrar("sintonia: %d pistas añadidas desde %s\n", n, c);
    if (n && (!anadir || atual < 0)) { abrir_pista(antes, true); mostrar_pista(antes); }
    else if (n && sel < 0) sel = antes;
}

static void accion_radio(void)
{
    char c[200];
    copia_texto(c, "http://", sizeof c);
    if (!preguntar("Emisora de radio", "Dirección de la emisora (http:// o https://, flujo MP3):", c, sizeof c)) return;
    if (!sl_es_url(c)) { copia_texto(aviso, "La dirección debe empezar por http:// o https://", sizeof aviso); return; }
    int antes = qtd;
    if (anadir_ruta(c)) {
        sl_pista_t *p = &pistas[antes];
        p->meta = 1;
        filtrar();
        if (atual < 0 || estado == PARADO) { abrir_pista(antes, true); mostrar_pista(antes); }
        registrar("sintonia: emisora añadida %s\n", c);
    }
}

static void accion_guardar(bool pls)
{
    char c[200];
    formatar_texto(c, sizeof c, "%s%slista.%s", ult_dir, compara_texto(ult_dir, "/") ? "/" : "", pls ? "pls" : "m3u");
    if (!qtd) { copia_texto(aviso, "No hay nada que guardar: la lista está vacía", sizeof aviso); return; }
    if (!preguntar(pls ? "Guardar lista PLS" : "Guardar lista M3U", "Ruta del archivo:", c, sizeof c)) return;
    if (guardar_lista(c, pls)) formatar_texto(aviso, sizeof aviso, "Lista guardada en %s", c);
}

static void accion_abrir_lista(void)
{
    char c[200];
    copia_texto(c, ult_dir, sizeof c);
    if (!preguntar("Abrir lista", "Archivo .m3u o .pls (se añade a la lista):", c, sizeof c)) return;
    aviso[0] = 0;
    int antes = qtd, n = anadir_ruta(c);
    filtrar();
    if (n && atual < 0) { abrir_pista(antes, false); mostrar_pista(antes); }
}

static void mover_sel(int d)
{
    if (!nvista) return;
    int k = -1;
    for (int i = 0; i < nvista; i++) if (vista[i] == sel) { k = i; break; }
    k += d;
    if (k < 0) k = 0;
    if (k >= nvista) k = nvista - 1;
    sel = vista[k];
    mostrar_pista(sel);
}

static void accion(int id)
{
    aviso[0] = 0;
    switch (id) {
    case C_ANT: otra_pista(-1, estado != PARADO, false); break;
    case C_SIG: otra_pista(1, estado != PARADO, false); break;
    case C_PLAY: reproducir(); break;
    case C_PAUSA: if (estado == TOCANDO) pausar(); else if (estado == PAUSADO) reanudar(); break;
    case C_PARAR: parar(); break;
    case C_ABRIR: accion_abrir(false); break;
    case C_ALEA: aleatorio = !aleatorio; orden_ok = false; registrar("sintonia: aleatorio %d\n", aleatorio); break;
    case C_REP: repetir = (repetir + 1) % 3; registrar("sintonia: repetir %d\n", repetir); break;
    case C_EQ: ver_eq = !ver_eq; paneles(); break;
    case C_LISTA: ver_lista = !ver_lista; paneles(); break;
    case C_MINI: alternar_mini(); break;
    case C_QUITAR: quitar(); break;
    case C_SUBIR: mover(-1); break;
    case C_BAJAR: mover(1); break;
    default:
        if (id >= C_PRE0 && id < C_PRE0 + 4) eq_preset(id - C_PRE0);
        break;
    }
    dibujar_todo();
}

static void menu_del_boton(int id)
{
    for (int i = 0; i < nctl; i++) {
        if (ctl[i].id != id) continue;
        int x = ctl[i].r.x, y = ctl[i].r.y + ctl[i].r.h;
        if (id == C_ADD) menu_contextual(j->id, x, y, "Archivo, carpeta o lista...\nEmisora de radio (URL)...\nPegar rutas del portapapeles\n-\nVaciar la lista", 1);
        else if (id == C_ORDENAR) menu_contextual(j->id, x, y, "Por nombre\nPor duración\nInvertir el orden\nMezclar", 2);
        else if (id == C_LISTAS) menu_contextual(j->id, x, y, "Abrir lista M3U/PLS...\nGuardar como M3U...\nGuardar como PLS...", 3);
        return;
    }
}

static void tratar_menu(const evento_t *e)
{
    if (e->x < 0) return;
    aviso[0] = 0;
    if (e->tecla == 1) {
        if (e->x == 0) accion_abrir(true);
        else if (e->x == 1) accion_radio();
        else if (e->x == 2) pegar_rutas();
        else if (e->x == 4 && qtd && confirmar("Vaciar la lista", "¿Quitar todas las pistas de la lista?")) vaciar();
    } else if (e->tecla == 2) ordenar(e->x);
    else if (e->tecla == 3) {
        if (e->x == 0) accion_abrir_lista();
        else accion_guardar(e->x == 2);
    }
    filtrar();
    dibujar_todo();
}

static void tecla(unsigned t, unsigned mod)
{
    if (busca.foco && ver_lista && !mini) {
        if (t == TECLA_ESC) { busca.foco = false; caixa_definir(&busca, ""); filtrar(); dibujar_todo(); return; }
        evento_t ev = { .tipo = EV_TECLA, .tecla = t };
        if (caixa_tratar(j, &busca, &ev)) busca.foco = false;   /* Intro: sigue filtrando y devuelve el teclado */
        if (compara_texto(busca.texto, busca_ant) != 0) { filtrar(); topo = 0; }
        dibujar_todo();
        return;
    }
    aviso[0] = 0;
    bool ctrl = (mod & 2) != 0;
    switch (t) {
    case 'z': case 'Z': accion(C_ANT); return;
    case 'x': case 'X': reproducir(); break;
    case 'c': case 'C': accion(C_PAUSA); return;
    case 'v': case 'V': accion(C_PARAR); return;
    case 'b': case 'B': accion(C_SIG); return;
    case ' ': alternar(); break;
    case TECLA_DIREITA: saltar_relativo(SALTO_MS); break;
    case TECLA_ESQUERDA: saltar_relativo(-SALTO_MS); break;
    case TECLA_CIMA: if (ver_lista && !mini && !ctrl) mover_sel(-1); else { mudar_volume(volume + 5); dib_volumen(); jan_mostrar(j); return; } break;
    case TECLA_BAIXO: if (ver_lista && !mini && !ctrl) mover_sel(1); else { mudar_volume(volume - 5); dib_volumen(); jan_mostrar(j); return; } break;
    case TECLA_PGUP: mover_sel(-filas_visibles()); break;
    case TECLA_PGDN: mover_sel(filas_visibles()); break;
    case TECLA_HOME: if (nvista) { sel = vista[0]; mostrar_pista(sel); } break;
    case TECLA_END: if (nvista) { sel = vista[nvista - 1]; mostrar_pista(sel); } break;
    case '+': case '=': mudar_volume(volume + 5); dib_volumen(); jan_mostrar(j); return;
    case '-': mudar_volume(volume - 5); dib_volumen(); jan_mostrar(j); return;
    case 's': case 'S': accion(C_ALEA); return;
    case 'r': case 'R': accion(C_REP); return;
    case 'e': case 'E': accion(C_EQ); return;
    case 'l': case 'L': accion(C_LISTA); return;
    case 'm': case 'M': accion(C_MINI); return;
    case 't': case 'T': mostrar_restante = !mostrar_restante; break;
    case 'a': case 'A': accion_abrir(true); break;
    case 'o': case 'O': case 15: accion_abrir(false); break;
    case TECLA_DELETE: quitar(); break;
    case '\n': if (sel >= 0 && sel < qtd) abrir_pista(sel, true); break;
    case 22: pegar_rutas(); break;
    case 19: accion_guardar(false); break;
    case '/': case 6:
        if (mini) alternar_mini();
        if (!ver_lista) { ver_lista = true; paneles(); }
        busca.foco = true;
        break;
    default: return;
    }
    dibujar_todo();
}

static uint32_t ms_de_x(int x)
{
    int d = x - ly.seek.x;
    if (d < 0) d = 0;
    if (d > ly.seek.w) d = ly.seek.w;
    uint32_t dur = duracao_ms();
    return (uint32_t)d * (dur / 64) / (uint32_t)ly.seek.w * 64;
}

static int valor_x(const rc_t *r, int x, int max)
{
    int d = x - r->x;
    if (d < 0) d = 0;
    if (d > r->w) d = r->w;
    return d * max / r->w;
}

static void arrastre_eq(int b, int y)
{
    rc_t r = ly.banda[b];
    int v = 12 - (y - r.y) * 24 / r.h;
    eq_poner(b, v);
    dib_eq();
    jan_mostrar_area(j, ly.eq.x, ly.eq.y, ly.eq.w, ly.eq.h);
}

static void ajustar_balance(int v)
{
    balance = v < -100 ? -100 : v > 100 ? 100 : v;
    if (balance > -6 && balance < 6) balance = 0;   /* un imán en el centro */
    dib_volumen();
    jan_mostrar_area(j, 0, 134, j->larg, 28);
}

static void clic(const evento_t *e)
{
    if (!(e->botoes & 1)) return;
    for (int i = 0; i < nctl; i++) {
        if (!dentro(&ctl[i].r, e->x, e->y)) continue;
        int id = ctl[i].id;
        if (id == C_ADD || id == C_ORDENAR || id == C_LISTAS) menu_del_boton(id);
        else accion(id);
        return;
    }
    if (!mini && ver_eq)
        for (int b = 0; b < EQ_BANDAS; b++) {
            rc_t r = { ly.banda[b].x - 8, ly.banda[b].y - 6, ly.banda[b].w + 16, ly.banda[b].h + 12 };
            if (dentro(&r, e->x, e->y)) { arrastrando = ARR_EQ; arr_banda = b; arrastre_eq(b, e->y); return; }
        }
    rc_t s = { ly.seek.x - 4, ly.seek.y - 6, ly.seek.w + 8, ly.seek.h + 12 };
    if (dentro(&s, e->x, e->y) && duracao_ms()) {
        arrastrando = ARR_SEEK;
        previa_ms = ms_de_x(e->x);
        redib_tiempo();
        return;
    }
    if (!mini) {
        rc_t v = { ly.vol.x - 4, ly.vol.y - 4, ly.vol.w + 8, ly.vol.h + 8 }, b = { ly.bal.x - 4, ly.bal.y - 4, ly.bal.w + 8, ly.bal.h + 8 };
        if (dentro(&v, e->x, e->y)) { arrastrando = ARR_VOL; mudar_volume(valor_x(&ly.vol, e->x, 100)); dib_volumen(); jan_mostrar_area(j, 0, 134, j->larg, 28); return; }
        if (dentro(&b, e->x, e->y)) { arrastrando = ARR_BAL; ajustar_balance(valor_x(&ly.bal, e->x, 200) - 100); return; }
    }
    if (dentro(&ly.tiempo, e->x, e->y)) { mostrar_restante = !mostrar_restante; redib_tiempo(); return; }
    if (dentro(&ly.vis, e->x, e->y)) { vis_modo = (vis_modo + 1) % 3; redib_vis(); return; }
    if (ver_lista && !mini && dentro(&ly.lista, e->x, e->y)) {
        int fila = topo + (e->y - ly.lista.y - 1) / ALTO_FILA;
        if (fila >= 0 && fila < nvista) {
            sel = vista[fila];
            if (e->cliques == 2) { abrir_pista(sel, true); dibujar_todo(); }
            else redib_lista();
        }
    }
}

/* Devuelve false para cerrar. */
static bool tratar(evento_t *e)
{
    if (e->tipo == EV_FECHAR) return false;
    if (e->tipo == EV_REDIMENSIONAR) {
        if (jan_redimensionar(j, e)) {
            if (!mini && ver_lista) {
                alt_lista = j->alt - ALT_BASE - (ver_eq ? ALT_EQ : 0);
                if (alt_lista < ALT_LISTA_MIN) alt_lista = ALT_LISTA_MIN;
            }
            dibujar_todo();
        }
        return true;
    }
    if (e->tipo == EV_MENU) { tratar_menu(e); return true; }
    if (e->tipo == EV_TECLA) { tecla(e->tecla, e->modificadores); return true; }
    if (e->tipo == EV_RODA) {
        int n = (int)e->cliques;
        if (ver_lista && !mini && dentro(&ly.lista, e->x, e->y)) { topo += n * RODA_LINHAS; ajustar_topo(); redib_lista(); }
        else { mudar_volume(volume - n * 5); if (!mini) { dib_volumen(); jan_mostrar_area(j, 0, 134, j->larg, 28); } }
        return true;
    }
    if (e->tipo == EV_CLIQUE && ver_lista && !mini) {
        caixa_tratar(j, &busca, e);   /* da o quita el foco al cuadro de búsqueda */
        jan_mostrar_area(j, busca.x, busca.y, busca.l, LETRA_A + 8);
    }
    if (e->tipo == EV_CLIQUE) { clic(e); return true; }
    if (e->tipo == EV_MOVER && (e->botoes & 1)) {
        if (arrastrando == ARR_SEEK) { previa_ms = ms_de_x(e->x); redib_tiempo(); }
        else if (arrastrando == ARR_VOL) { mudar_volume(valor_x(&ly.vol, e->x, 100)); dib_volumen(); jan_mostrar_area(j, 0, 134, j->larg, 28); }
        else if (arrastrando == ARR_BAL) ajustar_balance(valor_x(&ly.bal, e->x, 200) - 100);
        else if (arrastrando == ARR_EQ) arrastre_eq(arr_banda, e->y);
        return true;
    }
    if (e->tipo == EV_SOLTOU && arrastrando) {
        int a = arrastrando;
        arrastrando = ARR_NADA;
        if (a == ARR_SEEK) { saltar(ms_de_x(e->x)); redib_tiempo(); }
        else if (a == ARR_EQ) { eq_conf_gravar(eq_db); registrar("sintonia: ecualizador %d %d %d %d %d\n", eq_db[0], eq_db[1], eq_db[2], eq_db[3], eq_db[4]); }
        return true;
    }
    return true;
}

/* ------------------------------------------------------------ principal */

int principal(int argc, char **argv)
{
    asegurar_conf();
    cargar_tema();
    ver_lista = tema.lista;
    ver_eq = tema.eq;
    vis_modo = tema.visualizador;
    cargar_estado(argc <= 1);
    aviso[0] = 0;
    memset(&busca, 0, sizeof busca);
    j = jan_abrir(mini ? "Sintonía (mini)" : "Sintonía", mini ? LARG_MINI : LARG_INICIO, mini ? ALT_MINI : alt_normal());
    if (!j) return 1;
    estilo_ventana();
    calcular_layout();
    eq_cargar();
    for (int i = 1; i < argc; i++) anadir_ruta(argv[i]);
    filtrar();
    if (argc > 1 && qtd) { abrir_pista(0, true); }
    else if (pista_pend >= 0 && pista_pend < qtd) {   /* recuerda dónde se quedó: elegida y lista para seguir */
        int p = pista_pend;
        abrir_pista(p, false);
        if (arq && pos_pend_ms) { saltar(pos_pend_ms); pos_pend_ms = 0; }
        pista_pend = -1;
        mostrar_pista(p);
    }
    mostrar_pista(atual);
    dibujar_todo();
    registrar("sintonia: pronto (%d pistas, volumen %d)\n", qtd, volume);

    evento_t e;
    bool continuar = true;
    uint32_t rad_est_vis = 99;
    while (continuar) {
        bool animado = estado == TOCANDO || !vis_en_reposo() || hay_meta_pendiente();
        if (esperar_evento(j->id, &e, animado ? 15 : -1) == 1) {
            continuar = tratar(&e);
            while (continuar && proximo_evento(j->id, &e, false) == 1) continuar = tratar(&e);
        }
        if (!continuar) break;
        uint32_t ahora = tempo_ms();
        if (estado == TOCANDO && modo_radio && t_tick) radio_ms += ahora - t_tick;
        t_tick = ahora;
        if (bombear()) dibujar_todo();
        if (estado == TOCANDO || !vis_en_reposo()) {
            if (ahora - t_vis >= 50) {
                t_vis = ahora;
                actualizar_vis();
                redib_vis();
                if (modo_radio) { dib_seek(); mostrar_rc(&ly.seek); }
                if (modo_radio && rad && rad->estado != rad_est_vis) { rad_est_vis = rad->estado; dib_info(); jan_mostrar_area(j, ly.info1.x - 2, ly.info1.y - 1, ly.info1.w + 4, 30); }
            }
        }
        if (estado == TOCANDO && ahora - t_titulo >= 160) {
            t_titulo = ahora;
            char t[300];
            texto_titulo(t, sizeof t);
            if ((int)tam_texto(t) > ly.titulo.w / LETRA_L) { desplaza++; redib_titulo(); }
        }
        if (estado == TOCANDO && arrastrando != ARR_SEEK && posicao_ms() / 1000 != seg_mostrado) redib_tiempo();
        /* etiquetas de la lista, de una en una y sin apurar al sonido */
        if (hay_meta_pendiente() && (estado != TOCANDO || (fluxo >= 0 && audio_pendente(fluxo) > 250))) {
            leer_meta(meta_sig++);
            if (!hay_meta_pendiente() || (meta_sig & 7) == 0) { filtrar(); redib_lista(); }
        }
    }
    registrar("sintonia: cerrado\n");
    guardar_estado();
    cerrar_pista();
    jan_fechar(j);
    return 0;
}
