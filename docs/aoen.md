# AoEN en SaguiOS (D382, mejora 140)

AoEN es un juego de estrategia en tiempo real al estilo de Age of Empires, escrito desde cero por el mantenedor
(C99 + SDL2, dibujo por software). Código en `userland/juegos/aoen/` (GPL-3.0-or-later). Se abre desde
**Inicio > Juegos > AoEN**; ayuda para el jugador en `/ayuda/aoen.md`.

## Datos y licencias

- **No se distribuye** ningún dato del Age of Empires original (`GRAPHICS.DRS`, `INTERFAC.DRS`, `TERRAIN.DRS`, `SOUNDS.DRS`),
  ni las texturas HD de terreno ni la música MIDI de `input_share/aoen`: son de terceros. No van a git ni a las imágenes
  públicas (`INPUT_SHARE=`).
- Orden de búsqueda de los datos: argumento, `AOEN_DATA`, `aoen.cfg` (`aoen-config`), **`/jogos/AOE`** (con `DATA` o
  `GAME/DATA`) y `/jogos/AOE/DATA`. `input_share/games/AOE` los deja en `/jogos/AOE` en las imágenes privadas.
  Texturas y música: `/apps/aoen/texturas` y `/apps/aoen/musica` (las copia `aoen.mk` si existen en
  `$(INPUT_SHARE)/aoen`; ver `docs/input_share.md`). Configuración y partidas: `~/.config/aoen/`.
- `stb_image` (dominio público o MIT): `userland/terceros/stbimage/`, fila en `docs/terceros.md`.

## Compilar

`make` lo incluye (`userland/juegos/aoen/aoen.mk`): `/apps/bin/aoen` y `/apps/bin/aoen-config`. Sin los DRS el juego
muestra las instrucciones en su ventana.

## Efectos gráficos (Opciones > Gráficos)

Tres perfiles: **Equipos lentos** (todo apagado: el AoEN original), **Normal** (transiciones, agua, niebla suave, sombras,
partículas, obras y ruinas; por defecto con SSE2) y **Completo** (más día y noche y zoom suave). Cada efecto se activa por
separado (perfil «Personalizado»). Para depurar: `AOEN_EFECTOS=0|1|2`, `AOEN_FX=<máscara de bits>` (1 transiciones, 2 agua,
4 niebla, 8 sombras, 16 partículas, 32 obras, 64 día y noche, 128 zoom suave). Interfaz: escala entera (x2 desde 1600x960).
Presentación: superficie de la ventana o renderizador 2D de la SDL (`render2d`, `AOEN_RENDER=1`).

## Variables de prueba

`AOEN_SCENE=game|fx|options`, `AOEN_SEED`, `AOEN_SELECT=1`, `AOEN_FULL=1` (pantalla completa a la resolución del escritorio),
`AOEN_BENCH=N` (N cuadros sin límite de 60 por segundo, con la cámara en vaivén, y sale). Cada suceso sale por el registro
(serie) como `[AOEN] ...`.

## Rendimiento (fps, escaramuza de 4 jugadores, cámara en vaivén)

Antes (sin rutas rápidas, división por píxel, ARGB, minimapa por cuadro; DRS sintéticos):

| Máquina | 800x600 | 1024x768 |
|---|---|---|
| QEMU TCG (sin KVM) | 26 | 20 |
| QEMU KVM | 226 | 116 |

Después (sin efectos, misma escena y medida con cámara en vaivén; los números de antes eran con cámara quieta):

| Máquina / efectos | 800x600 | 1024x768 |
|---|---|---|
| TCG, efectos 0 / 1 / 2 | 149 / 100 / 68 | 114 / 56 / 88 |
| KVM, efectos 0 / 1 / 2 | 1622 / 1225 / 923 | 1304 / 732 / 909 |

**Con los datos reales** (medida privada, KVM, cámara en vaivén, DRS originales sin texturas HD): 800x600, efectos 0 / 1 / 2:
1667 / 635 / 645 fps; 1024x768: 1212 / 488 / 494 fps. **Con las texturas HD opcionales** (terreno calculado píxel a píxel con
coma flotante) bajaba a unos 13 fps (800x600) y 10 fps (1024x768), con cualquier perfil. **v8.5.4.7:** el terreno HD pasa a
punto fijo (ver «v8.5.4.7: interfaz») y con KVM sube de 14,8 a 235 fps a 1024x768 (de 0,7 a 20 sin KVM) (perfil Normal, texturas HD, DRS sintéticos;
`AOEN_HD_FLOAT=1` vuelve a la versión de coma flotante para comparar). El perfil *Equipos lentos* sigue sin texturas HD salvo que
se pidan en Opciones.

Con cámara quieta y efectos 0 (comparable con «antes»): TCG 57 y 53 fps, KVM 811 y 667 fps. Las medidas con TCG son ruidosas
(otros procesos en el equipo). Pendiente: Pentium III y
Athlon reales.

## v8.5.4.7: funcionalidades

- **Asedio** (Taller de asedio, Hierro): balista (daño alto a unidades), catapulta y catapulta pesada (daño de área; la pesada
  pide la mejora Ingeniería). Mejoras: Ingeniería (alcance), Balística (+25% de daño).
- **Flota** (Muelle, en la orilla, Edad de Herramientas): barco de pesca (clic derecho en un pez; entrega en el muelle), barco de
  transporte (clic derecho de una unidad sobre el barco para embarcar; mandar el barco a tierra desembarca) y trirreme.
- **Murallas** (piedra) con Muralla media y Fortificación (Centro de gobierno); **torres** con Centinela, Guardia y Ballesta.
- **Mejoras**: Arquitectura, Rueda, Monoteísmo, Fe. **Civilizaciones**: bonificación de asedio o de barcos y mejoras prohibidas.
- **Victoria** (`Setup.victory`): conquista, Maravilla, reliquias (60 s) o tiempo (puntuación). **Diplomacia**: enemigo,
  neutral, aliado y tributo (hace falta un mercado).
