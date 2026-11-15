// SPDX-License-Identifier: GPL-3.0-or-later
/* principal_mgba.c - el front-end de mGBA para el SaguiOS (D322).
 *
 * mGBA (MPL-2.0) trae su propio front-end con Qt o SDL, que tira de hilos, OpenGL, libpng, el
 * depurador y la configuración de escritorio. Aquí se usa solo el núcleo (mCore: GBA, Game Boy
 * y Game Boy Color) y este fichero es lo único nuestro que lo maneja, sobre la SDL 2 del
 * sistema (userland/libsdl2):
 *
 *   mgba [opciones] ROM.gba|ROM.gb|ROM.gbc
 *   mgba --medir [opciones] ROM [segundos]      mide la velocidad (docs/mgba.md)
 *
 *   --pantalla-completa     la ventana nace a pantalla completa (F11 la alterna)
 *   --escala N              ventana de N veces la pantalla del cartucho (3 por defecto)
 *   --salto auto|N          salto de cuadros: auto (por defecto), o N cuadros saltados de cada N+1
 *   --sin-sonido            sin sonido (menos trabajo en un PC antiguo)
 *   --gpu                   renderizador de la SDL 2 por la GPU (D288)
 *   --suavizado             escalado bilineal (por defecto, el más cercano: más ligero)
 *   --guardados DIR         carpeta de guardados (por defecto /home/.config/mgba)
 *
 * Teclas: flechas, Z = A, X = B, A = L, S = R, Intro = Start, Retroceso o Mayús derecha = Select;
 * F5 guarda el estado, F7 lo carga (Mayús+F5/F7: ranura siguiente, 1 a 9), Tab mantenido =
 * velocidad sin límite, F11 pantalla completa, Esc sale. Mando: SDL_GameController (D307) con la
 * disposición de Nintendo por posición (el botón derecho es la A).
 * Configuración opcional: /home/.config/mgba/mgba.conf (clave=valor: salto, escala, sonido,
 * suavizado, gpu). La BIOS no hace falta (mGBA lleva la suya, HLE) y no se usa ninguna.
 */
#include "sagui.h"
#include "SDL.h"

#include <mgba/core/core.h>
#include <mgba/core/blip_buf.h>
#include <mgba/core/config.h>
#include <mgba/core/serialize.h>
#include <mgba-util/vfs.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#define DIR_PADRAO "/home/.config/mgba"
#define TAXA_AUDIO 44100
#define BIT(k) (1u << (k))

/* Los bits de las teclas son los mismos en la GBA y en la Game Boy (internal/gba/input.h). */
enum { MK_A, MK_B, MK_SELECT, MK_START, MK_DIREITA, MK_ESQUERDA, MK_CIMA, MK_BAIXO, MK_R, MK_L };

static struct {
    int escala, cheia, sonido, suave, gpu;
    int salto;          /* -1 = auto */
    int medir;
    char dir[160];
} cfg = { 3, 0, 1, 0, 0, -1, 0, DIR_PADRAO };

static struct mCore *nucleo;
static unsigned larg, alt;
static color_t *tela;               /* el búfer de vídeo del núcleo */
static Uint32 *argb;                /* lo mismo, en el orden que quiere la textura */
static SDL_Window *janela;
static SDL_Renderer *rend;
static SDL_Texture *textura;
static SDL_AudioDeviceID audio;
static SDL_GameController *mando;
static char rom_base[80];
static int ranura = 0;

/* registrar() del sistema no entiende %f: se formatea con la libc y se pasa como texto. */
static void rlog(const char *fmt, ...)
{
    char b[300];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    registrar("%s", b);
}

static void ruta(char *d, size_t n, const char *ext)
{
    snprintf(d, n, "%s/%s%s", cfg.dir, rom_base, ext);
}

static void criar_pastas(void)
{
    info_arquivo_t inf;
    if (info_arquivo("/home", &inf) < 0) criar_pasta("/home");
    if (info_arquivo("/home/.config", &inf) < 0) criar_pasta("/home/.config");
    if (info_arquivo(cfg.dir, &inf) < 0) criar_pasta(cfg.dir);
}

