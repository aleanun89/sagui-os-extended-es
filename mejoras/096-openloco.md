# 096 - OpenLoco (Chris Sawyer's Locomotion) como port

- **Estado:** propuesta (**v9.0**, decidido por el mantenedor el 2026-11-14; tiene el juego original)
- **Prioridad:** media
- **Alta:** 2026-11-14
- **Relacionadas:** D228 (`input_share/`), D229-D235 (ports de juegos: Quake, C&C, GNUBoy), `userland/libsdl2`, `userland/libopenal`, `userland/libcxx`, D247 (SDL 2 más fluida)

## Qué es

[OpenLoco](https://github.com/OpenLoco/OpenLoco): reimplementación libre en C++ del juego de
transportes *Chris Sawyer's Locomotion* (2004). **Licencia MIT**. Desde diciembre de 2025 es una
reimplementación completa: **no necesita el `loco.exe` original**, solo los **datos** del juego.

## Datos del juego

Los pone el mantenedor desde su copia de **Steam o GOG** en `input_share/games/Locomotion/` (D228), y
van a `/jogos/Locomotion`. **Nunca en git ni en las entregas publicadas.**

## Qué hace falta

| Necesita | Qué hay | Trabajo |
| --- | --- | --- |
| **SDL 3** | SDL 1.2 y SDL 2 propias | Capa SDL 3 sobre la SDL 2 propia (ventana, renderizador por software, eventos, audio); útil para futuros ports |
| **C++ moderno** (C++20 probable) | `userland/libcxx` (ampliado con C&C) | `std::filesystem` (sobre el VFS), `optional`, `variant`, `span`, `string_view`, `<format>` o `fmt`, `chrono`, `<thread>`/`<mutex>` mínimos si los usa |
| **tbb** (paralelo de libstdc++) | — | Quitarlo: versiones secuenciales de los algoritmos |
| **yaml-cpp, fmt, sfl** | — | Compilarlas (licencias MIT/BSD), registradas en `docs/terceros.md` |
| **libpng, libzip, zlib** | Lectores PNG y ZIP propios, deflate propio (D103) | Compilar las originales o adaptar una capa sobre las propias |
| **OpenAL** | `userland/libopenal` (de C&C) | Comprobar lo que use OpenLoco y completar |
| **32 bits** | SaguiOS es de 32 bits | El proyecto avisa de que en 32 bits en Linux «puede ser difícil»: revisar lo que asuma punteros de 64 bits |

## Plan

1. Compilar OpenLoco **en el PC** con la capa SDL 3 y el C++ ampliado, contra los datos, para separar
   los problemas de C++ y 32 bits de los del sistema.
2. Llevarlo a SaguiOS: `/apps/bin/openloco`, entrada en Inicio > Juegos, configuración y partidas en
   `/home/openloco`.
3. Medir el rendimiento con `--sdl-medir` (D247) en el Amilo, el L850 y el dv7.

## Pruebas

- Prueba con datos (si `input_share` los tiene): arranca, carga un escenario, avanza el tiempo, guarda
  y carga partida, capturas.
- Sin datos: el lanzador explica dónde ponerlos.

## Criterios de aceptación

- [ ] Arranca y se juega un escenario en QEMU y en al menos un portátil real
- [ ] Partidas guardadas en `/home/openloco`
- [ ] Sin datos del juego en git ni en entregas
- [ ] Test automático en `tests/`
- [ ] Decisión registrada en `docs/decisoes.md`

## Notas

- Modelo sugerido: **Opus** para la parte de C++/32 bits y la capa SDL 3 (avisando, por el coste); Sonnet
  para el lanzador, las pruebas y la documentación.
