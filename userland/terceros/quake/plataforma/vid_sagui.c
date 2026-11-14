/* vid_sagui.c - el vídeo y la entrada de Quake en el SaguiOS (D229).
 *
 * El renderizador de Quake es por software: dibuja en un búfer de 8 bits
 * con paleta. Aquí ese búfer es la superficie de la SDL 1.2 común del
 * SaguiOS (userland/libsdl12, D228), que la convierte por la paleta y la
 * copia a la ventana: 320x200 en una ventana al doble (640x400) o 640x480
 * tal cual, y a pantalla completa en 4:3 con bandas, como OpenJazz y
 * OpenTyrian. Hace el papel de vid_x.c/vid_null.c e in_null.c del WinQuake.
 *
 * Proporción: en ventana los píxeles son cuadrados; a pantalla completa
 * 320x200 se estira a 4:3 (píxeles más altos que anchos, como la VGA). Quake
 * corrige la perspectiva con vid.aspect, que se recalcula al cambiar.
 *
 * Entrada: el teclado llega como eventos de la SDL (con su KEYUP fabricado
 * del mapa de teclas pulsadas); las teclas que la SDL no sabe soltar (los
 * signos con Mayús) se dan pulsadas y soltadas a la vez, porque Quake
 * ignora una tecla que ya cree pulsada (por lo mismo, en la consola y en
 * el menú una pulsación repetida se suelta antes). El ratón: los botones y la rueda
 * siempre; el movimiento para mirar, a pantalla completa o con
 * «_windowed_mouse 1», o en ventana mientras el ratón esté capturado (D242: el
 * juego captura el ratón al estar en la partida y lo suelta en la consola y el menú;
 * Ctrl+Alt lo suelta, y un clic en la ventana lo vuelve a capturar).
 */
#include "quakedef.h"
#include "d_local.h"
#include "arquivos_quake.h"
#include "SDL.h"

viddef_t vid;
unsigned short d_8to16table[256];
unsigned d_8to24table[256];

cvar_t _windowed_mouse = { "_windowed_mouse", "0", true };
cvar_t m_filter = { "m_filter", "0" };

static SDL_Surface *tela = NULL;
static short *zbuffer = NULL;
static byte *cache_superficies = NULL;
static float rato_x, rato_y, rato_x_antes, rato_y_antes;
static int rato_ativo = 0;

extern qboolean keydown[256];   /* keys.c */

/* ------------------------------------------------------------------ vídeo */

static void calcular_aspecto(int cheia)
{
    /* vid.aspect es el ancho de un píxel dividido por su alto en la pantalla. */
    if (!vid.width || !vid.height) return;   /* el aviso llega dentro de SDL_SetVideoMode: VID_Init lo calcula después */
    if (cheia)
        vid.aspect = ((float)vid.height / (float)vid.width) * (4.0f / 3.0f);
    else
        vid.aspect = 1.0f;
    vid.recalc_refdef = 1;
}

static void ao_mudar_cheia(int cheia)
{
    calcular_aspecto(cheia);
    registrar("[QUAKE] pantalla completa: %s", cheia ? "activada" : "desactivada");
}

void VID_SetPalette(unsigned char *palette)
{
    SDL_Color cores[256];
    for (int i = 0; i < 256; i++) {
        cores[i].r = palette[i * 3];
        cores[i].g = palette[i * 3 + 1];
        cores[i].b = palette[i * 3 + 2];
        cores[i].unused = 0;
        d_8to24table[i] = ((unsigned)cores[i].r << 16) | ((unsigned)cores[i].g << 8) | cores[i].b;
    }
    if (tela) SDL_SetColors(tela, cores, 0, 256);
}

void VID_ShiftPalette(unsigned char *palette)
{
    VID_SetPalette(palette);
}

