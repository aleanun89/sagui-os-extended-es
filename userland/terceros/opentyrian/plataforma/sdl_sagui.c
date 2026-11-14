/* sdl_sagui.c - la SDL 1.2 del SaguiOS para el motor de OpenTyrian (D156).
 *
 * El juego (codigo de GPL-2.0 que no se toca) habla SDL 1.2. Aqui se
 * implementa lo que el motor usa de verdad, sobre la API del sistema:
 *
 *   - video:    una ventana del tamano que pide el juego con un pixel por
 *               pantalla; la superficie de SDL es la misma zona de memoria
 *               que da el kernel, y SDL_Flip la publica. Con SDL_FULLSCREEN
 *               (o F11, o Alt+Intro) la ventana pasa a pantalla completa y
 *               la superficie del juego se escala a 4:3 (D204).
 *   - teclado:  los eventos de ventana (EV_TECLA) se convierten en
 *               SDL_KEYDOWN. El kernel no avisa de quando se suelta una
 *               tecla, asi que se consulta teclas_seguradas() para
 *               fabricar los SDL_KEYUP que el motor necesita.
 *   - raton:    EV_MOVER, EV_CLIQUE y EV_SOLTOU.
 *   - reloj:    SDL_GetTicks y SDL_Delay sobre tempo_ms() y dormir().
 *   - audio:    el motor mezcla en un callback; aqui se llama al callback y
 *               el resultado se escribe en el flujo de sonido del sistema.
 *               Sin placa de sonido, el juego funciona igual (mudo).
 *   - joystick: no hay joystick en el SaguiOS: se informa que no hay ninguno.
 *
 * Ver ORIGEM.md: esto es codigo nuestro, no de la SDL.
 */
#include "SDL.h"

#include "sagui.h"
#include "gui.h"

/* O teclado.h do kernel (com TECLA_F1..TECLA_F12) nao entra no userland, e
 * o abi.h so declara TECLA_F11. Os valores sao os do enum do kernel. */
#ifndef TECLA_F1
#define TECLA_F1   0x10A
#define TECLA_F12  0x115
#endif
#ifndef TECLA_IMPRPANT
#define TECLA_IMPRPANT 0x118
#endif

#define TAM_FILA 64      /* eventos de SDL en la cola */
#define TAM_TECLAS 48    /* teclas que se vigilan para saber cuando se sueltan */

static int janela_id = -1;
static uint32_t *janela_px = NULL;
static int janela_l = 0, janela_a = 0;

/* ---- pantalla completa (D204) ----
 * Con la ventana del tamano de la superficie (modo ventana) el juego pinta
 * directo en la memoria de la ventana. Si la ventana es de otro tamano
 * (pantalla completa, o la ventana agrandada a mano) el juego pinta en
 * sup_buf y SDL_Flip lo copia escalado, a 4:3, al centro de la ventana con
 * bandas negras. Es vecino mas proximo con tablas de columnas precalculadas y
 * las filas repetidas se copian enteras (sin float por pixel). */
#define ESTILO_BASE (JAN_REDIMENSIONAVEL | JAN_TELA_CHEIA_OK | JAN_ESC_LIVRE)
bool sagui_cheia_inicial = false;   /* --pantalla-completa: la primera ventana nace a pantalla completa */
extern bool fullscreen_enabled;     /* jogo/video.c: lo que el motor guarda en su configuracion */
static bool em_cheia = false;
static int sup_l = 0, sup_a = 0;    /* tamano de la superficie que ve el juego */
static uint32_t *sup_buf = NULL;    /* memoria de esa superficie (siempre reservada) */
static bool escalando = false;      /* true: la ventana no es del tamano de la superficie */
static int dest_x, dest_y, dest_l, dest_a;
static int *tab_col = NULL, *tab_fila = NULL;
static int tab_col_n = 0, tab_fila_n = 0;

static SDL_Surface video_superficie;
static SDL_PixelFormat video_formato;
static SDL_Color video_paleta[256];
static SDL_Palette video_paleta_info;

static SDL_Event fila[TAM_FILA];
static int fila_ini = 0, fila_n = 0;

static Uint32 sub_sistemas = 0;
static SDLMod mods_atual = KMOD_NONE;

/* ---- teclas: para saber cuando se sueltan ---- */
typedef struct {
    SDLKey      sim;
    unsigned char sc;    /* codigo de rastreo (set 1); 128+ = tecla extendida */
    bool        down;
} tecla_vigiada_t;

static tecla_vigiada_t teclas[TAM_TECLAS];
static int n_teclas = 0;

/* ---- audio ---- */
static SDL_AudioSpec audio_pedido;
static SDL_AudioCallback audio_cb = NULL;
static void *audio_userdata = NULL;
static int audio_fd = -1;
static bool audio_pausado = true;
static bool audio_aberto = false;
static unsigned char audio_buf[8192];

static bool primeiro_quadro = false;

