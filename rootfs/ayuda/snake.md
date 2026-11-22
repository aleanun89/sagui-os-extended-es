<!-- categoria: Juegos y aplicaciones; orden: 26; titulo: Snake -->
# Snake

**Snake** (Inicio > Juegos > Snake) es el clásico de la serpiente: come la fruta, crece y no choques con las paredes ni con
tu propio cuerpo. Cada cuatro frutas sube el nivel y el juego va más rápido.

## Controles

| Tecla | Acción |
|-------|--------|
| Flechas o W, A, S, D | cambian la dirección |
| Espacio | empieza la partida, o la repite tras un choque |
| P o Espacio | pausa y reanuda |
| Esc | cierra |

Los giros se aplican de uno en uno por paso, así que dos teclas muy seguidas no pueden hacer que la serpiente gire sobre su
propio cuello.

## Récord

El mejor resultado se guarda en tu perfil (`/home/.config/snake.conf`) y aparece como **RECORD** en cada partida.

## Desde el Terminal

    snake                  abre el juego
    snake --semilla 7      partida repetible (la misma fruta cada vez)
    snake --prueba         comprueba las reglas del juego sin abrir ventana

## Para aprender

La versión sencilla de Snake está en `/ejemplos/tcc/snake.c` (unas 360 líneas de C que se compilan con `tcc`). Muestra cómo se
hace un bucle de juego con la libsagui: el tablero como un array, el temporizador con `esperar_evento` y el teclado. La guía de
C de la Ayuda (Programación > C) la recorre en «Programas completos para estudiar».
