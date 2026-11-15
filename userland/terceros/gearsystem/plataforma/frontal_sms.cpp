// SPDX-License-Identifier: GPL-2.0-or-later
/* frontal_sms.cpp - la interfaz de Gearsystem en SaguiOS (D321).
 *
 * Gearsystem (GPL-3.0, Ignacio Sanchez) trae su núcleo (jogo/src) separado de las interfaces
 * (platforms/: SDL con OpenGL e ImGui, libretro...). Aquí se usa solo el núcleo, con el marco común
 * de userland/terceros/emucomun (vídeo, sonido, mandos, reloj) sobre la SDL 2 del sistema.
 *
 *   gearsystem [--pantalla-completa] ROM.sms|ROM.gg|ROM.sg
 *
 * La SRAM con batería va a /home/.config/gearsystem/<rom>.sav (al salir y cada ~10 s) y el estado de
 * guardado (F5 guardar, F7 cargar) a /home/.config/gearsystem/<rom>.state1. El sonido es de 44,1 kHz
 * en estéreo; la velocidad, la de la región de la ROM (60 Hz NTSC, 50 Hz PAL).
 */
#include "../../emucomun/marco.hpp"
#include "gearsystem.h"

#include <stdlib.h>
#include <sys/stat.h>

extern "C" const char *sagui_emu_nombre = "Gearsystem";

// El núcleo lo declara (log.h) para su servidor MCP de depuración, que aquí no existe.
bool g_mcp_stdio_mode = false;

static const char *CARPETA = "/home/.config/gearsystem";

int main(int argc, char **argv)
{
    Marco marco;
    marco.nombre = "GEARSYSTEM";
    const char *rom = argv[argc - 1];
    mkdir("/home", 0755);
    mkdir("/home/.config", 0755);
    mkdir(CARPETA, 0755);

    GearsystemCore *nucleo = new GearsystemCore();
    nucleo->Init(GS_PIXEL_BGRA8888);
    if (!nucleo->LoadROM(rom)) { marco.registrar("no es una ROM de Master System, Game Gear o SG-1000 (los .zip no se leen)"); return 1; }
    nucleo->LoadRam(CARPETA, false);
    GS_RuntimeInfo info;
    nucleo->GetRuntimeInfo(info);
    bool gg = nucleo->GetCartridge()->IsGameGear() && !nucleo->GetCartridge()->IsGameGearInSMSMode();
    marco.registrar("ROM cargada (Game Gear = %d)", gg ? 1 : 0);
    marco.registrar("region %d (0 = NTSC, 1 = PAL), imagen %d", (int)info.region, info.screen_width);

    int esc = gg ? 3 : 2;
    if (!marco.abrir("Gearsystem", 256, 224, esc, info.screen_width, info.screen_height)) return 1;
    marco.abrir_audio(44100, 2);

    // 320x288 es lo más grande que da el núcleo (con overscan); el texto de la textura es de 256x224.
    static u8 cuadro[320 * 288 * 4 + 4096];
    static s16 muestras[8192];
    int cuadros = 0;
    while (!marco.salir) {
        marco.sondear();
        if (marco.acciones & ACC_GUARDAR) { nucleo->SaveState(CARPETA, 1); marco.registrar("estado guardado"); }
        if (marco.acciones & ACC_CARGAR) { nucleo->LoadState(CARPETA, 1); marco.registrar("estado cargado"); }
        if (marco.acciones & ACC_REINICIAR) nucleo->ResetROMPreservingRAM();
        marco.acciones = 0;
        for (int j = 0; j < 2; j++) {
            int p = marco.pad(j);
            GS_Joypads jp = j ? Joypad_2 : Joypad_1;
            struct { int bit; GS_Keys k; } m[] = { { PAD_UP, Key_Up }, { PAD_DOWN, Key_Down }, { PAD_LEFT, Key_Left },
                                                   { PAD_RIGHT, Key_Right }, { PAD_A, Key_1 }, { PAD_B, Key_2 }, { PAD_START, Key_Start } };
            for (auto &e : m) { if (p & e.bit) nucleo->KeyPressed(jp, e.k); else nucleo->KeyReleased(jp, e.k); }
        }
        int n = 0;
        bool dibujar = !marco.tarde;
        nucleo->RunToVBlank(cuadro, muestras, &n, nullptr, dibujar);
        marco.enviar_audio(muestras, n);
        nucleo->GetRuntimeInfo(info);
        if (dibujar) {
            marco.logico(info.screen_width, info.screen_height);
            SDL_Rect r = { 0, 0, info.screen_width, info.screen_height };
            marco.presentar((const uint32_t *)cuadro, info.screen_width * 4, r);
        }
        marco.esperar_cuadro(info.fps);
        if (++cuadros % 600 == 0) nucleo->SaveRam(CARPETA, false);
    }
    nucleo->SaveRam(CARPETA, false);
    marco.cerrar();
    marco.registrar("fin");
    return 0;
}
