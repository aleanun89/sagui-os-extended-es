# Renderizador de la SDL 2 por la GPU (v8.3, D288)

La SDL 2 de SaguiOS (`userland/libsdl2/sdl2_sagui.c`) dibuja siempre por software sobre el lienzo
lógico de la ventana y manda al sistema solo lo que cambia (D247). Desde la v8.3, si el programa
pide `SDL_CreateRenderer(..., SDL_RENDERER_ACCELERATED)` y hay una GPU con 3D (capa común `gpu3d`,
`docs/gpu3d.md`, D285), el renderizador lleva el lienzo **en la GPU**. Si no la hay, o algo falla,
sigue por software **sin que el programa lo note**.

## Qué va a la GPU

| SDL 2 | En la GPU |
| --- | --- |
| `SDL_CreateTexture`, `CreateTextureFromSurface`, `UpdateTexture`, `Lock/UnlockTexture` | la textura se sube a uno de los 8 huecos de `gpu3d` cuando se usa y cambió (la copia de la CPU, `t->px`, se conserva siempre; si hay más de 8 texturas se desaloja la menos usada) |
| `SDL_RenderCopy` / `SDL_RenderCopyEx` | un cuadrilátero con textura: escalado (vecino más próximo), espejo, giro alrededor de un centro, mezcla alfa (`SDL_BLENDMODE_BLEND`), modulación de color y de alfa (`SetTextureColorMod/AlphaMod`) |
| `SDL_RenderClear`, `FillRect`, `DrawPoint`, `DrawLine`, `DrawRect` | cuadriláteros de color (con `SetRenderDrawBlendMode` y alfa); las líneas horizontales y verticales son un solo rectángulo |
| `SDL_RenderPresent`, `SDL_RenderReadPixels` | el cuadro vuelve de la GPU al lienzo lógico (`GPU3D_OP_LER`) y sigue el camino de siempre |

Todo se recorta al lienzo con Sutherland-Hodgman antes de mandarlo (el kernel solo admite 2 píxeles
de margen) y los vértices se acumulan por lotes (hasta 1020); un cambio de textura o de mezcla
vacía el lote. Dibujar sobre una textura (`SDL_SetRenderTarget`) sigue siendo por software.

## Cuándo se usa

- Solo con `SDL_RENDERER_ACCELERATED` (y sin `SDL_RENDERER_SOFTWARE`). El resto de los programas
  (los ports piden `0` o `SDL_RENDERER_TARGETTEXTURE`) no cambian.
- `SAGUI_SDL2_GPU=1` (o `sagui_sdl2_gpu = 1` en el programa) lo fuerza para cualquier renderizador que
  no pida `SOFTWARE`: sirve para medir los ports en una máquina con 3D; `SAGUI_SDL2_GPU=0` lo apaga.
- Vuelve al software (con lo ya dibujado traído al lienzo) si: no hay driver 3D o su autoprueba
  falló; el destino no cabe en la memoria de la GPU; cambia el tamaño del lienzo; una textura mide más de
  2048; la GPU rechaza algo; o el programa dibuja por software sobre el lienzo de la ventana. Si la
  GPU misma falla (la capa la apaga) se pierde el cuadro en curso, no más. El log lo dice:
  `[SDL2] renderizador por GPU: destino 640x400`, `renderizador por software (sin GPU 3D...)`,
  `renderizador por GPU apagado: motivo (sigue por software)`.

## Lo que no hace todavía

- **Presentar sin copiar a la CPU:** `gpu3d` no comparte búferes con el compositor, así que cada cuadro
  se trae a la RAM (`GPU3D_OP_LER`) y de ahí va a la ventana. En Intel Gen3 esa lectura es por la
  apertura sin caché (lenta); es **lo que habrá que medir** en una máquina real (abajo). Cuando el
  compositor pueda usar el búfer de la GPU directamente, `SDL_RenderPresent` dejará de copiar.
- **Camino intermedio por el acelerador 2D (D195):** el 2D no tiene llamada para programas
  (solo lo usa el compositor del kernel), así que no hay copias ni rellenos acelerados desde la SDL;
  añadirla tocaría el kernel central y queda fuera de esta versión. Sin 3D, la SDL va por software.
- Filtro bilineal (`SDL_HINT_RENDER_SCALE_QUALITY`), ventana gráfica (`SDL_RenderSetViewport`),
  `SDL_RenderGeometry`, el modo de mezcla `ADD`/`MOD` y dibujar en texturas por la GPU.
- Los píxeles no son bit a bit iguales a los del software en los bordes del escalado no entero y del giro
  (la GPU muestrea en el centro del píxel, el software en la esquina).

## Medir

`/apps/bin/sdl2gpu [cuadros]` dibuja once escenas (limpiar, rellenos, copia 1:1, escalada x2 y 3/4,
espejos, mezcla alfa, modulación, giro, recortada, líneas) con un renderizador por software sobre una
superficie y con el acelerado de la ventana, y compara píxel a píxel; escribe además los ms de cada uno.
`tests/h288_sdl_gpu.py` lo ejecuta sin GPU 3D (idéntico) y con `--gpu3d=simulado`.

Con `--sdl-medir` (o `SAGUI_SDL2_MEDIR=1`) los ports escriben «cuadro: juego X us, paleta Y us, escalado Z us,
presentar W us»: con la GPU, «escalado» es el tiempo de mandar órdenes y «presentar» incluye traer el cuadro.

### Qué probar en el dv7 / Amilo (sin confirmar)

1. `gpu3d probar` (la autoprueba tiene que dar BIEN) y `sdl2gpu 120`: `gpu=1`, escenas dentro de los límites
   de h288 y los ms «acelerado» frente a «software».
2. `SAGUI_SDL2_GPU=1 /apps/bin/conquer --sdl-medir` (y `openjazz`, `opentyrian`, `gnuboy` con su opción
   de renderizador acelerado): comparar los `fps` con los de `SAGUI_SDL2_GPU=0`. Esperable: ganar en
   escalados grandes con mezcla; perder si la lectura del cuadro (apertura sin caché) domina. Si pierde,
   dejarlo solo con `SDL_RENDERER_ACCELERATED` explícito (es lo que hay).
3. Que `gpu3d` no se apague (`gpu3d` tras jugar: «fallos 0») y que al cerrar el programa el motor quede libre.