static void bombear_audio(void);

/* ------------------------------------------------------------------ utiles */

static const char mensagem_erro[] = "erro da plataforma SDL do SaguiOS";

const char *SDL_GetError(void) { return mensagem_erro; }

void *SDL_malloc(size_t n) { return aloca(n); }
void  SDL_free(void *p) { libera(p); }

int SDL_Init(Uint32 flags)
{
    sub_sistemas |= flags;
    return 0;
}

int SDL_InitSubSystem(Uint32 flags)
{
    sub_sistemas |= flags;
    return 0;
}

void SDL_QuitSubSystem(Uint32 flags) { sub_sistemas &= ~flags; }

void SDL_Quit(void) { sub_sistemas = 0; }

Uint32 SDL_WasInit(Uint32 flags)
{
    if (flags == 0) return sub_sistemas;
    return (sub_sistemas & flags) ? flags : 0;
}

/* ------------------------------------------------------------------ video */

int SDL_VideoModeOK(int largura, int altura, int bits, Uint32 flags)
{
    (void)flags;
    if (largura <= 0 || altura <= 0) return 0;
    if (bits != 8 && bits != 16 && bits != 32) return 0;
    return bits;
}

static void video_formato_preparar(SDL_PixelFormat *f, int bits)
{
    f->palette = NULL;
    f->BitsPerPixel = (Uint8)bits;
    f->BytesPerPixel = (Uint8)(bits / 8);
    f->Rloss = f->Gloss = f->Bloss = f->Aloss = 0;
    f->Rshift = 16; f->Gshift = 8; f->Bshift = 0; f->Ashift = 0;
    f->Rmask = 0xFF0000u; f->Gmask = 0x00FF00u; f->Bmask = 0x0000FFu; f->Amask = 0;
    f->colormap = 0;
}

/* Calcula el rectangulo destino (4:3, centrado) y las tablas de escalado. */
static void preparar_escala(void)
{
    escalando = (janela_l != sup_l || janela_a != sup_a);
    if (!escalando) { video_superficie.pixels = janela_px; return; }
    video_superficie.pixels = sup_buf;
    /* 320x200 es 16:10 con pixeles cuadrados, pero la VGA lo mostraba en una
     * pantalla 4:3 (pixeles altos): se rellena la pantalla al alto. */
    dest_a = janela_a;
    dest_l = (janela_a * 4) / 3;
    if (dest_l > janela_l) { dest_l = janela_l; dest_a = (janela_l * 3) / 4; }
    if (dest_l < 1) dest_l = 1;
    if (dest_a < 1) dest_a = 1;
    dest_x = (janela_l - dest_l) / 2;
    dest_y = (janela_a - dest_a) / 2;
    if (tab_col_n < dest_l)
    {
        libera(tab_col);
        tab_col = aloca((size_t)dest_l * sizeof(int));
        tab_col_n = tab_col ? dest_l : 0;
    }
    if (tab_fila_n < dest_a)
    {
        libera(tab_fila);
        tab_fila = aloca((size_t)dest_a * sizeof(int));
        tab_fila_n = tab_fila ? dest_a : 0;
    }
    if (!tab_col || !tab_fila) { escalando = false; video_superficie.pixels = janela_px; return; }
    for (int x = 0; x < dest_l; x++) tab_col[x] = (x * sup_l) / dest_l;
    for (int y = 0; y < dest_a; y++) tab_fila[y] = (y * sup_a) / dest_a;
    preencher32(janela_px, 0, (size_t)janela_l * (size_t)janela_a);   /* las bandas, negras */
    registrar("[OPENTYRIAN] escalado 4:3: superficie %dx%d en ventana %dx%d -> %dx%d en (%d,%d)",
              sup_l, sup_a, janela_l, janela_a, dest_l, dest_a, dest_x, dest_y);
}

/* Copia la superficie escalada a la ventana (solo si escalando). */
static void escalar_para_janela(void)
{
    int prev = -1;
    uint32_t *d = janela_px + (size_t)dest_y * janela_l + dest_x;
    for (int y = 0; y < dest_a; y++, d += janela_l)
    {
        int sy = tab_fila[y];
        if (sy == prev) { memcpy(d, d - janela_l, (size_t)dest_l * 4); continue; }
        prev = sy;
        const uint32_t *o = sup_buf + (size_t)sy * sup_l;
        for (int x = 0; x < dest_l; x++) d[x] = o[tab_col[x]];
    }
}

