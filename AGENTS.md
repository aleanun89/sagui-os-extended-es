# AGENTS.md — reglas para agentes de IA en este repositorio

## Entorno local (`~/.local/saguitools/env.sh`)

`run_in_qemu.sh` usa `set -u` y hace `. ~/.local/saguitools/env.sh`. Por eso:

- **Nunca** referenciar una variable posiblemente no definida con `$VAR` en `env.sh`
  ni en el bloque que lo genera (`tools/entorno-local.sh`). Usar siempre
  `${VAR:+:$VAR}` (para ampliar PATH, LD_LIBRARY_PATH, LIBRARY_PATH, CPATH…) o `${VAR:-}`.
  Fallo ya ocurrido: `env.sh: line 5: LD_LIBRARY_PATH: unbound variable`.
- `env.sh` se **genera** desde `tools/entorno-local.sh` (heredoc). Si se cambia uno, cambiar
  el otro y comprobar que quedan iguales.
- Antes de dar por buena una modificación del entorno, probar con las variables sin definir:
  `bash -c 'set -u; unset LD_LIBRARY_PATH LIBRARY_PATH CPATH; . ~/.local/saguitools/env.sh && echo OK'`
  y arrancar `./run_in_qemu.sh --help` y `./run_in_qemu.sh`.
- No tocar `~/.local/saguitools/` a mano sin reflejarlo en `tools/entorno-local.sh`.
- No escribir texto en otros idiomas (cirílico, portugués…) en comentarios o archivos:
  el proyecto está en español.

## `CPATH` y las cabeceras del anfitrión (D191)

`env.sh` exporta `CPATH` con la glibc del anfitrión y GCC lo añade **incluso con
`-nostdinc`**. El `Makefile` lo anula para todo lo que va dentro de SaguiOS: si algo falla
por una cabecera que «no existe», es que falta una nuestra o un `-I`; **nunca** arreglarlo
volviendo a pasar `CPATH`/`-I` a la glibc del anfitrión. Solo las herramientas del PC
(`HOSTCC`) la reciben, por `HOST_CPATH`. `tests/h191_cpath.py` lo vigila.

## Copias de trabajo aisladas (`miniOS_master_os_<característica>`)

Cada característica de la v6.1 se desarrolla y se prueba en su **propia copia**, en
`../miniOS_master_os_<característica>`, para que los cambios de una no rompan el
árbol principal ni las herramientas de las demás (cada copia compila a su propio
`build/`).

- Se crean con `tools/criar-os.sh <característica> [rama_base] [nombre_rama]`, que
  las hace con `git worktree` (comparten historial con el repo principal).
- El árbol principal (`miniOS_master`) se queda como **integración**: rama `main`,
  sin cambios sin commitear. Las ramas de trabajo viven en las copias.
- Antes de tocar una copia, comprobar `git worktree list` para saber dónde está
  cada rama.
- Para retirar una copia: `git worktree remove ../miniOS_master_os_<característica>`.
- En una copia, las fuentes de NetSurf se enlazan a las del árbol principal
  (`ln -sfn /home/.../miniOS_master/userland/terceros/netsurf/fuentes userland/terceros/netsurf/fuentes`).
  **Ese `ln` solo dentro de la copia** (`cd ../miniOS_master_os_<x> && ln ...`): ejecutado en el
  árbol principal deja un enlace a sí mismo y borra las fuentes (pasó el 2026-11-13; se
  recuperan con `tools/netsurf_fuentes.sh`).

## Codificación de los ficheros

El proyecto mezcla UTF‑8 e ISO‑8859‑1 (Latin‑1) y hay ficheros con acentos: hay que
saber de qué lado está cada uno antes de parchearlo, o el fichero se estropea.

- **UTF‑8**: todo `userland/` salvo lo de abajo, `kernel/gui/arte.c`, `kernel/inc/arte.h`,
  `kernel/gui/barra.c`, `docs/*.md`, `tests/*.py`, `mejoras/*.md`, `tools/*.py`.
- **ISO‑8859‑1 (Latin‑1)**: `userland/programas/arquivos.c` y `userland/programas/cipo.c`.

El código del userland se compila con `-finput-charset=UTF-8 -fexec-charset=ISO-8859-1`:
los textos de la interfaz salen en Latin‑1, que es lo que espera el terminal.

Al parchear un fichero, leer y escribir **con la misma codificación**:

```python
import io
s = io.open('userland/libsagui/mpeg1.c', encoding='utf-8').read()
s = s.replace('velho', 'novo')
io.open('userland/libsagui/mpeg1.c', 'w', encoding='utf-8').write(s)
```

