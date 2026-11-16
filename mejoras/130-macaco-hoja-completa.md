# 130 — Macaco: una hoja de cálculo como las de verdad (v8.4.4)

Pedido del mantenedor (2026-11-15): «Mejora la hoja de cálculo, dótala de más capacidades: las columnas y
filas por defecto se me hacen raras comparado con las versiones reales; funcionalidad de combinar celdas…».

## Problema

- La hoja tiene un tope de **1000 filas × 100 columnas** (`MAC_MAX_LINHAS`, `MAC_MAX_COLUNAS` en
  `userland/programas/macaco.c`), y se reserva entera. Excel 97 tenía 65 536 × 256 (A..IV), y el actual
  tiene 1 048 576 × 16 384 (A..XFD).
- El ancho de columna (72 px) y el alto de fila (20 px) no se parecen a los de Excel/Calc: hay que
  medirlos con la fuente del sistema para que la cuadrícula «se vea» como la de una hoja de cálculo.

## Objetivo

1. **Hoja dispersa** (solo se guardan las celdas con algo) con, como mínimo, **65 536 filas × 256
   columnas** (A..IV, como Excel 97), y si no cuesta memoria, 1 048 576 × 16 384. Ctrl+Fin, Ctrl+flechas
   (saltar al borde de los datos) y el cuadro de nombre (ir a `XFD1048576`) han de funcionar.
2. **Ancho y alto por omisión** como los de Excel o LibreOffice Calc en proporción a la fuente;
   ancho por columna y alto por fila; doble clic en el borde de la cabecera = autoajuste.
3. **Combinar celdas**: «Combinar y centrar», combinar, combinar horizontalmente y separar. Se guardan en
   el formato propio y se exportan e importan en XLSX (`mergeCells`) y ODS (`table:number-columns-spanned`).
4. Lo que más se echa de menos al compararla con las reales (comprobar antes qué existe ya):
   - inmovilizar paneles (filas o columnas fijas);
   - ocultar y mostrar filas y columnas;
   - insertar y eliminar filas, columnas y celdas, con las referencias de las fórmulas ajustadas;
   - bordes y relleno de celda, copiar formato (brocha);
   - ajustar texto y alineación vertical;
   - ordenar y autofiltro;
   - formato condicional sencillo;
   - validación de datos (lista desplegable);
   - buscar y reemplazar;
   - comentarios de celda.

## Pruebas

- Una prueba hNN nueva que:
  - escriba en `IV65536`, guarde y vuelva a abrir;
  - combine `B2:D4`;
  - exporte e importe en XLSX y ODS;
  - compruebe la memoria con una hoja casi vacía (no puede crecer con el tamaño teórico).
- Capturas de la cuadrícula comparables a Calc.
- Las pruebas de Macaco existentes (h29x/h30x) siguen pasando.

## Estado (fase 2, v8.4.5, D345)

Hecho: barras de desplazamiento, buscar y reemplazar (Ctrl+F / Ctrl+H / F3), ajustar texto con altura automática, copiar
formato (brocha), autofiltro por valor, formato condicional (mayor, menor, igual, entre), comentarios con marca y recuadro al
pasar el ratón, validación de datos (lista o rango numérico). Todo en el formato propio, con botones en la cinta y en los menús.
Prueba: `tests/h347_macaco_fase2.py`.

Queda: exportar/importar a XLSX y ODS los comentarios, las reglas condicionales, la validación y el filtro (hoy se pierden);
alineación vertical; autofiltro con varios valores, por condición u ordenar desde la flecha; que insertar o borrar filas y
columnas desplace comentarios, reglas, validaciones y el rango del filtro; formato condicional con barras de datos o escalas
de color; deshacer de comentarios, reglas y validación; hojas de 1 048 576 x 16 384; buscar en todas las hojas.
