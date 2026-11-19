# AoEN: estrategia en tiempo real

**AoEN** es un juego de estrategia en tiempo real al estilo de Age of Empires: construyes una aldea, recoges recursos,
avanzas de edad y derrotas a los demás jugadores (la IA). Se abre desde **Inicio > Juegos > AoEN**.

## Los datos del juego

AoEN **no trae** los gráficos del juego original: necesita una copia legítima de Age of Empires con estos archivos:
`GRAPHICS.DRS`, `INTERFAC.DRS` y `TERRAIN.DRS` (y `SOUNDS.DRS` para los sonidos). Cópialos a **`/jogos/AOE`** (dentro de
`DATA` o `GAME/DATA`, como en el CD) y AoEN los encuentra solo. También sirven `aoen-config --set RUTA`, la variable
`AOEN_DATA` o `aoen RUTA`. Sin los datos, la ventana te lo explica.

## Antes de empezar

**Un jugador** abre la configuración de la partida: para cada jugador, color (clic en el cuadrado), civilización (abajo se
lee su ventaja), equipo y dificultad; a la derecha, el tipo y el tamaño del mapa, la semilla (la misma semilla da el mismo
mapa), los recursos iniciales, la población máxima y la condición de victoria, con una **vista previa del mapa** y los inicios
numerados. Con el teclado: flechas o `Tab` para moverse, izquierda y derecha para cambiar, `Intro` para empezar. La casilla
**Espectador** deja que la máquina juegue también por ti (partida de demostración).

## Cómo se juega

- **Arriba:** recursos (pasa el ratón para ver cuántos los recogen), población, edad, reloj y velocidad, y los botones de
  puntuación, diplomacia, ayuda y menú.
- **Abajo:** la ficha de lo elegido (vida, ataque, armaduras, alcance, velocidad; en verde lo que suman tus mejoras), las
  órdenes y el minimapa. Cada botón de orden explica al pasar el ratón qué hace, cuánto cuesta y, si está gris, por qué no
  se puede («Falta oro: 30», «Necesitas la Edad del Bronce»). En las esquinas del minimapa: aldeano y soldado inactivos (con
  cuántos hay) y los filtros de recursos y de solo ejércitos.
- **Ratón:** clic elige; arrastrar dibuja una caja; `Mayús`+clic añade o quita; doble clic elige todos los de ese tipo a la
  vista. Clic derecho ordena (mover, recolectar, atacar, construir, embarcar en un transporte); con `Mayús` se encadenan
  hasta cuatro destinos. Con un edificio elegido, el clic derecho pone su **punto de reunión** (una bandera). La rueda cambia
  el zoom; el minimapa mueve la vista (clic) o manda a la selección (clic derecho).
- **Órdenes con teclas:** `1`..`9`, `0` según la posición en la rejilla; `Z` `X` `C` `V` posturas (agresiva, defensiva,
  quieta, pasiva); `G` proteger; `P` patrullar; `Q` detener; `E` desembarcar; `R` embarcar; `Y` punto de reunión; `Supr`
  eliminar; `AvPág` otra página de órdenes. `Mayús`+clic en entrenar pone cinco.
- **Otras teclas:** flechas o WASD mueven la vista; `H` centro urbano; `.` y `,` aldeano y soldado inactivos; `Espacio` va al
  último aviso (un ataque); `Tab` marcador pequeño, ampliado o nada; `F1` ayuda de teclas; `F3` diplomacia; `+` y `-`
  velocidad; `Ctrl`+número guarda un grupo y `Mayús`+número lo recupera; `F5` guarda, `F9` carga, `Esc` cancela o pausa, `F11`
  pantalla completa.
- **Avisos:** arriba a la izquierda salen los ataques (con una marca que parpadea en el minimapa), las edades, las mejoras y
  obras terminadas, la población llena y los cambios de diplomacia. Si alguien termina una Maravilla o reúne las reliquias, la
  cuenta atrás se ve arriba.
- **Diplomacia (`F3`):** cambia la relación con cada jugador (la máquina acepta la paz si eres más fuerte) y envía tributo:
  hace falta un mercado, y el mercado se queda el 20 %.
- **Al terminar:** estadísticas de cada jugador (militar, economía, tecnología) y la gráfica de la puntuación en el tiempo.

## Novedades de la 8.5.4.7

- **Taller de asedio:** balista, catapulta y catapulta pesada (esta necesita la mejora Ingeniería). La catapulta daña en área.
- **Muelle** (en la orilla): barco de pesca, barco de transporte (clic derecho con soldados sobre el barco para subir; envía el
  barco a tierra y desembarcan) y trirreme.
- **Murallas** y mejoras de torres; mejoras nuevas (Arquitectura, Rueda, Monoteísmo, Fe); honda y elefantes de guerra.
- Diplomacia (enemigo, neutral, aliado), tributo y condiciones de victoria: conquista, Maravilla, reliquias o tiempo.

