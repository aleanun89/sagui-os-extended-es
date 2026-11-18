# Notas para quien integre la IA (v8.5.4.7, parte B)

> v8.5.4.7: la IA (`ia.c`) ya usa todo lo de abajo (asedio, muelle y barcos, transportes en islas, murallas con huecos,
> diplomacia, unidades y mejoras nuevas); lo que necesite de la interfaz va en `NOTAS-INTERFAZ.md`.

Lo que el juego ofrece ahora y la IA (funciones `ai_*`, que no se han tocado) aún no usa. Todo está en `game.h`.

Asedio
- Tipos `E_BALLISTA`, `E_CATAPULT`, `E_HEAVYCAT` (Taller de asedio `E_SIEGE`, edad del Hierro). `is_siege(t)`.
- `E_HEAVYCAT` exige la mejora `T_ENGINEERING` (`unit_tech_req(t)`, `cmd_train` lo comprueba y devuelve 0).
- `T_BALLISTICS` (+25% daño de asedio) y `T_ENGINEERING` (+1 alcance) se investigan en el Taller de asedio.
- Las catapultas dañan en área (`unit_splash`) y tardan más en disparar (`unit_cooldown`). Dañan poco a unidades, mucho a edificios.
  Sugerencia: oleadas con 2 a 4 máquinas escoltadas; el combate lo resuelve solo `ST_ATTACK`.

Murallas y torres
- Edificio `E_WALL` (1x1, 5 de piedra), bloquea el paso. Las unidades no lo atraviesan: una IA que rodee su base con murallas debe
  dejar huecos (o no ponerlas cerradas), porque `find_path` no sabe abrir brechas.
- Mejoras `T_WALL_MED` y `T_FORTIFICATION` (Centro de gobierno): las murallas resisten x2 y x3.
- Torres: `T_SENTRY`, `T_GUARD`, `T_BALLISTA_T` (se investigan en una Torre): más daño, alcance y cadencia.

Flota (mapas de islas)
- Edificio `E_DOCK` (Muelle): `can_place` exige tierra libre con agua pegada. Entrena `E_FISHBOAT`, `E_TRANSPORT` y `E_WARSHIP`
  (`is_naval(t)`). Los barcos solo navegan por agua (`tile_sail`); la búsqueda de caminos ya lo sabe (`pf_sea`).
- Pesca: `cmd_target(barco_de_pesca, pez)`; entrega comida en el Muelle más cercano. `find_next_resource` solo elige peces para barcos.
- Transporte: `cmd_target(unidad_de_tierra, transporte)` la hace ir a la orilla y embarcar (`ST_BOARD` -> `ST_ABOARD`, 5 plazas).
  `cmd_move(transporte, x, y)` con (x,y) en tierra lo acerca a la costa y desembarca al llegar. `transport_load(b)` da los pasajeros.
- El Trirreme (`E_WARSHIP`) ataca como una unidad a distancia; contra objetivos en tierra navega al agua más cercana.
- Para que la IA use barcos en mapas de agua: comprobar `W.setup.map_kind == MAP_ISLANDS` (o que no haya camino terrestre al enemigo),
  construir un Muelle, 1 barco de pesca por cada 6 peces cercanos, y transportes con 5 soldados hacia la costa enemiga.

Unidades nuevas
- `E_SLINGER` (Campo de tiro, Bronce), `E_WARELE` y `E_ELEARCHER` (Establo, Hierro). Se entrenan con `cmd_train` como el resto.

Diplomacia, victoria y mercado
- `W.pl[a].dipl[b]`: `DIPL_ENEMY/NEUTRAL/ALLY`. `cmd_diplomacy(de, a, nivel)` y `cmd_tribute(de, a, recurso, cantidad)`.
  `hostile()` ya usa la diplomacia; `same_team()` = aliados. La IA puede llamar a `cmd_diplomacy` con humanos (acepta la máquina si su puntuación es menor).
- `W.setup.victory` (`VIC_STANDARD, VIC_CONQUEST, VIC_WONDER, VIC_RELICS, VIC_SCORE`) y `W.setup.time_limit` (minutos).
  `Setup.color[p]` (0 = automático, 1..8) y `pcolor(p)` para el color del jugador.
- Estadísticas para la pantalla final: `Player.units_lost`, `bld_lost`, `gathered_k[4]`, `hist[]` (puntuación cada 15 s, `hist_n` muestras).

Civilizaciones: `Civ.siege`, `Civ.naval` y `Civ.bans` (`tech_allowed(p, tech)`); `cmd_research` ya rechaza lo prohibido.

Escenas de desarrollo (solo pruebas): `AOEN_ESCENA=asedio|mar|flota|muralla|muralla2` (con `AOEN_SCENE=game AOEN_SIZE=4 AOEN_PLAYERS=2`).

## Segunda tanda (v8.5.4.7)

- Unidades nuevas: `E_PHALANX` y `E_CENTURION` (Academia, Hierro), `E_CATAPHRACT` (Establo, Hierro) y `E_HORSEARCHER` (Establo, Bronce,
  a distancia). Se entrenan con `cmd_train`; la falange es muy blindada (armadura 5/8), la catafracta es la caballería más dura.
- Catapultas: alcance mínimo (`unit_min_range`: 2,5 y 3,0). Una catapulta con enemigos pegados retrocede sola; conviene escoltarlas
  con infantería y no pedirles que ataquen a unidades cuerpo a cuerpo.
- Mejoras: `T_ASTRONOMY` (Mercado, visión +2), `T_MYSTICISM` (Templo, requiere `T_FAITH`, curación +50%), `T_LOGISTICS` (Centro de
  gobierno, población máxima +20). `world_pop_cap` ya suma la Logística.
- `Civ.build` (velocidad de construcción: Shang 1,3, Fenicios 1,15), `Civ.tower` (daño extra de torre: Babilonios +3) y `Civ.priest`
  (velocidad de conversión: Egipcios 1,5, Persas 1,25). 0 = sin bonificación.
- Mapas de islas y costeros: cada inicio tiene una ensenada de agua a unas 15 casillas con orilla libre para el Muelle
  (`can_place(E_DOCK, x, y)`); la partida registra `costa: el jugador N tiene sitio para un muelle`.
- Gráficos: falange, centurión, elefante de guerra y arquero de elefante tienen ya sus SLP originales; honda, catafracta y arquero a
  caballo siguen con los del arquero, caballería pesada y caballería (no encontré SLP propios).
