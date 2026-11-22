# Renderizador de la SDL 2 por la GPU (v8.3, D288)

La SDL 2 de SaguiOS (`userland/libsdl2/sdl2_sagui.c`) dibuja siempre por software sobre el lienzo
lógico de la ventana y manda al sistema solo lo que cambia (D247). Desde la v8.3, si el programa
pide `SDL_CreateRenderer(..., SDL_RENDERER_ACCELERATED)` y hay una GPU con 3D (capa común `gpu3d`,
`docs/gpu3d.md`, D285), el renderizador lleva el lienzo **en la GPU**. Si no la hay, o algo falla,
sigue por software **sin que el programa lo note**.

## Qué va a la GPU

| SDL 2 | En la GPU |
| --- | --- |
| `SDL_CreateTexture`, `CreateTextureFromSurface`, `UpdateTexture`, `Lock/UnlockTexture` | la textura se sube a uno de los **32** huecos de `gpu3d` (D436; 8 con un kernel de antes de la D425) cuando se usa y cambió (la copia de la CPU, `t->px`, se conserva siempre; si no caben, se desaloja la menos usada y el registro dice `faltan huecos de textura`: mejor un atlas grande que muchas texturas pequeñas). No hay subida parcial: una textura que cambia se sube entera |
| `SDL_RenderCopy` / `SDL_RenderCopyEx` | un cuadrilátero con textura: escalado (vecino más próximo), espejo, giro alrededor de un centro, modulación de color y de alfa (`SetTextureColorMod/AlphaMod`) |
| `SDL_RenderGeometry` (D436) | triángulos con color por vértice (que multiplica la textura y su modulación); coordenadas de textura fuera de [0, 1] **repiten** la textura si sus lados son potencia de 2 (extensión de SaguiOS: la SDL real las rechaza) |
| Modos de mezcla (D436) | por la GPU: `NONE`, `BLEND`, `ADD` y el alfa premultiplicado (`SDL_ComposeCustomBlendMode(ONE, ONE_MINUS_SRC_ALPHA, ADD, ...)`); `MOD`, `MUL` y los demás compuestos, por software |
| `SDL_RenderSetClipRect` (D436) | los polígonos se recortan con la CPU antes de mandarlos (no hace falta la tijera de gpu3d v2) |
| `SDL_RenderClear`, `FillRect`, `DrawPoint`, `DrawLine`, `DrawRect` | cuadriláteros de color (con `SetRenderDrawBlendMode` y alfa); las líneas horizontales y verticales son un solo rectángulo |
| `SDL_RenderPresent` a **pantalla completa** (D432) | la GPU copia el cuadro, escalado y con sus bandas negras, **directamente a la pantalla** (`GPU3D_OP_APRESENTAR`): no vuelve a la RAM |
| `SDL_RenderPresent` en ventana, `SDL_RenderReadPixels` | el cuadro vuelve de la GPU al lienzo lógico (`GPU3D_OP_LER`) y sigue el camino de siempre |

Todo se recorta al lienzo (o al recorte) con Sutherland-Hodgman antes de mandarlo (el kernel solo admite 2 píxeles
de margen; lo que cae dentro no pasa por el recorte) y los vértices se acumulan por lotes (hasta **3072**, D436, el
máximo del kernel); un cambio de textura, de mezcla o de repetición vacía el lote: las copias consecutivas con la misma
textura y el mismo estado van en una sola llamada. Dibujar sobre una textura (`SDL_SetRenderTarget`) sigue siendo por
software.

**Lo que cuesta en la GPU** (medido con AoEN en el Amilo, D436): cada triángulo (en R5xx los vértices van por la cola del
CP, y con más de unos 680 triángulos pendientes la CPU espera a que la GPU acabe), cada textura que se sube entera y cada
píxel mezclado. Un juego que quiera ir rápido: pocas texturas grandes (atlas), pocos triángulos grandes y sin mezcla donde no
haga falta.

## Cuándo se usa

- Solo con `SDL_RENDERER_ACCELERATED` (y sin `SDL_RENDERER_SOFTWARE`). El resto de los programas
  (los ports piden `0` o `SDL_RENDERER_TARGETTEXTURE`) no cambian.
- `SDL_SAGUI_GPU=1` (el nombre de la D432) o `SAGUI_SDL2_GPU=1` (o `sagui_sdl2_gpu = 1` en el programa) lo fuerza para cualquier renderizador que
  no pida `SOFTWARE`: sirve para medir los ports en una máquina con 3D; `SAGUI_SDL2_GPU=0` lo apaga.
- Vuelve al software (con lo ya dibujado traído al lienzo) si: no hay driver 3D o su autoprueba
  falló; el destino no cabe en la memoria de la GPU; cambia el tamaño del lienzo; una textura mide más de
  2048; la GPU rechaza algo; o el programa dibuja por software sobre el lienzo de la ventana. Si la
  GPU misma falla (la capa la apaga) se pierde el cuadro en curso, no más. El log lo dice:
  `[SDL2] renderizador por GPU: destino 640x400`, `renderizador por software (sin GPU 3D...)`,
  `renderizador por GPU apagado: motivo (sigue por software)`.

## Presentar sin volver a la RAM (D432, v8.5.5.2)

A pantalla completa, y si el driver 3D sabe (Radeon R5xx, R6xx/R7xx y Evergreen/NI; no Intel Gen3), el cuadro **no
vuelve a la RAM**: el compositor cede la pantalla a la ventana y la GPU escribe en ella (`docs/gpu3d.md`, «Presentar
sin volver a la RAM»). El programa no cambia nada: es lo que hace `SDL_RenderPresent`.