void VID_Init(unsigned char *palette)
{
    int largura = 320, altura = 200, i;
    Uint32 flags = SDL_SWSURFACE | SDL_HWPALETTE;

    /* -width/-height, como vid_x.c: 320x200 por defecto; 640x480 se pide. */
    if ((i = COM_CheckParm("-width")) && i + 1 < com_argc) largura = Q_atoi(com_argv[i + 1]);
    if ((i = COM_CheckParm("-height")) && i + 1 < com_argc) altura = Q_atoi(com_argv[i + 1]);
    if (largura < 320) largura = 320;
    if (altura < 200) altura = 200;
    if (largura > MAXWIDTH) largura = MAXWIDTH;
    if (altura > MAXHEIGHT) altura = MAXHEIGHT;
    largura &= ~7;
    if (COM_CheckParm("-fullscreen")) flags |= SDL_FULLSCREEN;

    sagui_sdl_escala = largura <= 320 ? 2 : 1;
    sagui_sdl_ao_mudar_cheia = ao_mudar_cheia;
    tela = SDL_SetVideoMode(largura, altura, 8, flags);
    if (!tela) Sys_Error("VID_Init: %s", SDL_GetError());

    vid.width = vid.conwidth = largura;
    vid.height = vid.conheight = altura;
    vid.maxwarpwidth = WARP_WIDTH;
    vid.maxwarpheight = WARP_HEIGHT;
    vid.numpages = 1;
    vid.colormap = host_colormap;
    vid.fullbright = 256 - LittleLong(*((int *)vid.colormap + 2048));
    vid.buffer = vid.conbuffer = (pixel_t *)tela->pixels;
    vid.rowbytes = vid.conrowbytes = tela->pitch;
    vid.direct = NULL;
    calcular_aspecto(sagui_sdl_em_cheia());

    /* El búfer de profundidad y la caché de superficies, del malloc (el
     * «hunk» del motor queda para los niveles). */
    zbuffer = malloc((size_t)largura * altura * sizeof(short));
    int tam_cache = D_SurfaceCacheForRes(largura, altura);
    cache_superficies = malloc((size_t)tam_cache);
    if (!zbuffer || !cache_superficies) Sys_Error("VID_Init: sin memoria para %dx%d", largura, altura);
    d_pzbuffer = zbuffer;
    D_InitCaches(cache_superficies, tam_cache);

    VID_SetPalette(palette);
    registrar("[QUAKE] video %dx%d de 8 bits (cache %d KB)", largura, altura, tam_cache / 1024);
}

void VID_Shutdown(void)
{
    /* La ventana se cierra con el proceso (SDL_Quit la cierra antes si hace falta). */
}

void VID_Update(vrect_t *rects)
{
    (void)rects;   /* se copia la superficie entera: es poca cosa */
    if (tela) SDL_Flip(tela);
}

/* El disquete de «cargando» se dibuja directo en la pantalla: aquí no hace
 * falta (la pantalla es el mismo búfer y la ventana se actualiza al final). */
void D_BeginDirectRect(int x, int y, byte *pbitmap, int width, int height)
{
    (void)x; (void)y; (void)pbitmap; (void)width; (void)height;
}

void D_EndDirectRect(int x, int y, int width, int height)
{
    (void)x; (void)y; (void)width; (void)height;
}

/* ------------------------------------------------------------------ teclado */

static int tecla_quake(SDLKey s)
{
    switch (s) {
    case SDLK_BACKSPACE: return K_BACKSPACE;
    case SDLK_TAB: return K_TAB;
    case SDLK_RETURN: case SDLK_KP_ENTER: return K_ENTER;
    case SDLK_ESCAPE: return K_ESCAPE;
    case SDLK_SPACE: return K_SPACE;
    case SDLK_DELETE: return K_DEL;
    case SDLK_UP: return K_UPARROW;
    case SDLK_DOWN: return K_DOWNARROW;
    case SDLK_LEFT: return K_LEFTARROW;
    case SDLK_RIGHT: return K_RIGHTARROW;
    case SDLK_INSERT: return K_INS;
    case SDLK_HOME: return K_HOME;
    case SDLK_END: return K_END;
    case SDLK_PAGEUP: return K_PGUP;
    case SDLK_PAGEDOWN: return K_PGDN;
    case SDLK_LALT: case SDLK_RALT: return K_ALT;
    case SDLK_LCTRL: case SDLK_RCTRL: return K_CTRL;
    case SDLK_LSHIFT: case SDLK_RSHIFT: return K_SHIFT;
    case SDLK_PAUSE: return K_PAUSE;
    case SDLK_KP_PLUS: return '+';
    case SDLK_KP_MINUS: return '-';
    case SDLK_KP_MULTIPLY: return '*';
    case SDLK_KP_DIVIDE: return '/';
    case SDLK_KP_PERIOD: return '.';
    default: break;
    }
    if (s >= SDLK_F1 && s <= SDLK_F12) return K_F1 + (s - SDLK_F1);
    if (s >= SDLK_KP0 && s <= SDLK_KP9) return '0' + (s - SDLK_KP0);
    if (s > 32 && s < 127) return (int)s;   /* letras (ya en minúscula), números y signos */
    /* La tecla de la izquierda del 1 da «º» o «ª» con el teclado español: es la
     * de la consola de Quake («`»). El resto de Latin-1 no tiene tecla en Quake
     * (sus números de 128 a 255 son las teclas especiales). */
    if (s == 0xBA || s == 0xAA) return '`';
    return 0;
}

static int rato_para_olhar(void)
{
    return rato_ativo && (sagui_sdl_em_cheia() || _windowed_mouse.value || SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON);
}

/* D242: captura del ratón. Se pide al estar en la partida (key_game) y se suelta en la
 * consola y el menú. Si el sistema la suelta (Ctrl+Alt, perdió el foco), no se vuelve a
 * pedir sola: espera un clic dentro de la ventana. */