/* La ventana cambio de tamano (pantalla completa, maximizar, arrastre). */
static void ao_redimensionar(int l, int a, int estado)
{
    if (l == janela_l && a == janela_a) { em_cheia = (estado == JAN_ESTADO_TELA_CHEIA); return; }
    if (!escalando && sup_buf && janela_px)      /* guarda el cuadro actual antes de perder la ventana */
        memcpy(sup_buf, janela_px, (size_t)sup_l * sup_a * 4);
    uint32_t *px = NULL;
    if (redimensionar_janela(janela_id, l, a, &px) < 0 || !px) return;
    janela_px = px; janela_l = l; janela_a = a;
    em_cheia = (estado == JAN_ESTADO_TELA_CHEIA);
    preparar_escala();
    if (escalando) escalar_para_janela();
    else memcpy(janela_px, sup_buf, (size_t)sup_l * sup_a * 4);
    atualizar_janela(janela_id, NULL);
    registrar("[OPENTYRIAN] ventana %dx%d (%s)", l, a, em_cheia ? "pantalla completa" : "ventana");
}

static void tratar_evento(evento_t *e);

/* Espera (poco) a que el kernel confirme la pantalla completa o la salida. */
static void esperar_estado(bool alvo)
{
    unsigned limite = tempo_ms() + 700;
    while (em_cheia != alvo && (int)(limite - tempo_ms()) > 0)
    {
        evento_t e;
        if (esperar_evento(janela_id, &e, 30) == 1) tratar_evento(&e);
    }
}

SDL_Surface *SDL_SetVideoMode(int largura, int altura, int bits, Uint32 flags)
{
    if (largura <= 0 || altura <= 0) return NULL;
    if (bits != 32) bits = 32;   /* el kernel pinta en 32 bits (0xRRGGBB) */

    bool quer_cheia = (flags & SDL_FULLSCREEN) != 0;
    if (sagui_cheia_inicial) { quer_cheia = true; sagui_cheia_inicial = false; }

    /* En modo ventana la ventana se rehace al tamano nuevo; a pantalla
     * completa se queda como esta (la superficie se escala). */
    if (janela_id >= 0 && !em_cheia && (janela_l != largura || janela_a != altura))
    {
        fechar_janela(janela_id);
        janela_id = -1;
        janela_px = NULL;
    }
    if (janela_id < 0)
    {
        janela_id = criar_janela("OpenTyrian", largura, altura, &janela_px);
        if (janela_id < 0 || !janela_px) { janela_id = -1; janela_px = NULL; return NULL; }
        janela_l = largura;
        janela_a = altura;
        em_cheia = false;
        estilo_janela(janela_id, ESTILO_BASE, 0, 0);   /* F11 del kernel, y el Esc llega al juego */
        registrar("[OPENTYRIAN] janela %dx%d criada", largura, altura);
    }

    if (!sup_buf || sup_l != largura || sup_a != altura)
    {
        libera(sup_buf);
        sup_buf = aloca((size_t)largura * altura * 4);
        if (!sup_buf) return NULL;
        memset(sup_buf, 0, (size_t)largura * altura * 4);
        sup_l = largura;
        sup_a = altura;
    }
    preparar_escala();

    video_formato_preparar(&video_formato, 32);
    video_paleta_info.ncolors = 256;
    video_paleta_info.colors = video_paleta;
    video_formato.palette = &video_paleta_info;

    video_superficie.flags = SDL_SWSURFACE | SDL_HWPALETTE;
    video_superficie.format = &video_formato;
    video_superficie.w = largura;
    video_superficie.h = altura;
    video_superficie.pitch = (Uint16)(largura * 4);
    video_superficie.offset = 0;
    video_superficie.hwdata = NULL;

    if (quer_cheia != em_cheia)
    {
        estilo_janela(janela_id, ESTILO_BASE | (quer_cheia ? JAN_TELA_CHEIA : 0), 0, 0);
        esperar_estado(quer_cheia);
        registrar("[OPENTYRIAN] pantalla completa %s: ventana %dx%d", em_cheia ? "activada" : "desactivada", janela_l, janela_a);
        /* Al salir, la ventana vuelve al tamano de la superficie (el scaler pudo cambiar). */
        if (!em_cheia && (janela_l != sup_l || janela_a != sup_a)) ao_redimensionar(sup_l, sup_a, JAN_ESTADO_NORMAL);
    }
    return &video_superficie;
}

SDL_Surface *SDL_GetVideoSurface(void)
{
    return janela_id >= 0 ? &video_superficie : NULL;
}

int SDL_Flip(SDL_Surface *superficie)
{
    (void)superficie;
    if (janela_id < 0) return -1;
    fullscreen_enabled = em_cheia;   /* el motor guarda en su configuracion lo que de verdad hay (D204) */
    if (escalando) escalar_para_janela();
    if (!primeiro_quadro)
    {
        primeiro_quadro = true;
        registrar("[OPENTYRIAN] primeiro quadro desenhado (%dx%d)", janela_l, janela_a);
    }
    return atualizar_janela(janela_id, NULL);
}

