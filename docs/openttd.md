<!-- SPDX-License-Identifier: GPL-2.0-or-later -->
# OpenTTD como paquete aparte (D375, prueba de concepto de la v9.0)

OpenTTD es **GPL-2.0 solo** (sus cabeceras dicen «version 2», sin «or later»), así que no se puede
combinar con el resto del sistema (GPL v3) dentro de una imagen pública. Es la prueba de concepto de
la decisión D375 (`decisoes.md`): la aplicación vive en **su propio repositorio**, se compila contra una
copia del sistema y se entrega como un **paquete** que se instala en `/apps`.

**Estado (2026-11-17):** compila, enlaza y arranca en SaguiOS (menú principal y partida rápida, h377).
Hay un **bloqueo legal** de la capa de enlace (más abajo): decide el mantenedor.

## Dónde está cada cosa

| Qué | Dónde |
|---|---|
| Código de OpenTTD 1.10.3 y el porte | repositorio **local** `../openttd-saguios` (junto a la copia del sistema), rama `saguios` (sobre la etiqueta `1.10.3`, commit `baf5bf29`). Sin push ni repositorio público (lo autorizará el mantenedor aparte) |
| Solo el porte | `saguios/` de ese repositorio (compilar.sh, fuentes.txt, empaquetar.py, `include/` con las cabeceras finas que faltan, `plataforma/`) y dos parches en `src/video/sdl2_v.cpp` |
| Receta en este repositorio | `tools/openttd_paquete.sh` (clona/actualiza el otro repositorio, hace `make sdk_externo`, compila y deja el paquete en `build/paquetes/`, fuera de git) |
| Instalador de paquetes | `tools/paquete_instalar.py` (valida el manifiesto y las sumas y extrae `apps/` donde se le diga; `--input-share` lo deja en `input_share/apps`) |
| SDK para compilar fuera | `make sdk_externo` (`userland/sdk_externo.mk`) → `build/sdk/sdk.mk` y `sdk.sh` con las banderas exactas, el guion de enlace y los objetos de la libc, libcxx, libm, libsagui y SDL 2 |
| Prueba | `tests/h377_openttd_paquete.py` (partes `paquete`, `licencia`, `os`; se salta sin paquete) |

## Cómo se compila y se prueba

```sh
. ~/.local/saguitools/env.sh
SAGUI_TURNOS=6 tools/openttd_paquete.sh -j 4      # SDK + compilación cruzada + paquete en build/paquetes/
tools/openttd_paquete.sh instalar                  # paquete -> input_share/apps (lo vuelca el make en /apps)
python3 tests/h377_openttd_paquete.py              # instala en una copia de build/apps/raiz y arranca el juego
```

OpenTTD 1.10.3 se eligió porque es la versión estable que **menos** C++ exige: C++11, sin CMake (usa
`configure` + `source.list`), sin `std::string_view`/`std::optional`/`std::variant` (las 12.x y 13.x los usan).
Se compila sin zlib, lzma, lzo, png, freetype, fontconfig, ICU ni harfbuzz: texto con su fuente de
sprites, partidas sin comprimir, sin hilos (`StartNewThread` falla y OpenTTD cae al modo sin hilos), sin
música (controlador `null`; el sonido va por la SDL 2 del sistema), red por la libc BSD sockets (sin
`getifaddrs`: la difusión LAN queda limitada).

## Formato del paquete SaguiOS (formato 1) — sirve también para NetSurf, GNUBoy…

Un tar sin comprimir llamado `<nombre>-<versión>-saguios.tar` (+ `.sha256`) con:

```
sagui-paquete.json    manifiesto
LICENSE               licencia de la aplicación (aquí, GPL v2)
ORIGEN-FUENTES.txt    de dónde salen las fuentes exactas y cómo recompilar (la «oferta de fuentes»)
icono.png             icono 16x16 de la entrada de menú
apps/                 se vuelca TAL CUAL sobre /apps:  bin/<ejecutable>  y  <app>/ (datos, licencias, leeme)
```

Manifiesto (`sagui-paquete.json`, UTF-8):

| Campo | Contenido |
|---|---|
| `formato` | `1` |
| `nombre`, `titulo`, `version`, `paquete`, `descripcion` | identidad (`paquete` = `<versión>-saguios[.N]`) |
| `licencia` | SPDX de la aplicación (`GPL-2.0-only`) |
| `origen` | `url`, `etiqueta`, `commit` del upstream y `porte_commit` (commit del porte que se compiló) |
| `instalacion` | `destino` (`/apps`), `contenido` (`apps/`), `ejecutable`, `datos` |
| `menu` | `grupo` (Juegos…), `nombre`, `ejecutable`, `argumentos`, `icono` |
| `requisitos` | arquitectura, memoria, pantalla |
| `datos_externos` | cada dato redistribuible bajado: `nombre`, `version`, `licencia`, `url`, `sha256` (el empaquetador aborta si no coincide) |
| `ficheros` | cada fichero del paquete: `ruta`, `bytes`, `sha256` (el instalador comprueba todo antes de extraer nada y rechaza rutas absolutas, `..` o ficheros no listados) |

La entrada de **Inicio > Juegos > OpenTTD** está en `kernel/gui/barra.c` y solo se muestra si existe
`/apps/bin/openttd` (como las demás aplicaciones de `/apps`). Pendiente para el instalador de la v9.0:
leer el campo `menu` del manifiesto en vez de tenerla fija en la barra (solo hay un icono genérico).

## Licencias del enlace (punto 2 de D375)

