# 129 - PlanoMini: mini CAD 2D

- **Estado:** hecha (**v8.3.3**, D312)
- **Prioridad:** media
- **Alta:** 2026-11-15
- **Relacionadas:** 128 (GebraMini, mismo enfoque), 116 (PDF vectorial), 103 (diálogos de archivo, D272)

## Motivación
El mantenedor quiere «una especie de Mini AutoCAD» a partir de su prototipo **PlanoMini** (C++/SDL, unas 300
líneas, GPL-3.0-or-later, © 2026 Alejandro Anuncibay): consola de órdenes, líneas, círculos, rectángulos,
puntos, formato `.pmini`. Inspirado en los CAD de órdenes clásicos pero independiente: sin código, iconos,
marcas ni formatos propietarios (DWG no; DXF, formato de intercambio documentado, sí).

## Hecho
- Motor propio en `userland/planomini/` (sin dibujo ni sistema; se prueba en el anfitrión): entidades, capas,
  órdenes con opciones, geometría, REFENT, deshacer, `.pmini` v1/v2 y DXF.
- Aplicación `planomini` (Win98): menús, barras de dibujo/modificación, propiedades (capa, color, tipo de
  línea), línea de órdenes con historial y autocompletado, barra de estado REJILLA/ORTO/FORZC/POLAR/REFENT.
- Entidades: punto, línea, polilínea (con arcos), rectángulo, polígono, círculo (4 formas), arco (2 formas),
  texto, cotas (horizontal, vertical, alineada, radio, diámetro, angular), sombreado (líneas y sólido).
- Modificar: mover, copiar, girar, escala, simetría, equidistancia, recortar, alargar, empalme (también de
  todas las esquinas de una polilínea), chaflán, explotar, matriz rectangular y polar, igualar propiedades,
  pinzamientos y propiedades editables.
- Ficheros: `.pmini` v2 (abre los v1 del prototipo), DXF ASCII, PDF vectorial a escala con cajetín, PNG.
- Ejemplos en `/ejemplos/planomini/`.

## Pendiente
Elipse, spline, bloques/INSERT del DXF, tipos de línea con escala propia, cotas asociadas, recortar
polilíneas sin explotarlas, empalme de línea con arco o círculo, impresión por ventana.
