<!-- categoria: Conoce SaguiOS/Sistemas operativos; orden: 13; titulo: Anexo, la biblioteca SDL en SaguiOS -->
# Anexo, la biblioteca SDL en SaguiOS

[Español](anexo-sdl.md) · [Português](anexo-sdl_PT.md) · [English](anexo-sdl_EN.md)

> Qué es SDL, por qué SaguiOS tiene la suya propia, cómo encaja con el sistema y qué hubo que hacer, con los temas de este curso, para portar juegos y programas.

## 1. Teoría

### 1.1 Qué es SDL

SDL (*Simple DirectMedia Layer*) es una biblioteca que da a un programa ventanas, dibujo, teclado, ratón, audio, hilos y temporizadores con una interfaz
única, sea cual sea el sistema. SDL 1.2 es la versión clásica; SDL 2 es la actual, con renderizador, texturas y eventos más completos. En Linux se apoya en
X11 o Wayland; en Windows, en sus propias APIs.

### 1.2 Por qué hace falta una capa

Un programa que usa SDL solo conoce la interfaz; la implementación cambia según el sistema. Por eso SaguiOS no depende de X11 ni de Wayland (no los tiene):
implementa la interfaz de SDL sobre su propia biblioteca gráfica, la libsagui. El juego se compila sin cambios.

### 1.3 Ventana, lienzo, evento y cuadro

- **Ventana**: lo que el sistema muestra.
- **Lienzo**: memoria de 32 bits donde se dibuja; se copia a la ventana al presentar.
- **Eventos**: una cola que se llena con el teclado, el ratón y la ventana; el programa la vacía con `SDL_PollEvent`.
- **Cuadro**: un ciclo de eventos, dibujo, presentación y espera. Un juego repite el cuadro decenas de veces por segundo.

### 1.4 Los temas del curso, aplicados

- **Hilos**: crear un hilo y esperar a que termine; qué puede hacer cada hilo.
- **Cerrojos y condiciones**: un mutex protege los datos y una condición avisa de un cambio; la espera debe repetirse en un bucle.
- **Audio**: el sonido se mezcla en una llamada de vuelta que corre en un hilo propio; si ese hilo no llega a tiempo, el sonido se corta.
- **Prioridades**: un hilo de audio puede pedir más CPU al planificador.
- **Tiempo**: el reloj del sistema y las esperas, sin temporizadores con llamada de vuelta.

## 2. Cómo encaja en SaguiOS

### 2.1 Lo que hay

- **Ventana y lienzo**: una ventana lógica del tamaño que pidió el programa; el lienzo se escala al tamaño de la pantalla, con bandas negras si hace falta, y
  en 320 × 200 y 640 × 400 se mantiene la proporción 4:3 de la VGA (`userland/libsdl2/sdl2_sagui.c`, decisión D204).
- **Teclado y ratón**: los códigos de tecla de SDL 2 salen del mapa de teclas del núcleo, así que hay KEYDOWN y KEYUP también para Mayús, Ctrl y Alt.
- **Audio**: por llamada de vuelta o por cola (`SDL_QueueAudio`); la conversión de formato, canales y frecuencia está en `SDL_BuildAudioCVT` y `SDL_ConvertAudio`
  (decisión D383). `SDL_PauseAudio(0)` arranca un **hilo de audio** propio que llama a la llamada de vuelta con el cerrojo del dispositivo tomado
  (decisión D393).
- **Renderizador por software y por GPU**: con `SDL_CreateRenderer(..., SDL_RENDERER_ACCELERATED)` el lienzo va a la GPU si hay una con 3D; si no, sigue por
  software sin que el programa lo note (decisión D288, `docs/sdl-gpu.md`). `SDL_RenderGeometry` dibuja triángulos (D436 lo usa en AoEN).
- **Pantalla completa sin pasar por la RAM**: la GPU escribe el cuadro en la pantalla (decisión D432).
- **OpenGL ES 1.1**: `userland/libsdl2/sdl2_gl.c` implementa `SDL_GL_*` sobre EGL y SaguiGL (decisión D425).

### 2.2 Dos bibliotecas: SDL 1.2 y SDL 2

- `userland/libsdl12/` es la SDL 1.2 común (`sdl_sagui.c`, decisión D228).
- `userland/libsdl2/` es la SDL 2 (`sdl2_sagui.c`, decisión D230), con sus cabeceras, `SDL_mixer`, `SDL_image` y `SDL_ttf` a su lado.
- Ejemplos de portes con estas bibliotecas: Command & Conquer (D230 y D247), DOOM con Chocolate Doom (D383), Freeciv (D374), mGBA (D322), Brumacanto (D444) y AoEN (D393, D431, D436).

