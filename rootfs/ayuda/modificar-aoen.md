# Modificar AoEN y recompilarlo dentro de SaguiOS

**AoEN** (el juego de estrategia de Inicio > Juegos) está escrito en C y sus fuentes vienen con el sistema, en
`/apps/aoen/fuentes`. Con **TCC** (`/ayuda/tcc.md`) lo puedes cambiar y recompilar aquí mismo en un segundo.

## Preparar una copia

Trabaja sobre una copia: el AoEN del sistema (`/apps/bin/aoen`) no cambia.

    cp -r /apps/aoen/fuentes ~/aoen
    cd ~/aoen
    sh compilar.sh                 deja ./aoen
    ./aoen                         tu versión (con los datos en /jogos/AOE, como siempre)

Para cambiar los ficheros usa el **Editor** o el **Mico** (`mico fichero.c`). Si algo no compila, TCC dice el
fichero y la línea del error. Si te pierdes, vuelve a copiar los fuentes.

**Truco:** `sh compilar.sh -b` compila con **comprobación de límites**: si un cambio tuyo escribe fuera de una
tabla o usa memoria liberada, el juego se para y dice el fichero y la línea. Va más lento; úsalo para buscar errores.

## Dónde está cada cosa

| Fichero | Qué tiene |
|---|---|
| `game.c` | las reglas: tabla de unidades (`unit_def`), de edificios (`bld_def`), de tecnologías (`tech_def`), recolección, combate |
| `game.h` | las estructuras y la lista de tecnologías (`TechId`) |
| `ia.c` | la inteligencia de la máquina; sus niveles de dificultad en la tabla `niveles` |
| `interfaz.h` | colores (`COL_...`) y medidas de la interfaz |
| `ui_menu.c`, `ui_hud.c`, `ui_paneles.c` | menús, barra de abajo y paneles |
| `render.c`, `terreno.c` | el dibujo del mapa y de las unidades |

## Ejercicio 1: el coste y la vida de una unidad

En `game.c`, la tabla `unit_def` tiene una fila por unidad. Las columnas: nombre, puntos de vida, madera, comida,
oro, piedra, tiempo de entrenamiento, velocidad, daño, daño a edificios, alcance, edad...

    { "Aldeano",        25,  0,  50,   0, 0, 15, 1.4f, ...

Cambia el aldeano para que tenga 40 de vida y cueste 30 de comida:

    { "Aldeano",        40,  0,  30,   0, 0, 15, 1.4f, ...

Recompila, empieza una partida y mira la ficha del aldeano (abajo a la izquierda) y el coste en el centro urbano.

## Ejercicio 2: una tecnología nueva

1. En `game.h`, añade `T_SANDALS` justo antes de `T_COUNT` (la lista `TechId`).
2. En `game.c`, al final de la tabla `tech_def` (después de «Logística»), añade su fila (nombre, efecto, edificio,
   edad, madera, comida, oro, piedra, tiempo, requisito):

        { "Sandalias", "Aldeanos: velocidad +15%", E_MARKET, AGE_BRONZE, 50, 50, 0, 0, 25, -1 },

3. Dale su efecto. En `game.c`, en la función `walk_step`, después de la línea de la tecnología «Rueda» (`T_WHEEL`):

        if (e->owner >= 0 && has_tech(e->owner, T_SANDALS) && e->type == E_VILLAGER) speed *= 1.15f;

Recompila: en la Edad del Bronce, el mercado ofrece «Sandalias». Su botón sale con el primer dibujo de la tabla de
iconos: en `ui_comun.c`, la función `icono_mejora` tiene `fr[T_COUNT]`, un número de cuadro por tecnología; la nueva
no tiene número y usa el 0. Para darle otro, añade un número al final de esa lista (por ejemplo `14`, el de «Rueda»).

## Ejercicio 3: una máquina más agresiva

En `ia.c`, la tabla `niveles` tiene una fila por dificultad. En la de **normal**, el 8 es el número de soldados del
primer ataque y el `10.0f`, los minutos que espera antes de atacar:

    { 1.0f, 14, 5, 34, 2, { 0, 12, 18, 24 }, { 0, 0, 0, 0 }, 1, 8, 3, 10.0f, ...

Prueba con `1, 5, 3, 4.0f`: la máquina normal atacará antes y con menos soldados. Juega una partida en «normal».

## Ejercicio 4: el color de la interfaz

En `interfaz.h`, `COL_RED` y `COL_REDH` son el rojo de los botones (normal y con el ratón encima), en
`gfx_rgb(rojo, verde, azul)`. Para botones azules:

    #define COL_RED    gfx_rgb(30, 60, 150)
    #define COL_REDH   gfx_rgb(50, 90, 190)

Recompila y mira el menú principal. Con `sed` también se puede, sin abrir el editor:

    sed -i 's/gfx_rgb(150, 28, 24)/gfx_rgb(30, 60, 150)/' interfaz.h

## Para seguir

- `[AOEN]` en los mensajes: el juego escribe en el registro del sistema lo que hace (`AOEN_LOG`).
- AoEN es software libre (GPL 3 o posterior): puedes compartir tus cambios con la misma licencia.
