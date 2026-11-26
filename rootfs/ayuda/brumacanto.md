# Brumacanto

**Brumacanto** (Inicio > Juegos > Brumacanto) es un juego de exploración, minería y construcción en primera persona con
bloques. Está hecho con SDL 2 y OpenGL ES 1.1, y SaguiOS lo dibuja con SaguiGL (por software, o por la GPU si el driver
la acelera). Código MIT; gráficos CC0 de Kenney y algunas texturas CC BY-SA 3.0 de Minetest Game (créditos en `/apps/brumacanto/CREDITOS-luanti.md`).

## Controles

WASD y ratón: moverse y mirar. Espacio: saltar. Mayús: correr. Clic izquierdo: minar o atacar. Clic derecho: colocar.
1 a 9 o la rueda: objeto. E: mochila y fabricación. F: comerciar. F2: supervivencia o creativo. **F5: guardar.**
**F3: distancia de dibujo** (32, 48, 64, 80 o 104 bloques; se guarda y vale la vez siguiente). **F4: escala de dibujo** (100, 75 o 50 %; también se guarda). **F6: límite de FPS** (60, 30 o ilimitado; se guarda).
**F11: pantalla completa.** Esc: pausa.

## Minerales, horno y herramientas (D449)

Bajo tierra hay **cobre**, **diamante** (solo muy hondo), **obsidiana** y **lava** en las cuevas más profundas (quema: no te metas).
En las zonas frías crecen **pinos**; bajo la arena hay **arenisca**. Se fabrica madera de pino, ladrillo de piedra, arenisca y
un **horno**: con el horno en la mochila puedes fundir hierro o cobre con carbón (E > fabricación). Con lingotes de hierro o diamantes
haces **herramientas de hierro y de diamante**, más rápidas y duraderas. Las herramientas **se desgastan** (barra verde bajo su icono)
y se rompen. El diamante y la obsidiana solo se sacan con pico de hierro o mejor. Las partidas antiguas siguen cargando.

## Rendimiento

**La primera vez** el juego elige la distancia de dibujo según el equipo: **48** si no hay 3D por la GPU o la GPU es antigua
(sin gpu3d v3), **64** con v3 y **104** si unos cuadros medidos al arrancar van sobrados. Lo que elijas luego con F3 se
guarda, junto con la escala de F4, en `/home/brumacanto/ajustes.txt` (un solo fichero; el `escala.txt` de antes se sigue leyendo).
`brumacanto --view 48` manda solo para esa vez y `brumacanto --view auto` vuelve a elegirla según el equipo.
Por software el juego va más suave con una distancia de dibujo menor: pulsa F3 o abre una terminal con
`brumacanto --view 48`. El mundo no se guarda entero: se genera por sectores al explorar (como mucho 16x16 en memoria),
así que cualquier tamaño de mundo cabe en una máquina de 1 o 2 GB. Con el 3D por la GPU (`gpu3d probar`), a pantalla
completa (F11 o `brumacanto --fullscreen`) el cuadro va directo a la pantalla. La escala de dibujo al 75 o al 50 %
(F4 o `--escala 50`) dibuja a menos resolución y la amplía: sobre todo ayuda por software. `brumacanto --bench 200` mide los cuadros por segundo.

Las partidas se guardan en `/home/brumacanto/`. Más en `docs/brumacanto.md`.
