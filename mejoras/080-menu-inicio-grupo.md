# 080 - Menú Inicio con iconos, rejilla que rueda por filas y acciones de grupo

- **Estado:** hecha
- **Prioridad:** alta
- **Alta:** 2026-11-11
- **Relacionadas:** 058 (el explorador de archivos), D051 (menú contextual), D135 (biblioteca de iconos)

## Problema / motivación

Tres cosas que se notaban al usar el escritorio:

1. **El menú Inicio tenía programas sin icono.** «Recortes», «Iconos», «Música» y
   «Fecha y hora» salían con el nombre pelado, y los programas nuevos (el Visor de
   PDF, el Reproductor de vídeo y el chat de la IA) ni siquiera estaban en el menú.
2. **La vista de iconos del Archivos se movía hacia un lado al rolar.** La rejilla
   es una tabla de celdas de 92x74 px, pero la rueda del ratón (que usa la lista
   común de la `libsagui`) desplazaba un *elemento* de cada vez: los iconos se
   colaban en la celda de al lado en vez de bajar una fila.
3. **Con varios archivos marcados no había acciones de grupo.** El botón derecho
   seleccionaba el elemento sobre el que se pulsaba y ahí se acababa: no había
   manera de copiar, cortar, borrar ni comprimir todo lo marcado de una vez.

## Propuesta

- **Iconos nuevos de 16x16** en `kernel/gui/arte.c` (con la paleta del propio
  kernel): `arte_mini_recortes` (una cámara), `arte_mini_icones` (cuatro cuadrados
  de colores), `arte_mini_musica` (una nota), `arte_mini_relogio` (un reloj),
  `arte_mini_visorpdf` (una hoja con una «A» roja), `arte_mini_video` (una pantalla
  con el botón de play) y `arte_mini_ia` (una cabeza de robot). Declarados en
  `kernel/inc/arte.h` y usados en la tabla `programas[]` de `kernel/gui/barra.c`,
  que además gana las entradas de los programas nuevos.
- **La rejilla del Archivos rueda por filas.** `lista.topo` pasa a ser siempre el
  primer elemento de una fila (un múltiplo del número de columnas), la rueda salta
  `RODA_LINHAS` filas enteras, las flechas van de celda en celda y de fila en fila,
  el recuadro de selección marca las celdas de verdad y la barra de desplazamiento
  cuenta líneas.
- **Acciones de grupo.** Con más de un elemento marcado, el botón derecho abre un
  menú propio: «Abrir (N)», «Copiar», «Cortar», «Borrar (N)», «Propiedades (N)»,
  «Comprimir en .zip» y «Comprimir en .tar.gz». El botón derecho sobre un elemento
  *no* marcado sigue seleccionando solo ese elemento.

Archivos: `kernel/gui/arte.c`, `kernel/inc/arte.h`, `kernel/gui/barra.c`,
`userland/programas/arquivos.c`, `tests/h108_fixes_arquivos.py` (nueva),
`tests/h109_menu_inicio.py` (nueva), `docs/decisoes.md` (D139).

## Criterios de aceptación

- [x] Todos los programas del menú Inicio tienen icono (los que estaban sin él y
      los nuevos).
- [x] La rueda en la vista de iconos salta filas enteras: los items siguen en las
      mismas celdas y la rejilla no se desplaza de lado.
- [x] Las flechas mueven la selección de celda en celda y de fila en fila.
- [x] Con varios elementos marcados, el botón derecho da el menú de grupo con sus
      nueve líneas, y «Comprimir en .zip» mete todos los marcados en un archivo.
- [x] El menú de un elemento, el de fondo, el portapapeles de archivos y los
      mensajes del registro no cambian.
- [x] Test automático en `tests/` que lo comprueba: `h108` (rejilla y grupo) y
      `h109` (iconos del menú Inicio).
- [x] Decisión registrada en `docs/decisoes.md` (D139).

## Lo que vino con esto

- **La asociación por extensión en el Archivos:** un `.pdf` se abría en el editor de
  texto y un `.avi` también. Ahora la extensión decide (`.pdf` → `visorpdf`,
  `.avi`/`.mpg`/`.mpeg` → `reproductor`), tanto al hacer doble clic como en
  «Abrir con…», que ya incluye el Visor de PDF entre los programas.

## Notas

- La compresión en grupo llama a los programas `zip` y `tar` del sistema con todos
  los nombres marcados. El `tar` del sistema acepta 64 rutas, así que el grupo se
  recorta a 61 elementos y avisa de los que quedan fuera.
- La geometría de la lista (x=146, y=70, items de 20 px en las vistas de lista y
  detalles) no se toca: las pruebas de la H23, la H34, la H43, la H45 y la H55
  dependen de ella.
