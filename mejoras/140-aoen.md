# 140 - AoEN: un RTS estilo Age of Empires, portado y mejorado

- **Estado:** publicada en la v8.5.4.6 (D382); falta medir en hardware real y las fases de juego siguientes
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
- [x] Traducir al español los comentarios del código original (D096)

## Fase v8.5.4.7: funcionalidades (rama `v8.5.4.7-aoen`)

Hecho: asedio (balista, catapulta, catapulta pesada; daño de área), murallas (+ Muralla media y Fortificación), mejoras de
torres, Muelle y flota (barco de pesca, transporte con embarque y desembarco, trirreme; los barcos navegan solo por agua),
honda y elefantes de guerra, mejoras (Ingeniería, Balística, Arquitectura, Rueda, Monoteísmo, Fe), bonificaciones y
prohibiciones por civilización, condiciones de victoria (conquista, Maravilla, reliquias, tiempo), diplomacia y tributo,
órdenes en cola con Mayús y patrulla (funciones del juego, la interfaz las enseña otra rama), formato de partida 4.
Prueba: `h389_aoen.py funciones` (escenas `AOEN_ESCENA`).

Pendiente de esta fase: gráficos propios de la honda y de los elefantes de guerra (hoy usan los del arquero y del elefante),
falange, centurión y catafracta, arquero a caballo, catapulta con alcance mínimo, que la IA use asedio, barcos y murallas
(`userland/juegos/aoen/NOTAS-IA.md`), mapas de islas con inicio solo costero, y las mejoras Astronomía, Mística y Logística.

## Fase v8.5.4.7: IA de la máquina (rama `v8.5.4.7-aoen-interfaz`)

Hecho: la IA sale a `ia.c` y se rehace (visión sin trampas con memoria, economía por necesidades, ataques en grupo con
retirada, defensa, contraunidades, exploración, torres, asedio, flota y transportes en islas, murallas con huecos,
diplomacia entre máquinas) con cuatro dificultades (la «muy difícil» ve un 20 % más lejos y recoge más, y lo dice el menú).
Arnés `tests/aoen_ia.c` y parte `h389_aoen.py ia`: difícil contra fácil 20-0, normal contra fácil 10-0, muy difícil contra
difícil 14-4. Detalle en `docs/aoen.md`.

Segunda tanda: Maravilla y reliquias como victoria alternativa, colonias y desembarcos en oleada en islas, fin de las partidas
«cada uno por su cuenta» y rejilla espacial en el mundo (2,7 veces más rápido con 8 jugadores en 400x400).

Pendiente: que la IA proponga diplomacia a humanos (necesita interfaz) y que en islas termine siempre antes de una hora
(hoy 3-4 de cada 30 se alargan).

## Fase v8.5.4.7: interfaz (rama `v8.5.4.7-aoen-ui`)

Hecho: menú de partida con vista previa del mapa, color, equipo, civilización (con su descripción), cuatro dificultades,
recursos, población, victoria y modo espectador, todo con teclado; barras con las texturas originales por estilo de
civilización y fuente de 8x16; ficha de la selección completa (estadísticas, mejoras aplicadas, carga, cola con cancelar, varios
tipos con filtro); rejilla de órdenes con ayudas (coste con iconos, tiempo, requisitos, tecla, descripción) y motivo de los
botones grises; posturas, patrulla, detener, embarcar y desembarcar, punto de reunión con bandera, Mayús para encadenar;
aldeano y soldado inactivos con contador; avisos (ataque con Espacio para ir, edades, mejoras, obras, población, Maravillas,
diplomacia) y cuentas atrás; minimapa con color por jugador, filtros y alertas que parpadean; diplomacia y tributo (F3);
marcador ampliado (Tab); ayuda de teclas (F1); pantalla final con estadísticas y gráfica de la puntuación. Sin solapes de
640x480 a 1920x1080. Terreno HD en punto fijo: de 14,8 a 235 fps (1024x768, KVM). Prueba: `h389_aoen.py interfaz`.

Pendiente: aceptar o rechazar propuestas de diplomacia de la máquina (cuando la IA las haga), arrastrar para formar.

Fases siguientes (no en esta versión): **campañas**, **editor de mapas y escenarios** y **multijugador**.

## Notas

Los datos del Age of Empires, las texturas y la música MIDI no son redistribuibles y no van a git. Los números de fps y la
guía de uso están en `docs/aoen.md`.

## Fase futura: AoEN sin datos originales (pedido del mantenedor, 2026-11-18; no ahora)

El motor no depende del juego original (h389 juega con DRS sintéticos). Dos pasos posibles:
1. **Paquete gráfico libre en formato DRS** (como Freedoom para Doom): arte original y libre (CC0/CC-BY) convertido a
   GRAPHICS/INTERFAC/TERRAIN.DRS con los mismos ids que pide el motor, con una herramienta `tools/aoen_paquete_libre.py`
   que lea un ZIP de PNG + `manifiesto.json` (encargo de arte ya redactado para otra IA). Podría ir en las imágenes públicas.
2. **Formato propio de recursos** (carpetas de PNG + manifiesto, sonidos WAV/OGG, música MIDI) leído directamente por
   `assets_load`, para mods y temas (otras épocas o fantasía) con las reglas de `game.c` o reglas propias.