SDL_Surface *SDL_CreateRGBSurface(Uint32 flags, int largura, int altura, int bits,
                                  Uint32 rmask, Uint32 gmask, Uint32 bmask, Uint32 amask)
{
    (void)flags; (void)rmask; (void)gmask; (void)bmask; (void)amask;
    if (largura <= 0 || altura <= 0 || (bits != 8 && bits != 16 && bits != 32)) return NULL;

    SDL_Surface *s = aloca(sizeof *s);
    if (!s) return NULL;
    SDL_PixelFormat *f = aloca(sizeof *f);
    if (!f) { libera(s); return NULL; }
    video_formato_preparar(f, bits);
    if (bits == 8)
    {
        SDL_Color *cores = aloca(sizeof(SDL_Color) * 256);
        if (!cores) { libera(f); libera(s); return NULL; }
        for (int i = 0; i < 256; i++) { cores[i].r = cores[i].g = cores[i].b = 0; cores[i].unused = 0; }
        SDL_Palette *p = aloca(sizeof *p);
        if (!p) { libera(cores); libera(f); libera(s); return NULL; }
        p->ncolors = 256;
        p->colors = cores;
        f->palette = p;
    }
    int pitch = (largura * bits + 7) / 8;
    pitch = (pitch + 3) & ~3;           /* SDL alinea la fila a 4 bytes */
    void *pixels = aloca((size_t)pitch * (size_t)altura);
    if (!pixels) { libera(f); libera(s); return NULL; }
    memset(pixels, 0, (size_t)pitch * (size_t)altura);

    s->flags = SDL_SWSURFACE;
    s->format = f;
    s->w = largura;
    s->h = altura;
    s->pitch = (Uint16)pitch;
    s->pixels = pixels;
    s->offset = 0;
    s->hwdata = NULL;
    return s;
}

void SDL_FreeSurface(SDL_Surface *superficie)
{
    if (!superficie) return;
    if (superficie == &video_superficie) return;
    if (superficie->pixels) libera(superficie->pixels);
    if (superficie->format)
    {
        if (superficie->format->palette)
        {
            if (superficie->format->palette->colors) libera(superficie->format->palette->colors);
            libera(superficie->format->palette);
        }
        libera(superficie->format);
    }
    libera(superficie);
}

int SDL_FillRect(SDL_Surface *destino, void *rect, Uint32 cor)
{
    if (!destino || !destino->pixels) return -1;
    int x = 0, y = 0, l = destino->w, a = destino->h;
    if (rect)
    {
        SDL_Rect *r = rect;
        x = r->x; y = r->y; l = r->w; a = r->h;
    }
    if (x < 0) { l += x; x = 0; }
    if (y < 0) { a += y; y = 0; }
    if (x + l > destino->w) l = destino->w - x;
    if (y + a > destino->h) a = destino->h - y;
    if (l <= 0 || a <= 0) return 0;

    unsigned char *p = destino->pixels;
    int bpp = destino->format->BytesPerPixel;
    for (int j = 0; j < a; j++)
    {
        unsigned char *linha = p + (size_t)(y + j) * destino->pitch + (size_t)x * bpp;
        if (bpp == 1) memset(linha, (int)(cor & 0xFF), (size_t)l);
        else if (bpp == 2)
        {
            for (int i = 0; i < l; i++) ((Uint16 *)linha)[i] = (Uint16)cor;
        }
        else
        {
            for (int i = 0; i < l; i++) ((Uint32 *)linha)[i] = cor;
        }
    }
    return 0;
}

Uint32 SDL_MapRGB(const SDL_PixelFormat *formato, Uint8 r, Uint8 g, Uint8 b)
{
    (void)formato;
    return ((Uint32)r << 16) | ((Uint32)g << 8) | (Uint32)b;
}

int SDL_SetColors(SDL_Surface *superficie, SDL_Color *cores, int primeiro, int n)
{
    if (!superficie || !superficie->format || !superficie->format->palette) return 0;
    SDL_Palette *p = superficie->format->palette;
    for (int i = 0; i < n; i++)
    {
        int k = primeiro + i;
        if (k < 0 || k >= p->ncolors) continue;
        p->colors[k] = cores[i];
    }
    return 1;
}

void SDL_WM_SetCaption(const char *titulo, const char *icone)
{
    (void)icone;
    if (janela_id >= 0 && titulo) titulo_janela(janela_id, titulo);
}

/* ------------------------------------------------------------------ teclado */

SDLMod SDL_GetModState(void) { return mods_atual; }