El paquete enlaza, estáticamente, la libc común, el runtime de C++, la libm, la libsagui y la SDL 2 del
sistema (todo en un solo ejecutable). Resultado de `tools/spdx.py` y de las cabeceras (`SPDX-License-Identifier`;
sin marca = fichero nuevo del fork, GPL-2.0-or-later, o heredado de Eli → GPL-3.0-only según `docs/licencia.md`):

| Biblioteca | Ficheros | Licencia |
|---|---|---|
| `userland/libc/` (libc común) | 60 | GPL-2.0-or-later (nueva del fork; sin marca en el fichero) |
| `userland/libcxx/` (runtime y cabeceras de C++) | 7 | GPL-2.0-or-later |
| `userland/libsdl2/` (SDL 2 del sistema, capas image/ttf) | 38 | GPL-2.0-or-later (nueva; sin marca) |
| `userland/libsagui/` | 93 | 78 GPL-2.0-or-later, 3 nuevas sin marca, 1 GPL-3.0-or-later y **11 GPL-3.0-only** |
| `userland/libfontes/` (no se enlaza en este paquete) | 6 | `fonte.c` GPL-3.0-only, el resto 2.0-or-later |
| `userland/inc/` y `kernel/inc/` (cabeceras de la ABI de las llamadas al sistema, que se incluyen al compilar la libc) | 165 | 120 GPL-2.0-or-later, 43 **GPL-3.0-only** (todas de `kernel/inc/`: `abi.h`, `sistema.h`, `memoria.h`…), 1 GPL-3.0-or-later, 1 nueva sin marca |
| zlib | (no se usa; OpenTTD se compila sin zlib) | zlib |

**BLOQUEO LEGAL:** los 11 ficheros GPL-3.0-only de la libsagui **sí entran en el ejecutable**, porque son
el cimiento que toda la libc necesita: `inicio.asm` (punto de entrada `_inicio` → `principal`),
`chamadas.c` (las llamadas al sistema), `memoria.c`, `formatar.c`, `texto.c`, `entrada.c`, `gui.c`,
`widget.c` y, aunque no se usen, `mp4.c`, `h264.c` y `aac.c` (se enlazan todos los `.o`). Un ejecutable que
une código GPL-2.0-only (OpenTTD) y GPL-3.0-only **no se puede distribuir** (la GPL-2.0 sin «or later» y
la GPL-3.0 no son compatibles en un mismo programa), ni siquiera como paquete aparte: el problema no es dónde
vive el repositorio sino qué se enlaza. Además, las cabeceras de la ABI (`kernel/inc/abi.h` y otras,
GPL-3.0-only) se incluyen al compilar la libc y el SDK.

Las salidas posibles (las decide el mantenedor; no se resuelve cambiando licencias):

1. Reescribir en limpio (GPL-2.0-or-later o más permisiva) la **capa mínima de arranque y llamadas al
   sistema** que necesita la libc: `inicio.asm`, `chamadas.c`, `memoria.c`, `formatar.c`, `texto.c`,
   `entrada.c` y las cabeceras de la ABI, y enlazar los paquetes solo con esa capa («libsagui-núcleo»).
   La libc, libcxx y SDL 2 ya son GPL-2.0-or-later.
2. Que el titular de esos ficheros (Eli Rigobeli, proyecto original) los relicencie a «GPL-3.0-or-later»
   no ayuda (sigue sin ser compatible con 2.0-only) y a «GPL-2.0-or-later» o con una excepción de enlace
   (por ejemplo, la «GPL linking exception» o LGPL para esa capa).
3. Distribuir solo las fuentes y el `compilar.sh` (cada usuario compila para sí: uso privado, como
   NetSurf y GNUBoy en D261) y no el binario.

La primera es la más limpia y coincide con la nota de D375 («una capa de compatibilidad propia con licencia
compatible con las dos»): son pocos ficheros, pero `chamadas.c` y `inicio.asm` son la frontera del núcleo.

## Lo que tuvo que ampliarse en la libcxx (general, con prueba)

OpenTTD pidió a las cabeceras finas de C++ cosas que cualquier port usará: iteradores inversos en
`vector`/`map`/`set`/`string`, `reverse_iterator` convertible y con `+`, `shrink_to_fit`,
`vector::erase(rango)`, `insert(pos, k, valor)`, `==`/`<` de vectores, `stack` comparable,
`list::splice`, `partition`, `rotate`, `nth_element`, `none_of`, `count_if`, `lower_bound` con
comparador, `std::begin/end`, `<tuple>`, `shared_ptr`, `bad_alloc`, `is_integral` y `sort` de elementos
que solo se mueven. Todo con prueba en `tests/cxx_biblioteca_unidad.cpp` (se ejecuta en el PC con la
cabecera nueva). Lo demás que falta (hilos, `<atomic>`, `<mutex>`, `<array>`, `<bitset>`, `<deque>`,
`<queue>`) vive en `saguios/include/` del repositorio de OpenTTD (sin hilos: los cerrojos no hacen nada
y `std::thread` no se puede crear).

## Qué falta

- Resolver el bloqueo legal de arriba antes de publicar ningún paquete.
- Música (MIDI de OpenMSX): el sistema tiene el controlador `midi`, falta un controlador de música de OpenTTD.
- Texto no latino (sin freetype/ICU); partidas comprimidas (zlib en el paquete) y red probada en partida real.
- Entrada de menú tomada del manifiesto (hoy fija en `kernel/gui/barra.c` con un icono genérico).
- Crear el repositorio público y la Release (solo con la orden del mantenedor, D260).