static void ler_config(void)
{
    char c[200];
    snprintf(c, sizeof c, "%s/mgba.conf", cfg.dir);
    FILE *f = fopen(c, "r");
    if (!f) return;
    char l[120];
    while (fgets(l, sizeof l, f)) {
        char *v = strchr(l, '=');
        if (!v || l[0] == '#') continue;
        *v++ = 0;
        v[strcspn(v, "\r\n")] = 0;
        if (!strcmp(l, "salto")) cfg.salto = !strcmp(v, "auto") ? -1 : atoi(v);
        else if (!strcmp(l, "escala")) cfg.escala = atoi(v);
        else if (!strcmp(l, "sonido")) cfg.sonido = atoi(v);
        else if (!strcmp(l, "suavizado")) cfg.suave = atoi(v);
        else if (!strcmp(l, "gpu")) cfg.gpu = atoi(v);
    }
    fclose(f);
}

static void config_inicial(void)
{
    char c[200];
    snprintf(c, sizeof c, "%s/mgba.conf", cfg.dir);
    FILE *f = fopen(c, "r");
    if (f) { fclose(f); return; }
    f = fopen(c, "w");
    if (!f) return;
    fputs("# mGBA en SaguiOS (D322): configuracion de este usuario (docs/mgba.md)\n"
          "# salto de cuadros: auto, o N (0 = ninguno) para ir mas ligero en un PC antiguo\n"
          "salto=auto\n"
          "escala=3\n"
          "sonido=1\n"
          "suavizado=0\n"
          "gpu=0\n", f);
    fclose(f);
}

/* ------------------------------------------------------------------ vídeo y sonido */

static int iniciar_video(int escala, int cheia)
{
    sagui_sdl2_escala = escala;
    if (cheia) sagui_sdl2_cheia_inicial = 1;
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, cfg.suave ? "1" : "0");
    janela = SDL_CreateWindow("mGBA", 0, 0, (int)larg, (int)alt, 0);
    if (!janela) return -1;
    rend = SDL_CreateRenderer(janela, -1, cfg.gpu ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE);
    if (!rend) return -1;
    SDL_RenderSetLogicalSize(rend, (int)larg, (int)alt);
    textura = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, (int)larg, (int)alt);
    return textura ? 0 : -1;
}

static void fechar_video(void)
{
    if (textura) SDL_DestroyTexture(textura);
    if (rend) SDL_DestroyRenderer(rend);
    if (janela) SDL_DestroyWindow(janela);
    textura = NULL; rend = NULL; janela = NULL;
}

/* El píxel de mGBA es 0xAABBGGRR (rojo abajo); la textura, 0xAARRGGBB. */
static void apresentar(void)
{
    unsigned n = larg * alt;
    for (unsigned i = 0; i < n; i++) {
        Uint32 p = tela[i];
        argb[i] = 0xFF000000u | ((p & 0xFF) << 16) | (p & 0xFF00) | ((p >> 16) & 0xFF);
    }
    SDL_UpdateTexture(textura, NULL, argb, (int)(larg * 4));
    SDL_RenderClear(rend);
    SDL_RenderCopy(rend, textura, NULL, NULL);
    SDL_RenderPresent(rend);
}

static void iniciar_audio(void)
{
    SDL_AudioSpec p;
    memset(&p, 0, sizeof p);
    p.freq = TAXA_AUDIO; p.format = AUDIO_S16SYS; p.channels = 2; p.samples = 1024;
    SDL_AudioSpec o;
    audio = SDL_OpenAudioDevice(NULL, 0, &p, &o, 0);
    if (audio) SDL_PauseAudioDevice(audio, 0);
    blip_set_rates(nucleo->getAudioChannel(nucleo, 0), nucleo->frequency(nucleo), TAXA_AUDIO);
    blip_set_rates(nucleo->getAudioChannel(nucleo, 1), nucleo->frequency(nucleo), TAXA_AUDIO);
}