## Nombres de fichero

Los nombres se parecen entre sí (`configuracion.c` / `ifconfig.c`,
`h83_configuracion.py`) y es fácil escribirlos mal. **Usar comodines** en los
comandos (`git add userland/programas/config*.c`, `python3 tests/h83*.py`) y
comprobar con `ls` o `git ls-files` cuando haya duda.

## General

- Tras cambiar scripts de arranque (`run_in_qemu.sh`, `tools/`), ejecutarlos de verdad antes de
  decir que funcionan.
- No commitear las capturas de `docs/capturas/` que solo regeneró la batería sin cambio real.
- No tocar la batería y la compilación a la vez: las dos usan `build/`.
- **Nunca probar sobre un build fallido:** `make ... && python3 tests/...` (con `&&`,
  nunca `;`) y mirar el código de salida de `make`. Si el enlace falla, la imagen
  no se regenera y la prueba arranca la vieja sin avisar (pasó dos veces con el SMP).
- Antes de compilar, buscar caracteres de otros alfabetos en lo tocado:
  `git diff | grep -nP '[\x{0370}-\x{03FF}\x{0400}-\x{04FF}\x{3000}-\x{9FFF}]'`.
- **Si el usuario dice PARA, se para:** primero se deja todo anotado (`ayudame.md`) y
  no se hace "una última prueba".
- No usar `git stash` para probar cambios (corrompió objetos de `userland/`):
  ramas o copias aisladas.

## Trabajadores especiales: drivers de hardware real (solo con autorización)

`especialista-drivers` (Fable, esfuerzo bajo: desarrollar y ampliar drivers, sobre todo de GPU) y
`depurador-drivers` (Opus, esfuerzo medio: pruebas y corrección de fallos de esos drivers en
máquinas reales) se usan **solo con autorización expresa del mantenedor en esa conversación**. Nunca
por iniciativa propia ni «ante la duda»: para lo demás, los trabajadores normales (Sonnet/Haiku por
defecto mientras la suscripción sea limitada).

## GitHub: Issues y Discussions con el token de los agentes

- El token está en `.env_agents` (variable `GT`), **ignorado por git y con permisos 600**. **Nunca**
  mostrarlo, copiarlo a otro archivo, a un commit, a un log, a una issue ni a la salida de un comando.
- Usarlo siempre por `tools/gh-agentes.sh` (lo pasa a `gh` sin mostrarlo): `issue list/view/create/comment`,
  `discusion-listar`, `discusion-leer N`, `discusion-crear "Categoría" "Título" fichero.md`, `discusion-editar N fichero.md`, `discusion-titulo N "Título"`, `discusion-comentar N fichero.md`.
- Para qué: **Issues** para fallos y tareas concretas (p. ej. «dv7: C&C lento», con etiqueta de la
  máquina), con los resultados de pruebas como comentarios; **Discussions** para planes y notas largas
  que conviene leer antes de empezar (estado, decisiones abiertas).
- **Roadmap y decisiones en Discussions** (pedido del mantenedor, 2026-11-14): #4 (Announcements) es el
  resumen del roadmap; cada versión próxima tiene su discusión en «General» con sus contenidos y las
  **decisiones pendientes como preguntas con opciones**. El mantenedor contesta allí; al empezar una sesión,
  leer esas discusiones (`discusion-listar`, `discusion-leer N`) y pasar lo decidido a `docs/ROADMAP.md` y
  `docs/decisoes.md` (la fuente de verdad sigue siendo el repositorio). Al publicar una versión, actualizar #4.
- Antes de abrir una issue, buscar si ya existe (`issue list --search`). No cerrar issues del
  mantenedor sin que lo diga. No publicar en ellas datos personales (números de serie, MAC, IP pública).

## SMP y KVM

- **Con KVM la CPU es la del anfitrión (AMD).** Las pruebas que esperan Intel (h39)
  fallan solo con KVM: repetirlas sin KVM.
- **El TCG no es prueba de que algo funcione en hardware real:** no comprueba
  límites de segmento, `CR4.OSFXSR`, etc. Lo de arranque y CPU, probarlo con KVM.
- **El SMP es opcional (D164):** solo se encienden los secundarios con
  `mkimagem.py --smp` (o `imagem_de_teste(..., ["--smp"])`). El kernel corre solo en
  la CPU 0 (modelo maestro/esclavo): el código del kernel no necesita cerrojos
  nuevos mientras siga así, pero el planificador, la FPU y lo que corra en el tic
  de un secundario sí.