static int captura_pedida = 0, captura_liberada = 0;
static void atualizar_captura(void)
{
    int quer = rato_ativo && key_dest == key_game;
    int tem = SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON;
    if (!quer) {
        if (tem) SDL_WM_GrabInput(SDL_GRAB_OFF);
        captura_pedida = 0; captura_liberada = 0;
        return;
    }
    if (tem) return;
    if (captura_pedida) { captura_pedida = 0; captura_liberada = 1; return; }   /* la soltó el sistema */
    if (!captura_liberada && SDL_WM_GrabInput(SDL_GRAB_ON) == SDL_GRAB_ON) captura_pedida = 1;
}

void Sys_SendKeyEvents(void)
{
    SDL_Event ev;
    atualizar_captura();
    while (SDL_PollEvent(&ev)) {
        switch (ev.type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP: {
            int k = tecla_quake(ev.key.keysym.sym);
            if (!k) break;
            if (ev.type == SDL_KEYDOWN) {
                /* Fuera del juego (consola, menú), una tecla que Quake cree pulsada
                 * es otra pulsación (dos «l» seguidas que llegan antes de que la SDL
                 * vea soltarse la primera) o la repetición del teclado: se suelta
                 * antes, que Quake ignora las pulsaciones repetidas. En el juego no:
                 * la repetición de una flecha mantenida no debe cortar el paso. */
                if (key_dest != key_game && keydown[k]) Key_Event(k, false);
                Key_Event(k, true);
                if (!ev.key.keysym.scancode) Key_Event(k, false);   /* no llegará su KEYUP */
            } else {
                Key_Event(k, false);
            }
            break;
        }
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP: {
            int down = ev.type == SDL_MOUSEBUTTONDOWN, k = 0;
            switch (ev.button.button) {
            case SDL_BUTTON_LEFT: k = K_MOUSE1; break;
            case SDL_BUTTON_RIGHT: k = K_MOUSE2; break;
            case SDL_BUTTON_MIDDLE: k = K_MOUSE3; break;
            case SDL_BUTTON_WHEELUP: k = K_MWHEELUP; break;
            case SDL_BUTTON_WHEELDOWN: k = K_MWHEELDOWN; break;
            }
            if (down && captura_liberada && key_dest == key_game) captura_liberada = 0;   /* un clic la vuelve a pedir */
            if (k && rato_ativo) Key_Event(k, down);
            break;
        }
        case SDL_MOUSEMOTION:
            if (rato_para_olhar()) {
                rato_x += ev.motion.xrel;
                rato_y += ev.motion.yrel;
            }
            break;
        case SDL_QUIT:
            registrar("[QUAKE] la ventana se cierra");
            Sys_Quit();
            break;
        default:
            break;
        }
    }
}

/* ------------------------------------------------------------------ ratón (in_*) */

void IN_Init(void)
{
    Cvar_RegisterVariable(&_windowed_mouse);
    Cvar_RegisterVariable(&m_filter);
    rato_ativo = !COM_CheckParm("-nomouse");
    rato_x = rato_y = 0;
}

void IN_Shutdown(void)
{
    rato_ativo = 0;
}

void IN_Commands(void)
{
    /* Los botones ya llegan como teclas en Sys_SendKeyEvents. */
}

/* Lo de IN_Move de vid_x.c. */
void IN_Move(usercmd_t *cmd)
{
    if (!rato_para_olhar()) {
        rato_x = rato_y = 0;
        return;
    }
    if (m_filter.value) {
        float x = (rato_x + rato_x_antes) * 0.5f, y = (rato_y + rato_y_antes) * 0.5f;
        rato_x_antes = rato_x;
        rato_y_antes = rato_y;
        rato_x = x;
        rato_y = y;
    } else {
        rato_x_antes = rato_x;
        rato_y_antes = rato_y;
    }
    rato_x *= sensitivity.value;
    rato_y *= sensitivity.value;

    if ((in_strafe.state & 1) || (lookstrafe.value && (in_mlook.state & 1)))
        cmd->sidemove += m_side.value * rato_x;
    else
        cl.viewangles[YAW] -= m_yaw.value * rato_x;
    if (in_mlook.state & 1)
        V_StopPitchDrift();
    if ((in_mlook.state & 1) && !(in_strafe.state & 1)) {
        cl.viewangles[PITCH] += m_pitch.value * rato_y;
        if (cl.viewangles[PITCH] > 80) cl.viewangles[PITCH] = 80;
        if (cl.viewangles[PITCH] < -70) cl.viewangles[PITCH] = -70;
    } else {
        if ((in_strafe.state & 1) && noclip_anglehack)
            cmd->upmove -= m_forward.value * rato_y;
        else
            cmd->forwardmove -= m_forward.value * rato_y;
    }
    rato_x = rato_y = 0;
}
