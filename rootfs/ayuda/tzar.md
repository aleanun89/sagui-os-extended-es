# Tzar

**Tzar** se abre desde **Inicio > Juegos > Tzar**. Es el port a SaguiOS de *Tzar: The Burden of the Crown* (Haemimont,
1999), un juego de estrategia en tiempo real, escrito en C a partir de ReTzar (un remake libre, GPL-3.0-or-later). Trae
la interfaz original, las seis campañas con sus historias y conversaciones, las escaramuzas contra la IA original, los
mapas aleatorios, el sonido y la música.

## Los datos del juego

Los datos de Tzar **no vienen con SaguiOS**: son del juego comercial y hay que tener una copia propia (por ejemplo la
edición *Tzar Anthology*).

- Copia la carpeta de la instalación (la que tiene **`Packs`**, **`Maps`** y **`Campaign`**, y si quieres `intro.MPG`)
  a **`/jogos/TZAR`**. No hacen falta el `Tzar.exe` ni las DLL.
- También puedes indicar otra carpeta: `tzar /ruta/a/Tzar` en una terminal, o la variable `TZAR_DATA`.
- Al compilar una imagen propia: pon la instalación en **`input_share/Tzar Anthology`** (o en
  `input_share/games/TZAR`); la imagen la copia a `/jogos/TZAR`. Necesita unos 500 MB en la partición de juegos
  (`make JOGOS_MB=1500`).
- Si faltan los datos, Tzar abre una ventana que lo explica y dice qué carpeta ha buscado.

## Cómo se juega

- **Menú:** *Un solo jugador* lleva a las campañas, a la escaramuza (un mapa de la instalación o uno aleatorio, contra
  la IA) y a cargar una partida. *Opciones* tiene el volumen del sonido y de la música.
- **Mapa aleatorio:** su pantalla deja elegir el tamaño, el tipo de mapa (islas, lagos, continente...), el terreno, los
  recursos, las torres de guardia, los animales, los objetos y las puertas mágicas, los reinos con su civilización y su equipo
  (los del mismo equipo son aliados) y las reglas. A la derecha de las reglas se ve el mapa (clic para verlo grande) y
  *Otro mapa* cambia la semilla. Lo último elegido se recuerda.
- **Ratón:** clic o arrastre para seleccionar, **clic derecho** para la orden por defecto (andar, atacar, recoger,
  talar, construir, reparar, entrar). Los botones del panel izquierdo dan las órdenes, construyen y entrenan.
- **Teclado:** las letras de los botones, `+` y `-` para la velocidad, **F5** guarda la partida rápida y **F9** la carga,
  **Esc** abre el menú de la partida (seguir, guardar, cargar, salir).
- En las historias y conversaciones de las campañas: clic o Intro para seguir; las respuestas, con el ratón o 1, 2, 3...

Las partidas guardadas y el progreso de las campañas están en `~/.tzar/`. El visor de mapas sigue disponible con
`tzar --visor` (en una terminal). Más en `docs/tzar.md`.