- **Órdenes**: Mayús + clic derecho encola puntos de ruta (hasta 4); patrulla.
- Partidas guardadas: formato 4; las anteriores se rechazan como incompatibles.

Estas funciones del juego están en `game.c`; la interfaz que las muestra (menús, tooltips, pantalla final) la lleva otra rama.
`userland/juegos/aoen/NOTAS-IA.md` explica lo que la IA aún no usa.

## v8.5.4.7: interfaz

La interfaz se rehace entera; el bucle queda en `main.c` y lo demás en cinco ficheros: `ui_comun.c` (controles, ayudas
emergentes, iconos, texturas de los paneles, foco del teclado, comprobación de solapes), `ui_menu.c` (menú principal,
configuración de la partida, opciones, partidas guardadas), `ui_hud.c` (barras de la partida, ficha de la selección, rejilla de
órdenes, minimapa, avisos y órdenes con el ratón y el teclado), `ui_paneles.c` (marcador, diplomacia, ayuda de teclas, pausa,
pantalla final) y `ui_pruebas.c` (guiones de prueba). El estado compartido está en `interfaz.h`.

- **Texto:** la fuente de 8x16 de SaguiOS (minúsculas y acentos, con sombra), tomada de la libsagui (`AOEN_FONTE_SAGUI`);
  fuera de SaguiOS sigue la de 5x7.
- **Aspecto:** con los datos originales, las barras y los diálogos usan las texturas de INTERFAC.DRS del estilo de la
  civilización del jugador (piedra egipcia, mármol griego, ladrillo babilonio, madera asiática; el relieve de la barra inferior
  se repite con su periodo, que se busca comparando columnas) y los iconos originales de recursos, órdenes y mejoras; la
  bandera del punto de reunión es la animada del juego. Sin ellos (DRS sintéticos) hay iconos y paneles dibujados.
- **Disposición** (coordenadas lógicas, x2 a partir de 1600x960): barra superior de 32 (recursos con quién los recoge,
  población, edad con su progreso, reloj y velocidad, puntuación, diplomacia, ayuda, menú) y barra inferior de 150 si caben
  tres filas de órdenes (alto lógico desde 600) o 118 (la de 1920x1080 con x2 es de 960x540): ficha, rejilla y minimapa; los
  botones de aldeano y soldado inactivos y los filtros del minimapa van en las esquinas que deja libres el rombo. Medido sin
  solapes a 640x480, 800x600, 1024x768, 1280x720 y 1920x1080 (`AOEN_SOLAPES=1` cuenta los bloques que se pisan).
- **Ficha:** retrato con el color del jugador, vida, ataque, armaduras cuerpo a cuerpo y a distancia, alcance (con el mínimo),
  velocidad, el aumento de las mejoras en verde (la ayuda dice cuáles), carga y estado; edificios con la cola (clic: cancelar),
  la mejora y la edad en curso; varios tipos agrupados con su número y vida media (clic: solo ese tipo; Mayús: quitarlo).
- **Órdenes:** rejilla de 3x(5..9) o 2x(4..10) con páginas; cada botón tiene ayuda (nombre, coste con iconos y tiempo,
  requisitos, tecla y descripción) y, si está gris, el motivo («Necesitas la Edad del Bronce», «Falta oro: 30», «Necesitas más
  casas», «La cola está llena (5)»). Teclas 1..9, 0 por posición y letras fijas: Z X C V posturas, G proteger, P patrullar,
  Q detener, E desembarcar, R embarcar, Y punto de reunión, Supr eliminar, AvPág/RePág páginas.
- **Mapa:** Mayús+clic derecho encola puntos de ruta (se dibujan con puntos), la patrulla se ve entre sus dos extremos, cada
  orden deja una marca breve, doble clic elige todos los del tipo a la vista y Mayús+clic añade o quita.
- **Avisos** (arriba a la izquierda, se borran a los 12 s): ataque (parpadea, clic o Espacio para ir), edades propias y
  ajenas, mejoras terminadas, obras terminadas, población llena, Maravillas, cambios de diplomacia y derrotas. Cuentas atrás
  de Maravilla, reliquias y tiempo límite, visibles para todos.
- **Diplomacia (F3):** relación con cada jugador (la máquina decide con `cmd_diplomacy`), tributo de 50 a 500 por recurso con el
  20 % del mercado indicado y el motivo si no se puede; de los aliados de la máquina, `ia_estado` en la ayuda.
- **Marcador (Tab):** compacto, ampliado (edad, población, militar, economía, tecnología, total) o nada.
- **Pantalla final:** pestañas Resumen, Militar, Economía, Tecnología y Gráfica (la puntuación cada 15 s, `hist[]`).
- **Configuración de la partida:** por jugador color, civilización (con su descripción y estilo), equipo y dificultad (con la
  descripción de cada una, «Muy difícil» incluida); mapa (tipo, tamaño, semilla), recursos (cuatro preajustes o por recurso),
  población máxima, victoria y tiempo límite, mapa explorado y **modo espectador** (la máquina también juega por ti). La
  **vista previa** genera el mapa de verdad con la misma semilla (unos milisegundos a 100x100) y marca los inicios. Todo con
  el teclado (flechas o Tab, Izquierda/Derecha, Intro). En pantallas de menos de 760 de ancho lógico, dos pestañas.
- **Ayuda (F1):** las teclas, en la partida; la ayuda completa sigue en `/ayuda/aoen.md`.

