// SPDX-License-Identifier: GPL-2.0-or-later
/* marco.hpp - la parte común de los emuladores de consolas de SaguiOS (D321): LaiNES (NES) y
 * Gearsystem (Master System y Game Gear). Todo sobre la SDL 2 del sistema (userland/libsdl2):
 *
 *   - vídeo: una ventana con renderizador (por la GPU si la hay, D288) y una textura de 32 bits;
 *     el tamaño lógico guarda la proporción y llena la ventana o la pantalla completa (F11);
 *   - sonido: SDL_QueueAudio, con tope de cola (si el emulador va por delante, se tira lo que sobra);
 *   - mandos: SDL_GameController (D307) con el mapeo por defecto de abajo, y el teclado;
 *   - velocidad: un reloj por cuadro (60 Hz NTSC, 50 Hz PAL) que duerme lo que sobra y avisa cuando va
 *     tarde para que el emulador se salte el dibujo (salto de cuadros); cada 5 s anota los cuadros por
 *     segundo y el porcentaje de CPU que necesitó al registro del sistema.
 *
 * Mapeo por defecto (el orden de bits es el del mando de la NES):
 *   teclado   flechas = cruceta; X = A; Z = B; Intro = Start; Espacio o Mayús der. = Select;
 *             Esc = salir; F11 o Alt+Intro = pantalla completa; F5 = guardar estado; F7 = cargar;
 *             F9 = reiniciar; Tab (mantenido) = avance rápido.
 *   mando     cruceta y palanca izquierda = cruceta; B (derecha) o Y = A; A (abajo) o X = B;
 *             Start = Start; Back = Select; el segundo mando es el jugador 2.
 */
#pragma once
#include "SDL.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

extern "C" void sagui_emu_registrar(const char *texto);   /* principal_emu.c */
extern "C" int sagui_emu_completa;                         /* --pantalla-completa */

enum { PAD_A = 1, PAD_B = 2, PAD_SELECT = 4, PAD_START = 8, PAD_UP = 16, PAD_DOWN = 32, PAD_LEFT = 64, PAD_RIGHT = 128 };
enum { ACC_GUARDAR = 1, ACC_CARGAR = 2, ACC_REINICIAR = 4, ACC_RAPIDO = 8 };

struct Marco {
    const char *nombre;                  /* para el registro: «LAINES», «GEARSYSTEM» */
    SDL_Window *vent = nullptr;
    SDL_Renderer *rend = nullptr;
    SDL_Texture *tex = nullptr;
    int tw = 0, th = 0, lw = 0, lh = 0;
    SDL_AudioDeviceID audio = 0;
    int audio_hz = 0, audio_canales = 1;
    SDL_GameController *mando[2] = { nullptr, nullptr };
    bool completa = false, salir = false;
    int acciones = 0;                    /* ACC_* pedidas desde el último sondeo (F5, F7, F9) */
    bool rapido = false;
    bool primero = true;
    double periodo = 1000.0 / 60.0, proximo = 0;
    uint32_t t_ini = 0, n_cuadros = 0, n_saltados = 0;
    uint64_t pc_ocup = 0, pc_cuadro0 = 0, pc_ini = 0;   /* TSC: tiempo de trabajo de los cuadros (el reloj en ms es muy grueso) */
    int saltos_seguidos = 0;
    bool tarde = false;

    void registrar(const char *fmt, int a = 0, int b = 0, int c = 0) {
        char t[200], u[240];
        snprintf(t, sizeof t, fmt, a, b, c);
        snprintf(u, sizeof u, "[%s] %s", nombre, t);
        sagui_emu_registrar(u);
    }

