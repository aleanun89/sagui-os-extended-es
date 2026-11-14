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

## Pasos

1. Que `main` tenga todo integrado y limpio (también la rama de licencia `v8.0b7-licencia-gplv3` y
   `v8.0b7-sin-linux`: sin ellas fallan las comprobaciones de licencia, a propósito).
2. Las imágenes de la versión, **sin NetSurf ni `input_share`**: `make variantes DESTINO=entrega/vX.Y`
   (nunca `priv_variantes`: esas son de uso propio y no se publican).
3. Preparar y comprobar:

   ```bash
   tools/publicar.sh preparar vX.Y /ruta/preparacion/vX.Y          # [--commit REF] [--compilar]
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
   tools/publicar.sh subir vX.Y /ruta/preparacion/vX.Y https://github.com/ORG/REPO.git [--imagenes entrega/vX.Y]
   ```

   Si lo preparado cambió después de `preparar` (huella del directorio), `subir` se niega.
5. Release con las imágenes: `subir` imprime la orden (y, con `tools/gh-agentes.sh` y confirmación
   `RELEASE`, la ejecuta):
   `gh release create vX.Y --title ... --notes-file ... saguios.img saguios-smp.img SHA256SUMS.txt VERSION.txt`.
   Las notas dicen que el código fuente es el commit etiquetado.

## Qué no se publica y por qué

La lista está comentada en `tools/publicar-excluir.txt`. Resumen: `entrega/` (1,2 GB de binarios;
van a la Release), `build*/`, `input_share/`, `NEW_FONTS/`, `data_extra/`, audio/vídeo personales,
`.env*` (tokens), `.claude/` (configuración de los agentes), `testes_hw/` (informes y fotos de
máquinas reales: números de serie, MAC), `ayudame.md`, `status.md`, `docs/CONTINUAR.md`,
`docs/regressao-final.txt` (notas internas), NetSurf entero (GPL-2.0 solo v2, no compatible con la
libsagui GPL-3.0-only; `docs/licencia.md`) y `kernel/terceros/linux/` (rama `v8.0b7-sin-linux`).
Sí van `docs/capturas/` (25 MB) y `testes_eli/` (980 KB, fotos del proyecto original). GNUBoy está
pendiente de decisión del mantenedor (hay una línea comentada para excluirlo).

La amalgama de SQLite (9,5 MB, dominio público) es la única excepción al límite de 5 MB.

## Corregir una publicación

Nunca se reescribe la historia del público (ni `push --force`, ni mover etiquetas): se arregla en el
repo de trabajo, se integra en `main` y se publica **una versión nueva** (por ejemplo `v8.0.1`) con
otro commit y otra etiqueta. Si algo sensible llegó a publicarse, avisar al mantenedor: hay que
rotar el secreto, no solo borrarlo.

## Ajustar las comprobaciones

Las comprobaciones son heurísticas y pueden dar falsos positivos (una IP en un comentario, una MAC
en un volcado). Se resuelve en `tools/publicar.sh` (listas de permitidos, con comentario del porqué)
o quitando el dato del repo de trabajo, **no** saltándose la comprobación. Prueba:
`python3 tests/h260_publicar.py` (repositorio de juguete, sin QEMU).