## Opciones de la partida (8.5.4.7)

- **Riqueza:** escasos, normales o abundantes. Todos los jugadores tienen el mismo oro y la misma piedra cerca: en la base y un
  parche de cada a media distancia.
- **Victoria compartida por equipos y aliados** (activada): si al final solo quedáis aliados, ganáis todos juntos. Desactivada,
  solo gana un equipo y las alianzas se rompen.
- **Comercio mejorado (barcos y carros)** (desactivada): el **barco mercante** (Muelle) y el **carro mercante** (Mercado) traen
  oro yendo y viniendo a un muelle o mercado de otro jugador (clic derecho en él); cuanto más lejos, más oro. Con la opción
  también pueden **intercambiar**: llevan 15 de un recurso tuyo y traen otro (más si está lejos y si lo que das escasea en la
  partida). Si no tienes 15 de lo que hay que llevar, el barco espera en su puerto.
  Con un mercante elegido: **O** modo oro, **I** modo intercambio, **N** qué das y **M** qué recibes (cada pulsación pasa al
  siguiente recurso). Si «Modo intercambio» sale gris, es que la partida se creó sin «Comercio mejorado».

## Qué hace cada edificio (8.5.4.10r2)

- **Centro urbano:** aldeanos, avanzar de edad; recoge todos los recursos.
- **Casa:** población +5. **Granja:** 250 de comida; agotada, se reconstruye.
- **Granero:** recoge comida (bayas, granjas y caza); agricultura, mejoras de torres y de murallas.
- **Almacén:** recoge madera, oro, piedra y carne de caza; mejoras de recolección, armas y armaduras.
- **Cuartel:** clubman, hachero, espadachín, hondero. **Campo de tiro:** arqueros, carro arquero, arquero a caballo y de
  elefante; mejoras de flechas y arcos. **Establo:** explorador, caballería, catafracta, elefante de guerra.
- **Academia:** hoplita, falange, centurión. **Taller de asedio:** balista y catapultas.
- **Muelle:** barcos de pesca, mercantes, de transporte y trirremes; recoge el pescado. Los barcos solo navegan por el agua y
  las tropas solo andan por tierra: para cruzar el mar, súbelas a un transporte.
- **Mercado:** compra y venta, tributo, carro mercante, Rueda y Astronomía. **Templo:** sacerdotes, mejoras religiosas,
  reliquias. **Centro de gobierno:** Arquitectura, Ingeniería, Balística y Logística.
- **Torre**, **Muralla** y **Maravilla** (si resiste 10 minutos terminada, ganas).

## La máquina

Los jugadores de la máquina tienen cuatro niveles: **Fácil** (pocos aldeanos, avanza despacio y ataca tarde),
**Normal**, **Difícil** (más economía, torres, murallas, mercado y ataques organizados) y **Muy difícil**, la única con
ventaja: ve un 20 % más lejos y recoge más rápido. Las tres primeras no hacen trampas: solo saben lo que han visto sus
unidades, exploran el mapa, se reúnen antes de atacar, se retiran si pierden y construyen barcos en los mapas de islas.

## Opciones

En **Opciones > Gráficos** eliges el perfil de efectos: **Equipos lentos** (todo apagado, el más rápido), **Normal** (por
defecto en equipos con SSE2) o **Completo**, y cada efecto por separado: transiciones del terreno, agua animada, niebla
suave, sombras, humo, fuego y polvo, obras y ruinas, día y noche y zoom suave, y la pantalla completa (F11). Con una pantalla
grande y un equipo lento, la **Resolución de dibujo** al 75 % o al 50 % dibuja menos píxeles en pantalla completa y el
sistema los amplía. En un equipo antiguo (Pentium III o Athlon) a 800x600, usa **Equipos lentos**.

En **Opciones > Interfaz**: el tamaño de la interfaz (Automático: x2 desde 1600x960 y x3 desde 2880x1620), el **cursor del
juego** (cambia según lo que haría el clic derecho: espada para atacar, hacha, cesta o pico para recoger, martillo para
construir, círculo tachado si un edificio no cabe) y el rótulo con el nombre de lo que hay bajo el ratón. **Opciones >
Teclas** tiene la lista de teclas (también con F1 en la partida).

**Varios núcleos** (8.5.5.3): AoEN reparte el dibujo y la máquina entre todos los núcleos que tenga el sistema (en SaguiOS,
los que arranca con varios procesadores). La partida y la imagen son las mismas que con uno. Para usar solo uno, pon
`hilos=1` en `~/.config/aoen/aoen.cfg` (`aoen-config --hilos 1` lo hace por ti; o arranca con `AOEN_HILOS=1 aoen`).

Texturas HD y música (opcionales, tuyas): carpetas `/apps/aoen/texturas` y `/apps/aoen/musica`. La configuración y las
partidas están en `~/.config/aoen/`.

Más detalles: `docs/aoen.md` del repositorio.
