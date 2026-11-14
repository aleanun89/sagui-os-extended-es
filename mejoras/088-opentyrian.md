# 088 - OpenTyrian: un juego de verdad dentro del SaguiOS

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-11

## Problema / motivación

El SaguiOS tenía juegos propios (el CHIP-8, el Macaco, el Sagui 3D), pero
**ningún juego de fuera**: el sistema no tenía con qué demostrar que puede
correr software de verdad, con gráficos, sonido, teclado y ratón, hecho para
DOS y reescrito en C.

El OpenTyrian (https://github.com/sfiera/opentyrian) es un tiro vertical de
1995 (el Tyrian de Epic MegaGames) portado a SDL: usa números reales, tiene
audio, reproduce animaciones y menús, y su código es pequeño. Es **GPL-2.0**,
que en este proyecto significa: aislado en `userland/terceros/opentyrian/` y
fuera de la variante sin GPL (D109).

## Propuesta

- **El motor** (`userland/terceros/opentyrian/jogo/`) es el del upstream, byte
  a byte, con **una sola** excepción: `jogo/file.c` gana las carpetas donde se
  buscan los datos (`/jogos/opentyrian` y `/usb/jogos/opentyrian`).
- **La plataforma** (`plataforma/`) es un **SDL 1.2** sobre la API de ventanas
  del SaguiOS: la ventana del juego (640×400), el teclado, el ratón, el sonido
  y el vídeo.
- **La libc** (`libc/`) es el mínimo que el juego pide (memoria, cadenas,
  ficheros, `printf` con `%f`, hora), sobre la API del sistema.
- **Los datos** (la versión shareware del Tyrian, que se distribuye libre) van
  en un **disco aparte**: no caben en la imagen del sistema (la meta son 16 MB,
  D122, y la imagen ya pasa de 11 MB). El juego los busca en el pendrive
  montado en `/usb`.
- **`tools/imagem_tyrian.sh`** arma ese disco (los 79 ficheros que el juego
  usa; fuera los ejecutables de DOS, la documentación y el `tyrend.anm` de 3 MB
  que es un *easter egg*). La imagen comprimida (1,8 MB) va versionada para
  que la prueba y el usuario la tengan sin más.

## Criterios de aceptación

- [x] El juego abre su ventana de 640×400 y dibuja la pantalla de título.
- [x] Encuentra sus datos (el pendrive se monta en `/usb` y el juego los ve).
- [x] Las teclas llegan bien: las flechas llegan como flechas y el Enter
      llega como Enter (no como una letra).
- [x] El navegador no se cae (ninguna falta de página).
- [x] El juego **no** entra en la variante sin GPL y la auditoría de licencias
      sigue cuadrando.
- [x] Test automático en `tests/` que lo comprueba (`h136_opentyrian.py`).
- [x] Decisión registrada en `docs/decisoes.md` (D156).

## Notas

- El juego usa números reales, así que compila con la **x87** (sin
  `-mgeneral-regs-only`), como el motor de PDF y el de la web.
- **El `Enter` era una letra**: el kernel da el código 10 al Enter, y el
  juego, que espera Ctrl+letra en ese código, recibía la letra `j` y el menú
  no respondía a nada. Ahora el Ctrl se distingue por los modificadores.
- Falta: el juego no está en el menú de Inicio (se arranca con
  `/bin/opentyrian` en el Terminal), y no hay partida guardada en el SO
  (`tyrian.sav` va al directorio del usuario, que el juego escribe donde
  puede).

## Pantalla completa (D204)

`--pantalla-completa` (o `-f`), Setup > Fullscreen, F11, Alt+Intro, o la entrada
«OpenTyrian (pantalla completa)» de Inicio > Juegos. Escalado 4:3 al alto de la
pantalla, vecino más próximo, bandas negras. La opción se guarda en
`/home/opentyrian.conf`. Prueba: h204.