**Qué no hay.** No hay `SDL_AddTimer` (devuelve error) ni temporizadores con llamada de vuelta, ni mandos, ni ratón relativo, y los cursores
propios se aceptan pero no se dibujan. El comentario de cabecera de `userland/libsdl2/SDL.h` decía que no había hilos ni OpenGL; ya no es cierto
(D393 añadió los hilos y `sdl2_gl.c` implementa OpenGL ES) y la cabecera está al día.

## 3. Lo que hubo que hacer, y por qué

### 3.1 Hilos: `SDL_CreateThread` sobre `thread_create`

Hasta la decisión D383, `SDL_CreateThread` fallaba y los cerrojos de la SDL no hacían nada. La decisión D393 los conectó con los hilos reales del sistema
(`thread_create` de la libsagui, decisión D167). Quedan dos reglas:

- Un hilo de trabajo **no llama** a vídeo, a eventos ni a audio: solo calcula y avisa al principal.
- Un programa que se llamaba a sí mismo con el cerrojo tomado sigue funcionando, porque el mismo hilo puede volver a tomarlo.

### 3.2 Cerrojos y condiciones

`SDL_CreateMutex`, `SDL_LockMutex` y `SDL_CondSignal` usan el `mutex_t` y el futex del sistema (decisión D150). Una condición puede despertar sin que nadie
la avise, así que la espera siempre va en un bucle que vuelve a comprobar el dato (el ejemplo `hilo_avisa.c` lo hace así).

Una espera de `SDL_CondWait` en el hilo principal se corta cada 2 ms (para hacer avanzar la OpenAL, que se mueve desde el principal) y vuelve a mirar
(D393). Quien espera a la llamada de vuelta del audio, como Chocolate Doom, recibe el aviso de verdad desde el hilo de audio.

### 3.3 Audio: un hilo propio para que no se corte el sonido

Al principio la SDL de SaguiOS movía el audio **desde el hilo principal**, una vez por cuadro, con unos 120 ms de adelanto: si un cuadro tardaba más
(Brumacanto tiene cuadros de 400 ms al crear trozos nuevos), el sonido se cortaba. La decisión D393 (ampliación del 2026-11-22) le dio a la SDL su
**hilo de audio**: `SDL_PauseAudio(0)` (la primera vez) lo crea con `thread_create`, y cada 5 ms mira cuánto queda por tocar en el flujo del sistema; si
es menos que la latencia del núcleo más 60 ms (como mínimo 120 ms), llama a la llamada de vuelta del programa **desde ese hilo** y le pasa el bloque.
Como el hilo no depende de los cuadros del juego, un cuadro lento ya no corta el sonido. `SDL_SAGUI_AUDIO_HILO=0` (variable de entorno o pista) vuelve al
modo de antes. AoEN lleva además su propio mezclador sin SDL (D431).

Consecuencia práctica: `SDL_LockAudio` y `SDL_LockAudioDevice` son ahora cerrojos de verdad (recursivos), y la llamada de vuelta corre con él tomado. Si el
programa toca desde otro hilo lo que comparte con la llamada de vuelta (un volumen, un contador, la posición de una música), debe tomarlo: son dos hilos y
sin él habría una condición de carrera (capítulo 7). Es el patrón de `tono.c`.

### 3.4 Prioridades: `PLAN_AUDIO`

La prioridad de audio del planificador es `PLAN_AUDIO`, decisión **D436** (la D431 es otra cosa: el audio de AoEN independiente de los FPS). Un hilo la pide
con la llamada del sistema; el hilo de audio de la SDL **solo la pide si se le dice** (`SDL_SAGUI_AUDIO_PRIO=1`, variable o pista), porque con ella Freeciv,
con cliente, servidor y música en la misma CPU, dejaba de contestar a los pings del servidor. Está limitada por uso (`PLAN_AUDIO_MAX_USO`, 5 tics de cada aprox. 250 ms,
alrededor del 20 % de una CPU), así que no sirve para acaparar la CPU. Es el tema del capítulo 5 en la práctica.

### 3.5 Temporizadores y reloj

Los programas usan `SDL_GetTicks` y `SDL_Delay`. `SDL_AddTimer` no está disponible (devuelve error), así que un programa que dependa de un temporizador
con llamada de vuelta debe hacer su propio bucle comparando el tiempo, como el ejemplo `tono.c`.

**Decisiones y límites.**
- **Decisión: una SDL propia sobre la libsagui.** Permite portar sin depender de X11 ni de Wayland (D230).
- **Decisión: renderizador por GPU opcional.** Si la GPU falla, el programa sigue por software (D288).
- **Decisión: un hilo de audio propio en la SDL.** La decisión D393 (ampliación) evita que un cuadro lento corte el sonido, a costa de que la llamada de vuelta
  corra en otro hilo y haya que protegerla con `SDL_LockAudio`.
