# 119 - DOSBox y un lanzador de juegos de DOS (Recreativa)

- **Estado:** hecha (**v8.3.1**, D294); pendiente de probar en Amilo, dv7 y L635
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** SDL 1.2 (D228), runtime de C++ (D228, D230), captura del ratón (D242), diálogos de archivo (103, D272), Correo (113, D289), `docs/input_share.md`

## Problema / motivación

El sistema tiene sus juegos propios y varios ports (OpenJazz, Quake, Command & Conquer, Game Boy), pero ninguna forma de usar el
inmenso catálogo de juegos de DOS que el mantenedor y los usuarios tienen. Lo natural es un emulador (DOSBox) y, como cada juego
pide su núcleo, sus ciclos, su sonido o su CD, una lista donde guardar esa configuración y lanzarlos con un clic.

## Propuesta

1. **DOSBox 0.74-3** (el clásico, GPL-2.0-or-later) en `userland/terceros/dosbox/`: fuentes en git con un parche mínimo
   (`tools/dosbox_fuentes.sh`, versión y SHA-256 fijos), compilado con la SDL 1.2 del sistema y un runtime de C++ ampliado.
2. **Runtime de C++** general: RTTI (`dynamic_cast`, `typeid`), `<map>`, `<set>`, flujos (`<iostream>`, `<sstream>`, `<fstream>`,
   `<iomanip>`), `<limits>`, `<iterator>`, `<functional>`, más `<algorithm>`; sirve a futuros ports.
3. **Núcleos** normal (intérprete) y dynamic (recompilador de x86), `core=auto` por defecto; Sound Blaster 16, AdLib y altavoz;
   teclado, ratón capturado y pantalla completa; `dosbox.conf` en `/home/.config`, C: = `/home/dos`.
4. **Recreativa**: el lanzador, con interfaz de Windows 98 como Correo; juegos en `/home/dos/juegos.conf`, detección automática de
   carpetas con programas de DOS en `/home/dos` y `/jogos`, Añadir (diálogo común), Propiedades, Jugar, Configurar, Quitar y Carpeta.
5. `.exe`, `.com` y `.bat` se abren con DOSBox desde Archivos. Guía: `docs/dosbox.md`.

## Criterios de aceptación

- [x] DOSBox arranca, ejecuta un `.COM` (texto, modo 13h, altavoz, Sound Blaster) y sale con código 0 (`tests/h294_dosbox.py juego`)
- [x] El recompilador dinámico funciona y es más de 2 veces más rápido que el intérprete (x8,7 medido; `h294 nucleos`)
- [x] `dynamic_cast` y `typeid` en el runtime, con prueba de unidad a -Os/-O0/-O2 (`h294 unidad`); `<map>`, flujos... (`tests/cxx_biblioteca_unidad.cpp`)
- [x] Recreativa: encuentra sola carpetas, añade un juego, lo lanza y lo cierra; guarda `juegos.conf` (`h294 lanzador`)
- [x] Pantalla completa y ratón capturado en modo relativo (`h294 completa`); Archivos abre `.COM` (`h294 archivos`)
- [x] C&C (h230) y OpenJazz (h228) siguen pasando; h81_licencias
- [x] Decisión D294 y guía `docs/dosbox.md`
- [ ] Probado en máquinas reales (Amilo, dv7, L635): velocidad del intérprete y del recompilador, sonido, ratón

## Notas

- **MIDI (mpu401) sin hacer**: `audio_midi` solo sintetiza archivos; habría que ampliarlo con eventos en directo.
- Sin red (módem, IPX), sin captura de pantalla, sin joystick.
- No usa el renderizador de la SDL 2 por la GPU (DOSBox 0.74 es SDL 1.2). DOSBox Staging (SDL 2) lo haría, pero es otro port.
- El nombre «Recreativa» lo propone esta implementación; el mantenedor puede cambiarlo.
- Los juegos nunca van a git ni a las imágenes publicadas (`docs/input_share.md`).