static void sacar_audio(void)
{
    static short buf[4096 * 2];
    blip_t *l = nucleo->getAudioChannel(nucleo, 0), *r = nucleo->getAudioChannel(nucleo, 1);
    int n = blip_samples_avail(l);
    if (n > 4096) n = 4096;
    if (n <= 0) return;
    blip_read_samples(l, buf, n, 1);
    blip_read_samples(r, buf + 1, n, 1);
    if (audio) {
        /* Si el sistema va atrasado, no se acumula: se tira lo que sobra de más de ~0,25 s. */
        if (SDL_GetQueuedAudioSize(audio) > TAXA_AUDIO) SDL_ClearQueuedAudio(audio);
        SDL_QueueAudio(audio, buf, (Uint32)n * 4);
    }
}

/* ------------------------------------------------------------------ entradas */

static int stick(int v) { return v > 16000 ? 1 : v < -16000 ? -1 : 0; }

static unsigned teclas_mando(void)
{
    unsigned k = 0;
    if (!mando) return 0;
#define B(sdl, bit) if (SDL_GameControllerGetButton(mando, sdl)) k |= BIT(bit)
    B(SDL_CONTROLLER_BUTTON_B, MK_A);          /* por posición: el botón de la derecha es la A */
    B(SDL_CONTROLLER_BUTTON_A, MK_B);
    B(SDL_CONTROLLER_BUTTON_X, MK_B);
    B(SDL_CONTROLLER_BUTTON_Y, MK_A);
    B(SDL_CONTROLLER_BUTTON_LEFTSHOULDER, MK_L);
    B(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, MK_R);
    B(SDL_CONTROLLER_BUTTON_BACK, MK_SELECT);
    B(SDL_CONTROLLER_BUTTON_START, MK_START);
    B(SDL_CONTROLLER_BUTTON_DPAD_UP, MK_CIMA);
    B(SDL_CONTROLLER_BUTTON_DPAD_DOWN, MK_BAIXO);
    B(SDL_CONTROLLER_BUTTON_DPAD_LEFT, MK_ESQUERDA);
    B(SDL_CONTROLLER_BUTTON_DPAD_RIGHT, MK_DIREITA);
#undef B
    int x = stick(SDL_GameControllerGetAxis(mando, SDL_CONTROLLER_AXIS_LEFTX));
    int y = stick(SDL_GameControllerGetAxis(mando, SDL_CONTROLLER_AXIS_LEFTY));
    if (x > 0) k |= BIT(MK_DIREITA); else if (x < 0) k |= BIT(MK_ESQUERDA);
    if (y > 0) k |= BIT(MK_BAIXO); else if (y < 0) k |= BIT(MK_CIMA);
    return k;
}

static void abrir_mando(void)
{
    if (mando) return;
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (!SDL_IsGameController(i)) continue;
        mando = SDL_GameControllerOpen(i);
        if (mando) {
            rlog("[MGBA] mando abierto como SDL_GameController: %s", SDL_GameControllerName(mando));
            return;
        }
    }
}

static void estado_guardar(void)
{
    char c[200], e[8];
    snprintf(e, sizeof e, ".ss%d", ranura);
    ruta(c, sizeof c, e);
    struct VFile *vf = VFileOpen(c, O_CREAT | O_TRUNC | O_RDWR);
    int ok = vf && mCoreSaveStateNamed(nucleo, vf, SAVESTATE_ALL & ~SAVESTATE_SCREENSHOT);
    if (vf) vf->close(vf);
    rlog("[MGBA] estado %s: %s (ranura %d)", ok ? "guardado" : "NO guardado", c, ranura);
}

