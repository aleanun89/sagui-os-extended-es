# Publicar en el repositorio público (D260)

Hay **dos repositorios**:

- **De trabajo** (`aleanun89/miniOS`, **privado**, y seguirá siéndolo): todo el historial, las
  imágenes de todas las versiones en `entrega/`, las notas internas, los informes de las máquinas.
  Aquí se desarrolla.
- **Público** (nuevo): historial limpio, solo lo que cumple la GPL v3, sin binarios. Cada
  publicación es **un commit nuevo** encima de los anteriores, con mensaje «Publicación vX.Y» y la
  etiqueta de la versión. Los binarios van en la **GitHub Release** de esa etiqueta.

La GPL v3 se cumple así: el commit etiquetado es el **código fuente correspondiente** de las
imágenes de la Release (se dice en las notas de la Release), y `LICENSE` + `AUTORES.md` (con el enlace
al proyecto original de Eli Rigobeli) van en el árbol. Ver `docs/licencia.md`.

## Quién y cuándo

**Los agentes nunca publican** (ni preparan para subir, ni crean el repositorio, ni hacen push al
público) sin la orden explícita del mantenedor **en esa conversación**. Se publica al cerrar una
versión X.Y, o una beta cuando el mantenedor lo decida. El mantenedor es quien escribe `PUBLICAR`
en la confirmación.

## Nombres y repositorio (D261)

El repo público es `aleanun89/sagui-os-extended-es` (`PUBLICAR_URL` lo cambia; es el valor por defecto de
`subir`). Tiene su propia nomenclatura, distinta de la privada: la primera publicación es **`beta1`**
(luego `beta2`...) y después **`AAAAMM-VERSION`** con el año-mes y la versión del repo de trabajo
(`202611-8.0`, `202612-8.0.1`). Es la etiqueta y el título de la Release. `--origen vX.Y` (por defecto,
`git describe`) dice de qué versión del repo de trabajo sale; el commit, la etiqueta y las notas de la
Release llevan «beta1 = v8.0b7, commit X del repo de trabajo». Las imágenes se buscan en
`entrega/<origen>`.

## Pasos

1. Que `main` tenga todo integrado y limpio (también la rama de licencia `v8.0b7-licencia-gplv3` y
   `v8.0b7-sin-linux`: sin ellas fallan las comprobaciones de licencia, a propósito).
2. Las imágenes de la versión, **sin NetSurf ni `input_share`**: `make variantes DESTINO=entrega/vX.Y`
   (nunca `priv_variantes`: esas son de uso propio y no se publican).
3. Preparar y comprobar:

   ```bash
   tools/publicar.sh preparar beta1 /ruta/preparacion/beta1 --origen v8.0b7   # [--commit REF] [--compilar]
   ```

   Exporta con `git archive` **solo los ficheros versionados** (nunca el árbol de trabajo), aplica
   `tools/publicar-excluir.txt` y pasa las comprobaciones: secretos y tokens, correos, MAC, IP
   públicas, números de serie, ficheros de más de 5 MB, que no exista `kernel/terceros/linux/`, que
   no haya texto GPL-2.0-only en código propio, `LICENSE` (GPL v3) y `AUTORES.md`,
   `tools/auditar_licencias.py` (con `--estricto` cuando existe) y `tools/spdx.py --comprobar`. Con
   `--compilar` también compila el árbol preparado (`PUBLICAR_TURNO=/ruta/turno.sh`). Si una falla,
   se para, lista todo lo que falló y **no deja nada** en el destino.
4. Subir (clona el público en `~/.cache/saguios-publicar/repo-publico`, sustituye su contenido por
   el preparado borrando lo que ya no está, commit con el autor de `git config` del repo de trabajo,
   etiqueta, y **pide escribir `PUBLICAR`** antes del `git push`):

   ```bash
   tools/publicar.sh subir beta1 /ruta/preparacion/beta1 [URL] [--imagenes entrega/v8.0b7]
   ```

   Si lo preparado cambió después de `preparar` (huella del directorio), `subir` se niega.
5. Release con las imágenes: `subir` imprime la orden (y, con `tools/gh-agentes.sh` y confirmación
   `RELEASE`, la ejecuta):
   `gh release create beta1 --title ... --notes-file ... saguios.img saguios-smp.img SHA256SUMS.txt VERSION.txt`.
   Las notas dicen que el código fuente es el commit etiquetado.

## Qué no se publica y por qué

La lista está comentada en `tools/publicar-excluir.txt`. Resumen: `entrega/` (1,2 GB de binarios;
van a la Release), `build*/`, `input_share/`, `NEW_FONTS/`, `data_extra/`, audio/vídeo personales,
`.env*` (tokens), `.claude/` (configuración de los agentes), `testes_hw/` (informes y fotos de
máquinas reales: números de serie, MAC), `ayudame.md`, `status.md`, `docs/CONTINUAR.md`,
`docs/regressao-final.txt` (notas internas), el **código** de NetSurf y de GNUBoy (GPL-2.0 solo v2, no
compatible con la libsagui GPL-3.0-only: `netsurf/fuentes/` y `gnuboy/jogo/`; `docs/licencia.md`) y
`kernel/terceros/linux/r8169/`. **Sí van** nuestro pegamento con NetSurf y GNUBoy (`netsurf.mk`,
`gnuboy.mk`, `plataforma/`, `libc/`, SPDX GPL-2.0-or-later) y los guiones que descargan el original
(D261): cada usuario se los compila para uso propio con `make priv_variantes`.
Sí van `docs/capturas/` (25 MB) y `testes_eli/` (980 KB, fotos del proyecto original).
Las fuentes de **TCC** (D384, LGPL-2.1 o posterior, compatible) tampoco van al repositorio (se bajan con
`tools/tcc_fuentes.sh`, que `make priv_descargas` llama), pero el compilador **sí va en las imágenes públicas**
(`/apps/bin/tcc`, `/apps/tcc` con sus fuentes parcheadas y `LICENCIA-TCC.txt`): antes de `make variantes`, bajarlas.
`publicar.sh` y `auditar_licencias.py --publico` fallan si aparece código de NetSurf o GNUBoy.

La amalgama de SQLite (9,5 MB, dominio público) es la única excepción al límite de 5 MB.

## Corregir una publicación

Nunca se reescribe la historia del público (ni `push --force`, ni mover etiquetas): se arregla en el
repo de trabajo, se integra en `main` y se publica **una versión nueva** (por ejemplo `202612-8.0.1`) con
otro commit y otra etiqueta. Si algo sensible llegó a publicarse, avisar al mantenedor: hay que
rotar el secreto, no solo borrarlo.

## Ajustar las comprobaciones

Las comprobaciones son heurísticas y pueden dar falsos positivos (una IP en un comentario, una MAC
en un volcado). Se resuelve en `tools/publicar.sh` (listas de permitidos, con comentario del porqué)
o quitando el dato del repo de trabajo, **no** saltándose la comprobación. Prueba:
`python3 tests/h260_publicar.py` (repositorio de juguete, sin QEMU).

## DOOM (D383)

Antes de `make variantes`, ejecutar `bash tools/doom_fuentes.sh` y `bash tools/doom_datos.sh` (o `make priv_descargas`): Chocolate Doom (GPL-2.0 o posterior) y Freedoom (BSD-3-Clause) van en las imágenes públicas. Nunca un WAD comercial.