**Terreno HD en punto fijo:** la posición en el mundo va en 16.16 y avanza sumando un paso por píxel; el ruido de las manchas se
precalcula en las esquinas de las casillas (una vez por mapa) y se interpola; las ondas del agua salen de una tabla de senos
con la fase en 0.32; niebla y mezclas con pesos de 0 a 256. Comparado con la versión de coma flotante en el PC: PSNR 58,7 dB
(prácticamente igual). Además ni el terreno HD ni el de losetas se dibujan bajo las barras.

| Terreno HD, perfil Normal (DRS sintéticos, cámara en vaivén) | 800x600 | 1024x768 |
|---|---|---|
| SaguiOS KVM: coma flotante -> punto fijo | 27,3 -> 387 fps | 14,8 -> 235 fps |
| SaguiOS TCG (sin KVM) | | 0,7 -> 20,1 fps |
| PC (x86-64, SSE, datos reales) | | 181 -> 304 fps |

La interfaz nueva cuesta poco: en el PC, 1024x768 sin efectos, de 1801 a 1422 fps (texturas de las barras ya dibujadas y
copiadas por filas).

**Variables de prueba de la interfaz:** `AOEN_PRUEBA=teclado|ayuda|reunion|cola|patrulla|alerta|diplomacia|seleccion`
(guiones de `ui_pruebas.c`, que simulan ratón y teclado), `AOEN_SCENE=setup|final` (`AOEN_PEST=0..4` elige la pestaña),
`AOEN_SOLAPES=1`, `AOEN_HD_FLOAT=1`. Prueba: `h389_aoen.py interfaz` (capturas H389-20 a H389-29).

**Pendiente:** que la máquina proponga paz o alianza al humano con un aviso de aceptar o rechazar (la IA no lo hace aún:
`NOTAS-INTERFAZ.md`), iconos propios para mejoras que no tienen dibujo en el original, y arrastrar el ratón para formar.

## v8.5.4.7: IA de la máquina (`ia.c`)

La IA sale de `game.c` a `userland/juegos/aoen/ia.c` (game.c solo llama a `ia_actualizar`). Cada jugador de la máquina piensa
cada 2 s (fácil), 1 s (normal) o 0,6 s (difícil y muy difícil):

- **Visión sin trampas:** una cuadrícula de celdas de 4x4 casillas con lo explorado y lo visible ahora, y la memoria de los
  edificios enemigos vistos. Solo ataca lo que ha visto; si no conoce nada, explora (primero el anillo donde empiezan los
  jugadores, después lo que hace más tiempo que no ve). La «muy difícil» ve un 20 % más lejos (no lo ve todo).
- **Economía:** reparto de aldeanos por recurso según lo que necesita (edad siguiente, piedra para torres, lo que sobra pesa
  menos), almacenes y graneros junto al recurso que se recoge más lejos, casas antes de llenarse (con más margen cuanto más
  produce), granjas cuando se acaba la comida silvestre, caza y exploración si se queda sin comida, mercado (difícil) para
  cambiar sobrantes. Muelle y barcos de pesca donde hay peces.
- **Edades:** construye lo que pide la edad siguiente y ahorra para ella, pero mantiene un ejército mínimo mientras ahorra.
- **Ejército:** la mejor unidad de cada edificio según fuerza/coste y lo que ha visto del rival (arqueros contra infantería,
  caballería contra arqueros, hoplitas y falanges contra caballería); asedio (2-4 máquinas) contra edificios; sacerdotes que
  siguen al grupo y curan; un explorador a caballo. Defiende la base con todo lo que tiene (los aldeanos heridos huyen al
  centro urbano). Ataca en grupo: se reúne entre la base y el centro del mapa, sale cuando su fuerza supera 1,3 veces la mayor
  fuerza rival que ha visto, avanza por pasos con los rezagados alcanzando al grupo, y se retira si pierde dos tercios y el
  rival es más fuerte (los heridos vuelven antes). Contraataca tras rechazar un ataque.
- **Islas:** muelle, trirremes que exploran el agua y atacan la costa, y transportes que embarcan a 5 soldados cada uno y zarpan
  juntos hacia el objetivo. **Murallas** (difícil): tramos sueltos delante de la base con huecos (la búsqueda de caminos no
  abre brechas). **Torres** (1 normal, 3 difícil) y sus mejoras.
- **Diplomacia** entre máquinas: si hay un líder claro, busca la paz y la alianza con otro rival que tampoco lo es; con
  mercado, envía tributo a un aliado escaso. A los humanos no les propone nada (la difícil y la muy difícil no aceptan alianzas).

| Dificultad | Aldeanos | Mejoras | Primer ataque | Retirada y contraunidades | Otras |
|---|---|---|---|---|---|
| Fácil | 10-18 | no | 15 min, 6 soldados, lo más barato | no | edades lentas (Herramientas no antes de 7 min) |
| Normal | 14-34 | economía (todas desde el Bronce) | 10 min, 8 | sí | 1 torre, sacerdote |
| Difícil | 18-50 | todas | 8 min, 10 | sí | 3 torres, murallas con huecos, mercado, 3 obras a la vez |
| Muy difícil | 18-55 | todas | 7 min, 10 | sí | la difícil + **visión +20 %** y **recogida +0,22/s por recolector** (declarado en el menú) |

- **Victorias alternativas:** con la victoria por Maravilla (o la estándar, la difícil cuando le sobra) construye la Maravilla
  con 6-10 aldeanos y ahorra para ella, la guarda con el ejército y sale enseguida contra una Maravilla rival (las terminadas
  las anuncia el juego a todos). Con la de reliquias lleva más sacerdotes, las busca por todo el mapa y ataca los templos rivales.
