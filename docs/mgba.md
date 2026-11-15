# mGBA: Game Boy Advance, Game Boy y Game Boy Color

*(D322, v8.4.3)*

`/apps/bin/mgba` es el emulador **mGBA 0.10.5** (MPL-2.0) con un front-end propio sobre la SDL 2 del
sistema. Corre ROM de **GBA** (`.gba`), **Game Boy** (`.gb`) y **Game Boy Color** (`.gbc`). Se registra en
Recreativa (`/etc/recreativa/sistemas/gb.conf`, `gbc.conf` y `gba.conf`: docs/recreativa.md) como emulador
predeterminado de los tres sistemas; GNUBoy (D235) queda como alternativa para GB y GBC en la imagen privada.

**No hay ROM ni BIOS con el sistema.** mGBA no necesita la BIOS de la GBA (lleva una propia, emulada: HLE) y
**nunca** se usa ni se distribuye la de Nintendo. Las ROM las pones tú.

## Meter tus ROM

| Sistema | Carpetas donde las busca Recreativa | Por `input_share/` al compilar |
|---|---|---|
| Game Boy Advance | `/jogos/gba`, `/home/juegos/gba` | `input_share/games/gba/` |
| Game Boy | `/jogos/gb`, `/home/juegos/gb` | `input_share/games/gb/` |
| Game Boy Color | `/jogos/gbc`, `/home/juegos/gbc` | `input_share/games/gbc/` |

Las ROM de `input_share/` no van a git ni a las imágenes publicadas (docs/input_share.md). También valen un
pendrive o `Archivos > Abrir con`. Desde el terminal:

```
mgba /jogos/gba/juego.gba
mgba --pantalla-completa --escala 4 /jogos/gb/juego.gb
```

No se abren `.zip` (el port no lleva zlib): hay que descomprimir la ROM.

## Controles

| Consola | Teclado | Mando (SDL_GameController, D307) |
|---|---|---|
| Cruceta | flechas | cruceta o stick izquierdo |
| A | Z | botón derecho (B de Xbox) o arriba (Y) |
| B | X | botón de abajo (A de Xbox) o izquierda (X) |
| L / R (GBA) | A / S | LB / RB |
| Start | Intro | Start |
| Select | Retroceso o Mayús derecha | Back |

La disposición del mando es por **posición** (la A de Nintendo está a la derecha, como en un mando de
Nintendo). Los mandos USB, Xbox 360 y del puerto de juegos de D307 funcionan sin configurar nada.

| Tecla | Hace |
|---|---|
| F5 / F7 | guarda / carga el estado (con Mayús, antes pasa a la ranura siguiente, 1 a 9) |
| Tab (mantenida) | velocidad sin límite |
| F11 | pantalla completa (alterna) |
| Esc | salir |

## Guardados

Todo va a **`/home/.config/mgba/`**, con el nombre de la ROM **más su extensión** (así `juego.gb` y `juego.gba`
no se pisan):

| Fichero | Contenido |
|---|---|
| `juego.gba.sav` | la partida del cartucho (SRAM, Flash o EEPROM; el tipo lo detecta mGBA) y la batería/reloj de GB y GBC |
| `juego.gba.ss0` a `.ss9` | los estados de guardado (F5 / F7) |
| `mgba.conf` | la configuración (se crea la primera vez) |

## Opciones y configuración

```
mgba [--pantalla-completa] [--escala N] [--salto auto|N] [--sin-sonido] [--gpu] [--suavizado]
     [--guardados DIR] ROM
mgba --medir [opciones] ROM [segundos]
```

`/home/.config/mgba/mgba.conf` (líneas `clave=valor`; las opciones de la línea de órdenes mandan):

| Clave | Valor | Por defecto |
|---|---|---|
| `salto` | `auto` o N: saltar el dibujo de N de cada N+1 cuadros | `auto` |
| `escala` | 1 a 8: la ventana mide N veces la pantalla del cartucho (240x160 en GBA, 160x144 en GB/GBC) | 3 |
| `sonido` | 1 con sonido, 0 sin él | 1 |
| `suavizado` | 1 = escalado bilineal (más lento), 0 = el píxel más cercano | 0 |
| `gpu` | 1 = renderizador de la SDL 2 por la GPU (D288), solo si la GPU lo da | 0 |