static void estado_cargar(void)
{
    char c[200], e[8];
    snprintf(e, sizeof e, ".ss%d", ranura);
    ruta(c, sizeof c, e);
    struct VFile *vf = VFileOpen(c, O_RDONLY);
    int ok = vf && mCoreLoadStateNamed(nucleo, vf, SAVESTATE_ALL & ~SAVESTATE_SCREENSHOT);
    if (vf) vf->close(vf);
    rlog("[MGBA] estado %s: %s (ranura %d)", ok ? "cargado" : "NO cargado", c, ranura);
}

/* ------------------------------------------------------------------ núcleo */

static int abrir_rom(const char *rom)
{
    struct VFile *vf = VFileOpen(rom, O_RDONLY);
    if (!vf) { rlog("[MGBA] no se puede abrir %s", rom); return -1; }
    nucleo = mCoreFindVF(vf);
    if (!nucleo) { vf->close(vf); rlog("[MGBA] %s no es una ROM de GBA, GB ni GBC", rom); return -1; }
    nucleo->init(nucleo);
    mCoreInitConfig(nucleo, NULL);
    struct mCoreOptions o;
    memset(&o, 0, sizeof o);
    o.frameskip = 0; o.volume = 0x100; o.audioBuffers = 1024; o.skipBios = true; o.useBios = false;
    o.rewindEnable = false;
    mCoreConfigLoadDefaults(&nucleo->config, &o);
    mCoreConfigSetDefaultIntValue(&nucleo->config, "sgb.borders", 0);   /* sin el marco de la Super Game Boy: 160x144 */
    {    /* Paleta de la Game Boy en gris (mGBA parte de negro si no se le da ninguna). */
        static const unsigned gris[4] = { 0xF8F8F8, 0xA8A8A8, 0x505050, 0x000000 };
        for (int i = 0; i < 12; i++) {
            char k[16];
            snprintf(k, sizeof k, "gb.pal[%d]", i);
            mCoreConfigSetDefaultIntValue(&nucleo->config, k, M_RGB8_TO_NATIVE(gris[i & 3]));
        }
    }
    mCoreLoadForeignConfig(nucleo, &nucleo->config);
    nucleo->desiredVideoDimensions(nucleo, &larg, &alt);
    tela = calloc((size_t)larg * alt, sizeof *tela);
    argb = calloc((size_t)larg * alt, sizeof *argb);
    nucleo->setVideoBuffer(nucleo, tela, larg);
    if (!nucleo->loadROM(nucleo, vf)) { rlog("[MGBA] no carga %s", rom); return -1; }
    char c[200];
    ruta(c, sizeof c, ".sav");
    struct VFile *sv = VFileOpen(c, O_CREAT | O_RDWR);
    if (sv) nucleo->loadSave(nucleo, sv);
    nucleo->reset(nucleo);
    char tit[32] = "";
    nucleo->getGameTitle(nucleo, tit);
    rlog("[MGBA] ROM: %s \"%s\" (%s, %ux%u)", rom, tit, nucleo->platform(nucleo) == mPLATFORM_GBA ? "GBA" : "GB/GBC", larg, alt);
    return 0;
}

static void poner_salto(int n)
{
    mCoreConfigSetOverrideIntValue(&nucleo->config, "frameskip", n);
    nucleo->reloadConfigOption(nucleo, "frameskip", &nucleo->config);
}

static double agora_ms(void)
{
    return (double)SDL_GetPerformanceCounter() * 1000.0 / (double)SDL_GetPerformanceFrequency();
}

/* ------------------------------------------------------------------ bucle normal */