- **Islas:** cada casilla de tierra sabe a qué isla pertenece (relleno al empezar), así que nadie va a por un recurso de otra
  isla. Si en la propia se acaba la madera o el oro, un transporte lleva 5 aldeanos a otra isla conocida, donde levantan un
  almacén (colonia). Los desembarcos salen en oleada (todos los transportes a la vez) con los trirremes delante; el ejército de
  tierra se limita a 20-30 para dejar población a la flota, y el mercado llega antes si falta oro.
- **Final de las partidas «cada uno por su cuenta»:** las paces y alianzas hechas durante la partida se rompen cuando ya no
  queda ningún enemigo vivo (los equipos del menú se respetan siempre), para que la partida termine.
- Los aldeanos son como mucho la mitad de la población máxima de la partida.

**Rendimiento del mundo (v8.5.4.7):** `nearest_enemy` usa una rejilla de celdas de 8 casillas que se rehace en cada tic, y la
población máxima se calcula para todos los jugadores de una vez por tic. Con 8 jugadores de la máquina en 400x400, 10 minutos
de juego pasan de 37 s a 14 s en el PC; la partida guardada sigue igual al recargarla (`AOEN_SCENE=savetest`: IDENTICAL).

**IA contra IA** (`tests/aoen_ia.c`, en el PC, sin ventana; mapa 100x100 de tierra, 75 de población, `h389_aoen.py ia`):
difícil contra fácil 20-0 (media 18 min), normal contra fácil 10-0 (23 min), difícil contra normal 7-1 (2 sin terminar en
islas), muy difícil contra difícil 14-4 en 20 (25 min; con +0,25 ganaba 18-0 y con +0,20, 12-6), normal contra normal 5-5 (28 min). Edades: la
normal y la difícil llegan a Herramientas a los 4 min y al Bronce a los 10-13; la fácil a Herramientas a los 7,5. En islas
terminan casi todas gracias a los transportes; algunas se alargan más de 60 min. Tras las mejoras de islas (30 partidas
en mapa de islas): difícil contra fácil 10-0 (25 min), normal contra normal 5-4 con 1 sin terminar, difícil contra normal 6-1
con 3 sin terminar (40 min de media). Las que no terminan son un último aldeano escondido o una flota rival que domina el mar.
Con la victoria por Maravilla y partida rica (`AOEN_IA_RICO=1 AOEN_IA_VICTORIA=2`) todas terminan en 10-20 min, la mayoría
por Maravilla; con reliquias (`AOEN_IA_VICTORIA=3`) la mitad termina por reliquias.

## v8.5.4.7: victoria compartida, recursos del mapa y comercio

Tres opciones nuevas en la configuración de la partida (formato de partida guardada 5; las de formato 4 se rechazan como
incompatibles, porque `Setup` cambia de tamaño):

- **Victoria compartida por equipos y aliados** (activada por defecto). La conquista acaba cuando ha caído alguien y todos los
  que quedan son del mismo bando (`mismo_bando`: mismo equipo del menú o, con esta opción, aliados por la diplomacia); ganan
  todos juntos, también los del bando que ya cayeron, y el humano ve «VICTORIA (compartida)». La Maravilla y las reliquias de
  un aliado cuentan para el bando. La IA no rompe sus alianzas en este modo (solo las paces). Desactivada: solo un equipo del
  menú gana; las alianzas de la partida se rompen al final. Si el humano juega en modo espectador, su caída no acaba la partida.
- **Riqueza** (recursos del mapa: escasos, normales —por defecto—, abundantes). Además de lo de la base, cada jugador recibe los
  mismos parches de expansión de oro y piedra a unas 24 casillas (16 en mapas pequeños), en su lado del mapa y en su isla
  (escasos 1+1 de 2 casillas, normales 1+1 de 4, abundantes 2+2 de 5). Luego hay minas y bayas repartidas, (tamaño/2500 +
  2·jugadores) × 0,5/1/2, cada sitio con hasta 40 reintentos; los bosques son más o menos densos según el nivel, y en las islas
  la mayor isla sin jugadores recibe 2-4 parches de oro y piedra. En 100x100 con 4 jugadores hay 85, 157 y 349 casillas de oro y
  piedra; las distancias medias de los parches de cada jugador difieren menos de una casilla.
- **Comercio mejorado (barcos y carros)** (desactivada por defecto, como el juego original; así un jugador nuevo ve solo el modo
  clásico). El **barco mercante** (Muelle, Herramientas) y el **carro mercante** (Mercado, Bronce; gráfico provisional del carro
  arquero; el barco usa el barco de carga 648 del original) van y vienen entre un muelle o mercado propio y el de **otro jugador**
  (cualquiera: aliado, neutral o enemigo), fijado con clic derecho; la ruta se repite sola hasta que se le da otra orden.
  La distancia `d` es la recta entre los centros de los dos edificios (la misma para barcos y carros, y no cambia de un viaje a
  otro aunque el camino real sí):
  - **modo oro** (siempre): `oro = min(120, 5 + 0,6·d)` por viaje de ida y vuelta; nadie pierde nada.
  - **modo intercambio** (solo con la opción): carga 15 del recurso «dar» en su puerto (si no los hay, espera allí: «Esperando
    15 de piedra»), los cambia en el puerto ajeno sin tocar a su dueño y al volver da `floor(15 · min(3, 1 + d/60) · tasa)` del
    recurso «recibir», con `tasa = clamp(total_recibir / total_dar, 0,25, 4)` sobre lo que tienen ahora todos los jugadores
    vivos (total_dar 0 cuenta como 1). Máximo 180 por viaje; en la práctica 20-90. Si se hunde con carga, se pierde.
  - **Reglas de equilibrio (decisión del mantenedor):** el jugador del puerto de destino no pierde nada (ni en el modo oro ni en
    el intercambio); vale cualquier puerto ajeno, también el de un aliado; y el intercambio puede rendir más que el modo oro
    (hasta 180 por viaje frente a 120), a cambio de inmovilizar 15 del recurso y de depender de la escasez de la partida.
  - Cambiar el modo, «dar» o «recibir» a mitad de ruta la reinicia (sin carga y desde el puerto propio): si había cargado 15 y aún
    no había salido del puerto, se le devuelven; si ya había salido, se pierden. Si el puerto ajeno desaparece, el mercante queda
    libre («Ruta perdida»); si es el propio, busca otro propio y, si no hay, también queda libre.
  - La IA (normal y más) tiene 1-3 mercantes en modo oro hacia el puerto ajeno no enemigo más lejano (o el enemigo más cercano) y,
    con la opción, pasa uno al intercambio cuando le sobra un recurso (más de 900) y le falta otro (menos de 150).