const char *SDL_GetKeyName(SDLKey chave)
{
    static char buf[16];
    if (chave >= SDLK_a && chave <= SDLK_z)
    {
        buf[0] = (char)chave; buf[1] = 0;
        return buf;
    }
    if (chave >= SDLK_0 && chave <= SDLK_9)
    {
        buf[0] = (char)chave; buf[1] = 0;
        return buf;
    }
    switch (chave)
    {
    case SDLK_RETURN:    return "Return";
    case SDLK_ESCAPE:    return "Escape";
    case SDLK_SPACE:     return "Space";
    case SDLK_BACKSPACE: return "Backspace";
    case SDLK_TAB:       return "Tab";
    case SDLK_UP:        return "Up";
    case SDLK_DOWN:      return "Down";
    case SDLK_LEFT:      return "Left";
    case SDLK_RIGHT:     return "Right";
    case SDLK_HOME:      return "Home";
    case SDLK_END:       return "End";
    case SDLK_PAGEUP:    return "PageUp";
    case SDLK_PAGEDOWN:  return "PageDown";
    case SDLK_INSERT:    return "Insert";
    case SDLK_DELETE:    return "Delete";
    case SDLK_LSHIFT:    return "Left Shift";
    case SDLK_RSHIFT:    return "Right Shift";
    case SDLK_LCTRL:     return "Left Ctrl";
    case SDLK_RCTRL:     return "Right Ctrl";
    case SDLK_LALT:      return "Left Alt";
    case SDLK_RALT:      return "Right Alt";
    default: break;
    }
    if (chave >= SDLK_F1 && chave <= SDLK_F12)
    {
        buf[0] = 'F'; buf[1] = (char)('1' + (chave - SDLK_F1)); buf[2] = 0;
        return buf;
    }
    if (chave >= SDLK_KP0 && chave <= SDLK_KP9)
    {
        buf[0] = (char)('0' + (chave - SDLK_KP0)); buf[1] = 0;
        return buf;
    }
    return "?";
}

/* Convierte lo que da el kernel (un caracter Latin-1 o TECLA_*) en una tecla
 * de SDL.
 *
 * O kernel da as teclas como caracteres Latin-1, e os codigos 1..26 sao
 * usados tanto por Ctrl+letra como pelos caracteres de controlo (8 = retrocesso,
 * 9 = tabulador, 10 = Enter). O que os distingue e o modificador Ctrl, por
 * isso a funcao precisa de saber se o Ctrl estava apertado. */
static SDLKey chave_de_tecla(unsigned t, unsigned mods)
{
    if (t >= TECLA_CIMA && t <= TECLA_DELETE)
    {
        switch (t)
        {
        case TECLA_CIMA:    return SDLK_UP;
        case TECLA_BAIXO:   return SDLK_DOWN;
        case TECLA_ESQUERDA: return SDLK_LEFT;
        case TECLA_DIREITA: return SDLK_RIGHT;
        case TECLA_HOME:    return SDLK_HOME;
        case TECLA_END:     return SDLK_END;
        case TECLA_PGUP:    return SDLK_PAGEUP;
        case TECLA_PGDN:    return SDLK_PAGEDOWN;
        case TECLA_INSERT:  return SDLK_INSERT;
        case TECLA_DELETE:  return SDLK_DELETE;
        default: break;
        }
        return SDLK_UNKNOWN;
    }
    if (t >= TECLA_F1 && t <= TECLA_F12) return (SDLKey)(SDLK_F1 + (t - TECLA_F1));
    if (t == TECLA_ESC) return SDLK_ESCAPE;
    if (t >= 1 && t <= 26)
    {
        /* D156: os codigos 1..26 sao do Ctrl+letra, mas alguns sao caracteres
         * de controlo quando o Ctrl nao esta apertado (o kernel da 8 = retrocesso,
         * 9 = tabulador e 10 = Enter). Sem isto o Enter chegava ao jogo como a
         * letra 'j' (SDLK_a + 10 - 1) e o menu nao respondia. */
        if (!(mods & KMOD_CTRL))
        {
            if (t == 8)  return SDLK_BACKSPACE;
            if (t == 9)  return SDLK_TAB;
            if (t == 10 || t == 13) return SDLK_RETURN;
        }
        return (SDLKey)(SDLK_a + (t - 1));   /* Ctrl+letra */
    }
    if (t >= 'a' && t <= 'z') return (SDLKey)t;
    if (t >= 'A' && t <= 'Z') return (SDLKey)(t - 'A' + 'a');
    if (t >= '0' && t <= '9') return (SDLKey)t;
    switch (t)
    {
    case '\r': case '\n': return SDLK_RETURN;
    case ' ':  return SDLK_SPACE;
    case '\b': return SDLK_BACKSPACE;
    case '\t': return SDLK_TAB;
    case '-':  return SDLK_MINUS;
    case '=':  return SDLK_EQUALS;
    case '[':  return SDLK_LEFTBRACKET;
    case ']':  return SDLK_RIGHTBRACKET;
    case '\\': return SDLK_BACKSLASH;
    case ';':  return SDLK_SEMICOLON;
    case '\'': return SDLK_QUOTE;
    case ',':  return SDLK_COMMA;
    case '.':  return SDLK_PERIOD;
    case '/':  return SDLK_SLASH;
    default:   return SDLK_UNKNOWN;
    }
}

/* El codigo de rastreo (set 1) de una tecla, para saber cuando se suelta.
 * 0 = no se sabe. */
