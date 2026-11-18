# AoEN en SaguiOS (D382, mejora 140)

AoEN es un juego de estrategia en tiempo real al estilo de Age of Empires, escrito desde cero por el mantenedor
(C99 + SDL2, dibujo por software). Código en `userland/juegos/aoen/` (GPL-3.0-or-later). Se abre desde
**Inicio > Juegos > AoEN**; ayuda para el jugador en `/ayuda/aoen.md`.

## Datos y licencias

- **No se distribuye** ningún dato del Age of Empires original (`GRAPHICS.DRS`, `INTERFAC.DRS`, `TERRAIN.DRS`, `SOUNDS.DRS`),
  ni las texturas HD de terreno ni la música MIDI de `input_share/aoen`: son de terceros. No van a git ni a las imágenes
  públicas (`INPUT_SHARE=`).
- Orden de búsqueda de los datos: argumento, `AOEN_DATA`, `aoen.cfg` (`aoen-config`), **`/jogos/AOE`** (con `DATA` o
  `GAME/DATA`) y `/jogos/AOE/DATA`. `input_share/games/AOE` los deja en `/jogos/AOE` en las imágenes privadas.
  Texturas y música: `/apps/aoen/texturas` y `/apps/aoen/musica` (las copia `aoen.mk` si existen en
  `$(INPUT_SHARE)/aoen`; ver `docs/input_share.md`). Configuración y partidas: `~/.config/aoen/`.
- `stb_image` (dominio público o MIT): `userland/terceros/stbimage/`, fila en `docs/terceros.md`.

## Compilar

`make` lo incluye (`userland/juegos/aoen/aoen.mk`): `/apps/bin/aoen` y `/apps/bin/aoen-config`. Sin los DRS el juego
muestra las instrucciones en su ventana.

## Efectos gráficos (Opciones > Gráficos)

Tres perfiles: **Equipos lentos** (todo apagado: el AoEN original), **Normal** (transiciones, agua, niebla suave, sombras,
partículas, obras y ruinas; por defecto con SSE2) y **Completo** (más día y noche y zoom suave). Cada efecto se activa por
separado (perfil «Personalizado»). Para depurar: `AOEN_EFECTOS=0|1|2`, `AOEN_FX=<máscara de bits>` (1 transiciones, 2 agua,
4 niebla, 8 sombras, 16 partículas, 32 obras, 64 día y noche, 128 zoom suave). Interfaz: escala entera (x2 desde 1600x960).
Presentación: superficie de la ventana o renderizador 2D de la SDL (`render2d`, `AOEN_RENDER=1`).

## Variables de prueba

`AOEN_SCENE=game|fx|options`, `AOEN_SEED`, `AOEN_SELECT=1`, `AOEN_FULL=1` (pantalla completa a la resolución del escritorio),
`AOEN_BENCH=N` (N cuadros sin límite de 60 por segundo, con la cámara en vaivén, y sale). Cada suceso sale por el registro
(serie) como `[AOEN] ...`.

## Rendimiento (fps, escaramuza de 4 jugadores, cámara en vaivén)

Antes (sin rutas rápidas, división por píxel, ARGB, minimapa por cuadro; DRS sintéticos):

| Máquina | 800x600 | 1024x768 |
|---|---|---|
| QEMU TCG (sin KVM) | 26 | 20 |
| QEMU KVM | 226 | 116 |

Después (sin efectos, misma escena y medida con cámara en vaivén; los números de antes eran con cámara quieta):

| Máquina / efectos | 800x600 | 1024x768 |
|---|---|---|
| TCG, efectos 0 / 1 / 2 | 149 / 100 / 68 | 114 / 56 / 88 |
| KVM, efectos 0 / 1 / 2 | 1622 / 1225 / 923 | 1304 / 732 / 909 |

**Con los datos reales** (medida privada, KVM, cámara en vaivén, DRS originales sin texturas HD): 800x600, efectos 0 / 1 / 2:
1667 / 635 / 645 fps; 1024x768: 1212 / 488 / 494 fps. **Con las texturas HD opcionales** (terreno calculado píxel a píxel con
coma flotante) baja a unos 13 fps (800x600) y 10 fps (1024x768), con cualquier perfil: por eso el perfil *Equipos lentos* las deja
apagadas salvo que se pidan en Opciones. Acelerar ese terreno queda pendiente.

Con cámara quieta y efectos 0 (comparable con «antes»): TCG 57 y 53 fps, KVM 811 y 667 fps. Las medidas con TCG son ruidosas
(otros procesos en el equipo). Pendiente: Pentium III y
Athlon reales.

## Pruebas

`python3 tests/h389_aoen.py [generador licencia sin-datos con-datos efectos]` y, aparte, `bench`. Usan DRS sintéticos de
`tools/aoen_drs_sintetico.py`; nunca los reales.