Medidas con el arnés (10 partidas por serie): en tierra, difícil contra fácil 10-0, normal contra difícil 1-8, muy difícil
contra difícil 9-1; en islas, difícil contra fácil 10-0 (18 min), y entre rivales parejos aún 4 de cada 10 pasan de 60 min.

## v8.5.4.10r2: barcos en tierra, modo intercambio y edificios contra el original

Fallos que vio el mantenedor jugando, y lo que se cambió:

1. **Los barcos atravesaban tierra.** `walk_step` seguía la ruta casilla a casilla, pero cuando no había ruta (la búsqueda no
   encontraba camino y la casilla más cercana era la de salida), en el tramo final hacia `tx,ty` y al atacar algo en tierra (con
   un `tx,ty` viejo) iba en línea recta sin mirar el terreno; además, `unstick` apartaba a los barcos buscando casillas **de
   tierra**. Con 4 unidades por jugador y órdenes al azar, en islas había miles de casos por partida, y en las partidas de IA
   contra IA en islas la tropa también **andaba por el agua** (así era como la IA invadía otras islas). Ahora todo movimiento
   pasa por `mover_a`: una unidad no cambia de casilla a otra de distinto terreno (agua para los barcos, tierra para el resto;
   en diagonal tampoco cruza una esquina), y si el paso no vale se desliza por un eje o se para en la orilla y suelta la ruta
   (quien la pidió la vuelve a buscar o se detiene). Lo usan también los empujones entre unidades, `unstick` (que ya busca la
   casilla de la unidad) y el retroceso de las catapultas. Una unidad que ya estuviera en terreno ajeno (partida antigua) puede
   moverse para salir. Al atacar, el `tx,ty` es el del objetivo. La IA ya no cuenta barcos ni pasajeros como amenaza dentro de
   su base (el ejército de tierra no los alcanza y se quedaba «defendiendo» para siempre); ahora invade en transporte.
   Prueba: `h389 terreno` (6 partidas de 150 s con barcos y tropa recibiendo órdenes al azar cada medio segundo, en islas y
   costa: 0 unidades en terreno ajeno en 27 000 comprobaciones, y todos siguen moviéndose, embarcando y desembarcando) y las
   partidas de `h389 ia` comprueban el terreno en cada tic. Como la tropa ya no cruza el mar andando, en islas la difícil tarda
   más en acabar con la fácil (media de 40 min en vez de 20; 5 de 6 antes de una hora, como antes del arreglo).
2. **No se podía configurar el modo intercambio del mercante.** El botón de cada orden de comercio (`OR_COM_*`) buscaba su
   icono en `por_tipo[]`, que acababa en `OR_ELIMINAR`: leía fuera de la tabla. Ahora tienen icono propio (oro, o el recurso
   que se da o se recibe). Los botones se llaman «Modo oro» (O), «Modo intercambio» (I), «Dar» (N) y «Recibir» (M). Sin la
   opción «Comercio mejorado», «Modo intercambio» sale gris y su ayuda dice «Activa Comercio mejorado al crear la partida»; la
   ficha del mercante lo repite en una segunda línea (con la opción, resume las teclas y los recursos). Los barcos ya no ofrecen
   «Embarcar». En la configuración, la vista previa del mapa deja sitio a las filas de debajo, para que la casilla no se salga
   del panel con «Recursos: a medida» y «Victoria: puntuación». La opción va en la partida guardada (`Setup`). Prueba: guion
   `AOEN_PRUEBA=comercio` en `h389 interfaz` (capturas H389-31 y H389-32).
3. **Edificios.** Revisados uno a uno contra las reglas del Age of Empires original (manual y tablas de tecnologías de los
   sitios de aficionados; solo las reglas). Lo que se corrigió: las armas y la armadura de caballería se investigan en el
   **Almacén** (antes en el Cuartel y el Establo), las mejoras de torres y murallas en el **Granero** (antes en la Torre y el
   Centro de gobierno), Ingeniería y Balística en el **Centro de gobierno** (antes en el Taller de asedio); el arquero a caballo
   y el de elefante salen del **Campo de tiro** (antes del Establo) y el hondero del **Cuartel** (como en Rise of Rome); la
   **carne de caza** se entrega también en el Almacén. La IA clasifica como «economía» solo las mejoras de recolección. Prueba:
   `h389 edificios` (lo que entrena, investiga y recoge cada edificio, contra esta tabla).

