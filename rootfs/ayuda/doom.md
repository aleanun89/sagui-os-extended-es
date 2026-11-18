# DOOM (Chocolate Doom)

**DOOM** se abre desde **Inicio > Juegos > DOOM**. Es Chocolate Doom, un motor fiel al DOOM original de 1993, con
Heretic, Hexen y Strife. **Freedoom** (Inicio > Juegos > Freedoom) es el mismo juego con contenido libre.

## Los datos del juego

- **Freedoom** viene con el sistema en `/apps/doom` (`freedoom1.wad` y `freedoom2.wad`): el juego funciona nada más instalarlo.
- Si tienes el DOOM original, copia `doom.wad`, `doom2.wad`, `plutonia.wad` o `tnt.wad` a **`/jogos/DOOM`** (al compilar una
  imagen propia: `input_share/games/DOOM`). **DOOM** usa el original si lo encuentra; **Freedoom** fuerza el libre
  (`chocolate-doom --freedoom`). Heretic, Hexen y Strife (`chocolate-heretic`, `chocolate-hexen`, `chocolate-strife` en una
  terminal) necesitan su WAD original en `/jogos/DOOM`; no hay versión libre.
- DOOM va en `/apps` (y en las imágenes con `/apps` en partición); en el ISO o la imagen «completa» no viene, y las entradas de Inicio > Juegos no salen. Sin ningún WAD, una ventana te explica dónde ponerlos.

## Cómo se juega

- **Teclado:** flechas o WASD mueven y giran, `Ctrl` dispara, `Espacio` abre puertas y pulsadores, `Mayús` corre, `Tab` abre
  el mapa, `1` a `7` cambian de arma y `Esc` abre el menú. **Ratón:** mover gira, el botón izquierdo dispara y el derecho avanza.
- **F5** y **F8** (guardar y cargar), **F11** gamma, **Alt+Intro** pantalla completa.
- Opciones útiles (en una terminal): `chocolate-doom -warp 1 1 -skill 3`, `-fullscreen`, `-nomusic`, `-nosound`.
- **Rendimiento:** en un equipo antiguo baja el detalle (F5 en el menú de DOOM, o `-nosound`), y juega en ventana pequeña.
  `chocolate-doom -timedemo demo1` mide los cuadros por segundo.

La configuración y las partidas guardadas están en `~/.config/chocolate-doom/`. Más en `docs/doom.md`.