- Las marcas de depuración por el puerto `0xE9` pisan AL/DX: no ponerlas entre
  cargas de segmento. Mejor escribir el paso en memoria (ver `_ap_progresso`).

## La batería: cuidado, el PC se reinicia

En WSL2, la batería completa **en paralelo** (`tests/rodar_paralelo.py -j 8`) satura la CPU
y la memoria con los QEMU en TCG y **el ordenador entero se cuelga o se reinicia**, lo cual
además corta el desarrollo. Reglas:

- Una prueba cada vez mientras se desarrolla: `python3 tests/h101_sagui_gl_v6.py`.
- Para la batería completa, `-j 2` o `-j 3` como mucho, y mejor con KVM
  (`sg kvm -c ". ~/.local/saguitools/env.sh; python3 tests/rodar_paralelo.py --kvm -j 4"`).
- Dejar la máquina con todo commitear antes de una batería larga: si se reinicia, no se
  pierde el trabajo.
- Si hay que parar la batería, parar el proceso; no dejar QEMU a medias.
- **La carga es la suma de todo** (2026-11-12, el PC se reinició): la batería con
  `-j 3` corriendo a la vez que seis agentes que compilaban y lanzaban su propio QEMU
  colgó WSL2. La regla «esperar si hay 3 QEMU» de cada agente no basta: no cuenta las
  compilaciones ni la memoria del resto. Por eso:
  - **Nunca la batería completa mientras haya agentes trabajando.** Primero terminan o se
    paran los agentes; después, la batería sola.
  - **Como mucho 2 agentes a la vez** que compilen o prueben en QEMU; el resto, esperando.
    **En el PC principal del mantenedor (16 núcleos, 36 GB) pueden ser 4** (dicho el 2026-11-14), y
    la batería con `-j 4`. En otro ordenador, 2.
  - **Escribir código no cuenta:** pueden trabajar más agentes a la vez si solo escriben; el límite
    es para compilar y probar. El coordinador lo hace cumplir con un guion de turnos (`flock` con 2 a 4
    huecos, que además espera a que acabe la batería) con el que cada agente envuelve `make` y las pruebas
    (o lanzarlos después).
  - Cada agente con `make -j2` y una prueba QEMU cada vez.


## Repositorio público (D260)

- Este repositorio (`aleanun89/miniOS`) **sigue privado**, con todo su historial. El público es otro,
  con historial limpio: un commit «Publicación vX.Y» por versión, solo lo que cumple la GPL v3, sin
  binarios (las imágenes van a la GitHub Release).
- **Ningún agente publica, crea el repositorio público ni hace push a él** sin la orden explícita del
  mantenedor en esa conversación. Se hace con `tools/publicar.sh preparar|subir` (`docs/publicar.md`).
- Lo que no se publica está en `tools/publicar-excluir.txt`. Un fichero nuevo con datos personales,
  números de serie, MAC, tokens o notas internas debe añadirse ahí (o no versionarse).
- Nunca reescribir la historia del público: los errores se corrigen con una publicación nueva.

## Traducciones del README

- `README.md` (español) es la fuente. `README_PT.md` (portugués de Brasil) y `README_EN.md` (inglés) son traducciones completas que mantiene **`trabajador-haiku`**.
- Cuando cambie `README.md`, quien lo cambie lanza a `trabajador-haiku` para aplicar el cambio a las dos traducciones. `python3 tools/readme_sincronia.py --diff` dice qué cambió desde el hash de traducción; al terminar, la primera línea de cada traducción pasa a llevar el hash del último commit que toca `README.md`.
- Antes de publicar una versión, `python3 tools/readme_sincronia.py` tiene que salir con 0 (sale con 1 si alguna traducción va por detrás).
- La línea de idiomas (`[Español](README.md) · [Português](README_PT.md) · [English](README_EN.md)`) va bajo el título de los tres ficheros.
- **`compilar_versiones.md`** (raíz) es la guía de compilación para usuarios del repositorio público, con tres secciones completas e iguales (Español, English, Português Brasil) y un índice arriba. Se mantiene igual que los README: cuando cambien opciones de compilación (`Makefile`, `gerar_versao.sh`, `tools/mkimagem.py`, `run_in_qemu.sh`), las descargas (`tools/*_datos.sh`, `netsurf_fuentes.sh`, Typodermic) o lo que se publica (`tools/publicar-excluir.txt`), se actualizan **las tres secciones a la vez** (`trabajador-haiku` aplica la traducción). Las órdenes no se traducen.