| Edificio | Original (AoE 1997) | AoEN (v8.5.4.10r2) | Pendiente o diferencia deliberada |
|---|---|---|---|
| Centro urbano | Aldeanos; avanzar de edad; recoge todo | Igual; población +10 | Original: más centros urbanos solo con un Centro de gobierno (en AoEN se construyen sin él) |
| Casa | Población +4 | Población +5 | Diferencia deliberada (equilibrio de AoEN) |
| Granero | Recoge comida (bayas, granjas); investiga murallas y torres (Pequeña, Media, Fortificación; Atalaya, Centinela, Guardia, Ballesta) | Recoge comida (bayas, granjas y caza); investiga Domesticación y Arado, Torre centinela, de guardia y de ballesta, Muralla media y Fortificación | Agricultura: en el original va en el Mercado (Herramientas); aquí el Mercado es del Bronce. La muralla y la torre se construyen sin investigarlas |
| Almacén | Recoge madera, oro, piedra y carne; investiga armas (Herramientas, Metalurgia...), armaduras de infantería, arqueros y caballería y escudos | Recoge madera, oro, piedra y carne; investiga Herramientas, Hachas de bronce, Minería (recolección), Armadura de cuero y de escamas, Armas de bronce y de hierro, Armadura de caballería | Las de recolección van en el Mercado en el original. Faltan los escudos y la tercera armadura y arma |
| Granja | 250 de comida, se agota y se reconstruye; necesita Mercado | 250 de comida para un aldeano; se agota | Pendiente: exigir el Mercado (aquí es del Bronce y la granja de Herramientas) |
| Cuartel | Clubman, hachero, espadachines; mejoras de unidad | Clubman, hachero, espadachín y hondero | Faltan las mejoras de espada (corta, ancha, larga, legión) |
| Campo de tiro | Arqueros, carro arquero, arquero a caballo, arquero de elefante; arco mejorado y compuesto | Igual; Flechas mejoradas y Arcos compuestos | — |
| Establo | Explorador, carro, caballería, elefante de guerra; caballería pesada, catafracta | Explorador, caballería, caballería pesada, catafracta, elefante de guerra | Falta el carro (no arquero) |
| Academia | Hoplita; falange, centurión | Igual | — |
| Taller de asedio | Lanzapiedras (Bronce), catapulta, balista, helépolis | Balista, catapulta, catapulta pesada (Hierro) | Pendiente: taller en el Bronce; falta la helépolis |
| Muelle | Pesca, mercante, transporte, barcos de guerra; recoge pescado; mejoras de barcos; de la Edad de Piedra | Pesca, mercante, transporte y trirreme; recoge el pescado de los barcos; de Herramientas | Pendiente: mejoras de barcos, trirreme de catapultas; los aldeanos no pescan desde la orilla |
| Mercado | Mejoras económicas (madera, minería, agricultura, Rueda, Acuñación); necesario para las granjas y el tributo | Rueda y Astronomía; compra y venta de recursos; tributo (se queda el 20 %); carro mercante | Compra/venta y carro mercante son de AoEN; las de recolección están en Almacén y Granero |
| Templo | Sacerdotes; mejoras religiosas; (las reliquias no existen en el original) | Sacerdotes; Medicina, Fe, Mística, Monoteísmo; guarda reliquias (dan oro) | Faltan Politeísmo, Más allá y Fanatismo |
| Centro de gobierno | Arquitectura, Nobleza, Escritura, Aristocracia, Alquimia, Balística, Ingeniería; permite más centros urbanos | Arquitectura, Ingeniería, Balística, Logística | Faltan Nobleza, Escritura, Aristocracia y Alquimia |
| Torre | Defiende; se mejora en el Granero | Igual | — |
| Muralla | Corta el paso; se mejora en el Granero | Igual | — |
| Maravilla | Victoria si resiste | Igual (10 minutos terminada) | — |

## v8.5.5.3: interfaz y presentación (D394)

- **Cursor propio** según la orden y **aro y rótulo** de lo que hay bajo el ratón (`ui_cursor.c`, `hud_cursor` y
  `hud_rotulo_raton` en `ui_hud.c`). Opciones `cursor_propio` y `nombres_raton` en `aoen.cfg`; `AOEN_CURSOR=0` lo apaga.
- **Menú principal** con rótulo (`gfx_titulo`), **fundido** de 0,25 s entre pantallas y aviso **«Preparando la partida»**.
- **Opciones en pestañas**: Gráficos, Interfaz (tamaño Automático/1x/2x/3x, cursor, rótulo, renderizador 2D), Sonido y
  Teclas (la misma tabla que F1).
- **Fuego** en los edificios por debajo del 40 % (con «Humo, fuego y polvo»).
- **Resolución de dibujo** en pantalla completa (SaguiOS): `escala_dibujo` = 100, 75 o 50 (`AOEN_DIBUJO`); el búfer se
  hace más pequeño y la SDL del sistema lo amplía. Nunca por debajo de 640x480.
- **GPU**: el renderizador de la SDL (`render2d`) sube el cuadro hecho por la CPU y lo vuelve a leer para presentarlo
  (D288): no gana nada con AoEN y sigue apagado por defecto. Ver D394.
- Escenas nuevas: `AOEN_SCENE=cursores` (todos los cursores ampliados), `AOEN_SCENE=options AOEN_PAG=0..3`; guion
  `AOEN_PRUEBA=cursor`.

## v8.5.5.3: varios núcleos (D393)

AoEN usa un hilo por núcleo (hasta 8; con `--smp` en SaguiOS, los secundarios): el terreno (HD y clásico) y lo que va encima
(entidades, sombras, barras de vida, partículas, día y noche) se dibujan por franjas horizontales, y los jugadores de la máquina
piensan a la vez. El resultado es el mismo que con un hilo, bit a bit: la imagen y la partida (misma semilla, misma suma).

- **Forzar un hilo**: `AOEN_HILOS=1`, `hilos=1` en `aoen.cfg` o `aoen-config --hilos 1` (`hilos=0`, por defecto: tantos
  como núcleos). Con un núcleo no se crea ningún hilo.