static unsigned char rastreo_de_chave(SDLKey sim)
{
    if (sim >= SDLK_a && sim <= SDLK_z) return (unsigned char)(0x1E + (sim - SDLK_a));
    if (sim >= SDLK_0 && sim <= SDLK_9)
    {
        if (sim == SDLK_0) return 0x0B;
        return (unsigned char)(0x02 + (sim - SDLK_1));
    }
    if (sim >= SDLK_F1 && sim <= SDLK_F10) return (unsigned char)(0x3B + (sim - SDLK_F1));
    if (sim >= SDLK_F11 && sim <= SDLK_F12) return (unsigned char)(0x80 | (0x57 + (sim - SDLK_F11)));
    if (sim >= SDLK_KP0 && sim <= SDLK_KP9) return 0;   /* o teclado numerico nao se distingue */
    switch (sim)
    {
    case SDLK_RETURN:    return 0x1C;
    case SDLK_ESCAPE:    return 0x01;
    case SDLK_SPACE:     return 0x39;
    case SDLK_BACKSPACE: return 0x0E;
    case SDLK_TAB:       return 0x0F;
    case SDLK_MINUS:     return 0x0C;
    case SDLK_EQUALS:    return 0x0D;
    case SDLK_LEFTBRACKET:  return 0x1A;
    case SDLK_RIGHTBRACKET: return 0x1B;
    case SDLK_BACKSLASH: return 0x2B;
    case SDLK_SEMICOLON: return 0x27;
    case SDLK_QUOTE:     return 0x28;
    case SDLK_COMMA:     return 0x33;
    case SDLK_PERIOD:    return 0x34;
    case SDLK_SLASH:     return 0x35;
    case SDLK_UP:        return 0x80 | 0x48;
    case SDLK_DOWN:      return 0x80 | 0x50;
    case SDLK_LEFT:      return 0x80 | 0x4B;
    case SDLK_RIGHT:     return 0x80 | 0x4D;
    case SDLK_HOME:      return 0x47;
    case SDLK_END:       return 0x4F;
    case SDLK_PAGEUP:    return 0x49;
    case SDLK_PAGEDOWN:  return 0x51;
    case SDLK_INSERT:    return 0x52;
    case SDLK_DELETE:    return 0x53;
    default: break;
    }
    return 0;
}

/* O caractere que a SDL leva na tecla (D156): para as teclas de controlo e
 * o que o jogo espera (Enter = 13), e para as letras o proprio caracter. */
static Uint16 unicode_de_tecla(SDLKey sim, unsigned t)
{
    switch (sim)
    {
    case SDLK_RETURN:   return 13;
    case SDLK_BACKSPACE: return 8;
    case SDLK_TAB:      return 9;
    case SDLK_ESCAPE:   return 27;
    default: break;
    }
    if (t >= 'a' && t <= 'z') return (Uint16)t;
    if (t >= 'A' && t <= 'Z') return (Uint16)(t - 'A' + 'a');
    if (t >= 0x20 && t < 0x7F) return (Uint16)t;
    return 0;
}

static void marcar_tecla(SDLKey sim, bool down){
    unsigned char sc = rastreo_de_chave(sim);
    if (!sc) return;
    for (int i = 0; i < n_teclas; i++)
        if (teclas[i].sim == sim) { teclas[i].down = down; return; }
    if (n_teclas >= TAM_TECLAS) return;
    teclas[n_teclas].sim = sim;
    teclas[n_teclas].sc = sc;
    teclas[n_teclas].down = down;
    n_teclas++;
}

/* Compara con o mapa de teclas que o kernel diz que estao premidas e fabrica
 * os SDL_KEYUP das que se soltaron. */
static void soltar_teclas(void)
{
    unsigned char mapa[32];
    memset(mapa, 0, sizeof mapa);
    int foco = teclas_seguradas(mapa);
    for (int i = 0; i < n_teclas; i++)
    {
        if (!teclas[i].down) continue;
        bool agora = foco && (mapa[teclas[i].sc >> 3] & (1u << (teclas[i].sc & 7)));
        if (!agora)
        {
            teclas[i].down = false;
            if (fila_n < TAM_FILA)
            {
                SDL_Event *ev = &fila[(fila_ini + fila_n) % TAM_FILA];
                memset(ev, 0, sizeof *ev);
                ev->type = SDL_KEYUP;
                ev->key.type = SDL_KEYUP;
                ev->key.state = SDL_RELEASED;
                ev->key.keysym.sym = teclas[i].sim;
                ev->key.keysym.mod = mods_atual;
                fila_n++;
            }
        }
    }
}

static void enfileirar(SDL_Event *ev)
{
    if (fila_n >= TAM_FILA) return;
    fila[(fila_ini + fila_n) % TAM_FILA] = *ev;
    fila_n++;
}

/* ------------------------------------------------------------------ eventos */