El **salto de cuadros** es automático: el núcleo siempre emula todos los cuadros (el sonido y la lógica no se
tocan), pero si un cuadro cuesta más que su tiempo (16,7 ms) el dibujo se salta en parte de ellos (hasta 3 de
cada 4); cuando sobra tiempo vuelve a dibujar todos. En un PC rápido nunca se activa. Con `salto=0` se dibujan
todos siempre.

### Lo que se ha dejado ligero

El núcleo software de mGBA (el único: no hay OpenGL), sin rebobinado, sin trucos, sin depurador, sin hilos
(un solo hilo, como el resto del sistema), sin la BIOS real (la HLE no ejecuta la animación de arranque),
sonido a 44100 Hz con la mezcla del propio núcleo, el escalado por la SDL 2 del sistema con el vecino más
cercano, y el estado de guardado sin comprimir ni captura de pantalla. GB y GBC sin el marco de la Super Game
Boy.

## Medir la velocidad: `mgba --medir`

```
mgba --medir ROM [segundos]       (5 segundos por fase por omisión)
```

Corre la ROM sin vídeo, con la ventana a escala 1x y escalada (`--escala`, 3 por defecto), **sin límite de
velocidad**, con salto 1 y 2, y a **60 Hz** (la velocidad real de la consola). Para cada fase escribe los FPS medios
y mínimos (en ventanas de 1 s), cuántas veces la velocidad de la consola (59,73 FPS en GBA) son, el
**% de CPU** que gasta la consola a su velocidad (100 entre las veces en las fases «sin límite»; en las «a 60 Hz», el tiempo
ocupado, medido con el TSC) y los millones de ciclos del contador TSC por cuadro (`Mticks/cuadro`). Sale por el terminal y
por el registro con el prefijo `[MGBA-MEDIDA]`.

Cómo leerlo en tu PC: si «escalado, sin límite» da **más de 60 FPS** (más de 1 vez la consola) va a velocidad
completa; el margen es lo que sobra. Si en «escalado, sin límite» da menos de 60 pero «salto 2» da más, ponlo con
`salto=auto` (ya es el valor por omisión) o `salto=2`. Para tu PC antiguo, mide **con una ROM tuya** (una
cargada, un juego 3D o con muchos fondos y sprites): las ROM de prueba de abajo son el mínimo y el máximo
sintéticos. Ejemplo: `mgba --medir /jogos/gba/juego.gba 5`.

## Rendimiento medido

Medido en QEMU con KVM (el invitado corre en la CPU del anfitrión, un Ryzen 9 9900X: Zen 5, ~5 GHz), con
`mgba --medir` y las dos ROM de prueba propias (`tools/mgba_rom_teste.py`):

| ROM de prueba | Escenario | FPS sin límite | veces la consola | CPU a 60 Hz | Mticks/cuadro |
|---|---|---|---|---|---|
| GBA ligera (modo 3, bucle de espera) | núcleo solo | 6190 | 104x | 1,0 % | 0,71 |
| GBA ligera | escalado 3x | 5240 | 88x | 1,1 % | 0,82 |
| GBA cargada (4 fondos, 128 sprites 64x64) | núcleo solo | 2440 | 41x | 2,4 % | 1,80 |
| GBA cargada | escalado 3x | 2260 | 38x | 2,6 % | 1,94 |
| GBA cargada | escalado 3x, salto 1 / salto 2 | 3340 / 3940 | 56x / 66x | 1,8 % / 1,5 % | 1,31 / 1,10 |
| Game Boy | escalado 3x | 5730 | 96x | 1,0 % | 0,76 |
| Game Boy Color | escalado 3x | 5660 | 95x | 1,1 % | 0,77 |

A 60 Hz (límite de la consola) las cuatro ROM dan 59,7 FPS con mínimos de 60,0, y la prueba h322 lo exige.
El escalado y la presentación casi no cuestan (menos del 10 %) porque la SDL 2 del sistema hace un solo
pase; el **salto de cuadros** baja el coste de la ROM cargada un 30-40 % (es la parte del dibujo).