- **Medir**: `AOEN_PERFIL=1` con `AOEN_BENCH=N` escribe `[AOEN] perfil: por cuadro (ms): simulación ... ia ... terreno ...
  entidades ... interfaz ... presentar ...`. Partida cargada: `AOEN_SCENE=game AOEN_PLAYERS=8 AOEN_SIZE=4 AOEN_DIF=2
  AOEN_ESPECTADOR=1 AOEN_REVEAL=1 AOEN_FFWD=600` (la máquina lleva también al jugador 1, así nadie pierde enseguida).
- **Determinismo**: `AOEN_SIM=segundos` simula sin dibujo y escribe `[AOEN] sim t=... suma=...` cada minuto;
  `AOEN_IA_ORDEN=inverso`; `AOEN_PASO_FIJO=1`, `AOEN_TIEMPO_ANIM=s` y `AOEN_SUMA_CUADRO=N` para comparar cuadros.
- **Texturas HD sintéticas** para probar sin las del mantenedor: `python3 tools/aoen_texturas_sintetico.py CARPETA` y
  `AOEN_TEX=CARPETA`.

Medidas (KVM, 1024x768 a pantalla completa, DRS y texturas sintéticos, partida de 8 jugadores de la máquina adelantada 10
minutos, `--smp`; `tests/h398_aoen_multinucleo.py bench`), en fps; el detalle, en D393 (`docs/decisoes.md`):

| | 1 CPU | 2 CPU | 4 CPU |
|---|---|---|---|
| Terreno HD, antes / ahora | 107 / 131 | 124 / 224 | 120 / 260 |
| Clásico con efectos, antes / ahora | 201 / 283 | 206 / 359 | 201 / 323 |

## v8.5.5.3: islas que terminan y diplomacia con la máquina (D415)

**Qué fallaba en las islas.** El generador de «islas» casi nunca deja a los jugadores en islas de verdad: de 25 mapas de
100x100, un tercio dejaba a los dos jugadores separados por agua **o por bosque** (los árboles bloquean a las unidades; el
arnés lo cuenta con `tile_walk`, línea `mapa separadas=1`). Eso rompía a la IA de cuatro maneras:
1. Con rivales en la misma masa de tierra, los soldados esperaban un transporte en la reunión para siempre (la regla «en las
   islas, esperan el transporte» valía aunque se pudiera ir andando).
2. Los transportes iban al agua más cercana al objetivo **en línea recta**, que a menudo era una ensenada cerrada junto a casa:
   desembarcaban en su propia isla y volvían andando (`desembarcan 5 unidades (jugador 0, en 72,16)` una y otra vez).
3. Un uno o dos infiltrados (un transporte rival con 1 soldado) hacían que el ejército entero «defendiera» y deshiciera la
   reunión y la invasión, indefinidamente.
4. Con la población llena de tropa de tierra no cabía el segundo transporte; y los peces del generador cerraban a veces
   el único canal de una ensenada, con toda la flota del jugador dentro.

**Qué hace ahora la IA** (`ia.c`):
- **Zonas por las que se anda** (`terr`, al empezar la partida: tierra sin edificios ni recursos, 4 vecinos) y **masas de agua**
  (`marc`). Un rival está «por mar» si su edificio conocido está en otra zona; las islas a las que no se llega por mar desde el
  agua del muelle no son objetivo (`isla_alcanzable`). Con todos los rivales en la misma zona no se hacen transportes ni flota
  de más y el ejército marcha por tierra.
- **Plan de desembarco** (`sitio_orilla`): la orilla de la isla del objetivo más cercana a él, alcanzable desde el agua del muelle
  (casilla de agua y casilla de tierra pegadas), lejos de las torres que conoce. La flota navega a esa agua y los transportes
  bajan a la tropa allí; lo desembarcado va directo al objetivo (grupo en la isla enemiga) y los refuerzos esperan en casa el
  siguiente viaje. Antes de zarpar hay **flota de guerra** para dominar el agua (rivales vistos + 1, hasta 4; la fácil no la
  exige; si en 150 s no la hay, salen igualmente). Se reservan plazas de población para los transportes que faltan.
- **Recoger varados** (`rescatar`): una unidad militar o un sacerdote ocioso en otra zona que la de la base (desembarcado que no
  sigue el ataque, dejado en un islote) lo recoge un transporte libre (un rescate a la vez, 4 minutos como máximo) y lo lleva al
  muelle; si no se llega, se aparta 10 minutos a esa unidad.
- **Infiltrados**: con 1-3 soldados rivales dentro de la base y menos del 35 % de la fuerza del ejército, solo defienden los de
  casa; la reunión y la invasión siguen.
- **Los peces no cierran canales** (`game.c`, generador): no se pone un pez en una casilla de agua con tierra a los dos lados.
  El resto del mapa y los recursos salen igual (el generador consume los mismos números aleatorios).

**Regla de victoria por conquista sin bloqueos** (`reglas_caida` en `game.c`). Un jugador **sin ningún edificio** (las murallas no
cuentan) cae: **enseguida** si solo le quedan barcos de pesca, mercantes o transportes (ni luchan ni construyen), y **a los 180
s** si le quedan unidades (un aldeano escondido, tropa en un islote): es lo que tardaría en levantar otro edificio. Al caer, el
resto de sus unidades se rinden. Con un edificio en pie el contador vuelve a cero; el humano sin edificios ve el aviso
«¡Sin edificios! Levanta uno en 3 minutos o perderás». Se aplica a todas las partidas (no solo islas) y a todos los modos de
victoria; `W.pl[p].alive` y el final los decide el mismo `check_victory`.

