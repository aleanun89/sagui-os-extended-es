# Notas para la interfaz de AoEN (desde la IA, v8.5.4.7)

Lo que la IA (`ia.c`, `ia.h`) ofrece o necesita de la interfaz. La interfaz es de otro trabajo; aquí solo se anota.

- **Dificultades:** `IA_DIFICULTADES` (4) en `ia.h`: 0 Fácil, 1 Normal, 2 Difícil, 3 Muy difícil. `world_new` y `save.c` ya
  aceptan el 3 (`Setup.ai_diff[p]`), así que el menú de partida solo tiene que ofrecer los cuatro.
- **Texto del menú** para la cuarta (decisión del mantenedor): «Muy difícil: ve un 20 % más lejos y recoge más rápido».
  Para las otras, si hay espacio para una línea de ayuda: «Fácil: avanza despacio y ataca tarde», «Normal», «Difícil: sin
  trampas, economía y ataques organizados».
- **`ia_estado(p)`** devuelve lo que hace un jugador de la máquina («en paz», «reuniendo el ejército», «atacando»,
  «retirándose», «defendiendo»; lo último que decidió; su objetivo). Sirve para la depuración; si se enseña en la pantalla de
  diplomacia (Tab), solo para los aliados, para no regalar información del rival.
- **`ia_olvidar()`**: no hace falta llamarla (la IA detecta sola una partida nueva o cargada), pero es inofensiva.
- **Diplomacia con humanos:** la IA no propone nada a los humanos; las propuestas del humano las decide `cmd_diplomacy`
  (game.c). Si la interfaz quiere que la máquina «ofrezca la paz» al humano, haría falta un aviso con botones Aceptar/Rechazar:
  queda pendiente.
- **Partida IA contra IA visible** (escena de demostración o prueba en SaguiOS): basta con poner `W.pl[0].ai = 1` tras
  `world_new` y `game_speed` alto; la IA del jugador 0 funciona igual. La pantalla final debería tolerar que nadie sea humano.

## Respuesta de la interfaz (v8.5.4.7, rama `v8.5.4.7-aoen-ui`)

- El menú ofrece las cuatro dificultades con esos textos (y la descripción de cada una bajo la civilización del jugador).
- `ia_estado(p)` sale en la ayuda del nombre de un **aliado** de la máquina en la diplomacia (F3); de los rivales, nada.
- Modo espectador en la configuración: `W.pl[0].ai = 1` (difícil) tras `world_new`, mapa explorado y velocidad x2.
- Propuestas de la máquina al humano: la interfaz las mostraría como aviso con «Aceptar» y «Rechazar» (que llaman a
  `cmd_diplomacy(humano, p, nivel)`), pero hace falta un dato del juego para la propuesta pendiente, por ejemplo
  `W.propuesta_de`, `W.propuesta_nivel` y `W.propuesta_t` en `game.h` (no se guardan en la partida), que la IA rellene.
  Queda pendiente para quien lleve `game.h` y la IA; la interfaz solo tiene que leerlo.

- **Alianzas que se rompen:** en «cada uno por su cuenta», cuando ya no queda ningún enemigo vivo, la IA declara la guerra a
  quien tenga de aliado o neutral por la diplomacia de la partida (los equipos del menú se respetan). Si eso le pasa al humano,
  `cmd_diplomacy` ya pone el mensaje «Relación con el jugador N: Enemigo»; quizá merezca una alerta con sonido.
- **Victorias:** la IA juega a la Maravilla y a las reliquias según `Setup.victory`; la cuenta atrás de la Maravilla rival ya
  la enseña la barra superior.
