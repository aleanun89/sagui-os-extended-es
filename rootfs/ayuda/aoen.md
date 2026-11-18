# AoEN: estrategia en tiempo real

**AoEN** es un juego de estrategia en tiempo real al estilo de Age of Empires: construyes una aldea, recoges recursos,
avanzas de edad y derrotas a los demás jugadores (la IA). Se abre desde **Inicio > Juegos > AoEN**.

## Los datos del juego

AoEN **no trae** los gráficos del juego original: necesita una copia legítima de Age of Empires con estos archivos:
`GRAPHICS.DRS`, `INTERFAC.DRS` y `TERRAIN.DRS` (y `SOUNDS.DRS` para los sonidos). Cópialos a **`/jogos/AOE`** (dentro de
`DATA` o `GAME/DATA`, como en el CD) y AoEN los encuentra solo. También sirven `aoen-config --set RUTA`, la variable
`AOEN_DATA` o `aoen RUTA`. Sin los datos, la ventana te lo explica.

## Cómo se juega

- **Ratón:** clic izquierdo selecciona; arrastrar dibuja una caja; clic derecho ordena (mover, recolectar, atacar,
  construir). La rueda cambia el zoom; el minimapa de abajo a la derecha mueve la cámara.
- **Teclado:** flechas o WASD mueven la cámara; `H` va al centro urbano; `.` elige un aldeano parado; `+` y `-` cambian la
  velocidad; `Ctrl` + número guarda un grupo y `Mayús` + número lo recupera; `F5` guarda, `F9` carga, `Esc` pausa y `F11` pone
  la pantalla completa.
- **Aldeano:** botones de construcción (teclas 1 a 9). **Edificios:** botones de entrenar y de mejoras.

## Opciones

En **Opciones > Gráficos** eliges el perfil de efectos: **Equipos lentos** (todo apagado, el más rápido), **Normal** (por
defecto en equipos con SSE2) o **Completo**, y cada efecto por separado: transiciones del terreno, agua animada, niebla
suave, sombras, humo y polvo, obras y ruinas, día y noche y zoom suave. También el tamaño de la interfaz (Automático escala
x2 desde 1600x960) y la pantalla completa. En un equipo antiguo (Pentium III o Athlon) a 800x600, usa **Equipos lentos**.

Texturas HD y música (opcionales, tuyas): carpetas `/apps/aoen/texturas` y `/apps/aoen/musica`. La configuración y las
partidas están en `~/.config/aoen/`.

Más detalles: `docs/aoen.md` del repositorio.