static int jogar(void)
{
    const double periodo = 1000.0 * nucleo->frameCycles(nucleo) / nucleo->frequency(nucleo);
    unsigned teclas_teclado = 0, ult = ~0u;
    int salto = cfg.salto < 0 ? 0 : cfg.salto, turbo = 0, sair = 0, primeiro = 1;
    unsigned contador = 0;
    double prox = agora_ms(), t_info = prox;
    unsigned n_cuadros = 0, n_apres = 0;
    poner_salto(salto);
    while (!sair) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) sair = 1;
            else if (e.type == SDL_CONTROLLERDEVICEADDED) abrir_mando();
            else if (e.type == SDL_CONTROLLERDEVICEREMOVED && mando) { SDL_GameControllerClose(mando); mando = NULL; }
            else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) {
                int d = e.type == SDL_KEYDOWN;
                unsigned b = 0;
                switch (e.key.keysym.sym) {
                case SDLK_z: b = BIT(MK_A); break;
                case SDLK_x: b = BIT(MK_B); break;
                case SDLK_a: b = BIT(MK_L); break;
                case SDLK_s: b = BIT(MK_R); break;
                case SDLK_RETURN: b = BIT(MK_START); break;
                case SDLK_BACKSPACE: case SDLK_RSHIFT: b = BIT(MK_SELECT); break;
                case SDLK_UP: b = BIT(MK_CIMA); break;
                case SDLK_DOWN: b = BIT(MK_BAIXO); break;
                case SDLK_LEFT: b = BIT(MK_ESQUERDA); break;
                case SDLK_RIGHT: b = BIT(MK_DIREITA); break;
                case SDLK_TAB: turbo = d; break;
                case SDLK_ESCAPE: if (d) sair = 1; break;
                case SDLK_F5: if (d) { if (e.key.keysym.mod & KMOD_SHIFT) ranura = ranura % 9 + 1; estado_guardar(); } break;
                case SDLK_F7: if (d) { if (e.key.keysym.mod & KMOD_SHIFT) ranura = ranura % 9 + 1; estado_cargar(); } break;
                case SDLK_F11:
                    if (d) {
                        static int cheia;
                        cheia = !cheia;
                        SDL_SetWindowFullscreen(janela, cheia ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    }
                    break;
                default: break;
                }
                if (d) teclas_teclado |= b; else teclas_teclado &= ~b;
            }
        }
        unsigned k = teclas_teclado | teclas_mando();
        if (k != ult) {
            rlog("[MGBA] teclas: %03x", k);
            ult = k;
        }
        nucleo->setKeys(nucleo, k);

        double t0 = agora_ms();
        nucleo->runFrame(nucleo);
        n_cuadros++;
        if (cfg.sonido) sacar_audio(); else {
            blip_clear(nucleo->getAudioChannel(nucleo, 0)); blip_clear(nucleo->getAudioChannel(nucleo, 1));
        }
        if (contador++ % (unsigned)(salto + 1) == 0) {
            apresentar();
            n_apres++;
            if (primeiro) { rlog("[MGBA] primer cuadro dibujado"); primeiro = 0; }
        }
        double t1 = agora_ms();

        /* Salto de cuadros automático: si el cuadro costó más que el periodo, se salta más (hasta
         * 4); si sobra más de la mitad del tiempo, se va bajando. */
        if (cfg.salto < 0) {
            int nuevo = salto;
            double gasto = t1 - t0;
            if (gasto > periodo && salto < 4) nuevo = salto + 1;
            else if (gasto < periodo * 0.5 && salto > 0) nuevo = salto - 1;
            if (nuevo != salto) { salto = nuevo; poner_salto(salto); }
        }
        if (turbo) {
            prox = agora_ms();
        } else {
            prox += periodo;
            double resto = prox - agora_ms();
            if (resto > 1) SDL_Delay((Uint32)resto);
            else if (resto < -periodo * 5) prox = agora_ms();   /* muy atrasado: no se persigue el reloj */
        }
        if (t1 - t_info > 5000) {
            rlog("[MGBA] fps: %.1f (presentados %.1f, salto %d)", n_cuadros * 1000.0 / (t1 - t_info),
                      n_apres * 1000.0 / (t1 - t_info), salto);
            n_cuadros = n_apres = 0; t_info = t1;
        }
    }
    return 0;
}

/* ------------------------------------------------------------------ --medir */

typedef struct { double fps_med, fps_min, cpu, total, ticks; unsigned cuadros; } medida_t;

static inline unsigned long long tsc(void)
{
    unsigned lo, hi;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((unsigned long long)hi << 32) | lo;
}