- **Límite: la prioridad de audio no está activada de serie.** Si el equipo está muy cargado, el hilo de audio compite con los demás como cualquiera; se puede pedir
  `PLAN_AUDIO` con `SDL_SAGUI_AUDIO_PRIO=1`.
- **Límite: no hay temporizadores con llamada de vuelta.** Hay que usar bucles con el reloj.

## 4. Ejemplos

Tres programas pequeños están en `/ejemplos/sdl/` (en el repositorio, `rootfs/ejemplos/sdl/`). Según la guía de TCC (`tcc.md`), se compilan dentro de
SaguiOS desde una carpeta tuya, así:

```
cd /tmp
tcc /ejemplos/sdl/ventana_mueve.c -o ventana_mueve -lSDL2
tcc /ejemplos/sdl/hilo_avisa.c -o hilo_avisa -lSDL2
tcc /ejemplos/sdl/tono.c -o tono -lSDL2
```

Y se ejecutan con `./ventana_mueve`, `./hilo_avisa` y `./tono`.

- `ventana_mueve.c`: una ventana de 320 × 200 con un rectángulo que se mueve con las flechas (eventos y renderizador). Escape sale.
- `hilo_avisa.c`: un hilo calcula una suma y avisa al principal con una condición (hilos, mutex y condiciones).
- `tono.c`: un tono de 440 Hz generado en la llamada de vuelta del audio; al final escribe cuántas veces la llamaron y si lo hizo otro hilo (audio y reloj).

**Estado de la comprobación.** Los tres se han compilado y ejecutado dentro de SaguiOS con QEMU (`tests/curso_ejemplos.py`, con y sin KVM). `hilo_avisa`
escribe `suma de 1 a 1000000 = 500000500000`. `tono` dice que la llamada de vuelta corrió unas 85 veces en dos segundos y en otro hilo que el principal (la
imagen de prueba no tiene tarjeta de sonido, así que no se oye nada: el hilo de audio da el ritmo con el reloj). En `ventana_mueve`, seis flechas a la
derecha y tres hacia abajo mueven el rectángulo 24 y 12 puntos, y Escape cierra la ventana. La prueba sirvió para corregir dos cosas: `hilo_avisa.c`
sumaba con `long`, que en SaguiOS (32 bits) tiene 32 bits y desbordaba (escribía 1784293664 en vez de 500000500000; es el recordatorio de que en 32 bits
`long` y los punteros caben en 32 bits, capítulo 6), y el título «Rectángulo» salía como «RectAingulo» porque TCC deja las cadenas en UTF-8 y la ventana
las dibuja como Latin-1 (ahora el título va sin acento).

## 5. Preguntas de repaso

1. ¿Por qué SaguiOS implementa su propia SDL en lugar de usar X11 o Wayland? ¿Qué ventaja tiene para un juego ya escrito?
2. En `hilo_avisa.c`, ¿por qué la espera es un `while` y no un `if`? ¿Qué pasaría si el hilo principal llamara a `SDL_PollEvent` desde el hilo de trabajo?
3. Un juego tiene cuadros de 150 ms y el sonido no se corta con la SDL de serie. ¿Por qué? ¿Qué pasaría con `SDL_SAGUI_AUDIO_HILO=0`?
4. ¿Qué es `PLAN_AUDIO`, quién lo pide y por qué tiene un límite de uso?
5. ¿Por qué la llamada de vuelta del audio necesita ahora `SDL_LockAudio` para tocar un dato compartido con el programa?

## 6. Soluciones

1. X11 y Wayland no existen en SaguiOS, así que la capa propia es necesaria. Para un juego ya escrito contra la SDL, la ventaja es que se compila sin cambios:
   la interfaz es la misma y solo cambia la implementación.
2. Porque una condición puede despertar sin aviso, y el dato debe comprobarse de nuevo tras cada despertar. Y el hilo de trabajo no debe llamar a `SDL_PollEvent`,
   porque los eventos y el vídeo solo se tratan en el hilo principal.
3. Porque desde la decisión D393 el audio lo mueve un hilo propio de la SDL, que alimenta la tarjeta con unos 120 ms de adelanto sin depender de los cuadros
   del juego. Con `SDL_SAGUI_AUDIO_HILO=0` volvería al modo antiguo, en el que el principal mueve el audio una vez por cuadro: con cuadros de 150 ms, el sonido
   se cortaría. (AoEN, además, lleva su propio mezclador: D431.)
4. `PLAN_AUDIO` es la prioridad de audio del planificador (D436). La pide un hilo con la llamada del sistema. Tiene límite de uso para que un hilo no monopolice la CPU
   pidiendo siempre esa prioridad.
5. Porque la llamada de vuelta corre en el hilo de audio, distinto del hilo del programa: sin el cerrojo, los dos podrían tocar el mismo dato a la vez
   (condición de carrera, capítulo 7).
