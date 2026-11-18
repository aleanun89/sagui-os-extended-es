# 140 - AoEN: un RTS estilo Age of Empires, portado y mejorado

- **Estado:** hecha en la rama `v8.5.4.6-aoen` (D382); falta fusionar y medir en hardware real
- **Prioridad:** media
- **Alta:** 2026-11-18

## Problema / motivación

El mantenedor escribió desde cero un motor de estrategia en tiempo real al estilo de Age of Empires (C99 + SDL2, dibujo por
software, pensado para equipos antiguos). SaguiOS tiene SDL2 y varios juegos portados, pero no uno de estrategia en tiempo
real propio. Además el juego dibujaba con rutas lentas (división por píxel, rellenos en C) y con una interfaz de tamaño fijo.

## Propuesta

- Portar `aoen` y `aoen-config` a `/apps/bin` con la SDL2 del sistema (`userland/juegos/aoen/`, `aoen.mk`), entrada en
  Inicio > Juegos con icono propio, ayuda `/ayuda/aoen.md`. Datos del juego original en `/jogos/AOE` (nunca en git ni en
  imágenes públicas); texturas HD y música, si el mantenedor las tiene en `input_share/aoen`, solo en imágenes privadas.
- Rendimiento: rutas rápidas (SSE2 por CPUID con alternativa en C), sprites sin divisiones, minimapa en caché.
- Mejoras gráficas graduables en Opciones: transiciones y espuma, agua animada, niebla difuminada, sombras, partículas
  (humo, polvo, fuego), construcción y derrumbe, día y noche, zoom suave, interfaz moderna escalable (800x600 a 1920x1080).
- Prueba `tests/h389_aoen.py` con DRS sintéticos (`tools/aoen_drs_sintetico.py`).

## Criterios de aceptación

- [x] Compila y arranca en SaguiOS; sin datos muestra las instrucciones en su ventana
- [x] Con DRS sintéticos: escaramuza, selección, orden de mover, guardar y cargar
- [x] Mejoras gráficas desactivables una a una y por perfiles (Equipos lentos / Normal / Completo)
- [x] Interfaz x2 a 1920x1080 y búfer a la resolución del escritorio en pantalla completa
- [x] Test automático en `tests/h389_aoen.py` (partes generador, licencia, sin-datos, con-datos, efectos; bench aparte)
- [x] Decisión registrada en `docs/decisoes.md` (D382) y fila de stb_image en `docs/terceros.md`
- [ ] Medida en un Pentium III / Athlon reales a 800x600 (solo hay QEMU/KVM)
- [ ] Traducir al español los comentarios del código original (D096)

## Notas

Los datos del Age of Empires, las texturas y la música MIDI no son redistribuibles y no van a git. Los números de fps y la
guía de uso están en `docs/aoen.md`.
