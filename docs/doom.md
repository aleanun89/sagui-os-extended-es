# DOOM en SaguiOS (D383)

Chocolate Doom 3.1.1 (GPL-2.0 o posterior), con Heretic, Hexen y Strife, y Freedoom 0.13.0 (BSD-3-Clause) como contenido
libre. Salió en la v8.5.4.8.

## Cómo jugar

**Inicio > Juegos > DOOM** (usa el WAD original de `/jogos/DOOM` si existe, y si no Freedoom) o **Freedoom** (fuerza el libre).
En una terminal: `chocolate-doom [--freedoom] [-warp 1 1] [-skill 3] [-fullscreen] [-nosound] [-timedemo demo1]`.
`chocolate-heretic`, `chocolate-hexen` y `chocolate-strife` necesitan su WAD original en `/jogos/DOOM`.

## Datos

| Qué | Dónde |
|---|---|
| Binarios | `/apps/bin/chocolate-doom`, `chocolate-heretic`, `chocolate-hexen`, `chocolate-strife` |
| Freedoom (`freedoom1.wad`, `freedoom2.wad`) y licencias | `/apps/doom` (también en las imágenes públicas) |
| WAD originales (tuyos) | `/jogos/DOOM` (`input_share/games/DOOM`, solo imágenes privadas) |
| Configuración y partidas | `~/.config/chocolate-doom/` |

Nunca va un WAD comercial a git, pruebas ni imágenes públicas. La Recreativa (DOSBox) muestra `/jogos/DOOM` si hay `DOOM.EXE`;
no choca con esto.

## Compilar

`tools/doom_fuentes.sh` (Chocolate Doom, SHA-256 fijado, aplica `parches/sagui.patch`) y `tools/doom_datos.sh` (Freedoom),
ambos incluidos en `make priv_descargas`. Después `make` (`userland/terceros/chocolatedoom/chocolatedoom.mk`; `DOOM=0` lo
deja fuera). Sin las fuentes, el sistema se construye igual sin el juego; sin Freedoom, el juego avisa de que falta un WAD.

## Qué se hizo en el sistema

SDL 2: cerrojos y condiciones nulos (no hay hilos), `SDL_BuildAudioCVT`/`SDL_ConvertAudio`, `SDL_LowerBlit`. SDL2_mixer:
panorama por canal, `Mix_HookMusic`, efectos post-mix. libc: `putenv`. La música sale por el MIDI del sistema (no por el OPL
emulado, que costaría CPU en equipos antiguos). Sin SDL2_net no hay juego en red.

## Rendimiento

Ver la tabla de abajo (se rellena con `chocolate-doom --freedoom -timedemo demo1`).