/* Convierte la posicion de la ventana en la de la superficie del juego. */
static void pos_para_superficie(int *x, int *y)
{
    if (!escalando) return;
    int sx = ((*x - dest_x) * sup_l) / dest_l, sy = ((*y - dest_y) * sup_a) / dest_a;
    *x = sx < 0 ? 0 : sx >= sup_l ? sup_l - 1 : sx;
    *y = sy < 0 ? 0 : sy >= sup_a ? sup_a - 1 : sy;
}

static void tratar_evento(evento_t *pe)
{
    evento_t e = *pe;
    {
        SDL_Event ev;
        memset(&ev, 0, sizeof ev);
        switch (e.tipo)
        {
        case EV_REDIMENSIONAR:
            ao_redimensionar(e.x, e.y, e.tecla);
            break;
        case EV_TECLA:
        {
            if ((e.modificadores & 4) && e.tecla == 10)   /* Alt+Intro: alterna la pantalla completa */
            {
                estilo_janela(janela_id, ESTILO_BASE | (em_cheia ? 0 : JAN_TELA_CHEIA), 0, 0);
                break;
            }
            mods_atual = KMOD_NONE;
            if (e.modificadores & 1) mods_atual |= KMOD_SHIFT;
            if (e.modificadores & 2) mods_atual |= KMOD_CTRL;
            if (e.modificadores & 4) mods_atual |= KMOD_ALT;
            SDLKey sim = chave_de_tecla(e.tecla, mods_atual);
            ev.type = SDL_KEYDOWN;
            ev.key.type = SDL_KEYDOWN;
            ev.key.state = SDL_PRESSED;
            ev.key.keysym.sym = sim;
            ev.key.keysym.mod = mods_atual;
            ev.key.keysym.unicode = unicode_de_tecla(sim, e.tecla);
            marcar_tecla(sim, true);
            registrar("[OPENTYRIAN] tecla %u (SDL %d)", e.tecla, (int)sim);
            enfileirar(&ev);
            break;
        }
        case EV_MOVER:
            ev.type = SDL_MOUSEMOTION;
            ev.motion.type = SDL_MOUSEMOTION;
            { int mx = e.x, my = e.y; pos_para_superficie(&mx, &my); ev.motion.x = (Uint16)mx; ev.motion.y = (Uint16)my; }
            registrar("[OPENTYRIAN] mover %d,%d", (int)e.x, (int)e.y);
            enfileirar(&ev);
            break;
        case EV_CLIQUE:
            ev.type = SDL_MOUSEBUTTONDOWN;
            ev.button.type = SDL_MOUSEBUTTONDOWN;
            ev.button.button = (Uint8)(e.botoes & 1 ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT);
            ev.button.state = SDL_PRESSED;
            { int mx = e.x, my = e.y; pos_para_superficie(&mx, &my); ev.button.x = (Uint16)mx; ev.button.y = (Uint16)my; }
            enfileirar(&ev);
            break;
        case EV_SOLTOU:
            ev.type = SDL_MOUSEBUTTONUP;
            ev.button.type = SDL_MOUSEBUTTONUP;
            ev.button.button = (Uint8)(e.botoes & 1 ? SDL_BUTTON_LEFT : SDL_BUTTON_RIGHT);
            ev.button.state = SDL_RELEASED;
            { int mx = e.x, my = e.y; pos_para_superficie(&mx, &my); ev.button.x = (Uint16)mx; ev.button.y = (Uint16)my; }
            enfileirar(&ev);
            break;
        case EV_FECHAR:
            ev.type = SDL_QUIT;
            ev.quit.type = SDL_QUIT;
            enfileirar(&ev);
            break;
        default:
            break;
        }
    }
}

static void eventos_do_sistema(void)
{
    if (janela_id < 0) return;
    evento_t e;
    while (esperar_evento(janela_id, &e, 0)) tratar_evento(&e);
    soltar_teclas();
}

int SDL_PollEvent(SDL_Event *evento)
{
    bombear_audio();
    eventos_do_sistema();
    if (fila_n <= 0) return 0;
    *evento = fila[fila_ini];
    if (evento->type == SDL_KEYDOWN || evento->type == SDL_KEYUP || evento->type == SDL_QUIT)
        registrar("[OPENTYRIAN] ev %d sym %d", (int)evento->type, (int)evento->key.keysym.sym);
    fila_ini = (fila_ini + 1) % TAM_FILA;
    fila_n--;
    return 1;
}

int SDL_PushEvent(SDL_Event *evento)
{
    eventos_do_sistema();
    enfileirar(evento);
    return 0;
}

int SDL_EnableKeyRepeat(int delay, int intervalo) { (void)delay; (void)intervalo; return 0; }
int SDL_EnableUNICODE(int enable) { (void)enable; return 0; }
int SDL_ShowCursor(int estado) { (void)estado; return 0; }
/* D242: sin captura en OpenTyrian (el ratón del menú es el del sistema) */
int SDL_WM_GrabInput(int modo) { (void)modo; return 0; }