### Qué CPU hace falta

Un cuadro de GBA cuesta, en el **peor caso medido** (la ROM cargada), unos **2,3 millones de ciclos** de un núcleo
Zen 5, y 60 cuadros por segundo son **~140 MHz de ese núcleo**: sobra hasta para GBA con muchísimo margen. Las
ROM de prueba corren el procesador de la GBA a pleno (el bucle de espera ejecuta las mismas ~140 000 instrucciones por
cuadro que un juego) pero el vídeo es el de dos casos extremos, no el de un juego real: un juego real con fondos, sprites,
mezcla y ventanas queda en la zona de la ROM cargada o hasta ~2-3 veces más.

**Estimación** (no medida en hardware real; para eso está `mgba --medir`): un núcleo de la época del Core 2 / Core Duo
rinde unas 2,5-3 veces menos por ciclo que un Zen 5 en un intérprete así. Con eso, **una GBA a velocidad completa pide
del orden de 400-1200 MHz equivalentes de uno de esos núcleos según el juego**: un Core Duo T2400 (1,83 GHz) la
mueve a 60 FPS y con margen (con `salto=auto` si algún juego pesado se queda corto), y un Pentium III o Athlon de
~1 GHz va justo con salto de cuadros 1-2. **Game Boy y Game Boy Color** cuestan tanto como la GBA ligera o menos (un
cuadro de GB son 70 224 ciclos de la consola, cuatro veces menos que en la GBA): sobran en cualquier PC i686 (el binario no usa SSE).
Falta confirmarlo en el Amilo (T2400, Radeon X1400): `mgba --medir /jogos/gba/tu_juego.gba 5` y
mira la fila «escalado, sin límite» (más de 60 FPS = velocidad completa).

## ROM de prueba (propias, generadas)

`tools/mgba_rom_teste.py DIRECTORIO` deja cuatro ROM sin ningún código de terceros (la prueba h322 las genera al
ejecutarse; no hay ninguna en git):

- `saguitest.gba`: programa ARM (modo 3) con cuatro cuadrantes (azul, rojo, verde, blanco). A permuta los colores,
  Select cambia un modo que queda en el estado de guardado y Start escribe «SAGUI» en la SRAM. **El logo de
  Nintendo de la cabecera va a ceros a propósito** (no se copia): la acepta mGBA, que no lo comprueba, pero no una
  GBA real.
- `saguiestres.gba`: una GBA cargada (modo 0 con los cuatro fondos opacos desplazándose y 128 sprites de 64x64
  que se pisan) para medir el coste del vídeo.
- `saguitest.gb` y `saguitest.gbc`: cuadrantes de cuatro tonos o de cuatro colores, A invierte la paleta (GB), Start
  escribe la batería y un tono de 1000 Hz. Llevan los 48 bytes del logo que exige el arranque de la consola.

## Licencia

mGBA es **MPL-2.0** y se enlaza con la libsagui (GPL v3). La MPL 2.0 es compatible con la GPL v3 por su sección
3.3 («Secondary License»): el conjunto se distribuye bajo la GPL v3 y los ficheros de mGBA siguen bajo la MPL, con
su texto (`/apps/mgba/LICENSE`) y el código fuente disponible en este repositorio (`userland/terceros/mgba/jogo/`).
blip_buf (LGPL-2.1 o posterior) e inih (BSD) van con sus licencias (`LICENSE-blip_buf`, `LICENSE-inih`).
Las dependencias opcionales de mGBA no se copian. Detalle en docs/terceros.md; la prueba
`tests/h322_mgba.py licencia` lo comprueba.

## Pruebas

`tests/h322_mgba.py` (partes `unidad`, `licencia` y `os`): el núcleo en el PC con las ROM de prueba (cuadrantes,
teclas, estado de guardado, batería), las licencias, y en QEMU el sistema entero: ventana, FPS, el mando de D307,
F5/F7, Start y la SRAM, GB y GBC, y `--medir`. Capturas `docs/capturas/H322-*.png`.