- **Una textura por cuadro** (lo que hacen DOOM, AoEN con el renderizador 2D y los emuladores: `SDL_UpdateTexture` +
  `SDL_RenderCopy` a todo el lienzo + `SDL_RenderPresent`): la copia se aplaza y la GPU lleva **la textura** a la pantalla,
  escalada. La CPU solo sube la textura a la VRAM; ni escala ni copia más.
- **Dibujo suelto** (sprites, rellenos): la GPU copia el destino del 3D a la pantalla.
- **Cuando no se puede** (un aviso en pantalla, el panel de Alt+Tab, un menú, el salvapantallas, el bloqueo, varias
  pantallas, la ventana sin foco): ese cuadro va por el camino de siempre, y vuelve a ir directo cuando el motivo
  desaparece. En ventana (no a pantalla completa) siempre es el camino de siempre.
- Lo que el kernel deja en el registro: `[GUI] pantalla exclusiva: cedida a la ventana N` / `la devuelve tras M cuadros
  (motivo)`; la SDL: `presentar por la GPU en la pantalla (D432): lienzo LxA -> ...`.

### Cómo se enciende (sigue siendo opcional: por defecto ningún juego cambia)

Hace falta (1) un **driver 3D activo** (en las Radeon no se enciende solo: `gpu3d encender`, o una imagen hecha con
`--gpu3d=radeon`; `gpu3d` debe decir «presentar en la pantalla (D432): si»), (2) que el juego use el **renderizador por
GPU** de la SDL y (3) **pantalla completa**.

| Variable | Qué hace |
| --- | --- |
| `SDL_SAGUI_GPU=1` | renderizador por GPU para cualquier programa de la SDL 2 que no pida el de software (`=0`: nunca). Es el nombre nuevo de `SAGUI_SDL2_GPU` (D288), que sigue valiendo; también vale Configuración > Pantalla > Aceleración gráfica (D323) |
| `SDL_SAGUI_GPU_PRESENTAR=0` | con la GPU, presentar como antes de la D432 (traer el cuadro a la RAM): para comparar |
| `SDL_SAGUI_GPU_VSYNC=1` | esperar al retrazado vertical antes de copiar (menos desgarro; como mucho los Hz de la pantalla) |
| `SDL_SAGUI_GPU_FILTRO=1` | escalado bilineal (por defecto, vecino más próximo, como el software) |
| `SDL_SAGUI_GPU_TIEMPOS=1` | D436: cada 5 s, en el registro, cuánto duran las llamadas al 3D por operación (la más larga y cuántas pasan de 20 y de 80 ms); mientras dura una, el kernel no atiende a nadie más |

| Juego | Cómo presenta | Cómo se enciende |
| --- | --- | --- |
| **AoEN** | fase 2 (D436, v8.5.5.2r2): **la partida se dibuja por la GPU** (terreno, sprites del atlas y la interfaz en una capa); los menús, un cuadro hecho por la CPU en una textura | `AOEN_GPU=1 aoen` o Opciones > Gráficos, «Dibujo por GPU» (`dibujo_gpu = 1`); solo la fase 1: `AOEN_RENDER=1` o `render2d = 1`. Los dos piden el acelerado ellos solos y siguen **apagados por defecto**. En R7xx/Evergreen el lienzo no puede pasar de 1024x1024: con pantallas mayores, «Resolución de dibujo» al 75 % o al 50 % |
| **DOOM** y los demás Chocolate (Heretic, Hexen, Strife) | textura por cuadro | `SDL_SAGUI_GPU=1 chocolate-doom` |
| **Command & Conquer** (`conquer`), **OpenJazz**, **Freeciv** | textura por cuadro (piden `SDL_CreateRenderer` sin pedir el acelerado) | `SDL_SAGUI_GPU=1 conquer` (etc.) |
| **mGBA**, **gnuboy**, **LaiNES**, Gearsystem | textura por cuadro | su opción de renderizador acelerado, o `SDL_SAGUI_GPU=1` |
| **Tzar**, **OpenTTD** | pintan en la superficie de la ventana (`SDL_UpdateWindowSurface`), sin renderizador | `SDL_SAGUI_GPU=1 tzar`: **experimental**; el lienzo entero se sube a la VRAM cada vez que cambia (sin subida parcial), así que en un juego de estrategia con la pantalla casi quieta puede no ganar: medirlo |
| OpenTyrian, Quake, DOSBox... (SDL 1.2 o propios) | no usan la SDL 2 | no les afecta |

`/apps/bin/sdl2presentar` mide los tres caminos (directo, GPU con vuelta a la RAM, software) sin depender de ningún
juego: ver «Qué ejecutar en cada máquina» en `docs/gpu3d.md`. Con `--sdl-medir` los ports escriben sus fps.

## Lo que no hace todavía

- **Presentar en ventana sin copiar a la CPU:** fuera de la pantalla completa, cada cuadro se trae a la RAM
  (`GPU3D_OP_LER`) y de ahí va a la ventana. En Intel Gen3 no hay presentación directa.
- **Camino intermedio por el acelerador 2D (D195):** el 2D no tiene llamada para programas
  (solo lo usa el compositor del kernel), así que no hay copias ni rellenos acelerados desde la SDL;
  añadirla tocaría el kernel central y queda fuera de esta versión. Sin 3D, la SDL va por software.
- Filtro bilineal (`SDL_HINT_RENDER_SCALE_QUALITY`), ventana gráfica (`SDL_RenderSetViewport`), `MOD`/`MUL` por la GPU
  (necesitan los factores de color de destino de gpu3d v2) y dibujar en texturas por la GPU. `SDL_RenderGeometry`, el
  recorte y `ADD` ya están (D436).
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