/* Corre «seg» segundos. limite = 1: a la velocidad de la consola (60 Hz con el salto elegido); el %
 * de CPU es el tiempo ocupado entre el total. limite = 0: sin límite (todo el tiempo ocupado) y los
 * FPS son lo que el PC da; el % de CPU que haría falta a 60 Hz se saca de ahí (informar()).
 * dibujar = 0: solo el núcleo. «ticks» = ciclos del contador TSC por cuadro (núcleo y dibujo). */
static medida_t medir_fase(double seg, int limite, int dibujar, int salto)
{
    medida_t m = { 0, 1e9, 0, 0, 0, 0 };
    const double periodo = 1000.0 * nucleo->frameCycles(nucleo) / nucleo->frequency(nucleo);
    poner_salto(salto);
    double ini = agora_ms(), prox = ini, janela_ini = ini, ocupado = 0;
    unsigned c_janela = 0, contador = 0;
    unsigned long long tick_ocupado = 0, tick_ini = tsc();
    while (agora_ms() - ini < seg * 1000.0) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) { }
        unsigned long long k0 = tsc();
        double t0 = agora_ms();
        nucleo->runFrame(nucleo);
        blip_clear(nucleo->getAudioChannel(nucleo, 0));
        blip_clear(nucleo->getAudioChannel(nucleo, 1));
        if (dibujar && contador % (unsigned)(salto + 1) == 0) apresentar();
        contador++;
        tick_ocupado += tsc() - k0;
        double t1 = agora_ms();
        ocupado += t1 - t0;
        m.cuadros++; c_janela++;
        if (t1 - janela_ini >= 1000.0) {
            double f = c_janela * 1000.0 / (t1 - janela_ini);
            if (f < m.fps_min) m.fps_min = f;
            janela_ini = t1; c_janela = 0;
        }
        if (limite) {
            prox += periodo;
            double resto = prox - agora_ms();
            if (resto > 1) SDL_Delay((Uint32)resto);
            else if (resto < -periodo * 5) prox = agora_ms();
        }
    }
    m.total = agora_ms() - ini;
    m.fps_med = m.cuadros * 1000.0 / m.total;
    /* El reloj de la SDL es de milisegundos: el tiempo ocupado se mide con el TSC (ocupado / total, en ticks). */
    m.cpu = 100.0 * (double)tick_ocupado / (double)(tsc() - tick_ini);
    (void)ocupado;
    m.ticks = (double)tick_ocupado / (m.cuadros ? m.cuadros : 1);
    if (m.fps_min > 1e8) m.fps_min = m.fps_med;
    return m;
}

/* Sin límite: «CPU a 60 Hz» es la parte de un PC como este que gasta la consola a su velocidad
 * (100 / veces la consola). */
static void informar(const char *que, medida_t m, double fps_consola, int libre)
{
    double veces = m.fps_med / fps_consola;
    double cpu = libre ? 100.0 / veces : m.cpu;
    printf("[MGBA-MEDIDA] %-34s fps medios %8.1f  minimos %8.1f  %s %5.1f%%  (%.2f x la consola, %.2f Mticks/cuadro)\n",
           que, m.fps_med, m.fps_min, libre ? "CPU a 60 Hz" : "CPU", cpu, veces, m.ticks / 1e6);
    rlog("[MGBA-MEDIDA] %s fps_medios=%.1f minimos=%.1f cpu=%.1f%% x_consola=%.2f mticks=%.3f", que, m.fps_med, m.fps_min, cpu,
         veces, m.ticks / 1e6);
}