/* O SaguiOS nao tem chamada para mudar a posicao do rato do sistema, mas o
 * motor usa a posicao logica do rato: nos menus ele recentra o rato a cada
 * espera (set_mouse_position) e, se nao receivesmos isso, o menu fica
 * preso no canto superior esquerdo. Enfileiramos um movimento de rato com a
 * posicao pedida, que e o que o motor le. */
void SDL_WarpMouse(int x, int y)
{
    if (janela_id < 0) return;
    SDL_Event ev;
    memset(&ev, 0, sizeof ev);
    ev.type = SDL_MOUSEMOTION;
    ev.motion.type = SDL_MOUSEMOTION;
    ev.motion.x = (Uint16)x;
    ev.motion.y = (Uint16)y;
    enfileirar(&ev);
}

/* ------------------------------------------------------------------ relogio */

Uint32 SDL_GetTicks(void) { return tempo_ms(); }

void SDL_Delay(Uint32 ms)
{
    bombear_audio();
    dormir(ms);
}

/* ------------------------------------------------------------------ audio */

int SDL_OpenAudio(SDL_AudioSpec *pedido, SDL_AudioSpec *obtido)
{
    if (!pedido) return -1;
    audio_pedido = *pedido;
    audio_cb = pedido->callback;
    audio_userdata = pedido->userdata;
    audio_aberto = true;
    audio_pausado = true;

    /* O motor mistura em 16 bits (ou 8) e a gente entrega o mesmo formato. */
    int bits = (pedido->format == AUDIO_S8) ? 8 : 16;
    audio_fd = audio_abrir(pedido->freq, pedido->channels ? pedido->channels : 1, bits);
    if (audio_fd < 0) audio_fd = -1;   /* sem placa de som: o jogo fica mudo */

    if (obtido) *obtido = audio_pedido;
    return 0;
}

void SDL_CloseAudio(void)
{
    if (audio_fd >= 0) { audio_fechar(audio_fd, 0); audio_fd = -1; }
    audio_aberto = false;
    audio_cb = NULL;
}

void SDL_PauseAudio(int pausa) { audio_pausado = (pausa != 0); }

void SDL_LockAudio(void) { }
void SDL_UnlockAudio(void) { }

int SDL_BuildAudioCVT(SDL_AudioCVT *cvt, Uint16 formato_origem, Uint8 canais_origem,
                      int freq_origem, Uint16 formato_destino, Uint8 canais_destino,
                      int freq_destino)
{
    if (!cvt) return -1;
    memset(cvt, 0, sizeof *cvt);
    cvt->needed = 0;
    cvt->src_format = formato_origem;
    cvt->dst_format = formato_destino;
    cvt->len_mult = 1;
    (void)canais_origem; (void)freq_origem; (void)canais_destino; (void)freq_destino;
    return 0;
}

int SDL_ConvertAudio(SDL_AudioCVT *cvt)
{
    (void)cvt;
    return 0;
}

/* O motor mistura numa chamada; aqui metemos o resultado no fluxo de som do
 * sistema. Sem placa de som, nao se mistura nada (o jogo fica mudo). */
static void bombear_audio(void)
{
    if (!audio_aberto || audio_pausado || !audio_cb || audio_fd < 0) return;
    int livre = audio_livre(audio_fd);
    if (livre < (int)sizeof audio_buf) return;
    memset(audio_buf, 0, sizeof audio_buf);
    audio_cb(audio_userdata, audio_buf, (int)sizeof audio_buf);
    audio_escrever(audio_fd, audio_buf, sizeof audio_buf);
}

/* ------------------------------------------------------------------ joystick */

struct _SDL_Joystick { int indice; };

int SDL_NumJoysticks(void) { return 0; }

SDL_Joystick *SDL_JoystickOpen(int indice)
{
    (void)indice;
    return NULL;
}

void SDL_JoystickClose(SDL_Joystick *joy) { (void)joy; }
const char *SDL_JoystickName(SDL_Joystick *joy) { (void)joy; return ""; }
int SDL_JoystickNumAxes(SDL_Joystick *joy) { (void)joy; return 0; }
int SDL_JoystickNumButtons(SDL_Joystick *joy) { (void)joy; return 0; }
int SDL_JoystickNumHats(SDL_Joystick *joy) { (void)joy; return 0; }
Sint16 SDL_JoystickGetAxis(SDL_Joystick *joy, int eixo) { (void)joy; (void)eixo; return 0; }
Uint8 SDL_JoystickGetButton(SDL_Joystick *joy, int botao) { (void)joy; (void)botao; return 0; }
Uint8 SDL_JoystickGetHat(SDL_Joystick *joy, int chapeu) { (void)joy; (void)chapeu; return SDL_HAT_CENTERED; }
void SDL_JoystickUpdate(void) { }
int SDL_JoystickEventState(int estado) { (void)estado; return 0; }
