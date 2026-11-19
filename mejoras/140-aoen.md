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

## Fase v8.5.5.3: interfaz y presentación (rama `v8.5.5.3-aoen-interfaz`, D394)

Hecho: cursor propio según la orden (atacar, talar, recoger, oro, piedra, construir, no cabe, destino, desplazar), aro y
rótulo de lo que hay bajo el ratón, rótulo «AoEN» del menú principal con entrada animada, fundido entre pantallas, aviso
«Preparando la partida», opciones en cuatro pestañas (Gráficos, Interfaz, Sonido, Teclas) con textos legibles, interfaz x3,
fuego en los edificios muy dañados y resolución de dibujo en pantalla completa (75 % o 50 %, amplía el sistema). Prueba:
`h389_aoen.py presentacion` (capturas H389-33 a H389-38).

Propuesto para después (por impacto):
1. **Dibujo por GPU de verdad**: sprites como texturas (atlas por SLP y color de jugador) con `SDL_RenderCopy`, y
   presentación desde la GPU sin leer el cuadro en el sistema de ventanas (hoy D288 lo lee: con un cuadro hecho por la CPU, el
   renderizador no gana nada).
2. Arrastrar con el botón derecho para formar en línea y orientar la formación; vista previa de la formación.
3. Minimapa: señales de ataque con ondas que se expanden, clic con Alt para avisar a los aliados, zoom del minimapa.
4. Teclas configurables (la pestaña Teclas hoy solo enseña la tabla).
5. Animación de selección (aro que se cierra al elegir) y marcas de orden con flechas de dirección; barras de vida
   «siempre / al elegir / dañados» como opción.
6. Cursor con sombra suave y a 2x también a 1x en pantallas de alta densidad; cursor de «reparar» distinto del de construir.
7. Proyectiles con estela, chispas al golpear y ondas en el agua al pasar los barcos (efectos baratos, con su casilla).
8. Pantalla de carga con barra de progreso real (generación del mapa por fases) en mapas Épico/Titánico.

Fases siguientes (no en esta versión): **campañas**, **editor de mapas y escenarios** y **multijugador**.

## Fase v8.5.5.3: varios núcleos (rama `v8.5.5.3-aoen-multinucleo`, D393)

- [x] La SDL 2 del sistema con hilos de verdad (`SDL_CreateThread`, cerrojos recursivos, condiciones, semáforos, atómicas,
      `SDL_GetCPUCount` con las CPU arrancadas); el audio sigue moviéndose en el hilo principal
- [x] `hilos.c`: un hilo por núcleo (hasta 8); `AOEN_HILOS=N` o `hilos=N` en `aoen.cfg` (1: sin hilos, el camino de siempre)
- [x] Terreno HD y clásico por franjas; entidades, sombras, partículas y luz del día con una lista de dibujo reproducida por
      franjas: el cuadro sale igual bit a bit con 1 y con N hilos
- [x] IA de los jugadores de la máquina en paralelo; obras y diplomacia aplicadas después en el hilo principal en orden de
      jugador; caminos de las órdenes de la IA buscados por lotes en los hilos
- [x] Partida determinista: la misma suma del mundo con 1, 2, 4 y 8 hilos y con las IA pensando en orden inverso
      (`AOEN_SIM`, `AOEN_IA_ORDEN=inverso`, `world_suma`)
- [x] Medidas con `AOEN_PERFIL=1` y el banco de `tests/h398_aoen_multinucleo.py` (KVM, 1, 2 y 4 CPU, `--smp`)
- [ ] Pendiente: buscar por lotes también los caminos que piden las unidades en `world_update` (cambiaría cuándo empiezan a
      andar), las torres con la rejilla y la copia a la ventana (`present_frame`) en varios hilos

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
