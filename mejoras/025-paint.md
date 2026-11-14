# 025 - Paint, programa de dibujo

- **Estado:** hecha
- **Prioridad:** media
- **Alta:** 2026-11-07

## Problema / motivación

No había forma de crear una imagen dentro de SaguiOS. Con la biblioteca de imágenes (mejora
024) ya se pueden leer y guardar BMP; faltaba el clásico programa de dibujo.

## Propuesta (aplicada)

`userland/programas/paint.c` (D056): lienzo de tamaño configurable (640x480 por defecto),
lápiz, pincel con grosor, goma, aerógrafo, línea, rectángulo y elipse (contorno y relleno),
bote de relleno iterativo, cuentagotas y texto con la fuente del sistema; paleta de 28 colores
con principal (clic izquierdo) y secundario (clic derecho); deshacer de hasta 16 niveles / 24
MB; Nuevo, Abrir (BMP/PPM/PNG) y Guardar (BMP 24 bits) con ruta escrita; Ctrl+Z/S/O/N;
redimensionable; `paint archivo.bmp`. Icono «Paint» en el escritorio y `ajuda paint`.

## Criterios de aceptación

- [x] Herramientas: lápiz, pincel, goma, línea, rectángulo, elipse (contorno y relleno), bote,
      cuentagotas, texto
- [x] Color principal y secundario (botón izquierdo y derecho)
- [x] Deshacer de varios niveles con límite de memoria
- [x] Nuevo, Abrir, Guardar (BMP) y atajos Ctrl+Z, Ctrl+S, Ctrl+O, Ctrl+N
- [x] Redimensionable; `paint archivo.bmp`
- [x] Test automático `tests/h57_paint.py` (dibujo con el ratón, relleno, deshacer, BMP
      comprobado con `visor -i` y con una captura del visor)
- [x] Decisión registrada en `docs/decisoes.md` (D056)

## Notas

- Pendiente: zoom del lienzo, rehacer (Ctrl+Y), selección rectangular con copiar/pegar,
  tolerancia en el bote, polígonos y curvas.
- El botón derecho en el lienzo se usa para el color secundario. Si el gestor de ventanas
  añade un menú contextual con el botón derecho, el Paint debe seguir recibiendo el clic
  derecho dentro del lienzo.
