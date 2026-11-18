# 141 - DOOM clásico: Chocolate Doom con Freedoom

- **Estado:** hecha en la v8.5.4.8 (D383); falta medir en un Pentium II/III reales
- **Prioridad:** media
- **Alta:** 2026-11-18

## Problema / motivación

SaguiOS tiene juegos portados (Quake, OpenJazz, Freeciv, AoEN) pero no DOOM, el clásico por excelencia, y la Recreativa solo
lo ejecuta dentro de DOSBox. Un puerto nativo es más rápido en equipos antiguos y se integra con el escritorio.

## Propuesta

- Portar Chocolate Doom 3.1.1 (GPL-2.0 o posterior) con Heretic, Hexen y Strife; fuentes fuera de git (`tools/doom_fuentes.sh`).
- Freedoom 0.13.0 (BSD-3-Clause) en las imágenes públicas (`tools/doom_datos.sh`); WAD originales solo por `input_share/games/DOOM`.
- Entradas Inicio > Juegos > DOOM y Freedoom con icono propio, ayuda `/ayuda/doom.md`, `docs/doom.md`.
- Capas reutilizables en la SDL 2 y SDL2_mixer (sin hilos, conversión de audio, panorama), `putenv` en la libc.

## Criterios de aceptación

- [x] Compila y enlaza `chocolate-doom`, `-heretic`, `-hexen`, `-strife` en x87 y libc común
- [x] Arranca con Freedoom, pasa del menú a una partida, se mueve y dispara (`tests/h391_doom.py`)
- [x] Sin WAD, un mensaje claro
- [x] Timedemo medido en QEMU TCG y KVM (cifras en `docs/doom.md`)
- [x] Licencias (`docs/terceros.md`, D383), auditoría y SPDX limpios
- [ ] Medida en un Pentium II/III reales
- [ ] Música por OPL emulado (hoy solo MIDI del sistema), juego en red (sin SDL2_net)

## Notas

Los WAD comerciales no son redistribuibles: nunca en git, pruebas ni imágenes públicas.
