# Capibara

**Capibara** es un motor de juegos 2D sencillo, hecho para SaguiOS: un árbol de nodos, escenas en texto, señales y guiones
en Lua 5.4, todo con nombres en español. En Inicio > Juegos están dos ejemplos listos para jugar:

- **Capibara: plataformas**: un nivel de saltos con monedas, limos y murciélagos (flechas o A/D para moverse, Espacio o
  flecha arriba para saltar; salta encima de los enemigos para aplastarlos).
- **Capibara: escuadrón**: un juego de disparos con vista cenital (flechas o WASD, Espacio para disparar).

Los gráficos son de Kenney (CC0); los sonidos, sintetizados por el propio proyecto. Los ejemplos están en `/apps/capibara/`.

## Teclas del motor

**F3**: HUD de rendimiento (fps, tiempo de cuadro, lotes de dibujo, CPU, RAM y si se dibuja por la GPU).
**F5**: límite de FPS (60, 30 o ilimitado; se guarda). **F4**: dibuja las cajas de colisión. **F11** o **Alt+Intro**: pantalla completa. **Esc**: salir.

## Desde una terminal

    capibara /apps/capibara/plataformas
    capibara --pantalla-completa --gpu=si /apps/capibara/disparos
    capibara --bench 600 /apps/capibara/plataformas        mide los cuadros por segundo
    capibara --empaquetar mi_juego/ mi_juego.capi          crea un paquete; capibara mi_juego.capi lo abre

A pantalla completa el motor dibuja por la GPU (si el sistema tiene 3D) y presenta sin volver a la RAM (D432); si no hay GPU
sigue por software sin cambiar nada. Con `--gpu=no` se fuerza el software. El audio va en un hilo propio, independiente de los
cuadros por segundo.

Para hacer un juego propio con un editor visual: **Inicio > Programación > Capibara: editor** (ver `capibara-editor.md`). A mano: copia un ejemplo y edita `capibara.cfg`, las escenas `.esc` y los guiones `.lua`. La guía completa
(formato de escena, todas las funciones de Lua) está en `docs/capibara.md` del repositorio.