    /* Ventana de tw x th píxeles de la imagen, a escala «esc». */
    bool abrir(const char *titulo, int ancho, int alto, int esc, int ancho_ventana = 0, int alto_ventana = 0) {
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER) != 0) {
            registrar("SDL_Init falla");
            return false;
        }
        tw = ancho; th = alto;
        int wl = ancho_ventana ? ancho_ventana : ancho, hl = alto_ventana ? alto_ventana : alto;
        vent = SDL_CreateWindow(titulo, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, wl * esc, hl * esc, SDL_WINDOW_RESIZABLE);
        if (!vent) { registrar("no se puede crear la ventana"); return false; }
        rend = SDL_CreateRenderer(vent, -1, SDL_RENDERER_ACCELERATED);
        if (!rend) rend = SDL_CreateRenderer(vent, -1, 0);
        if (!rend) { registrar("no se puede crear el renderizador"); return false; }
        SDL_RenderSetLogicalSize(rend, wl, hl);
        lw = wl; lh = hl;
        tex = SDL_CreateTexture(rend, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, ancho, alto);
        if (!tex) { registrar("no se puede crear la textura"); return false; }
        if (sagui_emu_completa) poner_completa(true);
        for (int i = 0, k = 0; i < SDL_NumJoysticks() && k < 2; i++)
            if (SDL_IsGameController(i)) mando[k++] = SDL_GameControllerOpen(i);
        for (int k = 0; k < 2; k++)
            if (mando[k]) registrar(k ? "mando 2 abierto como SDL_GameController" : "mando 1 abierto como SDL_GameController");
        t_ini = SDL_GetTicks();
        pc_cuadro0 = pc_ini = __builtin_ia32_rdtsc();
        proximo = t_ini;
        return true;
    }

    /* La imagen cambió de tamaño (p. ej. la Master System pasa a 224 líneas): se ajusta la proporción. */
    void logico(int w, int h) {
        if (w == lw && h == lh) return;
        lw = w; lh = h;
        SDL_RenderSetLogicalSize(rend, w, h);
    }

    void poner_completa(bool si) {
        completa = si;
        SDL_SetWindowFullscreen(vent, si ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    }

    /* Sonido de 16 bits, «canales» (1 o 2) a «hz». */
    bool abrir_audio(int hz, int canales) {
        SDL_AudioSpec pedido, obtido;
        SDL_memset(&pedido, 0, sizeof pedido);
        pedido.freq = hz; pedido.format = AUDIO_S16SYS; pedido.channels = (Uint8)canales; pedido.samples = 1024;
        audio = SDL_OpenAudioDevice(nullptr, 0, &pedido, &obtido, 0);
        if (!audio) { registrar("sin sonido (SDL_OpenAudioDevice falla)"); return false; }
        audio_hz = obtido.freq ? obtido.freq : hz;
        audio_canales = obtido.channels ? obtido.channels : canales;
        SDL_PauseAudioDevice(audio, 0);
        registrar("sonido abierto: %d Hz, %d canal(es)", audio_hz, audio_canales);
        return true;
    }

    /* «n» muestras de 16 bits en total (cuadros x canales). Tope de cola: 150 ms. */
    void enviar_audio(const int16_t *m, int n) {
        if (!audio || rapido) return;
        uint32_t tope = (uint32_t)audio_hz * (uint32_t)audio_canales * 2u * 150u / 1000u;
        if (SDL_GetQueuedAudioSize(audio) > tope) return;
        SDL_QueueAudio(audio, m, (uint32_t)n * 2u);
    }

    /* Pinta una región de la textura (pix: ARGB8888, pitch en bytes) escalada a la ventana. */
    void presentar(const uint32_t *pix, int pitch, SDL_Rect fuente) {
        /* Con retraso se salta el dibujo, pero no más de 3 seguidos (al menos 15 imágenes por segundo). */
        if (!tarde || primero || saltos_seguidos >= 3) {
            saltos_seguidos = 0;
            SDL_UpdateTexture(tex, nullptr, pix, pitch);
            SDL_RenderClear(rend);
            SDL_RenderCopy(rend, tex, &fuente, nullptr);
            SDL_RenderPresent(rend);
            if (primero) { primero = false; registrar("primer cuadro dibujado"); }
        } else {
            n_saltados++;
            saltos_seguidos++;
        }
    }

    /* Teclado y mandos: el estado del jugador j como bits PAD_*. Atiende también los eventos. */
    int sondear() {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) salir = true;
            else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
                SDL_Keycode k = e.key.keysym.sym;
                if (k == SDLK_ESCAPE) salir = true;
                else if (k == SDLK_F11 || (k == SDLK_RETURN && (e.key.keysym.mod & KMOD_ALT))) poner_completa(!completa);
                else if (k == SDLK_F5) acciones |= ACC_GUARDAR;
                else if (k == SDLK_F7) acciones |= ACC_CARGAR;
                else if (k == SDLK_F9) acciones |= ACC_REINICIAR;
            } else if (e.type == SDL_CONTROLLERDEVICEADDED) {
                int i = e.cdevice.which;
                for (int k = 0; k < 2; k++)
                    if (!mando[k]) { mando[k] = SDL_GameControllerOpen(i); if (mando[k]) registrar("mando abierto como SDL_GameController"); break; }
            }
        }
        const Uint8 *t = SDL_GetKeyboardState(nullptr);
        rapido = t[SDL_SCANCODE_TAB] != 0;
        return 0;
    }

    int pad(int j) {
        int r = 0;
        if (j == 0) {
            const Uint8 *t = SDL_GetKeyboardState(nullptr);
            if (t[SDL_SCANCODE_UP]) r |= PAD_UP;
            if (t[SDL_SCANCODE_DOWN]) r |= PAD_DOWN;
            if (t[SDL_SCANCODE_LEFT]) r |= PAD_LEFT;
            if (t[SDL_SCANCODE_RIGHT]) r |= PAD_RIGHT;
            if (t[SDL_SCANCODE_X]) r |= PAD_A;
            if (t[SDL_SCANCODE_Z]) r |= PAD_B;
            if (t[SDL_SCANCODE_RETURN]) r |= PAD_START;
            if (t[SDL_SCANCODE_SPACE] || t[SDL_SCANCODE_RSHIFT]) r |= PAD_SELECT;
        }
        SDL_GameController *c = mando[j];
        if (c) {
            SDL_GameControllerUpdate();
            auto b = [&](SDL_GameControllerButton x) { return SDL_GameControllerGetButton(c, x) != 0; };
            if (b(SDL_CONTROLLER_BUTTON_DPAD_UP)) r |= PAD_UP;
            if (b(SDL_CONTROLLER_BUTTON_DPAD_DOWN)) r |= PAD_DOWN;
            if (b(SDL_CONTROLLER_BUTTON_DPAD_LEFT)) r |= PAD_LEFT;
            if (b(SDL_CONTROLLER_BUTTON_DPAD_RIGHT)) r |= PAD_RIGHT;
            const int Z = 12000;
            int ax = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTX), ay = SDL_GameControllerGetAxis(c, SDL_CONTROLLER_AXIS_LEFTY);
            if (ax < -Z) r |= PAD_LEFT;
            if (ax > Z) r |= PAD_RIGHT;
            if (ay < -Z) r |= PAD_UP;
            if (ay > Z) r |= PAD_DOWN;
            if (b(SDL_CONTROLLER_BUTTON_B) || b(SDL_CONTROLLER_BUTTON_Y)) r |= PAD_A;
            if (b(SDL_CONTROLLER_BUTTON_A) || b(SDL_CONTROLLER_BUTTON_X)) r |= PAD_B;
            if (b(SDL_CONTROLLER_BUTTON_START)) r |= PAD_START;
            if (b(SDL_CONTROLLER_BUTTON_BACK)) r |= PAD_SELECT;
        }
        return r;
    }

    /* Se llama al acabar cada cuadro emulado: duerme hasta el siguiente (o no, si va tarde) y deja
     * «tarde» a punto para el cuadro siguiente. */
    void esperar_cuadro(double hz) {
        periodo = 1000.0 / hz;
        uint64_t pc = __builtin_ia32_rdtsc();
        pc_ocup += pc - pc_cuadro0;                       /* lo que costó emular y dibujar este cuadro */
        uint32_t ahora = SDL_GetTicks();
        n_cuadros++;
        proximo += periodo;
        if (rapido) { proximo = ahora; tarde = false; }
        else if ((double)ahora < proximo) {
            SDL_Delay((Uint32)(proximo - (double)ahora));
            tarde = false;
        } else {
            tarde = ((double)ahora - proximo) > periodo;     /* más de un cuadro de retraso: saltar el dibujo */
            if ((double)ahora - proximo > 5 * periodo) proximo = ahora;   /* demasiado: no se recupera */
        }
        uint32_t fin = SDL_GetTicks();
        pc_cuadro0 = __builtin_ia32_rdtsc();
        if (fin - t_ini >= 5000) {
            int fps10 = (int)(n_cuadros * 10000u / (fin - t_ini));
            uint64_t total = pc_cuadro0 - pc_ini;
            int cpu = (int)(pc_ocup * 100u / (total + 1));
            char t[120];
            snprintf(t, sizeof t, "%d.%d cuadros/s, CPU %d%%, %d saltados", fps10 / 10, fps10 % 10, cpu, (int)n_saltados);
            char u[160];
            snprintf(u, sizeof u, "[%s] %s", nombre, t);
            sagui_emu_registrar(u);
            t_ini = fin; pc_ini = pc_cuadro0; n_cuadros = 0; pc_ocup = 0; n_saltados = 0;
        }
    }

    void cerrar() {
        if (audio) SDL_CloseAudioDevice(audio);
        for (int k = 0; k < 2; k++) if (mando[k]) SDL_GameControllerClose(mando[k]);
        if (tex) SDL_DestroyTexture(tex);
        if (rend) SDL_DestroyRenderer(rend);
        if (vent) SDL_DestroyWindow(vent);
        SDL_Quit();
    }
};