**Diplomacia con el humano** (F3; `ia_responde`, `ia_pide`, `ia_tributo`, `ia_traicion` y `proponer_al_humano` en `ia.c`; el chat en
`game.c`/`ui_hud.c`):
- *Personalidad* (fija por jugador y semilla: belicosa, equilibrada o pacífica) y *dificultad* (fácil +0,6, normal 0, difícil
  -0,5, muy difícil -0,8) dan una puntuación base; se suma la *fuerza relativa* de quien pide (de -1,5 a +1,5, doble de la razón
  menos uno), +1,5 si hay un *enemigo común* (un tercero vivo, enemigo de los dos y no más débil que ella), hasta +2,5 por los
  *tributos recibidos* (1 por cada 200 llegados) y se resta 0,7 por cada *traición* recordada (hasta 2) y 4 durante los 15
  minutos siguientes a una. La paz (enemigo a neutral) necesita 0,4 y la alianza 2,0 y venir de la paz; en difícil y muy
  difícil, además, un enemigo común y algún tributo.
- *Responde en el chat* (avisos arriba a la izquierda): «Eres fuerte: acepto la paz», «Tus regalos cuentan: acepto la alianza»,
  «Eres demasiado débil para pedirme la paz», «Todavía no me fío: envíame un tributo», «Me traicionaste hace poco: no me fío
  de ti», «Primero hagamos las paces»...
- *Traición*: romper una paz o una alianza con la máquina la anota; responde «¡Traidor! No olvidaré esto», no acepta nada ni da
  tributo en 15 minutos y después sigue desconfiando.
- *Tributos y peticiones*: enviar tributo a la máquina suma a su confianza («Gracias por el tributo: lo recordaré»). El botón
  **Modo: pedir** de F3 le pide un tributo: lo envía (con el mismo 20 % del mercado) si es aliada, o neutral y le has enviado tributo,
  y le sobra (300, o 600 si es belicosa, por encima de lo pedido); no lo envía a un enemigo ni a quien la traicionó.
- *La máquina propone* (normal y más, pasados 5 minutos, cada 4 minutos por jugador): la paz si es más débil (menos del 70 %),
  si hay un enemigo común o si es pacífica y no es mucho más fuerte; la alianza si hay un enemigo común (y no es belicosa) o le
  has enviado 200 en tributos; y te pide un recurso que le falta si a ti te sobra. La propuesta sale en la fila del jugador en
  F3 con «Aceptar» y «Rechazar» (caduca a los 90 s). Si la traicionaste, no propone nada y te lo recuerda cada 5 minutos.
- Entre máquinas la diplomacia sigue como antes (`cmd_diplomacy`, la más fuerte acepta).
- AoEN no tiene tablas de traducción: los textos van en castellano como el resto de la interfaz, así que no hay nada que
  traducir.

**Medidas** (arnés de IA contra IA, 100x100, 2 jugadores, 60 minutos de juego como máximo; mapas de islas con los jugadores
separados): difícil contra fácil, **antes** (sin cambios, 10 semillas separadas): 6 terminan, 5 por conquista, el resto no
termina en una hora o gana por Maravilla tras 50 minutos; con solo la IA nueva y el generador viejo, 7 de 10 (5 por
conquista); **ahora**, con la IA y el generador nuevos (15 semillas separadas): **14 de 15 terminan por conquista**, de 12 a
27 minutos de juego (media 20), y la que falta (semilla 4) deja a un rival con un almacén y dos aldeanos en una isla que
la flota no llega a ver. Normal contra normal en 6 semillas: antes 0 terminaban, ahora 3 (las otras 3 siguen parejas a
los 60 minutos). Sin unidades en terreno ajeno en ningún caso, y la misma semilla da las mismas sumas del mundo.

**Pruebas** (`tests/h415_aoen_islas_diplomacia.py`; arnés `tests/aoen_ia.c`): `islas` (semillas con jugadores separados),
`caida` (`AOEN_IA_CAIDA=1|2|3`), `diplomacia` (`AOEN_IA_DIPLO=1`, guion del humano contra dos jugadores de la máquina),
`fuentes` y, aparte, `compat` (h389 ia/reglas/terreno y h398 pc/determinismo-pc). Variables nuevas del arnés:
`AOEN_IA_MAPA=1` (dibuja el mapa), `AOEN_IA_VOLCAR=segundos` (vuelca entidades a esa hora) y la línea `mapa separadas=N`.

## Plan: gráficos propios de las unidades (solo documentado)

Decisión del mantenedor (2026-11-19): **sin trabajo previsto** en la 8.5.5.3; aquí solo el plan y el alcance.

- **Qué**: sustituir los gráficos de unidades «prestados» (los que se ven con los DRS del juego original del
  mantenedor) por gráficos propios o con licencia libre, para que AoEN se pueda jugar y publicar entero sin datos ajenos.
- **Alcance**: todas las unidades y sus animaciones (andar, atacar, morir; 8 direcciones), con la paleta y el tamaño de
  sprite que ya usa el motor; los edificios y el terreno ya tienen sustituto sintético (`tools/aoen_drs_sintetico.py`,
  `tools/aoen_texturas_sintetico.py`) y servirían de base.
- **Cómo** (cuando se haga): generados por código o dibujados, con licencia compatible con la GPL v3 (CC0/CC-BY o
  propios), anotados en `docs/terceros.md`; el motor no cambia (mismo formato de DRS).
- **Pruebas**: las de `h389` con los gráficos nuevos en vez de los sintéticos.

## Pruebas

`python3 tests/h389_aoen.py [generador licencia sin-datos con-datos efectos funciones ia interfaz presentacion reglas terreno edificios]` y, aparte, `bench`.
Varios núcleos: `python3 tests/h398_aoen_multinucleo.py [pc determinismo-pc tcc saguios]` y, aparte, `bench` (mejor con
`SAGUI_KVM=1`; `H398_CPUS="1,2,4"`).
Usan DRS sintéticos de
`tools/aoen_drs_sintetico.py`; nunca los reales.
