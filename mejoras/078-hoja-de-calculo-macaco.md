# 078 - Hoja de cálculo minimalista «Macaco» (ficheros `.mxs`)

- **Estado:** hecha (D142, rama `v6.4-macaco`, prueba `h110`)
- **Prioridad:** media
- **Alta:** 2026-11-11
- **Relacionadas:** 042 (componentes da libsagui), 058 (explorador de archivos),
  054 (SaguiGL, para los gráficos), 047 (JPEG)

## Problema / motivación

El SaguiOS tiene un explorador de archivos, un visor de imágenes, un reproductor
de vídeo y un terminal, pero no tiene una **hoja de cálculo**. Es el programa
que más se echa de menos en un sistema de escritorio: para hacer cuentas
rápidas, una lista de precios, un presupuesto o un planning.

El requisito del usuario es un programa **minimalista**, con las **funciones
típicas** de una hoja de cálculo y **gráficos** (sólo de **barras** y de
**tarta**), que pueda **guardar y cargar** sus propios ficheros.

## Propuesta

Programa `macaco` (Macaco eXcel Sheet) con ventana propia:

| Pieza | Qué hace |
| --- | --- |
| **Hoja** | Cuadrícula de celdas con el desplazamiento y la selección por teclado y ratón. Se ve el valor de la celda activa arriba, como en cualquier hoja |
| **Fórmulas** | Las de toda la vida, con `=`: `=A1+B2`, `=SUM(A1:A10)`, `=AVERAGE(...)`, `=MIN`, `=MAX`, `=COUNT`, `=IF`, `=ROUND`, `=ABS`, `=PRODUCT`. Referencia a otras hojas con `Hoja2!A1` |
| **Gráficos** | A partir de un rango seleccionado, dos tipos: **barras** (verticales y horizontales) y **tarta** (reparto por porcentaje). Se dibujan en una ventana aparte, con su propio gráfico y su leyenda |
| **Ficheros** | Guardar y cargar los ficheros propios, con extensión **`.mxs`** (Macaco eXcel Sheet) |
| **Opciones** | Abrir, guardar, guardar como, deshacer, copiar y pegar un rango, insertar o borrar filas y columnas, ordenar un rango, ajustar el ancho de las columnas |

### El formato `.mxs`

Un formato propio, sencillo y **legible**: el fichero lleva una cabecera con la
versión, las hojas con su nombre y el número de filas y columnas, y después el
contenido celda a celda (valor y fórmula). Al ser texto, un `.mxs` se puede
mirar con cualquier editor y se puede migrar a otros formatos sin problema.

## Criterios de aceptación

- [x] Abrir, editar y guardar un fichero `.mxs`, y volverlo a cargar sin cambios
- [x] Las fórmulas (suma, promedio, mínimo, máximo, contar, si, redondear) dan
      el resultado correcto
- [x] Hacer un gráfico de barras y uno de tarta a partir de un rango
- [x] Un `.mxs` con dos hojas, y una fórmula que referirse a la otra
- [x] Test automático en `tests/` que lo comprueba: `h110`
- [x] Decisión registrada en `docs/decisoes.md`: D142

## Notas

- Lo decide el usuario: entra en una **release posterior** (la v6.4), no en la
  entrega del visor de PDF ni en la del reproductor de MPEG-1.
- *Sin código de terceros*: el formato `.mxs`, las fórmulas y los gráficos se
  escriben desde cero, como el resto del proyecto.
- Los gráficos de barras y de tarta se pueden dibujar con los componentes de
  la `libsagui` (el lienzo de la 042) y con el rasterizador del SaguiGL, sin
  números de coma flotante si hace falta (el motor con flotantes va aparte,
  como el del PDF).
- De momento no hay impresión ni importación/exportación a CSV: quedan
  pendientes si la mejora crece.

## Lo que se hizo (v6.4)

- El programa `macaco`, todo en `userland/programas/macaco.c` (un solo fichero, sin
  tocar el `Makefile`): cuadrícula con desplazamiento, selección por teclado y
  ratón, barra de fórmulas, pestañas de hoja y botón «+».
- Las fórmulas con evaluador propio: `SUM`, `AVERAGE`/`AVG`, `MIN`, `MAX`, `COUNT`,
  `IF`, `ROUND`, `ABS`, `PRODUCT`, comparadores, rangos `A1:B10` y referencias a otra
  hoja (`Hoja2!A1`). Todo en punto fijo entero de centésimas, con la división de
  64 bits escrita a mano (los programas no llevan `libgcc`).
- Los tres gráficos (barras verticales, barras horizontales y tarta) en una ventana
  aparte, con su leyenda y sus porcentajes.
- El formato `.mxs` en texto legible, con palabras clave en español (`hoja`,
  `celda`, `larguras`) y cabecera `MXS 1`. Las fórmulas se guardan y se recalculan al
  cargar.
- El menú de opciones (25 líneas), la barra de botones, el deshacer (24 pasos) y los
  atajos Ctrl+S/O/Z/C/X/V.
- La prueba `tests/h110_macaco.py` y la decisión D142 en `docs/decisoes.md`.

### Lo que queda pendiente

- El programa todavía **no aparece en el menú Inicio**: abrirlo desde la terminal
  (`macaco`) o desde el Archivos.
- **Pegar no ajusta las referencias relativas** de las fórmulas: se pega el texto
  tal cual.
- Sin impresión ni importación/exportación a CSV.