static int medir(const char *rom, double seg)
{
    double fc = (double)nucleo->frequency(nucleo) / nucleo->frameCycles(nucleo);
    printf("[MGBA-MEDIDA] ROM %s: la consola va a %.4f cuadros/s; %.0f s por fase\n", rom, fc, seg);
    informar("nucleo solo, sin limite", medir_fase(seg, 0, 0, 0), fc, 1);
    if (iniciar_video(1, 0) == 0) {
        informar("1x (sin escalado), sin limite", medir_fase(seg, 0, 1, 0), fc, 1);
        informar("1x (sin escalado), a 60 Hz", medir_fase(seg, 1, 1, 0), fc, 0);
        fechar_video();
        if (iniciar_video(cfg.escala > 1 ? cfg.escala : 3, 0) == 0) {
            informar("escalado, sin limite", medir_fase(seg, 0, 1, 0), fc, 1);
            informar("escalado, sin limite, salto 1", medir_fase(seg, 0, 1, 1), fc, 1);
            informar("escalado, sin limite, salto 2", medir_fase(seg, 0, 1, 2), fc, 1);
            informar("escalado, a 60 Hz", medir_fase(seg, 1, 1, 0), fc, 0);
        }
    } else {
        printf("[MGBA-MEDIDA] sin ventana: solo se mide el nucleo\n");
    }
    printf("[MGBA-MEDIDA] fin\n");
    return 0;
}

/* ------------------------------------------------------------------ principal */

int principal(int argc, char **argv)
{
    const char *rom = NULL, *seg = NULL;
    int escala_cli = 0;
    criar_pastas();
    ler_config();
    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--medir")) cfg.medir = 1;
        else if (!strcmp(a, "--pantalla-completa")) cfg.cheia = 1;
        else if (!strcmp(a, "--sin-sonido")) cfg.sonido = 0;
        else if (!strcmp(a, "--gpu")) cfg.gpu = 1;
        else if (!strcmp(a, "--suavizado")) cfg.suave = 1;
        else if (!strcmp(a, "--escala") && i + 1 < argc) escala_cli = atoi(argv[++i]);
        else if (!strcmp(a, "--salto") && i + 1 < argc) { i++; cfg.salto = !strcmp(argv[i], "auto") ? -1 : atoi(argv[i]); }
        else if (!strcmp(a, "--guardados") && i + 1 < argc) snprintf(cfg.dir, sizeof cfg.dir, "%s", argv[++i]);
        else if (a[0] != '-' && !rom) rom = a;
        else if (a[0] != '-' && cfg.medir && !seg) seg = a;
        else { printf("mgba: opcion desconocida: %s\n", a); rom = NULL; break; }
    }
    if (escala_cli > 0) cfg.escala = escala_cli;
    if (cfg.escala < 1 || cfg.escala > 8) cfg.escala = 3;
    if (!rom) {
        printf("uso: mgba [--pantalla-completa] [--escala N] [--salto auto|N] [--sin-sonido] [--gpu]\n"
               "          [--suavizado] [--guardados DIR] ROM.gba|.gb|.gbc\n"
               "     mgba --medir [opciones] ROM [segundos]\n"
               "Las ROM no vienen con el sistema (docs/mgba.md).\n");
        return 2;
    }
    criar_pastas();
    config_inicial();
    const char *b = strrchr(rom, '/');
    snprintf(rom_base, sizeof rom_base, "%s", b ? b + 1 : rom);
    /* El nombre de los guardados lleva la extension (juego.gba.sav): juego.gb y juego.gba no se pisan. */

    Uint32 ini = SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER | (cfg.medir ? 0 : SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    if (cfg.medir) ini |= SDL_INIT_VIDEO;
    if (SDL_Init(ini) < 0) rlog("[MGBA] SDL_Init: %s", SDL_GetError());
    if (abrir_rom(rom) < 0) return 1;
    int r;
    if (cfg.medir) {
        r = medir(rom, seg ? atof(seg) : 5.0);
    } else {
        if (iniciar_video(cfg.escala, cfg.cheia) < 0) { rlog("[MGBA] sin ventana: %s", SDL_GetError()); return 1; }
        if (cfg.sonido) iniciar_audio();
        abrir_mando();
        r = jogar();
    }
    nucleo->unloadROM(nucleo);
    nucleo->deinit(nucleo);
    fechar_video();
    SDL_Quit();
    rlog("[MGBA] fin");
    return r;
}
