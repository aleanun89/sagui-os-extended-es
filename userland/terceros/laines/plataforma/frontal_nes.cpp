// SPDX-License-Identifier: GPL-2.0-or-later
/* frontal_nes.cpp - la interfaz de LaiNES en SaguiOS (D321).
 *
 * LaiNES (BSD-2-Clause, Andrea Orru) trae su propia interfaz con SDL_ttf, SDL_image y un menú; aquí no
 * se usa nada de eso: este fichero reemplaza a src/gui.cpp, menu.cpp, config.cpp y main.cpp del upstream
 * (que no se compilan) y define lo que el núcleo pide de «GUI::» (cuadro nuevo, muestras de sonido, mando).
 * El menú y la elección de juego los pone Recreativa. Lo común (vídeo, sonido, mandos, reloj) está en
 * userland/terceros/emucomun/marco.hpp.
 *
 *   laines [--pantalla-completa] ROM.nes
 *
 * La SRAM con batería (cabecera iNES, byte 6, bit 1) se guarda en /home/.config/laines/<rom>.sav (al salir
 * y cada pocos segundos si cambia); los estados de guardado (F5 guardar, F7 cargar) en <rom>.sta.
 */
#include "../../emucomun/marco.hpp"
#include "gui.hpp"
#include "apu.hpp"
#include "cartridge.hpp"
#include "cpu.hpp"
#include "mapper.hpp"
#include "savestate.hpp"

#include <string>
#include <cstdio>
#include <cstring>
#include <sys/stat.h>

extern "C" const char *sagui_emu_nombre = "LaiNES";

namespace {

Marco marco;
bool bateria = false;
std::string ruta_sav, ruta_sta;
const uint32_t *ultimo_cuadro = nullptr;

std::string nombre_base(const char *ruta)
{
    std::string r(ruta);
    size_t b = r.rfind('/');
    if (b != std::string::npos) r = r.substr(b + 1);
    size_t p = r.rfind('.');
    if (p != std::string::npos) r = r.substr(0, p);
    return r;
}

void sram_cargar()
{
    Mapper *m = Cartridge::get_mapper();
    if (!bateria || !m || !m->get_prg_ram()) return;
    FILE *f = fopen(ruta_sav.c_str(), "rb");
    if (!f) return;
    size_t n = fread(m->get_prg_ram(), 1, m->get_prg_ram_size(), f);
    fclose(f);
    marco.registrar("SRAM cargada: %d bytes", (int)n);
}

uint32_t sram_suma = 0;

uint32_t sumar(const u8 *p, u32 n)
{
    uint32_t s = 5381;
    for (u32 i = 0; i < n; i++) s = s * 33 + p[i];
    return s;
}

void sram_guardar(bool siempre)
{
    Mapper *m = Cartridge::get_mapper();
    if (!bateria || !m || !m->get_prg_ram()) return;
    uint32_t s = sumar(m->get_prg_ram(), m->get_prg_ram_size());
    if (!siempre && s == sram_suma) return;
    sram_suma = s;
    FILE *f = fopen(ruta_sav.c_str(), "wb");
    if (!f) return;
    fwrite(m->get_prg_ram(), 1, m->get_prg_ram_size(), f);
    fclose(f);
    marco.registrar("SRAM guardada");
}

}  // namespace

namespace GUI {

u8 get_joypad_state(int n) { return (u8)marco.pad(n & 1); }

void new_frame(u32 *pixels) { ultimo_cuadro = pixels; }

void new_samples(const blip_sample_t *samples, size_t count)
{
    marco.enviar_audio(samples, (int)count);
}

bool is_fast_forward() { return marco.rapido; }

}  // namespace GUI

int main(int argc, char **argv)
{
    const char *rom = argv[argc - 1];
    marco.nombre = "LAINES";
    // ¿Tiene batería? Byte 6 de la cabecera iNES, bit 1.
    FILE *f = fopen(rom, "rb");
    if (!f) { marco.registrar("no se puede abrir la ROM"); return 1; }
    unsigned char cab[16] = { 0 };
    size_t n = fread(cab, 1, sizeof cab, f);
    fclose(f);
    if (n < 16 || memcmp(cab, "NES\x1a", 4) != 0) { marco.registrar("no es una ROM iNES"); return 1; }
    bateria = (cab[6] & 2) != 0;

    mkdir("/home", 0755);
    mkdir("/home/.config", 0755);
    mkdir("/home/.config/laines", 0755);
    std::string base = "/home/.config/laines/" + nombre_base(rom);
    ruta_sav = base + ".sav";
    ruta_sta = base + ".sta";

    if (!marco.abrir("LaiNES", 256, 240, 2)) return 1;
    APU::init();
    marco.abrir_audio(48000, 1);
    Cartridge::load(rom);
    if (!Cartridge::loaded()) { marco.registrar("mapper no soportado o ROM danada"); marco.cerrar(); return 1; }
    marco.registrar("ROM cargada: mapper %d, bateria %d", Cartridge::get_mapper_id(), bateria ? 1 : 0);
    sram_cargar();
    sram_suma = Cartridge::get_mapper() && Cartridge::get_mapper()->get_prg_ram()
                    ? sumar(Cartridge::get_mapper()->get_prg_ram(), Cartridge::get_mapper()->get_prg_ram_size()) : 0;

    SDL_Rect todo = { 0, 0, 256, 240 };
    int cuadros = 0;
    while (!marco.salir) {
        marco.sondear();
        if (marco.acciones & ACC_GUARDAR) { SaveState::save(ruta_sta.c_str()); marco.registrar("estado guardado"); }
        if (marco.acciones & ACC_CARGAR) { SaveState::load(ruta_sta.c_str()); marco.registrar("estado cargado"); }
        if (marco.acciones & ACC_REINICIAR) { Cartridge::reset(); sram_cargar(); }
        marco.acciones = 0;
        CPU::run_frame();
        if (ultimo_cuadro) marco.presentar(ultimo_cuadro, 256 * 4, todo);
        marco.esperar_cuadro(60.0988);
        if (++cuadros % 600 == 0) sram_guardar(false);
    }
    sram_guardar(true);
    marco.cerrar();
    marco.registrar("fin");
    return 0;
}
